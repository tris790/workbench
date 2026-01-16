# WSH - Input & Keybindings Implementation Plan

## Overview
Mirroring Fish shell's keybindings and input behavior is critical for user adoption. We will implement a custom line editor (similar to `readline` or `linenoise` but tailored for async highlights/suggestions) that handles raw VT inputs.

## Input Mode
*   The terminal will be put into **Raw Mode** using `termios` (POSIX/Cygwin).
*   Non-canonical mode: Input is available immediately (byte-by-byte), no echo, no buffer processing by kernel.
*   Control characters (Ctrl+C, Ctrl+Z) are trapped and handled as signals or editor actions.

## Keybinding Map (Fish-style)

We must support the standard Fish bind set.

| Key Sequence | Action | Description |
| :--- | :--- | :--- |
| `Ctrl + F` / `Right Arrow` | `forward-char` / `accept-autosuggestion` | Move cursor right OR accept the phantom suggestion if at EOL. |
| `Alt + Right Arrow` | `next-word-autosuggestion` | Accept only the next word of the suggestion. |
| `Ctrl + W` | `backward-kill-word` | Delete word to the left of cursor. |
| `Ctrl + L` | `clear-screen` | Clear screen and repaint prompt at top. |
| `Ctrl + U` | `kill-whole-line` | Clear the entire line. |
| `Ctrl + K` | `kill-line` | Kill from cursor to end of line. |
| `Ctrl + A` / `Home` | `beginning-of-line` | Move to start. |
| `Ctrl + E` / `End` | `end-of-line` | Move to end. |
| `Ctrl + P` / `Up Arrow` | `history-search-backward` | Search history matching prefix. |
| `Ctrl + N` / `Down Arrow` | `history-search-forward` | Search history matching prefix. |
| `Tab` | `complete` | Trigger tab completion menu. |
| `Alt + Enter` | `insert-newline` | Insert literal newline (multiline command). |
| `Ctrl + D` | `delete-or-exit` | Delete char or exit shell if empty. |
| `Ctrl + H` / `Backspace` | `backward-delete-char` | Delete character left. |

## Input Event Loop (Pseudo-code)

```c
void input_loop() {
    setup_raw_mode();
    Buffer buffer = {0};
    
    while (running) {
        // Draw prompt + buffer + suggestions + highlighting
        render_interface(buffer);
        
        char c = read_byte();
        
        // Handle Escape Sequences
        if (c == '\x1b') {
            handle_escape_sequence();
            continue;
        }
        
        // Handle Control Keys
        switch (c) {
            case CTRL_W: action_kill_word(&buffer); break;
            case CTRL_F: action_accept_suggestion(&buffer); break;
            // ... other bindings
            case '\r': // Enter
                 execute_command(buffer);
                 break;
            default:
                 buffer_insert(&buffer, c);
        }
        
        // Async Trigger updates
        update_highlighting(&buffer);
        update_suggestions(&buffer);
    }
}
```

## Windows/Cygwin Considerations
*   **Arrow Keys**: Windows terminals send standard ANSI escape sequences (`^[[A`, `^[[B`, etc.) or SS3 sequences (`^[OA`). We must parse both.
*   **Alt Key**: Often comes as an external Escape (`^[`) prefix. `Alt+Right` might be `^[[1;3C`. We need a robust state machine parser for escape sequences.
