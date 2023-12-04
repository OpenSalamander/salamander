// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "udf.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

typedef wchar_t unicode_t;

#define SECTOR_SIZE 0x800
#define INVALID_SECTOR_NUMBER -11111111 // reasonable negative number; Should not be -1 or so!!

// Maximum length of filenames allowed in UDF.
#define MAX_UDF_FILE_NAME_LEN 2048

// Flagy pro testovani File Identifier Descriptoru
#define UDF_HIDDEN 0x01
#define UDF_DIRECTORY 0x02
#define UDF_DELETED 0x04
#define UDF_PARENT 0x08
#define UDF_METADATA 0x10

/* For direct data access, LSB first */
#define GET_BYTE(data, p) ((Uint8)data[p])
#define GET_WORD(data, p) ((Uint16)data[p] | ((Uint16)data[(p) + 1] << 8))
//#define GETN3(p) ((Uint32)data[p] | ((Uint32)data[(p) + 1] << 8) \
//		  | ((Uint32)data[(p) + 2] << 16))
#define GET_DWORD(data, p) ((Uint32)data[p] | ((Uint32)data[(p) + 1] << 8) | ((Uint32)data[(p) + 2] << 16) | ((Uint32)data[(p) + 3] << 24))
#define GET_QWORD(data, p) ((Uint64)data[p] | ((Uint64)data[(p) + 1] << 8) | ((Uint64)data[(p) + 2] << 16) | ((Uint64)data[(p) + 3] << 24) | ((Uint64)data[(p) + 4] << 32) | ((Uint64)data[(p) + 5] << 40) | ((Uint64)data[(p) + 6] << 48) | ((Uint64)data[(p) + 7] << 56))
/* This is wrong with regard to endianess */
#define GETN(data, p, n, target) memcpy(target, &data[p], n)

static void
UDFTimeStampToSystemTime(CUDF::CTimeStamp& stamp, SYSTEMTIME* st)
{
    st->wYear = stamp.Year;
    st->wMonth = stamp.Month;
    st->wDayOfWeek = 0;
    st->wDay = stamp.Day;
    st->wHour = stamp.Hour;
    st->wMinute = stamp.Minute;
    st->wSecond = stamp.Second;
    st->wMilliseconds = 0;
}

static void
UDFTimeStampToFileTime(CUDF::CTimeStamp& stamp, FILETIME* ft)
{
    SYSTEMTIME st;
    int offset;

    UDFTimeStampToSystemTime(stamp, &st);
    SystemTimeToFileTime(&st, ft);
    offset = stamp.TypeAndTimezone & 0x0FFF;
    if ((1 == (stamp.TypeAndTimezone >> 12)) && (0xFFF != offset))
    { // Valid timezone, offset in minutes
        __int64 newtime = ft->dwLowDateTime + (((__int64)ft->dwHighDateTime) << 32);

        if (offset & 0x800) // negative 12-bit number -> extend to 32-bit negative number
            offset |= 0xFFFFF000;
        newtime -= ((__int64)offset) * 60 * 1000 * 1000 * 10; // from min units to 100ns units
        ft->dwLowDateTime = (DWORD)(newtime & 0x00000000ffffffff);
        ft->dwHighDateTime = (DWORD)(newtime >> 32);
    }
}

static int
UnicodeLength(unicode_t* string)
{
    int length;
    length = 0;
    while (*string++)
        length++;

    return length;
}

static int
UnicodeUncompress(BYTE* compressed, int numberOfBytes, unicode_t* unicode)
{
    unsigned int compID;
    int returnValue, unicodeIndex, byteIndex;

    // Use 'compressed' to store current byte being read.
    compID = compressed[0];

    // First check for valid compID.
    if (compID != 8 && compID != 16)
    {
        returnValue = -1;
    }
    else
    {
        unicodeIndex = 0;
        byteIndex = 1;
        if (16 == compID)
        {
            // Make it odd number: length byte plus even number of text bytes
            numberOfBytes = ((numberOfBytes - 1) & 0xFFFE) + 1;
        }

        // Loop through all the bytes.
        while (byteIndex < numberOfBytes)
        {
            if (compID == 16)
            {
                // Move the first byte to the high bits of the unicode char.
                unicode[unicodeIndex] = compressed[byteIndex++] << 8;
            }
            else
            {
                unicode[unicodeIndex] = 0;
            }
            if (byteIndex < numberOfBytes)
            {
                // Then the next byte to the low bits.
                unicode[unicodeIndex] |= compressed[byteIndex++];
            }
            unicodeIndex++;
        }
        returnValue = unicodeIndex;
    }

    return returnValue;
}

void DecodeOSTACompressed(BYTE* id, int len, char* result)
{
    unicode_t uncompressed[1024];
    int ucompChars;

    ucompChars = UnicodeUncompress(id, len, uncompressed);
    if (ucompChars < 0)
    { // Invalid string?
        *result = 0;
        return;
    }
    uncompressed[ucompChars] = 0;
    int length = MIN(ucompChars, UnicodeLength(uncompressed)) + 1 /*terminating zero*/;

    char final[1024];

    WideCharToMultiByte(CP_ACP, 0, uncompressed, length, final, sizeof(final) - 1, 0, 0);
    final[sizeof(final) - 1] = 0;
    strcpy(result, final);
}

/*
static char *
DString(BYTE *ds, int count)
{
  static char buffer[256];

  BYTE len = ds[count - 1];
  UnicodeUncompress(ds, len, (unicode_t *)buffer);
  buffer[len] = '\0';
  return buffer;
}
*/

//
//

// ****************************************************************************
//
// CUDF
//

CUDF::CUDF(CISOImage* image, DWORD extent, int trackno) : CUnISOFSAbstract(image),
                                                          PartitionMaps(2, 1, dtDelete)
{
    TrackTryingToOpen = trackno;
    ExtentOffset = extent;
    PartitionMapping.Translation = NULL;
    PartitionMapping.VAT = NULL;
    PartitionMapping.nMetadataICBs = 0;
}

