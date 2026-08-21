@echo off
setlocal
set "ISCC=%LOCALAPPDATA%\Programs\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" set "ISCC=C:\Program Files (x86)\Inno Setup 6\ISCC.exe"
if not exist "%ISCC%" (
  echo Inno Setup 6 was not found.
  exit /b 1
)
call "%~dp0..\launcher\TuinDoomRT\build-launcher.cmd"
if errorlevel 1 exit /b %errorlevel%
"%ISCC%" "%~dp0TuinDoomRT.iss"
exit /b %errorlevel%
