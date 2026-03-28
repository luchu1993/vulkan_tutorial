@echo off
setlocal

for %%i in ("%~dp0.") do set PROJECT_ROOT=%%~fi
set BUILD_DIR=%PROJECT_ROOT%\build

:: Default config is Debug
set CONFIG=%~1
if "%CONFIG%"=="" set CONFIG=Debug

echo ============================================
echo  vulkan_tutorial - Build (%CONFIG%)
echo ============================================

:: Configure if needed
if not exist "%BUILD_DIR%" (
    echo [Build] Configuring ...
    cmake -S "%PROJECT_ROOT%" -B "%BUILD_DIR%"
    if errorlevel 1 (
        echo [Build] ERROR: cmake configure failed.
        exit /b 1
    )
)

:: Build
echo [Build] Building %CONFIG% ...
cmake --build "%BUILD_DIR%" --config %CONFIG%
if errorlevel 1 (
    echo [Build] ERROR: build failed.
    exit /b 1
)

echo.
echo [Build] Success! Output in bin\%CONFIG%\
endlocal