CUDF::~CUDF()
{
    delete[] PartitionMapping.VAT;
}

BOOL CUDF::ReadBlockPhys(Uint32 lbNum, size_t blocks, unsigned char* data)
{
    DWORD len = (DWORD)(blocks * SECTOR_SIZE);

    if (Image->ReadBlock(PD.Start + lbNum, len, data) != len)
        return FALSE;
    else
        return TRUE;
}

Uint32 CUDF::LogSector(Uint32 lbNum, Uint16 part)
{
    DWORD physBlock = INVALID_SECTOR_NUMBER;

    if (part >= PartitionMaps.Count)
    {
        TRACE_E("Invalid partition number");
        return lbNum;
    }

    switch (PartitionMaps[part]->Type)
    {
    case mapPhysical:
    case mapSparable:
        physBlock = lbNum - ExtentOffset;
        break;

    case mapVirtual:
        if (lbNum >= 0 && lbNum <= PartitionMapping.Entries)
            physBlock = PartitionMapping.Translation[lbNum] - ExtentOffset;
        else
            physBlock = INVALID_SECTOR_NUMBER;
        break;

    case mapMetadata:
        LONGLONG pos;
        pos = (LONGLONG)lbNum * SECTOR_SIZE;
        int i;
        for (i = 0; i < PartitionMapping.nMetadataICBs; i++)
        {
            if (pos < PartitionMapping.MetadataICBs[i].Length)
            {
                physBlock = (Uint32)(pos / SECTOR_SIZE + PartitionMapping.MetadataICBs[i].Location) - ExtentOffset;
                break;
            }
            pos -= PartitionMapping.MetadataICBs[i].Length;
            if (PartitionMapping.MetadataICBs[i].Length & (SECTOR_SIZE - 1))
            {
                TRACE_E("Metadata partition translation block is not multiple of sector size");
            }
        }
    } // switch
      /*
  char u[1024];
  sprintf(u, "CUDF::LogSector(): 0x%X -> 0x%X", lbNum, physBlock);
  TRACE_I(u);
*/
    return physBlock;
}

BOOL CUDF::ReadBlockLog(Uint32 lbNum, size_t blocks, unsigned char* data)
{
    if (lbNum == INVALID_SECTOR_NUMBER)
        return FALSE;

    DWORD len = (DWORD)(blocks * SECTOR_SIZE);
    DWORD ofs = 0;

    unsigned int i;
    for (i = 0; i < blocks; i++)
    {
        DWORD readBytes;
        if ((readBytes = Image->ReadBlock(PD.Start + lbNum, SECTOR_SIZE, data + ofs)) != SECTOR_SIZE)
            return FALSE;

        ofs += readBytes;
        lbNum++;
    } // for

    return TRUE;
}

void CUDF::ReadExtentAd(BYTE sector[], CExtentAd* extent)
{
    extent->Length = GET_DWORD(sector, 0);
    extent->Location = GET_DWORD(sector, 4);
}

void CUDF::ReadEntityID(BYTE sector[], CEntityID* entityID)
{
    entityID->Flags = GET_BYTE(sector, 0);
    GETN(sector, 1, 23, &entityID->Identifier);
    GETN(sector, 24, 8, &entityID->Suffix);
}

void CUDF::ReadAVDP(BYTE sector[], CAVDP* avdp)
{
    ReadExtentAd(sector + 16, &avdp->MVDS);
    ReadExtentAd(sector + 24, &avdp->RVDS);
}

void CUDF::ReadPD(BYTE sector[], CPD* pd)
{
    pd->Flags = GET_WORD(sector, 20);
    pd->PartitionNumber = GET_WORD(sector, 22);
    pd->Start = GET_DWORD(sector, 188);
    pd->Length = GET_DWORD(sector, 192);
}

int CUDF::ReadPartitionMap(BYTE data[])
{
    CPartitionMap* partMap = new CPartitionMap;
    CEntityID entityID;

    BYTE partType = GET_BYTE(data, 0);
    switch (partType)
    {
    case 1:
        partMap->VolumeSequenceNumber = GET_WORD(data, 2);
        partMap->PartitionNumber = GET_WORD(data, 4);
        partMap->Type = mapPhysical;
        break;

    case 2:
        partMap->VolumeSequenceNumber = GET_WORD(data, 36);
        partMap->PartitionNumber = GET_WORD(data, 38);

        ReadEntityID(data + 4, &entityID);
        if (strncmp(entityID.Identifier, "*UDF Virtual Partition", 22) == 0)
        {
            partMap->Type = mapVirtual;
        }
        else if (strncmp(entityID.Identifier, "*UDF Sparable Partition", 23) == 0)
        {
            partMap->Type = mapSparable;
        }
        else if (strncmp(entityID.Identifier, "*UDF Metadata Partition", 23) == 0)
        {
            CICB icb;
            BYTE sector[0x800];
            CDescriptorTag tag;
            CICBTag icbTag;

            partMap->Type = mapMetadata;

            // Parsing struct part_map_meta
            icb.Partition = GET_WORD(data, 38);
            icb.Location = GET_DWORD(data, 40) - ExtentOffset;

            if (!ReadBlockLog(icb.Location, 1, sector))
            {
                Error(IDS_ERROR_READING_SECTOR, false, icb.Location);
                break;
            }
            ReadDescriptorTag(sector, &tag);
            if ((UDF_TAGID_EXTFENTRY == tag.ID) || (UDF_TAGID_FENTRY == tag.ID))
            {
                // It is assumed that there is no more than 1 Metadata partition present
                PartitionMapping.nMetadataICBs = ReadFileEntry(sector, UDF_TAGID_EXTFENTRY == tag.ID, 0 /*??*/, &icbTag,
                                                               PartitionMapping.MetadataICBs, sizeof(PartitionMapping.MetadataICBs) / sizeof(PartitionMapping.MetadataICBs[0]));
            }
        }
        break;
    } // switch
    PartitionMapping.Type = partMap->Type;
    PartitionMaps.Add(partMap);

    return GET_BYTE(data, 1);
}

