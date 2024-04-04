// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "edtlbwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "plugins.h"
#include "fileswnd.h"
#include "shellib.h"
#include "editwnd.h"
#include "codetbl.h"
#include "execute.h"
#include "viewer.h"
#include "find.h"
#include "gui.h"

//****************************************************************************
//
// CHighlightMasksItem
//

CHighlightMasksItem::CHighlightMasksItem()
{
    CALL_STACK_MESSAGE1("CHighlightMasksItem()");
    Masks = NULL;
    Attr = 0;
    ValidAttr = 0;
    NormalFg = RGBF(0, 0, 0, SCF_DEFAULT);
    NormalBk = RGBF(0, 0, 0, SCF_DEFAULT);
    FocusedFg = RGBF(0, 0, 0, SCF_DEFAULT);
    FocusedBk = RGBF(0, 0, 0, SCF_DEFAULT);
    SelectedFg = RGBF(0, 0, 0, SCF_DEFAULT);
    SelectedBk = RGBF(0, 0, 0, SCF_DEFAULT);
    FocSelFg = RGBF(0, 0, 0, SCF_DEFAULT);
    FocSelBk = RGBF(0, 0, 0, SCF_DEFAULT);
    HighlightFg = RGBF(0, 0, 0, SCF_DEFAULT);
    HighlightBk = RGBF(0, 0, 0, SCF_DEFAULT);
    Set("");
}

CHighlightMasksItem::CHighlightMasksItem(CHighlightMasksItem& item)
{
    CALL_STACK_MESSAGE1("CHighlightMasksItem(&)");
    Masks = NULL;
    Attr = item.Attr;
    ValidAttr = item.ValidAttr;
    NormalFg = item.NormalFg;
    NormalBk = item.NormalBk;
    FocusedFg = item.FocusedFg;
    FocusedBk = item.FocusedBk;
    SelectedFg = item.SelectedFg;
    SelectedBk = item.SelectedBk;
    FocSelFg = item.FocSelFg;
    FocSelBk = item.FocSelBk;
    HighlightFg = item.HighlightFg;
    HighlightBk = item.HighlightBk;
    Set(item.Masks->GetMasksString());
}

CHighlightMasksItem::~CHighlightMasksItem()
{
    if (Masks != NULL)
        delete Masks;
}

BOOL CHighlightMasksItem::Set(const char* masks)
{
    if (Masks == NULL)
        Masks = new CMaskGroup;
    if (Masks == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    Masks->SetMasksString(masks);
    return TRUE;
}

BOOL CHighlightMasksItem::IsGood()
{
    return Masks != NULL;
}

BOOL CHighlightMasks::Load(CHighlightMasks& source)
{
    CALL_STACK_MESSAGE1("CHighlightMasks::Load()");
    CHighlightMasksItem* item;
    DestroyMembers();
    int i;
    for (i = 0; i < source.Count; i++)
    {
        item = new CHighlightMasksItem(*source[i]);
        if (!item->IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        Add(item);
        if (!IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    return TRUE;
}

//****************************************************************************
//
// ValidatePathIsNotEmpty
//
// if the path is empty, returns FALSE, otherwise TRUE (the path contains "something")

BOOL ValidatePathIsNotEmpty(HWND hParent, const char* path)
{
    const char* iterator = path;
    BOOL empty = TRUE;
    while (*iterator != 0)
    {
        if (*iterator != ' ')
        {
            empty = FALSE;
            break;
        }
        else
            iterator++;
    }

    // an empty string is not allowed
    if (empty)
    {
        SalMessageBox(hParent, LoadStr(IDS_THEPATHISINVALID), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    /* puvodne se funkce jmenovala ValidateCommandFile a overovala existenci souboru, ale odesli jsme od toho (mozna problemy na siti?)
  if (testFileExist)
  {
    // zkusim na soubor sahnout
    if (!FileExists(myPath))
    {
      // soubor jsem nenasel - nemusi existovat nebo neni dostupna cesta
      // neacham to usera forcenout
      char buff[1000];
      sprintf(buff, LoadStr(IDS_THECOMMANDISINVALID), myPath, itemName);
      int ret = SalMessageBox(hParent, buff, LoadStr(IDS_ERRORTITLE),
                              MB_YESNO | MB_ICONQUESTION);
      if (ret == IDYES)
        return FALSE;    // user chce zmenit polozku
    }
  }
*/
    return TRUE;
}

//
// ****************************************************************************
// CLoadSaveToRegistryMutex
//

CLoadSaveToRegistryMutex::CLoadSaveToRegistryMutex()
{
    Mutex = NULL;
    DebugCheck = 0;
}

CLoadSaveToRegistryMutex::~CLoadSaveToRegistryMutex()
{
    if (DebugCheck != 0)
        TRACE_E("CLoadSaveToRegistryMutex(): fatal error: incorrect use of mutex for Load/Save to Registry: " << DebugCheck);
    if (Mutex != NULL)
        HANDLES(CloseHandle(Mutex));
    Mutex = NULL;
}

extern const char* LOADSAVE_REGISTRY_MUTEX_NAME;

void CLoadSaveToRegistryMutex::Init()
{
    // 2.52b1: we add support for FastUserSwitching/Terminal Services
    // In this version, the mutex was in the local namespace under the name SalamanderLoadSaveToRegistryMutex.
    // Now we want to ensure interoperability across all sessions, so we will put it into the Global namespace.
    // Additionally, we insert SID into the name (from W2K onwards), so that the mutexes of Salamander running under
    // with different users -- they work with their tree in the Registry in the same way, synchronization is not needed there.
    // We could also insert the Salamander version into the mutex name (each version has its own tree in the Registry).
    // However, because the new versions of Salamander are capable of deleting old abandoned configurations, we will not do it.
    LPTSTR sid = NULL;
    if (!GetStringSid(&sid))
        sid = NULL;

    char buff[1000];
    if (sid == NULL)
    {
        // error in obtaining SID -- we will proceed in emergency mode
        _snprintf_s(buff, _TRUNCATE, "%s", LOADSAVE_REGISTRY_MUTEX_NAME);
    }
    else
    {
        _snprintf_s(buff, _TRUNCATE, "Global\\%s_%s", LOADSAVE_REGISTRY_MUTEX_NAME, sid);
        LocalFree(sid);
    }

    PSID psidEveryone;
    PACL paclNewDacl;
    SECURITY_ATTRIBUTES sa;
    SECURITY_DESCRIPTOR sd;
    SECURITY_ATTRIBUTES* saPtr = CreateAccessableSecurityAttributes(&sa, &sd, SYNCHRONIZE /*| MUTEX_MODIFY_STATE*/, &psidEveryone, &paclNewDacl);

    Mutex = HANDLES_Q(CreateMutex(saPtr, FALSE, buff));
    if (Mutex == NULL)
    {
        Mutex = HANDLES_Q(OpenMutex(SYNCHRONIZE, FALSE, buff));
        if (Mutex == NULL)
        {
            DWORD err = GetLastError();
            TRACE_I("CLoadSaveToRegistryMutex::Init(): Unable to create/open mutex for Load/Save to Registry synchronization! Error: " << GetErrorText(err));
        }
    }

    if (psidEveryone != NULL)
        FreeSid(psidEveryone);
    if (paclNewDacl != NULL)
        LocalFree(paclNewDacl);

    DebugCheck = 0;
}

void CLoadSaveToRegistryMutex::Enter()
{
    DebugCheck++;
    if (Mutex != NULL)
    {
        if (WaitForSingleObject(Mutex, INFINITE) == WAIT_FAILED)
            TRACE_E("CLoadSaveToRegistryMutex::Enter(): WaitForSingleObject() failed!");
    }
    else
        TRACE_E("CLoadSaveToRegistryMutex::Enter(): the Mutex==NULL! Not initialized?");
}

void CLoadSaveToRegistryMutex::Leave()
{
    DebugCheck--;
    if (Mutex != NULL)
    {
        if (!ReleaseMutex(Mutex))
            TRACE_E("CLoadSaveToRegistryMutex::Enter(): ReleaseMutex() failed!");
    }
    else
        TRACE_E("CLoadSaveToRegistryMutex::Leave(): the Mutex==NULL! Not initialized?");
}

//
// ****************************************************************************
// CConfiguration
//

const char* DefTopToolBar = "11,14,15,70,-1,40,56,-1,30,31,32,-1,41,42,18,27,55,33,-1,3,34,35,20,19,-1,43,46,49";
const char* DefMiddleToolBar = "2,3,17,21,22,23,72,26,24,25,27,55,28,29,30,31,32,33";
const char* DefLeftToolBar = "36";
const char* DefRightToolBar = "51";

CConfiguration::CConfiguration()
{
    ConfigVersion = 0;
    IncludeDirs = FALSE;
    AutoSave = TRUE;
    CloseShell = FALSE;
    ShowGrepErrors = FALSE; // all other finders (FAR/WinCmd/PowerDesk/Windows Find) do not show errors
    FindFullRowSelect = FALSE;
    MinBeepWhenDone = TRUE;
    UseRecycleBin = 1;
    FileNameFormat = 4; // as on the disk
    SizeFormat = SIZE_FORMAT_BYTES;
    RecycleMasks.SetMasksString("*.txt;*.doc");
    LastFocusedPage = 0;
    ConfigurationHeight = 0; // the logic of the dialogue does not allow a smaller dialogue than its largest retained page
    ViewersAndEditorsExpanded = 0;
    PackersAndUnpackersExpanded = 0;
    ClearReadOnly = TRUE;
    PrimaryContextMenu = TRUE;
    NotHiddenSystemFiles = FALSE;
    AlwaysOnTop = FALSE;
    //  FastDirectoryMove = TRUE;
    SortUsesLocale = TRUE;
    SortDetectNumbers = TRUE;
    SortNewerOnTop = FALSE; // Implicitly sort as Explorer under XP, newer items at the bottom
    SortDirsByName = FALSE; // because people don't write bug reports to us like they do to Ghisler
    SortDirsByExt = FALSE;  // Directories do not have extensions, it is an option for companies/people using an old type of directory sorting
    SaveHistory = TRUE;
    SaveWorkDirs = FALSE; // we are implicitly saving space in the registry, the list is large
    EnableCmdLineHistory = TRUE;
    SaveCmdLineHistory = TRUE;
    //  LantasticCheck = FALSE;
    UseSalOpen = FALSE;
    NetwareFastDirMove = FALSE; // We choose a slower but 100% functional mode, fine-tuners can switch it
    UseAsyncCopyAlg = TRUE;
    ReloadEnvVariables = TRUE;
    QuickRenameSelectAll = FALSE;
    EditNewSelectAll = TRUE;
    ShiftForHotPaths = TRUE;
    OnlyOneInstance = FALSE;
    ForceOnlyOneInstance = FALSE;
    StatusArea = FALSE;
    SingleClick = FALSE;
    TopToolBarVisible = TRUE;
    PluginsBarVisible = FALSE;
    MiddleToolBarVisible = FALSE;
    BottomToolBarVisible = TRUE;
    UserMenuToolBarVisible = FALSE;
    HotPathsBarVisible = FALSE;
    DriveBarVisible = TRUE;
    DriveBar2Visible = FALSE;
    IconSpacingVert = 43;
    IconSpacingHorz = 43;
    TileSpacingVert = 8;
    ThumbnailSpacingHorz = 19; // 29 is under WinXP
    ThumbnailSize = THUMBNAIL_SIZE_DEFAULT;

    // options for Compare Directories
    CompareByTime = TRUE;
    CompareBySize = TRUE;
    CompareByContent = FALSE;
    CompareByAttr = FALSE;
    CompareSubdirs = FALSE;
    CompareSubdirsAttr = FALSE;
    CompareOnePanelDirs = FALSE;
    CompareMoreOptions = FALSE;
    CompareIgnoreFiles = FALSE;
    CompareIgnoreDirs = FALSE;
    int errPos;
    CompareIgnoreFilesMasks.SetMasksString("");
    CompareIgnoreFilesMasks.PrepareMasks(errPos);
    CompareIgnoreDirsMasks.SetMasksString("");
    CompareIgnoreDirsMasks.PrepareMasks(errPos);

    // Confirmation
    CnfrmFileDirDel = TRUE;
    CnfrmNEDirDel = FALSE;
    CnfrmFileOver = TRUE;
    CnfrmDirOver = FALSE;
    CnfrmSHFileDel = TRUE;
    CnfrmSHDirDel = FALSE;
    CnfrmSHFileOver = TRUE;
    CnfrmNTFSPress = TRUE;
    CnfrmNTFSCrypt = TRUE;
    CnfrmDragDrop = FALSE;
    CnfrmCloseArchive = TRUE;
    CnfrmCreateDir = TRUE;
    CnfrmCloseFind = TRUE;
    CnfrmStopFind = TRUE;
    CnfrmCreatePath = TRUE;
    CnfrmAlwaysOnTop = TRUE;
    CnfrmOnSalClose = FALSE;
    CnfrmSendEmail = TRUE;
    CnfrmAddToArchive = TRUE;
    CnfrmChangeDirTC = TRUE;
    CnfrmShowNamesToCompare = TRUE;
    CnfrmDSTShiftsIgnored = TRUE;
    CnfrmDSTShiftsOccured = TRUE;
    CnfrmCopyMoveOptionsNS = TRUE;
    //  PanelTooltip = TRUE;
    KeepPluginsSorted = TRUE;
    ShowSLGIncomplete = TRUE;

    LastUsedSpeedLimit = 1024 * 1024; // default 1 MB/s

    QuickSearchEnterAlt = FALSE;

    // to display items in the panel
    FullRowSelect = FALSE;
    FullRowHighlight = TRUE;
    UseIconTincture = TRUE;
    ShowPanelCaption = TRUE;
    ShowPanelZoom = TRUE;
    strcpy(InfoLineContent, "$(FileName): $(FileSize), $(FileDate), $(FileTime), $(FileAttributes), $(FileDOSName)");

    HotPathAutoConfig = TRUE;

    DrvSpecFloppyMon = TRUE;
    DrvSpecFloppySimple = TRUE;
    DrvSpecRemovableMon = TRUE;
    DrvSpecRemovableSimple = FALSE; // 2.5b11: Floppy has its own category, so we can afford to read icons
    DrvSpecFixedMon = TRUE;
    DrvSpecFixedSimple = FALSE;
    DrvSpecRemoteMon = TRUE;
    DrvSpecRemoteSimple = FALSE;
    DrvSpecRemoteDoNotRefreshOnAct = FALSE;
    DrvSpecCDROMMon = TRUE;
    DrvSpecCDROMSimple = FALSE;

    IfPathIsInaccessibleGoToIsMyDocs = TRUE;
    IfPathIsInaccessibleGoTo[0] = 0;

    SpaceSelCalcSpace = TRUE;

    UseTimeResolution = TRUE;
    TimeResolution = 2;
    IgnoreDSTShifts = FALSE;

    UseDragDropMinTime = TRUE;
    DragDropMinTime = 500;

    strcpy(TopToolBar, DefTopToolBar);
    strcpy(MiddleToolBar, DefMiddleToolBar);
    strcpy(LeftToolBar, DefLeftToolBar);
    strcpy(RightToolBar, DefRightToolBar);

    SkillLevel = SKILL_LEVEL_ADVANCED; // we will try to present the maximum possibilities

    int i;
    for (i = 0; i < SELECT_HISTORY_SIZE; i++)
        SelectHistory[i] = NULL;
    for (i = 0; i < COPY_HISTORY_SIZE; i++)
        CopyHistory[i] = NULL;
    for (i = 0; i < EDIT_HISTORY_SIZE; i++)
        EditHistory[i] = NULL;
    for (i = 0; i < CHANGEDIR_HISTORY_SIZE; i++)
        ChangeDirHistory[i] = NULL;
    for (i = 0; i < FILELIST_HISTORY_SIZE; i++)
        FileListHistory[i] = NULL;
    for (i = 0; i < CREATEDIR_HISTORY_SIZE; i++)
        CreateDirHistory[i] = NULL;
    for (i = 0; i < QUICKRENAME_HISTORY_SIZE; i++)
        QuickRenameHistory[i] = NULL;
    for (i = 0; i < EDITNEW_HISTORY_SIZE; i++)
        EditNewHistory[i] = NULL;
    for (i = 0; i < CONVERT_HISTORY_SIZE; i++)
        ConvertHistory[i] = NULL;
    for (i = 0; i < FILTER_HISTORY_SIZE; i++)
        FilterHistory[i] = NULL;

    FileListHistory[0] = DupStr("$(FileName)$(CRLF)"); // default for MakeFileList

    FileListName[0] = 0;
    FileListAppend = FALSE;
    FileListDestination = 0;
    // Internal Viewer:
    CopyFindText = TRUE;
    EOL_CRLF = TRUE;
    EOL_CR = TRUE;
    EOL_LF = TRUE;
    EOL_NULL = TRUE;
    DefViewMode = 0; // Auto-Select
    TabSize = 8;
    SavePosition = FALSE;
    TextModeMasks.SetMasksString("*.txt;*.602;*.xml");
    TextModeMasks.PrepareMasks(errPos);
    HexModeMasks.SetMasksString("");
    HexModeMasks.PrepareMasks(errPos);
    WindowPlacement.length = 0;
    WrapText = TRUE;
    CodePageAutoSelect = TRUE;
    DefaultConvert[0] = 0;
    AutoCopySelection = FALSE;
    GoToOffsetIsHex = TRUE;

    // Change drive
    ChangeDriveShowMyDoc = TRUE;
    ChangeDriveShowAnother = TRUE;
    ChangeDriveShowNet = TRUE;
    ChangeDriveCloudStorage = TRUE;

    MenuIndex = 0;
    MenuBreak = TRUE;
    MenuWidth = 1; // dummy

    TopToolbarIndex = 1;
    TopToolbarBreak = TRUE;
    TopToolbarWidth = 1; // dummy

    PluginsBarIndex = 2;
    PluginsBarBreak = TRUE;
    PluginsBarWidth = 1; // dummy

    UserMenuToolbarIndex = 3;
    UserMenuToolbarBreak = TRUE;
    UserMenuToolbarWidth = 1; // dummy
    UserMenuToolbarLabels = 1;

    HotPathsBarIndex = 4;
    HotPathsBarBreak = TRUE;
    HotPathsBarWidth = 1; // dummy

    DriveBarIndex = 5;
    DriveBarBreak = TRUE;
    DriveBarWidth = 1; // dummy

    GripsVisible = TRUE;

    // Packers / Unpackers
    UseAnotherPanelForPack = FALSE;   // like WinZip - let's speed it up
    UseAnotherPanelForUnpack = FALSE; // let's rather debone it :-)
    UseSubdirNameByArchiveForUnpack = FALSE;
    UseSimpleIconsInArchives = FALSE;

    UseEditNewFileDefault = FALSE;
    EditNewFileDefault[0] = 0;

    // Tip of the Day
    //  ShowTipOfTheDay = TRUE;
    //  LastTipOfTheDay = 0;

    LastPluginVer = 0;
    LastPluginVerOP = 0;

    ConfigWasImported = FALSE;

    // Find dialog
    SearchFileContent = FALSE;
    FindDialogWindowPlacement.length = 0; // currently invalid
    // Width of the columns in the Find dialog
    FindColNameWidth = -1; // set according to the window

    // Language
    LoadedSLGName[0] = 0;
    SLGName[0] = 0;
    DoNotDispCantLoadPluginSLG = FALSE;
    DoNotDispCantLoadPluginSLG2 = FALSE;
    UseAsAltSLGInOtherPlugins = FALSE;
    AltPluginSLGName[0] = 0;

    // variable ConversionTable is not loaded from configuration
    strcpy(ConversionTable, "*");

    TitleBarShowPath = TRUE;
    TitleBarMode = TITLE_BAR_MODE_DIRECTORY; // according to Explorer
    UseTitleBarPrefix = FALSE;
    strcpy(TitleBarPrefix, "ADMIN");
    UseTitleBarPrefixForced = FALSE;
    TitleBarPrefixForced[0] = 0;

    MainWindowIconIndex = 0; // default icon
    MainWindowIconIndexForced = -1;

    ClickQuickRename = TRUE;

    VisibleDrives = DRIVES_MASK; // implicitly displaying all disks
    SeparatedDrives = 0;         // we do not implicitly insert any separators, we do not know where

    ShowSplashScreen = TRUE;

    EnableCustomIconOverlays = TRUE;
    DisabledCustomIconOverlays = NULL;

#ifndef _WIN64
    AddX86OnlyPlugins = FALSE; // FIXME_X64_WINSCP
#endif                         // _WIN64
}

CConfiguration::~CConfiguration()
{
    ClearHistory();
    ClearListOfDisabledCustomIconOverlays();
}

void CConfiguration::ClearHistory()
{
    int i;
    for (i = 0; i < SELECT_HISTORY_SIZE; i++)
    {
        if (SelectHistory[i] != NULL)
        {
            free(SelectHistory[i]);
            SelectHistory[i] = NULL;
        }
    }

    for (i = 0; i < COPY_HISTORY_SIZE; i++)
    {
        if (CopyHistory[i] != NULL)
        {
            free(CopyHistory[i]);
            CopyHistory[i] = NULL;
        }
    }

    for (i = 0; i < EDIT_HISTORY_SIZE; i++)
    {
        if (EditHistory[i] != NULL)
        {
            free(EditHistory[i]);
            EditHistory[i] = NULL;
        }
    }

    for (i = 0; i < CHANGEDIR_HISTORY_SIZE; i++)
    {
        if (ChangeDirHistory[i] != NULL)
        {
            free(ChangeDirHistory[i]);
            ChangeDirHistory[i] = NULL;
        }
    }

    for (i = 0; i < FILELIST_HISTORY_SIZE; i++)
    {
        if (FileListHistory[i] != NULL)
        {
            free(FileListHistory[i]);
            FileListHistory[i] = NULL;
        }
    }

    for (i = 0; i < CREATEDIR_HISTORY_SIZE; i++)
    {
        if (CreateDirHistory[i] != NULL)
        {
            free(CreateDirHistory[i]);
            CreateDirHistory[i] = NULL;
        }
    }

    for (i = 0; i < QUICKRENAME_HISTORY_SIZE; i++)
    {
        if (QuickRenameHistory[i] != NULL)
        {
            free(QuickRenameHistory[i]);
            QuickRenameHistory[i] = NULL;
        }
    }

    for (i = 0; i < EDITNEW_HISTORY_SIZE; i++)
    {
        if (EditNewHistory[i] != NULL)
        {
            free(EditNewHistory[i]);
            EditNewHistory[i] = NULL;
        }
    }

    for (i = 0; i < CONVERT_HISTORY_SIZE; i++)
    {
        if (ConvertHistory[i] != NULL)
        {
            free(ConvertHistory[i]);
            ConvertHistory[i] = NULL;
        }
    }

    for (i = 0; i < FILTER_HISTORY_SIZE; i++)
    {
        if (FilterHistory[i] != NULL)
        {
            free(FilterHistory[i]);
            FilterHistory[i] = NULL;
        }
    }
}

BOOL CConfiguration::PrepareRecycleMasks(int& errorPos)
{
    return RecycleMasks.PrepareMasks(errorPos);
}

BOOL CConfiguration::AgreeRecycleMasks(const char* fileName, const char* fileExt)
{
    return RecycleMasks.AgreeMasks(fileName, fileExt);
}

int CConfiguration::GetMainWindowIconIndex()
{
    int index = MainWindowIconIndexForced != -1 ? MainWindowIconIndexForced : MainWindowIconIndex;
    if (index >= 0 && index < MAINWINDOWICONS_COUNT)
        return index;
    else
        return 0; // default
}

//
// ****************************************************************************
// CConfigurationDlg
//

CConfigurationDlg::CConfigurationDlg(HWND parent, CUserMenuItems* userMenuItems,
                                     int mode, int param)
    : CTreePropDialog(parent, HLanguage, LoadStr(IDS_CONFIGURATION),
                      mode == 0 ? Configuration.LastFocusedPage : mode == 1 ? 14
                                                              : mode == 2   ? 13
                                                              : mode == 3   ? 21
                                                              : mode == 4   ? 12
                                                              : mode == 5   ? 1
                                                                            : 11 /* mode == 6*/,
                      PSH_NOAPPLYNOW | PSH_HASHELP,
                      &Configuration.LastFocusedPage,
                      &Configuration.ConfigurationHeight),
      PageView(mode == 4 ? param : -1), //-1 = active panel
      PageUserMenu(userMenuItems),
      PageHotPath(mode == 1 ? TRUE : FALSE, param),
      PageDrives(mode == 6 ? TRUE : FALSE),
      Page14(TRUE)
{
    HOldPluginMsgBoxParent = NULL;
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    // When changing the order of pages, it is necessary to change the constructor
    // mode == 0 ? Configuration.LastFocusedPage : 4
    // in 1.6b2 it crashed on me
    // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
    /*00*/ Add(&PageGeneral);       // General
    /*01*/ Add(&PagePanels);        // Panels
    /*02*/ Add(&PageHistory);       // History
    /*03*/ Add(&PageSystem);        // System
    /*04*/ Add(&PageRegional);      // Regional
    /*05*/ Add(&PageMainWindow);    // MainWindow
    /*06*/ Add(&PageAppear);        // Appearance
    /*07*/ Add(&PageColors);        // Colors
    /*08*/ Add(&PageKeyboard);      // Keyboard
    /*09*/ Add(&PageConfirmations); // Confirmations
    /*10*/ Add(&PageChangeDrive);   // Change Drive Menu
    /*11*/ Add(&PageDrives);        // Drives
    /*12*/ Add(&PageView);          // Views
    /*13*/ Add(&PageUserMenu);      // User Menu
    /*14*/ Add(&PageHotPath);       // Hot Paths
    /*15*/ Add(&PageSecurity);      // Security
    /*16*/ Add(&PageIconOvrls);     // Icon Overlays
    /*17*/ Add(&PageViewEdit, NULL, &Configuration.ViewersAndEditorsExpanded);
    /*18*/ Add(&Page13, &PageViewEdit);
    /*19*/ Add(&Page14, &PageViewEdit);
    /*20*/ Add(&Page15, &PageViewEdit);
    /*21*/ Add(&PageViewer, &PageViewEdit);
    /*22*/ Add(&PagePP, NULL, &Configuration.PackersAndUnpackersExpanded);
    /*23*/ Add(&PageP4, &PagePP);
    /*24*/ Add(&PageP3, &PagePP);
    /*25*/ Add(&PageP1, &PagePP);
    /*26*/ Add(&PageP2, &PagePP);
    /*27*/ //  Add(&PageShellExtensions);
           // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
           // When changing the order of pages, it is necessary to change the constructor
           // mode == 0 ? Configuration.LastFocusedPage : 4
           // in 1.6b2 it crashed on me
           // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
}

void CConfigurationDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // The ColorsChanged() method calls a method of the plug-in (the method is called when colors are changed
        // PLUGINEVENT_COLORSCHANGED) -> necessary setting of the parent for their message boxes
        HOldPluginMsgBoxParent = PluginMsgBoxParent;
        PluginMsgBoxParent = Dialog.HWindow;
        MultiMonCenterWindow(Dialog.HWindow, Parent, TRUE);
        break;
    }

    case WM_DESTROY:
    {
        if (GetKeyState(VK_ESCAPE) & 0x8000) // measure to prevent interruption of the listing in the panel after each ESC
            WaitForESCReleaseBeforeTestingESC = TRUE;

        PluginMsgBoxParent = HOldPluginMsgBoxParent;
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        int i;
        for (i = 0; i < Count; i++)
        {
            HWND w = At(i)->HWindow;
            if (w != NULL)
                PostMessage(w, WM_SYSCOLORCHANGE, 0, 0);
        }
        break;
    }
    }
}

//
// ****************************************************************************
// CCfgPageGeneral
//

CCfgPageGeneral::CCfgPageGeneral()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_GENERAL, IDD_CFGPAGE_GENERAL, PSP_USETITLE, NULL)
{
}

void CCfgPageGeneral::Validate(CTransferInfo& ti)
{
    BOOL useTimeRes;
    ti.CheckBox(IDC_TIMERESOLUTION, useTimeRes);
    if (useTimeRes)
    {
        int timerRes;
        ti.EditLine(IDE_TIMERESOLUTION, timerRes);
        if (timerRes < 0 || timerRes > 3600)
        {
            SalMessageBox(HWindow, LoadStr(IDS_BADTIMERESOLUTION), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDE_TIMERESOLUTION);
        }
    }
}

void CCfgPageGeneral::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_AUTOSAVE, Configuration.AutoSave);
    ti.CheckBox(IDC_CLOSESHELL, Configuration.CloseShell);
    ti.CheckBox(IDC_CLEARREADONLY, Configuration.ClearReadOnly);
    //  ti.CheckBox(IDC_FASTDIRMOVE, Configuration.FastDirectoryMove);
    ti.CheckBox(IDC_TIMERESOLUTION, Configuration.UseTimeResolution);
    ti.CheckBox(IDC_IGNOREDSTSHIFTS, Configuration.IgnoreDSTShifts);
    ti.EditLine(IDE_TIMERESOLUTION, Configuration.TimeResolution);
    //  ti.CheckBox(IDC_LANTASTICCHECK, Configuration.LantasticCheck);
    ti.CheckBox(IDC_ONLYONEINSTANCE, Configuration.OnlyOneInstance);
    ti.CheckBox(IDC_MINBEEPWHENDONE, Configuration.MinBeepWhenDone);
    ti.CheckBox(IDC_USESALOPEN, Configuration.UseSalOpen);
    ti.CheckBox(IDC_QUICKRENAME_SELALL, Configuration.QuickRenameSelectAll);
    ti.CheckBox(IDC_EDITNEW_SELALL, Configuration.EditNewSelectAll);
    ti.CheckBox(IDC_NETWAREFASTDIRMOVE, Configuration.NetwareFastDirMove);
    int dummy = 0;
    ti.CheckBox(IDC_ASYNCCOPYALG, Windows7AndLater ? Configuration.UseAsyncCopyAlg : dummy);
    int oldReloadEnvVariables = Configuration.ReloadEnvVariables;
    ti.CheckBox(IDC_RELOADENVVARS, Configuration.ReloadEnvVariables);
    if (ti.Type == ttDataFromWindow && Configuration.ReloadEnvVariables && oldReloadEnvVariables != Configuration.ReloadEnvVariables)
    {
        InitEnvironmentVariablesDifferences();
    }

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CCfgPageGeneral::EnableControls()
{
    BOOL useTimeRes = IsDlgButtonChecked(HWindow, IDC_TIMERESOLUTION);
    EnableWindow(GetDlgItem(HWindow, IDE_TIMERESOLUTION), useTimeRes);
    EnableWindow(GetDlgItem(HWindow, IDC_ASYNCCOPYALG), Windows7AndLater);
}

INT_PTR
CCfgPageGeneral::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageRegional
//

CCfgPageRegional::CCfgPageRegional()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_REGIONAL, IDD_CFGPAGE_REGIONAL, PSP_USETITLE, NULL)
{
    lstrcpy(SLGName, Configuration.SLGName);
    lstrcpy(DirName, Configuration.ConversionTable);
}

