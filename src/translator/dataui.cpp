// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtree.h"
#include "wndtext.h"
#include "wndprev.h"
#include "datarh.h"

void CData::FillTree()
{
    HWND hTree = TreeWindow.GetTreeView();

    SendMessage(hTree, WM_SETREDRAW, FALSE, 0); // zakazeme zbytecne kresleni behem pridavani polozek

    TreeView_DeleteAllItems(hTree);

    HTREEITEM hDlg = NULL;
    HTREEITEM hMenu = NULL;
    HTREEITEM hStr = NULL;

    TVINSERTSTRUCT tvis;
    tvis.hParent = NULL;
    tvis.hInsertAfter = TVI_ROOT;
    tvis.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_PARAM | TVIF_STATE;
    tvis.item.iImage = 2;
    tvis.item.iSelectedImage = 2;
    //  tvis.item.pszText = "Dialog";
    //  tvis.item.cChildren = 0;
    //  HTREEITEM hDlg = TreeView_InsertItem(hTree, &tvis);

    if (DlgData.Count > 0)
    {
        tvis.item.lParam = TREE_TYPE_NONE | TREE_TYPE_NONE_DIALOGS; // nadpis
        tvis.item.pszText = (char*)"Dialog";
        tvis.item.cChildren = 1;
        tvis.item.state = 0;
        if (Data.OpenDialogTables)
            tvis.item.state |= TVIS_EXPANDED;
        tvis.item.stateMask = tvis.item.state;
        hDlg = TreeView_InsertItem(hTree, &tvis);

        for (int i = 0; i < DlgData.Count; i++)
        {
            TVINSERTSTRUCT tvis2;
            tvis2.hParent = hDlg;
            tvis2.hInsertAfter = TVI_LAST;
            tvis2.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;

            WORD state = PROGRESS_STATE_TRANSLATED;
            for (int j = 0; j < DlgData[i]->Controls.Count; j++)
            {
                if (DlgData[i]->Controls[j]->State != PROGRESS_STATE_TRANSLATED)
                {
                    state = PROGRESS_STATE_UNTRANSLATED;
                    break;
                }
            }

            tvis2.item.iImage = (state == PROGRESS_STATE_UNTRANSLATED) ? 0 : 1;
            tvis2.item.iSelectedImage = tvis2.item.iImage;

            char buff[1000];
            lstrcpyn(buff, DataRH.GetIdentifier(DlgData[i]->ID), 1000);
            tvis2.item.pszText = buff;
            tvis2.item.cChildren = 0;
            tvis2.item.lParam = TREE_TYPE_DIALOG | DlgData[i]->ID;
            TreeView_InsertItem(hTree, &tvis2);
        }
    }

    if (MenuData.Count > 0)
    {
        tvis.item.lParam = TREE_TYPE_NONE | TREE_TYPE_NONE_MENUS; // nadpis
        tvis.item.pszText = (char*)"Menu";
        tvis.item.cChildren = 1;
        tvis.item.state = 0;
        if (Data.OpenMenuTables)
            tvis.item.state |= TVIS_EXPANDED;
        tvis.item.stateMask = tvis.item.state;
        hMenu = TreeView_InsertItem(hTree, &tvis);

        for (int i = 0; i < MenuData.Count; i++)
        {
            TVINSERTSTRUCT tvis2;
            tvis2.hParent = hMenu;
            tvis2.hInsertAfter = TVI_LAST;
            tvis2.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;

            WORD state = PROGRESS_STATE_TRANSLATED;

            for (int j = 0; j < MenuData[i]->Items.Count; j++)
            {
                if (MenuData[i]->Items[j].State != PROGRESS_STATE_TRANSLATED)
                {
                    state = PROGRESS_STATE_UNTRANSLATED;
                    break;
                }
            }

            tvis2.item.iImage = (state == PROGRESS_STATE_UNTRANSLATED) ? 0 : 1;
            tvis2.item.iSelectedImage = tvis2.item.iImage;

            char buff[1000];
            lstrcpyn(buff, DataRH.GetIdentifier(MenuData[i]->ID), 1000);
            tvis2.item.pszText = buff;
            tvis2.item.cChildren = 0;
            tvis2.item.lParam = TREE_TYPE_MENU | MenuData[i]->ID;
            TreeView_InsertItem(hTree, &tvis2);
        }
    }

    if (StrData.Count > 0)
    {
        tvis.item.lParam = TREE_TYPE_NONE | TREE_TYPE_NONE_STRINGS; // nadpis
        tvis.item.pszText = (char*)"String Table";
        tvis.item.cChildren = 1;
        tvis.item.state = 0;
        if (Data.OpenStringTables)
            tvis.item.state |= TVIS_EXPANDED;
        tvis.item.stateMask = tvis.item.state;

        hStr = TreeView_InsertItem(hTree, &tvis);

        for (int i = 0; i < StrData.Count; i++)
        {
            TVINSERTSTRUCT tvis2;
            tvis2.hParent = hStr;
            tvis2.hInsertAfter = TVI_LAST;
            tvis2.item.mask = TVIF_TEXT | TVIF_IMAGE | TVIF_SELECTEDIMAGE | TVIF_CHILDREN | TVIF_PARAM;

            WORD state = PROGRESS_STATE_TRANSLATED;
            for (int j = 0; j < 16; j++)
            {
                if (StrData[i]->TStrings[j] != NULL && StrData[i]->TState[j] != PROGRESS_STATE_TRANSLATED)
                {
                    state = PROGRESS_STATE_UNTRANSLATED;
                    break;
                }
            }

            tvis2.item.iImage = (state == PROGRESS_STATE_UNTRANSLATED) ? 0 : 1;
            tvis2.item.iSelectedImage = tvis2.item.iImage;

            char buff[100];
            sprintf_s(buff, "<%d>", (StrData[i]->ID - 1) << 4);
            tvis2.item.pszText = buff;
            tvis2.item.cChildren = 0;
            tvis2.item.lParam = TREE_TYPE_STRING | StrData[i]->ID;
            TreeView_InsertItem(hTree, &tvis2);
        }
    }

    TreeWindow.HDlgTreeItem = hDlg;
    TreeWindow.HMenuTreeItem = hMenu;
    TreeWindow.HStrTreeItem = hStr;

    SendMessage(hTree, WM_SETREDRAW, TRUE, 0); // povolime prekreslovani
}