void CUDF::ReadLVD(BYTE sector[], CLVD* lvd)
{
    lvd->VDSN = GET_DWORD(sector, 16);
    lvd->LogicalBlockSize = GET_DWORD(sector, 212);
    GETN(sector, 84, 128, lvd->LogicalVolumeIdentifier);

    LongAD(sector + 248, &lvd->FSD);

    DWORD mapLen = GET_DWORD(sector, 264);
    DWORD mapCount = GET_DWORD(sector, 268);
    int offset = 440;
    DWORD i;
    for (i = 0; i < mapCount; i++)
        offset += ReadPartitionMap(sector + offset);
}

void CUDF::ReadDescriptorTag(BYTE sector[], CDescriptorTag* tag)
{
    tag->ID = (UDF_TAG_ID)GET_WORD(sector, 0);
    tag->Version = GET_WORD(sector, 2);
    tag->Checksum = GET_BYTE(sector, 4);
    tag->Reserved = GET_BYTE(sector, 5);
    tag->SerialNumber = GET_WORD(sector, 6);
    tag->CRC = GET_WORD(sector, 8);
    tag->CRCLength = GET_WORD(sector, 10);
    tag->TagLocation = GET_DWORD(sector, 12);
}

//
// in:
//    data
//
// out:
//    ad, partition
int CUDF::ShortAD(Uint8* data, CUDF::CAD* ad, WORD partNumber)
{
    ad->Length = GET_DWORD(data, 0);
    ad->Flags = (Uint8)(ad->Length >> 30);
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GET_DWORD(data, 4);
    ad->Partition = partNumber; // use number of current partition
    ad->Offset = 0;
    return 0;
}

//
// in:
//    data
//
// out:
//    ad
int CUDF::LongAD(Uint8* data, CUDF::CAD* ad)
{
    Uint32 len = GET_DWORD(data, 0);
    ad->Flags = (Uint8)(len >> 30);
    ad->Length = len & 0x3FFFFFFF;
    ad->Location = GET_DWORD(data, 4);
    ad->Partition = GET_WORD(data, 8);
    ad->Offset = 0;
    return 0;
}

//
// in:
//    data
//
// out:
//    ad
int CUDF::ExtAD(Uint8* data, CUDF::CAD* ad)
{
    ad->Length = GET_DWORD(data, 0);
    ad->Flags = (Uint8)(ad->Length >> 30);
    ad->Length &= 0x3FFFFFFF;
    ad->Location = GET_DWORD(data, 12);
    ad->Partition = GET_WORD(data, 16);
    ad->Offset = 0;
    return 0;
}

void CUDF::ReadICBTag(BYTE data[], CICBTag* icb)
{
    icb->Strategy = GET_WORD(data, 4);
    icb->StrategyParametr = GET_WORD(data, 6);
    icb->Entries = GET_WORD(data, 8);
    icb->FileType = GET_WORD(data, 11);
    icb->Flags = GET_WORD(data, 18);
    /*
  if (icb->FileType == 248)
  {
    char u[1024];
    sprintf(u, "CUDF::ReadICBTag() - %d", icb->FileType);
    TRACE_I(u);
  }
*/
}

//
// in:
//    struct file_entry or struct extfile_entry
//
// out:
//    fileType, partition, ad
int CUDF::ReadFileEntry(Uint8* data, bool bEFE, Uint16 part, CICBTag* icbTag, CUDF::CAD* ad, int maxAds)
{
    Uint32 l_ea, l_ad;
    unsigned int p;
    int cnt = maxAds;

    ReadICBTag(data + 16, icbTag);

    if (bEFE)
    {
        p = 176 + 40; // Extended File Entry (UDF 2.50+)
    }
    else
    {
        p = 176; // File Entry
    }
    l_ea = GET_DWORD(data, p - 8);
    l_ad = GET_DWORD(data, p - 4);
    p += l_ea;
    l_ea = p;
    while (p < l_ea + l_ad)
    {
        if (maxAds > 0)
        {
            switch (icbTag->Flags & 0x0007)
            {
            case 0:
                ShortAD(&data[p], ad, part);
                p += 8;
                ad++;
                maxAds--;
                break;
            case 1:
                LongAD(&data[p], ad);
                p += 16;
                ad++;
                maxAds--;
                break;
            case 2:
                ExtAD(&data[p], ad);
                p += 20;
                ad++;
                maxAds--;
                break;
            case 3:
                ad->Offset = p;
                ad->Flags = UDF_EXT_ALLOCATED;
                ad->Length = l_ad;
                ad->Location = 0; // To be determined by the caller
                ad->Partition = part;
                return -1; // -1 tells the caller the data is inlined, but Location is not set
            }
        }
        else
        {
            TRACE_E("UnISO: UNSUPPORTED: Not enough CAD structures");
            break;
        }
    }
    cnt -= maxAds; // # of used ad entries
    if (maxAds > 0)
    {
        // Terminate the sequence
        // And also init ad for an empty file (i.e. there isn't a AD, L_AD == 0 )
        ad->Length = 0;
        ad->Flags = 0;
        ad->Location = 0; // what should we put here?
        ad->Partition = part;
    }
    return cnt;
}

int CUDF::ReadFileIdentifier(Uint8* sector, Uint8* fileChar, char* fileName, CAD* fileICB)
{
    Uint8 fi;
    Uint16 iu;

    *fileChar = GET_BYTE(sector, 18);
    fi = GET_BYTE(sector, 19);
    LongAD(sector + 20, fileICB);
    iu = GET_WORD(sector, 36);
    if (fi)
        DecodeOSTACompressed(sector + 38 + iu, fi, fileName);
    else
        fileName[0] = '\0';
    return 4 * ((38 + fi + iu + 3) / 4);
}

