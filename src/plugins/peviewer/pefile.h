// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#define SIZE_OF_NT_SIGNATURE sizeof(DWORD)
#define MAXRESOURCENAME 13

/* global macros to define header offsets into file */
/* offset to PE file signature */
#define NTSIGNATURE(a) ((LPVOID)((BYTE*)a + \
                                 ((PIMAGE_DOS_HEADER)a)->e_lfanew))

/* DOS header identifies the NT PEFile signature dword
   the PEFILE header exists just after that dword */
#define PEFHDROFFSET(a) ((LPVOID)((BYTE*)a + \
                                  ((PIMAGE_DOS_HEADER)a)->e_lfanew + \
                                  SIZE_OF_NT_SIGNATURE))

/* PE optional header is immediately after PEFile header */
#define OPTHDROFFSET(a) ((LPVOID)((BYTE*)a + \
                                  ((PIMAGE_DOS_HEADER)a)->e_lfanew + \
                                  SIZE_OF_NT_SIGNATURE + \
                                  sizeof(IMAGE_FILE_HEADER)))

/* section headers are immediately after PE optional header */
#define SECHDROFFSET(a) ((LPVOID)((BYTE*)a + \
                                  ((PIMAGE_DOS_HEADER)a)->e_lfanew + \
                                  SIZE_OF_NT_SIGNATURE + \
                                  sizeof(IMAGE_FILE_HEADER) + \
                                  ((PIMAGE_FILE_HEADER)(((BYTE*)a) + SIZE_OF_NT_SIGNATURE + ((PIMAGE_DOS_HEADER)a)->e_lfanew))->SizeOfOptionalHeader /*sizeof(IMAGE_OPTIONAL_HEADER)*/))

// MakePtr is a macro that allows you to easily add to values (including
// pointers) together without dealing with C's pointer arithmetic.  It
// essentially treats the last two parameters as DWORDs.  The first
// parameter is used to typecast the result to the appropriate pointer type.
#define MakePtr(cast, ptr, addValue) (cast)((BYTE*)(ptr) + (addValue))

#define FIELD_SIZE(s, f) sizeof(((s*)0)->f)
#define STRUCT_HAS_FIELD(s, f, size) ((size) >= offsetof(s, f) + FIELD_SIZE(s, f))

BOOL TimeDateStampToString(DWORD timeDateStamp, char* buffer);

class CPEFile
{
public:
    CPEFile(LPVOID File, DWORD FileSize);

    DWORD ImageFileType(int* exceptionCount);
    PIMAGE_FILE_HEADER GetPEFileHeader();
    PIMAGE_OPTIONAL_HEADER32 GetPEOptionalHeader();
    int GetNumberOfExportedFunctions();
    BOOL ImageDirectoryOffset(DWORD dwIMAGE_DIRECTORY, LPVOID* pid, PIMAGE_SECTION_HEADER* psh);
    BOOL GetSectionHdrByName(IMAGE_SECTION_HEADER* sh, const char* szSection);

    BOOL Is64Bit()
    {
        return b64Bit;
    }

    LPBYTE GetMappingBaseAddress()
    {
        return lpFile;
    }

    DWORD GetMappingSize()
    {
        return fileSize;
    }

    PIMAGE_SECTION_HEADER GetImageSectionHeader()
    {
        return pISH;
    }

private:
    LPBYTE lpFile;
    DWORD fileSize;
    BOOL b64Bit;
    PIMAGE_SECTION_HEADER pISH;
};

class CFileStream
{
private:
    FILE* Stream;
    BOOL OutputEnabled;
    BOOL Dirty;
    BOOL Empty;

public:
    CFileStream(FILE* stream)
    {
        Stream = stream;
        OutputEnabled = TRUE;
        Dirty = FALSE;
        Empty = TRUE;
    }
    void fprintf(const char* format, ...)
    {
        if (OutputEnabled)
        {
            va_list args;
            va_start(args, format);
            vfprintf(Stream, format, args);
            va_end(args);
            Dirty = TRUE;
            Empty = FALSE;
        }
    }
    void SetEnabled(BOOL enabled) { OutputEnabled = enabled; }
    BOOL GetEnabled() { return OutputEnabled; }
    void SetDirty(BOOL dirty) { Dirty = dirty; }
    BOOL GetDirty() { return Dirty; }
    BOOL IsEmpty() { return Empty; }
};

class CFileStreamEnabler
{
private:
    BOOL WasEnabled;
    CFileStream* Stream;

public:
    CFileStreamEnabler(CFileStream* stream, BOOL enable)
    {
        Stream = stream;
        WasEnabled = stream->GetEnabled();
        Stream->SetEnabled(enable);
    }
    ~CFileStreamEnabler()
    {
        Stream->SetEnabled(WasEnabled);
    }
};

class CPEDumper
{
private:
    BOOL m_wasException;

protected:
    CPEFile& m_peFile;

    CPEDumper(CPEFile& PEFile)
        : m_peFile(PEFile),
          m_wasException(FALSE)
    {
    }

