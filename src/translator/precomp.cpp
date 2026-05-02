// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// the ALTAPDB project contains three groups of modules
//
// 1) precomp.cpp builds altapdb.pch (/Yc"precomp.h")
// 2) modules that use altapdb.pch (/Yu"precomp.h")
// 3) commons and tasklist.cpp have their own automatically generated
//    WINDOWS.PCH (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
