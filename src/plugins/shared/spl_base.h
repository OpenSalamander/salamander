// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_base) // so that structures are independent of the current alignment setting
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// in the debug build we test whether the source and destination memory overlap (they must not overlap for memcpy)
#if defined(_DEBUG) && defined(TRACE_ENABLE)
#define memcpy _sal_safe_memcpy
#ifdef __cplusplus
extern "C"
{
#endif
    void* _sal_safe_memcpy(void* dest, const void* src, size_t count);
#ifdef __cplusplus
}
#endif
#endif // defined(_DEBUG) && defined(TRACE_ENABLE)

// The following functions do not crash when working with invalid memory (or even with NULL):
// lstrcpy, lstrcpyn, lstrlen, and lstrcat (they are defined with the A or W suffix, so
// we do not redefine them directly). For easier debugging we need them to crash,
// because otherwise the error is discovered later, at a place where it may no longer be clear what
// caused it
#define lstrcpyA _sal_lstrcpyA
#define lstrcpyW _sal_lstrcpyW
#define lstrcpynA _sal_lstrcpynA
#define lstrcpynW _sal_lstrcpynW
#define lstrlenA _sal_lstrlenA
#define lstrlenW _sal_lstrlenW
#define lstrcatA _sal_lstrcatA
#define lstrcatW _sal_lstrcatW
#ifdef __cplusplus
extern "C"
{
#endif
    LPSTR _sal_lstrcpyA(LPSTR lpString1, LPCSTR lpString2);
    LPWSTR _sal_lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2);
    LPSTR _sal_lstrcpynA(LPSTR lpString1, LPCSTR lpString2, int iMaxLength);
    LPWSTR _sal_lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength);
    int _sal_lstrlenA(LPCSTR lpString);
    int _sal_lstrlenW(LPCWSTR lpString);
    LPSTR _sal_lstrcatA(LPSTR lpString1, LPCSTR lpString2);
    LPWSTR _sal_lstrcatW(LPWSTR lpString1, LPCWSTR lpString2);
#ifdef __cplusplus
}
#endif

// the original SDK that shipped with VC6 had this value defined as 0x00000040 (year 1998, when the attribute was not yet used; it only arrived with W2K)
#if (FILE_ATTRIBUTE_ENCRYPTED != 0x00004000)
#pragma message(__FILE__ " ERROR: FILE_ATTRIBUTE_ENCRYPTED != 0x00004000. You have to install latest version of Microsoft SDK. This value has changed!")
#endif

class CSalamanderGeneralAbstract;
class CPluginDataInterfaceAbstract;
class CPluginInterfaceForArchiverAbstract;
class CPluginInterfaceForViewerAbstract;
class CPluginInterfaceForMenuExtAbstract;
class CPluginInterfaceForFSAbstract;
class CPluginInterfaceForThumbLoaderAbstract;
class CSalamanderGUIAbstract;
class CSalamanderSafeFileAbstract;
class CGUIIconListAbstract;

//
// ****************************************************************************
// CSalamanderDebugAbstract
//
// sada metod ze Salamandera, ktere se pouzivaji pro hledani chyb v debug i release verzi

// makro CALLSTK_MEASURETIMES - zapne mereni casu straveneho pri priprave call-stack hlaseni (meri se pomer proti
//                              celkovemu casu behu funkci)
//                              POZOR: nutne zapnout tez pro kazdy plugin zvlast
// makro CALLSTK_DISABLEMEASURETIMES - potlaci mereni casu straveneho pri priprave call-stack hlaseni v DEBUG/SDK/PB verzi

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext
{
    DWORD PushesCounterStart;                      // initial state of the Push counter for calls made in this thread
    LARGE_INTEGER PushPerfTimeCounterStart;        // initial state of the time counter for time spent in Push methods called in this thread
    LARGE_INTEGER IgnoredPushPerfTimeCounterStart; // initial state of the time counter for time spent in unmeasured (ignored) Push methods called in this thread
    LARGE_INTEGER StartTime;                       // the "time" of the Push in this call-stack macro
    DWORD_PTR PushCallerAddress;                   // address of the CALL_STACK_MESSAGE macro (address of the Push)
};
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

class CSalamanderDebugAbstract
{
public:
    // writes 'file'+'line'+'str' as TRACE_I to the TRACE SERVER - only in the DEBUG/SDK/PB build of Salamander
    virtual void WINAPI TraceI(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceIW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // writes 'file'+'line'+'str' as TRACE_E to the TRACE SERVER - only in the DEBUG/SDK/PB build of Salamander
    virtual void WINAPI TraceE(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceEW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // registers a new thread with TRACE (assigns a Unique ID); 'thread'+'tid' are returned by
    // _beginthreadex and CreateThread; optional (UID is then -1)
    virtual void WINAPI TraceAttachThread(HANDLE thread, unsigned tid) = 0;

    // nastavi jmeno aktivniho threadu pro TRACE, nepovine (thread je oznacen jako "unknown")
    // POZOR: vyzaduje registraci threadu u TRACE (viz TraceAttachThread), jinak nic nedela
    virtual void WINAPI TraceSetThreadName(const char* name) = 0;
    virtual void WINAPI TraceSetThreadNameW(const WCHAR* name) = 0;

    // initializes the thread with the things needed for CALL-STACK methods (see Push and Pop below);
    // in all called plugin methods, CALL_STACK methods can then be used directly;
    // this method is used only for new plugin threads;
    // it runs 'threadBody' with the 'param' parameter and returns the result of 'threadBody'
    virtual unsigned WINAPI CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param) = 0;

    // uklada na CALL-STACK zpravu ('format'+'args' viz vsprintf), pri padu aplikace je
    // obsah CALL-STACKU vypsan do okna Bug Report ohlasujiciho pad aplikace
    virtual void WINAPI Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                             BOOL doNotMeasureTimes) = 0;

    // odstranuje z CALL-STACKU posledni zpravu, volani musi parovat s Push
    virtual void WINAPI Pop(CCallStackMsgContext* callStackMsgContext) = 0;

    // sets the active thread name for the VC debugger
    virtual void WINAPI SetThreadNameInVC(const char* name) = 0;

    // calls TraceSetThreadName and SetThreadNameInVC for 'name' (see those two methods for details)
    virtual void WINAPI SetThreadNameInVCAndTrace(const char* name) = 0;

    // If we are not already connected to the Trace Server, it tries to connect (the server
    // must be running). In the SDK version of Salamander only (including Preview Builds): if server
    // autostart is enabled and the server is not running (e.g. the user has already terminated it), it tries to start it before
    // connecting.
    virtual void WINAPI TraceConnectToServer() = 0;

    // used for modules that may report memory leaks; if memory leaks are detected,
    // all modules registered this way are loaded "as image" (without module initialization) during the leak check
    // (by then these modules are already unloaded), and only then are the memory leaks reported, so the
    // .cpp module names are visible instead of "#File Error#"
    // may be called from any thread
    virtual void WINAPI AddModuleWithPossibleMemoryLeaks(const char* fileName) = 0;
};

