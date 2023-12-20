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
#pragma pack(push, enter_include_spl_base) // so that the strucures are independent of the alignment setting
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// in debug version, we will test if source and destination memory overlap (memcpy must not overlap)
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

// following functions do not crash when working with invalid memory (or with NULL):
// lstrcpy, lstrcpyn, lstrlen and lstrcat (they are defined with A or W suffix, so we do not
// redefine them), for easier debugging of errors we need them to crash, because otherwise
// the error will be detected later in place where it is not clear what caused it
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

// original SDK which was a part of VC6 had this value defined as 0x00000040 (year 1998, when the attribute was not used yet, it was introduced with W2K)
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
// set of methods from Salamander which are used for debugging in both debug and release version

// macro CALLSTK_MEASURETIMES - enables measuring of time spent by preparing call-stack messages (ratio against
//                              total time of functions is measured)
//                              CAUTION: must be enabled for each plugin separately
// macro CALLSTK_DISABLEMEASURETIMES - suppresses measuring of time spent by preparing call-stack messages in
//                                     DEBUG/SDK/PB version

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext
{
    DWORD PushesCounterStart;                      // start-state of counter of time spent by Push methods called in this thread
    LARGE_INTEGER PushPerfTimeCounterStart;        // start-state of counter of time spent by Push methods called in this thread
    LARGE_INTEGER IgnoredPushPerfTimeCounterStart; // start-state of counter of time spent by ignored Push methods called in this thread
    LARGE_INTEGER StartTime;                       // "time" of Push of this call-stack macro
    DWORD_PTR PushCallerAddress;                   // the address of CALL_STACK_MESSAGE macro (Push address)
};
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

class CSalamanderDebugAbstract
{
public:
    // prints 'file'+'line'+'str' TRACE_I to TRACE SERVER - only for DEBUG/SDK/PB version of Salamander
    virtual void WINAPI TraceI(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceIW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // prints 'file'+'line'+'str' TRACE_E to TRACE SERVER - only for DEBUG/SDK/PB version of Salamander
    virtual void WINAPI TraceE(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceEW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // registers new thread at TRACE (assigns Unique ID), 'thread'+'tid' is returned by
    // _beginthreadex and CreateThread, optional (UID is -1 then)
    virtual void WINAPI TraceAttachThread(HANDLE thread, unsigned tid) = 0;

    // sets the name of active thread for TRACE, optional (thread is marked as "unknown")
    // CAUTION: requires registration of thread at TRACE (see TraceAttachThread), otherwise does nothing
    virtual void WINAPI TraceSetThreadName(const char* name) = 0;
    virtual void WINAPI TraceSetThreadNameW(const WCHAR* name) = 0;

    // loads into thread the things needed for CALL-STACK methods (see Push and Pop below),
    // all called methods can use CALL-STACK methods directly, this method is used only
    // for new threads of plugin, calls fuction 'threadBody' with parameter 'param', returns result of 'threadBody'
    virtual unsigned WINAPI CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param) = 0;

    // stores a message to CALL_STACK ('format'+'args' see vsprintf), when application crashes
    // the content of CALL_STACK is printed into Bug Report window reporting the crash of application
    virtual void WINAPI Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                             BOOL doNotMeasureTimes) = 0;

    // removes last message from CALL-STACK, call must match with Push
    virtual void WINAPI Pop(CCallStackMsgContext* callStackMsgContext) = 0;

    // sets the name of active thread for VC debugger
    virtual void WINAPI SetThreadNameInVC(const char* name) = 0;

    // calls TraceSetThreadName and SetThreadNameInVC for 'name' (for description see these two methods)
    virtual void WINAPI SetThreadNameInVCAndTrace(const char* name) = 0;

    // if we are not connected to Trace Server anymore, tries to connect to it
    // (server must be running). Only SDK version of Salamander (including Preview Builds):
    // if autostart of server is enabled and server is not running (e.g. user terminated it),
    // tries to start it before connecting to it.
    virtual void WINAPI TraceConnectToServer() = 0;

    // called for modules in which memory leaks can be reported, if memory leaks are detected,
    // it is loaded "as image" (without module initialization) all registered modules (during
    // memory leak check these modules are unloaded), and then memory leaks are reported =
    // names of .cpp modules are visible instead of "#File Error#"
    // can be called from any thread
    virtual void WINAPI AddModuleWithPossibleMemoryLeaks(const char* fileName) = 0;
};

//
// ****************************************************************************
// CSalamanderRegistryAbstract
//
// set of methods of Salamander for working with system registry,
// used in CPluginInterfaceAbstract::LoadConfiguration
// and CPluginInterfaceAbstract::SaveConfiguration

class CSalamanderRegistryAbstract
{
public:
    // cleans up registry key 'key' from all subkeys and values, returns success
    virtual BOOL WINAPI ClearKey(HKEY key) = 0;

    // creates or opens existing subkey 'name' of key 'key', returns 'createdKey' and success;
    // the obtained key ('createdKey') must be closed by calling CloseKey
    virtual BOOL WINAPI CreateKey(HKEY key, const char* name, HKEY& createdKey) = 0;

    // opens existing subkey 'name' of key 'key', returns 'openedKey' and success;
    // the obtained key ('openedKey') must be closed by calling CloseKey
    virtual BOOL WINAPI OpenKey(HKEY key, const char* name, HKEY& openedKey) = 0;

    // closes key opened by OpenKey or CreateKey
    virtual void WINAPI CloseKey(HKEY key) = 0;