int CUDF::MapICB(CUDF::CAD icb, CICBTag* icbTag, CUDF::CAD* file)
{
    BYTE sector[SECTOR_SIZE];
    Uint32 lbNum = LogSector(icb.Location, icb.Partition);
    CDescriptorTag tag;

    do
    {
        if (ReadBlockLog(lbNum, 1, sector))
            ReadDescriptorTag(sector, &tag);
        else
            tag.ID = UDF_TAGID_UNKNOWN;

        if ((UDF_TAGID_FENTRY == tag.ID) || (UDF_TAGID_EXTFENTRY == tag.ID))
        {
            if (-1 == ReadFileEntry(sector, (UDF_TAGID_EXTFENTRY == tag.ID), icb.Partition, icbTag, file, 1))
            {
                file->Location = lbNum;
            }
            return 1;
        }

        lbNum++;
    } while (lbNum <= icb.Location + (icb.Length - 1) / SECTOR_SIZE);

    return 0;
}

int CUDF::MapICB(CUDF::CAD icb, CFileEntry* fe)
{
    BYTE sector[SECTOR_SIZE];
    Uint32 lbNum = LogSector(icb.Location, icb.Partition);
    CDescriptorTag tag;

    do
    {
        if (ReadBlockLog(lbNum, 1, sector))
            ReadDescriptorTag(sector, &tag);
        else
            tag.ID = UDF_TAGID_UNKNOWN;

        if (UDF_TAGID_FENTRY == tag.ID)
        {
            ReadFileEntry(sector, false, fe);
            return 1;
        }
        if (UDF_TAGID_EXTFENTRY == tag.ID)
        {
            ReadFileEntry(sector, true, fe);
            return 1;
        }

        lbNum++;
    } while (lbNum <= icb.Location + (icb.Length - 1) / SECTOR_SIZE);

    return 0;
}

BOOL CUDF::Open(BOOL quiet)
{
    CALL_STACK_MESSAGE2("CUDF::Open(%d)", quiet);

    CDescriptorTag tag;

    BOOL ret = TRUE;
    DWORD block = 0x20, lastBlock = 0x200;
    BYTE sector[SECTOR_SIZE]; // (2k)
    BOOL terminate = FALSE;

    if (Image->ReadBlock(256, SECTOR_SIZE, sector) != SECTOR_SIZE)
    {
        // Something went wrong, probably EOF?
        Error(IDS_ERROR_READING_SECTOR, quiet, block);
        return FALSE;
    }
    ReadDescriptorTag(sector, &tag);
    if (UDF_TAGID_ANCHOR == tag.ID)
    {
        ReadAVDP(sector, &AVDP);
        block = AVDP.MVDS.Location - ExtentOffset;
        lastBlock = block + AVDP.MVDS.Length / SECTOR_SIZE;
    }

    // detecting UDF Volume Descriptor Set
    while (!terminate)
    {
        // Try to read a block
        if (Image->ReadBlock(block, SECTOR_SIZE, sector) != SECTOR_SIZE)
        {
            /*      // Something went wrong, probably EOF?
      if (block > 200) {
         block = 0x20; lastBlock = 0x200;
         continue;
      }*/
            Error(IDS_ERROR_READING_SECTOR, quiet, block);
            return FALSE;
        }

        ReadDescriptorTag(sector, &tag);
        // TODO: better validation test of tag struct
        if (tag.TagLocation - ExtentOffset == block)
        {
            switch (tag.ID)
            {
            case UDF_TAGID_PRI_VOL:
                // Primary Volume Descriptor
                memcpy(&PVD, sector, sizeof(PVD));
                break;

                //        case UDF_TAGID_ANCHOR: ReadAVDP(sector, &AVDP); break;
            case UDF_TAGID_PARTITION:
                ReadPD(sector, &PD);
                break;
            case UDF_TAGID_LOGVOL:
                ReadLVD(sector, &LVD);
                break;

            case UDF_TAGID_TERM:
                // Terminating Descriptor
                terminate = TRUE;
                break;
            };
        }

        block++;

        if (block > lastBlock)
        {
            // Just to make sure we don't read in a too big file, stop after 128 sectors.
            ret = FALSE;
            break;
        }
    } // while

    if (ret)
        ret = ReadSupportingTables(quiet);

    return ret;
}

void CUDF::ReadFileEntry(BYTE sector[], bool bEFE, CFileEntry* fe)
{
    ReadICBTag(sector + 16, &fe->ICBTag);

    fe->InformationLength = GET_QWORD(sector, 56);

    if (!bEFE)
    {
        fe->L_EA = GET_DWORD(sector, 168);
        fe->L_AD = GET_DWORD(sector, 172);
        memcpy(&fe->mtime, sector + 84, sizeof(CTimeStamp));
    }
    else
    {
        fe->L_EA = GET_DWORD(sector, 208);
        fe->L_AD = GET_DWORD(sector, 212);
        memcpy(&fe->mtime, sector + 92, sizeof(CTimeStamp));
    }
}

//
// vrati delku precteneho alloc descriptoru
int CUDF::ReadAllocDesc(BYTE data[], CFileEntry* fe, BYTE* vat, DWORD* o)
{
    CAD ad;
    int len = 0;

    switch (fe->ICBTag.Flags & 0x0007)
    {
    case 0:
        ShortAD(data, &ad, PD.PartitionNumber);
        len = 8;
        break;
    case 1:
        LongAD(data, &ad);
        len = 16;
        break;
    case 2:
        ExtAD(data, &ad);
        len = 20;
        break;
    case 3:
        memcpy(vat + *o, data, fe->L_AD);
        *o += fe->L_AD;
        return fe->L_AD;
    }

    if (!Image->ReadBlock(PD.Start + ad.Location - ExtentOffset, ad.Length, vat + *o))
    {
        return -1;
    }
    *o += ad.Length;

    return len;
}

