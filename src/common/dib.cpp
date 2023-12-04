// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <ostream>
#include <limits.h>
#include <commctrl.h> // potrebuju LPCOLORMAP

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "messages.h"
#include "handles.h"

#include "dib.h"

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

HBITMAP LoadBitmapAndMapColors(HINSTANCE hInst, HRSRC hRsrc, int mapCount,
                               COLORREF* mapColor, COLORREF* toColor)
{
    HGLOBAL hglb;
    if ((hglb = LoadResource(hInst, hRsrc)) == NULL)
    {
        TRACE_E("Unable to load resource of bitmap.");
        return NULL;
    }

    LPBITMAPINFOHEADER lpBitmap = (LPBITMAPINFOHEADER)LockResource(hglb);
    if (lpBitmap == NULL)
        return NULL;

    // make copy of BITMAPINFOHEADER so we can modify the color table
    const int nColorTableSize = 16;
    UINT nSize = lpBitmap->biSize + nColorTableSize * sizeof(RGBQUAD);
    LPBITMAPINFOHEADER lpBitmapInfo = (LPBITMAPINFOHEADER)malloc(nSize);
    if (lpBitmapInfo == NULL)
    {
        TRACE_E("Out of memory.");
        return NULL;
    }
    memcpy(lpBitmapInfo, lpBitmap, nSize);

    // color table is in RGBQUAD DIB format
    RGBQUAD* pColorTable = (RGBQUAD*)(((LPBYTE)lpBitmapInfo) + (UINT)lpBitmapInfo->biSize);

    for (int iColor = 0; iColor < nColorTableSize; iColor++)
    {
        COLORREF color = RGB(pColorTable[iColor].rgbRed,
                             pColorTable[iColor].rgbGreen,
                             pColorTable[iColor].rgbBlue);
        // look for matching RGBQUAD color in original
        for (int i = 0; i < mapCount; i++)
        {
            if (color == mapColor[i])
            {
                pColorTable[iColor].rgbRed = GetRValue(toColor[i]);
                pColorTable[iColor].rgbGreen = GetGValue(toColor[i]);
                pColorTable[iColor].rgbBlue = GetBValue(toColor[i]);
                break;
            }
        }
    }

    HDC hDCScreen = HANDLES(GetDC(NULL));
    LPBYTE lpBits;
    lpBits = (LPBYTE)(lpBitmap + 1);
    lpBits += (1 << (lpBitmapInfo->biBitCount)) * sizeof(RGBQUAD);
    HBITMAP hbm = HANDLES(CreateDIBitmap(hDCScreen, lpBitmapInfo, CBM_INIT, lpBits,
                                         (LPBITMAPINFO)lpBitmapInfo, DIB_RGB_COLORS));
    if (hbm == NULL)
        TRACE_E("Unable to create bitmap.");
    HANDLES(ReleaseDC(NULL, hDCScreen));

    // free copy of bitmap info struct and resource itself
    free(lpBitmapInfo);
    FreeResource(hglb);

    return hbm;
}

// WIDTHBYTES takes # of bits in a scan line and rounds up to nearest
//  word.

#define WIDTHBYTES(bits) (((bits) + 31) / 32 * 4)

// Given a pointer to a DIB header, return TRUE if is a Windows 3.0 style
//  DIB, false if otherwise (PM style DIB).

#define IS_WIN30_DIB(lpbi) ((*(LPDWORD)(lpbi)) == sizeof(BITMAPINFOHEADER))

#define PALVERSION 0x300
#define WIN30_ONLY

//---------------------------------------------------------------------
//
// Function:   DIBNumColors
//
// Purpose:    Given a pointer to a DIB, returns a number of colors in
//             the DIB's color table.
//
// Parms:      lpbi == pointer to DIB header (either BITMAPINFOHEADER
//                       or BITMAPCOREHEADER)
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

