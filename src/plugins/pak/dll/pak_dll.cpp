// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "texts.rh2"
#include "array2.h"
#include "pakiface.h"
#include "pak_dll.h"

#ifdef PAK_DLL
// ****************************************************************************

class C__StrCriticalSection
{
public:
    CRITICAL_SECTION cs;

    C__StrCriticalSection() { InitializeCriticalSection(&cs); }
    ~C__StrCriticalSection() { DeleteCriticalSection(&cs); }
};

// zajistime vcasnou konstrukci kriticke sekce
#pragma warning(disable : 4073)
#pragma init_seg(lib)
C__StrCriticalSection __StrCriticalSection;

// ****************************************************************************

HINSTANCE DLLInstance = NULL; //dll instance handle

char* StringBuffer = NULL; // buffer pro mnoho stringu
char* StrAct = StringBuffer;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DLLInstance = hinstDLL;
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        break;
    }
    }
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    EnterCriticalSection(&__StrCriticalSection.cs);

    char* ret;
    if (!StringBuffer)
        ret = "ERROR LOADING STRING - INSUFFICIENT MEMORY";
    else
    {
        if (5120 - (StrAct - StringBuffer) < 200)
            StrAct = StringBuffer;

#ifdef _DEBUG
        // radeji si pojistime, aby nas nekdo nevolal pred inicializaci handlu s resourcy
        if (DLLInstance == NULL)
            TRACE_E("LoadStr: DLLInstance == NULL");
#endif

    RELOAD:
        int size = LoadString(DLLInstance, resID, StrAct, 5120 - (StrAct - StringBuffer));
        // size obsahuje pocet nakopirovanych znaku bez terminatoru
        //    DWORD error = GetLastError();
        if (size != 0 /* || error == NO_ERROR*/) // error je NO_ERROR, i kdyz string neexistuje - nepouzitelne
        {
            if (5120 - (StrAct - StringBuffer) == size + 1 && StrAct > StringBuffer)
            {
                // pokud byl retezec presne na konci bufferu, mohlo
                // jit o oriznuti retezce -- pokud muzeme posunout okno
                // na zacatek bufferu, nacteme string jeste jednou
                StrAct = StringBuffer;
                goto RELOAD;
            }
            else
            {
                ret = StrAct;
                StrAct += size + 1;
            }
        }
        else
        {
            TRACE_E("Error in LoadStr(" << resID << ")." /*"): " << GetErrorText(error)*/);
            ret = "ERROR LOADING STRING";
        }
    }

    LeaveCriticalSection(&__StrCriticalSection.cs);

    return ret;
}

#endif //PAK_DLL

CPakIfaceAbstract* WINAPI PAKGetIFace()
{
#ifdef PAK_DLL
    if (!StringBuffer)
        StrAct = StringBuffer = (char*)malloc(5120);
    if (!StringBuffer)
        return NULL;
#endif //PAK_DLL
    return new CPakIface;
}

void WINAPI PAKReleaseIFace(CPakIfaceAbstract* pakIFace)
{
#ifdef PAK_DLL
    if (StringBuffer)
    {
        free(StringBuffer);
        StringBuffer = NULL;
    }
#endif //PAK_DLL
    delete pakIFace;
    return;
}

// ****************************************************************************
//
// CPakIface
//

CPakIface::CPakIface() : DelRegions(256), ZeroSizedFiles(64)
{
    Callbacks = NULL;
    PakFile = INVALID_HANDLE_VALUE;
    PakDir = NULL;
}

/*
CPakIface::~CPakIface()
{
  if (PakDir) free(PakDir);
  if (PakFile != INVALID_HANDLE_VALUE) CloseHandle(PakFile);
  return TRUE
}
*/

BOOL CPakIface::Init(CPakCallbacksAbstract* callbacks)
{
    Callbacks = callbacks;
    return TRUE;
}

