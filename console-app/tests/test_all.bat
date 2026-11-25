REM Build and run all CommunityBridge console app unit and integration tests.
@echo off
cd /d "%~dp0..\.."
REM The stress test reads a copy of the sample data in console-app\data_check. It is made fresh here, so console-app\data is never touched.
if exist console-app\data_check rmdir /s /q console-app\data_check
xcopy /E /I /Y console-app\data console-app\data_check >nul
setlocal

set PAUSE_EACH=0

if not exist "%TEMP%\cb_tests" mkdir "%TEMP%\cb_tests"

set /a PASS_COUNT=0
set /a FAIL_COUNT=0
set /a BUILD_FAIL_COUNT=0
set /a BLOCKED_COUNT=0
set /a NOT_PRESENT_COUNT=0

echo ========================================================
echo CommunityBridge - console app Test Runner
echo ========================================================
echo.
echo ========================================================
echo MODULE STATUS
echo ========================================================
if exist console-app\src\Exceptions.cpp echo [  OK   ] Exceptions
if not exist console-app\src\Exceptions.cpp echo [PENDING] Exceptions
if exist console-app\src\Duration.cpp echo [  OK   ] Duration
if not exist console-app\src\Duration.cpp echo [PENDING] Duration
if exist console-app\src\EntityId.cpp echo [  OK   ] EntityId
if not exist console-app\src\EntityId.cpp echo [PENDING] EntityId
if exist console-app\src\Models.cpp echo [  OK   ] Models
if not exist console-app\src\Models.cpp echo [PENDING] Models
if exist console-app\src\Datamanager.cpp echo [  OK   ] DataManager
if not exist console-app\src\Datamanager.cpp echo [PENDING] DataManager
if exist console-app\src\main.cpp echo [  OK   ] main
if not exist console-app\src\main.cpp echo [PENDING] main
if exist console-app\src\PriorityQueue.cpp echo [  OK   ] PriorityQueue
if not exist console-app\src\PriorityQueue.cpp echo [PENDING] PriorityQueue
if exist console-app\src\intstack.cpp echo [  OK   ] IntStack
if not exist console-app\src\intstack.cpp echo [PENDING] IntStack
if exist console-app\src\graph.cpp echo [  OK   ] Graph
if not exist console-app\src\graph.cpp echo [PENDING] Graph
if exist console-app\src\traveltime.cpp echo [  OK   ] TravelTime
if not exist console-app\src\traveltime.cpp echo [PENDING] TravelTime
if exist console-app\include\Sorting.h echo [  OK   ] Sorting
if not exist console-app\include\Sorting.h echo [PENDING] Sorting
if exist console-app\src\RecommendationEngine.cpp echo [  OK   ] RecommendationEngine
if not exist console-app\src\RecommendationEngine.cpp echo [PENDING] RecommendationEngine
if exist console-app\src\ConsoleView.cpp echo [  OK   ] ConsoleView
if not exist console-app\src\ConsoleView.cpp echo [PENDING] ConsoleView
if exist console-app\src\allocationmanager.cpp echo [  OK   ] AllocationManager
if not exist console-app\src\allocationmanager.cpp echo [PENDING] AllocationManager
if exist console-app\src\hashindex.cpp echo [  OK   ] HashIndex
if not exist console-app\src\hashindex.cpp echo [PENDING] HashIndex
if exist console-app\src\intqueue.cpp echo [  OK   ] IntQueue
if not exist console-app\src\intqueue.cpp echo [PENDING] IntQueue
echo.

REM --- Test Suites ---

set NAME=test_duration
set COVERS=DU1-DU6
set SRC=console-app/src/Duration.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_entityid
set COVERS=ID1-ID7
set SRC=console-app/src/EntityId.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_exceptions
set COVERS=EX1-EX2
set SRC=console-app/src/Exceptions.cpp
call :runtest

