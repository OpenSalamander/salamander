// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "xpm.h"
#include <assert.h>

PVCODE ImageXpm::Open(const char* filename, PVImageInfo& pvii)
{
    auto result = xpm::Image::Open(filename);
    if (!result)
    {
        switch (result.error())
        {
        case xpm::Code::InvalidFormat:
            return PVC_UNKNOWN_FILE_STRUCT;
        case xpm::Code::NoMemory:
            return PVC_OUT_OF_MEMORY;
        case xpm::Code::OpenFailed:
        default:
            return PVC_CANNOT_OPEN_FILE;
        }
    }

    // Image loaded successfully, fill out image info struct
    m_image = std::move(result.value());

    pvii.cbSize = sizeof(pvii);
    pvii.Format = PVF_BMP;
    pvii.Width = m_image->GetWidth();
    pvii.Height = m_image->GetHeight();
    pvii.NumOfImages = 1;
    pvii.FileSize = static_cast<DWORD>(m_image->GetFileSize());
    pvii.Colors = PV_COLOR_TC24;
    pvii.ColorModel = PVCM_RGB;
    pvii.Compression = PVCS_NO_COMPRESSION;

    strcpy_s(pvii.Info1, "X PixMap");
    if (m_image->GetFormat() == xpm::Format::Version2)
    {
        strcat_s(pvii.Info1, " v2");
    }
    else if (m_image->GetFormat() == xpm::Format::Version3)
    {
        strcat_s(pvii.Info1, " v3");
    }

    return PVC_OK;
}

PVCODE ImageXpm::Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific)
{
    if (!m_image->CheckImageSize())
        return PVC_OUT_OF_MEMORY;

    std::vector<uint8_t> bgraData;
    try
    {
        const size_t pixelCount = m_image->GetHeight() * m_image->GetWidth();
        bgraData.reserve(pixelCount * 4);
    }
    catch (...)
    {
        return PVC_OUT_OF_MEMORY;
    }

    int progressRound = 0;
    auto appender = [&bgraData, &progressRound, Progress, AppSpecific](COLORREF color)
    {
        // Ensure data can be appended to the preallocated buffer
        assert(bgraData.size() + 4 <= bgraData.capacity());

        // Convert decoded RGBA pixels to BGRA byte order for Windows DIB
        bgraData.push_back(GetBValue(color)); // blue
        bgraData.push_back(GetGValue(color)); // green
        bgraData.push_back(GetRValue(color)); // red
        bgraData.push_back(0xFF);             // ignore alpha for now

        // Report progress
        if (Progress && (progressRound++ > 1000))
        {
            progressRound = 0;
            const auto progressPercentage = static_cast<int>(bgraData.size() * 100 / bgraData.capacity());
            if (Progress(progressPercentage, AppSpecific))
                return false;
        }

        return true;
    };

    const auto result = m_image->Decode(appender);
    switch (result)
    {
    case xpm::Code::Success:
        break;
    case xpm::Code::InvalidFormat:
        return PVC_UNKNOWN_FILE_STRUCT;
    case xpm::Code::NoMemory:
        return PVC_OUT_OF_MEMORY;
    case xpm::Code::Cancelled:
        return PVC_CANCELED;
    case xpm::Code::DecodeError:
    default:
        return PVC_READING_ERROR;
    }

    // Create the DIB bitmap
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = static_cast<LONG>(m_image->GetWidth());
    bmi.bmiHeader.biHeight = -static_cast<LONG>(m_image->GetHeight()); // top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 32-bit BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return PVC_GDI_ERROR;
    bmp = CreateDIBitmap(hdc, &bmi.bmiHeader, CBM_INIT, bgraData.data(), &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (!bmp)
        return PVC_GDI_ERROR;

    return PVC_OK;
}
