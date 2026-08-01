// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <time.h>
//#ifdef MSVC_RUNTIME_CHECKS
#include <rtcapi.h>
//#endif // MSVC_RUNTIME_CHECKS

#include "allochan.h"
#include "menu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "shellib.h"
#include "worker.h"
#include "snooper.h"
#include "viewer.h"
#include "editwnd.h"
#include "find.h"
#include "zip.h"
#include "pack.h"
#include "cache.h"
#include "dialogs.h"
#include "gui.h"
#include "tasklist.h"
#include <uxtheme.h>
#include "olespy.h"
#include "geticon.h"
#include "logo.h"
#include "color.h"
#include "toolbar.h"

#include "svg.h"

extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "shiconov.h"
#include "salmoncl.h"
#include "jumplist.h"
#include "usermenu.h"
#include "execute.h"
#include "drivelst.h"

#pragma comment(linker, "/ENTRY:MyEntryPoint") // we want a custom entry point for the application

#pragma comment(lib, "UxTheme.lib")

// Make the application's original entry point available.
extern "C" int WinMainCRTStartup();

#ifdef X64_STRESS_TEST

#define X64_STRESS_TEST_ALLOC_COUNT 1000

LPVOID X64StressTestPointers[X64_STRESS_TEST_ALLOC_COUNT];

void X64StressTestAlloc()
{
    // At this point the loader has already loaded the EXE and the RTL, and RTL initialization
    // has already created and allocated a heap that lives below 4 GB. To push subsequent
    // allocations above 4 GB, we have to reserve the lower part of the virtual address space
    // and then force the RTL to expand its heap by making allocations.
    //
    // Reserve space in the virtual address space.
    UINT64 vaAllocated = 0;
    _int64 allocSize[] = {10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0};
    for (int i = 0; allocSize[i] != 0; i++)
        while (VirtualAlloc(0, allocSize[i], MEM_RESERVE, PAGE_NOACCESS) <= (LPVOID)(UINT_PTR)0x00000000ffffffff) // If this memory is accessed, we want an exception, and we do not want MEM_COMMIT so we do not waste memory unnecessarily.
            vaAllocated += allocSize[i];

    // Now fill up the RTL heap.
    UINT64 rtlAllocated = 0;
    _int64 rtlAllocSize[] = {10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0};
    for (int i = 0; rtlAllocSize[i] != 0; i++)
        while (_malloc_dbg(rtlAllocSize[i], _CRT_BLOCK, __FILE__, __LINE__) <= (LPVOID)(UINT_PTR)0x00000000ffffffff)
            rtlAllocated += rtlAllocSize[i];

    // check for success
    void* testNew = new char; // new uses alloc too, but let's verify that as well
    if (testNew <= (LPVOID)(UINT_PTR)0x00000000ffffffff)
        MessageBox(NULL, "new address <= 0x00000000ffffffff!\nPlease contact jan.rysavy@altap.cz with this information.", "X64_STRESS_TEST", MB_OK | MB_ICONEXCLAMATION);
    delete testNew;
}

#endif //X64_STRESS_TEST
// our own entry point, which we told the linker to use via pragma
int MyEntryPoint()
{
#ifdef X64_STRESS_TEST
    // Consume the lower 4 GB of address space so later allocations get pointers larger than a DWORD.
    X64StressTestAlloc();
#endif //X64_STRESS_TEST

    int ret = 1; // error

    // Start Salmon so it can catch as many of our crashes as possible
    if (SalmonInit())
    {
        // call the application's original entry point to start the program
        ret = WinMainCRTStartup();
    }
    else
        MessageBox(NULL, "Open Salamander Bug Reporter (salmon.exe) initialization has failed. Please reinstall Open Salamander.",
                   SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONSTOP);

    // the debugger no longer gets this far; we crash in the RTL (tested with VC 2008 and our RTL)

    // done
    return ret;
}

BOOL SalamanderBusy = TRUE;       // Is Salamander busy?
DWORD LastSalamanderIdleTime = 0; // GetTickCount() value from when SalamanderBusy last changed to TRUE

int PasteLinkIsRunning = 0; // if greater than zero, the Paste Shortcuts command is currently running in one of the panels

BOOL CannotCloseSalMainWnd = FALSE; // TRUE = the main window must not be closed

DWORD MainThreadID = -1;

int MenuNewExceptionHasOccured = 0;
int FGIExceptionHasOccured = 0;
int ICExceptionHasOccured = 0;
int QCMExceptionHasOccured = 0;
int OCUExceptionHasOccured = 0;
int GTDExceptionHasOccured = 0;
int SHLExceptionHasOccured = 0;
int RelExceptionHasOccured = 0;

char DecimalSeparator[5] = "."; // characters retrieved from the system (max. 4 characters)
int DecimalSeparatorLen = 1;    // length in characters, excluding the terminating null
char ThousandsSeparator[5] = " ";
int ThousandsSeparatorLen = 1;

BOOL WindowsXP64AndLater = FALSE;  // JRYFIXME - remove
BOOL WindowsVistaAndLater = FALSE; // JRYFIXME - remove this
BOOL Windows7AndLater = FALSE;     // JRYFIXME - remove
BOOL Windows8AndLater = FALSE;
BOOL Windows8_1AndLater = FALSE;
BOOL Windows10AndLater = FALSE;

BOOL Windows64Bit = FALSE;

BOOL RunningAsAdmin = FALSE;

DWORD CCVerMajor = 0;
DWORD CCVerMinor = 0;

char ConfigurationName[MAX_PATH];
BOOL ConfigurationNameIgnoreIfNotExists = TRUE;

int StopRefresh = 0;

BOOL ExecCmdsOrUnloadMarkedPlugins = FALSE;
BOOL OpenPackOrUnpackDlgForMarkedPlugins = FALSE;

int StopIconRepaint = 0;
BOOL PostAllIconsRepaint = FALSE;

int StopStatusbarRepaint = 0;
BOOL PostStatusbarRepaint = FALSE;

int ChangeDirectoryAllowed = 0;
BOOL ChangeDirectoryRequest = FALSE;

BOOL SkipOneActivateRefresh = FALSE;

const char* DirColumnStr = NULL;
int DirColumnStrLen = 0;
const char* ColExtStr = NULL;
int ColExtStrLen = 0;
int TextEllipsisWidth = 0;
int TextEllipsisWidthEnv = 0;
const char* ProgDlgHoursStr = NULL;
const char* ProgDlgMinutesStr = NULL;
const char* ProgDlgSecsStr = NULL;

char FolderTypeName[80] = "";
int FolderTypeNameLen = 0;
const char* UpDirTypeName = NULL;
int UpDirTypeNameLen = 0;
const char* CommonFileTypeName = NULL;
int CommonFileTypeNameLen = 0;
const char* CommonFileTypeName2 = NULL;

char WindowsDirectory[MAX_PATH] = "";

// Ensures escape from removed drives to a fixed drive after a device is ejected (USB flash drive, etc.).
BOOL ChangeLeftPanelToFixedWhenIdleInProgress = FALSE; // TRUE = the path is currently being changed; setting ChangeLeftPanelToFixedWhenIdle to TRUE is unnecessary
BOOL ChangeLeftPanelToFixedWhenIdle = FALSE;
BOOL ChangeRightPanelToFixedWhenIdleInProgress = FALSE; // TRUE = the path is currently being changed; setting ChangeRightPanelToFixedWhenIdle to TRUE is unnecessary
BOOL ChangeRightPanelToFixedWhenIdle = FALSE;
BOOL OpenCfgToChangeIfPathIsInaccessibleGoTo = FALSE; // TRUE = when idle, opens the configuration on Drives and focuses "If path in panel is inaccessible, go to:"

char IsSLGIncomplete[ISSLGINCOMPLETE_SIZE]; // If the string is empty, the SLG is fully translated; otherwise it contains the URL of the forum section for that language.

UINT TaskbarBtnCreatedMsg = 0;

// ****************************************************************************

C__MainWindowCS MainWindowCS;
BOOL CanDestroyMainWindow = FALSE;
CMainWindow* MainWindow = NULL;
CFilesWindow* DropSourcePanel = NULL;
BOOL OurClipDataObject = FALSE;
const char* SALCF_IDATAOBJECT = "SalIDataObject";
const char* SALCF_FAKE_REALPATH = "SalFakeRealPath";
const char* SALCF_FAKE_SRCTYPE = "SalFakeSrcType";
const char* SALCF_FAKE_SRCFSPATH = "SalFakeSrcFSPath";

const char* MAINWINDOW_NAME = "Open Salamander";
const char* CMAINWINDOW_CLASSNAME = "SalamanderMainWindowVer25";
const char* SAVEBITS_CLASSNAME = "SalamanderSaveBits";
const char* SHELLEXECUTE_CLASSNAME = "SalamanderShellExecute";

CAssociations Associations; // associations loaded from the registry
CShares Shares;

char DefaultDir['Z' - 'A' + 1][MAX_PATH];

HACCEL AccelTable1 = NULL;
HACCEL AccelTable2 = NULL;

HINSTANCE NtDLL = NULL;             // handle to ntdll.dll
HINSTANCE Shell32DLL = NULL;        // handle to shell32.dll (icons)
HINSTANCE ImageResDLL = NULL;       // handle to imageres.dll (Vista icons)
HINSTANCE User32DLL = NULL;         // handle to user32.dll (DisableProcessWindowsGhosting)
HINSTANCE HLanguage = NULL;         // handle to language-specific resources (.SPL file)
char CurrentHelpDir[MAX_PATH] = ""; // after Help is used for the first time, this stores the path to the help directory (where all .chm files are located)
WORD LanguageID = 0;                // language ID of the .SPL file

char OpenReadmeInNotepad[MAX_PATH]; // used only when launched from the installer: the name of the file to open in Notepad when idle

BOOL UseCustomPanelFont = FALSE;
HFONT Font = NULL;
HFONT FontUL = NULL;
LOGFONT LogFont;
int FontCharHeight = 0;

HFONT EnvFont = NULL;
HFONT EnvFontUL = NULL;
//LOGFONT EnvLogFont;
int EnvFontCharHeight = 0;
HFONT TooltipFont = NULL;

HBRUSH HNormalBkBrush = NULL;
HBRUSH HFocusedBkBrush = NULL;
HBRUSH HSelectedBkBrush = NULL;
HBRUSH HFocSelBkBrush = NULL;
HBRUSH HDialogBrush = NULL;
HBRUSH HButtonTextBrush = NULL;
HBRUSH HDitherBrush = NULL;
HBRUSH HActiveCaptionBrush = NULL;
HBRUSH HInactiveCaptionBrush = NULL;

HBRUSH HMenuSelectedBkBrush = NULL;
HBRUSH HMenuSelectedTextBrush = NULL;
HBRUSH HMenuHilightBrush = NULL;
HBRUSH HMenuGrayTextBrush = NULL;

HPEN HActiveNormalPen = NULL; // pens for the rectangle around an item
HPEN HActiveSelectedPen = NULL;
HPEN HInactiveNormalPen = NULL;
HPEN HInactiveSelectedPen = NULL;

HPEN HThumbnailNormalPen = NULL; // Pens for the thumbnail border
HPEN HThumbnailFucsedPen = NULL;
HPEN HThumbnailSelectedPen = NULL;
HPEN HThumbnailFocSelPen = NULL;

HPEN BtnShadowPen = NULL;
HPEN BtnHilightPen = NULL;
HPEN Btn3DLightPen = NULL;
HPEN BtnFacePen = NULL;
HPEN WndFramePen = NULL;
HPEN WndPen = NULL;
HBITMAP HFilter = NULL;
HBITMAP HHeaderSort = NULL;

HIMAGELIST HFindSymbolsImageList = NULL;
HIMAGELIST HMenuMarkImageList = NULL;
HIMAGELIST HGrayToolBarImageList = NULL;
HIMAGELIST HHotToolBarImageList = NULL;
HIMAGELIST HBottomTBImageList = NULL;
HIMAGELIST HHotBottomTBImageList = NULL;

CBitmap ItemBitmap;

HBITMAP HUpDownBitmap = NULL;
HBITMAP HZoomBitmap = NULL;

//HBITMAP HWorkerBitmap = NULL;

HCURSOR HHelpCursor = NULL;

int SystemDPI = 0; // Global DPI across all monitors. Salamander does not support per-monitor DPI; see https://msdn.microsoft.com/library/windows/desktop/dn469266.aspx
int IconSizes[] = {16, 32, 48};
int IconLRFlags = 0;
HICON HSharedOverlays[] = {0};
HICON HShortcutOverlays[] = {0};
HICON HSlowFileOverlays[] = {0};
CIconList* SimpleIconLists[] = {0};
CIconList* ThrobberFrames = NULL;
CIconList* LockFrames = NULL; // for simplicity, declare and load it like the throbber

HICON HGroupIcon = NULL;
HICON HFavoritIcon = NULL;
HICON HSlowFileIcon = NULL;

RGBQUAD ColorTable[256] = {0};

DWORD MouseHoverTime = 0;

SYSTEMTIME SalamanderStartSystemTime = {0}; // Salamander start time (GetSystemTime)

BOOL WaitForESCReleaseBeforeTestingESC = FALSE; // should we wait for ESC to be released before starting to list the path in the panel?

int SPACE_WIDTH = 10;

const char* LOW_MEMORY = "Low memory.";

BOOL DragFullWindows = TRUE;

CWindowQueue ViewerWindowQueue("Internal Viewers");

CFindSetDialog GlobalFindDialog(NULL /* ignored */, 0 /* ignored */, 0 /* ignored */);

CNames GlobalSelection;
CDirectorySizesHolder DirectorySizesHolder;

HWND PluginProgressDialog = NULL;
HWND PluginMsgBoxParent = NULL;

BOOL CriticalShutdown = FALSE;

HANDLE SalOpenFileMapping = NULL;
void* SalOpenSharedMem = NULL;

// mutex for synchronizing load/save to the Registry (two processes at once cause problems)
CLoadSaveToRegistryMutex LoadSaveToRegistryMutex;

BOOL IsNotAlphaNorNum[256]; // TRUE/FALSE array for characters (TRUE = not a letter or digit)
BOOL IsAlpha[256];          // TRUE/FALSE array for characters (TRUE = letter)

// the default user charset for fonts; on W2K+ DEFAULT_CHARSET alone would already be enough
//
// Under WinXP, Czech can be selected as the default in the regional settings,
// but Czech fonts may still be missing on the Advanced tab. Then, when creating
// a font with UserCharset encoding, the operating system returns a font with a completely
// different name (face name), mainly to satisfy the requested encoding. Therefore, it is IMPORTANT to
// choose the lfPitchAndFamily variable correctly when specifying font parameters,
// because it lets you select between FF_SWISS and FF_ROMAN fonts (sans serif/serif).
int UserCharset = DEFAULT_CHARSET;

DWORD AllocationGranularity = 1; // Allocation granularity (needed when using memory-mapped files)

#ifdef USE_BETA_EXPIRATION_DATE

// specifies the first day on which this beta/PB version will no longer run
// beta/PB version 4.0 beta 1 will run only until February 1, 2020
//                                 YEAR  MONTH DAY
SYSTEMTIME BETA_EXPIRATION_DATE = {2020, 2, 0, 1, 0, 0, 0, 0};
#endif // USE_BETA_EXPIRATION_DATE

//******************************************************************************
//
// Idle processing control (CMainWindow::OnEnterIdle)
//

BOOL IdleRefreshStates = TRUE;  // initialize the state variables on the first idle pass
BOOL IdleForceRefresh = FALSE;  // bypasses the Enabler* cache
BOOL IdleCheckClipboard = TRUE; // also check the clipboard

DWORD EnablerUpDir = FALSE;
DWORD EnablerRootDir = FALSE;
DWORD EnablerForward = FALSE;
DWORD EnablerBackward = FALSE;
DWORD EnablerFileOnDisk = FALSE;
DWORD EnablerLeftFileOnDisk = FALSE;
DWORD EnablerRightFileOnDisk = FALSE;
DWORD EnablerFileOnDiskOrArchive = FALSE;
DWORD EnablerFileOrDirLinkOnDisk = FALSE;
DWORD EnablerFiles = FALSE;
DWORD EnablerFilesOnDisk = FALSE;
DWORD EnablerFilesOnDiskCompress = FALSE;
DWORD EnablerFilesOnDiskEncrypt = FALSE;
DWORD EnablerFilesOnDiskOrArchive = FALSE;
DWORD EnablerOccupiedSpace = FALSE;
DWORD EnablerFilesCopy = FALSE;
DWORD EnablerFilesMove = FALSE;
DWORD EnablerFilesDelete = FALSE;
DWORD EnablerFileDir = FALSE;
DWORD EnablerFileDirANDSelected = FALSE;
DWORD EnablerQuickRename = FALSE;
DWORD EnablerOnDisk = FALSE;
DWORD EnablerCalcDirSizes = FALSE;
DWORD EnablerPasteFiles = FALSE;
DWORD EnablerPastePath = FALSE;
DWORD EnablerPasteLinks = FALSE;
DWORD EnablerPasteSimpleFiles = FALSE;
DWORD EnablerPasteDefEffect = FALSE;
DWORD EnablerPasteFilesToArcOrFS = FALSE;
DWORD EnablerPaste = FALSE;
DWORD EnablerPasteLinksOnDisk = FALSE;
DWORD EnablerSelected = FALSE;
DWORD EnablerUnselected = FALSE;
DWORD EnablerHiddenNames = FALSE;
DWORD EnablerSelectionStored = FALSE;
DWORD EnablerGlobalSelStored = FALSE;
DWORD EnablerSelGotoPrev = FALSE;
DWORD EnablerSelGotoNext = FALSE;
DWORD EnablerLeftUpDir = FALSE;
DWORD EnablerRightUpDir = FALSE;
DWORD EnablerLeftRootDir = FALSE;
DWORD EnablerRightRootDir = FALSE;
DWORD EnablerLeftForward = FALSE;
DWORD EnablerRightForward = FALSE;
DWORD EnablerLeftBackward = FALSE;
DWORD EnablerRightBackward = FALSE;
DWORD EnablerFileHistory = FALSE;
DWORD EnablerDirHistory = FALSE;
DWORD EnablerCustomizeLeftView = FALSE;
DWORD EnablerCustomizeRightView = FALSE;
DWORD EnablerDriveInfo = FALSE;
DWORD EnablerCreateDir = FALSE;
DWORD EnablerViewFile = FALSE;
DWORD EnablerChangeAttrs = FALSE;
DWORD EnablerShowProperties = FALSE;
DWORD EnablerItemsContextMenu = FALSE;
DWORD EnablerOpenActiveFolder = FALSE;
DWORD EnablerPermissions = FALSE;

COLORREF* CurrentColors = SalamanderColors;

COLORREF UserColors[NUMBER_OF_COLORS];

SALCOLOR ViewerColors[NUMBER_OF_VIEWERCOLORS] =
    {
        RGBF(0, 0, 0, SCF_DEFAULT),       // VIEWER_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // VIEWER_BK_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // VIEWER_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT),       // VIEWER_BK_SELECTED
};

COLORREF SalamanderColors[NUMBER_OF_COLORS] =
    {
        // pen colors for the frame around the item
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_SELECTED
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_NORMAL
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_SELECTED

        // item text colors in the panel
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_NORMAL
        RGBF(255, 0, 0, 0),         // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 0, 0, 0),         // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // background colors of items in the panel
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_SELECTED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCUSED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT),       // ITEM_BK_HIGHLIGHT

        // icon blend colors
        RGBF(255, 128, 128, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),           // ICON_BLEND_FOCUSED
        RGBF(255, 0, 0, 0),               // ICON_BLEND_FOCSEL

        // progress bar colors
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // colors of hot items
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // panel title colors
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // Pen colors for the frame around thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 0, 0, 0),       // THUMBNAIL_FRAME_FOCUSED
        RGBF(255, 0, 0, 0),     // THUMBNAIL_FRAME_SELECTED
        RGBF(128, 0, 0, 0),     // THUMBNAIL_FRAME_FOCSEL
};

COLORREF ExplorerColors[NUMBER_OF_COLORS] =
    {
        // Pen colors for the frame around the item
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_NORMAL
        RGBF(255, 255, 0, 0),             // FOCUS_ACTIVE_SELECTED
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),               // FOCUS_FG_INACTIVE_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_NORMAL
        RGBF(255, 255, 0, 0),             // FOCUS_BK_INACTIVE_SELECTED

        // item text colors in the panel
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_NORMAL
        RGBF(255, 255, 255, 0),     // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 255, 0),     // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // Background colors of items in the panel
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_NORMAL
        RGBF(0, 0, 128, 0),               // ITEM_BK_SELECTED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCUSED
        RGBF(0, 0, 128, 0),               // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT),       // ITEM_BK_HIGHLIGHT

        // colors for icon blending
        RGBF(0, 0, 128, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),       // ICON_BLEND_FOCUSED
        RGBF(0, 0, 128, 0),           // ICON_BLEND_FOCSEL

        // progress bar colors
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // hot item colors
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // panel title colors
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // pen colors for the frame around thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_FOCUSED
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_SELECTED
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_FOCSEL
};

COLORREF NortonColors[NUMBER_OF_COLORS] =
    {
        // pen colors for the frame around the item
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_SELECTED
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_SELECTED
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_SELECTED

        // panel item text colors
        RGBF(0, 255, 255, 0),       // ITEM_FG_NORMAL
        RGBF(255, 255, 0, 0),       // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 0, 0),       // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // background colors of items in the panel
        RGBF(0, 0, 128, 0),         // ITEM_BK_NORMAL
        RGBF(0, 0, 128, 0),         // ITEM_BK_SELECTED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCUSED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_BK_HIGHLIGHT

        // colors for icon blending
        RGBF(255, 255, 0, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),         // ICON_BLEND_FOCUSED
        RGBF(255, 255, 0, 0),           // ICON_BLEND_FOCSEL

        // progress bar colors
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // colors of hot items
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // panel title colors
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // Pen colors for the frame around thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 128, 128, 0),   // THUMBNAIL_FRAME_FOCUSED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_SELECTED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_FOCSEL
};

