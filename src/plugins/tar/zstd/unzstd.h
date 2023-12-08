// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <zstd.h>

class CZStd : public CZippedFile
{
public:
    CZStd(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    ~CZStd() override;

    BOOL BuggySize() override { return TRUE; }

protected:
    BOOL EndReached = FALSE; // set, when all data was extracted

    ZSTD_DCtx* m_DContext{}; // zstd decompression context
    BOOL DecompressBlock(unsigned short needed) override;
};
