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

    // otevreme image, v pripade problemu nabidneme pouze tlacitko OK
    if (!SalamanderSafeFile->SafeFileOpen(&File, fileName, GENERIC_READ, FILE_SHARE_READ, OPEN_EXISTING,
                                          FILE_FLAG_RANDOM_ACCESS, hParent, BUTTONS_OK, NULL, NULL))
    {
        return FALSE;
    }

    DWORD read; // pocet nactenych bajtu

restart:
    // naciteme boot sector
    SalamanderSafeFile->SafeFileSeek(&File, &VolumeStart, FILE_BEGIN, NULL);
    if (!SalamanderSafeFile->SafeFileRead(&File, &BS, sizeof(BS), &read,
                                          hParent, BUTTONS_RETRYCANCEL, NULL, NULL))
    {
        Close();
        return FALSE;
    }

    // overime konzistenci dat
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
        // FAT12 a FAT16 maji root dir v jednom kuse
        rootDirFAT.Add(FAT1216_ROOT_DIR);
        if (!rootDirFAT.IsGood())
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    else
    {
        // FAT32 ma root dir roztrhany jako kazdy jiny soubor nebo adresar
        if (!LoadFAT(FAT32.RootClus, &rootDirFAT, hParent, BUTTONS_RETRYCANCEL, NULL, NULL))
            return FALSE;
    }

    // rekurzivni funkce AddDirectory nacte adresar a vsechny podadresare
    char root[2 * MAX_PATH];
    root[0] = 0;
    return AddDirectory(root, &rootDirFAT, dir, FirstRootDirSecNum, hParent);
}

#pragma runtime_checks("c", off)
// Microsofti funkce pro vypocet checksumu 8.3 nazvu
BYTE ChkSum(BYTE* pFcpName)
{
    BYTE sum = 0;
    int fcbNameLen;
    for (fcbNameLen = 11; fcbNameLen != 0; fcbNameLen--)
        sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *pFcpName++;
    return sum;
}
#pragma runtime_checks("", restore)

