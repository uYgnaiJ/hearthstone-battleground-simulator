$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $vendor = Join-Path $root 'native/vendor'
    New-Item -ItemType Directory -Force -Path $vendor | Out-Null
    function Download([string]$url, [string]$file, [string]$hash) {
        if (-not (Test-Path -LiteralPath $file)) {
            & curl.exe -sSL --fail --retry 3 --max-time 600 $url -o $file
            if ($LASTEXITCODE -ne 0) { throw "Download failed: $url" }
        }
        if ((Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash -ne $hash) { throw "Checksum mismatch: $file" }
    }
    if (-not (Test-Path -LiteralPath "$vendor/raylib-6.0_win64_mingw-w64/include/raylib.h")) {
        Download 'https://github.com/raysan5/raylib/releases/download/6.0/raylib-6.0_win64_mingw-w64.zip' "$vendor/raylib.zip" '69688e025812c8132634c609c30938eda4a3fa14d63c4031108873a4d797e2d3'
        & tar.exe -xf "$vendor/raylib.zip" -C $vendor
        if ($LASTEXITCODE -ne 0) { throw 'Raylib extraction failed.' }
    }
    Download 'https://cdn.jsdelivr.net/gh/nlohmann/json@v3.12.0/single_include/nlohmann/json.hpp' "$vendor/json.hpp" 'aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63'
    if (-not (Test-Path -LiteralPath "$vendor/w64devkit/bin/g++.exe")) {
        Download 'https://github.com/skeeto/w64devkit/releases/download/v2.10.0/w64devkit-x64-2.10.0.7z.exe' "$vendor/compiler.7z.exe" '18d0a4c71a166f8401ab6305781bec5882b40b5e06ba9807c61cb5f3b3c6325e'
        $process = Start-Process -FilePath "$vendor/compiler.7z.exe" -ArgumentList '-y', '-o./native/vendor' -WindowStyle Hidden -Wait -PassThru
        if ($process.ExitCode -ne 0) { throw 'Compiler extraction failed.' }
    }
    Write-Host 'Native build dependencies are ready.'
} finally { Pop-Location }
