// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

template <typename CHAR>
class CClusterHeap
{
public:
    struct ITEM
    {
        int cluster;
        int seq;
        FILE_RECORD_I<CHAR>* dir;
    };

    CClusterHeap() { Init(); }
    void Init();
    BOOL Insert(int cluster, int seq, FILE_RECORD_I<CHAR>* dir);
    BOOL IsEmpty() { return Size == 0; }
    ITEM ExtractMin();
    void Release();

private:
    ITEM* Array;
    int Size, Max;
};

#pragma pack(push, fat_h)
#pragma pack(1)

struct DIR_ENTRY_SHORT
{
    char Name[11];     // name and extension
    BYTE Attr;         // attribute bits
    BYTE NTRes;        // Case for base and extension
    BYTE CrtTimeTenth; // Creation time, milliseconds
    WORD CrtTime;      // Creation time
    WORD CrtDate;      // Creation date
    WORD LstAccDate;   // Last access date
    WORD FstClusHI;    // High 16 bits of cluster in FAT32
    WORD WrtTime;      // Time of last write. Note that file creation is considered a write
    WORD WrtDate;      // Date of last write. Note that file creation is considered a write
    WORD FstClusLO;    // Low word of this entry’s first cluster number
    DWORD FileSize;    // 32-bit DWORD holding this file’s size in bytes
};

struct DIR_ENTRY_LONG
{
    BYTE Ord;       // order of the entry
    char Name1[10]; // first sub-segment of name
    BYTE Attr;      // attributes = ATTR_LONG_NAME
    BYTE Type;      // = 0
    BYTE Chksum;    // checksum of corresponding short entry
    char Name2[12]; // second sub-segment of name
    WORD FstClusLO; // = 0
    char Name3[4];  // third sub-segment of name
};

#pragma pack(pop, fat_h)

template <typename CHAR>
class CFATSnapshot : public CSnapshot<CHAR>
{
public:
    CFATSnapshot(CVolume<CHAR>* volume);
    virtual ~CFATSnapshot();
    virtual BOOL Update(CSnapshotProgressDlg* progress, DWORD udFlags, CLUSTER_MAP_I** clusterMap);
    virtual void Free();

private:
    struct CLUSTER
    {
        DWORD cluster;
        DWORD flags;
        DWORD next;
        BYTE* data;
    };

    enum EstimateDamageTodo
    {
        edtDraw,         // in first phase render files into FAT table, where we have available 2 bits (MARK_MASK)
        edtGetCondition, // during second phase using these two bits estimate file condition and set Condition variable
        edtDrawLost      // third (optional) phase is intended for obtaining lost clusters map
    };

    BOOL LoadFAT();
    DWORD ConvertFATAttr(DWORD fatattr);
    BOOL DecodeDirectoryClusters(BYTE* buffer, DWORD len, DIR_ITEM_I<CHAR>** diritems, DWORD* numitems, BOOL deleteddir, BOOL root = FALSE);
    BOOL ScheduleCluster(DWORD cluster, DWORD seq, FILE_RECORD_I<CHAR>* dir);
    BOOL ScheduleDirectory(FILE_RECORD_I<CHAR>* dir, int cluster);
    BOOL ScanDirectoryCluster(BYTE* buffer, DWORD len, BOOL forcedeleted);
    BOOL LoadExistingDirectories();
    DWORD AnalyzeCluster(BYTE* buffer, DWORD len, DWORD* flags);
    BOOL LoadDeletedDirectories();
    BOOL IsValidCluster(DWORD cluster);
    BOOL ScheduleDeletedDirectory(DWORD cluster, FILE_RECORD_I<CHAR>* dir);
    DWORD GetNextDirCluster(BYTE* buffer, DWORD level, DWORD phase);
    DWORD GetCluster(DIR_ENTRY_SHORT* entry);
    void FilterZeroFiles(FILE_RECORD_I<CHAR>* record);
    BOOL FilterExistingDirectories(FILE_RECORD_I<CHAR>* record);
    BOOL FilterEmptyDirectories(FILE_RECORD_I<CHAR>* record);
    BOOL IgnoreEntry(DIR_ENTRY_SHORT* entry, BOOL deleteddir);
    void CountClusters();
    void AddToProgress(DIR_ENTRY_SHORT* entry);
    void FreeRecord2(FILE_RECORD_I<CHAR>* record);
    BOOL AddVirtualDirs();
    BOOL AddFileOrDir(FILE_RECORD_I<CHAR>* dir, CHAR* name, BOOL isdir, DWORD flags, FILE_RECORD_I<CHAR>** newrec);
    DWORD AddDeletedFiles(FILE_RECORD_I<CHAR>* dir, FILE_RECORD_I<CHAR>* deletedfiles);
    BOOL ScanVacantClusters();
    CLUSTER* GetScannedCluster(DWORD cluster);
    BOOL DecodeScannedCluster(FILE_RECORD_I<CHAR>* record, CLUSTER* rclus);
    BOOL ProcessScannedClusters();
    void FreeScannedClusters();
    BOOL ReloadScannedClusters();
    BOOL RemoveDuplicateFiles(FILE_RECORD_I<CHAR>* record);
    BOOL RenameDuplicateDirectories(FILE_RECORD_I<CHAR>* dir);
    BOOL EncodeExistingChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file);
    BOOL EncodeDeletedChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file);
    BOOL EncodeClusterChains(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* dir);
    void DrawDeletedFile(FILE_RECORD_I<CHAR>* dir);
    void GetFileCondition(FILE_RECORD_I<CHAR>* dir);
    void DeleteAllMarks();
    void EstimateFileDamage(FILE_RECORD_I<CHAR>* dir, EstimateDamageTodo todo);
    BOOL GetLostClustersMap(CLUSTER_MAP_I* clusterMap);

    static int compare_names(const void* elem1, const void* elem2);
    static int compare_clusters(const void* elem1, const void* elem2);

    DWORD* FAT;
    DWORD EOC;
    CClusterHeap<CHAR>* HeapAhead1;
    CClusterHeap<CHAR>* HeapAhead2;
    CClusterHeap<CHAR>* HeapBehind1;
    CClusterHeap<CHAR>* HeapBehind2;
    CClusterHeap<CHAR> Heap1A;
    CClusterHeap<CHAR> Heap1B;
    CClusterHeap<CHAR> Heap2A;
    CClusterHeap<CHAR> Heap2B;
    DWORD CurrentPos1;
    DWORD CurrentPos2;
    DWORD FATItems;
    DWORD FreeClusters;
    DWORD UsedClusters;
    DWORD ClustersProcessed;
    DWORD ClustersTotal;
    DWORD VirtualDirsCount;

    TDirectArray<CLUSTER>* ScanResults;

#ifdef _WIN64
    TIndirectArray<FILE_RECORD_I<CHAR>> FILE_RECORD_Pointers;
#endif // _WIN64

#ifdef TRACE_ENABLE
    DWORD InsertedAhead;
    DWORD InsertedBehind;
#endif
};

// ****************************************************************************
//
//  Implementation
//

#define FAT_BUFFER 128 // in sectors -> 64KB
#define HEAP_DELTA 512
#define SCAN_BUFFER 256 // 256KB
#define SCAN_WEIGHT 15  // multiple of weight of scanned clusters for progress

#define SHOW_EMPTY_DELETED_DIRS // if defined, also deleted directories without clusters will be displayed

#define FAT_ATTR_READ_ONLY 0x01
#define FAT_ATTR_HIDDEN 0x02
#define FAT_ATTR_SYSTEM 0x04
#define FAT_ATTR_VOLUME_ID 0x08
#define FAT_ATTR_DIRECTORY 0x10
#define FAT_ATTR_ARCHIVE 0x20
#define FAT_ATTR_LONG_NAME (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID)
#define FAT_ATTR_LONG_NAME_MASK (FAT_ATTR_READ_ONLY | FAT_ATTR_HIDDEN | FAT_ATTR_SYSTEM | FAT_ATTR_VOLUME_ID | FAT_ATTR_DIRECTORY | FAT_ATTR_ARCHIVE)
#define LAST_LONG_ENTRY 0x40
#define LONG_ENTRY_ORD_MASK 0x3F
#define islong(attr) (((attr)&FAT_ATTR_LONG_NAME_MASK) == FAT_ATTR_LONG_NAME)

// maximum cluster count on FAT32 is 2^28, so we can use highest 4 bits in FAT record for
// marks and file damage estimation
#define MARK_MASK 0xF0000000    // high 4 bits of FAT records used for marks
#define MARK_DEL_DIR 0x10000000 // cluster mark for deleted directory that we already read

// helper function declarations
BOOL ConvertFATName(const char* fatName, char* name);
BYTE ChkSum(BYTE* pFcpName);
BOOL isupdir(DIR_ENTRY_SHORT* entry);

template <typename CHAR>
CFATSnapshot<CHAR>::CFATSnapshot(CVolume<CHAR>* volume)
    : CSnapshot<CHAR>(volume)
#ifdef _WIN64
      ,
      FILE_RECORD_Pointers(1000, 1000, dtNoDelete)
#endif // _WIN64
{
    FAT = NULL;
    VirtualDirsCount = 0;
    ScanResults = NULL;
}

template <typename CHAR>
CFATSnapshot<CHAR>::~CFATSnapshot()
{
    Free();
    if (ScanResults != NULL)
        delete ScanResults;
}

