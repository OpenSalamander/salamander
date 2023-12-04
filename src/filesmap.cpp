// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "snooper.h"
#include "worker.h"
#include "gui.h"

void CFilesWindow::DragEnter()
{
    ScrollObject.BeginScroll(TRUE);
}

void CFilesWindow::DragLeave()
{
    ScrollObject.EndScroll();
}

//****************************************************************************
//
// CFilesMap
//

CFilesMap::CFilesMap()
{
    Panel = NULL;
    Map = NULL;
    DestroyMap();
}

CFilesMap::~CFilesMap()
{
    Map = NULL;
    DestroyMap();
}

BOOL CFilesMap::CreateMap()
{
    CALL_STACK_MESSAGE1("CFilesMap::CreateMap()");
    if (Panel == NULL)
    {
        TRACE_E("CFilesMap::CreateMap() Panel == NULL");
        return FALSE;
    }

    if (Map != NULL)
    {
        TRACE_E("CFilesMap::CreateMap() Map != NULL");
        DestroyMap();
    }

    CViewModeEnum panelMode = Panel->GetViewMode();
    int dirsCount = Panel->Dirs->Count;
    int count = dirsCount + Panel->Files->Count;
    int columns = Panel->ListBox->ColumnsCount;
    int rows;
    switch (panelMode)
    {
    case vmDetailed:
    {
        rows = Panel->ListBox->ItemsCount;
        break;
    }

    case vmBrief:
    {
        rows = Panel->ListBox->EntireItemsInColumn;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        rows = (Panel->ListBox->ItemsCount + Panel->ListBox->ColumnsCount - 1) /
               Panel->ListBox->ColumnsCount;
        break;
    }
    }

    MapItemsCount = 0;
    Map = (CFilesMapItem*)malloc(rows * columns * sizeof(CFilesMapItem));
    if (Map == NULL)
    {
        TRACE_E(LOW_MEMORY);
        DestroyMap();
        return FALSE;
    }
    Rows = rows;
    Columns = columns;

    // projedu adresare a soubory a nastavim odpovidajici CFilesMapItem
    CFilesMapItem* itemIter = Map;

    if (panelMode == vmBrief || panelMode == vmDetailed)
    {
        // Brief || Detailed
        // polozky budou ulozeny shora dolu a pak zleve doprava (pro Brief)
        char formatedFileName[MAX_PATH];
        HDC dc = HANDLES(GetDC(Panel->GetListBoxHWND()));
        HFONT hOldFont = (HFONT)SelectObject(dc, Font);
        int width = Panel->ListBox->GetItemWidth();
        int i;
        for (i = 0; i < count; i++)
        {
            BOOL isDir = i < dirsCount;
            CFileData* f = isDir ? &Panel->Dirs->At(i) : &Panel->Files->At(i - dirsCount);
            {
                if (!Configuration.FullRowSelect)
                {
                    AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

                    const char* s = formatedFileName;
                    // potlacime ".."
                    if (*s == '.' && *(s + 1) == '.' && *(s + 2) == 0)
                        s = NULL;

                    width = IconSizes[ICONSIZE_16] + 2;

                    int len;
                    if (s != NULL)
                    {
                        if ((!isDir || Configuration.SortDirsByExt) && Panel->GetViewMode() == vmDetailed &&
                            Panel->IsExtensionInSeparateColumn() && f->Ext[0] != 0 && f->Ext > f->Name + 1) // vyjimka pro jmena jako ".htaccess", ukazuji se ve sloupci Name i kdyz jde o pripony
                        {
                            len = (int)(f->Ext - f->Name - 1);
                        }
                        else
                            len = f->NameLen;
                    }
                    else
                    {
                        len = 0;
                    }

                    // zjistim skutecnou delku textu
                    SIZE sz;
                    GetTextExtentPoint32(dc, s, len, &sz);
                    width += sz.cx + 4;

                    if (Panel->GetViewMode() == vmDetailed && width > (int)Panel->Columns[0].Width - 1)
                        width = Panel->Columns[0].Width - 1;
                }
                if (Configuration.FullRowSelect && Panel->GetViewMode() == vmBrief)
                {
                    width = Panel->Columns[0].Width - 10; // korekce
                }
            }

            itemIter->Width = width;
            if (Configuration.FullRowSelect)
                itemIter->Width--;
            itemIter->FileData = f;
            itemIter->Selected = f->Selected;

            itemIter++;
            MapItemsCount++;
        }
        SelectObject(dc, hOldFont);
        HANDLES(ReleaseDC(Panel->GetListBoxHWND(), dc));
    }
    else
    {
        // Icons || Thumbnails
        // polozky budou ulozeny zleva doprava a pak shora dolu
        int width = Panel->ListBox->GetItemWidth();
        int i;
        for (i = 0; i < count; i++)
        {
            CFileData* f = (i < dirsCount) ? &Panel->Dirs->At(i) : &Panel->Files->At(i - dirsCount);
            itemIter->Width = width;
            itemIter->FileData = f;
            itemIter->Selected = f->Selected;

            itemIter++;
            MapItemsCount++;
        }
    }
    return TRUE;
}

