// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// projekt Salamander obsahuje ctyri skupiny modulu
//
// 1) modul precomp.cpp, ktery postavi salamand.pch (/Yc"precomp.h")
// 2) moduly vyuzivajici salamand.pch (/Yu"precomp.h")
// 3) shelext.c, ktery nepouziva predkompilovane headry
// 4) commony a tasklist.cpp maji vlastni, automaticky generovany
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")

/* FIXME_X64 - projit printf specifikatory s ohledem na x64
Petr:  ok, to zni celkem osklive ... asi by stacilo projit %d, %x, %X, %u, %i, %o ... stejne sileny
nejlepe s regularem co muze byt vsechno mezi (specifikace sirky, atd.) ... aby to naslo i %08X, atd.
to bude hnuj na par dni prohrabovani
----
http://msdn.microsoft.com/en-us/library/tcxf1dw6%28v=vs.71%29.aspx
*/