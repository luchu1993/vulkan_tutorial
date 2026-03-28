@echo off
setlocal enabledelayedexpansion

set PROJECT_ROOT=%~dp0
set THIRDPARTY_DIR=%PROJECT_ROOT%thirdparty

echo ============================================
echo  Tutorial Project - Third-party Setup
echo ============================================

:: -------------------- GLFW --------------------
set GLFW_VERSION=3.4
set GLFW_DIR=%THIRDPARTY_DIR%\glfw
set GLFW_SRC_DIR=%THIRDPARTY_DIR%\_src\glfw
set GLFW_BUILD_DIR=%THIRDPARTY_DIR%\_src\glfw\build

set GLFW_NEED_BUILD=1
if exist "%GLFW_DIR%\lib\Release\glfw3.lib" (
    if exist "%GLFW_DIR%\lib\Debug\glfw3.lib" set GLFW_NEED_BUILD=0
)
if "!GLFW_NEED_BUILD!"=="0" (
    echo [GLFW] Already built, skipping.
) else (
    echo [GLFW] Downloading GLFW %GLFW_VERSION% ...

    if not exist "%THIRDPARTY_DIR%\_src" mkdir "%THIRDPARTY_DIR%\_src"

    if not exist "%GLFW_SRC_DIR%" (
        git clone --depth 1 --branch %GLFW_VERSION% https://github.com/glfw/glfw.git "%GLFW_SRC_DIR%"
        if errorlevel 1 (
            echo [GLFW] ERROR: git clone failed.
            exit /b 1
        )
    )

    echo [GLFW] Configuring ...
    cmake -S "%GLFW_SRC_DIR%" -B "%GLFW_BUILD_DIR%" ^
        -DGLFW_BUILD_DOCS=OFF ^
        -DGLFW_BUILD_TESTS=OFF ^
        -DGLFW_BUILD_EXAMPLES=OFF ^
        -DCMAKE_INSTALL_PREFIX="%GLFW_DIR%"
    if errorlevel 1 (
        echo [GLFW] ERROR: cmake configure failed.
        exit /b 1
    )

    echo [GLFW] Building Release ...
    cmake --build "%GLFW_BUILD_DIR%" --config Release
    if errorlevel 1 (
        echo [GLFW] ERROR: cmake build Release failed.
        exit /b 1
    )

    echo [GLFW] Building Debug ...
    cmake --build "%GLFW_BUILD_DIR%" --config Debug
    if errorlevel 1 (
        echo [GLFW] ERROR: cmake build Debug failed.
        exit /b 1
    )

    :: Install Release
    cmake --install "%GLFW_BUILD_DIR%" --config Release
    if errorlevel 1 (
        echo [GLFW] ERROR: cmake install Release failed.
        exit /b 1
    )
    :: Move Release lib to lib\Release
    if not exist "%GLFW_DIR%\lib\Release" mkdir "%GLFW_DIR%\lib\Release"
    move /y "%GLFW_DIR%\lib\glfw3.lib" "%GLFW_DIR%\lib\Release\glfw3.lib"

    :: Install Debug
    cmake --install "%GLFW_BUILD_DIR%" --config Debug
    if errorlevel 1 (
        echo [GLFW] ERROR: cmake install Debug failed.
        exit /b 1
    )
    :: Move Debug lib and PDB to lib\Debug
    if not exist "%GLFW_DIR%\lib\Debug" mkdir "%GLFW_DIR%\lib\Debug"
    move /y "%GLFW_DIR%\lib\glfw3.lib" "%GLFW_DIR%\lib\Debug\glfw3.lib"
    if exist "%GLFW_BUILD_DIR%\src\Debug\glfw3.pdb" copy /y "%GLFW_BUILD_DIR%\src\Debug\glfw3.pdb" "%GLFW_DIR%\lib\Debug\glfw3.pdb"

    echo [GLFW] Installed to %GLFW_DIR% (Debug + Release^)
)

:: -------------------- GLM (header-only) --------------------
set GLM_VERSION=1.0.1
set GLM_DIR=%THIRDPARTY_DIR%\glm

if exist "%GLM_DIR%\include\glm\glm.hpp" (
    echo [GLM] Already installed, skipping.
) else (
    echo [GLM] Downloading GLM %GLM_VERSION% ...

    if not exist "%THIRDPARTY_DIR%\_src" mkdir "%THIRDPARTY_DIR%\_src"

    set GLM_SRC_DIR=%THIRDPARTY_DIR%\_src\glm
    if not exist "!GLM_SRC_DIR!" (
        git clone --depth 1 --branch %GLM_VERSION% https://github.com/g-truc/glm.git "!GLM_SRC_DIR!"
        if errorlevel 1 (
            echo [GLM] ERROR: git clone failed.
            exit /b 1
        )
    )

    if not exist "%GLM_DIR%\include\glm" mkdir "%GLM_DIR%\include\glm"
    xcopy /s /e /q /y "!GLM_SRC_DIR!\glm\*" "%GLM_DIR%\include\glm\"
    echo [GLM] Installed to %GLM_DIR%
)

:: -------------------- tinyobjloader (header-only) --------------------
set TINYOBJ_VERSION=v2.0.0rc13
set TINYOBJ_DIR=%THIRDPARTY_DIR%\tinyobjloader

if exist "%TINYOBJ_DIR%\include\tiny_obj_loader.h" (
    echo [tinyobjloader] Already installed, skipping.
) else (
    echo [tinyobjloader] Downloading tinyobjloader %TINYOBJ_VERSION% ...

    if not exist "%THIRDPARTY_DIR%\_src" mkdir "%THIRDPARTY_DIR%\_src"

    set TINYOBJ_SRC_DIR=%THIRDPARTY_DIR%\_src\tinyobjloader
    if not exist "!TINYOBJ_SRC_DIR!" (
        git clone --depth 1 --branch %TINYOBJ_VERSION% https://github.com/tinyobjloader/tinyobjloader.git "!TINYOBJ_SRC_DIR!"
        if errorlevel 1 (
            echo [tinyobjloader] ERROR: git clone failed.
            exit /b 1
        )
    )

    if not exist "%TINYOBJ_DIR%\include" mkdir "%TINYOBJ_DIR%\include"
    copy /y "!TINYOBJ_SRC_DIR!\tiny_obj_loader.h" "%TINYOBJ_DIR%\include\"
    echo [tinyobjloader] Installed to %TINYOBJ_DIR%
)

:: -------------------- Cleanup --------------------
if exist "%THIRDPARTY_DIR%\_src" (
    echo [Cleanup] Removing temporary source directory ...
    rmdir /s /q "%THIRDPARTY_DIR%\_src"
    echo [Cleanup] Done.
)

echo.
echo ============================================
echo  Setup complete!
echo ============================================
echo.
echo Next steps:
echo   1. mkdir build ^&^& cd build
echo   2. cmake ..
echo   3. cmake --build . --config Release
echo.

endlocal