void CCfgPageRegional::LoadControls()
{
    CLanguage language;
    if (language.Init(SLGName, NULL))
    {
        char buff[200];
        language.GetLanguageName(buff, 200);
        SetDlgItemText(HWindow, IDE_LANGUAGE, buff);
        language.Free();
    }
}

void CCfgPageRegional::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        LoadControls();
    }
    else
    {
        if (stricmp(Configuration.SLGName, SLGName) != 0)
        {
            SalMessageBox(HWindow, LoadStr(IDS_LANGUAGE_CHANGE), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
            lstrcpy(Configuration.SLGName, SLGName);
            Configuration.ShowSLGIncomplete = TRUE; // If the language is not complete, we will display a message at startup (recruiting translators)
        }
        // if the table has changed and the old ones are already loaded, Salama needs to be restarted
        if (stricmp(Configuration.ConversionTable, DirName) != 0)
        {
            lstrcpy(Configuration.ConversionTable, DirName);
            if (CodeTables.IsLoaded())
            {
                SalMessageBox(HWindow, LoadStr(IDS_CONVERSION_CHANGE), LoadStr(IDS_INFOTITLE),
                              MB_OK | MB_ICONINFORMATION);
            }
        }
    }
}

INT_PTR
CCfgPageRegional::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (IsSLGIncomplete[0] != 0)
        {
            new CStaticText(HWindow, IDC_CFGREG_INCOMPLETE_TITLE, STF_BOLD);
            SetDlgItemText(HWindow, IDC_CFGREG_INCOMPLETE_TITLE, LoadStr(IDS_SLGINCOMPLETE_TITLE));
            SetDlgItemText(HWindow, IDC_CFGREG_INCOMPLETE_TEXT, LoadStr(IDS_SLGINCOMPLETE_TEXT));
            SetDlgItemText(HWindow, IDC_CFGREG_INCOMPLETE_URL, IsSLGIncomplete);
            CHyperLink* hl = new CHyperLink(HWindow, IDC_CFGREG_INCOMPLETE_URL);
            hl->SetActionOpen(IsSLGIncomplete);
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_LANGUAGE:
        {
            CLanguageSelectorDialog dlg(HWindow, SLGName, NULL);
            dlg.Initialize();
            if (dlg.Execute() == IDOK)
            {
                LoadControls();
                PostMessage(GetParent(), WM_NEXTDLGCTL, (WPARAM)GetDlgItem(GetParent(), 5 /* _TPD_IDC_OK*/), TRUE);
            }
            return 0;
        }

        case IDB_CONVERSION:
        {
            CConversionTablesDialog dlg(HWindow, DirName);
            dlg.Execute();
            return 0;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageView
//

// only serves to suppress quick search in listview
class CMyListView : public CWindow
{
public:
    CMyListView(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_CHAR)
        {
            PostMessage(GetParent(HWindow), WM_USER_CHAR, wParam, lParam);
            return 0;
        }

        if (uMsg == WM_LBUTTONDBLCLK)
        {
            int index = ListView_GetNextItem(HWindow, -1, LVNI_SELECTED);
            ListView_EditLabel(HWindow, index);
            return 0;
        }

        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

CCfgPageView::CCfgPageView(int index)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_VIEWS, IDD_CFGPAGE_VIEWS, PSP_USETITLE, NULL)
{
    Dirty = FALSE;
    Header = NULL;
    HListView = NULL;
    Header2 = NULL;
    HListView2 = NULL;
    DisableNotification = FALSE;
    LabelEdit = FALSE;
    if (index == -1)
        index = MainWindow->GetActivePanel()->GetViewTemplateIndex();
    SelectIndex = index;
}

BOOL CCfgPageView::IsDirty()
{
    return Dirty;
}

void CCfgPageView::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        Config.Load(MainWindow->ViewTemplates);

        DisableNotification = TRUE;
        char buff[20];
        int i;
        for (i = 0; i < VIEW_TEMPLATES_COUNT; i++)
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT | LVIF_STATE;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.state = 0;
            lvi.pszText = Config.Items[i].Name;
            ListView_InsertItem(HListView, &lvi);

            switch (Config.Items[i].Mode)
            {
            case VIEW_MODE_TREE:
                lstrcpy(buff, LoadStr(IDS_TREE_VIEW_NAME));
                break;
            case VIEW_MODE_BRIEF:
                lstrcpy(buff, LoadStr(IDS_BRIEF_VIEW_NAME));
                break;
            case VIEW_MODE_DETAILED:
                lstrcpy(buff, LoadStr(IDS_DETAILED_VIEW_NAME));
                break;
            case VIEW_MODE_ICONS:
                lstrcpy(buff, LoadStr(IDS_ICONS_VIEW_NAME));
                break;
            case VIEW_MODE_THUMBNAILS:
                lstrcpy(buff, LoadStr(IDS_THUMBNAILS_VIEW_NAME));
                break;
            case VIEW_MODE_TILES:
                lstrcpy(buff, LoadStr(IDS_TILES_VIEW_NAME));
                break;
            }
            ListView_SetItemText(HListView, i, 1, buff);

            sprintf(buff, "Alt+%d", i < VIEW_TEMPLATES_COUNT - 1 ? i + 1 : 0);
            ListView_SetItemText(HListView, i, 2, buff);
        }
        // set column widths
        ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
        int w = ListView_GetColumnWidth(HListView, 0);
        w += 30;
        ListView_SetColumnWidth(HListView, 0, w);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(HListView, 2, LVSCW_AUTOSIZE_USEHEADER);

        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, SelectIndex, state, state);
        DisableNotification = FALSE;
        LoadControls();
        EnableHeader();
    }
    else
    {
        MainWindow->ViewTemplates.Load(Config);
    }
}

