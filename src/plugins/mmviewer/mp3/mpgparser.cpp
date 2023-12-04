// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#ifdef _MPG_SUPPORT_

#include "mpgparser.h"
#include "..\mmviewer.rh"
#include "..\mmviewer.rh2"
#include "..\lang\lang.rh"
#include "..\output.h"
#include "mpeghead.h"
#include "id3tagv1.h"
#include "id3tagv2.h"

struct MPEG_INFO : public MPEGHEAD_DECODED
{
public:
    char str_emphasis[64];
    char str_mode[64];
    char str_time[64];
};

char* LoadStr(int resID);
char* FStr(const char* format, ...);
void FormatSize2(__int64 size, char* str_size, BOOL nozero = FALSE);
void chkstrcpy(char* out, const char* in, size_t maxoutsize)
{
    strncpy_s(out, maxoutsize, in, _TRUNCATE);
}

CParserResultEnum
CParserMPG::OpenFile(const char* fileName)
{
    CloseFile();

    f = fopen(fileName, "rb");

    if (!f)
        return preOpenError;

    return preOK;
}

CParserResultEnum
CParserMPG::CloseFile()
{
    if (f)
    {
        fclose(f);
        f = NULL;
    }

    return preOK;
}

CParserResultEnum
CParserMPG::GetFileInfo(COutputInterface* output)
{
    if (f)
    {
        DWORD header;
        MPEG_INFO mpeginfo;
        ID3TAGV1 id3v1;
        ID3TAGV1_DECODED id3v1dec;
        ID3TAGV2_HEADER id3v2head;
        ID3TAGV2_DECODED id3v2dec;
        DWORD filesize;

        BOOL id3tagv1found = FALSE;
        BOOL id3tagv2found = FALSE;

        int usetag = 0; //program rozhodne jaky tag pouzit (1 nebo 2)

        memset(&mpeginfo, 0, sizeof(mpeginfo));

        //zjisti velikost souboru
        fseek(f, 0, SEEK_END);
        filesize = ftell(f);
        fseek(f, 0, SEEK_SET);

        if (ID3TAGV2_ReadMainHead(f, &id3v2head))
        {
            if (ID3TAGV2_Read(f, &id3v2head, &id3v2dec))
            {
                id3tagv2found = TRUE;
            }

            fseek(f, sizeof(ID3TAGV2_HEADER) + CONVERT_C2DW(id3v2head.size), SEEK_SET); //bezpecne preskoc veskera id3v2 data
        }

        if ((header = ScanForHeader(f, filesize)) != NULL)
        {
            //header found
            if (DecodeHeader(header, &mpeginfo, f, filesize))
            {
                switch (mpeginfo.emphasis)
                {
                case 0:
                    chkstrcpy(mpeginfo.str_emphasis, LoadStr(IDS_MP3_DASH), sizeof(mpeginfo.str_emphasis));
                    break;
                case 1:
                    chkstrcpy(mpeginfo.str_emphasis, LoadStr(IDS_MP3_5015MS), sizeof(mpeginfo.str_emphasis));
                    break;
                case 3:
                    chkstrcpy(mpeginfo.str_emphasis, LoadStr(IDS_MP3_CCITTJ17), sizeof(mpeginfo.str_emphasis));
                    break;
                default:
                    chkstrcpy(mpeginfo.str_emphasis, LoadStr(IDS_MP3_QUESTIONMARK), sizeof(mpeginfo.str_emphasis));
                    break;
                }

                switch (mpeginfo.mode)
                {
                case 0:
                    chkstrcpy(mpeginfo.str_mode, LoadStr(IDS_MP3_STEREO), sizeof(mpeginfo.str_mode));
                    break;
                case 1:
                    chkstrcpy(mpeginfo.str_mode, LoadStr(IDS_MP3_JSTEREO), sizeof(mpeginfo.str_mode));
                    break;
                case 2:
                    chkstrcpy(mpeginfo.str_mode, LoadStr(IDS_MP3_DCHANNEL), sizeof(mpeginfo.str_mode));
                    break;
                case 3:
                    chkstrcpy(mpeginfo.str_mode, LoadStr(IDS_MP3_SCHANNEL), sizeof(mpeginfo.str_mode));
                    break;
                }

                DWORD length_sec = mpeginfo.length_msec / 1000;
                if (length_sec / 3600)
                    lstrcpy(mpeginfo.str_time, FStr("%02lu:%02lu:%02lu", length_sec / 3600, length_sec / 60 % 60, length_sec % 60));
                else
                    lstrcpy(mpeginfo.str_time, FStr("%02lu:%02lu", length_sec / 60 % 60, length_sec % 60));
            }
        }
        else
        {
            if (id3tagv2found)
                ID3TAGV2_Free(&id3v2dec);
            return preUnknownFile;
        }

        //Precti ID3TAGV1

        fseek(f, -signed(sizeof(ID3TAGV1)), SEEK_END);
        if (ID3TAGV1_Read(f, &id3v1))
        {
            if (ID3TAGV1_Decode(&id3v1, &id3v1dec))
            {
                id3tagv1found = TRUE;
            }
        }

        //dump
        char tmp[32], size[64];
        ZeroMemory(tmp, sizeof(tmp));
        int l;
        for (l = 0; l < mpeginfo.layer; l++)
            tmp[l] = 'I';

        output->AddHeader(LoadStr(IDS_MP3_MP3INFO));
        output->AddItem(LoadStr(IDS_MP3_MP3VERSION), FStr(LoadStr(IDS_MP3_MP3VERSIONFMT), HIBYTE(mpeginfo.mpeg), LOBYTE(mpeginfo.mpeg), tmp));
        output->AddItem(LoadStr(IDS_MP3_BITRATE), FStr("%s%u", mpeginfo.variable_bitrate ? LoadStr(IDS_MP3_AVERAGE) : "", mpeginfo.kbps));
        FormatSize2(mpeginfo.hz, size);
        output->AddItem(LoadStr(IDS_MP3_FREQUENCY), size);
        FormatSize2(mpeginfo.frames, size);
        output->AddItem(LoadStr(IDS_MP3_FRAMES), size);
        output->AddItem(LoadStr(IDS_MP3_LENGTH), mpeginfo.str_time);
        output->AddItem(LoadStr(IDS_MP3_MODE), mpeginfo.str_mode);
        output->AddItem(LoadStr(IDS_MP3_EMPHASIS), mpeginfo.str_emphasis);
        output->AddItem(LoadStr(IDS_MP3_COPYRIGHT), mpeginfo.copyright ? LoadStr(IDS_YES) : LoadStr(IDS_NO));
        output->AddItem(LoadStr(IDS_MP3_ORIGINAL), mpeginfo.original ? LoadStr(IDS_YES) : LoadStr(IDS_NO));

#define TAGITEM_AVAIL(tagitem) (tagitem && tagitem[0])

        if (id3tagv1found)
        {
            output->AddSeparator();

            output->AddHeader(FStr(LoadStr(IDS_MP3_ID3TAGV), HIBYTE(id3v1dec.version), LOBYTE(id3v1dec.version)));

            if (TAGITEM_AVAIL(id3v1dec.title))
                output->AddItem(LoadStr(IDS_MP3_TITLE), id3v1dec.title);
            if (TAGITEM_AVAIL(id3v1dec.artist))
                output->AddItem(LoadStr(IDS_MP3_AUTHOR), id3v1dec.artist);
            if (TAGITEM_AVAIL(id3v1dec.album))
                output->AddItem(LoadStr(IDS_MP3_ALBUM), id3v1dec.album);
            if (TAGITEM_AVAIL(id3v1dec.year))
                output->AddItem(LoadStr(IDS_MP3_YEAR), id3v1dec.year);
            if (TAGITEM_AVAIL(id3v1dec.genre))
                output->AddItem(LoadStr(IDS_MP3_GENRE), id3v1dec.genre);
            if (LOBYTE(id3v1dec.version) == 1)
                output->AddItem(LoadStr(IDS_MP3_TRACK), FStr("%u", id3v1dec.track));
            if (TAGITEM_AVAIL(id3v1dec.comments))
                output->AddItem(LoadStr(IDS_MP3_COMMENTS), id3v1dec.comments);
        }

        if (id3tagv2found)
        {
            output->AddSeparator();

            output->AddHeader(FStr(LoadStr(IDS_MP3_ID3TAGV), HIBYTE(id3v2dec.version), LOBYTE(id3v2dec.version)));

            if (TAGITEM_AVAIL(id3v2dec.title))
                output->AddItem(LoadStr(IDS_MP3_TITLE), id3v2dec.title);
            if (TAGITEM_AVAIL(id3v2dec.artist))
                output->AddItem(LoadStr(IDS_MP3_AUTHOR), id3v2dec.artist);
            if (TAGITEM_AVAIL(id3v2dec.album))
                output->AddItem(LoadStr(IDS_MP3_ALBUM), id3v2dec.album);
            if (TAGITEM_AVAIL(id3v2dec.year))
                output->AddItem(LoadStr(IDS_MP3_YEAR), id3v2dec.year);
            if (TAGITEM_AVAIL(id3v2dec.genre))
                output->AddItem(LoadStr(IDS_MP3_GENRE), id3v2dec.genre);
            if (TAGITEM_AVAIL(id3v2dec.track))
                output->AddItem(LoadStr(IDS_MP3_TRACK), id3v2dec.track);
            if (TAGITEM_AVAIL(id3v2dec.comments))
                output->AddItem(LoadStr(IDS_MP3_COMMENTS), id3v2dec.comments);

            if (TAGITEM_AVAIL(id3v2dec.composer))
                output->AddItem(LoadStr(IDS_MP3_COMPOSER), id3v2dec.composer);
            if (TAGITEM_AVAIL(id3v2dec.original_artist))
                output->AddItem(LoadStr(IDS_MP3_ORIGARTIST), id3v2dec.original_artist);
            if (TAGITEM_AVAIL(id3v2dec.publisher))
                output->AddItem(LoadStr(IDS_MP3_PUBLISHER), id3v2dec.publisher);
            if (TAGITEM_AVAIL(id3v2dec.copyright))
                output->AddItem(LoadStr(IDS_MP3_COPYRIGHT), id3v2dec.copyright);
            if (TAGITEM_AVAIL(id3v2dec.encodedby))
                output->AddItem(LoadStr(IDS_MP3_ENCODEDBY), id3v2dec.encodedby);

            if (TAGITEM_AVAIL(id3v2dec.text_userdefined))
                output->AddItem(LoadStr(IDS_MP3_USERTEXT), id3v2dec.text_userdefined);

            if (TAGITEM_AVAIL(id3v2dec.url_cominfo))
                output->AddItem(LoadStr(IDS_MP3_URL_COMINFO), id3v2dec.url_cominfo);
            if (TAGITEM_AVAIL(id3v2dec.url_copyright))
                output->AddItem(LoadStr(IDS_MP3_URL_COPYRIGHT), id3v2dec.url_copyright);
            if (TAGITEM_AVAIL(id3v2dec.url_official))
                output->AddItem(LoadStr(IDS_MP3_URL_OFFICIAL), id3v2dec.url_official);
            if (TAGITEM_AVAIL(id3v2dec.url_artist))
                output->AddItem(LoadStr(IDS_MP3_URL_ARTIST), id3v2dec.url_artist);
            if (TAGITEM_AVAIL(id3v2dec.url_audiosource))
                output->AddItem(LoadStr(IDS_MP3_URL_ASOURCE), id3v2dec.url_audiosource);
            if (TAGITEM_AVAIL(id3v2dec.url_iradio))
                output->AddItem(LoadStr(IDS_MP3_URL_IRADIO), id3v2dec.url_iradio);
            if (TAGITEM_AVAIL(id3v2dec.url_payment))
                output->AddItem(LoadStr(IDS_MP3_URL_PAYMENT), id3v2dec.url_payment);
            if (TAGITEM_AVAIL(id3v2dec.url_publisher))
                output->AddItem(LoadStr(IDS_MP3_URL_PUBLISHER), id3v2dec.url_publisher);
            if (TAGITEM_AVAIL(id3v2dec.url_userdefined))
                output->AddItem(LoadStr(IDS_MP3_USERURL), id3v2dec.url_userdefined);

            ID3TAGV2_Free(&id3v2dec);
        }

#undef TAGITEM_AVAIL

        return preOK;
    }

    return preUnknownFile;
}

#endif
