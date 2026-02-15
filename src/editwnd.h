// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define TXEL_SPACE 2

//
// ****************************************************************************

class CEditLine : public CWindow
{
protected:
    BOOL SkipCharacter;
    BOOL SelChangeDisabled;

public:
    CEditLine();

    void InsertText(char* s);

    void RegisterDragDrop();
    void RevokeDragDrop();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CMainWindow;
    friend class CEditWindow;
};

//
// ****************************************************************************
//

// text in the combo box in command line; we cach it so it will not flicker
// because it is handled from the main thread we can operate via ItemBitmap

class CInnerText : public CWindow
{
protected:
    char* Message;
    int Allocated;
    int Height, Width;
    CEditWindow* EditWindow;

public:
    CInnerText(CEditWindow* editWindow);
    ~CInnerText();

    BOOL SetText(const char* txt); // sets only Message, returns when a redraw is needed
    void UpdateControl();
    int GetNeededWidth();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************

class CEditWindow : public CWindow
{
protected:
    CEditLine* EditLine;
    CInnerText* Text;
    BOOL Enabled;
    BOOL Tracking;

    char* LastText; // when the window was temporarily hidden, its contents were stored here
    // the following two variables are relevant only when LastText != NULL
    int LastSelStart; // selection position
    int LastSelEnd;

public:
    CEditWindow();
    ~CEditWindow();

    BOOL Create(HWND hParent, int childID);
    void SetDirectory(const char* dir);
    void ResizeChilds(int cx, int cy, BOOL repaint);
    int GetNeededHeight();
    void SetFont();
    void FillHistory();
    BOOL Dropped(); // listbox is dropped down
    void Enable(BOOL enable);
    BOOL IsEnabled() { return Enabled; }
    CEditLine* GetEditLine() { return EditLine; }

    BOOL KnowHWND(HWND hWnd)
    {
        return HWindow != NULL && (hWnd == HWindow ||
                                   (Text != NULL && Text->HWindow == hWnd) ||
                                   (EditLine != NULL && EditLine->HWindow == hWnd));
    }
    BOOL IsGood() { return EditLine != NULL &&
                           (HWindow == NULL || EditLine->HWindow != NULL) &&
                           (HWindow == NULL || Text->HWindow != NULL); }
    void DetachWindow()
    {
        if (EditLine != NULL)
            EditLine->DetachWindow();
        if (Text != NULL)
            Text->DetachWindow();
        CWindow::DetachWindow();
    }

    BOOL HideCaret();
    void ShowCaret();

    void StoreContent();
    void RestoreContent();
    void ResetStoredContent();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CMainWindow;
    friend class CFilesWindow;
    friend class CEditLine;
    friend class CInnerText;
};
