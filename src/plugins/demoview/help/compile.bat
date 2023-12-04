@echo off

setlocal

set HHCEXE=C:\Program Files (x86)\HTML Help Workshop\hhc.exe

if not exist "%HHCEXE%" (
  echo Unable to find HTML Help Workshop.
  echo.
  pause
  endlocal
  exit /b
)

call "%HHCEXE%" demoview.hhp

if not "%1"=="" (call "%1" demoview.chm) else pause
endlocal
exit /b
