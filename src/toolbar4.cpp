// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "bitmap.h"
#include "toolbar.h"
#include "mainwnd.h"
//#include "usermenu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"

#include "nanosvg\nanosvg.h"
#include "nanosvg\nanosvgrast.h"
#include "svg.h"

struct CButtonData
{
    unsigned int ImageIndex : 16;   // zero base index
    unsigned int Shell32ResID : 8;  // 0: icon from image list; 1..254: icon resID from shell32.dll; 255: only allocate free space
    unsigned int ToolTipResID : 16; // resID with a string for the tooltip
    unsigned int ID : 16;           // universal Command
    unsigned int LeftID : 16;       // Command for the left panel
    unsigned int RightID : 16;      // Command for the right panel
    unsigned int DropDown : 1;      // will have drop down?
    unsigned int WholeDropDown : 1; // will have whole drop down?
    unsigned int Check : 1;         // Is this a checkbox?
    DWORD* Enabler;                 // Control variable for enabling the button
    DWORD* LeftEnabler;             // Control variable for enabling the button
    DWORD* RightEnabler;            // Control variable for enabling the button
    const char* SVGName;            // NULL if the button does not have an SVG representation
};

//****************************************************************************
//
// TBButtonEnum
//
// Unique indexes into the ToolBarButton array - used to address this array.
// Elements can only be added to the end of this array.
//

#define TBBE_CONNECT_NET 0
#define TBBE_DISCONNECT_NET 1
#define TBBE_CREATE_DIR 2
#define TBBE_FIND_FILE 3
#define TBBE_VIEW_MODE 4 // drive brief
//#define TBBE_DETAILED            5  // excluded
#define TBBE_SORT_NAME 6
#define TBBE_SORT_EXT 7
#define TBBE_SORT_SIZE 8
#define TBBE_SORT_DATE 9
//#define TBBE_SORT_ATTR          10  // excluded
#define TBBE_PARENT_DIR 11
#define TBBE_ROOT_DIR 12
#define TBBE_FILTER 13
#define TBBE_BACK 14
#define TBBE_FORWARD 15
#define TBBE_REFRESH 16
#define TBBE_SWAP_PANELS 17
#define TBBE_PROPERTIES 18
#define TBBE_USER_MENU_DROP 19
#define TBBE_CMD 20
#define TBBE_COPY 21
#define TBBE_MOVE 22
#define TBBE_DELETE 23
#define TBBE_COMPRESS 24
#define TBBE_UNCOMPRESS 25
#define TBBE_QUICK_RENAME 26
#define TBBE_CHANGE_CASE 27
#define TBBE_VIEW 28
#define TBBE_EDIT 29
#define TBBE_CLIPBOARD_CUT 30
#define TBBE_CLIPBOARD_COPY 31
#define TBBE_CLIPBOARD_PASTE 32
#define TBBE_CHANGE_ATTR 33
#define TBBE_COMPARE_DIR 34
#define TBBE_DRIVE_INFO 35
#define TBBE_CHANGE_DRIVE_L 36
#define TBBE_SELECT 37
#define TBBE_UNSELECT 38
#define TBBE_INVERT_SEL 39
#define TBBE_SELECT_ALL 40
#define TBBE_PACK 41
#define TBBE_UNPACK 42
#define TBBE_OPEN_ACTIVE 43
#define TBBE_OPEN_DESKTOP 44
#define TBBE_OPEN_MYCOMP 45
#define TBBE_OPEN_CONTROL 46
#define TBBE_OPEN_PRINTERS 47
#define TBBE_OPEN_NETWORK 48
#define TBBE_OPEN_RECYCLE 49
#define TBBE_OPEN_FONTS 50
#define TBBE_CHANGE_DRIVE_R 51
#define TBBE_HELP_CONTENTS 52
#define TBBE_HELP_CONTEXT 53
#define TBBE_PERMISSIONS 54
#define TBBE_CONVERT 55
#define TBBE_UNSELECT_ALL 56
#define TBBE_MENU 57 // enter the menu
#define TBBE_ALTVIEW 58
#define TBBE_EXIT 59
#define TBBE_OCCUPIEDSPACE 60
#define TBBE_EDITNEW 61
#define TBBE_CHANGEDIR 62
#define TBBE_HOTPATHS 63
#define TBBE_CONTEXTMENU 64
#define TBBE_VIEWWITH 65
#define TBBE_EDITWITH 66
#define TBBE_CALCDIRSIZES 67
#define TBBE_FILEHISTORY 68
#define TBBE_DIRHISTORY 69
#define TBBE_HOTPATHSDROP 70
#define TBBE_USER_MENU 71
#define TBBE_EMAIL 72
#define TBBE_ZOOM_PANEL 73
#define TBBE_SHARES 74
#define TBBE_FULLSCREEN 75
#define TBBE_PREV_SELECTED 76
#define TBBE_NEXT_SELECTED 77
#define TBBE_RESELECT 78
#define TBBE_PASTESHORTCUT 79
#define TBBE_FOCUSSHORTCUT 80
#define TBBE_SAVESELECTION 81
#define TBBE_LOADSELECTION 82
#define TBBE_NEW 83
#define TBBE_SEL_BY_EXT 84
#define TBBE_UNSEL_BY_EXT 85
#define TBBE_SEL_BY_NAME 86
#define TBBE_UNSEL_BY_NAME 87
#define TBBE_OPEN_FOLDER 88
#define TBBE_CONFIGRATION 89
#define TBBE_DIRMENU 90
#define TBBE_OPEN_IN_OTHER_ACT 91
#define TBBE_OPEN_IN_OTHER 92
#define TBBE_AS_OTHER_PANEL 93
#define TBBE_HIDE_SELECTED 94
#define TBBE_HIDE_UNSELECTED 95
#define TBBE_SHOW_ALL 96
#define TBBE_OPEN_MYDOC 97
#define TBBE_SMART_COLUMN_MODE 98

#define TBBE_TERMINATOR 99 // terminator

#define NIB1(x) x
#define NIB2(x) x,
#define NIB3(x) x

