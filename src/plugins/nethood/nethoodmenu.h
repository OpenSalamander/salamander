// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

/// Implements CPluginInterfaceForMenuExtAbstract interface for the Nethood plugin.
class CNethoodPluginInterfaceForMenuExt : public CPluginInterfaceForMenuExtAbstract
{
protected:
    class CNethoodFSInterface* CheckThrobberForPanel(
        __in int iPanel,
        __in CNethoodCache::Node node);

    /// Focuses item in panel by its name.
    /// \param iPanel Panel index. Either PANEL_LEFT or PANEL_RIGHT.
    /// \param pszItemName Name of the item.
    /// \return If the item was found and was focused, the return value
    ///         is true. Otherwise the return value is false.
    bool FocusItemInPanel(
        __in int iPanel,
        __in PCTSTR pszItemName);

public:
    /// Returns state of the menu item.
    /// \param id Menu item identifier.
    /// \param eventMask See CSalamanderConnectAbstract::AddMenuItem.
    /// \return Returns combination of MENU_ITEM_STATE_XXX values.
    virtual DWORD WINAPI GetMenuItemState(
        __in int id,
        __in DWORD eventMask);

    /// Executes menu command.
    /// \param salamander Pointer to the CSalamanderForOperationsAbstract
    ///        interface suitable for executing common operations.
    /// \param parent Handle of the parent window for dialog boxes.
    /// \param id Menu item identifier.
    /// \param eventMask See CSalamanderConnectAbstract::AddMenuItem.
    /// \return If the items in the panel should be deselected (the
    ///         operation was not Cancelled, some items might be Skipped)
    ///         the method should return TRUE. If the items should stay
    ///         selected the method should return FALSE.
    /// \warning If the command changes contents of a path (disk or FS) you
    ///          should call CSalamanderGeneralAbstract::PostChangeOnPathNotification
    ///          to notify panels without autorefresh feature and opened
    ///          FS (active and detached ones).
    /// \remark If the command works with files/directories on the path
    ///         currently shown in the panel or if it works with the path itself
    ///         you should call CSalamanderGeneralAbstract::SetUserWorkedOnPanelPath
    ///         for the current panel to put the current path to the list of
    ///         working directories (Alt+F12).
    virtual BOOL WINAPI ExecuteMenuItem(
        __in CSalamanderForOperationsAbstract* salamander,
        __in HWND parent,
        __in int id,
        __in DWORD eventMask);

    /// Displays help for the menu item.
    /// \param parent Handle of the parent window for dialog boxes.
    /// \param id Menu item identifier.
    /// \return If the method displays help for the menu item it should
    ///         return TRUE. If the method returns FALSE, Salamander displays
    ///         "Using Plugins" paragraph from the application's help.
    /// \remark To display the help for the menu item the user presses
    ///         Shift+F1 and then selects the menu item in the Plugins menu.
    virtual BOOL WINAPI HelpForMenuItem(
        __in HWND parent,
        __in int id);

    virtual void WINAPI BuildMenu(
        __in HWND parent,
        __in CSalamanderBuildMenuAbstract* salamander);
};
