// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"

#define UPDOWN_TIMER_ID 1  // timer id
#define UPDOWN_TIMER_TO 50 // time out [ms]

//*****************************************************************************
//
// CMenuPopup
//

CMenuPopup::CMenuPopup(DWORD id)
    : CWindow(ooStatic), Items(40, 20)
{
    CALL_STACK_MESSAGE_NONE
    HWindowsMenu = NULL;
    SharedRes = NULL;
    HImageList = NULL;
    HHotImageList = NULL;
    ImageWidth = 0;
    ImageHeight = 0;
    Style = 0;
    TrackFlags = 0;
    ID = id;
    FirstPopup = NULL;
    MinWidth = 0;
    ModifyMode = FALSE;
    UpDownTimerRunnig = FALSE;
    SkillLevel = MENU_LEVEL_ADVANCED;
    Cleanup();
}

void CMenuPopup::Cleanup()
{
    CALL_STACK_MESSAGE1("CMenuPopup::Cleanup()");
    ZeroMemory(&WindowRect, sizeof(WindowRect));
    TotalHeight = 0;
    Width = 0;
    Height = 0;
    TopItemY = 0;
    UpArrowVisible = FALSE;
    DownArrowVisible = FALSE;
    OpenedSubMenu = NULL;
    Closing = FALSE;
    if (UpDownTimerRunnig)
    {
        KillTimer(HWindow, UPDOWN_TIMER_ID);
        UpDownTimerRunnig = FALSE;
    }
    ResetMouseWheelAccumulator();
}

HWND CMenuPopup::PopupWindowFromPoint(POINT point)
{
    HWND hWindow = WindowFromPoint(point);

    // pokud jde o MenuBar, udelame vyjimku (je to child window)
    if (hWindow != NULL && (SharedRes == NULL ||
                            SharedRes->MenuBar == NULL ||
                            SharedRes->MenuBar->HWindow != hWindow))
    {
        DWORD style = 0;
        do
        {
            style = (DWORD)GetWindowLongPtr(hWindow, GWL_STYLE);
            if (style & WS_CHILD)
            {
                HWND hParent = GetParent(hWindow);
                if (hParent == NULL)
                    break;
                else
                    hWindow = hParent;
            }
        } while (style & WS_CHILD);
    }
    return hWindow;
}

BOOL CMenuPopup::SetStyle(DWORD style)
{
    CALL_STACK_MESSAGE_NONE
    Style = style;
    return TRUE;
}

void CMenuPopup::SetMinWidth(int minWidth)
{
    CALL_STACK_MESSAGE_NONE
    if (minWidth < 0)
        minWidth = 0;
    if (minWidth > 5000)
        minWidth = 50000;
    MinWidth = minWidth - 6;
}

void CMenuPopup::SetPopupID(DWORD id)
{
    ID = id;
}

DWORD
CMenuPopup::GetPopupID()
{
    return ID;
}

void CMenuPopup::AssignHotKeys()
{
    CALL_STACK_MESSAGE1("CMenuPopup::AssignHotKeys()");

    // udava zda je patricny znak uz prirazen
    BOOL assigned[256];

    // dame k dispozici pouze sikovne znaky
    int i;
    for (i = 0; i < 256; i++)
        assigned[i] = !IsCharAlpha(i);

    // v prvni fazi objehneme vsechny polozky menu a detekujeme jiz pouzite horke klavesy
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        item->Temp = FALSE;
        if (!(item->Type & MENU_TYPE_STRING) || (item->Type & MENU_TYPE_OWNERDRAW))
            continue;

        const char* p = item->String;
        if (*p == NULL)
            continue;

        BOOL hasHotKey = FALSE;
        while (*p != 0)
        {
            if (*p == '&' && *(p + 1) != '&' && *(p + 1) != 0)
            {
                assigned[UpperCase[*(p + 1)]] = TRUE;
                hasHotKey = TRUE;
                break;
            }
            p++;
        }
        if (!hasHotKey)
            item->Temp = TRUE;
    }

    // polozkam bez horke klavesy zkusime nejakou priradit
    char buff[1000];
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!item->Temp || (item->Flags & MENU_FLAG_NOHOTKEY) != 0)
            continue;

        BOOL keyAssigned = FALSE;
        int i2;
        for (i2 = 0; !keyAssigned && i2 < 2; i2++)
        {
            const char* p = item->String;
            if (*p == NULL)
                continue;
            while (*p != 0)
            {
                // v prvním kole se pokusime vyhnout znakum s diakritikou, aby uzivatel nemusel prepinat z anglicke klavesnice
                // viz https://forum.altap.cz/viewtopic.php?f=23&t=4025
                WCHAR buffw[10];
                buffw[1] = 0; // abychom prosli ve druhem kole

                if (i2 == 0)
                {
                    char buffa[2];
                    buffa[0] = *p;
                    buffa[1] = 0;
                    MultiByteToWideChar(CP_ACP, MB_COMPOSITE, buffa, -1, buffw, 10); // "á" -> "a´"
                }

                char upper = UpperCase[*p];
                if (buffw[1] == 0 && !assigned[upper])
                {
                    int len = (int)strlen(item->String);
                    if (len < 1000 - 1) // musime se vejit do bufferu
                    {
                        int pos = (int)(p - item->String);
                        if (pos > 0)
                            memcpy(buff, item->String, len);
                        buff[pos] = '&';
                        memcpy(buff + pos + 1, item->String + pos, len - pos);
                        // pokud se podarilo vlozit hot key, obsadime pole assigned
                        if (item->SetText(buff, len + 1))
                            assigned[upper] = TRUE;
                    }
                    keyAssigned = TRUE;
                    break;
                }
                p++;
            }
        }
    }
}

void CMenuPopup::SetSelectedItemIndex(int index)
{
    CALL_STACK_MESSAGE_NONE
    SelectedItemIndex = index;
}

BOOL CMenuPopup::BeginModifyMode()
{
    if (ModifyMode)
    {
        TRACE_E("MenuPopup is already in ModifyMode");
        return FALSE;
    }
    // pokud mame otevrene nejake submenu, zavreme ho
    if (OpenedSubMenu != NULL)
        CloseOpenedSubmenu();

    ModifyMode = TRUE;
    return TRUE;
}

BOOL CMenuPopup::EndModifyMode()
{
    if (!ModifyMode)
    {
        TRACE_E("MenuPopup is not in ModifyMode");
        return FALSE;
    }
    if (HWindow != NULL)
    {
        // osetrim meze
        if (SelectedItemIndex >= Items.Count)
            SelectedItemIndex = Items.Count - 1;
        if (SelectedItemIndex >= 0 && SelectedItemIndex < Items.Count)
        {
            // vybrana polozka je separator - nalezneme nonseparator
            if (Items[SelectedItemIndex]->Type & MENU_TYPE_SEPARATOR)
            {
                int index = SelectedItemIndex;
                if (!FindNextItemIndex(index, FALSE, &SelectedItemIndex) &&
                    !FindNextItemIndex(index, TRUE, &SelectedItemIndex))
                    SelectedItemIndex = -1;
            }
        }
        // napocitam rozmery menu
        LayoutColumns();

        // napocitam rozmery okna
        RECT clipRect;
        RECT r;
        GetWindowRect(HWindow, &r);
        MultiMonGetClipRectByRect(&r, &clipRect, NULL);
        int width = Width + 6;
        int height = TotalHeight + 6;

        if (r.top + height > clipRect.bottom)
            height = clipRect.bottom - r.top;

        Height = height - 6;
        TopItemY = 0;

        UpArrowVisible = FALSE;
        if (Height < TotalHeight)
            DownArrowVisible = TRUE;

        if (SelectedItemIndex != -1)
            EnsureItemVisible(SelectedItemIndex);

        if (Style & MENU_POPUP_UPDATESTATES)
            UpdateItemsState();

        // upravime rozmer okna
        InvalidateRect(HWindow, NULL, TRUE);
        SetWindowPos(HWindow, NULL, 0, 0, width, height,
                     SWP_NOMOVE | SWP_NOACTIVATE | SWP_NOZORDER);
        UpdateWindow(HWindow);
        /*
    // nechame aktualizovat aktualni polozku
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    ScreenToClient(HWindow, &cursorPos);
    SharedRes->LastMouseMove.x = cursorPos.x - 1; // vyradime podminku
    SendMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
*/
    }
    ModifyMode = FALSE;
    return TRUE;
}

void CMenuPopup::SetSkillLevel(DWORD skillLevel)
{
    if (skillLevel != MENU_LEVEL_BEGINNER &&
        skillLevel != MENU_LEVEL_INTERMEDIATE &&
        skillLevel != MENU_LEVEL_ADVANCED)
    {
        TRACE_E("CMenuPopup::SetSkillLevel disallowed skillLevel:" << skillLevel);
        return;
    }
    SkillLevel = skillLevel;
}

void CMenuPopup::RemoveAllItems()
{
    CALL_STACK_MESSAGE1("CMenuPopup::RemoveAllItems()");
    Cleanup();
    Items.DestroyMembers();
}

BOOL CMenuPopup::RemoveItemsRange(int firstIndex, int lastIndex)
{
    CALL_STACK_MESSAGE3("CMenuPopup::RemoveItemsRange(%d, %d)", firstIndex, lastIndex);
    int count = Items.Count;
    if (firstIndex < 0 || lastIndex < 0 || firstIndex >= count || lastIndex >= count ||
        lastIndex < firstIndex)
    {
        TRACE_E("CMenuPopup::RemoveItemsRange failed: firstIndex:" << firstIndex << " lastIndex:" << lastIndex);
        return FALSE;
    }
    int i;
    for (i = lastIndex; i >= firstIndex; i--)
        Items.Delete(i);
    return TRUE;
}

void CMenuPopup::SetImageList(HIMAGELIST hImageList, BOOL subMenu)
{
    CALL_STACK_MESSAGE1("CMenuPopup::SetImageList()");
    HImageList = hImageList;
    if (HImageList != NULL)
    {
        ImageList_GetIconSize(HImageList, &ImageWidth, &ImageHeight);
    }
    else
    {
        ImageWidth = 0;
        ImageHeight = 0;
    }
    if (subMenu)
    {
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* item = Items[i];
            if (item->SubMenu != NULL)
                item->SubMenu->SetImageList(hImageList, TRUE);
        }
    }
}

HIMAGELIST
CMenuPopup::GetImageList()
{
    return HImageList;
}

void CMenuPopup::SetHotImageList(HIMAGELIST hImageList, BOOL subMenu)
{
    CALL_STACK_MESSAGE_NONE
    HHotImageList = hImageList;
    /*
  if (HImageList != NULL)
  {
    ImageList_GetIconSize(HImageList, &ImageWidth, &ImageHeight);
  }
  else
  {
    ImageWidth = 0;
    ImageHeight = 0;
  }
*/
    if (subMenu)
    {
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* item = Items[i];
            if (item->SubMenu != NULL)
                item->SubMenu->SetHotImageList(hImageList, TRUE);
        }
    }
}

HIMAGELIST
CMenuPopup::GetHotImageList()
{
    return HHotImageList;
}

void CMenuPopup::UpdateItemsState()
{
    CALL_STACK_MESSAGE1("CMenuPopup::UpdateItemsState()");
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];

        if (item->Enabler != NULL)
        {
            // bit TLBI_STATE_GRAYED je rizen
            BOOL enabled = (item->State & MENU_STATE_GRAYED) == 0;
            BOOL enabledSrc = *item->Enabler != 0;
            if (enabled != enabledSrc)
            {
                DWORD newState = item->State;
                if (enabledSrc)
                    newState &= ~MENU_STATE_GRAYED;
                else
                    newState |= MENU_STATE_GRAYED;

                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_STATE;
                mii.State = newState;
                SetItemInfo(i, TRUE, &mii);
            }
        }
    }
}

int CMenuPopup::FindItemPosition(DWORD id)
{
    CALL_STACK_MESSAGE_NONE
    int i;
    for (i = 0; i < Items.Count; i++)
        if (Items[i]->ID == id)
            return i;
    return -1;
}

BOOL CMenuPopup::InsertItem(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii)
{
    CALL_STACK_MESSAGE3("CMenuPopup::InsertItem(0x%X, %d, )", position, byPosition);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        if (position == -1 || position > (DWORD)Items.Count)
            newPos = Items.Count;
        else
            newPos = position;
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::InsertItem menu item with id=" << position << " was not found");
            return FALSE;
        }
    }

    // naalokujeme polozku
    CMenuItem* item = new CMenuItem();
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    // vlozime polozku do pole
    Items.Insert(newPos, item);
    if (!Items.IsGood())
    {
        Items.ResetState();
        delete item;
        return FALSE;
    }

    // nastavime data
    if (!SetItemInfo(newPos, TRUE, mii))
    {
        Items.Delete(newPos);
        return FALSE;
    }

    return TRUE;
}

