// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "gui.h"
#include "md5.h"

#include <uxtheme.h>
#include <vssym32.h>
#include <ppl.h>

#include "svg.h"

#include "versinfo.rh2"

void GetDlgItemRectAndDestroy(HWND hWindow, int resID, RECT* r)
{
    HWND hItem = GetDlgItem(hWindow, resID);
    if (hItem == NULL)
    {
        r->left = r->top = r->right = r->bottom = 0;
        return;
    }
    GetWindowRect(hItem, r);
    MapWindowPoints(NULL, hWindow, (POINT*)r, 2);
    DestroyWindow(hItem);
}

//*****************************************************************************
//
// Splash Screen
//

#define SPLASH_WIDTH_DLGUNITS 270
#define SPLASH_HEIGHT_DLGUNITS 72

CSplashScreen::CSplashScreen()
    : CDialog(HInstance, IDD_SPLASH, NULL, ooStatic)
{
    Bitmap = NULL;
    OriginalBitmap = NULL;
    HNormalFont = NULL;
    HBoldFont = NULL;
    GradientY = 0;
    Width = 0;
    Height = 0;

    // vytvorim font
    LOGFONT lf;
    //  GetSystemGUIFont(&lf);
    //  lf.lfWeight = FW_NORMAL;

    HDC hDC2 = HANDLES(GetDC(NULL));
    lf.lfHeight = -MulDiv(8, GetDeviceCaps(hDC2, LOGPIXELSY), 72);
    HANDLES(ReleaseDC(NULL, hDC2));
    lf.lfWidth = 0;
    lf.lfEscapement = 0;
    lf.lfOrientation = 0;
    lf.lfWeight = FW_NORMAL;
    lf.lfItalic = 0;
    lf.lfUnderline = 0;
    lf.lfStrikeOut = 0;
    lf.lfCharSet = DEFAULT_CHARSET;
    lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
    lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
    lf.lfQuality = DEFAULT_QUALITY;
    lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
    strcpy(lf.lfFaceName, "MS Shell Dlg 2");

    HNormalFont = HANDLES(CreateFontIndirect(&lf));
    // vytvorim tucnou variantu
    lf.lfWeight = FW_BOLD;
    HBoldFont = HANDLES(CreateFontIndirect(&lf));
}

CSplashScreen::~CSplashScreen()
{
    // touto dobou uz by mela byt bitmapa sestrelena, ale pojistime se
    DestroyBitmap();
    if (HNormalFont != NULL)
        HANDLES(DeleteObject(HNormalFont));
    if (HBoldFont != NULL)
        HANDLES(DeleteObject(HBoldFont));
}

BOOL CSplashScreen::PaintText(const char* text, int x, int y, BOOL bold, COLORREF clr)
{
    HDC hDC = NULL;
    if (Bitmap != NULL)
        hDC = Bitmap->HMemDC;
    if (hDC != NULL)
    {
        RECT r;
        r.left = x;
        r.top = y;
        r.right = x + Width;
        r.bottom = y + Height;
        int oldBkMode = SetBkMode(hDC, TRANSPARENT);
        COLORREF oldTextColor = SetTextColor(hDC, clr);
        HFONT hOldFont = (HFONT)SelectObject(hDC, bold ? HBoldFont : HNormalFont);
        DrawText(hDC, text, -1, &r, DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP);
        SelectObject(hDC, hOldFont);
        SetTextColor(hDC, oldTextColor);
        SetBkMode(hDC, oldBkMode);
    }
    return TRUE;
}

