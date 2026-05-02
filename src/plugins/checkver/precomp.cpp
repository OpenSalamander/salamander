// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// the Salamander project contains four groups of modules
//
// 1) the precomp.cpp module, which builds salamand.pch (/Yc"precomp.h")
// 2) modules using salamand.pch (/Yu"precomp.h")
// 3) common files and tasklist.cpp have their own automatically generated
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
