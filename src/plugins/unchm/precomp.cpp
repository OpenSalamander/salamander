// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// The DEMOPLUG project contains three groups of modules
//
// 1) the precomp.cpp module, which builds demoplug.pch (/Yc"precomp.h")
// 2) modules using demoplug.pch (/Yu"precomp.h")
// 3) common files have their own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
