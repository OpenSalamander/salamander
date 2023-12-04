// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <xmmintrin.h>

// opatreni proti runtime check failure v debug verzi: puvodni verze makra pretypovava rgb na WORD,
// takze hlasi ztratu dat (RED slozky)
#undef GetGValue
#define GetGValue(rgb) ((BYTE)(((rgb) >> 8) & 0xFF))

extern BYTE CCushionGraphics_defaultdata[];

class CCushionGraphics
{
protected:
    BYTE* _pix;
    int _fixed_top;
    int _fixed_right;
    int _fixed_bottom;
    int _fixed_left;
    int _width;
    int _height;
    //BYTE _alphaTable[256][256];
    unsigned int _alphaTable[256 * 256];

public:
    CCushionGraphics()
    {
        this->_pix = NULL;
        for (int i = 0; i < 256; i++)
        {
            for (int j = 0; j < 256; j++)
            {
                //this->_alphaTable[i][j] = (BYTE)((i * j) / 255);
                this->_alphaTable[i * 256 + j] = ((i * j) / 255) & 0xFF;
            }
        }
        this->ResetPix();
    }
    ~CCushionGraphics()
    {
        if ((this->_pix != NULL) && (this->_pix != CCushionGraphics_defaultdata))
            free(this->_pix);
    }

