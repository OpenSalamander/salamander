// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "isoimage.h"
#include "audio.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

// Nero Header
// pro cteni informace o session/tracks v .NRG
struct NRGHeader
{
    char ID[4];
    DWORD Size;
};

struct DAOX
{
    DWORD Size;
    BYTE Data[0x10];
    BYTE FirstTrack;
    BYTE LastTrack;
};

// swaping macros

#define SWAPWORD(d) \
    (((d)&0x00FF) << 8) | \
        (((d)&0xFF00) >> 8)

#define SWAPDWORD(d) \
    (((d)&0x000000FF) << 24) | \
        (((d)&0x0000FF00) << 8) | \
        (((d)&0x00FF0000) >> 8) | \
        (((d)&0xFF000000) >> 24)

#define SWAPLONGLONG(l) \
    (((l)&0x00000000000000FF) << 56) | \
        (((l)&0x000000000000FF00) << 40) | \
        (((l)&0x0000000000FF0000) << 24) | \
        (((l)&0x00000000FF000000) << 8) | \
        (((l)&0x000000FF00000000) >> 8) | \
        (((l)&0x0000FF0000000000) >> 24) | \
        (((l)&0x00FF000000000000) >> 40) | \
        (((l)&0xFF00000000000000) >> 56)

static WORD
getWord(void* var)
{
    WORD w;
    memcpy(&w, var, 2);
    return SWAPWORD(w);
}

static DWORD
getDWord(void* var)
{
    DWORD d;
    memcpy(&d, var, 4);
    return SWAPDWORD(d);
}

static LONGLONG
getLongLong(void* var)
{
    LONGLONG l;
    memcpy(&l, var, 8);
    return SWAPLONGLONG(l);
}

static void SetTrackTypeMode(CISOImage::Track* track, int type)
{
    CALL_STACK_MESSAGE2("SetTrackTypeMode(, %d)", type);

    if (type == 0x07)
    {
        track->FSType = CISOImage::fsAudio;
        track->Mode = CISOImage::mNone;
    }
    else if (type == 0x00 || type == 0x05)
    {
        track->FSType = CISOImage::fsISO9660;
        track->Mode = CISOImage::mMode1;
    }
    else if (type & 0x02)
    {
        track->FSType = CISOImage::fsISO9660;
        track->Mode = CISOImage::mMode2;
    }
    else
    {
        track->FSType = CISOImage::fsUnknown;
        track->Mode = CISOImage::mNone;
    }
}

static BOOL
ReadHeader(CFile& file, NRGHeader* header)
{
    CALL_STACK_MESSAGE2("ReadHeader(, %p)", header);

    DWORD read;

    BYTE hdr[8];
    if (!file.Read(hdr, sizeof(hdr), &read, NULL, NULL))
        return FALSE;

    if (read != sizeof(hdr))
        return FALSE;

    memcpy(header->ID, hdr, 4);
    header->Size = getDWord(hdr + 4);

    return TRUE;
}

static BOOL
readCUEX(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readCUEX(, %p, %p)", header, iso);

    DWORD read;

    int trackCount = (header->Size - 0x10) / 0x10;

    BYTE cuex_hdr[0x10];
    if (!file.Read(&cuex_hdr, sizeof(cuex_hdr), &read, NULL, NULL))
        return FALSE;

    int i;
    for (i = 0; i < trackCount; i++)
    {
        BYTE data[0x10];
        if (!file.Read(&data, sizeof(data), &read, NULL, NULL))
            return FALSE;

        CISOImage::Track* track = new CAudioTrack;
        track->ExtentOffset = getDWord(data + 0x04);
        iso->AddTrack(track);
    } // for

    return TRUE;
}

static BOOL
readDAOX(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readDAOX(, %p, %p)", header, iso);

    DWORD read;

    BYTE daox_hdr[0x16];
    if (!file.Read(&daox_hdr, sizeof(daox_hdr), &read, NULL, NULL))
        return FALSE;

    BYTE firstTrack = daox_hdr[0x14];
    BYTE lastTrack = daox_hdr[0x15];

    int i;
    for (i = firstTrack; i <= lastTrack; i++)
    {
        BYTE data[0x2A];
        if (!file.Read(&data, sizeof(data), &read, NULL, NULL))
            return FALSE;

        CISOImage::Track* track = iso->GetTrack(i - 1);
        track->SectorSize = getWord(data + 0x0C);
        SetTrackTypeMode(track, data[0x0E]);
        track->Start = getLongLong(data + 0x1A);
        track->End = getLongLong(data + 0x22);
        track->SectorHeaderSize = CISOImage::GetHeaderSizeFromMode(track->Mode, track->SectorSize);
    } // for

    return TRUE;
}

static BOOL
readETNF(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readETNF(, %p, %p)", header, iso);

    DWORD read;
    BYTE* data = new BYTE[header->Size];
    ZeroMemory(data, header->Size);

    if (!file.Read(data, header->Size, &read, NULL, NULL))
    {
        delete[] data;
        return FALSE;
    }

    int offset = 0;
    unsigned int i;
    for (i = 0; i < header->Size / 0x14; i++)
    {
        CISOImage::Track* track = new CISOImage::Track;

        // sector size
        if (data[offset + 0x0B] & 1)
            track->SectorSize = 0x920;
        else
            track->SectorSize = 0x800;

        track->Start = (LONGLONG)getDWord(data + offset + 0x00);
        track->End = track->Start + (LONGLONG)getDWord(data + offset + 0x04);
        SetTrackTypeMode(track, data[offset + 0x0B]);
        track->SectorHeaderSize = CISOImage::GetHeaderSizeFromMode(track->Mode, track->SectorSize);
        track->ExtentOffset = getDWord(data + offset + 0x0C);
        iso->AddTrack(track);

        offset += 0x14;
    }

    delete[] data;
    return TRUE;
}

