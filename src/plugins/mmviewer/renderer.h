// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define WM_ENSUREVISIBLE WM_APP

//****************************************************************************
//
// CRendererWindow
//

class CViewerWindow;

class CRendererWindow : public CWindow
{
public:
    COutput Output; // interface for working with the open database
    CViewerWindow* Viewer;
    char FileName[MAX_PATH]; // name of the currently opened file; 0 if none is open

    BOOL Creating; // the window is being created -- do not erase the background yet

    SIZE sLeft, sRight; // text size width/height
    int width, height;

protected:
    int EnumFilesSourceUID;    // source UID for enumerating files in the viewer
    int EnumFilesCurrentIndex; // index of the current file in the viewer within the source

public:
    CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);
    ~CRendererWindow();

    void OnFileOpen();

    BOOL OpenFile(const char* name);

    void Paint(HDC hDC, BOOL moveEditBoxes, DWORD deferFlg = SWP_NOSIZE);

    void SetViewerTitle();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnCommand(WPARAM wParam, LPARAM lParam);

    int ComputeExtents(HDC hDC, SIZE& s, BOOL value, BOOL computeHeaderWidth = FALSE);
    void SetupScrollBars();
};
