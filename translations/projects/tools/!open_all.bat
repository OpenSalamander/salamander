@rem Open all module .atp projects in the translator GUI.
@rem Run from a language project directory.

@if not exist 7zip.atp goto :skip
@start "" "%TRANSLATOR_EXE%" 7zip.atp
@start "" "%TRANSLATOR_EXE%" automation.atp
@start "" "%TRANSLATOR_EXE%" checksum.atp
@start "" "%TRANSLATOR_EXE%" checkver.atp
@start "" "%TRANSLATOR_EXE%" dbviewer.atp
@start "" "%TRANSLATOR_EXE%" diskmap.atp
@start "" "%TRANSLATOR_EXE%" filecomp.atp
@start "" "%TRANSLATOR_EXE%" ftp.atp
@start "" "%TRANSLATOR_EXE%" ieviewer.atp
@start "" "%TRANSLATOR_EXE%" mmviewer.atp
@start "" "%TRANSLATOR_EXE%" nethood.atp
@start "" "%TRANSLATOR_EXE%" pak.atp
@start "" "%TRANSLATOR_EXE%" peviewer.atp
@start "" "%TRANSLATOR_EXE%" pictview.atp
@start "" "%TRANSLATOR_EXE%" regedt.atp
@start "" "%TRANSLATOR_EXE%" renamer.atp
@start "" "%TRANSLATOR_EXE%" salamand.atp
@start "" "%TRANSLATOR_EXE%" splitcbn.atp
@start "" "%TRANSLATOR_EXE%" tar.atp
@start "" "%TRANSLATOR_EXE%" unarj.atp
@start "" "%TRANSLATOR_EXE%" uncab.atp
@start "" "%TRANSLATOR_EXE%" unchm.atp
@start "" "%TRANSLATOR_EXE%" undelete.atp
@start "" "%TRANSLATOR_EXE%" unfat.atp
@start "" "%TRANSLATOR_EXE%" uniso.atp
@start "" "%TRANSLATOR_EXE%" unlha.atp
@start "" "%TRANSLATOR_EXE%" unmime.atp
@start "" "%TRANSLATOR_EXE%" unrar.atp
@start "" "%TRANSLATOR_EXE%" wmobile.atp
@start "" "%TRANSLATOR_EXE%" zip.atp
@exit /b

:skip
@echo No .atp project files found in the current directory.
@echo Please run !setup_projects.bat first to generate project files.
@echo.
@pause
