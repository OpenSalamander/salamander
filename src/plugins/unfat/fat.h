// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2004-2023 Open Salamander Authors
// Copyright (c) 2010-2023 Milan Kase <manison@manison.cz>
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#pragma pack(push, enter_include_fat) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(1)

struct CHSINT13
{
    BYTE H;
    BYTE SCh;
    BYTE Cl;
};

struct CPartitionEntry
{
    BYTE Active;
    CHSINT13 StartCHS;
    BYTE Type;
    CHSINT13 EndCHS;
    DWORD StartLBA;
    DWORD SectorSize;
};

#define MBR_PARTITION_TABLE_OFFSET 446

enum PARTITIONTYPE
{
    PARTITIONTYPE_EMPTY = 0x00,
    PARTITIONTYPE_FAT12 = 0x01,
    PARTITIONTYPE_FAT16 = 0x04,
    PARTITIONTYPE_EXTENDED = 0x05,
    PARTITIONTYPE_FAT16_32M = 0x06,
    PARTITIONTYPE_NTFS = 0x07,
    PARTITIONTYPE_FAT32 = 0x0B,
    PARTITIONTYPE_EXTENDED_LBA = 0x0F,
    PARTITIONTYPE_DIAG = 0x12,
    PARTITIONTYPE_FAT32_LBA = 0x0C
};

struct CLabelBlk
{
    BYTE DrvNum;        // 36
    BYTE Reserved1;     // 37 Reserved (used by Windows NT)
    BYTE BootSig;       // 38 Extended boot signature
    BYTE VolID[4];      // 39 Volume serial number
    char VolLab[11];    // 43 Disk label
    char FilSysType[8]; // 54 Fat type
};

struct CFAT1216
{
    CLabelBlk LabelBlk;
};

struct CFAT32
{
    DWORD FATSz32;     // 36 Count of sectors occupied by one FAT
    WORD ExtFlags;     // 40 Extension flags
    WORD FSVer;        // 42 Version number of the FAT32 volume
    DWORD RootClus;    // 44 Start cluster of root dir
    WORD FSInfo;       // 48 Sector number of FSINFO structure in the reserved area of the FAT32 volume
    WORD BkBootSec;    // 50 Back up boot sector
    BYTE Reserved[12]; // 52 Reserved for future expansion
    CLabelBlk LabelBlk;
};

struct CBootsector
{
    BYTE JmpBoot[3]; // 0  Jump instruction to boot code
    char OEMName[8]; // 3  OEM name & version
    WORD BytsPerSec; // 11 Count of bytes per sector
    BYTE SecPerClus; // 13 Number of sectors per allocation unit
    WORD RsvdSecCnt; // 14 Number of reserved (boot) sectors
    BYTE NumFATs;    // 16 Number of FAT tables
    WORD RootEntCnt; // 17 Number of directory slots
    WORD TotSec16;   // 19 Total sectors on disk
    BYTE Media;      // 21 Media descriptor=first byte of FAT
    WORD FATSz16;    // 22 Sectors in FAT
    WORD SecPerTrk;  // 24 Sectors per track
    WORD NumHeads;   // 26 Number of heads
    DWORD HiddSec;   // 28 Count of hidden sectors
    DWORD TotSec32;  // 32 Total count of sectors on the volume
};

enum CFATType
{
    fteFAT12,
    fteFAT16,
    fteFAT32,
    fteFATUnknow
};

struct CDirEntryShort
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

struct CDirEntryLong
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

#pragma pack(pop, enter_include_fat)

union CDirEntry
{
    CDirEntryShort Short;
    CDirEntryLong Long;
};

enum CAllocWholeFileEnum
{
    awfNeededTest,
    awfEnabled,
    awfDisabled
};

class CFATImage
{
protected:
    SAFE_FILE File;

    CBootsector BS;
    CFATType FATType;
    CFAT1216 FAT1216;
    CFAT32 FAT32;
    DWORD FirstRootDirSecNum;
    DWORD RootDirSectors;
    DWORD FATSz;
    DWORD TotSec;
    DWORD FirstDataSec;
    DWORD DataSec;
    DWORD CountOfClusters;
    CQuadWord VolumeStart;

public:
    CFATImage();
    ~CFATImage();

    // otevre FAT image 'fileName' a nacte
    BOOL Open(const char* fileName, BOOL quiet, HWND hParent);

    BOOL ListImage(CSalamanderDirectoryAbstract* dir, HWND hParent);

    BOOL UnpackFile(CSalamanderForOperationsAbstract* salamander, const char* archiveName,
                    const char* nameInArchive, const CFileData* fileData,
                    const char* targetDir, DWORD* silentMask,
                    BOOL allowSkip, BOOL* skipped, char* skipPath, int skipPathMax,
                    HWND hParent, CAllocWholeFileEnum* allocWholeFileOnStart);

protected:
    void Close();

    BOOL AddDirectory(char* root, TDirectArray<DWORD>* fat,
                      CSalamanderDirectoryAbstract* dir, DWORD sector,
                      HWND hParent);

    // projde 1. FAT tabulku a do pole 'fat' prida retez zacinajici na clusteru 'cluster'
    // uklada primo hodnoty z fat, ktere je nasledne treba prelozit na realny sektor

    BOOL LoadFAT(DWORD cluster, TDirectArray<DWORD>* fat, HWND hParent,
                 DWORD buttons, DWORD* pressedButton, DWORD* silentMask);

    static BOOL PickFATVolume(const CPartitionEntry* partitionTable, DWORD numEntries,
                              CQuadWord* volumeStart);
};
