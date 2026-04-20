// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "bitmap.h"
#include "menu.h"

#define COLUMN_L1_L2_MARGIN 5 // space between column L1 and L2
#define STANDARD_BITMAP_SIZE 17

//*****************************************************************************
//
// CMenuSharedResources
//

CMenuSharedResources::CMenuSharedResources()
{
    CALL_STACK_MESSAGE_NONE
    // colored solid brushes
    NormalBkColor = 0xFFFFFFFF;
    SelectedBkColor = 0xFFFFFFFF;
    NormalTextColor = 0xFFFFFFFF;
    SelectedTextColor = 0xFFFFFFFF;
    HilightColor = 0xFFFFFFFF;
    GrayTextColor = 0xFFFFFFFF;

    CacheBitmap = NULL;
    MonoBitmap = NULL;

    // temp DC
    HTempMemDC = NULL;
    HTemp2MemDC = NULL;

    // fonts
    HNormalFont = NULL;
    HBoldFont = NULL;

    // menu bitmaps
    HMenuBitmaps = NULL;
    MenuBitmapWidth = 0;

    // other
    HParent = NULL;
    TextItemHeight = 0;
    BitmapsZoom = 1;
    ChangeTickCount = INFINITE;
    HideAccel = FALSE;

    HCloseEvent = NULL;
}

CMenuSharedResources::~CMenuSharedResources()
{
    CALL_STACK_MESSAGE1("CMenuSharedResources::~CMenuSharedResources()");
    if (CacheBitmap != NULL)
        delete CacheBitmap;

    if (MonoBitmap != NULL)
        delete MonoBitmap;

    // temp DC
    if (HTempMemDC != NULL)
        HANDLES(DeleteDC(HTempMemDC));
    if (HTemp2MemDC != NULL)
        HANDLES(DeleteDC(HTemp2MemDC));

    // fonts
    if (HNormalFont != NULL)
        HANDLES(DeleteObject(HNormalFont));
    if (HBoldFont != NULL)
        HANDLES(DeleteObject(HBoldFont));

    // menu bitmaps
    if (HMenuBitmaps != NULL)
        HANDLES(DeleteObject(HMenuBitmaps));

    if (HCloseEvent != NULL)
        HANDLES(CloseHandle(HCloseEvent));
}

BOOL CMenuSharedResources::Create(HWND hParent, int width, int height)
{
    CALL_STACK_MESSAGE3("CMenuSharedResources::Create(, %d, %d)", width, height);
    HParent = hParent;

    // colors
    NormalBkColor = GetSysColor(COLOR_BTNFACE);
    SelectedBkColor = GetSysColor(COLOR_HIGHLIGHT);
    NormalTextColor = GetSysColor(COLOR_BTNTEXT);
    SelectedTextColor = GetSysColor(COLOR_HIGHLIGHTTEXT);
    HilightColor = GetSysColor(COLOR_3DHILIGHT);
    GrayTextColor = GetSysColor(COLOR_3DSHADOW);

    // generate a copy and a bold version from the menu font
    NONCLIENTMETRICS ncm;
    ncm.cbSize = sizeof(ncm);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);

    LOGFONT* lf = &ncm.lfMenuFont;
    HNormalFont = HANDLES(CreateFontIndirect(lf));
    lf->lfWeight = FW_BOLD;
    HBoldFont = HANDLES(CreateFontIndirect(lf));

    CacheBitmap = new CBitmap();
    HDC hDC = HANDLES(GetDC(HParent));
    CacheBitmap->CreateBmp(hDC, 1, 1);

    // create memory DC
    HTempMemDC = HANDLES(CreateCompatibleDC(NULL));
    HTemp2MemDC = HANDLES(CreateCompatibleDC(NULL));

    // get font sizes
    TEXTMETRIC tm;
    HFONT hOldFont = (HFONT)SelectObject(hDC, HNormalFont);
    GetTextMetrics(hDC, &tm);
    TextItemHeight = 3 + tm.tmHeight + 3;
    if (TextItemHeight < 2 + STANDARD_BITMAP_SIZE)
        TextItemHeight = 2 + STANDARD_BITMAP_SIZE;
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(NULL, hDC));

    // determine the scaling factor for bitmaps
    BitmapsZoom = (TextItemHeight - 2) / STANDARD_BITMAP_SIZE;
    if (BitmapsZoom < 1)
        BitmapsZoom = 1;

    // prepare bitmap for check marks
    MenuBitmapWidth = TextItemHeight - 3 - 3;
    HMenuBitmaps = HANDLES(CreateBitmap(MenuBitmapWidth * 1, MenuBitmapWidth, 1, 1, NULL));
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(HTempMemDC, HMenuBitmaps);
    RECT r;
    r.top = 0;
    r.left = 0;
    r.bottom = MenuBitmapWidth;
    r.right = r.left + MenuBitmapWidth;
    DrawFrameControl(HTempMemDC, &r, DFC_MENU, DFCS_MENUARROW);
    /*
  r.top = 0;
  r.left = 0;
  r.bottom = MenuBitmapWidth;
  r.right = r.left + MenuBitmapWidth;
  DrawFrameControl(HTempMemDC, &r, DFC_MENU, DFCS_MENUARROWRIGHT);
  r.left =  r.right;
  r.right = r.left + MenuBitmapWidth;
  DrawFrameControl(HTempMemDC, &r, DFC_MENU, DFCS_MENUARROW);
  SelectObject(HTempMemDC, hOldBitmap);
*/
    // rotate the left and right arrows to become up and down arrows
    //  RotateBitmap90(HMenuBitmaps, menuBitmapArrowU * MenuBitmapWidth, 0, menuBitmapArrowL * MenuBitmapWidth, 0, MenuBitmapWidth);
    //  RotateBitmap90(HMenuBitmaps, menuBitmapArrowD * MenuBitmapWidth, 0, menuBitmapArrowR * MenuBitmapWidth, 0, MenuBitmapWidth);

    // monochrome bitmap for masks
    MonoBitmap = new CBitmap();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    int bwWidth = max(MenuBitmapWidth, iconSize); // small icons must fit inside it
    MonoBitmap->CreateBmpBW(bwWidth, bwWidth);

    HCloseEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL)); // "non-signaled" state, manual

    GetCursorPos(&LastMouseMove);
    return TRUE;
}

//*****************************************************************************
//
// CMenuPopup
//