//
// ****************************************************************************
// CSalamanderRegistryAbstract
//
// sada metod Salamandera pro praci se systemovym registry,
// pouziva se v CPluginInterfaceAbstract::LoadConfiguration
// a CPluginInterfaceAbstract::SaveConfiguration

class CSalamanderRegistryAbstract
{
public:
    // clears all subkeys and values from the 'key' key; returns success
    virtual BOOL WINAPI ClearKey(HKEY key) = 0;

    // creates or opens the existing subkey 'name' of the 'key' key, returns 'createdKey' and success;
    // the obtained key ('createdKey') must be closed by calling CloseKey
    virtual BOOL WINAPI CreateKey(HKEY key, const char* name, HKEY& createdKey) = 0;

    // opens the existing subkey 'name' of the 'key' key, returns 'openedKey' and success
    // the obtained key ('openedKey') must be closed by calling CloseKey
    virtual BOOL WINAPI OpenKey(HKEY key, const char* name, HKEY& openedKey) = 0;

    // closes the key opened via OpenKey or CreateKey
    virtual void WINAPI CloseKey(HKEY key) = 0;

    // deletes the 'name' subkey of the 'key' key; returns success
    virtual BOOL WINAPI DeleteKey(HKEY key, const char* name) = 0;

    // reads the 'name'+'type'+'buffer'+'bufferSize' value from the 'key' key; returns success
    virtual BOOL WINAPI GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize) = 0;

    // stores the 'name'+'type'+'data'+'dataSize' value in the 'key' key; for strings you can
    // specify 'dataSize' == -1 to calculate the string length with strlen;
    // returns success
    virtual BOOL WINAPI SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize) = 0;

    // deletes the 'name' value from the 'key' key; returns success
    virtual BOOL WINAPI DeleteValue(HKEY key, const char* name) = 0;

    // retrieves into 'bufferSize' the size required for the 'name'+'type' value from the 'key' key; returns success
    virtual BOOL WINAPI GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize) = 0;
};

//
// ****************************************************************************
// CSalamanderConnectAbstract
//
// sada metod Salamandera pro navazani pluginu do Salamandera
// (custom pack/unpack + panel archiver view/edit + file viewer + menu-items)

// constants for CSalamanderConnectAbstract::AddMenuItem
#define MENU_EVENT_TRUE 0x0001                    // always occurs
#define MENU_EVENT_DISK 0x0002                    // source is a Windows directory ("c:\path" or UNC)
#define MENU_EVENT_THIS_PLUGIN_ARCH 0x0004        // source is this plugin's archive
#define MENU_EVENT_THIS_PLUGIN_FS 0x0008          // source is this plugin's file system
#define MENU_EVENT_FILE_FOCUSED 0x0010            // focus is on a file
#define MENU_EVENT_DIR_FOCUSED 0x0020             // focus is on a directory
#define MENU_EVENT_UPDIR_FOCUSED 0x0040           // focus is on ".."
#define MENU_EVENT_FILES_SELECTED 0x0080          // files are selected
#define MENU_EVENT_DIRS_SELECTED 0x0100           // directories are selected
#define MENU_EVENT_TARGET_DISK 0x0200             // target is a Windows directory ("c:\path" or UNC)
#define MENU_EVENT_TARGET_THIS_PLUGIN_ARCH 0x0400 // target is this plugin's archive
#define MENU_EVENT_TARGET_THIS_PLUGIN_FS 0x0800   // target is this plugin's file system
#define MENU_EVENT_SUBDIR 0x1000                  // the directory is not the root (contains "..")
// focus is on a file for which this plugin provides "panel archiver view" or "panel archiver edit"
#define MENU_EVENT_ARCHIVE_FOCUSED 0x2000
// only 0x4000 is still available (both masks are stored in a DWORD and masked with 0x7FFF beforehand)

// specifies which type of user the item is intended for
#define MENU_SKILLLEVEL_BEGINNER 0x0001     // intended for the most important menu items, for beginners
#define MENU_SKILLLEVEL_INTERMEDIATE 0x0002 // assign also to less frequently used commands; for intermediate users
#define MENU_SKILLLEVEL_ADVANCED 0x0004     // assign to all commands (power users should have everything in the menu)
#define MENU_SKILLLEVEL_ALL 0x0007          // helper constant combining all previous ones

// macro for preparing 'HotKey' for AddMenuItem()
// LOWORD - hot key (virtual key + modifiers) (LOBYTE - virtual key, HIBYTE - modifiers)
// mods: combination of HOTKEYF_CONTROL, HOTKEYF_SHIFT, HOTKEYF_ALT
// examples: SALHOTKEY('A', HOTKEYF_CONTROL | HOTKEYF_SHIFT), SALHOTKEY(VK_F1, HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_EXT)
// #define SALHOTKEY(vk,mods,cst) ((DWORD)(((BYTE)(vk)|((WORD)((BYTE)(mods))<<8))|(((DWORD)(BYTE)(cst))<<16)))
#define SALHOTKEY(vk, mods) ((DWORD)(((BYTE)(vk) | ((WORD)((BYTE)(mods)) << 8))))

// makro pro pripravu 'hotKey' pro AddMenuItem()
// rika Salamanderu, ze polozky menu bude obsahovat horkou klavesu (oddelenou znakem '\t')
// Salamander nebude v tomto pripade kricet pomoci TRACE_E a horkou klavesu zobrazi v menu Plugins
// POZOR: nejedna se o horkou klavesu, kterou by Salamander dorucil pluginu, jde skutecne pouze o napis
// pokud uzivatel priradi v Plugin Manageru tomuto commandu vlastni horkou klavesu, bude hint potlacen
#define SALHOTKEY_HINT ((DWORD)0x00020000)

class CSalamanderConnectAbstract
{
public:
    // adds the plugin to the list for "custom archiver pack",
    // 'title' is the user-visible name of the custom packer, 'defaultExtension' is the default extension
    // for new archives; if this is not an upgrade of "custom pack" (or the addition of the whole plugin) and
    // 'update' je FALSE, je volani ignorovano; je-li 'update' TRUE, prepise se nastaveni na
    // nove hodnoty 'title' a 'defaultExtension' - nutna prevence proti opakovanemu 'update'==TRUE
    // (to avoid continually overwriting the settings)
    virtual void WINAPI AddCustomPacker(const char* title, const char* defaultExtension, BOOL update) = 0;

