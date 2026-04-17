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

// the original SDK that shipped with VC6 defined this value as 0x00000040 (in 1998 the attribute did not yet exist; it was introduced with W2K)
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

// ****************************************************************************
// CSalamanderDebugAbstract
//
// set of Salamander methods used for debugging in both debug and release builds

// CALLSTK_MEASURETIMES macro - enables measurement of time spent preparing call-stack reports (measured as a ratio to
// the total function runtime)
// WARNING: must also be enabled separately for each plugin
// CALLSTK_DISABLEMEASURETIMES macro - suppresses measurement of time spent preparing call-stack reports in DEBUG/SDK/PB builds

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

    // sets the active thread name for TRACE; optional (the thread is then marked as "unknown")
    // WARNING: requires the thread to be registered with TRACE (see TraceAttachThread), otherwise it does nothing
    virtual void WINAPI TraceSetThreadName(const char* name) = 0;
    virtual void WINAPI TraceSetThreadNameW(const WCHAR* name) = 0;

    // initializes the thread with the things needed for CALL-STACK methods (see Push and Pop below);
    // in all called plugin methods, CALL_STACK methods can then be used directly;
    // this method is used only for new plugin threads;
    // it runs 'threadBody' with the 'param' parameter and returns the result of 'threadBody'
    virtual unsigned WINAPI CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param) = 0;

    // stores a message on the CALL-STACK ('format'+'args', see vsprintf); if the application crashes,
    // the CALL-STACK contents are written to the Bug Report window reporting the crash
    virtual void WINAPI Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                             BOOL doNotMeasureTimes) = 0;

    // removes the last message from the CALL-STACK; the call must be paired with Push
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

// ****************************************************************************
// CSalamanderRegistryAbstract
//
// set of Salamander methods for working with the system registry,
// used in CPluginInterfaceAbstract::LoadConfiguration
// and CPluginInterfaceAbstract::SaveConfiguration

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

// ****************************************************************************
// CSalamanderConnectAbstract
//
// set of Salamander methods for connecting a plugin to Salamander
// (custom pack/unpack + panel archiver view/edit + file viewer + menu items)

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

// macro for preparing 'hotKey' for AddMenuItem()
// tells Salamander that the menu item text contains a hot-key hint (separated by '\t')
// in this case Salamander will not complain via TRACE_E and will display the hot key in the Plugins menu
// WARNING: this is not a hot key that Salamander delivers to the plugin; it is really only a label
// if the user assigns this command a custom hot key in Plugin Manager, the hint is suppressed
#define SALHOTKEY_HINT ((DWORD)0x00020000)

class CSalamanderConnectAbstract
{
public:
    // adds the plugin to the list for "custom archiver pack",
    // 'title' is the user-visible name of the custom packer, 'defaultExtension' is the default extension
    // for new archives; if this is not an upgrade of "custom pack" (or the addition of the whole plugin) and
    // 'update' is FALSE, the call is ignored; if 'update' is TRUE, the settings are overwritten with the
    // new values of 'title' and 'defaultExtension' - it is necessary to prevent repeated 'update'==TRUE
    // (constant overwriting of the settings)
    virtual void WINAPI AddCustomPacker(const char* title, const char* defaultExtension, BOOL update) = 0;

    // adds the plugin to the list for "custom archiver unpack",
    // 'title' is the user-visible name of the custom unpacker, 'masks' are archive file masks (used to determine
    // which unpacker should be used to unpack a given archive; the separator is ';' (the escape sequence for ';' is
    // ";;") and the usual '*' and '?' wildcards are used, plus '#' for '0'..'9'); if this is not an upgrade of
    // "custom unpack" (or the addition of the whole plugin) and 'update' is FALSE, the call is ignored;
    // if 'update' is TRUE, the settings are overwritten with the new 'title' and 'masks' values; it is necessary to prevent
    // repeated 'update'==TRUE calls (continuous overwriting of the settings)
    virtual void WINAPI AddCustomUnpacker(const char* title, const char* masks, BOOL update) = 0;

