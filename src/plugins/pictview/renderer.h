// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"

// Path to focus, used by menu File/Focus
extern TCHAR Focus_Path[MAX_PATH];

// Used by SaveAs dlg to get the current path in the source(=active) panel
extern HWND ghSaveAsWindow;

// interni drzak barvy, ktery navic obsahuje flag
typedef DWORD SALCOLOR;

// SALCOLOR flags
#define SCF_DEFAULT 0x01 // barevna slozka se ignoruje a pouzije se default hodnota

#define GetCOLORREF(rgbf) ((COLORREF)rgbf & 0x00ffffff)
#define RGBF(r, g, b, f) ((COLORREF)(((BYTE)(r) | ((WORD)((BYTE)(g)) << 8)) | (((DWORD)(BYTE)(b)) << 16) | (((DWORD)(BYTE)(f)) << 24)))
#define GetFValue(rgbf) ((BYTE)((rgbf) >> 24))

// Comment size when saving
#define SAVEAS_MAX_COMMENT_SIZE 64

inline void SetRGBPart(SALCOLOR* salColor, COLORREF rgb)
{
    *salColor = rgb & 0x00ffffff | (((DWORD)(BYTE)((BYTE)((*salColor) >> 24))) << 24);
}

// Renderer colors
enum ViewerColorsEnum
{
    vceBackground,    // color for workspace in window-renderer
    vceTransparent,   // color for transparent background in window-renderer
    vceFSBackground,  // color for workspace in full screen mode
    vceFSTransparent, // color for transparent background in full screen mode
    vceCount
};

// Active tool in the renderer window
typedef enum eTool
{
    RT_HAND,
    RT_ZOOM,
    RT_SELECT,
    RT_PIPETTE
} eTool;

typedef enum eZoomType
{
    eZoomGeneral = 0,
    eShrinkToFit = 1, // alias Zoom whole
    eShrinkToWidth,
    eZoomOriginal, // alias 1:1
    eZoomFullScreen
} eZoomType;

// Local settings for this file save
// The global settings kept in Registry are in G.Save
typedef struct _gen_saveas_info
{
    DWORD Compression;
    DWORD Colors;
    DWORD PrevInputColors;
    DWORD Rotation;
    DWORD Flip;
    DWORD Flags;
    char Comment[SAVEAS_MAX_COMMENT_SIZE];
    LPPVImageInfo pvii;
} SAVEAS_INFO, *SAVEAS_INFO_PTR;

BOOL IsCageValid(const RECT* r); // vraci TRUE, pokud r->left != 0x80000000
void InvalidateCage(RECT* r);    // nastavuje r->left = 0x80000000 (klec neni validni, nebude zobrazena)

//****************************************************************************
//
// CRendererWindow
//

class CViewerWindow;
class CPrintDlg;

class CRendererWindow : public CWindow
{
public:
    CViewerWindow* Viewer;

    // promenne pro PVW32
    LPPVHandle PVHandle; // handle obrazku
    LPPVImageSequence PVSequence, PVCurImgInSeq;
    BOOL ImageLoaded;
    BOOL DoNotAttempToLoad;
    BOOL Canceled;
    DWORD OldMousePos;
    HCURSOR HOldCursor;
    LPTSTR FileName;
    __int64 ZoomFactor;
    eZoomType ZoomType;
    BOOL Capturing;
    eTool CurrTool;
    BOOL HiddenCursor;       // ma vyznam ve FullScreen, je TRUE pokud uplynul cas a zhasnul se kurzor
    BOOL CanHideCursor;      // TRUE = je mozne kurzor schovat (FALSE napr. behem otevreni Save As okna, aby se porad neschovaval kurzor mysi)
    DWORD LastMoveTickCount; // tick count pri poslednim pohybu mysi
    RECT ClientRect;

protected:
    PVImageInfo pvii;
    char* Comment;
    int XStart, YStart, XStartLoading, YStartLoading;
    int XStretchedRange, YStretchedRange;
    int XRange, YRange;
    int ZoomIndex;
    int PageWidth, PageHeight;
    BOOL Loading;
    HDC LoadingDC;
    BOOL fMirrorHor, fMirrorVert;
    HWND PipWindow;
    TCHAR toolTipText[68];
    HBRUSH HAreaBrush;
    // SelectRect je v absolutnich souradnicich obrazku a nepodleha zoom a scrollu
    RECT SelectRect;    // selected area; left = 0x8000000 if no selection (v souradnicich obrazku)
    POINT LButtonDown;  // client-window souradnice, kde uzivatel stisknul LBUTTON (pro utrzeni d&d)
    RECT TmpCageRect;   // prave tazena klec (zoom || select); v souradnicich obrazku; left/top = Anchot; right/bottom = kurzor
    POINT CageAnchor;   // pocatek klece (protilehly bod proti CageCursor) v souradnicich obrazku
    POINT CageCursor;   // viz CageAnchor
    BOOL BeginDragDrop; // TRUE pokud d&d jeste nezacal (jsme v ochrannem prostoru)
    BOOL ShiftKeyDown;  // vime o stistene klavese SHIFT
    BOOL bDrawCageRect;
    BOOL bEatSBTextOnce; // Don't update SB texts once to keep information displayed for a while
    int inWMSizeCnt;

    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index aktualniho souboru ve vieweru ve zdroji

