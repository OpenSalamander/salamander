// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// the Renamer project contains four groups of modules
//
// 1) the precomp.cpp module, which builds renamer.pch (/Yc"precomp.h")
// 2) modules that use renamer.pch (/Yu"precomp.h")
// 3) the common files have their own automatically generated
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
