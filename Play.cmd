@echo off
rem Starts Downtown Hunting as a game (no editor UI) with the main menu.
rem Uses the installed UE 5.8 editor binaries; a packaged build is made with Package.cmd.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="DisableAllScreenMessages" %*
