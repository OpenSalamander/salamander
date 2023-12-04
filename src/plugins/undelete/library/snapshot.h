// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push, snapshot_h)
#pragma pack(4)

// FILE_NAME - structure for storing list of file hard-links, each hard-link has
// name of parent directory
template <typename CHAR>
struct FILE_NAME_I
{
    FILE_NAME_I() { memset(this, 0, sizeof(*this)); }

    CHAR* FNName;
    CHAR* DOSName;
    QWORD ParentRecord;
    FILE_NAME_I* FNNext;
};

// DATA_STREAM - contains info about one file data stream. Each stream
// could be resident data (ResidentData != NULL) or data elsewhere on disk,
// then Ptrs contains list of data runs blocks (DATA_POINTERS). Normal file has
// only one stream and one block of data runs. Heavily fragmented file whose list
// of data runs cannot fit inside one MFT stream has more DATA_POINTERS.

struct DATA_POINTERS
{
    DATA_POINTERS() { memset(this, 0, sizeof(*this)); }

    QWORD StartVCN;
    QWORD LastVCN;
    DWORD DPFlags;
    BYTE* Runs;
    DWORD RunsSize;
    DWORD CompUnit;
    DATA_POINTERS* DPNext;
};

template <typename CHAR>
struct DATA_STREAM_I
{
    DATA_STREAM_I() { memset(this, 0, sizeof(*this)); }

    CHAR* DSName; // not NULL for NTFS ADS, NULL otherwise
    union
    {
        BYTE* ResidentData; // for NTFS only
        DWORD FirstLCN;     // for FAT and exFAT until data runs are encoded, then NULL
    };
    DATA_POINTERS* Ptrs;
    QWORD DSSize;      // stream size in bytes
    QWORD DSValidSize; // valida data size in bytes; on NTFS and exFAT could be smaller than DSSize; on FAT it is same as DSSize
    DATA_STREAM_I<CHAR>* DSNext;
};

// DIR_ITEM - item of files list which are containing in directory. Points
// to file name (tj. to concrete hard-link) and file record.

template <typename CHAR>
struct FILE_RECORD_I;

template <typename CHAR>
struct DIR_ITEM_I
{
    DIR_ITEM_I()
    {
        FileName = NULL;
        Record = NULL;
    }

    FILE_NAME_I<CHAR>* FileName;
    FILE_RECORD_I<CHAR>* Record;
};

// FILE_RECORD - corresponds to one MFT record, we are storing only deleted files and
// directories. FileNames is list of names (hard-links), we prefer normal names instead of
// DOS names (if we find normal name and already have DOS name, we will overwrite it).
// Times are creation, last access, modification times of file. Streams is list of all
// file streams. Each stream could have data runs in several blocks (see above).
// DirItems is NULL for files, for directories it contains list of containing items.

template <typename CHAR>
struct FILE_RECORD_I
{
    FILE_RECORD_I() { memset(this, 0, sizeof(*this)); }

    BOOL IsDir;                   // TRUE for directory, FALSE for file
    DWORD Attr;                   // attributes from FILE_ATTRIBUTE_* family
    DWORD Flags;                  // flags from FR_FLAGS_ family and file condition set during damage estimation phase
    FILE_NAME_I<CHAR>* FileNames; // list of names (only one item on FAT and exFAT)
    FILETIME TimeCreation;        // creation time
    FILETIME TimeLastAccess;      // last access time
    FILETIME TimeLastWrite;       // modification time
    DATA_STREAM_I<CHAR>* Streams; // item streams (only one stream on FAT and exFAT)
    DIR_ITEM_I<CHAR>* DirItems;   // directory items (when IsDir==TRUE)
    DWORD NumDirItems;            // number of items in directory (when IsDir==TRUE)
};

// VIRTUAL_DIR - structure for holding directories on NTFS whose record doesn't exist (but
// are referenced by files that were contained in such directory)

template <typename CHAR>
struct VIRTUAL_DIR : public FILE_RECORD_I<CHAR>
{
    VIRTUAL_DIR(DWORD ref) : FILE_RECORD_I<CHAR>() { FileRef = ref; }

    DWORD FileRef;
};

struct CLUSTER_MAP_I
{
    DWORD Items;
    QWORD* FirstCluster;
    QWORD* ClustersCount;

    CLUSTER_MAP_I() { memset(this, 0, sizeof(*this)); }
    void Free()
    {
        if (FirstCluster != NULL)
        {
            delete[] FirstCluster;
            FirstCluster = NULL;
        }
        if (ClustersCount != NULL)
        {
            delete[] ClustersCount;
            ClustersCount = NULL;
        }
        memset(this, 0, sizeof(*this));
    }
};

#pragma pack(pop, snapshot_h)

// forward declarations
class CSnapshotProgressDlg;
template <typename CHAR>
class CVolume;

// CSnapshot - abstract class, base for CNTFSSnapshot and CFATSnapshot

template <typename CHAR>
class CSnapshot
{
public:
    CSnapshot(CVolume<CHAR>* volume)
    {
        Volume = volume;
        Root = NULL;
    }

    virtual ~CSnapshot(){};
    virtual BOOL Update(CSnapshotProgressDlg* progress, DWORD flags, CLUSTER_MAP_I** clusterMap) = 0;
    virtual void Free() = 0;

    FILE_RECORD_I<CHAR>* Root;

protected:
    FILE_RECORD_I<CHAR>* FreeRecord(FILE_RECORD_I<CHAR>* record);

    CVolume<CHAR>* Volume;
    CSnapshotProgressDlg* Progress;
    DWORD UdFlags; // undelete flags UF_*
};

template <typename CHAR>
FILE_RECORD_I<CHAR>* CSnapshot<CHAR>::FreeRecord(FILE_RECORD_I<CHAR>* record)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE1("CSnapshot::FreeRecord()");

    // free all hardlinks
    FILE_NAME_I<CHAR>* fname = record->FileNames;
    while (fname != NULL)
    {
        FILE_NAME_I<CHAR>* f = fname;
        fname = fname->FNNext;
        if (f->FNName != NULL)
            delete[] f->FNName;
        if (f->DOSName != NULL)
            delete[] f->DOSName;
        delete f;
    }

    // free all streams
    DATA_STREAM_I<CHAR>* stream = record->Streams;
    while (stream != NULL)
    {
        DATA_STREAM_I<CHAR>* s = stream;
        stream = stream->DSNext;
        if (s->DSName != NULL)
            delete[] s->DSName;
        if (s->ResidentData != NULL)
            delete[] s->ResidentData;
        // free all data runs
        DATA_POINTERS* ptrs = s->Ptrs;
        while (ptrs != NULL)
        {
            DATA_POINTERS* p = ptrs;
            ptrs = ptrs->DPNext;
            delete[] p->Runs;
            delete p;
        }
        delete[] s;
    }

    if (record->IsDir)
    {
        // do not delete marked items, see CMFTSnapshot<CHAR>::Mark()
        if ((record->DirItems != NULL) && (record->DirItems != (DIR_ITEM_I<CHAR>*)~NULL))
            delete[] record->DirItems;
    }
    return record;
}
