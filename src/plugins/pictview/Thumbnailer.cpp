// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* This file is based on thumbnl.cpp from Salamander core.
   Only minimal changes have been made to allow merging of possible bugfixes or updates:
     o CSalamanderThumbnailMaker not derived from CSalamanderThumbnailMakerAbstract
     o Avoided dependency on CFilesWindow (in ctor, GetCancelProcessing(), ProcessBuffer(), HandleIncompleteImages())
     o Several unneeded methods disabled 
     o CalculateThumbnailSize() separated from SetParameters()

   This files is included in both the plugin and the envelope.
   The plugin uses just CalculateThumbnailSize() and this only when using the envelope.
   The envelope uses the rest.
  */
#include "precomp.h"

#if defined(PICTVIEW_DLL_IN_SEPARATE_PROCESS) || defined(BUILD_ENVELOPE)

#include "Thumbnailer.h"

/*#include "plugins.h"
#include "fileswnd.h"
#include "thumbnl.h"
#include "cfgdlg.h"*/

//******************************************************************************
//
// CShrinkImage
//

CShrinkImage::CShrinkImage()
{
    Cleanup();
}

CShrinkImage::~CShrinkImage()
{
    Destroy();
}

void CShrinkImage::Cleanup()
{
    NormCoeffX = 0;
    NormCoeffY = 0;
    RowCoeff = NULL;
    ColCoeff = NULL;
    YCoeff = NULL;
    NormCoeff = 0;
    Y = 0;
    YBndr = 0;
    OutLine = NULL;
    Buff = NULL;
    OrigHeight = 0;
    NewWidth = 0;
    ProcessTopDown = TRUE;
}

BOOL CShrinkImage::Alloc(DWORD origWidth, DWORD origHeight,
                         WORD newWidth, WORD newHeight,
                         DWORD* outBuff, BOOL processTopDown)
{
#ifdef _DEBUG
    if (RowCoeff != NULL || ColCoeff != NULL || Buff != NULL)
        TRACE_E("RowCoeff != NULL || ColCoeff != NULL || Buff != NULL");
#endif // _DEBUG
    if (origWidth == 0 || origHeight == 0 || newWidth == 0 || newHeight == 0)
    {
        TRACE_E("origWidth == 0 || origHeight == 0 || newWidth == 0 || newHeight == 0");
        return FALSE;
    }
    // alokujeme a inicializujeme koeficienty
    RowCoeff = CreateCoeff(origWidth, newWidth, NormCoeffX);
    ColCoeff = CreateCoeff(origHeight, newHeight, NormCoeffY);
    // alokujeme a vycistime buffer
    Buff = (DWORD*)malloc(3 * newWidth * sizeof(DWORD));
    if (RowCoeff == NULL || ColCoeff == NULL || Buff == NULL)
    {
        TRACE_E(IDS_LOWMEMORY);
        Destroy();
        return FALSE;
    }

    ZeroMemory(Buff, 3 * newWidth * sizeof(DWORD));

    OrigHeight = origHeight;
    NewWidth = newWidth;
    ProcessTopDown = processTopDown;

    YCoeff = ColCoeff;
    // koeficienty pro stredove a pravy pixel pro pripadne dalsi kolo
    NormCoeff = NormCoeffY * NormCoeffX;
    // y-ova hranice sekce
    YBndr = *YCoeff++;
    // preskocime koeficient pro prvni radek
    YCoeff++;

    // pokud jedem odspodu, musime zacit poslednim radkem
    if (!ProcessTopDown)
        OutLine = outBuff + newWidth * (newHeight - 1);
    else
        OutLine = outBuff;

    return TRUE;
}

void CShrinkImage::Destroy()
{
    if (RowCoeff != NULL)
        free(RowCoeff);
    if (ColCoeff != NULL)
        free(ColCoeff);
    if (Buff != NULL)
        free(Buff);
    Cleanup();
}

