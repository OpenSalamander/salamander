@echo off
if not "%called_from_clean_all_interm%" == "yes" (
  echo Clean_Src_Interm - Clean VS 2022 Intermediate Files only from Source
  echo                    Directory.
  echo.
  echo Exit your Visual Studio and press any key...
  echo.
  pause
)

del /q src\vcxproj\rebuild_*.log 2>nul
del /q src\vcxproj\rebuild_*.log.* 2>nul
del /q src\*.aps 2>nul
del /q src\vcxproj\.vs\salamand\v17\Browse.VC.db 2>nul
del /q src\vcxproj\.vs\salamand\v17\Browse.VC.db-shm 2>nul
del /q src\vcxproj\.vs\salamand\v17\Browse.VC.db-wal 2>nul
del /q src\vcxproj\.vs\salamand\v17\Browse.VC.opendb 2>nul
del /q src\vcxproj\.vs\salamand\v17\Solution.VC.db 2>nul
del /q src\vcxproj\.vs\salamand\v17\Solution.VC.db-shm 2>nul
del /q src\vcxproj\.vs\salamand\v17\Solution.VC.db-wal 2>nul
del /q src\vcxproj\.vs\salamand\v17\fileList.bin 2>nul
rmdir /s /q src\vcxproj\.vs\salamand\v17\ipch 2>nul
rmdir /s /q src\vcxproj\.vs\salamand\FileContentIndex 2>nul

rmdir /s /q src\plugins\demomenu\vcxproj\.vs 2>nul
rmdir /s /q src\plugins\demoplug\vcxproj\.vs 2>nul
rmdir /s /q src\plugins\demoview\vcxproj\.vs 2>nul
rmdir /s /q src\plugins\unfat\vcxproj\.vs 2>nul

rem ---------------------------- Call cleandir subroutine for all subdirectories
for /D %%G in ("*") do call :clean_src_dir "%%G"

if not "%called_from_clean_all_interm%" == "yes" (
  echo.
  echo DONE
  echo.
  pause
)

exit /b

rem ---------------------------- Clean Function for Source Code Directory
:clean_src_dir
rem ---- skip .git directory
if not %1==".git" (
  rem echo Cleaning directory: %1
  pushd %1
  rem --- delete subdirs
  rem rd /s/q Debug 2>nul
  rem rd /s/q Release 2>nul
  rem --- delete visual studio intermediate files
  rem *.*.*.user: don't remove hand-made *.user that we have in repository as defaults
  del /q *.ncb *.aps *.opt *.*.*.user *.vcxproj.user *.sdf 2>nul
  rem --- *.suo has hidden attribute --- do not delete them, they contain
  rem breakpoints, window positions, etc.
  rem del /q /AH *.suo 2>nul
  for /D %%G in ("*") do call :clean_src_dir "%%G"
  popd
)
exit /b
