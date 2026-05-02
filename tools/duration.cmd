@echo off
rem Source: http://stackoverflow.com/questions/605522/print-time-in-a-batch-file-milliseconds
setlocal

rem The format of %TIME% is HH:MM:SS,CS for example 23:59:59,99
set STARTTIME=%~1
set ENDTIME=%~2

rem output as time
rem echo STARTTIME: "%STARTTIME%"
rem echo ENDTIME: "%ENDTIME%"

rem convert STARTTIME and ENDTIME to centiseconds
rem time can be space left-padded: " 6:14:50,21"
set /A STARTTIME=(%STARTTIME:~0,2%)*360000 + (1%STARTTIME:~3,2%-100)*6000 + (1%STARTTIME:~6,2%-100)*100 + (1%STARTTIME:~9,2%-100)
set /A ENDTIME=(%ENDTIME:~0,2%)*360000 + (1%ENDTIME:~3,2%-100)*6000 + (1%ENDTIME:~6,2%-100)*100 + (1%ENDTIME:~9,2%-100)

rem calculating the duratyion is easy
set /A DURATION=%ENDTIME%-%STARTTIME%

rem we might have measured the time inbetween days
if %ENDTIME% LSS %STARTTIME% set set /A DURATION=%STARTTIME%-%ENDTIME%

rem now break the centiseconds down to hors, minutes, seconds and the remaining centiseconds
set /A DURATIONH=%DURATION% / 360000
set /A DURATIONM=(%DURATION% - %DURATIONH%*360000) / 6000
set /A DURATIONS=(%DURATION% - %DURATIONH%*360000 - %DURATIONM%*6000) / 100
set /A DURATIONHS=(%DURATION% - %DURATIONH%*360000 - %DURATIONM%*6000 - %DURATIONS%*100)

rem some formatting
if %DURATIONH% LSS 10 set DURATIONH=0%DURATIONH%
if %DURATIONM% LSS 10 set DURATIONM=0%DURATIONM%
if %DURATIONS% LSS 10 set DURATIONS=0%DURATIONS%
if %DURATIONHS% LSS 10 set DURATIONHS=0%DURATIONHS%

rem outputing
rem echo STARTTIME: %STARTTIME% centiseconds
rem echo ENDTIME: %ENDTIME% centiseconds
rem echo DURATION: %DURATION% in centiseconds
echo %~3%DURATIONH%:%DURATIONM%:%DURATIONS%,%DURATIONHS%

endlocal
goto :EOF
