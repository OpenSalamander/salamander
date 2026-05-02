// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// According to http://www.microsoft.com/resources/documentation/windows/xp/all/proddocs/en-us/label.mspx?mfr=true
// we need 32 characters for NTFS, 11 characters for FAT
#define MAX_VOLNAME 100
#define MAX_FSNAME 50

template <typename CHAR>
struct VolumeDetails
{
    CHAR MountPoint[MAX_PATH];    // Path the volume is mounted on, e.g "C:\"
    CHAR GUIDPath[MAX_PATH];      // Unique identifier of the volume (valid only on W2k and higher)
    CHAR FSName[MAX_FSNAME];      // The name of the volume file system, e.g. NTFS, FAT32, FAT
    CHAR VolumeName[MAX_VOLNAME]; // The name (label) assigned to the volume by the user
    VolumeType Type;              // Type of the volume (fixed, removable etc.)
    CQuadWord BytesTotal;         // The total size of the volume (may not be accurate if quota is in effect)
    CQuadWord BytesFree;          // The total free space on the volume
};

template <typename CHAR>
class VolumeListing : public TDirectArray<VolumeDetails<CHAR>>
{
public:
    VolumeListing() : TDirectArray<VolumeDetails<CHAR>>(5, 5) {}
};

template <typename CHAR>
struct DiskRecord
{
    DiskRecord() : DiskName(NULL) { VolumeName[0] = 0; }
    ~DiskRecord()
    {
        if (DiskName)
            delete[] DiskName;
    }

    CHAR VolumeName[MAX_VOLNAME];
    CHAR* DiskName;
};

template <typename CHAR>
class DiskRecArray : public TIndirectArray<DiskRecord<CHAR>>
{
public:
    DiskRecArray() : TIndirectArray<DiskRecord<CHAR>>(5, 5) {}
};

template <typename CHAR>
struct VolumeRecord
{
    VolumeRecord() : MountPoints(5, 5), Root(NULL) { VolumeName[0] = 0; }
    ~VolumeRecord() { erase(); }

    void erase()
    {
        VolumeName[0] = 0;
        if (Root)
            delete[] Root;
        while (MountPoints.Count)
        {
            delete[] MountPoints[MountPoints.Count - 1];
            MountPoints.Delete(MountPoints.Count - 1);
        }
    }

    CHAR VolumeName[MAX_VOLNAME];
    CHAR* Root;
    TDirectArray<CHAR*> MountPoints;
};

template <typename CHAR>
class VolumeRecArray : public TIndirectArray<VolumeRecord<CHAR>>
{
public:
    VolumeRecArray() : TIndirectArray<VolumeRecord<CHAR>>(5, 5) {}
};

template <typename CHAR>
BOOL GetLocalDiskDrives(DiskRecArray<CHAR>& disks)
{
    BOOL ret = TRUE;
    if (OS<CHAR>::OS_GetLogicalDriveStringsExists() &&
        OS<CHAR>::OS_GetVolumeNameForVolumeMountPointExists())
    {
        // create initial list of disks in the system
        size_t bufsize = MAX_PATH / 2;
        CHAR* buffer = NULL;
        DWORD len;
        do
        {
            bufsize *= 2;
            if (buffer != NULL)
                delete[] buffer;
            buffer = new CHAR[bufsize];
            if (buffer != NULL)
                len = OS<CHAR>::OS_GetLogicalDriveStrings(bufsize, buffer);
            else
            {
                String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                ret = FALSE;
                break;
            }
        } while (len > bufsize);

        if (ret)
        {
            CHAR* disk = buffer;
            while (disk && *disk)
            {
                DiskRecord<CHAR>* rec = new DiskRecord<CHAR>;
                if (rec != NULL)
                {
                    rec->DiskName = String<CHAR>::NewStr(disk);
                    rec->VolumeName[0] = 0;
                    // there is no point in adding disks without volume name
                    if (!OS<CHAR>::OS_GetVolumeNameForVolumeMountPoint(disk, rec->VolumeName, MAX_VOLNAME) ||
                        rec->VolumeName[0] == 0)
                    {
                        delete rec;
                    }
                    else
                    {
                        disks.Add(rec);
                        if (!disks.IsGood())
                        {
                            String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                            disks.ResetState();
                            delete rec;
                            ret = FALSE;
                            break;
                        }
                    }
                }
                else
                {
                    String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }

                disk = disk + String<CHAR>::StrLen(disk) + 1;
            }
            delete[] buffer;
        }
    }
    return ret;
}

