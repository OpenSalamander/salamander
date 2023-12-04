@echo off

setlocal

set MSB=C:\Program Files\Microsoft Visual Studio\2022\Community\Msbuild\Current\Bin\MSBuild.exe

REM 
REM Verzi vysledneho balicku, je nutne nastavit na shodnou s verzi
REM podporovanou ZIP pluginem a Zip2Sfx
REM

set package_compatible_version=5
set package_version=5

REM
REM Tuto promennou je nutno nastavit na aktualni umisteni sfxmake.exe
REM
set sfxmake_exe="..\sfxmake\release\sfxmake.exe"

if not exist %sfxmake_exe% (
  echo.
  "%MSB%" ..\sfxmake\sfxmake.vcxproj /t:rebuild "/p:Configuration=Release" /p:Platform=Win32 /m:%NUMBER_OF_PROCESSORS%
  if not exist %sfxmake_exe% (
    echo.
    echo %sfxmake_exe% still does not exist!
    pause
    exit /B 1
  )
)

if "%1"=="" goto INVALID_LANGUAGE1
if "%2"=="" goto INVALID_LANGUAGE1

set output_directory=%~2

REM Vytvorime cilovy adresar
if not exist "%output_directory%" mkdir "%output_directory%"

if %1==ENGLISH_VERSION goto ENGLISH
if %1==CZECH_VERSION goto CZECH
if %1==SLOVAK_VERSION goto SLOVAK
if %1==GERMAN_VERSION goto GERMAN
if %1==HUNGARIAN_VERSION goto HUNGARIAN
if %1==SPANISH_VERSION goto SPANISH
if %1==ROMANIAN_VERSION goto ROMANIAN
if %1==RUSSIAN_VERSION goto RUSSIAN
if %1==CHINESESIMPL_VERSION goto CHINESESIMPL
if %1==DUTCH_VERSION goto DUTCH
if %1==FRENCH_VERSION goto FRENCH

goto INVALID_LANGUAGE2

:MAKE_SFX

echo.
echo ======================================================
echo Making %1 (code page: %2)
echo ======================================================
echo.
setlocal
set SALAMANDER_SFX_LANGUAGE=%1
set SALAMANDER_SFX_CODEPAGE=%2
if exist Release del /f /q Release\*.*
"%MSB%" selfextr.vcxproj /t:rebuild "/p:Configuration=Release" /p:Platform=Win32 /m:%NUMBER_OF_PROCESSORS%
echo.
if exist ReleaseEx del /f /q ReleaseEx\*.*
"%MSB%" selfextr.vcxproj /t:rebuild "/p:Configuration=ReleaseEx" /p:Platform=Win32 /m:%NUMBER_OF_PROCESSORS%
endlocal
%sfxmake_exe% "%output_directory%%3.sfx" "..\..\selfextr\language\%3\texts.txt" %package_version% %package_compatible_version%
exit /b

:ENGLISH

call :MAKE_SFX %1 windows-1252 english
goto END

:CZECH

call :MAKE_SFX %1 windows-1250 czech
goto END

:SLOVAK

call :MAKE_SFX %1 windows-1250 slovak
goto END

:GERMAN

call :MAKE_SFX %1 windows-1252 german
goto END

:HUNGARIAN

call :MAKE_SFX %1 windows-1250 hungarian
goto END

:SPANISH

call :MAKE_SFX %1 windows-1252 spanish
goto END

:ROMANIAN

call :MAKE_SFX %1 windows-1250 romanian
goto END

:RUSSIAN

call :MAKE_SFX %1 windows-1251 russian
goto END

:CHINESESIMPL

call :MAKE_SFX %1 gb2312 chinesesimplified
goto END

:DUTCH

call :MAKE_SFX %1 windows-1252 dutch
goto END

:FRENCH

call :MAKE_SFX %1 windows-1252 french
goto END

:INVALID_LANGUAGE1

echo No language or output directory specified.
echo.
goto USAGE

:INVALID_LANGUAGE2

echo Invalid language specified.
echo.

:USAGE

echo Usage: sfxmake.bat language output_directory
echo.
echo Valid values for the 'language' parameter: ENGLISH_VERSION, etc.
echo
echo Output directory should have trailing backslash.

:END

endlocal

if "%making_all_versions%"=="" echo.
if "%making_all_versions%"=="" pause
