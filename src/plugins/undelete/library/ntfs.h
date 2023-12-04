// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// attribute codes
#define $STANDARD_INFORMATION 0x10
#define $ATTRIBUTE_LIST 0x20
#define $FILE_NAME 0x30
#define $VOLUME_INFORMATION 0x70
#define $DATA 0x80
#define $LOGGED_UTILITY_STREAM 0x100

#define FILE_RECORD_SIGNATURE 0x454C4946 // 'FILE'
#define ATTR_END_MARKER 0xFFFFFFFF
#define MAX_METAFILES 24

// on-disk structures
#pragma pack(push, ntfs_h)
#pragma pack(1)

// FILE_RECORD_HEADER flags
#define FRHFLAG_RECORD_IS_IN_USE 0x01
#define FRHFLAG_RECORD_IS_DIRECTORY 0x02

struct FILE_RECORD_HEADER
{
    DWORD Signature;
    WORD UpdateOffset;
    WORD UpdateSize;
    QWORD LSN;
    WORD SeqNumber;
    WORD HardLinks;
    WORD FirstAttrOffset;
    WORD FRHFlags;
    DWORD RealSize;
    DWORD AllocSize;
    QWORD BaseRecord;
    WORD NextAttrId;
};

struct STANDARD_ATTRIBUTE_HEADER
{
    DWORD Type;
    DWORD Length;
    BYTE NonResident;
    BYTE NameLength;
    WORD NameOffset;
    WORD SAHFlags;
    WORD AttrId;
    union
    {
        struct // resident attribute
        {
            DWORD AttrLength;
            WORD AttrOffset;
            BYTE Indexed;
            BYTE Padding1;
        };
        struct // nonresident attribute
        {
            QWORD StartVCN;
            QWORD LastVCN;
            WORD DataRunsOffset;
            BYTE CompressionUnit;
            BYTE Padding2[5];
            QWORD AttrAllocSize;
            QWORD AttrRealSize;
            QWORD AttrInitSize;
        };
    };
};

struct ATTRIBUTE_STANDARD_INFORMATION
{
    FILETIME TimeCreated;
    FILETIME TimeModified;
    FILETIME TimeMFTChanged;
    FILETIME TimeAccessed;
    DWORD Attributes;
    DWORD MaxVersions;
    DWORD VersionNumber;
    DWORD ClassId;
};

struct ATTRIBUTE_FILE_NAME
{
    QWORD ParentDir;
    FILETIME TimeCreated;
    FILETIME TimeModified;
    FILETIME TimeMFTChanged;
    FILETIME TimeAccessed;
    QWORD AllocSize;
    QWORD RealSize;
    DWORD Flags;
    DWORD EaReparse;
    BYTE FileNameLength;
    BYTE Namespace;
    WCHAR FileName[1];
};

struct ATTRIBUTE_VOLUME_INFORMATION
{
    BYTE DontCare1[8];
    BYTE VerMajor;
    BYTE VerMinor;
    WORD Flags;
    BYTE DontCare2[4];
};

#pragma pack(pop, ntfs_h)

template <typename CHAR>
class CMFTSnapshot : public CSnapshot<CHAR>
{
public:
    CMFTSnapshot(CVolume<CHAR>* volume);
    virtual ~CMFTSnapshot() { Free(); };
    virtual BOOL Update(CSnapshotProgressDlg* progress, DWORD udflags, CLUSTER_MAP_I** clusterMap);
    virtual void Free();

    FILE_RECORD_I<CHAR>** MFT;
    QWORD MFTItems;

private:
    BOOL ParseRecord(BYTE* data, QWORD index);
    BOOL BuildDirectoryTree();
    BOOL IsValidRef(QWORD fileref);
    void Mark(FILE_RECORD_I<CHAR>* r, DWORD depth);
    BOOL IsMetafile(DWORD index);
    void AddDirItem(FILE_RECORD_I<CHAR>* record, FILE_RECORD_I<CHAR>* item, FILE_NAME_I<CHAR>* fname);
    BOOL AllocDirItems(FILE_RECORD_I<CHAR>* record);
    FILE_RECORD_I<CHAR>* GetVirtualDirectory(DWORD ref, int resID, DWORD attr, DWORD flags);
    BOOL LoadClusterBitmap(CClusterBitmap* clusterBitmap);
    void DrawDeletedFile(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap);
    DWORD GetFileCondition(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap);
    BOOL EstimateFileDamage(CLUSTER_MAP_I** clusterMap);
    BOOL GetLostClustersMap(CClusterBitmap* clusterBitmap, CLUSTER_MAP_I* clusterMap);

#ifdef TRACE_ENABLE
    QWORD GetRecordMemUsage(FILE_RECORD_I<CHAR>*);
    QWORD GetTotalMemUsage();
#endif //TRACE_ENABLE

#ifdef TRACE_ENABLE
    void DumpHexData(BYTE* data, DWORD len);
#endif

    DWORD BytesPerMFTRecord;
    DWORD SectorsPerMFTRecord;
    BOOL ErrorsFound;
    BOOL RootAllocated; // fixme: don't forget ErrorsFound on higher levels
    TIndirectArray<VIRTUAL_DIR<CHAR>> VirtualDirs;

    QWORD ClustersProcessed; // current progress in clusters
    QWORD ClustersTotal;     // total progress in clusters

    static const CHAR* const STRING_EMPTY;
    static const CHAR* const STRING_DOT;
    static const CHAR* const STRING_MFT;
    static const CHAR* const STRING_EFS;
    static const CHAR* const STRING_BITMAP;

    static const CHAR* const TEST_CONST;
};

// **************************************************************************************
//
//   CMFTSnapshot
//

template <typename CHAR>
CMFTSnapshot<CHAR>::CMFTSnapshot(CVolume<CHAR>* volume)
    : CSnapshot<CHAR>(volume),
      VirtualDirs(10, 10, dtNoDelete)
{
    MFT = NULL;
    RootAllocated = FALSE;
}

