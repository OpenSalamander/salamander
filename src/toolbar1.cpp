// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "bitmap.h"
#include "toolbar.h"

//*****************************************************************************
//
// CToolBarItem
//

CToolBarItem::CToolBarItem()
{
    CALL_STACK_MESSAGE_NONE
    Style = 0;
    State = 0;
    ID = 0;
    Text = NULL;
    TextLen = 0;
    ImageIndex = -1;
    HIcon = NULL;
    HOverlay = NULL;
    CustomData = 0;
    Enabler = NULL;
    Width = 0;
    Height = 0;
    Offset = 0;
    Name = NULL;
}

CToolBarItem::~CToolBarItem()
{
    CALL_STACK_MESSAGE_NONE
    if (Text != NULL)
        free(Text);
    if (Name != NULL)
        free(Name);
}

BOOL CToolBarItem::SetText(const char* text, int len)
{
    CALL_STACK_MESSAGE_NONE
    if (text != NULL && len == -1)
        len = lstrlen(text);

    if (Text != NULL)
    {
        free(Text);
        Text = NULL;
        TextLen = 0;
    }

    if (text != NULL)
    {
        Text = (char*)malloc(len + 1);
        if (Text == NULL)
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        if (len > 0)
            memmove(Text, text, len);
        Text[len] = 0;
        TextLen = len;
    }
    return TRUE;
}

//*****************************************************************************
//
// CToolBar
//

CToolBar::CToolBar(HWND hNotifyWindow, CObjectOrigin origin)
    : CWindow(origin), Items(20, 20)
{
    CALL_STACK_MESSAGE_NONE
    Width = 0;
    Height = 0;
    HFont = NULL;
    FontHeight = 0;
    HNotifyWindow = hNotifyWindow;
    HImageList = NULL;
    HHotImageList = NULL;
    Style = TLB_STYLE_IMAGE;
    ImageWidth = 0;
    ImageHeight = 0;
    DirtyItems = FALSE;
    CacheWidth = 0;
    CacheHeight = 0;
    HotIndex = -1;
    DownIndex = -1;
    MonitorCapture = TRUE;
    RelayToolTip = TRUE;
    CacheBitmap = NULL;
    MonoBitmap = NULL;
    Padding.ToolBarVertical = 0;
    Padding.ButtonIconText = 3;
    Padding.IconLeft = 2;
    Padding.IconRight = 2;
    Padding.TextLeft = 4;
    Padding.TextRight = 4;
    HasIcon = FALSE;
    HasIconDirty = FALSE;
    Customizing = FALSE;
    InserMarkIndex = -1;
    InserMarkAfter = FALSE;
    MouseIsTracked = FALSE;
    DropDownUpTime = GetTickCount();
    HelpMode = FALSE;
}

CToolBar::~CToolBar()
{
    CALL_STACK_MESSAGE1("CToolBar::~CToolBar()");
    // destruction is still in WM_DESTROY
    if (CacheBitmap != NULL)
    {
        delete CacheBitmap;
        CacheBitmap = NULL;
    }
    if (MonoBitmap != NULL)
    {
        delete MonoBitmap;
        MonoBitmap = NULL;
    }
}

BOOL CToolBar::CreateWnd(HWND hParent)
{
    CALL_STACK_MESSAGE1("CToolBar::CreateWnd()");
    if (HWindow != NULL)
    {
        TRACE_E("HWindow != NULL");
        return TRUE;
    }

    Create(CWINDOW_CLASSNAME2,
           NULL,
           WS_CHILD | WS_CLIPSIBLINGS,
           0, 0, 0, 0, // dummy
           hParent,
           (HMENU)0,
           HInstance,
           this);

    if (HWindow == NULL)
    {
        TRACE_E("CToolBar::CreateWnd failed.");
        return FALSE;
    }

    if (CacheBitmap != NULL)
    {
        TRACE_E("CacheBitmap != NULL");
    }

    CacheBitmap = new CBitmap();
    if (CacheBitmap == NULL || !CacheBitmap->CreateBmp(NULL, 1, 1))
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    SetFont();
    SetBkMode(CacheBitmap->HMemDC, TRANSPARENT);

    return TRUE;
}

