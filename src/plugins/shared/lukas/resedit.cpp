// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <crtdbg.h>
#include <ostream>
//#include <winioctl.h>
#include <commctrl.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#ifdef ZIP_DLL
#include "spl_base.h"
#include "dbg.h"
#else //ZIP_DLL
#include "killdbg.h"
#endif //ZIP_DLL

#include "array2.h"
#include "resedit.h"

//******************************************************************************
//
// CResEditRoot
//

BOOL CResEditRoot::Seek(DWORD offset)
{
    CALL_STACK_MESSAGE_NONE
    LONG dummy = 0;
    if (SetFilePointer(File, offset, &dummy, FILE_BEGIN) == 0xFFFFFFFF)
        return FALSE;
    return TRUE;
}

BOOL CResEditRoot::Read(void* buffer, DWORD size, DWORD* lastError)
{
    CALL_STACK_MESSAGE2("CResEditRoot::Read(, 0x%X)", size);
    DWORD read;
    if (!ReadFile(File, buffer, size, &read, NULL) || read != size)
    {
        if (lastError)
            *lastError = GetLastError();
        return FALSE;
    }
    return TRUE;
}

BOOL CResEditRoot::Write(void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("CResEditRoot::Write(, 0x%X)", size);
    DWORD written;
    if (!WriteFile(File, buffer, size, &written, NULL))
        return FALSE;
    return TRUE;
}

//******************************************************************************
//
// CResEdit
//

BOOL CResEdit::BeginUpdateResource(LPCSTR pFileName, BOOL bDeleteExistingResources)
{
    CALL_STACK_MESSAGE3("CResEdit::BeginUpdateResource(%s, %d)", pFileName,
                        bDeleteExistingResources);
    if (File)
        return FALSE;
    int num_of_retries = 0;

try_again:
    File = CreateFile(pFileName, GENERIC_WRITE | GENERIC_READ, 0, NULL, OPEN_EXISTING,
                      FILE_ATTRIBUTE_NORMAL, NULL);

    if (!File)
        return FALSE;

    // nacteme headery
    DWORD pesig;
    DWORD lastError = 0;
    if (!Read(&MZHead, sizeof(IMAGE_DOS_HEADER), &lastError) ||
        !Seek(MZHead.e_lfanew) ||
        !Read(&pesig, sizeof(DWORD)) ||
        pesig != IMAGE_NT_SIGNATURE ||
        !Read(&PEHead, sizeof(IMAGE_FILE_HEADER)) ||
        !Read(&OptHead, OPTHEAD_SIZE) ||
        !Read(OptHead.DataDirectory, OptHead.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY)) ||
        OptHead.NumberOfRvaAndSizes <= IMAGE_DIRECTORY_ENTRY_RESOURCE) //zkontrolujeme jestli mame ressource dir
    {
        if (lastError == ERROR_INVALID_HANDLE && ++num_of_retries <= 10)
        {
            CloseHandle(File);
            Sleep(100); // cekame 10x 100ms na "zvalidneni" handlu (asi do toho zasahuje antivir?)
            goto try_again;
        }
        TRACE_E("Error reading headers.");
        goto error;
    }

    // nactene section headery, allokujeme o jednu vice, kdyby mezi nima nebyla
    // .rsrc section, tak abych ji mohli pridat bez reallokace
    Sections = (IMAGE_SECTION_HEADER*)malloc(sizeof(IMAGE_SECTION_HEADER) * (PEHead.NumberOfSections + 1));
    if (!Sections)
    {
        TRACE_E("Low memory.");
        goto error;
    }
    if (!Read(Sections, sizeof(IMAGE_SECTION_HEADER) * PEHead.NumberOfSections))
    {
        TRACE_E("Error reading sections.");
        goto error;
    }

    // najdeme resource section
    DWORD i;
    RsrcSectIndex = -1;
    SizeOfInitializedData = 0;
    SizeOfImage = ((OptHead.SizeOfHeaders + OptHead.SectionAlignment - 1) / OptHead.SectionAlignment) * OptHead.SectionAlignment;
    for (i = 0; i < PEHead.NumberOfSections; i++)
    {
        if (strncmp((char*)Sections[i].Name, ".rsrc", 8) == 0)
        {
            RsrcSectIndex = i;
        }
        else
        {
            if (Sections[i].Characteristics & IMAGE_SCN_CNT_INITIALIZED_DATA)
            {
                SizeOfInitializedData += Sections[i].SizeOfRawData;
            }
            SizeOfImage += ((Sections[i].Misc.VirtualSize + OptHead.SectionAlignment - 1) / OptHead.SectionAlignment) * OptHead.SectionAlignment;
        }
    }

    if (RsrcSectIndex == -1)
        TRACE_I(".rsrc section not found.");
    else
    {
        // overime ze nalezena section obsahuje resource dir
        if (Sections[RsrcSectIndex].VirtualAddress > OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress ||
            Sections[RsrcSectIndex].VirtualAddress + Sections[RsrcSectIndex].SizeOfRawData <= OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress)
        {
            TRACE_E("too weird format .rsrc section doesn't contain resource dir");
            goto error;
        }

        // overime .rsrc section neobsahuje jine data adresare (jako napr UPXnuty exac,
        // ktery ma v .rsrc sekci ulozenou import tabulku)
        DWORD alignedSize = ((Sections[RsrcSectIndex].Misc.VirtualSize + OptHead.SectionAlignment - 1) / OptHead.SectionAlignment) * OptHead.SectionAlignment;
        for (i = 0; i < OptHead.NumberOfRvaAndSizes; i++)
        {
            if (i == IMAGE_DIRECTORY_ENTRY_RESOURCE)
                continue;
            if (OptHead.DataDirectory[i].VirtualAddress >= Sections[RsrcSectIndex].VirtualAddress &&
                OptHead.DataDirectory[i].VirtualAddress < Sections[RsrcSectIndex].VirtualAddress + alignedSize)
            {
                TRACE_E("too weird format .rsrc contains other than resource dir");
                goto error;
            }
        }
    }

    if (LoadResourceTree(bDeleteExistingResources))
        return TRUE;