    // adds the plugin to the list for "custom archiver unpack",
    // 'title' is the user-visible name of the custom unpacker, 'masks' are archive-file masks (used to determine
    // which unpacker should unpack a given archive; the separator is ';' (the escape sequence for ';' is
    // ";;") and the usual '*' and '?' wildcards are used, plus '#' for '0'..'9'); if this is not an upgrade
    // "custom unpack" (nebo pridani celeho pluginu) a 'update' je FALSE je volani ignorovano;
    // if 'update' is TRUE, the settings are overwritten with the new 'title' and 'masks' values - repeated
    // proti opakovanemu 'update'==TRUE (neustalemu prepisovani nastaveni)
    virtual void WINAPI AddCustomUnpacker(const char* title, const char* masks, BOOL update) = 0;

    // adds the plugin to the list for "panel archiver view/edit",
    // 'extensions' are the archive extensions handled by this plugin
    // (the separator is ';' (';' has no escape sequence here) and the wildcard '#' is used for
    // '0'..'9'), pokud je 'edit' TRUE, resi tento plugin "panel archiver view/edit", jinak jen
    // "panel archiver view"; if this is not an upgrade of "panel archiver view/edit" (or the addition
    // celeho pluginu) a 'updateExts' je FALSE je volani ignorovano; je-li 'updateExts' TRUE,
    // new archive extensions are added (ensuring that all extensions from 'extensions' are present) - repeated
    // prevence proti opakovanemu 'updateExts'==TRUE (neustalemu ozivovani pripon z 'extensions')
    virtual void WINAPI AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts) = 0;

    // removes an extension from the list for "panel archiver view/edit" (only from items belonging to
    // this plugin); 'extension' is a single archive extension (the '#' wildcard is used for '0'..'9');
    // repeated calls must be prevented (to avoid continually deleting 'extension')
    virtual void WINAPI ForceRemovePanelArchiver(const char* extension) = 0;

    // adds the plugin to the list for "file viewer",
    // 'masks' are the viewer masks handled by this plugin
    // (the separator is ';' (the escape sequence for ';' is ";;") and the '*' and '?' wildcards are used;
    // if possible, avoid spaces, and the '|' character is forbidden (inverse masks are not allowed)),
    // if this is not an upgrade of "file viewer" (or the addition of the whole plugin) and 'force' is FALSE,
    // je volani ignorovano; je-li 'force' TRUE, prida 'masks' vzdy (pokud jiz nejsou na
    // seznamu) - nutna prevence proti opakovanemu 'force'==TRUE (neustalemu pridavani 'masks')
    virtual void WINAPI AddViewer(const char* masks, BOOL force) = 0;

    // removes a mask from the list for "file viewer" (only from items belonging to this plugin);
    // 'mask' is a single viewer mask (the '*' and '?' wildcards are used); repeated calls must be prevented
    // (to avoid continually deleting 'mask')
    virtual void WINAPI ForceRemoveViewer(const char* mask) = 0;

    // adds items to the Plugins/"plugin name" menu in Salamander, 'iconIndex' is the
    // item icon index (-1=no icon; for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons; u separatoru se ignoruje), 'name' je
    // jmeno polozky (max. MAX_PATH - 1 znaku) nebo NULL jde-li o separator (parametry
    // 'state_or'+'state_and' parameters have no meaning in that case); 'hotKey' is the hot key
    // for the item, obtained with the SALHOTKEY macro; 'name' may contain a hot-key hint,
    // separated by '\t'; in that case the 'hotKey' variable must be assigned the
    // SALHOTKEY_HINT, vice viz komentar k SALHOTKEY_HINT; 'id' je unikatni identifikacni
    // cislo polozky v ramci pluginu (u separatoru ma vyznam jen je-li 'callGetState' TRUE),
    // pokud je 'callGetState' TRUE, vola se pro zjisteni stavu polozky metoda
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (u separatoru ma vyznam jen stav
    // MENU_ITEM_STATE_HIDDEN, ostatni se ignoruji), jinak se k vypoctu stavu polozky (enabled/disabled)
    // is computed from 'state_or'+'state_and' - the item state is calculated by first building the mask
    // ('eventMask') as the logical OR of all events that occurred (see
    // MENU_EVENT_XXX), polozka bude "enable" pokud bude nasl. vyraz TRUE:
    //   ('eventMask' & 'state_or') != 0 && ('eventMask' & 'state_and') == 'state_and',
    // the 'skillLevel' parameter specifies for which user levels the item (or separator)
    // is displayed; the value contains one or more MENU_SKILLLEVEL_XXX constants ORed together;
    // menu items are updated on every plugin load (the items may change with configuration)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddMenuItem
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // adds a submenu to the Plugins/"plugin name" menu in Salamander, 'iconIndex'
    // is the submenu icon index (-1=no icon; for assigning a bitmap with icons
    // viz CSalamanderConnectAbstract::SetBitmapWithIcons), 'name' je jmeno
    // name (max. MAX_PATH - 1 characters), 'id' is the unique menu-item identifier
    // menu v ramci pluginu (u submenu ma vyznam jen je-li 'callGetState' TRUE),
    // pokud je 'callGetState' TRUE, vola se pro zjisteni stavu submenu metoda
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (vyznam maji jen stavy
    // MENU_ITEM_STATE_ENABLED and MENU_ITEM_STATE_HIDDEN states matter; the others are ignored), otherwise
    // the item state (enabled/disabled) is computed from 'state_or'+'state_and' - for the state calculation
    // stavu polozky viz CSalamanderConnectAbstract::AddMenuItem(), parametr 'skillLevel'
    // specifies for which user levels the submenu is displayed; the value contains one or more
    // MENU_SKILLLEVEL_XXX constants ORed together; the submenu is closed by calling
    // CSalamanderConnectAbstract::AddSubmenuEnd();
    // menu items are updated on every plugin load (the items may change with configuration)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddSubmenuStart
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ukonci submenu v menu Plugins/"jmeno pluginu" v Salamanderu, dalsi polozky budou
    // pridavany do vyssi (rodicovske) urovne menu;
    // polozky v menu se updatuji pri kazdem loadu pluginu (mozna zmena polozek dle konfigurace)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddSubmenuEnd
    virtual void WINAPI AddSubmenuEnd() = 0;

    // nastavuje polozku pro FS v Change Drive menu a v Drive barach; 'title' je jeji text,
    // 'iconIndex' je index jeji ikony (-1=zadna ikona; zadani bitmapy s ikonami viz
    // CSalamanderConnectAbstract::SetBitmapWithIcons), 'title' muze obsahovat az tri sloupce
    // vzajemne oddelene '\t' (viz Alt+F1/F2 menu); viditelnost polozky je mozne nastavit
    // z Plugins Manageru nebo primo z pluginu pomoci metody
    // CSalamanderGeneralAbstract::SetChangeDriveMenuItemVisibility
    virtual void WINAPI SetChangeDriveMenuItem(const char* title, int iconIndex) = 0;

    // informs Salamander that the plugin can load thumbnails from files matching the group mask
    // 'masks' (the separator is ';' (the escape sequence for ';' is ";;") and the '*' and '?' wildcards are used);
    // to load a thumbnail, Salamander calls
    // CPluginInterfaceForThumbLoaderAbstract::LoadThumbnail
    virtual void WINAPI SetThumbnailLoader(const char* masks) = 0;

    // nastavi bitmapu s ikonami pluginu; Salamander si obsah bitmapy kopiruje do internich
    // struktur, plugin je zodpovedny za destrukci bitmapy (ze strany Salamanadera se
    // bitmapa pouzije pouze behem teto funkce); pocet ikon se odvozuje ze
    // sirky bitmapy, ikony jsou vzdy 16x16 bodu; transparentni cast ikon tvori fialova
    // barva (RGB(255,0,255)), barevna hloubka bitmapy muze byt 4 nebo 8 bitu (16 nebo 256
    // barev), idealni je mit pripravene obe barevne varianty a vybirat z nich podle
    // vysledku metody CSalamanderGeneralAbstract::CanUse256ColorsBitmap()
    // POZOR: tato metoda je zastarala, nepodporuje alpha transparenci, pouzijte misto ni
    //        SetIconListForGUI()
    virtual void WINAPI SetBitmapWithIcons(HBITMAP bitmap) = 0;

    // sets the plugin icon index used for the plugin in the Plugins/Plugins Manager window,
    // in the Help/About Plugin menu, and optionally also for the plugin submenu in the Plugins menu (for details
    // viz CSalamanderConnectAbstract::SetPluginMenuAndToolbarIcon()); pokud plugin tuto
    // method, the standard Salamander plugin icon is used; 'iconIndex'
    // is the icon index to set (for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons)
    virtual void WINAPI SetPluginIcon(int iconIndex) = 0;

    // sets the icon index for the plugin submenu, used for the plugin submenu
    // in the Plugins menu and optionally also in the top toolbar for the drop-down button used
    // to display the plugin submenu; if the plugin does not call this method, the
    // plugin icon is used for the plugin submenu in the Plugins menu (configured by
    // CSalamanderConnectAbstract::SetPluginIcon) and no toolbar button is shown
    // for the plugin; 'iconIndex' is the icon index to set (-1=use the plugin icon
    // pluginu, viz CSalamanderConnectAbstract::SetPluginIcon(); zadani bitmapy
    // s ikonami viz CSalamanderConnectAbstract::SetBitmapWithIcons);
    virtual void WINAPI SetPluginMenuAndToolbarIcon(int iconIndex) = 0;

    // sets the icon list with plugin icons; the icon list must be allocated by calling
    // CSalamanderGUIAbstract::CreateIconList() and then created and filled using
    // methods of the CGUIIconListAbstract interface; icon size must be 16x16 pixels;
    // Salamander takes ownership of the icon-list object; the plugin must not destroy it after calling
    // this function; the icon list is stored in Salamander's configuration
    // so that the icons can be used on the next start without loading the plugin, so
    // put only the necessary icons into it
    virtual void WINAPI SetIconListForGUI(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CDynamicString
//
// dynamicky string: realokuje se podle potreby

class CDynamicString
{
public:
    // returns TRUE if the string 'str' of length 'len' was added successfully; if 'len' is -1,
    // 'len' is taken as "strlen(str)" (add without the terminating null); if 'len' is -2,
    // 'len' is taken as "strlen(str)+1" (add including the terminating null)
    virtual BOOL WINAPI Add(const char* str, int len = -1) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceAbstract
//
// sada metod pluginu, ktere potrebuje Salamander pro praci s pluginem
//
// Pro vetsi prehlednost jsou oddelene casti pro:
// archivatory - viz CPluginInterfaceForArchiverAbstract,
// viewry - viz CPluginInterfaceForViewerAbstract,
// rozsireni menu - viz CPluginInterfaceForMenuExtAbstract,
// file-systemy - viz CPluginInterfaceForFSAbstract,
// nacitace thumbnailu - viz CPluginInterfaceForThumbLoaderAbstract.
// Casti jsou pripojeny k CPluginInterfaceAbstract pres CPluginInterfaceAbstract::GetInterfaceForXXX

// flags indicating which functions the plugin provides (which methods of the
// CPluginInterfaceAbstract descendant are actually implemented in the plugin):
#define FUNCTION_PANELARCHIVERVIEW 0x0001     // methods for "panel archiver view"
#define FUNCTION_PANELARCHIVEREDIT 0x0002     // methods for "panel archiver edit"
#define FUNCTION_CUSTOMARCHIVERPACK 0x0004    // methods for "custom archiver pack"
#define FUNCTION_CUSTOMARCHIVERUNPACK 0x0008  // methods for "custom archiver unpack"
#define FUNCTION_CONFIGURATION 0x0010         // Configuration() method
#define FUNCTION_LOADSAVECONFIGURATION 0x0020 // methods for "load/save configuration"
#define FUNCTION_VIEWER 0x0040                // methods for "file viewer"
#define FUNCTION_FILESYSTEM 0x0080            // methods for "file system"
#define FUNCTION_DYNAMICMENUEXT 0x0100        // methods for "dynamic menu extension"

// codes of various events (and the meaning of the 'param' parameter), received by CPluginInterfaceAbstract::Event():
// the colors have changed (because of a system color change / WM_SYSCOLORCHANGE or because of a configuration change); the plugin can
// fetch updated Salamander colors via CSalamanderGeneralAbstract::GetCurrentColor;
// if the plugin has a file system with pitFromPlugin icons, it should repaint the image-list background
// for simple icons to SALCOL_ITEM_BK_NORMAL; 'param' is ignored here
#define PLUGINEVENT_COLORSCHANGED 0

// Salamander's configuration has changed; the plugin can fetch updated versions of Salamander
// configuration parameters via CSalamanderGeneralAbstract::GetConfigParameter;
// 'param' is ignored here
#define PLUGINEVENT_CONFIGURATIONCHANGED 1

// the left and right panels were swapped (Swap Panels - Ctrl+U)
// 'param' is ignored here
#define PLUGINEVENT_PANELSSWAPPED 2

// the active panel has changed (switch between panels)
// 'param' is PANEL_LEFT or PANEL_RIGHT - it identifies the activated panel
#define PLUGINEVENT_PANELACTIVATED 3

// Salamander received WM_SETTINGCHANGE and regenerated the fonts for toolbars on that basis.
// It then sends this event to all plugins so they can call
// SetFont() on their toolbars;
// 'param' is ignored here
#define PLUGINEVENT_SETTINGCHANGE 4

// Password Manager event codes, received by CPluginInterfaceAbstract::PasswordManagerEvent():
#define PME_MASTERPASSWORDCREATED 1 // user created a master password (passwords need to be encrypted)
#define PME_MASTERPASSWORDCHANGED 2 // user changed the master password (passwords need to be decrypted and then re-encrypted)
#define PME_MASTERPASSWORDREMOVED 3 // user removed the master password (passwords need to be decrypted)

class CPluginInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginInterfaceEncapsulation)
    friend class CPluginInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // called in response to the About button in the Plugins window or the Help/About Plugins menu command
    virtual void WINAPI About(HWND parent) = 0;

    // called before the plugin is unloaded (naturally only if SalamanderPluginEntry returned
    // this object and not NULL); returns TRUE if the unload may proceed,
    // 'parent' is the parent window for message boxes, 'force' is TRUE if the return
    // value is ignored; if it returns TRUE, this object and all other objects obtained from it
    // will no longer be used and the plugin will be unloaded; if a critical shutdown is in progress (see
    // CSalamanderGeneralAbstract::IsCriticalShutdown), nema smysl se usera na cokoliv ptat
    // (do not open any more windows)
    // POZOR!!! Je nutne ukoncit vsechny thready pluginu (pokud Release vrati TRUE, vola se
    // FreeLibrary is called on the plugin .SPL => the plugin code is unmapped from memory => the threads then
    // have nothing left to execute => usually neither a bug report nor Windows exception info is generated)
    virtual BOOL WINAPI Release(HWND parent, BOOL force) = 0;

    // method for loading the default configuration and for "load/save configuration" (loading from the plugin's private
    // registry key), 'parent' is the parent window for message boxes; if 'regKey' == NULL, the default
    // configuration is being loaded; 'registry' is the object used to work with the registry; this method is always called
    // after SalamanderPluginEntry and before other calls (loading from the private key is performed if
    // this function is provided by the plugin and the registry key exists; otherwise only the default
    // configuration is loaded)
    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // method for "load/save configuration"; called to save the plugin configuration to its private
    // registry key, 'parent' is the parent window for message boxes, 'registry' is the object used to work with the registry;
    // when Salamander saves its configuration, it also calls this method (if the plugin provides it); Salamander
    // also supports saving the plugin configuration when the plugin is unloaded (for example, manually from Plugins Manager);
    // in that case, saving is performed only if Salamander's registry key exists
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // called in response to the Configuration button in the Plugins window
    virtual void WINAPI Configuration(HWND parent) = 0;

    // called to connect the plugin to Salamander; called only after LoadConfiguration,
    // 'parent' is the parent of the message boxes; 'salamander' is the set of methods used to connect the plugin

    /*  PRAVIDLA PRO IMPLEMENTACI METODY CONNECT
        (pluginy musi mit ulozenou verzi konfigurace - viz DEMOPLUGin,
         promenna ConfigVersion a konstanta CURRENT_CONFIG_VERSION; nize je
         nazorny PRIKLAD pridani pripony "dmp2" do DEMOPLUGinu):

      -s kazdou zmenou je potreba zvysit cislo verze konfigurace - CURRENT_CONFIG_VERSION
       (v prvni verzi metody Connect je CURRENT_CONFIG_VERSION=1)
      -do zakladni casti (pred podminky "if (ConfigVersion < YYY)"):
        -se napise kod pro instalaci pluginu (uplne prvni load pluginu):
         viz metody CSalamanderConnectAbstract
        -pri upgradech je nutne aktualizovat seznamy pripon pro instalaci pro "custom archiver
         unpack" (AddCustomUnpacker), "panel archiver view/edit" (AddPanelArchiver),
         "file viewer" (AddViewer), polozky menu (AddMenuItem), atd.
        -u volani AddPanelArchiver a AddViewer nechame 'updateExts' a 'force' FALSE
         (jinak bysme uzivateli nutili nejen nove, ale i stare pripony, ktere uz treba
         rucne smazal)
        -u volani AddCustomPacker/AddCustomUnpacker dame do parametru 'update' podminku
         "ConfigVersion < XXX", kde XXX je cislo posledni verze, kde se menily
         pripony pro custom packery/unpackery (obe volani je potreba posuzovat zvlast;
         zde pro jednoduchost vnutime uzivateli vsechny pripony, pokud si nejake promazal
         nebo pridal, ma smulu, bude to muset udelat rucne znovu)
        -AddMenuItem, SetChangeDriveMenuItem a SetThumbnailLoader funguje pri kazdem loadu
         pluginu stejne (instalace/upgrady se nelisi - vzdy se zacina na zelene louce)
      -jen pri upgradech: do casti pro upgrady (za zakladni casti):
        -pridame podminku "if (ConfigVersion < XXX)", kde XXX je nova hodnota
         konstanty CURRENT_CONFIG_VERSION + pridame komentar od teto verze;
         v tele teto podminky zavolame:
          -pokud pribyly pripony pro "panel archiver", zavolame
           "AddPanelArchiver(PPP, EEE, TRUE)", kde PPP jsou jen nove pripony oddelene
           strednikem a EEE je TRUE/FALSE ("panel view+edit"/"jen panel view")
          -pokud pribyly pripony pro "viewer", zavolame "AddViewer(PPP, TRUE)",
           kde PPP jsou jen nove pripony oddelene strednikem
          -pokud se maji smazat nejake stare pripony pro "viewer", zavolame
           pro kazdou takovou priponu PPP "ForceRemoveViewer(PPP)"
          -pokud se maji smazat nejake stare pripony pro "panel archiver", zavolame
           pro kazdou takovou priponu PPP "ForceRemovePanelArchiver(PPP)"

      KONTROLA: po techto upravach doporucuji vyzkouset, jestli to funguje, jak ma,
                staci nakompilovat plugin a zkusit ho naloadit do Salama, melo by
                dojit k automatickemu upgradu z predesle verze (bez potreby
                vyhozeni a pridani pluginu):
                -viz menu Options/Configuration:
                  -Viewery jsou na strance Viewers: najdete pridane pripony,
                   zkontrolujte, ze odebrane pripony jiz neexistuji
                  -Panel Archivers jsou na strance Archives Associations in Panels:
                   najdete pridane pripony
                  -Custom Unpackers jsou na strance Unackers in Unpack Dialog Box:
                   najdete vas plugin a zkontrolujte, jestli je seznam masek OK
                -zkontrolovat novou podobu submenu pluginu (v menu Plugins)
                -zkontrolovat novou podobu Change Drive menu (Alt+F1/F2)
                -zkontrolovat v Plugins Manageru (v menu Plugins) masky thumbnaileru:
                 fokusnout vas plugin, pak zkontrolovat editbox "Thumbnails"
              +nakonec muzete jeste taky zkusit vyhodit a pridat plugin, jestli
               funguje "instalace" pluginu: kontrola viz vsechny predesle body

      POZNAMKA: pri pridani pripon pro "panel archiver" je tez potreba doplnit
                seznam pripon v parametru 'extensions' metody SetBasicPluginData

      PRIKLAD PRIDANI PRIPONY "dmp2" VIEWERU A ARCHIVERU:
        (radky zacinajici na "-" byly odstraneny, radky zacinajici na "+" pridany,
         symbol "=====" na zacatku radky znaci preruseni souvisleho useku kodu)
        Prehled zmen:
          -zvysila se verze konfigurace z 2 na 3:
            -pridany komentar k verzi 3
            -zvyseni CURRENT_CONFIG_VERSION na 3
          -pridani pripony "dmp2" do parametru 'extensions' SetBasicPluginData
           (protoze pridavame priponu "dmp2" pro "panel archiver")
          -pridani masky "*.dmp2" do AddCustomUnpacker + zvyseni verze z 1 na 3
           v podmince (protoze pridavame priponu "dmp2" pro "custom unpacker")
          -pridani pripony "dmp2" do AddPanelArchiver (protoze pridavame priponu
           "dmp2" pro "panel archiver")
          -pridani masky "*.dmp2" do AddViewer (protoze pridavame priponu "dmp2"
           pro "viewer")
          -pridani podminky pro upgrade na verzi 3 + komentar tohoto upgradu,
           telo podminky:
            -volani AddPanelArchiver pro priponu "dmp2" s 'updateExts' TRUE
             (protoze pridavame priponu "dmp2" pro "panel archiver")
            -volani AddViewer pro masku "*.dmp2" s 'force' TRUE (protoze
             pridavame priponu "dmp2" pro "viewer")
=====
  // ConfigVersion: 0 - zadna konfigurace se z Registry nenacetla (jde o instalaci pluginu),
  //                1 - prvni verze konfigurace
  //                2 - druha verze konfigurace (pridane nejake hodnoty do konfigurace)
+ //                3 - treti verze konfigurace (pridani pripony "dmp2")

  int ConfigVersion = 0;
- #define CURRENT_CONFIG_VERSION 2
+ #define CURRENT_CONFIG_VERSION 3
  const char *CONFIG_VERSION = "Version";
=====
  // nastavime zakladni informace o pluginu
  salamander->SetBasicPluginData("Salamander Demo Plugin",
                                 FUNCTION_PANELARCHIVERVIEW | FUNCTION_PANELARCHIVEREDIT |
                                 FUNCTION_CUSTOMARCHIVERPACK | FUNCTION_CUSTOMARCHIVERUNPACK |
                                 FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION |
                                 FUNCTION_VIEWER | FUNCTION_FILESYSTEM,
                                 "2.0",
                                 "Copyright © 1999-2023 Open Salamander Authors",
                                 "This plugin should help you to make your own plugins.",
-                                "DEMOPLUG", "dmp", "dfs");
+                                "DEMOPLUG", "dmp;dmp2", "dfs");
=====
  void WINAPI
  CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract *salamander)
  {
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // zakladni cast:
    salamander->AddCustomPacker("DEMOPLUG (Plugin)", "dmp", FALSE);
-   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp", ConfigVersion < 1);
+   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp;*.dmp2", ConfigVersion < 3);
-   salamander->AddPanelArchiver("dmp", TRUE, FALSE);
+   salamander->AddPanelArchiver("dmp;dmp2", TRUE, FALSE);
-   salamander->AddViewer("*.dmp", FALSE);
+   salamander->AddViewer("*.dmp;*.dmp2", FALSE);
===== (vynechal jsem pridavani polozek do menu, nastavovani ikon a masek thumbnailu)
    // cast pro upgrady:
+   if (ConfigVersion < 3)   // verze 3: pridani pripony "dmp2"
+   {
+     salamander->AddPanelArchiver("dmp2", TRUE, TRUE);
+     salamander->AddViewer("*.dmp2", TRUE);
+   }
  }
=====
    */
    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) = 0;

    // releases the 'pluginData' interface that Salamander obtained from the plugin by calling
    // CPluginInterfaceForArchiverAbstract::ListArchive nebo
    // CPluginFSInterfaceAbstract::ListCurrentPath; pred timto volanim jeste
    // file and directory data (CFileData::PluginData) are released using methods of
    // CPluginDataInterfaceAbstract
    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) = 0;

    // returns the archiver interface; the plugin must return this interface if it has
    // at least one of the following functions (see SetBasicPluginData): FUNCTION_PANELARCHIVERVIEW,
    // FUNCTION_PANELARCHIVEREDIT, FUNCTION_CUSTOMARCHIVERPACK a/nebo FUNCTION_CUSTOMARCHIVERUNPACK;
    // pokud plugin archivator neobsahuje, vraci NULL
    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() = 0;

    // returns the viewer interface; the plugin must return this interface if it has the function
    // (see SetBasicPluginData) FUNCTION_VIEWER; if the plugin does not contain a viewer, it returns NULL
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() = 0;

    // returns the menu-extension interface; the plugin must return this interface if it adds
    // polozky do menu (viz CSalamanderConnectAbstract::AddMenuItem) nebo pokud ma
    // funkci (viz SetBasicPluginData) FUNCTION_DYNAMICMENUEXT; v opacnem pripade vraci NULL
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() = 0;

    // returns the file-system interface; the plugin must return this interface if it has the function
    // (see SetBasicPluginData) FUNCTION_FILESYSTEM; if the plugin does not contain a file system, it returns NULL
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() = 0;

    // returns the thumbnail-loader interface; the plugin must return this interface if it informed
    // Salamandera, ze umi nacitat thumbnaily (viz CSalamanderConnectAbstract::SetThumbnailLoader);
    // pokud plugin neumi nacitat thumbnaily, vraci NULL
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() = 0;

    // prijem ruznych udalosti, viz kody udalosti PLUGINEVENT_XXX; vola se jen pokud je plugin
    // nacteny; 'param' je parametr udalosti
    // POZOR: muze se zavolat kdykoliv po dokonceni entry-pointu pluginu (SalamanderPluginEntry)
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // user requested that all histories be cleared (by running Clear History from the configuration
    // History page); here, history means everything that arises automatically from values entered by the user
    // (e.g. the list of texts entered on the command line, the list of current paths on
    // individual drives, etc.); this does not include lists created by the user - e.g. hot paths, user menu,
    // etc.; 'parent' is the parent of any message boxes; after saving the configuration, histories must not
    // remain in the registry; if the plugin has open windows containing histories (combo boxes), it must
    // clear the histories there as well
    virtual void WINAPI ClearHistory(HWND parent) = 0;

    // prijem informace o zmene na ceste 'path' (je-li 'includingSubdirs' TRUE, tak
    // zahrnuje i zmenu v podadresarich cesty 'path'); teto metody je mozne vyuzit napr.
    // k invalidovani/cisteni cache souboru/adresaru; POZNAMKA: pro pluginove file-systemy (FS)
    // existuje metoda CPluginFSInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // this method is called only for a plugin that uses Password Manager (see
    // CSalamanderGeneralAbstract::SetPluginUsesPasswordManager()):
    // it informs the plugin about changes in Password Manager; 'parent' is the parent of any
    // message boxes/dialogs; 'event' contains the event, see PME_XXX
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) = 0;
};

