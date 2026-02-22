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

    // Returns TRUE when it is safe to close the program; otherwise FALSE.
    // Offers to save modified data and performs the save when required.
    BOOL QueryClose();

    void ProcessCmdLineParams(char* argv[], int p);

    BOOL OnSave();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL OpenChildWindows();
    void CloseChildWindows();
};

extern CFrameWindow FrameWindow;