#ifdef TRACE_ENABLE
template <typename CHAR>
void CMFTSnapshot<CHAR>::DumpHexData(BYTE* data, DWORD len)
{
    char text1[64];
    char text2[32];
    char text3[10];
    BYTE* p = data;
    DWORD offset = 0;

    while (len)
    {
        text1[0] = 0;
        text2[0] = 0;
        int i;
        for (i = 0; len > 0 && i < 16; i++, p++, len--)
        {
            sprintf(text1 + strlen(text1), "%02X ", (int)*p);
            if (!((i + 1) & 3))
                strcat(text1, " ");
            text2[i] = (*p < 32) ? '.' : *p;
        }
        text2[16] = 0;
        for (i = 52 - (int)strlen(text1); i > 0; i--)
            strcat(text1, " ");
        sprintf(text3, "%04X: ", offset);
        offset += 16;
        TRACE_I(text3 << text1 << text2);
    }
}
#else
#define DumpHexData(x, y) __TraceEmptyFunction()
#endif

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::ParseRecord(BYTE* data, QWORD index)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE2("CMFTSnapshot::ParseRecord(, %d)", index);

    FILE_RECORD_HEADER* rheader = (FILE_RECORD_HEADER*)data;
    BOOL meta = (index < MAX_METAFILES);

    // signature check
    if (rheader->Signature != FILE_RECORD_SIGNATURE)
    {
        char* sig = (char*)&rheader->Signature;
        TRACE_I("Invalid signature (record no. " << index << " ) - " << sig[0] << sig[1] << sig[2] << sig[3]);
        DumpHexData(data, BytesPerMFTRecord);
        /*if (meta)
      return Error(IDS_UNDELETE, IDS_ERRORMETADAMAGED); // metafiles must be OK
    else*/
        // fixme // update: on NT4 are records 16-24 unused and doesn't have header, we can skip this check
        return TRUE; // looks like one of 'BAAD' records, skip it
    }

    if (!meta && !(this->UdFlags & UF_SHOWEXISTING))
    {
        // if it isn't directory and record is used, skip it
        // don't take rheader->BaseRecord into account here, we want to hide (existing) files with BaseRecord != 0
        if (!(rheader->FRHFlags & FRHFLAG_RECORD_IS_DIRECTORY) && (rheader->FRHFlags & FRHFLAG_RECORD_IS_IN_USE))
            return TRUE;
    }

    WORD* update = (WORD*)(data + rheader->UpdateOffset);
    WORD* lastw = (WORD*)(data + this->Volume->NTFSBoot.BytesPerSector - 2);
    WORD seqnum = *update++;
    for (DWORD i = 0; i < SectorsPerMFTRecord; i++)
    {
        if (*lastw != seqnum)
        {
            ErrorsFound = TRUE;
            TRACE_E("MFT entry index " << index << " is corrupted "
                                       << "(i = " << i << ")");
            DumpHexData(data, BytesPerMFTRecord);
            if (index == 0)
                return String<CHAR>::Error(IDS_UNDELETE, IDS_MFTRECORDDAMAGED); // we cannot skip MFT
            else
                return TRUE;
        }
        *lastw = *update++;
        lastw += this->Volume->NTFSBoot.BytesPerSector / 2;
    }

    // go to baserecord
    if (rheader->BaseRecord != 0)
    {
        if (index == 0)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_MFTRECORDDAMAGED);

        QWORD oldindex = index;
        index = LODWORD(rheader->BaseRecord);
        TRACE_X("Extension record: " << oldindex << " -> " << index);
        if (index == 0)
        {
            TRACE_I("Enlargement MFT! (" << oldindex << ")");
            DumpHexData(data, BytesPerMFTRecord);
        }
    }

    // create a new record of arrays in MFT
    FILE_RECORD_I<CHAR>*& mftr = MFT[index];
    if (mftr == NULL)
    {
        mftr = new FILE_RECORD_I<CHAR>;
        if (mftr == NULL)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    FILE_RECORD_I<CHAR>* record = mftr;

    // set basic record information
    if (rheader->BaseRecord == 0)
    {
        record->IsDir = (rheader->FRHFlags & FRHFLAG_RECORD_IS_DIRECTORY) ? TRUE : FALSE;
        //record->SeqNum = seqnum;
    }

    // walk through all attributes
    DWORD offset = rheader->FirstAttrOffset;
    STANDARD_ATTRIBUTE_HEADER* aheader;
    while ((aheader = (STANDARD_ATTRIBUTE_HEADER*)(data + offset))->Type != ATTR_END_MARKER)
    {
        switch (aheader->Type)
        {
        case $STANDARD_INFORMATION:
        {
            if (aheader->NonResident)
            {
                TRACE_E("$STANDARD_INFORMATION is non-resident?! (index " << index << ")");
                break; // should be always resident
            }
            ATTRIBUTE_STANDARD_INFORMATION* attr = (ATTRIBUTE_STANDARD_INFORMATION*)(data + offset + aheader->AttrOffset);
            record->Attr = attr->Attributes;
            if (!(rheader->FRHFlags & FRHFLAG_RECORD_IS_IN_USE))
            {
                record->Flags |= FR_FLAGS_DELETED;
            }
            else if (!record->IsDir)
            {
                record->Flags &= ~FR_FLAGS_CONDITION_MASK;
                record->Flags |= FR_FLAGS_CONDITION_GOOD;
            }
            record->TimeCreation = attr->TimeCreated;
            record->TimeLastAccess = attr->TimeAccessed;
            record->TimeLastWrite = attr->TimeModified;
            break;
        }

        case $FILE_NAME:
        {
            if (aheader->NonResident)
            {
                TRACE_E("$FILE_NAME is non-resident?! (index " << index << ")");
                break; // should be always resident
            }
            ATTRIBUTE_FILE_NAME* attr = (ATTRIBUTE_FILE_NAME*)(data + offset + aheader->AttrOffset);

            // look if we don't have name with same parent
            FILE_NAME_I<CHAR>* fname = record->FileNames;
            BOOL ignore = FALSE;
            while (fname != NULL)
            {
                if (fname->ParentRecord == attr->ParentDir)
                {
                    if (attr->Namespace == 2)
                        ignore = TRUE; // we can ignore DOS names if we have standard names
                    break;             // ... otherwise overwrite existing
                                       // todo: we can show DOS name too
                }
                fname = fname->FNNext;
            }
            if (ignore)
                break;

            // do we need record?
            if (fname == NULL)
            {
                fname = new FILE_NAME_I<CHAR>;
                if (fname == NULL)
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                fname->FNNext = record->FileNames;
                record->FileNames = fname;
            }

            // store name
            if (fname->FNName != NULL)
                delete[] fname->FNName;
            fname->FNName = String<CHAR>::NewFromUnicode(attr->FileName, attr->FileNameLength);
            if (fname->FNName == NULL)
                return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
            fname->ParentRecord = attr->ParentDir;
            break;
        }

        case $DATA:
        case $LOGGED_UTILITY_STREAM:
        {
            //TRACE_I("Found data/stream.");

            // do we have already stream with same name?
            CHAR streamname[MAX_PATH];
            if (!String<CHAR>::CopyFromUnicode(streamname, (WCHAR*)(data + offset + aheader->NameOffset), aheader->NameLength, MAX_PATH))
                return String<CHAR>::Error(IDS_UNDELETE, IDS_READINGMFT);

            // for attribute $LOGGED_UTILITY_STREAM we are interested only in these related to EFS
            if (aheader->Type == $LOGGED_UTILITY_STREAM && String<CHAR>::StrICmp(streamname, STRING_EFS))
                break;
            DATA_STREAM_I<CHAR>* stream = record->Streams;
            DATA_STREAM_I<CHAR>** lastptr = &record->Streams;
            while (stream != NULL)
            {
                if (!String<CHAR>::StrICmp(streamname, (stream->DSName == NULL) ? STRING_EMPTY : stream->DSName))
                    break;
                lastptr = &stream->DSNext;
                stream = stream->DSNext;
            }

            // if no, create it
            if (stream == NULL)
            {
                stream = new DATA_STREAM_I<CHAR>;
                if (stream == NULL)
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                if (aheader->NameLength)
                {
                    stream->DSName = String<CHAR>::NewStr(streamname);
                    if (stream->DSName == NULL)
                        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }
                stream->DSNext = NULL;
                *lastptr = stream;
            }

            // store data runs or resident data
            if (aheader->NonResident)
            {
                DATA_POINTERS* ptrs = new DATA_POINTERS;
                if (ptrs == NULL)
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                ptrs->DPFlags = aheader->SAHFlags;
                ptrs->StartVCN = aheader->StartVCN;
                ptrs->LastVCN = aheader->LastVCN;
                ptrs->RunsSize = aheader->Length - aheader->DataRunsOffset;
                ptrs->Runs = new BYTE[ptrs->RunsSize];
                if (ptrs->Runs == NULL)
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                memcpy(ptrs->Runs, data + offset + aheader->DataRunsOffset, ptrs->RunsSize);
                ptrs->CompUnit = aheader->CompressionUnit;

                // store to the correct place inside list
                DATA_POINTERS** p = &stream->Ptrs;
                while (*p != NULL && (*p)->StartVCN < ptrs->StartVCN)
                    p = &(*p)->DPNext;
                ptrs->DPNext = *p;
                *p = ptrs;

                if (stream->DSSize < aheader->AttrRealSize)
                    stream->DSSize = aheader->AttrRealSize; // it looks like first record contains complete size
                if (stream->DSValidSize < aheader->AttrInitSize)
                    stream->DSValidSize = aheader->AttrInitSize; // it looks like first record contains complete size
#if _DEBUG
                if (stream->DSValidSize > stream->DSSize)
                    TRACE_EW(L"DSValidSize > DSSize on " << CWStr(record->FileNames->FNName).c_str());
                if (stream->DSValidSize != stream->DSSize)
                {
                    if (record->FileNames != NULL)
                        TRACE_IW(L"DSValidSize != DSSize on " << CWStr(record->FileNames->FNName).c_str());
                    else
                        TRACE_I("DSValidSize != DSSize");
                }
#endif
            }
            else
            {
                if (stream->ResidentData != NULL)
                    delete[] stream->ResidentData;
                stream->ResidentData = new BYTE[aheader->AttrLength];
                if (stream->ResidentData == NULL)
                    return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                memcpy(stream->ResidentData, data + offset + aheader->AttrOffset, aheader->AttrLength);
                stream->DSSize = aheader->AttrLength;
                stream->DSValidSize = stream->DSSize; // for resident data is valid data same as DSSize
            }
            break;
        }

        case $VOLUME_INFORMATION:
        {
            ATTRIBUTE_VOLUME_INFORMATION* attr = (ATTRIBUTE_VOLUME_INFORMATION*)(data + offset + aheader->AttrOffset);
            this->Volume->NTFS_VerMajor = attr->VerMajor;
            this->Volume->NTFS_VerMinor = attr->VerMinor;
            TRACE_I("NTFS Version: " << (int)attr->VerMajor << "." << (int)attr->VerMinor);
            break;
        }
        }

        // next attribute
        offset += aheader->Length;
        if (offset >= BytesPerMFTRecord)
        {
            TRACE_E("Out of MFT record?!? (index " << index << ")");
            break;
        }
    }

    // list information about deleted files into trace
#ifdef ENABLE_TRACE_X
    if (rheader->BaseRecord == 0 && !record->IsDir && record->FileNames)
    {
        if (record->FileNames->FNNext == NULL)
        {
            TRACE_X("Found file: " << record->FileNames->FNNext << "   (" << index << ")");
        }
        else
        {
            TRACE_X("Found file with hardlinks:   (" << index << ")");
            FILE_NAME_I<CHAR>* fname = record->FileNames;
            while (fname != NULL)
            {
                if (fname->FNNext != NULL)
                    TRACE_X("  " << fname->FNNext);
                else
                    TRACE_I("  NULL?!?");
                fname = fname->FNNext;
            }
        }

        if (record->Streams != NULL && record->Streams->DSNext != NULL)
        {
            TRACE_X("  streams:");
            DATA_STREAM_I<CHAR>* stream = record->Streams;
            while (stream != NULL)
            {
                if (stream->DSName != NULL)
                {
                    if (stream->DSName[0])
                        TRACE_XW(L"    " << CWStr(stream->DSName).c_str());
                    else
                        TRACE_X("    (unnamed)");
                }
                stream = stream->DSNext;
            }
        }
    }
#endif

    return TRUE;
}