    // deletes subkey 'name' of key 'key', returns success
    virtual BOOL WINAPI DeleteKey(HKEY key, const char* name) = 0;

    // loads value 'name'+'type'+'buffer'+'bufferSize' from key 'key', returns success
    virtual BOOL WINAPI GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize) = 0;

    // saves value 'name'+'type'+'buffer'+'bufferSize' to key 'key', for strings it is possible
    // to specify 'dataSize' == -1 -> length of string is calculated by strlen, returns success
    virtual BOOL WINAPI SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize) = 0;

    // deletes value 'name' of key 'key', returns success
    virtual BOOL WINAPI DeleteValue(HKEY key, const char* name) = 0;

    // gets size needed for value 'name'+'type' from key 'key' into 'bufferSize', returns success
    virtual BOOL WINAPI GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize) = 0;
};

//
// ****************************************************************************
// CSalamanderConnectAbstract
//
// set of methods of Salamander for connecting plugin to Salamander
// (custom pack/unpack + panel archiver view/edit + file viewer + menu-items)

// constants for CSalamanderConnectAbstract::AddMenuItem
#define MENU_EVENT_TRUE 0x0001                    // happens always
#define MENU_EVENT_DISK 0x0002                    // the source is a windows directory ("c:\path" or UNC)
#define MENU_EVENT_THIS_PLUGIN_ARCH 0x0004        // the source is an archive of this plugin
#define MENU_EVENT_THIS_PLUGIN_FS 0x0008          // the source is file-system of this plugin
#define MENU_EVENT_FILE_FOCUSED 0x0010            // focus is on a file
#define MENU_EVENT_DIR_FOCUSED 0x0020             // focus is on a directory
#define MENU_EVENT_UPDIR_FOCUSED 0x0040           // focus is on ".."
#define MENU_EVENT_FILES_SELECTED 0x0080          // files are selected
#define MENU_EVENT_DIRS_SELECTED 0x0100           // directories are selected
#define MENU_EVENT_TARGET_DISK 0x0200             // the target is a windows directory ("c:\path" or UNC)
#define MENU_EVENT_TARGET_THIS_PLUGIN_ARCH 0x0400 // the target is an archive of this plugin
#define MENU_EVENT_TARGET_THIS_PLUGIN_FS 0x0800   // the target is file-system of this plugin
#define MENU_EVENT_SUBDIR 0x1000                  // the directory is not root (contains "..")
// focus is on the file, for which this plugin provides "panel archiver view" or "panel archiver edit"
#define MENU_EVENT_ARCHIVE_FOCUSED 0x2000
// only 0x4000 is available (the mask is composed of two DWORDs and before it is masked by 0x7FFF)

// for which user is the item intended
#define MENU_SKILLLEVEL_BEGINNER 0x0001     // for the most important menu items, intended for beginners
#define MENU_SKILLLEVEL_INTERMEDIATE 0x0002 // set for less frequent commands; for advanced users
#define MENU_SKILLLEVEL_ADVANCED 0x0004     // set for all commands (pros should have all commands in menu)
#define MENU_SKILLLEVEL_ALL 0x0007          // helper constant including all above

// macro for preparing 'HotKey' for AddMenuItem()
// LOWORD - hot key (virtual key + modifiers) (LOBYTE - virtual key, HIBYTE - modifiers)
// mods: combination of HOTKEYF_CONTROL, HOTKEYF_SHIFT, HOTKEYF_ALT
// examples: SALHOTKEY('A', HOTKEYF_CONTROL | HOTKEYF_SHIFT), SALHOTKEY(VK_F1, HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_EXT)
//#define SALHOTKEY(vk,mods,cst) ((DWORD)(((BYTE)(vk)|((WORD)((BYTE)(mods))<<8))|(((DWORD)(BYTE)(cst))<<16)))
#define SALHOTKEY(vk, mods) ((DWORD)(((BYTE)(vk) | ((WORD)((BYTE)(mods)) << 8))))

// macro for preparing 'hotKey' for AddMenuItem()
// tells Salamander that the menu item will contain a hot key (separated by '\t')
// Salamander will not shout using TRACE_E in this case and will display the hot key in the Plugins menu
// CAUTION: it is not a hot key delivered by Salamander to the plugin, it is really only a text
// if user assigns a hot key to this command in Plugin Manager, the hint will be suppressed
#define SALHOTKEY_HINT ((DWORD)0x00020000)

class CSalamanderConnectAbstract
{
public:
    // adding the plugin to the list for "custom archiver pack",
    // 'title' is the name of custom packer for user, 'defaultExtension' is the standard extension
    // for new archives, if it is not an upgrade of "custom pack" (or adding the whole plugin) and
    // 'update' is FALSE, the call is ignored; if 'update' is TRUE, the settings are overwritten
    // with new values 'title' and 'defaultExtension' - necessary prevention against repeated
    // 'update'==TRUE (constant overwriting of settings)
    virtual void WINAPI AddCustomPacker(const char* title, const char* defaultExtension, BOOL update) = 0;

    // adding the plugin to the list for "custom archiver unpack",
    // 'title' is the name of custom unpacker for user, 'masks' are the masks of archive files
    // (searching for the right unpacker is done according to them, separator is ';' (escape
    // sequence for ';' is ";;") and wildcards '*' and '?' are used, plus '#' for '0'..'9'),
    // if it is not an upgrade of "custom unpack" (or adding the whole plugin) and 'update' is
    // FALSE, the call is ignored; if 'update' is TRUE, the settings are overwritten with new
    // values 'title' and 'masks' - necessary prevention against repeated 'update'==TRUE (constant
    // overwriting of settings)
    virtual void WINAPI AddCustomUnpacker(const char* title, const char* masks, BOOL update) = 0;

