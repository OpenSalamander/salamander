// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dep\pnglite\\pnglite.h"

//******************************************************************************
//
// CIconList
//

// specifies the number of items in one row; following the example of W2K, we set it to 4;
// probably for faster execution of operations?
const int IL_ITEMS_IN_ROW = 4;

#define IL_TYPE_NORMAL 0 // normal icon that can be drawn using BitBlt
#define IL_TYPE_XOR 1    // Icon containing areas that need to be XORed
#define IL_TYPE_ALPHA 2  // Icon containing an alpha channel

HDC CIconList::HMemDC = NULL;
HBITMAP CIconList::HOldBitmap = NULL;
HBITMAP CIconList::HTmpImage = NULL;
DWORD* CIconList::TmpImageRaw = NULL;
int CIconList::TmpImageWidth = 0;
int CIconList::TmpImageHeight = 0;
int CIconList::MemDCLocks = 0;
CRITICAL_SECTION CIconList::CriticalSection;
int CIconList::CriticalSectionLocks = 0;

CIconList::CIconList()
{
    if (CriticalSectionLocks == 0)
        HANDLES(InitializeCriticalSection(&CriticalSection));
    CriticalSectionLocks++;

    ImageWidth = 0;
    ImageHeight = 0;
    ImageCount = 0;
    BitmapWidth = 0;
    BitmapHeight = 0;

    HImage = NULL;
    ImageRaw = NULL;
    ImageFlags = NULL;

    BkColor = RGB(255, 255, 255);

    //  Dump = FALSE;
}

CIconList::~CIconList()
{
    HANDLES(EnterCriticalSection(&CriticalSection));
    MemDCLocks--;

    HBITMAP hCurrentBmp = GetCurrentBitmap();
    if (hCurrentBmp != NULL && (hCurrentBmp == HImage || hCurrentBmp == HTmpImage))
        SelectObject(HMemDC, HOldBitmap);

    if (MemDCLocks == 0)
    {
        if (HOldBitmap != NULL)
            SelectObject(HMemDC, HOldBitmap);
        HANDLES(DeleteDC(HMemDC));
        HMemDC = NULL;
        HOldBitmap = NULL;
        if (HTmpImage != NULL)
        {
            HANDLES(DeleteObject(HTmpImage));
            HTmpImage = NULL;
            TmpImageRaw = NULL;
        }
    }
    if (HImage != NULL)
    {
        ImageRaw = NULL;
        HANDLES(DeleteObject(HImage));
        HImage = NULL;
    }
    if (ImageFlags != NULL)
    {
        free(ImageFlags);
        ImageFlags = NULL;
    }
    HANDLES(LeaveCriticalSection(&CriticalSection));
    CriticalSectionLocks--;
    if (CriticalSectionLocks == 0)
        HANDLES(DeleteCriticalSection(&CriticalSection));
}

HBITMAP
CIconList::GetCurrentBitmap()
{
    if (HMemDC == NULL)
        return NULL;
    return (HBITMAP)GetCurrentObject(HMemDC, OBJ_BITMAP);
}

BOOL CIconList::CreateOrEnlargeTmpImage(int width, int height)
{
    if (HTmpImage == NULL || TmpImageWidth < width || TmpImageHeight < height)
    {
        // !!! Attention, when using BITMAPV4HEADER, Salamander was crashing in ICM32.DLL at Peter's,
        // old BITMAPINFOHEADER obviously suffices
        BITMAPINFOHEADER bmhdr;
        memset(&bmhdr, 0, sizeof(bmhdr));
        bmhdr.biSize = sizeof(bmhdr);
        bmhdr.biWidth = width;
        bmhdr.biHeight = -height; // top-down
        bmhdr.biPlanes = 1;
        bmhdr.biBitCount = 32;
        bmhdr.biCompression = BI_RGB;
        void* lpBits = NULL;
        HBITMAP hBmp = HANDLES(CreateDIBSection(NULL, (CONST BITMAPINFO*)&bmhdr,
                                                DIB_RGB_COLORS, &lpBits, NULL, 0));
        if (hBmp == NULL)
        {
            TRACE_E("CreateDIBSection() failed!");
            return FALSE;
        }

        if (HTmpImage != NULL)
            HANDLES(DeleteObject(HTmpImage)); // if there is an old bitmap, we will delete it

        HTmpImage = hBmp;
        TmpImageRaw = (DWORD*)lpBits;
        TmpImageWidth = width;
        TmpImageHeight = height;
    }
    return TRUE;
}

BOOL CIconList::Create(int imageWidth, int imageHeight, int imageCount)
{
    if (HImage != NULL)
    {
        TRACE_E("CIconList::Create: Image already exists!");
        return FALSE;
    }
    if (imageWidth < 1 || imageHeight < 1 || imageCount < 1)
    {
        TRACE_E("CIconList::Create: Incorrect parameters!");
        return FALSE;
    }

    HANDLES(EnterCriticalSection(&CriticalSection));

    ImageFlags = (BYTE*)malloc(sizeof(BYTE) * imageCount);
    if (ImageFlags == NULL)
    {
        TRACE_E(LOW_MEMORY);
        HANDLES(LeaveCriticalSection(&CriticalSection));
        return FALSE;
    }
    ZeroMemory(ImageFlags, sizeof(BYTE) * imageCount); // without alpha channel

    int bmpWidth = imageWidth * min(imageCount, IL_ITEMS_IN_ROW);
    int bmpHeight = imageHeight * ((imageCount + IL_ITEMS_IN_ROW - 1) / IL_ITEMS_IN_ROW);

    // auxiliary DC to create a compatible bitmap
    HDC hDC = HANDLES(GetDC(NULL));
    if (hDC == NULL)
    {
        TRACE_E("Error creating DC!");
        free(ImageFlags);
        ImageFlags = NULL;
        HANDLES(LeaveCriticalSection(&CriticalSection));
        return FALSE;
    }

    if (HMemDC == NULL)
    {
        HMemDC = HANDLES(CreateCompatibleDC(hDC));
        if (HMemDC == NULL)
        {
            TRACE_E("Error creating memory DC!");
            HANDLES(ReleaseDC(NULL, hDC));
            free(ImageFlags);
            ImageFlags = NULL;
            HANDLES(LeaveCriticalSection(&CriticalSection));
            return FALSE;
        }
        HOldBitmap = (HBITMAP)GetCurrentObject(HMemDC, OBJ_BITMAP);
    }
    MemDCLocks++;

    // if the cache/mask bitmap does not exist yet or is small, we will create it
    if (!CreateOrEnlargeTmpImage(imageWidth, imageHeight))
    {
        HANDLES(ReleaseDC(NULL, hDC));
        free(ImageFlags);
        ImageFlags = NULL;
        return FALSE;
    }

    // !!! Attention, when using BITMAPV4HEADER, Salamander was crashing in ICM32.DLL at Peter's,
    // old BITMAPINFOHEADER obviously suffices
    BITMAPINFOHEADER bmhdr;
    memset(&bmhdr, 0, sizeof(bmhdr));
    bmhdr.biSize = sizeof(bmhdr);
    bmhdr.biWidth = bmpWidth;
    bmhdr.biHeight = -bmpHeight; // top-down
    bmhdr.biPlanes = 1;
    bmhdr.biBitCount = 32;
    bmhdr.biCompression = BI_RGB;

    void* lpBits = NULL;
    HImage = HANDLES(CreateDIBSection(hDC, (CONST BITMAPINFO*)&bmhdr, DIB_RGB_COLORS, &lpBits, NULL, 0));
    ImageRaw = (DWORD*)lpBits;

    HANDLES(ReleaseDC(NULL, hDC));

    if (HImage == NULL)
    {
        TRACE_E("Error creating image bitmap!");
        free(ImageFlags);
        ImageFlags = NULL;
        HANDLES(LeaveCriticalSection(&CriticalSection));
        return FALSE;
    }

    ImageWidth = imageWidth;
    ImageHeight = imageHeight;
    ImageCount = imageCount;
    BitmapWidth = bmpWidth;
    BitmapHeight = bmpHeight;

    HANDLES(LeaveCriticalSection(&CriticalSection));

    return TRUE;
}

