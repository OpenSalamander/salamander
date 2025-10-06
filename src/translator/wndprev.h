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
    HWND HDialog; // Currently displayed dialog window.
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

    // Show a message stating that the dialog cannot be previewed.
    void SetInvalidPreview();

    // Display a preview for the dialog identified by 'index'.
    // Passing -1 clears the current preview.
    void PreviewDialog(int index);

    // Repaint the preview of the currently displayed dialog.
    void RefreshDialog();

    // 'index' addresses an entry in Data.MenuPreview.
    void PreviewMenu(int index);

    // Mark the specified control as active.
    // Passing 0 clears the highlighted control.
    void HighlightControl(WORD id);
    BOOL GetHighlightControl() { return HighlightedControlID; }

    void CloseCurrentDialog();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // Show information about the selected control below the dialog preview.
    void DisplayControlInfo();

    void DisplayMenuPreview();

    static BOOL CALLBACK PreviewDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern CPreviewWindow PreviewWindow;
