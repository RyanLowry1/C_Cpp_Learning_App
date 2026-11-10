@echo off
rem ======================================================================
rem  Malloc's Library 2D (V2) - build script (MSVC + raylib)
rem    build.bat          release build (no console window)
rem    build.bat debug    build with a console window for log output
rem ======================================================================
setlocal enabledelayedexpansion
cd /d "%~dp0"

where cl >nul 2>nul
if not errorlevel 1 goto have_cl

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" goto no_vs
for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
if not defined VSINSTALL goto no_vs
call "!VSINSTALL!\VC\Auxiliary\Build\vcvars64.bat" >nul
goto have_cl

:no_vs
echo [build] Could not find the Visual Studio C++ build tools.
echo [build] Install "Desktop development with C++" from the Visual Studio Installer.
exit /b 1

:have_cl
if not exist build mkdir build

set "SUBSYS=/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
if /i "%~1"=="debug" set "SUBSYS=/SUBSYSTEM:CONSOLE"

set CFLAGS=/nologo /std:c++17 /EHsc /MD /O2 /MP /W3 /utf-8 /D_CRT_SECURE_NO_WARNINGS /I include /I src
set LIBS=raylib.lib opengl32.lib gdi32.lib winmm.lib user32.lib shell32.lib

echo [build] Compiling Malloc's Library 2D (V2)...
cl %CFLAGS% src\*.cpp src\views\*.cpp /Fo:build\ /Fe:MallocsLibrary_V2.exe /link /LIBPATH:lib %LIBS% !SUBSYS!
if errorlevel 1 (
    echo [build] FAILED
    exit /b 1
)
echo [build] OK  -^>  MallocsLibrary_V2.exe
exit /b 0