error:
    CloseHandle(File);
    File = NULL;
    if (Sections)
    {
        free(Sections);
        Sections = NULL;
    }
    return FALSE;
}

BOOL CResEdit::UpdateResource(LPCSTR lpType, LPCSTR lpName, WORD wLanguage, LPVOID lpData, DWORD cbData)
{
    CALL_STACK_MESSAGE3("CResEdit::UpdateResource(, , 0x%X, , 0x%X)", wLanguage, cbData);
    if (!lpData || !File)
        return FALSE;

    if (!ResDir->AddResource(lpType, lpName, wLanguage, lpData, cbData))
        return FALSE;

    return TRUE;
}

BOOL CResEdit::RemoveAllResources()
{
    CALL_STACK_MESSAGE1("CResEdit::RemoveAllResources()");
    ResDir->Clean();
    return TRUE;
}

CResDir*
CResEdit::GetResourceDirectory()
{
    CALL_STACK_MESSAGE1("CResEdit::GetResourceDirectory()");
    CResDir* ret = (CResDir*)ResDir->GetCopy();
    if (ret && !ret->IsGood())
    {
        delete ret;
        ret = NULL;
    }
    return ret;
}

BOOL CResEdit::SetResourceDirectory(CResDir* directory)
{
    CALL_STACK_MESSAGE1("CResEdit::SetResourceDirectory()");
    CResDir* dir = (CResDir*)directory->GetCopy();
    if (dir && !dir->IsGood())
    {
        delete dir;
        return FALSE;
    }
    delete ResDir;
    ResDir = dir;
    return TRUE;
}

BOOL CResEdit::FreeResourceDirectory(CResDir* directory)
{
    CALL_STACK_MESSAGE1("CResEdit::FreeResourceDirectory()");
    delete directory;
    return TRUE;
}

