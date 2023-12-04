@echo off

set SAL_POPULATE_ROOT=%~dp0%

set MSVC_REDIST_PATH=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Redist\MSVC\14.38.33130
set UCRT_REDIST_PATH=C:\Program Files (x86)\Windows Kits\10\Redist\10.0.22621.0
set UCRTD_REDIST_PATH=C:\Program Files (x86)\Microsoft SDKs\Windows Kits\10\ExtensionSDKs\Microsoft.UniversalCRT.Debug\10.0.22621.0

echo Populate Open Salamander Build directory with redistributable DLLs and optionally
echo with SFX packages and helps.
echo.
echo Please UPDATE paths to redistributable DLLs MANUALLY for VC2022 and last SDK:
echo MSCV:
echo %MSVC_REDIST_PATH%
echo Universal CRT RELEASE:
echo %UCRT_REDIST_PATH%
echo Universal CRT DEBUG:
echo %UCRTD_REDIST_PATH%
echo.
pause
echo.

if "%OPENSAL_BUILD_DIR%"=="" (
  echo Please set OPENSAL_BUILD_DIR environment variable.
  echo.
  pause
  exit /b
)

if not exist "%OPENSAL_BUILD_DIR%salamander" (
  echo Target directory does not exist: %OPENSAL_BUILD_DIR%salamander
  echo.
  pause
  exit /b
)

powershell.exe -Command "$s=(New-Object -ComObject WScript.Shell).CreateShortcut($env:OPENSAL_BUILD_DIR+'salamander\salamander.lnk'); $s.TargetPath=$env:SAL_POPULATE_ROOT+'salamand.sln'; $s.Save()"

echo Populating directory: %OPENSAL_BUILD_DIR%salamander
call :copy_files Debug_x86   debug_nonredist\x86\Microsoft.VC143.DebugCRT concrt140d.dll msvcp140d.dll vcruntime140d.dll "%UCRTD_REDIST_PATH%\Redist\Debug\x86"
call :copy_files Release_x86 x86\Microsoft.VC143.CRT                      concrt140.dll  msvcp140.dll  vcruntime140.dll  "%UCRT_REDIST_PATH%\ucrt\DLLs\x86"
call :copy_files Debug_x64   debug_nonredist\x64\Microsoft.VC143.DebugCRT concrt140d.dll msvcp140d.dll vcruntime140d.dll "%UCRTD_REDIST_PATH%\Redist\Debug\x64"
call :copy_files Release_x64 x64\Microsoft.VC143.CRT                      concrt140.dll  msvcp140.dll  vcruntime140.dll  "%UCRT_REDIST_PATH%\ucrt\DLLs\x64"

echo.
echo Copying third-party files...

for %%t in (Debug_x86 Debug_x64 Release_x86 Release_x64) do (
  echo.
  echo Target directory: %%t
  call :copy_thirdparty "%OPENSAL_BUILD_DIR%salamander\%%t"
)

call :sfx_make_all

call :help_compile_all

echo.
echo DONE
echo.
echo Other important tasks:
echo -Build WinSCP plugin.
echo -Build all language versions of Salamander and plugins.
echo.
pause
exit /b


:sfx_make_all
echo.
set __your_choice=No
set /P __your_choice=Do you want to rebuild SFX packages for ZIP plugin and Zip2SFX? [No]: 

if /I "%__your_choice%" EQU "Y" goto build_sfx
if /I "%__your_choice%" EQU "YES" goto build_sfx
goto sfx_end
:build_sfx
echo.
pushd ..\plugins\zip\vcxproj\selfextr
set makeall_should_pause=no
call makeall.bat
popd
:sfx_end
exit /b


:help_compile_all
echo.
set __your_choice=No
set /P __your_choice=Do you want to compile all help files? [No]: 

if /I "%__your_choice%" EQU "Y" goto compile_help
if /I "%__your_choice%" EQU "YES" goto compile_help
goto help_end
:compile_help
echo.
pushd ..\..\help\src
call compileall.bat
popd
:help_end
exit /b

:copy_files
echo.
echo Target directory: %1
if not exist "%OPENSAL_BUILD_DIR%salamander\%1" mkdir "%OPENSAL_BUILD_DIR%salamander\%1"
echo Copying %2\%3...
call :mycopy "%MSVC_REDIST_PATH%\%2\%3" "%OPENSAL_BUILD_DIR%salamander\%1\"
echo Copying %2\%4...
call :mycopy "%MSVC_REDIST_PATH%\%2\%4" "%OPENSAL_BUILD_DIR%salamander\%1\"
echo Copying %2\%5...
call :mycopy "%MSVC_REDIST_PATH%\%2\%5" "%OPENSAL_BUILD_DIR%salamander\%1\"
echo Copying %6...
call :mycopy_dir %6 "%OPENSAL_BUILD_DIR%salamander\%1"
echo Copying ..\res\toolbars...
call :mycopy_dir ..\res\toolbars "%OPENSAL_BUILD_DIR%salamander\%1\toolbars\"
exit /b


:copy_thirdparty
call :mycopy_with_mkdir_bat convert.cfg ..\..\convert\centeuro %1\convert\centeuro
call :mycopy_with_mkdir_bat *.tab       ..\..\convert\centeuro %1\convert\centeuro
call :mycopy_with_mkdir_bat convert.cfg ..\..\convert\cyrillic %1\convert\cyrillic
call :mycopy_with_mkdir_bat *.tab       ..\..\convert\cyrillic %1\convert\cyrillic
call :mycopy_with_mkdir_bat convert.cfg ..\..\convert\westeuro %1\convert\westeuro
call :mycopy_with_mkdir_bat *.tab       ..\..\convert\westeuro %1\convert\westeuro

call :mycopy_with_mkdir_bat *.* ..\plugins\automation\sample-scripts %1\plugins\automation\scripts

call :mycopy_with_mkdir_bat *.* ..\plugins\ieviewer\cmark-gfm\css %1\plugins\ieviewer\css

call :mycopy_with_mkdir_bat readme.txt     ..\plugins\zip\zip2sfx %1\plugins\zip\zip2sfx
call :mycopy_with_mkdir_bat sam_cz.set     ..\plugins\zip\zip2sfx %1\plugins\zip\zip2sfx
call :mycopy_with_mkdir_bat sample.set     ..\plugins\zip\zip2sfx %1\plugins\zip\zip2sfx

exit /b


:mycopy_with_mkdir_bat

echo Copying %~2\%~1...

if not exist %3 mkdir %3

copy "%~2\%~1" "%~3\%~1"
if errorlevel 1 (
  echo.
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo.
  if not exist "%SystemRoot%\system32\choice.exe" (pause) else (
    echo %~2\%~1
    echo.
    echo %~3\%~1
    echo.
    echo Do you want to retry copying?
    choice
    if not ERRORLEVEL ==2 goto mycopy_with_mkdir_bat
  )
)
exit /b


:mycopy

copy %1 %2
if errorlevel 1 (
  echo.
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo.
  if not exist "%SystemRoot%\system32\choice.exe" (pause) else (
    echo %1
    echo.
    echo %2
    echo.
    echo Do you want to retry copying?
    choice
    if not ERRORLEVEL ==2 goto mycopy
  )
)
exit /b


:mycopy_dir

robocopy %1 %2 >nul
if not errorlevel 1 (
  echo.
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
  echo.
  if not exist "%SystemRoot%\system32\choice.exe" (pause) else (
    echo %1
    echo.
    echo %2
    echo.
    echo Do you want to retry copying of whole directory?
    choice
    if not ERRORLEVEL ==2 goto mycopy_dir
  )
)
exit /b
