// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <lm.h>

#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "menu.h"
#include "gui.h"
#include "drivelst.h"
#include "shiconov.h"

/*  //****************************************************************************
//
// CTipOfTheDayWindow a CTipOfTheDayDialog
//
//

CTipOfTheDayWindow::CTipOfTheDayWindow()
{
  CALL_STACK_MESSAGE1("CTipOfTheDayWindow::CTipOfTheDayWindow()");

  LOGFONT srcLF;
  HFONT hSystemFont = (HFONT)HANDLES(GetStockObject(DEFAULT_GUI_FONT));
  GetObject(hSystemFont, sizeof(srcLF), &srcLF);

  LOGFONT lf;

  lf.lfHeight = (int)(srcLF.lfHeight * 1.9);
  lf.lfWidth = 0;
  lf.lfEscapement = 0;
  lf.lfOrientation = 0;
  lf.lfWeight = FW_BOLD;
  lf.lfItalic = 0;
  lf.lfUnderline = 0;
  lf.lfStrikeOut = 0;
  lf.lfCharSet = UserCharset;
  lf.lfOutPrecision = OUT_DEFAULT_PRECIS;
  lf.lfClipPrecision = CLIP_DEFAULT_PRECIS;
  lf.lfQuality = DEFAULT_QUALITY;
  lf.lfPitchAndFamily = VARIABLE_PITCH | FF_SWISS;
  lstrcpy(lf.lfFaceName, "Times New Roman");
  HHeadingFont = HANDLES(CreateFontIndirect(&lf));

  lf.lfHeight = (int)(srcLF.lfHeight * 1.2);
  lf.lfWeight = FW_NORMAL;
  lstrcpy(lf.lfFaceName, "Arial");
  HBodyFont = HANDLES(CreateFontIndirect(&lf));

}

CTipOfTheDayWindow::~CTipOfTheDayWindow()
{
  CALL_STACK_MESSAGE1("CTipOfTheDayWindow::~CTipOfTheDayWindow()");
  HANDLES(DeleteObject(HHeadingFont));
  HANDLES(DeleteObject(HBodyFont));
}

void
CTipOfTheDayWindow::PaintBodyText(HDC hDC)
{
  CALL_STACK_MESSAGE1("CTipOfTheDayWindow::PaintBodyText()");
  BOOL releaseDC = FALSE;
  if (hDC == NULL)
  {
    hDC = HANDLES(GetDC(HWindow));
    releaseDC = TRUE;
  }
  RECT clientR;
  GetClientRect(HWindow, &clientR);
  int leftWidth = clientR.right / 7;
  int topWidth = clientR.right / 8;

  RECT r;
  r = clientR;
  r.top = topWidth + 1;
  r.left = leftWidth;
  r.right--;
  r.bottom--;

  InflateRect(&r, -leftWidth / 8, -leftWidth / 8);

  FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));

  int tipIndex = Configuration.LastTipOfTheDay;
  if (tipIndex >= 0 && tipIndex < Parent->Tips.Count)
  {
    HFONT hOldFont = (HFONT)SelectObject(hDC, HBodyFont);
    int oldBkMode = SetBkMode(hDC, TRANSPARENT);
    COLORREF oldTextColor = SetTextColor(hDC, GetSysColor(COLOR_WINDOWTEXT));
    char *line = (char *)Parent->Tips[tipIndex];
    DrawText(hDC, line, -1, &r, DT_LEFT | DT_NOPREFIX | DT_WORDBREAK);
    SetTextColor(hDC, oldTextColor);
    SetBkMode(hDC, oldBkMode);
    SelectObject(hDC, hOldFont);
  }
  if (releaseDC)
    HANDLES(ReleaseDC(HWindow, hDC));
}

LRESULT
CTipOfTheDayWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CTipOfTheDayWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_ERASEBKGND:
    {
      return TRUE;
    }

    case WM_PAINT:
    {
      PAINTSTRUCT ps;
      HDC hDC = HANDLES(BeginPaint(HWindow, &ps));
      RECT clientR;
      GetClientRect(HWindow, &clientR);
      RECT r;
      int leftWidth = clientR.right / 7;
      int topWidth = clientR.right / 8;
      // obvod
      HPEN hOldPen = (HPEN)SelectObject(hDC, BtnShadowPen);
      MoveToEx(hDC, 0, clientR.bottom - 1, NULL);
      LineTo(hDC, 0, 0);
      LineTo(hDC, clientR.right - 1, 0);
      SelectObject(hDC, BtnHilightPen);
      MoveToEx(hDC, clientR.right - 1, 0, NULL);
      LineTo(hDC, clientR.right - 1, clientR.bottom - 1);
      LineTo(hDC, -1, clientR.bottom - 1);

      // sedivy pruh vlevo
      r = clientR;
      r.left++;
      r.right = leftWidth;
      r.top++;
      r.bottom--;
      FillRect(hDC, &r, (HBRUSH)(COLOR_BTNSHADOW + 1));
      // na nej ikonku ukradenou od Microsoftu - proc si na ne
      // vsichni tak stezujou, kdyz maj tak pekne ikonky ? ;-)
      DrawIcon(hDC, r.left + (r.right - r.left - 32) / 2, 16,
               HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(IDI_TIPOFTHEDAY))));

      // bily pruh nahore
      r = clientR;
      r.top++;
      r.left = leftWidth;
      r.bottom = topWidth;
      r.right--;
      FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
      r.top += leftWidth / 5;
      r.left += leftWidth / 8;
      HFONT hOldFont = (HFONT)SelectObject(hDC, HHeadingFont);
      int oldBkMode = SetBkMode(hDC, TRANSPARENT);
      DrawText(hDC, LoadStr(IDS_TOD_DIDYOUKNOW), -1, &r, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);

      // oddelovaci sediva line
      SelectObject(hDC, BtnShadowPen);
      MoveToEx(hDC, leftWidth, topWidth, NULL);
      LineTo(hDC, clientR.right - 1, topWidth);
      // bily pruh dole
      r = clientR;
      r.top = topWidth + 1;
      r.left = leftWidth;
      r.right--;
      r.bottom--;
      FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));

      PaintBodyText(hDC);

      SetBkMode(hDC, oldBkMode);
      SelectObject(hDC, hOldFont);
      SelectObject(hDC, hOldPen);
      HANDLES(EndPaint(HWindow, &ps));
      return 0;
    }
  }
  return CWindow::WindowProc(uMsg, wParam, lParam);
}

CTipOfTheDayDialog::CTipOfTheDayDialog(BOOL quiet)
 : CCommonDialog(HLanguage, IDD_TIPOFTHEDAY, NULL),
   Tips(200, 100)
{
  CALL_STACK_MESSAGE2("CTipOfTheDayDialog::CTipOfTheDayDialog(%d)", quiet);
  TipWindow.Parent = this;
  LoadTips(quiet);
}

CTipOfTheDayDialog::~CTipOfTheDayDialog()
{
  CALL_STACK_MESSAGE1("CTipOfTheDayDialog::~CTipOfTheDayDialog()");
  FreeTips();
}

BOOL
CTipOfTheDayDialog::LoadTips(BOOL quiet)
{
  CALL_STACK_MESSAGE2("CTipOfTheDayDialog::LoadTips(%d)", quiet);
  char fileName[MAX_PATH];
  GetModuleFileName(HInstance, fileName, MAX_PATH);
  CutDirectory(fileName);
  SalPathAppend(fileName, "help\\tips.txt", MAX_PATH);
  HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ |
                                      FILE_SHARE_WRITE, NULL,
                                      OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
  if (hFile == INVALID_HANDLE_VALUE)
  {
    if (!quiet)
    {
      char buf[MAX_PATH + 100];
      sprintf(buf, LoadStr(IDS_FILEREADERROR), fileName);
      SalMessageBox(MainWindow->HWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    return FALSE;
  }

  DWORD size = GetFileSize(hFile, NULL);
  if (size == 0xFFFFFFFF || size == 0)
  {
    if (!quiet)
    {
      char buf[MAX_PATH + 100];
      sprintf(buf, LoadStr(IDS_FILEREADERROR), fileName);
      SalMessageBox(MainWindow->HWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    HANDLES(CloseHandle(hFile));
    return FALSE;
  }

  BYTE *data = (BYTE*)malloc(size);
  if (data == NULL)
  {
    TRACE_E(LOW_MEMORY);
    HANDLES(CloseHandle(hFile));
    return FALSE;
  }

  DWORD read;
  if (!ReadFile(hFile, data, size, &read, NULL) || read != size)
  {
    if (!quiet)
    {
      char buf[MAX_PATH + 100];
      sprintf(buf, LoadStr(IDS_FILEREADERROR), fileName);
      SalMessageBox(MainWindow->HWindow, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    free(data);
    HANDLES(CloseHandle(hFile));
    return FALSE;
  }


  BYTE *lineStart = data;
  DWORD count = 0;
  while (count < size)
  {
    BYTE *lineEnd = lineStart;
    while (*lineEnd != '\r' && *lineEnd != '\n' && count < size)
    {
      lineEnd++;
      count++;
    }
    int lineLen = lineEnd - lineStart;
    if (lineLen > 0)
    {
      BYTE *line = (BYTE*)malloc(lineLen + 1);
      if (line == NULL)
      {
        TRACE_E(LOW_MEMORY);
        free(data);
        HANDLES(CloseHandle(hFile));
        return FALSE;
      }
      memmove(line, lineStart, lineLen);
      *(line + lineLen) = 0;
      Tips.Add((DWORD)line);
      if (!Tips.IsGood())
      {
        Tips.ResetState();
        free(line);
        free(data);
        HANDLES(CloseHandle(hFile));
        return FALSE;
      }
    }
    while ((*lineEnd == '\r' || *lineEnd == '\n') && count < size)
    {
      lineEnd++;
      count++;
    }
    lineStart = lineEnd;
  }

  free(data);

  HANDLES(CloseHandle(hFile));

  return TRUE;
}

void
CTipOfTheDayDialog::FreeTips()
{
  CALL_STACK_MESSAGE1("CTipOfTheDayDialog::FreeTips()");
  int count = Tips.Count;
  int i;
  for (i = 0; i < count; i++)
    free((char*)Tips[i]);
}

void
CTipOfTheDayDialog::Transfer(CTransferInfo &ti)
{
  CALL_STACK_MESSAGE1("CTipOfTheDayDialog::Transfer()");
  ti.CheckBox(IDC_TOD_SHOW, Configuration.ShowTipOfTheDay);
}

void
CTipOfTheDayDialog::IncrementTipIndex()
{
  Configuration.LastTipOfTheDay++;
  if (Configuration.LastTipOfTheDay >= Tips.Count)
    Configuration.LastTipOfTheDay = 0;
}

BOOL
CTipOfTheDayDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("CTipOfTheDayWindow::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      TipWindow.AttachToWindow(GetDlgItem(HWindow, IDC_TOD_TIP));

      // priradim oknu ikonku
      SendMessage(HWindow, WM_SETICON, ICON_BIG,
                  (LPARAM)HANDLES(LoadIcon(HInstance, MAKEINTRESOURCE(IDI_TIPOFTHEDAY))));

      break;
    }

    case WM_DESTROY:
    {
      TipWindow.DetachWindow();
      if (MainWindow != NULL)
        MainWindow->TipOfTheDayDialog = NULL;
      break;
    }

    case WM_COMMAND:
    {
      if (LOWORD(wParam) == IDCANCEL)
        wParam = IDOK;
      if (LOWORD(wParam) == IDOK)
        IncrementTipIndex();
      if (LOWORD(wParam) == IDC_TOD_NEXT)
      {
        IncrementTipIndex();
        TipWindow.PaintBodyText();
        return 0;
      }
      if (LOWORD(wParam) == IDC_TOD_SHOW)
        TransferData(ttDataFromWindow);
      break;
    }
  }
  return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
*/