BOOL CResEdit::EndUpdateResource(BOOL fDiscard)
{
    CALL_STACK_MESSAGE2("CResEdit::EndUpdateResource(%d)", fDiscard);
    if (!File)
        return FALSE;

    BOOL ret = TRUE;

    if (!fDiscard)
        ret = SaveResourceTree();

    if (!CloseHandle(File))
        ret = FALSE;
    File = NULL;

    free(Sections);
    Sections = NULL;

    delete ResDir;

    return ret;
}

BOOL CResEdit::LoadResourceTree(BOOL bDeleteExistingResources)
{
    CALL_STACK_MESSAGE2("CResEdit::LoadResourceTree(%d)", bDeleteExistingResources);
    ResDir = new CResDir(File);
    if (!ResDir)
        return FALSE;

    if (bDeleteExistingResources || RsrcSectIndex == -1)
        return TRUE;

    COffsets offsets;

    offsets.PointerToRawData = Sections[RsrcSectIndex].PointerToRawData;
    offsets.SectionVirtualAddress = Sections[RsrcSectIndex].VirtualAddress;
    offsets.DirRootOffset = Sections[RsrcSectIndex].PointerToRawData +
                            OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress -
                            Sections[RsrcSectIndex].VirtualAddress;

    if (!ResDir->Load(0, 0, &offsets))
    {
        delete ResDir;
        return FALSE;
    }

    return TRUE;
}

BOOL CResEdit::SaveResourceTree()
{
    CALL_STACK_MESSAGE1("CResEdit::SaveResourceTree()");
    if (ResDir->IsEmpty())
    {
        //odstranime resource section
        if (RsrcSectIndex != -1)
        {
            PEHead.NumberOfSections--;
            memcpy(Sections + RsrcSectIndex,
                   Sections + RsrcSectIndex + 1,
                   (PEHead.NumberOfSections - RsrcSectIndex) * sizeof(IMAGE_SECTION_HEADER));
            OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = 0;
            OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = 0;
            RsrcSectIndex = -1;
        }
        OptHead.SizeOfInitializedData = SizeOfInitializedData;
        OptHead.SizeOfImage = SizeOfImage;
    }
    else
    {
        //ulozime resourcy
        if (RsrcSectIndex == -1)
        {
            //pridame resource section
            RsrcSectIndex = PEHead.NumberOfSections;
            memcpy(Sections[RsrcSectIndex].Name, ".rsrc", 5);
            memset(Sections[RsrcSectIndex].Name + 5, 0, 3);
            Sections[RsrcSectIndex].VirtualAddress = GetBiggestAvailableVA();
            Sections[RsrcSectIndex].PointerToRawData = GetBiggestAvailableFP();
            Sections[RsrcSectIndex].PointerToRelocations = 0;
            Sections[RsrcSectIndex].PointerToLinenumbers = 0;
            Sections[RsrcSectIndex].NumberOfRelocations = 0;
            Sections[RsrcSectIndex].NumberOfLinenumbers = 0;
            Sections[RsrcSectIndex].Characteristics = IMAGE_SCN_CNT_INITIALIZED_DATA | IMAGE_SCN_MEM_READ;
            PEHead.NumberOfSections++;
        }

        CSaveRes save(Sections[RsrcSectIndex].VirtualAddress);

        ResDir->AddSize(&save);
        if (!save.Init())
            return FALSE;
        ResDir->Save(&save);

        DWORD size = save.DirectorySize + save.StringsSize + save.DataDescriptSize + save.DataSize;
        if (!Seek(Sections[RsrcSectIndex].PointerToRawData))
            return FALSE;
        if (!Write(save.Directory, size))
            return FALSE;

        DWORD sizeAligned = ((size + OptHead.FileAlignment - 1) / OptHead.FileAlignment) * OptHead.FileAlignment;
        OptHead.SizeOfInitializedData = SizeOfInitializedData + sizeAligned;
        OptHead.SizeOfImage = SizeOfImage + ((sizeAligned + OptHead.SectionAlignment - 1) / OptHead.SectionAlignment) * OptHead.SectionAlignment;
        OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].VirtualAddress = Sections[RsrcSectIndex].VirtualAddress;
        OptHead.DataDirectory[IMAGE_DIRECTORY_ENTRY_RESOURCE].Size = size;

        Sections[RsrcSectIndex].Misc.VirtualSize = size;
        Sections[RsrcSectIndex].SizeOfRawData = sizeAligned;
    }

    if (!Seek(MZHead.e_lfanew + sizeof(DWORD)))
        return FALSE;
    if (!Write(&PEHead, sizeof(IMAGE_FILE_HEADER)) ||
        !Write(&OptHead, OPTHEAD_SIZE + OptHead.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY)))
        return FALSE;

    if (!Seek(MZHead.e_lfanew + sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) +
              OPTHEAD_SIZE + OptHead.NumberOfRvaAndSizes * sizeof(IMAGE_DATA_DIRECTORY)))
        return FALSE;
    if (!Write(Sections, sizeof(IMAGE_SECTION_HEADER) * PEHead.NumberOfSections))
        return FALSE;

    DWORD end = GetBiggestAvailableFP();

    if (!Seek(end))
        return FALSE;
    if (!SetEndOfFile(File))
        return FALSE;

    return TRUE;
}

