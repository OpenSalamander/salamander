// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push, volume_h)
#pragma pack(1)

struct GENERIC_BOOT_SECTOR
{
    BYTE Jump[3];
    BYTE OemName[8];
    WORD BytesPerSector;
    BYTE SectorsPerCluster;
    BYTE DontCare[512 - 14];
};

struct NTFS_BOOT_SECTOR
{
    BYTE Jump[3];
    BYTE OemName[8]; // "NTFS    "
    WORD BytesPerSector;
    BYTE SectorsPerCluster;
    BYTE Unused1[7];
    BYTE MediaDescriptor;
    BYTE Unused2[2];
    WORD SectorsPerTrack;
    WORD NumberOfHeads;
    BYTE Unused3[12];
    QWORD NumberOfSectors;
    QWORD MFTStartLCN;
    QWORD MFTMirrStartLCN;
    signed char ClustersPerMFTRecord;
    BYTE Unused4[3];
    signed char ClustersPerIndexRecord;
    BYTE Unused5[3];
    QWORD VolumeSerial;
};

struct FAT_BOOT_SECTOR
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
    DWORD FATSz32;   // 36 Count of sectors occupied by one FAT
    WORD ExtFlags;   // 40 Extension flags
    WORD FSVer;      // 42 Version number of the FAT32 volume
    DWORD RootClus;  // 44 Start cluster of root dir
    WORD FSInfo;     // 48 Sector number of FSINFO structure in the reserved area of the FAT32 volume
    WORD BkBootSec;  // 50 Back up boot sector
};

struct EXFAT_BOOT_SECTOR
{
    BYTE JmpBoot[3];           // 0xEB7690
    char OEMName[8];           // "EXFAT   "
    BYTE MustBeZero[53];       // Must be 0x00
    QWORD PartitionOffset;     // Sector Address
    QWORD VolumeLength;        // Size of total volume in sectors
    DWORD FATOffset;           // Sector address of 1st FAT
    DWORD FATLength;           // Size of FAT in Sectors
    DWORD ClusterHeapOffset;   // Sector address of the Data Region
    DWORD ClusterCount;        // Number of clusters in the Cluster Heap
    DWORD RootDirFirstCluster; // Cluster address of the Root Directory
    DWORD VolumeSerialNumber;  // Volume Serial Number
    WORD FileSystemRevision;   // VV.MM (01.00 for this release)
    WORD VolumeFlags;
    BYTE BytesPerSectorExp;    // Exponent of power of 2. (min 2^9=512 for 512 bytes sector size)
    BYTE SectorsPerClusterExp; // Exponent of power of 2. (min 2^0=1 for 512 bytes cluster size)
    BYTE NumberOfFATs;         // Either 1 or 2, 2 if TexFAT is in use.
    BYTE DriveSelect;
    BYTE PercentInUse; // Percentage of Heap in use
    BYTE Reserved[7];
    BYTE BootCode[390];
    WORD BootSignature; // 0xAA55
    BYTE Padding[512];  // If the sector is larger than 512 bytes, extra padding may exist beyond the signature
};

typedef struct _DISKIO
{
    DWORD diStartSector; // sector number to start at
    WORD diSectors;      // number of sectors
    DWORD diBuffer;      // address of buffer
} DISKIO, *PDISKIO;

#pragma pack(pop, volume_h)

enum CVolumeType
{
    vtUnknown,
    vtFAT,
    vtNTFS,
    vtExFAT
};

enum CFATType
{
    ftUnknown,
    ftFAT12,
    ftFAT16,
    ftFAT32
};

template <typename CHAR>
class CVolume
{
public:
    CVolume();
    BOOL Open(const CHAR* rootPath);
    BOOL ReadSectors(void* buffer, QWORD sector, QWORD num);
    BOOL ReadClusters(void* buffer, QWORD cluster, QWORD num);
    BOOL IsOpen() { return HVolume != INVALID_HANDLE_VALUE; }
    void Close();

    // boot sector(s)
    union
    {
        GENERIC_BOOT_SECTOR Boot;
        FAT_BOOT_SECTOR FATBoot;
        NTFS_BOOT_SECTOR NTFSBoot;
        EXFAT_BOOT_SECTOR ExFATBoot;
    };