void CMenuPopup::LayoutColumns()
{
    CALL_STACK_MESSAGE1("CMenuPopup::LayoutColumns()");
    BOOL threeCol = Style & MENU_POPUP_THREECOLUMNS;

    TotalHeight = 0;

    int maxItemHeight = 0; // for the shared bitmap
    // set the height of all items
    // in the case of a string, let the column widths be calculated
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CALL_STACK_MESSAGE2("CMenuPopup::LayoutColumns_1(%d)", i);
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;
        int sectionWidth = 0;
        int itemHeight = 0;
        // find out the size of each item
        if (item->Type & MENU_TYPE_OWNERDRAW)
        {
            // obtain dimensions by sending a query
            MEASUREITEMSTRUCT mis;
            mis.CtlType = ODT_MENU;
            mis.CtlID = 0;
            mis.itemID = item->ID;
            mis.itemWidth = SharedRes->TextItemHeight;
            mis.itemHeight = SharedRes->TextItemHeight;
            mis.itemData = item->CustomData;
            SendMessage(SharedRes->HParent, WM_MEASUREITEM, 0, (LPARAM)&mis);
            item->Height = mis.itemHeight;
            item->MinWidth = mis.itemWidth;
        }
        else
        {
            if (item->Type & MENU_TYPE_STRING)
            {
                item->DecodeSubTextLenghtsAndWidths(SharedRes, threeCol);
                item->Height = SharedRes->TextItemHeight;
                item->MinWidth = 0;
                if (item->HBmpItem == HBMMENU_CALLBACK)
                {
                    // get the dimensions by querying (HBMMENU_CALLBACK is used by TortoiseCVS for icons in the context menu)
                    MEASUREITEMSTRUCT mis;
                    mis.CtlType = ODT_MENU;
                    mis.CtlID = 0;
                    mis.itemID = item->ID;
                    mis.itemWidth = SharedRes->TextItemHeight;
                    mis.itemHeight = SharedRes->TextItemHeight;
                    mis.itemData = item->CustomData;
                    SendMessage(SharedRes->HParent, WM_MEASUREITEM, 0, (LPARAM)&mis);
                    if ((int)mis.itemHeight > item->Height)
                        item->Height = mis.itemHeight;
                }
            }
            else
            {
                if (item->Type & MENU_TYPE_SEPARATOR)
                {
                    item->Height = 2 + 2 + 2;
                    item->MinWidth = SharedRes->TextItemHeight;
                }
                else
                {
                    {
                        // extract the dimensions for the bitmap
                        BITMAP bitmap;
                        if (!GetObject(item->HBmpItem, sizeof(bitmap), &bitmap))
                        {
                            // if GetObject fails, feed at least some values so we don't mess up the menu completely
                            TRACE_E("GetObject() failed!");
                            bitmap.bmWidth = 10;
                            bitmap.bmHeight = 10;
                        }
                        item->Height = 1 + bitmap.bmHeight + 1;
                        if (item->Height < SharedRes->TextItemHeight)
                            item->Height = SharedRes->TextItemHeight;
                        item->MinWidth = SharedRes->TextItemHeight + 1 + 2 + bitmap.bmWidth;

                        // ensure the mask is large enough
                        if (SharedRes->MonoBitmap->NeedEnlarge(bitmap.bmWidth, bitmap.bmHeight))
                            SharedRes->MonoBitmap->Enlarge(bitmap.bmWidth, bitmap.bmHeight);
                    }
                }
            }
        }
        TotalHeight += item->Height;
        if (item->Height > maxItemHeight)
            maxItemHeight = item->Height;
    }

    // the first column must have the same width across all items
    int col1MaxWidth = 0;
    if (threeCol)
    {
        int maxWidth = 0;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* item = Items[i];
            if (!(SharedRes->SkillLevel & item->SkillLevel))
                continue;
            if ((item->Type & MENU_TYPE_STRING) &&
                !(item->Type & MENU_TYPE_OWNERDRAW) &&
                (item->ColumnL1 != NULL && item->ColumnL1Width > col1MaxWidth))
                col1MaxWidth = item->ColumnL1Width;
        }
    }

    int maxWidth = 0;
    Width = 0;
    int yOffset = 0;
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;

        item->YOffset = yOffset;
        yOffset += item->Height;

        if (!(item->Type & MENU_TYPE_OWNERDRAW) && (item->Type & MENU_TYPE_STRING))
        {
            // width of the two left columns
            int columnWidth = 0;
            if (item->ColumnL1 != NULL)
            {
                item->ColumnL1X = 2;
                if (threeCol)
                    columnWidth += 2 + col1MaxWidth;
                else
                    columnWidth += 2 + item->ColumnL1Width;
            }
            if (item->ColumnL2 != NULL)
            {
                item->ColumnL2X = 2 + col1MaxWidth;
                if (item->ColumnL1 != NULL)
                {
                    columnWidth += COLUMN_L1_L2_MARGIN;
                    item->ColumnL2X += COLUMN_L1_L2_MARGIN;
                }
                columnWidth += item->ColumnL2Width;
            }
            // width of the right column
            if (item->ColumnR != NULL)
            {
                if (item->ColumnL1 != NULL || item->ColumnL2 != NULL)
                {
                    columnWidth += SharedRes->TextItemHeight; // separator for column R
                }
                columnWidth += item->ColumnRWidth;
            }
            if (columnWidth > maxWidth)
                maxWidth = columnWidth;
            // handle the menu stretching after the loop
        }
        else
        {
            // stretch the menu according to this item
            int width = 0;
            if (item->Type & MENU_TYPE_OWNERDRAW)
                width = item->MinWidth + SharedRes->TextItemHeight;
            else
                width = SharedRes->TextItemHeight + 1 + item->MinWidth +
                        SharedRes->TextItemHeight;

            if (Width < width)
                Width = width;
        }
    }
    if (Width < MinWidth)
        Width = MinWidth;
    maxWidth += SharedRes->TextItemHeight + 1 + SharedRes->TextItemHeight;
    if (Width < maxWidth)
        Width = maxWidth;

    // position column R
    for (i = 0; i < Items.Count; i++)
    {
        CMenuItem* item = Items[i];
        if (!(SharedRes->SkillLevel & item->SkillLevel))
            continue;
        if (!(item->Type & MENU_TYPE_OWNERDRAW) && (item->Type & MENU_TYPE_STRING))
        {
            if (item->ColumnR != NULL)
                item->ColumnRX = Width - SharedRes->TextItemHeight - 1 - item->ColumnRWidth - SharedRes->TextItemHeight;
        }
    }

    // extend the cache bitmap if necessary
    if (SharedRes->CacheBitmap->NeedEnlarge(SharedRes->TextItemHeight + 1, maxItemHeight))
        SharedRes->CacheBitmap->Enlarge(Width, maxItemHeight);
}