DWORD
CResEdit::GetBiggestAvailableVA()
{
    CALL_STACK_MESSAGE1("CResEdit::GetBiggestAvailableVA()");
    DWORD ret = 0;
    DWORD size = 0;
    for (int i = 0; i < PEHead.NumberOfSections; i++)
    {
        size = ((Sections[i].Misc.VirtualSize + OptHead.SectionAlignment - 1) / OptHead.SectionAlignment) * OptHead.SectionAlignment;
        if (Sections[i].VirtualAddress + size > ret)
            ret = Sections[i].VirtualAddress + size;
    }
    if (ret == 0)
        TRACE_E("biggest available VA is 0");
    return ret;
}

DWORD
CResEdit::GetBiggestAvailableFP()
{
    CALL_STACK_MESSAGE1("CResEdit::GetBiggestAvailableFP()");
    DWORD ret = OptHead.SizeOfHeaders;
    DWORD size = 0;
    for (int i = 0; i < PEHead.NumberOfSections; i++)
    {
        if (Sections[i].PointerToRawData + Sections[i].SizeOfRawData > ret)
            ret = Sections[i].PointerToRawData + Sections[i].SizeOfRawData;
    }
    return ret;
}

//******************************************************************************
//
// CSaveRes
//

CSaveRes::CSaveRes(DWORD sectionVirtualAddress)
{
    CALL_STACK_MESSAGE_NONE
    Directory = NULL;
    DirectoryPos = 0;
    DirectorySize = 0;
    Strings = NULL;
    StringsPos = 0;
    StringsSize = 0;
    DataDescript = NULL;
    DataDescriptPos = 0;
    DataDescriptSize = 0;
    Data = NULL;
    DataPos = 0;
    DataSize = 0;
    SectionVirtualAddress = sectionVirtualAddress;
}

CSaveRes::~CSaveRes()
{
    CALL_STACK_MESSAGE_NONE
    if (Directory)
        free(Directory);
}

BOOL CSaveRes::Init()
{
    CALL_STACK_MESSAGE1("CSaveRes::Init()");
    Directory = (char*)malloc(DirectorySize + StringsSize + DataDescriptSize + DataSize);
    if (!Directory)
        return FALSE;
    Strings = Directory + DirectorySize;
    DataDescript = Directory + DirectorySize + StringsSize;
    Data = Directory + DirectorySize + StringsSize + DataDescriptSize;
    return TRUE;
}

PIMAGE_RESOURCE_DIRECTORY_ENTRY CSaveRes::AddDir(PIMAGE_RESOURCE_DIRECTORY dir,
                                                 DWORD totalEntries, DWORD* offset)
{
    CALL_STACK_MESSAGE2("CSaveRes::AddDir(, 0x%X, )", totalEntries);
    PIMAGE_RESOURCE_DIRECTORY_ENTRY ret;

    *offset = DirectoryPos;
    ret = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(Directory + (DirectoryPos +
                                                         sizeof(IMAGE_RESOURCE_DIRECTORY)));
    memcpy(Directory + DirectoryPos, dir, sizeof(IMAGE_RESOURCE_DIRECTORY));
    DirectoryPos += sizeof(IMAGE_RESOURCE_DIRECTORY) +
                    totalEntries * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    return ret;
}