DWORD*
CShrinkImage::CreateCoeff(DWORD origLen, WORD newLen, DWORD& norm)
{
    DWORD* res = (DWORD*)malloc(3 * newLen * sizeof(DWORD));
    if (res == NULL)
        return NULL;
    DWORD* coeff = res;
    DWORD sum = 0;
    DWORD lCoeff, rCoeff = 0;
    DWORD boundary, modulo;

    norm = (newLen << 12) / origLen;
    DWORD i;
    for (i = 0; i < newLen; i++)
    {
        sum += origLen;
        // vypocet pixelu, kterym prochazi nova hranice
        boundary = sum / newLen;
        // kolik z predesle hranice bude v leve casti teto sekce
        lCoeff = norm - rCoeff;
        // a nakonec vaha pixelu u praveho okraje sekce
        modulo = sum % newLen;
        if (modulo == 0)
        {
            // pokud nam hranice prochazi mezi pixely, uprednostnime levy pixel
            boundary--;
            rCoeff = norm;
        }
        else
            rCoeff = (modulo << 12) / origLen;
        // a ulozime do pole - prvni je souradnice hranice
        *coeff++ = boundary;
        // dalsi je vaha pixelu u leveho okraje
        *coeff++ = lCoeff;
        // a vaha u praveho okraje
        *coeff++ = rCoeff;
    }
    return res;
}