BOOL CUDF::AddFileDir(const char* path, char* fileName, BYTE fileChar, CAD* icb,
                      CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CFileData fd;
    CISOImage::CFilePos* filePos = NULL;
    BOOL ret = TRUE;

    memset(&fd, 0, sizeof(CFileData));
    try
    {
        fd.Name = SalamanderGeneral->DupStr(fileName);
        if (fd.Name == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        } // if

        fd.NameLen = strlen(fd.Name);
        char* s = strrchr(fd.Name, '.');
        if (s != NULL)
            fd.Ext = s + 1; // ".cvspass" is extension in Windows
        else
            fd.Ext = fd.Name + fd.NameLen;

        filePos = new CISOImage::CFilePos;
        if (filePos == NULL)
        {
            Error(IDS_INSUFFICIENT_MEMORY);
            throw FALSE;
        }

        filePos->Type = FS_TYPE_UDF;
        // ExtentOffset is added to help detect which track the file is in
        filePos->Extent = icb->Location + ExtentOffset;
        filePos->Partition = icb->Partition;

        /*
    char u[1024];
    sprintf(u, "CUDF::AddFileDir(): opening Extent: %x", icb->Location);
    TRACE_I(u);
*/

        fd.PluginData = (DWORD_PTR)filePos;

        fd.DosName = NULL;

        fd.Attr = FILE_ATTRIBUTE_READONLY; // vse je defaultne read-only
        fd.Hidden = 0;
        fd.IsOffline = 0;

        if (fileChar & UDF_HIDDEN)
        {
            fd.Attr |= FILE_ATTRIBUTE_HIDDEN;
            fd.Hidden = 1;
        }

        if (fileChar & UDF_DIRECTORY)
        {
            fd.Size = CQuadWord(0, 0);
            fd.Attr |= FILE_ATTRIBUTE_DIRECTORY;
            fd.IsLink = 0;

            UDFTimeStampToFileTime(PVD.RecordingDateandTime, &fd.LastWrite);

            if (!SortByExtDirsAsFiles)
                fd.Ext = fd.Name + fd.NameLen; // adresare nemaji priponu

            if (dir && !dir->AddDir(path, fd, pluginData))
            {
                Error(IDS_ERROR);
                throw FALSE;
            }
        }
        else
        {
            CFileEntry fe;
            if (!MapICB(*icb, &fe))
            {
                SalamanderGeneral->Free(fd.Name);
                throw FALSE;
            }

            fd.Size.Value = fe.InformationLength;
            UDFTimeStampToFileTime(fe.mtime, &fd.LastWrite);

            // soubor
            fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
            if (dir && !dir->AddFile(path, fd, pluginData))
            {
                Error(IDS_ERROR);
                throw FALSE;
            }
        }
    }
    catch (BOOL e)
    {
        delete filePos;

        ret = e;
    }

    return ret;
}

BOOL CUDF::ListDirectory(char* path, int session, CSalamanderDirectoryAbstract* dir,
                         CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE3("CUDF::ListDirectory(%s, %d, , )", path, session);

    Uint8 sector[SECTOR_SIZE];
    CAD rootICB;
    /*
  char u[1024];
  sprintf(u, "CUDF::ListDirectory()");
  TRACE_I(u);
*/
    CDescriptorTag tag;
    // find root ICB
    Uint32 lbNum = LogSector(LVD.FSD.Location, LVD.FSD.Partition);
    // This while should no longer be needed since we use LVD.FSD
    do
    {
        if (!ReadBlockLog(lbNum, 1, sector))
        {
            Error(IDS_ERROR_LISTING_IMAGE, FALSE, lbNum);
            return ERR_TERMINATE;
        }

        ReadDescriptorTag(sector, &tag);
        if (UDF_TAGID_FSD == tag.ID)
        { // File Set Descriptor
            LongAD(sector + 400, &rootICB);
            break;
        }

        lbNum++;
    } while (lbNum < PD.Length);

    // Sanity checks.
    if (UDF_TAGID_FSD != tag.ID)
        return Error(IDS_UDF_INVALID_TAG_ID);

    // Find root dir
    CAD fileICB;
    CICBTag icbTag;

    if (!MapICB(rootICB, &icbTag, &fileICB))
        return Error(IDS_UDF_BAD_ROOT_ICB);
    if (icbTag.FileType != 4)
        return Error(IDS_UDF_BAD_ROOT_ICB); // Root dir should be dir

    return ScanDir(fileICB, path, dir, pluginData) != ERR_TERMINATE;
}