int CToolBar::GetNeededWidth()
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return 0;
    }
    int width = 0;
    if (Style & TLB_STYLE_VERTICAL)
    {
        if (Style & TLB_STYLE_IMAGE)
        {
            if (HImageList != NULL)
                width = max(width, Padding.IconLeft + ImageWidth + Padding.IconRight);
            if (HasIcon)
            {
                int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
                width = max(width, Padding.IconLeft + iconSize + Padding.IconRight);
            }
        }
        Refresh();
        int i;
        for (i = 0; i < Items.Count; i++)
            width = max(width, Items[i]->Width);
    }
    else
    {
        if (Items.Count > 0)
        {
            Refresh();
            CToolBarItem* item = Items[Items.Count - 1];
            width = item->Offset + item->Width;
        }
    }
    return width;
}

int CToolBar::GetNeededHeight()
{
    CALL_STACK_MESSAGE_NONE
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return 0;
    }
    int height = 0;
    if (Style & TLB_STYLE_VERTICAL)
    {
        if (Items.Count > 0)
        {
            Refresh();
            CToolBarItem* item = Items[Items.Count - 1];
            height = item->Offset + item->Height;
        }
    }
    else
    {
        if (HasIconDirty)
        {
            // Let's check if we are holding any icon
            HasIcon = FALSE;
            HasIconDirty = FALSE;
            int i;
            for (i = 0; i < Items.Count; i++)
            {
                if (Items[i]->HIcon != NULL)
                {
                    HasIcon = TRUE;
                    break;
                }
            }
        }
        if (Style & TLB_STYLE_TEXT)
            height = 3 + FontHeight + 3;
        if (Style & TLB_STYLE_IMAGE)
        {
            if (HImageList != NULL)
                height = max(height, 3 + ImageHeight + 3);
            if (HasIcon)
                height = max(height, 3 + GetIconSizeForSystemDPI(ICONSIZE_16) + 3);
        }

        height += 2 * Padding.ToolBarVertical;
    }
    return height;
}

BOOL CToolBar::GetItemRect(int index, RECT& r)
{
    CALL_STACK_MESSAGE2("CToolBar::GetItemRect(%d, )", index);
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return FALSE;
    }
    if (index < 0 || index >= Items.Count)
    {
        TRACE_E("CToolBar::GetItemRect index is out of range.");
        return FALSE;
    }
    Refresh();
    POINT p;
    p.x = 0;
    p.y = 0;
    ClientToScreen(HWindow, &p);
    CToolBarItem* item = Items[index];
    if (Style & TLB_STYLE_VERTICAL)
    {
        r.left = p.x + (Width - item->Width) / 2;
        r.top = p.y + item->Offset;
    }
    else
    {
        r.left = p.x + item->Offset;
        r.top = p.y + (Height - item->Height) / 2;
    }
    r.right = r.left + item->Width;
    r.bottom = r.top + item->Height;
    return TRUE;
}

void CToolBar::SetImageList(HIMAGELIST hImageList)
{
    CALL_STACK_MESSAGE1("CToolBar::SetImageList()");
    HImageList = hImageList;
    if (HImageList != NULL)
    {
        ImageList_GetIconSize(HImageList, &ImageWidth, &ImageHeight);
        if (MonoBitmap != NULL)
            MonoBitmap->Enlarge(ImageWidth, ImageHeight);
    }
    else
    {
        ImageWidth = 0;
        ImageHeight = 0;
    }
    DirtyItems = TRUE;
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

HIMAGELIST
CToolBar::GetImageList()
{
    CALL_STACK_MESSAGE_NONE
    return HImageList;
}

void CToolBar::SetHotImageList(HIMAGELIST hImageList)
{
    CALL_STACK_MESSAGE_NONE
    HHotImageList = hImageList;
    /*    if (HImageList != NULL)
  {
    ImageList_GetIconSize(HImageList, &ImageWidth, &ImageHeight);
    if (MonoBitmap != NULL)
      MonoBitmap->Enlarge(ImageWidth, ImageHeight);
  }
  else
  {
    ImageWidth = 0;
    ImageHeight = 0;
  }*/
    DirtyItems = TRUE;
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

HIMAGELIST
CToolBar::GetHotImageList()
{
    CALL_STACK_MESSAGE_NONE
    return HHotImageList;
}

int CToolBar::FindItemPosition(DWORD id)
{
    CALL_STACK_MESSAGE_NONE
    int i;
    for (i = 0; i < Items.Count; i++)
        if (Items[i]->ID == id)
            return i;
    return -1;
}

BOOL CToolBar::InsertItem2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii)
{
    CALL_STACK_MESSAGE3("CToolBar::InsertItem2(0x%X, %d, )", position, byPosition);
    int newPos;
    // Find the position where the new item will come
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
            TRACE_E("CToolBar::InsertItem2 item with id=" << position << " was not found");
            return FALSE;
        }
    }

    // allocate item
    CToolBarItem* item = new CToolBarItem();
    if (item == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    // insert item into array
    Items.Insert(newPos, item);
    if (!Items.IsGood())
    {
        Items.ResetState();
        delete item;
        return FALSE;
    }

    // set data
    if (!SetItemInfo2(newPos, TRUE, tii))
    {
        Items.Delete(newPos);
        return FALSE;
    }
    return TRUE;
}

