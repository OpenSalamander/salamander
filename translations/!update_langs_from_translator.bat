@echo off


echo This batch is outdated = needs to be fixed before using again.
echo.
pause
exit /b


if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

set TRLS_SOURCE=%OPENSAL_BUILD_DIR%salamander\translator\Salamand 4.0

echo This batch will update .slt files from:
echo %TRLS_SOURCE%
echo Please check if it is the last published version's directory.
echo.
pause
echo.

if not exist "%TRLS_SOURCE%" (
  echo Source directory does not exist, unable to continue.
  echo.
  pause
  exit /b
)

if exist hg_status_result.txt (
  echo hg_status_result.txt exists, unable to continue.
  echo.
  pause
  exit /b
)

hg status -m -a -r -d "glob:**" -X "glob:!update_langs_from_translator.bat" >hg_status_result.txt
set status_ok=yes
call :test_hg_status_size hg_status_result.txt
del hg_status_result.txt
if %status_ok%==no exit /b

for /D %%d in ("%TRLS_SOURCE%\projects\*") do (
  if /I "%%~nxd" neq "tools" (
    if /I "%%~nxd" neq "english" (
      if not exist "%%~nxd" (
        echo !!! %%~nxd was not found in translations directory on Hg
        echo.
        pause
        echo.
      )
    )
  )
)

for /D %%d in ("*") do (
  if not exist "%TRLS_SOURCE%\projects\%%~nxd" (
    echo !!! %%~nxd was not found in source directory
    echo.
    pause
    echo.
  ) else (
    echo Copying %%~nxd .slt files...
    xcopy "%TRLS_SOURCE%\projects\%%~nxd\slt\*.slt" "%%~nxd\" /Q /E /Y
  )
)
echo.

pause
exit /b


:test_hg_status_size
if not "%~z1"=="0" (
  type hg_status_result.txt
  echo.
  echo Uncommited changes found, please commit before running this batch.
  echo.
  pause
  set status_ok=no
)
exit /b