    BOOL SavedZoomParams;
    eZoomType SavedZoomType;
    __int64 SavedZoomFactor;
    int SavedXScrollPos;
    int SavedYScrollPos;

    int BrushOrg;
    BOOL ScrollTimerIsRunning;

    // print dialog je alokovany, protoze slouzi jako nositel nastavenych parametru
    // tiskarny, orientace papiru, atd.; pokud uzivatel da po sobe dva tisky, tato
    // nastaveni zustanou zachovana
    CPrintDlg* pPrintDlg;

public:
    CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);
    ~CRendererWindow();

    BOOL OnFileOpen(LPCTSTR defaultDirectory); // set 'defaultDirectory' to NULL for current dir
    BOOL OnFileSaveAs(LPCTSTR pInitDir);
    BOOL SaveImage(LPCTSTR name, DWORD format, SAVEAS_INFO_PTR psai);
    int OpenFile(LPCTSTR name, int ShowCmd, HBITMAP hBmp);
    int HScroll(int ScrollRequest, int ThumbPos);
    int VScroll(int ScrollRequest, int ThumbPos);
    void WMSize(void);
    void ZoomTo(int zoomPercent);
    void ZoomIn(void);
    void ZoomOut(void);

    void SetTitle(void);

    BOOL IsCanceled(void) { return Canceled; };

    void CancelCapture();

    BOOL PageAvailable(BOOL next);

    BOOL SaveWallpaper(LPCTSTR fileName);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    LRESULT OnPaint();
    LRESULT OnCommand(WPARAM wParam, LPARAM lParam, BOOL* closingViewer);

    void SimplifyImageSequence(HDC dc);
    void PaintImageSequence(PAINTSTRUCT* ps, HDC dc, BOOL bTopLeft);

    void DetermineZoomIndex(void);
    void AdjustZoomFactor(void);

    void ScreenCapture();
    void SetAsWallpaper(WORD command);

    PVCODE InitiatePageLoad(void);
    void TryEnterHandToolMode(void);
    void ShutdownTool(void);

    void SelectTool(eTool tool);

    void OnContextMenu(const POINT* p);
    BOOL OnSetCursor(DWORD lParam);

    void OnDelete(BOOL toRecycle);
    void OnCopyTo();

    BOOL RenameFileInternal(LPCTSTR oldPath, LPCTSTR oldName, TCHAR (&newName)[MAX_PATH], BOOL* tryAgain);

    void FreeComment(void);
    void DuplicateComment(void);

    PVCODE GetHistogramData(LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb);

    bool GetRGBAtCursor(int x, int y, RGBQUAD* pRGB, int* pIndex);

    void UpdatePipetteTooltip();
    void UpdateInfos();

    void ClientToPicture(POINT* p); // konvertuje souradnice v bodech client area okna na souradnice obrazku
    void PictureToClient(POINT* p); // konvertuje souradnice v bodech obrazku na souradnice v client area okna

    void AutoScroll(); // hlida kurzor proti client area; pokud je za hranici, scroluje okno

    void DrawCageRect(HDC hDC, const RECT* cageRect);

    void ZoomOnTheSpot(LPARAM dwMousePos, BOOL bZoomIn);

    friend class CViewerWindow;
    friend void FillTypeFmt(HWND hDlg, OPENFILENAME* lpOFN, BOOL bUpdCompressions,
                            BOOL bUpdBitDepths, BOOL onlyEnable);
};
