@echo off
rem Jumps straight into a match in the city with bots.
rem   Play-Downtown.cmd            Team Deathmatch on Downtown (DH_Block), switch perspective with V / D-pad up
rem   Play-Downtown.cmd FP         first person only
rem   Play-Downtown.cmd DOM        Domination
rem   Play-Downtown.cmd PARK       Waterfront (DH_Park) instead of Downtown; combinable: PARK DOM FP
rem   Play-Downtown.cmd DEPOT      Depot (Lyra's Convolution, restyled)
rem   Play-Downtown.cmd OVERPASS   Overpass (under the elevated freeway)
rem   Play-Downtown.cmd GG         Gun Game (every kill the next weapon)
rem Rules are URL options (see docs\PLAN.md, "Modi aus Konfiguration").
set EXP=B_DH_TDM
set MAP=/Game/DH/Maps/DH_Block
set RULES=?Perspective=Choice?Loadout=Player
for %%A in (%*) do (
  if /I "%%A"=="FP" set RULES=?Perspective=FP?Loadout=Player
  if /I "%%A"=="DOM" set EXP=B_DH_Domination
  if /I "%%A"=="PARK" set MAP=/Game/DH/Maps/DH_Park
  if /I "%%A"=="DEPOT" set MAP=/DowntownCore/Maps/DH_Depot
  if /I "%%A"=="OVERPASS" set MAP=/Game/DH/Maps/DH_Freeway
  if /I "%%A"=="GG" set RULES=?Perspective=Choice?GunGame=MP5,AK47,Rifle,Shotgun,M1911,Pistol
)
start "" "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe" "%~dp0DowntownHunting.uproject" "%MAP%?Experience=%EXP%%RULES%" -game -windowed -ResX=1600 -ResY=900 -ExecCmds="DisableAllScreenMessages" -log=Game.log