//
// ****************************************************************************
// CSalamanderPluginEntryAbstract
//
// sada metod ze Salamandera, ktere se pouzivaji v SalamanderPluginEntry

// flags indicating the reason for plugin load (see CSalamanderPluginEntryAbstract::GetLoadInformation)
#define LOADINFO_INSTALL 0x0001          // first plugin load (installation into Salamander)
#define LOADINFO_NEWSALAMANDERVER 0x0002 // new Salamander version (installs all plugins from \
                                         // plugins subdirectory), loads all plugins (possible \
                                         // upgrade of all)
#define LOADINFO_NEWPLUGINSVER 0x0004    // change in the plugins.ver file (plugin install/upgrade), \
                                         // for simplicity, it loads all plugins (possible \
                                         // upgrade of all)
#define LOADINFO_LOADONSTART 0x0008      // load occurred because the "load on start" flag was found

class CSalamanderPluginEntryAbstract
{
public:
    // returns the Salamander version; see spl_vers.h and the LAST_VERSION_OF_SALAMANDER and REQUIRE_LAST_VERSION_OF_SALAMANDER constants
    virtual int WINAPI GetVersion() = 0;

    // returns Salamander's parent window (parent for message boxes)
    virtual HWND WINAPI GetParentWindow() = 0;

    // returns a pointer to the interface for Salamander debug functions,
    // the interface is valid for the entire lifetime of the plugin (not only within the
    // "SalamanderPluginEntry" function) and is only a reference, so it is not released
    virtual CSalamanderDebugAbstract* WINAPI GetSalamanderDebug() = 0;

