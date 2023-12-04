// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <math.h>

#include "lib\\pvw32dll.h"
#include "renderer.h"
#include "pictview.h"
#include "dialogs.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang\lang.rh"

#define PREVIEW_MARGIN 8 // odsazeni stranky od hrany preview v bodech

typedef struct _prevWriteInfo
{
    char* pBits;
    char* pCurPos;
    DWORD size;
} sPrevWriteInfo, *psPrevWriteInfo;

DWORD WINAPI PreviewSeek(void* AppSpecific, LONG NewPos, int Origin)
{
    psPrevWriteInfo pPWI = (psPrevWriteInfo)AppSpecific;
    return NewPos;
}

DWORD WINAPI PreviewWrite(void* AppSpecific, void* pData, DWORD Size)
{
    psPrevWriteInfo pPWI = (psPrevWriteInfo)AppSpecific;
    if (Size > pPWI->size - (pPWI->pCurPos - pPWI->pBits))
    {
        Size = (DWORD)(pPWI->size - (pPWI->pCurPos - pPWI->pBits));
    }
    memcpy(pPWI->pCurPos, pData, Size);
    pPWI->pCurPos += Size;
    return Size;
} /* PreviewWrite */

HBITMAP CreatePreview(CPrintParams* pParams, int width, int height, const RECT* pCrop)
{
    BITMAPINFO bi;
    HBITMAP hBmp;
    sPrevWriteInfo pwi;
    PVSaveImageInfo sii;
    PVCODE code;

    memset(&bi, 0, sizeof(bi));
    memset(&sii, 0, sizeof(sii));
    sii.Flags |= PVSF_FLIP_VERT;
    if (width < 0)
    {
        sii.Flags |= PVSF_FLIP_HOR;
        width = -width;
    }
    if (height < 0)
    {
        sii.Flags ^= PVSF_FLIP_VERT;
        height = -height;
    }
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 32;
    bi.bmiHeader.biWidth = width;
    bi.bmiHeader.biHeight = height;
    hBmp = CreateDIBSection(NULL, &bi, DIB_RGB_COLORS, (void**)&pwi.pBits, NULL, 0);
    if (!hBmp)
    {
        return NULL;
    }
    pwi.pCurPos = pwi.pBits;
    pwi.size = width * height * 4;
    sii.cbSize = sizeof(sii);
    sii.Colors = PV_COLOR_TC32;
    sii.Format = PVF_RAW;
    sii.Height = height;
    sii.Width = width;
    sii.Flags |= PVSF_USERDEFINED_OUTPUT;
    sii.WriteFunc = PreviewWrite;
    sii.SeekFunc = PreviewSeek;
    if (pParams->bMirrorHor)
        sii.Flags ^= PVSF_FLIP_HOR;
    if (pParams->bMirrorVert)
        sii.Flags ^= PVSF_FLIP_VERT;
    if (pCrop)
    {
        sii.CropLeft = pCrop->left;
        sii.CropTop = pCrop->top;
        sii.CropWidth = pCrop->right - pCrop->left + 1;
        sii.CropHeight = pCrop->bottom - pCrop->top + 1;
        // Mirror is applied before crop
        if (pParams->bMirrorHor)
        {
            sii.CropLeft = pParams->pPVII->Width - pCrop->right - 1;
        }
        if (pParams->bMirrorVert)
        {
            sii.CropTop = pParams->pPVII->Height - pCrop->bottom - 1;
        }
    }
    code = PVW32DLL.PVSaveImage(pParams->PVHandle, NULL, &sii, NULL, &pwi, pParams->pPVII->CurrentImage);
    if (code == PVC_OK)
    {
        return hBmp;
    }
    DeleteObject(hBmp);
    return NULL;
} /* CreatePreview */

//****************************************************************************
//
// CPreviewWnd
//

CPreviewWnd::CPreviewWnd(HWND hDlg, int ctrlID, CPrintDlg* printDlg)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    pPrintDlg = printDlg;
    hPreview = NULL;
    previousPrevWidth = previousPrevHeight = 0;
}

CPreviewWnd::~CPreviewWnd()
{
    if (hPreview)
    {
        DeleteObject(hPreview);
    }
}