void CFilesMap::DestroyMap()
{
    if (Map != NULL)
    {
        free(Map);
        Map = NULL;
    }
    MapItemsCount = 0;
    Rows = 0;
    Columns = 0;
    AnchorX = 0;
    AnchorY = 0;
    PointX = 0;
    PointY = 0;
}

void CFilesMap::SetAnchor(int x, int y)
{
    CALL_STACK_MESSAGE3("CFilesMap::SetAnchor(%d, %d)", x, y);
    if (Panel == NULL)
    {
        TRACE_E("CFilesMap::SetAnchor() Panel == NULL");
        return;
    }

    if (x < Panel->ListBox->FilesRect.left)
        x = Panel->ListBox->FilesRect.left;
    else if (x > Panel->ListBox->FilesRect.right)
        x = Panel->ListBox->FilesRect.right;
    if (y < Panel->ListBox->FilesRect.top)
        y = Panel->ListBox->FilesRect.top;
    else if (y > Panel->ListBox->FilesRect.bottom)
        y = Panel->ListBox->FilesRect.bottom;
    // prevedu x a y na absolutni hodnoty
    switch (Panel->GetViewMode())
    {
    case vmBrief:
    {
        x += Panel->ListBox->GetItemWidth() * GetScrollPos(Panel->ListBox->HHScrollBar, SB_CTL);
        break;
    }

    case vmDetailed:
    {
        x += Panel->ListBox->XOffset;
        y += Panel->ListBox->GetItemHeight() * Panel->ListBox->TopIndex;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        y += Panel->ListBox->TopIndex;
        break;
    }
    }

    BOOL updateNeeded = FALSE;
    if (!(GetKeyState(VK_SHIFT) & 0x8000) && !(GetKeyState(VK_CONTROL) & 0x8000))
    {
        updateNeeded = (Panel->GetSelCount() != 0);
        int count = Panel->Dirs->Count + Panel->Files->Count;
        int i;
        for (i = 0; i < count; i++)
            Map[i].Selected = 0;
    }
    PointX = AnchorX = x;
    PointY = AnchorY = y;
    if (updateNeeded)
        UpdatePanel();
}

CFilesMapItem*
CFilesMap::GetMapItem(int column, int row)
{
    CViewModeEnum panelMode = Panel->GetViewMode();
    int index;
    if (panelMode == vmBrief || panelMode == vmDetailed)
        index = column * Rows + row; // polozky jsou shora dolu a pak zleva doprava
    else
        index = row * Columns + column; // polozky jsou zleva doprava a pak shora dolu
    if (index >= MapItemsCount)
    {
        TRACE_E("Access to index out of array bounds.");
        return NULL;
    }
    return Map + index;
}

