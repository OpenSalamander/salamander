// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "webp.h"
#include <array>
#include <assert.h>

ImageWebp::ImageWebp()
{
    WebPInitDecoderConfig(&m_config);
}

ImageWebp::~ImageWebp()
{
    if (m_idec)
        WebPIDelete(m_idec);
    WebPFreeDecBuffer(&m_config.output);
}

PVCODE ImageWebp::Open(const char* filename, PVImageInfo& pvii)
{
    pvii = {};

    m_file.open(filename, std::ios::binary);
    if (!m_file.is_open())
        return PVC_CANNOT_OPEN_FILE;

    // get the file size
    m_file.seekg(0, std::ios::end);
    m_fileSize = static_cast<DWORD>(m_file.tellg());
    m_file.seekg(0, std::ios::beg);

    // read the first 30 bytes of the file
    const size_t HeaderSize = 30;
    std::array<uint8_t, HeaderSize> buffer;
    if (!m_file.read(reinterpret_cast<char*>(buffer.data()), buffer.size()))
        return PVC_READING_ERROR;
    m_file.seekg(0, std::ios::beg);

    // get the image features
    auto result = WebPGetFeatures(buffer.data(), buffer.size(), &m_config.input);
    if (result != VP8_STATUS_OK)
    {
        switch (result)
        {
        case VP8_STATUS_OUT_OF_MEMORY:
            return PVC_OUT_OF_MEMORY;
        case VP8_STATUS_BITSTREAM_ERROR:
            return PVC_UNKNOWN_FILE_STRUCT;
        default:
            return PVC_READING_ERROR;
        }
    }

    // fill out the image info
    pvii.cbSize = sizeof(pvii);
    pvii.Format = PVF_BMP;
    pvii.Width = m_config.input.width;
    pvii.Height = m_config.input.height;
    pvii.BytesPerLine = m_config.input.width * 4; // 32-bit RGBA format
    pvii.Colors = PV_COLOR_TC32;
    pvii.ColorModel = PVCM_RGB;
    pvii.Compression = PVCS_NO_COMPRESSION;
    pvii.FileSize = static_cast<DWORD>(m_fileSize);
    pvii.NumOfImages = 1;

    strcpy_s(pvii.Info1, "WebP");

    return PVC_OK;
}

PVCODE ImageWebp::Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific)
{
    assert(m_file.is_open());

    // initialize the decoder
    m_config.output.colorspace = MODE_BGRA;
    m_idec = WebPIDecode(nullptr, 0, &m_config);
    if (!m_idec)
        return PVC_EXCEPTION;

    const size_t ChunkSize = 1024;
    std::array<uint8_t, ChunkSize> buffer;
    VP8StatusCode result = VP8_STATUS_SUSPENDED;

    // read file by chunks
    size_t totalBytesRead = 0;
    int progressRound = 0;
    while (!m_file.eof() && result == VP8_STATUS_SUSPENDED)
    {
        m_file.read(reinterpret_cast<char*>(buffer.data()), buffer.size());
        if (m_file.bad())
            return PVC_READING_ERROR;
        const auto bytesRead = static_cast<size_t>(m_file.gcount());
        if (bytesRead == 0)
            return PVC_UNEXPECTED_EOF;
        totalBytesRead += bytesRead;

        result = WebPIAppend(m_idec, buffer.data(), bytesRead);

        // report progress
        if (Progress && (progressRound++ > 10))
        {
            progressRound = 0;
            const auto progressPercentage = static_cast<int>(totalBytesRead * 100 / m_fileSize);
            if (Progress(progressPercentage, AppSpecific))
                return PVC_CANCELED;
        }
    }

    if (result != VP8_STATUS_OK)
        return PVC_READING_ERROR;

    // convert the decoded image to a bitmap
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
    bmi.bmiHeader.biWidth = m_config.input.width;
    bmi.bmiHeader.biHeight = -m_config.input.height; // Negative height to indicate a top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 32; // 32-bit BGRA
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    if (!hdc)
        return PVC_GDI_ERROR;
    bmp = CreateDIBitmap(hdc, &bmi.bmiHeader, CBM_INIT, m_config.output.u.RGBA.rgba,
                         &bmi, DIB_RGB_COLORS);
    ReleaseDC(nullptr, hdc);
    if (!bmp)
        return PVC_GDI_ERROR;

    // reading done
    if (Progress)
        Progress(100, AppSpecific);
    // image loaded successfully
    return PVC_OK;
}
