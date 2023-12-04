// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: et:sw=2:ts=2

#include "precomp.h"
#include "peviewer.rh2"
#include "peviewer.h"
#include "pefile.h"
#include "dump_res.h"
#include "dump_dbg.h"
#include "cfg.h"

extern HINSTANCE DLLInstance; // handle k SPL-ku - jazykove nezavisle resourcy
extern HINSTANCE HLanguage;   // handle k SLG-cku - jazykove zavisle resourcy

// ****************************************************************************

int HandleFileException(EXCEPTION_POINTERS* e, LPVOID fileMem, DWORD fileMemSize)
{
    if (e->ExceptionRecord->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) // in-page-error znamena urcite chybu souboru
    {
        return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
    }
    else
    {
        if (e->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&    // access violation znamena chybu souboru jen pokud adresa chyby odpovida souboru
            (e->ExceptionRecord->NumberParameters >= 2 &&                         // mame co testovat
             e->ExceptionRecord->ExceptionInformation[1] >= (ULONG_PTR)fileMem && // ptr chyby ve view souboru
             e->ExceptionRecord->ExceptionInformation[1] < ((ULONG_PTR)fileMem) + fileMemSize))
        {
            return EXCEPTION_EXECUTE_HANDLER; // spustime __except blok
        }
        else
        {
            return EXCEPTION_CONTINUE_SEARCH; // hodime vyjimku dale ... call-stacku
        }
    }
}

/* return file signature */
DWORD CPEFile::ImageFileType(int* exceptionCount)
{
    __try
    {
        /* dos file signature comes first */
        if (*(USHORT*)lpFile == IMAGE_DOS_SIGNATURE)
        {
            if ((BYTE*)NTSIGNATURE(lpFile) + sizeof(DWORD) < (BYTE*)lpFile + fileSize &&
                (BYTE*)NTSIGNATURE(lpFile) + sizeof(DWORD) >= (BYTE*)lpFile)
            {
                /* determine location of PE File header from dos header */
                if (LOWORD(*(DWORD*)NTSIGNATURE(lpFile)) == IMAGE_OS2_SIGNATURE ||
                    LOWORD(*(DWORD*)NTSIGNATURE(lpFile)) == IMAGE_OS2_SIGNATURE_LE)
                    return (DWORD)LOWORD(*(DWORD*)NTSIGNATURE(lpFile));
                else if (*(DWORD*)NTSIGNATURE(lpFile) == IMAGE_NT_SIGNATURE)
                {
                    pISH = (PIMAGE_SECTION_HEADER)SECHDROFFSET(lpFile);
                    return IMAGE_NT_SIGNATURE;
                }
            }
            return IMAGE_DOS_SIGNATURE;
        }
        else
            return 0; /* unknown file type */
    }
    __except (HandleFileException(GetExceptionInformation(), lpFile, fileSize))
    {
        // chyba v souboru
        if (exceptionCount != NULL)
            *exceptionCount += 1;
        return 0; /* unknown file type */
    }
}

/* return file header information */
PIMAGE_FILE_HEADER CPEFile::GetPEFileHeader()
{
    /* file header follows dos header */
    if (ImageFileType(NULL) == IMAGE_NT_SIGNATURE)
        return (PIMAGE_FILE_HEADER)PEFHDROFFSET(lpFile);
    else
        return NULL;
}

/* return optional file header information */
PIMAGE_OPTIONAL_HEADER32 CPEFile::GetPEOptionalHeader()
{
    /* optional header follows file header and dos header */
    if (ImageFileType(NULL) == IMAGE_NT_SIGNATURE)
        return (PIMAGE_OPTIONAL_HEADER32)OPTHDROFFSET(lpFile);
    else
        return NULL;
}

/* return the total number of sections in the module */
int NumOfSections(LPVOID lpFile)
{
    /* number of sections is indicated in file header */
    return ((int)((PIMAGE_FILE_HEADER)PEFHDROFFSET(lpFile))->NumberOfSections);
}

/* return offset to specified IMAGE_DIRECTORY entry */
BOOL CPEFile::ImageDirectoryOffset(DWORD dwIMAGE_DIRECTORY,
                                   LPVOID* pid, PIMAGE_SECTION_HEADER* ppsh)
{
    PIMAGE_OPTIONAL_HEADER32 poh = (PIMAGE_OPTIONAL_HEADER32)OPTHDROFFSET(lpFile);
    PIMAGE_SECTION_HEADER psh = pISH;
    int nSections = NumOfSections(lpFile);
    int i = 0;
    DWORD VAImageDir;

    if (!b64Bit)
    {
        /* must be 0 thru (NumberOfRvaAndSizes-1) */
        if (dwIMAGE_DIRECTORY >= poh->NumberOfRvaAndSizes)
            return FALSE;

        /* locate specific image directory's relative virtual address */
        VAImageDir = poh->DataDirectory[dwIMAGE_DIRECTORY].VirtualAddress;
    }
    else
    {
        PIMAGE_OPTIONAL_HEADER64 poh64 = (PIMAGE_OPTIONAL_HEADER64)poh;

        /* must be 0 thru (NumberOfRvaAndSizes-1) */
        if (dwIMAGE_DIRECTORY >= poh64->NumberOfRvaAndSizes)
            return FALSE;

        /* locate specific image directory's relative virtual address */
        VAImageDir = poh64->DataDirectory[dwIMAGE_DIRECTORY].VirtualAddress;
    }

    /* locate section containing image directory */
    while (i++ < nSections)
    {
        if (psh->VirtualAddress <= VAImageDir &&
            psh->VirtualAddress + psh->SizeOfRawData > VAImageDir)
            break;
        psh++;
    }

    if (i > nSections)
        return FALSE;

    *pid = (LPVOID)((lpFile + VAImageDir - psh->VirtualAddress) +
                    (int)psh->PointerToRawData);
    if (ppsh != NULL)
        *ppsh = psh;

    return TRUE;
}

/* function gets the function header for a section identified by name */
BOOL CPEFile::GetSectionHdrByName(IMAGE_SECTION_HEADER* sh, const char* szSection)
{
    PIMAGE_SECTION_HEADER psh = pISH;
    int nSections = NumOfSections(lpFile);
    int i;

    if (psh != NULL)
    {
        /* find the section by name */
        for (i = 0; i < nSections; i++)
        {
            if (!strcmp((const char*)psh->Name, szSection))
            {
                /* copy data to header */
                CopyMemory((LPVOID)sh, (LPVOID)psh, sizeof(IMAGE_SECTION_HEADER));
                return TRUE;
            }
            else
                psh++;
        }
    }
    return FALSE;
}

/* return the number of exported functions in the module */
int CPEFile::GetNumberOfExportedFunctions()
{
    PIMAGE_EXPORT_DIRECTORY ped;

    if (!ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_EXPORT, (LPVOID*)&ped, NULL))
        return 0;
    else
        return (int)ped->NumberOfNames;
}

typedef struct
{
    WORD flag;
    PCSTR name;
} WORD_FLAG_DESCRIPTIONS;

typedef struct
{
    DWORD flag;
    PCSTR name;
} DWORD_FLAG_DESCRIPTIONS;

// Bitfield values and names for the IMAGE_FILE_HEADER flags
static const WORD_FLAG_DESCRIPTIONS ImageFileHeaderCharacteristics[] =
    {
        {IMAGE_FILE_RELOCS_STRIPPED, "Relocation info stripped from file"},
        {IMAGE_FILE_EXECUTABLE_IMAGE, "File is executable"},
        {IMAGE_FILE_LINE_NUMS_STRIPPED, "Line numbers stripped from file"},
        {IMAGE_FILE_LOCAL_SYMS_STRIPPED, "Local symbols stripped from file"},
        {IMAGE_FILE_AGGRESIVE_WS_TRIM, "Agressively trim working set"},
        {IMAGE_FILE_LARGE_ADDRESS_AWARE, "App can handle >2GB addresses"},
        {IMAGE_FILE_BYTES_REVERSED_LO, "Low bytes of machine word are reversed"},
        {IMAGE_FILE_32BIT_MACHINE, "32 bit word machine"},
        {IMAGE_FILE_DEBUG_STRIPPED, "Debugging info stripped from file in .DBG file"},
        {IMAGE_FILE_REMOVABLE_RUN_FROM_SWAP, "If Image is on removable media, copy and run from the swap file"},
        {IMAGE_FILE_NET_RUN_FROM_SWAP, "If Image is on Net, copy and run from the swap file"},
        {IMAGE_FILE_SYSTEM, "System File"},
        {IMAGE_FILE_DLL, "File is a DLL"},
        {IMAGE_FILE_UP_SYSTEM_ONLY, "File should only be run on a UP machine"},
        {IMAGE_FILE_BYTES_REVERSED_HI, "High bytes of machine word are reversed"},
};

#define NUMBER_IMAGE_HEADER_FLAGS _countof(ImageFileHeaderCharacteristics)

// Convert a TimeDateStamp (i.e., # of seconds since 1/1/1970) into a FILETIME

BOOL TimeDateStampToString(DWORD timeDateStamp, char* buffer)
{
    SYSTEMTIME st;
    FILETIME lft;

    *buffer = 0;

    if (timeDateStamp == 0 || timeDateStamp == 0xFFFFFFFF)
        return FALSE;

    __int64 t1970 = 0x019DB1DED53E8000; // Magic... GMT...  Don't ask....

    __int64 timeStampIn100nsIncr = (__int64)timeDateStamp * 10000000;

    __int64 finalValue = t1970 + timeStampIn100nsIncr;

    if (!FileTimeToLocalFileTime((FILETIME*)&finalValue, &lft))
        return FALSE;

    if (!FileTimeToSystemTime(&lft, &st))
        return FALSE;

    strcpy(buffer, " (");
    if (GetDateFormatA(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, buffer + 2, 50) == 0)
    {
        *buffer = 0;
        return FALSE;
    }
    strcat(buffer, " ");
    if (GetTimeFormatA(LOCALE_USER_DEFAULT, 0, &st, NULL, buffer + strlen(buffer), 50) == 0)
    {
        *buffer = 0;
        return FALSE;
    }
    strcat(buffer, ")");

    return TRUE;
}

CPEFile::CPEFile(LPVOID File, DWORD FileSize)
{
    lpFile = (LPBYTE)File;
    fileSize = FileSize;
    b64Bit = FALSE;
    pISH = NULL;

    PIMAGE_FILE_HEADER pPEFileHeader = GetPEFileHeader();
    if (pPEFileHeader != NULL)
    {
        switch (pPEFileHeader->Machine)
        {
        case IMAGE_FILE_MACHINE_IA64:
        case IMAGE_FILE_MACHINE_AMD64:
            b64Bit = TRUE;
            break;
        }
    }
}

