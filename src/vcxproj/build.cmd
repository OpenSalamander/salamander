@echo off

:: Check for command line arguments
if "%~1"=="help" (
  echo Usage: build.cmd [build_config] [build_arch]
  echo.
  echo build_config: Build configuration, default is 'Debug'
  echo build_arch: Build architecture, default is 'x64'
  echo.
  goto :eof
)

:: Make sure that we are in a Developer Command Prompt for Visual Studio
where msbuild
if %errorlevel% neq 0 (
  echo MSBuild.exe not found. Please open a Developer Command Prompt for Visual Studio.
  echo.
  goto :eof
)

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  goto :eof
)

:: Default values for build_config and build_arch
set build_config=Debug
set build_arch=x64

:: Override default values with command line arguments if provided
if not "%~1"=="" set build_config=%~1
if not "%~2"=="" set build_arch=%~2

call :build %build_config% %build_arch%

goto :eof

:build
  echo Building %~1/%2
  if exist %3 del /q %3
  "%MSB%" salamand.sln /t:build "/p:Configuration=%~1" /p:Platform=%2 /m:%NUMBER_OF_PROCESSORS%
  exit /b