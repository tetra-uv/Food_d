REM Purpose: Build the Release version of CommunityBridge Pro (communitybridge_pro) and put it, with its sample data, in
REM desktop-app\dist_pro\CommunityBridge. The old program, desktop-app\dist and desktop-app\stable are not touched.
@echo off
setlocal

REM The release build of the pro program has its own folder (desktop-app\build-dist-pro) and uses CMake and Ninja.
echo Preparing the release build of CommunityBridge Pro ...
cmake -G Ninja -S "%~dp0." -B "%~dp0build-dist-pro" -DCMAKE_BUILD_TYPE=Release
if errorlevel 1 goto :fail

echo Building ...
cmake --build "%~dp0build-dist-pro" --target communitybridge_pro
if errorlevel 1 goto :fail

REM Only the pro part is installed. The data folder is only made when it is not there yet, so making the folder again never
REM overwrites the changes a person made in the program; sample_data always holds the original files.
echo Putting the program in desktop-app\dist_pro\CommunityBridge ...
cmake --install "%~dp0build-dist-pro" --component pro --prefix "%~dp0dist_pro\CommunityBridge"
if errorlevel 1 goto :fail

echo.
echo Done. Start desktop-app\run_pro.bat, or run create_pro_shortcut.bat for a Desktop icon.
exit /b 0

:fail
echo The build of CommunityBridge Pro failed. Nothing was changed in desktop-app\dist_pro.
exit /b 1
