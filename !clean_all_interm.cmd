@echo off

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if "%skip_pauses_in_clean_all_interm%"=="" (
  echo Clean_All_Interm - Clean Intermediate Files
  echo                    from Source and Binary Directories.
  echo.
  echo Exit your Visual Studio and press any key...
  echo.
  pause
)

rem ---------------------------- Call cleandir subroutine for all subdirectories
set called_from_clean_all_interm=yes
call !clean_src_interm.cmd
rem echo.

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo Build directory does not exist: %OPENSAL_BUILD_DIR%salamander
  echo.
  pause
  exit /b
)

for %%t in (Debug_x86 Release_x86 Debug_x64 Release_x64) do (
  call :clean_salbin_dir "%OPENSAL_BUILD_DIR%salamander\%%t"
)

if "%skip_pauses_in_clean_all_interm%"=="" (
  echo.
  echo DONE
  echo.
  pause
)
exit /b


rem ---------------------------- Clean Function for SALBIN Subdirectory
:clean_salbin_dir
if exist %1 (
  rem echo.
  rem echo Cleaning directory: %1
  pushd %1
  call :clean_bin_dir
  rem echo.
  popd
)
exit /b


rem ---------------------------- Clean Function for Directory with Binaries
:clean_bin_dir
for /R %%G in (.) do (
  pushd "%%G"
  rem --- delete subdirs
  rd /s/q Intermediate 2>nul
  rd /s/q Debug 2>nul
  rd /s/q Release 2>nul
  rem --- delete files
  del /q *.lib *.ilk *.exp 2>nul
  popd
)
exit /b