    // adding the plugin to the list for "panel archiver view/edit",
    // 'extensions' are the extensions of archives which are processed by this plugin
    // (separator is ';' (there is no escape sequence for ';' here) and wildcard '#' is used for
    // '0'..'9'), if 'edit' is TRUE, this plugin processes "panel archiver view/edit", otherwise
    // only "panel archiver view", if it is not an upgrade of "panel archiver view/edit" (or adding
    // the whole plugin) and 'updateExts' is FALSE, the call is ignored; if 'updateExts' is TRUE,
    // it is adding new extensions of archives (ensuring the presence of all extensions from
    // 'extensions') - necessary prevention against repeated 'updateExts'==TRUE (constant
    // refreshing of extensions from 'extensions')
    virtual void WINAPI AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts) = 0;

    // removing the extension from the list for "panel archiver view/edit" (only from items
    // concerning this plugin), 'extension' is the extension of archive (only one; wildcard '#'
    // is used for '0'..'9'), necessary prevention against repeated calls (constant deleting of
    // 'extension')
    virtual void WINAPI ForceRemovePanelArchiver(const char* extension) = 0;

    // adding the plugin to the list for "file viewer",
    // 'masks' are the extensions of viewer which are processed by this plugin
    // (separator is ';' (escape sequence for ';' is ";;") and wildcards '*' and '?' are used,
    // if not necessary do not use spaces + '|' is forbidden (inverted masks are not allowed)),
    // if it is not an upgrade of "file viewer" (or adding the whole plugin) and 'force' is FALSE,
    // the call is ignored; if 'force' is TRUE, 'masks' are always added (if they are not already
    // on the list) - necessary prevention against repeated 'force'==TRUE (constant adding of
    // 'masks')
    virtual void WINAPI AddViewer(const char* masks, BOOL force) = 0;

    // removing the extension from the list for "file viewer" (only from items concerning this
    // plugin), 'mask' is the extension of viewer (only one; wildcard '*' and '?' are used),
    // necessary prevention against repeated calls (constant deleting of 'mask')
    virtual void WINAPI ForceRemoveViewer(const char* mask) = 0;

    // adding items to the menu Plugins/"plugin name" in Salamander, 'iconIndex' is the index
    // of icon of item (-1=no icon; setting bitmap with icons see CSalamanderConnectAbstract::SetBitmapWithIcons),
    // ignored for separator, 'name' is the name of item (max. MAX_PATH - 1 characters) or NULL
    // for separator (parameters 'state_or'+'state_and' have no meaning in this case); 'hotKey'
    // is the hot key of item obtained by macro SALHOTKEY; 'name' can contain hint for hot key
    // separated by '\t', in variable 'hotKey' must be assigned constant SALHOTKEY_HINT in this
    // case, see comment to SALHOTKEY_HINT; 'id' is the unique identification number of item
    // in the plugin (for separator it has meaning only if 'callGetState' is TRUE), if 'callGetState'
    // is TRUE, method CPluginInterfaceForMenuExtAbstract::GetMenuItemState is called for
    // determining the state of item (for separator it has meaning only for state MENU_ITEM_STATE_HIDDEN,
    // other states are ignored), otherwise 'state_or'+'state_and' are used for calculating the
    // state of item (enabled/disabled) - for calculating the state of item, first the mask
    // ('eventMask') is created by logically adding all events which happened (events see
    // MENU_EVENT_XXX), item will be "enable" if the following expression is TRUE:
    // ('eventMask' & 'state_or') != 0 && ('eventMask' & 'state_and') == 'state_and',
    // parameter 'skillLevel' determines for which user levels the item (or separator) will be
    // displayed; the value contains one or more (ORed) constants MENU_SKILLLEVEL_XXX;
    // items in menu are updated at every load of plugin (possible change of items according
    // to configuration)
    // CAUTION: for "dynamic menu extension" CSalamanderBuildMenuAbstract::AddMenuItem is used
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // adding submenu to the menu Plugins/"plugin name" in Salamander, 'iconIndex' is the index
    // of icon of submenu (-1=no icon; setting bitmap with icons see CSalamanderConnectAbstract::SetBitmapWithIcons),
    // 'name' is the name of submenu (max. MAX_PATH - 1 characters), 'id' is the unique
    // identification number of item in the plugin (for submenu it has meaning only if
    // 'method CPluginInterfaceForMenuExtAbstract::GetMenuItemState is called for determining
    // 'callGetState' is TRUE), if 'callGetState' is TRUE, method CPluginInterfaceForMenuExtAbstract::GetMenuItemState
    // is called for determining the state of submenu (for submenu only states MENU_ITEM_STATE_ENABLED
    // and MENU_ITEM_STATE_HIDDEN have meaning, other states are ignored), otherwise 'state_or'+'state_and'
    // are used for calculating the state of submenu (enabled/disabled) - for calculating the state
    // see CSalamanderConnectAbstract::AddMenuItem(), parameter 'skillLevel' determines for which
    // user levels the submenu will be displayed, the value contains one or more (ORed) constants
    // MENU_SKILLLEVEL_XXX, submenu is ended by calling CSalamanderConnectAbstract::AddSubmenuEnd();
    // items in menu are updated at every load of plugin (possible change of items according
    // to configuration)
    // CAUTION: for "dynamic menu extension" CSalamanderBuildMenuAbstract::AddSubmenuStart is used
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ending submenu in the menu Plugins/"plugin name" in Salamander, other items will be
    // added to higher (parent) level of menu;
    // items in menu are updated at every load of plugin (possible change of items according
    // to configuration)
    // CAUTION: for "dynamic menu extension" CSalamanderBuildMenuAbstract::AddSubmenuEnd is used
    virtual void WINAPI AddSubmenuEnd() = 0;

    // sets an item for FS in Change Drive menu and in Drive bars; 'title' is its text,
    // 'iconIndex' is the index of its icon (-1=no icon; setting bitmap with icons see
    // CSalamanderConnectAbstract::SetBitmapWithIcons), 'title' can contain up to three columns
    // separated by '\t' (see Alt+F1/F2 menu); visibility of item can be set from Plugins
    // Manager or directly from plugin using method
    // CSalamanderGeneralAbstract::SetChangeDriveMenuItemVisibility
    virtual void WINAPI SetChangeDriveMenuItem(const char* title, int iconIndex) = 0;

    // informs Salamander that the plugin is able to load thumbnails from files
    // corresponding to group mask 'masks' (separator is ';' (escape sequence for ';' is ";;")
    // and wildcards '*' and '?' are used); for loading thumbnails method
    // CPluginInterfaceForThumbLoaderAbstract::LoadThumbnail is called
    virtual void WINAPI SetThumbnailLoader(const char* masks) = 0;

    // sets bitmap with icons for plugin, Salamander copies the content of bitmap to its
    // internal structures, plugin is responsible for destruction of bitmap (from the side
    // of Salamander the bitmap is used only during this function); number of icons is
    // derived from the width of bitmap, icons are always 16x16 pixels; transparent part
    // of icons is violet color (RGB(255,0,255)), color depth of bitmap can be 4 or 8 bits
    // (16 or 256 colors), it is ideal to have both color variants prepared and to choose
    // from them according to the result of method CSalamanderGeneralAbstract::CanUse256ColorsBitmap()
    // WARNING: this method is obsolete, it does not support alpha transparency, use
    //          SetIconListForGUI() instead
    virtual void WINAPI SetBitmapWithIcons(HBITMAP bitmap) = 0;

    // sets index of icon for plugin, which is used for plugin in menu Plugins/Plugins Manager,
    // in menu Help/About Plugin and possibly also for submenu of plugin in menu Plugins
    // (details see CSalamanderConnectAbstract::SetPluginMenuAndToolbarIcon()); if plugin does
    // not call this method, standard Salamander icon for plugin is used; 'iconIndex' is the
    // index of icon (setting bitmap with icons see CSalamanderConnectAbstract::SetBitmapWithIcons)
    virtual void WINAPI SetPluginIcon(int iconIndex) = 0;

    // sets index of icon for plugin submenu, which is used for plugin submenu in menu Plugins
    // and possibly also in the top toolbar for drop-down button for displaying submenu of plugin;
    // if plugin does not call this method, plugin icon is used for plugin submenu (for settings
    // CSalamanderConnectAbstract::SetPluginIcon) and in the top toolbar is no button for plugin
    // 'iconIndex' is the index of icon (-1=plugin icon is used, see CSalamanderConnectAbstract::SetPluginIcon);
    // setting bitmap with icons see CSalamanderConnectAbstract::SetBitmapWithIcons
    virtual void WINAPI SetPluginMenuAndToolbarIcon(int iconIndex) = 0;

    // sets bitmap with icons for plugin, bitmaps has to be allocated by calling
    // CSalamanderGUIAbstract::CreateIconList() and then created and filled by methods
    // CGUIIconListAbstract interface; icons must have size 16x16 pixels; Salamander
    // takes the bitmap object into its management, plugin must not destroy it after
    // calling this function; bitmap is stored in Salamander configuration, so that
    // icons can be used without loading the plugin again, so put into it only icons
    // which are really needed
    virtual void WINAPI SetIconListForGUI(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CDynamicString
//
// dynamic string: it is reallocated according to need

class CDynamicString
{
public:
    // returns TRUE if the string 'str' with length 'len' was successfully added; if 'len' == -1,
    // 'len' is determined as "strlen(str)" (adding without terminating zero); if 'len' == -2,
    // 'len' is determined as "strlen(str)+1" (adding with terminating zero)
    virtual BOOL WINAPI Add(const char* str, int len = -1) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceAbstract
//
// set of methods of plugin which Salamander needs for working with plugin
//
// For better overview, the interface is divided into parts for:
// archivers - see CPluginInterfaceForArchiverAbstract,
// viewers - see CPluginInterfaceForViewerAbstract,
// menu extensions - see CPluginInterfaceForMenuExtAbstract,
// file-systems - see CPluginInterfaceForFSAbstract,
// thumbnail loaders - see CPluginInterfaceForThumbLoaderAbstract.
// The parts are connected to CPluginInterfaceAbstract through CPluginInterfaceAbstract::GetInterfaceForXXX

// flags indicating which functions the plugin provides (which methods of CPluginInterfaceAbstract
// are implemented in the plugin):
#define FUNCTION_PANELARCHIVERVIEW 0x0001     // methods for "panel archiver view"
#define FUNCTION_PANELARCHIVEREDIT 0x0002     // methods for "panel archiver edit"
#define FUNCTION_CUSTOMARCHIVERPACK 0x0004    // methods for "custom archiver pack"
#define FUNCTION_CUSTOMARCHIVERUNPACK 0x0008  // methods for "custom archiver unpack"
#define FUNCTION_CONFIGURATION 0x0010         // Configuration method
#define FUNCTION_LOADSAVECONFIGURATION 0x0020 // methods for "load/save configuration"
#define FUNCTION_VIEWER 0x0040                // methods for "file viewer"
#define FUNCTION_FILESYSTEM 0x0080            // methods for "file system"
#define FUNCTION_DYNAMICMENUEXT 0x0100        // methods for "dynamic menu extension"

// flags for various events (and the meaning of 'param' parameter), accepted by method CPluginInterfaceAbstract::Event
// color change occurred (due to change of system colors / WM_SYSCOLORCHANGE or due to change in configuration);
// plugin can get new versions of Salamander colors through CSalamanderGeneralAbstract::GetCurrentColor;
// if plugin has file-system with icons of type pitFromPlugin, it should repaint background of image-list
// with simple icons to color SALCOL_ITEM_BK_NORMAL; 'param' is ignored here
#define PLUGINEVENT_COLORSCHANGED 0

// Salamander configuration has been changed; plugin can get new versions of Salamander configuration
// parameters through CSalamanderGeneralAbstract::GetConfigParameter; 'param' is ignored here
#define PLUGINEVENT_CONFIGURATIONCHANGED 1

// left and right panels have been swapped (Swap Panels - Ctrl+U); 'param' is ignored here
#define PLUGINEVENT_PANELSSWAPPED 2

// active panel has been changed (switching between panels); 'param' is PANEL_LEFT or PANEL_RIGHT
// indicating activated panel
#define PLUGINEVENT_PANELACTIVATED 3

// Salamander received WM_SETTINGCHANGE and regenerated fonts for toolbars according to it.
// Then it sends this event to all plugins, so that they can call their toolbar lists'
// method SetFont();
// 'param' is ignored here
#define PLUGINEVENT_SETTINGCHANGE 4

// codes of events in Password Manager, accepted by method CPluginInterfaceAbstract::PasswordManagerEvent():
#define PME_MASTERPASSWORDCREATED 1 // user created master password (passwords must be encrypted)
#define PME_MASTERPASSWORDCHANGED 2 // user changed master password (passwords must be decrypted and encrypted again)
#define PME_MASTERPASSWORDREMOVED 3 // user removed master password (passwords must be decrypted)

class CPluginInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calling of methods (see CPluginInterfaceEncapsulation)
    friend class CPluginInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // called as a reaction to the About button in the Plugins window or to the command from the Help/About Plugins menu
    virtual void WINAPI About(HWND parent) = 0;

    // called before unloading plugin (of course only if SalaamanderPluginEntry returned this object and not NULL),
    // returns TRUE if unload can be done, 'parent' is parent messagebox, 'force' is TRUE if return value
    // is not taken into account, if it returns TRUE, this object and all other objects obtained from it
    // will not be used anymore and the plugin will be unloaded; if critical shutdown is in progress
    // (see CSalamanderGeneralAbstract::IsCriticalShutdown for more info), it does not make sense to
    // ask user anything (no windows will be opened)
    // CAUTION!!! It is necessary to terminate all threads of the plugin (if Release returns TRUE,
    // FreeLibrary is called on plugin .SPL => plugin code is unmapped from memory => threads
    // have nothing to run => usually neither bug-report nor Windows exception info is shown)
    virtual BOOL WINAPI Release(HWND parent, BOOL force) = 0;

    // function for loading default configuration and for "load/save configuration" (load from private
    // key of plugin in registry), 'parent' is parent of messagebox, if 'regKey' == NULL, it is default
    // configuration, 'registry' is object for working with registry, this method is called always after
    // SalamanderPluginEntry and before other calls (load from private key is called if this function
    // is provided by plugin and the key in registry exists, otherwise only load of default configuration
    // is called)
    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // function for "load/save configuration", called for saving configuration of plugin to private key
    // of plugin in registry, 'parent' is parent of messagebox, 'registry' is object for working with
    // registry, if Salamander is saving configuration, this method is also called (if provided by plugin)
    // Salamander also offers saving configuration of plugin when unloading it (e.g. manually from Plugins
    // Manager), in this case the saving is done only if the key in registry exists
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // called as a reaction to the Configuration button in the Plugins window
    // vola se jako reakce na tlacitko Configurate v okne Plugins
    virtual void WINAPI Configuration(HWND parent) = 0;

    // called to attaching plugin to Salamander, called after LoadConfiguration, 'parent' is parent
    // of messagebox, 'salamander' is set of methods for attaching plugin

    /*  RULES FOR IMPLEMENTATION OF METHOD CONNECT
        (plugins must have saved version of configuration - see DEMOPLUGin,
         variable ConfigVersion and constant CURRENT_CONFIG_VERSION; below is
         an example of adding extension "dmp2" to DEMOPLUGin):

      -  with each change of configuration is necessary to increase the number of version
         of configuration - CURRENT_CONFIG_VERSION (in the first version of method Connect
         is CURRENT_CONFIG_VERSION=1)
      -  to basic part (before conditions "if (ConfigVersion < YYY)"):
      -  code for installation of plugin is written (first load of plugin):
           see methods CSalamanderConnectAbstract
        -  for upgrades it is necessary to update lists of extensions for installation
           for "custom archiver unpack" (AddCustomUnpacker), "panel archiver view/edit"
           (AddPanelArchiver), "file viewer" (AddViewer), menu items (AddMenuItem), etc.
        -  when calling AddPanelArchiver and AddViewer we leave 'updateExts' and 'force' FALSE
           (otherwise we would force user not only new, but also old extensions, which he
           may have deleted manually)
        -  when calling AddCustomPacker/AddCustomUnpacker we put into parameter 'update'
           condition "ConfigVersion < XXX", where XXX is the number of last version, where
           extensions for custom packers/unpackers were changed (both calls must be judged
           separately; here for simplicity we force user all extensions, if he deleted
           or added some, he has bad luck, he will have to do it manually again)
        -  AddMenuItem, SetChangeDriveMenuItem and SetThumbnailLoader works with every load
           of plugin the same (installation/upgrades do not differ - it always starts from scratch)

      - during upgrades only: into the part for upgrades (for the basic part):
        - we will add condition "if (ConfigVersion < XXX)", where XXX is the new value of constant
          CURRENT_CONFIG_VERSION + we will add comment from this version;
          into the body of this condition we will call:
             -if new extensions for "panel archiver" were added, we will call
                "AddPanelArchiver(PPP, EEE, TRUE)", where PPP are only new extensions separated
                by semicolon and EEE is TRUE/FALSE ("panel view+edit"/"only panel view")
             -if new extensions for "viewer" were added, we will call "AddViewer(PPP, TRUE)",
                where PPP are only new extensions separated by semicolon
             -if some old extensions for "viewer" should be deleted, we will call for each
                such extension PPP "ForceRemoveViewer(PPP)"
             -if some old extensions for "panel archiver" should be deleted, we will call for
                each such extension PPP "ForceRemovePanelArchiver(PPP)"

        CHECK: after these changes I recommend to test, if it works as it should,
               it is enough to compile the plugin and try to load it into Salamander,
               there should be automatic upgrade from previous version (without need
               to remove and add the plugin):
               -see menu Options/Configuration:
                 -Viewers are on page Viewers: you will find added extensions,
                   check that removed extensions do not exist anymore
                 -Panel Archivers are on page Archives Associations in Panels:
                   you will find added extensions
                 -Custom Unpackers are on page Unackers in Unpack Dialog Box:
                   you will find your plugin and check if the list of masks is OK
               -check new submenu of plugin (in menu Plugins)
               -check new Change Drive menu (Alt+F1/F2)
               -check in Plugins Manager (in menu Plugins) masks of thumbnailer:
                focus your plugin, then check editbox "Thumbnails"
               +finally you can also try to remove and add the plugin, if the "installation"
                of plugin works: check all previous points
           
      NOTE: when adding extensions for "panel archiver" it is necessary to add the list of extensions
            in 'extensions' parameter of SetBasicPluginData method

      EXAMPLE OF ADDING EXTENSION "DMP2" TO VIEWER AND ARCHIVER:
        (lines starting with "-" were removed, lines starting with "+" were added,
         symbol "=====" at the beginning of line means break of continuous code)
        List of changes:
          -version of configuration was increased from 2 to 3:
            -comment for version 3 was added
            -increase of CURRENT_CONFIG_VERSION to 3
          -extension "dmp2" was added to 'extensions' parameter of SetBasicPluginData
           (because we add extension "dmp2" for "panel archiver")
          -mask "*.dmp2" was added to AddCustomUnpacker + increase of version from 1 to 3
           in condition (because we add extension "dmp2" for "panel archiver")
          -extension "dmp2" was added to AddPanelArchiver (because we add extension "dmp2"
           for "panel archiver")
          -mask "*.dmp2" was added to AddViewer (because we add extension "dmp2" for "viewer")
          -condition for upgrade to version 3 was added + comment for this upgrade,
             body of condition:
                -calling AddPanelArchiver for extension "dmp2" with 'updateExts' TRUE
                 (because we add extension "dmp2" for "panel archiver")
                -calling AddViewer for mask "*.dmp2" with 'force' TRUE (because we add extension
                 "dmp2" for "viewer")
=====
  // ConfigVersion: 0 - no configuration was loaded from Registry (it is installation of plugin),
  //                1 - first version of configuration
  //                2 - second version of configuration (some values were added to configuration)
+ //                3 - third version of configuration (adding extension "dmp2")

  int ConfigVersion = 0;
- #define CURRENT_CONFIG_VERSION 2
+ #define CURRENT_CONFIG_VERSION 3
  const char *CONFIG_VERSION = "Version";
=====
  // setting the basic information about plugin
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
===== (I skipped adding items to menu, setting icons and masks of thumbnails)
    // part for upgrades:
+   if (ConfigVersion < 3)   // version 3: adding extension "dmp2"
+   {
+     salamander->AddPanelArchiver("dmp2", TRUE, TRUE);
+     salamander->AddViewer("*.dmp2", TRUE);
+   }
  }
=====
    */
    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) = 0;

    // releases interface 'pluginData' obtained by Salamander from plugin by calling
    // CPluginInterfaceForArchiverAbstract::ListArchive or
    // CPluginFSInterfaceAbstract::ListCurrentPath; before this call, there is also
    // release of data of file and directory (CFileData::PluginData) by calling
    // CPluginDataInterfaceAbstract
    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) = 0;

    // returns interface for archiver, if plugin has at least one of the following functions
    // (see SetBasicPluginData): FUNCTION_PANELARCHIVERVIEW, FUNCTION_PANELARCHIVEREDIT,
    // FUNCTION_CUSTOMARCHIVERPACK and/or FUNCTION_CUSTOMARCHIVERUNPACK; if plugin does not
    // have archiver, it returns NULL
    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() = 0;

    // returns interface for viewer, if plugin to return this interface, if plugin has
    // FUNCTION_VIEWER function (see SetBasicPluginData); if plugin does not have viewer,
    // it returns NULL
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() = 0;

    // returns interface for menu extension, the plugin must return this interface, if it
    // adds items to menu (see CSalamanderConnectAbstract::AddMenuItem) or if it has
    // FUNCTION_DYNAMICMENUEXT function (see SetBasicPluginData); otherwise it returns NULL
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() = 0;

    // returns interface for file-system, the plugin must return this interface, if it has
    // FUNCTION_FILESYSTEM function (see SetBasicPluginData); if the plugin does
    // not have file-system, it returns NULL
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() = 0;

    // returns interface for thumbnail loader, the plugin must return this interface, if it informed
    // Salamander that it can load thumbnails (see CSalamanderConnectAbstract::SetThumbnailLoader);
    // if the plugin does not load thumbnails, it returns NULL
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() = 0;

    // accepting various events, see PLUGINEVENT_XXX codes; only called if plugin has
    // been loaded; 'param' is parameter of event
    // CAUTION: it can be called anytime after entry-point of plugin (SalamanderPluginEntry)
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // user wants to delete all history (he started Clear History from History page of
    // configuration); history means everything which is created automatically from values
    // entered by user (e.g. list of texts entered in command line, list of current paths
    // on drives, etc.); lists created by user (e.g. hot-paths, user-menu, etc.) do not
    // belong here; 'parent' is parent of possible messageboxes; after saving configuration
    // history must not remain in registry; if plugin has windows containing history
    // (comboboxes), it must delete history there too
    virtual void WINAPI ClearHistory(HWND parent) = 0;

    // accepting information about a changed on the path 'path' (if 'includingSubdirs' is TRUE,
    // it includes also change in subdirectories of the path 'path'); this method can be used
    // e.g. for invalidating/clearing cache of files/directories; NOTE: for plugin file-systems
    // there exists method CPluginFSInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // this method is called only for plugins which use Password Manager (see
    // CSalamanderGeneralAbstract::SetPluginUsesPasswordManager()):
    // it informs plugin about changes in Password Manager; 'parent' is parent of possible
    // messageboxes/dialogs; 'event' contains event, see PME_XXX
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) = 0;
};

