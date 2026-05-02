// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WIN32_LEAN_AND_MEAN // exclude rarely-used stuff from Windows headers

#include <windows.h>

#include "killdbg.h"
#include "fcremote.h"
#include "messages.h"
#include "../remotmsg.h"
#include "../filecomp.rh"
#include "../filecomp.rh2"
#include "../lang/lang.rh"

#ifndef _DEBUG
#define memcpy my_memcpy
void my_memcpy(void* dst, const void* src, int len);
#endif // _DEBUG
