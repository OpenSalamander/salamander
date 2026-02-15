// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "lib/pvw32dll.h"
#include "xpm-lite.h"

class ImageXpm
{
public:
    ImageXpm() = default;
    ~ImageXpm() = default;

    PVCODE Open(const char* filename, PVImageInfo& pvii);
    PVCODE Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific);

private:
    std::unique_ptr<xpm::Image> m_image;
};
