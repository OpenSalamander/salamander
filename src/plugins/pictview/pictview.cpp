// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "lib/pvw32dll.h"
#include "pictview.h"
#include "dialogs.h"
#ifdef ENABLE_WIA
#include "wiawrap.h"
#endif // ENABLE_WIA
#ifdef ENABLE_TWAIN32
#include "twain/twain.h"
#include "pvtwain.h"
#endif // ENABLE_TWAIN32
#include "exif/exif.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang/lang.rh"
#include "histwnd.h"
#include "PVEXEWrapper.h"
#include "PixelAccess.h"

// plugin interface object; its methods are called from Salamander
CPluginInterface PluginInterface;
// part of the CPluginInterface for the viewer
CPluginInterfaceForViewer InterfaceForViewer;
CPluginInterfaceForMenuExt InterfaceForMenuExt;
CPluginInterfaceForThumbLoader InterfaceForThumbLoader;

LPCTSTR TIP_WINDOW_CLASSNAME = _T("PictViewToolTip Class");
LPCTSTR CLIPBOARD = _T("<<Clipboard>>");
LPCTSTR CAPTURE = _T("<<Capture>>");
LPCTSTR SCAN = _T("<<Scan>>");
#ifdef ENABLE_TWAIN32
LPCTSTR SCAN_SOURCE = _T("<<ScanSource>>");
#endif // ENABLE_TWAIN32
LPCTSTR SCANEXTRA = _T("<<ScanExtra>>");
LPCTSTR DELETED = _T("<<Deleted>>");

LPCTSTR PLUGIN_NAME_EN = _T("PICTVIEW"); // non-translated plugin name, used before loading the language module + for debug stuff

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources
HACCEL HAccel = NULL;

BOOL SalamanderRegistered = FALSE;

// ConfigVersion: 0 - default,
//                1 - released with Salamander 1.6 beta 3
//                2 - with Salamander 1.6 beta 4/5
//                3 - 1.04 beta 1
//                4 - 1.04 Beta 2
//                5 - 1.05 Beta 1 (with Salamander 1.6 beta 6)
//                6 - 1.06 Beta 1 (with Salamander 1.6 beta 7)
//                7 - 1.06 Beta 2
//                8 - with Salamander 2.5
//                9 - with Salamander 2.5 beta 2
//               10 - with Salamander 2.5 beta 3
//               11 - with Salamander 2.5 beta 4; MNG
//               12 - PSP* instead of PSP; DTX
//               13 - with Salamander 2.5 beta 5; DDS
//               14 - with Salamander 2.5 beta 7 (change of default configuration)
//               15 - added NEF format (currently support D70 and D100, hopefully people will send more)
//               16 - added CRW format
//               17 - Added EPS/EPT/AI fformat (Only encapsulated EPS w/ TIFF thumbnail)
//               18 - Added FUJI raw; QuickTime MJPEG PICT video from digital cameras
//               19 - Salamander 2.5 Beta 11: Added HPI format - Hemera Photo Objects
//               20 - Salamander 2.5 RC2: Added RAW Digital Camera images: *.ARW;*.CR2;*.DNG;*.PEF
//               21 - Salamander 2.5 RC3: Added Olympus *.ORF Digital Camera images
//               22 - Salamander 2.52 B1: Added *.BLP files used by Blizzard Entertainment in World of Wordcraft

int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 22

SGlobals G; // initialized in InitViewer
TDirectArray<DWORD> ExifHighlights(20, 10);
BOOL ExifGroupHighlights = FALSE;

LPCTSTR CONFIG_VERSION = _T("Version");
LPCTSTR CONFIG_SHRINK = _T("ShrinkToFit");
LPCTSTR CONFIG_PAGEDNUP = _T("PageUpDnScrolls");
LPCTSTR CONFIG_THUMBNAILS = _T("IgnoreThumbnails");
LPCTSTR CONFIG_THUMB_MAX_IMG_SIZE = _T("MaxThumbImgSize");
LPCTSTR CONFIG_AUTOROTATE = _T("AutoRotate");
LPCTSTR CONFIG_PIPETTE = _T("PipetteInHex");
LPCTSTR CONFIG_WINPOS = _T("WinPos");
LPCTSTR CONFIG_FILESHISTORY = _T("FilesHistory");
LPCTSTR CONFIG_DIRSHISTORY = _T("DirsHistory");
LPCTSTR CONFIG_COPYTO = _T("CopyTo");
LPCTSTR CONFIG_TOOLBARVISIBLE = _T("ToolbarVisible");
LPCTSTR CONFIG_STATUSBARVISIBLE = _T("StatusbarVisible");
LPCTSTR CONFIG_EXIFHIGHLIGHTS = _T("ExifHighlights");
LPCTSTR CONFIG_EXIFGROUPHIGHLIGHTS = _T("ExifGroupHighlights");
LPCTSTR CONFIG_EXIFDLGWIDTH = _T("ExifDlgWidth");
LPCTSTR CONFIG_EXIFDLGHEIGHT = _T("ExifDlgHeight");
LPCTSTR CONFIG_PATHINTITLE = _T("ShowPathInTitle");
LPCTSTR CONFIG_DONTSHOWANYMORE = _T("DontShowAnymore");

LPCTSTR CONFIG_RGBRENDBGCOLOR = _T("RendererBGColor");
LPCTSTR CONFIG_RGBRENDWSCOLOR = _T("RendererWSColor");
LPCTSTR CONFIG_RGBFSBGCOLOR = _T("FullScreenBGColor");
LPCTSTR CONFIG_RGBFSWSCOLOR = _T("FullScreenWSColor");

LPCTSTR CONFIG_SELECT_RATIO_X = _T("SelectRatioX");
LPCTSTR CONFIG_SELECT_RATIO_Y = _T("SelectRatioY");

LPCTSTR CONFIG_SAVE = _T("Save");
LPCTSTR CONFIG_SAVE_FLAGS = _T("Flags");
LPCTSTR CONFIG_SAVE_JPEG_QUALITY = _T("JPEG Quality");
LPCTSTR CONFIG_SAVE_JPEG_SUBSAMPLING = _T("JPEG Subsampling");
LPCTSTR CONFIG_SAVE_TIFF_STRIP_SIZE = _T("TIFF Strip Size");
LPCTSTR CONFIG_SAVE_INIT_DIR = _T("InitDir");
LPCTSTR CONFIG_SAVE_REMEMBER_PATH = _T("RememberPath");
LPCTSTR CONFIG_SAVE_FILTER_MONO = _T("FilterMono");
LPCTSTR CONFIG_SAVE_FILTER_COLOR = _T("FilterColor");

// predefined zoom values in hundreds of percent, terminated by the sentinel
int PredefinedZooms[] = {/*625, 1250*/ 600, 1200, 2500, 5000, 7500, 10000, 12500, 15000, 20000, 40000, 60000, 80000, 100000, 160000, -1};

CPVW32DLL PVW32DLL;

HINSTANCE EXIFLibrary = NULL;

LRESULT CALLBACK ToolTipWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

// general Salamander interface - valid from startup until the plugin is closed
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

// interface providing customized Windows controls used in Salamander
CSalamanderGUIAbstract* SalamanderGUI = NULL;

CWindowQueue ViewerWindowQueue("PictView Viewers"); // list of all viewer windows
CThreadQueue ThreadQueue("PictView Viewers");       // list of all window threads

CExtraScanImagesToOpen ExtraScanImagesToOpen; // list of all images from the scanner to open in windows

#define IDX_TB_ZOOMNUMBER -3
#define IDX_TB_TERMINATOR -2
#define IDX_TB_SEPARATOR -1
#define IDX_TB_PROPERTIES 0
#define IDX_TB_COPY 1
#define IDX_TB_PASTE 2
#define IDX_TB_FULLSCREEN 3
#define IDX_TB_OPEN 4
#define IDX_TB_180 5
#define IDX_TB_RIGHT 6
#define IDX_TB_LEFT 7
#define IDX_TB_FLIPV 8
#define IDX_TB_FLIPH 9
#define IDX_TB_ZOOMIN 10
#define IDX_TB_ZOOMOUT 11
#define IDX_TB_FIRST 12
#define IDX_TB_LAST 13
#define IDX_TB_SAVE 14
#define IDX_TB_PREVPAGE 15
#define IDX_TB_NEXTPAGE 16
#define IDX_TB_HELP 17
#define IDX_TB_HAND 18
#define IDX_TB_PICK 19
#define IDX_TB_SELECT 20
#define IDX_TB_ZOOM 21
#define IDX_TB_PRINT 22
#define IDX_TB_ZOOMACTUAL 23
#define IDX_TB_ZOOMWHOLE 24
#define IDX_TB_ZOOMWIDTH 25
#define IDX_TB_PREV 26
#define IDX_TB_NEXT 27
#define IDX_TB_OTHERCHANNELS 28 // used in the histogram
#define IDX_TB_LUMINOSITY 29    // used in the histogram
#define IDX_TB_RED 30           // used in the histogram
#define IDX_TB_GREEN 31         // used in the histogram
#define IDX_TB_BLUE 32          // used in the histogram
#define IDX_TB_RGBSUM 33        // used in the histogram
#define IDX_TB_SELSRCFILE 34
#define IDX_TB_CROP 35
#define IDX_TB_PREVSELFILE 36
#define IDX_TB_NEXTSELFILE 37
#define IDX_TB_COUNT 38

MENU_TEMPLATE_ITEM MenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        // Files
        {MNTT_PB, IDS_MENU_FILE, MNTS_B | MNTS_I | MNTS_A, CML_FILE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_OPEN, MNTS_B | MNTS_I | MNTS_A, CMD_OPEN, IDX_TB_OPEN, 0, (DWORD*)vweNotLoading},
        {MNTT_IT, IDS_MENU_FILE_REFRESH, MNTS_B | MNTS_I | MNTS_A, CMD_RELOAD, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_SAVEAS, MNTS_B | MNTS_I | MNTS_A, CMD_SAVEAS, IDX_TB_SAVE, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_CAPTURE, MNTS_B | MNTS_I | MNTS_A, CMD_CAPTURE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_SCAN, MNTS_B | MNTS_I | MNTS_A, CMD_SCAN, -1, 0, NULL},
#ifdef ENABLE_TWAIN32
        {MNTT_IT, IDS_MENU_FILE_SCAN_SOURCE, MNTS_B | MNTS_I | MNTS_A, CMD_SCAN_SOURCE, -1, 0, NULL},
#endif // ENABLE_TWAIN32
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_PB, IDS_MENU_FILE_WALLPAPER, MNTS_B | MNTS_I | MNTS_A, CML_WALLPAPER, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_WALLPARER_CENTER, MNTS_B | MNTS_I | MNTS_A, CMD_WALLPAPER_CENTER, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_WALLPARER_TILE, MNTS_B | MNTS_I | MNTS_A, CMD_WALLPAPER_TILE, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_WALLPARER_STRETCH, MNTS_B | MNTS_I | MNTS_A, CMD_WALLPAPER_STRETCH, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_WALLPARER_RESTORE, MNTS_B | MNTS_I | MNTS_A, CMD_WALLPAPER_RESTORE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_WALLPARER_NONE, MNTS_B | MNTS_I | MNTS_A, CMD_WALLPAPER_NONE, -1, 0, NULL},
        {MNTT_PE},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PROP, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_PROP, IDX_TB_PROPERTIES, 0, (DWORD*)vweImgInfoAvailable},
        {MNTT_IT, IDS_MENU_FILE_EXIF, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_EXIF, -1, 0, (DWORD*)vweImgExifAvailable},
        {MNTT_IT, IDS_MENU_FILE_HISTOGRAM, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_HISTOGRAM, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_FOCUS, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_FOCUS, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_SELCRCFILE, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_TOGGLESELECT, IDX_TB_SELSRCFILE, 0, (DWORD*)vweSelSrcFile},
        {MNTT_IT, IDS_MENU_FILE_RENAME, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_RENAME, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_COPYTO, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_COPYTO, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_DELETE, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_DELETE, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        //    {MNTT_IT,    IDS_MENU_FILE_PAGE,           MNTS_B|MNTS_I|MNTS_A, CMD_PAGE_SETUP,             -1,                      0,                 (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_FILE_PRINT, MNTS_B | MNTS_I | MNTS_A, CMD_PRINT, IDX_TB_PRINT, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_PB, IDS_MENU_FILE_OTHER, MNTS_B | MNTS_I | MNTS_A, CML_OTHERFILES, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREV, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_PREV, IDX_TB_PREV, 0, (DWORD*)vwePrevFile},
        {MNTT_IT, IDS_MENU_FILE_NEXT, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_NEXT, IDX_TB_NEXT, 0, (DWORD*)vweNextFile},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREVSEL, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_PREVSELFILE, IDX_TB_PREVSELFILE, 0, (DWORD*)vwePrevSelFile},
        {MNTT_IT, IDS_MENU_FILE_NEXTSEL, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_NEXTSELFILE, IDX_TB_NEXTSELFILE, 0, (DWORD*)vweNextSelFile},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_FIRST, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_FIRST, IDX_TB_FIRST, 0, (DWORD*)vweFirstFile},
        {MNTT_IT, IDS_MENU_FILE_LAST, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_LAST, IDX_TB_LAST, 0, (DWORD*)vweFirstFile},
        {MNTT_PE},
        {MNTT_PB, IDS_MENU_FILE_FILES, MNTS_B | MNTS_I | MNTS_A, CML_RECENTFILES, -1, 0, NULL},
        {MNTT_PE},
        {MNTT_PB, IDS_MENU_FILE_DIRS, MNTS_B | MNTS_I | MNTS_A, CML_RECENTDIRS, -1, 0, NULL},
        {MNTT_PE},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_EXIT, MNTS_B | MNTS_I | MNTS_A, CMD_EXIT, -1, 0, NULL},
        {MNTT_PE},

        // Edit
        {MNTT_PB, IDS_MENU_EDIT, MNTS_B | MNTS_I | MNTS_A, CML_EDIT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CMD_COPY, IDX_TB_COPY, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_PASTE, MNTS_B | MNTS_I | MNTS_A, CMD_PASTE, IDX_TB_PASTE, 0, (DWORD*)vwePaste},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_SELECTALL, MNTS_B | MNTS_I | MNTS_A, CMD_SELECTALL, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_DESELECT, MNTS_B | MNTS_I | MNTS_A, CMD_DESELECT, -1, 0, (DWORD*)vweSelection},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_CROP, MNTS_B | MNTS_I | MNTS_A, CMD_CROP, IDX_TB_CROP, 0, (DWORD*)vweSelection},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_LEFT, MNTS_B | MNTS_I | MNTS_A, CMD_ROTATE_LEFT, IDX_TB_LEFT, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_RIGHT, MNTS_B | MNTS_I | MNTS_A, CMD_ROTATE_RIGHT, IDX_TB_RIGHT, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_180, MNTS_B | MNTS_I | MNTS_A, CMD_ROTATE180, IDX_TB_180, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_EDIT_FLIPH, MNTS_B | MNTS_I | MNTS_A, CMD_MIRROR_HOR, IDX_TB_FLIPH, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_FLIPV, MNTS_B | MNTS_I | MNTS_A, CMD_MIRROR_VERT, IDX_TB_FLIPV, 0, (DWORD*)vweFileOpened},
        {MNTT_PE},

        // View
        {MNTT_PB, IDS_MENU_VIEW, MNTS_B | MNTS_I | MNTS_A, CML_VIEW, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_ZOOMIN, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOM_IN, IDX_TB_ZOOMIN, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_VIEW_ZOOMOUT, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOM_OUT, IDX_TB_ZOOMOUT, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_VIEW_ZOOM, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOM_TO, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_WHOLE, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOMWHOLE, IDX_TB_ZOOMWHOLE, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_VIEW_WIDTH, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOMWIDTH, IDX_TB_ZOOMWIDTH, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_VIEW_ACTUAL, MNTS_B | MNTS_I | MNTS_A, CMD_ZOOMACTUAL, IDX_TB_ZOOMACTUAL, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_TOOLBAR, MNTS_B | MNTS_I | MNTS_A, CMD_TOOLBAR, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_STATUSBAR, MNTS_B | MNTS_I | MNTS_A, CMD_STATUSBAR, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_FULLSCREEN, MNTS_B | MNTS_I | MNTS_A, CMD_FULLSCREEN, IDX_TB_FULLSCREEN, 0, (DWORD*)vweFileOpened /*vweNotLoading*/},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_PB, IDS_MENU_MULTIPAGE, MNTS_B | MNTS_I | MNTS_A, CML_MULTIPAGE, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_PREVPAGE, MNTS_B | MNTS_I | MNTS_A, CMD_PREVPAGE, IDX_TB_PREVPAGE, 0, (DWORD*)vwePrevPage},
        {MNTT_IT, IDS_MENU_NEXTPAGE, MNTS_B | MNTS_I | MNTS_A, CMD_NEXTPAGE, IDX_TB_NEXTPAGE, 0, (DWORD*)vweNextPage},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FIRSTPAGE, MNTS_B | MNTS_I | MNTS_A, CMD_FIRSTPAGE, -1, 0, (DWORD*)vwePrevPage},
        {MNTT_IT, IDS_MENU_LASTPAGE, MNTS_B | MNTS_I | MNTS_A, CMD_LASTPAGE, -1, 0, (DWORD*)vweNextPage},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_PAGE, MNTS_B | MNTS_I | MNTS_A, CMD_PAGE, -1, 0, (DWORD*)vweMorePages},
        {MNTT_PE},
        {MNTT_PE},

        // Tools
        {MNTT_PB, IDS_MENU_TOOLS, MNTS_B | MNTS_I | MNTS_A, CML_TOOLS, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_TOOLS_HAND, MNTS_B | MNTS_I | MNTS_A, CMD_TOOLS_HAND, IDX_TB_HAND, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_TOOLS_ZOOM, MNTS_B | MNTS_I | MNTS_A, CMD_TOOLS_ZOOM, IDX_TB_ZOOM, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_TOOLS_SELECT, MNTS_B | MNTS_I | MNTS_A, CMD_TOOLS_SELECT, IDX_TB_SELECT, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_TOOLS_PICK, MNTS_B | MNTS_I | MNTS_A, CMD_TOOLS_PIPETTE, IDX_TB_PICK, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_TOOLS_OPTIONS, MNTS_B | MNTS_I | MNTS_A, CMD_CFG, -1, 0, NULL},
        {MNTT_PE},

        // Help
        {MNTT_PB, IDS_MENU_HELP, MNTS_B | MNTS_I | MNTS_A, CML_HELP, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_CONTENTS, MNTS_B | MNTS_I | MNTS_A, CMD_HELP_CONTENTS, IDX_TB_HELP, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_INDEX, MNTS_B | MNTS_I | MNTS_A, CMD_HELP_INDEX, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_HELP_SEARCH, MNTS_B | MNTS_I | MNTS_A, CMD_HELP_SEARCH, -1, 0, NULL},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        //    {MNTT_IT,    IDS_MENU_HELP_KEYBOARD,       MNTS_B|MNTS_I|MNTS_A, CMD_HELP_KEYBOARD,          -1,                      0,                 NULL},
        //    {MNTT_IT,    IDS_MENU_HELP_MOUSE,          MNTS_B|MNTS_I|MNTS_A, CMD_HELP_MOUSE,             -1,                      0,                 NULL},
        //    {MNTT_SP,    -1,                           MNTS_B|MNTS_I|MNTS_A, 0,                          -1,                      0,                 NULL},
        {MNTT_IT, IDS_MENU_HELP_ABOUT, MNTS_B | MNTS_I | MNTS_A, CMD_ABOUT, -1, 0, NULL},
        {MNTT_PE},

        {MNTT_PE}, // terminator
};

