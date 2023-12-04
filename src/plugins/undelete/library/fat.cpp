// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "..\undelete.rh2"

#include "undelete.h"
#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "dataruns.h"
#include "..\dialogs.h"
#include "..\undelete.h"
#include "fat.h"

// HOW IT WORKS:
//
// - First, we will read whole FAT, so we know which cluster follows which.
//
// - Then we will analyze disk in four phases:
//
//   1. Reading existing directories - we will start with root, followed by
//      all existing sub directories. Reading is (HDD) optimized - instead of reading
//      whole directory we will just schedule its clusters for reading - we will
//      store cluster numbers to one of two heaps (we are using heaps because we
//      can find minimum easily): HeapAhead, and HeapBehind - heap before head
//      and heap behind head. We are reading clusters near HDD head from HeapAhead
//      heap so HDD head is moving in one direction, while reading scheduled clusters.
//      When HeapAhead heap is empty, we will swap heaps and repeat process again.
//      When cluster is read, directories with records are analyzed (ScanDirectoryCluster)
//      and further clusters are scheduled. Clusters are added to buffers of related
//      directories. When we have complete directory, we will transform records to
//      FILE_RECORD structures and free buffer (DecodeDirectoryClusters).
//      ScanDirectoryClusters also marks where are beginning of deleted directories
//      and files.
//
//   2. Reading of deleted directories - we already know deleted directories from
//      phase 1. and we have marks in FAT with beginning of deleted directories.
//
// - Optimization of clusters reading works well: we are 2-3x faster compared to R-Undelete
//
// - How we are searching long names for deleted records: when short record is
//   preceded by long record, we can reconstruct first letter of short record
//   from the long record (convert from Unicode and with UpCase), because there
//   is 0xE5 instead. Then we will get checksum of short name and get previous
//   long records where checksum matches.

// ****************************************************************************
//
//  DecodeDirectoryClusters() - walk through directory clusters and make list
//                              of FILE_RECORD structures
//

BOOL ConvertFATName(const char* fatName, char* name) // convert name from format 83 to 8.3
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE2("ConvertFATName(%s, )", fatName);
    char* p = name;
    for (int i = 0; i < 11; i++)
    {
        if (i == 8)
        {
            // of there is extension, insert dot
            if (fatName[i] != ' ')
            {
                if (p == name) // wrong file name
                    return FALSE;
                *p = '.';
                p++;
            }
            else
                break; // we can leave
        }
        if (fatName[i] != ' ')
        {
            *p = fatName[i];
            if (i == 0 && *p == 0x05)
                *p = (char)0xE5; // 0x05 is used instead of 0xE5 in the Japan/KANJI
            p++;
        }
    }
    if (p == name)
        return FALSE; // wrong file name
    *p = 0;
    return TRUE;
}

// MS function for 8.3 name checksum
BYTE ChkSum(BYTE* pFcpName)
{
    CALL_STACK_MESSAGE1("ChkSum()");
    BYTE sum = 0;
    for (int fcbNameLen = 11; fcbNameLen != 0; fcbNameLen--)
        sum = (((sum & 1) ? 0x80 : 0) + (sum >> 1) + *pFcpName++) & 0xff;
    return sum;
}

#ifdef ENABLE_TRACE_X
void DumpEntry(DIR_ENTRY_SHORT* entry)
{
    CALL_STACK_MESSAGE1("DumpEntry()");

    char text[200];
    if (!islong(entry->Attr))
    {
        sprintf(text, "S: Name=%c%.10s Attr=0x%02X Hi=%d Lo=%d Size=%d",
                entry->Name[0] == 0xE5 ? '$' : entry->Name[0], entry->Name + 1,
                entry->Attr, entry->FstClusHI, entry->FstClusLO, entry->FileSize);
    }
    else
    {
#define long_entry ((DIR_ENTRY_LONG*)entry)
#define STR10(x) x[0], x[2], x[4], x[6], x[8]
#define STR12(x) x[0], x[2], x[4], x[6], x[8], x[10]
#define STR04(x) x[0], x[2]
        sprintf(text, "L: Ord=0x%02X Attr=%d Sum=%d Name=%c%c%c%c%c%c%c%c%c%c%c%c%c",
                long_entry->Ord, long_entry->Attr, long_entry->Chksum, STR10(long_entry->Name1),
                STR12(long_entry->Name2), STR04(long_entry->Name3));
    }
    TRACE_X(text);
}
#endif

BOOL isupdir(DIR_ENTRY_SHORT* entry)
{
    CALL_STACK_MESSAGE_NONE
    //CALL_STACK_MESSAGE1("isupdir()");
    return !memcmp(entry->Name, ".          ", 11) || !memcmp(entry->Name, "..         ", 11);
}
