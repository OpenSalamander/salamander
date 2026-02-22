// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "zip.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "pack.h"
#include "fileswnd.h"

//
// ****************************************************************************
// Types
// ****************************************************************************
//

// Data structure for the parsing function
struct SPackExpData
{
    const char* ArcName;
    const char* SrcDir;
    const char* TgtDir;
    const char* LstName;
    const char* ExtName;
    char Buffer[MAX_PATH];
    // following variables exist because we cannot obtain the DOS name of a non-existent file
    // we handle it by returning a substitute DOS name which will later (after creating the archive) be renamed to the desired long name
    // once the archive is created
    BOOL ArcNameFilePossible; // TRUE until the substitute DOS name is used (we must use one name everywhere)
    BOOL DOSTmpFilePossible;  // TRUE while ArcName can be replaced with the substitute DOS name
    char* DOSTmpFile;         // substitute name for ArcName (non-NULL only if DOSTmpFilePossible is TRUE)
};

class CExecuteWindow : public CWindow
{
protected:
    char* Text;
    HWND HParent;

public:
    CExecuteWindow(HWND hParent, int textResID, CObjectOrigin origin = ooAllocated);
    ~CExecuteWindow();

    HWND Create();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//
// ****************************************************************************
// Constants and global variables
// ****************************************************************************
//

// Timeout for opening the packer console in milliseconds
// (if less than one, it is not opened at all)
LONG PackWinTimeout = 15000;

// configuration item names in the registry
//
// predefined packers associations
const char* SALAMANDER_PPA_EXTENSIONS = "Extension List";
const char* SALAMANDER_PPA_PINDEX = "Packer Index";
const char* SALAMANDER_PPA_UINDEX = "Unpacker Index";
const char* SALAMANDER_PPA_USEPACKER = "Packer Supported";
// predefined packers configuration
const char* SALAMANDER_PPC_TITLE = "Packer Title";
const char* SALAMANDER_PPC_PACKEXE = "Packer Executable";
const char* SALAMANDER_PPC_EXESAME = "Use Packer Executable To Unpack";
const char* SALAMANDER_PPC_UID = "Packer UID";
const char* SALAMANDER_PPC_UNPACKEXE = "Unpacker Executable";

// main packer configuration
CPackerFormatConfig PackerFormatConfig /*(FALSE)*/;
CArchiverConfig ArchiverConfig /*(FALSE)*/;

//
// Return code tables for individual supported packers
// error code 0 always means success
//

// JAR
const TPackErrorTable JARErrors =
    {
        {1, IDS_PACKRET_WARNING},
        {2, IDS_PACKRET_FATAL},
        {3, IDS_PACKRET_CRC},
        {5, IDS_PACKRET_DISK},
        {6, IDS_PACKRET_FOPEN},
        {7, IDS_PACKRET_PARAMS},
        {8, IDS_PACKRET_MEMORY},
        {9, IDS_PACKRET_NOTARC},
        {10, IDS_PACKRET_INTERN},
        {11, IDS_PACKRET_BREAK},
        {-1, -1}};
// RAR
const TPackErrorTable RARErrors =
    {
        {1, IDS_PACKRET_WARNING},
        {2, IDS_PACKRET_FATAL},
        {3, IDS_PACKRET_CRC},
        {4, IDS_PACKRET_SECURITY},
        {5, IDS_PACKRET_DISK},
        {6, IDS_PACKRET_FOPEN},
        {7, IDS_PACKRET_PARAMS},
        {8, IDS_PACKRET_MEMORY},
        {255, IDS_PACKRET_BREAK},
        {-1, -1}};
// ARJ
const TPackErrorTable ARJErrors =
    {
        {1, IDS_PACKRET_WARNING},
        {2, IDS_PACKRET_FATAL},
        {3, IDS_PACKRET_CRC},
        {4, IDS_PACKRET_SECURITY},
        {5, IDS_PACKRET_DISK},
        {6, IDS_PACKRET_FOPEN},
        {7, IDS_PACKRET_PARAMS},
        {8, IDS_PACKRET_MEMORY},
        {9, IDS_PACKRET_NOTARC},
        {10, IDS_PACKRET_XMSMEM},
        {11, IDS_PACKRET_BREAK},
        {12, IDS_PACKRET_CHAPTERS},
        {-1, -1}};
// LHA
const TPackErrorTable LHAErrors =
    {
        {1, IDS_PACKRET_EXTRACT_LHA},
        {2, IDS_PACKRET_FATAL},
        {3, IDS_PACKRET_TEMP},
        {-1, -1}};
// UC2
const TPackErrorTable UC2Errors =
    {
        {5, IDS_PACKRET_INTERN},
        {7, IDS_PACKRET_SECURITY},
        {10, IDS_PACKRET_FOPEN},
        {15, IDS_PACKRET_WARNING},
        {20, IDS_PACKRET_FOPEN},
        {25, IDS_PACKRET_SKIPPED},
        {30, IDS_PACKRET_SKIPPED},
        {35, IDS_PACKRET_SKIPPED},
        {50, IDS_PACKRET_INTERN},
        {55, IDS_PACKRET_DISK},
        {60, IDS_PACKRET_DISK},
        {65, IDS_PACKRET_FATAL},
        {70, IDS_PACKRET_DISK},
        {75, IDS_PACKRET_WARNING},
        {80, IDS_PACKRET_SKIPPED},
        {85, IDS_PACKRET_DISK},
        {90, IDS_PACKRET_DAMAGED},
        {95, IDS_PACKRET_VIRUS},
        {100, IDS_PACKRET_BREAK},
        {105, IDS_PACKRET_INTERN},
        {110, IDS_PACKRET_PARAMS},
        {115, IDS_PACKRET_PARAMS},
        {120, IDS_PACKRET_NOTARC},
        {123, IDS_PACKRET_PARAMS},
        {125, IDS_PACKRET_SECURITY},
        {130, IDS_PACKRET_NOTARC},
        {135, IDS_PACKRET_FOPEN},
        {140, IDS_PACKRET_PARAMS},
        {145, IDS_PACKRET_EXTRACT},
        {150, IDS_PACKRET_FOPEN},
        {155, IDS_PACKRET_WARNING},
        {157, IDS_PACKRET_WARNING},
        {160, IDS_PACKRET_MEMORY},
        {163, IDS_PACKRET_MEMORY},
        {165, IDS_PACKRET_MEMORY},
        {170, IDS_PACKRET_FATAL},
        {175, IDS_PACKRET_TEMP},
        {180, IDS_PACKRET_DISK},
        {185, IDS_PACKRET_FOPEN},
        {190, IDS_PACKRET_VIRUS},
        {195, IDS_PACKRET_DAMAGED},
        {200, IDS_PACKRET_DAMAGED},
        {205, IDS_PACKRET_FATAL},
        {210, IDS_PACKRET_FATAL},
        {250, IDS_PACKRET_FOPEN},
        {255, IDS_PACKRET_INTERN},
        {-1, -1}};
// PKZIP 2.04g
const TPackErrorTable ZIP204Errors =
    {
        {1, IDS_PACKRET_FOPEN},
        {2, IDS_PACKRET_CRC},
        {3, IDS_PACKRET_CRC},
        {4, IDS_PACKRET_MEMORY},
        {5, IDS_PACKRET_MEMORY},
        {6, IDS_PACKRET_MEMORY},
        {7, IDS_PACKRET_MEMORY},
        {8, IDS_PACKRET_MEMORY},
        {9, IDS_PACKRET_MEMORY},
        {10, IDS_PACKRET_MEMORY},
        {11, IDS_PACKRET_MEMORY},
        {12, IDS_PACKRET_PARAMS},
        {13, IDS_PACKRET_FOPEN},
        {14, IDS_PACKRET_DISK},
        {15, IDS_PACKRET_DISK},
        {16, IDS_PACKRET_PARAMS},
        {17, IDS_PACKRET_PARAMS},
        {18, IDS_PACKRET_FOPEN},
        {255, IDS_PACKRET_BREAK},
        {-1, -1}};
// PKUNZIP 2.04g
const TPackErrorTable UNZIP204Errors =
    {
        {1, IDS_PACKRET_WARNING},
        {2, IDS_PACKRET_CRC},
        {3, IDS_PACKRET_CRC},
        {4, IDS_PACKRET_MEMORY},
        {5, IDS_PACKRET_MEMORY},
        {6, IDS_PACKRET_MEMORY},
        {7, IDS_PACKRET_MEMORY},
        {8, IDS_PACKRET_MEMORY},
        {9, IDS_PACKRET_FOPEN},
        {10, IDS_PACKRET_PARAMS},
        {11, IDS_PACKRET_FOPEN},
        {50, IDS_PACKRET_DISK},
        {51, IDS_PACKRET_CRC},
        {255, IDS_PACKRET_BREAK},
        {-1, -1}};
// ACE
const TPackErrorTable ACEErrors =
    {
        {1, IDS_PACKRET_MEMORY},
        {2, IDS_PACKRET_FOPEN},
        {3, IDS_PACKRET_FOPEN},
        {4, IDS_PACKRET_DISK},
        {5, IDS_PACKRET_FOPEN},
        {6, IDS_PACKRET_FOPEN},
        {7, IDS_PACKRET_DISK},
        {8, IDS_PACKRET_PARAMS},
        {9, IDS_PACKRET_CRC},
        {10, IDS_PACKRET_FATAL},
        {11, IDS_PACKRET_FOPEN},
        {255, IDS_PACKRET_BREAK2},
        {-1, -1}};

// Variables distinguished in the command line and the current directory when
// launching an external program
const char* PACK_ARC_PATH = "ArchivePath";
const char* PACK_ARC_FILE = "ArchiveFileName";
const char* PACK_ARC_NAME = "ArchiveFullName";
const char* PACK_SRC_PATH = "SourcePath";
const char* PACK_TGT_PATH = "TargetPath";
const char* PACK_LST_NAME = "ListFullName";
const char* PACK_EXT_NAME = "ExtractFullName";
const char* PACK_ARC_DOSFILE = "ArchiveDOSFileName";
const char* PACK_ARC_DOSNAME = "ArchiveDOSFullName";
const char* PACK_TGT_DOSPATH = "TargetDOSPath";
const char* PACK_LST_DOSNAME = "ListDOSFullName";

const char* PACK_EXE_JAR32 = "Jar32bitExecutable";
const char* PACK_EXE_JAR16 = "Jar16bitExecutable";
const char* PACK_EXE_RAR32 = "Rar32bitExecutable";
const char* PACK_EXE_RAR16 = "Rar16bitExecutable";
const char* PACK_EXE_ARJ32 = "Arj32bitExecutable";
const char* PACK_EXE_ARJ16 = "Arj16bitExecutable";
const char* PACK_EXE_ACE32 = "Ace32bitExecutable";
const char* PACK_EXE_ACE16 = "Ace16bitExecutable";
const char* PACK_EXE_LHA16 = "Lha16bitExecutable";
const char* PACK_EXE_UC216 = "UC216bitExecutable";
const char* PACK_EXE_ZIP32 = "Zip32bitExecutable";
const char* PACK_EXE_ZIP16 = "Zip16bitExecutable";
const char* PACK_EXE_UZP16 = "Unzip16bitExecutable";

// Menu in configuration

/* used by the export_mnu.py script that generates salmenu.mnu for the
   Translator; keep synchronized with the array below...
MENU_TEMPLATE_ITEM CmdCustomPackers[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PACK_EXE_JAR32
  {MNTT_IT, IDS_PACK_EXE_JAR16
  {MNTT_IT, IDS_PACK_EXE_RAR32
  {MNTT_IT, IDS_PACK_EXE_RAR16
  {MNTT_IT, IDS_PACK_EXE_ARJ32
  {MNTT_IT, IDS_PACK_EXE_ARJ16
  {MNTT_IT, IDS_PACK_EXE_ACE32
  {MNTT_IT, IDS_PACK_EXE_ACE16
  {MNTT_IT, IDS_PACK_EXE_LHA16
  {MNTT_IT, IDS_PACK_EXE_UC216
  {MNTT_IT, IDS_PACK_EXE_ZIP32
  {MNTT_IT, IDS_PACK_EXE_ZIP16
  {MNTT_IT, IDS_PACK_EXE_UZP16
  {MNTT_IT, IDS_PACK_EXE_BROWSE
  {MNTT_PE, 0
};
*/

// Command
CExecuteItem CmdCustomPackers[] =
    {
        {PACK_EXE_JAR32, IDS_PACK_EXE_JAR32, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_JAR16, IDS_PACK_EXE_JAR16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_RAR32, IDS_PACK_EXE_RAR32, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_RAR16, IDS_PACK_EXE_RAR16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ARJ32, IDS_PACK_EXE_ARJ32, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ARJ16, IDS_PACK_EXE_ARJ16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ACE32, IDS_PACK_EXE_ACE32, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ACE16, IDS_PACK_EXE_ACE16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_LHA16, IDS_PACK_EXE_LHA16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_UC216, IDS_PACK_EXE_UC216, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ZIP32, IDS_PACK_EXE_ZIP32, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_ZIP16, IDS_PACK_EXE_ZIP16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {PACK_EXE_UZP16, IDS_PACK_EXE_UZP16, EIF_VARIABLE | EIF_REPLACE_ALL},
        {EXECUTE_SEPARATOR, 0, 0},
        {EXECUTE_BROWSE, IDS_PACK_EXE_BROWSE, EIF_REPLACE_ALL},
        {EXECUTE_TERMINATOR, 0, 0},
};

/* used by the export_mnu.py script that generates salmenu.mnu for the
   Translator; keep synchronized with the array below...
MENU_TEMPLATE_ITEM ArgsCustomPackers[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_PACK_ARC_NAME
  {MNTT_IT, IDS_PACK_ARC_FILE
  {MNTT_IT, IDS_PACK_ARC_PATH
  {MNTT_IT, IDS_PACK_LST_NAME
  {MNTT_IT, IDS_PACK_ARC_DOSNAME
  {MNTT_IT, IDS_PACK_ARC_DOSFILE
  {MNTT_IT, IDS_PACK_LST_DOSNAME
  {MNTT_PE, 0
};
*/

// Arguments
// Custom packers/unpackers
CExecuteItem ArgsCustomPackers[] =
    {
        {PACK_ARC_NAME, IDS_PACK_ARC_NAME, EIF_VARIABLE},
        {PACK_ARC_FILE, IDS_PACK_ARC_FILE, EIF_VARIABLE},
        {PACK_ARC_PATH, IDS_PACK_ARC_PATH, EIF_VARIABLE},
        {PACK_LST_NAME, IDS_PACK_LST_NAME, EIF_VARIABLE},
        {EXECUTE_SEPARATOR, 0, 0},
        {PACK_ARC_DOSNAME, IDS_PACK_ARC_DOSNAME, EIF_VARIABLE},
        {PACK_ARC_DOSFILE, IDS_PACK_ARC_DOSFILE, EIF_VARIABLE},
        {PACK_LST_DOSNAME, IDS_PACK_LST_DOSNAME, EIF_VARIABLE},
        {EXECUTE_TERMINATOR, 0, 0},
};

//
// ****************************************************************************
// Functions
// ****************************************************************************
//

// Function for initializing the spawn executable name with full path
BOOL InitSpawnName(HWND parent)
{
    CALL_STACK_MESSAGE1("InitSpawnName()");
    if (!SpawnExeInitialised)
    {
        if (GetModuleFileName(NULL, SpawnExe, MAX_PATH) == 0)
        {
            char buffer[1000];
            strcpy(buffer, "GetModuleFileName: ");
            strcat(buffer, GetErrorText(GetLastError()));
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
        }
        char* ptr = strrchr(SpawnExe, '\\');
        if (ptr == NULL)
            ptr = SpawnExe;
        else
            ptr++;
        strcpy(ptr, "utils\\");
        strcat(ptr, SPAWN_EXE_NAME);
        SpawnExeInitialised = TRUE;
    }
    return TRUE;
}

//
// ****************************************************************************
// Implementation of the configuration object - association of extensions and packers
//

CPackerFormatConfig::CPackerFormatConfig(/*BOOL disableDefaultValues*/)
    : Formats(10, 5)
{
    /*
  if (!disableDefaultValues)
  {
    AddDefault(0);
    if (!BuildArray())
    {
      TRACE_E("Unable to create data for archive detection");
    }
  }
*/
}

void CPackerFormatConfig::InitializeDefaultValues()
{
    AddDefault(0);
    if (!BuildArray())
    {
        TRACE_E("Unable to create data for archive detection");
    }
}

void CPackerFormatConfig::AddDefault(int SalamVersion)
{
    int index;

    switch (SalamVersion)
    {
    case 0: // default
    case 1: // version 1.52 had no packers
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "zip", TRUE, -1, -1, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "j", TRUE, 0, 0, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "rar;r##", TRUE, 1, 1, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "arj;a##", TRUE, 9, 9, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "lzh", TRUE, 3, 3, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "uc2", TRUE, 4, 4, TRUE);
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "ace", TRUE, 10, 10, TRUE);

    case 2: // what was added after beta1
        // workaround to add the PK3 extension to ZIP
        for (index = 0; index < Formats.Count; index++)
            if (!stricmp(Formats[index]->Ext, "zip"))
            {
                char* ptr = (char*)malloc(strlen(Formats[index]->Ext) + 5);
                if (ptr != NULL)
                {
                    strcpy(ptr, Formats[index]->Ext);
                    strcat(ptr, ";pk3");
                    free(Formats[index]->Ext);
                    Formats[index]->Ext = ptr;
                }
                break;
            }
        // and new extensions
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "pak", TRUE, -3, -3, TRUE);

