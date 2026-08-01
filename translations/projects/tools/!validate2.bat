@rem Validate all modules - reports all failures without stopping.
@rem Run from a language project directory.

@set all_sal_trl_modules_validated=1
@call :run_validation 7zip.atp
@call :run_validation automation.atp
@call :run_validation checksum.atp
@call :run_validation checkver.atp
@call :run_validation dbviewer.atp
@call :run_validation diskmap.atp
@call :run_validation filecomp.atp
@call :run_validation ftp.atp
@call :run_validation ieviewer.atp
@call :run_validation mmviewer.atp
@call :run_validation nethood.atp
@call :run_validation pak.atp
@call :run_validation peviewer.atp
@call :run_validation pictview.atp
@call :run_validation regedt.atp
@call :run_validation renamer.atp
@call :run_validation salamand.atp
@call :run_validation splitcbn.atp
@call :run_validation tar.atp
@call :run_validation unarj.atp
@call :run_validation uncab.atp
@call :run_validation unchm.atp
@call :run_validation undelete.atp
@call :run_validation unfat.atp
@call :run_validation uniso.atp
@call :run_validation unlha.atp
@call :run_validation unmime.atp
@call :run_validation unrar.atp
@call :run_validation wmobile.atp
@call :run_validation zip.atp

@echo.
@if %all_sal_trl_modules_validated% equ 1 @echo All validated modules are OK.
@if %all_sal_trl_modules_validated% equ 0 @echo Validation FAILED for at least one module.
@echo.

@if NOT "%CALLED_FROM_MAKE_ALL%" == "" goto SKIP_PAUSE;
@pause
:SKIP_PAUSE

@exit /b


:run_validation
@echo Validating %1 ...
@if not exist "%1" (
  @echo   Skipping %1 - project file not found.
  @exit /b
)
@call "%TRANSLATOR_EXE%" -build-dir "%BUILD_DIR%" -quiet-validate-all %1
@if %ERRORLEVEL% EQU 0 goto FAILED
@exit /b
:FAILED
@set all_sal_trl_modules_validated=0
@echo ... FAILED
@exit /b