void CShrinkImage::ProcessRows(DWORD* inBuff, DWORD rowCount)
{
    DWORD* ptrXCoeff;
    DWORD xCoeff, yCoeff, xNewCoeff;
    DWORD x1, x2, xBndr;
    DWORD* currPix;
    BYTE r, g, b;
    DWORD rgb;

    // jedem pres vsechny radky
    DWORD y;
    for (y = Y; y < Y + rowCount; y++)
    {
        // nainicializujeme pointery do bufferu
        currPix = Buff;
        // nainicializujem pointer do pole koeficientu
        ptrXCoeff = RowCoeff;
        // maximalni x-ova souradnice
        xBndr = *ptrXCoeff++;
        // levej koeficient je na zacatku radku stejnej jako stredni
        ptrXCoeff++;
        // pravej koeficient
        xCoeff = *ptrXCoeff++;

        x2 = 0;
        // rozdeleni podle polohy radku v sekci (stredni nebo posledni)
        if (y == YBndr)
        {
            // vytahneme koeficient pro posledni radek
            DWORD yLastCoeff = *YCoeff++;
            // vytahneme koeficient pro prvni radek dalsi sekce (je-li nejaka)
            if (y + 1 < OrigHeight)
            {
                YBndr = *YCoeff++; // nova y-ova hranice sekce
                yCoeff = *YCoeff++;
            }
            else
            {
                YBndr = 0; // nova y-ova hranice sekce
                yCoeff = 0;
            }
            // koeficienty pro stredove a pravy pixel
            xNewCoeff = yCoeff * xCoeff;
            xCoeff *= yLastCoeff;
            // koeficienty pro dalsi radek
            DWORD midNewCoeff = yCoeff * NormCoeffX;
            DWORD midCoeff = yLastCoeff * NormCoeffX;
            // pomocne promenne pro pixel dalsiho radku
            DWORD nextR = 0;
            DWORD nextG = 0;
            DWORD nextB = 0;
            // a predpocitavame dalsi
            for (x1 = 0; x1 + 1 < NewWidth; x1++)
            {
                // jsme-li na poslednim radku, aktualni ukladame do vysledku
                // projedem stredni cast
                for (; x2 < xBndr; x2++)
                {
                    // vytahneme pixel
                    rgb = *inBuff++;
                    r = GetRValue(rgb);
                    g = GetGValue(rgb);
                    b = GetBValue(rgb);
                    // pripocitame ho do bufferu
                    currPix[0] += midCoeff * r;
                    currPix[1] += midCoeff * g;
                    currPix[2] += midCoeff * b;
                    // a pripravime i pixel z pristiho radku
                    nextR += midNewCoeff * r;
                    nextG += midNewCoeff * g;
                    nextB += midNewCoeff * b;
                }
                // vytahneme nejpravejsi pixel
                rgb = *inBuff++;
                r = GetRValue(rgb);
                g = GetGValue(rgb);
                b = GetBValue(rgb);
                // napocitany pixel uz muzem poslat na vystup
                *OutLine++ = RGB((currPix[0] + xCoeff * r) >> 24,
                                 (currPix[1] + xCoeff * g) >> 24,
                                 (currPix[2] + xCoeff * b) >> 24);
                // pripravime pixel pro dalsi radek
                currPix[0] = nextR + xNewCoeff * r;
                currPix[1] = nextG + xNewCoeff * g;
                currPix[2] = nextB + xNewCoeff * b;
                // zvetsime souradnici
                x2++;
                // soupnem se ve vystupu na dalsi pixel
                currPix += 3;
                // nova maximalni x-ova souradnice
                xBndr = *ptrXCoeff++;
                // novej levej koeficient pro oba radky
                xNewCoeff = yCoeff * *ptrXCoeff;
                xCoeff = yLastCoeff * *ptrXCoeff++;
                // a taky ho pripocitame do bufferu pro dalsi pixel
                currPix[0] += xCoeff * r;
                currPix[1] += xCoeff * g;
                currPix[2] += xCoeff * b;
                // a pripravime i pixel z pristiho radku
                nextR = xNewCoeff * r;
                nextG = xNewCoeff * g;
                nextB = xNewCoeff * b;
                // a novej pravej koeficient
                xNewCoeff = yCoeff * *ptrXCoeff;
                xCoeff = yLastCoeff * *ptrXCoeff++;
            }
            // pro posledni pixel musime vynechat vypocet leve casti
            // dalsiho pixelu (zadnej neni)
            for (; x2 < xBndr; x2++)
            {
                // vytahneme pixel
                rgb = *inBuff++;
                r = GetRValue(rgb);
                g = GetGValue(rgb);
                b = GetBValue(rgb);
                // pripocitame ho do bufferu
                currPix[0] += midCoeff * r;
                currPix[1] += midCoeff * g;
                currPix[2] += midCoeff * b;
                // a pripravime i pixel z pristiho radku
                nextR += midNewCoeff * r;
                nextG += midNewCoeff * g;
                nextB += midNewCoeff * b;
            }
            // vytahneme nejpravejsi pixel
            rgb = *inBuff++;
            r = GetRValue(rgb);
            g = GetGValue(rgb);
            b = GetBValue(rgb);
            // napocitany pixel uz muzem poslat na vystup
            *OutLine++ = RGB((currPix[0] + xCoeff * r) >> 24,
                             (currPix[1] + xCoeff * g) >> 24,
                             (currPix[2] + xCoeff * b) >> 24);
            // pripravime pixel pro dalsi radek
            currPix[0] = nextR + xNewCoeff * r;
            currPix[1] = nextG + xNewCoeff * g;
            currPix[2] = nextB + xNewCoeff * b;
            // mame hotovej celej radek

            // pokud jedem odspodu, pokracujem o radek vys
            if (!ProcessTopDown)
                OutLine -= NewWidth * 2;
        }
        else
        {
            // pravej koeficient
            xCoeff *= NormCoeffY;
            // jsme-li na stredovych pixelech, pocitame normalne
            for (x1 = 0; x1 + 1 < NewWidth; x1++)
            {
                // projedem stredni cast
                for (; x2 < xBndr; x2++)
                {
                    // vytahneme pixel
                    rgb = *inBuff++;
                    r = GetRValue(rgb);
                    g = GetGValue(rgb);
                    b = GetBValue(rgb);
                    // pripocitame ho do bufferu
                    currPix[0] += NormCoeff * r;
                    currPix[1] += NormCoeff * g;
                    currPix[2] += NormCoeff * b;
                }
                // vytahneme nejpravejsi pixel
                rgb = *inBuff++;
                r = GetRValue(rgb);
                g = GetGValue(rgb);
                b = GetBValue(rgb);
                // a taky ho pripocitame do bufferu
                currPix[0] += xCoeff * r;
                currPix[1] += xCoeff * g;
                currPix[2] += xCoeff * b;
                // zvetsime souradnici
                x2++;
                // soupnem se ve vystupu na dalsi pixel
                currPix += 3;
                // nova maximalni x-ova souradnice
                xBndr = *ptrXCoeff++;
                // novej levej koeficient
                xCoeff = NormCoeffY * *ptrXCoeff++;
                // a taky ho pripocitame do bufferu pro dalsi pixel
                currPix[0] += xCoeff * r;
                currPix[1] += xCoeff * g;
                currPix[2] += xCoeff * b;
                // a novej pravej koeficient
                xCoeff = NormCoeffY * *ptrXCoeff++;
            }
            // pro posledni pixel musime vynechat vypocet leve casti
            for (; x2 < xBndr; x2++)
            {
                // vytahneme pixel
                rgb = *inBuff++;
                r = GetRValue(rgb);
                g = GetGValue(rgb);
                b = GetBValue(rgb);
                // pripocitame ho do bufferu
                currPix[0] += NormCoeff * r;
                currPix[1] += NormCoeff * g;
                currPix[2] += NormCoeff * b;
            }
            // vytahneme nejpravejsi pixel
            rgb = *inBuff++;
            r = GetRValue(rgb);
            g = GetGValue(rgb);
            b = GetBValue(rgb);
            // a taky ho pripocitame do bufferu
            currPix[0] += xCoeff * r;
            currPix[1] += xCoeff * g;
            currPix[2] += xCoeff * b;
            // mame hotovej celej radek
        }
    }
    Y += rowCount;
}