BOOL CMenuPopup::SetItemInfo(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii)
{
    CALL_STACK_MESSAGE3("CMenuPopup::SetItemInfo(0x%X, %d, )", position, byPosition);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::SetItemInfo menu item with position=" << position << " is not available");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::SetItemInfo menu item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CMenuItem* item = Items[newPos];

    if (mii->Mask & MENU_MASK_TYPE)
    {
        item->Type = mii->Type;
    }

    if (mii->Mask & MENU_MASK_STATE)
    {
        item->State = mii->State;
        if (item->State & MENU_STATE_DEFAULT)
        {
            // zadna jina polozka nesmi mit nahozeny tento flag
            int i;
            for (i = 0; i < Items.Count; i++)
            {
                if (i != newPos)
                {
                    CMenuItem* tmpItem = Items[i];
                    tmpItem->State &= ~MENU_STATE_DEFAULT;
                }
            }
        }
    }

    if (mii->Mask & MENU_MASK_ID)
        item->ID = mii->ID;

    if (mii->Mask & MENU_MASK_SUBMENU)
    {
        item->SubMenu = (CMenuPopup*)mii->SubMenu;
        if (item->SubMenu != NULL)
        {
            // tohle nedelalo dobrotu, propagovala se 0 i v pripade, ze polozka mela
            // prirazene nejake ID
            /*
      if (mii->Mask & MENU_MASK_ID)
        item->SubMenu->ID = mii->ID;
      else
        item->SubMenu->ID = 0;
      */

            // radeji budeme vzdy propagovat ID polozky
            item->SubMenu->ID = item->ID;
        }
    }

    if (mii->Mask & MENU_MASK_CHECKMARKS)
    {
        item->HBmpChecked = mii->HBmpChecked;
        item->HBmpUnchecked = mii->HBmpUnchecked;
    }

    if (mii->Mask & MENU_MASK_IMAGEINDEX)
        item->ImageIndex = mii->ImageIndex;

    if (mii->Mask & MENU_MASK_ICON)
        item->HIcon = mii->HIcon;

    if (mii->Mask & MENU_MASK_OVERLAY)
        item->HOverlay = mii->HOverlay;

    if (mii->Mask & MENU_MASK_CUSTOMDATA)
        item->CustomData = mii->CustomData;

    if (mii->Mask & MENU_MASK_ENABLER)
        item->Enabler = mii->Enabler;

    if (mii->Mask & MENU_MASK_SKILLLEVEL)
        item->SkillLevel = mii->SkillLevel;

    if (mii->Mask & MENU_MASK_STRING)
    {
        // typ string ma alokovany retezec, ktery budeme uvolnovat
        const char* p = mii->String;
        if (p == NULL)
        {
            TRACE_E("CMenuPopup::SetItemInfo menu item with id=" << position << " obtained mii->String == NULL.");
            p = "";
        }
        if (!item->SetText(p))
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }

    // ochrana pred zmatecnejma datama
    if (item->Type & MENU_TYPE_STRING && item->String == NULL)
    {
        if (!item->SetText(""))
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }

    if (mii->Mask & MENU_MASK_BITMAP)
        item->HBmpItem = mii->HBmpItem;

    if (mii->Mask & MENU_MASK_FLAGS)
        item->Flags = mii->Flags;

    return TRUE;
}

BOOL CMenuPopup::GetItemInfo(DWORD position, BOOL byPosition, MENU_ITEM_INFO* mii)
{
    CALL_STACK_MESSAGE3("CMenuPopup::GetItemInfo(0x%X, %d, )", position,
                        byPosition);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::GetItemInfo menu item with position=" << position << " is not available");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::GetItemInfo menu item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CMenuItem* item = Items[newPos];

    if (mii->Mask & MENU_MASK_TYPE)
        mii->Type = item->Type;

    if (mii->Mask & MENU_MASK_STATE)
        mii->State = item->State;

    if (mii->Mask & MENU_MASK_ID)
        mii->ID = item->ID;

    if (mii->Mask & MENU_MASK_SUBMENU)
        mii->SubMenu = item->SubMenu;

    if (mii->Mask & MENU_MASK_CHECKMARKS)
    {
        mii->HBmpChecked = item->HBmpChecked;
        mii->HBmpUnchecked = item->HBmpUnchecked;
    }

    if (mii->Mask & MENU_MASK_IMAGEINDEX)
        mii->ImageIndex = item->ImageIndex;

    if (mii->Mask & MENU_MASK_ICON)
        mii->HIcon = item->HIcon;

    if (mii->Mask & MENU_MASK_OVERLAY)
        mii->HOverlay = item->HOverlay;

    if (mii->Mask & MENU_MASK_CUSTOMDATA)
        mii->CustomData = item->CustomData;

    if (mii->Mask & MENU_MASK_ENABLER)
        mii->Enabler = item->Enabler;

    if (mii->Mask & MENU_MASK_SKILLLEVEL)
        mii->SkillLevel = item->SkillLevel;

    if (mii->Mask & MENU_MASK_STRING)
    {
        if (mii->String == NULL && item->String != NULL)
            mii->StringLen = lstrlen(item->String);
        else
            lstrcpyn(mii->String, item->String, mii->StringLen);
    }

    if (mii->Mask & MENU_MASK_BITMAP)
        mii->HBmpItem = item->HBmpItem;

    if (mii->Mask & MENU_MASK_FLAGS)
        mii->Flags = item->Flags;

    return TRUE;
}

BOOL CMenuPopup::CheckItem(DWORD position, BOOL byPosition, BOOL checked)
{
    CALL_STACK_MESSAGE4("CMenuPopup::CheckItem(0x%X, %d, %d)", position,
                        byPosition, checked);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::CheckItem menu item with position=" << position << " is not available");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::CheckItem menu item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CMenuItem* item = Items[newPos];
    DWORD newState = item->State & ~MENU_STATE_CHECKED;
    if (checked)
        newState |= MENU_STATE_CHECKED;

    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_STATE;
    mii.State = newState;
    return SetItemInfo(newPos, TRUE, &mii);
}

BOOL CMenuPopup::CheckRadioItem(DWORD positionFirst, DWORD positionLast, DWORD positionCheck, BOOL byPosition)

{
    CALL_STACK_MESSAGE5("CMenuPopup::CheckRadioItem(0x%X, 0x%X, 0x%X, %d)",
                        positionFirst, positionLast, positionCheck, byPosition);
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        int firstPos, lastPos, checkPos;
        firstPos = positionFirst;
        lastPos = positionLast;
        checkPos = positionCheck;
        if (firstPos < 0 || firstPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::CheckRadioItem menu item with position=" << firstPos << " is not available");
            return FALSE;
        }
        if (lastPos < 0 || lastPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::CheckRadioItem menu item with position=" << lastPos << " is not available");
            return FALSE;
        }
        if (checkPos < 0 || checkPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::CheckRadioItem menu item with position=" << checkPos << " is not available");
            return FALSE;
        }

        if (firstPos > lastPos)
        {
            int tmp = firstPos;
            firstPos = lastPos;
            lastPos = tmp;
        }

        BOOL ret = TRUE;
        int i;
        for (i = firstPos; i <= lastPos; i++)
        {
            CMenuItem* item = Items[i];
            DWORD newState = item->State & ~MENU_STATE_CHECKED;
            DWORD newType = item->Type & ~MENU_TYPE_RADIOCHECK;
            if (i == checkPos)
            {
                newState |= MENU_STATE_CHECKED;
                newType |= MENU_TYPE_RADIOCHECK;
            }

            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_STATE | MENU_MASK_TYPE;
            mii.State = newState;
            mii.Type = newType;
            ret &= SetItemInfo(i, TRUE, &mii);
            if (!ret)
                break;
        }
        return ret;
    }
    else
    {
        DWORD firstID, lastID, checkID;
        firstID = positionFirst;
        lastID = positionLast;
        checkID = positionCheck;

        if (firstID > lastID)
        {
            int tmp = firstID;
            firstID = lastID;
            lastID = tmp;
        }

        BOOL ret = TRUE;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* item = Items[i];
            if (item->ID >= firstID && item->ID <= lastID)
            {
                DWORD newState = item->State & ~MENU_STATE_CHECKED;
                DWORD newType = item->Type & ~MENU_TYPE_RADIOCHECK;
                if (item->ID == checkID)
                {
                    newState |= MENU_STATE_CHECKED;
                    newType |= MENU_TYPE_RADIOCHECK;
                }

                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_STATE | MENU_MASK_TYPE;
                mii.State = newState;
                mii.Type = item->Type | MENU_TYPE_RADIOCHECK;
                ret &= SetItemInfo(i, TRUE, &mii);
            }
            if (!ret)
                break;
        }
        return ret;
    }
}

BOOL CMenuPopup::SetDefaultItem(DWORD position, BOOL byPosition)
{
    CALL_STACK_MESSAGE3("CMenuPopup::SetDefaultItem(0x%X, %d)", position, byPosition);
    int newPos;
    if (position == -1)
    {
        newPos = -1;
    }
    else
    {
        // vyhledame pozici, ktera bude oznacena jako default
        if (byPosition)
        {
            newPos = position;
            if (newPos < 0 || newPos >= Items.Count)
            {
                TRACE_E("CMenuPopup::SetDefaultItem menu item with position=" << position << " is not available");
                return FALSE;
            }
        }
        else
        {
            newPos = FindItemPosition(position);
            if (newPos == -1)
            {
                TRACE_E("CMenuPopup::SetDefaultItem menu item with id=" << position << " was not found");
                return FALSE;
            }
        }
    }

    BOOL ret = TRUE;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        DWORD newState = item->State & ~MENU_STATE_DEFAULT;
        if (newPos != -1 && newPos == i)
            newState |= MENU_STATE_DEFAULT;

        MENU_ITEM_INFO mii;
        mii.Mask = MENU_MASK_STATE;
        mii.State = newState;
        ret &= SetItemInfo(i, TRUE, &mii);
        if (!ret)
            break;
    }

    return ret;
}

BOOL CMenuPopup::EnableItem(DWORD position, BOOL byPosition, BOOL enabled)
{
    CALL_STACK_MESSAGE4("CMenuPopup::EnableItem(0x%X, %d, %d)", position,
                        byPosition, enabled);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::EnableItem menu item with position=" << position << " is not available");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::EnableItem menu item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CMenuItem* item = Items[newPos];
    DWORD newState = item->State & ~MENU_STATE_GRAYED;
    if (!enabled)
        newState |= MENU_STATE_GRAYED;

    MENU_ITEM_INFO mii;
    mii.Mask = MENU_MASK_STATE;
    mii.State = newState;
    return SetItemInfo(newPos, TRUE, &mii);
}

CGUIMenuPopupAbstract*
CMenuPopup::GetSubMenu(DWORD position, BOOL byPosition)
{
    CALL_STACK_MESSAGE3("CMenuPopup::GetSubMenu(0x%X, %d)", position, byPosition);
    int newPos;
    // vyhledame pozici, kam prijde nova polozka
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CMenuPopup::GetSubMenu menu item with position=" << position << " is not available");
            return NULL;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CMenuPopup::GetSubMenu menu item with id=" << position << " was not found");
            return NULL;
        }
    }
    return Items[newPos]->SubMenu;
}

CMenuPopupHittestEnum
CMenuPopup::HitTest(const POINT* point, int* userData)
{
    CALL_STACK_MESSAGE_NONE
    /*
  if (point->x < WindowRect.left || point->x >= WindowRect.right ||
      point->y < WindowRect.top || point->y >= WindowRect.bottom)
    return mphOutside;
  if (point->x < WindowRect.left + 3 || point->x >= WindowRect.right - 3 ||
      point->y < WindowRect.top + 3 || point->y >= WindowRect.bottom - 3)
    return mphBorder;
*/
    if (point->x < 0 || point->x > Width || point->y < 0 || point->y > Height)
        return mphBorderOrOutside;

    if (UpArrowVisible && point->y <= UPDOWN_ITEM_HEIGHT)
        return mphUpArrow;

    if (DownArrowVisible && point->y > Height - UPDOWN_ITEM_HEIGHT)
        return mphDownArrow;

    POINT p = *point;
    //  ScreenToClient(HWindow, &p);
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;
        if (p.y >= TopItemY + item->YOffset &&
            p.y < TopItemY + item->YOffset + item->Height)
        {
            if (DownArrowVisible && TopItemY + item->YOffset + item->Height > Height - UPDOWN_ITEM_HEIGHT)
                return mphBorderOrOutside; // castecne viditelna polozka -> zadna polozka
            *userData = i;
            return mphItem;
        }
    }
    return mphBorderOrOutside;
}

