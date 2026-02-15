// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "xpm-lite.h"
#include "x11-colormap.h"

#include <cctype>
#include <cstdio>
#include <unordered_map>

#undef new
#undef min

using namespace xpm;

// ---------------------------------------------------------------------------
// Local helpers
// ---------------------------------------------------------------------------

inline bool isspace(char c)
{
    return std::isspace(static_cast<int>(c)) != 0;
}

inline bool isxdigit(char c)
{
    return std::isxdigit(static_cast<int>(c)) != 0;
}

/// Advance past the current line (including CR, LF, or CRLF).
static const char* NextLine(const char* p)
{
    while (*p && *p != '\n' && *p != '\r')
        ++p;
    if (*p == '\r' && *(p + 1) == '\n')
        p += 2;
    else if (*p == '\r' || *p == '\n')
        ++p;
    return p;
}

/// Skip sequences of CR/LF (blank lines).
static const char* SkipBlankLines(const char* p)
{
    while (*p == '\r' || *p == '\n')
        ++p;
    return p;
}

/// Return the numeric value of a single hex digit.
static uint8_t HexVal(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return 10 + c - 'a';
    if (c >= 'A' && c <= 'F')
        return 10 + c - 'A';
    return 0;
}

/// Parse a hex color string.
static bool ParseHexColor(std::string_view str, COLORREF& color)
{
    if (str.length() == 0 || str[0] != '#')
        return false;
    str.remove_prefix(1); // skip '#'

    // Count hex digits length
    size_t len = 0;
    for (const auto c : str)
    {
        if (!isxdigit(c))
            break;
        ++len;
    }

    uint8_t r, g, b;

    if (len == 6)
    {
        // #RRGGBB
        r = (HexVal(str[0]) << 4) | HexVal(str[1]);
        g = (HexVal(str[2]) << 4) | HexVal(str[3]);
        b = (HexVal(str[4]) << 4) | HexVal(str[5]);
    }
    else if (len == 3)
    {
        // #RGB – expand each nibble to a full byte
        r = (HexVal(str[0]) << 4) | HexVal(str[0]);
        g = (HexVal(str[1]) << 4) | HexVal(str[1]);
        b = (HexVal(str[2]) << 4) | HexVal(str[2]);
    }
    else if (len == 9)
    {
        // #RRRGGGBBB – use upper 8 bits of each 12-bit component
        r = (HexVal(str[0]) << 4) | HexVal(str[1]);
        g = (HexVal(str[3]) << 4) | HexVal(str[4]);
        b = (HexVal(str[6]) << 4) | HexVal(str[7]);
    }
    else if (len == 12)
    {
        // #RRRRGGGGBBBB – use upper 8 bits of each 16-bit component
        r = (HexVal(str[0]) << 4) | HexVal(str[1]);
        g = (HexVal(str[4]) << 4) | HexVal(str[5]);
        b = (HexVal(str[8]) << 4) | HexVal(str[9]);
    }
    else
    {
        return false;
    }

    color = RGB(r, g, b);
    return true;
}

/// Extract the color-value substring that follows a " c " key inside a color
/// definition line.  The value extends until the next visual-type key
/// (" m ", " s ", " g ", " g4 ") or end of string.
static std::string_view ExtractColorValue(std::string_view line, size_t start)
{
    size_t end = start;
    while (end < line.size())
    {
        char ch = line[end];
        if (ch == ' ' || ch == '\t')
        {
            const size_t next = end + 1;
            if (next < line.size())
            {
                const char k = line[next];
                const size_t afterKey = next + 1;

                // Single-char keys: m, s, c
                if ((k == 'm' || k == 's' || k == 'c') &&
                    afterKey < line.size() &&
                    (line[afterKey] == ' ' || line[afterKey] == '\t'))
                    break;

                // 'g' or 'g4'
                if (k == 'g')
                {
                    if (afterKey < line.size() && line[afterKey] == '4')
                    {
                        const size_t afterG4 = afterKey + 1;
                        if (afterG4 < line.size() &&
                            (line[afterG4] == ' ' || line[afterG4] == '\t'))
                            break;
                    }
                    else if (afterKey < line.size() &&
                             (line[afterKey] == ' ' || line[afterKey] == '\t'))
                        break;
                }
            }
        }
        ++end;
    }

    std::string_view value = line.substr(start, end - start);

    // Trim trailing whitespace
    while (!value.empty() && isspace(value.back()))
        value.remove_suffix(1);
    return value;
}

/// Parse a color specifier string (hex, "None", or X11 name).
static bool ParseColorSpec(std::string_view spec, COLORREF& color)
{
    if (spec.size() == 0)
        return false;

    // Hex color
    if (spec[0] == '#')
        return ParseHexColor(spec, color);

    // Transparent (temporary white color used)
    static constexpr const char* TRANSPARENT_COLOR = "None";
    const auto TRANSPARENT_COLOR_LEN = std::string::traits_type::length(TRANSPARENT_COLOR);
    if (spec.size() == TRANSPARENT_COLOR_LEN && _strnicmp(spec.data(), TRANSPARENT_COLOR, TRANSPARENT_COLOR_LEN) == 0)
    {
        spec = "White";
    }

    // Convert X11 named color to COLORREF
    return LookupX11Color(spec, color);
}

