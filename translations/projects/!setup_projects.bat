@echo off
setlocal EnableDelayedExpansion

rem ==========================================================================
rem Set Up Translator Project Directories
rem ==========================================================================
rem
rem This script creates the per-language translator project directories under
rem translations\projects\, generating .atp project files that point to the
rem built binaries and symbol files.
rem
rem Prerequisites:
rem   - OPENSAL_BUILD_DIR environment variable must be set
rem   - The main solution must be built (english.slg files must exist)
rem   - translator.exe must be built
rem
rem Usage:
rem   !setup_projects.bat [config] [arch]
rem
rem   config  - Build configuration: Release or Debug (default: Release)
rem   arch    - Target architecture: x86 or x64 (default: x86)
rem ==========================================================================

if "%OPENSAL_BUILD_DIR%"=="" (
  echo ERROR: Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b 1
)

rem --- Parse arguments ---
set "BUILD_CONFIG=%~1"
set "BUILD_ARCH=%~2"
if "%BUILD_CONFIG%"=="" set "BUILD_CONFIG=Release"
if "%BUILD_ARCH%"=="" set "BUILD_ARCH=x86"

rem --- Set paths ---
set "BUILD_DIR=%OPENSAL_BUILD_DIR%salamander\%BUILD_CONFIG%_%BUILD_ARCH%"
set "TRANSLATOR=%OPENSAL_BUILD_DIR%translator\Release\translator.exe"
set "SCRIPT_DIR=%~dp0"
set "SYMBOLS_DIR=%SCRIPT_DIR%..\symbols"
set "PROJ_BASE=%SCRIPT_DIR%"

rem --- Verify prerequisites ---
if not exist "%TRANSLATOR%" (
  echo ERROR: translator.exe not found at:
  echo   %TRANSLATOR%
  echo.
  echo Build the translator first.
  echo.
  pause
  exit /b 1
)

if not exist "%BUILD_DIR%\lang\english.slg" (
  echo ERROR: english.slg not found at:
  echo   %BUILD_DIR%\lang\english.slg
  echo.
  echo Build the main solution first.
  echo.
  pause
  exit /b 1
)

rem --- Define modules ---
set "MAIN_MODULE=salamand"
set PLUGIN_MODULES=7zip automation checksum checkver dbviewer diskmap filecomp
set PLUGIN_MODULES=%PLUGIN_MODULES% ftp ieviewer mmviewer nethood pak peviewer
set PLUGIN_MODULES=%PLUGIN_MODULES% pictview regedt renamer splitcbn tar unarj
set PLUGIN_MODULES=%PLUGIN_MODULES% uncab unchm undelete unfat uniso unlha unmime
set PLUGIN_MODULES=%PLUGIN_MODULES% unrar wmobile zip

rem --- Define languages (all directories under translations/ that contain .slt files) ---
set LANGUAGES=chinesesimplified czech dutch french german hungarian romanian russian slovak spanish

echo ==========================================================================
echo Setting up translator projects for %BUILD_CONFIG%_%BUILD_ARCH%
echo ==========================================================================
echo Build dir:    %BUILD_DIR%
echo Translator:   %TRANSLATOR%
echo Symbols dir:  %SYMBOLS_DIR%
echo.

for %%L in (%LANGUAGES%) do (
  echo Setting up project: %%L
  set "LANG_DIR=%PROJ_BASE%%%L"

  if not exist "!LANG_DIR!" mkdir "!LANG_DIR!"

  rem .slt files are stored directly in the language project directory

  rem --- Generate main module .atp ---
  call :gen_main_atp "%%L" "!LANG_DIR!"

  rem --- Generate plugin module .atp files ---
  for %%M in (%PLUGIN_MODULES%) do (
    if exist "%BUILD_DIR%\plugins\%%M\lang\english.slg" (
      call :gen_plugin_atp "%%L" "%%M" "!LANG_DIR!"
    )
  )

  rem --- Create per-language wrapper scripts ---
  call :gen_wrapper_scripts "!LANG_DIR!"

  echo   Done.
)

echo.
echo ==========================================================================
echo Project setup complete.
echo.
echo To use the translator interactively:
echo   1. cd translations\projects\^<language^>
echo   2. Run !open_all.bat to open all modules in the translator GUI
echo      or open individual .atp files with translator.exe
echo.
echo To import/export SLT files:
echo   1. cd translations\projects\^<language^>
echo   2. Run !import_slt.bat or !export_slt.bat
echo ==========================================================================
echo.
pause
exit /b 0


rem ==========================================================================
rem :gen_main_atp <language> <lang_dir>
rem ==========================================================================
:gen_main_atp
set "G_LANG=%~1"
set "G_DIR=%~2"
set "G_ORIGINAL=%BUILD_DIR%\lang\english.slg"
set "G_TRANSLATED=%BUILD_DIR%\lang\%G_LANG%.slg"

rem Copy english.slg if translated doesn't exist yet
if not exist "%G_TRANSLATED%" (
  copy /Y "%G_ORIGINAL%" "%G_TRANSLATED%" >nul 2>&1
)

