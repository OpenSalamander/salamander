// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct CPreviewInitData
{
    CSfxSettings* Settings;
    char* About;
    char* AboutButton1;
    char* AboutButton2;
    HICON LargeIcon;
    HICON SmallIcon;
    HINSTANCE SfxHInstance;
};

#define WM_USER_SETHCURSOR (WM_APP + 1)

INT_PTR WINAPI SfxPreviewDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