void CData::FillTexts(DWORD lParam)
{
    HWND hListView = TextWindow.ListView.HWindow;
    if (hListView == NULL)
        return;
    ListView_DeleteAllItems(hListView);

    TextWindow.Mode = lParam & 0xFFFF0000;

    switch (lParam & 0xFFFF0000)
    {
    case TREE_TYPE_NONE:
    {
        PreviewWindow.PreviewDialog(-1);
        SetWindowText(TextWindow.HWindow, "Texts");
        break;
    }

    case TREE_TYPE_STRING:
    {
        wchar_t buffW[10000];
        PreviewWindow.PreviewDialog(-1);

        WORD block = LOWORD(lParam);
        int index = FindStrData(block);
        if (index != -1)
        {
            swprintf_s(buffW, L"Texts for strings <%d>", (block - 1) << 4);
            SetWindowTextW(TextWindow.HWindow, buffW);

            SendMessage(hListView, WM_SETREDRAW, FALSE, 0); // zakazeme zbytecne kresleni behem pridavani polozek

            CStrData* data = StrData[index];
            int pos = 0;
            for (int i = 0; i < 16; i++)
            {
                if (data->OStrings[i] != NULL)
                {
                    WORD strID = ((block - 1) << 4) | i;
                    LVITEMW lvi;
                    lvi.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
                    lvi.iItem = pos++;
                    lvi.iSubItem = 0;
                    lvi.iImage = data->TState[i] == PROGRESS_STATE_UNTRANSLATED ? 0 : 1;
                    BOOL isBookmark = Data.FindBookmark(TreeWindow.GetCurrentItem(), lvi.iItem) != -1;
                    if (isBookmark)
                        lvi.iImage += 3;

                    MultiByteToWideChar(CP_ACP, 0, DataRH.GetIdentifier(strID), -1, buffW, 9999);
                    lvi.pszText = buffW;
                    lvi.lParam = (LPARAM)MAKELPARAM(index, i);
                    SendMessageW(hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

                    lvi.mask = LVIF_TEXT;
                    lvi.pszText = buffW;
                    lstrcpynW(buffW, data->TStrings[i], 9999);
                    lvi.iSubItem = 1;
                    SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);

                    lstrcpynW(buffW, data->OStrings[i], 9999);
                    lvi.iSubItem = 2;
                    SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);
                }
            }
            if (pos > 0)
                ListView_SetItemState(hListView, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            SendMessage(hListView, WM_SETREDRAW, TRUE, 0); // povolime prekreslovani
        }
        break;
    }

    case TREE_TYPE_DIALOG:
    {
        char buffA[10000];
        wchar_t buffW[10000];

        WORD id = LOWORD(lParam);
        int index = FindDialogData(id);
        if (index != -1)
        {
            sprintf_s(buffA, "Texts for dialog %s", DataRH.GetIdentifier(id));
            SetWindowText(TextWindow.HWindow, buffA);
            sprintf_s(buffA, "Preview for dialog %s", DataRH.GetIdentifier(id));
            SetWindowText(PreviewWindow.HWindow, buffA);

            SendMessage(hListView, WM_SETREDRAW, FALSE, 0); // zakazeme zbytecne kresleni behem pridavani polozek

            CDialogData* data = DlgData[index];
            PreviewWindow.PreviewDialog(index);

            int pos = 0;
            for (int i = 0; i < data->Controls.Count; i++)
            {
                CControl* control = data->Controls[i];
                if (!control->ShowInLVWithControls(i))
                    continue;

                WORD strID = control->ID;
                LVITEMW lvi;
                lvi.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
                lvi.iItem = pos;
                lvi.iSubItem = 0;
                lvi.iImage = control->State == PROGRESS_STATE_UNTRANSLATED ? 0 : 1;
                BOOL isBookmark = Data.FindBookmark(TreeWindow.GetCurrentItem(), lvi.iItem) != -1;
                if (isBookmark)
                    lvi.iImage += 3;

                MultiByteToWideChar(CP_ACP, 0, DataRH.GetIdentifier(strID), -1, buffW, 9999);
                lvi.pszText = buffW;
                if (strID == 0)
                    buffW[0] = 0;

                lvi.lParam = (LPARAM)MAKELPARAM(index, i);
                SendMessageW(hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

                lvi.mask = LVIF_TEXT;
                lvi.pszText = buffW;

                lstrcpynW(buffW, control->TWindowName, 9999);
                lvi.iSubItem = 1;
                SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);

                lstrcpynW(buffW, control->OWindowName, 9999);
                lvi.iSubItem = 2;
                SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);

                pos++;
            }
            if (pos > 0)
                ListView_SetItemState(hListView, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            SendMessage(hListView, WM_SETREDRAW, TRUE, 0); // povolime prekreslovani
        }
        break;
    }

    case TREE_TYPE_MENU:
    {
        char buffA[10000];
        wchar_t buffW[10000];
        PreviewWindow.PreviewDialog(-1);

        WORD id = LOWORD(lParam);
        int index = FindMenuData(id);
        if (index != -1)
        {
            sprintf_s(buffA, "Texts for menu %s", DataRH.GetIdentifier(id));
            SetWindowText(TextWindow.HWindow, buffA);
            sprintf_s(buffA, "Preview: click in this window to display menu %s", DataRH.GetIdentifier(id));
            SetWindowText(PreviewWindow.HWindow, buffA);

            SendMessage(hListView, WM_SETREDRAW, FALSE, 0); // zakazeme zbytecne kresleni behem pridavani polozek

            CMenuData* data = MenuData[index];
            int pos = 0;
            for (int i = 0; i < data->Items.Count; i++)
            {
                if (wcslen(data->Items[i].TString) == 0)
                    continue;

                WORD strID = data->Items[i].ID;
                LVITEMW lvi;
                lvi.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_PARAM;
                lvi.iItem = pos;
                lvi.iSubItem = 0;
                lvi.iImage = data->Items[i].State == PROGRESS_STATE_UNTRANSLATED ? 0 : 1;
                BOOL isBookmark = Data.FindBookmark(TreeWindow.GetCurrentItem(), lvi.iItem) != -1;
                if (isBookmark)
                    lvi.iImage += 3;

                MultiByteToWideChar(CP_ACP, 0, DataRH.GetIdentifier(strID), -1, buffW, 9999);
                lvi.pszText = buffW;
                if (strID == 0)
                    buffW[0] = 0;

                lvi.lParam = (LPARAM)MAKELPARAM(index, i);
                SendMessageW(hListView, LVM_INSERTITEMW, 0, (LPARAM)&lvi);

                lvi.mask = LVIF_TEXT;
                lvi.pszText = buffW;

                // vnorena submenu odhodime o level vpravo
                int j = 0;
                while (j < data->Items[i].Level * 5) // pet mezer je jeste na dalsim miste
                {
                    buffW[j] = ' ';
                    j++;
                }

                lstrcpynW(buffW + j, data->Items[i].TString, 9999);
                lvi.iSubItem = 1;
                SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);

                lstrcpynW(buffW + j, data->Items[i].OString, 9999);
                lvi.iSubItem = 2;
                SendMessageW(hListView, LVM_SETITEMW, 0, (LPARAM)&lvi);

                pos++;
            }
            if (pos > 0)
                ListView_SetItemState(hListView, 0, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
            SendMessage(hListView, WM_SETREDRAW, TRUE, 0); // povolime prekreslovani
        }
        break;
    }

    default:
    {
        TRACE_E("Unknown type: 0x" << std::hex << lParam << std::dec);
    }
    }
    TextWindow.EnableControls();
}

