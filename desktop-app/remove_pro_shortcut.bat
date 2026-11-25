REM Purpose: Remove the CommunityBridge Pro icon from the Desktop (only that one shortcut file; the program stays in dist_pro).
@echo off
setlocal

REM The shortcut is looked for on the Desktop of this person, unless CB_SHORTCUT_DIR names another folder (used for tests).
powershell -NoProfile -ExecutionPolicy Bypass -Command "$dir = $env:CB_SHORTCUT_DIR; if (-not $dir) { $dir = [Environment]::GetFolderPath('Desktop') }; $link = Join-Path $dir 'CommunityBridge Pro.lnk'; if (Test-Path -LiteralPath $link) { Remove-Item -LiteralPath $link; Write-Host ('Shortcut removed: ' + $link) } else { Write-Host 'There was no CommunityBridge Pro shortcut.' }"
exit /b 0
