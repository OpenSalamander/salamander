// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// experimental - when we find "zeros" entry, we assume there are no further entries and we don't read rest
// JRYFIXME - does it really work? If yes, we should also allocate smaller buffer.
#define FAST_ENTRIES_SCAN

// #define MEASURE_READING_PERFORMANCE
#ifdef MEASURE_READING_PERFORMANCE
extern LARGE_INTEGER ReadingDirEntriesTicks;
extern LARGE_INTEGER ticks;
#endif

/*
#ifdef MEASURE_READING_PERFORMANCE
LARGE_INTEGER _t1, _t2;
QueryPerformanceCounter(&_t1);
#endif
#ifdef MEASURE_READING_PERFORMANCE
QueryPerformanceCounter(&_t2);
ticks.QuadPart += (_t2.QuadPart - _t1.QuadPart);
#endif
*/

#pragma pack(push, exfat_h)
#pragma pack(1)
struct EXFAT_DIRENTRY_GENERIC
{
    BYTE EntryType;
};

// Allocation Bitmap Directory Entry
struct EXFAT_DIRENTRY_BITMAP
{
    BYTE EntryType; // 0x81
    BYTE BitmapFlags;
    BYTE Reserved[18];
    DWORD FirstCluster; // Cluster Address of First Data Block
    QWORD DataLength;   // Length of data in bytes
};

// UP-Case Table Directory Entry
struct EXFAT_DIRENTRY_UPCASE
{
    BYTE EntryType; // 0x82
    BYTE Reserved1[3];
    DWORD TableChecksum;
    BYTE Reserved2[12];
    DWORD FirstCluster; // Cluster Address of First Data Block
    QWORD DataLength;   // Length of data in bytes
};

// File Directory Entry
struct EXFAT_DIRENTRY_FILE
{
    BYTE EntryType; // 0x85
    BYTE SecondaryCount;
    WORD SetChecksum;
    WORD FileAttributes;
    BYTE Reserved1[2];
    DWORD Created;
    DWORD LastModified;
    DWORD LastAccessed;
    BYTE Created10ms;
    BYTE LastModified10ms;
    BYTE CreateTZOffset;
    BYTE LastModifiedTZOffset;
    BYTE LastAccessZOffset;
    BYTE Reserved2[7];
};

// Stream Extension Directory Entry
struct EXFAT_DIRENTRY_STREXT
{
    BYTE EntryType; // 0xC0
    BYTE SecondaryFlags;
    BYTE Reserved1[1];
    BYTE NameLength;
    WORD NameHash; // Used for directory searches
    BYTE Reserved2[2];
    QWORD ValidDataLength;
    BYTE Reserved3[4];
    DWORD FirstCluster; // Cluster Address of First Data Block
    QWORD DataLength;   // Length of the Data. If this is a directory, then the maximum value for this field is 256M
};

// File Name Extension Directory Entry
struct EXFAT_DIRENTRY_FILENAME
{
    BYTE EntryType; // 0xC1
    BYTE SecondaryFlags;
    wchar_t FileName[15];
};

union EXFAT_DIRENTRY
{
    EXFAT_DIRENTRY_GENERIC Generic;
    EXFAT_DIRENTRY_BITMAP Bitmap;
    EXFAT_DIRENTRY_UPCASE UpCase;
    EXFAT_DIRENTRY_FILE File;
    EXFAT_DIRENTRY_STREXT StrExt;
    EXFAT_DIRENTRY_FILENAME FileName;
};
#pragma pack(pop, exfat_h)

#define EXFAT_NO_FAT_CHAIN 0x02 // bit from EXFAT_DIRENTRY_STREXT::SecondaryFlags

#define EXFAT_ATTR_ARCHIVE 0x20
#define EXFAT_ATTR_DIRECTORY 0x10
#define EXFAT_ATTR_SYSTEM 0x04
#define EXFAT_ATTR_HIDDEN 0x02
#define EXFAT_ATTR_READONLY 0x01

// ExFAT FAT entry points
#define EXFAT_ENTRY_BITMAP 0x01  // 0x81 - Allocation Bitmap Directory Entry
#define EXFAT_ENTRY_UPCASE 0x02  // 0x82 - UP-Case Table Directory Entry
#define EXFAT_ENTRY_VLABEL 0x03  // 0x83 - Volume Label Directory Entry
#define EXFAT_ENTRY_FILEDIR 0x05 // 0x85 - File Directory Entry
#define EXFAT_ENTRY_VGUID 0x20   // 0xA0 - Volume GUID Directory Entry
#define EXFAT_ENTRY_TEXFAT 0x21  // 0xA1 - TexFAT Padding Directory Entry
#define EXFAT_ENTRY_STRMEXT 0x40 // 0xC0 - Stream Extension Directory Entry
#define EXFAT_ENTRY_NAMEEXT 0x41 // 0xC1 - File Name Extension Directory Entry
#define EXFAT_ENTRY_WCEACT 0x62  // 0xE2 - Windows CE Access Control Table Directory Entry

#define EXFAT_ENTRY_INUSE 0x80 // In Use bit used in combination with other EXFAT_ENTRY_*

#define RUN_DELTA 1024 // size of buffer enlargement for data runs encoding

template <typename CHAR>
class CRunsBuffer; // defined in fat.h

