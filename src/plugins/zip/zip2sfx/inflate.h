// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//PKZIP 1.93a problem--live with it
#define PKZIP_BUG_WORKAROUND

struct huft
{
    unsigned char e; /* number of extra bits or operation */
    unsigned char b; /* number of bits in this code or subcode */
    union
    {
        unsigned short n; /* literal, length base, or distance base */
        struct huft* t;   /* pointer to next level of table */
    } v;
};

#define WSIZE 0x8000

extern unsigned char* SlideWin;
extern unsigned char* InPtr;
extern unsigned char* InEnd;
extern DWORD UCSize;
extern DWORD Crc;

int Inflate();
int FreeFixedHufman();
