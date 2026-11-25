REM Purpose: Build the console app console program and run it on a fresh copy of the sample data (console-app\data is never changed).
@echo off
cd /d "%~dp0"
if exist console-app\data_check rmdir /s /q console-app\data_check
xcopy /E /I /Y console-app\data console-app\data_check >nul
g++ -std=c++17 -Wall -Wextra -Iconsole-app/include console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp console-app/src/Duration.cpp console-app/src/EntityId.cpp console-app/src/Exceptions.cpp console-app/src/Models.cpp console-app/src/PriorityQueue.cpp console-app/src/RecommendationEngine.cpp console-app/src/allocationmanager.cpp console-app/src/graph.cpp console-app/src/hashindex.cpp console-app/src/intstack.cpp console-app/src/main.cpp console-app/src/traveltime.cpp -o console-app/CommunityBridge.exe
if errorlevel 1 (
    echo Build failed.
    pause
    exit /b 1
)
console-app\CommunityBridge.exe console-app\data_check\
pause
