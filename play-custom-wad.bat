@echo off
setlocal

set "GAME_DIR=%~dp0build\test-release"
set "DOOM2_WAD=C:\Program Files (x86)\Steam\steamapps\common\Ultimate Doom\base\doom2\DOOM2.WAD"
set "PWAD=C:\DoomMods\DoomMaps\nerve.wad"

rem Drag a WAD onto this file to override the default NERVE.WAD test.
if not "%~1"=="" set "PWAD=%~f1"

if not exist "%GAME_DIR%\gzdoom-autosun.exe" goto missing_game
if not exist "%DOOM2_WAD%" goto missing_iwad
if not exist "%PWAD%" goto missing_pwad

start "GZDoom RT Enhanced" /D "%GAME_DIR%" "%GAME_DIR%\gzdoom-autosun.exe" -iwad "%DOOM2_WAD%" -file "%PWAD%" +rt_stockscenes 0 +rt_sun 1 +rt_sun_a 15 +rt_sun_b 0 +rt_sun_intensity 100 +rt_sun_color "ff a0 60" +rt_autosun 0 +rt_autosun_seed 0 +rt_volume_type 1 +rt_volume_scatter 1 +rt_volume_ambient 0.03 +rt_volume_lintensity 1 +rt_volume_lassymetry 0.5 +bind n rt_autosun_next
exit /b 0

:missing_game
echo The enhanced build was not found at:
echo %GAME_DIR%\gzdoom-autosun.exe
goto failed

:missing_iwad
echo DOOM2.WAD was not found at:
echo %DOOM2_WAD%
goto failed

:missing_pwad
echo Custom WAD was not found at:
echo %PWAD%

:failed
pause
exit /b 1