void CPreviewWnd::Paint(HDC hDC)
{
    int prevWidth, prevHeight, prevLeft, prevTop;
    RECT clientRect;

    GetClientRect(HWindow, &clientRect); // POZOR: rozmery client area nebudou vzdy ctvercove, zalezi na fontech atd.
                                         // budeme se poskytnuty prostor snazit maximalne vyuzit...
    int dstWidth = clientRect.right;
    int dstHeight = clientRect.bottom;

    if (dstWidth < 1 || dstHeight < 1)
    {
        return;
    }

    BOOL releaseDC = FALSE;
    if (hDC == NULL)
    {
        hDC = GetDC(HWindow);
        releaseDC = TRUE;
    }

    // budeme kreslit pres cache, kterou vytvorime/zahodime pri kazdem paintu (nikam nespechame)
    HBITMAP hCacheBmp = CreateCompatibleBitmap(hDC, dstWidth, dstHeight);
    HDC hCacheDC = CreateCompatibleDC(NULL);
    HBITMAP hOldCacheBmp = (HBITMAP)SelectObject(hCacheDC, hCacheBmp);

    double paperWidth;
    double paperHeight;
    double pageLeftMargin;
    double pageTopMargin;
    double pageRightMargin;
    double pageBottomMargin;
    double ratioX, ratioY;
    DWORD sz = sizeof(double);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAPER_WIDTH, &paperWidth, sz);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAPER_HEIGHT, &paperHeight, sz);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAGE_LEFTMARGIN, &pageLeftMargin, sz);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAGE_TOPMARGIN, &pageTopMargin, sz);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAGE_RIGHTMARGIN, &pageRightMargin, sz);
    pPrintDlg->GetPrinterInfo(PRNINFO_PAGE_BOTTOMMARGIN, &pageBottomMargin, sz);

    FillRect(hCacheDC, &clientRect, (HBRUSH)(COLOR_BTNFACE + 1));

    // rozmer custom controlu v bodech, do ktereho vykreslime papir
    int wWidth = clientRect.right - 2 * PREVIEW_MARGIN;
    int wHeight = clientRect.bottom - 2 * PREVIEW_MARGIN;

    double zoomRatio;
    if (paperWidth / paperHeight <= (double)clientRect.right / (double)wHeight)
    {
        zoomRatio = (double)wHeight / paperHeight;
    }
    else
    {
        zoomRatio = (double)wWidth / paperWidth;
    }

    // vycentrujeme stranku do naseho preview prostoru
    RECT pageRect;
    pageRect.left = (LONG)((clientRect.right - paperWidth * zoomRatio) / 2);
    pageRect.right = (LONG)(pageRect.left + paperWidth * zoomRatio);
    pageRect.top = (LONG)((clientRect.bottom - paperHeight * zoomRatio) / 2);
    pageRect.bottom = (LONG)(pageRect.top + paperHeight * zoomRatio);

    // vykreslime stranku
    FillRect(hCacheDC, &pageRect, (HBRUSH)(COLOR_WINDOW + 1));
    Rectangle(hCacheDC, pageRect.left, pageRect.top, pageRect.right, pageRect.bottom);

    // pixel/(point)
    ratioX = (DOUBLE)(pageRect.right - pageRect.left) / (paperWidth * 72);
    ratioY = (DOUBLE)(pageRect.bottom - pageRect.top) / (paperHeight * 72);

    // realne po-tisknutelna oblast bude mensi o okraje
    pageRect.left += (LONG)(pageLeftMargin * zoomRatio);
    pageRect.top += (LONG)(pageTopMargin * zoomRatio);
    pageRect.right -= (LONG)(pageRightMargin * zoomRatio);
    pageRect.bottom -= (LONG)(pageBottomMargin * zoomRatio);

    prevLeft = prevTop = 0; // preview top-left corner

    if (ratioX > ratioY)
    {
        ratioX = ratioY;
    }

    if (pPrintDlg->Params.bFit)
    {
        prevWidth = pageRect.right - pageRect.left;
        prevHeight = pageRect.bottom - pageRect.top;
        if (pPrintDlg->Params.bKeepAspect)
        {

            ratioX = paperWidth / (pPrintDlg->origImgWidth / 72);
            ratioY = paperHeight / (pPrintDlg->origImgHeight / 72);
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
        prevWidth = (int)(pPrintDlg->Params.Width * ratioX);
        prevHeight = (int)(pPrintDlg->Params.Height * ratioY);
    }
    if ((previousPrevWidth != prevWidth) || (previousPrevHeight != prevHeight))
    {
        // Options have changed -> preview size has changed
        if (hPreview)
        {
            DeleteObject(hPreview);
            hPreview = NULL;
        }
    }
    if (hPreview == NULL)
    {
        hPreview = CreatePreview(&pPrintDlg->Params, prevWidth, prevHeight,
                                 pPrintDlg->Params.bSelection ? pPrintDlg->Params.ImageCage : NULL);
        previousPrevWidth = prevWidth;
        previousPrevHeight = prevHeight;
    }

    prevWidth = abs(prevWidth);
    prevHeight = abs(prevHeight);

    if (!pPrintDlg->Params.bFit || pPrintDlg->Params.bKeepAspect)
    {
        if (pPrintDlg->Params.bCenter)
        {
            prevTop = (pageRect.bottom - pageRect.top - prevHeight) / 2;
            prevLeft = (pageRect.right - pageRect.left - prevWidth) / 2;
        }
        else
        {
            // convert  paperWidth: inches;  pageRect: pixels;  Params: points
            double scale = (double)(pageRect.right - pageRect.left) / (paperWidth * 72);
            prevTop = (int)(pPrintDlg->Params.Top * scale);
            prevLeft = (int)(pPrintDlg->Params.Left * scale);
        }
    }

    DrawFocusRect(hCacheDC, &pageRect); // vizualizace tisknutelne oblasti

    // FIXME print -- zobrazit obrazek ve sprave orientaci, meritku a posunuti
    // mozna by bylo sikovnejsi pouzit nejake transformace DC nez to otacet rucne?
    // netusim zda jsou ale podporovany na vsech tiskarnach
    //
    // dal je potreba zobrazit oramovani obrazku, pokud je zapnuty checkbox IDC_PRINT_BOX

    if (hPreview)
    {
        HDC hPreviewDC = CreateCompatibleDC(hDC);
        HBITMAP hOldBmp = (HBITMAP)SelectObject(hPreviewDC, hPreview);
        HRGN hClipRgn = CreateRectRgn(pageRect.left, pageRect.top,
                                      pageRect.right, pageRect.bottom);

        SelectClipRgn(hCacheDC, hClipRgn);
        BitBlt(hCacheDC, pageRect.left + prevLeft, pageRect.top + prevTop,
               prevWidth, prevHeight, hPreviewDC, 0, 0, SRCCOPY);
        SelectObject(hPreviewDC, hOldBmp);
        SelectClipRgn(hCacheDC, NULL);
        DeleteObject(hClipRgn);
        DeleteDC(hPreviewDC);
    }
    if (pPrintDlg->Params.bBoundingBox)
    {
        pageRect.left += prevLeft;
        pageRect.top += prevTop;
        pageRect.right = pageRect.left + prevWidth;
        pageRect.bottom = pageRect.top + prevHeight;
        DrawFocusRect(hCacheDC, &pageRect); // vizualizace vlastniho nahledu
    }

    // vse hodime do obrazovky
    BitBlt(hDC, 0, 0, dstWidth, dstHeight, hCacheDC, 0, 0, SRCCOPY);

    // uvolnime cache
    SelectObject(hCacheDC, hOldCacheBmp);
    DeleteDC(hCacheDC);
    DeleteObject(hCacheBmp);

    if (releaseDC)
        ReleaseDC(HWindow, hDC);
}