BOOL CMenuPopup::GetItemRect(int index, RECT* rect)
{
    CALL_STACK_MESSAGE_NONE
    if (index < 0 || index >= Items.Count)
    {
        TRACE_E("Index is out of range. Index=" << index);
        return FALSE;
    }
    POINT p;
    p.x = 0;
    p.y = 0;
    ClientToScreen(HWindow, &p);

    CMenuItem* item = Items[index];

    rect->left = p.x;
    rect->top = p.y + TopItemY + item->YOffset;
    rect->right = p.x + Width;
    rect->bottom = p.y + TopItemY + item->YOffset + item->Height;
    return TRUE;
}

BOOL CMenuPopup::FillMenuHandle(HMENU hMenu)
{
    CALL_STACK_MESSAGE1("CMenuPopup::FillMenuHandle()");
    MENUITEMINFO mii;
    ZeroMemory(&mii, sizeof(mii));
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_ID | MIIM_TYPE;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        mii.fType = MFT_STRING;
        mii.wID = item->ID;
        mii.dwTypeData = item->String;
        if (!InsertMenuItem(hMenu, i, TRUE, &mii))
        {
            TRACE_E("InsertMenuItem failed");
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CMenuPopup::GetStatesFromHWindowsMenu(HMENU hMenu)
{
    CALL_STACK_MESSAGE1("CMenuPopup::GetStatesFromHWindowsMenu()");
    if (Items.Count != GetMenuItemCount(hMenu))
    {
        TRACE_E("Items.Count != GetMenuItemCount(hMenu)");
        return FALSE;
    }
    MENUITEMINFO mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_STATE;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        DWORD newState = 0;
        GetMenuItemInfo(hMenu, i, TRUE, &mii);
        if (mii.fState & MFS_CHECKED)
            newState |= MENU_STATE_CHECKED;
        if (mii.fState & MFS_DEFAULT)
            newState |= MENU_STATE_DEFAULT;
        if (mii.fState & MFS_GRAYED || mii.fState & MFS_DISABLED)
            newState |= MENU_STATE_GRAYED;

        MENU_ITEM_INFO mii2;
        mii2.Mask = MENU_MASK_STATE;
        mii2.State = newState;
        SetItemInfo(i, TRUE, &mii2);
    }
    return TRUE;
}

// Tato verze slape od W2K, my ji vyuzivame od Visty, kde MS zavedli alpha blended ikonky do menu
typedef struct
{
    UINT cbSize;
    UINT fMask;
    UINT fType;            // used if MIIM_TYPE (4.0) or MIIM_FTYPE (>4.0)
    UINT fState;           // used if MIIM_STATE
    UINT wID;              // used if MIIM_ID
    HMENU hSubMenu;        // used if MIIM_SUBMENU
    HBITMAP hbmpChecked;   // used if MIIM_CHECKMARKS
    HBITMAP hbmpUnchecked; // used if MIIM_CHECKMARKS
    ULONG_PTR dwItemData;  // used if MIIM_DATA
    LPSTR dwTypeData;      // used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
    UINT cch;              // used if MIIM_TYPE (4.0) or MIIM_STRING (>4.0)
    HBITMAP hbmpItem;      // used if MIIM_BITMAP
} MENUITEMINFOA_NEW, FAR* LPMENUITEMINFOA_NEW;

#define MIIM_STRING 0x00000040
#define MIIM_BITMAP 0x00000080
#define MIIM_FTYPE 0x00000100

BOOL CMenuPopup::LoadFromHandle()
{
    CALL_STACK_MESSAGE1("CMenuPopup::LoadFromHandle()");
    char buff[2048];
    MENUITEMINFOA_NEW mii;
    mii.cbSize = sizeof(mii);
    mii.fMask = MIIM_CHECKMARKS | MIIM_DATA | MIIM_ID | MIIM_STATE | MIIM_SUBMENU | MIIM_FTYPE | MIIM_BITMAP | MIIM_STRING;
    // vsechny polozky v menu prevedu na nase data
    int count = GetMenuItemCount(HWindowsMenu);
    Items.DestroyMembers();
    int i;
    for (i = 0; i < count; i++)
    {
        mii.dwTypeData = buff;
        mii.cch = 2048;
        // vytahnu vsechny dostupne informace o polozce z menu
        if (!GetMenuItemInfo(HWindowsMenu, i, TRUE, (MENUITEMINFO*)&mii))
        {
            TRACE_E("GetMenuItemInfo failed");
            return FALSE;
        }

        CMenuItem* item = new CMenuItem();
        if (item == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        int index = Items.Add(item);
        if (!Items.IsGood())
        {
            Items.ResetState();
            delete item;
            return FALSE;
        }

        // konverze promenne mii.fType
        if (mii.fType & MFT_BITMAP)
            item->Type |= MENU_TYPE_BITMAP;
        else if (mii.fType & MFT_SEPARATOR)
            item->Type |= MENU_TYPE_SEPARATOR;
        else
            item->Type |= MENU_TYPE_STRING;

        if (mii.fType & MFT_OWNERDRAW)
            item->Type |= MENU_TYPE_OWNERDRAW;
        if (mii.fType & MFT_RADIOCHECK)
            item->Type |= MENU_TYPE_RADIOCHECK;

        // konverze promenne mii.fState
        if (mii.fState & MFS_CHECKED)
        {
            if (mii.hbmpChecked == NULL) // quick hack kvuli ICQ, ktere ma checked && mii.hbmpChecked
                item->State |= MENU_STATE_CHECKED;
            else
                mii.hbmpUnchecked = mii.hbmpChecked;
        }
        if (mii.fState & MFS_DEFAULT)
            item->State |= MENU_STATE_DEFAULT;
        if (mii.fState & MFS_GRAYED || mii.fState & MFS_DISABLED)
            item->State |= MENU_STATE_GRAYED;

        // command
        item->ID = mii.wID;

        // bitmapy
        item->HBmpItem = mii.hbmpItem;
        item->HBmpChecked = mii.hbmpChecked;
        item->HBmpUnchecked = mii.hbmpUnchecked;

        // data
        item->CustomData = mii.dwItemData;

        // v pripade stringu okopci retezec
        if (item->Type & MENU_TYPE_STRING)
        {
            const char* p = mii.dwTypeData;
            int len = mii.cch;
            if (p == NULL)
            {
                p = "";
                len = 0;
            }
            if (!item->SetText(p, len))
            {
                TRACE_E(LOW_MEMORY);
                Items.Detach(index);
                delete item;
                return FALSE;
            }
        }
        else if (item->Type & MENU_TYPE_BITMAP)
            item->HBmpItem = (HBITMAP)mii.dwTypeData;

        if (mii.hSubMenu != NULL)
        {
            CMenuPopup* subMenu = new CMenuPopup();
            if (subMenu == NULL)
            {
                TRACE_E(LOW_MEMORY);
                Items.Detach(index);
                delete item;
                return FALSE;
            }
            subMenu->SetTemplateMenu(mii.hSubMenu);
            item->SubMenu = subMenu;
        }
    }
    return TRUE;
}

BOOL CMenuPopup::LoadFromTemplate(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList, HIMAGELIST hHotImageList)
{
    return LoadFromTemplate2(hInstance, menuTemplate, enablersOffset, hImageList, hHotImageList, NULL);
}

// na zaklade sablony 'menuTemplate' naladuje 'menuPopup'
BOOL CMenuPopup::LoadFromTemplate2(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList, HIMAGELIST hHotImageList, int* addedRows)
{
    CALL_STACK_MESSAGE1("CMenuPopup::LoadFromTemplate2(, , , , , )");
    // inicializujeme a vycistime menu
    Cleanup();
    Items.DestroyMembers();
    if (addedRows != NULL)
        *addedRows = 1;

    SetImageList(hImageList);
    SetHotImageList(hHotImageList);

    char stringBuff[1000];
    MENU_ITEM_INFO mii;
    ZeroMemory(&mii, sizeof(mii));
    mii.String = stringBuff;

    const MENU_TEMPLATE_ITEM* row = menuTemplate;
    if (addedRows == 0)
    {
        if (row->RowType != MNTT_PB)
        {
            TRACE_E("First row of template must be MNTP_PB type");
            return FALSE;
        }
        ID = row->ID; // pro contextova menu, aby nosny popup mel prirazeni ID
        row++;
    }
    while (row->RowType != MNTT_PE)
    {
        switch (row->RowType)
        {
        case MNTT_IT: // item
        {
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING |
                       MENU_MASK_IMAGEINDEX | MENU_MASK_STATE | MENU_MASK_ENABLER |
                       MENU_MASK_SKILLLEVEL;
            mii.Type = MENU_TYPE_STRING;
            mii.ID = row->ID;
            lstrcpy(stringBuff, LoadStr(row->TextResID, hInstance));
            mii.ImageIndex = row->ImageIndex;
            mii.State = row->State;
            mii.SkillLevel = row->SkillLevel;
            // pokud je enablersOffset != NULL, hodnota row->Enabler neni ukazatel
            // na enabler, ale index do pole enableru
            if (enablersOffset != NULL && row->Enabler != NULL)
                mii.Enabler = enablersOffset + (DWORD)(DWORD_PTR)row->Enabler;
            else
                mii.Enabler = row->Enabler;
            if (!InsertItem(-1, TRUE, &mii))
            {
                Items.DestroyMembers();
                return FALSE;
            }
            break;
        }

        case MNTT_SP: // separator
        {
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_SKILLLEVEL;
            mii.Type = MENU_TYPE_SEPARATOR;
            mii.ID = row->ID;
            mii.SkillLevel = row->SkillLevel;
            if (!InsertItem(-1, TRUE, &mii))
            {
                Items.DestroyMembers();
                return FALSE;
            }
            break;
        }

        case MNTT_PB: // popup begin
        {
            CMenuPopup* subMenu = new CMenuPopup();
            if (subMenu == NULL)
            {
                TRACE_E(LOW_MEMORY);
                Items.DestroyMembers();
                return FALSE;
            }

            subMenu->SetStyle(MENU_POPUP_UPDATESTATES); //j.r. zde je to malo obecne

            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING |
                       MENU_MASK_IMAGEINDEX | MENU_MASK_SUBMENU | MENU_MASK_STATE |
                       MENU_MASK_ENABLER | MENU_MASK_SKILLLEVEL;
            mii.Type = MENU_TYPE_STRING;
            mii.ID = row->ID;
            lstrcpy(stringBuff, LoadStr(row->TextResID, hInstance));
            mii.ImageIndex = row->ImageIndex;
            mii.SubMenu = subMenu;
            mii.State = row->State;
            // pokud je enablersOffset != NULL, hodnota row->Enabler neni ukazatel
            // na enabler, ale index do pole enableru
            if (enablersOffset != NULL && row->Enabler != NULL)
                mii.Enabler = enablersOffset + (DWORD)(DWORD_PTR)row->Enabler;
            else
                mii.Enabler = row->Enabler;
            mii.SkillLevel = row->SkillLevel;

            int deltaRows;
            if (!subMenu->LoadFromTemplate2(hInstance, row + 1, enablersOffset, hImageList, hHotImageList, &deltaRows))
            {
                TRACE_E(LOW_MEMORY);
                Items.DestroyMembers();
                return FALSE;
            }
            row += deltaRows;
            if (addedRows != NULL)
                (*addedRows) += deltaRows;

            if (!InsertItem(-1, TRUE, &mii))
            {
                TRACE_E(LOW_MEMORY);
                Items.DestroyMembers();
                return FALSE;
            }
            break;
        }

        default:
            TRACE_E("Unknown RowType=" << row->RowType);
            break;
        }

        row++;
        if (addedRows != NULL)
            (*addedRows)++;
    }
    return TRUE;
}

BOOL CMenuPopup::FindNextItemIndex(int fromIndex, BOOL topToDown, int* index)
{
    CALL_STACK_MESSAGE3("CMenuPopup::FindNextItemIndex(%d, %d, )", fromIndex, topToDown);
    if (Items.Count == 0)
        return FALSE;
    if (topToDown)
    {
        if (fromIndex == -1)
            fromIndex = 0;
        else
        {
            fromIndex++;
            if (fromIndex > Items.Count - 1)
                fromIndex = 0;
        }
        int i;
        for (i = fromIndex; i < Items.Count; i++)
        {
            if (!(SharedRes->SkillLevel & Items[i]->SkillLevel))
                continue;
            if (!(Items[i]->Type & MENU_TYPE_SEPARATOR))
            {
                *index = i;
                return TRUE;
            }
        }
        // pokud jsem nenasel polozku dole, doprohledam vrsek
        for (i = 0; i < fromIndex; i++)
        {
            if (!(SharedRes->SkillLevel & Items[i]->SkillLevel))
                continue;
            if (!(Items[i]->Type & MENU_TYPE_SEPARATOR))
            {
                *index = i;
                return TRUE;
            }
        }
    }
    else
    {
        if (fromIndex == -1)
            fromIndex = Items.Count - 1;
        else
        {
            fromIndex--;
            if (fromIndex < 0)
                fromIndex = Items.Count - 1;
        }

        int i;
        for (i = fromIndex; i >= 0; i--)
        {
            if (!(SharedRes->SkillLevel & Items[i]->SkillLevel))
                continue;
            if (!(Items[i]->Type & MENU_TYPE_SEPARATOR))
            {
                *index = i;
                return TRUE;
            }
        }
        // pokud jsem nenasel polozku nahore, doprohledam spodek
        for (i = Items.Count - 1; i > fromIndex; i--)
        {
            if (!(SharedRes->SkillLevel & Items[i]->SkillLevel))
                continue;
            if (!(Items[i]->Type & MENU_TYPE_SEPARATOR))
            {
                *index = i;
                return TRUE;
            }
        }
    }
    return FALSE;
}

CMenuPopup*
CMenuPopup::FindPopup(HWND hWindow)
{
    CALL_STACK_MESSAGE1("CMenuPopup::FindPopup()");
    if (hWindow == NULL)
        return NULL;
    CMenuPopup* iterator = this;
    while (iterator != NULL)
    {
        if (iterator->HWindow == hWindow)
            return iterator;
        iterator = iterator->OpenedSubMenu;
    }
    return NULL;
}

CMenuPopup*
CMenuPopup::FindActivePopup()
{
    CALL_STACK_MESSAGE1("CMenuPopup::FindActivePopup()");
    CMenuPopup* iterator = this;
    while (iterator->OpenedSubMenu != NULL)
        iterator = iterator->OpenedSubMenu;
    return iterator;
}

void CMenuPopup::CheckSelectedPath(CMenuPopup* terminator)
{
    CALL_STACK_MESSAGE1("CMenuPopup::CheckSelectedPath()");
    if (this == terminator)
        return;
    if (OpenedSubMenu != NULL)
    {
        CMenuItem* selectedItem = NULL;
        if (SelectedItemIndex != -1)
            selectedItem = Items[SelectedItemIndex];
        if (selectedItem == NULL || OpenedSubMenu != selectedItem->SubMenu)
        {
            // najdu polozku, kterou je treba vybrat
            int newItemIndex = -1;
            int i;
            for (i = 0; i < Items.Count; i++)
                if (Items[i]->SubMenu == OpenedSubMenu)
                {
                    newItemIndex = i;
                    break;
                }
            if (newItemIndex == -1)
            {
                TRACE_E("newItemIndex == -1");
                return;
            }
            // musim opravit select
            HDC hPrivateDC = NOHANDLES(GetDC(HWindow));
            if (selectedItem != NULL)
            {
                // pokud je treba, deaktivuju minulou polozku
                DrawItem(hPrivateDC, selectedItem, TopItemY + selectedItem->YOffset, FALSE);
                SelectedItemIndex = -1;
            }
            // aktivuju novou polozku
            CMenuItem* newItem = Items[newItemIndex];
            DrawItem(hPrivateDC, newItem, TopItemY + newItem->YOffset, TRUE);
            SelectedItemIndex = newItemIndex;
        }
        // rekurzivne pro vsechny deti az po terminator
        OpenedSubMenu->CheckSelectedPath(terminator);
    }
}

void CMenuPopup::OnKeyRight(BOOL* leaveMenu)
{
    CALL_STACK_MESSAGE1("CMenuPopup::OnKeyRight()");
    if (SelectedItemIndex != -1 && !(Items[SelectedItemIndex]->State & MENU_STATE_GRAYED))
    {
        // otevreme submenu
        if (OpenedSubMenu != NULL)
            TRACE_E("OpenedSubMenu != NULL");
        CMenuItem* selectedItem = Items[SelectedItemIndex];
        if (selectedItem->SubMenu != NULL)
        {
            RECT itemRect;
            GetItemRect(SelectedItemIndex, &itemRect);
            selectedItem->SubMenu->SharedRes = SharedRes;
            selectedItem->SubMenu->TrackFlags = MENU_TRACK_SELECT;
            selectedItem->SubMenu->SetSelectedItemIndex(0);
            selectedItem->SubMenu->SelectedByMouse = FALSE;
            if (selectedItem->SubMenu->CreatePopupWindow(FirstPopup, itemRect.right, itemRect.top - 3,
                                                         SelectedItemIndex, &itemRect))
                OpenedSubMenu = selectedItem->SubMenu;
            return;
        }
    }
    if (SharedRes->MenuBar != NULL)
    {
        // zkusime se prepnout do dalsiho popupu
        int newHotIndex = SharedRes->MenuBar->HotIndex + 1;
        if (newHotIndex >= SharedRes->MenuBar->Menu->Items.Count)
            newHotIndex = 0;
        if (newHotIndex != SharedRes->MenuBar->HotIndex)
        {
            SharedRes->MenuBar->IndexToOpen = newHotIndex;
            SharedRes->MenuBar->OpenWithSelect = TRUE;
            *leaveMenu = TRUE;
        }
    }
}

void CMenuPopup::OnKeyReturn(BOOL* leaveMenu, DWORD* retValue)
{
    CALL_STACK_MESSAGE1("CMenuPopup::OnKeyReturn(, )");
    if (SelectedItemIndex == -1 ||
        (Items[SelectedItemIndex]->State & MENU_STATE_GRAYED) &&
            !(SharedRes->MenuBar != NULL && SharedRes->MenuBar->HelpMode &&                      // Petr: negace: jsme otevreni z menubar + jsme v HelpMode (Shift+F1) +
              (Items[SelectedItemIndex]->SubMenu == NULL || Items[SelectedItemIndex]->ID != 0))) // Petr: + mame IDcko prikazu nebo submenu ("grayed" submenu nelze vybalit)
        return;
    if (OpenedSubMenu != NULL)
        TRACE_E("OpenedSubMenu != NULL");
    CMenuItem* selectedItem = Items[SelectedItemIndex];
    if (selectedItem->SubMenu != NULL &&
        !(selectedItem->State & MENU_STATE_GRAYED)) // Petr: otevirat budeme jen submenu, ktera nejsou "grayed"
    {
        OnKeyRight(leaveMenu);
        return;
    }
    *leaveMenu = TRUE;
    *retValue = selectedItem->ID;
    if (SharedRes->MenuBar != NULL)              // jsme otevreni z menubar
        SharedRes->MenuBar->ExitMenuLoop = TRUE; // takze z ni vypadneme
}

int CMenuPopup::FindNextItemIndex(int firstIndex, char key)
{
    CALL_STACK_MESSAGE3("CMenuPopup::FindNextItemIndex(%d, %u)", firstIndex, key);
    key = UpperCase[key];
    if (firstIndex == -1)
        firstIndex = 0;
    else
        firstIndex++;
    if (firstIndex > Items.Count - 1)
        firstIndex = 0;

    int prefixIndexFirst = -1; // prvni vyskyt varianty s prefixem
    int charIndexFirst = -1;   // prvni vyskyt varianty bex prefixu
    int prefixIndex = -1;      // posledni vyskyt varianty s prefixem
    int charIndex = -1;        // posledni vyskyt varianty bex prefixu
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;
        if (item->Type & MENU_TYPE_STRING && !(item->State & MENU_STATE_GRAYED) && !(item->Type & MENU_TYPE_OWNERDRAW))
        {
            const char* p = item->String;
            BOOL firstCharFound = FALSE;
            BOOL prefixFound = FALSE;
            BOOL anotherPrefixFound = FALSE;
            if (*p != NULL)
            {
                while (*p != 0)
                {
                    if (*p == '&')
                    {
                        if (*(p + 1) != 0 && UpperCase[*(p + 1)] == key)
                        {
                            prefixFound = TRUE;
                            break;
                        }
                        else if (*(p + 1) != 0 && *(p + 1) != '&')
                        {
                            anotherPrefixFound = TRUE;
                            break;
                        }
                        p++;
                    }
                    p++;
                }
                // pokud jsme nenasli spravny prefix, ani jiny prefix, zkusim prvni pismeno
                if (!prefixFound && !anotherPrefixFound)
                {
                    p = item->String;
                    if (UpperCase[*p] == key)
                        firstCharFound = TRUE;
                }
            }
            if (prefixFound)
            {
                if (prefixIndexFirst == -1)
                    prefixIndexFirst = i;
                if (prefixIndex < firstIndex)
                    prefixIndex = i;
            }
            if (firstCharFound)
            {
                if (charIndexFirst == -1)
                    charIndexFirst = i;
                if (charIndex < firstIndex)
                    charIndex = i;
            }
        }
    }
    // prefixy maji prioritu
    if (prefixIndexFirst != -1)
    {
        if (prefixIndex >= firstIndex)
            return prefixIndex;
        else
            return prefixIndexFirst;
    }
    // pak prvni znaky
    if (charIndexFirst != -1)
    {
        if (charIndex >= firstIndex)
            return charIndex;
        else
            return charIndexFirst;
    }
    return -1;
}

void CMenuPopup::OnChar(char key, BOOL* leaveMenu, DWORD* retValue)
{
    CALL_STACK_MESSAGE2("CMenuPopup::OnChar(%u, , )", key);
    int firstIndex = FindNextItemIndex(SelectedItemIndex, key);
    if (firstIndex == -1)
    {
        // nenasli jsme zadnou odpovidajici polozku
        // zkusime se doptat context menu (Tortoise SVN pouziva ownerdraw popupy, nevyplnuje retezce, ale resi WM_MENUCHAR)
        HMENU hMenu = GetTemplateMenu();
        // x64 7zip vraci pri right-click na .7z souboru, rozbaleni 7Zip menu a stiku 'h' na klevesnici 0xcccccccccccccccc, coz
        // je v rozporu s http://msdn.microsoft.com/en-us/library/windows/desktop/ms646349%28v=vs.85%29.aspx (mel by vratit 0, protoze nic na H v menu nezacina)
        // kazdopadne nam padaly RTC, takze orezavam na spodni DWORD pomoci masky
        DWORD ret = (DWORD)(SendMessage(SharedRes->HParent, WM_MENUCHAR, MF_POPUP, (LPARAM)hMenu) & 0xffffffff);
        if (HIWORD(ret) == MNC_SELECT)
            SelectNewItemIndex(LOWORD(ret), FALSE);
        return;
    }
    int nextIndex = FindNextItemIndex(firstIndex, key); // existuje jeste jedna?
    if (nextIndex != firstIndex)
    {
        // select
        SelectNewItemIndex(firstIndex, FALSE);
    }
    else
    {
        // select and run
        SelectNewItemIndex(firstIndex, FALSE);
        OnKeyReturn(leaveMenu, retValue);
    }
}

int CMenuPopup::FindGroupIndex(int firstIndex, BOOL down)
{
    CALL_STACK_MESSAGE3("CMenuPopup::FindGroupIndex(%d, %d)", firstIndex, down);
    if (firstIndex == -1)
        firstIndex = 0;
    if (firstIndex > Items.Count - 1)
        firstIndex = 0;

    int newIndex = firstIndex;
    BOOL separatorFound = FALSE;

    if (down)
    {
        int i;
        for (i = firstIndex; i < Items.Count; i++)
        {
            CMenuItem* item = Items[i];
            if (!(SharedRes->SkillLevel & item->SkillLevel))
                continue;
            if (Items[i]->Type & MENU_TYPE_SEPARATOR)
            {
                separatorFound = TRUE;
            }
            else
            {
                if (separatorFound)
                {
                    newIndex = i;
                    break;
                }
            }
        }
    }
    else
    {
        // up
        if (firstIndex > 0)
        {
            BOOL first = TRUE;
            int i;
            for (i = firstIndex - 1; i >= 0; i--)
            {
                CMenuItem* item = Items[i];
                if (!(SharedRes->SkillLevel & item->SkillLevel))
                    continue;
                if (first)
                {
                    if (Items[i]->Type & MENU_TYPE_SEPARATOR)
                        separatorFound = TRUE;
                    first = FALSE;
                }
                if (Items[i]->Type & MENU_TYPE_SEPARATOR)
                {
                    if (!separatorFound)
                    {
                        newIndex = i + 1;
                        break;
                    }
                }
                else
                    separatorFound = FALSE;
            }
            if (i < 0)
                newIndex = 0;
        }
    }
    return newIndex;
}

void CMenuPopup::EnsureItemVisible(int index)
{
    int topLimit = 0;
    if (UpArrowVisible)
        topLimit += UPDOWN_ITEM_HEIGHT;

    int bottomLimit = Height;
    if (DownArrowVisible)
        bottomLimit -= UPDOWN_ITEM_HEIGHT;

    BOOL upArrow = UpArrowVisible;
    BOOL downArrow = DownArrowVisible;

    CMenuItem* item = Items[index];

    if (TopItemY + item->YOffset < topLimit)
    {
        // najdu spodni hranu posledni zobrazene polozky
        int firstVisibleTopY = 0;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            int tmp = TopItemY + iterator->YOffset;
            if (tmp > topLimit)
            {
                firstVisibleTopY = tmp;
                break;
            }
        }
        /*
    int lastVisibleBottomY = 0;
    for (i = 0; i < Items.Count; i++)
    {
      CMenuItem *iterator = Items[i];
      int tmp = TopItemY + iterator->YOffset + iterator->Height;
      if (tmp <= bottomLimit)
        lastVisibleBottomY = tmp;
      else
        break;
    }
*/
        // pokud zobrazime nejakou polozku nad hornim okrajem, urcite se zaroven
        // zobrazi sipka dolu
        downArrow = TRUE;
        bottomLimit = Height - UPDOWN_ITEM_HEIGHT;

        // pokud zobrazime prvni polozku ze seznamu, nebude uz existovat
        // sipka nahoru
        if (index == 0)
            upArrow = FALSE;
        else
            upArrow = TRUE;

        topLimit = 0;
        if (upArrow)
            topLimit += UPDOWN_ITEM_HEIGHT;

        int delta = topLimit - (TopItemY + item->YOffset);

        RECT r;
        r.left = 0;
        r.top = 0;
        r.right = Width;
        r.bottom = Height - UPDOWN_ITEM_HEIGHT;
        if (upArrow)
            r.top += UPDOWN_ITEM_HEIGHT;

        TopItemY += delta;

        int lastVisibleBottomY = 0;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            if (!(SharedRes->SkillLevel & iterator->SkillLevel))
                continue;
            int tmp = TopItemY + iterator->YOffset + iterator->Height;
            if (tmp <= bottomLimit)
                lastVisibleBottomY = tmp;
            else
                break;
        }

        HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
        ScrollWindowEx(HWindow, 0, delta, &r, &r, hUpdateRgn, NULL, 0);

        // rozsirime oblast prekresleni o spodni sipku
        HRGN hItemRgn = HANDLES(CreateRectRgn(0, lastVisibleBottomY, Width, Height));
        CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
        HANDLES(DeleteObject(hItemRgn));

        // rozsirime oblast prekresleni o prostor zacinajici horni hranou drive prvni polozky
        hItemRgn = HANDLES(CreateRectRgn(0, 0, Width, firstVisibleTopY + delta));
        CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
        HANDLES(DeleteObject(hItemRgn));

        UpArrowVisible = upArrow;
        DownArrowVisible = downArrow;
        PaintAllItems(hUpdateRgn);
        HANDLES(DeleteObject(hUpdateRgn));

        return;
    }

    if (TopItemY + item->YOffset + item->Height > bottomLimit)
    {
        // najdu spodni hranu posledni zobrazene polozky
        int lastVisibleBottomY = 0;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            if (!(SharedRes->SkillLevel & iterator->SkillLevel))
                continue;
            int tmp = TopItemY + iterator->YOffset + iterator->Height;
            if (tmp <= bottomLimit)
                lastVisibleBottomY = tmp;
            else
                break;
        }

        // pokud zobrazime nejakou polozku pod spodnim okrajem, urcite se zaroven
        // zobrazi sipka nahoru
        upArrow = TRUE;
        topLimit = UPDOWN_ITEM_HEIGHT;

        // pokud zobrazime posledni polozku ze seznamu, nebude uz existovat
        // sipka dolu
        if (index == Items.Count - 1)
            downArrow = FALSE;
        else
            downArrow = TRUE;

        bottomLimit = Height;
        if (downArrow)
            bottomLimit -= UPDOWN_ITEM_HEIGHT;

        int delta = bottomLimit - (TopItemY + item->YOffset + item->Height);

        RECT r;
        r.left = 0;
        r.top = UPDOWN_ITEM_HEIGHT;
        r.right = Width;
        r.bottom = lastVisibleBottomY;

        TopItemY += delta;

        // najdu horni hranu prvni zobrazene polozky
        int firstItemY = 0;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            if (!(SharedRes->SkillLevel & iterator->SkillLevel))
                continue;
            int tmp = TopItemY + iterator->YOffset;
            if (tmp >= topLimit)
            {
                firstItemY = tmp;
                break;
            }
        }

        HRGN hUpdateRgn = HANDLES(CreateRectRgn(0, 0, 0, 0));
        ScrollWindowEx(HWindow, 0, delta, &r, &r, hUpdateRgn, NULL, 0);

        // rozsirim oblast prekresleni o horni sipku
        HRGN hItemRgn = HANDLES(CreateRectRgn(0, 0, Width, firstItemY));
        CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
        HANDLES(DeleteObject(hItemRgn));

        // rozsirime oblast prekresleni o prostor zacinajici spodni hranou drive posledni polozky
        hItemRgn = HANDLES(CreateRectRgn(0, lastVisibleBottomY + delta, Width, Height));
        CombineRgn(hUpdateRgn, hUpdateRgn, hItemRgn, RGN_OR);
        HANDLES(DeleteObject(hItemRgn));

        UpArrowVisible = upArrow;
        DownArrowVisible = downArrow;
        PaintAllItems(hUpdateRgn);
        HANDLES(DeleteObject(hUpdateRgn));
    }
}

