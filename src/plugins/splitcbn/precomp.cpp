// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// projekt SPLITCBN obsahuje tri skupiny modulu
//
// 1) modul precomp.cpp, ktery postavi splitcbn.pch (/Yc"precomp.h")
// 2) moduly vyuzivajici splitcbn.pch (/Yu"precomp.h")
// 3) commony nepouzivaji precompiled headers