#define BUFSIZE 16 // how much MFT clusters we read in one time
#define DIVROUNDUP(a, b) (((a) + (b)-1) / (b))

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::Update(CSnapshotProgressDlg* progress, DWORD udFlags, CLUSTER_MAP_I** clusterMap)
{
    CALL_STACK_MESSAGE2("CMFTSnapshot::Update(, , 0x%X, )", udFlags);

    TRACE_I("CMFTSnapshot::Update(), udFlags=" << udFlags);

    if (udFlags & UF_GETLOSTCLUSTERMAP)
        udFlags |= UF_ESTIMATEDAMAGE;

    this->Progress = progress;
    this->UdFlags = udFlags;
    if (clusterMap != NULL)
        *clusterMap = NULL;

    if (!this->Volume->IsOpen())
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTOPEN);
    if (this->Volume->Type != vtNTFS)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORNOTNTFS);

    // init
    Free();
    ErrorsFound = FALSE; // todo: report on end when true

    // get SectorsPerMFTRecord, BytesPerMFTRecord
    if (this->Volume->NTFSBoot.ClustersPerMFTRecord < 0)
    {
        BytesPerMFTRecord = (DWORD)1 << (-this->Volume->NTFSBoot.ClustersPerMFTRecord);
        SectorsPerMFTRecord = BytesPerMFTRecord / this->Volume->NTFSBoot.BytesPerSector;
    }
    else
    {
        SectorsPerMFTRecord = (DWORD)this->Volume->NTFSBoot.SectorsPerCluster * this->Volume->NTFSBoot.ClustersPerMFTRecord;
        BytesPerMFTRecord = SectorsPerMFTRecord * this->Volume->NTFSBoot.BytesPerSector;
    }
    TRACE_I("SectorsPerMFTRecord=" << SectorsPerMFTRecord << " BytesPerMFTRecord=" << BytesPerMFTRecord);

    // read first MFT record
    progress->SetProgressText(IDS_READINGMFT);
    DWORD clustersPerMFTRecord = DIVROUNDUP(BytesPerMFTRecord, this->Volume->BytesPerCluster);
    BYTE* firstrec = new BYTE[clustersPerMFTRecord * this->Volume->BytesPerCluster];
    if (firstrec == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    if (!this->Volume->ReadClusters(firstrec, this->Volume->NTFSBoot.MFTStartLCN, clustersPerMFTRecord))
        return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGMFT);
    //TRACE_I("First MFT record:");
    //DumpHexData(firstrec, BytesPerMFTRecord);

    // get info from first record
    FILE_RECORD_I<CHAR>* mft = new FILE_RECORD_I<CHAR>;
    if (mft == NULL)
    {
        delete[] firstrec;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    MFT = &mft;
    MFTItems = 1;
    if (!ParseRecord(firstrec, 0))
    {
        TRACE_I("CMFTSnapshot::Update: ParseRecord failed on first record");
        delete[] firstrec;
        this->FreeRecord(mft);
        MFT = NULL;
        return FALSE;
    }
    delete[] firstrec;

    // check
    if (mft->Streams == NULL || mft->Streams->DSNext != NULL ||     // just one stream
        mft->IsDir ||                                               // can not be directory
        mft->FileNames == NULL || mft->FileNames->FNNext != NULL || // just one hardlink
        String<CHAR>::StrICmp(mft->FileNames->FNName, STRING_MFT))  // name: $MFT
    {
        TRACE_I("MFT looks corrupted");
        this->FreeRecord(mft);
        MFT = NULL;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_MFTRECORDDAMAGED);
    }

    // allocate MFT array and buffer
    MFTItems = mft->Streams->DSSize / BytesPerMFTRecord;
    TRACE_I("MFTItems=" << MFTItems);
    MFT = new FILE_RECORD_I<CHAR>*[(size_t)MFTItems];
    BYTE* buffer = new BYTE[BUFSIZE * this->Volume->BytesPerCluster];
    if (MFT == NULL || buffer == NULL)
    {
        delete[] MFT;
        delete[] buffer;
        this->FreeRecord(mft);
        MFT = NULL;
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    }
    memset(MFT, 0, (size_t)(MFTItems * sizeof(FILE_RECORD_I<CHAR>*)));
    MFT[0] = mft;

    ClustersProcessed = 0;
    ClustersTotal = DIVROUNDUP(mft->Streams->DSSize, this->Volume->BytesPerCluster); // size of MFT in clusters
    if (this->UdFlags & UF_ESTIMATEDAMAGE)
    {
        QWORD volumeClusters = (this->Volume->NTFSBoot.NumberOfSectors / this->Volume->NTFSBoot.SectorsPerCluster);
        QWORD bitmapClusters = (volumeClusters / 8) / this->Volume->BytesPerCluster;
        ClustersTotal += bitmapClusters; // size of bitmap in clusters
    }

    // scan whole MFT
    CStreamReader<CHAR> reader;
    reader.Init(this->Volume, mft->Streams);
    QWORD clustleft = DIVROUNDUP(mft->Streams->DSSize, this->Volume->BytesPerCluster);
    QWORD i = 0;
    QWORD lastprogress = 0;
    BOOL ret = TRUE;
    BOOL canceled = FALSE;

    while (clustleft && i < MFTItems)
    {
        QWORD n = min(clustleft, BUFSIZE);
        if (!reader.IsSafeChunk(n))
            n = clustersPerMFTRecord; // if we are nearing end of fragment, we need to read
        // with minimized blocks because there could come extension record for $MFT, which we would not process
        // and stream reader would go out of runs

        if (!reader.GetClusters(buffer, n))
        {
            String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGMFT);
            TRACE_I("CMFTSnapshot::Update: GetClusters failed, n=" << n << " i=" << i << " clustleft=" << clustleft);
            ret = FALSE;
            break;
        }

        for (QWORD j = 0; j < (n * this->Volume->BytesPerCluster) && i < MFTItems; i++)
        {
            if (i && !ParseRecord(buffer + j, i))
            {
                TRACE_I("CMFTSnapshot::Update: ParseRecord failed");
                ret = FALSE;
                break;
            }
            j += BytesPerMFTRecord;
        }

        if ((i - lastprogress >= 512) || i == MFTItems)
        {
            progress->SetProgress(MulDiv((int)ClustersProcessed, 1000, (int)ClustersTotal));
            if (progress->GetWantCancel())
            {
                ret = FALSE;
                canceled = TRUE;
            }
            lastprogress = i;
        }

        if (!ret)
            break;
        clustleft -= n;
        ClustersProcessed += n;
    }

    delete[] buffer;

    // we ended with error but if we have something, don't throw it away
    if (!ret && !canceled && i > MAX_METAFILES && i < MFTItems)
    {
        if (SalamanderGeneral->SalMessageBox(progress->HWindow, String<char>::LoadStr(IDS_INCOMPLETEMFT),
                                             String<char>::LoadStr(IDS_UNDELETE), MB_YESNO | MB_ICONQUESTION) == IDYES)
        {
            TRACE_I("i = " << i << ", user wants partial results");
            ret = TRUE;
        }
    }

    TRACE_I("Size of snapshot after load: " << GetTotalMemUsage());

    // create directory structure and estimate file damage
    if (ret)
    {
        progress->SetProgressText(IDS_ANALYZINGDIRS);
        ret = BuildDirectoryTree();
        if (ret)
        {
            if (this->UdFlags & UF_ESTIMATEDAMAGE)
                EstimateFileDamage(clusterMap); // we can live without damage estimation
        }
    }

    TRACE_I("Final size of snapshot: " << GetTotalMemUsage());
    TRACE_I("ErrorsFound=" << ErrorsFound);

    if (!ret)
        Free();
    return ret;
}