BOOL CSplashScreen::PrepareBitmap()
{
    Bitmap = new CBitmap();
    OriginalBitmap = new CBitmap();
    if (Bitmap != NULL && OriginalBitmap != NULL)
    {
        HDC hDC = HANDLES(GetDC(NULL));
        if (!Bitmap->CreateBmp(hDC, Width, Height))
        {
            delete Bitmap;
            Bitmap = NULL;
        }
        if (!OriginalBitmap->CreateBmp(hDC, Width, Height))
        {
            delete OriginalBitmap;
            OriginalBitmap = NULL;
        }
        HANDLES(ReleaseDC(NULL, hDC));
    }
    else
        return FALSE;

    HDC hDC = Bitmap->HMemDC;

    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = Width;
    r.bottom = Height;

    // podmazeme pozadi bilou barvou
    SetBkColor(hDC, RGB(255, 255, 255));
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);

    CSVGSprite svgText;
    CSVGSprite svgGrad;
    CSVGSprite svgHand;
    concurrency::parallel_invoke(
        [&]
        { svgText.Load(IDB_LOGO_TEXT, OpenSalR.right - OpenSalR.left, OpenSalR.bottom - OpenSalR.top, SVGSTATE_ORIGINAL); },
        [&]
        { svgGrad.Load(IDB_LOGO_GRAD, Width, -1, SVGSTATE_ORIGINAL); },
        [&]
        { svgHand.Load(IDB_LOGO_HAND, -1, Height, SVGSTATE_ORIGINAL); });

    SIZE textSize, gradSize, handSize;
    svgText.GetSize(&textSize);
    svgGrad.GetSize(&gradSize);
    svgHand.GetSize(&handSize);

    svgText.AlphaBlend(hDC, OpenSalR.left, OpenSalR.top, textSize.cx, textSize.cy, SVGSTATE_ORIGINAL);
    svgGrad.AlphaBlend(hDC, 0, GradientY, gradSize.cx, Height - GradientY, SVGSTATE_ORIGINAL);
    svgHand.AlphaBlend(hDC, Width - handSize.cx, 0, handSize.cx, handSize.cy, SVGSTATE_ORIGINAL);

    // pevne texty
    PaintText(SALAMANDER_TEXT_VERSION,
              VersionR.left,
              VersionR.top,
              FALSE, RGB(128, 128, 128));

    PaintText(VERSINFO_COPYRIGHT,
              CopyrightR.left,
              CopyrightR.top,
              TRUE, RGB(255, 255, 255));

    // zaloha bitmapy bez textu
    BitBlt(OriginalBitmap->HMemDC, 0, 0, Width, Height, Bitmap->HMemDC, 0, 0, SRCCOPY);

    return TRUE;
}

void CSplashScreen::SetText(const char* text)
{
    if (Bitmap != NULL && OriginalBitmap != NULL)
    {
        // obnovim pozadi
        BitBlt(Bitmap->HMemDC, StatusR.left, StatusR.top, StatusR.right - StatusR.left, StatusR.bottom - StatusR.top, OriginalBitmap->HMemDC, StatusR.left, StatusR.top, SRCCOPY);
        PaintText(text,
                  StatusR.left, StatusR.top,
                  FALSE, RGB(255, 255, 255));

        // pokud jsme zobrazeni, promitneme zmenu do obrazovky
        if (HWindow != NULL)
        {
            HDC hDC = HANDLES(GetDC(HWindow));
            BitBlt(hDC, StatusR.left, StatusR.top, StatusR.right - StatusR.left, StatusR.bottom - StatusR.top, Bitmap->HMemDC, StatusR.left, StatusR.top, SRCCOPY);
            HANDLES(ReleaseDC(HWindow, hDC));
        }
    }
}

void CSplashScreen::DestroyBitmap()
{
    if (Bitmap != NULL)
    {
        delete Bitmap;
        Bitmap = NULL;
    }
    if (OriginalBitmap != NULL)
    {
        delete OriginalBitmap;
        OriginalBitmap = NULL;
    }
}

