// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "translator.h"
#include "wndprev.h"
#include "wndframe.h"
#include "wndtext.h"
#include "config.h"

const char* PREVIEWWINDOW_NAME = "Preview";

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

//*****************************************************************************
//
// CPreviewWindow
//

CPreviewWindow::CPreviewWindow()
    : CWindow(ooStatic)
{
    DefWndProc = DefMDIChildProc;

    CurrentDialog = NULL;
    CurrentDialogIndex = -1;
    HDialog = NULL;
    HighlightedControlID = 0;
    HFont = NULL;
    HMenuFont = NULL;
    MenuPreview = NULL;
}

CPreviewWindow::~CPreviewWindow()
{
    if (MenuPreview != NULL)
    {
        delete MenuPreview;
        MenuPreview = NULL;
    }
}

void CPreviewWindow::SetInvalidPreview()
{
    SetWindowText(HWindow, "Preview is not available for this item");
    CloseCurrentDialog();
    InvalidateRect(HWindow, NULL, TRUE);
}

void BufferKey(WORD vk, BOOL bExtended, BOOL down, BOOL up)
{
    INPUT input;
    if (down)
    {
        // generate down
        ::ZeroMemory(&input, sizeof(input));
        input.type = INPUT_KEYBOARD;
        if (bExtended)
            input.ki.dwFlags = KEYEVENTF_EXTENDEDKEY;
        input.ki.wVk = vk;
        ::SendInput(1, &input, sizeof(input));
    }
    if (up)
    {
        // generate up
        ::ZeroMemory(&input, sizeof(input));
        input.type = INPUT_KEYBOARD;
        input.ki.dwFlags = KEYEVENTF_KEYUP;
        if (bExtended)
            input.ki.dwFlags |= KEYEVENTF_EXTENDEDKEY;
        input.ki.wVk = vk;
        ::SendInput(1, &input, sizeof(input));
    }
}

CPreviewWindow* CurrentPreviewWindow = NULL;

/*
const char *DEFBTN_SUBCLASSPROC = "DefBtnSubClass";

LRESULT CALLBACK
DefButtonSubclassProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  WNDPROC oldWndProc = (WNDPROC)GetProp(hwnd, DEFBTN_SUBCLASSPROC);
  if (oldWndProc == NULL)
  {
    TRACE_E("DefButtonSubclassProc: OldWndProc == NULL");
    return 0;
  }
  switch (message)
  {
    case WM_PAINT:
    {
      PostMessage(GetParent(hwnd), WM_POSTPAINT, 0, 0);
      break;
    }
  }
  return CallWindowProc(oldWndProc, hwnd, message, wParam, lParam);
}
*/