void CFileHeaderDumper::DumpCore(CFileStream* outStream)
{
    PIMAGE_FILE_HEADER pPEFileHeader = m_peFile.GetPEFileHeader();
    if (pPEFileHeader != NULL)
    {
        char buff[100];
        UINT headerFieldWidth = 31;
        const char* szMachine;

        outStream->fprintf("File Header:\n");

        switch (pPEFileHeader->Machine)
        {
        case IMAGE_FILE_MACHINE_I386:
            szMachine = "Intel x86";
            break;
        case IMAGE_FILE_MACHINE_R3000:
            szMachine = "R3000, MIPS little-endian";
            break;
        case IMAGE_FILE_MACHINE_R4000:
            szMachine = "R4000, MIPS little-endian";
            break;
        case IMAGE_FILE_MACHINE_R10000:
            szMachine = "R10000, MIPS little-endian";
            break;
        case IMAGE_FILE_MACHINE_WCEMIPSV2:
            szMachine = "WCEMIPSV2, MIPS little-endian WCE v2";
            break;
        case IMAGE_FILE_MACHINE_ALPHA:
            szMachine = "ALPHA, Alpha AXP";
            break;
        case IMAGE_FILE_MACHINE_SH3:
            szMachine = "SH3, little-endian";
            break;
        case IMAGE_FILE_MACHINE_SH3DSP:
            szMachine = "SH3DSP";
            break;
        case IMAGE_FILE_MACHINE_SH3E:
            szMachine = "SH3E";
            break;
        case IMAGE_FILE_MACHINE_SH4:
            szMachine = "SH4, little-endian";
            break;
        case IMAGE_FILE_MACHINE_SH5:
            szMachine = "SH5";
            break;
        case IMAGE_FILE_MACHINE_ARM:
            szMachine = "ARM, little-endian";
            break;
        case IMAGE_FILE_MACHINE_THUMB:
            szMachine = "THUMB, ARM Thumb/Thumb-2 little-endian";
            break;
        case IMAGE_FILE_MACHINE_ARMNT:
            szMachine = "ARMNT, ARM Thumb-2 little-endian";
            break;
        case IMAGE_FILE_MACHINE_AM33:
            szMachine = "AM33";
            break;
        case IMAGE_FILE_MACHINE_POWERPC:
            szMachine = "POWERPC, IBM PowerPC little-endian";
            break;
        case IMAGE_FILE_MACHINE_POWERPCFP:
            szMachine = "POWERPCFP";
            break;
        case IMAGE_FILE_MACHINE_IA64:
            szMachine = "IA64, Intel 64";
            break;
        case IMAGE_FILE_MACHINE_MIPS16:
            szMachine = "MIPS16, MIPS";
            break;
        case IMAGE_FILE_MACHINE_ALPHA64:
            szMachine = "ALPHA64, MIPS";
            break;
        case IMAGE_FILE_MACHINE_MIPSFPU:
            szMachine = "MIPSFPU, MIPS";
            break;
        case IMAGE_FILE_MACHINE_MIPSFPU16:
            szMachine = "MIPSFPU16, MIPS";
            break;
        case IMAGE_FILE_MACHINE_TRICORE:
            szMachine = "TRICORE, Infineon";
            break;
        case IMAGE_FILE_MACHINE_CEF:
            szMachine = "CEF";
            break;
        case IMAGE_FILE_MACHINE_EBC:
            szMachine = "EBC, EFI Byte Code";
            break;
        case IMAGE_FILE_MACHINE_AMD64:
            szMachine = "AMD64";
            break;
        case IMAGE_FILE_MACHINE_M32R:
            szMachine = "M32R, little-endian";
            break;
        case IMAGE_FILE_MACHINE_ARM64:
            szMachine = "ARM64, little-endian";
            break;
        case IMAGE_FILE_MACHINE_CEE:
            szMachine = "CEE";
            break;
        default:
            szMachine = "Unknown";
            break;
        }

        outStream->fprintf("  %-*s0x%04X (%s)\n", headerFieldWidth, "Machine:",
                           pPEFileHeader->Machine, szMachine);
        outStream->fprintf("  %-*s%u\n", headerFieldWidth, "Number of Sections:",
                           pPEFileHeader->NumberOfSections);
        TimeDateStampToString(pPEFileHeader->TimeDateStamp, buff);
        outStream->fprintf("  %-*s0x%08X%s\n", headerFieldWidth, "Time Date Stamp:",
                           pPEFileHeader->TimeDateStamp, buff);
        outStream->fprintf("  %-*s0x%08X\n", headerFieldWidth, "Pointer to Symbol Table:",
                           pPEFileHeader->PointerToSymbolTable);
        outStream->fprintf("  %-*s%u\n", headerFieldWidth, "Number of Symbols:",
                           pPEFileHeader->NumberOfSymbols);
        outStream->fprintf("  %-*s0x%04X (%u)\n", headerFieldWidth, "Size of Optional Header:",
                           pPEFileHeader->SizeOfOptionalHeader, pPEFileHeader->SizeOfOptionalHeader);
        outStream->fprintf("  %-*s0x%04X\n", headerFieldWidth, "Characteristics:",
                           pPEFileHeader->Characteristics);
        int i;
        for (i = 0; i < NUMBER_IMAGE_HEADER_FLAGS; i++)
        {
            if (pPEFileHeader->Characteristics & ImageFileHeaderCharacteristics[i].flag)
                outStream->fprintf("    %s\n", ImageFileHeaderCharacteristics[i].name);
        }
    }
}

const char* CFileHeaderDumper::GetExceptionMessage()
{
    return "File Header is corrupted.";
}

//j.r.
#define IMAGE_LIBRARY_PROCESS_INIT 0x0001 // Reserved.
#define IMAGE_LIBRARY_PROCESS_TERM 0x0002 // Reserved.
#define IMAGE_LIBRARY_THREAD_INIT 0x0004  // Reserved.
#define IMAGE_LIBRARY_THREAD_TERM 0x0008  // Reserved.

// SDK supplied with VC2008 does not contain this constant, so we must define it here
#ifndef IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA
#define IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA 0x0020 // Image can handle a high entropy 64-bit virtual address space.
#endif

// SDK supplied with VC2008 does not contain this constant, so we must define it here
#ifndef IMAGE_DLLCHARACTERISTICS_GUARD_CF
#define IMAGE_DLLCHARACTERISTICS_GUARD_CF 0x4000 // Image supports Control Flow Guard.
#endif

// Bitfield values and names for the DllCharacteritics flags
static const WORD_FLAG_DESCRIPTIONS DllCharacteristics[] =
    {
        {IMAGE_DLLCHARACTERISTICS_HIGH_ENTROPY_VA, "Image can handle a high entropy 64-bit virtual address space"},
        {IMAGE_DLLCHARACTERISTICS_DYNAMIC_BASE, "Image can be relocated at load time"},
        {IMAGE_DLLCHARACTERISTICS_FORCE_INTEGRITY, "Code integrity checks are forced"},
        {IMAGE_DLLCHARACTERISTICS_NX_COMPAT, "Image is NX compatible"},
        {IMAGE_DLLCHARACTERISTICS_NO_ISOLATION, "Image understands isolation and doesn't want it"},
        {IMAGE_DLLCHARACTERISTICS_NO_SEH, "Image does not use SEH, no SE handler may reside in this image"},
        {IMAGE_DLLCHARACTERISTICS_NO_BIND, "Do not bind this image"},
        {IMAGE_DLLCHARACTERISTICS_WDM_DRIVER, "Driver uses WDM model"},
        {IMAGE_DLLCHARACTERISTICS_GUARD_CF, "Image supports Control Flow Guard"},
        {IMAGE_DLLCHARACTERISTICS_TERMINAL_SERVER_AWARE, "Image is Terminal Server aware"},
};

#define NUMBER_DLL_CHARACTERISTICS _countof(DllCharacteristics)

// Names of the data directory elements that are defined
static const char* const ImageDirectoryNames[] = {
    "Export directory:", "Import directory:", "Resource directory:", "Exception directory:",
    "Security directory:", "Base relocation table:", "Debug directory:",
    "Architecture-specific data:", "RVA of global pointer:", "Thread local storage directory:",
    "Load configuration directory:", "Bound import directory:", "Import address table:",
    "Delay load import descriptors:", "COM run-time descriptor:"};

#define NUMBER_IMAGE_DIRECTORY_ENTRIES _countof(ImageDirectoryNames)

void COptionalFileHeaderDumper::DumpCore(CFileStream* outStream)
{
    UINT width = 31;
    const char* s;
    UINT i;

    PIMAGE_OPTIONAL_HEADER32 optionalHeader = m_peFile.GetPEOptionalHeader();
    PIMAGE_OPTIONAL_HEADER64 optionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)optionalHeader;

    if (optionalHeader != NULL)
    {
        outStream->fprintf("Optional Header:\n");

        outStream->fprintf("  %-*s0x%04X\n", width, "Magic:", optionalHeader->Magic);
        outStream->fprintf("  %-*s%u.%u\n", width, "Linker Version:", optionalHeader->MajorLinkerVersion, optionalHeader->MinorLinkerVersion);
        outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Code:", optionalHeader->SizeOfCode, optionalHeader->SizeOfCode);
        outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Initialized Data:", optionalHeader->SizeOfInitializedData, optionalHeader->SizeOfInitializedData);
        outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Uninitialized Data:", optionalHeader->SizeOfUninitializedData, optionalHeader->SizeOfUninitializedData);
        outStream->fprintf("  %-*s0x%08X\n", width, "Address of Entry Point:", optionalHeader->AddressOfEntryPoint);
        outStream->fprintf("  %-*s0x%08X\n", width, "Base of Code:", optionalHeader->BaseOfCode);
        if (m_peFile.Is64Bit())
        {
            outStream->fprintf("  %-*s0x%016I64X\n", width, "Image Base:", optionalHeader64->ImageBase);
        }
        else
        {
            outStream->fprintf("  %-*s0x%08X\n", width, "Base of Data:", optionalHeader->BaseOfData);
            outStream->fprintf("  %-*s0x%08X\n", width, "Image Base:", optionalHeader->ImageBase);
        }

        outStream->fprintf("  %-*s0x%08X\n", width, "Section Align:", optionalHeader->SectionAlignment);
        outStream->fprintf("  %-*s0x%08X\n", width, "File Align:", optionalHeader->FileAlignment);
        outStream->fprintf("  %-*s%u.%u\n", width, "Operating System Version:", optionalHeader->MajorOperatingSystemVersion, optionalHeader->MinorOperatingSystemVersion);
        outStream->fprintf("  %-*s%u.%u\n", width, "Image Version:", optionalHeader->MajorImageVersion, optionalHeader->MinorImageVersion);
        outStream->fprintf("  %-*s%u.%u\n", width, "Subsystem Version:", optionalHeader->MajorSubsystemVersion, optionalHeader->MinorSubsystemVersion);
        outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Image:", optionalHeader->SizeOfImage, optionalHeader->SizeOfImage);
        outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Headers:", optionalHeader->SizeOfHeaders, optionalHeader->SizeOfHeaders);
        outStream->fprintf("  %-*s0x%08X\n", width, "Checksum:", optionalHeader->CheckSum);
        switch (optionalHeader->Subsystem)
        {
        case IMAGE_SUBSYSTEM_NATIVE:
            s = "Image doesn't require a subsystem";
            break;
        case IMAGE_SUBSYSTEM_WINDOWS_GUI:
            s = "Windows GUI subsystem";
            break;
        case IMAGE_SUBSYSTEM_WINDOWS_CUI:
            s = "Windows character subsystem";
            break;
        case IMAGE_SUBSYSTEM_OS2_CUI:
            s = "OS/2 character subsystem";
            break;
        case IMAGE_SUBSYSTEM_POSIX_CUI:
            s = "Posix character subsystem";
            break;
        case IMAGE_SUBSYSTEM_NATIVE_WINDOWS:
            s = "Native Win9x driver";
            break;
        case IMAGE_SUBSYSTEM_WINDOWS_CE_GUI:
            s = "Windows CE";
            break;
        case IMAGE_SUBSYSTEM_EFI_APPLICATION:
            s = "EFI Application";
            break;
        case IMAGE_SUBSYSTEM_EFI_BOOT_SERVICE_DRIVER:
            s = "EFI Boot Service Driver";
            break;
        case IMAGE_SUBSYSTEM_EFI_RUNTIME_DRIVER:
            s = "EFI Runtime Driver";
            break;
        case IMAGE_SUBSYSTEM_EFI_ROM:
            s = "EFI ROM";
            break;
        case IMAGE_SUBSYSTEM_XBOX:
            s = "Xbox";
            break;
        case IMAGE_SUBSYSTEM_WINDOWS_BOOT_APPLICATION:
            s = "Windows Boot Application";
            break;
        default:
            s = "Unknown subsystem";
        }
        outStream->fprintf("  %-*s0x%04X (%s)\n", width, "Subsystem:", optionalHeader->Subsystem, s);

        outStream->fprintf("  %-*s0x%04X\n", width, "DLL Characteristics:", optionalHeader->DllCharacteristics);
        WORD dllCharacteristics = optionalHeader->DllCharacteristics;
        for (i = 0; i < NUMBER_DLL_CHARACTERISTICS; i++)
        {
            if (dllCharacteristics & DllCharacteristics[i].flag)
            {
                outStream->fprintf("    %s\n", DllCharacteristics[i].name);
                dllCharacteristics &= ~DllCharacteristics[i].flag;
            }
        }
        if (dllCharacteristics != 0)
        {
            // Zustaly nejake flagy, ktere nezname.
            outStream->fprintf("    0x%04X\n", dllCharacteristics);
        }

        PIMAGE_DATA_DIRECTORY pIDD;
        DWORD NumberOfRvaAndSizes;

        if (m_peFile.Is64Bit())
        {
            outStream->fprintf("  %-*s0x%016I64X (%I64u)\n", width, "Size of Stack Reserve:", optionalHeader64->SizeOfStackReserve, optionalHeader64->SizeOfStackReserve);
            outStream->fprintf("  %-*s0x%016I64X (%I64u)\n", width, "Size of Stack Commit:", optionalHeader64->SizeOfStackCommit, optionalHeader64->SizeOfStackCommit);
            outStream->fprintf("  %-*s0x%016I64X (%I64u)\n", width, "Size of Heap Reserve:", optionalHeader64->SizeOfHeapReserve, optionalHeader64->SizeOfHeapReserve);
            outStream->fprintf("  %-*s0x%016I64X (%I64u)\n", width, "Size of Heap Commit:", optionalHeader64->SizeOfHeapCommit, optionalHeader64->SizeOfHeapCommit);
            pIDD = optionalHeader64->DataDirectory;
            NumberOfRvaAndSizes = optionalHeader64->NumberOfRvaAndSizes;
        }
        else
        {
            outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Stack Reserve:", optionalHeader->SizeOfStackReserve, optionalHeader->SizeOfStackReserve);
            outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Stack Commit:", optionalHeader->SizeOfStackCommit, optionalHeader->SizeOfStackCommit);
            outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Heap Reserve:", optionalHeader->SizeOfHeapReserve, optionalHeader->SizeOfHeapReserve);
            outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size of Heap Commit:", optionalHeader->SizeOfHeapCommit, optionalHeader->SizeOfHeapCommit);
            pIDD = optionalHeader->DataDirectory;
            NumberOfRvaAndSizes = optionalHeader->NumberOfRvaAndSizes;
        }

        outStream->fprintf("  %-*s0x%08X\n", width, "Loader Flags:", m_peFile.Is64Bit() ? optionalHeader64->LoaderFlags : optionalHeader->LoaderFlags);
        outStream->fprintf("  %-*s%u\n", width, "Number of RVA and Sizes:", NumberOfRvaAndSizes);

        outStream->fprintf("\nData Directory:\n");
        for (i = 0; i < NumberOfRvaAndSizes; i++, pIDD++)
        {
            if (pIDD->VirtualAddress == 0 &&
                pIDD->Size == 0)
            {
                outStream->fprintf("  %-31s VA: 0           Size: 0\n",
                                   (i >= NUMBER_IMAGE_DIRECTORY_ENTRIES)
                                       ? "(unknown directory entry):"
                                       : ImageDirectoryNames[i]);
            }
            else
            {
                outStream->fprintf("  %-31s VA: 0x%08X  Size: 0x%08X (%u)\n",
                                   (i >= NUMBER_IMAGE_DIRECTORY_ENTRIES)
                                       ? "(unknown directory entry):"
                                       : ImageDirectoryNames[i],
                                   pIDD->VirtualAddress,
                                   pIDD->Size,
                                   pIDD->Size);
            }
        }
    }
}