INT_PTR
CSplashScreen::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSplashScreen::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RECT r;
        GetClientRect(HWindow, &r);
        Width = r.right - r.left;
        Height = r.bottom - r.top;

        GetDlgItemRectAndDestroy(HWindow, IDC_SPLASH_OPENSAL, &OpenSalR);
        GetDlgItemRectAndDestroy(HWindow, IDC_SPLASH_VERSION, &VersionR);
        GetDlgItemRectAndDestroy(HWindow, IDC_SPLASH_COPYRIGHT, &CopyrightR);
        GetDlgItemRectAndDestroy(HWindow, IDC_SPLASH_STATUS, &StatusR);

        GradientY = VersionR.bottom + 5;

        TRACE_I("!!!!!!!!!!!!! BEG ");
        PrepareBitmap();
        TRACE_I("!!!!!!!!!!!!! END ");
        break;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        BitBlt(ps.hdc, 0, 0, Width, Height, Bitmap->HMemDC, 0, 0, SRCCOPY);
        HANDLES(EndPaint(HWindow, &ps));
        return FALSE;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }
    }

    return CDialog::DialogProc(uMsg, wParam, lParam);
}

CSplashScreen SplashScreen;

BOOL SplashScreenOpen()
{
    if (SplashScreen.HWindow != NULL)
    {
        TRACE_E("SplashScreenOpen(): splash screen already exists!");
        return FALSE;
    }

    SplashScreen.Create();

    if (SplashScreen.HWindow != NULL)
    {
        MultiMonCenterWindow(SplashScreen.HWindow, NULL, FALSE);
        ShowWindow(SplashScreen.HWindow, SW_SHOWNOACTIVATE);
        UpdateWindow(SplashScreen.HWindow);
        return TRUE;
    }
    else
    {
        SplashScreen.DestroyBitmap();
        return FALSE;
    }
}

void SplashScreenCloseIfExist()
{
    if (SplashScreen.HWindow != NULL)
    {
        DestroyWindow(SplashScreen.HWindow);
        SplashScreen.DestroyBitmap();
    }
}

BOOL ExistSplashScreen()
{
    return (SplashScreen.HWindow != NULL);
}

void IfExistSetSplashScreenText(const char* text)
{
    if (SplashScreen.HWindow != NULL)
        SplashScreen.SetText(text);
}

HWND GetSplashScreenHandle()
{
    return SplashScreen.HWindow;
}

//
// ****************************************************************************
// CAboutDialog
//

CAboutDialog::CAboutDialog(HWND parent)
    : CCommonDialog(HLanguage, IDD_ABOUT, parent)
{
    HGradientBkBrush = HANDLES(CreateSolidBrush(RGB(221, 151, 4))); // musi byt zluta z res\logoline.png
    BackgroundBitmap = NULL;
}

CAboutDialog::~CAboutDialog()
{
    if (BackgroundBitmap != NULL)
        delete BackgroundBitmap;
    HANDLES(DeleteObject(HGradientBkBrush));
}

