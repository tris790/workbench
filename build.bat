@echo off
REM build.bat - Windows build script for Workbench
REM Usage: build.bat [debug|release]

setlocal EnableDelayedExpansion

REM Set build mode (default: debug)
set MODE=debug
if "%1"=="release" set MODE=release

REM Create build directory
if not exist build mkdir build

REM Build and run the embedding tool (native C approach)
cl /nologo /O2 scripts/embed.c /Fe:build/embed.exe
build\embed.exe

REM Set compiler flags based on mode
if "%MODE%"=="release" (
    echo Building in RELEASE mode...
    set CFLAGS=/O2 /DNDEBUG
) else (
    echo Building in DEBUG mode...
    set CFLAGS=/Od /Zi /DWB_DEBUG
)

REM Common flags
set CFLAGS=%CFLAGS% /W4 /WX /DUNICODE /D_UNICODE /D_CRT_SECURE_NO_WARNINGS

REM Include paths
set INCLUDES=/I src /I src\core /I src\platform /I src\renderer /I src\ui

REM Source files - Core
set CORE_SRC=src\core\animation.c src\core\fs.c src\core\theme.c src\core\assets_embedded.c

REM Source files - Platform (Windows)
set PLATFORM_SRC=src\platform\windows\windows_platform.c ^
    src\platform\windows\windows_window.c ^
    src\platform\windows\windows_events.c ^
    src\platform\windows\windows_filesystem.c ^
    src\platform\windows\windows_clipboard.c ^
    src\platform\windows\windows_process.c ^
    src\platform\windows\windows_time.c ^
    src\platform\windows\windows_fs_watcher.c ^
    src\platform\windows\windows_font.c ^
    src\platform\windows\windows_pty.c ^
    src\platform\windows\workbench.rc

REM Source files - Renderer
set RENDERER_SRC=src\renderer\renderer_software.c src\renderer\icons.c

REM Source files - UI
set UI_SRC=src\ui\ui.c src\ui\layout.c src\ui\input.c src\ui\key_repeat.c src\ui\commands.c ^
    src\ui\components\explorer.c ^
    src\ui\components\command_palette.c ^
    src\ui\components\context_menu.c ^
    src\ui\components\terminal.c ^
    src\ui\components\quick_filter.c

REM Main entry point
set MAIN_SRC=src\main.c

REM Libraries
set LIBS=user32.lib gdi32.lib shell32.lib shlwapi.lib dwrite.lib uuid.lib

REM Output
set OUTPUT=/Fe:build\wb.exe

REM Set subsystem based on mode
REM CONSOLE for debug (allows printf debugging), WINDOWS for release (proper GUI app)
if "%MODE%"=="release" (
    set SUBSYSTEM=WINDOWS
) else (
    set SUBSYSTEM=CONSOLE
)

REM Full command
echo Compiling...
cl.exe %CFLAGS% %INCLUDES% %OUTPUT% ^
    %MAIN_SRC% %CORE_SRC% %PLATFORM_SRC% %RENDERER_SRC% %UI_SRC% ^
    %LIBS% ^
    /link /SUBSYSTEM:%SUBSYSTEM%

if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    exit /b 1
)

echo Build successful: build\wb.exe