template <typename CHAR>
void CMFTSnapshot<CHAR>::Free()
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::Free()");

    // release MFT
    if (MFT != NULL)
    {
        // NOTE: Following cycle takes approx 15 seconds for 130 000 items when
        // it is executed from Visual Studio 6 (debug or release version, it doesn't matter).
        // The slowdown is caused by Visual Studio heap checking. (Tested on Athlon 1.4Ghz)
        for (QWORD i = 0; i < MFTItems; i++)
        {
            if (MFT[i] != NULL)
                delete this->FreeRecord(MFT[i]);
        }

        delete[] MFT;
        MFT = NULL;
        MFTItems = 0;
    }

    // release Root if it was allocated
    if (RootAllocated)
    {
        delete this->FreeRecord(this->Root);
        this->Root = NULL;
        RootAllocated = FALSE;
    }

    // release VirtualDirs
    for (int i = 0; i < VirtualDirs.Count; i++)
        delete (VIRTUAL_DIR<CHAR>*)this->FreeRecord(VirtualDirs[i]);
    VirtualDirs.DestroyMembers();
}

template <typename CHAR>
inline BOOL CMFTSnapshot<CHAR>::IsValidRef(QWORD fileref)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CMFTSnapshot::IsValidRef()");
    //WORD seqnum = (WORD) (HIDWORD(fileref) >> 16);
    int index = LODWORD(fileref);
    return MFT[index] != NULL && /*MFT[index]->SeqNum == seqnum &&*/ MFT[index]->IsDir;
}

