@echo off

set HELP_ROOT=%~dp0%

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo Target directory does not exist: %OPENSAL_BUILD_DIR%salamander
  echo.
  pause
  exit /b
)

echo Compiling help for Salamander...
pushd %HELP_ROOT%
call compile.bat %HELP_ROOT%copy_to_salbin.bat >%HELP_ROOT%result.log
popd

set PLUGIN_LIST=(zip filecomp demoplug demoview demomenu 7zip dbviewer diskmap^
 ftp checksum checkver ieviewer mmviewer pak\spl peviewer pictview regedt renamer splitcbn tar unarj^
 uncab unchm undelete unfat uniso unlha unmime unrar wmobile winscp nethood automation)

for %%i in %PLUGIN_LIST% do (
  echo Compiling help for %%i...
  pushd %HELP_ROOT%..\..\src\plugins\%%i\help
  call compile.bat %HELP_ROOT%copy_to_salbin.bat >>%HELP_ROOT%result.log
  popd
)

echo.
echo Go to INDEX tab in opened HTML Help to create salamand.chw file.
echo.

for %%t in (Debug_x86 Release_x86 Debug_x64 Release_x64) do (
  call :my_del "%OPENSAL_BUILD_DIR%salamander\%%t\help\english\salamand.chw"
)

call "%OPENSAL_BUILD_DIR%salamander\Debug_x86\help\english\salamand.chm"

for %%t in (          Release_x86 Debug_x64 Release_x64) do (
  call :mycopy_bat "%OPENSAL_BUILD_DIR%salamander\Debug_x86\help\english\salamand.chw" "%OPENSAL_BUILD_DIR%salamander\%%t\help\english\salamand.chw"
)

echo.
echo Searching for WARNINGS:
find /I "warn" %HELP_ROOT%result.log

echo.
echo Searching for ERRORS:
find /I "err" %HELP_ROOT%result.log

echo.
echo Check RESULT.LOG file for results! Search for errors/warnings.
echo Press any key to delete RESULT.LOG file.
echo.
pause

del %HELP_ROOT%result.log

exit /b


rem ---------------------------- Delete File rutina

:my_del
if exist %1 del %1
exit /b

rem ---------------------------- Copy rutina

:mycopy_bat

copy %1 %2
if errorlevel 1 (
  echo.
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo.
  if not exist "%SystemRoot%\system32\choice.exe" (pause) else (
    echo %1
    echo.
    echo %2
    echo.
    echo Do you want to retry coping?
    choice
    if not ERRORLEVEL ==2 goto mycopy_bat
  )
)
exit /b