MENU_TEMPLATE_ITEM PopupMenuTemplate[] =
    {
        {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, CML_CONTEXT, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PREV, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_PREV, IDX_TB_PREV, 0, (DWORD*)vwePrevFile},
        {MNTT_IT, IDS_MENU_FILE_NEXT, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_NEXT, IDX_TB_NEXT, 0, (DWORD*)vweNextFile},
        {MNTT_IT, IDS_MENU_EDIT_COPY, MNTS_B | MNTS_I | MNTS_A, CMD_COPY, IDX_TB_COPY, 0, (DWORD*)vweFileOpened},
        {MNTT_IT, IDS_MENU_EDIT_PASTE, MNTS_B | MNTS_I | MNTS_A, CMD_PASTE, IDX_TB_PASTE, 0, (DWORD*)vwePaste},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_PROP, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_PROP, IDX_TB_PROPERTIES, 0, (DWORD*)vweImgInfoAvailable},
        {MNTT_IT, IDS_MENU_FILE_EXIF, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_EXIF, -1, 0, (DWORD*)vweImgExifAvailable},
        {MNTT_IT, IDS_MENU_FILE_HISTOGRAM, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_HISTOGRAM, -1, 0, (DWORD*)vweFileOpened},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_SELCRCFILE, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_TOGGLESELECT, IDX_TB_SELSRCFILE, 0, (DWORD*)vweSelSrcFile},
        {MNTT_IT, IDS_MENU_FILE_PREVSEL, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_PREVSELFILE, IDX_TB_PREVSELFILE, 0, (DWORD*)vwePrevSelFile},
        {MNTT_IT, IDS_MENU_FILE_NEXTSEL, MNTS_B | MNTS_I | MNTS_A, CMD_FILE_NEXTSELFILE, IDX_TB_NEXTSELFILE, 0, (DWORD*)vweNextSelFile},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_FILE_FOCUS, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_FOCUS, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_RENAME, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_RENAME, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_IT, IDS_MENU_FILE_DELETE, MNTS_B | MNTS_I | MNTS_A, CMD_IMG_DELETE, -1, 0, (DWORD*)vweFileOpened2},
        {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
        {MNTT_IT, IDS_MENU_VIEW_FULLSCREEN, MNTS_B | MNTS_I | MNTS_A, CMD_FULLSCREEN, IDX_TB_FULLSCREEN, 0, (DWORD*)vweFileOpened /*vweNotLoading*/},
        {MNTT_PE}, // terminator
};

struct CButtonData
{
    int ImageIndex;                   // zero base index
    WORD ToolTipResID;                // resID with the string for the tooltip
    WORD ID;                          // universal command
    CViewerWindowEnablerEnum Enabler; // control variable for enabling the button
};

CButtonData ToolBarButtons[] =
    {
        // ImageIndex          ToolTipResID          ID               Enabler

        {IDX_TB_OPEN, IDS_TT_OPEN, CMD_OPEN, vweNotLoading},
        {IDX_TB_SAVE, IDS_TT_SAVE, CMD_SAVEAS, vweFileOpened},
        {IDX_TB_PROPERTIES, IDS_TT_PROPERTIES, CMD_IMG_PROP, vweFileOpened},
        {IDX_TB_PRINT, IDS_TT_PRINT, CMD_PRINT, vweFileOpened},
        {IDX_TB_SEPARATOR},
        {IDX_TB_PREV, IDS_TT_PREV, CMD_FILE_PREV, vwePrevFile},
        {IDX_TB_NEXT, IDS_TT_NEXT, CMD_FILE_NEXT, vweNextFile},
        {IDX_TB_SELSRCFILE, IDS_TT_SELSRCFILE, CMD_FILE_TOGGLESELECT, vweSelSrcFile},
        {IDX_TB_PREVSELFILE, IDS_TT_PREVSELFILE, CMD_FILE_PREVSELFILE, vwePrevSelFile},
        {IDX_TB_NEXTSELFILE, IDS_TT_NEXTSELFILE, CMD_FILE_NEXTSELFILE, vweNextSelFile},
        {IDX_TB_SEPARATOR},
        {IDX_TB_COPY, IDS_TT_COPY, CMD_COPY, vweFileOpened},
        {IDX_TB_PASTE, IDS_TT_PASTE, CMD_PASTE, vwePaste},
        {IDX_TB_SEPARATOR},
        {IDX_TB_HAND, IDS_TT_TOOL_HAND, CMD_TOOLS_HAND, vweFileOpened},
        {IDX_TB_ZOOM, IDS_TT_TOOL_ZOOM, CMD_TOOLS_ZOOM, vweFileOpened},
        {IDX_TB_SELECT, IDS_TT_TOOL_SELECT, CMD_TOOLS_SELECT, vweFileOpened},
        {IDX_TB_PICK, IDS_TT_TOOL_PIPETTE, CMD_TOOLS_PIPETTE, vweFileOpened},
        {IDX_TB_SEPARATOR},
        {IDX_TB_LEFT, IDS_TT_LEFT, CMD_ROTATE_LEFT, vweFileOpened},
        {IDX_TB_RIGHT, IDS_TT_RIGHT, CMD_ROTATE_RIGHT, vweFileOpened},
        {IDX_TB_CROP, IDS_TT_CROP, CMD_CROP, vweSelection},
        {IDX_TB_SEPARATOR},
        {IDX_TB_ZOOMOUT, IDS_TT_ZOOM_OUT, CMD_ZOOM_OUT, vweFileOpened},
        {IDX_TB_ZOOMNUMBER, IDS_TT_ZOOM_TO, CMD_ZOOM_TO, vweFileOpened},
        {IDX_TB_ZOOMIN, IDS_TT_ZOOM_IN, CMD_ZOOM_IN, vweFileOpened},
        {IDX_TB_SEPARATOR},
        {IDX_TB_ZOOMWHOLE, IDS_TT_ZOOM_WHOLE, CMD_ZOOMWHOLE, vweFileOpened},
        {IDX_TB_ZOOMWIDTH, IDS_TT_ZOOM_WIDTH, CMD_ZOOMWIDTH, vweFileOpened},
        {IDX_TB_ZOOMACTUAL, IDS_TT_ZOOM_ACTUAL, CMD_ZOOMACTUAL, vweFileOpened},
        {IDX_TB_FULLSCREEN, IDS_TT_FULL_SCREEN, CMD_FULLSCREEN, vweFileOpened /*vweNotLoading*/},
        {IDX_TB_TERMINATOR}};

// sets colors for the entries defined by vceCount
void RebuildColors(SALCOLOR* colors)
{
    if (GetFValue(colors[vceBackground]) & SCF_DEFAULT)
        SetRGBPart(&colors[vceBackground], GetSysColor(COLOR_APPWORKSPACE));
    if (GetFValue(colors[vceTransparent]) & SCF_DEFAULT)
        SetRGBPart(&colors[vceTransparent], GetSysColor(COLOR_WINDOW));
    if (GetFValue(colors[vceFSBackground]) & SCF_DEFAULT)
        SetRGBPart(&colors[vceFSBackground], RGB(0, 0, 0));
    if (GetFValue(colors[vceFSTransparent]) & SCF_DEFAULT)
        SetRGBPart(&colors[vceFSTransparent], GetSysColor(COLOR_WINDOW));
}

void InitGlobalGUIParameters(void)
{
    G.rgbPanelBackground = SalamanderGeneral->GetCurrentColor(SALCOL_ITEM_BK_NORMAL);

    RebuildColors(G.Colors);

    G.TotalNCWidth = 2 * GetSystemMetrics(SM_CXFRAME) + 2 * GetSystemMetrics(SM_CXEDGE); // edges of renderer
    G.TotalNCHeight = 2 * GetSystemMetrics(SM_CYFRAME) + 2 * GetSystemMetrics(SM_CYEDGE) // edges of renderer
                      + GetSystemMetrics(SM_CYCAPTION);
} /* InitGlobalGUIParameters */

int ShowOneTimeMessage(HWND HParent, int msg, BOOL* pChecked, int flags, int dontShowMsg)
{
    MSGBOXEX_PARAMS params;
    memset(&params, 0, sizeof(params));
    params.HParent = HParent;
    // key icon flag if specified or add info icon
    params.Flags = flags | MSGBOXEX_ESCAPEENABLED | ((flags & (MSGBOXEX_ICONHAND | MSGBOXEX_ICONQUESTION | MSGBOXEX_ICONEXCLAMATION)) ? 0 : MSGBOXEX_ICONINFORMATION);
    params.Caption = LoadStr(IDS_PLUGINNAME);
    params.Text = LoadStr(msg);
    params.CheckBoxText = LoadStr(dontShowMsg);
    params.CheckBoxValue = pChecked;
    return SalamanderGeneral->SalMessageBoxEx(&params);
}

//
// ****************************************************************************
// LoadStr
//

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

WCHAR* LoadStrW(int resID)
{
    return SalamanderGeneral->LoadStrW(HLanguage, resID);
}

const char* WINAPI GetExtText(int msgID)
{
    char* ret;

    ret = LoadStr(IDS_DLL + msgID);
    if (!strcmp(ret, "ERROR LOADING STRING"))
    {
        return "";
    }
    return ret;
}

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH) // start PictView.spl
    {
        DLLInstance = hinstDLL;
    }

    if (fdwReason == DLL_PROCESS_DETACH) // shutdown (unload) PVW32.SPL
    {
        if (EXIFLibrary != NULL)
        {
            FreeLibrary(EXIFLibrary); // release EXIF.DLL as well
            EXIFLibrary = NULL;
        }
        if (PVW32DLL.Handle != NULL)
            FreeLibrary(PVW32DLL.Handle); // release PVW32.DLL as well
        if (G.HAccel != NULL)
            DestroyAcceleratorTable(G.HAccel);
        if (G.CaptureAtomID != 0)
            GlobalDeleteAtom(G.CaptureAtomID);
    }

    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    HWND hParentWnd = salamander->GetParentWindow();

    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is made for the current Salamander version and higher - perform the check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(hParentWnd,
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   PLUGIN_NAME_EN, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // let it load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), PLUGIN_NAME_EN);
    if (HLanguage == NULL)
        return NULL;

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();

    // set the help file name
    SalamanderGeneral->SetHelpFileName("pictview.chm");

    // obtain the interface providing customized Windows controls used in Salamander
    SalamanderGUI = salamander->GetSalamanderGUI();

    if (!InitViewer(hParentWnd))
        return NULL; // error

    // set the basic plugin information
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME), FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   _T("PictView") /* do not translate! */);

    // set the plugin home page URL
    salamander->SetPluginHomePageURL("www.pictview.com/salamander");

    // If we crash inside pictview.spl, this message box will be displayed
    // and the happy recipient of the images will be Honza Patera.
    // Honza appears on the web in the plural (authors, we fixed, ....)
    TCHAR exceptInfo[512];
    lstrcpyn(exceptInfo, LoadStr(IDS_EXCEPT_INFO1), SizeOf(exceptInfo));
    _tcsncat(exceptInfo, LoadStr(IDS_EXCEPT_INFO2), SizeOf(exceptInfo) - _tcslen(exceptInfo));
    SalamanderGeneral->SetPluginBugReportInfo(exceptInfo, "support@pictview.com");
    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    CAboutDialog dlg(parent);
    dlg.Execute();
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);
    BOOL ret = ViewerWindowQueue.Empty();
    if (!ret && (force || SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SOME_WINS_OPENED),
                                                           LoadStr(IDS_PLUGINNAME),
                                                           MB_YESNO | MB_ICONQUESTION) == IDYES))
    {
        ret = ViewerWindowQueue.CloseAllWindows(force) || force;
    }
    if (ret)
    {
        if (!ThreadQueue.KillAll(force) && !force)
            ret = FALSE;
        else
            ReleaseViewer();
    }
    return ret;
}

// ****************************************************************************

