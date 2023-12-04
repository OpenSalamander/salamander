// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "tasklist.h"
#include "mainwnd.h"
#include "edtlbwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "gui.h"
#include "menu.h"
#include "shellib.h"

static char LastSelectedPluginDLLName[MAX_PATH] = {0}; // po dalsim otevreni Plugins manageru vybereme posledni vybranej plugin

//
// ****************************************************************************
// CPluginsDlg
//

CPluginsDlg::CPluginsDlg(HWND hParent) : CCommonDialog(HLanguage, IDD_PLUGINS, IDD_PLUGINS, hParent)
{
    HListView = NULL;
    Header = NULL;
    HImageList = NULL;
    RefreshPanels = FALSE;
    DrivesBarChange = FALSE;
    FocusPlugin[0] = 0;
    Url = NULL;
    ShowInBarText[0] = 0;
    ShowInChDrvText[0] = 0;
    InstalledPluginsText[0] = 0;
}

void CPluginsDlg::InitColumns()
{
    CALL_STACK_MESSAGE1("CPluginsDlg::InitColumns()");
    LV_COLUMN lvc;
    int header[4] = {IDS_PLUGINS_NAME, IDS_PLUGINS_LOADED, IDS_PLUGINS_VERSION, IDS_PLUGINS_LOCATION};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 4; i++) // vytvorim sloupce
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
        //    ListView_SetColumnWidth(HListView, i, LVSCW_AUTOSIZE_USEHEADER);   // sirky nastavime az v SetColumnWidths()
    }
}

void CPluginsDlg::SetColumnWidths()
{
    // vsem sloupcum nastavim optimalni sirky
    int i;
    for (i = 0; i <= 3; i++)
        ListView_SetColumnWidth(HListView, i, i < 3 ? LVSCW_AUTOSIZE_USEHEADER : LVSCW_AUTOSIZE); // posledni sloupec nechceme vyplnovej, zaroven bude obsah sirsi nez nadpis

    // nascitam sirky sloupcu, ktere urcite chceme mit zobrazene cele (vse za nazvem, ktery pripadne budeme zkracovat)
    int nameWidth = ListView_GetColumnWidth(HListView, 0);
    int otherWidths = 0 + GetSystemMetrics(SM_CXHSCROLL);
    for (i = 1; i < 4; i++)
        otherWidths += ListView_GetColumnWidth(HListView, i);

    //  SCROLLBARINFO si; // bylo by potreba loadit dynamicky, kaslem na to
    //  si.cbSize = sizeof(si);
    //  GetScrollBarInfo(HListView, OBJID_VSCROLL, &si);

    RECT r;
    GetClientRect(HListView, &r);
    int lvWidth = r.right - r.left;
    if (nameWidth + otherWidths < lvWidth + 10 + 10)
    {
        // pokud na to je prostor, zvetsime sloupce Loaded a Version o 10px (lip to pak vypada)
        for (i = 1; i <= 2; i++)
            ListView_SetColumnWidth(HListView, i, ListView_GetColumnWidth(HListView, i) + 10);
        otherWidths += 10 + 10;
    }

    // doplnkovy sloupec bude prvni -- Name, ale pouze pokud neni celkova sirka sloupcu moc velka
    if (nameWidth + otherWidths < lvWidth)
        ListView_SetColumnWidth(HListView, 0, lvWidth - otherWidths);
}

void CPluginsDlg::RefreshListView(BOOL setOnly, int selIndex, const CPluginData* selectPlugin, BOOL setColumnWidths)
{
    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);

    HIMAGELIST hIcons = Plugins.CreateIconsList(FALSE); // o destrukci se postara listview
    HIMAGELIST hOldIcons = ListView_SetImageList(HListView, hIcons, LVSIL_SMALL);
    if (hOldIcons != NULL)
        ImageList_Destroy(hOldIcons);

    int numOfLoaded = 0;
    Plugins.AddNamesToListView(HListView, setOnly, &numOfLoaded);

    if (Header != NULL)
    {
        char buf[300];
        sprintf(buf, InstalledPluginsText, Plugins.GetCount(), numOfLoaded);
        SendMessage(Header->HWindow, WM_SETREDRAW, FALSE, 0);
        SetWindowText(Header->HWindow, buf);
        SendMessage(Header->HWindow, WM_SETREDRAW, TRUE, 0);
        InvalidateRect(Header->HWindow, NULL, TRUE);
    }

    if (setColumnWidths)
        SetColumnWidths(); // nastavime sirky sloupcu

    // Petr: kdyz byl tento radek na konci funkce (kde je logictejsi), tak zustavala
    // fokusla polozka schovana na spodnim scrollbarem, aneb nebyla plne viditelna,
    // asi MS potrebuji redraw, aby scrollbar ukazali, a pak s nim i pocitali
    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);

    if (!setOnly)
    {
        int count = ListView_GetItemCount(HListView);
        if (count > 0)
        {
            if (selectPlugin != NULL)
                selIndex = Plugins.GetPluginOrderIndex(selectPlugin);
            if (selIndex >= count)
                selIndex = count - 1;
            if (selIndex < 0)
                selIndex = 0;
            DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItemState(HListView, selIndex, state, state);
            ListView_EnsureVisible(HListView, selIndex, FALSE);
        }
        else
            OnSelChanged(); // vynutime prazdne polozky
    }
    else
    {
        int count = ListView_GetItemCount(HListView);
        if (count > 0 && selIndex != -1)
        {
            DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
            ListView_SetItemState(HListView, selIndex, state, state);
            ListView_EnsureVisible(HListView, selIndex, FALSE);
        }
        OnSelChanged(); // vynutime enablery
    }
}

void CPluginsDlg::EnableButtons(CPluginData* plugin)
{
    HWND focus = GetFocus();
    BOOL changeFocus = FALSE;
    if (GetDlgItem(HWindow, IDB_PLUGINREMOVE) == focus && plugin == NULL)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINREMOVE), plugin != NULL);
    if (GetDlgItem(HWindow, IDB_PLUGINTEST) == focus && plugin == NULL)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINTEST), plugin != NULL);
    if (GetDlgItem(HWindow, IDB_PLUGINTESTALL) == focus && plugin == NULL)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINTESTALL), plugin != NULL);
    if (GetDlgItem(HWindow, IDB_PLUGINABOUT) == focus && plugin == NULL)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINFOCUS), plugin != NULL);
    if (GetDlgItem(HWindow, IDB_PLUGINFOCUS) == focus && plugin == NULL)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINABOUT), plugin != NULL);
    if (GetDlgItem(HWindow, IDB_PLUGINUNLOAD) == focus &&
        (plugin == NULL || !plugin->GetLoaded()))
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINUNLOAD), plugin != NULL && plugin->GetLoaded());
    if (GetDlgItem(HWindow, IDB_PLUGINCONFIG) == focus &&
        (plugin == NULL || !plugin->SupportConfiguration))
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINCONFIG), plugin != NULL && plugin->SupportConfiguration);
    if (GetDlgItem(HWindow, IDB_PLUGINKEYS) == focus &&
        (plugin == NULL || plugin->MenuItems.Count == 0 && !plugin->SupportDynMenuExt))
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_PLUGINKEYS), plugin != NULL && (plugin->MenuItems.Count > 0 || plugin->SupportDynMenuExt));

    if (changeFocus)
    {
        PostMessage(focus, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        focus = GetDlgItem(HWindow, IDOK);
        PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        SetFocus(focus);
    }
}

void CPluginsDlg::EnableHeader()
{
    DWORD mask = TLBHDRMASK_SORT;
    int index = ListView_GetNextItem(HListView, -1, LVNI_FOCUSED);
    int count = ListView_GetItemCount(HListView);
    if (index > 0)
        mask |= TLBHDRMASK_UP;
    if (index < count - 1)
        mask |= TLBHDRMASK_DOWN;
    Header->EnableToolbar(mask);
}

void CPluginsDlg::OnSelChanged()
{
    CPluginData* p = GetSelectedPlugin();
    HWND showInBar = GetDlgItem(HWindow, IDC_PLUGINSHOWINBAR);
    HWND showInChDrv = GetDlgItem(HWindow, IDC_PLUGINSHOWINCHDRV);
    if (p != NULL)
    {
        if (p->DLLName != NULL)
            lstrcpyn(LastSelectedPluginDLLName, p->DLLName, MAX_PATH);
        else
            LastSelectedPluginDLLName[0] = 0;

        if (!IsWindowVisible(showInBar))
            ShowWindow(showInBar, SW_SHOW);
        if (!IsWindowVisible(showInChDrv))
            ShowWindow(showInChDrv, SW_SHOW);

        // description
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINDESCRIPTION), p->Description);
        // copyright
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINCOPYRIGHT), p->Copyright);
        // www
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINWWW),
                      p->PluginHomePageURL != NULL ? p->PluginHomePageURL : LoadStr(IDS_PLUGINURLNONE));
        Url->SetActionOpen(p->PluginHomePageURL != NULL ? p->PluginHomePageURL : "");
        // extension
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINEXTENSIONS),
                      p->Extensions[0] == 0 ? LoadStr(IDS_PLUGINEXTNONE) : p->Extensions);
        // FS Name
        char buf[500];
        buf[0] = 0;
        int remainingSize = sizeof(buf); // ulozime do 'buf' seznam fs-names, jmena budou oddelena ';'
        int i;
        for (i = 0; remainingSize > 1 && i < p->FSNames.Count; i++)
        {
            _snprintf_s(buf + (sizeof(buf) - remainingSize), remainingSize, _TRUNCATE,
                        (i + 1 != p->FSNames.Count) ? "%s;" : "%s", p->FSNames[i]);
            remainingSize = (int)sizeof(buf) - (int)strlen(buf);
        }
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINFSNAME),
                      buf[0] == 0 ? LoadStr(IDS_PLUGINFSNONE) : buf);
        // Functions
        buf[0] = 0;
        if (p->SupportPanelView)
            strcat(buf, LoadStr(IDS_PLUGINFUNCVIEW));
        if (p->SupportPanelEdit)
        {
            if (buf[0] != 0)
                strcat(buf, ",\n");
            strcat(buf, LoadStr(IDS_PLUGINFUNCEDIT));
        }
        if (p->SupportCustomPack)
        {
            if (buf[0] != 0)
                strcat(buf, ",\n");
            strcat(buf, LoadStr(IDS_PLUGINFUNCCUSTPACK));
        }
        if (p->SupportCustomUnpack)
        {
            if (p->SupportCustomPack)
                strcat(buf, ", "); // text custom packeru je kratsi - stejna radka
            else
            {
                if (buf[0] != 0)
                    strcat(buf, ",\n");
            }
            strcat(buf, LoadStr(IDS_PLUGINFUNCCUSTUNPACK));
        }
        if (p->SupportViewer)
        {
            if (buf[0] != 0)
                strcat(buf, ",\n");
            strcat(buf, LoadStr(IDS_PLUGINFUNCFILEVIEWER));
        }
        if (p->MenuItems.Count > 0 || p->SupportDynMenuExt)
        {
            if (p->SupportViewer)
                strcat(buf, ", "); // text viewru je kratsi - stejna radka
            else
            {
                if (buf[0] != 0)
                    strcat(buf, ",\n");
            }
            strcat(buf, LoadStr(IDS_PLUGINFUNCMENUEXTENSION));
        }

        if (p->SupportFS)
        {
            // text viewru+menu je kratsi - stejna radka
            if (p->SupportViewer || p->MenuItems.Count > 0 || p->SupportDynMenuExt)
                strcat(buf, ", ");
            else
            {
                if (buf[0] != 0)
                    strcat(buf, ",\n");
            }
            strcat(buf, LoadStr(IDS_PLUGINFUNCFILESYSTEM));
        }

        // Thumbnails
        if (p->ThumbnailMasks.GetMasksString()[0] != 0)
        {
            if (p->SupportViewer || p->MenuItems.Count > 0 || p->SupportDynMenuExt || p->SupportFS)
                strcat(buf, ", ");
            else
            {
                if (buf[0] != 0)
                    strcat(buf, ",\n");
            }
            strcat(buf, LoadStr(IDS_PLUGINFUNCTHUMBLOADER));
            SetWindowText(GetDlgItem(HWindow, IDC_PLUGINTHUMBNAILS), p->ThumbnailMasks.GetMasksString());
        }
        else
            SetWindowText(GetDlgItem(HWindow, IDC_PLUGINTHUMBNAILS), LoadStr(IDS_PLUGINTHUMBNONE));

        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINFUNCTIONS), buf);

        char buff[MAX_PATH + 200];
        char pluginName[300];
        lstrcpyn(pluginName, p->Name, 299);
        DuplicateAmpersands(pluginName, 299); // nazev pluginu muze obsahovat znak '&'
        sprintf(buff, ShowInBarText, pluginName);
        SetWindowText(showInBar, buff);

        char fsItemText[200];
        const char* itemText;
        if (p->ChDrvMenuFSItemName != NULL)
        {
            char* s = p->ChDrvMenuFSItemName;
            while (*s != 0 && *s != '\t')
                s++;
            if (*s == 0)
                itemText = p->ChDrvMenuFSItemName;
            else // je tam aspon jeden tabelator
            {
                lstrcpyn(fsItemText, s + 1, 200);
                itemText = fsItemText;
                s = fsItemText;
                while (*s != 0 && *s != '\t')
                    s++;
                if (*s == '\t')
                    *s = 0;
            }
        }
        else
            itemText = "FS";

        sprintf(buff, ShowInChDrvText, itemText);
        SetWindowText(showInChDrv, buff);

        int orderIndex = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
        if (orderIndex != -1)
        {
            BOOL hasMenu = p->MenuItems.Count > 0 || p->SupportDynMenuExt;
            BOOL showInBar2 = Plugins.GetShowInBar(Plugins.GetIndexByOrder(orderIndex));
            if (!hasMenu)
                showInBar2 = FALSE;
            CheckDlgButton(HWindow, IDC_PLUGINSHOWINBAR, showInBar2 ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(HWindow, IDC_PLUGINSHOWINBAR), hasMenu);

            BOOL hasItem = p->ChDrvMenuFSItemName != NULL;
            BOOL showInChDrv2 = Plugins.GetShowInChDrv(Plugins.GetIndexByOrder(orderIndex));
            if (!hasItem)
                showInChDrv2 = FALSE;
            CheckDlgButton(HWindow, IDC_PLUGINSHOWINCHDRV, showInChDrv2 ? BST_CHECKED : BST_UNCHECKED);
            EnableWindow(GetDlgItem(HWindow, IDC_PLUGINSHOWINCHDRV), hasItem);
        }

        EnableHeader();

        EnableButtons(p);
    }
    else
    {
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINDESCRIPTION), "");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINCOPYRIGHT), "");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINWWW), "");
        Url->SetActionOpen("");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINEXTENSIONS), "");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINFSNAME), "");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINTHUMBNAILS), "");
        SetWindowText(GetDlgItem(HWindow, IDC_PLUGINFUNCTIONS), "");
        /*if (IsWindowVisible(showInBar))*/ ShowWindow(showInBar, SW_HIDE);     // podminka zakomentovana, protoze jinak zlobi pri WM_INITDIALOG (dialog jeste neni videt jako celek -> podminka selze)
        /*if (IsWindowVisible(showInChDrv))*/ ShowWindow(showInChDrv, SW_HIDE); // podminka zakomentovana, protoze jinak zlobi pri WM_INITDIALOG (dialog jeste neni videt jako celek -> podminka selze)
        EnableButtons(NULL);
    }
}