void CCfgPageView::Validate(CTransferInfo& ti)
{
}

const int CFGP2ItemsCount = 8 /*9*/;
const int CFGP2Flags[CFGP2ItemsCount] = {0, VIEW_SHOW_EXTENSION, VIEW_SHOW_DOSNAME, VIEW_SHOW_SIZE, VIEW_SHOW_TYPE, VIEW_SHOW_DATE, VIEW_SHOW_TIME, VIEW_SHOW_ATTRIBUTES /*, VIEW_SHOW_DESCRIPTION*/};
const int CFGP2ResID[CFGP2ItemsCount] = {IDS_COLUMN_CFG_NAME, IDS_COLUMN_CFG_EXT, IDS_COLUMN_CFG_DOSNAME, IDS_COLUMN_CFG_SIZE, IDS_COLUMN_CFG_TYPE, IDS_COLUMN_CFG_DATE, IDS_COLUMN_CFG_TIME, IDS_COLUMN_CFG_ATTR /*, IDS_COLUMN_CFG_DESC*/};

void CCfgPageView::LoadControls()
{
    DisableNotification = TRUE;
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);

    BOOL checked[CFGP2ItemsCount];

    BOOL empty = TRUE;
    if (index >= 2 && index != 3 && index != 4 && index != 5) // tree and brief are disabled checkboxes
    {
        checked[0] = TRUE;
        int i;
        for (i = 1; i < CFGP2ItemsCount; i++)
            checked[i] = (Config.Items[index].Flags & CFGP2Flags[i]) ? TRUE : FALSE;
        empty = FALSE;
    }
    DWORD count = ListView_GetItemCount(HListView2);
    if (empty && count > 0)
        ListView_DeleteAllItems(HListView2);
    if (!empty && count == 0)
    {
        int i;
        for (i = 0; i < CFGP2ItemsCount; i++)
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT | LVIF_STATE;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.state = 0;
            lvi.pszText = LoadStr(CFGP2ResID[i]);
            ListView_InsertItem(HListView2, &lvi);
        }
        ListView_SetItemState(HListView2, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
        ListView_SetColumnWidth(HListView2, 0, LVSCW_AUTOSIZE_USEHEADER);
    }
    int index2 = -1;
    if (!empty)
    {
        int i;
        for (i = 0; i < CFGP2ItemsCount; i++)
        {
            UINT state = INDEXTOSTATEIMAGEMASK((checked[i] ? 2 : 1));
            ListView_SetItemState(HListView2, i, state, LVIS_STATEIMAGEMASK);
        }
        CTransferInfo ti(HWindow, ttDataToWindow);
        int tmp = Config.Items[index].LeftSmartMode;
        ti.CheckBox(IDC_VIEW_LEFT_SMARTMODE, tmp);
        tmp = Config.Items[index].RightSmartMode;
        ti.CheckBox(IDC_VIEW_RIGHT_SMARTMODE, tmp);
        index2 = ListView_GetNextItem(HListView2, -1, LVNI_SELECTED);
        if (index2 != -1)
        {
            tmp = Config.Items[index].Columns[index2].LeftFixedWidth;
            ti.CheckBox(IDC_VIEW_LEFT_FIXED, tmp);
            tmp = Config.Items[index].Columns[index2].RightFixedWidth;
            ti.CheckBox(IDC_VIEW_RIGHT_FIXED, tmp);
            tmp = Config.Items[index].Columns[index2].LeftWidth;
            ti.EditLine(IDC_VIEW_LEFT_WIDTH, tmp);
            tmp = Config.Items[index].Columns[index2].RightWidth;
            ti.EditLine(IDC_VIEW_RIGHT_WIDTH, tmp);
        }
    }

    EnableControls();
    DisableNotification = FALSE;
}

void CCfgPageView::StoreControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index >= 2)
    {
        DWORD flags = 0;
        int i;
        for (i = 1; i < CFGP2ItemsCount; i++)
        {
            DWORD state = ListView_GetItemState(HListView2, i, LVIS_STATEIMAGEMASK);
            if (state == INDEXTOSTATEIMAGEMASK(2))
                flags |= CFGP2Flags[i];
        }
        Config.Items[index].Flags = flags;
        Dirty = TRUE;
    }
}

void CCfgPageView::EnableControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    BOOL enable = TRUE;
    if (index == -1 || index < 2 || index == 3 || index == 4 || index == 5 || strlen(Config.Items[index].Name) == 0)
        enable = FALSE;

    int index2 = ListView_GetNextItem(HListView2, -1, LVNI_SELECTED);
    BOOL supportFixedWidth = (index2 != -1 && enable);

    EnableWindow(HListView2, enable);
    CTransferInfo ti(HWindow, ttDataToWindow);
    int tmp = 0;
    if (!supportFixedWidth)
    {
        ti.CheckBox(IDC_VIEW_LEFT_FIXED, tmp);
        ti.CheckBox(IDC_VIEW_RIGHT_FIXED, tmp);
        char buff[] = "";
        ti.EditLine(IDC_VIEW_LEFT_WIDTH, buff, 1);
        ti.EditLine(IDC_VIEW_RIGHT_WIDTH, buff, 1);
    }
    if (!enable)
    {
        ti.CheckBox(IDC_VIEW_LEFT_SMARTMODE, tmp);
        ti.CheckBox(IDC_VIEW_RIGHT_SMARTMODE, tmp);
    }
    BOOL enableLeftEdit = supportFixedWidth && IsDlgButtonChecked(HWindow, IDC_VIEW_LEFT_FIXED) == BST_CHECKED;
    BOOL enableRightEdit = supportFixedWidth && IsDlgButtonChecked(HWindow, IDC_VIEW_RIGHT_FIXED) == BST_CHECKED;

    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TEXT4), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TEXT6), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_LEFT_FIXED), supportFixedWidth);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_RIGHT_FIXED), supportFixedWidth);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_LEFT_WIDTH), enableLeftEdit);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_RIGHT_WIDTH), enableRightEdit);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TEXT2), supportFixedWidth);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TEXT5), supportFixedWidth);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TEXT7), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_LEFT_SMARTMODE), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_RIGHT_SMARTMODE), enable);
}

DWORD
CCfgPageView::GetEnabledFunctions()
{
    DWORD mask = 0;
    if (!LabelEdit)
    {
        int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
        if (index > 6)
        {
            mask |= TLBHDRMASK_MODIFY;
            if (lstrlen(Config.Items[index].Name) > 0)
            {
                mask |= TLBHDRMASK_DELETE;
                if (index > 7)
                    mask |= TLBHDRMASK_UP;
                if (index < VIEW_TEMPLATES_COUNT - 1)
                    mask |= TLBHDRMASK_DOWN;
            }
        }
    }
    return mask;
}

void CCfgPageView::EnableHeader()
{
    Header->EnableToolbar(GetEnabledFunctions());
}

void CCfgPageView::OnModify()
{
    if ((GetEnabledFunctions() & TLBHDRMASK_MODIFY) == 0)
        return;
    Dirty = TRUE;
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != -1)
        PostMessage(HListView, LVM_EDITLABEL, index, 0);
}

void CCfgPageView::OnDelete()
{
    if ((GetEnabledFunctions() & TLBHDRMASK_DELETE) == 0)
        return;
    Dirty = TRUE;
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != -1)
    {
        Config.Items[index].Name[0] = 0;
        char buff[] = "";
        ListView_SetItemText(HListView, index, 0, buff);
        LoadControls();
    }
    EnableHeader();
}

void CCfgPageView::OnMove(BOOL up)
{
    CALL_STACK_MESSAGE2("CCfgPageView::OnMove(%d)", up);
    DWORD mask = GetEnabledFunctions();
    if (up && (mask & TLBHDRMASK_UP) == 0 ||
        !up && (mask & TLBHDRMASK_DOWN) == 0)
        return;
    Dirty = TRUE;
    DisableNotification = TRUE;
    int index1 = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    int index2 = index1;
    if (index1 == -1)
        return;
    if (up && index1 > 0)
        index2 = index1 - 1;
    if (!up && index1 < VIEW_TEMPLATES_COUNT - 1)
        index2 = index1 + 1;
    if (index2 != index1)
    {
        ListView_SetItemText(HListView, index1, 0, Config.Items[index2].Name);
        ListView_SetItemText(HListView, index2, 0, Config.Items[index1].Name);
        DWORD state1 = ListView_GetItemState(HListView, index1, LVIS_STATEIMAGEMASK);
        DWORD state2 = ListView_GetItemState(HListView, index2, LVIS_STATEIMAGEMASK);
        ListView_SetItemState(HListView, index1, state2, LVIS_STATEIMAGEMASK);
        state1 |= LVIS_FOCUSED | LVIS_SELECTED;
        ListView_SetItemState(HListView, index2, state1, LVIS_STATEIMAGEMASK | LVIS_FOCUSED | LVIS_SELECTED);
        Config.SwapItems(index1, index2);
        LoadControls();
    }
    DisableNotification = FALSE;
    EnableHeader();
}

INT_PTR
CCfgPageView::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageView::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CMyListView* listView = new CMyListView(HWindow, IDC_VIEW_LIST);
        HListView = listView->HWindow;
        HListView2 = GetDlgItem(HWindow, IDC_VIEW_LIST2);
        Header = new CToolbarHeader(HWindow, IDC_VIEWLIST_HEADER, HListView,
                                    TLBHDRMASK_MODIFY | TLBHDRMASK_DELETE |
                                        TLBHDRMASK_UP | TLBHDRMASK_DOWN);
        if (Header == NULL)
            TRACE_E(LOW_MEMORY);

        Header2 = new CToolbarHeader(HWindow, IDC_VIEWLIST_HEADER2, HListView2, 0);
        if (Header2 == NULL)
            TRACE_E(LOW_MEMORY);

        DWORD exFlags = LVS_EX_FULLROWSELECT /*| LVS_EX_CHECKBOXES*/;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        origFlags = ListView_GetExtendedListViewStyle(HListView2);
        ListView_SetExtendedListViewStyle(HListView2, origFlags | exFlags); // 4.71

        // get the size of the listview
        RECT r;
        GetClientRect(HListView, &r);

        // I will pour the columns Name, Mode, and HotKey into the listview
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_HOTPATH_NAME);
        lvc.cx = 1; // dummy
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_VIEW_MODE);
        lvc.iSubItem = 1;
        ListView_InsertColumn(HListView, 1, &lvc);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_HOTPATH_HOTKEY);
        lvc.iSubItem = 2;
        ListView_InsertColumn(HListView, 2, &lvc);
        ListView_SetColumnWidth(HListView, 2, LVSCW_AUTOSIZE_USEHEADER);

        // populate the column Name into the listview with columns
        GetClientRect(HListView2, &r);
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_COLUMN_NAME);
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        ListView_InsertColumn(HListView2, 0, &lvc);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(2, IDC_VIEW_LIST, IDC_VIEW_LIST2);

        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        ListView_SetBkColor(HListView2, GetSysColor(COLOR_WINDOW));
        break;
    }

    case WM_USER_CHAR:
    {
        int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
        if (wParam != ' ' && !LabelEdit)
        {
            ListView_EditLabel(HListView, index);
            HWND hEdit = ListView_GetEditControl(HListView);
            SendMessage(hEdit, WM_CHAR, wParam, 0);
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        if (DisableNotification)
            break;

        if (wParam == IDC_VIEW_LIST2)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_DBLCLK:
            {
                LVHITTESTINFO ht;
                DWORD pos = GetMessagePos();
                ht.pt.x = GET_X_LPARAM(pos);
                ht.pt.y = GET_Y_LPARAM(pos);
                ScreenToClient(HListView2, &ht.pt);
                ListView_HitTest(HListView2, &ht);
                int index = ListView_GetNextItem(HListView2, -1, LVNI_SELECTED);
                if (index != -1 && ht.iItem == index && ht.flags != LVHT_ONITEMICON)
                {
                    UINT state = ListView_GetItemState(HListView2, index, LVIS_STATEIMAGEMASK);
                    if (state == INDEXTOSTATEIMAGEMASK(2))
                        state = INDEXTOSTATEIMAGEMASK(1);
                    else
                        state = INDEXTOSTATEIMAGEMASK(2);
                    ListView_SetItemState(HListView2, index, state, LVIS_STATEIMAGEMASK);
                }
                break;
            }

            case LVN_ITEMCHANGING:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                // disagree :-) we will beep when trying to turn off the Name column
                if (nmhi->iItem == 0 && (nmhi->uOldState & 0xF000) != (nmhi->uNewState & 0xF000))
                {
                    MessageBeep(MB_ICONASTERISK);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                }
                else
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);

                return TRUE;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if ((nmhi->uOldState & 0xF000) != (nmhi->uNewState & 0xF000))
                {
                    StoreControls(); // When clicking on the checkbox, I save the data
                    EnableControls();
                }
                else if (!(nmhi->uOldState & LVIS_SELECTED) && nmhi->uNewState & LVIS_SELECTED)
                    LoadControls(); // When a column is changed, new data will be loaded into the grids and edit lines
                break;
            }
            }
            break;
        }

        if (wParam == IDC_VIEW_LIST)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if (nmhk->wVKey == VK_F2)
                {
                    OnModify();
                }
                if (nmhk->wVKey == VK_DELETE)
                {
                    OnDelete();
                }
                if ((GetKeyState(VK_MENU) & 0x8000) &&
                    (nmhk->wVKey == VK_UP || nmhk->wVKey == VK_DOWN))
                {
                    OnMove(nmhk->wVKey == VK_UP);
                }
                if (!LabelEdit && nmhk->wVKey == VK_INSERT)
                    ListView_EditLabel(HListView, index);
                break;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if (!(nmhi->uOldState & LVIS_SELECTED) && nmhi->uNewState & LVIS_SELECTED)
                {
                    LoadControls();
                }

                //            if (nmhi->uOldState & 0x2000 && nmhi->uNewState & 0x1000 ||
                //                nmhi->uOldState & 0x1000 && nmhi->uNewState  & 0x2000)
                //            {
                //              BOOL checked = nmhi->uNewState & 0x2000;
                //              Config->SetVisible(nmhi->iItem, checked);
                //            }

                EnableHeader();
                break;
            }

            case LVN_BEGINLABELEDIT:
            {
                if (GetEnabledFunctions() & TLBHDRMASK_MODIFY)
                {
                    LabelEdit = TRUE;
                    EnableHeader();
                }
                else
                {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }

            case LVN_ENDLABELEDIT:
            {
                NMLVDISPINFO* nmhd = (NMLVDISPINFO*)nmh;
                LabelEdit = FALSE;
                EnableHeader();
                if (nmhd->item.pszText != NULL)
                {
                    char name[VIEW_NAME_MAX];
                    lstrcpyn(name, nmhd->item.pszText, VIEW_NAME_MAX);
                    Config.CleanName(name);
                    int index = nmhd->item.iItem;
                    if (lstrlen(Config.Items[index].Name) == 0)
                        Config.Items[index].Flags = 0;
                    lstrcpy(Config.Items[index].Name, name);
                    LoadControls();
                    ListView_SetItemText(HListView, index, 0, name);
                    Dirty = TRUE;
                    break;
                }
                break;
            }
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (!DisableNotification && HIWORD(wParam) == BN_CLICKED)
        {
            if (LOWORD(wParam) == IDC_VIEW_LEFT_FIXED || LOWORD(wParam) == IDC_VIEW_RIGHT_FIXED ||
                LOWORD(wParam) == IDC_VIEW_LEFT_SMARTMODE || LOWORD(wParam) == IDC_VIEW_RIGHT_SMARTMODE)
            {
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                int index2 = ListView_GetNextItem(HListView2, -1, LVNI_SELECTED);
                if (index >= 2 && index2 != -1)
                {
                    BOOL checked = IsDlgButtonChecked(HWindow, LOWORD(wParam)) == BST_CHECKED;
                    switch (LOWORD(wParam))
                    {
                    case IDC_VIEW_LEFT_FIXED:
                        Config.Items[index].Columns[index2].LeftFixedWidth = checked ? 1 : 0;
                        break;
                    case IDC_VIEW_RIGHT_FIXED:
                        Config.Items[index].Columns[index2].RightFixedWidth = checked ? 1 : 0;
                        break;
                    case IDC_VIEW_LEFT_SMARTMODE:
                        Config.Items[index].LeftSmartMode = checked;
                        break;
                    case IDC_VIEW_RIGHT_SMARTMODE:
                        Config.Items[index].RightSmartMode = checked;
                        break;
                    }
                    Dirty = TRUE;
                }
                EnableControls();
            }
            break;
        }

        if (!DisableNotification && HIWORD(wParam) == EN_CHANGE)
        {
            if (LOWORD(wParam) == IDC_VIEW_LEFT_WIDTH || LOWORD(wParam) == IDC_VIEW_RIGHT_WIDTH)
            {
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                int index2 = ListView_GetNextItem(HListView2, -1, LVNI_SELECTED);
                if (index >= 2 && index2 != -1)
                {
                    CTransferInfo ti(HWindow, ttDataFromWindow);
                    int tmp;
                    if (LOWORD(wParam) == IDC_VIEW_LEFT_WIDTH)
                    {
                        ti.EditLine(IDC_VIEW_LEFT_WIDTH, tmp);
                        Config.Items[index].Columns[index2].LeftWidth = tmp;
                    }
                    else
                    {
                        ti.EditLine(IDC_VIEW_RIGHT_WIDTH, tmp);
                        Config.Items[index].Columns[index2].RightWidth = tmp;
                    }
                    Dirty = TRUE;
                }
            }
            break;
        }

        if (LOWORD(wParam) == IDC_VIEWLIST_HEADER)
        {
            if (GetFocus() != HListView)
                SetFocus(HListView);
            switch (HIWORD(wParam))
            {
            case TLBHDR_MODIFY:
                OnModify();
                break;
            case TLBHDR_DELETE:
                OnDelete();
                break;
            case TLBHDR_UP:
                OnMove(TRUE);
                break;
            case TLBHDR_DOWN:
                OnMove(FALSE);
                break;
            }
        }
        break;
    }
    }

    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageViewer
//

CCfgPageViewer::CCfgPageViewer()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_VIEWER, IDD_CFGPAGE_VIEWER, PSP_USETITLE, NULL)
{
    HFont = NULL;
    LocalUseCustomViewerFont = UseCustomViewerFont;
    memcpy(&LocalViewerLogFont, &ViewerLogFont, sizeof(LocalViewerLogFont));
    NormalText = NULL;
    SelectedText = NULL;
}

CCfgPageViewer::~CCfgPageViewer()
{
    if (HFont != NULL)
        HANDLES(DeleteObject(HFont));
}

void CCfgPageViewer::Validate(CTransferInfo& ti)
{
    int dummy;
    ti.EditLine(IDC_TABSIZE, dummy);
    if (dummy <= 0 || dummy > 30)
    {
        SalMessageBox(HWindow, LoadStr(IDS_BADTABSIZE), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDC_TABSIZE);
    }

    char buf[MAX_PATH];
    if (ti.IsGood())
    {
        lstrcpyn(buf, Configuration.TextModeMasks.GetMasksString(), MAX_PATH); // Backup TextModeMasks
        // we provide MasksString, there is a range check, nothing to worry about
        ti.EditLine(IDC_VIEW_INTEXT, Configuration.TextModeMasks.GetWritableMasksString(), MAX_PATH);
        int errorPos;
        if (!Configuration.TextModeMasks.PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(HWindow, IDC_VIEW_INTEXT));
            SendMessage(GetDlgItem(HWindow, IDC_VIEW_INTEXT), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDC_VIEW_INTEXT);
        }
        Configuration.TextModeMasks.SetMasksString(buf); // Restore TextModeMasks
        Configuration.TextModeMasks.PrepareMasks(errorPos);
    }

    if (ti.IsGood())
    {
        lstrcpyn(buf, Configuration.HexModeMasks.GetMasksString(), MAX_PATH); // Backup HexModeMasks
        // provide MasksString, there is a range check, nothing to worry about
        ti.EditLine(IDC_VIEW_INHEX, (char*)Configuration.HexModeMasks.GetWritableMasksString(), MAX_PATH);
        int errorPos;
        if (!Configuration.HexModeMasks.PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(HWindow, IDC_VIEW_INHEX));
            SendMessage(GetDlgItem(HWindow, IDC_VIEW_INHEX), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDC_VIEW_INHEX);
        }
        Configuration.HexModeMasks.SetMasksString(buf); // Restoring HexModeMasks
        Configuration.HexModeMasks.PrepareMasks(errorPos);
    }
}

void CCfgPageViewer::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageViewer::Transfer()");
    if (ti.Type == ttDataToWindow)
        LoadControls();
    ti.CheckBox(IDC_COPYFINDTEXT, Configuration.CopyFindText);
    ti.CheckBox(IDC_CRLFEOL, Configuration.EOL_CRLF);
    ti.CheckBox(IDC_CREOL, Configuration.EOL_CR);
    ti.CheckBox(IDC_LFEOL, Configuration.EOL_LF);
    ti.CheckBox(IDC_NULLEOL, Configuration.EOL_NULL);
    ti.EditLine(IDC_TABSIZE, Configuration.TabSize);
    ti.RadioButton(IDC_SAVEONCLOSE, 1, Configuration.SavePosition);
    ti.RadioButton(IDC_SETBYMAINWINDOW, 0, Configuration.SavePosition);
    if (Configuration.SavePosition)
        Configuration.WindowPlacement.length = 0;

    // we provide MasksString, there is a range check, nothing to worry about
    ti.EditLine(IDC_VIEW_INHEX, (char*)Configuration.HexModeMasks.GetWritableMasksString(), MAX_PATH);
    // we provide MasksString, there is a range check, nothing to worry about
    ti.EditLine(IDC_VIEW_INTEXT, (char*)Configuration.TextModeMasks.GetWritableMasksString(), MAX_PATH);
    int errPos;
    Configuration.TextModeMasks.PrepareMasks(errPos);
    Configuration.HexModeMasks.PrepareMasks(errPos);

    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < NUMBER_OF_VIEWERCOLORS; i++)
            TmpColors[i] = ViewerColors[i];
        NormalText->SetColor(GetCOLORREF(TmpColors[VIEWER_FG_NORMAL]), GetCOLORREF(TmpColors[VIEWER_BK_NORMAL]));
        SelectedText->SetColor(GetCOLORREF(TmpColors[VIEWER_FG_SELECTED]), GetCOLORREF(TmpColors[VIEWER_BK_SELECTED]));
    }
    else
    {
        UseCustomViewerFont = LocalUseCustomViewerFont;
        if (memcmp(&ViewerLogFont, &LocalViewerLogFont, sizeof(LocalViewerLogFont)) != 0)
        { // When changing the font, we need to measure the new font.
            HANDLES(EnterCriticalSection(&ViewerFontMeasureCS));
            memcpy(&ViewerLogFont, &LocalViewerLogFont, sizeof(LocalViewerLogFont));
            ViewerFontMeasured = FALSE;
            HANDLES(LeaveCriticalSection(&ViewerFontMeasureCS));
        }
        BOOL colorChanged = FALSE;
        int i;
        for (i = 0; i < NUMBER_OF_VIEWERCOLORS; i++)
        {
            if (ViewerColors[i] != TmpColors[i])
            {
                ViewerColors[i] = TmpColors[i];
                colorChanged = TRUE;
            }
        }
        UpdateViewerColors(ViewerColors);

        // we will also distribute this news among the plug-ins
        if (colorChanged)
            Plugins.Event(PLUGINEVENT_COLORSCHANGED, 0);

        // after closing the dialog, BroadcastConfigChanged will be called
    }
}

void CCfgPageViewer::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageViewer::LoadControls()");

    LOGFONT logFont;
    if (LocalUseCustomViewerFont)
        logFont = LocalViewerLogFont;
    else
        GetDefaultViewerLogFont(&logFont);

    HWND hEdit = GetDlgItem(HWindow, IDE_VIEWERFONT);
    int origHeight = logFont.lfHeight;
    logFont.lfHeight = GetWindowFontHeight(hEdit); // to present the font in the edit line, we will use its font size
    if (HFont != NULL)
        HANDLES(DeleteObject(HFont));
    HFont = HANDLES(CreateFontIndirect(&logFont));
    HDC hDC = HANDLES(GetDC(HWindow));
    SendMessage(hEdit, WM_SETFONT, (WPARAM)HFont, MAKELPARAM(TRUE, 0));
    char buf[LF_FACESIZE + 200];
    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_FONTDESCRIPTION),
                MulDiv(-origHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY)),
                LocalViewerLogFont.lfFaceName,
                LoadStr(LocalUseCustomViewerFont ? IDS_FONTDESCRIPTION_CST : IDS_FONTDESCRIPTION_DEF));
    SetWindowText(hEdit, buf);

    HANDLES(ReleaseDC(HWindow, hDC));
}

INT_PTR
CCfgPageViewer::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageViewer::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        new CButton(HWindow, IDB_VIEWERFONT, BTF_RIGHTARROW);

        NormalText = new CColorArrowButton(HWindow, IDC_IV_NORMAL_TEXT, TRUE);
        SelectedText = new CColorArrowButton(HWindow, IDC_IV_SELECTED_TEXT, TRUE);

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDB_VIEWERFONT:
            {
                /* used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() call below...
MENU_TEMPLATE_ITEM CfgPageViewerMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_USEDEFAULTFONT
  {MNTT_IT, IDS_USECUSTOMFONT
  {MNTT_PE, 0
};*/
                HMENU hMenu = CreatePopupMenu();
                BOOL cstFont = LocalUseCustomViewerFont;
                InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_USEDEFAULTFONT));
                InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING, 2, LoadStr(IDS_USECUSTOMFONT));

                TPMPARAMS tpmPar;
                tpmPar.cbSize = sizeof(tpmPar);
                GetWindowRect((HWND)lParam, &tpmPar.rcExclude);
                DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                             HWindow, &tpmPar);
                if (cmd == 1)
                {
                    // default font
                    LocalUseCustomViewerFont = FALSE;
                    LoadControls();
                }
                if (cmd == 2)
                {
                    // custom font
                    CHOOSEFONT cf;
                    LOGFONT logFont = LocalViewerLogFont;
                    memset(&cf, 0, sizeof(CHOOSEFONT));
                    cf.lStructSize = sizeof(CHOOSEFONT);
                    cf.hwndOwner = HWindow;
                    cf.hDC = NULL;
                    cf.lpLogFont = &logFont;
                    cf.iPointSize = 10;
                    cf.Flags = CF_NOVERTFONTS | CF_FIXEDPITCHONLY | CF_SCREENFONTS |
                               CF_INITTOLOGFONTSTRUCT;
                    if (ChooseFont(&cf) != 0)
                    {
                        LocalViewerLogFont = logFont;
                        LocalUseCustomViewerFont = TRUE;
                        LoadControls();
                    }
                }
                return 0;
            }

            case IDC_IV_NORMAL_TEXT:
            case IDC_IV_SELECTED_TEXT:
            {
                CColorArrowButton* button = (CColorArrowButton*)WindowsManager.GetWindowPtr((HWND)lParam);
                if (button != NULL)
                {
                    /* Used for the export_mnu.py script, which generates salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() calls below...
MENU_TEMPLATE_ITEM CfgPageViewerMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_FG
  {MNTT_IT, IDS_SETCOLOR_SYSTEM_FG
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_BK
  {MNTT_IT, IDS_SETCOLOR_SYSTEM_BK
  {MNTT_PE, 0
};*/
                    HMENU hMenu = CreatePopupMenu();
                    BOOL normal = button == NormalText;
                    BOOL checkedDefaultFg = GetFValue(TmpColors[normal ? VIEWER_FG_NORMAL : VIEWER_FG_SELECTED]) & SCF_DEFAULT;
                    BOOL checkedDefaultBk = GetFValue(TmpColors[normal ? VIEWER_BK_NORMAL : VIEWER_BK_SELECTED]) & SCF_DEFAULT;
                    InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_SETCOLOR_CUSTOM_FG));
                    InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING, 2, LoadStr(IDS_SETCOLOR_SYSTEM_FG));
                    InsertMenu(hMenu, 0xFFFFFFFF, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
                    InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultBk ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 3, LoadStr(IDS_SETCOLOR_CUSTOM_BK));
                    InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultBk ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING, 4, LoadStr(IDS_SETCOLOR_SYSTEM_BK));

                    //            int i;
                    //            for (i = 0; i < 4; i++)
                    //              SetMenuItemBitmaps(hMenu, i + 1, MF_BYCOMMAND, NULL, HMenuCheckDot);

                    TPMPARAMS tpmPar;
                    tpmPar.cbSize = sizeof(tpmPar);
                    GetWindowRect(button->HWindow, &tpmPar.rcExclude);
                    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                                 HWindow, &tpmPar);
                    if (cmd != 0)
                    {
                        int colorIndex;
                        if (normal)
                            colorIndex = (cmd == 1 || cmd == 2) ? VIEWER_FG_NORMAL : VIEWER_BK_NORMAL;
                        else
                            colorIndex = (cmd == 1 || cmd == 2) ? VIEWER_FG_SELECTED : VIEWER_BK_SELECTED;
                        if (cmd == 1 || cmd == 3)
                        {
                            CHOOSECOLOR cc;
                            cc.lStructSize = sizeof(cc);
                            cc.hwndOwner = HWindow;
                            cc.lpCustColors = (LPDWORD)CustomColors;
                            if (cmd == 1)
                                cc.rgbResult = button->GetTextColor();
                            else
                                cc.rgbResult = button->GetBkgndColor();
                            cc.Flags = CC_RGBINIT | CC_FULLOPEN;
                            if (ChooseColor(&cc) == TRUE)
                            {
                                if (cmd == 1)
                                    button->SetTextColor(cc.rgbResult);
                                else
                                    button->SetBkgndColor(cc.rgbResult);

                                BYTE flags = GetFValue(TmpColors[colorIndex]);
                                flags &= ~SCF_DEFAULT;
                                TmpColors[colorIndex] = cc.rgbResult & 0x00ffffff | (((DWORD)flags) << 24);
                            }
                        }
                        else
                        {
                            BYTE flags = GetFValue(TmpColors[colorIndex]);
                            flags |= SCF_DEFAULT;
                            TmpColors[colorIndex] = TmpColors[colorIndex] & 0x00ffffff | (((DWORD)flags) << 24);

                            UpdateViewerColors(TmpColors);
                            button->SetColor(GetCOLORREF(TmpColors[normal ? VIEWER_FG_NORMAL : VIEWER_FG_SELECTED]),
                                             GetCOLORREF(TmpColors[normal ? VIEWER_BK_NORMAL : VIEWER_BK_SELECTED]));
                        }
                    }
                    DestroyMenu(hMenu);
                }
                return 0;
            }
            }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
//
// ****************************************************************************
// CCfgPageUserMenu
//
/*  void 
CSmallIconWindow::SetIcon(HICON hIcon)
{
  HIcon = hIcon;
  InvalidateRect(HWindow, NULL, TRUE);
  UpdateWindow(HWindow);
}

LRESULT 
CSmallIconWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  switch (uMsg)
  {
    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hdc = HANDLES(BeginPaint(HWindow, &ps));

      RECT r;
      GetClientRect(HWindow, &r);

      FillRect(hdc, &r, HDialogBrush);

      if (IsWindowEnabled(HWindow))
        DrawIconEx(hdc, (r.right - 16) / 2, (r.bottom - 16) / 2,
                   HIcon, 0, 0, 0, NULL, DI_NORMAL);
      HANDLES(EndPaint(HWindow, &ps));
      return 0;
    }
  }
  return CWindow::WindowProc(uMsg, wParam, lParam);
}*/

CCfgPageUserMenu::CCfgPageUserMenu(CUserMenuItems* userMenuItems)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_USERMENU, IDD_CFGPAGE_USERMENU, PSP_USETITLE, NULL)
{
    SourceUserMenuItems = userMenuItems;
    UserMenuItems = new CUserMenuItems(10, 5);
    UserMenuItems->LoadUMI(*SourceUserMenuItems, FALSE);
    EditLB = NULL;
    //  SmallIcon = NULL;
    DisableNotification = FALSE;
}

