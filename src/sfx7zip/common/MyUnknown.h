// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// MyUnknown.h

#pragma once

#ifdef _WIN32

#ifdef _WIN32_WCE
#if (_WIN32_WCE > 300)
#include <basetyps.h>
#else
#define MIDL_INTERFACE(x) struct
#endif
#else
#include <basetyps.h>
#endif

#include <unknwn.h>

#else
#include "MyWindows.h"
#endif