//****************************************************************************
//
// CSharesDialog
//

CSharesDialog::CSharesDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_SHARES, IDD_SHARES, hParent),
      SharedDirs(FALSE) // we want complete spheres
{
    HListView = NULL;
    SortBy = 0; // advising according to shares
    FocusedIndex = -1;
}

HIMAGELIST
CSharesDialog::CreateImageList()
{
    HIMAGELIST himl = ImageList_Create(16, 16, GetImageListColorFlags() | ILC_MASK, 2, 0);
    //  ImageList_SetBkColor(himl, GetSysColor(COLOR_WINDOW)); // aby pod XP chodily pruhledne ikonky

    HICON hIcon = SalLoadImage(4, 4, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags); // symbolsDirectory
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(himl, -1, hIcon);
        HANDLES(DestroyIcon(hIcon));
    }
    else
        ImageList_SetImageCount(himl, 1);

    ImageList_ReplaceIcon(himl, -1, HSharedOverlays[ICONSIZE_16]);
    ImageList_SetOverlayImage(himl, 1, 1);
    return himl;
}

void CSharesDialog::InitColumns()
{
    CALL_STACK_MESSAGE1("CSharesDialog::InitColumns()");
    LV_COLUMN lvc;
    int header[3] = {IDS_SHARES_NAME, IDS_SHARES_PATH, IDS_SHARES_COMMENT};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 3; i++) // create columns
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
    }
    ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
    int width = ListView_GetColumnWidth(HListView, 0);
    int scrollW = GetSystemMetrics(SM_CXHSCROLL);
    ListView_SetColumnWidth(HListView, 0, width + 15 + scrollW);
    ListView_SetColumnWidth(HListView, 1, width * 3);
    ListView_SetColumnWidth(HListView, 2, LVSCW_AUTOSIZE_USEHEADER); // remainder
    ListView_SetColumnWidth(HListView, 0, width + 15);               // space for scrollbar
}

int CALLBACK
CSharesDialog::SortFunc(LPARAM lParam1, LPARAM lParam2, LPARAM lParamSort)
{
    int nRetVal = 0;

    CSharesDialog* dlg = (CSharesDialog*)lParamSort;

    int index1 = (int)lParam1;
    const char* localPath1;
    const char* remoteName1;
    const char* comment1;
    dlg->SharedDirs.GetItem(index1, &localPath1, &remoteName1, &comment1);

    int index2 = (int)lParam2;
    const char* localPath2;
    const char* remoteName2;
    const char* comment2;
    dlg->SharedDirs.GetItem(index2, &localPath2, &remoteName2, &comment2);

    switch (dlg->SortBy)
    {
    case 1: // shared path
    {
        nRetVal = StrICmp(localPath1, localPath2);
        break;
    }

    case 2: // comment
    {
        nRetVal = StrICmp(comment1, comment2);
        break;
    }

    default: // share name
    {
        nRetVal = StrICmp(remoteName1, remoteName2);
        break;
    }
    }
    return nRetVal;
}

void CSharesDialog::EnableControls()
{
    BOOL focused = GetFocusedIndex() != -1;
    EnableWindow(GetDlgItem(HWindow, IDC_SHARES_STOP), focused);
    EnableWindow(GetDlgItem(HWindow, IDOK), focused);
}

void CSharesDialog::Refresh()
{
    SharedDirs.Refresh();

    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(HListView);
    const char* localPath;
    const char* remoteName;
    const char* comment;
    int i;
    for (i = 0; i < SharedDirs.GetCount(); i++)
    {
        if (SharedDirs.GetItem(i, &localPath, &remoteName, &comment))
        {
            LVITEM lvi;
            lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_STATE | LVIF_PARAM;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.iImage = 0;
            lvi.state = INDEXTOOVERLAYMASK(1);
            lvi.pszText = (char*)remoteName;
            lvi.lParam = i; // for later sorting
            int index = ListView_InsertItem(HListView, &lvi);
            ListView_SetItemText(HListView, index, 1, (char*)localPath);
            ListView_SetItemText(HListView, index, 2, (char*)comment);
        }
    }
    SortItems();
    DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_SetItemState(HListView, 0, state, state);
    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);
    EnableControls();
}

void CSharesDialog::SortItems()
{
    ListView_SortItems(HListView, SortFunc, (LPARAM)this);
}

const char*
CSharesDialog::GetFocusedPath()
{
    if (FocusedIndex == -1)
        return NULL;
    const char* localPath;
    SharedDirs.GetItem(FocusedIndex, &localPath, NULL, NULL);
    return localPath;
}

int CSharesDialog::GetFocusedIndex()
{
    int index = ListView_GetNextItem(HListView, -1, LVIS_SELECTED);
    if (index != -1)
    {
        LVITEM lvi;
        lvi.iItem = index;
        lvi.iSubItem = 0;
        lvi.mask = LVIF_PARAM;
        ListView_GetItem(HListView, &lvi);
        index = (int)lvi.lParam;
    }
    return index;
}

void CSharesDialog::DeleteShare(const char* shareName)
{
    OLECHAR oleShareName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, shareName, -1, oleShareName, MAX_PATH);
    oleShareName[MAX_PATH - 1] = 0;
    NetShareDel(NULL, oleShareName, 0);
}

void CSharesDialog::OnContextMenu(int x, int y)
{
    HWND hListView = HListView;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;

    // extracting texts from buttons and filling the context menu
    int ids[] = {-2, IDOK, // -2 -> next item will be default
                 IDC_SHARES_STOP,
                 -1, // -1 -> separator
                 IDC_SHARES_REFRESH,
                 0}; // 0 -> terminator
    CMenuPopup popup;
    FillContextMenuFromButtons(&popup, HWindow, ids);

    DWORD cmd = popup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                            x, y, HWindow, NULL);
    if (cmd != 0)
        PostMessage(HWindow, WM_COMMAND, cmd, 0);
}

INT_PTR
CSharesDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HListView = GetDlgItem(HWindow, IDC_SHARES_LIST);
        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        HIMAGELIST himl = CreateImageList();
        ListView_SetImageList(HListView, himl, LVSIL_SMALL);

        /*        // insert a resize window handle into the bottom right corner
      // do not insert, it would be too much unnecessary code
      RECT r;
      GetClientRect(HWindow, &r);
      CreateWindowEx(0,
                     "scrollbar",
                     "",
                     WS_CHILDWINDOW | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | WS_VISIBLE |
                     WS_GROUP | SBS_SIZEBOX | SBS_SIZEGRIP | SBS_SIZEBOXBOTTOMRIGHTALIGN,
                     0, 0, r.right, r.bottom,
                     HWindow,
                     (HMENU)IDC_SHARES_GRIP,
                     HInstance,
                     NULL);*/

        InitColumns();
        Refresh();
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            FocusedIndex = GetFocusedIndex();
            if (FocusedIndex == -1)
                return 0;
            break;
        }

        case IDC_SHARES_REFRESH:
        {
            Refresh();
            break;
        }

        case IDC_SHARES_STOP:
        {
            int index = GetFocusedIndex();
            if (index != -1)
            {
                const char* remoteName;
                if (SharedDirs.GetItem(index, NULL, &remoteName, NULL))
                {
                    char buff[2000];
                    wsprintf(buff, LoadStr(IDS_CONFIRM_STOPSHARE), remoteName);
                    if (SalMessageBox(HWindow, buff, LoadStr(IDS_QUESTION),
                                      MB_OKCANCEL | MB_ICONQUESTION) == IDOK)
                    {
                        DeleteShare(remoteName);
                        Refresh();
                    }
                }
            }
            break;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_SHARES_LIST)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDOK, BN_CLICKED), 0);
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
                NMLVKEYDOWN* kd = (NMLVKEYDOWN*)lParam;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed && kd->wVKey == VK_F10 || kd->wVKey == VK_APPS)
                {
                    POINT p;
                    GetListViewContextMenuPos(HListView, &p);
                    OnContextMenu(p.x, p.y);
                }
                return 0;
            }

            case LVN_ITEMCHANGED:
            {
                EnableControls();
                return 0;
            }

            case LVN_COLUMNCLICK:
            {
                int subItem = ((NM_LISTVIEW*)lParam)->iSubItem;
                if (subItem >= 0 && subItem < 3)
                {
                    if (subItem != SortBy)
                    {
                        SortBy = subItem;
                        SortItems();
                    }
                }
                break;
            }
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

