# LAN through the menus: instance 1 hosts via the play screen (network LAN), instance 2 finds it in the join screen and joins.
$repo = "C:\dev\privat\github\DowntownHunting"
$exe = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor.exe"
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
Start-Sleep 2
$shots = Join-Path $repo "Saved\DHShots"
Get-ChildItem $shots -ErrorAction SilentlyContinue | ForEach-Object { $_.Delete() }
$h = "wait 35; DH.OpenScreen /Script/DowntownUI.DHPlayScreen; wait 3; DH.Key Gamepad_DPad_Up; wait 1; DH.Key Gamepad_DPad_Up; wait 1; DH.Key Gamepad_FaceButton_Bottom; wait 1; DH.Key Gamepad_DPad_Down; wait 1; DH.Key Gamepad_FaceButton_Bottom; wait 120; DH.Shot host"
Start-Process $exe -ArgumentList "`"$repo\DowntownHunting.uproject`" -game -windowed -ResX=960 -ResY=540 -WinX=0 -WinY=0 -DHName=Host `"-ExecCmds=DisableAllScreenMessages, DH.Run $h`" -log=Host.log"
Start-Sleep 75
$c = "wait 30; DH.OpenScreen /Script/DowntownUI.DHJoinScreen; wait 10; DH.Shot join; DH.Key Gamepad_DPad_Down; wait 1; DH.Key Gamepad_FaceButton_Bottom; wait 50; DH.Shot client"
Start-Process $exe -ArgumentList "`"$repo\DowntownHunting.uproject`" -game -windowed -ResX=960 -ResY=540 -WinX=970 -WinY=0 -DHName=Guest `"-ExecCmds=DisableAllScreenMessages, DH.Run $c`" -log=Client.log"
$t = 0
while ($t -lt 200 -and -not ((Test-Path "$shots\host.png") -and (Test-Path "$shots\client.png"))) { Start-Sleep 5; $t += 5 }
Get-Process UnrealEditor -ErrorAction SilentlyContinue | Stop-Process -Force
"t=$t"