CPluginData*
CPluginsDlg::GetSelectedPlugin(int* index, int* lvIndex)
{
    int i = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
    if (i == -1)
        return NULL;

    int orderIndex = Plugins.GetIndexByOrder(i);
    CPluginData* plugin = Plugins.Get(orderIndex);
    if (plugin != NULL && index != NULL)
    {
        *index = orderIndex;
        if (lvIndex != NULL)
            *lvIndex = i;
    }
    return plugin;
}

void CPluginsDlg::OnContextMenu(int x, int y)
{
    HWND hListView = HListView;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;

    // vytahame texty z tlacitek a naplnime context menu
    int ids[] = {-2, IDB_PLUGINCONFIG, // -2 -> next item will be default
                 IDB_PLUGINKEYS,
                 IDB_PLUGINABOUT,
                 IDB_PLUGINTEST,
                 IDB_PLUGINUNLOAD,
                 IDB_PLUGINFOCUS,
                 IDB_PLUGINREMOVE,
                 //               -1,                      // -1 -> separator
                 0}; // 0 -> terminator
    CMenuPopup popup;
    FillContextMenuFromButtons(&popup, HWindow, ids);

    DWORD cmd = popup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                            x, y, HWindow, NULL);
    if (cmd != 0)
        PostMessage(HWindow, WM_COMMAND, cmd, 0);
}

void CPluginsDlg::OnMove(BOOL up)
{
    int index = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
    int newIndex = up ? index - 1 : index + 1;
    if (Plugins.ChangePluginsOrder(index, newIndex))
    {
        if (Configuration.KeepPluginsSorted)
            OnSort();
        Plugins.UpdatePluginsOrder(FALSE);
        RefreshListView(TRUE, newIndex);
    }
}

void CPluginsDlg::OnSort()
{
    Configuration.KeepPluginsSorted = !Configuration.KeepPluginsSorted;
    Header->CheckToolbar(Configuration.KeepPluginsSorted ? TLBHDRMASK_SORT : 0);
    if (Configuration.KeepPluginsSorted)
    {
        CPluginData* selectedPlugin = GetSelectedPlugin();
        Plugins.UpdatePluginsOrder(TRUE);
        RefreshListView(FALSE, -1, selectedPlugin);
    }
}

/*
void
CPluginsDlg::OnDrag()
{
  LVHITTESTINFO ht;
  DWORD pos = GetMessagePos();
  ht.pt.x = GET_X_LPARAM(pos);
  ht.pt.y = GET_Y_LPARAM(pos);
  ScreenToClient(HListView, &ht.pt);
  ListView_HitTest(HListView, &ht);
  int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
  if (ht.iItem != -1 && ht.iItem != index)
  {

  }
}
*/

INT_PTR
CPluginsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    SLOW_CALL_STACK_MESSAGE4("CPluginsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // vytahnu text Show In Bar checkboxu do naseho bufferu
        GetDlgItemText(HWindow, IDC_PLUGINSHOWINBAR, ShowInBarText, 200);

        // vytahnu text Show In Change Drive Menu checkboxu do naseho bufferu
        GetDlgItemText(HWindow, IDC_PLUGINSHOWINCHDRV, ShowInChDrvText, 200);

        // vytahnu text "Installed Plugins:" do naseho bufferu
        GetDlgItemText(HWindow, IDC_PLUGINHEADER, InstalledPluginsText, 200);

        // Add bude mit drop down
        // new CButton(HWindow, IDB_PLUGINADD, BTF_DROPDOWN);

        // www bude hyperlink
        Url = new CHyperLink(HWindow, IDC_PLUGINWWW);
        if (Url == NULL)
            TRACE_E(LOW_MEMORY);

        // listview setup
        HListView = GetDlgItem(HWindow, IDL_PLUGINS);
        ListView_SetExtendedListViewStyleEx(HListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        // header line
        Header = new CToolbarHeader(HWindow, IDC_PLUGINHEADER, HListView,
                                    TLBHDRMASK_SORT | TLBHDRMASK_UP | TLBHDRMASK_DOWN);
        if (Header == NULL)
            TRACE_E(LOW_MEMORY);

        Header->CheckToolbar(Configuration.KeepPluginsSorted ? TLBHDRMASK_SORT : 0);

        // vlozime sloupce
        InitColumns();

        // pro vetsi pohodli vybereme posledne vybranou polozku (pokud existuje)
        CPluginData* lastSelectPluginData = NULL;
        int lastSelectedPluginIndex = 0;
        if (Plugins.FindDLL(LastSelectedPluginDLLName, lastSelectedPluginIndex))
            lastSelectPluginData = Plugins.Get(lastSelectedPluginIndex);

        // vlozime polozky
        RefreshListView(FALSE, 0, lastSelectPluginData, TRUE);

        break;
    }

    case WM_DESTROY:
    {
        MainWindow->OnPluginsStateChanged(); // mozna by to chtelo Dirty flag
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDL_PLUGINS)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, IDB_PLUGINCONFIG, 0);
                break;
            }

            case NM_RCLICK:
            {
                DWORD pos = GetMessagePos();
                OnContextMenu(GET_X_LPARAM(pos), GET_Y_LPARAM(pos));
                return 0;
            }

            case LVN_KEYDOWN:
            {
                LPNMLVKEYDOWN nmhk = (LPNMLVKEYDOWN)nmh;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed && nmhk->wVKey == VK_F10 || nmhk->wVKey == VK_APPS)
                {
                    POINT p;
                    GetListViewContextMenuPos(HListView, &p);
                    OnContextMenu(p.x, p.y);
                }
                if (nmhk->wVKey == VK_INSERT)
                    PostMessage(HWindow, WM_COMMAND, IDB_PLUGINADD, 0);
                if (nmhk->wVKey == VK_DELETE)
                    PostMessage(HWindow, WM_COMMAND, IDB_PLUGINREMOVE, 0);
                if ((GetKeyState(VK_MENU) & 0x8000) &&
                    (nmhk->wVKey == VK_UP || nmhk->wVKey == VK_DOWN))
                {
                    OnMove(nmhk->wVKey == VK_UP);
                }
                return 0;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if (!(nmhi->uOldState & LVIS_SELECTED) && nmhi->uNewState & LVIS_SELECTED)
                {
                    OnSelChanged();
                }
                return 0;
            }

                //case LVN_BEGINDRAG:
                //{
                //  OnDrag();
                //  return 0;
                //}
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_PLUGINHEADER:
        {
            if (GetFocus() != HListView)
                SetFocus(HListView);
            switch (HIWORD(wParam))
            {
            case TLBHDR_UP:
                OnMove(TRUE);
                break;
            case TLBHDR_DOWN:
                OnMove(FALSE);
                break;
            case TLBHDR_SORT:
                if (!Configuration.KeepPluginsSorted)
                    OnSort();
                break;
            }
            break;
        }

        case IDC_PLUGINSHOWINBAR:
        {
            int index = Plugins.GetIndexByOrder(ListView_GetNextItem(HListView, -1, LVIS_FOCUSED));
            if (index != -1)
            {
                BOOL showInBar = IsDlgButtonChecked(HWindow, IDC_PLUGINSHOWINBAR) == BST_CHECKED;
                Plugins.SetShowInBar(index, showInBar);
            }
            break;
        }

        case IDC_PLUGINSHOWINCHDRV:
        {
            int index = Plugins.GetIndexByOrder(ListView_GetNextItem(HListView, -1, LVIS_FOCUSED));
            if (index != -1)
            {
                DrivesBarChange = TRUE;
                BOOL showInChDrv = IsDlgButtonChecked(HWindow, IDC_PLUGINSHOWINCHDRV) == BST_CHECKED;
                Plugins.SetShowInChDrv(index, showInChDrv);
            }
            break;
        }

        case IDB_PLUGINABOUT:
        {
            RefreshPanels = TRUE;
            CPluginData* p = GetSelectedPlugin();
            if (p != NULL)
            {
                p->About(HWindow);
                RefreshListView(); // doslo k loadu DLL, mame novejsi data ...
            }
            return 0;
        }

        case IDB_PLUGINCONFIG:
        {
            RefreshPanels = TRUE;
            CPluginData* p = GetSelectedPlugin();
            if (p != NULL && p->SupportConfiguration)
            {
                p->Configuration(HWindow);
                RefreshListView(); // doslo k loadu DLL, mame novejsi data ...
            }
            return 0;
        }

        case IDB_PLUGINKEYS:
        {
        AGAIN:
            CPluginData* p = GetSelectedPlugin();
            if (p != NULL && (p->MenuItems.Count > 0 || p->SupportDynMenuExt))
            {
                // pokud uz je plugin nacteny, rebuildneme jeho menu - pokud nacteny neni, rebuildne se
                // jeho menu az pri nacitani pluginu samo
                if (p->GetLoaded() && p->SupportDynMenuExt)
                {
                    p->BuildMenu(HWindow, TRUE);
                    p->ReleasePluginDynMenuIcons(); // tenhle objekt nikdo nepotrebuje (pro dalsi zobrazeni menu se vse ziska znovu)
                }
                // pripadne nacteme plugin, at mame posledni verzi menu
                if (p->InitDLL(HWindow) &&
                    (p->MenuItems.Count > 0 ||
                     p->SupportDynMenuExt && p->GetPluginInterfaceForMenuExt()->NotEmpty()))
                {
                    if (p->MenuItems.Count > 0) // pro prazdne dynamicke menu dialog otevirat nebudeme (prazdne staticke vypadne o podminku drive bez hlasky)
                    {
                        CPluginKeys keys(HWindow, p);
                        if (keys.IsGood())
                        {
                            keys.Execute();
                            if (keys.Reset)
                            {
                                UpdateWindow(HWindow); // nechame updatnout Plugins Manager, at tam nezustava viset Shortcut Keys okno
                                p->Unload(HWindow, FALSE);
                                goto AGAIN;
                            }
                        }
                    }
                    else
                        TRACE_I("Plugin has dynamic menu which is empty (unexpected situation). We will not open Keyboard Shortcuts dialog.");
                }
                RefreshListView(); // doslo k loadu DLL, mame novejsi data ...
            }
            return 0;
        }

        case IDB_PLUGINADD:
        {
            RefreshPanels = TRUE;
            char fileName[2000];
            fileName[0] = 0;
            OPENFILENAME ofn;
            memset(&ofn, 0, sizeof(OPENFILENAME));
            ofn.lStructSize = sizeof(OPENFILENAME);
            ofn.hwndOwner = HWindow;
            char* s = LoadStr(IDS_PLUGINFILTER);
            ofn.lpstrFilter = s;
            while (*s != 0) // vytvoreni double-null terminated listu
            {
                if (*s == '|')
                    *s = 0;
                s++;
            }
            ofn.lpstrFile = fileName;
            ofn.nMaxFile = 2000;
            ofn.lpstrDefExt = "SPL";

            char buf[MAX_PATH];
            GetModuleFileName(HInstance, buf, MAX_PATH);
            s = strrchr(buf, '\\');
            if (s != NULL)
            {
                strcpy(s + 1, "plugins");
                ofn.lpstrInitialDir = buf;
            }

            ofn.nFilterIndex = 1;
            ofn.lpstrTitle = LoadStr(IDS_PLUGINADDTITLE);
            ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST |
                        OFN_HIDEREADONLY | OFN_LONGNAMES | OFN_NOCHANGEDIR;

            if (SafeGetOpenFileName(&ofn))
            {
                // cyklus pro vsechna oznacena jmena
                char oneName[MAX_PATH];
                char* fName = NULL;
                int off = 0;
                strcpy(oneName, fileName);
                off = (int)strlen(oneName);
                if (off + 1 < 2000 && *(fileName + off + 1) != 0) // neni jedine jmeno
                {
                    fName = oneName + off;
                    if (off > 0 && *(fileName + off - 1) != '\\') // chybi backslash
                    {
                        *fName++ = '\\';
                        *fName = 0;
                    }
                }

                BOOL pluginAdded = FALSE;
                CPluginData* addedPlugin = NULL;
                while (1)
                {
                    if (fName != NULL && off + 1 < 2000)
                    {
                        strcpy(fName, fileName + off + 1);
                        off += (int)(strlen(fileName + off + 1) + 1);
                    }

                    // v oneName je jmeno x-teho oznaceneho pluginu (enumerace)
                    char pluginName[MAX_PATH];
                    if (StrNICmp(oneName, buf, (int)strlen(buf)) == 0 && oneName[(int)strlen(buf)] == '\\')
                    {
                        memmove(pluginName, oneName + strlen(buf) + 1, strlen(oneName) - strlen(buf) + 1 - 1);
                    }
                    else
                        strcpy(pluginName, oneName);
                    BOOL add = TRUE;
                    int index;
                    if (Plugins.FindDLL(pluginName, index))
                    {
                        char buf2[MAX_PATH + 300];
                        sprintf(buf2, LoadStr(IDS_PLUGINEXISTS), Plugins.Get(index)->Name,
                                Plugins.Get(index)->DLLName);
                        //                add = SalMessageBox(HWindow, buf2, LoadStr(IDS_QUESTION),
                        //                                    MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES;
                        SalMessageBox(HWindow, buf2, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        add = FALSE;
                    }
                    if (add && Plugins.AddPlugin(HWindow, pluginName))
                    {
                        pluginAdded = TRUE;
                        int pluginIndex;
                        if (Plugins.FindDLL(pluginName, pluginIndex))
                            addedPlugin = Plugins.Get(pluginIndex);
                    }

                    // konec pokud najdeme dve nuly
                    if (off + 1 >= 2000 || *(fileName + off + 1) == 0)
                        break;
                }

                if (pluginAdded)
                {
                    if (Configuration.KeepPluginsSorted)
                        Plugins.UpdatePluginsOrder(TRUE);
                    // vlozime polozky a vybereme pridanou
                    RefreshListView(FALSE, -1, addedPlugin);
                }
            }
            return 0;
        }

        case IDB_PLUGINREMOVE:
        {
            RefreshPanels = TRUE;
            char name[MAX_PATH];
            int index, lvIndex;
            CPluginData* p = GetSelectedPlugin(&index, &lvIndex);
            if (p != NULL)
            {
                strcpy(name, p->Name);
                char buf[MAX_PATH + 100];
                sprintf(buf, LoadStr(IDS_PLUGINREMOVEOK), name);
                if (SalMessageBox(HWindow, buf, LoadStr(IDS_QUESTION),
                                  MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    Plugins.Remove(HWindow, index, TRUE);
                    RefreshListView(FALSE, lvIndex); // doslo k odstraneni DLL, mame novejsi data ...
                }
            }
            return 0;
        }

        case IDB_PLUGINTEST:
        {
            RefreshPanels = TRUE;
            CPluginData* p = GetSelectedPlugin();
            if (p != NULL)
            {
                if (p->InitDLL(HWindow))
                {
                    char buf[MAX_PATH + 100];
                    sprintf(buf, LoadStr(IDS_PLUGINTESTOK), p->Name);
                    SalMessageBox(HWindow, buf, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                }
                RefreshListView(); // doslo k loadu DLL, mame novejsi data ...
            }
            return 0;
        }

        case IDB_PLUGINTESTALL:
        {
            RefreshPanels = TRUE;
            if (Plugins.TestAll(HWindow)) // doslo k loadu aspon jednoho pluginu, mame novejsi data ...
            {
                RefreshListView();
            }
            return 0;
        }

        case IDB_PLUGINFOCUS:
        {
            CPluginData* p = GetSelectedPlugin();
#ifdef _WIN64 // FIXME_X64_WINSCP - tohle bude chtit asi poresit jinak... (ignorace chybejiciho WinSCP v x64 verzi Salama)
            char bufText[MAX_PATH + 200];
            if (p != NULL && IsPluginUnsupportedOnX64(p->DLLName))
            {
                // napiseme userovi, ze tenhle plugin je k mani jen v 32-bit verzi (x86)
                // IDS_PLUGINISX86ONLY neni idealni text, ale kaslu na to, svuj ucel to
                // splni a nebudeme zbytecne prudit prekladatele
                sprintf(bufText, LoadStr(IDS_PLUGINISX86ONLY), p->Name);
                SalMessageBox(HWindow, bufText, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                return 0;
            }
#endif // _WIN64
            if (p != NULL)
            {
                char buf[MAX_PATH];
                char* s = p->DLLName;
                if ((*s != '\\' || *(s + 1) != '\\') && // ne UNC
                    (*s == 0 || *(s + 1) != ':'))       // ne "c:" -> relativni cesta k podadresari plugins
                {
                    GetModuleFileName(HInstance, buf, MAX_PATH);
                    s = strrchr(buf, '\\') + 1;
                    strcpy(s, "plugins\\");
                    strcat(s, p->DLLName);
                    s = buf;
                }
                strcpy(FocusPlugin, s);
                PostMessage(HWindow, WM_COMMAND, IDOK, 0);
            }
            return 0;
        }

        case IDB_PLUGINUNLOAD:
        {
            RefreshPanels = TRUE;
            CPluginData* p = GetSelectedPlugin();
            if (p != NULL)
            {
                p->Unload(HWindow, TRUE);
                RefreshListView(); // doslo k loadu DLL, mame novejsi data ...
            }
            return 0;
        }
        }
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPluginKeys
//

void GetKeyName(UINT vk, char* buff)
{
    LONG scan = (LONG)MapVirtualKey(vk, 0) << 16;
    GetKeyNameText(scan, buff, 50);
}

void GetHotKeyText(WORD hotKey, char* buff)
{
    BYTE wVirtKey = LOBYTE(hotKey);
    BYTE wMods = HIBYTE(hotKey);
    buff[0] = 0;
    if (wVirtKey != 0 || wMods != 0)
    {
        const char* plus = "+";
        if (wMods & HOTKEYF_CONTROL)
        {
            GetKeyName(VK_CONTROL, buff);
            lstrcat(buff, plus);
        }
        if (wMods & HOTKEYF_SHIFT)
        {
            GetKeyName(VK_SHIFT, buff + lstrlen(buff));
            lstrcat(buff, plus);
        }
        if (wMods & HOTKEYF_ALT)
        {
            GetKeyName(VK_MENU, buff + lstrlen(buff));
            lstrcat(buff, plus);
        }
        GetKeyName(wVirtKey, buff + lstrlen(buff));
    }
}

CPluginKeys::CPluginKeys(HWND hParent, CPluginData* plugin)
    : CCommonDialog(HLanguage, IDD_PLUGINKEYS, IDD_PLUGINKEYS, hParent)
{
    HListView = NULL;
    Header = NULL;
    Plugin = plugin;
    Reset = FALSE;
    HotKeys = (DWORD*)malloc(sizeof(DWORD) * Plugin->MenuItems.Count);
    if (HotKeys == NULL)
        TRACE_E(LOW_MEMORY);
    else
    {
        int i;
        for (i = 0; i < Plugin->MenuItems.Count; i++)
            HotKeys[i] = Plugin->MenuItems[i]->HotKey;
    }
}

CPluginKeys::~CPluginKeys()
{
    if (HotKeys != NULL)
        free(HotKeys);
}

BOOL CPluginKeys::IsGood()
{
    return HotKeys != NULL;
}

void CPluginKeys::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
    {
        int i;
        for (i = 0; i < Plugin->MenuItems.Count; i++)
        {
            if (HotKeys[i] != 0)
            {
                WORD hotKey = HOTKEY_GET(HotKeys[i]);
                Plugins.RemoveHotKey(hotKey, NULL);        // ve vsech pluginech nechame sestrelit 'hotKey'
                Plugin->MenuItems[i]->HotKey = HotKeys[i]; // k nam ji priradime
            }
        }
    }
}

void CPluginKeys::InitColumns()
{
    CALL_STACK_MESSAGE1("CPluginKeys::InitColumns()");
    LV_COLUMN lvc;
    int header[2] = {IDS_PLUGIN_COMMAND, IDS_PLUGIN_KEY};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 2; i++) // vytvorim sloupce
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
    }
}