BOOL LoadHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, LPCTSTR name, LPTSTR history[], int maxCount)
{
    HKEY historyKey;
    int i;
    for (i = 0; i < maxCount; i++)
        if (history[i] != NULL)
        {
            free(history[i]);
            history[i] = NULL;
        }
    if (registry->OpenKey(hKey, name, historyKey))
    {
        TCHAR buf[10];
        int j;
        for (j = 0; j < maxCount; j++)
        {
            _itot(j + 1, buf, 10);
            DWORD bufferSize;
            if (registry->GetSize(historyKey, buf, REG_SZ, bufferSize))
            {
                history[j] = (LPTSTR)malloc(bufferSize);
                if (history[j] == NULL)
                {
                    TRACE_E("Low memory");
                    break;
                }
                if (!registry->GetValue(historyKey, buf, REG_SZ, history[j], bufferSize))
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

// ****************************************************************************

BOOL SaveHistory(CSalamanderRegistryAbstract* registry, HKEY hKey, LPCTSTR name, LPTSTR history[], int maxCount)
{
    HKEY historyKey;
    if (registry->CreateKey(hKey, name, historyKey))
    {
        registry->ClearKey(historyKey);

        BOOL saveHistory = FALSE;
        SalamanderGeneral->GetConfigParameter(SALCFG_SAVEHISTORY, &saveHistory, sizeof(BOOL), NULL);
        if (saveHistory)
        {
            TCHAR buf[10];
            int i;
            for (i = 0; i < maxCount; i++)
            {
                if (history[i] != NULL)
                {
                    _itot(i + 1, buf, 10);
                    registry->SetValue(historyKey, buf, REG_SZ, history[i], -1);
                }
                else
                    break;
            }
        }
        registry->CloseKey(historyKey);
    }
    return TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    HKEY hSaveKey;

    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL) // load from the registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = CURRENT_CONFIG_VERSION; // probably some prankster... ;-)
        }
        if (!registry->GetValue(regKey, CONFIG_SHRINK, REG_DWORD, &G.ZoomType, sizeof(DWORD)))
        {
            G.ZoomType = eShrinkToFit;
        }

        // for versions prior to 14 (Salamander 2.5 beta 7) force the use of one of the viewing modes
        // where the entire image is visible (most people view photos and want to see them complete)
        if (ConfigVersion < 14 && G.ZoomType != eShrinkToFit && G.ZoomType != eZoomFullScreen)
            G.ZoomType = eShrinkToFit;

        if (!registry->GetValue(regKey, CONFIG_PAGEDNUP, REG_DWORD, &G.PageDnUpScrolls, sizeof(DWORD)))
        {
            G.PageDnUpScrolls = TRUE;
        }
        if (!registry->GetValue(regKey, CONFIG_AUTOROTATE, REG_DWORD, &G.AutoRotate, sizeof(DWORD)))
        {
            G.AutoRotate = TRUE;
        }
        if (!registry->GetValue(regKey, CONFIG_PIPETTE, REG_DWORD, &G.PipetteInHex, sizeof(DWORD)))
        {
            G.PipetteInHex = FALSE;
        }
        if (!registry->GetValue(regKey, CONFIG_WINPOS, REG_DWORD, &G.WindowPos, sizeof(DWORD)) ||
            ((G.WindowPos != WINDOW_POS_SAME) && (G.WindowPos != WINDOW_POS_LARGER) && (G.WindowPos != WINDOW_POS_ANY)))
        {
            G.WindowPos = WINDOW_POS_SAME;
        }
        if (!registry->GetValue(regKey, CONFIG_THUMBNAILS, REG_DWORD, &G.IgnoreThumbnails, sizeof(DWORD)))
        {
            G.IgnoreThumbnails = FALSE;
        }
        if (!registry->GetValue(regKey, CONFIG_THUMB_MAX_IMG_SIZE, REG_DWORD, &G.MaxThumbImgSize, sizeof(DWORD)))
        {
            G.MaxThumbImgSize = PV_MAX_IMG_SIZE_TO_THUMBNAIL; // megapixels
        }

        if (registry->OpenKey(regKey, CONFIG_SAVE, hSaveKey))
        {
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_FLAGS, REG_DWORD, &G.Save.Flags, sizeof(DWORD)))
            {
                G.Save.Flags = PVSF_GIF89;
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_JPEG_QUALITY, REG_DWORD, &G.Save.JPEGQuality, sizeof(DWORD)))
            {
                G.Save.JPEGQuality = 75;
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_JPEG_SUBSAMPLING, REG_DWORD, &G.Save.JPEGSubsampling, sizeof(DWORD)))
            {
                G.Save.JPEGSubsampling = 1;
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_TIFF_STRIP_SIZE, REG_DWORD, &G.Save.TIFFStripSize, sizeof(DWORD)))
            {
                G.Save.TIFFStripSize = 64;
            }
            if (!registry->GetValue(regKey, CONFIG_SAVE_REMEMBER_PATH, REG_DWORD, &G.Save.RememberPath, sizeof(DWORD)))
            {
                G.Save.RememberPath = TRUE; // compatible with previous version of PictView
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_INIT_DIR, REG_SZ, G.Save.InitDir, MAX_PATH))
            {
                G.Save.InitDir[0] = 0;
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_FILTER_MONO, REG_DWORD, &G.LastSaveAsFilterIndexMono, sizeof(DWORD)))
            {
                G.LastSaveAsFilterIndexMono = 14; // index is 1-based (IDS_SAVEASFILTERMONO: 14=BMP)
            }
            if (!registry->GetValue(hSaveKey, CONFIG_SAVE_FILTER_COLOR, REG_DWORD, &G.LastSaveAsFilterIndexColor, sizeof(DWORD)))
            {
                G.LastSaveAsFilterIndexColor = 13; // index is 1-based (IDS_SAVEASFILTERCOLOR: 13=BMP)
            }
            registry->CloseKey(hSaveKey);
        }

        if (!registry->GetValue(regKey, CONFIG_TOOLBARVISIBLE, REG_DWORD, &G.ToolbarVisible, sizeof(DWORD)))
        {
            G.ToolbarVisible = TRUE;
        }
        if (!registry->GetValue(regKey, CONFIG_STATUSBARVISIBLE, REG_DWORD, &G.StatusbarVisible, sizeof(DWORD)))
        {
            G.StatusbarVisible = TRUE;
        }

        if (!registry->GetValue(regKey, CONFIG_PATHINTITLE, REG_DWORD, &G.bShowPathInTitle, sizeof(DWORD)))
        {
            G.bShowPathInTitle = TRUE;
        }
        if (!registry->GetValue(regKey, CONFIG_DONTSHOWANYMORE, REG_DWORD, &G.DontShowAnymore, sizeof(DWORD)))
        {
            G.DontShowAnymore = 0;
        }

        if (ConfigVersion >= 10) // incompatible format, discard the old configuration
        {
            registry->GetValue(regKey, CONFIG_RGBRENDWSCOLOR, REG_DWORD, &G.Colors[vceBackground], sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_RGBRENDBGCOLOR, REG_DWORD, &G.Colors[vceTransparent], sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_RGBFSWSCOLOR, REG_DWORD, &G.Colors[vceFSBackground], sizeof(DWORD));
            registry->GetValue(regKey, CONFIG_RGBFSBGCOLOR, REG_DWORD, &G.Colors[vceFSTransparent], sizeof(DWORD));
        }

        if (!registry->GetValue(regKey, CONFIG_SELECT_RATIO_X, REG_DWORD, &G.SelectRatioX, sizeof(DWORD)) || (G.SelectRatioX <= 0))
        {
            G.SelectRatioX = 1;
        }
        if (!registry->GetValue(regKey, CONFIG_SELECT_RATIO_Y, REG_DWORD, &G.SelectRatioY, sizeof(DWORD)) || (G.SelectRatioY <= 0))
        {
            G.SelectRatioY = 1;
        }

        G.ExifDlgWidth = 520;
        G.ExifDlgHeight = 440;
        registry->GetValue(regKey, CONFIG_EXIFDLGWIDTH, REG_DWORD, &G.ExifDlgWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_EXIFDLGHEIGHT, REG_DWORD, &G.ExifDlgHeight, sizeof(DWORD));

        InitGlobalGUIParameters();

        registry->GetValue(regKey, CONFIG_EXIFGROUPHIGHLIGHTS, REG_DWORD, &ExifGroupHighlights, sizeof(DWORD));
        TCHAR buf[5000];
        registry->GetValue(regKey, CONFIG_EXIFHIGHLIGHTS, REG_SZ, buf, 5000);
        LPTSTR p = _tcstok(buf, _T(","));
        while (p != NULL)
        {
            int tag = _ttoi(p);
            ExifHighlights.Add(tag);
            if (!ExifHighlights.IsGood())
            {
                ExifHighlights.ResetState();
                break;
            }
            p = _tcstok(NULL, _T(","));
        }

        LoadHistory(registry, regKey, CONFIG_FILESHISTORY, G.FilesHistory, FILES_HISTORY_SIZE);
        LoadHistory(registry, regKey, CONFIG_DIRSHISTORY, G.DirsHistory, DIRS_HISTORY_SIZE);

        HKEY hCopyToKey;
        if (registry->OpenKey(regKey, CONFIG_COPYTO, hCopyToKey))
        {
            TCHAR buf2[10];

            int i;
            for (i = 0; i < COPYTO_LINES; i++)
            {
                DWORD bufferSize;

                _itot(i + 1, buf2, 10);
                if (registry->GetSize(hCopyToKey, buf2, REG_SZ, bufferSize))
                {
                    // Must do this complicated way because the strings are later
                    // freed via SalamanderGeneral->Free which may crash when using
                    // various version of RTL (salrtl9.dll vs. msvcr90d/libcmtd)
                    LPTSTR buff = (LPTSTR)malloc(bufferSize);
                    if (buff == NULL)
                        break;
                    if (!registry->GetValue(hCopyToKey, buf2, REG_SZ, buff, bufferSize))
                        break;
                    G.CopyToDestinations[i] = SalamanderGeneral->DupStr(buff);
                    free(buff);
                }
            }
            registry->CloseKey(hCopyToKey);
        }

        CHistogramWindow::LoadConfiguration(regKey, registry);
    }
    else // default configuration
    {
        ConfigVersion = 0;
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    HKEY hSaveKey;

    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
    v = G.ZoomType;
    registry->SetValue(regKey, CONFIG_SHRINK, REG_DWORD, &v, sizeof(DWORD));
    v = G.PageDnUpScrolls;
    registry->SetValue(regKey, CONFIG_PAGEDNUP, REG_DWORD, &v, sizeof(DWORD));
    v = G.AutoRotate;
    registry->SetValue(regKey, CONFIG_AUTOROTATE, REG_DWORD, &v, sizeof(DWORD));
    v = G.PipetteInHex;
    registry->SetValue(regKey, CONFIG_PIPETTE, REG_DWORD, &v, sizeof(DWORD));
    v = G.WindowPos;
    registry->SetValue(regKey, CONFIG_WINPOS, REG_DWORD, &v, sizeof(DWORD));
    v = G.IgnoreThumbnails;
    registry->SetValue(regKey, CONFIG_THUMBNAILS, REG_DWORD, &v, sizeof(DWORD));
    v = G.MaxThumbImgSize;
    registry->SetValue(regKey, CONFIG_THUMB_MAX_IMG_SIZE, REG_DWORD, &v, sizeof(DWORD));
    v = G.ToolbarVisible;
    registry->SetValue(regKey, CONFIG_TOOLBARVISIBLE, REG_DWORD, &v, sizeof(DWORD));
    v = G.StatusbarVisible;
    registry->SetValue(regKey, CONFIG_STATUSBARVISIBLE, REG_DWORD, &v, sizeof(DWORD));
    v = G.bShowPathInTitle;
    registry->SetValue(regKey, CONFIG_PATHINTITLE, REG_DWORD, &v, sizeof(DWORD));
    v = G.DontShowAnymore;
    registry->SetValue(regKey, CONFIG_DONTSHOWANYMORE, REG_DWORD, &v, sizeof(DWORD));
    v = G.Colors[vceBackground];
    registry->SetValue(regKey, CONFIG_RGBRENDWSCOLOR, REG_DWORD, &v, sizeof(DWORD));
    v = G.Colors[vceTransparent];
    registry->SetValue(regKey, CONFIG_RGBRENDBGCOLOR, REG_DWORD, &v, sizeof(DWORD));
    v = G.Colors[vceFSBackground];
    registry->SetValue(regKey, CONFIG_RGBFSWSCOLOR, REG_DWORD, &v, sizeof(DWORD));
    v = G.Colors[vceFSTransparent];
    registry->SetValue(regKey, CONFIG_RGBFSBGCOLOR, REG_DWORD, &v, sizeof(DWORD));

    v = G.SelectRatioX;
    registry->SetValue(regKey, CONFIG_SELECT_RATIO_X, REG_DWORD, &v, sizeof(DWORD));
    v = G.SelectRatioY;
    registry->SetValue(regKey, CONFIG_SELECT_RATIO_Y, REG_DWORD, &v, sizeof(DWORD));

    if (registry->CreateKey(regKey, CONFIG_SAVE, hSaveKey))
    {
        v = G.Save.Flags;
        registry->SetValue(hSaveKey, CONFIG_SAVE_FLAGS, REG_DWORD, &v, sizeof(DWORD));
        v = G.Save.JPEGQuality;
        registry->SetValue(hSaveKey, CONFIG_SAVE_JPEG_QUALITY, REG_DWORD, &v, sizeof(DWORD));
        v = G.Save.JPEGSubsampling;
        registry->SetValue(hSaveKey, CONFIG_SAVE_JPEG_SUBSAMPLING, REG_DWORD, &v, sizeof(DWORD));
        v = G.Save.TIFFStripSize;
        registry->SetValue(hSaveKey, CONFIG_SAVE_TIFF_STRIP_SIZE, REG_DWORD, &v, sizeof(DWORD));
        registry->SetValue(hSaveKey, CONFIG_SAVE_INIT_DIR, REG_SZ, G.Save.InitDir, -1);
        v = G.Save.RememberPath;
        registry->SetValue(hSaveKey, CONFIG_SAVE_REMEMBER_PATH, REG_DWORD, &v, sizeof(DWORD));
        registry->SetValue(hSaveKey, CONFIG_SAVE_FILTER_MONO, REG_DWORD, &G.LastSaveAsFilterIndexMono, sizeof(DWORD));
        registry->SetValue(hSaveKey, CONFIG_SAVE_FILTER_COLOR, REG_DWORD, &G.LastSaveAsFilterIndexColor, sizeof(DWORD));
        registry->CloseKey(hSaveKey);
    }

    registry->SetValue(regKey, CONFIG_EXIFDLGWIDTH, REG_DWORD, &G.ExifDlgWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_EXIFDLGHEIGHT, REG_DWORD, &G.ExifDlgHeight, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_EXIFGROUPHIGHLIGHTS, REG_DWORD, &ExifGroupHighlights, sizeof(DWORD));
    char buf[5000];
    char* p = buf;
    int i;
    for (i = 0; i < ExifHighlights.Count; i++)
    {
        p += sprintf(p, "%u", ExifHighlights[i]);
        if (p - buf > 4950)
            break;
        if (i < ExifHighlights.Count - 1)
        {
            *p = ',';
            p++;
        }
    }
    *p = 0;
    registry->SetValue(regKey, CONFIG_EXIFHIGHLIGHTS, REG_SZ, buf, -1);

    SaveHistory(registry, regKey, CONFIG_FILESHISTORY, G.FilesHistory, FILES_HISTORY_SIZE);
    SaveHistory(registry, regKey, CONFIG_DIRSHISTORY, G.DirsHistory, DIRS_HISTORY_SIZE);

    HKEY hCopyToKey;
    if (registry->CreateKey(regKey, CONFIG_COPYTO, hCopyToKey))
    {
        registry->ClearKey(hCopyToKey); // store only variables that are not NULL
        TCHAR buf2[10];
        int j;
        for (j = 0; j < COPYTO_LINES; j++)
        {
            LPCTSTR line = G.CopyToDestinations[j];
            if (line != NULL)
            {
                _itot(j + 1, buf2, 10);
                registry->SetValue(hCopyToKey, buf2, REG_SZ, line, -1);
            }
        }
        registry->CloseKey(hCopyToKey);
    }

    CHistogramWindow::SaveConfiguration(regKey, registry);
}

void OnConfiguration(HWND hParent)
{
    static BOOL InConfiguration = FALSE;
    if (InConfiguration)
    {
        SalamanderGeneral->SalMessageBox(hParent,
                                         LoadStr(IDS_CFG_CONFLICT), LoadStr(IDS_PLUGINNAME),
                                         MB_ICONINFORMATION | MB_OK);
        return;
    }
    InConfiguration = TRUE;

    BOOL ignoreThumbnails = G.IgnoreThumbnails;
    DWORD maxThumbImgSize = G.MaxThumbImgSize;

    if (CConfigDialog(hParent).Execute() == IDOK)
    {
        ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
        if (ignoreThumbnails != G.IgnoreThumbnails || maxThumbImgSize != G.MaxThumbImgSize)
        { // if the panel contains thumbnails, a refresh is needed
            SalamanderGeneral->PostMenuExtCommand(CMD_INTERNAL_REREADTHUMBS, TRUE);
        }
    }
    InConfiguration = FALSE;
}