DWORD CSaveRes::AddString(WCHAR* string)
{
    CALL_STACK_MESSAGE1("CSaveRes::AddString()");
    DWORD ret = StringsPos + DirectorySize;
    USHORT len = lstrlenW(string);
    *((USHORT*)(Strings + StringsPos)) = len;
    memcpy(Strings + StringsPos + 2, string, len * 2);
    StringsPos += ((2 + len * 2 + 3) / 4) * 4;
    return ret;
}

DWORD CSaveRes::AddDataDescr(PIMAGE_RESOURCE_DATA_ENTRY dataDescr)
{
    CALL_STACK_MESSAGE1("CSaveRes::AddDataDescr()");
    DWORD ret = DataDescriptPos + StringsSize + DirectorySize;
    memcpy(DataDescript + DataDescriptPos, dataDescr, sizeof(IMAGE_RESOURCE_DATA_ENTRY));
    DataDescriptPos += sizeof(IMAGE_RESOURCE_DATA_ENTRY);
    return ret;
}

DWORD CSaveRes::AddData(void* data, DWORD size)
{
    CALL_STACK_MESSAGE2("CSaveRes::AddData(, 0x%X)", size);
    DWORD ret = DataPos + DataDescriptSize + StringsSize + DirectorySize + SectionVirtualAddress;
    memcpy(Data + DataPos, data, size);
    DataPos += ((size + 3) / 4) * 4;
    return ret;
}

//******************************************************************************
//
// CDirEntry
//

CDirEntry::CDirEntry(CDirEntry* entry)
{
    CALL_STACK_MESSAGE1("CDirEntry::CDirEntry()");
    State = TRUE;
    ID = entry->ID;
    if (ID == -1)
    {
        int len = lstrlenW(entry->Name);
        Name = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        ;
        if (!Name)
        {
            State = FALSE;
            return; // low memory
        }
        lstrcpyW(Name, entry->Name);
    }
    else
        Name = NULL;
    Node = entry->Node->GetCopy();
    if (!Node || !Node->IsGood())
    {
        State = FALSE;
        return; // low memory
    }
}

//******************************************************************************
//
// CResDir
//

CResDir::CResDir(CResDir* dir) : DirEntries(32)
{
    CALL_STACK_MESSAGE1("CResDir::CResDir()");
    File = dir->File;
    Characteristics = dir->Characteristics;
    TimeDateStamp = dir->TimeDateStamp;
    MajorVersion = dir->MajorVersion;
    MinorVersion = dir->MinorVersion;
    CDirEntry* entry;
    for (int i = 0; i < dir->DirEntries.Count; i++)
    {
        entry = new CDirEntry(dir->DirEntries[i]);
        if (!entry || !entry->IsGood() || !DirEntries.Add(entry))
        {
            if (entry)
                delete entry;
            State = FALSE;
            return; // low memory
        }
    }
}

CResTreeNode*
CResDir::GetCopy()
{
    CALL_STACK_MESSAGE1("CResDir::GetCopy()");
    return new CResDir(this);
}

void CResDir::Clean()
{
    CALL_STACK_MESSAGE1("CResDir::Clean()");
    Characteristics = 0;
    TimeDateStamp = 0;
    MajorVersion = 0;
    MinorVersion = 0;
    DirEntries.Destroy();
}