//******************************************************************************
//
// CSalamanderThumbnailMaker
//

CSalamanderThumbnailMaker::CSalamanderThumbnailMaker(/*CFilesWindow *window*/)
{
    //  Window = window;
    Buffer = NULL;
    BufferSize = 0;

    ThumbnailBuffer = NULL;
    AuxTransformBuffer = NULL;
    ThumbnailMaxWidth = 0; // musime inicializovat, Clear() volame s -1
    ThumbnailMaxHeight = 0;

    Clear(-1);
}

CSalamanderThumbnailMaker::~CSalamanderThumbnailMaker()
{
    if (Buffer != NULL)
        free(Buffer);
    if (ThumbnailBuffer != NULL)
        free(ThumbnailBuffer);
    if (AuxTransformBuffer != NULL)
        free(AuxTransformBuffer);
}

// vycisteni objektu - vola se pred zpracovanim dalsiho thumbnailu nebo kdyz uz
// neni potreba thumbnail (at uz hotovy nebo ne) z tohoto objektu
void CSalamanderThumbnailMaker::Clear(int thumbnailMaxSize)
{
    Error = FALSE;
    NextLine = -1;

    OriginalWidth = 0;
    OriginalHeight = 0;
    PictureFlags = 0;
    ProcessTopDown = TRUE;

    ThumbnailRealWidth = 0;
    ThumbnailRealHeight = 0;

    if (thumbnailMaxSize != -1)
    {
        if (thumbnailMaxSize != ThumbnailMaxWidth || thumbnailMaxSize != ThumbnailMaxHeight)
        {
            // pokud uzivatel zmenil velikost thumbnailu (v konfiguraci), musime nechat znovu
            // naalokovat ThumbnailBuffer a AuxTransformBuffer
            if (ThumbnailBuffer != NULL)
            {
                free(ThumbnailBuffer);
                ThumbnailBuffer = NULL;
            }
            if (AuxTransformBuffer != NULL)
            {
                free(AuxTransformBuffer);
                AuxTransformBuffer = NULL;
            }
            ThumbnailMaxWidth = thumbnailMaxSize;
            ThumbnailMaxHeight = thumbnailMaxSize;
        }
    }

    ShrinkImage = FALSE;
    Shrinker.Destroy();
}