// konverze jmena ve formatu 83 na 8.3
BOOL ConvertFATName(const char* fatName, char* name)
{
    char* p = name;
    int i;
    for (i = 0; i < 11; i++)
    {
        if (i == 8)
        {
            // pokud za existuje pripona, vlozime tecku
            if (fatName[i] != ' ')
            {
                if (p == name) // chybny nazev souboru
                    return FALSE;
                *p = '.';
                p++;
            }
            else
                break; // jinak muzeme vypadnout
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
        return FALSE; // chybny nazev souboru
    *p = 0;
    return TRUE;
}

// docasne uloziste jmen adresaru; slouzi k optimalizovanemu volani AddFile/AddDir
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

    int fatIndex = 0; // nulty zaznam z pole 'fat' ignorujeme, dostali jsme 'sector'

    CDirEntry dirEnt;
    wchar_t longName[63 * 13 + 1];
    DWORD longNameOrd = 0; // 0=nenacita se long name; vetsi nez 0=naposledy jsme nacetli tento Ord
    BYTE longNameChksum;
    TDirectArray<CDirStore> dirStore(10, 50);

#ifdef TRACE_ENABLE
    char seekStr[50];
#endif                                             //TRACE_ENABLE
    DWORD remains = BS.BytsPerSec * BS.SecPerClus; // kolik zbyva do konce clusteru
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
                break; // nacetli jsme vsechny clustery adresare, koncime
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

        // terminator -- koncime
        if (dirEnt.Short.Name[0] == 0)
            break;

        // free directory entry preskakujeme
        if (dirEnt.Short.Name[0] == 0xE5)
        {
            longNameOrd = 0; // nenacitame long_name
            continue;
        }

        // label preskakujeme
        if ((dirEnt.Short.Attr == ATTR_VOLUME_ID) || (dirEnt.Short.Attr == (ATTR_ARCHIVE | ATTR_VOLUME_ID)))
        {
            longNameOrd = 0; // nenacitame long_name
            continue;
        }

        if ((dirEnt.Short.Attr & ATTR_LONG_NAME_MASK) == ATTR_LONG_NAME)
        {
            // jde o long name
            BOOL lastEntry = ((dirEnt.Long.Ord & LAST_LONG_ENTRY) != 0);
            DWORD ord = dirEnt.Long.Ord & LONG_ENTRY_ORD_MASK;

            if (longNameOrd == 0 && !lastEntry)
            {
                // nejsme uprostred long_name a prisla cast, ktera neni zaverecna
                TRACE_E("CFATImage::AddDirectory: Long name terminator was expected. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                continue; // skipneme ji
            }

            if (ord == 0)
            {
                // zmetek, prerusime nacitani
                longNameOrd = 0; // nenacitame long_name
                TRACE_E("CFATImage::AddDirectory: Invalid Ord. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                continue;
            }

            if (!lastEntry)
            {
                if (ord != longNameOrd - 1)
                {
                    // zmetek, prerusime nacitani
                    longNameOrd = 0; // nenacitame long_name
                    TRACE_E("CFATImage::AddDirectory: Non continuous ord. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                    continue;
                }
            }

            // vsechny checksumy musi byt rovny
            if (lastEntry)
                longNameChksum = dirEnt.Long.Chksum; // ulozime pro overovani
            else
            {
                if (dirEnt.Long.Chksum != longNameChksum)
                {
                    // zmetek, prerusime nacitani
                    longNameOrd = 0; // nenacitame long_name
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
                longName[ord * 13 + 13] = 0; // pokud je nazev presne na 13 znaku, je bez terminatoru -> vyrobime si ho
            continue;                        // sahneme pro dalsi cast long_name nebo pro short_name
        }

        if (longNameOrd > 1) // nejsme nekde uprostred (jine nez 0 a jine nez 1)?
            longNameOrd = 0; // nenacetli jsme cely long_name -> zahodime ho

        CFileData file;
        char name8_3[13];
        if (!ConvertFATName(dirEnt.Short.Name, name8_3))
        {
            longNameOrd = 0;
            TRACE_E("CFATImage::AddDirectory: Error converting to 8.3 name. offset=0x" << _i64toa(seek.Value, seekStr, 16));
            continue; // nesmyslny nazev preskocime
        }

        // preskocime . a ..
        if (name8_3[0] == '.' && (name8_3[1] == 0 || (name8_3[1] == '.' && name8_3[2] == 0)))
        {
            longNameOrd = 0;
            if (*root == 0)
                TRACE_E("CFATImage::AddDirectory: . and .. in the root directory. offset=0x" << _i64toa(seek.Value, seekStr, 16)); // root dir nema obsahovat . a ..
            continue;
        }

        // overim checksum long name casti
        if (longNameOrd == 1)
        {
            BYTE sum = ChkSum((BYTE*)dirEnt.Short.Name);
            if (sum != longNameChksum)
            {
                TRACE_E("CFATImage::AddDirectory: Different checksum of short name. offset=0x" << _i64toa(seek.Value, seekStr, 16));
                longNameOrd = 0;
            }
        }

        // napocitam delku long name (UNICODE)
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
            // adresar nema priponu
            file.Ext = file.Name + file.NameLen;
        }
        else
        {
            // u souboru je pripona za posledni teckou
            char* s = strrchr(file.Name, '.');
            if (s != NULL)
                file.Ext = s + 1; // ".cvspass" ve Windows je pripona
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
            // adresar

            // pridame do listingu
            file.IsLink = 0;
            if (!dir->AddDir(root, file, NULL))
            {
                SalamanderGeneral->Free(file.Name);
                if (file.DosName != NULL)
                    SalamanderGeneral->Free(file.DosName);
                ok = FALSE;
                break;
            }

            // ulozime abychom mohli na zaver traverzovat pres vsechny adresare
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
                dirStore.ResetState(); // aby probehla destrukce pole
                ok = FALSE;
                break;
            }
        }
        else
        {
            // soubor pridame do listingu
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

    // na zaver zavolame sami sebe pro vsechny ulozene adresare
    if (ok && dirStore.Count > 0)
    {
        char* rootEnd = root + strlen(root);
        int i;
        for (i = 0; i < dirStore.Count; i++)
        {
            CDirStore* ds = &dirStore[i];

            if (rootEnd - root + strlen(ds->Name) + 2 >= MAX_PATH)
            {
                // nesmime povolit prekroceni MAX_PATH
                TRACE_E("The path len exceeds MAX_PATH. Skipping directory: " << ds->Name);
                continue;
            }

            sprintf(rootEnd, "%s\\", ds->Name);

            // firstSectorOfCluster = FirstDataSec + (N - 2) * SecPerClus
            DWORD newSector = FirstDataSec + (ds->Cluster - 2) * BS.SecPerClus;
            // fat polozky uz nebudeme potrebovat, muzeme pouzit nase pole
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

    // pri odchodu uvolnime store
    int j;
    for (j = 0; j < dirStore.Count; j++)
        SalamanderGeneral->Free(dirStore[j].Name);

    return ok;
}

BOOL CFATImage::LoadFAT(DWORD cluster, TDirectArray<DWORD>* fat, HWND hParent,
                        DWORD buttons, DWORD* pressedButton, DWORD* silentMask)
{
    if (pressedButton != NULL)          // pokud vratime FALSE a nezmenime tuto hodnotu
        *pressedButton = DIALOG_CANCEL; // meli jsme na mysli preruseni cele operace

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

        // pokusime se nacist polozku z FAT
        if (!SalamanderSafeFile->SafeFileSeekMsg(&File, &seek, FILE_BEGIN, hParent,
                                                 buttons, pressedButton, silentMask, TRUE))
        {
            return FALSE;
        }

        DWORD read; // pocet nactenych bajtu
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

// pripravi string pro chybove hlasky; obsahuje velikost, datum, cas
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

#define COPY_MIN_FILE_SIZE CQuadWord(1024, 0) // nesmi byt mensi nez 1 (duvod: jinak nebude slapat test alokace potrebneho mista pro soubor pred zahajenim kopirovani v DoCopyFile)

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
            skipped = NULL; // pro zkraceni podminek v teto metode
    }

    // nacteme cluster chain pro soubor 'fileData'
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

    // pro kopirovani budeme potrebovat buffer o velikosti clusteru
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

    // podpora pro minimalizaci fragmentace disku
    CQuadWord minAllocFileSize = max(COPY_MIN_FILE_SIZE, CQuadWord(clusterSize, 0)); // soubory pod touto velokosti nebudeme predalokovavat
    CQuadWord allocFileSize = fileData->Size;
    BOOL allocateWholeFile = FALSE;
    if (*allocWholeFileOnStart != awfDisabled &&  // alokovani celeho souboru neni zakazano
        allocFileSize > minAllocFileSize &&       // podminka platnosti fileSize + pod velikost kopirovaciho bufferu nema alokace souboru smysl (mensi soubory maji vsechny COPY_MIN_FILE_SIZE) (navic nema smysl na souborech pod 1 byte)
        allocFileSize < CQuadWord(0, 0x80000000)) // velikost souboru je kladne cislo (jinak nelze seekovat - jde o cisla nad 8EB, takze zrejme nikdy nenastane)
    {
        allocateWholeFile = TRUE;
        if (*allocWholeFileOnStart == awfNeededTest) // na prvnim souboru provedeme test
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

    // podpora pro minimalizaci fragmentace disku
    if (allocateWholeFile && allocFileSize == CQuadWord(-1, 0))
        *allocWholeFileOnStart = awfDisabled; // predalokace se nezdarila a nemame se o ni v teto davce ani pokouset

    BOOL ok = TRUE;

    // cluster po clusteru ulozime cilovy soubor
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

        // nactemem cluster
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

        // ulozime ho do vystupniho souboru
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

        // posuneme progress
        if (!salamander->ProgressAddSize(read, TRUE)) // delayedPaint==TRUE, abychom nebrzdili
        {
            salamander->ProgressEnableCancel(FALSE);
            ok = FALSE;
            goto EXIT; // preruseni akce
        }

        remains.Value -= read;
    }

    if (fileData->Size < COPY_MIN_FILE_SIZE) // maly soubor -- pro progress zvetsime
    {
        remains = COPY_MIN_FILE_SIZE - fileData->Size;
        if (!salamander->ProgressAddSize((int)remains.Value, TRUE))
        {
            ok = FALSE;
            goto EXIT; // preruseni akce
        }
    }

    // casove razitko nastavime na zaver
    if (!SetFileTime(outFile.HFile, &fileData->LastWrite, &fileData->LastWrite, &fileData->LastWrite))
        TRACE_E("SetFileTime failed");

EXIT:
    SalamanderSafeFile->SafeFileClose(&outFile);
    if (!ok)
    {
        // soubor mohl dostat read-only atribut a DeleteFile by ho nedokazal smazat
        SalamanderGeneral->ClearReadOnlyAttr(targetName);
        if (!DeleteFile(targetName))
            TRACE_E("DeleteFile failed");
    }
    else
    {
        // FIXME: (konzultovat s Petrem) tady si nejsem jisty
        // pokud zdroj nema zadny atribut, SafeCreateFile mu nastavilo 'A'
        // timto zarucuji exaktni kopii, ale neni to kompatibilni s prikazem Copy Salamandera
        // kazdy plugin se chova jinak, meme v tom hokej

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
