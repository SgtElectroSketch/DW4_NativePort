@echo off
setlocal

where python.exe >nul 2>nul
if errorlevel 1 (
    echo Python 3 was not found on PATH. 1>&2
    exit /b 1
)

set "ASSET_OUT=%DW4_ASSET_OUT%"
if "%ASSET_OUT%"=="" set "ASSET_OUT=%~dp0native\assets\generated"

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\prepare-native-asset-catalogs.ps1" ^
    -RepositoryRoot "%~dp0." -OutputPath "%ASSET_OUT%" -ValidateOutputOnly
if errorlevel 1 exit /b %ERRORLEVEL%

call "%~dp0reference\build.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%

python.exe "%~dp0reference\tools\AssetExtract\extract_assets.py" ^
    --rom "%~dp0reference\build\Release\Dragon Warrior IV (USA).nes" ^
    --out "%ASSET_OUT%" %*
if errorlevel 1 exit /b %ERRORLEVEL%

powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\prepare-native-asset-catalogs.ps1" ^
    -RepositoryRoot "%~dp0." -OutputPath "%ASSET_OUT%"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%

python.exe "%~dp0scripts\port\extract-native-title.py" ^
    --rom "%~dp0reference\build\Release\Dragon Warrior IV (USA).nes" ^
    --out "%ASSET_OUT%\title"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
python.exe "%~dp0scripts\port\extract-native-opening.py" ^
    --rom "%~dp0reference\build\Release\Dragon Warrior IV (USA).nes" ^
    --output "%ASSET_OUT%\opening" --verify-curves
exit /b %ERRORLEVEL%