// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <CommDlg.h>
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#include "versinfo.rh2"
#include "spl_com.h"
#include "spl_base.h"
#include "spl_gen.h"
#include "spl_arc.h"
#include "spl_vers.h"
#include "spl_file.h"
#include "dbg.h"
