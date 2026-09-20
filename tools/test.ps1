param([switch]$SkipBuild, [switch]$Smoke, [int]$Soak = 0)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    if (-not $SkipBuild) { & "$PSScriptRoot/build.ps1" -Tests }
    if ($Soak -gt 0) { & "$root/test-results/native-tests.exe" --soak $Soak }
    else { & "$root/test-results/native-tests.exe" }
    if ($LASTEXITCODE -ne 0) { throw 'Native tests failed.' }
    if ($Smoke) { & "$PSScriptRoot/run.ps1" -Smoke }
} finally { Pop-Location }