// ****************************************************************************
//
//  LoadFAT() - read whole FAT and convert to 32bit
//

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::LoadFAT()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::LoadFAT()");

    // obtain number of records and allocate FAT (+eoc)
    int num;
    int den;
    switch (this->Volume->FAT_FATType)
    {
    case ftFAT12:
        num = 2;
        den = 3;
        EOC = 0x00000FF8;
        break;
    case ftFAT16:
        num = 1;
        den = 2;
        EOC = 0x0000FFF8;
        break;
    case ftFAT32:
        num = 1;
        den = 4;
        EOC = 0x0FFFFFF8;
        break;
    }
    int bps = this->Volume->Boot.BytesPerSector;
    FATItems = (DWORD)((QWORD)(this->Volume->FAT_FATSize * bps * num) / den + 1);
    FAT = new DWORD[FATItems];
    if (FAT == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    FAT[FATItems - 1] = 0;

    // allocate buffer
    int bufsize = FAT_BUFFER;
    if (this->Volume->FAT_FATType == ftFAT12)
        bufsize -= bufsize % 3; // for FAT12 we need to have bufsize divisible by 3
    BYTE* buffer = new BYTE[bufsize * bps];
    if (buffer == NULL)
    {
        delete[] FAT;
        FAT = NULL;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }

    // read
    int sector = this->Volume->FATBoot.RsvdSecCnt;
    int clusters = this->Volume->FAT_FATSize;
    int i;
    int fatpos = 0;
    while (clusters)
    {
        int n = min(clusters, bufsize);
        if (!this->Volume->ReadSectors(buffer, sector, n) || this->Progress->GetWantCancel())
        {
            delete[] FAT;
            FAT = NULL;
            delete[] buffer;
            return this->Progress->GetWantCancel() ? FALSE : String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORREADINGFAT);
        }
        sector += n;
        clusters -= n;

        // convert to DWORDs
        n *= bps;
        switch (this->Volume->FAT_FATType)
        {
        case ftFAT12:
        {
            for (i = 0; i < n; i += 3)
            {
                FAT[fatpos++] = *((WORD*)(buffer + i)) & 0x0FFF;
                FAT[fatpos++] = *((WORD*)(buffer + i + 1)) >> 4;
            }
            break;
        }

        case ftFAT16:
        {
            for (i = 0; i < n; i += 2)
                FAT[fatpos++] = *((WORD*)(buffer + i));
            break;
        }

        case ftFAT32:
        {
            for (i = 0; i < n; i += 4)
                FAT[fatpos++] = *((DWORD*)(buffer + i)) & 0x0FFFFFFF;
            break;
        }
        }
    }

    delete[] buffer;
    return TRUE;
}

// ****************************************************************************
//
//  Heap implementation for speed optimization (sequential/faster reading of clusters)
//

template <typename CHAR>
void CClusterHeap<CHAR>::Init()
{
    Size = 0;
    Max = 0;
    Array = NULL;
}

template <typename CHAR>
BOOL CClusterHeap<CHAR>::Insert(int cluster, int seq, FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE3("CClusterHeap::Insert(%d, %d, )", cluster, seq);

    // if needed, reallocate array
    if (Size >= Max)
    {
        Max += HEAP_DELTA; // maybe better to increase size twice or 1.5x
        ITEM* temp = new ITEM[Max];
        if (temp == NULL)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        if (Array != NULL)
        {
            memcpy(temp, Array, Size * sizeof(ITEM));
            delete[] Array;
        }
        Array = temp;
    }

    // add a new item to the heap
    int parent;
    int child = Size++;
    while (child > 0 && Array[parent = (child - 1) / 2].cluster > cluster)
    {
        Array[child] = Array[parent];
        child = parent;
    }
    Array[child].cluster = cluster;
    Array[child].seq = seq;
    Array[child].dir = dir;
    return TRUE;
}

template <typename CHAR>
typename CClusterHeap<CHAR>::ITEM CClusterHeap<CHAR>::ExtractMin()
{
    CALL_STACK_MESSAGE1("CClusterHeap::ExtractMin()");

    if (IsEmpty())
        TRACE_E("Heap is empty!");
    ITEM result = Array[0];
    ITEM item = Array[--Size];
    int child;
    int parent = 0;
    while ((child = 2 * parent + 1) < Size)
    {
        if (child + 1 < Size && Array[child].cluster > Array[child + 1].cluster)
            child++;

        if (item.cluster > Array[child].cluster)
        {
            Array[parent] = Array[child];
            parent = child;
        }
        else
        {
            break;
        }
    }
    Array[parent] = item;
    return result;
}

template <typename CHAR>
void CClusterHeap<CHAR>::Release()
{
    CALL_STACK_MESSAGE1("CClusterHeap::Release()");
    if (Array != NULL)
    {
        delete[] Array;
        TRACE_I("CClusterHeap Max: " << Max);
    }
    Init();
}

// ****************************************************************************
//
//  Decode directories clusters
//

template <typename CHAR>
DWORD CFATSnapshot<CHAR>::GetCluster(DIR_ENTRY_SHORT* entry)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CFATSnapshot::GetCluster()");
    DWORD cluster = entry->FstClusLO;
    if (this->Volume->FAT_FATType == ftFAT32)
        cluster += ((DWORD)entry->FstClusHI) << 16;
    return cluster;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::IgnoreEntry(DIR_ENTRY_SHORT* entry, BOOL deleteddir)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE2("CFATSnapshot::IgnoreEntry(, %d)", deleteddir);

    // ignore updirs
    if (isupdir(entry))
        return TRUE;

    // skip long records, read them with short list
    if (islong(entry->Attr))
        return TRUE;

    // skip volume id
    if (entry->Attr & FAT_ATTR_VOLUME_ID)
        return TRUE;

    // ignore existing files if flag UF_SHOWEXSTING is not set and if we are not in deleted directory
    if (!(entry->Attr & FAT_ATTR_DIRECTORY))
    {
        if (!deleteddir && entry->Name[0] != 0xE5 && !(this->UdFlags & UF_SHOWEXISTING))
            return TRUE;
    }
    else
    {
        int cluster = GetCluster(entry);
        // also directories points to invalid cluster
        if (!IsValidCluster(cluster))
            return TRUE;
        // and deleted directories, whose cluster is already used again
        if (entry->Name[0] == 0xE5 && (FAT[cluster] & ~MARK_MASK))
            return TRUE;
    }

    // ignore also deleted files which has second and third character zero - such combination should not
    // exist for short nor long record, (it could exist in case the record was shredded)
    if (!entry->Name[1] && !entry->Name[2])
        return TRUE;

    // accept other records
    return FALSE;
}

template <typename CHAR>
void CFATSnapshot<CHAR>::AddToProgress(DIR_ENTRY_SHORT* entry)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CFATSnapshot::AddToProgress()");
    if (islong(entry->Attr) || entry->Name[0] == 0xE5 || isupdir(entry))
        return;
    int cluster = GetCluster(entry);
    while (cluster && (DWORD)cluster < EOC && IsValidCluster(cluster)) // type cast (DWORD)cluster is OK
    {
        ClustersProcessed++;
        cluster = FAT[cluster] & ~MARK_MASK;
    }
}

#ifdef ENABLE_TRACE_X
void DumpEntry(DIR_ENTRY_SHORT* entry);
#endif