void CPluginKeys::SetColumnWidths()
{
    RECT r;
    GetClientRect(HListView, &r);
    ListView_SetColumnWidth(HListView, 0, (int)((double)r.right * 0.70));
    // posledni sloupec bude zbytkovy
    ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
}

void CPluginKeys::RefreshListView(BOOL setOnly)
{
    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);

    HIMAGELIST hIcons = ImageList_LoadBitmap(HInstance, MAKEINTRESOURCE(IDB_MENUITEMS), 13, 0, RGB(255, 0, 255)); // o destrukci se postara listview
    HIMAGELIST hOldIcons = ListView_SetImageList(HListView, hIcons, LVSIL_SMALL);
    if (hOldIcons != NULL)
        ImageList_Destroy(hOldIcons);

    if (!setOnly)
        ListView_DeleteAllItems(HListView);

    int row = 0;
    int level = 0; // odsazeni
    int i;
    for (i = 0; i < Plugin->MenuItems.Count; i++)
    {
        CPluginMenuItem* item = Plugin->MenuItems[i];
        if (item->Type == pmitEndSubmenu && level > 0)
            level--;
        if ((item->Type != pmitItemOrSeparator && item->Type != pmitStartSubmenu) || item->Name == NULL)
            continue;

        if (!setOnly)
        {
            LVITEM lvi;
            lvi.mask = LVIF_IMAGE | LVIF_TEXT | LVIF_INDENT | LVIF_PARAM;
            lvi.iImage = item->Type == pmitStartSubmenu ? 1 : 0;
            lvi.iItem = row;
            lvi.iSubItem = 0;
            char pszTextBuff[] = "";
            lvi.pszText = pszTextBuff;
            lvi.iIndent = level;
            lvi.lParam = i; // pro identifikaci
            ListView_InsertItem(HListView, &lvi);
        }
        // command name
        char buff[500];
        lstrcpyn(buff, item->Name, 500);
        RemoveAmpersands(buff);

        // pokud mame hint v textu, odstranime ho
        if ((item->HotKey & HOTKEY_HINT) != 0)
        {
            char* p = buff;
            while (*p != 0)
            {
                if (*p == '\t')
                {
                    *p = 0;
                    break;
                }
                p++;
            }
        }

        ListView_SetItemText(HListView, row, 0, buff);
        // shortcut key
        GetHotKeyText(LOWORD(HotKeys[i]), buff);
        ListView_SetItemText(HListView, row, 1, buff);
        row++;
        if (item->Type == pmitStartSubmenu)
            level++;
    }

    if (!setOnly)
    {
        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, 0, state, state);
        ListView_EnsureVisible(HListView, 0, FALSE);
    }

    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);
}

CPluginMenuItem*
CPluginKeys::GetItem(int index)
{
    LVITEM lvi;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    lvi.mask = LVIF_PARAM;
    ListView_GetItem(HListView, &lvi);
    int i = (int)lvi.lParam;
    if (i >= 0 && i < Plugin->MenuItems.Count)
        return Plugin->MenuItems[i];
    return NULL;
}

CPluginMenuItem*
CPluginKeys::GetSelectedItem(int* orgIndex)
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index == -1)
        return NULL;
    if (orgIndex != NULL)
    {
        LVITEM lvi;
        lvi.iItem = index;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_PARAM;
        ListView_GetItem(HListView, &lvi);
        *orgIndex = (int)lvi.lParam;
    }
    return GetItem(index);
}

WORD CPluginKeys::GetHotKey(BYTE* virtKey, BYTE* mods)
{
    WORD hotKey = (WORD)SendDlgItemMessage(HWindow, IDC_PLUGINKEY, HKM_GETHOTKEY, 0, 0);
    if (virtKey != NULL)
        *virtKey = LOBYTE(hotKey);
    if (mods != NULL)
        *mods = HIBYTE(hotKey);
    return hotKey;
}

void CPluginKeys::EnableButtons()
{
    // vytahneme selected polozku
    int orgIndex;
    CPluginMenuItem* item = GetSelectedItem(&orgIndex);
    BOOL keyAssigned = ((item != NULL) && (HotKeys[orgIndex] & HOTKEY_MASK) != 0);
    BOOL validItem = ((item != NULL) && (item->Type == pmitItemOrSeparator)); // separatory jsou odfiltrovane

    // vytahneme hotkey
    BYTE virtKey;
    WORD hotKey = GetHotKey(&virtKey, NULL);
    BOOL valiKey = (virtKey != 0) && !IsSalHotKey(hotKey);

    BOOL changeFocus = FALSE;
    HWND focus = GetFocus();

    EnableWindow(GetDlgItem(HWindow, IDC_PLUGINKEY), validItem);
    if (GetDlgItem(HWindow, IDC_ASSIGN) == focus && !(validItem && valiKey))
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDC_ASSIGN), validItem && valiKey);
    if (GetDlgItem(HWindow, IDC_REMOVE) == focus && !(validItem && keyAssigned))
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDC_REMOVE), validItem && keyAssigned);

    if (changeFocus)
    {
        PostMessage(focus, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        focus = GetDlgItem(HWindow, IDOK);
        PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        SetFocus(HListView);
    }
}

void CPluginKeys::HandleConflictWarning()
{
    char buff[500];
    buff[0] = 0;

    BYTE virtKey;
    WORD hotKey = GetHotKey(&virtKey);
    if (virtKey != 0)
    {
        // nepatri hot key Salamu?
        if (IsSalHotKey(hotKey))
        {
            strcpy(buff, LoadStr(IDS_HOTKEY_SAL_CONFLICT));
        }

        // hledame u nas
        if (buff[0] == 0)
        {
            int i;
            for (i = 0; i < Plugin->MenuItems.Count; i++)
            {
                if (HOTKEY_GET(HotKeys[i]) == hotKey)
                {
                    sprintf(buff, LoadStr(IDS_HOTKEY_PLUGIN_CONFLICT), Plugin->Name);
                    break;
                }
            }
        }

        // hledame u ostatnich pluginu
        if (buff[0] == 0)
        {
            int pluginIndex;
            int menuItemIndex;
            if (Plugins.FindHotKey(hotKey, TRUE, Plugin, &pluginIndex, &menuItemIndex))
            {
                CPluginData* plugin = Plugins.Get(pluginIndex);
                sprintf(buff, LoadStr(IDS_HOTKEY_PLUGIN_CONFLICT), plugin->Name);
            }
        }
    }
    SetDlgItemText(HWindow, IDC_CONFLICT_WARNING, buff);
}

INT_PTR
CPluginKeys::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPluginKeys::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // dialog box title
        char buff[500];
        char buff2[500];
        GetWindowText(HWindow, buff, 500);
        sprintf(buff2, buff, Plugin->Name);
        SetWindowText(HWindow, buff2);

        // listview setup
        HListView = GetDlgItem(HWindow, IDL_COMMANDS);
        ListView_SetExtendedListViewStyleEx(HListView, LVS_EX_FULLROWSELECT, LVS_EX_FULLROWSELECT);

        // header line
        Header = new CToolbarHeader(HWindow, IDC_PLUGINHEADER, HListView, 0);
        if (Header == NULL)
            TRACE_E(LOW_MEMORY);

        // vlozime sloupce
        InitColumns();

        // vlozime polozky
        RefreshListView(FALSE);

        // nastavime sirky sloupcu
        SetColumnWidths();

        EnableButtons();
        HandleConflictWarning();

        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;
    }

    case WM_SYSCOMMAND:
    {
        // nechceme pipani pri zadavani klaves jako je Alt+Shift+Z (napriklad)
        if (wParam == SC_KEYMENU && GetFocus() == GetDlgItem(HWindow, IDC_PLUGINKEY))
        {
            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, 0);
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_PLUGINKEY:
        {
            if (HIWORD(wParam) == EN_CHANGE)
            {
                EnableButtons();
                HandleConflictWarning();
            }
            break;
        }

        case IDC_ASSIGN:
        {
            int orgIndex;
            CPluginMenuItem* item = GetSelectedItem(&orgIndex);
            WORD hotKey = GetHotKey();
            // sestrelime redundance
            int i;
            for (i = 0; i < Plugin->MenuItems.Count; i++)
                if (HOTKEY_GET(HotKeys[i]) == hotKey)
                    HotKeys[i] = 0;
            HotKeys[orgIndex] = (HotKeys[orgIndex] & ~HOTKEY_MASK) | HOTKEY_DIRTY; // nechceme, aby tuto zmenu prevalcoval Connect() pluginu
            HotKeys[orgIndex] |= (DWORD)hotKey;
            SendDlgItemMessage(HWindow, IDC_PLUGINKEY, HKM_SETHOTKEY, 0, 0);
            EnableButtons();
            HandleConflictWarning();
            RefreshListView(TRUE);
            return 0;
        }

        case IDC_REMOVE:
        {
            int orgIndex;
            CPluginMenuItem* item = GetSelectedItem(&orgIndex);
            BOOL keyAssigned = ((item != NULL) && (HotKeys[orgIndex] & HOTKEY_MASK) != 0);
            if (keyAssigned)
            {
                HotKeys[orgIndex] = (HotKeys[orgIndex] & ~HOTKEY_MASK) | HOTKEY_DIRTY; // nechceme, aby tuto zmenu prevalcoval Connect() pluginu
                EnableButtons();
                HandleConflictWarning();
                RefreshListView(TRUE);
            }
            return 0;
        }

        case IDC_RESET:
        {
            char buf[MAX_PATH + 100];
            sprintf(buf, LoadStr(IDS_PLUGINRESETKEYS), Plugin->Name);
            if (SalMessageBox(HWindow, buf, LoadStr(IDS_INFOTITLE), MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
            {
                int i;
                for (i = 0; i < Plugin->MenuItems.Count; i++)
                    Plugin->MenuItems[i]->HotKey = 0; // postrilime horke klavesy, pristi connect je obnovi
                Reset = TRUE;
                PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            }
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDL_COMMANDS)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case LVN_ITEMCHANGED:
            {
                int i;
                for (i = 0; i < ListView_GetItemCount(HListView); i++)
                {
                    CPluginMenuItem* item = GetItem(i);
                    if (item == NULL || item->Type == pmitStartSubmenu)
                    {
                        // sestrelime SELECTION
                        ListView_SetItemState(HListView, i, 0, LVIS_SELECTED);
                    }
                }

                EnableButtons();
                return 0;
            }
            }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CArchiveUpdateDlg
//

CArchiveUpdateDlg::CArchiveUpdateDlg(HWND hParent, CFileTimeStamps* fileStamps, CFilesWindow* panel)
    : CCommonDialog(HLanguage, IDD_ARCHIVEUPDATE, IDD_ARCHIVEUPDATE, hParent)
{
    FileStamps = fileStamps;
    Panel = panel;
}

void CArchiveUpdateDlg::EnableButtons()
{
    int sel = (int)SendMessage(GetDlgItem(HWindow, IDL_UPDATEDFILES), LB_GETSELCOUNT, 0, 0);

    HWND focus = GetFocus();
    BOOL changeFocus = FALSE;
    if (GetDlgItem(HWindow, IDB_COPYSELTO) == focus && sel == 0)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_COPYSELTO), sel != 0);
    if (GetDlgItem(HWindow, IDB_IGNORESEL) == focus && sel == 0)
        changeFocus = TRUE;
    EnableWindow(GetDlgItem(HWindow, IDB_IGNORESEL), sel != 0);
    if (changeFocus)
    {
        PostMessage(focus, BM_SETSTYLE, BS_PUSHBUTTON, TRUE);
        focus = GetDlgItem(HWindow, IDOK);
        PostMessage(focus, BM_SETSTYLE, BS_DEFPUSHBUTTON, TRUE);
        SetFocus(focus);
    }
}

