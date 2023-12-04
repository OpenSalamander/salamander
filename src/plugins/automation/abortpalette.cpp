// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	abortpalette.cpp
	Palette window with toolbar button to abort running script.
*/

#include "precomp.h"
#include "abortpalette.h"
#include "automationplug.h"
#include "scriptlist.h"
#include "processlist.h"
#include "lang\lang.rh"

extern CSalamanderGeneralAbstract* SalamanderGeneral;
extern CSalamanderGUIAbstract* SalamanderGUI;
extern HINSTANCE g_hInstance;
extern HINSTANCE g_hLangInst;
extern CAutomationPluginInterface g_oAutomationPlugin;
extern CWindowQueue AbortPaletteWindowQueue;

CScriptAbortPaletteWindow::CScriptAbortPaletteWindow(CScriptInfo* pScriptInfo)
{
    m_pToolBar = NULL;
    m_pScriptInfo = pScriptInfo;
    m_cInitialCountDown = InitialDelay / PeriodCheckForegroundApp;
}

CScriptAbortPaletteWindow::~CScriptAbortPaletteWindow()
{
}

LRESULT CScriptAbortPaletteWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        OnCreate();
        break;

    case WM_DESTROY:
        OnDestroy();
        break;

    case WM_ERASEBKGND:
        return TRUE;

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        lstrcpy(tt->Buffer, SalamanderGeneral->LoadStr(g_hLangInst, IDS_ABORTTIP));
        SalamanderGUI->PrepareToolTipText(tt->Buffer, FALSE);
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (m_pToolBar != NULL)
            m_pToolBar->SetFont();
        return 0;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == ToolCmdAbort)
        {
            OnAbortBtnClick();
            return 0;
        }

        break;
    }

    case WM_USER_CLOSEPALETTE:
        DestroyWindow(HWindow);
        return 0;

    case WM_TIMER:
        if (wParam == TimerCheckForegroundApp)
        {
            if (m_cInitialCountDown > 0)
            {
                --m_cInitialCountDown;
            }
            CheckForegroundApp();
            return 0;
        }
        break;
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

void CScriptAbortPaletteWindow::OnCreate()
{
    RECT rc;
    int cx, cy;
    TLBI_ITEM_INFO2 tii = {
        0,
    };
    POINT pt;

    m_pToolBar = SalamanderGUI->CreateToolBar(HWindow);
    m_pToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);
    m_pToolBar->CreateWnd(HWindow);
    m_pToolBar->SetImageList(g_oAutomationPlugin.GetImageList(false));
    m_pToolBar->SetHotImageList(g_oAutomationPlugin.GetImageList(true));

    tii.Mask = TLBI_MASK_ID | TLBI_MASK_TEXT | TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX;
    tii.ID = ToolCmdAbort;
    tii.Style = TLBI_STYLE_SHOWTEXT;
    tii.Text = SalamanderGeneral->LoadStr(g_hLangInst, IDS_ABORTCAPTION);
    tii.ImageIndex = PluginIconStop;
    m_pToolBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);

    cx = m_pToolBar->GetNeededWidth();
    cy = m_pToolBar->GetNeededHeight();

    rc.left = 0;
    rc.top = 0;
    rc.right = cx;
    rc.bottom = cy;
    AdjustWindowRectEx(&rc, GetWindowLong(HWindow, GWL_STYLE), FALSE, // FIXME_X64 - nahradit GetWindowLong -> GetWindowLongPtr (napric celym balikem Salamandera); tento warinig odstrani Honza Rysavy, prosim zachovat
                       GetWindowLong(HWindow, GWL_EXSTYLE));

    SetWindowPos(m_pToolBar->GetHWND(), NULL, 0, 0, cx, cy,
                 SWP_SHOWWINDOW | SWP_NOACTIVATE | SWP_NOZORDER);

    cx = rc.right - rc.left;
    cy = rc.bottom - rc.top;
    pt = FindPlacement(cx, cy);
    SetWindowPos(HWindow, NULL, pt.x, pt.y, cx, cy,
                 SWP_NOZORDER | SWP_NOACTIVATE);

    // Initialize latch status for the Ctrl+Break
    GetAsyncKeyState(VK_CANCEL);

    SetTimer(HWindow, TimerCheckForegroundApp, PeriodCheckForegroundApp, NULL);

    AbortPaletteWindowQueue.Add(new CWindowQueueItem(HWindow));
}

void CScriptAbortPaletteWindow::OnDestroy()
{
    KillTimer(HWindow, TimerCheckForegroundApp);

    AbortPaletteWindowQueue.Remove(HWindow);

    SalamanderGUI->DestroyToolBar(m_pToolBar);
    m_pToolBar = NULL;

    // Quit the thread.
    PostQuitMessage(0);
}

void CScriptAbortPaletteWindow::OnAbortBtnClick()
{
    m_pScriptInfo->AbortScript();
}

