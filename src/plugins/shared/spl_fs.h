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
#pragma pack(push, enter_include_spl_fs) // so that the structures are independent of the current alignment setting
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

struct CFileData;
class CPluginFSInterfaceAbstract;
class CSalamanderDirectoryAbstract;
class CPluginDataInterfaceAbstract;

//
// ****************************************************************************
// CSalamanderForViewFileOnFSAbstract
//
// sada metod ze Salamandera pro podporu provedeni ViewFile v CPluginFSInterfaceAbstract,
// platnost interfacu je omezena na metodu, ktere je interface predan jako parametr

class CSalamanderForViewFileOnFSAbstract
{
public:
    // Finds an existing copy of the file in the disk cache or, if no copy exists yet,
    // reserves a name for it (the target file, for example for an FTP download);
    // 'uniqueFileName' is the unique name of the original file (the disk cache is searched
    // by this name; the full file name in Salamander format should be sufficient -
    // "fs-name:fs-user-part"; WARNING: the name is compared case-sensitively, so if the
    // plugin requires case-insensitive comparison, it must convert all names, for example,
    // to lowercase - see CSalamanderGeneralAbstract::ToLowerCase); 'nameInCache' is the name
    // of the file copy stored in the disk cache (the last component of the original file name
    // is expected here so that it later reminds the user of the original file in the viewer caption);
    // if 'rootTmpPath' is NULL, the disk cache is in the Windows TEMP
    // directory; otherwise, 'rootTmpPath' contains the path to the disk cache; on a system error it returns
    // NULL (this should not happen at all), otherwise it returns the full name of the file copy in the disk cache
    // and returns TRUE in 'fileExists' if the file already exists in the disk cache (for example,
    // the FTP download has already been performed), or FALSE if the file still needs to be prepared
    // (for example by downloading it); 'parent' is the parent of error message boxes (for example, an overly long
    // file name)
    // WARNING: if it did not return NULL (no system error occurred), it is necessary to later call
    //          FreeFileNameInCache (for the same 'uniqueFileName')
    // NOTE: if the FS uses the disk cache, it should at least call
    //       CSalamanderGeneralAbstract::RemoveFilesFromCache("fs-name:") when the plugin is unloaded, otherwise
    //       its file copies will needlessly clutter the disk cache
    virtual const char* WINAPI AllocFileNameInCache(HWND parent, const char* uniqueFileName, const char* nameInCache,
                                                    const char* rootTmpPath, BOOL& fileExists) = 0;

    // Opens file 'fileName' from a Windows path in the viewer requested by the user (either via viewer association or via the View With command). 'parent' is the parent of error message boxes. If 'fileLock' and 'fileLockOwner' are not NULL, they return the link to the open viewer (used as a parameter of FreeFileNameInCache). Returns TRUE if the viewer was opened.
    virtual BOOL WINAPI OpenViewer(HWND parent, const char* fileName, HANDLE* fileLock,
                                   BOOL* fileLockOwner) = 0;

    // Must be paired with AllocFileNameInCache; it is called only after the viewer has been opened (or after an error while preparing the file copy or opening the viewer). 'uniqueFileName' is the unique name of the original file (use the same string as in AllocFileNameInCache). 'fileExists' is FALSE if the file copy did not exist in the disk cache and TRUE if it already existed (the same value as output parameter 'fileExists' of AllocFileNameInCache). If 'fileExists' is TRUE, 'newFileOK' and 'newFileSize' are ignored; otherwise 'newFileOK' is TRUE if the file copy was prepared successfully (for example the download succeeded), and 'newFileSize' contains the size of the prepared file copy. If 'newFileOK' is FALSE, 'newFileSize' is ignored. 'fileLock' and 'fileLockOwner' tie the open viewer to file copies in the disk cache (after the viewer is closed, the disk cache is allowed to delete the file copy - when it is deleted depends on the size of the disk cache on disk); both values can be obtained when calling OpenViewer. If the viewer could not be opened (or the file copy could not be prepared in the disk cache, or the viewer is not tied to the disk cache), set 'fileLock' to NULL and 'fileLockOwner' to FALSE. If 'fileExists' is TRUE (the file copy already existed), 'removeAsSoonAsPossible' is ignored; otherwise, if 'removeAsSoonAsPossible' is TRUE, the file copy is not kept in the disk cache longer than necessary (after the viewer is closed it is deleted immediately; if the viewer was not opened at all ('fileLock' is NULL), the file is not inserted into the disk cache and is deleted instead).
    virtual void WINAPI FreeFileNameInCache(const char* uniqueFileName, BOOL fileExists, BOOL newFileOK,
                                            const CQuadWord& newFileSize, HANDLE fileLock,
                                            BOOL fileLockOwner, BOOL removeAsSoonAsPossible) = 0;
};

// ****************************************************************************
// CPluginFSInterfaceAbstract
//
// Set of plugin methods that Salamander needs to work with the file system

// typ ikon v panelu pri listovani FS (pouziva se v CPluginFSInterfaceAbstract::ListCurrentPath())
#define pitSimple 0       // simple icons for files and directories - by extension (association)
#define pitFromRegistry 1 // icons loaded from the registry according to the file/directory extension
#define pitFromPlugin 2   // the plugin provides the icons (obtained via CPluginDataInterfaceAbstract)

// FS event codes (and the meaning of parameter 'param'), received by CPluginFSInterfaceAbstract::Event():
// CPluginFSInterfaceAbstract::TryCloseOrDetach returned TRUE, but the new path could not be opened, so we stay on the current path (the FS that receives this message);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_CLOSEORDETACHCANCELED 0

// successful attachment of a new FS to the panel (after the path is changed and listed)
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_OPENED 1

// successful addition to the list of detached FSs (end of "panel" FS mode, start of "detached" FS mode);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_DETACHED 2

// successful attachment of a detached FS (end of "detached" FS mode, start of "panel" FS mode);
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ATTACHED 3

// activation of Salamander's main window (when the window is minimized, this event is sent only after restore/maximize so any error windows appear above Salamander),
// it is delivered only to an FS that is in a panel (not detached); if changes on the FS are not monitored automatically,
// this event indicates a suitable time to refresh;
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ACTIVATEREFRESH 4

// A timer of this FS has expired; 'param' is the parameter of that timer;
// POZOR: metoda CPluginFSInterfaceAbstract::Event() s kodem FSE_TIMER se vola
// from the main thread after the WM_TIMER message is delivered to the main window (so, for example,
// any modal dialog may currently be open), so the timer reaction should
// happen quietly (do not open windows, etc.); a call to
// CPluginFSInterfaceAbstract::Event() s kodem FSE_TIMER muze dojit hned po
// the call to CPluginInterfaceForFS::OpenFS (if it adds a timer for
// a newly created FS object)
#define FSE_TIMER 5

