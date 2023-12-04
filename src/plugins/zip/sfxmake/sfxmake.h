// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define TITLELEN 0
#define TEXTLEN 1
#define ABOUTLICENCEDLEN 2
#define BUTTONTEXTLEN 3
#define VENDORLEN 4
#define WWWLEN 5
#define LENARRAYSIZE 6

#define SFX_SIGNATURE 0x01584654

//#define CURRENT_SFX_VERSION 1 //sfx package version

#pragma pack(push)
#pragma pack(4)

struct CSfxFileHeader
{
    DWORD Signature;
    DWORD CompatibleVersion;
    DWORD CurrentVersion;
    DWORD LangID;
    DWORD TextLen[LENARRAYSIZE];
    DWORD TotalTextSize;
    DWORD SmallSfxOffset;
    DWORD SmallSfxSize;
    DWORD SmallSfxCRC;
    DWORD BigSfxOffset;
    DWORD BigSfxSize;
    DWORD BigSfxCRC;
    DWORD TextsCRC;
    DWORD HeaderCRC;
};

#pragma pack(pop)