BOOL CALLBACK
CPreviewWindow::PreviewDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        /*
      // HACK HACK: pokus o prebiti Aero animace na DEF tlacitku, ktera nam nici outline ramecek kolem nej
      // EDIT: zruseno, poblikava to
      DWORD defID = (DWORD)SendMessage(hwndDlg, DM_GETDEFID, 0, 0); 
      HWND hDefBtn = GetDlgItem(hwndDlg, LOWORD(defID));
      if (hDefBtn != NULL)
      {
        WNDPROC oldWndProc = (WNDPROC)GetWindowLongPtr(hDefBtn, GWLP_WNDPROC);
        if (SetProp(hDefBtn, DEFBTN_SUBCLASSPROC, (HANDLE)oldWndProc))
          SetWindowLongPtr(hDefBtn, GWLP_WNDPROC, (LONG_PTR)DefButtonSubclassProc);
      }
*/
        if (Config.PreviewFillControls)
            FillControlsWithDummyValues(hwndDlg);
        break;
    }

    case WM_PAINT:
    {
        /*
      PAINTSTRUCT ps;
      HDC hDC = BeginPaint(hwndDlg, &ps);
      MoveToEx(hDC, 0, 0, NULL);
      LineTo(hDC, 100, 100);
      EndPaint(hwndDlg, &ps);
      */
        PostMessage(hwndDlg, WM_POSTPAINT, 0, 0);
        break;
    }

    case WM_POSTPAINT:
    {
        // child windows jeste nejsou prekreslena - donutime je
        UpdateWindow(hwndDlg);

        if (Config.PreviewOutlineControls)
        {
            CDialogData* dialogData = CurrentPreviewWindow->CurrentDialog;
            HDC hDC = GetDC(GetParent(hwndDlg));
            dialogData->OutlineControls(hwndDlg, hDC);
            ReleaseDC(GetParent(hwndDlg), hDC);
        }

        // nakreslime obdelnik kolem aktivniho child window (existuje-li)
        HWND hChild = NULL;
        if (PreviewWindow.HighlightedControlID != 0)
            hChild = GetDlgItem(hwndDlg, PreviewWindow.HighlightedControlID);

        if (hChild != NULL)
        {
            HWND parent = GetParent(hwndDlg); // musim kreslit do parenta, pri kresleni do hwndDlg je ramecek prerusovany controly (DC dialogu je tam asi oclipovane, ci co)
            RECT r;
            GetWindowRect(hChild, &r);
            POINT p1;
            p1.x = r.left;
            p1.y = r.top;
            ScreenToClient(parent, &p1);
            POINT p2;
            p2.x = r.right;
            p2.y = r.bottom;
            ScreenToClient(parent, &p2);
            r.left = p1.x;
            r.top = p1.y;
            r.right = p2.x;
            r.bottom = p2.y;

            HDC hDC = GetDC(parent);
            HPEN hPen = CreatePen(PS_DOT, 3, GetSelectionColor(FALSE));

            HBRUSH hOldBrush = (HBRUSH)SelectObject(hDC, (HBRUSH)GetStockObject(NULL_BRUSH));
            HPEN hOldPen = (HPEN)SelectObject(hDC, hPen);
            Rectangle(hDC, r.left - 2, r.top - 2, r.right + 2, r.bottom + 2);
            // InvertRect(hDC, &r);
            SelectObject(hDC, hOldBrush);
            SelectObject(hDC, hOldPen);
            DeleteObject(hPen);
            ReleaseDC(parent, hDC);
        }

        return 0;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        case IDCANCEL:
        {
            DestroyWindow(hwndDlg);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE;
}

void CPreviewWindow::RefreshDialog()
{
    PreviewDialog(CurrentDialogIndex);
}

void CPreviewWindow::PreviewDialog(int index)
{
    if (index == -1)
    {
        SetInvalidPreview();
        return;
    }

    if (MenuPreview != NULL)
    {
        delete MenuPreview;
        MenuPreview = NULL;
    }

    CDialogData* data = Data.DlgData[index];
    WORD buff[200000];
    data->PrepareTemplate(buff, FALSE, TRUE, Config.PreviewExtendDialogs);

    // dialog umistime do pocatku okna a shodime mu napriklad DS_CENTER, aby se nepokousel centrovat
    DWORD addStyles = WS_CHILDWINDOW | WS_DISABLED | WS_OVERLAPPED | DS_MODALFRAME;
    DWORD removeStyles = WS_VISIBLE | WS_POPUP | DS_CENTER | DS_SETFOREGROUND;
    data->TemplateAddRemoveStyles(buff, addStyles, removeStyles);
    data->TemplateSetPos(buff, 1, 1);

    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;

    if (!altPressed && !shiftPressed && !controlPressed)
        BufferKey(VK_CONTROL); // donutime Windows zobrazit horke klavesy; pokud je Ctrl stisknuty, nesmime ho rozhodit; navic jsou pak podtriztka zobrazeny tak jako tak

    CurrentPreviewWindow = this;
    HWND hDlg = CreateDialogIndirectW(HInstance, (LPDLGTEMPLATE)buff, HWindow, PreviewDialogProcW);
    if (hDlg == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("CreateDialogIndirect() failed, error=" << err);
        SetInvalidPreview();
        return;
    }

    CloseCurrentDialog();

    ShowWindow(hDlg, SW_SHOWNA);
    HDialog = hDlg;

    CurrentDialog = data;
    CurrentDialogIndex = index;
    InvalidateRect(HWindow, NULL, TRUE);
}

BOOL CALLBACK InvalidateAllEnum(HWND hwnd, LPARAM lParam)
{
    /*  // Petr: odstavil jsem invalidovani jen postizenych controlu, protoze to delalo neplechu u Renameru, kde je velky editbox prekryvajici spoustu controlu, takze pri pohybu po controlech se pak ty vyoptimalizovane neukazaly (prekryl je editbox)
  RECT r;
  GetWindowRect(hwnd, &r);
  RECT dummy;
  if (IntersectRect(&dummy, (RECT *)lParam, &r))
    InvalidateRect(hwnd, NULL, TRUE);
*/
    InvalidateRect(hwnd, NULL, TRUE);
    return TRUE;
}

void InvalidateChild(HWND hDialog, int childID)
{
    HWND hChild = GetDlgItem(hDialog, childID);
    if (hChild == NULL)
        return;

    RECT r;
    GetWindowRect(hChild, &r);
    //  RECT scrR = r;
    RECT parentR = r;
    POINT p1;
    p1.x = r.left;
    p1.y = r.top;
    ScreenToClient(hDialog, &p1);
    POINT p2;
    p2.x = r.right;
    p2.y = r.bottom;
    ScreenToClient(hDialog, &p2);
    r.left = p1.x;
    r.top = p1.y;
    r.right = p2.x;
    r.bottom = p2.y;

    r.left -= 5;
    r.top -= 5;
    r.right += 5;
    r.bottom += 5;
    InvalidateRect(hDialog, &r, TRUE);

    HWND parent = GetParent(hDialog);
    p1.x = parentR.left;
    p1.y = parentR.top;
    ScreenToClient(parent, &p1);
    p2.x = parentR.right;
    p2.y = parentR.bottom;
    ScreenToClient(parent, &p2);
    parentR.left = p1.x;
    parentR.top = p1.y;
    parentR.right = p2.x;
    parentR.bottom = p2.y;

    parentR.left -= 5;
    parentR.top -= 5;
    parentR.right += 5;
    parentR.bottom += 5;
    InvalidateRect(parent, &parentR, TRUE);

    /*
  // jeste invalidatnu vsechny controly v oblastni, kterou chceme prekreslit,
  // protoze mi tu zustavaly vnitrky kontrolu neprekresleny,
  // viz Checksum, dialog 1000, control 1005/1007/1006 (jsou pres sebe)
  scrR.left -= 5;
  scrR.top -= 5;
  scrR.right += 5;
  scrR.bottom += 5;
  EnumChildWindows(hDialog, InvalidateEnum, (LPARAM)&scrR);
*/
    // jeste invalidatnu vsechny controly v dialogu, protoze mi tu zustavaly
    // vnitrky kontrolu neprekresleny, viz Checksum, dialog 1000, control
    // 1005/1007/1006 (jsou pres sebe)
    EnumChildWindows(hDialog, InvalidateAllEnum, 0);
}

void CPreviewWindow::HighlightControl(WORD id)
{
    if (HDialog == NULL)
        return;

    HWND hChild = NULL;
    if (id != 0)
    {
        hChild = GetDlgItem(HDialog, id);
        if (hChild == NULL)
        {
            TRACE_E("Unknown ID:" << id);
            return;
        }
    }

    if (id == HighlightedControlID)
        return;

    if (HighlightedControlID != 0)
        InvalidateChild(HDialog, HighlightedControlID);

    HighlightedControlID = id;

    if (HighlightedControlID != NULL)
        InvalidateChild(HDialog, HighlightedControlID);

    UpdateWindow(HDialog);

    DisplayControlInfo();
}

void CPreviewWindow::CloseCurrentDialog()
{
    if (HDialog == NULL)
        return;

    CurrentDialog = NULL;
    CurrentDialogIndex = -1;
    DestroyWindow(HDialog);
    HDialog = NULL;
}

HWND GetChildWindow(POINT p, HWND hDialog, int* index)
{
    // prohledame vsechny childy a vyberem nejmensi, ktery lezi pod bodem
    if (hDialog == NULL)
        return NULL;

    int i = 1;
    HWND hFirst = GetWindow(hDialog, GW_CHILD);
    HWND hIter = hFirst;
    HWND hChild = NULL;
    int childSurface = -1;
    do
    {
        RECT r;
        GetWindowRect(hIter, &r);
        if (PtInRect(&r, p))
        {
            int surface = (r.right - r.left) * (r.bottom - r.top);
            if (childSurface == -1 || surface < childSurface)
            {
                hChild = hIter;
                childSurface = surface;
                if (index != NULL)
                    *index = i;
            }
        }
        hIter = GetNextWindow(hIter, GW_HWNDNEXT);
        i++;
    } while (hIter != hFirst && hIter != NULL);

    return hChild;
}

void CPreviewWindow::DisplayControlInfo()
{
    int yOffset = 0;
    if (HDialog != NULL && CurrentDialog != NULL)
    {
        HDC hDC = HANDLES(GetDC(HWindow));
        SelectObject(hDC, HFont);

        RECT dlgR;
        GetWindowRect(HDialog, &dlgR);
        yOffset = dlgR.bottom - dlgR.top + 10;

        RECT r;
        GetClientRect(HWindow, &r);
        r.top = yOffset + 1;

        wchar_t info[10000];
        info[0] = 0;

        CDialogData* dlg = CurrentDialog;
        CControl* control = NULL;
        for (int i = 0; i < dlg->Controls.Count; i++)
        {
            if (dlg->Controls[i]->ID == HighlightedControlID)
            {
                control = dlg->Controls[i];
                break;
            }
        }

        wchar_t* p = info;
        if (control != NULL)
        {
            p += swprintf_s(p, _countof(info) - (p - info), L" CONTROL INFO\n");
            p += swprintf_s(p, _countof(info) - (p - info), L" ID:%d\n", control->ID);
            if (control->TX == control->OX && control->TY == control->OY && control->TCX == control->OCX && control->TCY == control->OCY)
            {
                p += swprintf_s(p, _countof(info) - (p - info), L" L:%d T:%d R:%d B:%d W:%d H:%d\n", control->TX, control->TY,
                                control->TX + control->TCX, control->TY + control->TCY, control->TCX, control->TCY);
            }
            else
            {
                p += swprintf_s(p, _countof(info) - (p - info), L" TL:%d TT:%d TR:%d TB:%d TW:%d TH:%d\n", control->TX, control->TY,
                                control->TX + control->TCX, control->TY + control->TCY, control->TCX, control->TCY);
                p += swprintf_s(p, _countof(info) - (p - info), L" OL:%d OT:%d OR:%d OB:%d OW:%d OH:%d\n", control->OX, control->OY,
                                control->OX + control->OCX, control->OY + control->OCY, control->OCX, control->OCY);
            }
            p += swprintf_s(p, _countof(info) - (p - info), L" Style:0x%08X ExStyle:0x%08X\n", control->Style, control->ExStyle);
            if (control->ClassName != NULL && LOWORD(control->ClassName) != 0xFFFF)
                p += swprintf_s(p, _countof(info) - (p - info), L" Class:%s\n", control->ClassName);
            else
                p += swprintf_s(p, _countof(info) - (p - info), L" Class:0x%08X\n", (DWORD)control->ClassName);
        }
        else
        {
            p += swprintf_s(p, _countof(info) - (p - info), L" DIALOG INFO\n");
            p += swprintf_s(p, _countof(info) - (p - info), L" ID:%d\n", dlg->ID);
            if (dlg->TX == dlg->OX && dlg->TY == dlg->OY && dlg->TCX == dlg->OCX && dlg->TCY == dlg->OCY)
            {
                p += swprintf_s(p, _countof(info) - (p - info), L" X:%d Y:%d W:%d H:%d\n", dlg->TX, dlg->TY, dlg->TCX, dlg->TCY);
            }
            else
            {
                p += swprintf_s(p, _countof(info) - (p - info), L" TX:%d TY:%d TW:%d TH:%d\n", dlg->TX, dlg->TY, dlg->TCX, dlg->TCY);
                p += swprintf_s(p, _countof(info) - (p - info), L" OX:%d OY:%d OW:%d OH:%d\n", dlg->OX, dlg->OY, dlg->OCX, dlg->OCY);
            }
            p += swprintf_s(p, _countof(info) - (p - info), L" Style:0x%08X ExStyle:0x%08X\n", dlg->Style, dlg->ExStyle);
        }

        FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));
        DrawTextW(hDC, info, -1, &r, DT_LEFT);

        HANDLES(ReleaseDC(HWindow, hDC));
    }
}

