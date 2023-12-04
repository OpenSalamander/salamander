// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// For MSVC8 (VS2005), to turn off "deprecated" warning on all string functions
#pragma warning(disable : 4996)
//#define _CRT_SECURE_NO_DEPRECATE 1
//#define _CRT_NON_CONFORMING_SWPRINTFS 1

//#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <shlobj.h>
#include <crtdbg.h>
#include <limits.h>
#include <commctrl.h>
#include <ostream>
#include <stdio.h>
#include <winioctl.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include <uxtheme.h>

#include "../versinfo.rh2"
#include "spl_com.h"
#include "spl_base.h"
#include "spl_arc.h"
#include "spl_gen.h"
#include "spl_fs.h"
#include "spl_menu.h"
#include "spl_gui.h"
#include "spl_file.h"
#include "spl_vers.h"
#include "dbg.h"
#include "mhandles.h"

#include "arraylt.h"

#include "winliblt.h"
#include "auxtools.h"

// old SDKs support
#ifndef INVALID_FILE_ATTRIBUTES
#define INVALID_FILE_ATTRIBUTES (DWORD(-1))
#endif

typedef unsigned __int64 QWORD;
#define HIDWORD(x) ((DWORD)((x) >> 32))
#define LODWORD(x) ((DWORD)(x & 0xffffffff))
#define PHIDWORD(x) (((DWORD*)&(x)) + 1)
#define PLODWORD(x) ((DWORD*)&(x))
#define MAKEQWORD(lo, hi) (((QWORD)(hi) << 32) | (lo))