void CMenuPopup::OnMouseWheel(WPARAM wParam, LPARAM lParam)
{
    if (Items.Count < 1)
        return;

    BOOL upArrow = UpArrowVisible;
    BOOL downArrow = DownArrowVisible;

    // urcime vysku o kterou budeme rolovat
    CMenuItem* item = Items[0];
    int itemHeight = item->Height; // prvni polozka nebude separator
    if (itemHeight < 5)
        itemHeight = 15; // kdyby to byl separator, budeme rolovat o 15 bodu

    short zDelta = (short)HIWORD(wParam);
    if ((zDelta < 0 && MouseWheelAccumulator > 0) || (zDelta > 0 && MouseWheelAccumulator < 0))
        ResetMouseWheelAccumulator(); // pri zmene smeru naklapeni kolecka je potreba nulovat akumulator

    DWORD wheelScroll = GetMouseWheelScrollLines();        // muze byt az WHEEL_PAGESCROLL(0xffffffff)
    wheelScroll = max(1, min(wheelScroll, (DWORD)Height)); // omezime maximalne na delku stranky

    MouseWheelAccumulator += 1000 * zDelta;
    int stepsPerLine = max(1, (1000 * WHEEL_DELTA) / wheelScroll);
    int linesToScroll = MouseWheelAccumulator / stepsPerLine;
    if (linesToScroll == 0)
        return;

    MouseWheelAccumulator -= linesToScroll * stepsPerLine;
    int delta = linesToScroll * itemHeight;

    // vyhledam prvni a posledni viditelnou polozku
    CMenuItem* firstVisibleItem = NULL;
    CMenuItem* lastVisibleItem = NULL;
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* iterator = Items[i];
        if (!(SharedRes->SkillLevel & iterator->SkillLevel))
            continue;
        if (firstVisibleItem == NULL)
            firstVisibleItem = iterator;
        lastVisibleItem = iterator;
    }
    if (firstVisibleItem == NULL || lastVisibleItem == NULL)
        return;
    if (TopItemY == 0 && lastVisibleItem->YOffset + lastVisibleItem->Height <= Height)
        return; // vsechny polozky jsou zobrazeny, v tomto popupu nema smysl rolovat

    // osetrime horni mez (aby horni polozka nebyla stazena dolu pod horni okraj menu)
    if (TopItemY + delta >= 0)
    {
        delta = -TopItemY;
        upArrow = FALSE;
    }
    else
        upArrow = TRUE;

    // osetrime spodni mez (aby spodni polozka nebyla vytazena nad spodni okraj menu)
    if (lastVisibleItem->YOffset + lastVisibleItem->Height + TopItemY + delta <= Height)
    {
        delta = Height - (lastVisibleItem->YOffset + lastVisibleItem->Height + TopItemY);
        downArrow = FALSE;
    }
    else
        downArrow = TRUE;

    // pokud se neco zmenilo, budeme kresli (kaslu na scrolling, pokud to bude blikat, muzeme doprogramovat)
    if (delta != 0 || UpArrowVisible != upArrow || DownArrowVisible != downArrow)
    {
        TopItemY += delta;

        UpArrowVisible = upArrow;
        DownArrowVisible = downArrow;
        SelectedItemIndex = -1;
        PaintAllItems(NULL);

        // nechame vybrat polozku pod kurzorem
        POINT cursorPos;
        GetCursorPos(&cursorPos);
        HWND hWndUnderCursor = WindowFromPoint(cursorPos);
        if (hWndUnderCursor == HWindow)
        {
            MSG myMsg;
            myMsg.hwnd = HWindow;
            myMsg.wParam = 0;
            myMsg.lParam = MAKELPARAM(cursorPos.x, cursorPos.y);
            myMsg.message = WM_MOUSEMOVE;
            myMsg.time = GetTickCount();
            myMsg.pt = cursorPos;
            SharedRes->LastMouseMove.x = cursorPos.x - 1; // abychom prosli skrz uvodni test

            BOOL dummy1;
            DWORD dummy2;
            BOOL dummy3;
            DoDispatchMessage(&myMsg, &dummy1, &dummy2, &dummy3);
        }
    }
    return;
}