    // nastaveni zakladnich dat o pluginu (data, ktera si o pluginu spolu se jmenem DLL souboru
    // Salamander pamatuje), nutne volat, jinak nemuze byt plugin pripojen;
    // 'pluginName' je jmeno pluginu; 'functions' obsahuje naORovane vsechny funkce, ktere plugin
    // podporuje (viz konstanty FUNCTION_XXX); 'version'+'copyright'+'description' jsou data pro
    // uzivatele zobrazovana v okne Plugins; 'regKeyName' je navrhovany nazev soukromeho klice
    // pro ulozeni konfigurace v registry (bez FUNCTION_LOADSAVECONFIGURATION je ignorovan);
    // 'extensions' jsou zakladni pripony (napr. jen "ARJ"; "A01", atd. uz ne) zpracovavanych
    // archivu oddelene ';' (zde nema ';' zadnou escape sekvenci) - Salamander tyto pripony pouziva
    // jen pri hledani nahrady za odstranene panelove archivatory (nastava pri odstraneni pluginu;
    // resi se problem "co se ted postara o priponu XXX, kdyz byl puvodni asociovany archivator
    // odstranen v ramci pluginu PPP?") (bez FUNCTION_PANELARCHIVERVIEW a bez FUNCTION_PANELARCHIVEREDIT
    // je ignorovan); 'fsName' je navrhovane jmeno (ziskani prideleneho jmena se provede pomoci
    // CSalamanderGeneralAbstract::GetPluginFSName) file systemu (bez FUNCTION_FILESYSTEM je
    // ignorovan, povolene znaky jsou 'a-zA-Z0-9_+-', min. delka 2 znaky), pokud plugin potrebuje
    // vic jmen file systemu, muze pouzit metodu CSalamanderPluginEntryAbstract::AddFSName;
    // vraci TRUE pri uspesnem prijeti dat
    virtual BOOL WINAPI SetBasicPluginData(const char* pluginName, DWORD functions,
                                           const char* version, const char* copyright,
                                           const char* description, const char* regKeyName = NULL,
                                           const char* extensions = NULL, const char* fsName = NULL) = 0;