template <typename CHAR>
BOOL GetVolumePathForVolumeName(const CHAR* volumeName, const VolumeRecArray<CHAR>& volumes, CHAR* pathName)
{
    BOOL ret = FALSE;
    pathName[0] = 0;
    if (OS<CHAR>::OS_GetVolumePathNamesForVolumeNameExists())
    {
        DWORD not_working;
        // this function should return size of required buffer, but it doesn't.
        if (!OS<CHAR>::OS_GetVolumePathNamesForVolumeName(volumeName, pathName, MAX_PATH, &not_working) &&
            ERROR_MORE_DATA != GetLastError())
        {
            pathName[0] = 0;
        }
        else
        {
            ret = TRUE;
            pathName[MAX_PATH - 1] = 0;
        }
    }
    else
    {
        for (int i = 0; i < volumes.Count; ++i)
        {
            if (!String<CHAR>::StrCmp(volumeName, const_cast<VolumeRecArray<CHAR>&>(volumes)[i]->VolumeName))
            {
                if (const_cast<VolumeRecArray<CHAR>&>(volumes)[i]->Root != 0)
                {
                    if (String<CHAR>::StrLen(const_cast<VolumeRecArray<CHAR>&>(volumes)[i]->Root) < MAX_PATH)
                    {
                        String<CHAR>::StrCpy(pathName, const_cast<VolumeRecArray<CHAR>&>(volumes)[i]->Root);
                        pathName[MAX_PATH - 1] = 0;
                    }
                    ret = TRUE;
                }
                break;
            }
        }
    }
    return ret;
}

BOOL GetDiskFreeSpace95(const char* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters);
BOOL GetDiskFreeSpace95(const wchar_t* path, LPDWORD lpSectorsPerCluster,
                        LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                        LPDWORD lpTotalNumberOfClusters);
BOOL GetDiskFreeSpace95Aux(const char* path, LPDWORD lpSectorsPerCluster,
                           LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                           LPDWORD lpTotalNumberOfClusters);
BOOL GetDiskFreeSpace95Aux(const wchar_t* path, LPDWORD lpSectorsPerCluster,
                           LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                           LPDWORD lpTotalNumberOfClusters);