void CMenuPopup::SelectNewItemIndex(int newItemIndex, BOOL byMouse)
{
    CALL_STACK_MESSAGE3("CMenuPopup::SelectNewItemIndex(%d, %d)", newItemIndex, byMouse);
    if (SelectedItemIndex != newItemIndex)
    {
        HDC hPrivateDC = NOHANDLES(GetDC(HWindow));
        if (SelectedItemIndex != -1)
        {
            CMenuItem* oldItem = Items[SelectedItemIndex];
            DrawItem(hPrivateDC, oldItem, TopItemY + oldItem->YOffset, FALSE);
        }
        if (newItemIndex != -1)
        {
            CMenuItem* newItem = Items[newItemIndex];
            DrawItem(hPrivateDC, newItem, TopItemY + newItem->YOffset, TRUE);
        }
        SelectedItemIndex = newItemIndex;
        SelectedByMouse = byMouse;
        if (newItemIndex != -1)
            EnsureItemVisible(newItemIndex);
    }
}

void CMenuPopup::DoDispatchMessage(MSG* msg, BOOL* leaveMenu, DWORD* retValue, BOOL* dispatchLater)
{
    CALL_STACK_MESSAGE1("CMenuPopup::DoDispatchMessage(, , , )");
    *dispatchLater = FALSE;
    switch (msg->message)
    {
    case WM_USER_CLOSEMENU:
    {
        *leaveMenu = TRUE;
        return;
    }

    case WM_CHAR:
    case WM_SYSCHAR: //j.r.: aby fungovalo take Alt+PISMENO uvnitr menu
    {
        CMenuPopup* popup = FindActivePopup();
        popup->OnChar((char)msg->wParam, leaveMenu, retValue);
        return;
    }

    case WM_SYSKEYUP:
    {
        // pokud se sys key up s VK_F10 dorucilo do modalniho dialogu, doslo k zaslani
        // zpravy WM_SYSCOMMAND s uCmdType=SC_KEYMENU a v dusledku k aktivaci window menu
        // projevovalo se v Plugins/Encrypt/Create Key/Shift+F10 v jedne z editlines
        // s passwordy, kde se vybaluje nase menu
        if (msg->wParam == VK_F10)
            return;
        break;
    }

    case WM_SYSKEYDOWN:
    case WM_KEYDOWN:
    {
        //      TRACE_I("WM_SYSKEYDOWN wParam="<<msg->wParam);
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftPressed && msg->wParam == VK_F10)
        {
            CMenuPopup* popup = FindActivePopup();
            if (popup != NULL && !(FirstPopup->TrackFlags & MENU_TRACK_RIGHTBUTTON))
            {
                if (SendMessage(SharedRes->HParent, WM_USER_CONTEXTMENU,
                                (WPARAM)(CGUIMenuPopupAbstract*)popup, (LPARAM)FALSE))
                {
                    popup->OnKeyReturn(leaveMenu, retValue); //p.s. rozbaleni sub menu, nebo nageneruju command
                }
            }
            return;
        }
        if (msg->wParam == VK_MENU && (msg->lParam & 0x40000000) == 0 || // Alt down, ale ne autorepeat
            (!shiftPressed && msg->wParam == VK_F10))
        {
            *leaveMenu = TRUE;
            if (SharedRes->MenuBar != NULL)              // jsme otevreni z menubar
                SharedRes->MenuBar->ExitMenuLoop = TRUE; // takze z ni vypadneme
            return;
        }

        //      TRACE_I("WM_KEYDOWN wParam="<<msg->wParam);
        CMenuPopup* popup = FindActivePopup();
        switch (msg->wParam)
        {
        case VK_APPS:
        {
            if (popup != NULL && !(FirstPopup->TrackFlags & MENU_TRACK_RIGHTBUTTON))
            {
                if (SendMessage(SharedRes->HParent, WM_USER_CONTEXTMENU,
                                (WPARAM)(CGUIMenuPopupAbstract*)popup, (LPARAM)FALSE))
                {
                    popup->OnKeyReturn(leaveMenu, retValue); //p.s. rozbaleni sub menu, nebo nageneruju command
                }
            }
            break;
        }

        case VK_UP:
        case VK_DOWN:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        {
            if (popup->Items.Count == 0)
                break;
            int newSelectedItemIndex = popup->SelectedItemIndex;

            switch (msg->wParam)
            {
            case VK_UP:
            {
                popup->FindNextItemIndex(popup->SelectedItemIndex, FALSE, &newSelectedItemIndex);
                break;
            }

            case VK_DOWN:
            {
                popup->FindNextItemIndex(popup->SelectedItemIndex, TRUE, &newSelectedItemIndex);
                break;
            }

            case VK_HOME:
            {
                newSelectedItemIndex = 0;
                break;
            }

            case VK_END:
            {
                newSelectedItemIndex = popup->Items.Count - 1;
                break;
            }

            case VK_PRIOR:
            {
                newSelectedItemIndex = popup->FindGroupIndex(popup->SelectedItemIndex, FALSE);
                break;
            }

            case VK_NEXT:
            {
                newSelectedItemIndex = popup->FindGroupIndex(popup->SelectedItemIndex, TRUE);
                break;
            }
            }
            popup->SelectNewItemIndex(newSelectedItemIndex, FALSE);
            break;
        }

        case VK_ESCAPE:
        case VK_LEFT:
        {
            // pracovni verze - pouze pro ladeni
            CMenuPopup* iterator = this;
            while (iterator != NULL && iterator->OpenedSubMenu != NULL)
            {
                if (iterator->OpenedSubMenu->OpenedSubMenu == NULL)
                {
                    DestroyWindow(iterator->OpenedSubMenu->HWindow);
                    iterator->OpenedSubMenu = NULL;
                }
                iterator = iterator->OpenedSubMenu;
            }
            if (iterator == this)
            {
                if (msg->wParam == VK_ESCAPE) // zavirani nejvrchnejsiho okna
                    *leaveMenu = TRUE;

                if (SharedRes->MenuBar != NULL && SharedRes->MenuBar->HelpMode &&
                    msg->wParam == VK_ESCAPE)
                {
                    // zavreme menu, ukoncime menu loop a mizime
                    *leaveMenu = TRUE;
                    SharedRes->MenuBar->ExitMenuLoop = TRUE;
                    return;
                }
                if (SharedRes->MenuBar != NULL && msg->wParam == VK_LEFT) // zkusime se prepnout do predchoziho popupu
                {
                    int newHotIndex = SharedRes->MenuBar->HotIndex - 1;
                    if (newHotIndex < 0)
                        newHotIndex = SharedRes->MenuBar->Menu->Items.Count - 1;
                    if (newHotIndex != SharedRes->MenuBar->HotIndex)
                    {
                        SharedRes->MenuBar->IndexToOpen = newHotIndex;
                        SharedRes->MenuBar->OpenWithSelect = TRUE;
                        *leaveMenu = TRUE;
                    }
                }
            }
            SharedRes->ChangeTickCount = INFINITE; // nechceme, aby se vybalovalo menu po timeoutu
            break;
        }

        case VK_RIGHT:
        {
            popup->OnKeyRight(leaveMenu); // rozbaleni sub menu, je-li to mozne
            break;
        }

        case VK_RETURN:
        {
            popup->OnKeyReturn(leaveMenu, retValue); // rozbaleni sub menu, nebo nageneruju command
            return;                                  // nesmime nechat Translatnou Return
        }
        }
        msg->hwnd = HWindow; // presmerujeme na nas (aby nedoslo k uniku klaves ven)
        TranslateMessage(msg);
        return;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_NCLBUTTONDOWN:
    case WM_NCLBUTTONDBLCLK:
    case WM_NCLBUTTONUP:
    case WM_NCRBUTTONDOWN:
    case WM_NCRBUTTONUP:
    case WM_NCMBUTTONDOWN:
    case WM_NCMBUTTONUP:
    {
        HWND hWndUnderCursor = PopupWindowFromPoint(msg->pt);
        CMenuPopup* popup = FindPopup(hWndUnderCursor);

        if (popup != NULL && popup->SelectedItemIndex != -1 &&
            (msg->message == WM_LBUTTONDOWN ||
             msg->message == WM_MBUTTONDOWN ||
             msg->message == WM_RBUTTONDOWN))
        {
            // mys stoji nad nevybranou polozkou a uzivatel stisknul nektere z tlacitek
            // -> napred polozku nechame selectnout
            MSG myMsg = *msg;
            myMsg.message = WM_MOUSEMOVE;
            SharedRes->LastMouseMove.x = myMsg.pt.x - 1; // abychom prosli skrze uvodni test
            DoDispatchMessage(&myMsg, leaveMenu, retValue, dispatchLater);
        }

        if (popup != NULL && popup->SelectedItemIndex != -1 &&
            ((msg->message == WM_LBUTTONDOWN ||
              msg->message == WM_LBUTTONDBLCLK ||
              msg->message == WM_LBUTTONUP) ||
             ((FirstPopup->TrackFlags & MENU_TRACK_RIGHTBUTTON) &&
              (msg->message == WM_RBUTTONDOWN ||
               msg->message == WM_RBUTTONDBLCLK ||
               msg->message == WM_RBUTTONUP))))
        {
            CMenuItem* item = popup->Items[popup->SelectedItemIndex];
            if (!(item->State & MENU_STATE_GRAYED) ||
                SharedRes->MenuBar != NULL && SharedRes->MenuBar->HelpMode && // Petr: jsme otevreni z menubar + jsme v HelpMode (Shift+F1) +
                    (item->SubMenu == NULL || item->ID != 0))                 // Petr: + mame IDcko prikazu nebo submenu ("grayed" submenu nelze vybalit)
            {
                if (!(item->State & MENU_STATE_GRAYED) && // Petr: otevirani "grayed" submenu neni mozne
                    item->SubMenu != NULL && item->SubMenu != popup->OpenedSubMenu)
                {
                    // pokud je otevrene nejake sub-menu, zavru ho
                    if (popup->OpenedSubMenu != NULL)
                        popup->CloseOpenedSubmenu();
                    // ted muzu otevrit nove
                    RECT itemRect;
                    popup->GetItemRect(popup->SelectedItemIndex, &itemRect);
                    item->SubMenu->SharedRes = SharedRes;
                    item->SubMenu->TrackFlags = 0;
                    if (item->SubMenu->CreatePopupWindow(FirstPopup, itemRect.right, itemRect.top - 3,
                                                         popup->SelectedItemIndex, &itemRect))
                    {
                        popup->OpenedSubMenu = item->SubMenu;
                    }
                }
                else
                {
                    if (item->SubMenu == NULL ||
                        (item->State & MENU_STATE_GRAYED) && item->SubMenu != NULL) // Petr: "grayed" submenu se v HelpMode chova jako prikaz
                    {
                        // kliknuti na enabled polozku, ktera neni submenu (Petr: + v HelpMode "grayed" polozku nebo submenu)
                        if (msg->message == WM_LBUTTONUP || msg->message == WM_RBUTTONUP)
                        {
                            *leaveMenu = TRUE;
                            *retValue = item->ID;
                            if (SharedRes->MenuBar != NULL)              // jsme otevreni z menubar
                                SharedRes->MenuBar->ExitMenuLoop = TRUE; // takze z ni vypadneme
                        }
                    }
                }
            }
            return;
        }
        if (popup != NULL && popup->SelectedItemIndex != -1 &&
            !(FirstPopup->TrackFlags & MENU_TRACK_RIGHTBUTTON) &&
            msg->message == WM_RBUTTONUP)
        {
            // user pustil tlacitko nad vybranou polozkou
            if (SendMessage(SharedRes->HParent, WM_USER_CONTEXTMENU,
                            (WPARAM)(CGUIMenuPopupAbstract*)popup, (LPARAM)TRUE))
            {
                popup->OnKeyReturn(leaveMenu, retValue); //p.s. rozbaleni sub menu, nebo nageneruju command
            }
            return;
        }
        if (popup == NULL)
        {
            if (SharedRes->MenuBar != NULL &&
                (msg->message == WM_LBUTTONDOWN || msg->message == WM_LBUTTONDBLCLK))
            {
                // jsme rozbaleni z MenuBar a prisel do ni LButtonUp => nesmime se zavrit
                if (hWndUnderCursor == SharedRes->MenuBar->HWindow)
                {
                    int hitIndex;
                    POINT p;
                    p = msg->pt;
                    ScreenToClient(SharedRes->MenuBar->HWindow, &p);
                    if (SharedRes->MenuBar->HitTest(p.x, p.y, hitIndex))
                        return; // stopim
                }
            }
            // nechame okno zavrit a pak dorucit zpravu
            if (SharedRes->ExcludeRect != NULL && PtInRect(SharedRes->ExcludeRect, msg->pt) &&
                (msg->message == WM_LBUTTONDOWN || msg->message == WM_LBUTTONDBLCLK))
                *dispatchLater = FALSE; // nad timto obdelnikem nedorucim ldown, aby nedoslo k opetovnemu otevreni menu
            else
                *dispatchLater = TRUE;
            *leaveMenu = TRUE;
            if (SharedRes->MenuBar != NULL)
                SharedRes->MenuBar->ExitMenuLoop = TRUE;
            return;
        }
        break;
    }

    case WM_MOUSEMOVE:
    {
        POINT p = msg->pt;
        if (SharedRes->LastMouseMove.x == p.x && SharedRes->LastMouseMove.y == p.y)
            return; // nechceme zpravu dorucit
        SharedRes->LastMouseMove = p;
        HWND hWindowUnderCursor = PopupWindowFromPoint(p);
        if (SharedRes->MenuBar != NULL && hWindowUnderCursor == SharedRes->MenuBar->HWindow)
        {
            // jsme otevreni z MenuBar okna a musime mu helpnout s prepinanim
            POINT p2;
            p2 = p;
            ScreenToClient(SharedRes->MenuBar->HWindow, &p2);
            int hitIndex;
            if (SharedRes->MenuBar->HitTest(p2.x, p2.y, hitIndex) &&
                hitIndex != SharedRes->MenuBar->HotIndex)
            {
                *leaveMenu = TRUE;
                SharedRes->MenuBar->IndexToOpen = hitIndex;
                SharedRes->MenuBar->OpenWithSelect = FALSE;
                return;
            }
        }
        CMenuPopup* popup = FindPopup(hWindowUnderCursor);
        CMenuPopup* activePopup = FindActivePopup();
        int newItemIndex = -1;
        if (popup != NULL)
        {
            ScreenToClient(popup->HWindow, &p);
            // chceme pouze client area
            if (p.x >= 0 && p.x < popup->Width &&
                p.y >= 0 && p.y < popup->Height)
            {
                int itemIndex;
                CMenuPopupHittestEnum hittest = popup->HitTest(&p, &itemIndex);
                if (hittest == mphItem &&
                    !((popup->Items[itemIndex]->Type & MENU_TYPE_SEPARATOR) ||
                      (popup->Items[itemIndex]->State & MENU_STATE_GRAYED) &&
                          !(SharedRes->MenuBar != NULL && SharedRes->MenuBar->HelpMode &&                     // Petr: negace: jsme otevreni z menubar + jsme v HelpMode (Shift+F1) +
                            (popup->Items[itemIndex]->SubMenu == NULL || popup->Items[itemIndex]->ID != 0)))) // Petr: + mame IDcko prikazu nebo submenu ("grayed" submenu nelze vybalit)
                {
                    newItemIndex = itemIndex;
                }
                if (hittest == mphUpArrow || hittest == mphDownArrow)
                {
                    SetTimer(HWindow, UPDOWN_TIMER_ID, UPDOWN_TIMER_TO, NULL);
                    UpDownTimerRunnig = TRUE;
                }
            }
        }

        // pokud je treba, deaktivuju vybranou polozku v aktivniho popupu
        if (activePopup != NULL && activePopup->SelectedByMouse &&
            (popup == NULL || popup != activePopup) && activePopup->SelectedItemIndex != -1)
        {
            HDC hPrivateDC = NOHANDLES(GetDC(activePopup->HWindow));
            CMenuItem* oldItem = activePopup->Items[activePopup->SelectedItemIndex];
            activePopup->DrawItem(hPrivateDC, oldItem, TopItemY + oldItem->YOffset, FALSE);
            activePopup->SelectedItemIndex = -1;
        }

        if (popup != NULL)
            CheckSelectedPath(popup);

        // pokud doslo ke zmene, vykreslim ji
        if (popup != NULL && (newItemIndex != popup->SelectedItemIndex))
        {
            popup->SelectNewItemIndex(newItemIndex, TRUE);
            /*
        if (popup->OpenedSubMenu != NULL)
        {
          // je-li otevrene submenu, nastavim cas pouze pro prvni zmenu
          if (SharedRes->ChangeTickCount == INFINITE)
            SharedRes->ChangeTickCount = GetTickCount();
        }
        else
          SharedRes->ChangeTickCount = GetTickCount();
        */
            // John: klasicke menu se chova takto.
            SharedRes->ChangeTickCount = GetTickCount();
        }
        return;
    }

    case WM_TIMER:
    {
        if (msg->wParam == UPDOWN_TIMER_ID)
        {
            CMenuPopup* popup = NULL;
            POINT p = msg->pt;
            HWND hWindowUnderCursor = PopupWindowFromPoint(p);
            if (hWindowUnderCursor != NULL)
                popup = FindPopup(hWindowUnderCursor);
            BOOL keepTimerAlive = FALSE;
            if (popup != NULL)
            {
                ScreenToClient(popup->HWindow, &p);
                // chceme pouze client area
                int itemIndex;
                CMenuPopupHittestEnum hittest = popup->HitTest(&p, &itemIndex);
                if (hittest == mphUpArrow || hittest == mphDownArrow)
                {
                    int newIndex = -1;
                    if (hittest == mphDownArrow)
                    {
                        // vyhledame prvni nezobrazenou polozku
                        int bottomLimit = popup->Height - UPDOWN_ITEM_HEIGHT;
                        int i;
                        for (i = 0; i < popup->Items.Count; i++)
                        {
                            CMenuItem* iterator = popup->Items[i];
                            int tmp = popup->TopItemY + iterator->YOffset + iterator->Height;
                            if (tmp > bottomLimit)
                            {
                                newIndex = i;
                                break;
                            }
                        }
                    }
                    else
                    {
                        // vyhledame prvni nezobrazenou polozku
                        int topLimit = UPDOWN_ITEM_HEIGHT;
                        int i;
                        for (i = 1; i < popup->Items.Count; i++)
                        {
                            CMenuItem* iterator = popup->Items[i];
                            int tmp = popup->TopItemY + iterator->YOffset;
                            if (tmp >= topLimit)
                            {
                                newIndex = i - 1;
                                break;
                            }
                        }
                    }
                    if (newIndex != -1)
                    {
                        popup->CloseOpenedSubmenu();
                        popup->EnsureItemVisible(newIndex);
                        keepTimerAlive = TRUE;
                    }
                }
            }
            if (!keepTimerAlive)
            {
                KillTimer(HWindow, UPDOWN_TIMER_ID);
                UpDownTimerRunnig = FALSE;
            }
            return;
        }
        break;
        // do AS 2.52b1 zde byl return; se zavedeni throbberu se nam tim nedorucovalo IDT_THROBBER
        // a throbber se netocil po dobu, co byl uzivatel v menu; bylo by mozne propustit explicitne
        // jen tento timer, ale momentalne nevidim duvod, proc nedorucit vsechny; pokud se neco podela,
        // muzeme poustet jen ten throbber
        //      return;
    }

        //    case WM_NCMOUSEMOVE:
        //    case WM_NCHITTEST:
        //      return;
    }
    TranslateMessage(msg);
    DispatchMessage(msg);
}

