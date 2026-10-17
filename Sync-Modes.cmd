@echo off
rem Writes the game modes from Modes.json into the menu (Play). Close the editor first.
"C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" "%~dp0DowntownHunting.uproject" -run=pythonscript -script="%~dp0Tools\Python\sync_modes.py" -nullrhi -nosound -unattended -nopause -stdout | findstr /C:"sync_modes" /C:"Error"
pause
