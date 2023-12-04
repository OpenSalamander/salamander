// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// CCompareFilesDialog
//

#define MAX_HISTORY_ENTRIES 20
extern char CBHistory[MAX_HISTORY_ENTRIES][MAX_PATH];
extern int CBHistoryEntries;

void AddToHistory(const char* path);

class CCompareFilesDialog : public CCommonDialog
{
protected:
    char *Path1,
        *Path2;
    BOOL& Succes;
    CCompareOptions* Options;
    WNDPROC OldEditProc1, OldEditProc2;

public:
    CCompareFilesDialog(HWND parent, char* path1, char* path2, BOOL& succes, CCompareOptions* options);
    virtual ~CCompareFilesDialog() { MainWindowQueue.Remove(HWindow); }
    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    static LRESULT CALLBACK DragDropEditProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CCommonPropSheetPage
//

class CCommonPropSheetPage : public CPropSheetPage
{
public:
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, flags, icon, origin) {}
    CCommonPropSheetPage(TCHAR* title, HINSTANCE modul, int resID, UINT helpID,
                         DWORD flags /* = PSP_USETITLE*/, HICON icon,
                         CObjectOrigin origin = ooStatic)
        : CPropSheetPage(title, modul, resID, helpID, flags, icon, origin) {}

protected:
    virtual void NotifDlgJustCreated();
};

// ****************************************************************************
//
// CPropPageColors
//

struct CColorsCfgButton
{
    int TextID;         // nazev tlacitka
    int ColorLineNumFG; // index FG barvy pro 1. tlacitko nebo -1 (FG nelze konfigurovat)
    int ColorLineNumBK; // index BK barvy pro 1. tlacitko nebo -1 (tlaciko nebude videt)
    int ColorTextFG;    // index FG barvy pro 2. tlacitko nebo -1 (FG nelze konfigurovat)
    int ColorTextBK;    // index BK barvy pro 2. tlacitko nebo -1 (tlaciko nebude videt)
};

struct CColorsCfgItem
{
    int TextID;       // nazev polozky
    int ButtonsCount; // pocet pouzitych tlacitek
    CColorsCfgButton* Buttons;
};

#define CD_NUMBER_OF_ITEMS 3

#define LINENUM_COLOR_BTN 0
#define TEXT_COLOR_BTN 1

// class CColorArrowButton;
class CPreviewWindow;
class CPropPageGeneral;