LRESULT
CPreviewWnd::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CProgressBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = BeginPaint(HWindow, &ps);
        Paint(hDC);
        EndPaint(HWindow, &ps);
        return 0;
    }
    }

    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPrintParams
//

void CPrintParams::Clear()
{
    bCenter = TRUE;
    Left = 0;
    Top = 0;
    Width = 0;
    Height = 0;
    Scale = 100;
    bBoundingBox = FALSE;
    bSelection = FALSE;
    bFit = TRUE;
    bKeepAspect = TRUE;
    Units = untMM;
}

//****************************************************************************
//
// CPrintDlg
//

CPrintDlg::CPrintDlg(HWND hParent, CPrintParams* printParams)
    : CCommonDialog(HLanguage, IDD_PRINT, IDD_PRINT, hParent)
{
    //  PrintParams = printParams;
    Params = *printParams; //*PrintParams;

    HDevMode = NULL;
    HDevNames = NULL;
    HPrinterDC = NULL;
    printerName[0] = 0;
}

CPrintDlg::~CPrintDlg()
{
    ReleasePrinterHandles();
}

void CPrintDlg::ReleasePrinterHandles()
{
    if (HDevMode != NULL)
    {
        GlobalFree(HDevMode);
        HDevMode = NULL;
    }
    if (HDevNames != NULL)
    {
        GlobalFree(HDevNames);
        HDevNames = NULL;
    }
    if (HPrinterDC != NULL)
    {
        DeleteDC(HPrinterDC);
        HPrinterDC = NULL;
    }
} /* CPrintDlg::ReleasePrinterHandles */

void CPrintDlg::Validate(CTransferInfo& ti)
{
}

int UnitsIDs[] = {IDS_UNIT_INCHES, IDS_UNIT_CM, IDS_UNIT_MM, IDS_UNIT_POINTS, IDS_UNIT_PICAS, -1}; // musi koncit terminatorem -1
CUnitsEnum Units[] = {untInches, untCM, untMM, untPoints, untPicas};

void SetNumber(HWND hWnd, double value)
{
    TCHAR text[16];
    LPTSTR s;

    _stprintf(text, _T("%1.4f"), value);
    s = (LPTSTR)_tcschr(text, '.');
    if (s)
    {
        // trim terminating decimaL zeros
        s = text + _tcslen(text) - 1;
        while (*s == '0')
            *s-- = 0;
        if (*s == '.')
            *s = 0;
    }
    SetWindowText(hWnd, text);
} /* SetNumber */

// naplni combobox jednotkama, vybere prvni
void FillUnits(HWND hDlg, int cbResID, CUnitsEnum select, double value)
{
    HWND hCombo = GetDlgItem(hDlg, cbResID);
    int selectIndex = 0;

    SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
    int i;
    for (i = 0; UnitsIDs[i] != -1; i++)
    {
        LRESULT index = SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)LoadStr(UnitsIDs[i]));
        SendMessage(hCombo, CB_SETITEMDATA, index, (LPARAM)Units[i]);
        if (select == Units[i])
            selectIndex = i;
    }
    SendMessage(hCombo, CB_SETCURSEL, selectIndex, 0);
    SetNumber(GetDlgItem(hDlg, cbResID - 1), value);
} /* FillUnits */