// A path change (or refresh) has just been completed in this FS in a panel, or this detached FS has just been attached to a panel (this event is sent after the path change and its listing). FSE_PATHCHANGED is sent after every successful call to ListCurrentPath.
// NOTE: FSE_PATHCHANGED follows every FSE_OPENED and FSE_ATTACHED immediately.
// 'param' is the panel containing this FS (PANEL_LEFT or PANEL_RIGHT).
#define FSE_PATHCHANGED 6

// Constants identifying the reason for calling CPluginFSInterfaceAbstract::TryCloseOrDetach(); the possible values of forceClose and canDetach are always given in parentheses (FALSE->TRUE means: first it tries without force; if the FS refuses, the user is asked and it is then retried with force):
//
// (FALSE, TRUE) when changing to a path outside the FS open in the panel
#define FSTRYCLOSE_CHANGEPATH 1
// (FALSE->TRUE, FALSE) for an FS open in a panel during plugin unload (user-requested unload + Salamander shutdown + before removing the plugin + unload requested by the plugin)
#define FSTRYCLOSE_UNLOADCLOSEFS 2
// (FALSE, TRUE) when changing the path or refreshing (Ctrl+R) an FS open in a panel, it was found that no accessible path on the FS remains - Salamander tries to change the panel path to a fixed drive (if the FS does not allow it, the FS remains in the panel with no files or directories)
#define FSTRYCLOSE_CHANGEPATHFAILURE 3
// (FALSE, FALSE) when reattaching a detached FS to a panel, it was found that no path on this FS is accessible any more - Salamander tries to close this detached FS (if the FS refuses, it remains in the list of detached FSs - e.g. in the Alt+F1/F2 menu)
#define FSTRYCLOSE_ATTACHFAILURE 4
// (FALSE->TRUE, FALSE) for a detached FS during plugin unload (user-requested unload + Salamander shutdown + before removing the plugin + unload requested by the plugin)
#define FSTRYCLOSE_UNLOADCLOSEDETACHEDFS 5
// (FALSE, FALSE) the plugin called CSalamanderGeneral::CloseDetachedFS() for a detached FS
#define FSTRYCLOSE_PLUGINCLOSEDETACHEDFS 6

// flags indicating which file-system services the plugin provides - which methods of
// CPluginFSInterfaceAbstract are defined:
// copy from FS (F5 on FS)
#define FS_SERVICE_COPYFROMFS 0x00000001
// move from FS + rename on FS (F6 on FS)
#define FS_SERVICE_MOVEFROMFS 0x00000002
// copy from disk to FS (F5 on disk)
#define FS_SERVICE_COPYFROMDISKTOFS 0x00000004
// move from disk to FS (F6 on disk)
#define FS_SERVICE_MOVEFROMDISKTOFS 0x00000008
// delete on FS (F8)
#define FS_SERVICE_DELETE 0x00000010
// quick rename on FS (F2)
#define FS_SERVICE_QUICKRENAME 0x00000020
// view from FS (F3)
#define FS_SERVICE_VIEWFILE 0x00000040
// edit from FS (F4)
#define FS_SERVICE_EDITFILE 0x00000080
// edit new file from FS (Shift+F4)
#define FS_SERVICE_EDITNEWFILE 0x00000100
// change attributes on FS (Ctrl+F2)
#define FS_SERVICE_CHANGEATTRS 0x00000200
// create directory on FS (F7)
#define FS_SERVICE_CREATEDIR 0x00000400
// show info about FS (Ctrl+F1)
#define FS_SERVICE_SHOWINFO 0x00000800
// show properties on FS (Alt+Enter)
#define FS_SERVICE_SHOWPROPERTIES 0x00001000
// calculate occupied space on FS (Alt+F10 + Ctrl+Shift+F10 + calc. needed space + spacebar key in panel)
#define FS_SERVICE_CALCULATEOCCUPIEDSPACE 0x00002000
// command line for FS (otherwise command line is disabled)
#define FS_SERVICE_COMMANDLINE 0x00008000
// get free space on FS (number in directory line)
#define FS_SERVICE_GETFREESPACE 0x00010000
// get icon of FS (icon in directory line or Disconnect dialog)
#define FS_SERVICE_GETFSICON 0x00020000
// get next directory-line FS hot-path (for shortening of current FS path in panel)
#define FS_SERVICE_GETNEXTDIRLINEHOTPATH 0x00040000
// context menu on FS (Shift+F10)
#define FS_SERVICE_CONTEXTMENU 0x00080000
// get item for change drive menu or Disconnect dialog (item for active/detached FS in Alt+F1/F2 or Disconnect dialog)
#define FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM 0x00100000
// accepts change on path notifications from Salamander (see PostChangeOnPathNotification)
#define FS_SERVICE_ACCEPTSCHANGENOTIF 0x00200000
// get path for main-window title (text in window caption) (see Configuration/Appearance/Display current path...)
// if it's not defined, full path is displayed in window caption in all display modes
#define FS_SERVICE_GETPATHFORMAINWNDTITLE 0x00400000
// Find (Alt+F7 on FS) - if it's not defined, standard Find Files and Directories dialog
// is opened even if FS is opened in panel
#define FS_SERVICE_OPENFINDDLG 0x00800000
// open active folder (Shift+F3)
#define FS_SERVICE_OPENACTIVEFOLDER 0x01000000
// show security information (click on security icon in Directory Line, see CSalamanderGeneralAbstract::ShowSecurityIcon)
#define FS_SERVICE_SHOWSECURITYINFO 0x02000000

// Missing: Change Case, Convert, Properties, Make File List

// types of context menus for CPluginFSInterfaceAbstract::ContextMenu()
#define fscmItemsInPanel 0 // context menu for items in the panel (selected/focused files and directories)
#define fscmPathInPanel 1  // context menu for the current path in the panel
#define fscmPanel 2        // context menu for the panel

#define SALCMDLINE_MAXLEN 8192 // maximum command length from the Salamander command line

class CPluginFSInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginFSInterfaceEncapsulation)
    friend class CPluginFSInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns the user-part of the current path in this FS; 'userPart' is a buffer of size MAX_PATH
    // for the path; returns TRUE on success
    virtual BOOL WINAPI GetCurrentPath(char* userPart) = 0;

    // returns the user-part of the full name of file/directory/up-dir 'file' ('isDir' is 0/1/2) on the current
    // path in this FS; for the up-dir entry (first in the directory list and also named ".."),
    // 'isDir'==2 and the method should return the current path shortened by the last component; 'buf'
    // is a buffer of size 'bufSize' for the resulting full name; returns TRUE on success
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize) = 0;

    // Returns the absolute path (including fs-name) corresponding to relative path 'path' on this FS. Returns FALSE if this method is not implemented (the other return values are then ignored). 'parent' is the parent of any error message boxes; 'fsName' is the current FS name; 'path' is a buffer of size 'pathSize': on input it contains a relative path on the FS, on output it contains the corresponding absolute path on the FS. 'success' returns TRUE if the path was translated successfully (the string in 'path' should then be used; otherwise it is ignored) - a path change follows (if it is a path on this FS, ChangePath() is called). If 'success' is FALSE, the user is assumed to have already seen an error message.
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize,
                                      BOOL& success) = 0;

    // returns the user-part of the root of the current path in this FS; 'userPart' is a buffer of size MAX_PATH
    // for the path (used by the "goto root" function); returns TRUE on success
    virtual BOOL WINAPI GetRootPath(char* userPart) = 0;

    // compares the current path in this FS with the path specified by 'fsNameIndex' and 'userPart'
    // (the FS name in the path belongs to this plugin and is identified by 'fsNameIndex'); returns TRUE
    // if the paths are identical; 'currentFSNameIndex' is the index of the current FS name
    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // Returns TRUE if the path belongs to this FS (which means Salamander may pass the path to ChangePath of this FS). The path always belongs to one of this plugin's FSs (for example, Windows paths and archive paths are never passed here). 'fsNameIndex' is the index of the FS name in the path (the index is zero for the fs-name specified in CSalamanderPluginEntryAbstract::SetBasicPluginData; for other fs-names the index is returned by CSalamanderPluginEntryAbstract::AddFSName). The user-part of the path is 'userPart'; 'currentFSNameIndex' is the index of the current FS name.
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // Changes the current path in this FS to the path specified by 'fsName' and 'userPart' (exactly, or to the nearest accessible subpath of 'userPart' - see 'mode'). If the path is shortened because it points to a file (it is enough to suspect that it might be a file path - after listing the path, it is verified whether the file exists and an error is shown if needed) and 'cutFileName' is not NULL (possible only in 'mode' 3), the method returns the name of that file (without the path) in the 'cutFileName' buffer (MAX_PATH characters); otherwise it returns an empty string in 'cutFileName'. 'currentFSNameIndex' is the index of the current FS name; 'fsName' is a MAX_PATH buffer: on input it contains the FS name from the path, which belongs to this plugin (it does not have to match the current FS name in this object, it is enough if IsOurPath() returns TRUE for it), and on output 'fsName' contains the current FS name in this object (it must belong to this plugin). 'fsNameIndex' is the index of FS name 'fsName' in the plugin (to make it easier to detect which FS name it is). If 'pathWasCut' is not NULL, TRUE is returned in it if the path was shortened; Salamander uses 'cutFileName' and 'pathWasCut' for the Change Directory command (Shift+F7) when a file name is entered, so that file gets focused. If 'forceRefresh' is TRUE, this is a hard refresh (Ctrl+R) and the plugin should change the path without using cached information (the new path must be verified to exist). 'mode' is the path-change mode:
    //   1 (refresh path) - shorten the path if needed; do not report a missing path (shorten it without a message); report a file where a path is expected, an inaccessible path, and other errors
    //   2 (ChangePanelPathToPluginFS call, back/forward in history, etc.) - shorten the path if needed; report all path errors (file where a path is expected, missing path, inaccessible path, and other errors)
    //   3 (change-dir command) - shorten the path only if it is a file or the path cannot be listed (ListCurrentPath returns FALSE for it); do not report a file where a path is expected (shorten it without a message and return the file name), but report all other path errors (missing path, inaccessible path, and others)
    // If 'mode' is 1 or 2, it returns FALSE only if no path on this FS is accessible (for example after a connection failure); if 'mode' is 3, it returns FALSE if the requested path or file is not accessible (path shortening occurs only if it is a file). If opening the FS is time-consuming (for example connecting to an FTP server) and 'mode' is 3, the behavior may be adjusted as for archives: shorten the path if needed and return FALSE only if no path on the FS is accessible; error reporting does not change.
    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode) = 0;

    // Loads files and directories from the current path and stores them in object 'dir' (for path NULL or empty string, files and directories on other paths are ignored; if a directory named .. is added, it is rendered as the up-dir symbol; file and directory names are fully plugin-defined, Salamander only displays them). Salamander obtains the contents of plugin-added columns through interface 'pluginData' (if the plugin does not add columns and has no custom icons, it returns 'pluginData'==NULL). 'iconsType' returns the requested way of obtaining file and directory icons for the panel; pitFromPlugin degrades to pitSimple if 'pluginData' is NULL (without 'pluginData', pitFromPlugin cannot be provided). If 'forceRefresh' is TRUE, this is a hard refresh (Ctrl+R) and the plugin should load files and directories without using a cache. Returns TRUE on successful load; if it returns FALSE, that is an error and ChangePath will be called for the current path, where ChangePath is expected to select an accessible subpath or return FALSE; after a successful ChangePath call, ListCurrentPath will be called again. If it returns FALSE, the returned value of 'pluginData' is ignored (the data in 'dir' must be released with 'dir.Clear(pluginData)', otherwise only the Salamander part of the data is released).
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh) = 0;

    // Prepares the FS to be closed/detached from the panel or closes a detached FS. If 'forceClose' is TRUE, the FS is closed regardless of return values - the action was forced by the user or a critical shutdown is in progress (see CSalamanderGeneralAbstract::IsCriticalShutdown); in any case, there is no point in asking the user anything, the FS should simply be closed immediately (do not open any windows anymore). If 'forceClose' is FALSE, the FS may either be closed or detached ('canDetach' TRUE) or only closed ('canDetach' FALSE). 'detach' returns TRUE if the FS wants only to detach; FALSE means close it. 'reason' contains the reason for calling this method (one of FSTRYCLOSE_XXX). Returns TRUE if the FS can be closed/detached; otherwise returns FALSE.
    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason) = 0;

    // receives an event on this FS; see the FSE_XXX event codes; 'param' is the event parameter
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // releases all FS resources except the listing data (the listing
    // may still be displayed in the panel while this method is running); called immediately before the listing in the panel
    // is destroyed
    // (the listing is destroyed only for active FSs; detached FSs have no listing) and before CloseFS is called for this FS;
    // 'parent' is the parent for any message boxes; if critical shutdown is in progress (see
    // CSalamanderGeneralAbstract::IsCriticalShutdown), do not show any windows
    virtual void WINAPI ReleaseObject(HWND parent) = 0;

    // returns the set of supported FS services (see the FS_SERVICE_XXX constants); returns the bitwise
    // OR of the constants; called after this FS is opened (see CPluginInterfaceForFSAbstract::OpenFS),
    // and after each call to ChangePath and ListCurrentPath on this FS
    virtual DWORD WINAPI GetSupportedServices() = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM:
    // gets the item for this FS (active or detached) for the Change Drive menu (Alt+F1/F2)
    // or the Disconnect dialog (hotkey: F12; disconnecting this FS, if needed, is handled by method
    // CPluginInterfaceForFSAbstract::DisconnectFS; pokud GetChangeDriveOrDisconnectItem vrati
    // FALSE a FS je v panelu, prida se polozka s ikonou ziskanou pres GetFSIcon a root cestou);
    // if the return value is TRUE, an item with icon 'icon' and text 'title' is added;
    // 'fsName' is the current FS name; if 'icon' is NULL, the item has no icon; if
    // 'destroyIcon' is TRUE and 'icon' is not NULL, 'icon' is released after use via the Win32 API
    // function DestroyIcon; 'title' is text allocated on Salamander's heap and may contain
    // up to three columns separated by '\t' (see the Alt+F1/F2 menu); only the second column is used in the Disconnect dialog;
    // if the return value is FALSE, the output values
    // 'title', 'icon', and 'destroyIcon' are ignored (no item is added)
    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title,
                                                       HICON& icon, BOOL& destroyIcon) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETFSICON:
    // gets the FS icon for the Directory Line toolbar or, if needed, for the Disconnect dialog (F12). The icon for the Disconnect dialog is obtained here only if GetChangeDriveOrDisconnectItem does not return an item for this FS (for example RegEdit and WMobile). Returns an icon, or NULL if the standard icon should be used. If 'destroyIcon' is TRUE and it returns an icon (not NULL), the returned icon is released after use by the Win32 API function DestroyIcon.
    // WARNING: if the icon resource is loaded by LoadIcon at 16x16, LoadIcon returns a 32x32 icon. When it is then drawn at 16x16, colored fringes appear around the icon. The 16->32->16 conversion can be avoided by using LoadImage:
    //        (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    //
    // No windows may be shown in this method (the panel contents are not consistent, messages such as repaint must not be dispatched, etc.).
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon) = 0;

    // returns the requested drop effect for a drag&drop operation from an FS (possibly this FS) to this FS;
    // 'srcFSPath' is the source path; 'tgtFSPath' is the target path (it belongs to this FS); 'allowedEffects'
    // contains the allowed drop effects; 'keyState' is the key state (a combination of MK_CONTROL,
    // MK_SHIFT, MK_ALT, MK_BUTTON, MK_LBUTTON, MK_MBUTTON, and MK_RBUTTON flags; see IDropTarget::Drop);
    // 'dropEffect' contains the recommended drop effects (equal to 'allowedEffects' or limited to
    // DROPEFFECT_COPY or DROPEFFECT_MOVE if the user holds Ctrl or Shift) and
    // returns the selected drop effect (DROPEFFECT_COPY, DROPEFFECT_MOVE, or DROPEFFECT_NONE);
    // if the method does not change 'dropEffect' and it contains more than one effect,
    // Copy is preferred
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETFREESPACE:
    // returns the amount of free space on the FS in 'retValue' (it must not be NULL) (displayed on the right in the Directory Line). If the free space cannot be determined, it returns CQuadWord(-1, -1) (the value is not displayed).
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // finds separator positions in the Directory Line text (for mouse-based path shortening - hot tracking). 'text' is the Directory Line text (path plus optional filter); 'pathLen' is the length of the path in 'text' (the rest is the filter); 'offset' is the character offset from which the next separator should be searched. Returns TRUE if another separator exists and returns its position in 'offset'; returns FALSE if no further separator exists (the end of the text is not considered a separator).
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // adjusts the shortened path text to be displayed in the panel (Directory Line - path shortening with the mouse, hot-tracking); used when the hot text from the Directory Line does not exactly match the path (for example, if it is missing the closing bracket - VMS paths on FTP - "[DIR1.DIR2.DIR3]");
    // 'path' is an in/out buffer containing the path (the buffer size is 'pathBufSize')
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_GETPATHFORMAINWNDTITLE:
    // obtains the text to display in the main window title when showing the current
    // path in the main window title is enabled (see Configuration/Appearance/Display current
    // path...); 'fsName' is the current FS name; if 'mode' is 1, this is
    // Directory Name Only mode (only the current directory name - the last
    // path component - should be displayed); if 'mode' is 2, this is
    // Shortened Path mode (the shortened form of the path should be displayed -
    // root (including the path separator) + ... + path separator
    // + the last path component); 'buf' is a buffer of size 'bufSize' for the
    // resulting text; returns TRUE if it returns the requested text; returns FALSE if the
    // GetNextDirectoryLineHotPath()
    // POZNAMKA: pokud GetSupportedServices() nevraci i FS_SERVICE_GETPATHFORMAINWNDTITLE,
    // text should be generated from separator positions obtained by the method
    // GetNextDirectoryLineHotPath()
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWINFO:
    // displays a dialog with information about the FS (free space, capacity, name, capabilities, etc.);
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the displayed dialog
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_COMMANDLINE:
    // executes a command for the FS in the active panel from the command line below the panels; returns FALSE on error (the command is not added to the command-line history and the other return values are ignored). Returns TRUE if the command was started successfully (note: the command result does not matter - only whether it was started, for example for FTP whether it was delivered to the server). 'parent' is the suggested parent of any displayed dialogs; 'command' is a buffer of size SALCMDLINE_MAXLEN+1 that contains the command to execute on input (the actual maximum command length depends on the Windows version and the contents of the COMSPEC environment variable) and the new contents of the command line on output (usually it is just cleared to an empty string). 'selFrom' and 'selTo' return the selection position in the new command-line contents (if they are equal, only the caret is positioned; if the output line is empty, these values are ignored).
    // WARNING: this method should not change the panel path directly - an invalid path may close the FS (the method's this pointer would cease to exist).
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_QUICKRENAME:
    // quickly renames file 'file' or a directory ('isDir' is FALSE/TRUE) on the FS. It may open its own quick-rename dialog ('mode' is 1) or use the standard dialog (when 'mode'==1 it returns FALSE and 'cancel' FALSE; Salamander then opens the standard dialog and passes the entered new name in 'newName' on the next call to QuickRename with 'mode'==2). 'fsName' is the current FS name; 'parent' is the suggested parent of any displayed dialogs; 'newName' is the new name when 'mode'==2. If it returns TRUE, 'newName' returns the new name (max. MAX_PATH characters; not the full name, only the name of the item in the panel) - Salamander will try to focus it after refresh (the refresh is handled by the FS itself, for example via CSalamanderGeneralAbstract::PostRefreshPanelFS). If it returns FALSE and 'mode'==2, 'newName' returns the invalid new name (possibly modified in some way - for example an operation mask may already have been applied). If the user wants to cancel the operation, 'cancel' returns TRUE. When 'cancel' is FALSE, the method returns TRUE on successful completion of the operation; if it returns FALSE with 'mode'==1, the standard quick-rename dialog should be opened; if it returns FALSE with 'mode'==2, it is an operation error (the invalid new name is returned in 'newName' - the standard dialog is reopened and the user can correct the invalid name there).
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file,
                                    BOOL isDir, char* newName, BOOL& cancel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_ACCEPTSCHANGENOTIF:
    // receives information about a change on path 'path' (if 'includingSubdirs' is TRUE, it
    // also includes changes in subdirectories of path 'path'); this method should decide
    // whether this FS needs to be refreshed (for example via method
    // CSalamanderGeneralAbstract::PostRefreshPanelFS); tyka se jak aktivnich FS, tak
    // detached FSs; 'fsName' is the current FS name
    // NOTE: for the plugin as a whole, there is also the method
    //           CPluginInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path,
                                                       BOOL includingSubdirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CREATEDIR:
    // creates a new directory on the FS. It may open its own create-directory dialog ('mode' is 1) or use the standard dialog (when 'mode'==1 it returns FALSE and 'cancel' FALSE; Salamander then opens the standard dialog and passes the directory name it obtains in 'newName' on the next call to CreateDir with 'mode'==2). 'fsName' is the current FS name; 'parent' is the suggested parent of any displayed dialogs; 'newName' is the new directory name if 'mode'==2. If it returns TRUE, 'newName' returns the new directory name (max. 2 * MAX_PATH characters; not the full name, only the item name in the panel) - Salamander will try to focus it after refresh (the refresh is handled by the FS itself, for example via CSalamanderGeneralAbstract::PostRefreshPanelFS). If it returns FALSE and 'mode'==2, 'newName' returns the invalid directory name (max. 2 * MAX_PATH characters, possibly adjusted to an absolute form). If the user wants to cancel the operation, 'cancel' returns TRUE. When 'cancel' is FALSE, the method returns TRUE on successful completion of the operation; if it returns FALSE with 'mode'==1, the standard create-directory dialog should be opened; if it returns FALSE with 'mode'==2, it is an operation error (the invalid directory name is returned in 'newName' - the standard dialog is reopened and the user can correct the invalid name there).
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent,
                                  char* newName, BOOL& cancel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_VIEWFILE:
    // views file 'file' (directories cannot be viewed with the View command) on the current path
    // on the FS; 'fsName' is the current FS name; 'parent' is the parent of any message boxes
    // for errors; 'salamander' is the set of Salamander methods required to implement
    // viewing with caching
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_DELETE:
    // deletes the files and directories selected in the panel; it may open its own delete confirmation dialog ('mode' is 1; whether the confirmation should be shown depends on SALCFG_CNFRMFILEDIRDEL - TRUE means the user wants delete confirmations) or use the standard dialog (when 'mode'==1 it returns FALSE and 'cancelOrError' FALSE; Salamander then opens the standard dialog (if SALCFG_CNFRMFILEDIRDEL is TRUE) and, if the user confirms, calls Delete again with 'mode'==2). 'fsName' is the current FS name; 'parent' is the suggested parent of any displayed dialogs; 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT) in which the FS is open (the files/directories to delete are taken from this panel); 'selectedFiles' + 'selectedDirs' is the number of selected files and directories. If both values are zero, the file/directory under the cursor (focus) is deleted. Before Delete is called, either files/directories are selected or there is at least focus on a file/directory, so there is always something to work with (no additional checks are needed). If it returns TRUE and 'cancelOrError' is FALSE, the operation completed correctly and the selected files/directories should be deselected (if they survived the delete). If the user cancels the operation or an error occurs, 'cancelOrError' is TRUE and the files/directories are not deselected. If it returns FALSE with 'mode'==1 and 'cancelOrError' is FALSE, the standard delete confirmation dialog should be opened.
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError) = 0;

    // Copy/move from the FS ('copy' is TRUE/FALSE); the rest of this text speaks only about copying, but everything applies equally to moving. 'copy' may be TRUE (copy) only if GetSupportedServices() returns FS_SERVICE_COPYFROMFS; 'copy' may be FALSE (move or rename) only if GetSupportedServices() returns FS_SERVICE_MOVEFROMFS.
    //
    // Copies files and directories (from the FS) selected in the panel; it may open its own dialog for entering the copy target ('mode' is 1) or use the standard dialog (it returns FALSE and 'cancelOrHandlePath' FALSE, then Salamander opens the standard dialog and passes the target path entered there in 'targetPath' on the next call to CopyOrMoveFromFS with 'mode'==2). For 'mode'==2, 'targetPath' is exactly the string entered by the user (CopyOrMoveFromFS may parse it in its own way). If CopyOrMoveFromFS supports only Windows target paths (or cannot process the user-entered path - for example it leads to another FS or an archive), it may use Salamander's standard path processing (currently it handles only Windows paths; in the future it may also handle FS and archive paths via the TEMP directory using a sequence of several basic operations) - it returns FALSE, 'cancelOrHandlePath' TRUE, and 'operationMask' TRUE/FALSE (supports/does not support operation masks - if it does not support them and the path contains a mask, an error message is shown). Salamander then processes the path returned in 'targetPath' (currently only splitting a Windows path into existing part, non-existing part, and optional mask; it can also create subdirectories from the non-existing part) and, if the path is valid, calls CopyOrMoveFromFS again with 'mode'==3 and 'targetPath' containing the target path and optionally an operation mask (two strings separated by a null; no mask -> two nulls at the end of the string). If there is an error in the path, Salamander calls CopyOrMoveFromFS again with 'mode'==4 and 'targetPath' containing the corrected invalid target path (the error has already been reported to the user; the user should be allowed to correct the path; . and .. etc. may have been removed from it).
    //
    // If the user initiates the operation by drag&drop (drops files/directories from the FS into the same panel or another drop target), 'mode'==5 and 'targetPath' is the operation target path (it may be a Windows path, an FS path, and in the future possibly an archive path); 'targetPath' is terminated by two nulls (for compatibility with 'mode'==3). In this case, 'dropTarget' is the drop-target window (used to reactivate the drop target after the operation progress window is opened; see CSalamanderGeneralAbstract::ActivateDropTarget). For 'mode'==5, only the TRUE return value is meaningful.
    //
    // 'fsName' is the current FS name; 'parent' is the suggested parent of any displayed dialogs; 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT) in which the FS is open (the files/directories to copy are taken from this panel). 'selectedFiles' + 'selectedDirs' is the number of selected files and directories; if both values are zero, the file/directory under the cursor (focus) is copied. Before CopyOrMoveFromFS is called, either files/directories are selected or there is at least focus on a file/directory, so there is always something to work with (no additional checks are needed). On input, 'targetPath' contains: for 'mode'==1, the suggested target path (Windows paths without a mask only, or an empty string); for 'mode'==2, the target path string entered by the user in the standard dialog; for 'mode'==3, the target path and mask (separated by a null); for 'mode'==4, the invalid target path; for 'mode'==5, the target path (Windows, FS, or archive) terminated by two nulls. If the method returns FALSE, 'targetPath' on output (a buffer of 2 * MAX_PATH characters) contains the suggested target path for the standard dialog when 'cancelOrHandlePath'==FALSE, and the target path string to be processed when 'cancelOrHandlePath'==TRUE. If the method returns TRUE and 'cancelOrHandlePath' is FALSE, 'targetPath' contains the name of the item that should receive focus in the source panel (a 2 * MAX_PATH character buffer; not the full name, only the item name as shown in the panel; if it is an empty string, the focus remains unchanged). 'dropTarget' is not NULL only when the operation path was entered by drag&drop (see above).
    //
    // If it returns TRUE and 'cancelOrHandlePath' is FALSE, the operation completed correctly and the selected files/directories should be deselected. If the user cancels the operation or an error occurs, the method returns TRUE and 'cancelOrHandlePath' TRUE; in both cases the files/directories are not deselected. If it returns FALSE, the standard dialog should be opened ('cancelOrHandlePath' is FALSE) or the path should be processed in the standard way ('cancelOrHandlePath' is TRUE).
    //
    // NOTE: if the user is offered copying/moving to a path in the target panel, CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath must be called for the target panel, otherwise the path in that panel will not be added to the List of Working Directories (Alt+F12).
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget) = 0;

    // Copy/move from a Windows path to the FS ('copy' is TRUE/FALSE); the rest of this text speaks only about copying, but everything applies equally to moving. 'copy' may be TRUE (copy) only if GetSupportedServices() returns FS_SERVICE_COPYFROMDISKTOFS; 'copy' may be FALSE (move or rename) only if GetSupportedServices() returns FS_SERVICE_MOVEFROMDISKTOFS.
    //
    // Copies selected files and directories (from a panel or elsewhere) to the FS. With 'mode'==1 it allows the plugin to prepare the target-path text for the user in the standard copy dialog; this is used when the source panel (the panel from which the Copy command, F5, is started) contains a Windows path and the target panel contains this FS. With 'mode'==2 and 'mode'==3, the plugin may perform the copy operation or report one of two errors: invalid path (for example it contains invalid characters or does not exist) and the requested operation cannot be performed in this FS (for example it is FTP, but the currently open path in this FS differs from the target path, e.g. a different FTP server - another/new FS must be opened; a newly opened FS cannot report this error).
    // WARNING: this method may be called for any target FS path of this plugin (so it may also be a path with a different FS name of this plugin).
    //
    // 'fsName' is the current FS name; 'parent' is the suggested parent of any displayed dialogs; 'sourcePath' is the source Windows path (all selected files and directories are addressed relative to it), and is NULL for 'mode'==1. The selected files and directories are provided by enumeration function 'next' with parameter 'nextParam'; for 'mode'==1 they are NULL. 'sourceFiles' + 'sourceDirs' is the number of selected files and directories (their sum is always nonzero). 'targetPath' is an in/out buffer of at least 2 * MAX_PATH characters for the target path. For 'mode'==1, 'targetPath' contains the current path on this FS on input and the target path for the standard copy dialog on output. For 'mode'==2, 'targetPath' contains the user-entered target path on input (unchanged, including mask, etc.) and the output is ignored except when the method returns FALSE (error) and 'invalidPathOrCancel' TRUE (invalid path); in that case the output contains the corrected target path (for example with . and .. removed) that the user will edit in the standard copy dialog. For 'mode'==3, 'targetPath' contains the drag&drop target path on input and the output is ignored. If 'invalidPathOrCancel' is not NULL (only for 'mode'==2 and 'mode'==3), TRUE is returned in it if the path is invalid (contains invalid characters, does not exist, etc.) or the operation was cancelled - error/cancel messages are shown before this method returns.
    //
    // For 'mode'==1 the method returns TRUE on success; if it returns FALSE, an empty string is used as the target path for the standard copy dialog. If the method returns FALSE for 'mode'==2 and 'mode'==3, another FS should be searched for to handle the operation (if 'invalidPathOrCancel' is FALSE) or the user should correct the target path (if 'invalidPathOrCancel' is TRUE). If the method returns TRUE for 'mode'==2 or 'mode'==3, the operation has completed and the selected files/directories should be deselected (if 'invalidPathOrCancel' is FALSE), or an error/cancellation occurred and the selected files/directories must not be deselected (if 'invalidPathOrCancel' is TRUE).
    //
    // WARNING: CopyOrMoveFromDiskToFS may be called in three situations:
    //        - this FS is active in a panel
    //        - this FS is detached
    //        - this FS has just been created (by OpenFS) and is destroyed again immediately after the method returns
    //          (by CloseFS) - no other method has been called on it yet (not even ChangePath).
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CHANGEATTRS:
    // changes the attributes of the files and directories selected in the panel; each plugin provides its own attribute dialog;
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the custom dialog; 'panel'
    // identifies the panel (PANEL_LEFT or PANEL_RIGHT) in which the FS is open (this panel supplies the files/directories being operated on);
    // 'selectedFiles' + 'selectedDirs' is the number of selected files and directories;
    // if both values are zero, the file/directory under the cursor is used
    // (focus); before ChangeAttributes is called, either files/directories are selected or there is at least
    // focus on a file/directory, so there is always something to work with (no additional checks
    // are needed); if it returns TRUE, the operation completed successfully and the selected files/directories
    // should be deselected; if the user cancels the operation or an error occurs, the method returns
    // FALSE and the files/directories are not deselected
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWPROPERTIES:
    // displays a properties window for the files and directories selected in the panel; each plugin
    // provides its own properties window;
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the custom window
    // (a Windows properties window is modeless - note: a modeless window must
    // have its own thread); 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT)
    // in which the FS is open (this panel supplies the files/directories
    // being worked with); 'selectedFiles' + 'selectedDirs' is the number of selected
    // files and directories; if both values are zero, the file/directory under the cursor is used
    // (focus); before ShowProperties is called, either files/directories are selected
    // or there is at least focus on a file/directory, so there is always
    // something to work with (no additional checks are needed)
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CONTEXTMENU:
    // displays a context menu for files and directories selected in the panel (right-click on items in the panel), for the current path in the panel (right-click on the change-drive button in the panel toolbar), or for the panel itself (right-click behind the items in the panel); each plugin provides its own context menu.
    //
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the context menu; 'menuX' + 'menuY' are the suggested coordinates of the top-left corner of the context menu; 'type' is the context-menu type (see the fscmXXX constants); 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT) for which the context menu should be opened (the files/directories/path to work with are taken from this panel). If 'type'==fscmItemsInPanel, 'selectedFiles' + 'selectedDirs' is the number of selected files and directories; if both values are zero, the file/directory under the cursor (focus) is used. Before ContextMenu is called, either files/directories are selected (and were clicked) or there is at least focus on a file/directory (not on the up-dir), so there is always something to work with (no additional checks are needed). If 'type'!=fscmItemsInPanel, 'selectedFiles' + 'selectedDirs' are always zero (ignored).
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_CONTEXTMENU:
    // if an FS is open in the panel and one of the messages WM_INITPOPUP, WM_DRAWITEM,
    // WM_MENUCHAR, or WM_MEASUREITEM arrives, Salamander calls HandleMenuMsg to allow the plugin
    // to work with IContextMenu2 and IContextMenu3
    // the plugin returns TRUE if it handled the message and FALSE otherwise
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_OPENFINDDLG:
    // opens the Find dialog for the FS in a panel. 'fsName' is the current FS name; 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT) for which the Find dialog should be opened (the search path is usually taken from this panel). Returns TRUE if the Find dialog is opened successfully; if it returns FALSE, Salamander opens the standard Find Files and Directories dialog.
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_OPENACTIVEFOLDER:
    // opens an Explorer window for the current path in the panel
    // 'fsName' is the current FS name; 'parent' is the suggested parent of the displayed dialog
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_MOVEFROMFS or FS_SERVICE_COPYFROMFS:
    // allows the plugin to influence the allowed drop effects for drag&drop from this FS. If 'allowedEffects' is not NULL, on input it contains the drop effects allowed so far (a combination of DROPEFFECT_MOVE and DROPEFFECT_COPY); on output it contains the drop effects allowed by this FS (effects should only be removed). 'mode' is 0 for the call that immediately precedes the start of the drag&drop operation; the effects returned in 'allowedEffects' are used for the DoDragDrop call (they apply to the whole drag&drop operation). 'mode' is 1 while dragging the mouse over an FS in this process (this FS or an FS in the other panel). For 'mode' 1, 'tgtFSPath' is the target path that will be used if a drop occurs; otherwise 'tgtFSPath' is NULL. 'mode' is 2 for the call that immediately follows completion of the drag&drop operation (successful or unsuccessful).
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) = 0;

    // Allows the plugin to change the standard message There are no items in this panel. displayed when there is no item in the panel (file/directory/up-dir). Returns FALSE if the standard message should be used (the value in 'textBuf' is then ignored). Returns TRUE if the plugin returns its own alternative message in 'textBuf' (a buffer of size 'textBufSize' characters).
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) = 0;

    // Only if GetSupportedServices() returns FS_SERVICE_SHOWSECURITYINFO:
    // the user clicked the security icon (see CSalamanderGeneralAbstract::ShowSecurityIcon;
    // for example, FTPS displays a dialog with the server certificate); 'parent' is the suggested parent of the dialog
    virtual void WINAPI ShowSecurityInfo(HWND parent) = 0;

    /* zbyva dokoncit:
// calculate occupied space on FS (Alt+F10 + Ctrl+Shift+F10 + calc. needed space + spacebar key in panel)
#define FS_SERVICE_CALCULATEOCCUPIEDSPACE
// edit from FS (F4)
#define FS_SERVICE_EDITFILE
// edit new file from FS (Shift+F4)
#define FS_SERVICE_EDITNEWFILE
*/
};

