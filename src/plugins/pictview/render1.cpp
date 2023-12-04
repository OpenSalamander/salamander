// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <zmouse.h>
#include <shlobj.h>

#include "lib/pvw32dll.h"
#include "renderer.h"
#include "dialogs.h"
#include "histwnd.h"
#include "pictview.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang/lang.rh"
#ifdef ENABLE_WIA
#include "wiawrap.h"
#endif // ENABLE_WIA
#ifdef ENABLE_TWAIN32
#include "twain/twain.h"
#include "pvtwain.h"
#endif // ENABLE_TWAIN32
#include "exif/exif.h"
#include "PixelAccess.h"

inline int sgn(int x)
{
    if (x < 0)
        return -1;
    if (x > 0)
        return 1;
    return 0;
}

void NormalizeRect(RECT* r)
{
    if (r->left > r->right)
    {
        int tmp = r->left;
        r->left = r->right;
        r->right = tmp;
    }
    if (r->top > r->bottom)
    {
        int tmp = r->top;
        r->top = r->bottom;
        r->bottom = tmp;
    }
}

// Path to focus, used by menu File/Focus
TCHAR Focus_Path[MAX_PATH];

// Used by SaveAs dlg to get the current path in the source(=active) panel
HWND ghSaveAsWindow = NULL;

//****************************************************************************
//
// CRendererWindow
//

CRendererWindow::CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex)
    : CWindow(ooStatic)
{
    Viewer = NULL;
    PVHandle = NULL;
    PVSequence = NULL;
    ImageLoaded = DoNotAttempToLoad = Loading = FALSE;
    fMirrorHor = fMirrorVert = FALSE;
    pvii.Width = pvii.Height = (DWORD)CW_USEDEFAULT;
    Comment = NULL;
    XStart = YStart = 0;
    XStretchedRange = YStretchedRange = XRange = YRange = 0;
    ZoomFactor = 0;
    ZoomIndex = 0;
    FileName = NULL;
    PipWindow = NULL;
    CurrTool = RT_HAND;
    HiddenCursor = FALSE;
    CanHideCursor = TRUE;
    Capturing = FALSE;
    HAreaBrush = NULL;
    EnumFilesSourceUID = enumFilesSourceUID;
    EnumFilesCurrentIndex = enumFilesCurrentIndex;
    PageWidth = PageHeight = 0;
    ZeroMemory(&ClientRect, sizeof(ClientRect));
    InvalidateCage(&SelectRect);
    bDrawCageRect = TRUE;
    inWMSizeCnt = 0;
    pPrintDlg = NULL;

    SavedZoomParams = FALSE;
    SavedZoomType = eZoomGeneral;
    SavedZoomFactor = 0;
    SavedXScrollPos = 0;
    SavedYScrollPos = 0;

    BrushOrg = 0;
    BeginDragDrop = FALSE;
    ShiftKeyDown = FALSE;
    ScrollTimerIsRunning = FALSE;
    bEatSBTextOnce = FALSE;

    InvalidateCage(&TmpCageRect);
}

CRendererWindow::~CRendererWindow()
{
    if (pPrintDlg != NULL)
        delete pPrintDlg;
    if (FileName != NULL)
        SalamanderGeneral->Free(FileName);
    if (HAreaBrush != NULL)
        DeleteObject(HAreaBrush);
    FreeComment();
}

void CRendererWindow::SetTitle()
{
    TCHAR buff[MAX_PATH + 100];

    if (PVHandle != NULL)
    {
        TCHAR colors[30];
        LPTSTR fname = FileName;
        int width, height, id, nColors;
        ;

        if ((pvii.cbSize >= sizeof(pvii)) && (pvii.Format == PVF_GIF) && pvii.FSI)
        {
            width = pvii.FSI->GIF.ScreenWidth;
            height = pvii.FSI->GIF.ScreenHeight;
        }
        else
        {
            width = pvii.Width;
            height = pvii.Height;
        }

        if (!G.bShowPathInTitle && _tcsrchr(FileName, '\\'))
        {
            fname = (LPTSTR)_tcsrchr(FileName, '\\') + 1;
        }
        id = IDS_NCOLORS;
        nColors = pvii.Colors; // the defaults
        switch (pvii.Colors)
        {
        case PV_COLOR_HC15:
            nColors = 32768;
            break;
        case PV_COLOR_HC16:
            nColors = 65536;
            break;
        case PV_COLOR_TC24:
        case PV_COLOR_TC32:
            if (pvii.ColorModel == PVCM_CMYK)
                id = IDS_NCOLORS_CMYK;
            else if (pvii.ColorModel == PVCM_CIELAB)
                id = IDS_NCOLORS_LAB;
            else
                nColors = 16777216;
            break;
        }
        if (IDS_NCOLORS == id)
        {
            TCHAR fmt[32];
            CQuadWord clrs(nColors, 0);
            SalamanderGeneral->ExpandPluralString(fmt, sizeof(fmt), LoadStr(id), 1, &clrs);
            _stprintf(colors, fmt, nColors);
        }
        else
        {
            _stprintf(colors, LoadStr(id), nColors);
        }
        if (pvii.NumOfImages == 1)
            _stprintf(buff, LoadStr(IDS_TITLE), fname, width, height,
                      colors, (int)(ZoomFactor / (ZOOM_SCALE_FACTOR / 100)), LoadStr(IDS_PLUGINNAME));
        else
            _stprintf(buff, LoadStr(IDS_TITLE_MULTI), fname, width, height,
                      colors, pvii.CurrentImage + 1, pvii.NumOfImages, (int)(ZoomFactor / (ZOOM_SCALE_FACTOR / 100)), LoadStr(IDS_PLUGINNAME));
    }
    else
        _tcscpy(buff, LoadStr(IDS_PLUGINNAME));
    SetWindowText(Viewer->HWindow, buff);
}

BOOL CRendererWindow::OnFileOpen(LPCTSTR defaultDirectory)
{
    TCHAR file[MAX_PATH] = _T("");
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(OPENFILENAME));
    ofn.lStructSize = sizeof(OPENFILENAME);
    ofn.hwndOwner = HWindow;
    // Pokousel jsem se do filtru narvat vsechny pripony, ale filtr prestal fungovat
    // v MSDN o zadnem omezeni nepisou. IrfanView to resi takhle
    TCHAR filterStr[1000];
    lstrcpyn(filterStr, LoadStr(IDS_OPENFILTER), SizeOf(filterStr));
    LPTSTR s = filterStr;
    ofn.lpstrFilter = s;
    while (*s != 0) // vytvoreni double-null terminated listu
    {
        if (*s == '|')
            *s = 0;
        s++;
    }
    ofn.lpstrFile = file;
    ofn.nMaxFile = MAX_PATH;
    ofn.nFilterIndex = 1;
    ofn.lpstrInitialDir = defaultDirectory;
    ofn.Flags = OFN_HIDEREADONLY | OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;
    if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
    {
        EnumFilesSourceUID = -1;
        OpenFile(file, -1, NULL);
    }
    return TRUE;
} /* CRendererWindow::OnFileOpen */

BOOL CRendererWindow::OpenFile(LPCTSTR name, int showCmd, HBITMAP hBmp)
{
    CALL_STACK_MESSAGE2(_T("CViewerWindow::OpenFile(%s)"), name);

    PVCODE code;
    LPPVHandle OldPVHandle = PVHandle;
    PVOpenImageExInfo oiei;
#ifdef _UNICODE
    char nameA[_MAX_PATH];
#endif

    if (showCmd != -1)
    {
        ZoomType = G.ZoomType;
    }
    else
    {
        // Opening image in an already open window
        // Don't switch to full screen if currently not in full screen although it's the default
        ZoomType = (G.ZoomType == eZoomFullScreen)
                       ? ((ZoomType == eZoomFullScreen) ? eZoomFullScreen : eShrinkToFit)
                       : G.ZoomType;
    }
    fMirrorHor = fMirrorVert = FALSE;
    InvalidateCage(&SelectRect);

    memset(&oiei, 0, sizeof(oiei));
    oiei.cbSize = sizeof(oiei);
    if (hBmp)
    {
        oiei.Flags = PVOF_ATTACH_TO_HANDLE;
        oiei.Handle = hBmp;
    }
    else
    {
#ifdef _UNICODE
        WideCharToMultiByte(CP_ACP, 0, name, -1, nameA, sizeof(nameA), NULL, NULL);
        nameA[sizeof(nameA) - 1] = 0;
        oiei.FileName = nameA;
#else
        oiei.FileName = name;
#endif
    }
    FreeComment();
    code = PVW32DLL.PVOpenImageEx(&PVHandle, &oiei, &pvii, sizeof(pvii));

    if (code == PVC_OK)
    {
        DuplicateComment();
        if (hBmp)
        {
            // we must be able to delete the bitmap, therefore we read it right here
            CALL_STACK_MESSAGE1("PVW32DLL.PVReadImage");
            code = PVW32DLL.PVReadImage2(PVHandle, 0, NULL, NULL, 0, 0);
            DeleteObject(hBmp);
            if (PVSequence)
            {
                KillTimer(HWindow, IMGSEQ_TIMER_ID);
            }
            PVCurImgInSeq = PVSequence = NULL;
        }

        if ((pvii.Flags & PVFF_IMAGESEQUENCE) && (pvii.Format == PVF_GIF))
        {
            // Patch: Width/Height describe the first frame, but we need total image-screen size
            // to be able init XStretchedRange, XStart, ZoomFactor etc.
            pvii.Width = pvii.FSI->GIF.ScreenWidth;
            pvii.Height = pvii.FSI->GIF.ScreenHeight;
        }
    }
    else
    {
        if (hBmp)
            DeleteObject(hBmp);
    }
    if (showCmd != -1)
    {
        // just opening the window -> must show
        DWORD tmp = pvii.Width;
        LPPVHandle tmp2 = PVHandle;

        pvii.Width = CW_USEDEFAULT; // to prevent WM_SIZE from modifying scrollbars
        PVHandle = NULL;            // to prevent WM_PAINT handler from calling PVReaDImage
        if (code == PVC_OK)
        {
            if (G.WindowPos != WINDOW_POS_SAME)
            {
                RECT r;
                int winWidth, winHeight;

                winWidth = tmp + G.TotalNCWidth;
                winHeight = pvii.Height + G.TotalNCHeight;
                // Rebar always includes the menu and may also include the toolbar (if visible)
                GetWindowRect(Viewer->HRebar, &r);
                winHeight += r.bottom - r.top;

                if (G.StatusbarVisible && Viewer->StatusBar)
                {
                    GetWindowRect(Viewer->StatusBar->HWindow, &r);
                    winHeight += r.bottom - r.top;
                }

                GetWindowRect(Viewer->HWindow, &r); // initially same as Salamander's window

                if (G.WindowPos == WINDOW_POS_LARGER)
                {
                    // do not make window smaller than Salamander's window
                    winWidth = max(winWidth, r.right - r.left);
                    winHeight = max(winHeight, r.bottom - r.top);
                }
                // center image against the Salamander's window
                r.left -= (winWidth - (int)(r.right - r.left)) / 2;
                r.top -= (winHeight - (int)(r.bottom - r.top)) / 2;

                r.right = r.left + winWidth;
                r.bottom = r.top + winHeight;

                SalamanderGeneral->MultiMonEnsureRectVisible(&r, FALSE);

                SetWindowPos(Viewer->HWindow, 0, r.left, r.top, r.right - r.left, r.bottom - r.top,
                             SWP_NOZORDER | SWP_NOREDRAW);
            }
        }
        ShowWindow(Viewer->HWindow, showCmd);
        SetForegroundWindow(Viewer->HWindow);
        UpdateWindow(Viewer->HWindow);
        pvii.Width = tmp;
        PVHandle = tmp2;
    }

    if (code != PVC_OK)
    {
        TCHAR errText[MAX_PATH + 100];
        _stprintf(errText, LoadStr(IDS_ERROR_OPENING), name, PVW32DLL.PVGetErrorText(code));
        SalamanderGeneral->SalMessageBox(HWindow, errText, LoadStr(IDS_ERRORTITLE), MB_ICONEXCLAMATION);
        // In case the new file is not recognized, we keep the old one
        if (PVHandle != NULL)
            PVW32DLL.PVCloseImage(PVHandle);
        PVHandle = OldPVHandle;
        //XRange = YRange = 0;
    }
    else
    {
        if (name != FileName)
        {
            if (FileName != NULL)
            {
                SalamanderGeneral->Free(FileName);
            }
            FileName = SalamanderGeneral->DupStr(name);
        }
        if (PVSequence)
        {
            KillTimer(HWindow, IMGSEQ_TIMER_ID);
        }
        PVCurImgInSeq = PVSequence = NULL;
        CALL_STACK_MESSAGE1("PVW32DLL.PVCloseImage");
        PVW32DLL.PVCloseImage(OldPVHandle);
        SetScrollPos(HWindow, SB_VERT, 0, FALSE);
        SetScrollPos(HWindow, SB_HORZ, 0, FALSE);
        Canceled = DoNotAttempToLoad = Loading = FALSE;
        if (hBmp)
        {
            TryEnterHandToolMode();
            ImageLoaded = TRUE;
        }
        else
        {
            ImageLoaded = FALSE;
        }
        XStart = YStart = 0;
        XStretchedRange = XRange = max(1, pvii.Width);
        if (pvii.HorDPI && pvii.VerDPI)
        {
            YRange = pvii.Height * pvii.HorDPI / pvii.VerDPI;
        }
        else
        {
            YRange = pvii.Height;
        }
        YStretchedRange = YRange;
        ZoomFactor = ZOOM_SCALE_FACTOR;
    }
    SetTitle();

    if ((code == PVC_OK) && (hBmp == NULL))
    {
        TCHAR path[MAX_PATH];

        _tcscpy(path, name);
        // nesmime do AddToHistory propadnout primo 'name', protoze muze byt z historie
        // a doslo by ke konfliktu pri presunu
        AddToHistory(TRUE, path);
        LPTSTR s = _tcsrchr(path, '\\');
        if (s != NULL)
        {
            if (s == path + 2) // disky C:\, D:\, ... lomitko nechame a terminujeme az za nim
                s++;
            *s = 0;
            if (path[0] != 0)
            {
                AddToHistory(FALSE, path);
            }
        }
    }

    if (PVHandle)
    {
        CALL_STACK_MESSAGE1("PVW32DLL.PVSetBkHandle");
        PVW32DLL.PVSetBkHandle(PVHandle, GetCOLORREF(G.Colors[Viewer->IsFullScreen() ? vceFSTransparent : vceTransparent]));
    }
    WMSize();
    UpdateInfos();
    InvalidateRect(HWindow, NULL, FALSE);

    return (code == PVC_OK);
} /* CRendererWindow::OpenFile */

BOOL IsCageValid(const RECT* r)
{
    return r->left != 0x80000000;
}

void InvalidateCage(RECT* r)
{
    r->left = 0x80000000;
}

void CRendererWindow::DrawCageRect(HDC hDC, const RECT* cageRect)
{
    if (!IsCageValid(cageRect))
        return;

    RECT r = *cageRect;
    PictureToClient((POINT*)&r.left);
    PictureToClient((POINT*)&r.right);

    NormalizeRect(&r); // normalizujeme obdelnik, abychom obesli GDI-specificke kresleni obdelniku

    // pravy dolni roh zvetsime tak, abychom lezeli na hranici bodu z jeho vnitrni strany
    if ((DWORD)XStretchedRange > pvii.Width) // pouze pro zoom > 100%
    {
        r.right += (LONG)((double)XStretchedRange / (double)pvii.Width - 1.0);
        r.bottom += (LONG)((double)YStretchedRange / (double)pvii.Height - 1.0);
    }

    WORD bits[8] = {0x000F, 0x001E, 0x003C, 0x0078, // srafovani 45 stupnu
                    0x00F0, 0x00E1, 0x00C3, 0x0087};
    HBITMAP hBrushBitmap = CreateBitmap(8, 8, 1, 1, &bits);
    HBRUSH hBrush = CreatePatternBrush(hBrushBitmap);
    HDC hDC2 = (hDC != NULL) ? hDC : GetDC(HWindow);
    SetBrushOrgEx(hDC2, 0, BrushOrg, NULL); // BrushOrg se meni kazdy 100ms
    DeleteObject(hBrushBitmap);
    HRGN hRgn = CreateRectRgn(r.left, r.top, r.right + 1, r.bottom + 1);
    int oldROP2 = SetROP2(hDC2, R2_XORPEN);
    FrameRgn(hDC2, hRgn, hBrush, 1, 1);
    SetROP2(hDC2, oldROP2);
    DeleteObject(hRgn);
    DeleteObject(hBrush);
    if (!hDC)
        ReleaseDC(HWindow, hDC2);
}

int CRendererWindow::VScroll(int ScrollRequest, int ThumbPos)
{
    if (PageHeight > YStretchedRange) // scrollbar vubec neni zobrazen, nemame tu co delat
        return 0;

    int oldYPos, YPos = GetScrollPos(HWindow, SB_VERT);
    oldYPos = YPos;

    switch (ScrollRequest)
    {
    case SB_LINEDOWN:
        YPos += YLine;
        break;
    case SB_LINEUP:
        YPos -= YLine;
        break;
    case SB_PAGEDOWN:
        YPos += PageHeight;
        break;
    case SB_PAGEUP:
        YPos -= PageHeight;
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        YPos = ThumbPos;
        break;
    }
    // kontrola mezi
    if (YPos < 0)
        YPos = 0;
    if (YPos > YStretchedRange - PageHeight)
        YPos = YStretchedRange - PageHeight;

    if (YPos != oldYPos)
    {
        DrawCageRect(NULL, &SelectRect);
        DrawCageRect(NULL, &TmpCageRect);
        SetScrollPos(HWindow, SB_VERT, YPos, TRUE);                                        //nastavime pozici scrolleru
                                                                                           //    ScrollWindow(HWindow, 0, oldYPos-YPos, NULL, NULL);//some people prefer ScrollWindowEx
        ScrollWindowEx(HWindow, 0, oldYPos - YPos, NULL, NULL, NULL, NULL, SW_INVALIDATE); //odscrollujeme okno
        bDrawCageRect = FALSE;
        UpdateWindow(HWindow);
        bDrawCageRect = TRUE;
        DrawCageRect(NULL, &TmpCageRect);
        DrawCageRect(NULL, &SelectRect);
        Sleep(1);
    }
    return oldYPos - YPos; //vratime zmenu
}

int CRendererWindow::HScroll(int ScrollRequest, int ThumbPos)
{
    if (PageWidth > XStretchedRange) // scrollbar vubec neni zobrazen, nemame tu co delat
        return 0;

    int oldXPos, XPos = GetScrollPos(HWindow, SB_HORZ);
    oldXPos = XPos;

    switch (ScrollRequest)
    {
    case SB_LINERIGHT:
        XPos += XLine;
        break;
    case SB_LINELEFT:
        XPos -= XLine;
        break;
    case SB_PAGERIGHT:
        XPos += PageWidth;
        break;
    case SB_PAGELEFT:
        XPos -= PageWidth;
        break;
    case SB_THUMBPOSITION:
    case SB_THUMBTRACK:
        XPos = ThumbPos;
        break;
    }
    // kontrola mezi
    if (XPos < 0)
        XPos = 0;
    if (XPos > XStretchedRange - PageWidth)
        XPos = XStretchedRange - PageWidth;

    if (XPos != oldXPos)
    {
        DrawCageRect(NULL, &SelectRect);
        DrawCageRect(NULL, &TmpCageRect);
        SetScrollPos(HWindow, SB_HORZ, XPos, TRUE);                                        //nastavime pozici scrolleru
                                                                                           //    ScrollWindow(HWindow, oldXPos-XPos, 0, NULL, NULL);//some people prefer ScrollWindowEx
        ScrollWindowEx(HWindow, oldXPos - XPos, 0, NULL, NULL, NULL, NULL, SW_INVALIDATE); //odscrollujeme okno
        bDrawCageRect = FALSE;
        UpdateWindow(HWindow);
        bDrawCageRect = TRUE;
        Sleep(1);
        DrawCageRect(NULL, &TmpCageRect);
        DrawCageRect(NULL, &SelectRect);
    }
    return oldXPos - XPos; //vratime zmenu
}

