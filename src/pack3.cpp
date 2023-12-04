// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
// Typy
// ****************************************************************************
//

// Struktura dat pro parsovaci funkce
struct SPackExpData
{
    const char* ArcName;
    const char* SrcDir;
    const char* TgtDir;
    const char* LstName;
    const char* ExtName;
    char Buffer[MAX_PATH];
    // nasledujici promenne jsou zde z duvodu, ze ziskat DOS jmeno od neexistujiciho
    // souboru nelze -> resime tak, ze vratime nahradni DOS jmeno, ktere se pozdeji
    // (po vytvoreni archivu) prejmenuje na pozadovane dlouhe jmeno
    BOOL ArcNameFilePossible; // TRUE dokud se nepouzilo nahradni DOS jmeno (musime pouzivat jedno jmeno vsude)
    BOOL DOSTmpFilePossible;  // TRUE dokud je mozne nahradit ArcName za nahradni DOS jmeno
    char* DOSTmpFile;         // nahradni jmeno za ArcName (neni NULL jen pokud DOSTmpFilePossible je TRUE)
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
// Konstanty a globalni promenne
// ****************************************************************************
//

// Timeout na otvirani konzole s pakovacem v milisekundach
// (pokud mensi nez jedna, neotvira se vubec)
LONG PackWinTimeout = 15000;

// nazvy konfiguracnich polozek v registry
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

// hlavni konfigurace pakovacu
CPackerFormatConfig PackerFormatConfig /*(FALSE)*/;
CArchiverConfig ArchiverConfig /*(FALSE)*/;

//
// Tabulky navratovych kodu jednotlivych podporovanych pakovacu
// errorkod 0 znamena vzdy succes
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

// Promenne rozlisovane v prikazove radce a aktualnim adresari pri spousteni
// externiho programu
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

// Menu v konfiguraci

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
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

/* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s polem dole...
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
// Funkce
// ****************************************************************************
//

// Funkce pro inicializaci nazvu spawn executable s plnou cestou
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
// Implementace konfiguracniho objektu - asociace pripon a pakovacu
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
    case 1: // 1.52 nemela pakovace
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

    case 2: // co pribylo po beta1
        // hack na pridani PK3 pripony k zipu
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
        // a nove pripony
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "pak", TRUE, -3, -3, TRUE);

    case 3: // co pribylo po beta2
    case 4: // beta 3 ale se starou konfiguraci (obsahuje $(SpawnName))
        // hack na pridani C## pripony k ACE
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

    case 5: // co je noveho v beta4 ?
        if ((index = AddFormat()) == -1)
            return;
        SetFormat(index, "tgz;tbz;taz;tar;gz;bz;bz2;z;rpm;cpio", FALSE, 0, -2, TRUE);
        // hack na pridani JAR pripony k zipu
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

    // zrusime stavajici pole
    int i;
    for (i = 0; i < 256; i++)
        Extensions[i].DestroyMembers();
    // a nakrmime ho znova
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

// vraci index do tabulky formatu + 1 nebo FALSE (0) kdyz neni archiv
int CPackerFormatConfig::PackIsArchive(const char* archiveName, int archiveNameLen)
{
    // j.r. makro jsem zakazal, protoze PackIsArchive se masivne vola z CFilesWindow::CommonRefresh()
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE2("PackIsArchive(%s)", archiveName);
    if (archiveName[0] == 0)
        return 0; // nenalezeno
    int idx;
    if (archiveNameLen == -1)
        idx = (int)strlen(archiveName) - 1;
    else
        idx = archiveNameLen - 1;
    CStringArray* array = &Extensions[LowerCase[archiveName[idx]]];
    // posledni znak bereme jak je
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
    // pokud je posledni znak cislo
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
    // nenalezeno
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
        if (Configuration.ConfigVersion < 44) // prevod pripon na lowercase
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
// Implementace tridy s konfiguraci - preddefinovane pakovace
//

// konstruktor
CArchiverConfig::CArchiverConfig(/*BOOL disableDefaultValues*/)
    : Archivers(20, 10)
{
    /*
  // nastavime default hodnoty, pokud to neni zakazano
  if (!disableDefaultValues)
    AddDefault(0);
*/
}

