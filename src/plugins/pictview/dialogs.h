// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

void CleanNonNumericChars(HWND hWnd, BOOL bComboBox, BOOL bKeepSeparator, BOOL bAllowMinus = FALSE);

//****************************************************************************
//
// CCommonDialog
//
// Dialog centrovany k parentu
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

    // vraci -1, pokud neni tag nalezen v poli ExifHighlights, jinak vraci jeho index
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

    // lokalni kopie z G.* pro pripade, ze uzivatel dal na zaver Cancel
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
    // dstName musi byt ukazatel do bufferu o minimalni velikosti MAX_PATH
    // pokud Execute() vrati IDOK, obsahuje tento buffer plnou cestu k souboru,
    // do ktereho mame zapsat (zaroven uzivatel potvrdil prepsani, pokud jiz existuje)
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
    // hDlg je parent window (dialog nebo okno)
    // ctrlID je ID child okna
    CPreviewWnd(HWND hDlg, int ctrlID, CPrintDlg* printDlg);
    ~CPreviewWnd();

    CPrintDlg* pPrintDlg; // ukazatel na vlastnika, abychom se mohli dotazovat pres GetPrinterInfo()

    void Paint(HDC hDC); // hDC muze byt NULL pro explicitni prekresleni

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
    //CAST1: za jeji plneni je zodpovedny volajici CPrintDlg
    LPPVImageInfo pPVII;
    LPPVHandle PVHandle;
    // vybrana oblast v souradnicich obrazku
    const RECT* ImageCage; // pokud je NULL, neexistuje selection

    //CAST2: umi si nastavit default hodnoty pomoci metody Clear()
    // parametry zvolene v tiskovem dialogu
    BOOL bCenter;      // obrazek bude centrovan vuci papiru
    double Left;       // pozadovane umisteni obrazku vuci pocatku papiru (X)
    double Top;        // pozadovane umisteni obrazku vuci pocatku papiru (Y)
    double Width;      // pozadovane rezmery obrazku
    double Height;     // pozadovane rezmery obrazku
    double Scale;      // scaling in percentage of original dimension
    BOOL bBoundingBox; // zobrazit ramecek kolem obrazku (just in preview)
    BOOL bSelection;   // tisknou pouze vyber
    BOOL bFit;         // roztahnout obrazek pres dostupnou plochu
    BOOL bKeepAspect;  // used only when bFit is TRUE
    CUnitsEnum Units;  // units used by all editboxes
    BOOL bMirrorHor, bMirrorVert;

public:
    CPrintParams()
    {
        Clear();
    }

    void Clear(); // nastavi default hodnoty
};

#define PRNINFO_ORIENTATION 1 // DMORIENT_PORTRAIT nebo DMORIENT_LANDSCAPE
//#define PRNINFO_PAGE_SIZE    2 // index do pole stranek
#define PRNINFO_PAPER_WIDTH 3       // fyzicka sirka papiru v milimetrech
#define PRNINFO_PAPER_HEIGHT 4      // fyzicka vyska papiru v milimetrech
#define PRNINFO_PAGE_LEFTMARGIN 5   //
#define PRNINFO_PAGE_TOPMARGIN 6    //
#define PRNINFO_PAGE_RIGHTMARGIN 7  //
#define PRNINFO_PAGE_BOTTOMMARGIN 8 //

// konstanta pro prevod palcu na mm (pocet mm v jednom palci)
#define INCH_TO_MM ((double)25.4)

//#define PRNINFO_PAGE_MINLEFTMARGIN
//#define PRNINFO_PAGE_MINTOPMARGIN
//#define PRNINFO_PAGE_MINRIGHTMARGIN
//#define PRNINFO_PAGE_MINBOTTOMMARGIN

class CPrintDlg : public CCommonDialog
{
protected:
    //    CPrintParams *PrintParams;    // pro komunikaci s vnejskem
    CPrintParams Params; // pro docasne vnitrni pouziti
    CPreviewWnd* Preview;

    // nastaveni tiskarny
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

    // volame pred volanim Execute()
    BOOL MyGetDefaultPrinter(); // vytahne informace o default tiskarne a v pripade uspechu vrati TRUE
                                // pokud vrati FALSE, zobrazi chybu; v tom pripade uz nevolame Execute() pro dialog

    BOOL GetPrinterInfo(DWORD index, void* value, DWORD& size);

    void UpdateImageParams(CPrintParams* printParams);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void EnableControls();
    void OnPrintSetup();

    void ReleasePrinterHandles(); // uvolni a vynuluje handly informaci o tiskarne

    void FillUnits();
    double GetNumber(HWND hCombo);
    void SetScale();
    void SetAspect();

    void CalcImgSize(double* pImgWidth, double* pImgHeight);

    friend class CPreviewWnd;
    friend class CRendererWindow;
};
