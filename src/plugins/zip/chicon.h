// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

/****************************************************************************/
// Structs

// These first two structs represent how the icon information is stored
// when it is bound into a EXE or DLL file. Structure members are WORD
// aligned and the last member of the structure is the ID instead of
// the imageoffset.
#pragma pack(push)
#pragma pack(2)
typedef struct
{
    BYTE bWidth;        // Width of the image
    BYTE bHeight;       // Height of the image (times 2)
    BYTE bColorCount;   // Number of colors in image (0 if >=8bpp)
    BYTE bReserved;     // Reserved
    WORD wPlanes;       // Color Planes
    WORD wBitCount;     // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    WORD nID;           // the ID
} MEMICONDIRENTRY, *LPMEMICONDIRENTRY;

typedef struct
{
    WORD idReserved;              // Reserved
    WORD idType;                  // resource type (1 for icons)
    WORD idCount;                 // how many images?
    MEMICONDIRENTRY idEntries[1]; // the entries for each image
} MEMICONDIR, *LPMEMICONDIR;
#pragma pack(pop)

// These next two structs represent how the icon information is stored
// in an ICO file.
typedef struct
{
    BYTE bWidth;         // Width of the image
    BYTE bHeight;        // Height of the image (times 2)
    BYTE bColorCount;    // Number of colors in image (0 if >=8bpp)
    BYTE bReserved;      // Reserved
    WORD wPlanes;        // Color Planes
    WORD wBitCount;      // Bits per pixel
    DWORD dwBytesInRes;  // how many bytes in this resource?
    DWORD dwImageOffset; // where in the file is this image
} ICONDIRENTRY, *LPICONDIRENTRY;

typedef struct
{
    WORD idReserved;           // Reserved
    WORD idType;               // resource type (1 for icons)
    WORD idCount;              // how many images?
    ICONDIRENTRY idEntries[1]; // the entries for each image
} ICONDIR, *LPICONDIR;

typedef struct
{
    BYTE bWidth;        // Width of the image
    BYTE bHeight;       // Height of the image (times 2)
    BYTE bColorCount;   // Number of colors in image (0 if >=8bpp)
    BYTE bReserved;     // Reserved
    WORD wPlanes;       // Color Planes
    WORD wBitCount;     // Bits per pixel
    DWORD dwBytesInRes; // how many bytes in this resource?
    LPVOID lpData;      // icon data
} CIcon;

BOOL ChangeSfxIconAndAddManifest(const char* sfxFile, CIcon* icons, int iconsCount, LPVOID manifest, DWORD manifestSize);
int LoadIcons(const char* iconFile, DWORD index, CIcon** icons, int* count);
void DestroyIcons(CIcon* icons, int count);