int CUDF::ScanDir(CUDF::CAD dirICB, char* path,
                  CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    char fileName[MAX_UDF_FILE_NAME_LEN + 1];
    Uint8 directory[2 * SECTOR_SIZE];
    Uint32 lbNum;
    Uint8 fileChar;
    unsigned int p;

    // Scan dir for ICB of file
    lbNum = LogSector(dirICB.Location, dirICB.Partition);
    if (!ReadBlockPhys(lbNum, 2, directory))
    {
        Error(IDS_ERROR_LISTING_IMAGE, FALSE, lbNum);
        // cteme-li z root (lbNum == 2), vratime ERR_CONTINUE, abychom se mohli vnorit do prazdneho image
        // snazime se chovat konzistentne z pohledu uzivatele
        // TODO: lepsi detekce toho, zda je cteny sector korenovy
        return lbNum == 2 ? ERR_CONTINUE : ERR_TERMINATE;
    }

    int ret = ERR_OK;
    CDescriptorTag tag;
    p = dirICB.Offset;
    dirICB.Length += p;
    while (p < dirICB.Length && ret != ERR_TERMINATE)
    {
        if (p > SECTOR_SIZE)
        {
            ++lbNum;
            p -= SECTOR_SIZE;
            dirICB.Length -= SECTOR_SIZE;
            if (!ReadBlockPhys(lbNum, 2, directory))
            {
                Error(IDS_ERROR_LISTING_IMAGE, FALSE, lbNum);
                return ERR_TERMINATE;
            }
        } // if

        ReadDescriptorTag(directory + p, &tag);
        if (UDF_TAGID_FID == tag.ID)
        {
            CAD icb;
            p += ReadFileIdentifier(directory + p, &fileChar, fileName, &icb);

            if (!(fileChar & UDF_PARENT))
            {
                if (!AddFileDir(path, fileName, fileChar, &icb, dir, pluginData))
                    return ERR_TERMINATE;

                if (fileChar & UDF_DIRECTORY)
                {
                    char newPath[2 * MAX_PATH];
                    ZeroMemory(newPath, 2 * MAX_PATH);
                    strcpy(newPath, path);
                    strcat(newPath, "\\");
                    strcat(newPath, fileName);

                    CAD fileICB;
                    CICBTag icbTag;
                    // zanorit, kdyz je vse OK
                    if (MapICB(icb, &icbTag, &fileICB) && ret == ERR_OK)
                    {
                        ret = ScanDir(fileICB, newPath, dir, pluginData);
                        // kdyz se vynorime s ukoncovaci chybou, pokracovat ve zpracovani, co to pujde
                        if (ret == ERR_TERMINATE)
                            ret = ERR_CONTINUE;
                    }
                }
            }
        }
        else
        {
            Error(IDS_UDF_INVALID_TAG_ID);
            return ERR_CONTINUE;
        } // if
    }     // while

    return ret;
}

int CUDF::UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                     const char* nameInArc, const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CUDF::UnpackFile( , %s, %s, %s, %p, %u, %d)", srcPath, path, nameInArc, fileData, silent, toSkip);

    // TODO: vybalovani podle alokacni strategie 4096
    // pokud je soubor sekvence allocation descriptoru, pak ho nedokazeme vybalit, protoze se musi sledovat,
    // kam odkazuji alloc. descriptory a pouzit ty spravne bloky (nemame image na otestovani,
    // neimplementujeme, snad casem)

    if (fileData == NULL)
        return UNPACK_ERROR;

    BOOL ret = UNPACK_OK;
    BYTE* sector = NULL;
    BYTE* fileEntry = NULL;
    try
    {
        fileEntry = new BYTE[SECTOR_SIZE];
        if (!fileEntry)
        {
            Error(IDS_INSUFFICIENT_MEMORY, silent == 1);
            throw UNPACK_CANCEL;
        }

        CISOImage::CFilePos* fp = (CISOImage::CFilePos*)fileData->PluginData;
        // read File Entry
        CDescriptorTag tag;
        DWORD extent = fp->Extent - ExtentOffset; // ExtentOffset was added to detect track #
        DWORD lbNum = LogSector(extent, fp->Partition);
        /*
    char u[1024];
    sprintf(u, "CUDF::UnpackFile(): Extent: 0x%X, lbNum: 0x%X", fp->Extent, lbNum);
    TRACE_I(u);
*/
        if (!ReadBlockLog(lbNum, 1, fileEntry))
        {
            delete[] fileEntry;
            Error(IDS_ERROR_READING_SECTOR, silent == 1, lbNum);
            throw UNPACK_ERROR;
        }
        ReadDescriptorTag(fileEntry, &tag);

        if ((UDF_TAGID_FENTRY != tag.ID) && (UDF_TAGID_EXTFENTRY != tag.ID))
        {
            delete[] fileEntry;
            Error(IDS_UDF_INVALID_TAG_ID, silent == 1);
            throw UNPACK_ERROR;
        }

        CICBTag icbTag;
        CAD icbs[32]; // Each ICB can describe at most 1GB-1B
        // -> 9 ICB's should be enough for a DVD9, but we are paranoic

        int nicbs = ReadFileEntry(fileEntry, UDF_TAGID_EXTFENTRY == tag.ID, fp->Partition, &icbTag, icbs, sizeof(icbs) / sizeof(CAD));

        // No longer needed
        delete[] fileEntry;
        fileEntry = NULL;

        // Verify that the icbs describe as many bytes as needed by the file
        ULONGLONG acc = 0;
        if (-1 == nicbs)
        {
            // File data is inlined in file entry
            nicbs = 1;
            icbs[0].Location = lbNum;
        }
        int i;
        for (i = 0; i < nicbs; i++)
        {
            if (UDF_EXT_ALLOCATED != icbs[i].Flags)
                continue;
            acc += icbs[i].Length;
        }
        if (acc != fileData->Size.Value)
        {
            Error(IDS_UDF_ICB_AND_FILESIZE_MISMATCH, silent == 1, fileData->Name);
            throw UNPACK_ERROR;
        }

        if (icbTag.Strategy != 4)
        {
            Error(IDS_UDF_UNKNOWN_ALLOCATION_STRATEGY, silent == 1, icbTag.Strategy);
            throw UNPACK_ERROR;
        }

        // unpack file
        sector = new BYTE[SECTOR_SIZE];
        if (!sector)
        {
            Error(IDS_INSUFFICIENT_MEMORY, silent == 1);
            throw UNPACK_CANCEL;
        }

        char name[MAX_PATH];
        strncpy_s(name, path, _TRUNCATE);
        if (!SalamanderGeneral->SalPathAppend(name, fileData->Name, MAX_PATH))
        {
            Error(IDS_ERR_TOO_LONG_NAME, silent == 1);
            throw UNPACK_ERROR;
        }

        char fileInfo[100];
        FILETIME ft = fileData->LastWrite;
        GetInfo(fileInfo, &ft, fileData->Size);
        DWORD attrs = fileData->Attr;

        HANDLE hFile = SalamanderSafeFile->SafeFileCreate(name, GENERIC_WRITE, FILE_SHARE_READ, attrs, FALSE,
                                                          SalamanderGeneral->GetMainWindowHWND(), nameInArc, fileInfo,
                                                          &silent, TRUE, &toSkip, NULL, 0, NULL, NULL);

        CBufferedFile file(hFile, GENERIC_WRITE);
        // set file time
        file.SetFileTime(&ft, &ft, &ft);

        // celkova operace muze pokracovat dal. pouze skip
        if (toSkip)
            throw UNPACK_ERROR;

        // celkova operace nemuze pokracovat dal. cancel
        if (hFile == INVALID_HANDLE_VALUE)
            throw UNPACK_CANCEL;

        BOOL bFileComplete = TRUE;

        // Loop over all ICB's
        CAD* picb = icbs;
        for (; (nicbs-- > 0) && bFileComplete; picb++)
        {
            if (UDF_EXT_ALLOCATED != picb->Flags)
                continue; // Skip unused blocks

            // ICB & data can be located in different tracks
            BYTE track = Image->GetTrackFromExtent(picb->Location);
            DWORD extentOfs = Image->GetTrack(track)->ExtentOffset;

            // je treba otevrit track, ze ktereho budeme cist (nastaveni parametru tracku)
            if (!Image->OpenTrack(track))
            {
                Error(IDS_CANT_OPEN_TRACK, silent == 1, track);
                throw UNPACK_ERROR;
            }

            DWORD block = picb->Location - extentOfs; // pouzit extent z tracku, kde je soubor ulozen
            DWORD remain = picb->Length;

            /*
      char u[1024];
      sprintf(u, "CUDF::UnpackFile(): opening Extent: 0x%X", block);
      TRACE_I(u);
*/

            DWORD nbytes = SECTOR_SIZE;
            while (remain > 0)
            {
                if (remain < nbytes)
                    nbytes = remain;

                if (!ReadBlockPhys(block, 1, sector))
                {
                    if (silent == 0)
                    {
                        char error[1024];
                        sprintf(error, LoadStr(IDS_ERROR_READING_SECTOR), block);
                        int userAction = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                                        fileData->Name, error, LoadStr(IDS_READERROR));

                        switch (userAction)
                        {
                        case DIALOG_CANCEL:
                            ret = UNPACK_CANCEL;
                            break;
                        case DIALOG_SKIP:
                            ret = UNPACK_ERROR;
                            break;
                        case DIALOG_SKIPALL:
                            ret = UNPACK_ERROR;
                            silent = 1;
                            break;
                        }
                    }
                    bFileComplete = FALSE;
                    break;
                }

                if (!salamander->ProgressAddSize(nbytes, TRUE)) // delayedPaint==TRUE, to make things faster
                {
                    salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING_OPERATION), FALSE);
                    salamander->ProgressEnableCancel(FALSE);

                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break; // preruseni akce
                }

                ULONG written;
                // picb->Offset is nonzero only for small (less than a sector) files inlined within File Entry
                if (!file.Write(sector + picb->Offset, nbytes, &written, name, NULL))
                {
                    // Error message was already displayed by SafeWriteFile()
                    ret = UNPACK_CANCEL;
                    bFileComplete = FALSE;
                    break;
                }
                remain -= nbytes;
                block++;
            } // while (remain > 0)
        }     // while over all ICB's

        //    sprintf(u, "CISOImage::UnpackFile(): ret: %d, bFileComplete: %d", ret, bFileComplete);
        //    TRACE_I(u);

        if (!file.Close(name, NULL))
        {
            // Flushing cache may fail
            ret = UNPACK_CANCEL;
            bFileComplete = FALSE;
        }

        if (!bFileComplete)
        {
            // protoze je vytvoren s read-only atributem, musime R attribut
            // shodit, aby sel soubor smazat
            attrs &= ~FILE_ATTRIBUTE_READONLY;
            if (!SetFileAttributes(name, attrs))
                Error(LoadStr(IDS_CANT_SET_ATTRS), GetLastError(), silent == 1);

            // user zrusil operaci
            // smazat po sobe neuplny soubor
            if (!DeleteFile(name))
                Error(LoadStr(IDS_CANT_DELETE_TEMP_FILE), GetLastError(), silent == 1);
        }
        else
            SetFileAttrs(name, attrs, silent == 1);
    }
    catch (int e)
    {
        ret = e;
    }

    delete[] sector;

    return ret;
}

