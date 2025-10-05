// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2004-2023 Open Salamander Authors
// Copyright (c) 2010-2023 Milan Kase <manison@manison.cz>
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

#include "fat.h"
#include "unfat.h"

#include "unfat.rh"
#include "unfat.rh2"
#include "lang\lang.rh"

//*****************************************************************************
//
// CFATImage
//

CFATImage::CFATImage()
{
    ZeroMemory(&File, sizeof(File));
    FATType = fteFATUnknow;
    VolumeStart.Set(0, 0);
}

CFATImage::~CFATImage()
{
    Close();
}

void CFATImage::Close()
{
    SalamanderSafeFile->SafeFileClose(&File);
}

BOOL CFATImage::Open(const char* fileName, BOOL quiet, HWND hParent)
{
    VolumeStart.Set(0, 0);

    // open the image; if there is a problem offer only the OK button
    if (!SalamanderSafeFile->SafeFileOpen(&File, fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                          FILE_FLAG_RANDOM_ACCESS, hParent, BUTTONS_OK, NULL, NULL))
    {
        return FALSE;
    }

    DWORD read; // number of bytes read

restart:
    // read the boot sector
    SalamanderSafeFile->SafeFileSeek(&File, &VolumeStart, FILE_BEGIN, NULL);
    if (!SalamanderSafeFile->SafeFileRead(&File, &BS, sizeof(BS), &read,
                                          hParent, BUTTONS_RETRYCANCEL, NULL, NULL))
    {
        Close();
        return FALSE;
    }

    // verify the consistency of the data
    if (read != sizeof(BS) ||
        (BS.JmpBoot[0] != 0xEB && BS.JmpBoot[0] != 0xE9) ||
        BS.BytsPerSec == 0 ||
        BS.SecPerClus == 0 ||
        BS.NumFATs == 0 ||
        (BS.TotSec16 == 0 && BS.TotSec32 == 0) ||
        (BS.TotSec16 != 0 && BS.TotSec32 != 0))
    {
        int id;
        if (VolumeStart.Value == 0)
        {
            id = IDS_ERROR_PARSING_FILE;
            if (0xFA == BS.JmpBoot[0])
            {
                BYTE marker[2];

                CQuadWord q1(0x200 - 2, 0);
                SalamanderSafeFile->SafeFileSeek(&File, &q1, FILE_BEGIN, NULL);
                SalamanderSafeFile->SafeFileRead(&File, marker, 2, &read, hParent, 0, NULL, NULL);
                if ((0x55 == marker[0]) && (0xAA == marker[1]))
                {
                    CPartitionEntry partitionTable[4];
                    CQuadWord q2(MBR_PARTITION_TABLE_OFFSET, 0);
                    SalamanderSafeFile->SafeFileSeek(&File, &q2, FILE_BEGIN, NULL);
                    SalamanderSafeFile->SafeFileRead(&File, partitionTable, sizeof(partitionTable), &read, hParent, 0, NULL, NULL);
                    if (PickFATVolume(partitionTable, 4, &VolumeStart))
                        goto restart;
                    id = IDS_ERROR_MBR_UNSUPPORTED;
                }
            }
        }
        else
        {
            id = IDS_ERROR_MBR_UNSUPPORTED;
        }
        Error(id, quiet, fileName, LoadStr(IDS_ERROR_BS_CORRUPTED));
        Close();
        return FALSE;
    }

    if (BS.FATSz16 != 0)
    {
        if (!SalamanderSafeFile->SafeFileRead(&File, &FAT1216, sizeof(FAT1216), &read, hParent,
                                              SAFE_FILE_CHECK_SIZE | BUTTONS_RETRYCANCEL, NULL, NULL))
        {
            Close();
            return FALSE;
        }
    }
    else
    {
        if (!SalamanderSafeFile->SafeFileRead(&File, &FAT32, sizeof(FAT32), &read, hParent,
                                              SAFE_FILE_CHECK_SIZE | BUTTONS_RETRYCANCEL, NULL, NULL))
        {
            Close();
            return FALSE;
        }
    }

    if (BS.FATSz16 != 0)
        FATSz = BS.FATSz16;
    else
        FATSz = FAT32.FATSz32;

    if (BS.TotSec16 != 0)
        TotSec = BS.TotSec16;
    else
        TotSec = BS.TotSec32;

    RootDirSectors = ((BS.RootEntCnt * 32) + (BS.BytsPerSec - 1)) / BS.BytsPerSec;

    FirstDataSec = BS.RsvdSecCnt + (BS.NumFATs * FATSz) + RootDirSectors;
    DataSec = TotSec - (BS.RsvdSecCnt + (BS.NumFATs * FATSz) + RootDirSectors);

    CountOfClusters = DataSec / BS.SecPerClus;

    if (CountOfClusters < 4085)
        FATType = fteFAT12;
    else if (CountOfClusters < 65525)
        FATType = fteFAT16;
    else
        FATType = fteFAT32;

    // FAT Directory Structure
    if (FATType != fteFAT32)
        FirstRootDirSecNum = BS.RsvdSecCnt + (BS.NumFATs * BS.FATSz16);
    else
        FirstRootDirSecNum = FirstDataSec + (FAT32.RootClus - 2) * BS.SecPerClus;

    return TRUE;
}