BOOL CIconList::CreateFromImageList(HIMAGELIST hIL, int requiredImageSize)
{
    if (hIL == NULL)
    {
        TRACE_E("CIconList::CreateFromImageList() hIL==NULL");
        return FALSE;
    }

    // extract the geometry of the image list
    int count = ImageList_GetImageCount(hIL);
    if (count == 0)
    {
        TRACE_E("CIconList::CreateFromImageList() hIL is empty");
        return FALSE;
    }

    int cx, cy;
    if (requiredImageSize == -1)
    {
        if (!ImageList_GetIconSize(hIL, &cx, &cy))
        {
            TRACE_E("ImageList_GetIconSize() failed");
            return FALSE;
        }
    }
    else
    {
        cx = requiredImageSize;
        cy = requiredImageSize;
    }

    if (!Create(cx, cy, count))
        return FALSE;

    // Here it could be optimized: instead of passing by icons, pass through ImageList_Draw
    if (count > 20)
    {
        TRACE_E("CIconList::CreateFromImageList(): Let me know that you need so many icons, it would deserve some optimizations here. Jan.");
    }

    int i;
    for (i = 0; i < count; i++)
    {
        HICON hIcon = ImageList_GetIcon(hIL, i, ILD_NORMAL);
        if (hIcon == NULL)
        {
            TRACE_E("ImageList_GetIcon() failed");
            return FALSE;
        }
        if (!ReplaceIcon(i, hIcon))
            return FALSE;
        DestroyIcon(hIcon);
    }

    return TRUE;
}

// If I have a 32bpp desktop under W2K, I get XP icons with alpha channel uncut,
// including the alpha channel. If I switch the desktop to 16bpp, the alpha channel is clipped (zeroed).
// So if ApplyMaskToImage receives an icon including an alpha channel, it will return the type
// IL_TYPE_ALPHA will correctly display these icons even under OS < XP

BYTE CIconList::ApplyMaskToImage(int index, BYTE forceXOR)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // We will examine the entire icon and determine what type it is (NORMAL, XOR, ALPHA)
    BYTE type = IL_TYPE_NORMAL;
    if (forceXOR)
        type = IL_TYPE_XOR;
    else
    {
        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* ptr = ImageRaw + iX + (iY + row) * BitmapWidth;
            DWORD* maskPtr = TmpImageRaw + row * TmpImageWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                if (((*ptr) & 0xFF000000) != 0)
                {
                    type = IL_TYPE_ALPHA;
                    goto SKIP;
                }
                if ((*maskPtr != 0) && (*ptr != 0))
                {
                    type = IL_TYPE_XOR;
                    // just because it's an icon candidate for XOR doesn't mean that,
                    // that there won't be ALPHA, so we can't drop out
                }
                ptr++;
                maskPtr++;
            }
        }
    }
    //  if (isXOR)
    //    DumpToTrace(index);

SKIP:
    if (type == IL_TYPE_NORMAL)
    {
        // prepare the background color in the correct format
        DWORD bkClr = GetRValue(BkColor) << 16 | GetGValue(BkColor) << 8 | GetBValue(BkColor);
        // Transfer the transparent areas of the mask to the alpha channel
        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* ptr = ImageRaw + iX + (iY + row) * BitmapWidth;
            DWORD* maskPtr = TmpImageRaw + row * TmpImageWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                if (*maskPtr != 0)
                {
                    // completely transparent area
                    // Alpha channel is in the highest byte, we will set it to 00, the rest will be the background color
                    *ptr = bkClr;
                }
                else
                {
                    *ptr |= 0xFF000000; // Set the region as opaque
                }
                ptr++;
                maskPtr++;
            }
        }
    }
    if (type == IL_TYPE_XOR)
    {
        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* ptr = ImageRaw + iX + (iY + row) * BitmapWidth;
            DWORD* maskPtr = TmpImageRaw + row * TmpImageWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                if (*maskPtr == 0)
                    *ptr |= 0xFF000000; // opaque area

                ptr++;
                maskPtr++;
            }
        }
    }
    HANDLES(LeaveCriticalSection(&CriticalSection));

    return type;
}

/*  void
CIconList::DumpToTrace(int index, BOOL dumpMask)
{
  HANDLES(EnterCriticalSection(&CriticalSection));

  // souradnice v bodech v HImage
  int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
  int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

  TRACE_E("COLOR BITMAP");
  int row;
  for (row = 0; row < ImageHeight; row++)
  {
    char buff[1000];
    buff[0] = 0;
    DWORD *ptr = ImageRaw + iX + (iY + row) * BitmapWidth;
    int col;
    for (col = 0; col < ImageWidth; col++)
    {
      sprintf(buff + strlen(buff), "%.8X ", *ptr);
      ptr++;
    }
    TRACE_I(buff);
  }
  if (dumpMask)
  {
    TRACE_E("MASK BITMAP");
    for (row = 0; row < ImageHeight; row++)
    {
      char buff[1000];
      buff[0] = 0;
      DWORD *maskPtr = TmpImageRaw + row * TmpImageWidth;
      int col;
      for (col = 0; col < ImageWidth; col++)
      {
        sprintf(buff + strlen(buff), "%.8X ", *maskPtr);
        maskPtr++;
      }
      TRACE_I(buff);
    }
  }
  char buff[100];
  sprintf(buff, "%d", ImageFlags[index]);
  TRACE_I("ImageFlags["<<index<<"]="<<buff);
  HANDLES(LeaveCriticalSection(&CriticalSection));
}
*/
//int counter = 0;

void CIconList::StoreMonoIcon(int index, WORD* mask)
{
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);
}

BOOL CIconList::ReplaceIcon(int index, HICON hIcon)
{
    CALL_STACK_MESSAGE2("CIconList::ReplaceIcon(%d, )", index);

    HICON hIconOrig = hIcon;
    BOOL ret = TRUE;
    ICONINFO ii;
    ZeroMemory(&ii, sizeof(ii));
    HDC hSrcDC;
    HBITMAP hOldSrcBmp;

    HANDLES(EnterCriticalSection(&CriticalSection));

    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // parameter check
    if (index < 0 || index >= ImageCount)
    {
        TRACE_E("CIconList::ReplaceIcon(): Index is out of range! index=" << index);
        goto BAIL;
    }

    // if necessary, we will resize the icon
    // Honza: under W10, the CopyImage call started crashing in the debug x64 version if the LR_COPYFROMRESOURCE flag was enabled
    // FIXME: Perform an audit to see if this downscale is still needed when SalLoadIcon() now calls LoadIconWithScaleDown()
    hIcon = (HICON)CopyImage(hIconOrig, IMAGE_ICON, ImageWidth, ImageHeight, /*LR_COPYFROMRESOURCE |*/ LR_COPYRETURNORG);

    // we will extract the MASK and COLOR bitmaps of the icon from the handle
    if (!GetIconInfo(hIcon, &ii))
    {
        TRACE_E("GetIconInfo() failed!");
        DWORD err = GetLastError();
        goto BAIL;
    }

    BITMAP bm;
    if (!GetObject(ii.hbmMask, sizeof(bm), &bm))
    {
        TRACE_E("GetObject() failed!");
        goto BAIL;
    }

    // if it is a b&w icon, it should have an even height
    if (ii.hbmColor == NULL && (bm.bmHeight & 1) == 1)
    {
        TRACE_E("CIconList::ReplaceIcon() Icon has wrong MASK height");
        goto BAIL;
    }
    // The icon should have the same dimensions as our item
    if (bm.bmWidth != ImageWidth ||
        (ii.hbmColor == NULL && bm.bmHeight != ImageHeight * 2) ||
        (ii.hbmColor != NULL && bm.bmHeight != ImageHeight))
        TRACE_E("CIconList::ReplaceIcon() Icon has wrong size: bmWidth=" << bm.bmWidth << " bmHeight=" << bm.bmHeight);

    // we need enough space for the mask
    if (!CreateOrEnlargeTmpImage(bm.bmWidth, bm.bmHeight))
        goto BAIL;

    hSrcDC = HANDLES(CreateCompatibleDC(NULL)); // Helper DC for BitBlt
    hOldSrcBmp = (HBITMAP)SelectObject(hSrcDC, ii.hbmMask);

    // ii.hbmMask -> HTmpImage
    SelectObject(HMemDC, HTmpImage);
    BitBlt(HMemDC, 0, 0, ImageWidth, ImageHeight, hSrcDC, 0, 0, SRCCOPY);

    // ii.hbmColor -> HImage (if hbmColor==NULL, the XOR part lies in the bottom half of hbmMask
    if (ii.hbmColor != NULL)
        SelectObject(hSrcDC, ii.hbmColor);
    SelectObject(HMemDC, HImage);
    BitBlt(HMemDC, iX, iY, ImageWidth, ImageHeight, hSrcDC, 0, (ii.hbmColor != NULL) ? 0 : ImageHeight, SRCCOPY);

    GdiFlush(); // According to MSDN, we need to call before we start accessing raw data

    //  TRACE_I("counter: "<<counter);
    //  if (++counter == 19)
    //    DumpToTrace(index);

    ImageFlags[index] = ApplyMaskToImage(index, ii.hbmColor == NULL);

    SelectObject(hSrcDC, hOldSrcBmp);
    HANDLES(DeleteDC(hSrcDC));

BAIL:
    HANDLES(LeaveCriticalSection(&CriticalSection));

    if (ii.hbmMask != NULL)
        DeleteObject(ii.hbmMask);

    if (ii.hbmColor != NULL)
        DeleteObject(ii.hbmColor);

    // If we have changed the size, we need to destroy the temporary icon
    if (hIcon != hIconOrig)
        DestroyIcon(hIcon);

    //  if (Dump)
    //  {
    //    TRACE_E("ReplaceIcon()");
    //    DumpToTrace(index, FALSE);
    //  }

    return ret;
}