BOOL CUDF::DumpInfo(FILE* outStream)
{
    CALL_STACK_MESSAGE1("CUDF::DumpInfo( )");

    char buffer[256], *s;

    // zobrazit info z PVD
    DecodeOSTACompressed((BYTE*)PVD.VolumeIdentifier, 32, buffer);
    fprintf(outStream, "    Volume:             %s\n", buffer);
    DecodeOSTACompressed((BYTE*)PVD.VolumeSetIdentifier, 128, buffer);
    fprintf(outStream, "    Volume Set:         %s\n", buffer);
    s = ViewerStrNcpy((char*)PVD.ApplicationIdentifier.Identifier, 23);
    if (*s)
        fprintf(outStream, "    Application:        %s\n", s);
    s = ViewerStrNcpy((char*)PVD.ImplementationIdentifier.Identifier, 23);
    if (*s)
        fprintf(outStream, "    Implementation:     %s\n", s);
    //  fprintf(outStream, "\n");

    SYSTEMTIME st;
    UDFTimeStampToSystemTime(PVD.RecordingDateandTime, &st);
    if (st.wYear != 0)
        fprintf(outStream, "    Recording Date:     %s\n", ViewerPrintSystemTime(&st));

    return TRUE;
}

int CUDF::ReadVAT(BYTE fileEntry[], bool bEFE, BYTE** vat, DWORD* vatLength, BOOL quiet)
{
    CALL_STACK_MESSAGE1("CUDF::ReadVAT(, , , )");

    CFileEntry fe;
    ReadFileEntry(fileEntry, bEFE, &fe);

    Uint16 entries = fe.ICBTag.Entries;
    Uint16 strategy = fe.ICBTag.Strategy;
    Uint32 dataLength = fe.L_AD;
    Uint8 addrType = fe.ICBTag.Flags & 0x03;

    Uint32 directEntries, indirectEntries;
    switch (strategy)
    {
    case 0:
    case 1:
    case 2:
    case 3:
        Error(IDS_UDF_UNKNOWN_ALLOCATION_STRATEGY, quiet, strategy);
        return ERR_TERMINATE;

    case 4:
        directEntries = dataLength;
        indirectEntries = 0;
        break;

    case 4096:
        directEntries = fe.ICBTag.StrategyParametr;
        indirectEntries = 1;
        Error(IDS_UDF_UNKNOWN_ALLOCATION_STRATEGY, quiet, strategy);
        return ERR_TERMINATE;

    default:
        Error(IDS_UDF_UNKNOWN_ALLOCATION_STRATEGY, quiet, strategy);
        return ERR_TERMINATE;
    } // switch

    DWORD length = (int)fe.InformationLength;
    *vatLength = length;
    *vat = new BYTE[length];
    if (*vat == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY, quiet);
        return ERR_TERMINATE;
    }

    DWORD offset = 176 + fe.L_EA, end;

    if (bEFE)
        offset += 40;
    end = offset + fe.L_AD;

    DWORD o = 0;
    while (offset < end)
    {
        int ret = ReadAllocDesc(fileEntry + offset, &fe, *vat, &o);

        if (ret < 0)
        {
            delete[] *vat;
            *vat = NULL;
            return ERR_CONTINUE;
        }
        offset += ret;
    }

    return ERR_OK;
}

