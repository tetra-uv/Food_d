REM Purpose: Build CommunityBridge Pro, update the program in desktop-app\dist_pro\CommunityBridge and start it on the data folder
REM next to it (dist_pro\CommunityBridge\data). Extra words go on to the program. A failed build starts nothing.
@echo off
setlocal
set "EXE=%~dp0dist_pro\CommunityBridge\communitybridge_pro.exe"

call "%~dp0make_dist_pro.bat"
if errorlevel 1 goto :build_fail
if not exist "%EXE%" goto :build_fail

REM Only the program that was just built is started, and the console says which file it is.
for %%F in ("%EXE%") do echo Starting %%~fF built %%~tF
start "" "%EXE%" %*
exit /b 0

:build_fail
echo The build failed. Nothing was started.
exit /b 1
