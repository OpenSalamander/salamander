// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "shiconov.h"

//****************************************************************************
//
// Paint panelu
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
        // barva textu
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

    // nastavime barvu pozadi
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
    int symbolIndex;                   // index v bitmape Symbols...
    DWORD iconState = 0;               // flagy pro vykresleni ikonky
    char lowerExtension[MAX_PATH + 4]; // pripona malymi pismeny, zarovnana na DWORDy

    if (!(drawFlags & DRAWFLAG_NO_STATE))
    {
        if ((f->Hidden == 1 || f->CutToClip == 1) && !isItemUpDir)
        {
            if (f->CutToClip == 0 && f->Selected == 1)
                iconState |= IMAGE_STATE_SELECTED; //hidden budou mit selected state (cut ne)
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
        // znaky pripony konvertuju do malych pismen
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

    // behem prepinani mezi rezimy panelu je nastavena TemporarilySimpleIcons==TRUE
    // takze pro kresleni ikon nepouzijeme jeste nepripravenou IconCache
    if (!TemporarilySimpleIcons && UseSystemIcons)
    {
        if (symbolIndex != symbolsUpDir && symbolIndex != symbolsArchive)
        {
            CIconList* iconList = NULL;
            int iconListIndex = -1; // at zavreme, pokud nebude nastavena
            char fileName[MAX_PATH + 4];

            if (GetPluginIconsType() != pitFromPlugin || !Is(ptPluginFS))
            {
                if (isDir) // jde o adresar
                {
                    int icon;
                    memmove(fileName, f->Name, f->NameLen);
                    *(DWORD*)(fileName + f->NameLen) = 0;

                    if (!IconCache->GetIndex(fileName, icon, NULL, NULL) ||                             // icon-thread ho nenacita
                        IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 ||     // ikona neni nactena nova ani stara
                        !IconCache->GetIcon(IconCache->At(icon).GetIndex(), &iconList, &iconListIndex)) // nepovede se ziskat jeho ikonu
                    {                                                                                   // budeme zobrazovat simple-symbol
                        if (!Associations.GetIcon(ASSOC_ICON_SOME_DIR, &iconList, &iconListIndex, iconSize))
                        {
                            iconList = NULL;
                            drawSimpleSymbol = TRUE;
                        }
                    }
                }
                else // jde o soubor
                {
                    int index;
                    BOOL exceptions = *(DWORD*)lowerExtension == *(DWORD*)"scr" || // ikony v souboru,
                                      *(DWORD*)lowerExtension == *(DWORD*)"pif" || // i kdyz to z Registry
                                      *(DWORD*)lowerExtension == *(DWORD*)"lnk";   // neni videt

                    if (exceptions || Associations.GetIndex(lowerExtension, index)) // pripona ma ikonku (asociaci)
                    {
                        if (!exceptions)
                            TransferAssocIndex = index;                               // zapamatujeme si platny index v Associations
                        if (exceptions || Associations[index].GetIndex(iconSize) < 0) // dynamicka ikonka (v souboru) nebo nacitana staticka ikona
                        {                                                             // ikona v souboru
                            int icon;
                            memmove(fileName, f->Name, f->NameLen);
                            *(DWORD*)(fileName + f->NameLen) = 0;
                            if (!IconCache->GetIndex(fileName, icon, NULL, NULL) ||                         // icon-thread ji nenacita
                                IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 || // ikona neni nactena nova ani stara
                                !IconCache->GetIcon(IconCache->At(icon).GetIndex(),
                                                    &iconList, &iconListIndex)) // nepovede se ziskat nactenou ikonku
                            {                                                   // budeme zobrazovat simple-symbol
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
                            index = -2; // zobrazuje se dynamicka ikonka
                        }
                        // else -> index je index ikonky spolecne pro soubory s danou priponou
                    }
                    else
                    {
                        index = -1;              // soubor nema zadnou zvlastni ikonku
                        TransferAssocIndex = -1; // zapamatujeme si, ze tato pripona v Associations neni
                    }
                    if (index != -2)     // ikona v souboru, 1. pruchod -> nectem
                    {                    // zkusime vykreslit ikonku,
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

                if (!IconCache->GetIndex(NULL /*fileName*/, icon, &PluginData, f) ||                // icon-thread ho nenacita
                    IconCache->At(icon).GetFlag() != 1 && IconCache->At(icon).GetFlag() != 2 ||     // ikona neni nactena nova ani stara
                    !IconCache->GetIcon(IconCache->At(icon).GetIndex(), &iconList, &iconListIndex)) // nepovede se ziskat jeho ikonu
                {                                                                                   // budeme zobrazovat simple-symbol z plug-inu
                    // index jednoduche ikony budeme ziskavat pomoci callbacku GetPluginIconIndex
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
            drawSimpleSymbol = TRUE; // adresar ".."
    }
    else
        drawSimpleSymbol = TRUE;

    if (drawSimpleSymbol)
    { // simple symbols
        StateImageList_Draw(SimpleIconLists[iconSize], symbolIndex, hDC, x, y, iconState, iconSize,
                            f->IconOverlayIndex, overlayRect, (drawFlags & DRAWFLAG_OVERLAY_ONLY) != 0,
                            iconOverlayFromPlugin, pluginIconOverlaysCount, pluginIconOverlays);
    }

    // nechapu proc, ale soucasna verze kresleni bitmap neblika.
    // pravdepodobne v pripade blendu jsou ikonky kresleny pres memory dc
    //    BitBlt(hDC, iconRect.left, iconRect.top,
    //           iconRect.right - iconRect.left,
    //           iconRect.bottom - iconRect.top,
    //           ItemMemDC, 0, 0, SRCCOPY);
}

//****************************************************************************
//
// FillIntersectionRegion
//
// pomoci FillRect vyplni plochy mezi
// oRect (vnejsim) a iRect (vnitrnim) obdelnikem
//

void FillIntersectionRegion(HDC hDC, const RECT* oRect, const RECT* iRect)
{
    RECT r;

    // prostor vlevo od innerRect
    r = *oRect;
    r.right = iRect->left;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // prostor vpravo od iRect
    r = *oRect;
    r.left = iRect->right;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // prostor nad iRect
    r = *iRect;
    r.top = oRect->top;
    r.bottom = iRect->top;
    if (r.left < r.right && r.top < r.bottom)
        FillRect(hDC, &r, HNormalBkBrush);

    // prostor pod iRect
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
// Kresli focus (plna/carkovana) kolem polozky
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
// Kresli polozku v rezimech Brief a Detailed
//

//
// zmeny v paintu polozky (rozlozeni) je treba zanest na tato mista:
//
// CFilesWindow::DrawItem(WPARAM wParam, LPARAM lParam)
// CFilesBox::GetIndex(int x, int y)
// CFilesMap::CreateMap()
//

char DrawItemBuff[1024]; // cilovy buffer pro retezce
int DrawItemAlpDx[1024]; // pro napocitavani sirek u sloupcu s FixedWidth bitem + elastickych sloupcu se zaplym smart-modem

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

    BOOL isItemUpDir = FALSE; // je vykreslovana polozka adresar ".."?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" muze byt jen prvni
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawingSmall itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // kreslime kurzor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // prepnuto v command-line
            isItemFocusedOrEditMode = TRUE;
    }
    if (drawFlags & DRAWFLAG_NO_FRAME)
        isItemFocusedOrEditMode = FALSE;

    BOOL fullRowHighlight = FALSE;
    if (GetViewMode() == vmDetailed && !Configuration.FullRowSelect && Configuration.FullRowHighlight)
        fullRowHighlight = TRUE;

    int xOffset = ListBox->XOffset;

    RECT rect = *itemRect; // pozice polozky posunuta o xOffset
    rect.left -= xOffset;
    rect.right -= xOffset;

    HDC hDC;
    int cacheValidWidth; // ma vyznam pouze pokud jedeme pres cache a udava pocet bodu (sirku),
                         // ktere jsou v bitmape vykresleny a je treba je prenest do obrazovky
    if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
    {
        //    TRACE_I("drawing index="<<itemIndex);
        // pokud jde o zmenu pozice kurzoru, nebudeme provadet zadne testy viditelnosti
        drawFlags |= DRAWFLAG_SKIP_VISTEST;

        // jedna se pouze o kresleni male plochy a casu je dost - pojedeme pres cache
        hDC = ItemBitmap.HMemDC;

        // zatim mame bitmapu prazdnou
        cacheValidWidth = 0;

        // musime provest korekci promenne rect
        rect.top -= itemRect->top;
        rect.bottom -= itemRect->top;
        rect.left -= itemRect->left;
        rect.right -= itemRect->left;
    }
    else
        hDC = hTgtDC; // jedna se o veliky paint - peceme na cache a pujdem rovnou do videa

    // pokud mi to nezatrhli, shodim flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // zatim jsme priponu pro vykreslovanou polozku v Associations nehledali

    //*****************************************
    //
    // vykresleni ikony
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

        // podmazu prostor kolem ikony
        if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkladovou barvu
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
            f->Ext[0] != 0 && f->Ext > f->Name + 1) // vyjimka pro jmena jako ".htaccess", ukazuji se ve sloupci Name i kdyz jde o pripony
        {
            nameLen = (int)(f->Ext - f->Name - 1);
        }
        else
            nameLen = f->NameLen;

        // nastavim pouzity font, barvu pozadi a barvu textu
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        RECT r = rect;
        int x = r.left;
        int y = (r.top + r.bottom - FontCharHeight) / 2;

        BOOL fileNameFormated = FALSE;
        SIZE textSize;
        SetTextAlign(hDC, TA_TOP | TA_LEFT | TA_NOUPDATECP);

        // platne, pokud bude zkracen focus
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

        // pokud je polozka orameckovana, budeme kreslit pouze vnitrek
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
        // vykresleni textu
        //
        // !!! pozor !!!
        // pro hladke vykresleni cele radky je pouzivan GdiBatchLimit
        // je treba zajistit, aby v prubehu kresleni textu nebylo prerusene
        // bufferovani (GDI funkce nevracejici BOOL volaji GdiFlush()
        //
        //    GdiFlush();

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); //chceme cerny text na cernem pozadi, kvuli aliasovanym fontum
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text i pozadi
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &r))
        {
            if (GetViewMode() == vmBrief && Configuration.FullRowSelect)
            {
                r.right -= 10; // korekce - musime vytvorit prostor pro tazeni klece
                focusFrameRightValid = TRUE;
                focusFrameRight = r.right;
            }

            if (isItemUpDir)
            {
                nameLen = 0;
                if (GetViewMode() == vmBrief && !Configuration.FullRowSelect)
                {
                    adjR.right -= 10; // 10 - abychom nebyli roztazeni pres celou sirku
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
                // namerime skutecnou delku textu
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

                // focus bude take kratsi
                focusFrameRightValid = TRUE;
                focusFrameRight = r.right;
            }
            if (!isItemUpDir && GetViewMode() == vmDetailed && (column->FixedWidth == 1 || NarrowedNameColumn))
            {
                if (Configuration.FullRowSelect)
                {
                    // jeste nemame namereno - jdeme na to
                    // retezec muze byt delsi nez dostupne misto a je treba zakoncit ho vypustkou ...
                    textWidth = nameWidth - 1 - IconSizes[ICONSIZE_16] - 1 - 2 - SPACE_WIDTH;
                    GetTextExtentExPoint(hDC, TransferBuffer, nameLen, textWidth,
                                         &fitChars, DrawItemAlpDx, &fnSZ);
                }
                if (fitChars < nameLen)
                {
                    // vyhledam od konce znak, za ktery muzu nakopirovat "..." a vejdou se do sloupce
                    while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > textWidth)
                        fitChars--;
                    // do jineho bufferu nakopcim cast puvodniho retezce
                    int totalCount;
                    if (fitChars > 0)
                    {
                        memmove(DrawItemBuff, TransferBuffer, fitChars);
                        // a pripojim "..."
                        memmove(DrawItemBuff + fitChars, "...", 3);
                        totalCount = fitChars + 3;
                    }
                    else
                    {
                        DrawItemBuff[0] = TransferBuffer[0];
                        DrawItemBuff[1] = '.';
                        totalCount = 2;
                    }

                    // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
                    ExtTextOut(hDC, r.left + 2, y, ETO_OPAQUE, &adjR, DrawItemBuff, (drawFlags & DRAWFLAG_MASK) ? 0 : totalCount, NULL);
                    goto SKIP1;
                }
            }
            // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
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
                    // domazu zbytek nazvu
                    adjR.left = r.left = r.right;
                    adjR.right = r.right = x + nameWidth;
                    SALCOLOR* bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_NORMAL] : &highlightMasksItem->NormalBk;
                    if (fullRowHighlight && isItemFocusedOrEditMode)
                        bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_HIGHLIGHT] : &highlightMasksItem->HighlightBk;
                    SetBkColor(hDC, GetCOLORREF(*bkColor));
                    if (drawFlags & DRAWFLAG_MASK) // maska je b&w, nesmime do ni kreslit barevne pozadi
                        SetBkColor(hDC, RGB(255, 255, 255));
                    ExtTextOut(hDC, 0, 0, ETO_OPAQUE, &r, "", 0, NULL);
                }
            }
        }
        x += nameWidth;
        if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
            cacheValidWidth = nameWidth;
        // neni-li FullRowSelect a kreslim pouze dirty polozky nebo doslo jen ke zmene
        // focusu/selectu, nebudu se zatezovat kreslenim dalsich sloupcu - nic se tam nezmenilo
        if (GetViewMode() == vmDetailed && (fullRowHighlight || Configuration.FullRowSelect ||
                                            !(drawFlags & DRAWFLAG_DIRTY_ONLY) && !(drawFlags & DRAWFLAG_SELFOC_CHANGE)))
        {
            if (!Configuration.FullRowSelect)
            {
                if (TrackingSingleClick && SingleClickIndex == itemIndex)
                    SelectObject(hDC, Font); // navrat ke klasickemu fontu
                if (forFrameAdjusted)
                {
                    adjR.top--;
                    adjR.bottom++;
                    forFrameAdjusted = FALSE;
                }
                // nastavim barvy pro zbyle sloupce
                SALCOLOR* fgColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_FG_NORMAL] : &highlightMasksItem->NormalFg;
                SALCOLOR* bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_NORMAL] : &highlightMasksItem->NormalBk;
                if (fullRowHighlight && isItemFocusedOrEditMode)
                {
                    fgColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_FG_HIGHLIGHT] : &highlightMasksItem->HighlightFg;
                    bkColor = (highlightMasksItem == NULL) ? &CurrentColors[ITEM_BK_HIGHLIGHT] : &highlightMasksItem->HighlightBk;
                }
                SetTextColor(hDC, GetCOLORREF(*fgColor));
                SetBkColor(hDC, GetCOLORREF(*bkColor));
                if (drawFlags & DRAWFLAG_MASK) // maska je b&w, nesmime do ni kreslit barevne pozadi
                    SetBkColor(hDC, RGB(255, 255, 255));
            }

            // poradi dalsich sloupcu je volitelne => jejich obsah budeme ziskavat pomoci
            // callbacku, ktere nam naplni buffer textem a my uz sloupec vykreslime
            TransferPluginDataIface = PluginData.GetInterface();
            TransferFileData = f;
            TransferIsDir = isDir ? (isItemUpDir ? 2 : 1) : 0;
            TransferRowData = 0; // pro kazdy radek musi byt tato promenna nulovana;
                                 // je vyuzivana k optimalizacim v ramci kresleni sloupcu
                                 // v jednom radku

            int deltaX;
            // preskocim sloupec Name (i==0) - ten uz je vykresleny
            int i;
            for (i = 1; i < Columns.Count; i++)
            {
                CColumn* column = &Columns[i];

                adjR.left = r.left = x;
                adjR.right = r.right = x + column->Width;

                // overime, jestli je plocha pro sloupec alepson castecne viditelna
                if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &r))
                {
                    // vytahneme text
                    if (column->ID != COLUMN_ID_EXTENSION)
                    {
                        TransferActCustomData = column->CustomData;
                        column->GetText();
                    }
                    else
                    {
                        // extension je vyjimka - vzdy nasleduje za Name a resime ji explicitne
                        if (isDir && !Configuration.SortDirsByExt || f->Ext[0] == 0 || f->Ext <= f->Name + 1) // prazdna hodnota v Ext sloupci (vyjimka pro jmena jako ".htaccess", ukazuji se ve sloupci Name i kdyz jde o pripony)
                            TransferLen = 0;
                        else
                        {
                            if (!fileNameFormated)
                                AlterFileName(TransferBuffer, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
                            TransferLen = (int)(f->NameLen - (f->Ext - f->Name));
                            if (TransferLen > 0)
                                MoveMemory(TransferBuffer, TransferBuffer + (f->Ext - f->Name), TransferLen); // muze dojit k prekryti bufferu
                        }
                    }

                    if (TransferLen == 0)
                        ExtTextOut(hDC, r.left, y, ETO_OPAQUE, &adjR, "", 0, NULL); // pouze mazeme
                    else
                    {
                        if (column->FixedWidth == 1) // tady se NarrowedNameColumn neuplatni (nejde o sloupec Name)
                        {
                            int fitChars;
                            // pokud jde o sloupce s pevnou sirkou, musim zkontrolovat jestli se cely vejde
                            int textWidth = r.right - r.left - SPACE_WIDTH;
                            GetTextExtentExPoint(hDC, TransferBuffer, TransferLen, textWidth,
                                                 &fitChars, DrawItemAlpDx, &textSize);
                            if (fitChars < TransferLen)
                            {
                                // vyhledam od konce znak, za ktery muzu nakopirovat "..." a vejdou se do sloupce
                                while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > textWidth)
                                    fitChars--;
                                // do jineho bufferu nakopcim cast puvodniho retezce
                                int totalCount;
                                if (fitChars > 0)
                                {
                                    memmove(DrawItemBuff, TransferBuffer, fitChars);
                                    // a pripojim "..."
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
                            // jde o elasticky sloupec a obsah se vejde urcite
                            if (column->LeftAlignment == 0)
                            {
                                // je-li sloupec zarovnan vpravo, musim zmerit sirku textu
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
            adjR.top--; // vratime se ke spravnym rozmerum
            adjR.bottom++;
            forFrameAdjusted = FALSE;
        }
        if (GetViewMode() == vmDetailed && !(drawFlags & DRAWFLAG_DIRTY_ONLY) &&
            !(drawFlags & DRAWFLAG_SELFOC_CHANGE))
        {
            if (r.right < rect.right)
            {
                // domazu pozadi
                adjR.left = r.left = r.right;
                adjR.right = r.right = rect.right;
                SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_NORMAL]));
                if (drawFlags & DRAWFLAG_MASK) // maska je b&w, nesmime do ni kreslit barevne pozadi
                    SetBkColor(hDC, RGB(255, 255, 255));
                ExtTextOut(hDC, r.left, r.top, ETO_OPAQUE, &adjR, "", 0, NULL);
            }
        }

        //*****************************************
        //
        // vykresleni focus ramecku
        //
        // pokud neni FullRowSelect a nevykresloval se nazev, ani se s rameckem nebudeme
        // obtezovat

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
      // pro odladeni chyby dvou kurzoru potrebujeme rozlisit normalni focus a drop target
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
        // jeli jsme pres cache - praskneme ji do obrazovky
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
// Vyuziva pole 'DrawItemAlpDx'
//
// text      [IN]  vstupni retezec, ktery budeme delit
// textLen   [IN]  pocet znaku v retezci 'text' (bez terminatoru)
// maxWidth  [IN]  maximalni pocet bodu, kolik delsi radek na sirku muze mit
//           [OUT] skutecna maximalni sirka sirka
// out1      [OUT] do tohoto retezce bude nakopirovan prvni radek vystupu bez terminatoru
// out1Len   [IN]  maximalni mozny pocet znaku, ktere lze zapsat do 'out1'
//           [OUT] pocet znaku nakopirovanych do 'out1'
// out1Width [OUT] sirka 'out1' v bodech
// out2            stejne jako out1, ale pro druhy radek
// out2Len
// out2Width
//

void SplitText(HDC hDC, const char* text, int textLen, int* maxWidth,
               char* out1, int* out1Len, int* out1Width,
               char* out2, int* out2Len, int* out2Width)
{
    SIZE sz;
    // namerime delky vsech znaku
    GetTextExtentExPoint(hDC, text, textLen, 0, NULL, DrawItemAlpDx, &sz);

    if (sz.cx > *maxWidth)
    {
        // pokud delka textu presahuje maximalni sirku,
        // pokusime se jej rozdelit do dvou radku v miste mezery
        // co bude presahovat, nahradime trema teckama

        // hledame posledni znak, ktery se jeste vejde na prvni radek
        // zaroven hledame index posledni mezery
        int lastSpaceIndex = -1;
        int maxW = *maxWidth;
        int w = 0;
        int index = 0;
        while (index < maxW) // tato podminka by se nemela uplatnit
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
            // pokud jsme nasli nejakou mezeru, zalomime prvni radek na ni
            // (mezeru usporne vypoustime)
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

            // posuneme ukazovatko za mezeru
            index = lastSpaceIndex + 1;
        }
        else
        {
            // zatim nebyla zadna mezera, musime zalomit vypustkou

            // na konec retezce soupnem vypustku "..."
            int backTrackIndex = index - 1;
            while (DrawItemAlpDx[backTrackIndex] + TextEllipsisWidth > maxW && backTrackIndex > 0)
                backTrackIndex--;

            *out1Len = min(*out1Len, backTrackIndex + 3);
            *out1Width = DrawItemAlpDx[backTrackIndex - 1] + TextEllipsisWidth;
            memmove(out1, text, *out1Len - 3);
            if (*out1Len >= 3)
                memmove(out1 + *out1Len - 3, "...", 3);

            // hledame mezeru, kde bychom mohli prejit na dalsi radek
            while (index < textLen)
            {
                if (text[index++] == ' ')
                    break;
            }
        }

        if (index < textLen)
        {
            // zpracujeme druhy radek
            int oldIndex = index;
            int offsetX;

            if (index > 0)
                offsetX = DrawItemAlpDx[index - 1]; // sirka prvniho radku vcetne delici mezery
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
                // druhy radek se nevesel cely; pripojime vypustku
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
                // druhy radek se vesel cely
                memmove(out2, text + oldIndex, index - oldIndex);
                *out2Len = index - oldIndex;
                *out2Width = DrawItemAlpDx[index - 1] - offsetX;
            }
        }
        else
        {
            // neni co dat na druhy radek
            *out2Len = 0;
            *out2Width = 0;
        }
    }
    else
    {
        // prvni radek bude obsahovat vse
        *out1Len = min(*out1Len, textLen);
        *out1Width = sz.cx;
        memmove(out1, text, *out1Len);

        // druhy radek bude prazdny
        *out2Len = 0;
        *out2Width = 0;
    }
    // maximalni sirka
    *maxWidth = max(*out1Width, *out2Width);
}