BOOL CMenuPopup::CreatePopupWindow(CMenuPopup* firstPopup, int x, int y, int submenuItemPos, const RECT* exclude)
{
    CALL_STACK_MESSAGE4("CMenuPopup::CreatePopupWindow(, %d, %d, %d, )", x, y, submenuItemPos);
    Cleanup();

    FirstPopup = firstPopup;

    // ! pozor: WM_USER_INITMENUPOPUP musi vzdy parovat s WM_USER_UNINITMENUPOPUP,
    // protoze aplikace muze pri zavirani destruovat data

    if (!(TrackFlags & MENU_TRACK_NONOTIFY))
        SendMessage(SharedRes->HParent, WM_USER_INITMENUPOPUP,
                    (WPARAM)(CGUIMenuPopupAbstract*)this, MAKELPARAM(submenuItemPos, (WORD)ID));

    if (HWindowsMenu != NULL)
    {
        if (!(TrackFlags & MENU_TRACK_NONOTIFY))
            SendMessage(SharedRes->HParent, WM_INITMENUPOPUP, (WPARAM)HWindowsMenu, MAKELPARAM(submenuItemPos, 0));
        LoadFromHandle();
    }

    if (Items.Count == 0 || FirstPopup->Closing) // behem initu mohlo dojit k zavreni menu
    {
        // posleme notifikaci, aby nam parovala k WM_USER_INITMENUPOPUP
        if (!(TrackFlags & MENU_TRACK_NONOTIFY))
            SendMessage(SharedRes->HParent, WM_USER_UNINITMENUPOPUP,
                        (WPARAM)(CGUIMenuPopupAbstract*)this, MAKELPARAM(submenuItemPos, (WORD)ID));
        return FALSE;
    }

    // pokud user predvybral polozku, osetrime krajni stavy
    if (TrackFlags & MENU_TRACK_SELECT)
    {
        if (SelectedItemIndex < 0 || SelectedItemIndex >= Items.Count)
            SelectedItemIndex = 0;

        if (Items[SelectedItemIndex]->Type & MENU_TYPE_SEPARATOR)
        {
            // trefili jsme se na separator, zkusime dohledat nejakou vybratelnou
            int firstIndex;
            if (FindNextItemIndex(SelectedItemIndex, TRUE, &firstIndex))
                SelectedItemIndex = firstIndex;
            // kdyz nic nenajdeme, nechame to na tom separatoru
        }
    }
    else
        SelectedItemIndex = -1;

    // napocitam rozmery menu
    LayoutColumns();

    RECT r;
    if (exclude != NULL)
        r = *exclude;
    else
    {
        r.left = x;
        r.right = x;
        r.top = y;
        r.bottom = y;
    }
    RECT clipRect;
    MultiMonGetClipRectByRect(&r, &clipRect, NULL);

    int newX = x;
    int newY = y;
    int width = Width + 6;
    int height = TotalHeight + 6;

    if (TrackFlags & MENU_TRACK_CENTERALIGN)
        newX -= width / 2;
    else if (TrackFlags & MENU_TRACK_RIGHTALIGN)
        newX -= width;

    if (TrackFlags & MENU_TRACK_VCENTERALIGN)
        newY -= height / 2;
    else if (TrackFlags & MENU_TRACK_BOTTOMALIGN)
        newY -= height;

    // osetrime prelezeni prave a leve strany viditelne oblasti
    if (newX + width > clipRect.right)
        newX = clipRect.right - width;
    if (newX < clipRect.left)
        newX = clipRect.left;
    // osetrime prelezeni dolni a horni strany viditelne oblasti
    if (newY + height > clipRect.bottom)
        newY = clipRect.bottom - height;
    if (newY < clipRect.top)
        newY = clipRect.top;

    RECT menuRect;
    menuRect.left = newX;
    menuRect.top = newY;
    menuRect.right = newX + width;
    menuRect.bottom = newY + height;
    RECT dummy;
    if (IntersectRect(&dummy, &menuRect, &r))
    {
        if (TrackFlags & MENU_TRACK_VERTICAL)
        {
            // mame ustoupit pod nebo nad exclude rectangle
            int topHeight = max(0, min(height, r.top - clipRect.top));
            int bottomHeight = max(0, min(height, clipRect.bottom - r.bottom));
            if (topHeight <= bottomHeight)
            {
                // menu umistime dolu
                newY = r.bottom;
                height = bottomHeight;
            }
            else
            {
                // menu umistime nahoru
                newY = r.top - topHeight;
                height = topHeight;
            }
        }
        else
        {
            // mame ustoupit pred nebo za exclude rectangle
            int leftWidth = max(0, min(width, r.left - clipRect.left));
            int rightWidth = max(0, min(width, clipRect.right - r.right));
            if (rightWidth >= leftWidth)
            {
                // menu umistime vpravo
                newX = r.right;
            }
            else
            {
                // menu umistime vlevo
                newX = r.left - leftWidth;
            }
        }
    }

    // osetrime vyteceni dole z obrazovky pro menu, ktere nema MENU_TRACK_VERTICAL (napriklad User Menu)
    if (newY + height > clipRect.bottom && (clipRect.bottom - newY) > 0)
        height = clipRect.bottom - newY;

    Height = height - 6;

    TopItemY = 0;

    UpArrowVisible = FALSE;
    if (Height < TotalHeight)
        DownArrowVisible = TRUE;

    if (CreateEx(WS_EX_TOPMOST | WS_EX_DLGMODALFRAME,
                 WC_POPUPMENU,
                 "",
                 WS_CLIPSIBLINGS | WS_POPUP | WS_BORDER,
                 newX,
                 newY,
                 width,
                 height,
                 SharedRes->HParent,
                 NULL,
                 HInstance,
                 this) == NULL)
    {
        TRACE_E("Unable to create window.");
        // posleme notifikaci, aby nam parovala k WM_USER_INITMENUPOPUP
        if (!(TrackFlags & MENU_TRACK_NONOTIFY))
            SendMessage(SharedRes->HParent, WM_USER_UNINITMENUPOPUP,
                        (WPARAM)(CGUIMenuPopupAbstract*)this, MAKELPARAM(submenuItemPos, (WORD)ID));
        return FALSE;
    }
    if (Style & MENU_POPUP_UPDATESTATES)
        UpdateItemsState();
    SetWindowPos(HWindow, NULL, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    GetWindowRect(HWindow, &WindowRect);
    if (SelectedItemIndex != -1)
        EnsureItemVisible(SelectedItemIndex);
    return TRUE;
}

// zavru otevrene submenu, vcetne jeho childu
// prvni se musi zavirat nejvzdalenejsi child a pak smerem k nam
void CMenuPopup::CloseOpenedSubmenu()
{
    CALL_STACK_MESSAGE1("CMenuPopup::CloseOpenedSubmenu()");
    if (OpenedSubMenu == NULL)
        return;

    TDirectArray<CMenuPopup*> stack(10, 10);
    CMenuPopup* iterator = OpenedSubMenu;
    while (iterator != NULL)
    {
        stack.Add(iterator);
        if (!stack.IsGood())
        {
            stack.ResetState();
            return;
        }
        iterator = iterator->OpenedSubMenu;
    }
    // v obracenem poradi je postrilim
    int i;
    for (i = stack.Count - 1; i >= 0; i--)
    {
        iterator = stack[i];
        DestroyWindow(iterator->HWindow);
    }
    OpenedSubMenu = NULL;
}

void CMenuPopup::HideAll()
{
    CALL_STACK_MESSAGE1("CMenuPopup::HideAll()");
    Closing = TRUE;
    if (OpenedSubMenu != NULL)
    {
        TDirectArray<CMenuPopup*> stack(10, 10);
        CMenuPopup* iterator = OpenedSubMenu;
        while (iterator != NULL)
        {
            stack.Add(iterator);
            if (!stack.IsGood())
            {
                stack.ResetState();
                return;
            }
            iterator = iterator->OpenedSubMenu;
        }
        // v obracenem poradi je zhasnu
        int i;
        for (i = stack.Count - 1; i >= 0; i--)
        {
            iterator = stack[i];
            ShowWindow(iterator->HWindow, SW_HIDE);
        }
    }
    ShowWindow(HWindow, SW_HIDE);
}

void CMenuPopup::OnTimerTimeout()
{
    CALL_STACK_MESSAGE1("CMenuPopup::OnTimerTimeout()");
    //
    // rekurzivni funkce
    // hleda od tatky smerem k poslednimu diteti takovy popup, kde nekoresponduje
    // index vybrane polozky a otevreneho childa; takoveho childa pak zavre
    //
    // pokud novy index stoji na popupu, otevre ho
    //
    CMenuItem* selectedItem = NULL;
    if (SelectedItemIndex != -1)
        selectedItem = Items[SelectedItemIndex];
    if (OpenedSubMenu != NULL)
    {
        if (selectedItem == NULL || selectedItem->SubMenu != OpenedSubMenu)
        {
            CloseOpenedSubmenu();
            goto createLabel;
        }
        else
            OpenedSubMenu->OnTimerTimeout();
    }
    else
    {
    createLabel:
        if (selectedItem != NULL && selectedItem->SubMenu != NULL &&
            !(selectedItem->State & MENU_STATE_GRAYED)) // Petr: otevirani "grayed" submenu neni mozne
        {
            RECT itemRect;
            GetItemRect(SelectedItemIndex, &itemRect);
            selectedItem->SubMenu->SharedRes = SharedRes;
            selectedItem->SubMenu->TrackFlags = 0;
            if (selectedItem->SubMenu->CreatePopupWindow(FirstPopup, itemRect.right, itemRect.top - 3,
                                                         SelectedItemIndex, &itemRect))
                OpenedSubMenu = selectedItem->SubMenu;
        }
    }
}

void CMenuPopup::PaintAllItems(HRGN hUpdateRgn)
{
    // prevezmu si private DC
    HDC hPrivateDC = NOHANDLES(GetDC(HWindow));

    if (hUpdateRgn != NULL)
        SelectClipRgn(hPrivateDC, hUpdateRgn);
    /*
  RECT clipRect;
  int clipRectType = GetClipBox(hPrivateDC, &clipRect);

  if (clipRectType == NULLREGION || clipRectType == SIMPLEREGION)
  {
  }
*/
    HFONT hOldFont = (HFONT)SelectObject(hPrivateDC, SharedRes->HNormalFont);
    int topY = 0;
    if (UpArrowVisible)
        topY = UPDOWN_ITEM_HEIGHT;
    int y = TopItemY;
    int maxY = Height;
    if (DownArrowVisible)
        maxY = Height - UPDOWN_ITEM_HEIGHT; // vytvorime prostor pro sipku dolu
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;
        if (y + item->Height > maxY)
            break;

        DrawItem(hPrivateDC, item, y, SelectedItemIndex == i);
        y += item->Height;
    }
    // pokud jsou viditelne, zobrazime sipky
    if (UpArrowVisible)
        DrawUpDownItem(hPrivateDC, TRUE);
    if (DownArrowVisible)
        DrawUpDownItem(hPrivateDC, FALSE);
    SelectObject(hPrivateDC, hOldFont);

    if (hUpdateRgn != NULL)
        SelectClipRgn(hPrivateDC, NULL); // vykopneme clip region, pokud jsme ho nastavili
}

