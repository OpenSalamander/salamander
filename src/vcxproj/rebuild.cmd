@echo off

set MSB=C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if not exist "%MSB%" (
  echo MSBuild.exe not found: %MSB%
  echo.
  pause
  exit /b
)

echo Rebuild Menu:
echo.
echo 3 - rebuild all targets
echo 5 - Internal Build (Utils+Debug x86/x64)
echo 6 - Developers Build (Utils+Debug+Release x86/x64)
echo 8 - Release/Beta Build (Utils+Release x86/x64)
echo 9 - Utils Only (Utils x86/x64)
echo.
set __your_choice=5
set /P __your_choice=Select what you want to rebuild (0-9) [5]: 
echo.

if %__your_choice% LSS 3 goto :choice_is_invalid
if %__your_choice% GTR 9 goto :choice_is_invalid
goto :choice_is_ok

:choice_is_invalid
echo Invalid selection, exiting...
echo.
pause
exit /b
  
:choice_is_ok
set __skip_debug=no
set __skip_release=no
set __skip_release_x64=no
set __skip_utils=no
goto :choice_%__your_choice%

:choice_3
echo Rebuild all targets
goto :end_choice

:choice_5
echo Internal Build (Utils+Debug x86/x64)
set __skip_release=yes
goto :end_choice

:choice_6
echo Developers Build (Utils+Debug+Release x86/x64)
goto :end_choice

:choice_8
echo Release/Beta Build (Utils+Release x86/x64)
set __skip_debug=yes
goto :end_choice

:choice_9
echo Utils Only (Utils x86/x64)
set __skip_release=yes
set __skip_debug=yes
goto :end_choice

:end_choice
echo.

call :remove_files "rebuild_*.log*"

set TIMESLOG=rebuild_times.log
set STARTTIME=%time%
set BUILDSTART=%time%
echo START TIME:%STARTTIME% >%TIMESLOG%

if "%__skip_debug%"=="no" (
  call :rebuild Debug             Win32 rebuild_debug_x86.log
  call :rebuild Debug             x64   rebuild_debug_x64.log
)

if "%__skip_release%"=="no" (
  call :rebuild Release           Win32 rebuild_release_x86.log
  if "%__skip_release_x64%"=="no" (
    call :rebuild Release         x64   rebuild_release_x64.log
  )
)

if "%__skip_utils%"=="no" (
  call :rebuild "Utils (Release)" Win32 rebuild_utils_x86.log
  call :rebuild "Utils (Release)" x64   rebuild_utils_x64.log
)

set ENDTIME=%time%
echo END TIME:%ENDTIME% >>%TIMESLOG%
echo. >>%TIMESLOG%
call ..\..\tools\duration.cmd "%STARTTIME%" "%ENDTIME%" "Total Duration: " >>%TIMESLOG%

if exist *.log.err (
  echo.
  echo REBUILD ERRORS:
  dir /B *.log.err
)
if exist *.log.wrn (
  echo.
  echo REBUILD WARNINGS:
  dir /B *.log.wrn
) else (
  if not exist *.log.err (
    echo.
    echo REBUILD DONE: 0 ERRORS, 0 WARNINGS
  )
)

echo.
pause
exit /b

:rebuild <target> <platform> <logfile>
echo Building %~1/%2
"%MSB%" salamand.sln /t:rebuild "/p:Configuration=%~1" /p:Platform=%2 /l:FileLogger,Microsoft.Build.Engine;logfile=%3;append=false;verbosity=normal;encoding=windows-1250 /flp1:logfile=%3.err;errorsonly /flp2:logfile=%3.wrn;warningsonly /m:%NUMBER_OF_PROCESSORS%
call ..\..\tools\duration.cmd "%BUILDSTART%" "%time%" "%~1/%2: Build Duration: " >>%TIMESLOG%
call :remove_file_if_empty %3.wrn
call :remove_file_if_empty %3.err
set BUILDSTART=%time%
exit /b

:remove_files <wildcard>
if exist %1 (
  echo Deleting files: %1 in %CD%
  dir /B %1
  del /q %1
  echo.
)
exit /b

:remove_file_if_empty <filename>
if %~z1==0 del %1
exit /b