    // shared variables for all file systems
    CVolumeType Type;
    DWORD BytesPerSector;
    DWORD SectorsPerCluster;
    DWORD BytesPerCluster;
    BOOL IsImage; // TRUE when working on disk image, FALSE for physical disks

    // valid for FAT volumes
    DWORD FAT_FirstRootDirSecNum;
    DWORD FAT_RootDirSectors;
    DWORD FAT_FATSize;
    DWORD FAT_TotSec;
    DWORD FAT_FirstDataSec;
    DWORD FAT_DataSec;
    DWORD FAT_CountOfClusters;
    CFATType FAT_FATType;

    // valid for NTFS volumes
    // NTFS version, read from $Volume file during CMFTSnapshot::Update
    BYTE NTFS_VerMajor;
    BYTE NTFS_VerMinor;

    static const CHAR* STRING_VOLUME_NT;
    static const CHAR* STRING_VOLUME_95;
    static const CHAR* STRING_ROOT;
    static const CHAR* STRING_FAT32;
    static const CHAR CHAR_A;
    static const CHAR CHAR_BSLASH;
    static const CHAR CHAR_COLON;

private:
    HANDLE HVolume;
    int IVolume; // order number of volume for Win98
};

template <typename CHAR>
CVolume<CHAR>::CVolume()
{
    HVolume = INVALID_HANDLE_VALUE;
    ZeroMemory(&Boot, sizeof(Boot));
    // Shared
    Type = vtUnknown;
    BytesPerSector = 0;
    SectorsPerCluster = 0;
    BytesPerCluster = 0;
    IsImage = FALSE;
    // FAT
    FAT_FirstRootDirSecNum = 0;
    FAT_RootDirSectors = 0;
    FAT_FATSize = 0;
    FAT_TotSec = 0;
    FAT_FirstDataSec = 0;
    FAT_DataSec = 0;
    FAT_CountOfClusters = 0;
    FAT_FATType = ftUnknown;
    // NTFS
    NTFS_VerMajor = 0;
    NTFS_VerMinor = 0;
}

