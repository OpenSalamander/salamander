@echo off
rem usage:
rem 	vdiff fullname1 fullname2
rem
rem switch to Open Salamander main directory, to let windows find salrtl.dll
cd ..\..
start rundll32 plugins\filecomp\filecomp.spl,RemoteCompareFiles %1 %2 
