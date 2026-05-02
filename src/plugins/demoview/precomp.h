// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <CommDlg.h>
#include <ShellAPI.h>
#include <shlobj.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif // _MSC_VER
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream>
#include <stdio.h>
#include <time.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_menu.h"
#include "spl_thum.h"
#include "spl_view.h"
#include "spl_vers.h"
#include "spl_gui.h"

#include "dbg.h"
#include "mhandles.h"
#include "arraylt.h"
#include "winliblt.h"
#include "auxtools.h"
#include "dialogs.h"
#include "demoview.h"
#include "demoview.rh"
#include "demoview.rh2"
#include "lang\lang.rh"

#ifdef __BORLANDC__
#define min(a, b) (((a) < (b)) ? (a) : (b))
#define max(a, b) (((a) > (b)) ? (a) : (b))
#endif // __BORLANDC__