//****************************************************************************
//
// DrawIconThumbnailItem
//
// Kresli polozku v rezimech Icons a Thumbnails
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

    BOOL isItemUpDir = FALSE; // je vykreslovana polozka adresar ".."?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" muze byt jen prvni
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawingLarge itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // kreslime kurzor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // prepnuto v command-line
            isItemFocusedOrEditMode = TRUE;
    }

    RECT rect = *itemRect; // pozice polozky posunuta o xOffset
                           //  rect.left -= xOffset;
                           //  rect.right -= xOffset;

    HDC hDC;
    /*
  int cacheValidWidth; // ma vyznam pouze pokud jedeme pres cache a udava pocet bodu (sirku),
                       // ktere jsou v bitmape vykresleny a je treba je prenest do obrazovky
  if (drawFlags & DRAWFLAG_SELFOC_CHANGE)
  {
//    TRACE_I("drawing index="<<itemIndex);
    // pokud jde o zmenu pozice kurzoru, nebudeme provadet zadne testy viditelnosti
    drawFlags |= DRAWFLAG_SKIP_VISTEST;

    // jedna se pouze o kresleni male plochy a casu je dost - pojedeme pres cache
    hDC = ItemMemDC;

    // zatim mame bitmapu prazdnou
    cacheValidWidth = 0;

    // musime provest korekci promenne rect
    rect.top -= itemRect->top;
    rect.bottom -= itemRect->top;
    rect.left -= itemRect->left;
    rect.right -= itemRect->left;
  }
  else
*/
    hDC = hTgtDC; // jedna se o veliky paint - peceme na cache a pujdem rovnou do videa

    // pokud mi to nezatrhli, shodim flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // zatim jsme priponu pro vykreslovanou polozku v Associations nehledali

    //*****************************************
    //
    // vykresleni ikony
    //

    if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &rect))
    {
        // velikost ikony/thumbnailu
        int iconW = IconSizes[iconSize];
        int iconH = IconSizes[iconSize];

        HBITMAP hScaled = NULL; // pokud je ruzny od NULL, vykresli se thumbnail; jinak ikonka
        if (GetViewMode() == vmThumbnails)
        {
            // umime thumbnaily pouze na disku
            if (Is(ptDisk) && !isDir)
            {
                int icon;
                char fileName[MAX_PATH + 4];
                memmove(fileName, f->Name, f->NameLen);
                *(DWORD*)(fileName + f->NameLen) = 0;

                if (IconCache->GetIndex(fileName, icon, NULL, NULL))
                {
                    DWORD flag = IconCache->At(icon).GetFlag();
                    if (flag == 5 || flag == 6) // o.k. || stara verze
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
                            // rekonstrukce bitmapy
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

        // vnejsi obedlnik, od ktereho mazu smerem k vnitrnimu
        RECT outerRect = rect;

        // obdelnik, ke kteremu budeme mazat
        RECT innerRect;
        BOOL thickFrame = FALSE; // pouze pro Thumbnails -- ma byt ramecek dvojnasobny?
        if (GetViewMode() == vmThumbnails)
        {
            // pro Thumbnail saha k ramecku kolem thumbnailu
            innerRect = rect;
            innerRect.left += (rect.right - rect.left - (ListBox->ThumbnailWidth + 2)) / 2;
            innerRect.right = innerRect.left + ListBox->ThumbnailWidth + 2;
            innerRect.top += 3;
            innerRect.bottom = innerRect.top + ListBox->ThumbnailHeight + 2;

            // pero pro ramecek
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
            // pro Icons saha az k ikone
            innerRect.left = iconX;
            innerRect.top = iconY;
            innerRect.right = iconX + iconW;
            innerRect.bottom = iconY + iconH;
            outerRect.bottom = innerRect.bottom;
        }

        // podmazu pozadi k ikone nebo ramecku (pro Thumbnail)
        if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkadovou barvu
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
                // vykreslim ramecek kolem thumbnailu
                Rectangle(hDC, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom);
                if (thickFrame)
                {
                    InflateRect(&innerRect, -1, -1);
                    Rectangle(hDC, innerRect.left, innerRect.top, innerRect.right, innerRect.bottom);
                }
                InflateRect(&innerRect, -1, -1);

                // podmazu prostor mezi rameckem a ikonou
                RECT iiRect;
                iiRect.left = iconX;
                iiRect.top = iconY;
                iiRect.right = iiRect.left + iconW;
                iiRect.bottom = iiRect.top + iconH;
                if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkladovou barvu
                    FillIntersectionRegion(hDC, &innerRect, &iiRect);
            }
        }

        if (GetViewMode() == vmThumbnails)
            drawFlags |= DRAWFLAG_NO_STATE;

        if (hScaled == NULL)
        {
            // nemame zmenseninu -> vykreslime ikonu
            DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                     iconX, iconY, iconSize, (GetViewMode() == vmThumbnails ? &overlayRect : NULL),
                     drawFlags);
        }
        else
        {
            // vykreslime zmenseninu
            HDC hTmpDC = HANDLES(CreateCompatibleDC(hDC));
            HBITMAP hOldBitmap = (HBITMAP)SelectObject(hTmpDC, hScaled);

            if ((drawFlags & DRAWFLAG_MASK) != 0)
            {
                // vykreslime masku pro overlay (pri kresleni masky je potreba napred nakreslit overlay, protoze ten neumime kreslit transparentne)
                DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                         iconX, iconY, iconSize, (GetViewMode() == vmThumbnails ? &overlayRect : NULL),
                         drawFlags | DRAWFLAG_OVERLAY_ONLY);
            }

            // vykreslime vlastni bitmapu
            BitBlt(hDC, iconX, iconY, iconW, iconH, hTmpDC, 0, 0,
                   (drawFlags & DRAWFLAG_MASK) == 0 ? SRCCOPY : SRCERASE);

            if ((drawFlags & DRAWFLAG_MASK) == 0)
            {
                // vykreslime overlay (pri normalnim kresleni musime overlay vykreslit az po bitmape)
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
    // vykresleni textu
    //

    if (!(drawFlags & DRAWFLAG_ICON_ONLY))
    {
        // zvolime patricny font
        SelectObject(hDC, Font);

        // budeme pozdeji zobrazovat ramecek kolem polozky?
        BOOL drawFocusFrame = (itemIndex == DropTargetIndex || isItemFocusedOrEditMode) &&
                              (drawFlags & DRAWFLAG_NO_FRAME) == 0;

        // detekce barev
        CHighlightMasksItem* highlightMasksItem = MainWindow->HighlightMasks->AgreeMasks(f->Name, isDir ? NULL : f->Ext, f->Attr);

        // nastavim pouzity font, barvu pozadi a barvu textu
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); //chceme cerny text na cernem pozadi, kvuli aliasovanym fontum
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text i pozadi
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        int nameLen = f->NameLen;
        int itemWidth = rect.right - rect.left; // sirka polozka

        // uhladime nazev do userem definovaneho tvaru
        AlterFileName(TransferBuffer, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

        // maximalni sirka, kterou dame textu k dispozici
        int maxWidth = itemWidth - 4 - 1; // -1, aby se nedotykaly
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

        // vnejsi obdelnik, od ktereho mazu smerem k vnitrnimu
        int y; // pozice textu
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

        // vnitrni obdelnik, ke kterememu mazu
        RECT r;
        r.left = rect.left + (itemWidth - maxWidth) / 2 - 2;
        r.top = y - 2;
        r.right = r.left + maxWidth + 4;
        r.bottom = y + FontCharHeight + 2;
        if (out2Len > 0)
            r.bottom += FontCharHeight;

        // podmazu prostor kolem textu
        if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkadovou barvu
            FillIntersectionRegion(hDC, &outerRect, &r);

        if (drawFocusFrame) // pokud nebude frame, zmensime se o nej
            InflateRect(&r, -1, -1);

        int oldRTop = r.top;

        // zobrazime centrovany prvni radek; podmazeme zaroven i pro druhy radek
        // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
        ExtTextOut(hDC, rect.left + (itemWidth - out1Width) / 2, y,
                   ETO_OPAQUE, &r, out1, (drawFlags & DRAWFLAG_MASK) ? 0 : out1Len, NULL);

        // zobrazime centrovany druhy radek
        if (out2Len > 0)
        {
            // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
            ExtTextOut(hDC, rect.left + (itemWidth - out2Width) / 2, y += FontCharHeight,
                       0, NULL, out2, (drawFlags & DRAWFLAG_MASK) ? 0 : out2Len, NULL);
        }

        //*****************************************
        //
        // vykresleni focus ramecku
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
// Kresli polozku v rezimu Tiles
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
        // vyhledam od konce znak, za ktery muzu nakopirovat "..." a vejdou se do sloupce
        while (fitChars > 0 && DrawItemAlpDx[fitChars - 1] + TextEllipsisWidth > maxTextWidth)
            fitChars--;
        // do jineho bufferu nakopcim cast puvodniho retezce
        if (fitChars > 0)
        {
            // a pripojim "..."
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
    // uhladime nazev do userem definovaneho tvaru
    AlterFileName(out0, f->Name, -1, Configuration.FileNameFormat, 0, isDir != 0);
    // 1. radek: NAME
    *out0Len = f->NameLen;
    // retezec muze byt delsi nez dostupne misto a bude treba zkratit ho vypustkou ...
    *widthNeeded = 0;
    TruncateSringToFitWidth(hDC, out0, out0Len, maxTextWidth, widthNeeded);

    // 2. radek: SIZE (pokud zname)
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
    // retezec muze byt delsi nez dostupne misto a bude treba zkratit ho vypustkou ...
    TruncateSringToFitWidth(hDC, out1, out1Len, maxTextWidth, widthNeeded);

    // 3. radek DATE TIME (pokud zname)
    SYSTEMTIME st;
    FILETIME ft;
    BOOL validDate = FALSE;
    BOOL validTime = FALSE;
    BOOL invalidDate = FALSE;
    BOOL invalidTime = FALSE;
    if (validFileData & (VALID_DATA_DATE | VALID_DATA_TIME)) // aspon neco je v LastWrite
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
    if (!validDate) // nemame nastaveny zadny datum, nejaky nastavime...
    {
        st.wYear = 2000;
        st.wMonth = 12;
        st.wDay = 24;
        st.wDayOfWeek = 0; // nedele
    }
    if (!validTime) // nemame nastaveny zadny cas, nejaky nastavime...
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
    // retezec muze byt delsi nez dostupne misto a bude treba zkratit ho vypustkou ...
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

    BOOL isItemUpDir = FALSE; // je vykreslovana polozka adresar ".."?

    if (itemIndex == 0 && isDir && *f->Name == '.' && *(f->Name + 1) == '.' &&
        *(f->Name + 2) == 0) // "up-dir" muze byt jen prvni
    {
        if (drawFlags & DRAWFLAG_DIRTY_ONLY)
            return;
        isItemUpDir = TRUE;
    }

    //  TRACE_I("DrawTileItem itemIndex="<<dec<<itemIndex<<" y="<<itemRect->top);

    BOOL isItemFocusedOrEditMode = FALSE;
    if (FocusedIndex == itemIndex) // kreslime kurzor
    {
        if (FocusVisible || Parent->EditMode && Parent->GetActivePanel() == this) // prepnuto v command-line
            isItemFocusedOrEditMode = TRUE;
    }

    RECT rect = *itemRect; // pozice polozky posunuta o xOffset

    HDC hDC = hTgtDC; // jedna se o veliky paint - peceme na cache a pujdem rovnou do videa

    // pokud mi to nezatrhli, shodim flag
    if (!(drawFlags & DRAWFLAG_KEEP_DIRTY))
        f->Dirty = 0;

    TransferAssocIndex = -2; // zatim jsme priponu pro vykreslovanou polozku v Associations nehledali

    //*****************************************
    //
    // vykresleni ikony
    //

    if (drawFlags & DRAWFLAG_SKIP_VISTEST || RectVisible(hDC, &rect))
    {
        // velikost ikony
        int iconW = IconSizes[iconSize];
        int iconH = IconSizes[iconSize];

        // umisteni ikonky
        int iconX = rect.left + TILE_LEFT_MARGIN;
        int iconY = rect.top + (rect.bottom - rect.top - iconH) / 2; // centrujeme

        // vnejsi obdelnik, od ktereho mazu smerem k vnitrnimu
        RECT outerRect = rect;
        outerRect.right = iconX + iconW;

        // obdelnik, ke kteremu budeme mazat
        RECT innerRect;
        BOOL thickFrame = FALSE; // pouze pro Thumbnails -- ma byt ramecek dvojnasobny?

        innerRect.left = iconX;
        innerRect.top = iconY;
        innerRect.right = iconX + iconW;
        innerRect.bottom = iconY + iconH;

        // podmazu pozadi k ikone nebo ramecku (pro Thumbnail)
        if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkadovou barvu
            FillIntersectionRegion(hDC, &outerRect, &innerRect);

        // nemame zmenseninu -> vykreslime ikonu
        DrawIcon(hDC, f, isDir, isItemUpDir, isItemFocusedOrEditMode,
                 iconX, iconY, iconSize, NULL, drawFlags);
    }

    //*****************************************
    //
    // vykresleni textu
    //

    if (!(drawFlags & DRAWFLAG_ICON_ONLY))
    {
        // zvolime patricny font
        SelectObject(hDC, Font);

        // budeme pozdeji zobrazovat ramecek kolem polozky?
        BOOL drawFocusFrame = (itemIndex == DropTargetIndex || isItemFocusedOrEditMode) &&
                              (drawFlags & DRAWFLAG_NO_FRAME) == 0;

        // detekce barev
        CHighlightMasksItem* highlightMasksItem = MainWindow->HighlightMasks->AgreeMasks(f->Name, isDir ? NULL : f->Ext, f->Attr);

        // nastavim pouzity font, barvu pozadi a barvu textu
        SetFontAndColors(hDC, highlightMasksItem, f, isItemFocusedOrEditMode, itemIndex);

        if (drawFlags & DRAWFLAG_MASK)
        {
            SetTextColor(hDC, RGB(0, 0, 0)); //chceme cerny text na cernem pozadi, kvuli aliasovanym fontum
            SetBkColor(hDC, RGB(0, 0, 0));
        }
        if (drawFlags & DRAWFLAG_DRAGDROP)
        {
            SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED])); // focused text i pozadi
            SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        }

        int nameLen = f->NameLen;
        int itemWidth = rect.right - rect.left; // sirka polozka

        // texty nesmi prekrocit tuto delku v bodech
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

        // podmazu prostor kolem textu
        if ((drawFlags & DRAWFLAG_MASK) == 0) // pokud kreslime masku (b&w), nesmi kreslit podkadovou barvu
            FillIntersectionRegion(hDC, &outerRect, &r);

        //    if (drawFocusFrame) // pokud nebude frame, zmensime se o nej
        //      InflateRect(&r, -1, -1);

        // zobrazime prvni radek; podmazeme take prostor nad nim
        r.bottom = textY + FontCharHeight;
        if (out1[0] == 0 && out2[0] == 0) // pokud je to posleni radek, podmazeme prostor pod nim
        {
            r.bottom += 2;
            if (drawFocusFrame)
                r.bottom--;
        }
        // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
        ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out0, (drawFlags & DRAWFLAG_MASK) ? 0 : out0Len, NULL);

        // zobrazime druhy radek
        if (out1[0] != 0)
        {
            textY += FontCharHeight;
            r.top = r.bottom;
            r.bottom = r.top + FontCharHeight;
            if (out2[0] == 0) // pokud je to posleni radek, podmazeme prostor pod nim
            {
                r.bottom += 2;
                if (drawFocusFrame)
                    r.bottom--;
            }
            // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
            ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out1, (drawFlags & DRAWFLAG_MASK) ? 0 : out1Len, NULL);
        }
        // zobrazime treti radek; podmazeme take prostor pod nim
        if (out2[0] != 0)
        {
            r.top = r.bottom;
            r.bottom = r.top + FontCharHeight + 2;
            if (drawFocusFrame)
                r.bottom--;
            textY += FontCharHeight;
            // DRAWFLAG_MASK: hack, pod XP se do masky pri kresleni kratkych textu pridavala pred text nejaka blitka, pokud text nekreslimne, nedela to
            ExtTextOut(hDC, textX, textY, ETO_OPAQUE, &r, out2, (drawFlags & DRAWFLAG_MASK) ? 0 : out2Len, NULL);
        }

        //*****************************************
        //
        // vykresleni focus ramecku
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

    // overlay muze (v pripade thumbnailu) lezet mimo ikonku (v levem dolnim rohu thubnailu)
    int xOverlayDst = xDst;
    int yOverlayDst = yDst;

    // na Viste se pouziva pro ikony 48x48 overlay ICONSIZE_32 a pro thumbnaily overlay ICONSIZE_48
    if (iconSize == ICONSIZE_48 && overlayRect == NULL)
    {
        iconSize = ICONSIZE_32;
        yOverlayDst += 48 - 32;
    }

    int iconW = IconSizes[iconSize];
    int iconH = IconSizes[iconSize];

    // v pripade thumbnailu dostaneme overlayRect != NULL a posuneme overlay do jeho leveho dolniho rohu
    if (overlayRect != NULL)
    {
        xOverlayDst = overlayRect->left;
        yOverlayDst = overlayRect->bottom - iconH;
    }

    if (state & IMAGE_STATE_MASK)
    {
        // musim prohodit poradi, protoze funkce DrawIconEx nekresli masku transparentne
        if (iconOverlayIndex != ICONOVERLAYINDEX_NOTUSED) // pokud je nacteny tento overlay, znamena to, ze je prioritnejsi nez nize uvedene overlaye
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
        if (iconOverlayIndex != ICONOVERLAYINDEX_NOTUSED) // pokud je nacteny tento overlay, znamena to, ze je prioritnejsi nez nize uvedene overlaye
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
