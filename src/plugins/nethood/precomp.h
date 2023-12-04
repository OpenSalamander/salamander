// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...

	Header file for standard system header files,
	or project specific include files that are used frequently, but
	are changed infrequently.
*/

#ifdef _MSC_VER
#pragma once
#endif

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <winnetwk.h>
#include <tchar.h>
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <lm.h>
#include <wtsapi32.h>

#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#ifdef _MSC_VER
#include <crtdbg.h>
#ifdef _DEBUG
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#define assert _ASSERTE
#else
#include <assert.h>
#endif

#include <spl_com.h>
#include <spl_base.h>
#include <spl_arc.h>
#include <spl_gen.h>
#include <spl_fs.h>
#include <spl_menu.h>
#include <spl_thum.h>
#include <spl_view.h>
#include <spl_gui.h>

#include "versinfo.rh2"

#pragma warning(push)
#pragma warning(disable : 4267) // possible loss of data
#include <dbg.h>
#pragma warning(pop)
#include <mhandles.h>
#include <arraylt.h>
#include <winliblt.h>
#include <auxtools.h>

// Define some SAL strings for VC6.
#ifndef __in
#define __in
#endif

#ifndef __out
#define __out
#endif

#ifndef __inout
#define __inout
#endif

#ifndef __in_opt
#define __in_opt
#endif

#ifndef __out_opt
#define __out_opt
#endif

#ifndef __out_ecount
#define __out_ecount(n)
#endif

#ifndef __out_bcount
#define __out_bcount(n)
#endif

#ifndef __inout_ecount
#define __inout_ecount(n)
#endif

#ifndef __out_ecount_opt
#define __out_ecount_opt(n)
#endif

// VC2005+ has its native definition of count-of in stdlib.h
#ifdef _countof
#define COUNTOF _countof
#else
#define COUNTOF(a) (sizeof(a) / sizeof((a)[0]))
#endif

// Macro to stringize the parameter.
#ifndef _STRINGIZE
#define _STRINGIZE(s) #s
#endif
#define STRINGIZE(s) _STRINGIZE(s)

// VC2008 removed following global variables, so we define
// and initialize them ourselves.
extern unsigned int _osplatform;
extern unsigned int _winmajor;
extern unsigned int _winminor;

#ifndef HWND_MESSAGE
// Not defined in old headers.
#define HWND_MESSAGE ((HWND)-3)
#endif

#ifndef SM_REMOTESESSION
#define SM_REMOTESESSION 0x1000
#endif

#ifndef WM_WTSSESSION_CHANGE
#define WM_WTSSESSION_CHANGE 0x02B1
#endif

#include "salutils.h"