void CPluginInterface::Configuration(HWND parent)
{
    OnConfiguration(parent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // this section should contain all suffixes the viewer can process,
    // the suffixes are added only when inserting the plugin into Salamander (during
    // plugin installation or auto-installation at the first Salamander launch),
    // this entire section is ignored for plugin upgrades
    salamander->AddViewer("*.psp*;*.dtx;*.dds;*.nef;*.crw;*.eps;*.ept;*.ai;*.raf;*.mov;*.hpi", FALSE);
    salamander->AddViewer("*.pntg;*.thumb;*.tiff;*.wbmp;*.ani;*.clk;*.mbm;*.thm;*.zno;*.mng", FALSE);
    salamander->AddViewer("*.st;*.cals;*.itiff;*.jfif;*.jpeg;*.macp;*.mpnt;*.paint;*.pict;*.2bp", FALSE);
    salamander->AddViewer("*.stw;*.sun;*.tga;*.tif;*.udi;*.web;*.wpg;*.xar;*.zbr;*.zmf;*.bw", FALSE);
    salamander->AddViewer("*.psd;*.pyx;*.qfx;*.ras;*.rgb;*.rle;*.sam;*.scx;*.sep;*.sgi;*.ska", FALSE);
    salamander->AddViewer("*.pat;*.pbm;*.pc2;*.pcd;*.pct;*.pcx;*.pgm;*.pic;*.png;*.pnm;*.ppm", FALSE);
    salamander->AddViewer("*.jff;*.jif;*.jmx;*.jpe;*.jpg;*.lbm;*.mac;*.mil;*.msp;*.ofx;*.pan", FALSE);
    salamander->AddViewer("*.flc;*.fli;*.gem;*.gif;*.ham;*.hmr;*.hrz;*.icn;*.ico;*.iff;*.img", FALSE);
    salamander->AddViewer("*.cdt;*.cel;*.clp;*.cit;*.cmx;*.cot;*.cpt;*.cur;*.cut;*.dcx;*.dib", FALSE);
    salamander->AddViewer("*.82i;*.83i;*.85i;*.86i;*.89i;*.92i;*.awd;*.bmi;*.bmp;*.cal;*.cdr", FALSE);
    salamander->AddViewer("*.arw;*.blp;*.cr2;*.dng;*.orf;*.pef", FALSE);

    // this section contains all suffixes added in (removed from) version X
    if (ConfigVersion < 3) // suffixes added in version 3
    {
        salamander->AddViewer("*.cal;*.cals;*.cit;*.mil;*.st;*.stw;*.zmf", TRUE);
    }
    if (ConfigVersion < 4) // suffixes added in version 4
    {
        salamander->AddViewer("*.macp;*.mpnt;*.paint;*.pntg", TRUE);
    }
    if (ConfigVersion < 5) // suffixes added in version 5
    {
        salamander->AddViewer("*.cot", TRUE);
    }
    if (ConfigVersion < 6) // suffixes added in (removed from) version 6
    {
        salamander->AddViewer("*.hmr;*.itiff", TRUE);
    }
    if (ConfigVersion < 7) // suffixes added in (removed from) version 7
    {
        salamander->AddViewer("*.gem;*.awd", TRUE);
    }
    if (ConfigVersion < 8) // suffixes added in (removed from) version 8; SS 2.5 Beta 1
    {
        // THUMB: PCTV Vision belonging to the Pinnacle Systems Studio PCTV PRO TV tuner
        salamander->AddViewer("*.82i;*.83i;*.85i;*.86i;*.89i;*.92i;*.ska;*.sun;*.thumb;*.web;*.xar;*.wbmp;*.mbm", TRUE);
    }
    if (ConfigVersion < 9) // suffixes added in (removed from) version 9; SS 2.5 Beta 2
    {
        salamander->AddViewer("*.ani;*.psp", TRUE);
    }
    if (ConfigVersion < 10) // suffixes added in (removed from) version 10; SS 2.5 Beta 3
    {
        // added the .2bp suffix (BMP under Pocket PC, comes in handy for the POCKETPC plugin)
        salamander->AddViewer("*.clk;*.thm;*.zno;*.2bp", TRUE);
    }
    if (ConfigVersion < 11) // suffixes added in (removed from) version 11; SS 2.5 Beta 4
    {
        salamander->AddViewer("*.mng", TRUE);
    }
    if (ConfigVersion < 12) // suffixes added in (removed from) version 12
    {
        // PSP8 uses ugly suffixes like pspimage
        salamander->ForceRemoveViewer("*.psp");
        salamander->AddViewer("*.dtx;*.psp*", TRUE);
    }
    if (ConfigVersion < 13) // suffixes added in (removed from) version 13; SS 2.5 Beta 5
    {
        salamander->AddViewer("*.dds", TRUE);
    }
    if (ConfigVersion < 15) // initial support for NEF (so far we handle D70 and D100, hopefully people will send more)
    {
        salamander->AddViewer("*.nef", TRUE);
    }
    if (ConfigVersion < 16) // added the CRW format
    {
        salamander->AddViewer("*.crw", TRUE);
    }

    if (ConfigVersion < 17) // Added EPS/EPT/AI format (Only encapsulated EPS w/ TIFF thumbnail)
    {
        salamander->AddViewer("*.eps;*.ept;*.ai", TRUE);
    }

    if (ConfigVersion < 18) // Added FUJI raw; QuickTime MJPEG PICT video from digital cameras
    {
        salamander->AddViewer("*.raf;*.mov", TRUE);
    }

    if (ConfigVersion < 19) // Added Hemera Photo Objets, see http://www.halley.cc/ed/linux/interop/hemera.html
    {
        salamander->AddViewer("*.hpi", TRUE);
    }

    if (ConfigVersion < 20) // Added Sony, Canon, Adobe Digital Negative, Pentax Digital Camera images
    {
        salamander->AddViewer("*.arw;*.cr2;*.dng;*.pef", TRUE);
    }

    if (ConfigVersion < 21) // Added Olympus Digital Camera images
    {
        salamander->AddViewer("*.orf", TRUE);
    }

    if (ConfigVersion < 22) // Added *.blp textures used by Blizzard Entertainment in World of Warcraft
    {
        salamander->AddViewer("*.blp", TRUE);
    }

    /* used by the export_mnu.py script that generates salmenu.mnu for Translator
   keep synchronized with the salamander->AddMenuItem() calls below...
MENU_TEMPLATE_ITEM PluginMenu[] = 
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_VIEW_BMP_IN_CLIPBOARD
	{MNTT_IT, IDS_PLUGINSMENU_SCREEN_CAPTURE
	{MNTT_IT, IDS_PLUGINSMENU_REGENERATE_THUMBNAIL
	{MNTT_IT, IDS_PLUGINSMENU_SCAN
	{MNTT_IT, IDS_MENU_FILE_SCAN_SOURCE
	{MNTT_PE, 0
};
*/
    salamander->AddMenuItem(-1, LoadStr(IDS_VIEW_BMP_IN_CLIPBOARD), SALHOTKEY('B', HOTKEYF_CONTROL | HOTKEYF_SHIFT),
                            CMD_PASTE, TRUE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, LoadStr(IDS_PLUGINSMENU_SCREEN_CAPTURE), 0, CMD_CAPTURE, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

    salamander->AddMenuItem(-1, LoadStr(IDS_PLUGINSMENU_REGENERATE_THUMBNAIL), 0, CMD_REGENERATE_THUMBNAIL, FALSE,
                            MENU_EVENT_FILE_FOCUSED | MENU_EVENT_FILES_SELECTED, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);

#ifdef ENABLE_TWAIN32
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
#endif                                                                         // ENABLE_TWAIN32

    salamander->AddMenuItem(-1, LoadStr(IDS_PLUGINSMENU_SCAN), 0, CMD_SCAN, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
#ifdef ENABLE_TWAIN32
    salamander->AddMenuItem(-1, LoadStr(IDS_MENU_FILE_SCAN_SOURCE), 0, CMD_SCAN_SOURCE, FALSE,
                            MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
#endif // ENABLE_TWAIN32

    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_PICTVIEW),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    // we can provide thumbnails for these formats:
    salamander->SetThumbnailLoader("*.mng;*.dtx;*.dds;*.nef;*.crw;*.eps;*.ept;*.ai;*.raf;*.mov;*.hpi;"
                                   "*.pntg;*.thumb;*.tiff;*.wbmp;*.mbm;*.ani;*.psp*;*.clk;*.thm;*.zno;"
                                   "*.st;*.cals;*.itiff;*.jfif;*.jpeg;*.macp;*.mpnt;*.paint;*.pict;*.2bp;"
                                   "*.stw;*.sun;*.tga;*.tif;*.udi;*.web;*.wpg;*.xar;*.zbr;*.zmf;*.bw;"
                                   "*.psd;*.pyx;*.qfx;*.ras;*.rgb;*.rle;*.sam;*.scx;*.sep;*.sgi;*.ska;"
                                   "*.pat;*.pbm;*.pc2;*.pcd;*.pct;*.pcx;*.pgm;*.pic;*.png;*.pnm;*.ppm;"
                                   "*.jff;*.jif;*.jmx;*.jpe;*.jpg;*.lbm;*.mac;*.mil;*.msp;*.ofx;*.pan;"
                                   "*.flc;*.fli;*.gem;*.gif;*.ham;*.hmr;*.hrz;*.icn;*.ico;*.iff;*.img;"
                                   "*.cdt;*.cel;*.clp;*.cit;*.cmx;*.cot;*.cpt;*.cur;*.cut;*.dcx;*.dib;"
                                   "*.82i;*.83i;*.85i;*.86i;*.89i;*.92i;*.awd;*.bmi;*.bmp;*.cal;*.cdr;"
                                   "*.arw;*.blp;*.cr2;*.dng;*.orf;*.pef");
}

void CPluginInterface::ClearHistory(HWND parent)
{
    int i;
    for (i = 0; i < FILES_HISTORY_SIZE; i++)
    {
        if (G.FilesHistory[i] != NULL)
        {
            free(G.FilesHistory[i]);
            G.FilesHistory[i] = NULL;
        }
    }

    int j;
    for (j = 0; j < DIRS_HISTORY_SIZE; j++)
    {
        if (G.DirsHistory[j] != NULL)
        {
            free(G.DirsHistory[j]);
            G.DirsHistory[j] = NULL;
        }
    }
}

void CPluginInterface::Event(int event, DWORD param)
{
    switch (event)
    {
    case PLUGINEVENT_COLORSCHANGED:
        InitGlobalGUIParameters();
        ViewerWindowQueue.BroadcastMessage(WM_USER_VIEWERCFGCHNG, 0, 0);
        break;

    case PLUGINEVENT_SETTINGCHANGE:
        ViewerWindowQueue.BroadcastMessage(WM_USER_SETTINGCHANGE, 0, 0);
        break;

    case PLUGINEVENT_CONFIGURATIONCHANGED:
        // Cache the value for use in CRenameDialog
        SalamanderGeneral->GetConfigParameter(SALCFG_SELECTWHOLENAME, &G.bSelectWhole, sizeof(BOOL), NULL);
        break;
    }
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    return &InterfaceForMenuExt;
}

CPluginInterfaceForThumbLoaderAbstract*
CPluginInterface::GetInterfaceForThumbLoader()
{
    return &InterfaceForThumbLoader;
}

// ****************************************************************************
// MENU SECTION
// ****************************************************************************

DWORD
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    if (id == CMD_PASTE)
    {
        return (IsClipboardFormatAvailable(CF_BITMAP) ? MENU_ITEM_STATE_ENABLED : 0);
    }
    return 0;
}

BOOL CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                                 HWND hParent, int id, DWORD eventMask)
{
    RECT r;
    LPCTSTR name = NULL;
    int nCmdShow = SW_SHOW;

    GetWindowRect(hParent, &r);
    if (IsZoomed(hParent))
    {
        nCmdShow = SW_MAXIMIZE;
    }
    switch (id)
    {
    case CMD_PASTE:
        name = CLIPBOARD;
        break;
    case CMD_CAPTURE:
    {
        CCaptureDialog dlg(hParent);
        if (dlg.Execute() == IDOK)
            name = CAPTURE;
        break;
    }
    case CMD_SCAN:
        name = SCAN;
        break;
#ifdef ENABLE_TWAIN32
    case CMD_SCAN_SOURCE:
        name = SCAN_SOURCE;
        break;
#endif // ENABLE_TWAIN32

    case CMD_INTERNAL_FOCUS:
    {
        TCHAR focusPath[MAX_PATH];
        lstrcpyn(focusPath, Focus_Path, SizeOf(focusPath));
        Focus_Path[0] = 0;
        if (focusPath[0] != 0) // only if we were lucky (we did not hit the start of Salamander's BUSY mode)
        {
            LPTSTR fname;
            if (SalamanderGeneral->CutDirectory(focusPath, &fname))
            {
                SalamanderGeneral->SkipOneActivateRefresh(); // the main window will not refresh when switching from the viewer
                SalamanderGeneral->FocusNameInPanel(PANEL_SOURCE, focusPath, fname);
            }
        }
        return TRUE;
    }

    case CMD_INTERNAL_SAVEAS:
    {
        TCHAR panelPath[MAX_PATH];
        HWND hWindow = ghSaveAsWindow;

        ghSaveAsWindow = NULL;

        if (!SalamanderGeneral->GetLastWindowsPanelPath(PANEL_SOURCE, panelPath, SizeOf(panelPath)))
        {
            if (!SalamanderGeneral->GetLastWindowsPanelPath(PANEL_TARGET, panelPath, SizeOf(panelPath)))
            {
                // Failure -> use the stored directory
                panelPath[0] = 0;
            }
        }
        PostMessage(hWindow, WM_USER_SAVEAS_INTERNAL, 0, panelPath[0] ? (LPARAM)_tcsdup(panelPath) : 0);
        return TRUE;
    }

    case CMD_REGENERATE_THUMBNAIL:
        UpdateThumbnails(salamander);
        break;

        /*  // Petr: I commented this out; hopefully it will never be needed again...
    case CMD_INTERNAL_REFRESHLPANEL:
    case CMD_INTERNAL_REFRESHRPANEL:
    {
      int panel = id == CMD_INTERNAL_REFRESHLPANEL ? PANEL_LEFT : PANEL_RIGHT;
      SalamanderGeneral->RefreshPanelPath(panel);
      break;
    }
*/
    case CMD_INTERNAL_REREADTHUMBS:
    {
        int leftType, rightType;
        SalamanderGeneral->GetPanelPath(PANEL_LEFT, NULL, 0, &leftType, NULL);
        SalamanderGeneral->GetPanelPath(PANEL_RIGHT, NULL, 0, &rightType, NULL);
        if (leftType == PATH_TYPE_WINDOWS)
            SalamanderGeneral->RefreshPanelPath(PANEL_LEFT, TRUE /* we want to reload thumbnails */);
        if (rightType == PATH_TYPE_WINDOWS)
            SalamanderGeneral->RefreshPanelPath(PANEL_RIGHT, TRUE /* we want to reload thumbnails */);
        break;
    }

    case CMD_INTERNAL_SCAN_EXTRA_IMAGES:
    {
        while (ExtraScanImagesToOpen.HaveNextImage())
        {
            if (!ExtraScanImagesToOpen.LockImages())
                return TRUE;

            BOOL alwaysOnTop = FALSE;
            SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &alwaysOnTop, sizeof(alwaysOnTop), NULL);
            while (ExtraScanImagesToOpen.HaveNextImage())
            {
                InterfaceForViewer.ViewFile(SCANEXTRA, r.left, r.top, r.right - r.left, r.bottom - r.top,
                                            nCmdShow, alwaysOnTop, FALSE /*returnLock*/, NULL /* *lock*/,
                                            NULL /*BOOL *lockOwner*/, NULL /* *viewerData*/, -1, -1);
                Sleep(200); // give the newly opened viewer some time to load the image
            }

            ExtraScanImagesToOpen.UnlockImages();
        }
        return TRUE;
    }
    }

    if (name != NULL)
    {
        BOOL alwaysOnTop = FALSE;
        SalamanderGeneral->GetConfigParameter(SALCFG_ALWAYSONTOP, &alwaysOnTop, sizeof(alwaysOnTop), NULL);
        InterfaceForViewer.ViewFile(name, r.left, r.top, r.right - r.left, r.bottom - r.top,
                                    nCmdShow, alwaysOnTop, FALSE /*returnLock*/, NULL /* *lock*/,
                                    NULL /*BOOL *lockOwner*/, NULL /* *viewerData*/, -1, -1);
    }
    return FALSE;
}

BOOL CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case CMD_PASTE:
        helpID = IDH_VIEWBMPFROMCLIP;
        break;
    case CMD_CAPTURE:
        helpID = IDH_SCREENCAPTURE;
        break;
    case CMD_REGENERATE_THUMBNAIL:
        helpID = IDH_UPDATETHUMBNAIL;
        break;
    case CMD_SCAN:
        helpID = IDH_SCANIMAGE;
        break;
#ifdef ENABLE_TWAIN32
    case CMD_SCAN_SOURCE:
        helpID = IDH_SELSCANNERSOURCE;
        break;
#endif // ENABLE_TWAIN32
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

// ****************************************************************************
// VIEWER SECTION
// ****************************************************************************

void WINAPI HTMLHelpCallback(HWND hWindow, UINT helpID)
{
    SalamanderGeneral->OpenHtmlHelp(hWindow, HHCDisplayContext, helpID, FALSE);
}