void CRendererWindow::WMSize(void)
{
    RECT rect;
    UINT32 pw, oldPW = PageWidth, oldPH = PageHeight;
    int oldRX = XStretchedRange, oldRY = YStretchedRange;
    SCROLLINFO sc;

    GetClientRect(HWindow, &rect);

    CALL_STACK_MESSAGE7(_T("CRendererWindow::WMSize: pg %dx%d, str %dx%d, cl %dx%d"),
                        PageWidth, PageHeight, XStretchedRange, YStretchedRange, rect.right, rect.bottom);

    if (pvii.Width == (DWORD)CW_USEDEFAULT)
        return; //obrazek jeste nebyl nascannovan

    if (IsIconic(Viewer->HWindow) || !rect.right || !rect.bottom)
        return; //we are minimized -> no point of changing size; crash otherwise

    pw = PageWidth = rect.right;
    PageHeight = rect.bottom;

    inWMSizeCnt++;

    switch (ZoomType)
    {
    case eZoomOriginal:
        ZoomFactor = ZOOM_SCALE_FACTOR;
        XStretchedRange = XRange;
        YStretchedRange = YRange;
        DetermineZoomIndex();
        break;

    case eShrinkToFit:
        if (XRange > PageWidth)
            XStretchedRange = PageWidth;
        else
            XStretchedRange = XRange;
        YStretchedRange = (int)(YRange * XStretchedRange) / XRange;
        if (YStretchedRange > PageHeight)
        {
            YStretchedRange = PageHeight;
            XStretchedRange = (int)(XRange * YStretchedRange) / YRange;
        }
        if (!XStretchedRange)
            XStretchedRange = 1;
        if (!YStretchedRange)
            YStretchedRange = 1;
        ZoomFactor = (ZOOM_SCALE_FACTOR * XStretchedRange) / XRange;
        DetermineZoomIndex();
        break;

    case eShrinkToWidth:
        if (!Loading)
        {
            int tmp1, tmp2;

            if (inWMSizeCnt > 5)
            {
                // Looks like we have entered infinite loop
                sc.cbSize = sizeof(sc);
                sc.fMask = SIF_RANGE | SIF_PAGE;
                GetScrollInfo(HWindow, SB_VERT, &sc);
                if (sc.nMax < (int)sc.nPage)
                { // no scroll bar visible
                    GetScrollInfo(HWindow, SB_HORZ, &sc);
                    if (sc.nMax < (int)sc.nPage)
                    { // no scroll bar visible
                        // keep the current settings - prevent stack overflow
                        inWMSizeCnt--;
                        return;
                    }
                }
            }
            XStart = 0;
            YStart = 0;
            if (XRange > PageWidth)
                XStretchedRange = PageWidth;
            else
                XStretchedRange = XRange;
            ZoomFactor = (ZOOM_SCALE_FACTOR * XStretchedRange) / XRange;
            YStretchedRange = (int)(YRange * ZoomFactor) / ZOOM_SCALE_FACTOR;
            DetermineZoomIndex();

            sc.cbSize = sizeof(sc);
            sc.fMask = SIF_RANGE | SIF_PAGE;
            GetScrollRange(HWindow, SB_VERT, &tmp1, &tmp2);
            if (tmp2 > tmp1)
            { // max > min
                //obsolete GetScrollRange vrati na rozdil od GetScrollInfo smysluplne udaje
                GetScrollInfo(HWindow, SB_VERT, &sc);
                if (sc.nMax >= (int)sc.nPage)
                    pw += GetSystemMetrics(SM_CXVSCROLL);
            }

            tmp1 = YRange * pw / XRange;
            if ((tmp1 <= PageHeight) && (pw <= (UINT32)XRange))
            {
                PageWidth = pw;
                XStretchedRange = pw;
                YStretchedRange = tmp1;
            }
            else
            {
                tmp1 = pw - GetSystemMetrics(SM_CXVSCROLL);
                if ((YRange * tmp1 / XRange <= PageHeight) && (YRange >= PageHeight))
                {
                    PageWidth = pw;
                    YStretchedRange = PageHeight;
                    XStretchedRange = XRange * PageHeight / YRange;
                }
            }
        }
        break;
    case eZoomFullScreen:
        ZoomType = eShrinkToFit;
        if (!Viewer->IsFullScreen())
        {
            Viewer->ToggleFullScreen();
        }
        WMSize();
        inWMSizeCnt--;
        return;

    case eZoomGeneral:
    default:
        XStretchedRange = (int)((ZoomFactor * (__int64)XRange) / ZOOM_SCALE_FACTOR);
        YStretchedRange = (int)((ZoomFactor * (__int64)YRange) / ZOOM_SCALE_FACTOR);
        if (!XStretchedRange)
            XStretchedRange = 1;
        if (!YStretchedRange)
            YStretchedRange = 1;
        break;
    }
    if (!Loading)
    {
        if (CFGCenterImage)
        {
            if (XStretchedRange < PageWidth)
                XStart = (PageWidth - XStretchedRange) / 2;
            else
                XStart = 0;
            if (YStretchedRange < PageHeight)
                YStart = (PageHeight - YStretchedRange) / 2;
            else
                YStart = 0;
        }
    }

    sc.cbSize = sizeof(sc);
    sc.fMask = SIF_RANGE | SIF_PAGE;
    sc.nMin = 0;
    sc.nMax = XStretchedRange - 1;
    // temhle jednickam nerozumim - kdyz dam nMin==nMax, tak windows
    // uz ukazou scrollbaru a jeste k tomu o bod mensi rolovatko - nechapu
    sc.nPage = PageWidth;
    if ((XStretchedRange > PageWidth) && (oldRX != XStretchedRange) && oldRX)
    { //aneb meli jsme scrollbar a menime sirku okna
        sc.fMask |= SIF_POS;
        if (oldPW > (UINT32)oldRX)
        {
            // no scrollbar previously
            sc.nPos = (sc.nMax - PageWidth) / 2;
        }
        else
        {
            sc.nPos = (int)(((__int64)GetScrollPos(HWindow, SB_HORZ) + oldPW / 2) * XStretchedRange / oldRX - PageWidth / 2);
        }
    }
    SetScrollInfo(HWindow, SB_HORZ, &sc, TRUE /*redraw*/);
    sc.nMin = 0;
    sc.nMax = YStretchedRange - 1;
    sc.nPage = PageHeight;
    if ((YStretchedRange > PageHeight) && (oldRY != YStretchedRange) && oldRY && oldPH)
    { //aneb meli jsme scrollbar a menime vysku okna
        sc.fMask |= SIF_POS;
        if (oldPH > (UINT32)oldRY)
        {
            // no scrollbar previously
            sc.nPos = (sc.nMax - PageHeight) / 2;
        }
        else
        {
            sc.nPos = (int)(((__int64)GetScrollPos(HWindow, SB_VERT) + oldPH / 2) * YStretchedRange / oldRY - PageHeight / 2);
        }
    }
    SetScrollInfo(HWindow, SB_VERT, &sc, TRUE /*redraw*/);
    PVW32DLL.PVSetStretchParameters(PVHandle, XStretchedRange * (1 - 2 * fMirrorHor),
                                    YStretchedRange * (1 - 2 * fMirrorVert), /*pvii.Colors == 2 ? BLACKONWHITE : */ COLORONCOLOR);
    SetTitle();
    Viewer->SetupStatusBarItems();
    Viewer->UpdateToolBar();
    inWMSizeCnt--;
}

void CRendererWindow::DetermineZoomIndex(void)
{
    __int64 zf = ZoomFactor;

    ZoomIndex = 0;

    if (zf > ZOOM_SCALE_FACTOR)
    {
        for (; zf >= 2 * ZOOM_SCALE_FACTOR; zf /= 2)
        {
            ZoomIndex += 2 * 3;
        }
        while (zf > (ZOOM_SCALE_FACTOR * ZOOM_STEP_FACT) / 1000)
        {
            ZoomIndex += 2;
            zf = (zf * 1000) / ZOOM_STEP_FACT;
        }
        if (zf > ZOOM_SCALE_FACTOR)
        {
            ZoomIndex++;
        }
    }
    else if (zf < ZOOM_SCALE_FACTOR)
    {
        //     ZoomIndex = -1;
        for (; zf <= ZOOM_SCALE_FACTOR / 2; zf *= 2)
        {
            ZoomIndex -= 2 * 3;
        }
        while (zf <= (ZOOM_SCALE_FACTOR * 1000) / ZOOM_STEP_FACT)
        {
            ZoomIndex -= 2;
            zf = (zf * ZOOM_STEP_FACT) / 1000;
        }
        if (zf < ZOOM_SCALE_FACTOR)
        {
            ZoomIndex--;
        }
    }
    /*
   if (zf != ZOOM_SCALE_FACTOR)
   if (zf > ZOOM_SCALE_FACTOR)
   {
     ZoomIndex = 1;
     for ( ; zf >= 2*ZOOM_SCALE_FACTOR; zf /= 2)
       ZoomIndex += 2*3;
     while (zf > (ZOOM_SCALE_FACTOR * ZOOM_STEP_FACT) / 1000)
     {
       ZoomIndex += 2;
       zf = (zf * 1000) / ZOOM_STEP_FACT;
     }
   }
   else
   {
     ZoomIndex = -1;
     for ( ; zf <= ZOOM_SCALE_FACTOR/2; zf *= 2)
       ZoomIndex -= 2*3;
     while (zf < (ZOOM_SCALE_FACTOR * 1000) / ZOOM_STEP_FACT)
     {
       ZoomIndex -= 2;
       zf = (zf * ZOOM_STEP_FACT) / 1000;
     }
   }
 */
}

void CRendererWindow::AdjustZoomFactor(void)
{
    int i = ZoomIndex / 2;

    ZoomFactor = ZOOM_SCALE_FACTOR;

    if (i > 0)
    {
        for (; i > 2; i -= 3)
            ZoomFactor *= 2;
        while (i-- > 0)
            ZoomFactor = (ZoomFactor * ZOOM_STEP_FACT) / 1000; /*Root[2, 3]*/
    }
    else
    {
        for (; i < -2; i += 3)
            ZoomFactor /= 2;
        while (i++ < 0)
            ZoomFactor = (ZoomFactor * 1000) / ZOOM_STEP_FACT; /*Root[2, 3]*/
    }
    if (ZoomFactor > ZOOM_MAX * ZOOM_SCALE_FACTOR)
        ZoomFactor = ZOOM_MAX * ZOOM_SCALE_FACTOR; //max factor 1000%
    if (ZoomFactor < 50)
    {
        ZoomFactor = 50;
        DetermineZoomIndex();
    }
}

void CRendererWindow::ZoomIn(void)
{
    if (!Loading && (ZoomFactor < ZOOM_MAX * ZOOM_SCALE_FACTOR))
    {
        __int64 OldZoomFactor = ZoomFactor;

        ZoomType = eZoomGeneral;
        if (ZoomIndex & 1)
            ZoomIndex++;
        else
            ZoomIndex += 2;
        AdjustZoomFactor();
        if ((ZoomFactor >= (__int64)(0.25 * ZOOM_SCALE_FACTOR)) &&
            (ZoomFactor / (ZOOM_SCALE_FACTOR / 100) == OldZoomFactor / (ZOOM_SCALE_FACTOR / 100)))
        {
            // Users don't like if the initial zoom factor is e.g. 79.375% and when they
            // hit Zoom In, a standard 79.428% zoom is obtained. From their perspective,
            // the window title still says 79% and due to the small difference the image
            // is enlarged by a single line/column or not at all.
            ZoomIndex += 2;
            AdjustZoomFactor();
        }
        WMSize();
        InvalidateRect(HWindow, NULL, FALSE);
    }
}

void CRendererWindow::ZoomOut(void)
{
    if (!Loading)
    {
        __int64 OldZoomFactor = ZoomFactor;

        ZoomType = eZoomGeneral;
        if (ZoomIndex & 1)
            ZoomIndex--;
        else
            ZoomIndex -= 2;
        AdjustZoomFactor();
        if ((ZoomFactor >= (__int64)(0.25 * ZOOM_SCALE_FACTOR)) &&
            (ZoomFactor / (ZOOM_SCALE_FACTOR / 100) == OldZoomFactor / (ZOOM_SCALE_FACTOR / 100)))
        {
            // Users don't like if the initial zoom factor is e.g. 79.499% and when they
            // hit Zoom Out, a standard 79.428% zoom is obtained. From their perspective,
            // the window title still says 79% and due to the small difference the image
            // is shrunk by a single line/column or not at all.
            ZoomIndex -= 2;
            AdjustZoomFactor();
        }
        WMSize();
        InvalidateRect(HWindow, NULL, FALSE);
    }
}

void CRendererWindow::ZoomTo(int zoomPercent)
{
    if (!Loading)
    {
        __int64 OldZoomFactor = ZoomFactor;

        if (zoomPercent < 0)
        { // mame se doptat dialogem
            // CZoomDialog works with percentage values scaled with (ZOOM_SCALE_FACTOR / 100)
            // This is to allow 2 decimal digits precision
            zoomPercent = (int)ZoomFactor; //(int)((double)ZoomFactor) / (ZOOM_SCALE_FACTOR / 100);
            CZoomDialog dlg(HWindow, &zoomPercent);
            if (dlg.Execute() != IDOK)
                return;
            ZoomFactor = zoomPercent;
        }
        else
        {
            ZoomFactor = (__int64)(((double)zoomPercent / 100.0) * (ZOOM_SCALE_FACTOR / 100));
        }
        ZoomType = eZoomGeneral;
        if (ZoomFactor > ZOOM_MAX * ZOOM_SCALE_FACTOR)
        {
            ZoomFactor = ZOOM_MAX * ZOOM_SCALE_FACTOR; //max factor 1600%
        }
        if (!ZoomFactor)
        {
            ZoomFactor = 1;
        }
        DetermineZoomIndex();
        WMSize();
        InvalidateRect(HWindow, NULL, FALSE);
    }
}

void CRendererWindow::OnContextMenu(const POINT* p)
{
    CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
    if (popup != NULL)
    {
        popup->LoadFromTemplate(HLanguage, PopupMenuTemplate, Viewer->Enablers,
                                Viewer->HGrayToolBarImageList, Viewer->HHotToolBarImageList);
        popup->UpdateItemsState();
        popup->Track(MENU_TRACK_RIGHTBUTTON, p->x, p->y, HWindow, NULL);
        SalamanderGUI->DestroyMenuPopup(popup);
    }
}

BOOL WINAPI ProgressProcedure(int done, void* data)
{
    //   if (GetInputState()) //any mouse/keyboard events in the queue?
    {
        MSG msg;

        while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
        {
            if (!TranslateAccelerator(((CRendererWindow*)data)->HWindow, G.HAccel, &msg))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }

            // John, Petr: predcasny navrat pred odcerpani vsech zprav zpusoboval
            // pod W2K zavreni PictView a nasledni nedoslo k aktivaci Salamandera
            // Patera 2002.08.11: must be conditionally enabled to allow e.g. fast
            // browsing between pages, before the current one is fully loaded
            if (((CRendererWindow*)data)->IsCanceled())
            {
                if (msg.message == WM_COMMAND)
                {
                    WORD cmd = LOWORD(msg.wParam);
                    if (cmd == CMD_PREVPAGE ||
                        cmd == CMD_NEXTPAGE ||
                        cmd == CMD_LASTPAGE ||
                        cmd == CMD_FIRSTPAGE ||
                        cmd == CMD_PAGE_INTERNAL ||
                        cmd == CMD_FILE_FIRST ||
                        cmd == CMD_FILE_PREV ||
                        cmd == CMD_FILE_NEXT ||
                        cmd == CMD_FILE_PREVSELFILE ||
                        cmd == CMD_FILE_NEXTSELFILE ||
                        cmd == CMD_FILE_LAST ||
                        cmd == CMD_RELOAD)
                    {
                        return TRUE;
                    }
                }
            }
        }

        if (((CRendererWindow*)data)->IsCanceled())
            return TRUE;
    }
    return FALSE;
}

BOOL WINAPI ProgressProcedure2(int done, void* data)
{
    ((CRendererWindow*)data)->Viewer->SetProgress(done);
    return ProgressProcedure(done, data);
}

BOOL CRendererWindow::PageAvailable(BOOL next)
{
    if (next)
        return pvii.CurrentImage + 1 < pvii.NumOfImages;
    else
        return pvii.CurrentImage > 0;
}

BOOL CRendererWindow::OnSetCursor(DWORD lParam)
{
    if (HiddenCursor)
    {
        SetCursor(NULL);
        return TRUE;
    }

    if (!Loading && !ImageLoaded)
    {
        SetCursor(LoadCursor(NULL, IDC_ARROW));
        return TRUE;
    }

    WORD nHittest = LOWORD(lParam);
    if (nHittest == HTCLIENT)
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

        switch (CurrTool)
        {
        case RT_HAND:
        {
            if (controlPressed && !altPressed && !shiftPressed)
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMIN)));
            else if (!controlPressed && !altPressed && shiftPressed)
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMOUT)));
            else
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_HAND1)));
            return TRUE;
        }

        case RT_ZOOM:
        {
            // vzadu podminka: behem tazeni klece nelze Shiftem nahodit ZOOMOUT
            if (!controlPressed && !altPressed && shiftPressed && (GetCapture() == NULL || BeginDragDrop))
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMOUT)));
            else
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMIN)));
            return TRUE;
        }

        case RT_PIPETTE:
        {
            if (controlPressed && !altPressed && !shiftPressed)
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMIN)));
            else if (!controlPressed && !altPressed && shiftPressed)
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_ZOOMOUT)));
            else
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_PIPETA)));
            return TRUE;
        }

        case RT_SELECT:
        {
            // select kurzor neni viditelny nad sedivou plochou, ktera obklopuje obrazek
            // budeme nad ni tedy zobrazovat bezny kurzor
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);
            if (p.x >= XStart && p.x < XStart + XStretchedRange &&
                p.y >= YStart && p.y < YStart + YStretchedRange)
                SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_SELECT)));
            else
                SetCursor(LoadCursor(NULL, IDC_ARROW));
            return TRUE;
        }

        default:
        {
            if (Loading)
            {
                if (!HiddenCursor)
                    SetCursor(LoadCursor(NULL, IDC_WAIT));
                return TRUE;
            }
        }
        }
        /*#ifndef ENABLE_TOOL_TIP
    if (PipWindow)
    {
      SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_PIPETA)));
      return TRUE;
    }
#endif
    if (ImageLoaded)
    {
      if (XStretchedRange > PageWidth || YStretchedRange > PageHeight)
      {
        SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_HAND1)));
        return TRUE;
      }
    }
    if (Loading)
    {
      SetCursor(LoadCursor(NULL, IDC_WAIT));
      return TRUE;
    }*/
    }
    //#ifndef ENABLE_TOOL_TIP
    //  else
    //   if (PipWindow)
    //    ShowWindow(PipWindow, SW_HIDE);
    //#endif
    return FALSE;
}