template <typename CHAR>
FILE_RECORD_I<CHAR>* CMFTSnapshot<CHAR>::GetVirtualDirectory(DWORD ref, int resID, DWORD attr, DWORD flags)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE3("CMFTSnapshot::GetVirtualDirectory(0x%X, %d)", ref, resID);

    // do we have it created already? (simple sequential search, should be enough)
    for (int i = 0; i < VirtualDirs.Count; i++)
    {
        if (VirtualDirs[i]->FileRef == ref)
            return VirtualDirs[i];
    }

    // create a new one
    VIRTUAL_DIR<CHAR>* vd = new VIRTUAL_DIR<CHAR>(ref);
    if (vd == NULL)
        return NULL;
    VirtualDirs.Add(vd);
    if (!VirtualDirs.IsGood())
    {
        VirtualDirs.ResetState();
        return NULL;
    }

    // set information
    vd->IsDir = TRUE;
    vd->Attr = attr;
    vd->Flags = flags;
    SYSTEMTIME st;
    GetSystemTime(&st);
    FILETIME ft;
    SystemTimeToFileTime(&st, &ft);
    vd->TimeCreation = ft;
    vd->TimeLastAccess = ft;
    vd->TimeLastWrite = ft;
    vd->FileNames = new FILE_NAME_I<CHAR>;
    if (vd->FileNames == NULL)
        return NULL;
    CHAR text[100];
    String<CHAR>::SPrintF(text, String<CHAR>::LoadStr(resID), ref);
    vd->FileNames->FNName = String<CHAR>::NewStr(text);
    if (vd->FileNames->FNName == NULL)
        return NULL;

    return vd;
}

template <typename CHAR>
void CMFTSnapshot<CHAR>::Mark(FILE_RECORD_I<CHAR>* r, DWORD depth)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CMFTSnapshot::Mark(, %d)", depth);

    // infinite recursion check (unlikely, FS would have to be corrupted)
    if (depth > 200)
    {
        TRACE_E("depth > 200 !!! (cyclic hardlinks?)");
        return;
    }

    // mark this record
    r->DirItems = (DIR_ITEM_I<CHAR>*)~NULL;

    // ...and parents of all hardlinks
    FILE_NAME_I<CHAR>* fname = r->FileNames;
    while (fname != NULL)
    {
        if (IsValidRef(fname->ParentRecord))
        {
            FILE_RECORD_I<CHAR>* parent = MFT[LODWORD(fname->ParentRecord)];
            if (parent != r) // parent to himself is only root, in such case we will end
                Mark(parent, depth + 1);
            // todo: to speed up we can test if it is already marked from some level and end?
        }
        fname = fname->FNNext;
    }
}

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::IsMetafile(DWORD index)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CMFTSnapshot::IsMetafile(%d)", index);
    if (index >= MAX_METAFILES || MFT[index] == NULL)
        return FALSE;
    FILE_NAME_I<CHAR>* fname = MFT[index]->FileNames;
    if (fname == NULL || fname->FNName == NULL || fname->FNName[0] != '$')
        return FALSE;
    return TRUE;
}

