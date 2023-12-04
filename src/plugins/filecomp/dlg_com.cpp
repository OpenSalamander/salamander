// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

SALCOLOR Colors[NUMBER_OF_COLORS]; // aktualni barvy

// standartni barvy
SALCOLOR DefaultColors[NUMBER_OF_COLORS] =
    {
        // barvy textu ve sloupci s cisly radek
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_FG_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_FG_LEFT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_FG_RIGHT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_FG_LEFT_CHANGE_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_FG_RIGHT_CHANGE_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_BK_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_BK_LEFT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_BK_RIGHT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_BK_LEFT_CHANGE_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_BK_RIGHT_CHANGE_FOCUSED

        // ramecek kolem aktualni zmeny ve sloupci s cisly radek
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_LEFT_BORDER
        RGBF(0, 0, 0, SCF_DEFAULT), // LINENUM_RIGHT_BORDER

        // barvy zobrazeneho obsahu souboru
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_LEFT_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_RIGHT_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_LEFT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_RIGHT_CHANGE
        RGBF(255, 255, 255, 0),     // TEXT_FG_LEFT_CHANGE_FOCUSED
        RGBF(255, 255, 255, 0),     // TEXT_FG_RIGHT_CHANGE_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_LEFT_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_RIGHT_FOCUSED
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_LEFT_CHANGE
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_RIGHT_CHANGE
        RGBF(10, 128, 240, 0),      // TEXT_BK_LEFT_CHANGE_FOCUSED
        RGBF(255, 120, 120, 0),     // TEXT_BK_RIGHT_CHANGE_FOCUSED

        // ramecek kolem aktualni zmeny v zobrazenem obsahu souboru
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_LEFT_BORDER
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_RIGHT_BORDER

        // barva vybraneho bloku textu
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_FG_SELECTION
        RGBF(0, 0, 0, SCF_DEFAULT), // TEXT_BK_SELECTION
};

COLORREF CustomColors[16];

CCompareOptions DefCompareOptions;

CConfiguration Configuration;

HACCEL HAccels;

int CaretWidth;

void CenterWindow(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CenterWindow()");
    HWND hParent = GetParent(hWindow);
    if (hParent == NULL)
        hParent = SG->GetMainWindowHWND();
    SG->MultiMonCenterWindow(hWindow, hParent, TRUE);
}

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(HLanguage, resID, hParent, origin){
          CALL_STACK_MESSAGE_NONE}

      CCommonDialog::CCommonDialog(int resID, int helpID, HWND hParent, CObjectOrigin origin) : CDialog(HLanguage, resID, helpID, hParent, origin){CALL_STACK_MESSAGE_NONE}

                                                                                                INT_PTR CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE_NONE
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        CenterWindow(HWindow);
        break; // chci focus od DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalGUI->ArrangeHorizontalLines(HWindow);
}

// ****************************************************************************

BOOL CheckMouseWheelMsg(HWND window, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CheckMouseWheelMsg(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam,
                        lParam);
    if (uMsg == WM_MOUSEWHEEL)
    {
        return SendMessage(window, WM_USER_MOUSEWHEEL, wParam, lParam) != 0;
    }
    return FALSE;
}

// ****************************************************************************

HBRUSH CreateDitheredBrush()
{
    CALL_STACK_MESSAGE1("CreateDitheredBrush()");
    unsigned short bmb[8];
    int i;
    for (i = 0; i < 8;)
    {
        bmb[i++] = 0x55;
        bmb[i++] = 0xAA;
    }
    HBITMAP hbm = CreateBitmap(8, 8, 1, 1, (LPBYTE)bmb);
    if (!hbm)
    {
        TRACE_E("Nepodarilo se vytvorit bitmapu pro rastrovany brush.");
        return NULL;
    }
    HBRUSH ret = CreatePatternBrush(hbm);
    DeleteObject(hbm);
    if (!ret)
    {
        TRACE_E("Nepodarilo se vytvorit rastrovany brush.");
        return NULL;
    }
    return ret;
}

COLORREF
GetInverseColor(COLORREF color)
{
    CALL_STACK_MESSAGE_NONE
    return RGB(255 - GetRValue(color), 255 - GetGValue(color), 255 - GetBValue(color));
}

