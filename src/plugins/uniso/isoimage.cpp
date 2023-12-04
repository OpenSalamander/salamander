// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#include "audio.h"
#include "iso9660.h"
#include "udf.h"
#include "udfiso.h"
#include "xdvdfs.h"
#include "rawfs.h"
#include "mdf.h"
#include "dmg.h"
#include "isz.h"
#include "apfs.h"

// ****************************************************************************
//
// Helpers
//

// RIFF header
// pro CIF format vytvareny Easy CD Creatorem
struct RIFFHeader
{
    char RIFF[4];
    DWORD Size;
    char ID[4];
};

// Converts binary date/time format to FILETIME
void ISODateTimeToFileTime(BYTE isodt[], FILETIME* ft)
{
    SYSTEMTIME st;
    __int64 newtime;

    CALL_STACK_MESSAGE1("ISODateTimeToSystemTime(, )");

    st.wYear = 1900 + isodt[0];
    st.wMonth = isodt[1];
    st.wDayOfWeek = 0;
    st.wDay = isodt[2];
    st.wHour = isodt[3];
    st.wMinute = isodt[4];
    st.wSecond = isodt[5];
    st.wMilliseconds = 0;

    SystemTimeToFileTime(&st, ft);
    // idodt[6] Offset from Greenwich Mean Time in number of 15 minute intervals from -48(West) to +52(East)
    newtime = ft->dwLowDateTime + (((__int64)ft->dwHighDateTime) << 32);
    newtime -= ((__int64)(signed char)isodt[6]) * 15 * 60 * 1000 * 1000 * 10; // from 15min units to 100ns units
    ft->dwLowDateTime = (DWORD)(newtime & 0xffffffff);
    ft->dwHighDateTime = (DWORD)(newtime >> 32);
}

// prevede retezcovy format datumu na SYSTEMTIME
void ISODateTimeStrToSystemTime(BYTE isodt[], SYSTEMTIME* st)
{
    CALL_STACK_MESSAGE1("ISODateTimeStrToSystemTime(, )");

#define CpyN(Var, Ofs, N) \
    ZeroMemory(strdate, 16); \
    memcpy(strdate, isodt + Ofs, N); \
    strdate[N] = '\0'; \
    sscanf(strdate, "%hu", &(st->##Var));

    char strdate[16];

    ZeroMemory(st, sizeof(*st));
    CpyN(wYear, 0, 4);
    CpyN(wMonth, 4, 2);
    CpyN(wDay, 6, 2);
    CpyN(wHour, 8, 2);
    CpyN(wMinute, 10, 2);
    CpyN(wSecond, 12, 2);
#undef CpyN
}

void GetInfo(char* buffer, FILETIME* lastWrite, CQuadWord size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%I64X)", size.Value);

    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, size), date, time);
}

void SetFileAttrs(const char* name, DWORD attrs, BOOL quiet)
{
    if (Options.ClearReadOnly)
        attrs &= ~FILE_ATTRIBUTE_READONLY;
    if (!SetFileAttributes(name, attrs))
    { // set attributes - always clear Archive attribute, clearing of Read-Only attribute depends on settings
        DWORD err = GetLastError();
        Error(LoadStr(IDS_CANT_SET_ATTRS), err, quiet);
    }
}

//
BOOL IsValidCDHeader(char hdr[])
{
    if (hdr[0] == 0x01 && strncmp((hdr + 1), "CD001", 5) == 0)
        return TRUE;
    else
        return FALSE;
}

BOOL IsValidUDFHeader(char hdr[])
{
    if (hdr[0] == 0x00 && strncmp((hdr + 1), "BEA01", 5) == 0)
        return TRUE;
    else
        return FALSE;
}

// ****************************************************************************
//
// CISOImage
//

CISOImage::Track::Track()
{
    FileSystem = NULL;
    Bootable = FALSE;
}

CISOImage::Track::~Track()
{
    delete FileSystem;
}

const char* CISOImage::Track::GetLabel()
{
    return "";
}

void CISOImage::Track::SetLabel(const char*)
{
}

CISOImage::CISOImage() : Session(10, 5),
                         Tracks(30, 10, dtDelete)
{
    FileName = NULL;
    File = NULL;

    DataOffset = 0;

    OpenedTrack = -1;

    // GUI
    DisplayMissingCCDWarning = TRUE;

    // default
    SectorHeaderSize = 0;
    SectorRawSize = 2048;
    SectorUserSize = 2048;

    Label = NULL;
}

CISOImage::~CISOImage()
{
    delete[] FileName;
    delete[] Label;
    delete File;
}

void CISOImage::AddTrack(Track* track)
{
    Tracks.Add(track);
}

void CISOImage::AddSessionTracks(int tracks)
{
    Session.Add(tracks);
}

void CISOImage::SetLabel(const char* label)
{
    delete[] Label;
    Label = new char[strlen(label) + 1];
    if (Label)
        strcpy(Label, label);
}

const char*
CISOImage::GetLabel()
{
    return Label ? Label : "";
}

DWORD
CISOImage::ReadDataByPos(LONGLONG position, DWORD size, void* data)
{
    if (File->Seek(position, FILE_BEGIN) == -1)
        return 0;

    DWORD read;

    if (File->Read(data, size, &read, FileName, SalamanderGeneral->GetMainWindowHWND()))
        return read;
    else
        return 0;
}