HDWP OffsetControl(HWND hWindow, HDWP hdwp, int id, int yOffset)
{
    HWND hCtrl = GetDlgItem(hWindow, id);
    RECT r;
    GetWindowRect(hCtrl, &r);
    ScreenToClient(hWindow, (LPPOINT)&r);

    hdwp = HANDLES(DeferWindowPos(hdwp, hCtrl, NULL, r.left, r.top + yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
    return hdwp;
}

CBitmap*
AboutAndEvalDlgCreateBkgnd(HWND hWindow)
{
    RECT opensalR;
    RECT logoR;
    RECT gradR;
    GetDlgItemRectAndDestroy(hWindow, IDC_ABOUT_OPENSAL, &opensalR);
    GetDlgItemRectAndDestroy(hWindow, IDC_ABOUT_LOGO, &logoR);
    GetDlgItemRectAndDestroy(hWindow, IDC_ABOUT_BOTTOM, &gradR);

    RECT r;
    GetClientRect(hWindow, &r);

    CBitmap* bitmap = new CBitmap();
    HDC hDC = HANDLES(GetDC(NULL));
    if (!bitmap->CreateBmp(hDC, r.right - r.left, r.bottom - r.top))
    {
        delete bitmap;
        bitmap = NULL;
    }
    HANDLES(ReleaseDC(NULL, hDC));
    if (bitmap == NULL)
        return NULL;

    hDC = bitmap->HMemDC;

    // podmazeme pozadi bilou barvou
    SetBkColor(hDC, RGB(255, 255, 255));
    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);

    CSVGSprite svgText;
    CSVGSprite svgGrad;
    CSVGSprite svgHand;
    concurrency::parallel_invoke(
        [&]
        { svgText.Load(IDB_LOGO_TEXT, opensalR.right - opensalR.left, opensalR.bottom - opensalR.top, SVGSTATE_ORIGINAL); },
        [&]
        { svgGrad.Load(IDB_ABOUT_GRAD, r.right - r.left, -1, SVGSTATE_ORIGINAL); },
        [&]
        { svgHand.Load(IDB_LOGO_HAND, logoR.right - logoR.left, logoR.bottom - logoR.top, SVGSTATE_ORIGINAL); });

    SIZE textSize, gradSize, handSize;
    svgText.GetSize(&textSize);
    svgGrad.GetSize(&gradSize);
    svgHand.GetSize(&handSize);

    svgText.AlphaBlend(hDC, opensalR.left, opensalR.top, textSize.cx, textSize.cy, SVGSTATE_ORIGINAL);
    svgGrad.AlphaBlend(hDC, 0, gradR.top, gradSize.cx, r.bottom - r.top - gradR.top, SVGSTATE_ORIGINAL);
    svgHand.AlphaBlend(hDC, r.right - r.left - handSize.cx, 0, handSize.cx, handSize.cy, SVGSTATE_ORIGINAL);

    return bitmap;
}

void AboutAndEvalDlgPaintBkgnd(HWND hWindow, HDC hDC, CBitmap* bitmap)
{
    BitBlt(hDC, 0, 0, bitmap->GetWidth(), bitmap->GetHeight(), bitmap->HMemDC, 0, 0, SRCCOPY);
}

INT_PTR
CAboutDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CAboutDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CHyperLink* hl;

        SetDlgItemText(HWindow, IDS_ABOUT_SALAMANDER, SALAMANDER_TEXT_VERSION);
        new CStaticText(HWindow, IDS_ABOUT_SALAMANDER, STF_BOLD);
        //      new CStaticText(HWindow, IDS_ABOUT_FIRM, STF_BOLD);

        // je-li prostredi v cestine nebo slovenstine, budeme ukazovat automaticky ceskou verzi webu
        BOOL english = LanguageID != 0x405 /* cesky */ && LanguageID != 0x41B /* slovensky */;

        hl = new CHyperLink(HWindow, IDC_ABOUT_WWW);
        if (hl != NULL)
        {
            const char* url = english ? "https://www.altap.cz" : "https://www.altap.cz/cz";
            SetDlgItemText(HWindow, IDC_ABOUT_WWW, url + 8);
            hl->SetActionOpen(url);
        }

        BackgroundBitmap = AboutAndEvalDlgCreateBkgnd(HWindow);
        break;
    }

    case WM_CTLCOLORSTATIC:
    {
        HDC hdcStatic = (HDC)wParam;
        HWND hwndStatic = (HWND)lParam;
        int resID = GetWindowLong(hwndStatic, GWL_ID);
        COLORREF textClr = RGB(70, 70, 70);
        switch (resID)
        {
        case IDC_STATIC_6:
        case IDC_STATIC_7:
        case IDC_STATIC_8:
            textClr = RGB(128, 128, 128);
            break;
        }
        SetTextColor(hdcStatic, textClr);
        SetBkColor(hdcStatic, RGB(255, 255, 255));
        return (BOOL)(UINT_PTR)GetStockObject(NULL_BRUSH);
    }

    case WM_CTLCOLORBTN:
    {
        if (IsAppThemed())
        {
            // bez tehle obezlicky je kolem OK tlacitka sedivy frame
            if (WindowsVistaAndLater)
                return (BOOL)(UINT_PTR)HGradientBkBrush;
            else
                return (BOOL)(UINT_PTR)GetStockObject(NULL_BRUSH); // pod XP to jeste chodilo dobre
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        HDC hDC = (HDC)wParam;
        AboutAndEvalDlgPaintBkgnd(HWindow, hDC, BackgroundBitmap);
        return TRUE;
    }

    case WM_NCHITTEST:
    {
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, HTCAPTION);
        return HTCAPTION;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
