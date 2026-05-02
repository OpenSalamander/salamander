// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Info taken from http://www.debuginfo.com/articles/debuginfomatch.html

enum CV_TYPE
{
    CV_TYPE_PDB20 = 0x3031424E, // 'NB10'
    CV_TYPE_PDB70 = 0x53445352, // 'RSDS'
};

#include <pshpack1.h>

struct CV_INFO_PDB20
{
    DWORD CvType;
    DWORD Offset;
    DWORD Signature;
    DWORD Age;
    BYTE PdbFileName[1];
};

struct CV_INFO_PDB70
{
    DWORD CvType;
    GUID Signature;
    DWORD Age;
    BYTE PdbFileName[1];
};

#include <poppack.h>