/*
// vraci TRUE pokud je v tomto objektu pripraveny cely thumbnail (povedlo se
// jeho ziskani od pluginu)
BOOL
CSalamanderThumbnailMaker::ThumbnailReady()
{
  return OriginalHeight != 0 && NextLine >= OriginalHeight && !Error;
}

void
CSalamanderThumbnailMaker::TransformThumbnail()
{
  // SSTHUMB_MIRROR_VERT uz je hotova, zbyva provest SSTHUMB_MIRROR_HOR a SSTHUMB_ROTATE_90CW
  int transformation = (PictureFlags & (SSTHUMB_MIRROR_HOR | SSTHUMB_ROTATE_90CW));
  switch (transformation)
  {
    case 0: break;  // neni to delat

    case SSTHUMB_MIRROR_HOR:
    {
      DWORD realWidth = ThumbnailRealWidth;
      DWORD realWidth_min1 = ThumbnailRealWidth - 1;
      DWORD realHeight = ThumbnailRealHeight;
      DWORD *lineData = ThumbnailBuffer;
      DWORD *lineDataTgt = AuxTransformBuffer;
      DWORD line;
      for (line = 0; line < realHeight; line++)
      {
        DWORD i;
        for (i = 0; i < realWidth; i++)
          lineDataTgt[realWidth_min1 - i] = lineData[i];
        lineData += realWidth;
        lineDataTgt += realWidth;
      }
      DWORD *swap = ThumbnailBuffer;
      ThumbnailBuffer = AuxTransformBuffer;
      AuxTransformBuffer = swap;
      break;
    }

    case SSTHUMB_MIRROR_HOR | SSTHUMB_ROTATE_90CW:
    {
      DWORD realWidth = ThumbnailRealWidth;
      DWORD endOffset = ThumbnailRealHeight * ThumbnailRealWidth - 1;
      DWORD realHeight = ThumbnailRealHeight;
      DWORD realHeight_min1 = ThumbnailRealHeight - 1;
      DWORD *lineData = ThumbnailBuffer;
      DWORD *dataTgt = AuxTransformBuffer;
      DWORD line;
      for (line = 0; line < realHeight; line++)
      {
        DWORD offset = endOffset - line;
        DWORD i;
        for (i = 0; i < realWidth; i++)
        {
          dataTgt[offset] = lineData[i];
          offset -= realHeight;
        }
        lineData += realWidth;
      }
      DWORD *swap = ThumbnailBuffer;
      ThumbnailBuffer = AuxTransformBuffer;
      AuxTransformBuffer = swap;
      ThumbnailRealWidth = realHeight;
      ThumbnailRealHeight = realWidth;
      break;
    }

    case SSTHUMB_ROTATE_90CW:
    {
      DWORD realWidth = ThumbnailRealWidth;
      DWORD realHeight = ThumbnailRealHeight;
      DWORD realHeight_min1 = ThumbnailRealHeight - 1;
      DWORD *lineData = ThumbnailBuffer;
      DWORD *dataTgt = AuxTransformBuffer;
      DWORD line;
      for (line = 0; line < realHeight; line++)
      {
        DWORD offset = realHeight_min1 - line;
        DWORD i;
        for (i = 0; i < realWidth; i++)
        {
          dataTgt[offset] = lineData[i];
          offset += realHeight;
        }
        lineData += realWidth;
      }
      DWORD *swap = ThumbnailBuffer;
      ThumbnailBuffer = AuxTransformBuffer;
      AuxTransformBuffer = swap;
      ThumbnailRealWidth = realHeight;
      ThumbnailRealHeight = realWidth;
      break;
    }
  }
}

// nama drzeny thumbnail prevedeme na DDB a jeji data ulozime do CThumbnailData
BOOL
CSalamanderThumbnailMaker::RenderToThumbnailData(CThumbnailData *data)
{
  // vytvorime DDB a nechame ji inicializovat RGB datama thumbnailu
  HDC hDC = HANDLES(GetDC(NULL));
	BITMAPINFO srcBI;
	memset(&srcBI, 0, sizeof(BITMAPINFO));
	srcBI.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
	srcBI.bmiHeader.biWidth       = ThumbnailRealWidth;
	srcBI.bmiHeader.biHeight      = -ThumbnailRealHeight; // mam top-down reprezentaci
	srcBI.bmiHeader.biPlanes      = 1;
	srcBI.bmiHeader.biBitCount    = 32;
  srcBI.bmiHeader.biCompression = BI_RGB;
  HBITMAP hBmp = HANDLES(CreateDIBitmap(hDC, &srcBI.bmiHeader, CBM_INIT, 
                         ThumbnailBuffer, &srcBI, DIB_RGB_COLORS));
  HANDLES(ReleaseDC(NULL, hDC));

  if (hBmp == NULL)
  {
    TRACE_E("Error creating bitmap!");
    return FALSE;
  }

  // ziskame "geometrii" prave vytvorene bitmapy
  BITMAP bitmap;
  if (GetObject(hBmp, sizeof(BITMAP), &bitmap) == NULL)
  {
    TRACE_E("GetObject failed!");
    HANDLES(DeleteObject(hBmp));
    return FALSE;
  }

  // alokujeme buffer pro raw data bitmpay
  // destrukce probehne v CIconCache::Destroy() nebo o par radku niz
  // v pripade refreshe

  DWORD rawSize = bitmap.bmWidthBytes * bitmap.bmPlanes * bitmap.bmHeight;  

  DWORD *bits = (DWORD *)malloc(rawSize);
  if (bits == NULL)
  {
    TRACE_E(LOW_MEMORY);
    HANDLES(DeleteObject(hBmp));
    return FALSE;
  }

  // vytahneme raw data do alokovaneho pole
  if (GetBitmapBits(hBmp, rawSize, bits) == NULL)
  {
    TRACE_E("GetBitmapBits failed!");
    free(bits);
    HANDLES(DeleteObject(hBmp));
    return FALSE;
  }

  // zahodime bitmapu
  HANDLES(DeleteObject(hBmp));

  // pokud uz nejaka data drzime, musime je uvolnit pro nova
  if (data->Bits != NULL)
    free(data->Bits);

  // ulozime vysledek
  data->Width = (WORD)bitmap.bmWidth;
  data->Height = (WORD)bitmap.bmHeight;
  data->Planes = bitmap.bmPlanes;
  data->BitsPerPixel = bitmap.bmBitsPixel;
  data->Bits = bits;

  return TRUE;
}
*/
void CSalamanderThumbnailMaker::HandleIncompleteImages()
{
    if (!Error && NextLine < OriginalHeight && ThumbnailRealHeight > 0 &&
        NextLine >= (3 * OriginalHeight / ThumbnailRealHeight) &&
        /*!Window->ICStopWork && */ OriginalWidth > 0)
    {
        if (GetBuffer(1) != NULL)
        {
            memset(Buffer, 0xFF, BufferSize);
            int maxRowsInBuf = BufferSize / OriginalWidth / sizeof(DWORD);
            if (maxRowsInBuf > 0)
            {
                while (NextLine < OriginalHeight)
                {
                    if (!ProcessBuffer(Buffer, min(maxRowsInBuf, OriginalHeight - NextLine)))
                        break;
                }
            }
            else
                TRACE_E("CSalamanderThumbnailMaker::HandleIncompleteImages(): this should never happen!");
        }
    }
}

