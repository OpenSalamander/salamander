// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <cstddef>
#include <expected>
#include <memory>
#include <string>
#include <vector>
#include <functional>

namespace xpm {

enum class Format
{
    Unknown = 0,
    Version2,
    Version3
};

enum class Code
{
    Success = 0,
    OpenFailed,
    InvalidFormat,
    NoMemory,
    DecodeError,
    Cancelled
};

class Image
{
    Image() = default;

public:
    ~Image() = default;

    static std::expected<std::unique_ptr<Image>, Code> Open(const char* file);

    Code Decode(std::function<bool(COLORREF)> appender);

    Format GetFormat() const
    {
        return m_format;
    }

    size_t GetFileSize() const
    {
        return m_fileSize;
    }

    uint32_t GetWidth() const
    {
        return m_width;
    }

    uint32_t GetHeight() const
    {
        return m_height;
    }

    bool CheckImageSize() const;

private:
    Code DoOpen(const char* file);

    Format m_format{Format::Unknown};
    std::vector<char> m_data;
    size_t m_fileSize{};
    uint32_t m_width{};
    uint32_t m_height{};
    uint32_t m_numColors{};
    uint32_t m_charsPerPixel{};
    const char* m_parsePos{};
};

} // namespace xpm