template <typename CHAR>
void GetVolumeDetails(const CHAR* rootPath, int volumeType, const VolumeRecArray<CHAR>& volumes,
                      DiskRecArray<CHAR>& disks, CHAR* volumeName, size_t volumeNameLen,
                      CHAR* volumeFSName, size_t volumeFSLen, CQuadWord& bytesTotal, CQuadWord& bytesFree,
                      CHAR* pathName = NULL)
{
    CALL_STACK_MESSAGE1("CConnectDialog::GetVolumeDetails()");

    if (pathName)
    {
        if (!GetVolumePathForVolumeName(rootPath, volumes, pathName))
            pathName[0] = 0;
    }

    // For floppies, skip check of FS type and free space, it's annoying as it access the medium
    BOOL skipCheck = FALSE;
    if (volumeType == DRIVE_REMOVABLE && IsVolumeFloppy(disks, rootPath))
        skipCheck = TRUE;

    bytesTotal.Set(-1, -1);
    bytesFree.Set(-1, -1);

    if (!skipCheck)
    {
        DWORD volumeSerial, volumeComponentLen, volumeFlags;
        if (!OS<CHAR>::OS_GetVolumeInfo(rootPath, volumeName, (DWORD)volumeNameLen,
                                        &volumeSerial, &volumeComponentLen,
                                        &volumeFlags, volumeFSName, (DWORD)volumeFSLen))
        {
            volumeName[0] = 0;
            volumeFSName[0] = 0;
        }
        if (OS<CHAR>::OS_GetDiskFreeSpaceExExists())
        {
            ULARGE_INTEGER available, total, free;
            if (OS<CHAR>::OS_GetDiskFreeSpaceEx(rootPath, &available, &total, &free))
            {
                bytesTotal.Set(total.LowPart, total.HighPart);
                bytesFree.Set(free.LowPart, free.HighPart);
            }
        }
        else
        {
            DWORD a;
            DWORD b;
            DWORD c;
            DWORD d;
            BOOL ret;

            if (!String<CHAR>::StrICmp(volumeFSName, CVolume<CHAR>::STRING_FAT32))
                ret = GetDiskFreeSpace95(rootPath, &a, &b, &c, &d);
            else
                ret = GetDiskFreeSpace95Aux(rootPath, &a, &b, &c, &d);

            if (ret)
            {
                bytesFree = CQuadWord(a, 0) * CQuadWord(b, 0) * CQuadWord(c, 0);
                bytesTotal = CQuadWord(a, 0) * CQuadWord(b, 0) * CQuadWord(d, 0);
            }
        }
    }
    else
    {
        volumeName[0] = 0;
        volumeFSName[0] = 0;
    }

    // strip trailing slash from mount points
    size_t l;
    if (pathName && (l = String<CHAR>::StrLen(pathName)) > 3)
    {
        if (l > 0 && pathName[l - 1] == (CHAR)'\\')
            pathName[l - 1] = 0;
    }
}

template <typename CHAR>
BOOL IsVolumeFloppy(DiskRecArray<CHAR>& disks, const CHAR* volumeName)
{
    BOOL isFloppy = FALSE;
    const CHAR* removableDriveDiskName = NULL;
    if (volumeName[0] != 0 &&
        volumeName[1] == ':' &&
        volumeName[2] == '\\' &&
        volumeName[3] == 0)
    {
        removableDriveDiskName = volumeName;
    }
    else
    {
        for (int j = 0; j < disks.Count; ++j)
        {
            if (String<CHAR>::StrCmp(disks[j]->VolumeName, volumeName) == 0)
            {
                removableDriveDiskName = disks[j]->DiskName;
                if (removableDriveDiskName[0] == 0 ||
                    removableDriveDiskName[1] != ':' ||
                    removableDriveDiskName[2] != '\\' ||
                    removableDriveDiskName[3] != 0)
                {
                    removableDriveDiskName = NULL;
                }
                else
                    break;
            }
        }
    }
    if (removableDriveDiskName != NULL)
    {
        DWORD formFactor = OS<CHAR>::OS_GetDriveFormFactor(removableDriveDiskName);
        if (formFactor == 350 || formFactor == 525 || formFactor == 800)
            isFloppy = TRUE;
    }
    return isFloppy;
}