// rootPath can have different contents:
//    C:\ - path to a volume root, must be terminated by backslash
//    \\?\Volume{GUID}\ - path to physical volume, must be terminated by backslash (not supported on W95/98)
//    C:\image.ima - path to a disk image
template <typename CHAR>
BOOL CVolume<CHAR>::Open(const CHAR* rootPath)
{
    CALL_STACK_MESSAGE1("CVolume::Open()");

//#define DEMO_VERSION
#ifdef DEMO_VERSION
    //                                 Year,Month,DayOfWeek,Day,....
    SYSTEMTIME BETA_EXPIRATION_DATE = {2018, 4, 0, 1, 0, 0, 0, 0};
    SYSTEMTIME st;
    GetLocalTime(&st);
    SYSTEMTIME* expire = &BETA_EXPIRATION_DATE;
    if (st.wYear > expire->wYear ||
        (st.wYear == expire->wYear && st.wMonth > expire->wMonth) ||
        (st.wYear == expire->wYear && st.wMonth == expire->wMonth && st.wDay >= expire->wDay))
    {
        String<CHAR>::SysError(IDS_UNDELETE, IDS_LOWMEM);
        return FALSE;
    }
#endif // DEMO_VERSION

    // open volume
    if (rootPath[String<CHAR>::StrLen(rootPath) - 1] == CHAR_BSLASH)
    {
        // volume
        IsImage = FALSE;
        if (IsWindowsNT)
        {
            CHAR diskVolume[MAX_PATH];
            if (rootPath[1] == CHAR_COLON && rootPath[2] == CHAR_BSLASH)
            {
                CHAR guidPath[MAX_PATH];
                if (OS<CHAR>::OS_GetVolumeNameForVolumeMountPointExists() &&
                    OS<CHAR>::OS_GetVolumeNameForVolumeMountPoint(rootPath, guidPath, MAX_PATH))
                {
                    String<CHAR>::StrCpy(diskVolume, guidPath);
                }
                else
                {
                    // Windows NT didn't support GUID paths
                    // for old disk syntax (c:\), we need to transform it to volume path
                    String<CHAR>::StrCpy(diskVolume, STRING_VOLUME_NT);
                    String<CHAR>::StrCat(diskVolume, rootPath);
                }
            }
            else
            {
                // otherwise try it as it is
                String<CHAR>::StrCpy(diskVolume, rootPath);
            }
            // and remove the backslash at the end
            diskVolume[String<CHAR>::StrLen(diskVolume) - 1] = 0;

            HVolume = OS<CHAR>::OS_CreateFile(diskVolume,
                                              GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                              NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
        }
        else
        {
            IVolume = String<CHAR>::ToUpper(rootPath[0]) - CHAR_A + 1;
            HVolume = OS<CHAR>::OS_CreateFile(STRING_VOLUME_95, 0, 0, NULL, 0,
                                              FILE_FLAG_DELETE_ON_CLOSE, NULL);
        }
    }
    else
    {
        // disk image
        IsImage = TRUE;
        HVolume = OS<CHAR>::OS_CreateFile(rootPath,
                                          GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                          NULL, OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    }

    if (HVolume == INVALID_HANDLE_VALUE)
        return String<CHAR>::SysError(IDS_UNDELETE, IDS_ERROROPENINGVOLUME);

    // read boot sector
    if (!ReadSectors(&Boot, 0, 1))
    {
        String<CHAR>::SysError(IDS_UNDELETE, IDS_ERRORREADINGBOOT);
        Close();
        return FALSE;
    }

    // detect volume file system
    if (!_memicmp(Boot.OemName, "NTFS    ", 8))
    {
        // test volume integrity
        if (NTFSBoot.BytesPerSector == 0 || NTFSBoot.SectorsPerCluster == 0)
        {
            Close();
            return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORVOLUMETYPE);
        }

        Type = vtNTFS;
        BytesPerSector = NTFSBoot.BytesPerSector;
        SectorsPerCluster = NTFSBoot.SectorsPerCluster;
        BytesPerCluster = BytesPerSector * SectorsPerCluster;
    }
    else
    {
        if (!_memicmp(Boot.OemName, "EXFAT   ", 8))
        {
            // test volume integrity
            // Windows 7 allows 64kB/cluster, there are 128kB cluster mentioned in MSDN, so we will not
            // test upper limits for BytesPerSectorExp and SectorsPerClusterExp
            if (ExFATBoot.JmpBoot[0] != 0xEB ||
                ExFATBoot.NumberOfFATs < 1 || ExFATBoot.NumberOfFATs > 2 ||
                ExFATBoot.BytesPerSectorExp < 9 ||
                ExFATBoot.BootSignature != 0xAA55)
            {
                Close();
                return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORVOLUMETYPE);
            }

            Type = vtExFAT;
            BytesPerSector = (1 << ExFATBoot.BytesPerSectorExp);
            SectorsPerCluster = (1 << ExFATBoot.SectorsPerClusterExp);
            BytesPerCluster = BytesPerSector * SectorsPerCluster;
        }
        else
        {
            // test volume integrity
            if ((FATBoot.JmpBoot[0] != 0xEB && FATBoot.JmpBoot[0] != 0xE9) ||
                FATBoot.BytsPerSec == 0 ||
                FATBoot.SecPerClus == 0 ||
                FATBoot.NumFATs == 0 ||
                (FATBoot.TotSec16 == 0 && FATBoot.TotSec32 == 0) ||
                (FATBoot.TotSec16 != 0 && FATBoot.TotSec32 != 0))
            {
                Close();
                return String<CHAR>::Error(IDS_UNDELETE, IDS_ERRORVOLUMETYPE);
            }

            Type = vtFAT;
            BytesPerSector = FATBoot.BytsPerSec;
            SectorsPerCluster = FATBoot.SecPerClus;
            BytesPerCluster = BytesPerSector * SectorsPerCluster;

            // calculate basic FAT specs
            if (FATBoot.FATSz16 != 0)
                FAT_FATSize = FATBoot.FATSz16;
            else
                FAT_FATSize = FATBoot.FATSz32;

            if (FATBoot.TotSec16 != 0)
                FAT_TotSec = FATBoot.TotSec16;
            else
                FAT_TotSec = FATBoot.TotSec32;

            FAT_RootDirSectors = ((FATBoot.RootEntCnt * 32) + (FATBoot.BytsPerSec - 1)) / FATBoot.BytsPerSec;
            FAT_FirstDataSec = FATBoot.RsvdSecCnt + (FATBoot.NumFATs * FAT_FATSize) + FAT_RootDirSectors;
            FAT_DataSec = FAT_TotSec - (FATBoot.RsvdSecCnt + (FATBoot.NumFATs * FAT_FATSize) + FAT_RootDirSectors);
            FAT_CountOfClusters = FAT_DataSec / FATBoot.SecPerClus;

            if (FAT_CountOfClusters < 4085)
                FAT_FATType = ftFAT12;
            else if (FAT_CountOfClusters < 65525)
                FAT_FATType = ftFAT16;
            else
                FAT_FATType = ftFAT32;

            if (FAT_FATType != ftFAT32)
                FAT_FirstRootDirSecNum = FATBoot.RsvdSecCnt + (FATBoot.NumFATs * FATBoot.FATSz16);
            else
                FAT_FirstRootDirSecNum = FAT_FirstDataSec + (FATBoot.RootClus - 2) * FATBoot.SecPerClus;
        }
    }

#ifdef TRACE_ENABLE
    TRACE_IW(L"Opened volume " << CWStr(rootPath).c_str() << L" - Type=" << Type << L" OemName='" << (char)Boot.OemName[0] << (char)Boot.OemName[1] << (char)Boot.OemName[2] << (char)Boot.OemName[3] << (char)Boot.OemName[4] << (char)Boot.OemName[5] << (char)Boot.OemName[6] << (char)Boot.OemName[7] << L"'");
    TRACE_I("BytesPerSector=" << BytesPerSector << " SectorsPerCluster=" << SectorsPerCluster << " BytesPerCluster=" << BytesPerCluster);
    if (Type == vtFAT)
    {
        TRACE_I("FATType=" << FAT_FATType << " FATSize=" << FAT_FATSize << " TotSec=" << FAT_TotSec);
        TRACE_I("RootDirSectors=" << FAT_RootDirSectors << " FAT_FirstDataSec=" << FAT_FirstDataSec);
        TRACE_I("DataSec=" << FAT_DataSec << " CountOfClusters=" << FAT_CountOfClusters);
        TRACE_I("FirstRootDirSecNum=" << FAT_FirstRootDirSecNum);
    }
#endif
    return TRUE;
}

