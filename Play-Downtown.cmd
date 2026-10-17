@echo off
rem Jumps straight into a match on the City Sample block (DH_Block) with bots.
rem   Play-Downtown.cmd            Team Deathmatch, switch perspective with V / D-pad up
rem   Play-Downtown.cmd FP         Team Deathmatch, first person only
rem   Play-Downtown.cmd DOM        Domination, switch perspective
rem Rules are URL options (see docs\PLAN.md, "Modi aus Konfiguration").
set EXP=B_DH_TDM
set RULES=?Perspective=Choice?Loadout=Pistol,Rifle,Shotgun
if /I "%1"=="FP" set RULES=?Perspective=FP?Loadout=Pistol,Rifle,Shotgun
if /I "%1"=="DOM" set EXP=B_DH_Domination
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" "/Game/DH/Maps/DH_Block?Experience=%EXP%%RULES%" -game -windowed -ResX=1600 -ResY=900 -log=Game.log
