@echo off

call :mycopy_with_mkdir_bat %2 %1 "%~1\..\..\..\..\Debug_x86\plugins\zip\zip2sfx" Debug_x86\plugins\zip\zip2sfx
call :mycopy_with_mkdir_bat %2 %1 "%~1\..\..\..\..\Release_x64\plugins\zip\zip2sfx" Release_x64\plugins\zip\zip2sfx
call :mycopy_with_mkdir_bat %2 %1 "%~1\..\..\..\..\Debug_x64\plugins\zip\zip2sfx" Debug_x64\plugins\zip\zip2sfx

exit /b


:mycopy_with_mkdir_bat

echo Copying %~1 and %~n1.pdb to %4...

if not exist %3 mkdir %3

copy "%~2\%~1" "%~3\"
if errorlevel 1 (
  echo Error: unable to copy file "%~2\%~1" to "%~3".
  exit 1
)
copy "%~2\%~n1.pdb" "%~3\"
if errorlevel 1 (
  echo Error: unable to copy file "%~2\%~n1.pdb" to "%~3\".
  exit 1
)
exit /b
