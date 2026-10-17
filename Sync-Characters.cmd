@echo off
rem Builds the playable crowd characters from Characters.json. Close the editor first.
rem   1. City Sample: resolve Characters.json (meshes, materials, hair) and list the packages to import
rem   2. Copy those packages from City Sample into this project
rem   3. Rebuild /DowntownCore/Characters/Crowd and the character list in Config\DefaultGame.ini
set UE=C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe
set CITYSAMPLE=%USERPROFILE%\Documents\Unreal Projects\CitySample
set CROWD=%~dp0Plugins\GameFeatures\DowntownCore\Content\Characters\Crowd
"%UE%" "%CITYSAMPLE%\CitySample.uproject" -run=pythonscript -script="%~dp0Tools\Python\cs_crowd_export.py" -nullrhi -nosound -unattended -nopause -stdout | findstr /C:"CROWD" /C:"Error:"
powershell -ExecutionPolicy Bypass -File "%~dp0Tools\Import-CitySample.ps1" -CitySample "%CITYSAMPLE%" -Manifest "%~dp0Tools\CitySample_Crowd.txt"
if exist "%CROWD%" rd /s /q "%CROWD%"
"%UE%" "%~dp0DowntownHunting.uproject" -run=pythonscript -script="%~dp0Tools\Python\sync_characters.py" -nullrhi -nosound -unattended -nopause -stdout | findstr /C:"CHARS" /C:"Error:"
pause
