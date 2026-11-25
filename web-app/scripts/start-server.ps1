# Starts the gateway (and through it cb_engine.exe) on a WORK COPY of a scenario.
#   .\scripts\start-server.ps1                 # scenario "lab", http://127.0.0.1:8787
#   .\scripts\start-server.ps1 -Scenario lab -Port 8790
# The engine writes the CSV files in place, so it never runs on data\scenarios\* (those stay pristine):
# data\scenarios\<name>\ is copied (all files, geo_overlay.json included) to data\work\<name>\ only when the work
# folder does not exist yet; an existing work folder is never overwritten (use scripts\reset-data.ps1 -Yes for that).
param(
  [string]$Scenario = 'lab',
  [int]$Port = 8787,
  [string]$Bind = '127.0.0.1'
)
$ErrorActionPreference = 'Stop'
$web      = Split-Path -Parent $PSScriptRoot
$server   = Join-Path $web 'server'
$pristine = Join-Path $web "data\scenarios\$Scenario"
$work     = Join-Path $web "data\work\$Scenario"
$exe      = Join-Path $web 'engine\build\cb_engine.exe'

if (-not (Test-Path -LiteralPath $pristine)) { throw "Scenario '$Scenario' not found: $pristine" }

if (-not (Test-Path -LiteralPath $exe)) {
  Write-Host ">> scripts\build-engine.ps1 (engine not built yet)"
  & (Join-Path $PSScriptRoot 'build-engine.ps1')
}

if (-not (Test-Path -LiteralPath (Join-Path $server 'node_modules'))) {
  Write-Host ">> npm install   (in $server)"
  Push-Location $server
  try { & npm install --no-audit --no-fund | Out-Host; if ($LASTEXITCODE -ne 0) { throw 'npm install failed' } } finally { Pop-Location }
}

if (-not (Test-Path -LiteralPath $work)) {
  Write-Host ">> Copy-Item -Recurse `"$pristine`" `"$work`"   (first start: work copy, never overwritten later)"
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $work) | Out-Null
  Copy-Item -LiteralPath $pristine -Destination $work -Recurse
} else {
  Write-Host "work copy exists, kept as it is: $work"
}

$env:CB_DATA   = $work
$env:CB_PORT   = "$Port"
$env:CB_BIND   = $Bind
$env:CB_ENGINE = $exe
Write-Host ">> CB_DATA=$work  CB_PORT=$Port  CB_BIND=$Bind  CB_ENGINE=$exe"
Write-Host ">> node src\index.js   (Ctrl+C stops the gateway and the engine)"
Push-Location $server
try { & node src\index.js } finally { Pop-Location }