set NAME=test_models
set COVERS=MO1-MO12
set SRC=console-app/src/Models.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_recommendationengine
set COVERS=F1-F8, R1-R6, SO3, candidate and rejection stream operators
set SRC=console-app/src/RecommendationEngine.cpp console-app/src/traveltime.cpp
set SRC=%SRC% console-app/src/graph.cpp console-app/src/PriorityQueue.cpp
set SRC=%SRC% console-app/src/intstack.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_sorting
set COVERS=SO1-SO3, R6
set SRC=console-app/src/RecommendationEngine.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_dijkstra
set COVERS=G1-G8
set SRC=console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp console-app/src/intstack.cpp
call :runtest

set NAME=test_graph
set COVERS=GR1-GR3, CN1
set SRC=console-app/src/graph.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_traveltime
set COVERS=TT1-TT4
set SRC=console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_priorityqueue
set COVERS=H1-H6
set SRC=console-app/src/PriorityQueue.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_intstack
set COVERS=ST1-ST3
set SRC=console-app/src/intstack.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_allocationmanager
set COVERS=A1-A9
set SRC=console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_datamanager
set COVERS=V1-V8, I1-I7, P1-P8
set SRC=console-app/src/Datamanager.cpp console-app/src/allocationmanager.cpp
set SRC=%SRC% console-app/src/RecommendationEngine.cpp console-app/src/traveltime.cpp
set SRC=%SRC% console-app/src/graph.cpp console-app/src/hashindex.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/Models.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_hashindex
set COVERS=HT1-HT6
set SRC=console-app/src/hashindex.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_intqueue
set COVERS=QU1-QU4
set SRC=console-app/src/intqueue.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_urgencyqueue
set COVERS=Q1-Q3
set SRC=console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_consoleview
set COVERS=S1-S8, CoordinatorChoice parsing, navigation menu, interactive donation addition
set SRC=console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp
set SRC=%SRC% console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/intqueue.cpp
set SRC=%SRC% console-app/src/intstack.cpp console-app/src/PriorityQueue.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp console-app/src/Models.cpp
call :runtest

set NAME=test_evaluation
set COVERS=E1-E3, SC2
set SRC=console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp
set SRC=%SRC% console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/intqueue.cpp
set SRC=%SRC% console-app/src/intstack.cpp console-app/src/PriorityQueue.cpp
set SRC=%SRC% console-app/src/Duration.cpp console-app/src/EntityId.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp console-app/src/Models.cpp
call :runtest

set NAME=test_datamanager_load
set COVERS=CSV file loading edge cases, corrupt headers, missing foreign keys, invalid enums, and duplicate IDs
set SRC=console-app/src/Datamanager.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_datamanager_invariants
set COVERS=Dataset invariant edge cases (I1-I7), transaction reconciliation, and capacity integrity
set SRC=console-app/src/Datamanager.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_recommendationengine_ranking
set COVERS=4-tier candidate ranking cascade (acceptance, travel time, unused capacity, EntityId)
set SRC=console-app/src/RecommendationEngine.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_allocationmanager_errors
set COVERS=Allocation error conditions (DONATION_NOT_OPEN, BAD_QUANTITY, EXCEEDS_REMAINING, EXCEEDS_CAPACITY, NOT_FEASIBLE) and state rollback
set SRC=console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_recommendationengine_boundary
set COVERS=Boundary travel feasibility (exact equality, off-by-one, zero time, INF unreachable)
set SRC=console-app/src/RecommendationEngine.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_stress_dataset
set COVERS=Stress dataset verification (12 donors, 26 recipients, 46 donations, 20 map locations, 26 roads) across all 5 deliberate abnormalities
set SRC=console-app/src/Datamanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Models.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/Exceptions.cpp
call :runtest

set NAME=test_consoleview_tables
set COVERS=Recipients and transactions tables: column width follows the longest cell, no columns touch
set SRC=console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp
set SRC=%SRC% console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Exceptions.cpp
set SRC=%SRC% console-app/src/Models.cpp
call :runtest

set NAME=test_datamanager_add
set COVERS=appenddonor, appendrecipient (round trip through the files, comma refusal), donor and recipient id issuers
set SRC=console-app/src/Datamanager.cpp console-app/src/allocationmanager.cpp
set SRC=%SRC% console-app/src/RecommendationEngine.cpp console-app/src/traveltime.cpp
set SRC=%SRC% console-app/src/graph.cpp console-app/src/hashindex.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/Models.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Exceptions.cpp
call :runtest

