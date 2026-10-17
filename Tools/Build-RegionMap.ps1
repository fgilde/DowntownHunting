# Builds /Game/DH/Maps/<Name> from a City Sample region selected with Tools/Python/cs_region.py (Tools/Maps/<Name>.json).
# Editor must be closed. Gameplay actors (starts, walls, control points) and the navmesh come afterwards.
#
#   .\Tools\Import-CitySample.ps1 -Manifest Tools\CitySample_DH_Park.txt
#   .\Tools\Build-RegionMap.ps1 DH_Park
param([Parameter(Mandatory)][string]$Name)

$repo = Split-Path $PSScriptRoot -Parent
$ue = "C:\Program Files\Epic Games\UE_5.8\Engine\Binaries\Win64\UnrealEditor-Cmd.exe"
$map = Join-Path $repo "Content\DH\Maps\$Name.umap"
if (Test-Path $map) { Remove-Item $map; Remove-Item -Recurse -Force (Join-Path $repo "Content\__ExternalActors__\DH\Maps\$Name") -ErrorAction SilentlyContinue }
foreach ($script in "save_region_map.py", "prune_region_map.py", "flatten_dh_block.py") {
    & $ue "$repo\DowntownHunting.uproject" -run=pythonscript -script="$repo\Tools\Python\$script $Name" -nullrhi -nosound -unattended -nopause | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "$script failed ($LASTEXITCODE), see Saved\Logs\DowntownHunting.log" }
}
Select-String -Path "$repo\Saved\Logs\DowntownHunting.log" -Pattern "PRUNE|DH_FLATTEN" | ForEach-Object { $_.Line -replace '^.*LogPython: ', '' }