// vrati zvolenou jednotku pro dany combobox; v pripade chyby vraci untInches
CUnitsEnum GetUnits(HWND hDlg, int cbResID)
{
    HWND hCombo = GetDlgItem(hDlg, cbResID);
    LRESULT index = SendMessage(hCombo, CB_GETCURSEL, 0, 0);
    if (index == CB_ERR)
    {
        TRACE_E("GetUnits(): no selected item for combobox id=" << cbResID);
        return untInches;
    }

    LRESULT data = SendMessage(hCombo, CB_GETITEMDATA, index, 0);
    if (data == CB_ERR)
    {
        TRACE_E("GetUnits(): no valid item for combobox id=" << cbResID);
        return untInches;
    }

    return (CUnitsEnum)data;
}

void CPrintDlg::FillUnits()
{
    double val = 1;

    switch (Params.Units)
    {
    case untInches:
        val /= 72;
        break;
    case untCM:
        val /= 72.0 / 2.54;
        break;
    case untMM:
        val /= 72.0 / 25.4;
        break;
    case untPoints: // 1 in = 72 pt
        break;
    case untPicas: // 1 in = 1440 pi; 1 pt = 20 pi
        val *= 20;
        break;
    }
    ::FillUnits(HWindow, IDC_PRINT_LEFT_UNITS, Params.Units, Params.Left * val);
    ::FillUnits(HWindow, IDC_PRINT_TOP_UNITS, Params.Units, Params.Top * val);
    ::FillUnits(HWindow, IDC_PRINT_WIDTH_UNITS, Params.Units, Params.Width * val);
    ::FillUnits(HWindow, IDC_PRINT_HEIGHT_UNITS, Params.Units, Params.Height * val);
}

void CPrintDlg::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_PRINT_CENTER, Params.bCenter);
    ti.CheckBox(IDC_PRINT_FIT, Params.bFit);
    ti.CheckBox(IDC_PRINT_ASPECT, Params.bKeepAspect);
    ti.CheckBox(IDC_PRINT_BOX, Params.bBoundingBox);
    ti.CheckBox(IDC_PRINT_SELECTION, Params.bSelection);

    // naplnime comboboxy
    if (ti.Type == ttDataToWindow)
    {
        SetScale();
        FillUnits();
        EnableControls();
    }
}

void CPrintDlg::EnableControls()
{
    BOOL bFit = IsDlgButtonChecked(HWindow, IDC_PRINT_FIT) == BST_CHECKED;
    BOOL bAspect = IsDlgButtonChecked(HWindow, IDC_PRINT_ASPECT) == BST_CHECKED;

    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_SCALE), !bFit);
    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_WIDTH), !bFit);
    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_HEIGHT), !bFit);
    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_CENTER), !bFit || bAspect);
    //  EnableWindow(GetDlgItem(HWindow, IDC_PRINT_ASPECT), bFit);
    /*  if (bFit)
  {
    CheckDlgButton(HWindow, IDC_PRINT_CENTER, BST_CHECKED);
  }*/

    // tento blok musi nasledovat az po nastaveni IDC_PRINT_CENTER
    BOOL bTopLeft = (IsDlgButtonChecked(HWindow, IDC_PRINT_CENTER) != BST_CHECKED) && (!bFit || bAspect);
    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_LEFT), bTopLeft);
    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_TOP), bTopLeft);

    EnableWindow(GetDlgItem(HWindow, IDC_PRINT_SELECTION), Params.ImageCage != NULL);
}

void CPrintDlg::OnPrintSetup()
{
    PRINTDLG pd;
    memset(&pd, 0, sizeof(pd));
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = HWindow;
    pd.hDevMode = HDevMode;
    pd.hDevNames = HDevNames;
    pd.nMaxPage = 1;
    pd.nMinPage = 1;
    pd.nFromPage = 1;
    pd.nToPage = 1;
    pd.nCopies = 1;
    pd.Flags = PD_PRINTSETUP | PD_RETURNDC | PD_USEDEVMODECOPIESANDCOLLATE;
    if (PrintDlg(&pd))
    {
        // ulozime nove parametry tiskarny
        if (HPrinterDC != NULL)
        {
            DeleteDC(HPrinterDC);
            HPrinterDC = pd.hDC;
        }
        if (HDevMode != pd.hDevMode)
        { // maybe not needed on W2K+
            GlobalFree(HDevMode);
            HDevMode = pd.hDevMode;
        }
        if (HDevNames != pd.hDevNames)
        { // maybe not needed on W2K+
            GlobalFree(HDevNames);
            HDevNames = pd.hDevNames;
        }
        // Store the printer name & port for use by all Print dialogs newly opened in other Windows
        printerName[0] = 0;
        EnterCriticalSection(&G.CS);
        if (G.pDevNames)
        {
            free(G.pDevNames);
            G.pDevNames = NULL;
            G.DevNamesSize = 0;
        }
        if (pd.hDevNames)
        {
            SIZE_T size = GlobalSize(pd.hDevNames);

            G.pDevNames = (DEVNAMES*)malloc(size);
            if (G.pDevNames)
            {
                G.DevNamesSize = size;
                memcpy(G.pDevNames, GlobalLock(pd.hDevNames), size);
                _sntprintf_s(printerName, _TRUNCATE, _T("%s, %s"), (char*)G.pDevNames + G.pDevNames->wDeviceOffset,
                             (char*)G.pDevNames + G.pDevNames->wOutputOffset);
                GlobalUnlock(pd.hDevNames);
            }
        }
        if (G.pDevMode)
        {
            free(G.pDevMode);
            G.pDevMode = NULL;
            G.DevModeSize = 0;
        }
        if (pd.hDevMode)
        {
            SIZE_T size = GlobalSize(pd.hDevMode);

            G.pDevMode = (DEVMODE*)malloc(size);
            if (G.pDevMode)
            {
                G.DevModeSize = size;
                memcpy(G.pDevMode, GlobalLock(pd.hDevMode), size);
                GlobalUnlock(pd.hDevMode);
            }
        }
        LeaveCriticalSection(&G.CS);
    }
    if (HWindow != NULL) // pri zavreni z jineho threadu (shutdown / zavreni hl. okna Salama) uz dialog neexistuje
    {
        CalcImgSize(&origImgWidth, &origImgHeight);

        Preview->Paint(NULL);
    }
}

