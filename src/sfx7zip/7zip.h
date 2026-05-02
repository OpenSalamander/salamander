// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

struct SCabinet
{
    HANDLE file;          // open file handle or NULL
                          //  unsigned char flags;                 // header flags
                          //  struct SFile *files;                 // array of files in this cabinet
    unsigned long offset; // offset to data blocks
};

int DecompressInit(struct SCabinet* cabinet);
int UnpackArchive(struct SCabinet* cabinet);
int DecompressDone(struct SCabinet* cabinet);