BOOL LoadPictViewDll(HWND hParentWnd)
{
    TCHAR path[_MAX_PATH];

    if (!GetModuleFileName(DLLInstance, path, SizeOf(path)))
    {
        TRACE_E("GetModuleFileName failed");
        return FALSE;
    }
    _tcsrchr(path, '\\')[0] = 0;
#ifndef PICTVIEW_DLL_IN_SEPARATE_PROCESS
    _tcscat(path, _T("\\PVW32Cnv.dll"));
    PVW32DLL.Handle = LoadLibrary(path); // load PVW32Cnv.dll
    if (!PVW32DLL.Handle)
    {
        TRACE_E("LoadLibrary(PVW32Cnv.dll) failed");
        SalamanderGeneral->SalMessageBox(hParentWnd, LoadStr(IDS_DLL_NOTFOUND),
                                         LoadStr(IDS_ERRORTITLE), MB_ICONSTOP | MB_OK);
        return FALSE;
    }
    PVW32DLL.PVReadImage2 = (TPVReadImage2)GetProcAddress(PVW32DLL.Handle, "PVReadImage2");
    PVW32DLL.PVCloseImage = (TPVCloseImage)GetProcAddress(PVW32DLL.Handle, "PVCloseImage");
    PVW32DLL.PVDrawImage = (TPVDrawImage)GetProcAddress(PVW32DLL.Handle, "PVDrawImage");
    PVW32DLL.PVGetErrorText = (TPVGetErrorText)GetProcAddress(PVW32DLL.Handle, "PVGetErrorText");
    PVW32DLL.PVOpenImageEx = (TPVOpenImageEx)GetProcAddress(PVW32DLL.Handle, "PVOpenImageEx");
    PVW32DLL.PVSetBkHandle = (TPVSetBkHandle)GetProcAddress(PVW32DLL.Handle, "PVSetBkHandle");
    PVW32DLL.PVGetDLLVersion = (TPVGetDLLVersion)GetProcAddress(PVW32DLL.Handle, "PVGetDLLVersion");
    PVW32DLL.PVSetStretchParameters = (TPVSetStretchParameters)GetProcAddress(PVW32DLL.Handle, "PVSetStretchParameters");
    PVW32DLL.PVLoadFromClipboard = (TPVLoadFromClipboard)GetProcAddress(PVW32DLL.Handle, "PVLoadFromClipboard");
    PVW32DLL.PVGetImageInfo = (TPVGetImageInfo)GetProcAddress(PVW32DLL.Handle, "PVGetImageInfo");
    PVW32DLL.PVSetParam = (TPVSetParam)GetProcAddress(PVW32DLL.Handle, "PVSetParam");
    PVW32DLL.PVGetHandles2 = (TPVGetHandles2)GetProcAddress(PVW32DLL.Handle, "PVGetHandles2");
    PVW32DLL.PVSaveImage = (TPVSaveImage)GetProcAddress(PVW32DLL.Handle, "PVSaveImage");
    PVW32DLL.PVChangeImage = (TPVChangeImage)GetProcAddress(PVW32DLL.Handle, "PVChangeImage");
    PVW32DLL.PVIsOutCombSupported = (TPVIsOutCombSupported)GetProcAddress(PVW32DLL.Handle, "PVIsOutCombSupported");
    PVW32DLL.PVReadImageSequence = (TPVReadImageSequence)GetProcAddress(PVW32DLL.Handle, "PVReadImageSequence");
    PVW32DLL.PVCropImage = (TPVCropImage)GetProcAddress(PVW32DLL.Handle, "PVCropImage");
    PVW32DLL.GetRGBAtCursor = GetRGBAtCursor;
    PVW32DLL.CalculateHistogram = CalculateHistogram;
    PVW32DLL.CreateThumbnail = CreateThumbnail;
    PVW32DLL.SimplifyImageSequence = SimplifyImageSequence;

    if (!PVW32DLL.PVReadImage2 || !PVW32DLL.PVIsOutCombSupported || !PVW32DLL.PVChangeImage || !PVW32DLL.PVSetParam || !PVW32DLL.PVGetHandles2 || !PVW32DLL.PVCropImage)
    {
        TRACE_E("PVW32Cnv was not compiled for Salamander or an old version was found");
        SalamanderGeneral->SalMessageBox(hParentWnd, LoadStr(IDS_DLL_WRONG_VERSION),
                                         LoadStr(IDS_ERRORTITLE), MB_ICONSTOP | MB_OK);
        return FALSE;
    }
#else  // PICTVIEW_DLL_IN_SEPARATE_PROCESS
    if (!InitPVEXEWrapper(hParentWnd, path))
    {
        return FALSE;
    }
#endif // PICTVIEW_DLL_IN_SEPARATE_PROCESS
    return TRUE;
}

BOOL InitViewer(HWND hParentWnd)
{
    int i;

    // initialize global variables
    memset(&G, sizeof(G), 0);
    G.ZoomType = eShrinkToFit;
    G.PageDnUpScrolls = TRUE;
    //  G.IgnoreThumbnails = FALSE;
    G.AutoRotate = TRUE;
    //G.PipetteInHex = FALSE;
    G.MaxThumbImgSize = PV_MAX_IMG_SIZE_TO_THUMBNAIL;
    G.WindowPos = WINDOW_POS_SAME;
    G.ToolbarVisible = TRUE;
    G.StatusbarVisible = TRUE;
    //  G.LastCfgPage = 0;
    G.CaptureScope = CAPTURE_SCOPE_DESKTOP;
    G.CaptureTrigger = CAPTURE_TRIGGER_HOTKEY;
    G.CaptureHotKey = VK_F10;
    G.CaptureTimer = 5;
    G.CaptureCursor = FALSE;
    G.LastSaveAsFilterIndexMono = 14;  // index is 1-based (IDS_SAVEASFILTERMONO: 14=BMP)
    G.LastSaveAsFilterIndexColor = 13; // index is 1-based (IDS_SAVEASFILTERCOLOR: 13=BMP)
    G.bShowPathInTitle = TRUE;
    G.Save.Flags = PVSF_GIF89;
    G.Save.JPEGQuality = 75;
    G.Save.JPEGSubsampling = 1;
    G.Save.TIFFStripSize = 64;
    G.Save.InitDir[0] = 0;
    G.SelectRatioX = G.SelectRatioY = 1;
    G.bSelectWhole = TRUE; // Assume failure of GetConfigParameter ;-)
    SalamanderGeneral->GetConfigParameter(SALCFG_SELECTWHOLENAME, &G.bSelectWhole, sizeof(BOOL), NULL);

    for (i = 0; i < vceCount; i++)
        G.Colors[i] = RGBF(0, 0, 0, SCF_DEFAULT);
    InitGlobalGUIParameters();
    InitializeCriticalSection(&G.CS);

    if (!LoadPictViewDll(hParentWnd))
    {
        DeleteCriticalSection(&G.CS);
        return FALSE;
    }
    i = PVW32DLL.PVGetDLLVersion();

    sprintf(PVW32DLL.Version, "PVW32Cnv.dll %d.%#02d.%d", i >> 16, i & 255, (i >> 8) & 255);

    PVW32DLL.PVSetParam(GetExtText);

    G.HAccel = LoadAccelerators(DLLInstance, MAKEINTRESOURCE(IDA_ACCELERATORS));

    WNDCLASS wc;
    wc.style = 0;
    wc.lpfnWndProc = ToolTipWindowProc;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    wc.hInstance = DLLInstance;
    wc.hIcon = 0;
    wc.hCursor = 0;
    wc.hbrBackground = 0; //(HBRUSH)(COLOR_WINDOW + 1);
    wc.lpszMenuName = NULL;
    wc.lpszClassName = TIP_WINDOW_CLASSNAME;
    if (RegisterClass(&wc) == 0)
    {
        TRACE_E("RegisterClass has failed");
        DeleteCriticalSection(&G.CS);
        return FALSE;
    }

    if (!InitializeWinLib(PLUGIN_NAME_EN, DLLInstance))
    {
        if (!UnregisterClass(TIP_WINDOW_CLASSNAME, DLLInstance))
            TRACE_E("UnregisterClass(TIP_WINDOW_CLASSNAME) has failed");
        DeleteCriticalSection(&G.CS);
        return FALSE;
    }
    SetWinLibStrings(_T("Invalid number!"), LoadStr(IDS_PLUGINNAME));
    SetupWinLibHelp(HTMLHelpCallback);

    INITCOMMONCONTROLSEX initCtrls;
    initCtrls.dwSize = sizeof(INITCOMMONCONTROLSEX);
    initCtrls.dwICC = ICC_BAR_CLASSES;
    if (!InitCommonControlsEx(&initCtrls))
        TRACE_E("InitCommonControlsEx failed");

    return TRUE;
}

BOOL InitEXIF(HWND hParent, BOOL bSilent)
{
    if (EXIFLibrary != NULL)
        return TRUE;

    TCHAR path[MAX_PATH];
    EXIFINITTRANSLATIONS initTransl;

    GetModuleFileName(DLLInstance, path, SizeOf(path));
    _tcscpy((LPTSTR)_tcsrchr(path, '\\') + 1, _T("exif.dll"));
    EXIFLibrary = LoadLibrary(path); // load EXIF.DLL
    if (EXIFLibrary == NULL)
    {
        if (!bSilent)
        {
            TCHAR errText[2 * MAX_PATH];
            _stprintf(errText, LoadStr(IDS_LOADEXIF), path);
            SalamanderGeneral->SalMessageBox(hParent, errText, LoadStr(IDS_PLUGINNAME),
                                             MB_ICONEXCLAMATION | MB_OK);
        }
        return FALSE;
    }
    SalamanderDebug->AddModuleWithPossibleMemoryLeaks(path);
    initTransl = (EXIFINITTRANSLATIONS)GetProcAddress(EXIFLibrary, "EXIFInitTranslations");
    if (initTransl)
    {
        TCHAR name[32];

        lstrcpyn(name, LoadStr(IDS_EXIF_LOCALIZATION_FNAME), SizeOf(name));
        if (*name && _tcscmp(name, _T("ERROR LOADING STRING")))
        {
            _stprintf((LPTSTR)_tcsrchr(path, '\\') + 1, _T("lang\\exif\\%s"), name);
            initTransl(path);
        }
    }

    return TRUE;
}

void ReleaseViewer()
{
#ifdef PICTVIEW_DLL_IN_SEPARATE_PROCESS
    ReleasePVEXEWrapper();
#endif
    if (!UnregisterClass(TIP_WINDOW_CLASSNAME, DLLInstance))
        TRACE_E("UnregisterClass(TIP_WINDOW_CLASSNAME) has failed");
    ReleaseWinLib(DLLInstance);
    int i;
    for (i = 0; i < FILES_HISTORY_SIZE; i++)
    {
        if (G.FilesHistory[i] != NULL)
        {
            free(G.FilesHistory[i]);
            G.FilesHistory[i] = NULL;
        }
    }
    int j;
    for (j = 0; j < DIRS_HISTORY_SIZE; j++)
    {
        if (G.DirsHistory[j] != NULL)
        {
            free(G.DirsHistory[j]);
            G.DirsHistory[j] = NULL;
        }
    }
    int k;
    for (k = 0; k < COPYTO_LINES; k++)
    {
        if (G.CopyToDestinations[k] != NULL)
        {
            SalamanderGeneral->Free(G.CopyToDestinations[k]);
            G.CopyToDestinations[k] = NULL;
        }
    }
    DeleteCriticalSection(&G.CS);
    if (G.pDevNames)
    {
        free(G.pDevNames);
        G.pDevNames = NULL;
        G.DevNamesSize = 0;
    }
    if (G.pDevMode)
    {
        free(G.pDevMode);
        G.pDevMode = NULL;
        G.DevModeSize = 0;
    }
}

class CViewerThread : public CThread
{
protected:
    TCHAR Name[MAX_PATH];
    int Left, Top, Width, Height;
    UINT ShowCmd;
    BOOL AlwaysOnTop;
    BOOL ReturnLock;

    HANDLE Continue; // once the following return values are filled, this event switches to "signaled"
    HANDLE* Lock;
    BOOL* LockOwner;
    BOOL* Success;

    int EnumFilesSourceUID;    // source UID for enumerating files in the viewer
    int EnumFilesCurrentIndex; // index of the first file in the viewer within the source

public:
    CViewerThread(LPCTSTR name, int left, int top, int width, int height,
                  UINT showCmd, BOOL alwaysOnTop, BOOL returnLock,
                  HANDLE* lock, BOOL* lockOwner, HANDLE contEvent,
                  BOOL* success, int enumFilesSourceUID,
                  int enumFilesCurrentIndex) : CThread(PLUGIN_NAME_EN)
    {
        lstrcpyn(Name, name, MAX_PATH);
        Left = left;
        Top = top;
        Width = width;
        Height = height;
        ShowCmd = showCmd;
        AlwaysOnTop = alwaysOnTop;
        ReturnLock = returnLock;

        Continue = contEvent;
        Lock = lock;
        LockOwner = lockOwner;
        Success = success;

        EnumFilesSourceUID = enumFilesSourceUID;
        EnumFilesCurrentIndex = enumFilesCurrentIndex;
    }

    virtual unsigned Body();
};

/*
unsigned WINAPI ViewerThreadBody(void *param)
{
  CALL_STACK_MESSAGE3(_T("ViewerThreadBody() PictView.spl %s %hs"), VERSINFO_VERSION, PVW32DLL.Version);
  SetThreadNameInVCAndTrace(PLUGIN_NAME_EN);
  TRACE_I("Begin");
  RECT    r;
  CTVData *data = (CTVData *)param;

  CViewerWindow *window = new CViewerWindow;
  if (window != NULL)
  {
    r.left = data->Left; r.top = data->Top;
    r.right = data->Width; r.bottom = data->Height;
    if (data->ReturnLock)
    {
      *data->Lock = window->GetLock();
      *data->LockOwner = TRUE;
    }
    CALL_STACK_MESSAGE1("ViewerThreadBody::CreateWindowEx");
    if ((!data->ReturnLock || *data->Lock != NULL) &&
        window->CreateEx(data->AlwaysOnTop ? WS_EX_TOPMOST : 0,
                         CWINDOW_CLASSNAME2,
                         LoadStr(IDS_PLUGINNAME),
                         WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                         data->Left,
                         data->Top,
                         data->Width,
                         data->Height,
                         NULL,
                         NULL,
                         DLLInstance,
                         window) != NULL)
    {
      // WARNING! icons obtained here must be destroyed in WM_DESTROY
      SendMessage(window->HWindow, WM_SETICON, ICON_BIG,
                  (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_WINDOW_ICON)));
      SendMessage(window->HWindow, WM_SETICON, ICON_SMALL,
                  (LPARAM)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_WINDOW_ICON),
                  IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR));

//    ShowWindow(window->HWindow, data->ShowCmd);
//    SetForegroundWindow(window->HWindow);
//    UpdateWindow(window->HWindow);
      data->Success = TRUE;
    }
    else
    {
      DWORD err = GetLastError();
      CALL_STACK_MESSAGE1("ViewerThreadBody::delete-window");
      if (data->ReturnLock && *data->Lock != NULL) CloseHandle(*data->Lock);
    }
  }

  CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
  char name[MAX_PATH];
  BOOL openFile = data->Success;
  int  ShowCmd = data->ShowCmd;

  strcpy(name, data->Name);
  SetEvent(data->Continue);    // let the main thread continue; from this point the data are invalid (=NULL)
  data = NULL;

  // if everything succeeded, open the requested file in the window
  if (openFile)
  {
    CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");

    CALL_STACK_MESSAGE1("ViewerThreadBody::OpenFile");

    if (strcmp(name, CLIPBOARD) != 0 && strcmp(name, CAPTURE) != 0)
    {
      window->Renderer.OpenFile(name, ShowCmd, NULL);
    }
    else
    {
      if (strcmp(name, CLIPBOARD) == 0)
      {
        ShowWindow(window->HWindow, ShowCmd);
        SetForegroundWindow(window->HWindow);
        UpdateWindow(window->HWindow);
        PostMessage(window->HWindow, WM_COMMAND, CMD_PASTE, 0);
      }
      else
      {
        PostMessage(window->HWindow, WM_COMMAND, CMD_CAPTURE_INTERNAL, 0);
      }
    }

    CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
    // message loop
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0))
    {
      if (
#ifdef ENABLE_TWAIN32
          (window->Twain == NULL || !window->Twain->IsTwainMessage(&msg)) &&
#endif // ENABLE_TWAIN32
          (!window->IsMenuBarMessage(&msg)) &&
          (!TranslateAccelerator(window->HWindow, G.HAccel, &msg)))
      {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
      }
    }
  }
  if (window != NULL)
    delete window;

  TRACE_I("End");
  return 0;
}

unsigned __stdcall ViewerThread(void *param)
{
  return SalamanderDebug->CallWithCallStack(ViewerThreadBody, param);
}
*/