void CData::UpdateNode(HTREEITEM hItem)
{
    HWND hTree = TreeWindow.GetTreeView();
    if (hTree == NULL)
        return;

    TVITEM tvi;
    tvi.mask = TVIF_HANDLE | TVIF_PARAM;
    tvi.hItem = hItem;
    if (!TreeView_GetItem(hTree, &tvi))
        return;

    DWORD type = (DWORD)(tvi.lParam & 0xFFFF0000);
    WORD groupID = (WORD)(tvi.lParam & 0x0000FFFF);
    WORD state = PROGRESS_STATE_TRANSLATED;
    switch (type)
    {
    case TREE_TYPE_STRING:
    {
        for (int i = 0; i < StrData.Count; i++)
        {
            if (StrData[i]->ID == groupID)
            {
                for (int j = 0; j < 16; j++)
                {
                    if (StrData[i]->TStrings[j] != NULL && StrData[i]->TState[j] != PROGRESS_STATE_TRANSLATED)
                    {
                        state = PROGRESS_STATE_UNTRANSLATED;
                        break;
                    }
                }
                break;
            }
        }
        break;
    }

    case TREE_TYPE_MENU:
    {
        for (int i = 0; i < MenuData.Count; i++)
        {
            if (MenuData[i]->ID == groupID)
            {
                for (int j = 0; j < MenuData[i]->Items.Count; j++)
                {
                    if (MenuData[i]->Items[j].State != PROGRESS_STATE_TRANSLATED)
                    {
                        state = PROGRESS_STATE_UNTRANSLATED;
                        break;
                    }
                }
                break;
            }
        }
        break;
    }

    case TREE_TYPE_DIALOG:
    {
        for (int i = 0; i < DlgData.Count; i++)
        {
            if (DlgData[i]->ID == groupID)
            {
                for (int j = 0; j < DlgData[i]->Controls.Count; j++)
                {
                    if (DlgData[i]->Controls[j]->State != PROGRESS_STATE_TRANSLATED)
                    {
                        state = PROGRESS_STATE_UNTRANSLATED;
                        break;
                    }
                }
                break;
            }
        }
        break;
    }
    }

    tvi.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    tvi.iImage = (state == PROGRESS_STATE_UNTRANSLATED) ? 0 : 1;
    tvi.iSelectedImage = tvi.iImage;
    TreeView_SetItem(hTree, &tvi);
}

