// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/* This is a dual file, include both in PictView.spl and PV Envelope.
   The envelope is built with #define BUILD_ENVELOPE.
 */
#include "precomp.h"

#include "lib/pvw32dll.h"

#include "pictview.h"
#include "PixelAccess.h"

#pragma runtime_checks("", off)
// false RTC error for PV_COLOR_HC16 mode - see https://forum.altap.cz/viewtopic.php?f=16&t=5577
bool GetRGBAtCursor(LPPVHandle PVHandle, DWORD Colors, int x, int y, RGBQUAD* pRGB, int* pIndex)
{
    LPPVImageHandles pHandles;
    int ind = 0;

#ifdef BUILD_ENVELOPE
    if ((PVC_OK == PVGetHandles2(PVHandle, &pHandles)) && pHandles->pLines)
    {
#else
    if ((PVC_OK == PVW32DLL.PVGetHandles2(PVHandle, &pHandles)) && pHandles->pLines)
    {
#endif
        BYTE* pLine = pHandles->pLines[y];
        RGBQUAD rgb;

        if (Colors >= PV_COLOR_TC24)
        {
            rgb.rgbBlue = pLine[x * 3];
            rgb.rgbGreen = pLine[x * 3 + 1];
            rgb.rgbRed = pLine[x * 3 + 2];
        }
        else if (Colors == PV_COLOR_HC16)
        {
            rgb.rgbBlue = pLine[x * 2] << 3;
            rgb.rgbGreen = ((pLine[x * 2] & 0xE0) >> 3) | (pLine[x * 2 + 1] << 5);
            rgb.rgbRed = pLine[x * 2 + 1] & 0xF8;
        }
        else if (Colors == PV_COLOR_HC15)
        {
            rgb.rgbBlue = pLine[x * 2] << 3;
            rgb.rgbGreen = ((pLine[x * 2] & 0xE0) >> 2) | (pLine[x * 2 + 1] << 6);
            rgb.rgbRed = (pLine[x * 2 + 1] << 1) & 0xF8;
        }
        else if (Colors > 16)
        {
            ind = pLine[x];
            rgb = pHandles->Palette[ind];
        }
        else if (Colors > 2)
        {
            ind = x >> 1;
            ind = (x & 1) ? (pLine[ind] & 0x0F) : (pLine[ind] >> 4);
            rgb = pHandles->Palette[ind];
        }
        else
        {
            ind = x >> 3;
            ind = (pLine[ind] & (0x80 >> (x & 7))) ? 1 : 0;
            rgb = pHandles->Palette[ind];
        }
        *pIndex = ind;
        *pRGB = rgb;
        return true;
    }
    return false;
}
#pragma runtime_checks("", restore)

PVCODE CalculateHistogram(LPPVHandle PVHandle, const LPPVImageInfo pvii, LPDWORD luminosity, LPDWORD red, LPDWORD green, LPDWORD blue, LPDWORD rgb)
{
    LPPVImageHandles pHandles;
    PVCODE ret;
    DWORD i, j;
    int k;
    BYTE* pLine;
    RGBQUAD* pPalette;
    DWORD tmp[256];

    memset(luminosity, 0, sizeof(DWORD) * 256);
    memset(red, 0, sizeof(DWORD) * 256);
    memset(green, 0, sizeof(DWORD) * 256);
    memset(blue, 0, sizeof(DWORD) * 256);
    memset(rgb, 0, sizeof(DWORD) * 256);
    memset(tmp, 0, sizeof(DWORD) * 256);

#ifdef BUILD_ENVELOPE
    if (PVC_OK != (ret = PVGetHandles2(PVHandle, &pHandles)))
    {
#else
    if (PVC_OK != (ret = PVW32DLL.PVGetHandles2(PVHandle, &pHandles)))
    {
#endif
        return ret;
    }
    pPalette = pHandles->Palette;
    // Check if the image is paletted and we succeed in getting the palette pointer
    if (((pvii->Colors <= 256) && !pPalette) || !pHandles->pLines)
    {
        // What's wrong? Can this ever happen?
        return PVC_UNSUP_COLOR_DEPTH;
    }
    if (pvii->Colors <= 256)
    {
        for (i = 0; i < pvii->Height; i++)
        {
            // do it in bytes
            // optimized for maximum speed for bilevel
            pLine = pHandles->pLines[i];
            if ((pvii->Colors <= 16) && (pvii->Colors > 2))
            {
                for (j = 0; j < pvii->BytesPerLine; j++)
                {
                    tmp[*pLine & 15]++;
                    tmp[*(pLine++) >> 4]++;
                }
            }
            else
            {
                for (j = 0; j < (int)pvii->BytesPerLine; j++)
                {
                    tmp[*(pLine++)]++;
                }
            }
        }
        if (pvii->Colors == 2)
        {
            DWORD zeros, ones;

            zeros = ones = 0;
            for (i = 0; i < 256; i++)
            {
                for (j = 1, k = 0; j < 256; j <<= 1)
                {
                    if (i & j)
                        k++;
                }
                zeros += tmp[i] * (8 - k); // # of zero bits
                ones += tmp[i] * k;        // # of set bits
            }
            i = pvii->Width % 8;
            if (i)
            {
                // removed padding bits
                zeros -= pvii->Height * (8 - i);
            }
            tmp[0] = zeros;
            tmp[1] = ones;
        }
        else if ((pvii->Colors <= 16) && (pvii->Width & 1))
        {
            // removed padding bits for 4-bit images
            tmp[0] -= pvii->Height;
        }
        for (i = 0; i < pvii->Colors; i++)
        {
            red[pPalette[i].rgbRed] += tmp[i];
            green[pPalette[i].rgbGreen] += tmp[i];
            blue[pPalette[i].rgbBlue] += tmp[i];
            rgb[pPalette[i].rgbRed] += tmp[i];
            rgb[pPalette[i].rgbGreen] += tmp[i];
            rgb[pPalette[i].rgbBlue] += tmp[i];
        }
        for (i = 0; i < pvii->Colors; i++)
        {
            j = (299 * ((DWORD)pPalette[i].rgbRed) + 587 * ((DWORD)pPalette[i].rgbGreen) + 114 * ((DWORD)pPalette[i].rgbBlue)) / 1000;
            luminosity[j] += tmp[i];
        }
        for (i = 0; i < 256; i++)
            rgb[i] /= 3;
        return PVC_OK;
    }
    for (i = 0; i < pvii->Height; i++)
    {
        pLine = pHandles->pLines[i];
        for (j = 0; j < pvii->Width; j++)
        {
            int r, g, b;

            switch (pvii->Colors)
            {
            case PV_COLOR_HC15:
                k = *(USHORT*)pLine;
                pLine += 2;
                r = (k & 31) << 3;
                g = (k >> 2) & 0xF8;
                b = (k >> 7) & 0xF8;
                break;
            case PV_COLOR_HC16:
                k = *(USHORT*)pLine;
                pLine += 2;
                r = (k & 31) << 3;
                g = (k >> 3) & 0xFC;
                b = (k >> 8) & 0xF8;
                break;
            default: // 24bit, originally also 32bit
                b = *pLine++;
                g = *pLine++;
                r = *pLine++;
            }
            red[r]++;
            green[g]++;
            blue[b]++;
            k = (299 * r + 587 * g + 114 * b) / 1000;
            luminosity[k]++;
            rgb[r]++; // "RGB" levels z Photoshopu
            rgb[g]++;
            rgb[b]++;
        }
    }
    for (i = 0; i < 256; i++)
        rgb[i] /= 3;
    return PVC_OK;
}
