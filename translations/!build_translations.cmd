@echo off
setlocal EnableDelayedExpansion

rem ==========================================================================
rem Build Translated SLG Files from SLT Text Archives
rem ==========================================================================
rem
rem Imports .slt text archives for all languages into .slg resource DLLs
rem using the Altap Translator tool (translator.exe).
rem
rem The .atp project files use relative paths (..\..\bin\, ..\..\symbols\).
rem Instead of creating directory junctions (which require admin privileges),
rem this script generates temporary .atp copies with absolute paths resolved
rem from OPENSAL_BUILD_DIR and the translations directory.
rem
rem Prerequisites:
rem   - OPENSAL_BUILD_DIR environment variable must be set
rem   - The main solution must be built (english.slg files must exist)
rem   - translator.exe must be built (from translator.sln or the
rem     "Utils (Release)" configuration in salamand.sln)
rem
rem Usage:
rem   !build_translations.cmd [config] [arch]
rem
rem   config  - Build configuration: Release or Debug (default: Release)
rem   arch    - Target architecture: x86 or x64 (default: x86)
rem
rem Examples:
rem   !build_translations.cmd Release x86
rem   !build_translations.cmd Debug x64
rem ==========================================================================

rem --- Check environment ---
if "%OPENSAL_BUILD_DIR%"=="" (
  echo ERROR: OPENSAL_BUILD_DIR environment variable is not set.
  echo.
  pause
  exit /b 1
)

rem --- Parse arguments ---
set "CONFIG=%~1"
set "ARCH=%~2"
if "!CONFIG!"=="" set "CONFIG=Release"
if "!ARCH!"=="" set "ARCH=x86"

if /I not "!CONFIG!"=="Release" if /I not "!CONFIG!"=="Debug" (
  echo ERROR: Invalid configuration "!CONFIG!". Use Release or Debug.
  pause
  exit /b 1
)

if /I not "!ARCH!"=="x86" if /I not "!ARCH!"=="x64" (
  echo ERROR: Invalid architecture "!ARCH!". Use x86 or x64.
  pause
  exit /b 1
)

rem --- Set paths ---
set "BUILD_DIR=%OPENSAL_BUILD_DIR%salamander\!CONFIG!_!ARCH!"
set "TRANSLATOR=%OPENSAL_BUILD_DIR%translator\Release\translator.exe"
set "SCRIPT_DIR=%~dp0"
set "PROJECTS_DIR=!SCRIPT_DIR!projects"
set "SYMBOLS_DIR=!SCRIPT_DIR!symbols"

rem --- Verify prerequisites ---
if not exist "!TRANSLATOR!" (
  echo ERROR: translator.exe not found:
  echo   !TRANSLATOR!
  echo.
  echo Build it using the "Utils ^(Release^)" configuration in salamand.sln
  echo or build translator.sln directly.
  pause
  exit /b 1
)

if not exist "!BUILD_DIR!" (
  echo ERROR: Build directory not found:
  echo   !BUILD_DIR!
  pause
  exit /b 1
)

if not exist "!BUILD_DIR!\lang\english.slg" (
  echo ERROR: english.slg not found:
  echo   !BUILD_DIR!\lang\english.slg
  echo.
  echo Build the main solution first.
  pause
  exit /b 1
)

if not exist "!BUILD_DIR!\salamand.exe" (
  echo ERROR: salamand.exe not found:
  echo   !BUILD_DIR!\salamand.exe
  echo.
  echo Build the main solution first.
  pause
  exit /b 1
)

if not exist "!PROJECTS_DIR!" (
  echo ERROR: Projects directory not found:
  echo   !PROJECTS_DIR!
  pause
  exit /b 1
)

if not exist "!SYMBOLS_DIR!" (
  echo ERROR: Symbols directory not found:
  echo   !SYMBOLS_DIR!
  pause
  exit /b 1
)

rem --- Print header ---
echo ==========================================================================
echo Building translations: !CONFIG!_!ARCH!
echo ==========================================================================
echo Build dir:    !BUILD_DIR!
echo Translator:   !TRANSLATOR!
echo Projects:     !PROJECTS_DIR!
echo Symbols:      !SYMBOLS_DIR!
echo.

set "TOTAL=0"
set "OK_COUNT=0"
set "WARN_COUNT=0"

rem --- Process each language directory ---
for /D %%L in ("!PROJECTS_DIR!\*") do (
  set "DNAME=%%~nxL"
  rem Skip the shared tools directory and English (validate-only)
  if /I "!DNAME!" neq "tools" if /I "!DNAME!" neq "English" (
    call :ProcessLanguage "%%L" "!DNAME!"
  )
)

rem --- Print summary ---
echo ==========================================================================
echo Done. Modules: !TOTAL! total, !OK_COUNT! OK, !WARN_COUNT! warnings.
if !WARN_COUNT! gtr 0 (
  echo.
  echo Some modules had warnings - check the output above.
  pause
  exit /b 1
)
echo ==========================================================================
exit /b 0


rem ==========================================================================
:ProcessLanguage
rem   %~1  Full path to language project directory
rem   %~2  Language name (directory name)
rem ==========================================================================
set "LANG_DIR=%~1"
set "LANG=%~2"

echo --------------------------------------------------------------------------
echo !LANG!
echo --------------------------------------------------------------------------
echo.

if not exist "!LANG_DIR!\slt" (
  echo   No slt directory found - skipping.
  echo.
  exit /b 0
)

rem translator.exe resolves .atp paths from CWD
pushd "!LANG_DIR!"

for %%A in ("*.atp") do (
  call :BuildModule "%%~nA" "%%A"
)

popd
echo.
exit /b 0


rem ==========================================================================
:BuildModule
rem   %~1  Module name (without .atp extension)
rem   %~2  ATP filename
rem   CWD = language project directory
rem
rem   Creates a temporary .atp with absolute paths, runs the SLT import,
rem   then cleans up the temp file.
rem ==========================================================================
set "MOD=%~1"
set "ATP=%~2"

echo   !MOD!

rem --- Skip modules without .slt source ---
if not exist "slt\!MOD!.slt" (
  echo   !MOD! - skipped, no .slt file
  exit /b 0
)

rem --- Run translator import ---
"!TRANSLATOR!" -build-dir "!BUILD_DIR!" -quiet-import-slt "slt" "%ATP%"
set "RC=!ERRORLEVEL!"

rem translator.exe: 1 = success, 0 = issues found
if !RC! equ 1 (
  set /A OK_COUNT+=1
) else (
  echo     WARNING: issues found ^(exit code !RC!^)
  set /A WARN_COUNT+=1
)

set /A TOTAL+=1
exit /b 0
