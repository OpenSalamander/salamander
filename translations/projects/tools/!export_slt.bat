@rem Export SLT text archives from all module .atp projects.
@rem This batch stops on the first module where export fails.
@rem Run from a language project directory (e.g., translations\projects\Czech\).

@if not exist slt @mkdir slt

@call :run_export 7zip.atp
@call :run_export automation.atp
@call :run_export checksum.atp
@call :run_export checkver.atp
@call :run_export dbviewer.atp
@call :run_export diskmap.atp
@call :run_export filecomp.atp
@call :run_export ftp.atp
@call :run_export ieviewer.atp
@call :run_export mmviewer.atp
@call :run_export nethood.atp
@call :run_export pak.atp
@call :run_export peviewer.atp
@call :run_export pictview.atp
@call :run_export regedt.atp
@call :run_export renamer.atp
@call :run_export salamand.atp
@call :run_export splitcbn.atp
@call :run_export tar.atp
@call :run_export unarj.atp
@call :run_export uncab.atp
@call :run_export unchm.atp
@call :run_export undelete.atp
@call :run_export unfat.atp
@call :run_export uniso.atp
@call :run_export unlha.atp
@call :run_export unmime.atp
@call :run_export unrar.atp
@call :run_export wmobile.atp
@call :run_export zip.atp

@echo.
@echo SLT exported for all modules.
@echo.

@if NOT "%CALLED_FROM_MAKE_ALL%" == "" goto SKIP_PAUSE;
@pause
:SKIP_PAUSE

@exit /b


:run_export
@echo Exporting SLT for %1 ...
@if not exist "%1" (
  @echo   Skipping %1 - project file not found.
  @exit /b
)
@call "%TRANSLATOR_EXE%" -build-dir "%BUILD_DIR%" -quiet-export-slt "slt" %1
@if %ERRORLEVEL% EQU 0 @exit
@exit /b
