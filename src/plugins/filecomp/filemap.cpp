// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

struct CAcces
{
    DWORD FileMappingProtection;
    DWORD ViewOfFileAccess;
};

CAcces AccesTypes[3] =
    {
        {PAGE_READONLY, FILE_MAP_READ},
        {PAGE_READWRITE, FILE_MAP_WRITE},
        {PAGE_WRITECOPY, FILE_MAP_COPY}};

CFileMapping::CFileMapping(DWORD minViewSize)
{
    CALL_STACK_MESSAGE2("CFileMapping::CFileMapping(0x%X)", minViewSize);
    File = INVALID_HANDLE_VALUE;
    FileSize = 0;
    MapObject = NULL;
    View = NULL;
    MinViewSize = minViewSize;
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    AllocationGranularity = si.dwAllocationGranularity;
}

void CFileMapping::Destroy()
{
    CALL_STACK_MESSAGE1("CFileMapping::Destroy()");
    if (View)
    {
        UnmapViewOfFile(View);
        View = NULL;
    }
    if (MapObject)
    {
        CloseHandle(MapObject);
        MapObject = NULL;
    }
    if (File != INVALID_HANDLE_VALUE)
    {
        CloseHandle(File);
        File = INVALID_HANDLE_VALUE;
    }
}

void CFileMapping::Release()
{
    CALL_STACK_MESSAGE1("CFileMapping::Release()");
    File = INVALID_HANDLE_VALUE;
    MapObject = NULL;
    View = NULL;
}

BOOL CFileMapping::SetFile(HANDLE file, CAccessType access)
{
    CALL_STACK_MESSAGE1("CFileMapping::SetFile(, )");
    Destroy();
    Access = access;
    File = file;
    BY_HANDLE_FILE_INFORMATION fi;
    if (!GetFileInformationByHandle(file, &fi))
        return FALSE;
    FileSize = MAKEQWORD(fi.nFileSizeLow, fi.nFileSizeHigh);
    if (FileSize)
    {
        MapObject = CreateFileMapping(File, NULL, AccesTypes[Access].FileMappingProtection, 0, 0, NULL);
        if (!MapObject)
            return FALSE;
    }
    return TRUE;
}

void* CFileMapping::MapViewOfFile(QWORD offset, DWORD size)
{
    CALL_STACK_MESSAGE_NONE
    // CALL_STACK_MESSAGE2("CFileMapping::MapViewOfFile(, 0x%X)", size);
    if (FileSize == 0)
        return NULL;
    if (!View || offset < ViewOffset ||
        offset + size > ViewOffset + ViewSize)
    {
        if (View)
        {
            UnmapViewOfFile(View);
            View = NULL;
        }
        QWORD offsetAligned = offset / AllocationGranularity * AllocationGranularity;
        DWORD s = std::max(
            size + DWORD(offset - offsetAligned),
            DWORD(std::min(
                QWORD(MinViewSize), FileSize - offsetAligned)));
        void* ret = ::MapViewOfFile(MapObject, AccesTypes[Access].ViewOfFileAccess,
                                    HIDWORD(offsetAligned), LODWORD(offsetAligned), s);
        if (!ret)
            return NULL;
        View = ret;
        ViewOffset = offsetAligned;
        ViewSize = s;
    }
    return (char*)View + (offset - ViewOffset);
}

#if (FILE_ATTRIBUTE_ENCRYPTED != 0x00004000) && !defined(ALG_SID_AES_256)
#define ULONG_PTR UINT_PTR // Old SDK compatibility
#endif

BOOL CFileMapping::IsFileIOException(EXCEPTION_POINTERS* e)
{
    CALL_STACK_MESSAGE1("CFileMapping::IsFileIOException()");
    if ((e->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR ||
         e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION) &&
        e->ExceptionRecord->NumberParameters >= 2 &&
        View != 0 &&
        e->ExceptionRecord->ExceptionInformation[1] >= (ULONG_PTR)View &&
        e->ExceptionRecord->ExceptionInformation[1] < (ULONG_PTR)View + ViewSize)
    {
        return TRUE;
    }
    return FALSE;
}