PVCODE SimplifyImageSequence(LPPVHandle hPVImage, HDC dc, int ScreenWidth, int ScreenHeight, LPPVImageSequence& pSequence, const COLORREF& bgColor)
{
    HDC hMemDC, hMemDC2;
    HBITMAP hMemBmp, hOldBmp, hTargetBmp;
    DWORD PutType;
    RECT rct, rctFull;
    HBRUSH hBr;
    int dispMethod = PVDM_UNDEFINED;
    LPPVImageSequence pSeq = pSequence;

    hMemBmp = CreateCompatibleBitmap(dc, ScreenWidth, ScreenHeight);
    hMemDC = CreateCompatibleDC(dc);
    SelectObject(hMemDC, hMemBmp);

    rct.left = rct.top = 0;
    rct.right = ScreenWidth;
    rct.bottom = ScreenHeight;
    rctFull = rct;

    //   hBr = CreateSolidBrush(pvii.FSI->GIF.BgColor);
    hBr = CreateSolidBrush(bgColor);
    FillRect(hMemDC, &rct, hBr); // image area

    hMemDC2 = CreateCompatibleDC(hMemDC);
    while (pSeq)
    {
        // The GIF89a spec doesn't say anything
        // GIF Construction Set help say dispMethod should be applied AFTER
        // MSIE, NN, Irfan do apply it AFTER
        // XnView (and PictView until 2004.05.24) applies it BEFORE
        // 2009.12.08: odrazka.gif: fill only the area of the subimage
        if ((dispMethod == PVDM_BACKGROUND) || (dispMethod == PVDM_PREVIOUS) /*|| (dispMethod == PVDM_UNDEFINED)*/)
        {
            FillRect(hMemDC, &rct, hBr);
        }
        dispMethod = pSeq->DisposalMethod;
        rct = pSeq->Rect;
        rct.right -= rct.left;
        rct.bottom -= rct.top;
        if (pSeq->TransparentHandle)
        {
            hOldBmp = (HBITMAP)SelectObject(hMemDC2, pSeq->TransparentHandle);
            BitBlt(hMemDC, rct.left, rct.top, rct.right, rct.bottom,
                   hMemDC2, 0, 0, SRCAND);
            SelectObject(hMemDC2, hOldBmp);
            PutType = SRCINVERT;
            DeleteObject(pSeq->TransparentHandle);
            pSeq->TransparentHandle = NULL;
        }
        else
        {
            PutType = SRCCOPY;
        }
        hOldBmp = (HBITMAP)SelectObject(hMemDC2, pSeq->ImgHandle);
        BitBlt(hMemDC, rct.left, rct.top, rct.right, rct.bottom,
               hMemDC2, 0, 0, PutType);

        hTargetBmp = CreateCompatibleBitmap(dc, ScreenWidth, ScreenHeight);
        SelectObject(hMemDC2, hTargetBmp);
        BitBlt(hMemDC2, 0, 0, ScreenWidth, ScreenHeight, hMemDC, 0, 0, SRCCOPY);
        rct = pSeq->Rect;
        pSeq->Rect.left = pSeq->Rect.top = 0;
        pSeq->Rect.right = ScreenWidth;
        pSeq->Rect.bottom = ScreenHeight;
        /*      hTargetBmp = CreateCompatibleBitmap(dc, rct.right, rct.bottom);
      SelectObject(hMemDC2, hTargetBmp);
      BitBlt(hMemDC2, 0, 0, rct.right, rct.bottom, hMemDC, rct.left, rct.top, SRCCOPY);*/
        SelectObject(hMemDC2, hOldBmp);
        DeleteObject(pSeq->ImgHandle);
        pSeq->ImgHandle = hTargetBmp;

        pSeq = pSeq->pNext;
    }
    DeleteDC(hMemDC2);
    DeleteObject(hBr);
    DeleteDC(hMemDC);
    DeleteObject(hMemBmp);
    return PVC_OK;
} /* SimplifyImageSequence */

void CRendererWindow::SimplifyImageSequence(HDC dc)
{
    int ScreenWidth, ScreenHeight;
    COLORREF bgColor(GetCOLORREF(G.Colors[Viewer->IsFullScreen() ? vceFSBackground : vceBackground]));

    if (pvii.Format == PVF_GIF)
    {
        ScreenWidth = pvii.FSI->GIF.ScreenWidth;
        ScreenHeight = pvii.FSI->GIF.ScreenHeight;
    }
    else
    {
        ScreenWidth = pvii.Width;
        ScreenHeight = pvii.Height;
    }
    PVW32DLL.SimplifyImageSequence(PVHandle, dc, ScreenWidth, ScreenHeight, PVSequence, bgColor);
    if (PVSequence && PVSequence->pNext)
    {
        // At least 2 images
        SetTimer(HWindow, IMGSEQ_TIMER_ID, PVSequence->Delay /*milliseconds*/, NULL);
    }
}

void CRendererWindow::PaintImageSequence(PAINTSTRUCT* ps, HDC dc, BOOL bTopLeft)
{
    HDC hMemDC;
    HBITMAP hOldBmp;
    int ScreenWidth, ScreenHeight, x, y, w, h;

    DrawCageRect(dc, &SelectRect);
    DrawCageRect(dc, &TmpCageRect);

    if (pvii.Format == PVF_GIF)
    {
        ScreenWidth = pvii.FSI->GIF.ScreenWidth;
        ScreenHeight = pvii.FSI->GIF.ScreenHeight;
    }
    else
    {
        ScreenWidth = pvii.Width;
        ScreenHeight = pvii.Height;
    }
    if (fMirrorHor)
        ScreenWidth = -ScreenWidth;
    if (fMirrorVert)
        ScreenHeight = -ScreenHeight;

    if (!PVCurImgInSeq)
    {
        return;
    }
    hMemDC = CreateCompatibleDC(dc);

    hOldBmp = (HBITMAP)SelectObject(hMemDC, PVCurImgInSeq->ImgHandle);

    if (bTopLeft)
    {
        x = y = 0;
        w = ScreenWidth;
        h = ScreenHeight;
    }
    else
    {
        x = XStart - GetScrollPos(HWindow, SB_HORZ) +
            PVCurImgInSeq->Rect.left * XStretchedRange / ScreenWidth;
        y = YStart - GetScrollPos(HWindow, SB_VERT) +
            PVCurImgInSeq->Rect.top * YStretchedRange / ScreenHeight;
        w = (PVCurImgInSeq->Rect.right - PVCurImgInSeq->Rect.left) * XStretchedRange / ScreenWidth;
        h = (PVCurImgInSeq->Rect.bottom - PVCurImgInSeq->Rect.top) * YStretchedRange / ScreenHeight;
    }
    if (fMirrorHor)
        x += XStretchedRange;
    if (fMirrorVert)
        y += YStretchedRange;

    StretchBlt(dc, x, y, w, h,
               hMemDC, 0, 0, PVCurImgInSeq->Rect.right - PVCurImgInSeq->Rect.left,
               PVCurImgInSeq->Rect.bottom - PVCurImgInSeq->Rect.top, SRCCOPY);

    if (!bTopLeft)
    {
        DrawCageRect(dc, &TmpCageRect);
        DrawCageRect(dc, &SelectRect);
    }

    SelectObject(hMemDC, hOldBmp);

    DeleteDC(hMemDC);
} /* CRendererWindow::PaintImageSequence */

bool CRendererWindow::GetRGBAtCursor(int x, int y, RGBQUAD* pRGB, int* pIndex)
{
    return PVW32DLL.GetRGBAtCursor(PVHandle, pvii.Colors, x, y, pRGB, pIndex);
}

void CRendererWindow::UpdatePipetteTooltip()
{
    HDC dc;
    SIZE size;
    RGBQUAD rgb;
    POINT pos, pos2, posInImg;
    COLORREF rgbRef;

    if (!PipWindow)
    {
        return;
    }
    dc = GetDC(HWindow);
    SelectObject(dc, GetStockObject(ANSI_VAR_FONT));
    GetCursorPos(&pos);
    ScreenToClient(HWindow, &pos);
    rgbRef = GetPixel(dc, pos.x, pos.y);
    rgb = *(RGBQUAD*)&rgbRef;
    pos2.x = pos.x - XStart + GetScrollPos(HWindow, SB_HORZ);
    pos2.y = pos.y - YStart + GetScrollPos(HWindow, SB_VERT);
    ClientToScreen(HWindow, &pos);
    if ((pos2.x >= 0) && (pos2.x < XStretchedRange) &&
        (pos2.y >= 0) && (pos2.y < YStretchedRange))
    {
        int ind;

        posInImg.x = pos2.x * pvii.Width / XStretchedRange;
        posInImg.y = pos2.y * pvii.Height / YStretchedRange;

        if (GetRGBAtCursor(posInImg.x, posInImg.y, &rgb, &ind))
        {
            _stprintf(toolTipText, LoadStr(G.PipetteInHex ? IDS_RGBXY_HEX : IDS_RGBXY),
                      posInImg.x, posInImg.y, rgb.rgbRed, rgb.rgbGreen, rgb.rgbBlue);

            _ASSERTE(_tcslen(toolTipText) < SizeOf(toolTipText));
            if (pvii.Colors <= 256)
            {
                _stprintf(toolTipText + _tcslen(toolTipText), LoadStr(IDS_INDEX), ind);
                _ASSERTE(_tcslen(toolTipText) < SizeOf(toolTipText));
            }
        }
    }
    else
    {
        _stprintf(toolTipText, LoadStr(G.PipetteInHex ? IDS_RGB_HEX : IDS_RGB),
                  rgb.rgbBlue, rgb.rgbGreen, rgb.rgbRed);
    }

    // nechame si poslat WM_MOUSELEAVE pri opusteni okna
    // tato varianta postihuje i rychle vyjeti mysi mimo okno, kdy
    // pri starem zpracovani zustal tooltip zobrazeny
    if (!IsWindowVisible(PipWindow))
    {
        TRACKMOUSEEVENT tme;
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_LEAVE;
        tme.hwndTrack = HWindow;
        _TrackMouseEvent(&tme);
    }

    GetTextExtentPoint32(dc, toolTipText, (int)_tcslen(toolTipText), &size);
    MoveWindow(PipWindow, pos.x + 2, pos.y + 2, size.cx + 7, size.cy + 3, TRUE);
    ShowWindow(PipWindow, SW_SHOW);
    InvalidateRect(PipWindow, NULL, FALSE);
    ReleaseDC(HWindow, dc);
} /* CRendererWindow::UpdatePipetteTooltip */

void CRendererWindow::UpdateInfos()
{
    if (PipWindow)
    {
        UpdatePipetteTooltip();
    }
    Viewer->SetStatusBarTexts();
} /* void CRendererWindow::UpdateInfos */

void CRendererWindow::ClientToPicture(POINT* p)
{
    // j.r. FIXME: Honzo, je tohle OK?
    // mrkni prosim po zbytku kodu, kde jsou tyhle konverze roztahane a preved to na volani tohoto
    if (XStretchedRange == 0 || YStretchedRange == 0)
    {
        p->x = 0;
        p->y = 0;
        return;
    }
    p->x = (p->x - XStart + GetScrollPos(HWindow, SB_HORZ)) * (int)pvii.Width / XStretchedRange;
    p->y = (p->y - YStart + GetScrollPos(HWindow, SB_VERT)) * (int)pvii.Height / YStretchedRange;
}

void CRendererWindow::PictureToClient(POINT* p)
{
    // j.r. FIXME: Honzo, je tohle OK?
    // mrkni prosim po zbytku kodu, kde jsou tyhle konverze roztahane a preved to na volani tohoto
    p->x = (int)(p->x * ((double)XStretchedRange / (double)pvii.Width)) + XStart - GetScrollPos(HWindow, SB_HORZ);
    p->y = (int)(p->y * ((double)YStretchedRange / (double)pvii.Height)) + YStart - GetScrollPos(HWindow, SB_VERT);
}

// orizne obdelnik 'r' obdelnikem left/top/right/bottom
void ClipRect(RECT* r, int left, int top, int right, int bottom)
{
    if (r->left < left)
        r->left = left;
    if (r->right < left)
        r->right = left;
    if (r->top < top)
        r->top = top;
    if (r->bottom < top)
        r->bottom = top;

    if (r->left >= right)
        r->left = right - 1;
    if (r->right >= right)
        r->right = right - 1;
    if (r->top >= bottom)
        r->top = bottom - 1;
    if (r->bottom >= bottom)
        r->bottom = bottom - 1;
}

LRESULT CRendererWindow::OnPaint()
{
    PAINTSTRUCT ps;
    HDC dc;
    HRGN hRgn = NULL;
    struct
    {
        RGNDATAHEADER rdh;
        RECT rects[4];
    } rgnData;

    if ((ImageLoaded || Loading) && !PVSequence /*&& (fMirrorHor || fMirrorVert || (XStretchedRange > 4000) || (YStretchedRange > 4000))*/)
    {
        // Optimize only if having loaded mirrored or huge still image
        hRgn = CreateRectRgn(0, 0, 1, 1);
        if (hRgn)
        {
            int size;

            // GetRegionData must be called before BeginPaint
            GetUpdateRgn(HWindow, hRgn, FALSE);
            size = GetRegionData(hRgn, 0, NULL);
            if (size <= sizeof(rgnData))
            {
                GetRegionData(hRgn, sizeof(rgnData), (LPRGNDATA)&rgnData);
#if defined(DEBUG) || defined(_DEBUG)
                char s[256];
                sprintf(s, "UpdateRgn: #: %u, bnd: (%d,%d)-(%d,%d), 1st: (%d,%d)-(%d,%d), 2nd: (%d,%d)-(%d,%d)\n",
                        rgnData.rdh.nCount, rgnData.rdh.rcBound.left, rgnData.rdh.rcBound.top, rgnData.rdh.rcBound.right, rgnData.rdh.rcBound.bottom,
                        rgnData.rects[0].left, rgnData.rects[0].top, rgnData.rects[0].right, rgnData.rects[0].bottom,
                        rgnData.rects[1].left, rgnData.rects[1].top, rgnData.rects[1].right, rgnData.rects[1].bottom);
                OutputDebugStringA(s);
#endif
            }
            else
            {
                DeleteObject(hRgn);
                hRgn = NULL;
            }
        }
    }

    if (hRgn)
    {
        GetUpdateRect(HWindow, &rgnData.rdh.rcBound, FALSE);
    }
    dc = BeginPaint(HWindow, &ps);
    if (hRgn)
    {
        // NT4: ps.rcPaint seems to be empty if GetUpdateRgn or GetUpdateRect was called!!!
        ps.rcPaint = rgnData.rdh.rcBound;
    }
    //   TRACE_I("paint left="<<ps.rcPaint.left<<" top="<<ps.rcPaint.top<<" right="<<ps.rcPaint.right<<" bottom="<<ps.rcPaint.bottom);

    if (HAreaBrush == NULL)
    {
        HAreaBrush = CreateSolidBrush(GetCOLORREF(G.Colors[Viewer->IsFullScreen() ? vceFSBackground : vceBackground]));
    }

    if (ImageLoaded || Loading)
    {
        if (PVSequence)
        {
            FillRect(dc, &ps.rcPaint, HAreaBrush); // entire window

            PaintImageSequence(&ps, dc, FALSE);
        }
        else
        {
            if (CFGCenterImage)
            {
                RECT r;
                r.left = r.top = 0;
                r.right = ps.rcPaint.right;
                r.bottom = YStart;
                FillRect(dc, &r, HAreaBrush);
                r.top = YStart;
                r.right = XStart;
                r.bottom = ps.rcPaint.bottom;
                FillRect(dc, &r, HAreaBrush);
                r.left = XStart + XStretchedRange;
                r.right = ps.rcPaint.right;
                FillRect(dc, &r, HAreaBrush);
                r.top = YStart + YStretchedRange;
                r.right = r.left;
                r.left = XStart;
                FillRect(dc, &r, HAreaBrush);
            }

            if (!hRgn)
            {
                rgnData.rdh.nCount = 1;
                rgnData.rects[0] = ps.rcPaint;
            }
            DWORD i;
            for (i = 0; i < rgnData.rdh.nCount; i++)
            {
                int X = XStart - GetScrollPos(HWindow, SB_HORZ);
                int Y = YStart - GetScrollPos(HWindow, SB_VERT);
                // Help optimize drawing, needed only when using the PVEXEWrapper
                if (X + XStretchedRange < rgnData.rects[i].right)
                {
                    rgnData.rects[i].right = X + XStretchedRange;
                }
                if (Y + YStretchedRange < rgnData.rects[i].bottom)
                {
                    rgnData.rects[i].bottom = Y + YStretchedRange;
                }
                if (X > rgnData.rects[i].left)
                {
                    rgnData.rects[i].left = X;
                }
                if (Y > rgnData.rects[i].top)
                {
                    rgnData.rects[i].top = Y;
                }
                PVW32DLL.PVDrawImage(PVHandle, dc, X, Y, &rgnData.rects[i]);
            }
        }
        if (bDrawCageRect)
        {
            // Selection rectangle is not painted now if we're just scrolling
            DrawCageRect(dc, &SelectRect);
            DrawCageRect(dc, &TmpCageRect);
        }
    }
    else
    {
        FillRect(dc, &ps.rcPaint, HAreaBrush);

        if ((PVHandle != NULL) && !DoNotAttempToLoad)
        {
            PVCODE code;
            HCURSOR oldCursor = NULL;
            if (!HiddenCursor)
            {
                oldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            }

            DoNotAttempToLoad = TRUE;
            Canceled = FALSE;
            Loading = TRUE;
            Viewer->UpdateEnablers(); // po nastaveni Loading promenne se zmeni Fullscreen enabler
            Viewer->UpdateToolBar();
            LoadingDC = dc;
            XStartLoading = XStart;
            YStartLoading = YStart;
            if (pvii.Flags & PVFF_IMAGESEQUENCE)
            {
                code = PVW32DLL.PVReadImageSequence(PVHandle, &PVSequence);
                if (code == PVC_OK)
                {
                    SimplifyImageSequence(dc);
                    PVCurImgInSeq = PVSequence;
                    PaintImageSequence(&ps, dc, FALSE);
                }
            }
            else
            {
                BOOL bAlreadyLoaded = FALSE;
                RECT drect;

                GetClientRect(HWindow, &drect);
                drect.left = XStart;
                drect.top = YStart;
                drect.right = min(drect.right, XStart + XStretchedRange);
                drect.bottom = min(drect.bottom, YStart + YStretchedRange);

                // Note: Some raw formats have PVF_JPG but "TIFF" in Info2, some have PVF_TIFF
                if (FileName != NULL && ((pvii.Format == PVF_JPG) || (pvii.Format == PVF_TIFF)) &&
                    (pvii.Flags & PVFF_EXIF) && G.AutoRotate && InitEXIF(NULL, TRUE))
                {
                    EXIFGETORIENTATIONINFO getInfo = (EXIFGETORIENTATIONINFO)GetProcAddress(EXIFLibrary, "EXIFGetOrientationInfo");

                    if (getInfo)
                    {
                        SThumbExifInfo info;
                        int cmd;

                        getInfo(FileName, &info);
                        if ((info.flags & (TEI_WIDTH | TEI_HEIGHT)) == (TEI_WIDTH | TEI_HEIGHT))
                        {
                            if (((DWORD)info.Width != pvii.Width) || ((DWORD)info.Height != pvii.Height) || (info.Width < info.Height))
                            {
                                // the file has been modified (most likely rotated in something buggy
                                // like UfonView or cropped)
                                // Normal situation is info.Width > info.Height. Some cameras (namely Konica Minolta DiMAGE Z3
                                // seem to store image data with normal orientatation but (Orient != 1) && (info.Width < info.Height)
                                info.Orient = 1;
                            }
                        }
                        else if ((pvii.Format == PVF_TIFF) || !strcmp(pvii.Info1, "TIFF"))
                        {
                            switch (pvii.Flags & (PVFF_BOTTOMTOTOP | PVFF_FLIP_HOR | PVFF_ROTATE90))
                            {
                            case PVFF_BOTTOMTOTOP | PVFF_FLIP_HOR | PVFF_ROTATE90:
                                info.Orient = 8;
                                break; // CMD_ROTATE_LEFT
                            case PVFF_BOTTOMTOTOP | PVFF_FLIP_HOR:
                                info.Orient = 3;
                                break; // CMD_ROTATE180
                            case PVFF_ROTATE90:
                                info.Orient = 6;
                                break; // CMD_ROTATE_RIGHT
                            }
                        }
                        cmd = 0;
                        switch (info.Orient)
                        {
                            //                  case 1/*normal case*/"
                        case 2:
                            cmd = CMD_MIRROR_HOR;
                            break;
                        case 3:
                            cmd = CMD_ROTATE180;
                            break;
                        case 4:
                            cmd = CMD_MIRROR_HOR;
                            break;
                        case 5:
                            cmd = CMD_ROTATE_LEFT;
                            break; // and mirror ?
                        case 6:
                            cmd = CMD_ROTATE_RIGHT;
                            break; // tested
                        case 7:
                            cmd = CMD_ROTATE_RIGHT;
                            break; // and mirror ?
                        case 8:
                            cmd = CMD_ROTATE_LEFT;
                            break; // tested
                        }
                        if (cmd)
                        {
                            Viewer->InitProgressBar();
                            code = PVW32DLL.PVReadImage2(PVHandle, NULL, NULL, ProgressProcedure2 /*NULL*/, this, pvii.CurrentImage);
                            Viewer->KillProgressBar();
                            Viewer->SetStatusBarTexts(IDS_SB_AUTOROTATING);
                            bAlreadyLoaded = TRUE;
                            if (PVC_OK == code)
                            {
                                PostMessage(HWindow, WM_COMMAND, cmd, 0);
                            }
                        }
                    }
                }
                if (!bAlreadyLoaded)
                {
                    code = PVW32DLL.PVReadImage2(PVHandle, dc, &drect, ProgressProcedure /*NULL*/, this, pvii.CurrentImage);
                }
            }

            // prisla nam padacka, kdy se pristupovalo na neinicializovanou promennou oldCursor, takze se behem cteni obrazku musela zmenit promenna HiddenCursor
            // radeji nyni oldCursor inicializuji a testuji
            if (!HiddenCursor && oldCursor != NULL)
            {
                SetCursor(oldCursor);
            }

            // PictView STRESS TEST (po nacteni obrazku se zacne nacitat dalsi)
            // PostMessage(HWindow, WM_COMMAND, CMD_FILE_NEXT, 0);

            Loading = FALSE; // nastavime, ze mame nacteno a na Esc zavreme okno
            if ((code != PVC_OK) && (code != PVC_CANCELED))
            {
                SalamanderGeneral->SalMessageBox(HWindow, PVW32DLL.PVGetErrorText(code),
                                                 LoadStr(IDS_ERROR_CALLING_PVW32_DLL), MB_ICONEXCLAMATION);
                if (code == PVC_OUT_OF_MEMORY)
                { /* Image decompressed partially, let's display it */
                    ImageLoaded = TRUE;
                    TryEnterHandToolMode();
                    // enable toolbar buttons
                    PostMessage(GetParent(HWindow), WM_USER_INITMENUPOPUP, 0, 0);
                }
                else
                {
                    if (PipWindow)
                    { // quit the pipette tool, active from previous page
                        ShutdownTool();
                        SelectTool(RT_HAND);
                    }
                }
            }
            else
            {
                ImageLoaded = TRUE; //kdyz se to nepovede, ma cenu se pokouset znovu????
                TryEnterHandToolMode();
                // enable toolbar buttons
                PostMessage(GetParent(HWindow), WM_USER_INITMENUPOPUP, 0, 0);
            }
            //Soubor stejne celou dobu mame otevreny, nikdo nam ho nemuze zmenit....
        }
    }
    EndPaint(HWindow, &ps);
    if (hRgn)
    {
        DeleteObject(hRgn);
    }
    return 0;
} /* CRendererWindow::OnPaint */

