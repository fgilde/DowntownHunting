# Starts the game (uncooked, -game) with a DH.Run step list and waits until Saved/DHShots/<Last>.png exists.
# Closes running editor/game instances first and clears Saved/DHShots.
#   .\Tools\Test-Game.ps1 -Url "/Game/DH/Maps/DH_Block?Experience=B_DH_TDM" -Steps "wait 40; DH.Shot a; wait 5; DH.Shot end" -Last end
# Step commands: DH.Shot <name> [noui], DH.Key <Key[:Seconds]>, DH.Teleport X Y Z [Yaw], DH.OpenScreen <Screen>, DH.Streak <N>.
param([string]$Steps, [string]$Last = "end", [string]$Url = "", [int]$Timeout = 240)
$repo = Split-Path $PSScriptRoot -Parent
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
$shots = Join-Path $repo "Saved\DHShots"
if (Test-Path $shots) { Get-ChildItem $shots | ForEach-Object { $_.Delete() } }
$exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
$arguments = "`"$repo\DowntownHunting.uproject`" $Url -game -windowed -ResX=1600 -ResY=900 `"-ExecCmds=DisableAllScreenMessages, DH.Run $Steps`" -log=Game.log"
Start-Process $exe -ArgumentList $arguments
$t = 0
while ($t -lt $Timeout -and -not (Test-Path (Join-Path $shots "$Last.png"))) { Start-Sleep 5; $t += 5 }
Start-Sleep 1
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
"t=$t"
Get-ChildItem $shots -ErrorAction SilentlyContinue | ForEach-Object Name