template <typename CHAR>
DWORD CFATSnapshot<CHAR>::ConvertFATAttr(DWORD fatattr)
{
    CALL_STACK_MESSAGE_NONE
    DWORD attr = 0;
    if (fatattr & FAT_ATTR_DIRECTORY)
        attr |= FILE_ATTRIBUTE_DIRECTORY;
    if (fatattr & FAT_ATTR_READ_ONLY)
        attr |= FILE_ATTRIBUTE_READONLY;
    if (fatattr & FAT_ATTR_HIDDEN)
        attr |= FILE_ATTRIBUTE_HIDDEN;
    if (fatattr & FAT_ATTR_SYSTEM)
        attr |= FILE_ATTRIBUTE_SYSTEM;
    if (fatattr & FAT_ATTR_ARCHIVE)
        attr |= FILE_ATTRIBUTE_ARCHIVE;
    return attr;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::DecodeDirectoryClusters(BYTE* buffer, DWORD len, DIR_ITEM_I<CHAR>** diritems,
                                                 DWORD* numitems, BOOL deleteddir, BOOL root)
{
    CALL_STACK_MESSAGE3("CFATSnapshot::DecodeDirectoryClusters(, %d, , , %d)", len, deleteddir);

    *diritems = NULL;
    *numitems = 0;
    // buffer could be NULL for directories where cluster==0 in ScanDirectoryCluster
    if (buffer == NULL || len == 0)
        return TRUE;

    DIR_ENTRY_SHORT* entry = ((DIR_ENTRY_SHORT*)buffer) - 1;
    DIR_ENTRY_SHORT* bufmax = (DIR_ENTRY_SHORT*)(buffer + len);
    WCHAR long_name[63 * 13 + 1];
    int long_len;

    // calculate items, increase progress and allocate diritems
    DIR_ENTRY_SHORT* temp = entry;
    while (++temp < bufmax && temp->Name[0])
    {
        if (!IgnoreEntry(temp, deleteddir))
            (*numitems)++;
        if (!deleteddir)
            AddToProgress(temp);
#ifdef ENABLE_TRACE_X
        DumpEntry(temp);
#endif
    }
    if (root)
        *numitems += 2; // 2 for {All Deleted Files} and {Metafiles}
    if (*numitems == 0)
        return TRUE;
    *diritems = new DIR_ITEM_I<CHAR>[*numitems];
    *numitems = 0;
    if (diritems == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);

    // main loop
    while (++entry < bufmax && entry->Name[0])
    {
        if (!IgnoreEntry(entry, deleteddir))
        {
            // read long name if preceded short name
            long_len = 0;
            if (entry > (DIR_ENTRY_SHORT*)buffer)
            {
                DIR_ENTRY_LONG* l = (DIR_ENTRY_LONG*)(entry - 1);
                if (islong(l->Attr))
                {
                    char temp2[11];
                    memcpy(temp2, entry->Name, 11);
                    if (temp2[0] == 0xE5)
                    {
                        WCHAR firstchar;
                        int i = 0;
                        // special case: dots on the beginning of long name doesn't make it into short name
                        while (i < 5 && (firstchar = ((WCHAR*)l->Name1)[i]) == '.')
                            i++;
                        // first letter reconstruction to get original checksum
                        WideCharToMultiByte(CP_ACP, 0, &firstchar, 1, temp2, 1, NULL, NULL);
                        temp2[0] = (char)(UINT_PTR)CharUpper((LPTSTR)temp2[0]);
                    }
                    BYTE sum = ChkSum((BYTE*)temp2);
                    int ord;
                    int last_ord = 0;
                    BOOL existing = (entry->Name[0] != 0xE5);

                    // read long records in case checksum match and ord is increasing (+1) for existing
                    while (l >= (DIR_ENTRY_LONG*)buffer && islong(l->Attr) && l->Chksum == sum)
                    {
                        ord = l->Ord & LONG_ENTRY_ORD_MASK;
                        if (existing && ord != last_ord + 1)
                            break;
                        memcpy(long_name + long_len, l->Name1, 10);
                        long_len += 5;
                        memcpy(long_name + long_len, l->Name2, 12);
                        long_len += 6;
                        memcpy(long_name + long_len, l->Name3, 4);
                        long_len += 2;
                        if (existing && (l->Ord & LAST_LONG_ENTRY) != 0)
                            break;
                        last_ord = ord;
                        l--;
                    }

                    // terminate long name if it fits perfectly
                    long_name[long_len] = 0;
                    // but real length could be shorter
                    long_len = (int)wcslen(long_name);
                    // todo: if long name end is missing, attach extension from tshort name
                }
            }

            // convert to 8.3 name
            char name8_3[13];
            if (!ConvertFATName(entry->Name, name8_3))
            {
                TRACE_E("DecodeDirectoryClusters: Error converting to 8.3 name.");
                continue; // skip invalid name
            }

            // obtain FILE_RECORD for this entry
            FILE_RECORD_I<CHAR>* record;
            if (entry->Attr & FAT_ATTR_DIRECTORY)
            {
                // we already have record for directories
#ifdef _WIN64
                // pointer to FileSize doesn't fit on x64, so we store only index into FILE_RECORD_Pointers array
                record = FILE_RECORD_Pointers[entry->FileSize];
#else  // _WIN64
                record = (FILE_RECORD_I<CHAR>*)entry->FileSize;     // on x86 pointer fits directly into FileSize
#endif // _WIN64
            }
            else
            {
                // if no, create a new one
                record = new FILE_RECORD_I<CHAR>;
                if (record == NULL)
                    goto lowmem;
            }

            // store name, dosname
            FILE_NAME_I<CHAR>* fname = new FILE_NAME_I<CHAR>;
            if (fname == NULL)
                goto lowmem;
            record->FileNames = fname;
            // add to diritems (now so it can be deleted if needed)
            (*diritems)[*numitems].FileName = fname;
            (*diritems)[*numitems].Record = record;
            (*numitems)++;
            if (long_len > 0)
            {
                fname->FNName = String<CHAR>::NewFromUnicode(long_name, (unsigned long)wcslen(long_name));
                if (fname->FNName == NULL)
                    goto lowmem;
                fname->DOSName = String<CHAR>::NewFromASCII(name8_3);
                if (fname->DOSName == NULL)
                    goto lowmem;
            }
            else
            {
                if ((entry->NTRes & 0x08) != 0)
                    _strlwr(name8_3);
                fname->FNName = String<CHAR>::NewFromASCII(name8_3);
                if (fname->FNName == NULL)
                    goto lowmem;
                fname->DOSName = NULL;
            }

            // attributes conversion
            record->Attr = ConvertFATAttr(entry->Attr);
            record->IsDir = (record->Attr & FILE_ATTRIBUTE_DIRECTORY) != 0;
            record->Flags = 0;
            if (deleteddir || entry->Name[0] == 0xE5)
                record->Flags |= FR_FLAGS_DELETED;

            // filesize

            // filetime
            DosDateTimeToFileTime(entry->CrtDate, entry->CrtTime, &record->TimeCreation);
            LocalFileTimeToFileTime(&record->TimeCreation, &record->TimeCreation);
            DosDateTimeToFileTime(entry->WrtDate, entry->WrtTime, &record->TimeLastWrite);
            LocalFileTimeToFileTime(&record->TimeLastWrite, &record->TimeLastWrite);
            record->TimeLastAccess = record->TimeLastWrite;

            // for files store number of first cluster and size (streams and duplicates search)
            if (!record->IsDir)
            {
                DATA_STREAM_I<CHAR>* stream = new DATA_STREAM_I<CHAR>;
                if (stream == NULL)
                    goto lowmem;
                stream->DSSize = entry->FileSize;
                stream->DSValidSize = stream->DSSize; // sync with DSSize (FAT doesn't support valid data)
                stream->FirstLCN = GetCluster(entry);
                record->Streams = stream;
            }
        }
    }
    // end of normal run
    return TRUE;

    // on error free all we allocated
lowmem:
    for (DWORD i = 0; i < *numitems; i++)
    {
        FILE_RECORD_I<CHAR>* r = (*diritems)[i].Record;
        this->FreeRecord(r);
        if (!r->IsDir)
            delete r;
    }
    delete *diritems;
    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
}

// ****************************************************************************
//
//  1. phase: read existing directory tree
//

// *** Use item FILE_RECORD for directory in transition phase: ***
// DirItems - buffer for all allocated directories, it is filled gradually
// NumDirItems - total number of directory clusters
// Streams - counter of clusters remaining to read

// *** In entries in cluster buffer for directories: ***
// FileSize - x86 version: pointer to FILE_RECORD of directory (32bit is enough, store directly to FileSize)
//          - x64 version: index in array FILE_RECORD_Pointers, where pointer to FILE_RECORD is stored

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ScheduleCluster(DWORD cluster, DWORD seq, FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE3("CFATSnapshot::ScheduleCluster(%d, %d, )", cluster, seq);

    // add cluster into appropriate heap
    if (cluster >= CurrentPos1)
    {
#ifdef TRACE_ENABLE
        InsertedAhead++;
#endif
        return HeapAhead1->Insert(cluster, seq, dir);
    }
    else
    {
#ifdef TRACE_ENABLE
        InsertedBehind++;
#endif
        return HeapBehind1->Insert(cluster, seq, dir);
    }
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::IsValidCluster(DWORD cluster)
{
    return cluster >= 2 && cluster < this->Volume->FAT_CountOfClusters + 2;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ScheduleDirectory(FILE_RECORD_I<CHAR>* dir, int cluster)
{
    CALL_STACK_MESSAGE2("CFATSnapshot::ScheduleDirectory(, %d)", cluster);

    // fill directory clusters
    dir->NumDirItems = 0;
    do
    {
        if (!ScheduleCluster(cluster, dir->NumDirItems++, dir))
            return FALSE;

        cluster = FAT[cluster] & ~MARK_MASK;
        if ((DWORD)cluster < EOC && !IsValidCluster(cluster)) // type case (DWORD)cluster is OK
        {
            TRACE_E("ScheduleDirectory: invalid directory cluster chain (cluster=" << cluster << ")");
            break;
        }
    } while ((DWORD)cluster < EOC); // type case (DWORD)cluster is OK

    // allocate buffer for clusters
    dir->DirItems = (DIR_ITEM_I<CHAR>*)new BYTE[dir->NumDirItems * this->Volume->BytesPerCluster];
    if (dir->DirItems == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);

    // mark beginning cluster with zero, so FreeRecord2() could decide which cluster was loaded
    BYTE* p = (BYTE*)dir->DirItems;
    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        *p = 0;
        p += this->Volume->BytesPerCluster;
    }

    // directories on FAT doesn't have streams, so we can use dir->Streams as remaining cluster counter
    dir->Streams = (DATA_STREAM_I<CHAR>*)(UINT_PTR)dir->NumDirItems;
    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ScanDirectoryCluster(BYTE* buffer, DWORD len, BOOL forcedeleted)
{
    CALL_STACK_MESSAGE3("CFATSnapshot::ScanDirectoryCluster(, %d, %d)", len, forcedeleted);

    // walk through the cluster and schedule found directories
    DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)buffer;
    DIR_ENTRY_SHORT* bufmax = (DIR_ENTRY_SHORT*)(buffer + len);
    while (entry < bufmax && entry->Name[0])
    {
        if ((entry->Attr & FAT_ATTR_DIRECTORY) && !IgnoreEntry(entry, FALSE /* deleteddir doesn't matter */))
        {
            // already allocate record for directories
            FILE_RECORD_I<CHAR>* dir = new FILE_RECORD_I<CHAR>;
            if (dir == NULL)
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            dir->IsDir = TRUE;
#ifdef _WIN64
            // store pointer to directory: to FileSize put index in array FILE_RECORD_Pointers
            // (we cannot place x64 pointer into FileSize)
            int ptrIndex = FILE_RECORD_Pointers.Add(dir);
            if (!FILE_RECORD_Pointers.IsGood())
            {
                FILE_RECORD_Pointers.ResetState();
                FreeRecord2(dir);
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            }
            entry->FileSize = ptrIndex;
#else  // _WIN64
            entry->FileSize = (DWORD)dir;                           // store pointer to directory (it is possible in x86)
#endif // _WIN64

            int cluster = GetCluster(entry);
            if (entry->Name[0] == 0xE5 || forcedeleted)
            {
                // deleted directory (or any directory if we are in phase 2)
                // mark in FAT and store for phase 2
                //FAT[cluster] |= MARK_START_DEL_DIR;
                if ((FAT[cluster] & ~MARK_MASK) == 0)
                {
                    if (!ScheduleDeletedDirectory(cluster, dir))
                        return FALSE;
                }
            }
            else
            {
                // schedule existing directory for reading in this phase
                if (cluster)
                {
                    if (!ScheduleDirectory(dir, cluster))
                        return FALSE;
                }
            }
        }
        entry++;
    }
    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::LoadExistingDirectories()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::LoadExistingDirectories()");

    // read root
    this->Root = new FILE_RECORD_I<CHAR>;
    if (this->Root == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    this->Root->IsDir = TRUE;
    if (this->Volume->FAT_FATType != ftFAT32)
    {
        // we must use special loader for FAT12/16 root
        DWORD len = this->Volume->FAT_RootDirSectors * this->Volume->Boot.BytesPerSector;
        BYTE* buffer = new BYTE[len];
        if (buffer == NULL)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        if (!this->Volume->ReadSectors(buffer, this->Volume->FAT_FirstRootDirSecNum, this->Volume->FAT_RootDirSectors))
        {
            delete[] buffer;
            return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGDIRCLUSTERS);
        }
        if (!ScanDirectoryCluster(buffer, len, FALSE) ||
            !DecodeDirectoryClusters(buffer, len, &this->Root->DirItems, &this->Root->NumDirItems, FALSE, TRUE))
        {
            delete[] buffer;
            return FALSE;
        }
        delete[] buffer;
    }
    else
    {
        // FAT32 root is possible to read in the same way as any other directory
        if (!ScheduleDirectory(this->Root, this->Volume->FATBoot.RootClus))
            return FALSE;
    }

#ifdef TRACE_ENABLE
    int rounds = 0;
    int ndirs = 0;
    InsertedAhead = 0;
    InsertedBehind = 0;
#endif
    while (!HeapAhead1->IsEmpty())
    {
        // empty heap before header
        CurrentPos1 = 0;
        while (!HeapAhead1->IsEmpty())
        {
            auto item = HeapAhead1->ExtractMin();

            // read cluster
            BYTE* buffer = ((BYTE*)item.dir->DirItems) + (item.seq * this->Volume->BytesPerCluster);
            if (!this->Volume->ReadClusters(buffer, item.cluster, 1))
                return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGDIRCLUSTERS);

            // cluster scan and schedule further directories
            if (!ScanDirectoryCluster(buffer, this->Volume->BytesPerCluster, FALSE))
                return FALSE;

            // decrease number of remaining clusters for directories
            int* counter = (int*)&item.dir->Streams;
            (*counter)--;
            // decode directory when zero
            if (!*counter)
            {
                if (item.dir->FileNames != NULL && item.dir->FileNames->FNName != NULL)
                    TRACE_XW(L"***** " << CWStr(item.dir->FileNames->FNName).c_str() << L" *****");
                else
                    TRACE_X("***** ??? *****");

                DIR_ITEM_I<CHAR>* diritems;
                DWORD numitems;
                // convert directory cluster into array of DIR_ITEM structures
                if (!DecodeDirectoryClusters((BYTE*)item.dir->DirItems, item.dir->NumDirItems * this->Volume->BytesPerCluster,
                                             &diritems, &numitems, FALSE, item.dir == this->Root))
                {
                    return FALSE;
                }
                delete[] (BYTE*)item.dir->DirItems;
                item.dir->DirItems = diritems;
                item.dir->NumDirItems = numitems;
                item.dir->Streams = NULL; // just to be sure

                // update progress
                this->Progress->SetProgress(MulDiv(ClustersProcessed, 1000, ClustersTotal));
#ifdef TRACE_ENABLE
                ndirs++;
#endif
            }

            CurrentPos1 = item.cluster + 1;
            if (this->Progress->GetWantCancel())
                return FALSE;
        }

        // swap "ahead" and "behind" heaps
        CClusterHeap<CHAR>* temp = HeapAhead1;
        HeapAhead1 = HeapBehind1;
        HeapBehind1 = temp;
#ifdef TRACE_ENABLE
        rounds++;
#endif
    }

#ifdef TRACE_ENABLE
    TRACE_I("LoadExistingDirectories: ndirs=" << ndirs << " rounds=" << rounds << " InsertedAhead=" << InsertedAhead << " InsertedBehind=" << InsertedBehind);
#endif
    HeapAhead1->Release();
    HeapBehind1->Release();
    return TRUE;
}

// ****************************************************************************
//
//  2. phase: load deleted directories
//

// AnalyzeCluster: base on cluster content decide cluster type
// return value:
// - 0 -> cluster doesn't contain directories records
// - non zero value -> cluster contains directories records, further information stored in flags
// flags:
// - DC_FIRST: first cluster of directory
// - DC_LAST: last cluster of directory

#define DC_FIRST 0x0001
#define DC_LAST 0x0002

template <typename CHAR>
DWORD CFATSnapshot<CHAR>::AnalyzeCluster(BYTE* buffer, DWORD len, DWORD* flags)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CFATSnapshot::AnalyzeCluster(, %d, )", len);

    DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)buffer;
    *flags = 0;

    // we can decide if it is first cluster of directory based on first two items
    // (it is not true for root but it doesn't matter because it is valid directory)
    if (!memcmp(entry[0].Name, ".          ", 11) && !memcmp(entry[1].Name, "..         ", 11) ||
        !memcmp(entry[0].Name, "..         ", 11) && !memcmp(entry[1].Name, ".          ", 11))
    { // (^second combination just to be sure)
        *flags = DC_FIRST;
    }

    // determine how much potential records there is (incomplete cluster ends by zero)
    DWORD maxitems = len / sizeof(DIR_ENTRY_SHORT);
    DWORD nitems = 0;
    while (nitems < maxitems && entry[nitems].Name[0])
        nitems++;
    if (!nitems)
        return 0;
    // if cluster is not full, it is last cluster
    if (nitems < maxitems)
        *flags |= DC_LAST;
    // cluster with dots on beginning we are sure
    if (*flags & DC_FIRST)
        return 1;

    // count number of short and long (valid) records
    DWORD nshort = 0;
    DWORD nlong = 0;
    for (DWORD i = 0; i < nitems; i++)
    {
        if (!islong(entry[i].Attr))
        {
            // in short name could be only characters >= 32, not lower case letters, * \ ? etc
            char* name = entry[i].Name;
            int j;
            for (j = 0; j < 11; j++)
            {
                if ((BYTE)name[j] < 32 || islower(name[j]) || strchr("*\\?<>\"+,/:;=", name[j]))
                    break;
            }
            if (j < 11)
                continue;

            // highest two bits of Attr must be zero
            if (entry[i].Attr & 0xC0)
                continue;

            // number of cluster must be valid or zero
            if (this->Volume->FAT_FATType != ftFAT32 && entry[i].FstClusHI)
                continue;
            int cluster = GetCluster(entry + i);
            if (cluster && !IsValidCluster(cluster))
                continue;

            // record should be ok if we are here
            nshort++;
        }
        else
        {
            // for Ord long name must be highest bit zero and Type and FstClusLO also zero
            DIR_ENTRY_LONG* l = (DIR_ENTRY_LONG*)(entry + i);
            if ((l->Ord != 0xE5 && (l->Ord & 0x80)) || l->Type || l->FstClusLO)
                continue;

            nlong++;
        }
    }
    //TRACE_X("nitems=" << nitems << " nshort=" << nshort << " nlong=" << nlong);

    // cluster with one record is possible provided it is short record
    if (nitems == 1)
    {
        if (nshort != 1)
            return 0;
    }
    else if (nitems < 10) // up to ten records all must be OK
    {
        if (nshort + nlong != nitems)
            return 0;
    }
    else // otherwise we give tolerance 10%
    {
        if (nshort + nlong < nitems * 9 / 10)
            return 0;
    }

    return 1;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ScheduleDeletedDirectory(DWORD cluster, FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE2("CFATSnapshot::ScheduleDeletedDirectory(%d, )", cluster);
    // add directory to appropriate heap
    if (cluster >= CurrentPos2)
        return HeapAhead2->Insert(cluster, 0, dir);
    else
        return HeapBehind2->Insert(cluster, 0, dir);
}