template <typename CHAR>
inline void CMFTSnapshot<CHAR>::AddDirItem(FILE_RECORD_I<CHAR>* record, FILE_RECORD_I<CHAR>* item, FILE_NAME_I<CHAR>* fname)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CMFTSnapshot::AddDirItem(, , )");
    record->DirItems[record->NumDirItems].Record = item;
    record->DirItems[record->NumDirItems++].FileName = fname;
}

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::AllocDirItems(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CMFTSnapshot::AllocDirItems()");
    record->DirItems = new DIR_ITEM_I<CHAR>[record->NumDirItems];
    if (record->DirItems == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    record->NumDirItems = 0;
    return TRUE;
}

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::BuildDirectoryTree()
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::BuildDirectoryTree()");

#define NOTIFY \
    if ((i & 0x3ff) == 0 && this->Progress->GetWantCancel()) \
    return FALSE

    int i;
    int numfiles;

    // remove all records without names (often some from first 24 items)
    for (i = 0; i < MFTItems; i++)
    {
        if (MFT[i] != NULL && MFT[i]->FileNames == NULL)
        {
            delete this->FreeRecord(MFT[i]);
            MFT[i] = NULL;
        }
        NOTIFY; // let know about us
    }

    if (!(this->UdFlags & UF_SHOWZEROFILES))
    {
        // remove all files with zero size
        for (i = 0; i < MFTItems; i++)
        {
            FILE_RECORD_I<CHAR>* r = MFT[i];
            // to remove file it must not have "other" streams and must have zero size
            if (r != NULL && !r->IsDir && !IsMetafile(i) &&
                r->Streams != NULL && r->Streams->DSNext == NULL && r->Streams->DSSize == 0)
            {
                // TRACE_I("Removing zero file "<<r->FileNames->Name);
                delete this->FreeRecord(MFT[i]);
                MFT[i] = NULL;
            }
            NOTIFY;
        }
    }

    // find root (it should be on item 5 but we will walk all items to be sure)
    for (i = 0; i < MAX_METAFILES; i++)
    {
        if (MFT[i] != NULL && MFT[i]->IsDir && MFT[i]->FileNames && String<CHAR>::StrICmp(MFT[i]->FileNames->FNName, STRING_DOT) == 0)
        {
            this->Root = MFT[i];
            break;
        }
    }

    // if we didn't find root, create virtual one so we can store virtual folders
    if (i >= MAX_METAFILES)
    {
        String<CHAR>::Error(IDS_UNDELETE, IDS_ROOTNOTFOUND);
        this->Root = new FILE_RECORD_I<CHAR>;
        if (this->Root == NULL)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        RootAllocated = TRUE;
    }

    if (!(this->UdFlags & UF_SHOWEMPTYDIRS))
    {
        // mark directories and (all their up-directories) which contains deleted files
        for (i = 0; i < MFTItems; i++)
        {
            if (MFT[i] != NULL && !MFT[i]->IsDir)
                Mark(MFT[i], 0);
            NOTIFY;
        }

        // remove unmarked directories
        for (i = 0; i < MFTItems; i++)
        {
            FILE_RECORD_I<CHAR>* r = MFT[i];
            if (r != NULL)
            {
                DIR_ITEM_I<CHAR>* mark = r->DirItems;
                r->DirItems = NULL;
                if (r->IsDir && r != this->Root && mark == NULL)
                {
                    delete this->FreeRecord(r);
                    MFT[i] = NULL;
                }
            }
            NOTIFY;
        }
    }

    // calculate how much space we need for DirItems for each directory
    // (it is fast and we will not fragment heap with linear lists)
    for (i = 0; i < MFTItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = MFT[i];
        if (r != NULL && r != this->Root && !IsMetafile(i))
        {
            FILE_NAME_I<CHAR>* fname = r->FileNames;
            while (fname != NULL)
            {
                FILE_RECORD_I<CHAR>* parent;
                if (IsValidRef(fname->ParentRecord))
                {
                    parent = MFT[LODWORD(fname->ParentRecord)];
                }
                else
                {
                    parent = GetVirtualDirectory(LODWORD(fname->ParentRecord), IDS_VDIRECTORY,
                                                 FILE_ATTRIBUTE_NORMAL, FR_FLAGS_DELETED);
                    if (parent == NULL)
                        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }

                parent->NumDirItems++;
                fname = fname->FNNext;
            }
        }
        NOTIFY;
    }
    this->Root->NumDirItems += VirtualDirs.Count + 2; // 2 for {All Deleted Files} and {Metafiles}

    // allocate arrays in DirItems - in MFT
    for (i = 0; i < MFTItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = MFT[i];
        if (r != NULL && r->NumDirItems)
        {
            if (!AllocDirItems(r))
                return FALSE;
        }
    }
    // ...and in VirtualDirs
    for (i = 0; i < VirtualDirs.Count; i++)
    {
        if (!AllocDirItems(VirtualDirs[i]))
            return FALSE;
    }
    // ...and in root if it was allocated separately
    if (RootAllocated)
    {
        if (!AllocDirItems(this->Root))
            return FALSE;
    }

    // finally: create directories structures
    numfiles = 0;
    for (i = 0; i < MFTItems; i++)
    {
        FILE_RECORD_I<CHAR>* r = MFT[i];
        if (r != NULL && r != this->Root && !IsMetafile(i))
        {
            FILE_NAME_I<CHAR>* fname = r->FileNames;
            while (fname != NULL)
            {
                FILE_RECORD_I<CHAR>* parent;
                if (IsValidRef(fname->ParentRecord))
                {
                    parent = MFT[LODWORD(fname->ParentRecord)];
                }
                else
                {
                    parent = GetVirtualDirectory(LODWORD(fname->ParentRecord), IDS_VDIRECTORY,
                                                 FILE_ATTRIBUTE_NORMAL, FR_FLAGS_DELETED);
                }

                AddDirItem(parent, r, fname);
                fname = fname->FNNext;
            }
            if (!r->IsDir)
                numfiles++;
        }
        NOTIFY;
    }

    // add VirtualDirs to root
    for (i = 0; i < VirtualDirs.Count; i++)
        AddDirItem(this->Root, VirtualDirs[i], VirtualDirs[i]->FileNames);

    // add virtual directory "Metafiles"
    FILE_RECORD_I<CHAR>* dir;
    if (this->UdFlags & UF_SHOWMETAFILES)
    {
        dir = GetVirtualDirectory(0xFFFFFFFF, IDS_METAFILES, FILE_ATTRIBUTE_NORMAL, FR_FLAGS_VIRTUALDIR);
        if (dir == NULL)
            return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
        for (i = 0; i < MAX_METAFILES; i++)
        {
            if (IsMetafile(i))
                dir->NumDirItems++;
        }
        if (!AllocDirItems(dir))
            return FALSE;
        for (i = 0; i < MAX_METAFILES; i++)
        {
            if (IsMetafile(i))
            {
                AddDirItem(dir, MFT[i], MFT[i]->FileNames);
                MFT[i]->Flags |= FR_FLAGS_METAFILE;
            }
        }
        AddDirItem(this->Root, dir, dir->FileNames);
    }

    // add virtual directory "All Deleted Files"
    dir = GetVirtualDirectory(0xFFFFFFFE, IDS_ALLDELETEDFILES, FILE_ATTRIBUTE_NORMAL, FR_FLAGS_VIRTUALDIR);
    if (dir == NULL)
        return String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
    dir->NumDirItems = numfiles;
    if (!AllocDirItems(dir))
        return FALSE;
    for (i = 0; i < MFTItems; i++)
    {
        if (MFT[i] != NULL && !MFT[i]->IsDir && MFT[i] != this->Root && !IsMetafile(i) && (MFT[i]->Flags & FR_FLAGS_DELETED))
            AddDirItem(dir, MFT[i], MFT[i]->FileNames);
        NOTIFY;
    }
    AddDirItem(this->Root, dir, dir->FileNames);

    return TRUE;
}