#define ROP_DSna 0x00220326

HICON
CIconList::GetIcon(int index, BOOL useHandles)
{
    HICON icon = GetIcon(index);
    if (useHandles)
        HANDLES_ADD_EX(__otMessages, icon != NULL, __htIcon, __hoCreateIconIndirect, icon, GetLastError(), TRUE);
    return icon;
}

HICON
CIconList::GetIcon(int index)
{
    // parameter check
    if (index < 0 || index >= ImageCount)
    {
        TRACE_E("CIconList::GetIcon: index is out of range!");
        return NULL;
    }

    // create a B&W mask + color bitmap according to the screen
    HDC hDC = HANDLES(GetDC(NULL));
    HBITMAP hMask = HANDLES(CreateBitmap(ImageWidth, ImageHeight, 1, 1, NULL));
    HBITMAP hColor = HANDLES(CreateCompatibleBitmap(hDC, ImageWidth, ImageHeight));
    HANDLES(ReleaseDC(NULL, hDC));

    HANDLES(EnterCriticalSection(&CriticalSection));

    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    BYTE type = ImageFlags[index];

    // prepare COLOR cast icon for HTmpImage
    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

            if (type == IL_TYPE_ALPHA) // except for XP and newer systems, we return icons with alpha
            {
                *tmpPtr = argb;
            }
            else
            {
                // in the transparent area is the background color of the image list, but we are there
                // we need to add black color to make the XOR drawing mechanism work
                if (alpha != 0)
                    *tmpPtr = argb;
                else
                    *tmpPtr = 0x00000000;
            }

            imagePtr++;
            tmpPtr++;
        }
    }

    // Transfer HTmpImage to hColor
    SelectObject(HMemDC, HTmpImage);
    hDC = HANDLES(CreateCompatibleDC(NULL));
    HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hColor);
    BitBlt(hDC, 0, 0, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    SelectObject(hDC, hMask);

    // prepare the MASK part of the icon in the HTmpImage
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

            *tmpPtr = (alpha != 0) ? RGB(0, 0, 0) : RGB(255, 255, 255);

            imagePtr++;
            tmpPtr++;
        }
    }

    // Transfer HTmpImage to hMask
    BitBlt(hDC, 0, 0, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    //  if (Dump)
    //  {
    //    TRACE_E("GetIcon()");
    //    DumpToTrace(index, TRUE);
    //  }

    // we will sweep
    SelectObject(hDC, hOldBmp);
    HANDLES(DeleteDC(hDC));

    HANDLES(LeaveCriticalSection(&CriticalSection));

    // we will create an icon from hColor + hMask
    ICONINFO ii;
    ii.fIcon = TRUE;
    ii.xHotspot = 0;
    ii.yHotspot = 0;
    ii.hbmColor = hColor;
    ii.hbmMask = hMask;
    HICON hIcon;
    hIcon = CreateIconIndirect(&ii); // must not be in handles, it exports the icon out of Salamander

    HANDLES(DeleteObject(hColor));
    HANDLES(DeleteObject(hMask));

    return hIcon;
}

BOOL CIconList::Draw(int index, HDC hDC, int x, int y, COLORREF blendClr, DWORD flags)
{
    // parameter check
    if (index < 0 || index >= ImageCount)
    {
        TRACE_E("CIconList::Draw() Index is out of range! index=" << index << " ImageCount=" << ImageCount);
        return FALSE;
    }

    if (flags & IL_DRAW_MASK) // The mask is used during drag & drop, for example in shared addresses, see StateImageList_Draw()
        return DrawMask(hDC, x, y, index, RGB(0, 0, 0), RGB(255, 255, 255));
    if (flags & IL_DRAW_BLEND)
        return AlphaBlend(hDC, x, y, index, BkColor, blendClr);
    if (flags & IL_DRAW_ASALPHA)
        return DrawAsAlphaLeaveBackground(hDC, x, y, index, blendClr);
    if (ImageFlags[index] == IL_TYPE_ALPHA)
    {
        if (flags & IL_DRAW_TRANSPARENT)
            return DrawALPHALeaveBackground(hDC, x, y, index);
        else
            return DrawALPHA(hDC, x, y, index, BkColor);
    }

    if (ImageFlags[index] == IL_TYPE_XOR)
        return DrawXOR(hDC, x, y, index, BkColor);

    HANDLES(EnterCriticalSection(&CriticalSection));

    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    SelectObject(HMemDC, HImage);

    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, iX, iY, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));
    return TRUE;
}

BOOL CIconList::DrawMask(HDC hDC, int x, int y, int index, COLORREF fgColor, COLORREF bkColor)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    //DumpToTrace(index);

    // we will extract data from the screen to HTmpImage
    // our DrawMask only sets black points at the mask location so we can easily merge with the overlay
    // when calling from StateImageList_Draw(); if it will be necessary to display another overlay in the future, it will be needed
    // perform a merger in StateImageList_Draw() based on boolean bitblt operations and this method would then
    // could also set fgColor; the condition *** below would be omitted
    SelectObject(HMemDC, HTmpImage);
    BitBlt(HMemDC, 0, 0, ImageWidth, ImageHeight, hDC, x, y, SRCCOPY);

    // we will draw into HTmpImage
    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

            DWORD clr = (alpha == 0) ? bkColor : fgColor;
            if (alpha != 0) // *** see comment above
                *tmpPtr = clr;

            imagePtr++;
            tmpPtr++;
        }
    }

    // draw HTmpImage to the screen
    //  SelectObject(HMemDC, HTmpImage);
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));
    return TRUE;
}

BOOL CIconList::DrawXOR(HDC hDC, int x, int y, int index, COLORREF bkColor)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // we will draw into HTmpImage
    BYTE bkR = GetRValue(bkColor);
    BYTE bkG = GetGValue(bkColor);
    BYTE bkB = GetBValue(bkColor);
    DWORD bkClr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);

    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

            DWORD clr = (alpha == 0) ? bkClr : 0;
            clr ^= (argb & 0x00FFFFFF);
            *tmpPtr = clr;

            imagePtr++;
            tmpPtr++;
        }
    }

    // draw HTmpImage to the screen
    SelectObject(HMemDC, HTmpImage);
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));
    return TRUE;
}

BOOL CIconList::DrawALPHA(HDC hDC, int x, int y, int index, COLORREF bkColor)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // in the first phase we will transfer the data to HMask
    SelectObject(HMemDC, HTmpImage);

    BYTE bkR = GetRValue(bkColor);
    BYTE bkG = GetGValue(bkColor);
    BYTE bkB = GetBValue(bkColor);
    DWORD bkClr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);

    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);
            if (alpha == 0)
                *tmpPtr = bkClr;
            else if (alpha == 255)
                *tmpPtr = argb;
            else
            {
                BYTE r = (BYTE)((argb & 0x00FF0000) >> 16);
                BYTE g = (BYTE)((argb & 0x0000FF00) >> 8);
                BYTE b = (BYTE)((argb & 0x000000FF));

                BYTE newR = (BYTE)((DWORD)r * alpha / 255 + (DWORD)bkR * (255 - alpha) / 255);
                BYTE newG = (BYTE)((DWORD)g * alpha / 255 + (DWORD)bkG * (255 - alpha) / 255);
                BYTE newB = (BYTE)((DWORD)b * alpha / 255 + (DWORD)bkB * (255 - alpha) / 255);
                *tmpPtr = (DWORD)newR << 16 | (DWORD)newG << 8 | (DWORD)newB;
            }

            imagePtr++;
            tmpPtr++;
        }
    }

    // draw HTmpImage to the screen
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));

    return TRUE;
}

