// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// the FOLDERS project contains three module groups
//
// 1) the precomp.cpp module, which builds folders.pch (/Yc"precomp.h")
// 2) modules using folders.pch (/Yu"precomp.h")
// 3) commons have their own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
