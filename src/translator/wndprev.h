// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* PREVIEWWINDOW_NAME;

//*****************************************************************************
//
// CPreviewWindow
//

class CPreviewWindow : public CWindow
{
public:
    HWND HDialog; // prave zobrazeny dialog
    CDialogData* CurrentDialog;
    int CurrentDialogIndex;
    int HighlightedControlID;
    HFONT HFont;
    HFONT HMenuFont;
    int MenuFontHeight;
    CMenuPreview* MenuPreview;

public:
    CPreviewWindow();
    ~CPreviewWindow();

    // zobrazi napis, ze nemuze zobrazit dialog
    void SetInvalidPreview();

    // zobrazi nahled na dialog urceny indexem
    // pokud je index == -1, zhasne soucasny nahled
    void PreviewDialog(int index);

    // prekresli preview prave zobrazeneho dialogu
    void RefreshDialog();

    // 'index' je do pole Data.MenuPreview
    void PreviewMenu(int index);

    // zobrazi dany prvek jako aktivni
    // pokud je id == 0, zhasne se aktivni control
    void HighlightControl(WORD id);
    BOOL GetHighlightControl() { return HighlightedControlID; }

    void CloseCurrentDialog();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // zobrazi pod dialog info o vybranem controlu
    void DisplayControlInfo();

    void DisplayMenuPreview();

    static BOOL CALLBACK PreviewDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern CPreviewWindow PreviewWindow;
