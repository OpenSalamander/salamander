@echo off

if "%~1"=="" (
  echo This is an internal batch, do not call it directly.
  echo.
  pause
  exit 1
)

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit 1
)

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo %OPENSAL_BUILD_DIR%salamander does not exist!
  echo.
  pause
  exit 1
)

set SLGS=%OPENSAL_BUILD_DIR%salamander\%1
for /r %SLGS% %%i in (*.slg) do call :sign %%i
pause
exit 0

:sign
call ..\..\tools\codesign\sign_with_retry.cmd %1
if %errorlevel% NEQ 0 (
  echo sign_with_retry.cmd failed, press Enter to exit
  echo.
  pause
  exit %errorlevel%
)
exit /b