void CPreviewWindow::DisplayMenuPreview()
{
    if (MenuPreview == NULL)
        return;

    HDC hDC = HANDLES(GetDC(HWindow));
    SelectObject(hDC, (HFONT)HMenuFont);

    RECT r;
    GetClientRect(HWindow, &r);

    RECT tr = r;
    tr.top = 0;
    tr.bottom = MenuFontHeight;
    for (int i = 0; i < MenuPreview->Texts.Count; i++)
    {
        DrawTextW(hDC, MenuPreview->Texts[i], -1, &tr, DT_LEFT /*| DT_NOPREFIX*/);
        tr.top += MenuFontHeight;
        tr.bottom += MenuFontHeight;
    }
    HANDLES(ReleaseDC(HWindow, hDC));
}

void CPreviewWindow::PreviewMenu(int index)
{
    if (index == -1)
    {
        if (MenuPreview != NULL)
        {
            delete MenuPreview;
            MenuPreview = NULL;
            InvalidateRect(HWindow, NULL, TRUE);
        }
        return;
    }
    if (index < 0 || index >= Data.MenuPreview.Count)
    {
        TRACE_E("Invalid index!");
        return;
    }
    CMenuPreview* menuPreview = Data.MenuPreview[index];
    if (MenuPreview != NULL)
        delete MenuPreview;
    MenuPreview = new CMenuPreview();
    MenuPreview->LoadFrom(menuPreview);
    InvalidateRect(HWindow, NULL, TRUE);
}