template <typename CHAR>
DWORD CFATSnapshot<CHAR>::GetNextDirCluster(BYTE* buffer, DWORD level, DWORD phase)
{
    CALL_STACK_MESSAGE3("CFATSnapshot::GetNextDirCluster(, %d, %d)", level, phase);

    // prevent infinite recursion (unlikely)
    if (level > 20)
    {
        TRACE_E("GetNextDirCluster: level > 20");
        return 0;
    }

    DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)buffer;
    DWORD i = 0;
    int lastshort = -1;
    DWORD max = this->Volume->BytesPerCluster / sizeof(DIR_ENTRY_SHORT);
    DWORD cluster;
    DWORD flags;

    // find last short record in cluster
    while (i < max && entry[i].Name[0])
    {
        if (!islong(entry[i].Attr) && (entry[i].FstClusLO || entry[i].FstClusHI))
            lastshort = i;
        i++;
    }
    if (!i || lastshort < 0 || isupdir(entry + lastshort))
        return 0;

    // get number of cluster
    cluster = GetCluster(entry + lastshort);

    // if it is a file, maybe we can end
    if (!(entry[lastshort].Attr & FAT_ATTR_DIRECTORY))
    {
        // move to latest cluster of the file
        cluster += (DWORD)(entry[lastshort].FileSize - 1) / this->Volume->BytesPerCluster + 1;
        if (!IsValidCluster(cluster))
            return 0;
        // if cluster is not full or we are first called, we have result
        if (level == 0 || i < max)
            return cluster;
    }

    // no: it is subdirectory or this directory continues -> read next cluster
    BYTE* temp;
    if (phase == 2)
    {
        temp = new BYTE[this->Volume->BytesPerCluster];
        if (temp == NULL || !this->Volume->ReadClusters(temp, cluster, 1))
        {
            if (temp)
                delete[] temp;
            return 0;
        }
    }
    else
    {
        CLUSTER* c = GetScannedCluster(cluster);
        if (c == NULL)
            return 0;
        temp = c->data;
    }
    TRACE_X("GetNextDirCluster: next cluster available: " << cluster);

    // is it really directory cluster?
    if (phase == 3 || AnalyzeCluster(temp, this->Volume->BytesPerCluster, &flags) != 0)
    {
        cluster = GetNextDirCluster(temp, level + 1, phase);
        if (phase == 2)
            delete[] temp;
        return cluster;
    }
    else
    {
        delete[] temp;
        return 0;
    }
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::LoadDeletedDirectories()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::LoadDeletedDirectories()");

    BYTE* buffer = NULL;
    DWORD nclus;
    DWORD bufmax = 0;
    DWORD cluster;
    DWORD flags;
    int ret;
    BOOL ret2 = FALSE;