INT_PTR
CArchiveUpdateDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CArchiveUpdateDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetDlgItemText(HWindow, IDT_ARCHIVENAME, FileStamps->GetZIPFile());
        HWND list = GetDlgItem(HWindow, IDL_UPDATEDFILES);
        FileStamps->AddFilesToListBox(list);
        SendMessage(list, LB_SETSEL, TRUE, -1);
        PostMessage(HWindow, WM_COMMAND, MAKELONG(IDL_UPDATEDFILES, LBN_SELCHANGE), (LPARAM)list);
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDL_UPDATEDFILES:
        {
            if (HIWORD(wParam) == LBN_SELCHANGE)
            {
                EnableButtons();
            }
            break;
        }

        case IDCANCEL:
        {
            if (SendMessage(GetDlgItem(HWindow, IDL_UPDATEDFILES), LB_GETCOUNT, 0, 0) != 0)
            { // jen pokud jsou nejake soubory v listboxu
                if (SalMessageBox(HWindow, LoadStr(IDS_ARCREALLYIGNOREALL), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION | MB_DEFBUTTON2) != IDYES)
                {
                    return 0; // cancel of cancel
                }
            }
            break;
        }

        case IDB_COPYSELTO:
        {
            HWND list = GetDlgItem(HWindow, IDL_UPDATEDFILES);
            int selCount = (int)SendMessage(list, LB_GETSELCOUNT, 0, 0);
            if (selCount > 0)
            {
                int* indexes = new int[selCount];
                if (indexes != NULL)
                {
                    SendMessage(list, LB_GETSELITEMS, selCount, (LPARAM)indexes);
                    IntSort(indexes, 0, selCount - 1); // indexy zrejme nemusi byt razene, proto radsi seradime

                    char path[MAX_PATH];
                    char* initPath;
                    strcpy(path, FileStamps->GetZIPFile());
                    if (!CutDirectory(path))
                        initPath = NULL;
                    else
                        initPath = path;
                    if (Panel->CheckPath(TRUE, initPath) != ERROR_SUCCESS)
                        initPath = NULL;

                    // nechame vybrane soubory zkopirovat
                    FileStamps->CopyFilesTo(HWindow, indexes, selCount, initPath);

                    delete indexes;
                }
                else
                    TRACE_E(LOW_MEMORY);
            }
            return 0;
        }

        case IDB_IGNORESEL:
        {
            HWND list = GetDlgItem(HWindow, IDL_UPDATEDFILES);
            int selCount = (int)SendMessage(list, LB_GETSELCOUNT, 0, 0);
            if (selCount > 0)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_ARCREALLYIGNORESEL), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
                {
                    int* indexes = new int[selCount];
                    if (indexes != NULL)
                    {
                        SendMessage(list, LB_GETSELITEMS, selCount, (LPARAM)indexes);
                        IntSort(indexes, 0, selCount - 1);     // indexy zrejme nemusi byt razene, proto radsi seradime
                        FileStamps->Remove(indexes, selCount); // odstranime vybrane soubory

                        // naplnime znovu listbox
                        SendMessage(list, LB_RESETCONTENT, 0, 0);
                        FileStamps->AddFilesToListBox(list);
                        PostMessage(HWindow, WM_COMMAND, MAKELONG(IDL_UPDATEDFILES, LBN_SELCHANGE), (LPARAM)list);

                        if (SendMessage(list, LB_GETCOUNT, 0, 0) == 0) // bylo to "ignore all"
                        {
                            PostMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
                        }

                        delete indexes;
                    }
                    else
                        TRACE_E(LOW_MEMORY);
                }
            }
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageConfirmations
//

// slouzi pouze na potlaceni quick searche v treeview
class CMyTreeView : public CWindow
{
public:
    CMyTreeView(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        if (uMsg == WM_CHAR)
        {
            PostMessage(GetParent(HWindow), WM_USER_CHAR, wParam, lParam);
            return 0;
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

CCfgPageConfirmations::CCfgPageConfirmations()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_CONFIRMATIONS, IDD_CFGPAGE_CONFIRMATIONS, PSP_USETITLE, NULL),
      List(20, 5)
{
    HTreeView = NULL;
    DisableNotification = FALSE;
}

void CCfgPageConfirmations::SetItemChecked(HTREEITEM hTreeItem, BOOL checked)
{
    TVITEM item;
    item.mask = TVIF_HANDLE | TVIF_IMAGE | TVIF_SELECTEDIMAGE;
    item.hItem = hTreeItem;
    item.iImage = checked ? 1 : 0;
    item.iSelectedImage = item.iImage;
    TreeView_SetItem(HTreeView, &item);
}

//BOOL
//CCfgPageConfirmations::GetItemChecked(HTREEITEM hTreeItem)
//{
//  TVITEM item;
//  item.mask = TVIF_HANDLE | TVIF_IMAGE;
//  item.hItem = hTreeItem;
//  TreeView_GetItem(HTreeView, &item);
//  return item.iImage = 1;
//}

void CCfgPageConfirmations::Transfer(CTransferInfo& ti)
{
    DisableNotification = TRUE;
    int i;
    for (i = 0; i < List.Count; i++)
    {
        CConfirmationItem* cnfrm = &List[i];
        if (ti.Type == ttDataToWindow)
        {
            cnfrm->Checked = *cnfrm->Variable;
            SetItemChecked(cnfrm->HTreeItem, cnfrm->Checked);
        }
        else
        {
            *cnfrm->Variable = cnfrm->Checked;
        }
    }
    DisableNotification = FALSE;
}

HTREEITEM
CCfgPageConfirmations::AddItem(HTREEITEM hParent, int iImage, int textResID, int* value)
{
    TVINSERTSTRUCT tvis;
    tvis.hParent = hParent;
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT | TVIF_STATE; // | TVIF_PARAM;
    tvis.item.state = 0;
    if (iImage != -1)
    {
        tvis.item.mask |= TVIF_IMAGE | TVIF_SELECTEDIMAGE;
        tvis.item.iImage = iImage;
        tvis.item.iSelectedImage = iImage;
        tvis.item.state |= TVIS_EXPANDED;
    }

    tvis.item.pszText = LoadStr(textResID);
    tvis.item.stateMask = tvis.item.state;

    HTREEITEM ret = TreeView_InsertItem(HTreeView, &tvis);
    if (iImage == -1)
    {
        CConfirmationItem item;
        item.HTreeItem = ret;
        item.Variable = value;
        item.Checked = 0;
        List.Add(item); // handly na listy nahazim do pole pro snadny pristup
    }
    return ret;
}

void CCfgPageConfirmations::InitTree()
{
    // Confirm On
    HConfirmOn = AddItem(NULL, 2, IDS_CNFRM_CONFIRMON, NULL);
    HTREEITEM hFirst = AddItem(HConfirmOn, -1, IDS_CNFRM_FILEDIRDEL, &Configuration.CnfrmFileDirDel);
    AddItem(HConfirmOn, -1, IDS_CNFRM_NEDIRDEL, &Configuration.CnfrmNEDirDel);
    AddItem(HConfirmOn, -1, IDS_CNFRM_FILEOVER, &Configuration.CnfrmFileOver);
    AddItem(HConfirmOn, -1, IDS_CNFRM_SHFILEDEL, &Configuration.CnfrmSHFileDel);
    AddItem(HConfirmOn, -1, IDS_CNFRM_SHDIRDEL, &Configuration.CnfrmSHDirDel);
    AddItem(HConfirmOn, -1, IDS_CNFRM_SHFILEOVER, &Configuration.CnfrmSHFileOver);
    AddItem(HConfirmOn, -1, IDS_CNFRM_DIRALREADYEXISTS, &Configuration.CnfrmDirOver);
    AddItem(HConfirmOn, -1, IDS_CNFRM_NTFSPRESS, &Configuration.CnfrmNTFSPress);
    AddItem(HConfirmOn, -1, IDS_CNFRM_NTFSCRYPT, &Configuration.CnfrmNTFSCrypt);
    AddItem(HConfirmOn, -1, IDS_CNFRM_DAD, &Configuration.CnfrmDragDrop);
    AddItem(HConfirmOn, -1, IDS_CNFRM_UMDIFFDLG, &Configuration.CnfrmShowNamesToCompare);
    AddItem(HConfirmOn, -1, IDS_CNFRM_DSTIGNORED, &Configuration.CnfrmDSTShiftsIgnored);
    AddItem(HConfirmOn, -1, IDS_CNFRM_DSTCANIGN, &Configuration.CnfrmDSTShiftsOccured);

    // Show message
    HShowMessage = AddItem(NULL, 3, IDS_CNFRM_SHOWMESSAGE, NULL);
    AddItem(HShowMessage, -1, IDS_CNFRM_CLOSEARCHIVE, &Configuration.CnfrmCloseArchive);
    AddItem(HShowMessage, -1, IDS_CNFRM_CLOSEFIND, &Configuration.CnfrmCloseFind);
    AddItem(HShowMessage, -1, IDS_CNFRM_STOPFIND, &Configuration.CnfrmStopFind);
    AddItem(HShowMessage, -1, IDS_CNFRM_CREATEPATH, &Configuration.CnfrmCreatePath);
    AddItem(HShowMessage, -1, IDS_CNFRM_ALWAYSONTOP, &Configuration.CnfrmAlwaysOnTop);
    AddItem(HShowMessage, -1, IDS_CNFRM_ONSALCLOSE, &Configuration.CnfrmOnSalClose);
    AddItem(HShowMessage, -1, IDS_CNFRM_ONSENDEMAIL, &Configuration.CnfrmSendEmail);
    AddItem(HShowMessage, -1, IDS_CNFRM_ONADDTOARCHIVE, &Configuration.CnfrmAddToArchive);
    AddItem(HShowMessage, -1, IDS_CNFRM_ONCREATEDIR, &Configuration.CnfrmCreateDir);
    AddItem(HShowMessage, -1, IDS_CNFRM_COPYMOVEOPTNS, &Configuration.CnfrmCopyMoveOptionsNS);

    // select na prvni pouzitelnou polozku
    TreeView_Select(HTreeView, hFirst, TVGN_CARET);
}

HIMAGELIST
CCfgPageConfirmations::CreateImageList()
{
    int iconSize = IconSizes[ICONSIZE_16];
    HIMAGELIST hIL = CreateCheckboxImagelist(iconSize);

    HICON hIcon;
    LoadIconWithScaleDown(NULL, (PCWSTR)IDI_EXCLAMATION, iconSize, iconSize, &hIcon);
    ImageList_AddIcon(hIL, hIcon);
    DestroyIcon(hIcon);

    LoadIconWithScaleDown(NULL, (PCWSTR)IDI_QUESTION, iconSize, iconSize, &hIcon);
    ImageList_AddIcon(hIL, hIcon);
    DestroyIcon(hIcon);

    return hIL;
}

int CCfgPageConfirmations::FindInList(HTREEITEM hTreeItem)
{
    int i;
    for (i = 0; i < List.Count; i++)
    {
        if (List[i].HTreeItem == hTreeItem)
            return i;
    }
    return -1;
}

void CCfgPageConfirmations::ChangeSelected()
{
    HTREEITEM hSelected = TreeView_GetSelection(HTreeView);
    if (hSelected != NULL)
    {
        int index = FindInList(hSelected);
        if (index != -1)
        {
            List[index].Checked = !List[index].Checked;
            SetItemChecked(hSelected, List[index].Checked);
        }
    }
}

INT_PTR
CCfgPageConfirmations::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CMyTreeView* treeView = new CMyTreeView(HWindow, IDC_CNFRM_TREE);
        HTreeView = treeView->HWindow;

        // DWORD style = GetWindowLongPtr(HTreeView, GWL_STYLE);
        // style |= TVS_CHECKBOXES;
        // SetWindowLongPtr(HTreeView, GWL_STYLE, style);

        HImageList = CreateImageList();
        TreeView_SetImageList(HTreeView, HImageList, TVSIL_NORMAL);
        InitTree();

        // prvky dialogu se maji natahovat podle jeho velikosti, nastavime delici controly
        ElasticVerticalLayout(1, IDC_CNFRM_TREE);

        break;
    }

    case WM_DESTROY:
    {
        // podle MSDN TreeView image list nedestrukti, ale Checked Build W2K rve
        // behem nasledneho volani ImageList_Destroy, takze image list pro jistoru odeberem
        if (HTreeView != NULL)
            TreeView_SetImageList(HTreeView, NULL, TVSIL_NORMAL);
        if (HImageList != NULL)
        {
            ImageList_Destroy(HImageList);
            HImageList = NULL;
        }
        break;
    }

    // space na polozce
    case WM_USER_CHAR:
    {
        if (wParam == ' ')
            ChangeSelected();
        break;
    }

    case WM_NOTIFY:
    {
        if (DisableNotification)
            break;

        if (wParam == IDC_CNFRM_TREE)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            // kliknuti mysi na nevybranou polozku -> toggle checkbox
            case TVN_SELCHANGED:
            {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                if (pnmtv->action == TVC_BYMOUSE)
                    ChangeSelected();
                break;
            }

            // kliknuti mysi na vybranou polozku -> toggle checkbox
            case NM_CLICK:
            case NM_DBLCLK:
            {
                HTREEITEM hSelected = TreeView_GetSelection(HTreeView);
                if (hSelected != NULL)
                {
                    DWORD pos = GetMessagePos();
                    TVHITTESTINFO hti;
                    hti.pt.x = GET_X_LPARAM(pos);
                    hti.pt.y = GET_Y_LPARAM(pos);
                    ScreenToClient(HTreeView, &hti.pt);
                    HTREEITEM hHit = TreeView_HitTest(HTreeView, &hti);
                    if (hHit == hSelected && (hti.flags & TVHT_ONITEM) != 0)
                        ChangeSelected();
                }
                break;
            }
            }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageDrives
//

CCfgPageDrives::CCfgPageDrives(BOOL focusIfPathIsInaccessibleGoTo)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_DRIVES, IDD_CFGPAGE_DRIVES, PSP_USETITLE, NULL)
{
    IfPathIsInaccessibleGoToChanged = FALSE;
    FocusIfPathIsInaccessibleGoTo = focusIfPathIsInaccessibleGoTo;
}

