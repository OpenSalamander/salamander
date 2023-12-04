// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// projekt FOLDERS obsahuje tri skupiny modulu
//
// 1) modul precomp.cpp, ktery postavi folders.pch (/Yc"precomp.h")
// 2) moduly vyuzivajici folders.pch (/Yu"precomp.h")
// 3) commony maji vlastni, automaticky generovany WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
