// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "heif.h"
#include <fstream>
#include <assert.h>

ImageHeif::~ImageHeif()
{
    if (m_image)
        heif_image_release(m_image);
    if (m_handle)
        heif_image_handle_release(m_handle);
    if (m_ctx)
        heif_context_free(m_ctx);
}

PVCODE ImageHeif::Open(const char* filename, PVImageInfo& pvii)
{
    pvii = {};

    // initialize libheif context
    m_ctx = heif_context_alloc();
    if (!m_ctx)
        return PVC_OUT_OF_MEMORY;
    heif_error err = heif_context_read_from_file(m_ctx, filename, nullptr);
    if (err.code != heif_error_Ok)
    {
        switch (err.code)
        {
        case heif_error_Invalid_input:
            return PVC_UNKNOWN_FILE_STRUCT;
        case heif_error_Memory_allocation_error:
            return PVC_OUT_OF_MEMORY;
        default:
            return PVC_CANNOT_OPEN_FILE;
        }
    }

    // get the primary image handle
    err = heif_context_get_primary_image_handle(m_ctx, &m_handle);
    if (err.code != heif_error_Ok)
        return PVC_UNKNOWN_FILE_STRUCT;

    // fill out the image info
    pvii.cbSize = sizeof(pvii);
    pvii.Format = PVF_BMP;
    pvii.Width = heif_image_handle_get_width(m_handle);
    pvii.Height = heif_image_handle_get_height(m_handle);
    const int luma_bits = heif_image_handle_get_luma_bits_per_pixel(m_handle);
    const int chroma_bits = heif_image_handle_get_chroma_bits_per_pixel(m_handle);
    pvii.Colors = (1 << luma_bits) * (1 << chroma_bits);
    pvii.NumOfImages = 1;

    // compute the number of bytes per line (stride)
    const int bytes_per_pixel = (luma_bits + chroma_bits) / 8; // assuming 24-bit RGB format
    pvii.BytesPerLine = pvii.Width * bytes_per_pixel;
    pvii.Colors = PV_COLOR_TC24;
    pvii.ColorModel = PVCM_RGB;
    pvii.Compression = PVCS_NO_COMPRESSION;

    // get the file size
    if (std::ifstream file(filename, std::ios::binary | std::ios::ate);
        file.is_open())
    {
        file.seekg(0, std::ios::end);
        pvii.FileSize = static_cast<DWORD>(file.tellg());
    }

    strcpy_s(pvii.Info1, "HEIF");

    return PVC_OK;
}

PVCODE ImageHeif::Read(HBITMAP& bmp, TProgressProc Progress, void* AppSpecific)
{
    assert(m_ctx);
    assert(m_handle);

#if 0
    // TODO: decoding progress and cancellation doesn't work yet!
    heif_decoding_options* options = heif_decoding_options_alloc();
    if (!options)
        return PVC_OUT_OF_MEMORY;

    options->version = 1;
    options->start_progress = start_progress;
    options->on_progress = on_progress;
    options->end_progress = end_progress;
    options->cancel_decoding = cancel_decoding;
    options->progress_user_data = &progress_data;
#endif

    // decode the image
    heif_error err = heif_decode_image(m_handle, &m_image, heif_colorspace_RGB, heif_chroma_interleaved_RGB, nullptr);
    if (err.code != heif_error_Ok)
        return PVC_READING_ERROR;

    // check image bit depth
    const int bitDepth = heif_image_get_bits_per_pixel(m_image, heif_channel_interleaved);
    if (bitDepth != 24)
        return PVC_UNSUP_COLOR_DEPTH;

    // get image dimensions
    const int width = heif_image_get_width(m_image, heif_channel_interleaved);
    const int height = heif_image_get_height(m_image, heif_channel_interleaved);

    // get image data
    int stride{};
    const uint8_t* data = heif_image_get_plane_readonly(m_image, heif_channel_interleaved, &stride);
    if (!data)
        return PVC_READING_ERROR;

    // convert the decoded image to a bitmap
    BITMAPINFO bmi = {0};
    bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth = width;
    bmi.bmiHeader.biHeight = -height; // Negative to indicate top-down DIB
    bmi.bmiHeader.biPlanes = 1;
    bmi.bmiHeader.biBitCount = 24; // 24-bit RGB
    bmi.bmiHeader.biCompression = BI_RGB;

    HDC hdc = GetDC(nullptr);
    void* bits = nullptr;
    bmp = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);

    // copy image data to the DIB
    for (int y = 0; y < height; ++y)
    {
        memcpy(static_cast<uint8_t*>(bits) + y * width * 3, data + y * stride, width * 3);
    }

    // image loaded successfully
    return PVC_OK;
}
