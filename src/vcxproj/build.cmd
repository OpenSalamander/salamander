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

call :build	Debug	x64

pause
exit /b

:build
echo Building %~1/%2
if exist %3 del /q %3
"%MSB%" salamand.sln /t:build "/p:Configuration=%~1" /p:Platform=%2 /m:%NUMBER_OF_PROCESSORS%
exit /b