BOOL CISOImage::SetSectorFormat(ESectorType format)
{
    CALL_STACK_MESSAGE1("CISOImage::SetSectorFormat()");

    SectorType = format;

    switch (format)
    {
    case stMode1:
        SectorHeaderSize = 16;
        SectorRawSize = 0x930;
        break;

    case stMode2Form1:
        SectorHeaderSize = 24;
        SectorRawSize = 0x930;
        break;

    case stMode2Form2:
        SectorHeaderSize = 24;
        SectorUserSize = 2324;
        SectorRawSize = 0x930;
        break;

    case stMode2Form12336:
        SectorHeaderSize = 8;
        SectorRawSize = 0x920;
        break;

    case stCIF:
        SectorHeaderSize = 8;
        SectorRawSize = 0x808;
        break;

    case stISO:
    default:
        SectorHeaderSize = 0;
        SectorRawSize = 0x800;
        break;
    }

    return TRUE;
}

void CISOImage::DetectSectorType()
{
    CALL_STACK_MESSAGE1("CISOImage::DetectSectorType()");

    if (File->Seek(0, FILE_BEGIN) == -1)
        return;

    char buffer[2352];
    DWORD read;
    ESectorType type;

    if (!File->Read(buffer, 2352, &read, FileName, SalamanderGeneral->GetMainWindowHWND()))
        return;

    if (File->Seek(0, FILE_BEGIN) == -1)
        return;

    if (!memcmp(buffer, "\0\xff\xff\xff\xff\xff\xff\xff\xff\xff\xff\0", 12))
    {
        if (buffer[15] == 1)
            type = stMode1;
        else if (buffer[15] == 2)
        {
            if (buffer[18] & 0x20)
                type = stMode2Form2;
            else
                type = stMode2Form1;
        }
        else
            type = stISO;
    }
    else
        type = stISO;

    SectorType = type;
}

LONGLONG
CISOImage::GetSectorOffset(int nSector)
{
    return Int32x32To64(nSector, SectorRawSize) + SectorHeaderSize + DataOffset;
}

//
// Sada funkci pro detekci, zda je image validni. Pro kazdy format jedna funkce.
// Je to sice ponekud pitome, protoze pro kazdy novy format musime pridat novou funkci.
// jiny pluginy, hledaji signaturu CD001 a urcuji format podle ni. To bychom taky mohli
// udelat, ale neda se na to uplne spolehnout.
// Timto zpusobem ale dokazeme nastavovat rovnou i format image aj dulezite informace.
// A mame jistotu, ze urcite pracujeme s CD imagem
//

BOOL CISOImage::CheckForISO(BOOL quiet /* = FALSE*/)
{
    char hdr[6];
    LONGLONG offset = GetSectorOffset(16);
    if (ReadDataByPos(offset, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr) || IsValidUDFHeader(hdr))
        return TRUE; // ok, it's ISO

    return FALSE;
}

