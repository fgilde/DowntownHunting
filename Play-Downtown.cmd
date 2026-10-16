@echo off
rem Jumps straight into Team Deathmatch on the City Sample block (DH_Block) with bots.
rem Ego perspective: Play-Downtown.cmd FP
set EXP=B_DH_TDM
if /I "%1"=="FP" set EXP=B_DH_TDM_FP
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" "/Game/DH/Maps/DH_Block?Experience=%EXP%" -game -windowed -ResX=1600 -ResY=900