BOOL CToolBar::SetItemInfo2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii)
{
    CALL_STACK_MESSAGE3("CToolBar::SetItemInfo2(0x%X, %d, )", position, byPosition);
    int newPos;
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::SetItemInfo2 item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CToolBar::SetItemInfo2 item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CToolBarItem* item = Items[newPos];

    if (tii->Mask & TLBI_MASK_STYLE)
        item->Style = tii->Style;

    if (tii->Mask & TLBI_MASK_STATE)
        item->State = tii->State;

    if (tii->Mask & TLBI_MASK_ID)
    {
        item->ID = tii->ID;
        if (tii->ID == 0)
            item->State |= TLBI_STATE_GRAYED;
    }

    if (tii->Mask & TLBI_MASK_IMAGEINDEX)
        item->ImageIndex = tii->ImageIndex;

    if (tii->Mask & TLBI_MASK_ICON)
    {
        BOOL hadIcon = item->HIcon != NULL;

        item->HIcon = tii->HIcon;

        if (item->HIcon != NULL)
        {
            HasIcon = TRUE;
            HasIconDirty = FALSE;
        }
        else if (hadIcon)
        {
            HasIconDirty = TRUE; // We don't know if there are any icons left - we will need to find out.
        }
    }

    if (tii->Mask & TLBI_MASK_OVERLAY)
        item->HOverlay = tii->HOverlay;

    if (tii->Mask & TLBI_MASK_WIDTH && item->Style & TLBI_STYLE_FIXEDWIDTH)
        item->Width = tii->Width;

    if (tii->Mask & TLBI_MASK_TEXT)
    {
        if (!item->SetText(tii->Text, (tii->Mask & TLBI_MASK_TEXTLEN) ? tii->TextLen : -1))
            return FALSE;
    }

    if (tii->Mask & TLBI_MASK_CUSTOMDATA)
        item->CustomData = tii->CustomData;

    if (tii->Mask & TLBI_MASK_ENABLER)
        item->Enabler = tii->Enabler;

    if (HWindow != NULL)
    {
        if (tii->Mask & TLBI_MASK_TEXT || tii->Mask & TLBI_MASK_STYLE ||
            tii->Mask & TLBI_MASK_IMAGEINDEX || tii->Mask & TLBI_MASK_ICON ||
            (tii->Mask & TLBI_MASK_WIDTH && item->Style & TLBI_STYLE_FIXEDWIDTH))
        {
            // this change can have an impact on the entire toolbar
            DirtyItems = TRUE;
            if (HWindow != NULL)
                InvalidateRect(HWindow, NULL, FALSE);
        }
        else
        {
            // We will only redraw one button that was changing
            if ((tii->Mask & TLBI_MASK_STATE) && HWindow != NULL)
            {
                RECT r;
                if (Style & TLB_STYLE_VERTICAL)
                {
                    r.top = item->Offset;
                    r.left = (Width - item->Width) / 2;
                }
                else
                {
                    r.left = item->Offset;
                    r.top = (Height - item->Height) / 2;
                }
                r.right = r.left + item->Width;
                r.bottom = r.top + item->Height;
                InvalidateRect(HWindow, &r, FALSE);
            }
        }
    }
    return TRUE;
}

