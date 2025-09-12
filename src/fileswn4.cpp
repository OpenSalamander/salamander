// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "shiconov.h"

//****************************************************************************
//
// Panel paint
//
//  - Brief mode
//  - Detailed mode
//  - Icons mode
//  - Thumbnails mode
//

//****************************************************************************
//
// CFilesWindow::SetFontAndColors
//

void CFilesWindow::SetFontAndColors(HDC hDC, CHighlightMasksItem* highlightMasksItem, CFileData* f,
                                    BOOL isItemFocusedOrEditMode, int itemIndex)
{
    // local scope enum
    enum ColorModeEnum
    {
        cmeNormal,
        cmeFocused,
        cmeSelected,
        cmeFocSel
    };

    int colorMode;

    if (!f->Selected == 1 && !isItemFocusedOrEditMode)
        colorMode = cmeNormal;
    else if (f->Selected == 0 && isItemFocusedOrEditMode)
        colorMode = cmeFocused;
    else if (f->Selected == 1 && !isItemFocusedOrEditMode)
        colorMode = cmeSelected;
    else
        colorMode = cmeFocSel;

    if (TrackingSingleClick && SingleClickIndex == itemIndex)
    {
        SelectObject(hDC, FontUL);
        SetTextColor(hDC, GetCOLORREF(CurrentColors[HOT_PANEL]));
    }
    else
    {
        SelectObject(hDC, Font);
        // text color
        SALCOLOR* fgColor;
        if (highlightMasksItem == NULL)
        {
            if (colorMode == cmeNormal)
                fgColor = &CurrentColors[ITEM_FG_NORMAL];
            else if (colorMode == cmeFocused)
                fgColor = &CurrentColors[ITEM_FG_FOCUSED];
            else if (colorMode == cmeSelected)
                fgColor = &CurrentColors[ITEM_FG_SELECTED];
            else
                fgColor = &CurrentColors[ITEM_FG_FOCSEL];
        }
        else
        {
            if (colorMode == cmeNormal)
                fgColor = &highlightMasksItem->NormalFg;
            else if (colorMode == cmeFocused)
                fgColor = &highlightMasksItem->FocusedFg;
            else if (colorMode == cmeSelected)
                fgColor = &highlightMasksItem->SelectedFg;
            else
                fgColor = &highlightMasksItem->FocSelFg;
        }
        SetTextColor(hDC, GetCOLORREF(*fgColor));
    }

    // set background color
    SALCOLOR* bkColor;
    if (highlightMasksItem == NULL)
    {
        if (colorMode == cmeNormal)
            bkColor = &CurrentColors[ITEM_BK_NORMAL];
        else if (colorMode == cmeFocused)
            bkColor = &CurrentColors[ITEM_BK_FOCUSED];
        else if (colorMode == cmeSelected)
            bkColor = &CurrentColors[ITEM_BK_SELECTED];
        else
            bkColor = &CurrentColors[ITEM_BK_FOCSEL];
    }
    else
    {
        if (colorMode == cmeNormal)
            bkColor = &highlightMasksItem->NormalBk;
        else if (colorMode == cmeFocused)
            bkColor = &highlightMasksItem->FocusedBk;
        else if (colorMode == cmeSelected)
            bkColor = &highlightMasksItem->SelectedBk;
        else
            bkColor = &highlightMasksItem->FocSelBk;
    }

    if (itemIndex == DropTargetIndex)
    {
        if (colorMode == cmeSelected)
            bkColor = &CurrentColors[ITEM_BK_SELECTED];
        else if (colorMode == cmeFocSel)
            bkColor = &CurrentColors[ITEM_BK_FOCSEL];
        else
            bkColor = &CurrentColors[ITEM_BK_FOCUSED];
    }
    SetBkColor(hDC, GetCOLORREF(*bkColor));
}

//****************************************************************************
//
// CFilesWindow::DrawIcon
//