void CArchiverConfig::InitializeDefaultValues()
{
    // nastavime default hodnoty, pokud to neni zakazano
    AddDefault(0);
}

// nastavi default hodnoty
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
    case 1: // 1.52 nemela pakovace
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
        //    case 2:  // co pribylo po beta1
        //    case 3:  // co pribylo po beta2
        //    case 4:  // beta 3 ale se starou konfiguraci (obsahuje $(SpawnName))
        //    case 5:   // beta 3 ale bez taru
        //    case 6:   // co je noveho v beta4 ?
    }
}

// inicializuje konfigurace podle jine konfigurace
BOOL CArchiverConfig::Load(CArchiverConfig& src)
{
    // vycistime, co mame (pokud neco mame)
    DeleteAllArchivers();
    // a pridame co jsme dostali
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

// vytvori na konci pole novy prazdny archiver
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

// nastavi archiver na danem indexu na pozadovane hodnoty
BOOL CArchiverConfig::SetArchiver(int index, DWORD uid, const char* title, EPackExeType type, BOOL exesAreSame,
                                  const char* packerVariable, const char* unpackerVariable,
                                  const char* packerExecutable, const char* unpackerExecutable,
                                  const char* packExeFile, const char* unpackExeFile)
{
    // vycistime stare udaje, pokud nejake mame
    CArchiverConfigData* data = Archivers[index];
    data->Destroy();

    data->UID = uid;
    data->Title = DupStr(title);
    data->Type = type;
    data->ExesAreSame = exesAreSame;
    // promenna a jmeno exace jsou konstantni stringy z kodu salama, staci melka kopie
    data->PackerVariable = packerVariable;
    data->PackerExecutable = packerExecutable;
    // cesta k exaci je alokovana, udelame kopii
    data->PackExeFile = DupStr(packExeFile);
    if (!data->ExesAreSame)
    {
        // pokud je unpacker jiny, inicializujeme ho take
        data->UnpackerVariable = unpackerVariable;
        data->UnpackerExecutable = unpackerExecutable;
        data->UnpackExeFile = DupStr(unpackExeFile);
    }
    else
    {
        // pokud je pakovac stejny, mame lehci praci
        data->UnpackerVariable = NULL;
        data->UnpackerExecutable = NULL;
        data->UnpackExeFile = DupStr(packExeFile);
    }

    // jsou hodnoty smysluplne ?
    if (data->IsValid())
        return TRUE;
    else
    {
        TRACE_E("invalid data");
        Archivers.Delete(index);
        return FALSE;
    }
}

// nastavi cestu k pakovaci pro pakovac na danem indexu
// (volano z transferu pri uzavreni autokonfiguracniho dialogu)
void CArchiverConfig::SetPackerExeFile(int index, const char* filename)
{
    CArchiverConfigData* data = Archivers[index];
    if (data->PackExeFile)
        free(data->PackExeFile);
    // pokud dostaneme NULL (nenalezeno z autokonfigurace), bereme jako default nazev exace
    data->PackExeFile = DupStr(filename != NULL ? filename : data->PackerExecutable);
}

// nastavi cestu k pakovaci pro rozpakovavac na danem indexu
// (volano z transferu pri uzavreni autokonfiguracniho dialogu)
void CArchiverConfig::SetUnpackerExeFile(int index, const char* filename)
{
    CArchiverConfigData* data = Archivers[index];
    if (data->UnpackExeFile)
        free(data->UnpackExeFile);
    // pokud dostaneme NULL (nenalezeno z autokonfigurace), bereme jako default nazev exace
    data->UnpackExeFile = DupStr(filename != NULL ? filename : data->UnpackerExecutable);
}

// ulozi konfiguraci jedne polozky do registry
BOOL CArchiverConfig::Save(int index, HKEY hKey)
{
    DWORD d;
    BOOL ret = TRUE;
    // ulozi UID
    d = GetArchiverUID(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_UID, REG_DWORD, &d, sizeof(d));
    // (ulozi titulek) - diky tomu, ze se preklada, uz se neda pouzivat pro identifikaci archivatoru (nove se pouziva UID), tedy nema smysl ho ukladat
    //  if (ret) ret &= SetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, GetArchiverTitle(index), -1);
    // ulozi exac
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, GetPackerExeFile(index), -1);
    // ulozi, zda je pakovac a rozpakovavac stejny
    d = ArchiverExesAreSame(index);
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &d, sizeof(d));

    // a pokud ne, ulozi i cestu k rozpakovavaci
    if (!ArchiverExesAreSame(index))
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, GetUnpackerExeFile(index), -1);
    return ret;
}