    // returns a pointer to the interface for Salamander's general-purpose functions,
    // the interface is valid for the entire lifetime of the plugin (not only within the
    // "SalamanderPluginEntry" function) and is only a reference, so it is not released
    virtual CSalamanderGeneralAbstract* WINAPI GetSalamanderGeneral() = 0;

    // returns information related to plugin load; the information is returned in a DWORD value
    // as a logical OR of LOADINFO_XXX flags (to test for the presence of a flag, use the
    // condition: (GetLoadInformation() & LOADINFO_XXX) != 0)
    virtual DWORD WINAPI GetLoadInformation() = 0;

    // loads the module with language-dependent resources (the SLG module); it always first tries to load the module
    // for the same language Salamander is currently running in; if it cannot find such a module (or
    // the version does not match), it lets the user choose an alternative module (if there is more than one
    // alternative and the user's choice from the previous plugin load is not already stored);
    // pluginu); pokud nenajde zadny modul, vraci NULL -> plugin by se mel ukoncit;
    // 'parent' is the parent of the error message boxes and of the dialog for selecting an alternative
    // language module; 'pluginName' is the plugin name (so the user knows which plugin
    // the error message or the alternative language module selection refers to)
    // POZOR: tuto metodu je mozne volat jen jednou; ziskany handle jazykoveho modulu
    //        is released automatically when the plugin is unloaded
    virtual HINSTANCE WINAPI LoadLanguageModule(HWND parent, const char* pluginName) = 0;