LRESULT
CMenuPopup::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CMenuPopup::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_SETCURSOR:
    {
        if (SharedRes->MenuBar != NULL && SharedRes->MenuBar->HelpMode)
        {
            SetCursor(HHelpCursor);
            return TRUE;
        }
        break;
    }

    case WM_USER_CLOSEMENU:
    {
        Closing = TRUE;
        SetEvent(SharedRes->HCloseEvent); // nechame rozjet message queue
        HideAll();
        return 0;
    }

    case WM_CREATE:
    {
        HDC hPrivateDC = NOHANDLES(GetDC(HWindow));
        SelectObject(hPrivateDC, SharedRes->HNormalFont);
        break;
    }

    case WM_DESTROY:
    {
        // posleme notifikaci, aby nam parovala k WM_USER_INITMENUPOPUP
        if (!(TrackFlags & MENU_TRACK_NONOTIFY))
            SendMessage(SharedRes->HParent, WM_USER_UNINITMENUPOPUP,
                        (WPARAM)(CGUIMenuPopupAbstract*)this, MAKELPARAM(0, (WORD)ID));
        break;
    }

    case WM_ERASEBKGND:
    {
        HDC hDC = (HDC)wParam;
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, HDialogBrush);
        PatBlt(hDC, 0, 0, Width, TotalHeight, PATCOPY);
        SelectObject(hDC, hOldBrush);
        return 1;
    }

    case WM_MOUSEACTIVATE:
    {
        return MA_NOACTIVATE;
    }

    case WM_LBUTTONUP:
    {
        return 0;
    }

    case WM_USER_MOUSEWHEEL:
    {
        OnMouseWheel(wParam, lParam);
        return 0;
    }

    case WM_NCPAINT:
    {
        HDC hdc = HANDLES(GetWindowDC(HWindow));
        RECT r;
        GetWindowRect(HWindow, &r);
        r.right = r.right - r.left;
        r.bottom = r.bottom - r.top;
        r.left = 0;
        r.top = 0;

        // tmavsi ramecek kolem menu
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, HMenuGrayTextBrush);
        PatBlt(hdc, 0, 0, r.right, 1, PATCOPY);
        PatBlt(hdc, r.right - 1, 0, r.right, r.bottom, PATCOPY);
        PatBlt(hdc, 0, r.bottom - 1, r.right, r.bottom, PATCOPY);
        PatBlt(hdc, 0, 0, 1, r.bottom, PATCOPY);
        SelectObject(hdc, hOldBrush);

        // vnitrek obycejne pozadi
        InflateRect(&r, -1, -1);
        FillRect(hdc, &r, HDialogBrush);

        HANDLES(ReleaseDC(HWindow, hdc));
        return 0;
    }

    case WM_PAINT:
    {
        // nepojedeme pres CACHE - nakreslime to rovnou do obrazovky
        // vzheldem k efektu otevirani nebude mrkani viditelne
        PAINTSTRUCT ps;
        HANDLES(BeginPaint(HWindow, &ps));
        PaintAllItems(NULL);
        HANDLES(EndPaint(HWindow, &ps)); // pouze zhasina caret, ktery stejne nemame
        return 0;
    }
        /*
    // podpora pro animaci
    case WM_PRINTCLIENT:
    {
      HDC hDC = (HDC) wParam; 
      if (lParam & PRF_CLIENT)
      {
        int y = 0;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
          CMenuItem *item = Items[i];
          DrawItem(hDC, item, y, FALSE);
          y += item->Height;
        }
      }
      return 0;
    }
*/
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

