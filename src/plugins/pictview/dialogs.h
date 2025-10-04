// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

void CleanNonNumericChars(HWND hWnd, BOOL bComboBox, BOOL bKeepSeparator, BOOL bAllowMinus = FALSE);

//****************************************************************************
//
// CCommonDialog
//
// Dialog centered to the parent
//

class CCommonDialog : public CDialog
{
public:
    CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin = ooStandard);
    CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin = ooStandard);

protected:
    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void NotifDlgJustCreated();
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

//****************************************************************************
//
// CAboutDialog
//

class CAboutDialog : public CCommonDialog
{
public:
    CAboutDialog(HWND hParent);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CImgPropDialog
//

class CImgPropDialog : public CCommonDialog
{
protected:
    const PVImageInfo* PVII;
    const char* Comment;
    int nFrames;

public:
    CImgPropDialog(HWND hParent, const PVImageInfo* pvii, const char* comment, int frames);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CExifDialog
//

struct CExifItem
{
    DWORD Tag;
    char* TagTitle;
    char* TagDescription;
    char* Value;
};

class CExifDialog : public CCommonDialog
{
protected:
    LPCTSTR FileName;
    int Format; // PVF_xxx
    BOOL DisableNotification;
    HWND HListView;
    TDirectArray<CExifItem> Items;
    TDirectArray<DWORD> Highlights;

    int ListX, ListY;
    int InfoX, InfoHeight, InfoBorder;
    int CtrlX[6], CtrlY[6];
    int MinX, MinY;

public:
    CExifDialog(HWND hParent, LPCTSTR fileName, int format);
    ~CExifDialog();

    BOOL AddItem(CExifItem* item);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void Transfer(CTransferInfo& ti);
    void InitListView();
    void FillListView();
    LPARAM GetFocusedItemLParam();
    void OnContextMenu(int x, int y);

    // returns -1 if the tag is not found in the ExifHighlights array, otherwise returns its index
    int GetHighlightIndex(DWORD tag);
    void ToggleHighlight(DWORD tag);

    void InitLayout(int id[], int n, int m);
    void RecalcLayout(int cx, int cy, int id[], int n, int m);
};

//****************************************************************************
//
// CConfigPageAppearance
//

class CConfigPageAppearance : public CCommonPropSheetPage
{
public:
    CConfigPageAppearance();

    virtual void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CConfigPageColors
//

class CConfigPageColors : public CCommonPropSheetPage
{
public:
    CGUIColorArrowButtonAbstract* ButtonBackground;
    CGUIColorArrowButtonAbstract* ButtonTransparent;
    CGUIColorArrowButtonAbstract* ButtonFSBackground;
    CGUIColorArrowButtonAbstract* ButtonFSTransparent;

    // local copy from G.* in case the user ends with Cancel
    SALCOLOR Colors[vceCount];

public:
    CConfigPageColors();

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CConfigPageKeyboard
//

class CConfigPageKeyboard : public CCommonPropSheetPage
{
public:
    CConfigPageKeyboard();

    virtual void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CConfigPageTools
//

class CConfigPageTools : public CCommonPropSheetPage
{
public:
    CConfigPageTools();

    virtual void Transfer(CTransferInfo& ti);
    virtual void Validate(CTransferInfo& ti);

protected:
    void EnableControls();
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CConfigPageAdvanced
//

class CConfigPageAdvanced : public CCommonPropSheetPage
{
public:
    CConfigPageAdvanced();

    virtual void Transfer(CTransferInfo& ti);
};

//****************************************************************************
//
// CConfigDialog
//

class CConfigDialog : public CPropertyDialog
{
protected:
    CConfigPageAppearance PageAppearance;
    CConfigPageColors PageColors;
    CConfigPageKeyboard PageKeyboard;
    CConfigPageTools PageTools;
    CConfigPageAdvanced PageAdvanced;

public:
    CConfigDialog(HWND parent);
};

//****************************************************************************
//
// CZoomDialog
//

class CZoomDialog : public CCommonDialog
{
protected:
    int* Zoom;

public:
    CZoomDialog(HWND hParent, int* zoom);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CPageDialog
//

class CPageDialog : public CCommonDialog
{
protected:
    DWORD* Page;
    DWORD NumOfPages;

public:
    CPageDialog(HWND hParent, DWORD* page, DWORD numOfPages);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CCaptureDialog
//

class CCaptureDialog : public CCommonDialog
{
public:
    CCaptureDialog(HWND hParent);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//****************************************************************************
//
// CRenameDialog
//

class CRenameDialog : public CCommonDialog
{
protected:
    LPTSTR Path;
    int PathBufSize;
    BOOL bFirstShow;

public:
    CRenameDialog(HWND hParent, LPTSTR path, int pathBufSize);

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// COverwriteDlg
//

class COverwriteDlg : public CCommonDialog
{
public:
    COverwriteDlg(HWND parent, LPCTSTR sourceName, LPCTSTR sourceAttr,
                  LPCTSTR targetName, LPCTSTR targetAttr);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    LPCTSTR SourceName,
        SourceAttr,
        TargetName,
        TargetAttr;
};

//****************************************************************************
//
// CCopyToDlg
//

class CCopyToDlg : public CCommonDialog
{
protected:
    LPCTSTR SrcName;
    LPTSTR DstName;

public:
    // dstName must point to a buffer of at least MAX_PATH in size
    // if Execute() returns IDOK, this buffer contains the full path to the file
    // we should write to (the user also confirmed overwriting if it already exists)
    CCopyToDlg(HWND parent, LPCTSTR srcName, LPTSTR dstName);

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
};

//****************************************************************************
//
// CPrintDlg
//

class CPreviewWnd : public CWindow
{
public:
    // hDlg is the parent window (dialog or window)
    // ctrlID is the ID of the child window
    CPreviewWnd(HWND hDlg, int ctrlID, CPrintDlg* printDlg);
    ~CPreviewWnd();

