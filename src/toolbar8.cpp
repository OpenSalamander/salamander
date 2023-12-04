// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "plugins.h"
#include "toolbar.h"

//*****************************************************************************
//
// CPluginsBar
//

CPluginsBar::CPluginsBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin)
{
    CALL_STACK_MESSAGE_NONE
    HPluginsIcons = NULL;
    HPluginsIconsGray = NULL;
}

CPluginsBar::~CPluginsBar()
{
    DestroyImageLists();
}

void CPluginsBar::DestroyImageLists()
{
    if (HPluginsIcons != NULL)
    {
        ImageList_Destroy(HPluginsIcons);
        HPluginsIcons = NULL;
    }
    if (HPluginsIconsGray != NULL)
    {
        ImageList_Destroy(HPluginsIconsGray);
        HPluginsIconsGray = NULL;
    }
}

BOOL CPluginsBar::CreatePluginButtons()
{
    CALL_STACK_MESSAGE1("CPluginsBar::CreateButtons()");
    if (HWindow == NULL)
        return FALSE;

    RemoveAllItems();

    SetStyle(TLB_STYLE_IMAGE /*| TLB_STYLE_TEXT*/);

    DestroyImageLists();

    HPluginsIcons = Plugins.CreateIconsList(FALSE);
    HPluginsIconsGray = Plugins.CreateIconsList(TRUE);

    SetImageList(HPluginsIconsGray);
    SetHotImageList(HPluginsIcons);

    Plugins.InitPluginsBar(this);
    /*
  TLBI_ITEM_INFO2 tii;
  int i;
  for (i = 0; i < Order.GetCount(); i++)
  {
    CPluginData *plugin = Plugins.Get(i);
    if (plugin == NULL || plugin->MenuItems.Count == 0) 
      continue;

    tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID;
    tii.Style = TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
    tii.ImageIndex = i;
    tii.ID = CM_PLUGINCMD_MIN + i; // do mainwnd3 prijde jako WM_USER_TBDROPDOWN
    InsertItem2(0xFFFFFFFF, TRUE, &tii);
  }
  */

    return TRUE;
}

int CPluginsBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    // i v pripade, ze nedrzime zadnou ikonu budeem vracet spravnou vysku
    int height = CToolBar::GetNeededHeight();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    int minH = 3 + iconSize + 3;
    if (height < minH)
        height = minH;
    return height;
}

void CPluginsBar::Customize()
{
    CALL_STACK_MESSAGE_NONE
    // zobrazim okno Plugins
    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CUSTOMIZEPLUGINS, 0);
}

void CPluginsBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CPluginsBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;

    int index = tt->ID - CM_PLUGINCMD_MIN;
    tt->Buffer[0] = 0;
    CPluginData* plugin = Plugins.Get(index);
    if (plugin != NULL)
        lstrcpy(tt->Buffer, plugin->Name);
}