LRESULT
CPreviewWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        HMENU hMenu = GetSystemMenu(HWindow, FALSE);
        if (hMenu != NULL)
            EnableMenuItem(hMenu, SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        LOGFONT lf;
        GetFixedLogFont(&lf);
        HFont = HANDLES(CreateFontIndirect(&lf));

        NONCLIENTMETRICS ncm;
        ncm.cbSize = sizeof(ncm);
        SystemParametersInfo(SPI_GETNONCLIENTMETRICS, ncm.cbSize, &ncm, 0);
        LOGFONT* lff = &ncm.lfMenuFont;
        HMenuFont = HANDLES(CreateFontIndirect(lff));
        MenuFontHeight = (abs(lff->lfHeight) * 3) / 2;

        break;
    }

    case WM_SIZE:
    {
        /*
      if (HTree != NULL)
      {
        RECT r;
        GetClientRect(HWindow, &r);
        SetWindowPos(HTree, NULL, 0, 0, r.right, r.bottom, SWP_NOZORDER);
      }
*/
        break;
    }

    case WM_ERASEBKGND:
    {
        RECT r;
        GetClientRect(HWindow, &r);
        FillRect((HDC)wParam, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
        if (MenuPreview == NULL)
            DisplayControlInfo();
        else
            DisplayMenuPreview();
        return TRUE;
    }

    case WM_CLOSE:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, CM_CLOSE, 0); // bezpecny close-window
        return 0;
    }

    case WM_DESTROY:
    {
        CloseCurrentDialog();
        if (HFont != NULL)
        {
            HANDLES(DeleteObject(HFont));
            HFont = NULL;
        }
        if (HMenuFont != NULL)
        {
            HANDLES(DeleteObject(HMenuFont));
            HMenuFont = NULL;
        }
        return 0;
    }

    case WM_LBUTTONDBLCLK:
    {
        PostMessage(FrameWindow.HWindow, WM_COMMAND, ID_TOOLS_EDITLAYOUT, 0);
        break;
    }

    case WM_LBUTTONDOWN:
    {
        if (HDialog != NULL)
        {
            DWORD pos = GetMessagePos();
            POINT pt;
            pt.x = GET_X_LPARAM(pos);
            pt.y = GET_Y_LPARAM(pos);

            DWORD ncHitTest = SendMessageW(HDialog, WM_NCHITTEST, 0, MAKELPARAM(pt.x, pt.y));
            if (ncHitTest == HTCAPTION)
            {
                TextWindow.SelectIndex(0);
                return 0;
            }

            HWND hChild = GetChildWindow(pt, HDialog);
            if (hChild != NULL)
            {
                TextWindow.SelectItem((WORD)(UINT_PTR)GetMenu(hChild));
                return 0;
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CPreviewWindow PreviewWindow;