    CPrintDlg* pPrintDlg; // pointer to the owner so we can query through GetPrinterInfo()

    void Paint(HDC hDC); // hDC can be NULL for explicit repainting

protected:
    HBITMAP hPreview;
    int previousPrevWidth, previousPrevHeight;

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

enum CUnitsEnum
{
    untInches,
    untCM,
    untMM,
    untPoints,
    untPicas
};

class CPrintDlg;

class CPrintParams
{
public:
    // CAST1: the caller of CPrintDlg is responsible for filling it
    LPPVImageInfo pPVII;
    LPPVHandle PVHandle;
    // selected area in image coordinates
    const RECT* ImageCage; // if NULL, no selection exists

    // CAST2: can set default values via the Clear() method
    // parameters chosen in the print dialog
    BOOL bCenter;      // image will be centered relative to the paper
    double Left;       // desired image position relative to the paper origin (X)
    double Top;        // desired image position relative to the paper origin (Y)
    double Width;      // desired image dimensions
    double Height;     // desired image dimensions
    double Scale;      // scaling in percentage of original dimension
    BOOL bBoundingBox; // show a frame around the image (preview only)
    BOOL bSelection;   // print selection only
    BOOL bFit;         // stretch the image across the available area
    BOOL bKeepAspect;  // used only when bFit is TRUE
    CUnitsEnum Units;  // units used by all editboxes
    BOOL bMirrorHor, bMirrorVert;

public:
    CPrintParams()
    {
        Clear();
    }

    void Clear(); // sets default values
};

#define PRNINFO_ORIENTATION 1 // DMORIENT_PORTRAIT or DMORIENT_LANDSCAPE
//#define PRNINFO_PAGE_SIZE    2 // index into the page array
#define PRNINFO_PAPER_WIDTH 3       // physical paper width in millimeters
#define PRNINFO_PAPER_HEIGHT 4      // physical paper height in millimeters
#define PRNINFO_PAGE_LEFTMARGIN 5   //
#define PRNINFO_PAGE_TOPMARGIN 6    //
#define PRNINFO_PAGE_RIGHTMARGIN 7  //
#define PRNINFO_PAGE_BOTTOMMARGIN 8 //

// constant for converting inches to millimeters (number of mm in one inch)
#define INCH_TO_MM ((double)25.4)

//#define PRNINFO_PAGE_MINLEFTMARGIN
//#define PRNINFO_PAGE_MINTOPMARGIN
//#define PRNINFO_PAGE_MINRIGHTMARGIN
//#define PRNINFO_PAGE_MINBOTTOMMARGIN

class CPrintDlg : public CCommonDialog
{
protected:
    //    CPrintParams *PrintParams;    // for communication with the outside world
    CPrintParams Params; // for temporary internal use
    CPreviewWnd* Preview;

    // printer settings
    HANDLE HDevMode;
    HANDLE HDevNames;
    HDC HPrinterDC;
    TCHAR printerName[2 * CCHDEVICENAME];
    double origImgWidth, origImgHeight;

public:
    CPrintDlg(HWND parent, CPrintParams* printParams);
    ~CPrintDlg();

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    // called before calling Execute()
    BOOL MyGetDefaultPrinter(); // retrieves information about the default printer and returns TRUE on success
                                // if it returns FALSE, it shows an error; in that case Execute() for the dialog is not called

    BOOL GetPrinterInfo(DWORD index, void* value, DWORD& size);

    void UpdateImageParams(CPrintParams* printParams);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
    void OnPrintSetup();

    void ReleasePrinterHandles(); // releases and clears printer information handles

    void FillUnits();
    double GetNumber(HWND hCombo);
    void SetScale();
    void SetAspect();

    void CalcImgSize(double* pImgWidth, double* pImgHeight);

    friend class CPreviewWnd;
    friend class CRendererWindow;
};