COLORREF NavigatorColors[NUMBER_OF_COLORS] =
    {
        // Pen colors for the frame around the item
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_SELECTED
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_SELECTED
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_SELECTED

        // panel item text colors
        RGBF(255, 255, 255, 0),     // ITEM_FG_NORMAL
        RGBF(255, 255, 0, 0),       // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 0, 0),       // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // item background colors in the panel
        RGBF(80, 80, 80, 0),        // ITEM_BK_NORMAL
        RGBF(80, 80, 80, 0),        // ITEM_BK_SELECTED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCUSED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_BK_HIGHLIGHT

        // colors for icon blending
        RGBF(255, 255, 0, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),         // ICON_BLEND_FOCUSED
        RGBF(255, 255, 0, 0),           // ICON_BLEND_FOCSEL

        // progress bar colors
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // hot item colors
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(173, 182, 205, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(212, 212, 212, SCF_DEFAULT), // HOT_INACTIVE

        // panel title colors
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // Pen colors for the frame around thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 128, 128, 0),   // THUMBNAIL_FRAME_FOCUSED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_SELECTED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_FOCSEL
};

COLORREF CustomColors[NUMBER_OF_CUSTOMCOLORS] =
    {
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
        RGB(255, 255, 255),
};

//*****************************************************************************
//
// CRC32
//

static DWORD Crc32Tab[256];
static BOOL Crc32TabInitialized = FALSE;

void MakeCrc32Table(DWORD* crcTab)
{
    DWORD c;
    DWORD poly = 0xedb88320L; //polynomial exclusive-or pattern

    /*
      // generate CRC polynomial; using a precomputed polynomial should be faster
      // terms of the polynomial defining this CRC (except x^32):
      static const Byte p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

      // make exclusive-or pattern from the polynomial (0xedb88320L)
      poly = 0L;
      for (n = 0; n < sizeof(p)/sizeof(Byte); n++)
        poly |= 1L << (31 - p[n]);
    */
    int n;
    for (n = 0; n < 256; n++)
    {
        c = (UINT32)n;

        int k;
        for (k = 0; k < 8; k++)
            c = c & 1 ? poly ^ (c >> 1) : c >> 1;

        crcTab[n] = c;
    }
}

DWORD UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal)
{
    CALL_STACK_MESSAGE_NONE

    if (buffer == NULL)
        return 0;

    if (!Crc32TabInitialized)
    {
        MakeCrc32Table(Crc32Tab);
        Crc32TabInitialized = TRUE;
    }

    BYTE* p = (BYTE*)buffer;
    DWORD c = crcVal ^ 0xFFFFFFFF;

    if (count)
        do
        {
            c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
        } while (--count);

    // The following optimizations were benchmarked and have no practical benefit;
    // the only remaining option would be to rewrite this in ASM and read memory in DWORDs,
    // from which individual bytes could then be extracted;
    // with the current release-build settings, the compiler cannot perform this
    // optimization for us.
    /*
  int remain = count % 8;
  count -= remain;
  while (remain)
  {
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    remain--;
  }
  while (count)
  {
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    count -= 8;
  }
*/
    /*
  int remain = count % 4;
  count -= remain;
  while (remain > 0)
  {
    c = Crc32Tab[((int)c ^ (*p++)) & 0xff] ^ (c >> 8);
    remain--;
  }


  DWORD *pdw = (DWORD*)p;
  DWORD dw;
  while (count > 0)
  {
    dw = *pdw++;
    c = Crc32Tab[((int)c ^ ((BYTE)(dw))) & 0xFF] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ ((BYTE)(dw >> 8))) & 0xFF] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ ((BYTE)(dw >> 16))) & 0xFF] ^ (c >> 8);
    c = Crc32Tab[((int)c ^ ((BYTE)(dw >> 24))) & 0xFF] ^ (c >> 8);
    count -= 4;
  }
*/
    return c ^ 0xFFFFFFFF; /* (instead of ~c for 64-bit machines) */
}

BOOL IsRemoteSession(void)
{
    return GetSystemMetrics(SM_REMOTESESSION);
}

// ****************************************************************************

BOOL SalamanderIsNotBusy(DWORD* lastIdleTime)
{
    // SalamanderBusy and LastSalamanderIdleTime are accessed without critical sections, which is fine,
    // because they are DWORDs and therefore cannot be left in a "partially updated" state during a context switch
    // (the value is always either the old one or the new one, and nothing else can occur)
    if (lastIdleTime != NULL)
        *lastIdleTime = LastSalamanderIdleTime;
    if (!SalamanderBusy)
        return TRUE;
    DWORD oldLastIdleTime = LastSalamanderIdleTime;
    if (GetTickCount() - oldLastIdleTime <= 100)                                   // if SalamanderBusy has not changed for too long already (for example, because a modal dialog is open)
        Sleep(100);                                                                // wait to see if SalamanderBusy changes
    return !SalamanderBusy || (int)(LastSalamanderIdleTime - oldLastIdleTime) > 0; // not busy, or at least changing state
}

BOOL InitPreloadedStrings()
{
    DirColumnStr = DupStr(LoadStr(IDS_DIRCOLUMN));
    DirColumnStrLen = lstrlen(DirColumnStr);

    ColExtStr = DupStr(LoadStr(IDS_COLUMN_NAME_EXT));
    ColExtStrLen = lstrlen(ColExtStr);

    UpDirTypeName = DupStr(LoadStr(IDS_UPDIRTYPENAME));
    UpDirTypeNameLen = lstrlen(UpDirTypeName);

    CommonFileTypeName = DupStr(LoadStr(IDS_COMMONFILETYPE));
    CommonFileTypeNameLen = lstrlen(CommonFileTypeName);
    CommonFileTypeName2 = DupStr(LoadStr(IDS_COMMONFILETYPE2));

    ProgDlgHoursStr = DupStr(LoadStr(IDS_PROGDLGHOURS));
    ProgDlgMinutesStr = DupStr(LoadStr(IDS_PROGDLGMINUTES));
    ProgDlgSecsStr = DupStr(LoadStr(IDS_PROGDLGSECS));

    return TRUE;
}

void ReleasePreloadedStrings()
{
    if (DirColumnStr != NULL)
        free((void*)DirColumnStr);
    if (ColExtStr != NULL)
        free((void*)ColExtStr);

    if (UpDirTypeName != NULL)
        free((void*)UpDirTypeName);

    if (CommonFileTypeName != NULL)
        free((void*)CommonFileTypeName);
    if (CommonFileTypeName2 != NULL)
        free((void*)CommonFileTypeName2);

    if (ProgDlgHoursStr != NULL)
        free((void*)ProgDlgHoursStr);
    if (ProgDlgMinutesStr != NULL)
        free((void*)ProgDlgMinutesStr);
    if (ProgDlgSecsStr != NULL)
        free((void*)ProgDlgSecsStr);

    DirColumnStr = NULL;
    ColExtStr = NULL;

    UpDirTypeName = NULL;

    CommonFileTypeName = NULL;
    CommonFileTypeName2 = NULL;

    ProgDlgHoursStr = NULL;
    ProgDlgMinutesStr = NULL;
    ProgDlgSecsStr = NULL;
}

// ****************************************************************************

void InitLocales()
{
    int i;
    for (i = 0; i < 256; i++)
    {
        IsNotAlphaNorNum[i] = !IsCharAlphaNumeric((char)i);
        IsAlpha[i] = IsCharAlpha((char)i);
    }

    if ((DecimalSeparatorLen = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SDECIMAL, DecimalSeparator, 5)) == 0 ||
        DecimalSeparatorLen > 5)
    {
        strcpy(DecimalSeparator, ".");
        DecimalSeparatorLen = 1;
    }
    else
    {
        DecimalSeparatorLen--;
        DecimalSeparator[DecimalSeparatorLen] = 0; // make sure the string is null-terminated
    }

    if ((ThousandsSeparatorLen = GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_STHOUSAND, ThousandsSeparator, 5)) == 0 ||
        ThousandsSeparatorLen > 5)
    {
        strcpy(ThousandsSeparator, " ");
        ThousandsSeparatorLen = 1;
    }
    else
    {
        ThousandsSeparatorLen--;
        ThousandsSeparator[ThousandsSeparatorLen] = 0; // ensure the null terminator stays at the end
    }
}

// ****************************************************************************

HICON GetFileOrPathIconAux(const char* path, BOOL large, BOOL isDir)
{
    __try
    {
        SHFILEINFO shi;
        if (!GetFileIcon(path, FALSE, &shi.hIcon, large ? ICONSIZE_32 : ICONSIZE_16, TRUE, isDir))
            shi.hIcon = NULL;
        // We switched to our own implementation (lower memory use, and XOR icons work correctly)
        //shi.hIcon = NULL;
        //SHGetFileInfo(path, 0, &shi, sizeof(shi),
        //              SHGFI_ICON | SHGFI_SHELLICONSIZE | (large ? 0 : SHGFI_SMALLICON));
        // add the handle for 'shi.hIcon' to HANDLES
        if (shi.hIcon != NULL)
            HANDLES_ADD(__htIcon, __hoLoadImage, shi.hIcon);
        return shi.hIcon;
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 13))
    {
        FGIExceptionHasOccured++;
    }
    return NULL;
}

HICON GetDriveIcon(const char* root, UINT type, BOOL accessible, BOOL large)
{
    CALL_STACK_MESSAGE5("GetDriveIcon(%s, %u, %d, %d)", root, type, accessible, large);
    int id;
    switch (type)
    {
    case DRIVE_REMOVABLE: // 3.5" and 5.25" drive icons
    {
        HICON i = GetFileOrPathIconAux(root, large, TRUE);
        if (i != NULL)
            return i;
        id = 28; // 3.5" drive
        break;
    }

    case DRIVE_REMOTE:
        id = (accessible ? 33 : 31);
        break;
    case DRIVE_CDROM:
        id = 30;
        break;
    case DRIVE_RAMDISK:
        id = 34;
        break;

    default:
    {
        id = 32;
        if (type == DRIVE_FIXED && root[1] == ':')
        {
            char win[MAX_PATH];
            if (GetWindowsDirectory(win, MAX_PATH) && win[1] == ':' && win[0] == root[0])
                id = 36;
        }
        break;
    }
    }
    int iconSize = IconSizes[large ? ICONSIZE_32 : ICONSIZE_16];
    return SalLoadIcon(ImageResDLL, id, iconSize);

    // JRYFIXME - investigate whether IconLRFlags should be removed. (W7+)

    /* JRYFIXME - grep the source tree for LoadImage / IMAGE_ICON
      return (HICON)HANDLES(LoadImage(ImageResDLL, MAKEINTRESOURCE(id), IMAGE_ICON,
                                      large ? ICON32_CX : ICON16_CX,
                                      large ? ICON32_CX : ICON16_CX,
                                      IconLRFlags));
      */
}

HICON SalLoadIcon(HINSTANCE hDLL, int id, int iconSize)
{
    //return (HICON)HANDLES(LoadImage(hDLL, MAKEINTRESOURCE(id), IMAGE_ICON, iconSize, iconSize, IconLRFlags));
    HICON hIcon;
    LoadIconWithScaleDown(hDLL, MAKEINTRESOURCEW(id), iconSize, iconSize, &hIcon);
    HANDLES_ADD(__htIcon, __hoLoadImage, hIcon);
    return hIcon;
}

// ****************************************************************************

char* BuildName(char* path, char* name, char* dosName, BOOL* skip, BOOL* skipAll, const char* sourcePath)
{
    if (skip != NULL)
        *skip = FALSE;
    int l1 = (int)strlen(path); // Always on the stack...
    int l2, len = l1;
    if (name != NULL)
    {
        l2 = (int)strlen(name);
        len += l2;
        if (path[l1 - 1] != '\\')
            len++;
        if (len >= MAX_PATH && dosName != NULL)
        {
            int l3 = (int)strlen(dosName);
            if (len - l2 + l3 < MAX_PATH)
            {
                len = len - l2 + l3;
                name = dosName;
                l2 = l3;
            }
        }
    }
    if (len >= MAX_PATH)
    {
        char text[2 * MAX_PATH + 100];
        _snprintf_s(text, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG), name, path);

        if (skip != NULL)
        {
            if (skipAll == NULL || !*skipAll)
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = MainWindow->HWindow;
                params.Flags = MSGBOXEX_YESNOOKCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
                params.Caption = LoadStr(IDS_ERRORTITLE);
                params.Text = text;
                char aliasBtnNames[200];
                /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
                   for message box buttons, let hotkey collisions be handled by pretending they are a menu
                MENU_TEMPLATE_ITEM MsgBoxButtons[] =
                {
                  {MNTT_PB, 0
                  {MNTT_IT, IDS_MSGBOXBTN_SKIP
                  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
                  {MNTT_IT, IDS_MSGBOXBTN_FOCUS
                  {MNTT_PE, 0
                };
                */
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                        DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL),
                        DIALOG_OK, LoadStr(IDS_MSGBOXBTN_FOCUS));
                params.AliasBtnNames = aliasBtnNames;
                int msgRes = SalMessageBoxEx(&params);
                if (msgRes == DIALOG_YES /* Skip */ || msgRes == DIALOG_NO /* Skip All */)
                    *skip = TRUE;
                if (msgRes == DIALOG_NO /* Skip All */ && skipAll != NULL)
                    *skipAll = TRUE;
                if (msgRes == DIALOG_OK /* Focus */)
                    MainWindow->PostFocusNameInPanel(PANEL_SOURCE, sourcePath, name);
            }
            else
                *skip = TRUE;
        }
        else
        {
            SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
        }
        return NULL;
    }
    char* txt = (char*)malloc(len + 1);
    if (txt == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return txt;
    }
    if (name != NULL)
    {
        memmove(txt, path, l1);
        if (path[l1 - 1] != '\\')
            txt[l1++] = '\\';
        memmove(txt + l1, name, l2 + 1);
    }
    else
        memmove(txt, path, l1 + 1);
    return txt;
}

// ****************************************************************************

BOOL HasTheSameRootPath(const char* path1, const char* path2)
{
    if (LowerCase[path1[0]] == LowerCase[path2[0]] && path1[1] == path2[1])
    {
        if (path1[1] == ':')
            return TRUE; // same root for regular ("c:\path") paths
        else
        {
            if (path1[0] == '\\' && path1[1] == '\\') // both UNC paths
            {
                const char* s1 = path1 + 2;
                const char* s2 = path2 + 2;
                while (*s1 != 0 && *s1 != '\\')
                {
                    if (LowerCase[*s1] == LowerCase[*s2])
                    {
                        s1++;
                        s2++;
                    }
                    else
                        break; // different machines
                }
                if (*s1 != 0 && *s1++ == *s2++) // skip '\\'
                {
                    while (*s1 != 0 && *s1 != '\\')
                    {
                        if (LowerCase[*s1] == LowerCase[*s2])
                        {
                            s1++;
                            s2++;
                        }
                        else
                            break; // different shares
                    }
                    return (*s1 == 0 && (*s2 == 0 || *s2 == '\\')) || *s1 == *s2 ||
                           (*s2 == 0 && (*s1 == 0 || *s1 == '\\'));
                }
            }
        }
    }
    return FALSE;
}

// ****************************************************************************

BOOL HasTheSameRootPathAndVolume(const char* p1, const char* p2)
{
    CALL_STACK_MESSAGE3("HasTheSameRootPathAndVolume(%s, %s)", p1, p2);

    BOOL ret = FALSE;
    if (HasTheSameRootPath(p1, p2))
    {
        ret = TRUE;
        char root[MAX_PATH];
        char ourPath[MAX_PATH];
        char p1Volume[100] = "1";
        char p2Volume[100] = "2";
        char resPath[MAX_PATH];
        lstrcpyn(resPath, p1, MAX_PATH);
        ResolveSubsts(resPath);
        GetRootPath(root, resPath);
        if (!IsUNCPath(root) && GetDriveType(root) == DRIVE_FIXED) // only look for reparse points on fixed drives
        {
            // if this is not a root path, also try traversing through reparse points
            BOOL cutPathIsPossible = TRUE;
            char p1NetPath[MAX_PATH];
            p1NetPath[0] = 0;
            ResolveLocalPathWithReparsePoints(ourPath, p1, &cutPathIsPossible, NULL, NULL, NULL, NULL, p1NetPath);

            if (p1NetPath[0] == 0) // The volume cannot be obtained from a network path, so do not even try
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p1Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    {
                        strcpy(p1Volume, "fail"); // Even the root path failed, unexpectedly (unfortunately this happens on SUBST drives under W2K, debugged at Bachaalany; if both paths fail, we treat them as matching because that is more likely)
                        break;
                    }
                    SalPathAddBackslash(ourPath, MAX_PATH);
                }
            }

            // if it is not a root path, try traversing the reparse points as well
            cutPathIsPossible = TRUE;
            char p2NetPath[MAX_PATH];
            p2NetPath[0] = 0;
            ResolveLocalPathWithReparsePoints(ourPath, p2, &cutPathIsPossible, NULL, NULL, NULL, NULL, p2NetPath);

            if ((p1NetPath[0] == 0) != (p2NetPath[0] == 0) || // if only one of the paths is a network path, or
                p1NetPath[0] != 0 && !HasTheSameRootPath(p1NetPath, p2NetPath))
                ret = FALSE; // Different root path, so treat them as different volumes (volumes cannot be checked on network paths)

            if (p2NetPath[0] == 0 && ret) // A volume cannot be obtained from a network path, so do not even try; also skip it if the result is already decided
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p2Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    {
                        strcpy(p2Volume, "fail"); // Even the root path failed, unexpectedly (unfortunately this happens on SUBST drives under W2K, debugged at Bachaalany; if both paths fail, we treat them as matching because that is more likely)
                        break;
                    }
                    SalPathAddBackslash(ourPath, MAX_PATH);
                }
                if (strcmp(p1Volume, p2Volume) != 0)
                    ret = FALSE;
            }
        }
    }
    return ret;
}

// ****************************************************************************

BOOL PathsAreOnTheSameVolume(const char* path1, const char* path2, BOOL* resIsOnlyEstimation)
{
    char root1[MAX_PATH];
    char root2[MAX_PATH];
    char ourPath[MAX_PATH];
    char path1NetPath[MAX_PATH];
    char path2NetPath[MAX_PATH];
    lstrcpyn(ourPath, path1, MAX_PATH);
    ResolveSubsts(ourPath);
    GetRootPath(root1, ourPath);
    lstrcpyn(ourPath, path2, MAX_PATH);
    ResolveSubsts(ourPath);
    GetRootPath(root2, ourPath);
    BOOL ret = TRUE;
    BOOL trySimpleTest = TRUE;
    if (resIsOnlyEstimation != NULL)
        *resIsOnlyEstimation = TRUE;
    if (!IsUNCPath(path1) && !IsUNCPath(path2)) // No point checking volumes for UNC paths
    {
        char p1Volume[100] = "1";
        char p2Volume[100] = "2";
        UINT drvType1 = GetDriveType(root1);
        UINT drvType2 = GetDriveType(root2);
        if (drvType1 != DRIVE_REMOTE && drvType2 != DRIVE_REMOTE) // except on network paths, the "volume name" can be determined
        {
            BOOL cutPathIsPossible = TRUE;
            path1NetPath[0] = 0;         // network path that the current (last) local symlink in the path points to
            if (drvType1 == DRIVE_FIXED) // only look for reparse points on fixed drives
            {
                // if it is not a root path, try traversing the reparse points as well
                ResolveLocalPathWithReparsePoints(ourPath, path1, &cutPathIsPossible, NULL, NULL, NULL, NULL, path1NetPath);
            }
            else
                lstrcpyn(ourPath, root1, MAX_PATH);
            int numOfGetVolNamesFailed = 0;
            if (path1NetPath[0] == 0) // Volume names cannot be obtained for network paths, so do not even try.
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p1Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    { // Even the root path failed, which is unexpected (unfortunately this happens on SUBST drives under W2K, debugged at Bachaalany; if both paths with the same root fail, treat them as the same volume because that is more likely)
                        numOfGetVolNamesFailed++;
                        break;
                    }
                    SalPathAddBackslash(ourPath, MAX_PATH);
                }
            }

            cutPathIsPossible = TRUE;
            path2NetPath[0] = 0;         // network path the current (last) local symlink in the path leads to
            if (drvType2 == DRIVE_FIXED) // only look for reparse points on fixed drives
            {
                // if it is not a root path, try traversing the reparse points as well
                ResolveLocalPathWithReparsePoints(ourPath, path2, &cutPathIsPossible, NULL, NULL, NULL, NULL, path2NetPath);
            }
            else
                lstrcpyn(ourPath, root2, MAX_PATH);
            if (path2NetPath[0] == 0) // Volume names cannot be obtained for network paths, so do not even try.
            {
                if (path1NetPath[0] == 0)
                {
                    while (!GetVolumeNameForVolumeMountPoint(ourPath, p2Volume, 100))
                    {
                        if (!cutPathIsPossible || !CutDirectory(ourPath))
                        { // Even the root path failed, which is unexpected (unfortunately this happens on SUBST drives under W2K, debugged at Bachaalany; if both paths with the same root fail, treat them as the same volume because that is more likely)
                            numOfGetVolNamesFailed++;
                            break;
                        }
                        SalPathAddBackslash(ourPath, MAX_PATH);
                    }
                    if (numOfGetVolNamesFailed != 2)
                    {
                        if (numOfGetVolNamesFailed == 0 && resIsOnlyEstimation != NULL)
                            *resIsOnlyEstimation = FALSE; // The only time we know the result for sure is when we managed to get the "volume name" for both paths (which also means neither path could have been a network path).
                        if (numOfGetVolNamesFailed == 1 || strcmp(p1Volume, p2Volume) != 0)
                            ret = FALSE; // Only one volume name was obtained, so we must treat them as different volumes (and if they are actually the same, we cannot detect it; if the failure was caused by SUBST, this might be fixable by resolving the SUBST target path)
                        trySimpleTest = FALSE;
                    }
                }
                else // Only one of the paths is a network path, so they are not on the same volume (and if they are, we cannot determine that).
                {
                    ret = FALSE;
                    trySimpleTest = FALSE;
                }
            }
            else
            {
                if (path1NetPath[0] != 0) // compare the roots of the network paths
                {
                    GetRootPath(root1, path1NetPath);
                    GetRootPath(root2, path2NetPath);
                }
                else // Only one of the paths is a network path, so they are not on the same volume (and if they are, we cannot determine that).
                {
                    ret = FALSE;
                    trySimpleTest = FALSE;
                }
            }
        }
    }

    if (trySimpleTest) // Only test whether the root paths match (network paths and everything on NT)
    {
        ret = _stricmp(root1, root2) == 0;

        if (resIsOnlyEstimation != NULL)
        {
            lstrcpyn(path1NetPath, path1, MAX_PATH);
            lstrcpyn(path2NetPath, path2, MAX_PATH);
            if (ResolveSubsts(path1NetPath) && ResolveSubsts(path2NetPath))
            {
                if (IsTheSamePath(path1NetPath, path2NetPath))
                    *resIsOnlyEstimation = FALSE; // same paths definitely mean the same volumes
            }
        }
    }
    return ret;
}

// ****************************************************************************

BOOL IsTheSamePath(const char* path1, const char* path2)
{
    if (*path1 == '\\')
        path1++;
    if (*path2 == '\\')
        path2++;
    while (*path1 != 0 && LowerCase[*path1] == LowerCase[*path2])
    {
        path1++;
        path2++;
    }
    if (*path1 == '\\')
        path1++;
    if (*path2 == '\\')
        path2++;
    return *path1 == 0 && *path2 == 0;
}

// ****************************************************************************

