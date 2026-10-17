@echo off
rem Jumps straight into a match in the city with bots.
rem   Play-Downtown.cmd            Team Deathmatch on Downtown (DH_Block), switch perspective with V / D-pad up
rem   Play-Downtown.cmd FP         first person only
rem   Play-Downtown.cmd DOM        Domination
rem   Play-Downtown.cmd PARK       Waterfront (DH_Park) instead of Downtown; combinable: PARK DOM FP
rem Rules are URL options (see docs\PLAN.md, "Modi aus Konfiguration").
set EXP=B_DH_TDM
set MAP=DH_Block
set RULES=?Perspective=Choice?Loadout=Pistol,Rifle,Shotgun
for %%A in (%*) do (
  if /I "%%A"=="FP" set RULES=?Perspective=FP?Loadout=Pistol,Rifle,Shotgun
  if /I "%%A"=="DOM" set EXP=B_DH_Domination
  if /I "%%A"=="PARK" set MAP=DH_Park
)
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" "/Game/DH/Maps/%MAP%?Experience=%EXP%%RULES%" -game -windowed -ResX=1600 -ResY=900 -log=Game.log