(
  echo [Files]
  echo Original=%G_ORIGINAL%
  echo Translated=%G_TRANSLATED%
  echo Include=%SYMBOLS_DIR%\symbols.inc
  echo SalMenu=%SYMBOLS_DIR%\salmenu.mnu
  if exist "%SYMBOLS_DIR%\ignore.lst" echo IgnoreList=%SYMBOLS_DIR%\ignore.lst
  if exist "%SYMBOLS_DIR%\check.lst" echo CheckList=%SYMBOLS_DIR%\check.lst
  echo SalamanderExe=%BUILD_DIR%\salamand.exe
  echo.
  echo [Settings]
  echo ExpandStrings=0
  echo ExpandMenus=0
  echo ExpandDialogs=0
  echo SelectedTreeItem=0
  echo.
  echo [DialogsTranslation]
  echo.
  echo [MenusTranslation]
  echo.
  echo [StringsTranslation]
  echo.
  echo [Relayout]
) > "%G_DIR%\salamand.atp"
exit /b 0


rem ==========================================================================
rem :gen_plugin_atp <language> <plugin> <lang_dir>
rem ==========================================================================
:gen_plugin_atp
set "G_LANG=%~1"
set "G_PLUGIN=%~2"
set "G_DIR=%~3"
set "G_ORIGINAL=%BUILD_DIR%\plugins\%G_PLUGIN%\lang\english.slg"
set "G_TRANSLATED=%BUILD_DIR%\plugins\%G_PLUGIN%\lang\%G_LANG%.slg"
set "G_SYM_DIR=%SYMBOLS_DIR%\plugins\%G_PLUGIN%"

rem Copy english.slg if translated doesn't exist yet
if not exist "%G_TRANSLATED%" (
  copy /Y "%G_ORIGINAL%" "%G_TRANSLATED%" >nul 2>&1
)

(
  echo [Files]
  echo Original=%G_ORIGINAL%
  echo Translated=%G_TRANSLATED%
  if exist "%G_SYM_DIR%\symbols.inc" echo Include=%G_SYM_DIR%\symbols.inc
  if exist "%G_SYM_DIR%\salmenu.mnu" echo SalMenu=%G_SYM_DIR%\salmenu.mnu
  if exist "%G_SYM_DIR%\ignore.lst" echo IgnoreList=%G_SYM_DIR%\ignore.lst
  if exist "%G_SYM_DIR%\check.lst" echo CheckList=%G_SYM_DIR%\check.lst
  echo SalamanderExe=%BUILD_DIR%\salamand.exe
  echo.
  echo [Settings]
  echo ExpandStrings=0
  echo ExpandMenus=0
  echo ExpandDialogs=0
  echo SelectedTreeItem=0
  echo.
  echo [DialogsTranslation]
  echo.
  echo [MenusTranslation]
  echo.
  echo [StringsTranslation]
  echo.
  echo [Relayout]
) > "%G_DIR%\%G_PLUGIN%.atp"
exit /b 0


rem ==========================================================================
rem :gen_wrapper_scripts <lang_dir>
rem ==========================================================================
:gen_wrapper_scripts
set "G_DIR=%~1"

rem Create wrapper scripts that call the shared tools
rem Each sets TRANSLATOR_EXE and calls the corresponding tool

if not exist "%G_DIR%\!import_slt.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!import_slt.bat
  ) > "%G_DIR%\!import_slt.bat"
)

if not exist "%G_DIR%\!export_slt.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!export_slt.bat
  ) > "%G_DIR%\!export_slt.bat"
)

if not exist "%G_DIR%\!export_slt_for_diff.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!export_slt_for_diff.bat
  ) > "%G_DIR%\!export_slt_for_diff.bat"
)

if not exist "%G_DIR%\!export_spellchck.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!export_spellchck.bat
  ) > "%G_DIR%\!export_spellchck.bat"
)

if not exist "%G_DIR%\!import.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!import.bat
  ) > "%G_DIR%\!import.bat"
)

if not exist "%G_DIR%\!import_trlprop.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!import_trlprop.bat
  ) > "%G_DIR%\!import_trlprop.bat"
)

if not exist "%G_DIR%\!open_all.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!open_all.bat
  ) > "%G_DIR%\!open_all.bat"
)

if not exist "%G_DIR%\!translate1.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!translate1.bat
  ) > "%G_DIR%\!translate1.bat"
)

if not exist "%G_DIR%\!translate2.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!translate2.bat
  ) > "%G_DIR%\!translate2.bat"
)

if not exist "%G_DIR%\!validate1.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!validate1.bat
  ) > "%G_DIR%\!validate1.bat"
)

if not exist "%G_DIR%\!validate2.bat" (
  (
    echo @set "TRANSLATOR_EXE=%TRANSLATOR%"
    echo @call ..\tools\!validate2.bat
  ) > "%G_DIR%\!validate2.bat"
)

exit /b 0
