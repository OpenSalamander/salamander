// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "wndtext.h"
#include "wndtree.h"
#include "wndprev.h"
#include "wndframe.h"
#include "config.h"
#include "datarh.h"

const char* TEXTWINDOW_NAME = "Texts (Alt+3)";

//*****************************************************************************
//
// Edit WindowsProc
//

WNDPROC DefEditWndProc = NULL;
WNDPROC DefEditWndOriginalProc = NULL;
HWND HLastFocus = NULL;

LRESULT
EditWindowOriginalProcW(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_GETDLGCODE:
    {
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }

    case WM_KILLFOCUS:
    {
        HLastFocus = hWnd;
        break;
    }
    }

    return DefEditWndProc(hWnd, uMsg, wParam, lParam);
}

BOOL SkipNextCharacter = FALSE;

LRESULT
EditWindowProcW(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_GETDLGCODE:
    {
        return DLGC_WANTARROWS | DLGC_WANTCHARS;
    }

    case WM_KILLFOCUS:
    {
        HLastFocus = hWnd;
        break;
    }

    case WM_CHAR:
    {
        if (SkipNextCharacter)
        {
            SkipNextCharacter = FALSE; // zamezime pipnuti
            return FALSE;
        }

        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        if (wParam == 13 || wParam == 10) // VK_RETURN
        {
            if (TextWindow.CanLeaveText())
            {
                TextWindow.ChangeCurrentState(TRUE /*!controlPressed*/);
                if (controlPressed)
                    PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_GOTO_NEXT, 0); // dalsi polozka z Find
                else
                    TextWindow.Navigate(TRUE);
            }
            return 0;
        }

        DWORD style = GetWindowLong(hWnd, GWL_STYLE);
        if (style & ES_READONLY)
            MessageBeep(-1);
        break;
    }

    case WM_KEYDOWN:
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (wParam == 'A' && controlPressed && !shiftPressed && !altPressed)
        {
            SendMessage(hWnd, EM_SETSEL, 0, -1);
            SkipNextCharacter = TRUE; // zamezime pipnuti
            return 0;
        }

        int iStart, iEnd;
        SendMessage(hWnd, EM_GETSEL, (WPARAM)&iStart, (LPARAM)&iEnd);
        int count = GetWindowTextLength(hWnd);
        int lineCount = SendMessage(hWnd, EM_GETLINECOUNT, 0, 0);
        BOOL wholeSelected = iStart == 0 && iEnd == count && iEnd > iStart;
        if (!wholeSelected && !controlPressed && lineCount > 1)
            break;
        switch (wParam)
        {
        case VK_UP:
        {
            if (TextWindow.CanLeaveText())
            {
                if (controlPressed)
                    TreeWindow.Navigate(FALSE);
                else
                    TextWindow.Navigate(FALSE);
            }
            return 0;
        }

        case VK_DOWN:
        {
            if (TextWindow.CanLeaveText())
            {
                if (controlPressed)
                    TreeWindow.Navigate(TRUE);
                else
                    TextWindow.Navigate(TRUE);
            }
            return 0;
        }
        }
        break;
    }
    }
    return DefEditWndProc(hWnd, uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CListView
//

CListView::CListView()
    : CWindow(ooStatic)
{
}

LRESULT
CListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KILLFOCUS:
    {
        HLastFocus = HWindow;
        break;
    }

    case WM_KEYDOWN:
    {
        BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        switch (wParam)
        {
        case VK_UP:
        {
            if (controlPressed)
            {
                TreeWindow.Navigate(FALSE);
                return 0;
            }
            break;
        }

        case VK_DOWN:
        {
            if (controlPressed)
            {
                TreeWindow.Navigate(TRUE);
                return 0;
            }
            break;
        }

        case VK_RETURN:
        {
            TextWindow.ChangeCurrentState(/*!controlPressed*/ TRUE);
            if (controlPressed)
                PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_GOTO_NEXT, 0); // dalsi polozka z Find
            else
                TextWindow.Navigate(TRUE);
            return 0;
        }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//*****************************************************************************
//
// CTextWindow
//

CTextWindow::CTextWindow()
    : CWindow(ooStatic)
{
    DefWndProc = DefMDIChildProc;
    HOriginalLabel = NULL;
    HTranslatedLabel = NULL;
    HImageList = NULL;
    Mode = 0;
}

CTextWindow::~CTextWindow()
{
}

void CTextWindow::InitListView()
{
    LV_COLUMN lvc;
    const char* header[3] = {"ID", "Translated", "Original"};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    for (int i = 0; i < 3; i++) // vytvorim sloupce
    {
        lvc.pszText = (char*)header[i];
        lvc.iSubItem = i;
        ListView_InsertColumn(ListView.HWindow, i, &lvc);
    }
    SetColumnsWidth();
}

void CTextWindow::SetColumnsWidth()
{
    RECT r;
    GetClientRect(HWindow, &r);
    r.right -= 23;
    ListView_SetColumnWidth(ListView.HWindow, 0, Config.ColumnNameWidth == -1 ? r.right / 5 : Config.ColumnNameWidth);
    ListView_SetColumnWidth(ListView.HWindow, 1, Config.ColumnTranslatedWidth == -1 ? (r.right - r.right / 5) / 2 : Config.ColumnTranslatedWidth);
    ListView_SetColumnWidth(ListView.HWindow, 2, Config.ColumnOriginalWidth == -1 ? (r.right - r.right / 5) / 2 : Config.ColumnOriginalWidth);
}

void CTextWindow::SaveColumnsWidth()
{
    Config.ColumnNameWidth = ListView_GetColumnWidth(ListView.HWindow, 0);
    Config.ColumnTranslatedWidth = ListView_GetColumnWidth(ListView.HWindow, 1);
    Config.ColumnOriginalWidth = ListView_GetColumnWidth(ListView.HWindow, 2);
}

void CTextWindow::EnableControls()
{
    if (HTranslated == NULL || HOriginal == NULL || ListView.HWindow == NULL)
        return;
    int index = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
    BOOL enabled = index != -1;
    EnableWindow(HTranslated, enabled);
    EnableWindow(ListView.HWindow, enabled);
    if (!enabled)
    {
        SetWindowText(HTranslated, "");
        SetWindowText(HOriginal, "");
    }
}

void CTextWindow::Navigate(BOOL down)
{
    if (ListView.HWindow == NULL)
        return;

    int count = ListView_GetItemCount(ListView.HWindow);
    int index = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
    if (index == -1 || count < 2)
        return;

    if (down)
        index++;
    else
        index--;

    if (index < 0)
        index = 0;
    if (index >= count)
        index = count - 1;

    ListView_SetItemState(ListView.HWindow, index, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED);
    ListView_EnsureVisible(ListView.HWindow, index, FALSE);
}

void CTextWindow::RemoveSelection()
{
    int selIndex = ListView_GetNextItem(ListView.HWindow, -1, LVNI_SELECTED);
    if (selIndex != -1)
        ListView_SetItemState(ListView.HWindow, selIndex, 0, LVNI_SELECTED);
}
void CTextWindow::SelectIndex(WORD index)
{
    if (ListView.HWindow == NULL)
        return;

    if (index != -1)
    {
        ListView_SetItemState(ListView.HWindow, index, LVNI_SELECTED | LVNI_FOCUSED, LVNI_SELECTED | LVNI_FOCUSED);
        ListView_EnsureVisible(ListView.HWindow, index, FALSE);

        SendMessage(FrameWindow.HMDIClient, WM_MDIACTIVATE, (WPARAM)HWindow, NULL);
        SetFocus(HTranslated);
    }
}

WORD CTextWindow::GetSelectIndex()
{
    if (ListView.HWindow == NULL)
        return -1;
    int index = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
    return index;
}

void CTextWindow::SelectItem(WORD id)
{
    if (ListView.HWindow == NULL)
        return;

    if (Mode != TREE_TYPE_DIALOG)
        return;

    int index = -1;
    int count = ListView_GetItemCount(ListView.HWindow);
    for (int i = 0; i < count; i++)
    {
        LVITEM lvi;
        lvi.mask = LVIF_PARAM;
        lvi.iItem = i;
        ListView_GetItem(ListView.HWindow, &lvi);

        int groupIndex = LOWORD(lvi.lParam);
        int ctrlIndex = HIWORD(lvi.lParam);

        if (ctrlIndex > 0) // na indexu 0 je titulek dialogu, ten nehledame
        {
            CControl* control = Data.DlgData[groupIndex]->Controls[ctrlIndex];
            if (control->ID == id)
            {
                index = i;
                break;
            }
        }
    }

    if (index != -1)
        SelectIndex(index);
}

BOOL CTextWindow::CanLeaveText()
{
    if (ListView.HWindow == NULL)
        return TRUE;

    int index = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
    if (index == -1)
        return TRUE;

    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
    lvi.iItem = index;
    ListView_GetItem(ListView.HWindow, &lvi);
    int groupIndex = LOWORD(lvi.lParam);
    int strIndex = HIWORD(lvi.lParam);

    wchar_t* str = (wchar_t*)L"";
    switch (Mode)
    {
    case TREE_TYPE_STRING:
    {
        str = Data.StrData[groupIndex]->TStrings[strIndex];
        break;
    }

    case TREE_TYPE_MENU:
    {
        str = Data.MenuData[groupIndex]->Items[strIndex].TString;
        break;
    }

    case TREE_TYPE_DIALOG:
    {
        str = Data.DlgData[groupIndex]->Controls[strIndex]->TWindowName;
        break;
    }
    }

    if (wcslen(str) == 0 && // prazdne texty nejsou povolene krome statickych textu v dialozich
        (Mode != TREE_TYPE_DIALOG || strIndex == 0 ||
         !Data.DlgData[groupIndex]->Controls[strIndex]->IsStaticText()))
    {
        MessageBox(GetMsgParent(), "Empty string is not allowed here.", ERROR_TITLE, MB_OK | MB_ICONEXCLAMATION);
        SetFocus(HTranslated);
        return FALSE;
    }

    return TRUE;
}

void CTextWindow::SetFocusTranslated()
{
    SetFocus(HTranslated);
}

void CTextWindow::ChangeCurrentState(BOOL forceTranslated)
{
    int selIndex = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);

    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
    lvi.iItem = selIndex;
    ListView_GetItem(ListView.HWindow, &lvi);

    WORD state = PROGRESS_STATE_UNTRANSLATED;

    switch (Mode)
    {
    case TREE_TYPE_STRING:
    {
        int groupIndex = LOWORD(lvi.lParam);
        int strIndex = HIWORD(lvi.lParam);
        state = Data.StrData[groupIndex]->TState[strIndex];
        if (state == PROGRESS_STATE_UNTRANSLATED)
            state = PROGRESS_STATE_TRANSLATED;
        else
            state = PROGRESS_STATE_UNTRANSLATED;
        if (forceTranslated)
            state = PROGRESS_STATE_TRANSLATED;
        Data.StrData[groupIndex]->TState[strIndex] = state;
        break;
    }

    case TREE_TYPE_MENU:
    {
        int groupIndex = LOWORD(lvi.lParam);
        int strIndex = HIWORD(lvi.lParam);
        state = Data.MenuData[groupIndex]->Items[strIndex].State;
        if (state == PROGRESS_STATE_UNTRANSLATED)
            state = PROGRESS_STATE_TRANSLATED;
        else
            state = PROGRESS_STATE_UNTRANSLATED;
        if (forceTranslated)
            state = PROGRESS_STATE_TRANSLATED;
        Data.MenuData[groupIndex]->Items[strIndex].State = state;
        break;
    }

    case TREE_TYPE_DIALOG:
    {
        int groupIndex = LOWORD(lvi.lParam);
        int strIndex = HIWORD(lvi.lParam);
        state = Data.DlgData[groupIndex]->Controls[strIndex]->State;
        if (state == PROGRESS_STATE_UNTRANSLATED)
            state = PROGRESS_STATE_TRANSLATED;
        else
            state = PROGRESS_STATE_UNTRANSLATED;
        if (forceTranslated)
            state = PROGRESS_STATE_TRANSLATED;
        Data.DlgData[groupIndex]->Controls[strIndex]->State = state;
        break;
    }
    }

    lvi.mask = LVIF_IMAGE;
    lvi.iSubItem = 0;
    lvi.iImage = state == PROGRESS_STATE_UNTRANSLATED ? 0 : 1;
    BOOL isBookmark = Data.FindBookmark(TreeWindow.GetCurrentItem(), lvi.iItem) != -1;
    if (isBookmark)
        lvi.iImage += 3;
    ListView_SetItem(ListView.HWindow, &lvi);

    Data.UpdateSelectedNode();
    Data.SetDirty();
    Data.SLGSignature.SLTDataChanged();
}

void CTextWindow::RefreshBookmarks()
{
    int count = ListView_GetItemCount(ListView.HWindow);
    for (int i = 0; i < count; i++)
    {
        LVITEM lvi;
        lvi.mask = LVIF_PARAM;
        lvi.iSubItem = 0;
        lvi.iItem = i;
        ListView_GetItem(ListView.HWindow, &lvi);

        WORD state = PROGRESS_STATE_UNTRANSLATED;

        switch (Mode)
        {
        case TREE_TYPE_STRING:
        {
            int groupIndex = LOWORD(lvi.lParam);
            int strIndex = HIWORD(lvi.lParam);
            state = Data.StrData[groupIndex]->TState[strIndex];
            break;
        }

        case TREE_TYPE_MENU:
        {
            int groupIndex = LOWORD(lvi.lParam);
            int strIndex = HIWORD(lvi.lParam);
            state = Data.MenuData[groupIndex]->Items[strIndex].State;
            break;
        }

        case TREE_TYPE_DIALOG:
        {
            int groupIndex = LOWORD(lvi.lParam);
            int strIndex = HIWORD(lvi.lParam);
            state = Data.DlgData[groupIndex]->Controls[strIndex]->State;
            break;
        }
        }

        lvi.mask = LVIF_IMAGE;
        lvi.iImage = state == PROGRESS_STATE_UNTRANSLATED ? 0 : 1;
        BOOL isBookmark = Data.FindBookmark(TreeWindow.GetCurrentItem(), lvi.iItem) != -1;
        if (isBookmark)
            lvi.iImage += 3;
        ListView_SetItem(ListView.HWindow, &lvi);
    }
}

void CTextWindow::OnContextMenu(int x, int y)
{
    HWND hListView = ListView.HWindow;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;
    int selIndex = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
    if (selIndex == -1)
        return;

    char buff[300];
    char buff2[200];
    HMENU hMenu = CreatePopupMenu();

    LVITEM lvi;
    lvi.mask = LVIF_TEXT | LVIF_PARAM;
    lvi.iItem = selIndex;
    lvi.iSubItem = 0;
    lvi.pszText = buff2;
    lvi.cchTextMax = 200;
    ListView_GetItem(hListView, &lvi);

    sprintf_s(buff, "Toggle state: Translated / Untranslated");
    InsertMenu(hMenu, -1, MF_BYPOSITION, 1, buff);
    SetMenuDefaultItem(hMenu, 0, MF_BYPOSITION);
    InsertMenu(hMenu, -1, MF_BYPOSITION | MF_SEPARATOR, 0, "");
    int showID = 0;
    int groupIndex = LOWORD(lvi.lParam);
    int strIndex = HIWORD(lvi.lParam);
    switch (Mode)
    {
    case TREE_TYPE_STRING:
    {
        showID = ((Data.StrData[groupIndex]->ID - 1) << 4) | strIndex;
        break;
    }

    case TREE_TYPE_MENU:
    {
        showID = Data.MenuData[groupIndex]->Items[strIndex].ID;
        break;
    }

    case TREE_TYPE_DIALOG:
    {
        showID = strIndex == 0 ? Data.DlgData[groupIndex]->ID : Data.DlgData[groupIndex]->Controls[strIndex]->ID;
        break;
    }
    }
    sprintf_s(buff, "Copy to Clipboard: %s", DataRH.GetIdentifier(showID, FALSE));
    InsertMenu(hMenu, -1, MF_BYPOSITION, 2, buff);
    sprintf_s(buff, "Copy to Clipboard: %d", showID);
    InsertMenu(hMenu, -1, MF_BYPOSITION, 3, buff);

    DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, HWindow, NULL);
    DestroyMenu(hMenu);

    switch (cmd)
    {
    case 1:
    {
        ChangeCurrentState();
        break;
    }

    case 2:
    {
        CopyTextToClipboard(DataRH.GetIdentifier(showID, FALSE), -1);
        break;
    }

    case 3:
    {
        sprintf_s(buff, "%d", showID);
        CopyTextToClipboard(buff, -1);
        break;
    }
    }
}

