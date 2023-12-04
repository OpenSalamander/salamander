// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "isoimage.h"

#include "uniso.h"
#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

#include "iso9660.h"

// El Torito specification

// length of each entry in the boot catalog in bytes
#define ENTRY_LENGTH 0x20

// validation entry
struct etValidationEntry
{
    BYTE ID;
    BYTE PlatformID; // 0 - 0x86
                     // 1 - Power MAC
                     // 2 - Mac
    WORD Reserved;   // should be 0
    BYTE IDstring[0x18];
    WORD CheckSum;   // should be zero
    BYTE KeyByte[2]; // should be 0x55 0xAA
};

// default entry
struct etDefaultEntry
{
    BYTE BootIndicator; // 88 - Bootable
                        // 00 - Not bootable
    BYTE BootMedia;     // 0 - No emulation
                        // 1 - 1.2 meg diskette
                        // 2 - 1.44 meg diskette
                        // 3 - 2.88 meg diskette
                        // 4 - Hard disk (drive 80)
    WORD LoadSegment;
    BYTE SystemType;
    BYTE Unused;
    WORD SectorCount;
    DWORD LoadRBA;
};

// section header entry
struct etSectionHeaderEntry
{
    BYTE HeaderIndicator; // 90 - header, more headers follow
                          // 91 - final header
    BYTE PlatformID;      // 0 - 80x86
                          // 1 - Power MAC
                          // 2 - Mac
    WORD NextHeadersCount;
    BYTE IDstring[0x1C];
};

// section entry
struct etSectionEntry
{
    BYTE BootIndicator; // 88 - Bootlable
                        // 00 - Not bootable
    BYTE BootMedia;
    // Bits 0-3 count as follows
    //  0 No Emulation
    //  1 1.2 meg diskette
    //  2 1.44 meg diskette
    //  3 2.88 meg diskette
    //  4 Hard Disk (drive 80)

    // bit 4 - Reserved, must be 0
    // bit 5 - Continuation Entry Follows
    // bit 6 - Image contains an ATAPI driver
    // bit 7 - Image contains SCSI drivers

    WORD LoadSegment;
    BYTE SystemType;
    BYTE Unused;
    WORD SectorCount;
    DWORD LoadRBA;
};

//
BOOL CISO9660::FillBootRecordInfoElTorito(CBootRecordInfo* bri, BYTE bootMedia, WORD sectorCount, DWORD loadRBA)
{
    CALL_STACK_MESSAGE4("CISO9660::FillBootRecordInfoElTorito(, %u, %u, %u)", bootMedia, sectorCount, loadRBA);

    DWORD bootRecordLength;

    //  bri->SectorCount = sectorCount;
    bri->LoadRBA = loadRBA - ExtentOffset;

    switch (bootMedia & 0x0F)
    {
    case 0:
        bri->Type = biNoEmul;
        break;
    case 1:
        bri->Type = bi120;
        break;
    case 2:
        bri->Type = bi144;
        break;
    case 3:
        bri->Type = bi288;
        break;
    case 4:
        bri->Type = biHDD;
        break;
    } // switch

    switch (bri->Type)
    {
    case biNoEmul:
        bootRecordLength = sectorCount * 0x200;
        break;

    case bi120:
        bootRecordLength = 0x50 * 0x02 * 0x0F * 0x200;
        break;

    case bi144:
        bootRecordLength = 0x50 * 0x02 * 0x12 * 0x200;
        break;

    case bi288:
        bootRecordLength = 0x50 * 0x02 * 0x24 * 0x200;
        break;

    case biHDD:
    {
        // read MBR
        char mbr[0x200];
        if (Image->ReadBlock(loadRBA - ExtentOffset, 0x200, &mbr) != 0x200)
            return FALSE;

        // is it MBR ?
        if (mbr[510] != 0x55 || mbr[511] != 0xAA)
            return FALSE;

        DWORD hddSectorCount; // pocet sektoru v prvni partition
        // v ElTorito HDD boot discich nesmi byt vice nez jedna partition
        memcpy(&hddSectorCount, mbr + 0x1CA, sizeof(DWORD));

        bootRecordLength = hddSectorCount * 0x200;
    }
    break;
    } // switch

    bri->Length = bootRecordLength;

    return TRUE;
}

BOOL CISO9660::ReadElTorito(BYTE* catalog, DWORD size, BOOL quiet)
{
    CALL_STACK_MESSAGE3("CISO9660::ReadElTorito(, %u, %d)", size, quiet);

    // read the booting catalog
    DWORD offset = 0;

    // validation entry should be first
    etValidationEntry ve;
    memcpy(&ve, catalog + offset, sizeof(ve));
    offset += ENTRY_LENGTH;

    // check if it is validation entry
    if (ve.ID != 01 || ve.Reserved != 0x00 || ve.KeyByte[0] != 0x55 || ve.KeyByte[1] != 0xAA)
        return FALSE;

    // next is the initial/default entry
    etDefaultEntry de;
    memcpy(&de, catalog + offset, sizeof(de));
    offset += ENTRY_LENGTH;

    if (de.BootIndicator == 0x88)
    {
        // it is bootable
        BootRecordInfo = new CBootRecordInfo;
        if (BootRecordInfo == NULL)
        {
            return Error(IDS_INSUFFICIENT_MEMORY, quiet);
        }

        FillBootRecordInfoElTorito(BootRecordInfo, de.BootMedia, de.SectorCount, de.LoadRBA);

        return TRUE;
    }

    // it is valid entry, go on...
    //  BYTE entryBuffer[ENTRY_LENGTH];

    while (offset < size)
    {

        //    switch () {
        //    } // switch

        offset += ENTRY_LENGTH;
    } // whiles

    return FALSE;
}