template <typename CHAR>
class CExFATSnapshot : public CSnapshot<CHAR>
{
public:
    CExFATSnapshot(CVolume<CHAR>* volume);
    virtual ~CExFATSnapshot() { Free(); }
    virtual BOOL Update(CSnapshotProgressDlg* progress, DWORD udFlags, CLUSTER_MAP_I** clusterMap);
    virtual void Free();

private:
    BOOL OpenFAT();
    DWORD GetFATItem(DWORD index);
    QWORD GetFATItems() { return FATItems; }
    BOOL LoadDirectoryTree(FILE_RECORD_I<CHAR>* parent, DWORD dirFirstCluster, QWORD dirFileSize, BOOL dirClusterChainInFAT, BOOL isRootDirectory, CHAR* tracePath);
    DWORD GetClusterChainLen(DWORD index);                                                                    // return number of clusters in cluster chain starting at 'index'
    BOOL LoadClusterChain(BYTE** buff, DWORD firstCluster, QWORD fileSize, BOOL chainInFAT, QWORD* clusters); // allocate 'buffer' and read clusters based on cluster chain starting at 'index', returns number of 'clusters'
    DWORD ConvertExFATAttr(DWORD exfatattr);
    void DosTimeToFileTime(DWORD dosTime, BYTE dosTime10ms, FILETIME* ft);
    void FilterZeroFiles(FILE_RECORD_I<CHAR>* record);
    BOOL FilterExistingDirectories(FILE_RECORD_I<CHAR>* record);
    BOOL FilterEmptyDirectories(FILE_RECORD_I<CHAR>* record);
    BOOL AddVirtualDirs(FILE_RECORD_I<CHAR>** DeletedFiles);
    BOOL AddFileOrDir(FILE_RECORD_I<CHAR>* dir, const wchar_t* name, int nameLen, DWORD exfatAttr, DWORD flags,
                      const FILETIME* timeCreation, const FILETIME* timeLastAccess, const FILETIME* timeLastWrite,
                      DWORD firstCluster, QWORD fileSize, QWORD validSize, FILE_RECORD_I<CHAR>** newRec); // 'attr' from EXFAT_ATTR_* family
    DWORD AddDeletedFiles(FILE_RECORD_I<CHAR>* dir, FILE_RECORD_I<CHAR>* deletedfiles);
    void FreeRecord2(FILE_RECORD_I<CHAR>* record);
    BOOL EncodeClusterChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file);
    BOOL EncodeClusterChains(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* dir);
    BOOL LoadClusterBitmap(CClusterBitmap* clusterBitmap);
    void DrawDeletedFile(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap);
    DWORD GetFileCondition(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap);
    BOOL EstimateFileDamage(const FILE_RECORD_I<CHAR>* deletedFiles, CLUSTER_MAP_I** clusterMap);
    BOOL GetLostClustersMap(CClusterBitmap* clusterBitmap, CLUSTER_MAP_I* clusterMap);

    // exFAT FAT table could contain 2^32 items (cluster),
    // it is not possible to allocate/read it whole as we do with FAT32 volumes
    DWORD FATItems;          // total number of DWORD items in FAT
    DWORD* FATHead;          // buffer with first clusters of FAT, up to MAX_FAT_HEAD_SIZE size
    DWORD FATHeadItems;      // number of DWORD items in FATHead
    DWORD* FATCache;         // read buffer for FAT sectors beyond FATHead
    DWORD FATCacheItems;     // total number of DWORD items in FATCache
    DWORD FATCacheFirstItem; // zero based index of first DWORD item in FATCache (0=invalid index)

    DWORD VirtualDirsCount;

    EXFAT_DIRENTRY_BITMAP DirEntryBitmap;
    EXFAT_DIRENTRY_UPCASE DirEntryUpCase;
    FILE_RECORD_I<CHAR>* BitmapFileRecord;
};

template <typename CHAR>
CExFATSnapshot<CHAR>::CExFATSnapshot(CVolume<CHAR>* volume)
    : CSnapshot<CHAR>(volume)
{
    FATItems = 0;
    FATHead = NULL;
    FATHeadItems = 0;
    FATCache = NULL;
    FATCacheItems = 0;
    FATCacheFirstItem = 0;
    VirtualDirsCount = 0;
    BitmapFileRecord = NULL;
    ZeroMemory(&DirEntryBitmap, sizeof(DirEntryBitmap));
    ZeroMemory(&DirEntryUpCase, sizeof(DirEntryUpCase));
}

template <typename CHAR>
void CExFATSnapshot<CHAR>::Free()
{
    CALL_STACK_MESSAGE1("CExFATSnapshot::Free()");

    if (this->Root != NULL)
    {
        // {All Deleted Files} and {Metafiles} free separately
        if (VirtualDirsCount > 0)
        {
            delete this->FreeRecord(this->Root->DirItems[this->Root->NumDirItems - VirtualDirsCount].Record);
            if (VirtualDirsCount == 2)
                FreeRecord2(this->Root->DirItems[this->Root->NumDirItems - 1].Record);
            this->Root->NumDirItems -= VirtualDirsCount;
        }

        // rest
        FreeRecord2(this->Root);
        this->Root = NULL;
    }
    VirtualDirsCount = 0;

    if (FATHead != NULL)
    {
        free(FATHead);
        FATHead = NULL;
        FATHeadItems = 0;
    }
    if (FATCache != NULL)
    {
        free(FATCache);
        FATCache = NULL;
        FATCacheItems = 0;
        FATCacheFirstItem = 0;
    }

    ZeroMemory(&DirEntryBitmap, sizeof(DirEntryBitmap));
    ZeroMemory(&DirEntryUpCase, sizeof(DirEntryUpCase));
    BitmapFileRecord = NULL;
}

