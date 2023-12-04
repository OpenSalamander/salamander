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

// tri stavy pro chyby, jsou situace, kdy nestaci TRUE/FALSE.
// mame cinnost, pri ktere muze nastat chyba. nekdy muzeme a chceme v cinnosti pokracovat, jindy to nejde.
// pokud muzeme pokracovat, vracime ERR_CONTINUE, pokud to dal nejde (dosla pamet, aj.) ERR_TERMINATE.
// pokud vse ok, ERR_OK
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

    // struktury
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

    // parametery image
    DWORD SectorHeaderSize;
    DWORD SectorRawSize;
    DWORD SectorUserSize;

    DWORD ExtentOffset; // pro multisession
    int LastTrack;      // posledni track, ktery umime precist
    int OpenedTrack;    // otevreny track

    FILETIME LastWrite;

    char* Label;

    //
    TIndirectArray<Track> Tracks; // tracky
    TDirectArray<int> Session;    // pocet tracku v session

public:
    DWORD ReadBlock(DWORD block, DWORD size, void* data);

    // Otevre ISO image s nazev 'fileName'. Parametr 'quiet' urcuje, zda
    // budou vyskakovat MessageBoxy s chybama
    BOOL Open(const char* fileName, BOOL quiet = FALSE);
    BOOL OpenTrack(int track, BOOL quiet = FALSE);
    BOOL DetectTrackFS(int track);

    BOOL ListImage(CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData);
    // vracit jednu z konstant UNPACK_XXX
    int UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* name, const CFileData* fileData,
                   DWORD& silent, BOOL& toSkip);
    // vracit jednu z konstant UNPACK_XXX
    int UnpackDir(const char* dirName, const CFileData* fileData);

    // vracit jednu z konstant UNPACK_XXX
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