    // adds the plugin to the list for "panel archiver view/edit";
    // 'extensions' are the archive extensions handled by this plugin
    // (the separator is ';' (';' has no escape sequence here), and the wildcard '#' is used for
    // '0'..'9'); if 'edit' is TRUE, this plugin handles "panel archiver view/edit", otherwise only
    // "panel archiver view"; if this is not an upgrade of "panel archiver view/edit" (or the addition
    // of the entire plugin) and 'updateExts' is FALSE, the call is ignored; if 'updateExts' is TRUE,
    // new archive extensions are added (ensuring that all extensions from 'extensions' are present) -
    // repeated 'updateExts'==TRUE must be prevented (to avoid constantly reviving extensions from 'extensions')
    virtual void WINAPI AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts) = 0;

    // removes an extension from the list for "panel archiver view/edit" (only from items belonging to
    // this plugin); 'extension' is a single archive extension (the '#' wildcard is used for '0'..'9');
    // repeated calls must be prevented (to avoid continually deleting 'extension')
    virtual void WINAPI ForceRemovePanelArchiver(const char* extension) = 0;

    // adds the plugin to the list for "file viewer",
    // 'masks' are the viewer masks that this plugin should handle
    // (the separator is ';' (the escape sequence for ';' is ";;") and the '*' and '?' wildcards are used;
    // if possible, avoid spaces, and the '|' character is forbidden (inverse masks are not allowed)),
    // if this is not an upgrade of "file viewer" (or the addition of the whole plugin) and 'force' is FALSE,
    // the call is ignored; if 'force' is TRUE, 'masks' are always added (if they are not already on the
    // list) - repeated 'force'==TRUE must be prevented (to avoid constantly adding 'masks')
    virtual void WINAPI AddViewer(const char* masks, BOOL force) = 0;

    // removes a mask from the list for "file viewer" (only from items belonging to this plugin);
    // 'mask' is a single viewer mask (the '*' and '?' wildcards are used); repeated calls must be prevented
    // (to avoid continually deleting 'mask')
    virtual void WINAPI ForceRemoveViewer(const char* mask) = 0;

    // adds items to the Plugins/"plugin name" menu in Salamander, 'iconIndex' is the
    // item icon index (-1=no icon; for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons; u separatoru se ignoruje), 'name' je
    // the item name (max. MAX_PATH - 1 characters), or NULL for a separator (the
    // 'state_or'+'state_and' parameters have no meaning in that case); 'hotKey' is the item's hot key
    // obtained with the SALHOTKEY macro; 'name' may contain a hot-key hint,
    // separated by '\t'; in that case the 'hotKey' variable must be assigned the
    // SALHOTKEY_HINT, vice viz komentar k SALHOTKEY_HINT; 'id' je unikatni identifikacni
    // item's unique identifier within the plugin (for a separator it matters only if 'callGetState' is TRUE),
    // if 'callGetState' is TRUE, the item state is obtained by calling
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (u separatoru ma vyznam jen stav
    // MENU_ITEM_STATE_HIDDEN, ostatni se ignoruji), jinak se k vypoctu stavu polozky (enabled/disabled)
    // is computed from 'state_or'+'state_and' - the item state is calculated by first building the mask
    // ('eventMask') as the logical OR of all events that occurred (see
    // MENU_EVENT_XXX); the item is enabled if the following expression is TRUE:
    //   ('eventMask' & 'state_or') != 0 && ('eventMask' & 'state_and') == 'state_and',
    // the 'skillLevel' parameter specifies for which user levels the item (or separator)
    // is displayed; the value contains one or more MENU_SKILLLEVEL_XXX constants ORed together;
    // menu items are updated on every plugin load (the items may change with configuration)
    // WARNING: use CSalamanderBuildMenuAbstract::AddMenuItem for a "dynamic menu extension"
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // adds a submenu to the Plugins/"plugin name" menu in Salamander, 'iconIndex'
    // is the submenu icon index (-1=no icon; for assigning a bitmap with icons
    // see CSalamanderConnectAbstract::SetBitmapWithIcons), 'name' is the submenu
    // name (max. MAX_PATH - 1 characters), 'id' is the unique menu-item identifier
    // within the plugin menu (for a submenu it matters only if 'callGetState' is TRUE),
    // if 'callGetState' is TRUE, the submenu state is obtained by calling
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (vyznam maji jen stavy
    // MENU_ITEM_STATE_ENABLED and MENU_ITEM_STATE_HIDDEN states matter; the others are ignored), otherwise
    // the item state (enabled/disabled) is computed from 'state_or'+'state_and' - for the state calculation
    // stavu polozky viz CSalamanderConnectAbstract::AddMenuItem(), parametr 'skillLevel'
    // specifies for which user levels the submenu is displayed; the value contains one or more
    // MENU_SKILLLEVEL_XXX constants ORed together; the submenu is closed by calling
    // CSalamanderConnectAbstract::AddSubmenuEnd();
    // menu items are updated on every plugin load (the items may change with configuration)
    // WARNING: use CSalamanderBuildMenuAbstract::AddSubmenuStart for a "dynamic menu extension"
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ends the submenu in the Plugins/"plugin name" menu in Salamander; subsequent items are added
    // to the higher-level (parent) menu;
    // menu items are updated on every plugin load (the items may change with configuration)
    // WARNING: use CSalamanderBuildMenuAbstract::AddSubmenuEnd for a "dynamic menu extension"
    virtual void WINAPI AddSubmenuEnd() = 0;

    // sets the item for FS in the Change Drive menu and in drive bars; 'title' is its text,
    // 'iconIndex' is the icon index (-1=no icon; for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons), 'title' muze obsahovat az tri sloupce
    // separated by '\t' (see the Alt+F1/F2 menu); item visibility can be set
    // from Plugins Manager or directly from the plugin by calling
    // CSalamanderGeneralAbstract::SetChangeDriveMenuItemVisibility
    virtual void WINAPI SetChangeDriveMenuItem(const char* title, int iconIndex) = 0;

    // informs Salamander that the plugin can load thumbnails from files matching the group mask
    // 'masks' (the separator is ';' (the escape sequence for ';' is ";;") and the '*' and '?' wildcards are used);
    // to load a thumbnail, Salamander calls
    // CPluginInterfaceForThumbLoaderAbstract::LoadThumbnail
    virtual void WINAPI SetThumbnailLoader(const char* masks) = 0;

    // sets the bitmap with plugin icons; Salamander copies the bitmap contents into its internal
    // structures, and the plugin is responsible for destroying the bitmap (on Salamander's side the
    // bitmap is used only during this function); the number of icons is derived from the
    // bitmap width, and the icons are always 16x16 pixels; the transparent part of the icons is
    // purple (RGB(255,0,255)); the bitmap color depth may be 4 or 8 bits (16 or 256
    // colors); ideally, prepare both color variants and choose between them according to the
    // vysledku metody CSalamanderGeneralAbstract::CanUse256ColorsBitmap()
    // WARNING: this method is obsolete, does not support alpha transparency; use
    //        SetIconListForGUI()
    virtual void WINAPI SetBitmapWithIcons(HBITMAP bitmap) = 0;

    // sets the plugin icon index used for the plugin in the Plugins/Plugins Manager window,
    // in the Help/About Plugin menu, and optionally also for the plugin submenu in the Plugins menu (for details
    // see CSalamanderConnectAbstract::SetPluginMenuAndToolbarIcon()); if the plugin does not call this
    // method, the standard Salamander icon for the plugin is used; 'iconIndex'
    // is the icon index to set (for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons)
    virtual void WINAPI SetPluginIcon(int iconIndex) = 0;

    // sets the icon index for the plugin submenu, used for the plugin submenu
    // in the Plugins menu and optionally also in the top toolbar for the drop-down button used
    // to display the plugin submenu; if the plugin does not call this method, the
    // plugin icon is used for the plugin submenu in the Plugins menu (see
    // CSalamanderConnectAbstract::SetPluginIcon) and no button appears in the top toolbar
    // for the plugin; 'iconIndex' is the icon index to set (-1=use the plugin icon,
    // see CSalamanderConnectAbstract::SetPluginIcon(); for assigning a bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons);
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

// ****************************************************************************
// CDynamicString
//
// dynamic string: reallocates as needed

class CDynamicString
{
public:
    // returns TRUE if the string 'str' of length 'len' was added successfully; if 'len' is -1,
    // 'len' is taken as "strlen(str)" (add without the terminating null); if 'len' is -2,
    // 'len' is taken as "strlen(str)+1" (add including the terminating null)
    virtual BOOL WINAPI Add(const char* str, int len = -1) = 0;
};

// ****************************************************************************
// CPluginInterfaceAbstract
//
// set of plugin methods that Salamander needs to work with the plugin
//
// For better clarity, separate parts are provided for:
// archivers - see CPluginInterfaceForArchiverAbstract,
// viewers - see CPluginInterfaceForViewerAbstract,
// menu extensions - see CPluginInterfaceForMenuExtAbstract,
// file systems - see CPluginInterfaceForFSAbstract,
// thumbnail loaders - see CPluginInterfaceForThumbLoaderAbstract.
// These parts are attached to CPluginInterfaceAbstract through CPluginInterfaceAbstract::GetInterfaceForXXX

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

    // called before the plugin is unloaded (only if SalamanderPluginEntry returned
    // this object and not NULL); returns TRUE if the unload may proceed,
    // 'parent' is the parent window for message boxes; 'force' is TRUE if the return
    // value is ignored; if it returns TRUE, this object and all other objects obtained from it
    // will no longer be used and the plugin will be unloaded; if a critical shutdown is in progress (see
    // CSalamanderGeneralAbstract::IsCriticalShutdown), nema smysl se usera na cokoliv ptat
    // (do not open any windows)
    // POZOR!!! Je nutne ukoncit vsechny thready pluginu (pokud Release vrati TRUE, vola se
    // FreeLibrary is called on the plugin .SPL, so the plugin code is unmapped from memory and the threads
    // have nothing left to run; usually neither a bug report nor Windows exception info is generated)
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

/*  RULES FOR IMPLEMENTING THE CONNECT METHOD
        (plugins must store the configuration version - see DEMOPLUGin,
         variable ConfigVersion and constant CURRENT_CONFIG_VERSION; below is
         an illustrative EXAMPLE of adding the "dmp2" extension to DEMOPLUGin):

      -with every change, increase the configuration version number - CURRENT_CONFIG_VERSION
       (in the first version of the Connect method, CURRENT_CONFIG_VERSION=1)
      -in the basic section (before the condition "if (ConfigVersion < YYY)"):
        -write code for plugin installation (the very first plugin load):
         see methods of CSalamanderConnectAbstract
        -during upgrades, update the extension lists used for installation for "custom archiver
         unpack" (AddCustomUnpacker), "panel archiver view/edit" (AddPanelArchiver),
         "file viewer" (AddViewer), menu items (AddMenuItem), etc.
        -for AddPanelArchiver and AddViewer, keep 'updateExts' and 'force' FALSE
         (otherwise we would force not only new but also old extensions on the user, even if
         they had removed them manually)
        -for AddCustomPacker/AddCustomUnpacker, pass an 'update' condition
         "ConfigVersion < XXX", where XXX is the last version number in which the
         custom packer/unpacker extensions changed (the two calls must be evaluated separately;
         for simplicity here we force all extensions back onto the user, so if they removed
         or added some, they will have to fix it manually again)
        -AddMenuItem, SetChangeDriveMenuItem and SetThumbnailLoader work the same on every plugin load
         (installation/upgrades do not differ - everything always starts from a clean slate)
      -only during upgrades: in the upgrade section (after the basic section):
        -add a condition "if (ConfigVersion < XXX)", where XXX is the new value
         of constant CURRENT_CONFIG_VERSION + add a comment for that version;
         in the body of that condition call:
          -if new extensions were added for "panel archiver", call
           "AddPanelArchiver(PPP, EEE, TRUE)", where PPP are only the new extensions separated
           by semicolons and EEE is TRUE/FALSE ("panel view+edit"/"panel view only")
          -if new extensions were added for "viewer", call "AddViewer(PPP, TRUE)",
           where PPP are only the new extensions separated by semicolons
          -if some old extensions for "viewer" must be removed, call
           "ForceRemoveViewer(PPP)" for each such extension PPP
          -if some old extensions for "panel archiver" must be removed, call
           "ForceRemovePanelArchiver(PPP)" for each such extension PPP

      CHECK: after these changes, I recommend testing that everything works as expected.
             It is enough to compile the plugin and try loading it into Salamander; there should
             be an automatic upgrade from the previous version (without needing
             to remove and add the plugin again):
             -see menu Options/Configuration:
               -Viewers are on the Viewers page: find the added extensions,
                check that the removed extensions no longer exist
               -Panel Archivers are on the Archives Associations in Panels page:
                find the added extensions
               -Custom Unpackers are on the Unpackers in Unpack Dialog Box page:
                find your plugin and check that the mask list is correct
             -check the new form of the plugin submenu (in the Plugins menu)
             -check the new form of the Change Drive menu (Alt+F1/F2)
             -check thumbnailer masks in Plugins Manager (in the Plugins menu):
              focus your plugin, then check the "Thumbnails" edit box
           +finally, you can also try removing and adding the plugin again to check whether
            plugin "installation" works: use all the checks listed above

      NOTE: when adding extensions for a "panel archiver", it is also necessary to update
            the extension list in the 'extensions' parameter of SetBasicPluginData

      EXAMPLE OF ADDING THE "dmp2" EXTENSION TO THE VIEWER AND ARCHIVER:
        (lines starting with "-" were removed, lines starting with "+" were added,
         the "=====" symbol at the start of a line means the continuous code section was truncated)
        Summary of changes:
          -the configuration version was increased from 2 to 3:
            -comment added for version 3
            -CURRENT_CONFIG_VERSION increased to 3
          -extension "dmp2" added to the 'extensions' parameter of SetBasicPluginData
           (because we are adding the "dmp2" extension for the "panel archiver")
          -mask "*.dmp2" added to AddCustomUnpacker + version in the condition increased from 1 to 3
           (because we are adding the "dmp2" extension for the "custom unpacker")
          -extension "dmp2" added to AddPanelArchiver (because we are adding extension
           "dmp2" for the "panel archiver")
          -mask "*.dmp2" added to AddViewer (because we are adding extension "dmp2"
           for the "viewer")
          -condition for upgrade to version 3 added + comment for this upgrade,
           body of the condition:
            -call AddPanelArchiver for extension "dmp2" with 'updateExts' TRUE
             (because we are adding extension "dmp2" for the "panel archiver")
            -call AddViewer for mask "*.dmp2" with 'force' TRUE (because
             we are adding extension "dmp2" for the "viewer")
=====
  // ConfigVersion: 0 - no configuration was loaded from the Registry (plugin installation),
  //                1 - first configuration version
  //                2 - second configuration version (some values added to the configuration)
+ //                3 - third configuration version (the "dmp2" extension added)

  int ConfigVersion = 0;
- #define CURRENT_CONFIG_VERSION 2
+ #define CURRENT_CONFIG_VERSION 3
  const char *CONFIG_VERSION = "Version";
=====
  // set basic plugin information
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

    // basic section:
    salamander->AddCustomPacker("DEMOPLUG (Plugin)", "dmp", FALSE);
-   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp", ConfigVersion < 1);
+   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp;*.dmp2", ConfigVersion < 3);
-   salamander->AddPanelArchiver("dmp", TRUE, FALSE);
+   salamander->AddPanelArchiver("dmp;dmp2", TRUE, FALSE);
-   salamander->AddViewer("*.dmp", FALSE);
+   salamander->AddViewer("*.dmp;*.dmp2", FALSE);
===== (I omitted adding menu items, setting icons and thumbnail masks)
    // upgrade section:
+   if (ConfigVersion < 3)   // version 3: the "dmp2" extension was added
+   {
+     salamander->AddPanelArchiver("dmp2", TRUE, TRUE);
+     salamander->AddViewer("*.dmp2", TRUE);
+   }
  }
=====
    */
    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) = 0;

    // releases the 'pluginData' interface that Salamander obtained from the plugin by calling
    // CPluginInterfaceForArchiverAbstract::ListArchive or
    // CPluginFSInterfaceAbstract::ListCurrentPath; before this call,
    // file and directory data (CFileData::PluginData) are released using methods of
    // CPluginDataInterfaceAbstract
    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) = 0;

    // returns the archiver interface; the plugin must return this interface if it has
    // at least one of the following functions (see SetBasicPluginData): FUNCTION_PANELARCHIVERVIEW,
    // FUNCTION_PANELARCHIVEREDIT, FUNCTION_CUSTOMARCHIVERPACK, and/or FUNCTION_CUSTOMARCHIVERUNPACK;
    // if the plugin does not contain an archiver, it returns NULL
    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() = 0;

    // returns the viewer interface; the plugin must return this interface if it has the function
    // (see SetBasicPluginData) FUNCTION_VIEWER; if the plugin does not contain a viewer, it returns NULL
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() = 0;

    // returns the menu-extension interface; the plugin must return this interface if it adds
    // menu items (see CSalamanderConnectAbstract::AddMenuItem) or if it has the
    // FUNCTION_DYNAMICMENUEXT function (see SetBasicPluginData); otherwise it returns NULL
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() = 0;

    // returns the file-system interface; the plugin must return this interface if it has the function
    // (see SetBasicPluginData) FUNCTION_FILESYSTEM; if the plugin does not contain a file system, it returns NULL
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() = 0;

    // returns the thumbnail-loader interface; the plugin must return this interface if it notified
    // Salamander that it can load thumbnails (see CSalamanderConnectAbstract::SetThumbnailLoader);
    // if the plugin cannot load thumbnails, it returns NULL
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() = 0;

    // receives various events; see the PLUGINEVENT_XXX event codes; called only when the plugin is
    // loaded; 'param' is the event parameter
    // WARNING: it may be called at any time after the plugin entry point (SalamanderPluginEntry) completes
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // user requested that all histories be cleared (by running Clear History from the configuration
    // History page); here, history means everything that arises automatically from values entered by the user
    // (e.g. the list of texts entered on the command line, the list of current paths on
    // individual drives, etc.); this does not include lists created by the user - e.g. hot paths, user menu,
    // etc.; 'parent' is the parent of any message boxes; after saving the configuration, histories must not
    // remain in the registry; if the plugin has open windows containing histories (combo boxes), it must
    // clear the histories there as well
    virtual void WINAPI ClearHistory(HWND parent) = 0;

    // receives notification of a change on the path 'path' (if 'includingSubdirs' is TRUE, it also
    // includes changes in subdirectories of 'path'); this method can be used, for example,
    // to invalidate or clear file/directory caches; NOTE: plugin file systems (FS) have the
    // CPluginFSInterfaceAbstract::AcceptChangeOnPathNotification() method
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // this method is called only for a plugin that uses Password Manager (see
    // CSalamanderGeneralAbstract::SetPluginUsesPasswordManager()):
    // it informs the plugin about changes in Password Manager; 'parent' is the parent of any
    // message boxes/dialogs; 'event' contains the event, see PME_XXX
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) = 0;
};

