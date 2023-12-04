// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <tchar.h>
#include <windows.h>
#include <windowsx.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <limits.h>
#include <process.h>
#include <stdio.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#ifdef _WIN64
#define PICTVIEW_DLL_IN_SEPARATE_PROCESS // x64 verze PictView pouziva 32-bit verzi pvw32cnv.dll pomoci IPC (inter-process communication) se salpvenv.exe
#define ENABLE_WIA                       // x64 verze PictView pouziva ke skenovani WIA 1.0
#else                                    // _WIN64
#define ENABLE_TWAIN32                   // x86 verze PictView pouziva ke skenovani TWAIN 1.x (ktery vnitrne podporuje i WIA)
#endif                                   // _WIN64

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

#include "versinfo.rh2"

#include "pictview.rh"
#include "pictview.rh2"
#include "lang\lang.rh"
#include "spl_com.h"
#include "spl_base.h"
#include "spl_arc.h"
#include "spl_gen.h"
#include "spl_menu.h"
#include "spl_thum.h"
#include "spl_view.h"
#include "spl_gui.h"
#include "spl_vers.h"
#include "dbg.h"
#include "arraylt.h"
#include "winliblt.h"
#include "auxtools.h"
#include "lukas\gdi.h"

//Kdyz mame starou verzi windowsx.h
#ifndef GET_Y_LPARAM
#define GET_Y_LPARAM(x) HIWORD(x)
#define GET_X_LPARAM(x) LOWORD(x)
#endif

#ifndef INT32
#define INT32 int
#define UINT32 unsigned int
#endif

#ifndef SetWindowLongPtr
// compiling on VC6 w/o reasonably new SDK
#define SetWindowLongPtr SetWindowLong
#define GetWindowLongPtr GetWindowLong
#define GWLP_USERDATA GWL_USERDATA
#endif

#define SizeOf(a) sizeof(a) / sizeof(a[0])