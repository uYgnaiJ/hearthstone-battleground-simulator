param([switch]$Tests)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
Push-Location $root
try {
    $output = Join-Path $root 'release/native'
    $vendor = Join-Path $root 'native/vendor/raylib-6.0_win64_mingw-w64'
    $compiler = if ($env:CXX) { $env:CXX } else { Join-Path $root 'native/vendor/w64devkit/bin/g++.exe' }
    if (-not (Test-Path -LiteralPath $compiler)) { throw 'Compiler missing. Run tools/setup.ps1 first, or set CXX.' }
    New-Item -ItemType Directory -Force -Path "$output/data", "$output/licenses", "$root/test-results" | Out-Null
    foreach ($name in 'cards.json','heroes.json','token-art.json','manifest.json') {
        Copy-Item -LiteralPath "src/data/$name" -Destination "$output/data/$name" -Force
    }
    Copy-Item -LiteralPath 'native/assets' -Destination $output -Recurse -Force
    Copy-Item -LiteralPath 'public/art' -Destination $output -Recurse -Force
    Copy-Item -LiteralPath "$vendor/LICENSE" -Destination "$output/licenses/raylib.txt" -Force
    Copy-Item -LiteralPath 'native/vendor/json.hpp' -Destination "$output/licenses/nlohmann-json.hpp" -Force
    Copy-Item -LiteralPath 'native/vendor/w64devkit/COPYING.MinGW-w64-runtime.txt' -Destination "$output/licenses/MinGW-runtime.txt" -Force
    Copy-Item -LiteralPath 'native/ASSETS.md' -Destination "$output/licenses/ASSETS.md" -Force
    Copy-Item -LiteralPath 'README.md' -Destination "$output/README.md" -Force
    & $compiler '-std=c++20' '-O2' '-static' "-I$vendor/include" 'native/main.cpp' "$vendor/lib/libraylib.a" '-lopengl32' '-lgdi32' '-lwinmm' '-o' "$output/Battlegrounds.exe" '-mwindows'
    if ($LASTEXITCODE -ne 0) { throw 'Game compilation failed.' }
    if ($Tests) {
        & $compiler '-std=c++20' '-O2' '-static' 'native/tests.cpp' '-o' 'test-results/native-tests.exe'
        if ($LASTEXITCODE -ne 0) { throw 'Test compilation failed.' }
    }
    # Remove obsolete runtime files from older builds, only within this output folder.
    foreach ($relative in 'runtime','rules.cjs','rules-errors.log','licenses/Node.txt') {
        $target = [IO.Path]::GetFullPath((Join-Path $output $relative))
        if (-not $target.StartsWith($output + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) { throw 'Invalid cleanup target.' }
        if (Test-Path -LiteralPath $target) { Remove-Item -LiteralPath $target -Recurse -Force }
    }
    Write-Host "Built $output/Battlegrounds.exe"
} finally { Pop-Location }
