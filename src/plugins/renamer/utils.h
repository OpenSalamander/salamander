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

void LoadHistory(HKEY regKey, const char* keyPattern, char** history,
                 char* buffer, int bufferSize,
                 CSalamanderRegistryAbstract* registry);
void SaveHistory(HKEY regKey, const char* keyPattern, char** history,
                 CSalamanderRegistryAbstract* registry);

BOOL FileError(HWND parent, const char* fileName, int error,
               BOOL retry, BOOL* skip, BOOL* skipAll, int title);

BOOL FileOverwrite(HWND parent, const char* fileName1, const char* fileData1,
                   const char* fileName2, const char* fileData2, DWORD attr,
                   int shquestion, int shtitle, BOOL* skip, DWORD* silent);

// ****************************************************************************

class CBuffer
{
public:
    CBuffer(BOOL persistent = FALSE)
    {
        Buffer = NULL;
        Allocated = 0;
        Persistent = persistent;
    }
    ~CBuffer() { Release(); }
    BOOL Reserve(size_t size);
    void Release()
    {
        if (Buffer)
            free(Buffer);
        Buffer = NULL;
        Allocated = 0;
    }
    size_t GetSize() { return Allocated; }

protected:
    void* Buffer;
    size_t Allocated;
    BOOL Persistent;
};

template <class DATA_TYPE>
class TBuffer : public CBuffer
{
public:
    TBuffer(BOOL persistent = FALSE) : CBuffer(persistent) { ; }
    BOOL Reserve(size_t size) { return CBuffer::Reserve(size * sizeof(DATA_TYPE)); }
    DATA_TYPE* Get() { return (DATA_TYPE*)Buffer; }
    size_t GetSize() { return Allocated / sizeof(DATA_TYPE); }
};

// ****************************************************************************

struct CVarStrHelpMenuItem
{
    int MenuItemStringID;
    const char* VariableName;
    BOOL(*FParameterGetValue)
    (HWND parent, char* buffer);
    CVarStrHelpMenuItem* SubMenu;
};

BOOL SelectVarStrVariable(HWND parent, int x, int y,
                          CVarStrHelpMenuItem* helpMenu, char* buffer, BOOL varStr);

// ****************************************************************************

const char* StrQChr(const char* start, const char* end, char q, char c);
BOOL IsValidInt(const char* begin, const char* end, BOOL isSigned);
BOOL IsValidFloat(const char* begin, const char* end);
int GetRegExpErrorID(CRegExpErrors err);
char* StripRoot(char* path, int rootLen);
enum CRenameSpec;
BOOL ValidateFileName(const char* name, int len, CRenameSpec spec,
                      BOOL* skip, BOOL* skipAll);
int CutTrailingDots(char* name, int len, CRenameSpec spec);
inline const char* GetNextPathComponent(const char* name)
{
    while (*name != '\\' && *name != 0)
        name++;
    return name;
}
BOOL GetOpenFileName(HWND parent, const char* title, const char* filter,
                     char* buffer, BOOL save = FALSE);