void CFilesMap::SetPoint(int x, int y)
{
    CALL_STACK_MESSAGE3("CFilesMap::SetPoint(%d, %d)", x, y);
    if (Panel == NULL)
    {
        TRACE_E("CFilesMap::SetAnchor() Panel == NULL");
        return;
    }

    if (x < Panel->ListBox->FilesRect.left)
        x = Panel->ListBox->FilesRect.left;
    else if (x > Panel->ListBox->FilesRect.right)
        x = Panel->ListBox->FilesRect.right;
    if (y < Panel->ListBox->FilesRect.top)
        y = Panel->ListBox->FilesRect.top;
    else if (y > Panel->ListBox->FilesRect.bottom)
        y = Panel->ListBox->FilesRect.bottom;
    // prevedu x a y na absolutni hodnoty
    switch (Panel->GetViewMode())
    {
    case vmBrief:
    {
        x += Panel->ListBox->ItemWidth * GetScrollPos(Panel->ListBox->HHScrollBar, SB_CTL);
        break;
    }

    case vmDetailed:
    {
        x += Panel->ListBox->XOffset;
        y += Panel->ListBox->GetItemHeight() * Panel->ListBox->TopIndex;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        y += Panel->ListBox->TopIndex;
        break;
    }
    }

    //  BOOL shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    BOOL ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    int oldRectLeft, oldRectRight, oldRectTop, oldRectBottom;
    if (PointX < AnchorX)
    {
        oldRectLeft = PointX;
        oldRectRight = AnchorX;
    }
    else
    {
        oldRectLeft = AnchorX;
        oldRectRight = PointX;
    }
    if (PointY < AnchorY)
    {
        oldRectTop = PointY;
        oldRectBottom = AnchorY;
    }
    else
    {
        oldRectTop = AnchorY;
        oldRectBottom = PointY;
    }

    int newRectLeft, newRectRight, newRectTop, newRectBottom;
    if (x < AnchorX)
    {
        newRectLeft = x;
        newRectRight = AnchorX;
    }
    else
    {
        newRectLeft = AnchorX;
        newRectRight = x;
    }
    if (y < AnchorY)
    {
        newRectTop = y;
        newRectBottom = AnchorY;
    }
    else
    {
        newRectTop = AnchorY;
        newRectBottom = y;
    }

    RECT oldRect;
    RECT newRect;
    GetCROfRect(PointX, PointY, oldRect);
    GetCROfRect(x, y, newRect);
    RECT rect;

    // najdu obepsany obdelnik
    if (oldRect.left < newRect.left)
        rect.left = oldRect.left;
    else
        rect.left = newRect.left;

    if (oldRect.right > newRect.right)
        rect.right = oldRect.right;
    else
        rect.right = newRect.right;

    if (oldRect.top < newRect.top)
        rect.top = oldRect.top;
    else
        rect.top = newRect.top;

    if (oldRect.bottom > newRect.bottom)
        rect.bottom = oldRect.bottom;
    else
        rect.bottom = newRect.bottom;

    // prohledam ho
    CViewModeEnum panelMode = Panel->GetViewMode();
    int row, col;
    CFilesMapItem* item;
    int count = Panel->Dirs->Count + Panel->Files->Count;
    BOOL changed = FALSE;
    for (row = rect.top; row <= rect.bottom; row++)
    {
        for (col = rect.left; col <= rect.right; col++)
        {
            // osetrim hranici pole
            if (panelMode == vmBrief || panelMode == vmDetailed)
            {
                // Brief || Detailed
                if (Rows * col + row >= count || row >= Rows)
                    continue;
            }
            else
            {
                // Icons || Thumbnails
                if (Columns * row + col >= count || col >= Columns)
                    continue;
            }
            item = GetMapItem(col, row);

            if (item == NULL) // meli jsme nekolik padacek v CFilesMap::SetPoint; vznik neznamy
                continue;     // takhle nepadneme, maximalne nebude chodit selection

            BOOL inOld = PointInRect(col, row, oldRect);
            if (inOld)
            {
                if (panelMode == vmBrief || panelMode == vmDetailed)
                {
                    int mx = col * Panel->ListBox->ItemWidth;
                    int mw = item->Width;
                    if (oldRectLeft > mx + mw)
                        inOld = FALSE;
                }
                else if (panelMode == vmIcons || panelMode == vmThumbnails)
                {
                    int mw;
                    int mh;
                    if (panelMode == vmThumbnails)
                    {
                        mw = Panel->ListBox->ThumbnailWidth + 2;
                        mh = Panel->ListBox->ThumbnailHeight + 2;
                    }
                    else
                    {
                        mw = 32;
                        mh = 32;
                    }
                    int mx = col * Panel->ListBox->ItemWidth;
                    mx += (Panel->ListBox->ItemWidth - mw) / 2;
                    int my = row * Panel->ListBox->ItemHeight + 2;
                    if (oldRectRight < mx || oldRectLeft > mx + mw ||
                        oldRectBottom < my || oldRectTop > my + mh)
                        inOld = FALSE;
                }
                else
                {
                    int mw = IconSizes[ICONSIZE_48];
                    int mh = IconSizes[ICONSIZE_48];
                    int mx = col * Panel->ListBox->ItemWidth + 4;
                    int my = row * Panel->ListBox->ItemHeight + 4;
                    if (oldRectRight < mx || oldRectLeft > mx + mw ||
                        oldRectBottom < my || oldRectTop > my + mh)
                        inOld = FALSE;
                }
            }
            BOOL inNew = PointInRect(col, row, newRect);
            if (inNew)
            {
                if (panelMode == vmBrief || panelMode == vmDetailed)
                {
                    int mx = col * Panel->ListBox->ItemWidth;
                    int mw = item->Width;
                    if (newRectLeft > mx + mw)
                        inNew = FALSE;
                }
                else if (panelMode == vmIcons || panelMode == vmThumbnails)
                {
                    int mw;
                    int mh;
                    if (panelMode == vmThumbnails)
                    {
                        mw = Panel->ListBox->ThumbnailWidth + 2;
                        mh = Panel->ListBox->ThumbnailHeight + 2;
                    }
                    else
                    {
                        mw = 32;
                        mh = 32;
                    }
                    int mx = col * Panel->ListBox->ItemWidth;
                    mx += (Panel->ListBox->ItemWidth - mw) / 2;
                    int my = row * Panel->ListBox->ItemHeight + 2;
                    if (newRectRight < mx || newRectLeft > mx + mw ||
                        newRectBottom < my || newRectTop > my + mh)
                        inNew = FALSE;
                }
                else
                {
                    int mw = IconSizes[ICONSIZE_48];
                    int mh = IconSizes[ICONSIZE_48];
                    int mx = col * Panel->ListBox->ItemWidth + 4;
                    int my = row * Panel->ListBox->ItemHeight + 4;
                    if (newRectRight < mx || newRectLeft > mx + mw ||
                        newRectBottom < my || newRectTop > my + mh)
                        inNew = FALSE;
                }
            }

            if (ctrl)
            {
                if (!inOld && inNew)
                {
                    item->Selected = 1 - item->Selected;
                    changed = TRUE;
                }
            }
            else
            {
                if (item->Selected == 0 && !inOld && inNew)
                {
                    item->Selected = 1;
                    changed = TRUE;
                }
            }

            if (ctrl)
            {
                if (inOld && !inNew)
                {
                    item->Selected = 1 - item->Selected;
                    changed = TRUE;
                }
            }
            else
            {
                if (item->Selected == 1 && inOld && !inNew)
                {
                    item->Selected = 0;
                    changed = TRUE;
                }
            }
        }
    }

    PointX = x;
    PointY = y;
    if (changed)
        UpdatePanel();
}