WORD DIBNumColors(LPSTR lpbi)
{
    WORD wBitCount;

#ifndef WIN30_ONLY
    if (IS_WIN30_DIB(lpbi))
    {
#endif
        DWORD dwClrUsed;

        dwClrUsed = ((LPBITMAPINFOHEADER)lpbi)->biClrUsed;

        if (dwClrUsed)
            return (WORD)dwClrUsed;
#ifndef WIN30_ONLY
    }
#endif

    // Calculate the number of colors in the color table based on
    //  the number of bits per pixel for the DIB.

#ifndef WIN30_ONLY
    if (IS_WIN30_DIB(lpbi))
#endif
        wBitCount = ((LPBITMAPINFOHEADER)lpbi)->biBitCount;
#ifndef WIN30_ONLY
    else
        wBitCount = ((LPBITMAPCOREHEADER)lpbi)->bcBitCount;
#endif

    switch (wBitCount)
    {
    case 1:
        return 2;

    case 4:
        return 16;

    case 8:
        return 256;

    default:
        return 0;
    }
}

//---------------------------------------------------------------------
//
// Function:   PaletteSize
//
// Purpose:    Given a pointer to a DIB, returns number of bytes
//             in the DIB's color table.
//
// Parms:      lpbi == pointer to DIB header (either BITMAPINFOHEADER
//                       or BITMAPCOREHEADER)
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

WORD PaletteSize(LPSTR lpbi)
{
#ifndef WIN30_ONLY
    if (IS_WIN30_DIB(lpbi))
#endif
        return (WORD)(DIBNumColors(lpbi) * sizeof(RGBQUAD));
#ifndef WIN30_ONLY
    else
        return (DIBNumColors(lpbi) * sizeof(RGBTRIPLE));
#endif
}

//---------------------------------------------------------------------
//
// Function:   FindDIBBits
//
// Purpose:    Given a pointer to a DIB, returns a pointer to the
//             DIB's bitmap bits.
//
// Parms:      lpbi == pointer to DIB header (either BITMAPINFOHEADER
//                       or BITMAPCOREHEADER)
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

LPSTR FindDIBBits(LPSTR lpbi)
{
    return (lpbi + (int)*(LPDWORD)lpbi + PaletteSize(lpbi));
}

DWORD DIBHeight(LPSTR lpDIB)
{
    LPBITMAPINFOHEADER lpbmi;
    LPBITMAPCOREHEADER lpbmc;

    lpbmi = (LPBITMAPINFOHEADER)lpDIB;
    lpbmc = (LPBITMAPCOREHEADER)lpDIB;

    if (lpbmi->biSize == sizeof(BITMAPINFOHEADER))
        return lpbmi->biHeight;
    else
        return (DWORD)lpbmc->bcHeight;
}

DWORD DIBWidth(LPSTR lpDIB)
{
    LPBITMAPINFOHEADER lpbmi;
    LPBITMAPCOREHEADER lpbmc;

    lpbmi = (LPBITMAPINFOHEADER)lpDIB;
    lpbmc = (LPBITMAPCOREHEADER)lpDIB;

    if (lpbmi->biSize == sizeof(BITMAPINFOHEADER))
        return lpbmi->biWidth;
    else
        return (DWORD)lpbmc->bcWidth;
}

//---------------------------------------------------------------------
//
// Function:   DIBToBitmap
//
// Purpose:    Given a handle to global memory with a DIB spec in it,
//             and a palette, returns a device dependent bitmap.  The
//             The DDB will be rendered with the specified palette.
//
// Parms:      hDIB == HANDLE to global memory containing a DIB spec
//                     (either BITMAPINFOHEADER or BITMAPCOREHEADER)
//             hPal == Palette to render the DDB with.  If it's NULL,
//                     use the default palette.
//
// History:   Date      Reason
//             6/01/91  Created
//
//---------------------------------------------------------------------