BOOL CResDir::Load(int level, DWORD offset, COffsets* offsets)
{
    CALL_STACK_MESSAGE3("CResDir::Load(%d, 0x%p, )", level, offsets);
    if (!Seek(offsets->DirRootOffset + offset))
        return FALSE;

    IMAGE_RESOURCE_DIRECTORY rsdirh;
    if (!Read(&rsdirh, sizeof(IMAGE_RESOURCE_DIRECTORY)))
        return FALSE;

    Characteristics = rsdirh.Characteristics;
    TimeDateStamp = rsdirh.TimeDateStamp;
    MajorVersion = rsdirh.MajorVersion;
    MinorVersion = rsdirh.MinorVersion;
    //NumberOfNamedEntries = rsdirh.NumberOfNamedEntries;
    //NumberOfIdEntries = rsdirh.NumberOfIdEntries;

    int count = rsdirh.NumberOfNamedEntries + rsdirh.NumberOfIdEntries;

    PIMAGE_RESOURCE_DIRECTORY_ENTRY rsdir = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)malloc(count * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY));
    if (!rsdir)
        return FALSE;
    if (!Read(rsdir, count * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY)))
    {
        free(rsdir);
        return FALSE;
    }

    int i;
    for (i = 0; i < count; i++)
    {
        CDirEntry* entry;
        entry = new CDirEntry();
        if (!entry)
            break;

        if (!LoadName(entry, rsdir[i].Name, offsets->DirRootOffset))
        {
            delete entry;
            break;
        }

        if (level < 1) //resource types and resource names
        {
            entry->Node = new CResDir(File);
        }
        else //language and resource data
        {
            entry->Node = new CResTreeLeaf(File);
        }
        if (!entry->Node)
        {
            delete entry;
            break;
        }

        if (!entry->Node->Load(level + 1, rsdir[i].OffsetToData & ~0x80000000, offsets))
        {
            delete entry;
            break;
        }

        if (!DirEntries.Add(entry))
        {
            delete entry;
            break;
        }
    }
    free(rsdir);
    if (i == count)
        return TRUE;
    else
        return FALSE;
}

BOOL CResDir::LoadName(CDirEntry* entry, UINT name, DWORD dirRootOffset)
{
    CALL_STACK_MESSAGE3("CResDir::LoadName(, 0x%X, 0x%X)", name, dirRootOffset);
    if (name & 0x80000000)
    {
        if (!Seek(dirRootOffset + name & ~0x80000000))
            return FALSE;

        USHORT len;
        if (!Read(&len, sizeof(USHORT)))
            return FALSE;

        entry->Name = (WCHAR*)malloc((len + 1) * sizeof(WCHAR));
        if (!entry->Name)
            return FALSE;

        if (!Read(entry->Name, len * 2))
            return FALSE;
        entry->Name[len] = 0;
        entry->ID = -1;
    }
    else
    {
        entry->ID = name;
    }
    return TRUE;
}

int CompareEntries(CDirEntry* entry1, CDirEntry* entry2)
{
    CALL_STACK_MESSAGE_NONE
    if (entry1->ID == -1)
    {
        if (entry2->ID == -1)
            return lstrcmpW(entry1->Name, entry2->Name);
        else
            return -1;
    }
    else
    {
        if (entry2->ID == -1)
            return 1;
        else
            return entry1->ID - entry2->ID;
    }
}

void SortDirEntries(int left, int right, TIndirectArray2<CDirEntry>& entries)
{
    CALL_STACK_MESSAGE_NONE
    int i = left, j = right;
    CDirEntry* pivot = entries[(i + j) / 2];
    do
    {
        while (CompareEntries(entries[i], pivot) < 0 && i < right)
            i++;
        while (CompareEntries(pivot, entries[j]) < 0 && j > left)
            j--;
        if (i <= j)
        {
            CDirEntry* tmp = entries[i];
            entries[i] = entries[j];
            entries[j] = tmp;
            i++;
            j--;
        }
    } while (i <= j); //musej bejt shodny?
    if (left < j)
        SortDirEntries(left, j, entries);
    if (i < right)
        SortDirEntries(i, right, entries);
}