//****************************************************************************
//
// CDisconnectDialog
//

CDisconnectDialog::CDisconnectDialog(CFilesWindow* panel)
    : CCommonDialog(HLanguage, IDD_DISCONNECT, IDD_DISCONNECT, panel->HWindow), Connections(10, 5)
{
    Panel = panel;
    HListView = NULL;
    HImageList = NULL;
    NoConncection = FALSE;
}

CDisconnectDialog::~CDisconnectDialog()
{
    DestroyConnections();
}

void CDisconnectDialog::Validate(CTransferInfo& ti)
{
    if (!OnDisconnect())
    {
        ti.ErrorOn(IDC_DISCONNECT_LIST);
        return;
    }
}

BOOL CDisconnectDialog::OnDisconnect()
{
    int index;
    int iStart = -1;
    while ((index = ListView_GetNextItem(HListView, iStart, LVNI_SELECTED)) != -1)
    {
        iStart = index;

        // We will only disconnect items, but leave them in the Connections array, which will be swept
        // in the destructor of this dialog or in case of Refresh() before returning FALSE from this method

        if (Connections[index].Type == citNetwork)
        {
            // NETWORK
            DISCDLGSTRUCT conn;
            conn.cbStructure = sizeof(conn);
            conn.hwndOwner = HWindow;
            if (stricmp(Connections[index].Name, LoadStr(IDS_NETWORK_NONE)) == 0)
            {
                conn.lpLocalName = Connections[index].Path;
                conn.lpRemoteName = NULL;
            }
            else
            {
                conn.lpLocalName = Connections[index].Name;
                conn.lpRemoteName = Connections[index].Path;
            }
            conn.dwFlags = DISC_UPDATE_PROFILE;
            if (WNetDisconnectDialog1(&conn) != NO_ERROR)
            {
                Refresh(); // We need to rebuild the array+listview to sweep away already disconnected items
                // without invalidating+updating the main window, the area will not be redrawn after closing the Disconnect dialog
                // below the dialog (the dialog has save-bits and apparently its stored background will not be violated,
                // probably paint optimization)
                InvalidateRect(MainWindow->HWindow, NULL, FALSE);
                UpdateWindow(MainWindow->HWindow);
                return FALSE;
            }
        }
        else
        {
            if (Connections[index].Type == citPlugin)
            {
                // PLUGIN FILE SYSTEM
                CPluginFSInterfaceAbstract* pluginFS = Connections[index].PluginFS;
                if (pluginFS != NULL)
                {
                    BOOL isInPanel = FALSE;
                    int panel = 0;
                    CPluginFSInterfaceEncapsulation* fs = NULL; // find encapsulation of the FS object
                    if (MainWindow->LeftPanel->Is(ptPluginFS) &&
                        MainWindow->LeftPanel->GetPluginFS()->Contains(pluginFS))
                    {
                        fs = MainWindow->LeftPanel->GetPluginFS();
                        isInPanel = TRUE;
                        panel = PANEL_LEFT;
                    }
                    if (fs == NULL && MainWindow->RightPanel->Is(ptPluginFS) &&
                        MainWindow->RightPanel->GetPluginFS()->Contains(pluginFS))
                    {
                        fs = MainWindow->RightPanel->GetPluginFS();
                        isInPanel = TRUE;
                        panel = PANEL_RIGHT;
                    }
                    if (fs == NULL)
                    {
                        CDetachedFSList* list = MainWindow->DetachedFSList;
                        int j;
                        for (j = 0; j < list->Count; j++)
                        {
                            CPluginFSInterfaceEncapsulation* detachedFS = list->At(j);
                            if (detachedFS->Contains(pluginFS))
                            {
                                fs = detachedFS;
                                break;
                            }
                        }
                    }
                    if (fs != NULL)
                    {
                        CPluginInterfaceForFSEncapsulation* ifaceForFS = fs->GetPluginInterfaceForFS();
                        if (!ifaceForFS->DisconnectFS(HWindow, isInPanel, panel, pluginFS,
                                                      fs->GetPluginFSName(), fs->GetPluginFSNameIndex()))
                        {              // disconnect was rejected (probably by the user)
                            Refresh(); // We need to rebuild the array+listview to sweep away already disconnected items
                            // without invalidating+updating the main window, the area will not be redrawn after closing the Disconnect dialog
                            // below the dialog (the dialog has save-bits and apparently its stored background will not be violated,
                            // probably paint optimization)
                            InvalidateRect(MainWindow->HWindow, NULL, FALSE);
                            UpdateWindow(MainWindow->HWindow);
                            return FALSE;
                        }
                    }
                    else
                        TRACE_E("CDisconnectDialog::OnDisconnect(): unexpected situation - unable to disconnect FS, because it not not found (most probably it has been already closed)!");
                }
                else
                    TRACE_E("CDisconnectDialog::OnDisconnect(): unexpected situation - CConnectionItem::PluginFS is NULL!");
            }
        }
    }
    // without invalidating+updating the main window, the area will not be redrawn after closing the Disconnect dialog
    // below the dialog (the dialog has save-bits and apparently its stored background will not be violated,
    // probably paint optimization)
    InvalidateRect(MainWindow->HWindow, NULL, FALSE);
    UpdateWindow(MainWindow->HWindow);
    return TRUE;
}

void CDisconnectDialog::EnableControls()
{
    int index;
    index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    EnableWindow(GetDlgItem(HWindow, IDOK), index != -1);
}

HIMAGELIST
CDisconnectDialog::CreateImageList()
{
    HIMAGELIST himl = ImageList_Create(16, 16, GetImageListColorFlags() | ILC_MASK, 4, 10);
    //  ImageList_SetBkColor(himl, GetSysColor(COLOR_WINDOW)); // aby pod XP chodily pruhledne ikonky

    // CONNECTION_ICON_NETWORK
    HICON hIcon = SalLoadImage(33, 10, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags); // accessible network drive
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(himl, -1, hIcon);
        HANDLES(DestroyIcon(hIcon));
    }
    else
        TRACE_E("Icon was not found!");

    // CONNECTION_ICON_PLUGIN
    hIcon = SalLoadIcon(HInstance, IDI_PLUGINFS, IconSizes[ICONSIZE_16]);
    ImageList_ReplaceIcon(himl, -1, hIcon);
    HANDLES(DestroyIcon(hIcon));

    // CONNECTION_ICON_ACCESSIBLE
    hIcon = SalLoadImage(33, 10, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags); // accessible network drive
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(himl, -1, hIcon);
        HANDLES(DestroyIcon(hIcon));
    }
    else
        TRACE_E("Icon was not found!");

    // CONNECTION_ICON_INACCESSIBLE
    hIcon = SalLoadImage(31, 11, IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags); // non-accessible network drive
    if (hIcon != NULL)
    {
        ImageList_ReplaceIcon(himl, -1, hIcon);
        HANDLES(DestroyIcon(hIcon));
    }
    else
        TRACE_E("Icon was not found!");

    return himl;
}

void CDisconnectDialog::InitColumns()
{
    CALL_STACK_MESSAGE1("CDisconnectDialog::InitColumns()");
    LV_COLUMN lvc;
    int header[2] = {IDS_DISCONNECT_NAME, IDS_DISCONNECT_PATH};

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_SUBITEM;
    lvc.fmt = LVCFMT_LEFT;
    int i;
    for (i = 0; i < 2; i++) // create columns
    {
        lvc.pszText = LoadStr(header[i]);
        lvc.iSubItem = i;
        ListView_InsertColumn(HListView, i, &lvc);
    }
    ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
    int width = ListView_GetColumnWidth(HListView, 0);
    int scrollW = GetSystemMetrics(SM_CXHSCROLL);
    ListView_SetColumnWidth(HListView, 0, width + 15 + scrollW);
    ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER); // remainder
    ListView_SetColumnWidth(HListView, 0, width + 15);               // space for scrollbar
}

char* CreateIndexedPluginPathText(const char* pathText, int index)
{
    char* newText = (char*)malloc(strlen(pathText) + 15); // reserve 14 characters (" [-1234567890]")
    if (newText != NULL)
        sprintf(newText, "%s [%d]", pathText, index);
    return newText;
}

