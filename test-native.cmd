@echo off
setlocal
call "%~dp0build-native.cmd"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
"%~dp0build\Release\DW4.Tests.exe"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-asset-catalogs.ps1"
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-replay.ps1" -Configuration Release
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-graphics.ps1" -Configuration Release
if not "%ERRORLEVEL%"=="0" exit /b %ERRORLEVEL%
if exist "%~dp0native\assets\generated\title\index.json" (
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-title.ps1" -Configuration Release
	if errorlevel 1 exit /b 1
	powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-adventure-log.ps1" -Configuration Release
	if errorlevel 1 exit /b 1
	if exist "%~dp0native\assets\generated\opening\index.json" (
		powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\port\test-native-gameplay.ps1" -Configuration Release
	)
) else (
	echo Local title parity skipped: prepare native title assets to enable reference image checks.
)
exit /b %ERRORLEVEL%