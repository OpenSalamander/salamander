// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// The CHECKSUM project contains three groups of modules
//
// 1) the precomp.cpp module, which builds ftp.pch (/Yc"precomp.h")
// 2) modules that use checksum.pch (/Yu"precomp.h")
// 3) the common modules have their own automatically generated WINDOWS.PCH
//    (/YX"windows.h" /Fp"$(OutDir)\WINDOWS.PCH")
