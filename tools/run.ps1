param([switch]$Smoke)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$executable = Join-Path $root 'release/native/Battlegrounds.exe'
if (-not (Test-Path -LiteralPath $executable)) { throw 'Build the app first: tools/build.ps1' }
$priorData = $env:BOBS_DATA_DIR
try {
    if ($Smoke) {
        $env:BOBS_DATA_DIR = Join-Path $root ('test-results/native-smoke-' + [Guid]::NewGuid().ToString('N'))
        $result = Join-Path $root 'release/native/smoke-result.json'
        if (Test-Path -LiteralPath $result) { Remove-Item -LiteralPath $result }
        $process = Start-Process -FilePath $executable -ArgumentList '--smoke' -WorkingDirectory $root -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw "Native smoke exited $($process.ExitCode)" }
        $report = Get-Content -LiteralPath $result -Raw | ConvertFrom-Json
        $report | ConvertTo-Json -Depth 8
        if (-not $report.ok) { throw 'Native smoke failed.' }
    } else {
        Start-Process -FilePath $executable -WorkingDirectory $root -WindowStyle Hidden
    }
} finally { $env:BOBS_DATA_DIR = $priorData }
