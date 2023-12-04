// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// ****************************************************************************
//
//  UNDELETE
//
//
//      +=============+   +=========+  +---------+  +----+
//      | UNDELETE    |   | RESTORE |  | MISCSTR |  | OS |
//      +=============+   +=========+  +---------+  +----+
//             |
//      +======*======+                    +-------------+
//      | FS1         |-------------------*| DIALOGS     |
//      +=============+                    +------*------+
//             |                                  |
//      +======*=========================================+
//      | FS2                                            |
//      +================================================+
//                      |                         |
//      +---------------*-------------+    +------*------+
//      | SNAPSHOT                    |*---| STREAM      |
//      +-----------------------------+    +-------------+
//          |          |          |               |
//      +---*---+  +---*---+  +---*---+           |
//      | NTFS  |  | FAT   |  | EXFAT |           |
//      +-------+  +-------+  +-------+           |
//          |          |          |               |
//      +---*----------*----------*---------------*------+
//      | VOLUME                                         |
//      +------------------------------------------------+
//
//  Legenda: A -* B ... A uses B
//           ====== ... Salamander dependent
//           ------ ... independent
//

#include "precomp.h"

#include "undelete.rh"
#include "undelete.rh2"
#include "lang\lang.rh"

#include "library\undelete.h"
#include "miscstr.h"
#include "os.h"
#include "volume.h"
#include "snapshot.h"
#include "dialogs.h"
#include "undelete.h"
#include "restore.h"

HINSTANCE DLLInstance = NULL; // handle for SPL - language independent resources
HINSTANCE HLanguage = NULL;   // handle for SLG - language dependent resources

CPluginInterface PluginInterface;
//CPluginInterfaceForMenuExt InterfaceForMenuExt;
CPluginInterfaceForFS InterfaceForFS;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
CSalamanderDebugAbstract* SalamanderDebug = NULL;
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;
CSalamanderGUIAbstract* SalamanderGUI = NULL;

int SalamanderVersion = 0;

CLUSTER_MAP_I cluster_map;

// ****************************************************************************
//
//  CTopIndexMem
//