BOOL CPrintDlg::MyGetDefaultPrinter()
{
    PRINTDLG pd = {0};
    printerName[0] = 0;
    pd.lStructSize = sizeof(pd);
    pd.hwndOwner = Parent;
    pd.Flags = PD_RETURNDEFAULT | PD_NOWARNING | PD_RETURNDC;
    EnterCriticalSection(&G.CS);
    if (G.pDevNames && G.pDevMode)
    {
        HDC hDC;

        pd.hDevNames = GlobalAlloc(GMEM_MOVEABLE, G.DevNamesSize);
        if (pd.hDevNames)
        {
            memcpy(GlobalLock(pd.hDevNames), G.pDevNames, G.DevNamesSize);
            _sntprintf_s(printerName, _TRUNCATE, _T("%s, %s"), (LPTSTR)G.pDevNames + G.pDevNames->wDeviceOffset,
                         (LPTSTR)G.pDevNames + G.pDevNames->wOutputOffset);
            GlobalUnlock(pd.hDevNames);
        }
        pd.hDevMode = GlobalAlloc(GMEM_MOVEABLE, G.DevModeSize);
        if (pd.hDevMode)
        {
            memcpy(GlobalLock(pd.hDevMode), G.pDevMode, G.DevModeSize);
            GlobalUnlock(pd.hDevMode);
            pd.Flags &= ~PD_RETURNDEFAULT;
        }
        hDC = CreateDC(NULL, (LPTSTR)G.pDevNames + G.pDevNames->wDeviceOffset, NULL, G.pDevMode);
        if (hDC)
        {
            ReleasePrinterHandles();
            HDevMode = pd.hDevMode;
            HDevNames = pd.hDevNames;
            HPrinterDC = hDC;
            LeaveCriticalSection(&G.CS);
            return TRUE;
        }
        LeaveCriticalSection(&G.CS);
        // FIXME: Quick and dirty error reporting
        SalamanderGeneral->SalMessageBox(Parent, "Could not create device context for the printer", LoadStr(IDS_ERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    // 2009.04.27: 32bit app elevated on admin level in Vista64: PrintDlg(PD_RETURNDEFAULT) fails
    // -> Therefore we use another approach.
    DWORD bufSize = SizeOf(printerName);
    if (GetDefaultPrinter(printerName, &bufSize))
    {
        HANDLE hPrinter;
        if (OpenPrinter(printerName, &hPrinter, NULL))
        {
            PRINTER_INFO_2* p2;

            GetPrinter(hPrinter, 2, NULL, 0, &bufSize);
            p2 = (PRINTER_INFO_2*)malloc(bufSize);
            if (p2 && GetPrinter(hPrinter, 2, (LPBYTE)p2, bufSize, &bufSize))
            {
                SIZE_T sizeDevMode = sizeof(*p2->pDevMode) + p2->pDevMode->dmDriverExtra;
                G.pDevMode = (DEVMODE*)malloc(sizeDevMode);
                if (G.pDevMode)
                {
                    G.DevModeSize = sizeDevMode;
                    memcpy(G.pDevMode, p2->pDevMode, G.DevModeSize);
                    // Compute size of DEVNAMES structure from PRINTER_INFO_2's data
                    DWORD drvNameLen = (int)_tcslen(p2->pDriverName) + 1;  // driver name
                    DWORD ptrNameLen = (int)_tcslen(p2->pPrinterName) + 1; // printer name
                    DWORD porNameLen = (int)_tcslen(p2->pPortName) + 1;    // port name
                    SIZE_T size = sizeof(DEVNAMES) + (drvNameLen + ptrNameLen + porNameLen) * sizeof(TCHAR);
                    G.pDevNames = (DEVNAMES*)malloc(size);
                    if (G.pDevNames)
                    {
                        G.DevNamesSize = size;
                        int tcOffset = sizeof(DEVNAMES) / sizeof(TCHAR);

                        G.pDevNames->wDriverOffset = tcOffset;
                        memcpy((LPTSTR)G.pDevNames + tcOffset, p2->pDriverName, drvNameLen * sizeof(TCHAR));
                        tcOffset += drvNameLen;

                        G.pDevNames->wDeviceOffset = tcOffset;
                        memcpy((LPTSTR)G.pDevNames + tcOffset, p2->pPrinterName, ptrNameLen * sizeof(TCHAR));
                        tcOffset += ptrNameLen;

                        G.pDevNames->wOutputOffset = tcOffset;
                        memcpy((LPTSTR)G.pDevNames + tcOffset, p2->pPortName, porNameLen * sizeof(TCHAR));
                        G.pDevNames->wDefault = 0;
                        free(p2);
                        ClosePrinter(hPrinter);
                        LeaveCriticalSection(&G.CS);
                        // Have the handles and DC created
                        return MyGetDefaultPrinter();
                    }
                    free(G.pDevMode);
                    G.pDevMode = NULL;
                    G.DevModeSize = 0;
                }
            }
            free(p2);
            ClosePrinter(hPrinter);
        }
    }
    LeaveCriticalSection(&G.CS);
    // The following code can be removed (it is not needed on W2K+).
    // However, a better error-handling & reporting needs to be added to the above code.
    if (PrintDlg(&pd))
    {
        // ulozime nove parametry tiskarny
        ReleasePrinterHandles();
        HDevMode = pd.hDevMode;
        HDevNames = pd.hDevNames;
        HPrinterDC = pd.hDC;

        if (HDevNames)
        {
            DEVNAMES* pDevNames = (DEVNAMES*)GlobalLock(HDevNames);

            _sntprintf_s(printerName, _TRUNCATE, _T("%s, %s"), (LPTSTR)pDevNames + pDevNames->wDeviceOffset,
                         (LPTSTR)pDevNames + pDevNames->wOutputOffset);
            GlobalUnlock(HDevNames);
        }

        return TRUE;
    }
    else
    {
        DWORD err = CommDlgExtendedError();
        int resID = IDS_PRINT_SETUPFAILED;
        if (err != 0)
        {
            if (err == PDERR_NODEFAULTPRN)
            {
                resID = IDS_PRINT_NODEFPRINTER;
            }
            else if (err == PDERR_NODEVICES)
            {
                resID = IDS_PRINT_NOPRINTERS;
            }
        }
        SalamanderGeneral->SalMessageBox(Parent, LoadStr(resID), LoadStr(IDS_ERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
}

BOOL CheckDCIValueSize(DWORD valSz, void* value, DWORD& bufSz, BOOL& retVal)
{
    if (value == NULL)
    {
        bufSz = valSz;
        retVal = TRUE;
        // DCI cannot be processed
        TRACE_E("CheckDCIValueSize(): value == NULL!");
        return FALSE;
    }
    else if (bufSz < valSz)
    {
        bufSz = valSz;
        retVal = FALSE;
        // DCI cannot be processed
        TRACE_E("CheckDCIValueSize(): bufSz < valSz!");
        return FALSE;
    }
    // DCI can be processed
    return TRUE;
}

BOOL CPrintDlg::GetPrinterInfo(DWORD index, void* value, DWORD& size)
{
    BOOL retVal = FALSE;
    switch (index)
    {
        /*    case PRNINFO_ORIENTATION:
    {
      if (HDevMode == NULL)
      {
        TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
        return FALSE;
      }
      if (CheckDCIValueSize(sizeof(short), value, size, retVal))
      {
        DEVMODE* dm = (DEVMODE*)GlobalLock(HDevMode);
        retVal = FALSE;
        if ((dm->dmFields & DM_ORIENTATION) != 0)
        {
          *(short*)value = dm->dmOrientation;
          retVal = TRUE;
        }
        GlobalUnlock(HDevMode);
      }
      break;
    }*/
        /*
    case PRNINFO_PAGE_SIZE:
    {
      if (HDevMode == NULL)
      {
        TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
        return FALSE;
      }
      if (CheckDCIValueSize(sizeof(short), value, size, retVal))
      {
        DEVMODE* dm = (DEVMODE*)GlobalLock(HDevMode);
        retVal = FALSE;
        if ((dm->dmFields & DM_PAPERSIZE) != 0)
        {
          *(short*)value = dm->dmPaperSize;
          retVal = TRUE;
        }
        GlobalUnlock(HDevMode);
      }
      break;
    }
*/
    case PRNINFO_PAPER_WIDTH:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)GetDeviceCaps(HPrinterDC, PHYSICALWIDTH) /
                              (double)GetDeviceCaps(HPrinterDC, LOGPIXELSX);
            retVal = TRUE;
        }
        break;
    }

    case PRNINFO_PAPER_HEIGHT:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)GetDeviceCaps(HPrinterDC, PHYSICALHEIGHT) /
                              (double)GetDeviceCaps(HPrinterDC, LOGPIXELSY);
            retVal = TRUE;
        }
        break;
    }

    case PRNINFO_PAGE_LEFTMARGIN:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)GetDeviceCaps(HPrinterDC, PHYSICALOFFSETX) /
                              (double)GetDeviceCaps(HPrinterDC, LOGPIXELSX);
            retVal = TRUE;
        }
        break;
    }

    case PRNINFO_PAGE_TOPMARGIN:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)GetDeviceCaps(HPrinterDC, PHYSICALOFFSETY) /
                              (double)GetDeviceCaps(HPrinterDC, LOGPIXELSY);
            retVal = TRUE;
        }
        break;
    }

    case PRNINFO_PAGE_RIGHTMARGIN:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)(GetDeviceCaps(HPrinterDC, PHYSICALWIDTH) -
                                       GetDeviceCaps(HPrinterDC, PHYSICALOFFSETX) -
                                       GetDeviceCaps(HPrinterDC, HORZRES)) /
                              GetDeviceCaps(HPrinterDC, LOGPIXELSX);
            retVal = TRUE;
        }
        break;
    }

    case PRNINFO_PAGE_BOTTOMMARGIN:
    {
        if (HDevMode == NULL)
        {
            TRACE_E("No printer is selected, call MyGetDefaultPrinter()!");
            return FALSE;
        }
        if (CheckDCIValueSize(sizeof(double), value, size, retVal))
        {
            *(double*)value = (double)(GetDeviceCaps(HPrinterDC, PHYSICALHEIGHT) -
                                       GetDeviceCaps(HPrinterDC, PHYSICALOFFSETY) -
                                       GetDeviceCaps(HPrinterDC, VERTRES)) /
                              GetDeviceCaps(HPrinterDC, LOGPIXELSY);
            retVal = TRUE;
        }
        break;
    }

    default:
    {
        TRACE_E("CPrintDlg::GetPrinterInfo() unknown index=" << index);
    }
    }
    return retVal;
} /* CPrintDlg::GetPrinterInfo */