int CUDF::CheckForVAT(BYTE sector[], bool bEFE, CPartitionMapping* pm, BOOL quiet)
{
    CALL_STACK_MESSAGE1("CUDF::CheckForVAT(, , )");

    CICBTag icbTag;
    DWORD vatLen;
    BYTE* vat = NULL;
    int ret;

    ReadICBTag(sector + 16, &icbTag);
    if (icbTag.FileType == 248)
    {
        // VAT in UDF 2.00+
        CFileEntry fe;
        ReadFileEntry(sector, bEFE, &fe);
        if ((ret = ReadVAT(sector, bEFE, &vat, &vatLen, quiet)) != ERR_OK)
            return ret;

        // decode VAT header
        WORD l_hd = GET_WORD(vat, 0);

        pm->Entries = (fe.InformationLength - l_hd) / 4; // definition
        pm->Length = vatLen;
        pm->Translation = (DWORD*)(vat + l_hd);
        pm->VAT = vat;
    }
    else if (icbTag.FileType == 0)
    {
        if ((ret = ReadVAT(sector, bEFE, &vat, &vatLen, quiet)) != ERR_OK)
            return ret;

        // check the old UDF 1.50 VAT
        Uint32 entries = (vatLen - 36) / 4; // definition

        if (entries * 4 >= vatLen)
        {
            delete[] vat;
            return ERR_CONTINUE;
        }

        CEntityID entityID;
        ReadEntityID(vat + (entries * 4), &entityID);
        if (strncmp(entityID.Identifier, "*UDF Virtual Alloc Tbl", 22) != 0)
        {
            delete[] vat;
            return ERR_CONTINUE;
        }

        // VAT in UDF 1.50
        pm->Entries = entries;
        pm->Length = vatLen;
        pm->Translation = (DWORD*)vat;
        pm->VAT = vat;
    }
    else
        return ERR_CONTINUE;

    return ERR_OK;
}

BOOL CUDF::ReadSupportingTables(BOOL quiet)
{
    CALL_STACK_MESSAGE1("CUDF::ReadSupportingTables( )");

    DWORD vatLoc, lastVatLoc;
    BYTE sector[SECTOR_SIZE];
    CDescriptorTag tag;
    BOOL ret = TRUE;
    int vatRet;

    int i;
    for (i = 0; i < PartitionMaps.Count; i++)
    {
        switch (PartitionMaps[i]->Type)
        {
        case mapPhysical:
            break;

        case mapVirtual:
            vatLoc = PD.Length - ExtentOffset;
            /*
        {
        char u[1024];
        sprintf(u, "CUDF::ReadSupportingTables(): vatLoc: 0x%X", vatLoc);
        TRACE_I(u);
        }
*/
            lastVatLoc = vatLoc + 7;
            vatLoc -= 7;

            vatRet = ERR_CONTINUE;
            do
            {
                if (!ReadBlockPhys(vatLoc, 1, sector))
                {
                    LONGLONG size;

                    size = Image->GetCurrentTrackEnd(TrackTryingToOpen);
                    if (size != 0xFFFFFFFFFFFFFFFF)
                    {
                        DWORD nSectors;

                        nSectors = (DWORD)((size) / SECTOR_SIZE) - PD.Start;
                        if (nSectors < vatLoc - 7)
                        {
                            vatLoc = nSectors - 7;
                            lastVatLoc = nSectors;
                            continue;
                        }
                    }

                    return Error(IDS_UDF_ERROR_READING_VAT, quiet, vatLoc);
                }

                ReadDescriptorTag(sector, &tag);
                if ((UDF_TAGID_FENTRY == tag.ID) || (UDF_TAGID_EXTFENTRY == tag.ID))
                {
                    vatRet = CheckForVAT(sector, UDF_TAGID_EXTFENTRY == tag.ID, &PartitionMapping, quiet);
                    if (vatRet == ERR_OK)
                        ret = TRUE;
                    if (vatRet == ERR_TERMINATE)
                        break;
                }
                vatLoc++;
            } while (vatLoc < lastVatLoc && vatRet == ERR_CONTINUE);

            if (vatRet == ERR_CONTINUE)
                Error(IDS_UDF_MISSING_VAT, quiet);
            break;

        case mapSparable:
            break;

        case mapMetadata:
            break;
        } // switch
    }     // loop over all partitions

    return ret;
}