static WORD UpdateEntriesChecksum(WORD checksum, EXFAT_DIRENTRY* item, BOOL primary)
{
    CALL_STACK_MESSAGE_NONE

    const int ITEM_SIZE = sizeof(*item);
#ifdef _DEBUG
    if (sizeof(*item) != ITEM_SIZE)
        TRACE_C("Wrong EXFAT_DIRENTRY size!");
#endif
    for (int i = 0; i < ITEM_SIZE; i++)
    {
        if (primary && (i == 2 || i == 3))
            continue;
        // deleted entries (without EXFAT_ENTRY_INUSE bit set) keeps the original checksum,
        // computed while INUSE bit was set, so we will calculate checksum with this bit set
        BYTE b = ((BYTE*)item)[i];
        if (i == 0)
            b |= EXFAT_ENTRY_INUSE;
        checksum = ((((checksum << 15) & 0xFFFF) | ((checksum >> 1) & 0xFFFF)) + b) & 0xFFFF;
    }
    return checksum;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::LoadDirectoryTree(FILE_RECORD_I<CHAR>* parent, DWORD dirFirstCluster, QWORD dirFileSize, BOOL dirClusterChainInFAT, BOOL isRootDirectory, CHAR* tracePath)
{
    CALL_STACK_MESSAGE_NONE

    BYTE* dirClusterChain = NULL;
    QWORD dirClusterCount;

#ifdef TRACE_ENABLE
    int tracePathLen = (int)String<CHAR>::StrLen(tracePath);
    if (parent->FileNames != NULL)
        String<CHAR>::StrCat_s(tracePath, MAX_PATH, parent->FileNames->FNName);
    CHAR* backslash = String<CHAR>::NewFromASCII("\\");
    String<CHAR>::StrCat_s(tracePath, MAX_PATH, backslash);
    delete backslash;
#endif

    if (!LoadClusterChain(&dirClusterChain, dirFirstCluster, dirFileSize, dirClusterChainInFAT, &dirClusterCount))
        return FALSE;

    BOOL ret = TRUE;

    // fast initial scan: we will find count of entries and try to recognize invalid directory structure,
    // because deleted directory could point to cluster already used by some existing or deleted file
    DWORD entriesCount = 0;
    EXFAT_DIRENTRY* iter = (EXFAT_DIRENTRY*)dirClusterChain;
    BOOL exit = FALSE;
    do
    {
#ifdef FAST_ENTRIES_SCAN
        if (iter->Generic.EntryType == 0)
            break;
#endif
        BOOL inUse = (iter->Generic.EntryType & EXFAT_ENTRY_INUSE) != 0;
        switch (iter->Generic.EntryType & ~EXFAT_ENTRY_INUSE)
        {
        // following entries are valid only inside root directory
        case EXFAT_ENTRY_BITMAP: // 0x01 || 0x81 - Allocation Bitmap Directory Entry
        case EXFAT_ENTRY_UPCASE: // 0x02 || 0x82 - UP-Case Table Directory Entry
        case EXFAT_ENTRY_VLABEL: // 0x03 || 0x83 - Volume Label Directory Entry
        case EXFAT_ENTRY_VGUID:  // 0x20 || 0xA0 - Volume GUID Directory Entry
        {
            if (!isRootDirectory)
            {
                TRACE_IW(L"LoadDirectoryTree() entry 0x" << std::hex << (int)iter->Generic.EntryType << std::dec << L" found outside Root directory! Path: " << CWStr(tracePath).c_str());
                exit = TRUE;
            }
            break;
        }

        case EXFAT_ENTRY_TEXFAT:  // 0x21 || 0xA1 - TexFAT Padding Directory Entry
        case EXFAT_ENTRY_WCEACT:  // 0x62 || 0xE2 - Windows CE Access Control Table Directory Entry
        case EXFAT_ENTRY_STRMEXT: // 0x40 || 0xC0 - Stream Extension Directory Entry
        case EXFAT_ENTRY_NAMEEXT: // 0x41 || 0xC1 - File Name Extension Directory Entry
        {
            break;
        }

        case EXFAT_ENTRY_FILEDIR: // 0x05 || 0x85 - File Directory Entry
        {
            entriesCount++;
            iter += iter->File.SecondaryCount;
            break;
        }

        default:
        {
            TRACE_IW(L"LoadDirectoryTree() unknown entry 0x" << std::hex << (int)iter->Generic.EntryType << std::dec << L" Path: " << CWStr(tracePath).c_str());
            exit = TRUE;
        }
        }
        iter++;
    } while ((BYTE*)iter < dirClusterChain + dirClusterCount * this->Volume->BytesPerCluster && !exit);

    if (isRootDirectory)
        entriesCount += 2; // 2 for {All Deleted Files} and {Metafiles}

    if (entriesCount > 0)
    {
        // FIXME - allocation
        parent->DirItems = (DIR_ITEM_I<CHAR>*)malloc(entriesCount * sizeof(DIR_ITEM_I<CHAR>));

        iter = (EXFAT_DIRENTRY*)dirClusterChain;
        exit = FALSE;
        do
        {
#ifdef FAST_ENTRIES_SCAN
            if (iter->Generic.EntryType == 0)
                break;
#endif
            BOOL inUse = (iter->Generic.EntryType & EXFAT_ENTRY_INUSE) != 0;
            switch (iter->Generic.EntryType & ~EXFAT_ENTRY_INUSE)
            {
            case EXFAT_ENTRY_BITMAP: // 0x01 || 0x81 - Bitmap Directory Entry
            {
                if (inUse)
                {
                    if (DirEntryBitmap.EntryType == 0)
                        DirEntryBitmap = iter->Bitmap;
                    else
                        TRACE_E("LoadDirectoryTree() ignoring another Allocation Bitmap Table");
                }
                else
                {
                    TRACE_E("LoadDirectoryTree() ignoring deleted Allocation Bitmap Table");
                }
                break;
            }

            case EXFAT_ENTRY_UPCASE: // 0x02 || 0x82 - Up-Case Directory Entry
            {
                if (inUse)
                {
                    if (DirEntryUpCase.EntryType == 0)
                        DirEntryUpCase = iter->UpCase;
                    else
                        TRACE_E("LoadDirectoryTree() ignoring another Up-Case Table");
                }
                else
                {
                    TRACE_E("LoadDirectoryTree() ignoring deleted Up-Case Table");
                }
                break;
            }

            case EXFAT_ENTRY_FILEDIR: // 0x05 || 0x85 - File Directory Entry
            {
                wchar_t name[256];
                wchar_t* nameiter = name;
                int namelen = 0;
                DWORD exfatattr = iter->File.FileAttributes;
                FILETIME timeCreation = {0};
                FILETIME timeLastAccess = {0};
                FILETIME timeLastWrite = {0};
                DosTimeToFileTime(iter->File.Created, iter->File.Created10ms, &timeCreation);
                DosTimeToFileTime(iter->File.LastAccessed, 0, &timeLastAccess);
                DosTimeToFileTime(iter->File.LastModified, iter->File.LastModified10ms, &timeLastWrite);
                BOOL isDir = (iter->File.FileAttributes & EXFAT_ATTR_DIRECTORY) != 0;
                WORD setChecksum = iter->File.SetChecksum; // stored checksum across entries set
                WORD ourChecksum = 0;
                ourChecksum = UpdateEntriesChecksum(ourChecksum, iter, TRUE);

                QWORD fileSize = 0;
                QWORD validSize = 0;
                DWORD firstCluster = 0;
                BYTE secondaryCount = iter->File.SecondaryCount;
                BOOL chainInFAT = FALSE;
                for (int i = 0; i < secondaryCount; i++)
                {
                    iter++;
                    switch (iter->Generic.EntryType & ~EXFAT_ENTRY_INUSE)
                    {
                    case EXFAT_ENTRY_STRMEXT: // 0x40 || 0xC0 - Stream Extension Directory Entry
                    {
                        namelen = iter->StrExt.NameLength;
                        fileSize = iter->StrExt.DataLength;
                        validSize = iter->StrExt.ValidDataLength;
#if _DEBUG
                        if (validSize > fileSize)
                            TRACE_EW(L"LoadDirectoryTree() validSize > fileSize for " << name);
                        if (validSize != fileSize)
                            TRACE_IW(L"LoadDirectoryTree() validSize != fileSize for " << name);
#endif
                        firstCluster = iter->StrExt.FirstCluster;
                        chainInFAT = (iter->StrExt.SecondaryFlags & EXFAT_NO_FAT_CHAIN) == 0;
                        ourChecksum = UpdateEntriesChecksum(ourChecksum, iter, FALSE);
                        break;
                    }

                    case EXFAT_ENTRY_NAMEEXT: // 0x41 || 0xC1 - File Name Extension Directory Entry
                    {
                        memcpy(nameiter, iter->FileName.FileName, 30);
                        nameiter += 15;
                        ourChecksum = UpdateEntriesChecksum(ourChecksum, iter, FALSE);
                        break;
                    }

                    default:
                    {
                        // don't update checksum so item is ignored
                        TRACE_IW(L"LoadDirectoryTree() Unknown secondary entry under '" << name << L"' entry! Path: " << CWStr(tracePath).c_str());
                        break;
                    }
                    }
                }
                name[namelen] = 0;

                // use only items with valid checksum, ignore others
                if (ourChecksum == setChecksum)
                {
                    FILE_RECORD_I<CHAR>* alldel;

                    BOOL ignoreItem = (!isDir && inUse && !(this->UdFlags & UF_SHOWEXISTING));
                    if (!ignoreItem)
                    {
                        DWORD flags = 0;
                        if (!inUse)
                            flags |= FR_FLAGS_DELETED;
                        if (chainInFAT)
                            flags |= FR_FLAGS_CHAININFAT;
                        AddFileOrDir(parent, name, namelen, exfatattr, flags, &timeCreation, &timeLastAccess, &timeLastWrite,
                                     firstCluster, fileSize, validSize, &alldel);
                    }

                    if (isDir)
                    {
                        if (!LoadDirectoryTree(alldel, firstCluster, fileSize, chainInFAT, FALSE, tracePath))
                        {
                            if (this->Progress->GetWantCancel())
                            {
                                exit = TRUE;
                                ret = FALSE;
                            }
                        }
                    }
                }
                else
                {
                    TRACE_IW(L"LoadDirectoryTree() invalid checksum on '" << name << L"' entry! Path: " << CWStr(tracePath).c_str());
                }

                break;
            }
            }
            iter++;
        } while ((BYTE*)iter < dirClusterChain + dirClusterCount * this->Volume->BytesPerCluster && !exit);
    }
    delete dirClusterChain;
#ifdef TRACE_ENABLE
    tracePath[tracePathLen] = 0;
#endif
    return ret;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::EncodeClusterChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CExFATSnapshot::EncodeClusterChain()");

    DWORD cluster = file->Streams->FirstLCN;

    //TRACE_I("EncodeClusterChain() file: " << file->FileNames->Name);

    // encode cluster chain in sequence of pairs [lcn, length]
    if (file->Flags & FR_FLAGS_CHAININFAT)
    {
        DWORD lcn = cluster;
        DWORD length = 1;
        do
        {
            if (cluster >= GetFATItems())
            {
                TRACE_EW(L"EncodeClusterChain(): cluster >= ExFAT items for " << CWStr(file->FileNames->FNName).c_str());
                break;
            }
            DWORD next = GetFATItem(cluster);
            if (next == cluster + 1)
            {
                length++; // merge continuous sequence of clusters into one data run
            }
            else
            {
                //TRACE_I("EncodeClusterChain() data run lcn: " << lcn << " length: " << length);
                if (!tmpRunsBuffer->EncodeRun(lcn, length))
                    return FALSE;
                lcn = next;
                length = 1;
            }
            cluster = next;
        } while (cluster != 0xFFFFFFFF && cluster != 0);
    }
    else
    {
        DWORD length = (DWORD)((file->Streams->DSSize + this->Volume->BytesPerCluster - 1) / this->Volume->BytesPerCluster);
        //TRACE_I("EncodeClusterChain() data run lcn: " << cluster << " length: " << length);
        if (!tmpRunsBuffer->EncodeRun(cluster, length))
            return FALSE;
    }
    return TRUE;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::EncodeClusterChains(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CExFATSnapshot::EncodeClusterChains()");

    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = dir->DirItems[i].Record;
        if (r->IsDir)
        {
            EncodeClusterChains(tmpRunsBuffer, r);
        }
        else
        {
            DATA_STREAM_I<CHAR>* stream = r->Streams;

            // for zero files we can end here
            if (stream->DSSize == 0)
                continue;

            // encode cluster chain into NTFS format
            tmpRunsBuffer->Clear();
            if (!EncodeClusterChain(tmpRunsBuffer, r))
                return FALSE;

            DATA_POINTERS* ptrs = new DATA_POINTERS;
            BYTE* runs = new BYTE[tmpRunsBuffer->RunsLen + 1]; // +1 for terminator
            if (ptrs == NULL || runs == NULL)
            {
                delete stream;
                delete ptrs;
                delete[] runs;
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            }
            stream->FirstLCN = 0; // not needed anymore, encoded in data runs
            stream->Ptrs = ptrs;
            ptrs->StartVCN = 0;
            ptrs->LastVCN = (stream->DSSize - 1) / this->Volume->BytesPerCluster;
            memcpy(runs, tmpRunsBuffer->RunsBuffer, tmpRunsBuffer->RunsLen + 1);
            ptrs->Runs = runs;
            ptrs->RunsSize = tmpRunsBuffer->RunsLen + 1;
        }
    }

    return TRUE;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::AddVirtualDirs(FILE_RECORD_I<CHAR>** DeletedFiles)
{
    CALL_STACK_MESSAGE1("CExFATSnapshot::AddVirtualDirs()");

    FILE_RECORD_I<CHAR>* deletedfiles;
    FILE_RECORD_I<CHAR>* metafiles;

    // add virtual directories {All Deleted Files} and {Metafiles} into root
    if (!AddFileOrDir(this->Root, String<wchar_t>::LoadStr(IDS_ALLDELETEDFILES), -1, EXFAT_ATTR_DIRECTORY,
                      FR_FLAGS_VIRTUALDIR, NULL, NULL, NULL, 0, 0, 0, &deletedfiles))
    {
        return FALSE;
    }
    VirtualDirsCount++;
    if (!AddFileOrDir(this->Root, String<wchar_t>::LoadStr(IDS_METAFILES), -1, EXFAT_ATTR_DIRECTORY,
                      FR_FLAGS_VIRTUALDIR, NULL, NULL, NULL, 0, 0, 0, &metafiles))
    {
        return FALSE;
    }
    VirtualDirsCount++;

    // fill {All Deleted Files}
    this->Root->NumDirItems -= VirtualDirsCount;             // don't add virtual dirs to {All Deleted Files}
    DWORD allFilesCount = AddDeletedFiles(this->Root, NULL); // get total count of virtual files
    deletedfiles->DirItems = new DIR_ITEM_I<CHAR>[allFilesCount];
    if (deletedfiles->DirItems == NULL)
    {
        this->Root->NumDirItems += VirtualDirsCount;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    AddDeletedFiles(this->Root, deletedfiles);
    this->Root->NumDirItems += VirtualDirsCount;

    // fill {Metafiles}
    metafiles->NumDirItems = 0;
    metafiles->DirItems = new DIR_ITEM_I<CHAR>[2]; //4
    if (metafiles->DirItems == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    FILE_RECORD_I<CHAR> /**boot, *fat, */ *bitmap, *uppercase;
    if (/*!AddFileOrDir(metafiles, L"$Boot", -1, 0, FALSE, NULL, NULL, NULL, FALSE, 0, 0, 0, &boot) ||  // these metafiles are currently not accessible through CStreamReader
      !AddFileOrDir(metafiles, L"$FAT", -1, 0, FALSE, NULL, NULL, NULL, FALSE, 0, 0, 0, &fat) || */
        !AddFileOrDir(metafiles, L"$Bitmap", -1, 0, FR_FLAGS_METAFILE, NULL, NULL, NULL, DirEntryBitmap.FirstCluster, DirEntryBitmap.DataLength, DirEntryBitmap.DataLength, &bitmap) ||
        !AddFileOrDir(metafiles, L"$Uppercase", -1, 0, FR_FLAGS_METAFILE, NULL, NULL, NULL, DirEntryUpCase.FirstCluster, DirEntryUpCase.DataLength, DirEntryUpCase.DataLength, &uppercase))
    {
        return FALSE;
    }

    BitmapFileRecord = bitmap;

    CRunsBuffer<CHAR> tmpRunsBuffer;
    EncodeClusterChains(&tmpRunsBuffer, metafiles);

    *DeletedFiles = deletedfiles;
    return TRUE;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::Update(CSnapshotProgressDlg* progress, DWORD udFlags, CLUSTER_MAP_I** clusterMap)
{
    CALL_STACK_MESSAGE2("CExFATSnapshot::Update(, , 0x%X, )", udFlags);

    TRACE_I("CExFATSnapshot::Update(), udFlags=" << udFlags);

    if (udFlags & UF_GETLOSTCLUSTERMAP)
        udFlags |= UF_ESTIMATEDAMAGE;

    // initialization
    this->Progress = progress;

    this->UdFlags = udFlags;
    if (clusterMap != NULL)
        *clusterMap = NULL;

    if (!this->Volume->IsOpen())
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTOPEN);
    if (this->Volume->Type != vtExFAT)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTFAT);

    // init
    Free();

    this->Progress->SetProgress(-1);

    // open FAT
    this->Progress->SetProgressText(IDS_LOADINGFAT);
    if (!OpenFAT())
        return FALSE;

    this->Root = new FILE_RECORD_I<CHAR>;
    if (this->Root == NULL)
    {
        Free();
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    this->Root->IsDir = TRUE;

    this->Progress->SetProgressText(IDS_READINGDIRS);
#ifdef TRACE_ENABLE
    CHAR tracePath[MAX_PATH] = {0};
#else
    CHAR* tracePath = NULL;
#endif
    if (!LoadDirectoryTree(this->Root, this->Volume->ExFATBoot.RootDirFirstCluster, -1, TRUE, TRUE, tracePath))
    {
        Free();
        return FALSE;
    }

#ifdef MEASURE_READING_PERFORMANCE
    LARGE_INTEGER f;
    QueryPerformanceFrequency(&f);
    double time = (double)(ReadingDirEntriesTicks.QuadPart * 1000) / f.QuadPart;
    TRACE_I("Reading directory entries: " << time << " ms");
    time = (double)(ticks.QuadPart * 1000) / f.QuadPart;
    TRACE_I("ticks: " << time << " ms");
#endif

    this->Progress->SetProgressText(IDS_ANALYZINGDIRS);

    // filter undeleted (existing) directories (files are filtered during reading)
    if (!(this->UdFlags & UF_SHOWEXISTING))
        FilterExistingDirectories(this->Root);

    // encode cluster chains into data-runs
    CRunsBuffer<CHAR> tmpRunsBuffer;
    if (!EncodeClusterChains(&tmpRunsBuffer, this->Root))
    {
        Free();
        return FALSE;
    }

    // filter files with zero size
    if (!(this->UdFlags & UF_SHOWZEROFILES))
        FilterZeroFiles(this->Root);

    // filter empty directories
    if (!(this->UdFlags & UF_SHOWEMPTYDIRS))
        FilterEmptyDirectories(this->Root);

    // add {All Deleted Files} and {Metafiles} virtual directories
    FILE_RECORD_I<CHAR>* deletedFiles;
    AddVirtualDirs(&deletedFiles);

    // file damage estimation
    if (this->UdFlags & UF_ESTIMATEDAMAGE)
        EstimateFileDamage(deletedFiles, clusterMap);

    // we can destroy {Metafiles} virtual directory now if not needed anymore
    if ((this->UdFlags & UF_SHOWMETAFILES) == 0 && VirtualDirsCount == 2)
    {
        FreeRecord2(this->Root->DirItems[this->Root->NumDirItems - 1].Record);
        this->Root->NumDirItems -= 1;
        VirtualDirsCount--;
    }

    return TRUE;
}

template <typename CHAR>
DWORD CExFATSnapshot<CHAR>::GetClusterChainLen(DWORD index)
{
    CALL_STACK_MESSAGE_NONE
    DWORD len = 1;
    DWORD i = index;
    while (GetFATItem(i) != 0xFFFFFFFF && GetFATItem(i) != 0)
    {
        i = GetFATItem(i);
        if (i < 2 || i >= GetFATItems())
        {
            TRACE_E("ExFAT contains wrong index in cluster chain!");
            return len;
        }
        len++;
    }
    return len;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::LoadClusterChain(BYTE** buff, DWORD firstCluster, QWORD fileSize, BOOL chainInFAT, QWORD* clusters)
{
    CALL_STACK_MESSAGE_NONE
    if (*buff != NULL)
    {
        TRACE_E("Buffer is not NULL!");
        return FALSE;
    }

    if (this->Progress->GetWantCancel())
        return FALSE;

    // get total chain length in clusters
    // if it is fragmented, we must get cluster sequence from FAT
    // otherwise clusters are stored in continuous sequence
    QWORD chainLen;
    if (chainInFAT)
        chainLen = GetClusterChainLen(firstCluster);
    else
        chainLen = ((fileSize + this->Volume->BytesPerCluster - 1) / this->Volume->BytesPerCluster);

    *buff = new BYTE[(size_t)(chainLen * this->Volume->BytesPerCluster)];
    if (*buff == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    ZeroMemory(*buff, (size_t)(chainLen * this->Volume->BytesPerCluster));

    DWORD i = firstCluster;
    QWORD len = chainLen;
    BYTE* iter = *buff;
#ifdef MEASURE_READING_PERFORMANCE
    LARGE_INTEGER t1, t2;
    QueryPerformanceCounter(&t1);
#endif
    while (len > 0)
    {
#ifdef FAST_ENTRIES_SCAN
        if (i == firstCluster)
        {
            // read some clusters
            const DWORD read_sectors = min(8, this->Volume->SectorsPerCluster); // 8 sectors seems to be fastest during my tests
            this->Volume->ReadSectors(iter,
                                      this->Volume->ExFATBoot.ClusterHeapOffset + (i - 2) * this->Volume->SectorsPerCluster,
                                      read_sectors);
            // when last item is 0, exist
            if (*(iter + this->Volume->BytesPerSector * read_sectors - 32) == 0)
                break;
            // otherwise read rest of cluster
            if (this->Volume->SectorsPerCluster > read_sectors)
            {
                this->Volume->ReadSectors(iter + this->Volume->BytesPerSector * read_sectors,
                                          this->Volume->ExFATBoot.ClusterHeapOffset + (i - 2) * this->Volume->SectorsPerCluster + read_sectors,
                                          this->Volume->SectorsPerCluster - read_sectors);
            }
        }
        else
        {
            this->Volume->ReadSectors(iter,
                                      this->Volume->ExFATBoot.ClusterHeapOffset + (i - 2) * this->Volume->SectorsPerCluster,
                                      this->Volume->SectorsPerCluster);
        }
#else
        Volume->ReadSectors(iter,
                            Volume->ExFATBoot.ClusterHeapOffset + (i - 2) * Volume->SectorsPerCluster,
                            Volume->SectorsPerCluster);
#endif
        iter += this->Volume->SectorsPerCluster * this->Volume->BytesPerSector;
        len--;
        if (chainInFAT)
            i = GetFATItem(i); // go to the next cluster based on FAT
        else
            i++; // file is not fragmented
    }
#ifdef MEASURE_READING_PERFORMANCE
    QueryPerformanceCounter(&t2);
    ReadingDirEntriesTicks.QuadPart += (t2.QuadPart - t1.QuadPart);
#endif
    if (clusters != NULL)
        *clusters = chainLen;

    return TRUE;
}

template <typename CHAR>
inline DWORD CExFATSnapshot<CHAR>::ConvertExFATAttr(DWORD exfatattr)
{
    CALL_STACK_MESSAGE_NONE
    DWORD attr = 0;
    if (exfatattr & EXFAT_ATTR_ARCHIVE)
        attr |= FILE_ATTRIBUTE_ARCHIVE;
    if (exfatattr & EXFAT_ATTR_DIRECTORY)
        attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (exfatattr & EXFAT_ATTR_SYSTEM)
        attr |= FILE_ATTRIBUTE_SYSTEM;
    if (exfatattr & EXFAT_ATTR_HIDDEN)
        attr |= FILE_ATTRIBUTE_HIDDEN;
    if (exfatattr & EXFAT_ATTR_READONLY)
        attr |= FILE_ATTRIBUTE_READONLY;
    return attr;
}

template <typename CHAR>
void CExFATSnapshot<CHAR>::DosTimeToFileTime(DWORD dosTime, BYTE dosTime10ms, FILETIME* ft)
{
    CALL_STACK_MESSAGE_NONE
    FILETIME t;
    DosDateTimeToFileTime(HIWORD(dosTime), LOWORD(dosTime), &t);
    LocalFileTimeToFileTime(&t, &t);
    QWORD ns100; // 100-nanosecond intervals since January 1, 1601 (UTC)
    ns100 = MAKEQWORD(t.dwLowDateTime, t.dwHighDateTime);
    ns100 += dosTime10ms * 10 * 1000000 / 100; // *10:ms *1000000:ns /100:100_nanosecond_intervals
    t.dwLowDateTime = LODWORD(ns100);
    t.dwHighDateTime = HIDWORD(ns100);
    *ft = t;
}

template <typename CHAR>
void CExFATSnapshot<CHAR>::FilterZeroFiles(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CExFATSnapshot::FilterZeroFiles()");

    DWORD j = 0;
    for (DWORD i = 0; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = record->DirItems[i].Record;
        if (!r->IsDir && r->Streams != NULL && r->Streams->DSSize == 0)
        {
            delete this->FreeRecord(r);
        }
        else
        {
            if (r->IsDir)
                FilterZeroFiles(r);
            record->DirItems[j++] = record->DirItems[i];
        }
    }
    record->NumDirItems = j;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::FilterExistingDirectories(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CExFATSnapshot::FilterExistingDirectories()");
    // remove existing directories which contains only existing files and directories
    // (single deleted subdirectory of subfile means we cannot remove this directory)
    DWORD j = 0;
    for (DWORD i = 0; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = record->DirItems[i].Record;
        if (r->IsDir &&
            FilterExistingDirectories(r) &&
            !(r->Flags & FR_FLAGS_DELETED))
        {
            delete this->FreeRecord(r);
        }
        else
        {
            record->DirItems[j++] = record->DirItems[i];
        }
    }
    record->NumDirItems = j;
    return j == 0; // returns TRUE when directory was removed
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::FilterEmptyDirectories(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CExFATSnapshot::FilterEmptyDirectories()");

    // remove directories which contains only further directories (doesn't contain files)
    DWORD j = 0;
    for (DWORD i = 0; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = record->DirItems[i].Record;
        if (r->IsDir && FilterEmptyDirectories(r))
            delete this->FreeRecord(r);
        else
            record->DirItems[j++] = record->DirItems[i];
    }
    record->NumDirItems = j;
    return j == 0;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::AddFileOrDir(FILE_RECORD_I<CHAR>* dir, const wchar_t* name, int nameLen, DWORD exfatAttr, DWORD flags,
                                        const FILETIME* timeCreation, const FILETIME* timeLastAccess, const FILETIME* timeLastWrite,
                                        DWORD firstCluster, QWORD fileSize, QWORD validSize, FILE_RECORD_I<CHAR>** newRec)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CExFATSnapshot::AddFileOrDir()");

    *newRec = new FILE_RECORD_I<CHAR>;
    FILE_RECORD_I<CHAR>* rec = *newRec;
    FILE_NAME_I<CHAR>* fn = new FILE_NAME_I<CHAR>;
    if (nameLen == -1)
        nameLen = (int)wcslen(name);
    fn->FNName = String<CHAR>::NewFromUnicode(name, nameLen);
    if (rec == NULL || fn == NULL || fn->FNName == NULL)
    {
        if (rec != NULL)
            delete rec;
        if (fn != NULL && fn->FNName != NULL)
            delete fn->FNName;
        if (fn != NULL)
            delete fn;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    rec->Attr = ConvertExFATAttr(exfatAttr);
    rec->Flags = flags;
    rec->IsDir = (rec->Attr & FILE_ATTRIBUTE_DIRECTORY) != 0;

    // for file store number of first cluster and size (streams and duplicate searching)
    if (!rec->IsDir)
    {
        DATA_STREAM_I<CHAR>* stream = new DATA_STREAM_I<CHAR>;
        if (stream == NULL)
        {
            delete rec;
            delete fn->FNName;
            delete fn;
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        }
        rec->Streams = stream;
        rec->Streams->DSSize = fileSize;
        rec->Streams->DSValidSize = validSize;
        rec->Streams->FirstLCN = firstCluster;
    }

    FILETIME currentTime;
    if (timeCreation == NULL || timeLastAccess == NULL || timeLastWrite == NULL)
    {
        SYSTEMTIME st;
        GetSystemTime(&st);
        SystemTimeToFileTime(&st, &currentTime);
    }

    rec->TimeCreation = (timeCreation != NULL) ? *timeCreation : currentTime;
    rec->TimeLastAccess = (timeLastAccess != NULL) ? *timeLastAccess : currentTime;
    rec->TimeLastWrite = (timeLastWrite != NULL) ? *timeLastWrite : currentTime;

    rec->Flags &= ~FR_FLAGS_CONDITION_MASK;
    BOOL deleted = rec->Flags & FR_FLAGS_DELETED;
    if (!rec->IsDir && !deleted)
        rec->Flags |= FR_FLAGS_CONDITION_GOOD;
    else
        rec->Flags |= FR_FLAGS_CONDITION_UNKNOWN;

    rec->FileNames = fn;
    DIR_ITEM_I<CHAR>* di = dir->DirItems + dir->NumDirItems;
    di->FileName = fn;
    di->Record = rec;
    dir->NumDirItems++;
    return TRUE;
}

template <typename CHAR>
void CExFATSnapshot<CHAR>::FreeRecord2(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CExFATSnapshot::FreeRecord2()");
    // standard directory
    for (DWORD i = 0; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = record->DirItems[i].Record;
        if (r->IsDir)
            FreeRecord2(r);
        else
            delete this->FreeRecord(r);
    }
    delete this->FreeRecord(record);
}

template <typename CHAR>
DWORD CExFATSnapshot<CHAR>::AddDeletedFiles(FILE_RECORD_I<CHAR>* dir, FILE_RECORD_I<CHAR>* deletedfiles)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CExFATSnapshot::AddDeletedFiles(, )");

    DWORD count = 0;
    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = dir->DirItems[i].Record;
        if (r->IsDir)
        {
            count += AddDeletedFiles(r, deletedfiles);
        }
        else if (r->Flags & FR_FLAGS_DELETED)
        {
            if (deletedfiles == NULL)
            {
                count++;
            }
            else
            {
                deletedfiles->DirItems[deletedfiles->NumDirItems] = dir->DirItems[i];
                deletedfiles->NumDirItems++;
            }
        }
    }
    return count;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::LoadClusterBitmap(CClusterBitmap* clusterBitmap)
{
    CALL_STACK_MESSAGE1("CExFATSnapshot::LoadClusterBitmap()");

    DATA_STREAM_I<CHAR>* stream = BitmapFileRecord->Streams;

    // allocate buffer
    const int bufsize = 256 * 1024;
    BYTE* buffer = new BYTE[bufsize];
    if (buffer == NULL)
    {
        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        return FALSE;
    }
    int bufclusters = bufsize / this->Volume->BytesPerCluster;

    // allocate memory for bitmap
    clusterBitmap->Free();
    if (!clusterBitmap->AllocateBitmap((size_t)stream->DSSize))
    {
        delete[] buffer;
        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEMBITMAP);
        return FALSE;
    }

    // load bitmap to memory
    CStreamReader<CHAR> reader;
    reader.Init(this->Volume, stream);
    QWORD bytesleft = stream->DSSize;
    QWORD clustersleft = (bytesleft - 1) / this->Volume->BytesPerCluster + 1;
    QWORD clusterstotal = clustersleft;

    while (bytesleft > 0)
    {
        this->Progress->SetProgress(MulDiv((int)(clusterstotal - clustersleft), 1000, (int)clusterstotal));
        if (this->Progress->GetWantCancel())
        {
            delete[] buffer;
            return FALSE;
        }

        QWORD nc = min(clustersleft, bufclusters);
        QWORD nb = min(bytesleft, nc * this->Volume->BytesPerCluster);
        if (!reader.GetClusters(buffer, nc))
        {
            delete[] buffer;
            String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORREADINGBMP);
            return FALSE;
        }
        clusterBitmap->AddBitmapBlock(buffer, nb);

        bytesleft -= nb;
        clustersleft -= nc;
    }
    delete[] buffer;
    return TRUE;
}

// Bitmap contains 2 bits for each cluster with following values:
//   0 - this cluster doesn't contain data (or we don't know about it)
//   1 - there is one delete file
//   2 - there is more than one delete file
//   3 - there is existing directory

template <typename CHAR>
void CExFATSnapshot<CHAR>::DrawDeletedFile(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap)
{
    CALL_STACK_MESSAGE_NONE
    // walk through all nonresident streams
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->DSSize > 0)
        {
            // "render" all cluster runs to bitmap
            CRunsWalker<CHAR> runsWalker(stream->Ptrs);
            QWORD clustersleft = (stream->DSSize - 1) / this->Volume->BytesPerCluster + 1;
            while (clustersleft > 0)
            {
                QWORD lcn;
                QWORD length;
                if (!runsWalker.GetNextRun(&lcn, &length, NULL))
                    break; // unlikely error, moreover it doesn't matter
                if (lcn != -1)
                {
                    // if value in bitmap is smaller than 2, increase it
                    for (QWORD i = lcn; i < lcn + length; i++)
                        clusterBitmap->IncreaseValue(i - 2); // exFAT bitmap is zero based
                }
                clustersleft -= min(clustersleft, length);
            }
        }
        stream = stream->DSNext;
    }
}

template <typename CHAR>
DWORD CExFATSnapshot<CHAR>::GetFileCondition(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap)
{
    CALL_STACK_MESSAGE_NONE
    QWORD cnt[4] = {0, 0, 0, 0};
    BOOL resident = FALSE;

    // basically the same such in DrawDeletedFile function but this time we are just collecting number of clusters in each class
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->DSSize > 0)
        {
            // walk through all cluster runs in bitmap
            CRunsWalker<CHAR> runsWalker(stream->Ptrs);
            QWORD clustersleft = (stream->DSSize - 1) / this->Volume->BytesPerCluster + 1;
            while (clustersleft > 0)
            {
                QWORD lcn;
                QWORD length;
                if (!runsWalker.GetNextRun(&lcn, &length, NULL))
                {
                    TRACE_IW(L"Error, name=" << CWStr(record->FileNames->FNName).c_str());
                    break;
                }
                if (lcn != -1)
                {
                    for (QWORD i = lcn; i < lcn + length; i++)
                    {
                        // for number cluster 'i' find related two bits in bitmap
                        BYTE c;
                        if (clusterBitmap->GetValue(i - 2, &c)) // exFAT bitmap is zero based
                            cnt[c]++;
                        else
                            TRACE_E("GetFileCondition(): stream is out of BITMAP range");
                    }
                }
                clustersleft -= min(clustersleft, length);
            }
        }
        else
        {
            resident = TRUE;
        }

        stream = stream->DSNext;
    }

    // result
    if (cnt[0] != 0)
        TRACE_E("Internal error!");
    if (cnt[0] == 0 && cnt[1] == 0 && cnt[2] == 0 && !resident)
        return FR_FLAGS_CONDITION_LOST;
    else if (cnt[3] != 0)
        return FR_FLAGS_CONDITION_POOR;
    else if (cnt[2] != 0)
        return FR_FLAGS_CONDITION_FAIR;
    else
        return FR_FLAGS_CONDITION_GOOD;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::EstimateFileDamage(const FILE_RECORD_I<CHAR>* deletedFiles, CLUSTER_MAP_I** clusterMap)
{
    CALL_STACK_MESSAGE1("CExFATSnapshot::EstimateFileDamage()");

    // get disk bitmap
    CClusterBitmap clusterBitmap;
    if (!LoadClusterBitmap(&clusterBitmap))
        return FALSE;

    // "render" deleted files to bitmap
    DWORD i;
    for (i = 0; i < deletedFiles->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = deletedFiles->DirItems[i].Record;
        if ((r->Flags & FR_FLAGS_DELETED) && !r->IsDir)
            DrawDeletedFile(r, &clusterBitmap);
    }

    // finally get state of deleted files
    for (i = 0; i < deletedFiles->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = deletedFiles->DirItems[i].Record;
        if ((r->Flags & FR_FLAGS_DELETED) && !r->IsDir)
        {
            r->Flags &= ~FR_FLAGS_CONDITION_MASK;
            r->Flags |= GetFileCondition(r, &clusterBitmap);
        }
    }

    if (this->UdFlags & UF_GETLOSTCLUSTERMAP) // is lost cluster map required?
    {
        if (clusterMap != NULL)
        {
            // files with Condition FC_FAIR or FC_POOR render as "2 - there are more then one delete file in this place"
            // because we should return map of clusters which are not used by existing files and files which could be recovered (FC_GOOD)
            for (i = 0; i < deletedFiles->NumDirItems; i++)
            {
                FILE_RECORD_I<CHAR>* r = deletedFiles->DirItems[i].Record;
                if ((r->Flags & FR_FLAGS_DELETED) && !r->IsDir)
                {
                    DWORD condition = r->Flags & FR_FLAGS_CONDITION_MASK;
                    if (condition == FR_FLAGS_CONDITION_FAIR || condition == FR_FLAGS_CONDITION_POOR)
                        DrawDeletedFile(r, &clusterBitmap); // second render will increase 1 -> 2, (2 will stay 2)
                }
            }

            *clusterMap = (CLUSTER_MAP_I*)&cluster_map;
            if (!GetLostClustersMap(&clusterBitmap, *clusterMap))
                *clusterMap = NULL; // out of memory, return NULL
        }
        else
            TRACE_E("UF_GETLOSTCLUSTERMAP is specified, but clusterMap is NULL!");
    }

    return TRUE;
}

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::GetLostClustersMap(CClusterBitmap* clusterBitmap, CLUSTER_MAP_I* clusterMap)
{
    CALL_STACK_MESSAGE1("CExFATSnapshot::GetLostClustersMap()");
    clusterMap->Free();
    QWORD clusterCount = clusterBitmap->GetClusterCount();

    // pass 0: get size of arrays we should allocate, allocate them
    // pass 1: fill these arrays
    for (int pass = 0; pass < 2; pass++)
    {
        DWORD clusterPairs = 0;
        BOOL inside = FALSE;
        BOOL flush = FALSE;
        QWORD lcnFirst = 0;
        QWORD i = 0;
        while (i < clusterCount)
        {
            // for cluster number 'i' find related two bits in bitmap
            BYTE c;
            clusterBitmap->GetValue(i - 2, &c); // exFAT bitmap is zero based
            if (c == 0 || c == 2)               // cluster is not used (or we don't know about it) || there is FC_FAIR or FC_POOR
            {
                // if it is beginning of segment that we are interested in, store lcnFirst and set we are in segment
                if (!inside)
                {
                    lcnFirst = i;
                    inside = TRUE;
                }
            }
            else
            {
                if (inside)
                    flush = TRUE;
            }
            if (inside && (i >= clusterCount - 1))
                flush = TRUE;

            if (flush)
            {
                flush = FALSE;
                inside = FALSE;
                if (pass == 1)
                {
                    clusterMap->FirstCluster[clusterPairs] = lcnFirst;
                    clusterMap->ClustersCount[clusterPairs] = i - lcnFirst + 1;
                }
                clusterPairs++;
            }
            i++;
        }
        if (pass == 0)
        {
            clusterMap->Items = clusterPairs;
            if (clusterMap->Items > 0)
            {
                clusterMap->FirstCluster = new QWORD[clusterMap->Items];
                if (clusterMap->FirstCluster == NULL)
                {
                    clusterMap->Items = 0;
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }
                clusterMap->ClustersCount = new QWORD[clusterMap->Items];
                if (clusterMap->ClustersCount == NULL)
                {
                    clusterMap->Items = 0;
                    delete[] clusterMap->FirstCluster;
                    clusterMap->FirstCluster = NULL;
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }
            }
            else
                break; // we can stop here, there is nothing to store
        }
    }

    return TRUE;
}

#define MAX_FAT_HEAD_SIZE (10 * 1000 * 1000) // max FAT head buffer size in bytes
#define MAX_FAT_CACHE_SECTORS 8              // max FAT cache buffer size in sectors

template <typename CHAR>
BOOL CExFATSnapshot<CHAR>::OpenFAT()
{
    if (this->Volume == NULL)
    {
        TRACE_E("Volume not opened!");
        return FALSE;
    }
    // sanity check: (size of FAT in sectors * byte per sector) / size of DWORD
    QWORD fatItems = this->Volume->ExFATBoot.FATLength * (this->Volume->BytesPerSector / 4);
    if (fatItems > 0xFFFFFFFF)
    {
        TRACE_E("FAT items (clusters) count > 2^32!");
        return FALSE;
    }
    FATItems = (DWORD)fatItems;
    // head buffer must be aligned to sector size
    DWORD maxHeadSectors = MAX_FAT_HEAD_SIZE / this->Volume->BytesPerSector;
    DWORD headSectors = min(this->Volume->ExFATBoot.FATLength, maxHeadSectors);
    FATHeadItems = headSectors * (this->Volume->BytesPerSector / 4);
    FATHead = (DWORD*)malloc(FATHeadItems * 4);
    if (FATHead == NULL)
    {
        TRACE_E("Low memory!");
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }

    // load head buffer
    this->Progress->GetWantCancel();
    if (!this->Volume->ReadSectors(FATHead, this->Volume->ExFATBoot.FATOffset, headSectors))
    {
        free(FATHead);
        FATHead = NULL;
        FATHeadItems = 0;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORREADINGFAT);
    }
    this->Progress->GetWantCancel();

    // sanity checks
    if (FATHead[0] != 0xFFFFFFF8)
        TRACE_E("Wrong ExFAT media descriptor!");
    if (FATHead[1] != 0xFFFFFFFF)
        TRACE_E("Wrong ExFAT header!");

    // when whole FAT doesn't fit to FATHead buffer, allocate another "cache" buffer
    if (headSectors < this->Volume->ExFATBoot.FATLength)
    {
        FATCacheItems = MAX_FAT_CACHE_SECTORS * (this->Volume->BytesPerSector / 4);
        FATCache = (DWORD*)malloc(FATCacheItems * 4);
        FATCacheFirstItem = 0;
        if (FATCache == NULL)
        {
            TRACE_E("Low memory!");
            free(FATHead);
            FATHead = NULL;
            FATHeadItems = 0;
            FATCacheItems = 0;
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        }
    }

    return TRUE;
}

template <typename CHAR>
DWORD CExFATSnapshot<CHAR>::GetFATItem(DWORD index)
{
    if (index < FATHeadItems)
        return FATHead[index];
    if (index >= FATCacheFirstItem && index < FATCacheFirstItem + FATCacheItems)
        return FATCache[index - FATCacheFirstItem];
    DWORD firstSector = index / (this->Volume->BytesPerSector / 4);          // convert index to sectors
    DWORD cacheSectors = FATCacheItems / (this->Volume->BytesPerSector / 4); // number of sectors allocated in cache
    // for simplicity we can read over on end of FAT
    if (!this->Volume->ReadSectors(FATCache, this->Volume->ExFATBoot.FATOffset + firstSector, cacheSectors))
    {
        // JRYFIXME - projit ostatni volani ReadSectors(), resime navratove hodnoty?? Pokud ano, meli bychom zde vracet BOOL
        String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORREADINGFAT);
        return 0;
    }
    FATCacheFirstItem = firstSector * (this->Volume->BytesPerSector / 4);
    return FATCache[index - FATCacheFirstItem];
}