DWORD
CMenuPopup::Track(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude)
{
    CALL_STACK_MESSAGE4("CMenuPopup::Track(0x%X, %d, %d, , )", trackFlags, x, y);
    MSG msg;
    BOOL dispatchMsg;

    // zahakujeme tento thread
    HHOOK hOldHookProc = OldMenuHookTlsAllocator.HookThread();

    if (!(trackFlags & MENU_TRACK_NONOTIFY))
        SendMessage(hwnd, WM_USER_ENTERMENULOOP, 0, 0);

    SelectedByMouse = FALSE; // v 2.5b9 nebyla promenna inicializovana a ChangeDeriveMenu Alt+F1/2
                             // se chovalo nahodile -- pohnuti kurzorem mimo menu obcas zpusobilo
                             // ztratu selectiony
                             // pokud se toto chovani nebude hodit pro CMenuPopup::Track(), je
                             // vec asi zrala na zavedeni ridiciho flagu v trackFlags
    DWORD retValue = TrackInternal(trackFlags, x, y, hwnd, exclude, NULL, msg, dispatchMsg);

    if (!(trackFlags & MENU_TRACK_NONOTIFY))
        SendMessage(hwnd, WM_USER_LEAVEMENULOOP, 0, 0);

    // pokud jsme hookovali, budeme take uvolnovat
    if (hOldHookProc != NULL)
        OldMenuHookTlsAllocator.UnhookThread(hOldHookProc);

    // prichazi dorucovani vysledku - stejna vec se resi v CMenuBar::EnterMenuInternal
    // simulujeme pohyb mysi, aby si ji ostatni mohli zachytit
    POINT cursorPos;
    GetCursorPos(&cursorPos);
    HWND hWndUnderCursor = WindowFromPoint(cursorPos);
    if (hWndUnderCursor != NULL)
    {
        ScreenToClient(hWndUnderCursor, &cursorPos);
        SendMessage(hWndUnderCursor, WM_MOUSEMOVE, 0, MAKELPARAM(cursorPos.x, cursorPos.y));
    }
    // dorucime opozdenou message
    if (dispatchMsg)
    {
        TranslateMessage(&msg);
        PostMessage(msg.hwnd, msg.message, msg.wParam, msg.lParam);
    }
    // vratime command
    DWORD ret = TRUE;

    if (trackFlags & MENU_TRACK_RETURNCMD)
        ret = retValue;
    else
    {
        if (retValue != 0)
            PostMessage(hwnd, WM_COMMAND, retValue, 0);
    }

    if (!(trackFlags & MENU_TRACK_NONOTIFY))
        PostMessage(hwnd, WM_USER_LEAVEMENULOOP2, 0, 0);
    return ret;
}

DWORD
CMenuPopup::TrackInternal(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude,
                          CMenuBar* menuBar, MSG& delayedMsg, BOOL& dispatchDelayedMsg)
{
    CALL_STACK_MESSAGE5("CMenuPopup::TrackInternal(0x%X, %d, %d, , , , , %d)",
                        trackFlags, x, y, dispatchDelayedMsg);
    //  TRACE_I("CMenuPopup::TrackInternal begin");
    dispatchDelayedMsg = FALSE;
    // vytvorim sdilene prostredky
    SharedRes = new CMenuSharedResources();
    if (SharedRes == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return 0;
    }
    SharedRes->MenuBar = menuBar;
    SharedRes->ExcludeRect = exclude;
    SharedRes->SkillLevel = SkillLevel;
    if (menuBar != NULL)
    {
        SharedRes->MenuBar->IndexToOpen = -1;
        SharedRes->MenuBar->OpenWithSelect = FALSE;
    }

    if (trackFlags & MENU_TRACK_HIDEACCEL)
    {
        // pokud je v systemu vnucene zobrazovani podtrzitek, budeme flag MENU_TRACK_HIDEACCEL ignorovat
        BOOL alwaysVisible;
        if (SystemParametersInfo(SPI_GETKEYBOARDCUES, 0, &alwaysVisible, FALSE) == 0)
            alwaysVisible = TRUE;
        if (!alwaysVisible)
            SharedRes->HideAccel = TRUE;
    }

    // tady nam to padalo s exception po nasazeni Microsoft Application Verifier
    // event jeste nemusi existovat
    if (SharedRes->HCloseEvent != NULL)
        ResetEvent(SharedRes->HCloseEvent);

    TrackFlags = trackFlags;

    // necham je inicializovat
    if (!SharedRes->Create(hwnd, 1, 1))
        return 0;

    DWORD retValue = 0;

    BeginStopRefresh();     // neprejeme si zadne refreshe
    BeginStopIconRepaint(); // neprejeme si zadne repainty ikon

    // otevru okno - tatku
    //  TRACE_I("MENU: creating window");
    if (CreatePopupWindow(this, x, y, 0, exclude))
    {
        if (GetCapture() != NULL)
        {
            //      TRACE_I("MENU: releasing capture");
            ReleaseCapture();
        }

        // pridame se do monitoringu zaviracich zprav
        MenuWindowQueue.Add(HWindow);

        MSG msg;
        BOOL leaveMenu = FALSE;
        BOOL skipFirstLBtnUp = TRUE;
        BOOL skipFirstLBtnDblclk = TRUE;
        do
        {
            if (Closing)
            {
                leaveMenu = TRUE;
                if (SharedRes->MenuBar != NULL)              // jsme otevreni z menubar
                    SharedRes->MenuBar->ExitMenuLoop = TRUE; // takze z ni vypadneme
            }
            else
            {
                //        TRACE_I("MENU: entering message queue");
                if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE) != 0)
                {
                    if (!leaveMenu)
                    {
                        BOOL dispatchLater = FALSE;
                        if (skipFirstLBtnDblclk)
                        {
                            if (msg.message == WM_LBUTTONDOWN)
                                skipFirstLBtnDblclk = FALSE;
                            if (msg.message == WM_LBUTTONDBLCLK)
                                continue; // skipneme message, protoze je to preklep po lbuttondown
                        }
                        if (skipFirstLBtnUp)
                        {
                            if (msg.message == WM_LBUTTONDOWN)
                                skipFirstLBtnUp = FALSE;
                            if (msg.message == WM_LBUTTONUP)
                            {
                                skipFirstLBtnUp = FALSE;
                                if (FindPopup(PopupWindowFromPoint(msg.pt)) == NULL)
                                    continue; // skipneme message, aby nedoslo k okamzitemu zavreni okna
                            }
                        }
                        DoDispatchMessage(&msg, &leaveMenu, &retValue, &dispatchLater);
                        if (dispatchLater)
                        {
                            delayedMsg = msg;
                            dispatchDelayedMsg = TRUE;
                        }
                    }
                }
                else
                {
                    DWORD timeOut = INFINITE;
                    if (SharedRes->ChangeTickCount != INFINITE)
                    {
                        DWORD delta = GetTickCount() - SharedRes->ChangeTickCount;
                        DWORD delay;
                        if (SystemParametersInfo(SPI_GETMENUSHOWDELAY, 0, &delay, FALSE) == 0)
                            delay = 400;
                        if (delta > delay)
                            timeOut = 0;
                        else
                            timeOut = delay - delta;
                    }

                    DWORD ret = MsgWaitForMultipleObjects(1, &SharedRes->HCloseEvent, FALSE, timeOut, QS_ALLINPUT);
                    if (ret != 0xFFFFFFFF)
                    {
                        if (timeOut != INFINITE &&
                            (ret == WAIT_TIMEOUT || GetTickCount() - SharedRes->ChangeTickCount > timeOut))
                        {
                            SharedRes->ChangeTickCount = INFINITE;
                            OnTimerTimeout();
                        }
                    }
                    else
                        TRACE_E("MsgWaitForMultipleObjects failed");
                }
            }
        } while (!leaveMenu);

        CloseOpenedSubmenu();

        // vyhodime se z monitoringu zaviracich zprav
        MenuWindowQueue.Remove(HWindow);

        DestroyWindow(HWindow);
    }
    EndStopIconRepaint();
    EndStopRefresh();
    delete SharedRes;
    SharedRes = NULL;
    //  TRACE_I("CMenuPopup::TrackInternal end");
    return retValue;
}