void CCfgPageDrives::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_DRVSPEC_FLOPPYMON, Configuration.DrvSpecFloppyMon);
    ti.CheckBox(IDC_DRVSPEC_FLOPPYSIMPLE, Configuration.DrvSpecFloppySimple);
    ti.CheckBox(IDC_DRVSPEC_REMOVABLEMON, Configuration.DrvSpecRemovableMon);
    ti.CheckBox(IDC_DRVSPEC_REMOVABLESIMPLE, Configuration.DrvSpecRemovableSimple);
    ti.CheckBox(IDC_DRVSPEC_FIXEDMON, Configuration.DrvSpecFixedMon);
    ti.CheckBox(IDC_DRVSPEC_FIXEDSIMPLE, Configuration.DrvSpecFixedSimple);
    ti.CheckBox(IDC_DRVSPEC_CDROMMON, Configuration.DrvSpecCDROMMon);
    ti.CheckBox(IDC_DRVSPEC_CDROMSIMPLE, Configuration.DrvSpecCDROMSimple);
    ti.CheckBox(IDC_DRVSPEC_REMOTEMON, Configuration.DrvSpecRemoteMon);
    ti.CheckBox(IDC_DRVSPEC_REMOTESIMPLE, Configuration.DrvSpecRemoteSimple);
    ti.CheckBox(IDC_DRVSPEC_REMOTEACT, Configuration.DrvSpecRemoteDoNotRefreshOnAct);
    char path[MAX_PATH];
    char newPath[MAX_PATH];
    if (ti.Type == ttDataToWindow)
    {
        GetIfPathIsInaccessibleGoTo(path);
        ti.EditLine(IDE_DRVSPEC_ONERRGOTO, path, MAX_PATH);
        IfPathIsInaccessibleGoToChanged = FALSE;
    }
    else
    {
        if (IfPathIsInaccessibleGoToChanged) // menime jen pokud user skutecne editoval cestu
        {
            ti.EditLine(IDE_DRVSPEC_ONERRGOTO, newPath, MAX_PATH);
            GetIfPathIsInaccessibleGoTo(path, TRUE);
            if (IsTheSamePath(path, newPath)) // user chce chodit do my-documents
            {
                Configuration.IfPathIsInaccessibleGoToIsMyDocs = TRUE;
                Configuration.IfPathIsInaccessibleGoTo[0] = 0;
            }
            else
            {
                Configuration.IfPathIsInaccessibleGoToIsMyDocs = FALSE;
                lstrcpyn(Configuration.IfPathIsInaccessibleGoTo, newPath, MAX_PATH);
            }
        }
    }
}

INT_PTR
CCfgPageDrives::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        // strasna prasarna - potrebuju nejakou message, kter prijde
        // po WM_INITDIALOG, abych mohl nastavit focus
        // do pristi verze Salama to tu snad vydrzi :-)
        if (FocusIfPathIsInaccessibleGoTo)
        {
            SendMessage(HWindow, WM_NEXTDLGCTL, (WPARAM)GetDlgItem(HWindow, IDE_DRVSPEC_ONERRGOTO), TRUE);
            FocusIfPathIsInaccessibleGoTo = FALSE;
        }
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_BROWSECOMMAND)
        {
            char path[MAX_PATH];
            GetDlgItemText(HWindow, IDE_DRVSPEC_ONERRGOTO, path, MAX_PATH);
            if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_BROWSEONERRGOTOTITLE),
                                   LoadStr(IDS_BROWSEONERRGOTOTEXT), path, FALSE, path))
            {
                SetDlgItemText(HWindow, IDE_DRVSPEC_ONERRGOTO, path);
            }
        }
        if (HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDE_DRVSPEC_ONERRGOTO)
            IfPathIsInaccessibleGoToChanged = TRUE;
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageViewEdit
//

CCfgPageViewEdit::CCfgPageViewEdit()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_VIEWEDIT /*, prazdna stranka nema help */, PSP_USETITLE, NULL)
{
}

//
// ****************************************************************************
// CCfgPageViewers
//

CCfgPageViewers::CCfgPageViewers(BOOL alternative)
    : CCommonPropSheetPage(alternative ? LoadStr(IDS_ALTVIEWERS) : NULL, HLanguage,
                           IDD_CFGPAGE_VIEWERS, IDD_CFGPAGE_VIEWERS, PSP_USETITLE, NULL),
      ViewerMasks(10, 5)
{
    Alternative = alternative;
    SourceViewerMasks = Alternative ? MainWindow->AltViewerMasks : MainWindow->ViewerMasks;
    ViewerMasks.Load(*SourceViewerMasks);
    DisableNotification = FALSE;
    EditLB = NULL;
}

void CCfgPageViewers::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageViewers::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        Dirty = FALSE;
        // naleju combo s viewerama
        HWND hCombo = GetDlgItem(HWindow, IDC_VIEW_TYPE);
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_VIEWER_EXTERNAL));
        SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_VIEWER_INTERNAL));
        int count = 0;
        int index;
        while ((index = Plugins.GetViewerIndex(count++)) != -1) // dokud existuji "file viewer" plug-iny
        {
            CPluginData* p = Plugins.Get(index);
            if (p != NULL)
            {
                char buf[MAX_PATH];
                p->GetDisplayName(buf, MAX_PATH);
                SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buf);
            }
            else
                TRACE_E("Unexpected situation in CCfgPageViewers::Transfer().");
        }

        // naleju seznam vieweru
        int i;
        for (i = 0; i < ViewerMasks.Count; i++)
            EditLB->AddItem((INT_PTR)ViewerMasks[i]);
        DisableNotification = TRUE;
        EditLB->SetCurSel(0);
        DisableNotification = FALSE;
        LoadControls();
        EnableControls();
    }
    else
    {
        MainWindow->EnterViewerMasksCS();
        SourceViewerMasks->Load(ViewerMasks);
        MainWindow->LeaveViewerMasksCS();
    }
}

void CCfgPageViewers::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageViewers::Validate()");
    if (Dirty)
    {
        int i;
        for (i = 0; i < ViewerMasks.Count; i++)
        {
            CMaskGroup masks(ViewerMasks[i]->Masks->GetMasksString());
            int errorPos1, errorPos2;
            const char* forbiddenChar = strchr(masks.GetMasksString(), '|');
            if (forbiddenChar != NULL || !masks.PrepareMasks(errorPos1))
            {
                if (forbiddenChar != NULL)
                    errorPos1 = (int)(forbiddenChar - masks.GetMasksString());
                EditLB->SetCurSel(i);
                SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDL_FILEMASKS);
                PostMessage(HWindow, WM_USER_EDIT, errorPos1, errorPos1 + 1);
                return;
            }

            CViewerMasksItem* item = ViewerMasks[i];
            if (item->ViewerType == VIEWER_EXTERNAL)
            {
                if (!ValidateCommandFile(HWindow, item->Command, errorPos1, errorPos2))
                {
                    EditLB->SetCurSel(i);
                    ti.ErrorOn(IDE_COMMAND);
                    PostMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_SETSEL,
                                errorPos1, errorPos2);
                    return;
                }
                if (!ValidateArguments(HWindow, item->Arguments, errorPos1, errorPos2))
                {
                    EditLB->SetCurSel(i);
                    ti.ErrorOn(IDE_ARGUMENTS);
                    PostMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL,
                                errorPos1, errorPos2);
                    return;
                }
                if (!ValidateInitDir(HWindow, item->InitDir, errorPos1, errorPos2))
                {
                    EditLB->SetCurSel(i);
                    ti.ErrorOn(IDE_INITDIR);
                    PostMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL,
                                errorPos1, errorPos2);
                    return;
                }
            }
        }
    }
}

void CCfgPageViewers::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageViewers::LoadControls()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    BOOL empty = FALSE;
    if (itemID == -1)
        empty = TRUE;

    CViewerMasksItem* item = NULL;
    if (!empty)
        item = (CViewerMasksItem*)itemID;
    DisableNotification = TRUE;

    int type = item == NULL ? 2 : item->ViewerType;
    int cmbSel = -1;
    switch (type)
    {
    case VIEWER_EXTERNAL:
        cmbSel = 0;
        break;
    case VIEWER_INTERNAL:
        cmbSel = 1;
        break;

    default:
    {
        if (type < 0)
        {
            cmbSel = Plugins.GetViewerCount(-type - 1);
            if (cmbSel != -1)
                cmbSel += 2;
        }
    }
    }
    SendDlgItemMessage(HWindow, IDC_VIEW_TYPE, CB_SETCURSEL, cmbSel, 0);
    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->Command));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->Arguments));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL, 0, -1); // aby browse prepsal obsah
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->InitDir));
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL, 0, -1); // aby browse prepsal obsah
    DisableNotification = FALSE;
}

void CCfgPageViewers::StoreControls()
{
    CALL_STACK_MESSAGE1("CCfgPageViewers::StoreControls()");
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        Dirty = TRUE;
        CViewerMasksItem* item = ViewerMasks[index];

        char command[MAX_PATH];
        char arguments[MAX_PATH];
        char initdir[MAX_PATH];
        SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_GETTEXT,
                    MAX_PATH, (LPARAM)command);
        SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_GETTEXT,
                    MAX_PATH, (LPARAM)arguments);
        SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_GETTEXT,
                    MAX_PATH, (LPARAM)initdir);
        item->Set(item->Masks->GetMasksString(), command, arguments, initdir);

        int cmbSel = (int)SendDlgItemMessage(HWindow, IDC_VIEW_TYPE, CB_GETCURSEL, 0, 0);
        int type;
        switch (cmbSel)
        {
        case 0:
            type = VIEWER_EXTERNAL;
            break;
        case 1:
            type = VIEWER_INTERNAL;
            break;

        default:
        {
            type = Plugins.GetViewerIndex(cmbSel - 2);
            if (type != -1)
                type = -type - 1;
            else
            {
                TRACE_E("Unexpected situation in CCfgPageViewers::StoreControls().");
                type = VIEWER_INTERNAL;
            }
            break;
        }
        }
        item->ViewerType = type;
    }
}

void CCfgPageViewers::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageViewers::EnableControls()");
    BOOL validItem = TRUE;
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (itemID == -1)
        validItem = FALSE;

    EnableWindow(GetDlgItem(HWindow, IDC_VIEW_TYPE), validItem);

    int type = (int)SendDlgItemMessage(HWindow, IDC_VIEW_TYPE, CB_GETCURSEL, 0, 0);
    BOOL external = (type == VIEWER_EXTERNAL);
    EnableWindow(GetDlgItem(HWindow, IDE_COMMAND), validItem & external);
    EnableWindow(GetDlgItem(HWindow, IDE_ARGUMENTS), validItem & external);
    EnableWindow(GetDlgItem(HWindow, IDE_INITDIR), validItem & external);

    EnableWindow(GetDlgItem(HWindow, IDB_BROWSECOMMAND), validItem & external);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEARGUMENTS), validItem & external);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEINITDIR), validItem & external);
}

INT_PTR
CCfgPageViewers::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageViewers::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDL_FILEMASKS);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            EditLB->MakeHeader(IDS_MASKSHDR);
            EditLB->EnableDrag(::GetParent(HWindow));
        }
        ChangeToArrowButton(HWindow, IDB_BROWSECOMMAND);
        ChangeToArrowButton(HWindow, IDB_BROWSEARGUMENTS);
        ChangeToArrowButton(HWindow, IDB_BROWSEINITDIR);

        // prvky dialogu se maji natahovat podle jeho velikosti, nastavime delici controly
        ElasticVerticalLayout(1, IDL_FILEMASKS);

        break;
    }

    case WM_USER_EDIT:
    {
        SetFocus(GetDlgItem(HWindow, IDL_FILEMASKS));
        EditLB->OnBeginEdit((int)wParam, (int)lParam);
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE || (LOWORD(wParam) == IDC_VIEW_TYPE && HIWORD(wParam) == CBN_SELCHANGE))
        {
            EnableControls();
            StoreControls();
        }

        if (LOWORD(wParam) == IDL_FILEMASKS && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            LoadControls();
            EnableControls();
        }

        switch (LOWORD(wParam))
        {
        case IDB_BROWSECOMMAND:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSECOMMAND, IDE_COMMAND, FALSE,
                             CommandExecutes, IDS_EXEFILTER);
            return 0;
        }

        case IDB_BROWSEARGUMENTS:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEARGUMENTS, IDE_ARGUMENTS, FALSE,
                             ArgumentsExecutes);
            return 0;
        }

        case IDB_BROWSEINITDIR:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEINITDIR, IDE_INITDIR, FALSE,
                             InitDirExecutes);
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        /*
      LRESULT result;
      if (EditLB->OnWMNotify(lParam, result)) 
      {
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, result);
        return 0;
      }
*/
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDL_FILEMASKS:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    lstrcpyn(dispInfo->Buffer, ((CViewerMasksItem*)dispInfo->ItemID)->Masks->GetMasksString(), MAX_PATH);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    Dirty = TRUE;
                    CViewerMasksItem* item;
                    if (dispInfo->ItemID == -1)
                    {
                        item = new CViewerMasksItem();
                        if (item == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        ViewerMasks.Add(item);
                        item->Set(dispInfo->Buffer, item->Command, item->Arguments, item->InitDir);
                        EditLB->SetItemData((INT_PTR)item);
                    }
                    else
                    {
                        item = (CViewerMasksItem*)dispInfo->ItemID;
                        item->Set(dispInfo->Buffer, item->Command, item->Arguments, item->InitDir);
                    }

                    LoadControls();
                    EnableControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*
            case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              int srcIndex = index;
              int dstIndex = index + (dispInfo->Up ? -1 : 1);

              char buf[sizeof(CViewerMasksItem)];
              memcpy(buf, ViewerMasks[srcIndex], sizeof(CViewerMasksItem));
              memcpy(ViewerMasks[srcIndex], ViewerMasks[dstIndex], sizeof(CViewerMasksItem));
              memcpy(ViewerMasks[dstIndex], buf, sizeof(CViewerMasksItem));

              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
              return TRUE;
            }
*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                int srcIndex = index;
                int dstIndex = dispInfo->NewIndex;

                char buf[sizeof(CViewerMasksItem)];
                memcpy(buf, ViewerMasks[srcIndex], sizeof(CViewerMasksItem));
                if (srcIndex < dstIndex)
                {
                    int i;
                    for (i = srcIndex; i < dstIndex; i++)
                        memcpy(ViewerMasks[i], ViewerMasks[i + 1], sizeof(CViewerMasksItem));
                }
                else
                {
                    int i;
                    for (i = srcIndex; i > dstIndex; i--)
                        memcpy(ViewerMasks[i], ViewerMasks[i - 1], sizeof(CViewerMasksItem));
                }
                memcpy(ViewerMasks[dstIndex], buf, sizeof(CViewerMasksItem));

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolime zmenu
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                ViewerMasks.Delete(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolim smazani
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDL_FILEMASKS)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageEditors
//

