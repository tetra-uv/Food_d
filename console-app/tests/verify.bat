REM Build CommunityBridge, run all tests, refresh data_check, and launch.
@echo off
setlocal
set "PATH=%~dp0;%PATH%"

set DO_PAUSE=1

if /i "%~1"=="nopause" set DO_PAUSE=0
if /i "%~2"=="nopause" set DO_PAUSE=0

echo ========================================================
echo CommunityBridge - Verification and Launch
echo ========================================================
echo.
echo Building CommunityBridge...
g++ -std=c++17 -Wall -Wextra -I"%~dp0..\include" "%~dp0..\src\ConsoleView.cpp" "%~dp0..\src\Datamanager.cpp" "%~dp0..\src\Duration.cpp" "%~dp0..\src\EntityId.cpp" "%~dp0..\src\Exceptions.cpp" "%~dp0..\src\Models.cpp" "%~dp0..\src\PriorityQueue.cpp" "%~dp0..\src\RecommendationEngine.cpp" "%~dp0..\src\allocationmanager.cpp" "%~dp0..\src\graph.cpp" "%~dp0..\src\hashindex.cpp" "%~dp0..\src\intstack.cpp" "%~dp0..\src\main.cpp" "%~dp0..\src\traveltime.cpp" -o "%~dp0..\CommunityBridge.exe"
if errorlevel 1 goto :build_fail
echo Build succeeded.
echo.

echo ========================================================
echo Running test_all.bat nopause...
echo ========================================================
call test_all.bat nopause
echo.

echo ========================================================
echo Refreshing console-app\data_check from console-app\data...
echo ========================================================
if not exist "%~dp0..\data_check" mkdir "%~dp0..\data_check"
copy /y "%~dp0..\data\*.*" "%~dp0..\data_check\" >nul
echo console-app\data_check refreshed.
echo.

echo ========================================================
echo Launching CommunityBridge.exe against console-app\data_check...
echo ========================================================
"%~dp0..\CommunityBridge.exe" "%~dp0..\data_check"
goto :finish

:build_fail
echo Build failed.

:finish
if "%DO_PAUSE%"=="1" pause