// *********************************************************************************
// metody rozhrani CSalamanderThumbnailMakerAbstract
// *********************************************************************************

BOOL CSalamanderThumbnailMaker::SetParameters(int picWidth, int picHeight, DWORD flags)
{
    if (Error)
    {
        TRACE_E("CSalamanderThumbnailMaker::SetParameters(): Error == TRUE");
        return FALSE;
    }
    if (picWidth < 1 || picHeight < 1)
    {
        TRACE_E("CSalamanderThumbnailMaker::SetParameters invalid parameters: picWidth=" << picWidth << " picHeight=" << picHeight);
        Error = TRUE;
        return FALSE;
    }
    OriginalWidth = picWidth;
    OriginalHeight = picHeight;
    PictureFlags = flags;
    ProcessTopDown = (flags & SSTHUMB_MIRROR_VERT) == 0;

    int maxWidth = ThumbnailMaxWidth; // maximalni velikost thumbnailu
    int maxHeight = ThumbnailMaxHeight;

    if (maxWidth < 1 || maxHeight < 1)
    {
        TRACE_E("CSalamanderThumbnailMaker::SetParameters invalid parameters: ThumbnailMaxWidth=" << maxWidth << " or ThumbnailMaxHeight=" << maxHeight);
        Error = TRUE;
        return FALSE;
    }

    ShrinkImage = CalculateThumbnailSize(OriginalWidth, OriginalHeight, maxWidth, maxHeight, ThumbnailRealWidth, ThumbnailRealHeight);

    if (ThumbnailBuffer == NULL)
        ThumbnailBuffer = (DWORD*)malloc(maxWidth * maxHeight * sizeof(DWORD));
    if (AuxTransformBuffer == NULL)
        AuxTransformBuffer = (DWORD*)malloc(maxWidth * maxHeight * sizeof(DWORD));
    if (ThumbnailBuffer == NULL || AuxTransformBuffer == NULL)
    {
        if (ThumbnailBuffer != NULL)
            free(ThumbnailBuffer);
        if (AuxTransformBuffer != NULL)
            free(AuxTransformBuffer);
        ThumbnailBuffer = NULL;
        AuxTransformBuffer = NULL;
        TRACE_E(IDS_LOWMEMORY);
        Error = TRUE;
        return FALSE;
    }

    if (ShrinkImage)
    {
        Shrinker.Destroy();
        if (!Shrinker.Alloc(OriginalWidth, OriginalHeight,
                            ThumbnailRealWidth, ThumbnailRealHeight,
                            ThumbnailBuffer, ProcessTopDown))
        {
            Error = TRUE;
            return FALSE;
        }
    }

    NextLine = 0;
    return TRUE;
}