#ifdef _WIN64
    int pointersCount = FILE_RECORD_Pointers.Count;
#endif // _WIN64

    while (!HeapAhead2->IsEmpty())
    {
        // empty heap before head
        CurrentPos2 = 0;
        while (!HeapAhead2->IsEmpty())
        {
            auto item = HeapAhead2->ExtractMin();

            if (item.dir->FileNames != NULL && item.dir->FileNames->FNName != NULL)
                TRACE_XW(L"##### " << CWStr(item.dir->FileNames->FNName).c_str() << L" #####");
            else
                TRACE_X("##### ??? #####");

            // read directory clusters
            nclus = 0;
            cluster = item.cluster;
            while (cluster &&                       // GetNextDirCluster can return zero
                   (FAT[cluster] & ~MARK_MASK) == 0 /*&& // cluster could be free,
             (FAT[cluster] & MARK_DEL_DIR) == 0*/
                   )                                // it must not be marked as already collected
                                                    // update: last test commented, maybe we will present some item multiple time, but it looks
                                                    // better than directory lost else
            {
                // reallocate buffer if needed
                if (nclus >= bufmax)
                {
                    bufmax = (bufmax == 0) ? 10 : 2 * bufmax;
                    BYTE* temp = new BYTE[bufmax * this->Volume->BytesPerCluster];
                    if (temp == NULL)
                    {
                        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                        goto error;
                    }
                    if (buffer != NULL)
                    {
                        memcpy(temp, buffer, nclus * this->Volume->BytesPerCluster);
                        delete[] buffer;
                    }
                    buffer = temp;
                }

                // read cluster
                BYTE* p = buffer + nclus * this->Volume->BytesPerCluster;
                if (!this->Volume->ReadClusters(p, cluster, 1))
                {
                    String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGDIRCLUSTERS);
                    goto error;
                }
                CurrentPos2 = cluster;

                // analyze content
                ret = AnalyzeCluster(p, this->Volume->BytesPerCluster, &flags);
                TRACE_X("cluster=" << cluster << " ret=" << ret << " flags=" << flags);
                if (!ret)
                    break; // it is not directory cluster

                // it is directory cluster, accept it
                nclus++;
                FAT[cluster] |= MARK_DEL_DIR; // mark
                // this cluster will not be used during ScanVacantClusters
                if (this->UdFlags & UF_SCANVACANTCLUSTERS)
                    ClustersTotal -= min(ClustersTotal, SCAN_WEIGHT);

                // scan for further deleted directories
                if (!ScanDirectoryCluster(p, this->Volume->BytesPerCluster, TRUE))
                    goto error;

                // if it was last, we are ending
                if (flags & DC_LAST)
                    break;

                // try to fine next one
                cluster = GetNextDirCluster(p, 0, 2);
            }

            // convert whole buffer to list of DIR_ITEM structures
            if (!DecodeDirectoryClusters(buffer, nclus * this->Volume->BytesPerCluster, &item.dir->DirItems, &item.dir->NumDirItems, TRUE))
                goto error;

#ifdef _WIN64
            // buffer content will be released, also pointers to buffer will be released
            if (FILE_RECORD_Pointers.Count > pointersCount)
                FILE_RECORD_Pointers.Delete(pointersCount, FILE_RECORD_Pointers.Count - pointersCount);
#endif // _WIN64

            if (this->Progress->GetWantCancel())
                goto error;
        }

        // swap ahead and behind heaps
        CClusterHeap<CHAR>* temp = HeapAhead2;
        HeapAhead2 = HeapBehind2;
        HeapBehind2 = temp;
    }

    ret2 = TRUE;
    TRACE_I("LoadDeletedDirectories: bufmax=" << bufmax);

error:
#ifdef _WIN64
    // buffer will be released, also pointers to buffer will be released
    if (FILE_RECORD_Pointers.Count > pointersCount)
        FILE_RECORD_Pointers.Delete(pointersCount, FILE_RECORD_Pointers.Count - pointersCount);
#endif // _WIN64

    delete[] buffer;
    return ret2;
}

// ****************************************************************************
//
//  3. phase: scan empty clusters
//

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ScanVacantClusters()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::ScanVacantClusters()");

    BYTE* buffer = new BYTE[SCAN_BUFFER * 1024];
    if (buffer == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    DWORD cluster = 2;
    DWORD bufcl = SCAN_BUFFER * 1024 / this->Volume->BytesPerCluster;
    DWORD numcl;
    int nfound = 0;

    do
    {
        // skip used and already loaded clusters
        while (cluster < this->Volume->FAT_CountOfClusters && FAT[cluster])
            cluster++;
        if (cluster >= this->Volume->FAT_CountOfClusters)
            break;

        // how much we will take at once?
        numcl = 1;
        while (cluster + numcl < this->Volume->FAT_CountOfClusters &&
               !FAT[cluster + numcl] && numcl < bufcl)
        {
            numcl++; // there should not be & ~MARK_MASK
        }

        // read cluster
        if (!this->Volume->ReadClusters(buffer, cluster, numcl))
        {
            delete[] buffer;
            return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGDIRCLUSTERS);
        }

        // analyze / add to ScanResults
        int oldfound = nfound;
        for (DWORD i = 0; i < numcl; i++)
        {
            DWORD flags;
            BYTE* p = buffer + i * this->Volume->BytesPerCluster;
            if (AnalyzeCluster(p, this->Volume->BytesPerCluster, &flags) != 0)
            {
                BYTE* data = new BYTE[this->Volume->BytesPerCluster];
                if (data == NULL)
                {
                    delete[] buffer;
                    return FALSE;
                }
                memcpy(data, p, this->Volume->BytesPerCluster);
                CLUSTER c = {cluster + i, flags, 0, data};
                ScanResults->Add(c);
                if (!ScanResults->IsGood())
                {
                    ScanResults->ResetState();
                    delete[] data;
                    delete[] buffer;
                    return FALSE;
                }
                nfound++;
            }
        }

        // update info line
        if (nfound != oldfound)
        {
            this->Progress->SetProgressText(IDS_SCANNINGFOUND, nfound);
        }

        // update progress
        ClustersProcessed += numcl * SCAN_WEIGHT;
        this->Progress->SetProgress(MulDiv(ClustersProcessed, 1000, ClustersTotal));
        if (this->Progress->GetWantCancel())
        {
            delete[] buffer;
            return FALSE;
        }
        cluster += numcl;
    } while (cluster < this->Volume->FAT_CountOfClusters);

    delete[] buffer;
    return TRUE;
}

template <typename CHAR>
typename CFATSnapshot<CHAR>::CLUSTER* CFATSnapshot<CHAR>::GetScannedCluster(DWORD cluster)
{
    CALL_STACK_MESSAGE2("CFATSnapshot::GetScannedCluster(%d)", cluster);
    CLUSTER c = {cluster, 0, NULL};
    return (CLUSTER*)bsearch(&c, ScanResults->GetData(), ScanResults->Count, sizeof(CLUSTER), compare_clusters);
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::DecodeScannedCluster(FILE_RECORD_I<CHAR>* record, CLUSTER* rclus)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::DecodeScannedCluster(, )");

    // count of directory clusters linked through 'next'?
    int num = 1;
    CLUSTER* c = rclus;
    while (c->next)
    {
        c = GetScannedCluster(c->next);
        if (c == NULL)
        {
            TRACE_E("Invalid should not be executed.");
            break;
        }
        num++;
    }

    // store directory clusters into on buffer for DecodeDirectoryClusters
    int len = num * this->Volume->BytesPerCluster;
    BYTE* buffer = new BYTE[len];
    if (buffer == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    BYTE* p = buffer;
    c = rclus;
    do
    {
        memcpy(p, c->data, this->Volume->BytesPerCluster);
        if (!c->next)
            break;
        c = GetScannedCluster(c->next);
        p += this->Volume->BytesPerCluster;
    } while (c != NULL);

#ifdef _WIN64
    int pointersCount = FILE_RECORD_Pointers.Count;
#endif // _WIN64

    // similar to ScanDirectoryCluster
    DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)buffer;
    DIR_ENTRY_SHORT* bufmax = (DIR_ENTRY_SHORT*)(buffer + len);
    BOOL ret = TRUE;
    while (entry < bufmax && entry->Name[0])
    {
        if ((entry->Attr & FAT_ATTR_DIRECTORY) && !IgnoreEntry(entry, TRUE))
        {
            FILE_RECORD_I<CHAR>* dir = new FILE_RECORD_I<CHAR>;
            if (dir == NULL)
            {
                ret = String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                goto error;
            }
            dir->IsDir = TRUE;
#ifdef _WIN64
            // store pointer to directory: to FileSize put index in array FILE_RECORD_Pointers
            // (we cannot place x64 pointer into FileSize)
            int ptrIndex = FILE_RECORD_Pointers.Add(dir);
            if (!FILE_RECORD_Pointers.IsGood())
            {
                FILE_RECORD_Pointers.ResetState();
                FreeRecord2(dir);
                ret = String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                goto error;
            }
            entry->FileSize = ptrIndex;
#else  // _WIN64
            entry->FileSize = (DWORD)dir;                           // store pointer to directory (it is possible in x86)
#endif // _WIN64
            if ((c = GetScannedCluster(GetCluster(entry))) != NULL)
            {
                if (!DecodeScannedCluster(dir, c))
                {
                    ret = FALSE;
                    goto error;
                }
            }
        }
        entry++;
    }

    // convert to list structure
    TRACE_X("%%%%%%%%%%%%%%%%");
    ret = DecodeDirectoryClusters(buffer, len, &record->DirItems, &record->NumDirItems, TRUE);

error:
#ifdef _WIN64
    // buffer will be released, also pointers to buffer will be released
    if (FILE_RECORD_Pointers.Count > pointersCount)
        FILE_RECORD_Pointers.Delete(pointersCount, FILE_RECORD_Pointers.Count - pointersCount);
#endif // _WIN64

    delete[] buffer;
    return ret;
}

