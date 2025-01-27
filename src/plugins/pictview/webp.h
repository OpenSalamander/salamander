// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"
#include <webp/decode.h>
#include <fstream>

class ImageWebp
{
public:
    ImageWebp();
    ~ImageWebp();

    PVCODE Open(const char* filename, PVImageInfo& pvii);
    PVCODE Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific);

private:
    void MoveFrom(ImageWebp& other);

    // image decoder
    WebPIDecoder* m_idec{nullptr};
    WebPDecoderConfig m_config{};
    std::ifstream m_file;
    size_t m_fileSize{};
};
