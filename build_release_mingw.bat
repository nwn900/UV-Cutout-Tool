@echo off
setlocal

set "EXIT_CODE=0"

rem Release build helper for the verified Qt 6.11.0 MinGW toolchain.
rem Override any of these before running if your install lives elsewhere:
rem   set QT_VERSION=6.11.0
rem   set QT_ROOT=C:\Qt
rem   set BUILD_DIR=build

if not defined QT_VERSION set "QT_VERSION=6.11.0"
if not defined QT_ROOT set "QT_ROOT=C:\Qt"
if not defined BUILD_DIR set "BUILD_DIR=build"

for %%I in ("%~dp0.") do set "SCRIPT_DIR=%%~fI"
set "CMAKE_EXE=%QT_ROOT%\Tools\CMake_64\bin\cmake.exe"
set "NINJA_DIR=%QT_ROOT%\Tools\Ninja"
set "MINGW_BIN=%QT_ROOT%\Tools\mingw1310_64\bin"
set "QT_PREFIX=%QT_ROOT%\%QT_VERSION%\mingw_64"
set "WINDEPLOYQT_EXE=%QT_PREFIX%\bin\windeployqt.exe"
set "BUILD_PATH=%SCRIPT_DIR%\%BUILD_DIR%"
set "EXE_PATH=%BUILD_PATH%\UV Cutout Tool.exe"

echo.
echo [1/4] Checking toolchain paths...

if not exist "%CMAKE_EXE%" (
    echo ERROR: cmake.exe not found at:
    echo   %CMAKE_EXE%
    set "EXIT_CODE=1"
    goto :finish
)

if not exist "%NINJA_DIR%\ninja.exe" (
    echo ERROR: ninja.exe not found at:
    echo   %NINJA_DIR%\ninja.exe
    set "EXIT_CODE=1"
    goto :finish
)

if not exist "%MINGW_BIN%\g++.exe" (
    echo ERROR: g++.exe not found at:
    echo   %MINGW_BIN%\g++.exe
    set "EXIT_CODE=1"
    goto :finish
)

if not exist "%QT_PREFIX%\bin\qmake.exe" (
    echo ERROR: Qt MinGW prefix not found at:
    echo   %QT_PREFIX%
    set "EXIT_CODE=1"
    goto :finish
)

set "PATH=%QT_ROOT%\Tools\CMake_64\bin;%NINJA_DIR%;%MINGW_BIN%;%PATH%"

echo.
echo [2/4] Configuring CMake...
"%CMAKE_EXE%" -S "%SCRIPT_DIR%" -B "%BUILD_PATH%" -G Ninja -DCMAKE_PREFIX_PATH="%QT_PREFIX%" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 (
    set "EXIT_CODE=%errorlevel%"
    goto :finish
)

echo.
echo [3/4] Building Release...
"%CMAKE_EXE%" --build "%BUILD_PATH%" --config Release
if errorlevel 1 (
    set "EXIT_CODE=%errorlevel%"
    goto :finish
)

if not exist "%EXE_PATH%" (
    echo ERROR: Build completed but executable was not found at:
    echo   %EXE_PATH%
    set "EXIT_CODE=1"
    goto :finish
)

echo.
echo [4/4] Running windeployqt...
if exist "%WINDEPLOYQT_EXE%" (
    "%WINDEPLOYQT_EXE%" --release --no-translations "%EXE_PATH%"
    if errorlevel 1 (
        set "EXIT_CODE=%errorlevel%"
        goto :finish
    )
) else (
    echo WARNING: windeployqt.exe not found at:
    echo   %WINDEPLOYQT_EXE%
    echo Skipping deployment step.
)

:finish
echo.
if "%EXIT_CODE%"=="0" (
    echo Build complete:
    echo   %EXE_PATH%
) else (
    echo Build failed with exit code %EXIT_CODE%.
)

echo.
pause

endlocal
exit /b %EXIT_CODE%