// ****************************************************************************
// CSalamanderPluginEntryAbstract
//
// set of Salamander methods used in SalamanderPluginEntry

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

    // sets the basic plugin data (data Salamander remembers about the plugin together with the DLL file name);
    // this call is required, otherwise the plugin cannot be connected;
    // 'pluginName' is the plugin name; 'functions' contains all functions supported by the plugin ORed
    // together (see FUNCTION_XXX constants); 'version'+'copyright'+'description' are values shown to the
    // user in the Plugins window; 'regKeyName' is the proposed name of the private key
    // for storing configuration in the registry (ignored without FUNCTION_LOADSAVECONFIGURATION);
    // 'extensions' are the basic extensions of handled archives (e.g. just "ARJ"; not "A01", etc.)
    // separated by ';' (here ';' has no escape meaning) - Salamander uses these extensions
    // only when searching for a replacement for removed panel archivers (this happens when the plugin is removed;
    // it solves the problem "which associated archiver should handle extension XXX now
    // that the original archiver from plugin PPP has been removed?") (ignored without
    // FUNCTION_PANELARCHIVERVIEW and FUNCTION_PANELARCHIVEREDIT); 'fsName' is the proposed name
    // (the actually assigned name is obtained by calling
    // CSalamanderGeneralAbstract::GetPluginFSName) of the file system (ignored without FUNCTION_FILESYSTEM;
    // allowed characters are 'a-zA-Z0-9_+-', minimum length 2 characters). If the plugin needs
    // more file-system names, it can use CSalamanderPluginEntryAbstract::AddFSName;
    // returns TRUE if the data was accepted successfully
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
    // for the same language Salamander is currently running in; if no such module is found (or
    // the version does not match), it lets the user choose an alternative module (if more
    // than one alternative exists and the user's choice from the previous plugin load is not already stored);
    // if no module is found, it returns NULL and the plugin should terminate;
    // 'parent' is the parent window for error message boxes and the dialog for selecting an alternative
    // language module; 'pluginName' is the plugin name (so the user knows which plugin
    // the error message or the alternative language module selection refers to)
    // WARNING: this method may be called only once; the returned handle to the language module
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
    // 'fsName' is the proposed file-system name (the assigned name can be obtained with
    // CSalamanderGeneralAbstract::GetPluginFSName); allowed characters are
    // 'a-zA-Z0-9_+-'; the minimum length is 2 characters; 'newFSNameIndex' (must not be NULL)
    // receives the index of the newly added file-system name; returns TRUE on success;
    // returns FALSE on a fatal error - in that case 'newFSNameIndex' is ignored
    // limitation: must not be called before SetBasicPluginData
    virtual BOOL WINAPI AddFSName(const char* fsName, int* newFSNameIndex) = 0;
};