//
// ****************************************************************************
// CSalamanderPluginEntryAbstract
//
// set of methods of Salamander which are used in SalamanderPluginEntry

// flags informing about the reason of loading plugin (see CSalamanderPluginEntryAbstract::GetLoadInformation method)
#define LOADINFO_INSTALL 0x0001          // first load of plugin (installation into Salamander)
#define LOADINFO_NEWSALAMANDERVER 0x0002 // new version of Salamander (installation of all plugins \
                                         // from the plugins subdirectory), loads all plugins (possible \
                                         // upgrade of all)
#define LOADINFO_NEWPLUGINSVER 0x0004    // change in file plugins.ver (installation/upgrade of plugin), \
                                         // to make it simple, all plugins are loaded (possible upgrade \
                                         // of all)
#define LOADINFO_LOADONSTART 0x0008      // loaded, because the flag "load on start" was found

class CSalamanderPluginEntryAbstract
{
public:
    // returns version of Salamander, see spl_vers.h, constants LAST_VERSION_OF_SALAMANDER and REQUIRE_LAST_VERSION_OF_SALAMANDER
    virtual int WINAPI GetVersion() = 0;

    // returns "parent" window of Salamander (parent for messageboxes)
    virtual HWND WINAPI GetParentWindow() = 0;

    // returns pointer to interface to Salamander debug functions,
    // the interface is valid during the whole existence of plugin (not only in
    // "SalamanderPluginEntry" function) and it is only a reference, so it is not
    // released
    virtual CSalamanderDebugAbstract* WINAPI GetSalamanderDebug() = 0;