unsigned
CViewerThread::Body()
{
    CALL_STACK_MESSAGE3(_T("ViewerThreadBody() PictView.spl %s %hs"), VERSINFO_VERSION, PVW32DLL.Version);
    SetThreadNameInVCAndTrace(PLUGIN_NAME_EN);
    TRACE_I("Begin");

    CViewerWindow* window = new CViewerWindow(EnumFilesSourceUID, EnumFilesCurrentIndex, AlwaysOnTop);
    if (window != NULL)
    {
        if (ReturnLock)
        {
            *Lock = window->GetLock();
            *LockOwner = TRUE;
        }
        CALL_STACK_MESSAGE1("ViewerThreadBody::CreateWindowEx");
        if (!ReturnLock || *Lock != NULL)
        {
            // NOTE: on an existing window/dialog the top-most state can be set easily:
            //   SetWindowPos(HWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);

            if (window->CreateEx(AlwaysOnTop ? WS_EX_TOPMOST : 0,
                                 CWINDOW_CLASSNAME2,
                                 LoadStr(IDS_PLUGINNAME),
                                 WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
                                 Left,
                                 Top,
                                 Width,
                                 Height,
                                 NULL,
                                 NULL,
                                 DLLInstance,
                                 window) != NULL)
            {
                CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");

                // WARNING! icons obtained here must be destroyed in WM_DESTROY
                SendMessage(window->HWindow, WM_SETICON, ICON_BIG,
                            (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_WINDOW_ICON)));
                SendMessage(window->HWindow, WM_SETICON, ICON_SMALL,
                            (LPARAM)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_WINDOW_ICON),
                                              IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags()));

                //        ShowWindow(window->HWindow, ShowCmd);
                //        SetForegroundWindow(window->HWindow);
                //        UpdateWindow(window->HWindow);
                *Success = TRUE;
            }
            else
            {
                if (ReturnLock && *Lock != NULL)
                    CloseHandle(*Lock);
            }
        }
    }

    CALL_STACK_MESSAGE1("ViewerThreadBody::SetEvent");
    BOOL openFile = *Success;
    // before letting the main thread continue, optionally take over an image from the scanner to open in the viewer
    HBITMAP scanExtraImg = _tcscmp(Name, SCANEXTRA) == 0 ? ExtraScanImagesToOpen.GiveNextImage() : NULL;
    SetEvent(Continue); // let the main thread continue; from this point the following variables are no longer valid:
    Continue = NULL;    // clearing is unnecessary, just for clarity
    Lock = NULL;        // clearing is unnecessary, just for clarity
    LockOwner = NULL;   // clearing is unnecessary, just for clarity
    Success = NULL;     // clearing is unnecessary, just for clarity

    // if everything succeeded, open the requested file in the window
    if (openFile)
    {
        CALL_STACK_MESSAGE1("ViewerThreadBody::ShowWindow");

        if (_tcscmp(Name, CLIPBOARD) != 0 && _tcscmp(Name, CAPTURE) != 0 &&
            _tcscmp(Name, SCAN) != 0 && _tcscmp(Name, SCANEXTRA) != 0
#ifdef ENABLE_TWAIN32
            && _tcscmp(Name, SCAN_SOURCE) != 0
#endif // ENABLE_TWAIN32
        )
        {
            window->Renderer.OpenFile(Name, ShowCmd, NULL);
        }
        else
        {
            if (_tcscmp(Name, CLIPBOARD) == 0)
            {
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                PostMessage(window->HWindow, WM_COMMAND, CMD_PASTE, 0);
            }
            if (_tcscmp(Name, CAPTURE) == 0)
            {
                PostMessage(window->HWindow, WM_COMMAND, CMD_CAPTURE_INTERNAL, 0);
            }
            if (_tcscmp(Name, SCAN) == 0)
            {
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                PostMessage(window->HWindow, WM_COMMAND, CMD_SCAN, 0);
            }
#ifdef ENABLE_TWAIN32
            if (_tcscmp(Name, SCAN_SOURCE) == 0)
            {
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                PostMessage(window->HWindow, WM_COMMAND, CMD_SCAN_SOURCE, 0);
            }
#endif // ENABLE_TWAIN32
            if (_tcscmp(Name, SCANEXTRA) == 0)
            {
                ShowWindow(window->HWindow, ShowCmd);
                SetForegroundWindow(window->HWindow);
                UpdateWindow(window->HWindow);
                if (scanExtraImg != NULL)
                    window->OpenScannedImage(scanExtraImg);
            }
        }

        CALL_STACK_MESSAGE1("ViewerThreadBody::message-loop");
        // message loop
        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0))
        {
            if (
#ifdef ENABLE_TWAIN32
                (window->Twain == NULL || !window->Twain->IsTwainMessage(&msg)) &&
#endif                                                                       // ENABLE_TWAIN32
                (GetCapture() != NULL || !window->IsMenuBarMessage(&msg)) && // disable Alt menu access when dragging the cage (captured cursor)
                (!TranslateAccelerator(window->HWindow, G.HAccel, &msg)))
            {
                TranslateMessage(&msg);
                DispatchMessage(&msg);
            }
        }
    }
    else
    {
        if (scanExtraImg != NULL)
            DeleteObject(scanExtraImg);
    }
    if (window != NULL)
        delete window;

    TRACE_I("End");
    return 0;
}

BOOL CPluginInterfaceForViewer::ViewFile(LPCTSTR name, int left, int top, int width, int height,
                                         UINT showCmd, BOOL alwaysOnTop, BOOL returnLock, HANDLE* lock,
                                         BOOL* lockOwner, CSalamanderPluginViewerData* viewerData,
                                         int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    HANDLE contEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
    if (contEvent == NULL)
    {
        TRACE_E("Nepodarilo se vytvorit Continue event.");
        return FALSE;
    }

    // viewerData are not used in PictView; otherwise values would need to be passed (not by reference)
    // to the viewer thread...
    BOOL success = FALSE;
    CViewerThread* t = new CViewerThread(name, left, top, width, height,
                                         showCmd, alwaysOnTop, returnLock, lock,
                                         lockOwner, contEvent, &success,
                                         enumFilesSourceUID, enumFilesCurrentIndex);
    BOOL failed = TRUE;
    if (t != NULL)
    {
        if (t->Create(ThreadQueue) != NULL) // the thread started
        {
            t = NULL;                                 // unnecessary nulling, just for order (the pointer may already be deallocated)
            WaitForSingleObject(contEvent, INFINITE); // wait until the thread processes the data and returns the results
            failed = FALSE;
        }
        else
            delete t; // on failure the thread object must be deallocated
    }
    if (failed)
    {
        // before letting the main thread continue, dispose of the scanner image queued for opening
        // in the viewer (prevents an infinite loop)
        HBITMAP scanExtraImg = _tcscmp(name, SCANEXTRA) == 0 ? ExtraScanImagesToOpen.GiveNextImage() : NULL;
        if (scanExtraImg != NULL)
            DeleteObject(scanExtraImg);
    }
    CloseHandle(contEvent);
    return success;
}

BOOL CPluginInterfaceForViewer::CanViewFile(LPCTSTR name)
{
    // We do quick check for files with suffixes that we now can collided
    // with other formats supported by other viewers
    LPCTSTR ext = _tcsrchr(name, '.'); // ".cvspass" is extension in Windows
    BOOL bTest = FALSE;

    // JR: what does PVW32DLL.PVOpenImageEx do? Does it load the entire image or just some headers?
    // JP: only headers
    // JR: is there any reason not to try every file (regardless of the suffix) and if
    //     it does not look right, pass it to another viewer? If there is no other,
    //     it ends up in the internal viewer
    // JP: I would rather not; when the primary format is not recognized it goes
    //     through detection for a number of formats. Moreover with TIFF, CDR, CMX it scans the entire
    //     file, with JPEG repeatedly to detect a preview....
    if (ext && (!_tcsicmp(ext, _T(".SCR")) || !_tcsicmp(ext, _T(".PCT")) || !_tcsicmp(ext, _T(".PIC")) ||
                !_tcsicmp(ext, _T(".PICT")) || !_tcsicmp(ext, _T(".IMG")) ||
                !_tcsicmp(ext, _T(".EPS")) || !_tcsicmp(ext, _T(".EPT")) || !_tcsicmp(ext, _T(".AI")) ||
                !_tcsicmp(ext, _T(".MOV")) || !_tcsicmp(ext, _T(".MSP")) ||
                !_tcsicmp(ext, _T(".CDR")) || !_tcsicmp(ext, _T(".CDT")) ||
                !_tcsicmp(ext, _T(".SEP"))))
    {
        // SCR: Screen Saver EXE files supported by PEViewer
        // PCT, PIC, PICT: MacIntosh PICT is supported by Eroiica Viewer as well.
        //    EV handles vector objects, however, it does not handle HiColor images
        // IMG: ISO image supported by UnISO plugin
        // EPS/EPT/AI: Supported only if it contains a thumbnail
        // MOV: Supported only if MJPEG from a digital camera, not a QuickTime Movie
        // MSP: Support MS Paint, not MSI.Patch installer packages (Compound files in fact)
        // CDR: CorelDraw! 14+ is an OpenOffice-like ZIP, handled by EV)
        // SEP: Accept SEP files being actually TIFFs, refuse DjVu SEP files (what is it?)
        bTest = TRUE;
    }
    if (!bTest)
    {
        ext = _tcschr(name, '\\');
        if (ext)
        {
            ext = _tcschr(ext, '.');
            if (ext && !_tcsnicmp(ext, _T(".PSP"), 4))
            {
                // We are associated to wild suffix *.PSP*: always perform test if the suffix starts with .PSP
                bTest = TRUE;
            }
        }
    }
    if (bTest)
    {
        LPPVHandle PVHandle;
        PVOpenImageExInfo oiei;
        PVImageInfo pvii;
        PVCODE code;

        memset(&oiei, 0, sizeof(oiei));
        oiei.cbSize = sizeof(oiei);
#ifdef _UNICODE
        char nameA[_MAX_PATH];

        WideCharToMultiByte(CP_ACP, 0, name, -1, nameA, sizeof(nameA), NULL, NULL);
        nameA[sizeof(nameA) - 1] = 0;
        oiei.FileName = nameA;
#else
        oiei.FileName = name;
#endif

        code = PVW32DLL.PVOpenImageEx(&PVHandle, &oiei, &pvii, sizeof(pvii));
        if (code != PVC_OK)
        {
            return FALSE;
        }
        PVW32DLL.PVCloseImage(PVHandle);
    }
    return TRUE;
}

//
// ****************************************************************************
// CExtraScanImagesToOpen
//

CExtraScanImagesToOpen::CExtraScanImagesToOpen() : AllExtraScanImages(10, 30)
{
    InitializeCriticalSection(&LockCS);
    InitializeCriticalSection(&AESI_CS);
    Locked = FALSE;
}

CExtraScanImagesToOpen::~CExtraScanImagesToOpen()
{
    for (int i = 0; i < AllExtraScanImages.Count; i++)
        DeleteObject(AllExtraScanImages[i]);
    AllExtraScanImages.DestroyMembers();

    DeleteCriticalSection(&AESI_CS);
    DeleteCriticalSection(&LockCS);
}

BOOL CExtraScanImagesToOpen::LockImages()
{
    EnterCriticalSection(&LockCS);
    BOOL ret = FALSE;
    if (!Locked)
        Locked = ret = TRUE;
    LeaveCriticalSection(&LockCS);
    return ret;
}

void CExtraScanImagesToOpen::UnlockImages()
{
    EnterCriticalSection(&LockCS);
    if (!Locked)
        TRACE_C("CExtraScanImagesToOpen::UnlockImages(): invalid call: not locked!");
    Locked = FALSE;
    LeaveCriticalSection(&LockCS);
}

void CExtraScanImagesToOpen::AddImages(TDirectArray<HBITMAP>* newImgs)
{
    EnterCriticalSection(&AESI_CS);
    if (newImgs != NULL)
        AllExtraScanImages.Add(newImgs->GetData(), newImgs->Count);
    LeaveCriticalSection(&AESI_CS);
}

BOOL CExtraScanImagesToOpen::HaveNextImage()
{
    EnterCriticalSection(&AESI_CS);
    BOOL ret = AllExtraScanImages.Count > 0;
    LeaveCriticalSection(&AESI_CS);
    return ret;
}

HBITMAP
CExtraScanImagesToOpen::GiveNextImage()
{
    HBITMAP img = NULL;
    EnterCriticalSection(&AESI_CS);
    if (AllExtraScanImages.Count > 0)
    {
        img = AllExtraScanImages[0];
        AllExtraScanImages.Delete(0);
    }
    LeaveCriticalSection(&AESI_CS);
    return img;
}

//
// ****************************************************************************
// CViewerWindow
//

#define BANDID_MENU 1
#define BANDID_TOOLBAR 2

CViewerWindow::CViewerWindow(int enumFilesSourceUID, int enumFilesCurrentIndex, BOOL alwaysOnTop)
    : CWindow(ooStatic), Renderer(enumFilesSourceUID, enumFilesCurrentIndex)
{
    ExtraScanImages = NULL;
    AlwaysOnTop = alwaysOnTop;
    HHistogramWindow = NULL;
    Lock = NULL;
    Renderer.Viewer = this;
    HRebar = NULL;
    MainMenu = NULL;
    MenuBar = NULL;
    ToolBar = NULL;
    StatusBar = NULL;
#ifdef ENABLE_WIA
    WiaWrap = NULL;
#endif // ENABLE_WIA
#ifdef ENABLE_TWAIN32
    Twain = NULL;
#endif // ENABLE_TWAIN32
    HGrayToolBarImageList = NULL;
    HHotToolBarImageList = NULL;
    FullScreen = FALSE;
    ZeroMemory(Enablers, sizeof(Enablers));
    IsSrcFileSelected = FALSE;
}

BOOL CViewerWindow::IsMenuBarMessage(CONST MSG* lpMsg)
{
    if (MenuBar == NULL)
        return FALSE;
    return MenuBar->IsMenuBarMessage(lpMsg);
}

HANDLE
CViewerWindow::GetLock()
{
    if (Lock == NULL)
        Lock = CreateEvent(NULL, FALSE, FALSE, NULL);
    return Lock;
}

BOOL CViewerWindow::InitializeGraphics()
{
    HBITMAP hTmpMaskBitmap;
    HBITMAP hTmpGrayBitmap;
    HBITMAP hTmpColorBitmap;

    hTmpColorBitmap = LoadBitmap(DLLInstance, MAKEINTRESOURCE(SalamanderGeneral->CanUse256ColorsBitmap() ? IDB_TOOLBAR256 : IDB_TOOLBAR16));
    SalamanderGUI->CreateGrayscaleAndMaskBitmaps(hTmpColorBitmap, RGB(255, 0, 255),
                                                 hTmpGrayBitmap, hTmpMaskBitmap);
    HHotToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    HGrayToolBarImageList = ImageList_Create(16, 16, ILC_MASK | ILC_COLORDDB, IDX_TB_COUNT, 1);
    ImageList_Add(HHotToolBarImageList, hTmpColorBitmap, hTmpMaskBitmap);
    ImageList_Add(HGrayToolBarImageList, hTmpGrayBitmap, hTmpMaskBitmap);
    DeleteObject(hTmpMaskBitmap);
    DeleteObject(hTmpGrayBitmap);
    DeleteObject(hTmpColorBitmap);
    return TRUE;
}

BOOL CViewerWindow::ReleaseGraphics()
{
    if (HHotToolBarImageList != NULL)
        ImageList_Destroy(HHotToolBarImageList);
    if (HGrayToolBarImageList != NULL)
        ImageList_Destroy(HGrayToolBarImageList);
    return TRUE;
}

BOOL CViewerWindow::FillToolBar()
{
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_ENABLER | TLBI_MASK_STYLE;

    ToolBar->SetImageList(HGrayToolBarImageList);
    ToolBar->SetHotImageList(HHotToolBarImageList);

    int i;
    for (i = 0; ToolBarButtons[i].ImageIndex != IDX_TB_TERMINATOR; i++)
    {
        int imgIndex = ToolBarButtons[i].ImageIndex;
        if (imgIndex == IDX_TB_ZOOMNUMBER)
        {
            TLBI_ITEM_INFO2 tii2;
            tii2.Mask = TLBI_MASK_ID | TLBI_MASK_TEXT | TLBI_MASK_STYLE | TLBI_MASK_ENABLER;
            tii2.Style = TLBI_STYLE_DROPDOWN | TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_SHOWTEXT;
            char buff[] = "0%";
            tii2.Text = _T(buff);
            tii2.ID = ToolBarButtons[i].ID;
            if (ToolBarButtons[i].Enabler == vweAlwaysEnabled)
                tii2.Enabler = NULL;
            else
                tii2.Enabler = Enablers + ToolBarButtons[i].Enabler;
            ToolBar->InsertItem2(0xFFFFFFFF, TRUE, &tii2);
            continue;
        }
        if (imgIndex == IDX_TB_SEPARATOR)
        {
            tii.Style = TLBI_STYLE_SEPARATOR;
        }
        else
        {
            tii.Style = 0;
            if (imgIndex == IDX_TB_HAND || imgIndex == IDX_TB_PICK ||
                imgIndex == IDX_TB_SELECT || imgIndex == IDX_TB_ZOOM)
                tii.Style = TLBI_STYLE_RADIO;
            if (imgIndex == IDX_TB_SELSRCFILE)
                tii.Style = TLBI_STYLE_CHECK;
            tii.ImageIndex = imgIndex;
            tii.ID = ToolBarButtons[i].ID;
            if (ToolBarButtons[i].Enabler == vweAlwaysEnabled)
                tii.Enabler = NULL;
            else
                tii.Enabler = Enablers + ToolBarButtons[i].Enabler;
        }
        ToolBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
    }

    // update the enablers
    ToolBar->UpdateItemsState();
    return TRUE;
}

BOOL CViewerWindow::InsertMenuBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = MenuBar->GetNeededWidth();
    rbbi.cyMinChild = MenuBar->GetNeededHeight();
    rbbi.fStyle = RBBS_NOGRIPPER;
    rbbi.hwndChild = MenuBar->GetHWND();
    rbbi.wID = BANDID_MENU;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)0, (LPARAM)&rbbi);
    return TRUE;
}

