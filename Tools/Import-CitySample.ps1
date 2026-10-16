# Copies the City Sample packages listed in a manifest into this project's Content folder.
# City Sample content is not in git (several GB of 8K textures); every machine imports it from a local City Sample copy.
#
#   .\Tools\Import-CitySample.ps1                               # default manifest and City Sample path
#   .\Tools\Import-CitySample.ps1 -CitySample "D:\Projects\CitySample"
param(
    [string]$CitySample = "$env:USERPROFILE\Documents\Unreal Projects\CitySample",
    [string]$Manifest = "$PSScriptRoot\CitySample_DH_Block.txt"
)

$src = Join-Path $CitySample "Content"
$dst = Join-Path (Split-Path $PSScriptRoot -Parent) "Content"
if (-not (Test-Path $src)) { throw "City Sample content not found: $src" }

$copied = 0; $missing = @()
foreach ($pkg in Get-Content $Manifest) {
    if (-not $pkg -or $pkg.StartsWith("#")) { continue }
    $rel = $pkg.Substring("/Game/".Length) -replace "/", "\"
    $found = $false
    foreach ($ext in ".uasset", ".umap", ".ubulk", ".uptnl", ".uexp") {
        $from = Join-Path $src ($rel + $ext)
        if (Test-Path -LiteralPath $from) {
            $to = Join-Path $dst ($rel + $ext)
            New-Item -ItemType Directory -Force (Split-Path $to -Parent) | Out-Null
            if (-not (Test-Path -LiteralPath $to)) { Copy-Item -LiteralPath $from $to; $copied++ }
            $found = $true
        }
    }
    if (-not $found) { $missing += $pkg }
}
"copied $copied files, $($missing.Count) packages missing"
$missing | Select-Object -First 20
