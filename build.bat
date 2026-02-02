@echo off
REM build.bat - Windows build wrapper for Workbench
REM This delegates to build_windows.sh which uses zig cc
REM 
REM Usage: build.bat [debug|release]
REM Requires: Git Bash (comes with Git for Windows) and Zig

setlocal EnableDelayedExpansion

REM Pass all arguments to the bash script
set ARGS=%*

REM Check if sh is available (Git Bash)
where sh >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: Git Bash not found. Please install Git for Windows.
    echo Download from: https://git-scm.com/download/win
    exit /b 1
)

REM Check if zig is available
where zig >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: Zig compiler not found. Please install Zig.
    echo Download from: https://ziglang.org/download/
    exit /b 1
)

REM Run the bash build script
sh build_windows.sh %ARGS%