void CScriptAbortPaletteWindow::CheckForegroundApp()
{
    HWND hWndFg;
    bool bIsSalamanderFg;
    bool bIsVisible;

    hWndFg = GetForegroundWindow();
    if (hWndFg != NULL)
    {
        bIsSalamanderFg = !!WindowBelongsToProcessID(hWndFg, GetCurrentProcessId());
    }
    else
    {
        bIsSalamanderFg = false;
    }

    // Handle the Ctrl+Break shortcut
    if (bIsSalamanderFg)
    {
        if (GetAsyncKeyState(VK_CANCEL) & 0x8001)
        {
            // pump the message out of the queue
            MSG msg;
            while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
            {
            }

            OnAbortBtnClick();
        }
    }

#if 1
    bIsVisible = !!IsWindowVisible(HWindow);
    if (bIsVisible && !bIsSalamanderFg)
    {
        ShowWindow(HWindow, SW_HIDE);
    }
    else if (!bIsVisible && bIsSalamanderFg && m_cInitialCountDown == 0)
    {
        ShowWindow(HWindow, SW_SHOWNOACTIVATE);
    }
#endif
}

POINT CScriptAbortPaletteWindow::FindPlacement(int cx, int cy)
{
    RECT rcMain;
    int cxBorder;
    RECT rcPalette;

    GetWindowRect(SalamanderGeneral->GetMainWindowHWND(), &rcMain);

    // Try to position the palette to the upper right corner of
    // the main window where there's unlikely to obscure menu bar
    // or toolbars.

    cxBorder = GetSystemMetrics(SM_CXFRAME);
    rcPalette.left = rcMain.right - cx - cxBorder - 8;
    rcPalette.top = rcMain.top + GetSystemMetrics(SM_CYCAPTION) + 8;
    rcPalette.right = rcPalette.left + cx;
    rcPalette.bottom = rcPalette.top + cy;

    SalamanderGeneral->MultiMonEnsureRectVisible(&rcPalette, FALSE);

    return *(POINT*)&rcPalette;
}

////////////////////////////////////////////////////////////////////////////////

CScriptAbortPaletteThread::CScriptAbortPaletteThread(CScriptInfo* pScriptInfo)
{
    m_pPalette = NULL;
    m_hWindowCreatedEvt = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    m_pScriptInfo = pScriptInfo;
    StringCchCopy(Name, _countof(Name), _T("AutomationAbortPalette"));
}

CScriptAbortPaletteThread::~CScriptAbortPaletteThread()
{
    _ASSERTE(m_hWindowCreatedEvt != NULL);
    HANDLES(CloseHandle(m_hWindowCreatedEvt));

    _ASSERTE(m_pPalette == NULL);
}

unsigned CScriptAbortPaletteThread::Body()
{
    MSG msg;
    HWND hwndPalette;

    // Assign slightly higher priority so the abort
    // toolbar remains responsive.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // WS_SYSMENU is required on Vista+ with Aero active, otherwise
    // window manager draws thick and awful border around a tool window.
    m_pPalette = new CScriptAbortPaletteWindow(m_pScriptInfo);
    hwndPalette = m_pPalette->CreateEx(
        WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        CWINDOW_CLASSNAME2,
        SalamanderGeneral->LoadStr(g_hLangInst, IDS_PLUGINNAME),
        WS_OVERLAPPED | WS_CAPTION | WS_CLIPCHILDREN | WS_SYSMENU,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        NULL,
        NULL,
        g_hInstance,
        m_pPalette);

    _ASSERTE(hwndPalette != NULL);

    // Let everyone know that the palette window is now ready.
    SetEvent(m_hWindowCreatedEvt);

    // Remove unneccessary items from the system menu.
    HMENU hSysMenu = GetSystemMenu(hwndPalette, FALSE);
    if (hSysMenu != NULL)
    {
        int cItems = GetMenuItemCount(hSysMenu);
        for (int i = 0; i < cItems; i++)
        {
            UINT uId = GetMenuItemID(hSysMenu, i);
            if (uId == SC_CLOSE)
            {
                // We cannot delete the SC_CLOSE menu item,
                // since that would not disable the close button
                // on the title bar on Vista+ with Aero.
                // Moreover the item cannot be referenced by
                // position but by command only.
                EnableMenuItem(hSysMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);
            }
            else if (uId != SC_MOVE)
            {
                DeleteMenu(hSysMenu, i, MF_BYPOSITION);
                --i;
                --cItems;
            }
        }
    }

    while (GetMessage(&msg, NULL, 0, 0))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    m_pPalette = NULL;

    return 0;
}

void CScriptAbortPaletteThread::ClosePalette()
{
    WaitWindowReady();
    PostMessage(m_pPalette->HWindow, CScriptAbortPaletteWindow::WM_USER_CLOSEPALETTE, 0, 0);
}

////////////////////////////////////////////////////////////////////////////////

CScriptAbortPalette::CScriptAbortPalette(CScriptInfo* pScriptInfo) : m_oThreadQueue("Automation Abort Palette Window")
{
    m_pThread = new CScriptAbortPaletteThread(pScriptInfo);
    m_pThread->Create(m_oThreadQueue);
    m_pThread->WaitWindowReady();
}

CScriptAbortPalette::~CScriptAbortPalette()
{
    HANDLE h = m_pThread->GetHandle();
    m_pThread->ClosePalette();
    m_oThreadQueue.WaitForExit(h);
}