/// Locate the position of a " c " (color) key inside a line string.
/// Tries common whitespace combinations around the key.
static size_t FindColorKey(std::string_view line)
{
    static const char* patterns[] = {" c ", "\tc ", " c\t", "\tc\t"};
    for (const auto pat : patterns)
    {
        const auto pos = line.find(pat);
        if (pos != std::string::npos)
            return pos;
    }
    return std::string::npos;
}

/// Parse one color-definition line (the text between quotes for XPM3,
/// or the plain-text line for XPM2) and return the character key and
/// resolved Color.
static bool ParseColorDef(std::string_view line, uint32_t cpp,
                           std::string& outKey, COLORREF& outColor)
{
    if (line.size() < cpp)
        return false;

    outKey.assign(line.data(), cpp);

    const auto tail = line.substr(cpp);
    const size_t cpos = FindColorKey(tail);

    if (cpos != std::string::npos)
    {
        size_t specStart = cpp + cpos + 3; // skip past " c "
        while (specStart < line.size() &&
               isspace(line[specStart]))
            ++specStart;

        const auto colorValue = ExtractColorValue(line, specStart);
        if (!colorValue.empty())
            return ParseColorSpec(colorValue, outColor);
    }

    return true;
}

// ---------------------------------------------------------------------------
// Image public interface
// ---------------------------------------------------------------------------

std::expected<std::unique_ptr<Image>, Code> Image::Open(const char* file)
{
    auto img = std::unique_ptr<Image>(new Image());
    const auto code = img->DoOpen(file);
    if (code != Code::Success)
        return std::unexpected(code);
    return img;
}

// ---------------------------------------------------------------------------
// DoOpen – phase 1: read file, detect version, parse header values
// ---------------------------------------------------------------------------

Code Image::DoOpen(const char* file)
{
    // Open and read the entire file into memory
    FILE* f = nullptr;
    if (fopen_s(&f, file, "rb") != 0 || !f)
        return Code::OpenFailed;

    fseek(f, 0, SEEK_END);
    const long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0)
    {
        fclose(f);
        return Code::OpenFailed;
    }

    m_fileSize = static_cast<size_t>(size);

    try
    {
        m_data.resize(m_fileSize + 1);
    }
    catch (...)
    {
        fclose(f);
        return Code::NoMemory;
    }

    const size_t bytesRead = fread(m_data.data(), 1, m_fileSize, f);
    fclose(f);

    if (bytesRead != m_fileSize)
        return Code::OpenFailed;

    // This is expected to be a text file, make it a zero-terminated string.
    m_data[m_fileSize] = '\0';

    // --- Detect format and parse the header (values) line ---

    const char* ptr = m_data.data();

    // Skip leading whitespace
    while (*ptr && isspace(*ptr))
        ++ptr;

    static constexpr const char* XPM2_HEADER = "! XPM2";
    static constexpr const char* XPM3_HEADER = "/* XPM */";
    constexpr size_t XPM2_HEADER_LEN = std::string::traits_type::length(XPM2_HEADER);
    constexpr size_t XPM3_HEADER_LEN = std::string::traits_type::length(XPM3_HEADER);

    // NOTE: XPM version 1 is not supported!

    if (m_fileSize > XPM2_HEADER_LEN && strncmp(ptr, XPM2_HEADER, XPM2_HEADER_LEN) == 0)
    {
        // ---- XPM version 2 ----
        m_format = Format::Version2;

        // Skip past the header identifier line
        ptr = NextLine(ptr);
        ptr = SkipBlankLines(ptr);

        // Values line: <width> <height> <ncolors> <cpp>
        if (sscanf_s(ptr, "%d %d %d %d", &m_width, &m_height, &m_numColors, &m_charsPerPixel) != 4)
            return Code::InvalidFormat;

        if (m_width == 0 || m_height == 0 || m_numColors == 0 || m_charsPerPixel == 0)
            return Code::InvalidFormat;

        ptr = NextLine(ptr);
        m_parsePos = ptr; // colour definitions start here
        return Code::Success;
    }
    else if (m_fileSize > XPM3_HEADER_LEN && strncmp(ptr, XPM3_HEADER, XPM3_HEADER_LEN) == 0)
    {
        // ---- XPM version 3 ----
        const char* brace = strchr(ptr, '{');
        if (!brace)
            return Code::InvalidFormat;

        m_format = Format::Version3;

        ptr = brace + 1;

        // Find the first quoted string – this is the header / values string
        while (*ptr && *ptr != '"')
            ++ptr;
        if (!*ptr)
            return Code::InvalidFormat;
        ++ptr; // skip opening quote

        if (sscanf_s(ptr, "%d %d %d %d", &m_width, &m_height, &m_numColors, &m_charsPerPixel) != 4)
            return Code::InvalidFormat;

        if (m_width == 0 || m_height == 0 || m_numColors == 0 || m_charsPerPixel == 0)
            return Code::InvalidFormat;

        // Advance past the closing quote of the header string
        ptr = strchr(ptr, '"');
        if (!ptr)
            return Code::InvalidFormat;
        ++ptr;

        m_parsePos = ptr; // colour definition strings follow
        return Code::Success;
    }

    return Code::InvalidFormat; // unrecognized format
}