BOOL CIconList::DrawALPHALeaveBackground(HDC hDC, int x, int y, int index)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // In the first phase, we will transfer data from the target DC to HMask
    SelectObject(HMemDC, HTmpImage);
    BitBlt(HMemDC, 0, 0, ImageWidth, ImageHeight, hDC, x, y, SRCCOPY);
    GdiFlush(); // According to MSDN, we need to call before we start accessing raw data

    /*    BYTE bkR = GetRValue(bkColor);
  BYTE bkG = GetGValue(bkColor);
  BYTE bkB = GetBValue(bkColor);
  DWORD bkClr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);*/
    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);
            if (alpha != 0)
            {
                if (alpha == 255)
                    *tmpPtr = argb;
                else
                {
                    BYTE bkR = GetRValue(*tmpPtr);
                    BYTE bkG = GetGValue(*tmpPtr);
                    BYTE bkB = GetBValue(*tmpPtr);

                    BYTE r = (BYTE)((argb & 0x00FF0000) >> 16);
                    BYTE g = (BYTE)((argb & 0x0000FF00) >> 8);
                    BYTE b = (BYTE)((argb & 0x000000FF));
                    BYTE newR = (BYTE)((DWORD)r * alpha / 255 + (DWORD)bkR * (255 - alpha) / 255);
                    BYTE newG = (BYTE)((DWORD)g * alpha / 255 + (DWORD)bkG * (255 - alpha) / 255);
                    BYTE newB = (BYTE)((DWORD)b * alpha / 255 + (DWORD)bkB * (255 - alpha) / 255);
                    *tmpPtr = (DWORD)newR << 16 | (DWORD)newG << 8 | (DWORD)newB;
                }
            }
            imagePtr++;
            tmpPtr++;
        }
    }

    // draw HTmpImage to the screen
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));

    return TRUE;
}

BOOL CIconList::DrawAsAlphaLeaveBackground(HDC hDC, int x, int y, int index, COLORREF fgColor)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // In the first phase, we will transfer data from the target DC to HMask
    SelectObject(HMemDC, HTmpImage);
    BitBlt(HMemDC, 0, 0, ImageWidth, ImageHeight, hDC, x, y, SRCCOPY);
    GdiFlush(); // According to MSDN, we need to call before we start accessing raw data

    BYTE fgR = GetRValue(fgColor);
    BYTE fgG = GetGValue(fgColor);
    BYTE fgB = GetBValue(fgColor);

    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
        DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = 255 - (BYTE)(argb & 0x000000FF); // all channels carry the same value, which we should consider as the alpha channel
            if (alpha != 0)
            {
                BYTE bkR = (BYTE)((*tmpPtr & 0x00FF0000) >> 16);
                BYTE bkG = (BYTE)((*tmpPtr & 0x0000FF00) >> 8);
                BYTE bkB = (BYTE)((*tmpPtr & 0x000000FF));

                BYTE newR = (BYTE)((DWORD)fgR * alpha / 255 + (DWORD)bkR * (255 - alpha) / 255);
                BYTE newG = (BYTE)((DWORD)fgG * alpha / 255 + (DWORD)bkG * (255 - alpha) / 255);
                BYTE newB = (BYTE)((DWORD)fgB * alpha / 255 + (DWORD)bkB * (255 - alpha) / 255);
                *tmpPtr = (DWORD)newR << 16 | (DWORD)newG << 8 | (DWORD)newB;
            }
            imagePtr++;
            tmpPtr++;
        }
    }

    // draw HTmpImage to the screen
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));

    return TRUE;
}

BOOL CIconList::AlphaBlend(HDC hDC, int x, int y, int index, COLORREF bkColor, COLORREF fgColor)
{
    HANDLES(EnterCriticalSection(&CriticalSection));

    // Coordinates in points in HImage
    int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
    int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

    // It's a variant where XORing is needed?
    BOOL xorType = ImageFlags[index] == IL_TYPE_XOR;

    // first we prepare raw data for HTmpImage
    int bitsPerPixel = GetCurrentBPP(hDC);
    if (bitsPerPixel <= 8)
    {
        // 256 colors or less: instead of blending, we overlay a checkerboard
        DWORD bkClrOpaque = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);
        DWORD bkClr;
        if (fgColor != CLR_NONE)
            bkClr = GetRValue(fgColor) << 16 | GetGValue(fgColor) << 8 | GetBValue(fgColor);
        else
            bkClr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);
        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
            DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                DWORD argb = *imagePtr;
                BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

                if (xorType && alpha == 0)
                {
                    // XOR && transparent area
                    BYTE bkR = GetRValue(bkColor);
                    BYTE bkG = GetGValue(bkColor);
                    BYTE bkB = GetBValue(bkColor);
                    *tmpPtr = (DWORD)bkR << 16 | (DWORD)bkG << 8 | (DWORD)bkB;
                    *tmpPtr ^= (argb & 0x00FFFFFF);
                }
                else
                {
                    if (alpha == 0)
                    {
                        // transparent area
                        argb = bkClrOpaque;
                    }
                    else
                    {
                        if ((row & 0x00000001) == 1 && (col & 0x00000001) == 1)
                            argb = bkClr;
                        else if ((row & 0x00000001) == 0 && (col & 0x00000001) == 0)
                            argb = bkClr;
                        else
                            argb = *imagePtr;
                    }
                    *tmpPtr = argb;
                }

                imagePtr++;
                tmpPtr++;
            }
        }
    }
    else
    {
        // more than 256 colors: fading using alpha channel
        BYTE bkR = GetRValue(bkColor);
        BYTE bkG = GetGValue(bkColor);
        BYTE bkB = GetBValue(bkColor);
        DWORD bkClr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);

        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* imagePtr = ImageRaw + iX + (iY + row) * BitmapWidth;
            DWORD* tmpPtr = TmpImageRaw + row * TmpImageWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                DWORD argb = *imagePtr;
                BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

                if (xorType && alpha == 0)
                {
                    // XOR && transparent area
                    *tmpPtr = (DWORD)bkR << 16 | (DWORD)bkG << 8 | (DWORD)bkB;
                    *tmpPtr ^= (argb & 0x00FFFFFF);
                }
                else
                {
                    BYTE r = (BYTE)((argb & 0x00FF0000) >> 16);
                    BYTE g = (BYTE)((argb & 0x0000FF00) >> 8);
                    BYTE b = (BYTE)((argb & 0x000000FF));

                    BYTE alpha50;
                    if (fgColor != CLR_NONE)
                        alpha50 = alpha;
                    else
                        alpha50 = alpha / 2; // 50%

                    BYTE newR = (BYTE)((DWORD)r * alpha50 / 255 + (DWORD)bkR * (255 - alpha50) / 255);
                    BYTE newG = (BYTE)((DWORD)g * alpha50 / 255 + (DWORD)bkG * (255 - alpha50) / 255);
                    BYTE newB = (BYTE)((DWORD)b * alpha50 / 255 + (DWORD)bkB * (255 - alpha50) / 255);

                    if (fgColor != CLR_NONE && alpha > 127)
                    {
                        newR = newR / 2 + GetRValue(fgColor) / 2;
                        newG = newG / 2 + GetGValue(fgColor) / 2;
                        newB = newB / 2 + GetBValue(fgColor) / 2;
                    }
                    *tmpPtr = (DWORD)newR << 16 | (DWORD)newG << 8 | (DWORD)newB;
                }

                imagePtr++;
                tmpPtr++;
            }
        }
    }

    // draw HTmpImage to the screen
    SelectObject(HMemDC, HTmpImage);
    BitBlt(hDC, x, y, ImageWidth, ImageHeight, HMemDC, 0, 0, SRCCOPY);

    HANDLES(LeaveCriticalSection(&CriticalSection));
    return TRUE;
}

BOOL CIconList::SetBkColor(COLORREF bkColor)
{
    if (bkColor == BkColor)
        return TRUE;

    HANDLES(EnterCriticalSection(&CriticalSection));

    BkColor = bkColor;
    /*    if (BkColor == IL_CLR_NONE)
    return TRUE;*/

    DWORD clr = GetRValue(bkColor) << 16 | GetGValue(bkColor) << 8 | GetBValue(bkColor);

    int index;
    for (index = 0; index < ImageCount; index++)
    {
        if (ImageFlags[index] != IL_TYPE_NORMAL) // It only makes sense to set the background color for normal icons
            continue;

        int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
        int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            DWORD* ptr = ImageRaw + iX + (iY + row) * BitmapWidth;
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                if (((*ptr) & 0xff000000) == 0x00000000)
                    *ptr = clr;
                ptr++;
            }
        }
    }

    HANDLES(LeaveCriticalSection(&CriticalSection));

    return TRUE;
}

COLORREF
CIconList::GetBkColor()
{
    return BkColor;
}