    case 3: // what was added after beta2
    case 4: // beta3 but with old configuration (contains $(SpawnName))
        // workaround to add the C## extension to ACE
        for (index = 0; index < Formats.Count; index++)
            if (!stricmp(Formats[index]->Ext, "ace"))
            {
                char* ptr = (char*)malloc(strlen(Formats[index]->Ext) + 5);
                if (ptr != NULL)
                {
                    strcpy(ptr, Formats[index]->Ext);
                    strcat(ptr, ";c##");
                    free(Formats[index]->Ext);
                    Formats[index]->Ext = ptr;
                }
                break;
            }

    case 5: // what's new in beta4?
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "tgz;tbz;taz;tar;gz;bz;bz2;z;rpm;cpio", FALSE, 0, -2, TRUE);
        // workaround to add the JAR extension to ZIP
        for (index = 0; index < Formats.Count; index++)
            if (!stricmp(Formats[index]->Ext, "zip;pk3"))
            {
                char* ptr = (char*)malloc(strlen(Formats[index]->Ext) + 5);
                if (ptr != NULL)
                {
                    strcpy(ptr, Formats[index]->Ext);
                    strcat(ptr, ";jar");
                    free(Formats[index]->Ext);
                    Formats[index]->Ext = ptr;
                }
                break;
            }
    }
}

