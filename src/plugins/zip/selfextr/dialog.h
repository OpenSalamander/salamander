// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef struct
{
    const char* Title;
    const char* File;
    const char* Error;
    bool Retry;
    bool NoSkip;
} CErrorDlgInfo;

typedef struct
{
    const char* Source;
    const char* SourceAttr;
    const char* Target;
    const char* TargetAttr;
    bool ReadOnly;
} COverwriteDlgInfo;

//sfx dialog
#define WM_USER_STARTEXTRACTING (WM_APP + 1) //user massages
#define WM_USER_REFRESHFILENAME (WM_USER_STARTEXTRACTING + 1)

//progress control
#define PGC_FONT_NORMAL RGB(0, 0, 192) // barvy progressBary
#define PGC_FONT_SELECT RGB(255, 255, 255)
#define PGC_BKGND_NORMAL RGB(255, 255, 255)
#define PGC_BKGND_SELECT RGB(0, 0, 192)
#define PGC_BLACK RGB(0, 0, 0)
#define WM_USER_REFRESHPROGRESS (WM_APP + 1) //user massages
#define WM_USER_INITCONTROL (WM_USER_REFRESHPROGRESS + 1)
#define WM_USER_DESTROY (WM_USER_INITCONTROL + 1)
#define IDT_REPAINT 1 //timer

//web link control
#define PGC_FONT RGB(0, 0, 192)

extern HWND DlgWin;
extern __int64 Progress;
extern __int64 ProgressTotalSize;
extern HWND ProgressWin;
extern HANDLE Thread;
extern WNDPROC OrigTextControlProc;

void CenterDialog(HWND dlg);
int OverwriteDialog(const char* source, const char* sourceAttr, const char* target, const char* targetAttr, DWORD attr);
int ErrorDialog(const char* title, const char* file, const char* error, bool retry, bool noSkip = false);
BOOL WINAPI LongMessageDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
BOOL WINAPI SfxDlgProc(HWND hDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
LRESULT CALLBACK TextControlProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
int StartExtracting();
