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
#pragma pack(push, enter_include_spl_menu) // so the structures are independent of the current alignment setting
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
// sada metod Salamandera pro stavbu menu pluginu
//
// jde o podmnozinu metod CSalamanderConnectAbstract, metody se stejne chovaji,
// pouzivaji se stejne konstanty, popis viz CSalamanderConnectAbstract

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

    // see CSalamanderConnectAbstract::AddSubmenuEnd for details
    virtual void WINAPI AddSubmenuEnd() = 0;

    // sets the plugin icon list for the menu; the icon list must be allocated by calling
    // CSalamanderGUIAbstract::CreateIconList() and then created and filled using the
    // CGUIIconListAbstract interface methods; the icon size must be 16x16 pixels;
    // Salamander takes ownership of the icon list object, so the plugin must not destroy it after calling
    // this function; Salamander keeps it only in memory and does not store it anywhere
    virtual void WINAPI SetIconListForMenu(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceForMenuExtAbstract
//

// menu item state flags (for menu extension plugins)
#define MENU_ITEM_STATE_ENABLED 0x01 // enabled; without this flag the item is disabled
#define MENU_ITEM_STATE_CHECKED 0x02 // a "check" or "radio" mark is shown before the item
#define MENU_ITEM_STATE_RADIO 0x04   // ignored without MENU_ITEM_STATE_CHECKED, \
                                     // "radio" mark; without this flag a "check" mark
#define MENU_ITEM_STATE_HIDDEN 0x08  // the item should not appear in the menu at all

class CPluginInterfaceForMenuExtAbstract
{
#ifdef INSIDE_SALAMANDER
private: // protection against incorrect direct method calls (see CPluginInterfaceForMenuExtEncapsulation)
    friend class CPluginInterfaceForMenuExtEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // returns the state of the menu item with identifier 'id'; the return value is a combination
    // of flags (see MENU_ITEM_STATE_XXX); for 'eventMask' see CSalamanderConnectAbstract::AddMenuItem
    virtual DWORD WINAPI GetMenuItemState(int id, DWORD eventMask) = 0;

    // executes the menu command with identifier 'id'; for 'eventMask' see
    // CSalamanderConnectAbstract::AddMenuItem, 'salamander' is the set of available
    // Salamander methods for performing operations (WARNING: it may be NULL, see the description of the method
    // CSalamanderGeneralAbstract::PostMenuExtCommand), 'parent' je parent messageboxu,
    // returns TRUE if the selection in the panel should be cleared (Cancel was not used; Skip may have
    // been used), otherwise returns FALSE (no deselection is performed);
    // POZOR: Pokud prikaz zpusobi zmeny na nejake ceste (diskove/FS), mel by pouzit
    //        CSalamanderGeneralAbstract::PostChangeOnPathNotification pro informovani
    //        the panel without an automatic refresh and any open FSs (active and disconnected)
    // NOTE: if the command works with files/directories from the path in the current panel or
    //           even directly with this path, it is necessary to call
    //           CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath pro aktualni panel,
    //           otherwise the path in this panel will not be added to the list of working
    //           directories - List of Working Directories (Alt+F12)
    virtual BOOL WINAPI ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander, HWND parent,
                                        int id, DWORD eventMask) = 0;

    // displays help for the menu command with identifier 'id' (the user presses Shift+F1,
    // finds this plugin's menu in the Plugins menu, and selects the command); 'parent' is the parent
    // of the message box; returns TRUE if any help was displayed, otherwise the "Using Plugins"
    // chapter from Salamander help is shown
    virtual BOOL WINAPI HelpForMenuItem(HWND parent, int id) = 0;

    // function for "dynamic menu extension"; called only if FUNCTION_DYNAMICMENUEXT is specified in
    // SetBasicPluginData; builds the plugin menu when the plugin is loaded, and then again just before
    // it is opened in the Plugins menu or on the Plugin bar (also before the Keyboard
    // Shortcuts window is opened from Plugins Manager); commands in the new menu should use the same ID
    // as in the old one so user-assigned hotkeys stay assigned and they can still work as the last
    // used command (see Plugins / Last Command); 'parent' is the parent of the message box, 'salamander'
    // is the set of methods for building the menu
    virtual void WINAPI BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_menu)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