CCfgPageEditors::CCfgPageEditors()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_EDITORS, IDD_CFGPAGE_EDITORS, PSP_USETITLE, NULL),
      EditorMasks(10, 5)
{
    SourceEditorMasks = MainWindow->EditorMasks;
    EditorMasks.Load(*SourceEditorMasks);
    DisableNotification = FALSE;
    EditLB = NULL;
}

void CCfgPageEditors::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageEditors::Transfer()");
    if (ti.Type == ttDataToWindow)
    {
        Dirty = FALSE;
        int i;
        for (i = 0; i < EditorMasks.Count; i++)
            EditLB->AddItem((INT_PTR)EditorMasks[i]);
        EditLB->SetCurSel(0);
        LoadControls();
        EnableControls();
    }
    else
    {
        SourceEditorMasks->Load(EditorMasks);
    }
}

void CCfgPageEditors::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageEditors::Validate()");
    if (Dirty)
    {
        int i;
        for (i = 0; i < EditorMasks.Count; i++)
        {
            CMaskGroup masks(EditorMasks[i]->Masks->GetMasksString());
            int errorPos1, errorPos2;
            if (!masks.PrepareMasks(errorPos1))
            {
                EditLB->SetCurSel(i);
                SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                ti.ErrorOn(IDL_FILEMASKS);
                PostMessage(HWindow, WM_USER_EDIT, errorPos1, errorPos1 + 1);
                return;
            }

            CEditorMasksItem* item = EditorMasks[i];
            if (!ValidateCommandFile(HWindow, item->Command, errorPos1, errorPos2))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_COMMAND);
                PostMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
            if (!ValidateArguments(HWindow, item->Arguments, errorPos1, errorPos2))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_ARGUMENTS);
                PostMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
            if (!ValidateInitDir(HWindow, item->InitDir, errorPos1, errorPos2))
            {
                EditLB->SetCurSel(i);
                ti.ErrorOn(IDE_INITDIR);
                PostMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL,
                            errorPos1, errorPos2);
                return;
            }
        }
    }
}

void CCfgPageEditors::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageEditors::LoadControls()");
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    BOOL empty = FALSE;
    if (itemID == -1)
        empty = TRUE;

    CEditorMasksItem* item = NULL;
    if (!empty)
        item = (CEditorMasksItem*)itemID;
    DisableNotification = TRUE;
    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_LIMITTEXT, MAX_PATH - 1, 0);
    SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->Command));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->Arguments));
    SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), EM_SETSEL, 0, -1); // aby browse prepsal obsah
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_SETTEXT, 0,
                (LPARAM)(empty ? "" : item->InitDir));
    SendMessage(GetDlgItem(HWindow, IDE_INITDIR), EM_SETSEL, 0, -1); // aby browse prepsal obsah
    DisableNotification = FALSE;
}

void CCfgPageEditors::StoreControls()
{
    int index;
    EditLB->GetCurSel(index);
    if (!DisableNotification && index >= 0 && index < EditLB->GetCount())
    {
        Dirty = TRUE;
        CEditorMasksItem* item = EditorMasks[index];

        char command[MAX_PATH];
        char arguments[MAX_PATH];
        char initdir[MAX_PATH];
        SendMessage(GetDlgItem(HWindow, IDE_COMMAND), WM_GETTEXT,
                    MAX_PATH, (LPARAM)command);
        SendMessage(GetDlgItem(HWindow, IDE_ARGUMENTS), WM_GETTEXT,
                    MAX_PATH, (LPARAM)arguments);
        SendMessage(GetDlgItem(HWindow, IDE_INITDIR), WM_GETTEXT,
                    MAX_PATH, (LPARAM)initdir);
        item->Set(item->Masks->GetMasksString(), command, arguments, initdir);
    }
}

void CCfgPageEditors::EnableControls()
{
    CALL_STACK_MESSAGE1("CCfgPageEditors::EnableControls()");
    BOOL validItem = TRUE;
    INT_PTR itemID;
    EditLB->GetCurSelItemID(itemID);
    if (itemID == -1)
        validItem = FALSE;

    EnableWindow(GetDlgItem(HWindow, IDE_COMMAND), validItem);
    EnableWindow(GetDlgItem(HWindow, IDE_ARGUMENTS), validItem);
    EnableWindow(GetDlgItem(HWindow, IDE_INITDIR), validItem);

    EnableWindow(GetDlgItem(HWindow, IDB_BROWSECOMMAND), validItem);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEARGUMENTS), validItem);
    EnableWindow(GetDlgItem(HWindow, IDB_BROWSEINITDIR), validItem);
}

INT_PTR
CCfgPageEditors::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageEditors::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EditLB = new CEditListBox(HWindow, IDL_FILEMASKS);
        if (EditLB == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            EditLB->MakeHeader(IDS_MASKSHDR);
            EditLB->EnableDrag(::GetParent(HWindow));
        }
        ChangeToArrowButton(HWindow, IDB_BROWSECOMMAND);
        ChangeToArrowButton(HWindow, IDB_BROWSEARGUMENTS);
        ChangeToArrowButton(HWindow, IDB_BROWSEINITDIR);

        // prvky dialogu se maji natahovat podle jeho velikosti, nastavime delici controly
        ElasticVerticalLayout(1, IDL_FILEMASKS);

        break;
    }

    case WM_USER_EDIT:
    {
        SetFocus(GetDlgItem(HWindow, IDL_FILEMASKS));
        EditLB->OnBeginEdit((int)wParam, (int)lParam);
        return 0;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE || HIWORD(wParam) == BN_CLICKED)
        {
            EnableControls();
            StoreControls();
        }

        if (LOWORD(wParam) == IDL_FILEMASKS && HIWORD(wParam) == LBN_SELCHANGE)
        {
            EditLB->OnSelChanged();
            EnableControls();
            LoadControls();
        }

        switch (LOWORD(wParam))
        {
        case IDB_BROWSECOMMAND:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSECOMMAND, IDE_COMMAND, FALSE,
                             CommandExecutes, IDS_EXEFILTER);
            return 0;
        }

        case IDB_BROWSEARGUMENTS:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEARGUMENTS, IDE_ARGUMENTS, FALSE,
                             ArgumentsExecutes);
            return 0;
        }

        case IDB_BROWSEINITDIR:
        {
            TrackExecuteMenu(HWindow, IDB_BROWSEINITDIR, IDE_INITDIR, FALSE,
                             InitDirExecutes);
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        /*
      LRESULT result;
      if (EditLB->OnWMNotify(lParam, result)) 
      {
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, result);
        return 0;
      }
*/
        NMHDR* nmhdr = (NMHDR*)lParam;
        switch (nmhdr->idFrom)
        {
        case IDL_FILEMASKS:
        {
            switch (nmhdr->code)
            {
            case EDTLBN_GETDISPINFO:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                if (dispInfo->ToDo == edtlbGetData)
                {
                    lstrcpyn(dispInfo->Buffer, ((CEditorMasksItem*)dispInfo->ItemID)->Masks->GetMasksString(), MAX_PATH);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
                    return TRUE;
                }
                else
                {
                    Dirty = TRUE;
                    CEditorMasksItem* item;
                    if (dispInfo->ItemID == -1)
                    {
                        item = new CEditorMasksItem();
                        if (item == NULL)
                        {
                            TRACE_E(LOW_MEMORY);
                            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                            return TRUE;
                        }
                        EditorMasks.Add(item);
                        item->Set(dispInfo->Buffer, item->Command, item->Arguments, item->InitDir);
                        EditLB->SetItemData((INT_PTR)item);
                    }
                    else
                    {
                        item = (CEditorMasksItem*)dispInfo->ItemID;
                        item->Set(dispInfo->Buffer, item->Command, item->Arguments, item->InitDir);
                    }

                    EnableControls();
                    LoadControls();
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                    return TRUE;
                }
                break;
            }
                /*
            case EDTLBN_MOVEITEM:
            {
              EDTLB_DISPINFO *dispInfo = (EDTLB_DISPINFO *)lParam;
              int index;
              EditLB->GetCurSel(index);
              int srcIndex = index;
              int dstIndex = index + (dispInfo->Up ? -1 : 1);

              char buf[sizeof(CEditorMasksItem)];
              memcpy(buf, EditorMasks[srcIndex], sizeof(CEditorMasksItem));
              memcpy(EditorMasks[srcIndex], EditorMasks[dstIndex], sizeof(CEditorMasksItem));
              memcpy(EditorMasks[dstIndex], buf, sizeof(CEditorMasksItem));

              SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);  // povolim prohozeni
              return TRUE;
            }
*/
            case EDTLBN_MOVEITEM2:
            {
                EDTLB_DISPINFO* dispInfo = (EDTLB_DISPINFO*)lParam;
                int index;
                EditLB->GetCurSel(index);
                int srcIndex = index;
                int dstIndex = dispInfo->NewIndex;

                char buf[sizeof(CEditorMasksItem)];
                memcpy(buf, EditorMasks[srcIndex], sizeof(CEditorMasksItem));
                if (srcIndex < dstIndex)
                {
                    int i;
                    for (i = srcIndex; i < dstIndex; i++)
                        memcpy(EditorMasks[i], EditorMasks[i + 1], sizeof(CEditorMasksItem));
                }
                else
                {
                    int i;
                    for (i = srcIndex; i > dstIndex; i--)
                        memcpy(EditorMasks[i], EditorMasks[i - 1], sizeof(CEditorMasksItem));
                }
                memcpy(EditorMasks[dstIndex], buf, sizeof(CEditorMasksItem));

                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolime zmenu
                return TRUE;
            }

            case EDTLBN_DELETEITEM:
            {
                int index;
                EditLB->GetCurSel(index);
                EditorMasks.Delete(index);
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE); // povolim smazani
                return TRUE;
            }
            }
            break;
        }
        }
        break;
    }

    case WM_DRAWITEM:
    {
        int idCtrl = (int)wParam;
        if (idCtrl == IDL_FILEMASKS)
        {
            EditLB->OnDrawItem(lParam);
            return TRUE;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageMainWindow
//

CMainWindowIconItem MainWindowIcons[MAINWINDOWICONS_COUNT] =
    {
        {IDI_SALAMANDER, IDS_SALAMANDERICON_DEFAULT}, // default icon
        {IDI_SALAMANDER_RED, IDS_SALAMANDERICON_RED},
        {IDI_SALAMANDER_GREEN, IDS_SALAMANDERICON_GREEN},
        {IDI_SALAMANDER_BLUE, IDS_SALAMANDERICON_BLUE},
};

CCfgPageMainWindow::CCfgPageMainWindow()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_MAINWINDOW, IDD_CFGPAGE_MAINWINDOW, PSP_USETITLE, NULL)
{
    HIconsList = NULL;
}

CCfgPageMainWindow::~CCfgPageMainWindow()
{
    if (HIconsList != NULL)
        ImageList_Destroy(HIconsList);
}

void CCfgPageMainWindow::LoadControls()
{
}

void CCfgPageMainWindow::EnableControls()
{
    BOOL usePrefix = IsDlgButtonChecked(HWindow, IDC_TITLEBAR_PREFIX);
    EnableWindow(GetDlgItem(HWindow, IDC_TITLEBAR_PREFIX_TEXT), usePrefix);
}

void CCfgPageMainWindow::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageMainWindow::Transfer()");

    if (ti.Type == ttDataToWindow)
    {
        int resIDs[3] = {IDS_TITLEBAR_DIRECTORY, IDS_TITLEBAR_COMPOSITE, IDS_TITLEBAR_FULLPATH}; // musi korespondovat s TITLE_BAR_MODE_xxx
        int i;
        for (i = 0; i < 3; i++)
            SendDlgItemMessage(HWindow, IDC_TITLEBAR_MODE, CB_ADDSTRING, 0, (LPARAM)LoadStr(resIDs[i]));
    }

    ti.CheckBox(IDC_STATUSAREA, Configuration.StatusArea);
    ti.CheckBox(IDC_SPLASHSCREEN, Configuration.ShowSplashScreen);
    ti.CheckBox(IDC_TITLEBAR_PATH, Configuration.TitleBarShowPath);
    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_TITLEBAR_MODE, CB_SETCURSEL, Configuration.TitleBarMode, 0);
    else
        Configuration.TitleBarMode = (int)SendDlgItemMessage(HWindow, IDC_TITLEBAR_MODE, CB_GETCURSEL, 0, 0);

    // zazalohujeme si data, abychom rozpoznali zmenu
    BOOL oldUseTitleBarPrefix = Configuration.UseTitleBarPrefix;
    char oldTitleBarPrefix[TITLE_PREFIX_MAX];
    lstrcpyn(oldTitleBarPrefix, Configuration.TitleBarPrefix, TITLE_PREFIX_MAX);

    ti.CheckBox(IDC_TITLEBAR_PREFIX, Configuration.UseTitleBarPrefix);
    ti.EditLine(IDC_TITLEBAR_PREFIX_TEXT, Configuration.TitleBarPrefix, TITLE_PREFIX_MAX);

    if (ti.Type == ttDataFromWindow)
    {
        // pokud uzivatel zmenil veci kolem prefixu, sestrelime pripadnej command line option
        if (Configuration.UseTitleBarPrefix != oldUseTitleBarPrefix ||
            Configuration.UseTitleBarPrefix && strcmp(Configuration.TitleBarPrefix, oldTitleBarPrefix) != 0)
        {
            Configuration.UseTitleBarPrefixForced = FALSE;
            Configuration.TitleBarPrefixForced[0] = 0;
        }
    }

    int oldMainWindowIconIndex = Configuration.MainWindowIconIndex;
    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_TITLEBAR_ICON_INDEX, CB_SETCURSEL, Configuration.MainWindowIconIndex, 0);
    else
    {
        Configuration.MainWindowIconIndex = (int)SendDlgItemMessage(HWindow, IDC_TITLEBAR_ICON_INDEX, CB_GETCURSEL, 0, 0);
        if (Configuration.MainWindowIconIndex != oldMainWindowIconIndex)
            Configuration.MainWindowIconIndexForced = -1; // doslo ke zmene, sestrelime pripadny command line option
    }

    if (ti.Type == ttDataToWindow)
    {
        EnableControls();
    }
}

void CCfgPageMainWindow::Validate(CTransferInfo& ti)
{
}