void CFilesMap::GetCROfRect(int x, int y, RECT& r)
{
    CALL_STACK_MESSAGE3("CFilesMap::GetCROfRect(%d, %d)", x, y);
    int anchorCol, anchorRow;
    int col, row;

    GetCROfPoint(AnchorX, AnchorY, anchorCol, anchorRow);
    GetCROfPoint(x, y, col, row);

    if (anchorCol < col)
    {
        r.left = anchorCol;
        r.right = col;
    }
    else
    {
        r.left = col;
        r.right = anchorCol;
    }

    if (anchorRow < row)
    {
        r.top = anchorRow;
        r.bottom = row;
    }
    else
    {
        r.top = row;
        r.bottom = anchorRow;
    }
}

void CFilesMap::GetCROfPoint(int x, int y, int& column, int& row)
{
    CALL_STACK_MESSAGE3("CFilesMap::GetCROfPoint(%d, %d, )", x, y);
    row = (y - Panel->ListBox->FilesRect.top) / Panel->ListBox->ItemHeight;
    if (Panel->GetViewMode() == vmDetailed)
        column = 0;
    else
        column = x / Panel->ListBox->ItemWidth;
}

void CFilesMap::UpdatePanel()
{
    CALL_STACK_MESSAGE1("CFilesMap::UpdatePanel()");
    int dirsCount = Panel->Dirs->Count;
    int count = dirsCount + Panel->Files->Count;
    int start; // od jakeho indexu zacit zjistovat oznaceni (preskoceni "..")
    if (Panel->Dirs->Count > 0 && strcmp(Panel->Dirs->At(0).Name, "..") == 0)
        start = 1;
    else
        start = 0;
    // nastavim prislusne selection bity
    Panel->SelectedCount = 0;

    int i;
    for (i = start; i < count; i++)
    {
        CFileData* item = (i < dirsCount) ? &Panel->Dirs->At(i) : &Panel->Files->At(i - dirsCount);
        BYTE sel = Map[i].Selected;
        if (item->Selected != sel)
        {
            item->Dirty = 1;
            item->Selected = sel;
        }
        Panel->SelectedCount += sel;
    }
    Panel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(Panel->HWindow, WM_USER_SELCHANGED, 0, 0);
}

