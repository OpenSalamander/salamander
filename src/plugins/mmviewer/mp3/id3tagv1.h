// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef struct
{
    char tag[3];
    char title[30];
    char artist[30];
    char album[30];
    char year[4];
    char comments[30];
    BYTE genre;
} ID3TAGV1;

typedef struct
{
    WORD version;
    char title[31];
    char artist[31];
    char album[31];
    char year[5];
    char comments[31];
    char genre[64];
    BYTE track;
} ID3TAGV1_DECODED;

const char* ID3TAGV1_GetGenreStr(BYTE genre);

BOOL ID3TAGV1_Read(FILE* f, ID3TAGV1* ph);
BOOL ID3TAGV1_Decode(ID3TAGV1* ph, ID3TAGV1_DECODED* phd); //pozor, modifikuje (kvuli uspore) i vstupni 'ph'