BOOL CToolBar::GetItemInfo2(DWORD position, BOOL byPosition, TLBI_ITEM_INFO2* tii)
{
    CALL_STACK_MESSAGE3("CToolBar::GetItemInfo2(0x%X, %d, )", position, byPosition);
    Refresh();
    int newPos;
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::GetItemInfo2 item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CToolBar::GetItemInfo2 item with id=" << position << " was not found");
            return FALSE;
        }
    }

    CToolBarItem* item = Items[newPos];

    if (tii->Mask & TLBI_MASK_STYLE)
        tii->Style = item->Style;

    if (tii->Mask & TLBI_MASK_STATE)
        tii->State = item->State;

    if (tii->Mask & TLBI_MASK_ID)
        tii->ID = item->ID;

    if (tii->Mask & TLBI_MASK_IMAGEINDEX)
        tii->ImageIndex = item->ImageIndex;

    if (tii->Mask & TLBI_MASK_ICON)
        tii->HIcon = item->HIcon;

    if (tii->Mask & TLBI_MASK_OVERLAY)
        tii->HOverlay = item->HOverlay;

    if (tii->Mask & TLBI_MASK_WIDTH)
        tii->Width = item->Width;

    if (tii->Mask & TLBI_MASK_ENABLER)
        tii->Enabler = item->Enabler;

    if (tii->Mask & TLBI_MASK_TEXT)
    {
        memmove(tii->Text, item->Text, item->TextLen);
        tii->Text[item->TextLen] = 0;
    }

    if (tii->Mask & TLBI_MASK_CUSTOMDATA)
        tii->CustomData = item->CustomData;

    return TRUE;
}

BOOL CToolBar::CheckItem(DWORD position, BOOL byPosition, BOOL checked)
{
    CALL_STACK_MESSAGE4("CToolBar::CheckItem(0x%X, %d, %d)", position, byPosition, checked);
    int newPos;
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::CheckItem item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            //      TRACE_E("CToolBar::CheckItem item with id="<<position<<" was not found");
            return FALSE;
        }
    }

    CToolBarItem* item = Items[newPos];
    if (((item->State & TLBI_STATE_CHECKED) != 0) != checked)
    {
        DWORD newState = item->State & ~TLBI_STATE_CHECKED;
        if (checked)
            newState |= TLBI_STATE_CHECKED;

        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STATE;
        tii.State = newState;
        return SetItemInfo2(newPos, TRUE, &tii);
    }
    else
        return TRUE;
}

BOOL CToolBar::EnableItem(DWORD position, BOOL byPosition, BOOL enabled)
{
    CALL_STACK_MESSAGE4("CToolBar::EnableItem(0x%X, %d, %d)", position, byPosition, enabled);
    int newPos;
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::EnableItem item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            //      TRACE_E("CToolBar::EnableItem item with id="<<position<<" was not found");
            return FALSE;
        }
    }

    CToolBarItem* item = Items[newPos];
    if (((item->State & TLBI_STATE_GRAYED) == 0) != enabled)
    {
        DWORD newState = item->State & ~TLBI_STATE_GRAYED;
        if (!enabled)
            newState |= TLBI_STATE_GRAYED;

        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STATE;
        tii.State = newState;
        return SetItemInfo2(newPos, TRUE, &tii);
    }
    else
        return TRUE;
}

BOOL CToolBar::ReplaceImage(DWORD position, BOOL byPosition, HICON hIcon, BOOL normal, BOOL hot)
{
    CALL_STACK_MESSAGE5("CToolBar::ReplaceImage(0x%X, %d, , %d, %d)", position,
                        byPosition, normal, hot);
    if (!normal && !hot || !normal && HHotImageList == NULL)
    {
        TRACE_E("!normal && !hot || !normal && HHotImageList == NULL");
        return FALSE;
    }
    int newPos;
    if (HImageList == NULL)
    {
        TRACE_E("CToolBar::ReplaceImage HImageList == NULL");
        return FALSE;
    }
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::ReplaceImage item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
            return FALSE;
    }
    CToolBarItem* item = Items[newPos];
    if (Items[newPos]->ImageIndex == -1)
    {
        TRACE_E("CToolBar::ReplaceImage ImageIndex == -1.");
        return FALSE;
    }
    if (normal)
        ImageList_ReplaceIcon(HImageList, Items[newPos]->ImageIndex, hIcon);
    if (hot && HHotImageList != NULL)
        ImageList_ReplaceIcon(HHotImageList, Items[newPos]->ImageIndex, hIcon);
    DrawItem(newPos);
    return TRUE;
}