// Returns whether shrinking needs to happen
// maxWidth, maxHeight: max thumb size on input
// thumbWidth, thumbHeight: calculated thumb size on output
BOOL CSalamanderThumbnailMaker::CalculateThumbnailSize(int originalWidth, int originalHeight,
                                                       int maxWidth, int maxHeight,
                                                       int& thumbWidth, int& thumbHeight)
{
    if (originalWidth <= maxWidth && originalHeight <= maxHeight)
    {
        // okopirujeme data
        thumbWidth = originalWidth;
        thumbHeight = originalHeight;
        return FALSE;
    }

    // zachovame pomer stran
    if ((double)maxWidth / (double)maxHeight < (double)originalWidth / (double)originalHeight)
    {
        thumbWidth = maxWidth;
        thumbHeight = (int)((double)maxWidth / ((double)originalWidth / (double)originalHeight));
    }
    else
    {
        thumbHeight = maxHeight;
        thumbWidth = (int)((double)maxHeight / ((double)originalHeight / (double)originalWidth));
    }
    // do algoritmu nesmi vstoupit zadny z rozmeru nulovy; radeji porusime proporce
    if (thumbWidth < 1)
        thumbWidth = 1;
    if (thumbHeight < 1)
        thumbHeight = 1;
    return TRUE;
}

BOOL CSalamanderThumbnailMaker::GetCancelProcessing()
{
    if (Error || NextLine >= OriginalHeight /*|| Window->ICStopWork*/)
        return TRUE;
    else
        return FALSE;
}

BOOL CSalamanderThumbnailMaker::ProcessBuffer(void* buffer, int rowsCount)
{
    if (Error || NextLine >= OriginalHeight /* || Window->ICStopWork*/)
    {
        /*    if (!Window->ICStopWork)
      TRACE_E("CSalamanderThumbnailMaker::ProcessBuffer failed. Error="<<Error<<
              " NextLine="<<NextLine<<" OriginalHeight="<<OriginalHeight);*/
        return FALSE; // budeme koncit (chyba, presah nebo sleep-icon-cache)
    }
    if (NextLine == -1)
    {
        TRACE_E("Call SetParameters before ProcessBuffer!");
        return FALSE;
    }
#ifdef _DEBUG
    if (NextLine + rowsCount > OriginalHeight)
    {
        TRACE_E("CSalamanderThumbnailMaker::ProcessBuffer(): Too much rows (" << rowsCount << ") to process (they overlap picture)!");
        Error = TRUE;
        return FALSE;
    }
#endif // _DEBUG
    if (buffer == NULL)
    {
        buffer = Buffer;
#ifdef _DEBUG
        if (BufferSize / OriginalWidth / (int)sizeof(DWORD) < rowsCount)
        {
            TRACE_E("CSalamanderThumbnailMaker::ProcessBuffer(): Too much rows (" << rowsCount << ") in internal buffer! (insufficient size of buffer)");
            Error = TRUE;
            return FALSE;
        }
#endif // _DEBUG
    }

    if (rowsCount > 0)
    {
        if (ShrinkImage)
        {
            // zmensime na thumbnail
            Shrinker.ProcessRows((DWORD*)buffer, rowsCount);
        }
        else
        {
            // preneseme 1:1
            if (ProcessTopDown)
            {
                memcpy(ThumbnailBuffer + NextLine * ThumbnailRealWidth, buffer, rowsCount * ThumbnailRealWidth * sizeof(DWORD));
            }
            else
            {
                int i;
                for (i = 0; i < rowsCount; i++)
                    memcpy(ThumbnailBuffer + (OriginalHeight - NextLine - i - 1) * ThumbnailRealWidth,
                           (DWORD*)buffer + i * ThumbnailRealWidth, ThumbnailRealWidth * sizeof(DWORD));
            }
        }
        NextLine += rowsCount;
    }

    return NextLine < OriginalHeight;
}

void* CSalamanderThumbnailMaker::GetBuffer(int rowsCount)
{
    if (Error)
    {
        TRACE_E("CSalamanderThumbnailMaker::GetBuffer(): Error == TRUE");
        return NULL;
    }
    int required = rowsCount * OriginalWidth * sizeof(DWORD);
    if (required > BufferSize)
    {
        if (Buffer != NULL)
            free(Buffer);
        Buffer = (DWORD*)malloc(required);
        if (Buffer != NULL)
            BufferSize = required;
        else
        {
            BufferSize = 0;
            TRACE_E("CSalamanderThumbnailMaker::GetBuffer(): Unable to allocate internal buffer (size=" << required << ")!");
        }
    }
    return Buffer;
}

#endif // #if defined(PICTVIEW_DLL_IN_SEPARATE_PROCESS) || defined(BUILD_ENVELOPE)