    // setting the basic information about plugin (data which Salamander remembers about plugin with
    // name of DLL file), necessary to call, otherwise plugin cannot be attached; 'pluginName' is
    // name of plugin; 'functions' contains logically ORed all functions which plugin supports
    // (see FUNCTION_XXX constants); 'version'+'copyright'+'description' are
    // data for user displayed in Plugins window; 'regKeyName' is proposed name of private key
    // for saving configuration in registry (without FUNCTION_LOADSAVECONFIGURATION it is ignored);
    // 'extensions' are basic extensions (e.g. only "ARJ", not "A01", etc.) of processed archives
    // separated by ';' (here ';' has no escape sequence) - Salamander uses these extensions only
    // when searching for replacement for removed panel archivers (it happens when plugin is removed;
    // it solves the problem "who will take care of extension XXX now, when the original associated
    // archiver was removed in plugin PPP?") (without FUNCTION_PANELARCHIVERVIEW and without
    // FUNCTION_PANELARCHIVEREDIT it is ignored); 'fsName' is proposed name of file system (the
    // assigned name is obtained by calling CSalamanderGeneralAbstract::GetPluginFSName) (without
    // FUNCTION_FILESYSTEM it is ignored, allowed characters are 'a-zA-Z0-9_+-', min. length is 2 chars),
    // if plugin needs more names of file system, it can use method CSalamanderPluginEntryAbstract::AddFSName;
    // returns TRUE if data were accepted
    virtual BOOL WINAPI SetBasicPluginData(const char* pluginName, DWORD functions,
                                           const char* version, const char* copyright,
                                           const char* description, const char* regKeyName = NULL,
                                           const char* extensions = NULL, const char* fsName = NULL) = 0;

