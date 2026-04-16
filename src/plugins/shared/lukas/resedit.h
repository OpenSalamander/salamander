// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

// ******************************************************************************
//
// CResEdit class methods replace the BeginUpdateResource, UpdateResource, and EndUpdateResource API functions.
//
// Current limitations:
// - the .rsrc section must exist in the executable
// - the .rsrc section must be at the end of the PE file; otherwise (for example, in a debug build)
//   the rest of the executable is overwritten
// - deleting a resource from the executable is not supported (the API allows this by passing NULL as the 'lpData' parameter to UpdateResource)
// - the exact error cannot be determined; none of the three methods sets last error, and they return only FALSE on failure

class CResEditRoot
{
protected:
    HANDLE File;
    BOOL State; // TRUE = OK

public:
    CResEditRoot() { State = TRUE; }
    BOOL IsGood() { return State; }

protected:
    BOOL Seek(DWORD offset);
    BOOL Read(void* buffer, DWORD size, DWORD* lastError = NULL);
    BOOL Write(void* buffer, DWORD size);
};

//******************************************************************************
//
// CResEdit
//

class CResDir;

class CResEdit : public CResEditRoot
{
    IMAGE_DOS_HEADER MZHead;
    IMAGE_FILE_HEADER PEHead;
    DWORD SizeOfInitializedData;
    DWORD SizeOfImage;
    IMAGE_OPTIONAL_HEADER32 OptHead;
    IMAGE_SECTION_HEADER* Sections;
    int RsrcSectIndex;
    //DWORD                     ResDirOffs;
    CResDir* ResDir;

public:
    CResEdit()
    {
        File = NULL;
        ResDir = NULL;
        Sections = NULL;
    }

    // The BeginUpdateResource initilalize CResEdit class and begin resource update in
    // an executable file.
    BOOL BeginUpdateResource(
        LPCSTR pFileName,             // executable file name
        BOOL bDeleteExistingResources // deletion option
    );

    // The UpdateResource function adds, deletes, or replaces a resource in
    // an executable file.
    BOOL UpdateResource(
        LPCSTR lpType,  // resource type
        LPCSTR lpName,  // resource name
        WORD wLanguage, // language identifier
        LPVOID lpData,  // resource data
        DWORD cbData    // length of resource data
    );

    // The RemoveAllResources function deletes all resources from executable.
    BOOL RemoveAllResources();

    // The GetResourceDirectory function obstain a copy of resource directory
    // tree. Returns pointer to copy of resource directory tree or NULL in case
    // of error (low memory).
    CResDir* GetResourceDirectory();

    // The SetResourceDirectory function  replace current resource directory
    // tree by 'directory'.
    BOOL SetResourceDirectory(CResDir* directory);

    // The FreeResourceDirectory function frees memory occupied by 'directory'.
    BOOL FreeResourceDirectory(CResDir* directory);

    // The EndUpdateResource function ends a resource update in an executable file.
    BOOL EndUpdateResource(
        BOOL fDiscard // write option
    );

private:
    BOOL LoadResourceTree(BOOL bDeleteExistingResources);
    BOOL SaveResourceTree();
    DWORD GetBiggestAvailableVA();
    DWORD GetBiggestAvailableFP();
};

//******************************************************************************
//
// Internal Declarations
//

#define OPTHEAD_SIZE (24 * 4)

//******************************************************************************
//
// Resource Directory Tree Classes
//

struct COffsets
{
    DWORD PointerToRawData;
    DWORD SectionVirtualAddress;
    DWORD DirRootOffset;
};

class CSaveRes : public CResEditRoot
{
public:
    char* Directory;
    DWORD DirectoryPos;
    DWORD DirectorySize;

    char* Strings;
    DWORD StringsPos;
    DWORD StringsSize;

    char* DataDescript;
    DWORD DataDescriptPos;
    DWORD DataDescriptSize;

    char* Data;
    DWORD DataPos;
    DWORD DataSize;

    DWORD SectionVirtualAddress;

    CSaveRes(DWORD sectionVirtualAddress);

    ~CSaveRes();

    BOOL Init();