DWORD CResDir::Save(CSaveRes* save)
{
    CALL_STACK_MESSAGE1("CResDir::Save()");
    //seradime polozky, nejprve abecedne ty se jmenem a pak ciselne ty s ID
    SortDirEntries(0, DirEntries.Count - 1, DirEntries);

    IMAGE_RESOURCE_DIRECTORY rsdirh;

    rsdirh.Characteristics = Characteristics;
    rsdirh.TimeDateStamp = TimeDateStamp;
    rsdirh.MajorVersion = MajorVersion;
    rsdirh.MinorVersion = MinorVersion;
    int i;
    for (i = 0; i < DirEntries.Count; i++)
    {
        if (DirEntries[i]->ID != -1)
            break;
    }
    rsdirh.NumberOfNamedEntries = i;
    rsdirh.NumberOfIdEntries = DirEntries.Count - i;

    DWORD offset;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY rsdir = save->AddDir(&rsdirh, DirEntries.Count, &offset);

    for (i = 0; i < DirEntries.Count; i++)
    {
        CDirEntry* entry;
        entry = DirEntries[i];

        if (entry->ID == -1)
        {
            rsdir[i].Name = save->AddString(entry->Name) | 0x80000000;
        }
        else
            rsdir[i].Name = entry->ID;
        rsdir[i].OffsetToData = entry->Node->Save(save) | 0x80000000;
    }

    return offset;
}

void CResDir::AddSize(CSaveRes* save)
{
    CALL_STACK_MESSAGE_NONE
    save->AddDirSize(DirEntries.Count);

    for (int i = 0; i < DirEntries.Count; i++)
    {
        CDirEntry* entry;
        entry = DirEntries[i];

        if (entry->ID == -1)
        {
            save->AddStringSize(entry->Name);
        }
        entry->Node->AddSize(save);
    }
}

BOOL CResDir::AddResource(LPCSTR type, LPCSTR name, WORD language, void* data, DWORD size)
{
    CALL_STACK_MESSAGE3("CResDir::AddResource(, , 0x%X, , 0x%X)", language, size);
    WCHAR* buf = NULL;

    const char* n = type ? type : name;

    if ((DWORD)(DWORD_PTR)n & 0xFFFF0000)
    {
        int len = lstrlen(n);
        buf = (WCHAR*)malloc(2 * len + 2);
        if (!buf)
            return FALSE;
        if (!MultiByteToWideChar(CP_ACP, 0, n, -1, buf, len + 1))
        {
            free(buf);
            return FALSE;
        }
    }

    CDirEntry* entry;

    int i;
    for (i = 0; i < DirEntries.Count; i++)
    {
        entry = DirEntries[i];

        if ((DWORD)(DWORD_PTR)n & 0xFFFF0000 && entry->ID == -1)
        {
            if (lstrcmpiW(buf, entry->Name) == 0)
            {
                free(buf);
                buf = NULL;
                break;
            }
        }
        else
        {
            if ((DWORD)(DWORD_PTR)n == entry->ID)
                break;
        }
    }

    if (i == DirEntries.Count)
    {
        entry = new CDirEntry();
        if (!entry)
        {
            free(buf);
            return FALSE;
        }

        if (buf)
        {
            entry->Name = buf;
            entry->ID = -1;
        }
        else
            entry->ID = (WORD)(DWORD_PTR)n;

        if (type) //resource types and resource names
        {
            entry->Node = new CResDir(File);
        }
        else //language and resource data
        {
            entry->Node = new CResTreeLeaf(File);
        }
        if (!entry->Node || !DirEntries.Add(entry))
        {
            delete entry;
            return FALSE;
        }
    }

    if (!entry->Node->AddResource(0, name, language, data, size))
        return FALSE;

    return TRUE;
}

//******************************************************************************
//
// CResTreeLeaf
//

CResTreeLeaf::CResTreeLeaf(HANDLE file)
{
    CALL_STACK_MESSAGE1("CResTreeLeaf::CResTreeLeaf()");
    File = file;
    Data = NULL;
    Characteristics = 0;
    TimeDateStamp = 0;
    MajorVersion = 0;
    MinorVersion = 0;
    CodePage = 0;
    Reserved = 0;
}

CResTreeLeaf::CResTreeLeaf(CResTreeLeaf* leaf)
{
    CALL_STACK_MESSAGE1("CResTreeLeaf::CResTreeLeaf()");
    File = leaf->File;
    Characteristics = leaf->Characteristics;
    TimeDateStamp = leaf->TimeDateStamp;
    MajorVersion = leaf->MajorVersion;
    MinorVersion = leaf->MinorVersion;
    LangID = leaf->LangID;
    if (leaf->Data)
    {
        Data = malloc(leaf->Size);
        if (!Data)
        {
            State = FALSE;
            free(Data);
            return;
        }
        memcpy(Data, leaf->Data, leaf->Size);
    }
    else
        Data = NULL;
    Size = leaf->Size;
    CodePage = leaf->CodePage;
    Reserved = leaf->Reserved;
}

