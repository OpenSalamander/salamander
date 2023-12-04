// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

typedef struct
{
    int version;
    int channels;
    int sampling_rate;
    int bitrate;
    int length;
    int object_type;
    int header_type;
} AACHEAD_DECODED;

bool DecodeAACHeader(FILE* file, AACHEAD_DECODED* info);