void CDisconnectDialog::EnumConnections()
{
    CALL_STACK_MESSAGE1("CDisconnectDialog::EnumConnections()");

    // Network: CONNECTED resources

    char noneText[50]; // (none)
    lstrcpyn(noneText, LoadStr(IDS_NETWORK_NONE), sizeof(noneText));
    HANDLE hEnumNet;
    DWORD err = WNetOpenEnum(RESOURCE_CONNECTED, RESOURCETYPE_DISK, 0, NULL, &hEnumNet);
    if (err == ERROR_SUCCESS)
    {
        DWORD bufSize;
        DWORD entries = 0;
        char buffer[10000];
        NETRESOURCE* netSources = (NETRESOURCE*)buffer;
        while (1)
        {
            DWORD e = 0xFFFFFFFF; // as many as possible
            bufSize = 10000;
            err = WNetEnumResource(hEnumNet, &e, netSources, &bufSize);
            if (err == ERROR_SUCCESS && e > 0)
            {
                int i;
                for (i = 0; i < (int)e; i++) // processing new data
                {
                    const char* name = netSources[i].lpLocalName;
                    if (name == NULL)
                        name = noneText;
                    const char* path = netSources[i].lpRemoteName;
                    if (path == NULL)
                        path = "";

                    BOOL defaultItem = Panel->Is(ptDisk) && HasTheSameRootPath(Panel->GetPath(), name == noneText ? path : name);
                    InsertItem(-2, FALSE, citNetwork, CONNECTION_ICON_ACCESSIBLE, name, path, defaultItem, NULL); // Sort alphabetically -2
                }
                entries += e;
            }
            else
                break;
        }
        WNetCloseEnum(hEnumNet);
    }
    else
    {
        if (err != ERROR_NO_NETWORK)
            SalMessageBox(Parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR), MB_OK | MB_ICONEXCLAMATION);
    }

    // Network: REMEMBERED resources

    // available disks
    DWORD mask = GetLogicalDrives();

    err = WNetOpenEnum(RESOURCE_REMEMBERED, RESOURCETYPE_DISK, 0, NULL, &hEnumNet);
    if (err == ERROR_SUCCESS)
    {
        DWORD bufSize;
        DWORD entries = 0;
        char buffer[10000];
        NETRESOURCE* netSources = (NETRESOURCE*)buffer;
        while (1)
        {
            DWORD e = 0xFFFFFFFF; // as many as possible
            bufSize = 10000;
            err = WNetEnumResource(hEnumNet, &e, netSources, &bufSize);
            if (err == ERROR_SUCCESS && e > 0)
            {
                int i;
                for (i = 0; i < (int)e; i++) // processing new data
                {
                    const char* name = netSources[i].lpLocalName;
                    if (name != NULL)
                    {
                        BOOL iconIndex = CONNECTION_ICON_ACCESSIBLE;
                        char drv = LowerCase[name[0]];
                        if (drv >= 'a' && drv <= 'z' && (mask & (0x00000001 << (drv - 'a'))) == 0)
                            iconIndex = CONNECTION_ICON_INACCESSIBLE;

                        const char* path = netSources[i].lpRemoteName;
                        if (path == NULL)
                            path = "";

                        BOOL defaultItem = Panel->Is(ptDisk) && HasTheSameRootPath(Panel->GetPath(), *name == 0 ? path : name);
                        InsertItem(-2, TRUE, citNetwork, iconIndex, name, path, defaultItem, NULL); // Sort alphabetically -2
                    }
                }
                entries += e;
            }
            else
                break;
        }
        WNetCloseEnum(hEnumNet);
    }
    else
    {
        if (err != ERROR_NO_NETWORK)
            SalMessageBox(Parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR), MB_OK | MB_ICONEXCLAMATION);
    }

    // if we have inserted at least one network disk, we insert before the first network group
    if (Connections.Count > 0)
        InsertItem(0, FALSE, citGroup, CONNECTION_ICON_NETWORK, LoadStr(IDS_NETWORK_NETWORK), "", FALSE, NULL);

    int pluginGroupIndex = Connections.Count;

    // Plugins: FILE SYSTEMS

    CDetachedFSList* list = MainWindow->DetachedFSList;
    CPluginFSInterfaceEncapsulation** fsList = (CPluginFSInterfaceEncapsulation**)malloc(sizeof(CPluginFSInterfaceEncapsulation*) * (list->Count + 2));
    if (fsList != NULL)
    {
        CPluginFSInterfaceEncapsulation** fsListItem = fsList;
        CPluginFSInterfaceEncapsulation* activePanelFS = NULL;
        if (Panel->Is(ptPluginFS))
        {
            activePanelFS = Panel->GetPluginFS();
            *fsListItem++ = activePanelFS;
        }
        CFilesWindow* otherPanel = MainWindow->LeftPanel == Panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
        CPluginFSInterfaceEncapsulation* nonactivePanelFS = NULL;
        if (otherPanel->Is(ptPluginFS))
        {
            nonactivePanelFS = otherPanel->GetPluginFS();
            *fsListItem++ = nonactivePanelFS;
        }
        int i;
        for (i = 0; i < list->Count; i++)
            *fsListItem++ = list->At(i);
        int count = (int)(fsListItem - fsList);
        if (count > 1)
            SortPluginFSTimes(fsList, 0, count - 1);

        if (count > 0)
        {
            HWND oldPluginMsgBoxParent = PluginMsgBoxParent;
            PluginMsgBoxParent = HWindow;

            BOOL addFSItemForActivePanelFS = FALSE;
            BOOL addFSItemForNonactivePanelFS = FALSE;
            for (i = 0; i < count; i++)
            {
                CPluginFSInterfaceEncapsulation* fs = fsList[i];
                char* txt = NULL;
                HICON icon = NULL;
                BOOL destroyIcon = FALSE;
                if (fs->GetChangeDriveOrDisconnectItem(fs->GetPluginFSName(), txt, icon, destroyIcon))
                {
                    int iconIndex = -1;
                    if (icon != NULL)
                        iconIndex = ImageList_ReplaceIcon(HImageList, -1, icon);
                    if (destroyIcon && icon != NULL)
                        HANDLES(DestroyIcon(icon));
                    char* text = txt;
                    while (*text != 0 && *text != '\t')
                        text++;
                    if (*text == '\t')
                        text++;
                    char* s = text;
                    while (*s != 0 && *s != '\t')
                        s++;
                    *s = 0;
                    InsertItem(-1, FALSE, citPlugin, iconIndex != -1 ? iconIndex : CONNECTION_ICON_PLUGIN,
                               "", text, fs == activePanelFS, fs->GetInterface());
                    free(txt);
                }
                else // FS does not want to add any item
                {
                    if (fs == activePanelFS)
                        addFSItemForActivePanelFS = TRUE;
                    else if (fs == nonactivePanelFS)
                        addFSItemForNonactivePanelFS = TRUE;
                }
            }

            // we will check the uniqueness of the text items, and index duplicate items if necessary
            for (i = pluginGroupIndex; i < Connections.Count; i++)
            {
                BOOL freePluginPath = FALSE;
                char* pluginPath = Connections[i].Path;
                int currentIndex = 1;
                int x;
                for (x = i + 1; x < Connections.Count; x++)
                {
                    char* testedPath = Connections[x].Path;
                    if (StrICmp(pluginPath, testedPath) == 0) // match -> we need to index the item
                    {
                        if (!freePluginPath) // first match found, we need to index the first occurrence of a duplicate item as well
                        {
                            currentIndex = GetIndexForDrvText(fsList, count, Connections[i].PluginFS, currentIndex);
                            Connections[i].Path = CreateIndexedPluginPathText(pluginPath, currentIndex++);
                            if (Connections[i].Path == NULL)
                                Connections[i].Path = pluginPath;
                            else
                                freePluginPath = TRUE;
                        }
                        currentIndex = GetIndexForDrvText(fsList, count, Connections[x].PluginFS, currentIndex);
                        Connections[x].Path = CreateIndexedPluginPathText(testedPath, currentIndex++);
                        if (Connections[x].Path == NULL)
                            Connections[x].Path = testedPath;
                        else
                            free(testedPath);
                    }
                }
                if (freePluginPath)
                    free(pluginPath);
            }

            // we will ensure adding support for "unary" FS (RegEdit, WMobile and others) - they do not have items for open FS,
            // Allow only one open file system
            if (addFSItemForActivePanelFS || addFSItemForNonactivePanelFS)
            {
                int i2;
                for (i2 = 0; i2 < 2; i2++)
                {
                    if (i2 == 0 && addFSItemForActivePanelFS ||
                        i2 == 1 && addFSItemForNonactivePanelFS)
                    {
                        CPluginFSInterfaceEncapsulation* fs = i2 == 0 ? activePanelFS : nonactivePanelFS;
                        char path[2 * MAX_PATH];
                        sprintf(path, "%s:", fs->GetPluginFSName());
                        char* userPart = path + strlen(path);
                        if (!fs->GetRootPath(userPart))
                            *userPart = 0;

                        BOOL destroyIcon = FALSE;
                        HICON icon = fs->GetFSIcon(destroyIcon);
                        int iconIndex = -1;
                        if (icon != NULL)
                            iconIndex = ImageList_ReplaceIcon(HImageList, -1, icon);
                        if (destroyIcon && icon != NULL)
                            HANDLES(DestroyIcon(icon));
                        InsertItem(-1, FALSE, citPlugin, iconIndex != -1 ? iconIndex : CONNECTION_ICON_PLUGIN,
                                   "", path, fs == activePanelFS, fs->GetInterface());
                    }
                }
            }

            PluginMsgBoxParent = oldPluginMsgBoxParent;
        }
        free(fsList);
    }

    // If we have inserted at least one file system, we insert a group before the first plugin
    if (pluginGroupIndex < Connections.Count)
        InsertItem(pluginGroupIndex, FALSE, citGroup, CONNECTION_ICON_PLUGIN, LoadStr(IDS_NETWORK_PLUGINS), "", FALSE, NULL);
}

