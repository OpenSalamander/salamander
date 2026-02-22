// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"

// Path to focus, used by menu File/Focus
extern TCHAR Focus_Path[MAX_PATH];

// Used by SaveAs dlg to get the current path in the source(=active) panel
extern HWND ghSaveAsWindow;

// internal color holder that also contains a flag
typedef DWORD SALCOLOR;

// SALCOLOR flags
#define SCF_DEFAULT 0x01 // ignore the color component and use the default value

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

BOOL IsCageValid(const RECT* r); // returns TRUE if r->left != 0x80000000
void InvalidateCage(RECT* r);    // sets r->left = 0x80000000 (the cage is invalid and will not be shown)

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

    // variables for PVW32
    LPPVHandle PVHandle; // image handle
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
    BOOL HiddenCursor;       // relevant in FullScreen, TRUE when the cursor faded after a timeout
    BOOL CanHideCursor;      // TRUE = the cursor may be hidden (FALSE e.g. while opening Save As so the cursor stays visible)
    DWORD LastMoveTickCount; // tick count at the last mouse movement
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
    // SelectRect is in absolute image coordinates and is unaffected by zoom or scroll
    RECT SelectRect;    // selected area; left = 0x8000000 if no selection (in image coordinates)
    POINT LButtonDown;  // client-window coordinates where the user pressed LBUTTON (to detect drag start)
    RECT TmpCageRect;   // cage currently being dragged (zoom || select); in image coordinates; left/top = anchor; right/bottom = cursor
    POINT CageAnchor;   // cage origin (the point opposite to CageCursor) in image coordinates
    POINT CageCursor;   // see CageAnchor
    BOOL BeginDragDrop; // TRUE if drag&drop has not started yet (we are in the safe zone)
    BOOL ShiftKeyDown;  // remember the pressed SHIFT key
    BOOL bDrawCageRect;
    BOOL bEatSBTextOnce; // Don't update SB texts once to keep information displayed for a while
    int inWMSizeCnt;

    int EnumFilesSourceUID;    // source UID for enumerating files in the viewer
    int EnumFilesCurrentIndex; // index of the current file in the viewer within the source

    BOOL SavedZoomParams;
    eZoomType SavedZoomType;
    __int64 SavedZoomFactor;
    int SavedXScrollPos;
    int SavedYScrollPos;

    int BrushOrg;
    BOOL ScrollTimerIsRunning;

    // the print dialog is allocated because it carries the configured parameters
    // for printer, paper orientation, etc.; if the user prints twice in a row,
    // these settings remain preserved
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

    void ClientToPicture(POINT* p); // converts client-area window coordinates to image coordinates
    void PictureToClient(POINT* p); // converts image coordinates to client-area window coordinates

    void AutoScroll(); // tracks the cursor relative to the client area; if it is outside, scroll the window

    void DrawCageRect(HDC hDC, const RECT* cageRect);

    void ZoomOnTheSpot(LPARAM dwMousePos, BOOL bZoomIn);

    friend class CViewerWindow;
    friend void FillTypeFmt(HWND hDlg, OPENFILENAME* lpOFN, BOOL bUpdCompressions,
                            BOOL bUpdBitDepths, BOOL onlyEnable);
};
