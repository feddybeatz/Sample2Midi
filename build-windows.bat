@echo off
REM ============================================================
REM  Sample2MIDI - Windows Visual Studio Build Script
REM  Generates a Visual Studio solution (.sln) for FL Studio
REM ============================================================

setlocal enabledelayedexpansion

REM Check for clean argument
if "%1"=="clean" (
    echo.
    echo  Cleaning Build-Windows directory...
    if exist "Build-Windows" (
        rmdir /s /q "Build-Windows"
        echo  Clean complete.
    ) else (
        echo  No Build-Windows directory to clean.
    )
    exit /b 0
)

REM Check for rebuild argument (clean + build)
if "%1"=="rebuild" (
    echo.
    echo  Cleaning Build-Windows directory...
    if exist "Build-Windows" (
        rmdir /s /q "Build-Windows"
        echo  Clean complete.
    )
    echo  Starting fresh build...
    echo.
)

echo.
echo  Sample2MIDI - Windows Build Setup
echo  ===================================
echo.

REM Auto-detect Visual Studio version using vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"

for /f "usebackq tokens=*" %%i in (
    `"%VSWHERE%" -latest -property installationVersion`
) do set VS_VERSION=%%i

for /f "tokens=1 delims=." %%v in ("%VS_VERSION%") do set VS_MAJOR=%%v

if "%VS_MAJOR%"=="18" set VS_GENERATOR=Visual Studio 18 2026
if "%VS_MAJOR%"=="17" set VS_GENERATOR=Visual Studio 17 2022
if "%VS_MAJOR%"=="16" set VS_GENERATOR=Visual Studio 16 2019
if "%VS_MAJOR%"=="15" set VS_GENERATOR=Visual Studio 15 2017

if not defined VS_GENERATOR (
    set VS_GENERATOR=Visual Studio 17 2022
)

echo  Detected Visual Studio version: %VS_VERSION%
echo  Using CMake generator: %VS_GENERATOR%
echo.

echo  [1/2] Creating Build-Windows directory...
if not exist "Build-Windows" mkdir "Build-Windows"

REM Get the VS installation path for CMake hint
for /f "usebackq tokens=*" %%i in (
    `"%VSWHERE%" -latest -property installationPath`
) do set VS_INSTALL_PATH=%%i

echo  [2/2] Generating Visual Studio solution...
cmake -S . -B Build-Windows -G "%VS_GENERATOR%" -A x64 ^
    -DCMAKE_GENERATOR_INSTANCE="%VS_INSTALL_PATH%"
if %ERRORLEVEL% NEQ 0 (
    echo.
    echo  ERROR: CMake configuration failed.
    echo  Make sure the JUCE submodule is present:
    echo    git submodule update --init --recursive
    pause
    exit /b 1
)

REM Copy ONNX Runtime DLL to build output if it exists in project root
if exist "onnxruntime.dll" (
    echo.
    echo  Copying ONNX Runtime DLL to build output...
    if not exist "Build-Windows\bin" mkdir "Build-Windows\bin"
    copy /Y "onnxruntime.dll" "Build-Windows\bin\"
    copy /Y "onnxruntime.dll" "Build-Windows\bin\Release\"
)

echo.
echo  ============================================================
echo   SUCCESS: Open  Build-Windows\Sample2MIDI.sln  in Visual Studio
echo  ============================================================
echo.
echo  Steps to build the VST3 for FL Studio:
echo    1. Open Build-Windows\Sample2MIDI.sln
echo    2. Set configuration: Release  ^|  platform: x64
echo    3. Right-click Sample2MIDI_VST3 ^> Build
echo    4. VST3 installs to:
echo       %%PROGRAMFILES%%\Common Files\VST3\Sample2MIDI.vst3
echo    5. FL Studio ^> Options ^> Manage Plugins ^> scan VST3 folder
echo.
echo  OR use command line:
echo    cmake --build Build-Windows --config Release
echo.
pause
