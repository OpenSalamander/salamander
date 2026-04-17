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
#pragma pack(push, enter_include_spl_gen) // to keep structures independent of the current packing/alignment setting
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

struct CFileData;
class CPluginDataInterfaceAbstract;

// ****************************************************************************
// CSalamanderGeneralAbstract
//
// General-purpose Salamander methods (for all plugin types)

// Message box types
#define MSGBOX_INFO 0
#define MSGBOX_ERROR 1
#define MSGBOX_EX_ERROR 2
#define MSGBOX_QUESTION 3
#define MSGBOX_EX_QUESTION 4
#define MSGBOX_WARNING 5
#define MSGBOX_EX_WARNING 6

// Constants for CSalamanderGeneralAbstract::SalMessageBoxEx
#define MSGBOXEX_OK 0x00000000                // MB_OK
#define MSGBOXEX_OKCANCEL 0x00000001          // MB_OKCANCEL
#define MSGBOXEX_ABORTRETRYIGNORE 0x00000002  // MB_ABORTRETRYIGNORE
#define MSGBOXEX_YESNOCANCEL 0x00000003       // MB_YESNOCANCEL
#define MSGBOXEX_YESNO 0x00000004             // MB_YESNO
#define MSGBOXEX_RETRYCANCEL 0x00000005       // MB_RETRYCANCEL
#define MSGBOXEX_CANCELTRYCONTINUE 0x00000006 // MB_CANCELTRYCONTINUE
#define MSGBOXEX_CONTINUEABORT 0x00000007     // MB_CONTINUEABORT
#define MSGBOXEX_YESNOOKCANCEL 0x00000008

#define MSGBOXEX_ICONHAND 0x00000010        // MB_ICONHAND / MB_ICONSTOP / MB_ICONERROR
#define MSGBOXEX_ICONQUESTION 0x00000020    // MB_ICONQUESTION
#define MSGBOXEX_ICONEXCLAMATION 0x00000030 // MB_ICONEXCLAMATION / MB_ICONWARNING
#define MSGBOXEX_ICONINFORMATION 0x00000040 // MB_ICONASTERISK / MB_ICONINFORMATION

#define MSGBOXEX_DEFBUTTON1 0x00000000 // MB_DEFBUTTON1
#define MSGBOXEX_DEFBUTTON2 0x00000100 // MB_DEFBUTTON2
#define MSGBOXEX_DEFBUTTON3 0x00000200 // MB_DEFBUTTON3
#define MSGBOXEX_DEFBUTTON4 0x00000300 // MB_DEFBUTTON4

#define MSGBOXEX_HELP 0x00004000 // MB_HELP (bit mask)

#define MSGBOXEX_SETFOREGROUND 0x00010000 // MB_SETFOREGROUND (bit mask)

// altap specific
#define MSGBOXEX_SILENT 0x10000000 // The message box does not play any sound when opened (bit mask)
// For an MB_YESNO message box, Escape is allowed (generates IDNO); for an MB_ABORTRETRYIGNORE message box,
// Escape is allowed (generates IDCANCEL) (bit mask)
#define MSGBOXEX_ESCAPEENABLED 0x20000000
#define MSGBOXEX_HINT 0x40000000 // If CheckBoxText is used, a \t separator is searched for in it and displayed as a hint
// Vista: the default button will require elevation (an elevated icon is displayed)
#define MSGBOXEX_SHIELDONDEFBTN 0x80000000

#define MSGBOXEX_TYPEMASK 0x0000000F // MB_TYPEMASK
#define MSGBOXEX_ICONMASK 0x000000F0 // MB_ICONMASK
#define MSGBOXEX_DEFMASK 0x00000F00  // MB_DEFMASK
#define MSGBOXEX_MODEMASK 0x00003000 // MB_MODEMASK
#define MSGBOXEX_MISCMASK 0x0000C000 // MB_MISCMASK
#define MSGBOXEX_EXMASK 0xF0000000

// Message box return values
#define DIALOG_FAIL 0x00000000 // The dialog could not be opened
// Individual buttons
#define DIALOG_OK 0x00000001       // IDOK
#define DIALOG_CANCEL 0x00000002   // IDCANCEL
#define DIALOG_ABORT 0x00000003    // IDABORT
#define DIALOG_RETRY 0x00000004    // IDRETRY
#define DIALOG_IGNORE 0x00000005   // IDIGNORE
#define DIALOG_YES 0x00000006      // IDYES
#define DIALOG_NO 0x00000007       // IDNO
#define DIALOG_TRYAGAIN 0x0000000a // IDTRYAGAIN
#define DIALOG_CONTINUE 0x0000000b // IDCONTINUE
// altap specific
#define DIALOG_SKIP 0x10000000
#define DIALOG_SKIPALL 0x20000000
#define DIALOG_ALL 0x30000000

typedef void(CALLBACK* MSGBOXEX_CALLBACK)(LPHELPINFO helpInfo);

struct MSGBOXEX_PARAMS
{
    HWND HParent;
    const char* Text;
    const char* Caption;
    DWORD Flags;
    HICON HIcon;
    DWORD ContextHelpId;
    MSGBOXEX_CALLBACK HelpCallback;
    const char* CheckBoxText;
    BOOL* CheckBoxValue;
    const char* AliasBtnNames;
    const char* URL;
    const char* URLText;
};

/*
HParent
  Handle to the owner window. Message box is centered to this window.
  If this parameter is NULL, the message box has no owner window.

Text
  Pointer to a null-terminated string that contains the message to be displayed.

Caption
  Pointer to a null-terminated string that contains the message box title.
  If this member is NULL, the default title "Error" is used.

Flags
  Specifies the contents and behavior of the message box.
  This parameter can be a combination of flags from the following groups of flags.

   To indicate the buttons displayed in the message box, specify one of the following values.
    MSGBOXEX_OK                   (MB_OK)
      The message box contains one push button: OK. This is the default.
      Message box can be closed using Escape and return value will be DIALOG_OK (IDOK).
    MSGBOXEX_OKCANCEL             (MB_OKCANCEL)
      The message box contains two push buttons: OK and Cancel.
    MSGBOXEX_ABORTRETRYIGNORE     (MB_ABORTRETRYIGNORE)
      The message box contains three push buttons: Abort, Retry, and Ignore.
      Message box can be closed using Escape when MSGBOXEX_ESCAPEENABLED flag is specified.
      In that case return value will be DIALOG_CANCEL (IDCANCEL).
    MSGBOXEX_YESNOCANCEL          (MB_YESNOCANCEL)
      The message box contains three push buttons: Yes, No, and Cancel.
    MSGBOXEX_YESNO                (MB_YESNO)
      The message box contains two push buttons: Yes and No.
      Message box can be closed using Escape when MSGBOXEX_ESCAPEENABLED flag is specified.
      In that case return value will be DIALOG_NO (IDNO).
    MSGBOXEX_RETRYCANCEL          (MB_RETRYCANCEL)
      The message box contains two push buttons: Retry and Cancel.
    MSGBOXEX_CANCELTRYCONTINUE    (MB_CANCELTRYCONTINUE)
      The message box contains three push buttons: Cancel, Try Again, Continue.

   To display an icon in the message box, specify one of the following values.
    MSGBOXEX_ICONHAND             (MB_ICONHAND / MB_ICONSTOP / MB_ICONERROR)
      A stop-sign icon appears in the message box.
    MSGBOXEX_ICONQUESTION         (MB_ICONQUESTION)
      A question-mark icon appears in the message box.
    MSGBOXEX_ICONEXCLAMATION      (MB_ICONEXCLAMATION / MB_ICONWARNING)
      An exclamation-point icon appears in the message box.
    MSGBOXEX_ICONINFORMATION      (MB_ICONASTERISK / MB_ICONINFORMATION)
      An icon consisting of a lowercase letter i in a circle appears in the message box.

   To indicate the default button, specify one of the following values.
    MSGBOXEX_DEFBUTTON1           (MB_DEFBUTTON1)
      The first button is the default button.
      MSGBOXEX_DEFBUTTON1 is the default unless MSGBOXEX_DEFBUTTON2, MSGBOXEX_DEFBUTTON3,
      or MSGBOXEX_DEFBUTTON4 is specified.
    MSGBOXEX_DEFBUTTON2           (MB_DEFBUTTON2)
      The second button is the default button.
    MSGBOXEX_DEFBUTTON3           (MB_DEFBUTTON3)
      The third button is the default button.
    MSGBOXEX_DEFBUTTON4           (MB_DEFBUTTON4)
      The fourth button is the default button.

   To specify other options, use one or more of the following values.
    MSGBOXEX_HELP                 (MB_HELP)
      Adds a Help button to the message box.
      When the user clicks the Help button or presses F1, the system sends a WM_HELP message to the owner
      or calls HelpCallback (see HelpCallback for details).
    MSGBOXEX_SETFOREGROUND        (MB_SETFOREGROUND)
      The message box becomes the foreground window. Internally, the system calls the SetForegroundWindow
      function for the message box.
    MSGBOXEX_SILENT
      No sound will be played when message box is displayed.
    MSGBOXEX_ESCAPEENABLED
      When MSGBOXEX_YESNO is specified, user can close message box using Escape key and DIALOG_NO (IDNO)
      will be returned. When MSGBOXEX_ABORTRETRYIGNORE is specified, user can close message box using
      Escape key and DIALOG_CANCEL (IDCANCEL) will be returned. Otherwise this option is ignored.

HIcon
  Handle to the icon to be drawn in the message box. Icon will not be destroyed when messagebox is closed.
  If this parameter is NULL, MSGBOXEX_ICONxxx style will be used.

ContextHelpId
  Identifies a help context. If a help event occurs, this value is specified in
  the HELPINFO structure that the message box sends to the owner window or callback function.

HelpCallback
  Pointer to the callback function that processes help events for the message box.
  The callback function has the following form:
    VOID CALLBACK MSGBOXEX_CALLBACK(LPHELPINFO helpInfo)
  If this member is NULL, the message box sends WM_HELP messages to the owner window
  when help events occur.

CheckBoxText
  Pointer to a null-terminated string that contains the checkbox text.
  If the MSGBOXEX_HINT flag is specified in the Flags, this text must contain HINT.
  Hint is separated from string by the TAB character. Hint is divided by the second TAB character
  on two parts. The first part is label, that will be displayed behind the check box.
  The second part is the text displayed when user clicks the hint label.

  Example: "This is text for checkbox\tHint Label\tThis text will be displayed when user click the Hint Label."
  If this member is NULL, checkbox will not be displayed.

CheckBoxValue
  Pointer to a BOOL variable contains the checkbox initial and return state (TRUE: checked, FALSE: unchecked).
  This parameter is ignored if CheckBoxText parameter is NULL. Otherwise this parameter must be set.

AliasBtnNames
  Pointer to a buffer containing pairs of id and alias strings. The last string in the
  buffer must be terminated by NULL character.

  The first string in each pair is a decimal number that specifies button ID.
  Number must be one of the DIALOG_xxx values. The second string specifies alias text
  for this button.

  First and second string in each pair are separated by TAB character.
  Pairs are separated by TAB character too.

  If this member is NULL, normal names of buttons will displayed.

  Example: sprintf(buffer, "%d\t%s\t%d\t%s", DIALOG_OK, "&Start", DIALOG_CANCEL, "E&xit");
           buffer: "1\t&Start\t2\tE&xit"

URL
  Pointer to a null-terminated string that contains the URL displayed below text.
  If this member is NULL, the URL is not displayed.

URLText
  Pointer to a null-terminated string that contains the URL text displayed below text.
  If this member is NULL, the URL is displayed instead.

*/

// Panel identifiers
#define PANEL_SOURCE 1 // Source panel (active panel)
#define PANEL_TARGET 2 // Target panel (inactive panel)
#define PANEL_LEFT 3   // Left panel
#define PANEL_RIGHT 4  // Right panel

// Path types
#define PATH_TYPE_WINDOWS 1 // Windows path ("c:\path" or UNC path)
#define PATH_TYPE_ARCHIVE 2 // Path inside an archive (the archive lies on a Windows path)
#define PATH_TYPE_FS 3      // Path on a plugin file system

// Only one flag from the following group can be selected.
// They define the set of buttons displayed in various error messages.
#define BUTTONS_OK 0x00000000               // OK
#define BUTTONS_RETRYCANCEL 0x00000001      // Retry / Cancel
#define BUTTONS_SKIPCANCEL 0x00000002       // Skip / Skip all / Cancel
#define BUTTONS_RETRYSKIPCANCEL 0x00000003  // Retry / Skip / Skip all / Cancel
#define BUTTONS_YESALLSKIPCANCEL 0x00000004 // Yes / All / Skip / Skip all / Cancel
#define BUTTONS_YESNOCANCEL 0x00000005      // Yes / No / Cancel
#define BUTTONS_YESALLCANCEL 0x00000006     // Yes / All / Cancel
#define BUTTONS_MASK 0x000000FF             // Internal mask, do not use
// detekci zda kombinace ma tlacitko SKIP nebo YES nechavam zde ve forme inline, aby
// v pripade zavadeni novych kombinaci byla dobre na ocich a nezapomeli jsme ji doplnit
inline BOOL ButtonsContainsSkip(DWORD btn)
{
    return (btn & BUTTONS_MASK) == BUTTONS_SKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_RETRYSKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESALLSKIPCANCEL;
}
inline BOOL ButtonsContainsYes(DWORD btn)
{
    return (btn & BUTTONS_MASK) == BUTTONS_YESALLSKIPCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESNOCANCEL ||
           (btn & BUTTONS_MASK) == BUTTONS_YESALLCANCEL;
}

// Error constants for CSalamanderGeneralAbstract::SalGetFullName
#define GFN_SERVERNAMEMISSING 1   // UNC path is missing the server name
#define GFN_SHARENAMEMISSING 2    // UNC path is missing the share name
#define GFN_TOOLONGPATH 3         // The operation would produce a path that is too long
#define GFN_INVALIDDRIVE 4        // In a normal path (c:\), the drive letter is not A-Z (or a-z)
#define GFN_INCOMLETEFILENAME 5   // Relative path without a specified 'curDir' -> cannot be resolved
#define GFN_EMPTYNAMENOTALLOWED 6 // Empty 'name' string
#define GFN_PATHISINVALID 7       // Cannot rule out "..", e.g. "c:\.."

// Error code for the case when the user aborts CSalamanderGeneralAbstract::SalCheckPath with the ESC key
#define ERROR_USER_TERMINATED -100

#define PATH_MAX_PATH 248 // Limit for the maximum path length (full directory name); note: the limit already includes the null terminator (the maximum string length is 247 characters)

// Error constants for CSalamanderGeneralAbstract::SalParsePath:
// The input path was empty and 'curPath' was NULL (an empty path is replaced with the current path,
// but it is not known here)
#define SPP_EMPTYPATHNOTALLOWED 1
// The Windows path (normal + UNC) does not exist, is not accessible, or the user aborted the path
// accessibility test (this also includes an attempt to restore the network connection)
#define SPP_WINDOWSPATHERROR 2
// Windows path starts with a file name that is not an archive (otherwise it would be a path inside an archive)
#define SPP_NOTARCHIVEFILE 3
// FS path - the plugin FS name (fs-name - before ':' in the path) is unknown (no plugin
// has registered this name)
#define SPP_NOTPLUGINFS 4
// The path is relative, but the current path is unknown, or it is an FS path (there the root cannot be determined
// and the structure of the fs-user-part path is unknown, so it cannot be converted to an absolute path)
// If the current path is an FS path ('curPathIsDiskOrArchive' is FALSE), no error is reported
// to the user in this case (further processing is expected on the FS side that called SalParsePath)
#define SPP_INCOMLETEPATH 5

// Internal Salamander color constants
#define SALCOL_FOCUS_ACTIVE_NORMAL 0 // Pen colors for the border around the item
#define SALCOL_FOCUS_ACTIVE_SELECTED 1
#define SALCOL_FOCUS_FG_INACTIVE_NORMAL 2
#define SALCOL_FOCUS_FG_INACTIVE_SELECTED 3
#define SALCOL_FOCUS_BK_INACTIVE_NORMAL 4
#define SALCOL_FOCUS_BK_INACTIVE_SELECTED 5
#define SALCOL_ITEM_FG_NORMAL 6 // Text colors of items in the panel
#define SALCOL_ITEM_FG_SELECTED 7
#define SALCOL_ITEM_FG_FOCUSED 8
#define SALCOL_ITEM_FG_FOCSEL 9
#define SALCOL_ITEM_FG_HIGHLIGHT 10
#define SALCOL_ITEM_BK_NORMAL 11 // Background colors of items in the panel
#define SALCOL_ITEM_BK_SELECTED 12
#define SALCOL_ITEM_BK_FOCUSED 13
#define SALCOL_ITEM_BK_FOCSEL 14
#define SALCOL_ITEM_BK_HIGHLIGHT 15
#define SALCOL_ICON_BLEND_SELECTED 16 // Icon blend colors
#define SALCOL_ICON_BLEND_FOCUSED 17
#define SALCOL_ICON_BLEND_FOCSEL 18
#define SALCOL_PROGRESS_FG_NORMAL 19 // Progress bar colors
#define SALCOL_PROGRESS_FG_SELECTED 20
#define SALCOL_PROGRESS_BK_NORMAL 21
#define SALCOL_PROGRESS_BK_SELECTED 22
#define SALCOL_HOT_PANEL 23           // Hot item color in the panel
#define SALCOL_HOT_ACTIVE 24          // In the active window caption
#define SALCOL_HOT_INACTIVE 25        // In the inactive caption, status bar, ...
#define SALCOL_ACTIVE_CAPTION_FG 26   // Text color in the active panel title
#define SALCOL_ACTIVE_CAPTION_BK 27   // Background color in the active panel title
#define SALCOL_INACTIVE_CAPTION_FG 28 // Text color in the inactive panel title
#define SALCOL_INACTIVE_CAPTION_BK 29 // Background color in the inactive panel title
#define SALCOL_VIEWER_FG_NORMAL 30    // Text color in the internal text/hex viewer
#define SALCOL_VIEWER_BK_NORMAL 31    // Background color in the internal text/hex viewer
#define SALCOL_VIEWER_FG_SELECTED 32  // Selected text color in the internal text/hex viewer
#define SALCOL_VIEWER_BK_SELECTED 33  // Selected background color in the internal text/hex viewer
#define SALCOL_THUMBNAIL_NORMAL 34    // Pen colors for the border around the thumbnail
#define SALCOL_THUMBNAIL_SELECTED 35
#define SALCOL_THUMBNAIL_FOCUSED 36
#define SALCOL_THUMBNAIL_FOCSEL 37

// Constants for the reasons why CSalamanderGeneralAbstract::ChangePanelPathToXXX methods returned failure:
#define CHPPFR_SUCCESS 0 // The panel contains the new path; success (return value is TRUE)
// The new path (or archive name) cannot be converted from relative to absolute, or
// the new path (or archive name) is not accessible, or
// the FS path cannot be opened (no plugin, the plugin refuses to load, it refuses to open the FS, or a fatal ChangePath error occurred)
#define CHPPFR_INVALIDPATH 1
#define CHPPFR_INVALIDARCHIVE 2  // The file is not an archive or cannot be listed as an archive
#define CHPPFR_CANNOTCLOSEPATH 4 // The current path cannot be closed
// The panel contains a shortened new path,
// for FS specifically: the panel contains either the shortened new path, the original path, or the shortened
// original path - the original path is restored in the panel only if the new path was opened
// in the current FS (the IsOurPath method returned TRUE for it) and the new path is not accessible
// (and neither is any of its subpaths)
#define CHPPFR_SHORTERPATH 5
// The panel contains a shortened new path; it was shortened because the requested path was a file
// name - the panel contains the path to the file and the file will be focused
#define CHPPFR_FILENAMEFOCUSED 6

// Types for CSalamanderGeneralAbstract::ValidateVarString() and CSalamanderGeneralAbstract::ExpandVarString()
typedef const char*(WINAPI* FSalamanderVarStrGetValue)(HWND msgParent, void* param);
struct CSalamanderVarStrEntry
{
    const char* Name;                  // Name of the variable in the string (e.g. in the string "$(name)" it is "name")
    FSalamanderVarStrGetValue Execute; // Function that returns the text representing the variable
};

class CSalamanderRegistryAbstract;

// callback type used for configuration load/save via
// CSalamanderGeneral::CallLoadOrSaveConfiguration; 'regKey' is NULL when loading
// the default configuration (save is not called with 'regKey' == NULL); 'registry' is an object for
// working with the registry; 'param' is a user-defined parameter of the function (see
// CSalamanderGeneral::CallLoadOrSaveConfiguration)
typedef void(WINAPI* FSalLoadOrSaveConfiguration)(BOOL load, HKEY regKey,
                                                  CSalamanderRegistryAbstract* registry, void* param);

// base structure for CSalamanderGeneralAbstract::ViewFileInPluginViewer (each plugin
// viewer can extend this structure with its own parameters; the structure is passed to
// CPluginInterfaceForViewerAbstract::ViewFile; parameters can be, for example, the window title,
// viewer mode, offset from the beginning of the file, selection position, etc.); WARNING: structure
// packing must be 4 bytes (see "#pragma pack(4)")
struct CSalamanderPluginViewerData
{
    // Number of bytes from the start of the structure that are valid (to distinguish structure versions)
    int Size;
    // Name of the file to open in the viewer (do not use in the method
    // CPluginInterfaceForViewerAbstract::ViewFile - the file name is given by the 'name' parameter)
    const char* FileName;
};

// extension of the CSalamanderPluginViewerData structure for the internal text/hex viewer
struct CSalamanderPluginInternalViewerData : public CSalamanderPluginViewerData
{
    int Mode;            // 0 - textovy mod, 1 - hexa mod
    const char* Caption; // NULL -> the window caption contains FileName, otherwise Caption
    BOOL WholeCaption;   // Meaningful only if Caption != NULL. TRUE ->
                         // only the Caption string is shown in the viewer title; FALSE ->
                         // Caption is followed by the standard " - Viewer".
};

// Salamander configuration parameter type constants (see CSalamanderGeneralAbstract::GetConfigParameter)
#define SALCFGTYPE_NOTFOUND 0 // parameter not found
#define SALCFGTYPE_BOOL 1     // TRUE/FALSE
#define SALCFGTYPE_INT 2      // 32-bit integer
#define SALCFGTYPE_STRING 3   // null-terminated multibyte string
#define SALCFGTYPE_LOGFONT 4  // Win32 LOGFONT structure

// constants for Salamander configuration parameters (see CSalamanderGeneralAbstract::GetConfigParameter);
// the parameter type is given in the comment (BOOL, INT, STRING); for STRING, the required
// buffer size is given in parentheses
//
// general parameters
#define SALCFG_SELOPINCLUDEDIRS 1        // BOOL, select/deselect operations (num *, num +, num -) work also with directories
#define SALCFG_SAVEONEXIT 2              // BOOL, save configuration on Salamander exit
#define SALCFG_MINBEEPWHENDONE 3         // BOOL, should it beep (play sound) after end of work in inactive window?
#define SALCFG_HIDEHIDDENORSYSTEMFILES 4 // BOOL, should it hide system and/or hidden files?
#define SALCFG_ALWAYSONTOP 6             // BOOL, main window is Always On Top?
#define SALCFG_SORTUSESLOCALE 7          // BOOL, should it use regional settings when sorting?
#define SALCFG_SINGLECLICK 8             // BOOL, single click mode (single click to open file, etc.)
#define SALCFG_TOPTOOLBARVISIBLE 9       // BOOL, is top toolbar visible?
#define SALCFG_BOTTOMTOOLBARVISIBLE 10   // BOOL, is bottom toolbar visible?
#define SALCFG_USERMENUTOOLBARVISIBLE 11 // BOOL, is user-menu toolbar visible?
#define SALCFG_INFOLINECONTENT 12        // STRING (200), content of Information Line (string with parameters)
#define SALCFG_FILENAMEFORMAT 13         // INT, how to alter file name before displaying (parameter 'format' to CSalamanderGeneralAbstract::AlterFileName)
#define SALCFG_SAVEHISTORY 14            // BOOL, may history related data be stored to configuration?
#define SALCFG_ENABLECMDLINEHISTORY 15   // BOOL, is command line history enabled?
#define SALCFG_SAVECMDLINEHISTORY 16     // BOOL, may command line history be stored to configuration?
#define SALCFG_MIDDLETOOLBARVISIBLE 17   // BOOL, is middle toolbar visible?
#define SALCFG_SORTDETECTNUMBERS 18      // BOOL, should it use numerical sort for numbers contained in strings when sorting?
#define SALCFG_SORTBYEXTDIRSASFILES 19   // BOOL, should it treat dirs as files when sorting by extension? BTW, if TRUE, directories extensions are also displayed in separated Ext column. (directories have no extensions, only files have extensions, but many people have requested sort by extension and displaying extension in separated Ext column even for directories)
#define SALCFG_SIZEFORMAT 20             // INT, units for custom size columns, 0 - Bytes, 1 - KB, 2 - short (mixed B, KB, MB, GB, ...)
#define SALCFG_SELECTWHOLENAME 21        // BOOL, should be whole name selected (including extension) when entering new filename? (for dialog boxes F2:QuickRename, Alt+F5:Pack, etc)
// recycle bin parameters
#define SALCFG_USERECYCLEBIN 50   // INT, 0 - do not use, 1 - use for all, 2 - use for files matching at \
                                  //      least one of masks (see SALCFG_RECYCLEBINMASKS)
