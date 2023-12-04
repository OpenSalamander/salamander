// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <tchar.h>
#include <commctrl.h>
#include <shlobj.h>
#include <crtdbg.h>
#include <limits.h>
#include <process.h>
#include <commctrl.h>
#include <ostream> // pro verzi 2.5 beta 7 a novejsi
#include <stdio.h>
#include <time.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "versinfo.rh2"

#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_fs.h"
#include "spl_menu.h"
#include "spl_gui.h"
#include "spl_vers.h"
#include "spl_zlib.h"

#include "dbg.h"
#include "mhandles.h"
#include "arraylt.h"
#include "winliblt.h"
#include "auxtools.h"
#include "ftp.rh"
#include "ftp.rh2"
#include "lang\lang.rh"
#include "ftputils.h"
#include "ftp.h"
#include "dialogs.h"
#include "ssl.h"
#include "sockets.h"
#include "ctrlcon.h"
#include "parser.h"
#include "operats.h"
#include "datacon.h"