void CMenuPopup::DrawCheckBitmapVista(HDC hDC, CMenuItem* item, int yOffset, BOOL selected)
{
    CALL_STACK_MESSAGE_NONE

    if (item->HBmpItem != NULL)
    {
        // fill the entire area with the normal color
        HBRUSH hOldBrush = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDialogBrush);
        PatBlt(SharedRes->CacheBitmap->HMemDC, 0, 0, SharedRes->TextItemHeight + 1, item->Height, PATCOPY);
        SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush);

        // center the check mark if the line height is greater than the check mark height
        int myYOffset = 0;
        if (item->Height > SharedRes->TextItemHeight)
            myYOffset += (item->Height - SharedRes->TextItemHeight) / 2;

        HBITMAP hBitmap;
        /*
    if (item->State & MENU_STATE_CHECKED)
    {
      if (item->HBmpChecked != NULL)
        hBitmap = item->HBmpChecked; // they supplied their own checkmark
    }
    else
      hBitmap = item->HBmpUnchecked; // they supplied their own checkmark
    */
        hBitmap = item->HBmpItem;

        // the bitmap may actually be smaller than required - in that case, adjust the values
        BITMAP bitmap;
        GetObject(hBitmap, sizeof(bitmap), &bitmap);

        int bmpX = 0; // the position in the bitmap from which the image should be extracted
        // 2.12.2012: if I set the resolution to 105% DPI (or 125, 150, etc.) on Windows 7
        // and bmpW/bmpH were increased here using the original max() function,
        // the subsequent AlphaBlend() call failed and the icons were not displayed
        // I went through the change history in the repository and believe no workarounds belong here; the values must be assigned directly
        // tested under Windows 7 with various DPIs using TortoiseSVN and HG, SourceGear DiffMerge, BeyondCompare, Adobe Acrobat and everything looks OK
        int bmpW = bitmap.bmWidth;
        int bmpH = bitmap.bmHeight;
        int targetBmpW = bmpW; // final size
        int targetBmpH = bmpW;
        BOOL monoBitmap = TRUE;
        if (SharedRes->BitmapsZoom > 1 &&
            SharedRes->BitmapsZoom * bmpW <= SharedRes->BitmapsZoom * STANDARD_BITMAP_SIZE &&
            SharedRes->BitmapsZoom * bmpH <= SharedRes->BitmapsZoom * STANDARD_BITMAP_SIZE)
        {
            targetBmpW *= SharedRes->BitmapsZoom;
            targetBmpH *= SharedRes->BitmapsZoom;
        }
        monoBitmap = (bitmap.bmBitsPixel == 1);

        if (selected || item->State & MENU_STATE_CHECKED)
        {
            // paint the background
            RECT r;
            r.left = 0;
            r.top = myYOffset;
            r.right = 0 + SharedRes->TextItemHeight + 1;
            r.bottom = myYOffset + SharedRes->TextItemHeight;
            if (!selected && item->State & MENU_STATE_CHECKED && !(item->State & MENU_STATE_GRAYED))
            {
                // the item is checked - draw a dithered brush
                // it is not selected, so it is already painted with the correct color
                SetBrushOrgEx(SharedRes->CacheBitmap->HMemDC, 0, r.top, NULL);
                HBRUSH hOldBrush2 = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDitherBrush);
                int oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_BTNFACE));
                int oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_3DHILIGHT));
                PatBlt(SharedRes->CacheBitmap->HMemDC, r.left + 1, r.top + 1,
                       SharedRes->TextItemHeight - 1, SharedRes->TextItemHeight - 1,
                       PATCOPY);
                SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
                SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
                SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush2);
            }

            // draw the frame in the appropriate state
            if (item->State & MENU_STATE_GRAYED && selected || !(item->State & MENU_STATE_GRAYED))
            {
                DWORD mode = BDR_RAISEDINNER;
                if (!(item->State & MENU_STATE_GRAYED) && item->State & MENU_STATE_CHECKED)
                    mode = BDR_SUNKENOUTER;
                r.right--;
                DrawEdge(SharedRes->CacheBitmap->HMemDC, &r, mode, BF_RECT);
            }
        }

        // draw the actual bitmap
        int itemHeight = SharedRes->TextItemHeight - 2;
        int xO = (itemHeight - targetBmpW) / 2;
        int yO = (itemHeight - targetBmpH) / 2;

        if (!(item->State & MENU_STATE_GRAYED) && (item->State & MENU_STATE_CHECKED))
        {
            // if the button is pressed, shift the bitmap one pixel right and down
            xO++;
            yO++;
        }

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, hBitmap);

        BLENDFUNCTION bf;
        bf.BlendOp = AC_SRC_OVER;
        bf.BlendFlags = 0;
        bf.SourceConstantAlpha = 0xff; // want to use per-pixel alpha values
        bf.AlphaFormat = AC_SRC_ALPHA;

        if (item->State & MENU_STATE_GRAYED)
            bf.SourceConstantAlpha = 128; // gray

        AlphaBlend(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                   SharedRes->HTempMemDC, bmpX, 0, bmpW, bmpH, bf);
        BitBlt(hDC, 0, yOffset, SharedRes->TextItemHeight, item->Height,
               SharedRes->CacheBitmap->HMemDC, 0, 0, SRCCOPY);
    }
}

DWORD
CMenuPopup::GetOwnerDrawItemState(const CMenuItem* item, BOOL selected)
{
    DWORD state = 0;
    if (selected)
        state |= ODS_SELECTED;
    if (item->State & MENU_STATE_CHECKED)
        state |= ODS_CHECKED;
    if (item->State & MENU_STATE_GRAYED)
        state |= ODS_GRAYED;
    if (item->State & MFS_DEFAULT)
        state |= ODS_DEFAULT;
    return state;
}

