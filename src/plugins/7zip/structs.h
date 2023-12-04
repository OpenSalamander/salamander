// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/MyString.h"
#include "Common/StringConvert.h"

struct CUpdateInfo
{
    bool NewData;
    bool NewProperties;

    bool ExistsInArchive;
    int ArchiveItemIndex;

    bool ExistsOnDisk;
    int FileItemIndex;
    bool IsAnti;
};

// pouziva se v updatecallback
struct CFileItem
{
    UINT32 Attributes;
    FILETIME CreationTime;
    FILETIME LastAccessTime;
    FILETIME LastWriteTime;
    UINT64 Size;
    UString Name;
    UString FullPath;
    bool IsDir;

    BOOL CanDelete; // TRUE, pokud se pri update do archivu zvolilo Overwrite, jinak FALSE

    CFileItem(const char* sourcePath, const char* archiveRoot, const char* name, DWORD attr, UINT64 size, FILETIME lastWrite, bool isDir)
    {
        // pokud je archiveRoot prazdne, nesmi jmeno zacinat backslashem '\'
        if (strlen(archiveRoot) > 0)
            Name = GetUnicodeString(archiveRoot) + GetUnicodeString("\\") + GetUnicodeString(name);
        else
            Name = GetUnicodeString(name);

        FullPath = GetUnicodeString(sourcePath) + GetUnicodeString("\\") + GetUnicodeString(name);
        Attributes = attr;
        Size = size;
        LastWriteTime = CreationTime = LastAccessTime = lastWrite;
        IsDir = isDir;

        CanDelete = FALSE;
    }
};

// pouziva se v extractcallback
struct CArchiveItem
{
    UString NameInArchive; // v archivu (tedy i s cestou)
    UString Name;
    DWORD Attr;
    FILETIME LastWrite;
    UINT64 Size;
    bool IsDir;
    UINT32 Idx;

    CArchiveItem(UINT32 idx, UString name, UINT64 size, DWORD attr, FILETIME lastWrite, bool isDir)
    {
        Idx = idx;
        //    NameInArchive = nameInArchive;
        Name = name;
        Size = size;
        Attr = attr;
        LastWrite = lastWrite;
        IsDir = isDir;
    }
};