#define DC_REFERENCED 0x0004

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ProcessScannedClusters()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::ProcessScannedClusters()");

    // clean flag DC_REFERENCED from possible previous use
    int i;
    for (i = 0; i < ScanResults->Count; i++)
        (*ScanResults)[i].flags &= ~DC_REFERENCED;

    for (i = 0; i < ScanResults->Count; i++)
    {
        CLUSTER* c = &(*ScanResults)[i];
        // mark cluster that is referenced by this cluster (if exist)
        DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)c->data;
        DIR_ENTRY_SHORT* max = (DIR_ENTRY_SHORT*)((BYTE*)entry + this->Volume->BytesPerCluster);
        while (entry < max)
        {
            if ((entry->Attr & FAT_ATTR_DIRECTORY) && !IgnoreEntry(entry, TRUE))
            {
                CLUSTER* ref = GetScannedCluster(GetCluster(entry));
                if (ref != NULL)
                    ref->flags |= DC_REFERENCED;
            }
            entry++;
        }
        // select following cluster
        if (!(c->flags & DC_LAST))
        {
            c->next = GetNextDirCluster(c->data, 0, 3);
            CLUSTER* ref = GetScannedCluster(c->next);
            if (ref != NULL)
                ref->flags |= DC_REFERENCED;
            else
                c->next = 0;
        }
    }

    // count of unreferenced clusters (will go to the root)
    int num = 0;
    for (i = 0; i < ScanResults->Count; i++)
    {
        if (!((*ScanResults)[i].flags & DC_REFERENCED))
            num++;
    }

    // reallocate root
    DIR_ITEM_I<CHAR>* temp = (DIR_ITEM_I<CHAR>*)realloc(this->Root->DirItems,
                                                        (this->Root->NumDirItems + num + 2 /* vdirs */) * sizeof(DIR_ITEM_I<CHAR>));
    if (temp == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    this->Root->DirItems = temp;

    // attach unreferenced clusters to the root and decode their subtrees
    for (i = 0; i < ScanResults->Count; i++)
    {
        CLUSTER* c = &(*ScanResults)[i];
        if (!(c->flags & DC_REFERENCED))
        {
            CHAR name[100];
            String<CHAR>::SPrintF(name, String<CHAR>::LoadStr(IDS_VDIRECTORY), c->cluster);
            FILE_RECORD_I<CHAR>* r;
            if (!AddFileOrDir(this->Root, name, TRUE, FR_FLAGS_DELETED, &r))
                return FALSE;
            if (!DecodeScannedCluster(r, c))
                return FALSE;
        }
    }

    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::ReloadScannedClusters()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::ReloadScannedClusters()");

    // prepare progress
    if (!ScanResults->Count)
        return TRUE;
    int cnt = ScanResults->Count;
    int div = FreeClusters * SCAN_WEIGHT / cnt;
    int mod = FreeClusters * SCAN_WEIGHT % cnt;
    int acc = 0;

    for (int i = 0; i < ScanResults->Count; i++)
    {
        // read cluster
        CLUSTER* c = &(*ScanResults)[i];
        if (c->data == NULL)
        {
            c->data = new BYTE[this->Volume->BytesPerCluster];
            if (c->data == NULL)
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            if (!this->Volume->ReadClusters(c->data, c->cluster, 1))
                return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGDIRCLUSTERS);

            // to be sure: if cluster doesn't contain directory, remove it also from ScanResults
            DWORD flags;
            if (AnalyzeCluster(c->data, this->Volume->BytesPerCluster, &flags) == 0)
            {
                delete[] c->data;
                ScanResults->Delete(i--);
            }
        }
        else
            TRACE_E("c->data is not NULL while reload!");

        // progress
        ClustersProcessed += div;
        acc += mod;
        if (acc >= cnt)
        {
            acc -= cnt;
            ClustersProcessed++;
        }
        if (!(i % 5))
            this->Progress->SetProgress(MulDiv(ClustersProcessed, 1000, ClustersTotal));
        if (this->Progress->GetWantCancel())
            return FALSE;
    }

    return TRUE;
}

template <typename CHAR>
void CFATSnapshot<CHAR>::FreeScannedClusters()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::FreeScannedClusters()");

    if (ScanResults == NULL)
        return;
    for (int i = 0; i < ScanResults->Count; i++)
    {
        CLUSTER* c = &(*ScanResults)[i];
        if (c->data != NULL)
        {
            delete[] c->data;
            c->data = NULL;
        }
    }
}

// ****************************************************************************
//
//  4. phase: estimate file damage
//

template <typename CHAR>
void CFATSnapshot<CHAR>::DrawDeletedFile(FILE_RECORD_I<CHAR>* file)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CFATSnapshot::DrawDeletedFile()");

    // get number of first cluster stored from DecodeDirectoryClusters
    DWORD cluster = file->Streams->FirstLCN;
    QWORD bytesleft = file->Streams->DSSize; // on FAT32 max file size is 4GB

    if (bytesleft == 0)
        return;

    // special case: if deleted file begins on (again) used clusters, skip it
    while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK))
    {
        cluster++;
        bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
    }
    if (bytesleft == 0 || !IsValidCluster(cluster))
        return;

    // for first sequence of unused cluster increase counter in marker
    while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK) == 0)
    {
        if ((FAT[cluster] & MARK_MASK) < 0x20000000)
            FAT[cluster] += 0x10000000;
        cluster++;
        bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
    }
}

template <typename CHAR>
void CFATSnapshot<CHAR>::GetFileCondition(FILE_RECORD_I<CHAR>* file)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("CFATSnapshot::GetFileCondition()");

    // get number of first cluster stored from DecodeDirectoryClusters
    DWORD cluster = file->Streams->FirstLCN;
    QWORD bytesleft = file->Streams->DSSize; // on FAT32 max file size is 4GB
    DWORD cnt[4] = {0, 0, 0, 0};

    // special case: if deleted file begins on (again) used clusters, skip it
    while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK))
    {
        cnt[3]++;
        cluster++;
        bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
    }

    if (bytesleft > 0 && IsValidCluster(cluster))
    {
        // for first sequence of unused cluster increase counter in marker
        while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK) == 0)
        {
            cnt[(FAT[cluster] >> 28) & 0x3]++;
            cluster++;
            bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
        }

        // increase cnt[3] if we found used space
        if (bytesleft > 0 && IsValidCluster(cluster))
            cnt[3]++;
    }

    // result
    file->Flags &= ~FR_FLAGS_CONDITION_MASK;
    if (cnt[0] != 0)
        TRACE_E("Internal error!");
    if (cnt[0] == 0 && cnt[1] == 0 && cnt[2] == 0)
        file->Flags |= FR_FLAGS_CONDITION_LOST;
    else if (cnt[3] != 0)
        file->Flags |= FR_FLAGS_CONDITION_POOR;
    else if (cnt[2] != 0)
        file->Flags |= FR_FLAGS_CONDITION_FAIR;
    else
        file->Flags |= FR_FLAGS_CONDITION_GOOD;
}

template <typename CHAR>
void CFATSnapshot<CHAR>::DeleteAllMarks()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::DeleteAllMarks()");
    for (DWORD cluster = 2; cluster < this->Volume->FAT_CountOfClusters; cluster++)
        FAT[cluster] &= ~MARK_MASK;
}

template <typename CHAR>
void CFATSnapshot<CHAR>::EstimateFileDamage(FILE_RECORD_I<CHAR>* dir, EstimateDamageTodo todo)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::EstimateFileDamage()");

    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = dir->DirItems[i].Record;
        if (r->IsDir)
        {
            // recursion to subdirectory
            EstimateFileDamage(r, todo);
        }
        else
        {
            switch (todo)
            {
            case edtDraw:
            {
                // first pass: mark deleted files to FAT
                if (r->Flags & FR_FLAGS_DELETED)
                    DrawDeletedFile(r);
                break;
            }

            case edtGetCondition:
            {
                // second pass: get file damage
                if (r->Flags & FR_FLAGS_DELETED)
                {
                    GetFileCondition(r);
                }
                else
                {
                    // existing file
                    r->Flags &= ~FR_FLAGS_CONDITION_MASK;
                    r->Flags |= FR_FLAGS_CONDITION_GOOD;
                }
                break;
            }

            case edtDrawLost:
            {
                // third pass (optional)
                // files with Condition FC_FAIR or FC_POOR render as "2 - there are more then one delete file in this place"
                // because we should return map of clusters which are not used by existing files and files which could be recovered (FC_GOOD)
                if (r->Flags & FR_FLAGS_DELETED)
                {
                    DWORD condition = r->Flags & FR_FLAGS_CONDITION_MASK;
                    if (condition == FR_FLAGS_CONDITION_FAIR || condition == FR_FLAGS_CONDITION_POOR)
                        DrawDeletedFile(r);
                }
                break;
            }

            default:
                TRACE_E("Unknown todo=" << todo);
            }
        }
    }
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::GetLostClustersMap(CLUSTER_MAP_I* clusterMap)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::GetLostClustersMap()");
    clusterMap->Free();

    // pass 0: get size of arrays that we should allocate and allocate them
    // pass 1: fill these arrays
    for (int pass = 0; pass < 2; pass++)
    {
        int clusterPairs = 0;
        BOOL inside = FALSE;
        BOOL flush = FALSE;
        QWORD lcnFirst = 0;
        QWORD i = 2;
        while (i < this->Volume->FAT_CountOfClusters)
        {
            // for cluster number 'i' find in FAT related two bits
            int c = (FAT[i] >> 28) & 0x3;
            if ((FAT[i] & ~MARK_MASK) == 0 || c == 2) // cluster is not used (or we don't know about it) || there is FC_FAIR or FC_POOR
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
                {
                    flush = TRUE;
                }
            }
            if (inside && (i >= this->Volume->FAT_CountOfClusters - 1))
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
            {
                break; // we can stope here, there is nothing to store
            }
        }
    }

    return TRUE;
}

