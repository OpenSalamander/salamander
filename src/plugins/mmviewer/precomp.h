// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <tchar.h>
#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <commctrl.h>
#include <limits.h>
#include <stdio.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_arc.h"
#include "spl_gen.h"
#include "spl_menu.h"
#include "spl_view.h"
#include "spl_gui.h"
#include "spl_vers.h"
#include "dbg.h"
#include "winliblt.h"
#include "mhandles.h"
#include "arraylt.h"

#include <assert.h>

// select here which formats you want the plugin to support
// strings in the stringlist will always remain all of them (insufficient preprocessor support from MS)

//#define _MP4_SUPPORT_ - not implemented yet
#define _MPG_SUPPORT_
#define _MOD_SUPPORT_
#define _VQF_SUPPORT_
#define _WAV_SUPPORT_
#define _WMA_SUPPORT_
// FIXME - I hammered down OGG support because ogglib is a terrible behemoth (the library now has over 2MB)
// and the old version we used does not have x64 support; the solution is to move to the TagLib project
// http://taglib.github.com/ which is tiny and will allow us to support additional formats
// we need to examine its MPL license, but it is apparently more permissive than LGPL and the library could be compiled in
#if ((_MSC_VER < 1600) && !defined(_WIN64)) // only VC2008 and x87
//#define _OGG_SUPPORT_
#endif
