// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MP4_SUPPORT_

#include "mp4parser.h"
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"
#include "mp4head.h"
#include "mp4tag.h"

char* LoadStr(int resID);
char* FStr(const char* format, ...);
void FormatSize2(__int64 size, char* str_size, BOOL nozero = FALSE);

CParserResultEnum
CParserMP4::OpenFile(const char* fileName)
{
    CloseFile();

    f = fopen(fileName, "rb");

    if (!f)
        return preOpenError;

    return preOK;
}

CParserResultEnum
CParserMP4::CloseFile()
{
    if (f)
    {
        fclose(f);
        f = NULL;
    }

    return preOK;
}

CParserResultEnum
CParserMP4::GetFileInfo(COutputInterface* output)
{
    AACHEAD_DECODED head;
    bool ret = DecodeAACHeader(f, &head);

    if (ret)
    {
        const char* object = "";
        const char* header = "";
        switch (head.object_type)
        {
        case 0:
            object = LoadStr(IDS_MP4_OBJTYPE_MAIN);
            break;
        case 1:
            object = LoadStr(IDS_MP4_OBJTYPE_LC);
            break;
        case 2:
            object = LoadStr(IDS_MP4_OBJTYPE_SSR);
            break;
        case 3:
            object = LoadStr(IDS_MP4_OBJTYPE_LTP);
            break;
        }

        switch (head.header_type)
        {
        case 0:
            header = LoadStr(IDS_MP4_HEADTYPE_RAW);
            break;
        case 1:
            header = LoadStr(IDS_MP4_HEADTYPE_ADIF);
            break;
        case 2:
            header = LoadStr(IDS_MP4_HEADTYPE_ADTS);
            break;
        }

        output->AddHeader(LoadStr(IDS_MP4_AACINFO));

        output->AddItem(LoadStr(IDS_MP4_MPEGVERSION), FStr("MPEG-%d (%s %s)", head.version, header, object));
        output->AddItem(LoadStr(IDS_MP4_BITRATE), FStr("%d", head.bitrate));

        char size[64];
        FormatSize2(head.sampling_rate, size);
        output->AddItem(LoadStr(IDS_MP4_SAMPLINGRATE), size);

        char str_time[64];
        DWORD length_sec = head.length / 1000;
        if (length_sec / 3600)
            lstrcpy(str_time, FStr("%02lu:%02lu:%02lu", length_sec / 3600, length_sec / 60 % 60, length_sec % 60));
        else
            lstrcpy(str_time, FStr("%02lu:%02lu", length_sec / 60 % 60, length_sec % 60));
        output->AddItem(LoadStr(IDS_MP4_DURATION), FStr("%d", str_time));

        output->AddItem(LoadStr(IDS_MP4_CHANNELS), FStr("%d", head.channels));

        return preOK;
    }

    return preUnknownFile;
}

#endif