// Input: in inches; output: in points;
void CPrintDlg::CalcImgSize(double* pImgWidth, double* pImgHeight)
{
    int horDPI, verDPI;
    int width = Params.pPVII->Width;
    int height = Params.pPVII->Height;

    if (Params.pPVII->HorDPI)
    {
        horDPI = Params.pPVII->HorDPI;
    }
    else
    {
        horDPI = GetDeviceCaps(HPrinterDC, LOGPIXELSX);
    }
    if (Params.pPVII->VerDPI)
    {
        verDPI = Params.pPVII->VerDPI;
    }
    else
    {
        verDPI = GetDeviceCaps(HPrinterDC, LOGPIXELSY);
    }
    if (Params.ImageCage && Params.bSelection)
    {
        width = Params.ImageCage->right - Params.ImageCage->left + 1;
        height = Params.ImageCage->bottom - Params.ImageCage->top + 1;
    }
    // get the real image size in points
    *pImgWidth = ((DOUBLE)width / horDPI) * 72;
    *pImgHeight = ((DOUBLE)height / verDPI) * 72;
} /* CPrintDlg::CalcImgSize */

void CPrintDlg::UpdateImageParams(CPrintParams* printParams)
{
    Params.pPVII = printParams->pPVII;
    Params.PVHandle = printParams->PVHandle;
    Params.ImageCage = printParams->ImageCage;
    Params.bMirrorHor = printParams->bMirrorHor;
    Params.bMirrorVert = printParams->bMirrorVert;
}

