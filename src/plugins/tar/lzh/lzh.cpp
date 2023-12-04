// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "../dlldefs.h"
#include "../fileio.h"
#include "lzh.h"

#include "..\tar.rh"
#include "..\tar.rh2"
#include "..\lang\lang.rh"

// class constructor
CLZH::CLZH(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read) : CZippedFile(filename, file, buffer, 0, read, CQuadWord(0, 0))
{
    CALL_STACK_MESSAGE2("CLZH::CLZH(%s, , , )", filename);

    // pokud neprosel konstruktor parenta, balime to rovnou
    if (!Ok)
        return;

    // nemuze to byt compress archiv, pokud mame min nez identifikaci...
    if (DataEnd - DataStart < 3)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // pokud neni spravne "magicke cislo" na zacatku, nejde o LZH
    if (((unsigned short*)DataStart)[0] != LZH_MAGIC)
    {
        Ok = FALSE;
        FreeBufAndFile = FALSE;
        return;
    }
    // mame archiv, potvrdime precteny zacatek
    FReadBlock(2);
    if (!Ok)
        FreeBufAndFile = FALSE;
}

// class cleanup
CLZH::~CLZH()
{
    CALL_STACK_MESSAGE1("CLZH::~CLZH()");
}

BOOL CLZH::DecompressBlock(unsigned short needed)
{
    Ok = FALSE;
    ErrorCode = IDS_LZH_NOT_IMPLEMENTED;
    return FALSE;
}
