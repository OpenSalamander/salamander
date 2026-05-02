@echo off

if "%1"=="all" (
  for %%t in (Debug_x86 Release_x86 Debug_x64 Release_x64) do (
    call :mycopy_with_mkdir_bat %3 %2 "%~2\..\..\..\..\%%t\utils" %%t\utils
    
    if "%4"=="copy_pdb" (
      call :mycopy_with_mkdir_bat %~n3.pdb %2 "%~2\..\..\..\..\%%t\utils" %%t\utils
    )
  )
) else (
  for %%t in (Debug_ Release_) do (
    call :mycopy_with_mkdir_bat %3 %2 "%~2\..\..\..\..\%%t%1\utils" %%t%1\utils
 
    if "%4"=="copy_pdb" (
      call :mycopy_with_mkdir_bat %~n3.pdb %2 "%~2\..\..\..\..\%%t%1\utils" %%t%1\utils
    )
  )
)

exit /b


:mycopy_with_mkdir_bat

echo Copying %~1 to %4...

if not exist %3 mkdir %3

copy "%~2\%~1" "%~3\"
if errorlevel 1 (
  echo Error: unable to copy file "%~2\%~1" to "%~3\".
  exit 1
)
exit /b