void CFilesWindow::DrawIcon(HDC hDC, CFileData* f, BOOL isDir, BOOL isItemUpDir,
                            BOOL isItemFocusedOrEditMode, int x, int y, CIconSizeEnum iconSize,
                            const RECT* overlayRect, DWORD drawFlags)
{
    BOOL drawSimpleSymbol = FALSE;
    int symbolIndex;                   // index in the Symbols bitmap...
    DWORD iconState = 0;               // flags for drawing the icon
    char lowerExtension[MAX_PATH + 4]; // extension in lowercase, DWORD aligned

    if (!(drawFlags & DRAWFLAG_NO_STATE))
    {
        if ((f->Hidden == 1 || f->CutToClip == 1) && !isItemUpDir)
        {
            if (f->CutToClip == 0 && f->Selected == 1)
                iconState |= IMAGE_STATE_SELECTED; //hidden items will have the selected state (not cut)
            else
                iconState |= IMAGE_STATE_HIDDEN;
        }
        else
        {
            if (isItemFocusedOrEditMode)
                iconState |= IMAGE_STATE_FOCUSED;
            if (!isItemUpDir)
                if (f->Selected == 1)
                    iconState |= IMAGE_STATE_SELECTED;
        }
    }

    if (drawFlags & DRAWFLAG_MASK)
        iconState |= IMAGE_STATE_MASK;

    if (f->IsLink)
        iconState |= IMAGE_STATE_SHORTCUT;

    if (f->IsOffline)
        iconState |= IMAGE_STATE_OFFLINE;

    if (!isDir)
    {
        // convert extension characters to lowercase
        char *dstExt = lowerExtension, *srcExt = f->Ext;
        while (*srcExt != 0)
            *dstExt++ = LowerCase[*srcExt++];
        *((DWORD*)dstExt) = 0;

        if (*(DWORD*)lowerExtension == *(DWORD*)"exe" ||
            *(DWORD*)lowerExtension == *(DWORD*)"bat" ||
            *(DWORD*)lowerExtension == *(DWORD*)"pif" ||
            *(DWORD*)lowerExtension == *(DWORD*)"com" ||
            *(DWORD*)lowerExtension == *(DWORD*)"scf" ||
            *(DWORD*)lowerExtension == *(DWORD*)"scr" ||
            *(DWORD*)lowerExtension == *(DWORD*)"cmd")
            symbolIndex = symbolsExecutable;
        else
            symbolIndex = (f->Association) ? (f->Archive ? symbolsArchive : symbolsAssociated) : symbolsNonAssociated;
    }
    else
    {
        if (isItemUpDir)
        {
            symbolIndex = symbolsUpDir;
        }
        else
        {
            symbolIndex = symbolsDirectory;
            if (f->Shared)
                iconState |= IMAGE_STATE_SHARED;
        }
    }

    BOOL iconOverlayFromPlugin = Is(ptZIPArchive) || Is(ptPluginFS);
    int pluginIconOverlaysCount = 0;
    HICON* pluginIconOverlays = NULL;
    if (iconOverlayFromPlugin && f->IconOverlayIndex != ICONOVERLAYINDEX_NOTUSED)
    {
        CPluginData* plugin = GetPluginDataForPluginIface();
        if (plugin != NULL && plugin->IconOverlaysCount > 0)
        {
            pluginIconOverlaysCount = plugin->IconOverlaysCount;
            pluginIconOverlays = plugin->IconOverlays;
        }
    }

    // TemporarilySimpleIcons==TRUE is set while switching panel modes
    // so we don't use the not-yet-prepared IconCache for drawing icons
    if (!TemporarilySimpleIcons && UseSystemIcons)
    {
        if (symbolIndex != symbolsUpDir && symbolIndex != symbolsArchive)
        {
            CIconList* iconList = NULL;
            int iconListIndex = -1; // close it if not set
            char fileName[MAX_PATH + 4];

            if (GetPluginIconsType() != pitFromPlugin || !Is(ptPluginFS))
            {
                if (isDir) // it's a directory
                {
                    int icon;
                    memmove(fileName, f->Name, f->NameLen);
                    *(DWORD*)(fileName + f->NameLen) = 0;

                    if (!IconCache->GetIndex(fileName, icon, NULL, NULL) ||                             // the icon-thread isn't loading it
                        IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 ||     // neither new nor old icon is loaded
                        !IconCache->GetIcon(IconCache->At(icon).GetIndex(), &iconList, &iconListIndex)) // failed to obtain its icon
                    {                                                                                   // we will display a simple symbol
                        if (!Associations.GetIcon(ASSOC_ICON_SOME_DIR, &iconList, &iconListIndex, iconSize))
                        {
                            iconList = NULL;
                            drawSimpleSymbol = TRUE;
                        }
                    }
                }
                else // it's a file
                {
                    int index;
                    BOOL exceptions = *(DWORD*)lowerExtension == *(DWORD*)"scr" || // icons in the file,
                                      *(DWORD*)lowerExtension == *(DWORD*)"pif" || // even though it isn't visible
                                      *(DWORD*)lowerExtension == *(DWORD*)"lnk";   // in the Registry

                    if (exceptions || Associations.GetIndex(lowerExtension, index)) // the extension has an icon (association)
                    {
                        if (!exceptions)
                            TransferAssocIndex = index;                               // remember the valid index in Associations
                        if (exceptions || Associations[index].GetIndex(iconSize) < 0) // dynamic icon (from the file) or a loaded static icon
                        {                                                             // icon in the file
                            int icon;
                            memmove(fileName, f->Name, f->NameLen);
                            *(DWORD*)(fileName + f->NameLen) = 0;
                            if (!IconCache->GetIndex(fileName, icon, NULL, NULL) ||                         // the icon-thread isn't loading it
                                IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 || // neither new nor old icon is loaded
                                !IconCache->GetIcon(IconCache->At(icon).GetIndex(),
                                                    &iconList, &iconListIndex)) // failed to obtain loaded icon
                            {                                                   // we will display a simple symbol
                                if (*(DWORD*)lowerExtension == *(DWORD*)"pif" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"exe" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"com" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"bat" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"scf" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"scr" ||
                                    *(DWORD*)lowerExtension == *(DWORD*)"cmd")
                                    icon = ASSOC_ICON_SOME_EXE;
                                else
                                    icon = (exceptions || Associations[index].GetFlag() != 0) ? ASSOC_ICON_SOME_FILE : ASSOC_ICON_NO_ASSOC;
                                if (!Associations.GetIcon(icon, &iconList, &iconListIndex, iconSize))
                                {
                                    iconList = NULL;
                                    drawSimpleSymbol = TRUE;
                                }
                            }
                            index = -2; // a dynamic icon will be displayed
                        }
                        // else -> index is the icon index shared by files with this extension
                    }
                    else
                    {
                        index = -1;              // the file has no special icon
                        TransferAssocIndex = -1; // remember that this extension isn't in Associations
                    }
                    if (index != -2)     // icon in the file, first pass -> don't read
                    {                    // try to draw the icon,
                        if (index != -1) // index==-1 -> simple-symbol
                        {
                            int i = Associations.At(index).GetIndex(iconSize);
                            if (i >= 0)
                                index = i;
                            else
                            {
                                TRACE_E("Unexpected situation.");
                                drawSimpleSymbol = TRUE;
                            }
                        }
                        else
                            index = ASSOC_ICON_NO_ASSOC;
                        if (!drawSimpleSymbol && !Associations.GetIcon(index, &iconList, &iconListIndex, iconSize))
                        {
                            iconList = NULL;
                            drawSimpleSymbol = TRUE;
                        }
                    }
                }
            }
            else // FS, pitFromPlugin
            {
                int icon;
                //        memmove(fileName, f->Name, f->NameLen);
                //        *(DWORD *)(fileName + f->NameLen) = 0;

                if (!IconCache->GetIndex(NULL /*fileName*/, icon, &PluginData, f) ||                // the icon-thread isn't loading it
                    IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 ||     // neither new nor old icon is loaded
                    !IconCache->GetIcon(IconCache->At(icon).GetIndex(), &iconList, &iconListIndex)) // failed to obtain its icon
                {                                                                                   // we will display a simple symbol from the plug-in
                    // the simple icon index will be obtained via the GetPluginIconIndex callback
                    TransferFileData = f;
                    TransferIsDir = isDir ? (isItemUpDir ? 2 : 1) : 0;

                    iconList = SimplePluginIcons;
                    iconListIndex = GetPluginIconIndex();
                }
            }

            if (iconList != NULL && !drawSimpleSymbol)
            {
                BOOL leaveSection;
                if (!IconCacheValid)
                {
                    HANDLES(EnterCriticalSection(&ICSectionUsingIcon));
                    leaveSection = TRUE;
                }
                else
                    leaveSection = FALSE;

                StateImageList_Draw(iconList, iconListIndex, hDC, x, y, iconState, iconSize,
                                    f->IconOverlayIndex, overlayRect, (drawFlags & DRAWFLAG_OVERLAY_ONLY) != 0,
                                    iconOverlayFromPlugin, pluginIconOverlaysCount, pluginIconOverlays);

                if (leaveSection)
                {
                    HANDLES(LeaveCriticalSection(&ICSectionUsingIcon));
                }
            }
        }
        else
            drawSimpleSymbol = TRUE; // the ".." directory
    }
    else
        drawSimpleSymbol = TRUE;

    if (drawSimpleSymbol)
    { // simple symbols
        StateImageList_Draw(SimpleIconLists[iconSize], symbolIndex, hDC, x, y, iconState, iconSize,
                            f->IconOverlayIndex, overlayRect, (drawFlags & DRAWFLAG_OVERLAY_ONLY) != 0,
                            iconOverlayFromPlugin, pluginIconOverlaysCount, pluginIconOverlays);
    }

    // I don't understand why, but the current bitmap drawing doesn't flicker.
    // probably in the case of blending the icons are drawn via memory dc
    //    BitBlt(hDC, iconRect.left, iconRect.top,
    //           iconRect.right - iconRect.left,
    //           iconRect.bottom - iconRect.top,
    //           ItemMemDC, 0, 0, SRCCOPY);
}

//****************************************************************************
//
// FillIntersectionRegion
//
// using FillRect fills the areas between
// oRect (outer) and iRect (inner) rectangles
//

void FillIntersectionRegion(HDC hDC, const RECT* oRect, const RECT* iRect)
{
    RECT r;

    // area to the left of innerRect
    r = *oRect;
    r.right = iRect->left;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // area to the right of iRect
    r = *oRect;
    r.left = iRect->right;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // area above iRect
    r = *iRect;
    r.top = oRect->top;
    r.bottom = iRect->top;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // area below iRect
    r = *iRect;
    r.top = iRect->bottom;
    r.bottom = oRect->bottom;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);
}

//****************************************************************************
//
// DrawFocusRect
//
// Draws a solid or dashed focus around the item
//

void DrawFocusRect(HDC hDC, const RECT* r, BOOL selected, BOOL editMode)
{
    HPEN hPen;
    if (editMode)
    {
        COLORREF bkColor;
        if (selected)
        {
            hPen = HInactiveSelectedPen;
            bkColor = GetCOLORREF(CurrentColors[FOCUS_BK_INACTIVE_SELECTED]);
        }
        else
        {
            hPen = HInactiveNormalPen;
            bkColor = GetCOLORREF(CurrentColors[FOCUS_BK_INACTIVE_NORMAL]);
        }
        SetBkColor(hDC, bkColor);
        SetBkMode(hDC, OPAQUE);
    }
    else
    {
        if (selected)
            hPen = HActiveSelectedPen;
        else
            hPen = HActiveNormalPen;
    }

    SelectObject(hDC, hPen);
    SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
    Rectangle(hDC, r->left, r->top, r->right, r->bottom);
}

//****************************************************************************
//
// CFilesWindow::DrawBriefDetailedItem
//
// Draws an item in Brief and Detailed modes
//

//
// changes in item painting (layout) must be reflected in these locations:
//
// CFilesWindow::DrawItem(WPARAM wParam, LPARAM lParam)
// CFilesBox::GetIndex(int x, int y)
// CFilesMap::CreateMap()
//

char DrawItemBuff[1024]; // destination buffer for strings
int DrawItemAlpDx[1024]; // for width calculations of columns with FixedWidth bit + elastic columns with smart mode