template <typename CHAR>
void EnumerateAllVolumes(VolumeRecArray<CHAR>& volumes, DiskRecArray<CHAR>& disks)
{
    if (OS<CHAR>::OS_VolumeEnumExists() &&
        OS<CHAR>::OS_VolumeMountPointEnumExists() &&
        OS<CHAR>::OS_GetVolumeNameForVolumeMountPointExists())
    {
        // list all volumes along with their mount points
        CHAR volumeName[MAX_VOLNAME];
        HANDLE hVol = OS<CHAR>::OS_FindFirstVolume(volumeName, MAX_VOLNAME);
        if (hVol != INVALID_HANDLE_VALUE)
        {
            do
            {
                // For floppies, skip check of FS type and free space, it's annoying as it access the medium
                int type = OS<CHAR>::OS_GetVolumeType(volumeName);
                BOOL skipCheck = FALSE;
                if (type == DRIVE_REMOVABLE && IsVolumeFloppy(disks, volumeName))
                    skipCheck = TRUE;

                VolumeRecord<CHAR>* record = new VolumeRecord<CHAR>;
                if (record != NULL)
                {
                    String<CHAR>::StrCpy(record->VolumeName, volumeName);
                    if (!skipCheck)
                    {
                        CHAR mntPoint[MAX_PATH];
                        HANDLE hMntPt = OS<CHAR>::OS_FindFirstVolumeMountPoint(volumeName, mntPoint, MAX_PATH);
                        if (hMntPt != INVALID_HANDLE_VALUE)
                        {
                            do
                            {
                                CHAR* newStr = String<CHAR>::NewStr(mntPoint);
                                if (newStr != NULL)
                                {
                                    record->MountPoints.Add(newStr);
                                    if (!record->MountPoints.IsGood())
                                    {
                                        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                                        record->MountPoints.ResetState();
                                        delete[] newStr;
                                    }
                                }
                            } while (OS<CHAR>::OS_FindNextVolumeMountPoint(hMntPt, mntPoint, MAX_PATH));
                            OS<CHAR>::OS_FindVolumeMountPointClose(hMntPt);
                        }
                    }
                    volumes.Add(record);
                    if (!volumes.IsGood())
                    {
                        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                        volumes.ResetState();
                        delete record;
                    }
                }
                else
                {
                    String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                }
            } while (OS<CHAR>::OS_FindNextVolume(hVol, volumeName, MAX_VOLNAME));
            OS<CHAR>::OS_FindVolumeClose(hVol);
        }

        // now, we need to find volumes to disks, and also add mount point to the list of disks
        // and do this until we find all relations we can
        BOOL updated = TRUE;
        while (updated)
        {
            updated = FALSE;

            for (int i = 0; i < volumes.Count; ++i)
            {
                // if no root yet, try to find one
                if (!volumes[i]->Root)
                {
                    int j;
                    for (j = 0; j < disks.Count; ++j)
                    {
                        if (!String<CHAR>::StrCmp(volumes[i]->VolumeName, disks[j]->VolumeName))
                        {
                            // volume identified,
                            volumes[i]->Root = String<CHAR>::NewStr(disks[j]->DiskName);
                            if (volumes[i]->Root == NULL)
                            {
                                String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                            }
                            updated = TRUE;
                            break;
                        }
                    }

                    // now look for mount points
                    if (volumes[i]->Root)
                    {
                        for (j = 0; j < volumes[i]->MountPoints.Count; ++j)
                        {
                            DiskRecord<CHAR>* rec = new DiskRecord<CHAR>;
                            if (rec != NULL)
                            {
                                rec->DiskName = new CHAR[String<CHAR>::StrLen(volumes[i]->Root) +
                                                         String<CHAR>::StrLen(volumes[i]->MountPoints[j]) + 1];
                                if (rec->DiskName != NULL)
                                {
                                    String<CHAR>::StrCpy(rec->DiskName, volumes[i]->Root);
                                    String<CHAR>::StrCat(rec->DiskName, volumes[i]->MountPoints[j]);
                                    OS<CHAR>::OS_GetVolumeNameForVolumeMountPoint(rec->DiskName, rec->VolumeName, MAX_VOLNAME);
                                    disks.Add(rec);
                                    if (!disks.IsGood())
                                    {
                                        String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                                        disks.ResetState();
                                        delete rec;
                                    }
                                }
                                else
                                {
                                    String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                                    delete rec;
                                }
                            }
                            else
                            {
                                String<CHAR>::Error(IDS_UNDELETE, IDS_LOWMEM);
                            }
                        }
                    }
                }
            }
        }
    }
}