void CMenuPopup::DrawCheckBitmap(HDC hDC, CMenuItem* item, int yOffset, BOOL selected)
{
    CALL_STACK_MESSAGE_NONE

    // hack for TortoiseCVS whose shell extension uses HBMMENU_CALLBACK for adding
    // icons to the context menu
    if (item->HBmpItem == HBMMENU_CALLBACK)
    {
        // MENU_TYPE_OWNERDRAW
        DRAWITEMSTRUCT dis;
        dis.CtlType = ODT_MENU;
        dis.CtlID = 0;
        dis.itemID = item->ID;
        dis.itemAction = ODA_DRAWENTIRE;
        dis.itemState = GetOwnerDrawItemState(item, selected);
        dis.hwndItem = (HWND)HWindowsMenu;
        dis.hDC = hDC;
        dis.rcItem.left = 0 + 2; // + 16; // TortoiseCVS positions the icons 16 pixels to the left
        dis.rcItem.top = yOffset;
        dis.rcItem.right = Width;
        dis.rcItem.bottom = yOffset + item->Height;
        dis.itemData = item->CustomData;

        // patch for XP Visual Styles: draw transparently so we don't have to
        // deal with which color to fill the background (caused issues in the New menu)
        int oldBkMode = SetBkMode(hDC, /*OPAQUE*/ TRANSPARENT);

        // let the application supply the content
        SendMessage(SharedRes->HParent, WM_DRAWITEM, 0, (LPARAM)&dis);

        return;
    }

    if (item->State & MENU_STATE_CHECKED ||
        (item->HBmpUnchecked != NULL && !(item->State & MENU_STATE_CHECKED)))
    {
        // fill the entire area with the normal color
        HBRUSH hOldBrush = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDialogBrush);
        PatBlt(SharedRes->CacheBitmap->HMemDC, 0, 0, SharedRes->TextItemHeight + 1, item->Height, PATCOPY);
        SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush);

        // center the check mark if the line height is greater than the check mark height
        int myYOffset = 0;
        if (item->Height > SharedRes->TextItemHeight)
            myYOffset += (item->Height - SharedRes->TextItemHeight) / 2;

        HBITMAP hBitmap;
        int bmpX = 0;                          // from which position in the bitmap to extract the image
        int bmpW = SharedRes->MenuBitmapWidth; // how much to transfer
        int bmpH = SharedRes->MenuBitmapWidth;
        int targetBmpW = bmpW; // final width
        int targetBmpH = bmpH;
        BOOL monoBitmap = TRUE;
        if (item->State & MENU_STATE_CHECKED)
        {
            if (item->HBmpChecked != NULL)
                hBitmap = item->HBmpChecked; // they supplied their own checkmark
        }
        else
            hBitmap = item->HBmpUnchecked; // they supplied their own checkmark

        // the bitmap may actually be smaller than requested - in that case, adjust the values
        BITMAP bitmap;
        if (!GetObject(hBitmap, sizeof(bitmap), &bitmap))
        {
            TRACE_E("GetObject() failed!");
            bitmap.bmWidth = 10;
            bitmap.bmHeight = 10;
        }
        // DiffMerge x64 has 16x16 icons, which is one pixel larger than bmpW==15 that I have by default on Win7
        // The Acrobat command "Convert to Adobe PDF" has 12x12 icons
        if (bitmap.bmWidth != bmpW)
        {
            bmpW = bitmap.bmWidth;
            targetBmpW = bmpW;
        }
        if (bitmap.bmHeight != bmpH)
        {
            bmpH = bitmap.bmHeight;
            targetBmpH = bmpH;
        }
        if (SharedRes->BitmapsZoom > 1 &&
            SharedRes->BitmapsZoom * bmpW <= SharedRes->BitmapsZoom * STANDARD_BITMAP_SIZE &&
            SharedRes->BitmapsZoom * bmpH <= SharedRes->BitmapsZoom * STANDARD_BITMAP_SIZE)
        {
            targetBmpW *= SharedRes->BitmapsZoom;
            targetBmpH *= SharedRes->BitmapsZoom;
        }
        monoBitmap = (bitmap.bmBitsPixel == 1);

        if (selected || item->State & MENU_STATE_CHECKED)
        {
            // paint the background
            RECT r;
            r.left = 0;
            r.top = myYOffset;
            r.right = 0 + SharedRes->TextItemHeight + 1;
            r.bottom = myYOffset + SharedRes->TextItemHeight;
            if (!selected && item->State & MENU_STATE_CHECKED && !(item->State & MENU_STATE_GRAYED))
            {
                // the item is checked - draw a dithered brush
                // it is not selected, so it is already painted with the correct color
                SetBrushOrgEx(SharedRes->CacheBitmap->HMemDC, 0, r.top, NULL);
                HBRUSH hOldBrush2 = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDitherBrush);
                int oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_BTNFACE));
                int oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_3DHILIGHT));
                PatBlt(SharedRes->CacheBitmap->HMemDC, r.left + 1, r.top + 1,
                       SharedRes->TextItemHeight - 1, SharedRes->TextItemHeight - 1,
                       PATCOPY);
                SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
                SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
                SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush2);
            }

            // draw the frame in the appropriate state
            if (item->State & MENU_STATE_GRAYED && selected || !(item->State & MENU_STATE_GRAYED))
            {
                DWORD mode = BDR_RAISEDINNER;
                if (!(item->State & MENU_STATE_GRAYED) && item->State & MENU_STATE_CHECKED)
                    mode = BDR_SUNKENOUTER;
                r.right--;
                DrawEdge(SharedRes->CacheBitmap->HMemDC, &r, mode, BF_RECT);
            }
        }

        // draw the actual bitmap
        int itemHeight = SharedRes->TextItemHeight - 2;
        int xO = (itemHeight - targetBmpW) / 2;
        int yO = (itemHeight - targetBmpH) / 2;

        if (item->State & MENU_STATE_GRAYED)
        {
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, hBitmap);
            HDC hSourceDC = SharedRes->HTempMemDC; // input will be a monochrome bitmap
            if (!monoBitmap)
            {
                // if the bitmap is not monochrome, create a mask from it
                SetBkColor(SharedRes->HTempMemDC, WindowsVistaAndLater ? RGB(0, 0, 0) : RGB(255, 255, 255)); // since Vista, the mask needs black, otherwise DiffMerge and BeyondCompare icons misbehaved in the context menu
                BitBlt(SharedRes->MonoBitmap->HMemDC, 0, 0, bmpW, bmpH,
                       SharedRes->HTempMemDC, 0, 0, SRCCOPY);
                // and then redirect it as the input
                hSourceDC = SharedRes->MonoBitmap->HMemDC;
            }
            // I have a monochrome bitmap and can draw it in the disabled state
            int oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, RGB(255, 255, 255));
            int oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, RGB(0, 0, 0));
            HBRUSH hOldBrush2 = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HMenuHilightBrush);
            StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO + 1, myYOffset + 1 + yO + 1, targetBmpW, targetBmpH,
                       hSourceDC, bmpX, 0, bmpW, bmpH, ROP_PSDPxax);
            SelectObject(SharedRes->CacheBitmap->HMemDC, HMenuGrayTextBrush);
            StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                       hSourceDC, bmpX, 0, bmpW, bmpH, ROP_PSDPxax);
            SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush2);
            SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
            SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);

            SelectObject(SharedRes->HTempMemDC, hOldBitmap);
        }
        else
        {
            if (item->State & MENU_STATE_CHECKED)
            {
                // if the button is pressed, shift the bitmap one pixel right and down
                xO++;
                yO++;
            }

            HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, hBitmap);
            // prepare the mask
            SetBkColor(SharedRes->HTempMemDC, WindowsVistaAndLater ? RGB(0, 0, 0) : RGB(255, 255, 255)); // since Vista, the mask needs black, otherwise DiffMerge and BeyondCompare icons misbehaved in the context menu
            BitBlt(SharedRes->MonoBitmap->HMemDC, 0, 0, bmpW, bmpH,
                   SharedRes->HTempMemDC, bmpX, 0, SRCCOPY);

            COLORREF oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, RGB(255, 255, 255));
            COLORREF oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, RGB(0, 0, 0));
            StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                       SharedRes->HTempMemDC, bmpX, 0, bmpW, bmpH, SRCINVERT);
            StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                       SharedRes->MonoBitmap->HMemDC, 0, 0, bmpW, bmpH, SRCAND);

            // quick hack for B&W bitmaps on black backgrounds - force the text color
            if (monoBitmap && SharedRes->NormalBkColor == RGB(0, 0, 0))
                SetTextColor(SharedRes->CacheBitmap->HMemDC, SharedRes->SelectedTextColor);

            StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                       SharedRes->HTempMemDC, bmpX, 0, bmpW, bmpH, SRCINVERT);
            SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
            SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
            SelectObject(SharedRes->HTempMemDC, hOldBitmap);
            SetBkColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
            SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
        }

        BitBlt(hDC, 0, yOffset, SharedRes->TextItemHeight, item->Height,
               SharedRes->CacheBitmap->HMemDC, 0, 0, SRCCOPY);
    }
}