void CToolBar::RemoveAllItems()
{
    CALL_STACK_MESSAGE1("CToolBar::RemoveAllItems()");
    if (Items.Count > 0)
    {
        Items.DestroyMembers();
        DirtyItems = TRUE;
        if (HWindow != NULL)
            InvalidateRect(HWindow, NULL, FALSE);
    }
}

BOOL CToolBar::RemoveItem(DWORD position, BOOL byPosition)
{
    CALL_STACK_MESSAGE3("CToolBar::RemoveItem(0x%X, %d)", position, byPosition);
    int newPos;
    if (byPosition)
    {
        newPos = position;
        if (newPos < 0 || newPos >= Items.Count)
        {
            TRACE_E("CToolBar::RemoveItem item with position=" << position << " was not found");
            return FALSE;
        }
    }
    else
    {
        newPos = FindItemPosition(position);
        if (newPos == -1)
        {
            TRACE_E("CToolBar::RemoveItem item with id=" << position << " was not found");
            return FALSE;
        }
    }
    Items.Delete(newPos);
    DirtyItems = TRUE;
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
    return TRUE;
}

void CToolBar::SetStyle(DWORD style)
{
    CALL_STACK_MESSAGE2("CToolBar::SetStyle(0x%X)", style);
    if ((style & TLB_STYLE_TEXT) && (style & TLB_STYLE_VERTICAL))
    {
        TRACE_E("CToolBar::SetStyle you cannot set TLB_STYLE_TEXT and TLB_STYLE_VERTICAL at the same time.");
        style &= ~TLB_STYLE_TEXT;
    }
    DWORD oldStyle = Style;
    Style = style;
    // if the text display has changed, I will reallocate
    if ((oldStyle & TLB_STYLE_TEXT) != (Style & TLB_STYLE_TEXT))
        SetFont();
    DirtyItems = TRUE;
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

DWORD
CToolBar::GetStyle()
{
    CALL_STACK_MESSAGE_NONE
    return Style;
}

void CToolBar::GetPadding(TOOLBAR_PADDING* padding)
{
    CALL_STACK_MESSAGE1("CToolBar::GetPadding()");
    *padding = Padding;
}

void CToolBar::SetPadding(const TOOLBAR_PADDING* padding)
{
    CALL_STACK_MESSAGE1("CToolBar::SetPadding()");
    Padding = *padding;
    DirtyItems = TRUE;
    if (HWindow != NULL)
        InvalidateRect(HWindow, NULL, FALSE);
}

/*  int
CToolBar::SetHotItem(int index)
{
  if (GetCapture() == HWindow)
}*/

void CToolBar::UpdateItemsState()
{
    CALL_STACK_MESSAGE1("CToolBar::UpdateItemsState()");
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CToolBarItem* item = Items[i];

        if (item->Enabler != NULL)
        {
            // bit TLBI_STATE_GRAYED is controlled
            BOOL enabled = (item->State & TLBI_STATE_GRAYED) == 0;
            BOOL enabledSrc = *item->Enabler != 0;
            if (enabled != enabledSrc)
            {
                DWORD newState = item->State;
                if (enabledSrc)
                    newState &= ~TLBI_STATE_GRAYED;
                else
                    newState |= TLBI_STATE_GRAYED;

                TLBI_ITEM_INFO2 tii;
                tii.Mask = TLBI_MASK_STATE;
                tii.State = newState;
                SetItemInfo2(i, TRUE, &tii);
            }
        }
    }
}

void CToolBar::OnColorsChanged()
{
    // if there is a color bitmap, we let it be prebuilt for the current color depth
    if (CacheBitmap != NULL)
        CacheBitmap->ReCreateForScreenDC();
}