// ****************************************************************************
//
//  5. phase: assign clusters to deleted files and encode in NTFS data-runs style
//

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::EncodeExistingChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file)
{
    //  CALL_STACK_MESSAGE1("CFATSnapshot::EncodeExistingChain()"); // j.r. this callstack should be commented out, it is slow (slowdown over 20%)

    // get number of first cluster stored from DecodeDirectoryClusters
    DWORD cluster = file->Streams->FirstLCN;

    // encode cluster chain in sequence of pairs [lcn, length]
    DWORD lcn = cluster;
    DWORD length = 1;
    do
    {
        DWORD next = FAT[cluster] & ~MARK_MASK;
        if (next == cluster + 1)
        {
            length++; // merge continuous sequence of clusters into one data run
        }
        else
        {
            if (!tmpRunsBuffer->EncodeRun(lcn, length))
                return FALSE;
            lcn = next;
            length = 1;
        }
        cluster = next;
    } while (cluster < EOC); // type case (DWORD)cluster is OK

    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::EncodeDeletedChain(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* file)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::EncodeDeletedChain()");

    // get number of first cluster stored from DecodeDirectoryClusters
    DWORD cluster = file->Streams->FirstLCN;
    QWORD bytesleft = file->Streams->DSSize; // on FAT32 max file size is 4GB

    // special case: if deleted files begins on (again) used clusters, take them
    // - we will hope only beginning is damaged
    DWORD lcn = cluster;
    DWORD length = 0;
    while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK))
    {
        cluster++;
        length++;
        bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
    }

    // if whole file was encoded, file is lost
    /*if (bytesleft <= 0 || !IsValidCluster(cluster))
  {
    // set zero size
    file->FileSize = 0;
    return TRUE;
  }*/

    // main loop
    while (1)
    {
        // get sequence of unused clusters
        while (bytesleft > 0 && IsValidCluster(cluster) && (FAT[cluster] & ~MARK_MASK) == 0)
        {
            cluster++;
            length++;
            bytesleft -= min(bytesleft, this->Volume->BytesPerCluster);
        }

        // encode sequence; is it end?
        if (!tmpRunsBuffer->EncodeRun(lcn, length))
            return FALSE;
        if (bytesleft == 0 || !IsValidCluster(cluster))
        {
            if (bytesleft > 0)
            {
                file->Streams->DSSize -= min(file->Streams->DSSize, bytesleft);
                file->Streams->DSValidSize = file->Streams->DSSize; // sync with DSSize (FAT doesn't support valid data)
            }
            return TRUE;
        }

        // no: skip used space and hope file continues after it
        while (FAT[cluster] & ~MARK_MASK)
        {
            cluster++;
            if (!IsValidCluster(cluster))
                return TRUE;
        }
        lcn = cluster;
        length = 0;
    }
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::EncodeClusterChains(CRunsBuffer<CHAR>* tmpRunsBuffer, FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::EncodeClusterChains()");

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
            if (r->Flags & FR_FLAGS_DELETED)
            {
                if (!EncodeDeletedChain(tmpRunsBuffer, r))
                    return FALSE;
            }
            else
            {
                if (!EncodeExistingChain(tmpRunsBuffer, r))
                    return FALSE;
            }

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

// ****************************************************************************
//
//  Update()
//

template <typename CHAR>
void CFATSnapshot<CHAR>::FilterZeroFiles(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::FilterZeroFiles()");

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
BOOL CFATSnapshot<CHAR>::FilterExistingDirectories(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::FilterExistingDirectories()");
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
BOOL CFATSnapshot<CHAR>::FilterEmptyDirectories(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::FilterEmptyDirectories()");

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
void CFATSnapshot<CHAR>::CountClusters()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::CountClusters()");

    UsedClusters = 0;
    FreeClusters = 0;
    for (DWORD i = 2; i < this->Volume->FAT_CountOfClusters; i++) // what about first two items?
    {
        if (FAT[i])
            UsedClusters++;
        else
            FreeClusters++;
    }
    TRACE_I("UsedClusters=" << UsedClusters << " FreeClusters=" << FreeClusters);
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::AddFileOrDir(FILE_RECORD_I<CHAR>* dir, CHAR* name, BOOL isdir, DWORD flags, FILE_RECORD_I<CHAR>** newrec)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE3("CFATSnapshot::AddFileOrDir(, , %d, %d, )", isdir, flags);

    FILE_RECORD_I<CHAR>* r = new FILE_RECORD_I<CHAR>;
    FILE_NAME_I<CHAR>* fn = new FILE_NAME_I<CHAR>;
    fn->FNName = String<CHAR>::NewStr(name);
    if (r == NULL || fn == NULL || fn->FNName == NULL)
    {
        if (r != NULL)
            delete r;
        if (fn != NULL && fn->FNName != NULL)
            delete fn->FNName;
        if (fn != NULL)
            delete fn;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    r->IsDir = isdir;
    r->Attr = isdir ? FILE_ATTRIBUTE_DIRECTORY : 0;
    r->Flags = flags;
    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &r->TimeCreation);
    r->TimeLastWrite = r->TimeCreation;
    r->TimeLastAccess = r->TimeCreation;
    r->FileNames = fn;
    DIR_ITEM_I<CHAR>* di = dir->DirItems + dir->NumDirItems;
    di->FileName = fn;
    di->Record = r;
    dir->NumDirItems++;
    *newrec = r;
    return TRUE;
}

template <typename CHAR>
DWORD CFATSnapshot<CHAR>::AddDeletedFiles(FILE_RECORD_I<CHAR>* dir, FILE_RECORD_I<CHAR>* deletedfiles)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::AddDeletedFiles(, )");

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
int CFATSnapshot<CHAR>::compare_clusters(const void* elem1, const void* elem2)
{
    return ((CFATSnapshot<CHAR>::CLUSTER*)elem1)->cluster - ((CFATSnapshot<CHAR>::CLUSTER*)elem2)->cluster;
}

template <typename CHAR>
int CFATSnapshot<CHAR>::compare_names(const void* elem1, const void* elem2)
{
    DIR_ITEM_I<CHAR>* di1 = (DIR_ITEM_I<CHAR>*)elem1;
    DIR_ITEM_I<CHAR>* di2 = (DIR_ITEM_I<CHAR>*)elem2;
    return String<CHAR>::StrICmp(di1->Record->FileNames->FNName, di2->Record->FileNames->FNName);
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::RemoveDuplicateFiles(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::RemoveDuplicateFiles()");

    if (record->NumDirItems < 2)
        return TRUE;

    // sort files by name
    qsort(record->DirItems, record->NumDirItems, sizeof(DIR_ITEM_I<CHAR>), compare_names);

    // remove files with same name and same data runs
    DIR_ITEM_I<CHAR>* di = record->DirItems;
    DWORD j = 0;
    for (DWORD i = 1; i < record->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r1 = di[j].Record;
        FILE_RECORD_I<CHAR>* r2 = di[i].Record;
        if (String<CHAR>::StrICmp(r1->FileNames->FNName, r2->FileNames->FNName) != 0 ||
            r1->Streams == NULL || r1->Streams->Ptrs == NULL ||
            r2->Streams == NULL || r2->Streams->Ptrs == NULL ||
            r1->Streams->DSSize != r2->Streams->DSSize ||
            memcmp(r1->Streams->Ptrs, r2->Streams->Ptrs, (size_t)r1->Streams->DSSize) != 0)
        {
            j++;
            di[j] = di[i];
        }
    }
    j++; // first item (on zero index)

    // shrink DirItems
    if (record->NumDirItems != j)
    {
        // reallocation to smaller size should not be problem
        record->DirItems = (DIR_ITEM_I<CHAR>*)realloc(record->DirItems, j * sizeof(DIR_ITEM_I<CHAR>));
        if (j > 0 && record->DirItems == NULL)
        {
            record->NumDirItems = 0;
            return FALSE;
        }
        record->NumDirItems = j;
    }
    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::AddVirtualDirs()
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::AddVirtualDirs()");

    FILE_RECORD_I<CHAR>* deletedfiles;
    FILE_RECORD_I<CHAR>* metafiles;

    // add virtual directories {All Deleted Files} and {Metafiles} into root
    if (!AddFileOrDir(this->Root, String<CHAR>::LoadStr(IDS_ALLDELETEDFILES), TRUE, FR_FLAGS_VIRTUALDIR, &deletedfiles))
        return FALSE;
    VirtualDirsCount++;
    if (this->UdFlags & UF_SHOWMETAFILES)
    {
        if (!AddFileOrDir(this->Root, String<CHAR>::LoadStr(IDS_METAFILES), TRUE, FR_FLAGS_VIRTUALDIR, &metafiles))
            return FALSE;
        VirtualDirsCount++;
    }

    // fill {All Deleted Files}
    this->Root->NumDirItems -= VirtualDirsCount;           // don't add virtual dirs to {All Deleted Files}
    int allFilesCount = AddDeletedFiles(this->Root, NULL); // get total count of virtual files
    deletedfiles->DirItems = new DIR_ITEM_I<CHAR>[allFilesCount];
    if (deletedfiles->DirItems == NULL)
    {
        this->Root->NumDirItems += VirtualDirsCount;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    AddDeletedFiles(this->Root, deletedfiles);
    this->Root->NumDirItems += VirtualDirsCount;

    // because function LoadDeletedDirectories could read some directories multiple time (prevents
    // user confusion why directory moved elsewhere), we must remove duplicates in {All Deleted Files}
    if (!RemoveDuplicateFiles(deletedfiles))
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);

    // add $Boot, $FAT0, and $FAT1 into {Metafiles}
    if (this->UdFlags & UF_SHOWMETAFILES)
    {
        // these metafiles are currently not accessible through CStreamReader
        /*
    metafiles->NumDirItems = 0;
    metafiles->DirItems = new DIR_ITEM_I<CHAR>[3];
    if (metafiles->DirItems == NULL)
      return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    FILE_RECORD_I<CHAR> *boot;
    FILE_RECORD_I<CHAR> *fat0;
    FILE_RECORD_I<CHAR> *fat1;
    if (!AddFileOrDir(metafiles, "$Boot", FALSE, FR_FLAGS_METAFILE, &boot) ||
        !AddFileOrDir(metafiles, "$FAT0", FALSE, FR_FLAGS_METAFILE, &fat0) ||
        !AddFileOrDir(metafiles, "$FAT1", FALSE, FR_FLAGS_METAFILE, &fat1))
    {
      return FALSE;
    }
    */
        // there could be more than 2 FATs
    }

    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::RenameDuplicateDirectories(FILE_RECORD_I<CHAR>* dir)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CFATSnapshot::RenameDuplicateDirectories()");

    if (dir->NumDirItems < 2)
        return TRUE;

    // sort by name
    qsort(dir->DirItems, dir->NumDirItems, sizeof(DIR_ITEM_I<CHAR>), compare_names);

    // rename duplicates
    DIR_ITEM_I<CHAR>* di = dir->DirItems;
    for (DWORD i = 0; i < dir->NumDirItems - 1; i++)
    {
        DWORD j = i;
        while (j + 1 < dir->NumDirItems &&
               String<CHAR>::StrICmp(di[i].Record->FileNames->FNName, di[j + 1].Record->FileNames->FNName) == 0)
        {
            j++;
        }

        if (j > i)
        {
            int n = 1;
            for (DWORD k = i; k <= j; k++)
            {
                FILE_RECORD_I<CHAR>* r = di[k].Record;
                // if it is deleted file, add number suffix (before extension, for example test.txt -> test (1).txt)
                if (r->Flags & FR_FLAGS_DELETED)
                {
                    CHAR* newname = String<CHAR>::AddNumberSuffix(r->FileNames->FNName, n++);
                    if (newname == NULL)
                        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                    delete[] r->FileNames->FNName;
                    r->FileNames->FNName = newname;
                }
            }
            i = j;
        }
    }

    // recursion into subdirectories
    for (DWORD i = 0; i < dir->NumDirItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = dir->DirItems[i].Record;
        if (r->IsDir)
        {
            if (!RenameDuplicateDirectories(r))
                return FALSE;
        }
    }

    return TRUE;
}

template <typename CHAR>
BOOL CFATSnapshot<CHAR>::Update(CSnapshotProgressDlg* progress, DWORD udFlags, CLUSTER_MAP_I** clusterMap)
{
    CALL_STACK_MESSAGE2("CFATSnapshot::Update(, , 0x%X, )", udFlags);

    TRACE_I("CFATSnapshot::Update(), udFlags=" << udFlags);

    if (udFlags & UF_GETLOSTCLUSTERMAP)
        udFlags |= UF_ESTIMATEDAMAGE;

    // initialization
    this->Progress = progress;
    this->UdFlags = udFlags;
    if (clusterMap != NULL)
        *clusterMap = NULL;

    if (!this->Volume->IsOpen())
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTOPEN);
    if (this->Volume->Type != vtFAT)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTFAT);
    Free();

    // load FAT
    this->Progress->SetProgressText(IDS_LOADINGFAT);
    if (!LoadFAT())
        return FALSE;

    // prepare progress
    CountClusters();
    ClustersProcessed = 0;
    ClustersTotal = UsedClusters;
    if (this->UdFlags & UF_SCANVACANTCLUSTERS)
        ClustersTotal += FreeClusters * SCAN_WEIGHT;

    // initialize heaps for first and second phase
    CurrentPos1 = 0;
    CurrentPos2 = 0;
    HeapAhead1 = &Heap1A;
    HeapBehind1 = &Heap1B;
    HeapAhead2 = &Heap2A;
    HeapBehind2 = &Heap2B;

    // load existing directories
    this->Progress->SetProgressText(IDS_READINGEXISTINGDIRS);
    BOOL ret = LoadExistingDirectories();
    Heap1A.Release();
    Heap1B.Release();
    if (!ret)
    {
        Heap2A.Release();
        Free();
        return FALSE;
    }

    // load deleted directories
    this->Progress->SetProgressText(IDS_READINGDELETEDDIRS);
    ret = LoadDeletedDirectories();
    Heap2A.Release();
    Heap2B.Release();
    if (!ret)
    {
        Free();
        return FALSE;
    }

    // scan free clusters
    if (this->UdFlags & UF_SCANVACANTCLUSTERS)
    {
        this->Progress->SetProgressText(IDS_SCANNINGVACANTCLUSTERS);

        MSGBOXEX_PARAMS mbep;
        memset(&mbep, 0, sizeof(mbep));
        mbep.HParent = this->Progress->HWindow;
        mbep.Text = String<char>::LoadStr(IDS_REUSESCANNEDINFO);
        mbep.Caption = String<char>::LoadStr(IDS_UNDELETE);
        mbep.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_ESCAPEENABLED;
        mbep.HIcon = NULL;
        mbep.HelpCallback = NULL;
        mbep.CheckBoxText = String<char>::LoadStr(IDS_ALWAYSCHOOSEYES);
        mbep.CheckBoxValue = &ConfigAlwaysReuseScanInfo;
        mbep.AliasBtnNames = NULL;

        if (ScanResults == NULL ||
            (!ConfigAlwaysReuseScanInfo && SalamanderGeneral->SalMessageBoxEx(&mbep) == DIALOG_NO))
        {
            // full scan
            if (ScanResults != NULL)
                delete ScanResults;
            ScanResults = new TDirectArray<CLUSTER>(256, 256);
            if (ScanResults != NULL)
            {
                if (!ScanVacantClusters() || !ProcessScannedClusters())
                {
                    FreeScannedClusters();
                    delete ScanResults;
                    ScanResults = NULL;
                }
            }
            else
            {
                String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM); // we will not return FALSE because we already have something
            }
        }
        else
        {
            // just clusters reload
            this->Progress->SetProgressText(IDS_RELOADINGCLUSTERS);
            if (!ReloadScannedClusters() || !ProcessScannedClusters())
            {
                FreeScannedClusters();
                delete ScanResults;
                ScanResults = NULL;
            }
        }
        // release scanned clusters (but keep information about their location)
        FreeScannedClusters();
    }

    // set progress to 100%
    this->Progress->SetProgress(1000);

    // filter undeleted (existing) directories (files are filtered during reading)
    if (!(this->UdFlags & UF_SHOWEXISTING))
        FilterExistingDirectories(this->Root);

    // estimate file damage
    DeleteAllMarks();
    if (this->UdFlags & UF_ESTIMATEDAMAGE)
    {
        EstimateFileDamage(this->Root, edtDraw);
        EstimateFileDamage(this->Root, edtGetCondition);
    }
    if (this->UdFlags & UF_GETLOSTCLUSTERMAP) // is lost cluster map required?
    {
        if (clusterMap != NULL)
        {
            EstimateFileDamage(this->Root, edtDrawLost);
            *clusterMap = (CLUSTER_MAP_I*)&cluster_map;
            if (!GetLostClustersMap(*clusterMap))
                *clusterMap = NULL; // out of memory - return null
        }
        else
        {
            TRACE_E("UF_GETLOSTCLUSTERMAP is specified, but clusterMap is NULL!");
        }
    }

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

    // rename duplicate directories
    RenameDuplicateDirectories(this->Root);

    // add {All Deleted Files} and {Metafiles} virtual directories
    AddVirtualDirs();

    // we don't need FAT anymore
    delete[] FAT;
    FAT = NULL;

    TRACE_I("ClustersProcessed=" << ClustersProcessed);
    return TRUE;
}

template <typename CHAR>
void CFATSnapshot<CHAR>::FreeRecord2(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE1("CFATSnapshot::FreeRecord2()");

    if (record->Streams != NULL)
    {
        // this directory is in transitional reading phase (reading was not finished due to error or cancel)
        DIR_ENTRY_SHORT* entry = (DIR_ENTRY_SHORT*)record->DirItems;
        DIR_ENTRY_SHORT* max = (DIR_ENTRY_SHORT*)((BYTE*)record->DirItems + record->NumDirItems * this->Volume->BytesPerCluster);
        while (1)
        {
            // skip eventually loaded clusters
            while (entry < max && !entry->Name[0])
                entry = (DIR_ENTRY_SHORT*)((BYTE*)entry + this->Volume->BytesPerCluster);
            if (entry >= max)
                break;

            // free what was allocated by ScanDirectoryCluster
            if ((entry->Attr & FAT_ATTR_DIRECTORY) && !IgnoreEntry(entry, FALSE /* deleteddir doesn't matter */))
            {
#ifdef _WIN64
                // pointer to FileSize doesn't fit on x64, so we store only index into FILE_RECORD_Pointers array
                FreeRecord2(FILE_RECORD_Pointers[entry->FileSize]);
#else  // _WIN64
                FreeRecord2((FILE_RECORD_I<CHAR>*)entry->FileSize); // on x86 pointer fits directly into FileSize
#endif // _WIN64
            }
            entry++;
        }
        record->Streams = NULL;
    }
    else
    {
        // standard directory
        for (DWORD i = 0; i < record->NumDirItems; i++)
        {
            FILE_RECORD_I<CHAR>* r = record->DirItems[i].Record;
            if (r->IsDir)
                FreeRecord2(r);
            else
                delete this->FreeRecord(r);
        }
    }
    delete this->FreeRecord(record);
}

template <typename CHAR>
void CFATSnapshot<CHAR>::Free()
{
    CALL_STACK_MESSAGE1("CFATSnapshot::Free()");

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

#ifdef _WIN64
    FILE_RECORD_Pointers.DestroyMembers(); // used also in FreeRecord2()
#endif                                     // _WIN64

    // to FAT variable is accessed from FreeRecord2() -> IgnoreEntry(), we could not free it until now
    if (FAT != NULL)
    {
        delete[] FAT;
        FAT = NULL;
    }
}