CResTreeNode*
CResTreeLeaf::GetCopy()
{
    CALL_STACK_MESSAGE1("CResTreeLeaf::GetCopy()");
    return new CResTreeLeaf(this);
}

BOOL CResTreeLeaf::Load(int level, DWORD offset, COffsets* offsets)
{
    CALL_STACK_MESSAGE3("CResTreeLeaf::Load(%d, 0x%p, )", level, offsets);
    //nacteme language dir
    if (!Seek(offsets->DirRootOffset + offset))
        return FALSE;

    IMAGE_RESOURCE_DIRECTORY rsdirh;
    IMAGE_RESOURCE_DIRECTORY_ENTRY rsdir;
    if (!Read(&rsdirh, sizeof(IMAGE_RESOURCE_DIRECTORY)))
        return FALSE;
    if (!Read(&rsdir, sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY)))
        return FALSE;

    Characteristics = rsdirh.Characteristics;
    TimeDateStamp = rsdirh.TimeDateStamp;
    MajorVersion = rsdirh.MajorVersion;
    MinorVersion = rsdirh.MinorVersion;
    //NumberOfNamedEntries = 0;
    //NumberOfIdEntries = 1;
    LangID = (LANGID)rsdir.Name;

    //nacteme resource data
    if (!Seek(offsets->DirRootOffset + rsdir.OffsetToData & ~0x80000000))
        return FALSE;

    IMAGE_RESOURCE_DATA_ENTRY rsdata;
    if (!Read(&rsdata, sizeof(IMAGE_RESOURCE_DATA_ENTRY)))
        return FALSE;

    Size = rsdata.Size;
    CodePage = rsdata.CodePage;
    Reserved = rsdata.Reserved;

    Data = malloc(Size);
    if (!Data)
        return FALSE;

    if (!Seek(offsets->PointerToRawData + rsdata.OffsetToData - offsets->SectionVirtualAddress) ||
        !Read(Data, Size))
    {
        free(Data);
        return FALSE;
    }

    return TRUE;
}

DWORD CResTreeLeaf::Save(CSaveRes* save)
{
    CALL_STACK_MESSAGE1("CResTreeLeaf::Save()");
    IMAGE_RESOURCE_DATA_ENTRY rsdata;

    rsdata.OffsetToData = save->AddData(Data, Size);
    rsdata.Size = Size;
    rsdata.CodePage = CodePage;
    rsdata.Reserved = Reserved;

    IMAGE_RESOURCE_DIRECTORY rsdirh;

    rsdirh.Characteristics = Characteristics;
    rsdirh.TimeDateStamp = TimeDateStamp;
    rsdirh.MajorVersion = MajorVersion;
    rsdirh.MinorVersion = MinorVersion;
    rsdirh.NumberOfNamedEntries = 0;
    rsdirh.NumberOfIdEntries = 1;

    DWORD offset;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY rsdir = save->AddDir(&rsdirh, 1, &offset);

    rsdir[0].Name = LangID;
    rsdir[0].OffsetToData = save->AddDataDescr(&rsdata);

    return offset;
}

void CResTreeLeaf::AddSize(CSaveRes* save)
{
    CALL_STACK_MESSAGE_NONE
    save->AddDataSize(Size);
    save->AddDirSize(1);
    save->AddDataDescrSize();
}

BOOL CResTreeLeaf::AddResource(LPCSTR type, LPCSTR name, WORD language, void* data, DWORD size)
{
    CALL_STACK_MESSAGE3("CResTreeLeaf::AddResource(, , 0x%X, , 0x%X)", language, size);
    if (Data)
        free(Data);

    Data = malloc(size);
    if (!Data)
        return FALSE;
    memcpy(Data, data, size);
    Size = size;
    LangID = language;
    return TRUE;
}