#define SALCFG_RECYCLEBINMASKS 51 // STRING (MAX_PATH), masks for SALCFG_USERECYCLEBIN==2
// time resolution of file compare (used in command Compare Directories)
#define SALCFG_COMPDIRSUSETIMERES 60 // BOOL, should it use time resolution? (FALSE==exact match)
#define SALCFG_COMPDIRTIMERES 61     // INT, time resolution for file compare (from 0 to 3600 second)
// confirmations
#define SALCFG_CNFRMFILEDIRDEL 70 // BOOL, files or directories delete
#define SALCFG_CNFRMNEDIRDEL 71   // BOOL, non-empty directory delete
#define SALCFG_CNFRMFILEOVER 72   // BOOL, file overwrite
#define SALCFG_CNFRMSHFILEDEL 73  // BOOL, system or hidden file delete
#define SALCFG_CNFRMSHDIRDEL 74   // BOOL, system or hidden directory delete
#define SALCFG_CNFRMSHFILEOVER 75 // BOOL, system or hidden file overwrite
#define SALCFG_CNFRMCREATEPATH 76 // BOOL, show "do you want to create target path?" in Copy/Move operations
#define SALCFG_CNFRMDIROVER 77    // BOOL, directory overwrite (copy/move selected directory: ask user if directory already exists on target path - standard behaviour is to join contents of both directories)
// drive specific settings
#define SALCFG_DRVSPECFLOPPYMON 88         // BOOL, floppy disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECFLOPPYSIM 89         // BOOL, floppy disks - use simple icons
#define SALCFG_DRVSPECREMOVABLEMON 90      // BOOL, removable disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECREMOVABLESIM 91      // BOOL, removable disks - use simple icons
#define SALCFG_DRVSPECFIXEDMON 92          // BOOL, fixed disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECFIXEDSIMPLE 93       // BOOL, fixed disks - use simple icons
#define SALCFG_DRVSPECREMOTEMON 94         // BOOL, remote (network) disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECREMOTESIMPLE 95      // BOOL, remote (network) disks - use simple icons
#define SALCFG_DRVSPECREMOTEDONOTREF 96    // BOOL, remote (network) disks - do not refresh on activation of Salamander
#define SALCFG_DRVSPECCDROMMON 97          // BOOL, CDROM disks - use automatic refresh (changes monitoring)
#define SALCFG_DRVSPECCDROMSIMPLE 98       // BOOL, CDROM disks - use simple icons
#define SALCFG_IFPATHISINACCESSIBLEGOTO 99 // STRING (MAX_PATH), path where to go if path in panel is inaccessible
// internal text/hex viewer
#define SALCFG_VIEWEREOLCRLF 120          // BOOL, accept CR-LF ("\r\n") line ends?
#define SALCFG_VIEWEREOLCR 121            // BOOL, accept CR ("\r") line ends?
#define SALCFG_VIEWEREOLLF 122            // BOOL, accept LF ("\n") line ends?
#define SALCFG_VIEWEREOLNULL 123          // BOOL, accept NULL ("\0") line ends?
#define SALCFG_VIEWERTABSIZE 124          // INT, size of tab ("\t") character in spaces
#define SALCFG_VIEWERSAVEPOSITION 125     // BOOL, TRUE = save position of viewer window, FALSE = always use position of main window
#define SALCFG_VIEWERFONT 126             // LOGFONT, viewer font
#define SALCFG_VIEWERWRAPTEXT 127         // BOOL, wrap text (divide long text line to more lines)
#define SALCFG_AUTOCOPYSELTOCLIPBOARD 128 // BOOL, TRUE = selected text is copied to the clipboard immediately
// archivers
#define SALCFG_ARCOTHERPANELFORPACK 140    // BOOL, should it pack to other panel path?
#define SALCFG_ARCOTHERPANELFORUNPACK 141  // BOOL, should it unpack to other panel path?
#define SALCFG_ARCSUBDIRBYARCFORUNPACK 142 // BOOL, should it unpack to subdirectory named by archive?
#define SALCFG_ARCUSESIMPLEICONS 143       // BOOL, should it use simple icons in archives?

// callback type used by CSalamanderGeneral::SalSplitGeneralPath
typedef BOOL(WINAPI* SGP_IsTheSamePathF)(const char* path1, const char* path2);

// callback type used by CSalamanderGeneralAbstract::CallPluginOperationFromDisk
// 'sourcePath' is the source path on disk (all other paths are relative to it);
// selected files/directories are provided by the enumeration function 'next', whose parameter is
// 'nextParam'; 'param' is passed to CallPluginOperationFromDisk as 'param'
typedef void(WINAPI* SalPluginOperationFromDisk)(const char* sourcePath, SalEnumSelection2 next,
                                                 void* nextParam, void* param);

// flags for text search algorithms (CSalamanderBMSearchData and CSalamanderREGEXPSearchData);
// flags can be combined with bitwise OR
#define SASF_CASESENSITIVE 0x01 // case-sensitive search (if not set, the search is case-insensitive)
#define SASF_FORWARD 0x02       // forward search (if not set, the search runs backward)

// icons for GetSalamanderIcon
#define SALICON_EXECUTABLE 1    // exe/bat/pif/com
#define SALICON_DIRECTORY 2     // dir
#define SALICON_NONASSOCIATED 3 // non-associated file
#define SALICON_ASSOCIATED 4    // associated file
#define SALICON_UPDIR 5         // up-dir ".."
#define SALICON_ARCHIVE 6       // archive

// icon sizes for GetSalamanderIcon
#define SALICONSIZE_16 1 // 16x16
#define SALICONSIZE_32 2 // 32x32
#define SALICONSIZE_48 3 // 48x48

// interface of the Boyer-Moore text-search object
// WARNING: each allocated object may be used only from a single thread
// (it does not have to be the main thread, and different objects may use different threads)
class CSalamanderBMSearchData
{
public:
    // sets the pattern; 'pattern' is a null-terminated pattern string; 'flags' are algorithm flags
    // (see the SASF_XXX constants)
    virtual void WINAPI Set(const char* pattern, WORD flags) = 0;

    // sets the pattern; 'pattern' is a binary pattern of length 'length' (the 'pattern' buffer must
    // be at least ('length' + 1) bytes long; this is only for compatibility with text patterns);
    // 'flags' are algorithm flags (see the SASF_XXX constants)
    virtual void WINAPI Set(const char* pattern, const int length, WORD flags) = 0;

    // sets algorithm flags; 'flags' are algorithm flags (see SASF_XXX constants)
    virtual void WINAPI SetFlags(WORD flags) = 0;

    // returns the pattern length (usable only after a successful call to Set)
    virtual int WINAPI GetLength() const = 0;

    // returns the pattern (usable only after a successful call to Set)
    virtual const char* WINAPI GetPattern() const = 0;

    // returns TRUE if searching can start (the pattern and flags were set successfully;
    // only an empty pattern can still cause failure)
    virtual BOOL WINAPI IsGood() const = 0;

    // searches for the pattern in 'text' of length 'length' from offset 'start' forward;
    // returns the offset of the found pattern, or -1 if the pattern was not found;
    // WARNING: the algorithm must have the SASF_FORWARD flag set
    virtual int WINAPI SearchForward(const char* text, int length, int start) = 0;

    // searches for the pattern in 'text' of length 'length' backward (starts searching at the end of the text);
    // returns the offset of the found pattern, or -1 if the pattern was not found;
    // WARNING: the algorithm must not have the SASF_FORWARD flag set
    virtual int WINAPI SearchBackward(const char* text, int length) = 0;
};

// interface of the regular-expression search object
// WARNING: each allocated object may be used only from a single thread
// (it does not have to be the main thread, and different objects may use different threads)
class CSalamanderREGEXPSearchData
{
public:
    // sets the regular expression; 'pattern' is a null-terminated regular-expression string; 'flags'
    // are algorithm flags (see the SASF_XXX constants); returns FALSE on error, and the error text
    // can be obtained by calling GetLastErrorText
    virtual BOOL WINAPI Set(const char* pattern, WORD flags) = 0;

    // sets the algorithm flags; 'flags' are algorithm flags (see the SASF_XXX constants);
    // returns FALSE on error, and the error text can be obtained by calling GetLastErrorText
    virtual BOOL WINAPI SetFlags(WORD flags) = 0;

    // returns the error text from the last call to Set or SetFlags (may be NULL)
    virtual const char* WINAPI GetLastErrorText() const = 0;

    // returns the regular-expression text (usable only after a successful call to Set)
    virtual const char* WINAPI GetPattern() const = 0;

    // sets the line to search (the line is from 'start' to 'end', and 'end' points past the last character);
    // always returns TRUE
    virtual BOOL WINAPI SetLine(const char* start, const char* end) = 0;

    // searches the line set by SetLine for a substring matching the regular expression;
    // searches forward from offset 'start'; returns the offset of the found substring and its length
    // (in 'foundLen'), or -1 if no substring was found;
    // WARNING: the algorithm must have the SASF_FORWARD flag set
    virtual int WINAPI SearchForward(int start, int& foundLen) = 0;

    // searches the line set by SetLine for a substring matching the regular expression;
    // searches backward (starting at the end of the text segment of length 'length' from the start of the line);
    // returns the offset of the found substring and its length (in 'foundLen'), or -1 if no substring
    // was found;
    // WARNING: the algorithm must not have the SASF_FORWARD flag set
    virtual int WINAPI SearchBackward(int length, int& foundLen) = 0;
};

// types of Salamander commands used in CSalamanderGeneralAbstract::EnumSalamanderCommands
#define sctyUnknown 0
#define sctyForFocusedFile 1                 // for the focused file only (e.g. View)
#define sctyForFocusedFileOrDirectory 2      // for the focused file or directory (e.g. Open)
#define sctyForSelectedFilesAndDirectories 3 // for selected or focused files and directories (e.g. Copy)
#define sctyForCurrentPath 4                 // for the current path in the panel (e.g. Create Directory)
#define sctyForConnectedDrivesAndFS 5        // for connected drives and FS (e.g. Disconnect)

// Salamander commands used in CSalamanderGeneralAbstract::EnumSalamanderCommands
// and CSalamanderGeneralAbstract::PostSalamanderCommand
// (WARNING: only the range <0, 499> is reserved for command numbers)
#define SALCMD_VIEW 0     // view (F3 in the panel)
#define SALCMD_ALTVIEW 1  // alternate view (Alt+F3 in the panel)
#define SALCMD_VIEWWITH 2 // view with (klavesa Ctrl+Shift+F3 v panelu)
#define SALCMD_EDIT 3     // edit (F4 in the panel)
#define SALCMD_EDITWITH 4 // edit with (klavesa Ctrl+Shift+F4 v panelu)

#define SALCMD_OPEN 20        // open (Enter key in the panel)
#define SALCMD_QUICKRENAME 21 // quick rename (F2 in the panel)

#define SALCMD_COPY 40          // copy (F5 in the panel)
#define SALCMD_MOVE 41          // move/rename (F6 in the panel)
#define SALCMD_EMAIL 42         // email (Ctrl+E in the panel)
#define SALCMD_DELETE 43        // delete (Delete key in the panel)
#define SALCMD_PROPERTIES 44    // show properties (Alt+Enter in the panel)
#define SALCMD_CHANGECASE 45    // change case (Ctrl+F7 in the panel)
#define SALCMD_CHANGEATTRS 46   // change attributes (Ctrl+F2 in the panel)
#define SALCMD_OCCUPIEDSPACE 47 // calculate occupied space (klavesa Alt+F10 v panelu)

#define SALCMD_EDITNEWFILE 70     // edit new file (klavesa Shift+F4 v panelu)
#define SALCMD_REFRESH 71         // refresh (Ctrl+R in a panel)
#define SALCMD_CREATEDIRECTORY 72 // create directory (F7 in a panel)
#define SALCMD_DRIVEINFO 73       // drive info (Ctrl+F1 in a panel)
#define SALCMD_CALCDIRSIZES 74    // calculate directory sizes (klavesa Ctrl+Shift+F10 v panelu)

#define SALCMD_DISCONNECT 90 // disconnect (network drive or plugin-fs) (klavesa F12 v panelu)

#define MAX_GROUPMASK 1001 // maximum number of characters (including the terminating null) in a group mask

// Identifiers of shared histories (last values used in combo boxes) for
// CSalamanderGeneral::GetStdHistoryValues()
#define SALHIST_QUICKRENAME 1 // names in the Quick Rename dialog (F2)
#define SALHIST_COPYMOVETGT 2 // target paths in the Copy/Move dialog (F5/F6)
#define SALHIST_CREATEDIR 3   // directory names in the Create Directory dialog (F7)
#define SALHIST_CHANGEDIR 4   // paths in the Change Directory dialog (Shift+F7)
#define SALHIST_EDITNEW 5     // names in the Edit New dialog (Shift+F4)
#define SALHIST_CONVERT 6     // names in the Convert dialog (Ctrl+K)

// Interface for working with a group of file masks
// WARNING: The object's methods are not synchronized, so they may be used only
//        from a single thread (it does not have to be the main thread), or the
//        plugin must provide its own synchronization (no "write" may be performed
//        while another method is running; "write"=SetMasksString+PrepareMasks;
//        "read" operations may run from multiple threads at the same time; "read"=GetMasksString+
//        AgreeMasks)
//
// Object lifetime:
//   1) Allocate it with CSalamanderGeneralAbstract::AllocSalamanderMaskGroup
//   2) Pass the mask group to SetMasksString.
//   3) Call PrepareMasks to build the internal data; if it fails,
//      show the error position and, after correcting the mask, return to step (3)
//   4) Call AgreeMasks as needed to determine whether a name matches the mask group.
//   5) After calling SetMasksString again, continue from step (3)
//   6) Destroy the object with CSalamanderGeneralAbstract::FreeSalamanderMaskGroup
//
// Mask:
//   '?' - any character
//   '*' - any string, including an empty one
//   '#' - any digit (only if 'extendedMode'==TRUE)
//
//   Examples:
//     *     - all names
//     *.*   - all names
//     *.exe - names with the "exe" extension
//     *.t?? - names with an extension that starts with 't' and contains two more arbitrary characters
//     *.r## - names with an extension that starts with 'r' and contains two more arbitrary digits
//
class CSalamanderMaskGroup
{
public:
    // Sets the mask string (masks are separated by ';' (the escape sequence for ';' is ";;"));
    // 'masks' is the mask string (maximum length including the terminating null is MAX_GROUPMASK)
    // if 'extendedMode' is TRUE, '#' matches any digit ('0'-'9')
    // '|' may be used as a separator; the following masks (again separated by ';')
    // are evaluated inversely, i.e. if a name matches them,
    // AgreeMasks returns FALSE; '|' may appear at the beginning of the string
    //
    //   Examples:
    //     *.txt;*.cpp - all names with the txt or cpp extension
    //     *.h*|*.html - all names with an extension that starts with 'h', except names with the "html" extension
    //     |*.txt      - all names with an extension other than "txt"
    virtual void WINAPI SetMasksString(const char* masks, BOOL extendedMode) = 0;

    // Returns the mask string; 'buffer' is a buffer at least MAX_GROUPMASK characters long
    virtual void WINAPI GetMasksString(char* buffer) = 0;

    // Returns the 'extendedMode' value set by SetMasksString
    virtual BOOL WINAPI GetExtendedMode() = 0;

    // Working with file masks: ('?' any character, '*' any string - including an empty one; if
    //  'extendedMode' in SetMasksString was TRUE, '#' any digit - '0'..'9'):
    // 1) convert the masks to a simpler format; 'errorPos' returns the error position in the mask string;
    //    returns TRUE if no error occurred (FALSE means 'errorPos' is set)
    virtual BOOL WINAPI PrepareMasks(int& errorPos) = 0;
    // 2) we can use the converted masks to test whether any of them matches file 'fileName';
    //    'fileExt' points either to the end of 'fileName' or to the extension (if it exists); 'fileExt'
    //    may be NULL (the extension is found according to the standard rules); returns TRUE if the file
    //    matches at least one of the masks
    virtual BOOL WINAPI AgreeMasks(const char* fileName, const char* fileExt) = 0;
};

// Interface for an MD5 computation object
//
// Object lifetime:
//
//   1) Alokujeme metodou CSalamanderGeneralAbstract::AllocSalamanderMD5
//   2) Call Update() repeatedly for the data whose MD5 should be computed
//   3) Call Finalize()
//   4) Retrieve the computed MD5 with GetDigest()
//   5) If you want to reuse the object, call Init()
//      (it is called automatically in step (1)) and continue with step (2)
//   6) Destrukce objektu metodou CSalamanderGeneralAbstract::FreeSalamanderMD5
//
class CSalamanderMD5
{
public:
    // Initializes the object; it is called automatically in the constructor
    // this method is published so the allocated object can be reused multiple times
    virtual void WINAPI Init() = 0;

    // updates the internal object state from the data block specified by 'input',
    // 'input_length' specifies the buffer size in bytes
    virtual void WINAPI Update(const void* input, DWORD input_length) = 0;

    // Prepares the MD5 for retrieval by GetDigest
    // after Finalize() is called, only GetDigest() and Init() may be called
    virtual void WINAPI Finalize() = 0;

    // Retrieves the MD5; 'dest' must point to a buffer of size 16 bytes
    // this method may be called only after Finalize() has been called
    virtual void WINAPI GetDigest(void* dest) = 0;
};

#define SALPNG_GETALPHA 0x00000002    // when creating the DIB, the alpha channel is also initialized (otherwise it would be 0)
#define SALPNG_PREMULTIPLE 0x00000004 // Meaningful only when SALPNG_GETALPHA is set; premultiplies the RGB components so AlphaBlend() can be called on the bitmap with BLENDFUNCTION::AlphaFormat == AC_SRC_ALPHA

class CSalamanderPNGAbstract
{
public:
    // Creates a bitmap from a PNG resource; 'hInstance' and 'lpBitmapName' specify the resource,
    // 'flags' contains 0 or bits from the SALPNG_xxx family
    // returns a bitmap handle on success, otherwise NULL
    // the plugin is responsible for destroying the bitmap by calling DeleteObject()
    // can be called from any thread
    virtual HBITMAP WINAPI LoadPNGBitmap(HINSTANCE hInstance, LPCTSTR lpBitmapName, DWORD flags, COLORREF unused) = 0;

    // Creates a bitmap from PNG data supplied in memory; 'rawPNG' points to memory containing the PNG
    // (for example loaded from a file) and 'rawPNGSize' specifies the size of the memory occupied by the PNG in bytes,
    // 'flags' contains 0 or bits from the SALPNG_xxx family
    // returns a bitmap handle on success, otherwise NULL
    // the plugin is responsible for destroying the bitmap by calling DeleteObject()
    // can be called from any thread
    virtual HBITMAP WINAPI LoadRawPNGBitmap(const void* rawPNG, DWORD rawPNGSize, DWORD flags, COLORREF unused) = 0;

    // Note 1: it is recommended to compress loaded PNGs with PNGSlim; see https://forum.altap.cz/viewtopic.php?f=15&t=3278
    // Note 2: for an example of direct access to DIB data, see Demoplugin, function AlphaBlend
    // Note 3: supported non-interlaced PNG types are Greyscale, Greyscale with alpha, Truecolour, Truecolour with alpha, and Indexed-colour
    //         with 8 bits per channel
};

// vsechny metody je mozne volat pouze z hlavniho threadu
class CSalamanderPasswordManagerAbstract
{
public:
    // Returns TRUE if the user has configured a master password in Salamander; otherwise returns FALSE
    // (unrelated to whether the MP was entered in this session)
    virtual BOOL WINAPI IsUsingMasterPassword() = 0;

    // Returns TRUE if the user has entered the correct master password in this Salamander session; otherwise returns FALSE
    virtual BOOL WINAPI IsMasterPasswordSet() = 0;

    // Displays a window with parent 'hParent' that prompts for the master password
    // returns TRUE if the correct MP was entered, otherwise returns FALSE
    // prompts even if the master password has already been entered in this Salamander session; see IsMasterPasswordSet()
    // if the user does not use a master password, returns FALSE; see IsUsingMasterPassword()
    virtual BOOL WINAPI AskForMasterPassword(HWND hParent) = 0;

    // Reads the null-terminated 'plainPassword' and, depending on 'encrypt', either encrypts it with AES (if TRUE) or
    // only scrambles it (if FALSE); stores the allocated result in 'encryptedPassword' and returns its size in
    // 'encryptedPasswordSize'; returns TRUE on success, otherwise FALSE
    // if 'encrypt' == TRUE, the caller must ensure before calling the function that the master password has been entered; see AskForMasterPassword()
    // note: returned 'encryptedPassword' is allocated on Salamander's heap; if the plugin does not use salrtl, it must free the buffer
    // with SalamanderGeneral->Free(), otherwise free() is sufficient;
    virtual BOOL WINAPI EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt) = 0;

    // Reads 'encryptedPassword' of size 'encryptedPasswordSize' and converts it to the plain password, which is returned
    // in the allocated buffer 'plainPassword'; returns TRUE on success, otherwise FALSE
    // note: returned 'plainPassword' is allocated on Salamander's heap; if the plugin does not use salrtl, it must free the buffer
    // with SalamanderGeneral->Free(), otherwise free() is sufficient;
    virtual BOOL WINAPI DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword) = 0;

    // Returns TRUE if 'encyptedPassword' of length 'encyptedPasswordSize' is encrypted with AES; otherwise returns FALSE
    virtual BOOL WINAPI IsPasswordEncrypted(const BYTE* encyptedPassword, int encyptedPasswordSize) = 0;
};

// Modes for CSalamanderGeneralAbstract::ExpandPluralFilesDirs
#define epfdmNormal 0   // XXX files and YYY directories
#define epfdmSelected 1 // XXX selected files and YYY selected directories
#define epfdmHidden 2   // XXX hidden files and YYY hidden directories

// commands for HTML help: see CSalamanderGeneralAbstract::OpenHtmlHelp
enum CHtmlHelpCommand
{
    HHCDisplayTOC,     // viz HH_DISPLAY_TOC: dwData = 0 (zadny topic) nebo: pointer to a topic within a compiled help file
    HHCDisplayIndex,   // see HH_DISPLAY_INDEX: dwData = 0 (no keyword) or: keyword to select in the index (.hhk) file
    HHCDisplaySearch,  // viz HH_DISPLAY_SEARCH: dwData = 0 (prazdne hledani) nebo: pointer to an HH_FTS_QUERY structure
    HHCDisplayContext, // viz HH_HELP_CONTEXT: dwData = numeric ID of the topic to display
};

// used as a parameter of OpenHtmlHelpForSalamander when command==HHCDisplayContext
#define HTMLHELP_SALID_PWDMANAGER 1 // displays help for Password Manager

class CPluginFSInterfaceAbstract;

class CSalamanderZLIBAbstract;

class CSalamanderBZIP2Abstract;

class CSalamanderCryptAbstract;

class CSalamanderGeneralAbstract
{
public:
    // Displays a message box with the specified text and caption; the parent window is the HWND
    // returned by GetMsgBoxParent() (see below); uses SalMessageBox (see below)
    // type = MSGBOX_INFO        - information (OK)
    // type = MSGBOX_ERROR       - error message (OK)
    // type = MSGBOX_EX_ERROR    - error message (OK/Cancel) - returns IDOK, IDCANCEL
    // type = MSGBOX_QUESTION    - question (Yes/No) - returns IDYES, IDNO
    // type = MSGBOX_EX_QUESTION - question (Yes/No/Cancel) - returns IDYES, IDNO, IDCANCEL
    // type = MSGBOX_WARNING     - warning (OK)
    // type = MSGBOX_EX_WARNING  - warning (Yes/No/Cancel) - returns IDYES, IDNO, IDCANCEL
    // returns 0 on error
    // main thread only
    virtual int WINAPI ShowMessageBox(const char* text, const char* title, int type) = 0;

    // SalMessageBox and SalMessageBoxEx create, display, and close a message box after
    // one of the buttons is selected. The message box can contain a user-defined caption, message,
    // buttons, an icon, and a checkbox with custom text.
    //
    // If 'hParent' is not currently the foreground window (message box in an inactive application),
    // FlashWindow(mainwnd, TRUE) is called before the message box is shown, and
    // FlashWindow(mainwnd, FALSE) is called after it is closed; mainwnd is the parent of 'hParent'
    // that no longer has a parent (typically the Salamander main window).
    //
    // SalMessageBox fills the MSGBOXEX_PARAMS structure (hParent->HParent, lpText->Text,
    // lpCaption->Caption and uType->Flags; all other structure members are zeroed) and
    // then calls SalMessageBoxEx, so only SalMessageBoxEx is described below.
    //
    // SalMessageBoxEx tries to behave as much as possible like the Windows API functions
    // MessageBox and MessageBoxIndirect. The differences are:
    //   - the message box is centered on hParent (if it is a child window, the non-child parent is used)
    //   - for MB_YESNO/MB_ABORTRETRYIGNORE message boxes, it is possible to enable
    //     closing the window with Escape or by clicking the title-bar close box (flag
    //     MSGBOXEX_ESCAPEENABLED); the return value will then be IDNO/IDCANCEL
    //   - the beep can be suppressed (flag MSGBOXEX_SILENT)
    //
    // Comment for uType: see comment for MSGBOXEX_PARAMS::Flags
    //
    // Return Values
    //    DIALOG_FAIL       (0)            The function fails.
    //    DIALOG_OK         (IDOK)         'OK' button was selected.
    //    DIALOG_CANCEL     (IDCANCEL)     'Cancel' button was selected.
    //    DIALOG_ABORT      (IDABORT)      'Abort' button was selected.
    //    DIALOG_RETRY      (IDRETRY)      'Retry' button was selected.
    //    DIALOG_IGNORE     (IDIGNORE)     'Ignore' button was selected.
    //    DIALOG_YES        (IDYES)        'Yes' button was selected.
    //    DIALOG_NO         (IDNO)         'No' button was selected.
    //    DIALOG_TRYAGAIN   (IDTRYAGAIN)   'Try Again' button was selected.
    //    DIALOG_CONTINUE   (IDCONTINUE)   'Continue' button was selected.
    //    DIALOG_SKIP                      'Skip' button was selected.
    //    DIALOG_SKIPALL                   'Skip All' button was selected.
    //    DIALOG_ALL                       'All' button was selected.
    //
    // SalMessageBox and SalMessageBoxEx can be called from any thread
    virtual int WINAPI SalMessageBox(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType) = 0;
    virtual int WINAPI SalMessageBoxEx(const MSGBOXEX_PARAMS* params) = 0;