HBITMAP DIBToBitmap(HANDLE hDIB, HPALETTE hPal)
{
    LPSTR lpDIBHdr, lpDIBBits;
    HBITMAP hBitmap;
    HDC hDC;
    HPALETTE hOldPal = NULL;

    if (!hDIB)
        return NULL;

    lpDIBHdr = (LPSTR)LockResource(hDIB);
    if (lpDIBHdr == NULL)
    {
        TRACE_E("Unable to lock DIB resource.");
        return NULL;
    }
    lpDIBBits = FindDIBBits(lpDIBHdr);
    hDC = HANDLES(GetDC(NULL));

    if (!hDC)
    {
        TRACE_E("Unable to allocate DC.");
        return NULL;
    }
    if (hPal)
        hOldPal = SelectPalette(hDC, hPal, FALSE);

    RealizePalette(hDC);

    hBitmap = HANDLES(CreateDIBitmap(hDC,
                                     (LPBITMAPINFOHEADER)lpDIBHdr,
                                     CBM_INIT,
                                     lpDIBBits,
                                     (LPBITMAPINFO)lpDIBHdr,
                                     DIB_RGB_COLORS));
    if (hBitmap == NULL)
    {
        TRACE_E("Unable to create bitmap.");
        return NULL;
    }
    //   if (!hBitmap)
    //      DIBError (ERR_CREATEDDB);

    if (hOldPal)
        SelectPalette(hDC, hOldPal, FALSE);

    HANDLES(ReleaseDC(NULL, hDC));

    return hBitmap;
}

//---------------------------------------------------------------------
//
// Function:   MapColor(RGB fromColor, RGB toColor)
//
//             vsechny barvy fromColor premapuje na barvu toColor
//
//---------------------------------------------------------------------

int MapColor(HANDLE hDIB, COLORREF fromColor, COLORREF toColor)
{
    BITMAPINFO* dibInfo;
    int i;

    dibInfo = (BITMAPINFO*)LockResource(hDIB);
    if (dibInfo == NULL)
    {
        TRACE_E("Unable to lock DIB resource.");
        return INT_MAX;
    }
    for (i = 0; i < DIBNumColors((LPSTR)dibInfo); i++)
        if (RGB(dibInfo->bmiColors[i].rgbRed,
                dibInfo->bmiColors[i].rgbGreen,
                dibInfo->bmiColors[i].rgbBlue) == fromColor)
        {
            dibInfo->bmiColors[i].rgbRed = (BYTE)(toColor & 0xFFUL);
            dibInfo->bmiColors[i].rgbGreen = (BYTE)((toColor & 0xFF00UL) >> 8);
            dibInfo->bmiColors[i].rgbBlue = (BYTE)((toColor & 0xFF0000UL) >> 16);
            return i;
        }
    return INT_MAX;
}

//---------------------------------------------------------------------
//
// Function:   CreateMask(HANDLE hDIB, COLORREF bkgndColor)
//
//---------------------------------------------------------------------

void CreateMask(HANDLE hDIB, COLORREF bkgndColor)
{
    BITMAPINFO* dibInfo;
    int i;
    COLORREF black = RGB(0, 0, 0);
    COLORREF white = RGB(255, 255, 255);

    dibInfo = (BITMAPINFO*)hDIB;
    dibInfo = (BITMAPINFO*)LockResource(hDIB);
    if (dibInfo == NULL)
    {
        TRACE_E("Unable to lock DIB resource.");
        return;
    }
    for (i = 0; i < DIBNumColors((LPSTR)dibInfo); i++)
        if (RGB(dibInfo->bmiColors[i].rgbRed,
                dibInfo->bmiColors[i].rgbGreen,
                dibInfo->bmiColors[i].rgbBlue) == bkgndColor)
        {
            dibInfo->bmiColors[i].rgbRed = (BYTE)(white & 0xFFUL);
            dibInfo->bmiColors[i].rgbGreen = (BYTE)((white & 0xFF00UL) >> 8);
            dibInfo->bmiColors[i].rgbBlue = (BYTE)((white & 0xFF0000UL) >> 16);
        }
        else
        {
            dibInfo->bmiColors[i].rgbRed = (BYTE)(black & 0xFFUL);
            dibInfo->bmiColors[i].rgbGreen = (BYTE)((black & 0xFF00UL) >> 8);
            dibInfo->bmiColors[i].rgbBlue = (BYTE)((black & 0xFF0000UL) >> 16);
        }
}
