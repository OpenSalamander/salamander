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
#pragma pack(push, enter_include_spl_menu) // Ensure structures are independent of the current alignment settings.
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

class CSalamanderForOperationsAbstract;

// ****************************************************************************
// CSalamanderBuildMenuAbstract
//
// Set of Salamander methods for building a plugin menu.
//
// This is a subset of CSalamanderConnectAbstract methods; they behave the same and use the same constants (see CSalamanderConnectAbstract).

class CSalamanderBuildMenuAbstract
{
public:
    // ikony se zadavaji metodou CSalamanderBuildMenuAbstract::SetIconListForMenu, zbytek
    // popisu viz CSalamanderConnectAbstract::AddMenuItem
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ikony se zadavaji metodou CSalamanderBuildMenuAbstract::SetIconListForMenu, zbytek
    // popisu viz CSalamanderConnectAbstract::AddSubmenuStart
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // popis viz CSalamanderConnectAbstract::AddSubmenuEnd
    virtual void WINAPI AddSubmenuEnd() = 0;

    // Sets the plugin menu icon bitmap list. The bitmap list must be allocated using CSalamanderGUIAbstract::CreateIconList() and then created/filled via the CGUIIconListAbstract interface methods. Icons must be 16x16 pixels. Salamander takes ownership of the bitmap object; the plugin must not destroy it after this call. Salamander keeps it only in memory and does not persist it.
    virtual void WINAPI SetIconListForMenu(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForMenuExtAbstract
//

// flagy stavu polozek v menu (pro pluginy rozsireni menu)
#define MENU_ITEM_STATE_ENABLED 0x01 // enablovana, bez tohoto flagu je polozka disablovana
#define MENU_ITEM_STATE_CHECKED 0x02 // A 'check' or 'radio' marker precedes the item.
#define MENU_ITEM_STATE_RADIO 0x04   // bez MENU_ITEM_STATE_CHECKED se ignoruje, \
                                     // "radio" znacka, bez tohoto flagu "check" znacka
#define MENU_ITEM_STATE_HIDDEN 0x08  // polozka se v menu vubec nema objevit

class CPluginInterfaceForMenuExtAbstract
{
#ifdef INSIDE_SALAMANDER
private: // Guard against incorrect direct calls to methods (see CPluginInterfaceForMenuExtEncapsulation).
    friend class CPluginInterfaceForMenuExtEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // Returns the state of the menu item identified by 'id'. The return value is a combination of flags (see MENU_ITEM_STATE_XXX); see 'eventMask' in CSalamanderConnectAbstract::AddMenuItem.
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) = 0;

    // Executes the menu command identified by 'id' and 'eventMask' (see
    // CSalamanderConnectAbstract::AddMenuItem; 'salamander' is the set of Salamander
    // methods used to perform operations (NOTE: may be NULL; see method description
    // CSalamanderGeneralAbstract::PostMenuExtCommand), 'parent' je parent messageboxu,
    // returns TRUE if the panel selection should be cleared (Cancel was not used; Skip
    // may have been used), otherwise returns FALSE (no deselection will be performed);
    // NOTE: If the command makes changes to any path (disk/FS), it should use
    //        CSalamanderGeneralAbstract::PostChangeOnPathNotification pro informovani
    //        the panel without automatic refresh and the open FS (active or detached)
    // NOTE: if the command operates on files/directories from the current panel's path or
    //           directly on that path, it must call
    //           CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath pro aktualni panel,
    //           otherwise the path will not be added to the panel's list of working
    //           directories - List of Working Directories (Alt+F12)
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask) = 0;

    // Displays help for the menu command identified by 'id' (user presses Shift+F1, finds and selects this plugin's command from the Plugins menu). 'parent' is the parent messagebox. Returns TRUE if any help was shown; otherwise displays the 'Using Plugins' chapter from Salamander's help.
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id) = 0;

    // Function for the 'dynamic menu extension' (called only if FUNCTION_DYNAMICMENUEXT is set in SetBasicPluginData). Builds the plugin menu on load and again just before it is opened in the Plugins menu or on the Plugin bar (also before opening Keyboard Shortcuts from the Plugins Manager). Commands in the new menu should use the same IDs as the old menu so user-assigned hotkeys remain and commands can act as the 'Last Command' (see Plugins / Last Command). 'parent' is the parent messagebox; 'salamander' is the set of methods used to build the menu.
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_menu)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