#ifdef TRACE_ENABLE
template <typename CHAR>
QWORD CMFTSnapshot<CHAR>::GetRecordMemUsage(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CMFTSnapshot::GetRecordMemUsage()");
    QWORD mem = 0;

    // similar to FreeRecord function
    FILE_NAME_I<CHAR>* fname = record->FileNames;
    while (fname != NULL)
    {
        if (fname->FNName != NULL)
            mem += ((int)String<CHAR>::StrLen(fname->FNName) + 1) * sizeof(CHAR);
        mem += sizeof(FILE_NAME_I<CHAR>);
        fname = fname->FNNext;
    }

    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->DSName != NULL)
            mem += ((int)String<CHAR>::StrLen(stream->DSName) + 1) * sizeof(CHAR);
        if (stream->ResidentData != NULL)
            mem += LODWORD(stream->DSSize);
        DATA_POINTERS* ptrs = stream->Ptrs;
        while (ptrs != NULL)
        {
            mem += ptrs->RunsSize + sizeof(DATA_POINTERS);
            ptrs = ptrs->DPNext;
        }
        mem += sizeof(DATA_STREAM_I<CHAR>);
        stream = stream->DSNext;
    }

    mem += record->NumDirItems * sizeof(DIR_ITEM_I<CHAR>);
    return mem;
}

template <typename CHAR>
QWORD CMFTSnapshot<CHAR>::GetTotalMemUsage()
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::GetTotalMemUsage()");
    QWORD mem = 0;

    // similar to Free function
    if (MFT != NULL)
    {
        for (QWORD i = 0; i < MFTItems; i++)
        {
            if (MFT[i] != NULL)
                mem += GetRecordMemUsage(MFT[i]);
        }
        mem += MFTItems * sizeof(FILE_RECORD_I<CHAR>*);
    }

    if (RootAllocated)
        mem += GetRecordMemUsage(this->Root);

    for (QWORD i = 0; i < VirtualDirs.Count; i++)
        mem += GetRecordMemUsage(VirtualDirs[(int)i]);
    mem += VirtualDirs.Count * sizeof(VIRTUAL_DIR<CHAR>);

    return mem;
}
#endif //TRACE_ENABLE

template <typename CHAR>
BOOL CMFTSnapshot<CHAR>::LoadClusterBitmap(CClusterBitmap* clusterBitmap)
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::LoadClusterBitmap()");

    int i;

    // find file $Bitmap (it should be on item 6 but we will walk through all records to be sure)
    FILE_RECORD_I<CHAR>* record;
    for (i = 0; i < MAX_METAFILES; i++)
    {
        if (MFT[i] != NULL && !MFT[i]->IsDir && MFT[i]->FileNames && String<CHAR>::StrICmp(MFT[i]->FileNames->FNName, STRING_BITMAP) == 0)
        {
            record = MFT[i];
            break;
        }
    }
    if (i >= MAX_METAFILES)
    {
        String<CHAR>::Error(IDS_UNDELETE, IDS_BITMAPNOTFOUND);
        return NULL;
    }

    // find default stream
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL && stream->DSName != NULL)
        stream = stream->DSNext;
    if (stream == NULL)
    {
        String<CHAR>::Error(IDS_UNDELETE, IDS_BITMAPNOSTREAM);
        return NULL;
    }

    if (stream->DSSize == 0)
    {
        String<CHAR>::Error(IDS_UNDELETE, IDS_BITMAPISEMPTY);
        return NULL;
    }

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

    while (bytesleft > 0)
    {
        this->Progress->SetProgress(MulDiv((int)ClustersProcessed, 1000, (int)ClustersTotal));
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
        ClustersProcessed += nc;
    }
    delete[] buffer;
    return TRUE;
}

