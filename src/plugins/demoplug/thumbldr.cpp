// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// ****************************************************************************
// THUMBNAIL LOADER SECTION
// ****************************************************************************

// opens the specified file and converts it into a sequence of DWORDs
// that means 24 bits for color (R, G, B) and 8 bits of padding
// the size of a single row in bytes is: image_width * sizeof(DWORD)
/*
// simple variant built on LoadImage, which blocks the rest of the OS
// and the loading process cannot be interrupted
CPluginInterfaceForThumbLoader::LoadThumbnail(const char *filename, int thumbWidth, int thumbHeight,
                                              CSalamanderThumbnailMakerAbstract *thumbMaker,
                                              BOOL fastThumbnail)
{
  HBITMAP hSrcBitmap = (HBITMAP)HANDLES(LoadImage(DLLInstance, filename,
                                                  IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE));
  if (hSrcBitmap != NULL)
  {
    // get the width and height of the image
    BITMAP srcBitmap;
    if (GetObject(hSrcBitmap, sizeof(BITMAP), &srcBitmap) != 0)
    {
      // create a copy in which every pixel is represented as COLORREF
	    BITMAPINFO tmpBI;
	    memset(&tmpBI, NULL, sizeof(BITMAPINFO));
	    tmpBI.bmiHeader.biSize     = sizeof(BITMAPINFOHEADER);
	    tmpBI.bmiHeader.biWidth    = srcBitmap.bmWidth;
            tmpBI.bmiHeader.biHeight   = -srcBitmap.bmHeight; // send a top-down bitmap to Salamander
	    tmpBI.bmiHeader.biPlanes   = 1;
            tmpBI.bmiHeader.biBitCount = 32;  // R, G, B, one byte of padding

      void* ptr;
      HBITMAP hTmpBitmap = HANDLES(CreateDIBSection(NULL, &tmpBI, DIB_RGB_COLORS, &ptr, NULL, NULL));
      DWORD *tmpBitmapBits = (DWORD*)ptr;

      if (hTmpBitmap != NULL && tmpBitmapBits != NULL) 
      {
        // if the bitmap was allocated, convert it into our format

        HDC hDC = HANDLES(GetDC(NULL));
        HDC hSrcDC = HANDLES(CreateCompatibleDC(hDC));
        HDC hTmpDC = HANDLES(CreateCompatibleDC(hDC));
        HBITMAP hOldSrcBitmap = (HBITMAP)SelectObject(hSrcDC, hSrcBitmap);
        HBITMAP hOldTmpBitmap = (HBITMAP)SelectObject(hTmpDC, hTmpBitmap);

	      BitBlt(hTmpDC, 0, 0, srcBitmap.bmWidth, srcBitmap.bmHeight, hSrcDC, 0, 0, SRCCOPY);

        SelectObject(hSrcDC, hOldSrcBitmap);
        SelectObject(hTmpDC, hOldTmpBitmap);
        HANDLES(DeleteDC(hSrcDC));
        HANDLES(DeleteDC(hTmpDC));
        HANDLES(ReleaseDC(NULL, hDC));

        GdiFlush(); // we are going to work with raw data, ensure GDI operations are flushed

        // hand the image to Salamander for downscaling
        thumbMaker->SetParameters(srcBitmap.bmWidth, srcBitmap.bmHeight, 0);
//        int i = 0;
//        while (i < srcBitmap.bmHeight)
//        {
//          thumbMaker->ProcessBuffer(tmpBitmapBits + i * srcBitmap.bmWidth, 1);
//          i += 1;
//        } 
        thumbMaker->ProcessBuffer(tmpBitmapBits, srcBitmap.bmHeight);

        HANDLES(DeleteObject(hTmpBitmap));
      }
      else
        thumbMaker->SetError(); // probably not enough memory; the file gets a simple icon
    }
    else
      thumbMaker->SetError(); // it's a bitmap, but probably corrupted; the file gets a simple icon

    HANDLES(DeleteObject(hSrcBitmap));
  }
  else
    return FALSE; // probably not a bitmap
  return TRUE;
}
*/

DWORD GetClrUsed(const BITMAPINFOHEADER* bih, BOOL max)
{
    if (!max && bih->biClrUsed != 0)
        return bih->biClrUsed;

    switch (bih->biBitCount)
    {
    case 1:
        return 2;
    case 4:
        return 16;
    case 8:
        return 256;
    default:
        return 0; // A 24 or 32 bitcount DIB has no color table
    }
}