    PIMAGE_RESOURCE_DIRECTORY_ENTRY AddDir(PIMAGE_RESOURCE_DIRECTORY dir, DWORD totalEntries,
                                           DWORD* offset);

    DWORD AddString(WCHAR* string);

    DWORD AddDataDescr(PIMAGE_RESOURCE_DATA_ENTRY dataDescr);

    DWORD AddData(void* data, DWORD size);

    void AddDirSize(DWORD totalEntries)
    {
        DirectorySize += sizeof(IMAGE_RESOURCE_DIRECTORY) +
                         totalEntries * sizeof(IMAGE_RESOURCE_DIRECTORY_ENTRY);
    }

    void AddStringSize(WCHAR* string)
    {
        StringsSize += ((2 + lstrlenW(string) * 2 + 3) / 4) * 4;
    }

    void AddDataDescrSize()
    {
        DataDescriptSize += sizeof(IMAGE_RESOURCE_DATA_ENTRY);
    }

    void AddDataSize(DWORD size)
    {
        DataSize += ((size + 3) / 4) * 4;
    }
};

class CResTreeNode : public CResEditRoot
{
public:
    virtual ~CResTreeNode() {};
    virtual CResTreeNode* GetCopy() = 0;
    virtual BOOL Load(int level, DWORD offset, COffsets* offsets) = 0;
    virtual DWORD Save(CSaveRes* save) = 0;
    virtual void AddSize(CSaveRes* save) = 0;
    virtual BOOL AddResource(LPCSTR type, LPCSTR name, WORD language, void* data, DWORD size) = 0;
};

class CDirEntry
{
public:
    WCHAR* Name;
    UINT ID; // resource type/lang ID/resource ID
    CResTreeNode* Node;
    BOOL State;

    CDirEntry()
    {
        Name = NULL;
        Node = NULL;
        State = TRUE;
    }

    CDirEntry(CDirEntry* entry);

    ~CDirEntry()
    {
        if (Name)
            free(Name);
        if (Node)
            delete Node;
    }

    BOOL IsGood() { return State; }
};

//void DeleteDirEntry(void * entry);

class CResDir : public CResTreeNode
{
    ULONG Characteristics;
    ULONG TimeDateStamp;
    USHORT MajorVersion;
    USHORT MinorVersion;
    //USHORT      NumberOfNamedEntries;
    //USHORT      NumberOfIdEntries;
    TIndirectArray2<CDirEntry> DirEntries;

public:
    CResDir(HANDLE file) : DirEntries(32)
    {
        File = file;
        Characteristics = 0;
        TimeDateStamp = 0;
        MajorVersion = 0;
        MinorVersion = 0;
    }

    CResDir(CResDir* dir);

    virtual CResTreeNode* GetCopy();

    void Clean();

    BOOL IsEmpty() { return DirEntries.Count == 0; }

    virtual BOOL Load(int level, DWORD offset, COffsets* offsets);

    BOOL LoadName(CDirEntry* entry, UINT name, DWORD dirRootOffset);

    virtual DWORD Save(CSaveRes* save);

    virtual void AddSize(CSaveRes* save);

    virtual BOOL AddResource(LPCSTR type, LPCSTR name, WORD language, void* data, DWORD size);
};

class CResTreeLeaf : public CResTreeNode
{
    //language dir
    ULONG Characteristics;
    ULONG TimeDateStamp;
    USHORT MajorVersion;
    USHORT MinorVersion;
    //USHORT      NumberOfNamedEntries;
    //USHORT      NumberOfIdEntries;
    LANGID LangID;

    //resource data
    void* Data;
    ULONG Size;
    ULONG CodePage;
    ULONG Reserved;

public:
    CResTreeLeaf(HANDLE file);

    CResTreeLeaf(CResTreeLeaf* leaf);

    ~CResTreeLeaf()
    {
        if (Data)
            free(Data);
    }

    virtual CResTreeNode* GetCopy();

    virtual BOOL Load(int level, DWORD offset, COffsets* offsets);

    virtual DWORD Save(CSaveRes* save);

    virtual void AddSize(CSaveRes* save);

    virtual BOOL AddResource(LPCSTR type, LPCSTR name, WORD language, void* data, DWORD size);
};
