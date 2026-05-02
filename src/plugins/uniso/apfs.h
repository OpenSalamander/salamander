// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************
//
// APFS - Apple File System fully supported by macOS since High Sierra (10.13).
//        Based on https://blog.cugu.eu/post/apfs.
//

#define APFS_CONTAINER_SUPERBLOCK_SIGNATURE 'BSXN'

typedef BYTE UInt8;
typedef DWORD UInt32;
typedef unsigned __int64 UInt64;

class CAPFS
{
public:
#pragma pack(push, 1)
    struct ContainerSuperblock
    {
        UInt32 magic; // APFS_CONTAINER_SUPERBLOCK_SIGNATURE
        UInt32 blocksize;
        UInt64 totalblocks;
        UInt8 guid[16];
        UInt64 next_free_block_id;
        UInt64 next_version;
        UInt32 previous_containersuperblock_block;
        UInt64 spaceman_id;
        UInt64 block_map_block;
        UInt64 unknown_id;
        UInt32 padding2;
        UInt32 apfs_count;
        UInt64 offset_apfs[1]; // repeat apfs_count times
    };
};
