param(
    [string]$BuildDirectory = "build",
    [string]$Destination = "dist/windows"
)
$ErrorActionPreference = "Stop"
$product = Join-Path $BuildDirectory "CrisVoxTuner_artefacts/Release"
$vst = Join-Path $product "VST3/CRIS VOX TUNER.vst3"
$app = Join-Path $product "Standalone/CRIS VOX TUNER.exe"
if (-not (Test-Path $vst -PathType Container)) { throw "VST3 bundle missing: $vst" }
if (-not (Test-Path $app -PathType Leaf)) { throw "Standalone missing: $app" }
$vstDestination = Join-Path $Destination "VST3"
$appDestination = Join-Path $Destination "Standalone"
New-Item -ItemType Directory -Force $vstDestination, $appDestination | Out-Null
Copy-Item -LiteralPath $vst -Destination $vstDestination -Recurse -Force
Copy-Item -LiteralPath $app -Destination $appDestination -Force
Copy-Item README.md, LICENSE, THIRD_PARTY.md -Destination $Destination -Force
Copy-Item docs -Destination $Destination -Recurse -Force
Copy-Item licenses -Destination $Destination -Recurse -Force
$files = Get-ChildItem $Destination -Recurse -File | Where-Object { $_.Extension -eq ".vst3" -or $_.Extension -eq ".exe" }
$lines = foreach ($file in $files) {
    $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLower()
    "$hash  $($file.FullName.Substring((Resolve-Path $Destination).Path.Length + 1))"
}
$lines | Set-Content (Join-Path $Destination "SHA256SUMS.txt")
Write-Host "Packaged Windows VST3 and standalone in $Destination"
