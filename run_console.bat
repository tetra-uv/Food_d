REM Purpose: Build and run the console app console program on console-app\data (every change is saved there).
@echo off
cd /d "%~dp0"
setlocal

set DATA_DIR=console-app\data\
set DO_PAUSE=1

if /i "%~1"=="nopause" set DO_PAUSE=0
if /i "%~2"=="nopause" set DO_PAUSE=0

if not "%~1"=="" if /i not "%~1"=="nopause" set DATA_DIR=%~1
if not "%~2"=="" if /i not "%~2"=="nopause" set DATA_DIR=%~2

echo Building CommunityBridge...
g++ -std=c++17 -Wall -Wextra -Iconsole-app/include console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp console-app/src/Duration.cpp console-app/src/EntityId.cpp console-app/src/Exceptions.cpp console-app/src/Models.cpp console-app/src/PriorityQueue.cpp console-app/src/RecommendationEngine.cpp console-app/src/allocationmanager.cpp console-app/src/graph.cpp console-app/src/hashindex.cpp console-app/src/intstack.cpp console-app/src/main.cpp console-app/src/traveltime.cpp -o console-app/CommunityBridge.exe
if errorlevel 1 goto :build_fail

echo Running CommunityBridge...
console-app\CommunityBridge.exe %DATA_DIR%
goto :finish

:build_fail
echo Build failed.

:finish
if "%DO_PAUSE%"=="1" pause
