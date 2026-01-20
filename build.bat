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
set INCLUDES=/I src /I src\core /I src\platform /I src\renderer /I src\ui /I src\ui\components /I src\terminal /I src\config

REM Source files (Unity Build)
set SOURCES=src\unity_windows.c src\platform\windows\workbench.rc

REM Libraries
set LIBS=user32.lib gdi32.lib shell32.lib shlwapi.lib dwrite.lib uuid.lib opengl32.lib

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
echo Compiling (unity build)...
cl.exe %CFLAGS% %INCLUDES% %OUTPUT% ^
    %SOURCES% ^
    %LIBS% ^
    /link /SUBSYSTEM:%SUBSYSTEM%

if %ERRORLEVEL% NEQ 0 (
    echo Build FAILED!
    exit /b 1
)

echo Build successful: build\wb.exe