    // returns the ID of the current language selected for the Salamander UI (e.g. english.slg =
    // English (US) = 0x409, czech.slg = Czech = 0x405)
    virtual WORD WINAPI GetCurrentSalamanderLanguageID() = 0;

    // returns a pointer to the interface providing the modified Windows controls used
    // in Salamander; the interface is valid for the entire lifetime of the plugin (not only
    // within the "SalamanderPluginEntry" function) and is only a reference, so it is not released
    virtual CSalamanderGUIAbstract* WINAPI GetSalamanderGUI() = 0;

    // returns a pointer to the interface for convenient file handling,
    // the interface is valid for the entire lifetime of the plugin (not only within the
    // "SalamanderPluginEntry" function) and is only a reference, so it is not released
    virtual CSalamanderSafeFileAbstract* WINAPI GetSalamanderSafeFile() = 0;

    // sets the URL to be displayed in the Plugins Manager window as the plugin home page;
    // Salamander keeps the value until the next plugin load (the URL is shown even for
    // unloaded plugins); the URL must be set again on every plugin load, otherwise
    // no URL is shown (to avoid keeping an invalid home-page URL)
    virtual void WINAPI SetPluginHomePageURL(const char* url) = 0;

    // adds another file-system name; without FUNCTION_FILESYSTEM in the 'functions'
    // parameter passed to SetBasicPluginData, this method always fails;
    // 'fsName' is the proposed name (the assigned file-system name can be obtained with
    // CSalamanderGeneralAbstract::GetPluginFSName) file systemu (povolene znaky jsou
    // 'a-zA-Z0-9_+-', min. delka 2 znaky); v 'newFSNameIndex' (nesmi byt NULL) se
    // vraci index nove pridaneho jmena file systemu; vraci TRUE v pripade uspechu;
    // vraci FALSE pri fatalni chybe - v tomto pripade se 'newFSNameIndex' ignoruje
    // limitation: must not be called before SetBasicPluginData
    virtual BOOL WINAPI AddFSName(const char* fsName, int* newFSNameIndex) = 0;
};