LRESULT
CToolBar::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CToolBar::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_DESTROY:
    {
        // destruction is still in the destructor
        if (CacheBitmap != NULL)
        {
            delete CacheBitmap;
            CacheBitmap = NULL;
        }
        if (MonoBitmap != NULL)
        {
            delete MonoBitmap;
            MonoBitmap = NULL;
        }
        break;
    }

    case WM_SIZE:
    {
        if (HWindow != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);
            Width = r.right;
            Height = r.bottom;
            if (CacheBitmap != NULL)
            {
                if (Style & TLB_STYLE_VERTICAL)
                    CacheBitmap->Enlarge(Width, 1);
                else
                    CacheBitmap->Enlarge(1, Height);
            }
        }
        break;
    }

    case WM_ERASEBKGND:
    {
        if (WindowsVistaAndLater) // under the flashing rib
            return TRUE;
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect((HDC)wParam, &r, HDialogBrush);
        return TRUE;
    }

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
        DrawAllItems(hDC);
        HANDLES(EndPaint(HWindow, &ps));
        return 0;
    }

    case WM_USER_HELP_MOUSELEAVE:
    {
        HelpMode = FALSE;
    }
    case WM_CAPTURECHANGED:
    case WM_MOUSELEAVE:
    case WM_CANCELMODE:
    {
        MouseIsTracked = FALSE;
        SetCurrentToolTip(NULL, 0); // remove tooltip
        if (!MonitorCapture)
            break;
        if (HotIndex != -1)
        {
            int oldHotIndex = HotIndex;
            HotIndex = -1;
            if (DownIndex != -1)
            {
                Items[DownIndex]->State &= ~(TLBI_STATE_PRESSED | TLBI_STATE_DROPDOWNPRESSED);
                DownIndex = -1;
            }
            DrawItem(oldHotIndex);
        }
        break;
    }

    case WM_USER_HELP_MOUSEMOVE:
    {
        HelpMode = TRUE;
    }
    case WM_MOUSEMOVE:
    {
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);

        int index;
        BOOL dropDown;
        BOOL hitItem = HitTest(xPos, yPos, index, dropDown);
        int newHotIndex = -1;
        if (DownIndex == -1)
        {
            // mouse tracking
            if (hitItem)
            {
                if (!HelpMode && !MouseIsTracked)
                {
                    TRACKMOUSEEVENT tme;
                    tme.cbSize = sizeof(tme);
                    tme.dwFlags = TME_LEAVE;
                    tme.hwndTrack = HWindow;
                    TrackMouseEvent(&tme);
                    MouseIsTracked = TRUE;
                }
                newHotIndex = index;
            }
            else
            {
                if (GetCapture() == HWindow)
                    ReleaseCapture();
            }
        }
        else
        {
            // DownIndex != -1
            CToolBarItem* item = Items[DownIndex];
            if (hitItem && index == DownIndex)
            {
                newHotIndex = index;
                if (DropPressed)
                    item->State |= TLBI_STATE_DROPDOWNPRESSED;
                else
                    item->State |= TLBI_STATE_PRESSED;
            }
            else
            {
                newHotIndex = -1;
                item->State &= ~(TLBI_STATE_PRESSED | TLBI_STATE_DROPDOWNPRESSED);
            }
        }

        if (newHotIndex != HotIndex)
        {
            // draw changes
            int oldHotIndex = HotIndex;
            HotIndex = newHotIndex;
            if (oldHotIndex != -1)
                DrawItem(oldHotIndex);
            if (HotIndex != -1)
                DrawItem(HotIndex);
        }

        if (RelayToolTip && hitItem)
            SetCurrentToolTip(HWindow, index);
        else
            SetCurrentToolTip(NULL, 0);

        break;
    }

    case WM_LBUTTONDOWN:
    case WM_LBUTTONDBLCLK:
    {
        // if the click came within 25ms after unlocking the drop down, we discard it to prevent it
        // to unnecessary new pressing
        if (GetTickCount() - DropDownUpTime <= 25)
            break;

        SetCurrentToolTip(NULL, 0); // remove tooltip
        int xPos = (short)LOWORD(lParam);
        int yPos = (short)HIWORD(lParam);

        int index;
        BOOL dropDown;
        // If the Windows popup menu is open and we click into the toolbar, it will come right away
        // WM_LBUTTONDOWN so HotIndex == -1, therefore I am removing the condition index == HotIndex.
        if (HitTest(xPos, yPos, index, dropDown) /*&& index == HotIndex*/)
        {
            CToolBarItem* item = Items[index];
            if (!(item->State & TLBI_STATE_GRAYED))
            {
                item->State &= ~(TLBI_STATE_PRESSED | TLBI_STATE_DROPDOWNPRESSED);
                if (dropDown)
                    item->State |= TLBI_STATE_DROPDOWNPRESSED;
                else
                    item->State |= TLBI_STATE_PRESSED;
                DrawItem(index);
                DownIndex = index;
                DropPressed = dropDown;

                if (HotIndex != -1 && HotIndex != index)
                    DrawItem(HotIndex);
                HotIndex = index;

                if (GetCapture() != HWindow)
                    SetCapture(HWindow);

                if (item->Style & TLBI_STYLE_DROPDOWN || dropDown)
                {
                    RelayToolTip = FALSE;
                    BOOL oldMonitorCapture = MonitorCapture;
                    MonitorCapture = FALSE;
                    if (GetCapture() == HWindow)
                        ReleaseCapture();
                    WindowProc(WM_MOUSELEAVE, 0, 0);
                    SendMessage(HNotifyWindow, WM_USER_TBDROPDOWN, (WPARAM)HWindow, (LPARAM)DownIndex);
                    item->State &= ~(TLBI_STATE_PRESSED | TLBI_STATE_DROPDOWNPRESSED);
                    MonitorCapture = oldMonitorCapture;
                    POINT p;
                    GetCursorPos(&p);
                    ScreenToClient(HWindow, &p);
                    DownIndex = -1;
                    SendMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
                    if (HotIndex == index)
                        DrawItem(HotIndex); // if no change has occurred, I need to redraw the state
                    RelayToolTip = TRUE;
                    DropDownUpTime = GetTickCount();
                }
            }
        }
        else
        {
            if (uMsg == WM_LBUTTONDBLCLK)
                Customize();
        }
        break;
    }

    case WM_LBUTTONUP:
    {
        SetCurrentToolTip(NULL, 0); // remove tooltip
        if (DownIndex != -1)
        {
            int xPos = (short)LOWORD(lParam);
            int yPos = (short)HIWORD(lParam);

            int index = -1;
            BOOL dropDown;
            if (HitTest(xPos, yPos, index, dropDown) && index == DownIndex)
            {
                if (Items[index]->ID != 0)
                {
                    if (Items[index]->Style & TLBI_STYLE_RADIO)
                        Items[index]->State |= TLBI_STATE_CHECKED;
                    if (Items[index]->Style & TLBI_STYLE_CHECK)
                    {
                        if (Items[index]->State & TLBI_STATE_CHECKED)
                            Items[index]->State &= ~TLBI_STATE_CHECKED;
                        else
                            Items[index]->State |= TLBI_STATE_CHECKED;
                    }
                    PostMessage(HNotifyWindow, WM_COMMAND, MAKELPARAM(Items[index]->ID, 0), (LPARAM)HWindow);
                }
            }
            CToolBarItem* item = Items[DownIndex];
            item->State &= ~(TLBI_STATE_PRESSED | TLBI_STATE_DROPDOWNPRESSED);
            HotIndex = index;
            if (HotIndex == DownIndex)
                DrawItem(DownIndex);
            DownIndex = -1;
        }
        // I need to release the capture in order for WM_SYSCOMMANDs to work properly
        // when the button in the toolbar is clicked
        if (GetCapture() == HWindow)
            ReleaseCapture();
        break;
    }

    case WM_RBUTTONUP:
    {
        SetCurrentToolTip(NULL, 0); // remove tooltip
        NMHDR nmhdr;
        nmhdr.hwndFrom = HWindow;
        nmhdr.idFrom = (UINT_PTR)GetMenu(HWindow);
        nmhdr.code = NM_RCLICK;
        if (SendMessage(HNotifyWindow, WM_NOTIFY, (WPARAM)nmhdr.idFrom, (LPARAM)&nmhdr) == TRUE)
            return 0;
        break;
    }

    case WM_USER_TTGETTEXT:
    {
        DWORD index = (DWORD)wParam; // FIXME_X64 - verify typecasting to (DWORD)
        char* text = (char*)lParam;
        if (index >= 0 && index < (DWORD)Items.Count)
        {
            CToolBarItem* item = Items[index];
            if (item->Style & TLBI_STYLE_SEPARATOR)
                return 0; // separator does not have a tooltip
            TOOLBAR_TOOLTIP tt;
            tt.HToolBar = HWindow;
            tt.ID = item->ID;
            tt.Index = index;
            tt.Buffer = text;
            tt.CustomData = item->CustomData;
            SendMessage(HNotifyWindow, WM_USER_TBGETTOOLTIP, (WPARAM)HWindow, (LPARAM)&tt);
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