void CFilesWindow::DrawBriefDetailedItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags)
{
    CALL_STACK_MESSAGE_NONE
    if (itemIndex < 0 || itemIndex >= Dirs->Count + Files->Count)
    {
        TRACE_E("itemIndex = " << itemIndex);
        return;
    }

    if (!CanDrawItems)
        return;

    BOOL isDir = itemIndex < Dirs->Count;
    CFileData* f = isDir ? &Dirs->At(itemIndex) : &Files->At(itemIndex - Dirs->Count);

    if ((drawFlags & DRAWFLAG_DIRTY_ONLY) && f->Dirty == 0)
        return;

    BOOL isItemUpDir = FALSE; // is the drawn item the ".." directory?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" can only be first
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawingSmall itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // drawing the cursor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // switched in the command-line
            isItemFocusedOrEditMode = TRUE;
    }
    if (drawFlags & DRAWFLAG_NO_FRAME)
        isItemFocusedOrEditMode = FALSE;

    BOOL fullRowHighlight = FALSE;
    if (GetViewMode() == vmDetailed && !Configuration.FullRowSelect && Configuration.FullRowHighlight)
        fullRowHighlight = TRUE;

    int xOffset = ListBox->XOffset;

    RECT rect = *itemRect; // item position shifted by xOffset
    rect.left -= xOffset;
    rect.right -= xOffset;

    HDC hDC;
    int cacheValidWidth; // meaningful only when using the cache and specifies the number of pixels (width),
                         // which are rendered in the bitmap and must be transferred to the screen
    if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
    {
        //    TRACE_I("drawing index="<<itemIndex);
        // if it is only the cursor position change, skip visibility tests
        drawFlags |= DRAWFLAG_SKIP_VISTEST;

        // we are drawing only a small area and have plenty of time - so using the cache is fine
        hDC = ItemBitmap.HMemDC;

        // the bitmap is empty for now
        cacheValidWidth = 0;

        // we must adjust the rect variable
        rect.top -= itemRect->top;
        rect.bottom -= itemRect->top;
        rect.left -= itemRect->left;
        rect.right -= itemRect->left;
    }
    else
        hDC = hTgtDC; // this is a large paint; skip the cache and draw directly to the screen

    // if not prohibited, clear the flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // we haven't looked up the extension for the drawn item in Associations yet

    //*****************************************
    //
    // drawing the icon
    //

    RECT iconRect = rect;
    iconRect.right = iconRect.left + 1 + IconSizes[ICONSIZE_16] + 1;

    if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &iconRect))
    {
        RECT innerRect;
        innerRect.left = iconRect.left + 1;                                                           // iconX
        innerRect.top = iconRect.top + (iconRect.bottom - iconRect.top - IconSizes[ICONSIZE_16]) / 2; // iconY
        innerRect.right = innerRect.left + IconSizes[ICONSIZE_16];
        innerRect.bottom = innerRect.top + IconSizes[ICONSIZE_16];

        // clear the area around the icon
        if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
            FillIntersectionRegion(hDC, &iconRect, &innerRect);

        /*
    SHFILEINFO shi;
    HIMAGELIST hl = (HIMAGELIST)SHGetFileInfo("c:\\z\\cross_i.cur", 0, &shi, sizeof(shi),
                  SHGFI_SYSICONINDEX | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE);
//    DrawIconEx(hDC, innerRect.left, innerRect.top, shi.hIcon, 16, 16, 0, NULL,
//               DI_NORMAL);
    ImageList_Draw(hl, shi.iIcon, hDC, innerRect.left, innerRect.top, ILD_NORMAL);
*/

        DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                 innerRect.left, innerRect.top, ICONSIZE_16, NULL, drawFlags);

        if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
            cacheValidWidth += 1 + IconSizes[ICONSIZE_16] + 1;
    }

    BOOL showCaret = FALSE;
    if (!(drawFlags & DRAWFLAG_ICON_ONLY))
    {
        CHighlightMasksItem* highlightMasksItem = MainWindow->HighlightMasks->AgreeMasks(f->Name, isDir ? NULL : f->Ext, f->Attr);

        int nameLen = 0;
        if ((!isDir || Configuration.SortDirsByExt) && IsExtensionInSeparateColumn() &&
            f->Ext[0] != 0 && f->Ext > f->Name + 1) // exception for names like ".htaccess" which are shown in the Name column even though they are extensions
        {
            nameLen = (int)(f->Ext - f->Name - 1);
        }
        else
            nameLen = f->NameLen;

        // set the the applied font, background color and text color
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        RECT r = rect;
        int x = r.left;
        int y = (r.top + r.bottom - FontCharHeight) / 2;

        BOOL fileNameFormated = FALSE;
        SIZE textSize;
        SetTextAlign(hDC, TA_TOP | TA_LEFT | TA_NOUPDATECP);

        // valid if the focus is shortened
        BOOL focusFrameRightValid = FALSE;
        int focusFrameRight;

        if (QuickSearchMode && itemIndex == FocusedIndex)
        {
            HideCaret(ListBox->HWindow);
            showCaret = TRUE;
        }

        int nameWidth = Columns[0].Width;

        r.left = x + 1 + IconSizes[ICONSIZE_16] + 1;
        r.right = x + nameWidth;

        // if the item is framed, draw only its interior
        BOOL forFrameAdjusted = FALSE;
        RECT adjR = r;
        if ((itemIndex == DropTargetIndex || isItemFocusedOrEditMode))
        {
            forFrameAdjusted = TRUE;
            adjR.top++;
            adjR.bottom--;
        }

        //*****************************************
        //
        // text drawing
        //
        // !!! Warning !!!
        // For smooth rendering of the whole line the GdiBatchLimit is used
        // Ensure text drawing does not interrupt buffering
        // (GDI functions do not return BOOL they call GdiFlush())
        //
        //    GdiFlush();

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); // we want black text on black background because of aliased fonts
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text and background
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &r))
        {
            if (GetViewMode() == vmBrief && Configuration.FullRowSelect)
            {
                r.right -= 10; // adjustment - we must create space for dragging the frame
                focusFrameRightValid = TRUE;
                focusFrameRight = r.right;
            }

            if (isItemUpDir)
            {
                nameLen = 0;
                if (GetViewMode() == vmBrief && !Configuration.FullRowSelect)
                {
                    adjR.right -= 10; // 10 - so we don't stretch across the full width
                    r.right -= 10;
                    focusFrameRightValid = TRUE;
                    focusFrameRight = adjR.right;
                }
                else if (!Configuration.FullRowSelect)
                {
                    focusFrameRightValid = TRUE;
                    focusFrameRight = r.right;
                }
            }
            else
            {
                AlterFileName(TransferBuffer, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
                fileNameFormated = TRUE;
            }

            CColumn* column = &Columns[0];
            SIZE fnSZ;
            int fitChars;
            int textWidth;
            if (!isItemUpDir && !Configuration.FullRowSelect)
            {
                // measure the actual text length
                if (GetViewMode() == vmDetailed && (column->FixedWidth == 1 || NarrowedNameColumn))
                {
                    textWidth = nameWidth - 1 - IconSizes[ICONSIZE_16] - 1 - 2 - SPACE_WIDTH;
                    GetTextExtentExPoint(hDC, TransferBuffer, nameLen, textWidth,
                                         &fitChars, DrawItemAlpDx, &fnSZ);
                    int newWidth = 1 + IconSizes[ICONSIZE_16] + 1 + 2 + fnSZ.cx + 3;
                    if (newWidth > nameWidth)
                        newWidth = nameWidth;
                    adjR.right = r.right = rect.left + newWidth;
                }
                else
                {
                    GetTextExtentPoint32(hDC, TransferBuffer, nameLen, &fnSZ);
                    adjR.right = r.right = rect.left + 1 + IconSizes[ICONSIZE_16] + 1 + 2 + fnSZ.cx + 3;
                }

                // focus will also be shorter
                focusFrameRightValid = TRUE;
                focusFrameRight = r.right;
            }
            if (!isItemUpDir && GetViewMode() == vmDetailed && (column->FixedWidth == 1 || NarrowedNameColumn))
            {
                if (Configuration.FullRowSelect)
                {
                    // no width measured yet - let's measure it now
                    // the string may be longer than the available space and must end with "..."
                    textWidth = nameWidth - 1 - IconSizes[ICONSIZE_16] - 1 - 2 - SPACE_WIDTH;
                    GetTextExtentExPoint(hDC, TransferBuffer, nameLen, textWidth,
                                         &fitChars, DrawItemAlpDx, &fnSZ);
                }
                if (fitChars < nameLen)
                {
                    // search from the end for the character after which we can copy "..." and it fits in the column
                    while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > textWidth)
                        fitChars--;
                    // copy a part of the original string to another buffer
                    int totalCount;
                    if (fitChars > 0)
                    {
                        memmove(DrawItemBuff, TransferBuffer, fitChars);
                        // and append "..."
                        memmove(DrawItemBuff + fitChars, "...", 3);
                        totalCount = fitChars + 3;
                    }
                    else
                    {
                        DrawItemBuff[0] = TransferBuffer[0];
                        DrawItemBuff[1] = '.';
                        totalCount = 2;
                    }

                    // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
                    ExtTextOut(hDC, r.left + 2, y, ETO_OPAQUE, &adjR, DrawItemBuff, (drawFlags & DRAWFLAG_MASK) ? 0 : totalCount, NULL);
                    goto SKIP1;
                }
            }
            // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
            ExtTextOut(hDC, r.left + 2, y, ETO_OPAQUE, &adjR, TransferBuffer, (drawFlags & DRAWFLAG_MASK) ? 0 : nameLen, NULL);
        SKIP1:
            if (!Configuration.FullRowSelect || GetViewMode() == vmBrief)
            {
                if (forFrameAdjusted)
                {
                    adjR.top--;
                    adjR.bottom++;
                    forFrameAdjusted = FALSE;
                }
                if (r.right < x + nameWidth)
                {
                    // clear the remainder of the name
                    adjR.left = r.left = r.right;
                    adjR.right = r.right = x + nameWidth;
                    SALCOLOR* bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_NORMAL] : &highlightMasksItem->NormalBk;
                    if (fullRowHighlight && isItemFocusedOrEditMode)
                        bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_HIGHLIGHT] : &highlightMasksItem->HighlightBk;
                    SetBkColor(hDC, GetCOLORREF(*bkColor));
                    if (drawFlags & DRAWFLAG_MASK) // mask is b&w; we must not paint a colored background into it
                        SetBkColor(hDC, RGB(255, 255, 255));
                    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
                }
            }
        }
        x += nameWidth;
        if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
            cacheValidWidth = nameWidth;
        // if FullRowSelect is off and we draw only dirty items or only focus/select changed,
        // skip drawing the remaining columns - nothing changed there
        if (GetViewMode() == vmDetailed && (fullRowHighlight || Configuration.FullRowSelect ||
                                            !(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_SELFOC_CHANGE)))
        {
            if (!Configuration.FullRowSelect)
            {
                if (TrackingSingleClick && SingleClickIndex == itemIndex)
                    SelectObject(hDC, Font); // return to the normal font
                if (forFrameAdjusted)
                {
                    adjR.top--;
                    adjR.bottom++;
                    forFrameAdjusted = FALSE;
                }
                // set colors for the remaining columns
                SALCOLOR* fgColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_FG_NORMAL] : &highlightMasksItem->NormalFg;
                SALCOLOR* bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_NORMAL] : &highlightMasksItem->NormalBk;
                if (fullRowHighlight && isItemFocusedOrEditMode)
                {
                    fgColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_FG_HIGHLIGHT] : &highlightMasksItem->HighlightFg;
                    bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_HIGHLIGHT] : &highlightMasksItem->HighlightBk;
                }
                SetTextColor(hDC, GetCOLORREF(*fgColor));
                SetBkColor(hDC, GetCOLORREF(*bkColor));
                if (drawFlags & DRAWFLAG_MASK) // mask is b&w; we must not paint a colored background into it
                    SetBkColor(hDC, RGB(255, 255, 255));
            }

            // the order of additional columns is optional => their content will be obtained using
            // callback that fill the buffer with text and then we draw the column
            TransferPluginDataIface = PluginData.GetInterface();
            TransferFileData = f;
            TransferIsDir = isDir ? (isItemUpDir ? 2 : 1) : 0;
            TransferRowData = 0; // this variable must be zero for each row;
                                 // it is used for column drawing optimizations
                                 // within a single row

            int deltaX;
            // skip column Name (i==0) - it is already drawn
            int i;
            for (i = 1; i < Columns.Count; i++)
            {
                CColumn* column = &Columns[i];

                adjR.left = r.left = x;
                adjR.right = r.right = x + column->Width;

                // check whether the column area is at least partially visible
                if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &r))
                {
                    // retrieve the text
                    if (column->ID != COLUMN_ID_EXTENSION)
                    {
                        TransferActCustomData = column->CustomData;
                        column->GetText();
                    }
                    else
                    {
                        // extension is an exception - it always follows Name and we handle it explicitly
                        if (isDir && !Configuration.SortDirsByExt || f->Ext[0] == 0 || f->Ext <= f->Name + 1) // empty value in the Ext column (exception for names like ".htaccess", they appear in the Name column even though they are extensions)
                            TransferLen = 0;
                        else
                        {
                            if (!fileNameFormated)
                                AlterFileName(TransferBuffer, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
                            TransferLen = (int)(f->NameLen - (f->Ext - f->Name));
                            if (TransferLen > 0)
                                MoveMemory(TransferBuffer, TransferBuffer + (f->Ext - f->Name), TransferLen); // buffer overlap may occur
                        }
                    }

                    if (TransferLen == 0)
                        ExtTextOut(hDC, r.left, y, ETO_OPAQUE, &adjR, "", 0, NULL); // just clearing
                    else
                    {
                        if (column->FixedWidth == 1) // NarrowedNameColumn does not apply here (not Name column)
                        {
                            int fitChars;
                            // for fixed-width columns we must check whether the entire text fits
                            int textWidth = r.right - r.left - SPACE_WIDTH;
                            GetTextExtentExPoint(hDC, TransferBuffer, TransferLen, textWidth,
                                                 &fitChars, DrawItemAlpDx, &textSize);
                            if (fitChars < TransferLen)
                            {
                                // search from the end for the character after which we can copy "..." and it fits in the column
                                while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > textWidth)
                                    fitChars--;
                                // copy part of the original string to another buffer
                                int totalCount;
                                if (fitChars > 0)
                                {
                                    memmove(DrawItemBuff, TransferBuffer, fitChars);
                                    // and append "..."
                                    memmove(DrawItemBuff + fitChars, "...", 3);
                                    totalCount = fitChars + 3;
                                }
                                else
                                {
                                    DrawItemBuff[0] = TransferBuffer[0];
                                    DrawItemBuff[1] = '.';
                                    totalCount = 2;
                                }
                                ExtTextOut(hDC, r.left + SPACE_WIDTH / 2, y, ETO_OPAQUE, &adjR, DrawItemBuff, totalCount, NULL);
                            }
                            else
                            {
                                if (column->LeftAlignment == 0)
                                {
                                    deltaX = r.right - r.left - SPACE_WIDTH / 2 - textSize.cx;
                                    if (deltaX < SPACE_WIDTH / 2)
                                        deltaX = SPACE_WIDTH / 2;
                                }
                                else
                                    deltaX = SPACE_WIDTH / 2;
                                ExtTextOut(hDC, r.left + deltaX, y, ETO_OPAQUE, &adjR, TransferBuffer, TransferLen, NULL);
                            }
                        }
                        else
                        {
                            // it's a flexible column and the contents definitely fit
                            if (column->LeftAlignment == 0)
                            {
                                // if the column is right-aligned, measure the text width
                                GetTextExtentPoint32(hDC, TransferBuffer, TransferLen, &textSize);
                                deltaX = r.right - r.left - SPACE_WIDTH / 2 - textSize.cx;
                                if (deltaX < SPACE_WIDTH / 2)
                                    deltaX = SPACE_WIDTH / 2;
                            }
                            else
                                deltaX = SPACE_WIDTH / 2;

                            ExtTextOut(hDC, r.left + deltaX, y, ETO_OPAQUE, &adjR, TransferBuffer, TransferLen, NULL);
                        }
                    }
                }
                x += column->Width;
                if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
                    cacheValidWidth += column->Width;
            }
        }
        if (forFrameAdjusted)
        {
            adjR.top--; // restore the correct dimensions
            adjR.bottom++;
            forFrameAdjusted = FALSE;
        }
        if (GetViewMode() == vmDetailed && !(drawFlags & DRAWFLAG_DIRTY_ONLY) &&
            !(drawFlags & DRAWFLAG_SELFOC_CHANGE))
        {
            if (r.right < rect.right)
            {
                // clear the background
                adjR.left = r.left = r.right;
                adjR.right = r.right = rect.right;
                SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
                if (drawFlags & DRAWFLAG_MASK) // mask is b&w; we must not paint a colored background into it
                    SetBkColor(hDC, RGB(255, 255, 255));
                ExtTextOut(hDC, r.left, r.top, ETO_OPAQUE, &adjR, "", 0, NULL);
            }
        }

        //*****************************************
        //
        // drawing the focus frame
        //
        // if FullRowSelect is off and the name wasn't drawn, we won't even bother with 
        // the frame

        if ((itemIndex == DropTargetIndex || isItemFocusedOrEditMode) &&
            (Configuration.FullRowSelect || focusFrameRightValid))
        {
            r = rect;
            r.left += 1 + IconSizes[ICONSIZE_16] + 1;
            r.right = x;
            if (focusFrameRightValid)
                r.right = focusFrameRight;

            DrawFocusRect(hDC, &r, (f->Selected == 1), Parent->EditMode);
            /*
      // for debugging the dual cursor issue we need to distinguish normal focus and drop target
      if (itemIndex == DropTargetIndex)
      {
        MoveToEx(hDC, r.left, r.top + 3, NULL);
        LineTo(hDC, r.left+3, r.top);
      }
*/
        }
    }

    if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
    {
        // we used the cache - blit it to the screen
        int width = cacheValidWidth - xOffset;
        if (width > 0)
            BitBlt(hTgtDC, itemRect->left, itemRect->top,
                   width, itemRect->bottom - itemRect->top,
                   hDC, 0, 0, SRCCOPY);
    }

    if (showCaret)
        ShowCaret(ListBox->HWindow);
}