// ****************************************************************************
// FSalamanderPluginEntry
//
// Open Salamander 1.6 or Later Plugin Entry Point Function Type,
// the plugin exports this function as "SalamanderPluginEntry", and Salamander calls it
// when the plugin is loaded to connect it;
// returns the plugin interface if the connection succeeds, otherwise NULL;
// the plugin interface is released by calling its Release method before the plugin is unloaded

typedef CPluginInterfaceAbstract*(WINAPI* FSalamanderPluginEntry)(CSalamanderPluginEntryAbstract* salamander);

// ****************************************************************************
// FSalamanderPluginGetReqVer
//
// Open Salamander 2.5 Beta 2 or Later Plugin Get Required Version of Salamander Function Type,
// the plugin exports this function as "SalamanderPluginGetReqVer", and Salamander calls it
// as the first plugin function (before "SalamanderPluginGetSDKVer" and "SalamanderPluginEntry")
// when the plugin is loaded;
// returns the Salamander version the plugin was built for (the oldest version into which the plugin can be loaded)

typedef int(WINAPI* FSalamanderPluginGetReqVer)();

// ****************************************************************************
// FSalamanderPluginGetSDKVer
//
// Open Salamander 2.52 beta 2 (PB 22) or Later Plugin Get SDK Version Function Type,
// the plugin may optionally export this function as "SalamanderPluginGetSDKVer", and Salamander
// tries to call it as the second plugin function (before "SalamanderPluginEntry")
// when the plugin is loaded;
// returns the SDK version used to build the plugin (it tells Salamander which methods
// the plugin provides); exporting "SalamanderPluginGetSDKVer" only makes sense if
// "SalamanderPluginGetReqVer" returns a number smaller than LAST_VERSION_OF_SALAMANDER; it is best
// to return LAST_VERSION_OF_SALAMANDER directly

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
