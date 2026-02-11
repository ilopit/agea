@echo off
REM Convert .glb files to .ozz animation files using gltf2ozz
REM Usage: tools\convert_animations.bat [path_to_glb]
REM   If no argument given, converts all .glb files under resources/

setlocal enabledelayedexpansion

set "GLTF2OZZ=%~dp0..\build\thirdparty\upstream\ozz-animation\src\animation\offline\gltf\gltf2ozz.exe"

if not exist "%GLTF2OZZ%" (
    echo ERROR: gltf2ozz not found at %GLTF2OZZ%
    echo Build it first: tools\build.bat gltf2ozz
    exit /b 1
)

if "%~1"=="" (
    echo Scanning for .glb files in resources\...
    for /r "%~dp0..\resources" %%F in (*.glb) do (
        call :convert "%%F"
    )
) else (
    call :convert "%~1"
)

echo Done.
exit /b 0

:convert
set "GLB_PATH=%~1"
set "GLB_DIR=%~dp1"
set "GLB_NAME=%~n1"

echo Converting: %GLB_PATH%
pushd "%GLB_DIR%"

"%GLTF2OZZ%" --file="%GLB_NAME%.glb"
if errorlevel 1 (
    echo ERROR: Failed to convert %GLB_PATH%
    popd
    exit /b 1
)

REM Rename output files: skeleton.ozz -> {stem}_skeleton.ozz, {anim}.ozz -> {stem}_{anim}.ozz
if exist "skeleton.ozz" (
    move /Y "skeleton.ozz" "%GLB_NAME%_skeleton.ozz" >nul
    echo   Renamed skeleton.ozz -> %GLB_NAME%_skeleton.ozz
)

for %%A in (*.ozz) do (
    set "OZZ_NAME=%%~nA"
    REM Skip files already prefixed
    echo !OZZ_NAME! | findstr /b "%GLB_NAME%_" >nul
    if errorlevel 1 (
        move /Y "%%A" "%GLB_NAME%_!OZZ_NAME!.ozz" >nul
        echo   Renamed %%A -> %GLB_NAME%_!OZZ_NAME!.ozz
    )
)

popd
exit /b 0