//****************************************************************************
//
// SplitText
//
// Uses the 'DrawItemAlpDx' array
//
// text      [IN]  input string which we'll split
// textLen   [IN]  number of characters in string 'text' (without terminator)
// maxWidth  [IN]  maximum width in pixels which a longer line may have
//           [OUT] actual maximum width
// out1      [OUT] first output line without terminator will copy here
// out1Len   [IN]  maximum number of characters that can be written to 'out1'
//           [OUT] number of characters copied to 'out1'
// out1Width [OUT] width of 'out1' in pixels
// out2            same as out1 but for the second line
// out2Len
// out2Width
//

void SplitText(HDC hDC, const char* text, int textLen, int* maxWidth,
               char* out1, int* out1Len, int* out1Width,
               char* out2, int* out2Len, int* out2Width)
{
    SIZE sz;
    // measure the width of every character
    GetTextExtentExPoint(hDC, text, textLen, 0, NULL, DrawItemAlpDx, &sz);

    if (sz.cx > *maxWidth)
    {
        // if the text length exceeds the maximum width,
        // try to split it into two lines at a space
        // anything that still exceeds is replaced with "..."

        // find the last character that still fits on the first line
        // while also tracking the index of the last space
        int lastSpaceIndex = -1;
        int maxW = *maxWidth;
        int w = 0;
        int index = 0;
        while (index < maxW) // this condition should not be applied
        {
            if (text[index] == ' ')
                lastSpaceIndex = index;
            if (DrawItemAlpDx[index] <= maxW)
                index++;
            else
                break;
        }

        if (lastSpaceIndex != -1)
        {
            // if we found a space, break the first line there
            // (the space is omitted to save room)
            if (lastSpaceIndex > 0)
            {
                *out1Len = min(*out1Len, lastSpaceIndex);
                *out1Width = DrawItemAlpDx[lastSpaceIndex - 1];
                memmove(out1, text, *out1Len);
            }
            else
            {
                *out1Len = 0;
                *out1Width = 0;
            }

            // move the pointer past the space
            index = lastSpaceIndex + 1;
        }
        else
        {
            // no space encountered yet, so we must break with an ellipsis

            // append "..." at the end of the string
            int backTrackIndex = index - 1;
            while (DrawItemAlpDx[backTrackIndex] + TextEllipsisWidth > maxW && backTrackIndex > 0)
                backTrackIndex--;

            *out1Len = min(*out1Len, backTrackIndex + 3);
            *out1Width = DrawItemAlpDx[backTrackIndex - 1] + TextEllipsisWidth;
            memmove(out1, text, *out1Len - 3);
            if (*out1Len >= 3)
                memmove(out1 + *out1Len - 3, "...", 3);

            // look for a space where we can continue to the next line
            while (index < textLen)
            {
                if (text[index++] == ' ')
                    break;
            }
        }

        if (index < textLen)
        {
            // process the second line
            int oldIndex = index;
            int offsetX;

            if (index > 0)
                offsetX = DrawItemAlpDx[index - 1]; // width of the first line including the separating space
            else
                offsetX = 0;

            while (index < textLen)
            {
                if (DrawItemAlpDx[index] - offsetX < maxW)
                    index++;
                else
                    break;
            }

            if (index < textLen)
            {
                // the second line didn't fit completely; append an ellipsis
                int backTrackIndex = index - 1;
                while (DrawItemAlpDx[backTrackIndex] - offsetX + TextEllipsisWidth > maxW &&
                       backTrackIndex > oldIndex)
                    backTrackIndex--;

                *out2Len = min(*out2Len, backTrackIndex - oldIndex + 3);
                *out2Width = DrawItemAlpDx[backTrackIndex - 1] - offsetX + TextEllipsisWidth;
                memmove(out2, text + oldIndex, *out2Len - 3);
                if (*out2Len >= 3)
                    memmove(out2 + *out2Len - 3, "...", 3);
            }
            else
            {
                // the second line fit completely
                memmove(out2, text + oldIndex, index - oldIndex);
                *out2Len = index - oldIndex;
                *out2Width = DrawItemAlpDx[index - 1] - offsetX;
            }
        }
        else
        {
            // nothing to put on the second line
            *out2Len = 0;
            *out2Width = 0;
        }
    }
    else
    {
        // the first line will contain everything
        *out1Len = min(*out1Len, textLen);
        *out1Width = sz.cx;
        memmove(out1, text, *out1Len);

        // the second line will be empty
        *out2Len = 0;
        *out2Width = 0;
    }
    // maximum width
    *maxWidth = max(*out1Width, *out2Width);
}

