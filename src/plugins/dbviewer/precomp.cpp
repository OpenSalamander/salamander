// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// projekt Salamander obsahuje ctyri skupiny modulu
//
// 1) modul precomp.cpp, ktery postavi salamand.pch (/Yc"precomp.h")
// 2) moduly vyuzivajici salamand.pch (/Yu"precomp.h")
// 3) commony a tasklist.cpp maji vlastni, automaticky generovany
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
