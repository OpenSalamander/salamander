@rem Mark all changed texts as translated for all modules.
@rem Run from a language project directory.

@call :run_mark 7zip.atp
@call :run_mark automation.atp
@call :run_mark checksum.atp
@call :run_mark checkver.atp
@call :run_mark dbviewer.atp
@call :run_mark diskmap.atp
@call :run_mark filecomp.atp
@call :run_mark ftp.atp
@call :run_mark ieviewer.atp
@call :run_mark mmviewer.atp
@call :run_mark nethood.atp
@call :run_mark pak.atp
@call :run_mark peviewer.atp
@call :run_mark pictview.atp
@call :run_mark regedt.atp
@call :run_mark renamer.atp
@call :run_mark salamand.atp
@call :run_mark splitcbn.atp
@call :run_mark tar.atp
@call :run_mark unarj.atp
@call :run_mark uncab.atp
@call :run_mark unchm.atp
@call :run_mark undelete.atp
@call :run_mark unfat.atp
@call :run_mark uniso.atp
@call :run_mark unlha.atp
@call :run_mark unmime.atp
@call :run_mark unrar.atp
@call :run_mark wmobile.atp
@call :run_mark zip.atp

@echo.
@echo Changed texts marked as translated for all modules.
@echo.
@pause
@exit /b


:run_mark
@echo Marking changed texts as translated in %1 ...
@if not exist "%1" (
  @echo   Skipping %1 - project file not found.
  @exit /b
)
@call "%TRANSLATOR_EXE%" -build-dir "%BUILD_DIR%" -quiet-mark-changed-as-translated %1
@if %ERRORLEVEL% EQU 0 @exit 1
@exit /b