void CTopIndexMem::Push(const char* path, int topIndex)
{
    CALL_STACK_MESSAGE3("CTopIndexMem::Push(%s, %d)", path, topIndex);

    // detect if path continues after Path (path==Path+"\\name")
    const char* s = path + strlen(path);
    if (s > path && *(s - 1) == '\\')
        s--;
    BOOL ok;
    if (s == path)
        ok = FALSE;
    else
    {
        if (s > path && *s == '\\')
            s--;
        while (s > path && *s != '\\')
            s--;

        int l = (int)strlen(Path);
        if (l > 0 && Path[l - 1] == '\\')
            l--;
        ok = s - path == l && SalamanderGeneral->StrNICmp(path, Path, l) == 0;
    }

    if (ok) // it continues -> store next top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // we need to release first top-index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // it doesn't continue -> first top-index v raw
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(const char* path, int& topIndex)
{
    CALL_STACK_MESSAGE3("CTopIndexMem::FindAndPop(%s, %d)", path, topIndex);

    // detect if path match to Path (path==Path)
    int l1 = (int)strlen(path);
    if (l1 > 0 && path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(Path);
    if (l2 > 0 && Path[l2 - 1] == '\\')
        l2--;
    if (l1 == l2 && SalamanderGeneral->StrNICmp(path, Path, l1) == 0)
    {
        if (TopIndexesCount > 0)
        {
            char* s = Path + strlen(Path);
            if (s > Path && *(s - 1) == '\\')
                s--;
            if (s > Path && *s == '\\')
                s--;
            while (s > Path && *s != '\\')
                s--;
            *s = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // we don't have this item anymore (it wasn't stored or was released due to low memory)
        {
            Clear();
            return FALSE;
        }
    }
    else // another path -> release memory, it is long jump
    {
        Clear();
        return FALSE;
    }
}

// ****************************************************************************
//
//   InitIconOverlays
//

void InitIconOverlays()
{
    // 48x48 az od XP (brzy bude obsolete, pobezime jen na XP+, pak zahodit)
    // ve skutecnosti jsou velke ikonky podporeny uz davno, lze je nahodit
    // Desktop/Properties/???/Large Icons; pozor, nebude pak existovat system image list
    // pro ikonky 32x32; navic bychom meli ze systemu vytahnout realne velikosti ikonek
    // zatim na to kasleme a 48x48 povolime az od XP, kde jsou bezne dostupne
    int iconSizes[3] = {16, 32, 48};
    if (!SalIsWindowsVersionOrGreater(5, 1, 0)) // neni WindowsXPAndLater: neni XP and later
        iconSizes[2] = 32;

    HICON iconOverlays[3];
    for (int i = 0; i < 3; i++)
    {
        iconOverlays[i] = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_DELETED),
                                           IMAGE_ICON, iconSizes[i], iconSizes[i],
                                           SalamanderGeneral->GetIconLRFlags());
    }

    // POZN.: pri chybe loadu ikon SetPluginIconOverlays() selze, ale platne ikony z iconOverlays[] uvolni
    SalamanderGeneral->SetPluginIconOverlays(1, iconOverlays);
}

// ****************************************************************************
//
//   DllMain and SalamanderPluginEntry
//

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
    {
        DLLInstance = hinstDLL;

        INITCOMMONCONTROLSEX initCtrls;
        initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
        initCtrls.dwICC = ICC_USEREX_CLASSES;
        if (!InitCommonControlsEx(&initCtrls))
        {
            MessageBox(NULL, "InitCommonControlsEx failed!", "Error", MB_OK | MB_ICONERROR);
            return FALSE; // DLL won't start
        }
    }

    return TRUE; // DLL can be loaded
}

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();
    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // works with current and newer Salamander version - check it out
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // deny old versions
        MessageBox(salamander->GetParentWindow(), REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "Undelete" /* DO NOT TRANSLATE! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "Undelete" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // get Salamander interfaces
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();
    SalamanderGUI = salamander->GetSalamanderGUI();

    // set help file name
    SalamanderGeneral->SetHelpFileName("undelete.chm");

    // init
    if (!OS<char>::OS_InitLibraryData())
        return NULL;
    if (!InitFS())
    {
        OS<char>::OS_ReleaseLibraryData();
        return NULL;
    }
    InitializeWinLib("Undelete" /* DO NOT TRANSLATE! */, DLLInstance);
    SetupWinLibHelp(HTMLHelpCallback);

    InitIconOverlays();

    // set basic information about plugin
    salamander->SetBasicPluginData(String<char>::LoadStr(IDS_UNDELETE),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION |
                                       FUNCTION_FILESYSTEM,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   String<char>::LoadStr(IDS_DESCRIPTION),
                                   "UNDELETE" /* DO NOT TRANSLATE! */, NULL, "del");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // get our FS-name (it could be different than "del", Salamander could change it)
    SalamanderGeneral->GetPluginFSName(AssignedFSName, 0);

    return &PluginInterface;
}

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

// ****************************************************************************
//
//   CPluginInterface
//

void WINAPI CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                String<char>::LoadStr(IDS_UNDELETE),
                String<char>::LoadStr(IDS_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, String<char>::LoadStr(IDS_ABOUTTITLE), MB_OK | MB_ICONINFORMATION);
}

BOOL WINAPI CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    ReleaseFS();
    OS<char>::OS_ReleaseLibraryData();
    ReleaseWinLib(DLLInstance);
    /*if (ret && InterfaceForFS.GetActiveFSCount() != 0)
  {
    TRACE_E("Some FS interfaces were not closed (count=" << InterfaceForFS.GetActiveFSCount() << ")");
  }*/
    // fixme
    return TRUE;
}