BOOL CPackerFormatConfig::BuildArray(int* line, int* column)
{
    BOOL ret = TRUE;
    char buffer[501];
    buffer[500] = '\0';

    // clear the existing array
    int i;
    for (i = 0; i < 256; i++)
        Extensions[i].DestroyMembers();
    // and fill it again
    CExtItem item;
    for (i = 0; i < Formats.Count; i++)
    {
        strncpy(buffer, GetExt(i), 500);
        char* ptr = strtok(buffer, ";");
        while (ptr != NULL)
        {
            int len = (int)strlen(ptr);
            char* ext = (char*)malloc(len + 1);
            int last = len - 1;
            len -= 2;
            int idx = 0;
            while (len >= 0)
            {
                ext[idx++] = LowerCase[ptr[len--]];
            }
            ext[idx++] = '.';
            ext[idx] = '\0';
            item.Set(ext, i);
            if (!Extensions[LowerCase[ptr[last]]].SIns(item))
            {
                free(ext);
                if (ret)
                {
                    if (line != NULL)
                        *line = i;
                    if (column != NULL)
                        *column = (int)(ptr - buffer);
                    ret = FALSE;
                }
            }
            ptr = strtok(NULL, ";");
        }
    }
    item.Set(NULL, 0);
    return ret;
}

int CPackerFormatConfig::AddFormat()
{
    CPackerFormatConfigData* data = new CPackerFormatConfigData;
    if (data == NULL)
        return -1;
    int index = Formats.Add(data);
    if (!Formats.IsGood())
    {
        Formats.ResetState();
        return -1;
    }
    return index;
}

BOOL CPackerFormatConfig::SetFormat(int index, const char* ext, BOOL usePacker,
                                    const int packerIndex, const int unpackerIndex, BOOL old)

{
    CPackerFormatConfigData* data = Formats[index];
    data->Destroy();

    data->Ext = DupStr(ext);
    data->UsePacker = usePacker;
    if (usePacker)
        data->PackerIndex = packerIndex;
    else
        data->PackerIndex = -1;
    data->UnpackerIndex = unpackerIndex;
    data->OldType = old;

    if (data->IsValid())
        return TRUE;
    else
    {
        TRACE_E("invalid data");
        Formats.Delete(index);
        return FALSE;
    }
}

// returns the format table index + 1 or FALSE (0) when it's not an archive
int CPackerFormatConfig::PackIsArchive(const char* archiveName, int archiveNameLen)
{
    // j.r. I disabled the macro because PackIsArchive is heavily called from CFilesWindow::CommonRefresh()
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("PackIsArchive(%s)", archiveName);
    if (archiveName[0] == 0)
        return 0; // not found
    int idx;
    if (archiveNameLen == -1)
        idx = (int)strlen(archiveName) - 1;
    else
        idx = archiveNameLen - 1;
    CStringArray* array = &Extensions[LowerCase[archiveName[idx]]];
    // take the last character as it is
    int i;
    for (i = 0; i < array->Count; i++)
    {
        char* ptr = array->At(i).GetExt();
        const char* name = &archiveName[idx - 1];
        while (*ptr != '\0' && name >= archiveName &&
               ((*ptr == LowerCase[*name] && *ptr != '#') ||
                (*ptr == '#' && *name >= '0' && *name <= '9')))
        {
            ptr++;
            name--;
        }
        if (*ptr == '\0')
            return array->At(i).GetIndex() + 1;
    }
    // if the last character is a digit
    if (archiveName[idx] >= '0' && archiveName[idx] <= '9')
    {
        array = &Extensions['#'];
        for (i = 0; i < array->Count; i++)
        {
            char* ptr = array->At(i).GetExt();
            const char* name = &archiveName[idx - 1];
            while (*ptr != '\0' && name >= archiveName &&
                   ((*ptr == LowerCase[*name] && *ptr != '#') ||
                    (*ptr == '#' && *name >= '0' && *name <= '9')))
            {
                ptr++;
                name--;
            }
            if (*ptr == '\0')
                return array->At(i).GetIndex() + 1;
        }
    }
    // not found
    return 0;
}

BOOL CPackerFormatConfig::Load(CPackerFormatConfig& src)
{
    DeleteAllFormats();
    int i;
    for (i = 0; i < src.GetFormatsCount(); i++)
    {
        int index = AddFormat();
        if (index == -1)
            return FALSE;
        if (!SetFormat(index, src.GetExt(i), src.GetUsePacker(i),
                       src.GetPackerIndex(i), src.GetUnpackerIndex(i), src.GetOldType(i)))
            return FALSE;
    }
    return TRUE;
}

BOOL CPackerFormatConfig::Save(int index, HKEY hKey)
{
    DWORD d;
    BOOL ret = TRUE;
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPA_EXTENSIONS, REG_SZ, GetExt(index), -1);
    d = GetUsePacker(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPA_USEPACKER, REG_DWORD, &d, sizeof(d));
    d = GetPackerIndex(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPA_PINDEX, REG_DWORD, &d, sizeof(d));
    d = GetUnpackerIndex(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPA_UINDEX, REG_DWORD, &d, sizeof(d));
    return ret;
}

BOOL CPackerFormatConfig::Load(HKEY hKey)
{
    int max = MAX_PATH + 2;

    char ext[MAX_PATH + 2];
    ext[0] = 0;
    DWORD packerIndex, unpackerIndex, usePacker;

    BOOL ret = TRUE;
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPA_EXTENSIONS, REG_SZ, ext, max);
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPA_USEPACKER, REG_DWORD, &usePacker, sizeof(DWORD));
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPA_PINDEX, REG_DWORD, &packerIndex, sizeof(DWORD));
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPA_UINDEX, REG_DWORD, &unpackerIndex, sizeof(DWORD));

    if (ret)
    {
        int index;
        if ((index = AddFormat()) == -1)
            return FALSE;
        if (Configuration.ConfigVersion < 44) // convert extensions to lowercase
        {
            char extAux[MAX_PATH + 2];
            lstrcpyn(extAux, ext, MAX_PATH + 2);
            StrICpy(ext, extAux);
        }
        ret &= SetFormat(index, ext, usePacker, packerIndex, unpackerIndex,
                         Configuration.ConfigVersion < 6);
    }

    return ret;
}

/*
BOOL
CPackerFormatConfig::SwapFormats(int index1, int index2)
{
  BYTE buff[sizeof(CPackerFormatConfigData)];
  memcpy(buff, Formats[index1], sizeof(CPackerFormatConfigData));
  memcpy(Formats[index1], Formats[index2], sizeof(CPackerFormatConfigData));
  memcpy(Formats[index2], buff, sizeof(CPackerFormatConfigData));
  return TRUE;
}
*/

BOOL CPackerFormatConfig::MoveFormat(int srcIndex, int dstIndex)
{
    BYTE buff[sizeof(CPackerFormatConfigData)];
    memcpy(buff, Formats[srcIndex], sizeof(CPackerFormatConfigData));
    if (srcIndex < dstIndex)
    {
        int i;
        for (i = srcIndex; i < dstIndex; i++)
            memcpy(Formats[i], Formats[i + 1], sizeof(CPackerFormatConfigData));
    }
    else
    {
        int i;
        for (i = srcIndex; i > dstIndex; i--)
            memcpy(Formats[i], Formats[i - 1], sizeof(CPackerFormatConfigData));
    }
    memcpy(Formats[dstIndex], buff, sizeof(CPackerFormatConfigData));
    return TRUE;
}