const _TCHAR* COptionalFileHeaderDumper::GetExceptionMessage()
{
    return _T("Optional File Header is corrupted.");
}

void CExportTableDumper::DumpCore(CFileStream* outStream)
{
    UINT width = 31;
    if (m_peFile.GetNumberOfExportedFunctions() > 0)
    {
        PIMAGE_SECTION_HEADER psh;
        PIMAGE_EXPORT_DIRECTORY ped;
        PIMAGE_OPTIONAL_HEADER32 poh;
        DWORD* pEntryPoints;
        WORD* pNameOrdinals;
        DWORD* pNames;
        int i;
        char* sectionName;
        char buff[100];
        BOOL* eaIndexDone;

        DWORD exportSectionBegin = 0;
        DWORD exportSectionLen = 0;
        LPBYTE pBase;

        poh = m_peFile.GetPEOptionalHeader();
        if (poh != NULL)
        {
            if (!m_peFile.Is64Bit())
            {
                if (poh->NumberOfRvaAndSizes > 0)
                {
                    exportSectionBegin = poh->DataDirectory[0].VirtualAddress;
                    exportSectionLen = poh->DataDirectory[0].Size;
                }
            }
            else
            {
                PIMAGE_OPTIONAL_HEADER64 poh64 = (PIMAGE_OPTIONAL_HEADER64)poh;

                if (poh64->NumberOfRvaAndSizes > 0)
                {
                    exportSectionBegin = poh64->DataDirectory[0].VirtualAddress;
                    exportSectionLen = poh64->DataDirectory[0].Size;
                }
            }
        }

        /* get section header and pointer to data directory for .edata section */
        if (!m_peFile.ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_EXPORT, (LPVOID*)&ped, &psh))
            return;

        pBase = -(int)psh->VirtualAddress +
                (int)psh->PointerToRawData +
                m_peFile.GetMappingBaseAddress();

        /* determine the offset of the export function names */
        pNames = (DWORD*)((int)ped->AddressOfNames + pBase);
        pEntryPoints = (DWORD*)((int)ped->AddressOfFunctions + pBase);
        pNameOrdinals = (WORD*)((int)ped->AddressOfNameOrdinals + pBase);
        sectionName = (char*)((int)ped->Name + pBase);

        outStream->fprintf("Export Table:\n");

        outStream->fprintf("  %-*s%s\n", width, "Name:", sectionName);
        TimeDateStampToString(ped->TimeDateStamp, buff);
        outStream->fprintf("  %-*s0x%08X%s\n", width, "Time Date Stamp:", ped->TimeDateStamp, buff);

        outStream->fprintf("  %-*s%u.%02u\n", width, "Version:", ped->MajorVersion, ped->MinorVersion);
        outStream->fprintf("  %-*s%u\n", width, "Ordinal Base:", ped->Base);
        outStream->fprintf("  %-*s%u\n", width, "Number of Functions:", ped->NumberOfFunctions);
        outStream->fprintf("  %-*s%u\n", width, "Number of Names:", ped->NumberOfNames);

        // existuji funkce, ktere jsou exportovany podle ordinal hodnoty
        if (ped->NumberOfFunctions > ped->NumberOfNames)
        {
            // vytvorim pole BOOLu pro kazdou funkci
            eaIndexDone = (BOOL*)malloc(ped->NumberOfFunctions * sizeof(BOOL));
            if (eaIndexDone == NULL)
            {
                outStream->fprintf("OUT OF MEMORY!\n");
                return;
            }
            // a oznacim je jako nezpracovane
            ZeroMemory(eaIndexDone, ped->NumberOfFunctions * sizeof(BOOL));
        }
        else
            eaIndexDone = NULL;

        outStream->fprintf("\n");
        outStream->fprintf("  Ordinal   Entry Point   Name\n");

        for (i = 0; i < (int)ped->NumberOfNames; i++)
        {
            char* name;
            WORD ordinal;
            DWORD entryPoint;
            //      if (pEntryPoints[i] == 0)  //j.r. s touhle podminkou to neslapalo pro cabinet.dll
            //        continue;                // podle specifikace nechapu, proc jsem ji sem puvodne dal (mozna copy&paste)
            name = (char*)(pNames[i] + pBase);
            ordinal = (WORD)ped->Base + pNameOrdinals[i];
            entryPoint = pEntryPoints[pNameOrdinals[i]];
            if (entryPoint >= exportSectionBegin && entryPoint < exportSectionBegin + exportSectionLen)
            {
                char* forward;
                forward = (char*)((int)entryPoint + pBase);
                outStream->fprintf("  %7u                 %s (forwarded to %s)\n", ordinal, name, forward);
            }
            else
                outStream->fprintf("  %7u   0x%08X    %s\n", ordinal, entryPoint, name);
            if (eaIndexDone != NULL)
                eaIndexDone[pNameOrdinals[i]] = TRUE;
        }
        if (ped->NumberOfFunctions > ped->NumberOfNames)
        {
            const char* name;
            WORD ordinal;
            DWORD entryPoint;
            for (i = 0; i < (int)ped->NumberOfFunctions; i++)
            {
                if (!eaIndexDone[i] && pEntryPoints[i] != NULL)
                {
                    name = "(export by ordinal)";
                    ordinal = (WORD)ped->Base + i;
                    entryPoint = pEntryPoints[i];
                    if (entryPoint >= exportSectionBegin && entryPoint < exportSectionBegin + exportSectionLen)
                    {
                        char* forward;
                        forward = (char*)((int)entryPoint + pBase);
                        outStream->fprintf("  %7u                 %s (forwarded to %s)\n", ordinal, name, forward);
                    }
                    else
                        outStream->fprintf("  %7u   0x%08X    %s\n", ordinal, entryPoint, name);
                }
            }
            free(eaIndexDone);
        }
    }
}

const _TCHAR* CExportTableDumper::GetExceptionMessage()
{
    return _T("Export Table is corrupted.");
}

void CImportTableDumper::DumpCore(CFileStream* outStream)
{
    UINT width = 36;
    PIMAGE_IMPORT_DESCRIPTOR pid;
    PIMAGE_SECTION_HEADER psh;
    int nCnt = 0, nSize = 0, first = TRUE;
    LPBYTE pBase;

    if (!m_peFile.ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_IMPORT, (LPVOID*)&pid, &psh))
        return;

    if (pid->Name == 0)
        return;

    outStream->fprintf("Import Table:\n");

    pBase = -(int)psh->VirtualAddress +
            (int)psh->PointerToRawData +
            (LPBYTE)m_peFile.GetMappingBaseAddress();

    /* extract all import modules */
    while (pid->Name != 0)
    {
        char buff[100];
        DWORD dwFunction;
        DWORD dwFunction2;
        char* moduleName = (char*)(pBase + pid->Name);
        if (!first)
            outStream->fprintf("\n");
        else
            first = FALSE;
        outStream->fprintf("  %s\n", moduleName);
        outStream->fprintf("    %-*s0x%08X\n", width, "Import Address Table:", pid->FirstThunk);
        outStream->fprintf("    %-*s0x%08X\n", width, "Import Name Table:", pid->OriginalFirstThunk);
        // About "Bogus" values of TimeDateStamp: according to http://en.wikibooks.org/wiki/X86_Disassembly/Windows_Executable_Files,
        // it must match TimeDateStamp in the bound module's FileHeader
        TimeDateStampToString(pid->TimeDateStamp, buff);
        outStream->fprintf("    %-*s0x%08X%s\n", width, "Time Date Stamp:", pid->TimeDateStamp, buff);
        outStream->fprintf("    %-*s0x%08X\n", width, "Index of first forwarder reference:", pid->ForwarderChain);
        outStream->fprintf("\n");

        /* extract all import functions */
        if (pid->OriginalFirstThunk != 0)
        {
            dwFunction = pid->OriginalFirstThunk;
            dwFunction2 = pid->FirstThunk;
        }
        else
        {
            dwFunction = pid->FirstThunk;
            dwFunction2 = 0;
        }
        while (dwFunction != 0 && *(DWORD*)(pBase + dwFunction) != NULL)
        {
            DWORD hint_ordinal;
            BOOL bOrdinal;

            hint_ordinal = (*(DWORD*)(pBase + dwFunction) & 0x7FFFFFFF);
            if (!m_peFile.Is64Bit())
            {
                bOrdinal = (*(DWORD*)(pBase + dwFunction) & IMAGE_ORDINAL_FLAG32) ? TRUE : FALSE;
            }
            else
            {
                bOrdinal = (*(ULONGLONG*)(pBase + dwFunction) & IMAGE_ORDINAL_FLAG64) ? TRUE : FALSE;
            }
            if (bOrdinal)
            { // Patera 2009.12.04: The same problem as with IA64 hit on some Vista32 drivers (beep/disk/ndis.sys)
                // Therefore I added the IsBadReadPtr() test.
                if ((pid->OriginalFirstThunk != 0) && !IsBadReadPtr(pBase + dwFunction2, sizeof(DWORD)))
                    if (!m_peFile.Is64Bit())
                        outStream->fprintf("    0x%08X             Import by Ordinal %7u\n", *(DWORD*)(pBase + dwFunction2), hint_ordinal);
                    else
                        // Same problem as below
                        outStream->fprintf("              Import by Ordinal %7u\n", hint_ordinal);
                //          outStream->fprintf("    0x%08X             Import by Ordinal %7u\n", *(DWORD *)(pBase + dwFunction2), hint_ordinal);
                else
                    outStream->fprintf("                           Import by Ordinal %7u\n", hint_ordinal);
            }
            else
            {
                PIMAGE_IMPORT_BY_NAME pibn = (PIMAGE_IMPORT_BY_NAME)(pBase + hint_ordinal);
                if ((pid->OriginalFirstThunk != 0) && !IsBadReadPtr(pBase + dwFunction2, sizeof(DWORD)))
                    if (!m_peFile.Is64Bit())
                        outStream->fprintf("    0x%08X   %7u   %s\n", *(DWORD*)(pBase + dwFunction2), pibn->Hint, pibn->Name);
                    else
                        // Patera 2006.04.18: The meaning of pBase+dwFunction2 is unknown.
                        // Although it works for all ADM64 sample files I have seen, it doesn't
                        // work for any IA64 executable. In some cases it is outside of the file,
                        // in some cases it references to some strange piece of the binary file.
                        // I haven't found any sample code that parses this thing (called IMAGE_THUNK_DATA64)
                        // directly in disk file. All tools either ignore it or parse it once the executable
                        // is loaded into memory where it is modified against the disk image.
                        outStream->fprintf("    %7u   %s\n", pibn->Hint, pibn->Name);
                //            outStream->fprintf("    0x%016I64X   %7u   %s\n", *(ULONGLONG *)(pBase + dwFunction2), pibn->Hint, pibn->Name);
                else
                    outStream->fprintf("    %7u   %s\n", pibn->Hint, pibn->Name);
            }
            dwFunction += 4;
            dwFunction2 += 4;
            if (m_peFile.Is64Bit())
            {
                dwFunction += 4;
                dwFunction2 += 4;
            }
        }

        pid++;
    }
}