template <typename CHAR>
DWORD GetVolumeListing(VolumeListing<CHAR>& listing)
{
    // get a list of disks connected to the system
    DiskRecArray<CHAR> disks;
    GetLocalDiskDrives(disks);

    // if we got list of volumes, try to assign mount points to all of them
    VolumeRecArray<CHAR> volumes;
    if (!OS<CHAR>::OS_GetVolumePathNamesForVolumeNameExists())
        EnumerateAllVolumes(volumes, disks);

    // enumerate all available volumes
    DWORD err = ERROR_NO_MORE_FILES;
    VolumeDetails<CHAR> volumeDetails;
    if (OS<CHAR>::OS_VolumeEnumExists())
    {
        int selLen = 0;
        int serial = 0;
        CHAR guidPath[MAX_PATH];

        HANDLE volEnum = OS<CHAR>::OS_FindFirstVolume(guidPath, MAX_PATH);
        while (volEnum != INVALID_HANDLE_VALUE)
        {
            memset(&volumeDetails, 0, sizeof(volumeDetails));
            volumeDetails.Type = OS<CHAR>::OS_GetVolumeType(guidPath);

            // consider only fixed and removable drives
            if (volumeDetails.Type == VT_DRIVE_FIXED ||
                volumeDetails.Type == VT_DRIVE_REMOVABLE)
            {
                String<CHAR>::StrCpy(volumeDetails.GUIDPath, guidPath);
                GetVolumeDetails(guidPath, volumeDetails.Type, volumes, disks, volumeDetails.VolumeName, MAX_VOLNAME,
                                 volumeDetails.FSName, MAX_FSNAME, volumeDetails.BytesTotal, volumeDetails.BytesFree,
                                 volumeDetails.MountPoint);
                listing.Add(volumeDetails);
                if (!listing.IsGood())
                {
                    listing.ResetState();
                    SetLastError(ERROR_NOT_ENOUGH_MEMORY);
                    break;
                }
            }

            if (!OS<CHAR>::OS_FindNextVolume(volEnum, guidPath, MAX_PATH))
                break;
        }
        err = GetLastError();
        if (INVALID_HANDLE_VALUE != volEnum)
            OS<CHAR>::OS_FindVolumeClose(volEnum);
        if (ERROR_NO_MORE_FILES == err)
            err = ERROR_SUCCESS;
    }
    else
    {
        // old approach using GetLogicalDrives()
        DWORD drives = GetLogicalDrives();
        CHAR root[4];
        String<CHAR>::StrCpy(root, CVolume<CHAR>::STRING_ROOT);

        unsigned long mask = 1;
        for (CHAR i = 'A'; i <= 'Z'; i++, mask <<= 1)
        {
            if (drives & mask)
            {
                root[0] = i;
                memset(&volumeDetails, 0, sizeof(volumeDetails));
                volumeDetails.Type = OS<CHAR>::OS_GetVolumeType(root);
                if (volumeDetails.Type == VT_DRIVE_FIXED ||
                    volumeDetails.Type == VT_DRIVE_REMOVABLE)
                {
                    String<CHAR>::StrCpy(volumeDetails.MountPoint, root);
                    GetVolumeDetails<CHAR>(root, volumeDetails.Type, volumes, disks, volumeDetails.VolumeName, MAX_VOLNAME,
                                           volumeDetails.FSName, MAX_FSNAME, volumeDetails.BytesTotal, volumeDetails.BytesFree);
                    listing.Add(volumeDetails);
                    if (!listing.IsGood())
                    {
                        listing.ResetState();
                        err = ERROR_NOT_ENOUGH_MEMORY;
                        break;
                    }
                }
            }
        }
        if (ERROR_NO_MORE_FILES == err)
            err = ERROR_SUCCESS;
    }

    return err;
}
