@echo off
setlocal
call "%~dp0build-native.cmd"
if errorlevel 1 exit /b %ERRORLEVEL%
"%~dp0build\Release\DW4.Tests.exe"
exit /b %ERRORLEVEL%