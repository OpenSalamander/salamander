// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"
#include <libheif/heif.h>

class ImageHeif
{
public:
    ImageHeif() = default;
    ~ImageHeif();

    PVCODE Open(const char* filename, PVImageInfo& pvii);
    PVCODE Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific);

private:
    heif_context* m_ctx{};
    heif_image_handle* m_handle{};
    heif_image* m_image{};
};
