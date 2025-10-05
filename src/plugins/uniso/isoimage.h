// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "file.h"
#include "hfs.h"

class CUnISOFSAbstract;

#define UNPACK_ERROR 0
#define UNPACK_OK 1
#define UNPACK_CANCEL 2
#define UNPACK_AUDIO_UNSUP 3

// three states for errors; there are situations where TRUE/FALSE is not enough.
// we have an activity during which an error can occur. Sometimes we can and want to continue, sometimes we cannot.
// if we can continue, we return ERR_CONTINUE; if it cannot go on (out of memory, etc.) we return ERR_TERMINATE.
// if everything is OK, ERR_OK
#define ERR_OK 0
#define ERR_CONTINUE 1
#define ERR_TERMINATE 2

#define FS_TYPE_NONE 0
#define FS_TYPE_ISO9660 1
#define FS_TYPE_UDF 2
#define FS_TYPE_HFS 3

#define ISO_MAX_PATH_LEN 1024

// ****************************************************************************
//
// CISOImage
//

class CISOImage
{
public:
    CISOImage();
    virtual ~CISOImage();

    enum ETrackFSType
    {
        fsUnknown,
        fsAudio,
        fsISO9660,
        fsUDF_ISO9660,
        fsUDF,
        fsData,
        fsXbox,
        fsHFS,
        fsISO9660_HFS,
        fsUDF_HFS,
        fsUDF_ISO9660_HFS,
        fsAPFS,
        fsISO9660_APFS,
        fsUDF_APFS,
        fsUDF_ISO9660_APFS
    };

    enum ETrackMode
    {
        mNone,
        mMode1,
        mMode2
    };

    // structures
    struct Track
    {
        WORD SectorSize;
        BYTE SectorHeaderSize;
        ETrackMode Mode;
        LONGLONG Start;
        LONGLONG End;
        DWORD ExtentOffset;
        ETrackFSType FSType;
        BOOL Bootable;
        CUnISOFSAbstract* FileSystem;

        Track();
        virtual ~Track();
        virtual const char* GetLabel();
        virtual void SetLabel(const char* label);
    };

    struct CFilePos
    {
        DWORD Extent;
        WORD Partition;
        BYTE Type; // used in UDF/ISO bridge

        CFilePos()
        {
            Extent = 0;
            Type = FS_TYPE_NONE;
        }
    };

    // GUI
    BOOL DisplayMissingCCDWarning;

protected:
    enum ESectorType
    {
        stISO,
        stMode1,
        stMode2Form1,
        stMode2Form2,
        stMode2Form12336,
        stCIF
    };

    CFile* File;
    ESectorType SectorType;

    LONGLONG DataOffset; // Start of current track
    LONGLONG DataEnd;    // End of current track

    // image parameters
    DWORD SectorHeaderSize;
    DWORD SectorRawSize;
    DWORD SectorUserSize;

    DWORD ExtentOffset; // for multisession images
    int LastTrack;      // the last track we can read
    int OpenedTrack;    // the opened track

    FILETIME LastWrite;

    char* Label;

    //
    TIndirectArray<Track> Tracks; // tracks
    TDirectArray<int> Session;    // number of tracks in the session

public:
    DWORD ReadBlock(DWORD block, DWORD size, void* data);

    // Opens the ISO image named 'fileName'. The 'quiet' parameter determines whether
    // message boxes with errors will pop up
    BOOL Open(const char* fileName, BOOL quiet = FALSE);
    BOOL OpenTrack(int track, BOOL quiet = FALSE);
    BOOL DetectTrackFS(int track);

    BOOL ListImage(CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    // returns one of the UNPACK_XXX constants
    int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* name, const CFileData* fileData,
                   DWORD& silent, BOOL& toSkip);
    // returns one of the UNPACK_XXX constants
    int UnpackDir(const char* dirName, const CFileData* fileData);

    // returns one of the UNPACK_XXX constants
    int ExtractAllItems(CSalamanderForOperationsAbstract* salamander, char* srcPath, CSalamanderDirectoryAbstract const* dir,
                        const char* mask, char* path, int pathBufSize, DWORD& silent, BOOL& toSkip);

    BOOL DumpInfo(FILE* outStream);

    void SetLabel(const char* label);
    const char* GetLabel();

    // track things
    void AddTrack(Track* track);
    void AddSessionTracks(int tracks);

    int GetTrackCount() { return Tracks.Count; }
    Track* GetTrack(int track);
    int GetLastTrack() { return LastTrack; }

    BYTE GetTrackFromExtent(DWORD extent);
    static BYTE GetHeaderSizeFromMode(ETrackMode mode, DWORD sectorRawSize);

    FILETIME GetLastWrite() { return LastWrite; }
    LONGLONG GetCurrentTrackEnd(int trackno);

protected:
    DWORD ReadDataByPos(LONGLONG position, DWORD size, void* data);
    BOOL ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);

    // support
    void DetectSectorType();
    BOOL SetSectorFormat(ESectorType format);
    LONGLONG GetSectorOffset(int nSector);

    BOOL CheckForISO(BOOL quiet = FALSE);
    BOOL CheckForNRG(BOOL quiet = FALSE);
    BOOL CheckFor2336(BOOL quiet = FALSE);
    BOOL CheckForNCD(BOOL quiet = FALSE);
    BOOL CheckForPDI(BOOL quiet = FALSE);
    BOOL CheckForECDC(BOOL quiet = FALSE);
    BOOL CheckForCIF(BOOL quiet = FALSE);
    BOOL CheckForC2D(BOOL quiet = FALSE);
    BOOL CheckForXbox(BOOL quiet = FALSE);
    BOOL CheckForMDF(BOOL quiet = FALSE);
    BOOL CheckForHFS(BOOL quiet = FALSE);
    BOOL CheckForAPFS(BOOL quiet = FALSE);

    BOOL CheckForCIF2332(BOOL quiet = FALSE);

    BOOL ReadSessionInfo(BOOL quiet = FALSE);
    BOOL ReadSessionNRG(BOOL quiet = FALSE);
    BOOL ReadSessionCCD(char* fileName, BOOL quiet = FALSE);
    //    BOOL ProcessReadSessionCCD(char *fileName, BOOL quiet = FALSE);

    void SetTrackParams(int trackno);
    //    void SetTrackFromMode(ETrackMode mode);

    char* FileName;

    friend class CUDF;
    friend class CHFS;

public:
    BOOL CheckForBootSectorOrMBR();
};

// helpers
void ISODateTimeToFileTime(BYTE isodt[], FILETIME* ft);
void ISODateTimeStrToSystemTime(BYTE isodt[], SYSTEMTIME* st);
void GetInfo(char* buffer, FILETIME* lastWrite, CQuadWord size);
void SetFileAttrs(const char* name, DWORD attrs, BOOL quiet = FALSE);

// viewer
char* ViewerPrintSystemTime(SYSTEMTIME* st);
char* ViewerStrNcpy(char data[], int count);
