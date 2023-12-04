// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#pragma once

#ifndef OPENSAL_VERSION // verze kopilovana pro distribuci se Salamanderem nebude zpetne kompatibilni s verzi 5.0 (zbytecna komplikace)
#define SALSDK_COMPATIBLE_WITH_VER 103
#define REQUIRE_COMPATIBLE_SAL_VERSION "This plugin requires Open Salamander 5.0 or later."
#endif // OPENSAL_VERSION

//#define TIMINGTEST

#define SALAMANDER
//#define TRACE_ENABLE
#define WM_APP_ICONLOADED (WM_APP + 1)
#define WM_APP_DIRPOPULATED (WM_APP + 2) //wParam = 0; lParam = CZRoot
#define WM_APP_LOGUPDATED (WM_APP + 3)   //wParam = count; lParam = logger

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers
// Windows Header Files:
#include <windows.h>
#include <CommDlg.h>
#include <ShellAPI.h>
#include <shlobj.h>
#include <Windowsx.h> //pomocna makra

// C RunTime Header Files
#include <malloc.h>
#include <tchar.h>

// ???
#include <crtdbg.h>
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream>
#include <stdio.h>
#include <time.h>

// Salamander headers
#include "versinfo.rh2"

#include "spl_com.h"
//#include "spl_arc.h"
#include "spl_base.h"
#include "spl_gen.h"
//#include "spl_fs.h"
#include "spl_menu.h"
//#include "spl_thum.h"
//#include "spl_view.h"
#include "spl_vers.h"
#include "spl_gui.h"

#include "dbg.h"
#include "auxtools.h"
//#include "mhandles.h"

//#include "DiskMapPlugin.rh"
//#include "arraylt.h"
//#include "winliblt.h"
//#include "dialogs.h"
//#include "demoplug.h"
//#include "demoplug.rh"

// TODO: reference additional headers your program requires here

#ifndef ARRAYSIZE
#define RTL_NUMBER_OF_V1(A) (sizeof(A) / sizeof((A)[0]))
#define RTL_NUMBER_OF_V2(A) RTL_NUMBER_OF_V1(A)
#define ARRAYSIZE(A) RTL_NUMBER_OF_V2(A)
#endif // ARRAYSIZE

#include "../DiskMap/Resources/DiskMap.rh2"