void CFilesMap::DrawDragBox(POINT p)
{
    if (p.x < Panel->ListBox->FilesRect.left)
        p.x = Panel->ListBox->FilesRect.left;
    else if (p.x > Panel->ListBox->FilesRect.right)
        p.x = Panel->ListBox->FilesRect.right;

    if (p.y < Panel->ListBox->FilesRect.top)
        p.y = Panel->ListBox->FilesRect.top;
    else if (p.y > Panel->ListBox->FilesRect.bottom)
        p.y = Panel->ListBox->FilesRect.bottom;

    HDC hdc = HANDLES(GetDC(Panel->ListBox->HWindow));
    RECT r;

    int anchorX = AnchorX;
    int anchorY = AnchorY;

    CViewModeEnum panelMode = Panel->GetViewMode();
    switch (panelMode)
    {
    case vmBrief:
    {
        anchorX -= Panel->ListBox->ItemWidth * GetScrollPos(Panel->ListBox->HHScrollBar, SB_CTL);
        break;
    }

    case vmDetailed:
    {
        anchorX -= Panel->ListBox->XOffset;
        anchorY -= Panel->ListBox->GetItemHeight() * Panel->ListBox->TopIndex;
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        anchorY -= Panel->ListBox->TopIndex;
        break;
    }
    }

    r.left = anchorX;
    r.top = anchorY;
    r.right = p.x;
    r.bottom = p.y;
    if (r.left > r.right)
    {
        r.left = r.right;
        r.right = anchorX;
    }
    if (r.top > r.bottom)
    {
        r.top = r.bottom;
        r.bottom = anchorY;
    }
    int oldColor = SetTextColor(hdc, RGB(255, 255, 255));
    int oldBkColor = SetBkColor(hdc, RGB(0, 0, 0));
    DrawFocusRect(hdc, &r);
    SetBkColor(hdc, oldBkColor);
    SetTextColor(hdc, oldColor);
    HANDLES(ReleaseDC(Panel->ListBox->HWindow, hdc));
}

//****************************************************************************
//
// CScrollPanel
//

CScrollPanel::CScrollPanel()
{
    Panel = NULL;
    ExistTimer = FALSE;
    ScrollInside = FALSE;
}

CScrollPanel::~CScrollPanel()
{
    EndScroll();
}

BOOL CScrollPanel::BeginScroll(BOOL scrollInside)
{
    if (Panel == NULL)
    {
        TRACE_E("CScrollPanel::BeginScroll() Panel == NULL");
        return FALSE;
    }

    ScrollInside = scrollInside;
    BeginScrollCounter = 0;
    if (ScrollInside)
        GetCursorPos(&LastMousePoint);
    ExistTimer = SetTimer(Panel->ListBox->HWindow, IDT_PANELSCROLL,
                          Panel->GetViewMode() == vmBrief ? 200 : 100, NULL) != 0;
    if (ExistTimer)
        OnWMTimer();
    return ExistTimer;
}

void CScrollPanel::EndScroll()
{
    if (ExistTimer && Panel != NULL && Panel->ListBox != NULL)
    {
        KillTimer(Panel->ListBox->HWindow, IDT_PANELSCROLL);
        ExistTimer = FALSE;
    }
    ScrollInside = FALSE;
}