BOOL CPakIface::OpenPak(const char* fileName, DWORD mode)
{
    char buf[1024];

    if (PakFile != INVALID_HANDLE_VALUE)
        return HandleError(0, IDS_PAK_ERROPEN2);

    while (PakFile == INVALID_HANDLE_VALUE)
    {
        PakFile = CreateFile(fileName, mode, FILE_SHARE_READ, NULL, OPEN_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, NULL);
        if (PakFile != INVALID_HANDLE_VALUE)
            break;
        if (!HandleError(HE_RETRY, IDS_PAK_ERROPEN, LastErrorString(GetLastError(), buf)))
            return FALSE;
    }

    PakSize = GetFileSize(PakFile, NULL);
    if (PakSize == 0xFFFFFFFF)
    {
        CloseHandle(PakFile);
        return HandleError(0, IDS_PAK_ERRGETFILESIZE, LastErrorString(GetLastError(), buf));
    }

    DirSize = 0;
    EmptyPak = FALSE;
    if (PakSize == 0)
    {
        EmptyPak = TRUE;
        return TRUE;
    }

    if (PakSize < sizeof(CPackHeader))
    {
        CloseHandle(PakFile);
        return HandleError(0, IDS_PAK_INVLAIDDATA);
    }

    if (!SafeRead(PakFile, &Header, sizeof(CPackHeader)))
    {
        CloseHandle(PakFile);
        return FALSE;
    }

    if (Header.Pack != 0x4b434150 || Header.DirOffset + Header.DirSize > PakSize ||
        Header.DirSize % sizeof(CPackEntry) != 0)
    {
        CloseHandle(PakFile);
        return HandleError(0, IDS_PAK_INVLAIDDATA);
    }

    DirSize = Header.DirSize / sizeof(CPackEntry);
    if (Header.DirSize == 0)
    {
        EmptyPak = TRUE;
        return TRUE;
    }

    PakDir = (CPackEntry*)malloc(Header.DirSize);
    if (!PakDir)
    {
        CloseHandle(PakFile);
        return HandleError(0, IDS_PAK_LOWMEMORY);
    }

    if (!SafeSeek(PakFile, Header.DirOffset) ||
        !SafeRead(PakFile, PakDir, Header.DirSize))
    {
        CloseHandle(PakFile);
        return FALSE;
    }

    return TRUE;
}

BOOL CPakIface::ClosePak()
{
    if (PakDir)
        free(PakDir);
    if (PakFile != INVALID_HANDLE_VALUE)
        CloseHandle(PakFile);
    PakFile = INVALID_HANDLE_VALUE;
    return TRUE;
}

BOOL CPakIface::GetPakTime(FILETIME* lastWrite)
{
    GetFileTime(PakFile, NULL, NULL, lastWrite);
    int i = GetLastError();
    return TRUE;
}

BOOL CPakIface::GetName(const char* nameInPak, char* outName)
{
    const char* sour = nameInPak;
    char* dest = outName;
    while (*sour)
    {
        switch (*sour)
        {
        case '/':
            *dest++ = '\\';
            sour++;
            break;

        case '.':
        {
            if ((sour == PakDir[DirPos].FileName || *(sour - 1) == '/') &&
                *(sour + 1) == '.' && *(sour + 2) == '/')
            {
                lstrcpy(dest, PAK_UPDIR);
                dest += lstrlen(PAK_UPDIR);
                sour += 2;
                break;
            }
        }

        default:
            *dest++ = *sour++;
        }
        if (sour > nameInPak + 0x38)
            return HandleError(0, IDS_PAK_INVLAIDDATA);
    }
    *dest = NULL;
    return TRUE;
}

BOOL CPakIface::GetFirstFile(char* fileName, DWORD* size)
{
    DirPos = 0;
    if (EmptyPak)
    {
        fileName[0] = 0;
        return TRUE;
    }
    if (!GetName(PakDir[DirPos].FileName, fileName))
        return FALSE;
    *size = PakDir[DirPos].Size;
    return TRUE;
}