//****************************************************************************
//
// DrawIconThumbnailItem
//
// Draws an item in Icons and Thumbnails modes
//

void CFilesWindow::DrawIconThumbnailItem(HDC hTgtDC, int itemIndex, RECT* itemRect,
                                         DWORD drawFlags, CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE_NONE
    if (itemIndex < 0 || itemIndex >= Dirs->Count + Files->Count)
    {
        TRACE_E("itemIndex = " << itemIndex);
        return;
    }
#ifdef _DEBUG
    if (GetViewMode() != vmIcons && GetViewMode() != vmThumbnails)
    {
        TRACE_E("GetViewMode() != vmIcons && GetViewMode() != vmThumbnails");
        return;
    }
#endif // _DEBUG

    if (!CanDrawItems)
        return;

    BOOL isDir = itemIndex < Dirs->Count;
    CFileData* f = isDir ? &Dirs->At(itemIndex) : &Files->At(itemIndex - Dirs->Count);

    if ((drawFlags & DRAWFLAG_DIRTY_ONLY) && f->Dirty == 0)
        return;

    BOOL isItemUpDir = FALSE; // is the drawn item the ".." directory?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" can only be first
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawingLarge itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // drawing the cursor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // switched in the command-line
            isItemFocusedOrEditMode = TRUE;
    }

    RECT rect = *itemRect; // item position shifted by xOffset
                           //  rect.left -= xOffset;
                           //  rect.right -= xOffset;

    HDC hDC;
    /*
  int cacheValidWidth; // meaningful only when using the cache and specifies the number of pixels (the width),
                       // which are rendered in the bitmap and must be transferred to the screen
  if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
  {
//    TRACE_I("drawing index="<<itemIndex);
    // if it is only the cursor position change, skip visibility tests
    drawFlags |= DRAWFLAG_SKIP_VISTEST;

    // we are drawing only a small area and have plenty of time, so using the cache is fine
    hDC = ItemMemDC;

    // the bitmap is empty for now
    cacheValidWidth = 0;

    // we must adjust the rect variable
    rect.top -= itemRect->top;
    rect.bottom -= itemRect->top;
    rect.left -= itemRect->left;
    rect.right -= itemRect->left;
  }
  else
*/
    hDC = hTgtDC; // this is a large paint; skip the cache and draw directly to the screen

    // if not prohibited, clear the flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // we haven't looked up the extension for the drawn item in Associations yet

    //*****************************************
    //
    // drawing the icon
    //

    if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &rect))
    {
        // icon/thumbnail size
        int iconW = IconSizes[iconSize];
        int iconH = IconSizes[iconSize];

        HBITMAP hScaled = NULL; // if not NULL, draw the thumbnail; otherwise draw the icon
        if (GetViewMode() == vmThumbnails)
        {
            // thumbnails are supported only on disk
            if (Is(ptDisk) && !isDir)
            {
                int icon;
                char fileName[MAX_PATH + 4];
                memmove(fileName, f->Name, f->NameLen);
                *(DWORD*)(fileName + f->NameLen) = 0;

                if (IconCache->GetIndex(fileName, icon, NULL, NULL))
                {
                    DWORD flag = IconCache->At(icon).GetFlag();
                    if (flag == 5 || flag == 6) // o.k. || old version
                    {

                        BOOL leaveSection;
                        if (!IconCacheValid)
                        {
                            HANDLES(EnterCriticalSection(&ICSectionUsingThumb));
                            leaveSection = TRUE;
                        }
                        else
                            leaveSection = FALSE;

                        CThumbnailData* thumbnailData;
                        if (IconCache->GetThumbnail(IconCache->At(icon).GetIndex(), &thumbnailData))
                        {
                            // reconstruction of the bitmap
                            hScaled = HANDLES(CreateBitmap(thumbnailData->Width,
                                                           thumbnailData->Height,
                                                           thumbnailData->Planes,
                                                           thumbnailData->BitsPerPixel,
                                                           thumbnailData->Bits));

                            if (hScaled != NULL)
                            {
                                iconW = thumbnailData->Width;
                                iconH = thumbnailData->Height;
                            }
                            else
                                TRACE_E("Error creating bitmap!");
                        }

                        if (leaveSection)
                        {
                            HANDLES(LeaveCriticalSection(&ICSectionUsingThumb));
                        }
                    }
                }
            }
        }

        int iconX = rect.left + (rect.right - rect.left - iconW) / 2;
        int iconY;
        if (GetViewMode() == vmThumbnails)
            iconY = rect.top + (rect.bottom - Configuration.IconSpacingVert - rect.top - iconH) / 2 + 3;
        else
            iconY = rect.top + 2;

        // outer rectangle from which we clear towards the inner one
        RECT outerRect = rect;

        // rectangle we will clear to
        RECT innerRect;
        BOOL thickFrame = FALSE; // for Thumbnails only -- should the frame be doubled?
        if (GetViewMode() == vmThumbnails)
        {
            // for Thumbnails it reaches the frame around the thumbnail
            innerRect = rect;
            innerRect.left += (rect.right - rect.left - (ListBox->ThumbnailWidth + 2)) / 2;
            innerRect.right = innerRect.left + ListBox->ThumbnailWidth + 2;
            innerRect.top += 3;
            innerRect.bottom = innerRect.top + ListBox->ThumbnailHeight + 2;

            // pen for the frame
            HPEN hPen;
            if (!f->Selected == 1 && !isItemFocusedOrEditMode)
                hPen = HThumbnailNormalPen;
            else
            {
                thickFrame = TRUE;
                InflateRect(&innerRect, 1, 1);
                if (f->Selected == 0 && isItemFocusedOrEditMode)
                    hPen = HThumbnailFucsedPen;
                else if (f->Selected == 1 && !isItemFocusedOrEditMode)
                    hPen = HThumbnailSelectedPen;
                else
                    hPen = HThumbnailFocSelPen;
            }
            SelectObject(hDC, hPen);
            SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
            outerRect.bottom = innerRect.bottom;
        }
        else
        {
            // for Icons it extends to the icon
            innerRect.left = iconX;
            innerRect.top = iconY;
            innerRect.right = iconX + iconW;
            innerRect.bottom = iconY + iconH;
            outerRect.bottom = innerRect.bottom;
        }

        // clear the background around the icon or frame (for Thumbnails)
        if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
            FillIntersectionRegion(hDC, &outerRect, &innerRect);

        RECT overlayRect;
        overlayRect = innerRect;
        InflateRect(&overlayRect, -1, -1);
        if (GetViewMode() == vmThumbnails && thickFrame)
            InflateRect(&overlayRect, -1, -1);

        if (GetViewMode() == vmThumbnails)
        {
            if ((drawFlags & DRAWFLAG_SKIP_FRAME) == 0)
            {
                // draw the frame around the thumbnail
                Rectangle(hDC, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom);
                if (thickFrame)
                {
                    InflateRect(&innerRect, -1, -1);
                    Rectangle(hDC, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom);
                }
                InflateRect(&innerRect, -1, -1);

                // clear the area between the frame and the icon
                RECT iiRect;
                iiRect.left = iconX;
                iiRect.top = iconY;
                iiRect.right = iiRect.left + iconW;
                iiRect.bottom = iiRect.top + iconH;
                if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
                    FillIntersectionRegion(hDC, &innerRect, &iiRect);
            }
        }

        if (GetViewMode() == vmThumbnails)
            drawFlags |= DRAWFLAG_NO_STATE;

        if (hScaled == NULL)
        {
            // no thumbnail available -> draw the icon
            DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                     iconX, iconY, iconSize, (GetViewMode() == vmThumbnails ? &overlayRect : NULL),
                     drawFlags);
        }
        else
        {
            // draw the thumbnail
            HDC hTmpDC = HANDLES(CreateCompatibleDC(hDC));
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hTmpDC, hScaled);

            if ((drawFlags & DRAWFLAG_MASK) != 0)
            {
                // draw the mask for the overlay (the overlay must be drawn first because we cannot draw it transparently)
                DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                         iconX, iconY, iconSize, (GetViewMode() == vmThumbnails ? &overlayRect : NULL),
                         drawFlags | DRAWFLAG_OVERLAY_ONLY);
            }

            // draw the bitmap itself
            BitBlt(hDC, iconX, iconY, iconW, iconH, hTmpDC, 0, 0,
                   (drawFlags & DRAWFLAG_MASK) == 0 ? SRCCOPY : SRCERASE);

            if ((drawFlags & DRAWFLAG_MASK) == 0)
            {
                // draw the overlay (during normal drawing it must be drawn after the bitmap)
                DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                         iconX, iconY, iconSize, (GetViewMode() == vmThumbnails ? &overlayRect : NULL),
                         drawFlags | DRAWFLAG_OVERLAY_ONLY);
            }

            SelectObject(hTmpDC, hOldBitmap);
            HANDLES(DeleteDC(hTmpDC));
            HANDLES(DeleteObject(hScaled));
        }
    }

    //*****************************************
    //
    // text drawing
    //

    if (!(drawFlags & DRAWFLAG_ICON_ONLY))
    {
        // select the appropriate font
        SelectObject(hDC, Font);

        // will we later draw a frame around the item?
        BOOL drawFocusFrame = (itemIndex == DropTargetIndex || isItemFocusedOrEditMode) &&
                              (drawFlags & DRAWFLAG_NO_FRAME) == 0;

        // color detection
        CHighlightMasksItem* highlightMasksItem = MainWindow->HighlightMasks->AgreeMasks(f->Name, isDir ? NULL : f->Ext, f->Attr);

        // set the applied font, background color and text color
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); // we want black text on black background because of aliased fonts
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text and background
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        int nameLen = f->NameLen;
        int itemWidth = rect.right - rect.left; // item width

        // format the name to the user-defined form
        AlterFileName(TransferBuffer, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

        // maximum width available for the text
        int maxWidth = itemWidth - 4 - 1; // -1 so they don't touch
        char* out1 = DrawItemBuff;
        int out1Len = 512;
        int out1Width;
        char* out2 = DrawItemBuff + 512;
        int out2Len = 512;
        int out2Width;
        SplitText(hDC, TransferBuffer, nameLen, &maxWidth,
                  out1, &out1Len, &out1Width,
                  out2, &out2Len, &out2Width);

        if (isItemUpDir)
        {
            out1Len = 0;
            maxWidth = (GetViewMode() == vmThumbnails) ? ListBox->ThumbnailWidth : 32;
            out1Width = maxWidth;
            out2Len = 0;
        }

        // outer rectangle from which we clear towards the inner one
        int y; // text position
        RECT outerRect = rect;
        if (GetViewMode() == vmThumbnails)
        {
            outerRect.top += 3 + ListBox->ThumbnailHeight + 2;
            y = outerRect.top + 4;
            if (f->Selected == 1 || isItemFocusedOrEditMode)
                outerRect.top++; // thickFrame
        }
        else
        {
            outerRect.top += 2 + IconSizes[ICONSIZE_32];
            y = outerRect.top + 4;
        }

        // inner rectangle we clear towards
        RECT r;
        r.left = rect.left + (itemWidth - maxWidth) / 2 - 2;
        r.top = y - 2;
        r.right = r.left + maxWidth + 4;
        r.bottom = y + FontCharHeight + 2;
        if (out2Len > 0)
            r.bottom += FontCharHeight;

        // clear the background around the text
        if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
            FillIntersectionRegion(hDC, &outerRect, &r);

        if (drawFocusFrame) // if there won't be a frame displayed, reduce the area by it
            InflateRect(&r, -1, -1);

        int oldRTop = r.top;

        // display the centered first line; also clear background of the second line
        // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
        ExtTextOut(hDC, rect.left + (itemWidth - out1Width) / 2, y,
                   ETO_OPAQUE, &r, out1, (drawFlags & DRAWFLAG_MASK) ? 0 : out1Len, NULL);

        // display the centered second line
        if (out2Len > 0)
        {
            // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
            ExtTextOut(hDC, rect.left + (itemWidth - out2Width) / 2, y += FontCharHeight,
                       0, NULL, out2, (drawFlags & DRAWFLAG_MASK) ? 0 : out2Len, NULL);
        }

        //*****************************************
        //
        // drawing the focus frame
        //
        if (drawFocusFrame)
        {
            InflateRect(&r, 1, 1);
            DrawFocusRect(hDC, &r, (f->Selected == 1), Parent->EditMode);
        }
    }
    /*
  SelectObject(hDC, HInactiveNormalPen);
  SelectObject(hDC, HANDLES(GetStockObject(NULL_BRUSH)));
  Rectangle(hDC, rect.left, rect.top, rect.right, rect.bottom);
*/
}