void CData::UpdateSelectedNode()
{
    HWND hTree = TreeWindow.GetTreeView();
    if (hTree == NULL)
        return;

    HTREEITEM hItem = TreeView_GetSelection(hTree);
    if (hTree == NULL)
        return;

    UpdateNode(hItem);
}

void CData::UpdateTexts()
{
    // update list view
    TVITEM tvi;
    tvi.mask = TVIF_HANDLE | TVIF_PARAM;
    tvi.hItem = TreeView_GetSelection(TreeWindow.GetTreeView());
    TreeView_GetItem(TreeWindow.GetTreeView(), &tvi);
    Data.FillTexts(tvi.lParam);
}

void CData::UpdateAllNodes()
{
    HWND hTree = TreeWindow.GetTreeView();
    if (hTree == NULL)
        return;

    HTREEITEM hItem;
    // dialogs
    if (TreeWindow.HDlgTreeItem != NULL)
    {
        hItem = TreeView_GetChild(hTree, TreeWindow.HDlgTreeItem);
        while (hItem != NULL)
        {
            UpdateNode(hItem);
            hItem = TreeView_GetNextSibling(hTree, hItem);
        }
    }

    // menus
    if (TreeWindow.HMenuTreeItem != NULL)
    {
        hItem = TreeView_GetChild(hTree, TreeWindow.HMenuTreeItem);
        while (hItem != NULL)
        {
            UpdateNode(hItem);
            hItem = TreeView_GetNextSibling(hTree, hItem);
        }
    }

    // strings
    if (TreeWindow.HStrTreeItem != NULL)
    {
        hItem = TreeView_GetChild(hTree, TreeWindow.HStrTreeItem);
        while (hItem != NULL)
        {
            UpdateNode(hItem);
            hItem = TreeView_GetNextSibling(hTree, hItem);
        }
    }
}
