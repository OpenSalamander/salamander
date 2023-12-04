# ---------------------------------------------------------------------------
PROJECT = Salamand.cs
BCB = $(MAKEDIR)\..
OBJFILES = Salamand.obj
RESFILES = Salamand_DRC.res Salamand.res salamander\Salamander.res
# ---------------------------------------------------------------------------
LFLAGS = -D"" -aa -Tpd -x -Gn -b:0x38900000
INCLUDE = $(BCB)\include;$(BCB)\include\mfc
# ---------------------------------------------------------------------------
ALLOBJ = $(OBJFILES) c0d32.obj
ALLLIB = import32.lib cw32mt.lib
# ---------------------------------------------------------------------------
$(PROJECT): $(RESFILES) $(OBJFILES)
 $(MAKEDIR)\ilink32 @&&!
 $(LFLAGS) $(ALLOBJ), $(PROJECT),, $(ALLLIB),, $(RESFILES)
!
# ---------------------------------------------------------------------------
.cpp.obj:
 $(MAKEDIR)\bcc32 -c -n$(@D) {$< }

.rc.res:
 $(MAKEDIR)\brcc32 -c1250 -fo$@ -I$(INCLUDE) $<
# ---------------------------------------------------------------------------