void CPackerFormatConfig::DeleteFormat(int index)
{
    Formats.Delete(index);
}

//
// ****************************************************************************
// Implementation of the configuration class - predefined packers
//

// constructor
CArchiverConfig::CArchiverConfig(/*BOOL disableDefaultValues*/)
    : Archivers(20, 10)
{
    /*
  // set default values if it is not disabled
  if (!disableDefaultValues)
    AddDefault(0);
*/
}

void CArchiverConfig::InitializeDefaultValues()
{
    // set default values if it is not disabled
    AddDefault(0);
}

// sets default values
void CArchiverConfig::AddDefault(int SalamVersion)
{
    int index;

    // BOOL SetArchiver(int index, DWORD uid, const char *title, EPackExeType type, BOOL exesAreSame,
    //                  const char *packerVariable, const char *unpackerVariable,
    //                  const char *packerExecutable, const char *unpackerExecutable,
    //                  const char *packExeFile, const char *unpackExeFile)

    switch (SalamVersion)
    {
    case 0: // default
    case 1: // version 1.52 had no packers
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_JAR32, LoadStr(IDS_EXT_JAR32), EXE_32BIT, TRUE, PACK_EXE_JAR32, NULL,
                    "jar32", NULL, "jar32", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_RAR32, LoadStr(IDS_EXT_RAR32), EXE_32BIT, TRUE, PACK_EXE_RAR32, NULL,
                    "rar", NULL, "rar", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ARJ16, LoadStr(IDS_EXT_ARJ16), EXE_16BIT, TRUE, PACK_EXE_ARJ16, NULL,
                    "arj", NULL, "arj", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_LHA16, LoadStr(IDS_EXT_LHA16), EXE_16BIT, TRUE, PACK_EXE_LHA16, NULL,
                    "lha", NULL, "lha", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_UC216, LoadStr(IDS_EXT_UC216), EXE_16BIT, TRUE, PACK_EXE_UC216, NULL,
                    "uc", NULL, "uc", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_JAR16, LoadStr(IDS_EXT_JAR16), EXE_16BIT, TRUE, PACK_EXE_JAR16, NULL,
                    "jar16", NULL, "jar16", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_RAR16, LoadStr(IDS_EXT_RAR16), EXE_16BIT, TRUE, PACK_EXE_RAR16, NULL,
                    "rar", NULL, "rar", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ZIP32, LoadStr(IDS_EXT_ZIP32), EXE_32BIT, TRUE, PACK_EXE_ZIP32, NULL,
                    "pkzip25", NULL, "pkzip25", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ZIP16, LoadStr(IDS_EXT_ZIP16), EXE_16BIT, FALSE, PACK_EXE_ZIP16, PACK_EXE_UZP16,
                    "pkzip", "pkunzip", "pkzip", "pkunzip");
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ARJ32, LoadStr(IDS_EXT_ARJ32), EXE_32BIT, TRUE, PACK_EXE_ARJ32, NULL,
                    "arj32", NULL, "arj32", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ACE32, LoadStr(IDS_EXT_ACE32), EXE_32BIT, TRUE, PACK_EXE_ACE32, NULL,
                    "ace32", NULL, "ace32", NULL);
        if ((index = AddArchiver()) == -1)
            return;
        SetArchiver(index, ARC_UID_ACE16, LoadStr(IDS_EXT_ACE16), EXE_16BIT, TRUE, PACK_EXE_ACE16, NULL,
                    "ace", NULL, "ace", NULL);
        //    case 2:  // what was added after beta1
        //    case 3:  // what was added after beta2
        //    case 4:  // beta3 but with old configuration (contains $(SpawnName))
        //    case 5:   // beta3 but without tar
        //    case 6:   // what's new in beta4?
    }
}

// initializes the configuration based on another configuration
BOOL CArchiverConfig::Load(CArchiverConfig& src)
{
    // clear what we have (if we have anything)
    DeleteAllArchivers();
    // and add what we received
    int i;
    for (i = 0; i < src.GetArchiversCount(); i++)
    {
        int index = AddArchiver();
        if (index == -1)
            return FALSE;
        if (!SetArchiver(index, src.GetArchiverUID(i), src.GetArchiverTitle(i), src.GetArchiverType(i),
                         src.ArchiverExesAreSame(i),
                         src.GetPackerVariable(i), src.GetUnpackerVariable(i),
                         src.GetPackerExecutable(i), src.GetUnpackerExecutable(i),
                         src.GetPackerExeFile(i), src.GetUnpackerExeFile(i)))
            return FALSE;
    }
    return TRUE;
}

// creates a new empty archiver at the end of the array
int CArchiverConfig::AddArchiver()
{
    CArchiverConfigData* data = new CArchiverConfigData;
    if (data == NULL)
        return -1;
    int index = Archivers.Add(data);
    if (!Archivers.IsGood())
    {
        Archivers.ResetState();
        delete data;
        return -1;
    }
    return index;
}

// sets the archiver at the given index to the requested values
BOOL CArchiverConfig::SetArchiver(int index, DWORD uid, const char* title, EPackExeType type, BOOL exesAreSame,
                                  const char* packerVariable, const char* unpackerVariable,
                                  const char* packerExecutable, const char* unpackerExecutable,
                                  const char* packExeFile, const char* unpackExeFile)
{
    // clear old data, if we have any
    CArchiverConfigData* data = Archivers[index];
    data->Destroy();

    data->UID = uid;
    data->Title = DupStr(title);
    data->Type = type;
    data->ExesAreSame = exesAreSame;
    // the variable and executable name are constant strings from Salamander's code; a shallow copy is enough
    data->PackerVariable = packerVariable;
    data->PackerExecutable = packerExecutable;
    // the path to the executable is allocated, make a copy
    data->PackExeFile = DupStr(packExeFile);
    if (!data->ExesAreSame)
    {
        // if the unpacker differs, initialize it as well
        data->UnpackerVariable = unpackerVariable;
        data->UnpackerExecutable = unpackerExecutable;
        data->UnpackExeFile = DupStr(unpackExeFile);
    }
    else
    {
        // if the packer is the same, the work is easier
        data->UnpackerVariable = NULL;
        data->UnpackerExecutable = NULL;
        data->UnpackExeFile = DupStr(packExeFile);
    }

    // are the values meaningful?
    if (data->IsValid())
        return TRUE;
    else
    {
        TRACE_E("invalid data");
        Archivers.Delete(index);
        return FALSE;
    }
}

// sets the packer path for the packer at the given index
// (called from the transfer when closing the auto-configuration dialog)
void CArchiverConfig::SetPackerExeFile(int index, const char* filename)
{
    CArchiverConfigData* data = Archivers[index];
    if (data->PackExeFile)
        free(data->PackExeFile);
    // if we get NULL (not found by auto-configuration), use the default executable name
    data->PackExeFile = DupStr(filename != NULL ? filename : data->PackerExecutable);
}

// sets the packer path for the unpacker at the given index
// (called from the transfer when closing the auto-configuration dialog)
void CArchiverConfig::SetUnpackerExeFile(int index, const char* filename)
{
    CArchiverConfigData* data = Archivers[index];
    if (data->UnpackExeFile)
        free(data->UnpackExeFile);
    // if we get NULL (not found by auto-configuration), use the default executable name
    data->UnpackExeFile = DupStr(filename != NULL ? filename : data->UnpackerExecutable);
}

// saves the configuration of a single entry into the registry
BOOL CArchiverConfig::Save(int index, HKEY hKey)
{
    DWORD d;
    BOOL ret = TRUE;
    // save UID
    d = GetArchiverUID(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_UID, REG_DWORD, &d, sizeof(d));
    // (saves the title) - because it is translated it can no longer be used for identification
    //   of the archiver (UID is now used instead), there is no point in storing it
    //  if (ret) ret &= SetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, GetArchiverTitle(index), -1);
    // save the executable path
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, GetPackerExeFile(index), -1);
    // save whether packer and unpacker are the same
    d = ArchiverExesAreSame(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &d, sizeof(d));

    // and if not, also store the path to the unpacker
    if (!ArchiverExesAreSame(index))
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, GetUnpackerExeFile(index), -1);
    return ret;
}

// loads configuration of a single entry from the registry

// j.r. I reworked configuration loading -- it searches the default list and if a
//      registry item is found in this default list, its values are used, otherwise it is ignored.
//      The old method caused problems because users manually deleted the key
//      contents and there was no path (from the dialog) to restore the original list.
//      It also crashed Salamander, see bug CCfgPageExternalArchivers::DialogProc(0x111