BOOL CDisconnectDialog::InsertItem(int index, BOOL ignoreDuplicate, CConnectionItemType type, int iconIndex, const char* name,
                                   const char* path, BOOL defaultItem, CPluginFSInterfaceAbstract* pluginFS)
{
    if (ignoreDuplicate)
    {
        int i;
        for (i = 0; i < Connections.Count; i++)
        {
            if (Connections[i].Type == type &&
                stricmp(Connections[i].Name, name) == 0 &&
                stricmp(Connections[i].Path, path) == 0)
            {
                return TRUE;
            }
        }
    }

    CConnectionItem item;
    ZeroMemory(&item, sizeof(item));

    item.Type = type;
    item.IconIndex = iconIndex;
    item.Name = DupStr(name);
    if (item.Name == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    item.Default = defaultItem;
    item.PluginFS = pluginFS;

    item.Path = DupStr(path);
    if (item.Path == NULL)
    {
        free(item.Name);
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    if (index == -2)
    {
        // we need to sort the items alphabetically (tche@thlsofts.be sent a screenshot showing that he
        // enumerators return a list randomly shuffled alphabetically
        int i;
        for (i = 0; i < Connections.Count; i++)
        {
            if (Connections[i].Type == type &&
                stricmp(Connections[i].Name, name) > 0)
            {
                index = i;
                break;
            }
        }
        if (index == -2)
            index = -1;
    }

    if (index == -1)
        Connections.Add(item);
    else
        Connections.Insert(index, item);

    if (!Connections.IsGood())
    {
        free(item.Name);
        free(item.Path);
        Connections.ResetState();
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    TRACE_I("ADDED: " << name << "    " << path);
    return TRUE;
}

void CDisconnectDialog::DestroyConnections()
{
    int i;
    for (i = 0; i < Connections.Count; i++)
    {
        free(Connections[i].Name);
        free(Connections[i].Path);
    }
    Connections.DestroyMembers();
}

void CDisconnectDialog::Refresh()
{
    DestroyConnections();
    EnumConnections();

    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);
    ListView_DeleteAllItems(HListView);

    int defaultIndex = -1;
    int i;
    for (i = 0; i < Connections.Count; i++)
    {
        LVITEM lvi;
        lvi.mask = LVIF_TEXT | LVIF_IMAGE | LVIF_PARAM | LVIF_INDENT;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        lvi.iImage = Connections[i].IconIndex;
        lvi.pszText = Connections[i].Name;
        lvi.lParam = i; // for later sorting
        lvi.iIndent = (Connections[i].Type == citGroup) ? 0 : 1;
        int index = ListView_InsertItem(HListView, &lvi);
        ListView_SetItemText(HListView, index, 1, Connections[i].Path);
        if (Connections[i].Default)
        {
            if (defaultIndex == -1)
                defaultIndex = i;
            else
                TRACE_E("CDisconnectDialog::Refresh(): Only one item should have set Default==TRUE; ignoring others.");
        }
    }
    ListView_SetImageList(HListView, HImageList, LVSIL_SMALL);

    DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
    if (defaultIndex == -1)
        defaultIndex = 1;
    ListView_SetItemState(HListView, defaultIndex, state, state);
    ListView_EnsureVisible(HListView, defaultIndex, FALSE);

    ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
    ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);

    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);
}

INT_PTR
CDisconnectDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HListView = GetDlgItem(HWindow, IDC_DISCONNECT_LIST);
        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        HImageList = CreateImageList();
        ListView_SetImageList(HListView, HImageList, LVSIL_SMALL);

        InitColumns();
        Refresh();

        if (ListView_GetItemCount(HListView) == 0)
        {
            SendMessage(HWindow, WM_COMMAND, IDCANCEL, 0);
            NoConncection = TRUE;
            return 0;
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_DISCONNECT_LIST)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                if (IsWindowEnabled(GetDlgItem(HWindow, IDOK)))
                    PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDOK, BN_CLICKED), 0);
                break;
            }

            case LVN_ITEMCHANGED:
            {
                int i;
                for (i = 0; i < ListView_GetItemCount(HListView); i++)
                {
                    LVITEM lvi;
                    lvi.iItem = i;
                    lvi.iSubItem = 0;
                    lvi.mask = LVIF_PARAM;
                    ListView_GetItem(HListView, &lvi);
                    int index = (int)lvi.lParam;
                    if (index < 0 || index >= Connections.Count || // for safety
                        Connections[index].Type == citGroup)
                    {
                        // we will shoot down SELECTION
                        ListView_SetItemState(HListView, i, 0, LVIS_SELECTED);
                    }
                }

                EnableControls();
                return 0;
            }
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

//****************************************************************************
//
// CSaveSelectionDialog
//

CSaveSelectionDialog::CSaveSelectionDialog(HWND hParent, BOOL* clipboard)
    : CCommonDialog(HLanguage, IDD_SAVESELECTION, IDD_SAVESELECTION, hParent)
{
    Clipboard = clipboard;
}

void CSaveSelectionDialog::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_SAVESEL_MEMORY, FALSE, *Clipboard);
    ti.RadioButton(IDC_SAVESEL_CLIPBOARD, TRUE, *Clipboard);
}

//****************************************************************************
//
// CLoadSelectionDialog
//

CLoadSelectionDialog::CLoadSelectionDialog(HWND hParent, CLoadSelectionOperation* operation, BOOL* clipboard,
                                           BOOL clipboardValid, BOOL globalValid)
    : CCommonDialog(HLanguage, IDD_LOADSELECTION, IDD_LOADSELECTION, hParent)
{
    Operation = operation;
    Clipboard = clipboard;
    ClipboardValid = clipboardValid;
    GlobalValid = globalValid;
}

void CLoadSelectionDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        if (!ClipboardValid)
            EnableWindow(GetDlgItem(HWindow, IDC_LOADSEL_CLIPBOARD), FALSE);
        if (!GlobalValid)
            EnableWindow(GetDlgItem(HWindow, IDC_LOADSEL_MEMORY), FALSE);
    }
    ti.RadioButton(IDC_LOADSEL_MEMORY, FALSE, *Clipboard);
    ti.RadioButton(IDC_LOADSEL_CLIPBOARD, TRUE, *Clipboard);
    int op = *Operation;
    ti.RadioButton(IDC_LOADSEL_COPY, lsoCOPY, op);
    ti.RadioButton(IDC_LOADSEL_OR, lsoOR, op);
    ti.RadioButton(IDC_LOADSEL_DIFF, lsoDIFF, op);
    ti.RadioButton(IDC_LOADSEL_AND, lsoAND, op);
    *Operation = (CLoadSelectionOperation)op;
}

//****************************************************************************
//
// CCompareDirsDialog
//

CCompareDirsDialog::CCompareDirsDialog(HWND hParent, BOOL enableByDateAndTime, BOOL enableBySize,
                                       BOOL enableByAttrs, BOOL enableByContent, BOOL enableSubdirs,
                                       BOOL enableCompAttrsOfSubdirs, CFilesWindow* leftPanel,
                                       CFilesWindow* rightPanel)
    : CCommonDialog(HLanguage, IDD_COMPAREDIRS, IDD_COMPAREDIRS, hParent)
{
    EnableByDateAndTime = enableByDateAndTime;
    EnableBySize = enableBySize;
    EnableByAttrs = enableByAttrs;
    EnableByContent = enableByContent;
    EnableSubdirs = enableSubdirs;
    EnableCompAttrsOfSubdirs = enableCompAttrsOfSubdirs;
    LeftPanel = leftPanel;
    RightPanel = rightPanel;
    Expanded = TRUE;
}

BOOL ValidateMask(HWND parent, CTransferInfo& ti, int checkbox, int editline)
{
    BOOL ignore;
    ti.CheckBox(checkbox, ignore);
    if (ignore)
    {
        char buf[MAX_PATH];
        ti.EditLine(editline, buf, MAX_PATH);
        CMaskGroup masks(buf);
        int errorPos;
        if (!masks.PrepareMasks(errorPos))
        {
            SalMessageBox(parent, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
            SetFocus(GetDlgItem(parent, editline));
            SendMessage(GetDlgItem(parent, editline), EM_SETSEL, errorPos, errorPos + 1);
            ti.ErrorOn(editline);
            return FALSE;
        }
    }
    return TRUE;
}

void CCompareDirsDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CCompareDirsDialog::Validate()");

    BOOL ret = TRUE;
    if (ret)
        ret &= ValidateMask(HWindow, ti, IDC_COMPARE_IGNORE_FILES, IDE_COMPARE_IGNORE_FILES);
    if (ret)
        ret &= ValidateMask(HWindow, ti, IDC_COMPARE_IGNORE_DIRS, IDE_COMPARE_IGNORE_DIRS);
}

void CCompareDirsDialog::Transfer(CTransferInfo& ti)
{
    if (EnableBySize)
        ti.CheckBox(IDC_COMPARE_BYSIZE, Configuration.CompareBySize);
    if (EnableByDateAndTime)
        ti.CheckBox(IDC_COMPARE_BYTIME, Configuration.CompareByTime);
    if (EnableByAttrs)
        ti.CheckBox(IDC_COMPARE_BYATTR, Configuration.CompareByAttr);
    if (EnableByContent)
        ti.CheckBox(IDC_COMPARE_BYCONTENT, Configuration.CompareByContent);
    if (EnableSubdirs)
    {
        ti.CheckBox(IDC_COMPARE_SUBDIRS, Configuration.CompareSubdirs);
        if (EnableCompAttrsOfSubdirs)
            ti.CheckBox(IDC_COMPARE_SUBDIRS_ATTR, Configuration.CompareSubdirsAttr);
    }

    ti.CheckBox(IDC_COMPARE_ONE_PANEL_DIRS, Configuration.CompareOnePanelDirs);
    ti.CheckBox(IDC_COMPARE_IGNORE_FILES, Configuration.CompareIgnoreFiles);
    if (ti.Type == ttDataToWindow || EnableSubdirs || Configuration.CompareOnePanelDirs)
        ti.CheckBox(IDC_COMPARE_IGNORE_DIRS, Configuration.CompareIgnoreDirs);

    // we provide MasksString, there is a range check, nothing to worry about
    ti.EditLine(IDE_COMPARE_IGNORE_FILES, (char*)Configuration.CompareIgnoreFilesMasks.GetWritableMasksString(), MAX_PATH);
    // we provide MasksString, there is a range check, nothing to worry about
    ti.EditLine(IDE_COMPARE_IGNORE_DIRS, (char*)Configuration.CompareIgnoreDirsMasks.GetWritableMasksString(), MAX_PATH);

    if (ti.Type == ttDataToWindow)
    {
        EnableControls();
    }
    else
    {
        Configuration.CompareMoreOptions = Expanded;
    }
}

