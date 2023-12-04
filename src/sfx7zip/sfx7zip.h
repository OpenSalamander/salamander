// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WM_USER_THREADEXIT (WM_USER + 1000)
#define WM_USER_UPDATEPROGRESS (WM_USER + 1001)
#define WM_USER_UPDATEFNAME (WM_USER + 1002)

#define OPEN_VERSION

#ifdef OPEN_VERSION
#define FOR_SALAMANDER_SETUP
#endif // OPEN_VERSION
