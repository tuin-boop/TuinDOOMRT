@echo off
setlocal

set "GAME_DIR=%~dp0build\test-release"
set "DOOM_WAD=C:\Program Files (x86)\Steam\steamapps\common\Ultimate Doom\rerelease\doom.wad"
set "PWAD="

rem Run Ultimate Doom, or drag a Doom 1-compatible WAD onto this file.
if not "%~1"=="" set "PWAD=%~f1"

if not exist "%GAME_DIR%\gzdoom-autosun.exe" goto missing_game
if not exist "%DOOM_WAD%" goto missing_iwad
if not "%PWAD%"=="" goto check_pwad
goto launch_iwad

:check_pwad
if not exist "%PWAD%" goto missing_pwad
start "GZDoom RT Enhanced - Doom" /D "%GAME_DIR%" "%GAME_DIR%\gzdoom-autosun.exe" -rtdoom1 -iwad "%DOOM_WAD%" -file "%PWAD%"
exit /b 0

:launch_iwad
start "GZDoom RT Enhanced - Doom" /D "%GAME_DIR%" "%GAME_DIR%\gzdoom-autosun.exe" -rtdoom1 -iwad "%DOOM_WAD%"
exit /b 0

:missing_game
echo The enhanced build was not found at:
echo %GAME_DIR%\gzdoom-autosun.exe
goto failed

:missing_iwad
echo DOOM.WAD was not found at:
echo %DOOM_WAD%
goto failed

:missing_pwad
echo Custom WAD was not found at:
echo %PWAD%

:failed
pause
exit /b 1