BOOL CIconList::Copy(int dstIndex, CIconList* srcIL, int srcIndex)
{
    // parameter check
    if (dstIndex < 0 || dstIndex >= ImageCount)
    {
        TRACE_E("CIconList::Copy: dstIndex is out of range!");
        return FALSE;
    }
    if (srcIL == NULL)
    {
        TRACE_E("CIconList::Copy: srcIL is NULL!");
        return FALSE;
    }
    if (srcIndex < 0 || srcIndex >= srcIL->ImageCount)
    {
        TRACE_E("CIconList::Copy: srcIndex is out of range!");
        return FALSE;
    }
    if (ImageWidth != srcIL->ImageWidth || ImageHeight != srcIL->ImageHeight)
    {
        TRACE_E("CIconList::Copy: different ImageWidth || ImageHeight!");
        return FALSE;
    }

    // Version of copying using direct access to data, the advantage should be
    // higher speed and completely identical copy (the BitBlt function could be discarding
    // alpha channel)
    HANDLES(EnterCriticalSection(&CriticalSection));
    int srcX = ImageWidth * (srcIndex % IL_ITEMS_IN_ROW);
    int srcY = ImageHeight * (srcIndex / IL_ITEMS_IN_ROW);
    int dstX = ImageWidth * (dstIndex % IL_ITEMS_IN_ROW);
    int dstY = ImageHeight * (dstIndex / IL_ITEMS_IN_ROW);

    int row;
    for (row = 0; row < ImageHeight; row++)
    {
        DWORD* srcPtr = srcIL->ImageRaw + srcX + (srcY + row) * BitmapWidth;
        DWORD* ptr = ImageRaw + dstX + (dstY + row) * BitmapWidth;
        int col;
        for (col = 0; col < ImageWidth; col++)
        {
            *ptr = *srcPtr;
            ptr++;
            srcPtr++;
        }
    }
    ImageFlags[dstIndex] = srcIL->ImageFlags[srcIndex];
    HANDLES(LeaveCriticalSection(&CriticalSection));

    // version of copying using BitBlt
    //  HDC hSrcMemDC = HANDLES(CreateCompatibleDC(NULL));
    //  if (hSrcMemDC == NULL)
    //  {
    //    TRACE_E("Error creating DC!");
    //    return FALSE;
    //  }
    //  HANDLES(EnterCriticalSection(&CriticalSection));
    //  SelectObject(HMemDC, HImage);
    //
    //  // Image
    //  HBITMAP hOldSrcBitmap = (HBITMAP)SelectObject(hSrcMemDC, srcIL->HImage);
    //  BitBlt(HMemDC, dstX, dstY, ImageWidth, ImageHeight, hSrcMemDC, srcX, srcY, SRCCOPY);
    //  ImageFlags[dstIndex] = srcIL->ImageFlags[srcIndex];
    //  HANDLES(LeaveCriticalSection(&CriticalSection));
    //
    //  SelectObject(hSrcMemDC, hOldSrcBitmap);
    //  HANDLES(DeleteDC(hSrcMemDC));

    return TRUE;
}

BOOL CIconList::CreateFromBitmap(HBITMAP hBitmap, int imageCount, COLORREF transparentClr)
{

    BITMAP bmp;
    GetObject(hBitmap, sizeof(bmp), &bmp);

    int cx = bmp.bmWidth / imageCount;
    int cy = bmp.bmHeight;

    // allocate bitmap
    if (!Create(cx, cy, imageCount))
        return FALSE;

    // we will add stripes from the source bitmap to it line by line
    int index = 0;
    while (index < imageCount)
    {
        if (!CopyFromBitmapIternal(index, hBitmap, index, min(IL_ITEMS_IN_ROW, imageCount - index), transparentClr))
            return FALSE;
        index += IL_ITEMS_IN_ROW;
    }
    return TRUE;

    /*    BITMAP bmp;
  GetObject(hBitmap, sizeof(bmp), &bmp);

  BITMAPINFO *bi = (BITMAPINFO*)calloc(sizeof(BITMAPINFOHEADER) + 256 * sizeof(RGBQUAD), 1);
  bi->bmiHeader.biSize = sizeof(bi->bmiHeader);
  bi->bmiHeader.biWidth = bmp.bmWidth;
  bi->bmiHeader.biHeight = -bmp.bmHeight;  // top-down
  bi->bmiHeader.biPlanes = 1;
  bi->bmiHeader.biBitCount = 8;
  bi->bmiHeader.biCompression = BI_RGB;
  bi->bmiHeader.biClrUsed = 256;
  memcpy(bi->bmiColors, ColorTable, 256 * sizeof(RGBQUAD));

  void *lpBits = NULL;
  HBITMAP hDstBmp = HANDLES(CreateDIBSection(NULL, bi, DIB_RGB_COLORS, &lpBits, NULL, 0));
  free(bi);

  HDC hDstDC = HANDLES(CreateCompatibleDC(NULL));
  HBITMAP hOldDstBmp = (HBITMAP)SelectObject(hDstDC, hDstBmp);

  HDC hSrcDC = HANDLES(CreateCompatibleDC(NULL));
  HBITMAP hOldSrcBmp = (HBITMAP)SelectObject(hSrcDC, bitmap);

  BitBlt(hDstDC, 0, 0, bmp.bmWidth, bmp.bmHeight, hSrcDC, 0, 0, SRCCOPY);

  SelectObject(hSrcDC, hOldSrcBmp);
  HANDLES(DeleteDC(hSrcDC));
  SelectObject(hDstDC, hOldDstBmp);
  HANDLES(DeleteDC(hDstDC));

  if (p->PluginIcons != NULL)
    HANDLES(DeleteObject(p->PluginIcons));
  p->PluginIcons = hDstBmp;

  if (p->PluginIconsGray != NULL)
  {
    HANDLES(DeleteObject(p->PluginIconsGray));
    p->PluginIconsGray = NULL;
  }
  HBITMAP hGryscale;
  if (CreateGrayscaleDIB(hDstBmp, RGB(255, 0, 255), hGryscale))
    p->PluginIconsGray = hGryscale;
  */
}

BOOL CIconList::CopyFromBitmapIternal(int dstIndex, HBITMAP hSrcBitmap, int srcIndex, int imageCount, COLORREF transparentClr)
{
    HDC hSrcMemDC = HANDLES(CreateCompatibleDC(NULL));
    if (hSrcMemDC == NULL)
    {
        TRACE_E("Error creating DC!");
        return FALSE;
    }
    HANDLES(EnterCriticalSection(&CriticalSection));
    SelectObject(HMemDC, HImage);

    int srcX = ImageWidth * srcIndex;
    int srcY = 0;
    int dstX = ImageWidth * (dstIndex % IL_ITEMS_IN_ROW);
    int dstY = ImageHeight * (dstIndex / IL_ITEMS_IN_ROW);

    HBITMAP hOldSrcBitmap = (HBITMAP)SelectObject(hSrcMemDC, hSrcBitmap);
    BitBlt(HMemDC, dstX, dstY, ImageWidth * imageCount, ImageHeight, hSrcMemDC, srcX, srcY, SRCCOPY);
    ImageFlags[dstIndex] = IL_TYPE_NORMAL;

    GdiFlush(); // According to MSDN, we need to call before we start accessing raw data

    // Set the alpha channel according to the transparent color
    int row;
    for (row = dstY; row < dstY + ImageHeight; row++)
    {
        DWORD* ptr = ImageRaw + dstX + row * BitmapWidth;
        int col;
        for (col = dstX; col < dstX + imageCount * ImageWidth; col++)
        {
            DWORD argb = *ptr;
            if ((argb & 0x00FFFFFF) == (DWORD)transparentClr)
                argb &= 0x00FFFFFF;
            else
                argb |= 0xFF000000;
            *ptr = argb;
            ptr++;
        }
    }

    HANDLES(LeaveCriticalSection(&CriticalSection));
    SelectObject(hSrcMemDC, hOldSrcBitmap);
    HANDLES(DeleteDC(hSrcMemDC));

    return TRUE;
}

BOOL CIconList::CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale)
{
    return CreateAsCopy((const CIconList*)iconList, grayscale);
}