BOOL CPakIface::GetNextFile(char* fileName, DWORD* size)
{
    DirPos++;
    if (DirPos >= DirSize)
    {
        fileName[0] = 0;
        return TRUE;
    }
    if (!GetName(PakDir[DirPos].FileName, fileName))
        return FALSE;
    *size = PakDir[DirPos].Size;
    return TRUE;
}

BOOL CPakIface::FindFile(const char* fileName, DWORD* size)
{
    DWORD s = -1;
    char name[PAK_MAXPATH];
    unsigned i;
    for (i = 0; i < DirSize; i++)
    {
        if (!GetName(PakDir[(int)i].FileName, name))
            return FALSE;
        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, fileName, -1, name, -1) == CSTR_EQUAL)
        {
            DirPos = i;
            s = PakDir[DirPos].Size;
            break;
        }
    }
    *size = s;
    return TRUE;
}

BOOL CPakIface::ExtractFile()
{
    char buffer[IOBUFSIZE];
    if (!SafeSeek(PakFile, PakDir[DirPos].Offset))
        return FALSE;
    DWORD left = PakDir[DirPos].Size;
    int read;
    while (left)
    {
        read = left > IOBUFSIZE ? IOBUFSIZE : left;
        if (!SafeRead(PakFile, buffer, read))
            return FALSE;
        if (!Callbacks->Write(buffer, read))
            return FALSE;
        if (!Callbacks->AddProgress(read))
            return FALSE;
        left -= read;
    }
    return TRUE;
}

char* CPakIface::FormatMessage(char* buffer, int errorID, va_list arglist)
{
    *buffer = 0;
    wvsprintf(buffer, LoadStr(errorID), arglist);
    return buffer;
}

BOOL CPakIface::HandleError(DWORD flags, int errorID, ...)
{
    va_list arglist;
    va_start(arglist, errorID);
    BOOL ret = Callbacks->HandleError(flags, errorID, arglist);
    va_end(arglist);
    return ret;
}

char* CPakIface::LastErrorString(int lastError, char* buffer)
{
    *buffer = 0;
    ::FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                    lastError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                    buffer, 1024, NULL);
    return buffer;
}

BOOL CPakIface::SafeSeek(HANDLE file, DWORD position)
{
    char buf[1024];
    while (1)
    {
        if (SetFilePointer(file, position, NULL, FILE_BEGIN) != 0xFFFFFFFF)
            return TRUE;
        if (!HandleError(HE_RETRY, IDS_PAK_ERRSETFILEPTR, LastErrorString(GetLastError(), buf)))
            return FALSE;
    }
}

BOOL CPakIface::SafeRead(HANDLE file, void* buffer, DWORD size)
{
    if (size == 0)
        return TRUE;
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(file, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        if (!HandleError(HE_RETRY, IDS_PAK_ERRGETFILEPTR, LastErrorString(GetLastError(), buf)))
            return FALSE;
    }
    DWORD read;
    while (1)
    {
        if (ReadFile(file, buffer, size, &read, NULL) && read == size)
            return TRUE;
        if (!HandleError(HE_RETRY, IDS_PAK_ERRREADFILE, LastErrorString(GetLastError(), buf)))
            return FALSE;
        if (!SafeSeek(file, pos))
            return FALSE;
    }
}

BOOL CPakIface::SafeWrite(HANDLE file, void* buffer, DWORD size)
{
    if (size == 0)
        return TRUE;
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(file, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        if (!HandleError(HE_RETRY, IDS_PAK_ERRGETFILEPTR, LastErrorString(GetLastError(), buf)))
            return FALSE;
    }
    DWORD written;
    while (1)
    {
        if (WriteFile(file, buffer, size, &written, NULL))
            return TRUE;
        if (!HandleError(HE_RETRY, IDS_PAK_ERRWRITEFILE, LastErrorString(GetLastError(), buf)))
            return FALSE;
        if (!SafeSeek(file, pos))
            return FALSE;
    }
}