const _TCHAR* CImportTableDumper::GetExceptionMessage()
{
    return _T("Import Table is corrupted.");
}

//
// Section characteristics.
//

static const DWORD_FLAG_DESCRIPTIONS SectionCharacteristics[] =
    {
        {IMAGE_SCN_CNT_CODE, "Section contains code"},
        {IMAGE_SCN_CNT_INITIALIZED_DATA, "Section contains initialized data"},
        {IMAGE_SCN_CNT_UNINITIALIZED_DATA, "Section contains uninitialized data"},
        {IMAGE_SCN_LNK_INFO, "Section contains comments or some other type of information"},
        {IMAGE_SCN_LNK_REMOVE, "Section contents will not become part of image"},
        {IMAGE_SCN_LNK_COMDAT, "Section contents comdat"},
        {IMAGE_SCN_NO_DEFER_SPEC_EXC, "Reset speculative exceptions handling bits in the TLB entries for this section"},
        {IMAGE_SCN_GPREL, "Section content can be accessed relative to GP"},
        {IMAGE_SCN_LNK_NRELOC_OVFL, "Section contains extended relocations"},
        {IMAGE_SCN_MEM_DISCARDABLE, "Section can be discarded"},
        {IMAGE_SCN_MEM_NOT_CACHED, "Section is not cachable"},
        {IMAGE_SCN_MEM_NOT_PAGED, "Section is not pageable"},
        {IMAGE_SCN_MEM_SHARED, "Section is shareable"},
        {IMAGE_SCN_MEM_EXECUTE, "Section is executable"},
        {IMAGE_SCN_MEM_READ, "Section is readable"},
        {IMAGE_SCN_MEM_WRITE, "Section is writeable"},
};

#define NUMBER_SECTION_FLAGS _countof(SectionCharacteristics)

void CSectionTableDumper::DumpCore(CFileStream* outStream)
{
    UINT width = 34;
    int nSections = 0;
    int i, nCnt = 0;
    PIMAGE_SECTION_HEADER psh = m_peFile.GetImageSectionHeader();

    if ((m_peFile.ImageFileType(NULL) != IMAGE_NT_SIGNATURE) || !psh)
        return;

    nSections = NumOfSections(m_peFile.GetMappingBaseAddress());

    if (nSections == 0)
        return;

    outStream->fprintf("Section Table:\n");

    /* count the number of chars used in the section names */
    for (i = 0; i < nSections; i++, psh++)
    {
        char sctName[9];
        lstrcpynA(sctName, (const char*)psh->Name, 9);
        if (i > 0)
            outStream->fprintf("\n");
        outStream->fprintf("  Section Header #%d\n", i + 1);
        outStream->fprintf("    %-*s%s\n", width, "Name:", sctName);
        outStream->fprintf("    %-*s0x%08X (%u)\n", width, "Virtual Size:", psh->Misc.VirtualSize, psh->Misc.VirtualSize);
        outStream->fprintf("    %-*s0x%08X\n", width, "Virtual Address:", psh->VirtualAddress);
        outStream->fprintf("    %-*s0x%08X (%u)\n", width, "Size of Raw Data:", psh->SizeOfRawData, psh->SizeOfRawData);
        outStream->fprintf("    %-*s0x%08X\n", width, "File Pointer to Raw Data:", psh->PointerToRawData);
        outStream->fprintf("    %-*s0x%08X\n", width, "File Pointer to Relocation Table:", psh->PointerToRelocations);
        outStream->fprintf("    %-*s0x%08X\n", width, "File Pointer to Line Numbers:", psh->PointerToLinenumbers);
        outStream->fprintf("    %-*s%u\n", width, "Number of Relocations:", psh->NumberOfRelocations);
        outStream->fprintf("    %-*s%u\n", width, "Number of Line Numbers:", psh->NumberOfLinenumbers);
        outStream->fprintf("    %-*s0x%08X\n", width, "Characteristics:", psh->Characteristics);
        int j;
        for (j = 0; j < NUMBER_SECTION_FLAGS; j++)
        {
            if (psh->Characteristics & SectionCharacteristics[j].flag)
                outStream->fprintf("      %s\n", SectionCharacteristics[j].name);
        }
    }
}

const _TCHAR* CSectionTableDumper::GetExceptionMessage()
{
    return _T("Section Table is corrupted.");
}

static const char* const SzDebugFormats[] =
    {
        "UNKNOWN/BORLAND",
        "COFF",
        "CODEVIEW",
        "FPO",
        "MISC",
        "EXCEPTION",
        "FIXUP",
        "OMAP_TO_SRC",
        "OMAP_FROM_SRC",
        "BORLAND",
        "RESERVED10",
        "CLSID",
        "VC_FEATURE",
        "POGO",
        "ILTCG",
        "MPX",
        "REPRO"};

#define NUMBER_DEBUG_FORMATS _countof(SzDebugFormats)

void CDebugDirectoryDumper::DumpCore(CFileStream* outStream)
{
    PIMAGE_OPTIONAL_HEADER32 poh;
    PIMAGE_DEBUG_DIRECTORY debugDir;
    PIMAGE_DATA_DIRECTORY pidd;
    IMAGE_SECTION_HEADER idsh;
    PIMAGE_FILE_HEADER pPEFileHeader;
    unsigned cDebugFormats, i;
    DWORD offsetInto_rdata;
    DWORD va_debug_dir;
    PCSTR szDebugFormat;

    pPEFileHeader = m_peFile.GetPEFileHeader();
    if (pPEFileHeader == NULL)
        return;

    poh = m_peFile.GetPEOptionalHeader();
    if (poh == NULL)
        return;

    if (!m_peFile.Is64Bit())
    {
        pidd = poh->DataDirectory;
    }
    else
    {
        pidd = ((PIMAGE_OPTIONAL_HEADER64)poh)->DataDirectory;
    }
    va_debug_dir = pidd[IMAGE_DIRECTORY_ENTRY_DEBUG].VirtualAddress;
    if (va_debug_dir == 0)
        return;

    // If we found a .debug section, and the debug directory is at the
    // beginning of this section, it looks like a Borland file

    if (m_peFile.GetSectionHdrByName(&idsh, ".debug") && (idsh.VirtualAddress == va_debug_dir))
    {
        debugDir = (PIMAGE_DEBUG_DIRECTORY)(idsh.PointerToRawData + m_peFile.GetMappingBaseAddress());
        cDebugFormats = pidd[IMAGE_DIRECTORY_ENTRY_DEBUG].Size;
    }
    else // Look for microsoft debug directory in the .rdata section
    {
        // See if there's even any debug directories to speak of...
        cDebugFormats = pidd[IMAGE_DIRECTORY_ENTRY_DEBUG].Size / sizeof(IMAGE_DEBUG_DIRECTORY);
        if (cDebugFormats == 0)
            return;

        if (m_peFile.GetSectionHdrByName(&idsh, ".rdata") || m_peFile.GetSectionHdrByName(&idsh, ".text"))
        {
            // IA64 & AMD64 binaries have debug info in .text, x86 in .rdata
            offsetInto_rdata = va_debug_dir - idsh.VirtualAddress;
            debugDir = MakePtr(PIMAGE_DEBUG_DIRECTORY, m_peFile.GetMappingBaseAddress(), idsh.PointerToRawData + offsetInto_rdata);
        }
        else
        {
            return;
        }
    }

    outStream->fprintf(
        "Debug Formats in File:\n"
        "  Type            Size       Address    FilePtr    Charactr   TimeData   Version\n");

    for (i = 0; i < cDebugFormats; i++)
    {
        szDebugFormat = (debugDir->Type < NUMBER_DEBUG_FORMATS)
                            ? SzDebugFormats[debugDir->Type]
                            : "Unknown";

        outStream->fprintf("  %-15s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X %u.%02u\n",
                           szDebugFormat, debugDir->SizeOfData, debugDir->AddressOfRawData,
                           debugDir->PointerToRawData, debugDir->Characteristics,
                           debugDir->TimeDateStamp, debugDir->MajorVersion,
                           debugDir->MinorVersion);

        void* debugData = m_peFile.GetMappingBaseAddress() + debugDir->PointerToRawData;

        switch (debugDir->Type)
        {
        case IMAGE_DEBUG_TYPE_CODEVIEW:
            DumpCodeView(outStream, debugData, debugDir->SizeOfData);
            break;
        case IMAGE_DEBUG_TYPE_MISC:
            DumpMisc(outStream, debugData, debugDir->SizeOfData);
            break;
        }

        debugDir++;
    }
}

void CDebugDirectoryDumper::DumpCodeView(CFileStream* outStream, const void* data, DWORD size)
{
    LPCSTR pdbFileName = NULL;
    DWORD maxCchPdbFileName;
    DWORD cvType = *reinterpret_cast<const DWORD*>(data);
    if (cvType == CV_TYPE_PDB20)
    {
        const CV_INFO_PDB20* pdb20 = reinterpret_cast<const CV_INFO_PDB20*>(data);
        if (size >= sizeof(CV_INFO_PDB20))
        {
            pdbFileName = reinterpret_cast<LPCSTR>(pdb20->PdbFileName);
            maxCchPdbFileName = size - sizeof(CV_INFO_PDB20) + 1;
        }
    }
    else if (cvType == CV_TYPE_PDB70)
    {
        const CV_INFO_PDB70* pdb70 = reinterpret_cast<const CV_INFO_PDB70*>(data);
        if (size >= sizeof(CV_INFO_PDB70))
        {
            pdbFileName = reinterpret_cast<LPCSTR>(pdb70->PdbFileName);
            maxCchPdbFileName = size - sizeof(CV_INFO_PDB70) + 1;
        }
    }

    if (pdbFileName != NULL)
    {
        _ASSERTE(maxCchPdbFileName >= 1);
        outStream->fprintf("    (%.4s, %.*s)\n", &cvType, maxCchPdbFileName - 1, pdbFileName);
    }
}

void CDebugDirectoryDumper::DumpMisc(CFileStream* outStream, const void* data, DWORD size)
{
    DWORD minimumSize = sizeof(IMAGE_DEBUG_MISC);
    if (size >= minimumSize)
    {
        const IMAGE_DEBUG_MISC* pdm = reinterpret_cast<const IMAGE_DEBUG_MISC*>(data);
        if (pdm->DataType == IMAGE_DEBUG_MISC_EXENAME)
        {
            if (pdm->Unicode)
            {
                // Not supported right now.
            }
            else
            {
                // Petr: VC2013 pise pro nasledujici radek chybu: error C2070: 'unknown': illegal sizeof operand
                //static_assert(sizeof(IMAGE_DEBUG_MISC::Data) == 1, "Invalid definition of the IMAGE_DEBUG_MISC structure");
                // proto jsem ho zmenil na:
                IMAGE_DEBUG_MISC* ptr_IMAGE_DEBUG_MISC = NULL; // dal by se pouzit i 'pdm', ale takhle je to cistci
                static_assert(sizeof(ptr_IMAGE_DEBUG_MISC->Data) == 1, "Invalid definition of the IMAGE_DEBUG_MISC structure");
                DWORD maxCch = size - sizeof(IMAGE_DEBUG_MISC) + 1; // IMAGE_DEBUG_MISC includes one character.
                _ASSERTE(maxCch >= 1);
                if (pdm->Data[0] != '\0')
                {
                    outStream->fprintf("    (Image name: %.*s)\n", maxCch - 1, pdm->Data);
                }
            }
        }
    }
}

const _TCHAR* CDebugDirectoryDumper::GetExceptionMessage()
{
    return _T("Debug Directory is corrupted.");
}

static void BuildPEDumperChain(CPEFile& peFile, CPEDumper** chain, DWORD maxLength, DWORD* actualLength)
{
    *actualLength = min(maxLength, g_cfgChainLength);

    for (DWORD i = 0; i < *actualLength; i++)
    {
        chain[i] = g_cfgChain[i].pDumperCfg->pfnFactory(&peFile);
    }
}

static void FreePEDumperChain(CPEDumper** chain, DWORD actualLength)
{
    for (DWORD i = 0; i < actualLength; i++)
    {
        delete chain[i];
    }
}