BOOL CArchiverConfig::Load(HKEY hKey)
{
    int max = MAX_PATH + 2;
    char title[MAX_PATH + 2];
    title[0] = 0;
    char packExe[MAX_PATH + 2];
    packExe[0] = 0;
    char unpackExe[MAX_PATH + 2];
    unpackExe[0] = 0;
    DWORD exesAreSame;
    DWORD uid = -1;

    BOOL ret = TRUE;
    // loads the title
    if (ret && Configuration.ConfigVersion <= 64)
        ret &= GetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, title, max);
    // loads the packing executable
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, packExe, max);
    // determine whether the unpacker is the same
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &exesAreSame, sizeof(DWORD));
    // UID of the archiver (Title was previously used instead, but it is translated now and it can no longer be used)
    if (ret && Configuration.ConfigVersion > 64)
        ret &= GetValue(hKey, SALAMANDER_PPC_UID, REG_DWORD, &uid, sizeof(DWORD));
    // load the unpacker executable, if it is different from the packer
    if (!exesAreSame)
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, unpackExe, max);

    if (ret)
    {
        int i;
        for (i = 0; i < Archivers.Count; i++)
        {
            CArchiverConfigData* arch = Archivers[i];
            // for keys that are complete and whose title matches the default value, take over their paths
            if (Configuration.ConfigVersion <= 64 && stricmp(title, arch->Title) == 0 || // Title is now translated and cannot be used anymore
                Configuration.ConfigVersion > 64 && uid == arch->UID)                    // thus we introduced a standard UID
            {
                SetPackerExeFile(i, packExe);
                SetUnpackerExeFile(i, exesAreSame ? packExe : unpackExe);
                break;
            }
        }
    }
    return ret;
}
/*
BOOL
CArchiverConfig::Load(HKEY hKey)
{
  int max = MAX_PATH + 2;
  char title[MAX_PATH + 2]; title[0] = 0;
  char packExe[MAX_PATH + 2]; packExe[0] = 0;
  char unpackExe[MAX_PATH + 2]; unpackExe[0] = 0;
  DWORD exesAreSame;

  BOOL ret = TRUE;
  // loads the title
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, title, max);
  // loads the packing executable
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, packExe, max);
  // determine whether the unpacker is the same
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &exesAreSame, sizeof(DWORD));
  // loads the unpacker executable, if it is different from the packer
  if (!exesAreSame)
    if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, unpackExe, max);

  EPackExeType type;
  const char *name, *variablePack, *variableUnpack = NULL, *exePack, *exeUnpack = NULL;

  // and now convert to a newer configuration - missing information is taken from defaults
  // (none of it is configurable anyway :-))
  if (ret)
  {
    int index;
    if ((index = AddArchiver()) == -1) return FALSE;
    // I now assume the indices in the configuration keep their order. If not, nothing is loaded
    switch (index)
    {
      case 0:
        name = LoadStr(IDS_EXT_JAR32);
        type = EXE_32BIT;
        variablePack = PACK_EXE_JAR32;
        exePack = "jar32";
        break;
      case 1:
        name = LoadStr(IDS_EXT_RAR32);
        type = EXE_32BIT;
        variablePack = PACK_EXE_RAR32;
        exePack = "rar";
        break;
      case 2:
        name = LoadStr(IDS_EXT_ARJ16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_ARJ16;
        exePack = "arj";
        break;
      case 3:
        name = LoadStr(IDS_EXT_LHA16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_LHA16;
        exePack = "lha";
        break;
      case 4:
        name = LoadStr(IDS_EXT_UC216);
        type = EXE_16BIT;
        variablePack = PACK_EXE_UC216;
        exePack = "uc";
        break;
      case 5:
        name = LoadStr(IDS_EXT_JAR16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_JAR16;
        exePack = "jar16";
        break;
      case 6:
        name = LoadStr(IDS_EXT_RAR16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_RAR16;
        exePack = "rar";
        break;
      case 7:
        name = LoadStr(IDS_EXT_ZIP32);
        type = EXE_32BIT;
        variablePack = PACK_EXE_ZIP32;
        exePack = "pkzip25";
        break;
      case 8:
        name = LoadStr(IDS_EXT_ZIP16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_ZIP16;
        variableUnpack = PACK_EXE_UZP16;
        exePack = "pkzip";
        exeUnpack = "pkunzip";
        break;
      case 9:
        name = LoadStr(IDS_EXT_ARJ32);
        type = EXE_32BIT;
        variablePack = PACK_EXE_ARJ32;
        exePack = "arj32";
        break;
      case 10:
        name = LoadStr(IDS_EXT_ACE32);
        type = EXE_32BIT;
        variablePack = PACK_EXE_ACE32;
        exePack = "ace32";
        break;
      case 11:
        name = LoadStr(IDS_EXT_ACE16);
        type = EXE_16BIT;
        variablePack = PACK_EXE_ACE16;
        exePack = "ace";
        break;
      default:
        TRACE_E("Too big index of packer, probably mistake in registry");
        Archivers.Delete(index);  // To avoid leaving an uninitialized structure; Salamander 2.0 crashed in SaveConfig
        return FALSE;
    }
    // verify we are really adding the packer we think we are adding
    if (strncmp(title, name, 10) || (exesAreSame && exeUnpack != NULL) || (!exesAreSame && exeUnpack == NULL))
    {
      TRACE_E("Inconsistency in configuration of packers.");
      Archivers.Delete(index);  // To avoid leaving an uninitialized structure; Salamander 2.0 crashed in SaveConfig
      return FALSE;
    }
    // and set all information
    ret &= SetArchiver(index, name, type, exesAreSame, variablePack, variableUnpack,
                       exePack, exeUnpack, packExe, unpackExe);
  }
  return ret;
}
*/

//
// ****************************************************************************
// Parsing functions for replacing variables with their values
//

const char* WINAPI PackExpArcPath(HWND msgParent, void* param)
{
    SPackExpData* data = (SPackExpData*)param;
    const char* s = strrchr(data->ArcName, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in PackExpArcPath().");
        return NULL;
    }
    strncpy(data->Buffer, data->ArcName, s - data->ArcName + 1);
    data->Buffer[s - data->ArcName + 1] = '\0';
    return data->Buffer;
}

const char* WINAPI PackExpArcName(HWND msgParent, void* param)
{
    SPackExpData* data = (SPackExpData*)param;
    if (!data->ArcNameFilePossible)
    {
        TRACE_E("It is not possible to combine DOS and long archive file name (ArchiveFileName and ArchiveFullName) in PackExpArcName().");
        return NULL;
    }
    data->DOSTmpFilePossible = FALSE; // from now on only ArcName

    return data->ArcName;
}

const char* WINAPI PackExpArcFile(HWND msgParent, void* param)
{
    SPackExpData* data = (SPackExpData*)param;

    if (!data->ArcNameFilePossible)
    {
        TRACE_E("It is not possible to combine DOS and long archive file name (ArchiveFileName and ArchiveFullName) in PackExpArcFile().");
        return NULL;
    }
    data->DOSTmpFilePossible = FALSE; // from now on only ArcName

    const char* s = strrchr(data->ArcName, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in PackExpArcFile().");
        return NULL;
    }
    strcpy(data->Buffer, s + 1);
    return data->Buffer;
}

