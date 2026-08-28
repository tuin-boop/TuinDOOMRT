@echo off
setlocal
set "CSC=C:\Windows\Microsoft.NET\Framework64\v4.0.30319\csc.exe"
set "OUT=%~dp0..\..\build\launcher"
if not exist "%OUT%" mkdir "%OUT%"
if not exist "%OUT%\Assets" mkdir "%OUT%\Assets"
copy /y "%~dp0assets\tuindoom-face.png" "%OUT%\Assets\tuindoom-face.png" >nul
copy /y "%~dp0assets\tuindoom-launcher-art.png" "%OUT%\Assets\tuindoom-launcher-art.png" >nul
copy /y "%~dp0assets\tuindoom-desktop-icon.png" "%OUT%\Assets\tuindoom-desktop-icon.png" >nul
copy /y "%~dp0assets\intro-*.png" "%OUT%\Assets\" >nul
copy /y "%~dp0assets\launcher-sky-*.png" "%OUT%\Assets\" >nul
"%CSC%" /nologo /target:winexe /optimize+ /platform:x64 /win32icon:"%~dp0assets\TuinDoomRT-v2.ico" /out:"%OUT%\TuinDoomRT.exe" /reference:System.dll /reference:System.Core.dll /reference:System.Drawing.dll /reference:System.Windows.Forms.dll /reference:System.Web.Extensions.dll "%~dp0Program.cs"
exit /b %errorlevel%
