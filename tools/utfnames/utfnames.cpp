/*
    Demonstration utility for exploring file-system entries whose names are not
    well-formed UTF-16 strings. NTFS stores path components as raw 16-bit units,
    therefore a file can legally contain lone surrogates or other sequences that
    fail Unicode validation. Applications that rely on the Win32 ANSI ("*A") API
    and request the UTF-8 active code page often assume that the operating system
    converts directory listings to UTF-8 without data loss. In reality, Win32 can
    only round-trip valid UTF-16 input; invalid sequences will be normalised or
    substituted when translated to UTF-8, which is a useful discrepancy to study
    when debugging cross-encoding bugs.

    This tool intentionally works at the lowest practical layer so that it can
    create and inspect those problematic names:

      • The "createfile" command builds an empty file whose name is specified as a
        sequence of U+XXXX tokens. Each token is translated to one or more UTF-16
        units and applied via NtSetInformationFile, bypassing Win32 validation and
        allowing arbitrary 16-bit patterns (including lone surrogates).

      • The "createdir" command materialises an empty directory using the same
        token syntax so directory entries with invalid UTF-16 names can be studied.

      • The "list" command shows how the same directory entry appears through the
        ANSI and the wide Win32 APIs. For each variant it prints the Unicode code
        points, the UTF-16 units that back the name, and a preview string rendered
        using a UTF-8 console. When the ANSI view runs with an incompatible active
        code page the output highlights where conversions break down, making it
        clear why strict UTF-8 mode can still misinterpret invalid filenames.

    Usage:
      utfnames list [directory]
        Lists directory entries using both Win32 ANSI and wide APIs. When no
        directory is provided the current working directory is used.

      utfnames createfile <directory> <codepoint> [codepoint...]
        Creates an empty file inside <directory> whose name is composed from the
        provided U+ code point tokens (for example U+0041, U+1F600, U+D800).
        Tokens above U+FFFF expand into surrogate pairs; lone surrogates are
        emitted as-is so invalid UTF-16 names can be materialised on NTFS.

      utfnames createdir <directory> <codepoint> [codepoint...]
        Creates an empty directory inside <directory> whose name is composed from
        the provided tokens, using the same encoding rules as createfile.

    Invalid file names:
      For example, "utfnames createdir . U+0046 U+006F U+006F" builds a valid
      "Foo" directory, while "utfnames createdir . U+0046 U+D800" forces an
      ill-formed name ending with a lone high surrogate.

      High surrogates: U+D800..U+DBFF must be immediately followed by one low
      surrogate. Low surrogates: U+DC00..U+DFFF must be immediately preceded by
      one high surrogate. Any violation (missing, reversed, doubled, or interrupted
      pairing) makes the UTF-16 ill-formed, so conversion to UTF-8 must fail.
*/

#define NOMINMAX
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <windows.h>

#include <cwchar>
#include <cwctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>
#include <locale.h>
#include <string>
#include <vector>

#pragma comment(lib, "ntdll.lib")

extern "C"
{
    typedef LONG NTSTATUS;
    __declspec(dllimport) NTSTATUS __stdcall NtSetInformationFile(
        HANDLE FileHandle,
        struct _IO_STATUS_BLOCK* IoStatusBlock,
        PVOID FileInformation,
        ULONG Length,
        int FileInformationClass);
    __declspec(dllimport) ULONG __stdcall RtlNtStatusToDosError(NTSTATUS Status);
}

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID Pointer;
    } DUMMYUNIONNAME;
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

enum
{
    FileRenameInformation = 10
};

typedef struct _FILE_RENAME_INFORMATION
{
    BOOLEAN ReplaceIfExists;
    BOOLEAN Reserved1[3];
    HANDLE RootDirectory;
    ULONG FileNameLength;
    WCHAR FileName[1];
} FILE_RENAME_INFORMATION, *PFILE_RENAME_INFORMATION;

// Returns an extended-length (\\?\) absolute path.
static std::wstring ToExtendedPath(const std::wstring& absolutePath)
{
    if (absolutePath.rfind(L"\\\\?\\", 0) == 0)
        return absolutePath;

    if (absolutePath.rfind(L"\\\\", 0) == 0)
        return L"\\\\?\\UNC" + absolutePath.substr(1);

    return L"\\\\?\\" + absolutePath;
}

// Convert UTF-16 input to UTF-8 using strict error checking so invalid pairs are visible.
static bool WideToUtf8(const std::wstring& input, std::string& utf8)
{
    int needed = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (needed <= 0)
        return false;

    std::string buffer(static_cast<size_t>(needed), '\0');
    int written = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, input.c_str(), -1,
                                      buffer.data(), needed, nullptr, nullptr);
    if (written <= 0)
        return false;

    buffer.resize(static_cast<size_t>(written > 0 ? written - 1 : 0));
    utf8.swap(buffer);
    return true;
}