double CPrintDlg::GetNumber(HWND hEdit)
{
    TCHAR text[32];
    double val = 0;

    GetWindowText(hEdit, text, SizeOf(text));
    _stscanf(text, _T("%lf"), &val);
    switch (Params.Units)
    {
    case untInches:
        val *= 72;
        break;
    case untCM:
        val *= 72.0 / 2.54;
        break;
    case untMM:
        val *= 72.0 / 25.4;
        break;
    case untPoints: // 1 in = 72 pt
        break;
    case untPicas: // 1 in = 1440 pi; 1 pt = 20 pi
        val /= 20;
        break;
    }
    return val;
} /* CPrintDlg::GetNumber */

void CPrintDlg::SetScale()
{
    TCHAR text[16];
    LPTSTR s;

    _stprintf(text, _T("%1.2f"), Params.Scale);
    s = (LPTSTR)_tcschr(text, '.');
    if (s)
    {
        // trim terminating decimaL zeros
        s = text + _tcslen(text) - 1;
        while (*s == '0')
            *s-- = 0;
        if (*s == '.')
            *s = 0;
    }
    SetWindowText(GetDlgItem(HWindow, IDC_PRINT_SCALE), text);
} /* CPrintDlg::SetScale */

void CPrintDlg::SetAspect()
{
    double widthScale = Params.Width / origImgWidth;
    double heightScale = Params.Height / origImgHeight;

    Params.Scale = widthScale * 100;
    SetScale();
    Params.bKeepAspect = (fabs(widthScale / heightScale - 1) < 1e-3) ? TRUE : FALSE;
    CheckDlgButton(HWindow, IDC_PRINT_ASPECT, Params.bKeepAspect);
} /* CPrintDlg::SetAspect */

