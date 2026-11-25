REM Purpose: Put a CommunityBridge Pro icon on the Desktop. It starts desktop-app\dist_pro\CommunityBridge\communitybridge_pro.exe.
@echo off
setlocal

set "CB_TARGET=%~dp0dist_pro\CommunityBridge\communitybridge_pro.exe"
set "CB_WORKDIR=%~dp0dist_pro\CommunityBridge"

if not exist "%CB_TARGET%" (
    echo The pro program is not in the dist_pro folder yet. Run make_dist_pro.bat first.
    exit /b 1
)

REM The shortcut goes to the Desktop of this person, unless the variable CB_SHORTCUT_DIR names another folder (used for tests).
powershell -NoProfile -ExecutionPolicy Bypass -Command "$dir = $env:CB_SHORTCUT_DIR; if (-not $dir) { $dir = [Environment]::GetFolderPath('Desktop') }; if (-not (Test-Path -LiteralPath $dir)) { New-Item -ItemType Directory -Path $dir -Force | Out-Null }; $link = Join-Path $dir 'CommunityBridge Pro.lnk'; $shell = New-Object -ComObject WScript.Shell; $s = $shell.CreateShortcut($link); $s.TargetPath = $env:CB_TARGET; $s.WorkingDirectory = $env:CB_WORKDIR; $s.IconLocation = $env:CB_TARGET + ',0'; $s.Description = 'CommunityBridge Pro'; $s.Save(); Write-Host ('Shortcut made: ' + $link)"
if errorlevel 1 (
    echo The shortcut could not be made.
    exit /b 1
)
exit /b 0