CCfgPageUserMenu::~CCfgPageUserMenu()
{
    if (UserMenuItems != NULL)
        delete UserMenuItems;
}

void CCfgPageUserMenu::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageUserMenu::Validate()");
    int i;
    for (i = 0; i < UserMenuItems->Count; i++)
    {
        int errorPos1, errorPos2;
        CUserMenuItem* item = UserMenuItems->At(i);
        if (item->Type == umitItem)
        {
            if (!ValidateCommandFile(HWindow, item->UMCommand, errorPos1, errorPos2))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_COMMAND);
                PostMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
            if (!ValidateUserMenuArguments(HWindow, item->Arguments, errorPos1, errorPos2, NULL))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_ARGUMENTS);
                PostMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
            if (!ValidateInitDir(HWindow, item->InitDir, errorPos1, errorPos2))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_INITDIR);
                PostMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
        }
    }
}

void CCfgPageUserMenu::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageUserMenu::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < UserMenuItems->Count; i++)
            EditLB->AddItem((INT_PTR)UserMenuItems->At(i));
        EditLB->SetCurSel(0);
        LoadControls();
        EnableButtons();
    }
    else
    {
        SourceUserMenuItems->LoadUMI(*UserMenuItems, UserMenuIconBkgndReader.IsReadingIcons() ||
                                                         UserMenuIconBkgndReader.HasSysColorsChanged()); // After changing the system colors, simply copying the icons is not enough, they must be reloaded (in the icon dialog, changing system colors is not addressed, icons may be "broken")
    }
}

// Retrieve the child from the dialog and enable or disable it based on the 'enable' variable
// if 'enable' is FALSE and 'clear' is FALSE, it can still clear the editline or checkbox

void EnableDlgWindow(HWND hDialog, int resID, BOOL enable, BOOL clear = FALSE)
{
    HWND hChild = GetDlgItem(hDialog, resID);
    if (hChild == NULL)
    {
        TRACE_E("EnableDlgWindow: cannot find child window resID=" << resID);
        return;
    }
    EnableWindow(hChild, enable);
    if (!enable && clear)
    {
        char className[31];
        className[0] = 0;
        GetClassName(hChild, className, 30);
        if (StrICmp(className, "edit") == 0)
            SetWindowText(hChild, "");
        else if (StrICmp(className, "button") == 0)
            SendMessage(hChild, BM_SETCHECK, BST_UNCHECKED, 0);
    }
}

void CCfgPageUserMenu::EnableButtons()
{
    BOOL oldDisableNotification = DisableNotification;
    DisableNotification = TRUE;

    BOOL validItem = TRUE;
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (itemID != -1)
    {
        CUserMenuItem* item = (CUserMenuItem*)itemID;
        if (item->Type == umitSubmenuEnd)
            validItem = FALSE;
    }
    if (itemID == -1)
        validItem = FALSE;

    BOOL separator = IsDlgButtonChecked(HWindow, IDC_UM_SEPARATOR);
    BOOL submenu = IsDlgButtonChecked(HWindow, IDC_UM_SUBMENU);

    EnableDlgWindow(HWindow, IDC_UM_SUBMENU, validItem & !separator, TRUE);
    EnableDlgWindow(HWindow, IDC_UM_SEPARATOR, validItem & !submenu, TRUE);

    EnableWindow(GetDlgItem(HWindow, IDB_UM_CHANGEICON), validItem & !separator & !submenu);
    /*    EnableWindow(SmallIcon->HWindow, validItem & !separator & !submenu);
  InvalidateRect(SmallIcon->HWindow, NULL, TRUE);
  UpdateWindow(SmallIcon->HWindow);*/
    EnableDlgWindow(HWindow, IDE_COMMAND, validItem & !separator & !submenu, TRUE);
    EnableDlgWindow(HWindow, IDE_ARGUMENTS, validItem & !separator & !submenu, TRUE);
    EnableDlgWindow(HWindow, IDE_INITDIR, validItem & !separator & !submenu, TRUE);

    EnableWindow(GetDlgItem(HWindow, IDB_BROWSECOMMAND), validItem & !separator & !submenu);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEARGUMENTS), validItem & !separator & !submenu);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEINITDIR), validItem & !separator & !submenu);

    EnableDlgWindow(HWindow, IDC_THROUGHSHELL, validItem & !separator & !submenu, TRUE);
    BOOL throughShell = IsDlgButtonChecked(HWindow, IDC_THROUGHSHELL);
    EnableDlgWindow(HWindow, IDC_OUTPUTWND,
                    validItem & throughShell & !separator & !submenu, TRUE);
    BOOL openShell = IsDlgButtonChecked(HWindow, IDC_OUTPUTWND);
    EnableDlgWindow(HWindow, IDC_CLOSESHELL,
                    validItem & throughShell & openShell & !separator & !submenu, TRUE);

    EnableWindow(GetDlgItem(HWindow, IDC_UM_TOOLBAR), validItem);
    DisableNotification = oldDisableNotification;
}

void CCfgPageUserMenu::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageUserMenu::LoadControls()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    BOOL empty = FALSE;
    if (itemID == -1)
        empty = TRUE;

    CUserMenuItem* item = NULL;
    if (!empty)
        item = (CUserMenuItem*)itemID;
    DisableNotification = TRUE;

    CheckDlgButton(HWindow, IDC_UM_SUBMENU, (!empty && (item->Type == umitSubmenuBegin)) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(HWindow, IDC_UM_SEPARATOR, (!empty && (item->Type == umitSeparator)) ? BST_CHECKED : BST_UNCHECKED);

    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_LIMITTEXT, USRMNUARGS_MAXLEN - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->UMCommand));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->Arguments));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL, 0, -1); // to overwrite the content of the browse
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->InitDir));
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL, 0, -1); // to overwrite the content of the browse

    //  SmallIcon->SetIcon(!empty ? item->HIcon : NULL);

    CheckDlgButton(HWindow, IDC_OUTPUTWND, (!empty && (item->UseWindow)) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(HWindow, IDC_THROUGHSHELL, (!empty && (item->ThroughShell)) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(HWindow, IDC_CLOSESHELL, (!empty && (item->CloseShell)) ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(HWindow, IDC_UM_TOOLBAR, (!empty && (item->ShowInToolbar)) ? BST_CHECKED : BST_UNCHECKED);
    DisableNotification = FALSE;
}

void CCfgPageUserMenu::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPageUserMenu::StoreControls()");
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        CUserMenuItem* item = UserMenuItems->At(index);

        char command[MAX_PATH];
        char arguments[USRMNUARGS_MAXLEN];
        char initdir[MAX_PATH];
        SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_GETTEXT,
                    MAX_PATH, (LPARAM)command);
        SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_GETTEXT,
                    USRMNUARGS_MAXLEN, (LPARAM)arguments);
        SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_GETTEXT,
                    MAX_PATH, (LPARAM)initdir);

        item->Set(item->ItemName, command, arguments, initdir, item->Icon);

        BOOL submenu = (IsDlgButtonChecked(HWindow, IDC_UM_SUBMENU) == BST_CHECKED);
        BOOL separator = (IsDlgButtonChecked(HWindow, IDC_UM_SEPARATOR) == BST_CHECKED);

        CUserMenuItemType type;
        if (submenu)
            type = umitSubmenuBegin;
        else if (separator)
            type = umitSeparator;
        else
            type = umitItem;
        if (item->Type != type)
        {
            item->SetType(type);
            item->GetIconHandle(NULL, FALSE);
            EditLB->RedrawFocusedItem();
        }

        item->UseWindow = (IsDlgButtonChecked(HWindow, IDC_OUTPUTWND) == BST_CHECKED);
        item->ThroughShell = (IsDlgButtonChecked(HWindow, IDC_THROUGHSHELL) == BST_CHECKED);
        item->CloseShell = (IsDlgButtonChecked(HWindow, IDC_CLOSESHELL) == BST_CHECKED);
        item->ShowInToolbar = (IsDlgButtonChecked(HWindow, IDC_UM_TOOLBAR) == BST_CHECKED);
    }
}

void CCfgPageUserMenu::DeleteSubmenuEnd(int index)
{
    int endIndex = UserMenuItems->GetSubmenuEndIndex(index);
    if (endIndex != -1)
    {
        // We have a closing item, let's delete it
        EditLB->DeleteItem(endIndex);
        UserMenuItems->Delete(endIndex);
    }
}

void CCfgPageUserMenu::RefreshGroupIconInUMItems()
{
    for (int i = 0; i < UserMenuItems->Count; i++)
    {
        CUserMenuItem* item = UserMenuItems->At(i);
        if (item->Type == umitSubmenuBegin)
            item->UMIcon = HGroupIcon;
    }
}