BOOL CIconList::CreateAsCopy(const CIconList* iconList, BOOL grayscale)
{
    if (!Create(iconList->ImageWidth, iconList->ImageHeight, iconList->ImageCount))
        return FALSE;

    if (BitmapWidth != iconList->BitmapWidth || BitmapHeight != iconList->BitmapHeight)
    {
        TRACE_E("Different bitmap dimensions! Don't know what to do...");
        return FALSE;
    }

    if (ImageCount != iconList->ImageCount)
    {
        TRACE_E("Different image count! Don't know what to do...");
        return FALSE;
    }

    if (grayscale)
    {
        int i;
        for (i = 0; i < BitmapWidth * BitmapHeight; i++)
        {
            DWORD argb = iconList->ImageRaw[i];
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);
            BYTE r = (BYTE)((argb & 0x00FF0000) >> 16);
            BYTE g = (BYTE)((argb & 0x0000FF00) >> 8);
            BYTE b = (BYTE)((argb & 0x000000FF));

            BYTE brightness = GetGrayscaleFromRGB(r, g, b);

            ImageRaw[i] = (DWORD)brightness | (DWORD)brightness << 8 | (DWORD)brightness << 16 | (DWORD)alpha << 24;
        }
    }
    else
    {
        int i;
        for (i = 0; i < BitmapWidth * BitmapHeight; i++)
            ImageRaw[i] = iconList->ImageRaw[i];
    }

    int i;
    for (i = 0; i < ImageCount; i++)
        ImageFlags[i] = iconList->ImageFlags[i];

    BkColor = iconList->BkColor;

    return TRUE;
}

BOOL CIconList::ConvertToGrayscale(BOOL forceAlphaForBW)
{
    if (ImageCount > 0)
    {
        int i;
        for (i = 0; i < BitmapWidth * BitmapHeight; i++)
        {
            DWORD argb = ImageRaw[i];
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);
            BYTE r = (BYTE)((argb & 0x00FF0000) >> 16);
            BYTE g = (BYTE)((argb & 0x0000FF00) >> 8);
            BYTE b = (BYTE)((argb & 0x000000FF));

            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            if (forceAlphaForBW)
            {
                // when outputting to a black and white bitmap, we set the threshold between white and black (fine-tuned on Vista icons in the user menu)
                if (alpha < 200)
                    alpha = 0;
                if (brightness > 240)
                    alpha = 0;
            }

            ImageRaw[i] = (DWORD)brightness | (DWORD)brightness << 8 | (DWORD)brightness << 16 | (DWORD)alpha << 24;
        }
    }
    return TRUE;
}

HIMAGELIST
CIconList::GetImageList()
{
    HIMAGELIST hIL = ImageList_Create(ImageWidth, ImageHeight, GetImageListColorFlags() | ILC_MASK, 0, 1);
    if (hIL != NULL)
    {
        int i;
        for (i = 0; i < ImageCount; i++)
        {
            HICON hIcon = GetIcon(i, TRUE);
            ImageList_AddIcon(hIL, hIcon);
            HANDLES(DestroyIcon(hIcon));
        }
    }
    return hIL;

    /*    // retrieve the dimensions of the bitmap
  HDC hDC = HANDLES(GetDC(NULL));
  BITMAPINFO bi;
  memset(&bi, 0, sizeof(bi));
  bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
  bi.bmiHeader.biBitCount = 0;   // we do not want a palette
  
  if (!GetDIBits(hDC, 
                hSrc,
                0, 0, 
                NULL,
                &bi,
                DIB_RGB_COLORS))
  {
    TRACE_E("GetDIBits failed");
    if (hDC != NULL) HANDLES(ReleaseDC(NULL, hDC));
    return NULL;
  }

  if (bi.bmiHeader.biSizeImage == 0)
  {
    TRACE_E("bi.bmiHeader.biSizeImage == 0");
    if (hDC != NULL) HANDLES(ReleaseDC(NULL, hDC));
    return NULL;
  }
  if (hDC != NULL) HANDLES(ReleaseDC(NULL, hDC));
  
  HIMAGELIST hIL = ImageList_Create(16, 16, GetImageListColorFlags() | ILC_MASK, 0, 1);
  if (hIL != NULL)
  {
    // the ImageList_AddMasked function destroys the transparent color in hSrc, therefore
    // we add icons gradually and slowly
    int count = bi.bmiHeader.biWidth / 16;
    int i;
    for (i = 0; i < count; i++)
    {
      HICON hIcon = GetIconFromDIB(hSrc, i);
      ImageList_AddIcon(hIL, hIcon);
      HANDLES(DestroyIcon(hIcon));
    }
  }
  return hIL;*/
}

//****************************************************************************
//
// LoadPNGBitmap()
//
//

struct CPNGBitmapReadCallbackData
{
    const BYTE* RawPNGIterator;
    DWORD Size;
};

unsigned LoadPNGBitmapReadCallback(void* output, size_t size, size_t numel, void* user_pointer)
{
    CPNGBitmapReadCallbackData* callbackData = (CPNGBitmapReadCallbackData*)user_pointer;
    if (output != NULL)
        memcpy(output, callbackData->RawPNGIterator, size * numel);
    callbackData->RawPNGIterator += size * numel;
    return (unsigned)(size * numel);
}

HBITMAP LoadPNGBitmapCreateDIB(const png_t* png, const BYTE* buff, int buffSize, DWORD flags)
{
    HDC hMemDC = HANDLES(CreateCompatibleDC(NULL));
    BITMAPINFOHEADER bmhdr;
    memset(&bmhdr, 0, sizeof(bmhdr));
    bmhdr.biSize = sizeof(bmhdr);
    bmhdr.biWidth = png->width;
    bmhdr.biHeight = -1 * png->height; // top-down
    bmhdr.biPlanes = 1;
    bmhdr.biBitCount = 32;
    bmhdr.biCompression = BI_RGB;
    void* lpMemBits = NULL;
    HBITMAP hMemBmp = HANDLES(CreateDIBSection(hMemDC, (CONST BITMAPINFO*)&bmhdr,
                                               DIB_RGB_COLORS, &lpMemBits, NULL, 0));
    if (hMemBmp == NULL)
    {
        HANDLES(DeleteDC(hMemDC));
        return NULL;
    }
    HBITMAP hOldMemBmp = (HBITMAP)SelectObject(hMemDC, hMemBmp);

    switch (png->color_type)
    {
    case PNG_GREYSCALE:
    {
        DWORD row;
        for (row = 0; row < png->height; row++)
        {
            BYTE* srcPtr = (BYTE*)buff + row * png->width;
            DWORD* dstPtr = (DWORD*)lpMemBits + row * png->width;
            DWORD col;
            for (col = 0; col < png->width; col++)
            {
                BYTE clr = *srcPtr;
                BYTE alpha = 0;
                *dstPtr = (DWORD)alpha << 24 | (DWORD)clr << 16 | (DWORD)clr << 8 | (DWORD)clr;
                srcPtr++;
                dstPtr++;
            }
        }
        break;
    }

    case PNG_GREYSCALE_ALPHA:
    {
        DWORD row;
        for (row = 0; row < png->height; row++)
        {
            BYTE* srcPtr = (BYTE*)buff + row * png->width * 2;
            DWORD* dstPtr = (DWORD*)lpMemBits + row * png->width;
            DWORD col;
            for (col = 0; col < png->width; col++)
            {
                BYTE clr = *srcPtr;
                srcPtr++; // skip gray
                BYTE alpha = 0;
                if (flags & SALPNG_GETALPHA)
                {
                    alpha = *srcPtr;
                    if (flags & SALPNG_PREMULTIPLE)
                        clr = clr * alpha / 255;
                }
                srcPtr++; // skip alpha
                *dstPtr = (DWORD)alpha << 24 | (DWORD)clr << 16 | (DWORD)clr << 8 | (DWORD)clr;
                dstPtr++;
            }
        }
        break;
    }

    case PNG_TRUECOLOR:
    {
        DWORD row;
        for (row = 0; row < png->height; row++)
        {
            BYTE* srcPtr = (BYTE*)buff + row * png->width * 3;
            DWORD* dstPtr = (DWORD*)lpMemBits + row * png->width;
            DWORD col;
            for (col = 0; col < png->width; col++)
            {
                BYTE clrR = *srcPtr;
                BYTE clrG = *(srcPtr + 1);
                BYTE clrB = *(srcPtr + 2);
                BYTE alpha = 0;
                *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                srcPtr += 3;
                dstPtr++;
            }
        }
        break;
    }

    case PNG_TRUECOLOR_ALPHA:
    {
        DWORD row;
        for (row = 0; row < png->height; row++)
        {
            BYTE* srcPtr = (BYTE*)buff + row * png->width * 4;
            DWORD* dstPtr = (DWORD*)lpMemBits + row * png->width;
            DWORD col;
            for (col = 0; col < png->width; col++)
            {
                BYTE clrR = *srcPtr;
                BYTE clrG = *(srcPtr + 1);
                BYTE clrB = *(srcPtr + 2);
                srcPtr += 3; // skip RGB
                BYTE alpha = 0;
                if (flags & SALPNG_GETALPHA)
                {
                    alpha = *srcPtr;
                    if (flags & SALPNG_PREMULTIPLE)
                    {
                        clrR = clrR * alpha / 255;
                        clrG = clrG * alpha / 255;
                        clrB = clrB * alpha / 255;
                    }
                }
                srcPtr++; // skip alpha
                *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                dstPtr++;
            }
        }
        break;
    }

    case PNG_INDEXED:
    {
        DWORD row;
        for (row = 0; row < png->height; row++)
        {
            BYTE* srcPtr = (BYTE*)buff + row * png->width;
            DWORD* dstPtr = (DWORD*)lpMemBits + row * png->width;
            DWORD col;
            for (col = 0; col < png->width; col++)
            {
                if ((unsigned)*srcPtr * 3 + 2 < png->palettelen)
                {
                    BYTE clrR = png->palette[*srcPtr * 3];
                    BYTE clrG = png->palette[*srcPtr * 3 + 1];
                    BYTE clrB = png->palette[*srcPtr * 3 + 2];
                    BYTE alpha = 0;

                    if (flags & SALPNG_GETALPHA)
                    {
                        if ((unsigned)*srcPtr < png->transparencylen)
                        {
                            alpha = png->transparency[*srcPtr];
                            if (flags & SALPNG_PREMULTIPLE)
                            {
                                clrR = clrR * alpha / 255;
                                clrG = clrG * alpha / 255;
                                clrB = clrB * alpha / 255;
                            }
                        }
                        else
                            alpha = 255;
                    }
                    *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                }
                else
                    TRACE_E("CIconList::LoadPNGBitmapCreateDIB(): wrong png format");
                srcPtr++;
                dstPtr++;
            }
        }
        break;
    }

    default:
    {
        TRACE_E("LoadPNGBitmapCreateDIB(): unknown PNG type=" << png->color_type);
        break;
    }
    }

    SelectObject(hMemDC, hOldMemBmp);
    HANDLES(DeleteDC(hMemDC));
    return hMemBmp;
}

