# Suggested Commands for Workbench

Based on the analysis of the current codebase, Workbench currently serves as a hybrid File Explorer and Terminal multiplexer. It delegates file editing to the system default application (`xdg-open` on Linux). The following commands are suggested to enhance the user experience within this scope.

## 1. File Management
Enhance the explorer's capabilities to match standard file managers.

| Command Name | Default Keybinding | Description |
| :--- | :--- | :--- |
| **File: Copy Name** | palette | Copy just the filename. |
| **File: Copy Path** | palette | Copy the absolute path of the selected item to clipboard. |
| **File: Copy Relative Path**| palette | Copy the path relative to the current workspace root. |
| **File: Delete** | `Delete` | Move selected item to trash or permanently delete (with confirmation). |
| **File: Duplicate** | palette | Create a copy of the selected item (e.g., `file_copy.txt`). |
| **File: New File** | palette | Create a new empty file. |
| **File: New Folder** | palette | Create a new directory. |
| **File: Refresh** | palette | Reload the current directory listing. |
| **File: Rename** | `F2` | Rename the selected file or directory. |

## 2. Navigation
Improve movement through the directory structure.

| Command Name | Default Keybinding | Description |
| :--- | :--- | :--- |
| **Nav: Focus Path** | palette | Focus the address bar/breadcrumb to type a path manually. |
| **Nav: Go Back** | `Alt + Left` | Navigate to the previous location in history. |
| **Nav: Go Forward** | `Alt + Right` | Navigate forward in history. |
| **Nav: Go Home** | palette | Navigate to the user's home directory. |
| **Nav: Go to Parent** | `Alt + Up` | Navigate to the parent directory. |
| **Nav: Go to Root** | palette | Navigate to the filesystem root (`/`). |

## 3. Terminal Integration
Since the terminal is a core component, tight integration is key.

| Command Name | Default Keybinding | Description |
| :--- | :--- | :--- |
| **Terminal: Clear** | `Ctrl + L` | Clear the terminal buffer. |
| **Terminal: Toggle** | ` ` ` | Show/Hide the terminal panel. |

## 4. Window & Layout
Manage the application window and panels.

| Command Name | Default Keybinding | Description |
| :--- | :--- | :--- |
| **View: Focus Next Pane** | `Ctrl + Tab` | Switch focus between left/right explorer panes. |
| **View: Toggle Fullscreen** | `F11` | Toggle fullscreen mode. |
| **View: Toggle Split** | `Ctrl + \` | Toggle between single and dual pane explorer view. |
| **Window: Quit** | `Ctrl + Q` | Exit the application. |

## 5. System Integration
Bridge the gap between Workbench and the OS.

| Command Name | Default Keybinding | Description |
| :--- | :--- | :--- |
| **System: Open Default** | `Enter` | Open selected file in default OS application (already implemented, but could be a command). |
| **System: Open with...** | palette | Show a dialog to choose which application to open the file with. |

## 6. Future: Editor Commands (When Editor is Added)
*Note: These are deferred until an internal text editor is implemented.*

- Edit: Find / Replace
- Edit: Go to Line
- Edit: Save / Save As
- Edit: Undo / Redo