BOOL DumpFileInfo(LPVOID lpFile, DWORD fileSize, FILE* outStream)
{
    int exceptionCount = 0;
    CPEFile PEFile(lpFile, fileSize);
    CPEDumper* peDumperChain[AVAILABLE_PE_DUMPERS];
    DWORD peDumperChainLength;
    BOOL hasOutput = FALSE;

    CFileStream stream(outStream);
    stream.SetDirty(FALSE);

    BuildPEDumperChain(PEFile, peDumperChain, _countof(peDumperChain), &peDumperChainLength);

    __try
    {
        for (DWORD i = 0; i < peDumperChainLength; i++)
        {
            if (stream.GetDirty())
            {
                stream.fprintf("\n");
                stream.SetDirty(FALSE);
            }

            peDumperChain[i]->Dump(&stream);

            if (peDumperChain[i]->WasException())
            {
                ++exceptionCount;
                stream.SetDirty(FALSE);
            }
        }
        if (peDumperChainLength == 0 || stream.IsEmpty())
            stream.fprintf("%s", SalGeneral->LoadStr(HLanguage, IDS_HIDDEN_SECTIONS));
    }
    __except (HandleFileException(GetExceptionInformation(), lpFile, fileSize))
    {
        // chyba v souboru
        stream.fprintf("\n---- EXCEPTION: Error reading file.\n\n");
        exceptionCount++;
    }

    FreePEDumperChain(peDumperChain, peDumperChainLength);

    //  return exceptionCount == 0; // viewer se zda odladeny, uz si nenechame posilat bug reporty
    return TRUE;
}

void CNTDumperWithExceptionHandler::Dump(CFileStream* outStream)
{
    __try
    {
        if (m_peFile.ImageFileType(NULL) == IMAGE_NT_SIGNATURE)
        {
            DumpCore(outStream);
        }
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        outStream->fprintf("\n---- EXCEPTION: %s\n\n", GetExceptionMessage());
        SetException();
    }
}

void CFileTypeDumper::Dump(CFileStream* outStream)
{
    int exceptionCount = 0;

    // image type
    DWORD imageType = m_peFile.ImageFileType(&exceptionCount);
    const char* p;
    switch (imageType)
    {
    case IMAGE_DOS_SIGNATURE:
        p = "MS-DOS EXECUTABLE";
        break;
    case IMAGE_OS2_SIGNATURE:
        p = "OS2 EXECUTABLE";
        break;
    case IMAGE_VXD_SIGNATURE:
        p = "VXD EXECUTABLE";
        break;
    case IMAGE_NT_SIGNATURE:
        p = "WINDOWS EXECUTABLE";
        break;
    default:
        p = "UNKNOWN";
    }
    outStream->fprintf("File Type: %s\n", p);
}

void CLoadConfigDumper::DumpCore(CFileStream* outStream)
{
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32 = m_peFile.GetPEOptionalHeader();
    if (pOptionalHeader32 == NULL)
    {
        return;
    }

    bool bHasLoadConfig = false;
    DWORD dwLoadConfigSize;

    if (m_peFile.Is64Bit())
    {
        PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptionalHeader32;
        bHasLoadConfig = (pOptionalHeader64->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG);
        if (bHasLoadConfig)
        {
            dwLoadConfigSize = pOptionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size;
        }
    }
    else
    {
        bHasLoadConfig = (pOptionalHeader32->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG);
        if (bHasLoadConfig)
        {
            dwLoadConfigSize = pOptionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG].Size;
        }
    }

    if (!bHasLoadConfig || dwLoadConfigSize < sizeof(DWORD)) // IMAGE_LOAD_CONFIG_DIRECTORY::Size
    {
        return;
    }

    PIMAGE_LOAD_CONFIG_DIRECTORY32 pLoadConfig32;
    PIMAGE_SECTION_HEADER pSectionHeader;
    if (!m_peFile.ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_LOAD_CONFIG, (LPVOID*)&pLoadConfig32, &pSectionHeader))
    {
        return;
    }

    PIMAGE_LOAD_CONFIG_DIRECTORY64 pLoadConfig64 = (PIMAGE_LOAD_CONFIG_DIRECTORY64)pLoadConfig32;
    dwLoadConfigSize = min(dwLoadConfigSize, pLoadConfig32->Size);

    outStream->fprintf("Load Configuration:\n");

    int width = 31;
    outStream->fprintf("  %-*s0x%08X (%u)\n", width, "Size:", pLoadConfig32->Size, pLoadConfig32->Size);

    do
    {
        if (!STRUCT_HAS_FIELD(IMAGE_LOAD_CONFIG_DIRECTORY32, TimeDateStamp, dwLoadConfigSize))
        {
            break;
        }

        char buff[100];
        buff[0] = '\0';
        TimeDateStampToString(pLoadConfig32->TimeDateStamp, buff);
        outStream->fprintf("  %-*s0x%08X%s\n", width, "Time Date Stamp:", pLoadConfig32->TimeDateStamp, buff);

        if (m_peFile.Is64Bit())
        {
            if (!STRUCT_HAS_FIELD(IMAGE_LOAD_CONFIG_DIRECTORY64, SecurityCookie, dwLoadConfigSize))
            {
                break;
            }
            outStream->fprintf("  %-*s0x%016I64X\n", width, "Security Cookie VA:", pLoadConfig64->SecurityCookie);
        }
        else
        {
            if (!STRUCT_HAS_FIELD(IMAGE_LOAD_CONFIG_DIRECTORY32, SecurityCookie, dwLoadConfigSize))
            {
                break;
            }
            outStream->fprintf("  %-*s0x%08X\n", width, "Security Cookie VA:", pLoadConfig32->SecurityCookie);
        }
    } while (FALSE);
}

const _TCHAR* CLoadConfigDumper::GetExceptionMessage()
{
    return _T("Load Configuration Directory is corrupted.");
}

//
// ****************************************************************************
// CBaseResourceDumper
//

CBaseResourceDumper::CBaseResourceDumper(CPEFile& PEFile)
    : CNTDumperWithExceptionHandler(PEFile)
{
    m_pResDir = NULL;
    m_pBase = NULL;
}

void CBaseResourceDumper::DumpCore(CFileStream* outStream)
{
    PIMAGE_SECTION_HEADER pSectionHeader;

    if (m_peFile.ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_RESOURCE, (LPVOID*)&m_pResDir, &pSectionHeader))
    {
        m_pBase = -(int)pSectionHeader->VirtualAddress +
                  (int)pSectionHeader->PointerToRawData +
                  (LPBYTE)m_peFile.GetMappingBaseAddress();

        DumpResources(outStream);
    }
}

bool CBaseResourceDumper::EnumResourceDirectoryEntries(PIMAGE_RESOURCE_DIRECTORY pResourceDir, PFN_EnumResourceDirectoryEntryCallback pfnCallback, void* pUserData)
{
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntry;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntryMax;

    pEntry = (PIMAGE_RESOURCE_DIRECTORY_ENTRY)(pResourceDir + 1);
    pEntryMax = pEntry + (pResourceDir->NumberOfIdEntries + pResourceDir->NumberOfNamedEntries);
    while (pEntry < pEntryMax)
    {
        if (!pfnCallback(pEntry, pUserData))
        {
            return false;
        }
        ++pEntry;
    }

    return true;
}

PIMAGE_RESOURCE_DIRECTORY_ENTRY CBaseResourceDumper::FindResourceDirectoryEntry(PIMAGE_RESOURCE_DIRECTORY pResourceDir, int id)
{
    FindResourceDirectoryEntryById byId;
    byId.pEntry = NULL;
    byId.nId = id;
    EnumResourceDirectoryEntries(pResourceDir, FindResourceDirectoryEntryByIdCallback, &byId);
    return byId.pEntry;
}

bool CBaseResourceDumper::FindResourceDirectoryEntryByIdCallback(PIMAGE_RESOURCE_DIRECTORY_ENTRY pResourceDirEntry, void* pUserData)
{
    FindResourceDirectoryEntryById* byId = (FindResourceDirectoryEntryById*)pUserData;
    if (byId->nId == -1 || (!pResourceDirEntry->NameIsString && pResourceDirEntry->Id == byId->nId))
    {
        byId->pEntry = pResourceDirEntry;

        // Neni potreba pokracovat v enumeraci.
        return false;
    }
    return true;
}

void* CBaseResourceDumper::FindResource(int* paIds, DWORD caIds, PIMAGE_RESOURCE_DATA_ENTRY& pDataEntry)
{
    if (m_pResDir == NULL)
    {
        return NULL;
    }

    PIMAGE_RESOURCE_DIRECTORY pDir = m_pResDir;
    PIMAGE_RESOURCE_DIRECTORY_ENTRY pEntry;

    for (DWORD iLevel = 0;; iLevel++)
    {
        pEntry = FindResourceDirectoryEntry(pDir, paIds[iLevel]);
        if (pEntry == NULL)
        {
            break;
        }

        if (iLevel == caIds - 1)
        {
            if (pEntry->DataIsDirectory)
            {
                // Posledni musi byt uz data.
                break;
            }

            pDataEntry = (PIMAGE_RESOURCE_DATA_ENTRY)OffsetToPtr(pEntry->OffsetToData);
            return RvaToPtr(pDataEntry->OffsetToData);
        }
        else
        {
            if (!pEntry->DataIsDirectory)
            {
                // Vsechny pred tim musi byt adresare.
                break;
            }
            pDir = (PIMAGE_RESOURCE_DIRECTORY)OffsetToPtr(pEntry->OffsetToDirectory);
        }
    }

    return NULL;
}

//
// ****************************************************************************
// CFileVersionResourceDumper
//

#include "BinaryReader.h"

#define READW(r, w) \
    readOk = r.TryReadWord(w); \
    if (!readOk) \
        break;
#define READDW(r, dw) \
    readOk = r.TryReadDWord(dw); \
    if (!readOk) \
        break;
#define VS_TYPE_BINARY 0
#define VS_TYPE_TEXT 1

void CFileVersionResourceDumper::DumpResources(CFileStream* outStream)
{
    DWORD cbVer;
    void* pVer = FindResource(16 /* RT_VERSION */, -1 /* any */, -1 /* any */, cbVer);
    if (pVer == NULL)
    {
        return;
    }

    outStream->fprintf("File Version Information:\n");

    BinaryReader reader(pVer, cbVer);

    if (ReadVsVersionInfo(outStream, reader))
    {
        while (ReadXxxFileInfo(outStream, reader))
        {
            if (!reader.TryAlignDWord())
            {
                break;
            }
        }
    }
}

bool CFileVersionResourceDumper::ReadVsVersionInfo(CFileStream* outStream, BinaryReader& reader)
{
    bool readOk;

    do
    {
        WORD w, wValueLength;

        // VS_VERSIONINFO::wLength
        READW(reader, w);
        if (w < sizeof(WORD) || w > reader.TotalSize())
        {
            break;
        }

        // VS_VERSIONINFO::wValueLength
        READW(reader, wValueLength);
        if (wValueLength < sizeof(VS_FIXEDFILEINFO))
        {
            break;
        }

        // VS_VERSIONINFO::wType
        READW(reader, w);
        if (w != VS_TYPE_BINARY)
        {
            break;
        }

        // VS_VERSIONINFO::szKey
        if (!reader.EqualsUnicodeStringZ(L"VS_VERSION_INFO"))
        {
            break;
        }

        // VS_VERSIONINFO::Padding1
        if (!reader.TryAlignDWord())
        {
            break;
        }

        // VS_VERSIONINFO::Value (VS_FIXEDFILEINFO)
        const VS_FIXEDFILEINFO* pFixedInfo = reinterpret_cast<const VS_FIXEDFILEINFO*>(reader.GetPointer());
        if (!reader.TrySkip(wValueLength))
        {
            break;
        }

        DumpFixedFileInfo(outStream, pFixedInfo);

        // VS_VERSIONINFO::Padding1
        if (!reader.TryAlignDWord())
        {
            break;
        }
    } while (FALSE);

    return readOk;
}

bool CFileVersionResourceDumper::ReadXxxFileInfo(CFileStream* outStream, BinaryReader& reader)
{
    bool readOk;
    WORD wLength;

    do
    {
        WORD w, wValueLength;

        // XxxFileInfo::wLength
        READW(reader, wLength);
        if (wLength < sizeof(WORD) || wLength > reader.TotalSize())
        {
            break;
        }

        BinaryReader subReader(reader.GetPointer(), wLength - sizeof(WORD));

        // XxxFileInfo::wValueLength
        READW(subReader, wValueLength);
        if (wValueLength != 0)
        {
            break;
        }

        // XxxFileInfo::wType
        READW(subReader, w);
        /*if (w != VS_TYPE_BINARY)
    {
    break;
    }*/

        // XxxFileInfo::szKey
        LPCWSTR szKey;
        readOk = subReader.TryReadUnicodeStringZ(szKey);
        if (!readOk)
        {
            break;
        }

        // XxxFileInfo::Padding
        if (!subReader.TryAlignDWord())
        {
            break;
        }

        if (wcscmp(szKey, L"StringFileInfo") == 0)
        {
            readOk = ReadStringTable(outStream, subReader);
        }
        else if (wcscmp(szKey, L"VarFileInfo") == 0)
        {
            //readOk = ReadVar(outStream, subReader);
        }
        else
        {
            // Neznamy typ XxxFileInfo.
            _ASSERTE(0);
        }

    } while (FALSE);

    if (readOk)
    {
        readOk = reader.TrySkip(wLength - sizeof(WORD));
        _ASSERTE(readOk);
    }

    return readOk;
}