LRESULT
CTextWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {

        // priradim oknu ikonku
        //      SendMessage(HWindow, WM_SETICON, ICON_BIG,
        //                  (LPARAM)LoadIcon(HInstance, MAKEINTRESOURCE(IDI_MAIN)));

        ListView.CreateEx(WS_EX_STATICEDGE, WC_LISTVIEW, "",
                          WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | WS_TABSTOP |
                              LVS_REPORT | LVS_NOSORTHEADER | LVS_SHOWSELALWAYS | LVS_SINGLESEL,
                          0, 0, 0, 0,
                          HWindow,
                          NULL, //HMenu
                          HInstance,
                          &ListView);

        HImageList = ImageList_LoadImage(HInstance, MAKEINTRESOURCE(IDB_TREE),
                                         16, 0, RGB(255, 0, 255), IMAGE_BITMAP, 0);
        ListView_SetImageList(ListView.HWindow, HImageList, LVSIL_SMALL);

        ListView_SetExtendedListViewStyleEx(ListView.HWindow, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);
        InitListView();

        HTranslatedLabel = CreateWindowEx(0, "static", "T&ranslated text:",
                                          WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS,
                                          0, 0, 0, 0,
                                          HWindow,
                                          NULL, //HMenu
                                          HInstance,
                                          NULL);

        // Stara particka common controls (listbox, editline, ...) funguji jinak nez nove controls
        // jako je listview. U novych controlu se data drzi v unicode a prevadi pri setovani/ziskavani
        // do/z controlu. U starych controlu zalezi na tom, jak control vytvorime (zda ansi pres
        // CreateWindowEx() nebo UNICODE pres CreateWindowExW()
        DWORD otherFlags = 0;
        if (Data.MUIMode)
            otherFlags = ES_READONLY;
        HTranslated = CreateWindowExW(WS_EX_STATICEDGE, L"edit", L"",
                                      WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | WS_TABSTOP |
                                          ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | otherFlags,
                                      0, 0, 0, 0,
                                      HWindow,
                                      NULL, //HMenu
                                      HInstance,
                                      NULL);
        DefEditWndProc = (WNDPROC)GetWindowLongW(HTranslated, GWL_WNDPROC);
        SetWindowLongW(HTranslated, GWL_WNDPROC, (LONG)EditWindowProcW);

        SendMessage(HTranslated, EM_LIMITTEXT, 5000, 0); // omezim velikost pro buffery

        HOriginalLabel = CreateWindowEx(0, "static", "&Original text:",
                                        WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS,
                                        0, 0, 0, 0,
                                        HWindow,
                                        NULL, //HMenu
                                        HInstance,
                                        NULL);

        HOriginal = CreateWindowExW(WS_EX_STATICEDGE, L"edit", L"",
                                    WS_CHILDWINDOW | WS_VISIBLE | WS_CLIPSIBLINGS | WS_VSCROLL | WS_TABSTOP |
                                        ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL | ES_READONLY,
                                    0, 0, 0, 0,
                                    HWindow,
                                    NULL, //HMenu
                                    HInstance,
                                    NULL);
        DefEditWndOriginalProc = (WNDPROC)GetWindowLongW(HOriginal, GWL_WNDPROC);
        SetWindowLongW(HOriginal, GWL_WNDPROC, (LONG)EditWindowOriginalProcW);

        HFONT hFont = (HFONT)SendMessage(ListView.HWindow, WM_GETFONT, 0, 0);
        SendMessage(HOriginalLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(HOriginal, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(HTranslatedLabel, WM_SETFONT, (WPARAM)hFont, TRUE);
        SendMessage(HTranslated, WM_SETFONT, (WPARAM)hFont, TRUE);

        HMENU hMenu = GetSystemMenu(HWindow, FALSE);
        if (hMenu != NULL)
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        break;
    }

    case WM_SIZE:
    {
        if (ListView.HWindow != NULL)
        {
            RECT r;
            GetClientRect(HWindow, &r);

            SIZE sz;
            HDC hDC = HANDLES(GetDC(HWindow));
            HFONT hOldFont = (HFONT)SelectObject(hDC, (HFONT)SendMessage(HOriginalLabel, WM_GETFONT, 0, 0));
            GetTextExtentPoint32(hDC, "q", 1, &sz);
            SelectObject(hDC, hOldFont);
            HANDLES(ReleaseDC(HWindow, hDC));

            int height = sz.cy;

            int top = r.bottom - (3 * height + 2);
            SetWindowPos(HOriginal, NULL, 0, top, r.right, 3 * height + 2, SWP_NOZORDER);

            top -= (int)(1.5 * height);
            SetWindowPos(HOriginalLabel, NULL, 0, top + 4, r.right, height, SWP_NOZORDER);

            top -= (int)(3.5 * height);
            SetWindowPos(HTranslated, NULL, 0, top, r.right, 3 * height + 2, SWP_NOZORDER);

            top -= (int)(1.5 * height);
            SetWindowPos(HTranslatedLabel, NULL, 0, top + 4, r.right, height, SWP_NOZORDER);

            SetWindowPos(ListView.HWindow, NULL, 0, 0, r.right, top, SWP_NOZORDER);
        }
        break;
    }

    case WM_CLOSE:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_CLOSE, 0); // bezpecny close-window
        return 0;
    }

    case WM_DESTROY:
    {
        ImageList_Destroy(HImageList);
        return 0;
    }

    case WM_SETFOCUS:
    {
        if (HLastFocus != NULL)
        {
            SetFocus(HLastFocus);
        }
        else
        {
            if (ListView.HWindow != NULL)
                SetFocus(ListView.HWindow);
        }
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE && (HWND)lParam == HTranslated)
        {
            Data.SetDirty();
            Data.SLGSignature.SLTDataChanged();

            int selIndex = ListView_GetNextItem(ListView.HWindow, -1, LVNI_FOCUSED);
            if (selIndex != -1)
            {
                wchar_t buff[10000];
                LVITEMW lvi;
                lvi.mask = LVIF_PARAM;
                lvi.iItem = selIndex;
                ListView_GetItem(ListView.HWindow, &lvi);

                switch (Mode)
                {
                case TREE_TYPE_STRING:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);

                    GetStringFromWindow(HTranslated, &Data.StrData[groupIndex]->TStrings[strIndex]);

                    lvi.mask = LVIF_TEXT;
                    lstrcpynW(buff, Data.StrData[groupIndex]->TStrings[strIndex], 9999);
                    buff[9999] = 0;
                    lvi.pszText = buff;
                    lvi.iSubItem = 1;
                    SendMessageW(ListView.HWindow, LVM_SETITEMW, 0, (LPARAM)&lvi);
                    break;
                }

                case TREE_TYPE_MENU:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);

                    GetStringFromWindow(HTranslated, &Data.MenuData[groupIndex]->Items[strIndex].TString);

                    // vnorena submenu odhodime o level vpravo
                    int j = 0;
                    while (j < Data.MenuData[groupIndex]->Items[strIndex].Level * 5) // pet mezer je jeste na dalsim miste
                    {
                        buff[j] = ' ';
                        j++;
                    }

                    lvi.mask = LVIF_TEXT;
                    lstrcpynW(buff + j, Data.MenuData[groupIndex]->Items[strIndex].TString, 9999);
                    buff[9999] = 0;
                    lvi.pszText = buff;
                    lvi.iSubItem = 1;
                    SendMessageW(ListView.HWindow, LVM_SETITEMW, 0, (LPARAM)&lvi);
                    break;
                }

                case TREE_TYPE_DIALOG:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);

                    CControl* control = Data.DlgData[groupIndex]->Controls[strIndex];
                    GetStringFromWindow(HTranslated, &control->TWindowName);

                    lvi.mask = LVIF_TEXT;
                    lstrcpynW(buff, control->TWindowName, 9999);
                    buff[9999] = 0;
                    lvi.pszText = buff;
                    lvi.iSubItem = 1;
                    SendMessageW(ListView.HWindow, LVM_SETITEMW, 0, (LPARAM)&lvi);

                    // na nultem indexu je nazev dialog; potom jsou controly
                    HWND hWnd = PreviewWindow.HDialog;
                    if (strIndex > 0)
                        hWnd = GetDlgItem(hWnd, control->ID);

                    wchar_t text[10000];
                    wchar_t* iter = text;
                    EncodeString(control->TWindowName, &iter);

                    SetWindowTextW(hWnd, text);
                    break;
                }
                }
            }
        }
        return 0;
    }

    case WM_NOTIFY:
    {
        NMLISTVIEW* nmlv = (NMLISTVIEW*)lParam;
        if (nmlv->hdr.hwndFrom == ListView.HWindow && ListView.HWindow != NULL)
        {
            if (nmlv->hdr.code == LVN_KEYDOWN)
            {
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                LPNMLVKEYDOWN pnkd = (LPNMLVKEYDOWN)lParam;
                if (pnkd->wVKey == VK_UP && controlPressed)
                    TreeWindow.Navigate(FALSE);
                if (pnkd->wVKey == VK_DOWN && controlPressed)
                    TreeWindow.Navigate(TRUE);
                return 0;
            }

            if (nmlv->hdr.code == NM_DBLCLK)
            {
                LPNMITEMACTIVATE lpnmitem = (LPNMITEMACTIVATE)lParam;
                if (lpnmitem->iItem != -1)
                    ChangeCurrentState();
                return 0;
            }

            if (nmlv->hdr.code == NM_RCLICK)
            {
                DWORD pos = GetMessagePos();
                OnContextMenu(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
                return 0;
            }

            if (nmlv->hdr.code == LVN_ITEMCHANGED)
            {
                LVITEM lvi;
                lvi.mask = LVIF_PARAM;
                lvi.iItem = nmlv->iItem;
                ListView_GetItem(ListView.HWindow, &lvi);

                const wchar_t* oStr = L"";
                const wchar_t* tStr = L"";
                switch (Mode)
                {
                case TREE_TYPE_STRING:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);
                    tStr = Data.StrData[groupIndex]->TStrings[strIndex];
                    oStr = Data.StrData[groupIndex]->OStrings[strIndex];
                    DataRH.ShowIdentifier(((Data.StrData[groupIndex]->ID - 1) << 4) | strIndex);
                    break;
                }

                case TREE_TYPE_MENU:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);
                    tStr = Data.MenuData[groupIndex]->Items[strIndex].TString;
                    oStr = Data.MenuData[groupIndex]->Items[strIndex].OString;
                    DataRH.ShowIdentifier(Data.MenuData[groupIndex]->Items[strIndex].ID);
                    break;
                }

                case TREE_TYPE_DIALOG:
                {
                    int groupIndex = LOWORD(lvi.lParam);
                    int strIndex = HIWORD(lvi.lParam);
                    CControl* control = Data.DlgData[groupIndex]->Controls[strIndex];
                    tStr = control->TWindowName;
                    oStr = control->OWindowName;
                    DataRH.ShowIdentifier(control->ID);
                    if (strIndex > 0)
                        PreviewWindow.HighlightControl(control->ID);
                    else
                        PreviewWindow.HighlightControl(0);
                    break;
                }
                }

                SendMessageW(HTranslated, WM_SETTEXT, 0, (WPARAM)tStr);
                SendMessageW(HTranslated, EM_SETSEL, 0, (WPARAM)-1);
                SendMessageW(HOriginal, WM_SETTEXT, 0, (WPARAM)oStr);
                SendMessageW(HOriginal, EM_SETSEL, 0, (WPARAM)-1);
            }

            if (nmlv->hdr.code == LVN_ITEMCHANGING)
            {
                return !CanLeaveText();
            }
        }
        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CTextWindow TextWindow;
