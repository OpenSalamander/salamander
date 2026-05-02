@echo off
for /F "tokens=1 delims=" %%A in ('php version.php') do set VERSION=%%A
php translate.php translate.salamander.open.ini orig .. %VERSION%
