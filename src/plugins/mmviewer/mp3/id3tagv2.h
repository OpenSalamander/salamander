// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(1)
typedef struct
{
    char id[3];   //'ID3'
    BYTE ver;     //Version number
    BYTE rev;     //Revision number
    BYTE flags;   //Flags of tag
    BYTE size[4]; //Tag size excluding header
} ID3TAGV2_HEADER;

typedef struct
{
    WORD version;
    WORD reserved;
    long tagsize;
    union
    {
        struct
        {
            char* title;
            char* artist;
            char* album;
            char* year;
            char* comments;
            char* genre;
            char* track;
            char* copyright;
            char* encodedby;
            char* composer;
            char* original_artist;
            char* publisher;
            char* url_cominfo;     //The 'Commercial information' frame is a URL pointing at a webpage with information such as where the album can be bought. There may be more than one "WCOM" frame in a tag, but not with the same content.
            char* url_copyright;   //The 'Copyright/Legal information' frame is a URL pointing at a webpage where the terms of use and ownership of the file is described.
            char* url_official;    //The 'Official audio file webpage' frame is a URL pointing at a file specific webpage.
            char* url_artist;      //The 'Official artist/performer webpage' frame is a URL pointing at the artists official webpage. There may be more than one "WOAR" frame in a tag if the audio contains more than one performer, but not with the same content.
            char* url_audiosource; //The 'Official audio source webpage' frame is a URL pointing at the official webpage for the source of the audio file, e.g. a movie.
            char* url_iradio;      //The 'Official internet radio station homepage' contains a URL pointing at the homepage of the internet radio station.
            char* url_payment;     //The 'Payment' frame is a URL pointing at a webpage that will handle the process of paying for this file.
            char* url_publisher;   //The 'Publishers official webpage' frame is a URL pointing at the official wepage for the publisher
            char* url_userdefined;
            char* text_userdefined;
        };

        struct
        {
            char* items[22];
        };
    };

} ID3TAGV2_DECODED;
#pragma pack()

DWORD ConvertInt28(const BYTE size[4]);
DWORD ConvertBENumber(const BYTE size[4]);

#define CONVERT_C2DW(b4) ConvertInt28(b4)
#define CONVERT_C2DW4(b4) (DWORD(b4[0]) * 0x1000000 + DWORD(b4[1]) * 0x10000 + DWORD(b4[2]) * 0x100 + DWORD(b4[3])) //je mozne pouzit take convert BE number
#define CONVERT_C2DW3(b3) (DWORD(b3[0]) * 0x10000 + DWORD(b3[1]) * 0x100 + DWORD(b3[2]))

BOOL ID3TAGV2_ReadMainHead(FILE* f, ID3TAGV2_HEADER* pmh);
BOOL ID3TAGV2_Read(FILE* f, const ID3TAGV2_HEADER* pmh, ID3TAGV2_DECODED* phd);
BOOL ID3TAGV2_Free(ID3TAGV2_DECODED* phd);
