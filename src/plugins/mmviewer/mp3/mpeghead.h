// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef struct
{
    short lsf;
    short mpeg;
    short layer;
    short error_protection;
    short bitrate_index;
    short sampling_frequency_index;
    short padding;
    short extension;
    short mode;
    short mode_ext;
    short copyright;
    short original;
    short emphasis;
    short variable_bitrate;
    int framesize;
    int frames;
    int kbps;
    int hz;
    int length_msec;
    int headpos;
} MPEGHEAD_DECODED;

BOOL HeaderCheck(DWORD head);
DWORD ScanForHeader(FILE* f, DWORD size);
BOOL DecodeHeader(DWORD head, MPEGHEAD_DECODED* pmhd, FILE* f, DWORD filesize);
