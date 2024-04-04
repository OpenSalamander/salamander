// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "mainwnd.h"
#include "usermenu.h"
#include "toolbar.h"
/*  #include "shellib.h"
#include "cfgdlg.h"
#include "execute.h"
extern "C" {
#include "shexreg.h"
}
#include "salshlib.h"*/

//*****************************************************************************
//
// CHotPathsBar
//

CHotPathsBar::CHotPathsBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CToolBar(hNotifyWindow, origin){
          CALL_STACK_MESSAGE_NONE}

      BOOL CHotPathsBar::CreateButtons()
{
    CALL_STACK_MESSAGE1("CHotPathsBar::CreateButtons()");
    if (HWindow == NULL)
        return FALSE;

    RemoveAllItems();

    SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);

    // add icons to a custom toolbar
    // Only insert items and submenus from the top level; the rest will expand as submenus
    int level = 0;
    TLBI_ITEM_INFO2 tii;
    int i;
    for (i = 0; i < HOT_PATHS_COUNT; i++)
    {
        char srcName[MAX_PATH];
        MainWindow->HotPaths.GetName(i, srcName, MAX_PATH);
        if (srcName[0] != 0)
        {
            tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_TEXT | TLBI_MASK_ICON | TLBI_MASK_ID;
            tii.Style = TLBI_STYLE_SHOWTEXT;
            char buff[200];
            lstrcpyn(buff, srcName, 200);
            DuplicateAmpersands(buff, 200);
            tii.Text = buff;
            tii.HIcon = HFavoritIcon;
            tii.ID = CM_ACTIVEHOTPATH_MIN + i;
            InsertItem2(0xFFFFFFFF, TRUE, &tii);
        }
        /*      switch (item->Type)
    {
      case umitSubmenuEnd:
      {
        level--;
        break;
      }

      case umitSeparator:
      {
        if (level == 0 && item->ShowInToolbar)
        {
          tii.Mask = TLBI_MASK_STYLE;
          tii.Style = TLBI_STYLE_SEPARATOR;
          InsertItem2(0xFFFFFFFF, TRUE, &tii);
        }
        break;
      }

      case umitItem:
      case umitSubmenuBegin:
      {
        if (level == 0 && item->ShowInToolbar)
        {
          tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_TEXT | TLBI_MASK_ICON | TLBI_MASK_ID;
          tii.Style = TLBI_STYLE_SHOWTEXT | TLBI_STYLE_NOPREFIX;
          if (item->Type == umitSubmenuBegin)
            tii.Style |= TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN;
          char buff[80];
          lstrcpyn(buff, item->ItemName, 80);
          RemoveAmpersands(buff);
          tii.Text = buff;
          tii.HIcon = item->HIcon;
          tii.ID = CM_USERMENU_MIN + i;
          InsertItem2(0xFFFFFFFF, TRUE, &tii);
        }
        if (item->Type == umitSubmenuBegin)
          level++;
        break;
      }
    }*/
    }
    return TRUE;
}

int CHotPathsBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    // and in case we don't hold any icon, we will return the correct height
    int height = CToolBar::GetNeededHeight();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    int minH = 3 + iconSize + 3;
    if (height < minH)
        height = minH;
    return height;
}

void CHotPathsBar::Customize()
{
    CALL_STACK_MESSAGE_NONE
    // let's unpack the HotPaths page
    PostMessage(MainWindow->HWindow, WM_USER_CONFIGURATION, 1, -1);
}

void CHotPathsBar::OnGetToolTip(LPARAM lParam)
{
    CALL_STACK_MESSAGE2("CHotPathsBar::OnGetToolTip(0x%IX)", lParam);
    TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
    int index = tt->ID - CM_ACTIVEHOTPATH_MIN;
    tt->Buffer[0] = 0;
    if (index >= 0 && index < HOT_PATHS_COUNT)
    {
        if (MainWindow->HotPaths.GetNameLen(index) > 0)
        {
            MainWindow->HotPaths.GetPath(index, tt->Buffer, TOOLTIP_TEXT_MAX);
        }
    }
}

LRESULT
CHotPathsBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CHotPathsBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    /*    switch (uMsg)
  {
    case WM_CREATE:
    {
      CUMDropTarget *dropTarget = new CUMDropTarget(this);
      if (dropTarget != NULL)
      {
        if (HANDLES(RegisterDragDrop(HWindow, dropTarget)) != S_OK)
        {
          TRACE_E("RegisterDragDrop error.");
        }
        dropTarget->Release();  // RegisterDragDrop called AddRef()
      }
      break;
    }

    case WM_DESTROY:
    {
      HANDLES(RevokeDragDrop(HWindow));
      break;
    }
  }*/
    return CToolBar::WindowProc(uMsg, wParam, lParam);
}