//
// ****************************************************************************
// CPluginInterfaceForFSAbstract
//

class CPluginInterfaceForFSAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calls to methods (see CPluginInterfaceForFSEncapsulation)
    friend class CPluginInterfaceForFSEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // File-system entry point; called to open an FS. 'fsName' is the name of the FS to open; 'fsNameIndex' is the index of the FS name to open (the index is zero for the fs-name specified in CSalamanderPluginEntryAbstract::SetBasicPluginData; for other fs-names the index is returned by CSalamanderPluginEntryAbstract::AddFSName). Returns a pointer to the interface of the opened FS, CPluginFSInterfaceAbstract, or NULL on error.
    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex) = 0;

    // File-system entry point; called to close an FS. 'fs' is a pointer to the interface of the opened FS. After this call, the 'fs' interface is considered invalid in Salamander and will no longer be used (this function pairs with OpenFS).
    // WARNING: this method must not open any window or dialog
    //        (windows may be opened in CPluginFSInterfaceAbstract::ReleaseObject).
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* fs) = 0;

    // Executes the command on an FS item in the Change Drive menu or in the Drive bars (for adding it, see CSalamanderConnectAbstract::SetChangeDriveMenuItem). 'panel' identifies the panel we should work with - for a command from the Change Drive menu, 'panel' is always PANEL_SOURCE (this menu can be opened only for the active panel); for a command from the Drive bars, 'panel' may be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled, we may work with the inactive panel).
    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel) = 0;

    // Opens a context menu on the FS item in the Change Drive menu or in the Drive
    // bars, or for an active/detached FS in the Change Drive menu; 'parent' is the parent
    // of the context menu; 'x' and 'y' are the coordinates for opening the context menu
    // (the right-mouse-button click position or the suggested coordinates for Shift+F10);
    // je-li 'pluginFS' NULL jde o polozku pro FS, jinak je 'pluginFS' interface
    // of the active/detached FS ('isDetachedFS' is FALSE/TRUE); if 'pluginFS' is not
    // NULL, je v 'pluginFSName' jmeno FS otevreneho v 'pluginFS' (jinak je v
    // 'pluginFSName' NULL) and 'pluginFSNameIndex' contains the index of the FS name opened
    // in 'pluginFS' (to make it easier to detect which FS name it is; otherwise
    // 'pluginFSNameIndex' is -1); if it returns FALSE, the other return values are
    // ignored; otherwise they have this meaning: 'refreshMenu' returns TRUE if the
    // Change Drive menu should be refreshed (ignored for Drive bars, because active/detached FSs are not shown there); 'closeMenu' returns TRUE if the
    // Change Drive menu should be closed (there is nothing to close for Drive bars); if 'closeMenu'
    // is TRUE and 'postCmd' is not zero, after the Change Drive menu is closed (for Drive bars
    // TRUE a 'postCmd' neni nula, je po zavreni Change Drive menu (pro Drive bary
    // immediately) ExecuteChangeDrivePostCommand is also called with parameters 'postCmd'
    // and 'postCmdParam'; 'panel' identifies the panel we should work with - for a
    // context menu in the Change Drive menu, 'panel' is always PANEL_SOURCE (this menu
    // can be opened only for the active panel); for a context menu in the Drive bars
    // 'panel' can be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled, we can
    // work with the inactive panel as well)
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam) = 0;

    // Executes a context-menu command for an FS item or for an active/detached FS in the Change Drive menu after the Change Drive menu is closed, or executes a context-menu command for an FS item in the Drive bars (only for compatibility with the Change Drive menu). It is called in response to return values 'closeMenu' (TRUE), 'postCmd', and 'postCmdParam' from ChangeDriveMenuItemContextMenu after the Change Drive menu is closed (for Drive bars, immediately). 'panel' identifies the panel we should work with - for a context menu in the Change Drive menu, 'panel' is always PANEL_SOURCE (this menu can be opened only for the active panel); for a context menu in the Drive bars, 'panel' may be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled, we may work with the inactive panel).
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam) = 0;

    // Executes an item in a panel with an open FS (for example, in response to pressing Enter in the panel;
    // for a subdirectory/up-dir (it is an up-dir if its name is ".." and it is also the first directory),
    // a path change is assumed; for a file, a copy of the file on disk is opened, and any
    // changes are then loaded back to the FS); execution cannot be performed in an FS interface method because
    // methods for changing the path cannot be called there (they may even close the FS);
    // 'panel' specifies the panel in which execution takes place (PANEL_LEFT or PANEL_RIGHT);
    // 'pluginFS' is the interface of the FS opened in the panel; 'pluginFSName' is the name of the FS opened
    // in the panel; 'pluginFSNameIndex' is the index of the name of the FS opened in the panel (for easier detection
    // of which FS name it is); 'file' is the file/directory/up-dir being executed ('isDir' is 0/1/2);
    // WARNING: calling a method to change the path in the panel may invalidate 'pluginFS' (after the FS is closed)
    //          and 'file'+'isDir' (a change in the panel listing destroys the items of the original listing)
    // NOTE: if a file is executed or otherwise used (for example, downloaded),
    //       CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath must be called for panel
    //       'panel', otherwise the path in this panel will not be added to the list of working
    //       directories - List of Working Directories (Alt+F12)
    virtual void WINAPI ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex,
                                    CFileData& file, int isDir) = 0;

    // Disconnects the FS requested by the user in the Disconnect dialog; 'parent' is the parent of any message boxes (the Disconnect dialog is still open at that time). The disconnect cannot be performed in an FS-interface method because the FS is supposed to cease to exist. 'isInPanel' is TRUE if the FS is in a panel; then 'panel' specifies which panel (PANEL_LEFT or PANEL_RIGHT). 'isInPanel' is FALSE if the FS is detached; then 'panel' is 0. 'pluginFS' is the FS interface; 'pluginFSName' is the FS name; 'pluginFSNameIndex' is the index of the FS name (to make it easier to detect which FS name it is). The method returns FALSE if the disconnect fails and the Disconnect dialog should remain open (its contents are refreshed so previous successful disconnects are reflected).
    virtual BOOL WINAPI DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                     CPluginFSInterfaceAbstract* pluginFS,
                                     const char* pluginFSName, int pluginFSNameIndex) = 0;

    // converts the user-part of the path in buffer 'fsUserPart' (size MAX_PATH characters) from external
    // to internal format (for example, on FTP: internal format = paths as used by the server,
    // external format = URL format = paths containing hex escape sequences (e.g. "%20" = " "))
    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // converts the user-part of the path in buffer 'fsUserPart' (size MAX_PATH characters) from internal
    // to external format
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // This method is called only for a plugin that serves as a replacement for the Network item
    // in the Change Drive menu and in the Drive bars (see CSalamanderGeneralAbstract::SetPluginIsNethood()):
    // by calling this method, Salamander informs the plugin that the user is changing the path from the root of the UNC
    // path "\\server\share" via the up-dir entry ("..") to the plugin FS, to a path with
    // user-part "\\server" in panel 'panel' (PANEL_LEFT or PANEL_RIGHT); purpose of this method:
    // the plugin should immediately list at least this one share at that path so it can receive
    // focus in the panel (which is the normal behavior when changing path via up-dir)
    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_fs)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__