BOOL CISOImage::CheckForNRG(BOOL quiet /* = FALSE*/)
{
    char hdr[6];
    LONGLONG offset = GetSectorOffset(16);

    // check for NRG
    if (ReadDataByPos(offset + 0x4B000, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr) || IsValidUDFHeader(hdr))
    {
        // ok, it's NRG
        DataOffset = 0x4B000;
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckFor2336(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    if (ReadDataByPos(0x9208, sizeof(hdr), hdr) == sizeof(hdr))
    {
        if (IsValidCDHeader(hdr))
        {
            SetSectorFormat(stMode2Form12336);
            return TRUE;
        }
        else
        {
            if (ReadDataByPos(0x5EAC8, sizeof(hdr), hdr) == sizeof(hdr))
            {
                if (IsValidCDHeader(hdr))
                {
                    DataOffset = 0x558C0;
                    SetSectorFormat(stMode2Form12336);
                    SectorRawSize = 0x920;
                    return TRUE;
                }
            }
        }
    } // if - 2336 file format

    return FALSE;
}

BOOL CISOImage::CheckForNCD(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    // check for NCD
    if (ReadDataByPos(0x1EE4E, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's NCD
        DataOffset = 0x16E4E;
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckForPDI(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    // check for PDI
    if (ReadDataByPos(0x8130, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's PDI
        DataOffset = 0x130;
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckForECDC(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    // check for ISO from Easy CD Creator
    // Easy CD Creator dava do ISO souboru hlavicky oddelujici sektory, ale "spatne" (jsou to same nuly),
    // takze funkce CheckSectorType nic nepozna
    if (ReadDataByPos(0x9318, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's Easy CD Creator
        DataOffset = 0x0;
        SetSectorFormat(stMode2Form1);
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckForC2D(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    // check for ISO from WinOnCD
    if (ReadDataByPos(0x28000, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's WinOnCD
        DataOffset = 0x20000;
        return TRUE;
    }
    else
    {
        if (ReadDataByPos(0x7F538, sizeof(hdr), hdr) != sizeof(hdr))
            return FALSE;
        else
        {
            if (IsValidCDHeader(hdr))
            {
                DataOffset = 0x76220;
                SetSectorFormat(stMode2Form1);
                return TRUE;
            }
        }
    }

    // check for Roxio Image Format 3.0
    if (ReadDataByPos(0x008120, sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's WinOnCD
        DataOffset = 0x000120;
        return TRUE;
    }

    return FALSE;
}

// ****************************************************************************
// Support functions for CIF format

inline BOOL isRIFFHeader(RIFFHeader* hdr)
{
    if (strncmp(hdr->RIFF, "RIFF", 4) == 0)
        return TRUE;
    else
        return FALSE;
}

inline BOOL checkChunk(RIFFHeader* hdr, const char* id)
{
    if (strncmp(hdr->ID, id, 4) == 0)
        return TRUE;
    else
        return FALSE;
}

// Chunks are aligned to even positions
inline void alignOffset(LONGLONG* offset)
{
    if (*offset % 2 == 1)
        (*offset)++;
}

BOOL CISOImage::CheckForCIF(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    // RIFF header
    RIFFHeader riffHead;
    LONGLONG offset = 0x0000;

    // imag chunk
    if (ReadDataByPos(offset, sizeof(riffHead), &riffHead) != sizeof(riffHead))
        return FALSE;
    if (!isRIFFHeader(&riffHead) || !checkChunk(&riffHead, "imag"))
        return FALSE;
    offset += riffHead.Size + 8; // skip this block, sizeof(RIFF signature + chunk size) == 8
    alignOffset(&offset);

    // disc chunk
    if (ReadDataByPos(offset, sizeof(riffHead), &riffHead) != sizeof(riffHead))
        return FALSE;
    if (!isRIFFHeader(&riffHead) || !checkChunk(&riffHead, "disc"))
        return FALSE;
    offset += riffHead.Size + 8; // skip this block, sizeof(RIFF signature + chunk size) == 8
    alignOffset(&offset);

    // info chunk
    if (ReadDataByPos(offset, sizeof(riffHead), &riffHead) != sizeof(riffHead))
        return FALSE;
    if (!isRIFFHeader(&riffHead) || !checkChunk(&riffHead, "info"))
        return FALSE;
    offset += sizeof(riffHead) + 8; // sizeof(SectorHeader) == 8

    // je vysoce pravdepodobne, ze je to CIF
    SetSectorFormat(stCIF);

    if (ReadDataByPos(offset + GetSectorOffset(16), sizeof(hdr), hdr) != sizeof(hdr))
        return FALSE;

    if (IsValidCDHeader(hdr))
    {
        // ok, it's CIF
        DataOffset = offset;
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckForCIF2332(BOOL quiet /* = FALSE*/)
{
    char hdr[6];

    if (ReadDataByPos(0x91C8, sizeof(hdr), hdr) == sizeof(hdr))
    {
        if (IsValidCDHeader(hdr))
        {
            // nejaky obskurni format z CloneCD 5.0
            DataOffset = 0;
            SetSectorFormat(stCIF);
            SectorRawSize = 0x91C; // special format - override settings from SetSectorFormat
            return TRUE;
        }
    } // if - 2332 file format

    return FALSE;
}

BOOL CISOImage::CheckForXbox(BOOL quiet /* = FALSE*/)
{
    char hdr1[0x14];
    char hdr2[0x14];

    if (ReadDataByPos(0x10000, sizeof(hdr1), hdr1) == sizeof(hdr1) &&
        ReadDataByPos(0x107ec, sizeof(hdr2), hdr2) == sizeof(hdr2))
    {
        if (strncmp(hdr1, XBOX_DESCRIPTOR, 0x14) == 0 && strncmp(hdr2, XBOX_DESCRIPTOR, 0x14) == 0)
        {
            DataOffset = 0;
            SetSectorFormat(stISO);
            return TRUE;
        }
    }

    return FALSE;
}

BOOL CISOImage::CheckForMDF(BOOL quiet /* = FALSE*/)
{
    // check sync header
    char sync_hdr[12];
    if (ReadDataByPos(0x0, sizeof(sync_hdr), sync_hdr) == sizeof(sync_hdr) &&
        memcmp(sync_hdr, SYNC_HEADER, 12) == 0)
    {
        if (ReadDataByPos(2352, sizeof(sync_hdr), sync_hdr) == sizeof(sync_hdr))
        {
            if (memcmp(sync_hdr, SYNC_HEADER_MDF, 12) == 0)
            {
                DataOffset = 0;
                SectorRawSize = 0x990;
                // SetSectorFormat() already called no change needed
                return TRUE;
            }
            else if (memcmp(sync_hdr, SYNC_HEADER_MDF_AUDIO, 12) == 0)
            {
                DataOffset = 0;
                // set sector format
                SectorHeaderSize = 0;
                SectorRawSize = 0x990;
                SectorUserSize = 0x930;
                return TRUE;
            }
        }
    }

    return FALSE;
}

BOOL CISOImage::CheckForHFS(BOOL quiet /* = FALSE*/)
{
    // First check for Device Manager header
    CHFS::Block0 block0;

    if ((ReadDataByPos(GetSectorOffset(0), sizeof(block0), &block0) == sizeof(block0)) &&
        (block0.sbSig == HFS_DEVICE_SIGNATURE))
    {
        SetSectorFormat(stISO);
        SectorHeaderSize = 0;
        SectorUserSize = SectorRawSize = FromM16(block0.sbBlkSize);
        return TRUE;
    }

    // Check if dealing directly with a partition
    CHFS::HFSPlusVolumeHeader volHeader;

    if ((ReadDataByPos(GetSectorOffset(0) + 1024, sizeof(volHeader), &volHeader) == sizeof(volHeader)) &&
        ((volHeader.signature == kHFSPlusSigWord) || (volHeader.signature == HFS_PARTITION_HFS_SIGNATURE)))
    {
        SetSectorFormat(stISO);
        SectorHeaderSize = 0;
        SectorUserSize = SectorRawSize = 512;
        return TRUE;
    }

    return FALSE;
}

BOOL CISOImage::CheckForAPFS(BOOL quiet /* = FALSE*/)
{
    CAPFS::ContainerSuperblock superblock;

    if ((ReadDataByPos(GetSectorOffset(0) + 32, sizeof(superblock), &superblock) == sizeof(superblock)) &&
        (superblock.magic == APFS_CONTAINER_SUPERBLOCK_SIGNATURE))
    {
        SetSectorFormat(stISO);
        SectorHeaderSize = 0;
        SectorUserSize = SectorRawSize = FromM16(superblock.blocksize);
        return TRUE;
    }

    return FALSE;
}

//BOOL
//CISOImage::ReadSessionInfo(BOOL quiet/* = FALSE*/)
//{
//  if (!FileName)
//    return FALSE;
//
//  BOOL ret = FALSE;
//
//  int len = strlen(FileName);
//  char *fn = new char [len + 1];
//BUGBUG: test na fn == NULL
//  ZeroMemory(fn, len);
//  strcpy(fn, FileName);
//
//  // zjistit priponu
//  char *ext = strrchr(fn, '.');
//  if (ext != NULL) {   // ".cvspass" is extension in Windows
//
//BUGBUG: co kdyz se prijde pripona "nrgXX", ohodnotis ji jako "nrg"
//    char extlwr[3];
//    strncpy(extlwr, ext + 1, 3);
//BUGBUG: strncpy() neterminuje retezec, takze _strlwr nasledne prepisovalo stack
//    _strlwr(extlwr);
//
//    if (strncmp(extlwr, "nrg", 3) == 0)
//      ret = ReadSessionNRG(quiet);
//    else if (strncmp(extlwr, "img", 3) == 0)
//    {
//      fn[strlen(fn) - 3] = '\0';
//      strcat(fn, "ccd");
//      ret = ReadSessionCCD(fn, quiet);
//    }
//    else if (strncmp(extlwr, "ccd", 3) == 0)
//      ret = ReadSessionCCD(fn, quiet);
//  }
//  else
//    ret = FALSE;
//
//  delete [] fn;
//
//  return ret;
//}

BOOL CISOImage::ReadSessionInfo(BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE2("CISOImage::ReadSessionInfo(%d)", quiet);

    if (!FileName)
        return FALSE;

    BOOL ret = FALSE;

    char* fn = SalamanderGeneral->DupStr(FileName);
    if (fn != NULL)
    {
        char* ext = strrchr(fn, '.');
        if (ext != NULL) // ".cvspass" is extension in Windows
        {
            ext++;
            if (SalamanderGeneral->StrICmp(ext, "nrg") == 0)
                ret = ReadSessionNRG(quiet);
            else if (SalamanderGeneral->StrICmp(ext, "img") == 0)
            {
                strcpy(ext, "ccd");
                ret = ReadSessionCCD(fn, quiet);
            }
            //      else if (SalamanderGeneral->StrICmp(ext, "ccd") == 0)
            //        ret = ReadSessionCCD(fn, quiet);
        }
        SalamanderGeneral->Free(fn);
    }
    return ret;
}

BYTE CISOImage::GetHeaderSizeFromMode(ETrackMode mode, DWORD sectorRawSize)
{
    BYTE headerSize = 0;
    switch (mode)
    {
    case mMode1:
        if (sectorRawSize == 0x930)
            headerSize = 16;
        else if (sectorRawSize == 0x808)
            headerSize = 8;
        else if (sectorRawSize == 0x91C)
            headerSize = 8;
        else
            headerSize = 0;
        break;

    case mMode2:
        if (sectorRawSize == 0x920)
            headerSize = 8;
        else if (sectorRawSize == 0x930)
            headerSize = 24;
        else if (sectorRawSize == 0x990)
            headerSize = 16;
        else
            headerSize = 0;

        break;
    } // switch

    return headerSize;
}

LONGLONG CISOImage::GetCurrentTrackEnd(int trackno)
{
    DWORD loSize, hiSize;
    LONGLONG size, start;

    loSize = File->GetFileSize(&hiSize);
    if (INVALID_FILE_SIZE == loSize)
    {
        return 0xFFFFFFFFFFFFFFFF;
    }
    size = ((LONGLONG)hiSize << 32) | loSize;
    start = Tracks[trackno]->Start;
    int i;
    for (i = 0; i < Tracks.Count; i++)
    {
        if (i == trackno)
            continue;
        LONGLONG tmp = Tracks[i]->Start - Tracks[0]->Start;

        if ((tmp > start) && (tmp < size))
        {
            size = tmp;
        }
    }

    return size - start;
}

void CISOImage::SetTrackParams(int trackno)
{
    CALL_STACK_MESSAGE2("CISOImage::SetTrackParams(%d)", trackno);

    Track* track = Tracks[trackno];
    if (track == NULL)
        return;

    ExtentOffset = track->ExtentOffset;
    DataOffset = track->Start;
    DataEnd = track->End;
    SectorRawSize = track->SectorSize;
    SectorHeaderSize = track->SectorHeaderSize;
}

BOOL CISOImage::Open(const char* fileName, BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE3("CISOImage::Open(%s, %d)", fileName, quiet);

    if (!fileName)
        return FALSE;

    // zapamatovat si jmeno otviraneho souboru
    if ((FileName = new char[strlen(fileName) + 1]) == NULL)
        return Error(IDS_INSUFFICIENT_MEMORY, quiet);

    strcpy(FileName, fileName);

    HANDLE hFile = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE)
    {
        char errStr[MAX_PATH];

        sprintf(errStr, LoadStr(IDS_CANT_OPEN_FILE), fileName);
        return Error(errStr, GetLastError(), quiet);
    }
    DWORD l = 0;
    DWORD dwBytesRead;
    LONG posHi = -1; // Upper part of -512

    // NOTE: DMG might be uncompressed disk image not containing some (zero-filled) blocks
    // -> It is wrong to parse files starting with 'ER' as HFS+, we must check for DMG footer!
    SetFilePointer(hFile, -512, &posHi, FILE_END);
    ReadFile(hFile, &l, 4, &dwBytesRead, NULL);
    SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
    if (DMG_FOOTER_MAGIC != l)
    {
        posHi = -1;
        SetFilePointer(hFile, -4, &posHi, FILE_END);
        ReadFile(hFile, &l, 4, &dwBytesRead, NULL);
    }
    if ((DMG_FOOTER_MAGIC == l) || (DMG_FOOTER_ENCR == l))
    {
        CDMGFile* DMGFile = new CDMGFile(hFile, quiet);
        if (DMGFile && !DMGFile->IsOK())
        {
            // Error message already shown
            delete DMGFile;
            return FALSE;
        }
        File = DMGFile;
    }
    else
    {
        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
        ReadFile(hFile, &l, 4, &dwBytesRead, NULL);
        if (ISZ_HEADER_MAGIC == l)
        {
            CISZFile* ISZFile = new CISZFile(hFile, quiet);
            if (ISZFile && !ISZFile->IsOK())
            {
                // Error message already shown
                delete ISZFile;
                return FALSE;
            }
            File = ISZFile;
        }
        else
        {
            // If an error occured, CBufferedFile will complain soon...
            File = new CBufferedFile(hFile, GENERIC_READ);
        }
    }

    if (!File)
        return Error(IDS_INSUFFICIENT_MEMORY, quiet);

    File->GetFileTime(NULL, NULL, &LastWrite);

    int ret = ERR_OK;
    BOOL validISO = FALSE;
    try
    {
        if (ReadSessionInfo(quiet))
        {
            //
            int trk;
            for (trk = 0; trk < Tracks.Count; trk++)
            {
                if (Tracks[trk]->FSType != fsUnknown && Tracks[trk]->FSType != fsAudio)
                {
                    // pokud se nepodari track otevrit neni to podporovany format a nejspis jsou to data (napr. na video cd)
                    SetTrackParams(trk);
                    ret = DetectTrackFS(trk);
                    if (ret == ERR_CONTINUE)
                    {
                        Tracks[trk]->FSType = fsData;
                        ret = ERR_OK; // raw data tracky umime, takze chybu zmenime na OK (jsme to ale sibalove :-D)
                    }
                    if (ret == ERR_TERMINATE)
                        throw ERR_TERMINATE;
                }
            }

            // find last 'readable' track
            int lastTrack = Tracks.Count - 1;
            while (lastTrack >= 0)
            {
                Track* track = Tracks[lastTrack];
                if (track->FSType != fsUnknown)
                {
                    if (track->FSType == fsAudio)
                        validISO = TRUE;
                    else
                    {
                        SetTrackParams(lastTrack);
                        if (CheckForISO(quiet))
                        {
                            validISO = TRUE;
                            break;
                        }
                        else if (CheckForHFS(quiet))
                        {
                            validISO = TRUE;
                            break;
                        }
                    }
                }

                lastTrack--;
            }

            LastTrack = lastTrack;

            // Patera 2008.11.15: (LastTrack == -1) replaced with !validISO to support AudioCD
            if (!validISO)
                ret = ERR_CONTINUE;
        }
        else
        {
            // if we don't know the CD info, set it up according to known information
            DataOffset = 0x0;

            DetectSectorType();
            SetSectorFormat(SectorType);

            if (CheckForISO(quiet))
                validISO = TRUE;
            else if (CheckForNRG(quiet))
                validISO = TRUE;
            else if (CheckFor2336(quiet))
                validISO = TRUE;
            else if (CheckForNCD(quiet))
                validISO = TRUE;
            else if (CheckForPDI(quiet))
                validISO = TRUE;
            else if (CheckForECDC(quiet))
                validISO = TRUE;
            else if (CheckForCIF(quiet))
                validISO = TRUE;
            else if (CheckForC2D(quiet))
                validISO = TRUE;
            else if (CheckForCIF2332(quiet))
                validISO = TRUE;
            else if (CheckForXbox(quiet))
                validISO = TRUE;
            else if (CheckForMDF(quiet))
                validISO = TRUE;
            else if (CheckForHFS(quiet))
                validISO = TRUE;
            else if (CheckForAPFS(quiet))
                validISO = TRUE;

            if (validISO)
            {
                Track* track = new Track;
                track->Start = DataOffset;
                track->ExtentOffset = 0;

                DWORD hi;
                DWORD lo = File->GetFileSize(&hi);
                track->End = ((LONGLONG)hi << 32) | lo;
                track->SectorSize = (unsigned)SectorRawSize;
                track->SectorHeaderSize = (BYTE)SectorHeaderSize;

                switch (SectorType)
                {
                case stMode1:
                case stCIF:
                case stISO:
                    track->Mode = mMode1;
                    break;

                case stMode2Form1:
                case stMode2Form2:
                case stMode2Form12336:
                    track->Mode = mMode2;
                    break;
                } // switch

                LastTrack = 0;
                Tracks.Add(track);
                Session.Add(1);

                ret = DetectTrackFS(LastTrack);
                if (ret == ERR_CONTINUE)
                    track->FSType = fsData;
                if (ret == ERR_TERMINATE)
                    throw ERR_TERMINATE;
            }
            else
            {
                ret = ERR_CONTINUE;
            }
        }
    }
    catch (int e)
    {
        ret = e;
    }

    if (!validISO)
    {
        BOOL bBootOrMBR = (ret == ERR_CONTINUE) ? CheckForBootSectorOrMBR() : false;

        File->Close(FileName, SalamanderGeneral->GetMainWindowHWND());
        if (ret == ERR_CONTINUE)
            Error(bBootOrMBR ? IDS_BOOT_OR_MBR : IDS_UNKNOWN_FILE_FORMAT, quiet);
    }

    return (ret == ERR_OK);
}

int CISOImage::DetectTrackFS(int track)
{
    CALL_STACK_MESSAGE2("CISOImage::DetectTrackFS(%d)", track);

    if (track < 0 || track >= Tracks.Count)
        return ERR_CONTINUE;

    ETrackFSType trackFSType = fsUnknown;

    int ret = ERR_OK;
    // starting with block 16
    DWORD block = 0x10;

    char* sector = new char[SectorUserSize]; // (2k)
    if (!sector)
    {
        Error(IDS_INSUFFICIENT_MEMORY, FALSE);
        return ERR_TERMINATE;
    }

    BOOL terminate = FALSE;
    BOOL isoDescriptors = FALSE;
    BOOL udfDescriptors = FALSE;
    BOOL hfsDescriptors = FALSE;
    BOOL apfsDescriptors = FALSE;

    if (ReadBlock(0, SectorUserSize, sector) == SectorUserSize)
    {
        if ((sector[0] == 'E') && (sector[1] == 'R'))
        { // Looks like Apple Device Manager Block0
            hfsDescriptors = TRUE;
        }
    }
    if (!hfsDescriptors & (ReadBlock(2, SectorUserSize, sector) == SectorUserSize))
    {
        if (((sector[0] == 'H') && (sector[1] == '+')) || ((sector[0] == 'B') && (sector[1] == 'D')))
        { // Looks like HFSPlus resp. HFSP partition
            hfsDescriptors = TRUE;
        }
        if (((sector[0] == 'N') && (sector[1] == 'X')) || ((sector[0] == 'S') && (sector[1] == 'B')))
        { // Looks like APFS partition
            apfsDescriptors = TRUE;
        }
    }

    DWORD fileSizeHi;
    LONGLONG fileSize = File->GetFileSize(&fileSizeHi);

    fileSize |= (LONGLONG)fileSizeHi << 32;

    // Try to find VolumeDescriptor
    // But do not read beyond file size. See empty.isz having just 50KB uncompressed
    while (!terminate && (GetSectorOffset(block) + SectorUserSize <= fileSize))
    {
        // Try to read a block
        if (ReadBlock(block, SectorUserSize, sector) != SectorUserSize)
        {
            // Something went wrong, probably EOF?
            break;
        }

        if (strncmp(sector + 1, "CD001", 5) == 0)
        {
            switch (sector[0])
            {
            case 0x00: // boot record
                Tracks[track]->Bootable = TRUE;
                break;

            case 0x01: // primary volume descriptor
                isoDescriptors = TRUE;
                break;

            case 0xff: // terminator
                break;
            } // switch
        }     // if

        if (sector[0] == 0x00 && strncmp(sector + 1, "BEA01", 5) == 0)
        {
            udfDescriptors = TRUE;
        }
        else if (sector[0] == 0x00 && strncmp(sector + 1, "NSR02", 5) == 0 ||
                 sector[0] == 0x00 && strncmp(sector + 1, "NSR03", 5) == 0)
        {
            udfDescriptors = TRUE;
        }
        else if (sector[0] == 0x00 && strncmp(sector + 1, "TEA01", 5) == 0)
        {
        }

        block++;

        // Just to make sure we don't read in a too big file, stop after 128 sectors.
        if (block > 128)
            break;
    }

    // detecting XBOX
    BOOL xboxDescriptors = FALSE;

    if (GetSectorOffset(0x20) + SectorUserSize <= fileSize)
    {
        // Try to read a block
        if (ReadBlock(0x20, SectorUserSize, sector) == SectorUserSize)
        {
            if (strncmp(sector, XBOX_DESCRIPTOR, 0x14) == 0 &&
                strncmp(sector + 0x7ec, XBOX_DESCRIPTOR, 0x14) == 0)
            {
                xboxDescriptors = TRUE;
            }
        }
    }

    if (isoDescriptors)
    {
        if (udfDescriptors)
        {
            if (hfsDescriptors)
                trackFSType = fsUDF_ISO9660_HFS;
            else if (apfsDescriptors)
                trackFSType = fsUDF_ISO9660_APFS;
            else
                trackFSType = fsUDF_ISO9660;
        }
        else if (xboxDescriptors)
            trackFSType = fsXbox;
        else if (hfsDescriptors)
            trackFSType = fsISO9660_HFS;
        else if (apfsDescriptors)
            trackFSType = fsISO9660_APFS;
        else
            trackFSType = fsISO9660;
    }
    else
    {
        if (udfDescriptors)
        {
            if (hfsDescriptors)
                trackFSType = fsUDF_HFS;
            else if (apfsDescriptors)
                trackFSType = fsUDF_APFS;
            else
                trackFSType = fsUDF;
        }
        else if (xboxDescriptors)
            trackFSType = fsXbox;
        else if (hfsDescriptors)
            trackFSType = fsHFS;
        else if (apfsDescriptors)
            trackFSType = fsAPFS;
        else
            ret = ERR_CONTINUE;
    }

    Tracks[track]->FSType = trackFSType;

    delete[] sector;

    return ret;
}

BOOL CISOImage::OpenTrack(int track, BOOL quiet)
{
    CALL_STACK_MESSAGE3("CISOImage::OpenTrack(%d, %d)", track, quiet);

    if (track < 0 || track >= Tracks.Count)
        return FALSE;

    // chceme otevrit track, ktery uz je otevreny. to nemusime ;)
    if (OpenedTrack == track)
        return TRUE;

    BOOL ret = FALSE;
    if (Tracks[track]->FileSystem == NULL)
    {
        CUnISOFSAbstract* fileSystem = NULL;

        switch (Tracks[track]->FSType)
        {
        case fsAudio:
            Tracks[track]->FileSystem = new CAudio(this, track);
            break;

        case fsISO9660:
            SetTrackParams(track);
            fileSystem = new CISO9660(this, ExtentOffset);
            break;

        case fsUDF_ISO9660:
        case fsUDF_ISO9660_HFS:
        case fsISO9660_HFS:
        case fsUDF_HFS:
        case fsUDF_ISO9660_APFS:
        case fsISO9660_APFS:
        case fsUDF_APFS:
            SetTrackParams(track);
            fileSystem = new CUDFISO(this, ExtentOffset);
            break;

        case fsAPFS:
            Error(IDS_UNSUPPORTED_APFS, quiet);
            break;

        case fsHFS:
            SetTrackParams(track);
            fileSystem = new CHFS(this);
            break;

        case fsUDF:
            SetTrackParams(track);
            fileSystem = new CUDF(this, ExtentOffset, track);
            break;

        case fsXbox:
            SetTrackParams(track);
            fileSystem = new CXDVDFS(this, ExtentOffset);
            break;

        case fsData:
            SetTrackParams(track);
            fileSystem = new CRawFS(this, ExtentOffset);
            break;
        } // switch

        if (fileSystem != NULL)
        {
            if (fileSystem->Open(quiet))
                Tracks[track]->FileSystem = fileSystem;
            else
                delete fileSystem;
        }
    }
    else
    {
        SetTrackParams(track);
    }

    if (Tracks[track]->FileSystem != NULL)
    {
        OpenedTrack = track;
        ret = TRUE;
    }

    return ret;
}

DWORD
CISOImage::ReadBlock(DWORD block, DWORD size, void* data)
{
    SLOW_CALL_STACK_MESSAGE4("CISOImage::ReadBlock(%u, %u, 0x%p)", block, size, data);

    char sectorStat[0x8000];
    char* sector = SectorUserSize <= sizeof(sectorStat) ? sectorStat : new char[SectorUserSize]; // nemuze selhat (viz allochan.* v Salamanderovi)

    BYTE* end = (BYTE*)data;
    DWORD remain = size;
    while (remain > 0)
    {
        if (ReadDataByPos(GetSectorOffset(block), SectorUserSize, sector) != SectorUserSize)
        {
            if (sector != sectorStat)
                delete[] sector;

            return 0;
        }

        if (remain > SectorUserSize)
        {
            memcpy(end, sector, SectorUserSize);
            end += SectorUserSize;
            remain -= SectorUserSize;
        }
        else
        {
            memcpy(end, sector, remain);
            end += remain;
            remain = 0;
        }
        block++;
    }

    if (sector != sectorStat)
        delete[] sector;

    return size;
}

BYTE CISOImage::GetTrackFromExtent(DWORD extent)
{
    CALL_STACK_MESSAGE2("CISOImage::GetTrackFromExtent(%u)", extent);

    int track;
    for (track = 1; track < Tracks.Count; track++)
    {
        Track* t = Tracks[track];
        if (extent < t->ExtentOffset)
            return track - 1;
    }

    return Tracks.Count - 1;
}

BOOL CISOImage::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    BOOL ret = FALSE;
    if (OpenedTrack >= 0 && OpenedTrack < Tracks.Count)
        ret = Tracks[OpenedTrack]->FileSystem->ListDirectory(path, session, dir, pluginData);

    return ret;
}

BOOL CISOImage::ListImage(CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE1("CISOImage::ListImage(, )");

    char path[2 * MAX_PATH + 1];
    ZeroMemory(&path, sizeof(path));

    if (Options.SessionAsDirectory && Session.Count > 1)
    {
        BOOL ret = TRUE;

        int firstSessionTrack = 0;
        int session;
        for (session = 0; session < Session.Count; session++)
        {
            sprintf(path, "\\Session %02d", session + 1);

            int trackCount = Session[session];
            int lastSessionTrack = firstSessionTrack + trackCount;

            int track;
            for (track = firstSessionTrack; track < lastSessionTrack; track++)
            {
                switch (Tracks[track]->FSType)
                {
                case fsAudio:
                    if (OpenTrack(track, TRUE))
                        ListDirectory(path, session + 1, dir, pluginData);
                    break;

                case fsUnknown:
                    // s tim nic neudelame
                    break;

                default:
                    if (OpenTrack(track, TRUE))
                        ListDirectory(path, session + 1, dir, pluginData);
                    else
                        ret = FALSE;
                    break;
                } // switch
            }     // for
            firstSessionTrack += trackCount;
        }

        if (!ret)
            Error(IDS_ERROR_OPENING_MULTISESSION);

        return TRUE;
    }
    else
    {
        if (OpenTrack(LastTrack))
        {
            return ListDirectory(path, -1, dir, pluginData);
        }
        else
        {
            BOOL ret = FALSE;
            // jeste zkusime vlozit audio tracky, pokud nejaky jsou
            int track;
            for (track = 0; track < Tracks.Count; track++)
            {
                if (track != LastTrack && OpenTrack(track, TRUE))
                    if (ListDirectory(path, -1, dir, pluginData))
                        ret = TRUE;
            } // for

            return ret;
        }
    }
}

//
int CISOImage::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                          const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE6("CISOImage::UnpackFile(, %s, %s, %p, %u, %d)", srcPath, path, fileData, silent, toSkip);

    if (fileData == NULL)
        return UNPACK_ERROR;

    CFilePos* fp = (CFilePos*)fileData->PluginData;
    // is it virtual file ? (audio track)
    if (fp == NULL)
        return UNPACK_AUDIO_UNSUP;

    //  BYTE track = fp->Track;
    //  DWORD e = fp->Extent;
    BYTE track = GetTrackFromExtent(fp->Extent);
    /*
  char u[1024];
  sprintf(u, "CISOImage::UnpackFile(): opening track: %d, Extent: 0x%X", track, fp->Extent);
  TRACE_I(u);
*/
    if (!OpenTrack(track))
    {
        Error(IDS_UNKNOWN_TRACK_TYPE);
        return UNPACK_ERROR;
    }

    char nameInArc[MAX_PATH + MAX_PATH];
    strcpy(nameInArc, FileName);
    SalamanderGeneral->SalPathAppend(nameInArc, srcPath, MAX_PATH + MAX_PATH);
    SalamanderGeneral->SalPathAppend(nameInArc, fileData->Name, MAX_PATH + MAX_PATH);

    CUnISOFSAbstract* fileSystem = Tracks[track]->FileSystem;
    if (fileSystem != NULL)
        return fileSystem->UnpackFile(salamander, srcPath, path, nameInArc, fileData, silent, toSkip);
    else
        return Error(IDS_INTERNAL_PLUGIN_ERROR);
}

BOOL CISOImage::UnpackDir(const char* dirName, const CFileData* fileData)
{
    CALL_STACK_MESSAGE3("CISOImage::UnpackDir(%s, %p)", dirName, fileData);

    if (!SalamanderGeneral->CheckAndCreateDirectory(dirName))
        return UNPACK_ERROR;

    DWORD attrs = fileData->Attr;

    // set attrs to dir
    if (Options.ClearReadOnly) // clear ReadOnly Attribute if needed
        attrs &= ~FILE_ATTRIBUTE_READONLY;

    if (!SetFileAttributes(dirName, attrs))
    {
        DWORD err = GetLastError();
        Error(LoadStr(IDS_CANT_SET_ATTRS), err);
    }

    return UNPACK_OK;
}

int CISOImage::ExtractAllItems(CSalamanderForOperationsAbstract* salamander, char* srcPath,
                               CSalamanderDirectoryAbstract const* dir, const char* mask,
                               char* path, int pathBufSize, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CISOImage::ExtractAllItems(, %s, , %s, %s, %d, %u, %d)", srcPath, mask, path, pathBufSize, silent, toSkip);

    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetFile(i);
        //    TRACE_I("EnumAllItems(): file: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
        salamander->ProgressDialogAddText(file->Name, TRUE); // delayedPaint==TRUE, abychom nebrzdili

        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
        salamander->ProgressSetTotalSize(file->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

        if (SalamanderGeneral->AgreeMask(file->Name, mask, file->Ext[0] != 0))
        {
            //      SalamanderGeneral->CheckAndCreateDirectory(path);
            if (UnpackFile(salamander, srcPath, path, file, silent, toSkip) == UNPACK_CANCEL ||
                !salamander->ProgressAddSize(1, TRUE))
                return UNPACK_CANCEL;
        }
    } // for

    count = dir->GetDirsCount();
    int pathLen = (int)strlen(path);
    int srcPathLen = (int)strlen(srcPath);
    int j;
    for (j = 0; j < count; j++)
    {
        CFileData const* file = dir->GetDir(j);
        //    TRACE_I("EnumAllItems(): directory: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
        if (!SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize))
        {
            Error(IDS_ERR_TOO_LONG_NAME);
            return UNPACK_CANCEL;
        }
        if (UnpackDir(path, file) == UNPACK_CANCEL)
            return UNPACK_CANCEL;

        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);
        SalamanderGeneral->SalPathAppend(srcPath, file->Name, ISO_MAX_PATH_LEN);
        if (ExtractAllItems(salamander, srcPath, subDir, mask, path, pathBufSize, silent, toSkip) == UNPACK_CANCEL)
            return UNPACK_CANCEL;

        srcPath[srcPathLen] = '\0';
        path[pathLen] = '\0';
    }

    return UNPACK_OK;
}

CISOImage::Track*
CISOImage::GetTrack(int track)
{
    CALL_STACK_MESSAGE2("CISOImage::GetTrack(%d)", track);

    if (track >= 0 && track <= Tracks.Count)
        return Tracks[track];
    else
        return NULL;
}