void CCompareDirsDialog::EnableControls()
{
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_BYSIZE), EnableBySize);
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_BYTIME), EnableByDateAndTime);
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_BYATTR), EnableByAttrs);
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_BYCONTENT), EnableByContent);
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_SUBDIRS), EnableSubdirs);
    BOOL subdirs = EnableCompAttrsOfSubdirs &&
                   EnableSubdirs && IsDlgButtonChecked(HWindow, IDC_COMPARE_SUBDIRS) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_ONE_PANEL_DIRS), !subdirs);
    if (subdirs)
        CheckDlgButton(HWindow, IDC_COMPARE_ONE_PANEL_DIRS, BST_UNCHECKED);
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_SUBDIRS_ATTR), subdirs);
    if (!subdirs)
        CheckDlgButton(HWindow, IDC_COMPARE_SUBDIRS_ATTR, BST_UNCHECKED);

    BOOL ignoreFiles = IsDlgButtonChecked(HWindow, IDC_COMPARE_IGNORE_FILES);
    EnableWindow(GetDlgItem(HWindow, IDE_COMPARE_IGNORE_FILES), ignoreFiles);

    BOOL onePanelDirs = IsDlgButtonChecked(HWindow, IDC_COMPARE_ONE_PANEL_DIRS) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDC_COMPARE_IGNORE_DIRS), subdirs || onePanelDirs);
    if (!subdirs && !onePanelDirs)
        CheckDlgButton(HWindow, IDC_COMPARE_IGNORE_DIRS, BST_UNCHECKED);

    BOOL ignoreDirs = IsDlgButtonChecked(HWindow, IDC_COMPARE_IGNORE_DIRS);
    EnableWindow(GetDlgItem(HWindow, IDE_COMPARE_IGNORE_DIRS), ignoreDirs);
}

HDWP CCompareDirsDialog::OffsetControl(HDWP hdwp, int id, int yOffset)
{
    HWND hCtrl = GetDlgItem(HWindow, id);
    RECT r;
    GetWindowRect(hCtrl, &r);
    ScreenToClient(HWindow, (LPPOINT)&r);

    hdwp = HANDLES(DeferWindowPos(hdwp, hCtrl, NULL, r.left, r.top + yOffset, 0, 0, SWP_NOSIZE | SWP_NOZORDER));
    return hdwp;
}

void CCompareDirsDialog::DisplayMore(BOOL more)
{
    // Hidden controls must be hidden in order to exclude them from the tab order
    int controls[] = {IDC_COMPARE_SUBDIRS_ATTR, IDC_COMPARE_SUBDIRS_ATTR_DESCR,
                      IDC_COMPARE_ONE_PANEL_DIRS, IDC_COMPARE_MORE_OPTIONS, IDC_COMPARE_MORE_OPTIONS_SEP,
                      IDC_COMPARE_IGNORE_FILES, IDE_COMPARE_IGNORE_FILES, IDC_COMPARE_IGNORE_DIRS,
                      IDE_COMPARE_IGNORE_DIRS, IDC_FILEMASK_HINT, -1};

    int wndHeight = OriginalHeight;
    if (!more)
        wndHeight -= SpacerHeight;
    SetWindowPos(HWindow, NULL, 0, 0, OriginalWidth, wndHeight,
                 SWP_NOZORDER | SWP_NOMOVE);

    HWND hFocus = GetFocus();
    int i;
    for (i = 0; controls[i] != -1; i++)
    {
        HWND hCtrl = GetDlgItem(HWindow, controls[i]);
        if (!more && hCtrl == hFocus)
        {
            SendMessage(HWindow, DM_SETDEFID, IDOK, 0);
            SetFocus(GetDlgItem(HWindow, IDOK));
        }
        ShowWindow(hCtrl, more ? SW_SHOW : SW_HIDE);
    }

    int yOffset = more ? SpacerHeight : -SpacerHeight;

    HDWP hdwp = HANDLES(BeginDeferWindowPos(5));
    if (hdwp != NULL)
    {
        hdwp = OffsetControl(hdwp, IDC_COMPARE_BUTTONS_SEP, yOffset);
        hdwp = OffsetControl(hdwp, IDOK, yOffset);
        hdwp = OffsetControl(hdwp, IDCANCEL, yOffset);
        hdwp = OffsetControl(hdwp, IDC_MORE, yOffset);
        hdwp = OffsetControl(hdwp, IDHELP, yOffset);
        HANDLES(EndDeferWindowPos(hdwp));
    }
    CheckDlgButton(HWindow, IDC_MORE, more ? BST_CHECKED : BST_UNCHECKED);
    if (!more)
    {
        CheckDlgButton(HWindow, IDC_COMPARE_SUBDIRS_ATTR, BST_UNCHECKED);
        CheckDlgButton(HWindow, IDC_COMPARE_ONE_PANEL_DIRS, BST_UNCHECKED);
        CheckDlgButton(HWindow, IDC_COMPARE_IGNORE_FILES, BST_UNCHECKED);
        CheckDlgButton(HWindow, IDC_COMPARE_IGNORE_DIRS, BST_UNCHECKED);
    }
    Expanded = more;
}

INT_PTR
CCompareDirsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {

    case WM_INITDIALOG:
    {
        new CButton(HWindow, IDC_MORE, BTF_MORE | BTF_CHECKBOX);
        CheckDlgButton(HWindow, IDC_MORE, BST_CHECKED);

        // now we are in full size => we will measure the dialog
        RECT r;
        GetWindowRect(HWindow, &r);
        OriginalWidth = r.right - r.left;
        OriginalHeight = r.bottom - r.top;
        // original position of buttons
        GetWindowRect(GetDlgItem(HWindow, IDOK), &r);
        ScreenToClient(HWindow, (LPPOINT)&r);
        OriginalButtonsY = r.top;
        // height of the delimiter
        GetWindowRect(GetDlgItem(HWindow, IDC_CM_SPACER), &r);
        SpacerHeight = r.bottom - r.top;

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        if (!Configuration.CompareMoreOptions)
            DisplayMore(FALSE);

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_MORE:
            {
                DisplayMore(!Expanded /*, FALSE*/);
                break;
            }

            case IDC_COMPARE_IGNORE_FILES:
            case IDC_COMPARE_IGNORE_DIRS:
            case IDC_COMPARE_ONE_PANEL_DIRS:
            case IDC_COMPARE_SUBDIRS_ATTR:
            {
                if (!Expanded)
                    DisplayMore(TRUE /*, FALSE*/);
                break;
            }
            }

            EnableControls();

            // If the user clicks on the checkbox to enable the mask, they probably want to edit it
            if (LOWORD(wParam) == IDC_COMPARE_IGNORE_FILES)
            {
                if (IsDlgButtonChecked(HWindow, IDC_COMPARE_IGNORE_FILES))
                    SendMessage(HWindow, WM_NEXTDLGCTL, FALSE, FALSE); // focus on the mask
            }
            if (LOWORD(wParam) == IDC_COMPARE_IGNORE_DIRS)
            {
                if (IsDlgButtonChecked(HWindow, IDC_COMPARE_IGNORE_DIRS))
                    SendMessage(HWindow, WM_NEXTDLGCTL, FALSE, FALSE); // focus on the mask
            }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCmpDirProgressDialog
//

CCmpDirProgressDialog::CCmpDirProgressDialog(HWND hParent, BOOL hasProgress, CITaskBarList3* taskBarList3)
    : CCommonDialog(HLanguage, hasProgress ? IDD_CMPDIR_PROGRESS : IDD_CMPDIR_PROGRESS2, hParent, ooStatic)
{
    HasProgress = hasProgress;
    DelayedSource[0] = 0;
    DelayedTarget[0] = 0;
    DelayedSourceDirty = FALSE;
    DelayedTargetDirty = FALSE;
    Source = NULL;
    Target = NULL;
    Progress = NULL;
    TotalProgress = NULL;
    FileSize = CQuadWord(1, 0); // Protection against division by zero
    ActualFileSize = CQuadWord(0, 0);
    TotalSize = CQuadWord(1, 0); // Protection against division by zero
    ActualTotalSize = CQuadWord(0, 0);
    Cancel = FALSE;
    LastTickCount = 0;
    TaskBarList3 = taskBarList3;
}

void CCmpDirProgressDialog::SetSource(const char* text)
{
    lstrcpyn(DelayedSource, text, 2 * MAX_PATH);
    DelayedSourceDirty = TRUE;
}

void CCmpDirProgressDialog::SetTarget(const char* text)
{
    lstrcpyn(DelayedTarget, text, 2 * MAX_PATH);
    DelayedTargetDirty = TRUE;
}

void CCmpDirProgressDialog::SetFileSize(const CQuadWord& size)
{
    FileSize = max(CQuadWord(1, 0), size); // Protection against division by zero
}

void CCmpDirProgressDialog::SetActualFileSize(const CQuadWord& size)
{
    ActualFileSize = max(CQuadWord(0, 0), size);
    SizeIsDirty = TRUE;
}

void CCmpDirProgressDialog::SetTotalSize(const CQuadWord& size)
{
    TotalSize = max(CQuadWord(1, 0), size); // Protection against division by zero
}

void CCmpDirProgressDialog::SetActualTotalSize(const CQuadWord& size)
{
    ActualTotalSize = max(CQuadWord(0, 0), size);
    SizeIsDirty = TRUE;
}

void CCmpDirProgressDialog::GetActualTotalSize(CQuadWord& size)
{
    size = ActualTotalSize;
}

void CCmpDirProgressDialog::AddSize(const CQuadWord& size)
{
    CALL_STACK_MESSAGE_NONE;
    if (size != CQuadWord(0, 0))
    {
        ActualFileSize += size;
        ActualTotalSize += size; //CQuadWord(size, 0)
        SizeIsDirty = TRUE;
    }
}

BOOL CCmpDirProgressDialog::Continue()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // we will focus on the user for a moment ...
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // Every 100ms we redraw the changed data (text + progress bars)
    DWORD ticks = GetTickCount();
    if (ticks - LastTickCount > 100)
    {
        FlushDataToControls();
        LastTickCount = GetTickCount();
    }

    return !Cancel;
}

void CCmpDirProgressDialog::FlushDataToControls()
{
    // text
    if (DelayedSourceDirty && Source != NULL)
    {
        Source->SetText(DelayedSource);
        DelayedSourceDirty = FALSE;
    }

    // text
    if (DelayedTargetDirty && Target != NULL)
    {
        Target->SetText(DelayedTarget);
        DelayedTargetDirty = FALSE;
    }

    // progress bar
    if (SizeIsDirty)
    {
        if (Progress != NULL)
            Progress->SetProgress2(ActualFileSize, FileSize);
        if (TotalProgress != NULL)
            TotalProgress->SetProgress2(ActualTotalSize, TotalSize);
        if (TotalProgress != NULL)
            TaskBarList3->SetProgress2(ActualTotalSize, TotalSize);
        SizeIsDirty = FALSE;
    }
}