INT_PTR
CCfgPageUserMenu::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageUserMenu::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        RefreshGroupIconInUMItems();                                                          // if the colors have changed before the first access to the User Menu page, and we did not receive WM_SYSCOLORCHANGE, we handle it like this
        EditLB = new CEditListBox(HWindow, IDL_MENUITEMS, ELB_ENABLECOMMANDS | ELB_SHOWICON); // we need enabler and icons
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        ChangeToArrowButton(HWindow, IDB_BROWSECOMMAND);
        ChangeToArrowButton(HWindow, IDB_BROWSEARGUMENTS);
        ChangeToArrowButton(HWindow, IDB_BROWSEINITDIR);
        EditLB->MakeHeader(IDS_USHEADER);
        //      SmallIcon = new CSmallIconWindow(HWindow, IDC_UM_ICON);
        //      if (SmallIcon == NULL) TRACE_E(LOW_MEMORY);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDL_MENUITEMS);

        break;
    }

    case WM_COMMAND:
    {
        if (DisableNotification)
            return 0;

        if (HIWORD(wParam) == BN_CLICKED)
        {
            if (LOWORD(wParam) == IDC_UM_SUBMENU)
            {
                int index;
                EditLB->GetCurSel(index);
                if (index >= 0 && index < EditLB->GetCount())
                {
                    BOOL submenu = IsDlgButtonChecked(HWindow, IDC_UM_SUBMENU) == BST_CHECKED;
                    if (submenu)
                    {
                        // user checked the Submenu checkbox; let's add a closing item
                        static char emptyBuffer[] = "";
                        CUserMenuItem* item = new CUserMenuItem(LoadStr(IDS_ENDUSERSUBMENU), emptyBuffer, emptyBuffer, emptyBuffer, emptyBuffer,
                                                                FALSE, FALSE, FALSE, FALSE, umitSubmenuEnd, NULL);
                        if (item == NULL)
                            return 0;
                        UserMenuItems->Insert(index + 1, item);
                        EditLB->InsertItem((INT_PTR)item, index + 1);

                        // icon change will occur
                        //              EditLB->RedrawFocusedItem();
                    }
                    else
                    {
                        // user turned off the Submenu checkbox; we will remove the closing item
                        DeleteSubmenuEnd(index);
                    }
                }
            }

            EnableButtons();
            StoreControls();
        }

        if (HIWORD(wParam) == EN_CHANGE)
        {
            StoreControls();
            EnableButtons();
        }

        if (LOWORD(wParam) == IDL_MENUITEMS && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            LoadControls();
            EnableButtons();
        }

        switch (LOWORD(wParam))
        {
        case IDB_BROWSECOMMAND:
        {
            const CExecuteItem* item;
            item = TrackExecuteMenu(HWindow, IDB_BROWSECOMMAND, IDE_COMMAND, FALSE,
                                    CommandExecutes, IDS_EXEFILTER);
            if (item != NULL)
            {
                // update icons
                int index;
                EditLB->GetCurSel(index);
                if (index >= 0 && index < EditLB->GetCount())
                {
                    CUserMenuItem* item2 = UserMenuItems->At(index);
                    item2->GetIconHandle(NULL, FALSE);
                    EditLB->RedrawFocusedItem();
                }
            }
            return 0;
        }

        case IDB_BROWSEARGUMENTS:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEARGUMENTS, IDE_ARGUMENTS, FALSE,
                             UserMenuArgsExecutes);
            return 0;
        }

        case IDB_BROWSEINITDIR:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEINITDIR, IDE_INITDIR, FALSE,
                             InitDirExecutes);
            return 0;
        }

        case IDB_UM_CHANGEICON:
        {
            char fileName[MAX_PATH + 10];
            fileName[0] = 0;
            int resID = 0;

            int index;
            EditLB->GetCurSel(index);
            if (index >= 0 && index < EditLB->GetCount())
            {
                CUserMenuItem* item = UserMenuItems->At(index);
                if (item->Icon != NULL && item->Icon[0] != 0)
                {
                    // Icon is in the format "file name, resID"
                    // perform decomposition
                    char* iterator = item->Icon + strlen(item->Icon) - 1;
                    while (iterator > item->Icon && *iterator != ',')
                        iterator--;
                    if (iterator > item->Icon && *iterator == ',')
                    {
                        strncpy(fileName, item->Icon, iterator - item->Icon);
                        fileName[iterator - item->Icon] = 0;
                        iterator++;
                        resID = atoi(iterator);
                    }
                }
                BOOL error = FALSE;
                if (fileName[0] == 0 && item->UMCommand != NULL)
                {
                    if (!ExpandCommand(MainWindow->HWindow, item->UMCommand, fileName, MAX_PATH, FALSE))
                        error = TRUE;
                    else
                    {
                        while (strlen(fileName) > 2 && CutDoubleQuotesFromBothSides(fileName))
                            ;
                    }
                }

                if (!error)
                {
                    CChangeIconDialog dlg(HWindow, fileName, &resID);
                    if (dlg.Execute() == IDOK)
                    {
                        sprintf(fileName + strlen(fileName), ",%d", resID);
                        item->Set(item->ItemName, item->UMCommand, item->Arguments, item->InitDir, fileName);
                        item->GetIconHandle(NULL, FALSE);
                        EditLB->RedrawFocusedItem();
                    }
                }
            }
            return 0;
        }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        RefreshGroupIconInUMItems();
        return 0;
    }

    case WM_NOTIFY:
    {
        /*        LRESULT result;
      if (EditLB->OnWMNotify(lParam, result))
      {
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, result);
        return 0;
      }*/
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDL_MENUITEMS:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    CUserMenuItem* item = (CUserMenuItem*)dispInfo->ItemID;
                    strcpy(dispInfo->Buffer, item->ItemName);
                    dispInfo->HIcon = item->UMIcon;
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    CUserMenuItem* item;
                    if (dispInfo->ItemID == -1)
                    {
                        item = new CUserMenuItem();
                        if (item == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        UserMenuItems->Add(item);
                        item->Set(dispInfo->Buffer, item->UMCommand, item->Arguments, item->InitDir, item->Icon);
                        EditLB->SetItemData((INT_PTR)item);
                    }
                    else
                    {
                        item = (CUserMenuItem*)dispInfo->ItemID;
                        item->Set(dispInfo->Buffer, item->UMCommand, item->Arguments, item->InitDir, item->Icon);
                    }

                    LoadControls();
                    EnableButtons();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }

            case EDTLBN_ENABLECOMMANDS:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                dispInfo->Enable = TLBHDRMASK_NEW;
                if (index < 0 || index >= UserMenuItems->Count)
                {
                    dispInfo->Enable |= TLBHDRMASK_MODIFY;
                    return TRUE;
                }

                if (UserMenuItems->At(index)->Type != umitSubmenuEnd)
                    dispInfo->Enable |= TLBHDRMASK_DELETE | TLBHDRMASK_MODIFY;

                if (UserMenuItems->At(index)->Type == umitSubmenuBegin ||
                    UserMenuItems->At(index)->Type == umitSubmenuEnd)
                {
                    if (UserMenuItems->At(index)->Type == umitSubmenuBegin)
                    {
                        // umitSubmenuBegin - at the beginning, it is possible to move upwards always, downwards until its end hits the end of the list
                        dispInfo->Enable |= TLBHDRMASK_UP;
                        int endIndex = UserMenuItems->GetSubmenuEndIndex(index);
                        if (endIndex != -1 && endIndex + 1 < UserMenuItems->Count)
                        {
                            dispInfo->Enable |= TLBHDRMASK_DOWN;
                        }
                    }
                    else
                    {
                        // umitSubmenuEnd - we do not allow crossing over another end or beginning at the end
                        if (index > 0)
                        {
                            if (UserMenuItems->At(index - 1)->Type != umitSubmenuBegin &&
                                UserMenuItems->At(index - 1)->Type != umitSubmenuEnd)
                                dispInfo->Enable |= TLBHDRMASK_UP;
                        }
                        if (index >= 0 && index < UserMenuItems->Count - 1)
                        {
                            if (UserMenuItems->At(index + 1)->Type != umitSubmenuBegin &&
                                UserMenuItems->At(index + 1)->Type != umitSubmenuEnd)
                                dispInfo->Enable |= TLBHDRMASK_DOWN;
                        }
                    }
                }
                else
                {
                    // !umitSubmenuBegin && !umitSubmenuEnd
                    dispInfo->Enable |= TLBHDRMASK_UP | TLBHDRMASK_DOWN;
                }
                return TRUE;
            }
                /*              case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);

              CUserMenuItem *item = UserMenuItems->At(index);

              if (item->Type == umitSubmenuBegin)
              {
                int endIndex = UserMenuItems->GetSubmenuEndIndex(index);
                if (endIndex != -1)
                {
                  // nasli jsme SubmenuEnd
                  BYTE buf[sizeof(CUserMenuItem)];
                  if (dispInfo->Up)
                  {
                    // suneme s celym blokem nahoru
                    if (index > 0)
                    {
                      memcpy(buf, UserMenuItems->At(index - 1), sizeof(CUserMenuItem));
                      int i;
                      for (i = index; i <= endIndex; i++)
                        memcpy(UserMenuItems->At(i - 1), UserMenuItems->At(i), sizeof(CUserMenuItem));
                      memcpy(UserMenuItems->At(endIndex), buf, sizeof(CUserMenuItem));
                      EditLB->SetCurSel(index - 1);
                    }
                  }
                  else
                  {
                    // suneme s celym blokem dolu
                    if (endIndex > 0 && endIndex < EditLB->GetCount() - 1)
                    {
                      memcpy(buf, UserMenuItems->At(endIndex + 1), sizeof(CUserMenuItem));
                      int i;
                      for (i = endIndex; i >= index; i--)
                        memcpy(UserMenuItems->At(i + 1), UserMenuItems->At(i), sizeof(CUserMenuItem));
                      memcpy(UserMenuItems->At(index), buf, sizeof(CUserMenuItem));
                      EditLB->SetCurSel(index + 1);
                    }
                  }
                  InvalidateRect(EditLB->HWindow, NULL, FALSE);
                }

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);  // zakazu jeho zmeny
                return TRUE;
              }
              else
              {
                int srcIndex = index;
                int dstIndex = index + (dispInfo->Up ? -1 : 1);

                BYTE buf[sizeof(CUserMenuItem)];
                memcpy(buf, UserMenuItems->At(srcIndex), sizeof(CUserMenuItem));
                memcpy(UserMenuItems->At(srcIndex), UserMenuItems->At(dstIndex), sizeof(CUserMenuItem));
                memcpy(UserMenuItems->At(dstIndex), buf, sizeof(CUserMenuItem));

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim jeho zmeny
                return TRUE;
              }
            }
*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                int srcIndex = index;
                int dstIndex = dispInfo->NewIndex;

                CUserMenuItem* item = UserMenuItems->At(index);

                if (item->Type == umitSubmenuBegin)
                {
                    int endIndex = UserMenuItems->GetSubmenuEndIndex(index);
                    if (endIndex != -1)
                    {
                        // found SubmenuEnd
                        BYTE buf[sizeof(CUserMenuItem)];
                        if (srcIndex > dstIndex)
                        {
                            // we are moving the whole block up
                            if (index > 0)
                            {
                                memcpy(buf, UserMenuItems->At(index - 1), sizeof(CUserMenuItem));
                                int i;
                                for (i = index; i <= endIndex; i++)
                                    memcpy(UserMenuItems->At(i - 1), UserMenuItems->At(i), sizeof(CUserMenuItem));
                                memcpy(UserMenuItems->At(endIndex), buf, sizeof(CUserMenuItem));
                                EditLB->SetCurSel(index - 1);
                            }
                        }
                        else
                        {
                            // we are sliding down with the whole block
                            if (endIndex > 0 && endIndex < EditLB->GetCount() - 1)
                            {
                                memcpy(buf, UserMenuItems->At(endIndex + 1), sizeof(CUserMenuItem));
                                int i;
                                for (i = endIndex; i >= index; i--)
                                    memcpy(UserMenuItems->At(i + 1), UserMenuItems->At(i), sizeof(CUserMenuItem));
                                memcpy(UserMenuItems->At(index), buf, sizeof(CUserMenuItem));
                                EditLB->SetCurSel(index + 1);
                            }
                        }
                        InvalidateRect(EditLB->HWindow, NULL, FALSE);
                    }

                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE); // prohibit its modification
                    return TRUE;
                }
                else
                {
                    dstIndex = index + (srcIndex > dstIndex ? -1 : 1);

                    BYTE buf[sizeof(CUserMenuItem)];
                    memcpy(buf, UserMenuItems->At(srcIndex), sizeof(CUserMenuItem));
                    memcpy(UserMenuItems->At(srcIndex), UserMenuItems->At(dstIndex), sizeof(CUserMenuItem));
                    memcpy(UserMenuItems->At(dstIndex), buf, sizeof(CUserMenuItem));

                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow his changes
                    return TRUE;
                }
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);

                // if the user closes the popup, we must also close the overlay
                if (UserMenuItems->At(index)->Type == umitSubmenuBegin)
                    DeleteSubmenuEnd(index);

                UserMenuItems->Delete(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDL_MENUITEMS)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageHotPath
//

CCfgPageHotPath::CCfgPageHotPath(BOOL editMode, int editIndex)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_HOTPATH, IDD_CFGPAGE_HOTPATH, PSP_USETITLE, NULL)
{
    Dirty = FALSE;
    Header = NULL;
    HListView = NULL;
    DisableNotification = FALSE;
    EditMode = editMode;
    EditIndex = editIndex;
    if (editIndex < 0 || editIndex >= HOT_PATHS_COUNT)
    {
        EditMode = FALSE;
        EditIndex = 0;
    }
    LabelEdit = FALSE;
    Config = new CHotPathItems();
    if (Config == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
}

CCfgPageHotPath::~CCfgPageHotPath()
{
    if (Config != NULL)
        delete Config;
}

void CCfgPageHotPath::Validate(CTransferInfo& ti)
{
    if (Dirty)
    {
        HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
        {
            int errorPos1, errorPos2;
            if (Config->GetNameLen(i) > 0)
            {
                char path[HOTPATHITEM_MAXPATH];
                Config->GetPath(i, path, HOTPATHITEM_MAXPATH);
                if (!ValidateHotPath(HWindow, path, errorPos1, errorPos2))
                {
                    ListView_SetItemState(HListView, i, LVIS_SELECTED, LVIS_SELECTED);
                    ListView_EnsureVisible(HListView, i, FALSE);
                    ti.ErrorOn(IDC_HOTPATH_PATH);
                    PostMessage(GetDlgItem(HWindow, IDC_HOTPATH_PATH), EM_SETSEL,
                                errorPos1, errorPos2);
                    return;
                }
            }
        }
        SetCursor(hOldCur);
    }
}

extern char HotPathSetBufferName[MAX_PATH];
extern char HotPathSetBufferPath[HOTPATHITEM_MAXPATH];

void CCfgPageHotPath::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageHotPath::Transfer()");
    ti.CheckBox(IDC_HOTPATH_AUTOCONFIG, Configuration.HotPathAutoConfig);
    if (ti.Type == ttDataToWindow)
    {
        Config->Load(MainWindow->HotPaths);

        if (EditMode)
            Config->Set(EditIndex, HotPathSetBufferName, HotPathSetBufferPath);

        int index = 0;
        DisableNotification = TRUE;
        char buff[20];
        char name[MAX_PATH];
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT | LVIF_STATE;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.state = 0;
            name[0] = 0;
            Config->GetName(i, name, MAX_PATH);
            lvi.pszText = name;
            ListView_InsertItem(HListView, &lvi);

            UINT state = INDEXTOSTATEIMAGEMASK((Config->GetVisible(i) ? 2 : 1));
            ListView_SetItemState(HListView, i, state, LVIS_STATEIMAGEMASK);

            if (i < 10)
            {
                sprintf(buff, "%s+%d", LoadStr(IDS_CTRL), i < 9 ? i + 1 : 0);
                ListView_SetItemText(HListView, i, 1, buff);
            }
        }

        RECT r;
        GetClientRect(HListView, &r);
        ListView_SetColumnWidth(HListView, 0, r.right - r.left);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        int w = ListView_GetColumnWidth(HListView, 1);
        ListView_SetColumnWidth(HListView, 0, r.right - r.left - w);

        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        if (EditMode)
            index = EditIndex;
        ListView_SetItemState(HListView, index, state, state);
        DisableNotification = FALSE;
        LoadControls();
        EnableHeader();
    }
    else
    {
        MainWindow->HotPaths.Load(*Config);
    }
    Dirty = FALSE;
}

void CCfgPageHotPath::LoadControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    char path[HOTPATHITEM_MAXPATH];
    path[0] = 0;
    BOOL visible = FALSE;
    if (index != -1)
    {
        Config->GetPath(index, path, HOTPATHITEM_MAXPATH);
        visible = Config->GetVisible(index);
    }

    DisableNotification = TRUE;
    SendDlgItemMessage(HWindow, IDC_HOTPATH_PATH, EM_LIMITTEXT, HOTPATHITEM_MAXPATH - 1, 0);
    SetDlgItemText(HWindow, IDC_HOTPATH_PATH, path);

    DisableNotification = FALSE;
    EnableControls();
}

void CCfgPageHotPath::StoreControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != -1)
    {
        char buff[HOTPATHITEM_MAXPATH];
        GetDlgItemText(HWindow, IDC_HOTPATH_PATH, buff, HOTPATHITEM_MAXPATH);
        Config->SetPath(index, buff);
    }
}

void CCfgPageHotPath::EnableControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    BOOL enable = TRUE;
    if (index == -1 || Config->GetNameLen(index) == 0)
        enable = FALSE;
    EnableWindow(GetDlgItem(HWindow, IDC_HOTPATH_PATH), enable);
    EnableWindow(GetDlgItem(HWindow, IDC_HOTPATH_BROWSE), enable);
}

void CCfgPageHotPath::EnableHeader()
{
    DWORD mask = 0;
    if (!LabelEdit)
    {
        mask |= TLBHDRMASK_MODIFY | TLBHDRMASK_DELETE;
        int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
        if (index > 0)
            mask |= TLBHDRMASK_UP;
        if (index < HOT_PATHS_COUNT - 1)
            mask |= TLBHDRMASK_DOWN;
    }
    Header->EnableToolbar(mask);
}

void CCfgPageHotPath::OnModify()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != -1)
        PostMessage(HListView, LVM_EDITLABEL, index, 0);
}

void CCfgPageHotPath::OnDelete()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != -1)
    {
        Config->Set(index, "", "");
        char buffEmpty[] = "";
        ListView_SetItemText(HListView, index, 0, buffEmpty);
        LoadControls();
    }
    EnableHeader();
}

void CCfgPageHotPath::OnMove(BOOL up)
{
    CALL_STACK_MESSAGE2("CCfgPageHotPath::OnMove(%d)", up);
    DisableNotification = TRUE;
    int index1 = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    int index2 = index1;
    if (index1 == -1)
        return;
    if (up && index1 > 0)
        index2 = index1 - 1;
    if (!up && index1 < HOT_PATHS_COUNT - 1)
        index2 = index1 + 1;
    if (index2 != index1)
    {
        char name1[MAX_PATH];
        char name2[MAX_PATH];
        Config->GetName(index1, name1, MAX_PATH);
        Config->GetName(index2, name2, MAX_PATH);
        ListView_SetItemText(HListView, index1, 0, name2);
        ListView_SetItemText(HListView, index2, 0, name1);
        DWORD state1 = ListView_GetItemState(HListView, index1, LVIS_STATEIMAGEMASK);
        DWORD state2 = ListView_GetItemState(HListView, index2, LVIS_STATEIMAGEMASK);
        ListView_SetItemState(HListView, index1, state2, LVIS_STATEIMAGEMASK);
        state1 |= LVIS_FOCUSED | LVIS_SELECTED;
        ListView_SetItemState(HListView, index2, state1, LVIS_STATEIMAGEMASK | LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(HListView, index2, FALSE);
        Config->SwapItems(index1, index2);
        LoadControls();
    }
    DisableNotification = FALSE;
    EnableHeader();
}

INT_PTR
CCfgPageHotPath::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageHotPath::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_PAINT:
    {
        // terrible mess - I need some message that will come
        // after WM_INITDIALOG, so I can set the focus
        // Hopefully this will last until the next version of Salama :-)
        if (EditMode)
        {
            SetFocus(HListView);
            SendMessage(HListView, LVM_EDITLABEL, EditIndex, 0);
            EditMode = FALSE;
        }
        break;
    }

    case WM_INITDIALOG:
    {
        CMyListView* listView = new CMyListView(HWindow, IDC_HOTPATH_LIST);
        HListView = listView->HWindow;
        //      HListView = GetDlgItem(HWindow, IDC_HOTPATH_LIST);
        Header = new CToolbarHeader(HWindow, IDC_HOTPATH_HEADER, HListView,
                                    TLBHDRMASK_MODIFY | TLBHDRMASK_DELETE |
                                        TLBHDRMASK_UP | TLBHDRMASK_DOWN);
        if (Header == NULL)
            TRACE_E(LOW_MEMORY);

        DWORD exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        //      ListView_SetExtendedListViewStyleEx(HListView, exFlags, exFlags);  // 4.71

        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags);

        // get the size of the listview
        RECT r;
        GetClientRect(HListView, &r);
        int nameWidth = r.right - (int)(r.right / 7);

        // I will pour the columns Name and HotKey into the listview
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_WIDTH | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_HOTPATH_NAME);
        lvc.cx = 1;
        lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_HOTPATH_HOTKEY);
        lvc.cx = 1;
        lvc.iSubItem = 1;
        ListView_InsertColumn(HListView, 1, &lvc);

        ChangeToArrowButton(HWindow, IDC_HOTPATH_BROWSE);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_HOTPATH_LIST);

        break;
    }

    case WM_USER_CHAR:
    {
        int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
        if (wParam != ' ' && !LabelEdit)
        {
            ListView_EditLabel(HListView, index);
            HWND hEdit = ListView_GetEditControl(HListView);
            SendMessage(hEdit, WM_CHAR, wParam, 0);
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        if (DisableNotification)
            break;

        if (wParam == IDC_HOTPATH_LIST)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if (nmhk->wVKey == VK_F2)
                {
                    OnModify();
                }
                if (nmhk->wVKey == VK_DELETE)
                {
                    OnDelete();
                }
                if ((GetKeyState(VK_MENU) & 0x8000) &&
                    (nmhk->wVKey == VK_UP || nmhk->wVKey == VK_DOWN))
                {
                    OnMove(nmhk->wVKey == VK_UP);
                }
                if (!LabelEdit && nmhk->wVKey == VK_INSERT)
                    ListView_EditLabel(HListView, index);
                break;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if (!(nmhi->uOldState & LVIS_SELECTED) && nmhi->uNewState & LVIS_SELECTED)
                {
                    LoadControls();
                }
                if (nmhi->uOldState & 0x2000 && nmhi->uNewState & 0x1000 ||
                    nmhi->uOldState & 0x1000 && nmhi->uNewState & 0x2000)
                {
                    BOOL checked = nmhi->uNewState & 0x2000;
                    Config->SetVisible(nmhi->iItem, checked);
                }
                EnableHeader();
                break;
            }

            case LVN_BEGINLABELEDIT:
            {
                LabelEdit = TRUE;
                EnableHeader();
                break;
            }

            case LVN_ENDLABELEDIT:
            {
                NMLVDISPINFO* nmhd = (NMLVDISPINFO*)nmh;
                LabelEdit = FALSE;
                EnableHeader();
                if (nmhd->item.pszText != NULL)
                {
                    char name[MAX_PATH];
                    lstrcpyn(name, nmhd->item.pszText, MAX_PATH);
                    Config->CleanName(name);
                    int index = nmhd->item.iItem;
                    char path[HOTPATHITEM_MAXPATH];
                    path[0] = 0;
                    if (strlen(name) != 0)
                        Config->GetPath(index, path, HOTPATHITEM_MAXPATH);
                    Config->Set(index, name, path);
                    LoadControls();
                    ListView_SetItemText(HListView, index, 0, name);
                    Dirty = TRUE;
                    break;
                }
                break;
            }
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_HOTPATH_PATH)
        {
            if (!DisableNotification)
            {
                StoreControls();
                Dirty = TRUE;
            }
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_HOTPATH_BROWSE)
        {
            TrackExecuteMenu(HWindow, IDC_HOTPATH_BROWSE, IDC_HOTPATH_PATH, FALSE,
                             HotPathItems);
            return TRUE;
        }

        if (LOWORD(wParam) == IDC_HOTPATH_HEADER)
        {
            if (GetFocus() != HListView)
                SetFocus(HListView);
            switch (HIWORD(wParam))
            {
            case TLBHDR_MODIFY:
                OnModify();
                break;
            case TLBHDR_DELETE:
                OnDelete();
                break;
            case TLBHDR_UP:
                OnMove(TRUE);
                break;
            case TLBHDR_DOWN:
                OnMove(FALSE);
                break;
            }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageSystem
//

CCfgPageSystem::CCfgPageSystem()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_SYSTEM, IDD_CFGPAGE_SYSTEM, PSP_USETITLE, NULL)
{
}

void CCfgPageSystem::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageSystem::Validate()");
    int useRecycle;
    ti.RadioButton(IDR_RECYCLE1, 0, useRecycle);
    ti.RadioButton(IDR_RECYCLE2, 1, useRecycle);
    ti.RadioButton(IDR_RECYCLE3, 2, useRecycle);
    if (useRecycle == 2)
    {
        char buf[MAX_PATH];
        lstrcpyn(buf, Configuration.RecycleMasks.GetMasksString(), MAX_PATH); // Backup RecycleBinMasks
        // we provide MasksString, there is a range check, nothing to worry about
        ti.EditLine(IDE_RECYCLEMASKS, Configuration.RecycleMasks.GetWritableMasksString(), MAX_PATH);
        int errorPos;
        if (!Configuration.RecycleMasks.PrepareMasks(errorPos))
        {
            SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(HWindow, IDE_RECYCLEMASKS));
            SendMessage(GetDlgItem(HWindow, IDE_RECYCLEMASKS), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(IDE_RECYCLEMASKS);
        }
        Configuration.RecycleMasks.SetMasksString(buf); // Restore RecycleBinMasks
    }
}

void CCfgPageSystem::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDR_RECYCLE1, 0, Configuration.UseRecycleBin);
    ti.RadioButton(IDR_RECYCLE2, 1, Configuration.UseRecycleBin);
    ti.RadioButton(IDR_RECYCLE3, 2, Configuration.UseRecycleBin);
    // we provide MasksString, there is a range check, nothing to worry about
    ti.EditLine(IDE_RECYCLEMASKS, Configuration.RecycleMasks.GetWritableMasksString(), MAX_PATH);

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CCfgPageSystem::EnableControls()
{
}

INT_PTR
CCfgPageSystem::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageColors
//

#define CFG7F_SINGLECOLOR 0x01 // only one color (foreground) is valid
#define CFG7F_DEFFG 0x02       // foreground can acquire default values
#define CFG7F_DEFBK 0x04       // background can take on default values

struct CConfigurationPage7SubData
{
    int Label;    // Resource ID of the string with the name of the first color [static before the button]
    BYTE ColorFg; // index of the foreground color (text) to be affected by this
    BYTE ColorBk; // index of the background color to be affected by
    BYTE Flags;   // options for item
};

struct CConfigurationPage7Data
{
    int ItemLabel; // Resource ID of the item with the name [combobox]
    CConfigurationPage7SubData Items[CFG_COLORS_BUTTONS];
};

#define PAGE7DATA_COUNT 7

CConfigurationPage7Data Page7Data[PAGE7DATA_COUNT] =
    {
        // colors of items in the panel
        {
            IDS_COLORITEM_PANELITEM,
            {{IDS_COLORLABEL_NORMAL, ITEM_FG_NORMAL, ITEM_BK_NORMAL, CFG7F_DEFFG | CFG7F_DEFBK},
             {IDS_COLORLABEL_FOCUSED, ITEM_FG_FOCUSED, ITEM_BK_FOCUSED, CFG7F_DEFFG},
             {IDS_COLORLABEL_SELECTED, ITEM_FG_SELECTED, ITEM_BK_SELECTED, CFG7F_DEFBK},
             {IDS_COLORLABEL_FOCUSEDSELECTED, ITEM_FG_FOCSEL, ITEM_BK_FOCSEL, 0},
             {IDS_COLORLABEL_HIGHLIGHTED, ITEM_FG_HIGHLIGHT, ITEM_BK_HIGHLIGHT, CFG7F_DEFFG | CFG7F_DEFBK}},
        },
        // pen color for the frame around the item
        {
            IDS_COLORITEM_FOCUSEDFRAME,
            {{IDS_COLORLABEL_ACTIVENORMAL, FOCUS_ACTIVE_NORMAL, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_ACTIVESELECTED, FOCUS_ACTIVE_SELECTED, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_INACTIVENORMAL, FOCUS_FG_INACTIVE_NORMAL, FOCUS_BK_INACTIVE_NORMAL, CFG7F_DEFBK},
             {IDS_COLORLABEL_INACTIVESELECTED, FOCUS_FG_INACTIVE_SELECTED, FOCUS_BK_INACTIVE_SELECTED, CFG7F_DEFBK},
             {0, 0, 0, 0}}},
        // pen colors for the frame around thumbnails
        {
            IDS_COLORITEM_THUMBNAILFRAME,
            {{IDS_COLORLABEL_NORMAL, THUMBNAIL_FRAME_NORMAL, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_FOCUSED, THUMBNAIL_FRAME_FOCUSED, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_SELECTED, THUMBNAIL_FRAME_SELECTED, 0, CFG7F_SINGLECOLOR},
             {IDS_COLORLABEL_FOCUSEDSELECTED, THUMBNAIL_FRAME_FOCSEL, 0, CFG7F_SINGLECOLOR},
             {0, 0, 0, 0}}},
        // colors for blending icons
        {
            IDS_COLORITEM_BLENDEDICONS,
            {{IDS_COLORLABEL_SELECTED, ICON_BLEND_SELECTED, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_FOCUSED, ICON_BLEND_FOCUSED, 0, CFG7F_SINGLECOLOR},
             {IDS_COLORLABEL_FOCUSEDSELECTED, ICON_BLEND_FOCSEL, 0, CFG7F_SINGLECOLOR},
             {0, 0, 0, 0},
             {0, 0, 0, 0}}},
        // colors progress bars
        {
            IDS_COLORITEM_PROGRESS,
            {{IDS_COLORLABEL_LEFTPART, PROGRESS_FG_SELECTED, PROGRESS_BK_SELECTED, CFG7F_DEFFG | CFG7F_DEFBK},
             {IDS_COLORLABEL_RIGHTPART, PROGRESS_FG_NORMAL, PROGRESS_BK_NORMAL, CFG7F_DEFFG | CFG7F_DEFBK},
             {0, 0, 0, 0},
             {0, 0, 0, 0},
             {0, 0, 0, 0}}},
        // colors of the panel title
        {
            IDS_COLORITEM_CAPTION,
            {{IDS_COLORLABEL_ACTIVE, ACTIVE_CAPTION_FG, ACTIVE_CAPTION_BK, CFG7F_DEFFG | CFG7F_DEFBK},
             {IDS_COLORLABEL_INACTIVE, INACTIVE_CAPTION_FG, INACTIVE_CAPTION_BK, CFG7F_DEFFG | CFG7F_DEFBK},
             {0, 0, 0, 0},
             {0, 0, 0, 0},
             {0, 0, 0, 0}}},
        // colors of the hot item
        {
            IDS_COLORITEM_HOT,
            {{IDS_COLORLABEL_HOTPANEL, HOT_PANEL, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_HOTACTIVE, HOT_ACTIVE, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {IDS_COLORLABEL_HOTINACTIVE, HOT_INACTIVE, 0, CFG7F_SINGLECOLOR | CFG7F_DEFFG},
             {0, 0, 0, 0},
             {0, 0, 0, 0}}},
};

int CConfigurationPage7Items[CFG_COLORS_BUTTONS] = {IDC_C_ITEM1_L, IDC_C_ITEM2_L, IDC_C_ITEM3_L, IDC_C_ITEM4_L, IDC_C_ITEM5_L};
int CConfigurationPage7Masks[CFG_COLORS_BUTTONS] = {IDC_C_MASK1_L, IDC_C_MASK2_L, IDC_C_MASK3_L, IDC_C_MASK4_L, IDC_C_MASK5_L};
int CConfigurationPage7ItemsBut[CFG_COLORS_BUTTONS] = {IDC_C_ITEM1_C, IDC_C_ITEM2_C, IDC_C_ITEM3_C, IDC_C_ITEM4_C, IDC_C_ITEM5_C};
int CConfigurationPage7MasksBut[CFG_COLORS_BUTTONS] = {IDC_C_MASK1_C, IDC_C_MASK2_C, IDC_C_MASK3_C, IDC_C_MASK4_C, IDC_C_MASK5_C};

CCfgPageColors::CCfgPageColors()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_COLORS, IDD_CFGPAGE_COLORS, PSP_USETITLE, NULL), HighlightMasks(10, 5)
{
    HScheme = NULL;
    HItem = NULL;
    int i;
    for (i = 0; i < CFG_COLORS_BUTTONS; i++)
    {
        Items[i] = NULL;
        Masks[i] = NULL;
    }

    EditLB = NULL;
    DisableNotification = FALSE;
    SourceHighlightMasks = MainWindow->HighlightMasks;
    HighlightMasks.Load(*SourceHighlightMasks);

    Dirty = FALSE;
}

void CCfgPageColors::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageColors::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < NUMBER_OF_COLORS; i++)
            TmpColors[i] = UserColors[i];

        int schemes[5] = {IDS_COLORSCHEME_SALAMANDER, IDS_COLORSCHEME_EXPLORER, IDS_COLORSCHEME_NORTON, IDS_COLORSCHEME_NAVIGATOR, IDS_COLORSCHEME_CUSTOM};
        for (i = 0; i < 5; i++)
            SendMessage(HScheme, CB_ADDSTRING, 0, (LPARAM)LoadStr(schemes[i]));

        for (i = 0; i < PAGE7DATA_COUNT; i++)
            SendMessage(HItem, CB_ADDSTRING, 0, (LPARAM)LoadStr(Page7Data[i].ItemLabel));

        int labels[CFG_COLORS_BUTTONS] = {IDS_COLORLABEL_NORMAL, IDS_COLORLABEL_FOCUSED, IDS_COLORLABEL_SELECTED, IDS_COLORLABEL_FOCUSEDSELECTED, IDS_COLORLABEL_HIGHLIGHTED};
        for (i = 0; i < CFG_COLORS_BUTTONS; i++)
            SetDlgItemText(HWindow, CConfigurationPage7Masks[i], LoadStr(labels[i]));

        int index = 4; // custom
        if (CurrentColors == SalamanderColors)
            index = 0;
        else if (CurrentColors == ExplorerColors)
            index = 1;
        else if (CurrentColors == NortonColors)
            index = 2;
        else if (CurrentColors == NavigatorColors)
            index = 3;
        SendMessage(HScheme, CB_SETCURSEL, index, 0);
        SendMessage(HItem, CB_SETCURSEL, 0, 0);

        // fill the list of hilight items
        for (i = 0; i < HighlightMasks.Count; i++)
            EditLB->AddItem((INT_PTR)HighlightMasks[i]);

        DisableNotification = TRUE;
        EditLB->SetCurSel(0);
        DisableNotification = FALSE;
        LoadColors();
        LoadMasks();
        EnableControls();
    }
    else
    {
        int index = (int)SendMessage(HScheme, CB_GETCURSEL, 0, 0);
        if (index == 0)
            CurrentColors = SalamanderColors;
        else if (index == 1)
            CurrentColors = ExplorerColors;
        else if (index == 2)
            CurrentColors = NortonColors;
        else if (index == 3)
            CurrentColors = NavigatorColors;
        else
        {
            CurrentColors = UserColors;
            int i;
            for (i = 0; i < NUMBER_OF_COLORS; i++)
                UserColors[i] = TmpColors[i];
        }

        ColorsChanged(TRUE, TRUE, FALSE); // We save time, we only let color-dependent items change, we do not reload icons again

        SourceHighlightMasks->Load(HighlightMasks);
        int errPos;
        int i;
        for (i = 0; i < SourceHighlightMasks->Count; i++)
            SourceHighlightMasks->At(i)->Masks->PrepareMasks(errPos);
    }
}

void CCfgPageColors::LoadColors()
{
    CALL_STACK_MESSAGE1("CCfgPageColors::LoadColors()");

    COLORREF* tmpColors;
    int index = (int)SendMessage(HScheme, CB_GETCURSEL, 0, 0);
    if (index == 0)
        tmpColors = SalamanderColors;
    else if (index == 1)
        tmpColors = ExplorerColors;
    else if (index == 2)
        tmpColors = NortonColors;
    else if (index == 3)
        tmpColors = NavigatorColors;
    else
        tmpColors = TmpColors;

    // let's extract default values
    UpdateDefaultColors(tmpColors, &HighlightMasks, TRUE, TRUE);

    index = (int)SendMessage(HItem, CB_GETCURSEL, 0, 0);
    CConfigurationPage7Data* data = &Page7Data[index];

    int i;
    for (i = 0; i < CFG_COLORS_BUTTONS; i++)
    {
        CConfigurationPage7SubData* subData = &data->Items[i];
        const char* label;
        if (subData->Label != 0)
            label = LoadStr(subData->Label);
        else
            label = "";
        SetDlgItemText(HWindow, CConfigurationPage7Items[i], label);
        if (subData->Label != 0)
        {
            if (subData->Flags & CFG7F_SINGLECOLOR)
                Items[i]->SetColor(GetCOLORREF(tmpColors[subData->ColorFg]), GetCOLORREF(tmpColors[subData->ColorFg]));
            else
                Items[i]->SetColor(GetCOLORREF(tmpColors[subData->ColorFg]), GetCOLORREF(tmpColors[subData->ColorBk]));
        }
        ShowWindow(Items[i]->HWindow, subData->Label != 0 ? SW_SHOW : SW_HIDE);
    }

    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (itemID != -1)
    {
        CHighlightMasksItem* item = (CHighlightMasksItem*)itemID;
        Masks[0]->SetColor(GetCOLORREF(item->NormalFg), GetCOLORREF(item->NormalBk));
        Masks[1]->SetColor(GetCOLORREF(item->FocusedFg), GetCOLORREF(item->FocusedBk));
        Masks[2]->SetColor(GetCOLORREF(item->SelectedFg), GetCOLORREF(item->SelectedBk));
        Masks[3]->SetColor(GetCOLORREF(item->FocSelFg), GetCOLORREF(item->FocSelBk));
        Masks[4]->SetColor(GetCOLORREF(item->HighlightFg), GetCOLORREF(item->HighlightBk));
    }
}

#define PAGE7_CTRLCOUNT 7
DWORD Page7Attributes[PAGE7_CTRLCOUNT] = {FILE_ATTRIBUTE_ARCHIVE, FILE_ATTRIBUTE_READONLY, FILE_ATTRIBUTE_HIDDEN, FILE_ATTRIBUTE_SYSTEM, FILE_ATTRIBUTE_COMPRESSED, FILE_ATTRIBUTE_ENCRYPTED, FILE_ATTRIBUTE_DIRECTORY};
DWORD Page7Controls[PAGE7_CTRLCOUNT] = {IDC_C_ARCHIVE, IDC_C_READONLY, IDC_C_HIDDEN, IDC_C_SYSTEM, IDC_C_COMPRESSED, IDC_C_ENCRYPTED, IDC_C_DIRECTORY};

void CCfgPageColors::LoadMasks()
{
    CALL_STACK_MESSAGE1("CCfgPageColors::LoadMask()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    BOOL empty = FALSE;
    if (itemID == -1)
        empty = TRUE;

    CHighlightMasksItem* item = NULL;
    if (!empty)
        item = (CHighlightMasksItem*)itemID;

    DisableNotification = TRUE;
    DWORD state = BST_UNCHECKED;
    int i;
    for (i = 0; i < PAGE7_CTRLCOUNT; i++)
    {
        if (!empty)
            state = item->ValidAttr & Page7Attributes[i] ? (item->Attr & Page7Attributes[i] ? BST_CHECKED : BST_UNCHECKED) : BST_INDETERMINATE;
        CheckDlgButton(HWindow, Page7Controls[i], state);
    }
    DisableNotification = FALSE;
}

void CCfgPageColors::StoreMasks()
{
    CALL_STACK_MESSAGE1("CCfgPageColors::StoreMask()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (!DisableNotification && itemID != -1)
    {
        CHighlightMasksItem* item = (CHighlightMasksItem*)itemID;
        item->ValidAttr = 0;
        item->Attr = 0;
        int i;
        for (i = 0; i < PAGE7_CTRLCOUNT; i++)
        {
            DWORD state = IsDlgButtonChecked(HWindow, Page7Controls[i]);
            if (state == BST_CHECKED || state == BST_UNCHECKED)
            {
                item->ValidAttr |= Page7Attributes[i];
                if (state == BST_CHECKED)
                    item->Attr |= Page7Attributes[i];
            }
        }
    }
}

void CCfgPageColors::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageColors::EnableControls()");
    BOOL validItem = TRUE;
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (itemID == -1)
        validItem = FALSE;

    EnableWindow(GetDlgItem(HWindow, IDC_C_ARCHIVE), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_READONLY), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_HIDDEN), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_SYSTEM), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_COMPRESSED), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_ENCRYPTED), validItem);
    EnableWindow(GetDlgItem(HWindow, IDC_C_DIRECTORY), validItem);

    if (!validItem)
        Masks[0]->SetColor(RGB(255, 255, 255), RGB(255, 255, 255));
    EnableWindow(GetDlgItem(HWindow, IDC_C_MASK1_C), validItem);
    if (!validItem)
        Masks[1]->SetColor(RGB(255, 255, 255), RGB(255, 255, 255));
    EnableWindow(GetDlgItem(HWindow, IDC_C_MASK2_C), validItem);
    if (!validItem)
        Masks[2]->SetColor(RGB(255, 255, 255), RGB(255, 255, 255));
    EnableWindow(GetDlgItem(HWindow, IDC_C_MASK3_C), validItem);
    if (!validItem)
        Masks[3]->SetColor(RGB(255, 255, 255), RGB(255, 255, 255));
    EnableWindow(GetDlgItem(HWindow, IDC_C_MASK4_C), validItem);
    if (!validItem)
        Masks[4]->SetColor(RGB(255, 255, 255), RGB(255, 255, 255));
    EnableWindow(GetDlgItem(HWindow, IDC_C_MASK5_C), validItem);
}

void CCfgPageColors::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageColors::Validate()");
    if (Dirty)
    {
        int i;
        for (i = 0; i < HighlightMasks.Count; i++)
        {
            CMaskGroup masks(HighlightMasks[i]->Masks->GetMasksString());
            int errorPos1;
            if (!masks.PrepareMasks(errorPos1))
            {
                EditLB->SetCurSel(i);
                SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDC_C_LIST);
                PostMessage(HWindow, WM_USER_EDIT, errorPos1, errorPos1 + 1);
                return;
            }
        }
    }
}

INT_PTR
CCfgPageColors::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageColors::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HScheme = GetDlgItem(HWindow, IDC_C_SCHEME);
        HItem = GetDlgItem(HWindow, IDC_C_ITEM);

        int i;
        for (i = 0; i < 5; i++)
        {
            Items[i] = new CColorArrowButton(HWindow, CConfigurationPage7ItemsBut[i], TRUE);
            Masks[i] = new CColorArrowButton(HWindow, CConfigurationPage7MasksBut[i], TRUE);
        }

        EditLB = new CEditListBox(HWindow, IDC_C_LIST);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        EditLB->MakeHeader(IDC_C_LIST_HEADER);
        EditLB->EnableDrag(::GetParent(HWindow));

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_C_LIST);

        break;
    }

    case WM_USER_EDIT:
    {
        SetFocus(GetDlgItem(HWindow, IDC_C_LIST));
        EditLB->OnBeginEdit((int)wParam, (int)lParam);
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            StoreMasks();
        }

        if (HIWORD(wParam) == CBN_SELCHANGE && LOWORD(wParam) == IDC_C_SCHEME || LOWORD(wParam) == IDC_C_ITEM)
        {
            LoadColors();
            break;
        }
        if (HIWORD(wParam) == LBN_SELCHANGE && LOWORD(wParam) == IDC_C_LIST)
        {
            EditLB->OnSelChanged();
            LoadMasks();
            LoadColors();
            EnableControls();
            break;
        }

        WORD id = LOWORD(wParam);
        if (id == IDC_C_ITEM1_C || id == IDC_C_ITEM2_C || id == IDC_C_ITEM3_C || id == IDC_C_ITEM4_C || id == IDC_C_ITEM5_C ||
            id == IDC_C_MASK1_C || id == IDC_C_MASK2_C || id == IDC_C_MASK3_C || id == IDC_C_MASK4_C || id == IDC_C_MASK5_C)
        {
            CColorArrowButton* button = (CColorArrowButton*)WindowsManager.GetWindowPtr((HWND)lParam);
            HMENU hMenu = CreatePopupMenu();

            BOOL item = (id == IDC_C_ITEM1_C || id == IDC_C_ITEM2_C || id == IDC_C_ITEM3_C || id == IDC_C_ITEM4_C || id == IDC_C_ITEM5_C);

            CHighlightMasksItem* highlightItem = NULL;
            if (!item)
            {
                INT_PTR itemID;
                EditLB->GetCurSelItemID(itemID);
                if (itemID == -1)
                {
                    TRACE_E("This should never happen!");
                    break;
                }
                highlightItem = (CHighlightMasksItem*)itemID;
            }
            BOOL singleColor = FALSE;
            BOOL enabledFg = FALSE;
            BOOL enabledBk = FALSE;
            BOOL checkedDefaultFg = FALSE;
            BOOL checkedDefaultBk = FALSE;
            COLORREF* tmpColors = NULL;
            CConfigurationPage7Data* data = NULL;
            CConfigurationPage7SubData* subData = NULL;
            SALCOLOR* fgColor = NULL;
            SALCOLOR* bkColor = NULL;
            if (item)
            {
                data = &Page7Data[SendMessage(HItem, CB_GETCURSEL, 0, 0)];
                int index;

                index = (int)SendMessage(HScheme, CB_GETCURSEL, 0, 0);
                if (index == 0)
                    tmpColors = SalamanderColors;
                else if (index == 1)
                    tmpColors = ExplorerColors;
                else if (index == 2)
                    tmpColors = NortonColors;
                else if (index == 3)
                    tmpColors = NavigatorColors;
                else
                    tmpColors = TmpColors;

                if (id == IDC_C_ITEM1_C)
                    index = 0;
                else if (id == IDC_C_ITEM2_C)
                    index = 1;
                else if (id == IDC_C_ITEM3_C)
                    index = 2;
                else if (id == IDC_C_ITEM4_C)
                    index = 3;
                else if (id == IDC_C_ITEM5_C)
                    index = 4;

                subData = &data->Items[index];

                if (GetFValue(tmpColors[subData->ColorFg]) & SCF_DEFAULT)
                    checkedDefaultFg = TRUE;
                if (subData->Flags & CFG7F_SINGLECOLOR)
                    singleColor = TRUE;
                else if (GetFValue(tmpColors[subData->ColorBk]) & SCF_DEFAULT)
                    checkedDefaultBk = TRUE;

                if (subData->Flags & CFG7F_DEFFG)
                    enabledFg = TRUE;
                if (subData->Flags & CFG7F_DEFBK)
                    enabledBk = TRUE;
            }
            else
            {
                enabledFg = TRUE;
                enabledBk = TRUE;

                switch (id)
                {
                case IDC_C_MASK1_C:
                {
                    fgColor = &highlightItem->NormalFg;
                    bkColor = &highlightItem->NormalBk;
                    break;
                }

                case IDC_C_MASK2_C:
                {
                    fgColor = &highlightItem->FocusedFg;
                    bkColor = &highlightItem->FocusedBk;
                    break;
                }

                case IDC_C_MASK3_C:
                {
                    fgColor = &highlightItem->SelectedFg;
                    bkColor = &highlightItem->SelectedBk;
                    break;
                }

                case IDC_C_MASK4_C:
                {
                    fgColor = &highlightItem->FocSelFg;
                    bkColor = &highlightItem->FocSelBk;
                    break;
                }

                case IDC_C_MASK5_C:
                {
                    fgColor = &highlightItem->HighlightFg;
                    bkColor = &highlightItem->HighlightBk;
                    break;
                }
                }

                if (GetFValue(*fgColor) & SCF_DEFAULT)
                    checkedDefaultFg = TRUE;
                if (GetFValue(*bkColor) & SCF_DEFAULT)
                    checkedDefaultBk = TRUE;
            }
            int maxCmd;
            if (singleColor)
            {
                /* used for the export_mnu.py script, which generates salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() call below...
MENU_TEMPLATE_ITEM CfgPageColorsMenu1[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_SETCOLOR_CUSTOM
  {MNTT_IT, IDS_SETCOLOR_SYSTEM
  {MNTT_PE, 0
};*/
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_SETCOLOR_CUSTOM));
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING | enabledFg ? 0
                                                                                                                       : MF_GRAYED,
                           2, LoadStr(IDS_SETCOLOR_SYSTEM));
                maxCmd = 2;
            }
            else
            {
                /* Used for the export_mnu.py script, which generates salmenu.mnu for the Translator
   to keep synchronized with the InsertMenu() calls below...
MENU_TEMPLATE_ITEM CfgPageColorsMenu2[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_FG
  {MNTT_IT, IDS_SETCOLOR_SYSTEM_FG
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_BK
  {MNTT_IT, IDS_SETCOLOR_SYSTEM_BK
  {MNTT_PE, 0
};
MENU_TEMPLATE_ITEM CfgPageColorsMenu3[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_FG
  {MNTT_IT, IDS_SETCOLOR_DEFAULT_FG
  {MNTT_IT, IDS_SETCOLOR_CUSTOM_BK
  {MNTT_IT, IDS_SETCOLOR_DEFAULT_BK
  {MNTT_PE, 0
};*/
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_SETCOLOR_CUSTOM_FG));
                int textResID = (item || id == IDC_C_MASK5_C) ? IDS_SETCOLOR_SYSTEM_FG : IDS_SETCOLOR_DEFAULT_FG;
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultFg ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING | enabledFg ? 0
                                                                                                                       : MF_GRAYED,
                           2, LoadStr(textResID));
                InsertMenu(hMenu, 0xFFFFFFFF, MF_BYCOMMAND | MF_SEPARATOR, 0, NULL);
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultBk ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 3, LoadStr(IDS_SETCOLOR_CUSTOM_BK));
                textResID = (item || id == IDC_C_MASK5_C) ? IDS_SETCOLOR_SYSTEM_BK : IDS_SETCOLOR_DEFAULT_BK;
                InsertMenu(hMenu, 0xFFFFFFFF, checkedDefaultBk ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING | enabledBk ? 0
                                                                                                                       : MF_GRAYED,
                           4, LoadStr(textResID));
                maxCmd = 4;
            }

            //        int i;
            //        for (i = 0; i < maxCmd; i++)
            //          SetMenuItemBitmaps(hMenu, i + 1, MF_BYCOMMAND, NULL, HMenuCheckDot);

            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            GetWindowRect(button->HWindow, &tpmPar.rcExclude);
            DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                         HWindow, &tpmPar);
            if (cmd != 0)
            {
                int index = (int)SendMessage(HScheme, CB_GETCURSEL, 0, 0);
                if (index != 4)
                {
                    COLORREF* colors;
                    if (index == 0)
                        colors = SalamanderColors;
                    else if (index == 1)
                        colors = ExplorerColors;
                    else if (index == 2)
                        colors = NortonColors;
                    else if (index == 3)
                        colors = NavigatorColors;

                    int i;
                    for (i = 0; i < NUMBER_OF_COLORS; i++)
                        TmpColors[i] = colors[i];
                    SendMessage(HScheme, CB_SETCURSEL, 4, 0);
                }

                if (cmd == 1 || cmd == 3)
                {
                    CHOOSECOLOR cc;
                    cc.lStructSize = sizeof(cc);
                    cc.hwndOwner = HWindow;
                    cc.lpCustColors = (LPDWORD)CustomColors;

                    if (cmd == 1)
                        cc.rgbResult = button->GetTextColor();
                    else
                        cc.rgbResult = button->GetBkgndColor();
                    cc.Flags = CC_RGBINIT | CC_FULLOPEN;
                    if (ChooseColor(&cc))
                    {
                        if (item)
                        {
                            int colorIndex = (cmd == 1) ? subData->ColorFg : subData->ColorBk;
                            BYTE flags = GetFValue(TmpColors[colorIndex]);
                            flags &= ~SCF_DEFAULT;
                            TmpColors[colorIndex] = cc.rgbResult & 0x00ffffff | (((DWORD)flags) << 24);
                        }
                        else
                        {
                            // masks
                            SALCOLOR* color = (cmd == 1) ? fgColor : bkColor;
                            BYTE flags = GetFValue(*color);
                            flags &= ~SCF_DEFAULT;
                            *color = cc.rgbResult & 0x00ffffff | (((DWORD)flags) << 24);
                        }
                    }
                }
                else
                {
                    // cmd == 2 || cmd == 4
                    if (item)
                    {
                        int colorIndex = (cmd == 2) ? subData->ColorFg : subData->ColorBk;
                        BYTE flags = GetFValue(TmpColors[colorIndex]);
                        flags |= SCF_DEFAULT;
                        TmpColors[colorIndex] = TmpColors[colorIndex] & 0x00ffffff | (((DWORD)flags) << 24);
                    }
                    else
                    {
                        SALCOLOR* color = (cmd == 2) ? fgColor : bkColor;
                        BYTE flags = GetFValue(*color);
                        flags |= SCF_DEFAULT;
                        *color = *color & 0x00ffffff | (((DWORD)flags) << 24);
                    }
                }
                // we will load the colors into the buttons
                LoadColors();
            }
            DestroyMenu(hMenu);
            return 0;
        }
        break;
    }
    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDC_C_LIST:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    lstrcpyn(dispInfo->Buffer, ((CHighlightMasksItem*)dispInfo->ItemID)->Masks->GetMasksString(), MAX_PATH);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    Dirty = TRUE;
                    CHighlightMasksItem* item;
                    if (dispInfo->ItemID == -1)
                    {
                        item = new CHighlightMasksItem();
                        if (item == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        HighlightMasks.Add(item);
                        item->Set(dispInfo->Buffer);
                        EditLB->SetItemData((INT_PTR)item);
                        PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDC_C_LIST, LBN_SELCHANGE), (LPARAM)EditLB->HWindow);
                    }
                    else
                    {
                        item = (CHighlightMasksItem*)dispInfo->ItemID;
                        item->Set(dispInfo->Buffer);
                    }

                    LoadMasks();
                    LoadColors();
                    EnableControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*              case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              int srcIndex = index;
              int dstIndex = index + (dispInfo->Up ? -1 : 1);

              char buf[sizeof(CHighlightMasksItem)];
              memcpy(buf, HighlightMasks[srcIndex], sizeof(CHighlightMasksItem));
              memcpy(HighlightMasks[srcIndex], HighlightMasks[dstIndex], sizeof(CHighlightMasksItem));
              memcpy(HighlightMasks[dstIndex], buf, sizeof(CHighlightMasksItem));

              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // allow swapping
              return TRUE;
            }*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                int srcIndex = index;
                int dstIndex = dispInfo->NewIndex;

                char buf[sizeof(CHighlightMasksItem)];
                memcpy(buf, HighlightMasks[srcIndex], sizeof(CHighlightMasksItem));
                if (srcIndex < dstIndex)
                {
                    int i;
                    for (i = srcIndex; i < dstIndex; i++)
                        memcpy(HighlightMasks[i], HighlightMasks[i + 1], sizeof(CHighlightMasksItem));
                }
                else
                {
                    int i;
                    for (i = srcIndex; i > dstIndex; i--)
                        memcpy(HighlightMasks[i], HighlightMasks[i - 1], sizeof(CHighlightMasksItem));
                }
                memcpy(HighlightMasks[dstIndex], buf, sizeof(CHighlightMasksItem));

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow change
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                HighlightMasks.Delete(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // allow deletion
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDC_C_LIST)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageHistory
//