int CommonPrefixLength(const char* path1, const char* path2)
{
    const char* lastBackslash = path1;
    int backslashCount = 0;
    int sameCount = 0;
    const char* s1 = path1;
    const char* s2 = path2;
    while (*s1 != 0 && *s2 != 0 && LowerCase[*s1] == LowerCase[*s2])
    {
        if (*s1 == '\\')
        {
            lastBackslash = s1;
            backslashCount++;
        }
        s1++;
        s2++;
    }

    if (s1 - path1 < 3)
        return 0;

    if (*s1 == 0 && *s2 == '\\' || *s1 == '\\' && *s2 == 0 ||
        *s1 == 0 && *s2 == 0 && *(s1 - 1) != '\\')
    {
        lastBackslash = s1; // points at the terminator, but the terminator itself is not included
        backslashCount++;
    }

    if (path1[1] == ':')
    {
        // drive-letter path
        if (path1[2] != '\\')
            return 0;

        // Handle a special case: for a root path, we must return the length including the trailing backslash.
        if (lastBackslash - path1 < 3)
            return 3;

        return (int)(lastBackslash - path1);
    }
    else
    {
        // UNC path
        if (path1[0] != '\\' || path1[1] != '\\')
            return 0;
        if (backslashCount < 4) // the path must have the form "\\machine\share"
            return 0;

        return (int)(lastBackslash - path1);
    }
}

// ****************************************************************************

BOOL SalPathIsPrefix(const char* prefix, const char* path)
{
    int commonLen = CommonPrefixLength(prefix, path);
    if (commonLen == 0)
        return FALSE;

    int prefixLen = (int)strlen(prefix);
    if (prefixLen < 3)
        return FALSE;

    // CommonPrefixLength returned the length without the trailing backslash (unless this was a root path)
    // if our prefix ends with a backslash, we have to drop it
    if (prefixLen > 3 && prefix[prefixLen - 1] == '\\')
        prefixLen--;

    return (commonLen == prefixLen);
}

// ****************************************************************************

BOOL IsDirError(DWORD err)
{
    return err == ERROR_NETWORK_ACCESS_DENIED ||
           err == ERROR_ACCESS_DENIED ||
           err == ERROR_SECTOR_NOT_FOUND ||
           err == ERROR_SHARING_VIOLATION ||
           err == ERROR_BAD_PATHNAME ||
           err == ERROR_FILE_NOT_FOUND ||
           err == ERROR_PATH_NOT_FOUND ||
           err == ERROR_INVALID_NAME ||   // On English Windows, if the path contains characters with diacritics, this error is reported instead of ERROR_PATH_NOT_FOUND
           err == ERROR_INVALID_FUNCTION; // Reported by one user on WinXP on network drive Y: Salamander hit this when accessing a path that no longer existed (so the path was not shortened, which caused serious trouble; Shift+F7 on Y:\ fixed it)
}

// ****************************************************************************

BOOL CutDirectory(char* path, char** cutDir)
{
    CALL_STACK_MESSAGE2("CutDirectory(%s,)", path);
    int l = (int)strlen(path);
    char* lastBackslash = path + l - 1;
    while (--lastBackslash >= path && *lastBackslash != '\\')
        ;
    char* nextBackslash = lastBackslash;
    while (--nextBackslash >= path && *nextBackslash != '\\')
        ;
    if (lastBackslash < path)
    {
        if (cutDir != NULL)
            *cutDir = path + l;
        return FALSE; // "somedir" or "c:\"
    }
    if (nextBackslash < path) // "c:\somedir" or "c:\somedir\"
    {
        if (cutDir != NULL)
        {
            if (*(path + l - 1) == '\\')
                *(path + --l) = 0; // Remove trailing '\\'
            memmove(lastBackslash + 2, lastBackslash + 1, l - (lastBackslash - path));
            *cutDir = lastBackslash + 2; // "somedir" or "seconddir"
        }
        *(lastBackslash + 1) = 0; // "c:\"
    }
    else // "c:\firstdir\seconddir" or "c:\firstdir\seconddir\"
    {    // UNC: "\\server\share\path"
        if (path[0] == '\\' && path[1] == '\\' && nextBackslash <= path + 2)
        { // "\\server\share" - cannot be shortened further
            if (cutDir != NULL)
                *cutDir = path + l;
            return FALSE;
        }
        *lastBackslash = 0;
        if (cutDir != NULL) // remove trailing '\\'
        {
            if (*(path + l - 1) == '\\')
                *(path + l - 1) = 0;
            *cutDir = lastBackslash + 1;
        }
    }
    return TRUE;
}

// ****************************************************************************

int GetRootPath(char* root, const char* path)
{                                           // WARNING: special-case behavior for GetShellFolder(): for "\\\\" it returns "\\\\\\", and for "\\\\server" it returns "\\\\server\\"
    if (path[0] == '\\' && path[1] == '\\') // UNC
    {
        const char* s = path + 2;
        while (*s != 0 && *s != '\\')
            s++;
        if (*s != 0)
            s++; // '\\'
        while (*s != 0 && *s != '\\')
            s++;
        int len = (int)(s - path);
        if (len > MAX_PATH - 2)
            len = MAX_PATH - 2; // so it fits into the MAX_PATH buffer even with '\\' (expected size); truncation is fine, this is almost certainly an error anyway
        memcpy(root, path, len);
        root[len] = '\\';
        root[len + 1] = 0;
        return len + 1;
    }
    else
    {
        root[0] = path[0];
        root[1] = ':';
        root[2] = '\\';
        root[3] = 0;
        return 3;
    }
}

// ****************************************************************************

// Walks through all configured colors and assigns appropriate values
// to any entries still set to their default value.

COLORREF GetHilightColor(COLORREF clr1, COLORREF clr2)
{
    WORD h1, l1, s1;
    ColorRGBToHLS(clr1, &h1, &l1, &s1);
    BYTE gray1 = GetGrayscaleFromRGB(GetRValue(clr1), GetGValue(clr1), GetBValue(clr1));
    BYTE gray2 = GetGrayscaleFromRGB(GetRValue(clr2), GetGValue(clr2), GetBValue(clr2));
    COLORREF res;
    if (gray2 < 170 && gray1 <= 220)
    {
        unsigned wantedGray = (unsigned)gray1 + 20 + (220 - (unsigned)gray1) / 2;
        if (wantedGray < (unsigned)gray2 + 100)
            wantedGray = (unsigned)gray2 + 100;
        if (wantedGray > 255)
            wantedGray = 255;
        BOOL first = TRUE;
        while (first || l1 != 240)
        {
            first = FALSE;
            l1 += 5;
            if (l1 > 240)
                l1 = 240;
            res = ColorHLSToRGB(h1, l1, s1);
            if ((unsigned)GetGrayscaleFromRGB(GetRValue(res), GetGValue(res), GetBValue(res)) >= wantedGray)
                break;
        }
    }
    else
    {
        if ((gray1 >= gray2 ? gray1 - gray2 : gray2 - gray1) > 85 ||
            gray2 < 85 ||
            gray1 < 75)
        {
            if (gray1 > gray2)
            {
                res = RGB((4 * (unsigned)GetRValue(clr1) + 3 * (unsigned)GetRValue(clr2)) / 7,
                          (4 * (unsigned)GetGValue(clr1) + 3 * (unsigned)GetGValue(clr2)) / 7,
                          (4 * (unsigned)GetBValue(clr1) + 3 * (unsigned)GetBValue(clr2)) / 7);
            }
            else
            {
                res = RGB(((unsigned)GetRValue(clr1) + (unsigned)GetRValue(clr2)) / 2,
                          ((unsigned)GetGValue(clr1) + (unsigned)GetGValue(clr2)) / 2,
                          ((unsigned)GetBValue(clr1) + (unsigned)GetBValue(clr2)) / 2);
            }
        }
        else
        {
            res = RGB(0, 0, 0);
        }
    }
    return res;
}

COLORREF GetFullRowHighlight(COLORREF bkHighlightColor) // Returns a heuristic highlight color for full-row mode.
{
    // A bit of heuristic tweaking: slightly darken light backgrounds and slightly brighten dark ones
    WORD h, l, s;
    ColorRGBToHLS(bkHighlightColor, &h, &l, &s);

    if (l < 121) // [DARK] 0-120 -> progressively brighten luminance: 0..120 -> +40..+20
        l += 20 + 20 * (120 - l) / 120;
    else // [LIGHT] 121-240 -> reduce luminance by a constant 20
        l -= 20;

    return ColorHLSToRGB(h, l, s);
}

