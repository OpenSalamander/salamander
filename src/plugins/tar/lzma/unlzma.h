// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <lzma.h>

class CLZMa : public CZippedFile
{
public:
    CLZMa(const char* filename, HANDLE file, unsigned char* buffer, unsigned long start, unsigned long read, CQuadWord inputSize);
    ~CLZMa() override;

    BOOL BuggySize() override { return TRUE; }

protected:
    BOOL EndReached = FALSE; // set, when all data was extracted

    lzma_stream m_strm{};
    BOOL DecompressBlock(unsigned short needed) override;
};