set NAME=test_consoleview_add
set COVERS=Add a donation (existing and new donor) and add a recipient: cancel burns no id, bad answers re-ask, saved and reloaded, donor written before donation
set SRC=console-app/src/ConsoleView.cpp console-app/src/Datamanager.cpp
set SRC=%SRC% console-app/src/allocationmanager.cpp console-app/src/RecommendationEngine.cpp
set SRC=%SRC% console-app/src/traveltime.cpp console-app/src/graph.cpp
set SRC=%SRC% console-app/src/hashindex.cpp console-app/src/intstack.cpp
set SRC=%SRC% console-app/src/PriorityQueue.cpp console-app/src/Duration.cpp
set SRC=%SRC% console-app/src/EntityId.cpp console-app/src/Exceptions.cpp
set SRC=%SRC% console-app/src/Models.cpp
call :runtest

REM --- Summary ---
echo ========================================================
echo SUMMARY
echo ========================================================
echo Passed:            %PASS_COUNT%
echo Failed:            %FAIL_COUNT%
echo Build failed:      %BUILD_FAIL_COUNT%
echo Blocked:           %BLOCKED_COUNT%
echo Not present yet:   %NOT_PRESENT_COUNT%
echo.

set /a TOTAL_ISSUES=%FAIL_COUNT% + %BUILD_FAIL_COUNT% + %BLOCKED_COUNT%
if %TOTAL_ISSUES%==0 goto :summary_clean
echo console app prototype demo completed with some modules pending or requiring fixes.
goto :summary_done

:summary_clean
echo All present console app tests built and passed successfully.

:summary_done
echo ========================================================
echo.
if /i not "%~1"=="nopause" pause
goto :eof

REM ========================================================
REM Subroutine: runtest
REM ========================================================
:runtest
echo ========================================================
echo Test: %NAME%
echo Covers: %COVERS%

if not exist console-app\tests\%NAME%.cpp goto :test_missing

set MISSING_SRC=
call :check_src %SRC%
if not "%MISSING_SRC%"=="" goto :test_blocked

set GXX_CMD=g++ -std=c++17 -Wall -Wextra -Iconsole-app/include
set GXX_CMD=%GXX_CMD% console-app/tests/%NAME%.cpp %SRC%
set GXX_CMD=%GXX_CMD% -o "%TEMP%\cb_tests\%NAME%.exe"
echo %GXX_CMD%

%GXX_CMD% > "%TEMP%\cb_tests\%NAME%_build.log" 2>&1
set BUILD_ERR=%errorlevel%
if not %BUILD_ERR%==0 goto :test_build_fail

"%TEMP%\cb_tests\%NAME%.exe" > "%TEMP%\cb_tests\%NAME%_run.log" 2>&1
set RUN_EXIT=%errorlevel%
type "%TEMP%\cb_tests\%NAME%_run.log"

if not %RUN_EXIT%==0 goto :test_fail
findstr /b /c:"FAIL" "%TEMP%\cb_tests\%NAME%_run.log" > nul
if %errorlevel%==0 goto :test_fail

echo RESULT: PASS
set /a PASS_COUNT+=1
goto :test_finish

:test_missing
echo RESULT: NOT PRESENT YET
set /a NOT_PRESENT_COUNT+=1
goto :test_finish

:test_blocked
echo RESULT: BLOCKED - missing %MISSING_SRC%
set /a BLOCKED_COUNT+=1
goto :test_finish

:test_build_fail
type "%TEMP%\cb_tests\%NAME%_build.log"
echo RESULT: BUILD FAILED
set /a BUILD_FAIL_COUNT+=1
goto :test_finish

:test_fail
echo RESULT: FAIL - exit code %RUN_EXIT%
set /a FAIL_COUNT+=1
goto :test_finish

:test_finish
echo.
if "%PAUSE_EACH%"=="1" pause
goto :eof

REM ========================================================
REM Subroutine: check_src
REM ========================================================
:check_src
if "%~1"=="" goto :eof
if not exist "%~1" set MISSING_SRC=%~1& goto :eof
shift
goto :check_src