INT_PTR CPrintDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // nahodime staticu CLIENTEDGE
        HWND hChild = GetDlgItem(HWindow, IDC_PRINT_PREVIEW);
        DWORD style = GetWindowLong(hChild, GWL_EXSTYLE);
        style |= WS_EX_CLIENTEDGE;
        SetWindowLong(hChild, GWL_EXSTYLE, style);
        SetWindowPos(hChild, 0, 0, 0, 100, 100, SWP_NOSIZE | SWP_NOMOVE | SWP_NOZORDER | SWP_FRAMECHANGED);

        if ((Preview = new CPreviewWnd(HWindow, IDC_PRINT_PREVIEW, this)) == NULL)
            TRACE_E("Low memory");
        CalcImgSize(&origImgWidth, &origImgHeight);
        Params.Scale = 100;
        Params.Width = origImgWidth;
        Params.Height = origImgHeight;
        SetDlgItemText(HWindow, IDC_PRINT_PRINTER, printerName);
        break;
    }

    case WM_COMMAND:
    {
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
            EnableControls();
            break;
        case EN_UPDATE:
            // This notification is sent before screen is updated
            // we have to remove all non-numeric characters
            CleanNonNumericChars((HWND)lParam, FALSE, TRUE, TRUE);
            break;
        case EN_KILLFOCUS:
            switch (LOWORD(wParam))
            {
            case IDC_PRINT_LEFT:
                Params.Left = GetNumber((HWND)lParam);
                break;
            case IDC_PRINT_TOP:
                Params.Top = GetNumber((HWND)lParam);
                break;
            case IDC_PRINT_WIDTH:
                Params.Width = GetNumber((HWND)lParam);
                if (Params.bKeepAspect && (fabs(Params.Width) > 1e-3))
                {
                    Params.Height = origImgHeight * Params.Width / origImgWidth;
                    FillUnits();
                    SetAspect();
                }
                break;
            case IDC_PRINT_HEIGHT:
                Params.Height = GetNumber((HWND)lParam);
                if (Params.bKeepAspect && (fabs(Params.Height) > 1e-3))
                {
                    Params.Width = origImgWidth * Params.Height / origImgHeight;
                    FillUnits();
                    SetAspect();
                }
                break;
            case IDC_PRINT_SCALE:
            {
                TCHAR text[32];

                GetWindowText((HWND)lParam, text, SizeOf(text));
                _stscanf(text, _T("%lf"), &Params.Scale);
                Params.Width = origImgWidth * Params.Scale / 100;
                Params.Height = origImgHeight * Params.Scale / 100;
                FillUnits(); // reset width & height
                SetAspect();
                break;
            }
            }
            Preview->Paint(NULL);
            break;
        case CBN_SELCHANGE:
            Params.Units = (CUnitsEnum)SendMessage((HWND)lParam, CB_GETITEMDATA, SendMessage((HWND)lParam, CB_GETCURSEL, 0, 0), 0);
            FillUnits();
            break;
        }

        switch (LOWORD(wParam))
        {
        case IDC_PRINT_BOX:
            Params.bBoundingBox = IsDlgButtonChecked(HWindow, IDC_PRINT_BOX) == BST_CHECKED;
            Preview->Paint(NULL);
            break;
        case IDC_PRINT_CENTER:
            Params.bCenter = IsDlgButtonChecked(HWindow, IDC_PRINT_CENTER) == BST_CHECKED;
            Preview->Paint(NULL);
            break;
        case IDC_PRINT_FIT:
            Params.bFit = IsDlgButtonChecked(HWindow, IDC_PRINT_FIT) == BST_CHECKED;
            if (Params.bFit)
            {
                /*             SetNumber(GetDlgItem(HWindow, IDC_PRINT_WIDTH), 
             SetNumber(GetDlgItem(HWindow, IDC_PRINT_HEIGHT), */
            }
            else
            {
                FillUnits();
            }
            Preview->Paint(NULL);
            break;
        case IDC_PRINT_ASPECT:
            Params.bKeepAspect = IsDlgButtonChecked(HWindow, IDC_PRINT_ASPECT) == BST_CHECKED;
            if (Params.bKeepAspect)
            {
                Params.Width = origImgWidth * Params.Scale / 100;
                Params.Height = origImgHeight * Params.Scale / 100;
                FillUnits(); // reset width & height
            }
            Preview->Paint(NULL);
            break;
        case IDC_PRINT_SELECTION:
            Params.bSelection = IsDlgButtonChecked(HWindow, IDC_PRINT_SELECTION) == BST_CHECKED;
            CalcImgSize(&origImgWidth, &origImgHeight);
            Params.Width = origImgWidth * Params.Scale / 100;
            Params.Height = origImgHeight * Params.Scale / 100;
            FillUnits(); // reset width & height
            SetAspect();
            Preview->Paint(NULL);
            break;
        case IDC_PRINT_SETUP:
            OnPrintSetup();
            if (HWindow != NULL) // pri zavreni z jineho threadu (shutdown / zavreni hl. okna Salama) uz dialog neexistuje
                SetDlgItemText(HWindow, IDC_PRINT_PRINTER, printerName);
            break;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
} /* CPrintDlg::DialogProc */
