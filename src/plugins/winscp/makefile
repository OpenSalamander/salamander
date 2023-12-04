#------------------------------------------------------------------------------
!ifndef ROOT
ROOT = $(MAKEDIR)\..
!endif
#------------------------------------------------------------------------------
EXTERNALDEFINES = NO_FILEZILLA;
#------------------------------------------------------------------------------
MAKEBASE = $(ROOT)\bin\make.exe -$(MAKEFLAGS)
MAKE = $(MAKEBASE) -DEXTERNALDEFINES=$(EXTERNALDEFINES) -f$&.mak
BPR2MAK = $(ROOT)\bin\bpr2mak
#------------------------------------------------------------------------------
default: build

build: winscp.spl lang translations

clean: cleaninterm cleantarget

cleantarget:
 del winscp.spl
 del /q lang\*.*

cleaninterm:
 del Salamand.drc
 del Salamand.lib
 del Salamand.map
 del Salamand.tds
 del *.mak
 del *.obj
 del *.res
 del packages\*.mak
 del packages\*.obj
 del packages\my\*.dcu
 del packages\my\*.obj
 del packages\my\*.hpp
 del packages\theme\*.dcu
 del packages\theme\*.obj
 del packages\theme\*.hpp
 del lib\*.lib
 del lib\*.bpi
 del lib\*.bpl
 del lib\*.tds
 del core\*.obj
 del forms\*.obj
 del windows\*.obj
 del putty\*.obj
 del putty\charset\*.obj
 del putty\windows\*.obj
 del salamander\*.obj
 del salamander\*.res
 del Salamand_DRC.rc
 del lang\*.tds
 del ..\shared\*.obj
 del makefile.lang
 del Salamand.lang.rc
 del Salamand.lang.cpp
 rd /s /q translations\lang

winscp.spl: salamand.spl
 if exist winscp.spl del winscp.spl
 ren salamand.spl winscp.spl

salamand.spl: salamand.bpr
salamand.spl: lib\Moje_B5.lib lib\ThemeManagerC6.lib
salamand.spl: lib\Putty.lib lib\ScpCore.lib lib\SalamandForms.lib
 $(BPR2MAK) -tdefault.bmk salamand.bpr
 $(MAKE)

{packages}.bpk{lib}.lib:
 cd packages
 $(BPR2MAK) -t..\default.bmk $.
 $(MAKE)
 cd ..

{}.bpk{lib}.lib:
 $(BPR2MAK) -tdefault.bmk $.
 $(MAKE)

{}.bpr{lib}.lib:
 $(BPR2MAK) -tlib.bmk $.
 $(MAKE)

lang:
 copy Salamand.drc Salamand_DRC.rc
 cd translations
 generate.bat
 cd ..
 $(MAKEBASE) -fmakefile.lang

translations:
 cd translations
 mkdir lang
 translate.bat orig
 translate.bat CS
 translate.bat DE
 translate.bat ES
 translate.bat HU
 translate.bat SK
 translate.bat FR
 translate.bat RU
 translate.bat NL
 translate.bat CHS
 cd ..
