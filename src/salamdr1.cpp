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

#pragma comment(linker, "/ENTRY:MyEntryPoint") // chceme vlastni vstupni bod do aplikace

#pragma comment(lib, "UxTheme.lib")

// zpristupnime si puvodni vstupni bod aplikace
extern "C" int WinMainCRTStartup();

#ifdef X64_STRESS_TEST

#define X64_STRESS_TEST_ALLOC_COUNT 1000

LPVOID X64StressTestPointers[X64_STRESS_TEST_ALLOC_COUNT];

void X64StressTestAlloc()
{
    // v teto chvili jiz loader nacetl EXE a RTL a v inicializaci RTL doslo k vytvoreni a alokaci heapu,
    // ktery lezi na adresach pod 4GB; abychom dalsi alokace vystrcili nahoru nad 4GB, muysime zabrat spodek
    // virtualni pameti a pak nasledne donutit RTL pomoci alokaci k rozsireni jeho heapu
    //
    // zabereme prostor ve virtualni pameti
    UINT64 vaAllocated = 0;
    _int64 allocSize[] = {10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0};
    for (int i = 0; allocSize[i] != 0; i++)
        while (VirtualAlloc(0, allocSize[i], MEM_RESERVE, PAGE_NOACCESS) <= (LPVOID)(UINT_PTR)0x00000000ffffffff) // pri pristupu chceme exception a nechceme MEM_COMMIT, at zbytecne nezerem
            vaAllocated += allocSize[i];

    // nyni nafoukneme RTL heap
    UINT64 rtlAllocated = 0;
    _int64 rtlAllocSize[] = {10000000, 1000000, 100000, 10000, 1000, 100, 10, 1, 0};
    for (int i = 0; rtlAllocSize[i] != 0; i++)
        while (_malloc_dbg(rtlAllocSize[i], _CRT_BLOCK, __FILE__, __LINE__) <= (LPVOID)(UINT_PTR)0x00000000ffffffff)
            rtlAllocated += rtlAllocSize[i];

    // kontrola uspechu
    void* testNew = new char; // new jede pres alloc, ale radeji take overime
    if (testNew <= (LPVOID)(UINT_PTR)0x00000000ffffffff)
        MessageBox(NULL, "new address <= 0x00000000ffffffff!\nPlease contact jan.rysavy@altap.cz with this information.", "X64_STRESS_TEST", MB_OK | MB_ICONEXCLAMATION);
    delete testNew;
}

#endif //X64_STRESS_TEST
// nas vlastni vstupni bod, o ktery jsme si pozadali linker pomoci pragmy
int MyEntryPoint()
{
#ifdef X64_STRESS_TEST
    // alokacema vyzereme spodni 4GB pameti, aby dalsi alokace mely ukazatele vetsi nez DWORD
    X64StressTestAlloc();
#endif //X64_STRESS_TEST

    int ret = 1; // error

    // spustime Salmon, chceme aby pochytal maximum nasich padu
    if (SalmonInit())
    {
        // zavolame puvodni entry point aplikace a spustime tim program
        ret = WinMainCRTStartup();
    }
    else
        MessageBox(NULL, "Open Salamander Bug Reporter (salmon.exe) initialization has failed. Please reinstall Open Salamander.",
                   SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONSTOP);

    // sem uz mi debugger nechodi, sestreli nas v RTL (testovano pod VC 2008 s nasim RTL)

    // koncime
    return ret;
}

BOOL SalamanderBusy = TRUE;       // je Salamander busy?
DWORD LastSalamanderIdleTime = 0; // GetTickCount() z okamziku, kdy SalamanderBusy naposledy presel na TRUE

int PasteLinkIsRunning = 0; // pokud je vetsi nez nula, probiha prave Past Shortcuts prikaz v jednom z panelu

BOOL CannotCloseSalMainWnd = FALSE; // TRUE = nesmi dojit k zavreni hlavniho okna

DWORD MainThreadID = -1;

int MenuNewExceptionHasOccured = 0;
int FGIExceptionHasOccured = 0;
int ICExceptionHasOccured = 0;
int QCMExceptionHasOccured = 0;
int OCUExceptionHasOccured = 0;
int GTDExceptionHasOccured = 0;
int SHLExceptionHasOccured = 0;
int RelExceptionHasOccured = 0;

char DecimalSeparator[5] = "."; // "znaky" (max. 4 znaky) vytazene ze systemu
int DecimalSeparatorLen = 1;    // delka ve znacich bez nuly na konci
char ThousandsSeparator[5] = " ";
int ThousandsSeparatorLen = 1;

BOOL WindowsXP64AndLater = FALSE;  // JRYFIXME - zrusit
BOOL WindowsVistaAndLater = FALSE; // JRYFIXME - zrusit
BOOL Windows7AndLater = FALSE;     // JRYFIXME - zrusit
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

// pro zajisteni uniku z odstranenych drivu na fixed drive (po vysunuti device - USB flash disk, atd.)
BOOL ChangeLeftPanelToFixedWhenIdleInProgress = FALSE; // TRUE = prave se meni cesta, nastaveni ChangeLeftPanelToFixedWhenIdle na TRUE je zbytecne
BOOL ChangeLeftPanelToFixedWhenIdle = FALSE;
BOOL ChangeRightPanelToFixedWhenIdleInProgress = FALSE; // TRUE = prave se meni cesta, nastaveni ChangeRightPanelToFixedWhenIdle na TRUE je zbytecne
BOOL ChangeRightPanelToFixedWhenIdle = FALSE;
BOOL OpenCfgToChangeIfPathIsInaccessibleGoTo = FALSE; // TRUE = v idle otevre konfiguraci na Drives a focusne "If path in panel is inaccessible, go to:"

char IsSLGIncomplete[ISSLGINCOMPLETE_SIZE]; // pokud je retezec prazdny, SLG je kompletne prelozene; jinak obsahuje URL na forum do sekce daneho jazyka

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

CAssociations Associations; // asociace nactene z registry
CShares Shares;

char DefaultDir['Z' - 'A' + 1][MAX_PATH];

HACCEL AccelTable1 = NULL;
HACCEL AccelTable2 = NULL;

HINSTANCE NtDLL = NULL;             // handle k ntdll.dll
HINSTANCE Shell32DLL = NULL;        // handle k shell32.dll (ikonky)
HINSTANCE ImageResDLL = NULL;       // handle k imageres.dll (ikonky - Vista)
HINSTANCE User32DLL = NULL;         // handle k user32.dll (DisableProcessWindowsGhosting)
HINSTANCE HLanguage = NULL;         // handle k jazykove zavislym resourcum (.SPL souboru)
char CurrentHelpDir[MAX_PATH] = ""; // po prvnim pouziti helpu je zde cesta do adresare helpu (umisteni vsech .chm souboru)
WORD LanguageID = 0;                // language-id .SPL souboru

char OpenReadmeInNotepad[MAX_PATH]; // pouziva se jen pri spusteni z instalaku: jmeno souboru, ktere mame v IDLE otevrit v notepadu (spustit notepad)

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

HPEN HActiveNormalPen = NULL; // pera pro ramecek kolem polozky
HPEN HActiveSelectedPen = NULL;
HPEN HInactiveNormalPen = NULL;
HPEN HInactiveSelectedPen = NULL;

HPEN HThumbnailNormalPen = NULL; // pera pro ramecek kolem thumbnail
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

int SystemDPI = 0; // Globalni DPI pres vsechny monitory. Salamander nepodporuje Per-Monitor DPI, viz https://msdn.microsoft.com/library/windows/desktop/dn469266.aspx
int IconSizes[] = {16, 32, 48};
int IconLRFlags = 0;
HICON HSharedOverlays[] = {0};
HICON HShortcutOverlays[] = {0};
HICON HSlowFileOverlays[] = {0};
CIconList* SimpleIconLists[] = {0};
CIconList* ThrobberFrames = NULL;
CIconList* LockFrames = NULL; // pro jednoduchost deklaruji a nacitam jako throbber

HICON HGroupIcon = NULL;
HICON HFavoritIcon = NULL;
HICON HSlowFileIcon = NULL;

RGBQUAD ColorTable[256] = {0};

DWORD MouseHoverTime = 0;

SYSTEMTIME SalamanderStartSystemTime = {0}; // cas startu Salamandera (GetSystemTime)

BOOL WaitForESCReleaseBeforeTestingESC = FALSE; // ma se cekat na pusteni ESC pred zacatkem listovani cesty v panelu?

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

// mutex pro synchronizaci load/save do Registry (dva procesy najednou nemuzou, ma to neblahe vysledky)
CLoadSaveToRegistryMutex LoadSaveToRegistryMutex;

BOOL IsNotAlphaNorNum[256]; // pole TRUE/FALSE pro znaky (TRUE = neni pismeno ani cislice)
BOOL IsAlpha[256];          // pole TRUE/FALSE pro znaky (TRUE = pismeno)

// defaultni useruv charset pro fonty; pod W2K+ uz by stacilo DEFAULT_CHARSET
//
// Pod WinXP lze v regionalnim nastaveni zvolit jako default napriklad cestinu,
// ale na zalozce Advanced nenainstalovat ceske fotny. Potom pri konstrukci
// fontu s kodovanim UserCharset operacni system vrati font s uplne
// jinym nazvem (face name), hlavne aby mel pozadovane kodovani. Proto je DULEZITE pri
// specifikaci parametru fontu spravne zvolit promennou lfPitchAndFamily,
// kde si lze volit mezi FF_SWISS a FF_ROMAN fonty (bezpatkove/patkove).
int UserCharset = DEFAULT_CHARSET;

DWORD AllocationGranularity = 1; // granularita alokaci (potreba pro pouzivani souboru mapovanych do pameti)

#ifdef USE_BETA_EXPIRATION_DATE

// urcuje prvni den, kdy uz tato beta/PB verze nepobezi
// beta/PB verze 4.0 beta 1 pojede pouze do 1. unora 2020
//                                 YEAR  MONTH DAY
SYSTEMTIME BETA_EXPIRATION_DATE = {2020, 2, 0, 1, 0, 0, 0, 0};
#endif // USE_BETA_EXPIRATION_DATE

//******************************************************************************
//
// Rizeni Idle processingu (CMainWindow::OnEnterIdle)
//

BOOL IdleRefreshStates = TRUE;  // na uvod nechame nastavit promenne
BOOL IdleForceRefresh = FALSE;  // vyradi cache Enabler*
BOOL IdleCheckClipboard = TRUE; // koukneme taky na clipboard

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
        // barvy pera pro ramecek kolem polozky
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_NORMAL
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_SELECTED
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_NORMAL
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_SELECTED

        // barvy textu polozek v panelu
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_NORMAL
        RGBF(255, 0, 0, 0),         // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 0, 0, 0),         // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // barvy pozadi polozek v panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_SELECTED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCUSED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT),       // ITEM_BK_HIGHLIGHT

        // barvy pro blend ikonek
        RGBF(255, 128, 128, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),           // ICON_BLEND_FOCUSED
        RGBF(255, 0, 0, 0),               // ICON_BLEND_FOCSEL

        // barvy progress bary
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // barvy hot polozek
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // barvy titulku panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // barvy pera pro ramecek kolem thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 0, 0, 0),       // THUMBNAIL_FRAME_FOCUSED
        RGBF(255, 0, 0, 0),     // THUMBNAIL_FRAME_SELECTED
        RGBF(128, 0, 0, 0),     // THUMBNAIL_FRAME_FOCSEL
};