BOOL ConvertDIBToCOLORREF(WORD bitCount, int width, int rows, int srcLineSize,
                          const void* srcBuffer, void* dstBuffer,
                          const LOGPALETTE* palette)
{
    DWORD* out = (DWORD*)dstBuffer;
    switch (bitCount)
    {
    case 1:
    {
        BYTE* in;
        BYTE palIndex;
        int i;
        for (i = 0; i < rows; i++)
        {
            in = (BYTE*)srcBuffer + i * srcLineSize;
            int j;
            for (j = 0; j < width / 8; j++)
            {
                palIndex = *in++;
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 7) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 6) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 5) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 4) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 3) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 2) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> 1) & 0x01)));
                *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex) & 0x01)));
            }
            j *= 8;
            if (j < width)
            {
                palIndex = *in;
                BYTE rot = 7;
                for (; j < width; j++)
                    *out++ = *((DWORD*)(palette->palPalEntry + ((palIndex >> rot--) & 0x01)));
            }
        }
        break;
    }

    case 4:
    {
        BYTE* in;
        BYTE palIndex;
        int i;
        for (i = 0; i < rows; i++)
        {
            in = (BYTE*)srcBuffer + i * srcLineSize;
            int j;
            for (j = 0; j < width / 2; j++)
            {
                palIndex = *in++;
                *out++ = *((DWORD*)(palette->palPalEntry + (palIndex >> 4)));
                *out++ = *((DWORD*)(palette->palPalEntry + (palIndex & 0x0f)));
            }
            if (j * 2 < width)
            {
                palIndex = *in;
                *out++ = *((DWORD*)(palette->palPalEntry + (palIndex >> 4)));
            }
        }
        break;
    }

    case 8:
    {
        BYTE* in;
        int i;
        for (i = 0; i < rows; i++)
        {
            in = (BYTE*)srcBuffer + i * srcLineSize;
            int j;
            for (j = 0; j < width; j++)
                *out++ = *((DWORD*)(palette->palPalEntry + (*in++)));
        }
        break;
    }

    case 24:
    {
        BYTE* in;
        int i;
        for (i = 0; i < rows; i++)
        {
            in = (BYTE*)srcBuffer + i * srcLineSize;
            int j;
            for (j = 0; j < width; j++)
            {
                *out++ = *((DWORD*)in);
                in += 3;
            }
        }
        break;
    }

    case 32:
    {
        break;
    }

    default:
        return FALSE;
    }
    return TRUE;
}

