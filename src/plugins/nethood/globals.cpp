// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "nethood.h"
#include "nethoodfs.h"
#include "cache.h"
#include "icons.h"
#include "nethoodmenu.h"
#include "globals.h"

CSalamanderGeneralAbstract* SalamanderGeneral;
CSalamanderDebugAbstract* SalamanderDebug;
int SalamanderVersion;
CSalamanderGUIAbstract* SalamanderGUI;
CNethoodPluginInterface g_oNethoodPlugin;
CNethoodPluginInterfaceForFS g_oNethoodFS;
CNethoodCache g_oNethoodCache;
TCHAR g_szAssignedFSName[MAX_PATH];
size_t g_cchAssignedFSName;
HINSTANCE g_hInstance;
HINSTANCE g_hLangInstance;
const CFileData** g_transferFileData;
int* g_transferIsDir;
char* g_transferBuffer;
int* g_transferLen;
DWORD* g_transferRowData;
CPluginDataInterfaceAbstract** g_transferPluginDataIface;
DWORD* g_transferActCustomData;
CNethoodIcons g_oIcons;
CNethoodPluginInterfaceForMenuExt g_oMenuExt;

TCHAR g_aszRedirectPath[2][MAX_PATH];

DWORD_PTR g_adwPostedThrobberQueue[POSTED_THROBBER_QUEUE_LEN];
int g_iPostedThrobberQueue;

TCHAR g_aszFocusShareName[2][MAX_SHARE_NAME];
int g_iFocusSharePanel;