const char* WINAPI PackExpArcDosName(HWND msgParent, void* param)
{
    char buff2[MAX_PATH];
    SPackExpData* data = (SPackExpData*)param;

    if (data->ArcNameFilePossible)
    {
        if (!GetShortPathName(data->ArcName, buff2, MAX_PATH))
        {
            if (!data->DOSTmpFilePossible)
            {
                TRACE_E("Error (1) in GetShortPathName() in PackExpArcDosName().");
                return NULL;
            }
            data->ArcNameFilePossible = FALSE; // from now on only DOSTmpName
        }
        else
            data->DOSTmpFilePossible = FALSE; // from now on only ArcName
    }
    else
    {
        if (!data->DOSTmpFilePossible)
        {
            TRACE_E("Unable to return DOS nor long archive file name.");
            return NULL;
        }
    }

    if (data->DOSTmpFilePossible) // use a substitute name
    {
        if (data->DOSTmpFile[0] == 0) // it needs to be generated
        {
            char path[MAX_PATH + 50];
            strcpy(path, data->ArcName);
            if (CutDirectory(path))
            {
                char* s = path + strlen(path);
                if (s > path && *(s - 1) != '\\')
                    *s++ = '\\';
                strcpy(s, "PACK");
                s += 4;
                DWORD randNum = (GetTickCount() & 0xFFF);
                while (1)
                {
                    sprintf(s, "%X.*", randNum);
                    WIN32_FIND_DATA findData;
                    HANDLE find = HANDLES_Q(FindFirstFile(path, &findData));
                    if (find != INVALID_HANDLE_VALUE)
                        HANDLES(FindClose(find)); // this name already exists with some extension, searching again
                    else
                    {
                        sprintf(s, "%X", randNum);
                        s += strlen(s);
                        const char* ext = data->ArcName + strlen(data->ArcName);
                        //            while (--ext > data->ArcName && *ext != '\\' && *ext != '.');
                        while (--ext >= data->ArcName && *ext != '\\' && *ext != '.')
                            ;
                        //            if (ext > data->ArcName && *ext == '.' && *(ext - 1) != '\\')  // copy the archive extension (used by multi-volume archivers: ARJ->A01,A02,...); ".cvspass" in Windows is an extension ...
                        if (ext >= data->ArcName && *ext == '.') // copy the archive extension (used by multi-volume archivers: ARJ->A01,A02,...)
                        {
                            int count = 4; // copy '.' plus at most 3 allowed extension characters (of the 8.3 format)
                            while (count-- && *ext < 128 && *ext != '[' && *ext != ']' &&
                                   *ext != ';' && *ext != '=' && *ext != ',' && *ext != ' ')
                            {
                                *s++ = *ext++;
                            }
                            *s = 0;
                        }
                        break; // we can use this name (it does not exist with any extension yet)
                    }
                    randNum++;
                }

                HANDLE h = HANDLES_Q(CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                                FILE_ATTRIBUTE_NORMAL, NULL));
                if (h != INVALID_HANDLE_VALUE)
                {
                    HANDLES(CloseHandle(h));
                    strcpy(data->DOSTmpFile, path);
                    BOOL ok = GetShortPathName(data->DOSTmpFile, buff2, MAX_PATH);
                    DeleteFile(data->DOSTmpFile); // we no longer need the file (let the archiver create it)
                    if (!ok)
                    {
                        TRACE_E("Error (2) in GetShortPathName() in PackExpArcDosName().");
                        return NULL;
                    }
                    strcpy(data->DOSTmpFile, buff2); // we obtained a substitute name
                }
                else
                {
                    DWORD err = GetLastError();
                    TRACE_E("Unable to create file with DOS-name in PackExpArcDosName(), error=" << err);
                    return NULL;
                }
            }
            else
            {
                TRACE_E("Unexpected situation in PackExpArcDosName().");
                return NULL;
            }
        }
        strcpy(buff2, data->DOSTmpFile);
    }

    strcpy(data->Buffer, buff2);
    return data->Buffer;
}

const char* WINAPI PackExpArcDosFile(HWND msgParent, void* param)
{
    char* s = (char*)PackExpArcDosName(msgParent, param);
    if (s == NULL)
    {
        TRACE_E("Previous TRACE_E belongs to PackExpArcDosFile().");
        return NULL;
    }

    char buff2[MAX_PATH];
    SPackExpData* data = (SPackExpData*)param;
    strcpy(buff2, s);

    s = strrchr(buff2, '\\');
    if (s == NULL)
    {
        TRACE_E("Unexpected value in PackExpArcDosFile().");
        return NULL;
    }
    strcpy(data->Buffer, s + 1);
    return data->Buffer;
}

const char* WINAPI PackExpSrcPath(HWND msgParent, void* param)
{
    if (((SPackExpData*)param)->SrcDir == NULL)
    {
        TRACE_E("Unexpected call to PackExpSrcPath().");
        return NULL;
    }
    return ((SPackExpData*)param)->SrcDir;
}

const char* WINAPI PackExpTgtPath(HWND msgParent, void* param)
{
    if (((SPackExpData*)param)->TgtDir == NULL)
    {
        TRACE_E("Unexpected call to PackExpTgtPath().");
        return NULL;
    }
    return ((SPackExpData*)param)->TgtDir;
}

const char* WINAPI PackExpTgtDosPath(HWND msgParent, void* param)
{
    SPackExpData* data = (SPackExpData*)param;
    if (data->TgtDir == NULL)
    {
        TRACE_E("Unexpected call to PackExpTgtDosPath().");
        return NULL;
    }
    if (!GetShortPathName(data->TgtDir, data->Buffer, MAX_PATH))
    {
        TRACE_E("Error in GetShortPathName() in PackExpTgtDosPath().");
        return NULL;
    }
    return data->Buffer;
}

const char* WINAPI PackExpLstName(HWND msgParent, void* param)
{
    if (((SPackExpData*)param)->LstName == NULL)
    {
        TRACE_E("Unexpected call to PackExpLstName().");
        return NULL;
    }
    return ((SPackExpData*)param)->LstName;
}

const char* WINAPI PackExpLstDosName(HWND msgParent, void* param)
{
    SPackExpData* data = (SPackExpData*)param;
    if (data->LstName == NULL)
    {
        TRACE_E("Unexpected call to PackExpLstDosName().");
        return NULL;
    }
    if (!GetShortPathName(data->LstName, data->Buffer, MAX_PATH))
    {
        TRACE_E("Error in GetShortPathName() in PackExpLstDosName().");
        return NULL;
    }
    return data->Buffer;
}

const char* WINAPI PackExpExtName(HWND msgParent, void* param)
{
    if (((SPackExpData*)param)->ExtName == NULL)
    {
        TRACE_E("Unexpected call to PackExpExtName().");
        return NULL;
    }
    return ((SPackExpData*)param)->ExtName;
}

const char* WINAPI
PackExpExeName(unsigned int index, BOOL unpacker = FALSE)
{
    // buffer for shortening the program name
    static char PackExpExeName[MAX_PATH];
    char buff[MAX_PATH];
    const char* exe;
    if (!unpacker)
        exe = ArchiverConfig.GetPackerExeFile(index);
    else
        exe = ArchiverConfig.GetUnpackerExeFile(index);
    // if the packer is not configured it should not be used, but
    // it's not excluded, so use the program name without the path
    if (exe == NULL)
        if (!unpacker)
            exe = ArchiverConfig.GetPackerExecutable(index);
        else
            exe = ArchiverConfig.GetUnpackerExecutable(index);
    else
    {
        // on older Windows it was impossible to redirect output from a DOS program in a directory
        // with a long name; I no longer feel like patching and risking this that it won't work
        buff[0] = '\0';
        DWORD len = GetShortPathName(exe, buff, MAX_PATH);
        // if the path was shortened successfully, return the short name
        if (len == strlen(buff) && len > 0)
        {
            strcpy(PackExpExeName, buff);
            return PackExpExeName;
        }
    }
    // for long names, check the quotes...
    unsigned long src = 0, dst = 0;
    if (exe[src] != '"')
        buff[dst++] = '"';
    while (exe[src] != '\0' && dst < MAX_PATH)
        buff[dst++] = exe[src++];
    if (src == 0 || exe[src - 1] != '"')
        buff[dst++] = '"';
    buff[dst] = '\0';
    if (!ExpandCommand(NULL, buff, PackExpExeName, MAX_PATH, FALSE))
        strcpy(PackExpExeName, buff);
    return PackExpExeName;
}

const char* WINAPI PackExpJar32ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKJAR32INDEX);
}

const char* WINAPI PackExpJar16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKJAR16INDEX);
}

const char* WINAPI PackExpRar32ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKRAR32INDEX);
}

const char* WINAPI PackExpRar16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKRAR16INDEX);
}

const char* WINAPI PackExpArj32ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKARJ32INDEX);
}

const char* WINAPI PackExpArj16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKARJ16INDEX);
}

const char* WINAPI PackExpLha16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKLHA16INDEX);
}

const char* WINAPI PackExpUc216ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKUC216INDEX);
}

const char* WINAPI PackExpAce32ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKACE32INDEX);
}

const char* WINAPI PackExpAce16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKACE16INDEX);
}

const char* WINAPI PackExpZip32ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKZIP32INDEX);
}

const char* WINAPI PackExpZip16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKZIP16INDEX);
}

const char* WINAPI PackExpUzp16ExeName(HWND msgParent, void* param)
{
    return PackExpExeName(PACKZIP16INDEX, TRUE);
}

//
// ****************************************************************************
// Constants
// ****************************************************************************
//

// ****************************************************************************
// tables assigning individual evaluation functions to specific variables
//

// command line table
CSalamanderVarStrEntry PackCmdLineExpArray[] =
    {
        {PACK_ARC_PATH, PackExpArcPath},
        {PACK_ARC_FILE, PackExpArcFile},
        {PACK_ARC_DOSFILE, PackExpArcDosFile},
        {PACK_ARC_NAME, PackExpArcName},
        {PACK_ARC_DOSNAME, PackExpArcDosName},
        {PACK_TGT_PATH, PackExpTgtPath},
        {PACK_TGT_DOSPATH, PackExpTgtDosPath},
        {PACK_LST_NAME, PackExpLstName},
        {PACK_LST_DOSNAME, PackExpLstDosName},
        {PACK_EXT_NAME, PackExpExtName},
        {PACK_EXE_JAR32, PackExpJar32ExeName},
        {PACK_EXE_JAR16, PackExpJar16ExeName},
        {PACK_EXE_RAR32, PackExpRar32ExeName},
        {PACK_EXE_RAR16, PackExpRar16ExeName},
        {PACK_EXE_ARJ32, PackExpArj32ExeName},
        {PACK_EXE_ARJ16, PackExpArj16ExeName},
        {PACK_EXE_LHA16, PackExpLha16ExeName},
        {PACK_EXE_UC216, PackExpUc216ExeName},
        {PACK_EXE_ACE32, PackExpAce32ExeName},
        {PACK_EXE_ACE16, PackExpAce16ExeName},
        {PACK_EXE_ZIP32, PackExpZip32ExeName},
        {PACK_EXE_ZIP16, PackExpZip16ExeName},
        {PACK_EXE_UZP16, PackExpUzp16ExeName},
        // sentinel
        {NULL, NULL}};

