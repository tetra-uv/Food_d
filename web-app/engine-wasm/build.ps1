# Builds web-app\engine-wasm\dist\cb_engine.mjs + cb_engine.wasm with the Emscripten SDK (default folder %USERPROFILE%\tools\emsdk, or the EMSDK variable)
# (override with -Emsdk). Usage: powershell -File web-app\engine-wasm\build.ps1 [-Opt -O2|-Os] [-Clean]
param(
  [string]$Emsdk = $(if ($env:EMSDK) { $env:EMSDK } else { Join-Path $env:USERPROFILE 'tools\emsdk' }),
  [ValidateSet('-O2', '-Os')][string]$Opt = '-O2',
  [switch]$Clean
)
$ErrorActionPreference = 'Continue'   # native tools write progress to stderr; exit codes are checked instead
$env:EMSDK_QUIET = '1'
$here  = $PSScriptRoot
$build = Join-Path $here 'build'
$dist  = Join-Path $here 'dist'

if (-not (Test-Path -LiteralPath (Join-Path $Emsdk 'emsdk_env.ps1'))) { throw "emsdk not found at $Emsdk (see README.md, Build)" }
Write-Host ">> activating emsdk env ($Emsdk)"
& (Join-Path $Emsdk 'emsdk_env.ps1') *> $null
if (-not (Get-Command emcc -ErrorAction SilentlyContinue)) { throw 'emcc is not on PATH after emsdk_env.ps1 (run: emsdk activate latest)' }
& emcc --version | Select-Object -First 1 | Out-Host

if ($Clean -and (Test-Path -LiteralPath $build)) { Remove-Item -Recurse -Force -LiteralPath $build }
New-Item -ItemType Directory -Force $dist | Out-Null

Write-Host ">> emcmake cmake (Ninja, $Opt)"
& emcmake cmake -S $here -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCB_OPT=$Opt" | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake configure failed' }
Write-Host '>> cmake --build'
& cmake --build $build | Out-Host
if ($LASTEXITCODE -ne 0) { throw 'cmake build failed' }

function Get-GzipSize([string]$path) {
  $bytes = [System.IO.File]::ReadAllBytes($path)
  $ms = New-Object System.IO.MemoryStream
  $gz = New-Object System.IO.Compression.GZipStream($ms, [System.IO.Compression.CompressionLevel]::Optimal)
  $gz.Write($bytes, 0, $bytes.Length); $gz.Close()
  return $ms.ToArray().Length
}
foreach ($f in 'cb_engine.wasm', 'cb_engine.mjs') {
  $p = Join-Path $dist $f
  if (-not (Test-Path -LiteralPath $p)) { throw "$f was not produced" }
  $raw = (Get-Item -LiteralPath $p).Length
  Write-Host ("{0,-16} raw {1,9} B   gzip {2,9} B" -f $f, $raw, (Get-GzipSize $p))
}
Write-Host "wasm engine built: $dist"