BOOL CCfgPageMainWindow::InitIconCombobox()
{
    HWND hCombo = GetDlgItem(HWindow, IDC_TITLEBAR_ICON_INDEX);

    // vytahneme pozici puvodniho comboboxu
    RECT r;
    GetWindowRect(hCombo, &r);
    POINT p;
    p.x = r.left;
    p.y = r.top;
    ScreenToClient(HWindow, &p);

    // vytvorime EX verzi, ktera dokaze zobrazovat image list
    HWND hNewCombo = CreateWindowEx(0, WC_COMBOBOXEX, NULL,
                                    WS_BORDER | WS_CHILD | CBS_DROPDOWNLIST | WS_TABSTOP,
                                    0, 0, 0, (MAINWINDOWICONS_COUNT + 1) * (r.bottom - r.top), // radeji s rezervou, at neni seznam orezany pri hdpi
                                    HWindow,
                                    NULL,
                                    HInstance,
                                    NULL);
    SetWindowLongPtr(hNewCombo, GWLP_ID, IDC_TITLEBAR_ICON_INDEX);

    // od Vistou pokud je zapnuty aliasing fontu na Standard, mel combac aliasovanej font, zatimco zbytek dialogu
    // klasickej nealiasovanej; nastavime spravnej font
    HFONT hFont = (HFONT)SendMessage(hCombo, WM_GETFONT, 0, 0);
    SendMessage(hNewCombo, WM_SETFONT, (WPARAM)hFont, MAKELPARAM(TRUE, 0));

    HIconsList = ImageList_Create(16, 16, GetImageListColorFlags() | ILC_MASK, 0, 1);

    COMBOBOXEXITEM cbei;
    cbei.mask = CBEIF_TEXT | CBEIF_IMAGE | CBEIF_SELECTEDIMAGE;
    int i;
    for (i = 0; i < MAINWINDOWICONS_COUNT; i++)
    {
        HICON hIcon = LoadIcon(HInstance, MAKEINTRESOURCE(MainWindowIcons[i].IconResID));
        ImageList_AddIcon(HIconsList, hIcon);
        DestroyIcon(hIcon);

        cbei.iItem = i;
        cbei.pszText = LoadStr(MainWindowIcons[i].TextResID);
        cbei.iImage = i;
        cbei.iSelectedImage = i;
        SendMessage(hNewCombo, CBEM_INSERTITEM, 0, (LPARAM)&cbei);
    }

    SendMessage(hNewCombo, CBEM_SETIMAGELIST, 0, (LPARAM)HIconsList);

    SetWindowPos(hNewCombo, hCombo, p.x, p.y, r.right - r.left, r.bottom - r.top, SWP_NOACTIVATE | SWP_SHOWWINDOW);
    DestroyWindow(hCombo);

    return TRUE;
}

INT_PTR
CCfgPageMainWindow::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // existujici combobox pro volbu barvy ikonky nahradime jeho EX variantou
        InitIconCombobox();

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            EnableControls();
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageAppearance
//

CCfgPageAppearance::CCfgPageAppearance()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_APPEARANCE, IDD_CFGPAGE_APPEARANCE, PSP_USETITLE, NULL)
{
    HPanelFont = NULL;
    LocalUseCustomPanelFont = UseCustomPanelFont;
    memcpy(&LocalPanelLogFont, &LogFont, sizeof(LocalPanelLogFont));
    NotificationEnabled = TRUE;
}

CCfgPageAppearance::~CCfgPageAppearance()
{
    if (HPanelFont != NULL)
        HANDLES(DeleteObject(HPanelFont));
}

void CCfgPageAppearance::LoadControls()
{
    CALL_STACK_MESSAGE1("CCfgPageAppearance::LoadControls()");

    LOGFONT logFont;

    if (LocalUseCustomPanelFont)
        logFont = LocalPanelLogFont;
    else
        GetSystemGUIFont(&logFont);

    HWND hEdit = GetDlgItem(HWindow, IDE_PANELFONT);
    int origHeight = logFont.lfHeight;
    logFont.lfHeight = GetWindowFontHeight(hEdit); // pro prezentaci fontu v edit line pouzijeme jeji velikost fontu
    if (HPanelFont != NULL)
        HANDLES(DeleteObject(HPanelFont));
    HPanelFont = HANDLES(CreateFontIndirect(&logFont));

    HDC hDC = HANDLES(GetDC(HWindow));
    SendMessage(hEdit, WM_SETFONT, (WPARAM)HPanelFont, MAKELPARAM(TRUE, 0));
    char buf[LF_FACESIZE + 200];
    _snprintf_s(buf, _TRUNCATE, LoadStr(IDS_FONTDESCRIPTION),
                MulDiv(-origHeight, 72, GetDeviceCaps(hDC, LOGPIXELSY)),
                logFont.lfFaceName,
                LoadStr(LocalUseCustomPanelFont ? IDS_FONTDESCRIPTION_CST : IDS_FONTDESCRIPTION_DEF));
    SetWindowText(hEdit, buf);

    HANDLES(ReleaseDC(HWindow, hDC));
}

void CCfgPageAppearance::EnableControls()
{
    BOOL pathInTitle = IsDlgButtonChecked(HWindow, IDC_TITLEBAR_PATH);
    EnableWindow(GetDlgItem(HWindow, IDC_TITLEBAR_MODE), pathInTitle);
}

void CCfgPageAppearance::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageAppearance::Transfer()");

    ti.CheckBox(IDC_FULLROWSELECT, Configuration.FullRowSelect);       // vylucuje se s FullRowHighlight
    ti.CheckBox(IDC_FULLROWHIGHLIGHT, Configuration.FullRowHighlight); // vylucuje se s FullRowSelect
    ti.CheckBox(IDC_ICONTINCTURE, Configuration.UseIconTincture);
    ti.CheckBox(IDC_PANELCAPTION, Configuration.ShowPanelCaption);
    ti.CheckBox(IDC_PANELZOOM, Configuration.ShowPanelZoom);
    ti.CheckBox(IDC_SINGLECLICK, Configuration.SingleClick);

    ti.EditLine(IDC_INFOLINECONTENT, Configuration.InfoLineContent, 200);
    ti.EditLine(IDC_THUMBNAILSIZE, Configuration.ThumbnailSize);
    if (ti.Type == ttDataFromWindow)
        Configuration.ThumbnailSize = min(THUMBNAIL_SIZE_MAX, max(THUMBNAIL_SIZE_MIN, Configuration.ThumbnailSize));
    else
        SendDlgItemMessage(HWindow, IDC_THUMBNAILSIZE, EM_LIMITTEXT, 4, 0);

    if (ti.Type == ttDataToWindow)
    {
        EnableControls();
        LoadControls();
    }

    if (ti.Type == ttDataFromWindow)
    {
        UseCustomPanelFont = LocalUseCustomPanelFont;
        memcpy(&LogFont, &LocalPanelLogFont, sizeof(LocalPanelLogFont));
    }
}

void CCfgPageAppearance::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageAppearance::Validate()");
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_INFOLINECONTENT))
    {
        char buff[MAX_PATH];
        SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)buff);
        int errorPos1, errorPos2;
        if (!ValidateInfoLineItems(HWindow, buff, errorPos1, errorPos2))
        {
            ti.ErrorOn(IDC_INFOLINECONTENT);
            PostMessage(hWnd, EM_SETSEL, errorPos1, errorPos2);
            return;
        }
    }
}