CCfgPageHistory::CCfgPageHistory()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_HISTORY, IDD_CFGPAGE_HISTORY, PSP_USETITLE, NULL)
{
}

void CCfgPageHistory::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_HISTORY_SAVEHISTORY, Configuration.SaveHistory);
    ti.CheckBox(IDC_HISTORY_WORKDIRS, Configuration.SaveWorkDirs);
    ti.CheckBox(IDC_HISTORY_ENABLECMDLINE, Configuration.EnableCmdLineHistory);
    ti.CheckBox(IDC_HISTORY_SAVECMDLINE, Configuration.SaveCmdLineHistory);

    if (ti.Type == ttDataToWindow)
        EnableControls();
    else
        MainWindow->EditWindow->FillHistory();
}

void CCfgPageHistory::EnableControls()
{
    BOOL saveHistory = IsDlgButtonChecked(HWindow, IDC_HISTORY_SAVEHISTORY) == BST_CHECKED;
    if (!saveHistory)
    {
        CheckDlgButton(HWindow, IDC_HISTORY_WORKDIRS, BST_UNCHECKED);
    }
    EnableWindow(GetDlgItem(HWindow, IDC_HISTORY_WORKDIRS), saveHistory);
    BOOL enableCmdLineHistory = IsDlgButtonChecked(HWindow, IDC_HISTORY_ENABLECMDLINE) == BST_CHECKED;
    if (!saveHistory || !enableCmdLineHistory)
        CheckDlgButton(HWindow, IDC_HISTORY_SAVECMDLINE, BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_HISTORY_SAVECMDLINE), saveHistory && enableCmdLineHistory);
}