bool CFileVersionResourceDumper::ReadStringTable(CFileStream* outStream, BinaryReader& reader)
{
    bool readOk;
    WORD wLength;

    do
    {
        WORD w, wValueLength;

        // StringTable::wLength
        READW(reader, wLength);
        if (wLength < sizeof(WORD) || wLength > reader.TotalSize())
        {
            break;
        }

        BinaryReader subReader(reader.GetPointer(), wLength - sizeof(WORD));

        // StringTable::wValueLength
        READW(subReader, wValueLength);
        if (wValueLength != 0)
        {
            break;
        }

        // StringTable::wType
        READW(subReader, w);
        /*if (w != VS_TYPE_BINARY)
    {
    break;
    }*/

        // StringTable::szKey
        LPCWSTR szKey;
        DWORD cchKey;
        readOk = subReader.TryReadUnicodeStringZ(szKey, cchKey);
        if (!readOk)
        {
            break;
        }

        // 8 hexa znaku.
        readOk = (cchKey == 8);
        if (!readOk)
        {
            break;
        }

        DWORD languageAndCodepage = wcstoul(szKey, NULL, 16);
        LANGID language = HIWORD(languageAndCodepage);
        WORD codePage = LOWORD(languageAndCodepage);

        int width = 31;
        outStream->fprintf("  %-*s%u", width, "String File Information:", language);

        char langName[128];
        if (language != 0 && GetLocaleInfoA(MAKELCID(MAKELANGID(language, SUBLANG_NEUTRAL), SORT_DEFAULT), LOCALE_SLANGUAGE, langName, sizeof(langName)))
        {
            outStream->fprintf(" (%s)", langName);
        }
        outStream->fprintf("\n");

        // StringTable::Padding
        if (!subReader.TryAlignDWord())
        {
            break;
        }

        while ((readOk = ReadString(outStream, subReader)) != FALSE)
        {
            readOk = subReader.TryAlignDWord();
            if (!readOk)
            {
                break;
            }
        }
    } while (FALSE);

    if (readOk)
    {
        readOk = reader.TrySkip(wLength - sizeof(WORD));
        _ASSERTE(readOk);
    }

    return readOk;
}

#define STRCONV_STATIC_BUFFER_SIZE 64

class W2A
{
private:
    LPCWSTR m_pstrWString;
    char m_szStaticBuffer[STRCONV_STATIC_BUFFER_SIZE];
    char* m_pszDynamicBuffer;

public:
    W2A(LPCWSTR s);
    ~W2A();

    operator const char*();
};

W2A::W2A(LPCWSTR s)
{
    m_pstrWString = s;
    m_pszDynamicBuffer = NULL;
}

W2A::~W2A()
{
    delete[] m_pszDynamicBuffer;
}

W2A::operator const char*()
{
    if (m_pstrWString == NULL)
    {
        return NULL;
    }

    if (*m_pstrWString == L'\0')
    {
        return "";
    }

    if (m_pszDynamicBuffer != NULL)
    {
        return m_pszDynamicBuffer;
    }

    int len = WideCharToMultiByte(CP_ACP, 0, m_pstrWString, -1,
                                  m_szStaticBuffer, sizeof(m_szStaticBuffer), NULL, NULL);
    if (len > 0)
    {
        return m_szStaticBuffer;
    }

    len = WideCharToMultiByte(CP_ACP, 0, m_pstrWString, -1,
                              NULL, 0, NULL, NULL);

    m_pszDynamicBuffer = new char[len];
    WideCharToMultiByte(CP_ACP, 0, m_pstrWString, -1,
                        m_pszDynamicBuffer, len, NULL, NULL);

    return m_pszDynamicBuffer;
}

bool CFileVersionResourceDumper::ReadString(CFileStream* outStream, BinaryReader& reader)
{
    bool readOk;
    WORD wLength;

    do
    {
        WORD w, wValueLength;

        // String::wLength
        READW(reader, wLength);
        if (wLength < sizeof(WORD) || wLength > reader.TotalSize())
        {
            readOk = false;
            break;
        }

        BinaryReader subReader(reader.GetPointer(), wLength - sizeof(WORD));

        // String::wValueLength
        READW(subReader, wValueLength);
        if (wValueLength * 2U > subReader.TotalSize())
        {
            readOk = false;
            break;
        }

        // String::wType
        READW(subReader, w);
        /*if (w != VS_TYPE_BINARY)
    {
    break;
    }*/

        // String::szKey
        LPCWSTR szKey;
        DWORD cchKey;
        readOk = subReader.TryReadUnicodeStringZ(szKey, cchKey);
        if (!readOk)
        {
            break;
        }

        LPCWSTR szValue = NULL;
        DWORD cchValue = 0U;
        if (wValueLength > 0)
        {
            // String::Padding
            if (!subReader.TryAlignDWord())
            {
                break;
            }

            readOk = subReader.TryReadUnicodeStringZ(szValue, cchValue);
            if (!readOk)
            {
                break;
            }

            if (cchValue + 1 != wValueLength)
            {
                readOk = false;
                break;
            }
        }

        DWORD width = 28;
        outStream->fprintf("    %ls", szKey);
        if (cchValue > 0)
        {
            outStream->fprintf(":");
            if (cchKey < width)
            {
                char spacing[32];
                memset(spacing, ' ', sizeof(spacing));
                spacing[width - cchKey] = '\0';
                outStream->fprintf(spacing);
            }
            outStream->fprintf("%s", (const char*)W2A(szValue));
        }
        outStream->fprintf("\n");
    } while (FALSE);

    if (readOk)
    {
        readOk = reader.TrySkip(wLength - sizeof(WORD));
        _ASSERTE(readOk);
    }

    return readOk;
}

static const DWORD_FLAG_DESCRIPTIONS VsFileFlags[] =
    {
        {VS_FF_DEBUG, "Debug version"},
        {VS_FF_PATCHED, "Patched"},
        {VS_FF_PRERELEASE, "Prerelease"},
        {VS_FF_PRIVATEBUILD, "Private build"},
        {VS_FF_SPECIALBUILD, "Special build"},
};

#define NUMBER_VS_FILE_FLAGS _countof(VsFileFlags)

void CFileVersionResourceDumper::DumpFixedFileInfo(CFileStream* outStream, const VS_FIXEDFILEINFO* pFixedInfo)
{
    if (pFixedInfo->dwSignature != VS_FFI_SIGNATURE)
    {
        return;
    }

    int width = 31;
    outStream->fprintf("  %-*s%d.%d.%d.%d\n", width, "File Version:",
                       HIWORD(pFixedInfo->dwFileVersionMS), LOWORD(pFixedInfo->dwFileVersionMS),
                       HIWORD(pFixedInfo->dwFileVersionLS), LOWORD(pFixedInfo->dwFileVersionLS));
    outStream->fprintf("  %-*s%d.%d.%d.%d\n", width, "Product Version:",
                       HIWORD(pFixedInfo->dwProductVersionMS), LOWORD(pFixedInfo->dwProductVersionMS),
                       HIWORD(pFixedInfo->dwProductVersionLS), LOWORD(pFixedInfo->dwProductVersionLS));

    const char* pszFileType = NULL;
    switch (pFixedInfo->dwFileType)
    {
    case VFT_APP:
        pszFileType = "Application";
        break;
    case VFT_DLL:
        pszFileType = "Dynamic-link library";
        break;
    case VFT_DRV:
        pszFileType = "Device driver";
        break;
    case VFT_FONT:
        pszFileType = "Font";
        break;
    }
    outStream->fprintf("  %-*s0x%08X", width, "File Type:", pFixedInfo->dwFileType);
    if (pszFileType != NULL)
    {
        outStream->fprintf(" (%s)", pszFileType);
    }
    outStream->fprintf("\n");

    const char* pszFileSubtype = NULL;
    if (pFixedInfo->dwFileType == VFT_DRV)
    {
        switch (pFixedInfo->dwFileSubtype)
        {
        case VFT2_DRV_COMM:
            pszFileSubtype = "Communications driver";
            break;
        case VFT2_DRV_DISPLAY:
            pszFileSubtype = "Display driver";
            break;
        case VFT2_DRV_INSTALLABLE:
            pszFileSubtype = "Installable driver";
            break;
        case VFT2_DRV_KEYBOARD:
            pszFileSubtype = "Keyboard driver";
            break;
        case VFT2_DRV_LANGUAGE:
            pszFileSubtype = "Language driver";
            break;
        case VFT2_DRV_MOUSE:
            pszFileSubtype = "Mouse driver";
            break;
        case VFT2_DRV_NETWORK:
            pszFileSubtype = "Network driver";
            break;
        case VFT2_DRV_PRINTER:
            pszFileSubtype = "Printer driver";
            break;
        case VFT2_DRV_SOUND:
            pszFileSubtype = "Sound driver";
            break;
        case VFT2_DRV_SYSTEM:
            pszFileSubtype = "System driver";
            break;
        case VFT2_DRV_VERSIONED_PRINTER:
            pszFileSubtype = "Versioned printer driver";
            break;
        }
    }
    outStream->fprintf("  %-*s0x%08X", width, "File Subtype:", pFixedInfo->dwFileSubtype);
    if (pszFileSubtype != NULL)
    {
        outStream->fprintf(" (%s)", pszFileSubtype);
    }
    outStream->fprintf("\n");

    outStream->fprintf("  %-*s0x%08X\n", width, "File Flags Mask:", pFixedInfo->dwFileFlagsMask);
    outStream->fprintf("  %-*s0x%08X\n", width, "File Flags:", pFixedInfo->dwFileFlags);
    int j;
    DWORD dwFileFlags = pFixedInfo->dwFileFlags /*& pFixedInfo->dwFileFlagsMask*/;
    for (j = 0; j < NUMBER_VS_FILE_FLAGS; j++)
    {
        if (dwFileFlags & VsFileFlags[j].flag)
        {
            outStream->fprintf("    %s\n", VsFileFlags[j].name);
        }
    }
}

int CFileVersionResourceDumper::Bcd(WORD w)
{
    int value;

    value = ((w >> 12) & 0xF) * 1000;
    value += ((w >> 8) & 0xF) * 100;
    value += ((w >> 4) & 0xF) * 10;
    value += (w & 0xF);

    return value;
}

const _TCHAR* CFileVersionResourceDumper::GetExceptionMessage()
{
    return _T("File Version Resources are corrupted.");
}

//
// ****************************************************************************
// CManifestResourceDumper
//

void CManifestResourceDumper::DumpResources(CFileStream* outStream)
{
    DWORD cbManifest;
    const BYTE* pManifest = (const BYTE*)FindResource(24 /* RT_MANIFEST */, -1 /* any */, -1 /* any */, cbManifest);
    if (pManifest == NULL)
    {
        return;
    }

    BOOL isUtf8 = -1;
    if (cbManifest >= 3 && pManifest[0] == 0xEF && pManifest[1] == 0xBB && pManifest[2] == 0xBF)
    {
        isUtf8 = TRUE;
        pManifest += 3;
        cbManifest -= 3;
    }

    bool readOk;

    if (cbManifest > 5 && pManifest[0] == '<' && pManifest[1] == '?' && pManifest[2] == 'x' &&
        pManifest[3] == 'm' && pManifest[4] == 'l')
    {
        BinaryReader declReader(pManifest, cbManifest);
        const void* p;
        DWORD cb;
        readOk = declReader.TryReadUntil('>', p, cb);
        if (readOk)
        {
            char* decl = new char[cb + 1];
            memcpy(decl, p, cb);
            decl[cb] = '\0';
            char* encPos = strstr(decl, "encoding=");
            if (encPos != NULL)
            {
                encPos += 9;
                if (*encPos == '\'' || *encPos == '"')
                {
                    ++encPos;
                    if (_strnicmp(encPos, "utf-8", 5) == 0 || _strnicmp(encPos, "utf8", 4) == 0)
                    {
                        isUtf8 = TRUE;
                    }
                    else
                    {
                        isUtf8 = FALSE;
                    }
                }
            }
            delete[] decl;
        }
    }

    outStream->fprintf("Manifest:\n");

    if (!isUtf8)
    {
        outStream->fprintf("  Unsupported encoding.\n");
        return;
    }

    BinaryReader reader(pManifest, cbManifest);
    const void* p;
    DWORD cb;
    while ((readOk = reader.TryReadLine(p, cb)) != false)
    {
        outStream->fprintf("  %.*s\n", cb, (const char*)p);
    }

    if (!reader.Eos() && reader.TryReadToEnd(p, cb) && cb > 0)
    {
        outStream->fprintf("  %.*s\n", cb, (const char*)p);
    }

    return;
}

const _TCHAR* CManifestResourceDumper::GetExceptionMessage()
{
    return _T("Manifest Resource is corrupted.");
}

//
// ****************************************************************************
// CCorHeaderDumper
//