template <typename CHAR>
BOOL CVolume<CHAR>::ReadSectors(void* buffer, QWORD sector, QWORD num)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CVolume::ReadSectors(, , %d)", num);

    if (Type == vtFAT && sector + num > FAT_TotSec)
        TRACE_E("Access beyond end of disk! sector=" << sector << " num=" << num << " TotSec=" << FAT_TotSec);
    if (Type == vtExFAT && sector + num > ExFATBoot.VolumeLength)
        TRACE_E("Access beyond end of disk! sector=" << sector << " num=" << num << " TotSec=" << ExFATBoot.VolumeLength);
    if (Type == vtNTFS && sector + num > NTFSBoot.NumberOfSectors)
        TRACE_E("Access beyond end of disk! sector=" << sector << " num=" << num << " TotSec=" << NTFSBoot.NumberOfSectors);

    DWORD bytesPerSector = BytesPerSector;
    if (Type == vtUnknown)
        bytesPerSector = 512; // reading boot sector

    if (IsWindowsNT || IsImage)
    {
        // seek to the desired sector
        QWORD pos = sector * bytesPerSector;
        if (SetFilePointer(HVolume, LODWORD(pos), (PLONG)PHIDWORD(pos), FILE_BEGIN) == 0xFFFFFFFF &&
            GetLastError() != NO_ERROR)
        {
#ifdef TRACE_ENABLE
            DWORD err = GetLastError();
            TRACE_E("ReadSectors() SetFilePointer failed! sector=" << sector << " num=" << num << " err=" << err);
            SetLastError(err);
#endif
            return FALSE;
        }

        // read the clusters
        DWORD numr;
        QWORD len = num * bytesPerSector;
        if (!ReadFile(HVolume, buffer, (DWORD)len, &numr, NULL) || numr != len)
        {
#ifdef TRACE_ENABLE
            DWORD err = GetLastError();
            TRACE_E("ReadSectors() ReadFile failed! sector=" << sector << " num=" << num << " err=" << err);
            SetLastError(err);
#endif
            return FALSE;
        }
    }
    else
    {
        // Read raw sectors on Win95/98/Me
        //
        // From MSDN:
        // 1. Use DeviceIoControl with VWIN32_DIOC_DOS_INT25 and VWIN32_DIOC_DOS_INT26
        // to issue Absolute Disk Read (Int 25h) and Absolute Disk Write (Int 26h)
        // respectively. This method works for FAT12 and FAT16 volumes. This method is
        // compatible with the retail release version of Windows 95, but will not work
        // on FAT32 volumes.
        // 2. Use DeviceIoControl with VWIN32_DIOC_DOS_DRIVEINFO to issue Ext Absolute
        // Disk Read & Write (Int 21h function 7305h). This method works for FAT12,
        // FAT16, and FAT32 volumes, but is not backward compatible with the retail
        // release version of Windows 95.

        DIOC_REGISTERS reg;
        DWORD cb;
        DWORD ctrlcode;

        DISKIO di;
        di.diStartSector = (DWORD)sector;
        di.diSectors = (WORD)num;
        di.diBuffer = (DWORD)(DWORD_PTR)buffer;

        BOOL w95Retail = (IsWindows95 && !IsWindows95OSR2AndLater); // VWIN32_DIOC_DOS_DRIVEINFO is not supported under W95Retail
        if (w95Retail || (Type == vtFAT && (FAT_FATType == ftFAT12 || FAT_FATType == ftFAT16)))
        {
            reg.reg_EAX = IVolume - 1;
            reg.reg_EBX = (DWORD)(DWORD_PTR)&di;
            reg.reg_ECX = 0xFFFF;
            reg.reg_Flags = 1;
            ctrlcode = VWIN32_DIOC_DOS_INT25;
        }
        else // FAT32 and NTFS
        {
            reg.reg_EAX = 0x7305;
            reg.reg_EBX = (DWORD)(DWORD_PTR)&di;
            reg.reg_ECX = 0xFFFF;
            reg.reg_EDX = IVolume;
            reg.reg_ESI = 0;
            reg.reg_Flags = 1;
            ctrlcode = VWIN32_DIOC_DOS_DRIVEINFO;
        }

        if (!DeviceIoControl(HVolume, ctrlcode, &reg, sizeof(reg), &reg, sizeof(reg), &cb, 0) ||
            (reg.reg_Flags & CARRY_FLAG))
        {
            if (reg.reg_Flags & CARRY_FLAG)
                TRACE_I("ReadSectors: carry flag");
            else
                TRACE_I("ReadSectors: DeviceIoControl failed");
            return FALSE;
        }
    }
    return TRUE;
}