//****************************************************************************
//
// DrawTileItem
//
// Draws an item in Tiles mode
//

void TruncateSringToFitWidth(HDC hDC, char* buffer, int* bufferLen, int maxTextWidth, int* widthNeeded)
{
    if (*bufferLen == 0)
        return;

    SIZE fnSZ;
    int fitChars;
    GetTextExtentExPoint(hDC, buffer, *bufferLen, maxTextWidth,
                         &fitChars, DrawItemAlpDx, &fnSZ);
    if (fitChars < *bufferLen)
    {
        if (*widthNeeded < maxTextWidth)
            *widthNeeded = maxTextWidth;
        // search from the end for the character after which we can copy "..." and it fits in the column
        while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > maxTextWidth)
            fitChars--;
        // copy part of the original string to another buffer
        if (fitChars > 0)
        {
            // and append "..."
            memmove(buffer + fitChars, "...", 3);
            *bufferLen = fitChars + 3;
        }
        else
        {
            buffer[1] = '.';
            *bufferLen = 2;
        }
    }
    else
    {
        if (*widthNeeded < fnSZ.cx)
            *widthNeeded = fnSZ.cx;
    }
}

void GetTileTexts(CFileData* f, int isDir,
                  HDC hDC, int maxTextWidth, int* widthNeeded,
                  char* out0, int* out0Len,
                  char* out1, int* out1Len,
                  char* out2, int* out2Len, DWORD validFileData,
                  CPluginDataInterfaceEncapsulation* pluginData,
                  BOOL isDisk)
{
    // format the name to the user-defined form
    AlterFileName(out0, f->Name, -1, Configuration.FileNameFormat, 0, isDir != 0);
    // 1st line: NAME
    *out0Len = f->NameLen;
    // the string may be longer than available space and may need to be shortened with "..."
    *widthNeeded = 0;
    TruncateSringToFitWidth(hDC, out0, out0Len, maxTextWidth, widthNeeded);

    // 2nd line: SIZE (if known)
    CQuadWord plSize;
    BOOL plSizeValid = FALSE;
    if ((validFileData & VALID_DATA_PL_SIZE) &&
        pluginData->NotEmpty() &&
        pluginData->GetByteSize(f, isDir != 0, &plSize))
    {
        plSizeValid = TRUE;
    }
    if (!plSizeValid && ((validFileData & VALID_DATA_SIZE) == 0 || isDir && !f->SizeValid))
        *out1 = 0;
    else
        PrintDiskSize(out1, plSizeValid ? plSize : f->Size, 0);
    *out1Len = (int)strlen(out1);
    // the string may be longer than available space and may need to be shortened with "..."
    TruncateSringToFitWidth(hDC, out1, out1Len, maxTextWidth, widthNeeded);

    // 3rd line DATE TIME (if known)
    SYSTEMTIME st;
    FILETIME ft;
    BOOL validDate = FALSE;
    BOOL validTime = FALSE;
    BOOL invalidDate = FALSE;
    BOOL invalidTime = FALSE;
    if (validFileData & (VALID_DATA_DATE | VALID_DATA_TIME)) // at least something is in LastWrite
    {
        if (!FileTimeToLocalFileTime(&f->LastWrite, &ft) ||
            !FileTimeToSystemTime(&ft, &st))
        {
            if (validFileData & VALID_DATA_DATE)
                invalidDate = TRUE;
            if (validFileData & VALID_DATA_TIME)
                invalidTime = TRUE;
        }
        else
        {
            if (isDir != 2 /* UP-DIR */ || !isDisk || st.wYear != 1602 || st.wMonth != 1 || st.wDay != 1 ||
                st.wHour != 0 || st.wMinute != 0 || st.wSecond != 0 || st.wMilliseconds != 0)
            {
                if (validFileData & VALID_DATA_DATE)
                    validDate = TRUE;
                if (validFileData & VALID_DATA_TIME)
                    validTime = TRUE;
            }
        }
    }
    if ((validFileData & VALID_DATA_PL_DATE) &&
        pluginData->NotEmpty() &&
        pluginData->GetLastWriteDate(f, isDir != 0, &st))
    {
        validDate = TRUE;
    }
    if ((validFileData & VALID_DATA_PL_TIME) &&
        pluginData->NotEmpty() &&
        pluginData->GetLastWriteTime(f, isDir != 0, &st))
    {
        validTime = TRUE;
    }
    if (!validDate) // no date set, so we set one...
    {
        st.wYear = 2000;
        st.wMonth = 12;
        st.wDay = 24;
        st.wDayOfWeek = 0; // Sunday
    }
    if (!validTime) // no time set, so we set one...
    {
        st.wHour = 0;
        st.wMinute = 0;
        st.wSecond = 0;
        st.wMilliseconds = 0;
    }

    out2[0] = 0;
    int out2LenA = 0;
    int out2LenB = 0;
    if (validDate || invalidDate)
    {
        if (validDate)
        {
            out2LenA = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, out2, 500) - 1;
            if (out2LenA < 0)
                out2LenA = sprintf(out2, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
        }
        else
            out2LenA = sprintf(out2, LoadStr(IDS_INVALID_DATEORTIME));
        if (validTime || invalidTime)
        {
            out2[out2LenA] = ' ';
            out2LenA++;
        }
    }
    if (validTime || invalidTime)
    {
        if (validTime)
        {
            out2LenB = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, out2 + out2LenA, 500 - out2LenA) - 1;
            if (out2LenB < 0)
                out2LenB = sprintf(out2 + out2LenA, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
        }
        else
            out2LenB = sprintf(out2 + out2LenA, LoadStr(IDS_INVALID_DATEORTIME));
    }
    *out2Len = out2LenA + out2LenB;
    // the string may be longer than available space and may need to be shortened with "..."
    TruncateSringToFitWidth(hDC, out2, out2Len, maxTextWidth, widthNeeded);
}

