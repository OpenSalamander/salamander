// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* ERROR_TITLE;

extern const char* MDICHILD_CLASSNAME;

#define TREE_TYPE_NONE 0x00000000
#define TREE_TYPE_STRING 0x00010000
#define TREE_TYPE_DIALOG 0x00020000
#define TREE_TYPE_MENU 0x00030000

#define TREE_TYPE_NONE_STRINGS 0x00000001
#define TREE_TYPE_NONE_DIALOGS 0x00000002
#define TREE_TYPE_NONE_MENUS 0x00000003

// vrati hande okna,ktery se pouzije jako parent pro messagebox
HWND GetMsgParent();

// nakopiruje retezec na clipboard; textLen muze byt -1 pro omereni delky retezce
BOOL CopyTextToClipboard(const char* text, int textLen);

#define WM_AUTORELOAD WM_APP + 100
#define WM_POSTPAINT WM_APP + 101
#define WM_VALIDATIONFAILED WM_APP + 102
#define WM_FOCLASTINOUTPUTWND WM_APP + 103
#define WM_UNTRANSLATEDFOUND WM_APP + 104

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))