template <typename CHAR>
BOOL CVolume<CHAR>::ReadClusters(void* buffer, QWORD cluster, QWORD num)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CVolume::ReadClusters(, , %d)", num);
    switch (Type)
    {
    case vtFAT:
    {
        if (cluster < 2)
            TRACE_E("ReadClusters: cluster < 2 !!!");
        return ReadSectors(buffer, (cluster - 2) * SectorsPerCluster + FAT_FirstDataSec,
                           num * SectorsPerCluster);
    }

    case vtExFAT:
    {
        if (cluster < 2)
            TRACE_E("ReadClusters: cluster < 2 !!!");
        return ReadSectors(buffer, (cluster - 2) * SectorsPerCluster + ExFATBoot.ClusterHeapOffset,
                           num * SectorsPerCluster);
    }

    case vtNTFS:
    {
        return ReadSectors(buffer, cluster * SectorsPerCluster, num * SectorsPerCluster);
    }

    default:
    {
        TRACE_E("Unknown file system type!");
        return FALSE;
    }
    }
}

template <typename CHAR>
void CVolume<CHAR>::Close()
{
    CALL_STACK_MESSAGE1("CVolume::Close()");
    HANDLES(CloseHandle(HVolume));
    HVolume = INVALID_HANDLE_VALUE;
    TRACE_I("Volume is closed");
}
