// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// the SPLITCBN project contains three groups of modules
//
// 1) module precomp.cpp, which builds splitcbn.pch (/Yc"precomp.h")
// 2) modules using splitcbn.pch (/Yu"precomp.h")
// 3) commons that do not use precompiled headers