void UpdateDefaultColors(SALCOLOR* colors, CHighlightMasks* highlightMasks, BOOL processColors, BOOL processMasks)
{
    if (processColors)
    {
        int bitsPerPixel = GetCurrentBPP();

        // Take the pen colors for the frame around the item from the system window text color.
        if (GetFValue(colors[FOCUS_ACTIVE_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_ACTIVE_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[FOCUS_ACTIVE_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_ACTIVE_SELECTED], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[FOCUS_BK_INACTIVE_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_BK_INACTIVE_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[FOCUS_BK_INACTIVE_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_BK_INACTIVE_SELECTED], GetSysColor(COLOR_WINDOW));

        // Use the system window text color for panel item text
        if (GetFValue(colors[ITEM_FG_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[ITEM_FG_FOCUSED]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_FG_FOCUSED], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[ITEM_FG_HIGHLIGHT]) & SCF_DEFAULT) // FULL ROW HIGHLIGHT is derived from _NORMAL
            SetRGBPart(&colors[ITEM_FG_HIGHLIGHT], GetCOLORREF(colors[ITEM_FG_NORMAL]));

        // use the system window background color for panel item backgrounds
        if (GetFValue(colors[ITEM_BK_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_BK_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[ITEM_BK_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_BK_SELECTED], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[ITEM_BK_HIGHLIGHT]) & SCF_DEFAULT) // Derive HIGHLIGHT from NORMAL (so custom/Norton modes also work)
            SetRGBPart(&colors[ITEM_BK_HIGHLIGHT], GetFullRowHighlight(GetCOLORREF(colors[ITEM_BK_NORMAL])));

        // progress bar colors
        if (GetFValue(colors[PROGRESS_FG_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[PROGRESS_FG_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_FG_SELECTED], GetSysColor(COLOR_HIGHLIGHTTEXT));
        if (GetFValue(colors[PROGRESS_BK_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_BK_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[PROGRESS_BK_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_BK_SELECTED], GetSysColor(COLOR_HIGHLIGHT));

        // selected icon tint color
        if (GetFValue(colors[ICON_BLEND_SELECTED]) & SCF_DEFAULT)
        {
            // By default, copy the selected color from the focused+selected state
            SetRGBPart(&colors[ICON_BLEND_SELECTED], GetCOLORREF(colors[ICON_BLEND_FOCSEL]));
            // If the color is red (the Salamander profile, and the color depth allows it),
            // use a lighter shade for the selected state.
            if (bitsPerPixel > 8 && GetCOLORREF(colors[ICON_BLEND_FOCSEL]) == RGB(255, 0, 0))
                SetRGBPart(&colors[ICON_BLEND_SELECTED], RGB(255, 128, 128));
        }

#define COLOR_HOTLIGHT 26 // winuser.h

        // panel captions (active/inactive)

        // active panel caption: BACKGROUND
        if (GetFValue(colors[ACTIVE_CAPTION_BK]) & SCF_DEFAULT)
            SetRGBPart(&colors[ACTIVE_CAPTION_BK], GetSysColor(COLOR_ACTIVECAPTION));
        // active panel caption: TEXT
        if (GetFValue(colors[ACTIVE_CAPTION_FG]) & SCF_DEFAULT)
            SetRGBPart(&colors[ACTIVE_CAPTION_FG], GetSysColor(COLOR_CAPTIONTEXT));
        // inactive panel caption background
        if (GetFValue(colors[INACTIVE_CAPTION_BK]) & SCF_DEFAULT)
            SetRGBPart(&colors[INACTIVE_CAPTION_BK], GetSysColor(COLOR_INACTIVECAPTION));
        // inactive panel caption: TEXT
        if (GetFValue(colors[INACTIVE_CAPTION_FG]) & SCF_DEFAULT)
        {
            // We prefer the same text color as for the active caption, but sometimes that color is too
            // close to the background color, so we try the inactive caption text color instead
            COLORREF clrBk = GetCOLORREF(colors[INACTIVE_CAPTION_BK]);
            COLORREF clrFgAc = GetSysColor(COLOR_CAPTIONTEXT);
            COLORREF clrFgIn = GetSysColor(COLOR_INACTIVECAPTIONTEXT);
            BYTE grayBk = GetGrayscaleFromRGB(GetRValue(clrBk), GetGValue(clrBk), GetBValue(clrBk));
            BYTE grayFgAc = GetGrayscaleFromRGB(GetRValue(clrFgAc), GetGValue(clrFgAc), GetBValue(clrFgAc));
            BYTE grayFgIn = GetGrayscaleFromRGB(GetRValue(clrFgIn), GetGValue(clrFgIn), GetBValue(clrFgIn));
            SetRGBPart(&colors[INACTIVE_CAPTION_FG], (abs(grayFgAc - grayBk) >= abs(grayFgIn - grayBk)) ? clrFgAc : clrFgIn);
        }

        // hot item colors
        COLORREF hotColor = GetSysColor(COLOR_HOTLIGHT);
        if (GetFValue(colors[HOT_PANEL]) & SCF_DEFAULT)
            SetRGBPart(&colors[HOT_PANEL], hotColor);

        // highlight for the active panel caption
        if (GetFValue(colors[HOT_ACTIVE]) & SCF_DEFAULT)
        {
            COLORREF clr = GetCOLORREF(colors[ACTIVE_CAPTION_FG]);
            if (bitsPerPixel > 4)
                clr = GetHilightColor(clr, GetCOLORREF(colors[ACTIVE_CAPTION_BK]));
            SetRGBPart(&colors[HOT_ACTIVE], clr);
        }
        // highlight color for inactive panel caption
        if (GetFValue(colors[HOT_INACTIVE]) & SCF_DEFAULT)
        {
            COLORREF clr = GetCOLORREF(colors[INACTIVE_CAPTION_FG]);
            if (bitsPerPixel > 4)
                clr = GetHilightColor(clr, GetCOLORREF(colors[INACTIVE_CAPTION_BK]));
            SetRGBPart(&colors[HOT_INACTIVE], clr);
        }
    }

    if (processMasks)
    {
        // colors based on the file name and attributes
        int i;
        for (i = 0; i < highlightMasks->Count; i++)
        {
            CHighlightMasksItem* item = highlightMasks->At(i);
            if (GetFValue(item->NormalFg) & SCF_DEFAULT)
                SetRGBPart(&item->NormalFg, GetCOLORREF(colors[ITEM_FG_NORMAL]));
            if (GetFValue(item->NormalBk) & SCF_DEFAULT)
                SetRGBPart(&item->NormalBk, GetCOLORREF(colors[ITEM_BK_NORMAL]));
            if (GetFValue(item->FocusedFg) & SCF_DEFAULT)
                SetRGBPart(&item->FocusedFg, GetCOLORREF(colors[ITEM_FG_FOCUSED]));
            if (GetFValue(item->FocusedBk) & SCF_DEFAULT)
                SetRGBPart(&item->FocusedBk, GetCOLORREF(colors[ITEM_BK_FOCUSED]));
            if (GetFValue(item->SelectedFg) & SCF_DEFAULT)
                SetRGBPart(&item->SelectedFg, GetCOLORREF(colors[ITEM_FG_SELECTED]));
            if (GetFValue(item->SelectedBk) & SCF_DEFAULT)
                SetRGBPart(&item->SelectedBk, GetCOLORREF(colors[ITEM_BK_SELECTED]));
            if (GetFValue(item->FocSelFg) & SCF_DEFAULT)
                SetRGBPart(&item->FocSelFg, GetCOLORREF(colors[ITEM_FG_FOCSEL]));
            if (GetFValue(item->FocSelBk) & SCF_DEFAULT)
                SetRGBPart(&item->FocSelBk, GetCOLORREF(colors[ITEM_BK_FOCSEL]));
            if (GetFValue(item->HighlightFg) & SCF_DEFAULT)
                SetRGBPart(&item->HighlightFg, GetCOLORREF(item->NormalFg));
            if (GetFValue(item->HighlightBk) & SCF_DEFAULT) // FULL ROW HIGHLIGHT is derived from _NORMAL
                SetRGBPart(&item->HighlightBk, GetFullRowHighlight(GetCOLORREF(item->NormalBk)));
        }
    }
}

//****************************************************************************
//
// Determine whether to use 256-color or 16-color bitmaps based on the
// display color depth.
//

BOOL Use256ColorsBitmap()
{
    int bitsPerPixel = GetCurrentBPP();
    return (bitsPerPixel > 8); // more than 256 colors
}

DWORD GetImageListColorFlags()
{
    // If the image list uses 16-bit color depth, the alpha channel of new icons misbehaves on a 32-bit color display under WinXP (32-bit depth works correctly).
    // If the image list uses 32-bit color depth, blending when drawing a selected item misbehaves on a 32-bit color display under Win2K (16-bit depth works correctly).
    return ILC_COLOR32;
}

// ****************************************************************************

int LoadColorTable(int id, RGBQUAD* rgb, int rgbCount)
{
    int count = 0;
    HRSRC hRsrc = FindResource(HInstance, MAKEINTRESOURCE(id), RT_RCDATA);
    if (hRsrc)
    {
        void* data = LoadResource(HInstance, hRsrc);
        if (data)
        {
            DWORD size = SizeofResource(HInstance, hRsrc);
            if (size > 0)
            {
                int max = min(rgbCount, (WORD)size / 3);
                BYTE* ptr = (BYTE*)data;
                int i;
                for (i = 0; i < max; i++)
                {
                    rgb[i].rgbBlue = *ptr++;
                    rgb[i].rgbGreen = *ptr++;
                    rgb[i].rgbRed = *ptr++;
                    rgb[i].rgbReserved = 0;
                    count++;
                }
            }
        }
    }
    return count;
}

BOOL InitializeConstGraphics()
{
    // Ensure smooth graphics output.
    // Batching up to 20 GDI API calls should be more than enough.
    // This is the default value in Windows NT 4.0 Workstation.
    if (GdiGetBatchLimit() < 20)
    {
        TRACE_I("Increasing GdiBatchLimit");
        GdiSetBatchLimit(20);
    }

    if (LoadColorTable(IDC_COLORTABLE, ColorTable, 256) != 256)
    {
        TRACE_E("Loading ColorTable failed");
        return FALSE;
    }

    if (SystemParametersInfo(SPI_GETDRAGFULLWINDOWS, 0, &DragFullWindows, FALSE) == 0)
        DragFullWindows = TRUE;

    // Initialize the LOGFONT structure
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
    LogFont = ncm.lfStatusFont;
    /*
  LogFont.lfHeight = -10;
  LogFont.lfWidth = 0;
  LogFont.lfEscapement = 0;
  LogFont.lfOrientation = 0;
  LogFont.lfWeight = FW_NORMAL;
  LogFont.lfItalic = 0;
  LogFont.lfUnderline = 0;
  LogFont.lfStrikeOut = 0;
  LogFont.lfCharSet = UserCharset;
  LogFont.lfOutPrecision = OUT_DEFAULT_PRECIS;
  LogFont.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  LogFont.lfQuality = DEFAULT_QUALITY;
  LogFont.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
  strcpy(LogFont.lfFaceName, "MS Shell Dlg 2");
  */

    // These brushes are allocated by the system and update automatically when the colors change.
    HDialogBrush = GetSysColorBrush(COLOR_BTNFACE);
    HButtonTextBrush = GetSysColorBrush(COLOR_BTNTEXT);
    HMenuSelectedBkBrush = GetSysColorBrush(COLOR_HIGHLIGHT);
    HMenuSelectedTextBrush = GetSysColorBrush(COLOR_HIGHLIGHTTEXT);
    HMenuHilightBrush = GetSysColorBrush(COLOR_3DHILIGHT);
    HMenuGrayTextBrush = GetSysColorBrush(COLOR_3DSHADOW);
    if (HDialogBrush == NULL || HButtonTextBrush == NULL ||
        HMenuSelectedTextBrush == NULL || HMenuHilightBrush == NULL ||
        HMenuGrayTextBrush == NULL)
    {
        TRACE_E("Unable to create brush.");
        return FALSE;
    }
    ItemBitmap.CreateBmp(NULL, 1, 1); // ensure the bitmap exists

    // Load the bitmap only once; it is not reloaded when the resolution changes.
    // If the user switched from 256 colors to a higher color depth, LoadBitmap
    // would create a bitmap compatible with the display DC, and the bitmap would
    // remain in degraded colors. Therefore, load it as a DIB.
    // HWorkerBitmap = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_WORKER)));
    //HWorkerBitmap = (HBITMAP)HANDLES(LoadImage(HInstance, MAKEINTRESOURCE(IDB_WORKER), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    //if (HWorkerBitmap == NULL)
    //  return FALSE;

    // When the font changes, CreatePanelFont and CreateEnvFonts are called explicitly; do the initial setup here.
    CreatePanelFont();
    CreateEnvFonts();

    if (Font == NULL || FontUL == NULL || EnvFont == NULL || EnvFontUL == NULL || TooltipFont == NULL)
    {
        TRACE_E("Unable to create fonts.");
        return FALSE;
    }

    return TRUE;
}

void ReleaseConstGraphics()
{
    ItemBitmap.Destroy();
    //if (HWorkerBitmap != NULL)
    //{
    //  HANDLES(DeleteObject(HWorkerBitmap));
    //  HWorkerBitmap = NULL;
    //}

    if (Font != NULL)
    {
        HANDLES(DeleteObject(Font));
        Font = NULL;
    }

    if (FontUL != NULL)
    {
        HANDLES(DeleteObject(FontUL));
        FontUL = NULL;
    }

    if (TooltipFont != NULL)
    {
        HANDLES(DeleteObject(TooltipFont));
        TooltipFont = NULL;
    }

    if (EnvFont != NULL)
    {
        HANDLES(DeleteObject(EnvFont));
        EnvFont = NULL;
    }

    if (EnvFontUL != NULL)
    {
        HANDLES(DeleteObject(EnvFontUL));
        EnvFontUL = NULL;
    }
}

BOOL AuxAllocateImageLists()
{
    int i;
    for (i = 0; i < ICONSIZE_COUNT; i++)
    {
        SimpleIconLists[i] = new CIconList();
        if (SimpleIconLists[i] == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }

    ThrobberFrames = new CIconList();
    if (ThrobberFrames == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    LockFrames = new CIconList();
    if (LockFrames == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    return TRUE;
}

// Using TweakUI, users can change the shortcut icon (default, custom, or none).
// We'll try to honor that setting.
BOOL GetShortcutOverlay()
{
    int i;
    for (i = 0; i < ICONSIZE_COUNT; i++)
    {
        if (HShortcutOverlays[i] != NULL)
        {
            HANDLES(DestroyIcon(HShortcutOverlays[i]));
            HShortcutOverlays[i] = NULL;
        }
    }

    /*
 * //#include <CommonControls.h>
 *
 * // Loading overlay icons from the system image list is unnecessarily slow, so we load them directly from imageres.dll instead.
 * // This code is kept here in case we need to determine where those icons are again.
 * typedef DECLSPEC_IMPORT HRESULT (WINAPI *F__SHGetImageList)(int iImageList, REFIID riid, void **ppvObj);
 *
 * F__SHGetImageList MySHGetImageList = (F__SHGetImageList)GetProcAddress(Shell32DLL, "SHGetImageList"); // Min: XP
 * if (MySHGetImageList != NULL)
 * {
 * int shareIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SHARE);
 * int linkIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_LINK);
 * int offlineIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SLOWFILE);
 * shareIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SHARE);
 *
 * IImageList *imageList;
 * if (MySHGetImageList(1 /* SHIL_SMALL * /, IID_IImageList, (void **)&imageList) == S_OK &&
 * imageList != NULL)
 * {
 * int i;
 * imageList->GetOverlayImage(linkIndex, &i);
 * HICON icon;
 * if (imageList->GetIcon(i, 0, &icon) != S_OK)
 * icon = NULL;
 * HShortcutOverlays[ICONSIZE_16] = icon;
 * imageList->Release();
 * }
 * if (MySHGetImageList(0 /* SHIL_LARGE * /, IID_IImageList, (void **)&imageList) == S_OK &&
 * imageList != NULL)
 * {
 * int i;
 * imageList->GetOverlayImage(linkIndex, &i);
 * HICON icon;
 * if (imageList->GetIcon(i, 0, &icon) != S_OK)
 * icon = NULL;
 * HShortcutOverlays[ICONSIZE_32] = icon;
 * imageList->Release();
 * }
 * if (MySHGetImageList(2 /* SHIL_EXTRALARGE * /, IID_IImageList, (void **)&imageList) == S_OK &&
 * imageList != NULL)
 * {
 * int i;
 * imageList->GetOverlayImage(linkIndex, &i);
 * HICON icon;
 * if (imageList->GetIcon(i, 0, &icon) != S_OK)
 * icon = NULL;
 * HShortcutOverlays[ICONSIZE_48] = icon;
 * imageList->Release();
 * }
 * }
 * /
 */

    HKEY hKey;
    if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE,
                               "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Icons",
                               0, KEY_QUERY_VALUE, &hKey)) == ERROR_SUCCESS)
    {
        char buff[MAX_PATH + 10];
        DWORD buffLen = sizeof(buff);
        buff[0] = 0;
        SalRegQueryValueEx(hKey, "29", NULL, NULL, (LPBYTE)buff, &buffLen);
        if (buff[0] != 0)
        {
            char* num = strrchr(buff, ','); // The icon number comes after the last comma.
            if (num != NULL)
            {
                int index = atoi(num + 1);
                *num = 0;

                HICON hIcons[2] = {0, 0};

                ExtractIcons(buff, index,
                             MAKELONG(32, 16),
                             MAKELONG(32, 16),
                             hIcons, NULL, 2, IconLRFlags);

                HShortcutOverlays[ICONSIZE_32] = hIcons[0];
                HShortcutOverlays[ICONSIZE_16] = hIcons[1];

                ExtractIcons(buff, index,
                             48,
                             48,
                             hIcons, NULL, 1, IconLRFlags);
                HShortcutOverlays[ICONSIZE_48] = hIcons[0];

                for (i = 0; i < ICONSIZE_COUNT; i++)
                    if (HShortcutOverlays[i] != NULL)
                        HANDLES_ADD(__htIcon, __hoLoadImage, HShortcutOverlays[i]);
            }
        }
        NOHANDLES(RegCloseKey(hKey));
    }

    for (i = 0; i < ICONSIZE_COUNT; i++)
    {
        if (HShortcutOverlays[i] == NULL)
        {
            HShortcutOverlays[i] = (HICON)HANDLES(LoadImage(ImageResDLL, MAKEINTRESOURCE(163),
                                                            IMAGE_ICON, IconSizes[i], IconSizes[i], IconLRFlags));
        }
    }
    return (HShortcutOverlays[ICONSIZE_16] != NULL &&
            HShortcutOverlays[ICONSIZE_32] != NULL &&
            HShortcutOverlays[ICONSIZE_48] != NULL);
}

int GetCurrentBPP(HDC hDC)
{
    HDC hdc;
    if (hDC == NULL)
        hdc = GetDC(NULL);
    else
        hdc = hDC;
    int bpp = GetDeviceCaps(hdc, PLANES) * GetDeviceCaps(hdc, BITSPIXEL);
    if (hDC == NULL)
        ReleaseDC(NULL, hdc);

    return bpp;
}

int GetSystemDPI()
{
    if (SystemDPI == 0)
    {
        TRACE_E("GetSystemDPI() SystemDPI == 0!");
        return 96;
    }
    else
    {
        return SystemDPI;
    }
}

int GetScaleForSystemDPI()
{
    int dpi = GetSystemDPI();
    int scale;
    if (dpi <= 96)
        scale = 100;
    else if (dpi <= 120)
        scale = 125;
    else if (dpi <= 144)
        scale = 150;
    else if (dpi <= 192)
        scale = 200;
    else if (dpi <= 240)
        scale = 250;
    else if (dpi <= 288)
        scale = 300;
    else if (dpi <= 384)
        scale = 400;
    else
        scale = 500;

    return scale;
}

int GetIconSizeForSystemDPI(CIconSizeEnum iconSize)
{
    if (SystemDPI == 0)
    {
        TRACE_E("GetIconSizeForSystemDPI() SystemDPI == 0!");
        return 16;
    }

    if (iconSize < ICONSIZE_16 || iconSize >= ICONSIZE_COUNT)
    {
        TRACE_E("GetIconSizeForSystemDPI() unknown iconSize!");
        return 16;
    }

    // DPI Name      DPI   Scale factor
    // --------------------------------
    // Smaller        96   1.00 (100%)
    // Medium        120   1.25 (125%)
    // Larger        144   1.50 (150%)
    // Extra Large   192   2.00 (200%)
    // Custom        240   2.50 (250%)
    // Custom        288   3.00 (300%)
    // Custom        384   4.00 (400%)
    // Custom        480   5.00 (500%)

    int scale = GetScaleForSystemDPI();

    int baseIconSize[ICONSIZE_COUNT] = {16, 32, 48}; // must match CIconSizeEnum

    return (baseIconSize[iconSize] * scale) / 100;
}

void GetSystemDPI(HDC hDC)
{
    HDC hTmpDC;
    if (hDC == NULL)
        hTmpDC = GetDC(NULL);
    else
        hTmpDC = hDC;
    SystemDPI = GetDeviceCaps(hTmpDC, LOGPIXELSX);
#ifdef _DEBUG
    if (SystemDPI != GetDeviceCaps(hTmpDC, LOGPIXELSY))
        TRACE_E("Unexpected situation: LOGPIXELSX != LOGPIXELSY.");
#endif
    if (hDC == NULL)
        ReleaseDC(NULL, hTmpDC);
}

BOOL InitializeGraphics(BOOL colorsOnly)
{
    // 48x48 only on XP and later
    // Large icons have actually been supported for a long time; they can be enabled via
    // Desktop/Properties/???/Large Icons. Note that there will then be no system image list
    // for 32x32 icons; also, we should get the real icon sizes from the system.
    // For now we ignore that and enable 48x48 only on XP, where they are commonly available.

    //
    // Retrieve the requested icon color depth from the Registry
    //
    int iconColorsCount = 0;
    HDC hDesktopDC = GetDC(NULL);
    int bpp = GetCurrentBPP(hDesktopDC);
    GetSystemDPI(hDesktopDC);
    ReleaseDC(NULL, hDesktopDC);

    IconSizes[ICONSIZE_16] = GetIconSizeForSystemDPI(ICONSIZE_16);
    IconSizes[ICONSIZE_32] = GetIconSizeForSystemDPI(ICONSIZE_32);
    IconSizes[ICONSIZE_48] = GetIconSizeForSystemDPI(ICONSIZE_48);

    HKEY hKey;
    if (OpenKeyAux(NULL, HKEY_CURRENT_USER, "Control Panel\\Desktop\\WindowMetrics", hKey))
    {
        // other interesting values: "Shell Icon Size", "Shell Small Icon Size"
        char buff[100];
        if (GetValueAux(NULL, hKey, "Shell Icon Bpp", REG_SZ, buff, 100))
        {
            iconColorsCount = atoi(buff);
        }
        else
        {
            if (WindowsVistaAndLater)
            {
                // In Vista this key simply does not exist, and I still do not know what replaced it,
                // so we will assume the icons use full color (otherwise we were showing ugly 16-color icons)
                iconColorsCount = 32;
            }
        }

        if (iconColorsCount > bpp)
            iconColorsCount = bpp;
        if (bpp <= 8)
            iconColorsCount = 0;

        HANDLES(RegCloseKey(hKey));
    }

    TRACE_I("InitializeGraphics() bpp=" << bpp << " iconColorsCount=" << iconColorsCount);
    if (bpp >= 4 && iconColorsCount <= 4)
        IconLRFlags = LR_VGACOLOR;
    else
        IconLRFlags = 0;

    HDC dc = HANDLES(GetDC(NULL));
    CHighlightMasks* masks = MainWindow != NULL ? MainWindow->HighlightMasks : NULL;
    UpdateDefaultColors(CurrentColors, masks, TRUE, masks != NULL);
    if (!colorsOnly)
    {
        ImageResDLL = HANDLES(LoadLibraryEx("imageres.dll", NULL, LOAD_LIBRARY_AS_DATAFILE));
        if (ImageResDLL == NULL)
        {
            TRACE_E("Unable to load library imageres.dll.");
            return FALSE;
        }

        Shell32DLL = HANDLES(LoadLibraryEx("shell32.dll", NULL, LOAD_LIBRARY_AS_DATAFILE));
        if (Shell32DLL == NULL) // This should never happen; shell32.dll is part of the Windows 4.0 base.
        {
            TRACE_E("Unable to load library shell32.dll.");
            return FALSE;
        }

        HINSTANCE iconDLL = ImageResDLL;
        int iconIndex = 164;
        int i;
        for (i = 0; i < ICONSIZE_COUNT; i++)
        {
            HSharedOverlays[i] = (HICON)HANDLES(LoadImage(iconDLL, MAKEINTRESOURCE(iconIndex),
                                                          IMAGE_ICON, IconSizes[i], IconSizes[i], IconLRFlags));
        }
        GetShortcutOverlay(); // HShortcutOverlayXX

        iconIndex = 97;
        for (i = 0; i < ICONSIZE_COUNT; i++)
        {
            HSlowFileOverlays[i] = (HICON)HANDLES(LoadImage(iconDLL, MAKEINTRESOURCE(iconIndex),
                                                            IMAGE_ICON, IconSizes[i], IconSizes[i], IconLRFlags));
        }

        HGroupIcon = SalLoadImage(4, 20, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags);
        HFavoritIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(319), IMAGE_ICON, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags));
        if (HSharedOverlays[ICONSIZE_16] == NULL ||
            HSharedOverlays[ICONSIZE_32] == NULL ||
            HSharedOverlays[ICONSIZE_48] == NULL ||
            HShortcutOverlays[ICONSIZE_16] == NULL ||
            HShortcutOverlays[ICONSIZE_32] == NULL ||
            HShortcutOverlays[ICONSIZE_48] == NULL ||
            HSlowFileOverlays[ICONSIZE_16] == NULL ||
            HSlowFileOverlays[ICONSIZE_32] == NULL ||
            HSlowFileOverlays[ICONSIZE_48] == NULL ||
            HGroupIcon == NULL || HFavoritIcon == NULL)
        {
            TRACE_E("Unable to read icon overlays for shared directories, shortcuts or slow files, or icon for groups or favorites.");
            return FALSE;
        }

        // The compiler reported error C2712: Cannot use __try in functions that require object unwinding
        // Work around it by moving the allocation into a function
        //    SymbolsIconList = new CIconList();
        //    LargeSymbolsIconList = new CIconList();
        if (!AuxAllocateImageLists())
            return FALSE;

        for (i = 0; i < ICONSIZE_COUNT; i++)
        {
            if (!SimpleIconLists[i]->Create(IconSizes[i], IconSizes[i], symbolsCount))
            {
                TRACE_E("Unable to create image lists.");
                return FALSE;
            }
            SimpleIconLists[i]->SetBkColor(GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
        }

        if (!ThrobberFrames->CreateFromPNG(HInstance, MAKEINTRESOURCE(IDB_THROBBER), THROBBER_WIDTH))
        {
            TRACE_E("Unable to create throbber.");
            return FALSE;
        }

        if (!LockFrames->CreateFromPNG(HInstance, MAKEINTRESOURCE(IDB_LOCK), LOCK_WIDTH))
        {
            TRACE_E("Unable to create lock.");
            return FALSE;
        }

        HFindSymbolsImageList = ImageList_Create(IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], ILC_MASK | GetImageListColorFlags(), 2, 0);
        if (HFindSymbolsImageList == NULL)
        {
            TRACE_E("Unable to create image list.");
            return FALSE;
        }
        ImageList_SetImageCount(HFindSymbolsImageList, 2); // initialization
                                                           //    ImageList_SetBkColor(HFindSymbolsImageList, GetSysColor(COLOR_WINDOW)); // so transparent icons work on XP

        int iconSize = IconSizes[ICONSIZE_16];
        HBITMAP hTmpMaskBitmap;
        HBITMAP hTmpGrayBitmap;
        HBITMAP hTmpColorBitmap;
        if (!CreateToolbarBitmaps(HInstance,
                                  IDB_MENU,
                                  RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE),
                                  hTmpMaskBitmap, hTmpGrayBitmap, hTmpColorBitmap,
                                  FALSE, NULL, 0))
            return FALSE;
        HMenuMarkImageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLORDDB, 2, 1);
        ImageList_Add(HMenuMarkImageList, hTmpColorBitmap, hTmpMaskBitmap);
        HANDLES(DeleteObject(hTmpMaskBitmap));
        HANDLES(DeleteObject(hTmpGrayBitmap));
        HANDLES(DeleteObject(hTmpColorBitmap));

        CSVGIcon* svgIcons;
        int svgIconsCount;
        GetSVGIconsMainToolbar(&svgIcons, &svgIconsCount);
        if (!CreateToolbarBitmaps(HInstance,
                                  Use256ColorsBitmap() ? IDB_TOOLBAR_256 : IDB_TOOLBAR_16,
                                  RGB(255, 0, 255), GetSysColor(COLOR_BTNFACE),
                                  hTmpMaskBitmap, hTmpGrayBitmap, hTmpColorBitmap,
                                  TRUE, svgIcons, svgIconsCount))
            return FALSE;
        HHotToolBarImageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
        HGrayToolBarImageList = ImageList_Create(iconSize, iconSize, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
        ImageList_Add(HHotToolBarImageList, hTmpColorBitmap, hTmpMaskBitmap);
        ImageList_Add(HGrayToolBarImageList, hTmpGrayBitmap, hTmpMaskBitmap);
        HANDLES(DeleteObject(hTmpMaskBitmap));
        HANDLES(DeleteObject(hTmpGrayBitmap));
        HANDLES(DeleteObject(hTmpColorBitmap));

        if (HHotToolBarImageList == NULL || HGrayToolBarImageList == NULL)
        {
            TRACE_E("Unable to create image list.");
            return FALSE;
        }

        HBottomTBImageList = ImageList_Create(BOTTOMBAR_CX, BOTTOMBAR_CY, ILC_MASK | ILC_COLORDDB, 12, 0);
        HHotBottomTBImageList = ImageList_Create(BOTTOMBAR_CX, BOTTOMBAR_CY, ILC_MASK | ILC_COLORDDB, 12, 0);
        if (HBottomTBImageList == NULL || HHotBottomTBImageList == NULL)
        {
            TRACE_E("Unable to create image list.");
            return FALSE;
        }

        // Load icons from shell32:
        int indexes[] = {symbolsExecutable, symbolsDirectory, symbolsNonAssociated, symbolsAssociated, -1};
        int resID[] = {3, 4, 1, 2, -1};
        int vistaResID[] = {15, 4, 2, 90, -1};
        HICON hIcon;
        for (i = 0; indexes[i] != -1; i++)
        {
            int sizeIndex;
            for (sizeIndex = 0; sizeIndex < ICONSIZE_COUNT; sizeIndex++)
            {
                hIcon = SalLoadImage(vistaResID[i], resID[i], IconSizes[sizeIndex], IconSizes[sizeIndex], IconLRFlags);
                if (hIcon != NULL)
                {
                    SimpleIconLists[sizeIndex]->ReplaceIcon(indexes[i], hIcon);
                    if (sizeIndex == ICONSIZE_16)
                    {
                        if (indexes[i] == symbolsDirectory)
                            ImageList_ReplaceIcon(HFindSymbolsImageList, 0, hIcon);
                        if (indexes[i] == symbolsNonAssociated)
                            ImageList_ReplaceIcon(HFindSymbolsImageList, 1, hIcon);
                    }
                    HANDLES(DestroyIcon(hIcon));
                }
                else
                    TRACE_E("Cannot retrieve icon from IMAGERES.DLL or SHELL32.DLL resID=" << resID[i]);
            }
        }
        char systemDir[MAX_PATH];
        GetSystemDirectory(systemDir, MAX_PATH);
        // 16x16, 32x32, 48x48
        int sizeIndex;
        for (sizeIndex = ICONSIZE_16; sizeIndex < ICONSIZE_COUNT; sizeIndex++)
        {
            // folder icon
            hIcon = NULL;
            __try
            {
                if (!GetFileIcon(systemDir, FALSE, &hIcon, (CIconSizeEnum)sizeIndex, FALSE, FALSE))
                    hIcon = NULL;
            }
            __except (CCallStack::HandleException(GetExceptionInformation(), 15))
            {
                FGIExceptionHasOccured++;
                hIcon = NULL;
            }
            if (hIcon != NULL) // if we do not obtain the icon, the #4 one from shell32.dll remains
            {
                SimpleIconLists[sizeIndex]->ReplaceIcon(symbolsDirectory, hIcon);
                NOHANDLES(DestroyIcon(hIcon));
            }

            // ".." icon
            hIcon = (HICON)HANDLES(LoadImage(HInstance, MAKEINTRESOURCE(IDI_UPPERDIR),
                                             IMAGE_ICON, IconSizes[sizeIndex], IconSizes[sizeIndex],
                                             IconLRFlags));
            SimpleIconLists[sizeIndex]->ReplaceIcon(symbolsUpDir, hIcon);
            HANDLES(DestroyIcon(hIcon));

            // archive icon
            hIcon = LoadArchiveIcon(IconSizes[sizeIndex], IconSizes[sizeIndex], IconLRFlags);
            SimpleIconLists[sizeIndex]->ReplaceIcon(symbolsArchive, hIcon);
            HANDLES(DestroyIcon(hIcon));
        }

        WORD bits[8] = {0x0055, 0x00aa, 0x0055, 0x00aa,
                        0x0055, 0x00aa, 0x0055, 0x00aa};
        HBITMAP hBrushBitmap = HANDLES(CreateBitmap(8, 8, 1, 1, &bits));
        HDitherBrush = HANDLES(CreatePatternBrush(hBrushBitmap));
        HANDLES(DeleteObject(hBrushBitmap));
        if (HDitherBrush == NULL)
            return FALSE;

        HUpDownBitmap = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_UPDOWN)));
        HZoomBitmap = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_ZOOM)));
        HFilter = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_FILTER)));

        if (HUpDownBitmap == NULL ||
            HZoomBitmap == NULL || HFilter == NULL)
        {
            TRACE_E("HUpDownBitmap == NULL || HZoomBitmap == NULL || HFilter == NULL");
            return FALSE;
        }

        SVGArrowRight.Load(IDV_ARROW_RIGHT, -1, -1, SVGSTATE_ENABLED | SVGSTATE_DISABLED);
        SVGArrowRightSmall.Load(IDV_ARROW_RIGHT, -1, (int)((double)iconSize / 2.5), SVGSTATE_ENABLED | SVGSTATE_DISABLED);
        SVGArrowMore.Load(IDV_ARROW_MORE, -1, -1, SVGSTATE_ENABLED | SVGSTATE_DISABLED);
        SVGArrowLess.Load(IDV_ARROW_LESS, -1, -1, SVGSTATE_ENABLED | SVGSTATE_DISABLED);
        SVGArrowDropDown.Load(IDV_ARROW_DOWN, -1, -1, SVGSTATE_ENABLED | SVGSTATE_DISABLED);
    }

    ImageList_SetBkColor(HHotToolBarImageList, GetSysColor(COLOR_BTNFACE));
    ImageList_SetBkColor(HGrayToolBarImageList, GetSysColor(COLOR_BTNFACE));

    if (SystemParametersInfo(SPI_GETMOUSEHOVERTIME, 0, &MouseHoverTime, FALSE) == 0)
    {
        if (SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &MouseHoverTime, FALSE) == 0)
            MouseHoverTime = 400;
    }
    //  TRACE_I("MouseHoverTime="<<MouseHoverTime);

    COLORREF normalBkgnd = GetNearestColor(dc, GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
    COLORREF selectedBkgnd = GetNearestColor(dc, GetCOLORREF(CurrentColors[ITEM_BK_SELECTED]));
    COLORREF focusedBkgnd = GetNearestColor(dc, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
    COLORREF focselBkgnd = GetNearestColor(dc, GetCOLORREF(CurrentColors[ITEM_BK_FOCSEL]));
    COLORREF activeCaption = GetNearestColor(dc, GetCOLORREF(CurrentColors[ACTIVE_CAPTION_BK]));
    COLORREF inactiveCaption = GetNearestColor(dc, GetCOLORREF(CurrentColors[INACTIVE_CAPTION_BK]));
    HANDLES(ReleaseDC(NULL, dc));

    HNormalBkBrush = HANDLES(CreateSolidBrush(normalBkgnd));
    HFocusedBkBrush = HANDLES(CreateSolidBrush(focusedBkgnd));
    HSelectedBkBrush = HANDLES(CreateSolidBrush(selectedBkgnd));
    HFocSelBkBrush = HANDLES(CreateSolidBrush(focselBkgnd));
    HActiveCaptionBrush = HANDLES(CreateSolidBrush(activeCaption));
    HInactiveCaptionBrush = HANDLES(CreateSolidBrush(inactiveCaption));

    if (HNormalBkBrush == NULL || HFocusedBkBrush == NULL ||
        HSelectedBkBrush == NULL || HFocSelBkBrush == NULL ||
        HActiveCaptionBrush == NULL || HInactiveCaptionBrush == NULL ||
        HMenuSelectedBkBrush == NULL)
    {
        TRACE_E("Unable to create brush.");
        return FALSE;
    }

    HActiveNormalPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[FOCUS_ACTIVE_NORMAL])));
    HActiveSelectedPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[FOCUS_ACTIVE_SELECTED])));
    HInactiveNormalPen = HANDLES(CreatePen(PS_DOT, 0, GetCOLORREF(CurrentColors[FOCUS_FG_INACTIVE_NORMAL])));
    HInactiveSelectedPen = HANDLES(CreatePen(PS_DOT, 0, GetCOLORREF(CurrentColors[FOCUS_FG_INACTIVE_SELECTED])));

    HThumbnailNormalPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[THUMBNAIL_FRAME_NORMAL])));
    HThumbnailFucsedPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[THUMBNAIL_FRAME_FOCUSED])));
    HThumbnailSelectedPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[THUMBNAIL_FRAME_SELECTED])));
    HThumbnailFocSelPen = HANDLES(CreatePen(PS_SOLID, 0, GetCOLORREF(CurrentColors[THUMBNAIL_FRAME_FOCSEL])));

    BtnShadowPen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW)));
    BtnHilightPen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHILIGHT)));
    Btn3DLightPen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DLIGHT)));
    BtnFacePen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNFACE)));
    WndFramePen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_WINDOWFRAME)));
    WndPen = HANDLES(CreatePen(PS_SOLID, 0, GetSysColor(COLOR_WINDOW)));
    if (HActiveNormalPen == NULL || HActiveSelectedPen == NULL ||
        HInactiveNormalPen == NULL || HInactiveSelectedPen == NULL ||
        HThumbnailNormalPen == NULL || HThumbnailFucsedPen == NULL ||
        HThumbnailSelectedPen == NULL || HThumbnailFocSelPen == NULL ||
        BtnShadowPen == NULL || BtnHilightPen == NULL || BtnFacePen == NULL ||
        Btn3DLightPen == NULL || WndFramePen == NULL || WndPen == NULL)
    {
        TRACE_E("Unable to create a pen.");
        return FALSE;
    }

    COLORMAP clrMap[3];
    clrMap[0].from = RGB(255, 0, 255);
    clrMap[0].to = GetSysColor(COLOR_BTNFACE);
    clrMap[1].from = RGB(255, 255, 255);
    clrMap[1].to = GetSysColor(COLOR_BTNHILIGHT);
    clrMap[2].from = RGB(128, 128, 128);
    clrMap[2].to = GetSysColor(COLOR_BTNSHADOW);
    HHeaderSort = HANDLES(CreateMappedBitmap(HInstance, IDB_HEADER, 0, clrMap, 3));
    if (HHeaderSort == NULL)
    {
        TRACE_E("Unable to load bitmap HHeaderSort.");
        return FALSE;
    }

    clrMap[0].from = RGB(128, 128, 128); // gray -> COLOR_BTNSHADOW
    clrMap[0].to = GetSysColor(COLOR_BTNSHADOW);
    clrMap[1].from = RGB(0, 0, 0); // black -> COLOR_BTNTEXT
    clrMap[1].to = GetSysColor(COLOR_BTNTEXT);
    clrMap[2].from = RGB(255, 255, 255); // white -> transparent
    clrMap[2].to = RGB(255, 0, 255);
    HBITMAP hBottomTB = HANDLES(CreateMappedBitmap(HInstance, IDB_BOTTOMTOOLBAR, 0, clrMap, 3));
    BOOL remapWhite = FALSE;
    if (GetCurrentBPP() > 8)
    {
        clrMap[2].from = RGB(255, 255, 255); // white -> light gray (so it's not so harsh)
        clrMap[2].to = RGB(235, 235, 235);
        remapWhite = TRUE;
    }
    HBITMAP hHotBottomTB = HANDLES(CreateMappedBitmap(HInstance, IDB_BOTTOMTOOLBAR, 0, clrMap, remapWhite ? 3 : 2));
    ImageList_RemoveAll(HBottomTBImageList);
    ImageList_AddMasked(HBottomTBImageList, hBottomTB, RGB(255, 0, 255));
    ImageList_RemoveAll(HHotBottomTBImageList);
    ImageList_AddMasked(HHotBottomTBImageList, hHotBottomTB, RGB(255, 0, 255));
    HANDLES(DeleteObject(hBottomTB));
    HANDLES(DeleteObject(hHotBottomTB));
    ImageList_SetBkColor(HBottomTBImageList, GetSysColor(COLOR_BTNFACE));
    ImageList_SetBkColor(HHotBottomTBImageList, GetSysColor(COLOR_BTNFACE));
    return TRUE;
}

