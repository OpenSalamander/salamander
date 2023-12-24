// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED
//
//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_fs) // so that structures are independent of set alignment
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
// set of methods from Salamander for support of ViewFile in CPluginFSInterfaceAbstract,
// validity of interface is limited to the method to which the interface is passed as parameter

class CSalamanderForViewFileOnFSAbstract
{
public:
    // searches for an existing copy of the file in the disk cache or if the copy of the file
    // does not exist in the disk cache yet, reserves a name for it (target file for example
    // for download from FTP); 'uniqueFileName' is unique name of the original file (the name
    // is used for searching in the disk cache; it should be enough to use full name of the
    // file in Salamander form - "fs-name:fs-user-part"; WARNING: the name is compared
    // "case-sensitive", if the plugin requires "case-insensitive", it must convert all names
    // to lower case - see CSalamanderGeneralAbstract::ToLowerCase); 'nameInCache' is name
    // of the copy of the file which is placed in the disk cache (the last part of the name
    // of the original file is expected here, so that it reminds the original file in the
    // title of the viewer); if 'rootTmpPath' is NULL, the disk cache is in the Windows TEMP
    // directory, otherwise it is path to the disk cache in 'rootTmpPath'; in case of system
    // error returns NULL (it should not occur at all), otherwise returns full name of the
    // copy of the file in the disk cache and in 'fileExists' returns TRUE if the file in
    // the disk cache exists (for example download from FTP has already been done) or FALSE
    // if the file is needed to be prepared (for example to perform its download); 'parent'
    // is parent of messagebox with errors (for example too long name of the file)
    // WARNING: if it does not return NULL (system error did not occur), it is necessary to
    //          call FreeFileNameInCache later (for the same 'uniqueFileName')
    // NOTE: if FS uses disk cache, it should at least call
    //       CSalamanderGeneralAbstract::RemoveFilesFromCache("fs-name:") when unloading the
    //       plugin, otherwise its copies of files will unnecessarily disturb in the disk cache
    virtual const char* WINAPI AllocFileNameInCache(HWND parent, const char* uniqueFileName, const char* nameInCache,
                                                    const char* rootTmpPath, BOOL& fileExists) = 0;

    // opens file 'fileName' from Windows path in viewer chosen by user (either by association
    // or by command View With); 'parent' is parent of messagebox with errors; if 'fileLock'
    // and 'fileLockOwner' are not NULL, they return binding to the opened viewer (it is used
    // as parameter of method FreeFileNameInCache); returns TRUE if the viewer was opened
    virtual BOOL WINAPI OpenViewer(HWND parent, const char* fileName, HANDLE* fileLock,
                                   BOOL* fileLockOwner) = 0;

    // must match with AllocFileNameInCache, called after opening the viewer (or after error
    // during preparation of the copy of the file or opening the viewer); 'uniqueFileName'
    // is unique name of the original file (use the same string as when calling
    // AllocFileNameInCache); 'fileExists' is FALSE if the copy of the file in the disk cache
    // did not exist and TRUE if it already existed (the same value as output parameter
    // 'fileExists' of method AllocFileNameInCache); if 'fileExists' is TRUE, 'newFileOK'
    // and 'newFileSize' are ignored, otherwise 'newFileOK' is TRUE if the copy of the file
    // was successfully prepared (for example download was successful) and in 'newFileSize'
    // is size of the prepared copy of the file; if 'newFileOK' is FALSE, 'newFileSize' is
    // ignored; 'fileLock' and 'fileLockOwner' bind opened viewer with copies of files in
    // the disk cache (after closing the viewer the disk cache allows to delete the copy
    // of the file - when the copy of the file is deleted depends on size of the disk cache
    // on the disk), both these parameters can be obtained when calling method OpenViewer;
    // if the viewer could not be opened (or the copy of the file could not be prepared
    // in the disk cache or the viewer does not have binding with the disk cache), 'fileLock'
    // is set to NULL and 'fileLockOwner' to FALSE; if 'fileExists' is TRUE (the copy of the
    // file existed), the value 'removeAsSoonAsPossible' is ignored, otherwise: if
    // 'removeAsSoonAsPossible' is TRUE, the copy of the file in the disk cache will not be
    // stored longer than necessary (after closing the viewer it will be deleted immediately;
    // if the viewer was not opened at all ('fileLock' is NULL), the file will not be placed
    // in the disk cache, but it will be deleted)
    virtual void WINAPI FreeFileNameInCache(const char* uniqueFileName, BOOL fileExists, BOOL newFileOK,
                                            const CQuadWord& newFileSize, HANDLE fileLock,
                                            BOOL fileLockOwner, BOOL removeAsSoonAsPossible) = 0;
};

//
// ****************************************************************************
// CPluginFSInterfaceAbstract
//
// set of methods of plugin which are needed by Salamander for work with file system

// type of icons in panel during browsing FS (used in CPluginFSInterfaceAbstract::ListCurrentPath())
#define pitSimple 0       // simple icons for files and directories - according to extension (association)
#define pitFromRegistry 1 // icons loaded from registry according to file/directory extension
#define pitFromPlugin 2   // icons managed by plugin - plugin returns icons in CPluginDataInterfaceAbstract

// codes of events (and meaning of parameter 'param') sent to FS, received by method CPluginFSInterfaceAbstract::Event():
// CPluginFSInterfaceAbstract::TryCloseOrDetach returned TRUE, but new path could not be opened,
// so we stay on current path (FS which receives this message);
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_CLOSEORDETACHCANCELED 0

// successful connection of new FS to panel (after change of path and its listing)
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_OPENED 1

// successful addition to the list of disconnected FS (end of "panel" FS mode, beginning of "detached" FS mode);
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_DETACHED 2

// successful connection of disconnected FS to panel (end of "detached" FS mode, beginning of "panel" FS mode);
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ATTACHED 3

// activation of main Salamander window (when window is minimized, it is waiting for restore/maximize
// and then this event is sent, so that any error windows are shown above Salamander),
// goes only to FS which is in panel (not detached), if changes on FS are not monitored automatically,
// this event announces suitable moment for refresh;
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_ACTIVATEREFRESH 4

// timeout of one of timers of this FS has expired, 'param' is parameter of this timer;
// CAUTION: method CPluginFSInterfaceAbstract::Event() with code FSE_TIMER is called from main thread
// after receiving WM_TIMER message by main window (so for example any modal dialog can be opened),
// so reaction to timer should be silent (no opening of any windows, etc.); call of method
// CPluginFSInterfaceAbstract::Event() with code FSE_TIMER can occur immediately after calling
// method CPluginInterfaceForFS::OpenFS (if timer for newly created FS is added in it)
#define FSE_TIMER 5