void CFilesWindow::DrawTileItem(HDC hTgtDC, int itemIndex, RECT* itemRect, DWORD drawFlags,
                                CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE_NONE
    if (itemIndex < 0 || itemIndex >= Dirs->Count + Files->Count)
    {
        TRACE_E("itemIndex = " << itemIndex);
        return;
    }
#ifdef _DEBUG
    if (GetViewMode() != vmTiles)
    {
        TRACE_E("GetViewMode() != vmTiles");
        return;
    }
#endif // _DEBUG
    if (!CanDrawItems)
        return;

    BOOL isDir = itemIndex < Dirs->Count;

    CFileData* f = isDir ? &Dirs->At(itemIndex) : &Files->At(itemIndex - Dirs->Count);

    if ((drawFlags & DRAWFLAG_DIRTY_ONLY) && f->Dirty == 0)
        return;

    BOOL isItemUpDir = FALSE; // is the drawn item the ".." directory?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" can only be first
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawTileItem itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // drawing the cursor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // switched in the command-line
            isItemFocusedOrEditMode = TRUE;
    }

    RECT rect = *itemRect; // item position shifted by xOffset

    HDC hDC = hTgtDC; // this is a large paint; skip the cache and draw directly to the screen

    // if not prohibited, clear the flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // we haven't looked up the extension for the drawn item in Associations yet

    //*****************************************
    //
    // drawing the icon
    //

    if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &rect))
    {
        // icon size
        int iconW = IconSizes[iconSize];
        int iconH = IconSizes[iconSize];

        // icon position
        int iconX = rect.left + TILE_LEFT_MARGIN;
        int iconY = rect.top + (rect.bottom - rect.top - iconH) / 2; // center it

        // outer rectangle from which we clear towards the inner one
        RECT outerRect = rect;
        outerRect.right = iconX + iconW;

        // rectangle we will clear towards
        RECT innerRect;
        BOOL thickFrame = FALSE; // for Thumbnails only -- should the frame be doubled?

        innerRect.left = iconX;
        innerRect.top = iconY;
        innerRect.right = iconX + iconW;
        innerRect.bottom = iconY + iconH;

        // clear the background around the icon or frame (for Thumbnails)
        if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
            FillIntersectionRegion(hDC, &outerRect, &innerRect);
        // no thumbnail available -> draw the icon
        DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                 iconX, iconY, iconSize, NULL, drawFlags);
    }

    //*****************************************
    //
    // text drawing
    //

    if (!(drawFlags & DRAWFLAG_ICON_ONLY))
    {
        // select the appropriate font
        SelectObject(hDC, Font);

        // will we later draw a frame around the item?
        BOOL drawFocusFrame = (itemIndex == DropTargetIndex || isItemFocusedOrEditMode) &&
                              (drawFlags & DRAWFLAG_NO_FRAME) == 0;

        // colors detection
        CHighlightMasksItem* highlightMasksItem = MainWindow->HighlightMasks->AgreeMasks(f->Name, isDir ? NULL : f->Ext, f->Attr);

        // set the applied font, background color and text color
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); // we want black text on black background because of aliased fonts
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text and background
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        int nameLen = f->NameLen;
        int itemWidth = rect.right - rect.left; // item width

        // texts must not exceed this length in pixels
        int maxTextWidth = itemWidth - TILE_LEFT_MARGIN - IconSizes[iconSize] - TILE_LEFT_MARGIN - 4;
        int widthNeeded = 0;

        char* out0 = TransferBuffer;
        int out0Len;
        char* out1 = DrawItemBuff;
        int out1Len;
        char* out2 = DrawItemBuff + 512;
        int out2Len;
        GetTileTexts(f, isDir ? (isItemUpDir ? 2 /* UP-DIR */ : 1) : 0, hDC, maxTextWidth, &widthNeeded,
                     out0, &out0Len, out1, &out1Len, out2, &out2Len,
                     ValidFileData, &PluginData, Is(ptDisk));

        if (isItemUpDir)
        {
            out0Len = 0;
        }

        // vnejsi obdelnik, od ktereho mazu smerem k vnitrnimu
        RECT outerRect = rect;
        outerRect.left += TILE_LEFT_MARGIN + IconSizes[iconSize];

        //int oldRTop = r.top;

        // vnitrni obdelnik, ke kterememu mazu
        RECT r;
        r.left = outerRect.left + 2;
        r.right = r.left + 2 + widthNeeded + 2 + 1;

        int visibleLines = 1; // nazev je viditelny urcite
        if (out1[0] != 0)
            visibleLines++;
        if (out2[0] != 0)
            visibleLines++;
        int textH = visibleLines * FontCharHeight + 4;
        int textX = r.left + 2;
        int textY = rect.top + (rect.bottom - rect.top - textH) / 2; // centrujeme

        r.top = textY;
        r.bottom = textY + textH;
        RECT focusR = r; // zazalohujeme pro kresleni focusu

        if (drawFocusFrame)
        {
            r.left++;
            r.right--;
            r.top++;
        }

        textY += 2;

        // clear the background around the text
        if ((drawFlags & DRAWFLAG_MASK) == 0) // when drawing the mask (b&w), the background color must not be painted
            FillIntersectionRegion(hDC, &outerRect, &r);

        //    if (drawFocusFrame) // if there won't be a frame displayed, reduce the area by it
        //      InflateRect(&r, -1, -1);

        // display the first line and also clear the background of the area above it
        r.bottom = textY + FontCharHeight;
        if (out1[0] == 0 && out2[0] == 0) // if this is the last line, clear the background of the area below it
        {
            r.bottom += 2;
            if (drawFocusFrame)
                r.bottom--;
        }
        // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
        ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out0, (drawFlags & DRAWFLAG_MASK) ? 0 : out0Len, NULL);

        // display the second line
        if (out1[0] != 0)
        {
            textY += FontCharHeight;
            r.top = r.bottom;
            r.bottom = r.top + FontCharHeight;
            if (out2[0] == 0) // if this is the last line, clear the background of the area below it
            {
                r.bottom += 2;
                if (drawFocusFrame)
                    r.bottom--;
            }
            // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
            ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out1, (drawFlags & DRAWFLAG_MASK) ? 0 : out1Len, NULL);
        }
        // display the third line and clear the background of the area below it
        if (out2[0] != 0)
        {
            r.top = r.bottom;
            r.bottom = r.top + FontCharHeight + 2;
            if (drawFocusFrame)
                r.bottom--;
            textY += FontCharHeight;
            // DRAWFLAG_MASK: hack, under XP some stuff is added in font of the text in the mask while drawing short texts; not an issue if text is not drawn
            ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out2, (drawFlags & DRAWFLAG_MASK) ? 0 : out2Len, NULL);
        }

        //*****************************************
        //
        // drawing the focus frame
        //
        if (drawFocusFrame)
        {
            DrawFocusRect(hDC, &focusR, (f->Selected == 1), Parent->EditMode);
        }
    }
}