// ****************************************************************************

void ReleaseGraphics(BOOL colorsOnly)
{
    if (!colorsOnly)
    {
        int i;
        for (i = 0; i < ICONSIZE_COUNT; i++)
        {
            if (HSharedOverlays[i] != NULL)
            {
                HANDLES(DestroyIcon(HSharedOverlays[i]));
                HSharedOverlays[i] = NULL;
            }
            if (HShortcutOverlays[i] != NULL)
            {
                HANDLES(DestroyIcon(HShortcutOverlays[i]));
                HShortcutOverlays[i] = NULL;
            }
            if (HSlowFileOverlays[i] != NULL)
            {
                HANDLES(DestroyIcon(HSlowFileOverlays[i]));
                HSlowFileOverlays[i] = NULL;
            }
        }

        if (HGroupIcon != NULL)
        {
            HANDLES(DestroyIcon(HGroupIcon));
            HGroupIcon = NULL;
        }

        if (HFavoritIcon != NULL)
        {
            HANDLES(DestroyIcon(HFavoritIcon));
            HFavoritIcon = NULL;
        }

        if (HZoomBitmap != NULL)
        {
            HANDLES(DeleteObject(HZoomBitmap));
            HZoomBitmap = NULL;
        }

        if (HFilter != NULL)
        {
            HANDLES(DeleteObject(HFilter));
            HFilter = NULL;
        }

        if (HUpDownBitmap != NULL)
        {
            HANDLES(DeleteObject(HUpDownBitmap));
            HUpDownBitmap = NULL;
        }
    }

    if (HNormalBkBrush != NULL)
    {
        HANDLES(DeleteObject(HNormalBkBrush));
        HNormalBkBrush = NULL;
    }
    if (HFocusedBkBrush != NULL)
    {
        HANDLES(DeleteObject(HFocusedBkBrush));
        HFocusedBkBrush = NULL;
    }
    if (HSelectedBkBrush != NULL)
    {
        HANDLES(DeleteObject(HSelectedBkBrush));
        HSelectedBkBrush = NULL;
    }
    if (HFocSelBkBrush != NULL)
    {
        HANDLES(DeleteObject(HFocSelBkBrush));
        HFocSelBkBrush = NULL;
    }
    if (HActiveCaptionBrush != NULL)
    {
        HANDLES(DeleteObject(HActiveCaptionBrush));
        HActiveCaptionBrush = NULL;
    }
    if (HInactiveCaptionBrush != NULL)
    {
        HANDLES(DeleteObject(HInactiveCaptionBrush));
        HInactiveCaptionBrush = NULL;
    }
    if (HActiveNormalPen != NULL)
    {
        HANDLES(DeleteObject(HActiveNormalPen));
        HActiveNormalPen = NULL;
    }
    if (HActiveSelectedPen != NULL)
    {
        HANDLES(DeleteObject(HActiveSelectedPen));
        HActiveSelectedPen = NULL;
    }
    if (HInactiveNormalPen != NULL)
    {
        HANDLES(DeleteObject(HInactiveNormalPen));
        HInactiveNormalPen = NULL;
    }
    if (HInactiveSelectedPen != NULL)
    {
        HANDLES(DeleteObject(HInactiveSelectedPen));
        HInactiveSelectedPen = NULL;
    }
    if (HThumbnailNormalPen != NULL)
    {
        HANDLES(DeleteObject(HThumbnailNormalPen));
        HThumbnailNormalPen = NULL;
    }
    if (HThumbnailFucsedPen != NULL)
    {
        HANDLES(DeleteObject(HThumbnailFucsedPen));
        HThumbnailFucsedPen = NULL;
    }
    if (HThumbnailSelectedPen != NULL)
    {
        HANDLES(DeleteObject(HThumbnailSelectedPen));
        HThumbnailSelectedPen = NULL;
    }
    if (HThumbnailFocSelPen != NULL)
    {
        HANDLES(DeleteObject(HThumbnailFocSelPen));
        HThumbnailFocSelPen = NULL;
    }
    if (BtnShadowPen != NULL)
    {
        HANDLES(DeleteObject(BtnShadowPen));
        BtnShadowPen = NULL;
    }
    if (BtnHilightPen != NULL)
    {
        HANDLES(DeleteObject(BtnHilightPen));
        BtnHilightPen = NULL;
    }
    if (Btn3DLightPen != NULL)
    {
        HANDLES(DeleteObject(Btn3DLightPen));
        Btn3DLightPen = NULL;
    }
    if (BtnFacePen != NULL)
    {
        HANDLES(DeleteObject(BtnFacePen));
        BtnFacePen = NULL;
    }
    if (WndFramePen != NULL)
    {
        HANDLES(DeleteObject(WndFramePen));
        WndFramePen = NULL;
    }
    if (WndPen != NULL)
    {
        HANDLES(DeleteObject(WndPen));
        WndPen = NULL;
    }
    if (HHeaderSort != NULL)
    {
        HANDLES(DeleteObject(HHeaderSort));
        HHeaderSort = NULL;
    }

    if (!colorsOnly)
    {
        if (HDitherBrush != NULL)
        {
            HANDLES(DeleteObject(HDitherBrush));
            HDitherBrush = NULL;
        }
        if (HHotToolBarImageList != NULL)
        {
            ImageList_Destroy(HHotToolBarImageList);
            HHotToolBarImageList = NULL;
        }
        if (HGrayToolBarImageList != NULL)
        {
            ImageList_Destroy(HGrayToolBarImageList);
            HGrayToolBarImageList = NULL;
        }
        if (HBottomTBImageList != NULL)
        {
            ImageList_Destroy(HBottomTBImageList);
            HBottomTBImageList = NULL;
        }
        if (HHotBottomTBImageList != NULL)
        {
            ImageList_Destroy(HHotBottomTBImageList);
            HHotBottomTBImageList = NULL;
        }
        if (HMenuMarkImageList != NULL)
        {
            ImageList_Destroy(HMenuMarkImageList);
            HMenuMarkImageList = NULL;
        }
        int i;
        for (i = 0; i < ICONSIZE_COUNT; i++)
        {
            if (SimpleIconLists[i] != NULL)
            {
                delete SimpleIconLists[i];
                SimpleIconLists[i] = NULL;
            }
        }
        if (ThrobberFrames != NULL)
        {
            delete ThrobberFrames;
            ThrobberFrames = NULL;
        }
        if (LockFrames != NULL)
        {
            delete LockFrames;
            LockFrames = NULL;
        }
        if (HFindSymbolsImageList != NULL)
        {
            ImageList_Destroy(HFindSymbolsImageList);
            HFindSymbolsImageList = NULL;
        }
        if (Shell32DLL != NULL)
        {
            HANDLES(FreeLibrary(Shell32DLL));
            Shell32DLL = NULL;
        }
        if (ImageResDLL != NULL)
        {
            HANDLES(FreeLibrary(ImageResDLL));
            ImageResDLL = NULL;
        }
    }
}

// ****************************************************************************

char* NumberToStr(char* buffer, const CQuadWord& number)
{
    _ui64toa(number.Value, buffer, 10);
    int l = (int)strlen(buffer);
    char* s = buffer + l;
    int c = 0;
    while (--s > buffer)
    {
        if ((++c % 3) == 0)
        {
            memmove(s + ThousandsSeparatorLen, s, (c / 3) * 3 + (c / 3 - 1) * ThousandsSeparatorLen + 1);
            memcpy(s, ThousandsSeparator, ThousandsSeparatorLen);
        }
    }
    return buffer;
}

int NumberToStr2(char* buffer, const CQuadWord& number)
{
    _ui64toa(number.Value, buffer, 10);
    int l = (int)strlen(buffer);
    char* s = buffer + l;
    int c = 0;
    while (--s > buffer)
    {
        if ((++c % 3) == 0)
        {
            memmove(s + ThousandsSeparatorLen, s, (c / 3) * 3 + (c / 3 - 1) * ThousandsSeparatorLen + 1);
            memcpy(s, ThousandsSeparator, ThousandsSeparatorLen);
            l += ThousandsSeparatorLen;
        }
    }
    return l;
}

// ****************************************************************************

BOOL PointToLocalDecimalSeparator(char* buffer, int bufferSize)
{
    char* s = strrchr(buffer, '.');
    if (s != NULL)
    {
        int len = (int)strlen(buffer);
        if (len - 1 + DecimalSeparatorLen > bufferSize - 1)
        {
            TRACE_E("PointToLocalDecimalSeparator() small buffer!");
            return FALSE;
        }
        memmove(s + DecimalSeparatorLen, s + 1, len - (s - buffer));
        memcpy(s, DecimalSeparator, DecimalSeparatorLen);
    }
    return TRUE;
}

// ****************************************************************************
//
// GetCmdLine - get arguments from the command line
//
// buf + size - buffer for the arguments
// argv - array of pointers to be filled with the arguments
// argCount - on input, the number of elements in argv; on output, contains the number of arguments
// cmdLine - command-line arguments (without the .exe file name - from WinMain)

BOOL GetCmdLine(char* buf, int size, char* argv[], int& argCount, char* cmdLine)
{
    int space = argCount;
    argCount = 0;
    char* c = buf;
    char* end = buf + size;

    char* s = cmdLine;
    char term;
    while (*s != 0)
    {
        if (*s == '"') // leading '"'
        {
            if (*++s == 0)
                break;
            term = '"';
        }
        else
            term = ' ';

        if (argCount < space && c < end)
            argv[argCount++] = c;
        else
            return c < end; // Error only if the buffer is too small

        while (1)
        {
            if (*s == term || *s == 0)
            {
                if (*s == 0 || term != '"' || *++s != '"') // unless this is the "" -> " substitution
                {
                    if (*s != 0)
                        s++;
                    while (*s != 0 && *s == ' ')
                        s++;
                    if (c < end)
                    {
                        *c++ = 0;
                        break;
                    }
                    else
                        return FALSE;
                }
            }
            if (c < end)
                *c++ = *s++;
            else
                return FALSE;
        }
    }
    return TRUE;
}

// ****************************************************************************
//
// GetComCtlVersion
//

typedef struct _DllVersionInfo
{
    DWORD cbSize;
    DWORD dwMajorVersion; // Major version
    DWORD dwMinorVersion; // Minor version
    DWORD dwBuildNumber;  // Build number
    DWORD dwPlatformID;   // DLLVER_PLATFORM_*
} DLLVERSIONINFO;

typedef HRESULT(CALLBACK* DLLGETVERSIONPROC)(DLLVERSIONINFO*);

HRESULT GetComCtlVersion(LPDWORD pdwMajor, LPDWORD pdwMinor)
{
    HINSTANCE hComCtl;
    // Load the DLL
    hComCtl = HANDLES(LoadLibrary(TEXT("comctl32.dll")));
    if (hComCtl)
    {
        HRESULT hr = S_OK;
        DLLGETVERSIONPROC pDllGetVersion;
        /*
             This function must be obtained explicitly because earlier versions of the DLL
             do not implement it. The absence of this function therefore serves as a
             version marker.
            */
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hComCtl, TEXT("DllGetVersion")); // no header available
        if (pDllGetVersion)
        {
            DLLVERSIONINFO dvi;
            ZeroMemory(&dvi, sizeof(dvi));
            dvi.cbSize = sizeof(dvi);
            hr = (*pDllGetVersion)(&dvi);
            if (SUCCEEDED(hr))
            {
                *pdwMajor = dvi.dwMajorVersion;
                *pdwMinor = dvi.dwMinorVersion;
            }
            else
            {
                hr = E_FAIL;
            }
        }
        else
        {
            /*
      If GetProcAddress failed, then the DLL is a version previous to the one
      shipped with IE 3.x.
      */
            *pdwMajor = 4;
            *pdwMinor = 0;
        }
        HANDLES(FreeLibrary(hComCtl));
        return hr;
    }
    TRACE_E("LoadLibrary on comctl32.dll failed");
    return E_FAIL;
}

// ****************************************************************************

void InitDefaultDir()
{
    char dir[4] = " :\\";
    char d;
    for (d = 'A'; d <= 'Z'; d++)
    {
        dir[0] = d;
        strcpy(DefaultDir[d - 'A'], dir);
    }
}

// ****************************************************************************

BOOL PackErrorHandler(HWND parent, const WORD err, ...)
{
    va_list argList;
    char buff[1000];
    BOOL ret = FALSE;

    parent = parent == NULL ? (MainWindow != NULL ? MainWindow->HWindow : NULL) : parent;

    va_start(argList, err);
    FormatMessage(FORMAT_MESSAGE_FROM_STRING, LoadStr(err), 0, 0, buff, 1000, &argList);
    if (err < IDS_PACKQRY_PREFIX)
        SalMessageBox(parent, buff, LoadStr(IDS_PACKERR_TITLE), MB_OK | MB_ICONEXCLAMATION);
    else
        ret = SalMessageBox(parent, buff, LoadStr(IDS_PACKERR_TITLE), MB_OKCANCEL | MB_ICONQUESTION) == IDOK;
    va_end(argList);
    return ret;
}

//
// ****************************************************************************
// ColorsChanged
//

void ColorsChanged(BOOL refresh, BOOL colorsOnly, BOOL reloadUMIcons)
{
    CALL_STACK_MESSAGE2("ColorsChanged(%d)", refresh);
    // WARNING! fonts must be FALSE to avoid changing the font handle,
    // and the toolbars that use it would have to be notified of that change
    ReleaseGraphics(colorsOnly);
    InitializeGraphics(colorsOnly);
    ItemBitmap.ReCreateForScreenDC();
    UpdateViewerColors(ViewerColors);
    if (!colorsOnly)
        ShellIconOverlays.ColorsChanged();

    if (MainWindow != NULL && MainWindow->EditWindow != NULL)
        MainWindow->EditWindow->SetFont();

    Associations.ColorsChanged();

    if (MainWindow != NULL)
    {
        MainWindow->OnColorsChanged(reloadUMIcons);
    }

    // notify the Find dialogs about the color change
    FindDialogQueue.BroadcastMessage(WM_USER_COLORCHANGEFIND, 0, 0);

    // Notify the plugins of this change as well.
    Plugins.Event(PLUGINEVENT_COLORSCHANGED, 0);

    if (MainWindow != NULL && MainWindow->HTopRebar != NULL)
        SendMessage(MainWindow->HTopRebar, RB_SETBKCOLOR, 0, (LPARAM)GetSysColor(COLOR_BTNFACE));

    if (refresh && MainWindow != NULL)
    {
        InvalidateRect(MainWindow->HWindow, NULL, TRUE);
    }
    // Internal Viewer and Find: refresh all windows
    BroadcastConfigChanged();
}

#ifdef USE_BETA_EXPIRATION_DATE

int ShowBetaExpDlg()
{
    CBetaExpiredDialog dlg(NULL);
    return (int)dlg.Execute();
}

#endif // USE_BETA_EXPIRATION_DATE

//
// ****************************************************************************

struct VS_VERSIONINFO_HEADER
{
    WORD wLength;
    WORD wValueLength;
    WORD wType;
};

BOOL GetModuleVersion(HINSTANCE hModule, WORD* major, WORD* minor)
{
    HRSRC hRes = FindResource(hModule, MAKEINTRESOURCE(VS_VERSION_INFO), RT_VERSION);
    if (hRes == NULL)
        return FALSE;

    HGLOBAL hVer = LoadResource(hModule, hRes);
    if (hVer == NULL)
        return FALSE;

    DWORD resSize = SizeofResource(hModule, hRes);
    const BYTE* first = (BYTE*)LockResource(hVer);
    if (resSize == 0 || first == 0)
        return FALSE;

    const BYTE* iterator = first + sizeof(VS_VERSIONINFO_HEADER);

    DWORD signature = 0xFEEF04BD;

    while (memcmp(iterator, &signature, 4) != 0)
    {
        iterator++;
        if (iterator + 4 >= first + resSize)
            return FALSE;
    }

    VS_FIXEDFILEINFO* ffi = (VS_FIXEDFILEINFO*)iterator;

    *major = HIWORD(ffi->dwFileVersionMS);
    *minor = LOWORD(ffi->dwFileVersionMS);

    return TRUE;
}

//****************************************************************************
//
// CMessagesKeeper
//

CMessagesKeeper::CMessagesKeeper()
{
    Index = 0;
    Count = 0;
}

void CMessagesKeeper::Add(const MSG* msg)
{
    Messages[Index] = *msg;
    Index = (Index + 1) % MESSAGES_KEEPER_COUNT;
    if (Count < MESSAGES_KEEPER_COUNT)
        Count++;
}

void CMessagesKeeper::Print(char* buffer, int buffMax, int index)
{
    if (buffMax <= 0)
        return;
    if (index >= Count)
    {
        _snprintf_s(buffer, buffMax, _TRUNCATE, "(error)");
    }
    else
    {
        int i;
        if (Count == MESSAGES_KEEPER_COUNT)
            i = (Index + index) % MESSAGES_KEEPER_COUNT;
        else
            i = index;
        const MSG* msg = &Messages[i];
        _snprintf_s(buffer, buffMax, _TRUNCATE, "w=0x%p m=0x%X w=0x%IX l=0x%IX t=%u p=%d,%d",
                    msg->hwnd, msg->message, msg->wParam, msg->lParam, msg->time - SalamanderExceptionTime, msg->pt.x, msg->pt.y);
    }
}

CMessagesKeeper MessagesKeeper;

typedef VOID(WINAPI* FDisableProcessWindowsGhosting)(VOID);

void TurnOFFWindowGhosting() // If "ghosting" is not disabled, the safe-wait windows are hidden after the application has been in a "not responding" state for five seconds (when it is not processing messages)
{
    if (User32DLL != NULL)
    {
        FDisableProcessWindowsGhosting disableProcessWindowsGhosting = (FDisableProcessWindowsGhosting)GetProcAddress(User32DLL, "DisableProcessWindowsGhosting"); // Min: XP
        if (disableProcessWindowsGhosting != NULL)
            disableProcessWindowsGhosting();
    }
}

//
// ****************************************************************************

void UIDToString(GUID* uid, char* buff, int buffSize)
{
    wchar_t buffw[64] = {0};
    StringFromGUID2(*uid, buffw, 64);
    WideCharToMultiByte(CP_ACP, 0, buffw, -1, buff, buffSize, NULL, NULL);
    buff[buffSize - 1] = 0;
}

void StringToUID(char* buff, GUID* uid)
{
    wchar_t buffw[64] = {0};
    MultiByteToWideChar(CP_ACP, 0, buff, -1, buffw, 64);
    buffw[63] = 0;
    CLSIDFromString(buffw, uid);
}

void CleanUID(char* uid)
{
    char* s = uid;
    char* d = uid;
    while (*s != 0)
    {
        while (*s == '{' || *s == '}' || *s == '-')
            s++;
        *d++ = *s++;
    }
}

//
// ****************************************************************************

