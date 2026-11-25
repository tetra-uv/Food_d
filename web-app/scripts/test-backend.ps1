# Runs the engine tests (python) and the gateway tests (node:test, port 8788) on temp copies; the pristine data is never written.
$ErrorActionPreference = 'Stop'
$web    = Split-Path -Parent $PSScriptRoot
$server = Join-Path $web 'server'
$exe    = Join-Path $web 'engine\build\cb_engine.exe'
$failed = 0

if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host ">> scripts\build-engine.ps1"
  & (Join-Path $PSScriptRoot 'build-engine.ps1')
}

Write-Host ">> python -I engine\tests\test_engine.py"
& python -I (Join-Path $web 'engine\tests\test_engine.py') | Out-Host
if ($LASTEXITCODE -ne 0) { $failed++ }

if (-not (Test-Path -LiteralPath (Join-Path $server 'node_modules'))) {
  Write-Host ">> npm install   (in $server)"
  Push-Location $server; try { & npm install --no-audit --no-fund | Out-Host } finally { Pop-Location }
}

Write-Host ">> npm test   (in $server)"
Push-Location $server
try { & npm test | Out-Host; if ($LASTEXITCODE -ne 0) { $failed++ } } finally { Pop-Location }

if ($failed -gt 0) { Write-Host "BACKEND TESTS: $failed suite(s) FAILED"; exit 1 }
Write-Host 'BACKEND TESTS: all suites passed'
