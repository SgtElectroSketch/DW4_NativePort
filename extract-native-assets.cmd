@echo off
setlocal

where python.exe >nul 2>nul
if errorlevel 1 (
    echo Python 3 was not found on PATH. 1>&2
    exit /b 1
)

set "ASSET_OUT=%DW4_ASSET_OUT%"
if "%ASSET_OUT%"=="" set "ASSET_OUT=%~dp0native\assets\generated"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\assert-asset-output-ignored.ps1" ^
    -RepositoryRoot "%~dp0." -OutputPath "%ASSET_OUT%"
if errorlevel 1 exit /b %ERRORLEVEL%

call "%~dp0reference\build.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%

python.exe "%~dp0reference\tools\AssetExtract\extract_assets.py" ^
    --rom "%~dp0reference\build\Release\Dragon Warrior IV (USA).nes" ^
    --out "%ASSET_OUT%" %*
exit /b %ERRORLEVEL%