    // returns pointer to interface to Salamander general functions,
    // the interface is valid during the whole existence of plugin (not only in
    // "SalamanderPluginEntry" function) and it is only a reference, so it is not
    // released
    virtual CSalamanderGeneralAbstract* WINAPI GetSalamanderGeneral() = 0;

    // returns information related to loading plugin; information is returned in DWORD value
    // as logical sum of flags LOADINFO_XXX (for testing presence of flag use condition:
    // (GetLoadInformation() & LOADINFO_XXX) != 0)
    virtual DWORD WINAPI GetLoadInformation() = 0;

    // loads module with language resources (SLG); it always tries to load module with
    // language of Salamander, if it does not find such module (or if the version does
    // not match), it lets user to choose alternative module (if there is more than one
    // alternative + if user does not have saved his choice from previous load of plugin);
    // if it does not find any module, it returns NULL -> plugin should terminate;
    // 'parent' is parent of messagebox with errors and dialog for choosing alternative
    // language module; 'pluginName' is name of plugin (so that user knows which plugin
    // is it about in error message or in dialog for choosing alternative language module)
    // CAUTION: this method can be called only once; obtained handle of language module
    //          is released automatically at unload of plugin
    virtual HINSTANCE WINAPI LoadLanguageModule(HWND parent, const char* pluginName) = 0;

