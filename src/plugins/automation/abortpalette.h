// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Automation Plugin for Open Salamander
	
	Copyright (c) 2009-2023 Milan Kase <manison@manison.cz>
	Copyright (c) 2010-2023 Open Salamander Authors
	
	abortpalette.h
	Palette window with toolbar button to abort running script.
*/

#pragma once

/// Script abort palette window.
class CScriptAbortPaletteWindow : public CWindow
{
private:
    CGUIToolBarAbstract* m_pToolBar;
    class CScriptInfo* m_pScriptInfo;
    int m_cInitialCountDown;

    enum
    {
        TimerCheckForegroundApp = 1,
        PeriodCheckForegroundApp = 250, // we are calling WindowBelongsToProcessID() every time
        InitialDelay = 1000,
    };

    void OnCreate();
    void OnDestroy();
    void OnAbortBtnClick();

    /// Checks whether Salamander is currently foreground application
    /// and shows or hides the palette window appropriately.
    void CheckForegroundApp();

    /// Finds ideal placement for the palette window.
    /// \return Returns suggested placement in the screen coordinates.
    POINT FindPlacement(int cx, int cy);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

public:
    enum
    {
        WM_USER_CLOSEPALETTE = WM_USER + 0x1C,
        WM_USER_SETTINGCHANGE = WM_USER + 0x1D,
        ToolCmdAbort = 1,
    };

    CScriptAbortPaletteWindow(class CScriptInfo* pScriptInfo);
    ~CScriptAbortPaletteWindow();
};

class CScriptAbortPaletteThread : public CThread
{
private:
    CScriptAbortPaletteWindow* m_pPalette;
    HANDLE m_hWindowCreatedEvt;
    class CScriptInfo* m_pScriptInfo;

public:
    CScriptAbortPaletteThread(class CScriptInfo* pScriptInfo);
    ~CScriptAbortPaletteThread();

    virtual unsigned Body();

    void ClosePalette();

    /// Returns handle of the palette window.
    HWND GetHwnd() const
    {
        _ASSERTE(m_pPalette != NULL);
        return m_pPalette->HWindow;
    }

    /// Waits until the palette window gets created
    /// and is ready to receive messages.
    void WaitWindowReady()
    {
        _ASSERTE(m_hWindowCreatedEvt != NULL);
        WaitForSingleObject(m_hWindowCreatedEvt, INFINITE);
    }
};

/// Script abort palette window running its own thread.
class CScriptAbortPalette
{
private:
    CThreadQueue m_oThreadQueue;
    CScriptAbortPaletteThread* m_pThread;

public:
    CScriptAbortPalette(class CScriptInfo* pScriptInfo);
    ~CScriptAbortPalette();

    HWND GetHwnd() const
    {
        _ASSERTE(m_pThread != NULL);
        return m_pThread->GetHwnd();
    }
};