static BOOL IsManagedAssembly(CPEFile& peFile, OUT DWORD& dwCorHeaderSize)
{
    PIMAGE_OPTIONAL_HEADER32 pOptionalHeader32 = peFile.GetPEOptionalHeader();
    if (pOptionalHeader32 == NULL)
    {
        return FALSE;
    }

    BOOL bHasCorHeader;
    if (peFile.Is64Bit())
    {
        PIMAGE_OPTIONAL_HEADER64 pOptionalHeader64 = (PIMAGE_OPTIONAL_HEADER64)pOptionalHeader32;
        bHasCorHeader = (pOptionalHeader64->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);
        if (bHasCorHeader)
        {
            dwCorHeaderSize = pOptionalHeader64->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
        }
    }
    else
    {
        bHasCorHeader = (pOptionalHeader32->NumberOfRvaAndSizes > IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR);
        if (bHasCorHeader)
        {
            dwCorHeaderSize = pOptionalHeader32->DataDirectory[IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR].Size;
        }
    }

    return bHasCorHeader;
}

static BOOL IsManagedAssembly(CPEFile& peFile)
{
    DWORD dwCorHeaderSize;
    return IsManagedAssembly(peFile, dwCorHeaderSize) && dwCorHeaderSize > 0U;
}

#ifndef COMIMAGE_FLAGS_32BITPREFERRED // v SDK 7.1 (VC2008) jeste neni COMIMAGE_FLAGS_32BITPREFERRED
#define COMIMAGE_FLAGS_32BITPREFERRED 0x00020000
#endif // COMIMAGE_FLAGS_32BITPREFERRED

static const DWORD_FLAG_DESCRIPTIONS CorHeaderFlags[] =
    {
        {COMIMAGE_FLAGS_ILONLY, "IL only"},
        {COMIMAGE_FLAGS_32BITREQUIRED, "32 bit required"},
        {COMIMAGE_FLAGS_IL_LIBRARY, "IL library"},
        {COMIMAGE_FLAGS_STRONGNAMESIGNED, "Strong name signed"},
        {COMIMAGE_FLAGS_NATIVE_ENTRYPOINT, "Native entry point"},
        {COMIMAGE_FLAGS_TRACKDEBUGDATA, "Track debug data"},

        // Following comment was taken from the corhdr.h header file:
        //
        // The COMIMAGE_FLAGS_32BITREQUIRED and COMIMAGE_FLAGS_32BITPREFERRED flags defined below interact as a pair
        // in order to get the performance profile we desire for platform neutral assemblies while retaining backwards
        // compatibility with pre-4.5 runtimes/OSs, which don't know about COMIMAGE_FLAGS_32BITPREFERRED.
        //
        // COMIMAGE_FLAGS_32BITREQUIRED originally meant "this assembly is x86-only" (required to distinguish platform
        // neutral assemblies which also mark their PE MachineType as IMAGE_FILE_MACHINE_I386).
        //
        // COMIMAGE_FLAGS_32BITPREFERRED has been added so we can create a sub-class of platform neutral assembly that
        // prefers to be loaded into 32-bit environment for perf reasons, but is still compatible with 64-bit
        // environments.
        //
        // In order to retain maximum backwards compatibility you cannot simply read or write one of these flags
        // however. You must treat them as a pair, a two-bit field with the following meanings:
        //
        //  32BITREQUIRED  32BITPREFERRED
        //        0               0         :   no special meaning, MachineType and ILONLY flag determine image requirements
        //        0               1         :   illegal, reserved for future use
        //        1               0         :   image is x86-specific
        //        1               1         :   image is platform neutral and prefers to be loaded 32-bit when possible
        {COMIMAGE_FLAGS_32BITPREFERRED, "32 bit preferred"},
};

#define NUMBER_COR_HEADER_FLAGS _countof(CorHeaderFlags)

void CCorHeaderDumper::DumpCore(CFileStream* outStream)
{
    DWORD dwCorHeaderSize;
    BOOL bHasCorHeader = IsManagedAssembly(m_peFile, dwCorHeaderSize);

    if (!bHasCorHeader || dwCorHeaderSize < sizeof(DWORD)) // IMAGE_COR20_HEADER::cb
    {
        return;
    }

    PIMAGE_COR20_HEADER pCorHeader;
    PIMAGE_SECTION_HEADER pSectionHeader;
    if (!m_peFile.ImageDirectoryOffset(IMAGE_DIRECTORY_ENTRY_COM_DESCRIPTOR, (LPVOID*)&pCorHeader, &pSectionHeader))
    {
        return;
    }

    dwCorHeaderSize = min(dwCorHeaderSize, pCorHeader->cb);

    bHasCorHeader = STRUCT_HAS_FIELD(IMAGE_COR20_HEADER, MajorRuntimeVersion, dwCorHeaderSize) &&
                    STRUCT_HAS_FIELD(IMAGE_COR20_HEADER, MinorRuntimeVersion, dwCorHeaderSize) &&
                    STRUCT_HAS_FIELD(IMAGE_COR20_HEADER, Flags, dwCorHeaderSize);
    if (!bHasCorHeader)
    {
        return;
    }

    outStream->fprintf("CLR Header:\n");

    int width = 31;

    // For the following see
    // http://blogs.msdn.com/b/joshwil/archive/2004/10/15/243019.aspx
    if (pCorHeader->MajorRuntimeVersion == 2 && pCorHeader->MinorRuntimeVersion == 0)
    {
        outStream->fprintf("  .NET 1.0/1.1 Assembly\n");
    }
    else if (pCorHeader->MajorRuntimeVersion == 2 && pCorHeader->MinorRuntimeVersion == 5)
    {
        outStream->fprintf("  .NET 2.0+ Assembly\n");
    }
    else
    {
        outStream->fprintf("  %-*s%hu.%hu\n", width, "Runtime version:", pCorHeader->MajorRuntimeVersion, pCorHeader->MinorRuntimeVersion);
    }

    outStream->fprintf("  %-*s0x%08X\n", width, "Flags:", pCorHeader->Flags);
    int j;
    DWORD dwFlags = pCorHeader->Flags;
    for (j = 0; j < NUMBER_COR_HEADER_FLAGS; j++)
    {
        if (dwFlags & CorHeaderFlags[j].flag)
        {
            outStream->fprintf("    %s\n", CorHeaderFlags[j].name);
            dwFlags &= ~CorHeaderFlags[j].flag;
        }
    }

    if (dwFlags != 0U)
    {
        // Zustaly nejake flagy, ktere nezname.
        outStream->fprintf("    0x%08X\n", dwFlags);
    }

    return;
}

const _TCHAR* CCorHeaderDumper::GetExceptionMessage()
{
    return _T("CLR header is corrupted.");
}

#if WITH_COR_METADATA_DUMPER

//
// ****************************************************************************
// CCorMetadataDumper
//

#include <Shlwapi.h> // for StrStrI
#pragma comment(lib, "ole32")
#pragma comment(lib, "shlwapi")

#define SAFE_RELEASE(itf) \
    if (itf != NULL) \
    itf->Release()

void CCorMetadataDumper::DumpCore(CFileStream* outStream)
{
    BOOL streamWasEnabled = outStream->GetEnabled();
    if (!IsManagedAssembly(m_peFile))
    {
        return;
    }

    HRESULT hr;
    IMetaDataDispenser* pMetaDataDispenser = NULL;
    IMetaDataAssemblyImport* pMetaDataAssemblyImport = NULL;
    IMetaDataImport* pMetaDataImport = NULL;

    hr = CoCreateInstance(
        CLSID_CorMetaDataDispenser,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_IMetaDataDispenser,
        (LPVOID*)&pMetaDataDispenser);
    if (!SUCCEEDED(hr))
    {
        return;
    }

    __try
    {

        hr = pMetaDataDispenser->OpenScopeOnMemory(
            m_peFile.GetMappingBaseAddress(),
            m_peFile.GetMappingSize(),
            ofReadOnly,
            IID_IMetaDataImport,
            (IUnknown**)&pMetaDataImport);
        if (!SUCCEEDED(hr))
        {
            return;
        }

        hr = pMetaDataDispenser->OpenScopeOnMemory(
            m_peFile.GetMappingBaseAddress(),
            m_peFile.GetMappingSize(),
            ofReadOnly,
            IID_IMetaDataAssemblyImport,
            (IUnknown**)&pMetaDataAssemblyImport);
        if (!SUCCEEDED(hr))
        {
            return;
        }

        mdAssembly thisAssembly;
        hr = pMetaDataAssemblyImport->GetAssemblyFromScope(&thisAssembly);
        if (!SUCCEEDED(hr))
        {
            return;
        }

        outStream->fprintf("CLR Metadata:\n");
        DumpMetadata(outStream, pMetaDataImport, thisAssembly);
        outStream->fprintf("  Referenced Assemblies:\n");
        DumpReferencedAssemblies(outStream, pMetaDataAssemblyImport);
    }
    __finally
    {
        SAFE_RELEASE(pMetaDataImport);
        SAFE_RELEASE(pMetaDataAssemblyImport);
        SAFE_RELEASE(pMetaDataDispenser);
    }
    outStream->SetEnabled(streamWasEnabled);
}

const _TCHAR* CCorMetadataDumper::GetExceptionMessage()
{
    return _T("CLR metadata is corrupted.");
}

void CCorMetadataDumper::DumpMetadata(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdAssembly assembly)
{
    DumpRuntimeVersion(outStream, pMetaDataImport);
    DumpCustomAttributes(outStream, pMetaDataImport, assembly);
}

void CCorMetadataDumper::DumpRuntimeVersion(CFileStream* outStream, IMetaDataImport* pMetaDataImport)
{
    HRESULT hr;
    IMetaDataImport2* pMetaDataImport2;

    hr = pMetaDataImport->QueryInterface(IID_IMetaDataImport2, (void**)&pMetaDataImport2);
    if (SUCCEEDED(hr))
    {
        WCHAR wzVersion[100];
        DWORD cchWritten;
        hr = pMetaDataImport2->GetVersionString(wzVersion, _countof(wzVersion), &cchWritten);
        if (SUCCEEDED(hr))
        {
            outStream->fprintf("  %-*s%ls\n", 31, "Runtime Version:", wzVersion);
        }
        pMetaDataImport2->Release();
    }
}

void CCorMetadataDumper::DumpCustomAttributes(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdToken scope)
{
    HRESULT hr;
    HCORENUM hEnum = NULL;

    for (;;)
    {
        mdCustomAttribute customAttributes[5];
        ULONG cTokens;
        hr = pMetaDataImport->EnumCustomAttributes(&hEnum, scope, NULL, customAttributes, _countof(customAttributes), &cTokens);
        if (hr == S_OK) // no SUCCEEDED, may be S_FALSE
        {
            for (ULONG i = 0; i < cTokens; i++)
            {
                DumpCustomAttribute(outStream, pMetaDataImport, customAttributes[i]);
            }
        }
        else
        {
            if (SUCCEEDED(hr))
            {
                pMetaDataImport->CloseEnum(hEnum);
            }
            break;
        }
    }
}

void CCorMetadataDumper::DumpCustomAttribute(CFileStream* outStream, IMetaDataImport* pMetaDataImport, mdCustomAttribute customAttribute)
{
    HRESULT hr;
    mdToken methodDefOrMemberRef;
    const void* pBlob;
    ULONG cbBlob;

    hr = pMetaDataImport->GetCustomAttributeProps(customAttribute, NULL, &methodDefOrMemberRef, &pBlob, &cbBlob);
    if (SUCCEEDED(hr))
    {
        WCHAR wzTypeName[100];

        if (TypeFromToken(methodDefOrMemberRef) == mdtMethodDef)
        {
            // not implemented yet
            ////_ASSERTE(0);
            hr = E_NOTIMPL;
        }
        else if (TypeFromToken(methodDefOrMemberRef) == mdtMemberRef)
        {
            hr = GetCustomAttributeTypeNameFromMemberRef(pMetaDataImport, methodDefOrMemberRef, wzTypeName, _countof(wzTypeName));
        }
        else
        {
            // should not happen
            _ASSERTE(0);
            hr = HRESULT_FROM_WIN32(ERROR_INTERNAL_ERROR);
        }

        if (SUCCEEDED(hr))
        {
            if (wcscmp(wzTypeName, L"System.Runtime.Versioning.TargetFrameworkAttribute") == 0)
            {
                DumpTargetFrameworkVersion(outStream, (const BYTE*)pBlob, cbBlob);
            }
        }
    }
}

