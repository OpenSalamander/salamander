// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Windows Portable Devices Plugin for Open Salamander
	
	Copyright (c) 2015 Milan Kase <manison@manison.cz>
	Copyright (c) 2015 Open Salamander Authors
	
	precomp.h
	Header file for standard system header files,
	or project specific include files that are used frequently, but
	are changed infrequently.
*/

#ifdef _MSC_VER
#pragma once
#endif

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501

#include <windows.h>
#include <CommDlg.h>
#include <windowsx.h>
#include <shlwapi.h>
#include <shellapi.h>
#include <shlobj.h>
#include <commctrl.h>
#include <tchar.h>
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream>
#include <new>
#include <stdio.h>
#include <stdlib.h>

#define STRSAFE_NO_DEPRECATE
#include <strsafe.h>

#define _ATL_ALLOW_CHAR_UNSIGNED
#define _ATL_NO_AUTOMATIC_NAMESPACE
#include <atlbase.h>
#include <atlstr.h>
#include <atlsimpcoll.h>
#include <atlsync.h>
#define _WTL_NO_AUTOMATIC_NAMESPACE
#define _WTL_NO_CSTRING
#define _WTL_NO_WTYPES
#include "wtl/atlapp.h"
#include "wtl/atluser.h"

#ifdef _MSC_VER
#include <crtdbg.h>
#ifdef _DEBUG
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif
#define assert _ASSERTE
#else
#include <assert.h>
#endif

#include <PortableDevice.h>
#include <PortableDeviceTypes.h>
#include <PortableDeviceApi.h>
#pragma comment(lib, "PortableDeviceGuids.lib")

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