    BOOL LoadFromFile(TCHAR const* filename)
    {
        HANDLE fa;
        DWORD read, lo, hi;
        DWORD err;
        BYTE* tbs;
        BOOL val;

        fa = CreateFile(filename, FILE_READ_DATA, FILE_READ_DATA, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (fa == INVALID_HANDLE_VALUE)
        {
            err = GetLastError();
            //errlen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, errbuff, 400, NULL);
            return FALSE;
        }
        lo = GetFileSize(fa, &hi);
        if ((hi > 0) || (lo > 1024 * 1024))
        {
            CloseHandle(fa);
            //_tcscpy(errbuff, TEXT("Internal error: Too big data"));
            //errlen = _tcslen(errbuff);
            return FALSE;
        }
        tbs = (BYTE*)malloc(lo);

        ReadFile(fa, tbs, lo, &read, NULL);
        CloseHandle(fa);
        if (read != lo)
        {
            //_tcscpy(errbuff, TEXT("Internal error: Read error"));
            //errlen = _tcslen(errbuff);
            free(tbs);
            return FALSE;
        }
        val = this->Load(tbs, read);
        if (!val)
            this->ResetPix();
        free(tbs);
        return val;
    }

    BOOL LoadFromResource(HINSTANCE hInst, TCHAR const* name /*, TCHAR *type*/)
    {
        HRSRC hRsrc;
        HGLOBAL hresm;
        DWORD len;
        DWORD err;
        BYTE* tbs;
        BOOL val;

        //hRsrc = FindResource(hInst, MAKEINTRESOURCE(IDR_CUSHIONDATA_ALPHA), TEXT("CUSHIONDATA"));
        hRsrc = FindResource(hInst, name, TEXT("CUSHIONDATA"));
        if (hRsrc == NULL)
        {
            err = GetLastError();
            //errlen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, errbuff, 400, NULL);
            return FALSE;
        }
        len = SizeofResource(hInst, hRsrc);
        hresm = LoadResource(hInst, hRsrc);
        if (hresm == NULL)
        {
            err = GetLastError();
            //errlen = FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM, NULL, err, 0, errbuff, 400, NULL);
            return FALSE;
        }
        tbs = (BYTE*)LockResource(hresm);
        if (tbs == NULL)
        {
            //_tcscpy(errbuff, TEXT("Internal error: Lock failed"));
            //errlen = _tcslen(errbuff);
            return FALSE;
        }
        val = this->Load(tbs, len);
        if (!val)
            this->ResetPix();
        return val;
    }

    BOOL Load(BYTE* dta, int size)
    {
        if (size < 10) //10byte hlavicka... 8header + 2version
        {
            //_tcscpy(errbuff, TEXT("Internal error: Wrong data size"));
            //errlen = _tcslen(errbuff);
            return FALSE;
        }
        //Header
        // Idea: http://en.wikipedia.org/wiki/JPEG_Network_Graphics
        //0x91 -- Private Use 1  http://en.wikipedia.org/wiki/C1_control_code
        //0x5A, 0x41, 0x54, 0x43 -- 'ZATC' = Treemap.Cushion
        //0x0D, 0x0A -- new line
        //0x1A -- end of file
        if ((dta[0x00] != 0x91) || (dta[0x01] != 0x5A) || (dta[0x02] != 0x41) || (dta[0x03] != 0x54) ||
            (dta[0x04] != 0x43) || (dta[0x05] != 0x0D) || (dta[0x06] != 0x0A) || (dta[0x07] != 0x1A)) //HEADER
        {
            //_tcscpy(errbuff, TEXT("Internal error: Corrupted header"));
            //errlen = _tcslen(errbuff);
            return FALSE;
        }

        if ((dta[0x08] == 2) && (dta[0x09] == 0)) //VERZE
        {
            return this->Load_version2(dta, size) && this->PremultiplyAlpha();
        }
        //_tcscpy(errbuff, TEXT("Internal error: Unsupported data version"));
        //errlen = _tcslen(errbuff);
        return FALSE;
    }

    BYTE* AllocPix(int width, int height)
    {
        if ((this->_pix != NULL) && (this->_pix != CCushionGraphics_defaultdata))
            free(this->_pix);

        this->_pix = (BYTE*)malloc(width * height * 2);
        memset(this->_pix, 255, width * height * 2);

        this->_fixed_top = 0;
        this->_fixed_right = 0;
        this->_fixed_bottom = 0;
        this->_fixed_left = 0;

        this->_width = width;
        this->_height = height;

        return this->_pix;
    }

    BYTE* ResetPix()
    {
        if ((this->_pix != NULL) && (this->_pix != CCushionGraphics_defaultdata))
            free(this->_pix);

        this->_pix = CCushionGraphics_defaultdata;
        this->_fixed_top = 1;
        this->_fixed_right = 1;
        this->_fixed_bottom = 1;
        this->_fixed_left = 1;
        this->_width = 3;
        this->_height = 3;

        return this->_pix;
    }

    BOOL Load_version2(BYTE* dta, int size)
    {
        BYTE* end = dta + size;
        BYTE* cend;

        BYTE *tbs, *tbd = NULL;
        BOOL SZfound = FALSE;
        BOOL EFfound = FALSE;

        int w = 0;
        int h = 0;
        int fit = 0, fir = 0, fib = 0, fil = 0;

        int mxs = 0;
        int mxc = 0;

        tbs = dta + 0x0A; // skip header and version info

        BYTE* gby = NULL;
        BYTE* aby = NULL;
        BOOL isAlpha = FALSE;

        while (tbs + 4 <= end) // position + 4bytes chunk info
        {
            mxc = *(unsigned short*)tbs;
            tbs += 2; //Chunk ID
            mxs = *(unsigned short*)tbs;
            tbs += 2; //Chunk size

            cend = tbs + mxs;

            if (cend > end)
                return FALSE; //chunk size too large

            switch (mxc)
            {
            case 0x5A53: //SZ (size info)
                if (SZfound)
                    return FALSE; //chunk already read
                if (mxs != 4)
                    return FALSE; //wrong size
                SZfound = TRUE;
                w = *(unsigned short*)tbs;
                tbs += 2;
                h = *(unsigned short*)tbs;
                tbs += 2;
                this->AllocPix(w, h);
                gby = this->_pix + 0;
                aby = this->_pix + 1;
                break;
            case 0x4645: //EF (end of file)
                if (mxs != 0)
                    return FALSE; //EF always empty!
                EFfound = TRUE;
                break;
            case 0x4642: //BF (fixed border info)
                if (mxs != 8)
                    return FALSE; //wrong size
                fit = *(unsigned short*)tbs;
                tbs += 2;
                fir = *(unsigned short*)tbs;
                tbs += 2;
                fib = *(unsigned short*)tbs;
                tbs += 2;
                fil = *(unsigned short*)tbs;
                tbs += 2;
                if (fit + fib > h)
                    return FALSE; //fixed height is bigger then image height
                if (fir + fil > w)
                    return FALSE; //fixed width is bigger then image width
                break;
            case 0x4144: //DA (Alpha channel)
                tbd = aby;
                isAlpha = TRUE;
            case 0x4744: //DG (Grey channel)
                if (tbd == NULL)
                {
                    tbd = gby;
                    isAlpha = FALSE;
                }

                if (!SZfound)
                    return FALSE; //Error - SZ chunk must be first

                //for (int y = 0; y < w; y++)
                while (tbs < cend)
                {
                    int x = 0;
                    while (x < w)
                    {
                        int c = *tbs++;
                        if ((c & 0x80) == 0) // base data
                        {
                            c++; //vzdy o jedno vice
                            x += c;
                            for (int i = 0; i < c; i++)
                            {
                                *tbd = *tbs++;
                                tbd += 2;
                            }
                        }
                        else
                        {
                            if ((c & 0x40) == 0) // RLE
                            {
                                for (int i = 0; i <= (c & 0x3F); i++) //vzdy o jedno vice
                                {
                                    int dw = *tbs++;
                                    int t2 = *tbs++;
                                    dw++; //vzdy o jedno vice
                                    x += dw;
                                    for (int j = 0; j < dw; j++)
                                    {
                                        *tbd = (BYTE)t2;
                                        tbd += 2;
                                    }
                                }
                            }
                            else // Gradient RLE
                            {
                                int t1 = *tbs++;
                                int dw = 0;
                                int t2 = 0;
                                for (int i = 0; i <= (c & 0x3F); i++) //vzdy o jedno vice
                                {
                                    dw = *tbs++;
                                    t2 = *tbs++;
                                    dw++; //vzdy o jedno vice
                                    x += dw;
                                    for (int j = 0; j < dw; j++)
                                    {
                                        *tbd = (BYTE)((t1 * (dw - j) + t2 * j) / dw);
                                        tbd += 2;
                                    }
                                    t1 = t2;
                                }
                            }
                        }
                    }
                }
                if (tbs != cend)
                    return FALSE;
                if (isAlpha)
                {
                    aby = tbd;
                }
                else
                {
                    gby = tbd;
                }
                tbd = NULL;
                break;
            default:
                tbs += mxs;
                break;
            }

            if (EFfound)
                break; //End of file
        }
        if (!SZfound || !EFfound)
            return FALSE; //SZ & EF required
        this->_fixed_top = fit;
        this->_fixed_right = fir;
        this->_fixed_bottom = fib;
        this->_fixed_left = fil;
        return TRUE;
    }

    BOOL PremultiplyAlpha()
    {
        BYTE* tbs;
        BYTE alpha, value;

        if ((this->_pix == NULL) || (this->_pix == CCushionGraphics_defaultdata))
            return TRUE;

        tbs = this->_pix;
        for (int i = 0; i < this->_height; i++)
        {
            for (int j = 0; j < this->_width; j++)
            {
                value = *tbs;
                alpha = *(tbs + 1);
                *tbs++ = (BYTE)((alpha * value) >> 8);
                *tbs++ = 255 - alpha;
                //tbs += 2;
            }
        }
        return TRUE;
    }

private:
    __forceinline void DrawLinePartSimple(BYTE* dst, BYTE const* src, unsigned int const* atab, unsigned int width, int r, int g, int b)
    {
        unsigned int* pi = (unsigned int*)dst;

        unsigned int j;
        for (j = 0; j < width; j++)
        {
            unsigned int mask = *src++;
            unsigned int alpha = *src++;

            unsigned int const* a = atab + (alpha << 8);

            *pi++ = (a[b] | (a[g] << 8) | (a[r] << 16)) + (mask | (mask << 8) | (mask << 16));
        }
    }

    __forceinline void DrawLinePart(BYTE* dst, BYTE const* src, unsigned int const* atab, unsigned int sourcewidth, unsigned int width, int r, int g, int b)
    {
        unsigned int* pi = (unsigned int*)dst;

        unsigned int dx = (sourcewidth << 16) / width;
        unsigned int cx = 0;

        unsigned int j;
        for (j = 0; j < width; j++)
        {
            unsigned int mask = *src;
            unsigned int alpha = *(src + 1);

            unsigned int const* a = atab + (alpha << 8);

            *pi++ = (a[b] | (a[g] << 8) | (a[r] << 16)) + (mask | (mask << 8) | (mask << 16));

            cx += dx;
            src += (cx >> 16) << 1;
            cx &= 0xFFff;
        }
    }
    __forceinline void DrawLine(BYTE* dst, BYTE const* src, unsigned int const* atab, unsigned int sourcewidth, unsigned int fixed_left, unsigned int fixed_right, unsigned int width, int r, int g, int b)
    {
        //_mm_prefetch((char const *)src + sourcewidth, _MM_HINT_T0);
        unsigned int fixed_size = fixed_left + fixed_right;
        if (width <= fixed_size)
        {
            unsigned int leftpart = width * fixed_left / fixed_size;
            unsigned int rightpart = width * fixed_right / fixed_size;
            if (leftpart + rightpart < width)
            {
                if (leftpart < fixed_left)
                    leftpart++;
                else
                    rightpart++;
            }
            DrawLinePartSimple(dst, src, atab, leftpart, r, g, b);  //nakresli levy okraj
            dst += leftpart * 4;                                    // posun ve vysledku o to, co nakreslil (lp * 4bytes per pixel)
            src += (sourcewidth - rightpart) << 1;                  // posun zdroje na zacatek praveho okraje
            DrawLinePartSimple(dst, src, atab, rightpart, r, g, b); //nakresli pravy okraj
        }
        else
        {
            DrawLinePartSimple(dst, src, atab, fixed_left, r, g, b);
            src += fixed_left << 1;
            dst += fixed_left * 4;
            unsigned int sourcemiddle = sourcewidth - fixed_size; //melo by byt vzdy vetsi nez nula, ale compiler radsi, kdyz ohlidano?
            unsigned int cushionmiddle = width - fixed_size;
            //if (sourcemiddle >= 0)
            //__assume(sourcemiddle >= 0);
            {
                //__assume(sourcemiddle >= 0);
                //__assume(cushionmiddle >= 0);
                DrawLinePart(dst, src, atab, sourcemiddle, cushionmiddle, r, g, b);
                __assume(sourcemiddle >= 0); //nevim, proc to tady pomaha... :(
                src += sourcemiddle << 1;
            }
            //__assume(cushionmiddle >= 0);
            dst += cushionmiddle * 4;
            DrawLinePartSimple(dst, src, atab, fixed_right, r, g, b);
        }
    }

public:
    BOOL DrawCushion(BYTE* tBits, unsigned int pw, unsigned int ph, int cshx, int cshy, int cshw, int cshh, COLORREF color)
    {
        BYTE r, g, b;
        BYTE const* spa;
        int dy;
        int cy;

#ifdef _DEBUG
        if (cshx < 0)
            return FALSE;
        if (cshy < 0)
            return FALSE;
        if ((unsigned int)cshx >= pw)
            return FALSE;
        if ((unsigned int)cshy >= ph)
            return FALSE;

        if ((unsigned int)(cshx + cshw) > pw)
            cshw = pw - cshx;
        if ((unsigned int)(cshy + cshh) > ph)
            cshh = ph - cshy;
#endif

        if (cshw < 1)
            return FALSE;
        if (cshh < 1)
            return FALSE;

        r = GetRValue(color);
        g = GetGValue(color);
        b = GetBValue(color);

        //BYTE *tBits = p;

        if (cshh == 1 || cshw == 1 || this->_pix == NULL)
        {
            tBits += 4 * pw * cshy;
            tBits += 4 * cshx;
            for (int i = 0; i < cshh; i++)
            {
                for (int j = 0; j < cshw; j++)
                {
                    *tBits++ = b; //Blue
                    *tBits++ = g; //Green
                    *tBits++ = r; //Red
                    *tBits++ = 0; //XX
                }
                tBits += 4 * (pw - cshw);
            }
            //return TRUE;
        }
        else
        {
            int i; //for VC6
            spa = this->_pix;
            tBits += 4 * pw * cshy;
            tBits += 4 * cshx;
            if (cshh <= this->_fixed_top + this->_fixed_bottom)
            {
                int tp = cshh * this->_fixed_top / (this->_fixed_top + this->_fixed_bottom);
                int bp = cshh * this->_fixed_bottom / (this->_fixed_top + this->_fixed_bottom);
                if (tp + bp < cshh)
                {
                    if (tp < this->_fixed_top)
                        tp++;
                    else
                        bp++;
                }
                for (i = 0; i < tp; i++)
                {
                    DrawLine(tBits, spa, this->_alphaTable, this->_width, this->_fixed_left, this->_fixed_right, cshw, r, g, b);
                    tBits += 4 * pw;
                    spa += this->_width << 1;
                }
                spa = this->_pix + ((this->_width * (this->_height - bp)) << 1);
                for (i = 0; i < bp; i++)
                {
                    DrawLine(tBits, spa, this->_alphaTable, this->_width, this->_fixed_left, this->_fixed_right, cshw, r, g, b);
                    tBits += 4 * pw;
                    spa += this->_width << 1;
                }
            }
            else
            {
                for (i = 0; i < this->_fixed_top; i++)
                {
                    DrawLine(tBits, spa, this->_alphaTable, this->_width, this->_fixed_left, this->_fixed_right, cshw, r, g, b);
                    tBits += 4 * pw;
                    spa += this->_width << 1;
                }
                dy = ((this->_height - (this->_fixed_top + this->_fixed_bottom)) << 16) / (cshh - (this->_fixed_top + this->_fixed_bottom));
                cy = 0;
                for (i = 0; i < cshh - (this->_fixed_top + this->_fixed_bottom); i++)
                {
                    DrawLine(tBits, spa, this->_alphaTable, this->_width, this->_fixed_left, this->_fixed_right, cshw, r, g, b);
                    tBits += 4 * pw;
                    cy += dy;
                    spa += ((cy >> 16) * this->_width) << 1;
                    cy &= 0xFFFF;
                }
                spa = this->_pix + ((this->_width * (this->_height - this->_fixed_bottom)) << 1);
                for (i = 0; i < this->_fixed_bottom; i++)
                {
                    DrawLine(tBits, spa, this->_alphaTable, this->_width, this->_fixed_left, this->_fixed_right, cshw, r, g, b);
                    tBits += 4 * pw;
                    spa += this->_width << 1;
                }
            }
        }
        return TRUE;
    }
};
