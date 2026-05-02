// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push, undelete_h)
#pragma pack(4) // IMPORTANT: alignment of all structures

// ****************************************************************************
//
//  Definitions
//

// flags for UpdateSnapshot
#define UF_SHOWEXISTING 0x0001       // show existing files and directories
#define UF_SCANVACANTCLUSTERS 0x0002 // look for directories in vacant clusters (FAT)
#define UF_SHOWZEROFILES 0x0004      // show files with zero size
#define UF_SHOWEMPTYDIRS 0x0008      // show directories that do not contain any files
#define UF_SHOWMETAFILES 0x0010      // show meta files
#define UF_ESTIMATEDAMAGE 0x0020     // estimate files damage
#define UF_GETLOSTCLUSTERMAP 0x0100  // (for forensic needs only) returns clusters unused by files or occupied by FC_FAIR or FC_POOR files

typedef unsigned __int64 QWORD;

#ifndef QW_MAX
#define QW_MAX ((QWORD)(0xFFFFFFFFFFFFFFFF))
#endif // QW_MAX

#define MAX_VOLNAME 100
#define MAX_FSNAME 50

#define FR_FLAGS_VIRTUALDIR 0x10000000 // virtual directory such as {All Deleted Files} or {Metafiles}
#define FR_FLAGS_METAFILE 0x20000000   // metafiles in {Metafiles} virtual directory
#define FR_FLAGS_CHAININFAT 0x40000000 // exFAT only, cluster chain is located in FAT
#define FR_FLAGS_DELETED 0x80000000    // file or directory is deleted
#define FR_FLAGS_CONDITION_MASK 0x0000000F
#define FR_FLAGS_CONDITION_UNKNOWN 0x00000000 // damage estimation failed or was not performed at all
#define FR_FLAGS_CONDITION_GOOD 0x00000001    // existing file / MFT-resident / deleted but no collision with existing files
#define FR_FLAGS_CONDITION_FAIR 0x00000002    // some clusters conflict with clusters of other deleted files
#define FR_FLAGS_CONDITION_POOR 0x00000003    // some clusters conflict with existing files
#define FR_FLAGS_CONDITION_LOST 0x00000004    // all clusters of the deleted file are used again (ie. overwritten)

#define UPC_GETCANCEL 0
#define UPC_SETTEXT 1
#define UPC_SETPROGRESS 2
#define UPC_QUESTION 3

// ****************************************************************************
//
//  Data structures
//

// These structures are used for volume enumeration
enum VOLUMETYPE
{
    VTP_DRIVE_UNKNOWN = DRIVE_UNKNOWN,
    VTP_DRIVE_NO_ROOT_DIR = DRIVE_NO_ROOT_DIR,
    VTP_DRIVE_REMOVABLE = DRIVE_REMOVABLE,
    VTP_DRIVE_FIXED = DRIVE_FIXED,
    VTP_DRIVE_REMOTE = DRIVE_REMOTE,
    VTP_DRIVE_CDROM = DRIVE_CDROM,
    VTP_DRIVE_RAMDISK = DRIVE_RAMDISK,
};

// --- You can safely ignore this structure, as it is normally not needed. ---
struct CLUSTER_MAP
{
    DWORD Items;          // number of items in 'FirstCluster' and 'ClustersCount' arrays
    QWORD* FirstCluster;  // first cluster of cluster chain, use ReadVolumeClusters() to read these clusters
    QWORD* ClustersCount; // number of clusters in chain
};

#pragma pack(pop, undelete_h)