COLORREF
GetAverageColor(COLORREF color1, UINT weight1, COLORREF color2, UINT weight2)
{
    CALL_STACK_MESSAGE_NONE
    return RGB((GetRValue(color1) * weight1 + GetRValue(color2) * weight2) / (weight1 + weight2),
               (GetGValue(color1) * weight1 + GetGValue(color2) * weight2) / (weight1 + weight2),
               (GetBValue(color1) * weight1 + GetBValue(color2) * weight2) / (weight1 + weight2));
}

COLORREF
GetScrollbarColor()
{
    CALL_STACK_MESSAGE_NONE
    if (GetSysColor(COLOR_BTNFACE) != GetSysColor(COLOR_3DHILIGHT))
    {
        return GetAverageColor(GetSysColor(COLOR_SCROLLBAR), 1,
                               GetSysColor(COLOR_SCROLLBAR) == GetSysColor(COLOR_3DHILIGHT) &&
                                       GetSysColor(COLOR_SCROLLBAR) == GetSysColor(COLOR_WINDOW)
                                   ? GetSysColor(COLOR_BTNFACE)
                                   : GetSysColor(COLOR_3DHILIGHT),
                               1);
    }
    else
    {
        return GetSysColor(COLOR_BTNFACE);
    }
}

HPALETTE
CreateColorPallete()
{
    CALL_STACK_MESSAGE1("CreateColorPallete()");
    LPLOGPALETTE logPal =
        (LPLOGPALETTE)malloc(sizeof(LOGPALETTE) + (NUMBER_OF_COLORS - 1) * sizeof(PALETTEENTRY));
    if (!logPal)
    {
        TRACE_E("Low memory for logical palette");
        return NULL;
    }
    logPal->palVersion = 0x300; // Windows Version 3.0
    logPal->palNumEntries = NUMBER_OF_COLORS;
    memset(&logPal->palPalEntry[0], 0, NUMBER_OF_COLORS * sizeof(PALETTEENTRY));
    HPALETTE ret = CreatePalette(logPal);
    if (!ret)
        TRACE_E("CreatePalette() failed");
    free(logPal);
    return ret;
}

