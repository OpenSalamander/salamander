@echo off

for /F "tokens=1 delims=" %%A in ('php version.php') do set VERSION=%%A

echo ---
echo %1 %VERSION%
php translate.php translate.salamander.open.ini %1 "" %VERSION%
