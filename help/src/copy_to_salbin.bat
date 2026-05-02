@echo off

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo Target directory does not exist: %OPENSAL_BUILD_DIR%salamander
  pause
  exit /b
)

if "%1"=="" (
  echo This is an internal batch, do not call it directly.
  echo.
  pause
  exit /b
)

for %%t in (Debug_x86 Release_x86 Debug_x64 Release_x64) do (
  call :mycopy_with_mkdir_bat "%~1" "%OPENSAL_BUILD_DIR%salamander\%%t\help\english"
)
del "%~1"

exit /b


rem ---------------------------- Copy rutina

:mycopy_with_mkdir_bat

if not exist %2 mkdir %2

copy %1 "%~2\%~1"
if errorlevel 1 (
  echo.>&2
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!>&2
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!>&2
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!>&2
  echo.>&2
  if not exist "%SystemRoot%\system32\choice.exe" (pause>&2) else (
    echo %~1>&2
    echo.>&2
    echo %~2\%~1>&2
    echo.>&2
    echo Do you want to retry coping?>&2
    choice>&2
    if not ERRORLEVEL ==2 goto mycopy_with_mkdir_bat
  )
)
exit /b