#define ATTR_READ_ONLY 0x01
#define ATTR_HIDDEN 0x02
#define ATTR_SYSTEM 0x04
#define ATTR_VOLUME_ID 0x08
#define ATTR_DIRECTORY 0x10
#define ATTR_ARCHIVE 0x20
#define ATTR_LONG_NAME (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID)
#define ATTR_LONG_NAME_MASK (ATTR_READ_ONLY | ATTR_HIDDEN | ATTR_SYSTEM | ATTR_VOLUME_ID | ATTR_DIRECTORY | ATTR_ARCHIVE)
#define LAST_LONG_ENTRY 0x40
#define LONG_ENTRY_ORD_MASK 0x3F

#define FAT1216_ROOT_DIR -1

BOOL CFATImage::ListImage(CSalamanderDirectoryAbstract* dir, HWND hParent)
{
    TDirectArray<DWORD> rootDirFAT(50, 100);
    if (FATType != fteFAT32)
    {
        // FAT12 and FAT16 have the root directory stored contiguously
        rootDirFAT.Add(FAT1216_ROOT_DIR);
        if (!rootDirFAT.IsGood())
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    else
    {
        // FAT32 keeps the root directory fragmented like any other file or directory
        if (!LoadFAT(FAT32.RootClus, &rootDirFAT, hParent, BUTTONS_RETRYCANCEL, NULL, NULL))
            return FALSE;
    }

    // the recursive AddDirectory function loads a directory and all of its subdirectories
    char root[2 * MAX_PATH];
    root[0] = 0;
    return AddDirectory(root, &rootDirFAT, dir, FirstRootDirSecNum, hParent);
}

#pragma runtime_checks("c", off)
// Microsoft function for calculating the checksum of an 8.3 name
BYTE ChkSum(BYTE* pFcpName)
{
    BYTE sum = 0;
    int fcbNameLen;
    for (fcbNameLen = 11; fcbNameLen != 0; fcbNameLen--)
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *pFcpName++;
    return sum;
}
#pragma runtime_checks("", restore)

// convert a name in the 83 format to 8.3
BOOL ConvertFATName(const char* fatName, char* name)
{
    char* p = name;
    int i;
    for (i = 0; i < 11; i++)
    {
        if (i == 8)
        {
            // if an extension follows, insert a dot
            if (fatName[i] != ' ')
            {
                if (p == name) // invalid file name
                    return FALSE;
                *p = '.';
                p++;
            }
            else
                break; // otherwise we can exit
        }
        if (fatName[i] != ' ')
        {
            *p = fatName[i];
            if (i == 0 && *p == 0x05)
                *p = (char)0xE5; // 0x05 is used instead of 0xE5 in the Japan/KANJI
            p++;
        }
    }
    if (p == name)
        return FALSE; // invalid file name
    *p = 0;
    return TRUE;
}

// temporary storage for directory names; used to optimize AddFile/AddDir calls
struct CDirStore
{
    char* Name;
    DWORD Cluster;
};

BOOL CFATImage::AddDirectory(char* root, TDirectArray<DWORD>* fat,
                             CSalamanderDirectoryAbstract* dir, DWORD sector,
                             HWND hParent)
{

    CQuadWord seek;
    seek = VolumeStart;
    seek.Value += sector * BS.BytsPerSec;
    if (!SalamanderSafeFile->SafeFileSeekMsg(&File, &seek, FILE_BEGIN,
                                             hParent, BUTTONS_RETRYCANCEL, NULL, NULL, TRUE))
    {
        return FALSE;
    }

    int fatIndex = 0; // ignore the zeroth entry from the 'fat' array; we already received 'sector'

    CDirEntry dirEnt;
    wchar_t longName[63 * 13 + 1];
    DWORD longNameOrd = 0; // 0=long name not being read; greater than 0=we last read this Ord value
    BYTE longNameChksum;
    TDirectArray<CDirStore> dirStore(10, 50);

#ifdef TRACE_ENABLE
    char seekStr[50];
#endif                                             //TRACE_ENABLE
    DWORD remains = BS.BytsPerSec * BS.SecPerClus; // how much remains to the end of the cluster
    if (fat->Count > 0 && fat->At(0) == FAT1216_ROOT_DIR)
        remains = BS.BytsPerSec * RootDirSectors;

    BOOL ok = TRUE;
    while (fatIndex < fat->Count)
    {
        DWORD read;
        if (remains == 0)
        {
            fatIndex++;
            if (fatIndex >= fat->Count)
                break; // we have read all directory clusters; stop
            seek.Value = VolumeStart.Value + (FirstDataSec + (fat->At(fatIndex) - 2) * BS.SecPerClus) * BS.BytsPerSec;
            if (!SalamanderSafeFile->SafeFileSeekMsg(&File, &seek, FILE_BEGIN, hParent,
                                                     BUTTONS_RETRYCANCEL, NULL, NULL, TRUE))
            {
                ok = FALSE;
                break;
            }
            remains = BS.BytsPerSec * BS.SecPerClus;
        }

        if (!SalamanderSafeFile->SafeFileRead(&File, &dirEnt, sizeof(dirEnt), &read, hParent,
                                              SAFE_FILE_CHECK_SIZE | BUTTONS_RETRYCANCEL, NULL, NULL))
        {
            ok = FALSE;
            break;
        }
        seek.Value += read;
        remains -= read;

        // terminator -- stop processing
        if (dirEnt.Short.Name[0] == 0)
            break;

        // skip free directory entries
        if (dirEnt.Short.Name[0] == 0xE5)
        {
            longNameOrd = 0; // do not read long_name
            continue;
        }

        // skip the label
        if ((dirEnt.Short.Attr == ATTR_VOLUME_ID) || (dirEnt.Short.Attr == (ATTR_ARCHIVE | ATTR_VOLUME_ID)))
        {
            longNameOrd = 0; // do not read long_name
            continue;
        }

        if ((dirEnt.Short.Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
        {
            // this is a long name entry
            BOOL lastEntry = ((dirEnt.Long.Ord & LAST_LONG_ENTRY) != 0);
            DWORD ord = dirEnt.Long.Ord & LONG_ENTRY_ORD_MASK;

            if (longNameOrd == 0 && !lastEntry)
            {
                // we are not in the middle of a long_name and a non-terminal part arrived
                TRACE_E("CFATImage::AddDirectory: Long name terminator was expected. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                continue; // skip it
            }

            if (ord == 0)
            {
                // invalid data; abort reading
                longNameOrd = 0; // do not read long_name
                TRACE_E("CFATImage::AddDirectory: Invalid Ord. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                continue;
            }

            if (!lastEntry)
            {
                if (ord != longNameOrd - 1)
                {
                    // invalid data; abort reading
                    longNameOrd = 0; // do not read long_name
                    TRACE_E("CFATImage::AddDirectory: Non continuous ord. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                    continue;
                }
            }

            // all checksums must be equal
            if (lastEntry)
                longNameChksum = dirEnt.Long.Chksum; // store it for verification
            else
            {
                if (dirEnt.Long.Chksum != longNameChksum)
                {
                    // invalid data; abort reading
                    longNameOrd = 0; // do not read long_name
                    TRACE_E("CFATImage::AddDirectory: Different checksum in the long name. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                    continue;
                }
            }

            longNameOrd = ord;
            ord--;
            memcpy(longName + ord * 13 + 0, dirEnt.Long.Name1, 10);
            memcpy(longName + ord * 13 + 5, dirEnt.Long.Name2, 12);
            memcpy(longName + ord * 13 + 11, dirEnt.Long.Name3, 4);
            if (lastEntry)
                longName[ord * 13 + 13] = 0; // if the name is exactly 13 characters it lacks a terminator -> create one
            continue;                        // move on to the next part of the long_name or to the short_name
        }

        if (longNameOrd > 1) // are we stuck in the middle (neither 0 nor 1)?
            longNameOrd = 0; // the long_name was not fully read -> discard it

        CFileData file;
        char name8_3[13];
        if (!ConvertFATName(dirEnt.Short.Name, name8_3))
        {
            longNameOrd = 0;
            TRACE_E("CFATImage::AddDirectory: Error converting to 8.3 name. offset=0x" << _i64toa(seek.Value, seekStr, 16));
            continue; // skip the nonsensical name
        }

        // skip . and ..
        if (name8_3[0] == '.' && (name8_3[1] == 0 || (name8_3[1] == '.' && name8_3[2] == 0)))
        {
            longNameOrd = 0;
            if (*root == 0)
                TRACE_E("CFATImage::AddDirectory: . and .. in the root directory. offset=0x" << _i64toa(seek.Value, seekStr, 16)); // the root directory must not contain . and ..
            continue;
        }

        // verify the checksum of the long name part
        if (longNameOrd == 1)
        {
            BYTE sum = ChkSum((BYTE*)dirEnt.Short.Name);
            if (sum != longNameChksum)
            {
                TRACE_E("CFATImage::AddDirectory: Different checksum of short name. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                longNameOrd = 0;
            }
        }

        // determine the length of the long name (UNICODE)
        int longNameLen = 0;
        if (longNameOrd == 1)
        {
            while (longName[longNameLen] != 0)
                longNameLen++;
            longNameOrd = 0;
        }

        if (longNameLen > 0)
        {
            file.Name = (char*)SalamanderGeneral->Alloc(longNameLen + 1);
            if (file.Name == NULL)
            {
                TRACE_E(LOW_MEMORY);
                ok = FALSE;
                break;
            }
            // Convert the UNICODE string to ANSI
            WideCharToMultiByte(CP_ACP, 0, longName, longNameLen + 1,
                                file.Name, longNameLen + 1, NULL, NULL);
            file.Name[longNameLen] = 0;
            file.DosName = SalamanderGeneral->DupStr(name8_3);
            if (file.DosName == NULL)
            {
                TRACE_E(LOW_MEMORY);
                SalamanderGeneral->Free(file.Name);
                ok = FALSE;
                break;
            }
        }
        else
        {
            if ((dirEnt.Short.NTRes & 0x08) != 0)
                _strlwr(name8_3);
            file.Name = SalamanderGeneral->DupStr(name8_3);
            if (file.Name == NULL)
            {
                TRACE_E(LOW_MEMORY);
                ok = FALSE;
                break;
            }
            file.DosName = NULL;
        }

        file.NameLen = strlen(file.Name);
        if (!SortByExtDirsAsFiles && (dirEnt.Short.Attr & ATTR_DIRECTORY))
        {
            // directories have no extension
            file.Ext = file.Name + file.NameLen;
        }
        else
        {
            // for files the extension is behind the last dot
            char* s = strrchr(file.Name, '.');
            if (s != NULL)
                file.Ext = s + 1; // ".cvspass" is an extension in Windows
            else
                file.Ext = file.Name + file.NameLen;
        }
        file.Size = CQuadWord(dirEnt.Short.FileSize, 0);
        file.Attr = 0;
        if (dirEnt.Short.Attr & ATTR_READ_ONLY)
            file.Attr |= FILE_ATTRIBUTE_READONLY;
        if (dirEnt.Short.Attr & ATTR_HIDDEN)
            file.Attr |= FILE_ATTRIBUTE_HIDDEN;
        if (dirEnt.Short.Attr & ATTR_SYSTEM)
            file.Attr |= FILE_ATTRIBUTE_SYSTEM;
        if (dirEnt.Short.Attr & ATTR_ARCHIVE)
            file.Attr |= FILE_ATTRIBUTE_ARCHIVE;
        file.Hidden = 0;
        file.IsOffline = 0;
        file.PluginData = (((DWORD)dirEnt.Short.FstClusHI) << 16) | dirEnt.Short.FstClusLO;

        if (!DosDateTimeToFileTime(dirEnt.Short.WrtDate, dirEnt.Short.WrtTime, &file.LastWrite))
            TRACE_E("DosDateTimeToFileTime failed");
        if (!LocalFileTimeToFileTime(&file.LastWrite, &file.LastWrite))
            TRACE_E("LocalFileTimeToFileTime failed");

        if (dirEnt.Short.Attr & ATTR_DIRECTORY)
        {
            // directory

            // add to the listing
            file.IsLink = 0;
            if (!dir->AddDir(root, file, NULL))
            {
                SalamanderGeneral->Free(file.Name);
                if (file.DosName != NULL)
                    SalamanderGeneral->Free(file.DosName);
                ok = FALSE;
                break;
            }

            // store so that we can traverse all directories at the end
            CDirStore ds;
            ds.Name = SalamanderGeneral->DupStr(file.Name);
            ds.Cluster = (DWORD)file.PluginData;
            if (ds.Name == NULL)
            {
                TRACE_E(LOW_MEMORY);
                ok = FALSE;
                break;
            }
            dirStore.Add(ds);
            if (!dirStore.IsGood())
            {
                TRACE_E(LOW_MEMORY);
                SalamanderGeneral->Free(ds.Name);
                dirStore.ResetState(); // ensure the array's destructor runs
                ok = FALSE;
                break;
            }
        }
        else
        {
            // add the file to the listing
            file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
            if (!dir->AddFile(root, file, NULL))
            {
                SalamanderGeneral->Free(file.Name);
                if (file.DosName != NULL)
                    SalamanderGeneral->Free(file.DosName);
                ok = FALSE;
                break;
            }
        }
    }

    // finally call ourselves for all stored directories
    if (ok && dirStore.Count > 0)
    {
        char* rootEnd = root + strlen(root);
        int i;
        for (i = 0; i < dirStore.Count; i++)
        {
            CDirStore* ds = &dirStore[i];

            if (rootEnd - root + strlen(ds->Name) + 2 >= MAX_PATH)
            {
                // we must not allow exceeding MAX_PATH
                TRACE_E("The path len exceeds MAX_PATH. Skipping directory: " << ds->Name);
                continue;
            }

            sprintf(rootEnd, "%s\\", ds->Name);

            // firstSectorOfCluster = FirstDataSec + (N - 2) * SecPerClus
            DWORD newSector = FirstDataSec + (ds->Cluster - 2) * BS.SecPerClus;
            // we no longer need the FAT entries; we can use our own array
            if (!LoadFAT(ds->Cluster, fat, hParent, BUTTONS_RETRYCANCEL, NULL, NULL))
            {
                ok = FALSE;
                break;
            }
            if (!AddDirectory(root, fat, dir, newSector, hParent))
            {
                ok = FALSE;
                break;
            }
        }
    }

    // release the store on exit
    int j;
    for (j = 0; j < dirStore.Count; j++)
        SalamanderGeneral->Free(dirStore[j].Name);

    return ok;
}

BOOL CFATImage::LoadFAT(DWORD cluster, TDirectArray<DWORD>* fat, HWND hParent,
                        DWORD buttons, DWORD* pressedButton, DWORD* silentMask)
{
    if (pressedButton != NULL)          // if we return FALSE and do not change this value
        *pressedButton = DIALOG_CANCEL; // we meant cancellation of the entire operation

    fat->DetachMembers();

    if (cluster == 0) // size==0
        return TRUE;

    DWORD beginOfFAT = BS.RsvdSecCnt * BS.BytsPerSec;

    CQuadWord seek;

    DWORD entry = cluster;
    fat->Add(entry);
    if (!fat->IsGood())
    {
        TRACE_E(LOW_MEMORY);
        fat->ResetState();
        return FALSE;
    }

    BOOL eoc = FALSE; // End Of Clusterchain
    do
    {
        BOOL oddCluster = (entry & 1) != 0;

        seek.Value = VolumeStart.Value + beginOfFAT;
        switch (FATType)
        {
        case fteFAT12:
            seek.Value += entry + (entry / 2);
            break;
        case fteFAT16:
            seek.Value += entry * 2;
            break;
        case fteFAT32:
            seek.Value += entry * 4;
            break;
        }

        // attempt to read the entry from the FAT
        if (!SalamanderSafeFile->SafeFileSeekMsg(&File, &seek, FILE_BEGIN, hParent,
                                                 buttons, pressedButton, silentMask, TRUE))
        {
            return FALSE;
        }

        DWORD read; // number of bytes read
        if (!SalamanderSafeFile->SafeFileRead(&File, &entry, sizeof(entry), &read, hParent,
                                              SAFE_FILE_CHECK_SIZE | buttons, pressedButton, silentMask))
        {
            return FALSE;
        }

        switch (FATType)
        {
        case fteFAT12:
        {
            // Since 12 bits is not an integral number of bytes, we have
            // to specify how these are arranged. Two FAT12 entries are
            // stored into three bytes;
            // if these bytes are uv,wx,yz then the entries are xuv and yzw.
            if (oddCluster)
                entry >>= 4;
            entry &= 0x00000FFF;
            if (entry == 0x00000FFF)
                eoc = TRUE;
            break;
        }

        case fteFAT16:
        {
            entry &= 0x0000FFFF;
            if (entry == 0x0000FFFF)
                eoc = TRUE;
            break;
        }

        case fteFAT32:
        {
            entry &= 0x0FFFFFFF;
            if (entry == 0x0FFFFFFF)
                eoc = TRUE;
            break;
        }
        }
        if (!eoc)
        {
            fat->Add(entry);
            if (!fat->IsGood())
            {
                TRACE_E(LOW_MEMORY);
                fat->ResetState();
                return FALSE;
            }
        }
    } while (!eoc);
    return TRUE;
}

// prepare a string for error messages; it contains the size, date, and time
void GetFileInfo(char* buffer, int bufferLen, const CFileData* fileData)
{
    CALL_STACK_MESSAGE2("GetFileInfo(, %d, , )", bufferLen);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(&fileData->LastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[100], time[100], number[100];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    int len = _snprintf_s(buffer, bufferLen, _TRUNCATE, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, fileData->Size), date, time);
    if (len < 0)
        TRACE_E("GetFileInfo: small buffer. bufferLen=" << bufferLen);
}

#define COPY_MIN_FILE_SIZE CQuadWord(1024, 0) // must not be less than 1 (otherwise the allocation test for required file space before copying in DoCopyFile would fail)

BOOL CFATImage::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* archiveName,
                           const char* nameInArchive, const CFileData* fileData,
                           const char* targetDir, DWORD* silentMask, BOOL allowSkip,
                           BOOL* skipped, char* skipPath, int skipPathMax, HWND hParent,
                           CAllocWholeFileEnum* allocWholeFileOnStart)
{
    if (skipped != NULL)
    {
        *skipped = FALSE;
        if (!allowSkip)
            skipped = NULL; // to shorten condition checks in this method
    }

    // read the cluster chain for the 'fileData' file
    TDirectArray<DWORD> fat(50, 100);
    DWORD pressedButton;
    if (!LoadFAT((DWORD)fileData->PluginData, &fat, hParent,
                 allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                 &pressedButton, silentMask))
    {
        if (allowSkip && skipped != NULL && (pressedButton == DIALOG_SKIP || pressedButton == DIALOG_SKIPALL))
            *skipped = TRUE;
        return FALSE;
    }

    char targetName[2 * MAX_PATH];
    strcpy(targetName, targetDir);
    if (!SalamanderGeneral->SalPathAppend(targetName, fileData->Name, 2 * MAX_PATH))
    {
        TRACE_E("Name is too long, skipping");
        if (allowSkip)
            *skipped = TRUE;
        return FALSE;
    }

    // for copying we will need a buffer the size of a cluster
    DWORD clusterSize = 1 * BS.SecPerClus * BS.BytsPerSec;
    BYTE* clusterBuffer = (BYTE*)malloc(clusterSize);
    if (clusterBuffer == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    // "extracting: %s..."
    char progressText[2 * MAX_PATH + 100];
    sprintf(progressText, LoadStr(IDS_EXTRACTING), nameInArchive);
    salamander->ProgressDialogAddText(progressText, TRUE);

    char fileInfo[200];
    GetFileInfo(fileInfo, 200, fileData);

    char currentImgPath[2 * MAX_PATH];
    strcpy(currentImgPath, archiveName);
    if (!SalamanderGeneral->SalPathAppend(currentImgPath, nameInArchive, 2 * MAX_PATH))
    {
        TRACE_E("Name is too long, skipping");
        if (allowSkip)
            *skipped = TRUE;
        free(clusterBuffer);
        return FALSE;
    }

    // support for minimizing disk fragmentation
    CQuadWord minAllocFileSize = max(COPY_MIN_FILE_SIZE, CQuadWord(clusterSize, 0)); // files below this size will not be preallocated
    CQuadWord allocFileSize = fileData->Size;
    BOOL allocateWholeFile = FALSE;
    if (*allocWholeFileOnStart != awfDisabled &&  // preallocating the whole file is not disabled
        allocFileSize > minAllocFileSize &&       // fileSize validity condition + below the copy buffer size preallocation makes no sense (smaller files all use COPY_MIN_FILE_SIZE) (and there is no point below 1 byte)
        allocFileSize < CQuadWord(0, 0x80000000)) // the file size must be a positive number (otherwise seeking is impossible - these are numbers above 8EB, so this likely never happens)
    {
        allocateWholeFile = TRUE;
        if (*allocWholeFileOnStart == awfNeededTest) // perform a test on the first file
            allocFileSize = fileData->Size + CQuadWord(0, 0x80000000);
    }

    SAFE_FILE outFile;
    HANDLE hFile = SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE, FILE_SHARE_READ,
                                                      0, FALSE, hParent, currentImgPath,
                                                      fileInfo, silentMask,
                                                      allowSkip, skipped, skipPath, skipPathMax,
                                                      allocateWholeFile ? &allocFileSize : NULL,
                                                      &outFile);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        free(clusterBuffer);
        return FALSE;
    }

    // support for minimizing disk fragmentation
    if (allocateWholeFile && allocFileSize == CQuadWord(-1, 0))
        *allocWholeFileOnStart = awfDisabled; // preallocation failed and we should not attempt it again in this batch

    BOOL ok = TRUE;

    // store the target file cluster by cluster
    CQuadWord remains = fileData->Size;
    int i;
    for (i = 0; i < fat.Count; i++)
    {
        CQuadWord seek;
        seek.Value = VolumeStart.Value + (FirstDataSec + (fat[i] - 2) * BS.SecPerClus) * BS.BytsPerSec;
        if (!SalamanderSafeFile->SafeFileSeekMsg(&File, &seek, FILE_BEGIN, hParent,
                                                 allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                                 &pressedButton, silentMask, TRUE))
        {
            if (skipped != NULL && (pressedButton == DIALOG_SKIP || pressedButton == DIALOG_SKIPALL))
                *skipped = TRUE;
            ok = FALSE;
            goto EXIT;
        }

        DWORD toRead = (remains.Value < (unsigned __int64)clusterSize) ? remains.LoDWord : clusterSize;

        // read the cluster
        DWORD read;
        if (!SalamanderSafeFile->SafeFileRead(&File, clusterBuffer, toRead, &read, hParent,
                                              allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                              &pressedButton, silentMask))
        {
            if (skipped != NULL && (pressedButton == DIALOG_SKIP || pressedButton == DIALOG_SKIPALL))
                *skipped = TRUE;
            ok = FALSE;
            goto EXIT;
        }

        // write it to the output file
        DWORD written;
        if (!SalamanderSafeFile->SafeFileWrite(&outFile, clusterBuffer, read, &written, hParent,
                                               allowSkip ? BUTTONS_RETRYSKIPCANCEL : BUTTONS_RETRYCANCEL,
                                               &pressedButton, silentMask))
        {
            if (skipped != NULL && (pressedButton == DIALOG_SKIP || pressedButton == DIALOG_SKIPALL))
                *skipped = TRUE;
            ok = FALSE;
            goto EXIT;
        }

        // advance the progress
        if (!salamander->ProgressAddSize(read, TRUE)) // delayedPaint==TRUE so we do not slow things down
        {
            salamander->ProgressEnableCancel(FALSE);
            ok = FALSE;
            goto EXIT; // the operation was cancelled
        }

        remains.Value -= read;
    }

    if (fileData->Size < COPY_MIN_FILE_SIZE) // small file -- enlarge it for progress reporting
    {
        remains = COPY_MIN_FILE_SIZE - fileData->Size;
        if (!salamander->ProgressAddSize((int)remains.Value, TRUE))
        {
            ok = FALSE;
            goto EXIT; // the operation was cancelled
        }
    }

    // set the timestamp at the end
    if (!SetFileTime(outFile.HFile, &fileData->LastWrite, &fileData->LastWrite, &fileData->LastWrite))
        TRACE_E("SetFileTime failed");

EXIT:
    SalamanderSafeFile->SafeFileClose(&outFile);
    if (!ok)
    {
        // the file may have received a read-only attribute and DeleteFile would not be able to remove it
        SalamanderGeneral->ClearReadOnlyAttr(targetName);
        if (!DeleteFile(targetName))
            TRACE_E("DeleteFile failed");
    }
    else
    {
        // FIXME: (consult with Petr) I am not sure here
        // if the source has no attribute, SafeCreateFile assigned it 'A'
        // this guarantees an exact copy, but it is not compatible with Salamander's Copy command
        // every plugin behaves differently; it is a mess

        if (!SetFileAttributes(targetName, fileData->Attr))
            TRACE_E("SetFileAttributes failed");
    }
    free(clusterBuffer);
    return ok;
}

BOOL CFATImage::PickFATVolume(const CPartitionEntry* partitionTable, DWORD numEntries,
                              CQuadWord* volumeStart)
{
    DWORD i;
    for (i = 0; i < numEntries; i++)
    {
        if (partitionTable->Type == PARTITIONTYPE_FAT12 ||
            partitionTable->Type == PARTITIONTYPE_FAT16 ||
            partitionTable->Type == PARTITIONTYPE_FAT16_32M ||
            partitionTable->Type == PARTITIONTYPE_FAT32 ||
            partitionTable->Type == PARTITIONTYPE_FAT32_LBA)
        {
            volumeStart->SetUI64(partitionTable->StartLBA * (unsigned __int64)512);
            return TRUE;
        }
    }

    return FALSE;
}
