# Configures and builds web-app\engine\build\cb_engine.exe (out of tree; the backend is compiled from ..\console-app and ..\desktop-app as-is).
$ErrorActionPreference = 'Stop'
$web    = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $web 'engine'
$build  = Join-Path $engine 'build'

Write-Host ">> powershell engine\tools\gen_manifest.ps1"
& (Join-Path $engine 'tools\gen_manifest.ps1')

Write-Host ">> cmake -S `"$engine`" -B `"$build`" -G Ninja -DCMAKE_BUILD_TYPE=Release"
& cmake -S $engine -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_COMPILER=g++ | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }

Write-Host ">> cmake --build `"$build`""
& cmake --build $build | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }

$exe = Join-Path $build 'cb_engine.exe'
if (-not (Test-Path -LiteralPath $exe)) { throw "cb_engine.exe was not produced" }
Write-Host "engine built: $exe"
