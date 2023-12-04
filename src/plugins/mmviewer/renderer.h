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
    COutput Output; // rozhrani pro praci s otevrenou databazi
    CViewerWindow* Viewer;
    char FileName[MAX_PATH]; // naze v prave otevreneho souboru; 0 pokud neni zadny otevreny

    BOOL Creating; // okno se vytvari -- zatim nemazat pozadi

    SIZE sLeft, sRight; //velikost textu width/height
    int width, height;

protected:
    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index aktualniho souboru ve vieweru ve zdroji

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
