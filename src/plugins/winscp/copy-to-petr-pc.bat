@echo off
call :langcopy_bat english
call :langcopy_bat czech
call :langcopy_bat german
call :langcopy_bat spanish
call :langcopy_bat hungarian
call :mycopy_bat "Z:\PETR\PETR\SALAMAND\PLUGINS\WINSCP\lang\english.slg" "Z:\PETR\PETR\SALAMAND\PLUGINS\WINSCP\lang\romanian.slg"
call :langcopy_bat romanian
call :langcopy_bat chinesesimplified
call :langcopy_bat slovak
call :langcopy_bat french
call :langcopy_bat russian
call :langcopy_bat dutch

for %%v in (vc2019) do (
  if exist "Z:\WORK\MSVC\SALAMAND\%%v" (
    for %%t in (Debug_x86 Release_x86) do (
      if not exist "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp" mkdir "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp"
      call :mycopy_bat "Z:\PETR\PETR\SALAMAND\PLUGINS\WINSCP\winscp.spl" "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp\winscp.spl"
    )
  )
)

exit /b


rem ---------------------------- Copy rutina pro jazyky

:langcopy_bat

for %%v in (vc2019) do (
  if exist "Z:\WORK\MSVC\SALAMAND\%%v" (
    for %%t in (Debug_x86 Release_x86) do (
      if not exist "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp\lang" mkdir "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp\lang"
      call :mycopy_bat "Z:\PETR\PETR\SALAMAND\PLUGINS\WINSCP\lang\%1.slg" "Z:\WORK\MSVC\SALAMAND\%%v\%%t\plugins\winscp\lang\%1.slg"
    )
  )
)

exit /b


rem ---------------------------- Copy rutina

:mycopy_bat

echo Copying %1 to %2...
copy %1 %2
if not errorlevel 1 goto END
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
echo !!!!!!!!!!!!!!!!!!!   ERROR   !!!!!!!!!!!!!!!!!!!
pause

:END

exit /b