BOOL WINAPI
CPluginInterfaceForThumbLoader::LoadThumbnail(const char* filename,
                                              int thumbWidth,
                                              int thumbHeight,
                                              CSalamanderThumbnailMakerAbstract* thumbMaker,
                                              BOOL fastThumbnail)
{
    BOOL stopFurtherLoaders = TRUE; // don't try next parsers
    // we must call thumbMaker->SetError() when error occures and stopFurtherLoaders is TRUE

    // open the file
    HANDLE hFile = HANDLES_Q(CreateFile(filename, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                        NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
    if (hFile != NULL)
    {
        // read the DIB information (BITMAPFILEHEADER)
        BITMAPFILEHEADER bfh;
        DWORD read;
        if (ReadFile(hFile, &bfh, sizeof(bfh), &read, NULL))
        {
            //      if (thumbMaker->GetCancelProcessing())
            //      {
            //        // exit
            //      }

            if (read == sizeof(bfh) && bfh.bfType == 0x4d42) // 0x4d42 == "BM" (magic number)
            {
                // read the BITMAPINFOHEADER
                BITMAPINFOHEADER bih;
                if (ReadFile(hFile, &bih, sizeof(bih), &read, NULL))
                {
                    if (read == sizeof(bih))
                    {
                        // there may be BITMAPCOREHEADER instead of BITMAPINFOHEADER, but
                        // only BITMAPINFOHEADER is supported in this loader;
                        // only BI_RGB format is supported;
                        // only 1 plane format is supported
                        // only 1, 4, 8, 24, or 32 bits-per-pixel format is supported
                        if (bih.biSize == sizeof(BITMAPINFOHEADER) &&
                            bih.biCompression == BI_RGB &&
                            bih.biPlanes == 1 &&
                            (bih.biBitCount == 1 || bih.biBitCount == 4 ||
                             bih.biBitCount == 8 || bih.biBitCount == 24 ||
                             bih.biBitCount == 32))
                        {
                            int width = bih.biWidth;
                            int height = bih.biHeight;
                            BOOL processTopDown;
                            if (height > 0)
                                processTopDown = FALSE;
                            else
                            {
                                processTopDown = TRUE;
                                height *= -1;
                            }
                            // pass picture format to Open Salamander
                            if (thumbMaker->SetParameters(width, height, processTopDown ? 0 : SSTHUMB_MIRROR_VERT))
                            {
                                LOGPALETTE* palette = NULL;
                                BOOL exit = FALSE;
                                DWORD clrUsed = GetClrUsed(&bih, FALSE);
                                if (clrUsed != 0)
                                {
                                    // allocate and load palette
                                    DWORD paletteSize = clrUsed * sizeof(PALETTEENTRY);
                                    palette = (LOGPALETTE*)malloc(sizeof(LOGPALETTE) + GetClrUsed(&bih, TRUE) * sizeof(PALETTEENTRY));
                                    if (palette != NULL)
                                    {
                                        if (ReadFile(hFile, palette->palPalEntry, paletteSize, &read, NULL))
                                        {
                                            if (read == paletteSize)
                                            {
                                                palette->palVersion = 0x300;
                                                palette->palNumEntries = (WORD)clrUsed;
                                            }
                                            else
                                            {
                                                stopFurtherLoaders = FALSE; // unknown file format, try next parser
                                                exit = TRUE;
                                            }
                                        }
                                        else
                                        {
                                            thumbMaker->SetError(); // error reading the file
                                            exit = TRUE;
                                        }
                                    }
                                    else
                                    {
                                        thumbMaker->SetError(); // out of memory
                                        exit = TRUE;
                                    }
                                }

                                if (!exit)
                                    exit = thumbMaker->GetCancelProcessing();

                                if (!exit)
                                {
                                    //                  if (bfh.bfOffBits != 0)
                                    //                    SetFilePointer(hFile, bfh.bfOffBits, NULL, FILE_BEGIN);

                                    int rowSize = (DWORD)(bih.biWidth * bih.biBitCount + 31) / 32 * 4;

                                    int bufferLines = min(max(1, 50000 / rowSize), bih.biHeight);
                                    DWORD bufferSize = bufferLines * rowSize;
                                    void* srcBuffer = malloc(bufferSize + 1); // +1 as headroom for ConvertDIBToCOLORREF/24-bit
                                    void* dstBuffer;
                                    if (srcBuffer != NULL)
                                    {
                                        if (bih.biBitCount == 32)
                                            dstBuffer = srcBuffer; // DIB format is compatible with ProcessBuffer 'buffer' format
                                        else
                                            dstBuffer = thumbMaker->GetBuffer(bufferLines); // allocate destination buffer

                                        if (dstBuffer != NULL)
                                        {
                                            int line = 0;
                                            while (!exit)
                                            {
                                                int readLines = min(bufferLines, bih.biHeight - line);
                                                if (ReadFile(hFile, srcBuffer, readLines * rowSize, &read, NULL))
                                                {
                                                    if (read == (DWORD)readLines * rowSize)
                                                    {
                                                        if (ConvertDIBToCOLORREF(bih.biBitCount, bih.biWidth, readLines,
                                                                                 rowSize, srcBuffer, dstBuffer, palette))
                                                        {
                                                            if (thumbMaker->ProcessBuffer(dstBuffer, readLines))
                                                            {
                                                                line += readLines;
                                                                if (line >= bih.biHeight)
                                                                    exit = TRUE;
                                                            }
                                                            else
                                                            {
                                                                exit = TRUE;
                                                            }
                                                        }
                                                        else
                                                        {
                                                            stopFurtherLoaders = FALSE; // unknown file format, try next parser
                                                            exit = TRUE;
                                                        }
                                                    }
                                                    else
                                                    {
                                                        stopFurtherLoaders = FALSE; // unknown file format, try next parser
                                                        exit = TRUE;
                                                    }
                                                }
                                                else
                                                {
                                                    thumbMaker->SetError(); // error reading the file
                                                    exit = TRUE;
                                                }
                                            }
                                        }
                                        else
                                            thumbMaker->SetError(); // out of memory
                                        free(srcBuffer);
                                    }
                                    else
                                        thumbMaker->SetError(); // out of memory
                                }
                                if (palette != NULL)
                                    free(palette);
                            }
                        }
                        else
                            stopFurtherLoaders = FALSE; // unknown file format, try next parser
                    }
                    else
                        stopFurtherLoaders = FALSE; // unknown file format, try next parser
                }
                else
                    thumbMaker->SetError(); // error reading the file
            }
            else
                stopFurtherLoaders = FALSE; // unknown file format, try next parser
        }
        else
            thumbMaker->SetError(); // error reading the file

        HANDLES(CloseHandle(hFile));
    }
    else
        thumbMaker->SetError(); // error opening the file

    return stopFurtherLoaders;
}
