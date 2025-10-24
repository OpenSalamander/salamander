// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

// The Salamander project contains four groups of modules
//
// 1) module precomp.cpp which builds salamand.pch (/Yc"precomp.h")
// 2) modules using salamand.pch (/Yu"precomp.h")
// 3) shelext.c which does not use precompiled headers
// 4) common files and tasklist.cpp have their own automatically generated
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")

/* FIXME_X64 - go through printf specifiers with regard to x64
Petr:  ok, that sounds rather nasty ... it might be enough to check %d, %x, %X, %u, %i, %o ... the same madness
preferably with a regex that covers everything in between (width specification, etc.) ... so it also finds %08X, etc.
this will be a mess, a few days of digging through it
----
http://msdn.microsoft.com/en-us/library/tcxf1dw6%28v=vs.71%29.aspx
*/