@rem Search for untranslated strings in all modules - reports all modules.
@rem Run from a language project directory.

@set all_sal_trl_modules_translated=1
@call :run_translate 7zip.atp
@call :run_translate automation.atp
@call :run_translate checksum.atp
@call :run_translate checkver.atp
@call :run_translate dbviewer.atp
@call :run_translate diskmap.atp
@call :run_translate filecomp.atp
@call :run_translate ftp.atp
@call :run_translate ieviewer.atp
@call :run_translate mmviewer.atp
@call :run_translate nethood.atp
@call :run_translate pak.atp
@call :run_translate peviewer.atp
@call :run_translate pictview.atp
@call :run_translate regedt.atp
@call :run_translate renamer.atp
@call :run_translate salamand.atp
@call :run_translate splitcbn.atp
@call :run_translate tar.atp
@call :run_translate unarj.atp
@call :run_translate uncab.atp
@call :run_translate unchm.atp
@call :run_translate undelete.atp
@call :run_translate unfat.atp
@call :run_translate uniso.atp
@call :run_translate unlha.atp
@call :run_translate unmime.atp
@call :run_translate unrar.atp
@call :run_translate wmobile.atp
@call :run_translate zip.atp

@echo.
@if %all_sal_trl_modules_translated% equ 1 @echo All modules are completely translated.
@if %all_sal_trl_modules_translated% equ 0 @echo Untranslated strings FOUND in at least one module.
@echo.
@pause
@exit /b


:run_translate
@echo Searching untranslated strings in %1 ...
@if not exist "%1" (
  @echo   Skipping %1 - project file not found.
  @exit /b
)
@call "%TRANSLATOR_EXE%" -build-dir "%BUILD_DIR%" -quiet-translate %1
@if %ERRORLEVEL% EQU 0 goto FOUND
@exit /b
:FOUND
@set all_sal_trl_modules_translated=0
@echo ... FOUND untranslated strings
@exit /b
