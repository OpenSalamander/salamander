// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "../compress/compress.h"

class CLZH : public CZippedFile
{
public:
    CLZH(const char* filename, HANDLE file, unsigned char* buffer, unsigned long read);
    virtual ~CLZH();

    virtual BOOL IsCompressed() { return TRUE; }

protected:
    BOOL DecompressBlock(unsigned short needed);
};