BOOL GetBuffSizeForExpandedPNG(const png_t* png, DWORD* size)
{
    DWORD buffSize = png->width * png->height;
    BOOL knownType = TRUE;
    switch (png->color_type)
    {
    case PNG_GREYSCALE:
    {
        break;
    }

    case PNG_TRUECOLOR:
    {
        buffSize *= 3;
        break;
    }

    case PNG_INDEXED:
    {
        break;
    }

    case PNG_GREYSCALE_ALPHA:
    {
        buffSize *= 2;
        break;
    }

    case PNG_TRUECOLOR_ALPHA:
    {
        buffSize *= 4;
        break;
    }

    default:
    {
        TRACE_E("GetBuffSizeForExpandedPNG(): unknown PNG type=" << png->color_type);
        knownType = FALSE;
        break;
    }
    }
    if (knownType)
    {
        *size = buffSize;
        return TRUE;
    }
    else
        return FALSE;
}

HBITMAP LoadRawPNGBitmap(const void* rawPNG, DWORD rawPNGSize, DWORD flags)
{
    png_t png;

    // initialize pnglite with default memory allocator
    png_init(0, 0);

    CPNGBitmapReadCallbackData callbackData;
    callbackData.RawPNGIterator = (const BYTE*)rawPNG;
    callbackData.Size = rawPNGSize;
    int result = png_open(&png, LoadPNGBitmapReadCallback, &callbackData);
    if (result >= 0)
    {
        DWORD buffSize;
        if (GetBuffSizeForExpandedPNG(&png, &buffSize))
        {
            BYTE* buff = (BYTE*)malloc(buffSize + 1);
            if (buff != NULL)
            {
                buff[buffSize] = 0xfe;
                result = png_get_data(&png, buff);
                if (result >= 0)
                {
                    HBITMAP hBitmap = LoadPNGBitmapCreateDIB(&png, buff, buffSize, flags);
                    png_free_data(&png); // release palette, etc allocated in png_get_data
                    if (buff[buffSize] != 0xfe)
                    {
                        TRACE_E("Memory corrupted!!!");
                    }
                    free(buff);
                    return hBitmap;
                }
            }
        }
    }
    TRACE_E("LoadRawPNGBitmap() failed. Reason: " << png_error_string(result));
    return NULL;
}

HBITMAP LoadPNGBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, DWORD flags)
{
    int count = 0;
    HRSRC hRsrc = FindResource(hInstance, lpBitmapName, RT_RCDATA);
    if (hRsrc != NULL)
    {
        BYTE* rawPNG = (BYTE*)LoadResource(hInstance, hRsrc);
        if (rawPNG != NULL)
        {
            DWORD size = SizeofResource(hInstance, hRsrc);
            if (size > 0)
            {
                HBITMAP hRet = LoadRawPNGBitmap(rawPNG, size, flags);
                if (hRet != NULL)
                    return hRet;
                else
                    TRACE_E("LoadPNGBitmap() failed on lpBitmapName: " << (WORD)(ULONG_PTR)(lpBitmapName));
            }
        }
    }
    else
        TRACE_E("LoadPNGBitmap() failed, cannot find lpBitmapName: " << (WORD)(ULONG_PTR)(lpBitmapName));

    return NULL;
}

BOOL CIconList::CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth)
{
    int count = 0;
    HRSRC hRsrc = FindResource(hInstance, lpBitmapName, RT_RCDATA);
    if (hRsrc != NULL)
    {
        BYTE* rawPNG = (BYTE*)LoadResource(hInstance, hRsrc);
        if (rawPNG != NULL)
        {
            DWORD size = SizeofResource(hInstance, hRsrc);
            if (size > 0)
                return CreateFromRawPNG(rawPNG, size, imageWidth);
        }
    }
    TRACE_E("CIconList::CreateFromPNG() failed, cannot find lpBitmapName: " << (WORD)(ULONG_PTR)(lpBitmapName));
    return FALSE;
}