void WINAPI CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    /*
  HBITMAP hBmp = (HBITMAP) HANDLES(LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_FS),
                                   IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR));
  salamander->SetBitmapWithIcons(hBmp);
  HANDLES(DeleteObject(hBmp));
  */
    // switch to icons with alpha channel support
    CGUIIconListAbstract* iconList = SalamanderGUI->CreateIconList();
    iconList->Create(16, 16, 1);
    //  HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS), IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
    HICON hIcon = OS<char>::OS_GetEmptyRecycleBinIcon(FALSE);
    iconList->ReplaceIcon(0, hIcon);
    DestroyIcon(hIcon);
    salamander->SetIconListForGUI(iconList); // will be destroyed by Salamander

    salamander->SetChangeDriveMenuItem(String<char>::LoadStr(IDS_UNDELETEINCHDRVMENU), 0);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_UNDELETECMD
	{MNTT_IT, IDS_RESTORECMD
	{MNTT_PE, 0
};
*/

    // for better discoverability put plugin also to Plugins menu
    salamander->AddMenuItem(-1, String<char>::LoadStr(IDS_UNDELETECMD), SALHOTKEY('U', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
                            CMD_UNDELETE, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    salamander->AddMenuItem(-1, String<char>::LoadStr(IDS_RESTORECMD), 0,
                            CMD_RESTORE_ENCRYPTED, FALSE, MENU_EVENT_FILE_FOCUSED | MENU_EVENT_DIR_FOCUSED | MENU_EVENT_FILES_SELECTED | MENU_EVENT_DIRS_SELECTED, MENU_EVENT_DISK | MENU_EVENT_TARGET_DISK, MENU_SKILLLEVEL_ALL);
}

void WINAPI CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    delete ((CPluginFSDataInterface*)pluginData);
}

CPluginInterfaceForFSAbstract* WINAPI CPluginInterface::GetInterfaceForFS()
{
    return &InterfaceForFS;
}

CPluginInterfaceForMenuExtAbstract* WINAPI CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

void WINAPI CPluginInterface::Event(int event, DWORD param)
{
    if (event == PLUGINEVENT_COLORSCHANGED)
    {
        InitIconOverlays();

        // DFSImageList != NULL required, entry-point would fail otherwise
        // COLORREF bkColor = SalamanderGeneral->GetCurrentColor(SALCOL_ITEM_BK_NORMAL);
        // if (ImageList_GetBkColor(DFSImageList) != bkColor)
        //   ImageList_SetBkColor(DFSImageList, bkColor);
    }
}

// ****************************************************************************
//
// CPluginInterfaceForMenuExt
//

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* SalOp,
                                                 HWND parent, int id, DWORD eventMask)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForMenuExt::ExecuteMenuItem( , , %ld, %X)", id, eventMask);

    switch (id)
    {
    case CMD_UNDELETE:
    {
        InterfaceForFS.ExecuteChangeDriveMenuItem(PANEL_SOURCE);
        return FALSE;
    }

    case CMD_RESTORE_ENCRYPTED:
    {
        CRestoreDialog dlg(parent);
        if (dlg.Execute() == IDCANCEL)
            return FALSE;
        return RestoreEncryptedFiles(dlg.TargetPath, parent);
        // SalamanderGeneral->SalMessageBox(parent, "Not implemented yet.", "Restore", MB_OK | MB_ICONINFORMATION);
        // return FALSE;
    }

    default:
    {
        TRACE_E("Invalid menu item ID");
    }
    }
    return FALSE;
}

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case 1:
        helpID = IDH_UNDELETE;
        break;
    case 2:
        helpID = IDH_RESTOREENCRFILES;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

// ****************************************************************************
//
// Config
//

BOOL ConfigAlwaysReuseScanInfo;
BOOL ConfigScanVacantClusters;
BOOL ConfigShowExistingFiles;
BOOL ConfigShowZeroFiles;
BOOL ConfigShowEmptyDirs;
BOOL ConfigShowMetafiles;
BOOL ConfigEstimateDamage;
char ConfigTempPath[MAX_PATH];
BOOL ConfigDontShowEncryptedWarning;
BOOL ConfigDontShowSamePartitionWarning;
int ConditionFixedWidth = 0; // column Condition (FS): LO/HI-WORD: left/right panel: FixedWidth
int ConditionWidth = 0;      // column Condition (FS): LO/HI-WORD: left/right panel: Width