#define WM_MOUSEHWHEEL 0x020E

LRESULT CRendererWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CViewerWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

#ifdef ENABLE_TWAIN32
    if (Viewer->Twain != NULL && Viewer->Twain->GetModalUI())
    {
        // pokud je aktivni UI scanneru, zatluceme nasi funkcionalitu
        if (uMsg != WM_SIZE && uMsg != WM_PAINT)
            return CWindow::WindowProc(uMsg, wParam, lParam);
    }
#endif // ENABLE_TWAIN32

    if (uMsg == WM_MOUSEWHEEL)
    {
        int cnt = (signed short int)HIWORD(wParam);
        DWORD scrollLines = (wParam & MK_SHIFT) ? SalamanderGeneral->GetMouseWheelScrollChars() : SalamanderGeneral->GetMouseWheelScrollLines(); // 'scrollLines' muze byt az WHEEL_PAGESCROLL(0xffffffff)
        scrollLines = (wParam & MK_SHIFT) ? min(scrollLines, (DWORD)(PageHeight / YLine)) : min(scrollLines, (DWORD)(PageWidth / XLine));
        cnt *= scrollLines;
        BOOL bUpdateSB = FALSE;
        if (cnt < 0)
        {
            if (wParam & MK_SHIFT)
            {
                while (cnt < 0)
                {
                    SendMessage(HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
                    cnt += WHEEL_DELTA;
                }
                bUpdateSB = TRUE;
            }
            else if (wParam & MK_CONTROL)
            {
                ZoomOut();
                bUpdateSB = TRUE;
            }
            else
            {
                if (G.PageDnUpScrolls)
                {
                    while (cnt < 0)
                    {
                        SendMessage(HWindow, WM_VSCROLL, SB_LINEDOWN, 0);
                        cnt += WHEEL_DELTA;
                    }
                    bUpdateSB = TRUE;
                }
                else
                {
                    SendMessage(HWindow, WM_COMMAND, CMD_FILE_NEXT, 0);
                }
            }
        }
        else
        {
            if (wParam & MK_SHIFT)
            {
                while (cnt > 0)
                {
                    SendMessage(HWindow, WM_HSCROLL, SB_LINEUP, 0);
                    cnt -= WHEEL_DELTA;
                }
                bUpdateSB = TRUE;
            }
            else if (wParam & MK_CONTROL)
            {
                ZoomIn();
                bUpdateSB = TRUE;
            }
            else
            {
                if (G.PageDnUpScrolls)
                {
                    while (cnt > 0)
                    {
                        SendMessage(HWindow, WM_VSCROLL, SB_LINEUP, 0);
                        cnt -= WHEEL_DELTA;
                    }
                    bUpdateSB = TRUE;
                }
                else
                {
                    SendMessage(HWindow, WM_COMMAND, CMD_FILE_PREV, 0);
                }
            }
        }
        if (bUpdateSB)
        {
            PostMessage(HWindow, WM_MOUSEMOVE, 0, 0); // update SB
        }
        return 0;
    }

    if (uMsg == WM_MOUSEHWHEEL) // horizontall scroll, supported from Windows Vista
    {
        short zDelta = (short)HIWORD(wParam);
        DWORD scrollChars = SalamanderGeneral->GetMouseWheelScrollChars(); // can be also WHEEL_PAGESCROLL (0xffffffff) for page scroll
        if (scrollChars >= (DWORD)(PageWidth / XLine))
        {
            SendMessage(HWindow, WM_HSCROLL, zDelta < 0 ? SB_PAGEUP : SB_PAGEDOWN, 0);
        }
        else
        {
            DWORD i;
            for (i = 0; i < scrollChars; i++)
                SendMessage(HWindow, WM_HSCROLL, zDelta < 0 ? SB_LINEUP : SB_LINEDOWN, 0);
        }
        PostMessage(HWindow, WM_MOUSEMOVE, 0, 0); // update SB
        return 0;
    }

    if (uMsg == WM_MOUSEMOVE ||
        uMsg == WM_LBUTTONDOWN ||
        uMsg == WM_MBUTTONDOWN ||
        uMsg == WM_RBUTTONDOWN ||
        uMsg == WM_NCMOUSEMOVE ||
        uMsg == WM_NCLBUTTONDOWN ||
        uMsg == WM_NCMBUTTONDOWN ||
        uMsg == WM_NCRBUTTONDOWN)
    {
        if (HiddenCursor)
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }
        else if (Viewer->IsFullScreen())
            LastMoveTickCount = GetTickCount();
    }

    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_PAINT:
    {
        return OnPaint();
    }

    case WM_SYSCOLORCHANGE:
    {
        // tady by se mely premapovat barvy
        //      TRACE_I("CViewerWindow::WindowProc - WM_SYSCOLORCHANGE");
        break;
    }

    case WM_USER_INITMENUPOPUP:
    {
        return SendMessage(Viewer->HWindow, uMsg, wParam, lParam);
    }

    case WM_USER_VIEWERCFGCHNG:
    {
        // tady by se mely projevit zmeny v konfiguraci pluginu
        //      TRACE_I("CViewerWindow::WindowProc - config has changed");
        //      SalamanderGeneral->SalMessageBox(HWindow, "Viewer cfg changed","Co se deje??",0);
        if (HAreaBrush != NULL)
        {
            DeleteObject(HAreaBrush);
            HAreaBrush = NULL;
        }
        if (PVHandle)
        {
            PVW32DLL.PVSetBkHandle(PVHandle, GetCOLORREF(G.Colors[Viewer->FullScreen ? vceFSTransparent : vceTransparent]));
            SetTitle(); // add/remove full path from title
        }
        InvalidateRect(HWindow, NULL, TRUE);
        return 0;
    }

    case WM_USER_SAVEAS_INTERNAL:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        if (!Loading && ImageLoaded)
        {
            OnFileSaveAs((LPTSTR)lParam);
        }
        if (lParam)
            free((LPTSTR)lParam);
        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case WM_USER_ZOOM:
    {
        ZoomTo((int)wParam);
        return 0;
    }

    case WM_CREATE:
    {
        LastMoveTickCount = GetTickCount();
        SetTimer(HWindow, CURSOR_TIMER_ID, 200, NULL);
        SetTimer(HWindow, BRUSH_TIMER_ID, 100, NULL);
        DragAcceptFiles(HWindow, TRUE);
        break;
    }

    case WM_DESTROY:
    {
        KillTimer(HWindow, BRUSH_TIMER_ID);
        KillTimer(HWindow, CURSOR_TIMER_ID);
        KillTimer(HWindow, SAVEAS_TIMER_ID);
        if (PVSequence)
        {
            KillTimer(HWindow, IMGSEQ_TIMER_ID);
        }
        DragAcceptFiles(HWindow, FALSE);
        if (Loading) //Ooops. Soneone is closing our window, but we are still decompresing!
        {
            Canceled = TRUE;
            break; // j.r. overit tuto cestu
        }
        if (PVHandle != NULL)
        {
            PVW32DLL.PVCloseImage(PVHandle);
            PVHandle = NULL;
            PVSequence = NULL;
        }
        break;
    }

    case WM_SIZE:
    {
        GetClientRect(HWindow, &ClientRect);
        if (Capturing) // bylo kliknuto na minimalizovane okno v capture rezimu
        {
            CancelCapture();
            PostMessage(HWindow, WM_COMMAND, CMD_CAPTURE_CANCELED, 0);
        }
        WMSize();
        if (Loading)
        {
            SetViewportOrgEx(LoadingDC, XStart - XStartLoading - GetScrollPos(HWindow, SB_HORZ),
                             YStart - YStartLoading - GetScrollPos(HWindow, SB_VERT), NULL);
            InvalidateRect(HWindow, NULL, TRUE);
        }
        else
            InvalidateRect(HWindow, NULL, FALSE);
        break;
    }

    case WM_VSCROLL:
    {
        if ((YStretchedRange > 65535) && ((LOWORD(wParam) == SB_THUMBPOSITION) || (LOWORD(wParam) == SB_THUMBTRACK)))
        {
            // We need 32-bit accurary
            SCROLLINFO si;

            si.fMask = SIF_TRACKPOS;
            si.cbSize = sizeof(SCROLLINFO);
            GetScrollInfo(HWindow, SB_VERT, &si);
            VScroll(LOWORD(wParam), si.nTrackPos /*intermediate position*/);
        }
        else
        {
            VScroll(LOWORD(wParam), HIWORD(wParam));
        }
        /*      if ((VScroll(LOWORD(wParam), HIWORD(wParam)) != 0) && Loading)
      {
        SetViewportOrgEx(LoadingDC, XStart-XStartLoading-GetScrollPos(HWindow, SB_HORZ),
         YStart-YStartLoading-GetScrollPos(HWindow, SB_VERT), NULL);
        InvalidateRect(HWindow, NULL, TRUE);
      }*/
        break;
    }

    case WM_HSCROLL:
    {
        if ((XStretchedRange > 65535) && ((LOWORD(wParam) == SB_THUMBPOSITION) || (LOWORD(wParam) == SB_THUMBTRACK)))
        {
            // We need 32-bit accurary
            SCROLLINFO si;

            si.fMask = SIF_TRACKPOS;
            si.cbSize = sizeof(SCROLLINFO);
            GetScrollInfo(HWindow, SB_HORZ, &si);
            HScroll(LOWORD(wParam), si.nTrackPos /*intermediate position*/);
        }
        else
        {
            HScroll(LOWORD(wParam), HIWORD(wParam));
        }
        /*      if ((HScroll(LOWORD(wParam), HIWORD(wParam)) !=0) && Loading)
      {
        SetViewportOrgEx(LoadingDC, XStart-XStartLoading-GetScrollPos(HWindow, SB_HORZ),
         YStart-YStartLoading-GetScrollPos(HWindow, SB_VERT), NULL);
        InvalidateRect(HWindow, NULL, TRUE);
      }*/
        break;
    }

    case WM_DROPFILES:
    {
        UINT drag;
        TCHAR path[MAX_PATH];

        drag = DragQueryFile((HDROP)wParam, 0xFFFFFFFF, NULL, 0); // kolik souboru nam hodili
        // tenhle kod otevre vsechny soubory - je to kod z textoveho editoru,
        // ktery jsme s Petrem psali
        // pro PictView by to asi chtelo vybrat ze souboru prvni, a ten otevrit
        // na ostatni se vykaslat
        if (drag > 0)
        {
            DragQueryFile((HDROP)wParam, 0, path, MAX_PATH);
            EnumFilesSourceUID = -1;
            //Je-li puvodni image stale nacitan, jsme ted volani
            //z ProgressProcedure a proto nemuzeme ted zahajit nove cteni
            //nejprve musime odcancelovat stavajici a odlozit novy
            if (Loading)
            {
                Canceled = TRUE;
                SalamanderGeneral->Free(FileName);
                FileName = SalamanderGeneral->DupStr(path);
                PostMessage(HWindow, WM_COMMAND, CMD_RELOAD, 0);
            }
            else
            {
                OpenFile(path, -1, NULL);
            }
        }
        DragFinish((HDROP)wParam);
        break;
    }

    case WM_COMMAND:
    {
        if (GetCapture() != NULL) // pokud probiha nejaka operace tazeni, nepustime commandy
            return 0;
        BOOL closingViewer = FALSE;
        LRESULT res = OnCommand(wParam, lParam, &closingViewer);
        if (!closingViewer && // chystame-li se zavrit okno (uz je postnuty WM_CLOSE), nebudeme se tu zdrzovat enablery
            HWindow != NULL)  // pri zavreni z jineho threadu (shutdown / zavreni hl. okna Salama) uz okno neexistuje
        {
            Viewer->UpdateEnablers();
            Viewer->UpdateToolBar();
        }
        return res;
    }

    case WM_HOTKEY:
    {
        if (wParam == G.CaptureAtomID)
        {
            ScreenCapture();
        }
        return 0;
    }

    case WM_TIMER:
    {
        switch (wParam)
        {
        case CURSOR_TIMER_ID:
            if (Viewer->IsFullScreen() && CanHideCursor)
            {
                DWORD tickCount = GetTickCount();
                if (tickCount - LastMoveTickCount > 3000)
                {
                    // 3000 odpovida trem vterinam, po kterych zhasnem kurzor
                    HiddenCursor = TRUE;
                    SetCursor(NULL);
                }
            }
            break;
        case CAPTURE_TIMER_ID:
            ScreenCapture();
            break;
        case BRUSH_TIMER_ID:
            DrawCageRect(NULL, &TmpCageRect);
            DrawCageRect(NULL, &SelectRect);
            BrushOrg++;
            if (BrushOrg >= 8)
                BrushOrg = 0;
            DrawCageRect(NULL, &TmpCageRect);
            DrawCageRect(NULL, &SelectRect);
            break;
        case SCROLL_TIMER_ID:
        {
            POINT pt;
            GetCursorPos(&pt);
            ScreenToClient(HWindow, &pt);
            BOOL dirty = FALSE;
            if (pt.y < 0)
            {
                PostMessage(HWindow, WM_VSCROLL, SB_LINEUP, 0);
                dirty = TRUE;
            }
            if (pt.y > ClientRect.bottom)
            {
                PostMessage(HWindow, WM_VSCROLL, SB_LINEDOWN, 0);
                dirty = TRUE;
            }
            if (pt.x < 0)
            {
                PostMessage(HWindow, WM_HSCROLL, SB_LINEUP, 0);
                dirty = TRUE;
            }
            if (pt.x > ClientRect.right)
            {
                PostMessage(HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
                dirty = TRUE;
            }
            if (dirty)
            {
                // update klece
                POINT p;
                GetCursorPos(&p);
                ScreenToClient(HWindow, &p);
                PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
            }
            break;
        }
        case IMGSEQ_TIMER_ID:
        {
            KillTimer(HWindow, IMGSEQ_TIMER_ID);
            // redraw image sequence
            HDC hDC = GetDC(HWindow);

            PVCurImgInSeq = PVCurImgInSeq->pNext;
            if (!PVCurImgInSeq)
            {
                PVCurImgInSeq = PVSequence;
            }
            PaintImageSequence(NULL, hDC, FALSE);
            ReleaseDC(HWindow, hDC);
            SetTimer(HWindow, IMGSEQ_TIMER_ID, PVCurImgInSeq->Delay /*milliseconds*/, NULL);
            break;
        }
        case SAVEAS_TIMER_ID:
        {
            // Salamander is busy and doesn't tell us what the current directory is
            KillTimer(HWindow, SAVEAS_TIMER_ID);
            if (ghSaveAsWindow && (ghSaveAsWindow == HWindow))
            {
                TRACE_E("SAVEAS_TIMER_ID fired - Salamander busy?");
                // Still waiting in our window...
                PostMessage(HWindow, WM_USER_SAVEAS_INTERNAL, 0, 0);
            }
            ghSaveAsWindow = NULL;
            break;
        }
        break;
        }
        return 0;
    }

    case WM_RBUTTONDOWN:
    {
        POINT p;
        GetCursorPos(&p);
        OnContextMenu(&p);
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL firstDown = (lParam & 0x40000000) == 0; // bit 30: Specifies the previous key state.
        // The value is 1 if the key is down before the message is sent, or it is zero if the key is up.
        if (shiftPressed && !ShiftKeyDown && (GetCapture() == HWindow) && (CurrTool == RT_SELECT))
        {
            // pokud doslo ke stisteni (nebo uvolneni:KEYUP) SHIFT, nechame prekreslit selection
            ShiftKeyDown = TRUE;
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);
            PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        }
        if ((wParam == VK_F10) && shiftPressed || (wParam == VK_APPS))
        {
            POINT p;
            p.x = 0;
            p.y = 0;
            ClientToScreen(HWindow, &p);
            OnContextMenu(&p);
            return 0;
        }

        if (wParam == VK_SPACE)
        {
            // pokud drzime SPACE jeste z doby, kdy jsme tahli klec, bude firstDown==FALSE
            // a prechod na dalsi obrazek se neprovede
            // !IsCageValid(&TmpCageRect) needed fow fast-forward while the spacebar is kept down
            if (!GetCapture() && (firstDown || !IsCageValid(&TmpCageRect)))
            {
                int cmd = CMD_FILE_NEXT;
                if (controlPressed && !shiftPressed)
                    cmd = CMD_FILE_NEXTSELFILE;
                if (!controlPressed && shiftPressed)
                    cmd = CMD_FILE_LAST;
                PostMessage(Viewer->HWindow, WM_COMMAND, MAKEWPARAM(cmd, 0), 0);
            }
        }

        if (wParam == VK_ESCAPE)
        {
            if (GetCapture() == NULL)
            {
                PostMessage(Viewer->HWindow, WM_COMMAND, MAKEWPARAM(CMD_CLOSE, 0), 0);
            }
            else
            {
                ReleaseCapture();
                PostMessage(HWindow, WM_CANCELMODE, 0, 0);
            }
        }

        // pokud je kurzor nad oknem, nechame nastavit kurzor
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(HWindow, &p);
        if (PtInRect(&ClientRect, p))
            OnSetCursor(HTCLIENT);
        break;
    }

    case WM_SYSKEYUP:
    case WM_KEYUP:
    {
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (!shiftPressed && ShiftKeyDown && GetCapture() == HWindow)
        {
            // pokud doslo ke stisteni (nebo uvolneni:KEYUP) SHIFT, nechame prekreslit selection
            ShiftKeyDown = FALSE;
            POINT p;
            GetCursorPos(&p);
            ScreenToClient(HWindow, &p);
            PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        }
        // pokud je kurzor nad oknem, nechame nastavit kurzor
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(HWindow, &p);
        if (PtInRect(&ClientRect, p))
            OnSetCursor(HTCLIENT);
        break;
    }

    case WM_LBUTTONDBLCLK:
        if (ImageLoaded && ((CurrTool == RT_HAND) || (CurrTool == RT_PIPETTE)) && !(wParam & (MK_CONTROL | MK_SHIFT)))
        {
            Viewer->ToggleFullScreen();
        }
        break;

    case WM_LBUTTONDOWN:
    {
        if (ImageLoaded)
        {
            BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
            BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            switch (CurrTool)
            {
            case RT_HAND:
            {
                if (!altPressed && (controlPressed ^ shiftPressed))
                {
                    ZoomOnTheSpot(lParam, controlPressed);
                    break;
                }
                if (((XStretchedRange > PageWidth) || (YStretchedRange > PageHeight)))
                {
                    OldMousePos = GetMessagePos();
                    SetCapture(HWindow);
                    HOldCursor = SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_HAND2)));
                }
                break;
            }

            case RT_PIPETTE:
                if (!altPressed && (controlPressed ^ shiftPressed))
                {
                    ZoomOnTheSpot(lParam, controlPressed);
                    break;
                }
                break;

            case RT_ZOOM:
            {
                if (controlPressed && !altPressed && !shiftPressed)
                {
                    ZoomIn();
                    break;
                }
                if (!controlPressed && !altPressed && shiftPressed)
                {
                    ZoomOut();
                    break;
                }
                // propadneme do RT_SELECT pro vyber klece
            }
            case RT_SELECT:
            {
                if (CurrTool == RT_SELECT)
                {
                    // pokud existovala selection, sestrelime ji
                    DrawCageRect(NULL, &SelectRect);
                    InvalidateCage(&SelectRect);
                }

                // vyber zacneme az po utrzeni (drag&drop min limit)
                LButtonDown.x = LOWORD(lParam);
                LButtonDown.y = HIWORD(lParam);
                BeginDragDrop = TRUE;

                // ulozime si pocatek vuberove klece
                CageAnchor = LButtonDown;
                ClientToPicture(&CageAnchor);
                CageCursor = CageAnchor;

                // na hranicich obrazovky zajistime scroll okna
                SetTimer(HWindow, SCROLL_TIMER_ID, 25, NULL);
                ScrollTimerIsRunning = TRUE;

                // chceme vsechny zpravy
                SetCapture(HWindow);
                break;
            }
            } // switch (CurrTool)

            /*
        if (GetAsyncKeyState(VK_CONTROL) & 0x8000) ZoomIn();
        else
        if (GetAsyncKeyState(VK_MENU) & 0x8000) ZoomOut();
        else
        {
          if (CurrTool == RT_HAND &&
              ((XStretchedRange > PageWidth) || (YStretchedRange > PageHeight)))
          {
            OldMousePos = GetMessagePos();
            SetCapture(HWindow);
            HOldCursor = SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_HAND2)));
          }
        }
        */
        }
        break;
    }

    case WM_CANCELMODE:
    case WM_LBUTTONUP:
    {
        BOOL canceled = (uMsg != WM_LBUTTONUP);
        if (GetCapture() == HWindow)
            ReleaseCapture();
        if (ScrollTimerIsRunning)
        {
            KillTimer(HWindow, SCROLL_TIMER_ID);
            ScrollTimerIsRunning = FALSE;
        }
        if (CurrTool == RT_SELECT)
        {
            if (canceled)
                DrawCageRect(NULL, &TmpCageRect); // zhasneme pracovni klec
            else
            {
                NormalizeRect(&TmpCageRect); // SelectRect chceme mit spravne orientovany pro nasledne transformace (otaceni, flipovani)
                SelectRect = TmpCageRect;    // pracovni klec konvertujeme na selection
            }
            InvalidateCage(&TmpCageRect);
        }
        if ((CurrTool == RT_ZOOM) && ImageLoaded)
        {
            BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

            DrawCageRect(NULL, &TmpCageRect); // zhasneme pracovni klec
            NormalizeRect(&TmpCageRect);

            // Ignore if SHIFT: already handled in WM_LBUTTONDOWN
            if (!canceled && !shiftPressed)
            {
                POINT pt;
                RECT r;

                if (BeginDragDrop || !IsCageValid(&TmpCageRect))
                {
                    // nedoslo k natazeni klece, jde o klasicky zoom
                    // 2 fast click at the same pt: BeginDragDrop is false & TmpCageRect is invalid
                    ZoomOnTheSpot(lParam, TRUE /*bZoomIn*/);
                }
                else
                {
                    double zoom, zoom2;

                    pt.x = (TmpCageRect.left + TmpCageRect.right) / 2;
                    pt.y = (TmpCageRect.top + TmpCageRect.bottom) / 2;
                    GetClientRect(HWindow, &r);
                    zoom = (double)r.right / (TmpCageRect.right - TmpCageRect.left + 1);
                    zoom2 = (double)r.bottom / (TmpCageRect.bottom - TmpCageRect.top + 1);
                    if (zoom2 < zoom)
                        zoom = zoom2;
                    LockWindowUpdate(HWindow);
                    // zoom times 100 percent times our precision (100 - 2 dec. digits)
                    ZoomTo((int)(zoom * 100 * 100));
                    PictureToClient(&pt);
                    // prevent still unscaled image from scrolling before being
                    // scaled & painted
                    HScroll(SB_THUMBPOSITION, GetScrollPos(HWindow, SB_HORZ) + pt.x - r.right / 2);
                    VScroll(SB_THUMBPOSITION, GetScrollPos(HWindow, SB_VERT) + pt.y - r.bottom / 2);
                    // NT4: Desktop icons may get repainted as a response to this
                    // LockWindowUpdate(NULL) if a zoomed image is cached by PVW32Cnv.dll
                    LockWindowUpdate(NULL);
                }
            }
            InvalidateCage(&TmpCageRect);
        }
        BeginDragDrop = FALSE;
        Viewer->UpdateEnablers();
        break;
    }

        // j.r. tohle jsem musel vyhodit, protoze pri zoomovani problikavala ruka (pokud se predtim pouzila)
        // k cemu to tu bylo???
        //    case WM_CAPTURECHANGED:
        //    {
        //      SetCursor(HOldCursor);
        //      break;
        //    }

    case WM_SETCURSOR:
    {
        if (OnSetCursor((DWORD)lParam))
            return TRUE;
        break;
    }