// Convert UTF-8 to UTF-16 while rejecting malformed byte sequences.
static bool Utf8ToWide(const std::string& input, std::wstring& wide)
{
    int needed = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), -1, nullptr, 0);
    if (needed <= 0)
        return false;

    std::wstring buffer(static_cast<size_t>(needed), L'\0');
    int written = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, input.c_str(), -1, buffer.data(), needed);
    if (written <= 0)
        return false;

    buffer.resize(static_cast<size_t>(written > 0 ? written - 1 : 0));
    wide.swap(buffer);
    return true;
}

// Wrapper that converts to UTF-8 and reports a contextual error.
static bool WideToUtf8Checked(const std::wstring& value, std::string& utf8, const char* context)
{
    if (!WideToUtf8(value, utf8))
    {
        std::fprintf(stderr, "Failed to convert %s to UTF-8.\n", context);
        return false;
    }
    return true;
}

// Wrapper that converts from UTF-8 and reports a contextual error.
static bool Utf8ToWideChecked(const std::string& value, std::wstring& wide, const char* context)
{
    if (!Utf8ToWide(value, wide))
    {
        std::fprintf(stderr, "Failed to decode UTF-8 %s.\n", context);
        return false;
    }
    return true;
}

static void TrimTrailingNewlines(std::wstring& value)
{
    while (!value.empty() && (value.back() == L'\r' || value.back() == L'\n'))
        value.pop_back();
}

static std::wstring FormatSystemErrorMessageW(DWORD error)
{
    if (error == 0)
        return std::wstring();

    LPWSTR buffer = nullptr;
    DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_SYSTEM |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                  nullptr,
                                  error,
                                  0,
                                  reinterpret_cast<LPWSTR>(&buffer),
                                  0,
                                  nullptr);
    if (length == 0 || buffer == nullptr)
        return std::wstring();

    std::wstring message(buffer, length);
    LocalFree(buffer);
    TrimTrailingNewlines(message);
    return message;
}

static std::string FormatSystemErrorMessageUtf8(DWORD error)
{
    std::wstring wide = FormatSystemErrorMessageW(error);
    if (wide.empty())
        return std::string();

    std::string utf8;
    if (!WideToUtf8(wide, utf8))
        return std::string();
    return utf8;
}

static std::wstring FormatNtStatusMessageW(NTSTATUS status)
{
    HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
    if (!ntdll)
        return std::wstring();

    LPWSTR buffer = nullptr;
    DWORD length = FormatMessageW(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                      FORMAT_MESSAGE_FROM_HMODULE |
                                      FORMAT_MESSAGE_IGNORE_INSERTS,
                                  ntdll,
                                  static_cast<DWORD>(status),
                                  0,
                                  reinterpret_cast<LPWSTR>(&buffer),
                                  0,
                                  nullptr);
    if (length == 0 || buffer == nullptr)
        return std::wstring();

    std::wstring message(buffer, length);
    LocalFree(buffer);
    TrimTrailingNewlines(message);
    return message;
}

// Ensure the process stdio uses UTF-8 so printf-family calls can emit emoji.
static void ConfigureConsoleForUtf8()
{
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
    _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);
    _wsetlocale(LC_ALL, L".UTF-8");
}

