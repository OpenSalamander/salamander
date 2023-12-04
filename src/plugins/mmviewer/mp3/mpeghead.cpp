// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MPG_SUPPORT_

#include "mpeghead.h"

// See http://www.mpgedit.org/mpgedit/mpeg_format/MP3Format.html

WORD tabsel[3][3][16] =
    {
        {{0, 32, 64, 96, 128, 160, 192, 224, 256, 288, 320, 352, 384, 416, 448},
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384},
         {0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320}},
        {{0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}},
        {// NOTE: the tables are equal for MPEG2 and MPEG2.5
         {0, 32, 48, 56, 64, 80, 96, 112, 128, 144, 160, 176, 192, 224, 256},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160},
         {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}}};

WORD mpeg_freqs[9] = {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

BOOL HeaderCheck(DWORD head)
{
    if ((head & 0xffe00000) != 0xffe00000)
        return FALSE;
    if (!((head >> 17) & 3))
        return FALSE;
    if (((head >> 12) & 0xf) == 0xf)
        return FALSE;
    if (!((head >> 12) & 0xf))
        return FALSE;
    if (((head >> 10) & 0x3) == 0x3)
        return FALSE;
    if (((head >> 19) & 1) == 1 && ((head >> 17) & 3) == 3 && ((head >> 16) & 1) == 1)
        return FALSE;
    if ((head & 0xffff0000) == 0xfffe0000)
        return FALSE;

    return TRUE;
}

DWORD ScanForHeader(FILE* f, DWORD size)
{
    DWORD head;
    unsigned char tmp[4];

    if (fread(tmp, 1, sizeof(tmp), f) != sizeof(tmp))
        return NULL;

    head = ((DWORD)tmp[0] << 24) | ((DWORD)tmp[1] << 16) | ((DWORD)tmp[2] << 8) | (DWORD)tmp[3];

    while (!HeaderCheck(head))
    {
        head <<= 8;
        if (fread(tmp, 1, 1, f) != 1)
            return NULL;

        head |= tmp[0];
    }

    return head;
}

#define FRAMES_FLAG 0x0001
#define BYTES_FLAG 0x0002
#define TOC_FLAG 0x0004
#define VBR_SCALE_FLAG 0x0008

int CheckXingHeader_GetNumOfFrames(char inputheader[12])
{
    if (memcmp(inputheader, "Xing", 4))
        return -1;

    int flags = (int)(((inputheader[4] & 255) << 24) |
                      ((inputheader[5] & 255) << 16) |
                      ((inputheader[6] & 255) << 8) |
                      ((inputheader[7] & 255)));

    if (flags & FRAMES_FLAG)
    {
        return (int)(((inputheader[8] & 255) << 24) |
                     ((inputheader[9] & 255) << 16) |
                     ((inputheader[10] & 255) << 8) |
                     ((inputheader[11] & 255)));
    }

    return -1;
}

BOOL DecodeHeader(DWORD head, MPEGHEAD_DECODED* pmhd, FILE* f, DWORD filesize)
{
    if (!pmhd || !head || !f || !filesize)
        return FALSE;

    memset(pmhd, 0, sizeof(MPEGHEAD_DECODED));
    /*
MPEG Audio version ID
00 - MPEG Version 2.5 (later extension of MPEG 2)
01 - reserved
10 - MPEG Version 2 (ISO/IEC 13818-3)
11 - MPEG Version 1 (ISO/IEC 11172-3)
*/

    int v = ((head >> 19) & 3);

    switch (v)
    {
    case 0: //2.5
        pmhd->lsf = 2;
        pmhd->mpeg = MAKEWORD(5, 2);
        break;

    case 2: //2
        pmhd->lsf = 1;
        pmhd->mpeg = MAKEWORD(0, 2);
        break;

    case 1: //1
    case 3: //1
        pmhd->lsf = 0;
        pmhd->mpeg = MAKEWORD(0, 1);
        break;
    }

    pmhd->sampling_frequency_index = (short)((pmhd->lsf * 3) + ((head >> 10) & 0x3));

    pmhd->layer = (short)(4 - ((head >> 17) & 3));

    pmhd->error_protection = (short)(((head >> 16) & 0x1) ^ 0x1);

    pmhd->bitrate_index = (short)((head >> 12) & 0xf);
    pmhd->padding = (short)((head >> 9) & 0x1);
    pmhd->extension = (short)((head >> 8) & 0x1);
    pmhd->mode = (short)((head >> 6) & 0x3);
    pmhd->mode_ext = (short)((head >> 4) & 0x3);
    pmhd->copyright = (short)((head >> 3) & 0x1);
    pmhd->original = (short)((head >> 2) & 0x1);
    pmhd->emphasis = (short)(head & 0x3);

    if (!pmhd->bitrate_index)
        return FALSE;

    switch (pmhd->layer)
    {
    case 1:
        pmhd->kbps = (int)tabsel[pmhd->lsf][0][pmhd->bitrate_index];
        pmhd->hz = mpeg_freqs[pmhd->sampling_frequency_index];
        pmhd->framesize = pmhd->kbps * 12000;
        pmhd->framesize /= pmhd->hz;
        pmhd->framesize = ((pmhd->framesize + pmhd->padding) << 2); // - 4;
        break;

    case 2:
        pmhd->kbps = (int)tabsel[pmhd->lsf][1][pmhd->bitrate_index];
        pmhd->hz = mpeg_freqs[pmhd->sampling_frequency_index];
        pmhd->framesize = pmhd->kbps * 144000;
        pmhd->framesize /= pmhd->hz;
        pmhd->framesize += pmhd->padding; // - 4;
        break;

    case 3:
        pmhd->kbps = (int)tabsel[pmhd->lsf][2][pmhd->bitrate_index];
        // Patera 2008.12.05: Do not shift by lsf bits; lsf*4 has already been added
        pmhd->hz = mpeg_freqs[pmhd->sampling_frequency_index] /*<< (pmhd->lsf)*/;
        pmhd->framesize = pmhd->kbps * 144000;
        pmhd->framesize /= pmhd->hz;
        pmhd->framesize += pmhd->padding; // - 4;
        break;

    default:
        return FALSE;
    }

    pmhd->headpos = ftell(f) - 4;

    filesize -= pmhd->headpos;

    if (pmhd->framesize)
        pmhd->frames = filesize / pmhd->framesize;
    else
        pmhd->frames = 0;

    //kontrola variable bitrate
    char xinghead[12];
    int pos = 0;

    //zjisti pozici xing-headeru
    if ((pmhd->mpeg == 0x0200) || (pmhd->mpeg == 0x0205))
    {
        if (pmhd->mode == 3)
            pos += 9; // Single Channel
        else
            pos += 17;
    }
    else
    {
        if (pmhd->mode == 3)
            pos += 17; // Single Channel
        else
            pos += 32;
    }

    fseek(f, pos, SEEK_CUR);

    if (fread(xinghead, 1, 12, f) == 12)
    {
        pos = CheckXingHeader_GetNumOfFrames(xinghead); //pos reuse :-)

        pmhd->variable_bitrate = (pos != -1);

        if (pmhd->variable_bitrate)
        {
            pmhd->frames = pos;

            if (pmhd->frames)
            {
                pmhd->framesize = filesize / pmhd->frames;
                pmhd->kbps = int(((double(pmhd->framesize) * double(pmhd->hz)) / (((pmhd->layer == 1) ? 12000.0 : 144000.0))));
            }
            else
                pmhd->kbps = 0;
        }
    }

    pmhd->length_msec = (pmhd->kbps) ? (((8 * (filesize))) / pmhd->kbps) : 0;

    return TRUE;
}

#endif