//#ifdef MSVC_RUNTIME_CHECKS
char RTCErrorDescription[RTC_ERROR_DESCRIPTION_SIZE] = {0};
// custom reporting function copied from MSDN - http://msdn.microsoft.com/en-us/library/cb00sk7k(v=VS.90).aspx
#pragma runtime_checks("", off)
int MyRTCErrorFunc(int errType, const wchar_t* file, int line,
                   const wchar_t* module, const wchar_t* format, ...)
{
    // Prevent reentrancy.
    static long running = 0;
    while (InterlockedExchange(&running, 1))
        Sleep(0);
    // Now, disable all RTC failures.
    int numErrors = _RTC_NumErrors();
    int* errors = (int*)_alloca(numErrors);
    for (int i = 0; i < numErrors; i++)
        errors[i] = _RTC_SetErrorType((_RTC_ErrorNumber)i, _RTC_ERRTYPE_IGNORE);

    // First, retrieve the RTC error number from the variadic argument list.
    va_list vl;
    va_start(vl, format);
    _RTC_ErrorNumber rtc_errnum = va_arg(vl, _RTC_ErrorNumber);
    va_end(vl);

    static wchar_t buf[RTC_ERROR_DESCRIPTION_SIZE];
    static char bufA[RTC_ERROR_DESCRIPTION_SIZE];
    const char* err = _RTC_GetErrDesc(rtc_errnum);
    _snwprintf_s(buf, _TRUNCATE, L"  Error Number: %d\r\n  Description: %S\r\n  Line: #%d\r\n  File: %s\r\n  Module: %s\r\n",
                 rtc_errnum,
                 err,
                 line,
                 file ? file : L"Unknown",
                 module ? module : L"Unknown");

    WideCharToMultiByte(CP_ACP, 0, buf, -1, bufA, RTC_ERROR_DESCRIPTION_SIZE, NULL, NULL);
    bufA[RTC_ERROR_DESCRIPTION_SIZE - 1] = 0;
    lstrcpyn(RTCErrorDescription, bufA, RTC_ERROR_DESCRIPTION_SIZE);

    // Prefer raising an exception here so the call stack is hopefully clearer; if it is not, we can remove this exception.
    // See the description of _CrtDbgReportW behavior: http://msdn.microsoft.com/en-us/library/8hyw4sy7(v=VS.90).aspx
    RaiseException(OPENSAL_EXCEPTION_RTC, 0, 0, NULL); // our custom "RTC" exception

    // We should never get here because the process has been terminated; continuing only for form's sake in case this ever changes.

    // Now, restore the RTC error types.
    for (int i = 0; i < numErrors; i++)
        _RTC_SetErrorType((_RTC_ErrorNumber)i, errors[i]);
    running = 0;

    return -1;
}
#pragma runtime_checks("", restore)
//#endif // MSVC_RUNTIME_CHECKS

//
// ****************************************************************************

#ifdef _DEBUG

DWORD LastCrtCheckMemoryTime; // Time of the last memory check in IDLE

#endif //_DEBUG

STDAPI _StrRetToBuf(STRRET* psr, LPCITEMIDLIST pidl, LPSTR pszBuf, UINT cchBuf);

