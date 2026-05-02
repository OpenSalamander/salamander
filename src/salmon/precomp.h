// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>
#include <shlobj.h>
#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <limits.h>
#include <commctrl.h>
#include <tchar.h>
#include <process.h>

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"
#include "winlib.h"

#include "..\salmoncl.h"

#include "salmon.h"
#include "upload.h"
#include "compress.h"
#include "minidump.h"
#include "dialogs.h"
#include "config.h"

#include "..\lang\lang.rh"
#include "..\texts.rh2"
