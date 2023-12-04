// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <commctrl.h>

#include "config.h"
#include "zip.rh2"
#include "common.h"
#include "inflate.h" // CDecompressionObject

int UnBZIP2(CDecompressionObject* decompress) /* expand probabilistically reduced data */
{
    CSalBZIP2 bzi;
    int err = SAL_BZ_OK;
    int ret = 0;

    if (SAL_BZ_OK != SalamanderBZIP2->DecompressInit(&bzi, false))
    {
        return 3;
    }
    bzi.next_out = (BYTE*)decompress->Output->SlideWin;
    bzi.avail_out = decompress->Output->WinSize;
    bzi.avail_in = 0;
    do
    {
        if (!bzi.avail_in)
        {
            decompress->Input->Refill(decompress);
            if (decompress->Input->Error)
            {
                ret = 1;
                break;
            }
            bzi.avail_in = decompress->Input->BytesLeft;
            bzi.next_in = (BYTE*)decompress->Input->NextByte;
        }

        err = SalamanderBZIP2->Decompress(&bzi);
        if (!bzi.avail_out)
        {
            if (decompress->Output->Flush(decompress->Output->WinSize, decompress))
            {
                ret = 2;
                break;
            }
            bzi.next_out = (BYTE*)decompress->Output->SlideWin;
            bzi.avail_out = decompress->Output->WinSize;
        }
    } while (err == SAL_BZ_OK);

    decompress->Output->Flush(decompress->Output->WinSize - bzi.avail_out, decompress);
    SalamanderBZIP2->DecompressEnd(&bzi);

    if (err != SAL_BZ_STREAM_END)
    {
        return 4;
    }
    return ret;
}