COLORREF ExplorerColors[NUMBER_OF_COLORS] =
    {
        // barvy pera pro ramecek kolem polozky
        RGBF(0, 0, 0, SCF_DEFAULT),       // FOCUS_ACTIVE_NORMAL
        RGBF(255, 255, 0, 0),             // FOCUS_ACTIVE_SELECTED
        RGBF(128, 128, 128, 0),           // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),               // FOCUS_FG_INACTIVE_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // FOCUS_BK_INACTIVE_NORMAL
        RGBF(255, 255, 0, 0),             // FOCUS_BK_INACTIVE_SELECTED

        // barvy textu polozek v panelu
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_NORMAL
        RGBF(255, 255, 255, 0),     // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 255, 0),     // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // barvy pozadi polozek v panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ITEM_BK_NORMAL
        RGBF(0, 0, 128, 0),               // ITEM_BK_SELECTED
        RGBF(232, 232, 232, 0),           // ITEM_BK_FOCUSED
        RGBF(0, 0, 128, 0),               // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT),       // ITEM_BK_HIGHLIGHT

        // barvy pro blend ikonek
        RGBF(0, 0, 128, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),       // ICON_BLEND_FOCUSED
        RGBF(0, 0, 128, 0),           // ICON_BLEND_FOCSEL

        // barvy progress bary
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // barvy hot polozek
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // barvy titulku panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // barvy pera pro ramecek kolem thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_FOCUSED
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_SELECTED
        RGBF(0, 0, 128, 0),     // THUMBNAIL_FRAME_FOCSEL
};

COLORREF NortonColors[NUMBER_OF_COLORS] =
    {
        // barvy pera pro ramecek kolem polozky
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_SELECTED
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_SELECTED
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_SELECTED

        // barvy textu polozek v panelu
        RGBF(0, 255, 255, 0),       // ITEM_FG_NORMAL
        RGBF(255, 255, 0, 0),       // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 0, 0),       // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // barvy pozadi polozek v panelu
        RGBF(0, 0, 128, 0),         // ITEM_BK_NORMAL
        RGBF(0, 0, 128, 0),         // ITEM_BK_SELECTED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCUSED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_BK_HIGHLIGHT

        // barvy pro blend ikonek
        RGBF(255, 255, 0, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),         // ICON_BLEND_FOCUSED
        RGBF(255, 255, 0, 0),           // ICON_BLEND_FOCSEL

        // barvy progress bary
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // barvy hot polozek
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(128, 128, 128, SCF_DEFAULT), // HOT_INACTIVE

        // barvy titulku panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // barvy pera pro ramecek kolem thumbnails
        RGBF(192, 192, 192, 0), // THUMBNAIL_FRAME_NORMAL
        RGBF(0, 128, 128, 0),   // THUMBNAIL_FRAME_FOCUSED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_SELECTED
        RGBF(255, 255, 0, 0),   // THUMBNAIL_FRAME_FOCSEL
};