//****************************************************************************
//
// StateImageList_Draw
//

BOOL StateImageList_Draw(CIconList* iconList, int imageIndex, HDC hDC, int xDst, int yDst,
                         DWORD state, CIconSizeEnum iconSize, DWORD iconOverlayIndex,
                         const RECT* overlayRect, BOOL overlayOnly, BOOL iconOverlayFromPlugin,
                         int pluginIconOverlaysCount, HICON* pluginIconOverlays)
{
    COLORREF rgbFg = CLR_DEFAULT;
    BOOL blend = FALSE;
    if (Configuration.UseIconTincture)
    {
        if (state & IMAGE_STATE_FOCUSED && state & IMAGE_STATE_SELECTED)
            rgbFg = GetCOLORREF(CurrentColors[ICON_BLEND_FOCSEL]);
        else if (state & IMAGE_STATE_FOCUSED)
            rgbFg = GetCOLORREF(CurrentColors[ICON_BLEND_FOCUSED]);
        else if (state & IMAGE_STATE_SELECTED)
            rgbFg = GetCOLORREF(CurrentColors[ICON_BLEND_SELECTED]);
        else if (state & IMAGE_STATE_HIDDEN)
            rgbFg = CLR_NONE;
        blend = state & IMAGE_STATE_FOCUSED || state & IMAGE_STATE_SELECTED || state & IMAGE_STATE_HIDDEN;
    }
    DWORD flags;
    COLORREF rgbBk;
    if (blend)
    {
        rgbBk = GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]);
        flags = ILD_NORMAL | ILD_BLEND50;
    }
    else
    {
        rgbBk = CLR_DEFAULT;
        flags = ILD_NORMAL;
    }

    // the overlay may (in case of a thumbnail) lie outside the icon (in the thumbnail's lower left corner)
    int xOverlayDst = xDst;
    int yOverlayDst = yDst;

    // on Vista a 48x48 icon uses overlay ICONSIZE_32 and thumbnails use overlay ICONSIZE_48
    if (iconSize == ICONSIZE_48 && overlayRect == NULL)
    {
        iconSize = ICONSIZE_32;
        yOverlayDst += 48 - 32;
    }

    int iconW = IconSizes[iconSize];
    int iconH = IconSizes[iconSize];

    // for a thumbnail overlayRect != NULL and we move the overlay to its lower left corner
    if (overlayRect != NULL)
    {
        xOverlayDst = overlayRect->left;
        yOverlayDst = overlayRect->bottom - iconH;
    }

    if (state & IMAGE_STATE_MASK)
    {
        // we must swap the order because DrawIconEx doesn't draw the mask transparently
        if (iconOverlayIndex != ICONOVERLAYINDEX_NOTUSED) // if this overlay is loaded it has higher priority than the overlays below
        {
            if (iconOverlayFromPlugin)
            {
                if ((int)iconOverlayIndex < pluginIconOverlaysCount)
                {
                    DrawIconEx(hDC, xOverlayDst, yOverlayDst, pluginIconOverlays[3 * iconOverlayIndex + iconSize],
                               iconW, iconH, 0, NULL, DI_MASK);
                }
                else
                    TRACE_E("StateImageList_Draw(): invalid icon-overlay index: " << iconOverlayIndex << ", max = " << pluginIconOverlaysCount);
            }
            else
            {
                DrawIconEx(hDC, xOverlayDst, yOverlayDst, ShellIconOverlays.GetIconOverlay(iconOverlayIndex, iconSize),
                           iconW, iconH, 0, NULL, DI_MASK);
            }
        }
        else
        {
            if (state & IMAGE_STATE_SHARED)
                DrawIconEx(hDC, xOverlayDst, yOverlayDst, HSharedOverlays[iconSize],
                           iconW, iconH, 0, NULL, DI_MASK);
            else
            {
                if (state & IMAGE_STATE_SHORTCUT)
                    DrawIconEx(hDC, xOverlayDst, yOverlayDst, HShortcutOverlays[iconSize],
                               iconW, iconH, 0, NULL, DI_MASK);
                else
                {
                    if (state & IMAGE_STATE_OFFLINE)
                        DrawIconEx(hDC, xOverlayDst, yOverlayDst, HSlowFileOverlays[iconSize],
                                   iconW, iconH, 0, NULL, DI_MASK);
                }
            }
        }
        if (!overlayOnly)
            iconList->Draw(imageIndex, hDC, xDst, yDst, rgbFg, IL_DRAW_MASK);
    }
    else
    {
        if (!overlayOnly)
            iconList->Draw(imageIndex, hDC, xDst, yDst, rgbFg, blend ? IL_DRAW_BLEND : 0);
        if (iconOverlayIndex != ICONOVERLAYINDEX_NOTUSED) // if this overlay is loaded it has higher priority than the overlays below
        {
            if (iconOverlayFromPlugin)
            {
                if ((int)iconOverlayIndex < pluginIconOverlaysCount)
                {
                    DrawIconEx(hDC, xOverlayDst, yOverlayDst, pluginIconOverlays[3 * iconOverlayIndex + iconSize],
                               iconW, iconH, 0, NULL, DI_NORMAL);
                }
                else
                    TRACE_E("StateImageList_Draw(): invalid icon-overlay index: " << iconOverlayIndex << ", max = " << pluginIconOverlaysCount);
            }
            else
            {
                DrawIconEx(hDC, xOverlayDst, yOverlayDst, ShellIconOverlays.GetIconOverlay(iconOverlayIndex, iconSize),
                           iconW, iconH, 0, NULL, DI_NORMAL);
            }
        }
        else
        {
            if (state & IMAGE_STATE_SHARED)
                DrawIconEx(hDC, xOverlayDst, yOverlayDst, HSharedOverlays[iconSize],
                           iconW, iconH, 0, NULL, DI_NORMAL);
            else
            {
                if (state & IMAGE_STATE_SHORTCUT)
                    DrawIconEx(hDC, xOverlayDst, yOverlayDst, HShortcutOverlays[iconSize],
                               iconW, iconH, 0, NULL, DI_NORMAL);
                else
                {
                    if (state & IMAGE_STATE_OFFLINE)
                        DrawIconEx(hDC, xOverlayDst, yOverlayDst, HSlowFileOverlays[iconSize],
                                   iconW, iconH, 0, NULL, DI_NORMAL);
                }
            }
        }
    }

    return TRUE;
}