    // returns ID of the currently selected language for Salamander environment (e.g.
    // english.slg = English (US) = 0x409, czech.slg = Czech = 0x405)
    virtual WORD WINAPI GetCurrentSalamanderLanguageID() = 0;

    // returns pointer to interface providing modified Windows controls used in Salamander,
    // the interface is valid during the whole existence of plugin (not only in
    // "SalamanderPluginEntry" function) and it is only a reference, so it is not
    // released
    virtual CSalamanderGUIAbstract* WINAPI GetSalamanderGUI() = 0;

    // returns pointer to interface for comfortable work with files,
    // interface is valid during the whole existence of plugin (not only in
    // "SalamanderPluginEntry" function) and it is only a reference, so it is not
    // released
    virtual CSalamanderSafeFileAbstract* WINAPI GetSalamanderSafeFile() = 0;

    // sets URL, which is displayed in Plugins Manager as home-page of plugin;
    // the value is kept by Salamander until next load of plugin (URL is displayed
    // also for not loaded plugins); at every load of plugin it is necessary to
    // set URL again, otherwise no URL is displayed (protection against keeping
    // invalid URL as home-page)
    virtual void WINAPI SetPluginHomePageURL(const char* url) = 0;

    // adding another name of file system; without FUNCTION_FILESYSTEM in parameter 'functions',
    // this method returns always error when calling method SetBasicPluginData;
    // 'fsName' is proposed name of file system (the assigned name is obtained by calling
    // CSalamanderGeneralAbstract::GetPluginFSName) (allowed characters are 'a-zA-Z0-9_+-',
    // min. length is 2 chars); in 'newFSNameIndex' (must not be NULL) is returned index
    // of new added name of file system; returns TRUE in case of success; returns FALSE
    // in case of fatal error - in this case 'newFSNameIndex' is ignored
    // limitation: it must not be called before method SetBasicPluginData
    virtual BOOL WINAPI AddFSName(const char* fsName, int* newFSNameIndex) = 0;
};

