# WSH - Syntax Highlighting & Parsing Implementation Plan

## 1. Real-time Syntax Highlighting

### Goal
Color code input as the user types to provide immediate feedback on validity.

### Style Guide
*   **Valid Command**: Blue (or Theme Primary)
*   **Invalid Command**: Red
*   **String/Quoted**: Green/Yellow
*   **Options/Flags**: Cyan (`--help`, `-v`)
*   **Paths**: Underlined (if valid existence), Red (if invalid path)

### Parser Logic
The highlighting parser must be fast and error-tolerant (since input is often incomplete).

1.  **Tokenization**: Split input by whitespace, respecting quotes `""` `''` and escapes `\`.
2.  **Command Validation**:
    *   Check 1st token.
    *   Is it a keyword (`if`, `for`, `function`)? -> Color Keyword.
    *   Is it an absolute path to executable? -> Check `access(path, X_OK)`.
    *   Is it in `PATH`? -> hash map cache of PATH binaries.
    *   **Result**: Valid -> Blue, Invalid -> Red.
3.  **Argument Analysis**:
    *   Starts with `-`? -> Color Option.
    *   Looks like file path (contains `/` or `.` or matches existing file)? -> Check existence.
        *   Exists -> Underline.
        *   Doesn't exist but looks like path -> Red (optional, fish sometimes leaves these white).
    *   Variables (`$var`): Color Magenta.

### Windows/Cygwin Considerations
*   **File Permissions**: `access(path, X_OK)` on Windows/Cygwin generally works, relying on file extensions (`.exe`, `.bat`, `.cmd`) or Cygwin permissions.
*   **Case Sensitivity**: Windows is case-insensitive.
    *   `GiT` should highlight valid if `git.exe` exists.
    *   Cygwin usually handles this, but we should be aware of case-preserving but insensitive matches for user friendliness.

## 2. Parsing Engine (`wsh_parser`)

### Syntax Rules ("Sane Scripting")
We follow the Fish philosophy:
*   **No variable token splitting**: `$var` expands to a single argument, even with spaces.
*   **Block layout**: `if ...; end`, `switch ...; end`.

### Implementation
*   **Recursive Descent Parser**: Builds AST.
*   **AST Nodes**: `Job`, `Pipeline`, `Command`, `Argument`, `Redirection`.
*   **Expansion**: Wildcards (`*`), Tilde (`~`), Variables (`$`), Command Substitution (`()`).

## 3. Error Reporting
*   If syntax error (e.g., missing closing quote), display cursor indicator or change prompt color to red indicating "incomplete state".
