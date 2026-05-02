// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifdef PICTVIEW_DLL_IN_SEPARATE_PROCESS

#include "pictview.h"

BOOL InitPVEXEWrapper(HWND hParentWnd, LPCTSTR pPluginFolder);
void ReleasePVEXEWrapper();

#endif