INT_PTR
CCfgPageAppearance::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ChangeToArrowButton(HWindow, IDC_INFOLINEBROWSE);
        new CButton(HWindow, IDB_PANELFONT, BTF_RIGHTARROW);

        // pripojime UpDown control do editlajn
        int resID[] = {IDC_THUMBNAILSIZE, -1};
        int upDownID[] = {IDC_THUMBNAILSIZE_UPDOWN};
        int i;
        for (i = 0; resID[i] != -1; i++)
        {
            HWND hEdit = GetDlgItem(HWindow, resID[i]);
            HWND hWnd = CreateUpDownControl(WS_VISIBLE | WS_CHILD | WS_BORDER | UDS_SETBUDDYINT |
                                                UDS_ALIGNRIGHT | UDS_ARROWKEYS | UDS_NOTHOUSANDS,
                                            0, 0, 0, 0, HWindow, upDownID[i], HInstance,
                                            hEdit, THUMBNAIL_SIZE_MAX, THUMBNAIL_SIZE_MIN, 0);
            // posuneme UpDown control v z-orderu hned za editline, jinak
            // zobrazovani dialogu na pomalem stroji vypadalo divne
            // (UpDown se dokreslil az po vsech ostatnich controlech)
            SetWindowPos(hWnd, hEdit, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
        }

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            EnableControls();
        }

        if (NotificationEnabled && HIWORD(wParam) == EN_CHANGE && LOWORD(wParam) == IDC_THUMBNAILSIZE)
        {
            // notifikace zmenach v editline
            CTransferInfo ti(HWindow, ttDataFromWindow);
            int value;
            ti.EditLine(IDC_THUMBNAILSIZE, value); // meze si osetri slider
        }

        switch (LOWORD(wParam))
        {
        case IDC_FULLROWSELECT:
        {
            if (IsDlgButtonChecked(HWindow, IDC_FULLROWSELECT) == BST_CHECKED)
                CheckDlgButton(HWindow, IDC_FULLROWHIGHLIGHT, BST_UNCHECKED);
            return 0;
        }

        case IDC_FULLROWHIGHLIGHT:
        {
            if (IsDlgButtonChecked(HWindow, IDC_FULLROWHIGHLIGHT) == BST_CHECKED)
                CheckDlgButton(HWindow, IDC_FULLROWSELECT, BST_UNCHECKED);
            return 0;
        }

        case IDB_PANELFONT:
        {
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenu() dole...
MENU_TEMPLATE_ITEM CfgPageAppearanceMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_USEDEFAULTFONT
  {MNTT_IT, IDS_USECUSTOMFONT
  {MNTT_PE, 0
};
*/
            HMENU hMenu = CreatePopupMenu();
            BOOL cstFont = LocalUseCustomPanelFont;
            InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? 0 : MF_CHECKED | MF_BYCOMMAND | MF_STRING, 1, LoadStr(IDS_USEDEFAULTFONT));
            InsertMenu(hMenu, 0xFFFFFFFF, cstFont ? MF_CHECKED : 0 | MF_BYCOMMAND | MF_STRING, 2, LoadStr(IDS_USECUSTOMFONT));

            TPMPARAMS tpmPar;
            tpmPar.cbSize = sizeof(tpmPar);
            GetWindowRect((HWND)lParam, &tpmPar.rcExclude);
            DWORD cmd = TrackPopupMenuEx(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON, tpmPar.rcExclude.right, tpmPar.rcExclude.top,
                                         HWindow, &tpmPar);
            if (cmd != 0)
            {
                if (cmd == 1) // standard font
                {
                    LocalUseCustomPanelFont = FALSE;
                    LoadControls();
                }
                if (cmd == 2) // custom font
                {
                    CHOOSEFONT cf;
                    memset(&cf, 0, sizeof(CHOOSEFONT));
                    cf.lStructSize = sizeof(CHOOSEFONT);
                    cf.hwndOwner = HWindow;
                    cf.lpLogFont = &LocalPanelLogFont;
                    cf.iPointSize = 10;
                    cf.Flags = CF_NOVERTFONTS | CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT;
                    if (ChooseFont(&cf) != 0)
                    {
                        LocalUseCustomPanelFont = TRUE;
                        LoadControls();
                    }
                    return 0;
                }
            }
            return 0;
        }

        case IDC_INFOLINEBROWSE:
        {
            TrackExecuteMenu(HWindow, IDC_INFOLINEBROWSE, IDC_INFOLINECONTENT, FALSE,
                             InfoLineContentItems);
            return 0;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageChangeDrive
//

const int DRIVES_COUNT = 'z' - 'a' + 1;
const char FIRST_DRIVE = 'a'; // pokud chceme velka pismena, sem prijde 'A'

// umirnuje listbox, aby neslo klikat mimo existujici polozky
class CDriveListBox : public CWindow
{
public:
    CDriveListBox(HWND hDlg, int ctrlID) : CWindow(hDlg, ctrlID) {}

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_LBUTTONDOWN:
        {
            if (GetFocus() != HWindow)
                SendMessage(GetParent(HWindow), WM_NEXTDLGCTL, (WPARAM)HWindow, TRUE);
            int index = LOWORD(SendMessage(HWindow, LB_ITEMFROMPOINT, 0,
                                           MAKELPARAM(LOWORD(lParam), HIWORD(lParam))));
            if (index < 0 || index >= DRIVES_COUNT)
                return 0; // blbost, zahodime

            // zahodime kliknuti mimo polozku
            RECT r;
            SendMessage(HWindow, LB_GETITEMRECT, index, (LPARAM)&r);
            POINT pt;
            pt.x = LOWORD(lParam);
            pt.y = HIWORD(lParam);
            if (!PtInRect(&r, pt))
                return 0;

            break;
        }

        case WM_LBUTTONDBLCLK:
        {
            SendMessage(HWindow, WM_LBUTTONDOWN, wParam, lParam);
            return 0;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

CCfgPageChangeDrive::CCfgPageChangeDrive()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_CHANGEDRIVE, IDD_CFGPAGE_CHANGEDRIVE, PSP_USETITLE, NULL)
{
    CharSize.cx = 0;
    CharSize.cy = 0;
}

void CCfgPageChangeDrive::SetDrivesToListbox(int resID, DWORD drives)
{
    HWND hList = GetDlgItem(HWindow, resID);
    int i;
    for (i = 0; i < DRIVES_COUNT; i++)
    {
        BOOL select = (drives & (1 << i)) != 0;
        SendMessage(hList, LB_SETSEL, select, i);
    }
    // focus je na konci, vratime ho na zacatek
    SendMessage(hList, LB_SETCARETINDEX, 0, FALSE);
}

DWORD
CCfgPageChangeDrive::GetDrivesFromListbox(int resID)
{
    HWND hList = GetDlgItem(HWindow, resID);
    DWORD drives = 0;
    int i;
    for (i = 0; i < DRIVES_COUNT; i++)
    {
        BOOL selected = (BOOL)SendMessage(hList, LB_GETSEL, i, 0);
        if (selected)
            drives |= (1 << i);
    }
    return drives;
}

void CCfgPageChangeDrive::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPageChangeDrive::Transfer()");

    ti.CheckBox(IDC_CHD_SHOWMYDOC, Configuration.ChangeDriveShowMyDoc);
    ti.CheckBox(IDC_CHD_SHOWANOTHER, Configuration.ChangeDriveShowAnother);
    ti.CheckBox(IDC_CHD_SHOWNET, Configuration.ChangeDriveShowNet);
    ti.CheckBox(IDC_CHD_SHOWCLOUDSTORAGE, Configuration.ChangeDriveCloudStorage);

    if (ti.Type == ttDataToWindow)
    {
        SetDrivesToListbox(IDL_CHD_DRIVES, Configuration.VisibleDrives);
        SetDrivesToListbox(IDL_CHD_SEPARATORS, Configuration.SeparatedDrives);
        if (Plugins.GetFirstNethoodPluginFSName())
            EnableWindow(GetDlgItem(HWindow, IDC_CHD_SHOWNET), FALSE);
    }
    else
    {
        Configuration.VisibleDrives = GetDrivesFromListbox(IDL_CHD_DRIVES);
        Configuration.SeparatedDrives = GetDrivesFromListbox(IDL_CHD_SEPARATORS);
    }
}

void CCfgPageChangeDrive::InitList(int resID)
{
    HWND hList = GetDlgItem(HWindow, resID);

    RECT r;
    GetWindowRect(hList, &r);

    SendMessage(hList, LB_SETCOLUMNWIDTH, CharSize.cx + 2, 0);
    // vysku nastavime dle controlu, nastavil jsem mu LBS_NOINTEGRALHEIGHT, protoze u nekterych lidi mel vejsku nula asi diky jinym fontum
    SendMessage(hList, LB_SETITEMHEIGHT, 0, MAKELPARAM(/*CharSize.cy + 3*/ r.bottom - r.top - 4, 0));
    SendMessage(hList, LB_SETCOUNT, DRIVES_COUNT, 0);
}

INT_PTR
CCfgPageChangeDrive::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int staticsArr[] = {IDC_STATIC_6, IDS_CHD_HOTPATHS, IDC_STATIC_7, IDS_CHD_PLUGINS, IDC_STATIC_8, 0};
        CondenseStaticTexts(HWindow, staticsArr);

        // rozmery nejvetsiho pismenka: ulozime do CharSize
        HFONT hFont = (HFONT)SendDlgItemMessage(HWindow, IDL_CHD_DRIVES, WM_GETFONT, 0, 0);
        HDC hDC = HANDLES(GetDC(HWindow));
        HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
        char buff[] = " :";
        int i;
        for (i = 0; i < DRIVES_COUNT; i++)
        {
            buff[0] = FIRST_DRIVE + i;
            SIZE sz;
            GetTextExtentPoint32(hDC, buff, 2, &sz);
            if (sz.cx > CharSize.cx || sz.cy > CharSize.cy)
                CharSize = sz;
        }
        SelectObject(hDC, hOldFont);
        HANDLES(ReleaseDC(HWindow, hDC));

        // lepsi listboxy -- funguje doubleclick, ignoruje se klik mimo polozku
        new CDriveListBox(HWindow, IDL_CHD_DRIVES);
        new CDriveListBox(HWindow, IDL_CHD_SEPARATORS);

        // setup listboxes
        InitList(IDL_CHD_DRIVES);
        InitList(IDL_CHD_SEPARATORS);

        // HINTS for Drives, Hot Paths, Plugins
        CHyperLink* hl;
        hl = new CHyperLink(HWindow, IDS_CHD_HOTPATHS, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_CHDHOTPATHS_HINT));
        hl = new CHyperLink(HWindow, IDS_CHD_PLUGINS, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_CHDPLUGINS_HINT));

        break;
    }

    case WM_DRAWITEM:
    {
        LPDRAWITEMSTRUCT lpdis = (LPDRAWITEMSTRUCT)lParam;
        if ((int)lpdis->itemID >= 0 && (int)lpdis->itemID < DRIVES_COUNT)
        {
            RECT r = lpdis->rcItem;
            HDC hDC = lpdis->hDC;

            BOOL selected = (lpdis->itemState & ODS_SELECTED) != 0;
            BOOL focused = (GetFocus() == lpdis->hwndItem);

            FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
            if (selected)
            {
                RECT rr = r;
                InflateRect(&rr, -1, -1);
                FillRect(hDC, &rr, (HBRUSH)(UINT_PTR)((focused ? COLOR_HIGHLIGHT : COLOR_3DFACE) + 1));
            }

            int textColor;
            if (selected)
                textColor = focused ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT;
            else
                textColor = focused ? COLOR_GRAYTEXT : COLOR_GRAYTEXT;

            SetTextColor(hDC, GetSysColor(textColor));
            SetBkMode(hDC, TRANSPARENT);
            RECT dr = r;
            char text[] = " :";
            text[0] = FIRST_DRIVE + lpdis->itemID;
            DrawText(hDC, text, 2, &dr, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

            if (lpdis->itemState & ODS_FOCUS)
            {
                SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
                DrawFocusRect(hDC, &r);
            }
        }
        return TRUE;
    }

    case WM_CHARTOITEM:
    {
        int index = LowerCase[LOBYTE(LOWORD(wParam))] - 'a';
        HWND hList = (HWND)lParam;
        SendMessage(hList, LB_SETCARETINDEX, index, FALSE);
        BOOL selected = (BOOL)SendMessage(hList, LB_GETSEL, index, 0);
        SendMessage(hList, LB_SETSEL, !selected, index);
        return -1;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDL_CHD_DRIVES || LOWORD(wParam) == IDL_CHD_SEPARATORS)
        {
            if (HIWORD(wParam) == LBN_SETFOCUS || HIWORD(wParam) == LBN_KILLFOCUS)
            {
                InvalidateRect((HWND)lParam, NULL, TRUE);
                return 0;
            }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPagePanels
//

CCfgPagePanels::CCfgPagePanels()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_PANELS, IDD_CFGPAGE_PANELS, PSP_USETITLE, NULL)
{
}

void CCfgPagePanels::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCfgPagePanels::Transfer()");

    // hodnoty v konfiguraci (Configuration.FileNameFormat) drzime pro zpetnou kompatibilitu
    const int MANGLE_ITEMS = 6;
    int mangles[MANGLE_ITEMS] = {4 /*ONTHEDISK*/, 5 /*EXPLORER*/, 6 /*VC*/, 7 /*PARTMIXEDCASE*/, 2 /*LOWERCASE*/, 3 /*UPPERCASE*/};

    const int SIZE_ITEMS = 3;
    int sizes[SIZE_ITEMS] = {SIZE_FORMAT_BYTES, SIZE_FORMAT_KB, SIZE_FORMAT_MIXED};

    if (ti.Type == ttDataToWindow)
    {
        int resIDs[MANGLE_ITEMS] = {IDS_NAMEMANGLE_ONTHEDISK, IDS_NAMEMANGLE_EXPLORER, IDS_NAMEMANGLE_VC, IDS_NAMEMANGLE_PARTMIXEDCASE, IDS_NAMEMANGLE_LOWERCASE, IDS_NAMEMANGLE_UPPERCASE}; // musi korespondovat s TITLE_BAR_MODE_xxx
        BOOL selected = FALSE;
        int i;
        for (i = 0; i < MANGLE_ITEMS; i++)
        {
            SendDlgItemMessage(HWindow, IDC_NAMEMANGLE, CB_ADDSTRING, 0, (LPARAM)LoadStr(resIDs[i]));
            if (!selected && Configuration.FileNameFormat == mangles[i])
            {
                SendDlgItemMessage(HWindow, IDC_NAMEMANGLE, CB_SETCURSEL, i, 0);
                selected = TRUE;
            }
        }
        if (!selected)
            SendDlgItemMessage(HWindow, IDC_NAMEMANGLE, CB_SETCURSEL, 0, 0); // ONTHEDISK

        int resID2s[SIZE_ITEMS] = {IDS_SIZEMODE_BYTES, IDS_SIZEMODE_KB, IDS_SIZEMODE_MIXED}; // musi korespondovat s TITLE_BAR_MODE_xxx
        selected = FALSE;
        for (i = 0; i < SIZE_ITEMS; i++)
        {
            SendDlgItemMessage(HWindow, IDC_SIZEFORMAT, CB_ADDSTRING, 0, (LPARAM)LoadStr(resID2s[i]));
            if (!selected && Configuration.SizeFormat == sizes[i])
            {
                SendDlgItemMessage(HWindow, IDC_SIZEFORMAT, CB_SETCURSEL, i, 0);
                selected = TRUE;
            }
        }
        if (!selected)
            SendDlgItemMessage(HWindow, IDC_SIZEFORMAT, CB_SETCURSEL, 0, 0); // SIZE_FORMAT_BYTES
    }
    else
    {
        int index = (int)SendDlgItemMessage(HWindow, IDC_NAMEMANGLE, CB_GETCURSEL, 0, 0);
        if (index < 0 || index >= MANGLE_ITEMS)
            index = 0; // ONTHEDISK
        Configuration.FileNameFormat = mangles[index];

        index = (int)SendDlgItemMessage(HWindow, IDC_SIZEFORMAT, CB_GETCURSEL, 0, 0);
        if (index < 0 || index >= SIZE_ITEMS)
            index = 0; // SIZE_FORMAT_BYTES
        Configuration.SizeFormat = sizes[index];
    }

    ti.CheckBox(IDC_NOTHIDDENSYSTEM, Configuration.NotHiddenSystemFiles);
    ti.CheckBox(IDC_INCLUDEDIRS, Configuration.IncludeDirs);
    ti.CheckBox(IDC_DISABLEDANDD, Configuration.UseDragDropMinTime);
    ti.EditLine(IDE_DRAGDROPDELAY, Configuration.DragDropMinTime);
    ti.CheckBox(IDC_QUICKSEARCH_ALT, Configuration.QuickSearchEnterAlt);
    ti.CheckBox(IDE_PRIMARYCTXMENU, Configuration.PrimaryContextMenu);
    ti.CheckBox(IDC_SHIFTFORHOTPATHS, Configuration.ShiftForHotPaths);
    ti.CheckBox(IDC_CLICKTORENAME, Configuration.ClickQuickRename);
    ti.CheckBox(IDC_SORTUSESLOCALE, Configuration.SortUsesLocale);
    ti.CheckBox(IDC_SORTDETECTNUMBERS, Configuration.SortDetectNumbers);
    ti.CheckBox(IDC_SORTNEWERONTOP, Configuration.SortNewerOnTop);
    ti.CheckBox(IDC_SORTDIRSBYEXT, Configuration.SortDirsByExt);
    ti.CheckBox(IDC_SORTDIRSBYNAME, Configuration.SortDirsByName);

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CCfgPagePanels::EnableControls()
{
    BOOL useDDMin = IsDlgButtonChecked(HWindow, IDC_DISABLEDANDD);
    EnableWindow(GetDlgItem(HWindow, IDE_DRAGDROPDELAY), useDDMin);
}

INT_PTR
CCfgPagePanels::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CTaskListDialog
//

CTaskListDialog::CTaskListDialog(HWND parent) : CCommonDialog(HLanguage, IDD_TASKLIST, IDD_TASKLIST, parent)
{
    DisplayedVersion = 0;
}

void CTaskListDialog::Refresh()
{
    CALL_STACK_MESSAGE1("CTaskListDialog::Refresh()");

    HWND list = GetDlgItem(HWindow, IDC_SALAMLIST);
    if (list == NULL)
    {
        TRACE_E("Unexpected situation in CTaskListDialog::Refresh()");
        return;
    }

    // ulozim text minule vybrane polozky
    char oldSelected[250];
    int oldIndex = (int)SendMessage(list, LB_GETCURSEL, 0, 0);
    if (oldIndex == LB_ERR || SendMessage(list, LB_GETTEXT, oldIndex, (LPARAM)oldSelected) == LB_ERR)
        oldSelected[0] = 0;

    SendMessage(list, LB_RESETCONTENT, 0, 0);

    CProcessListItem items[MAX_TL_ITEMS];
    int c = TaskList.GetItems(items, &DisplayedVersion);
    DWORD PID = GetCurrentProcessId();
    int i;
    for (i = 0; i < c; i++)
    {
        char date[50], time[50];
        if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &items[i].StartTime, NULL, time, 50) == 0)
        {
            sprintf(time, "%u:%02u:%02u", items[i].StartTime.wHour, items[i].StartTime.wMinute,
                    items[i].StartTime.wSecond);
        }
        if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &items[i].StartTime, NULL, date, 50) == 0)
        {
            sprintf(date, "%u.%u.%u", items[i].StartTime.wDay, items[i].StartTime.wMonth,
                    items[i].StartTime.wYear);
        }

        char buf[100];
        sprintf(buf, LoadStr(IDS_TASKLISTLINE), items[i].PID, date, time,
                (items[i].PID == PID ? LoadStr(IDS_TASKLISTCURLINE) : ""));
        SendMessage(list, LB_ADDSTRING, 0, (LPARAM)buf);

        if (strcmp(buf, oldSelected) == 0)
            SendMessage(list, LB_SETCURSEL, i, 0);
    }
    if (SendMessage(list, LB_GETCURSEL, 0, 0) == LB_ERR)
        SendMessage(list, LB_SETCURSEL, 0, 0); // fallback
}

DWORD
CTaskListDialog::GetCurPID()
{
    HWND list = GetDlgItem(HWindow, IDC_SALAMLIST);
    if (list != NULL)
    {
        int i = (int)SendMessage(list, LB_GETCARETINDEX, 0, 0);
        char buf[100];
        if (SendMessage(list, LB_GETTEXT, i, (LPARAM)buf) != LB_ERR)
        {
            char* s = buf + 4;
            char* end = s;
            while (*end != ' ' && *end != 0)
                end++;
            *end = 0;
            return atoi(s);
        }
    }
    return -1;
}

INT_PTR
CTaskListDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CTaskListDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        Refresh();
        SetTimer(HWindow, IDT_UPDATETASKLIST, 200, NULL);
        break;
    }

    case WM_DESTROY:
    {
        KillTimer(HWindow, IDT_UPDATETASKLIST);
        break;
    }

    case WM_TIMER:
    {
        if (wParam == IDT_UPDATETASKLIST)
        {
            DWORD version;
            TaskList.GetItems(NULL, &version);
            if (version > DisplayedVersion)
                Refresh();
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_BREAKTASK:
        {
            DWORD pid = GetCurPID();
            if (pid == GetCurrentProcessId())
            {
                SalMessageBox(HWindow, LoadStr(IDS_CURRENTPROCESSBREAK), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                return 0;
            }
            if (pid != -1)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_CONFIRM_BREAK), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES)
                {
                    if (!TaskList.FireEvent(TASKLIST_TODO_BREAK, pid))
                    {
                        TRACE_I("Unable to deliver break-message.");
                    }
                }
            }
            return 0;
        }

        case IDB_KILLTASK:
        {
            DWORD pid = GetCurPID();
            if (pid == GetCurrentProcessId())
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_CURRENTPROCESSTERMINATE), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_DEFBUTTON2 | MB_ICONQUESTION) == IDYES)
                {
                    TRACE_I("CTaskListDialog::DialogProc(IDB_KILLTASK): calling ExitProcess(668).");
                    // ExitProcess(668);
                    TerminateProcess(GetCurrentProcess(), 668); // tvrdsi exit (tenhle jeste neco vola)
                }
                return 0;
            }
            if (pid != -1)
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_CONFIRM_TERMINATE), LoadStr(IDS_QUESTION),
                                  MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDYES)
                {
                    if (!TaskList.FireEvent(TASKLIST_TODO_TERMINATE, pid))
                    {
                        TRACE_I("Unable to deliver terminate-message.");
                    }
                }
            }
            return 0;
        }

        case IDB_SHOWTASK:
        {
            DWORD pid = GetCurPID();
            if (pid != -1)
            {
                TaskList.FireEvent(TASKLIST_TODO_HIGHLIGHT, pid);
            }
            return 0;
        }

        case IDB_REFRESHLIST:
        {
            Refresh();
            return 0;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageKeyboard
//

CCfgPageKeyboard::CCfgPageKeyboard()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_KEYBOARD, IDD_CFGPAGE_KEYBOARD, PSP_USETITLE, NULL)
{
}

INT_PTR
CCfgPageKeyboard::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CTaskListDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        int staticsArr[] = {IDC_STATIC_2, IDC_KEYBOARD_SHORTCUTS, 0};
        CondenseStaticTexts(HWindow, staticsArr);

        CHyperLink* hl = new CHyperLink(HWindow, IDC_KEYBOARD_SHORTCUTS);
        if (hl != NULL)
            hl->SetActionPostCommand(CM_HELP_KEYBOARD);
        //        hl->SetActionOpen("https://www.altap.cz/salam_en/features/keyboard.html"); // pozor, jeste na jednom miste
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == CM_HELP_KEYBOARD)
        {
            OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, CM_HELP_KEYBOARD, FALSE);
            return 0;
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
