// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* FRAMEWINDOW_NAME;

//*****************************************************************************
//
// CFrameWindow
//

class CFrameWindow : public CWindow
{
public:
    HWND HMDIClient;

    BOOL LayoutEditorOpened;

    HWND HPredLayoutActiveWnd;

public:
    CFrameWindow();

    BOOL OpenProject(const char* importSubPath = NULL);
    BOOL IsProjectOpened();

    void EnableNonLayoutWindows(BOOL enable);
    BOOL OpenLayoutEditor();
    BOOL CloseLayoutEditor();

    void SetTitle();

    // pokud je mozne program zavrit, vrati TRUE; jinak vrati FALSE
    // jsou-li zmenena data, nabidne ulozeni a provede ho
    BOOL QueryClose();

    void ProcessCmdLineParams(char* argv[], int p);

    BOOL OnSave();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL OpenChildWindows();
    void CloseChildWindows();
};

extern CFrameWindow FrameWindow;
