// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "peviewer.h"
#include "pefile.h"
#include "dump_res.h"

char* GetName(LPVOID ptr, char* buffer, int size)
{
    USHORT len = *(USHORT*)ptr;
    len = WideCharToMultiByte(CP_ACP, 0, MakePtr(LPWSTR, ptr, 2), len, buffer, size, NULL, NULL);
    buffer[min(len, size - 1)] = 0;
    return buffer;
}

char* MakeSpace(char* buffer, int length)
{
    char* iterator = buffer;
    while (iterator < buffer + length)
        *iterator++ = ' ';
    buffer[length] = 0;
    return buffer;
}

char* GetType(char* buffer, DWORD id, int level)
{
    char langName[128];
    *buffer = 0;
    if (level == 1)
        return buffer;
    const char* str = NULL;
    if (level == 0)
    {
        switch (id)
        {
        case (ULONG_PTR)RT_CURSOR:
            str = "Hardware-dependent cursor resource";
            break;
        case (ULONG_PTR)RT_BITMAP:
            str = "Bitmap resource";
            break;
        case (ULONG_PTR)RT_ICON:
            str = "Hardware-dependent icon resource";
            break;
        case (ULONG_PTR)RT_MENU:
            str = "Menu resource";
            break;
        case (ULONG_PTR)RT_DIALOG:
            str = "Dialog box";
            break;
        case (ULONG_PTR)RT_STRING:
            str = "String-table entry";
            break;
        case (ULONG_PTR)RT_FONTDIR:
            str = "Font directory resource";
            break;
        case (ULONG_PTR)RT_FONT:
            str = "Font resource";
            break;
        case (ULONG_PTR)RT_ACCELERATOR:
            str = "Accelerator table";
            break;
        case (ULONG_PTR)RT_RCDATA:
            str = "Application-defined resource (raw data)";
            break;
        case (ULONG_PTR)RT_MESSAGETABLE:
            str = "Message-table entry";
            break;
        case (ULONG_PTR)RT_GROUP_CURSOR:
            str = "Hardware-independent cursor resource";
            break;
        case (ULONG_PTR)RT_GROUP_ICON:
            str = "Hardware-independent icon resource";
            break;
        case (ULONG_PTR)RT_VERSION:
            str = "Version resource";
            break;
        case (ULONG_PTR)RT_DLGINCLUDE:
            str = "Dialog include";
            break;
        case (ULONG_PTR)RT_PLUGPLAY:
            str = "Plug and play resource";
            break;
        case (ULONG_PTR)RT_VXD:
            str = "VXD";
            break;
        case (ULONG_PTR)RT_ANICURSOR:
            str = "Animated cursor";
            break;
        case (ULONG_PTR)RT_ANIICON:
            str = "Animated icon";
            break;
        case (ULONG_PTR)RT_HTML:
            str = "HTML document";
            break;
        case (ULONG_PTR)RT_MANIFEST:
            str = "Manifest";
            break;
        }
    }
    else
    {
        if (id && GetLocaleInfoA(MAKELCID(MAKELANGID(id, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, 128))
            str = langName;
    }
    if (str)
    {
        sprintf(buffer, " (%s)", str);
    }
    return buffer;
}

BOOL DumpResDirectory(LPVOID lpFile, DWORD fileSize, CFileStream* outStream,
                      DWORD pointerToRawData, DWORD virtAddr, DWORD dirRootOffset,
                      DWORD dirOffset, int level)
{
    PIMAGE_RESOURCE_DIRECTORY rsdirh = MakePtr(PIMAGE_RESOURCE_DIRECTORY, lpFile, dirOffset);
    UINT width = 31;

    char buffer[1000];
    char buf[100];
    int space;
    const char* head;
    switch (level)
    {
    case 0:
        head = "%sTypes Directory Table:\n";
        space = 2;
        break;
    case 1:
        head = "%sName Directory Table:\n";
        space = 6;
        break;
    case 2:
        head = "%sLanguages Directory Table:\n";
        space = 10;
        break;
    }
    outStream->fprintf(head, MakeSpace(buf, space));
    space += 2;
    outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Characteristics:", rsdirh->Characteristics);
    TimeDateStampToString(rsdirh->TimeDateStamp, buffer);
    outStream->fprintf("%s%-*s0x%08X%s\n", MakeSpace(buf, space), width, "Time Date Stamp:",
                       rsdirh->TimeDateStamp, buffer);
    outStream->fprintf("%s%-*s%u.%02u\n", MakeSpace(buf, space), width, "Version:", rsdirh->MajorVersion, rsdirh->MinorVersion);
    outStream->fprintf("%s%-*s%u\n", MakeSpace(buf, space), width, "Number of Name Entries:", rsdirh->NumberOfNamedEntries);
    outStream->fprintf("%s%-*s%u\n", MakeSpace(buf, space), width, "Number of ID Entries:", rsdirh->NumberOfIdEntries);

    PIMAGE_RESOURCE_DIRECTORY_ENTRY rsdir = MakePtr(PIMAGE_RESOURCE_DIRECTORY_ENTRY, rsdirh,
                                                    sizeof(IMAGE_RESOURCE_DIRECTORY));
    int n = rsdirh->NumberOfNamedEntries;
    int j;
    for (j = 0; j < 2; j++)
    {
        int i;
        for (i = 0; i < n; i++, rsdir++)
        {
            if (j == 0)
            {
                const char* s;
                switch (level)
                {
                case 0:
                    s = "Type:";
                    break;
                case 1:
                    s = "Name:";
                    break;
                case 2:
                    s = "Language Name:";
                    break;
                }
                outStream->fprintf("%s%s %s\n", MakeSpace(buf, space), s,
                                   GetName(MakePtr(LPVOID, lpFile, dirRootOffset + rsdir->NameOffset), buffer, 1000));
            }
            else
            {
                const char* s;
                switch (level)
                {
                case 0:
                    s = "Type:";
                    break;
                case 1:
                    s = "ID:";
                    break;
                case 2:
                    s = "Language ID:";
                    break;
                }
                outStream->fprintf("%s%s %u%s\n", MakeSpace(buf, space), s, rsdir->Id, GetType(buffer, rsdir->Id, level));
            }
            if (level < 2)
            {
                space += 2;
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Subdirectory RVA:", rsdir->OffsetToDirectory);
                if (!DumpResDirectory(lpFile, fileSize, outStream, pointerToRawData, virtAddr, dirRootOffset, rsdir->OffsetToDirectory + dirRootOffset,
                                      level + 1))
                    return FALSE;
            }
            else
            {
                space += 2;
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Data Entry RVA:", rsdir->OffsetToData);
                //outStream->fprintf("%sData Entry:\n", MakeSpace(buf, space));
                PIMAGE_RESOURCE_DATA_ENTRY rsdata = MakePtr(PIMAGE_RESOURCE_DATA_ENTRY, lpFile,
                                                            rsdir->OffsetToData + dirRootOffset);
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Offset To Data:", rsdata->OffsetToData);
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Offset In File:", rsdata->OffsetToData - virtAddr + pointerToRawData);
                outStream->fprintf("%s%-*s0x%08X (%u)\n", MakeSpace(buf, space), width, "Size:", rsdata->Size, rsdata->Size);
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Code Page:", rsdata->CodePage);
                outStream->fprintf("%s%-*s0x%08X\n", MakeSpace(buf, space), width, "Reserved:", rsdata->Reserved);
            }
            space -= 2;
        }
        n = rsdirh->NumberOfIdEntries;
    }

    return TRUE;
}

void CResourceDirectoryDumper::DumpCore(CFileStream* outStream)
{
    PIMAGE_FILE_HEADER pPEFileHeader = m_peFile.GetPEFileHeader();
    PIMAGE_OPTIONAL_HEADER32 poh = m_peFile.GetPEOptionalHeader();
    IMAGE_SECTION_HEADER rsrcSection;

    if (m_peFile.ImageFileType(NULL) != IMAGE_NT_SIGNATURE)
        return;

    //najdeme .rsrc section
    if (!m_peFile.GetSectionHdrByName(&rsrcSection, ".rsrc"))
        return;

    DWORD pointerToRawData = rsrcSection.PointerToRawData;
    DWORD virtAddr = rsrcSection.VirtualAddress;
    DWORD dirOffset = pointerToRawData + (!m_peFile.Is64Bit() ? poh->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress : ((PIMAGE_OPTIONAL_HEADER64)poh)->DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress) - virtAddr;

    outStream->fprintf("Resource Directory:\n");
    DumpResDirectory(m_peFile.GetMappingBaseAddress(), m_peFile.GetMappingSize(),
                     outStream, pointerToRawData,
                     virtAddr, dirOffset, dirOffset, 0);
}

const _TCHAR* CResourceDirectoryDumper::GetExceptionMessage()
{
    return _T("Resource Directory is corrupted.");
}
