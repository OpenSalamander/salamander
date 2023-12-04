// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <CommDlg.h>
#include <ShellAPI.h>
#include <crtdbg.h>
#include <tchar.h>
#include <sys/stat.h>
#include <ostream>
#include <math.h>
#include <limits>
#include <commctrl.h>
#include <limits.h>
#include <vector>
#include <algorithm>
#include <map>
#include <zmouse.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_menu.h"
#include "spl_gui.h"

#include "versinfo.rh2"

#include "dbg.h"
#include "arraylt.h"
#include "winliblt.h"
#include "auxtools.h"

// get rid of some anoying warnings
#pragma warning(disable : 4661)
#pragma warning(disable : 4786)

// ****************************************************************************
//
// Uklid po includovani MS headeru
//
#undef min
#undef max
// ****************************************************************************

#include "lcutils.h"
#include "str.h"
#include "diff.h"

//#include "counter.h"
//#include "profile.h"

#include "filecomp.rh"
#include "filecomp.rh2"
#include "lang\lang.rh"

#include "xunicode.h"
#include "mtxtout.h"
#include "filemap.h"
#include "filecache.h"
#include "textio.h"
#include "worker.h"
#include "cwbase.h"
#include "cwstrict.h"
#include "cwoptim.h"
#include "filecomp.h"
#include "dlg_com.h"
#include "controls.h"
#include "dialogs.h"
#include "viewwnd.h"
#include "viewtext.h"
#include "mainwnd.h"
#include "messages.h"
#include "remotmsg.h"
#include "remote.h"

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))
