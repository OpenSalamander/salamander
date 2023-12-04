// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#ifndef QWORD
typedef unsigned __int64 QWORD;
typedef QWORD* LPQWORD;
#define LODWORD(qw) ((DWORD)qw)
#define HIDWORD(qw) ((DWORD)(qw >> 32))
#define MAKEQWORD(lo, hi) (((QWORD)hi << 32) + lo)
#endif

BOOL PathAppend(WCHAR* path, WCHAR* more, int pathSize);
BOOL CutDirectory(WCHAR* path, WCHAR* cutDir = NULL, int size = 0);
WCHAR* DupStr(const WCHAR* str);
char* DupStrA(const WCHAR* str);
char* StrNCat(char* dest, const char* sour, int destSize);

inline DWORD WStrToStr(char* dest, int destSize, const WCHAR* sour, int sourLen)
{
    int ret = WideCharToMultiByte(CP_ACP, 0, sour, sourLen, dest, destSize, NULL, NULL);
    if (ret == 0 && destSize > 0)
        dest[destSize - 1] = 0;
    return ret;
}

inline DWORD WStrToStr(char* dest, int destSize, const WCHAR* sour)
{
    return WStrToStr(dest, destSize, sour, -1);
}

inline DWORD StrToWStr(WCHAR* dest, int destSize, const char* sour, int sourLen)
{
    int res = MultiByteToWideChar(CP_ACP, 0, sour, sourLen, dest, destSize);
    if (res == 0 && destSize > 0)
        dest[destSize - 1] = 0;
    return res;
}

inline DWORD StrToWStr(WCHAR* dest, int destSize, const char* sour)
{
    return StrToWStr(dest, destSize, sour, -1);
}

void RemoveTrailingSlashes(char* path);
void RemoveTrailingSlashes(LPWSTR path);

BOOL RegOperationError(int lastError, int error, int title, int keyRoot, LPWSTR keyName,
                       LPBOOL skip, LPBOOL skipAllErrors);

void LoadHistory(HKEY regKey, const char* keyPattern, LPWSTR* history,
                 LPWSTR buffer, int bufferSize, CSalamanderRegistryAbstract* registry);
void SaveHistory(HKEY regKey, const char* keyPattern, LPWSTR* history,
                 CSalamanderRegistryAbstract* registry);

BOOL TestForCancel();

BOOL DuplicateChar(WCHAR dup, LPWSTR buffer, int bufferSize);
LPWSTR UnDuplicateChar(WCHAR dup, LPWSTR buffer);

BOOL ParseFullPath(WCHAR* path, WCHAR*& keyName, int& keyRoot);

void ConvertHexToString(LPWSTR text, char* hex, int& len);
BOOL ValidateHexString(LPWSTR text);

// ****************************************************************************

class CBuffer
{
public:
    CBuffer()
    {
        Buffer = NULL;
        Allocated = 0;
    }
    ~CBuffer() { Release(); }
    BOOL Reserve(int size);
    void Release()
    {
        if (Buffer)
            free(Buffer);
        Buffer = NULL;
        Allocated = 0;
    }
    int GetSize() { return Allocated; }

protected:
    void* Buffer;
    int Allocated;
};

template <class DATA_TYPE>
class TBuffer : public CBuffer
{
public:
    BOOL Reserve(int size) { return CBuffer::Reserve(size * sizeof(DATA_TYPE)); }
    DATA_TYPE* Get() { return (DATA_TYPE*)Buffer; }
};

// ****************************************************************************

BOOL GetOpenFileName(HWND parent, const char* title, const char* filter,
                     char* buffer, BOOL save = FALSE);

BOOL RemoveFSNameFromPath(LPWSTR path);

BOOL DecStringToNumber(char* string, QWORD& qw);
BOOL HexStringToNumber(char* string, QWORD& qw);

char* ReplaceUnsafeCharacters(char* string);

//LPDLGTEMPLATE LoadDlgTemplate(int id, DWORD &size);
//LPDLGTEMPLATE ReplaceDlgTemplateFont(LPDLGTEMPLATE dlgTemplate, DWORD &size, LPCWSTR newFont);