COLORREF NavigatorColors[NUMBER_OF_COLORS] =
    {
        // barvy pera pro ramecek kolem polozky
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_ACTIVE_SELECTED
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_NORMAL
        RGBF(0, 128, 128, 0), // FOCUS_FG_INACTIVE_SELECTED
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_NORMAL
        RGBF(0, 0, 128, 0),   // FOCUS_BK_INACTIVE_SELECTED

        // barvy textu polozek v panelu
        RGBF(255, 255, 255, 0),     // ITEM_FG_NORMAL
        RGBF(255, 255, 0, 0),       // ITEM_FG_SELECTED
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_FOCUSED
        RGBF(255, 255, 0, 0),       // ITEM_FG_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_FG_HIGHLIGHT

        // barvy pozadi polozek v panelu
        RGBF(80, 80, 80, 0),        // ITEM_BK_NORMAL
        RGBF(80, 80, 80, 0),        // ITEM_BK_SELECTED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCUSED
        RGBF(0, 128, 128, 0),       // ITEM_BK_FOCSEL
        RGBF(0, 0, 0, SCF_DEFAULT), // ITEM_BK_HIGHLIGHT

        // barvy pro blend ikonek
        RGBF(255, 255, 0, SCF_DEFAULT), // ICON_BLEND_SELECTED
        RGBF(128, 128, 128, 0),         // ICON_BLEND_FOCUSED
        RGBF(255, 255, 0, 0),           // ICON_BLEND_FOCSEL

        // barvy progress bary
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_FG_NORMAL
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_FG_SELECTED
        RGBF(255, 255, 255, SCF_DEFAULT), // PROGRESS_BK_NORMAL
        RGBF(0, 0, 192, SCF_DEFAULT),     // PROGRESS_BK_SELECTED

        // barvy hot polozek
        RGBF(0, 0, 255, SCF_DEFAULT),     // HOT_PANEL
        RGBF(173, 182, 205, SCF_DEFAULT), // HOT_ACTIVE
        RGBF(212, 212, 212, SCF_DEFAULT), // HOT_INACTIVE

        // barvy titulku panelu
        RGBF(255, 255, 255, SCF_DEFAULT), // ACTIVE_CAPTION_FG
        RGBF(0, 0, 128, SCF_DEFAULT),     // ACTIVE_CAPTION_BK
        RGBF(255, 255, 255, SCF_DEFAULT), // INACTIVE_CAPTION_FG
        RGBF(128, 128, 128, SCF_DEFAULT), // INACTIVE_CAPTION_BK

        // barvy pera pro ramecek kolem thumbnails
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
  // generate crc polonomial, using precomputed poly should be faster
  // terms of polynomial defining this crc (except x^32):
  static const Byte p[] = {0,1,2,4,5,7,8,10,11,12,16,22,23,26};

  // make exclusive-or pattern from polynomial (0xedb88320L)
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

    // Honza: meril jsem nasledujici optimalizace a nemaji zadny vyznam;
    // jedina sance by bylo prepsani do ASM a cteni pameti po DWORDech,
    // ze kterych by pak bylo mozne vyzobavat jednotlive bajty;
    // pri soucasnem nastaveni release verze neni prekladac schopen tuto
    // optimalizaci provest za nas.
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
    // k SalamanderBusy a k LastSalamanderIdleTime se chodi bez kritickych sekci, nevadi,
    // protoze jsou to DWORDy a tudiz nemuzou byt "rozpracovane" pri switchnuti kontextu
    // (vzdy je tam stara nebo nova hodnota, nic jineho nehrozi)
    if (lastIdleTime != NULL)
        *lastIdleTime = LastSalamanderIdleTime;
    if (!SalamanderBusy)
        return TRUE;
    DWORD oldLastIdleTime = LastSalamanderIdleTime;
    if (GetTickCount() - oldLastIdleTime <= 100)                                   // pokud neni SalamanderBusy uz prilis dlouho (napr. otevreny modalni dialog)
        Sleep(100);                                                                // pockame jestli se SalamanderBusy nezmeni
    return !SalamanderBusy || (int)(LastSalamanderIdleTime - oldLastIdleTime) > 0; // neni "busy" nebo aspon osciluje
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
        DecimalSeparator[DecimalSeparatorLen] = 0; // posychrujeme nulu na konci
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
        ThousandsSeparator[ThousandsSeparatorLen] = 0; // posychrujeme nulu na konci
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
        //Presli jsme na vlastni implementaci (mensi pametova narocnost, fungujici XOR ikonky)
        //shi.hIcon = NULL;
        //SHGetFileInfo(path, 0, &shi, sizeof(shi),
        //              SHGFI_ICON | SHGFI_SHELLICONSIZE | (large ? 0 : SHGFI_SMALLICON));
        // pridame handle na 'shi.hIcon' do HANDLES
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
    case DRIVE_REMOVABLE: // ikonky 3.5, 5.25
    {
        HICON i = GetFileOrPathIconAux(root, large, TRUE);
        if (i != NULL)
            return i;
        id = 28; // 3 1/2 " mechanika
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

    // JRYFIXME - prozkoumat jestli neni IconLRFlags na zruseni? (W7+)

    /* JRYFIXME - grepnout plosne zdrojaky na LoadImage / IMAGE_ICON
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
    int l1 = (int)strlen(path); // je vzdy na stacku ...
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
                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
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
            return TRUE; // stejny root normal ("c:\path") cesty
        else
        {
            if (path1[0] == '\\' && path1[1] == '\\') // oboji UNC
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
                        break; // ruzne masiny
                }
                if (*s1 != 0 && *s1++ == *s2++) // preskok '\\'
                {
                    while (*s1 != 0 && *s1 != '\\')
                    {
                        if (LowerCase[*s1] == LowerCase[*s2])
                        {
                            s1++;
                            s2++;
                        }
                        else
                            break; // ruzne disky
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
        if (!IsUNCPath(root) && GetDriveType(root) == DRIVE_FIXED) // reparse pointy ma smysl hledat jen na fixed discich
        {
            // pokud nejde o root cestu, zkusime jeste traverzovat po reparse pointech
            BOOL cutPathIsPossible = TRUE;
            char p1NetPath[MAX_PATH];
            p1NetPath[0] = 0;
            ResolveLocalPathWithReparsePoints(ourPath, p1, &cutPathIsPossible, NULL, NULL, NULL, NULL, p1NetPath);

            if (p1NetPath[0] == 0) // ze sitove cesty volume ziskat nelze, nebudeme se ani snazit
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p1Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    {
                        strcpy(p1Volume, "fail"); // ani root nevratil uspech, neocekavane (bohuzel se deje na substenych discich pod W2K - ladeno u Bachaalany - pri selhani na obou cestach vracime SHODU, protoze je pravdepodobnejsi)
                        break;
                    }
                    SalPathAddBackslash(ourPath, MAX_PATH);
                }
            }

            // pokud jsme pod W2K a nejde o root cestu, zkusime jeste traverzovat po reparse pointech
            cutPathIsPossible = TRUE;
            char p2NetPath[MAX_PATH];
            p2NetPath[0] = 0;
            ResolveLocalPathWithReparsePoints(ourPath, p2, &cutPathIsPossible, NULL, NULL, NULL, NULL, p2NetPath);

            if ((p1NetPath[0] == 0) != (p2NetPath[0] == 0) || // pokud je jen jedna z cest sitova nebo
                p1NetPath[0] != 0 && !HasTheSameRootPath(p1NetPath, p2NetPath))
                ret = FALSE; // nemaji stejny root, ohlasime ruzne volumy (na sitovych cestach nelze overit volumy)

            if (p2NetPath[0] == 0 && ret) // ze sitove cesty volume ziskat nelze, nebudeme se ani snazit + pokud uz je rozhodnuto, tez se nebudeme snazit
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p2Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    {
                        strcpy(p2Volume, "fail"); // ani root nevratil uspech, neocekavane (bohuzel se deje na substenych discich pod W2K - ladeno u Bachaalany - pri selhani na obou cestach vracime SHODU, protoze je pravdepodobnejsi)
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
    if (!IsUNCPath(path1) && !IsUNCPath(path2)) // svazky na UNC cestach nema smysl resit
    {
        char p1Volume[100] = "1";
        char p2Volume[100] = "2";
        UINT drvType1 = GetDriveType(root1);
        UINT drvType2 = GetDriveType(root2);
        if (drvType1 != DRIVE_REMOTE && drvType2 != DRIVE_REMOTE) // krome site je sance zjistit "volume name"
        {
            BOOL cutPathIsPossible = TRUE;
            path1NetPath[0] = 0;         // sitova cesta, na kterou vede aktualni (posledni) lokalni symlink v ceste
            if (drvType1 == DRIVE_FIXED) // reparse pointy ma smysl hledat jen na fixed discich
            {
                // pokud jsme pod W2K a nejde o root cestu, zkusime jeste traverzovat po reparse pointech
                ResolveLocalPathWithReparsePoints(ourPath, path1, &cutPathIsPossible, NULL, NULL, NULL, NULL, path1NetPath);
            }
            else
                lstrcpyn(ourPath, root1, MAX_PATH);
            int numOfGetVolNamesFailed = 0;
            if (path1NetPath[0] == 0) // ze sitove cesty "volume name" ziskat nelze, nebudeme se ani snazit
            {
                while (!GetVolumeNameForVolumeMountPoint(ourPath, p1Volume, 100))
                {
                    if (!cutPathIsPossible || !CutDirectory(ourPath))
                    { // ani root nevratil uspech, neocekavane (bohuzel se deje na substenych discich pod W2K - ladeno u Bachaalany - pri selhani na obou cestach se stejnymi rooty vracime SHODU, protoze je pravdepodobnejsi)
                        numOfGetVolNamesFailed++;
                        break;
                    }
                    SalPathAddBackslash(ourPath, MAX_PATH);
                }
            }

            cutPathIsPossible = TRUE;
            path2NetPath[0] = 0;         // sitova cesta, na kterou vede aktualni (posledni) lokalni symlink v ceste
            if (drvType2 == DRIVE_FIXED) // reparse pointy ma smysl hledat jen na fixed discich
            {
                // pokud jsme pod W2K a nejde o root cestu, zkusime jeste traverzovat po reparse pointech
                ResolveLocalPathWithReparsePoints(ourPath, path2, &cutPathIsPossible, NULL, NULL, NULL, NULL, path2NetPath);
            }
            else
                lstrcpyn(ourPath, root2, MAX_PATH);
            if (path2NetPath[0] == 0) // ze sitove cesty "volume name" ziskat nelze, nebudeme se ani snazit
            {
                if (path1NetPath[0] == 0)
                {
                    while (!GetVolumeNameForVolumeMountPoint(ourPath, p2Volume, 100))
                    {
                        if (!cutPathIsPossible || !CutDirectory(ourPath))
                        { // ani root nevratil uspech, neocekavane (bohuzel se deje na substenych discich pod W2K - ladeno u Bachaalany - pri selhani na obou cestach se stejnymi rooty vracime SHODU, protoze je pravdepodobnejsi)
                            numOfGetVolNamesFailed++;
                            break;
                        }
                        SalPathAddBackslash(ourPath, MAX_PATH);
                    }
                    if (numOfGetVolNamesFailed != 2)
                    {
                        if (numOfGetVolNamesFailed == 0 && resIsOnlyEstimation != NULL)
                            *resIsOnlyEstimation = FALSE; // jediny pripad, kdy jsme si jisty vysledkem je, kdyz se podarilo ziskat "volume name" z obou cest (zaroven tak nemohly byt sitove)
                        if (numOfGetVolNamesFailed == 1 || strcmp(p1Volume, p2Volume) != 0)
                            ret = FALSE; // povedl se ziskat jen jeden "volume name", takze nejde o stejne svazky (a pokud ano, nejsme schopny to zjistit - mozna pokud slo o selhani kvuli SUBSTu, dalo by se to resit resolvnutim cilove cesty ze SUBSTu)
                        trySimpleTest = FALSE;
                    }
                }
                else // sitova je jen jedna cesta, nejde o stejne svazky (a pokud ano, nejsme schopny to zjistit)
                {
                    ret = FALSE;
                    trySimpleTest = FALSE;
                }
            }
            else
            {
                if (path1NetPath[0] != 0) // srovname rooty sitovych cest
                {
                    GetRootPath(root1, path1NetPath);
                    GetRootPath(root2, path2NetPath);
                }
                else // sitova je jen jedna cesta, nejde o stejne svazky (a pokud ano, nejsme schopny to zjistit)
                {
                    ret = FALSE;
                    trySimpleTest = FALSE;
                }
            }
        }
    }

    if (trySimpleTest) // zkusime jen jestli se shoduji root-cesty (sitove cesty + vse na NT)
    {
        ret = _stricmp(root1, root2) == 0;

        if (resIsOnlyEstimation != NULL)
        {
            lstrcpyn(path1NetPath, path1, MAX_PATH);
            lstrcpyn(path2NetPath, path2, MAX_PATH);
            if (ResolveSubsts(path1NetPath) && ResolveSubsts(path2NetPath))
            {
                if (IsTheSamePath(path1NetPath, path2NetPath))
                    *resIsOnlyEstimation = FALSE; // stejne cesty = urcite i stejne svazky
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
        lastBackslash = s1; // tento terminator nebude v lastBackslash
        backslashCount++;
    }

    if (path1[1] == ':')
    {
        // klasicka cesta
        if (path1[2] != '\\')
            return 0;

        // osetrim specialni pripad: u root cesty musime vratit delku i s posledni zpetnym lomitkem
        if (lastBackslash - path1 < 3)
            return 3;

        return (int)(lastBackslash - path1);
    }
    else
    {
        // UNC cesta
        if (path1[0] != '\\' || path1[1] != '\\')
            return 0;
        if (backslashCount < 4) // cesta musi mit tvar "\\masina\share"
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

    // CommonPrefixLength nam vratila delku bez posledniho zpetneho lomitka (pokud neslo o root path)
    // pokud nas prefix ma koncove zpetne lomitko, musim ho zahodit
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
           err == ERROR_INVALID_NAME ||   // je-li hacek v ceste na anglickych Windows, hlasi se tato chyba misto ERROR_PATH_NOT_FOUND
           err == ERROR_INVALID_FUNCTION; // hlasilo jednomu chlapikovi na WinXP na sitovem disku Y: v okamziku, kdy Salam pristupoval na cestu, ktera jiz neexistovala (nedoslo tak ke zkraceni a chlapik byl dobre v riti ;-) Shift+F7 na Y:\ to vyresila)
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
                *(path + --l) = 0; // zruseni '\\' na konci
            memmove(lastBackslash + 2, lastBackslash + 1, l - (lastBackslash - path));
            *cutDir = lastBackslash + 2; // "somedir" or "seconddir"
        }
        *(lastBackslash + 1) = 0; // "c:\"
    }
    else // "c:\firstdir\seconddir" or "c:\firstdir\seconddir\"
    {    // UNC: "\\server\share\path"
        if (path[0] == '\\' && path[1] == '\\' && nextBackslash <= path + 2)
        { // "\\server\share" - neda se zkratit
            if (cutDir != NULL)
                *cutDir = path + l;
            return FALSE;
        }
        *lastBackslash = 0;
        if (cutDir != NULL) // odriznuti '\' na konci
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
{                                           // POZOR: netypicke pouziti z GetShellFolder(): pro "\\\\" vraci "\\\\\\", pro "\\\\server" vraci "\\\\server\\"
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
            len = MAX_PATH - 2; // aby se to veslo i s '\\' do MAX_PATH bufferu (ocekavana velikost), orez neva, 100% je to beztak chyba
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

// projede vsechny barvy z konfigurace a pokud maji nastavenu default hodnotu,
// nastavi jim prislusne barevne hodnoty

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

COLORREF GetFullRowHighlight(COLORREF bkHighlightColor) // vraci "heuristicky" highlight pro full row mode
{
    // trochu heuristiky: zsvetle pozadi budeme "trochu" ztmavovat a tmave pozadi "trochu" zesvetlovat
    WORD h, l, s;
    ColorRGBToHLS(bkHighlightColor, &h, &l, &s);

    if (l < 121) // [TMAVA]  0-120 -> zesvetlime Luminance progresivne 0..120 -> +40..+20
        l += 20 + 20 * (120 - l) / 120;
    else // [SVETLA] 121-240 -> ztmavime Luminance o konstatnich 20
        l -= 20;

    return ColorHLSToRGB(h, l, s);
}

void UpdateDefaultColors(SALCOLOR* colors, CHighlightMasks* highlightMasks, BOOL processColors, BOOL processMasks)
{
    if (processColors)
    {
        int bitsPerPixel = GetCurrentBPP();

        // barvy pera pro ramecek kolem polozky prebereme ze systemove barvy textu okna
        if (GetFValue(colors[FOCUS_ACTIVE_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_ACTIVE_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[FOCUS_ACTIVE_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_ACTIVE_SELECTED], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[FOCUS_BK_INACTIVE_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_BK_INACTIVE_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[FOCUS_BK_INACTIVE_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[FOCUS_BK_INACTIVE_SELECTED], GetSysColor(COLOR_WINDOW));

        // texty polozek v panelu prebereme ze systemove barvy textu okna
        if (GetFValue(colors[ITEM_FG_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[ITEM_FG_FOCUSED]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_FG_FOCUSED], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[ITEM_FG_HIGHLIGHT]) & SCF_DEFAULT) // FULL ROW HIGHLIGHT vychazi z _NORMAL
            SetRGBPart(&colors[ITEM_FG_HIGHLIGHT], GetCOLORREF(colors[ITEM_FG_NORMAL]));

        // pozadi polozek v panelu prebereme ze systemove barvy pozadi okna
        if (GetFValue(colors[ITEM_BK_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_BK_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[ITEM_BK_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[ITEM_BK_SELECTED], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[ITEM_BK_HIGHLIGHT]) & SCF_DEFAULT) // HIGHLIGHT kopirujeme z NORMAL (aby fungovaly i custom/norton mody)
            SetRGBPart(&colors[ITEM_BK_HIGHLIGHT], GetFullRowHighlight(GetCOLORREF(colors[ITEM_BK_NORMAL])));

        // barvy progress bary
        if (GetFValue(colors[PROGRESS_FG_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_FG_NORMAL], GetSysColor(COLOR_WINDOWTEXT));
        if (GetFValue(colors[PROGRESS_FG_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_FG_SELECTED], GetSysColor(COLOR_HIGHLIGHTTEXT));
        if (GetFValue(colors[PROGRESS_BK_NORMAL]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_BK_NORMAL], GetSysColor(COLOR_WINDOW));
        if (GetFValue(colors[PROGRESS_BK_SELECTED]) & SCF_DEFAULT)
            SetRGBPart(&colors[PROGRESS_BK_SELECTED], GetSysColor(COLOR_HIGHLIGHT));

        // barva selected odstinu ikonky
        if (GetFValue(colors[ICON_BLEND_SELECTED]) & SCF_DEFAULT)
        {
            // normalne kopirujeme do selected barvu z focused+selected
            SetRGBPart(&colors[ICON_BLEND_SELECTED], GetCOLORREF(colors[ICON_BLEND_FOCSEL]));
            // pokud jde o cervenou (salamandrovskej profil a muzeme si to diky barevne hloubce
            // dovolit) pouzijeme pro selected svetlejsi odstin
            if (bitsPerPixel > 8 && GetCOLORREF(colors[ICON_BLEND_FOCSEL]) == RGB(255, 0, 0))
                SetRGBPart(&colors[ICON_BLEND_SELECTED], RGB(255, 128, 128));
        }

#define COLOR_HOTLIGHT 26 // winuser.h

        // titulky panelu (aktivni/neaktivni)

        // aktivni titulek panelu: POZADI
        if (GetFValue(colors[ACTIVE_CAPTION_BK]) & SCF_DEFAULT)
            SetRGBPart(&colors[ACTIVE_CAPTION_BK], GetSysColor(COLOR_ACTIVECAPTION));
        // aktivni titulek panelu: TEXT
        if (GetFValue(colors[ACTIVE_CAPTION_FG]) & SCF_DEFAULT)
            SetRGBPart(&colors[ACTIVE_CAPTION_FG], GetSysColor(COLOR_CAPTIONTEXT));
        // neaktivni titulek panelu: POZADI
        if (GetFValue(colors[INACTIVE_CAPTION_BK]) & SCF_DEFAULT)
            SetRGBPart(&colors[INACTIVE_CAPTION_BK], GetSysColor(COLOR_INACTIVECAPTION));
        // neaktivni titulek panelu: TEXT
        if (GetFValue(colors[INACTIVE_CAPTION_FG]) & SCF_DEFAULT)
        {
            // preferujeme stejnou barvu textu jako pro aktivni titulek, ale nekdy je tato barva priliz
            // blizka barve pozadi, potom zkusime barvu pro textu pro neaktivni titulek
            COLORREF clrBk = GetCOLORREF(colors[INACTIVE_CAPTION_BK]);
            COLORREF clrFgAc = GetSysColor(COLOR_CAPTIONTEXT);
            COLORREF clrFgIn = GetSysColor(COLOR_INACTIVECAPTIONTEXT);
            BYTE grayBk = GetGrayscaleFromRGB(GetRValue(clrBk), GetGValue(clrBk), GetBValue(clrBk));
            BYTE grayFgAc = GetGrayscaleFromRGB(GetRValue(clrFgAc), GetGValue(clrFgAc), GetBValue(clrFgAc));
            BYTE grayFgIn = GetGrayscaleFromRGB(GetRValue(clrFgIn), GetGValue(clrFgIn), GetBValue(clrFgIn));
            SetRGBPart(&colors[INACTIVE_CAPTION_FG], (abs(grayFgAc - grayBk) >= abs(grayFgIn - grayBk)) ? clrFgAc : clrFgIn);
        }

        // barvy hot polozek
        COLORREF hotColor = GetSysColor(COLOR_HOTLIGHT);
        if (GetFValue(colors[HOT_PANEL]) & SCF_DEFAULT)
            SetRGBPart(&colors[HOT_PANEL], hotColor);

        // hilight pro active panel caption
        if (GetFValue(colors[HOT_ACTIVE]) & SCF_DEFAULT)
        {
            COLORREF clr = GetCOLORREF(colors[ACTIVE_CAPTION_FG]);
            if (bitsPerPixel > 4)
                clr = GetHilightColor(clr, GetCOLORREF(colors[ACTIVE_CAPTION_BK]));
            SetRGBPart(&colors[HOT_ACTIVE], clr);
        }
        // hilight pro inactive panel caption
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
        // barvy zavisle na jmenu+atributech souboru
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
            if (GetFValue(item->HighlightBk) & SCF_DEFAULT) // FULL ROW HIGHLIGHT vychazi z _NORMAL
                SetRGBPart(&item->HighlightBk, GetFullRowHighlight(GetCOLORREF(item->NormalBk)));
        }
    }
}

//****************************************************************************
//
// Na zaklade barevne hloubky displeje urci, jestli pouzivat 256 barevne
// nebo 16 barevne bitmapy.
//

BOOL Use256ColorsBitmap()
{
    int bitsPerPixel = GetCurrentBPP();
    return (bitsPerPixel > 8); // vice nez 256 barev
}

DWORD GetImageListColorFlags()
{
    // pokud ma image list 16bitu barevnou hloubku, zlobi alfa kanal novych ikonek pod WinXP 32-bit barevny display (32-bitova hloubka slape)
    // pokud ma image list 32bitu barevnou hloubku, zlobi blend pri kresleni selectene polozky pod Win2K 32-bit barevny display (16-bitova hloubka slape)
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
    // zajistime si hladky graficky vystup
    // 20 volani GDI API by melo bohate stacit
    // je to implicitni hodnota z NT 4.0 WS
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

    // inicializace LogFont struktury
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

    // tyto brushe jsou alokovane systemem a automaticky se meni pri zmene barev
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
    ItemBitmap.CreateBmp(NULL, 1, 1); // zajisteni existence bitmapy

    // bitmapu nacitame pouze jednou (neoprasujeme ji pri zmene rozliseni)
    // a pokud by user prepnul barvy z 256 vejs, pri LoadBitmap (tedy bitmape
    // kompatibilni s display DC) by bitmapa zustala v degradovanych barvach;
    // proto ji nacteme jako DIB
    // HWorkerBitmap = HANDLES(LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_WORKER)));
    //HWorkerBitmap = (HBITMAP)HANDLES(LoadImage(HInstance, MAKEINTRESOURCE(IDB_WORKER), IMAGE_BITMAP, 0, 0, LR_CREATEDIBSECTION));
    //if (HWorkerBitmap == NULL)
    //  return FALSE;

    // pri zmene fontu se volaji explicitne CreatePanelFont a CreateEnvFont, prvni inicializaci provedeme zde
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

// pomoci TweakUI si mohou uzivatele menit ikonku shortcuty (default, custom, zadna)
// pokusime se ji ctit
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
  //#include <CommonControls.h>

  // cteni ikon overlayu ze systemoveho image-listu, zbytecne pomale, nacteme je primo z imageres.dll
  // tenhle kod tu nechavam jen pro pripad, ze bysme zase potrebovali zjistit kde ty ikony jsou
  typedef DECLSPEC_IMPORT HRESULT (WINAPI *F__SHGetImageList)(int iImageList, REFIID riid, void **ppvObj);

  F__SHGetImageList MySHGetImageList = (F__SHGetImageList)GetProcAddress(Shell32DLL, "SHGetImageList"); // Min: XP
  if (MySHGetImageList != NULL)
  {
    int shareIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SHARE);
    int linkIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_LINK);
    int offlineIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SLOWFILE);
    shareIndex = SHGetIconOverlayIndex(NULL, IDO_SHGIOI_SHARE);

    IImageList *imageList;
    if (MySHGetImageList(1 /* SHIL_SMALL * /, IID_IImageList, (void **)&imageList) == S_OK &&
        imageList != NULL)
    {
      int i;
      imageList->GetOverlayImage(linkIndex, &i);
      HICON icon;
      if (imageList->GetIcon(i, 0, &icon) != S_OK)
        icon = NULL;
      HShortcutOverlays[ICONSIZE_16] = icon;
      imageList->Release();
    }
    if (MySHGetImageList(0 /* SHIL_LARGE * /, IID_IImageList, (void **)&imageList) == S_OK &&
        imageList != NULL)
    {
      int i;
      imageList->GetOverlayImage(linkIndex, &i);
      HICON icon;
      if (imageList->GetIcon(i, 0, &icon) != S_OK)
        icon = NULL;
      HShortcutOverlays[ICONSIZE_32] = icon;
      imageList->Release();
    }
    if (MySHGetImageList(2 /* SHIL_EXTRALARGE * /, IID_IImageList, (void **)&imageList) == S_OK &&
        imageList != NULL)
    {
      int i;
      imageList->GetOverlayImage(linkIndex, &i);
      HICON icon;
      if (imageList->GetIcon(i, 0, &icon) != S_OK)
        icon = NULL;
      HShortcutOverlays[ICONSIZE_48] = icon;
      imageList->Release();
    }
  }
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
            char* num = strrchr(buff, ','); // cislo ikony je za posledni carkou
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

    int baseIconSize[ICONSIZE_COUNT] = {16, 32, 48}; // musi odpovidat CIconSizeEnum

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
    // 48x48 az od XP
    // ve skutecnosti jsou velke ikonky podporeny uz davno, lze je nahodit
    // Desktop/Properties/???/Large Icons; pozor, nebude pak existovat system image list
    // pro ikonky 32x32; navic bychom meli ze systemu vytahnout realne velikosti ikonek
    // zatim na to kasleme a 48x48 povolime az od XP, kde jsou bezne dostupne

    //
    // Vytahneme z Registry pozadovanou barevnou hloubku ikonek
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
        // dalsi zajimave hodnoty: "Shell Icon Size", "Shell Small Icon Size"
        char buff[100];
        if (GetValueAux(NULL, hKey, "Shell Icon Bpp", REG_SZ, buff, 100))
        {
            iconColorsCount = atoi(buff);
        }
        else
        {
            if (WindowsVistaAndLater)
            {
                // ve viste tento klic proste neni a zatim netusim, cim je nahrazen,
                // takze se budeme tvarit, ze ikonky jedou v plnych barvach (jinak jsme zobrazovali 16 barevne hnusy)
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
        if (Shell32DLL == NULL) // to se snad vubec nemuze stat (zaklad win 4.0)
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

        // prekladac hlasil chybu: error C2712: Cannot use __try in functions that require object unwinding
        // obchazim to vlozenim alokace do funkce
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
        ImageList_SetImageCount(HFindSymbolsImageList, 2); // inicializace
                                                           //    ImageList_SetBkColor(HFindSymbolsImageList, GetSysColor(COLOR_WINDOW)); // aby pod XP chodily pruhledne ikonky

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

        // vytahnu z shell 32 ikony:
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
            // ikonka adresare
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
            if (hIcon != NULL) // pokud ikonku neziskame, je tam porad jeste 4-rka z shell32.dll
            {
                SimpleIconLists[sizeIndex]->ReplaceIcon(symbolsDirectory, hIcon);
                NOHANDLES(DestroyIcon(hIcon));
            }

            // ikonka ".."
            hIcon = (HICON)HANDLES(LoadImage(HInstance, MAKEINTRESOURCE(IDI_UPPERDIR),
                                             IMAGE_ICON, IconSizes[sizeIndex], IconSizes[sizeIndex],
                                             IconLRFlags));
            SimpleIconLists[sizeIndex]->ReplaceIcon(symbolsUpDir, hIcon);
            HANDLES(DestroyIcon(hIcon));

            // ikonka archiv
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

    clrMap[0].from = RGB(128, 128, 128); // seda -> COLOR_BTNSHADOW
    clrMap[0].to = GetSysColor(COLOR_BTNSHADOW);
    clrMap[1].from = RGB(0, 0, 0); // cerna -> COLOR_BTNTEXT
    clrMap[1].to = GetSysColor(COLOR_BTNTEXT);
    clrMap[2].from = RGB(255, 255, 255); // bila -> pruhledna
    clrMap[2].to = RGB(255, 0, 255);
    HBITMAP hBottomTB = HANDLES(CreateMappedBitmap(HInstance, IDB_BOTTOMTOOLBAR, 0, clrMap, 3));
    BOOL remapWhite = FALSE;
    if (GetCurrentBPP() > 8)
    {
        clrMap[2].from = RGB(255, 255, 255); // bila -> svetle sedivou (at to tak nerve)
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
// GetCmdLine - ziskani parametru z prikazove radky
//
// buf + size - buffer pro parametry
// argv - pole ukazatelu, ktere se naplni parametry
// argCount - na vstupu je to pocet prvku v argv, na vystupu obsahuje pocet parametru
// cmdLine - parametry prikazove radky (bez jmena .exe souboru - z WinMain)

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
        if (*s == '"') // pocatecni '"'
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
            return c < end; // chyba jen pokud je maly buffer

        while (1)
        {
            if (*s == term || *s == 0)
            {
                if (*s == 0 || term != '"' || *++s != '"') // neni-li to nahrada "" -> "
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
    //load the DLL
    hComCtl = HANDLES(LoadLibrary(TEXT("comctl32.dll")));
    if (hComCtl)
    {
        HRESULT hr = S_OK;
        DLLGETVERSIONPROC pDllGetVersion;
        /*
     You must get this function explicitly because earlier versions of the DLL
     don't implement this function. That makes the lack of implementation of the
     function a version marker in itself.
    */
        pDllGetVersion = (DLLGETVERSIONPROC)GetProcAddress(hComCtl, TEXT("DllGetVersion")); // nema header
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
    // POZOR! fonts musi byt FALSE, aby nedoslo k zmene handlu fontu, o ktere
    // se museji dozvedet toolbary, ktere jej pouzivaji
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

    // dame vedet findum o zmene barev
    FindDialogQueue.BroadcastMessage(WM_USER_COLORCHANGEFIND, 0, 0);

    // rozesleme tuto novinku i mezi plug-iny
    Plugins.Event(PLUGINEVENT_COLORSCHANGED, 0);

    if (MainWindow != NULL && MainWindow->HTopRebar != NULL)
        SendMessage(MainWindow->HTopRebar, RB_SETBKCOLOR, 0, (LPARAM)GetSysColor(COLOR_BTNFACE));

    if (refresh && MainWindow != NULL)
    {
        InvalidateRect(MainWindow->HWindow, NULL, TRUE);
    }
    // Internal Viewer a Find:  obnova vsech oken
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

void TurnOFFWindowGhosting() // kdyz se "ghosting" nevypne, schovavaji se safe-wait-okenka po peti sekundach "not responding" stavu aplikace (kdyz aplikace nezpracovava zpravy)
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
// custom reporting funkce opsana z MSDN - http://msdn.microsoft.com/en-us/library/cb00sk7k(v=VS.90).aspx
#pragma runtime_checks("", off)
int MyRTCErrorFunc(int errType, const wchar_t* file, int line,
                   const wchar_t* module, const wchar_t* format, ...)
{
    // Prevent re-entrance.
    static long running = 0;
    while (InterlockedExchange(&running, 1))
        Sleep(0);
    // Now, disable all RTC failures.
    int numErrors = _RTC_NumErrors();
    int* errors = (int*)_alloca(numErrors);
    for (int i = 0; i < numErrors; i++)
        errors[i] = _RTC_SetErrorType((_RTC_ErrorNumber)i, _RTC_ERRTYPE_IGNORE);

    // First, get the rtc error number from the var-arg list.
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

    // radeji to tady zalomime s exception, bude snad jasnejsi callstack - pokud by nebyl, muzeme tady tu exception odstranit
    // viz popis chovani _CrtDbgReportW - http://msdn.microsoft.com/en-us/library/8hyw4sy7(v=VS.90).aspx
    RaiseException(OPENSAL_EXCEPTION_RTC, 0, 0, NULL); // nase vlastni "rtc" exception

    // sem uz se nedostaneme, proces byl ukoncen; pokracuji jen z formalnich duvodu, kdybychom neco menili

    // Now, restore the RTC errortypes.
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

DWORD LastCrtCheckMemoryTime; // kdy jsme posledne kontrolovali pamet v IDLE

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
        /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
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

    GetSystemDirectory(buf, MAX_PATH); // dame mu systemovy adresar, at neblokuje mazani soucasneho pracovniho adresare
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
    // Pokud bezime pod XP nebo nasledujicim OS, hrozi ze snazivy uzivatel zapnul
    // Compatibility Mode. Pokud tomu tak je, zobrazime varovani.
    // POZOR: Application Verifier nastavuje verzi Windows na novejsi nez skutecne je,
    // dela to pri testovani aplikace pri ziskavani "Windows 7 Software Logo".
    WORD kernel32major, kernel32minor;
    if (GetModuleVersion(GetModuleHandle("kernel32.dll"), &kernel32major, &kernel32minor))
    {
        TRACE_I("kernel32.dll: " << kernel32major << ":" << kernel32minor);
        // musime zavolat GetVersionEx, protoze vraci hodnoty podle nastaveneho Compatibility Mode
        // (SalIsWindowsVersionOrGreater nastaveny Compatibility Mode ignoruje)
        OSVERSIONINFO os;
        os.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

        // jen se vyhybame deprecated warningu, GetVersionEx snad bude vzdy a vsude
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

        // aktualni verze Salamandera je manifestovana pro Windows 10
        const DWORD SAL_MANIFESTED_FOR_MAJOR = 10;
        const DWORD SAL_MANIFESTED_FOR_MINOR = 0;

        // GetVersionEx nikdy nevrati vic nez os.dwMajorVersion == SAL_MANIFESTED_FOR_MAJOR
        // a os.dwMinorVersion == SAL_MANIFESTED_FOR_MINOR, je-li tedy kernel32.dll vyssi
        // verze, nejsme schopni 100% detekovat Compatibility Mode, je potreba mafinestovat
        // Salamandera pro nove Windows a posunout konstanty SAL_MANIFESTED_FOR_MAJOR a
        // SAL_MANIFESTED_FOR_MINOR, detekujeme aspon nastaveni Compatibility Mode na
        // starsi Windows, nez pro ktera je Salamander manifestovan
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
        BOOL ret = ExpandHotPath(NULL, argv, target, targetSize, FALSE); // pokud neni syntax cesty OK, vyskoci TRACE_E, coz nas netrapi
        if (!ret)
        {
            TRACE_E("ExpandHotPath failed.");
            // pokud expanze selze, pouzijeme retezec bez expanze
            lstrcpyn(target, argv, targetSize);
        }
    }
    else
    {
        DWORD auxRes = ExpandEnvironmentStrings(argv, target, targetSize); // uzivatele si prali moznost predavat jako parametr env promenne
        if (auxRes == 0 || auxRes > targetSize)
        {
            TRACE_E("ExpandEnvironmentStrings failed.");
            // pokud expanze selze, pouzijeme retezec bez expanze
            lstrcpyn(target, argv, targetSize);
        }
    }
    if (!IsPluginFSPath(target) && GetCurrentDirectory(MAX_PATH, curDir))
    {
        SalGetFullName(target, NULL, curDir, NULL, NULL, targetSize);
    }
}

// pokud jsou parametry OK, vraci TRUE, jinak vraci FALSE
BOOL ParseCommandLineParameters(LPSTR cmdLine, CCommandLineParams* cmdLineParams)
{
    // nechceme menit cesty, menit ikonu, menit prefix -- vse je potreba vynulovat
    ZeroMemory(cmdLineParams, sizeof(CCommandLineParams));

    char buf[4096];
    char* argv[20];
    int p = 20; // pocet prvku pole argv

    char curDir[MAX_PATH];
    GetModuleFileName(HInstance, ConfigurationName, MAX_PATH);
    *(strrchr(ConfigurationName, '\\') + 1) = 0;
    const char* configReg = "config.reg";
    strcat(ConfigurationName, configReg);
    if (!FileExists(ConfigurationName) && GetOurPathInRoamingAPPDATA(curDir) &&
        SalPathAppend(curDir, configReg, MAX_PATH) && FileExists(curDir))
    { // pokud neexistuje soubor config.reg u .exe, hledame ho jeste v APPDATA
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

            if (StrICmp(argv[i], "-aj") == 0) // active panel path (hot paths syntax for jumplist) - interni, nedokumentovane
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
                        *s != 0 && *(s + 1) == ':')       // "c:\" full path
                    {                                     // plne jmeno
                        lstrcpyn(ConfigurationName, argv[i + 1], MAX_PATH);
                    }
                    else // relativni jmeno
                    {
                        GetModuleFileName(HInstance, ConfigurationName, MAX_PATH);
                        *(strrchr(ConfigurationName, '\\') + 1) = 0;
                        SalPathAppend(ConfigurationName, s, MAX_PATH);
                        if (!FileExists(ConfigurationName) && GetOurPathInRoamingAPPDATA(curDir) &&
                            SalPathAppend(curDir, s, MAX_PATH) && FileExists(curDir))
                        { // pokud neexistuje relativne zadany soubor za -C u .exe, hledame ho jeste v APPDATA
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

            if (StrICmp(argv[i], "-t") == 0) // title prefix
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

            if (StrICmp(argv[i], "-o") == 0) // tvarime se, jako by bylo nahozene OnlyOneIstance
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
            { // Vista+: after installation: installer (SFX7ZIP) executes Salamander and asks for execution of notepad with readme file
                lstrcpyn(OpenReadmeInNotepad, argv[i + 1], MAX_PATH);
                i++;
                continue;
            }

            return FALSE; // wrong parameters
        }
    }
    return TRUE;
}

int WinMainBody(HINSTANCE hInstance, HINSTANCE /*hPrevInstance*/, LPSTR cmdLine, int cmdShow)
{
    int myExitCode = 1;

    //--- nechci zadne kriticke chyby jako "no disk in drive A:"
    SetErrorMode(SetErrorMode(0) | SEM_FAILCRITICALERRORS);

    // seed generatoru nahodnych cisel
    srand((unsigned)time(NULL) ^ (unsigned)_getpid());

#ifdef _DEBUG
    // #define _CRTDBG_ALLOC_MEM_DF        0x01  /* Turn on debug allocation */ (DEFAULT ON)
    // #define _CRTDBG_DELAY_FREE_MEM_DF   0x02  /* Don't actually free memory */
    // #define _CRTDBG_CHECK_ALWAYS_DF     0x04  /* Check heap every alloc/dealloc */
    // #define _CRTDBG_RESERVED_DF         0x08  /* Reserved - do not use */
    // #define _CRTDBG_CHECK_CRT_DF        0x10  /* Leak check/diff CRT blocks */
    // #define _CRTDBG_LEAK_CHECK_DF       0x20  /* Leak check at program exit */

    // pri podezreni na prepis alokovane pameti lze odkomentovat nasledujici dva radky
    // dojde ke zpomaleni Salamandera a pri kazdem free alloc se provede test konzistence heapu
    // int crtDbg = _CrtSetDbgFlag(_CRTDBG_REPORT_FLAG);   // Get the current bits
    // _CrtSetDbgFlag(crtDbg | _CRTDBG_CHECK_ALWAYS_DF);
    // _CrtSetDbgFlag(crtDbg | _CRTDBG_LEAK_CHECK_DF);

    // dalsi zajimava funkce pro ladeni: pokud dojde k memory leaku, v zavorce je zobrazeno
    // dekadicke cislo, ktere udava poradi alokovaneho bloku, napriklad _CRT_WARN: {104200};
    // funkci _CrtSetBreakAlloc umoznuje breaknou na tomto bloku
    // _CrtSetBreakAlloc(33521);

    LastCrtCheckMemoryTime = GetTickCount();

    // na tomto pripade prepisu konce pameti zabere ochrana -- v IDLE se zobrazi messagebox
    // a do TraceServeru se nalejou debug hlasky
    //
//  char *p1 = (char*)malloc( 4 );
//  strcpy( p1 , "Oops" );
#endif //_DEBUG

    /*
   // test "Heap Block Corruptions: Full-page heap", viz http://support.microsoft.com/kb/286470
   // alokuje vsechny bloky (musi mit aspon 16 bytu) tak, ze za nimi je nepristupna stranka, takze
   // jakykoliv prepis konce bloku vede k exceptione
   // instalovat Debugging Tools for Windows, v gflags.exe pro "salamand.exe" vybrat "Enable page heap",
   // fungovalo mi to pod W2K i pod XP (pod Vistou by melo taky)
   // NEBO: pouzit pripravene sal-pageheap-register.reg a sal-pageheap-unregister.reg (to pak neni
   // potreba instalovat Debugging Tools for Windows)
   //
   // prosinec/2011: testoval jsem pod VS2008 + page heap + Win7x64 a nasledujici prepis nevyvolava exception
   // nasel jsem popis alokace v tomto rezimu: http://msdn.microsoft.com/en-us/library/ms220938(v=VS.90).aspx

  char *test = (char *)malloc(16);
//  char *test = (char *)HeapAlloc(GetProcessHeap(), 0, 16);
  char bufff[100];
  sprintf(bufff, "test=%p", test);
  MessageBox(NULL, bufff, "a", MB_OK);
  test[16] = 0;
*/

    char testCharValue = 129;
    int testChar = testCharValue;
    if (testChar != 129) // pokud bude testChar zaporne, mame problem: LowerCase[testCharValue] saha mimo pole...
    {
        MessageBox(NULL, "Default type 'char' is not 'unsigned char', but 'signed char'. See '/J' compiler switch in MSVC.",
                   "Compilation Error", MB_OK | MB_ICONSTOP);
    }

    MainThreadID = GetCurrentThreadId();
    HInstance = hInstance;
    CALL_STACK_MESSAGE4("WinMainBody(0x%p, , %s, %d)", hInstance, cmdLine, cmdShow);

    // Tak za tohle ja nemuzu ... co delat, kdyz to dela konkurence, musime
    // taky - inspirovano v Exploreru.
    // A ja se divil, ze jim tak pekne chodi paint.
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    SetTraceProcessName("Salamander");
    SetThreadNameInVCAndTrace("Main");
    SetMessagesTitle(MAINWINDOW_NAME);
    TRACE_I("Begin");

    // inicializace OLE
    if (FAILED(OleInitialize(NULL)))
    {
        TRACE_E("Error in CoInitialize.");
        return 1;
    }

    //  HOldWPHookProc = SetWindowsHookEx(WH_CALLWNDPROC,     // HANDLES neumi!
    //                                    WPMessageHookProc,
    //                                    NULL, GetCurrentThreadId());

    User32DLL = NOHANDLES(LoadLibrary("user32.dll"));
    if (User32DLL == NULL)
        TRACE_E("Unable to load library user32.dll."); // neni fatalni chyba

    TurnOFFWindowGhosting();

    NtDLL = HANDLES(LoadLibrary("NTDLL.DLL"));
    if (NtDLL == NULL)
        TRACE_E("Unable to load library ntdll.dll."); // neni fatalni chyba

    // detekce defaultniho userova charsetu pro fonty
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

    // kvuli pouzivani souboru mapovanych do pameti je nutne ziskat granularitu alokaci
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
    // Windows 10         10             0             (poznamka: preview verze W10 z 2014 vracely verzi 6.4)

    if (!SalIsWindowsVersionOrGreater(6, 1, 0))
    {
        // sem se to pravdepodobne nedostane, na starsich systemech budou chybet exporty staticky linkovanych
        // knihoven a uzivatele serve nejaka nepochopitelna hlaska na urovni PE loaderu ve Windows
        // nevolat SalMessageBox
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

    // pokud je to mozne, pouzijeme GetNativeSystemInfo, jinak si nechame vysledek GetSystemInfo
    typedef void(WINAPI * PGNSI)(LPSYSTEM_INFO);
    PGNSI pGNSI = (PGNSI)GetProcAddress(GetModuleHandle("kernel32.dll"), "GetNativeSystemInfo"); // Min: XP
    if (pGNSI != NULL)
        pGNSI(&si);
    Windows64Bit = si.wProcessorArchitecture == PROCESSOR_ARCHITECTURE_AMD64;

    if (!GetWindowsDirectory(WindowsDirectory, MAX_PATH))
        WindowsDirectory[0] = 0;

    // zajima nas iface ITaskbarList3, ktery MS zavedli od Windows 7 - napriklad progress v taskbar buttons
    if (Windows7AndLater)
    {
        TaskbarBtnCreatedMsg = RegisterWindowMessage("TaskbarButtonCreated");
        if (TaskbarBtnCreatedMsg == 0)
        {
            DWORD err = GetLastError();
            TRACE_E("RegisterWindowMessage() failed for 'TaskbarButtonCreated'. Error:" << err);
        }
    }

    // mame nastavene globalni promenne, muzeme inicializovat tento mutex
    if (!TaskList.Init())
        TRACE_E("TaskList.Init() failed!");

    if (!InitializeWinLib())
        goto EXIT_1; // musime inicializovat WinLib pred prvnim zobrazenim
                     // wait dialogu (musi byt registrovany tridy oken)
                     // ImportConfiguration uz muze otevrit tento dialog

    LoadSaveToRegistryMutex.Init();

    // zkusime z aktualni konfigurace vytahnout hodnotu "AutoImportConfig" -> existuje v pripade, ze provadime UPGRADE
    BOOL autoImportConfig = FALSE;
    char autoImportConfigFromKey[200];
    autoImportConfigFromKey[0] = 0;
    if (!GetUpgradeInfo(&autoImportConfig, autoImportConfigFromKey, 200)) // user si preje exit softu
    {
        myExitCode = 0;
    EXIT_1a:
        ReleaseWinLib();
        goto EXIT_1;
    }
    const char* configKey = autoImportConfig ? autoImportConfigFromKey : SalamanderConfigurationRoots[0];

    // zkusime z aktualni konfigurace vytahnout klic urcujici jazyk
    LoadSaveToRegistryMutex.Enter();
    HKEY hSalamander;
    DWORD langChanged = FALSE; // TRUE = startujeme Salama poprve s jinym jazykem (naloadime vsechny pluginy, at se overi ze mame tuto jazykovou verzi i pro ne, pripadne at user vyresi jake nahradni verze chce pouzivat)
    if (OpenKey(HKEY_CURRENT_USER, configKey, hSalamander))
    {
        HKEY actKey;
        DWORD configVersion = 1; // toto je konfig od 1.52 a starsi
        if (OpenKey(hSalamander, SALAMANDER_VERSION_REG, actKey))
        {
            configVersion = 2; // toto je konfig od 1.6b1
            GetValue(actKey, SALAMANDER_VERSIONREG_REG, REG_DWORD,
                     &configVersion, sizeof(DWORD));
            CloseKey(actKey);
        }
        if (configVersion >= 59 /* 2.53 beta 2 */ && // pred 2.53 beta 2 byla jen anglictina, tedy cteni nema smysl, nabidneme userovi defaultni jazyk systemu nebo rucni vyber jazyku
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

    // pokud klic neexistuje, zobrazime vyberovy dialog
    BOOL newSLGFile = FALSE; // TRUE pokud byl .SLG vybran pri tomto spusteni Salamandera
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
        if (!autoImportConfig &&                            // pri UPGRADE toto nema smysl (jazyk se cte o par radek vyse, tahle rutina by ho jen precetla znovu)
            FindLanguageFromPrevVerOfSal(prevVerSLGName) && // importneme jazyk z predchozi verze, je dost pravdepodobne, ze ho user opet chce pouzit (jde o import stare konfigurace Salama)
            slgDialog.SLGNameExists(prevVerSLGName))
        {
            lstrcpy(Configuration.SLGName, prevVerSLGName);
        }
        else
        {
            int langIndex = slgDialog.GetPreferredLanguageIndex(NULL, TRUE);
            if (langIndex == -1) // tato instalace neobsahuje jazyk souhlasici s aktualnim user-locale ve Windows
            {

// kdyz se tohle zakomentuje, nebudeme posilat lidi tahat jazykove verze z webu (napr. kdyz tam zadne nejsou)
// JRY: pro AS 2.53, kery jde s cestinou, nemcinou a anglictinou je pro ostatni jazyky posleme na forum do sekce
//      "Translations" https://forum.altap.cz/viewforum.php?f=23 - treba to nekoho namotivuje a pujde svuj preklad vytvorit
#define OFFER_OTHERLANGUAGE_VERSIONS

#ifndef OFFER_OTHERLANGUAGE_VERSIONS
                if (slgDialog.GetLanguagesCount() == 1)
                    slgDialog.GetSLGName(Configuration.SLGName); // pokud existuje jen jeden jazyk, pouzijeme ho
                else
                {
#endif // OFFER_OTHERLANGUAGE_VERSIONS

                    // otevreme dialog vyberu jazyku, aby mohl user downloadnout a nainstalovat dalsi jazyky
                    if (slgDialog.Execute() == IDCANCEL)
                        goto EXIT_1a;

#ifndef OFFER_OTHERLANGUAGE_VERSIONS
                }
#endif // OFFER_OTHERLANGUAGE_VERSIONS
            }
            else
            {
                slgDialog.GetSLGName(Configuration.SLGName, langIndex); // pokud existuje jazyk odpovidajici aktualnim user-locale ve Windows, pouzijeme ho
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
        if (!newSLGFile) // zapamatovany .SLG soubor prestal nejspis existovat, zkusime najit jiny
        {
            sprintf(errorText, "File %s was not found or is not valid language file.\nOpen Salamander "
                               "will try to search for some other language file (.SLG).",
                    path);
            MessageBox(NULL, errorText, SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
            Configuration.SLGName[0] = 0;
            goto FIND_NEW_SLG_FILE;
        }
        else // nemelo by vubec nastat - .SLG soubor jiz byl otestovan
        {
            sprintf(errorText, "File %s was not found or is not valid language file.\n"
                               "Please run Open Salamander again and try to choose some other language file.",
                    path);
            MessageBox(NULL, errorText, "Open Salamander", MB_OK | MB_ICONERROR);
            goto EXIT_1a;
        }
    }

    strcpy(Configuration.LoadedSLGName, Configuration.SLGName);

    // nechame jiz bezici salmon nacist zvolene SLG (zatim pouzival nejake provizorni)
    SalmonSetSLG(Configuration.SLGName);

    // nastavime lokalizovane hlasky do modulu ALLOCHAN (zajistuje pri nedostatku pameti hlaseni uzivateli + Retry button + kdyz vse selze tak i Cancel pro terminate softu)
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
    // beta verze je casove limitovana, viz BETA_EXPIRATION_DATE
    // pokud je dnes den urceny touto promennou nebo nejaky dalsi, zobrazime okenko a koncime
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

    // otevreme splash screen

    GetSystemDPI(NULL);

    // pokud konfigurace neexistuje nebo bude nasledne pri importu ze souboru zmenena, ma uzivatel
    // smulu a splash screen se bude ridit implicitni nebo starou hodnotou
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

    // okno pro import konfigurace obsahuje listview s checkboxama, musime inicializovat COMMON CONTROLS
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

    SetWinLibStrings(LoadStr(IDS_INVALIDNUMBER), MAINWINDOW_NAME); // j.r. - posunout na spravne misto

    // inicializace pakovacu; drive provadeno v konstruktorech; ted presunuto sem,
    // kdy uz je rozhodnuto o jazykovem DLL
    PackerFormatConfig.InitializeDefaultValues();
    ArchiverConfig.InitializeDefaultValues();
    PackerConfig.InitializeDefaultValues();
    UnpackerConfig.InitializeDefaultValues();

    // pokud soubor existuje, bude importovan do registry
    BOOL importCfgFromFileWasSkipped = FALSE;
    ImportConfiguration(NULL, ConfigurationName, ConfigurationNameIgnoreIfNotExists, autoImportConfig,
                        &importCfgFromFileWasSkipped);

    // obslouzime prechod ze stareho configu na novy

    // Zavolame funkci, ktera se pokusi najit konfiguraci odpovidajici nasi verzi programu.
    // Pokud se ji podari najit, bude nastavena promenna 'loadConfiguration' a funkce vrati
    // TRUE. Pokud konfigurace jeste nebude existovat, funkce postupne prohleda stare
    // konfigurace z pole 'SalamanderConfigurationRoots' (od nejmladsich k nejstrasim).
    // Pokud nalezne nekterou z konfiguraci, zobrazi dialog a nabidne jeji konverzi do
    // konfigurace soucasne a smazani z registry. Po zobrazeni posledniho dialogu vrati
    // TRUE a nastavi promenne 'deleteConfigurations' a 'loadConfiguration' dle voleb
    // uzivatele. Pokud uzivatel zvoli ukonceni aplikace, vrati funkce FALSE.

    // pole urcujici indexy konfiguraci v poli 'SalamanderConfigurationRoots',
    // ktere maji byt smazany (0 -> zadna)
    BOOL deleteConfigurations[SALCFG_ROOTS_COUNT];
    ZeroMemory(deleteConfigurations, sizeof(deleteConfigurations));

    CALL_STACK_MESSAGE1("WinMainBody::FindLatestConfiguration");

    // ukazatel do pole 'SalamanderConfigurationRoots' na konfiguraci, ktera ma byt
    // nactena (NULL -> zadna; pouziji se default hodnoty)
    if (autoImportConfig)
        SALAMANDER_ROOT_REG = autoImportConfigFromKey; // pri UPGRADE nema hledani konfigurace smysl
    else
    {
        if (!FindLatestConfiguration(deleteConfigurations, SALAMANDER_ROOT_REG))
        {
            SplashScreenCloseIfExist();
            goto EXIT_2;
        }
    }

    InitializeShellib(); // OLE je treba inicializovat pred otevrenim HTML helpu - CSalamanderEvaluation

    // pokud jeste neexistuje novy klic konfigurace, vytvorime ho pred pripadnym smazanim
    // starych klicu
    BOOL currentCfgDoesNotExist = autoImportConfig || SALAMANDER_ROOT_REG != SalamanderConfigurationRoots[0];
    BOOL saveNewConfig = currentCfgDoesNotExist;

    // pokud uzivatel nechce vic instanci, pouze aktivujeme predchozi
    if (!currentCfgDoesNotExist &&
        CheckOnlyOneInstance(&cmdLineParams))
    {
        SplashScreenCloseIfExist();
        myExitCode = 0;
    EXIT_3:
        ReleaseShellib();
        goto EXIT_2;
    }

    // overim verzi CommonControlu
    if (GetComCtlVersion(&CCVerMajor, &CCVerMinor) != S_OK) // JRYFIXME - testy kolem common controls posunout na W7+
    {
        CCVerMajor = 0; // tohle asi nikdy nenastane - nemaji comctl32.dll
        CCVerMinor = 0;
    }

    CALL_STACK_MESSAGE1("WinMainBody::StartupDialog");

    //  StartupDialog.Open(HLanguage);

    int i;
    for (i = 0; i < NUMBER_OF_COLORS; i++)
        UserColors[i] = SalamanderColors[i];

    //--- inicializacni cast
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

    // pripojeni OLE SPYe
    // posunuto pod InitializeGraphics, ktere pod WinXP vyhazovala leaky (asi zase neajake cache)
    // OleSpyRegister();    // odpojeno, protoze po updatu Windows 2000 z 02/2005 zacal pri spusteni+zavreni Salama z MSVC vylitavat debug-breakpoint: Invalid Address specified to RtlFreeHeap( 130000, 14bc74 ) - asi MS nekde zacali volat primo RtlFreeHeap misto OLE free a diky bloku informaci spye na zacatku alokovaneho bloku se to podelalo (malloc vraci pointer posunuty za blok informaci spye)
    //OleSpySetBreak(2754); // brakne na [n-te] alokaci z dumpu

    // inicializace workera (diskove operace)
    InitWorker();

    // inicializace knihovny pro komunikaci s SalShExt/SalamExt/SalExtX86/SalExtX64.DLL (shell copy hook + shell context menu)
    InitSalShLib();

    // inicializace knihovny pro praci s shell icon overlays (Tortoise SVN + CVS)
    LoadIconOvrlsInfo(SALAMANDER_ROOT_REG);
    InitShellIconOverlays();

    // inicializace funkci pro prochazeni pres next/prev soubor v panelu/Findu z vieweru
    InitFileNamesEnumForViewers();

    // nacteme seznam sharovanych adresaru
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

    Associations.ReadAssociations(FALSE); // nacteni asociaci z Registry

    // registrace shell extensions
    // pokud najdeme v podadresari "utils" knihovnu, overime jeji registraci a pripadne ji zaregistrujeme
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

    //--- vytvoreni hlavniho okna
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

                // vytahneme z registry Group Policy
                IfExistSetSplashScreenText(LoadStr(IDS_STARTUP_POLICY));
                SystemPolicies.LoadFromRegistry();

                CALL_STACK_MESSAGE1("WinMainBody::load_config");
                BOOL setActivePanelAndPanelPaths = FALSE; // aktivni panel + cesty v panelech se nastavuji v MainWindow->LoadConfig()
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
                    MainWindow->CreateAndInsertWorkerBand(); // na zaver vlozime workera
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
                    Plugins.ClearLastSLGNames(); // aby pripadne doslo k nove volbe nahradniho jazyka u vsech pluginu
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
                    if (!importCfgFromFileWasSkipped) // jen pokud hned nedojde k exitu softu (to pak nema smysl)
                        MainWindow->ApplyCommandLineParams(&cmdLineParams, setActivePanelAndPanelPaths);

                    if (Windows7AndLater)
                        CreateJumpList();

                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

                    AccelTable1 = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_MAINACCELS1)));
                    AccelTable2 = HANDLES(LoadAccelerators(HInstance, MAKEINTRESOURCE(IDA_MAINACCELS2)));

                    MainWindow->CanClose = TRUE; // ted teprve povolime zavreni hl. okna
                    // aby soubory nevyskakovali postupne (jak se nacitaji jejich ikony)
                    UpdateWindow(MainWindow->HWindow);

                    BOOL doNotDeleteImportedCfg = FALSE;
                    if (autoImportConfig && // zjistime jestli nova verze nema mene pluginu nez stara a diky tomu se cast stare konfigurace neprenese
                        FindPluginsWithoutImportedCfg(&doNotDeleteImportedCfg))
                    {                               // je potreba exit softu bez ulozeni konfigurace
                        SALAMANDER_ROOT_REG = NULL; // tohle by melo spolehlive zamezit zapisu do konfigurace v registry
                        PostMessage(MainWindow->HWindow, WM_USER_FORCECLOSE_MAINWND, 0, 0);
                    }
                    else
                    {
                        if (Configuration.ConfigVersion < THIS_CONFIG_VERSION
#ifndef _WIN64 // FIXME_X64_WINSCP
                            || Configuration.AddX86OnlyPlugins
#endif // _WIN64
                        )
                        {                                            // auto-install plug-inu ze standardniho plug-in-podadresare "plugins"
#ifndef _WIN64                                                       // FIXME_X64_WINSCP
                            Configuration.AddX86OnlyPlugins = FALSE; // jednou staci
#endif                                                               // _WIN64
                            Plugins.AutoInstallStdPluginsDir(MainWindow->HWindow);
                            Configuration.LastPluginVer = 0;   // pri prechodu na novou verzi bude zrusen soubor plugins.ver
                            Configuration.LastPluginVerOP = 0; // pri prechodu na novou verzi bude zrusen soubor plugins.ver i pro druhou platformu
                            saveNewConfig = TRUE;              // nova konfigurace se musi ulozit (aby se tohle pri pristim spusteni neopakovalo)
                        }
                        // nacteni souboru plugins.ver ((re)instalace plug-inu), nutne i poprve (pro pripad
                        // instalace plug-inu pred prvnim spustenim Salamandera)
                        if (Plugins.ReadPluginsVer(MainWindow->HWindow, Configuration.ConfigVersion < THIS_CONFIG_VERSION))
                            saveNewConfig = TRUE; // nova konfigurace se musi ulozit (aby se tohle pri pristim spusteni neopakovalo)
                        // load plug-inu, ktere maji nastaveny flag load-on-start
                        Plugins.HandleLoadOnStartFlag(MainWindow->HWindow);
                        // pokud startujeme poprve se zmenenym jazykem, naloadime vsechny pluginy, aby se ukazalo,
                        // jestli maji tuto jazykovou verzi + pripadne aby si user vybrat nahradni jazyky
                        if (langChanged)
                            Plugins.LoadAll(MainWindow->HWindow);

                        // pluginy FTP a WinSCP nove volaji SalamanderGeneral->SetPluginUsesPasswordManager() aby se prihlasily k odberu eventu z password managera
                        // zavedeno s verzi kofigurace 45 -- dame vsem pluginum moznost se prihlasit
                        if (Configuration.ConfigVersion < 45) // zavedeni password manageru
                            Plugins.LoadAll(MainWindow->HWindow);

                        // save uz pujde do nejnovejsiho klice
                        SALAMANDER_ROOT_REG = SalamanderConfigurationRoots[0];
                        // konfiguraci ulozime hned, dokud je to cista konverze stare verze -- user muze
                        // mit vypnuty "Save Cfg on Exit" a pokud behem chodu Salamandera neco zmeni, nechce to na zaver ulozit
                        if (saveNewConfig)
                        {
                            MainWindow->SaveConfig();
                        }
                        // prohleda pole a pokud je nektery z rootu oznaceny pro smazani, smaze ho + smaze starou konfiguraci
                        // po UPGRADE a tez smazne hodnotu "AutoImportConfig" v klici konfigurace teto verze Salama
                        MainWindow->DeleteOldConfigurations(deleteConfigurations, autoImportConfig, autoImportConfigFromKey,
                                                            doNotDeleteImportedCfg);

                        // jen prvni instance Salamandera: podivame se, jestli neni potreba vycistit
                        // TEMP od zbytecnych souboru disk-cache (pri padu nebo zamknuti jinou aplikaci
                        // muzou soubory v TEMPu zustat)
                        // musime testoval na globalni (skrz vsechny sessions) promennou, aby se videly dve
                        // instance Salamanderu spustene pod FastUserSwitching
                        // Problem nahlasen na foru: https://forum.altap.cz/viewtopic.php?t=2643
                        if (FirstInstance_3_or_later)
                        {
                            DiskCache.ClearTEMPIfNeeded(MainWindow->HWindow, MainWindow->GetActivePanelHWND());
                        }

                        if (importCfgFromFileWasSkipped) // pokud jsme preskocili import config.reg nebo jineho .reg souboru (parametr -C)
                        {                                // informujeme usera o nutnosti noveho startu Salama a nechame ho exitnout soft
                            MSGBOXEX_PARAMS params;
                            memset(&params, 0, sizeof(params));
                            params.HParent = MainWindow->HWindow;
                            params.Flags = MB_OK | MB_ICONINFORMATION;
                            params.Caption = SALAMANDER_TEXT_VERSION;
                            params.Text = LoadStr(IDS_IMPORTCFGFROMFILESKIPPED);
                            char aliasBtnNames[200];
                            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
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
            // je-li treba, vyvolame zobrazeni dialogu Tip of the Day
            // 0xffffffff = open quiet - pokud to nedopadne, neserveme usera
            if (Configuration.ShowTipOfTheDay)
              PostMessage(MainWindow->HWindow, WM_COMMAND, CM_HELP_TIP, 0xffffffff);
  */
                    }

                    // odted se uz budou pamatovat zavirane cesty
                    MainWindow->CanAddToDirHistory = TRUE;

                    // uzivatele chteji mit start-up cestu v historii i v pripade, ze ji neuspinili
                    MainWindow->LeftPanel->UserWorkedOnThisPath = TRUE;
                    MainWindow->RightPanel->UserWorkedOnThisPath = TRUE;

                    // dame seznamu procesu vedet, ze bezime a mame hlavni okno (je mozne nas aktivovat pri OnlyOneInstance)
                    TaskList.SetProcessState(PROCESS_STATE_RUNNING, MainWindow->HWindow);

                    // pozadame Salmon o kontrolu, zda na disku nejsou stare bug reporty, ktere by bylo potreba odeslat
                    SalmonCheckBugs();

                    if (IsSLGIncomplete[0] != 0 && Configuration.ShowSLGIncomplete)
                        PostMessage(MainWindow->HWindow, WM_USER_SLGINCOMPLETE, 0, 0);

                    //--- aplikacni smycka
                    CALL_STACK_MESSAGE1("WinMainBody::message_loop");
                    DWORD activateParamsRequestUID = 0;
                    BOOL skipMenuBar;
                    MSG msg;
                    BOOL haveMSG = FALSE; // FALSE pokud se ma volat GetMessage() v podmince cyklu
                    while (haveMSG || GetMessage(&msg, NULL, 0, 0))
                    {
                        haveMSG = FALSE;
                        if (msg.message != WM_USER_SHOWWINDOW && msg.message != WM_USER_WAKEUP_FROM_IDLE && /*msg.message != WM_USER_SETPATHS &&*/
                            msg.message != WM_QUERYENDSESSION && msg.message != WM_USER_SALSHEXT_PASTE &&
                            msg.message != WM_USER_CLOSE_MAINWND && msg.message != WM_USER_FORCECLOSE_MAINWND)
                        { // krom "connect", "shutdown", "do-paste" a "close-main-wnd" messages jsou vsechny zacatkem BUSY rezimu
                            SalamanderBusy = TRUE;
                            LastSalamanderIdleTime = GetTickCount();
                        }

                        if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN) &&
                            msg.wParam != VK_MENU && msg.wParam != VK_CONTROL && msg.wParam != VK_SHIFT)
                        {
                            SetCurrentToolTip(NULL, 0); // zhasneme tooltip
                        }

                        skipMenuBar = FALSE;
                        if (Configuration.QuickSearchEnterAlt && msg.message == WM_SYSCHAR)
                            skipMenuBar = TRUE;

                        // zajistime zaslani zprav do naseho menu (obchazime tim potrebu hooku pro klavesnici)
                        if (MainWindow == NULL || MainWindow->MenuBar == NULL || !MainWindow->CaptionIsActive ||
                            MainWindow->QuickRenameWindowActive() ||
                            skipMenuBar || GetCapture() != NULL || // je-li captured mouse - mohli bychom zpusobit vizualni problemy
                            !MainWindow->MenuBar->IsMenuBarMessage(&msg))
                        {
                            CWindowsObject* wnd = WindowsManager.GetWindowPtr(GetActiveWindow());

                            // Bottom Toolbar - zmena textu podle VK_CTRL, VK_MENU a VK_SHIFT
                            if ((msg.message == WM_SYSKEYDOWN || msg.message == WM_KEYDOWN ||
                                 msg.message == WM_SYSKEYUP || msg.message == WM_KEYUP) &&
                                MainWindow != NULL)
                                MainWindow->UpdateBottomToolBar();

                            if ((wnd == NULL || !wnd->Is(otDialog) ||
                                 !IsDialogMessage(wnd->HWindow, &msg)) &&
                                (MainWindow == NULL || !MainWindow->CaptionIsActive || // pridano "!MainWindow->CaptionIsActive", aby se v nemodalnich oknech pluginu neprekladaly akceleratory (F7 v "FTP Logs" neni nic moc)
                                 MainWindow->QuickRenameWindowActive() ||
                                 !TranslateAccelerator(MainWindow->HWindow, AccelTable1, &msg) &&
                                     (MainWindow->EditMode || !TranslateAccelerator(MainWindow->HWindow, AccelTable2, &msg))))
                            {
                                TranslateMessage(&msg);
                                DispatchMessage(&msg);
                            }
                        }

                        if (MainWindow != NULL && MainWindow->CanClose)
                        { // je-li Salamander nastartovany, muzeme ho prohlasit za NE BUSY
                            SalamanderBusy = FALSE;
                        }

                    TEST_IDLE:
                        if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
                        {
                            if (msg.message == WM_QUIT)
                                break;      // ekvivalent situace, kdy GetMessage() vraci FALSE
                            haveMSG = TRUE; // mame zpravu, jdeme ji zpracovat (bez volani GetMessage())
                        }
                        else // pokud ve fronte neni zadna message, provedeme Idle processing
                        {
#ifdef _DEBUG
                            // jednou za tri vteriny osetrime konzistenci heapu
                            if (_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & _CRTDBG_ALLOC_MEM_DF)
                            {
                                if (GetTickCount() - LastCrtCheckMemoryTime > 3000) // kazde tri vteriny
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
                                CannotCloseSalMainWnd = TRUE; // musime zamezit zavreni hlavniho okna Salamandera behem provadeni nasledujicich rutin
                                MainWindow->OnEnterIdle();

                                // na pusteni ESC cekame jen pokud refresh listingu v panelu primo
                                // navazuje (coz pres IDLE nehrozi)
                                if (WaitForESCReleaseBeforeTestingESC)
                                    WaitForESCReleaseBeforeTestingESC = FALSE;

                                // zjistime, zda nas nezada cizi "OnlyOneInstance" Salamander o aktivaci a nastaveni cest v panelech?
                                // FControlThread by v takovem pripade nastavil parametry do globalni CommandLineParams a zvysil RequestUID
                                // pokud byl hlavni thread v IDLE, probral se diky postnute WM_USER_WAKEUP_FROM_IDLE
                                if (!SalamanderBusy && CommandLineParams.RequestUID > activateParamsRequestUID)
                                {
                                    CCommandLineParams paramsCopy;
                                    BOOL applyParams = FALSE;

                                    NOHANDLES(EnterCriticalSection(&CommandLineParamsCS));
                                    // tesne pred vstupem do kriticke sekce mohlo dojit k timeoutu v control threadu, overime ze jeste stoji o vysledek
                                    // zaroven overime, ze pozadavak neexpiroval (volajici vlakno ceka pouze do TASKLIST_TODO_TIMEOUT a potom cekani
                                    // vzda a spusti novou instanci Salamander; nechceme v takovem pripade pozadavek vyplnit)
                                    DWORD tickCount = GetTickCount();
                                    if (CommandLineParams.RequestUID != 0 && tickCount - CommandLineParams.RequestTimestamp < TASKLIST_TODO_TIMEOUT)
                                    {
                                        memcpy(&paramsCopy, &CommandLineParams, sizeof(CCommandLineParams));
                                        applyParams = TRUE;

                                        // ulozime UID, ktere jsme jiz odbavili, abychom necyklili
                                        activateParamsRequestUID = CommandLineParams.RequestUID;
                                        // dame control threadu zpravu, ze jsme cesty prijali
                                        SetEvent(CommandLineParamsProcessed);
                                    }
                                    NOHANDLES(LeaveCriticalSection(&CommandLineParamsCS));

                                    // uvolnili jsme sdilene prostredky, muzeme se jit parat s cestama
                                    if (applyParams && MainWindow != NULL)
                                    {
                                        SendMessage(MainWindow->HWindow, WM_USER_SHOWWINDOW, 0, 0);
                                        MainWindow->ApplyCommandLineParams(&paramsCopy);
                                    }
                                }

                                // zajistime unik z odstranenych drivu na fixed drive (po vysunuti device - USB flash disk, atd.)
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

                                // pokud nejaky plug-in chtel unload nebo rebuild menu, provedeme ho... (jen neni-li "busy")
                                if (!SalamanderBusy && ExecCmdsOrUnloadMarkedPlugins)
                                {
                                    int cmd;
                                    CPluginData* data;
                                    Plugins.GetCmdAndUnloadMarkedPlugins(MainWindow->HWindow, &cmd, &data);
                                    ExecCmdsOrUnloadMarkedPlugins = (cmd != -1);
                                    if (cmd >= 0 && cmd < 500) // spusteni prikazu Salamandera na zadost plug-inu
                                    {
                                        int wmCmd = GetWMCommandFromSalCmd(cmd);
                                        if (wmCmd != -1)
                                        {
                                            // vygenerujeme WM_COMMAND a nechame ho hned zpracovat
                                            msg.hwnd = MainWindow->HWindow;
                                            msg.message = WM_COMMAND;
                                            msg.wParam = (DWORD)LOWORD(wmCmd); // radsi orizneme horni WORD (0 - cmd z menu)
                                            msg.lParam = 0;
                                            msg.time = GetTickCount();
                                            GetCursorPos(&msg.pt);

                                            haveMSG = TRUE; // mame zpravu, jdeme ji zpracovat (bez volani GetMessage())
                                        }
                                    }
                                    else
                                    {
                                        if (cmd >= 500 && cmd < 1000500) // spusteni prikazu menuExt na zadost plug-inu
                                        {
                                            int id = cmd - 500;
                                            SalamanderBusy = TRUE; // jdeme provest prikaz menu - uz jsme zase "busy"
                                            LastSalamanderIdleTime = GetTickCount();
                                            if (data != NULL && data->GetLoaded())
                                            {
                                                if (data->GetPluginInterfaceForMenuExt()->NotEmpty())
                                                {
                                                    CALL_STACK_MESSAGE4("CPluginInterfaceForMenuExt::ExecuteMenuItem(, , %d,) (%s v. %s)",
                                                                        id, data->DLLName, data->Version);

                                                    // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
                                                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                                                    CSalamanderForOperations sm(MainWindow->GetActivePanel());
                                                    data->GetPluginInterfaceForMenuExt()->ExecuteMenuItem(&sm, MainWindow->HWindow, id, 0);

                                                    // opet zvysime prioritu threadu, operace dobehla
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
                                                // nemusi byt naloaden, staci aby se PostMenuExtCommand zavolal z Release pluginu,
                                                // ktery je vyvolany postnutym unloadem
                                                // TRACE_E("Unexpected situation during call of menu command in \"sal-idle\".");
                                            }
                                            if (MainWindow != NULL && MainWindow->CanClose) // konec provadeni prikazu menu
                                            {                                               // je-li Salamander nastartovany, muzeme ho prohlasit za NE BUSY
                                                SalamanderBusy = FALSE;
                                            }
                                            CannotCloseSalMainWnd = FALSE;
                                            goto TEST_IDLE; // zkusime znovu "idle" (napr. aby se mohl zpracovat dalsi postnuty prikaz/unload)
                                        }
                                    }
                                }
                                if (!SalamanderBusy && OpenPackOrUnpackDlgForMarkedPlugins)
                                {
                                    CPluginData* data;
                                    int pluginIndex;
                                    Plugins.OpenPackOrUnpackDlgForMarkedPlugins(&data, &pluginIndex);
                                    OpenPackOrUnpackDlgForMarkedPlugins = (data != NULL);
                                    if (data != NULL) // otevreni Pack/Unpack dialogu na zadost plug-inu
                                    {
                                        SalamanderBusy = TRUE; // jdeme provest prikaz menu - uz jsme zase "busy"
                                        LastSalamanderIdleTime = GetTickCount();
                                        if (data->OpenPackDlg)
                                        {
                                            CFilesWindow* activePanel = MainWindow->GetActivePanel();
                                            if (activePanel != NULL && activePanel->Is(ptDisk))
                                            { // otevreni Pack dialogu
                                                MainWindow->CancelPanelsUI();
                                                activePanel->UserWorkedOnThisPath = TRUE;
                                                activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
                                                { // otevreni Unpack dialogu
                                                    MainWindow->CancelPanelsUI();
                                                    activePanel->UserWorkedOnThisPath = TRUE;
                                                    activePanel->StoreSelection(); // ulozime selection pro prikaz Restore Selection
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
                                        if (MainWindow != NULL && MainWindow->CanClose) // konec otevirani Pack/Unpack dialogu
                                        {                                               // je-li Salamander nastartovany, muzeme ho prohlasit za NE BUSY
                                            SalamanderBusy = FALSE;
                                        }
                                        CannotCloseSalMainWnd = FALSE;
                                        goto TEST_IDLE; // zkusime znovu "idle" (napr. aby se mohl zpracovat dalsi postnuty prikaz/unload/Pack/Unpack)
                                    }
                                }
                                if (!SalamanderBusy && OpenReadmeInNotepad[0] != 0)
                                { // spustime notepad se souborem 'OpenReadmeInNotepad' pro instalak pod Vista+
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

    // pro pripad chyby zkusim zavrit dialog
    SplashScreenCloseIfExist();

    // vratime prioritu do puvodniho stavu
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    //--- vsem oknum dame 1 sekundu na to, aby se uzavreli, pak je nechame odpojit
    int timeOut = 10;
    int winsCount = WindowsManager.GetCount();
    while (timeOut-- && winsCount > 0)
    {
        Sleep(100);
        int c = WindowsManager.GetCount();
        if (winsCount > c) // zatim jeste ubyvaji okna, budeme cekat dale aspon 1 sekundu
        {
            winsCount = c;
            timeOut = 10;
        }
    }

//--- informace
#ifdef __DEBUG_WINLIB
    TRACE_I("WindowsManager: " << WindowsManager.maxWndCount << " windows, " << WindowsManager.search << " searches, " << WindowsManager.cache << " cached searches.");
#endif
    //---
    DestroySafeWaitWindow(TRUE); // povel "terminate" safe-wait-message threadu
    Sleep(1000);                 // nechame vsem threadum viewru cas, aby se ukoncili
    NBWNetAC3Thread.Close(TRUE); // bezici thread nechame zabit (presun do AuxThreads), dalsi akce zablokujeme
    TerminateAuxThreads();       // zbytek nasilne terminujeme
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

    // pro jistotu zavreme az jako posledni, ale asi zbytecne obavy
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
    // OleSpyRevoke();     // odpojime OLESPY
    OleUninitialize(); // deinicializace OLE
    // OleSpyDump();       // vypiseme leaky

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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
        return 1;
    }
#endif // CALLSTK_DISABLE
}