void CScrollPanel::OnWMTimer()
{
    POINT p;
    GetCursorPos(&p);

    if (ScrollInside && LastMousePoint.x != p.x && LastMousePoint.y != p.y)
    {
        LastMousePoint = p;
        BeginScrollCounter = 0;
        return;
    }
    if (BeginScrollCounter < 1)
    {
        BeginScrollCounter++;
        return;
    }

    ScreenToClient(Panel->ListBox->HWindow, &p);
    int x = p.x;
    int y = p.y;

    int xDelta = 0;
    int yDelta = 0;

    RECT filesRect = Panel->ListBox->FilesRect;
    if (ScrollInside)
    {
        if (x >= filesRect.left && x < filesRect.right &&
            y >= filesRect.top && y < filesRect.bottom)
        {
            int border = 35;

            if (x < filesRect.left + border)
                xDelta = -1;
            else if (x > filesRect.right - border)
                xDelta = 1;

            if (Panel->GetViewMode() == vmDetailed)
                xDelta *= (border / 2);

            if (y < filesRect.top + border)
                yDelta = -1;
            else if (y > filesRect.bottom - border)
                yDelta = 1;

            if (Panel->GetViewMode() == vmIcons ||
                Panel->GetViewMode() == vmThumbnails ||
                Panel->GetViewMode() == vmTiles)
                yDelta *= Panel->ListBox->ItemHeight / 4;
        }
    }
    else
    {
        if (x < filesRect.left)
            xDelta = x - filesRect.left;
        else if (x > filesRect.right)
            xDelta = x - filesRect.right;

        if (y < filesRect.top)
            yDelta = y - filesRect.top;
        else if (y > filesRect.bottom)
            yDelta = y - filesRect.bottom;
    }

    if (xDelta == 0 && yDelta == 0)
        return;

    BOOL showDragBox = FALSE;
    if (Panel->DragBox && Panel->DragBoxVisible)
    {
        Panel->DrawDragBox(Panel->OldBoxPoint); // zhasnem box
        showDragBox = TRUE;
    }

    BOOL oldScrolling = Panel->ScrollingWindow;
    Panel->ScrollingWindow = TRUE;
    switch (Panel->GetViewMode())
    {
    case vmBrief:
    {
        // Brief
        if (xDelta != 0)
        {
            int leftColumn = Panel->ListBox->TopIndex / Panel->ListBox->EntireItemsInColumn;
            int myDelta = 1;
            if (abs(xDelta) > 20)
                myDelta = 2;
            if (abs(xDelta) > 50)
                myDelta = 3;
            int nPos = leftColumn + (xDelta < 0 ? -myDelta : myDelta);
            Panel->ListBox->OnHScroll(SB_THUMBPOSITION, nPos);
        }
        break;
    }

    case vmDetailed:
    {
        // Detailed
        if (xDelta != 0)
        {
            int mX;
            mX = xDelta;
            int nPos = Panel->ListBox->XOffset + mX;
            Panel->ListBox->OnHScroll(SB_THUMBPOSITION, nPos);
        }

        if (yDelta != 0)
        {
            int mY;
            mY = yDelta / 10;
            if (mY == 0)
                mY = yDelta < 0 ? -1 : 1;

            int nPos = Panel->ListBox->TopIndex + mY;
            Panel->ListBox->OnVScroll(SB_THUMBPOSITION, nPos);
        }
        break;
    }

    case vmIcons:
    case vmThumbnails:
    case vmTiles:
    {
        // Icons || Thumbnails
        if (yDelta != 0)
        {
            int mY;
            mY = yDelta * 2;
            if (mY == 0)
                mY = yDelta < 0 ? -1 : 1;

            int nPos = Panel->ListBox->TopIndex + mY;
            Panel->ListBox->OnVScroll(SB_THUMBPOSITION, nPos);
        }
        break;
    }
    }
    Panel->ScrollingWindow = oldScrolling;
    if (showDragBox)
        Panel->DrawDragBox(Panel->OldBoxPoint); // zase ho nahodime

    if (!ScrollInside)
        PostMessage(Panel->ListBox->HWindow, WM_MOUSEMOVE, MK_LBUTTON | MK_RBUTTON, MAKEWPARAM(x, y));
}