#ifndef ENABLE_TOOL_TIP
    case WM_MOUSELEAVE:
    {
        if (PipWindow)
            ShowWindow(PipWindow, SW_HIDE);
        break;
    }
#endif

    case WM_MOUSEMOVE:
    {
        if ((CurrTool == RT_HAND) && GetCapture() == HWindow)
        {
            DWORD newMousePos = GetMessagePos();
            int deltaX = GET_X_LPARAM(newMousePos) - GET_X_LPARAM(OldMousePos);
            int deltaY = GET_Y_LPARAM(newMousePos) - GET_Y_LPARAM(OldMousePos);
            OldMousePos = newMousePos;

            int scrollDeltaX = 0;
            int scrollDeltaY = 0;

            // je-li prislusna scrollbara zobrazena, napocitam deltu pro scroll
            if (XStretchedRange > PageWidth)
            {
                int xPos, oldXPos;
                xPos = oldXPos = GetScrollPos(HWindow, SB_HORZ);
                xPos -= deltaX;
                if (xPos < 0)
                    xPos = 0;
                if (xPos > XStretchedRange - PageWidth)
                    xPos = XStretchedRange - PageWidth;
                if (xPos != oldXPos)
                {
                    DrawCageRect(NULL, &SelectRect);
                    SetScrollPos(HWindow, SB_HORZ, xPos, TRUE); //nastavime pozici scrolleru
                    scrollDeltaX = oldXPos - xPos;
                }
            }

            if (YStretchedRange > PageHeight)
            {
                int yPos, oldYPos;
                yPos = oldYPos = GetScrollPos(HWindow, SB_VERT);
                yPos -= deltaY;
                if (yPos < 0)
                    yPos = 0;
                if (yPos > YStretchedRange - PageHeight)
                    yPos = YStretchedRange - PageHeight;
                if (yPos != oldYPos)
                {
                    if (!scrollDeltaX)
                    {
                        // not yet called because there is no horizontal scroll needed
                        DrawCageRect(NULL, &SelectRect);
                    }
                    SetScrollPos(HWindow, SB_VERT, yPos, TRUE); //nastavime pozici scrolleru
                    scrollDeltaY = oldYPos - yPos;
                }
            }

            if (scrollDeltaX != 0 || scrollDeltaY != 0)
            {
                //          ScrollWindow(HWindow, scrollDeltaX, scrollDeltaY, NULL, NULL);//Some people prefer ScrollWindowEx
                ScrollWindowEx(HWindow, scrollDeltaX, scrollDeltaY, NULL, NULL, NULL, NULL, SW_INVALIDATE); //odscrollujeme okno
                bDrawCageRect = FALSE;
                UpdateWindow(HWindow);
                bDrawCageRect = TRUE;
                DrawCageRect(NULL, &SelectRect);
                Sleep(1);
            }
            break;
        }
        UpdatePipetteTooltip();

        int statusBarID = 0;
        if (((CurrTool == RT_SELECT) || (CurrTool == RT_ZOOM)) && GetCapture() == HWindow)
        {
            BOOL shiftPressed = IsCageValid(&TmpCageRect) && (GetKeyState(VK_SHIFT) & 0x8000) != 0; // SHIFT=ctvercovy vyber
            BOOL spacePressed = IsCageValid(&TmpCageRect) && (GetKeyState(VK_SPACE) & 0x8000) != 0; // SPACE=posun vyberu

            POINT pt;
            pt.x = (short)LOWORD(lParam);
            pt.y = (short)HIWORD(lParam);
            ClientToPicture(&pt); // pt bude v souradnicich obrazku

            if (BeginDragDrop /*&& (wParam & (MK_LBUTTON | MK_RBUTTON))*/) // "trhani" pro drag&drop
            {
                int x = abs(LButtonDown.x - (short)LOWORD(lParam));
                int y = abs(LButtonDown.y - (short)HIWORD(lParam));
                if (x <= GetSystemMetrics(SM_CXDRAG) && y <= GetSystemMetrics(SM_CYDRAG)) // posun
                    break;                                                                // kurzor se dostatecne nevzdalil od pocatku tazeni -- vypadneme
                BeginDragDrop = FALSE;
            }
            else
                DrawCageRect(NULL, &TmpCageRect); // zhasneme predeslou klec

            int selWidth;
            int selHeight;

            if (spacePressed)
            {
                // velikost selection zustane zachovana, posuneme pocatek
                selWidth = CageCursor.x - CageAnchor.x;
                selHeight = CageCursor.y - CageAnchor.y;
                // pozici odchytime od kurzoru, aby se roh selection kryl s krizkem
                CageAnchor.x = pt.x - selWidth;
                CageAnchor.y = pt.y - selHeight;
            }
            else
            {
                // menime velikost selection
                selWidth = pt.x - CageAnchor.x;
                selHeight = pt.y - CageAnchor.y;
            }

            TmpCageRect.left = CageAnchor.x;
            TmpCageRect.top = CageAnchor.y;
            TmpCageRect.right = TmpCageRect.left + selWidth;
            TmpCageRect.bottom = TmpCageRect.top + selHeight;

            // klec nesmi presahnout pres hranici obrazku
            ClipRect(&TmpCageRect, 0, 0, pvii.Width, pvii.Height);

            if (shiftPressed && (CurrTool == RT_SELECT))
            {
                // SHIFT modifikator: chceme vepsany ctverec
                selWidth = TmpCageRect.right - TmpCageRect.left;
                selHeight = TmpCageRect.bottom - TmpCageRect.top;
                int width = abs(selWidth), height = abs(selHeight);

                if (width * G.SelectRatioY / G.SelectRatioX < height)
                {
                    TmpCageRect.right = TmpCageRect.left + width * sgn(selWidth);
                    TmpCageRect.bottom = TmpCageRect.top + (int)((((width + 1) * G.SelectRatioY) / (int)G.SelectRatioX) - 1) * sgn(selHeight);
                }
                else
                {
                    TmpCageRect.right = TmpCageRect.left + (int)((((height + 1) * G.SelectRatioX) / (int)G.SelectRatioY) - 1) * sgn(selWidth);
                    TmpCageRect.bottom = TmpCageRect.top + height * sgn(selHeight);
                }
                if (G.SelectRatioY == G.SelectRatioX)
                {
                    statusBarID = IDS_SB_TIP_SELECT_SQUARE;
                }
            }

            DrawCageRect(NULL, &TmpCageRect); // vykreslime klec
            CageCursor = pt;
        }
        if (!bEatSBTextOnce)
        {
            Viewer->SetStatusBarTexts(statusBarID);
        }
        else
        {
            bEatSBTextOnce = FALSE;
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
} /* CRendererWindow::WindowProc */

LRESULT CALLBACK ToolTipWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE5("ToolTipWindowProc(0x%p, 0x%X, 0x%IX, 0x%IX)", hwnd, uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_CREATE:
    {
        SetWindowLongPtr(hwnd, GWLP_USERDATA, (LONG_PTR)((LPCREATESTRUCT)lParam)->lpCreateParams);
        break;
    }
    case WM_PAINT: // prvni zprava - pripojeni objektu k oknu
    {
        PAINTSTRUCT ps;
        RECT rc;
        HBRUSH brush, oldBrush;
        HPEN pen, oldPen;
        HDC dc = BeginPaint(hwnd, &ps);
        DWORD color = GetSysColor(COLOR_INFOBK),
              color2 = GetSysColor(COLOR_INFOTEXT);
        LPTSTR toolTipText = (LPTSTR)GetWindowLongPtr(hwnd, GWLP_USERDATA);

        SetBkColor(dc, color);
        SelectObject(dc, GetStockObject(ANSI_VAR_FONT));
        brush = CreateSolidBrush(color);
        pen = CreatePen(PS_SOLID, 1, color2);
        GetClientRect(hwnd, &rc);
        //      FillRect(dc, &rc, brush);
        oldBrush = (HBRUSH)SelectObject(dc, brush); //keep the old one
        oldPen = (HPEN)SelectObject(dc, pen);       //keep the old one
        Rectangle(dc, rc.left, rc.top, rc.right, rc.bottom);
        SelectObject(dc, oldPen);   //set the old one
        SelectObject(dc, oldBrush); //set the old one
        DeleteObject(pen);
        DeleteObject(brush);
        SetTextColor(dc, color2);
        if (toolTipText)
            TextOut(dc, /*ttr.left+1, ttr.top+*/ 3, 1, toolTipText, (int)_tcslen(toolTipText));

        EndPaint(hwnd, &ps);
        SetFocus(GetParent(hwnd));
        return 0;
    }
    case WM_MOUSEMOVE:
    {
        ShowWindow(hwnd, SW_HIDE);
        break;
    }
    }

    return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void GetInfo(LPTSTR buffer, HANDLE file)
{
    FILETIME lastWrite;
    GetFileTime(file, NULL, NULL, &lastWrite);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(&lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    TCHAR date[50], time[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        _stprintf(time, _T("%u:%02u:%02u"), st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        _stprintf(date, _T("%u.%u.%u"), st.wDay, st.wMonth, st.wYear);

    TCHAR number[50];
    CQuadWord size;
    DWORD err;
    if (SalamanderGeneral->SalGetFileSize(file, size, err))
        SalamanderGeneral->NumberToStr(number, size);
    else
        number[0] = 0; // chyba - velikost neznama

    _stprintf(buffer, _T("%s, %s, %s"), number, date, time);
}

void MakeValidFileName(TCHAR* path)
{
    // oriznuti mezer na zacatku a mezer a tecek na konci jmena, dela to tak Explorer
    // a lidi tlacili, ze to tak taky chteji, viz https://forum.altap.cz/viewtopic.php?f=16&t=5891
    // a https://forum.altap.cz/viewtopic.php?f=2&t=4210
    TCHAR* n = path;
    while (*n != 0 && *n <= ' ')
        n++;
    if (n > path)
        memmove(path, n, (_tcslen(n) + 1) * sizeof(TCHAR));
    n = path + _tcslen(path);
    while (n > path && (*(n - 1) <= ' ' || *(n - 1) == '.'))
        n--;
    *n = 0;
}

BOOL CRendererWindow::RenameFileInternal(LPCTSTR oldPath, LPCTSTR oldName, TCHAR (&newName)[MAX_PATH], BOOL* tryAgain)
{
    BOOL renamed = FALSE;
    *tryAgain = TRUE;
    LPCTSTR s = newName;
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (newName[0] != 0 && *s == 0)
    {
        TCHAR myOldName[MAX_PATH];
        _tcscpy(myOldName, oldName);
        TCHAR finalName[2 * MAX_PATH];
        SalamanderGeneral->MaskName(finalName, 2 * MAX_PATH, myOldName, newName);

        // ocistime jmeno od nezadoucich znaku na zacatku a konci
        MakeValidFileName(finalName);
        // orez na MAX_PATH kvuli kopirovani do newName, chybu ohlasi kod o par radek nize
        if (_tcslen(finalName) >= MAX_PATH)
            finalName[MAX_PATH - 1] = 0;
        // aktualizujeme 'newName' na nove jmeno souboru
        _tcscpy(newName, finalName);

        int l = (int)_tcslen(oldPath);
        TCHAR tgtPath[MAX_PATH];
        memcpy(tgtPath, oldPath, l * sizeof(TCHAR));
        if (oldPath[l - 1] != '\\')
            tgtPath[l++] = '\\';
        if (_tcslen(finalName) + l < MAX_PATH)
        {
            _tcscpy(tgtPath + l, finalName);
            TCHAR path[MAX_PATH];
            _tcscpy(path, oldPath);
            LPTSTR end = path + l;
            if (*(end - 1) != '\\')
                *--end = '\\';
            _tcscpy(path + l, oldName);

            BOOL ret = FALSE;

            // zkusime prejmenovani nejdrive z dlouheho jmena a v pripade problemu jeste
            // z DOS-jmena (resi soubor/adresare dosazitelne jen pres Unicode nebo DOS-jmena)
            DWORD err = 0;
            BOOL moveRet = SalamanderGeneral->SalMoveFile(path, tgtPath, &err);
            if (!moveRet)
            {
                //err = GetLastError();
                /*
        if ((err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME) &&
            f->DosName != NULL)
        {
          strcpy(path + l, f->DosName);
          moveRet = SalMoveFile(path, tgtPath);
          if (!moveRet) err = GetLastError();
          strcpy(path + l, f->Name);
        }
        */
            }

            // ohlasime zmenu na ceste (prejmenovany soubor)
            TCHAR changedPath[MAX_PATH];
            lstrcpyn(changedPath, path, MAX_PATH);
            SalamanderGeneral->CutDirectory(changedPath);
            SalamanderGeneral->PostChangeOnPathNotification(changedPath, FALSE);

            if (moveRet)
            {
                renamed = TRUE;
                ret = TRUE;
            }
            else
            {
                if ((err == ERROR_ALREADY_EXISTS ||
                     err == ERROR_FILE_EXISTS) &&
                    SalamanderGeneral->StrICmp(path, tgtPath) != 0) // prepsat soubor ?
                {
                    DWORD inAttr = SalamanderGeneral->SalGetFileAttributes(path);
                    DWORD outAttr = SalamanderGeneral->SalGetFileAttributes(tgtPath);

                    if ((inAttr & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                        (outAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    { // jen pokud jsou oba soubory
                        HANDLE in = CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                               OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                               NULL);
                        HANDLE out = CreateFile(tgtPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                                NULL);
                        if (in != INVALID_HANDLE_VALUE && out != INVALID_HANDLE_VALUE)
                        {
                            TCHAR iAttr[101], oAttr[101];
                            GetInfo(iAttr, in);
                            GetInfo(oAttr, out);
                            CloseHandle(in);
                            CloseHandle(out);

                            COverwriteDlg dlg(HWindow, tgtPath, oAttr, path, iAttr);
                            int res = (int)dlg.Execute();

                            switch (res)
                            {
                            case IDCANCEL:
                                ret = TRUE;
                            case IDNO:
                                err = ERROR_SUCCESS;
                                break;

                            case IDYES:
                            {
                                SalamanderGeneral->ClearReadOnlyAttr(tgtPath); // aby sel smazat ...
                                if (!DeleteFile(tgtPath) || !SalamanderGeneral->SalMoveFile(path, tgtPath, &err))
                                {
                                    //err = GetLastError();
                                }
                                else
                                {
                                    renamed = TRUE;
                                    err = ERROR_SUCCESS;
                                    ret = TRUE;
                                }
                                // ohlasime zmenu na ceste (prejmenovany soubor)
                                SalamanderGeneral->PostChangeOnPathNotification(changedPath, FALSE);
                                break;
                            }
                            }
                        }
                        else
                        {
                            if (in == INVALID_HANDLE_VALUE)
                                TRACE_E("Nepodarilo se otevrit soubor " << path);
                            else
                                CloseHandle(in);
                            if (out == INVALID_HANDLE_VALUE)
                                TRACE_E("Nepodarilo se otevrit soubor " << tgtPath);
                            else
                                CloseHandle(out);
                        }
                    }
                }

                if (err != ERROR_SUCCESS)
                    SalamanderGeneral->SalMessageBox(HWindow, SalamanderGeneral->GetErrorText(err),
                                                     LoadStr(IDS_ERRORRENAMINGFILE),
                                                     MB_OK | MB_ICONEXCLAMATION);
            }
            *tryAgain = !ret;
        }
        else
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME),
                                             LoadStr(IDS_ERRORRENAMINGFILE),
                                             MB_OK | MB_ICONEXCLAMATION);
    }
    else
        SalamanderGeneral->SalMessageBox(HWindow, SalamanderGeneral->GetErrorText(ERROR_INVALID_NAME),
                                         LoadStr(IDS_ERRORRENAMINGFILE),
                                         MB_OK | MB_ICONEXCLAMATION);
    return renamed;
}

void CRendererWindow::OnCopyTo()
{
    CALL_STACK_MESSAGE1("CRendererWindow::OnCopyTo()");
    TCHAR dstName[MAX_PATH];
    CCopyToDlg dlg(HWindow, FileName, dstName);
    if (dlg.Execute() == IDOK)
    {
        // SHFileOperation works with multiple paths which must be NULL-separated
        // Thus extra terminating NULL is needed

        int srcListSize = (int)_tcslen(FileName) + 2;
        LPTSTR srcList = (LPTSTR)malloc(srcListSize * sizeof(TCHAR));
        if (srcList == NULL)
            return;
        memcpy(srcList, FileName, (srcListSize - 1) * sizeof(TCHAR));
        srcList[srcListSize - 1] = 0;

        int dstListSize = (int)_tcslen(dstName) + 2;
        LPTSTR dstList = (LPTSTR)malloc(dstListSize * sizeof(TCHAR));
        if (dstList == NULL)
        {
            free(srcList);
            return;
        }
        memcpy(dstList, dstName, (dstListSize - 1) * sizeof(TCHAR));
        dstList[dstListSize - 1] = 0;

        SHFILEOPSTRUCT fo;
        fo.hwnd = HWindow;
        fo.wFunc = FO_COPY;
        fo.pFrom = srcList;
        fo.pTo = dstList;
        fo.fFlags = 0;
        fo.fAnyOperationsAborted = FALSE;
        fo.hNameMappings = NULL;
        fo.lpszProgressTitle = _T("");
        // provedeme samotne mazani - uzasne snadne, bohuzel jim sem tam pada ;-)
        CALL_STACK_MESSAGE1("CRendererWindow::OnCopyTo::SHFileOperation");
        TCHAR changedPath[MAX_PATH];
        lstrcpyn(changedPath, FileName, MAX_PATH);
        if (SHFileOperation(&fo) == 0)
        {
            // ohlasime zmenu na ceste (prejmenovany soubor)
            SalamanderGeneral->CutDirectory(changedPath);
            SalamanderGeneral->PostChangeOnPathNotification(changedPath, FALSE);
        }

        free(srcList);
        free(dstList);
    }
}

void CRendererWindow::OnDelete(BOOL toRecycle)
{
    CALL_STACK_MESSAGE2("CRendererWindow::OnDelete(%d)", toRecycle);

    // SHFileOperation works with multiple paths which must be NULL-separated
    // Thus extra terminating NULL is needed

    int listSize = (int)_tcslen(FileName) + 2;
    LPTSTR list = (LPTSTR)malloc(listSize * sizeof(TCHAR));
    if (list == NULL)
        return;

    memcpy(list, FileName, (listSize - 1) * sizeof(TCHAR));
    list[listSize - 1] = 0;

    SHFILEOPSTRUCT fo;
    fo.hwnd = HWindow;
    fo.wFunc = FO_DELETE;
    fo.pFrom = list;
    fo.pTo = NULL;
    fo.fFlags = toRecycle ? FOF_ALLOWUNDO : 0;
    fo.fAnyOperationsAborted = FALSE;
    fo.hNameMappings = NULL;
    fo.lpszProgressTitle = _T("");
    // provedeme samotne mazani - uzasne snadne, bohuzel jim sem tam pada ;-)
    CALL_STACK_MESSAGE1("CRendererWindow::OnDelete::SHFileOperation");
    TCHAR changedPath[MAX_PATH];
    lstrcpyn(changedPath, FileName, MAX_PATH);
    if (SHFileOperation(&fo) == 0)
    {
        // z navratovych hodnot nepoznam zda byl soubor smazan nebo
        // uzivatel jen stisknul Cancel v konfirmaci
        if (!SalamanderGeneral->FileExists(FileName))
        {
            SalamanderGeneral->Free(FileName);
            FileName = SalamanderGeneral->DupStr(LoadStr(IDS_DELETED_TITLE));
            SetTitle();
        }
    }
    // ohlasime zmenu na ceste (prejmenovany soubor)
    SalamanderGeneral->CutDirectory(changedPath);
    SalamanderGeneral->PostChangeOnPathNotification(changedPath, FALSE);

    free(list);
}

LRESULT CRendererWindow::OnCommand(WPARAM wParam, LPARAM lParam, BOOL* closingViewer)
{
    CALL_STACK_MESSAGE3("CRendererWindow::OnCommand(0x%IX, 0x%IX,)", wParam, lParam);
    // files history
    if (LOWORD(wParam) >= CMD_RECENTFILES_FIRST && LOWORD(wParam) <= CMD_RECENTFILES_LAST)
    {
        int index = LOWORD(wParam) - CMD_RECENTFILES_FIRST;
        if (index < FILES_HISTORY_SIZE && G.FilesHistory[index] != NULL)
        {
            EnumFilesSourceUID = -1;
            if (!OpenFile(G.FilesHistory[index], -1, NULL))
                RemoveFromHistory(TRUE, index);
        }
        return 0;
    }
    // dirs history
    if (LOWORD(wParam) >= CMD_RECENRDIRS_FIRST && LOWORD(wParam) <= CMD_RECENTDIRS_LAST)
    {
        int index = LOWORD(wParam) - CMD_RECENRDIRS_FIRST;
        if (index < DIRS_HISTORY_SIZE && G.DirsHistory[index] != NULL)
            if (!OnFileOpen(G.DirsHistory[index]))
                RemoveFromHistory(FALSE, index);
        return 0;
    }

    switch (LOWORD(wParam))
    {
    case CMD_OPEN:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        if (!Loading)
        {
            TCHAR path[MAX_PATH];

            *path = 0;
            if (FileName && (*FileName != '<'))
            {
                // not clipboard, capture, scanned image
                LPTSTR s = _tcsrchr(FileName, '\\');
                if (s)
                {
                    if (s[-1] == ':')
                    {
                        s++; // root dir -> keep the backslash
                    }
                    lstrcpyn(path, FileName, (int)min(s - FileName + 1, SizeOf(path)));
                }
            }
            OnFileOpen(path);
        }
        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_SAVEAS:
    {
        if ((FileName[0] == '<') && !G.Save.RememberPath)
        {
            ghSaveAsWindow = HWindow;
            // Allow CMD_INTERNAL_SAVEAS 500 ms before we give up
            SetTimer(HWindow, SAVEAS_TIMER_ID, 500, NULL);
            SalamanderGeneral->PostMenuExtCommand(CMD_INTERNAL_SAVEAS, FALSE);
        }
        else
        {
            PostMessage(HWindow, WM_USER_SAVEAS_INTERNAL, 0, 0);
        }
        return 0;
    }

    case CMD_RELOAD:
    {
        if (!Viewer->Enablers[vweFileOpened2])
            return 0;

        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        if (Loading)
        {
            Canceled = TRUE; //postpone
            PostMessage(HWindow, WM_COMMAND, CMD_RELOAD, 0);
        }
        else
            OpenFile(FileName, -1, NULL);

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_FILE_FIRST:
    case CMD_FILE_PREV:
    case CMD_FILE_NEXT:
    case CMD_FILE_PREVSELFILE:
    case CMD_FILE_NEXTSELFILE:
    case CMD_FILE_LAST:
    {
        if (FileName == NULL || *FileName != '<' || _tcscmp(FileName, LoadStr(IDS_DELETED_TITLE)) == 0)
        {
            BOOL ok = FALSE;
            BOOL srcBusy = FALSE;
            BOOL noMoreFiles = FALSE;
            TCHAR fileName[MAX_PATH] = _T("");
            LPCTSTR reallyOpenedFileName, openedFileName = FileName;
            BOOL deletedFile = FileName != NULL && _tcscmp(FileName, LoadStr(IDS_DELETED_TITLE)) == 0;
            if (deletedFile)
                openedFileName = NULL;
            int enumFilesCurrentIndex = EnumFilesCurrentIndex;
            int what = LOWORD(wParam);
            reallyOpenedFileName = openedFileName;
            do
            {
                if ((what == CMD_FILE_PREV) || (what == CMD_FILE_LAST) || (what == CMD_FILE_PREVSELFILE))
                {
                    if (what == CMD_FILE_LAST)
                        enumFilesCurrentIndex = -1;
                    ok = SalamanderGeneral->GetPreviousFileNameForViewer(EnumFilesSourceUID,
                                                                         &enumFilesCurrentIndex,
                                                                         reallyOpenedFileName,
                                                                         what == CMD_FILE_PREVSELFILE,
                                                                         TRUE, fileName,
                                                                         &noMoreFiles, &srcBusy);
                    if (ok && (what == CMD_FILE_PREVSELFILE)) // bereme jen selected soubory
                    {
                        BOOL isSrcFileSel = FALSE;
                        ok = SalamanderGeneral->IsFileNameForViewerSelected(EnumFilesSourceUID,
                                                                            enumFilesCurrentIndex,
                                                                            fileName, &isSrcFileSel,
                                                                            &srcBusy);
                        if (ok && !isSrcFileSel)
                            ok = FALSE;
                    }
                }
                else
                {
                    if (what == CMD_FILE_FIRST)
                    {
                        enumFilesCurrentIndex = -1;
                    }
                    else
                    {
                        if (deletedFile && enumFilesCurrentIndex >= 0)
                            enumFilesCurrentIndex--; // aby pri smazanem souboru na Space (next-file) nedoslo k preskoceni dalsiho souboru vlivem sesunuti souboru v panelu
                    }
                    ok = SalamanderGeneral->GetNextFileNameForViewer(EnumFilesSourceUID,
                                                                     &enumFilesCurrentIndex,
                                                                     reallyOpenedFileName,
                                                                     what == CMD_FILE_NEXTSELFILE,
                                                                     TRUE, fileName,
                                                                     &noMoreFiles, &srcBusy);
                    if (ok && (what == CMD_FILE_NEXTSELFILE)) // bereme jen selected soubory
                    {
                        BOOL isSrcFileSel = FALSE;
                        ok = SalamanderGeneral->IsFileNameForViewerSelected(EnumFilesSourceUID,
                                                                            enumFilesCurrentIndex,
                                                                            fileName, &isSrcFileSel,
                                                                            &srcBusy);
                        if (ok && !isSrcFileSel)
                            ok = FALSE;
                    }
                }
                // repeat the search file if the found one is not supported by us
                reallyOpenedFileName = fileName;
            } while (ok && (!Loading && !InterfaceForViewer.CanViewFile(fileName)) && (what != CMD_FILE_FIRST) && (what != CMD_FILE_LAST));

            if (ok) // mame nove jmeno
            {
                if (openedFileName == NULL || SalamanderGeneral->StrICmp(fileName, openedFileName) != 0)
                {
                    if (Loading)
                    {
                        // pokud probiha nacitani, prerusime ho a operaci dokoncime potom
                        Canceled = TRUE;
                        PostMessage(HWindow, WM_COMMAND, wParam, lParam);
                        return 0;
                    }

                    if (Viewer->Lock != NULL)
                    {
                        SetEvent(Viewer->Lock);
                        Viewer->Lock = NULL; // ted uz je to jen na disk-cache
                    }
                    if (!OpenFile(fileName, -1, NULL))
                    {
                        // obrazek je vadny, pozavirame aktualni
                        if (PVHandle != NULL)
                        {
                            if (PVSequence)
                            {
                                KillTimer(HWindow, IMGSEQ_TIMER_ID);
                            }
                            PVCurImgInSeq = PVSequence = NULL;
                            PVW32DLL.PVCloseImage(PVHandle);
                            PVHandle = NULL;
                        }
                        ImageLoaded = FALSE;
                        SetTitle();
                        // Viewer->UpdateEnablers();   // zbytecne, provede se ihned po dokonceni teto metody
                        if (FileName != NULL)
                        {
                            SalamanderGeneral->Free(FileName);
                            FileName = NULL;
                        }
                    }

                    // index nastavime i v pripade neuspechu, aby user mohl prejit na dalsi/predchozi obrazek
                    EnumFilesCurrentIndex = enumFilesCurrentIndex;
                }
            }
            else
            {
                Viewer->SetStatusBarTexts(IDS_NO_MORE_IMAGES);
                bEatSBTextOnce = TRUE;
            }
        }
        return 0;
    }

    case CMD_IMG_FOCUS:
    {
        if (!Viewer->Enablers[vweFileOpened2])
            return 0;
        if (SalamanderGeneral->SalamanderIsNotBusy(NULL))
        {
            // nemuzeme zustat ve full screen, nemohli bychom vytahnout hlavni okno Salamandera
            if (Viewer->IsFullScreen())
                Viewer->ToggleFullScreen();

            lstrcpyn(Focus_Path, FileName, MAX_PATH);
            SalamanderGeneral->PostMenuExtCommand(CMD_INTERNAL_FOCUS, TRUE);
            Sleep(500);        // dojde k prepnuti do jineho okna, takze teoreticky tenhle Sleep nicemu nebude vadit
            Focus_Path[0] = 0; // po 0.5 sekunde uz o fokus nestojime (resi pripad, kdy jsme trefili zacatek BUSY rezimu Salamandera)
        }
        else
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SAL_IS_BUSY), LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
        return 0;
    }

    case CMD_IMG_RENAME:
    {
        if (!Viewer->Enablers[vweFileOpened2])
            return 0;
        LPCTSTR s = _tcsrchr(FileName, '\\');
        if (s == NULL)
            return 0;

        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        TCHAR oldPath[MAX_PATH];
        TCHAR oldName[MAX_PATH];
        memcpy(oldPath, FileName, (s - FileName) * sizeof(TCHAR));
        oldPath[s - FileName] = 0;
        _tcscpy(oldName, s + 1);

        TCHAR newName[MAX_PATH];
        _tcscpy(newName, s + 1);
        CRenameDialog dlg(HWindow, newName, MAX_PATH);
        while (1)
        {
            if (dlg.Execute() == IDOK)
            {
                BOOL tryAgain;
                BOOL renamed = RenameFileInternal(oldPath, oldName, newName, &tryAgain);
                /*  // Petr: zakomentoval jsem tenhle tvrdy refresh, protoze uz je to prezitek - refresh se provadi i v neaktivnim hl. okne Salamandera
          int sourcePanel;

          if (SalamanderGeneral->IsFileEnumSourcePanel(EnumFilesSourceUID, &sourcePanel))
            SalamanderGeneral->PostMenuExtCommand(sourcePanel == PANEL_LEFT ? CMD_INTERNAL_REFRESHLPANEL : CMD_INTERNAL_REFRESHRPANEL, TRUE);  // zajistime refresh i v neaktivnim panelu, aby fungovalo korektne prochazeni pres Next/Previous file ze zdroje
*/
                if (renamed)
                {
                    if (FileName != NULL)
                    {
                        SalamanderGeneral->Free(FileName);
                        FileName = NULL;
                    }

                    TCHAR newFileName[MAX_PATH];
                    int l = (int)_tcslen(oldPath);
                    memcpy(newFileName, oldPath, l * sizeof(TCHAR));
                    if (oldPath[l - 1] != '\\')
                        newFileName[l++] = '\\';
                    _tcscpy(newFileName + l, newName);
                    FileName = SalamanderGeneral->DupStr(newFileName);
                    SetTitle();
                    break;
                }
                else if (!tryAgain)
                    break;
            }
            else
                break;
        }
        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_IMG_COPYTO:
    {
        if (!Viewer->Enablers[vweFileOpened2])
            return 0;
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        OnCopyTo();

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_IMG_DELETE:
    {
        if (!Viewer->Enablers[vweFileOpened2])
            return 0;

        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        OnDelete((GetKeyState(VK_SHIFT) & 0x8000) == 0);
        /*  // Petr: zakomentoval jsem tenhle tvrdy refresh, protoze uz je to prezitek - refresh se provadi i v neaktivnim hl. okne Salamandera
      int sourcePanel;

      if (SalamanderGeneral->IsFileEnumSourcePanel(EnumFilesSourceUID, &sourcePanel))
        SalamanderGeneral->PostMenuExtCommand(sourcePanel == PANEL_LEFT ? CMD_INTERNAL_REFRESHLPANEL : CMD_INTERNAL_REFRESHRPANEL, TRUE);  // zajistime refresh i v neaktivnim panelu, aby fungovalo korektne prochazeni pres Next/Previous file ze zdroje
*/
        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_PRINT:
    {
        if (!Viewer->Enablers[vweFileOpened])
            return 0;

        CPrintParams params;
        params.pPVII = &pvii;
        params.PVHandle = PVHandle;
        params.ImageCage = IsCageValid(&SelectRect) ? &SelectRect : NULL;
        params.bMirrorHor = fMirrorHor;
        params.bMirrorVert = fMirrorVert;

        CALL_STACK_MESSAGE1("CMD_PRINT 1");

        // dialog je nositelem nastaveni tiskarny, papiru, atd, takze ho budeme alokovat
        // aby nastaveni zustalo zachovano pro dalsi tisk z tohoto okna
        if (pPrintDlg == NULL)
        {
            pPrintDlg = new CPrintDlg(HWindow, &params);
            if (!pPrintDlg->MyGetDefaultPrinter())
            {
                delete pPrintDlg;
                pPrintDlg = NULL;
            }
        }
        else
        {
            pPrintDlg->UpdateImageParams(&params);
        }

        if (pPrintDlg != NULL)
        {
            if (pPrintDlg->Execute() == IDOK)
            {
                DOCINFO docInfo;
                HDC HPrinterDC = pPrintDlg->HPrinterDC;
                int horDPI = GetDeviceCaps(HPrinterDC, LOGPIXELSX);
                int vertDPI = GetDeviceCaps(HPrinterDC, LOGPIXELSY);
                int h = GetDeviceCaps(HPrinterDC, HORZRES);
                int v = GetDeviceCaps(HPrinterDC, VERTRES);
                int prevWidth, prevHeight, prevLeft, prevTop;
                RECT r, *pR = NULL;
                double scaleX = (double)horDPI / 72;
                double scaleY = (double)vertDPI / 72;
                bool bSuccess;
                DWORD LastError;
                //           int      OldMapMode = -1;

                /*           if ((pvii.Width*2 < h) && (pvii.Height*2 < v) && (pvii.Colors > 256)) {
              // optimization to make print data smaller
              // better solution would be to use StretchBlt instead -> wait for next version
              // Doesn't work on HP LJ 5L on NT4  :-(
              SIZE     vwPort;

              OldMapMode = SetMapMode(HPrinterDC, MM_ANISOTROPIC);
              SetWindowExtEx(HPrinterDC, h, v, NULL);
              SetWindowOrgEx(HPrinterDC, 0, 0, NULL);
              SetViewportExtEx(HPrinterDC, 2*h, 2*v, NULL);
              SetViewportOrgEx(HPrinterDC, 0, 0, NULL);
              GetMapMode(HPrinterDC);
              GetViewportExtEx(HPrinterDC, &vwPort);
              if ((vwPort.cx == 2*h) && (vwPort.cy == 2*v)) {
                 // success
                 h /= 2; v /= 2;
              }
           }*/

                CALL_STACK_MESSAGE1("CMD_PRINT 2");

                // get the printed image size in pixels
                if (pPrintDlg->Params.bFit)
                {
                    prevWidth = h;
                    prevHeight = v;
                    if (pPrintDlg->Params.bKeepAspect)
                    {
                        double ratioX, ratioY;

                        ratioX = h / (pPrintDlg->origImgWidth * scaleX);
                        ratioY = v / (pPrintDlg->origImgHeight * scaleY);
                        ratioX /= ratioY;
                        if (ratioX > 1)
                        {
                            prevWidth = (int)(prevWidth / ratioX);
                        }
                        else
                        {
                            prevHeight = (int)(prevHeight * ratioX);
                        }
                    }
                }
                else
                {
                    // from points to pixels
                    prevWidth = abs((int)(pPrintDlg->Params.Width * scaleX));
                    prevHeight = abs((int)(pPrintDlg->Params.Height * scaleY));
                }
                prevLeft = prevTop = 0;
                // get top-left coordinates
                if (!pPrintDlg->Params.bFit || pPrintDlg->Params.bKeepAspect)
                {
                    if (pPrintDlg->Params.bCenter)
                    {
                        prevLeft = (h - prevWidth) / 2;
                        prevTop = (v - prevHeight) / 2;
                    }
                    else
                    {
                        prevLeft = (int)(pPrintDlg->Params.Left * scaleX);
                        prevTop = (int)(pPrintDlg->Params.Top * scaleY);
                    }
                }
                // printing selection -> update total size, specify printed region
                if (pPrintDlg->Params.ImageCage && pPrintDlg->Params.bSelection)
                {
                    int selWidth, selHeight;

                    // printed region (=clipping area)
                    pR = &r;
                    r.left = prevLeft;
                    r.top = prevTop;
                    r.right = prevLeft + prevWidth;
                    r.bottom = prevTop + prevHeight;
                    // get real image size for scaling
                    selWidth = params.ImageCage->right - params.ImageCage->left + 1;
                    selHeight = params.ImageCage->bottom - params.ImageCage->top + 1;
                    prevWidth = prevWidth * pvii.Width / selWidth;
                    prevHeight = prevHeight * pvii.Height / selHeight;
                    scaleX = (double)prevWidth / pvii.Width;
                    scaleY = (double)prevHeight / pvii.Height;
                    // get the top left corner of the full image
                    if (pPrintDlg->Params.Width < 0)
                    {
                        prevLeft -= (int)((pvii.Width - params.ImageCage->right - 1) * scaleX);
                    }
                    else
                    {
                        prevLeft -= (int)(params.ImageCage->left * scaleX);
                    }
                    if (pPrintDlg->Params.Height < 0)
                    {
                        prevTop -= (int)((pvii.Height - params.ImageCage->bottom - 1) * scaleY);
                    }
                    else
                    {
                        prevTop -= (int)(params.ImageCage->top * scaleY);
                    }
                }

                memset(&docInfo, 0, sizeof(docInfo));
                docInfo.cbSize = sizeof(docInfo);
                docInfo.lpszDocName = FileName;
                Viewer->SetStatusBarTexts(IDS_PRINTING);
                bSuccess = FALSE;
                LastError = 0;
                if (StartDoc(HPrinterDC, &docInfo) > 0)
                {
                    if (StartPage(HPrinterDC) > 0)
                    {
                        DWORD stretchedWidth = prevWidth * (1 - 2 * (pPrintDlg->Params.bMirrorHor ^ (pPrintDlg->Params.Width < 0)));
                        DWORD stretchedHeight = prevHeight * (1 - 2 * (pPrintDlg->Params.bMirrorVert ^ (pPrintDlg->Params.Height < 0)));

                        CALL_STACK_MESSAGE4("CMD_PRINT 3: %ux%u, %u", stretchedWidth, stretchedHeight, pvii.Colors);
                        PVW32DLL.PVSetStretchParameters(PVHandle, stretchedWidth, stretchedHeight, COLORONCOLOR);

                        if (!pR)
                        {
                            // don't print outside paper -> optimize scaling
                            r.left = max(0, prevLeft);
                            r.top = max(0, prevTop);
                            r.right = min(h, prevLeft + prevWidth);
                            r.bottom = min(v, prevTop + prevHeight);
                            pR = &r;
                        }
                        CALL_STACK_MESSAGE1("CMD_PRINT 4");
                        if (PVC_OK == PVW32DLL.PVDrawImage(PVHandle, HPrinterDC, prevLeft, prevTop, pR))
                        {
                            bSuccess = TRUE;
                        }
                        else
                        {
                            LastError = GetLastError();
                        }
                        CALL_STACK_MESSAGE1("CMD_PRINT 5");
                        // restore scaling for display
                        PVW32DLL.PVSetStretchParameters(PVHandle, XStretchedRange * (1 - 2 * fMirrorHor),
                                                        YStretchedRange * (1 - 2 * fMirrorVert), /*pvii.Colors == 2 ? BLACKONWHITE : */ COLORONCOLOR);
                        EndPage(HPrinterDC); // resets value returned by GetLastError()
                    }
                    else
                    {
                        LastError = GetLastError();
                    }
                    EndDoc(HPrinterDC); // resets value returned by GetLastError()
                }
                else
                {
                    LastError = GetLastError();
                }
                /*           if (OldMapMode >= 0) {
              // Restore previous: FinePrint driver for WNT 4.7x seems to remember all HDC changes
              SetViewportExtEx(HPrinterDC, GetDeviceCaps(HPrinterDC, HORZRES), 
                GetDeviceCaps(HPrinterDC, VERTRES), NULL);
              SetMapMode(HPrinterDC, OldMapMode);
           }*/
                // remove Printing... from Status bar
                Viewer->SetStatusBarTexts();
                if (!bSuccess)
                {
                    TCHAR errTmp[MAX_PATH + 20];
                    TCHAR errBuff[164];

                    if (LastError)
                    {
                        SalamanderGeneral->GetErrorText(LastError, errTmp, SizeOf(errTmp));
                    }
                    else
                    {
                        _tcscpy(errTmp, LoadStr(IDS_UNKNOWN_ERROR));
                        _tcscat(errTmp, LoadStr(IDS_CHECK_EVENTLOG));
                    }
                    _stprintf(errBuff, LoadStr(IDS_PRINTING_FAILED), errTmp);
                    SalamanderGeneral->SalMessageBox(HWindow, errBuff, LoadStr(IDS_ERRORTITLE), MB_ICONSTOP);
                }
            }
        }
        return 0;
    }

    case CMD_ABOUT:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        CAboutDialog dlg(HWindow);
        dlg.Execute();

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_HELP_CONTENTS:
    case CMD_HELP_INDEX:
    case CMD_HELP_SEARCH:
    {
        CHtmlHelpCommand command;
        DWORD_PTR dwData = 0;
        switch (LOWORD(wParam))
        {
        case CMD_HELP_CONTENTS:
        {
            SalamanderGeneral->OpenHtmlHelp(HWindow, HHCDisplayContext, IDH_INTRODUCTION, TRUE); // nechceme dva messageboxy za sebou
            command = HHCDisplayTOC;
            break;
        }

        case CMD_HELP_INDEX:
        {
            command = HHCDisplayIndex;
            break;
        }

        case CMD_HELP_SEARCH:
        {
            command = HHCDisplaySearch;
            break;
        }
        }

        SalamanderGeneral->OpenHtmlHelp(HWindow, command, dwData, FALSE);
        break;
    }

    case CMD_IMG_PROP:
    {
        if (pvii.Width != CW_USEDEFAULT)
        {
            int nFrames = 0;
            LPPVImageSequence pSeq = PVSequence;

            while (pSeq)
            {
                nFrames++;
                pSeq = pSeq->pNext;
            }

            if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
            {
                HiddenCursor = FALSE;
                POINT p;
                GetCursorPos(&p);
                SetCursorPos(p.x, p.y);
            }

            BOOL oldCanHideCursor = CanHideCursor;
            CanHideCursor = FALSE;

            CImgPropDialog dlg(HWindow, &pvii, Comment, nFrames);
            dlg.Execute();

            CanHideCursor = oldCanHideCursor;
        }
        return 0;
    }

    case CMD_IMG_EXIF:
    {
        if (ImageLoaded && (pvii.Flags & PVFF_EXIF) && InitEXIF(HWindow, FALSE))
        {
            if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
            {
                HiddenCursor = FALSE;
                POINT p;
                GetCursorPos(&p);
                SetCursorPos(p.x, p.y);
            }

            BOOL oldCanHideCursor = CanHideCursor;
            CanHideCursor = FALSE;

            // Digital camera RAWs are multipage TIFFs but may be marked as JPEG
            CExifDialog dlg(HWindow, FileName, ((PVF_JPG == pvii.Format) && (1 < pvii.NumOfImages)) ? PVF_TIFF : pvii.Format);
            dlg.Execute();

            CanHideCursor = oldCanHideCursor;
        }
        return 0;
    }

    case CMD_IMG_HISTOGRAM:
    {
        if (ImageLoaded)
        {
            DWORD luminosity[256], red[256], green[256], blue[256], rgb[256];
            LPDWORD data[5];

            if (!HiddenCursor)
                SetCursor(LoadCursor(NULL, IDC_WAIT));

            GetHistogramData(luminosity, red, green, blue, rgb);
            data[CHistogramControl::Luminosity] = luminosity;
            data[CHistogramControl::Red] = red;
            data[CHistogramControl::Green] = green;
            data[CHistogramControl::Blue] = blue;
            data[CHistogramControl::RGBSum] = rgb;

            if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
            {
                HiddenCursor = FALSE;
                POINT p;
                GetCursorPos(&p);
                SetCursorPos(p.x, p.y);
            }

            BOOL oldCanHideCursor = CanHideCursor;
            CanHideCursor = FALSE;

            SetCursor(LoadCursor(NULL, IDC_ARROW));

            CHistogramWindow(Viewer, data).Show();

            CanHideCursor = oldCanHideCursor;
        }
        return 0;
    }

    case CMD_CFG:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        OnConfiguration(HWindow);

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_COPY:
    {
        if (OpenClipboard(NULL))
        {
            if (EmptyClipboard())
            {
                RECT r; // 'r' je v souradnicich obrazku 1=1px; pocatek [0,0]

                if (IsCageValid(&SelectRect))
                {
                    r = SelectRect;
                    NormalizeRect(&r);
                    r.right++;
                    r.bottom++;
                }
                else
                {
                    r.left = 0;
                    r.top = 0;
                    r.right = pvii.Width;
                    r.bottom = pvii.Height;
                }

                HDC hDC = GetDC(NULL);
                HDC hMemDC = CreateCompatibleDC(hDC);
                HBITMAP hBmp = CreateCompatibleBitmap(hDC, r.right - r.left, r.bottom - r.top);
                ReleaseDC(NULL, hDC);
                HBITMAP hOldBmp = (HBITMAP)SelectObject(hMemDC, hBmp);

                // j.r. FIXME: nyni je potreba ukecat engine, aby na souradnice 0, 0 do bitmapy
                // hBmp vybrane v hMemDC vykreslil 1:1 obdelnik urceny 'r'
                // ?Bude potreba docasne zmenit parametry pres PVSetStretchParameters a pak je vratit zpet?
                // ?Nechetlo by to nejaky PUSH/POP metody pro tyto ucely?
                // ?Jak urcim, ze se ma vymalovat pouze ten obdelnicek?
                if (PVSequence)
                {
                    SetWindowOrgEx(hMemDC, r.left, r.top, NULL);
                    PaintImageSequence(NULL, hMemDC, TRUE);
                }
                else
                {
                    // Patera 2004.10.26: Rysavy wants 1:1 copy to clipboard; I have always preferred current zoom
                    RECT paintRect;

                    paintRect.left = paintRect.top = 0;
                    paintRect.right = pvii.Width;
                    paintRect.bottom = pvii.Height;
                    PVW32DLL.PVSetStretchParameters(PVHandle, pvii.Width * (1 - 2 * fMirrorHor),
                                                    pvii.Height * (1 - 2 * fMirrorVert), /*pvii.Colors == 2 ? BLACKONWHITE : */ COLORONCOLOR);
                    PVW32DLL.PVDrawImage(PVHandle, hMemDC, -r.left, -r.top, &paintRect);
                    PVW32DLL.PVSetStretchParameters(PVHandle, XStretchedRange * (1 - 2 * fMirrorHor),
                                                    YStretchedRange * (1 - 2 * fMirrorVert), /*pvii.Colors == 2 ? BLACKONWHITE : */ COLORONCOLOR);
                }

                SelectObject(hMemDC, hOldBmp);
                DeleteDC(hMemDC);

                SetClipboardData(CF_BITMAP, hBmp);
                // !pozor! bitmapu prevzal clipboard, nesmime ji sestrelit
            }
            CloseClipboard();
        }
        return 0;
    }

        /*
    case CMD_COPY:
          if (OpenClipboard(NULL))
          {
            if (EmptyClipboard())
            {
               HDC     dc1, dc2;
               HBITMAP bmp;
               RECT    r;//, r2;

//               r2.top = 0; r2.bottom = YStretchedRange; r2.left = 0; r2.right = XStretchedRange;
               if (ISSELECTION) {
                  r.left = (int)SelectRect.left;
                  r.top = (int)SelectRect.top;
                  r.right = (int)SelectRect.right;
                  r.bottom = (int)SelectRect.bottom;
                  if (r.left > r.right) {
                     register int tmp = r.left;
                     r.left = r.right;
                     r.right = tmp;
                  }
                  if (r.top > r.bottom) {
                     register int tmp = r.top;
                     r.top = r.bottom;
                     r.bottom = tmp;
                  }
//                  if ((r.left > 0) || (XStart > 0)) {
                  if (r.left != GetScrollPos(HWindow, SB_HORZ)) { // can be -1
                     r.left++;
                  }
//                  if ((r.top > 0)) {
                  if (r.top != GetScrollPos(HWindow, SB_VERT)) { // can be -1
                     r.top++;
                  }
//                  if ((r.right <= XStretchedRange) || (XStart > 0)) {
//                  if (r.right != XStretchedRange) {
                     r.right--;
//                  }
//                  if ((r.bottom <= YStretchedRange) || (YStart > 0)) {
//                  if (r.bottom != YStretchedRange) {
                     r.bottom--;
//                  }
               } else {
                  r.top = 0; r.bottom = YStretchedRange; r.left = 0; r.right = XStretchedRange;
//                  r = r2;
               }

               dc1 = GetWindowDC(HWindow);
               dc2 = CreateCompatibleDC(dc1);
               bmp = CreateCompatibleBitmap(dc1, r.right-r.left, r.bottom-r.top);//XStretchedRange, YStretchedRange);
               bmp = (HBITMAP) SelectObject(dc2, bmp);

               //FillRect(dc2, &r2, (HBRUSH)(COLOR_WINDOW+1));
               if (PVSequence) {
                  PaintImageSequence(NULL, dc2, TRUE);
               } else {
                  PVW32DLL.PVDrawImage(PVHandle, dc2, -r.left, -r.top, NULL);//&r);
               }
               bmp = (HBITMAP) SelectObject(dc2, bmp);
               SetClipboardData(CF_BITMAP, bmp);
               DeleteDC(dc2);
               ReleaseDC(HWindow, dc1);
            }
            CloseClipboard();
          }
          break;
*/
    case CMD_PASTE:
        LPPVHandle pvhnd;
        PVImageInfo pvii2;
        PVCODE result;

        result = PVW32DLL.PVLoadFromClipboard(&pvhnd, &pvii2, sizeof(pvii2));
        if (result == PVC_OK)
        {
            if (PVSequence)
            {
                KillTimer(HWindow, IMGSEQ_TIMER_ID);
            }
            PVCurImgInSeq = PVSequence = NULL;
            PVW32DLL.PVCloseImage(PVHandle);
            PVHandle = pvhnd;
            memcpy(&pvii, &pvii2, sizeof(pvii2));
            InvalidateRect(HWindow, NULL, FALSE);
            EnumFilesSourceUID = -1;
            SalamanderGeneral->Free(FileName);
            FileName = SalamanderGeneral->DupStr(LoadStr(IDS_CLIPBOARD_TITLE));
            fMirrorHor = fMirrorVert = FALSE;
            XStretchedRange = XRange = max(1, pvii.Width);
            YStretchedRange = YRange = pvii.Height;
            ZoomFactor = ZOOM_SCALE_FACTOR;
            ZoomIndex = 0;
            ZoomType = G.ZoomType;
            ImageLoaded = TRUE;
            InvalidateCage(&SelectRect);
            TryEnterHandToolMode();
            WMSize();
            PostMessage(HWindow, WM_MOUSEMOVE, 0, 0); // update SB
        }
        break;

    case CMD_PREVPAGE:
    case CMD_FIRSTPAGE:
    {
        if (Viewer->Enablers[vwePrevPage])
        {
            if (Loading)
            {
                Canceled = TRUE;
                PostMessage(HWindow, WM_COMMAND, wParam, lParam);
            }
            else
            {
                if (LOWORD(wParam) == CMD_PREVPAGE)
                    pvii.CurrentImage--;
                else
                    pvii.CurrentImage = 0;
                InitiatePageLoad();
            }
        }
        return 0;
    }

    case CMD_NEXTPAGE:
    case CMD_LASTPAGE:
    {
        if (Viewer->Enablers[vweNextPage])
        {
            if (Loading)
            {
                Canceled = TRUE;
                PostMessage(HWindow, WM_COMMAND, wParam, lParam);
            }
            else
            {
                if (LOWORD(wParam) == CMD_NEXTPAGE)
                    pvii.CurrentImage++;
                else
                    pvii.CurrentImage = pvii.NumOfImages - 1;
                InitiatePageLoad();
            }
        }
        return 0;
    }

    case CMD_PAGE_INTERNAL:
    {
        // numbering: 0 ... pvii.NumOfImages-1
        if ((DWORD)lParam < pvii.NumOfImages)
        {
            if (Loading)
            {
                Canceled = TRUE;
                PostMessage(HWindow, WM_COMMAND, wParam, lParam);
            }
            else
            {
                pvii.CurrentImage = (DWORD)lParam; // FIXME_X64 - docasne pretypovani na (DWORD), nejde o ukazatel?
                InitiatePageLoad();
            }
        }
        return 0;
    }

    case CMD_PAGE:
    {
        if (Viewer->Enablers[vweMorePages])
        {
            DWORD currImg = pvii.CurrentImage;
            CPageDialog dlg(HWindow, &pvii.CurrentImage, pvii.NumOfImages);
            if ((dlg.Execute() == IDOK) && (currImg != pvii.CurrentImage))
            {
                if (Loading)
                {
                    // must postpone because we are now called from ProgressProcedure
                    Canceled = TRUE;
                    PostMessage(HWindow, WM_COMMAND, CMD_PAGE_INTERNAL, pvii.CurrentImage);
                    pvii.CurrentImage = currImg;
                }
                else
                {
                    if (PVC_OK != InitiatePageLoad())
                    {
                        // unsupported page? Can happen in Digital Camera Raw TIFFs
                        pvii.CurrentImage = currImg;
                    }
                }
            }
        }
        return 0;
    }

    case CMD_ZOOM_TO:
        if (ImageLoaded)
        {
            if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
            {
                HiddenCursor = FALSE;
                POINT p;
                GetCursorPos(&p);
                SetCursorPos(p.x, p.y);
            }

            BOOL oldCanHideCursor = CanHideCursor;
            CanHideCursor = FALSE;

            ZoomTo(-1);

            CanHideCursor = oldCanHideCursor;
        }
        return 0;

    case CMD_ZOOM_IN:
        if (ImageLoaded)
        {
            ZoomIn();
            UpdateInfos();
        }
        return 0;

    case CMD_ZOOM_INMAX:
        if (ImageLoaded)
        {
            // maximum zoom times 100 percent times our precision (100 - 2 dec. digits)
            ZoomTo(ZOOM_MAX * 100 * 100);
        }
        return 0;

    case CMD_ZOOM_OUT:
        if (ImageLoaded)
        {
            ZoomOut();
            UpdateInfos();
        }
        return 0;

    case CMD_ZOOMWIDTH:
        if (ImageLoaded)
        {
            ZoomType = eShrinkToWidth;
            WMSize();
            InvalidateRect(HWindow, NULL, FALSE);
        }
        return 0;

    case CMD_ZOOMWHOLE:
        if (ImageLoaded)
        {
            ZoomType = eShrinkToFit;
            WMSize();
            InvalidateRect(HWindow, NULL, FALSE);
        }
        return 0;

    case CMD_ZOOMACTUAL:
        if (ImageLoaded)
        {
            ZoomType = eZoomOriginal;
            WMSize();
            InvalidateRect(HWindow, NULL, FALSE);
        }
        return 0;

    case CMD_CAPTURE:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        CCaptureDialog dlg(HWindow);
        if (dlg.Execute() == IDOK)
            PostMessage(HWindow, WM_COMMAND, CMD_CAPTURE_INTERNAL, 0);

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_CAPTURE_INTERNAL:
    {
        if (G.CaptureTrigger == CAPTURE_TRIGGER_HOTKEY)
        {
            if (RegisterHotKey(HWindow, G.CaptureAtomID, HIBYTE(G.CaptureHotKey), LOBYTE(G.CaptureHotKey)) == 0)
                return 0;
        }
        else
        {
            if (SetTimer(HWindow, CAPTURE_TIMER_ID, 1000 * G.CaptureTimer, NULL) == 0)
                return 0;
        }

        ShowWindow(Viewer->HWindow, SW_MINIMIZE);
        SetWindowText(Viewer->HWindow, LoadStr(IDS_CAPTURING));
        Capturing = TRUE;

        return 0;
    }

    case CMD_CAPTURE_CANCELED:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        SalamanderGeneral->SalMessageBox(HWindow,
                                         LoadStr(IDS_CAPTURECANCELED), LoadStr(IDS_PLUGINNAME),
                                         MB_ICONINFORMATION | MB_OK);

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_WALLPAPER_CENTER:
    case CMD_WALLPAPER_TILE:
    case CMD_WALLPAPER_STRETCH:
    case CMD_WALLPAPER_RESTORE:
    case CMD_WALLPAPER_NONE:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        SetAsWallpaper(LOWORD(wParam));

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

    case CMD_EXIT:
    {
        SendMessage(Viewer->HWindow, WM_CLOSE, 0, 0);
        return 0;
    }

    case CMD_TOOLBAR:
    {
        Viewer->ToggleToolBar();
        return 0;
    }

    case CMD_STATUSBAR:
    {
        Viewer->ToggleStatusBar();
        Viewer->OnSize();
        return 0;
    }

    case CMD_FULLSCREEN:
    {
        if (ImageLoaded)
        {
            Viewer->ToggleFullScreen();
        }
        return 0;
    }

        /*
    case CMD_TOOLS_HAND:
    {
      ShutdownTool();
      CurrTool = RT_HAND;
      PostMessage(HWindow, WM_SETCURSOR, 0, HTCLIENT);
      Viewer->UpdateToolBar();
      return 0;
    }
    */

    case CMD_H_PAGEUP:
        SendMessage(HWindow, WM_HSCROLL, SB_PAGEUP, 0);
        return 0;

    case CMD_H_PAGEDOWN:
        SendMessage(HWindow, WM_HSCROLL, SB_PAGEDOWN, 0);
        return 0;

    case CMD_H_HOME:
        HScroll(SB_THUMBPOSITION, 0);
        return 0;

    case CMD_H_END:
        HScroll(SB_THUMBPOSITION, XStretchedRange - 1);
        return 0;

    case CMD_V_PAGEUP:
        if (G.PageDnUpScrolls)
        {
            SendMessage(HWindow, WM_VSCROLL, SB_PAGEUP, 0);
        }
        else
        {
            SendMessage(HWindow, WM_COMMAND, CMD_FILE_PREV, 0);
        }
        return 0;

    case CMD_V_PAGEDOWN:
        if (G.PageDnUpScrolls)
        {
            SendMessage(HWindow, WM_VSCROLL, SB_PAGEDOWN, 0);
        }
        else
        {
            SendMessage(HWindow, WM_COMMAND, CMD_FILE_NEXT, 0);
        }
        return 0;

    case CMD_V_HOME:
        VScroll(SB_THUMBPOSITION, 0);
        return 0;

    case CMD_V_END:
        VScroll(SB_THUMBPOSITION, YStretchedRange - 1);
        return 0;

    case CMD_UP:
        SendMessage(HWindow, WM_VSCROLL, SB_LINEUP, 0);
        return 0;

    case CMD_DOWN:
        SendMessage(HWindow, WM_VSCROLL, SB_LINEDOWN, 0);
        return 0;

    case CMD_LEFT:
        SendMessage(HWindow, WM_HSCROLL, SB_LINEUP, 0);
        return 0;

    case CMD_RIGHT:
        SendMessage(HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
        return 0;

    case CMD_CLOSE:
    {
        if (PipWindow)
        {
            ShutdownTool();
            SelectTool(RT_HAND);
        }
        else if (Loading)
            Canceled = TRUE;
        else
            PostMessage(Viewer->HWindow, WM_CLOSE, 0, 0);
        return 0;
    }

    case CMD_TOOLS_HAND:
        ShutdownTool();
        SelectTool(RT_HAND);
        return 0;

    case CMD_TOOLS_ZOOM:
        ShutdownTool();
        SelectTool(RT_ZOOM);
        return 0;

    case CMD_TOOLS_SELECT:
        ShutdownTool();
        SelectTool(RT_SELECT);
        return 0;

    case CMD_TOOLS_PIPETTE:
    {
        if (PipWindow)
        {
            ShutdownTool();
            SelectTool(RT_HAND);
            return 0;
        }
        if (!ImageLoaded)
        {
            return 0;
        }
        ShutdownTool();
        SelectTool(RT_PIPETTE);
        SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_PIPETA)));

        //          HDC    dc = GetDC(HWindow);
        //          SIZE   size;
        //          int    rgb;
        POINT pos;
        //          RECT   rc;
        //          int    show;

        //          SelectObject(dc, GetStockObject(ANSI_VAR_FONT));
        GetCursorPos(&pos);
        //          ScreenToClient(HWindow, &pos);
        //          GetClientRect(HWindow, &rc);
        //          rgb = GetPixel(dc, pos.x, pos.y);
        //          _stprintf(toolTipText, LoadStr(G.PipetteInHex ? IDS_RGB_HEX : IDS_RGB),
        //           GetRValue(rgb), GetGValue(rgb), GetBValue(rgb));
        //          show = PtInRect(&rc, pos) ? SW_SHOW : SW_HIDE;
        //          ClientToScreen(HWindow, &pos);
        toolTipText[0] = 0;
        //          GetTextExtentPoint32(dc, toolTipText, _tcslen(toolTipText), &size);
        //          ReleaseDC(HWindow, dc);

        PipWindow = CreateWindow(TIP_WINDOW_CLASSNAME, _T(""), WS_POPUP,
                                 pos.x + 2, pos.y + 2, /*size.cx+*/ 7, /*size.cy+*/ 3,
                                 HWindow, NULL, DLLInstance, (LPVOID)toolTipText); //lParam
        ShowWindow(PipWindow, SW_SHOW /*show*/);
        SetFocus(HWindow);
        UpdateWindow(PipWindow);
        return 0;
    } // CMD_TOOLS_PIPETTE

    case CMD_MIRROR_HOR:
    case CMD_MIRROR_VERT:
    case CMD_ROTATE180:
    {
        if (ImageLoaded)
        {
            HCURSOR hOldCursor;
            if (!HiddenCursor)
                hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            if (LOWORD(wParam) == CMD_MIRROR_HOR || LOWORD(wParam) == CMD_ROTATE180)
            {
                fMirrorHor = !fMirrorHor;
                if (IsCageValid(&SelectRect))
                {
                    int tmp = SelectRect.left;
                    SelectRect.left = (pvii.Width - 1) - SelectRect.right;
                    SelectRect.right = (pvii.Width - 1) - tmp;
                }
            }
            if (LOWORD(wParam) == CMD_MIRROR_VERT || LOWORD(wParam) == CMD_ROTATE180)
            {
                fMirrorVert = !fMirrorVert;
                if (IsCageValid(&SelectRect))
                {
                    int tmp = SelectRect.top;
                    SelectRect.top = (pvii.Height - 1) - SelectRect.bottom;
                    SelectRect.bottom = (pvii.Height - 1) - tmp;
                }
            }
            WMSize();
            InvalidateRect(HWindow, NULL, FALSE);
            PostMessage(HWindow, WM_MOUSEMOVE, 0, 0); // update SB
            if (!HiddenCursor)
                SetCursor(hOldCursor);
        }
        return 0;
    }

    case CMD_DESELECT:
        if (IsCageValid(&SelectRect))
        {
            DrawCageRect(NULL, &SelectRect);
            InvalidateCage(&SelectRect);
            Viewer->SetStatusBarTexts();
        }
        break;

    case CMD_SELECTALL:
    {
        if (IsCageValid(&SelectRect))
            DrawCageRect(NULL, &SelectRect);
        SelectRect.left = 0;
        SelectRect.top = 0;
        SelectRect.right = pvii.Width - 1;
        SelectRect.bottom = pvii.Height - 1;
        DrawCageRect(NULL, &SelectRect);
        Viewer->SetStatusBarTexts();
        return 0;
    }

    case CMD_CROP:
    {
        if (ImageLoaded && !PVSequence)
        {
            if (IsCageValid(&SelectRect))
            {
                RECT r = SelectRect;
                NormalizeRect(&r);
                int width = r.right - r.left + 1;
                int height = r.bottom - r.top + 1;
                PVCODE code = PVW32DLL.PVCropImage(PVHandle,
                                                   fMirrorHor ? (pvii.Width - 1) - r.right : r.left,
                                                   fMirrorVert ? (pvii.Height - 1) - r.bottom : r.top, width, height);
                if (code != PVC_OK)
                {
                    SalamanderGeneral->SalMessageBox(HWindow, PVW32DLL.PVGetErrorText(code),
                                                     LoadStr(IDS_ERROR_CALLING_PVW32_DLL), MB_ICONEXCLAMATION);
                }
                InvalidateCage(&SelectRect);
                FreeComment();
                PVW32DLL.PVGetImageInfo(PVHandle, &pvii, sizeof(pvii), pvii.CurrentImage);
                DuplicateComment();
                XRange = XStretchedRange = pvii.Width;
                YRange = YStretchedRange = pvii.Height;
                WMSize();
                InvalidateRect(HWindow, NULL, FALSE);
                UpdateInfos();
            }
            else
            {
                SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_SELECT_REGION),
                                                 LoadStr(IDS_PLUGINNAME), MB_ICONSTOP);
            }
        }
        return 0;
    }

    case CMD_ROTATE_LEFT:
    case CMD_ROTATE_RIGHT:
    {
        if (ImageLoaded && !PVSequence)
        {
            HCURSOR hOldCursor;
            if (!HiddenCursor)
                hOldCursor = SetCursor(LoadCursor(NULL, IDC_WAIT));
            Viewer->SetStatusBarTexts(IDS_SB_AUTOROTATING);
            if (PVC_OK == PVW32DLL.PVChangeImage(PVHandle,
                                                 ((LOWORD(wParam) == CMD_ROTATE_RIGHT) ^ (fMirrorVert ^ fMirrorHor)) ? PVCF_ROTATE90CW : PVCF_ROTATE90CCW))
            {
                DWORD tmp;

                FreeComment();
                PVW32DLL.PVGetImageInfo(PVHandle, &pvii, sizeof(pvii), pvii.CurrentImage);
                DuplicateComment();
                // flip image extents that we keep internally
                tmp = XStretchedRange;
                XStretchedRange = YStretchedRange;
                YStretchedRange = tmp;
                tmp = XRange;
                XRange = YRange;
                YRange = tmp;
                if (IsCageValid(&SelectRect))
                {
                    if (LOWORD(wParam) == CMD_ROTATE_RIGHT)
                    {
                        int tmp2;

                        tmp2 = SelectRect.top;
                        SelectRect.top = SelectRect.left;
                        SelectRect.left = (pvii.Width - 1) - SelectRect.bottom;
                        SelectRect.bottom = SelectRect.right;
                        SelectRect.right = (pvii.Width - 1) - tmp2;
                    }
                    else
                    {
                        int tmp2;

                        tmp2 = SelectRect.left;
                        SelectRect.left = SelectRect.top;
                        SelectRect.top = (pvii.Height - 1) - SelectRect.right;
                        SelectRect.right = SelectRect.bottom;
                        SelectRect.bottom = (pvii.Height - 1) - tmp2;
                    }
                }

                WMSize();
                InvalidateRect(HWindow, NULL, FALSE);
                PostMessage(HWindow, WM_MOUSEMOVE, 0, 0); // update SB
            }
            if (!HiddenCursor)
                SetCursor(hOldCursor);
        }
        return 0;
    }

    case CMD_SCAN:
    {
        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

#if defined(ENABLE_TWAIN32) && defined(ENABLE_WIA)
        You_must_split_this_command_to_two_commands_one_for_TWAIN_and_second_for_WIA();
#endif // defined(ENABLE_TWAIN32) && defined(ENABLE_WIA)

        Viewer->ReleaseExtraScanImages(); // used for both WIA and TWAIN scanning

#ifdef ENABLE_WIA
        if (Viewer->InitWiaWrap())
        {
            BOOL closeParent = FALSE;
            HBITMAP bmp = Viewer->WiaWrap->AcquireImage(HWindow, &Viewer->ExtraScanImages, &closeParent);
            if (closeParent)
            {
                if (bmp != NULL)
                    DeleteObject(bmp);
                if (closingViewer != NULL)
                    *closingViewer = TRUE;
                PostMessage(Viewer->HWindow, WM_CLOSE, 0, 0);
            }
            else
            {
                if (bmp != NULL)
                    Viewer->OpenScannedImage(bmp);
                if (Viewer->ExtraScanImages != NULL)
                    PostMessage(Viewer->HWindow, WM_USER_SCAN_EXTRA_IMAGES, 0, 0);
            }
        }
#endif // ENABLE_WIA

#ifdef ENABLE_TWAIN32
        if (Viewer->InitTwain())
        {
            if (Viewer->Twain->Acquire())
            {
            }
        }
#endif // ENABLE_TWAIN32

        CanHideCursor = oldCanHideCursor;
        return 0;
    }

#ifdef ENABLE_TWAIN32
    case CMD_SCAN_SOURCE:
    {
#ifdef ENABLE_WIA
        This_command_may_seem_to_select_scanner_also_for_WIA__Add_TWAIN_to_command_name();
#endif // ENABLE_WIA

        if (HiddenCursor) // aby byl v dialogu videt kurzor mysi
        {
            HiddenCursor = FALSE;
            POINT p;
            GetCursorPos(&p);
            SetCursorPos(p.x, p.y);
        }

        BOOL oldCanHideCursor = CanHideCursor;
        CanHideCursor = FALSE;

        if (Viewer->InitTwain())
            Viewer->Twain->SelectSource(closingViewer);

        CanHideCursor = oldCanHideCursor;
        return 0;
    }
#endif // ENABLE_TWAIN32

    case CMD_FILE_TOGGLESELECT:
    {
        if (FileName != NULL && *FileName != '<')
        {
            BOOL isFileSelected, srcBusy;
            BOOL ok = SalamanderGeneral->IsFileNameForViewerSelected(EnumFilesSourceUID,
                                                                     EnumFilesCurrentIndex,
                                                                     FileName, &isFileSelected,
                                                                     &srcBusy);
            if (ok)
            {
                ok = SalamanderGeneral->SetSelectionOnFileNameForViewer(EnumFilesSourceUID,
                                                                        EnumFilesCurrentIndex,
                                                                        FileName, !isFileSelected,
                                                                        &srcBusy);
                //          if (ok) TRACE_I("CMD_FILE_TOGGLESELECT: success: file is now " << (!isFileSelected ? "selected" : "unselected"));
                //          else TRACE_I("CMD_FILE_TOGGLESELECT: error: " << (srcBusy ? "panel or Find is busy" : "file not found or connection was broken"));
            }
            //        else TRACE_I("CMD_FILE_TOGGLESELECT: error: " << (srcBusy ? "panel or Find is busy" : "file not found or connection was broken"));
        }
        return 0;
    }
    }
    return CWindow::WindowProc(WM_COMMAND, wParam, lParam);
} /* CRendererWindow::OnCommand */

PVCODE CRendererWindow::GetHistogramData(LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb)
{
    return PVW32DLL.CalculateHistogram(PVHandle, &pvii, luminosity, red, green, blue, rgb);
} /* CRendererWindow::GetHistogramData */

void CRendererWindow::ZoomOnTheSpot(LPARAM dwMousePos, BOOL bZoomIn)
{
    POINT pt;
    RECT r;

    pt.x = LOWORD(dwMousePos);
    pt.y = HIWORD(dwMousePos);
    ClientToPicture(&pt);
    if (bZoomIn)
    {
        ZoomIn();
    }
    else
    {
        ZoomOut();
    }
    PictureToClient(&pt);
    GetClientRect(HWindow, &r);
    // prevent still unscaled image from scrolling before being
    // scaled & painted
    LockWindowUpdate(HWindow);
    HScroll(SB_THUMBPOSITION, GetScrollPos(HWindow, SB_HORZ) + pt.x - r.right / 2);
    VScroll(SB_THUMBPOSITION, GetScrollPos(HWindow, SB_VERT) + pt.y - r.bottom / 2);
    LockWindowUpdate(NULL);
} /* CRendererWindow::ZoomOnTheSpot */
