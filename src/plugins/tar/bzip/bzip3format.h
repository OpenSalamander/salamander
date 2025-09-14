// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// BZip3 file format definitions

#pragma pack(push, 1)

struct BZ3_Header
{
    static constexpr const char SIGNATURE[] = "BZ3v1";
    static constexpr size_t SIGNATURE_LEN = sizeof(SIGNATURE) - 1;

    BYTE Signature[SIGNATURE_LEN];
    uint32_t BlockSize;
};

struct BZ3_Block
{
    uint32_t CompressedSize;
    uint32_t OrigSize;
};

#pragma pack(pop)
