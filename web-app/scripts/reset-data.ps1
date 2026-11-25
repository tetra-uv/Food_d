# Resets the work copy of a scenario: backs it up, then copies the pristine scenario over again.
#   .\scripts\reset-data.ps1 -Scenario lab -Yes
# Stop the gateway first (it keeps the files open through the engine). Without -Yes nothing is changed.
param(
  [string]$Scenario = 'lab',
  [switch]$Yes
)
$ErrorActionPreference = 'Stop'
$web      = Split-Path -Parent $PSScriptRoot
$pristine = Join-Path $web "data\scenarios\$Scenario"
$work     = Join-Path $web "data\work\$Scenario"
$stamp    = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup   = Join-Path $web "data\backups\$Scenario-$stamp"

if (-not (Test-Path -LiteralPath $pristine)) { throw "Scenario '$Scenario' not found: $pristine" }
if (-not $Yes) {
  Write-Host "This would back up   $work"
  Write-Host "to                   $backup"
  Write-Host "and copy             $pristine"
  Write-Host "over the work folder. Nothing was changed. Run again with -Yes to do it (stop the gateway first)."
  exit 1
}
if (Test-Path -LiteralPath $work) {
  Write-Host ">> backup  $work -> $backup"
  New-Item -ItemType Directory -Force -Path (Split-Path -Parent $backup) | Out-Null
  Copy-Item -LiteralPath $work -Destination $backup -Recurse
  Write-Host ">> Remove-Item -Recurse `"$work`""
  Remove-Item -LiteralPath $work -Recurse -Force
}
Write-Host ">> Copy-Item -Recurse `"$pristine`" `"$work`""
New-Item -ItemType Directory -Force -Path (Split-Path -Parent $work) | Out-Null
Copy-Item -LiteralPath $pristine -Destination $work -Recurse
Write-Host "reset done: $work"