BOOL CViewerWindow::InsertToolBarBand()
{
    REBARBANDINFO rbbi;
    ZeroMemory(&rbbi, sizeof(rbbi));

    rbbi.cbSize = sizeof(REBARBANDINFO);
    rbbi.fMask = RBBIM_SIZE | RBBIM_CHILD | RBBIM_CHILDSIZE | RBBIM_STYLE | RBBIM_ID;
    rbbi.cxMinChild = ToolBar->GetNeededWidth();
    rbbi.cyMinChild = ToolBar->GetNeededHeight();
    rbbi.cx = 10000;
    rbbi.fStyle = RBBS_NOGRIPPER | RBBS_BREAK;
    rbbi.hwndChild = ToolBar->GetHWND();
    rbbi.wID = BANDID_TOOLBAR;
    SendMessage(HRebar, RB_INSERTBAND, (WPARAM)1, (LPARAM)&rbbi);
    return TRUE;
}

void CViewerWindow::LayoutWindows()
{
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
}

void CViewerWindow::UpdateEnablers()
{
    LPCTSTR pDeletedTitle = LoadStr(IDS_DELETED_TITLE);
    Enablers[vweFileOpened] = Renderer.ImageLoaded;
    Enablers[vweFileOpened2] = Renderer.ImageLoaded && Renderer.FileName != NULL && *Renderer.FileName != '<';
    Enablers[vwePaste] = Renderer.ImageLoaded && IsClipboardFormatAvailable(CF_BITMAP);
    Enablers[vwePrevPage] = Renderer.ImageLoaded && Renderer.PageAvailable(FALSE);
    Enablers[vweNextPage] = Renderer.ImageLoaded && Renderer.PageAvailable(TRUE);
    Enablers[vweMorePages] = Enablers[vwePrevPage] || Enablers[vweNextPage];
    Enablers[vweImgInfoAvailable] = Renderer.pvii.Width != CW_USEDEFAULT;
    Enablers[vweImgExifAvailable] = Renderer.ImageLoaded && (Renderer.pvii.Flags & PVFF_EXIF) && _tcscmp(Renderer.FileName, pDeletedTitle);
    Enablers[vweNotLoading] = !Renderer.Loading;
    Enablers[vweSelection] = Renderer.ImageLoaded && IsCageValid(&Renderer.SelectRect);

    if (HWindow != NULL && // when closed from another thread (shutdown / closing Salamander's main window) the window may no longer exist
        IsWindowVisible(HWindow) && (Renderer.FileName == NULL || *Renderer.FileName != '<' || _tcscmp(Renderer.FileName, pDeletedTitle) == 0))
    {
        BOOL srcBusy, noMoreFiles;
        TCHAR fileName[MAX_PATH] = _T("");
        LPCTSTR openedFileName = Renderer.FileName;

        if (Renderer.FileName != NULL && _tcscmp(Renderer.FileName, pDeletedTitle) == 0)
            openedFileName = NULL;
        int enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
        BOOL ok = SalamanderGeneral->GetPreviousFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                                  &enumFilesCurrentIndex,
                                                                  openedFileName, FALSE,
                                                                  TRUE, fileName, &noMoreFiles,
                                                                  &srcBusy);
        Enablers[vwePrevFile] = ok || srcBusy;                 // only if there is a previous file (or Salamander is busy, the user has to try later)
        Enablers[vweFirstFile] = ok || srcBusy || noMoreFiles; // jumping to the first or last file works only if the source link is intact (or Salamander is busy, the user has to try later)

        if (Enablers[vweFirstFile]) // if there is no connection to the source, further queries make no sense
        {
            // find out whether a previous selected file exists
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            ok = SalamanderGeneral->GetPreviousFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                                 &enumFilesCurrentIndex,
                                                                 openedFileName,
                                                                 TRUE /* prefer selected */, TRUE,
                                                                 fileName, &noMoreFiles,
                                                                 &srcBusy);
            BOOL isSrcFileSel = FALSE;
            if (ok)
            {
                ok = SalamanderGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                                    enumFilesCurrentIndex,
                                                                    fileName, &isSrcFileSel,
                                                                    &srcBusy);
            }
            Enablers[vwePrevSelFile] = ok && isSrcFileSel || srcBusy; // only if the previous file is actually selected (or Salamander is busy, the user has to try later)

            if (Renderer.FileName != NULL && *Renderer.FileName != '<')
            {
                ok = SalamanderGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                                    Renderer.EnumFilesCurrentIndex,
                                                                    Renderer.FileName, &IsSrcFileSelected,
                                                                    &srcBusy);
                Enablers[vweSelSrcFile] = ok || srcBusy; // only if the source file exists (or Salamander is busy, the user has to try later)
            }
            else
            {
                Enablers[vweSelSrcFile] = FALSE;
                IsSrcFileSelected = FALSE;
            }

            BOOL deletedFile = Renderer.FileName != NULL && _tcscmp(Renderer.FileName, pDeletedTitle) == 0;
            fileName[0] = 0;
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            if (deletedFile && enumFilesCurrentIndex >= 0)
                enumFilesCurrentIndex--; // prevent skipping the next file after deleting with Space due to files shifting in the panel
            ok = SalamanderGeneral->GetNextFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                             &enumFilesCurrentIndex,
                                                             openedFileName, FALSE,
                                                             TRUE, fileName, &noMoreFiles,
                                                             &srcBusy);
            Enablers[vweNextFile] = ok || srcBusy; // only if there is another file (or Salamander is busy, the user has to try later)

            // find out whether the next file is selected or whether no selected file remains
            fileName[0] = 0;
            enumFilesCurrentIndex = Renderer.EnumFilesCurrentIndex;
            if (deletedFile && enumFilesCurrentIndex >= 0)
                enumFilesCurrentIndex--; // prevent skipping the next file after deleting with Space due to files shifting in the panel
            ok = SalamanderGeneral->GetNextFileNameForViewer(Renderer.EnumFilesSourceUID,
                                                             &enumFilesCurrentIndex,
                                                             openedFileName,
                                                             TRUE /* prefer selected */, TRUE,
                                                             fileName, &noMoreFiles,
                                                             &srcBusy);
            isSrcFileSel = FALSE;
            if (ok)
            {
                ok = SalamanderGeneral->IsFileNameForViewerSelected(Renderer.EnumFilesSourceUID,
                                                                    enumFilesCurrentIndex,
                                                                    fileName, &isSrcFileSel,
                                                                    &srcBusy);
            }
            Enablers[vweNextSelFile] = ok && isSrcFileSel || srcBusy; // only if the next file is actually selected (or Salamander is busy, the user has to try later)
        }
        else
        {
            Enablers[vweSelSrcFile] = FALSE;
            Enablers[vweNextFile] = FALSE;
            Enablers[vwePrevSelFile] = FALSE;
            Enablers[vweNextSelFile] = FALSE;
            IsSrcFileSelected = FALSE;
        }
    }
    else
    {
        Enablers[vweSelSrcFile] = FALSE;
        Enablers[vweNextFile] = FALSE;
        Enablers[vwePrevFile] = FALSE;
        Enablers[vwePrevSelFile] = FALSE;
        Enablers[vweNextSelFile] = FALSE;
        Enablers[vweFirstFile] = FALSE;
        IsSrcFileSelected = FALSE;
    }
    if (ToolBar != NULL)
        ToolBar->UpdateItemsState();
}

void CViewerWindow::UpdateToolBar()
{
    if (ToolBar == NULL)
        return;

    // set the selected/unselected state of the source file (in the panel or Find window)
    ToolBar->CheckItem(CMD_FILE_TOGGLESELECT, FALSE, IsSrcFileSelected);

    // update the zoom value shown on the toolbar
    TCHAR buff[100];
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_TEXT;
    tii.Text = buff;
    _stprintf(buff, _T("%d %%"), (int)(Renderer.ZoomFactor / (ZOOM_SCALE_FACTOR / 100)));
    ToolBar->SetItemInfo2(CMD_ZOOM_TO, FALSE, &tii);

    // press the buttons according to the Tool variable
    ToolBar->CheckItem(CMD_TOOLS_HAND, FALSE, Renderer.CurrTool == RT_HAND);
    ToolBar->CheckItem(CMD_TOOLS_ZOOM, FALSE, Renderer.CurrTool == RT_ZOOM);
    ToolBar->CheckItem(CMD_TOOLS_PIPETTE, FALSE, Renderer.CurrTool == RT_PIPETTE);
    ToolBar->CheckItem(CMD_TOOLS_SELECT, FALSE, Renderer.CurrTool == RT_SELECT);
}

BOOL AddToHistory(BOOL filesHistory, LPCTSTR buff)
{
    LPTSTR* history = filesHistory ? G.FilesHistory : G.DirsHistory;
    int historySize = filesHistory ? FILES_HISTORY_SIZE : DIRS_HISTORY_SIZE;

    EnterCriticalSection(&G.CS);
    int from = historySize - 1;
    int i;
    for (i = 0; i < historySize; i++)
        if (history[i] != NULL)
            if (SalamanderGeneral->StrICmp(history[i], buff) == 0)
            {
                from = i;
                break;
            }
    BOOL ret = TRUE;
    if (from > 0)
    {
        LPTSTR text = (LPTSTR)malloc(sizeof(TCHAR) * (_tcslen(buff) + 1));
        if (text != NULL)
        {
            free(history[from]);
            int j;
            for (j = from - 1; j >= 0; j--)
                history[j + 1] = history[j];
            history[0] = text;
            _tcscpy(history[0], buff);
        }
        else
            ret = FALSE;
    }
    LeaveCriticalSection(&G.CS);
    return ret;
}

BOOL RemoveFromHistory(BOOL filesHistory, int index)
{
    LPTSTR* history = filesHistory ? G.FilesHistory : G.DirsHistory;
    ;
    int historySize = filesHistory ? FILES_HISTORY_SIZE : DIRS_HISTORY_SIZE;

    EnterCriticalSection(&G.CS);
    if (index < historySize)
    {
        if (history[index] != NULL)
        {
            free(history[index]);
            int i;
            for (i = index; i < historySize - 1; i++)
                history[i] = history[i + 1];
            history[historySize - 1] = NULL;
        }
    }
    LeaveCriticalSection(&G.CS);
    return TRUE;
}

void FillMenuHistory(CGUIMenuPopupAbstract* popup, int cmdFirst, BOOL filesHistory)
{
    LPTSTR* history = filesHistory ? G.FilesHistory : G.DirsHistory;
    ;
    int historySize = filesHistory ? FILES_HISTORY_SIZE : DIRS_HISTORY_SIZE;

    popup->RemoveAllItems();

    MENU_ITEM_INFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING | MENU_MASK_STATE;
    mi.Type = MENU_TYPE_STRING;
    mi.State = 0;

    EnterCriticalSection(&G.CS);
    if (history[0] == NULL)
    {
        mi.String = LoadStr(IDS_EMPTY);
        mi.ID = cmdFirst + 0;
        mi.State = MENU_STATE_GRAYED;
        popup->InsertItem(-1, TRUE, &mi);
    }
    else
    {
        TCHAR buff[MAX_PATH + 3];
        mi.String = buff;
        int index = 0;
        while (history[index] != NULL && index < historySize)
        {
            _stprintf(buff, _T("&%d %s"), index < 9 ? index + 1 : 0, history[index]);
            mi.ID = cmdFirst + index;
            popup->InsertItem(-1, TRUE, &mi);
            index++;
        }
    }
    LeaveCriticalSection(&G.CS);
}

#ifdef ENABLE_WIA

BOOL CViewerWindow::InitWiaWrap()
{
    if (WiaWrap == NULL)
        WiaWrap = new CWiaWrap;
    return WiaWrap != NULL;
}

void CViewerWindow::ReleaseWiaWrap()
{
    if (WiaWrap != NULL)
    {
        delete WiaWrap;
        WiaWrap = NULL;
    }
}

#endif // ENABLE_WIA

#ifdef ENABLE_TWAIN32

BOOL CViewerWindow::InitTwain()
{
    if (Twain != NULL)
        return TRUE;
    Twain = new (CTwain);
    if (Twain == NULL)
    {
        TRACE_E("Low memory");
        return FALSE;
    }
    if (!Twain->InitTwain(this))
    {
        delete (Twain);
        Twain = NULL;
        return FALSE;
    }
    return TRUE;
}

void CViewerWindow::ReleaseTwain()
{
    if (Twain != NULL)
    {
        delete Twain;
        Twain = NULL;
    }
}

#endif // ENABLE_TWAIN32

BOOL CViewerWindow::OpenScannedImage(HBITMAP hBitmap)
{
    Renderer.EnumFilesSourceUID = -1;
    Renderer.OpenFile(LoadStr(IDS_SCAN_TITLE), -1, hBitmap);
    return TRUE;
}

void CViewerWindow::ReleaseExtraScanImages(BOOL deleteImgs)
{
    if (ExtraScanImages != NULL)
    {
        if (deleteImgs)
        {
            for (int i = 0; i < ExtraScanImages->Count; i++)
            {
                if (ExtraScanImages->At(i) != NULL)
                    DeleteObject(ExtraScanImages->At(i));
            }
        }
        delete ExtraScanImages;
        ExtraScanImages = NULL;
    }
}

void CViewerWindow::ToggleToolBar()
{
    if (ToolBar == NULL)
    {
        ToolBar = SalamanderGUI->CreateToolBar(HWindow);
        if (ToolBar == NULL)
            return;
        ToolBar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);
        ToolBar->CreateWnd(HRebar);
        FillToolBar();
        InsertToolBarBand();
        G.ToolbarVisible = TRUE;
    }
    else
    {
        LRESULT index = SendMessage(HRebar, RB_IDTOINDEX, BANDID_TOOLBAR, 0);
        SendMessage(HRebar, RB_DELETEBAND, index, 0);
        SalamanderGUI->DestroyToolBar(ToolBar);
        ToolBar = NULL;
        G.ToolbarVisible = FALSE;
    }
}

void CViewerWindow::ToggleStatusBar()
{
    if (StatusBar == NULL)
    {
        StatusBar = new CStatusBar();
        if (StatusBar == NULL)
            return;

        StatusBar->CreateEx(0,
                            STATUSCLASSNAME,
                            _T(""),
                            WS_VISIBLE | WS_CHILD | WS_CLIPSIBLINGS,
                            0,
                            0,
                            0,
                            0,
                            HWindow,
                            NULL,
                            DLLInstance,
                            StatusBar);
        if (StatusBar->HWindow == NULL)
        {
            delete StatusBar;
            StatusBar = NULL;
            return;
        }
        G.StatusbarVisible = TRUE;
    }
    else
    {
        DestroyWindow(StatusBar->HWindow);
        StatusBar = NULL;
        G.StatusbarVisible = FALSE;
    }
}

BOOL CViewerWindow::IsFullScreen()
{
    return FullScreen;
}

void CViewerWindow::ToggleFullScreen()
{
    if (Renderer.Loading)
        return;

    Renderer.HiddenCursor = FALSE;
    LockWindowUpdate(HWindow);
    FullScreen = !FullScreen;
    if (Renderer.HAreaBrush != NULL)
    {
        // must change workspace color: delete old handle now and create new one on WM_PAINT
        DeleteObject(Renderer.HAreaBrush);
        Renderer.HAreaBrush = NULL;
    }

    if (!FullScreen)
    {
        // when returning from full-screen mode, restore the original image view parameters
        if (Renderer.SavedZoomParams)
        {
            Renderer.ZoomType = Renderer.SavedZoomType;
            Renderer.ZoomFactor = Renderer.SavedZoomFactor;
        }

        DWORD style = WS_VISIBLE | WS_OVERLAPPED | WS_OVERLAPPEDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN;

        SetWindowLong(HWindow, GWL_STYLE, style);

        style = GetWindowLong(Renderer.HWindow, GWL_EXSTYLE);
        style |= WS_EX_CLIENTEDGE;
        SetWindowLong(Renderer.HWindow, GWL_EXSTYLE, style);
        ShowWindow(HRebar, SW_SHOW);
        if (StatusBar != NULL)
            ShowWindow(StatusBar->HWindow, SW_SHOW);

        EnsureNoTopmost();

        SetWindowPlacement(HWindow, &WindowPlacement);

        // when returning from full-screen mode, restore the original image view parameters
        if (Renderer.SavedZoomParams)
        {
            Renderer.SavedZoomParams = FALSE;
            SetScrollPos(Renderer.HWindow, SB_HORZ, Renderer.SavedXScrollPos, FALSE);
            SetScrollPos(Renderer.HWindow, SB_VERT, Renderer.SavedYScrollPos, FALSE);
        }

        if (Renderer.PVHandle)
            PVW32DLL.PVSetBkHandle(Renderer.PVHandle, GetCOLORREF(G.Colors[vceTransparent]));
    }
    else
    {
        RECT dummy, maxRect;
        DWORD style;

        // save the current image view parameters for switching back from full-screen mode
        Renderer.SavedZoomParams = TRUE;
        Renderer.SavedZoomType = Renderer.ZoomType;
        Renderer.SavedZoomFactor = Renderer.ZoomFactor;
        Renderer.SavedXScrollPos = GetScrollPos(Renderer.HWindow, SB_HORZ);
        Renderer.SavedYScrollPos = GetScrollPos(Renderer.HWindow, SB_VERT);

        Renderer.ZoomType = eShrinkToFit; // full-screen always uses shrink-to-fit

        WindowPlacement.length = sizeof(WindowPlacement);
        GetWindowPlacement(HWindow, &WindowPlacement);

        style = WS_POPUP | WS_VISIBLE | WS_OVERLAPPED | WS_CLIPSIBLINGS | WS_SYSMENU;
        SetWindowLong(HWindow, GWL_STYLE, style);

        style = GetWindowLong(Renderer.HWindow, GWL_EXSTYLE);
        style &= ~WS_EX_CLIENTEDGE;
        SetWindowLong(Renderer.HWindow, GWL_EXSTYLE, style);

        ShowWindow(HRebar, SW_HIDE);
        if (StatusBar != NULL)
            ShowWindow(StatusBar->HWindow, SW_HIDE);

        SalamanderGeneral->MultiMonGetClipRectByWindow(HWindow, &dummy, &maxRect);
        GetWindowRect(HWindow, &dummy);

        if ((dummy.left == maxRect.left) && (dummy.top == maxRect.top) && (dummy.right == maxRect.right) && (dummy.bottom == maxRect.bottom))
        {
            // This is an ugly patch, maybe not needed for W2K+:
            // If the window already covers entire monitor area (although not in maximized state),
            // the second (originallly the only) SetWindowPos in fact would not do anything.
            // As a result, the renderer would not get resized, window frame would stay and
            // the window title and rebar/menu area would display rubbish
            SetWindowPos(HWindow, HWND_TOPMOST, maxRect.left, maxRect.top,
                         maxRect.right - maxRect.left, maxRect.bottom - maxRect.top - 1,
                         0);
            WindowPlacement.rcNormalPosition.bottom--;
        }

        SetWindowPos(HWindow, HWND_TOPMOST, maxRect.left, maxRect.top,
                     maxRect.right - maxRect.left, maxRect.bottom - maxRect.top,
                     0);

        if (Renderer.PVHandle)
            PVW32DLL.PVSetBkHandle(Renderer.PVHandle, GetCOLORREF(G.Colors[vceFSTransparent]));
    }
    LockWindowUpdate(NULL);
}