static BOOL
readETN2(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readETN2(, %p, %p)", header, iso);

    DWORD read;
    BYTE* data = new BYTE[header->Size];
    ZeroMemory(data, header->Size);

    if (!file.Read(data, header->Size, &read, NULL, NULL))
    {
        delete[] data;
        return FALSE;
    }

    CISOImage::Track* track = new CISOImage::Track;
    track->SectorSize = 2048;
    track->FSType = CISOImage::fsISO9660;
    track->Mode = CISOImage::mMode1;
    track->SectorHeaderSize = CISOImage::GetHeaderSizeFromMode(CISOImage::mMode1, track->SectorSize);
    track->Start = getLongLong(data + 0x00);
    track->End = getLongLong(data + 0x08);
    track->ExtentOffset = 0;
    iso->AddTrack(track);

    delete[] data;
    return TRUE;
}

static BOOL
readCDTX(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readCDTX(, %p, %p)", header, iso);

    DWORD read;
    char* data = new char[header->Size + (1 + iso->GetTrackCount())];

    if (!file.Read(data, header->Size, &read, NULL, NULL))
    {
        delete[] data;
        return FALSE;
    }

    char *src, *dst;
    src = data + 4; // Skip unknown 4 bytes (flags)
    dst = data;
    int i;
    for (i = header->Size; i > 0; i -= 12 + 6)
    { // Remove some unknown data (there are blocks of 12 bytes of labels followed by unkown 6 bytes)
        memmove(dst, src, min(12, i));
        dst += 12;
        src += 12 + 6;
    }
    // Make our life easier by adding as many safety terminators as may be needed
    memset(dst, 0, 1 + iso->GetTrackCount());
    src = data;
    iso->SetLabel(src);
    src += strlen(src) + 1; // Skip volume label
    int j;
    for (j = 0; j < iso->GetTrackCount(); j++)
    {
        iso->GetTrack(j)->SetLabel(src);
        src += strlen(src) + 1;
    }

    delete[] data;
    return TRUE;
}

static BOOL
readSINF(CFile& file, NRGHeader* header, CISOImage* iso)
{
    CALL_STACK_MESSAGE3("readSINF(, %p, %p)", header, iso);

    DWORD read;
    DWORD count;

    if (!file.Read(&count, sizeof(count), &read, NULL, NULL))
        return FALSE;

    count = SWAPDWORD(count);
    iso->AddSessionTracks(count);

    return TRUE;
}

static BOOL
ReadNRGRoot(CFile& file, CISOImage* iso)
{
    CALL_STACK_MESSAGE2("ReadNRGRoot(, %p)", iso);

    BOOL ret = FALSE;

    // check header
    NRGHeader header;
    DWORD read;

    if (!file.Read(&header.ID, sizeof(header.ID), &read, NULL, NULL))
        return FALSE;

    if ((read != 4) || (strncmp(header.ID, "NER5", 4) != 0))
        return FALSE;

    // read offset
    LONGLONG offset;
    if (!file.Read(&offset, sizeof(offset), &read, NULL, NULL))
        return FALSE;
    if (read != 8)
        return FALSE;

    offset = getLongLong(&offset);

    // set file pointer to start of session information
    if (file.Seek(offset, FILE_BEGIN) == -1)
        return FALSE;

    do
    {
        if (!ReadHeader(file, &header))
            return FALSE;

        if (strncmp(header.ID, "CUEX", 4) == 0)
            readCUEX(file, &header, iso);
        else if (strncmp(header.ID, "DAOX", 4) == 0)
            readDAOX(file, &header, iso);
        else if (strncmp(header.ID, "SINF", 4) == 0)
            readSINF(file, &header, iso);
        else if (strncmp(header.ID, "ETNF", 4) == 0)
            readETNF(file, &header, iso);
        else if (strncmp(header.ID, "ETN2", 4) == 0)
            readETN2(file, &header, iso);
        else if (strncmp(header.ID, "CDTX", 4) == 0)
            readCDTX(file, &header, iso);
        else if (file.Seek(header.Size, FILE_CURRENT) == -1)
            return FALSE;

    } while (strncmp(header.ID, "NER5", 4) != 0);

    return TRUE;
}

BOOL CISOImage::ReadSessionNRG(BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE2("CISOImage::ReadSessionNRG(%d)", quiet);

    DWORD fileSizeHi;
    DWORD fileSizeLo = File->GetFileSize(&fileSizeHi);
    if (INVALID_FILE_SIZE == fileSizeLo)
        return FALSE;

    LARGE_INTEGER pos;
    pos.QuadPart = (((LONGLONG)fileSizeHi) << 32) + fileSizeLo - 12;
    if (File->Seek(pos.QuadPart, FILE_BEGIN) == -1)
        return FALSE;

    BOOL ret = ReadNRGRoot(*File, this);

    return ret;
}