INT_PTR
CCmpDirProgressDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (HasProgress)
        {
            if ((Progress = new CProgressBar(HWindow, IDF_OPERATION)) == NULL)
                TRACE_E(LOW_MEMORY);
            if ((TotalProgress = new CProgressBar(HWindow, IDF_SUMMARY)) == NULL)
                TRACE_E(LOW_MEMORY);
        }
        if ((Source = new CStaticText(HWindow, IDS_SOURCE, STF_PATH_ELLIPSIS | STF_CACHED_PAINT)) == NULL)
            TRACE_E(LOW_MEMORY);
        if ((Target = new CStaticText(HWindow, IDS_TARGET, STF_PATH_ELLIPSIS | STF_CACHED_PAINT)) == NULL)
            TRACE_E(LOW_MEMORY);

        // When opening the dialog, we set the parent msgbox for plugins to this dialog (only the main thread)
        // Restoring PluginMsgBoxParent in CCommonDialog::DialogProc on WM_DESTROY
        if (MainThreadID == GetCurrentThreadId())
        {
            HOldPluginMsgBoxParent = PluginMsgBoxParent;
            PluginMsgBoxParent = HWindow;
        }

        FlushDataToControls();
        break;
    }

    case WM_DESTROY:
    {
        TaskBarList3->SetProgressState(TBPF_NOPROGRESS);
        break;
    }

    case WM_COMMAND:
    {
        // if the user clicked on the Cancel button and did not confirm it earlier, we will ask
        if (LOWORD(wParam) == IDCANCEL && !Cancel)
        {
            // Button Cancel must be enabled
            if (IsWindowEnabled(GetDlgItem(HWindow, IDCANCEL)))
            {
                // Redraw explicitly before displaying the messagebox
                FlushDataToControls();

                // Ask the user if they want to interrupt the operation
                Cancel = (SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION), LoadStr(IDS_QUESTION),
                                        MB_YESNO | MB_ICONQUESTION) == IDYES);
            }
        }
        // we will not let the command fail, otherwise the dialog will close on us
        return TRUE;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// Exiting OpenSal
//

CExitingOpenSal::CExitingOpenSal(HWND hParent)
    : CCommonDialog(HLanguage, IDD_EXITINGOPENSAL, hParent)
{
    NextOpenedDlgIndex = 0;
}

INT_PTR
CExitingOpenSal::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetTimer(HWindow, 666, 200, NULL);
        PostMessage(HWindow, WM_TIMER, 666, 0);
        break;
    }

    case WM_TIMER:
    {
        if (wParam == 666)
        {
            int c = ProgressDlgArray.RemoveFinishedDlgs();
            if (c == 0)
                EndDialog(HWindow, IDOK); // End of Salamander is possible
            else
            {
                char num[50];
                itoa(c, num, 10);
                char buf[50];
                GetDlgItemText(HWindow, IDT_RUNNINGOPERS, buf, 50);
                buf[49] = 0;
                if (strcmp(buf, num) != 0)
                    SetDlgItemText(HWindow, IDT_RUNNINGOPERS, num);
            }
            return TRUE;
        }
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_CANCELALLOPERS:
        {
            ProgressDlgArray.PostCancelToAllDlgs();
            return TRUE;
        }

        case IDB_FOCUSNEXTOPER:
        {
            HWND dlg = ProgressDlgArray.GetNextOpenedDlg(&NextOpenedDlgIndex);
            if (dlg != NULL)
                PostMessage(dlg, WM_USER_FOCUSPROGRDLG, 0, 0);
            return TRUE;
        }
        }
        break;
    }

    case WM_DESTROY:
    {
        KillTimer(HWindow, 666);
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfirmADSLossDlg
//

CConfirmADSLossDlg::CConfirmADSLossDlg(HWND parent, BOOL isFile, const char* name,
                                       const char* streams, BOOL isMove) : CCommonDialog(HLanguage, IDD_CONFIRMADSLOSS, parent)
{
    IsFile = isFile;
    IsMove = isMove;
    Name = name;
    Streams = streams;
}

INT_PTR
CConfirmADSLossDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfirmADSLossDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(Name);
        else
            TRACE_E(LOW_MEMORY);

        SetDlgItemText(HWindow, IDE_ALTSTREAMS, Streams);

        if (IsFile)
            SetWindowText(GetDlgItem(HWindow, IDT_FILEORDIR), LoadStr(IDS_FILETITLE));

        int resId;
        if (IsMove)
            resId = IsFile ? IDS_CONFADSLOSS_WARNING_MOVE_FILE : IDS_CONFADSLOSS_WARNING_MOVE_DIR;
        else
            resId = IsFile ? IDS_CONFADSLOSS_WARNING_COPY_FILE : IDS_CONFADSLOSS_WARNING_COPY_DIR;
        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), LoadStr(resId));
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDB_ALL || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfirmLinkTgtCopyDlg
//

CConfirmLinkTgtCopyDlg::CConfirmLinkTgtCopyDlg(HWND parent, const char* name, const char* details) : CCommonDialog(HLanguage, IDD_CONFIRMLINKTGTCOPY, parent)
{
    Name = name;
    Details = details;
}

INT_PTR
CConfirmLinkTgtCopyDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfirmLinkTgtCopyDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS);
        name->SetTextToDblQuotesIfNeeded(Name);
        SetDlgItemText(HWindow, IDS_DETAILS, Details);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDB_ALL || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CConfirmEncryptionLossDlg
//

CConfirmEncryptionLossDlg::CConfirmEncryptionLossDlg(HWND parent, BOOL isFile, const char* name,
                                                     BOOL isMove) : CCommonDialog(HLanguage, IDD_CONFIRMENCRYPTLOSS, parent)
{
    IsFile = isFile;
    IsMove = isMove;
    Name = name;
}

INT_PTR
CConfirmEncryptionLossDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfirmEncryptionLossDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(Name);
        else
            TRACE_E(LOW_MEMORY);

        if (IsFile)
            SetWindowText(GetDlgItem(HWindow, IDT_FILEORDIR), LoadStr(IDS_FILETITLE));

        int resId;
        if (IsMove)
            resId = IsFile ? IDS_CONFENCLOSS_WARNING_MOVE_FILE : IDS_CONFENCLOSS_WARNING_MOVE_DIR;
        else
            resId = IsFile ? IDS_CONFENCLOSS_WARNING_COPY_FILE : IDS_CONFENCLOSS_WARNING_COPY_DIR;
        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), LoadStr(resId));
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_SKIP || LOWORD(wParam) == IDB_SKIPALL ||
            LOWORD(wParam) == IDB_ALL || LOWORD(wParam) == IDYES || LOWORD(wParam) == IDNO)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CErrorReadingADSDlg
//

CErrorReadingADSDlg::CErrorReadingADSDlg(HWND parent, const char* file, const char* error, const char* title) : CCommonDialog(HLanguage, IDD_CANNOTGETADSINFO, parent)
{
    File = file;
    Error = error;
    Title = title;
}

INT_PTR
CErrorReadingADSDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CErrorReadingADSDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Title != NULL)
        {
            SetWindowText(HWindow, Title);
            Title = NULL; // It's from LoadStr, its lifespan will soon expire (it will be overwritten), better to NULL it out
        }

        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(File);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR), Error);

        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_IGNORE || LOWORD(wParam) == IDB_IGNOREALL ||
            LOWORD(wParam) == IDRETRY)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CErrorSettingAttrsDlg
//

CErrorSettingAttrsDlg::CErrorSettingAttrsDlg(HWND parent, const char* file, DWORD neededAttrs,
                                             DWORD currentAttrs) : CCommonDialog(HLanguage, IDD_CANNOTSETATTRSINFO, parent)
{
    File = file;
    NeededAttrs = neededAttrs;
    CurrentAttrs = currentAttrs;
}

void GetAttrsString(char* text, DWORD attrs)
{
    // if we support displaying another attribute,
    // It is necessary to expand InternalGetAttr() and the DISPLAYED_ATTRIBUTES mask
    int l = 0;
    if (attrs & FILE_ATTRIBUTE_READONLY)
        text[l++] = 'R';
    if (attrs & FILE_ATTRIBUTE_HIDDEN)
        text[l++] = 'H';
    if (attrs & FILE_ATTRIBUTE_SYSTEM)
        text[l++] = 'S';
    if (attrs & FILE_ATTRIBUTE_ARCHIVE)
        text[l++] = 'A';
    if (attrs & FILE_ATTRIBUTE_TEMPORARY)
        text[l++] = 'T';
    if (attrs & FILE_ATTRIBUTE_COMPRESSED)
        text[l++] = 'C';
    if (attrs & FILE_ATTRIBUTE_ENCRYPTED)
        text[l++] = 'E';
    if (attrs & FILE_ATTRIBUTE_OFFLINE)
        text[l++] = 'O';
    text[l] = 0;
}

INT_PTR
CErrorSettingAttrsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CErrorSettingAttrsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_FILENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(File);
        else
            TRACE_E(LOW_MEMORY);

        char text[20];
        GetAttrsString(text, NeededAttrs);
        SetWindowText(GetDlgItem(HWindow, IDS_NEEDEDATTRS), text);
        GetAttrsString(text, CurrentAttrs);
        SetWindowText(GetDlgItem(HWindow, IDS_CURRENTATTRS), text);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_IGNORE || LOWORD(wParam) == IDB_IGNOREALL)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CErrorCopyingPermissionsDlg
//

CErrorCopyingPermissionsDlg::CErrorCopyingPermissionsDlg(HWND parent, const char* sourceFile,
                                                         const char* targetFile, DWORD error) : CCommonDialog(HLanguage, IDD_CANNOTCOPYPERMISSIONS, parent)
{
    SourceFile = sourceFile;
    TargetFile = targetFile;
    Error = error;
}