void CMenuPopup::DrawCheckImage(HDC hDC, CMenuItem* item, int yOffset, BOOL selected)
{
    CALL_STACK_MESSAGE_NONE
    // fill the entire area with the normal color
    HBRUSH hOldBrush = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDialogBrush);
    PatBlt(SharedRes->CacheBitmap->HMemDC, 0, 0, SharedRes->TextItemHeight + 1, item->Height, PATCOPY);
    SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush);

    // center the image if the line height is greater than the image height
    int myYOffset = 0;
    if (item->Height > SharedRes->TextItemHeight)
        myYOffset += (item->Height - SharedRes->TextItemHeight) / 2;

    int targetBmpW = ImageWidth; // final width
    int targetBmpH = ImageHeight;
    if (item->HIcon != NULL)
    {
        int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
        targetBmpW = iconSize;
        targetBmpH = iconSize;
    }

    BOOL checked = (item->State & MENU_STATE_CHECKED) != 0;

    if (selected || checked)
    {
        // paint the background
        RECT r;
        r.left = 0;
        r.top = myYOffset;
        r.right = 0 + SharedRes->TextItemHeight + 1;
        r.bottom = myYOffset + SharedRes->TextItemHeight;
        if (!selected && checked && !(item->State & MENU_STATE_GRAYED))
        {
            // the item is checked - draw a dithered brush
            // it is not selected, so it is already painted with the correct color
            SetBrushOrgEx(SharedRes->CacheBitmap->HMemDC, 0, r.top, NULL);
            HBRUSH hOldBrush2 = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HDitherBrush);
            int oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_BTNFACE));
            int oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, GetSysColor(COLOR_3DHILIGHT));
            PatBlt(SharedRes->CacheBitmap->HMemDC, r.left + 1, r.top + 1,
                   SharedRes->TextItemHeight - 1, SharedRes->TextItemHeight - 1,
                   PATCOPY);
            SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
            SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
            SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush2);
        }

        // draw the frame in the appropriate state
        if (item->State & MENU_STATE_GRAYED && selected || !(item->State & MENU_STATE_GRAYED))
        {
            DWORD mode = BDR_RAISEDINNER;
            if (!(item->State & MENU_STATE_GRAYED) && checked)
                mode = BDR_SUNKENOUTER;
            r.right--;
            DrawEdge(SharedRes->CacheBitmap->HMemDC, &r, mode, BF_RECT);
        }
    }

    // draw the actual image
    HIMAGELIST hImageList;
    int imageIndex;

    if (item->ImageIndex != -1)
    {
        hImageList = HHotImageList;
        //if (HHotImageList != NULL && (selected || checked) && !(item->State & MENU_STATE_GRAYED))
        //  hImageList = HHotImageList;
        imageIndex = item->ImageIndex;
    }
    else
    {
        hImageList = HMenuMarkImageList;
        imageIndex = item->Type & MENU_TYPE_RADIOCHECK ? 1 : 0;
        int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
        targetBmpW = iconSize; // dimensions of HMenuMarkImageList
        targetBmpH = iconSize;
    }

    int itemHeight = SharedRes->TextItemHeight - 2;
    int xO = (itemHeight - targetBmpW) / 2;
    int yO = (itemHeight - targetBmpH) / 2;

    if (item->State & MENU_STATE_GRAYED)
    {
        // the whole bitmap will be white (background)
        PatBlt(SharedRes->MonoBitmap->HMemDC, 0, 0, targetBmpW, targetBmpH, WHITENESS);

        int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);

        // transfer a monochrome version of the icon into it (white will remain white)
        if (item->HIcon != NULL)
            DrawIconEx(SharedRes->MonoBitmap->HMemDC, 0, 0, item->HIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        else
            ImageList_Draw(hImageList, imageIndex, SharedRes->MonoBitmap->HMemDC, 0, 0, ILD_TRANSPARENT);
        if (item->HOverlay != NULL)
            DrawIconEx(SharedRes->MonoBitmap->HMemDC, 0, 0, item->HOverlay, iconSize, iconSize, 0, NULL, DI_NORMAL);

        // and then redirect that as the input
        HDC hSourceDC = SharedRes->MonoBitmap->HMemDC;

        // I have a monochrome bitmap and can draw it in the disabled state
        int oldBkColor = SetBkColor(SharedRes->CacheBitmap->HMemDC, RGB(255, 255, 255));
        int oldTextColor = SetTextColor(SharedRes->CacheBitmap->HMemDC, RGB(0, 0, 0));
        HBRUSH hOldBrush2 = (HBRUSH)SelectObject(SharedRes->CacheBitmap->HMemDC, HMenuHilightBrush);
        StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO + 1, myYOffset + 1 + yO + 1, targetBmpW, targetBmpH,
                   hSourceDC, 0, 0, iconSize, iconSize, ROP_PSDPxax);
        SelectObject(SharedRes->CacheBitmap->HMemDC, HMenuGrayTextBrush);
        StretchBlt(SharedRes->CacheBitmap->HMemDC, 1 + xO, myYOffset + 1 + yO, targetBmpW, targetBmpH,
                   hSourceDC, 0, 0, iconSize, iconSize, ROP_PSDPxax);
        SelectObject(SharedRes->CacheBitmap->HMemDC, hOldBrush2);
        SetTextColor(SharedRes->CacheBitmap->HMemDC, oldTextColor);
        SetBkColor(SharedRes->CacheBitmap->HMemDC, oldBkColor);
    }
    else
    {
        //    if (checked)
        //    {

        // for better appearance, we sacrifice pressed items
        xO++;
        yO++;
        //    }

        int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);

        if (item->HIcon != NULL)
            DrawIconEx(SharedRes->CacheBitmap->HMemDC, /*1 + */ xO + 1, myYOffset + /*1 + */ yO + 1,
                       item->HIcon, iconSize, iconSize, 0, NULL, DI_NORMAL);
        else
            ImageList_Draw(hImageList, imageIndex, SharedRes->CacheBitmap->HMemDC,
                           /*1 + */ xO + 1, myYOffset + /*1 + */ yO + 1,
                           checked ? ILD_TRANSPARENT : ILD_NORMAL);
        if (item->HOverlay != NULL)
            DrawIconEx(SharedRes->CacheBitmap->HMemDC, /*1 + */ xO + 1, myYOffset + /*1 + */ yO + 1,
                       item->HOverlay, iconSize, iconSize, 0, NULL, DI_NORMAL);
    }

    BitBlt(hDC, 0, yOffset, SharedRes->TextItemHeight + 1, item->Height,
           SharedRes->CacheBitmap->HMemDC, 0, 0, SRCCOPY);
}

