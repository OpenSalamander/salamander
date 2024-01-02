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
#pragma pack(push, enter_include_spl_menu) // so that structures are independent of the set alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

class CSalamanderForOperationsAbstract;

//
// ****************************************************************************
// CSalamanderBuildMenuAbstract
//
// set of Salamander methods for building menu of plugin
//
// it is a subset of methods of CSalamanderConnectAbstract, methods behave the same,
// the same constants are used, for description see CSalamanderConnectAbstract

class CSalamanderBuildMenuAbstract
{
public:
    // icons are set by method CSalamanderBuildMenuAbstract::SetIconListForMenu, for the rest
    // of description see CSalamanderConnectAbstract::AddMenuItem
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // icons are set by method CSalamanderBuildMenuAbstract::SetIconListForMenu, for the rest
    // of description see CSalamanderConnectAbstract::AddSubmenuStart
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // for description see CSalamanderConnectAbstract::AddSubmenuEnd
    virtual void WINAPI AddSubmenuEnd() = 0;

    // sets bitmap with icons of plugin for menu; bitmap must be allocated by calling
    // CSalamanderGUIAbstract::CreateIconList() and then created and filled by methods
    // of CGUIIconListAbstract interface; icons must have dimensions 16x16 points;
    // Salamander takes the bitmap object into its management, plugin must not destroy it
    // after calling this method; Salamander keeps the bitmap only in memory, it is not
    // saved anywhere
    virtual void WINAPI SetIconListForMenu(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForMenuExtAbstract
//

// flags of menu item state (for menu extension plugins)
#define MENU_ITEM_STATE_ENABLED 0x01 // enabled, without this flag item is disabled
#define MENU_ITEM_STATE_CHECKED 0x02 // before item is "check" or "radio" tag
#define MENU_ITEM_STATE_RADIO 0x04   // without MENU_ITEM_STATE_CHECKED it is ignored, \
                                     // "radio" tag, without this flag "check" tag
#define MENU_ITEM_STATE_HIDDEN 0x08  // items is not supposed to be shown in menu

class CPluginInterfaceForMenuExtAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct calling of methods (see CPluginInterfaceForMenuExtEncapsulation)
    friend class CPluginInterfaceForMenuExtEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns state of menu item with ID 'id'; return value is combination of flags
    // (see MENU_ITEM_STATE_XXX); 'eventMask' see CSalamanderConnectAbstract::AddMenuItem
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) = 0;

    // executes menu command with ID 'id', 'eventMask' see CSalamanderConnectAbstract::AddMenuItem,
    // 'salamander' is set of usable methods of Salamander for performing operations
    // (WARNING: can be NULL, see description of method CSalamanderGeneralAbstract::PostMenuExtCommand),
    // 'parent' is parent messagebox, returns TRUE if selection in panel should be cleared
    // (Cancel was not used, Skip could be used), otherwise returns FALSE (selection is not cleared);
    // WARNING: If command causes changes on some path (disk/FS), it should use
    //          CSalamanderGeneralAbstract::PostChangeOnPathNotification for informing panel
    //          without automatic refresh and opened FS (active and disconnected)
    // NOTE: if command works with files/directories from path in current panel or even
    //       directly with this path, it is necessary to call
    //       CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath for current panel,
    //       otherwise path in this panel will not be inserted into the list of working
    //       directories - List of Working Directories (Alt+F12)
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask) = 0;

    // displays help for menu command with ID 'id' (user presses Shift+F1, finds menu
    // of this plugin in menu Plugins and selects command from it), 'parent' is parent
    // of messagebox, returns TRUE if some help was displayed, otherwise help from
    // Salamander - chapter "Using Plugins" is displayed
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id) = 0;

    // function for "dynamic menu extension", it is called only if you set FUNCTION_DYNAMICMENUEXT
    // to SetBasicPluginData; it builds menu of plugin when it is loaded and then again just
    // before it is opened in menu Plugins or on Plugin bar (in addition also before opening
    // Keyboard Shortcuts window from Plugins Manager); commands in new menu should have the
    // same ID as in old menu, so that user's hotkeys are preserved and so that they can be
    // used as Last Command (see Plugins / Last Command); 'parent' is parent of messagebox,
    // 'salamander' is set of methods for building menu
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_menu)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
