@rem Import SLT text archives into all module .atp projects.
@rem This batch stops on the first module where import fails.
@rem Run from a language project directory (e.g., translations\projects\Czech\).

@call :run_import 7zip.atp
@call :run_import automation.atp
@call :run_import checksum.atp
@call :run_import checkver.atp
@call :run_import dbviewer.atp
@call :run_import diskmap.atp
@call :run_import filecomp.atp
@call :run_import ftp.atp
@call :run_import ieviewer.atp
@call :run_import mmviewer.atp
@call :run_import nethood.atp
@call :run_import pak.atp
@call :run_import peviewer.atp
@call :run_import pictview.atp
@call :run_import regedt.atp
@call :run_import renamer.atp
@call :run_import salamand.atp
@call :run_import splitcbn.atp
@call :run_import tar.atp
@call :run_import unarj.atp
@call :run_import uncab.atp
@call :run_import unchm.atp
@call :run_import undelete.atp
@call :run_import unfat.atp
@call :run_import uniso.atp
@call :run_import unlha.atp
@call :run_import unmime.atp
@call :run_import unrar.atp
@call :run_import wmobile.atp
@call :run_import zip.atp

@echo.
@echo SLT imported for all modules.
@echo.
@if not "%do_not_pause_import_slt_bat%"=="yes" @pause
@exit /b


:run_import
@echo Importing SLT for %1 ...
@if not exist "%1" (
  @echo   Skipping %1 - project file not found.
  @exit /b
)
@call "%TRANSLATOR_EXE%" -build-dir "%BUILD_DIR%" -quiet-import-slt "slt" %1
@if %ERRORLEVEL% EQU 0 @exit 1
@exit /b