BOOL CIconList::CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth)
{
    BOOL ret = FALSE;
    png_t png;

    // initialize pnglite with default memory allocator
    png_init(0, 0);

    CPNGBitmapReadCallbackData callbackData;
    callbackData.RawPNGIterator = (const BYTE*)rawPNG;
    callbackData.Size = rawPNGSize;
    int result = png_open(&png, LoadPNGBitmapReadCallback, &callbackData);
    if (result >= 0)
    {
        DWORD buffSize;
        if (GetBuffSizeForExpandedPNG(&png, &buffSize))
        {
            BYTE* buff = (BYTE*)malloc(buffSize + 1);
            if (buff != NULL)
            {
                buff[buffSize] = 0xfe;
                result = png_get_data(&png, buff);
                if (result >= 0)
                {
                    int count = png.width / imageWidth;
                    if (!Create(imageWidth, png.height, count))
                        return FALSE;

                    HANDLES(EnterCriticalSection(&CriticalSection));

                    switch (png.color_type)
                    {
                    case PNG_GREYSCALE:
                    {
                        ret = TRUE;

                        int index;
                        for (index = 0; index < count; index++)
                        {
                            int row;
                            for (row = 0; row < ImageHeight; row++)
                            {
                                // Coordinates in points in HImage
                                int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
                                int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

                                BYTE* srcPtr = (BYTE*)buff + row * png.width + index * ImageWidth;
                                DWORD* dstPtr = (DWORD*)ImageRaw + iX + (iY + row) * BitmapWidth;

                                int col;
                                for (col = 0; col < ImageWidth; col++)
                                {
                                    BYTE clr = *srcPtr;
                                    BYTE alpha = 0;
                                    srcPtr++; // skip grayscale
                                    *dstPtr = (DWORD)alpha << 24 | (DWORD)clr << 16 | (DWORD)clr << 8 | (DWORD)clr;
                                    dstPtr++;
                                }
                            }
                        }
                        int i;
                        for (i = 0; i < count; i++)
                            ImageFlags[i] = IL_TYPE_NORMAL;
                        break;
                    }

                    case PNG_GREYSCALE_ALPHA:
                    {
                        ret = TRUE;
                        BOOL foundAlpha = FALSE;

                        int index;
                        for (index = 0; index < count; index++)
                        {
                            int row;
                            for (row = 0; row < ImageHeight; row++)
                            {
                                // Coordinates in points in HImage
                                int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
                                int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

                                BYTE* srcPtr = (BYTE*)buff + row * png.width * 2 + (index * ImageWidth) * 2;
                                DWORD* dstPtr = (DWORD*)ImageRaw + iX + (iY + row) * BitmapWidth;

                                int col;
                                for (col = 0; col < ImageWidth; col++)
                                {
                                    BYTE clr = *srcPtr;
                                    srcPtr++; // skip grayscale
                                    BYTE alpha = *srcPtr;
                                    srcPtr++; // skip alpha
                                    *dstPtr = (DWORD)alpha << 24 | (DWORD)clr << 16 | (DWORD)clr << 8 | (DWORD)clr;
                                    if (alpha != 0 && alpha != 0xFF)
                                        foundAlpha = TRUE;
                                    dstPtr++;
                                }
                            }
                        }
                        int i;
                        for (i = 0; i < count; i++)
                            ImageFlags[i] = foundAlpha ? IL_TYPE_ALPHA : IL_TYPE_NORMAL;
                        break;
                    }

                    case PNG_TRUECOLOR:
                    {
                        ret = TRUE;
                        int index;
                        for (index = 0; index < count; index++)
                        {
                            int row;
                            for (row = 0; row < ImageHeight; row++)
                            {
                                // Coordinates in points in HImage
                                int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
                                int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

                                BYTE* srcPtr = (BYTE*)buff + row * png.width * 4 + (index * ImageWidth) * 4;
                                DWORD* dstPtr = (DWORD*)ImageRaw + iX + (iY + row) * BitmapWidth;

                                int col;
                                for (col = 0; col < ImageWidth; col++)
                                {
                                    BYTE clrR = *srcPtr;
                                    BYTE clrG = *(srcPtr + 1);
                                    BYTE clrB = *(srcPtr + 2);
                                    srcPtr += 3; // skip RGB
                                    BYTE alpha = 0;
                                    *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                                    dstPtr++;
                                }
                            }
                        }
                        int i;
                        for (i = 0; i < count; i++)
                            ImageFlags[i] = IL_TYPE_NORMAL;
                        break;
                    }

                    case PNG_TRUECOLOR_ALPHA:
                    {
                        ret = TRUE;
                        BOOL foundAlpha = FALSE;

                        int index;
                        for (index = 0; index < count; index++)
                        {
                            int row;
                            for (row = 0; row < ImageHeight; row++)
                            {
                                // Coordinates in points in HImage
                                int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
                                int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

                                BYTE* srcPtr = (BYTE*)buff + row * png.width * 4 + (index * ImageWidth) * 4;
                                DWORD* dstPtr = (DWORD*)ImageRaw + iX + (iY + row) * BitmapWidth;

                                int col;
                                for (col = 0; col < ImageWidth; col++)
                                {
                                    BYTE clrR = *srcPtr;
                                    BYTE clrG = *(srcPtr + 1);
                                    BYTE clrB = *(srcPtr + 2);
                                    srcPtr += 3; // skip RGB
                                    BYTE alpha = *srcPtr;
                                    srcPtr++; // skip alpha
                                    *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                                    if (alpha != 0 && alpha != 0xFF)
                                        foundAlpha = TRUE;
                                    dstPtr++;
                                }
                            }
                        }
                        int i;
                        for (i = 0; i < count; i++)
                            ImageFlags[i] = foundAlpha ? IL_TYPE_ALPHA : IL_TYPE_NORMAL;
                        break;
                    }

                    case PNG_INDEXED:
                    {
                        ret = TRUE;
                        BOOL foundAlpha = FALSE;
                        int index;
                        for (index = 0; index < count; index++)
                        {
                            int row;
                            for (row = 0; row < ImageHeight; row++)
                            {
                                // Coordinates in points in HImage
                                int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
                                int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);

                                BYTE* srcPtr = (BYTE*)buff + row * png.width + (index * ImageWidth);
                                DWORD* dstPtr = (DWORD*)ImageRaw + iX + (iY + row) * BitmapWidth;

                                int col;
                                for (col = 0; col < ImageWidth; col++)
                                {
                                    if ((unsigned)*srcPtr * 3 + 2 < png.palettelen)
                                    {
                                        BYTE clrR = png.palette[*srcPtr * 3];
                                        BYTE clrG = png.palette[*srcPtr * 3 + 1];
                                        BYTE clrB = png.palette[*srcPtr * 3 + 2];
                                        BYTE alpha = 0;
                                        if ((unsigned)*srcPtr < png.transparencylen)
                                        {
                                            alpha = png.transparency[*srcPtr];
                                            *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                                            if (alpha != 0)
                                                foundAlpha = TRUE;
                                        }
                                        else
                                            TRACE_E("CIconList::CreateFromPNGInternal(): wrong png format");
                                    }
                                    else
                                        TRACE_E("CIconList::CreateFromPNGInternal(): wrong png format");
                                    srcPtr++;
                                    dstPtr++;
                                }
                            }
                        }
                        int i;
                        for (i = 0; i < count; i++)
                            ImageFlags[i] = foundAlpha ? IL_TYPE_ALPHA : IL_TYPE_NORMAL;
                        break;
                    }

                    default:
                    {
                        TRACE_E("CIconList::CreateFromPNGInternal(): unknown PNG type=" << (int)png.color_type);
                    }
                    }
                    HANDLES(LeaveCriticalSection(&CriticalSection));
                    png_free_data(&png); // release palette, etc allocated in png_get_data
                }
                free(buff);
            }
        }
    }

    //  if (Dump)
    //  {
    //    TRACE_E("CreateFromPNG()");
    //    DumpToTrace(0, FALSE);
    //  }
    return ret;
}

struct CPNGBitmapWriteCallbackData
{
    BYTE* RawPNGIterator;
    DWORD FreeSpace;
    // return values
    BOOL NotEnoughSpace;
    DWORD Size;
};

unsigned PNGBitmapWriteCallback(void* input, size_t size, size_t numel, void* user_pointer)
{
    CPNGBitmapWriteCallbackData* callbackData = (CPNGBitmapWriteCallbackData*)user_pointer;
    if (input == NULL)
        return 0;
    if (callbackData->FreeSpace >= size * numel)
    {
        memcpy(callbackData->RawPNGIterator, input, size * numel);
        callbackData->RawPNGIterator += size * numel;
        callbackData->FreeSpace -= (DWORD)(size * numel);
        callbackData->Size += (DWORD)(size * numel);
        return (unsigned)(size * numel);
    }
    else
    {
        callbackData->NotEnoughSpace = TRUE;
        TRACE_E("PNGBitmapWriteCallback(): not enough space in prepared buffer!");
    }
    return 0;
}

BOOL CIconList::SaveToPNG(BYTE** rawPNG, DWORD* rawPNGSize)
{
    // prepare icons into one long row
    DWORD* buff = (DWORD*)malloc(ImageWidth * ImageCount * ImageHeight * 4);
    int index;
    for (index = 0; index < ImageCount; index++)
    {
        int row;
        for (row = 0; row < ImageHeight; row++)
        {
            // coordinates in points in the source
            int iX = ImageWidth * (index % IL_ITEMS_IN_ROW);
            int iY = ImageHeight * (index / IL_ITEMS_IN_ROW);
            BYTE* srcPtr = (BYTE*)ImageRaw + iX * 4 + (iY + row) * BitmapWidth * 4;
            DWORD* dstPtr = (DWORD*)buff + row * ImageWidth * ImageCount + (index * ImageWidth);
            int col;
            for (col = 0; col < ImageWidth; col++)
            {
                BYTE clrR = *srcPtr;
                BYTE clrG = *(srcPtr + 1);
                BYTE clrB = *(srcPtr + 2);
                srcPtr += 3; // skip RGB
                BYTE alpha = *srcPtr;
                srcPtr++; // skip alpha
                *dstPtr = (DWORD)alpha << 24 | (DWORD)clrR << 16 | (DWORD)clrG << 8 | (DWORD)clrB;
                dstPtr++;
            }
        }
    }

    CPNGBitmapWriteCallbackData callbackData;
    callbackData.FreeSpace = ImageWidth * ImageCount * ImageHeight * 4 * 2; // I'd rather allocate twice as much memory, PNG MUST fit in there
    callbackData.Size = 0;
    BYTE* output = (BYTE*)malloc(callbackData.FreeSpace);
    callbackData.RawPNGIterator = output;
    callbackData.NotEnoughSpace = FALSE;

    png_t png;
    png_open_write(&png, PNGBitmapWriteCallback, &callbackData);
    png_set_data(&png, ImageWidth * ImageCount, ImageHeight, 8, PNG_TRUECOLOR_ALPHA, (unsigned char*)buff);
    free(buff);

    if (callbackData.NotEnoughSpace)
    {
        *rawPNG = NULL;
        *rawPNGSize = 0;
        return FALSE;
    }
    else
    {
        *rawPNG = output;
        *rawPNGSize = callbackData.Size;
        return TRUE;
    }
}