static const char* KEY_ALWAYSREUSE = "Always Reuse Scan Info";
static const char* KEY_SCANVACENT = "Scan Vacant Clusters";
static const char* KEY_SHOWEXISTING = "Show Existing Files";
static const char* KEY_SHOWZEROFILES = "Show Zero Files";
static const char* KEY_SHOWEMPTYDIRS = "Show Empty Dirs";
static const char* KEY_SHOWMETAFILES = "Show Metafiles";
static const char* KEY_ESTIMATEDAMAGE = "Estimate Damage";
static const char* KEY_TEMPPATH = "Alternate Temp Path";
static const char* KEY_DONTSHOWENCRYPTED = "Dont Show Encrypted Warning";
static const char* KEY_DONTSHOWSAMEPARTITION = "Dont Show Same Partition Warning";
static const char* KEY_CONDITIONFIXEDWIDTH = "Condition Fixed Width";
static const char* KEY_CONDITIONWIDTH = "Condition Width";

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    // default values
    ConfigAlwaysReuseScanInfo = FALSE;
    ConfigScanVacantClusters = TRUE;
    ConfigShowExistingFiles = FALSE;
    ConfigShowZeroFiles = TRUE;
    ConfigShowEmptyDirs = TRUE;
    ConfigShowMetafiles = FALSE;
    ConfigEstimateDamage = TRUE;
    ConfigTempPath[0] = 0;
    ConfigDontShowEncryptedWarning = FALSE;
    ConfigDontShowSamePartitionWarning = FALSE;
    ConditionFixedWidth = 0;
    ConditionWidth = 0;

    if (regKey != NULL) // load from the Registry
    {
        registry->GetValue(regKey, KEY_ALWAYSREUSE, REG_DWORD, &ConfigAlwaysReuseScanInfo, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SCANVACENT, REG_DWORD, &ConfigScanVacantClusters, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SHOWEXISTING, REG_DWORD, &ConfigShowExistingFiles, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SHOWZEROFILES, REG_DWORD, &ConfigShowZeroFiles, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SHOWEMPTYDIRS, REG_DWORD, &ConfigShowEmptyDirs, sizeof(DWORD));
        registry->GetValue(regKey, KEY_SHOWMETAFILES, REG_DWORD, &ConfigShowMetafiles, sizeof(DWORD));
        registry->GetValue(regKey, KEY_ESTIMATEDAMAGE, REG_DWORD, &ConfigEstimateDamage, sizeof(DWORD));
        registry->GetValue(regKey, KEY_TEMPPATH, REG_SZ, &ConfigTempPath, MAX_PATH);
        registry->GetValue(regKey, KEY_DONTSHOWENCRYPTED, REG_DWORD, &ConfigDontShowEncryptedWarning, MAX_PATH);
        registry->GetValue(regKey, KEY_DONTSHOWSAMEPARTITION, REG_DWORD, &ConfigDontShowSamePartitionWarning, MAX_PATH);
        registry->GetValue(regKey, KEY_CONDITIONFIXEDWIDTH, REG_DWORD, &ConditionFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, KEY_CONDITIONWIDTH, REG_DWORD, &ConditionWidth, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    registry->SetValue(regKey, KEY_ALWAYSREUSE, REG_DWORD, &ConfigAlwaysReuseScanInfo, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SCANVACENT, REG_DWORD, &ConfigScanVacantClusters, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SHOWEXISTING, REG_DWORD, &ConfigShowExistingFiles, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SHOWZEROFILES, REG_DWORD, &ConfigShowZeroFiles, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SHOWEMPTYDIRS, REG_DWORD, &ConfigShowEmptyDirs, sizeof(DWORD));
    registry->SetValue(regKey, KEY_SHOWMETAFILES, REG_DWORD, &ConfigShowMetafiles, sizeof(DWORD));
    registry->SetValue(regKey, KEY_ESTIMATEDAMAGE, REG_DWORD, &ConfigEstimateDamage, sizeof(DWORD));
    registry->SetValue(regKey, KEY_TEMPPATH, REG_SZ, &ConfigTempPath, -1);
    registry->SetValue(regKey, KEY_DONTSHOWENCRYPTED, REG_DWORD, &ConfigDontShowEncryptedWarning, sizeof(DWORD));
    registry->SetValue(regKey, KEY_DONTSHOWSAMEPARTITION, REG_DWORD, &ConfigDontShowSamePartitionWarning, sizeof(DWORD));
    registry->SetValue(regKey, KEY_CONDITIONFIXEDWIDTH, REG_DWORD, &ConditionFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, KEY_CONDITIONWIDTH, REG_DWORD, &ConditionWidth, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    CConfigDialog dlg(parent);
    dlg.Execute();
}