INT_PTR
CErrorCopyingPermissionsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CErrorCopyingPermissionsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_SOURCENAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(SourceFile);
        else
            TRACE_E(LOW_MEMORY);

        if ((name = new CStaticText(HWindow, IDS_TARGETNAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(TargetFile);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR),
                      Error != NO_ERROR ? GetErrorText(Error) : LoadStr(IDS_VIEWER_UNKNOWNERR));
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_IGNORE || LOWORD(wParam) == IDB_IGNOREALL)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CErrorCopyingDirTimeDlg
//

CErrorCopyingDirTimeDlg::CErrorCopyingDirTimeDlg(HWND parent, const char* targetFile, DWORD error) : CCommonDialog(HLanguage, IDD_CANNOTCOPYDIRTIME, parent)
{
    TargetFile = targetFile;
    Error = error;
}

INT_PTR
CErrorCopyingDirTimeDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CErrorCopyingDirTimeDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        CStaticText* name;
        if ((name = new CStaticText(HWindow, IDS_TARGETNAME, STF_PATH_ELLIPSIS)) != NULL)
            name->SetTextToDblQuotesIfNeeded(TargetFile);
        else
            TRACE_E(LOW_MEMORY);

        SetWindowText(GetDlgItem(HWindow, IDS_ERROR),
                      Error != NO_ERROR ? GetErrorText(Error) : LoadStr(IDS_VIEWER_UNKNOWNERR));
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDB_IGNORE || LOWORD(wParam) == IDB_IGNOREALL)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CDriveSelectErrDlg
//

CDriveSelectErrDlg::CDriveSelectErrDlg(HWND parent, const char* errText, const char* drvPath) : CCommonDialog(HLanguage, IDD_DRIVESELECTERR, parent)
{
    ErrText = errText;
    lstrcpyn(DrvPath, drvPath, MAX_PATH);
    CounterForAllowedUseOfTimer = 5 * 60; // we will try it for a maximum of 5 minutes, then let them press Retry themselves (precaution against "hammering" the drive)
}

INT_PTR
CDriveSelectErrDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CDriveSelectErrDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        LastDriveSelectErrDlgHWnd = HWindow;
        HICON hIcon = HANDLES(LoadIcon(NULL, IDI_EXCLAMATION));
        SendDlgItemMessage(HWindow, IDI_EXCLAMATIONICON, STM_SETICON, (WPARAM)hIcon, 0);
        SetDlgItemText(HWindow, IDT_ERRTEXT, ErrText);
        MessageBeep(MB_ICONEXCLAMATION);

        // Let's determine if it makes sense to periodically test the readiness of drives (excluding floppy (noisy) and network (long outages possible))
        BOOL setTimer = TRUE;
        UINT drvType = MyGetDriveType(DrvPath);
        // WARNING: unfortunately, mountpoints return drive-type DRIVE_NO_ROOT_DIR if there is no inserted medium, unbelievable ... so
        // jsem to musel oprasit (periodicke testy i pri drvType == DRIVE_NO_ROOT_DIR)
        if (drvType == DRIVE_REMOVABLE || drvType == DRIVE_CDROM || drvType == DRIVE_NO_ROOT_DIR)
        {
            char root[MAX_PATH];
            GetRootPath(root, DrvPath);
            switch (GetDriveType(root))
            {
            case DRIVE_REMOVABLE: // we will test if it is not about floppy disks (apparently they cannot be put into a mount point, so this is enough)
            {
                int drv = UpperCase[root[0]] - 'A' + 1;
                if (drv >= 1 && drv <= 26) // for safety we will perform a "range-check"
                {
                    DWORD medium = GetDriveFormFactor(drv);
                    switch (medium)
                    {
                    case 1:
                    case 350:
                    case 525:
                    case 800:
                        setTimer = FALSE;
                        break; // it's about a floppy
                    }
                }
                break;
            }

            case DRIVE_FIXED: // mount-point, we will determine the root of the removable drive
            {
                if (!GetCurrentLocalReparsePoint(DrvPath, root))
                    setTimer = FALSE; // cannot be a mount point, no periodic tests
                break;
            }

            case DRIVE_NO_ROOT_DIR:
                setTimer = FALSE;
                break; // I don't know how it's going, let's leave it without periodic tests
            }
            lstrcpyn(DrvPath, root, MAX_PATH);
        }
        else
            setTimer = FALSE; // It's probably a network connection issue

        if (setTimer)
            SetTimer(HWindow, 3725, 1000, NULL);

        break;
    }

    case WM_TIMER:
    {
        if (wParam == 3725 && GetForegroundWindow() == HWindow) // test if it is already available earlier
        {
            KillTimer(HWindow, 3725);

            // we used to check the accessibility of a drive using FindFirstFile, because using SalGetFileAttributes
            // returns success for junction points always (the "directories" attribute is not related to the content)
            BOOL ok = FALSE;
            char fileName[MAX_PATH + 10];
            lstrcpyn(fileName, DrvPath, MAX_PATH + 10);
            if (SalPathAppend(fileName, "*", MAX_PATH + 10))
            {
                WIN32_FIND_DATA fileData;
                HANDLE search;
                search = HANDLES_Q(FindFirstFile(fileName, &fileData));
                if (search == INVALID_HANDLE_VALUE)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_NO_MORE_FILES)
                        ok = TRUE;
                }
                else
                {
                    ok = TRUE;
                    HANDLES(FindClose(search));
                }
            }

            if (ok)
                PostMessage(HWindow, WM_COMMAND, IDRETRY, 0);
            else
            {
                if (--CounterForAllowedUseOfTimer > 0)
                    SetTimer(HWindow, 3725, 1000, NULL);
            }
        }
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDRETRY)
        {
            if (Modal)
                EndDialog(HWindow, LOWORD(wParam));
            else
                DestroyWindow(HWindow);
            return TRUE;
        }
        break;
    }

    case WM_DESTROY:
    {
        KillTimer(HWindow, 3725);
        LastDriveSelectErrDlgHWnd = NULL;
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCfgPageIconOvrls
//

CCfgPageIconOvrls::CCfgPageIconOvrls()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_ICONOVRLS, IDD_CFGPAGE_ICONOVRLS, PSP_USETITLE, NULL)
{
    HListView = NULL;
}

void CCfgPageIconOvrls::Transfer(CTransferInfo& ti)
{
    BOOL oldEnableCustomIconOverlays = Configuration.EnableCustomIconOverlays;
    ti.CheckBox(IDC_ICONOVRLS_ENABLE, Configuration.EnableCustomIconOverlays);
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < ListOfShellIconOverlays.Count; i++)
        {
            CShellIconOverlayItem2* item = ListOfShellIconOverlays[i];
            LVITEM lvi;
            lvi.mask = LVIF_TEXT;
            lvi.iItem = i;
            lvi.iSubItem = 0;
            lvi.pszText = item->IconOverlayName;
            ListView_InsertItem(HListView, &lvi);

            ListView_SetItemText(HListView, i, 1, item->IconOverlayDescr);

            UINT state = INDEXTOSTATEIMAGEMASK((!IsNameInListOfDisabledCustomIconOverlays(item->IconOverlayName) ? 2 : 1));
            ListView_SetItemState(HListView, i, state, LVIS_STATEIMAGEMASK);
        }
        // set column widths
        ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE_USEHEADER);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);

        DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
        ListView_SetItemState(HListView, 0, state, state);
        ListView_EnsureVisible(HListView, 0, FALSE);
        EnableControls();
    }
    else
    {
        char* oldDisabledCustomIconOverlays = Configuration.DisabledCustomIconOverlays;
        Configuration.DisabledCustomIconOverlays = NULL;
        int i;
        for (i = 0; i < ListOfShellIconOverlays.Count; i++)
        {
            if (ListView_GetItemState(HListView, i, LVIS_STATEIMAGEMASK) == INDEXTOSTATEIMAGEMASK(1))
            { // disabled checkbox -> add name to the list of disabled icon overlay handlers
                AddToListOfDisabledCustomIconOverlays(ListOfShellIconOverlays[i]->IconOverlayName);
            }
        }
        if (oldEnableCustomIconOverlays != Configuration.EnableCustomIconOverlays ||
            strcmp(oldDisabledCustomIconOverlays != NULL ? oldDisabledCustomIconOverlays : "",
                   Configuration.DisabledCustomIconOverlays != NULL ? Configuration.DisabledCustomIconOverlays : "") != 0)
        { // config change -> we need to report that it will only take effect on the next launch of Salama
            SalMessageBox(HWindow, LoadStr(IDS_ICONOVRLS_CHANGE), LoadStr(IDS_INFOTITLE),
                          MB_OK | MB_ICONINFORMATION);
        }
        if (oldDisabledCustomIconOverlays != NULL)
            free(oldDisabledCustomIconOverlays);
    }
}

void CCfgPageIconOvrls::EnableControls()
{
    BOOL enable = IsDlgButtonChecked(HWindow, IDC_ICONOVRLS_ENABLE) == BST_CHECKED;
    if ((IsWindowEnabled(HListView) != 0) != enable)
        EnableWindow(HListView, enable);
}

INT_PTR
CCfgPageIconOvrls::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageIconOvrls::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HListView = GetDlgItem(HWindow, IDC_ICONOVRLS_LIST);
        new CToolbarHeader(HWindow, IDC_ICONOVRLS_HEADER, HListView, 0);

        DWORD exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // populate the columns Name and Description in the listview
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_FMT;
        lvc.pszText = LoadStr(IDS_ICONOVRLS_NAME);
        lvc.fmt = LVCFMT_LEFT;
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.mask |= LVCF_SUBITEM;
        lvc.pszText = LoadStr(IDS_ICONOVRLS_DESCR);
        lvc.iSubItem = 1;
        ListView_InsertColumn(HListView, 1, &lvc);

        // Dialog elements should stretch according to its size, let's set the dividing controls
        ElasticVerticalLayout(1, IDC_ICONOVRLS_LIST);

        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        ListView_SetBkColor(HListView, GetSysColor(COLOR_WINDOW));
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_ICONOVRLS_ENABLE)
            EnableControls();
        break;
    }
    }

    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