    // Returns an HWND suitable as the parent for opened message boxes (or other modal windows),
    // namely the main window, a progress dialog, the Plugins/Plugins dialog, or another modal window
    // opened for the main window
    // main thread only; the returned HWND always belongs to the main thread
    virtual HWND WINAPI GetMsgBoxParent() = 0;

    // Returns the handle of Salamander's main window
    // can be called from any thread
    virtual HWND WINAPI GetMainWindowHWND() = 0;

    // Restores focus to the panel or command line, depending on what was active last; this
    // call is needed if a plugin disables/enables Salamander's main window (this creates
    // situations where the disabled main window becomes active - focus cannot be set in a
    // disabled window - after the main window is enabled again, focus must be restored with this method)
    virtual void WINAPI RestoreFocusInSourcePanel() = 0;

    // Commonly used dialogs, parent window 'parent', return values DIALOG_XXX;
    // if 'parent' is not currently the foreground window (dialog in an inactive application),
    // FlashWindow(mainwnd, TRUE) is called before the dialog is shown and
    // FlashWindow(mainwnd, FALSE) is called after it is closed; mainwnd is the parent of 'parent'
    // that no longer has a parent (typically the Salamander main window)
    // ERROR: filename+error+title (pokud 'title' == NULL, jde o std. titulek "Error")
    //
    // The 'flags' variable specifies the displayed buttons; DialogError accepts one of:
    // BUTTONS_OK               // OK                                    (old DialogError3)
    // BUTTONS_RETRYCANCEL      // Retry / Cancel                        (old DialogError4)
    // BUTTONS_SKIPCANCEL       // Skip / Skip all / Cancel              (old DialogError2)
    // BUTTONS_RETRYSKIPCANCEL  // Retry / Skip / Skip all / Cancel      (old DialogError)
    //
    // all of these can be called from any thread
    virtual int WINAPI DialogError(HWND parent, DWORD flags, const char* fileName, const char* error, const char* title) = 0;