void UpdateDefaultColors(SALCOLOR* colors, HPALETTE& palette)
{
    CALL_STACK_MESSAGE1("UpdateDefaultColors(, )");
    // barvy textu ve sloupci s cisly radek
    if (GetFValue(colors[LINENUM_FG_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
    //if (GetFValue(colors[LINENUM_FG_FOCUSED]) & SCF_DEFAULT)
    //  SetRGBPart(&colors[LINENUM_FG_FOCUSED], GetCOLORREF(colors[LINENUM_FG_NORMAL]));

    if (GetFValue(colors[LINENUM_FG_LEFT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_FG_LEFT_CHANGE], GetCOLORREF(colors[LINENUM_FG_NORMAL]));
    if (GetFValue(colors[LINENUM_FG_RIGHT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_FG_RIGHT_CHANGE], GetCOLORREF(colors[LINENUM_FG_NORMAL]));
    if (GetFValue(colors[LINENUM_FG_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_FG_LEFT_CHANGE_FOCUSED], GetCOLORREF(colors[LINENUM_FG_LEFT_CHANGE]));
    if (GetFValue(colors[LINENUM_FG_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_FG_RIGHT_CHANGE_FOCUSED], GetCOLORREF(colors[LINENUM_FG_RIGHT_CHANGE]));

    // barvy pozadi ve sloupci s cisly radek
    if (GetFValue(colors[LINENUM_BK_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_BK_NORMAL], GetScrollbarColor());
    //if (GetFValue(colors[LINENUM_BK_FOCUSED]) & SCF_DEFAULT)
    //  SetRGBPart(&colors[LINENUM_BK_FOCUSED], GetCOLORREF(colors[LINENUM_BK_NORMAL]));
    if (GetFValue(colors[LINENUM_BK_LEFT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_BK_LEFT_CHANGE], GetCOLORREF(colors[LINENUM_BK_NORMAL]));
    if (GetFValue(colors[LINENUM_BK_RIGHT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_BK_RIGHT_CHANGE], GetCOLORREF(colors[LINENUM_BK_NORMAL]));
    if (GetFValue(colors[LINENUM_BK_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_BK_LEFT_CHANGE_FOCUSED], GetSysColor(COLOR_BTNFACE));
    //GetAverageColor(GetCOLORREF(colors[LINENUM_BK_LEFT_CHANGE]), 9,
    //                GetCOLORREF(colors[LINENUM_FG_LEFT_CHANGE]), 1));
    if (GetFValue(colors[LINENUM_BK_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_BK_RIGHT_CHANGE_FOCUSED], GetSysColor(COLOR_BTNFACE));
    //GetAverageColor(GetCOLORREF(colors[LINENUM_BK_RIGHT_CHANGE]), 9,
    //                GetCOLORREF(colors[LINENUM_FG_RIGHT_CHANGE]), 1));

    // barva ramecku kolem vybrane zmeny ve sloupci s cisly radek
    if (GetFValue(colors[LINENUM_LEFT_BORDER]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_LEFT_BORDER], GetSysColor(COLOR_BTNSHADOW));
    if (GetFValue(colors[LINENUM_RIGHT_BORDER]) & SCF_DEFAULT)
        SetRGBPart(&colors[LINENUM_RIGHT_BORDER], GetSysColor(COLOR_BTNSHADOW));

    // barvy textu zobrazeneho obsahu souboru
    if (GetFValue(colors[TEXT_FG_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_FG_NORMAL], SG->GetCurrentColor(SALCOL_VIEWER_FG_NORMAL));
    if (GetFValue(colors[TEXT_FG_LEFT_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_FG_LEFT_FOCUSED], GetCOLORREF(colors[TEXT_FG_NORMAL]));
    if (GetFValue(colors[TEXT_FG_RIGHT_FOCUSED]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_FG_RIGHT_FOCUSED], GetCOLORREF(colors[TEXT_FG_NORMAL]));
    if (GetFValue(colors[TEXT_FG_LEFT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_FG_LEFT_CHANGE], GetCOLORREF(colors[TEXT_FG_NORMAL]));
    if (GetFValue(colors[TEXT_FG_RIGHT_CHANGE]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_FG_RIGHT_CHANGE], GetCOLORREF(colors[TEXT_FG_NORMAL]));
    //if (GetFValue(colors[TEXT_FG_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
    //  SetRGBPart(&colors[TEXT_FG_LEFT_CHANGE_FOCUSED], GetCOLORREF(colors[TEXT_FG_LEFT_CHANGE]));
    //if (GetFValue(colors[TEXT_FG_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
    //  SetRGBPart(&colors[TEXT_FG_RIGHT_CHANGE_FOCUSED], GetCOLORREF(colors[TEXT_FG_RIGHT_CHANGE]));

    // barvy pozadi zobrazeneho obsahu souboru
    if (GetFValue(colors[TEXT_BK_NORMAL]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_BK_NORMAL], SG->GetCurrentColor(SALCOL_VIEWER_BK_NORMAL));
    if (GetFValue(colors[TEXT_BK_LEFT_FOCUSED]) & SCF_DEFAULT)
    {
        SetRGBPart(&colors[TEXT_BK_LEFT_FOCUSED],
                   GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 12,
                                   GetCOLORREF(colors[TEXT_FG_NORMAL]), 1));
    }
    if (GetFValue(colors[TEXT_BK_RIGHT_FOCUSED]) & SCF_DEFAULT)
    {
        SetRGBPart(&colors[TEXT_BK_RIGHT_FOCUSED],
                   GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 12,
                                   GetCOLORREF(colors[TEXT_FG_NORMAL]), 1));
    }
    /*  if (GetFValue(colors[TEXT_BK_LEFT_CHANGE]) & SCF_DEFAULT)
    SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE], GetCOLORREF(colors[TEXT_BK_NORMAL]));
  if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE]) & SCF_DEFAULT)
    SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE], GetCOLORREF(colors[TEXT_BK_NORMAL]));
  if (GetFValue(colors[TEXT_BK_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
    SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE_FOCUSED], //GetAverageColor(GetSysColor(COLOR_WINDOW), 9, GetSysColor(COLOR_WINDOWTEXT), 1));
      GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 9, 
                      GetCOLORREF(colors[TEXT_FG_NORMAL]), 1));
  if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
    SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE_FOCUSED], //GetAverageColor(GetSysColor(COLOR_WINDOW), 9, GetSysColor(COLOR_WINDOWTEXT), 1));
      GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 9, 
                      GetCOLORREF(colors[TEXT_FG_NORMAL]), 1));
                      */

    /*
  if (GetFValue(colors[TEXT_BK_LEFT_CHANGE]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 9, 
                               GetInverseColor(GetCOLORREF(colors[TEXT_FG_LEFT_CHANGE])), 1));
  }
  if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 9, 
                               GetInverseColor(GetCOLORREF(colors[TEXT_FG_RIGHT_CHANGE])), 1));
  }
  if (GetFValue(colors[TEXT_BK_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE_FOCUSED], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 7, 
                               GetInverseColor(GetCOLORREF(colors[TEXT_FG_LEFT_CHANGE])), 3));
  }
  if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE_FOCUSED], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 7, 
                               GetInverseColor(GetCOLORREF(colors[TEXT_FG_RIGHT_CHANGE])), 3));

  }*/

    if (GetFValue(colors[TEXT_BK_LEFT_CHANGE]) & SCF_DEFAULT)
    {
        SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE],
                   GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 12,
                                   RGB(0, 128, 255), 4));
    }
    if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE]) & SCF_DEFAULT)
    {
        SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE],
                   GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 9,
                                   RGB(255, 128, 128), 6));
    }
    /*
  if (GetFValue(colors[TEXT_BK_LEFT_CHANGE_FOCUSED]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_LEFT_CHANGE_FOCUSED], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 1, 
                               RGB(0,128,255), 15));
  }
  if (GetFValue(colors[TEXT_BK_RIGHT_CHANGE_FOCUSED]) & SCF_DEFAULT)
  {
    SetRGBPart(&colors[TEXT_BK_RIGHT_CHANGE_FOCUSED], 
               GetAverageColor(GetCOLORREF(colors[TEXT_BK_NORMAL]), 1, 
                               RGB(255,120,120), 20));

  }
  */

    // barva ramecku kolem vybrane zmeny
    if (GetFValue(colors[TEXT_LEFT_BORDER]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_LEFT_BORDER], GetSysColor(COLOR_BTNSHADOW));
    if (GetFValue(colors[TEXT_RIGHT_BORDER]) & SCF_DEFAULT)
        SetRGBPart(&colors[TEXT_RIGHT_BORDER], GetSysColor(COLOR_BTNSHADOW));

    // barva vybraneho bloku textu
    if (GetFValue(colors[TEXT_FG_SELECTION]) & SCF_DEFAULT)
    {
        if (GetFValue(colors[TEXT_FG_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[TEXT_FG_SELECTION], SG->GetCurrentColor(SALCOL_VIEWER_FG_SELECTED));
        else
            SetRGBPart(&colors[TEXT_FG_SELECTION], GetCOLORREF(colors[TEXT_BK_NORMAL]));
    }
    if (GetFValue(colors[TEXT_BK_SELECTION]) & SCF_DEFAULT)
    {
        if (GetFValue(colors[TEXT_BK_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[TEXT_BK_SELECTION], SG->GetCurrentColor(SALCOL_VIEWER_BK_SELECTED));
        else
            SetRGBPart(&colors[TEXT_BK_SELECTION], GetCOLORREF(colors[TEXT_FG_NORMAL]));
    }

    HDC hdc = GetDC(NULL);
    int bitsPerPixel = GetDeviceCaps(hdc, BITSPIXEL);
    ReleaseDC(NULL, hdc);
    if (bitsPerPixel <= 8)
    {
        if (!palette)
            palette = CreateColorPallete();
        if (palette)
        {
            PALETTEENTRY entries[NUMBER_OF_COLORS];
            int i;
            for (i = 0; i < NUMBER_OF_COLORS; i++)
            {
                entries[i].peRed = GetRValue(colors[i]);
                entries[i].peGreen = GetGValue(colors[i]);
                entries[i].peBlue = GetBValue(colors[i]);
            }
            if (SetPaletteEntries(palette, 0, NUMBER_OF_COLORS, entries))
                UsePalette = TRUE;
        }
    }
    else
        UsePalette = FALSE;
}

// ****************************************************************************

BOOL CreateEnvFont()
{
    CALL_STACK_MESSAGE1("CreateEnvFont()");
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    LOGFONT* lf = &ncm.lfMenuFont;

    if (EnvFont != NULL)
        DeleteObject(EnvFont);
    EnvFont = CreateFontIndirect(lf);
    if (EnvFont == NULL)
    {
        TRACE_E("Font se nepovedl vytvorit.");
        return FALSE;
    }

    // zjistime si vysku
    HDC dc = GetDC(NULL);
    HFONT oldFont = (HFONT)SelectObject(dc, EnvFont);
    TEXTMETRIC tm;
    GetTextMetrics(dc, &tm);
    EnvFontHeight = tm.tmHeight + tm.tmExternalLeading;
    SelectObject(dc, oldFont);
    ReleaseDC(NULL, dc);
    return TRUE;
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SG->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL InitDialogs()
{
    CALL_STACK_MESSAGE1("InitDialogs()");

    HIbeamCursor = LoadCursor(NULL, IDC_IBEAM);
    HArrowCursor = LoadCursor(NULL, IDC_ARROW);
    HWaitCursor = LoadCursor(NULL, IDC_WAIT);

    if (!InitializeWinLib("FILECOMP", DLLInstance))
        return FALSE;
    SetWinLibStrings(LoadStr(IDS_INVALIDNUMBER), LoadStr(IDS_PLUGINNAME));
    SetupWinLibHelp(HTMLHelpCallback);
    if (!CWindow::RegisterUniversalClass(
            CS_DBLCLKS, 0, 0, DLLInstance,
            LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FCICO)),
            HArrowCursor,
            (HBRUSH)(COLOR_ACTIVEBORDER + 1),
            NULL, MAINWINDOW_CLASSNAME, NULL) ||
        !CWindow::RegisterUniversalClass(
            CS_DBLCLKS, 0, 0, DLLInstance,
            NULL,
            LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_SPLIT_VERT)),
            (HBRUSH)(COLOR_WINDOW + 1),
            NULL, SPLITBARWINDOW_CLASSNAME, NULL))
    {
        TRACE_E("RegisterUniversalClass has failed, last error:" << GetLastError());
        ReleaseWinLib(DLLInstance);
        return FALSE;
    }

    WNDCLASSEX windowClass;
    windowClass.cbSize = sizeof(WNDCLASSEX);
    windowClass.style = CS_DBLCLKS;
    windowClass.lpfnWndProc = DefWindowProc;
    windowClass.cbClsExtra = 0;
    windowClass.cbWndExtra = 0;
    windowClass.hInstance = DLLInstance;
    windowClass.hIcon = NULL;
    windowClass.hCursor = HArrowCursor;
    windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    windowClass.lpszMenuName = NULL;
    windowClass.lpszClassName = FILEVIEWWINDOW_CLASSNAME;
    windowClass.hIconSm = NULL;
    if (!RegisterClassEx(&windowClass))
    {
        ReleaseWinLib(DLLInstance);
        if (!UnregisterClass(MAINWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(MAINWINDOW_CLASSNAME) has failed");
        if (!UnregisterClass(SPLITBARWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(SPLITBARWINDOW_CLASSNAME) has failed");
        return FALSE;
    }

    if (!CreateEnvFont())
    {
        ReleaseWinLib(DLLInstance);
        if (!UnregisterClass(MAINWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(MAINWINDOW_CLASSNAME) has failed");
        if (!UnregisterClass(FILEVIEWWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(FILEVIEWWINDOW_CLASSNAME) has failed");
        if (!UnregisterClass(SPLITBARWINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(SPLITBARWINDOW_CLASSNAME) has failed");
        return FALSE;
    }

    HDitheredBrush = CreateDitheredBrush();

    HAccels = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELS));

    CaretWidth = GetSystemMetrics(SM_CXBORDER) * 2;

    return TRUE;
}

void ReleaseDialogs()
{
    CALL_STACK_MESSAGE1("ReleaseDialogs()");
    ReleaseWinLib(DLLInstance);
    if (!UnregisterClass(MAINWINDOW_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(MAINWINDOW_CLASSNAME) has failed");
    if (!UnregisterClass(FILEVIEWWINDOW_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(FILEVIEWWINDOW_CLASSNAME) has failed");
    if (!UnregisterClass(SPLITBARWINDOW_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(SPLITBARWINDOW_CLASSNAME) has failed");
    if (EnvFont)
        DeleteObject(EnvFont);
    if (HDitheredBrush)
        DeleteObject(HDitheredBrush);
    if (Palette)
        DeleteObject(Palette);
}