//
// ****************************************************************************
// FSalamanderPluginEntry
//
// Open Salamander 1.6 or Later Plugin Entry Point Function Type,
// tuto funkci plugin vyvazi jako "SalamanderPluginEntry" a Salamander ji vola
// pro pripojeni pluginu v okamziku loadu pluginu
// vraci interface pluginu v pripade uspesneho pripojeni, jinak NULL,
// interface pluginu se uvolnuje volanim jeho metody Release pred unloadem pluginu

typedef CPluginInterfaceAbstract*(WINAPI* FSalamanderPluginEntry)(CSalamanderPluginEntryAbstract* salamander);

//
// ****************************************************************************
// FSalamanderPluginGetReqVer
//
// Open Salamander 2.5 Beta 2 or Later Plugin Get Required Version of Salamander Function Type,
// tuto funkci plugin vyvazi jako "SalamanderPluginGetReqVer" a Salamander ji vola
// jako prvni funkci pluginu (pred "SalamanderPluginGetSDKVer" a "SalamanderPluginEntry")
// v okamziku loadu pluginu;
// vraci verzi Salamandera, pro kterou je plugin staven (nejstarsi verze, do ktere lze plugin nacist)

typedef int(WINAPI* FSalamanderPluginGetReqVer)();

//
// ****************************************************************************
// FSalamanderPluginGetSDKVer
//
// Open Salamander 2.52 beta 2 (PB 22) or Later Plugin Get SDK Version Function Type,
// tuto funkci plugin volitelne vyvazi jako "SalamanderPluginGetSDKVer" a Salamander
// ji zkousi volat jako druhou funkci pluginu (pred "SalamanderPluginEntry")
// v okamziku loadu pluginu;
// vraci verzi SDK, pouziteho pro stavbu pluginu (informuje Salamandera, ktere metody
// plugin poskytuje); exportovat "SalamanderPluginGetSDKVer" ma smysl jen pokud vraci
// "SalamanderPluginGetReqVer" mensi cislo nez LAST_VERSION_OF_SALAMANDER; je vhodne
// vracet primo LAST_VERSION_OF_SALAMANDER

typedef int(WINAPI* FSalamanderPluginGetSDKVer)();

// ****************************************************************************
// SalIsWindowsVersionOrGreater
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided
// version information. This function is useful in confirming a version of Windows
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
//

#ifdef __BORLANDC__
inline void* SecureZeroMemory(void* ptr, int cnt)
{
    char* vptr = (char*)ptr;
    while (cnt)
    {
        *vptr++ = 0;
        cnt--;
    }
    return ptr;
}
#endif // __BORLANDC__

inline BOOL SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // replacement for memset (does not require the RTL)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

// Find Windows version using bisection method and VerifyVersionInfo.
// Author:   M1xA, www.m1xa.com
// Licence:  MIT
// Version:  1.0
// https://bitbucket.org/AnyCPU/findversion/src/ebdec778fdbcdee67ac9a4d520239e134e047d8d/include/findversion.h?at=default
// Tested on: Windows 2000 .. Windows 8.1.
//
// WARNING: This function is ***SLOW_HACK***, use SalIsWindowsVersionOrGreater() instead (if you can).

#define M1xA_FV_EQUAL 0
#define M1xA_FV_LESS -1
#define M1xA_FV_GREAT 1
#define M1xA_FV_MIN_VALUE 0
#define M1xA_FV_MINOR_VERSION_MAX_VALUE 16
inline int M1xA_testValue(OSVERSIONINFOEX* value, DWORD verPart, DWORDLONG eq, DWORDLONG gt)
{
    if (VerifyVersionInfo(value, verPart, eq) == FALSE)
    {
        if (VerifyVersionInfo(value, verPart, gt) == TRUE)
            return M1xA_FV_GREAT;
        return M1xA_FV_LESS;
    }
    else
        return M1xA_FV_EQUAL;
}

#define M1xA_findPartTemplate(T) \
    inline BOOL M1xA_findPart##T(T* part, DWORD partType, OSVERSIONINFOEX* ret, T a, T b) \
    { \
        int funx = M1xA_FV_EQUAL; \
\
        DWORDLONG const eq = VerSetConditionMask(0, partType, VER_EQUAL); \
        DWORDLONG const gt = VerSetConditionMask(0, partType, VER_GREATER); \
\
        T* p = part; \
\
        *p = (T)((a + b) / 2); \
\
        while ((funx = M1xA_testValue(ret, partType, eq, gt)) != M1xA_FV_EQUAL) \
        { \
            switch (funx) \
            { \
            case M1xA_FV_GREAT: \
                a = *p; \
                break; \
            case M1xA_FV_LESS: \
                b = *p; \
                break; \
            } \
\
            *p = (T)((a + b) / 2); \
\
            if (*p == a) \
            { \
                if (M1xA_testValue(ret, partType, eq, gt) == M1xA_FV_EQUAL) \
                    return TRUE; \
\
                *p = b; \
\
                if (M1xA_testValue(ret, partType, eq, gt) == M1xA_FV_EQUAL) \
                    return TRUE; \
\
                a = 0; \
                b = 0; \
                *p = 0; \
            } \
\
            if (a == b) \
            { \
                *p = 0; \
                return FALSE; \
            } \
        } \
\
        return TRUE; \
    }
M1xA_findPartTemplate(DWORD)
    M1xA_findPartTemplate(WORD)
        M1xA_findPartTemplate(BYTE)

            inline BOOL SalGetVersionEx(OSVERSIONINFOEX* osVer, BOOL versionOnly)
{
    BOOL ret = TRUE;
    ZeroMemory(osVer, sizeof(OSVERSIONINFOEX));
    osVer->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!versionOnly)
    {
        ret &= M1xA_findPartDWORD(&osVer->dwPlatformId, VER_PLATFORMID, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
    }
    ret &= M1xA_findPartDWORD(&osVer->dwMajorVersion, VER_MAJORVERSION, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
    ret &= M1xA_findPartDWORD(&osVer->dwMinorVersion, VER_MINORVERSION, osVer, M1xA_FV_MIN_VALUE, M1xA_FV_MINOR_VERSION_MAX_VALUE);
    if (!versionOnly)
    {
        ret &= M1xA_findPartDWORD(&osVer->dwBuildNumber, VER_BUILDNUMBER, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
        ret &= M1xA_findPartWORD(&osVer->wServicePackMajor, VER_SERVICEPACKMAJOR, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartWORD(&osVer->wServicePackMinor, VER_SERVICEPACKMINOR, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartWORD(&osVer->wSuiteMask, VER_SUITENAME, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartBYTE(&osVer->wProductType, VER_PRODUCT_TYPE, osVer, M1xA_FV_MIN_VALUE, MAXBYTE);
    }
    return ret;
}

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_base)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
