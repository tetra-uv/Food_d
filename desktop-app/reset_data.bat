REM Purpose: Put the sample data back into desktop-app\data (removes offers.csv and every change made in the program).
@echo off
setlocal

REM The six sample files come from console-app\data. That folder is only read, never written.
if not exist "%~dp0..\console-app\data\donors.csv" (
    echo Cannot find ..\console-app\data. Nothing was changed.
    goto :finish
)

if not exist "%~dp0data" mkdir "%~dp0data"

echo Removing the files in desktop-app\data ...
del /q "%~dp0data\*.*"

echo Copying the sample data from console-app\data ...
copy /y "%~dp0..\console-app\data\donors.csv" "%~dp0data\" > nul
copy /y "%~dp0..\console-app\data\recipients.csv" "%~dp0data\" > nul
copy /y "%~dp0..\console-app\data\donations.csv" "%~dp0data\" > nul
copy /y "%~dp0..\console-app\data\transactions.csv" "%~dp0data\" > nul
copy /y "%~dp0..\console-app\data\graph_nodes.csv" "%~dp0data\" > nul
copy /y "%~dp0..\console-app\data\graph_edges.csv" "%~dp0data\" > nul

echo Done. desktop-app\data holds the six sample files and no offers.

:finish
pause