void CCfgPageHistory::OnClearHistory()
{
    // Set paths in panels to fixed (otherwise the path may be abandoned immediately)
    // fill in the panel Alt+F12, etc.)
    if (MainWindow->LeftPanel != NULL)
        MainWindow->LeftPanel->ChangeToRescuePathOrFixedDrive(HWindow);
    if (MainWindow->RightPanel != NULL)
        MainWindow->RightPanel->ChangeToRescuePathOrFixedDrive(HWindow);

    // history of the main window (FileHistory, DirHistory)
    MainWindow->ClearHistory();

    // history of configuration (SelectHistory, CopyHistory, EditHistory, ChangeDirHistory, FileListHistory)
    Configuration.ClearHistory();
    MainWindow->EditWindow->FillHistory();

    // history of Find dialog including combobox of open windows
    ClearFindHistory(FALSE);

    // History of internal viewers including the combo box of open Find windows
    ClearViewerHistory(FALSE);

    // Storage of selected names
    GlobalSelection.Clear();

    // history of both panels (PathHistory, FilterHistory, Selection)
    if (MainWindow->LeftPanel != NULL)
        MainWindow->LeftPanel->ClearHistory();
    if (MainWindow->RightPanel != NULL)
        MainWindow->RightPanel->ClearHistory();

    // we will clear the history in all plugins (all loaded ones will remain for it to work
    // perform additional Save - delete data in the Registry
    Plugins.ClearHistory(HWindow);

    // We will also clear the memory of the last used directories on individual disks
    InitDefaultDir();
}

INT_PTR
CCfgPageHistory::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_HISTORY_CLEARHISTORY:
        {
            if (SalMessageBox(HWindow, LoadStr(IDS_CLEARHISTORY_CNFRM), LoadStr(IDS_QUESTION),
                              MB_YESNO | MB_ICONQUESTION) == IDYES)
            {
                OnClearHistory();
                SalMessageBox(HWindow, LoadStr(IDS_HISTORY_WAS_CLEARED), LoadStr(IDS_INFOTITLE),
                              MB_OK | MB_ICONINFORMATION);
            }
            return 0;
        }

        case IDC_HISTORY_SAVEHISTORY:
        case IDC_HISTORY_ENABLECMDLINE:
        {
            EnableControls();
            break;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