void CMenuPopup::DrawCheckMark(HDC hDC, CMenuItem* item, int yOffset, BOOL selected)
{
    CALL_STACK_MESSAGE_NONE
    if (item->ImageIndex != -1 || item->HIcon != NULL ||
        item->HBmpChecked == NULL && (item->State & MENU_STATE_CHECKED))
    {
        DrawCheckImage(hDC, item, yOffset, selected); // insert the image before the text
    }
    else
    {
        if (WindowsVistaAndLater && item->HBmpItem != NULL && item->HBmpItem != HBMMENU_CALLBACK)
            DrawCheckBitmapVista(hDC, item, yOffset, selected);
        else
            DrawCheckBitmap(hDC, item, yOffset, selected); // insert the check mark before the text (if it exists)
    }
}

void CMenuPopup::DrawItem(HDC hDC, CMenuItem* item, int yOffset, BOOL selected)
{
    CALL_STACK_MESSAGE_NONE

    // if the item lies outside the visible area, do not draw
    int topLimit = 0;
    int bottomLimit = Height;
    if (UpArrowVisible)
        topLimit += UPDOWN_ITEM_HEIGHT;
    if (DownArrowVisible)
        bottomLimit -= UPDOWN_ITEM_HEIGHT;
    if (yOffset < topLimit || yOffset + item->Height > bottomLimit)
        return;

    // paint the background
    HBRUSH hBkBrush;
    if (selected && !(item->Type & MENU_TYPE_OWNERDRAW) && item->Type & MENU_TYPE_STRING)
        hBkBrush = HMenuSelectedBkBrush;
    else
        hBkBrush = HDialogBrush;
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, hBkBrush);
    // to prevent flickering, if there is a check mark, shift the background fill
    int xO = 0;
    if (item->Height == SharedRes->TextItemHeight &&
        !(item->Type & MENU_TYPE_OWNERDRAW) &&
        (item->Type & MENU_TYPE_STRING || item->Type & MENU_TYPE_BITMAP) &&
        (item->HBmpUnchecked != NULL && !(item->State & MENU_STATE_CHECKED) ||
         item->State & MENU_STATE_CHECKED || item->ImageIndex != -1 || item->HIcon != NULL))
        xO = SharedRes->TextItemHeight + 1;
    PatBlt(hDC, xO, yOffset, Width - xO, item->Height, PATCOPY);
    SelectObject(hDC, hOldBrush);

    if (!(item->Type & MENU_TYPE_OWNERDRAW))
    {
        if (item->Type & MENU_TYPE_STRING)
        {
            DrawCheckMark(hDC, item, yOffset, selected);

            RECT textR;
            textR.top = yOffset;
            textR.bottom = yOffset + item->Height;
            textR.right = Width;

            // save the original values
            COLORREF oldTextColor;
            COLORREF oldBkColor;
            int oldBkMode = SetBkMode(hDC, OPAQUE);

            if (selected)
            {
                oldTextColor = SetTextColor(hDC, SharedRes->SelectedTextColor);
                oldBkColor = SetBkColor(hDC, SharedRes->SelectedBkColor);
            }
            else
            {
                oldTextColor = SetTextColor(hDC, SharedRes->NormalTextColor);
                oldBkColor = SetBkColor(hDC, SharedRes->NormalBkColor);
            }

            // if the item is default, temporarily select the bold font
            HFONT hOldFont;
            if (item->State & MENU_STATE_DEFAULT)
                hOldFont = (HFONT)SelectObject(hDC, SharedRes->HBoldFont);

            if (item->ColumnL1 != NULL)
            {
                textR.left = SharedRes->TextItemHeight + 1 + item->ColumnL1X;
                DWORD dtFlags = DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER;
                if (SharedRes->HideAccel)
                    dtFlags |= DT_HIDEPREFIX;

                if (item->State & MENU_STATE_GRAYED)
                {
                    if (!selected)
                    {
                        SetTextColor(hDC, SharedRes->HilightColor);
                        RECT textR2 = textR;
                        textR2.left++;
                        textR2.top++;
                        textR2.right++;
                        textR2.bottom++;
                        DrawText(hDC, item->ColumnL1, item->ColumnL1Len, &textR2, dtFlags);
                        SetBkMode(hDC, TRANSPARENT);
                        SetTextColor(hDC, SharedRes->GrayTextColor);
                    }
                    else
                        SetTextColor(hDC, SharedRes->NormalBkColor);
                    DrawText(hDC, item->ColumnL1, item->ColumnL1Len, &textR, dtFlags);
                    SetBkMode(hDC, OPAQUE);
                }
                else
                    DrawText(hDC, item->ColumnL1, item->ColumnL1Len, &textR, dtFlags);
            }

            if (item->ColumnL2 != NULL)
            {
                textR.left = SharedRes->TextItemHeight + 1 + item->ColumnL2X;
                if (item->State & MENU_STATE_GRAYED)
                {
                    if (!selected)
                    {
                        SetTextColor(hDC, SharedRes->HilightColor);
                        RECT textR2 = textR;
                        textR2.left++;
                        textR2.top++;
                        textR2.right++;
                        textR2.bottom++;
                        DrawText(hDC, item->ColumnL2, item->ColumnL2Len,
                                 &textR2, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                        SetBkMode(hDC, TRANSPARENT);
                        SetTextColor(hDC, SharedRes->GrayTextColor);
                    }
                    else
                        SetTextColor(hDC, SharedRes->NormalBkColor);
                    DrawText(hDC, item->ColumnL2, item->ColumnL2Len,
                             &textR, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                    SetBkMode(hDC, OPAQUE);
                }
                else
                    DrawText(hDC, item->ColumnL2, item->ColumnL2Len,
                             &textR, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            }

            if (item->ColumnR != NULL)
            {
                textR.left = SharedRes->TextItemHeight + 1 + item->ColumnRX;
                if (item->State & MENU_STATE_GRAYED)
                {
                    if (!selected)
                    {
                        SetTextColor(hDC, SharedRes->HilightColor);
                        RECT textR2 = textR;
                        textR2.left++;
                        textR2.top++;
                        textR2.right++;
                        textR2.bottom++;
                        DrawText(hDC, item->ColumnR, item->ColumnRLen,
                                 &textR2, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                        SetBkMode(hDC, TRANSPARENT);
                        SetTextColor(hDC, SharedRes->GrayTextColor);
                    }
                    else
                        SetTextColor(hDC, SharedRes->NormalBkColor);
                    DrawText(hDC, item->ColumnR, item->ColumnRLen,
                             &textR, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
                }
                else
                    DrawText(hDC, item->ColumnR, item->ColumnRLen,
                             &textR, DT_NOCLIP | DT_LEFT | DT_SINGLELINE | DT_VCENTER);
            }
            // restore the original values
            if (item->State & MENU_STATE_DEFAULT)
                SelectObject(hDC, hOldFont);
            SetTextColor(hDC, oldTextColor);
            SetBkColor(hDC, oldBkColor);
            SetBkMode(hDC, oldBkMode);
        }
        else
        {
            if (item->Type & MENU_TYPE_SEPARATOR)
            {
                int y = yOffset + item->Height / 2 - 1;
                // horizontal gray line
                HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hDC, HMenuGrayTextBrush);
                PatBlt(hDC, 2, y, Width - 2 * 2, 1, PATCOPY);
                // horizontal highlight line
                SelectObject(hDC, HMenuHilightBrush);
                PatBlt(hDC, 2, y + 1, Width - 2 * 2, 1, PATCOPY);
                SelectObject(hDC, hOldBrush2);
            }
            else
            {
                if (item->Type & MENU_TYPE_BITMAP)
                {
                    DrawCheckMark(hDC, item, yOffset, selected);

                    // get dimensions of the bitmap
                    BITMAP bitmap;
                    GetObject(item->HBmpItem, sizeof(bitmap), &bitmap);

                    // select the bitmap into HTempMemDC
                    HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, item->HBmpItem);

                    if (item->State & MENU_STATE_GRAYED)
                    {
                        // create the mask
                        SetBkColor(SharedRes->HTempMemDC, RGB(255, 255, 255));
                        BitBlt(SharedRes->MonoBitmap->HMemDC, 0, 0,
                               bitmap.bmWidth, bitmap.bmHeight,
                               SharedRes->HTempMemDC, 0, 0, SRCCOPY);

                        // create the disabled effect
                        int oldBkColor = SetBkColor(hDC, RGB(255, 255, 255));
                        int oldTextColor = SetTextColor(hDC, RGB(0, 0, 0));
                        HBRUSH hOldBrush2 = (HBRUSH)SelectObject(hDC, HMenuHilightBrush);
                        BitBlt(hDC, SharedRes->TextItemHeight + 1 + 2 + 1, yOffset + 1 + 1,
                               bitmap.bmWidth, bitmap.bmHeight,
                               SharedRes->MonoBitmap->HMemDC, 0, 0, ROP_PSDPxax);
                        SelectObject(hDC, HMenuGrayTextBrush);
                        BitBlt(hDC, SharedRes->TextItemHeight + 1 + 2, yOffset + 1,
                               bitmap.bmWidth, bitmap.bmHeight,
                               SharedRes->MonoBitmap->HMemDC, 0, 0, ROP_PSDPxax);
                        SelectObject(hDC, hOldBrush2);
                        SetTextColor(hDC, oldTextColor);
                        SetBkColor(hDC, oldBkColor);
                    }
                    else
                    {
                        int yO = (item->Height - bitmap.bmHeight) / 2;
                        // draw the bitmap, either normally or inverted
                        int mode = selected ? NOTSRCCOPY : SRCCOPY;
                        BitBlt(hDC, SharedRes->TextItemHeight + 1 + 2, yOffset + yO,
                               bitmap.bmWidth, bitmap.bmHeight,
                               SharedRes->HTempMemDC, 0, 0, mode);
                    }
                    SelectObject(SharedRes->HTempMemDC, hOldBitmap);
                }
            }
        }
    }
    else
    {
        // MENU_TYPE_OWNERDRAW
        DRAWITEMSTRUCT dis;
        dis.CtlType = ODT_MENU;
        dis.CtlID = 0;
        dis.itemID = item->ID;
        dis.itemAction = ODA_DRAWENTIRE;
        dis.itemState = GetOwnerDrawItemState(item, selected);
        dis.hwndItem = (HWND)HWindowsMenu;
        dis.hDC = hDC;
        dis.rcItem.left = 0;
        dis.rcItem.top = yOffset;
        dis.rcItem.right = Width;
        dis.rcItem.bottom = yOffset + item->Height;
        dis.itemData = item->CustomData;

        // save DC values
        COLORREF oldTextColor;
        COLORREF oldBkColor;
        HFONT hOldFont;

        // patch for XP Visual Styles: draw transparently so we don't have to
        // deal with which color to fill the background (caused issues in the New menu)
        int oldBkMode = SetBkMode(hDC, /*OPAQUE*/ TRANSPARENT);

        // in any case, I set and restore the font - we can't know
        // what mischief others might do with the DC
        if (item->State & MENU_STATE_DEFAULT)
            hOldFont = (HFONT)SelectObject(hDC, SharedRes->HBoldFont);
        else
            hOldFont = (HFONT)SelectObject(hDC, SharedRes->HNormalFont);
        if (selected)
        {
            oldTextColor = SetTextColor(hDC, SharedRes->SelectedTextColor);
            oldBkColor = SetBkColor(hDC, SharedRes->SelectedBkColor);
        }
        else
        {
            oldTextColor = SetTextColor(hDC, SharedRes->NormalTextColor);
            oldBkColor = SetBkColor(hDC, SharedRes->NormalBkColor);
        }

        // let the application supply the content
        SendMessage(SharedRes->HParent, WM_DRAWITEM, 0, (LPARAM)&dis);

        // restore original values
        SetBkMode(hDC, oldBkMode);
        SetTextColor(hDC, oldTextColor);
        SetBkColor(hDC, oldBkColor);
        SelectObject(hDC, hOldFont);
    }
    if (item->SubMenu != NULL)
    {
        // if the item has a submenu, draw an arrow
        // no need to create a mask because the arrows are monochrome bitmaps

        int w = SharedRes->MenuBitmapWidth;
        int h = SharedRes->MenuBitmapWidth;
        int bmpX = menuBitmapArrowR * w;

        HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, SharedRes->HMenuBitmaps);
        int oldBkColor = SetBkColor(hDC, RGB(255, 255, 255));
        int oldTextColor = SetTextColor(hDC, RGB(0, 0, 0));

        HBRUSH hOldBrush2;
        if (item->State & MENU_STATE_GRAYED)
        {
            if (!selected)
            {
                hOldBrush2 = (HBRUSH)SelectObject(hDC, HMenuHilightBrush);
                BitBlt(hDC, Width - w + 1, yOffset + (item->Height - h) / 2 + 1, w, h,
                       SharedRes->HTempMemDC, bmpX, 0, ROP_PSDPxax);
                SelectObject(hDC, HMenuGrayTextBrush);
            }
            else
                hOldBrush2 = (HBRUSH)SelectObject(hDC, HMenuGrayTextBrush);
        }
        else
        {
            HBRUSH hBrush = selected ? HMenuSelectedTextBrush : HButtonTextBrush;
            hOldBrush2 = (HBRUSH)SelectObject(hDC, hBrush);
        }
        BitBlt(hDC, Width - w, yOffset + (item->Height - h) / 2, w, h,
               SharedRes->HTempMemDC, bmpX, 0, ROP_PSDPxax);

        SelectObject(hDC, hOldBrush2);
        SetTextColor(hDC, oldTextColor);
        SetBkColor(hDC, oldBkColor);
        SelectObject(SharedRes->HTempMemDC, hOldBitmap);
    }
}

void CMenuPopup::DrawUpDownItem(HDC hDC, BOOL up)
{
    RECT arrowR;
    RECT r;
    r.left = 0;
    r.right = Width;

    // place the rectangle
    if (up)
    {
        r.top = 0;
        r.bottom = UPDOWN_ITEM_HEIGHT;
        arrowR = r;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            int tmp = TopItemY + iterator->YOffset;
            if (tmp >= UPDOWN_ITEM_HEIGHT)
            {
                r.bottom = tmp;
                break;
            }
        }
    }
    else
    {
        r.top = Height - UPDOWN_ITEM_HEIGHT;
        r.bottom = Height;
        arrowR = r;
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CMenuItem* iterator = Items[i];
            int tmp = TopItemY + iterator->YOffset + iterator->Height;
            if (tmp <= Height - UPDOWN_ITEM_HEIGHT)
                r.top = tmp;
            else
                break;
        }
    }

    int w = UPDOWN_ARROW_WIDTH;
    int h = UPDOWN_ARROW_HEIGHT;
    int bmpX = up ? 0 : UPDOWN_ARROW_WIDTH;

    HBITMAP hOldBitmap = (HBITMAP)SelectObject(SharedRes->HTempMemDC, HUpDownBitmap);
    int oldBkColor = SetBkColor(hDC, RGB(255, 255, 255));
    int oldTextColor = SetTextColor(hDC, RGB(0, 0, 0));
    HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, HButtonTextBrush);

    int yOffset = up ? 0 : 1; // move the down arrow one pixel down (for balance)

    // fill the rectangle
    FillRect(hDC, &r, HDialogBrush);
    // draw the arrow
    BitBlt(hDC, r.left + (r.right - r.left - w) / 2,
           arrowR.top + (arrowR.bottom - arrowR.top - h) / 2 + yOffset,
           w, h, SharedRes->HTempMemDC, bmpX, 0, ROP_PSDPxax);

    SelectObject(hDC, hOldBrush);
    SetTextColor(hDC, oldTextColor);
    SetBkColor(hDC, oldBkColor);
    SelectObject(SharedRes->HTempMemDC, hOldBitmap);
}
