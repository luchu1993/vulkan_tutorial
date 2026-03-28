@echo off

set PROJECT_ROOT=%~dp0

echo ============================================
echo  Tutorial Project - Clean
echo ============================================

:: -------------------- build --------------------
if exist "%PROJECT_ROOT%build" (
    echo [Clean] Removing build\ ...
    rmdir /s /q "%PROJECT_ROOT%build"
)

:: -------------------- bin --------------------
if exist "%PROJECT_ROOT%bin" (
    echo [Clean] Removing bin\ ...
    rmdir /s /q "%PROJECT_ROOT%bin"
)

:: -------------------- thirdparty --------------------
if exist "%PROJECT_ROOT%thirdparty" (
    echo [Clean] Removing thirdparty\ ...
    rmdir /s /q "%PROJECT_ROOT%thirdparty"
)

echo.
echo [Clean] Done.