// current directory table
CSalamanderVarStrEntry PackInitDirExpArray[] =
    {
        {PACK_ARC_PATH, PackExpArcPath},
        {PACK_SRC_PATH, PackExpSrcPath},
        {PACK_TGT_PATH, PackExpTgtPath},
        {PACK_TGT_DOSPATH, PackExpTgtDosPath},
        {NULL, NULL}};

//
// ****************************************************************************
// Functions
// ****************************************************************************
//

//
// ****************************************************************************
// Functions for variable expansion
//

//
// ****************************************************************************
// BOOL PackExpandCmdLine(const char *archiveName, const char *tgtDir, const char *lstName,
//                        const char *extName, const char *exeName, const char *varText,
//                        char *buffer, const int bufferLen, char *DOSTmpName)
//
//   Expands variables in the command line
//
//   RET:  TRUE on success, FALSE on error
//   IN:   archiveName is the name of the archive we work with
//         tgtDir is the target directory name for the operation or NULL
//         lstName is the name of the file listing processed items or NULL
//         extName is the name of the extracted file or NULL
//         varText is the command line with variables
//         bufferLen is the size of the buffer parameter
//         DOSTmpName is NULL if a long name cannot be replaced by a substitute DOS name,
//           otherwise it points to a buffer at least MAX_PATH long
//   OUT:  buffer is the command line with variables expanded
//         DOSTmpName is the name of the temporary file (or an empty string if no replacement was made)

BOOL PackExpandCmdLine(const char* archiveName, const char* tgtDir, const char* lstName,
                       const char* extName, const char* varText, char* buffer,
                       const int bufferLen, char* DOSTmpName)
{
    CALL_STACK_MESSAGE7("PackExpandCmdLine(%s, %s, %s, %s, %s, , %d,)",
                        archiveName, tgtDir, lstName, extName, varText, bufferLen);
    SPackExpData data;
    data.ArcName = archiveName;
    data.SrcDir = NULL;
    data.TgtDir = tgtDir;
    data.LstName = lstName;
    data.ExtName = extName;
    data.ArcNameFilePossible = TRUE;
    data.DOSTmpFilePossible = DOSTmpName != NULL;
    if (DOSTmpName != NULL)
        DOSTmpName[0] = 0;
    data.DOSTmpFile = DOSTmpName;
    return ExpandVarString(MainWindow->HWindow, varText, buffer, bufferLen, PackCmdLineExpArray, &data);
}

//
// ****************************************************************************
// BOOL PackExpandInitDir(const char *archiveName, const char *srcDir, const char *tgtDir,
//                        const char *varText, char *buffer, const int bufferLen)
//
//   Expands variables in the string specifying the current directory for the launched program
//
//   RET:  TRUE on success, FALSE on error
//   IN:   archiveName is the archive name we work with
//         srcDir is the source directory for the operation or NULL
//         tgtDir is the target directory for the operation or NULL
//         varText is the command line with variables
//         bufferLen is the size of the buffer parameter
//   OUT:  buffer is the command line with variables expanded

BOOL PackExpandInitDir(const char* archiveName, const char* srcDir, const char* tgtDir,
                       const char* varText, char* buffer, const int bufferLen)
{
    CALL_STACK_MESSAGE6("PackExpandInitDir(%s, %s, %s, %s, , %d)",
                        archiveName, srcDir, tgtDir, varText, bufferLen);
    SPackExpData data;
    data.ArcName = archiveName;
    data.SrcDir = srcDir;
    data.TgtDir = tgtDir;
    data.LstName = NULL;
    data.ExtName = NULL;
    data.ArcNameFilePossible = TRUE;
    data.DOSTmpFilePossible = FALSE;
    data.DOSTmpFile = NULL;
    return ExpandVarString(MainWindow->HWindow, varText, buffer, bufferLen, PackInitDirExpArray, &data);
}

//
// ****************************************************************************
// General functions
//

//
// ****************************************************************************
// BOOL EmptyErrorHandler(HWND parent, const WORD err, ...)
//
//   Empty error function - to handle errors correctly, replace this function
//   in PackErrorHandlerPtr pointer with your own function that processes the error as needed.
//   It is used not only to report errors that occurred (IDS_PACKERR_*) but also
//   to resolve unexpected situations by asking the user (IDS_PACKQRY_*).
//
//   RET:  TRUE to continue, FALSE to abort
//   IN:   parent is the parent window of message boxes
//         err is the error number that occurred
//         remaining parameters further specify the error depending on its code

BOOL EmptyErrorHandler(HWND parent, const WORD err, ...)
{
    TRACE_E("Pack Empty Error Handler: error code " << err);
    return FALSE;
}

//
// ****************************************************************************
// void PackSetErrorHandler(BOOL (*handler)(HWND parent, const WORD errNum, ...))
//
//   Sets the error handling function
//
//   RET:
//   IN:   handler is the new function for error processing

void PackSetErrorHandler(BOOL (*handler)(HWND parent, const WORD errNum, ...))
{
    if (handler == NULL)
        PackErrorHandlerPtr = EmptyErrorHandler;
    else
        PackErrorHandlerPtr = handler;
}

//
// ****************************************************************************
// BOOL PackExecute(HWND parent, char *cmdLine, const char *currentDir, TPackErrorTable *const errorTable)
//
//   Runs the external program given (including parameters) in cmdLine string
//
//   RET: returns TRUE on success, FALSE on error
//        on error the callback *PackErrorHandlerPtr is called
//   IN:  parent is the parent window for message boxes
//        cmdLine is the command line to execute
//        currentDir is the full current directory for the launched program or NULL if it doesn't matter
//        errorTable is a pointer to the return code table (if NULL, no table)

