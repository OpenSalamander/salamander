// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <windows.h>
#include <crtdbg.h>
#include <limits.h>
#include <stdio.h>
#include <ostream>
#include <commctrl.h> // potrebuju LPCOLORMAP
#include <aclapi.h>
#include <process.h>
#include <math.h>
#include <tchar.h>

//#if defined(_DEBUG) && defined(_MSC_VER)  // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
//#define new new(_NORMAL_BLOCK, __FILE__, __LINE__)
//#endif