class CPropPageColors : public CCommonPropSheetPage
{
protected:
    SALCOLOR* Colors;
    SALCOLOR TempColors[NUMBER_OF_COLORS];
    CGUIColorArrowButtonAbstract* ColorButtons[4][2];
    CPreviewWindow* Preview1;
    CPreviewWindow* Preview2;
    HPALETTE HPalette;
    DWORD* ChangeFlag;
    CPropPageGeneral* PageGeneral;

public:
    CPropPageColors(SALCOLOR* colors, DWORD* changeFlag, CPropPageGeneral* pageGeneral);
    virtual ~CPropPageColors();
    void ItemChanged(int i);
    void UpdateColors(int i);
    void ChooseColor(int item, int colorFG, int colorBK, const RECT* btnRect);
    void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CPreviewWindow
//

class CPreviewWindow : public CWindow
{
protected:
    SALCOLOR* Colors;
    HFONT HFont;
    LONG FontHeight;
    LONG FontWidth;
    int Side; // 0 ... left, 1 ... right
    HPEN HLineNumBorderPen;
    HPEN HBorderPen;
    HPALETTE& HPalette;

public:
    CPreviewWindow(HWND hDlg, int ctrlID, SALCOLOR* colors, int side, HPALETTE& hPalette,
                   CObjectOrigin origin = ooAllocated);
    ~CPreviewWindow();
    void ColorsChanged();
    void RePaint();
    void SetFont(LOGFONT* font);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CButton
//

// class CButton : public CWindow
// {
//   protected:
//     BOOL         CheckBox;
//     BOOL         Checked;
//     BOOL         Highlighted;
//     BOOL         DefPushButton;
//     BOOL         Captured;
//     BOOL         Space;
//     RECT         ClientRect;
//     HBITMAP      HBitmap;
//     HBITMAP      HCopyBitmap;
//     int          CopyWidth;
//     int          CopyHeight;
//     HPEN         WndFramePen;
//     HPEN         BtnHilightPen;
//     HPEN         Btn3DLightPen;
//     HPEN         BtnShadowPen;
//
//   public:
//     CButton(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated, BOOL checkBox = FALSE);
//     ~CButton();
//     void ColorsChanged();
//     void RePaint();
//
//   protected:
//     void AllocateBitmap();
//     BOOL CreateCopyBitmap(HDC hMemoryDC, HBITMAP oldBitmap);
//     void NotifyParent(WORD notify);
//     virtual void PaintFace(HDC hdc, const RECT *rect) {}
//     virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
// };

//****************************************************************************
//
// CTextArrowButton
//
// klasicky text, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

// class CTextArrowButton : public CButton
// {
//   public:
//     CTextArrowButton(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated);
//
//   protected:
//     virtual void PaintFace(HDC hdc, const RECT *rect);
// };

//****************************************************************************
//
// CColorArrowButton
//
// pozadi s textem, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

// class CColorArrowButton : public CButton
// {
//   protected:
//     COLORREF TextColor;
//     COLORREF BkgndColor;
//     HPALETTE &HPalette;
//
//   public:
//     CColorArrowButton(HWND hDlg, int ctrlID, HPALETTE &hPalette, CObjectOrigin origin = ooAllocated);
//
//     void     SetColor(COLORREF textColor, COLORREF bkgndColor);
//     void     SetColor(COLORREF color);
//
//     void     SetTextColor(COLORREF textColor);
//     void     SetBkgndColor(COLORREF bkgndColor);
//
//     COLORREF GetTextColor() {return TextColor;}
//     COLORREF GetBkgndColor() {return BkgndColor;}
//
//   protected:
//     virtual void PaintFace(HDC hdc, const RECT *rect);
// };

// ****************************************************************************
//
// CAdvancedOptionsDialog
//

class CAdvancedOptionsDialog : public CCommonDialog
{
protected:
    CCompareOptions* InOutOptions;
    BOOL* SetDefault;
    CCompareOptions Options;

public:
    CAdvancedOptionsDialog(HWND parent, CCompareOptions* options, BOOL* setDefault);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void UpdateEncodingInfo();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CPropPageDefaultOptions
//

class CPropPageDefaultOptions : public CCommonPropSheetPage
{
protected:
    CCompareOptions* InOutOptions;
    DWORD* ChangeFlag;
    CCompareOptions oldOptions; // Original values as when the dlg was created
    CCompareOptions Options;    // Encoding part gets changed while the dlg is open

public:
    CPropPageDefaultOptions(CCompareOptions* options, DWORD* changeFlag);
    virtual void Transfer(CTransferInfo& ti);

protected:
    void UpdateEncodingInfo();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CPropPageGeneral
//

class CPropPageGeneral : public CCommonPropSheetPage
{
protected:
    CConfiguration* Configuration;
    LOGFONT LogFont;
    BOOL UseViewerFont;
    HFONT HFont;
    DWORD* ChangeFlag;

public:
    CPropPageGeneral(CConfiguration* configuration, DWORD* changeFlag);
    ~CPropPageGeneral();
    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);
    void LoadControls(BOOL initCombo);
    LOGFONT* GetCurrentFont() { return &LogFont; }

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

// ****************************************************************************
//
// CConfigurationDialog
//

extern DWORD LastCfgPage;

class CConfigurationDialog : public CPropertyDialog
{
public:
    CConfigurationDialog(HWND parent, CConfiguration* configuration, SALCOLOR* colors,
                         CCompareOptions* options, DWORD* changeFlag);

public:
    CPropPageGeneral Page1;
    CPropPageColors Page2;
    CPropPageDefaultOptions Page3;

protected:
};

UINT_PTR CALLBACK ComDlgHookProc(HWND hdlg, UINT uiMsg, WPARAM wParam, LPARAM lParam);
