@echo off

REM
REM Usage: makeall.bat
REM
REM Provede build SFX balicku do podadresare sfx

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo Build directory does not exist: %OPENSAL_BUILD_DIR%salamander
  echo.
  pause
  exit /b
)

setlocal
set making_all_versions=dummy

echo Making all language versions.

for %%v in (ENGLISH_VERSION CZECH_VERSION SLOVAK_VERSION GERMAN_VERSION ^
            HUNGARIAN_VERSION SPANISH_VERSION ROMANIAN_VERSION ^
            RUSSIAN_VERSION CHINESESIMPL_VERSION DUTCH_VERSION ^
            FRENCH_VERSION) do (
  call sfxmake.bat %%v sfx\
)

echo.
echo Deleting intermediate directories...

if exist ..\sfxmake\Release rmdir /Q /S ..\sfxmake\Release
call :remove_dir_autoretry Release
call :remove_dir_autoretry ReleaseEx

rem @mkdir Other_SFX
rem @call :sfxmove ..\..\RELEASE\plugins\zip\sfx\hungarian.sfx Other_SFX\hungarian.sfx

echo.
echo Press any key to copy SFX packages to %OPENSAL_BUILD_DIR%salamander...
echo.
pause

for %%t in (Debug_x86 Release_x86 Debug_x64 Release_x64) do (
  echo.
  echo Copying SFX packages to %%t...
  if not exist "%OPENSAL_BUILD_DIR%salamander\%%t\plugins\zip\sfx" mkdir "%OPENSAL_BUILD_DIR%salamander\%%t\plugins\zip\sfx"
  for %%s in (czech.sfx english.sfx german.sfx slovak.sfx spanish.sfx ^
              romanian.sfx hungarian.sfx russian.sfx chinesesimplified.sfx ^
              dutch.sfx french.sfx) do (
    @call :sfxcopy "sfx\%%s" "%OPENSAL_BUILD_DIR%salamander\%%t\plugins\zip\sfx\%%s"
  )
)

if exist sfx rmdir /Q /S sfx

endlocal
if "%makeall_should_pause%"=="" (
  echo.
  pause
)
@exit /b


rem ---------------------------- Copy rutina

:sfxcopy

copy /Y %1 %2
if not errorlevel 1 goto END_copy
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
pause

:END_copy

exit /b

rem ---------------------------- Move rutina

:sfxmove

move /Y %1 %2
if not errorlevel 1 goto END_move
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
pause

:END_move

exit /b

rem ---------------------------- Remove Directory with auto-retry rutina

:remove_dir_autoretry
if exist %1 rmdir /Q /S %1
if exist %1 (
  echo Trying to remove directory %1 again after 1 second waiting...
  CHOICE /T 1 /C yn /D y >nul
  goto :remove_dir_autoretry
)
exit /b