BOOL PackExecute(HWND parent, char* cmdLine, const char* currentDir, TPackErrorTable* const errorTable)
{
    CALL_STACK_MESSAGE3("PackExecute(, %s, %s, ,)", cmdLine, currentDir);

    // if we haven't determined the path to the spawn yet, do it now
    if (!InitSpawnName(parent))
        return FALSE;

    // set everything needed to create the process
    PROCESS_INFORMATION pi;
    STARTUPINFO si;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    if (PackWinTimeout != 0)
    {
        si.dwFlags = STARTF_USESHOWWINDOW;
        POINT p;
        if (MultiMonGetDefaultWindowPos(MainWindow->HWindow, &p))
        {
            // if the main window is on another monitor we should open the new window there
            // preferably at the default position (as on the primary monitor)
            si.dwFlags |= STARTF_USEPOSITION;
            si.dwX = p.x;
            si.dwY = p.y;
        }
        si.wShowWindow = SW_MINIMIZE;
    }

    // Determine what we are actually running (for error reporting)
    int i = 0, j = 0;
    char cmd[MAX_PATH];
    // skip leading whitespace
    while (cmdLine[i] != '\0' && (cmdLine[i] == ' ' || cmdLine[i] == '\t'))
        i++;
    // read the program name
    if (cmdLine[i] == '"')
    {
        i++;
        while (j < MAX_PATH && cmdLine[i] != '\0' && cmdLine[i] != '"')
            cmd[j++] = cmdLine[i++];
    }
    else
        while (j < MAX_PATH && cmdLine[i] != '\0' && cmdLine[i] != ' ' && cmdLine[i] != '\t' && cmdLine[i] != '"')
            cmd[j++] = cmdLine[i++];
    cmd[j] = '\0';

    char* tmpCmdLine = (char*)malloc(2 + strlen(SpawnExe) + 2 + strlen(SPAWN_EXE_PARAMS) + 1 + strlen(cmdLine) + 1);
    if (tmpCmdLine == NULL)
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_NOMEM);
    sprintf(tmpCmdLine, "\"%s\" %s %s", SpawnExe, SPAWN_EXE_PARAMS, cmdLine);
    // launch the external program
    if (!HANDLES(CreateProcess(NULL, tmpCmdLine, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, currentDir, &si, &pi)))
    {
        DWORD err = GetLastError();
        free(tmpCmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));
    }
    free(tmpCmdLine);

    // create a modal window
    HWND hFocusedWnd = GetFocus();
    HWND main = parent == NULL ? MainWindow->HWindow : parent;
    CExecuteWindow tmpWindow(main, IDS_PACK_EXECUTING, ooStatic);
    tmpWindow.Create();
    HWND oldPluginMsgBoxParent = PluginMsgBoxParent;
    // plugin timers may be invoked (happens with WinSCP open in the other panel) -> set parent for message boxes
    PluginMsgBoxParent = tmpWindow.HWindow;
    EnableWindow(main, FALSE);
    // activate the hourglass cursor
    HCURSOR prevCrsr = SetCursor(LoadCursor(NULL, IDC_WAIT));
    // Wait for the external program to finish
    HANDLE objects[] = {pi.hProcess};
    DWORD start = GetTickCount();
    DWORD elapsed = 0;

    DWORD ret;
    do
    {
        /*  // Petr: pumping only WM_PAINT leads to blocking all other instances of Salamander
    //       (even newly started ones) and other softwares (at least during Paste), if we
    //       put a file or directory on the clipboard before packing. Accessing clipboard
    //       data causes OLE to communicate with this process which doesn't respond
    //       because it pumps only WM_PAINT.
    // Original Tom's variant:
    ret = MsgWaitForMultipleObjects(1, objects, FALSE,
                                    PackWinTimeout <= 0 ? INFINITE : PackWinTimeout - elapsed,
                                    QS_PAINT);
*/
        ret = MsgWaitForMultipleObjects(1, objects, FALSE,
                                        PackWinTimeout <= 0 ? INFINITE : PackWinTimeout - elapsed,
                                        QS_ALLINPUT);
        if (ret == WAIT_OBJECT_0 + 1)
        {
            // if a message arrived, handle it
            MSG msg;
            /*    // Original Tom's variant: (see description above)
      while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
        DispatchMessage(&msg);
*/
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // if the timeout has expired, stop
            elapsed = GetTickCount() - start;
            if (PackWinTimeout > 0 && (int)elapsed >= PackWinTimeout)
            {
                ret = WAIT_TIMEOUT;
                break;
            }
        }
    } while (ret == WAIT_OBJECT_0 + 1); // wait while WM_PAINT messages arrive

    if (ret == WAIT_TIMEOUT)
    {
        HWND win = NULL;
        DWORD pid;
        do
        {
            win = FindWindowEx(NULL, win, "ConsoleWindowClass", NULL);
            GetWindowThreadProcessId(win, &pid);
            if (pid == pi.dwProcessId)
            {
                ShowWindow(win, SW_RESTORE);
                break;
            }
        } while (win != NULL);
        do
        {
            /*    // Original Tom's variant: (see description above)
      ret = MsgWaitForMultipleObjects(1, objects, FALSE, INFINITE, QS_PAINT);
*/
            ret = MsgWaitForMultipleObjects(1, objects, FALSE, INFINITE, QS_ALLINPUT);
            MSG msg;
            if (ret == WAIT_OBJECT_0 + 1)
            {
                /*      // Original Tom's variant: (see description above)
        while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
          DispatchMessage(&msg);
*/
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        } while (ret == WAIT_OBJECT_0 + 1); // wait while WM_PAINT messages arrive
    }

    EnableWindow(main, TRUE);
    PluginMsgBoxParent = oldPluginMsgBoxParent;
    DestroyWindow(tmpWindow.HWindow);
    // if Salamander is active, call SetFocus on the stored window (SetFocus does
    // not work when the main window is disabled - after deactivation/activation
    // of the disabled main window, the active panel has no focus)
    HWND hwnd = GetForegroundWindow();
    while (hwnd != NULL && hwnd != main)
        hwnd = GetParent(hwnd);
    if (hwnd == main)
        SetFocus(hFocusedWnd);
    // remove the hourglass cursor
    SetCursor(prevCrsr);
    UpdateWindow(main);

    if (ret == WAIT_FAILED)
    {
        char buffer[1000];
        strcpy(buffer, "WaitForSingleObject: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // and find out how it ended - hopefully they all return 0 as success
    DWORD exitCode;
    if (!GetExitCodeProcess(pi.hProcess, &exitCode))
    {
        char buffer[1000];
        strcpy(buffer, "GetExitCodeProcess: ");
        strcat(buffer, GetErrorText(GetLastError()));
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
    }

    // release handles of the process
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    if (exitCode != 0)
    {
        //
        // First handle salspawn.exe errors if we used it
        //
        if (exitCode >= SPAWN_ERR_BASE)
        {
            // salspawn.exe error - bad parameters or similar
            if (exitCode >= SPAWN_ERR_BASE && exitCode < SPAWN_ERR_BASE * 2)
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, SPAWN_EXE_NAME, LoadStr(IDS_PACKRET_SPAWN));
            // CreateProcess error
            if (exitCode >= SPAWN_ERR_BASE * 2 && exitCode < SPAWN_ERR_BASE * 3)
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PROCESS, cmd, GetErrorText(exitCode - SPAWN_ERR_BASE * 2));
            // WaitForSingleObject error
            if (exitCode >= SPAWN_ERR_BASE * 3 && exitCode < SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "WaitForSingleObject: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 3));
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            // GetExitCodeProcess error
            if (exitCode >= SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "GetExitCodeProcess: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 4));
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
        }
        //
        // now come the external program errors
        //
        // if errorTable == NULL, no translation is done (table doesn't exist)
        if (!errorTable)
        {
            char buffer[1000];
            sprintf(buffer, LoadStr(IDS_PACKRET_GENERAL), exitCode);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, cmd, buffer);
        }
        // find the corresponding text in the table
        for (i = 0; (*errorTable)[i][0] != -1 &&
                    (*errorTable)[i][0] != (int)exitCode;
             i++)
            ;
        // was it found?
        if ((*errorTable)[i][0] == -1)
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, cmd, LoadStr(IDS_PACKRET_UNKNOWN));
        else
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, cmd, LoadStr((*errorTable)[i][1]));
    }
    return TRUE;
}

//****************************************************************************
//
// CExecuteWindow
//

CExecuteWindow::CExecuteWindow(HWND hParent, int textResID, CObjectOrigin origin)
    : CWindow(origin)
{
    CALL_STACK_MESSAGE2("CExecuteWindow::CExecuteWindow(, %d, )", textResID);
    HParent = hParent;
    char* t = LoadStr(textResID);
    int len = (int)strlen(t);
    Text = new char[len + 1];
    if (Text == NULL)
        TRACE_E(LOW_MEMORY);
    else
        strcpy(Text, t);
}

CExecuteWindow::~CExecuteWindow()
{
    CALL_STACK_MESSAGE1("CExecuteWindow::~CExecuteWindow()");
    if (Text != NULL)
        delete[] Text;
}

#define EXECUTEWINDOW_HMARGIN 25
#define EXECUTEWINDOW_VMARGIN 18

HWND CExecuteWindow::Create()
{
    CALL_STACK_MESSAGE1("CExecuteWindow::Create()");
    // compute text size => window size
    SIZE s;
    s.cx = 300;
    s.cy = 30;
    HDC dc = HANDLES(GetDC(NULL));
    if (dc != NULL)
    {
        HFONT old = (HFONT)SelectObject(dc, EnvFont);
        GetTextExtentPoint32(dc, Text, (int)strlen(Text), &s);
        SelectObject(dc, old);
        HANDLES(ReleaseDC(NULL, dc));
    }

    int width = s.cx + 2 * EXECUTEWINDOW_HMARGIN;
    int height = s.cy + 2 * EXECUTEWINDOW_VMARGIN;
    int x;
    int y;

    RECT r2;
    GetWindowRect(MainWindow->HWindow, &r2);
    x = (r2.right + r2.left - width) / 2;
    GetWindowRect(MainWindow->LeftPanel->HWindow, &r2);
    y = (r2.bottom + r2.top - height) / 2;

    CreateEx(WS_EX_DLGMODALFRAME,
             SAVEBITS_CLASSNAME,
             "",
             WS_BORDER | WS_POPUP,
             x, y, width, height,
             HParent,
             NULL,
             HInstance,
             this);

    ShowWindow(HWindow, SW_SHOWNA);
    return HWindow;
}

LRESULT
CExecuteWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_ERASEBKGND:
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        HDC dc = (HDC)wParam;
        RECT r;
        GetClientRect(HWindow, &r);
        if (Text != NULL)
        {
            HFONT hOldFont = (HFONT)SelectObject(dc, EnvFont);
            int prevBkMode = SetBkMode(dc, TRANSPARENT);
            SetTextColor(dc, GetSysColor(COLOR_BTNTEXT));
            DrawText(dc, Text, (int)strlen(Text), &r, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
            SetBkMode(dc, prevBkMode);
            SelectObject(dc, hOldFont);
        }
        return ret;
    }
    case WM_SETCURSOR:
    {
        LRESULT ret = CWindow::WindowProc(uMsg, wParam, lParam);
        SetCursor(LoadCursor(NULL, IDC_WAIT));
        return TRUE;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