    // CONFIRM FILE OVERWRITE: filename1+filedata1+filename2+filedata2
    // The 'flags' variable specifies the displayed buttons; DialogOverwrite accepts one of:
    // BUTTONS_YESALLSKIPCANCEL // Yes / All / Skip / Skip all / Cancel  (old DialogOverwrite)
    // BUTTONS_YESNOCANCEL      // Yes / No / Cancel                     (old DialogOverwrite2)
    virtual int WINAPI DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                                       const char* fileName2, const char* fileData2) = 0;

    // QUESTION: filename+question+title (pokud 'title' == NULL, jde o std. titulek "Question")
    // The 'flags' variable specifies the displayed buttons; DialogQuestion accepts one of:
    // BUTTONS_YESALLSKIPCANCEL // Yes / All / Skip / Skip all / Cancel  (old DialogQuestion)
    // BUTTONS_YESNOCANCEL      // Yes / No / Cancel                     (old DialogQuestion2)
    // BUTTONS_YESALLCANCEL     // Yes / All / Cancel                    (old DialogQuestion3)
    virtual int WINAPI DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                                      const char* question, const char* title) = 0;

    // If path 'dir' does not exist, allows it to be created (asks the user; if needed, creates
    // multiple directories at the end of the path); if the path exists or is created successfully, returns TRUE;
    // if the path does not exist and 'quiet' is TRUE, it does not ask the user whether the
    // path 'dir' should be created; if 'errBuf' is NULL, errors are shown in windows; if 'errBuf' is not NULL,
    // error descriptions are written to buffer 'errBuf' of size 'errBufSize' (no error windows are
    // opened); all opened windows use 'parent' as their parent; if 'parent' is NULL,
    // Salamander's main window is used; if 'firstCreatedDir' is not NULL, it is a buffer
    // of size MAX_PATH for storing the full name of the first directory created on path
    // 'dir' (returns an empty string if path 'dir' already exists); if 'manualCrDir' is TRUE,
    // it does not allow creating a directory with a leading space in its name (Windows does not mind, but it is
    // potentially dangerous; Explorer does not allow it either)
    // can be called from any thread
    virtual BOOL WINAPI CheckAndCreateDirectory(const char* dir, HWND parent = NULL, BOOL quiet = TRUE,
                                                char* errBuf = NULL, int errBufSize = 0,
                                                char* firstCreatedDir = NULL, BOOL manualCrDir = FALSE) = 0;

    // Checks the free space on 'path' and, if it is not >= totalSize, asks whether the user wants to continue.
    // The dialog has parent 'parent'. Returns TRUE if there is enough space or if the user answered
    // "continue". If 'parent' is not currently the foreground window (a dialog in an inactive application),
    // FlashWindow(mainwnd, TRUE) is called before the dialog is shown and
    // FlashWindow(mainwnd, FALSE) is called after it is closed; mainwnd is the ancestor of 'parent'
    // that no longer has a parent (typically the Salamander main window).
    // 'messageTitle' is shown in the title bar of the question message box and should be
    // the name of the plugin that called the method.
    // Can be called from any thread.
    virtual BOOL WINAPI TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize,
                                      const char* messageTitle) = 0;

    // Returns the free space for the given path in 'retValue' ('retValue' must not be NULL) (currently the most accurate
    // value that can be obtained from Windows; on NT/W2K/XP/Vista it also works with reparse points
    // and SUBST drives (Salamander 2.5 works only with junction points)); 'path' is the path
    // whose free space is queried (it does not have to be the root); if 'total' is not NULL, it receives
    // the total disk size; if an error occurs, it returns CQuadWord(-1, -1)
    // can be called from any thread
    virtual void WINAPI GetDiskFreeSpace(CQuadWord* retValue, const char* path, CQuadWord* total) = 0;

    // Custom clone of Windows GetDiskFreeSpace: it can retrieve correct data for paths containing
    // SUBST drives and reparse points on Windows 2000/XP/Vista/7 (Salamander 2.5 works only
    // with junction points); 'path' is the path whose free space is queried; the remaining parameters
    // correspond to the standard Win32 API GetDiskFreeSpace
    //
    // WARNING: do not use the return values 'lpNumberOfFreeClusters' and 'lpTotalNumberOfClusters', because
    //          on larger disks they contain nonsense (DWORD may not be large enough for the total number of clusters);
    //          use the previous GetDiskFreeSpace method instead, which returns 64-bit numbers
    //
    // can be called from any thread
    virtual BOOL WINAPI SalGetDiskFreeSpace(const char* path, LPDWORD lpSectorsPerCluster,
                                            LPDWORD lpBytesPerSector, LPDWORD lpNumberOfFreeClusters,
                                            LPDWORD lpTotalNumberOfClusters) = 0;

    // Custom clone of Windows GetVolumeInformation: it can retrieve correct data even for
    // paths containing SUBST drives and reparse points on Windows 2000/XP/Vista (Salamander 2.5
    // works only with junction points); 'path' is the path whose information is queried;
    // 'rootOrCurReparsePoint' (if not NULL, it must point to a buffer at least MAX_PATH
    // characters long) receives the root directory or the current (last) local reparse
    // point on path 'path' (Salamander 2.5 returns the path for which the information could be retrieved
    // or at least the root directory); the remaining parameters correspond to the standard Win32 API
    // GetVolumeInformation
    // can be called from any thread
    virtual BOOL WINAPI SalGetVolumeInformation(const char* path, char* rootOrCurReparsePoint, LPTSTR lpVolumeNameBuffer,
                                                DWORD nVolumeNameSize, LPDWORD lpVolumeSerialNumber,
                                                LPDWORD lpMaximumComponentLength, LPDWORD lpFileSystemFlags,
                                                LPTSTR lpFileSystemNameBuffer, DWORD nFileSystemNameSize) = 0;

    // Custom clone of Windows GetDriveType: it can retrieve correct data even for paths
    // containing SUBST drives and reparse points on Windows 2000/XP/Vista (Salamander 2.5
    // works only with junction points); 'path' is the path whose type is queried
    // can be called from any thread
    virtual UINT WINAPI SalGetDriveType(const char* path) = 0;

    // Since the Windows GetTempFileName does not work correctly, this is a custom clone:
    // creates a file/directory (depending on 'file') in path 'path' (NULL -> Windows TEMP dir),
    // with prefix 'prefix'; returns the name of the created file/directory in 'tmpName'
    // ('tmpName' must be at least MAX_PATH characters long); returns success (on failure,
    // 'err' receives the Windows error code if it is not NULL)
    // Can be called from any thread
    virtual BOOL WINAPI SalGetTempFileName(const char* path, const char* prefix, char* tmpName, BOOL file, DWORD* err) = 0;

    // Removes a directory including its contents (SHFileOperation is terribly slow)
    // can be called from any thread
    virtual void WINAPI RemoveTemporaryDir(const char* dir) = 0;

    // Because the Windows version of MoveFile cannot rename a file with the read-only attribute on Novell,
    // a custom version is provided (if MoveFile fails, it tries to clear the read-only attribute, perform the operation,
    // and then restore it); returns success (on failure, 'err' receives the Windows error code if not NULL)
    // can be called from any thread
    virtual BOOL WINAPI SalMoveFile(const char* srcName, const char* destName, DWORD* err) = 0;

    // Alternative to the Windows GetFileSize with simpler error handling; 'file' is an open
    // file handle for GetFileSize(); 'size' receives the file size; returns success,
    // on FALSE (error) 'err' receives the Windows error code and 'size' is zero;
    // NOTE: SalGetFileSize2() exists and works with the full file name
    // can be called from any thread
    virtual BOOL WINAPI SalGetFileSize(HANDLE file, CQuadWord& size, DWORD& err) = 0;

    // Opens file/directory 'name' on path 'path'; follows Windows associations and opens it
    // through the Open item in the context menu (it may also use salopen.exe, depending on configuration);
    // before launching, it sets the current directories on local drives according to the panel;
    // 'parent' is the parent of any windows that may be opened (e.g. when opening an unassociated file)
    // main thread only (otherwise salopen.exe would not work - it uses one shared memory block)
    virtual void WINAPI ExecuteAssociation(HWND parent, const char* path, const char* name) = 0;

    // Opens a browse dialog in which the user selects a path; 'parent' is the browse dialog parent;
    // 'hCenterWindow' is the window the dialog is centered on; 'title' is the browse dialog caption;
    // 'comment' is the browse dialog comment; 'path' is the output buffer for the selected path (at least MAX_PATH
    // characters); if 'onlyNet' is TRUE, only network paths can be browsed (otherwise there is no restriction); if
    // 'initDir' is not NULL, it contains the path where the browse dialog should open; returns TRUE if
    // 'path' contains a newly selected path
    // WARNING: if called outside the main thread, COM must be initialized first (possibly better the whole
    //          OLE - see CoInitialize or OLEInitialize)
    // can be called from any thread
    virtual BOOL WINAPI GetTargetDirectory(HWND parent, HWND hCenterWindow, const char* title,
                                           const char* comment, char* path, BOOL onlyNet,
                                           const char* initDir) = 0;

    // Working with file masks: ('?' any character, '*' any string - including an empty one)
    // all of this can be called from any thread
    // 1) convert the mask to a simpler format (src -> buffer mask; minimum size of
    //    buffer 'mask' is strlen(src) + 1)
    virtual void WINAPI PrepareMask(char* mask, const char* src) = 0;
    // 2) we can use the converted mask to test whether it matches file 'filename',
    //    hasExtension = TRUE if the file has an extension
    //    returns TRUE if the file matches the mask
    virtual BOOL WINAPI AgreeMask(const char* filename, const char* mask, BOOL hasExtension) = 0;
    // 3) we can use the unmodified mask (do not call PrepareMask for it) to create a name from
    //    the specified name and mask ("a.txt" + "*.cpp" -> "a.cpp", etc.),
    //    buffer should be at least strlen(name)+strlen(mask) bytes long (2*MAX_PATH works well)
    //    returns the created name (pointer 'buffer')
    virtual char* WINAPI MaskName(char* buffer, int bufSize, const char* name, const char* mask) = 0;

    // Working with extended file masks: ('?' any character, '*' any string - including an empty one,
    // '#' any digit - '0'..'9')
    // all of this can be called from any thread
    // 1) convert the mask to a simpler format (src -> buffer mask; minimum length strlen(src) + 1)
    virtual void WINAPI PrepareExtMask(char* mask, const char* src) = 0;
    // 2) we can use the converted mask to test whether it matches file 'filename',
    //    hasExtension = TRUE if the file has an extension
    //    returns TRUE if the file matches the mask
    virtual BOOL WINAPI AgreeExtMask(const char* filename, const char* mask, BOOL hasExtension) = 0;

    // Allocates a new object for working with a group of file masks
    // can be called from any thread
    virtual CSalamanderMaskGroup* WINAPI AllocSalamanderMaskGroup() = 0;

    // Frees an object for working with a group of file masks (obtained by AllocSalamanderMaskGroup)
    // can be called from any thread
    virtual void WINAPI FreeSalamanderMaskGroup(CSalamanderMaskGroup* maskGroup) = 0;

    // Allocates memory on Salamander's heap (unnecessary when using salrtl9.dll - plain malloc is enough);
    // if memory is low, the user is shown a dialog with Retry (another allocation attempt),
    // Abort (after another prompt, terminates the application), and Ignore (lets the allocation error reach the application - after
    // warning the user that the application may crash, Alloc returns NULL;
    // checking for NULL probably makes sense only for large memory blocks, e.g. more than 500 MB, where
    // allocation may fail because the address space is fragmented by loaded DLLs);
    // NOTE: Realloc() was added later; it is listed below in this module
    // can be called from any thread
    virtual void* WINAPI Alloc(int size) = 0;
    // Deallocates memory from Salamander's heap (unnecessary when using salrtl9.dll - plain free is enough)
    // can be called from any thread
    virtual void WINAPI Free(void* ptr) = 0;

    // Duplicates a string - allocates memory (on Salamander's heap - available through salrtl9.dll)
    // + copies the string; returns NULL if 'str' == NULL;
    // can be called from any thread
    virtual char* WINAPI DupStr(const char* str) = 0;

    // Returns the lowercase and uppercase mapping tables (an array of 256 characters - the lowercase/uppercase character is at
    // the index of the queried character); if 'lowerCase' is not NULL, it receives the lowercase table;
    // if 'upperCase' is not NULL, it receives the uppercase table
    // can be called from any thread
    virtual void WINAPI GetLowerAndUpperCase(unsigned char** lowerCase, unsigned char** upperCase) = 0;

    // converts string 'str' to lowercase/uppercase; unlike ANSI C tolower/toupper it works
    // directly on the string and supports more than just characters 'A' to 'Z' (conversion to lowercase is performed via
    // an array initialized by the Win32 API function CharLower)
    virtual void WINAPI ToLowerCase(char* str) = 0;
    virtual void WINAPI ToUpperCase(char* str) = 0;

    //*****************************************************************************
    //
    // StrCmpEx
    //
    // Function compares two substrings.
    // If the two substrings are of different lengths, they are compared up to the
    // length of the shortest one. If they are equal to that point, then the return
    // value will indicate that the longer string is greater.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   l1    : compared length of s1 (must be less or equal to strlen(s1))
    //   l2    : compared length of s2 (must be less or equal to strlen(s1))
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrCmpEx(const char* s1, int l1, const char* s2, int l2) = 0;

    //*****************************************************************************
    //
    // StrICpy
    //
    // Function copies characters from source to destination. Upper case letters are mapped to
    // lower case using LowerCase array (filled using CharLower Win32 API call).
    //
    // Parameters
    //   dest: pointer to the destination string
    //   src: pointer to the null-terminated source string
    //
    // Return Values
    //   The StrICpy returns the number of bytes stored in buffer, not counting
    //   the terminating null character.
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICpy(char* dest, const char* src) = 0;

    //*****************************************************************************
    //
    // StrICmp
    //
    // Function compares two strings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    //
    // Parameters
    //   s1, s2: null-terminated strings to compare
    //
    // Return Values
    //   -1 if s1 < s2 (if string pointed to by s1 is less than the string pointed to by s2)
    //    0 if s1 = s2 (if the strings are equal)
    //   +1 if s1 > s2 (if string pointed to by s1 is greater than the string pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICmp(const char* s1, const char* s2) = 0;

    //*****************************************************************************
    //
    // StrICmpEx
    //
    // Function compares two substrings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    // If the two substrings are of different lengths, they are compared up to the
    // length of the shortest one. If they are equal to that point, then the return
    // value will indicate that the longer string is greater.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   l1    : compared length of s1 (must be less or equal to strlen(s1))
    //   l2    : compared length of s2 (must be less or equal to strlen(s2))
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrICmpEx(const char* s1, int l1, const char* s2, int l2) = 0;

    //*****************************************************************************
    //
    // StrNICmp
    //
    // Function compares two strings. The comparison is not case sensitive and ignores
    // regional settings. For the purposes of the comparison, all characters are converted
    // to lower case using LowerCase array (filled using CharLower Win32 API call).
    // The comparison stops after: (1) a difference between the strings is found,
    // (2) the end of the string is reached, or (3) n characters have been compared.
    //
    // Parameters
    //   s1, s2: strings to compare
    //   n:      maximum length to compare
    //
    // Return Values
    //   -1 if s1 < s2 (if substring pointed to by s1 is less than the substring pointed to by s2)
    //    0 if s1 = s2 (if the substrings are equal)
    //   +1 if s1 > s2 (if substring pointed to by s1 is greater than the substring pointed to by s2)
    //
    // Method can be called from any thread.
    virtual int WINAPI StrNICmp(const char* s1, const char* s2, int n) = 0;

    //*****************************************************************************
    //
    // MemICmp
    //
    // Compares n bytes of the two blocks of memory stored at buf1 and buf2.
    // Characters are converted to lowercase before comparing (not permanently;
    // using LowerCase array which was filled using CharLower Win32 API call),
    // so case is ignored in comparation.
    //
    // Parameters
    //   buf1, buf2: memory buffers to compare
    //   n:          maximum length to compare
    //
    // Return Values
    //   -1 if buf1 < buf2 (if buffer pointed to by buf1 is less than the buffer pointed to by buf2)
    //    0 if buf1 = buf2 (if the buffers are equal)
    //   +1 if buf1 > buf2 (if buffer pointed to by buf1 is greater than the buffer pointed to by buf2)
    //
    // Method can be called from any thread.
    virtual int WINAPI MemICmp(const void* buf1, const void* buf2, int n) = 0;

    // Case-insensitive comparison of strings 's1' and 's2';
    // if SALCFG_SORTUSESLOCALE is TRUE, Windows regional collation is used,
    // otherwise they are compared the same way as CSalamanderGeneral::StrICmp; if SALCFG_SORTDETECTNUMBERS is
    // TRUE, numeric sorting is used for numbers contained in the strings
    // returns <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrICmp(const char* s1, const char* s2) = 0;

    // Compares strings 's1' and 's2' (with lengths 'l1' and 'l2') case-insensitively.
    // If SALCFG_SORTUSESLOCALE is TRUE, Windows regional collation is used,
    // otherwise it compares them the same way as CSalamanderGeneral::StrICmp;
    // if SALCFG_SORTDETECTNUMBERS is TRUE, numeric sorting is used for numbers contained
    // in the strings; if 'numericalyEqual' is not NULL, it returns TRUE if the strings are
    // numerically equal (for example, "a01" and "a1"); it is automatically TRUE if the strings are equal
    // returns <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrICmpEx(const char* s1, int l1, const char* s2, int l2,
                                       BOOL* numericalyEqual) = 0;

    // Case-sensitive comparison of strings 's1' and 's2'; if SALCFG_SORTUSESLOCALE is TRUE,
    // Windows regional collation is used; otherwise they are compared the same way as the
    // standard C library function strcmp; if SALCFG_SORTDETECTNUMBERS is TRUE, numeric sorting is used
    // for numbers contained in the strings
    // returns <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrCmp(const char* s1, const char* s2) = 0;

    // Case-sensitive comparison of strings 's1' and 's2' (with lengths 'l1' and 'l2'); if
    // SALCFG_SORTUSESLOCALE is TRUE, Windows regional collation is used,
    // otherwise it compares them the same way as the standard C library function strcmp; if
    // SALCFG_SORTDETECTNUMBERS is TRUE, numeric sorting is used for numbers contained in the strings;
    // in 'numericalyEqual' (if not NULL), it returns TRUE if the strings are numerically equal
    // (e.g. "a01" and "a1"); it is automatically TRUE if the strings are equal
    // returns <0 ('s1' < 's2'), ==0 ('s1' == 's2'), >0 ('s1' > 's2')
    virtual int WINAPI RegSetStrCmpEx(const char* s1, int l1, const char* s2, int l2,
                                      BOOL* numericalyEqual) = 0;

    // Returns the path in the panel; 'panel' is one of PANEL_XXX; 'buffer' is the path buffer (it may
    // also be NULL); 'bufferSize' is the size of 'buffer' (if 'buffer' is NULL, this must
    // be zero); if 'type' is not NULL, it points to a variable that receives the path type
    // (see PATH_TYPE_XXX); if this is an archive and 'archiveOrFS' is not NULL and 'buffer' is not NULL,
    // 'archiveOrFS' receives a pointer into 'buffer' to the position after the archive file name;
    // if this is a file system and 'archiveOrFS' is not NULL and 'buffer' is not NULL,
    // 'archiveOrFS' receives a pointer into 'buffer' to the ':' after the file-system name (after ':' is the user part
    // of the file-system path); if 'convertFSPathToExternal' is TRUE and the panel contains an FS path,
    // the plugin to which the path belongs is found (by fs-name) and its
    // CPluginInterfaceForFSAbstract::ConvertPathToExternal() is called; returns success (if
    // 'bufferSize' != 0, it is also considered a failure if the path does not fit into
    // 'buffer')
    // main thread only
    virtual BOOL WINAPI GetPanelPath(int panel, char* buffer, int bufferSize, int* type,
                                     char** archiveOrFS, BOOL convertFSPathToExternal = FALSE) = 0;

    // Returns the last visited Windows path in the panel, useful when returning from an FS (nicer than
    // going straight to a fixed drive); 'panel' is one of PANEL_XXX; 'buffer' is the path buffer;
    // 'bufferSize' is the size of buffer 'buffer'; returns success
    // main thread only
    virtual BOOL WINAPI GetLastWindowsPanelPath(int panel, char* buffer, int bufferSize) = 0;

    // Returns the FS name assigned to the plugin by Salamander "for life" (according to the SetBasicPluginData design);
    // 'buf' is a buffer at least MAX_PATH characters long; 'fsNameIndex' is the fs-name index (the index is
    // zero for the fs-name specified in CSalamanderPluginEntryAbstract::SetBasicPluginData; the index of other
    // fs-names is returned by CSalamanderPluginEntryAbstract::AddFSName)
    // main thread only (otherwise the plugin configuration may change during the call),
    // in the entry point it can be called only after SetBasicPluginData; earlier it may be unknown
    virtual void WINAPI GetPluginFSName(char* buf, int fsNameIndex) = 0;

    // Returns the interface of the plugin file system (FS) opened in panel 'panel' (one of PANEL_XXX);
    // if no FS is open in the panel or it is an FS of another plugin (it does not belong to the calling plugin), the
    // method returns NULL (it is not possible to work with another plugin's object because its structure is unknown)
    // main thread only
    virtual CPluginFSInterfaceAbstract* WINAPI GetPanelPluginFS(int panel) = 0;

    // Returns the panel listing's plugin data interface (it may also be NULL); 'panel' is one of PANEL_XXX;
    // if a plugin data interface exists but does not belong to the calling plugin, the
    // method returns NULL (it is not possible to work with another plugin's object because its structure is unknown)
    // main thread only
    virtual CPluginDataInterfaceAbstract* WINAPI GetPanelPluginData(int panel) = 0;

    // Returns the focused panel item (file/directory/updir("..")); 'panel' is one of PANEL_XXX,
    // returns NULL (no item in the panel) or the data of the focused item; if 'isDir' is not NULL,
    // it receives FALSE for a file (otherwise it is a directory or updir)
    // WARNING: the returned item data are read-only
    // main thread only
    virtual const CFileData* WINAPI GetPanelFocusedItem(int panel, BOOL* isDir) = 0;

    // Returns panel items one by one (directories first, then files); 'panel' is one of PANEL_XXX,
    // 'index' is an in/out variable pointing to an int that must be 0 on the first call;
    // the function stores the value for the next call before returning (usage: initialize it to 0, then
    // do not modify it), returns NULL (no more items) or the data of the next (or first) item;
    // if 'isDir' is not NULL, it receives FALSE for a file (otherwise it is a directory or updir)
    // WARNING: the returned item data are read-only
    // main thread only
    virtual const CFileData* WINAPI GetPanelItem(int panel, int* index, BOOL* isDir) = 0;

    // Returns selected panel items one by one (directories first, then files); 'panel' is one of
    // PANEL_XXX, 'index' is an in/out variable pointing to an int that must be 0 on the first call,
    // the function stores the value for the next call before returning (usage: initialize it to 0, then
    // do not modify it), returns NULL (no more items) or the data of the next (or first) item;
    // if 'isDir' is not NULL, it receives FALSE for a file (otherwise it is a directory or updir)
    // WARNING: the returned item data are read-only
    // main thread only
    virtual const CFileData* WINAPI GetPanelSelectedItem(int panel, int* index, BOOL* isDir) = 0;

    // Determines how many files and directories are selected in the panel; 'panel' is one of PANEL_XXX;
    // if 'selectedFiles' is not NULL, it receives the number of selected files; if 'selectedDirs'
    // is not NULL, it receives the number of selected directories; returns TRUE if at least one
    // file or directory is selected, or if the focus is on a file or directory (i.e. there is something
    // to work with - the focus is not on updir)
    // main thread only (otherwise the panel contents may change)
    virtual BOOL WINAPI GetPanelSelection(int panel, int* selectedFiles, int* selectedDirs) = 0;

    // Returns the top index of the list box in the panel; 'panel' is one of PANEL_XXX
    // main thread only (otherwise the panel contents may change)
    virtual int WINAPI GetPanelTopIndex(int panel) = 0;

    // Notifies Salamander's main window that a viewer window is being deactivated; if the
    // main window is activated immediately afterward and the panels contain manually refreshed
    // drives, they are not refreshed (viewers do not change disk contents); this call is optional
    // (at worst it only causes an unnecessary refresh)
    // can be called from any thread
    virtual void WINAPI SkipOneActivateRefresh() = 0;

    // Selects/deselects a panel item; 'file' is a pointer to the item to change obtained by a previous
    // "get-item" call (GetPanelFocusedItem, GetPanelItem, or GetPanelSelectedItem);
    // it is necessary that the plugin has not been left since the "get-item" call and that this call runs in the main
    // thread (to avoid panel refresh invalidating the pointer); 'panel' must match
    // the 'panel' parameter of the corresponding "get-item" call; if 'select' is TRUE, the item is selected,
    // otherwise it is deselected; after the last call, RepaintChangedItems('panel') must be used to
    // repaint the panel
    // main thread only
    virtual void WINAPI SelectPanelItem(int panel, const CFileData* file, BOOL select) = 0;

    // Repaints panel items whose selection state has changed; 'panel' is
    // one of PANEL_XXX
    // main thread only
    virtual void WINAPI RepaintChangedItems(int panel) = 0;

    // Selects/deselects all items in the panel; 'panel' is one of PANEL_XXX; if 'select'
    // is TRUE, the items are selected, otherwise they are deselected; if 'repaint' is TRUE, all
    // changed items in the panel are repainted; otherwise no repaint occurs (RepaintChangedItems may be called later)
    // main thread only
    virtual void WINAPI SelectAllPanelItems(int panel, BOOL select, BOOL repaint) = 0;

    // Sets the focus in the panel; 'file' is a pointer to the focused item obtained by a previous
    // "get-item" call (GetPanelFocusedItem, GetPanelItem, or GetPanelSelectedItem);
    // it is necessary that the plugin has not been left since the "get-item" call and that this call runs in the main
    // thread (to avoid panel refresh invalidating the pointer); 'panel' must match
    // the 'panel' parameter of the corresponding "get-item" call; if 'partVis' is TRUE and the item would be
    // only partially visible, the panel is not scrolled when focusing it; if FALSE, the panel is scrolled
    // so the whole item is visible
    // main thread only
    virtual void WINAPI SetPanelFocusedItem(int panel, const CFileData* file, BOOL partVis) = 0;

    // Determines whether a filter is used in the panel and, if so, retrieves its mask string;
    // 'panel' identifies the panel of interest (one of PANEL_XXX);
    // 'masks' is the output buffer for the filter masks and must be at least 'masksBufSize' bytes long (the recommended
    // size is MAX_GROUPMASK); returns TRUE if a filter is used and the 'masks' buffer is large enough; returns FALSE if
    // no filter is used or the mask string does not fit into 'masks'.
    // main thread only
    virtual BOOL WINAPI GetFilterFromPanel(int panel, char* masks, int masksBufSize) = 0;

    // Returns the source panel position (left or right); returns PANEL_LEFT or PANEL_RIGHT
    // main thread only
    virtual int WINAPI GetSourcePanel() = 0;

    // Determines in which panel 'pluginFS' is open; if it is not open in either panel,
    // returns FALSE; if it returns TRUE, the panel number is stored in 'panel' (PANEL_LEFT or PANEL_RIGHT)
    // main thread only (otherwise the panel contents may change)
    virtual BOOL WINAPI GetPanelWithPluginFS(CPluginFSInterfaceAbstract* pluginFS, int& panel) = 0;

    // aktivuje druhy panel (ala klavesa TAB); panely oznacene pres PANEL_SOURCE a PANEL_TARGET
    // se tim prirozene prohazuji
    // omezeni: hlavni thread
    virtual void WINAPI ChangePanel() = 0;

    // Converts a number to a more readable string (a space after every three digits), writes the string to
    // 'buffer' (minimum size 50 bytes), returns 'buffer'
    // can be called from any thread
    virtual char* WINAPI NumberToStr(char* buffer, const CQuadWord& number) = 0;

    // Prints disk size to 'buf' (minimum buffer size 100 bytes),
    // mode==0 "1.23 MB", mode==1 "1 230 000 bytes, 1.23 MB", mode==2 "1 230 000 bytes",
    // mode==3 "12 KB" (always whole kilobytes), mode==4 (like mode==0, but always
    // at least 3 significant digits, e.g. "2.00 MB")
    // returns 'buf'
    // can be called from any thread
    virtual char* WINAPI PrintDiskSize(char* buf, const CQuadWord& size, int mode) = 0;

    // Converts a number of seconds to a string ("5 sec", "1 hr 34 min", etc.); 'buf' is the
    // output buffer and must be at least 100 characters long; 'secs' is the number of seconds;
    // returns 'buf'
    // can be called from any thread
    virtual char* WINAPI PrintTimeLeft(char* buf, const CQuadWord& secs) = 0;

    // Compares the roots of normal (c:\path) and UNC (\\server\share\path) paths; returns TRUE if the roots are the same
    // can be called from any thread
    virtual BOOL WINAPI HasTheSameRootPath(const char* path1, const char* path2) = 0;

    // Returns the number of characters in the common path prefix. On a normal path, the root must end with a backslash,
    // otherwise the function returns 0. ("C:\"+"C:"->0, "C:\A\B"+"C:\"->3, "C:\A\B\"+"C:\A"->4,
    // "C:\AA\BB"+"C:\AA\CC"->5)
    // Works for both normal and UNC paths.
    virtual int WINAPI CommonPrefixLength(const char* path1, const char* path2) = 0;

    // Returns TRUE if path 'prefix' is a prefix of path 'path'. Otherwise it returns FALSE.
    // "C:\aa","C:\Aa\BB"->TRUE
    // "C:\aa","C:\aaa"->FALSE
    // "C:\aa\","C:\Aa"->TRUE
    // "\\server\share","\\server\share\aaa"->TRUE
    // Works with both normal and UNC paths.
    virtual BOOL WINAPI PathIsPrefix(const char* prefix, const char* path) = 0;

    // Compares two normal (c:\path) and UNC (\\server\share\path) paths, ignoring case,
    // also ignores one leading and trailing backslash, returns TRUE if the paths are the same
    // can be called from any thread
    virtual BOOL WINAPI IsTheSamePath(const char* path1, const char* path2) = 0;

    // Gets the root path from a normal (c:\path) or UNC (\\server\share\path) path 'path'; in 'root' it returns
    // a path in the form 'c:\' or '\\server\share\' (minimum size of buffer 'root' is MAX_PATH),
    // returns the number of characters in the root path (without the null terminator); for a UNC root path longer than MAX_PATH,
    // it is truncated to MAX_PATH-2 characters and a backslash is appended (in that case it is definitely not a full root path)
    // can be called from any thread
    virtual int WINAPI GetRootPath(char* root, const char* path) = 0;

    // Removes the last directory from a normal (c:\path) or UNC (\\server\share\path) path
    // (cuts at the last backslash - the truncated path keeps the trailing backslash
    // only for 'c:\'); 'path' is an in/out buffer (minimum size strlen(path)+2 bytes),
    // if 'cutDir' is not NULL, it receives a pointer (in buffer 'path' after the first null terminator)
    // to the last directory (the removed part); this method replaces PathRemoveFileSpec,
    // returns TRUE if the path was shortened (i.e. it was not a root path)
    // can be called from any thread
    virtual BOOL WINAPI CutDirectory(char* path, char** cutDir = NULL) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // joins 'path' and 'name' into 'path', inserting a backslash if needed; 'path' is a buffer at least
    // 'pathSize' characters long, returns TRUE if 'name' fit after 'path'; if 'path' or 'name' is
    // empty, the separating backslash is omitted (e.g. "c:\" + "" -> "c:")
    // can be called from any thread
    virtual BOOL WINAPI SalPathAppend(char* path, const char* name, int pathSize) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // if 'path' does not already end with a backslash, appends one; 'path' is a buffer
    // at least 'pathSize' characters long; returns TRUE if the backslash fit after 'path'; if 'path'
    // is empty, no backslash is added
    // can be called from any thread
    virtual BOOL WINAPI SalPathAddBackslash(char* path, int pathSize) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // removes a trailing backslash from 'path' if present
    // can be called from any thread
    virtual void WINAPI SalPathRemoveBackslash(char* path) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // strips a full path down to the name ("c:\path\file" -> "file")
    // can be called from any thread
    virtual void WINAPI SalPathStripPath(char* path) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // removes the extension if the name has one
    // can be called from any thread
    virtual void WINAPI SalPathRemoveExtension(char* path) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // if 'path' does not already have an extension, appends 'extension' (e.g. ".txt"),
    // 'path' is a buffer at least 'pathSize' characters long, returns FALSE if buffer 'path' is too small
    // for the resulting path
    // can be called from any thread
    virtual BOOL WINAPI SalPathAddExtension(char* path, const char* extension, int pathSize) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths,
    // changes/adds extension 'extension' (e.g. ".txt") in 'path'; 'path' is a buffer
    // at least 'pathSize' characters long, returns FALSE if buffer 'path' is too small for the resulting path
    // can be called from any thread
    virtual BOOL WINAPI SalPathRenameExtension(char* path, const char* extension, int pathSize) = 0;

    // Works with normal (c:\path) and UNC (\\server\share\path) paths. Returns a pointer into 'path' to the file/directory name (ignores a trailing backslash in 'path'). If the name contains no backslashes other than a trailing one, returns 'path'. Callable from any thread.
    virtual const char* WINAPI SalPathFindFileName(const char* path) = 0;

    // Converts a relative or absolute normal (c:\path) or UNC (\\server\share\path) path to an absolute path without '.', '..', and without a trailing backslash (except for paths of the form "c:\"). If 'curDir' is NULL, relative paths of the form "\\path" and "path" fail because the base cannot be determined; otherwise 'curDir' is a valid normalized current path (UNC or normal). Current paths of the other drives ('curDir' plus normal paths only, not UNC) are stored in Salamander's DefaultDir array; it is good to call SalUpdateDefaultDir before using them. 'name' is an input/output path buffer of at least 'nameBufSize' characters. If 'nextFocus' is not NULL and the requested relative path contains no backslash, strcpy(nextFocus, name) is performed. Returns TRUE when 'name' is ready to use; otherwise, if 'errTextID' is not NULL, it receives an error constant GFN_XXX (the text can be obtained with GetGFNErrorText).
    // WARNING: it is good to call SalUpdateDefaultDir before use.
    // Restriction: main thread (DefaultDir can change in the main thread).
    virtual BOOL WINAPI SalGetFullName(char* name, int* errTextID = NULL, const char* curDir = NULL,
                                       char* nextFocus = NULL, int nameBufSize = MAX_PATH) = 0;

    // Updates Salamander's DefaultDir array from the panel paths. If 'activePrefered' is TRUE, the active panel path has priority and is written to DefaultDir later; otherwise the inactive panel path has priority.
    // Restriction: main thread (DefaultDir can change in the main thread).
    virtual void WINAPI SalUpdateDefaultDir(BOOL activePrefered) = 0;

    // Returns the text representation of the GFN_XXX error constant. Returns 'buf' so GetGFNErrorText can be used directly as a function argument. Callable from any thread.
    virtual char* WINAPI GetGFNErrorText(int GFN, char* buf, int bufSize) = 0;

    // Returns the text representation of a system error (ERROR_XXX) in 'buf' of size 'bufSize'. Returns 'buf' so GetErrorText can be used directly as a function argument. 'buf' may be NULL or 'bufSize' may be 0; in that case the text is returned in an internal buffer (the text may later change because additional GetErrorText calls from other plugins or Salamander can reuse that buffer; the buffer is sized for at least 10 texts before overwrite becomes possible. If you need the text later, copy it to a local buffer of size MAX_PATH + 20). Callable from any thread.
    virtual char* WINAPI GetErrorText(int err, char* buf = NULL, int bufSize = 0) = 0;

    // Returns Salamander's current internal color; 'color' is a color constant (see SALCOL_XXX). Callable from any thread.
    virtual COLORREF WINAPI GetCurrentColor(int color) = 0;

    // Activates Salamander's main window and focuses file/directory 'name' at path 'path' in panel 'panel'; changes the panel path if necessary. 'panel' is one of PANEL_XXX. 'path' may be any path (Windows disk path, FS path, or archive path). 'name' may be an empty string if nothing should be focused.
    // Restriction: main thread + outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual void WINAPI FocusNameInPanel(int panel, const char* path, const char* name) = 0;

    // changes the panel path - input may be an absolute or relative UNC (\\server\share\path)
    // or a normal (c:\path) path, either a Windows (disk) path, an archive path, or an FS path
    // (absolute/relative handling is performed directly by the plugin); if the input is a file path,
    // that file is focused; if suggestedTopIndex is not -1, the top index is set
    // in the panel; if suggestedFocusName is not NULL, an item with the same name is searched for
    // case-insensitively and focused; if 'failReason' is not NULL, it is set to one of the
    // CHPPFR_XXX constants (it reports the method result); if 'convertFSPathToInternal' is TRUE and
    // the path is an FS path, the plugin that owns the path is found (by fs-name) and its
    // CPluginInterfaceForFSAbstract::ConvertPathToInternal() is called; returns TRUE if the requested
    // path was listed successfully;
    // NOTE: when an FS path is entered, opening is attempted in this order: in the FS
    // in the panel, in a detached FS, or in a new FS (for the FS in the panel and detached FSs, it is checked
    // whether plugin-fs-name matches and whether the FS method IsOurPath returns TRUE for the given path);
    // restriction: main thread + outside the methods of CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract
    // (for example, an FS open in the panel could be closed and 'this' could stop existing for the method)
    virtual BOOL WINAPI ChangePanelPath(int panel, const char* path, int* failReason = NULL,
                                        int suggestedTopIndex = -1,
                                        const char* suggestedFocusName = NULL,
                                        BOOL convertFSPathToInternal = TRUE) = 0;

    // Changes the panel path to a relative or absolute UNC (\\server\share\path) or normal (c:\path) path. If the new path is not available, it tries shortened paths. If this is a path change within one disk, including archives on that disk, and no accessible path can be found on that disk, the panel path is changed to the root of the first local fixed drive so the panel does not stay empty. When resolving a relative path to an absolute path, the path in panel 'panel' is preferred, but only if it is a disk path, including an archive path. 'panel' is one of PANEL_XXX. 'path' is the new path. If 'suggestedTopIndex' is not -1, it is used as the panel top index, but only for the new path, not for a shortened or changed path. If 'suggestedFocusName' is not NULL, an item with the same name is searched for case-insensitively and focused, again only for the new path, not for a shortened or changed path. If 'failReason' is not NULL, it is set to one of the CHPPFR_XXX constants describing the result. Returns TRUE if the requested path was listed without shortening or other changes. Restriction: main thread + outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual BOOL WINAPI ChangePanelPathToDisk(int panel, const char* path, int* failReason = NULL,
                                              int suggestedTopIndex = -1,
                                              const char* suggestedFocusName = NULL) = 0;

    // Changes the panel path to an archive. 'archive' is a relative or absolute UNC
    // (\\server\share\path\file) or normal (c:\path\file) archive name; 'archivePath' is the path
    // inside the archive. If the new path in the archive is not accessible, the method tries to
    // succeed with shortened paths. When resolving a relative path to an absolute path, the path in
    // panel 'panel' is preferred (but only if it is a disk path, including a path to an archive;
    // otherwise it is not used). 'panel' is one of PANEL_XXX. If 'suggestedTopIndex' is not -1, it
    // is set as the top index in the panel (only for the new path; it is not set for a shortened
    // or modified path). If 'suggestedFocusName' is not NULL, the method tries to find
    // (case-insensitively) and focus an item with the same name (only for the new path; this is not
    // done for a shortened or modified path). If 'forceUpdate' is TRUE and the path inside archive
    // 'archive' is being changed while the archive is already open in the panel, the archive file is
    // tested for changes (size and time check); if it has changed, the archive is closed (to avoid
    // updating edited files) and listed again, or if the file no longer exists, the path is changed
    // to a disk path (the archive is closed; if the disk path is not accessible, the path is changed
    // to the root of the first local fixed drive). If 'forceUpdate' is FALSE, changes to the path
    // inside archive 'archive' are performed without checking the archive file. If 'failReason' is
    // not NULL, it is set to one of the CHPPFR_XXX constants (describing the method result). Returns
    // TRUE if the requested path (without shortening or modification) was listed successfully.
    // Restriction: main thread only, and outside the methods of CPluginFSInterfaceAbstract and
    // CPluginDataInterfaceAbstract (for example, an FS open in the panel could be closed, so 'this'
    // could cease to exist).
    virtual BOOL WINAPI ChangePanelPathToArchive(int panel, const char* archive, const char* archivePath,
                                                 int* failReason = NULL, int suggestedTopIndex = -1,
                                                 const char* suggestedFocusName = NULL,
                                                 BOOL forceUpdate = FALSE) = 0;

    // Changes the panel path to a plugin FS. 'fsName' is the FS name (see GetPluginFSName; it does not have to belong to this plugin), and 'fsUserPart' is the path within the FS. If the new FS path is not available, it tries shortened paths by repeated calls to ChangePath and ListCurrentPath (see CPluginFSInterfaceAbstract). If the path is being changed within the current FS (see CPluginFSInterfaceAbstract::IsOurPath) and no accessible path can be found starting from the new path, it also tries to find an accessible path starting from the old current path, and if that also fails, it changes the panel path to the root of the first local fixed drive so the panel does not stay empty. 'panel' is one of PANEL_XXX. If 'suggestedTopIndex' is not -1, it is used as the panel top index, but only for the new path, not for a shortened or changed path. If 'suggestedFocusName' is not NULL, an item with the same name is searched for case-insensitively and focused, again only for the new path, not for a shortened or changed path. If 'forceUpdate' is TRUE, the special case of changing to the current panel path is not optimized away; the path is listed normally even if the new path already matches the current path or was shortened to it by the first ChangePath call. If 'convertPathToInternal' is TRUE, the plugin identified by 'fsName' is found and its CPluginInterfaceForFSAbstract::ConvertPathToInternal() method is called for 'fsUserPart'. If 'failReason' is not NULL, it is set to one of the CHPPFR_XXX constants describing the result. Returns TRUE if the requested path was listed without shortening or other changes.
    // NOTE: If you need an FS path to be tried in detached FS instances as well, use ChangePanelPath; ChangePanelPathToPluginFS ignores detached FS instances and works only with an FS already open in a panel or opens a new FS.
    // Restriction: main thread + outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual BOOL WINAPI ChangePanelPathToPluginFS(int panel, const char* fsName, const char* fsUserPart,
                                                  int* failReason = NULL, int suggestedTopIndex = -1,
                                                  const char* suggestedFocusName = NULL,
                                                  BOOL forceUpdate = FALSE,
                                                  BOOL convertPathToInternal = FALSE) = 0;

    // Changes the panel path to a detached plugin FS (see FSE_DETACHED/FSE_ATTACHED). 'detachedFS' is a detached plugin FS. If the current path in the detached FS is not available, it tries shortened paths by repeated calls to ChangePath and ListCurrentPath (see CPluginFSInterfaceAbstract). 'panel' is one of PANEL_XXX. If 'suggestedTopIndex' is not -1, it is used as the panel top index, but only for the new path, not for a shortened or changed path. If 'suggestedFocusName' is not NULL, an item with the same name is searched for case-insensitively and focused, again only for the new path, not for a shortened or changed path. If 'failReason' is not NULL, it is set to one of the CHPPFR_XXX constants describing the result. Returns TRUE if the requested path was listed without shortening or other changes. Restriction: main thread + outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual BOOL WINAPI ChangePanelPathToDetachedFS(int panel, CPluginFSInterfaceAbstract* detachedFS,
                                                    int* failReason = NULL, int suggestedTopIndex = -1,
                                                    const char* suggestedFocusName = NULL) = 0;

    // Changes the panel path to the root of the first local fixed drive; this almost certainly changes the current path in the panel. 'panel' is one of PANEL_XXX. If 'failReason' is not NULL, it is set to one of the CHPPFR_XXX constants describing the result. Returns TRUE if the root of the first local fixed drive was successfully listed.
    // Restriction: main thread + outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual BOOL WINAPI ChangePanelPathToFixedDrive(int panel, int* failReason = NULL) = 0;

    // Refreshes the panel path (reloads the listing and transfers selections, icons, focus, etc. to the new panel contents). Disk and FS paths are always reloaded. Archive paths are reloaded only if the archive file changed (size & time check) or if 'forceRefresh' is TRUE. Thumbnails on disk paths are reloaded only if the file size or last-write date/time changed, or if 'forceRefresh' is TRUE. 'panel' is one of PANEL_XXX. If 'focusFirstNewItem' is TRUE and exactly one new item appeared in the panel, that new item is focused (for example, to focus a newly created file or directory).
    // Restriction: main thread, and only outside CPluginFSInterfaceAbstract and CPluginDataInterfaceAbstract methods (for example, an FS open in the panel could be closed and 'this' could cease to exist).
    virtual void WINAPI RefreshPanelPath(int panel, BOOL forceRefresh = FALSE,
                                         BOOL focusFirstNewItem = FALSE) = 0;

    // Posts a message to the panel requesting a path refresh (reloads the listing and transfers selections, icons, focus, etc. to the new panel contents). The refresh is performed only after Salamander's main window becomes active again, that is, after suspend mode ends. Disk and FS paths are always reloaded. Archive paths are reloaded only if the archive file changed (size & time check). 'panel' is one of PANEL_XXX. If 'focusFirstNewItem' is TRUE and exactly one new item appeared in the panel, that new item is focused (for example, to focus a newly created file or directory). Callable from any thread; if the main thread is not running code inside a plugin, the refresh happens as soon as possible, otherwise it waits at least until the main thread leaves the plugin.
    virtual void WINAPI PostRefreshPanelPath(int panel, BOOL focusFirstNewItem = FALSE) = 0;

    // Posts a message to the panel with active FS 'modifiedFS' requesting a path refresh (reloads the listing and transfers selections, icons, focus, etc. to the new panel contents). The refresh is performed only after Salamander's main window becomes active again, that is, after suspend mode ends. The FS path is always reloaded. If 'modifiedFS' is not open in any panel, nothing happens. If 'focusFirstNewItem' is TRUE and exactly one new item appeared in the panel, that new item is focused (for example, to focus a newly created file or directory).
    // NOTE: PostRefreshPanelFS2 also exists; it returns TRUE if a refresh was performed, FALSE if 'modifiedFS' was not found in either panel.
    // Callable from any thread; if the main thread is not running code inside a plugin, the refresh happens as soon as possible, otherwise it waits at least until the main thread leaves the plugin.
    virtual void WINAPI PostRefreshPanelFS(CPluginFSInterfaceAbstract* modifiedFS,
                                           BOOL focusFirstNewItem = FALSE) = 0;

    // Closes a detached plugin FS if possible (see CPluginFSInterfaceAbstract::TryCloseOrDetach). 'detachedFS' is a detached plugin FS. Returns TRUE on success; FALSE means the detached plugin FS was not closed. 'parent' is the parent of any message boxes; currently only CPluginFSInterfaceAbstract::ReleaseObject can open them.
    // Note: A plugin FS open in a panel can be closed, for example, with ChangePanelPathToRescuePathOrFixedDrive.
    // Restriction: main thread + outside CPluginFSInterfaceAbstract methods (we are trying to close the detached FS, so 'this' could cease to exist).
    virtual BOOL WINAPI CloseDetachedFS(HWND parent, CPluginFSInterfaceAbstract* detachedFS) = 0;

    // Duplicates '&'; useful for paths displayed in menus ('&&' is displayed as '&'). 'buffer' is an input/output string, and 'bufferSize' is the size of 'buffer' in bytes. Returns TRUE if duplicating '&' did not truncate characters from the end of the string, that is, the buffer was large enough. Callable from any thread.
    virtual BOOL WINAPI DuplicateAmpersands(char* buffer, int bufferSize) = 0;

    // removes '&' from the text; if it finds the pair "&&", it replaces it with a single '&'
    // can be called from any thread
    virtual void WINAPI RemoveAmpersands(char* text) = 0;

    // ValidateVarString and ExpandVarString:
    // Methods for validating and expanding strings with variables of the form "$(var_name)", "$(var_name:num)" (where num is the variable width, a numeric value from 1 to 9999), "$(var_name:max)" (where "max" means the variable width is taken from the values in the 'maxVarWidths' array; see ExpandVarString for details), and "$[env_var]" (expands the value of an environment variable). This is used when the user can enter a string format, for example in the info line. Example string with variables:
    // "$(files) files and $(dirs) directories" - variables 'files' and 'dirs';
    // source code for use in the info line, without variables of the form "$(varname:max)", is in DEMOPLUG
    //
    // Validates the syntax of 'varText' (a string with variables). Returns FALSE if it finds an error; the error position is stored in 'errorPos1' (offset of the start of the invalid part) and 'errorPos2' (offset of the end of the invalid part). 'variables' is an array of CSalamanderVarStrEntry structures terminated by an entry with Name==NULL. 'msgParent' is the parent of error message boxes; if NULL, no errors are displayed.
    virtual BOOL WINAPI ValidateVarString(HWND msgParent, const char* varText, int& errorPos1, int& errorPos2,
                                          const CSalamanderVarStrEntry* variables) = 0;
    // Fills 'buffer' with the result of expanding 'varText' (a string with variables). Returns FALSE if 'buffer' is too small (it assumes the variable string was validated by ValidateVarString; otherwise it also returns FALSE on syntax errors) or if the user clicked Cancel on an environment-variable error (not found or too long). 'bufferLen' is the size of 'buffer'.
    // 'variables' is an array of CSalamanderVarStrEntry structures terminated by an entry with Name==NULL. 'param' is a pointer passed to CSalamanderVarStrEntry::Execute when an encountered variable is expanded. 'msgParent' is the parent of error message boxes; if NULL, no errors are displayed. If 'ignoreEnvVarNotFoundOrTooLong' is TRUE, environment-variable errors (not found or too long) are ignored; if FALSE, an error message box is shown. If 'varPlacements' is not NULL, it points to an array of DWORD values with '*varPlacementsCount' items; the array is filled with DWORD values composed of the variable position in the output buffer (low WORD) and the variable length in characters (high WORD). If 'varPlacementsCount' is not NULL, it receives the number of filled items in 'varPlacements' (that is, the number of variables in the input string).
    // If this method is used only to expand the string for a single 'param' value, 'detectMaxVarWidths' should be FALSE, 'maxVarWidths' should be NULL, and 'maxVarWidthsCount' should be 0. If the method is used repeatedly to expand the string for a set of 'param' values (for example, in Make File List, where the line is expanded sequentially for all selected files and directories), it also makes sense to use variables of the form "$(varname:max)". For those variables, the width is the largest expanded variable width across the whole set of values. Measuring that maximum width is done in the first ExpandVarString pass over the whole set. In that first pass, 'detectMaxVarWidths' is TRUE and the 'maxVarWidths' array of 'maxVarWidthsCount' items is zero-initialized in advance (it stores maxima across individual ExpandVarString calls). The actual expansion then runs in the second ExpandVarString pass over the whole set. In that second pass, 'detectMaxVarWidths' is FALSE and the 'maxVarWidths' array of 'maxVarWidthsCount' items contains the precomputed maximum widths from the first pass.
    virtual BOOL WINAPI ExpandVarString(HWND msgParent, const char* varText, char* buffer, int bufferLen,
                                        const CSalamanderVarStrEntry* variables, void* param,
                                        BOOL ignoreEnvVarNotFoundOrTooLong = FALSE,
                                        DWORD* varPlacements = NULL, int* varPlacementsCount = NULL,
                                        BOOL detectMaxVarWidths = FALSE, int* maxVarWidths = NULL,
                                        int maxVarWidthsCount = 0) = 0;

    // Sets the plugin's load-on-Salamander-start flag. 'start' is the new value of the flag. Returns the old value of the flag. If SetFlagLoadOnSalamanderStart has never been called, the flag is FALSE, so the plugin is not loaded at startup but only when needed.
    // Restriction: main thread (otherwise plugin configuration may change during the call).
    virtual BOOL WINAPI SetFlagLoadOnSalamanderStart(BOOL start) = 0;

    // marks the calling plugin to be unloaded at the next possible opportunity
    // (once all posted menu commands have been processed (see PostMenuExtCommand), there are no
    // messages in the main-thread message queue, and Salamander is not "busy");
    // POZOR: pokud se vola z jineho nez hlavniho threadu, muze dojit k zadosti o unload (probiha
    // in the main thread) even before PostUnloadThisPlugin returns (for more information about
    // unloadu - viz CPluginInterfaceAbstract::Release)
    // callable from any thread, but only after the plugin entry point has finished; while the
    // entry point is running, the method may be called only from the main thread
    virtual void WINAPI PostUnloadThisPlugin() = 0;

    // Enumerates Salamander modules one by one (the executable file and the .spl files of installed plugins, all including version information). 'index' is an input/output variable pointing to an int; it must be 0 on the first call, and the function stores the value for the next call on return (usage: initialize it to 0, then do not modify it). 'module' is a buffer for the module name (minimum size MAX_PATH characters). 'version' is a buffer for the module version (minimum size MAX_PATH characters). Returns FALSE if 'module' and 'version' were not filled because there is no next module; returns TRUE if 'module' and 'version' contain the next module. Restriction: main thread (configuration may change during the call because of add/remove operations).
    virtual BOOL WINAPI EnumInstalledModules(int* index, char* module, char* version) = 0;

    // Calls 'loadOrSaveFunc' to load or save configuration. If 'load' is TRUE, this is a configuration load. If the plugin supports "load/save configuration" and the plugin's private registry key exists at call time, 'loadOrSaveFunc' is called for that key; otherwise the default configuration is loaded ('regKey' passed to 'loadOrSaveFunc' is NULL). If 'load' is FALSE, this is a configuration save, and 'loadOrSaveFunc' is called only if the plugin supports "load/save configuration" and Salamander's key exists at call time. 'param' is a user parameter passed to 'loadOrSaveFunc'.
    // Restriction: main thread; in the entry point, this can be called only after SetBasicPluginData, because before that it may not yet be known whether "load/save configuration" is supported and what the private registry key name is.
    virtual void WINAPI CallLoadOrSaveConfiguration(BOOL load, FSalLoadOrSaveConfiguration loadOrSaveFunc,
                                                    void* param) = 0;

    // Copies 'text' of length 'textLen' to the clipboard (-1 means "use strlen") in both multibyte
    // and Unicode form (otherwise, for example, Notepad cannot handle Czech text). On success it may,
    // if 'showEcho' is TRUE, display the message "Text was successfully copied to clipboard."
    // ('echoParent' is the parent of the message box). Returns TRUE on success.
    // Can be called from any thread.
    virtual BOOL WINAPI CopyTextToClipboard(const char* text, int textLen, BOOL showEcho, HWND echoParent) = 0;

    // Copies Unicode 'text' of length 'textLen' to the clipboard (-1 means "use wcslen") in both
    // Unicode and multibyte form (otherwise, for example, MSVC6.0 cannot handle Czech text correctly);
    // on success, if 'showEcho' is TRUE, it may display the message "Text was successfully copied to clipboard."
    // ('echoParent' is the parent window of the message box); returns TRUE on success
    // Can be called from any thread
    virtual BOOL WINAPI CopyTextToClipboardW(const wchar_t* text, int textLen, BOOL showEcho, HWND echoParent) = 0;

    // executes the menu command identified by 'id' in the main thread (calls
    // CPluginInterfaceForMenuExtAbstract::ExecuteMenuItem(salamander, main-window-hwnd, 'id', 0),
    // 'salamander' is NULL if 'waitForSalIdle' is FALSE; otherwise it contains a valid
    // set of Salamander methods that can be used to perform operations; the return value
    // is ignored). If 'waitForSalIdle' is FALSE, execution uses a message posted to the
    // main window (that message is delivered by every running message loop in the main thread,
    // including modal dialogs/message boxes, even those opened by the plugin), so re-entry
    // into the plugin is possible; if 'waitForSalIdle' is TRUE, 'id' is limited to <0, 999999> and
    // the command runs as soon as there are no messages in the main-thread message queue and Salamander
    // is not "busy" (no modal dialog is open and no message is being processed);
    // WARNING: if called from a thread other than the main thread, the menu command may run
    // (in the main thread) even before PostMenuExtCommand returns
    // callable from any thread, and if 'waitForSalIdle' is FALSE, it is necessary to wait until after calling
    // CPluginInterfaceAbstract::GetInterfaceForMenuExt (vola se po entry-pointu z hlavniho threadu)
    virtual void WINAPI PostMenuExtCommand(int id, BOOL waitForSalIdle) = 0;

    // Determines whether there is a high chance, though this cannot be known with certainty, that Salamander will not be "busy" during the next few moments (no modal dialog open and no message being processed); in that case it returns TRUE, otherwise FALSE. If 'lastIdleTime' is not NULL, it receives the GetTickCount() value from the last transition from the "idle" state to the "busy" state. This can be used, for example, as a prediction for the delivery of a command posted with CSalamanderGeneralAbstract::PostMenuExtCommand and 'waitForSalIdle'==TRUE. Callable from any thread.
    virtual BOOL WINAPI SalamanderIsNotBusy(DWORD* lastIdleTime) = 0;

    // Sets the message that the Bug Report dialog should display if a crash occurs inside the plugin (inside the plugin means at least one call-stack message stored from the plugin), and allows the standard bug-report email address (support@altap.cz) to be overridden. 'message' is the new message (NULL means "no message"). 'email' is the new email address (NULL means "use the default"; maximum email length is 100 characters). This method may be called repeatedly; the previous message and email are overwritten. Salamander does not remember either the message or the email for the next run, so this method must be called again every time the plugin is loaded, ideally in the entry point.
    // Restriction: main thread (otherwise plugin configuration may change during the call).
    virtual void WINAPI SetPluginBugReportInfo(const char* message, const char* email) = 0;

    // Determines whether a plugin is installed. It does not determine whether the plugin can actually be loaded, for example if the user deleted it from disk. 'pluginSPL' identifies the plugin; it is the required trailing part of the full path to the plugin .SPL file (for example, "ieviewer\\ieviewer.spl" identifies the IEViewer shipped with Salamander). Returns TRUE if the plugin is installed.
    // Restriction: main thread (otherwise plugin configuration may change during the call).
    virtual BOOL WINAPI IsPluginInstalled(const char* pluginSPL) = 0;

    // Opens a file in a viewer implemented by a plugin or in the internal text/hex viewer. If 'pluginSPL' is NULL, the internal text/hex viewer is used; otherwise it identifies the viewer plugin by the required trailing part of the full path to the plugin .SPL file (for example, "ieviewer\\ieviewer.spl" identifies the IEViewer shipped with Salamander). 'pluginData' is a data structure containing the name of the viewed file and optionally additional viewer parameters (see CSalamanderPluginViewerData). If 'useCache' is FALSE, 'rootTmpPath' and 'fileNameInCache' are ignored and the file is only opened in the viewer. If 'useCache' is TRUE, the file is first moved into the disk cache under the file name 'fileNameInCache' (name only, no path), then opened in the viewer, and after the viewer is closed it is removed from the disk cache. If 'pluginData->FileName' is on the same disk as the disk cache, the move is immediate; otherwise the file is copied across volumes, which may take longer and shows no progress. If 'rootTmpPath' is NULL, the disk cache is in the Windows TEMP directory; otherwise the disk cache path is in 'rootTmpPath'. SalMoveFile is used to move the file into the disk cache; ideally use SalGetTempFileName with its 'path' parameter equal to 'rootTmpPath'. Returns TRUE if the file was opened successfully in the viewer. Returns FALSE if an error occurs while opening; the specific reason is stored in 'error' (0 - success, 1 - the plugin cannot be loaded, 2 - the plugin's ViewFile returned an error, 3 - the file cannot be moved into the disk cache). If 'useCache' is TRUE, the file is removed from disk in that case as well, just as after the viewer is closed.
    // Restriction: main thread (otherwise plugin configuration may change during the call); additionally, it must not be called from the entry point (plugin loading is not reentrant).
    virtual BOOL WINAPI ViewFileInPluginViewer(const char* pluginSPL,
                                               CSalamanderPluginViewerData* pluginData,
                                               BOOL useCache, const char* rootTmpPath,
                                               const char* fileNameInCache, int& error) = 0;

    // Notifies Salamander, then all loaded plugins, and then all open FS instances (in panels and detached) about a change on path 'path' (disk path or FS path) as soon as possible. This is important for paths whose changes cannot be monitored automatically (see FindFirstChangeNotification) or when the user has turned that monitoring off; for FS paths, change monitoring is handled by the plugin itself. Change notification runs as soon as possible: if the main thread is not running code inside a plugin, the refresh happens after the message is delivered to the main window and, if needed, after refreshing is allowed again (after a dialog is closed, etc.); otherwise the refresh waits at least until the main thread leaves the plugin. 'includingSubdirs' is TRUE if the change can also affect subdirectories of 'path'.
    // WARNING: if called from a thread other than the main thread, change notification (which runs in the main thread) may happen even before PostChangeOnPathNotification returns.
    // Callable from any thread.
    virtual void WINAPI PostChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // Attempts to access Windows path 'path' (normal or UNC). It runs in a worker thread, so the test can be interrupted with the ESC key; after some time a window appears telling the user about ESC. 'echo' TRUE means error messages are allowed if the path is not accessible. If 'err' is not ERROR_SUCCESS and 'echo' is TRUE, the method only shows the error and does not access the path again. 'parent' is the parent of the message box. Returns ERROR_SUCCESS if the path is OK; otherwise it returns a standard Windows error code or ERROR_USER_TERMINATED if the user pressed ESC to cancel the test.
    // Restriction: main thread (repeated calls are not possible and the main thread uses this method).
    virtual DWORD WINAPI SalCheckPath(BOOL echo, const char* path, DWORD err, HWND parent) = 0;

    // Checks whether Windows path 'path' is accessible and, if necessary, restores network connections. For a normal path it tries to revive a remembered network connection; for a UNC path it allows login with a new user name and password. Returns TRUE if the path is accessible. 'parent' is the parent of message boxes and dialogs. 'tryNet' is TRUE if trying to restore network connections makes sense; if FALSE, the method degrades to SalCheckPath and exists only as an optimization.
    // Restriction: main thread (repeated calls are not possible and the main thread uses this method).
    virtual BOOL WINAPI SalCheckAndRestorePath(HWND parent, const char* path, BOOL tryNet) = 0;

    // More complex variant of SalCheckAndRestorePath. It checks whether Windows path 'path' is accessible and, if necessary, shortens it. If 'tryNet' is TRUE, it also tries to restore network connections and then sets 'tryNet' to FALSE. For a normal path it tries to revive a remembered network connection; for a UNC path it allows login with a new user name and password. If 'donotReconnect' is TRUE, only the error is determined and no reconnection is attempted. Returns 'err' (Windows error code of the current path), 'lastErr' (the error code that led to shortening the path), 'pathInvalid' (TRUE if restoring the network connection was attempted without success), and 'cut' (TRUE if the resulting path was shortened). 'parent' is the parent of the message box. Returns TRUE if the resulting path 'path' is accessible.
    // Restriction: main thread (repeated calls are not possible and the main thread uses this method).
    virtual BOOL WINAPI SalCheckAndRestorePathWithCut(HWND parent, char* path, BOOL& tryNet, DWORD& err,
                                                      DWORD& lastErr, BOOL& pathInvalid, BOOL& cut,
                                                      BOOL donotReconnect) = 0;

    // Recognizes what kind of path it is (FS/Windows/archive) and splits it into its parts (for FS paths, fs-name and fs-user-part; for archive paths, path-to-archive and path-in-archive; for Windows paths, the existing part and the rest of the path). FS paths are not validated. For Windows paths (normal + UNC), it checks how far the path exists and may restore a network connection. For archive paths, it checks whether the archive file exists (archives are recognized by extension).
    // 'path' is a full or relative path (buffer of at least 'pathBufSize' characters; for relative paths, the current path 'curPath' is used as the base for resolving the full path if it is not NULL; 'curPathIsDiskOrArchive' is TRUE if 'curPath' is a Windows or archive path; if the current path is an archive path, 'curArchivePath' contains the archive name, otherwise it is NULL). The resulting full path is stored back into 'path' (it must have room for at least 'pathBufSize' characters). Returns TRUE on successful recognition. Then 'type' is the path type (see PATH_TYPE_XXX) and 'secondPart' is set to:
    // - the position in 'path' just after the existing path (after '\\' or at the end of the string; if the path contains a file, it points just after the path to that file) for Windows paths; WARNING: the length of the returned path part is not limited, so the full path may be longer than MAX_PATH
    // - the position after the archive file for archive paths; WARNING: the path inside the archive may be longer than MAX_PATH
    // - the position after ':' after the file-system name, that is, the user part of the file-system path, for FS paths; WARNING: the user-part length may be longer than MAX_PATH
    // If TRUE is returned, 'isDir' is also set to:
    // - TRUE if the existing part of the path is a directory, FALSE if it is a file, for Windows paths
    // - FALSE for archive and FS paths
    // If FALSE is returned, an error was already reported to the user, except for one case described by SPP_INCOMLETEPATH; if 'error' is not NULL, it receives one of the SPP_XXX constants. 'errorTitle' is the title of the error message box. If 'nextFocus' != NULL and the Windows/archive path does not contain '\\' or ends right after '\\', the path is copied to 'nextFocus' (see SalGetFullName).
    // WARNING: this uses SalGetFullName, so it is best to call CSalamanderGeneralAbstract::SalUpdateDefaultDir first.
    // Restriction: main thread (repeated calls are not possible and the main thread uses this method).
    virtual BOOL WINAPI SalParsePath(HWND parent, char* path, int& type, BOOL& isDir, char*& secondPart,
                                     const char* errorTitle, char* nextFocus, BOOL curPathIsDiskOrArchive,
                                     const char* curPath, const char* curArchivePath, int* error,
                                     int pathBufSize) = 0;

    // Obtains the existing part and the operation mask from a target Windows path and allows any missing part to be created. On success it returns TRUE and provides the existing target Windows path in 'path' and the found operation mask in 'mask' (it points into the 'path' buffer, but path and mask are separated by a null character; if the path contains no mask, the method automatically creates "*.*"). 'parent' is the parent of any message boxes. 'title' and 'errorTitle' are the titles of the information and error message boxes. 'selCount' is the number of selected files and directories. 'path' on input is the target path to process; on output it must hold the existing target path and therefore must have room for at least 2 * MAX_PATH characters. 'secondPart' points into 'path' to the position after the existing path (after '\\' or at the end of the string; if the path contains a file, it points after the path to that file). 'pathIsDir' is TRUE/FALSE depending on whether the existing part of the path is a directory/file. 'backslashAtEnd' is TRUE if 'path' ended with a backslash before parsing (for example, SalParsePath removes such a backslash). 'dirName' and 'curDiskPath' are not NULL when at most one file/directory is selected (its name without path is in 'dirName'; if nothing is selected, the focused item is used) and the current path is a Windows path (stored in 'curDiskPath'). 'mask' on output is a pointer to the operation mask inside the 'path' buffer. If the path contains an error, the method returns FALSE and the problem has already been reported to the user. Callable from any thread.
    virtual BOOL WINAPI SalSplitWindowsPath(HWND parent, const char* title, const char* errorTitle,
                                            int selCount, char* path, char* secondPart, BOOL pathIsDir,
                                            BOOL backslashAtEnd, const char* dirName,
                                            const char* curDiskPath, char*& mask) = 0;

    // Obtains the existing part and the operation mask from a target path and recognizes any missing part. On success it returns TRUE, the relative path to create in 'newDirs', the existing target path in 'path' (existing only under the assumption that the relative path 'newDirs' will be created), and the found operation mask in 'mask' (it points into the 'path' buffer, but path and mask are separated by a null character; if the path contains no mask, the method automatically creates "*.*"). 'parent' is the parent of any message boxes. 'title' and 'errorTitle' are the titles of the information and error message boxes. 'selCount' is the number of selected files and directories. 'path' on input is the target path to process; on output it must contain the existing target path and therefore must have room for at least 2 * MAX_PATH characters. 'afterRoot' points into 'path' just after the path root (after '\\' or at the end of the string). 'secondPart' points into 'path' to the position after the existing path (after '\\' or at the end of the string; if the path contains a file, it points after the path to that file). 'pathIsDir' is TRUE/FALSE depending on whether the existing part of the path is a directory/file. 'backslashAtEnd' is TRUE if 'path' ended with a backslash before parsing (for example, SalParsePath removes such a backslash). 'dirName' and 'curPath' are not NULL when at most one file/directory is selected (its name without path is in 'dirName'; its path is in 'curPath'; if nothing is selected, the focused item is used). 'mask' on output is a pointer to the operation mask inside the 'path' buffer. If 'newDirs' is not NULL, it is a buffer of at least MAX_PATH characters for the relative path, relative to the existing path in 'path', that must be created; the user has already agreed to create it, using the same prompt as for copying from disk to disk; an empty string means create nothing. If 'newDirs' is NULL and a relative path would need to be created, only an error is displayed. 'isTheSamePathF' is a function for comparing two paths, needed only if 'curPath' is not NULL; if it is NULL, IsTheSamePath is used. If the path contains an error, the method returns FALSE and the problem has already been reported to the user. Callable from any thread.
    virtual BOOL WINAPI SalSplitGeneralPath(HWND parent, const char* title, const char* errorTitle,
                                            int selCount, char* path, char* afterRoot, char* secondPart,
                                            BOOL pathIsDir, BOOL backslashAtEnd, const char* dirName,
                                            const char* curPath, char*& mask, char* newDirs,
                                            SGP_IsTheSamePathF isTheSamePathF) = 0;

    // Removes ".." (removes ".." together with one subdirectory to the left) and "." (removes only ".") from a path. It requires backslashes as subdirectory separators. 'afterRoot' points just after the root of the processed path; the path is modified only after 'afterRoot'. Returns TRUE if the adjustment succeeded, FALSE if ".." cannot be removed because the root is already to the left. Callable from any thread.
    virtual BOOL WINAPI SalRemovePointsFromPath(char* afterRoot) = 0;

    // Returns a parameter from Salamander's configuration. 'paramID' identifies which parameter is requested (see SALCFG_XXX constants). 'buffer' points to the buffer that receives the parameter data; its size is 'bufferSize'. If 'type' is not NULL, it receives one of the SALCFGTYPE_XXX constants or SALCFGTYPE_NOTFOUND if no parameter with 'paramID' was found. Returns TRUE if 'paramID' is valid and the configuration parameter value fits into 'buffer'.
    // Note: Salamander configuration changes are reported by the PLUGINEVENT_CONFIGURATIONCHANGED event (see CPluginInterfaceAbstract::Event).
    // Restriction: main thread; configuration changes happen only in the main thread (there is no other synchronization).
    virtual BOOL WINAPI GetConfigParameter(int paramID, void* buffer, int bufferSize, int* type) = 0;

    // Changes letter case in a file name (the name does not include a path); 'tgtName' is the output
    // buffer (its size must be at least enough to store string 'srcName'); 'srcName' is the file name
    // (it is written to, but is always restored before the method returns); 'format' is the output format
    // (1 - capitalize words, 2 - all lowercase, 3 - all uppercase, 4 - unchanged, 5 - if it is a DOS
    // name (8.3) -> capitalize words, 6 - file lowercase, directory uppercase,
    // 7 - capitalize the name and lowercase the extension);
    // 'changedParts' specifies which parts of the name are changed (0 - change both name and extension,
    // 1 - change only the name (possible only with format == 1, 2, 3, 4), 2 - change only the extension
    // (possible only with format == 1, 2, 3, 4)); 'isDir' is TRUE if this is a directory name
    // Can be called from any thread
    virtual void WINAPI AlterFileName(char* tgtName, char* srcName, int format, int changedParts,
                                      BOOL isDir) = 0;

    // Shows/hides a message in a window running in its own thread (it does not drain the
    // message queue); only one message is shown at a time, repeated calls report an error
    // to TRACE (not fatal);
    // NOTE: this is used in SalCheckPath and other routines, so requests to open the window
    //       can collide (not fatal, the window is simply not shown)
    // All functions can be called from any thread (but the window itself must be handled
    // from only one thread - it cannot be shown from one thread and hidden from another)
    //
    // Opens a window with text 'message' after delay 'delay' (in ms), only if 'hForegroundWnd' is NULL
    // or identifies the foreground window
    // 'message' may be multiline; individual lines are separated by '\n'
    // 'caption' may be NULL: 'Open Salamander' is then used
    // 'showCloseButton' specifies whether the window contains a Close button; equivalent to the Escape key
    virtual void WINAPI CreateSafeWaitWindow(const char* message, const char* caption,
                                             int delay, BOOL showCloseButton, HWND hForegroundWnd) = 0;
    // closing the window
    virtual void WINAPI DestroySafeWaitWindow() = 0;
    // Hide/show the window (if it is open); call in response to WM_ACTIVATE from hForegroundWnd:
    //    case WM_ACTIVATE:
    //    {
    //      ShowSafeWaitWindow(LOWORD(wParam) != WA_INACTIVE);
    //      break;
    //    }
    // If the thread in which the window was created is busy, messages are not dispatched,
    // so WM_ACTIVATE is not delivered when the user clicks another application. The messages
    // are delivered only when a message box is shown, which is exactly what we need:
    // hide the window temporarily and show it again later (after the message box is closed
    // and the hForegroundWnd window is activated).
    virtual void WINAPI ShowSafeWaitWindow(BOOL show) = 0;
    // After CreateSafeWaitWindow or ShowSafeWaitWindow is called, the function returns FALSE until
    // the user clicks the Close button with the mouse (if it is shown); it then returns TRUE
    virtual BOOL WINAPI GetSafeWaitWindowClosePressed() = 0;
    // Used to change the text in the window later.
    // WARNING: the window layout is not recalculated, so if the text becomes much longer,
    // it will be clipped; use for example for countdowns: 60s, 55s, 50s, ...
    virtual void WINAPI SetSafeWaitWindowText(const char* message) = 0;

    // Finds an existing copy of a file in the disk cache and locks it (prevents its deletion); 'uniqueFileName'
    // is the unique name of the original file (the disk cache is searched by this name; the full
    // file name in Salamander format - "fs-name:fs-user-part" - should be sufficient; WARNING: the
    // name is compared case-sensitively; if the plugin requires case-insensitive matching, it must
    // convert all names, for example to lower case - see CSalamanderGeneralAbstract::ToLowerCase); 'tmpName'
    // receives a pointer (valid until the file copy in the disk cache is removed) to the full name
    // of the file copy located in a temporary directory; 'fileLock' is the lock for the file copy,
    // a system event in the nonsignaled state that transitions to the signaled state after the file
    // copy has been processed (it is necessary to use UnlockFileInCache; the plugin signals that the
    // disk-cache copy may already be removed); if the copy was not found, returns FALSE and 'tmpName'
    // is NULL (otherwise returns TRUE)
    // Can be called from any thread
    virtual BOOL WINAPI GetFileFromCache(const char* uniqueFileName, const char*& tmpName, HANDLE fileLock) = 0;

    // Unlocks the lock for a file copy in the disk cache (sets 'fileLock' to the signaled state, asks
    // the disk cache to check the locks, and then sets 'fileLock' back to the nonsignaled state);
    // if this was the last lock, the copy may be removed; when it is removed depends
    // on the size of the disk cache on disk; one lock can also be used for multiple file copies (the lock
    // must be of type 'manual reset', otherwise after the first copy is unlocked the lock is set back to
    // nonsignaled and unlocking stops); in that case all copies are unlocked
    // Can be called from any thread
    virtual void WINAPI UnlockFileInCache(HANDLE fileLock) = 0;

    // Inserts (moves) a file copy into the disk cache (the inserted copy is not locked, so it may
    // be removed at any time); 'uniqueFileName' is the unique name of the original file (the disk
    // cache is searched by this name; the full file name in Salamander format - "fs-name:fs-user-part"
    // - should be sufficient; WARNING: the name is compared case-sensitively; if the plugin
    // requires case-insensitive matching, it must convert all names, for example to lowercase - see
    // CSalamanderGeneralAbstract::ToLowerCase); 'nameInCache' is the name of the file copy that will
    // be placed in the temporary directory (the last part of the original file name is expected here
    // so that it later resembles the original file to the user); 'newFileName' is the full name of
    // the stored file copy that will be moved into the disk cache under the name 'nameInCache'; it
    // must be located on the same disk as the disk cache (if 'rootTmpPath' is NULL, the disk cache
    // is in the Windows TEMP directory; otherwise the path to the disk cache is in 'rootTmpPath';
    // this is required for renaming into the disk cache with the Win32 API function MoveFile);
    // ideally, obtain 'newFileName' by calling SalGetTempFileName with parameter 'path' equal to
    // 'rootTmpPath'); 'newFileSize' is the size of the stored file copy; returns TRUE on success
    // (the file was moved into the disk cache and disappeared from its original location on disk),
    // returns FALSE on an internal error or if the file is already in the disk cache (if
    // 'alreadyExists' is not NULL, it receives TRUE when the file is already in the disk cache)
    // NOTE: if the plugin uses the disk cache, it should at least call
    //       CSalamanderGeneralAbstract::RemoveFilesFromCache("fs-name:") when the plugin is
    //       unloaded, otherwise its file copies will remain in the disk cache unnecessarily
    // Can be called from any thread
    virtual BOOL WINAPI MoveFileToCache(const char* uniqueFileName, const char* nameInCache,
                                        const char* rootTmpPath, const char* newFileName,
                                        const CQuadWord& newFileSize, BOOL* alreadyExists) = 0;

    // Removes from the disk cache the file copy whose unique name is 'uniqueFileName' (WARNING: the name
    // is compared case-sensitively; if the plugin requires case-insensitive matching, it must convert all names,
    // for example to lower case - see CSalamanderGeneralAbstract::ToLowerCase); if the file copy is still
    // in use, it is removed when possible (after viewers are closed); in any case, the disk cache will no longer
    // provide it as a valid file copy (it is marked out-of-date)
    // Can be called from any thread
    virtual void WINAPI RemoveOneFileFromCache(const char* uniqueFileName) = 0;

    // Removes from the disk cache all file copies whose unique names start with 'fileNamesRoot'
    // (used when a file system is being closed and downloaded file copies should no longer be cached;
    // WARNING: names are compared case-sensitively; if the plugin requires case-insensitive matching,
    // it must convert all names, for example to lower case - see CSalamanderGeneralAbstract::ToLowerCase);
    // if file copies are still in use, they are removed when possible (after they are unlocked,
    // for example after a viewer is closed); in any case, the disk cache will no longer provide them as valid
    // file copies (they are marked out-of-date)
    // Can be called from any thread
    virtual void WINAPI RemoveFilesFromCache(const char* fileNamesRoot) = 0;

    // Returns conversion tables one by one (loaded from convert\XXX\convert.cfg
    // in the Salamander installation - XXX is the currently used conversion-table directory);
    // 'parent' is the parent of the message box (if NULL, the main window is the parent);
    // 'index' is an input/output variable pointing to an int that is 0 on the first call;
    // the function stores the value for the next call on return (usage: initialize it to 0, then
    // do not modify it); returns FALSE if there is no next table; if it returns TRUE,
    // 'name' (if not NULL) contains a pointer to the conversion name (it may contain '&' - the
    // underlined character in the menu) or NULL if it is a separator, and 'table' (if not NULL)
    // contains a pointer to a 256-byte conversion table or NULL if it is a separator; the 'name'
    // and 'table' pointers remain valid for the entire Salamander run (you do not need to copy them)
    // WARNING: use pointer 'table' this way (cast to "unsigned" is required):
    //        *s = table[(unsigned char)*s]
    // Can be called from any thread
    virtual BOOL WINAPI EnumConversionTables(HWND parent, int* index, const char** name, const char** table) = 0;

    // Returns conversion table 'table' (a buffer of at least 256 characters) for conversion 'conversion' (the
    // conversion name is defined in file convert\XXX\convert.cfg in the Salamander installation, e.g.
    // 'ISO-8859-2 - CP1250'; characters <= ' ', '-', and '&' in the name are ignored during the search;
    // the search is case-insensitive); 'parent' is the parent window of the message box (if NULL, the main
    // window is the parent); returns TRUE if the conversion was found
    // (otherwise the contents of 'table' are invalid);
    // WARNING: use it this way (cast to 'unsigned' is required): *s = table[(unsigned char)*s]
    // Can be called from any thread
    virtual BOOL WINAPI GetConversionTable(HWND parent, char* table, const char* conversion) = 0;

    // Returns the name of the code page used by Windows in this region (read from convert\XXX\convert.cfg
    // in the Salamander installation); this is a normally displayable encoding, so it is used when
    // text created in a different code page needs to be displayed (it is passed as the target
    // encoding when looking up the conversion table; see GetConversionTable);
    // 'parent' is the parent window of the message box (if NULL, the main window is the parent); 'codePage' is a buffer
    // (at least 101 characters) for the code-page name (if this name is not defined in file convert\XXX\convert.cfg,
    // an empty string is returned in the buffer)
    // Can be called from any thread
    virtual void WINAPI GetWindowsCodePage(HWND parent, char* codePage) = 0;

    // Determines from buffer 'pattern' of length 'patternLen' (e.g. the first 10000 characters) whether it is
    // text (there is a code page in which it contains only permitted characters - printable
    // and control) and, if it is text, also determines its most likely code page;
    // 'parent' is the parent of the message box (if NULL, the main window is the parent); if 'forceText'
    // is TRUE, the check for disallowed characters is skipped (used when 'pattern' contains
    // text); if 'isText' is not NULL, TRUE is returned in it if the buffer is text; if 'codePage'
    // is not NULL, it is a buffer (min. 101 characters) for the code-page name (the most likely one)
    // Can be called from any thread
    virtual void WINAPI RecognizeFileType(HWND parent, const char* pattern, int patternLen, BOOL forceText,
                                          BOOL* isText, char* codePage) = 0;

    // Determines from buffer 'text' of length 'textLen' whether it is ANSI text (it contains only
    // permitted characters in the ANSI character set - printable and control characters); it decides without context
    // (it does not depend on the number of characters or their order - the tested text can be split
    // into arbitrary parts and tested gradually); returns TRUE if it is ANSI text (otherwise
    // the contents of buffer 'text' are binary data)
    // Can be called from any thread
    virtual BOOL WINAPI IsANSIText(const char* text, int textLen) = 0;

    // Calls function 'callback' with parameters 'param' and a function for retrieving selected
    // files/directories (see the SalPluginOperationFromDisk type definition) from panel 'panel'
    // (a Windows path must be open in the panel); 'panel' is one of PANEL_XXX
    // Restriction: main thread
    virtual void WINAPI CallPluginOperationFromDisk(int panel, SalPluginOperationFromDisk callback,
                                                    void* param) = 0;

    // Returns the default charset configured by the user (part of the regional
    // settings); fonts must be created with this charset, otherwise the
    // texts may not be readable (if the text uses the default code page, see the Win32 API function
    // GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_IDEFAULTANSICODEPAGE, ...))
    // Can be called from any thread
    virtual BYTE WINAPI GetUserDefaultCharset() = 0;

    // alokuje novy objekt Boyer-Moorova vyhledavaciho algoritmu
    // mozne volat z libovolneho threadu
    virtual CSalamanderBMSearchData* WINAPI AllocSalamanderBMSearchData() = 0;

    // uvolni objekt Boyer-Moorova vyhledavaciho algoritmu (ziskany metodou AllocSalamanderBMSearchData)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderBMSearchData(CSalamanderBMSearchData* data) = 0;

    // alokuje novy objekt algoritmu pro vyhledavani pomoci regularnich vyrazu
    // mozne volat z libovolneho threadu
    virtual CSalamanderREGEXPSearchData* WINAPI AllocSalamanderREGEXPSearchData() = 0;

    // uvolni objekt algoritmu pro vyhledavani pomoci regularnich vyrazu (ziskany metodou
    // AllocSalamanderREGEXPSearchData)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderREGEXPSearchData(CSalamanderREGEXPSearchData* data) = 0;

    // Returns Salamander commands one by one (in the definition order of SALCMD_XXX constants);
    // 'index' is an input/output variable pointing to an int that is 0 on the first call;
    // the function stores the value for the next call on return (usage: initialize it to 0, then
    // do not modify it); returns FALSE if there is no next command; if it returns TRUE,
    // 'salCmd' (if not NULL) receives the Salamander command number (see SALCMD_XXX constants;
    // numbers 0 through 499 are reserved, so if Salamander commands need to appear in a menu together
    // with other commands, it is easy to create non-overlapping command-value sets, for example by
    // offsetting all values by a chosen number - see DEMOPLUGin -
    // CPluginFSInterface::ContextMenu), 'nameBuf' (a buffer of size 'nameBufSize' bytes)
    // contains the command name (prepared for use in a menu - ampersands are doubled,
    // underlined characters are marked by ampersands, and shortcut descriptions follow '\t'), 'enabled'
    // (if not NULL) contains the command state (TRUE/FALSE for enabled/disabled), and 'type'
    // (if not NULL) contains the command type (see the sctyXXX constants)
    // Can be called from any thread
    virtual BOOL WINAPI EnumSalamanderCommands(int* index, int* salCmd, char* nameBuf, int nameBufSize,
                                               BOOL* enabled, int* type) = 0;

    // Returns the Salamander command with number 'salCmd' (see SALCMD_XXX constants);
    // returns FALSE if no such command exists; if it returns TRUE,
    // 'nameBuf' (a buffer of size 'nameBufSize' bytes) contains the command name (prepared for
    // use in a menu - ampersands are doubled, underlined characters are marked by ampersands,
    // and shortcut descriptions follow '\t'), 'enabled' (if not NULL) contains the command state (TRUE/FALSE
    // for enabled/disabled), and 'type' (if not NULL) contains the command type (see the
    // sctyXXX constants)
    // Can be called from any thread
    virtual BOOL WINAPI GetSalamanderCommand(int salCmd, char* nameBuf, int nameBufSize, BOOL* enabled,
                                             int* type) = 0;

    // Sets a flag for the calling plugin so that the Salamander command with number 'salCmd' is run at the
    // next possible opportunity (as soon as there are no messages in the main-thread message queue and
    // Salamander is not 'busy' (no modal dialog is open and no message is being
    // processed));
    // WARNING: if this is called from a thread other than the main thread, the Salamander command
    // (which runs in the main thread) may be executed even before PostSalamanderCommand returns
    // Can be called from any thread
    virtual void WINAPI PostSalamanderCommand(int salCmd) = 0;

    // Sets the 'user worked with the current path' flag in panel 'panel' (this flag
    // is used when filling the List Of Working Directories (Alt+F12));
    // 'panel' is one of PANEL_XXX
    // Restriction: main thread
    virtual void WINAPI SetUserWorkedOnPanelPath(int panel) = 0;

    // In panel 'panel' (one of the PANEL_XXX constants), stores the selected names
    // in a special array from which the user can restore the selection with Edit/Restore Selection;
    // it is used for commands that clear the current selection so the user can
    // return to it and perform another operation
    // Restriction: main thread
    virtual void WINAPI StoreSelectionOnPanelPath(int panel) = 0;

    //
    // UpdateCrc32
    //   Updates CRC-32 (32-bit Cyclic Redundancy Check) with specified array of bytes.
    //
    // Parameters
    //   'buffer'
    //      [in] Pointer to the starting address of the block of memory to update 'crcVal' with.
    //
    //   'count'
    //      [in] Size, in bytes, of the block of memory to update 'crcVal' with.
    //
    //   'crcVal'
    //      [in] Initial crc value. Set this value to zero to calculate CRC-32 of the 'buffer'.
    //
    // Return Values
    //   Returns updated CRC-32 value.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual DWORD WINAPI UpdateCrc32(const void* buffer, DWORD count, DWORD crcVal) = 0;

    // alokuje novy objekt pro vypocet MD5
    // mozne volat z libovolneho threadu
    virtual CSalamanderMD5* WINAPI AllocSalamanderMD5() = 0;

    // uvolni objekt pro vypocet MD5 (ziskany metodou AllocSalamanderMD5)
    // mozne volat z libovolneho threadu
    virtual void WINAPI FreeSalamanderMD5(CSalamanderMD5* md5) = 0;

    // Finds '<' and '>' pairs in the text, removes them from the buffer, and adds references to
    // their contents to 'varPlacements'. 'varPlacements' is an array of DWORD values with
    // '*varPlacementsCount' items; each DWORD is composed of the position of the reference in the output buffer (low WORD)
    // and the length of the referenced text (high WORD). Strings '\<', '\>', and '\\' are treated
    // as escape sequences and replaced with '<', '>', and '\\'.
    // Returns TRUE on success, otherwise FALSE; 'varPlacementsCount' is always set to
    // the number of processed variables.
    // Can be called from any thread
    virtual BOOL WINAPI LookForSubTexts(char* text, DWORD* varPlacements, int* varPlacementsCount) = 0;

    // Waits (at most 0.2 seconds) for the ESC key to be released; used if the plugin contains
    // actions that are interrupted by the ESC key (monitoring the ESC key via
    // GetAsyncKeyState(VK_ESCAPE)) - this prevents pressing ESC in a dialog/message box from
    // immediately interrupting the next action that monitors the ESC key
    // Can be called from any thread
    virtual void WINAPI WaitForESCRelease() = 0;

    //
    // GetMouseWheelScrollLines
    //   An OS independent method to retrieve the number of wheel scroll lines.
    //
    // Return Values
    //   Number of scroll lines where WHEEL_PAGESCROLL (0xffffffff) indicates to scroll a page at a time.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual DWORD WINAPI GetMouseWheelScrollLines() = 0;

    //
    // GetTopVisibleParent
    //   Retrieves the visible root window by walking the chain of parent windows
    //   returned by GetParent.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the window whose parent window handle is to be retrieved.
    //
    // Return Values
    //   The return value is the handle to the top Popup or Overlapped visible parent window.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual HWND WINAPI GetTopVisibleParent(HWND hParent) = 0;

    //
    // MultiMonGetDefaultWindowPos
    //   Retrieves the default position of the upper-left corner for a newly created window
    //   on the display monitor that has the largest area of intersection with the bounding
    //   rectangle of a specified window.
    //
    // Parameters
    //   'hByWnd'
    //      [in] Handle to the window of interest.
    //
    //   'p'
    //      [out] Pointer to a POINT structure that receives the virtual-screen coordinates
    //      of the upper-left corner for the window that would be created with CreateWindow
    //      with CW_USEDEFAULT in the 'x' parameter. Note that if the monitor is not the
    //      primary display monitor, some of the point's coordinates may be negative values.
    //
    // Return Values
    //   If the default window position lies on the primary monitor or some error occured,
    //   the return value is FALSE and you should use CreateWindow with CW_USEDEFAULT in
    //   the 'x' parameter.
    //
    //   Otherwise the return value is TRUE and coordinates from 'p' structure should be used
    //   in the CreateWindow 'x' and 'y' parameters.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI MultiMonGetDefaultWindowPos(HWND hByWnd, POINT* p) = 0;

    //
    // MultiMonGetClipRectByRect
    //   Retrieves the bounding rectangle of the display monitor that has the largest
    //   area of intersection with a specified rectangle.
    //
    // Parameters
    //   'rect'
    //      [in] Pointer to a RECT structure that specifies the rectangle of interest
    //      in virtual-screen coordinates.
    //
    //   'workClipRect'
    //      [out] A RECT structure that specifies the work area rectangle of the
    //      display monitor, expressed in virtual-screen coordinates. Note that
    //      if the monitor is not the primary display monitor, some of the rectangle's
    //      coordinates may be negative values.
    //
    //   'monitorClipRect'
    //      [out] A RECT structure that specifies the display monitor rectangle,
    //      expressed in virtual-screen coordinates. Note that if the monitor is
    //      not the primary display monitor, some of the rectangle's coordinates
    //      may be negative values. This parameter can be NULL.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual void WINAPI MultiMonGetClipRectByRect(const RECT* rect, RECT* workClipRect, RECT* monitorClipRect) = 0;

    //
    // MultiMonGetClipRectByWindow
    //   Retrieves the bounding rectangle of the display monitor that has the largest
    //   area of intersection with the bounding rectangle of a specified window.
    //
    // Parameters
    //   'hByWnd'
    //      [in] Handle to the window of the interest. If this parameter is NULL,
    //      or window is not visible or is iconic, monitor with the currently active window
    //      from the same application will be used. Otherwise primary monitor will be used.
    //
    //   'workClipRect'
    //      [out] A RECT structure that specifies the work area rectangle of the
    //      display monitor, expressed in virtual-screen coordinates. Note that
    //      if the monitor is not the primary display monitor, some of the rectangle's
    //      coordinates may be negative values.
    //
    //   'monitorClipRect'
    //      [out] A RECT structure that specifies the display monitor rectangle,
    //      expressed in virtual-screen coordinates. Note that if the monitor is
    //      not the primary display monitor, some of the rectangle's coordinates
    //      may be negative values. This parameter can be NULL.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual void WINAPI MultiMonGetClipRectByWindow(HWND hByWnd, RECT* workClipRect, RECT* monitorClipRect) = 0;

    // MultiMonCenterWindow
    //   Centers the window relative to a specified window or monitor.
    //
    // Parameters
    //   'hWindow'
    //      [in] Handle to the window to center.
    //
    //   'hByWnd'
    //      [in] Handle to the window relative to which 'hWindow' is centered. If this
    //      parameter is NULL, or if the window is not visible or is iconic, the method
    //      centers 'hWindow' relative to the monitor work area. The monitor with the
    //      currently active window from the same application is used; otherwise the
    //      primary monitor is used.
    //
    //   'findTopWindow'
    //      [in] If this parameter is TRUE, the top visible non-child window found by
    //      walking the chain of parent windows of 'hByWnd' is used as the reference window.
    //
    //      If this parameter is FALSE, 'hByWnd' itself is used as the reference window.
    //
    // Remarks
    //   If the centered window extends beyond the work area of the monitor, the method
    //   positions it so that it is fully visible.
    //
    //   Method can be called from any thread.
    virtual void WINAPI MultiMonCenterWindow(HWND hWindow, HWND hByWnd, BOOL findTopWindow) = 0;

    //
    // MultiMonEnsureRectVisible
    //   Ensures that specified rectangle is either entirely or partially visible,
    //   adjusting the coordinates if necessary. All monitors are considered.
    //
    // Parameters
    //   'rect'
    //      [in/out] Pointer to the RECT structure that contain the coordinated to be
    //      adjusted. The rectangle is presumed to be in virtual-screen coordinates.
    //
    //   'partialOK'
    //      [in] Value specifying whether the rectangle must be entirely visible.
    //      If this parameter is TRUE, no moving occurs if the item is at least
    //      partially visible.
    //
    // Return Values
    //   If the rectangle is adjusted, the return value is TRUE.
    //
    //   If the rectangle is not adjusted, the return value is FALSE.
    //
    // Remarks
    //   Method can be called from any thread.
    //
    virtual BOOL WINAPI MultiMonEnsureRectVisible(RECT* rect, BOOL partialOK) = 0;

    // InstallWordBreakProc
    //   Installs a special word-break procedure for the specified window. This procedure
    //   is intended to make cursor movement easier in single-line edit controls.
    //   Delimiters '\\', '/', ' ', ';', ',', and '.' are used as cursor stops when the user
    //   navigates with Ctrl+Left or Ctrl+Right.
    //   Ctrl+Backspace can be used to delete one word.
    //
    // Parameters
    //   'hWindow'
    //      [in] Handle to the window or control where the word-break proc is to be installed.
    //      The window may be either an edit control or a combo box with an edit control.
    //
    // Return Values
    //   The return value is TRUE if the word-break proc is installed. It is FALSE if the
    //   window is neither an edit control nor a combo box with an edit control, if an
    //   error occurred, or if this special word-break proc is not supported on the current OS.
    //
    // Remarks
    //   You need not uninstall the word-break procedure before the window is destroyed.
    //
    //   Method can be called from any thread.
    virtual BOOL WINAPI InstallWordBreakProc(HWND hWindow) = 0;

    // Salamander 3 or later: returns TRUE if this Altap Salamander instance was the
    // first instance started (when an instance starts, other running instances of version 3 or later
    // are searched for);
    //
    // Notes on different SID / Session / Integrity Level combinations (does not apply to Salamander 2.5 and 2.51):
    // the function returns TRUE even if a Salamander instance is already running
    // under a different SID; session and integrity level do not matter, so if a
    // Salamander instance is already running in another session or with a different integrity level,
    // but with the same SID, the newly started instance returns FALSE
    //
    // Can be called from any thread
    virtual BOOL WINAPI IsFirstInstance3OrLater() = 0;

    // Support for parameter-dependent strings (handling singular/plural forms);
    // 'format' is the format string for the resulting string - see the description below;
    // the resulting string is copied to buffer 'buffer', whose size is 'bufferSize' bytes;
    // 'parametersArray' is an array of parameters; 'parametersCount' is the number of
    // these parameters; returns the length of the resulting string
    //
    // format string description:
    //   - every format string starts with signature '{!}'
    //   - the format string can contain the following escape sequences (to use a special
    //     character without its special meaning): '\\' = '\', '\{' = '{', '\}' = '}',
    //     '\:' = ':', and '\|' = '|' (do not forget to double backslashes when writing C++
    //     strings; this applies only to format strings placed directly in C++ source code)
    //   - text outside curly brackets is copied directly to the resulting string
    //     (only escape sequences are processed)
    //   - parameter-dependent text is placed in curly brackets
    //   - each parameter-dependent text uses one parameter from 'parametersArray'
    //     (a 64-bit unsigned int)
    //   - parameter-dependent text contains several resulting-text variants; the selected
    //     variant depends on the parameter value, specifically on the interval it falls into
    //   - resulting-text variants and interval bounds are separated by the '|' character
    //   - the first interval is from 0 to the first interval bound
    //   - the last interval is from the last interval bound plus one to infinity (2^64-1)
    //   - parameter-dependent text '{}' skips one parameter from 'parametersArray'
    //     (nothing is added to the resulting string)
    //   - you can also specify the index of the parameter to use for parameter-dependent text;
    //     just place its index (from one to the number of parameters) at the beginning of
    //     the parameter-dependent text and follow it with a ':' character
    //   - if you do not specify the index of the parameter to use, it is assigned automatically
    //     (from one to the number of parameters)
    //   - if you do specify the index of the parameter to use, the next automatically assigned
    //     index is not affected,
    //     e.g. in '{!}%d file{2:s|0||1|s} and %d director{y|1|ies}' the first parameter-
    //     dependent text uses parameter 2 and the second uses parameter 1
    //   - you can use any number of parameter-dependent texts with an explicitly specified
    //     parameter index
    //
    // examples of format strings:
    //   - '{!}director{y|1|ies}': for parameter values from 0 to 1 the resulting string is
    //     'directory', and for parameter values from 2 to infinity (2^64-1) it is
    //     'directories'
    //   - '{!}%d file{|1|s} and %d director{y|1|ies}': it needs two parameters because there
    //     are two dependent texts in curly brackets; for selected parameter pairs, for example:
    //       0, 0: '%d file and %d directory'
    //       1, 12: '%d file and %d directories'
    //       3, 4: '%d files and %d directories'
    //       13, 1: '%d files and %d directory'
    //
    // Method can be called from any thread.
    virtual int WINAPI ExpandPluralString(char* buffer, int bufferSize, const char* format,
                                          int parametersCount, const CQuadWord* parametersArray) = 0;

    // In the current Salamander language version, prepares the string 'XXX (selected/hidden)
    // files and YYY (selected/hidden) directories'; if XXX (the value of parameter 'files')
    // or YYY (the value of parameter 'dirs') is zero, the corresponding part of the string is omitted (both
    // parameters being zero at the same time is not considered); the use of 'selected' and 'hidden' depends
    // on mode 'mode' - see the epfdmXXX constants; the resulting text is returned in buffer 'buffer'
    // of size 'bufferSize' bytes; returns the length of the resulting text;
    // 'forDlgCaption' is TRUE/FALSE depending on whether the text is/is not intended for a dialog caption
    // (English requires initial capitals)
    // Can be called from any thread
    virtual int WINAPI ExpandPluralFilesDirs(char* buffer, int bufferSize, int files, int dirs,
                                             int mode, BOOL forDlgCaption) = 0;

    // In the current Salamander language version, prepares the string 'BBB bytes in XXX selected
    // files and YYY selected directories'; BBB is the value of parameter 'selectedBytes';
    // if XXX (the value of parameter 'files') or YYY (the value of parameter 'dirs') is zero,
    // the corresponding part of the string is omitted (both parameters being zero at the same
    // time is not considered); if 'useSubTexts' is TRUE, BBB is enclosed in '<' and '>' so it can be
    // processed further on the info line (see CSalamanderGeneralAbstract::LookForSubTexts and
    // CPluginDataInterfaceAbstract::GetInfoLineContent); the resulting text is returned in buffer
    // 'buffer' of size 'bufferSize' bytes; returns the length of the resulting text
    // Can be called from any thread
    virtual int WINAPI ExpandPluralBytesFilesDirs(char* buffer, int bufferSize,
                                                  const CQuadWord& selectedBytes, int files, int dirs,
                                                  BOOL useSubTexts) = 0;

    // Returns a string describing what the operation works with (e.g. 'file "test.txt"', 'directory "test"',
    // or '3 files and 1 directory'); 'sourceDescr' is the output buffer and must be at least
    // 'sourceDescrSize' bytes long; 'panel' describes the source panel of the operation (one of PANEL_XXX or -1
    // if the operation has no source panel, e.g. CPluginFSInterfaceAbstract::CopyOrMoveFromDiskToFS);
    // 'selectedFiles'+'selectedDirs' - if the operation has a source panel, this is the number of selected
    // files and directories in the source panel; if both values are zero, the operation works with the
    // file/directory under the cursor (focus); 'selectedFiles'+'selectedDirs' - if the operation has no
    // source panel, this is the number of files/directories the operation works with;
    // 'fileOrDirName'+'isDir' - used only if the operation has no source panel and if
    // 'selectedFiles + selectedDirs == 1'; they specify the file/directory name and whether it is a file
    // or a directory ('isDir' is FALSE or TRUE); 'forDlgCaption' is TRUE/FALSE depending on whether the
    // text is/is not intended for a dialog caption (English requires initial capitals)
    // Restriction: main thread (it can access the panel)
    virtual void WINAPI GetCommonFSOperSourceDescr(char* sourceDescr, int sourceDescrSize,
                                                   int panel, int selectedFiles, int selectedDirs,
                                                   const char* fileOrDirName, BOOL isDir,
                                                   BOOL forDlgCaption) = 0;

    // Copies string 'srcStr' after string 'dstStr' (after its terminating null);
    // 'dstStr' is a buffer of size 'dstBufSize' (it must be at least 2);
    // if both strings do not fit into the buffer, they are truncated (always so that
    // as many characters as possible from both strings fit)
    // Can be called from any thread
    virtual void WINAPI AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr) = 0;

    // Determines whether string 'fileNameComponent' can be used as a name component
    // on a Windows filesystem (handles strings longer than MAX_PATH-4 (4 = 'C:\' +
    // null terminator), empty strings, strings of '.' characters, strings of white-space characters,
    // and the characters '*', '?', '\\', '/', '<', '>', '|', '"', ':' as well as simple names such as 'prn' and 'prn  .txt')
    // Can be called from any thread
    virtual BOOL WINAPI SalIsValidFileNameComponent(const char* fileNameComponent) = 0;

    // Transforms string 'fileNameComponent' so it can be used as a file-name component
    // on the Windows file system (handles strings longer than MAX_PATH-4 (4 bytes for drive-root
    // plus null terminator), empty strings, strings consisting only of '.', strings of
    // white space, replaces characters such as *, ?, \\, /, <, >, |, ", and : with '_', and appends '_' to simple names such as "prn"
    // and "prn  .txt"); 'fileNameComponent' must be extensible by
    // at least one character (but at most MAX_PATH bytes of 'fileNameComponent'
    // are used)
    // Can be called from any thread
    virtual void WINAPI SalMakeValidFileNameComponent(char* fileNameComponent) = 0;

    // Returns TRUE if the enumeration source is a panel; 'panel' then receives PANEL_LEFT or
    // PANEL_RIGHT. If the enumeration source was not found or is a Find window, it returns FALSE;
    // 'srcUID' is the unique source identifier (passed as a parameter when opening the
    // viewer or obtainable by calling GetPanelEnumFilesParams)
    // Can be called from any thread
    virtual BOOL WINAPI IsFileEnumSourcePanel(int srcUID, int* panel) = 0;

    // Returns the next file name for the viewer from the source (left/right panel or Find);
    // 'srcUID' is the unique source identifier (passed as a parameter when opening the
    // viewer or obtainable by calling GetPanelEnumFilesParams); 'lastFileIndex'
    // (must not be NULL) is an IN/OUT parameter that the plugin should change only if it wants to return
    // the first file name, in which case it must set 'lastFileIndex' to -1; the initial
    // value of 'lastFileIndex' is passed both when opening the viewer and
    // when calling GetPanelEnumFilesParams; 'lastFileName' is the full name of the current file
    // (empty string if it is not known, for example if 'lastFileIndex' is -1); if
    // 'preferSelected' is TRUE and at least one name is selected, selected names are returned;
    // if 'onlyAssociatedExtensions' is TRUE, it returns only files whose extension is associated with
    // this plugin's viewer (F3 on such a file would try to open this plugin's viewer
    // and ignores any shadowing by another plugin's viewer); 'fileName' is the buffer
    // for the retrieved name (size at least MAX_PATH); returns TRUE if the name is obtained
    // successfully; returns FALSE on error: there is no next file name in the source (if
    // 'noMoreFiles' is not NULL, it receives TRUE), the source is busy (not processing messages;
    // if 'srcBusy' is not NULL, it receives TRUE), or the source no longer exists (panel path changed,
    // etc.);
    // Can be called from any thread; WARNING: calling it from the main thread is pointless
    // (Salamander is busy while calling the plugin method, so it always returns FALSE + TRUE
    // in 'srcBusy')
    virtual BOOL WINAPI GetNextFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                 BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                 char* fileName, BOOL* noMoreFiles, BOOL* srcBusy) = 0;

    // Returns the previous file name for the viewer from the source (left/right panel or Find);
    // 'srcUID' is the unique source identifier (passed as a parameter when opening the
    // viewer or obtainable by calling GetPanelEnumFilesParams); 'lastFileIndex' (must
    // not be NULL) is an IN/OUT parameter that the plugin should change only if it wants to return the
    // name of the last file, in which case it must set 'lastFileIndex' to -1; the initial value
    // of 'lastFileIndex' is passed both when opening the viewer and when calling
    // GetPanelEnumFilesParams; 'lastFileName' is the full name of the current file (an empty
    // string if it is not known, for example if 'lastFileIndex' is -1); if 'preferSelected'
    // is TRUE and at least one file is selected, selected file names are returned; if
    // 'onlyAssociatedExtensions' is TRUE, only files with an extension associated with
    // this plugin's viewer are returned (pressing F3 on such a file would try to open this plugin's viewer
    // and ignores any shadowing by another plugin's viewer); 'fileName' is the buffer
    // for the retrieved name (size at least MAX_PATH); returns TRUE if the name is obtained
    // successfully; returns FALSE on error: there is no previous file name in the source (if
    // 'noMoreFiles' is not NULL, it receives TRUE), the source is busy (not processing messages;
    // if 'srcBusy' is not NULL, it receives TRUE), or the source no longer exists (panel
    // path changed, etc.)
    // Can be called from any thread; WARNING: calling it from the main thread is pointless
    // (Salamander is busy while calling the plugin method, so it always returns FALSE + TRUE
    // in 'srcBusy')
    virtual BOOL WINAPI GetPreviousFileNameForViewer(int srcUID, int* lastFileIndex, const char* lastFileName,
                                                     BOOL preferSelected, BOOL onlyAssociatedExtensions,
                                                     char* fileName, BOOL* noMoreFiles, BOOL* srcBusy) = 0;

    // Determines whether the current file in the viewer is selected in the source (left/right
    // panel or Find); 'srcUID' is the unique source identifier (passed as a parameter
    // when opening the viewer or obtainable by calling GetPanelEnumFilesParams); 'lastFileIndex'
    // is a parameter that the plugin should not change; the initial value of 'lastFileIndex' is passed
    // both when opening the viewer and when calling GetPanelEnumFilesParams;
    // 'lastFileName' is the full name of the current file; returns TRUE if it was possible to determine
    // whether the current file is selected; the result is returned in 'isFileSelected' (must not be NULL);
    // returns FALSE on error: the source no longer exists (panel path changed, etc.) or file
    // 'lastFileName' is no longer in the source (for these two errors, if 'srcBusy' is not NULL,
    // it receives FALSE), or the source is busy (not processing messages; for this error,
    // if 'srcBusy' is not NULL, it receives TRUE)
    // Can be called from any thread; WARNING: calling it from the main thread is pointless
    // (Salamander is busy while calling the plugin method, so it always returns FALSE + TRUE
    // in 'srcBusy')
    virtual BOOL WINAPI IsFileNameForViewerSelected(int srcUID, int lastFileIndex,
                                                    const char* lastFileName,
                                                    BOOL* isFileSelected, BOOL* srcBusy) = 0;

    // Sets the selection state of the current file in the viewer in the source (left/right
    // panel or Find); 'srcUID' is the unique source identifier (passed as a parameter
    // when opening the viewer or obtainable by calling GetPanelEnumFilesParams);
    // 'lastFileIndex' is a parameter that the plugin should not change; the initial value
    // of 'lastFileIndex' is passed both when opening the viewer and when calling
    // GetPanelEnumFilesParams; 'lastFileName' is the full name of the current file; 'select'
    // is TRUE/FALSE depending on whether the current file should be selected/deselected; returns TRUE on success;
    // returns FALSE on error: the source no longer exists (panel path changed, etc.) or
    // file 'lastFileName' is no longer in the source (for these two errors, if 'srcBusy'
    // is not NULL, it receives FALSE), or the source is busy (not processing messages; for this
    // error, if 'srcBusy' is not NULL, it receives TRUE)
    // Can be called from any thread; WARNING: calling it from the main thread is pointless
    // (Salamander is busy while calling the plugin method, so it always returns FALSE + TRUE
    // in 'srcBusy')
    virtual BOOL WINAPI SetSelectionOnFileNameForViewer(int srcUID, int lastFileIndex,
                                                        const char* lastFileName, BOOL select,
                                                        BOOL* srcBusy) = 0;

    // Returns a reference to the shared history (last used values) of the selected combo box;
    // it is an array of allocated strings; the array has a fixed number of strings, which is returned
    // in 'historyItemsCount' (must not be NULL); a pointer to the array is returned in 'historyArr'
    // (must not be NULL); 'historyID' (one of SALHIST_XXX) specifies which shared history the reference should be returned to
    // point to
    // Restriction: main thread (shared histories cannot be used from another thread; access
    // to them is not synchronized in any way)
    virtual BOOL WINAPI GetStdHistoryValues(int historyID, char*** historyArr, int* historyItemsCount) = 0;

    // Adds an allocated copy of the new value 'value' to the shared history ('historyArr'+'historyItemsCount'); if 'caseSensitiveValue' is TRUE, the value
    // (string) is searched in the history array using a case-sensitive comparison
    // (FALSE = case-insensitive comparison),
    // if the value is found, it is only moved to the first position in the history array
    // Main thread only (shared histories cannot be used from another thread; access
    // to them is not synchronized in any way)
    // NOTE: if used for something other than shared histories, it can be called from any thread
    virtual void WINAPI AddValueToStdHistoryValues(char** historyArr, int historyItemsCount,
                                                   const char* value, BOOL caseSensitiveValue) = 0;

    // Adds strings from the shared history ('historyArr'+'historyItemsCount') to combo box 'combo';
    // before adding them, it resets the combo box contents (see CB_RESETCONTENT)
    // Main thread only (shared histories cannot be used from another thread; access
    // to them is not synchronized in any way)
    // NOTE: if used for something other than shared histories, it can be called from any thread
    virtual void WINAPI LoadComboFromStdHistoryValues(HWND combo, char** historyArr, int historyItemsCount) = 0;

    // Determines the color depth of the current display and returns TRUE if it is more than 8-bit (256 colors)
    // Can be called from any thread
    virtual BOOL WINAPI CanUse256ColorsBitmap() = 0;

    // Checks whether the enabled root parent of window 'parent' is the foreground window; if not,
    // calls FlashWindow(root parent of window 'parent', TRUE) and returns the root parent of window 'parent',
    // otherwise returns NULL
    // USAGE:
    //    HWND mainWnd = GetWndToFlash(parent);
    //    CDlg(parent).Execute();
    //    if (mainWnd != NULL) FlashWindow(mainWnd, FALSE);  // under W2K+ this is probably no longer needed: flashing must be cleared manually
    // Can be called from any thread
    virtual HWND WINAPI GetWndToFlash(HWND parent) = 0;

    // Reactivates the drop target (after a drop during drag&drop) after opening our progress
    // window (it becomes active when opened, which deactivates the drop target); if 'dropTarget'
    // is not NULL and is not a panel in this Salamander instance, it activates 'progressWnd' and then
    // activates the farthest enabled ancestor of 'dropTarget' (this combination clears the active
    // state without an active application, which otherwise sometimes occurs)
    // Can be called from any thread
    virtual void WINAPI ActivateDropTarget(HWND dropTarget, HWND progressWnd) = 0;

    // Schedules the Pack dialog to open with the selected packer from this plugin (see
    // CSalamanderConnectAbstract::AddCustomPacker); if that packer from this plugin
    // does not exist (for example because the user deleted it), an error message is shown to the user;
    // the dialog opens when the main thread message queue is empty
    // and Salamander is not "busy" (no modal dialog is open
    // and no message is being processed); repeated calls to this method before
    // the Pack dialog opens only change the 'delFilesAfterPacking' parameter;
    // 'delFilesAfterPacking' controls the "Delete files after packing" check box
    // in the Pack dialog: 0=default, 1=on, 2=off
    // Main thread only
    virtual void WINAPI PostOpenPackDlgForThisPlugin(int delFilesAfterPacking) = 0;

    // Schedules the Unpack dialog to open with the selected unpacker from this plugin (see
    // CSalamanderConnectAbstract::AddCustomUnpacker); if that unpacker from this plugin
    // does not exist (for example because the user deleted it), an error message is shown to the user;
    // the dialog opens when the main thread message queue is empty
    // and Salamander is not "busy" (no modal dialog is open
    // and no message is being processed); repeated calls to this method before
    // the Unpack dialog opens only change the 'unpackMask' parameter;
    // 'unpackMask' controls the "Unpack files" mask: NULL=default, otherwise the mask text
    // Main thread only
    virtual void WINAPI PostOpenUnpackDlgForThisPlugin(const char* unpackMask) = 0;

    // Creates a file named 'fileName' using the standard Win32 API
    // CreateFile (lpSecurityAttributes==NULL, dwCreationDisposition==CREATE_NEW,
    // hTemplateFile==NULL); this method resolves collisions between 'fileName' and the DOS name
    // of an existing file/directory (but only if there is no collision with the long
    // file/directory name as well) - it changes the DOS name so that the file named
    // 'fileName' can be created (it temporarily renames the conflicting
    // file/directory to another name and renames it back after creating 'fileName');
    // returns a file handle or, on error, INVALID_HANDLE_VALUE (returns the Windows error code
    // in 'err' if it is not NULL)
    // Can be called from any thread
    virtual HANDLE WINAPI SalCreateFileEx(const char* fileName, DWORD desiredAccess, DWORD shareMode,
                                          DWORD flagsAndAttributes, DWORD* err) = 0;

    // Creates a directory named 'name' using the standard Win32 API
    // CreateDirectory(lpSecurityAttributes==NULL); this method resolves collisions of 'name'
    // with the DOS name of an existing file/directory (but only if there is no
    // collision with the long file/directory name as well) - it changes the DOS name
    // so that the directory named 'name' can be created (it temporarily renames the
    // conflicting file/directory to another name and renames it back after creating 'name'); it also handles
    // names ending with spaces (it can create them, unlike CreateDirectory, which trims spaces without warning and thus creates a
    // different directory); returns TRUE on success, FALSE on error (returns the Windows error code
    // in 'err' if it is not NULL)
    // Can be called from any thread
    virtual BOOL WINAPI SalCreateDirectoryEx(const char* name, DWORD* err) = 0;

    // Allows you to disconnect/reconnect change monitoring (only for Windows paths and archive paths)
    // on paths currently displayed in one of the panels; purpose: if your code (formatting
    // a disk, shredding a disk, etc.) is obstructed because the panel has an open
    // "ChangeNotification" handle for the path, this method lets you disconnect it temporarily (after reconnecting,
    // a refresh is triggered for the panel path); 'panel' is one of PANEL_XXX; 'stopMonitoring'
    // is TRUE/FALSE (disconnect/reconnect)
    // Main thread only
    virtual void WINAPI PanelStopMonitoring(int panel, BOOL stopMonitoring) = 0;

    // Allocates a new CSalamanderDirectory object for working with files/directories in an archive or
    // file system; if 'isForFS' is TRUE, the object is preconfigured for file-system use,
    // otherwise it is preconfigured for archive use (the object's default flags differ
    // for archives and file systems; see CSalamanderDirectoryAbstract::SetFlags)
    // Can be called from any thread
    virtual CSalamanderDirectoryAbstract* WINAPI AllocSalamanderDirectory(BOOL isForFS) = 0;

    // Frees a CSalamanderDirectory object (obtained with AllocSalamanderDirectory,
    // WARNING: must not be called for any other CSalamanderDirectoryAbstract pointer)
    // Can be called from any thread
    virtual void WINAPI FreeSalamanderDirectory(CSalamanderDirectoryAbstract* salDir) = 0;

    // Adds a new timer for a plugin FS object; when the timer times out,
    // CPluginFSInterfaceAbstract::Event() is called on the plugin FS object 'timerOwner' with parameters
    // FSE_TIMER and 'timerParam'; 'timeout' is the timer timeout from the moment it is added (in milliseconds,
    // must be >= 0); the timer is canceled when it times out (before calling
    // CPluginFSInterfaceAbstract::Event()) or when the plugin FS object is closed;
    // returns TRUE if the timer was added successfully
    // Restriction: main thread
    virtual BOOL WINAPI AddPluginFSTimer(int timeout, CPluginFSInterfaceAbstract* timerOwner,
                                         DWORD timerParam) = 0;

    // Cancels either all timers of plugin FS object 'timerOwner' (if 'allTimers' is TRUE)
    // or only all timers whose parameter equals 'timerParam' (if 'allTimers' is FALSE);
    // returns the number of canceled timers
    // Main thread only
    virtual int WINAPI KillPluginFSTimer(CPluginFSInterfaceAbstract* timerOwner, BOOL allTimers,
                                         DWORD timerParam) = 0;

    // Determines the visibility of the FS item in the Change Drive menu and drive bars; returns TRUE,
    // if the item is visible, otherwise returns FALSE
    // Main thread only (otherwise plugin configuration may change during the call)
    virtual BOOL WINAPI GetChangeDriveMenuItemVisibility() = 0;

    // Sets the visibility of the FS item in the Change Drive menu and drive bars; use
    // only during plugin installation (otherwise the user-selected visibility may be overwritten);
    // 'visible' is TRUE if the item should be visible
    // Main thread only (otherwise plugin configuration may change during the call)
    virtual void WINAPI SetChangeDriveMenuItemVisibility(BOOL visible) = 0;

    // Sets a breakpoint on the x-th COM/OLE allocation. Used to locate COM/OLE leaks.
    // In the release version of Salamander it does nothing. The debug version of Salamander, when it exits,
    // displays a list of COM/OLE leaks in the debugger's Debug window and in Trace Server.
    // The allocation number shown in square brackets is passed as 'alloc' to
    // OleSpySetBreak. Can be called from any thread.
    virtual void WINAPI OleSpySetBreak(int alloc) = 0;

    // Returns copies of the icons used by Salamander in panels. 'icon' specifies the icon and is
    // one of the SALICON_xxx values. 'iconSize' specifies the size of the returned icon
    // and is one of the SALICONSIZE_xxx values.
    // On success, returns a handle to the created icon. The plugin must destroy the icon
    // by calling the DestroyIcon API. On failure, returns NULL.
    // Main thread only
    virtual HICON WINAPI GetSalamanderIcon(int icon, int iconSize) = 0;

    // GetFileIcon
    //   Retrieves a handle to a large or small icon for the specified object,
    //   such as a file, folder, directory, or drive root.
    //
    // Parameters
    //   'path'
    //      [in] Pointer to a null-terminated string containing the path and file
    //      name. If 'pathIsPIDL' is TRUE, this parameter must be the address of a
    //      fully qualified ITEMIDLIST (PIDL) structure containing the item identifiers
    //      that uniquely identify the file in the Shell namespace. Relative PIDLs are
    //      not allowed.
    //
    //   'pathIsPIDL'
    //      [in] Indicates that 'path' is the address of an ITEMIDLIST structure rather
    //      than a path name.
    //
    //   'hIcon'
    //      [out] Pointer to a handle that receives the icon extracted from the object.
    //
    //   'iconSize'
    //      [in] Required icon size. One of the SALICONSIZE_xxx values.
    //
    //   'fallbackToDefIcon'
    //      [in] Specifies whether to use the default (simple) icon if the specified
    //      object's icon is not available. If this parameter is TRUE, the function
    //      tries to return the default icon in that case. Otherwise it returns no icon
    //      (the return value is FALSE).
    //
    //   'defIconIsDir'
    //      [in] Specifies whether the default (simple) icon for 'path' is a directory
    //      icon. This parameter is ignored unless 'fallbackToDefIcon' is TRUE.
    //
    // Return Values
    //   Returns TRUE on success, or FALSE otherwise.
    //
    // Remarks
    //   You are responsible for freeing returned icons with DestroyIcon when they are
    //   no longer needed.
    //
    //   You must initialize COM with CoInitialize or OLEInitialize before calling
    //   GetFileIcon.
    //
    //   Can be called from any thread.
    virtual BOOL WINAPI GetFileIcon(const char* path, BOOL pathIsPIDL,
                                    HICON* hIcon, int iconSize, BOOL fallbackToDefIcon,
                                    BOOL defIconIsDir) = 0;

    // FileExists
    //   Checks whether a file exists. Returns TRUE if the specified file exists.
    //   If the file does not exist, it returns FALSE. FileExists checks only files;
    //   directories are ignored.
    // Can be called from any thread
    virtual BOOL WINAPI FileExists(const char* fileName) = 0;

    // Changes the panel path to the last known disk path; if it is not accessible,
    // it changes to the user-selected "rescue" path (see
    // SALCFG_IFPATHISINACCESSIBLEGOTO), and if that also fails, to the root of the first local
    // fixed drive (Salamander 2.5 and 2.51 only change to the root of the first local fixed drive);
    // used to close the file system in a panel (disconnect); 'parent' is the parent of any
    // message boxes; 'panel' is one of PANEL_XXX
    // Main thread only, and not from CPluginFSInterfaceAbstract or CPluginDataInterfaceAbstract methods
    // (for example, the FS open in the panel may be closed, so the method's 'this' pointer may cease to be valid)
    virtual void WINAPI DisconnectFSFromPanel(HWND parent, int panel) = 0;

    // Returns TRUE if the file name 'name' is associated with the calling plugin in Archives Associations in Panels
    // 'name' must contain only the file name, not a full or relative path
    // Main thread only
    virtual BOOL WINAPI IsArchiveHandledByThisPlugin(const char* name) = 0;

    // Used as the LR_xxx parameter for the LoadImage() API function;
    // if the user does not have hi-color icons enabled in the desktop configuration,
    // returns LR_VGACOLOR to prevent incorrect loading of the high-color icon version
    // otherwise returns 0 (LR_DEFAULTCOLOR); the result can be ORed with other LR_xxx flags
    // Can be called from any thread
    virtual DWORD WINAPI GetIconLRFlags() = 0;

    // Determines from the file extension whether it is a link ("lnk", "pif" or "url"); 'fileExtension'
    // is the file extension (pointer after the dot), must not be NULL; returns 1 if it is a link, otherwise
    // returns 0; NOTE: used to fill CFileData::IsLink
    // Can be called from any thread
    virtual int WINAPI IsFileLink(const char* fileExtension) = 0;

    // Returns ILC_COLOR??? according to the Windows version - tuned for use with image lists in list views
    // Typical use: ImageList_Create(16, 16, ILC_MASK | GetImageListColorFlags(), ???, ???)
    // Can be called from any thread
    virtual DWORD WINAPI GetImageListColorFlags() = 0;

    // "Safe" version of GetOpenFileName()/GetSaveFileName() that handles the case when the path
    // in OPENFILENAME::lpstrFile is invalid (for example z:\); in that case the standard API version of the
    // function does not open the dialog, silently returns FALSE, and CommDlgExtendedError() returns FNERR_INVALIDFILENAME.
    // In this case, the following two functions call the API once more, but with a "safe"
    // existing path (Documents or, if needed, Desktop).
    virtual BOOL WINAPI SafeGetOpenFileName(LPOPENFILENAME lpofn) = 0;
    virtual BOOL WINAPI SafeGetSaveFileName(LPOPENFILENAME lpofn) = 0;

    // The plugin must provide Salamander with the name of its .chm file
    // without a path (for example, "demoplug.chm") before using OpenHtmlHelp()
    // Can be called from any thread, but concurrent calls with OpenHtmlHelp() must be excluded
    virtual void WINAPI SetHelpFileName(const char* chmName) = 0;

    // Opens the plugin's HTML help; the help language (directory with .chm files) is chosen as follows:
    // -directory obtained from Salamander's current .slg file (see SLGHelpDir in shared\versinfo.rc)
    // -HELP\ENGLISH\*.chm
    // -the first subdirectory found under HELP
    // The plugin must call SetHelpFileName() before using OpenHtmlHelp(); 'parent' is the parent for error
    // message boxes; 'command' is the HTML help command, see HHCDisplayXXX; 'dwData' is the parameter
    // of the HTML help command, see HHCDisplayXXX
    // Can be called from any thread
    // NOTE: for Salamander help display, see OpenHtmlHelpForSalamander
    virtual BOOL WINAPI OpenHtmlHelp(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData,
                                     BOOL quiet) = 0;

    // Returns TRUE if paths 'path1' and 'path2' are on the same volume; 'resIsOnlyEstimation'
    // (if not NULL) receives TRUE if the result is not certain (it is certain only when the paths match or
    // when the "volume name" (volume GUID) can be obtained for both paths, which is possible only for
    // local paths on W2K or newer NT-based systems)
    // Can be called from any thread
    virtual BOOL WINAPI PathsAreOnTheSameVolume(const char* path1, const char* path2,
                                                BOOL* resIsOnlyEstimation) = 0;

    // Reallocates memory on the Salamander heap (unnecessary when using salrtl9.dll; plain realloc is enough);
    // on out of memory, shows the user a dialog with Retry and Cancel buttons (on the next out-of-memory prompt,
    // it terminates the application)
    // Can be called from any thread
    virtual void* WINAPI Realloc(void* ptr, int size) = 0;

    // Returns in 'enumFilesSourceUID' (must not be NULL) the unique source identifier for panel
    // 'panel' (one of PANEL_XXX); it is used in viewers when enumerating files
    // from a panel (see the 'srcUID' parameter, for example, in GetNextFileNameForViewer); this
    // identifier changes, for example, when the panel path changes; if 'enumFilesCurrentIndex'
    // is not NULL, it receives the index of the focused file (or -1 if there is no focused file);
    // Main thread only (otherwise the panel contents may change)
    virtual void WINAPI GetPanelEnumFilesParams(int panel, int* enumFilesSourceUID,
                                                int* enumFilesCurrentIndex) = 0;

    // Posts a message to the panel with active FS 'modifiedFS' that it should
    // refresh the path (reloads the listing and transfers selection, icons, focus, etc. to the
    // new panel contents); the refresh is performed when the Salamander main window becomes active
    // (when suspend mode ends); the FS path is always reloaded; if 'modifiedFS' is not in any
    // panel, nothing is done; if 'focusFirstNewItem' is TRUE and exactly one new
    // item was added to the panel, that new item is focused (used, for example, to focus a newly created
    // file/directory); returns TRUE if the refresh was performed, FALSE if 'modifiedFS'
    // was not found in any panel
    // Can be called from any thread (if the main thread is not running code inside the plugin,
    // the refresh is performed as soon as possible; otherwise it waits at least until the main
    // thread leaves the plugin)
    virtual BOOL WINAPI PostRefreshPanelFS2(CPluginFSInterfaceAbstract* modifiedFS,
                                            BOOL focusFirstNewItem = FALSE) = 0;

    // Loads the text with ID 'resID' from module 'module'; returns the text in an internal buffer (the text may change
    // because the internal buffer can be reused by subsequent LoadStr calls from other
    // plugins or Salamander; the buffer is 10000 characters long, so overwrite is only possible after it
    // fills up
    // (it is used cyclically); if you need the text later, we recommend
    // copying it to a local buffer); if 'module' is NULL or 'resID' is not present in the module,
    // it returns the text "ERROR LOADING STRING" (and the debug/SDK version outputs TRACE_E)
    // Can be called from any thread
    virtual char* WINAPI LoadStr(HINSTANCE module, int resID) = 0;

    // Loads the text with ID 'resID' from the resources of module 'module'; returns the text in an
    // internal buffer (the text may change when later LoadStrW calls from other plugins or Salamander
    // reuse the internal buffer; the buffer holds 10000 characters and is used cyclically, so the text
    // is overwritten only after it fills up; if you need the text later, copy it to a local buffer).
    // If 'module' is NULL or 'resID' is not present in the module, returns
    // L"ERROR LOADING WIDE STRING" (and the debug/SDK build outputs TRACE_E).
    // Can be called from any thread.
    virtual WCHAR* WINAPI LoadStrW(HINSTANCE module, int resID) = 0;

    // Changes the panel path to the user-selected "rescue" path (see
    // SALCFG_IFPATHISINACCESSIBLEGOTO), and if that also fails, to the root of the first local fixed
    // drive; this is very likely to change the current path in the panel. 'panel' is one of PANEL_XXX.
    // If 'failReason' is not NULL, it is set to one of the CHPPFR_XXX constants (indicating the method
    // result). Returns TRUE if the path change succeeds (to the "rescue" path or the fixed drive).
    // Restriction: main thread only, and not from CPluginFSInterfaceAbstract or CPluginDataInterfaceAbstract
    // methods (for example, the FS opened in the panel could be closed, and 'this' could stop existing).
    virtual BOOL WINAPI ChangePanelPathToRescuePathOrFixedDrive(int panel, int* failReason = NULL) = 0;

    // Registers the plugin as a replacement for the Network item in the Change Drive menu and in the drive bars.
    // The plugin must add a file system to Salamander that is then used to open incomplete UNC paths
    // (the root-only and server-only forms) from the Change Directory command and that is entered
    // via the up-dir symbol ("..") from the root of UNC paths.
    // Limitation: call only from the plugin entry point and only after SetBasicPluginData.
    virtual void WINAPI SetPluginIsNethood() = 0;

    // Opens the system context menu for selected items or the focused item on a network path
    // (if 'forItems' is TRUE), or for a network path itself (if 'forItems' is FALSE); it also executes the
    // selected command from the menu. The menu is obtained by browsing the CSIDL_NETWORK folder. 'parent' is the
    // proposed parent window of the context menu. 'panel' identifies the panel (PANEL_LEFT or PANEL_RIGHT) for
    // which the context menu should be opened; the focused/selected files or directories are taken from this panel.
    // 'menuX' and 'menuY' are the proposed coordinates of the upper-left corner of the context menu. 'netPath' is
    // the network path; only "\\" and "\\server" are allowed. If 'newlyMappedDrive' is not NULL, it returns
    // the letter ('A' to 'Z') of the newly mapped drive (via the Map Network Drive command from the context menu);
    // if zero is returned there, no new mapping was created.
    // Limitation: main thread
    virtual void WINAPI OpenNetworkContextMenu(HWND parent, int panel, BOOL forItems, int menuX,
                                               int menuY, const char* netPath,
                                               char* newlyMappedDrive) = 0;

    // Duplicates backslashes; useful for texts sent to LookForSubTexts(), which then reduces doubled
    // backslashes again. 'buffer' is the input/output string and 'bufferSize' is the size of 'buffer'
    // in bytes. Returns TRUE if doubling the backslashes did not truncate any characters from the end of
    // the string (the buffer was large enough).
    // Can be called from any thread.
    virtual BOOL WINAPI DuplicateBackslashes(char* buffer, int bufferSize) = 0;

    // Shows a throbber in panel 'panel' (an animation that informs the user about activity related
    // to the panel, for example "loading data from the network") after a delay of 'delay' ms; 'panel'
    // is one of PANEL_XXX. If 'tooltip' is not NULL, it is the text shown when the mouse hovers over
    // the throbber (if it is NULL, no text is shown). If a throbber is already shown in the panel or
    // is waiting to be shown, its identifier and tooltip are changed (if it is already shown, 'delay'
    // is ignored; if it is still waiting to be shown, the new delay from 'delay' is used). Returns the
    // throbber identifier (never -1, so -1 can be used as an empty value; all returned identifiers are
    // unique, more precisely they would repeat only after an unrealistic 2^32 throbber displays).
    // NOTE: a suitable place to show the throbber for an FS is when handling FSE_PATHCHANGED; at that point
    // the FS is already in the panel (whether the throbber should be shown can be decided in advance in
    // ChangePath or ListCurrentPath).
    // limitation: main thread
    virtual int WINAPI StartThrobber(int panel, const char* tooltip, int delay) = 0;

    // Hides the throbber with identifier 'id'; returns TRUE if the throbber is hidden.
    // Returns FALSE if this throbber has already been hidden or another throbber has been shown over it.
    // NOTE: the throbber is automatically hidden immediately before the panel path changes or
    // before a refresh (for an FS, this means immediately after a successful ListCurrentPath call; for an archive
    // after the archive is opened and listed; for a drive after the path accessibility is verified).
    // Limitation: main thread
    virtual BOOL WINAPI StopThrobber(int id) = 0;

    // Shows a security icon in panel 'panel' (a locked or unlocked padlock, for example FTPS uses it to inform
    // the user that the connection to the server is secured with SSL and that the server identity is either
    // verified (locked padlock) or not verified (unlocked padlock)); 'panel' is one of PANEL_XXX;
    // if 'showIcon' is TRUE, the icon is shown, otherwise it is hidden; 'isLocked' determines whether
    // the padlock is locked (TRUE) or unlocked (FALSE); if 'tooltip' is not NULL, it is the text shown
    // when the mouse hovers over the icon (if it is NULL, no text is shown); if clicking the security icon should
    // perform an action (for example, FTPS displays a server certificate dialog), it must be added to the
    // file-system's CPluginFSInterfaceAbstract::ShowSecurityInfo method
    // shown for the file system displayed in the panel;
    // NOTE: a suitable place to show the security icon for an FS is when handling
    // FSE_PATHCHANGED, once the FS is already in the panel (whether the icon should be shown or hidden can be determined
    // in ChangePath or ListCurrentPath)
    // NOTE: the security icon is automatically hidden immediately before the panel path changes or
    // before a refresh (for an FS, this means immediately after a successful ListCurrentPath call; for archives,
    // after the archive is opened and listed; for drives, after the path accessibility is verified)
    // limitation: main thread
    virtual void WINAPI ShowSecurityIcon(int panel, BOOL showIcon, BOOL isLocked,
                                         const char* tooltip) = 0;

    // Removes the current panel path from the history of directories shown in the panel (Alt+Left/Right)
    // and from the working directory list (Alt+F12). It is used to hide transient paths; for example,
    // "net:\Entire Network\Microsoft Windows Network\WORKGROUP\server\share" automatically changes to
    // "\\server\share", and it is undesirable for this transition to occur while moving through the history.
    // Limitation: main thread
    virtual void WINAPI RemoveCurrentPathFromHistory(int panel) = 0;

    // Returns TRUE if the current user is a member of the Administrators group, otherwise FALSE.
    // Can be called from any thread.
    virtual BOOL WINAPI IsUserAdmin() = 0;

    // Returns TRUE if Salamander is running in a Remote Desktop session, otherwise FALSE.
    // Can be called from any thread.
    virtual BOOL WINAPI IsRemoteSession() = 0;

    // Equivalent to calling WNetAddConnection2(lpNetResource, NULL, NULL, CONNECT_INTERACTIVE).
    // The advantage is more detailed display of error states (for example, an expired password,
    // an incorrect password or user name, or a required password change).
    // Can be called from any thread.
    virtual DWORD WINAPI SalWNetAddConnection2Interactive(LPNETRESOURCE lpNetResource) = 0;

    //
    // GetMouseWheelScrollChars
    //   An OS independent method to retrieve the number of wheel scroll chars.
    //
    // Return Values
    //   Number of scroll characters where WHEEL_PAGESCROLL (0xffffffff) indicates to scroll a page at a time.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual DWORD WINAPI GetMouseWheelScrollChars() = 0;

    //
    // GetSalamanderZLIB
    //   Provides simplified interface to the ZLIB library provided by Salamander,
    //   for details see spl_zlib.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderZLIBAbstract* WINAPI GetSalamanderZLIB() = 0;

    //
    // GetSalamanderPNG
    //   Provides interface to the PNG library provided by Salamander.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderPNGAbstract* WINAPI GetSalamanderPNG() = 0;

    //
    // GetSalamanderCrypt
    //   Provides interface to encryption libraries provided by Salamander,
    //   for details see spl_crypt.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderCryptAbstract* WINAPI GetSalamanderCrypt() = 0;

    // Informs Salamander that the plugin uses the Password Manager, so Salamander should notify
    // the plugin about creation, modification, or removal of the master password (see
    // CPluginInterfaceAbstract::PasswordManagerEvent).
    // Limitation: call only from the plugin entry point and only after SetBasicPluginData.
    virtual void WINAPI SetPluginUsesPasswordManager() = 0;

    //
    // GetSalamanderPasswordManager
    //   Provides interface to Password Manager provided by Salamander.
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual CSalamanderPasswordManagerAbstract* WINAPI GetSalamanderPasswordManager() = 0;

    // Opens Salamander's own HTML help (instead of the plugin help opened by OpenHtmlHelp()).
    // The help language (the directory with the .chm files) is selected in this order:
    // - the directory obtained from Salamander's current .slg file (see SLGHelpDir in shared\versinfo.rc)
    // - HELP\ENGLISH\*.chm
    // - the first subdirectory found under HELP
    // 'parent' is the parent of the error message box; 'command' is the HTML Help command, see HHCDisplayXXX.
    // 'dwData' is the HTML Help command parameter, see HHCDisplayXXX; if command == HHCDisplayContext,
    // 'dwData' must be a value from the HTMLHELP_SALID_XXX family of constants.
    // Can be called from any thread.
    virtual BOOL WINAPI OpenHtmlHelpForSalamander(HWND parent, CHtmlHelpCommand command, DWORD_PTR dwData, BOOL quiet) = 0;

    //
    // GetSalamanderBZIP2
    //   Provides simplified interface to the BZIP2 library provided by Salamander,
    //   for details see spl_bzip2.h.
    //
    // Remarks
    //   Method can be called from any thread.
    virtual CSalamanderBZIP2Abstract* WINAPI GetSalamanderBZIP2() = 0;

    //
    // GetFocusedItemMenuPos
    //   Returns point (in screen coordinates) where the context menu for focused item in the
    //   active panel should be displayed. The upper left corner of the panel is returned when
    //   focused item is not visible
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual void WINAPI GetFocusedItemMenuPos(POINT* pos) = 0;

    //
    // LockMainWindow
    //   Locks main window to pretend it is disabled. Main windows is still able to receive focus
    //   in the locked state. Set 'lock' to TRUE to lock main window and to FALSE to revert it back
    //   to normal state. 'hToolWnd' is reserverd parameter, set it to NULL. 'lockReason' is (optional,
    //   can be NULL) describes the reason for main window locked state. It will be displayed during
    //   attempt to close locked main window; content of string is copied to internal structure
    //   so buffer can be deallocated after return from LockMainWindow().
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual void WINAPI LockMainWindow(BOOL lock, HWND hToolWnd, const char* lockReason) = 0;

    // Only for "dynamic menu extension" plugins (see FUNCTION_DYNAMICMENUEXT):
    // sets a flag for the calling plugin that causes the menu to be rebuilt at the next possible opportunity
    // (as soon as the main thread message queue is empty and Salamander is not "busy"
    // (no modal dialog is open and no message is being processed))
    // by calling CPluginInterfaceForMenuExtAbstract::BuildMenu().
    // WARNING: if called from a thread other than the main thread, BuildMenu
    // (which runs in the main thread) may be called even before PostPluginMenuChanged returns.
    // Can be called from any thread.
    virtual void WINAPI PostPluginMenuChanged() = 0;

    // GetMenuItemHotKey
    //   Searches the plugin's menu items added with AddMenuItem() for the item with 'id'.
    //   If found, stores its hot key in 'hotKey' and its text form (up to 'hotKeyTextSize' characters)
    //   in 'hotKeyText'. Both 'hotKey' and 'hotKeyText' may be NULL.
    //   Returns TRUE if an item with 'id' is found; otherwise returns FALSE.
    //
    // Remarks
    //   Method can be called from main thread only.
    virtual BOOL WINAPI GetMenuItemHotKey(int id, WORD* hotKey, char* hotKeyText, int hotKeyTextSize) = 0;

    // Our variants of RegQueryValue and RegQueryValueEx, unlike the API variants,
    // ensure that a null terminator is added for REG_SZ, REG_MULTI_SZ, and REG_EXPAND_SZ values.
    // WARNING: when querying the required buffer size, they return one or two extra characters
    //          (two only for REG_MULTI_SZ) in case the string needs to be terminated with one or two nulls.
    // Can be called from any thread.
    virtual LONG WINAPI SalRegQueryValue(HKEY hKey, LPCSTR lpSubKey, LPSTR lpData, PLONG lpcbData) = 0;
    virtual LONG WINAPI SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                                           LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) = 0;

    // Because the Windows version of GetFileAttributes cannot work with names ending in a space,
    // we wrote our own version (for such names it appends a backslash, after which
    // GetFileAttributes works correctly, but only for directories; for files with a trailing space
    // we have no solution, but at least it no longer reads attributes from a different file - the Windows version
    // trims the spaces and thus works with a different file or directory).
    // Can be called from any thread.
    virtual DWORD WINAPI SalGetFileAttributes(const char* fileName) = 0;

    // There is currently no Win32 API for detecting SSDs, so detection is done heuristically
    // based on support for TRIM, StorageDeviceSeekPenaltyProperty, and so on.
    // Returns TRUE if the disk for path 'path' appears to be an SSD; otherwise FALSE.
    // The result is not 100% reliable; users report failures of the algorithm, for example on SSD PCIe cards:
    // http://stackoverflow.com/questions/23363115/detecting-ssd-in-windows/33359142#33359142
    // It can determine correct information even for paths containing SUBSTs and reparse points under Windows
    // 2000/XP/Vista (Salamander 2.5 works only with junction points). 'path' is the path for which the
    // information is requested; if the path goes through a network path, it silently returns FALSE.
    // Can be called from any thread.
    virtual BOOL WINAPI IsPathOnSSD(const char* path) = 0;

    // Returns TRUE if this is a UNC path (detects both formats: \\server\share and \\?\UNC\server\share).
    // Can be called from any thread.
    virtual BOOL WINAPI IsUNCPath(const char* path) = 0;

    // Replaces SUBST drive letters in path 'resPath' with their target paths (converts it to a path without SUBST drive letters).
    // 'resPath' must point to a buffer of at least MAX_PATH characters.
    // Returns TRUE on success, FALSE on error.
    // Can be called from any thread.
    virtual BOOL WINAPI ResolveSubsts(char* resPath) = 0;

    // Call only for paths 'path' whose root (after resolving SUBSTs) is DRIVE_FIXED; it makes no sense
    // to search for reparse points elsewhere. The function looks for a path without reparse points that
    // leads to the same volume as 'path'. For a path containing a symlink that leads to a network path
    // (UNC or mapped), only the root of that network path is returned. If no such path exists because the
    // current (last) local reparse point is a volume mount point (or an unknown reparse-point type), the
    // path to that volume mount point (or unknown reparse point) is returned. If the path contains more
    // than 50 reparse points (most likely an infinite loop), the original path is returned.
    //
    //'resPath' is the output buffer of size MAX_PATH; 'path' is the original path; 'cutResPathIsPossible'
    // (must not be NULL) is set to FALSE if the resulting path in 'resPath' ends with a reparse point
    // (a volume mount point or an unknown reparse-point type) and therefore must not be shortened.
    // If 'rootOrCurReparsePointSet' is non-NULL and contains FALSE, and the original path contains at least
    // one local reparse point (reparse points in the network part of the path are ignored), it is set to TRUE
    // and 'rootOrCurReparsePoint' (if not NULL) receives the full path to the current (last) local reparse
    // point (note: not the path it leads to). The target path of the current reparse point, but only for a
    // junction or symlink, is returned in 'junctionOrSymlinkTgt' (if not NULL), and its type is returned in
    // 'linkType': 2 (JUNCTION POINT), 3 (SYMBOLIC LINK). 'netPath' (if not NULL) receives the network path
    // targeted by the current (last) local symlink in the path; in that case the root of the network path is
    // returned in 'resPath'.
    // Can be called from any thread.
    virtual void WINAPI ResolveLocalPathWithReparsePoints(char* resPath, const char* path,
                                                          BOOL* cutResPathIsPossible,
                                                          BOOL* rootOrCurReparsePointSet,
                                                          char* rootOrCurReparsePoint,
                                                          char* junctionOrSymlinkTgt, int* linkType,
                                                          char* netPath) = 0;

    // Resolves SUBSTs and reparse points in path 'path', then tries to obtain a GUID path for the path's mount point
    // (or for the path root if no mount point is present). On failure returns FALSE. On success returns TRUE and sets
    // 'mountPoint' and 'guidPath' (if they are not NULL, they must point to buffers of at least MAX_PATH; the strings
    // will be terminated with a backslash).
    // Can be called from any thread.
    virtual BOOL WINAPI GetResolvedPathMountPointAndGUID(const char* path, char* mountPoint, char* guidPath) = 0;

    // Replaces the last '.' character in the string with the decimal separator from the system LOCALE_SDECIMAL setting.
    // The string may grow because according to MSDN the separator can be up to 4 characters long.
    // Returns TRUE if the buffer was large enough and the operation completed successfully, otherwise FALSE.
    // Can be called from any thread.
    virtual BOOL WINAPI PointToLocalDecimalSeparator(char* buffer, int bufferSize) = 0;

    // Sets the icon-overlays array for this plugin; once set, the plugin can return in file listings the
    // icon-overlay index (see CFileData::IconOverlayIndex) that should be displayed over the item's
    // icon; up to 15 icon-overlays can be used this way (indexes 0 through 14, because
    // index 15=ICONOVERLAYINDEX_NOTUSED means: do not display an icon-overlay); 'iconOverlaysCount'
    // is the number of icon-overlays for the plugin; the 'iconOverlays' array contains, for each icon-overlay,
    // all icon sizes in order: SALICONSIZE_16, SALICONSIZE_32, and SALICONSIZE_48 - that is,
    // the 'iconOverlays' array contains 3 * 'iconOverlaysCount' icons; Salamander releases the icons in
    // 'iconOverlays' (by calling DestroyIcon()), but the array itself belongs to the caller; if the array contains
    // any NULL entries (for example, an icon failed to load), the function fails but still releases the
    // valid icons from the array; when system colors change, the plugin should reload the icon-overlays and
    // set them again with this function; the ideal reaction is to handle PLUGINEVENT_COLORSCHANGED in
    // CPluginInterfaceAbstract::Event()
    // POZOR: pred Windows XP (ve W2K) je velikost ikony SALICONSIZE_48 jen 32 bodu!
    // limitation: main thread
    virtual void WINAPI SetPluginIconOverlays(int iconOverlaysCount, HICON* iconOverlays) = 0;

    // For a description, see SalGetFileSize(); the first difference is that the file is specified
    // by a full path; the second is that 'err' may be NULL if the error code is not needed;
    virtual BOOL WINAPI SalGetFileSize2(const char* fileName, CQuadWord& size, DWORD* err) = 0;

    // Determines the size of the file targeted by symlink 'fileName'; returns the size in 'size'.
    // 'ignoreAll' is in/out: if it is TRUE, all errors are ignored (before the operation it should
    // be set to FALSE, otherwise the error dialog is never shown; do not change it afterwards).
    // On error, shows a standard Retry / Ignore / Ignore All / Cancel dialog with parent 'parent';
    // if the size is obtained successfully, returns TRUE. On error and Ignore / Ignore All,
    // returns FALSE and sets 'cancel' to FALSE. If 'ignoreAll' is TRUE, the dialog is not shown,
    // no button is waited for, and it behaves as if the user pressed Ignore. On error and Cancel,
    // returns FALSE and sets 'cancel' to TRUE.
    // Can be called from any thread.
    virtual BOOL WINAPI GetLinkTgtFileSize(HWND parent, const char* fileName, CQuadWord* size,
                                           BOOL* cancel, BOOL* ignoreAll) = 0;

    // Deletes a directory link (junction point, symbolic link, mount point); on success
    // returns TRUE. On error returns FALSE and, if 'err' is not NULL, stores the error code in 'err'.
    // Can be called from any thread.
    virtual BOOL WINAPI DeleteDirLink(const char* name, DWORD* err) = 0;

    // If file/directory 'name' has the read-only attribute, attempts to clear it
    // (for example, so it can be deleted with DeleteFile). If the attributes of 'name'
    // are already known, pass them in 'attr'; if 'attr' is -1, the attributes of 'name' are read from disk.
    // Returns TRUE if an attempt to change the attribute is made (success is not checked).
    // NOTE: only the read-only attribute is cleared, so that when a file has multiple hard links the remaining
    // hard links are not affected by an unnecessarily large attribute change (all hard links share attributes).
    // Can be called from any thread.
    virtual BOOL WINAPI ClearReadOnlyAttr(const char* name, DWORD attr = -1) = 0;

    // Determines whether a critical shutdown (or logoff) is currently in progress; if so, returns TRUE.
    // During this type of shutdown, only 5 seconds are available to save the configuration of the entire application,
    // including plugins, so more time-consuming operations must be skipped. After 5 seconds, the system
    // forcibly terminates the process. For more information, see WM_ENDSESSION and the ENDSESSION_CRITICAL flag.
    // Vista+
    virtual BOOL WINAPI IsCriticalShutdown() = 0;

    // Enumerates all windows in thread 'tid' (0 = current) via EnumThreadWindows and posts WM_CLOSE to all enabled
    // and visible dialogs (class name "#32770") owned by window 'parent'. It is used during critical shutdown
    // to unblock a window or dialog over which modal dialogs are open; if there are multiple layers, it must be called repeatedly.
    virtual void WINAPI CloseAllOwnedEnabledDialogs(HWND parent, DWORD tid = 0) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_gen)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