// change of path (or refresh) in this FS in panel or connection of this detached FS to panel
// (this event is sent after change of path and its listing); FSE_PATHCHANGED is sent after
// every successful call of ListCurrentPath
// NOTE: FSE_PATHCHANGED immediately follows all FSE_OPENED and FSE_ATTACHED
// 'param' is panel containing this FS (PANEL_LEFT or PANEL_RIGHT)
#define FSE_PATHCHANGED 6

// constants marking reason of calling CPluginFSInterfaceAbstract::TryCloseOrDetach();
// in brackets are possible values of forceClose ("FALSE->TRUE" means "first try without force,
// if FS refuses, ask user and possibly do it with force") and canDetach:
//
// (FALSE, TRUE) when changing path outside FS opened in panel
#define FSTRYCLOSE_CHANGEPATH 1
// (FALSE->TRUE, FALSE) for FS opened in panel when unloading plugin (user wants to unload +
// close Salamander + before removing plugin + unload on request of plugin)
#define FSTRYCLOSE_UNLOADCLOSEFS 2
// (FALSE, TRUE) when changing path or refreshing (Ctrl+R) FS opened in panel, it was found
// that no path on FS is accessible - Salamander tries to change path in panel to fixed-drive
// (if FS refuses, FS remains in panel without files and directories)
#define FSTRYCLOSE_CHANGEPATHFAILURE 3
// (FALSE, TRUE) when connecting detached FS back to panel, it was found that no path on FS
// is accessible - Salamander tries to close this detached FS (if FS refuses, FS remains
// in list of detached FS - for example in Alt+F1/F2 menu)
#define FSTRYCLOSE_ATTACHFAILURE 4
// (FALSE->TRUE, FALSE) for detached FS when unloading plugin (user wants to unload +
// close Salamander + before removing plugin + unload on request of plugin)
#define FSTRYCLOSE_UNLOADCLOSEDETACHEDFS 5
// (FALSE, FALSE) plugin called CSalamanderGeneral::CloseDetachedFS() for detached FS
#define FSTRYCLOSE_PLUGINCLOSEDETACHEDFS 6

// flags signalling which file-system services are provided by plugin - which methods
// CPluginFSInterfaceAbstract are defined):
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

// missing: Change Case, Convert, Properties, Make File List

// types of context menus for method CPluginFSInterfaceAbstract::ContextMenu()
#define fscmItemsInPanel 0 // context menu for items in panel (selected/focused files and directories)
#define fscmPathInPanel 1  // context menu for current path in panel
#define fscmPanel 2        // context menu for panel

#define SALCMDLINE_MAXLEN 8192 // maximal length of a command from Salamander command line

class CPluginFSInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect calling of methods (see CPluginFSInterfaceEncapsulation)
    friend class CPluginFSInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns user-part of the current path in this FS, 'userPart' is buffer of size MAX_PATH
    // for path, returns success
    virtual BOOL WINAPI GetCurrentPath(char* userPart) = 0;

    // returns user-part of the full name of file/directory/up-dir 'file' ('isDir' is 0/1/2)
    // on the current path in this FS; for up-dir (first in the list of directories and also
    // named ".."), 'isDir'==2 and the method should return current path shortened by the last
    // component; 'buf' is buffer of size 'bufSize' for the result full name, returns success
    virtual BOOL WINAPI GetFullName(CFileData& file, int isDir, char* buf, int bufSize) = 0;

    // returns absolute path (including fs-name) corresponding to relative path 'path' on this FS;
    // returns FALSE if this method is not implemented (other return values are ignored);
    // 'parent' is parent of possible messageboxes; 'fsName' is current name of FS; 'path' is
    // buffer of size 'pathSize' characters, on input it contains relative path on FS, on output
    // it contains corresponding absolute path on FS; in 'success' returns TRUE if the path
    // was successfully translated (the string in 'path' should be used - otherwise it is ignored)
    // - then change of path follows (if it is a path on this FS, ChangePath() is called);
    // if it returns FALSE in 'success', it is assumed that user has already seen an error message
    virtual BOOL WINAPI GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize,
                                      BOOL& success) = 0;

    // returns user-part of root of the current path in this FS, 'userPart' is buffer of size MAX_PATH
    // for path (used in function "goto root"), returns success
    virtual BOOL WINAPI GetRootPath(char* userPart) = 0;

    // compares the current path in this FS with the path specified by 'fsNameIndex' and 'userPart'
    // (name of FS in the path is from this plugin and is given by index 'fsNameIndex'), returns TRUE
    // if the paths are the same; 'currentFSNameIndex' is index of current name of FS
    virtual BOOL WINAPI IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // returns TRUE if the path is from this FS (which means that Salamander can run the path
    // in ChangePath of this FS); the path is always from one of FS of this plugin (for example
    // Windows paths and paths to archives are not passed here); 'fsNameIndex' is index of name
    // of FS in the path (index is zero for fs-name specified in CSalamanderPluginEntryAbstract::SetBasicPluginData,
    // for other fs-names index is returned by CSalamanderPluginEntryAbstract::AddFSName);
    // user-part of the path is 'userPart'; 'currentFSNameIndex' is index of current name of FS
    virtual BOOL WINAPI IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart) = 0;

    // changes the current path in this FS to the path specified by 'fsName' and 'userPart'
    // (exact or nearest accessible subpath 'userPart' - see value 'mode'); if the path is
    // shortened because it is a path to a file (only a guess that it is a path to a file
    // is enough - after listing the path it is checked if the file exists, otherwise an error
    // is shown to the user) and 'cutFileName' is not NULL (possible only in 'mode' 3), returns
    // in buffer 'cutFileName' (of size MAX_PATH characters) name of this file (without path),
    // otherwise returns empty string in buffer 'cutFileName'; 'currentFSNameIndex' is index
    // of current name of FS; 'fsName' is buffer of size MAX_PATH, on input it contains name
    // of FS in the path from this plugin (but it does not have to match current name of FS
    // in this object, it is enough if IsOurPath() returns TRUE for it), on output in 'fsName'
    // is current name of FS in this object (it must be from this plugin); 'fsNameIndex' is
    // index of name of FS 'fsName' in plugin (for easier detection of which FS name it is);
    // if 'pathWasCut' is not NULL, it returns TRUE if the path was shortened; Salamander uses
    // 'cutFileName' and 'pathWasCut' in Change Directory command (Shift+F7) when entering
    // file name - the file gets focus; if 'forceRefresh' is TRUE, it is hard refresh (Ctrl+R)
    // and plugin should change path without using information from cache (it is necessary to
    // check if the new path exists); 'mode' is mode of changing path:
    //   1 (refresh path) - shortens the path if necessary; do not show error if the path does
    //                      not exist (without showing shorten), show file instead of path,
    //                      inaccessibility of path and other path errors
    //   2 (calling ChangePanelPathToPluginFS, back/forward in history, etc.) - shortens the path,
    //                      if needed; report all path errors (file instead of path, non-existence,
    //                      inaccessibility and others)
    //   3 (change-dir command) - shortens the path only if it is a file or the path cannot be listed
    //                      (ListCurrentPath returns FALSE for it); do not report file instead of path
    //                      (shorten and return file name without reporting), report all other path
    //                      errors (non-existence, inaccessibility and others)
    // if 'mode' is 1 or 2, returns FALSE only if no path is accessible on this FS (for example
    // connection lost); if 'mode' is 3, returns FALSE if the path or file is not accessible
    // (path is shortened only if it is a file); if opening FS is time-consuming (for example
    // connection to FTP server), and 'mode' is 3, it is possible to change behavior as for archives
    // - shorten the path if needed and return FALSE only if no path is accessible on this FS
    // error reporting is not changed
    virtual BOOL WINAPI ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                   const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                   BOOL forceRefresh, int mode) = 0;

    // loads files and directories from the current path, saves them to object 'dir' (on path
    // NULL or "", files and directories on other paths are ignored; if directory with name ".."
    // is added, it is displayed as "up-dir" symbol; names of files and directories are fully
    // dependent on plugin, Salamander only displays them); Salamander gets content of columns
    // added by plugin using interface 'pluginData' (if plugin does not add columns and does
    // not have its own icons, 'pluginData'==NULL is returned); 'iconsType' returns required
    // way of getting icons of files and directories to panel, pitFromPlugin degrades to pitSimple
    // if 'pluginData' is NULL (pitFromPlugin cannot be provided without 'pluginData'); if
    // 'forceRefresh' is TRUE, it is hard refresh (Ctrl+R) and plugin should load files and
    // directories without using cache; returns TRUE if loading was successful, if it returns
    // FALSE, it is an error and ChangePath on current path will be called, it is expected that
    // ChangePath will select accessible subpath or return FALSE, after successful call of
    // ChangePath, ListCurrentPath will be called again; if it returns FALSE, return value
    // 'pluginData' is ignored (data in 'dir' must be freed using 'dir.Clear(pluginData)',
    // otherwise only Salamander part of data is freed);
    virtual BOOL WINAPI ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                        CPluginDataInterfaceAbstract*& pluginData,
                                        int& iconsType, BOOL forceRefresh) = 0;

    // preparation of FS for closing/detaching from panel or closing of detached FS; if
    // 'forceClose' is TRUE, FS is closed regardless of return values, the action was
    // forced by user or critical shutdown is in progress (see CSalamanderGeneralAbstract::IsCriticalShutdown);
    // anyway, it does not make sense to ask user anything, FS should be closed immediately
    // (no opening of any windows); if 'forceClose' is FALSE, FS can be closed/detached
    // ('canDetach'==TRUE) or only closed ('canDetach'==FALSE); 'detach' returns TRUE if
    // FS wants to detach, FALSE means close; 'reason' contains reason of calling this method
    // (one of FSTRYCLOSE_XXX); returns TRUE if it can be closed/detached, otherwise returns FALSE
    virtual BOOL WINAPI TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason) = 0;

    // receipt of event on this FS, see codes of events FSE_XXX; 'param' is parameter of event
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // release of all resources of this FS except listing data (during calling of this method
    // listing can be displayed in panel); it is called just before deleting listing in panel
    // (listing is deleted only for active FS, detached FS do not have listing) and CloseFS
    // for this FS; 'parent' is parent of possible messageboxes, if critical shutdown is in
    // progress (see CSalamanderGeneralAbstract::IsCriticalShutdown for more information),
    // no windows should be displayed
    virtual void WINAPI ReleaseObject(HWND parent) = 0;

    // getting a set of services supported by FS (see constants FS_SERVICE_XXX); it returns
    // logical sum of constants; it is called after opening of this FS (see CPluginInterfaceForFSAbstract::OpenFS),
    // and then after every call of ChangePath and ListCurrentPath of this FS
    virtual DWORD WINAPI GetSupportedServices() = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM:
    // getting item for this FS (active or detached) for Change Drive menu (Alt+F1/F2) or
    // Disconnect dialog (hotkey: F12; possible disconnect of this FS is ensured by method
    // CPluginInterfaceForFSAbstract::DisconnectFS; if GetChangeDriveOrDisconnectItem returns
    // FALSE and FS is in panel, item with icon obtained by GetFSIcon and root path is added);
    // if return value is TRUE, item with icon 'icon' and text 'title' is added; 'fsName' is
    // current name of FS; if 'icon' is NULL, item has no icon; if 'destroyIcon' is TRUE and
    // 'icon' is not NULL, 'icon' is freed after using it by Win32 API function DestroyIcon;
    // 'title' is text allocated on Salamander heap and can contain up to three columns
    // separated by '\t' (see Alt+F1/F2 menu), in Disconnect dialog only second column is used;
    // if return value is FALSE, return values 'title', 'icon' and 'destroyIcon' are ignored
    // (item is not added)
    virtual BOOL WINAPI GetChangeDriveOrDisconnectItem(const char* fsName, char*& title,
                                                       HICON& icon, BOOL& destroyIcon) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETFSICON:
    // getting icon of FS for toolbar directory line or possibly for Disconnect dialog (F12);
    // icon for Disconnect dialog is obtained only if for this FS method GetChangeDriveOrDisconnectItem
    // does not return item (for example RegEdit and WMobile); it returns icon or NULL if
    // standard icon should be used; if 'destroyIcon' is TRUE and returns icon (not NULL),
    // returned icon is freed after using it by Win32 API function DestroyIcon
    // Caution: if resource of icon is loaded using LoadIcon in size 16x16, LoadIcon returns
    //          32x32 icon; when drawing it to 16x16, color contours are created around the icon;
    //          conversion 16->32->16 can be avoided by using LoadImage:
    //          (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(id), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
    //
    // no windows can be displayed in this method (content of panel is not consistent, no
    // messages can be distributed - repainting, etc.)
    virtual HICON WINAPI GetFSIcon(BOOL& destroyIcon) = 0;

    // returns requested drop-effect for drag&drop operation from FS (can be this FS) to this FS;
    // 'srcFSPath' is source path; 'tgtFSPath' is target path (is from this FS); 'allowedEffects'
    // contains allowed drop-effects; 'keyState' is state of keys (combination of flags MK_CONTROL,
    // MK_SHIFT, MK_ALT, MK_BUTTON, MK_LBUTTON, MK_MBUTTON and MK_RBUTTON, see IDropTarget::Drop);
    // 'dropEffect' contains recommended drop-effects (equal to 'allowedEffects' or limited to
    // DROPEFFECT_COPY or DROPEFFECT_MOVE if user holds Ctrl or Shift keys) and returns selected
    // drop-effect (DROPEFFECT_COPY, DROPEFFECT_MOVE or DROPEFFECT_NONE); if method 'dropEffect'
    // does not change 'dropEffect' and it contains more effects, priority selection of Copy
    // operation is performed
    virtual void WINAPI GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                      DWORD allowedEffects, DWORD keyState,
                                      DWORD* dropEffect) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETFREESPACE:
    // returns size of free space on FS in 'retValue' (must not be NULL) (displayed on right
    // in directory line); if free space cannot be obtained, returns CQuadWord(-1, -1) (value
    // is not displayed)
    virtual void WINAPI GetFSFreeSpace(CQuadWord* retValue) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // finding separating points in text of Directory Line (for shortening of path using mouse
    // - hot-tracking); 'text' is text in Directory Line (path + possibly filter); 'pathLen'
    // is length of path in 'text' (rest is filter); 'offset' is offset of character from which
    // to find separating point; returns TRUE if next separating point exists, its position
    // is returned in 'offset', returns FALSE if no next separating point exists (end of text
    // is not considered as separating point)
    virtual BOOL WINAPI GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETNEXTDIRLINEHOTPATH:
    // modification of the text of shortened path which is displayed in panel (Directory Line
    // - shortening of path using mouse - hot-tracking); used if hot-text from Directory Line
    // does not exactly correspond to the path (for example it lacks closing bracket - VMS
    // paths on FTP - "[DIR1.DIR2.DIR3]"); 'path' is in/out buffer with path (size of buffer
    // is 'pathBufSize')
    virtual void WINAPI CompleteDirectoryLineHotPath(char* path, int pathBufSize) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_GETPATHFORMAINWNDTITLE:
    // getting text which is displayed in title of main window if displaying of current path
    // in title of main window is enabled (see Configuration/Appearance/Display current path...);
    // 'fsName' is current name of FS; if 'mode' is 1, it is "Directory Name Only" mode (only
    // name of current directory - last component of path - is displayed); if 'mode' is 2, it
    // is "Shortened Path" mode (shortened form of path is displayed - root (including path
    // separator) + "..." + path separator + last component of path); 'buf' is buffer of size
    // 'bufSize' for result text; returns TRUE if it returns requested text; returns FALSE if
    // the text should be created using information about separating points obtained by method
    // GetNextDirectoryLineHotPath()
    // NOTE: if GetSupportedServices() does not return FS_SERVICE_GETPATHFORMAINWNDTITLE,
    //       full path on FS is displayed in title of main window in all display modes
    //       (including "Directory Name Only" and "Shortened Path")
    virtual BOOL WINAPI GetPathForMainWindowTitle(const char* fsName, int mode, char* buf, int bufSize) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_SHOWINFO:
    // show dialog with information about FS (free space, capacity, name, options, etc.);
    // 'fsName' is current name of FS; 'parent' is proposed parent of displayed dialog
    virtual void WINAPI ShowInfoDialog(const char* fsName, HWND parent) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_COMMANDLINE:
    // runs command for FS in active panel from command line under panels; returns FALSE on error
    // (command is not inserted into command line history and other return values are ignored);
    // returns TRUE on successful execution of command (caution: the result of command is not
    // important - only if it was executed (for example for FTP it is important if it was
    // delivered to server)); 'parent' is proposed parent of possible displayed dialogs;
    // 'command' is buffer of size SALCMDLINE_MAXLEN+1, on input it contains executed command
    // (real maximal length of command depends on Windows version and content of environment
    // variable COMSPEC) and on output it contains new content of command line (usually it is
    // only cleared to empty string); 'selFrom' and 'selTo' return position of selection in
    // new content of command line (if they are equal, only cursor is placed; if output is
    // empty line, these values are ignored)
    // CAUTION: this method should not change path in panel directly - closing of FS can occur
    //          in case of path error ('this' pointer of this method would cease to exist)
    virtual BOOL WINAPI ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_QUICKRENAME:
    // quick rename of file or directory ('isDir' is FALSE/TRUE) 'file' on FS; allows to open
    // own dialog for quick rename (parameter 'mode' is 1) or to use standard dialog (if 'mode'==1,
    // it returns FALSE and 'cancel' is also FALSE, then Salamander opens standard dialog and
    // new name is returned in 'newName' in next call of QuickRename with 'mode'==2); 'fsName'
    // is current name of FS; 'parent' is proposed parent of possible displayed dialogs;
    // 'newName' is new name if 'mode'==2; if it returns TRUE, 'newName' contains new name
    // (max. MAX_PATH characters; not full name, only name of item in panel) - Salamander tries
    // to focus it after refresh (FS takes care of refresh, for example using method
    // CSalamanderGeneralAbstract::PostRefreshPanelFS); if it returns FALSE and 'mode'==2,
    // 'newName' contains incorrect new name (possibly modified in some way - for example
    // operation mask can be already applied) if user wants to cancel operation, it returns
    // 'cancel' TRUE; if 'cancel' is FALSE, method returns TRUE on successful completion of
    // operation, if it returns FALSE on 'mode'==1, standard dialog for quick rename should
    // be opened, if it returns FALSE on 'mode'==2, it is an error of operation (incorrect
    // new name is returned in 'newName' - standard dialog is opened again and user can correct
    // incorrect name)
    virtual BOOL WINAPI QuickRename(const char* fsName, int mode, HWND parent, CFileData& file,
                                    BOOL isDir, char* newName, BOOL& cancel) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_ACCEPTSCHANGENOTIF:
    // receipt of information about change on path 'path' (if 'includingSubdirs' is TRUE, it
    // includes also change in subdirectories of path 'path'); this method should decide if
    // it is necessary to refresh this FS (for example using method
    // CSalamanderGeneralAbstract::PostRefreshPanelFS); it concerns both active FS and detached
    // FS; 'fsName' is current name of FS
    // NOTE: for plugin as a whole there is method
    //       CPluginInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* fsName, const char* path,
                                                       BOOL includingSubdirs) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_CREATEDIR:
    // creation of new directory on FS; allows to open own dialog for creation of directory
    // (parameter 'mode' is 1) or to use standard dialog (if 'mode'==1, it returns FALSE and
    // 'cancel' is also FALSE, then Salamander opens standard dialog and new name is returned
    // in 'newName' in next call of CreateDir with 'mode'==2); 'fsName' is current name of FS;
    // 'parent' is proposed parent of possible displayed dialogs; 'newName' is new name if
    // 'mode'==2; if it returns TRUE, 'newName' contains new name (max. 2 * MAX_PATH characters;
    // not full name, only name of item in panel) - Salamander tries to focus it after refresh
    // (FS takes care of refresh, for example using method CSalamanderGeneralAbstract::PostRefreshPanelFS);
    // if it returns FALSE and 'mode'==2, 'newName' contains incorrect new name (max. 2 * MAX_PATH
    // characters, possibly modified to absolute form) if user wants to cancel operation, it
    // returns 'cancel' TRUE; if 'cancel' is FALSE, method returns TRUE on successful completion
    // of operation, if it returns FALSE on 'mode'==1, standard dialog for creation of directory
    // should be opened, if it returns FALSE on 'mode'==2, it is an error of operation (incorrect
    // new name is returned in 'newName' - standard dialog is opened again and user can correct
    // incorrect name)
    virtual BOOL WINAPI CreateDir(const char* fsName, int mode, HWND parent,
                                  char* newName, BOOL& cancel) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_VIEWFILE:
    // viewing of file 'file' (directory cannot be viewed using the function View) on the current
    // path on FS; 'fsName' is current name of FS; 'parent' is parent of possible messageboxes
    // with errors; 'salamander' is set of methods from Salamander necessary for implementation
    // of viewing with caching
    virtual void WINAPI ViewFile(const char* fsName, HWND parent,
                                 CSalamanderForViewFileOnFSAbstract* salamander,
                                 CFileData& file) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_DELETE:
    // deletion of files and directories selected in panel; allows to open own dialog for
    // confirmation of deletion (parameter 'mode' is 1; if it should or should not be displayed
    // depends on value of SALCFG_CNFRMFILEDIRDEL - TRUE means that user wants to confirm deletion)
    // or to use standard dialog (if 'mode'==1, it returns FALSE and 'cancelOrError' is also
    // FALSE, then Salamander opens standard question (if SALCFG_CNFRMFILEDIRDEL is TRUE) and
    // in case of positive answer calls Delete with 'mode'==2); 'fsName' is current name of FS;
    // 'parent' is proposed parent of possible displayed dialogs; 'panel' identifies panel
    // (PANEL_LEFT or PANEL_RIGHT) in which FS is opened (files and directories to delete are
    // obtained from this panel); 'selectedFiles' + 'selectedDirs' - number of selected files
    // and directories, if both values are zero, file/directory under cursor (focus) is deleted,
    // before calling method Delete, either files and directories are selected or at least
    // focus is on file/directory, so there is always something to work with (no other tests
    // are necessary); if it returns TRUE and 'cancelOrError' is FALSE, operation was successful
    // and selected files and directories should be deselected (if they survived deletion);
    // if user wants to cancel operation or error occurs, 'cancelOrError' returns TRUE and
    // files and directories won't be deselected; if it returns FALSE on 'mode'==1 and
    // 'cancelOrError' is FALSE, standard question for deletion should be opened
    virtual BOOL WINAPI Delete(const char* fsName, int mode, HWND parent, int panel,
                               int selectedFiles, int selectedDirs, BOOL& cancelOrError) = 0;

    // copy/move from FS (parameter 'copy' is TRUE/FALSE), in the rest of text only copying
    // is mentioned, but everything is the same for moving; 'copy' can be TRUE (copying) only
    // if GetSupportedServices() returns FS_SERVICE_COPYFROMFS; 'copy' can be FALSE (moving
    // or renaming) only if GetSupportedServices() returns FS_SERVICE_MOVEFROMFS as well;
    //
    // copying of files and directories (from FS) marked in panel; allows to open own dialog
    // for entering target path (parameter 'mode' is 1) or to use standard dialog (returns
    // FALSE and 'cancelOrHandlePath' is also FALSE, then Salamander opens standard dialog
    // and passes the obtained target path in 'targetPath' in next call of CopyOrMoveFromFS
    // with 'mode'==2); for 'mode'==2, 'targetPath' is a string entered by user (CopyOrMoveFromFS
    // can analyze it on its own); if CopyOrMoveFromFS supports only windows target paths
    // (or cannot process user-entered path - for example it leads to another FS or to archive),
    // it can use standard processing of path in Salamander (currently it can process only
    // windows paths, in the future it can process FS and archive paths using TEMP directory
    // and sequence of basic operations) - it returns FALSE, 'cancelOrHandlePath' TRUE and
    // 'operationMask' TRUE/FALSE (supports/does not support operation masks - if it does not
    // support operation masks and there is an operation mask in path, error message is displayed),
    // then Salamander processes path returned in 'targetPath' (currently only splitting of
    // windows path to existing part, non-existing part and possible mask; it also allows to
    // create subdirectories from non-existing part) and if the path is correct, it calls
    // CopyOrMoveFromFS again with 'mode'==3 and in 'targetPath' with target path and possibly
    // also with operation mask (two strings separated by zero; no mask -> two zeros at the
    // end of string), if there is some error in path, it calls CopyOrMoveFromFS again with
    // 'mode'==4 in 'targetPath' with corrected incorrect target path (error was already
    // reported to user; user should be allowed to correct the path; "." and ".." can be
    // removed from path, etc.);
    //
    // if user executes the operation using drag&drop (drops files/directories from FS to
    // panel or to another drop-target), 'mode'==5 and 'targetPath' contains target path
    // (can be windows path, FS path or in the future also path to archive), 'targetPath'
    // is terminated by two zeros (for compatibility with 'mode'==3); 'dropTarget' is in
    // this case window of drop-target (it is used for reactivation of drop-target after
    // opening of progress window of operation, see CSalamanderGeneralAbstract::ActivateDropTarget);
    // for 'mode'==5, only return value TRUE makes sense;
    //
    // 'fsName' is current name of FS; 'parent' is proposed parent of possible displayed dialogs;
    // 'panel' identifies panel (PANEL_LEFT or PANEL_RIGHT) in which FS is opened (files and
    // directories to copy are obtained from this panel); 'selectedFiles' + 'selectedDirs' -
    // number of selected files and directories, if both values are zero, file/directory under
    // cursor (focus) is copied, before calling method CopyOrMoveFromFS, either files and
    // directories are selected or at least focus is on file/directory, so there is always
    // something to work with (no other tests are necessary); on input 'targetPath' in 'mode'==1
    // contains proposed target path (only windows path without mask or empty string), in 'mode'==2
    // contains target path entered by user in standard dialog, in 'mode'==3 contains target path
    // and mask (separated by zero), in 'mode'==4 contains incorrect target path, in 'mode'==5
    // contains target path (Windows, FS or archive) terminated by two zeros; if the method
    // returns FALSE, 'targetPath' on output (buffer 2 * MAX_PATH characters) with
    // 'cancelOrHandlePath'==FALSE contains proposed target path for standard dialog and with
    // 'cancelOrHandlePath'==TRUE contains target path for processing; if the method returns
    // TRUE and 'cancelOrHandlePath' is FALSE, 'targetPath' contains name of item to which
    // focus should be moved in source panel (buffer 2 * MAX_PATH characters; not full name,
    // only name of item in panel; if it is empty string, focus remains unchanged); 'dropTarget'
    // is not NULL only if path was entered using drag&drop (see above)
    //
    // if it returns TRUE and 'cancelOrHandlePath' is FALSE, operation was successful and
    // selected files and directories should be deselected; if user wants to cancel operation
    // or error occurs, the method returns TRUE and 'cancelOrHandlePath' is TRUE, in both
    // cases files and directories won't be deselected; if it returns FALSE, a standard dialog
    // should be opened ('cancelOrHandlePath' is FALSE) or path should be processed in standard
    // way ('cancelOrHandlePath' is TRUE)
    //
    // NOTE: if the option to copy/paste to target panel path is offered, it is necessary to
    //       call CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath for target panel
    //       otherwise the path in this panel will not be inserted into List of Working Directories
    //       (Alt+F12)
    virtual BOOL WINAPI CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                         int panel, int selectedFiles, int selectedDirs,
                                         char* targetPath, BOOL& operationMask,
                                         BOOL& cancelOrHandlePath, HWND dropTarget) = 0;

    // copying/moving from window path to FS (parameter 'copy' is TRUE/FALSE), in the following
    // text only copying is mentioned, but everything is the same for moving; 'copy' can be TRUE
    // (copying) only if GetSupportedServices() returns FS_SERVICE_COPYFROMDISKTOFS as well; 'copy'
    // can be FALSE (moving or renaming) only if GetSupportedServices() returns FS_SERVICE_MOVEFROMDISKTOFS;
    //
    // copying selected (in panel or elsewhere) files and directories to FS; in 'mode'==1, it
    // allows preparing text of target path for user in standard dialog for copying, this is the
    // case when the source panel (panel where the copying is started (F5 key)) has windows path
    // and the target panel has this FS; in 'mode'==2 and 'mode'==3, the plugin can perform
    // copying or report one of two errors: "error in path" (for example it contains invalid
    // characters or does not exist) and "this operation cannot be performed in this FS" (for
    // example it is FTP, but the opened path in this FS is different from the target path
    // (for example different FTP server) - it is necessary to open another/new FS);
    // a different/new FS needs to be opened; this error cannot be reported by newly opened FS);
    //
    // CAUTION: this method can be called for any target FS path of this plugin (it can be
    //          a path with different name of FS of this plugin)
    //
    // 'fsName' is the current name of FS; 'parent' is proposed parent of possible displayed
    // dialogs; 'sourcePath' is source windows path (all selected files and directories are
    // addressed relatively to this it), if 'mode'==1, it is NULL; the selected files and
    // directories are specified by enumeration function 'next' with parameter 'nextParam';
    // in 'mode'==1, they are NULL; 'sourceFiles' + 'sourceDirs' - number of selected files
    // and directories (sum is always non-zero); 'targetPath' is in/out buffer for target path
    // (min. 2 * MAX_PATH characters); in 'mode'==1, 'targetPath' on input is current path
    // in this FS and on output is proposed target path for standard dialog; in 'mode'==2,
    // 'targetPath' on input is target path entered by user (without any changes, including
    // mask, etc.) and on output it is ignored, except for the case when the method returns
    // FALSE (error) and 'invalidPathOrCancel' is TRUE (error in path), in this case 'targetPath'
    // on output is modified target path (for example "." and ".." are removed) and user should
    // correct it in standard dialog; in 'mode'==3, 'targetPath' on input is target path specified
    // by drag&drop and on output it is ignored; if 'invalidPathOrCancel' is not NULL (only
    // 'mode'==2 and 'mode'==3), it returns TRUE if the path is incorrectly entered (contains
    // invalid characters or does not exist, etc.) or the operation was canceled (cancel) - error
    // message/cancel message is displayed before the end of this method;
    //
    // in 'mode'==1, the method returns TRUE for success, if it returns FALSE, and empty string
    // is used as target path for standard copy dialog; if the method returns FALSE in 'mode'==2
    // or 'mode'==3, other FS should be searched for processing of the operation (if 'invalidPathOrCancel'
    // is FALSE) or user should correct target path (if 'invalidPathOrCancel' is TRUE); if the method
    // returns TRUE in 'mode'==2 or 'mode'==3, the operation was successful and selected files
    // and directories should be deselected (if 'invalidPathOrCancel' is FALSE) or an error/cancel
    // of the operation occurred and selected files and directories should not be deselected
    // (if 'invalidPathOrCancel' is TRUE)
    //
    // CAUTION: CopyOrMoveFromDiskToFS method can be called in three situations:
    //          - this FS is active in panel
    //          - this FS is detached
    //          - this FS has just been created (by calling OpenFS) and after the end of the
    //            method it will immediately disappear again (by calling CloseFS) - no other
    //            method (including ChangePath) was called from it
    virtual BOOL WINAPI CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                               const char* sourcePath, SalEnumSelection2 next,
                                               void* nextParam, int sourceFiles, int sourceDirs,
                                               char* targetPath, BOOL* invalidPathOrCancel) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_CHANGEATTRS:
    // changing of attributes of files and directories selected in panel; each plugin has its
    // own dialog for specifying attributes changes;
    // 'fsName' is current name of FS; 'parent' is proposed parent for the custom dialog;
    // 'panel' identifies panel (PANEL_LEFT or PANEL_RIGHT) in which FS is opened (files and
    // directories to work with are obtained from this panel);
    // 'selectedFiles' + 'selectedDirs' - number of selected files and directories, if both
    // values are zero, file/directory under cursor (focus) is used, before calling method
    // ChangeAttributes, either files and directories are selected or at least focus is on
    // file/directory, so there is always something to work with (no other tests are necessary);
    // if it returns TRUE, operation was successful and selected files and directories should
    // be deselected; if user wants to cancel operation or error occurs, it returns FALSE and
    // files and directories won't be deselected
    virtual BOOL WINAPI ChangeAttributes(const char* fsName, HWND parent, int panel,
                                         int selectedFiles, int selectedDirs) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_SHOWPROPERTIES:
    // displaying of properties of files and directories selected in panel; each plugin has its
    // own dialog for displaying properties;
    // 'fsName' is current name of FS; 'parent' is proposed parent for the custom dialog;
    // (Windows dialog with properties is non-modal - caution: non-modal dialog must have
    // its own thread); 'panel' identifies panel (PANEL_LEFT or PANEL_RIGHT) in which FS is
    // opened (files and directories to work with are obtained from this panel);
    // 'selectedFiles' + 'selectedDirs' - number of selected files and directories, if both
    // values are zero, file/directory under cursor (focus) is used, before calling method
    // ShowProperties, either files and directories are selected or at least focus is on
    // file/directory, so there is always something to work with (no other tests are necessary)
    virtual void WINAPI ShowProperties(const char* fsName, HWND parent, int panel,
                                       int selectedFiles, int selectedDirs) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_CONTEXTMENU:
    // displaying of context menu for files and directories selected in panel (right mouse
    // button click on items in panel) or for current path in panel (right mouse button click
    // on change-drive button in panel toolbar) or for panel (right mouse button click behind
    // items in panel); each plugin has its own context menu;
    //
    // 'fsName' is current name of FS; 'parent' is proposed parent for the context menu;
    // 'menuX' + 'menuY' are proposed coordinates of left upper corner of context menu;
    // 'type' is type of context menu (see descriptions of constants fscmXXX); 'panel'
    // identifies panel (PANEL_LEFT or PANEL_RIGHT) for which context menu should be opened
    // (files/directory/path are obtained from this panel); for 'type'==fscmItemsInPanel,
    // 'selectedFiles' + 'selectedDirs' are number of selected files and directories, if both
    // values are zero, file/directory under cursor (focus) is used, before calling method
    // ContextMenu, either files and directories are selected (and user clicked on them) or
    // at least focus is on file/directory (not on updir), so there is always something to
    // work with (no other tests are necessary); if 'type'!=fscmItemsInPanel, 'selectedFiles'
    // + 'selectedDirs' are always set to zero (they are ignored)
    virtual void WINAPI ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                    int panel, int selectedFiles, int selectedDirs) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_CONTEXTMENU:
    // if there is FS opened in panel and WM_INITPOPUP, WM_DRAWITEM, WM_MENUCHAR or WM_MEASUREITEM
    // message arrives, Salamander calls HandleMenuMsg to allow plugin to work with
    // IContextMenu2 and IContextMenu3;
    // plugin returns TRUE if it processesed the message and FALSE otherwise
    virtual BOOL WINAPI HandleMenuMsg(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_OPENFINDDLG:
    // opening of Find dialog for FS in panel; 'fsName' is current name of FS; 'panel' identifies
    // panel (PANEL_LEFT or PANEL_RIGHT) for which Find dialog should be opened (the path for
    // searching is usually obtained from this panel); returns TRUE on successful opening of
    // Find dialog; if it returns FALSE, Salamander opens standard Find Files and Directories
    // dialog
    virtual BOOL WINAPI OpenFindDialog(const char* fsName, int panel) = 0;

    // only if GetSupportedServices() also returns FS_SERVICE_OPENACTIVEFOLDER:
    // opens Explorer window for current path in panel; 'fsName' is current name of FS;
    // 'parent' is proposed parent for displayed dialog
    virtual void WINAPI OpenActiveFolder(const char* fsName, HWND parent) = 0;

    // only if GetSupportedServices() returns FS_SERVICE_MOVEFROMFS nebo FS_SERVICE_COPYFROMFS:
    // allows to influence allowed drop-effects during drag&drop from this FS; if 'allowedEffects'
    // is not NULL, it contains on input allowed drop-effects (combination of DROPEFFECT_MOVE
    // and DROPEFFECT_COPY), on output it contains drop-effects allowed by this FS (effects
    // should only be removed); 'mode' is 0 during call which immediately precedes start of
    // drag&drop operation, effects returned in 'allowedEffects' are used for call of DoDragDrop
    // (it concerns whole drag&drop operation); 'mode' is 1 during dragging of mouse over FS
    // from this process (it can be this FS or FS from other panel); for 'mode'==1, 'tgtFSPath'
    // is target path which is used if drop occurs, otherwise 'tgtFSPath' is NULL; 'mode' is 2
    // during call which immediately follows end of drag&drop operation (successful or not)
    virtual void WINAPI GetAllowedDropEffects(int mode, const char* tgtFSPath, DWORD* allowedEffects) = 0;

    // allows plugin to modify standard message "There are no items in this panel." displayed
    // in situation when there is no item (file/directory/up-dir) in panel; returns FALSE if
    // standard message should be used (return value 'textBuf' is ignored); returns TRUE if
    // plugin returns its own alternative of this message in 'textBuf' (buffer of size
    // 'textBufSize' characters)
    virtual BOOL WINAPI GetNoItemsInPanelText(char* textBuf, int textBufSize) = 0;

    // only if GetSupportedServices() returns FS_SERVICE_SHOWSECURITYINFO:
    // used clicked security icon (see CSalamanderGeneralAbstract::ShowSecurityIcon;
    // for example FTPS shows dialog with server certificate); 'parent' is proposed
    // parent of displayed dialog
    virtual void WINAPI ShowSecurityInfo(HWND parent) = 0;

    /* to be finished:
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
private: // protection against incorrect calling of methods (see CPluginInterfaceForFSEncapsulation)
    friend class CPluginInterfaceForFSEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // function for "file system"; called for opening FS; 'fsName' is name of FS to open;
    // 'fsNameIndex' is index of FS name to open (index is zero for FS name specified in
    // CSalamanderPluginEntryAbstract::SetBasicPluginData, for other FS names index is
    // returned by CSalamanderPluginEntryAbstract::AddFSName); returns pointer to interface
    // of opened FS CPluginFSInterfaceAbstract or NULL on error
    virtual CPluginFSInterfaceAbstract* WINAPI OpenFS(const char* fsName, int fsNameIndex) = 0;

    // function for "file system", called after closing FS; 'fs' is interface of closed FS,
    // after this call 'fs' interface is considered invalid in Salaamnder and will not be
    // used anymore (function pairs with OpenFS)
    // CAUTION: in this method it openinig of any window or dialog is not allowed
    //          (windows can be opened in method CPluginFSInterfaceAbstract::ReleaseObject)
    virtual void WINAPI CloseFS(CPluginFSInterfaceAbstract* fs) = 0;

    // performing command on an item for FS in Change Drive menu or in Drive bar
    // (its addition see CSalamanderConnectAbstract::SetChangeDriveMenuItem);
    // 'panel' identifies panel which we will work with - for command from Change Drive menu
    // 'panel' is always PANEL_SOURCE (this menu can be opened only in active panel), for command
    // from Drive bars 'panel' can be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled,
    // we can work also with inactive panel)
    virtual void WINAPI ExecuteChangeDriveMenuItem(int panel) = 0;

    // opening of context menu on item for FS in Change Drive menu or in Drive bar
    // or for active/detached FS in Change Drive menu; 'parent' is parent of the context menu;
    // 'x' and 'y' are coordinates for opening of the context menu (place of right mouse button
    // click or proposed coordinates for Shift+F10); if 'pluginFS' is NULL, it is FS item,
    // otherwise 'pluginFS' is interface of active/detached FS (see 'isDetachedFS'); if 'pluginFS' is
    // not NULL, 'pluginFSName' is name of FS opened in 'pluginFS' (otherwise 'pluginFSName' is NULL);
    // 'pluginFSNameIndex' is index of FS name in 'pluginFS' (for easier detection of FS name;
    // otherwise 'pluginFSNameIndex' is -1); if the method returns FALSE, other return values
    // are ignored, otherwise they have the following meaning: 'refreshMenu' returns TRUE if
    // Change Drive menu should be refreshed (for Drive bars it is ignored, because active/detached
    // FS are not displayed on them); 'closeMenu' returns TRUE if Change Drive menu should be
    // closed (for Drive bars there is nothing to close); if 'closeMenu' is TRUE and 'postCmd'
    // is not NULL, after closing of Change Drive menu (for Drive bars immediately) method
    // ExecuteChangeDrivePostCommand is called with parameters 'postCmd' and 'postCmdParam';
    // 'panel' identifies panel which we will work with - for context menu in Change Drive menu
    // 'panel' is always PANEL_SOURCE (this menu can be opened only in active panel), for context
    // menu in Drive bars 'panel' can be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled,
    // we can work also with inactive panel)
    virtual BOOL WINAPI ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                       CPluginFSInterfaceAbstract* pluginFS,
                                                       const char* pluginFSName, int pluginFSNameIndex,
                                                       BOOL isDetachedFS, BOOL& refreshMenu,
                                                       BOOL& closeMenu, int& postCmd, void*& postCmdParam) = 0;

    // performing command from context menu on item for FS or for active/detached FS in Change
    // Drive menu only after closing Change Drive menu or performing command from context menu
    // on item for FS in Drive bars (for compatibility with Change Drive menu only); called
    // as a reaction to return values 'closeMenu' (TRUE), 'postCmd' and 'postCmdParam' from
    // ChangeDriveMenuItemContextMenu after closing Change Drive menu (for Drive bars immediately);
    // 'panel' identifies panel which we will work with - for context menu in Change Drive menu
    // 'panel' is always PANEL_SOURCE (this menu can be opened only in active panel), for context
    // menu in Drive bars 'panel' can be PANEL_LEFT or PANEL_RIGHT (if two Drive bars are enabled,
    // we can work also with inactive panel)
    virtual void WINAPI ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam) = 0;

    // performs execustion of the item of panel with opened FS (e.g. reaction to pressing of
    // Enter key in panel; for subdirectory/up-dir (up-dir means both that the name is ".." and it
    // is the first directory) it is supposed that path will be changed, for file a copy of the
    // file on disk will be opened with possible loading of changes from FS); execution cannot
    // be performed in FS interface, because it is not possible to call methods for changing
    // path there (closing of FS can occur there); 'panel' identifies panel in which execution
    // is performed (PANEL_LEFT or PANEL_RIGHT); 'pluginFS' is interface of FS opened in panel;
    // 'pluginFSName' is name of FS opened in panel; 'pluginFSNameIndex' is index of FS name
    // in 'pluginFS' (for easier detection of FS name); 'file' is executed file/directory/up-dir
    // ('isDir' is 0/1/2);
    // CAUTION: calling of method for changing path in panel can invalidate 'pluginFS' (after
    //          closing of FS) and 'file'+'isDir' (change of listing in panel -> deletion of
    //          items of original listing)
    // NOTE: if file is executed or otherwise worked with (e.g. downloaded), it is necessary
    //       to call CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath for panel 'panel',
    //       otherwise the path in this panel will not be inserted into List of Working Directories
    //       (Alt+F12)
    virtual void WINAPI ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex,
                                    CFileData& file, int isDir) = 0;

    // disconnects FS, requested by user in Disconnect dialog; 'parent' is parent
    // of possible messageboxes (it is still opened Disconnect dialog); disconnect
    // cannot be performed in FS interface method, because FS has to disappear;
    // 'isInPanel' is TRUE if FS is in panel, then 'panel' identifies panel
    // (PANEL_LEFT or PANEL_RIGHT); 'isInPanel' is FALSE if FS is detached, then
    // 'panel' is 0; 'pluginFS' is interface of FS; 'pluginFSName' is name of FS;
    // 'pluginFSNameIndex' is index of FS name (for easier detection of FS name);
    // method returns FALSE if disconnect was not successful and Disconnect dialog
    // should remain opened (its content is refreshed to show previous successful
    // disconnects)
    virtual BOOL WINAPI DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                     CPluginFSInterfaceAbstract* pluginFS,
                                     const char* pluginFSName, int pluginFSNameIndex) = 0;

    // converts user-part paths in buffer 'fsUserPart' (size MAX_PATH characters) from
    // external to internal format (for example for FTP: internal format = paths as used
    // by server, external format = URL format = paths contain hex-escape-sequences
    // (for example "%20" = " "))
    virtual void WINAPI ConvertPathToInternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // converts user-part paths in buffer 'fsUserPart' (size MAX_PATH characters) from
    // internal to external format
    virtual void WINAPI ConvertPathToExternal(const char* fsName, int fsNameIndex,
                                              char* fsUserPart) = 0;

    // this method is only called for the plugin which is used as replacement for Network
    // item in Change Drive menu and in Drive bars (see CSalamanderGeneralAbstract::SetPluginIsNethood);
    // By this call, Salamander informs the plugin that user is changing path from root UNC
    // path "\\server\share" through symbol of up-dir ("..") to plugin FS path with user-part
    // "\\server" in panel 'panel' (PANEL_LEFT or PANEL_RIGHT); purpose of this method:
    // plugin should without waiting list at least this one share on this path, so it can
    // be focused in panel (which is normal behavior when changing path through up-dir)
    virtual void WINAPI EnsureShareExistsOnServer(int panel, const char* server, const char* share) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_fs)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__

/*   Preliminary version of help for plugin interface
 
 Opening, changing, listing and refreshing of path:
  - for opening of path in new FS, ChangePath is called (first call of ChangePath is always
    for opening of path)
  - for changing of path, ChangePath is called (second and all other calls of ChangePath
    are for changing of path)
  - for fatal error, ChangePath returns FALSE (FS path is not opened in panel, if it was
    for changing of path, original path is tried to be opened again, if it is not possible,
    fixed-drive path is opened)
  - if ChangePath returns TRUE (success) and path was not shortened to the original one (whose
    listing is already loaded), ListCurrentPath is called for obtaining of new listing
  - after successful listing, ListCurrentPath returns TRUE
  - for fatal error, ListCurrentPath returns FALSE and next call of ChangePath must also
    return FALSE
  - if current path cannot be listed, ListCurrentPath returns FALSE and next call of ChangePath
    must change path and return TRUE (ListCurrentPath is called again), if path cannot be
    changed (root, etc.), ChangePath also returns FALSE (FS path is not opened in panel,
    if it was for changing of path, ChangePath is called again for original path, if it is
    not possible, fixed-drive path is opened)
  - refresh of path (Ctrl+R) behaves the same as changing of path to current path (path
    does not have to change at all or it can be shortened or in case of fatal error changed
    to fixed-drive); for refresh of path, parameter 'forceRefresh' is TRUE for all calls
    of methods ChangePath and ListCurrentPath (FS must not use any cache for changing of
    path or loading of listing - user does not want to use cache);
 
 When browsing history (back/forward), FS interface in which listing of FS path
 ('fsName':'fsUserPart') is performed is obtained by first possible way from the following:
   - FS interface in which path was last opened was not closed yet and is among detached
     interfaces or is active in panel (is not active in second panel)
   - active FS interface in panel ('currentFSName') is from the same plugin as 'fsName'
     and returns TRUE on IsOurPath('currentFSName', 'fsName', 'fsUserPart')
   - first of detached FS interfaces ('currentFSName') is from the same plugin as 'fsName'
     and returns TRUE on IsOurPath('currentFSName', 'fsName', 'fsUserPart')
   - new FS interface
*/