// Bitmap contains 2 bits for each cluster with following values:
// 0b00 (0) - this cluster doesn't contain data (or we don't know about it)
// 0b01 (1) - there is one delete file
// 0b10 (2) - there is more than one delete file
// 0b11 (3) - there is existing directory

template <typename CHAR>
void CMFTSnapshot<CHAR>::DrawDeletedFile(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap)
{
    // walk through all nonresident streams
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->ResidentData == NULL && stream->DSSize > 0)
        {
            // "render" all cluster runs to bitmap
            CRunsWalker<CHAR> runsWalker(stream->Ptrs);
            QWORD clustersleft = (stream->DSSize - 1) / this->Volume->BytesPerCluster + 1;
            while (clustersleft > 0)
            {
                QWORD lcn;
                QWORD length;
                if (!runsWalker.GetNextRun(&lcn, &length, NULL))
                {
                    TRACE_IW(L"CMFTSnapshot::DrawDeletedFile() Ignoring: " << CWStr(record->FileNames->FNName).c_str());
                    break; // unlikely error, moreover it doesn't matter
                }
                if (lcn != -1)
                {
                    for (QWORD i = lcn; i < lcn + length; i++)
                        clusterBitmap->IncreaseValue(i); // if value in bitmap is smaller than 2, increase it
                }
                clustersleft -= min(clustersleft, length);
            }
        }
        stream = stream->DSNext;
    }
}

template <typename CHAR>
DWORD CMFTSnapshot<CHAR>::GetFileCondition(FILE_RECORD_I<CHAR>* record, CClusterBitmap* clusterBitmap)
{
    QWORD cnt[4] = {0, 0, 0, 0};
    BOOL resident = FALSE;

    // basically the same such in DrawDeletedFile function but this time we are just collecting number of clusters in each class
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        if (stream->ResidentData == NULL && stream->DSSize > 0)
        {
            // walk through all cluster runs in bitmap
            CRunsWalker<CHAR> runsWalker(stream->Ptrs);
            QWORD lcn = 0;
            QWORD clustersleft = (stream->DSSize - 1) / this->Volume->BytesPerCluster + 1;
            while (clustersleft > 0)
            {
                QWORD lcn;
                QWORD length;
                if (!runsWalker.GetNextRun(&lcn, &length, NULL))
                {
                    TRACE_IW(L"CMFTSnapshot::GetFileCondition() Ignoring: " << CWStr(record->FileNames->FNName).c_str());
                    break;
                }
                if (lcn != -1)
                {
                    for (QWORD i = lcn; i < lcn + length; i++)
                    {
                        // for number cluster 'i' find related two bits in bitmap
                        BYTE c;
                        if (clusterBitmap->GetValue(i, &c))
                            cnt[c]++;
                        else
                            TRACE_E("CMFTSnapshot::GetFileCondition(): stream is out of BITMAP range");
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
BOOL CMFTSnapshot<CHAR>::EstimateFileDamage(CLUSTER_MAP_I** clusterMap)
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::EstimateFileDamage()");

    int i;

    // get disk bitmap
    CClusterBitmap clusterBitmap;
    if (!LoadClusterBitmap(&clusterBitmap))
        return FALSE;

    // "render" deleted files to bitmap
    for (i = 0; i < MFTItems; i++)
    {
        if (MFT[i] != NULL && (MFT[i]->Flags & FR_FLAGS_DELETED) && !MFT[i]->IsDir)
            DrawDeletedFile(MFT[i], &clusterBitmap);
    }

    // finally get state of deleted files
    for (i = 0; i < MFTItems; i++)
    {
        if (MFT[i] != NULL && (MFT[i]->Flags & FR_FLAGS_DELETED) && !MFT[i]->IsDir)
        {
            MFT[i]->Flags &= ~FR_FLAGS_CONDITION_MASK;
            MFT[i]->Flags |= GetFileCondition(MFT[i], &clusterBitmap);
        }
    }

    if (this->UdFlags & UF_GETLOSTCLUSTERMAP) // does user want to return map of lost clusters?
    {
        if (clusterMap != NULL)
        {
            // files with Condition FC_FAIR or FC_POOR render as "2 - there are more then one delete file in this place"
            // because we should return map of clusters which are not used by existing files and files which could be recovered (FC_GOOD)
            for (i = 0; i < MFTItems; i++)
            {
                if (MFT[i] != NULL && (MFT[i]->Flags & FR_FLAGS_DELETED) && !MFT[i]->IsDir)
                {
                    DWORD condition = MFT[i]->Flags & FR_FLAGS_CONDITION_MASK;
                    if (condition == FR_FLAGS_CONDITION_FAIR || condition == FR_FLAGS_CONDITION_POOR)
                        DrawDeletedFile(MFT[i], &clusterBitmap); // second render will increase 1 -> 2, (2 will stay 2)
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
BOOL CMFTSnapshot<CHAR>::GetLostClustersMap(CClusterBitmap* clusterBitmap, CLUSTER_MAP_I* clusterMap)
{
    CALL_STACK_MESSAGE1("CMFTSnapshot::GetLostClustersMap()");
    clusterMap->Free();
    QWORD clusterCount = clusterBitmap->GetClusterCount();

    // pass 0: get size of arrays we shoud allocate, allocet them
    // pass 1: fill these arrays
    for (int pass = 0; pass < 2; pass++)
    {
        int clusterPairs = 0;
        BOOL inside = FALSE;
        BOOL flush = FALSE;
        QWORD lcnFirst = 0;
        QWORD i = 0;
        while (i < clusterCount)
        {
            // for cluster number 'i' find related two bits in bitmap
            BYTE c;
            clusterBitmap->GetValue(i, &c);
            if (c == 0 || c == 2) // cluster is not used (or we don't know about it) || there is FC_FAIR or FC_POOR
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