CButtonData ToolBarButtons[TBBE_TERMINATOR] =
    {
        //                          ImageIndex        Shell32ResID ToolTipResID             ID                          LeftID              RightID      DropDown WD Chk Enabler                       LeftEnabler                   RightEnabler             SVGName
        /*TBBE_CONNECT_NET*/ {NIB1(IDX_TB_CONNECTNET), 0, IDS_TBTT_CONNECTNET, CM_CONNECTNET, 0, 0, 0, 0, 0, NULL, NULL, NULL, "ConnectNetworkDrive"},
        /*TBBE_DISCONNECT_NET*/ {IDX_TB_DISCONNECTNET, 0, IDS_TBTT_DISCONNECTNET, CM_DISCONNECTNET, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Disconnect"},
        /*TBBE_CREATE_DIR*/ {IDX_TB_CREATEDIR, 0, IDS_TBTT_CREATEDIR, CM_CREATEDIR, 0, 0, 0, 0, 0, &EnablerCreateDir, NULL, NULL, "CreateDirectory"},
        /*TBBE_FIND_FILE*/ {IDX_TB_FINDFILE, 0, IDS_TBTT_FINDFILE, CM_FINDFILE, 0, 0, 0, 0, 0, NULL, NULL, NULL, "FindFilesAndDirectories"},
        /*TBBE_VIEW_MODE*/ {IDX_TB_VIEW_MODE, 0, IDS_TBTT_VIEW_MODE, CM_ACTIVEVIEWMODE, CM_LEFTVIEWMODE, CM_RIGHTVIEWMODE, 0, 1, 0, NULL, NULL, NULL, "Views"},
        /*TBBE_DETAILED*/ {0xFFFF, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_SORT_NAME*/ {IDX_TB_SORTBYNAME, 0, IDS_TBTT_SORTBYNAME, CM_ACTIVENAME, CM_LEFTNAME, CM_RIGHTNAME, 0, 0, 1, NULL, NULL, NULL, "SortByName"},
        /*TBBE_SORT_EXT*/ {IDX_TB_SORTBYEXT, 0, IDS_TBTT_SORTBYEXT, CM_ACTIVEEXT, CM_LEFTEXT, CM_RIGHTEXT, 0, 0, 1, NULL, NULL, NULL, "SortByExtension"},
        /*TBBE_SORT_SIZE*/ {IDX_TB_SORTBYSIZE, 0, IDS_TBTT_SORTBYSIZE, CM_ACTIVESIZE, CM_LEFTSIZE, CM_RIGHTSIZE, 0, 0, 1, NULL, NULL, NULL, "SortBySize"},
        /*TBBE_SORT_DATE*/ {IDX_TB_SORTBYDATE, 0, IDS_TBTT_SORTBYDATE, CM_ACTIVETIME, CM_LEFTTIME, CM_RIGHTTIME, 0, 0, 1, NULL, NULL, NULL, "SortByDate"},
        /*TBBE_SORT_ATTR*/ {0xFFFF, 0, 0, 0, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_PARENT_DIR*/ {IDX_TB_PARENTDIR, 0, IDS_TBTT_PARENTDIR, CM_ACTIVEPARENTDIR, CM_LPARENTDIR, CM_RPARENTDIR, 0, 0, 0, &EnablerUpDir, &EnablerLeftUpDir, &EnablerRightUpDir, "ParentDirectory"},
        /*TBBE_ROOT_DIR*/ {IDX_TB_ROOTDIR, 0, IDS_TBTT_ROOTDIR, CM_ACTIVEROOTDIR, CM_LROOTDIR, CM_RROOTDIR, 0, 0, 0, &EnablerRootDir, &EnablerLeftRootDir, &EnablerRightRootDir, "RootDirectory"},
        /*TBBE_FILTER*/ {IDX_TB_FILTER, 0, IDS_TBTT_FILTER, CM_CHANGEFILTER, CM_LCHANGEFILTER, CM_RCHANGEFILTER, 0, 0, 0, NULL, NULL, NULL, "Filter"},
        /*TBBE_BACK*/ {IDX_TB_BACK, 0, IDS_TBTT_BACK, CM_ACTIVEBACK, CM_LBACK, CM_RBACK, 1, 0, 0, &EnablerBackward, &EnablerLeftBackward, &EnablerRightBackward, "Back"},
        /*TBBE_FORWARD*/ {IDX_TB_FORWARD, 0, IDS_TBTT_FORWARD, CM_ACTIVEFORWARD, CM_LFORWARD, CM_RFORWARD, 1, 0, 0, &EnablerForward, &EnablerLeftForward, &EnablerRightForward, "Forward"},
        /*TBBE_REFRESH*/ {IDX_TB_REFRESH, 0, IDS_TBTT_REFRESH, CM_ACTIVEREFRESH, CM_LEFTREFRESH, CM_RIGHTREFRESH, 0, 0, 0, NULL, NULL, NULL, "Refresh"},
        /*TBBE_SWAP_PANELS*/ {IDX_TB_SWAPPANELS, 0, IDS_TBTT_SWAPPANELS, CM_SWAPPANELS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SwapPanels"},
        /*TBBE_PROPERTIES*/ {IDX_TB_PROPERTIES, 0, IDS_TBTT_PROPERTIES, CM_PROPERTIES, 0, 0, 0, 0, 0, &EnablerShowProperties, NULL, NULL, "Properties"},
        /*TBBE_USER_MENU_DROP*/ {IDX_TB_USERMENU, 0, IDS_TBTT_USERMENU, CM_USERMENUDROP, 0, 0, 0, 1, 0, &EnablerOnDisk, NULL, NULL, "UserMenu"},
        /*TBBE_CMD*/ {IDX_TB_COMMANDSHELL, 0, IDS_TBTT_COMMANDSHELL, CM_DOSSHELL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "CommandShell"},
        /*TBBE_COPY*/ {IDX_TB_COPY, 0, IDS_TBTT_COPY, CM_COPYFILES, 0, 0, 0, 0, 0, &EnablerFilesCopy, NULL, NULL, "Copy"},
        /*TBBE_MOVE*/ {IDX_TB_MOVE, 0, IDS_TBTT_MOVE, CM_MOVEFILES, 0, 0, 0, 0, 0, &EnablerFilesMove, NULL, NULL, "Move"},
        /*TBBE_DELETE*/ {IDX_TB_DELETE, 0, IDS_TBTT_DELETE, CM_DELETEFILES, 0, 0, 0, 0, 0, &EnablerFilesDelete, NULL, NULL, "Delete"},
        /*TBBE_COMPRESS*/ {IDX_TB_COMPRESS, 0, IDS_TBTT_COMPRESS, CM_COMPRESS, 0, 0, 0, 0, 0, &EnablerFilesOnDiskCompress, NULL, NULL, "NTFSCompress"},
        /*TBBE_UNCOMPRESS*/ {IDX_TB_UNCOMPRESS, 0, IDS_TBTT_UNCOMPRESS, CM_UNCOMPRESS, 0, 0, 0, 0, 0, &EnablerFilesOnDiskCompress, NULL, NULL, "NTFSUncompress"},
        /*TBBE_QUICK_RENAME*/ {IDX_TB_QUICKRENAME, 0, IDS_TBTT_QUICKRENAME, CM_RENAMEFILE, 0, 0, 0, 0, 0, &EnablerQuickRename, NULL, NULL, "QuickRename"},
        /*TBBE_CHANGE_CASE*/ {IDX_TB_CHANGECASE, 0, IDS_TBTT_CHANGECASE, CM_CHANGECASE, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "ChangeCase"},
        /*TBBE_VIEW*/ {IDX_TB_VIEW, 0, IDS_TBTT_VIEW, CM_VIEW, 0, 0, 1, 0, 0, &EnablerViewFile, NULL, NULL, "View"},
        /*TBBE_EDIT*/ {IDX_TB_EDIT, 0, IDS_TBTT_EDIT, CM_EDIT, 0, 0, 1, 0, 0, &EnablerFileOnDiskOrArchive, NULL, NULL, "Edit"},
        /*TBBE_CLIPBOARD_CUT*/ {IDX_TB_CLIPBOARDCUT, 0, IDS_TBTT_CLIPBOARDCUT, CM_CLIPCUT, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "ClipboardCut"},
        /*TBBE_CLIPBOARD_COPY*/ {IDX_TB_CLIPBOARDCOPY, 0, IDS_TBTT_CLIPBOARDCOPY, CM_CLIPCOPY, 0, 0, 0, 0, 0, &EnablerFilesOnDiskOrArchive, NULL, NULL, "ClipboardCopy"},
        /*TBBE_CLIPBOARD_PASTE*/ {IDX_TB_CLIPBOARDPASTE, 0, IDS_TBTT_CLIPBOARDPASTE, CM_CLIPPASTE, 0, 0, 0, 0, 0, &EnablerPaste, NULL, NULL, "ClipboardPaste"},
        /*TBBE_CHANGE_ATTR*/ {IDX_TB_CHANGEATTR, 0, IDS_TBTT_CHANGEATTR, CM_CHANGEATTR, 0, 0, 0, 0, 0, &EnablerChangeAttrs, NULL, NULL, "ChangeAttributes"},
        /*TBBE_COMPARE_DIR*/ {IDX_TB_COMPAREDIR, 0, IDS_TBTT_COMPAREDIR, CM_COMPAREDIRS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "CompareDirectories"},
        /*TBBE_DRIVE_INFO*/ {IDX_TB_DRIVEINFO, 0, IDS_TBTT_DRIVEINFO, CM_DRIVEINFO, 0, 0, 0, 0, 0, &EnablerDriveInfo, NULL, NULL, "DriveInformation"},
        /*TBBE_CHANGE_DRIVE_L*/ {IDX_TB_CHANGEDRIVEL, 255, IDS_TBTT_CHANGEDRIVE, CM_LCHANGEDRIVE, CM_LCHANGEDRIVE, 0, 0, 1, 0, NULL, NULL, NULL, NULL},
        /*TBBE_SELECT*/ {IDX_TB_SELECT, 0, IDS_TBTT_SELECT, CM_ACTIVESELECT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Select"},
        /*TBBE_UNSELECT*/ {IDX_TB_UNSELECT, 0, IDS_TBTT_UNSELECT, CM_ACTIVEUNSELECT, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "Unselect"},
        /*TBBE_INVERT_SEL*/ {IDX_TB_INVERTSEL, 0, IDS_TBTT_INVERTSEL, CM_ACTIVEINVERTSEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "InvertSelection"},
        /*TBBE_SELECT_ALL*/ {IDX_TB_SELECTALL, 0, IDS_TBTT_SELECTALL, CM_ACTIVESELECTALL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SelectAll"},
        /*TBBE_PACK*/ {IDX_TB_PACK, 0, IDS_TBTT_PACK, CM_PACK, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Pack"},
        /*TBBE_UNPACK*/ {IDX_TB_UNPACK, 0, IDS_TBTT_UNPACK, CM_UNPACK, 0, 0, 0, 0, 0, &EnablerFileOnDisk, NULL, NULL, "Unpack"},
        /*TBBE_OPEN_ACTIVE*/ {NIB1(IDX_TB_OPENACTIVE), 5, IDS_TBTT_OPENACTIVE, CM_OPENACTUALFOLDER, 0, 0, 0, 0, 0, &EnablerOpenActiveFolder, NULL, NULL, NULL},
        /*TBBE_OPEN_DESKTOP*/ {NIB1(IDX_TB_OPENDESKTOP), 35, IDS_TBTT_OPENDESKTOP, CM_OPENDESKTOP, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_MYCOMP*/ {NIB1(IDX_TB_OPENMYCOMP), 16, IDS_TBTT_OPENMYCOMP, CM_OPENMYCOMP, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_CONTROL*/ {NIB1(IDX_TB_OPENCONTROL), 137, IDS_TBTT_OPENCONTROL, CM_OPENCONROLPANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_PRINTERS*/ {NIB1(IDX_TB_OPENPRINTERS), 138, IDS_TBTT_OPENPRINTERS, CM_OPENPRINTERS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_NETWORK*/ {NIB1(IDX_TB_OPENNETWORK), 18, IDS_TBTT_OPENNETWORK, CM_OPENNETNEIGHBOR, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_RECYCLE*/ {NIB1(IDX_TB_OPENRECYCLE), 33, IDS_TBTT_OPENRECYCLE, CM_OPENRECYCLEBIN, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_FONTS*/ {NIB1(IDX_TB_OPENFONTS), 39, IDS_TBTT_OPENFONTS, CM_OPENFONTS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_CHANGE_DRIVE_R*/ {IDX_TB_CHANGEDRIVER, 255, IDS_TBTT_CHANGEDRIVE, CM_RCHANGEDRIVE, 0, CM_RCHANGEDRIVE, 0, 1, 0, NULL, NULL, NULL, NULL},
        /*TBBE_HELP_CONTENTS*/ {NIB1(IDX_TB_HELP), 0, IDS_TBTT_HELP, CM_HELP_CONTENTS, 0, 0, 0, 0, 0, NULL, NULL, NULL, "HelpContents"},
        /*TBBE_HELP_CONTEXT*/ {NIB1(IDX_TB_CONTEXTHELP), 0, IDS_TBTT_CONTEXTHELP, CM_HELP_CONTEXT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "WhatIsThis"},
        /*TBBE_PERMISSIONS*/ {IDX_TB_PERMISSIONS, 0, IDS_TBTT_PERMISSIONS, CM_SEC_PERMISSIONS, 0, 0, 0, 0, 0, &EnablerPermissions, NULL, NULL, "Security"},
        /*TBBE_CONVERT*/ {IDX_TB_CONVERT, 0, IDS_TBTT_CONVERT, CM_CONVERTFILES, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Convert"},
        /*TBBE_UNSELECT_ALL*/ {IDX_TB_UNSELECTALL, 0, IDS_TBTT_UNSELECTALL, CM_ACTIVEUNSELECTALL, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "UnselectAll"},
        /*TBBE_MENU*/ {0xFFFF, 0, IDS_TBTT_MENU, CM_MENU, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_ALTVIEW*/ {0xFFFF, 0, IDS_TBTT_ALTVIEW, CM_ALTVIEW, 0, 0, 0, 0, 0, &EnablerViewFile, NULL, NULL, NULL},
        /*TBBE_EXIT*/ {0xFFFF, 0, IDS_TBTT_EXIT, CM_EXIT, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OCCUPIEDSPACE*/ {IDX_TB_OCCUPIEDSPACE, 0, IDS_TBTT_OCCUPIEDSPACE, CM_OCCUPIEDSPACE, 0, 0, 0, 0, 0, &EnablerOccupiedSpace, NULL, NULL, "CalculateOccupiedSpace"},
        /*TBBE_EDITNEW*/ {IDX_TB_EDITNEW, 0, IDS_TBTT_EDITNEW, CM_EDITNEW, 0, 0, 0, 0, 0, &EnablerOnDisk, NULL, NULL, "EditNewFile"},
        /*TBBE_CHANGEDIR*/ {IDX_TB_CHANGE_DIR, 0, IDS_TBTT_CHANGEDIR, CM_ACTIVE_CHANGEDIR, CM_LEFT_CHANGEDIR, CM_RIGHT_CHANGEDIR, 0, 0, 0, NULL, NULL, NULL, "ChangeDirectory"},
        /*TBBE_HOTPATHS*/ {0xFFFF, 0, IDS_TBTT_HOTPATHS, CM_OPENHOTPATHS, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_CONTEXTMENU*/ {0xFFFF, 0, IDS_TBTT_CONTEXTMENU, CM_CONTEXTMENU, 0, 0, 0, 0, 0, &EnablerItemsContextMenu, NULL, NULL, NULL},
        /*TBBE_VIEWWITH*/ {0xFFFF, 0, IDS_TBTT_VIEWWITH, CM_VIEW_WITH, 0, 0, 0, 0, 0, &EnablerViewFile, NULL, NULL, NULL},
        /*TBBE_EDITWITH*/ {0xFFFF, 0, IDS_TBTT_EDITWITH, CM_EDIT_WITH, 0, 0, 0, 0, 0, &EnablerFileOnDiskOrArchive, NULL, NULL, NULL},
        /*TBBE_CALCDIRSIZES*/ {IDX_TB_CALCDIRSIZES, 0, IDS_TBTT_CALCDIRSIZES, CM_CALCDIRSIZES, 0, 0, 0, 0, 0, &EnablerCalcDirSizes, NULL, NULL, "CalculateDirectorySizes"},
        /*TBBE_FILEHISTORY*/ {0xFFFF, 0, IDS_TBTT_FILEHISTORY, CM_FILEHISTORY, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_DIRHISTORY*/ {0xFFFF, 0, IDS_TBTT_DIRHISTORY, CM_DIRHISTORY, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_HOTPATHSDROP*/ {IDX_TB_HOTPATHS, 0, IDS_TBTT_HOTPATHSDROP, CM_OPENHOTPATHSDROP, 0, 0, 0, 1, 0, NULL, NULL, NULL, "GoToHotPath"},
        /*TBBE_USER_MENU*/ {IDX_TB_USERMENU, 0, IDS_TBTT_USERMENU, CM_USERMENU, 0, 0, 0, 0, 0, &EnablerOnDisk, NULL, NULL, "UserMenu"},
        /*TBBE_EMAIL*/ {IDX_TB_EMAIL, 0, IDS_TBTT_EMAIL, CM_EMAILFILES, 0, 0, 0, 0, 0, &EnablerFilesOnDisk, NULL, NULL, "Email"},
        /*TBBE_ZOOM_PANEL*/ {0xFFFF, 0, IDS_TBTT_ZOOMPANEL, CM_ACTIVEZOOMPANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "SharedDirectories"},
        /*TBBE_SHARES*/ {IDX_TB_SHARED_DIRS, 0, IDS_TBTT_SHARES, CM_SHARES, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_FULLSCREEN*/ {0xFFFF, 0, IDS_TBTT_FULLSCREEN, CM_FULLSCREEN, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_PREV_SELECTED*/ {IDX_TB_PREV_SELECTED, 0, IDS_TBTT_PREV_SEL, CM_GOTO_PREV_SEL, 0, 0, 0, 0, 0, &EnablerSelGotoPrev, NULL, NULL, "GoToPreviousSelectedName"},
        /*TBBE_NEXT_SELECTED*/ {IDX_TB_NEXT_SELECTED, 0, IDS_TBTT_NEXT_SEL, CM_GOTO_NEXT_SEL, 0, 0, 0, 0, 0, &EnablerSelGotoNext, NULL, NULL, "GoToNextSelectedName"},
        /*TBBE_RESELECT*/ {IDX_TB_RESELECT, 0, IDS_TBTT_RESELECT, CM_RESELECT, 0, 0, 0, 0, 0, &EnablerSelectionStored, NULL, NULL, "RestoreSelection"},
        /*TBBE_PASTESHORTCUT*/ {IDX_TB_PASTESHORTCUT, 0, IDS_TBTT_PASTESHORTCUT, CM_CLIPPASTELINKS, 0, 0, 0, 0, 0, &EnablerPasteLinksOnDisk, NULL, NULL, "PasteShortcut"},
        /*TBBE_FOCUSSHORTCUT*/ {IDX_TB_FOCUSSHORTCUT, 0, IDS_TBTT_FOCUSSHORTCUT, CM_AFOCUSSHORTCUT, 0, 0, 0, 0, 0, &EnablerFileOrDirLinkOnDisk, NULL, NULL, "GoToShortcutTarget"},
        /*TBBE_SAVESELECTION*/ {IDX_TB_SAVESELECTION, 0, IDS_TBTT_SAVESELECTION, CM_STORESEL, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "SaveSelection"},
        /*TBBE_LOADSELECTION*/ {IDX_TB_LOADSELECTION, 0, IDS_TBTT_LOADSELECTION, CM_RESTORESEL, 0, 0, 0, 0, 0, &EnablerGlobalSelStored, NULL, NULL, "LoadSelection"},
        /*TBBE_NEW*/ {IDX_TB_NEW, 0, IDS_TBTT_NEW, CM_NEWDROP, 0, 0, 0, 1, 0, &EnablerOnDisk, NULL, NULL, "New"},
        /*TBBE_SEL_BY_EXT*/ {IDX_TB_SEL_BY_EXT, 0, IDS_TBTT_SEL_BY_EXT, CM_SELECTBYFOCUSEDEXT, 0, 0, 0, 0, 0, &EnablerFileDir, NULL, NULL, "SelectFilesWithSameExtension"},
        /*TBBE_UNSEL_BY_EXT*/ {IDX_TB_UNSEL_BY_EXT, 0, IDS_TBTT_UNSEL_BY_EXT, CM_UNSELECTBYFOCUSEDEXT, 0, 0, 0, 0, 0, &EnablerFileDirANDSelected, NULL, NULL, "UnselectFilesWithSameExtension"},
        /*TBBE_SEL_BY_NAME*/ {IDX_TB_SEL_BY_NAME, 0, IDS_TBTT_SEL_BY_NAME, CM_SELECTBYFOCUSEDNAME, 0, 0, 0, 0, 0, &EnablerFileDir, NULL, NULL, "SelectFilesWithSameName"},
        /*TBBE_UNSEL_BY_NAME*/ {IDX_TB_UNSEL_BY_NAME, 0, IDS_TBTT_UNSEL_BY_NAME, CM_UNSELECTBYFOCUSEDNAME, 0, 0, 0, 0, 0, &EnablerFileDirANDSelected, NULL, NULL, "UnselectFilesWithSameName"},
        /*TBBE_OPEN_FOLDER*/ {NIB1(IDX_TB_OPEN_FOLDER), 0, IDS_TBTT_OPEN_FOLDER, CM_OPEN_FOLDER_DROP, 0, 0, 0, 1, 0, NULL, NULL, NULL, "OpenFolder"},
        /*TBBE_CONFIGRATION*/ {IDX_TB_CONFIGURARTION, 0, IDS_TBTT_CONFIGURATION, CM_CONFIGURATION, 0, 0, 0, 0, 0, NULL, NULL, NULL, "Configuration"},
        /*TBBE_DIRMENU*/ {0xFFFF, 0, IDS_TBTT_DIRMENU, CM_DIRMENU, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_OPEN_IN_OTHER_ACT*/ {IDX_TB_OPEN_IN_OTHER_ACT, 0, IDS_TBTT_OPENINOTHER_A, CM_OPEN_IN_OTHER_PANEL_ACT, 0, 0, 0, 0, 0, NULL, NULL, NULL, "FocusNameInOtherPanel"},
        /*TBBE_OPEN_IN_OTHER*/ {IDX_TB_OPEN_IN_OTHER, 0, IDS_TBTT_OPENINOTHER, CM_OPEN_IN_OTHER_PANEL, 0, 0, 0, 0, 0, NULL, NULL, NULL, "OpenNameInOtherPanel"},
        /*TBBE_AS_OTHER_PANEL*/ {IDX_TB_AS_OTHER_PANEL, 0, IDS_TBTT_ASOTHERPANEL, CM_ACTIVE_AS_OTHER, CM_LEFT_AS_OTHER, CM_RIGHT_AS_OTHER, 0, 0, 0, NULL, NULL, NULL, "GoToPathFromOtherPanel"},
        /*TBBE_HIDE_SELECTED*/ {IDX_TB_HIDE_SELECTED, 0, IDS_TBTT_HIDE_SELECTED, CM_HIDE_SELECTED_NAMES, 0, 0, 0, 0, 0, &EnablerSelected, NULL, NULL, "HideSelectedNames"},
        /*TBBE_HIDE_UNSELECTED*/ {IDX_TB_HIDE_UNSELECTED, 0, IDS_TBTT_HIDE_UNSELECTED, CM_HIDE_UNSELECTED_NAMES, 0, 0, 0, 0, 0, &EnablerUnselected, NULL, NULL, "HideUnselectedNames"},
        /*TBBE_SHOW_ALL*/ {IDX_TB_SHOW_ALL, 0, IDS_TBTT_SHOW_ALL, CM_SHOW_ALL_NAME, 0, 0, 0, 0, 0, &EnablerHiddenNames, NULL, NULL, "ShowHiddenNames"},
        /*TBBE_OPEN_MYDOC*/ {NIB1(IDX_TB_OPENMYDOC), 21, IDS_TBTT_OPENMYDOC, CM_OPENPERSONAL, 0, 0, 0, 0, 0, NULL, NULL, NULL, NULL},
        /*TBBE_SMART_COLUMN_MODE*/ {IDX_TB_SMART_COLUMN_MODE, 0, IDS_TBTT_SMARTMODE, CM_ACTIVE_SMARTMODE, CM_LEFT_SMARTMODE, CM_RIGHT_SMARTMODE, 0, 0, 0, NULL, NULL, NULL, "SmartColumnMode"},

};

//
// TopToolbar
//
// Represents all possible buttons that TopToolbar can contain.
// Order specifies the order of buttons in the configuration dialog of toolbars and can
// Apartment arbitrarily named.
//

DWORD TopToolBarButtons[] =
    {
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_HOTPATHSDROP,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,

        NIB2(TBBE_CONNECT_NET)
            TBBE_DISCONNECT_NET,
        TBBE_CREATE_DIR,
        TBBE_FIND_FILE,
        TBBE_CHANGEDIR,
        TBBE_SHARES,

        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,

        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SWAP_PANELS,
        TBBE_SMART_COLUMN_MODE,

        TBBE_USER_MENU_DROP,
        TBBE_CMD,

        TBBE_COPY,
        TBBE_MOVE,
        TBBE_DELETE,
        TBBE_EMAIL,

        TBBE_COMPRESS,
        TBBE_UNCOMPRESS,
        TBBE_QUICK_RENAME,
        TBBE_CHANGE_CASE,
        TBBE_CONVERT,
        TBBE_VIEW,
        TBBE_EDIT,
        TBBE_EDITNEW,
        TBBE_NEW,

        TBBE_CLIPBOARD_CUT,
        TBBE_CLIPBOARD_COPY,
        TBBE_CLIPBOARD_PASTE,
        TBBE_PASTESHORTCUT,
        TBBE_CHANGE_ATTR,
        TBBE_COMPARE_DIR,
        TBBE_DRIVE_INFO,
        TBBE_CALCDIRSIZES,
        TBBE_OCCUPIEDSPACE,
        TBBE_PERMISSIONS,
        TBBE_PROPERTIES,
        TBBE_FOCUSSHORTCUT,

        TBBE_OPEN_IN_OTHER_ACT,
        TBBE_OPEN_IN_OTHER,
        TBBE_AS_OTHER_PANEL,

        TBBE_SELECT,
        TBBE_UNSELECT,
        TBBE_INVERT_SEL,
        TBBE_SELECT_ALL,
        TBBE_UNSELECT_ALL,
        TBBE_RESELECT,
        TBBE_SEL_BY_EXT,
        TBBE_UNSEL_BY_EXT,
        TBBE_SEL_BY_NAME,
        TBBE_UNSEL_BY_NAME,
        TBBE_PREV_SELECTED,
        TBBE_NEXT_SELECTED,
        TBBE_SAVESELECTION,
        TBBE_LOADSELECTION,
        TBBE_HIDE_SELECTED,
        TBBE_HIDE_UNSELECTED,
        TBBE_SHOW_ALL,

        TBBE_PACK,
        TBBE_UNPACK,

        NIB2(TBBE_OPEN_FOLDER)
            NIB2(TBBE_OPEN_ACTIVE)
                NIB2(TBBE_OPEN_DESKTOP)
                    NIB2(TBBE_OPEN_MYCOMP)
                        NIB2(TBBE_OPEN_CONTROL)
                            NIB2(TBBE_OPEN_PRINTERS)
                                NIB2(TBBE_OPEN_NETWORK)
                                    NIB2(TBBE_OPEN_RECYCLE)
                                        NIB2(TBBE_OPEN_FONTS)
                                            NIB2(TBBE_OPEN_MYDOC)

                                                TBBE_CONFIGRATION,

        NIB2(TBBE_HELP_CONTENTS)
            NIB2(TBBE_HELP_CONTEXT)

                TBBE_TERMINATOR // terminator - must be here!
};

DWORD LeftToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_L,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - must be here!
};

DWORD RightToolBarButtons[] =
    {
        TBBE_CHANGE_DRIVE_R,
        TBBE_PARENT_DIR,
        TBBE_ROOT_DIR,
        TBBE_CHANGEDIR,
        TBBE_AS_OTHER_PANEL,
        TBBE_BACK,
        TBBE_FORWARD,
        TBBE_VIEW_MODE,
        TBBE_SORT_NAME,
        TBBE_SORT_EXT,
        TBBE_SORT_SIZE,
        TBBE_SORT_DATE,
        TBBE_FILTER,
        TBBE_REFRESH,
        TBBE_SMART_COLUMN_MODE,

        TBBE_TERMINATOR // terminator - must be here!
};

void GetSVGIconsMainToolbar(CSVGIcon** svgIcons, int* svgIconsCount)
{
    static CSVGIcon SVGIcons[TBBE_TERMINATOR];
    for (auto i = 0; i < TBBE_TERMINATOR; i++)
    {
        SVGIcons[i].ImageIndex = ToolBarButtons[i].ImageIndex;
        SVGIcons[i].SVGName = ToolBarButtons[i].SVGName;
    }
    *svgIcons = SVGIcons;
    *svgIconsCount = TBBE_TERMINATOR;
}

//****************************************************************************
//
// CreateGrayscaleAndMaskBitmaps
//
// Creates a new bitmap with a depth of 24 bits, copies the source into it
// bitmap and converts it to grayscale. It also prepares a second bitmap
// with a mask according to the transparent color.
//

BOOL CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                   HBITMAP& hGrayscale, HBITMAP& hMask)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleAndMaskBitmaps(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    VOID* lpvBitsMask = NULL;
    hGrayscale = NULL;
    hMask = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // retrieve bitmap dimensions
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // we do not want a palette

    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    if (bi.bmiHeader.biSizeImage == 0)
    {
        TRACE_E("bi.bmiHeader.biSizeImage = 0");
        goto exitus;
    }

    // desired color depth is 24 bits
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // allocate the necessary space
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }
    lpvBitsMask = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBitsMask == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // extract my own data
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBits,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // extract my own data for the mask
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBitsMask,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // convert to grayscale
    BYTE* rgb;
    BYTE* rgbMask;
    rgb = (BYTE*)lpvBits;
    rgbMask = (BYTE*)lpvBitsMask;
    int i;
    for (i = 0; i < bi.bmiHeader.biWidth * bi.bmiHeader.biHeight; i++)
    {
        BYTE r = rgb[2];
        BYTE g = rgb[1];
        BYTE b = rgb[0];
        if (transparent != RGB(r, g, b))
        {
            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            rgb[0] = rgb[1] = rgb[2] = brightness;
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)0;
        }
        else
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)255;
        rgb += 3;
        rgbMask += 3;
    }

    // create a new bitmap over grayscale data
    hGrayscale = HANDLES(CreateDIBitmap(hDC,
                                        &bi.bmiHeader,
                                        (LONG)CBM_INIT,
                                        lpvBits,
                                        &bi,
                                        DIB_RGB_COLORS));
    if (hGrayscale == NULL)
    {
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    // create a new bitmap over the mask data
    hMask = HANDLES(CreateDIBitmap(hDC,
                                   &bi.bmiHeader,
                                   (LONG)CBM_INIT,
                                   lpvBitsMask,
                                   &bi,
                                   DIB_RGB_COLORS));
    if (hMask == NULL)
    {
        HANDLES(DeleteObject(hGrayscale));
        hGrayscale = NULL;
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    ret = TRUE;
exitus:
    if (lpvBits != NULL)
        free(lpvBits);
    if (lpvBitsMask != NULL)
        free(lpvBitsMask);
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));
    return ret;
}

// JRYFIXME - temporary for transition to SVG
BOOL CreateGrayscaleAndMaskBitmaps_tmp(HBITMAP hSource, COLORREF transparent, COLORREF bkColorForAlpha,
                                       HBITMAP& hGrayscale, HBITMAP& hMask)
{
    CALL_STACK_MESSAGE1("CreateGrayscaleAndMaskBitmaps(, , ,)");
    BOOL ret = FALSE;
    VOID* lpvBits = NULL;
    VOID* lpvBitsMask = NULL;
    hGrayscale = NULL;
    hMask = NULL;
    HDC hDC = HANDLES(GetDC(NULL));

    // retrieve bitmap dimensions
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // we do not want a palette

    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    if (bi.bmiHeader.biSizeImage == 0)
    {
        TRACE_E("bi.bmiHeader.biSizeImage = 0");
        goto exitus;
    }

    // desired color depth is 24 bits
    bi.bmiHeader.biSizeImage = ((((bi.bmiHeader.biWidth * 24) + 31) & ~31) >> 3) * bi.bmiHeader.biHeight;
    // allocate the necessary space
    lpvBits = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBits == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }
    lpvBitsMask = malloc(bi.bmiHeader.biSizeImage);
    if (lpvBitsMask == NULL)
    {
        TRACE_E("malloc failed");
        goto exitus;
    }

    bi.bmiHeader.biPlanes = 1;
    bi.bmiHeader.biBitCount = 24;
    bi.bmiHeader.biCompression = BI_RGB;

    // extract my own data
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBits,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // extract my own data for the mask
    if (!GetDIBits(hDC,
                   hSource,
                   0, bi.bmiHeader.biHeight,
                   lpvBitsMask,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    // convert to grayscale
    BYTE* rgb;
    BYTE* rgbMask;
    rgb = (BYTE*)lpvBits;
    rgbMask = (BYTE*)lpvBitsMask;
    int i;
    for (i = 0; i < bi.bmiHeader.biWidth * bi.bmiHeader.biHeight; i++)
    {
        BYTE r = rgb[2];
        BYTE g = rgb[1];
        BYTE b = rgb[0];
        if (transparent != RGB(r, g, b) && bkColorForAlpha != RGB(r, g, b))
        {
            BYTE brightness = GetGrayscaleFromRGB(r, g, b);
            float alpha = 0.5;
            float alphablended = (float)GetRValue(bkColorForAlpha) * alpha + (float)brightness * (1 - alpha);
            rgb[0] = rgb[1] = rgb[2] = (BYTE)alphablended;
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)0;
        }
        else
            rgbMask[0] = rgbMask[1] = rgbMask[2] = (BYTE)255;
        rgb += 3;
        rgbMask += 3;
    }

    // create a new bitmap over grayscale data
    hGrayscale = HANDLES(CreateDIBitmap(hDC,
                                        &bi.bmiHeader,
                                        (LONG)CBM_INIT,
                                        lpvBits,
                                        &bi,
                                        DIB_RGB_COLORS));
    if (hGrayscale == NULL)
    {
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    // create a new bitmap over the mask data
    hMask = HANDLES(CreateDIBitmap(hDC,
                                   &bi.bmiHeader,
                                   (LONG)CBM_INIT,
                                   lpvBitsMask,
                                   &bi,
                                   DIB_RGB_COLORS));
    if (hMask == NULL)
    {
        HANDLES(DeleteObject(hGrayscale));
        hGrayscale = NULL;
        TRACE_E("CreateDIBitmap failed");
        goto exitus;
    }

    ret = TRUE;
exitus:
    if (lpvBits != NULL)
        free(lpvBits);
    if (lpvBitsMask != NULL)
        free(lpvBitsMask);
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));
    return ret;
}

void RenderSVGImages(HDC hDC, int iconSize, COLORREF bkColor, const CSVGIcon* svgIcons, int svgIconsCount)
{
    NSVGrasterizer* rast = nsvgCreateRasterizer();
    // JRYFIXME: temporarily reading from file, switch to shared storage with toolbars
    for (int i = 0; i < svgIconsCount; i++)
        if (svgIcons[i].SVGName != NULL)
            RenderSVGImage(rast, hDC, svgIcons[i].ImageIndex * iconSize, 0, svgIcons[i].SVGName, iconSize, bkColor, TRUE);

    nsvgDeleteRasterizer(rast);
}

//****************************************************************************
//
// CreateToolbarBitmaps
//
// Extracts a bitmap from resID, copies it to a new bitmap, which is
// color compatible with the screen. Then it connects to this bitmap
// Icons from shell32.dll. Read the transparent color.
// bkColorForAlpha specifies the color that will shine through the transparent part of icons (WinXP)
//

BOOL CreateToolbarBitmaps(HINSTANCE hInstance, int resID, COLORREF transparent, COLORREF bkColorForAlpha,
                          HBITMAP& hMaskBitmap, HBITMAP& hGrayBitmap, HBITMAP& hColorBitmap, BOOL appendIcons,
                          const CSVGIcon* svgIcons, int svgIconsCount)
{
    CALL_STACK_MESSAGE5("CreateToolbarBitmaps(%p, %d, %x, %x, , , )", hInstance, resID, transparent, bkColorForAlpha);
    BOOL ret = FALSE;
    HDC hDC = NULL;
    HBITMAP hOldSrcBitmap = NULL;
    HBITMAP hOldTgtBitmap = NULL;
    HDC hTgtMemDC = NULL;
    HDC hSrcMemDC = NULL;
    hMaskBitmap = NULL;
    hGrayBitmap = NULL;
    hColorBitmap = NULL;

    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16); // small icon size
    int iconCount = 0;

    // Windows XP and newer use transparent icons; because of the mask
    // we will display it in this temporary bitmap and ensure that it is under the transparent part
    // gray color from the toolbar and not our purple camouflage
    HBITMAP hTmpBitmap = NULL;
    HDC hTmpMemDC = NULL;
    HBITMAP hOldTmpBitmap = NULL;

    // load the source bitmap
    HBITMAP hSource;
    if (resID == IDB_TOOLBAR_256) // dirty hack, it would be nice to have detection based on the type of resource (RCDATA), possibly based on the PNG signature
        hSource = LoadPNGBitmap(hInstance, MAKEINTRESOURCE(resID), 0);
    else
        hSource = HANDLES(LoadBitmap(hInstance, MAKEINTRESOURCE(resID)));
    if (hSource == NULL)
    {
        TRACE_E("LoadBitmap failed on redID " << resID);
        goto exitus;
    }

    hDC = HANDLES(GetDC(NULL));
    // retrieve bitmap dimensions
    BITMAPINFO bi;
    memset(&bi, 0, sizeof(bi));
    bi.bmiHeader.biSize = sizeof(bi.bmiHeader);
    bi.bmiHeader.biBitCount = 0; // we do not want a palette
    if (!GetDIBits(hDC,
                   hSource,
                   0, 0,
                   NULL,
                   &bi,
                   DIB_RGB_COLORS))
    {
        TRACE_E("GetDIBits failed");
        goto exitus;
    }

    int tbbe_BMPCOUNT;
    int i;
    tbbe_BMPCOUNT = 0;
    if (appendIcons)
    {
        for (i = 0; i < TBBE_TERMINATOR; i++)
            if (ToolBarButtons[i].Shell32ResID != 0)
                tbbe_BMPCOUNT++;
    }

    // prepare a new bitmap that will fit hSource and icons from the DLL
    // Extend the length of icons from DLL
    iconCount = bi.bmiHeader.biWidth / 16 + tbbe_BMPCOUNT;

    //  hColorBitmap = CreateBitmap(width, height, bh.bV4Planes, bh.bV4BitCount, NULL);
    //because CreateBitmap() is suitable only for creating B&W bitmaps (see MSDN)
    //we are transitioning from room 2.5b7 to the fast CreateCompatibleBitmap()
    hColorBitmap = HANDLES(CreateCompatibleBitmap(hDC, iconSize * iconCount, iconSize));

    hTgtMemDC = HANDLES(CreateCompatibleDC(NULL));
    hSrcMemDC = HANDLES(CreateCompatibleDC(NULL));
    hOldTgtBitmap = (HBITMAP)SelectObject(hTgtMemDC, hColorBitmap);
    hOldSrcBitmap = (HBITMAP)SelectObject(hSrcMemDC, hSource);

    // for transferring icons (including transparent ones)
    hTmpBitmap = HANDLES(CreateBitmap(iconSize, iconSize, 1, 1, NULL));
    hTmpMemDC = HANDLES(CreateCompatibleDC(NULL));
    hOldTmpBitmap = (HBITMAP)SelectObject(hTmpMemDC, hTmpBitmap);

    // Copy the original bitmap to a new bitmap
    if (!StretchBlt(hTgtMemDC, 0, 0, (iconCount - tbbe_BMPCOUNT) * iconSize, iconSize,
                    hSrcMemDC, 0, 0, bi.bmiHeader.biWidth, bi.bmiHeader.biHeight,
                    SRCCOPY))
    {
        TRACE_E("BitBlt failed");
        goto exitus;
    }

    // discard the source bitmap
    SelectObject(hSrcMemDC, hOldSrcBitmap);
    hOldSrcBitmap = NULL;

    // if we have an SVG version, we will use it
    if (svgIcons != NULL)
    {
        RenderSVGImages(hTgtMemDC, iconSize, bkColorForAlpha, svgIcons, svgIconsCount);
    }

    // we will use hTmpMemDC->hTgtMemDC in BitBlt
    //SetBkColor(hTgtMemDC, transparent);
    //SetTextColor(hTgtMemDC, bkColorForAlpha);

    if (appendIcons)
    {
        // Milking shell32.dll
        HICON hIcon;
        for (i = 0; i < TBBE_TERMINATOR; i++)
        {
            BYTE resID2 = ToolBarButtons[i].Shell32ResID;
            if (resID2 == 0 || resID2 == 255)
                continue;

            if ((resID2 == 21 && (resID2 = 112) != 0 || // Documents
                 resID2 == 33 && (resID2 = 54) != 0 ||  // Recycle Bin
                 resID2 == 138 && (resID2 = 26) != 0 || // Printers
                 resID2 == 39 && (resID2 = 77) != 0 ||  // Fonts
                 resID2 == 18 && (resID2 = 152) != 0))  // Network
            {
                hIcon = SalLoadIcon(ImageResDLL, resID2, iconSize);
            }
            else
            {
                // Documents are elsewhere from WinXP
                if (resID2 == 21)
                    resID2 = 235;

                hIcon = SalLoadIcon(Shell32DLL, resID2, iconSize);
            }
            if (hIcon == NULL)
            {
                TRACE_E("LoadImage failed on resID2 " << resID2);
                continue;
            }

            // prepare the background for icons with alpha channel under WinXP
            DrawIconEx(hTmpMemDC, 0, 0, hIcon, iconSize, iconSize, 0, 0, DI_MASK);

            // we will use hTmpMemDC->hTgtMemDC in BitBlt
            SetBkColor(hTgtMemDC, transparent);
            SetTextColor(hTgtMemDC, bkColorForAlpha);
            BitBlt(hTgtMemDC, iconSize * ToolBarButtons[i].ImageIndex, 0, iconSize, iconSize,
                   hTmpMemDC, 0, 0,
                   SRCCOPY);

            if (!DrawIconEx(hTgtMemDC, iconSize * ToolBarButtons[i].ImageIndex, 0, hIcon,
                            iconSize, iconSize, 0, 0, DI_NORMAL))
            {
                HANDLES(DestroyIcon(hIcon));
                TRACE_E("DrawIconEx failed on resID2 " << resID2);
                continue;
            }
            else
            {
                /*          // --- crazy patch BEGIN
        // John: under W2K, it didn't work for icon 21 (Documents) DrawIconEx with parameter DI_NORMAL
        // for some unknown reason, it saturated the transparent space and thus changed the 'transparent' color
        SetBkColor(hTgtMemDC, RGB(0, 0, 0));
        SetTextColor(hTgtMemDC, RGB(255, 255, 255));
        BitBlt(hTgtMemDC, ICON16_CX * ToolBarButtons[i].ImageIndex, 0, ICON16_CX, ICON16_CX,
               hTmpMemDC, 0, 0,
               SRCAND);
        SetBkColor(hTgtMemDC, RGB(255, 0, 255));
        SetTextColor(hTgtMemDC, RGB(0, 0, 0));
        BitBlt(hTgtMemDC, ICON16_CX * ToolBarButtons[i].ImageIndex, 0, ICON16_CX, ICON16_CX,
               hTmpMemDC, 0, 0,
               SRCPAINT);
        // --- crazy patch END*/
                HANDLES(DestroyIcon(hIcon));
            }
        }
    }

    ret = TRUE;
exitus:
    if (hOldSrcBitmap != NULL)
        SelectObject(hSrcMemDC, hOldSrcBitmap);
    if (hOldTgtBitmap != NULL)
        SelectObject(hTgtMemDC, hOldTgtBitmap);
    if (hSrcMemDC != NULL)
        HANDLES(DeleteDC(hSrcMemDC));
    if (hTgtMemDC != NULL)
        HANDLES(DeleteDC(hTgtMemDC));
    if (hSource != NULL)
        HANDLES(DeleteObject(hSource));
    if (hDC != NULL)
        HANDLES(ReleaseDC(NULL, hDC));

    if (hOldTmpBitmap != NULL)
        SelectObject(hTmpMemDC, hOldTmpBitmap);
    if (hTmpBitmap != NULL)
        HANDLES(DeleteObject(hTmpBitmap));
    if (hTmpMemDC != NULL)
        HANDLES(DeleteDC(hTmpMemDC));

    if (ret)
    {
        ret = CreateGrayscaleAndMaskBitmaps_tmp(hColorBitmap, transparent, bkColorForAlpha, hGrayBitmap, hMaskBitmap);
        if (!ret)
        {
            HANDLES(DeleteObject(hColorBitmap));
            hColorBitmap = NULL;
        }
    }

    return ret;
}

//****************************************************************************
//
// PrepareToolTipText
//
// Search the buff for the first occurrence of the '\t' character. If the variable is set
// stripHotKey, inserts a terminator in its place and returns. Otherwise, in its
// Inserts a space instead, shifts the rest by one character to the right, and encloses it in parentheses.
//

void PrepareToolTipText(char* buff, BOOL stripHotKey)
{
    CALL_STACK_MESSAGE2("PrepareToolTipText(, %d)", stripHotKey);
    char* p = buff;
    while (*p != '\t' && *p != 0)
        p++;
    if (*p == '\t')
    {
        if (!stripHotKey && *(p + 1) != 0)
        {
            *p = ' ';
            p++;
            memmove(p + 1, p, lstrlen(p) + 1);
            *p = '(';
            lstrcat(p, ")");
        }
        else
            *p = 0;
    }
}

//*****************************************************************************
//
// CMainToolBar
//

CMainToolBar::CMainToolBar(HWND hNotifyWindow, CMainToolBarType type, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    Type = type;
}

BOOL CMainToolBar::FillTII(int tbbeIndex, TLBI_ITEM_INFO2* tii, BOOL fillName)
{
    CALL_STACK_MESSAGE3("CMainToolBar::FillTII(%d, , %d)", tbbeIndex, fillName);
    if (tbbeIndex < -1 || tbbeIndex >= TBBE_TERMINATOR)
    {
        TRACE_E("tbbeIndex = " << tbbeIndex);
        return FALSE;
    }
    if (ToolBarButtons[tbbeIndex].ImageIndex == 0xFFFF)
    {
        // old item that was removed in this version
        return FALSE;
    }

    if (tbbeIndex == -1 || tbbeIndex >= TBBE_TERMINATOR)
    {
        tii->Mask = TLBI_MASK_STYLE;
        tii->Style = TLBI_STYLE_SEPARATOR;
    }
    else
    {
        if (Type == mtbtLeft && tbbeIndex == TBBE_CHANGE_DRIVE_R)
            tbbeIndex = TBBE_CHANGE_DRIVE_L;
        if (Type == mtbtRight && tbbeIndex == TBBE_CHANGE_DRIVE_L)
            tbbeIndex = TBBE_CHANGE_DRIVE_R;
        tii->Mask = TLBI_MASK_STYLE | TLBI_MASK_ID |
                    TLBI_MASK_ENABLER | TLBI_MASK_CUSTOMDATA;
        tii->Style = 0;
        tii->CustomData = tbbeIndex;
        tii->Mask |= TLBI_MASK_IMAGEINDEX;
        tii->ImageIndex = ToolBarButtons[tbbeIndex].ImageIndex;

        if (ToolBarButtons[tbbeIndex].DropDown)
            tii->Style |= TLBI_STYLE_SEPARATEDROPDOWN;
        if (ToolBarButtons[tbbeIndex].WholeDropDown)
            tii->Style |= TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
        if (ToolBarButtons[tbbeIndex].Check)
            tii->Style |= TLBI_STYLE_RADIO;
        switch (Type)
        {
        case mtbtMiddle:
        case mtbtTop:
        {
            tii->ID = ToolBarButtons[tbbeIndex].ID;
            tii->Enabler = ToolBarButtons[tbbeIndex].Enabler;
            break;
        }

        case mtbtLeft:
        {
            tii->ID = ToolBarButtons[tbbeIndex].LeftID;
            tii->Enabler = ToolBarButtons[tbbeIndex].LeftEnabler;
            break;
        }

        case mtbtRight:
        {
            tii->ID = ToolBarButtons[tbbeIndex].RightID;
            tii->Enabler = ToolBarButtons[tbbeIndex].RightEnabler;
            break;
        }
        }
        if (fillName)
        {
            tii->Name = LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID);
            // The string will be truncated, so we can perform the operation on the buffer from LoadStr
            PrepareToolTipText(tii->Name, TRUE);
        }
    }
    return TRUE;
}

BOOL CMainToolBar::Load(const char* data)
{
    CALL_STACK_MESSAGE2("CMainToolBar::Load(%s)", data);
    char tmp[5000];
    lstrcpyn(tmp, data, 5000);

    RemoveAllItems();
    char* p = strtok(tmp, ",");
    while (p != NULL)
    {
        int tbbeIndex = atoi(p);
        if (tbbeIndex >= -1 && tbbeIndex < TBBE_TERMINATOR)
        {
            TLBI_ITEM_INFO2 tii;
            if (FillTII(tbbeIndex, &tii, FALSE))
            {
                if (!InsertItem2(0xFFFFFFFF, TRUE, &tii))
                    return FALSE;
            }
            else
            {
                TRACE_I("CMainToolBar::Load skipping tbbeIndex=" << tbbeIndex);
            }
            p = strtok(NULL, ",");
        }
        else
        {
            TRACE_I("CMainToolBar::Load skipping tbbeIndex=" << tbbeIndex);
            p = strtok(NULL, ",");
        }
    }
    return TRUE;
}

BOOL CMainToolBar::Save(char* data)
{
    CALL_STACK_MESSAGE1("CMainToolBar::Save()");
    TLBI_ITEM_INFO2 tii;
    int count = GetItemCount();
    int tbbeIndex;
    char* p = data;
    *p = 0;
    int i;
    for (i = 0; i < count; i++)
    {
        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_CUSTOMDATA;
        GetItemInfo2(i, TRUE, &tii);
        if (tii.Style == TLBI_STYLE_SEPARATOR)
            tbbeIndex = -1;
        else
            tbbeIndex = tii.CustomData;
        p += sprintf(p, "%d", tbbeIndex);
        if (i < count - 1)
        {
            lstrcpy(p, ",");
            p++;
        }
    }
    return TRUE;
}

void CMainToolBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CMainToolBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    int tbbeIndex = tt->CustomData;
    if (tbbeIndex < TBBE_TERMINATOR)
    {
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID));
        if (tbbeIndex == TBBE_CLIPBOARD_PASTE)
        {
            CFilesWindow* activePanel = MainWindow != NULL ? MainWindow->GetActivePanel() : NULL;
            BOOL activePanelIsDisk = (activePanel != NULL && activePanel->Is(ptDisk));
            if (EnablerPastePath &&
                (!activePanelIsDisk || !EnablerPasteFiles) && // PasteFiles is a priority
                !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS is a priority
            {
                char tail[50];
                tail[0] = 0;
                char* p = strrchr(tt->Buffer, '\t');
                if (p != NULL)
                    strcpy(tail, p);
                else
                    p = tt->Buffer + strlen(tt->Buffer);
                sprintf(p, " (%s)%s", LoadStr(IDS_PASTE_CHANGE_DIRECTORY), tail);
            }
        }
        PrepareToolTipText(tt->Buffer, FALSE);
    }
}

BOOL CMainToolBar::OnEnumButton(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CMainToolBar::OnEnumButton(0x%IX)", lParam);
    TLBI_ITEM_INFO2* tii = (TLBI_ITEM_INFO2*)lParam;
    DWORD tbbeIndex;
    switch (Type)
    {
    case mtbtMiddle:
    case mtbtTop:
        tbbeIndex = TopToolBarButtons[tii->Index];
        break;
    case mtbtLeft:
        tbbeIndex = LeftToolBarButtons[tii->Index];
        break;
    case mtbtRight:
        tbbeIndex = RightToolBarButtons[tii->Index];
        break;
    }
    if (tbbeIndex == TBBE_TERMINATOR)
        return FALSE; // all buttons have already been pressed
    FillTII(tbbeIndex, tii, TRUE);
    return TRUE;
}

void CMainToolBar::OnReset()
{
    CALL_STACK_MESSAGE1("CMainToolBar::OnReset()");
    const char* defStr = NULL;
    switch (Type)
    {
    case mtbtTop:
        defStr = DefTopToolBar;
        break;
    case mtbtMiddle:
        defStr = DefMiddleToolBar;
        break;
    case mtbtLeft:
        defStr = DefLeftToolBar;
        break;
    case mtbtRight:
        defStr = DefRightToolBar;
        break;
    }
    if (defStr != NULL)
        Load(defStr);
}

void CMainToolBar::SetType(CMainToolBarType type)
{
    CALL_STACK_MESSAGE_NONE
    Type = type;
}

//*****************************************************************************
//
// CBottomToolBar
//

#define BOTTOMTB_TEXT_MAX 15 // maximum length of string for one key
struct CBottomTBData
{
    DWORD Index;
    BYTE TextLen;                 // number of characters in variable 'Text'
    char Text[BOTTOMTB_TEXT_MAX]; // text without terminator
};

CBottomTBData BottomTBData[btbsCount][12] =
    {
        // btbdNormal
        {
            {NIB3(TBBE_HELP_CONTENTS)}, // F1
            {TBBE_QUICK_RENAME},        // F2
            {TBBE_VIEW},                // F3
            {TBBE_EDIT},                // F4
            {TBBE_COPY},                // F5
            {TBBE_MOVE},                // F6
            {TBBE_CREATE_DIR},          // F7
            {TBBE_DELETE},              // F8
            {TBBE_USER_MENU},           // F9
            {TBBE_MENU},                // F10
            {NIB3(TBBE_CONNECT_NET)},   // F11
            {TBBE_DISCONNECT_NET},      // F12
        },
        // btbdAlt,
        {
            {TBBE_CHANGE_DRIVE_L}, // F1
            {TBBE_CHANGE_DRIVE_R}, // F2
            {TBBE_ALTVIEW},        // F3
            {TBBE_EXIT},           // F4
            {TBBE_PACK},           // F5
            {TBBE_UNPACK},         // F6
            {TBBE_FIND_FILE},      // F7
            {TBBE_TERMINATOR},     // F8
            {TBBE_UNPACK},         // F9
            {TBBE_OCCUPIEDSPACE},  // F10
            {TBBE_FILEHISTORY},    // F11
            {TBBE_DIRHISTORY},     // F12
        },
        // btbdCtrl
        {
            {TBBE_DRIVE_INFO},  // F1
            {TBBE_CHANGE_ATTR}, // F2
            {TBBE_SORT_NAME},   // F3
            {TBBE_SORT_EXT},    // F4
            {TBBE_SORT_DATE},   // F5
            {TBBE_SORT_SIZE},   // F6
            {TBBE_CHANGE_CASE}, // F7
            {TBBE_TERMINATOR},  // F8
            {TBBE_REFRESH},     // F9
            {TBBE_COMPARE_DIR}, // F10
            {TBBE_ZOOM_PANEL},  // F11
            {TBBE_FILTER},      // F12
        },
        // btbdShift
        {
            {NIB3(TBBE_HELP_CONTEXT)}, // F1
            {TBBE_TERMINATOR},         // F2
            {NIB3(TBBE_OPEN_ACTIVE)},  // F3
            {TBBE_EDITNEW},            // F4
            {TBBE_TERMINATOR},         // F5
            {TBBE_TERMINATOR},         // F6
            {TBBE_CHANGEDIR},          // F7
            {TBBE_DELETE},             // F8
            {TBBE_HOTPATHS},           // F9
            {TBBE_CONTEXTMENU},        // F10
            {TBBE_TERMINATOR},         // F11
            {TBBE_TERMINATOR},         // F12
        },
        // btbdCtrlShift
        {
            {TBBE_TERMINATOR},    // F1
            {TBBE_TERMINATOR},    // F2
            {TBBE_VIEWWITH},      // F3
            {TBBE_EDITWITH},      // F4
            {TBBE_SAVESELECTION}, // F5
            {TBBE_LOADSELECTION}, // F6
            {TBBE_TERMINATOR},    // F7
            {TBBE_TERMINATOR},    // F8
            {TBBE_SHARES},        // F9
            {TBBE_CALCDIRSIZES},  // F10
            {TBBE_FULLSCREEN},    // F11
            {TBBE_TERMINATOR},    // F12
        },
        // btbsAltShift,
        {
            {TBBE_TERMINATOR}, // F1
            {TBBE_TERMINATOR}, // F2
            {TBBE_TERMINATOR}, // F3
            {TBBE_TERMINATOR}, // F4
            {TBBE_TERMINATOR}, // F5
            {TBBE_TERMINATOR}, // F6
            {TBBE_TERMINATOR}, // F7
            {TBBE_TERMINATOR}, // F8
            {TBBE_TERMINATOR}, // F9
            {TBBE_DIRMENU},    // F10
            {TBBE_TERMINATOR}, // F11
            {TBBE_TERMINATOR}, // F12
        },
        // btbsMenu,
        {
            {NIB3(TBBE_HELP_CONTENTS)}, // F1
            {TBBE_TERMINATOR},          // F2
            {TBBE_TERMINATOR},          // F3
            {TBBE_TERMINATOR},          // F4
            {TBBE_TERMINATOR},          // F5
            {TBBE_TERMINATOR},          // F6
            {TBBE_TERMINATOR},          // F7
            {TBBE_TERMINATOR},          // F8
            {TBBE_TERMINATOR},          // F9
            {TBBE_MENU},                // F10
            {TBBE_TERMINATOR},          // F11
            {TBBE_TERMINATOR},          // F12
        },
};

CBottomToolBar::CBottomToolBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    State = btbsCount;
    Padding.ButtonIconText = 1; // align text to the icon
    Padding.IconLeft = 2;       // space in front of the icon
    Padding.TextRight = 2;      // space behind the text
}

// naplni v poli BottomTBData promennou 'Text', kterou vycte z resourcu
// 'state' indicates the row in the BottomTBData array and 'BottomTBData' denotes a string with texts
// Texts for individual keys are separated by the character ';'
BOOL CBottomToolBar::InitDataResRow(CBottomTBStateEnum state, int textResID)
{
    CALL_STACK_MESSAGE2("CBottomToolBar::InitDataResRow(, %d)", textResID);
    char buff[BOTTOMTB_TEXT_MAX * 12];
    lstrcpyn(buff, LoadStr(textResID), BOTTOMTB_TEXT_MAX * 12);

    int index = 0;
    const char* begin = buff;
    const char* end;
    do
    {
        end = begin;
        while (*end != 0 && *end != ',')
            end++;
        if (index < 12)
        {
            int count = (int)(end - begin);
            if (count > BOTTOMTB_TEXT_MAX)
            {
                TRACE_E("Bottom Toolbar text state:" << state << " index:" << index << " exceeds " << BOTTOMTB_TEXT_MAX << " chars");
                count = BOTTOMTB_TEXT_MAX;
            }
            if (BottomTBData[state][index].Index == TBBE_TERMINATOR)
                count = 0;
            BottomTBData[state][index].TextLen = count;
            if (count > 0)
                memmove(BottomTBData[state][index].Text, begin, count);
            begin = end + 1;
        }
        else
        {
            TRACE_E("Bottom Toolbar text state:" << state << " index:" << index << " exceeds array range");
        }
        index++;
    } while (*end != 0);
    if (index != 12)
    {
        TRACE_E("Bottom Toolbar text state:" << state << " does not contains all 12 items.");
        return FALSE;
    }
    return TRUE;
}

CBottomTBStateEnum BottomTBStates[btbsCount] = {btbsNormal, btbsAlt, btbsCtrl, btbsShift, btbsCtrlShift, btbsAltShift, btbsMenu};

BOOL CBottomToolBar::InitDataFromResources()
{
    CALL_STACK_MESSAGE1("CBottomToolBar::InitDataFromResources()");
    int res[btbsCount] = {IDS_BTBTEXTS, IDS_BTBTEXTS_A, IDS_BTBTEXTS_C, IDS_BTBTEXTS_S, IDS_BTBTEXTS_CS, IDS_BTBTEXTS_AS, IDS_BTBTEXTS_MENU};
    int i;
    for (i = 0; i < btbsCount; i++)
    {
        if (!InitDataResRow(BottomTBStates[i], res[i]))
            return FALSE;
    }
    return TRUE;
}

void CBottomToolBar::SetFont()
{
    CToolBar::SetFont();
    SetMaxItemWidths();
}

BOOL CBottomToolBar::SetMaxItemWidths()
{
    CALL_STACK_MESSAGE1("CBottomToolBar::SetMaxItemWidths()");
    if (HWindow == NULL || Items.Count == 0)
        return TRUE;
    HFONT hOldFont = (HFONT)SelectObject(CacheBitmap->HMemDC, HFont);
    int i;
    for (i = 0; i < 12; i++)
    {
        WORD maxWidth = 0;
        int j;
        for (j = 0; j < btbsCount; j++)
        {
            const char* text = BottomTBData[j][i].Text;
            int textLen = BottomTBData[j][i].TextLen;

            RECT r;
            r.left = 0;
            r.top = 0;
            r.right = 0;
            r.bottom = 0;
            DrawText(CacheBitmap->HMemDC, text, textLen,
                     &r, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_NOPREFIX | DT_CALCRECT);
            if (r.right > maxWidth)
                maxWidth = (WORD)r.right;
        }
        maxWidth = 3 + BOTTOMBAR_CX + 1 + maxWidth + 3;
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_WIDTH;
        tii.Width = maxWidth;
        if (!SetItemInfo2(i, TRUE, &tii))
        {
            if (hOldFont != NULL)
                SelectObject(CacheBitmap->HMemDC, hOldFont);
            return FALSE;
        }
    }
    if (hOldFont != NULL)
        SelectObject(CacheBitmap->HMemDC, hOldFont);
    return TRUE;
}

BOOL CBottomToolBar::CreateWnd(HWND hParent)
{
    CALL_STACK_MESSAGE1("CBottomToolBar::CreateWnd()");
    if (!CToolBar::CreateWnd(hParent))
        return FALSE;

    RemoveAllItems();
    SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);
    int i;
    for (i = 0; i < 12; i++)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX | TLBI_MASK_WIDTH;
        tii.Style = TLBI_STYLE_SHOWTEXT | TLBI_STYLE_FIXEDWIDTH | TLBI_STYLE_NOPREFIX;
        tii.ImageIndex = i;
        tii.Width = 60;
        if (!InsertItem2(i, TRUE, &tii))
            return FALSE;
    }
    SetMaxItemWidths();
    return TRUE;
}

BOOL CBottomToolBar::SetState(CBottomTBStateEnum state)
{
    CALL_STACK_MESSAGE1("CBottomToolBar::SetState()");
    if (State == state)
        return TRUE;
    if (Items.Count != 12)
    {
        TRACE_E("Items.Count != 12");
        return FALSE;
    }
    BOOL empty = state == btbsCount;
    int i;
    for (i = 0; i < 12; i++)
    {
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_TEXT | TLBI_MASK_TEXTLEN | TLBI_MASK_ID | TLBI_MASK_ENABLER |
                   TLBI_MASK_STATE | TLBI_MASK_CUSTOMDATA;
        tii.State = 0; // enable the command - it will call UpdateItemsState(), which will disable what is needed
        char emptyBuff[] = "";
        tii.Text = empty ? emptyBuff : BottomTBData[state][i].Text;
        tii.TextLen = empty ? 0 : BottomTBData[state][i].TextLen;
        int btIndex = BottomTBData[state][i].Index;
        tii.CustomData = btIndex;
        tii.ID = 0;
        tii.Enabler = NULL;
        if (!empty && btIndex != TBBE_TERMINATOR)
        {
            tii.ID = ToolBarButtons[btIndex].ID;
            tii.Enabler = ToolBarButtons[btIndex].Enabler;
        }
        if (!SetItemInfo2(i, TRUE, &tii))
            return FALSE;
    }
    State = state;

    POINT p;
    GetCursorPos(&p);
    if (WindowFromPoint(p) == HWindow)
    {
        // ensure the restoration of the tooltip if necessary
        ScreenToClient(HWindow, &p);
        SetCurrentToolTip(NULL, 0);
        PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
    }

    return TRUE;
}

void CBottomToolBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CBottomToolBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    int tbbeIndex = tt->CustomData;
    if (tbbeIndex < TBBE_TERMINATOR)
    {
        lstrcpy(tt->Buffer, LoadStr(ToolBarButtons[tbbeIndex].ToolTipResID));
        PrepareToolTipText(tt->Buffer, TRUE);
    }
}