// nacte konfiguraci jedne polozky z registry

// j.r. Prepracoval jsem nacteni konfigurace -- prohledava se deafult seznam
//      pokud se polozka z Registry najde v tomto default seznamu, pouziji se
//      jeji hodnoty. Jinak se ignoruje.
//      Stara metoda delala problemy, protoze uzivatele si rucne smazali obsah
//      klice a uz nebyla zadna cesta (z dialogu) jak obnovit puvodni seznam.
//      Navic nam padal Salamander, viz chyba CCfgPageExternalArchivers::DialogProc(0x111

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
    // nacte titulek
    if (ret && Configuration.ConfigVersion <= 64)
        ret &= GetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, title, max);
    // nacte exac k pakovani
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, packExe, max);
    // zjisti, jestli rozpakovavac je stejny
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &exesAreSame, sizeof(DWORD));
    // UID archivatoru (drive se misto nej pouzival Title, ovsem ten se nove preklada, tedy dale neni pouzitelny)
    if (ret && Configuration.ConfigVersion > 64)
        ret &= GetValue(hKey, SALAMANDER_PPC_UID, REG_DWORD, &uid, sizeof(DWORD));
    // nacte exac k rozpakovavani, pokud je jiny ne pakovac
    if (!exesAreSame)
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, unpackExe, max);

    if (ret)
    {
        int i;
        for (i = 0; i < Archivers.Count; i++)
        {
            CArchiverConfigData* arch = Archivers[i];
            // z klicu, ktere jsou kompletni a sedi jejich title s default hodnotou prevezmu cesty
            if (Configuration.ConfigVersion <= 64 && stricmp(title, arch->Title) == 0 || // Title se nove preklada, tedy dale neni pouzitelny
                Configuration.ConfigVersion > 64 && uid == arch->UID)                    // zavedli jsme tedy klasicky UID
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
  // nacte titulek
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_TITLE, REG_SZ, title, max);
  // nacte exac k pakovani
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_PACKEXE, REG_SZ, packExe, max);
  // zjisti, jestli rozpakovavac je stejny
  if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_EXESAME, REG_DWORD, &exesAreSame, sizeof(DWORD));
  // nacte exac k rozpakovavani, pokud je jiny ne pakovac
  if (!exesAreSame)
    if (ret) ret &= GetValue(hKey, SALAMANDER_PPC_UNPACKEXE, REG_SZ, unpackExe, max);

  EPackExeType type;
  const char *name, *variablePack, *variableUnpack = NULL, *exePack, *exeUnpack = NULL;

  // a ted konverze do novejsi konfigurace - chybejici informace vezmu podle defaults (stejne zadna z nich neni konfigurovatena :-) )
  if (ret)
  {
    int index;
    if ((index = AddArchiver()) == -1) return FALSE;
    // ted predpokladam, ze v konfiguraci jsou indexy s nezmenenym poradim. Pokud ne, nactu prdlajs
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
        Archivers.Delete(index);  // Aby nezustala neinicializovana struktura; padalo v Sal2.0 v SaveConfig
        return FALSE;
    }
    // zkontrolujem, jestli opravdu pridavame pakovac, o kterem si myslime ze pridavame
    if (strncmp(title, name, 10) || (exesAreSame && exeUnpack != NULL) || (!exesAreSame && exeUnpack == NULL))
    {
      TRACE_E("Inconsistency in configuration of packers.");
      Archivers.Delete(index);  // Aby nezustala neinicializovana struktura; padalo v Sal2.0 v SaveConfig
      return FALSE;
    }
    // a nastavime vsechny udaje
    ret &= SetArchiver(index, name, type, exesAreSame, variablePack, variableUnpack,
                       exePack, exeUnpack, packExe, unpackExe);
  }
  return ret;
}
*/

