@echo off
rem Starts the game with Steam online (Lyra's Config/Custom/Steam): host and join over the internet, invite Steam friends.
rem Needs the Steam client running and logged in. Uses Steam's developer app id 480 (Spacewar) until the game has its own.
rem   Play -> host a match with "Online" or browse matches; friends start this script too.
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" -game -windowed -ResX=1600 -ResY=900 -CustomConfig=Steam -ExecCmds="DisableAllScreenMessages" -log=Online.log %*