//
// ****************************************************************************
// FSalamanderPluginEntry
//
// Open Salamander 1.6 or Later Plugin Entry Point Function Type,
// this function is exported by plugin "SalamanderPluginEntry" and Salamander
// calls it for loading plugin in the moment of loading plugin
// returns interface of plugin in case of successful loading, otherwise NULL,
// interface of plugin is released by calling its method Release before unloading plugin

typedef CPluginInterfaceAbstract*(WINAPI* FSalamanderPluginEntry)(CSalamanderPluginEntryAbstract* salamander);

//
// ****************************************************************************
// FSalamanderPluginGetReqVer
//
// Open Salamander 2.5 Beta 2 or Later Plugin Get Required Version of Salamander Function Type,
// this function is exported by plugin "SalamanderPluginGetReqVer" and Salamander calls it
// as the first function of plugin (before "SalamanderPluginGetSDKVer" and "SalamanderPluginEntry")
// in the moment of loading plugin;
// returns version of Salamander for which the plugin is built (the oldest version
// which can load the plugin)

typedef int(WINAPI* FSalamanderPluginGetReqVer)();

//
// ****************************************************************************
// FSalamanderPluginGetSDKVer
//
// Open Salamander 2.52 beta 2 (PB 22) or Later Plugin Get SDK Version Function Type,
// this function is optionally exported by plugin "SalamanderPluginGetSDKVer" and Salamander
// tries to call it as the second function of plugin (before "SalamanderPluginEntry")
// in the moment of loading plugin;
// returns version of SDK used for building plugin (informs Salamander which methods
// plugin provides); exporting "SalamanderPluginGetSDKVer" makes sense only if it
// returns "SalamanderPluginGetReqVer" with smaller number than LAST_VERSION_OF_SALAMANDER;
// it is suitable to return directly LAST_VERSION_OF_SALAMANDER

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

    SecureZeroMemory(&osvi, sizeof(osvi)); // replacement for memset (does not require RTL)
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