void CCorMetadataDumper::DumpTargetFrameworkVersion(CFileStream* outStream, const BYTE* pBlob, DWORD cbBlob)
{
    // Decode the blob
    // see ECMA-335 Common Language Infrastructure (CLI)
    // II.23.3
    // http://www.ecma-international.org/publications/files/ECMA-ST/ECMA-335.pdf

    if (cbBlob < sizeof(UINT16))
    {
        return;
    }

    UINT16 prolog = *((UINT16*)pBlob);
    if (prolog != 0x0001)
    {
        return;
    }

    pBlob += sizeof(UINT16);
    cbBlob -= sizeof(UINT16);

    BYTE packedLen = *pBlob;
    if (packedLen == 0xFF)
    {
        // String is null.
        return;
    }

    if (packedLen == 0x00)
    {
        // String is empty.
        return;
    }

    ++pBlob;
    --cbBlob;

    if (packedLen > cbBlob)
    {
        return;
    }

    const char* pUtf8String = (const char*)pBlob;

    // Check if the string contains only ASCII chars.
    for (int i = 0; i < packedLen; i++)
    {
        if (pUtf8String[i] < 0x20 || pUtf8String[i] > 0x80)
        {
            return;
        }
    }

    outStream->fprintf("  %-*s%.*s\n", 31, "Target Framework:", packedLen, pUtf8String);
}

HRESULT CCorMetadataDumper::GetCustomAttributeTypeNameFromMemberRef(IMetaDataImport* pMetaDataImport, mdMemberRef memberRef, PWSTR wzTypeName, DWORD cchTypeName)
{
    HRESULT hr;
    mdToken type;

    hr = pMetaDataImport->GetMemberRefProps(memberRef, &type, NULL, NULL, NULL, NULL, NULL);
    if (SUCCEEDED(hr))
    {
        if (TypeFromToken(type) == mdtTypeRef)
        {
            hr = pMetaDataImport->GetTypeRefProps(type, NULL, wzTypeName, cchTypeName, NULL);
        }
        else
        {
            hr = E_NOTIMPL;
        }
    }

    return hr;
}

void CCorMetadataDumper::DumpReferencedAssemblies(CFileStream* outStream, IMetaDataAssemblyImport* pMetaDataAssemblyImport)
{
    HRESULT hr;
    HCORENUM hEnum = NULL;
    CSortedStringListW list(SortAssemblyNameCallback);

    for (;;)
    {
        mdAssemblyRef assemblyRefs[5];
        ULONG cTokens;
        hr = pMetaDataAssemblyImport->EnumAssemblyRefs(&hEnum, assemblyRefs, _countof(assemblyRefs), &cTokens);
        if (hr == S_OK) // no SUCCEEDED, may be S_FALSE
        {
            for (ULONG i = 0; i < cTokens; i++)
            {
                DumpAssemblyReference(list, pMetaDataAssemblyImport, assemblyRefs[i]);
            }
        }
        else
        {
            if (SUCCEEDED(hr))
            {
                pMetaDataAssemblyImport->CloseEnum(hEnum);
            }
            break;
        }
    }

    CStringList::HITEM hItem = NULL;
    while (list.Next(hItem))
    {
        PCWSTR pszName = list.GetBuffer(hItem);
        outStream->fprintf("    %ls\n", pszName);
    }
}

typedef HRESULT(STDAPICALLTYPE* PFN_CreateAssemblyNameObject)(IAssemblyName**, LPCWSTR, DWORD, LPVOID);
typedef HRESULT(STDAPICALLTYPE* PFN_LoadLibraryShim)(LPCWSTR, LPCWSTR, LPVOID, HMODULE*);

static PFN_CreateAssemblyNameObject g_pfnCreateAssemblyNameObject;

static HRESULT STDAPICALLTYPE CreateAssemblyNameObjectNotImpl(IAssemblyName**, LPCWSTR, DWORD, LPVOID)
{
    return E_NOTIMPL;
}

static HMODULE LoadFusionLibrary()
{
    HMODULE hFusion = NULL;
    HMODULE hMsCorEE;

    hMsCorEE = LoadLibraryW(L"mscoree.dll");
    if (hMsCorEE != NULL)
    {
        PFN_LoadLibraryShim pfnLoadLibraryShim = (PFN_LoadLibraryShim)GetProcAddress(hMsCorEE, "LoadLibraryShim");
        if (pfnLoadLibraryShim != NULL)
        {
            HRESULT hr;
            hr = pfnLoadLibraryShim(L"fusion.dll", NULL, NULL, &hFusion);
            if (!SUCCEEDED(hr))
            {
                hFusion = NULL;
            }
        }
        FreeLibrary(hMsCorEE);
    }

    return hFusion;
}

static HRESULT CreateAssemblyNameObjectHelper(IAssemblyName** ppAssemblyName, LPCWSTR pwzAssemblyName, DWORD dwFlags, LPVOID lpReserved = NULL)
{
    if (g_pfnCreateAssemblyNameObject == NULL)
    {
        HMODULE hMod = LoadFusionLibrary();
        if (hMod != NULL)
        {
            PFN_CreateAssemblyNameObject pfn = (PFN_CreateAssemblyNameObject)GetProcAddress(hMod, "CreateAssemblyNameObject");
            if (pfn == NULL)
            {
                pfn = CreateAssemblyNameObjectNotImpl;
            }

            if (InterlockedCompareExchangePointer((volatile PVOID*)&g_pfnCreateAssemblyNameObject, pfn, NULL) != NULL)
            {
                FreeLibrary(hMod);
            }
        }
        else
        {
            InterlockedExchangePointer((volatile PVOID*)&g_pfnCreateAssemblyNameObject, CreateAssemblyNameObjectNotImpl);
        }
    }

    return g_pfnCreateAssemblyNameObject(ppAssemblyName, pwzAssemblyName, dwFlags, lpReserved);
}

void CCorMetadataDumper::DumpAssemblyReference(CStringList& list, IMetaDataAssemblyImport* pMetaDataAssemblyImport, mdAssemblyRef assemblyRef)
{
    HRESULT hr;
    IAssemblyName* pAssemblyName;
    WCHAR wzName[MAX_PATH];
    ULONG cchName;
    ASSEMBLYMETADATA metaData = {
        0,
    };
    WCHAR wzLocale[100];
    DWORD adwProcessor[10];
    OSINFO aOSInfo[10];
    DWORD dwCorAssemblyFlags;
    const void* pPublicKey;
    ULONG cbPublicKey;

    metaData.szLocale = wzLocale;
    metaData.cbLocale = _countof(wzLocale);
    metaData.rProcessor = adwProcessor;
    metaData.ulProcessor = _countof(adwProcessor);
    metaData.rOS = aOSInfo;
    metaData.ulOS = _countof(aOSInfo);

    hr = pMetaDataAssemblyImport->GetAssemblyRefProps(
        assemblyRef,
        &pPublicKey,
        &cbPublicKey,
        wzName,
        _countof(wzName),
        &cchName,
        &metaData,
        NULL,
        NULL,
        &dwCorAssemblyFlags);

    hr = CreateAssemblyNameObjectHelper(&pAssemblyName, wzName, CANOF_PARSE_DISPLAY_NAME);
    if (SUCCEEDED(hr))
    {
        AsmNameSetMetaData(pAssemblyName, &metaData);
        AsmNameSetPublicKey(pAssemblyName, pPublicKey, cbPublicKey, IsAfPublicKey(dwCorAssemblyFlags));
        AsmNameSetFlags(pAssemblyName, dwCorAssemblyFlags);
        hr = pAssemblyName->SetProperty(ASM_NAME_PROCESSOR_ID_ARRAY, metaData.rProcessor, metaData.ulProcessor);
        DumpAssemblyFullyQualifiedName(list, pAssemblyName);
        pAssemblyName->Release();
    }
    else
    {
        DumpAssemblyFullyQualifiedName(list, wzName, &metaData);
    }
}

void CCorMetadataDumper::DumpAssemblyFullyQualifiedName(CStringList& list, LPCWSTR pwzName, const ASSEMBLYMETADATA* pMetaData)
{
    CStringList::HITEM hItem = list.Alloc(MAX_ASSEMBLY_NAME);
    DWORD cchMax;
    CStringList::XCHAR* buffer = list.GetBuffer(hItem, cchMax);
    StringCchPrintfW(buffer, cchMax, L"%ls, Version=%hu.%hu.%hu.%hu",
                     pwzName,
                     pMetaData->usMajorVersion,
                     pMetaData->usMinorVersion,
                     pMetaData->usBuildNumber,
                     pMetaData->usRevisionNumber);
    list.Insert(hItem);
}

void CCorMetadataDumper::DumpAssemblyFullyQualifiedName(CStringList& list, IAssemblyName* pAssemblyName)
{
    HRESULT hr;
    CStringList::HITEM hItem = list.Alloc(MAX_ASSEMBLY_NAME);
    DWORD cchMax;
    CStringList::XCHAR* buffer = list.GetBuffer(hItem, cchMax);

    hr = pAssemblyName->GetDisplayName(buffer, &cchMax, ASM_DISPLAYF_FULL);
    if (SUCCEEDED(hr))
    {
        list.Insert(hItem);
    }
    else
    {
        list.Free(hItem);
    }
}

void CCorMetadataDumper::AsmNameSetMetaData(IAssemblyName* pAssemblyName, const ASSEMBLYMETADATA* pMetaData)
{
    HRESULT hr;

    hr = pAssemblyName->SetProperty(ASM_NAME_MAJOR_VERSION, (LPVOID)&pMetaData->usMajorVersion, sizeof(pMetaData->usMajorVersion));
    hr = pAssemblyName->SetProperty(ASM_NAME_MINOR_VERSION, (LPVOID)&pMetaData->usMinorVersion, sizeof(pMetaData->usMinorVersion));
    hr = pAssemblyName->SetProperty(ASM_NAME_BUILD_NUMBER, (LPVOID)&pMetaData->usBuildNumber, sizeof(pMetaData->usBuildNumber));
    hr = pAssemblyName->SetProperty(ASM_NAME_REVISION_NUMBER, (LPVOID)&pMetaData->usRevisionNumber, sizeof(pMetaData->usRevisionNumber));

    WCHAR wzCulture[20];
    if (pMetaData->cbLocale == 0)
    {
        wzCulture[0] = L'\0';
    }
    else
    {
        LPCWSTR pwzDelim = wcschr(pMetaData->szLocale, L';');
        if (pwzDelim != NULL)
        {
            StringCchCopyNW(wzCulture, _countof(wzCulture), pMetaData->szLocale, pwzDelim - pMetaData->szLocale);
        }
        else
        {
            StringCbCopyNW(wzCulture, sizeof(wzCulture), pMetaData->szLocale, pMetaData->cbLocale);
        }
    }

    hr = pAssemblyName->SetProperty(ASM_NAME_CULTURE, wzCulture, (lstrlenW(wzCulture) + 1) * sizeof(TCHAR));
}

void CCorMetadataDumper::AsmNameSetPublicKey(IAssemblyName* pAssemblyName, const void* pPublicKey, DWORD cbPublicKey, BOOL bIsFullKey)
{
    HRESULT hr;

    if (bIsFullKey)
    {
        hr = pAssemblyName->SetProperty((pPublicKey && cbPublicKey) ? ASM_NAME_PUBLIC_KEY : ASM_NAME_NULL_PUBLIC_KEY, (LPVOID)pPublicKey, cbPublicKey);
    }
    else
    {
        hr = pAssemblyName->SetProperty((pPublicKey && cbPublicKey) ? ASM_NAME_PUBLIC_KEY_TOKEN : ASM_NAME_NULL_PUBLIC_KEY_TOKEN, (LPVOID)pPublicKey, cbPublicKey);
    }
}

void CCorMetadataDumper::AsmNameSetFlags(IAssemblyName* pAssemblyName, DWORD dwCorAssemblyFlags)
{
    HRESULT hr;

    BOOL bRetargetable = !!IsAfRetargetable(dwCorAssemblyFlags);
    hr = pAssemblyName->SetProperty(ASM_NAME_RETARGET, &bRetargetable, sizeof(bRetargetable));
}

int CCorMetadataDumper::SortAssemblyNameCallback(const CStringList::XCHAR* s1, const CStringList::XCHAR* s2)
{
    int boost1 = GetAssemblyNameSortBoost(s1);
    int boost2 = GetAssemblyNameSortBoost(s2);

    int diff = boost2 - boost1;
    if (diff == 0)
    {
        diff = CStringListTraits::CompareNoCase(s1, s2);
    }

    return diff;
}

int CCorMetadataDumper::GetAssemblyNameSortBoost(const CStringList::XCHAR* pszAssemblyName)
{
    int boost = 0;

    if (StrStrIW(pszAssemblyName, L"mscorlib") == pszAssemblyName)
    {
        // mscorlib vzdy na prvnim miste...
        boost = 100;
    }
    else if (StrStrIW(pszAssemblyName, L"System") == pszAssemblyName)
    {
        // ...pak systemove assemblies...
        boost = 50;
    }
    else
    {
        // ...a vsechny dalsi podle abecedy nakonec.
    }

    return boost;
}

#endif /* WITH_COR_METADATA_DUMPER */
