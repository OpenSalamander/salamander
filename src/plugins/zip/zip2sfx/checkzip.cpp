// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
//#include <windows.h>
#include <crtdbg.h>
#include <stdio.h>

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "selfextr\\comdefs.h"
#include "typecons.h"
#include "checkzip.h"
#include "sfxmake\\sfxmake.h"
#include "chicon.h"
#include "zip2sfx.h"

BOOL CheckEntries(DWORD dirOffs, DWORD totalEntries)
{
    DWORD offs = dirOffs;
    CFileHeader header;
    LONG dummy;
    DWORD i;
    for (i = 0; i < totalEntries; i++)
    {
        dummy = 0;
        if (SetFilePointer(ZipFile, offs, &dummy, FILE_BEGIN) == 0xFFFFFFFF)
            return Error(STR_ERRACCESS, ZipName);
        if (!Read(ZipFile, &header, sizeof(CFileHeader)))
            return Error(STR_ERRREAD, ZipName);
        if (header.Signature != SIG_CENTRFH)
            return Error(STR_BADFORMAT);
        if (header.Flag & GPF_ENCRYPTED)
            Encrypt = TRUE;
        if (header.Method != CM_STORED && header.Method != CM_DEFLATED)
            return Error(STR_BADMETHOD);
        if (header.VersionExtr >= 45 &&
            (header.Size == 0xFFFFFFFF ||
             header.CompSize == 0xFFFFFFFF ||
             header.LocHeaderOffs == 0xFFFFFFFF ||
             header.StartDisk == 0xFFFF))
        {
            // jde o Zip64 odmitneme ho
            return Error(STR_ZIP64);
        }
        offs += sizeof(CFileHeader) + header.NameLen + header.ExtraLen + header.CommentLen;
    }
    return TRUE;
}

BOOL CheckZip()
{
    ArcSize = GetFileSize(ZipFile, NULL);
    DWORD toRead;
    DWORD offs;
    if (ArcSize == 0xFFFFFFFF)
        return Error(STR_ERRACCESS, ZipName);
    if (ArcSize < 22)
        return Error(STR_BADFORMAT);
    if (ArcSize > 0xFFFF)
    {
        toRead = 0xFFFF;
        offs = ArcSize - 0xFFFF;
    }
    else
    {
        toRead = ArcSize;
        offs = 0;
    }
    LONG dummy;
    dummy = 0;
    if (SetFilePointer(ZipFile, offs, &dummy, FILE_BEGIN) == 0xFFFFFFFF)
        return Error(STR_ERRACCESS, ZipName);
    if (!Read(ZipFile, IOBuffer, toRead))
        return Error(STR_ERRREAD, ZipName);
    for (char* ptr = IOBuffer + toRead - 22; ptr > IOBuffer; ptr--)
    {
        if (*(__UINT32*)ptr == SIG_EOCENTRDIR)
        {
            EOCentrDirOffs = offs + (ptr - IOBuffer);
            CEOCentrDirRecord* eocdr = (CEOCentrDirRecord*)ptr;
            if (eocdr->DiskNum > 0)
                return Error(STR_MULTVOL);
            if (eocdr->TotalEntries == 0)
                return Error(STR_EMPTYARCHIVE);
            if (EOCentrDirOffs > eocdr->CentrDirOffs + eocdr->CentrDirSize)
                return Error(STR_BADFORMAT, ZipName);
            return CheckEntries(eocdr->CentrDirOffs, eocdr->TotalEntries);
        }
    }
    return Error(STR_BADFORMAT);
}