// Resolve a potentially relative path and produce both Win32 and extended-length variants.
static bool ResolveFullPath(const std::wstring& input, std::wstring& absoluteOut, std::wstring& extendedOut)
{
    if (!input.empty() && input.rfind(L"\\\\?\\", 0) == 0)
    {
        extendedOut = input;
        if (input.rfind(L"\\\\?\\UNC\\", 0) == 0)
        {
            absoluteOut = L"\\\\";
            absoluteOut += input.substr(8);
        }
        else
        {
            absoluteOut = input.substr(4);
        }
        return true;
    }

    std::wstring candidate = input.empty() ? L"." : input;

    DWORD required = GetFullPathNameW(candidate.c_str(), 0, nullptr, nullptr);
    if (required == 0)
        return false;

    std::vector<wchar_t> buffer(required + 1);
    DWORD written = GetFullPathNameW(candidate.c_str(), static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    if (written == 0 || written >= buffer.size())
        return false;

    absoluteOut.assign(buffer.data(), written);
    extendedOut = ToExtendedPath(absoluteOut);
    return true;
}

// Minimal path join that mirrors Win32 semantics for backslashes.
static std::wstring JoinPath(const std::wstring& base, const std::wstring& leaf)
{
    if (base.empty())
        return leaf;

    if (!leaf.empty() && (leaf.front() == L'\\' || leaf.front() == L'/'))
        return base + leaf;

    std::wstring result = base;
    if (!result.empty() && result.back() != L'\\' && result.back() != L'/')
        result.push_back(L'\\');
    result += leaf;
    return result;
}

static std::string JoinPathUtf8(const std::string& base, const std::string& leaf)
{
    if (base.empty())
        return leaf;

    if (!leaf.empty() && (leaf.front() == '\\' || leaf.front() == '/'))
        return base + leaf;

    std::string result = base;
    if (!result.empty() && result.back() != '\\' && result.back() != '/')
        result.push_back('\\');
    result += leaf;
    return result;
}

static std::wstring MakePrintable(const std::wstring& value)
{
    std::wstring printable;
    printable.reserve(value.size());
    for (size_t i = 0; i < value.size(); ++i)
    {
        wchar_t ch = value[i];
        if (ch == 0)
            break;
        if (ch >= 0xD800 && ch <= 0xDBFF)
        {
            if ((i + 1) < value.size())
            {
                wchar_t next = value[i + 1];
                if (next >= 0xDC00 && next <= 0xDFFF)
                {
                    printable.push_back(ch);
                    printable.push_back(next);
                    ++i;
                    continue;
                }
            }
            printable.push_back(L'?');
            continue;
        }
        if (ch >= 0xDC00 && ch <= 0xDFFF)
        {
            printable.push_back(L'?');
            continue;
        }
        if (ch >= 0x20 && ch != 0x7F)
            printable.push_back(ch);
        else if (ch == L'\n' || ch == L'\r' || ch == L'\t')
            printable.push_back(ch);
        else
            printable.push_back(L'?');
    }
    if (printable.empty())
        printable = L"(empty)";
    return printable;
}

// Format raw 16-bit units as 0xXXXX tokens for logging.
static std::wstring FormatUtf16Units(const std::vector<uint16_t>& units)
{
    if (units.empty())
        return L"(none)";

    std::wstring formatted;
    wchar_t part[16];
    for (size_t i = 0; i < units.size(); ++i)
    {
        if (i != 0)
            formatted.push_back(L' ');
        std::swprintf(part, sizeof(part) / sizeof(part[0]), L"0x%04X", units[i]);
        formatted.append(part);
    }
    return formatted;
}

// Overload accepting a UTF-16 string so callers do not need to split it first.
static std::wstring FormatUtf16Units(const std::wstring& name)
{
    if (name.empty())
        return L"(none)";

    std::wstring formatted;
    wchar_t part[16];
    for (wchar_t ch : name)
    {
        if (ch == 0)
            break;
        if (!formatted.empty())
            formatted.push_back(L' ');
        std::swprintf(part, sizeof(part) / sizeof(part[0]), L"0x%04X", static_cast<uint16_t>(ch));
        formatted.append(part);
    }
    return formatted;
}

// Walk a UTF-16 string and emit a scalar per code point, tolerating lone surrogates.
static std::vector<uint32_t> DecodeCodepoints(const std::wstring& value)
{
    std::vector<uint32_t> codepoints;
    codepoints.reserve(value.size());

    for (size_t i = 0; i < value.size(); ++i)
    {
        uint16_t unit = static_cast<uint16_t>(value[i]);

        if (unit >= 0xD800 && unit <= 0xDBFF)
        {
            if (i + 1 < value.size())
            {
                uint16_t next = static_cast<uint16_t>(value[i + 1]);
                if (next >= 0xDC00 && next <= 0xDFFF)
                {
                    uint32_t high = unit - 0xD800;
                    uint32_t low = next - 0xDC00;
                    uint32_t codepoint = (high << 10) + low + 0x10000;
                    codepoints.push_back(codepoint);
                    ++i;
                    continue;
                }
            }
        }

        codepoints.push_back(unit);
    }

    return codepoints;
}

// Turn Unicode scalar values into U+XXXX tokens.
static std::wstring FormatCodepoints(const std::vector<uint32_t>& codepoints)
{
    if (codepoints.empty())
        return L"(none)";

    std::wstring formatted;
    wchar_t part[16];
    for (size_t i = 0; i < codepoints.size(); ++i)
    {
        if (i != 0)
            formatted.push_back(L' ');

        uint32_t cp = codepoints[i];
        if (cp <= 0xFFFF)
            std::swprintf(part, sizeof(part) / sizeof(part[0]), L"U+%04X", static_cast<unsigned int>(cp));
        else
            std::swprintf(part, sizeof(part) / sizeof(part[0]), L"U+%06X", static_cast<unsigned int>(cp));
        formatted.append(part);
    }
    return formatted;
}

// Describe an UTF-8 buffer as a sequence of byte literals for logging.
static std::string FormatUtf8Bytes(const char* utf8)
{
    if (utf8 == nullptr || *utf8 == '\0')
        return std::string("(none)");

    std::string formatted;
    char part[16];
    for (size_t i = 0; utf8[i] != '\0'; ++i)
    {
        if (i != 0)
            formatted.push_back(' ');
        unsigned char byte = static_cast<unsigned char>(utf8[i]);
        std::snprintf(part, sizeof(part), "0x%02X", byte);
        formatted.append(part);
    }
    return formatted;
}

static std::string FormatUtf8Bytes(const std::string& utf8)
{
    return FormatUtf8Bytes(utf8.c_str());
}

static std::string FormatFileAttributesMask(DWORD attributes)
{
    std::string parts;

    auto appendFlag = [&parts, attributes](DWORD mask, const char* name)
    {
        if ((attributes & mask) == 0)
            return;
        if (!parts.empty())
            parts.push_back('|');
        parts.append(name);
    };

    appendFlag(FILE_ATTRIBUTE_READONLY, "READONLY");
    appendFlag(FILE_ATTRIBUTE_HIDDEN, "HIDDEN");
    appendFlag(FILE_ATTRIBUTE_SYSTEM, "SYSTEM");
    appendFlag(FILE_ATTRIBUTE_DIRECTORY, "DIRECTORY");
    appendFlag(FILE_ATTRIBUTE_ARCHIVE, "ARCHIVE");
    appendFlag(FILE_ATTRIBUTE_DEVICE, "DEVICE");
    appendFlag(FILE_ATTRIBUTE_NORMAL, "NORMAL");
    appendFlag(FILE_ATTRIBUTE_TEMPORARY, "TEMPORARY");
    appendFlag(FILE_ATTRIBUTE_SPARSE_FILE, "SPARSE");
    appendFlag(FILE_ATTRIBUTE_REPARSE_POINT, "REPARSE");
    appendFlag(FILE_ATTRIBUTE_COMPRESSED, "COMPRESSED");
    appendFlag(FILE_ATTRIBUTE_OFFLINE, "OFFLINE");
    appendFlag(FILE_ATTRIBUTE_NOT_CONTENT_INDEXED, "NOT_CONTENT_INDEXED");
    appendFlag(FILE_ATTRIBUTE_ENCRYPTED, "ENCRYPTED");
#ifdef FILE_ATTRIBUTE_INTEGRITY_STREAM
    appendFlag(FILE_ATTRIBUTE_INTEGRITY_STREAM, "INTEGRITY");
#endif
    appendFlag(FILE_ATTRIBUTE_VIRTUAL, "VIRTUAL");
#ifdef FILE_ATTRIBUTE_NO_SCRUB_DATA
    appendFlag(FILE_ATTRIBUTE_NO_SCRUB_DATA, "NO_SCRUB");
#endif
#ifdef FILE_ATTRIBUTE_EA
    appendFlag(FILE_ATTRIBUTE_EA, "EXTENDED_ATTRIBUTES");
#endif
#ifdef FILE_ATTRIBUTE_PINNED
    appendFlag(FILE_ATTRIBUTE_PINNED, "PINNED");
#endif
#ifdef FILE_ATTRIBUTE_UNPINNED
    appendFlag(FILE_ATTRIBUTE_UNPINNED, "UNPINNED");
#endif
#ifdef FILE_ATTRIBUTE_RECALL_ON_OPEN
    appendFlag(FILE_ATTRIBUTE_RECALL_ON_OPEN, "RECALL_ON_OPEN");
#endif
#ifdef FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS
    appendFlag(FILE_ATTRIBUTE_RECALL_ON_DATA_ACCESS, "RECALL_ON_DATA_ACCESS");
#endif

    if (parts.empty())
        parts = "NONE";

    char buffer[20];
    std::snprintf(buffer, sizeof(buffer), "0x%08lX", attributes);
    parts.append(" (");
    parts.append(buffer);
    parts.push_back(')');
    return parts;
}

static NTSTATUS RenameToInvalidUtf16Name(HANDLE hFile, const uint16_t* nameUnits, DWORD nameUnitCount)
{
    const ULONG nameByteLen = nameUnitCount * sizeof(WCHAR);
    const ULONG bufferSize = sizeof(FILE_RENAME_INFORMATION) - sizeof(WCHAR) + nameByteLen;

    std::vector<BYTE> buffer(bufferSize);
    auto fri = reinterpret_cast<FILE_RENAME_INFORMATION*>(buffer.data());
    ZeroMemory(fri, bufferSize);

    fri->ReplaceIfExists = FALSE;
    fri->RootDirectory = nullptr;
    fri->FileNameLength = nameByteLen;
    memcpy(fri->FileName, nameUnits, nameByteLen);

    IO_STATUS_BLOCK iosb{};
    return NtSetInformationFile(hFile, &iosb, fri, bufferSize, FileRenameInformation);
}

static bool RenamePathComponent(const std::wstring& win32Path,
                                const std::vector<uint16_t>& newNameUnits,
                                bool isDirectory)
{
    DWORD flags = FILE_ATTRIBUTE_NORMAL |
                  (isDirectory ? FILE_FLAG_BACKUP_SEMANTICS : 0);

    HANDLE h = CreateFileW(win32Path.c_str(),
                           DELETE | SYNCHRONIZE | FILE_WRITE_ATTRIBUTES,
                           FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           nullptr,
                           OPEN_EXISTING,
                           flags,
                           nullptr);

    if (h == INVALID_HANDLE_VALUE)
    {
        DWORD lastError = GetLastError();
        std::wstring message = FormatSystemErrorMessageW(lastError);
        if (!message.empty())
            std::fwprintf(stderr, L"CreateFileW failed (%lu: %ls)\n", lastError, message.c_str());
        else
            std::fwprintf(stderr, L"CreateFileW failed (%lu)\n", lastError);
        return false;
    }

    NTSTATUS status = RenameToInvalidUtf16Name(h, newNameUnits.data(),
                                               static_cast<DWORD>(newNameUnits.size()));
    CloseHandle(h);

    if (status < 0)
    {
        DWORD w32 = RtlNtStatusToDosError(status);
        std::wstring winMessage = FormatSystemErrorMessageW(w32);
        std::wstring ntMessage = FormatNtStatusMessageW(status);
        if (!ntMessage.empty() && !winMessage.empty())
            std::fwprintf(stderr, L"NtSetInformationFile failed: NTSTATUS=0x%08X (%ls) (Win32=%lu: %ls)\n",
                          status, ntMessage.c_str(), w32, winMessage.c_str());
        else if (!ntMessage.empty())
            std::fwprintf(stderr, L"NtSetInformationFile failed: NTSTATUS=0x%08X (%ls) (Win32=%lu)\n",
                          status, ntMessage.c_str(), w32);
        else if (!winMessage.empty())
            std::fwprintf(stderr, L"NtSetInformationFile failed: NTSTATUS=0x%08X (Win32=%lu: %ls)\n",
                          status, w32, winMessage.c_str());
        else
            std::fwprintf(stderr, L"NtSetInformationFile failed: NTSTATUS=0x%08X (Win32=%lu)\n",
                          status, w32);
        SetLastError(w32);
        return false;
    }
    return true;
}

// Build a unique temp filename so we can rename it to the requested pattern.
static std::wstring GeneratePlaceholderName()
{
    SYSTEMTIME st{};
    GetSystemTime(&st);

    wchar_t buffer[64];
    std::swprintf(buffer, sizeof(buffer) / sizeof(buffer[0]),
                  L"utfnames_tmp_%lu_%04u%02u%02u%02u%02u%02u.tmp",
                  GetCurrentProcessId(),
                  st.wYear, st.wMonth, st.wDay,
                  st.wHour, st.wMinute, st.wSecond);
    return buffer;
}

// Dump directory contents via both ANSI and wide Win32 APIs without tolerating conversion errors.
static bool ListDirectory(const std::wstring& directoryArg)
{
    ConfigureConsoleForUtf8();

    std::wstring absoluteDir;
    std::wstring extendedDir;
    if (!ResolveFullPath(directoryArg, absoluteDir, extendedDir))
    {
        std::fwprintf(stderr, L"Unable to resolve path: %ls\n", directoryArg.c_str());
        return false;
    }

    DWORD attr = GetFileAttributesW(extendedDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        std::fwprintf(stderr, L"Not a directory: %ls\n", absoluteDir.c_str());
        return false;
    }

    std::wprintf(L"Listing for %ls\n", absoluteDir.c_str());

    std::wstring searchPatternWide = JoinPath(absoluteDir, L"*");
    std::string patternUtf8;
    if (!WideToUtf8Checked(searchPatternWide, patternUtf8, "ANSI search pattern"))
        return false;

    WIN32_FIND_DATAA dataA{};
    HANDLE findA = FindFirstFileA(patternUtf8.c_str(), &dataA);
    if (findA == INVALID_HANDLE_VALUE)
    {
        DWORD lastError = GetLastError();
        std::string message = FormatSystemErrorMessageUtf8(lastError);
        if (!message.empty())
            std::fprintf(stderr, "FindFirstFileA failed (%lu: %s)\n", lastError, message.c_str());
        else
            std::fprintf(stderr, "FindFirstFileA failed (%lu)\n", lastError);
        return false;
    }

    UINT acp = GetACP();
    if (acp != CP_UTF8)
    {
        std::fprintf(stderr, "Process ANSI code page is %u; UTF-8 required.\n", acp);
        FindClose(findA);
        return false;
    }

    std::string absoluteDirUtf8;
    if (!WideToUtf8Checked(absoluteDir, absoluteDirUtf8, "ANSI base path"))
    {
        FindClose(findA);
        return false;
    }

    std::printf("[UTF-8]\n");
    do
    {
        if (std::strcmp(dataA.cFileName, ".") == 0 || std::strcmp(dataA.cFileName, "..") == 0)
            continue;

        bool isDir = (dataA.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        std::string utf8Name(dataA.cFileName);
        std::printf("  [%s] %s\n", isDir ? "DIR" : "FILE", utf8Name.c_str());

        std::wstring wideName;
        if (!Utf8ToWideChecked(utf8Name, wideName, "directory entry"))
        {
            FindClose(findA);
            return false;
        }

        std::wstring printable = MakePrintable(wideName);
        std::wstring codepointsWide = FormatCodepoints(DecodeCodepoints(wideName));
        std::wstring utf16UnitsWide = FormatUtf16Units(wideName);

        std::string codepointsUtf8;
        std::string utf16UnitsUtf8;
        std::string previewUtf8;
        if (!WideToUtf8Checked(codepointsWide, codepointsUtf8, "code points") ||
            !WideToUtf8Checked(utf16UnitsWide, utf16UnitsUtf8, "UTF-16 encoding") ||
            !WideToUtf8Checked(printable, previewUtf8, "preview string"))
        {
            FindClose(findA);
            return false;
        }

        std::string utf8Bytes = FormatUtf8Bytes(utf8Name);
        std::printf("    code points: %s\n", codepointsUtf8.c_str());
        std::printf("    utf-8 encoding: %s\n", utf8Bytes.c_str());
        std::printf("    utf-16 encoding (from utf-8): %s\n", utf16UnitsUtf8.c_str());
        std::printf("    utf-16 preview: %s\n", previewUtf8.c_str());

        std::string ansiPath = JoinPathUtf8(absoluteDirUtf8, utf8Name);
        WIN32_FILE_ATTRIBUTE_DATA attrData{};
        if (GetFileAttributesExA(ansiPath.c_str(), GetFileExInfoStandard, &attrData))
        {
            DWORD attributes = attrData.dwFileAttributes;
            bool dirByAttributes = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            std::string attrText = FormatFileAttributesMask(attributes);
            if (dirByAttributes)
                std::printf("    utf-8 access: directory, attributes %s\n", attrText.c_str());
            else
                std::printf("    utf-8 access: ok, attributes %s\n", attrText.c_str());
        }
        else
        {
            DWORD lastError = GetLastError();
            std::string message = FormatSystemErrorMessageUtf8(lastError);
            if (!message.empty())
                std::printf("    utf-8 access: failed (%lu: %s)\n", lastError, message.c_str());
            else
                std::printf("    utf-8 access: failed (%lu)\n", lastError);
        }
    } while (FindNextFileA(findA, &dataA));

    DWORD errA = GetLastError();
    FindClose(findA);
    if (errA != ERROR_NO_MORE_FILES)
    {
        std::string message = FormatSystemErrorMessageUtf8(errA);
        if (!message.empty())
            std::fprintf(stderr, "FindNextFileA failed (%lu: %s)\n", errA, message.c_str());
        else
            std::fprintf(stderr, "FindNextFileA failed (%lu)\n", errA);
        return false;
    }

    std::wstring patternW = JoinPath(extendedDir, L"*");
    WIN32_FIND_DATAW dataW{};
    HANDLE findW = FindFirstFileW(patternW.c_str(), &dataW);
    if (findW == INVALID_HANDLE_VALUE)
    {
        DWORD lastError = GetLastError();
        std::wstring message = FormatSystemErrorMessageW(lastError);
        if (!message.empty())
            std::fwprintf(stderr, L"FindFirstFileW failed (%lu: %ls)\n", lastError, message.c_str());
        else
            std::fwprintf(stderr, L"FindFirstFileW failed (%lu)\n", lastError);
        return false;
    }

    std::wprintf(L"\n[UTF-16]\n");
    do
    {
        std::wstring name(dataW.cFileName);
        if (name == L"." || name == L"..")
            continue;

        bool isDir = (dataW.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        std::wstring printable = MakePrintable(name);
        std::wstring codepoints = FormatCodepoints(DecodeCodepoints(name));
        std::wstring units = FormatUtf16Units(name);

        std::string printableUtf8;
        std::string codepointsUtf8;
        std::string unitsUtf8;
        if (!WideToUtf8Checked(printable, printableUtf8, "UTF-16 preview") ||
            !WideToUtf8Checked(codepoints, codepointsUtf8, "UTF-16 code points") ||
            !WideToUtf8Checked(units, unitsUtf8, "UTF-16 encoding"))
        {
            FindClose(findW);
            return false;
        }

        std::printf("  [%s] %s\n", isDir ? "DIR" : "FILE", printableUtf8.c_str());
        std::printf("    code points: %s\n", codepointsUtf8.c_str());
        std::printf("    utf-16 encoding: %s\n", unitsUtf8.c_str());

        std::wstring widePath = JoinPath(absoluteDir, name);
        WIN32_FILE_ATTRIBUTE_DATA attrDataW{};
        if (GetFileAttributesExW(widePath.c_str(), GetFileExInfoStandard, &attrDataW))
        {
            DWORD attributes = attrDataW.dwFileAttributes;
            bool dirByAttributes = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            std::string attrText = FormatFileAttributesMask(attributes);
            if (dirByAttributes)
                std::printf("    utf-16 access: directory, attributes %s\n", attrText.c_str());
            else
                std::printf("    utf-16 access: ok, attributes %s\n", attrText.c_str());
        }
        else
        {
            DWORD lastError = GetLastError();
            std::string message = FormatSystemErrorMessageUtf8(lastError);
            if (!message.empty())
                std::printf("    utf-16 access: failed (%lu: %s)\n", lastError, message.c_str());
            else
                std::printf("    utf-16 access: failed (%lu)\n", lastError);
        }
    } while (FindNextFileW(findW, &dataW));

    DWORD errW = GetLastError();
    FindClose(findW);
    if (errW != ERROR_NO_MORE_FILES)
    {
        std::wstring message = FormatSystemErrorMessageW(errW);
        if (!message.empty())
            std::fwprintf(stderr, L"FindNextFileW failed (%lu: %ls)\n", errW, message.c_str());
        else
            std::fwprintf(stderr, L"FindNextFileW failed (%lu)\n", errW);
        return false;
    }

    return true;
}

// Translate command-line U+XXXX tokens into raw UTF-16 units without validation.
static bool ParseCodepoints(int argc, wchar_t* argv[], int startIndex, std::vector<uint16_t>& units)
{
    for (int i = startIndex; i < argc; ++i)
    {
        std::wstring token = argv[i];
        while (!token.empty() && (token.back() == L',' || token.back() == L';'))
            token.pop_back();
        if (token.empty())
            continue;

        if (token.size() < 3 || !((token[0] == L'U') || (token[0] == L'u')) || token[1] != L'+')
        {
            std::fwprintf(stderr, L"Tokens must use U+ notation (e.g., U+0041): %ls\n", argv[i]);
            return false;
        }

        std::wstring digits = token.substr(2);
        if (digits.empty())
        {
            std::fwprintf(stderr, L"Cannot parse code point token: %ls\n", argv[i]);
            return false;
        }

        wchar_t* endPtr = nullptr;
        unsigned long value = std::wcstoul(digits.c_str(), &endPtr, 16);
        if (endPtr == nullptr || *endPtr != L'\0')
        {
            std::fwprintf(stderr, L"Invalid hexadecimal value: %ls\n", argv[i]);
            return false;
        }

        if (value > 0x10FFFF)
        {
            std::fwprintf(stderr, L"Code point out of range (max U+10FFFF): %ls\n", argv[i]);
            return false;
        }

        if (value <= 0xFFFF)
        {
            units.push_back(static_cast<uint16_t>(value));
        }
        else
        {
            value -= 0x10000;
            units.push_back(static_cast<uint16_t>(0xD800 + (value >> 10)));
            units.push_back(static_cast<uint16_t>(0xDC00 + (value & 0x3FF)));
        }
    }

    if (units.empty())
    {
        std::fwprintf(stderr, L"At least one code point must be specified.\n");
        return false;
    }
    return true;
}

// Create a placeholder entry (file or directory) and rename it to contain the caller-specified units.
static bool CreateInvalidEntry(const std::wstring& directoryArg,
                               const std::vector<uint16_t>& units,
                               bool createDirectory)
{
    ConfigureConsoleForUtf8();

    std::wstring absoluteDir;
    std::wstring extendedDir;
    if (!ResolveFullPath(directoryArg, absoluteDir, extendedDir))
    {
        std::fwprintf(stderr, L"Unable to resolve path: %ls\n", directoryArg.c_str());
        return false;
    }

    DWORD attr = GetFileAttributesW(extendedDir.c_str());
    if (attr == INVALID_FILE_ATTRIBUTES || !(attr & FILE_ATTRIBUTE_DIRECTORY))
    {
        std::fwprintf(stderr, L"Target is not a directory: %ls\n", absoluteDir.c_str());
        return false;
    }

    std::wstring placeholderName = GeneratePlaceholderName();
    std::wstring placeholderExtended = JoinPath(extendedDir, placeholderName);

    if (createDirectory)
    {
        if (!CreateDirectoryW(placeholderExtended.c_str(), nullptr))
        {
            DWORD lastError = GetLastError();
            std::wstring message = FormatSystemErrorMessageW(lastError);
            if (!message.empty())
                std::fwprintf(stderr, L"CreateDirectoryW failed for placeholder (%lu: %ls)\n", lastError, message.c_str());
            else
                std::fwprintf(stderr, L"CreateDirectoryW failed for placeholder (%lu)\n", lastError);
            return false;
        }
    }
    else
    {
        HANDLE tempHandle = CreateFileW(placeholderExtended.c_str(),
                                        GENERIC_READ | GENERIC_WRITE,
                                        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                        nullptr,
                                        CREATE_NEW,
                                        FILE_ATTRIBUTE_NORMAL,
                                        nullptr);
        if (tempHandle == INVALID_HANDLE_VALUE)
        {
            DWORD lastError = GetLastError();
            std::wstring message = FormatSystemErrorMessageW(lastError);
            if (!message.empty())
                std::fwprintf(stderr, L"CreateFileW failed for placeholder (%lu: %ls)\n", lastError, message.c_str());
            else
                std::fwprintf(stderr, L"CreateFileW failed for placeholder (%lu)\n", lastError);
            return false;
        }
        CloseHandle(tempHandle);
    }

    if (!RenamePathComponent(placeholderExtended, units, createDirectory))
    {
        if (createDirectory)
            RemoveDirectoryW(placeholderExtended.c_str());
        else
            DeleteFileW(placeholderExtended.c_str());
        return false;
    }

    std::wstring namePreview(units.begin(), units.end());
    std::wstring sanitized = MakePrintable(namePreview);
    std::wstring unitString = FormatUtf16Units(units);
    std::wstring codepoints = FormatCodepoints(DecodeCodepoints(namePreview));

    const wchar_t* entryKind = createDirectory ? L"directory" : L"file";
    std::wprintf(L"Created %ls in %ls\n  code points: %ls\n  utf-16 units: %ls\n  preview: %ls\n",
                 entryKind,
                 absoluteDir.c_str(),
                 codepoints.c_str(),
                 unitString.c_str(),
                 sanitized.c_str());
    return true;
}

static bool CreateInvalidFile(const std::wstring& directoryArg, const std::vector<uint16_t>& units)
{
    return CreateInvalidEntry(directoryArg, units, false);
}

static bool CreateInvalidDirectory(const std::wstring& directoryArg, const std::vector<uint16_t>& units)
{
    return CreateInvalidEntry(directoryArg, units, true);
}

static void PrintUsage()
{
    std::fwprintf(stderr,
                  L"Usage:\n"
                  L"  utfnames list [directory]\n"
                  L"  utfnames createfile <directory> <codepoint> [codepoint...]\n"
                  L"  utfnames createdir <directory> <codepoint> [codepoint...]\n"
                  L"    codepoint examples: U+0041 U+D800 U+1F600\n");
}

// Entry point dispatches the subcommands defined above.
int wmain(int argc, wchar_t* argv[])
{
    if (argc < 2)
    {
        PrintUsage();
        return 1;
    }

    std::wstring command = argv[1];
    for (auto& ch : command)
        ch = static_cast<wchar_t>(std::towlower(ch));

    if (command == L"list")
    {
        std::wstring directory = (argc >= 3) ? argv[2] : L".";
        return ListDirectory(directory) ? 0 : 1;
    }
    else if (command == L"createfile" || command == L"createdir")
    {
        if (argc < 4)
        {
            std::fwprintf(stderr, L"%ls requires a directory and at least one code point.\n", command.c_str());
            PrintUsage();
            return 1;
        }

        std::vector<uint16_t> units;
        if (!ParseCodepoints(argc, argv, 3, units))
            return 1;

        bool createDir = (command == L"createdir");
        return createDir ? (CreateInvalidDirectory(argv[2], units) ? 0 : 1)
                         : (CreateInvalidFile(argv[2], units) ? 0 : 1);
    }

    std::fwprintf(stderr, L"Unknown command: %ls\n", argv[1]);
    PrintUsage();
    return 1;
}