// Check if image size may cause overflow
bool Image::CheckImageSize() const
{
    return !(m_height != 0 && m_width > UINT_MAX / m_height);
}

// ---------------------------------------------------------------------------
// Decode – phase 2: parse colour table and pixel data
// ---------------------------------------------------------------------------

Code Image::Decode(std::function<bool(COLORREF)> appender)
{
    if (m_format == Format::Unknown || !m_parsePos)
        return Code::DecodeError;

    if (!CheckImageSize())
        return Code::NoMemory;

    const char* ptr = m_parsePos;

    // =====================================================================
    // Phase 2a – Parse the colour table
    // =====================================================================

    std::unordered_map<std::string, COLORREF> colorMap;
    try
    {
        colorMap.reserve(m_numColors);
    }
    catch (...)
    {
        return Code::NoMemory;
    }

    if (m_format == Format::Version2)
    {
        // XPM2: each colour definition is a plain-text line
        for (uint32_t i = 0; i < m_numColors;)
        {
            ptr = SkipBlankLines(ptr);
            if (!*ptr)
                return Code::DecodeError;

            // Skip comments
            while (*ptr == '!')
            {
                ptr = NextLine(ptr);
            }

            const char* lineStart = ptr;
            const char* lineEnd = ptr;
            while (*lineEnd && *lineEnd != '\n' && *lineEnd != '\r')
                ++lineEnd;
            std::string_view line(lineStart, lineEnd);

            // Skip empty lines (shouldn't happen after SkipBlankLines, but be safe)
            if (line.size() == 0)
            {
                ptr = NextLine(lineStart);
                continue;
            }

            std::string key;
            COLORREF color{};
            if (!ParseColorDef(line, m_charsPerPixel, key, color))
                return Code::DecodeError;

            colorMap[std::move(key)] = color;
            ++i;

            ptr = NextLine(lineStart);
        }
    }
    else // Version3
    {
        // XPM3: each colour definition is a quoted string
        for (uint32_t i = 0; i < m_numColors; ++i)
        {
            // Find opening quote
            while (*ptr && *ptr != '"')
                ++ptr;
            if (!*ptr)
                return Code::DecodeError;
            ++ptr; // skip opening quote

            const char* lineStart = ptr;
            const char* lineEnd = strchr(ptr, '"');
            if (!lineEnd)
                return Code::DecodeError;
            std::string_view line(lineStart, lineEnd);

            std::string key;
            COLORREF color{};
            if (!ParseColorDef(line, m_charsPerPixel, key, color))
                return Code::DecodeError;

            colorMap[std::move(key)] = color;

            ptr = lineEnd + 1; // past closing quote
        }
    }

    // Ensure that all colors were read
    if (m_numColors != colorMap.size())
        return Code::DecodeError;

    // =====================================================================
    // Phase 2b – Decode pixel data
    // =====================================================================

    for (uint32_t y = 0; y < m_height; ++y)
    {
        const char* rowStart = nullptr;

        if (m_format == Format::Version2)
        {
            // Plain-text lines – skip blank / comment lines (starting with '!')
            for (;;)
            {
                ptr = SkipBlankLines(ptr);
                if (!*ptr)
                    return Code::DecodeError;

                if (*ptr == '!')
                {
                    ptr = NextLine(ptr);
                    continue;
                }
                break;
            }
            rowStart = ptr;
        }
        else // Version3
        {
            // Find the next quoted string
            while (*ptr && *ptr != '"')
                ++ptr;
            if (!*ptr)
                return Code::DecodeError;
            ++ptr; // skip opening quote
            rowStart = ptr;
        }

        // Decode all pixels in this row
        const char* px = rowStart;
        for (uint32_t x = 0; x < m_width; ++x, px += m_charsPerPixel)
        {
            const std::string key(px, m_charsPerPixel);
            auto it = colorMap.find(key);
            if (it == std::end(colorMap))
                return Code::DecodeError;

            // append the next pixel to the consumer
            if (!appender(it->second))
                return Code::Cancelled;
        }

        // Advance past this row
        if (m_format == Format::Version2)
        {
            ptr = NextLine(rowStart);
        }
        else
        {
            // Skip past the closing quote
            const char* endQuote = strchr(rowStart, '"');
            ptr = endQuote ? endQuote + 1 : px;
        }
    }

    return Code::Success;
}