void CViewerWindow::EnsureNoTopmost()
{
    if (!AlwaysOnTop && HWindow != NULL)
    {
        DWORD exStyle = GetWindowLong(HWindow, GWL_EXSTYLE);
        if (exStyle & WS_EX_TOPMOST)
            SetWindowPos(HWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOSIZE | SWP_NOMOVE);
    }
}

void CViewerWindow::OnSize(void)
{
    if (Renderer.HWindow != NULL && HRebar != NULL && MenuBar != NULL /*&& StatusBar.HWindow != NULL*/)
    {
        RECT r;

        BOOL rebarVisible = IsWindowVisible(HRebar);
        int rebarHeight = 0;
        if (rebarVisible)
        {
            GetWindowRect(HRebar, &r);
            rebarHeight = r.bottom - r.top;
        }

        BOOL statusVisible = StatusBar != NULL && IsWindowVisible(StatusBar->HWindow);
        int statusHeight = 0;
        if (statusVisible)
        {
            GetWindowRect(StatusBar->HWindow, &r);
            statusHeight = r.bottom - r.top;
        }

        GetClientRect(HWindow, &r);

        int wndCount = 1;
        if (rebarVisible)
            wndCount++;
        if (StatusBar != NULL)
            wndCount++;
        HDWP hdwp = BeginDeferWindowPos(wndCount);
        if (hdwp != NULL)
        {
            // +4: when widening the window the last 4 pixels of the rebar were not redrawn;
            // after several hours I still could not find the cause; it works fine in Salamander.
            // For now I'm handling it like this; maybe I'll remember the issue later.
            // Patera 2003.03.11: Surprisingly it is not related to the gripper visibility.
            if (rebarVisible)
            {
                hdwp = DeferWindowPos(hdwp, HRebar, NULL,
                                      0, 0, r.right + 4, rebarHeight,
                                      SWP_NOACTIVATE | SWP_NOZORDER);
            }

            hdwp = DeferWindowPos(hdwp, Renderer.HWindow, NULL,
                                  0, rebarHeight, r.right, r.bottom - rebarHeight - statusHeight,
                                  SWP_NOACTIVATE | SWP_NOZORDER);
            if (statusVisible)
            {
                hdwp = DeferWindowPos(hdwp, StatusBar->HWindow, NULL,
                                      0, r.bottom - statusHeight, r.right, statusHeight,
                                      SWP_NOACTIVATE | SWP_NOZORDER);
            }
            EndDeferWindowPos(hdwp);
        }
    }
} /* CViewerWindow::OnSize */

LRESULT
CViewerWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CViewerWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

#ifdef ENABLE_TWAIN32
    if (Twain != NULL && Twain->GetModalUI())
    {
        // if the scanner UI is active, suppress our functionality
        if (uMsg != WM_CLOSE && uMsg != WM_SIZE && uMsg != WM_PAINT &&
            (uMsg != WM_TIMER || wParam != CLOSEWND_TIMER_ID))
        {
            return CWindow::WindowProc(uMsg, wParam, lParam);
        }
    }
#endif // ENABLE_TWAIN32

    // WM_MOUSEWHEEL sometimes arrives here; forward it
    if (uMsg == WM_MOUSEWHEEL && Renderer.HWindow != NULL)
        return SendMessage(Renderer.HWindow, uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_CREATE:
    {
        InitializeGraphics();
        MainMenu = SalamanderGUI->CreateMenuPopup();
        if (MainMenu == NULL)
            return -1;
        MainMenu->LoadFromTemplate(HLanguage, MenuTemplate, Enablers, HGrayToolBarImageList, HHotToolBarImageList);
        MenuBar = SalamanderGUI->CreateMenuBar(MainMenu, HWindow);
        if (MenuBar == NULL)
            return -1;

        RECT r;
        GetClientRect(HWindow, &r);
        HRebar = CreateWindowEx(WS_EX_TOOLWINDOW, REBARCLASSNAME, _T(""),
                                WS_VISIBLE | /*WS_BORDER |  */ WS_CHILD |
                                    WS_CLIPCHILDREN | WS_CLIPSIBLINGS |
                                    RBS_VARHEIGHT | CCS_NODIVIDER |
                                    RBS_BANDBORDERS | CCS_NOPARENTALIGN |
                                    RBS_AUTOSIZE,
                                0, 0, r.right, r.bottom, // dummy
                                HWindow, (HMENU)0, DLLInstance, NULL);

        // we do not want visual styles for the rebar
        SalamanderGUI->DisableWindowVisualStyles(HRebar);

        Renderer.CreateEx(/*WS_EX_STATICEDGE*/ WS_EX_CLIENTEDGE,
                          CWINDOW_CLASSNAME2,
                          _T(""),
                          WS_VISIBLE | WS_CHILD | /*WS_VSCROLL | WS_HSCROLL | */ WS_CLIPSIBLINGS,
                          0,
                          0,
                          0,
                          0,
                          HWindow,
                          NULL,
                          DLLInstance,
                          &Renderer);

        MenuBar->CreateWnd(HRebar);
        InsertMenuBand();

        if (G.ToolbarVisible)
            ToggleToolBar();
        if (G.StatusbarVisible)
            ToggleStatusBar();

        SetFocus(Renderer.HWindow);

        ViewerWindowQueue.Add(new CWindowQueueItem(HWindow));

        UpdateEnablers();

        break;
    }

    case WM_CANCELMODE:
    {
        EnsureNoTopmost();
        break;
    }

    case WM_NCACTIVATE:
    {
        if (wParam == FALSE)
            EnsureNoTopmost();
        break;
    }

    case WM_ACTIVATE:
    {
        if (!LOWORD(wParam))
        {
            // the main window will not refresh when switching from the viewer
            SalamanderGeneral->SkipOneActivateRefresh();
        }
        if (LOWORD(wParam) == WA_ACTIVE || LOWORD(wParam) == WA_CLICKACTIVE)
        {
            // j.r.: this does not work for me; if I put the cursor on the toolbar and switch between Salamander and PV, things freeze
            // PostMessage(HWindow, WM_APP + 1, 0, 0);
            SetTimer(HWindow, ENABLERS_TIMER_ID, 100, NULL);
        }
        break;
    }

    //case WM_APP + 1:
    case WM_TIMER:
    {
        if (wParam == ENABLERS_TIMER_ID)
        {
            KillTimer(HWindow, ENABLERS_TIMER_ID);
            UpdateEnablers();
            UpdateToolBar();
        }
        if (wParam == CLOSEWND_TIMER_ID) // we should send ourselves WM_CLOSE; the blocking dialog is hopefully gone
        {
            KillTimer(HWindow, CLOSEWND_TIMER_ID);
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            return 0;
        }
        break;
    }

    case WM_USER_SCAN_EXTRA_IMAGES:
    {
        if (ExtraScanImages != NULL && ExtraScanImages->Count > 0)
        {
            char buf[500];
            _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_SCAN_OPEN_EXTRA_IMGS), ExtraScanImages->Count);
            if (SalamanderGeneral->SalMessageBox(HWindow, buf, LoadStr(IDS_PLUGINNAME),
                                                 MB_ICONQUESTION | MB_YESNOCANCEL) == IDYES)
            {
                ExtraScanImagesToOpen.AddImages(ExtraScanImages);
                ReleaseExtraScanImages(FALSE);
                SalamanderGeneral->PostMenuExtCommand(CMD_INTERNAL_SCAN_EXTRA_IMAGES, FALSE);
            }
            else
                ReleaseExtraScanImages();
        }
        return 0;
    }

    case WM_CLOSE:
    {
#ifdef ENABLE_WIA
        if (WiaWrap != NULL && WiaWrap->IsAcquiringImage()) // a WIA routine is running = cannot close the window and free WIA
#endif                                                      // ENABLE_WIA
#ifdef ENABLE_TWAIN32
            if (Twain != NULL && Twain->IsSourceManagerOpened()) // a Twain routine is running = cannot close the window and free Twain
#endif                                                           // ENABLE_TWAIN32
            {
#ifdef ENABLE_WIA
                WiaWrap->CloseParentAfterAcquiring(); // close the viewer window right after the WIA routine finishes; recalculating enablers would only slow things down
#endif                                                // ENABLE_WIA
#ifdef ENABLE_TWAIN32
                Twain->CloseViewerAfterClosingSM(); // close the viewer window right after the Twain routine finishes; recalculating enablers would only slow things down
#endif                                              // ENABLE_TWAIN32
                if (!IsWindowEnabled(HWindow))
                { // close all dialogs above this dialog one by one (send them WM_CLOSE and then send it here again),
                    // hoping to terminate the Twain / WIA routine
                    SalamanderGeneral->CloseAllOwnedEnabledDialogs(HWindow);
                    // Petr: the window of my Twain scanner is not caught by this; never mind, at least it does not crash (we wait for the user to close the window manually)
                    // Petr: WIA through Twain will not send WM_CLOSE here either; it closes only after the WIA window is closed
                }
                SetTimer(HWindow, CLOSEWND_TIMER_ID, 100, NULL); // request WM_CLOSE again, waiting for the Twain / WIA routine to finish
                return 0;
            }
        break;
    }

    case WM_DESTROY:
    {
        if (MenuBar != NULL)
        {
            SalamanderGUI->DestroyMenuBar(MenuBar);
            MenuBar = NULL;
        }
        if (MainMenu != NULL)
        {
            SalamanderGUI->DestroyMenuPopup(MainMenu);
            MainMenu = NULL;
        }
        if (ToolBar != NULL)
        {
            SalamanderGUI->DestroyToolBar(ToolBar);
            ToolBar = NULL;
        }

        if (Renderer.HWindow != NULL)
            DestroyWindow(Renderer.HWindow);
        ViewerWindowQueue.Remove(HWindow);

        if (Lock != NULL)
        {
            SetEvent(Lock);
            Lock = NULL;
        }
#ifdef ENABLE_WIA
        ReleaseWiaWrap();
#endif // ENABLE_WIA
#ifdef ENABLE_TWAIN32
        ReleaseTwain();
#endif // ENABLE_TWAIN32
        ReleaseGraphics();

        // icons obtained without the LR_SHARED flag must be destroyed (ICON_SMALL)
        // destroying shared ones (ICON_BIG) does no harm (nothing happens)
        HICON hIcon = (HICON)SendMessage(HWindow, WM_SETICON, ICON_BIG, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);
        hIcon = (HICON)SendMessage(HWindow, WM_SETICON, ICON_SMALL, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);

        PostQuitMessage(0);
        break;
    }

    case WM_USER_VIEWERCFGCHNG:
    {
        if (Renderer.HWindow != NULL)
            SendMessage(Renderer.HWindow, uMsg, wParam, lParam);
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (MenuBar != NULL)
            MenuBar->SetFont();
        if (ToolBar != NULL)
            ToolBar->SetFont();
        if (HHistogramWindow != NULL)
            SendMessage(HHistogramWindow, WM_USER_SETTINGCHANGE, wParam, lParam);
        return 0;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        _tcscpy(tt->Buffer, LoadStr(ToolBarButtons[tt->Index].ToolTipResID));
        SalamanderGUI->PrepareToolTipText(tt->Buffer, FALSE);
        return TRUE;
    }

    case WM_COMMAND:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP:
    {
        // Use Send instead of Post, because if this window gets activated,
        // WM_CONTEXTMENU arrives here on Shift+F10 instead of being caught
        // in the Renderer window, which would activate this window's system menu.
        if (Renderer.HWindow != NULL)
            SendMessage(Renderer.HWindow, uMsg, wParam, lParam);
        break;
    }

    case WM_ERASEBKGND:
    {
        return 1;
    }

    case WM_NOTIFY:
    {
        LPNMHDR lphdr = (LPNMHDR)lParam;
        if (lphdr->code == RBN_AUTOSIZE)
        {
            LayoutWindows();
            return 0;
        }
        break;
    }

    case WM_SETFOCUS:
    {
        if (Renderer.HWindow != NULL)
            SetFocus(Renderer.HWindow);
        break;
    }

    case WM_SIZE:
    {
        OnSize();
        break;
    }

    case WM_USER_INITMENUPOPUP:
    {
        CGUIMenuPopupAbstract* popup = (CGUIMenuPopupAbstract*)wParam;
        WORD popupID = HIWORD(lParam);
        switch (popupID)
        {
        case CML_RECENTFILES:
        {
            FillMenuHistory(popup, CMD_RECENTFILES_FIRST, TRUE);
            break;
        }

        case CML_RECENTDIRS:
        {
            FillMenuHistory(popup, CMD_RECENRDIRS_FIRST, FALSE);
            break;
        }

        case CML_FILE:
        {
            popup->CheckItem(CMD_FILE_TOGGLESELECT, FALSE, IsSrcFileSelected);
            break;
        }

        case CML_VIEW:
        {
            popup->CheckItem(CMD_TOOLBAR, FALSE, G.ToolbarVisible);
            popup->CheckItem(CMD_STATUSBAR, FALSE, G.StatusbarVisible);
            popup->CheckItem(CMD_FULLSCREEN, FALSE, IsFullScreen());
            break;
        }

        case CML_CONTEXT:
        {
            popup->CheckItem(CMD_FULLSCREEN, FALSE, IsFullScreen());
            popup->CheckItem(CMD_FILE_TOGGLESELECT, FALSE, IsSrcFileSelected);
            break;
        }
        }

        UpdateEnablers();
        UpdateToolBar();
        return 0;
    }

    case WM_USER_TBDROPDOWN:
    {
        if (ToolBar != NULL && ToolBar->GetHWND() == (HWND)wParam)
        {
            int index = (int)lParam;
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_ID;
            if (!ToolBar->GetItemInfo2(index, TRUE, &tii))
                return 0;

            RECT r;
            ToolBar->GetItemRect(index, r);

            switch (tii.ID)
            {
            case CMD_ZOOM_TO:
            {
                CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
                if (popup != NULL)
                {
                    TCHAR buff[20];
                    MENU_ITEM_INFO mi;
                    memset(&mi, 0, sizeof(mi));
                    mi.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING;
                    mi.Type = MENU_TYPE_STRING;
                    mi.String = buff;
                    int i;
                    for (i = 0; PredefinedZooms[i] != -1; i++)
                    {
                        _stprintf(buff, _T("%1.2f"), (double)PredefinedZooms[i] / 100.0);
                        TrailZeros(buff);
                        _tcscat(buff, _T("%"));
                        mi.ID = i + 1;
                        popup->InsertItem(-1, TRUE, &mi);
                    }
                    DWORD cmd = popup->Track(MENU_TRACK_RETURNCMD, r.left, r.bottom, HWindow, &r);
                    SalamanderGUI->DestroyMenuPopup(popup);
                    if (cmd != 0)
                        PostMessage(Renderer.HWindow, WM_USER_ZOOM, (WPARAM)PredefinedZooms[cmd - 1], 0);
                }
                break;
            }
            }
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