/*
 * Preliminary version of the plugin interface documentation
 *
 *   Opening, changing, listing, and refreshing a path:
 *     - ChangePath is called to open a path in a new FS (the first call to ChangePath always opens a path)
 *     - ChangePath is called to change a path (the second and all subsequent calls to ChangePath are path changes)
 *     - on a fatal error, ChangePath returns FALSE (the FS path is not opened in the panel; if this was a path change, ChangePath is then called for the original path, and if that also fails, Salamander switches to a fixed-drive path)
 *     - if ChangePath returns TRUE (success) and the path was not shortened back to the original path (whose listing is currently loaded), ListCurrentPath is called to obtain the new listing
 *     - after a successful listing, ListCurrentPath returns TRUE
 *     - on a fatal error, ListCurrentPath returns FALSE, and the subsequent call to ChangePath must also return FALSE
 *     - if the current path cannot be listed, ListCurrentPath returns FALSE and the subsequent call to ChangePath must change the path and return TRUE (ListCurrentPath is then called again); if the path can no longer be changed (root, etc.), ChangePath also returns FALSE (the FS path is not opened in the panel; if this was a path change, ChangePath is then called for the original path, and if that also fails, Salamander switches to a fixed-drive path)
 *     - refreshing a path (Ctrl+R) behaves the same as changing to the current path (the path may remain unchanged, be shortened, or, in case of a fatal error, change to a fixed drive); during path refresh, the parameter 'forceRefresh' is TRUE for all calls to ChangePath and ListCurrentPath (the FS must not use any cache to change the path or load the listing; the user does not want cached data)
 *
 *   When traversing history (back/forward), the FS interface in which the FS path ('fsName':'fsUserPart') will be listed is chosen by the first applicable method from the following:
 *     - the FS interface in which the path was last opened has not yet been closed and is either detached or active in a panel (and is not active in the other panel)
 *     - the active FS interface in the panel ('currentFSName') belongs to the same plugin as 'fsName' and IsOurPath('currentFSName', 'fsName', 'fsUserPart') returns TRUE
 *     - the first detached FS interface ('currentFSName') that belongs to the same plugin as 'fsName' and for which IsOurPath('currentFSName', 'fsName', 'fsUserPart') returns TRUE
 *     - a new FS interface
 */