    void SetException()
    {
        m_wasException = TRUE;
    }

public:
    virtual ~CPEDumper()
    {
    }

    BOOL WasException()
    {
        return m_wasException;
    }

    virtual void Dump(CFileStream* outStream) = 0;
};

class CNTDumperWithExceptionHandler : public CPEDumper
{
public:
    CNTDumperWithExceptionHandler(CPEFile& PEFile)
        : CPEDumper(PEFile)
    {
    }

    virtual void Dump(CFileStream* outStream);

protected:
    virtual void DumpCore(CFileStream* outStream) = 0;
    virtual const _TCHAR* GetExceptionMessage() = 0;
};

#define WITH_COR_METADATA_DUMPER 1

// Pocet dostupnych dumperu.
// Pri implementaci noveho potomka tridy CPEDumper zvys tuto hodnotu!
#define AVAILABLE_PE_DUMPERS (12 + WITH_COR_METADATA_DUMPER)

class CFileTypeDumper : public CPEDumper
{
public:
    CFileTypeDumper(CPEFile& PEFile)
        : CPEDumper(PEFile)
    {
    }

    virtual void Dump(CFileStream* outStream);

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CFileTypeDumper(*file);
    }
};

class CFileHeaderDumper : public CNTDumperWithExceptionHandler
{
public:
    CFileHeaderDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CFileHeaderDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class COptionalFileHeaderDumper : public CNTDumperWithExceptionHandler
{
public:
    COptionalFileHeaderDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new COptionalFileHeaderDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CExportTableDumper : public CNTDumperWithExceptionHandler
{
public:
    CExportTableDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CExportTableDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CImportTableDumper : public CNTDumperWithExceptionHandler
{
public:
    CImportTableDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CImportTableDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CSectionTableDumper : public CNTDumperWithExceptionHandler
{
public:
    CSectionTableDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CSectionTableDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CDebugDirectoryDumper : public CNTDumperWithExceptionHandler
{
private:
    void DumpCodeView(CFileStream* outStream, const void* data, DWORD size);
    void DumpMisc(CFileStream* outStream, const void* data, DWORD size);

public:
    CDebugDirectoryDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CDebugDirectoryDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CResourceDirectoryDumper : public CNTDumperWithExceptionHandler
{
public:
    CResourceDirectoryDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CResourceDirectoryDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CLoadConfigDumper : public CNTDumperWithExceptionHandler
{
public:
    CLoadConfigDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CLoadConfigDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

#undef EnumResourceTypes
#undef FindResource

class CBaseResourceDumper : public CNTDumperWithExceptionHandler
{
private:
    PIMAGE_RESOURCE_DIRECTORY m_pResDir;
    LPBYTE m_pBase;

protected:
    CBaseResourceDumper(CPEFile& PEFile);

    typedef bool (*PFN_EnumResourceDirectoryEntryCallback)(PIMAGE_RESOURCE_DIRECTORY_ENTRY pResourceDirEntry, void* pUserData);

    PIMAGE_RESOURCE_DIRECTORY GetResourceDirectory()
    {
        return m_pResDir;
    }

    LPBYTE OffsetToPtr(DWORD dwOffset)
    {
        return (LPBYTE)m_pResDir + dwOffset;
    }

    LPBYTE RvaToPtr(DWORD dwRva)
    {
        return m_pBase + dwRva;
    }

    bool EnumResourceDirectoryEntries(PIMAGE_RESOURCE_DIRECTORY pResourceDir, PFN_EnumResourceDirectoryEntryCallback pfnCallback, void* pUserData);

    PIMAGE_RESOURCE_DIRECTORY_ENTRY FindResourceDirectoryEntry(PIMAGE_RESOURCE_DIRECTORY pResourceDir, int id);

    PIMAGE_RESOURCE_DIRECTORY_ENTRY FindResourceTypeDirectoryEntry(int id)
    {
        if (m_pResDir == NULL)
        {
            return NULL;
        }

        return FindResourceDirectoryEntry(m_pResDir, id);
    }

private:
    void* FindResource(int* paIds, DWORD caIds, PIMAGE_RESOURCE_DATA_ENTRY& pDataEntry);

protected:
    void* FindResource(WORD wType, int nId, int nLang, PIMAGE_RESOURCE_DATA_ENTRY& pDataEntry)
    {
        int ids[3] = {wType, nId, nLang};
        return FindResource(ids, 3, pDataEntry);
    }

    void* FindResource(WORD wType, int nId, int nLang, DWORD& dwSize)
    {
        PIMAGE_RESOURCE_DATA_ENTRY pEntry;
        void* pData = FindResource(wType, nId, nLang, pEntry);
        if (pData != NULL)
        {
            dwSize = pEntry->Size;
        }
        return pData;
    }

    virtual void DumpCore(CFileStream* outStream);
    virtual void DumpResources(CFileStream* outStream) = 0;
    virtual const _TCHAR* GetExceptionMessage() = 0;

private:
    struct FindResourceDirectoryEntryById
    {
        PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntry;
        int nId; // -1 je prvni dostupny zaznam
    };

    static bool FindResourceDirectoryEntryByIdCallback(PIMAGE_RESOURCE_DIRECTORY_ENTRY pResourceDirEntry, void* pUserData);
};

class CFileVersionResourceDumper : public CBaseResourceDumper
{
public:
    CFileVersionResourceDumper(CPEFile& PEFile)
        : CBaseResourceDumper(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CFileVersionResourceDumper(*file);
    }

private:
    static bool ReadVsVersionInfo(CFileStream* outStream, class BinaryReader& reader);
    static bool ReadXxxFileInfo(CFileStream* outStream, class BinaryReader& reader);
    static bool ReadStringTable(CFileStream* outStream, class BinaryReader& reader);
    static bool ReadString(CFileStream* outStream, class BinaryReader& reader);
    static void DumpFixedFileInfo(CFileStream* outStream, const VS_FIXEDFILEINFO* pFixedInfo);
    static int Bcd(WORD w);

protected:
    virtual void DumpResources(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CManifestResourceDumper : public CBaseResourceDumper
{
public:
    CManifestResourceDumper(CPEFile& PEFile)
        : CBaseResourceDumper(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CManifestResourceDumper(*file);
    }

protected:
    virtual void DumpResources(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

class CCorHeaderDumper : public CNTDumperWithExceptionHandler
{
public:
    CCorHeaderDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CCorHeaderDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();
};

#if WITH_COR_METADATA_DUMPER

#pragma warning(push)
#pragma warning(disable : 4091)
#include <cor.h> // pokud chybi, je potreba nainstalovat .NET Framework SDK (21.1.2019 to byla verze 4.7.2), viz Visual Studio Installer / Modify / Individual components
#pragma warning(pop)

#include <fusion.h>

#include "SortedStringList.h"

class CCorMetadataDumper : public CNTDumperWithExceptionHandler
{
public:
    CCorMetadataDumper(CPEFile& PEFile)
        : CNTDumperWithExceptionHandler(PEFile)
    {
    }

    static CPEDumper* WINAPI Create(CPEFile* file)
    {
        return new CCorMetadataDumper(*file);
    }

protected:
    virtual void DumpCore(CFileStream* outStream);
    virtual const _TCHAR* GetExceptionMessage();

private:
    typedef CSortedStringListTraitsW CStringListTraits;
    typedef CSortedStringListT<WCHAR, CStringListTraits> CStringList;

    enum
    {
        MAX_ASSEMBLY_NAME = 500
    };

    static void DumpMetadata(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdAssembly assembly);
    static void DumpRuntimeVersion(CFileStream* outStream, IMetaDataImport* pMetaDataImport);
    static void DumpCustomAttributes(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdToken scope);
    static void DumpCustomAttribute(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdCustomAttribute customAttribute);
    static HRESULT GetCustomAttributeTypeNameFromMemberRef(IMetaDataImport* pMetaDataImport, mdMemberRef memberRef, PWSTR wzTypeName, DWORD cchTypeName);
    static void DumpTargetFrameworkVersion(CFileStream* outStream, const BYTE* pBlob, DWORD cbBlob);
    static void DumpReferencedAssemblies(CFileStream* outStream, IMetaDataAssemblyImport* pMetaDataAssemblyImport);
    static void DumpAssemblyReference(CStringList& list, IMetaDataAssemblyImport* pMetaDataAssemblyImport, mdAssemblyRef assemblyRef);
    static void DumpAssemblyFullyQualifiedName(CStringList& list, LPCWSTR pwzName, const ASSEMBLYMETADATA* pMetaData);
    static void DumpAssemblyFullyQualifiedName(CStringList& list, IAssemblyName* pAssemblyName);
    static void AsmNameSetMetaData(IAssemblyName* pAssemblyName, const ASSEMBLYMETADATA* pMetaData);
    static void AsmNameSetPublicKey(IAssemblyName* pAssemblyName, const void* pPublicKey, DWORD cbPublicKey, BOOL bIsFullKey);
    static void AsmNameSetFlags(IAssemblyName* pAssemblyName, DWORD dwCorAssemblyFlags);
    static int SortAssemblyNameCallback(const CStringList::XCHAR* s1, const CStringList::XCHAR* s2);
    static int GetAssemblyNameSortBoost(const CStringList::XCHAR* pszAssemblyName);
};

#endif /* WITH_COR_METADATA_DUMPER */

// backports from latest SDK:

#ifndef IMAGE_FILE_MACHINE_ARMNT
#define IMAGE_FILE_MACHINE_ARMNT 0x01c4 // ARM Thumb-2 Little-Endian
#endif

#ifndef IMAGE_FILE_MACHINE_ARM64
#define IMAGE_FILE_MACHINE_ARM64 0xAA64 // ARM64 Little-Endian
#endif