BOOL FindPluginsWithoutImportedCfg(BOOL* doNotDeleteImportedCfg)
{
    char names[1000];
    int skipped;
    Plugins.RemoveNoLongerExistingPlugins(FALSE, TRUE, names, 1000, 10, &skipped, MainWindow->HWindow);
    if (names[0] != 0)
    {
        *doNotDeleteImportedCfg = TRUE;
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = MainWindow->HWindow;
        params.Flags = MB_OKCANCEL | MB_ICONQUESTION;
        params.Caption = SALAMANDER_TEXT_VERSION;
        char skippedNames[200];
        skippedNames[0] = 0;
        if (skipped > 0)
            sprintf(skippedNames, LoadStr(IDS_NUMOFSKIPPEDPLUGINNAMES), skipped);
        char msg[2000];
        sprintf(msg, LoadStr(IDS_NOTALLPLUGINSCFGIMPORTED), names, skippedNames);
        params.Text = msg;
        char aliasBtnNames[200];
        /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
           for message-box buttons, let hotkey collisions be resolved by pretending they are a menu
        MENU_TEMPLATE_ITEM MsgBoxButtons[] =
        {
          {MNTT_PB, 0
          {MNTT_IT, IDS_STARTWITHOUTMISSINGPLUGINS
          {MNTT_IT, IDS_SELLANGEXITBUTTON
          {MNTT_PE, 0
        };
        */
        sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                DIALOG_OK, LoadStr(IDS_STARTWITHOUTMISSINGPLUGINS),
                DIALOG_CANCEL, LoadStr(IDS_SELLANGEXITBUTTON));
        params.AliasBtnNames = aliasBtnNames;
        return SalMessageBoxEx(&params) == IDCANCEL;
    }
    return FALSE;
}

void StartNotepad(const char* file)
{
    STARTUPINFO si = {0};
    PROCESS_INFORMATION pi;
    char buf[MAX_PATH];
    char buf2[MAX_PATH + 50];

    if (lstrlen(file) >= MAX_PATH)
        return;

    GetSystemDirectory(buf, MAX_PATH); // Use the system directory so it does not prevent deletion of the current working directory.
    wsprintf(buf2, "notepad.exe \"%s\"", file);
    si.cb = sizeof(STARTUPINFO);
    if (HANDLES(CreateProcess(NULL, buf2, NULL, NULL, TRUE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                              NULL, buf, &si, &pi)))
    {
        HANDLES(CloseHandle(pi.hProcess));
        HANDLES(CloseHandle(pi.hThread));
    }
}

BOOL RunningInCompatibilityMode()
{
    // If running on Windows XP or a later OS, there is a risk that an overhelpful user enabled
    // Compatibility Mode. If so, display a warning.
    // WARNING: Application Verifier reports the Windows version as newer than it really is.
    // It does this while testing the application for the "Windows 7 Software Logo".
    WORD kernel32major, kernel32minor;
    if (GetModuleVersion(GetModuleHandle("kernel32.dll"), &kernel32major, &kernel32minor))
    {
        TRACE_I("kernel32.dll: " << kernel32major << ":" << kernel32minor);
        // GetVersionEx must be called because it returns values based on the active Compatibility Mode
        // (SalIsWindowsVersionOrGreater ignores the active Compatibility Mode)
        OSVERSIONINFO os;
        os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        // just avoiding the deprecation warning; GetVersionEx will hopefully always be available everywhere
        typedef BOOL(WINAPI * FDynGetVersionExA)(LPOSVERSIONINFOA lpVersionInformation);
        FDynGetVersionExA DynGetVersionExA = (FDynGetVersionExA)GetProcAddress(GetModuleHandle("kernel32.dll"),
                                                                               "GetVersionExA");
        if (DynGetVersionExA == NULL)
        {
            TRACE_E("RunningInCompatibilityMode(): unable to get address of GetVersionEx()");
            return FALSE;
        }

        DynGetVersionExA(&os);
        TRACE_I("GetVersionEx(): " << os.dwMajorVersion << ":" << os.dwMinorVersion);

        // the current version of Salamander is manifested for Windows 10
        const DWORD SAL_MANIFESTED_FOR_MAJOR = 10;
        const DWORD SAL_MANIFESTED_FOR_MINOR = 0;

        // GetVersionEx never returns a version higher than
        // os.dwMajorVersion == SAL_MANIFESTED_FOR_MAJOR and
        // os.dwMinorVersion == SAL_MANIFESTED_FOR_MINOR. If kernel32.dll is newer,
        // we cannot detect Compatibility Mode with 100% certainty; Salamander must be
        // manifested for the newer Windows version and the constants
        // SAL_MANIFESTED_FOR_MAJOR and SAL_MANIFESTED_FOR_MINOR must be updated.
        // We can at least detect Compatibility Mode set to an older Windows version
        // than the one Salamander is manifested for
        if (kernel32major > SAL_MANIFESTED_FOR_MAJOR ||
            kernel32major == SAL_MANIFESTED_FOR_MAJOR && kernel32minor > SAL_MANIFESTED_FOR_MINOR)
        {
            kernel32major = SAL_MANIFESTED_FOR_MAJOR;
            kernel32minor = SAL_MANIFESTED_FOR_MINOR;
            TRACE_I("kernel32.dll version was limited by Salamander's manifest to: " << kernel32major << ":" << kernel32minor);
        }
        if (kernel32major > os.dwMajorVersion || kernel32major == os.dwMajorVersion && kernel32minor > os.dwMinorVersion)
            return TRUE;
    }
    return FALSE;
}

void GetCommandLineParamExpandEnvVars(const char* argv, char* target, DWORD targetSize, BOOL hotpathForJumplist)
{
    char curDir[MAX_PATH];
    if (hotpathForJumplist)
    {
        BOOL ret = ExpandHotPath(NULL, argv, target, targetSize, FALSE); // If the path syntax is invalid, TRACE_E will fire, which is fine here.
        if (!ret)
        {
            TRACE_E("ExpandHotPath failed.");
            // If expansion fails, use the original unexpanded string.
            lstrcpyn(target, argv, targetSize);
        }
    }
    else
    {
        DWORD auxRes = ExpandEnvironmentStrings(argv, target, targetSize); // Users wanted to be able to pass environment variables as parameters.
        if (auxRes == 0 || auxRes > targetSize)
        {
            TRACE_E("ExpandEnvironmentStrings failed.");
            // If expansion fails, use the original string unchanged.
            lstrcpyn(target, argv, targetSize);
        }
    }
    if (!IsPluginFSPath(target) && GetCurrentDirectory(MAX_PATH, curDir))
    {
        SalGetFullName(target, NULL, curDir, NULL, NULL, targetSize);
    }
}

// Returns TRUE if the parameters are valid; otherwise returns FALSE
BOOL ParseCommandLineParameters(LPSTR cmdLine, CCommandLineParams* cmdLineParams)
{
    // We do not want to change the paths, icon, or prefix, so everything has to be zeroed out.
    ZeroMemory(cmdLineParams, sizeof(CCommandLineParams));

    char buf[4096];
    char* argv[20];
    int p = 20; // number of elements in the argv array

    char curDir[MAX_PATH];
    GetModuleFileName(HInstance, ConfigurationName, MAX_PATH);
    *(strrchr(ConfigurationName, '\\') + 1) = 0;
    const char* configReg = "config.reg";
    strcat(ConfigurationName, configReg);
    if (!FileExists(ConfigurationName) && GetOurPathInRoamingAPPDATA(curDir) &&
        SalPathAppend(curDir, configReg, MAX_PATH) && FileExists(curDir))
    { // If config.reg is not found next to the .exe, also look for it in APPDATA.
        lstrcpyn(ConfigurationName, curDir, MAX_PATH);
        ConfigurationNameIgnoreIfNotExists = FALSE;
    }
    OpenReadmeInNotepad[0] = 0;
    if (GetCmdLine(buf, _countof(buf), argv, p, cmdLine))
    {
        int i;
        for (i = 0; i < p; i++)
        {
            if (StrICmp(argv[i], "-l") == 0) // left panel path
            {
                if (i + 1 < p)
                {
                    GetCommandLineParamExpandEnvVars(argv[i + 1], cmdLineParams->LeftPath, 2 * MAX_PATH, FALSE);
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-r") == 0) // right panel path
            {
                if (i + 1 < p)
                {
                    GetCommandLineParamExpandEnvVars(argv[i + 1], cmdLineParams->RightPath, 2 * MAX_PATH, FALSE);
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-a") == 0) // active panel path
            {
                if (i + 1 < p)
                {
                    GetCommandLineParamExpandEnvVars(argv[i + 1], cmdLineParams->ActivePath, 2 * MAX_PATH, FALSE);
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-aj") == 0) // active panel path (Hot Paths syntax for Jump List) - internal, undocumented
            {
                if (i + 1 < p)
                {
                    GetCommandLineParamExpandEnvVars(argv[i + 1], cmdLineParams->ActivePath, 2 * MAX_PATH, TRUE);
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-c") == 0) // default config file
            {
                if (i + 1 < p)
                {
                    char* s = argv[i + 1];
                    if (*s == '\\' && *(s + 1) == '\\' || // UNC full path
                        *s != 0 && *(s + 1) == ':')       // full path
                    {                                     // full path
                        lstrcpyn(ConfigurationName, argv[i + 1], MAX_PATH);
                    }
                    else // relative path
                    {
                        GetModuleFileName(HInstance, ConfigurationName, MAX_PATH);
                        *(strrchr(ConfigurationName, '\\') + 1) = 0;
                        SalPathAppend(ConfigurationName, s, MAX_PATH);
                        if (!FileExists(ConfigurationName) && GetOurPathInRoamingAPPDATA(curDir) &&
                            SalPathAppend(curDir, s, MAX_PATH) && FileExists(curDir))
                        { // If the file specified after -C relative to the .exe does not exist, also look for it in APPDATA.
                            lstrcpyn(ConfigurationName, curDir, MAX_PATH);
                        }
                    }
                    ConfigurationNameIgnoreIfNotExists = FALSE;
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-i") == 0) // icon index
            {
                if (i + 1 < p)
                {
                    char* s = argv[i + 1];
                    if ((*s == '0' || *s == '1' || *s == '2' || *s == '3') && *(s + 1) == 0) // 0, 1, 2, 3
                    {
                        Configuration.MainWindowIconIndexForced = (*s - '0');

                        cmdLineParams->SetMainWindowIconIndex = TRUE;
                        cmdLineParams->MainWindowIconIndex = Configuration.MainWindowIconIndexForced;
                    }
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-t") == 0) // title bar prefix
            {
                if (i + 1 < p)
                {
                    Configuration.UseTitleBarPrefixForced = TRUE;
                    char* s = argv[i + 1];
                    if (*s != 0)
                    {
                        lstrcpyn(Configuration.TitleBarPrefixForced, s, TITLE_PREFIX_MAX);

                        cmdLineParams->SetTitlePrefix = TRUE;
                        lstrcpyn(cmdLineParams->TitlePrefix, s, MAX_PATH);
                    }
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-o") == 0) // behave as if OnlyOneInstance were enabled
            {
                Configuration.ForceOnlyOneInstance = TRUE;
                continue;
            }

            if (StrICmp(argv[i], "-p") == 0) // activate panel
            {
                if (i + 1 < p)
                {
                    char* s = argv[i + 1];
                    if ((*s == '0' || *s == '1' || *s == '2') && *(s + 1) == 0) // 0, 1, 2
                    {
                        cmdLineParams->ActivatePanel = (*s - '0');
                    }
                    i++;
                    continue;
                }
            }

            if (StrICmp(argv[i], "-run_notepad") == 0 && i + 1 < p)
            { // Vista+: after installation, the installer (SFX7ZIP) runs Salamander and asks it to launch Notepad with the readme file
                lstrcpyn(OpenReadmeInNotepad, argv[i + 1], MAX_PATH);
                i++;
                continue;
            }

            return FALSE; // invalid parameters
        }
    }
    return TRUE;
}

int WinMainBody(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR cmdLine, int cmdShow)
{
    int myExitCode = 1;

    //--- I do not want any critical errors such as "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    // seed the random number generator
    srand((unsigned)time(NULL) ^ (unsigned)_getpid());

#ifdef _DEBUG
    // #define _CRTDBG_ALLOC_MEM_DF        0x01  /* Turn on debug allocation */ (DEFAULT ON)
    // #define _CRTDBG_DELAY_FREE_MEM_DF   0x02  /* Don't actually free memory */
    // #define _CRTDBG_CHECK_ALWAYS_DF     0x04  /* Check heap every alloc/dealloc */
    // #define _CRTDBG_RESERVED_DF         0x08  /* Reserved - do not use */
    // #define _CRTDBG_CHECK_CRT_DF        0x10  /* Leak check/diff CRT blocks */
    // #define _CRTDBG_LEAK_CHECK_DF       0x20  /* Leak check at program exit */

    // If you suspect overwriting allocated memory, uncomment the following two lines.
    // Salamander will slow down and each free/alloc will run a heap consistency check.
    // int crtDbg = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);   // Get the current bits
    // _CrtSetDbgFlag(crtDbg | _CRTDBG_CHECK_ALWAYS_DF);
    // _CrtSetDbgFlag(crtDbg | _CRTDBG_LEAK_CHECK_DF);

    // Another useful debugging function: if a memory leak occurs, the number in braces
    // is the decimal order of the allocated block, for example _CRT_WARN: {104200};
    // _CrtSetBreakAlloc lets you break on this block.
    // _CrtSetBreakAlloc(33521);

    LastCrtCheckMemoryTime = GetTickCount();

    // In this end-of-buffer overwrite case, the protection kicks in: in IDLE a message box is shown
    // and debug messages are sent to TraceServer.
    //
//  char *p1 = (char*)malloc( 4 );
//  strcpy( p1 , "Oops" );
#endif //_DEBUG

    /*
       // test "Heap Block Corruptions: Full-page heap", see http://support.microsoft.com/kb/286470
       // allocates all blocks (they must be at least 16 bytes) so that there is an inaccessible page after them, so
       // any overwrite past the end of a block leads to an exception
       // install Debugging Tools for Windows, in gflags.exe select "Enable page heap" for "salamand.exe",
       // it worked for me on W2K and XP (it should work on Vista too)
       // OR: use the prepared sal-pageheap-register.reg and sal-pageheap-unregister.reg files (then there is no
       // need to install Debugging Tools for Windows)
       //
       // December 2011: I tested this under VS2008 + page heap + Win7x64 and the overwrite below does not raise an exception
       // I found a description of allocation in this mode here: http://msdn.microsoft.com/en-us/library/ms220938(v=VS.90).aspx

      char *test = (char *)malloc(16);
    //  char *test = (char *)HeapAlloc(GetProcessHeap(), 0, 16);
      char bufff[100];
      sprintf(bufff, "test=%p", test);
      MessageBox(NULL, bufff, "a", MB_OK);
      test[16] = 0;
    */

    char testCharValue = 129;
    int testChar = testCharValue;
    if (testChar != 129) // If testChar is negative, we have a problem: LowerCase[testCharValue] indexes outside the array bounds...
    {
        MessageBox(NULL, "Default type 'char' is not 'unsigned char', but 'signed char'. See '/J' compiler switch in MSVC.",
                   "Compilation Error", MB_OK | MB_ICONSTOP);
    }

    MainThreadID = GetCurrentThreadId();
    HInstance = hInstance;
    CALL_STACK_MESSAGE4("WinMainBody(0x%p, , %s, %d)", hInstance, cmdLine, cmdShow);

    // I cannot help this one... if the competition does it, we have to do it too
    // - inspired by Explorer.
    // I used to wonder why their painting worked so nicely.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    SetTraceProcessName("Salamander");
    SetThreadNameInVCAndTrace("Main");
    SetMessagesTitle(MAINWINDOW_NAME);
    TRACE_I("Begin");

    // OLE initialization
    if (FAILED(OleInitialize(NULL)))
    {
        TRACE_E("Error in CoInitialize.");
        return 1;
    }

    //  HOldWPHookProc = SetWindowsHookEx(WH_CALLWNDPROC,     // HANDLES cannot do this!
    //                                    WPMessageHookProc,
    //                                    NULL, GetCurrentThreadId());

    User32DLL = NOHANDLES(LoadLibrary("user32.dll"));
    if (User32DLL == NULL)
        TRACE_E("Unable to load library user32.dll."); // Not a fatal error.

    TurnOFFWindowGhosting();

    NtDLL = HANDLES(LoadLibrary("NTDLL.DLL"));
    if (NtDLL == NULL)
        TRACE_E("Unable to load library ntdll.dll."); // not a fatal error

    // detect the user's default charset for fonts
    CHARSETINFO ci;
    memset(&ci, 0, sizeof(ci));
    char bufANSI[10];
    if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, bufANSI, 10))
    {
        if (TranslateCharsetInfo((DWORD*)(DWORD_PTR)MAKELONG(atoi(bufANSI), 0), &ci, TCI_SRCCODEPAGE))
        {
            UserCharset = ci.ciCharset;
        }
    }

    // Get the allocation granularity needed for memory-mapped files.
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    AllocationGranularity = si.dwAllocationGranularity;

    // Windows Versions supported by Open Salamander
    //
    // Name               wMajorVersion  wMinorVersion
    //------------------------------------------------
    // Windows XP         5              1
    // Windows XP x64     5              2
    // Windows Vista      6              0
    // Windows 7          6              1
    // Windows 8          6              2
    // Windows 8.1        6              3
    // Windows 10         10             0             (note: preview builds of W10 from 2014 reported version 6.4)

    if (!SalIsWindowsVersionOrGreater(6, 1, 0))
    {
        // We will probably never get here; on older systems, exports from statically linked
        // libraries will be missing and the user will get some incomprehensible PE-loader-level error in Windows.
        // Do not call SalMessageBox.
        MessageBox(NULL, "You need at least Windows 7 to run this program.",
                   SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONEXCLAMATION);
    EXIT_1:
        if (User32DLL != NULL)
        {
            NOHANDLES(FreeLibrary(User32DLL));
            User32DLL = NULL;
        }
        if (NtDLL != NULL)
        {
            HANDLES(FreeLibrary(NtDLL));
            NtDLL = NULL;
        }
        return myExitCode;
    }

    WindowsVistaAndLater = SalIsWindowsVersionOrGreater(6, 0, 0);
    WindowsXP64AndLater = SalIsWindowsVersionOrGreater(5, 2, 0);
    Windows7AndLater = SalIsWindowsVersionOrGreater(6, 1, 0);
    Windows8AndLater = SalIsWindowsVersionOrGreater(6, 2, 0);
    Windows8_1AndLater = SalIsWindowsVersionOrGreater(6, 3, 0);
    Windows10AndLater = SalIsWindowsVersionOrGreater(10, 0, 0);

    DWORD integrityLevel;
    if (GetProcessIntegrityLevel(&integrityLevel) && integrityLevel >= SECURITY_MANDATORY_HIGH_RID)
        RunningAsAdmin = TRUE;

    // If possible, use GetNativeSystemInfo; otherwise keep the result from GetSystemInfo.
    typedef void(WINAPI * PGNSI)(LPSYSTEM_INFO);
    PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo"); // Min: XP
    if (pGNSI != NULL)
        pGNSI(&si);
    Windows64Bit = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;

    if (!GetWindowsDirectory(WindowsDirectory, MAX_PATH))
        WindowsDirectory[0] = 0;

    // We care about the ITaskbarList3 interface, introduced by Microsoft in Windows 7, for example taskbar button progress.
    if (Windows7AndLater)
    {
        TaskbarBtnCreatedMsg = RegisterWindowMessage("TaskbarButtonCreated");
        if (TaskbarBtnCreatedMsg == 0)
        {
            DWORD err = GetLastError();
            TRACE_E("RegisterWindowMessage() failed for 'TaskbarButtonCreated'. Error:" << err);
        }
    }

    // The global variables are set, so we can initialize this mutex.
    if (!TaskList.Init())
        TRACE_E("TaskList.Init() failed!");

    if (!InitializeWinLib())
        goto EXIT_1; // We must initialize WinLib before the wait dialog is shown for the first time
                     // because its window classes must already be registered.
                     // ImportConfiguration may already open this dialog.

    LoadSaveToRegistryMutex.Init();

    // Try to read the "AutoImportConfig" value from the current configuration; it exists when we are doing an upgrade.
    BOOL autoImportConfig = FALSE;
    char autoImportConfigFromKey[200];
    autoImportConfigFromKey[0] = 0;
    if (!GetUpgradeInfo(&autoImportConfig, autoImportConfigFromKey, 200)) // the user requested that the application exit
    {
        myExitCode = 0;
    EXIT_1a:
        ReleaseWinLib();
        goto EXIT_1;
    }
    const char* configKey = autoImportConfig ? autoImportConfigFromKey : SalamanderConfigurationRoots[0];

    // Try to get the key that specifies the language from the current configuration.
    LoadSaveToRegistryMutex.Enter();
    HKEY hSalamander;
    DWORD langChanged = FALSE; // TRUE = Salamander is being started for the first time in a different language (load all plugins to verify that this language version is available for them as well, or let the user choose which fallback versions to use)
    if (OpenKey(HKEY_CURRENT_USER, configKey, hSalamander))
    {
        HKEY actKey;
        DWORD configVersion = 1; // config from 1.52 and earlier
        if (OpenKey(hSalamander, SALAMANDER_VERSION_REG, actKey))
        {
            configVersion = 2; // config from 1.6b1 or later
            GetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD,
                     &configVersion, sizeof(DWORD));
            CloseKey(actKey);
        }
        if (configVersion >= 59 /* Before 2.53 beta 2, only English was available, so reading this setting is pointless; offer the user the system default language or a manual language selection. */ &&
            OpenKey(hSalamander, SALAMANDER_CONFIG_REG, actKey))
        {
            GetValue(actKey, CONFIG_LANGUAGE_REG, REG_SZ,
                     Configuration.SLGName, MAX_PATH);
            GetValue(actKey, CONFIG_USEALTLANGFORPLUGINS_REG, REG_DWORD,
                     &Configuration.UseAsAltSLGInOtherPlugins, sizeof(DWORD));
            GetValue(actKey, CONFIG_ALTLANGFORPLUGINS_REG, REG_SZ,
                     Configuration.AltPluginSLGName, MAX_PATH);
            GetValue(actKey, CONFIG_LANGUAGECHANGED_REG, REG_DWORD, &langChanged, sizeof(DWORD));
            CloseKey(actKey);
        }
        CloseKey(hSalamander);
    }
    LoadSaveToRegistryMutex.Leave();

FIND_NEW_SLG_FILE:

    // If the key does not exist, show the selection dialog.
    BOOL newSLGFile = FALSE; // TRUE if the .SLG file was selected during this run of Salamander
    if (Configuration.SLGName[0] == 0)
    {
        CLanguageSelectorDialog slgDialog(NULL, Configuration.SLGName, NULL);
        slgDialog.Initialize();
        if (slgDialog.GetLanguagesCount() == 0)
        {
            MessageBox(NULL, "Unable to find any language file (.SLG) in subdirectory LANG.\n"
                             "Please reinstall Open Salamander.",
                       SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
            goto EXIT_1a;
        }
        Configuration.UseAsAltSLGInOtherPlugins = FALSE;
        Configuration.AltPluginSLGName[0] = 0;

        char prevVerSLGName[MAX_PATH];
        if (!autoImportConfig &&                            // This is pointless during an upgrade (the language is read a few lines above, and this routine would only read it again)
            FindLanguageFromPrevVerOfSal(prevVerSLGName) && // Import the language from the previous version; it will likely still be preferred when importing an old Salamander configuration.
            slgDialog.SLGNameExists(prevVerSLGName))
        {
            lstrcpy(Configuration.SLGName, prevVerSLGName);
        }
        else
        {
            int langIndex = slgDialog.GetPreferredLanguageIndex(NULL, TRUE);
            if (langIndex == -1) // this installation does not contain a language matching the current Windows user locale
            {

// If this is commented out, users will not be sent to download language versions from the web, for example when none are available.
// JRY: For Altap Salamander 2.53, which ships with Czech, German, and English, send users looking for other languages to the forum section
//      "Translations" https://forum.altap.cz/viewforum.php?f=23 - perhaps this will motivate someone to create their own translation.
#define OFFER_OTHERLANGUAGE_VERSIONS

#ifndef OFFER_OTHERLANGUAGE_VERSIONS
                if (slgDialog.GetLanguagesCount() == 1)
                    slgDialog.GetSLGName(Configuration.SLGName); // if there is only one language available, use it
                else
                {
#endif // OFFER_OTHERLANGUAGE_VERSIONS

                    // Open the language selection dialog so the user can download and install additional languages.
                    if (slgDialog.Execute() == IDCANCEL)
                        goto EXIT_1a;

#ifndef OFFER_OTHERLANGUAGE_VERSIONS
                }
#endif // OFFER_OTHERLANGUAGE_VERSIONS
            }
            else
            {
                slgDialog.GetSLGName(Configuration.SLGName, langIndex); // If a language matching the current Windows user locale is available, use it.
            }
        }
        newSLGFile = TRUE;
        langChanged = TRUE;
    }

    char path[MAX_PATH];
    char errorText[MAX_PATH + 200];
    GetModuleFileName(NULL, path, MAX_PATH);
    sprintf(strrchr(path, '\\') + 1, "lang\\%s", Configuration.SLGName);
    HLanguage = HANDLES(LoadLibrary(path));
    LanguageID = 0;
    if (HLanguage == NULL || !IsSLGFileValid(HInstance, HLanguage, LanguageID, IsSLGIncomplete))
    {
        if (HLanguage != NULL)
            HANDLES(FreeLibrary(HLanguage));
        if (!newSLGFile) // The saved .SLG file probably no longer exists; try to find another one.
        {
            sprintf(errorText, "File %s was not found or is not valid language file.\nOpen Salamander "
                               "will try to search for some other language file (.SLG).",
                    path);
            MessageBox(NULL, errorText, SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
            Configuration.SLGName[0] = 0;
            goto FIND_NEW_SLG_FILE;
        }
        else // This should not happen at all - the .SLG file has already been validated
        {
            sprintf(errorText, "File %s was not found or is not valid language file.\n"
                               "Please run Open Salamander again and try to choose some other language file.",
                    path);
            MessageBox(NULL, errorText, "Open Salamander", MB_OK | MB_ICONERROR);
            goto EXIT_1a;
        }
    }

    strcpy(Configuration.LoadedSLGName, Configuration.SLGName);

    // Let the already running Salmon load the selected SLG (so far it used a provisional one).
    SalmonSetSLG(Configuration.SLGName);

    // Set localized messages in the ALLOCHAN module (it provides out-of-memory messages for the user, a Retry button,
    // and if everything fails, even Cancel to terminate the application).
    SetAllocHandlerMessage(LoadStr(IDS_ALLOCHANDLER_MSG), SALAMANDER_TEXT_VERSION,
                           LoadStr(IDS_ALLOCHANDLER_WRNIGNORE), LoadStr(IDS_ALLOCHANDLER_WRNABORT));

    CCommandLineParams cmdLineParams;
    if (!ParseCommandLineParameters(cmdLine, &cmdLineParams))
    {
        SalMessageBox(NULL, LoadStr(IDS_INVALIDCMDLINE), SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);

    EXIT_2:
        if (HLanguage != NULL)
            HANDLES(FreeLibrary(HLanguage));
        goto EXIT_1a;
    }

    if (RunningInCompatibilityMode())
    {
        CCommonDialog dlg(HLanguage, IDD_COMPATIBILITY_MODE, NULL);
        if (dlg.Execute() == IDCANCEL)
            goto EXIT_2;
    }

#ifdef USE_BETA_EXPIRATION_DATE
    // Beta versions are time-limited; see BETA_EXPIRATION_DATE.
    // If today is the date specified by this variable or any later date, show a dialog and exit.
    SYSTEMTIME st;
    GetLocalTime(&st);
    SYSTEMTIME* expire = &BETA_EXPIRATION_DATE;
    if (st.wYear > expire->wYear ||
        (st.wYear == expire->wYear && st.wMonth > expire->wMonth) ||
        (st.wYear == expire->wYear && st.wMonth == expire->wMonth && st.wDay >= expire->wDay))
    {
        if (ShowBetaExpDlg() == IDCANCEL)
            goto EXIT_2;
    }
#endif // USE_BETA_EXPIRATION_DATE

    // Open the splash screen.

    GetSystemDPI(NULL);

    // If the configuration does not exist yet or is later changed when importing from a file, the user
    // is out of luck and the splash screen will use the implicit or old value.
    LoadSaveToRegistryMutex.Enter();
    if (OpenKey(HKEY_CURRENT_USER, configKey, hSalamander))
    {
        HKEY actKey;
        if (OpenKey(hSalamander, SALAMANDER_CONFIG_REG, actKey))
        {
            GetValue(actKey, CONFIG_SHOWSPLASHSCREEN_REG, REG_DWORD,
                     &Configuration.ShowSplashScreen, sizeof(DWORD));
            CloseKey(actKey);
        }
        CloseKey(hSalamander);
    }
    LoadSaveToRegistryMutex.Leave();

    if (Configuration.ShowSplashScreen)
        SplashScreenOpen();

    // The configuration import window contains a list view with checkboxes, so we must initialize common controls.
    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrls.dwICC = ICC_BAR_CLASSES | ICC_LISTVIEW_CLASSES |
                      ICC_TAB_CLASSES | ICC_COOL_CLASSES |
                      ICC_DATE_CLASSES | ICC_USEREX_CLASSES;
    if (!InitCommonControlsEx(&initCtrls))
    {
        TRACE_E("InitCommonControlsEx failed");
        SplashScreenCloseIfExist();
        goto EXIT_2;
    }

    SetWinLibStrings(LoadStr(IDS_INVALIDNUMBER), MAINWINDOW_NAME); // j.r. - move this to the proper place

    // Initialize packers; previously done in constructors; now moved here,
    // after the language DLL has been chosen.
    PackerFormatConfig.InitializeDefaultValues();
    ArchiverConfig.InitializeDefaultValues();
    PackerConfig.InitializeDefaultValues();
    UnpackerConfig.InitializeDefaultValues();

    // If the file exists, it will be imported into the registry.
    BOOL importCfgFromFileWasSkipped = FALSE;
    ImportConfiguration(NULL, ConfigurationName, ConfigurationNameIgnoreIfNotExists, autoImportConfig,
                        &importCfgFromFileWasSkipped);

    // handle the transition from the old config to the new one

    // Call a function that attempts to find a configuration matching our program version.
    // If it succeeds, the 'loadConfiguration' variable is set and the function returns
    // TRUE. If such a configuration does not exist yet, the function gradually searches older
    // configurations in the 'SalamanderConfigurationRoots' array (from newest to oldest).
    // If it finds any of these configurations, it displays a dialog and offers to convert it to
    // the current configuration and delete it from the registry. After the last dialog is shown,
    // the function returns TRUE and sets the 'deleteConfigurations' and 'loadConfiguration'
    // variables according to the user's choices. If the user chooses to exit the application,
    // the function returns FALSE.

    // Array specifying indexes of configurations in the 'SalamanderConfigurationRoots' array
    // that should be deleted (0 -> none).
    BOOL deleteConfigurations[SALCFG_ROOTS_COUNT];
    ZeroMemory(deleteConfigurations, sizeof(deleteConfigurations));

    CALL_STACK_MESSAGE1("WinMainBody::FindLatestConfiguration");

    // Pointer into the 'SalamanderConfigurationRoots' array to the configuration that should be
    // loaded (NULL -> none; default values will be used).
    if (autoImportConfig)
        SALAMANDER_ROOT_REG = autoImportConfigFromKey; // When upgrading, searching for a configuration is pointless.
    else
    {
        if (!FindLatestConfiguration(deleteConfigurations, SALAMANDER_ROOT_REG))
        {
            SplashScreenCloseIfExist();
            goto EXIT_2;
        }
    }

    InitializeShellib(); // OLE must be initialized before opening HTML Help - CSalamanderEvaluation

    // If the new configuration key does not exist yet, create it before possibly deleting
    // the old keys.
    BOOL currentCfgDoesNotExist = autoImportConfig || SALAMANDER_ROOT_REG != SalamanderConfigurationRoots[0];
    BOOL saveNewConfig = currentCfgDoesNotExist;

    // If the user does not want multiple instances, only activate the previous one.
    if (!currentCfgDoesNotExist &&
        CheckOnlyOneInstance(&cmdLineParams))
    {
        SplashScreenCloseIfExist();
        myExitCode = 0;
    EXIT_3:
        ReleaseShellib();
        goto EXIT_2;
    }

    // Check the Common Controls version.
    if (GetComCtlVersion(&CCVerMajor, &CCVerMinor) != S_OK) // JRYFIXME - move common controls tests to W7+
    {
        CCVerMajor = 0; // this probably never happens; comctl32.dll is missing
        CCVerMinor = 0;
    }

    CALL_STACK_MESSAGE1("WinMainBody::StartupDialog");

    //  StartupDialog.Open(HLanguage);

    int i;
    for (i = 0; i < NUMBER_OF_COLORS; i++)
        UserColors[i] = SalamanderColors[i];

    //--- initialization section
    CALL_STACK_MESSAGE1("WinMainBody::inicialization");
    IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_DATA));

    InitDefaultDir();
    PackSetErrorHandler(PackErrorHandler);
    InitLocales();

    if (!InitPreloadedStrings())
    {
        SplashScreenCloseIfExist();
    EXIT_4:
        ReleasePreloadedStrings();
        goto EXIT_3;
    }
    if (!InitializeCheckThread() || !InitializeFind())
    {
        SplashScreenCloseIfExist();
    EXIT_5:
        ReleaseCheckThreads();
        goto EXIT_4;
    }
    InitializeMenuWheelHook();
    SetupWinLibHelp(&SalamanderHelp);
    if (!InitializeDiskCache())
    {
        SplashScreenCloseIfExist();
    EXIT_6:
        ReleaseFind();
        goto EXIT_5;
    }
    if (!InitializeConstGraphics())
    {
        SplashScreenCloseIfExist();
    EXIT_7:
        ReleaseConstGraphics();
        goto EXIT_6;
    }
    if (!InitializeGraphics(FALSE))
    {
        SplashScreenCloseIfExist();
    EXIT_8:
        ReleaseGraphics(FALSE);
        goto EXIT_7;
    }
    if (!InitializeMenu() || !BuildSalamanderMenus())
    {
        SplashScreenCloseIfExist();
        goto EXIT_8;
    }
    if (!InitializeThread())
    {
        SplashScreenCloseIfExist();
    EXIT_9:
        TerminateThread();
        goto EXIT_8;
    }
    if (!InitializeViewer())
    {
        SplashScreenCloseIfExist();
        ReleaseViewer();
        goto EXIT_9;
    }

    // OLE Spy attachment
    // moved below InitializeGraphics, which leaked under WinXP (probably some cache again)
    // OleSpyRegister();    // disabled because after the Windows 2000 update from 02/2005, a debug breakpoint started appearing in MSVC when Salam was started and closed: Invalid Address specified to RtlFreeHeap( 130000, 14bc74 ) - MS probably started calling RtlFreeHeap directly somewhere instead of OLE free, and the spy info block at the start of the allocated block broke it (malloc returns a pointer shifted past the spy info block)
    //OleSpySetBreak(2754); // breaks on the nth allocation from the dump

    // Initialize the worker (disk operations).
    InitWorker();

    // initialize the library for communicating with SalShExt/SalamExt/SalExtX86/SalExtX64.DLL (shell copy hook + shell context menu)
    InitSalShLib();

    // initialize the library for working with shell icon overlays (Tortoise SVN + CVS)
    LoadIconOvrlsInfo(SALAMANDER_ROOT_REG);
    InitShellIconOverlays();

    // initialize support for moving to the next/previous file in the panel or Find from the viewer
    InitFileNamesEnumForViewers();

    // Load the list of shared directories
    IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_SHARES));
    Shares.Refresh();

    CMainWindow::RegisterUniversalClass(CS_DBLCLKS | CS_SAVEBITS,
                                        0,
                                        0,
                                        NULL,
                                        LoadCursor(NULL, IDC_ARROW),
                                        (HBRUSH)(COLOR_3DFACE + 1),
                                        NULL,
                                        SAVEBITS_CLASSNAME,
                                        NULL);
    CMainWindow::RegisterUniversalClass(CS_DBLCLKS,
                                        0,
                                        0,
                                        NULL,
                                        LoadCursor(NULL, IDC_ARROW),
                                        (HBRUSH)(COLOR_3DFACE + 1),
                                        NULL,
                                        SHELLEXECUTE_CLASSNAME,
                                        NULL);

    Associations.ReadAssociations(FALSE); // read associations from the Registry

    // Shell extension registration.
    // If we find the library in the "utils" subdirectory, check whether it is registered and register it if needed.
    char shellExtPath[MAX_PATH];
    GetModuleFileName(HInstance, shellExtPath, MAX_PATH);
    char* shellExtPathSlash = strrchr(shellExtPath, '\\');
    if (shellExtPathSlash != NULL)
    {
        strcpy(shellExtPathSlash + 1, "utils\\salextx86.dll");
#ifdef _WIN64
        if (FileExists(shellExtPath))
            SalShExtRegistered = SECRegisterToRegistry(shellExtPath, TRUE, KEY_WOW64_32KEY);
        strcpy(shellExtPathSlash + 1, "utils\\salextx64.dll");
        if (FileExists(shellExtPath))
            SalShExtRegistered &= SECRegisterToRegistry(shellExtPath, FALSE, 0);
        else
            SalShExtRegistered = FALSE;
#else  // _WIN64
        if (FileExists(shellExtPath))
            SalShExtRegistered = SECRegisterToRegistry(shellExtPath, FALSE, 0);
        if (Windows64Bit)
        {
            strcpy(shellExtPathSlash + 1, "utils\\salextx64.dll");
            if (FileExists(shellExtPath))
                SalShExtRegistered &= SECRegisterToRegistry(shellExtPath, TRUE, KEY_WOW64_64KEY);
            else
                SalShExtRegistered = FALSE;
        }
#endif // _WIN64
    }

    //--- create the main window
    if (CMainWindow::RegisterUniversalClass(CS_DBLCLKS | CS_OWNDC,
                                            0,
                                            0,
                                            NULL, // HIcon
                                            LoadCursor(NULL, IDC_ARROW),
                                            NULL /*(HBRUSH)(COLOR_WINDOW + 1)*/, // HBrush
                                            NULL,
                                            CFILESBOX_CLASSNAME,
                                            NULL) &&
        CMainWindow::RegisterUniversalClass(CS_DBLCLKS,
                                            0,
                                            0,
                                            HANDLES(LoadIcon(HInstance,
                                                             MAKEINTRESOURCE(IDI_SALAMANDER))),
                                            LoadCursor(NULL, IDC_ARROW),
                                            (HBRUSH)(COLOR_WINDOW + 1),
                                            NULL,
                                            CMAINWINDOW_CLASSNAME,
                                            NULL))
    {
        MainWindow = new CMainWindow;
        if (MainWindow != NULL)
        {
            MainWindow->CmdShow = cmdShow;
            if (MainWindow->Create(CMAINWINDOW_CLASSNAME,
                                   "",
                                   WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
                                   CW_USEDEFAULT, 0, CW_USEDEFAULT, 0,
                                   NULL,
                                   NULL,
                                   HInstance,
                                   MainWindow))
            {
                SetMessagesParent(MainWindow->HWindow);
                PluginMsgBoxParent = MainWindow->HWindow;

                // read Group Policy from the registry
                IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_POLICY));
                SystemPolicies.LoadFromRegistry();

                CALL_STACK_MESSAGE1("WinMainBody::load_config");
                BOOL setActivePanelAndPanelPaths = FALSE; // The active panel and the panel paths are set in MainWindow->LoadConfig().
                if (!MainWindow->LoadConfig(currentCfgDoesNotExist, !importCfgFromFileWasSkipped ? &cmdLineParams : NULL))
                {
                    setActivePanelAndPanelPaths = TRUE;
                    UpdateDefaultColors(CurrentColors, MainWindow->HighlightMasks, FALSE, TRUE);
                    Plugins.CheckData();
                    MainWindow->InsertMenuBand();
                    if (Configuration.TopToolBarVisible)
                        MainWindow->ToggleTopToolBar();
                    if (Configuration.DriveBarVisible)
                        MainWindow->ToggleDriveBar(Configuration.DriveBar2Visible, FALSE);
                    if (Configuration.PluginsBarVisible)
                        MainWindow->TogglePluginsBar();
                    if (Configuration.MiddleToolBarVisible)
                        MainWindow->ToggleMiddleToolBar();
                    if (Configuration.BottomToolBarVisible)
                        MainWindow->ToggleBottomToolBar();
                    MainWindow->CreateAndInsertWorkerBand(); // finally, insert the worker band
                    MainWindow->LeftPanel->UpdateDriveIcon(TRUE);
                    MainWindow->RightPanel->UpdateDriveIcon(TRUE);
                    MainWindow->LeftPanel->UpdateFilterSymbol();
                    MainWindow->RightPanel->UpdateFilterSymbol();
                    if (!SystemPolicies.GetNoRun())
                        SendMessage(MainWindow->HWindow, WM_COMMAND, CM_TOGGLEEDITLINE, TRUE);
                    MainWindow->SetWindowIcon();
                    MainWindow->SetWindowTitle();
                    SplashScreenCloseIfExist();
                    ShowWindow(MainWindow->HWindow, cmdShow);
                    UpdateWindow(MainWindow->HWindow);
                    MainWindow->RefreshDirs();
                    MainWindow->FocusLeftPanel();
                }

                if (Configuration.ReloadEnvVariables)
                    InitEnvironmentVariablesDifferences();

                if (newSLGFile)
                {
                    Plugins.ClearLastSLGNames(); // so that a new fallback language will be selected for all plugins if needed
                    Configuration.ShowSLGIncomplete = TRUE;
                }

                MainMenu.SetSkillLevel(CfgSkillLevelToMenu(Configuration.SkillLevel));

                if (!MainWindow->IsGood())
                {
                    SetMessagesParent(NULL);
                    DestroyWindow(MainWindow->HWindow);
                    TRACE_E(LOW_MEMORY);
                }
                else
                {
                    if (!importCfgFromFileWasSkipped) // Only if the application does not exit immediately; otherwise this is pointless.
                        MainWindow->ApplyCommandLineParams(&cmdLineParams, setActivePanelAndPanelPaths);

                    if (Windows7AndLater)
                        CreateJumpList();

                    IdleRefreshStates = TRUE;  // Force a check of the state variables on the next idle cycle.
                    IdleCheckClipboard = TRUE; // also request checking the clipboard

                    AccelTable1 = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_MAINACCELS1)));
                    AccelTable2 = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_MAINACCELS2)));

                    MainWindow->CanClose = TRUE; // Only now do we allow the main window to be closed.
                    // so files do not pop up one by one as their icons load
                    UpdateWindow(MainWindow->HWindow);

                    BOOL doNotDeleteImportedCfg = FALSE;
                    if (autoImportConfig && // Check whether the new version has fewer plugins than the old one, so part of the old configuration would not be imported.
                        FindPluginsWithoutImportedCfg(&doNotDeleteImportedCfg))
                    {                               // The app needs to exit without saving the configuration
                        SALAMANDER_ROOT_REG = NULL; // This should reliably prevent the configuration from being written to the registry.
                        PostMessage(MainWindow->HWindow, WM_USER_FORCECLOSE_MAINWND, 0, 0);
                    }
                    else
                    {
                        if (Configuration.ConfigVersion < THIS_CONFIG_VERSION
#ifndef _WIN64 // FIXME_X64_WINSCP
                            || Configuration.AddX86OnlyPlugins
#endif // _WIN64
                        )
                        {                                            // auto-install plug-ins from the standard "plugins" plug-in subdirectory
#ifndef _WIN64                                                       // FIXME_X64_WINSCP
                            Configuration.AddX86OnlyPlugins = FALSE; // only need to do this once
#endif                                                               // _WIN64
                            Plugins.AutoInstallStdPluginsDir(MainWindow->HWindow);
                            Configuration.LastPluginVer = 0;   // when upgrading to a new version, the plugins.ver file will also be removed for the other platform
                            Configuration.LastPluginVerOP = 0; // when upgrading to a new version, the plugins.ver file will also be removed for the other platform
                            saveNewConfig = TRUE;              // The new configuration must be saved so this does not happen again on the next startup.
                        }
                        // Load the plugins.ver file ((re)installing plug-ins), needed even on the first run (in case
                        // plug-ins were installed before Salamander was started for the first time).
                        if (Plugins.ReadPluginsVer(MainWindow->HWindow, Configuration.ConfigVersion < THIS_CONFIG_VERSION))
                            saveNewConfig = TRUE; // the new configuration must be saved (so this does not happen again on the next startup)
                        // load plugins that have the load-on-start flag set
                        Plugins.HandleLoadOnStartFlag(MainWindow->HWindow);
                        // If we are starting for the first time with a changed language, load all plug-ins so it becomes clear
                        // whether they have this language version, and if needed let the user choose replacement languages.
                        if (langChanged)
                            Plugins.LoadAll(MainWindow->HWindow);

                        // The FTP and WinSCP plugins now call SalamanderGeneral->SetPluginUsesPasswordManager() to subscribe to password manager events.
                        // Introduced in configuration version 45; give all plugins a chance to register.
                        if (Configuration.ConfigVersion < 45) // introduce the password manager
                            Plugins.LoadAll(MainWindow->HWindow);

                        // Saving will now go to the newest key.
                        SALAMANDER_ROOT_REG = SalamanderConfigurationRoots[0];
                        // Save the configuration immediately while this is still a clean conversion of the old version; the user may
                        // have "Save Cfg on Exit" disabled and may not want later changes made during the Salamander session to be saved at exit
                        if (saveNewConfig)
                        {
                            MainWindow->SaveConfig();
                        }
                        // scan the array and, if any root is marked for deletion, delete it; also delete the old configuration
                        // after an UPGRADE and also remove the "AutoImportConfig" value from the configuration key for this Salamander version
                        MainWindow->DeleteOldConfigurations(deleteConfigurations, autoImportConfig, autoImportConfigFromKey,
                                                            doNotDeleteImportedCfg);

                        // only the first Salamander instance: check whether TEMP needs to be cleaned up
                        // of unnecessary disk cache files (after a crash or when locked by another application,
                        // files may remain in TEMP)
                        // we must test a global variable (shared across all sessions) so that two
                        // Salamander instances started under Fast User Switching can see each other
                        // Problem reported on the forum: https://forum.altap.cz/viewtopic.php?t=2643
                        if (FirstInstance_3_or_later)
                        {
                            DiskCache.ClearTEMPIfNeeded(MainWindow->HWindow, MainWindow->GetActivePanelHWND());
                        }

                        if (importCfgFromFileWasSkipped) // if importing config.reg or another .reg file was skipped (-C parameter)
                        {                                // notify the user that Salamander needs to be restarted, then let them exit the application
                            MSGBOXEX_PARAMS params;
                            memset(&params, 0, sizeof(params));
                            params.HParent = MainWindow->HWindow;
                            params.Flags = MB_OK | MB_ICONINFORMATION;
                            params.Caption = SALAMANDER_TEXT_VERSION;
                            params.Text = LoadStr(IDS_IMPORTCFGFROMFILESKIPPED);
                            char aliasBtnNames[200];
                            /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
                               for message box buttons, handle hotkey collisions by simulating a menu
                            MENU_TEMPLATE_ITEM MsgBoxButtons[] =
                            {
                              {MNTT_PB, 0
                              {MNTT_IT, IDS_SELLANGEXITBUTTON
                              {MNTT_PE, 0
                            };
                            */
                            sprintf(aliasBtnNames, "%d\t%s", DIALOG_OK, LoadStr(IDS_SELLANGEXITBUTTON));
                            params.AliasBtnNames = aliasBtnNames;
                            SalMessageBoxEx(&params);
                            PostMessage(MainWindow->HWindow, WM_USER_FORCECLOSE_MAINWND, 0, 0);
                        }
                        /*
                                    // If needed, show the Tip of the Day dialog
                                    // 0xffffffff = quiet open; if it fails, do not disturb the user
                                    if (Configuration.ShowTipOfTheDay)
                                      PostMessage(MainWindow->HWindow, WM_COMMAND, CM_HELP_TIP, 0xffffffff);
                          */
                    }

                    // paths being closed will now be remembered
                    MainWindow->CanAddToDirHistory = TRUE;

                    // Users want the startup path kept in the history even if they have not worked with it yet
                    MainWindow->LeftPanel->UserWorkedOnThisPath = TRUE;
                    MainWindow->RightPanel->UserWorkedOnThisPath = TRUE;

                    // Tell the process list that we are running and have a main window (so it can activate us in OnlyOneInstance mode)
                    TaskList.SetProcessState(PROCESS_STATE_RUNNING, MainWindow->HWindow);

                    // Ask Salmon to check whether there are any old bug reports on disk that need to be sent.
                    SalmonCheckBugs();

                    if (IsSLGIncomplete[0] != 0 && Configuration.ShowSLGIncomplete)
                        PostMessage(MainWindow->HWindow, WM_USER_SLGINCOMPLETE, 0, 0);

                    //--- message loop
                    CALL_STACK_MESSAGE1("WinMainBody::message_loop");
                    DWORD activateParamsRequestUID = 0;
                    BOOL skipMenuBar;
                    MSG msg;
                    BOOL haveMSG = FALSE; // FALSE if GetMessage() should be called in the loop condition
                    while (haveMSG || GetMessage(&msg, NULL, 0, 0))
                    {
                        haveMSG = FALSE;
                        if (msg.message != WM_USER_SHOWWINDOW && msg.message != WM_USER_WAKEUP_FROM_IDLE && /*msg.message != WM_USER_SETPATHS &&*/
                            msg.message != WM_QUERYENDSESSION && msg.message != WM_USER_SALSHEXT_PASTE &&
                            msg.message != WM_USER_CLOSE_MAINWND && msg.message != WM_USER_FORCECLOSE_MAINWND)
                        { // All messages except "connect", "shutdown", "do-paste", and "close-main-wnd" put Salamander into BUSY mode.
                            SalamanderBusy = TRUE;
                            LastSalamanderIdleTime = GetTickCount();
                        }

                        if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN) &&
                            msg.wParam != VK_MENU && msg.wParam != VK_CONTROL && msg.wParam != VK_SHIFT)
                        {
                            SetCurrentToolTip(NULL, 0); // Hide the tooltip.
                        }

                        skipMenuBar = FALSE;
                        if (Configuration.QuickSearchEnterAlt && msg.message == WM_SYSCHAR)
                            skipMenuBar = TRUE;

                        // Ensure messages are sent to our menu (this avoids the need for a keyboard hook).
                        if (MainWindow == NULL || MainWindow->MenuBar == NULL || !MainWindow->CaptionIsActive ||
                            MainWindow->QuickRenameWindowActive() ||
                            skipMenuBar || GetCapture() != NULL || // if the mouse is captured, we could cause visual glitches
                            !MainWindow->MenuBar->IsMenuBarMessage(&msg))
                        {
                            CWindowsObject* wnd = WindowsManager.GetWindowPtr(GetActiveWindow());

                            // Bottom toolbar: update text based on VK_CTRL, VK_MENU, and VK_SHIFT
                            if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN ||
                                 msg.message == WM_SYSKEYUP || msg.message == WM_KEYUP) &&
                                MainWindow != NULL)
                                MainWindow->UpdateBottomToolBar();

                            if ((wnd == NULL || !wnd->Is(otDialog) ||
                                 !IsDialogMessage(wnd->HWindow, &msg)) &&
                                (MainWindow == NULL || !MainWindow->CaptionIsActive || // Added "!MainWindow->CaptionIsActive" so accelerators are not processed in non-modal plugin windows (F7 in "FTP Logs" is not a great idea).
                                 MainWindow->QuickRenameWindowActive() ||
                                 !TranslateAccelerator(MainWindow->HWindow, AccelTable1, &msg) &&
                                     (MainWindow->EditMode || !TranslateAccelerator(MainWindow->HWindow, AccelTable2, &msg))))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }

                        if (MainWindow != NULL && MainWindow->CanClose)
                        { // if Salamander has started, it can be marked as not busy
                            SalamanderBusy = FALSE;
                        }

                    TEST_IDLE:
                        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                        {
                            if (msg.message == WM_QUIT)
                                break;      // equivalent to when GetMessage() returns FALSE
                            haveMSG = TRUE; // We have a message, so process it without calling GetMessage().
                        }
                        else // If there is no message in the queue, perform idle processing
                        {
#ifdef _DEBUG
                            // Check heap consistency every three seconds.
                            if (_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & _CRTDBG_ALLOC_MEM_DF)
                            {
                                if (GetTickCount() - LastCrtCheckMemoryTime > 3000) // every three seconds
                                {
                                    if (!_CrtCheckMemory())
                                    {
                                        HWND hParent = NULL;
                                        if (MainWindow != NULL)
                                            hParent = MainWindow->HWindow;
                                        MessageBox(hParent, "_CrtCheckMemory failed. Look to the Trace Server for details.", "Open Salamander", MB_OK | MB_ICONERROR);
                                    }
                                    LastCrtCheckMemoryTime = GetTickCount();
                                }
                            }
#endif //_DEBUG

                            if (MainWindow != NULL)
                            {
                                CannotCloseSalMainWnd = TRUE; // Prevent the Salamander main window from closing while the following routines run.
                                MainWindow->OnEnterIdle();

                                // Wait for ESC to be released only if a panel listing refresh follows immediately
                                // which cannot happen via IDLE.
                                if (WaitForESCReleaseBeforeTestingESC)
                                    WaitForESCReleaseBeforeTestingESC = FALSE;

                                // check whether another "OnlyOneInstance" Salamander is requesting activation and panel path setup?
                                // In that case, FControlThread would store the parameters in the global CommandLineParams and increment RequestUID.
                                // If the main thread was idle, it woke up because WM_USER_WAKEUP_FROM_IDLE was posted.
                                if (!SalamanderBusy && CommandLineParams.RequestUID > activateParamsRequestUID)
                                {
                                    CCommandLineParams paramsCopy;
                                    BOOL applyParams = FALSE;

                                    NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                                    // A timeout may have occurred in the control thread just before entering the critical section; verify that it still wants the result.
                                    // Also verify that the request has not expired (the calling thread waits only until TASKLIST_TODO_TIMEOUT, then
                                    // gives up and starts a new Salamander instance; in that case we do not want to fulfill the request).
                                    DWORD tickCount = GetTickCount();
                                    if (CommandLineParams.RequestUID != 0 && tickCount - CommandLineParams.RequestTimestamp < TASKLIST_TODO_TIMEOUT)
                                    {
                                        memcpy(&paramsCopy, &CommandLineParams, sizeof(CCommandLineParams));
                                        applyParams = TRUE;

                                        // Remember the UID that has already been handled so we do not loop.
                                        activateParamsRequestUID = CommandLineParams.RequestUID;
                                        // signal to the control thread that the paths have been accepted
                                        SetEvent(CommandLineParamsProcessed);
                                    }
                                    NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));

                                    // the shared resources have been released; the paths can now be handled
                                    if (applyParams && MainWindow != NULL)
                                    {
                                        SendMessage(MainWindow->HWindow, WM_USER_SHOWWINDOW, 0, 0);
                                        MainWindow->ApplyCommandLineParams(&paramsCopy);
                                    }
                                }

                                // Ensure we switch away from removed drives to a fixed drive (after device ejection - USB flash drive, etc.).
                                if (!SalamanderBusy && ChangeLeftPanelToFixedWhenIdle)
                                {
                                    ChangeLeftPanelToFixedWhenIdle = FALSE;
                                    ChangeLeftPanelToFixedWhenIdleInProgress = TRUE;
                                    if (MainWindow != NULL && MainWindow->LeftPanel != NULL)
                                        MainWindow->LeftPanel->ChangeToRescuePathOrFixedDrive(MainWindow->LeftPanel->HWindow);
                                    ChangeLeftPanelToFixedWhenIdleInProgress = FALSE;
                                }
                                if (!SalamanderBusy && ChangeRightPanelToFixedWhenIdle)
                                {
                                    ChangeRightPanelToFixedWhenIdle = FALSE;
                                    ChangeRightPanelToFixedWhenIdleInProgress = TRUE;
                                    if (MainWindow != NULL && MainWindow->RightPanel != NULL)
                                        MainWindow->RightPanel->ChangeToRescuePathOrFixedDrive(MainWindow->RightPanel->HWindow);
                                    ChangeRightPanelToFixedWhenIdleInProgress = FALSE;
                                }
                                if (!SalamanderBusy && OpenCfgToChangeIfPathIsInaccessibleGoTo)
                                {
                                    OpenCfgToChangeIfPathIsInaccessibleGoTo = FALSE;
                                    if (MainWindow != NULL)
                                        PostMessage(MainWindow->HWindow, WM_USER_CONFIGURATION, 6, 0);
                                }

                                // If any plugin requested an unload or a menu rebuild, handle it... (but only when we're not "busy")
                                if (!SalamanderBusy && ExecCmdsOrUnloadMarkedPlugins)
                                {
                                    int cmd;
                                    CPluginData* data;
                                    Plugins.GetCmdAndUnloadMarkedPlugins(MainWindow->HWindow, &cmd, &data);
                                    ExecCmdsOrUnloadMarkedPlugins = (cmd != -1);
                                    if (cmd >= 0 && cmd < 500) // Execute a Salamander command at the request of a plugin
                                    {
                                        int wmCmd = GetWMCommandFromSalCmd(cmd);
                                        if (wmCmd != -1)
                                        {
                                            // Generate a WM_COMMAND and process it immediately
                                            msg.hwnd = MainWindow->HWindow;
                                            msg.message = WM_COMMAND;
                                            msg.wParam = (DWORD)LOWORD(wmCmd); // better strip off the high WORD (0 for menu commands)
                                            msg.lParam = 0;
                                            msg.time = GetTickCount();
                                            GetCursorPos(&msg.pt);

                                            haveMSG = TRUE; // We already have a message, so process it without calling GetMessage().
                                        }
                                    }
                                    else
                                    {
                                        if (cmd >= 500 && cmd < 1000500) // Execute the menuExt command at the plugin's request
                                        {
                                            int id = cmd - 500;
                                            SalamanderBusy = TRUE; // We are about to execute the menu command, so we are busy again.
                                            LastSalamanderIdleTime = GetTickCount();
                                            if (data != NULL && data->GetLoaded())
                                            {
                                                if (data->GetPluginInterfaceForMenuExt()->NotEmpty())
                                                {
                                                    CALL_STACK_MESSAGE4("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d,) (%s v. %s)",
                                                                        id, data->DLLName, data->Version);

                                                    // lower the thread priority to "normal" so operations do not put too much load on the machine
                                                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                                                    CSalamanderForOperations sm(MainWindow->GetActivePanel());
                                                    data->GetPluginInterfaceForMenuExt()->ExecuteMenuItem(&sm, MainWindow->HWindow, id, 0);

                                                    // raise the thread priority back to above normal; the operation has finished
                                                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                                                }
                                                else
                                                {
                                                    TRACE_E("Plugin must have PluginInterfaceForMenuExt when "
                                                            "calling CSalamanderGeneral::PostMenuExtCommand()!");
                                                }
                                            }
                                            else
                                            {
                                                // It does not have to be loaded; it is enough for PostMenuExtCommand to be called from the Release plug-in,
                                                // which is triggered by the posted unload.
                                                // TRACE_E("Unexpected situation during call of menu command in \"sal-idle\".");
                                            }
                                            if (MainWindow != NULL && MainWindow->CanClose) // end of menu command processing
                                            {                                               // if Salamander has started up, we can mark it as not busy
                                                SalamanderBusy = FALSE;
                                            }
                                            CannotCloseSalMainWnd = FALSE;
                                            goto TEST_IDLE; // try "idle" again (e.g. so another posted command/unload can be processed)
                                        }
                                    }
                                }
                                if (!SalamanderBusy && OpenPackOrUnpackDlgForMarkedPlugins)
                                {
                                    CPluginData* data;
                                    int pluginIndex;
                                    Plugins.OpenPackOrUnpackDlgForMarkedPlugins(&data, &pluginIndex);
                                    OpenPackOrUnpackDlgForMarkedPlugins = (data != NULL);
                                    if (data != NULL) // Open the Pack/Unpack dialog at the plugin's request
                                    {
                                        SalamanderBusy = TRUE; // We're about to execute the menu command, so we're busy again
                                        LastSalamanderIdleTime = GetTickCount();
                                        if (data->OpenPackDlg)
                                        {
                                            CFilesWindow* activePanel = MainWindow->GetActivePanel();
                                            if (activePanel != NULL && activePanel->Is(ptDisk))
                                            { // open the Pack dialog
                                                MainWindow->CancelPanelsUI();
                                                activePanel->UserWorkedOnThisPath = TRUE;
                                                activePanel->StoreSelection(); // Store the selection for the Restore Selection command.
                                                activePanel->Pack(MainWindow->GetNonActivePanel(), pluginIndex,
                                                                  data->Name, data->PackDlgDelFilesAfterPacking);
                                            }
                                            else
                                                TRACE_E("Unexpected situation: type of active panel is not Disk!");
                                            data->OpenPackDlg = FALSE;
                                            data->PackDlgDelFilesAfterPacking = 0;
                                        }
                                        else
                                        {
                                            if (data->OpenUnpackDlg)
                                            {
                                                CFilesWindow* activePanel = MainWindow->GetActivePanel();
                                                if (activePanel != NULL && activePanel->Is(ptDisk))
                                                { // opening the Unpack dialog
                                                    MainWindow->CancelPanelsUI();
                                                    activePanel->UserWorkedOnThisPath = TRUE;
                                                    activePanel->StoreSelection(); // Save the selection for the Restore Selection command
                                                    activePanel->Unpack(MainWindow->GetNonActivePanel(), pluginIndex,
                                                                        data->Name, data->UnpackDlgUnpackMask);
                                                }
                                                else
                                                    TRACE_E("Unexpected situation: type of active panel is not Disk!");
                                                data->OpenUnpackDlg = FALSE;
                                                if (data->UnpackDlgUnpackMask != NULL)
                                                    free(data->UnpackDlgUnpackMask);
                                                data->UnpackDlgUnpackMask = NULL;
                                            }
                                        }
                                        if (MainWindow != NULL && MainWindow->CanClose) // Pack/Unpack dialog opening has finished.
                                        {                                               // if Salamander has started up, we can mark it as not busy
                                            SalamanderBusy = FALSE;
                                        }
                                        CannotCloseSalMainWnd = FALSE;
                                        goto TEST_IDLE; // try idle again (e.g. so another posted command, unload, or Pack/Unpack request can be processed)
                                    }
                                }
                                if (!SalamanderBusy && OpenReadmeInNotepad[0] != 0)
                                { // Vista+: for the installer, launch Notepad with the file path stored in OpenReadmeInNotepad
                                    StartNotepad(OpenReadmeInNotepad);
                                    OpenReadmeInNotepad[0] = 0;
                                }
                                CannotCloseSalMainWnd = FALSE;
                            }
                        }
                    }
                }
                PluginMsgBoxParent = NULL;
            }
            else
            {
                TRACE_E(LOW_MEMORY);
            }
        }
    }
    else
        TRACE_E("Unable to register main window class.");

    // try to close the dialog if an error occurs
    SplashScreenCloseIfExist();

    // restore the thread priority to its original setting
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    //--- give all windows 1 second to close, then let them detach
    int timeOut = 10;
    int winsCount = WindowsManager.GetCount();
    while (timeOut-- && winsCount > 0)
    {
        Sleep(100);
        int c = WindowsManager.GetCount();
        if (winsCount > c) // Window count is still decreasing, so wait at least 1 more second.
        {
            winsCount = c;
            timeOut = 10;
        }
    }

//--- information
#ifdef __DEBUG_WINLIB
    TRACE_I("WindowsManager: " << WindowsManager.maxWndCount << " windows, " << WindowsManager.search << " searches, " << WindowsManager.cache << " cached searches.");
#endif
    //---
    DestroySafeWaitWindow(TRUE); // send the "terminate" command to the safe-wait-message thread
    Sleep(1000);                 // give all viewer threads time to shut down
    NBWNetAC3Thread.Close(TRUE); // Let shutdown kill any still-running thread by moving it to AuxThreads, and block further actions.
    TerminateAuxThreads();       // forcibly terminate the rest
                                 //---
    TerminateThread();
    ReleaseFileNamesEnumForViewers();
    ReleaseShellIconOverlays();
    ReleaseSalShLib();
    ReleaseWorker();
    ReleaseViewer();
    ReleaseWinLib();
    ReleaseMenuWheelHook();
    ReleaseFind();
    ReleaseCheckThreads();
    ReleasePreloadedStrings();
    ReleaseShellib();
    ReleaseGraphics(FALSE);
    ReleaseConstGraphics();

    HANDLES(FreeLibrary(HLanguage));
    HLanguage = NULL;

    // just to be safe, close it last, though that's probably unnecessary caution
    ReleaseSalOpen();

    if (NtDLL != NULL)
    {
        HANDLES(FreeLibrary(NtDLL));
        NtDLL = NULL;
    }
    if (User32DLL != NULL)
    {
        NOHANDLES(FreeLibrary(User32DLL));
        User32DLL = NULL;
    }

    //OleSpyStressTest(); // multi-threaded stress test
    // OleSpyRevoke();     // detach OLESPY
    OleUninitialize(); // Uninitialize OLE
    // OleSpyDump();       // dump leaks

    TRACE_I("End");
    return 0;
}

int WINAPI
WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR cmdLine, int cmdShow)
{
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE

        //#ifdef MSVC_RUNTIME_CHECKS
        _RTC_SetErrorFuncW(&MyRTCErrorFunc);
        //#endif // MSVC_RUNTIME_CHECKS

        int result = WinMainBody(hInstance, hPrevInstance, cmdLine, cmdShow);

        return result;
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread Main: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // more forceful exit
        return 1;
    }
#endif // CALLSTK_DISABLE
}