//
// ****************************************************************************
// Parsovaci funkce pro nahradu promennych jejich hodnotami
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
    data->DOSTmpFilePossible = FALSE; // odtedka uz jen ArcName

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
    data->DOSTmpFilePossible = FALSE; // odtedka uz jen ArcName

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
            data->ArcNameFilePossible = FALSE; // odtedka uz jen DOSTmpName
        }
        else
            data->DOSTmpFilePossible = FALSE; // odtedka uz jen ArcName
    }
    else
    {
        if (!data->DOSTmpFilePossible)
        {
            TRACE_E("Unable to return DOS nor long archive file name.");
            return NULL;
        }
    }

    if (data->DOSTmpFilePossible) // pouzijeme nahradni jmeno
    {
        if (data->DOSTmpFile[0] == 0) // je ho potreba nagenerovat
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
                        HANDLES(FindClose(find)); // toto jmeno uz s nejakou priponou existuje, hledame dale
                    else
                    {
                        sprintf(s, "%X", randNum);
                        s += strlen(s);
                        const char* ext = data->ArcName + strlen(data->ArcName);
                        //            while (--ext > data->ArcName && *ext != '\\' && *ext != '.');
                        while (--ext >= data->ArcName && *ext != '\\' && *ext != '.')
                            ;
                        //            if (ext > data->ArcName && *ext == '.' && *(ext - 1) != '\\')  // nakopirujeme priponu archivu (pouzivaji archivery u multivolumu: ARJ->A01,A02,...); ".cvspass" ve Windows je pripona ...
                        if (ext >= data->ArcName && *ext == '.') // nakopirujeme priponu archivu (pouzivaji archivery u multivolumu: ARJ->A01,A02,...)
                        {
                            int count = 4; // nakopiruju '.' plus max. 3 povolene znaky pripony formatu (8.3)
                            while (count-- && *ext < 128 && *ext != '[' && *ext != ']' &&
                                   *ext != ';' && *ext != '=' && *ext != ',' && *ext != ' ')
                            {
                                *s++ = *ext++;
                            }
                            *s = 0;
                        }
                        break; // tohle jmeno muzeme pouzit (jeste neexistuje se zadnou priponou)
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
                    DeleteFile(data->DOSTmpFile); // uz soubor nepotrebujeme (nechame ho vytvorit archiveru)
                    if (!ok)
                    {
                        TRACE_E("Error (2) in GetShortPathName() in PackExpArcDosName().");
                        return NULL;
                    }
                    strcpy(data->DOSTmpFile, buff2); // ziskali jsme nahradni jmeno
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
    // buffer pro zkraceni nazvu programu
    static char PackExpExeName[MAX_PATH];
    char buff[MAX_PATH];
    const char* exe;
    if (!unpacker)
        exe = ArchiverConfig.GetPackerExeFile(index);
    else
        exe = ArchiverConfig.GetUnpackerExeFile(index);
    // pokud neni pakovac nakonfigurovan, nemel by byt ani pouzit, ale
    // vylouceno to neni. Takze pouzijeme jmeno programu bez cesty
    if (exe == NULL)
        if (!unpacker)
            exe = ArchiverConfig.GetPackerExecutable(index);
        else
            exe = ArchiverConfig.GetUnpackerExecutable(index);
    else
    {
        // na starsich Windows neslo presmerovat vystup z DOS programu v adresari s dlouhym
        // nazvem; ted uz to nemam naladu patchovat a riskovat, ze to nepojede
        buff[0] = '\0';
        DWORD len = GetShortPathName(exe, buff, MAX_PATH);
        // kdyz uspesne zkratil, vratim kratky nazev
        if (len == strlen(buff) && len > 0)
        {
            strcpy(PackExpExeName, buff);
            return PackExpExeName;
        }
    }
    // u dlouheho nazvu zkontrolujem uvozovky...
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
// Konstanty
// ****************************************************************************
//

// ****************************************************************************
// tabulky prirazeni jednotlivych evaluacnich funkci pro jednotlive promenne
//

// tabulka prikazove radky
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

// tabulka aktualniho adresare
CSalamanderVarStrEntry PackInitDirExpArray[] =
    {
        {PACK_ARC_PATH, PackExpArcPath},
        {PACK_SRC_PATH, PackExpSrcPath},
        {PACK_TGT_PATH, PackExpTgtPath},
        {PACK_TGT_DOSPATH, PackExpTgtDosPath},
        {NULL, NULL}};

//
// ****************************************************************************
// Funkce
// ****************************************************************************
//

//
// ****************************************************************************
// Funkce pro expanzi promennych
//

//
// ****************************************************************************
// BOOL PackExpandCmdLine(const char *archiveName, const char *tgtDir, const char *lstName,
//                        const char *extName, const char *exeName, const char *varText,
//                        char *buffer, const int bufferLen, char *DOSTmpName)
//
//   Expanduje promenne v prikazove radce
//
//   RET:  TRUE pro uspech, FALSE pro chybu
//   IN:   archiveName je nazev archivu, se kterym pracujeme
//         tgtDir je nazev ciloveho adresare pro operaci nebo NULL
//         lstName je nazev souboru se seznamem souboru, nad kterymi probiha operace nebo NULL
//         extName je nazev vybalovaneho souboru nebo NULL
//         varText je prikazova radka s promennymi
//         bufferLen je velikost promenne buffer
//         DOSTmpName je NULL pokud nelze nahradit long-name nahradnim DOS jmenem, jinak ukazuje do
//           bufferu o velikosti min. MAX_PATH
//   OUT:  buffer je prikazova radka s nahrazenymi promennymi
//         DOSTmpName je jmeno nahradniho souboru (nebo prazdny retezec, pokud se nenahrazovalo)

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
//   Expanduje promenne v retezci urcujicim aktualni adresar pro spousteny program
//
//   RET:  TRUE pro uspech, FALSE pro chybu
//   IN:   archiveName je nazev archivu, se kterym pracujeme
//         srcDir je nazev zdrojoveho adresare pro operaci nebo NULL
//         tgtDir je nazev ciloveho adresare pro operaci nebo NULL
//         varText je prikazova radka s promennymi
//         bufferLen je velikost promenne buffer
//   OUT:  buffer je prikazova radka s nahrazenymi promennymi

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
// Obecne funkce
//

//
// ****************************************************************************
// BOOL EmptyErrorHandler(HWND parent, const WORD err, ...)
//
//   Prazdna chybova fce - pro spravne zpracovani chyb je treba nahradit
//   v ukazateli PackErrorHandlerPtr tuto funkci funci vlastni, ktera zpracuje
//   chybu podle potreby. Neslouzi pouze k oznameni chyb, ktere nastaly,
//   (IDS_PACKERR_...) ale take k reseni nenadalych situaci formou dotazu na
//   uzivatele (IDS_PACKQRY_...)
//
//   RET:  TRUE pro pokracovani, FALSE pro ukonceni
//   IN:   parent je parent message-boxu
//         err je cislo chyby, ktera vznikla
//         ostatni parametry jsou udaje pro upresneni chyby, zavisle na jejim kodu

BOOL EmptyErrorHandler(HWND parent, const WORD err, ...)
{
    TRACE_E("Pack Empty Error Handler: error code " << err);
    return FALSE;
}

//
// ****************************************************************************
// void PackSetErrorHandler(BOOL (*handler)(HWND parent, const WORD errNum, ...))
//
//   Nastaveni chybove funkce
//
//   RET:
//   IN:   handler je nova funkce pro zpracovani chyb

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
//   Spusti externi program uvedeny (i s parametry) v retezci cmdLine
//
//   RET: vraci TRUE pri uspechu, FALSE pri chybe
//        pri chybe vola callback funkci *PackErrorHandlerPtr
//   IN:  parent je parent pro message-boxy
//        cmdLine je prikazova radka urcena k provedeni
//        currentDir je uplne urceni aktualniho adresare pro spousteny program,
//                   nebo NULL, pokud na nem nezalezi
//        errorTable je ukazatel na tabulku navratovych kodu (pokud NULL, tabulka neni)

BOOL PackExecute(HWND parent, char* cmdLine, const char* currentDir, TPackErrorTable* const errorTable)
{
    CALL_STACK_MESSAGE3("PackExecute(, %s, %s, ,)", cmdLine, currentDir);

    // pokud jsme jeste nezjistili cestu ke spawnu, udelame to ted
    if (!InitSpawnName(parent))
        return FALSE;

    // nastavime vse potrebne pro vytvoreni procesu
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
            // pokud je hlavni okno na jinem monitoru, meli bychom tam take otevrit
            // okno vznikajici a nejlepe na default pozici (stejne jako na primaru)
            si.dwFlags |= STARTF_USEPOSITION;
            si.dwX = p.x;
            si.dwY = p.y;
        }
        si.wShowWindow = SW_MINIMIZE;
    }

    // Zjistime, co to vlastne poustime (pro oznamovani chyb)
    int i = 0, j = 0;
    char cmd[MAX_PATH];
    // preskocime uvodni whitespace
    while (cmdLine[i] != '\0' && (cmdLine[i] == ' ' || cmdLine[i] == '\t'))
        i++;
    // nacteme jmeno programu
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
    // spustime externi program
    if (!HANDLES(CreateProcess(NULL, tmpCmdLine, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS, NULL, currentDir, &si, &pi)))
    {
        DWORD err = GetLastError();
        free(tmpCmdLine);
        return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PROCESS, SpawnExe, GetErrorText(err));
    }
    free(tmpCmdLine);

    // vytvorime modalni okenko
    HWND hFocusedWnd = GetFocus();
    HWND main = parent == NULL ? MainWindow->HWindow : parent;
    CExecuteWindow tmpWindow(main, IDS_PACK_EXECUTING, ooStatic);
    tmpWindow.Create();
    HWND oldPluginMsgBoxParent = PluginMsgBoxParent;
    // muzou se volat pluginove timery (deje se pri WinSCP otevrenem v druhem panelu) -> nutne nastaveni parenta pro messageboxy
    PluginMsgBoxParent = tmpWindow.HWindow;
    EnableWindow(main, FALSE);
    // nahodime presypacky
    HCURSOR prevCrsr = SetCursor(LoadCursor(NULL, IDC_WAIT));
    // Pockame na dokonceni externiho programu
    HANDLE objects[] = {pi.hProcess};
    DWORD start = GetTickCount();
    DWORD elapsed = 0;

    DWORD ret;
    do
    {
        /*  // Petr: pumpovani jen WM_PAINT vede k zablokovani vsech ostatnich instanci Salamandera
    //       (i nove spustenych) a i dalsich softu (minimalne pri Paste) v pripade, ze pred
    //       balenim vlozime na clipboard soubor nebo adresar - dojde k tomu, ze se pri pristupu
    //       na data na clipboardu OLE snazi komunikovat s timto procesem a ten nekomunikuje,
    //       protoze pumpuje jen WM_PAINT
    // Puvodni Tomova varianta:
    ret = MsgWaitForMultipleObjects(1, objects, FALSE,
                                    PackWinTimeout <= 0 ? INFINITE : PackWinTimeout - elapsed,
                                    QS_PAINT);
*/
        ret = MsgWaitForMultipleObjects(1, objects, FALSE,
                                        PackWinTimeout <= 0 ? INFINITE : PackWinTimeout - elapsed,
                                        QS_ALLINPUT);
        if (ret == WAIT_OBJECT_0 + 1)
        {
            // pokud prisla message, obslouzime
            MSG msg;
            /*    // Puvodni Tomova varianta: (popis viz vyse)
      while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
        DispatchMessage(&msg);
*/
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
            // pokud uz vyprsel timeout, konec
            elapsed = GetTickCount() - start;
            if (PackWinTimeout > 0 && (int)elapsed >= PackWinTimeout)
            {
                ret = WAIT_TIMEOUT;
                break;
            }
        }
    } while (ret == WAIT_OBJECT_0 + 1); // cekame dokud chodi WM_PAINT

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
            /*    // Puvodni Tomova varianta: (popis viz vyse)
      ret = MsgWaitForMultipleObjects(1, objects, FALSE, INFINITE, QS_PAINT);
*/
            ret = MsgWaitForMultipleObjects(1, objects, FALSE, INFINITE, QS_ALLINPUT);
            MSG msg;
            if (ret == WAIT_OBJECT_0 + 1)
            {
                /*      // Puvodni Tomova varianta: (popis viz vyse)
        while (PeekMessage(&msg, NULL, WM_PAINT, WM_PAINT, PM_REMOVE))
          DispatchMessage(&msg);
*/
                while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        } while (ret == WAIT_OBJECT_0 + 1); // cekame dokud chodi WM_PAINT
    }

    EnableWindow(main, TRUE);
    PluginMsgBoxParent = oldPluginMsgBoxParent;
    DestroyWindow(tmpWindow.HWindow);
    // pokud je aktivni Salamander, zavolame SetFocus na zapamatovane okno (SetFocus nefunguje
    // pokud je hl. okno disablovane - po deaktivaci/aktivaci disablovaneho hl. okna aktivni panel
    // nema fokus)
    HWND hwnd = GetForegroundWindow();
    while (hwnd != NULL && hwnd != main)
        hwnd = GetParent(hwnd);
    if (hwnd == main)
        SetFocus(hFocusedWnd);
    // zrusime presypacky
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

    // a zjistime jak to dopadlo - snad vsechny vraci 0 jako success
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

    // uvolnime handly procesu
    HANDLES(CloseHandle(pi.hProcess));
    HANDLES(CloseHandle(pi.hThread));

    if (exitCode != 0)
    {
        //
        // Nejprve chyby salspawn.exe, pokud jsme ho pouzili
        //
        if (exitCode >= SPAWN_ERR_BASE)
        {
            // chyba salspawn.exe - spatne parametry nebo tak...
            if (exitCode >= SPAWN_ERR_BASE && exitCode < SPAWN_ERR_BASE * 2)
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, SPAWN_EXE_NAME, LoadStr(IDS_PACKRET_SPAWN));
            // chyba CreateProcess
            if (exitCode >= SPAWN_ERR_BASE * 2 && exitCode < SPAWN_ERR_BASE * 3)
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_PROCESS, cmd, GetErrorText(exitCode - SPAWN_ERR_BASE * 2));
            // chyba WaitForSingleObject
            if (exitCode >= SPAWN_ERR_BASE * 3 && exitCode < SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "WaitForSingleObject: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 3));
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
            // chyba GetExitCodeProcess
            if (exitCode >= SPAWN_ERR_BASE * 4)
            {
                char buffer[1000];
                strcpy(buffer, "GetExitCodeProcess: ");
                strcat(buffer, GetErrorText(exitCode - SPAWN_ERR_BASE * 4));
                return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_GENERAL, buffer);
            }
        }
        //
        // ted budou chyby externiho programu
        //
        // pokud je errorTable == NULL, pak nedelame preklad (neexistuje tabulka)
        if (!errorTable)
        {
            char buffer[1000];
            sprintf(buffer, LoadStr(IDS_PACKRET_GENERAL), exitCode);
            return (*PackErrorHandlerPtr)(parent, IDS_PACKERR_RETURN, cmd, buffer);
        }
        // najdeme v tabulce patricny text
        for (i = 0; (*errorTable)[i][0] != -1 &&
                    (*errorTable)[i][0] != (int)exitCode;
             i++)
            ;
        // nasli jsme ho ?
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
    // napocitam velikost textu => velikost okna
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
