// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <windows.h>
#include <crtdbg.h>
#include <tchar.h>
#include <ostream>
#include <uxtheme.h>

#if defined(_DEBUG) && defined(_MSC_VER) // without passing file+line to 'new' operator, list of memory leaks shows only 'crtdbg.h(552)'
#define new new (_NORMAL_BLOCK, __FILE__, __LINE__)
#endif

#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression

#include "trace.h"
#include "handles.h"
#include "array.h"
#include "winlib.h"
#include "multimon.h"
#include "sheets.h"

extern CWinLibHelp* WinLibHelp;

//
// ****************************************************************************
// CElasticLayout
//

CElasticLayout::CElasticLayout(HWND hWindow)
    : ResizeCtrls(2, 2), MoveCtrls(20, 20)
{
    HWindow = hWindow;
    SplitY = 0;
}

void CElasticLayout::AddResizeCtrl(int resID)
{
    HWND hChild = GetDlgItem(HWindow, resID);
    if (hChild != NULL)
    {
        RECT r;
        GetWindowRect(hChild, &r);

        // if the bottom edge of the element is greater than SplitY, we move the SplitY boundary
        POINT p = {r.right, r.bottom};
        ScreenToClient(HWindow, &p);
        if (p.y > SplitY)
            SplitY = p.y;

        CElasticLayoutCtrl ctrl;
        ctrl.HCtrl = hChild;
        ctrl.Pos.x = r.right - r.left;
        ctrl.Pos.y = 0;
        ResizeCtrls.Add(ctrl);
    }
    else
    {
        TRACE_E("CElasticLayout::AddResizeCtrl() Unknown control: resID=" << resID);
    }
}

BOOL CALLBACK
CElasticLayout::FindMoveControls(HWND hChild, LPARAM lParam)
{
    CElasticLayout* el = (CElasticLayout*)lParam;

    // if the element lies below SplitY, we add it to the list of elements that will be moved
    RECT r;
    GetWindowRect(hChild, &r);
    POINT p = {r.left, r.top};
    ScreenToClient(el->HWindow, &p);
    if (p.y >= el->SplitY)
    {
        CElasticLayoutCtrl mc;
        mc.HCtrl = hChild;
        mc.Pos = p;
        el->MoveCtrls.Add(mc);
    }

    return TRUE;
}

void CElasticLayout::FindMoveCtrls()
{
    EnumChildWindows(HWindow, FindMoveControls, (LPARAM)this);

    // find the envelope of all 'move' elements
    RECT rEnvelope = {0};
    for (int i = 0; i < MoveCtrls.Count; i++)
    {
        HWND hCtrl = MoveCtrls[i].HCtrl;

        RECT r;
        GetWindowRect(hCtrl, &r);
        if (r.left < rEnvelope.left)
            rEnvelope.left = r.left;
        if (r.top < rEnvelope.top)
            rEnvelope.top = r.top;
        if (r.right > rEnvelope.right)
            rEnvelope.right = r.right;
        if (r.bottom > rEnvelope.bottom)
            rEnvelope.bottom = r.bottom;
    }
    POINT p = {rEnvelope.right, rEnvelope.bottom};
    ScreenToClient(HWindow, &p);
    int envelopeBottom = p.y;
    // Coordinates 'MoveCtrlsY' will be relative to the bottom edge of the envelope
    for (int i = 0; i < MoveCtrls.Count; i++)
        MoveCtrls[i].Pos.y = envelopeBottom - MoveCtrls[i].Pos.y;

    // for elements of ResizeCtrls we save their distance from the bottom edge to the bottom edge of the envelope
    for (int i = 0; i < ResizeCtrls.Count; i++)
    {
        if (ResizeCtrls[i].Pos.y == 0)
        {
            RECT r;
            GetWindowRect(ResizeCtrls[i].HCtrl, &r);
            ResizeCtrls[i].Pos.y = max(0, rEnvelope.bottom - r.bottom);
        }
    }
}

void CElasticLayout::LayoutCtrls()
{
    if (ResizeCtrls.Count == 0)
    {
        TRACE_E("No controls to layout!");
        return;
    }
    RECT cR;
    GetClientRect(HWindow, &cR);

    FindMoveCtrls();

    HDWP hdwp = HANDLES(BeginDeferWindowPos(ResizeCtrls.Count + MoveCtrls.Count));
    if (hdwp != NULL)
    {
        for (int i = 0; i < ResizeCtrls.Count; i++)
        {
            HWND hCtrl = ResizeCtrls[i].HCtrl;
            RECT r;
            GetWindowRect(hCtrl, &r);
            POINT p = {r.left, r.top};
            ScreenToClient(HWindow, &p);
            HANDLES(DeferWindowPos(hdwp, hCtrl, NULL,
                                   0, 0,
                                   ResizeCtrls[i].Pos.x, cR.bottom - p.y - ResizeCtrls[i].Pos.y,
                                   SWP_NOMOVE | SWP_NOZORDER));
        }
        for (int i = 0; i < MoveCtrls.Count; i++)
        {
            HWND hCtrl = MoveCtrls[i].HCtrl;
            HANDLES(DeferWindowPos(hdwp, hCtrl, NULL,
                                   MoveCtrls[i].Pos.x, cR.bottom - MoveCtrls[i].Pos.y,
                                   0, 0,
                                   SWP_NOSIZE | SWP_NOZORDER));
        }
        HANDLES(EndDeferWindowPos(hdwp));
    }
    MoveCtrls.DestroyMembers();
}

//
// ****************************************************************************
// CPropSheetPage
//

CPropSheetPage::CPropSheetPage(const TCHAR* title, HINSTANCE modul, int resID,
                               DWORD flags, HICON icon, CObjectOrigin origin)
    : CDialog(modul, resID, NULL, origin)
{
    Init(title, modul, resID, icon, flags, origin);
}

CPropSheetPage::CPropSheetPage(const TCHAR* title, HINSTANCE modul, int resID, UINT helpID,
                               DWORD flags, HICON icon, CObjectOrigin origin)
    : CDialog(modul, resID, helpID, NULL, origin)
{
    Init(title, modul, resID, icon, flags, origin);
}

void CPropSheetPage::Init(const TCHAR* title, HINSTANCE modul, int resID,
                          HICON icon, DWORD flags, CObjectOrigin origin)
{
    Title = NULL;
    if (title != NULL)
    {
        int len = (int)_tcslen(title);
        Title = new TCHAR[len + 1];
        if (Title != NULL)
            _tcscpy_s(Title, len + 1, title);
        else
            TRACE_ET(_T("Low memory!"));
    }
    Flags = flags;
    Icon = icon;

    ParentDialog = NULL; // is set from CPropertyDialog::Execute()
    ParentPage = NULL;
    HTreeItem = NULL;
    Expanded = NULL;
    ElasticLayout = NULL;
}

CPropSheetPage::~CPropSheetPage()
{
    if (Title != NULL)
        delete[] Title;
    if (ElasticLayout != NULL)
        delete ElasticLayout;
}

BOOL CPropSheetPage::ValidateData()
{
    CTransferInfo ti(HWindow, ttDataFromWindow);
    Validate(ti);
    if (!ti.IsGood())
    {
        if (PropSheet_GetCurrentPageHwnd(Parent) != HWindow)
            PropSheet_SetCurSel(Parent, HWindow, 0);

        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

BOOL CPropSheetPage::TransferData(CTransferType type)
{
    CTransferInfo ti(HWindow, type);
    Transfer(ti);
    if (!ti.IsGood())
    {
        if (ti.Type == ttDataFromWindow &&
            PropSheet_GetCurrentPageHwnd(Parent) != HWindow)
            PropSheet_SetCurSel(Parent, HWindow, 0);

        ti.EnsureControlIsFocused(ti.FailCtrlID);
        return FALSE;
    }
    else
        return TRUE;
}

HPROPSHEETPAGE
CPropSheetPage::CreatePropSheetPage()
{
    PROPSHEETPAGE psp;
    psp.dwSize = sizeof(PROPSHEETPAGE);
    psp.dwFlags = Flags;
    psp.hInstance = Modul;
    psp.pszTemplate = MAKEINTRESOURCE(ResID);
    psp.hIcon = Icon;
    psp.pszTitle = Title;
    psp.pfnDlgProc = CPropSheetPage::CPropSheetPageProc;
    psp.lParam = (LPARAM)this;
    psp.pfnCallback = NULL;
    psp.pcRefParent = NULL;
    return CreatePropertySheetPage(&psp);
}

BOOL CPropSheetPage::ElasticVerticalLayout(int count, ...)
{
    if (ElasticLayout != NULL)
    {
        TRACE_E("ElasticLayout already set!");
        return FALSE;
    }
    ElasticLayout = new CElasticLayout(HWindow);
    va_list list;
    va_start(list, count);
    for (int arg = 0; arg < count; arg++)
        ElasticLayout->AddResizeCtrl(va_arg(list, int));
    va_end(list);
    return TRUE;
}

INT_PTR
CPropSheetPage::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        ParentDialog->HWindow = Parent;
        TransferData(ttDataToWindow);
        if (ElasticLayout != NULL)
            ElasticLayout->LayoutCtrls();
        return TRUE; // I want focus from DefDlgProc
    }

    case WM_SIZE:
    {
        if (ElasticLayout != NULL)
            ElasticLayout->LayoutCtrls();
        break;
    }

    case WM_HELP:
    {
        if (WinLibHelp != NULL && HelpID != -1)
        {
            WinLibHelp->OnHelp(HWindow, HelpID, (HELPINFO*)lParam,
                               (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                               (GetKeyState(VK_SHIFT) & 0x8000) != 0);
            return TRUE;
        }
        break; // Let F1 fall through to the parent
    }

    case WM_CONTEXTMENU:
    {
        if (WinLibHelp != NULL)
            WinLibHelp->OnContextMenu((HWND)wParam, LOWORD(lParam), HIWORD(lParam));
        return TRUE;
    }

    case WM_NOTIFY:
    {
        if (((NMHDR*)lParam)->code == PSN_KILLACTIVE) // deactivate page
        {
            if (ValidateData())
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            else // do not allow deactivation of the page
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_SETACTIVE) // page activation
        {
            if (ParentDialog != NULL && ParentDialog->LastPage != NULL)
            { // remembering the last page
                *ParentDialog->LastPage = ParentDialog->GetCurSel();
            }
            break;
        }

        if (((NMHDR*)lParam)->code == PSN_APPLY)
        { // button ApplyNow or OK pressed
            if (TransferData(ttDataFromWindow))
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_NOERROR);
            else
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, PSNRET_INVALID_NOCHANGEPAGE);
            return TRUE;
        }

        if (((NMHDR*)lParam)->code == PSN_WIZFINISH)
        { // Button Finish pressed
            // PSN_KILLACTIVE did not arrive - I will perform validation
            if (!ValidateData())
            {
                SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                return TRUE;
            }

            // Loop through all pages for transfer
            for (int i = 0; i < ParentDialog->Count; i++)
            {
                if (ParentDialog->At(i)->HWindow != NULL)
                {
                    if (!ParentDialog->At(i)->TransferData(ttDataFromWindow))
                    {
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, TRUE);
                        return TRUE;
                    }
                }
            }
            SetWindowLongPtr(HWindow, DWLP_MSGRESULT, FALSE);
            return TRUE;
        }
        break;
    }
    }
    return FALSE;
}

INT_PTR CALLBACK
CPropSheetPage::CPropSheetPageProc(HWND hwndDlg, UINT uMsg, WPARAM wParam,
                                   LPARAM lParam)
{
    CPropSheetPage* dlg;
    switch (uMsg)
    {
    case WM_INITDIALOG: // first message - connecting an object to a dialog
    {
        dlg = (CPropSheetPage*)((PROPSHEETPAGE*)lParam)->lParam;
        if (dlg == NULL)
        {
            TRACE_ET(_T("Unable to create dialog."));
            return TRUE;
        }
        else
        {
            dlg->HWindow = hwndDlg;
            dlg->Parent = ::GetParent(hwndDlg);
            //--- insert window with hwndDlg into the list of windows
            if (!WindowsManager.AddWindow(hwndDlg, dlg)) // error
            {
                TRACE_ET(_T("Unable to create dialog."));
                return TRUE;
            }
            dlg->NotifDlgJustCreated(); // Set as a place for editing the dialog layout
        }
        break;
    }

    case WM_DESTROY: // last message - disconnecting object from dialog
    {
        dlg = (CPropSheetPage*)WindowsManager.GetWindowPtr(hwndDlg);
        INT_PTR ret = FALSE; // in case it is not processed
        if (dlg != NULL && dlg->Is(otDialog))
        {
            // Petr: I moved it down below wnd->WindowProc(), so that during WM_DESTROY
            //       there were still messages (Lukas needed)
            // WindowsManager.DetachWindow(hwndDlg);

            ret = dlg->DialogProc(uMsg, wParam, lParam);

            WindowsManager.DetachWindow(hwndDlg);
            if (dlg->IsAllocated())
                delete dlg;
            else
                dlg->HWindow = NULL; // information about disconnection
        }
        return ret;
    }

    default:
    {
        dlg = (CPropSheetPage*)WindowsManager.GetWindowPtr(hwndDlg);
#ifdef __DEBUG_WINLIB
        if (dlg != NULL && !dlg->Is(otPropSheetPage))
        {
            TRACE_CT(_T("This should never happen."));
            dlg = NULL;
        }
#endif
    }
    }
    //--- calling the DialogProc(...) method of the corresponding dialog object
    if (dlg != NULL)
        return dlg->DialogProc(uMsg, wParam, lParam);
    else
        return FALSE; // error or message did not arrive between WM_INITDIALOG and WM_DESTROY
}

//
// ****************************************************************************
// CPropertyDialog
//

INT_PTR
CPropertyDialog::Execute()
{
    if (Count > 0)
    {
        PROPSHEETHEADER psh;
        psh.dwSize = sizeof(PROPSHEETHEADER);
        psh.dwFlags = Flags;
        psh.hwndParent = Parent;
        psh.hInstance = Modul;
        psh.hIcon = Icon;
        psh.pszCaption = Caption;
        psh.nPages = Count;
        if (StartPage < 0 || StartPage >= Count)
            StartPage = 0;
        psh.nStartPage = StartPage;
        HPROPSHEETPAGE* pages = new HPROPSHEETPAGE[Count];
        if (pages == NULL)
        {
            TRACE_ET(_T("Low memory!"));
            return -1;
        }
        psh.phpage = pages;
        for (int i = 0; i < Count; i++)
        {
            psh.phpage[i] = At(i)->CreatePropSheetPage();
            At(i)->ParentDialog = this;
        }
        psh.pfnCallback = Callback;
        INT_PTR ret = PropertySheet(&psh);
        delete pages;
        return ret;
    }
    else
    {
        TRACE_ET(_T("Incorrect call to CPropertyDialog::Execute."));
        return -1;
    }
}

int CPropertyDialog::GetCurSel()
{
    HWND tabCtrl = PropSheet_GetTabControl(HWindow);
    return TabCtrl_GetCurSel(tabCtrl);
}

//
// ****************************************************************************
// CTreePropDialog
//

#define _TPD_IDC_TREE 1
#define _TPD_IDC_HELP 9
#define _TPD_IDC_GRIP 10
#define _TPD_IDC_SEP 11
#define _TPD_IDC_CAPTION 3
#define _TPD_IDC_RECT 4
#define _TPD_IDC_OK 5
// dimensions in dialog units
#define _TPD_LEFTMARGIN 4  // Indentation of TreeView and Caption from the left edge
#define _TPD_TOPMARGIN 4   // Indentation of TreeView and Caption from the top edge
#define _TPD_TREE_W 100    // Width of TreeView
#define _TPD_CAPTION_H 16  // height of the caption
#define _TPD_BUTTON_W 50   // width of the button
#define _TPD_BUTTON_H 14   // height of buttons
#define _TPD_BUTTON_MARG 4 // spacing between buttons

CTPHCaptionWindow::CTPHCaptionWindow(HWND hDlg, int ctrlID)
    : CWindow(hDlg, ctrlID, ooAllocated)
{
    Allocated = 0;
    Text = NULL;
}

CTPHCaptionWindow::~CTPHCaptionWindow()
{
    if (Text != NULL)
        free(Text);
}

void CTPHCaptionWindow::SetText(const TCHAR* text)
{
    int l = (int)_tcslen(text);
    if (Allocated < l + 2)
    {
        if (Text != NULL)
            free(Text);
        Text = (TCHAR*)malloc((l + 2) * sizeof(TCHAR));
        if (Text == NULL)
        {
            TRACE_ET(_T("Low memory!"));
            Allocated = 0;
            return;
        }
        Allocated = l + 2;
    }
    _tcscpy_s(Text, Allocated, text);
    InvalidateRect(HWindow, NULL, TRUE);
    UpdateWindow(HWindow);
}

LRESULT
CTPHCaptionWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(HWindow, &ps);

        RECT r;
        GetClientRect(HWindow, &r);

        int devCaps = GetDeviceCaps(hdc, NUMCOLORS);
        if (devCaps == -1) // gradient will be used only for more than 256 colors
        {
            HBRUSH hOldBrush = (HBRUSH)GetCurrentObject(hdc, OBJ_BRUSH);
#define TPH_STEPS 100
            double stepW = (double)(r.right - r.left + 1) / TPH_STEPS;
            RECT r2 = r;
            r2.right = (long)(r2.left + stepW + 1);
            COLORREF base = GetSysColor(COLOR_BTNFACE);
            for (int i = 0; i <= TPH_STEPS; i++)
            {

                LOGBRUSH lb;
                lb.lbStyle = BS_SOLID;
                lb.lbColor = RGB(max(GetRValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0),
                                 max(GetGValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0),
                                 max(GetBValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0));
                HBRUSH hColorBrush = CreateBrushIndirect(&lb);
                /*          HBRUSH hColorBrush = CreateSolidBrush(RGB(max(GetRValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0),
        max(GetGValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0),
        max(GetBValue(base) - TPH_STEPS / 2 + i / 2 + 1, 0)));*/
                FillRect(hdc, &r2, hColorBrush);
                DeleteObject(hColorBrush);
                r2.left = (long)(i * stepW);
                r2.right = (long)(r2.left + stepW + 1);
            }
        }
        else
            FillRect(hdc, &r, (HBRUSH)(COLOR_GRAYTEXT + 1));

        if (Text != NULL)
        {
            int oldBkMode = SetBkMode(hdc, TRANSPARENT);

            HFONT hFont = NULL;
            HFONT hOldFont = NULL;
            LOGFONT srcLF;
            HFONT hSrcFont = (HFONT)HANDLES(GetStockObject(DEFAULT_GUI_FONT));
            GetObject(hSrcFont, sizeof(srcLF), &srcLF);
            srcLF.lfHeight = (int)(srcLF.lfHeight * 1.2);
            // srcLF.lfWeight = FW_BOLD; // looks quite ugly + unreadable on Vista
            hFont = CreateFontIndirect(&srcLF);
            hOldFont = (HFONT)SelectObject(hdc, hFont);

            int oldColor;
            if (devCaps == -1)
                oldColor = SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
            else
                oldColor = SetTextColor(hdc, GetSysColor(COLOR_CAPTIONTEXT));
            r.left += 8;
            DrawText(hdc, Text, (int)_tcslen(Text), &r, DT_SINGLELINE | DT_VCENTER | DT_LEFT | DT_NOPREFIX);
            SetTextColor(hdc, oldColor);
            SelectObject(hdc, hOldFont);
            SetBkMode(hdc, oldBkMode);

            if (hFont != NULL)
                DeleteObject(hFont);
        }
        EndPaint(HWindow, &ps);
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

LRESULT
CTPHGripWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SETCURSOR:
    {
        // we only want the north-south cursor
        SetCursor(LoadCursor(NULL, IDC_SIZENS));
        return TRUE;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

CTreePropHolderDlg::CTreePropHolderDlg(HWND hParent, DWORD* windowHeight)
    : CDialog(NULL, 0, hParent, ooStatic)
{
    HTreeView = NULL;
    ChildDialog = NULL;
    CaptionWindow = NULL;
    GripWindow = NULL;
    CurrentPageIndex = -1;
    ExitButton = -1;
    MinWindowSize.cx = 0;
    MinWindowSize.cy = 0;
    WindowHeight = windowHeight;
}

INT_PTR
CTreePropHolderDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    // WM_INITDIALOG will be called once we know the dimensions of the window
    if (TPD != NULL && uMsg != WM_INITDIALOG)
        TPD->DialogProc(uMsg, wParam, lParam); // forward message
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND hwnd = GetDlgItem(HWindow, _TPD_IDC_RECT);
        GetWindowRect(hwnd, &ChildDialogRect);
        POINT p;
        p.x = ChildDialogRect.left;
        p.y = ChildDialogRect.top;
        ScreenToClient(HWindow, &p);
        int w = ChildDialogRect.right - ChildDialogRect.left;
        int h = ChildDialogRect.bottom - ChildDialogRect.top;
        ChildDialogRect.left = p.x;
        ChildDialogRect.top = p.y;
        ChildDialogRect.right = p.x + w;
        ChildDialogRect.bottom = p.y + h;
        DestroyWindow(hwnd);
        HTreeView = GetDlgItem(HWindow, _TPD_IDC_TREE);
        BOOL appIsThemed = IsAppThemed();
        if (appIsThemed)
            SetWindowTheme(HTreeView, L"explorer", NULL);

        int treeIndent = 0;
        if (appIsThemed)
        {
            RECT rect = {0, 0, 4, 8};
            MapDialogRect(HWindow, &rect); // we obtain baseUnitX and baseUnitY for converting dlg-units to pixels
            treeIndent = MulDiv(9 /* indentation in dlg-units*/, rect.right /* baseUnitX*/, 4);
            TreeView_SetIndent(HTreeView, treeIndent);
        }

        // dlg units -> pixels conversions
        RECT r = {_TPD_BUTTON_W, _TPD_BUTTON_H, _TPD_LEFTMARGIN, _TPD_TOPMARGIN};
        MapDialogRect(HWindow, &r);
        ButtonSize.cx = r.left;
        ButtonSize.cy = r.top;
        MarginSize.cx = r.right;
        MarginSize.cy = r.bottom;
        r = {_TPD_CAPTION_H, _TPD_BUTTON_MARG, 0, 0};
        MapDialogRect(HWindow, &r);
        CaptionHeight = r.left;
        ButtonMargin = r.top;

        CaptionWindow = new CTPHCaptionWindow(HWindow, _TPD_IDC_CAPTION);
        if (CaptionWindow == NULL)
            TRACE_ET(_T("Low memory!"));
        TreeWidth = BuildAndMeasureTree() + 2 * treeIndent + treeIndent / 2 + GetSystemMetrics(SM_CXVSCROLL);
        if (TPD->StartPage < 0 || TPD->StartPage >= TPD->Count)
            TPD->StartPage = 0;
        TreeView_SelectItem(HTreeView, TPD->At(TPD->StartPage)->HTreeItem);

        GripWindow = new CTPHGripWindow(HWindow, _TPD_IDC_GRIP);

        // default dimensions are minimal - we will save them so that we can monitor them later
        GetWindowRect(HWindow, &r);
        RECT cR;
        GetClientRect(HWindow, &cR);
        int marginW = (r.right - r.left) - cR.right;
        int marginH = (r.bottom - r.top) - cR.bottom;
        MinWindowSize.cx = TreeWidth + ChildDialogRect.right - ChildDialogRect.left + 3 * MarginSize.cx + marginW;
        MinWindowSize.cy = MarginSize.cy + CaptionHeight + MarginSize.cy +
                           ChildDialogRect.bottom - ChildDialogRect.top +
                           MarginSize.cy + 1 + MarginSize.cy +
                           ButtonSize.cy + MarginSize.cy + marginH;

        // set the user dimension of the window and perform the layout of the element
        int height = (int)*WindowHeight;
        RECT clipR; // we do not want to be larger than the screen height
        MultiMonGetClipRectByWindow(HWindow, &clipR, NULL);
        if (height > clipR.bottom - clipR.top)
            height = clipR.bottom - clipR.top;
        if (height < MinWindowSize.cy)
            height = MinWindowSize.cy;
        SetWindowPos(HWindow, NULL, 0, 0, r.right - r.left, height,
                     SWP_NOZORDER | SWP_NOMOVE);

        LayoutControls();
        TreeView_EnsureVisible(HTreeView, TPD->At(TPD->StartPage)->HTreeItem);

        TPD->DialogProc(uMsg, wParam, lParam); // forward message

        break;
    }

    case WM_HELP:
    {
        if (WinLibHelp != NULL && ChildDialog != NULL && ChildDialog->HelpID != -1)
        {
            WinLibHelp->OnHelp(HWindow, ChildDialog->HelpID, (HELPINFO*)lParam,
                               (GetKeyState(VK_CONTROL) & 0x8000) != 0,
                               (GetKeyState(VK_SHIFT) & 0x8000) != 0);
        }
        return TRUE; // We will not let F1 fall through to the parent even if we do not call WinLibHelp->OnHelp()
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case _TPD_IDC_HELP:
        {
            if (WinLibHelp != NULL && ChildDialog != NULL && ChildDialog->HelpID != -1)
            {
                HELPINFO hi;
                memset(&hi, 0, sizeof(hi));
                hi.cbSize = sizeof(hi);
                hi.iContextType = HELPINFO_WINDOW;
                hi.dwContextId = ChildDialog->HelpID;
                GetCursorPos(&hi.MousePos);
                WinLibHelp->OnHelp(HWindow, ChildDialog->HelpID, &hi, FALSE, FALSE);
            }
            else
                TRACE_ET(_T("CTreePropHolderDlg::DialogProc(): ignoring _TPD_IDC_HELP: SetupWinLibHelp() was not called or ChildDialog is NULL or ChildDialog->HelpID is -1!"));
            return TRUE;
        }

        case _TPD_IDC_OK:
        {
            // need to validate the current page
            if (!ChildDialog->ValidateData())
                return TRUE;

            // Loop through all pages for transfer
            for (int i = 0; i < TPD->Count; i++)
                if (TPD->At(i)->HWindow != NULL)
                    if (!TPD->At(i)->TransferData(ttDataFromWindow))
                        return TRUE;
            wParam = IDOK;
        }
        case IDCANCEL:
        {
            ExitButton = LOWORD(wParam);
            return TRUE;
        }
        }

        // Forward the message so that the Enter key activates the default button
        if (ChildDialog != NULL && HIWORD(wParam) == BN_CLICKED)
            ::SendMessage(ChildDialog->HWindow, uMsg, wParam, lParam);

        break;
    }

    case WM_NOTIFY:
    {
        LPNMHDR pnmh = (LPNMHDR)lParam;
        if (pnmh->hwndFrom == HTreeView)
        {
            switch (pnmh->code)
            {
            case TVN_SELCHANGING:
            {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                CPropSheetPage* page = (CPropSheetPage*)pnmtv->itemOld.lParam;
                if (page != NULL && page->HWindow != NULL)
                {
                    NMHDR nmhdr;
                    nmhdr.hwndFrom = HWindow;
                    nmhdr.idFrom = _TPD_IDC_TREE;
                    nmhdr.code = PSN_KILLACTIVE;
                    SendMessage(page->HWindow, WM_NOTIFY, _TPD_IDC_TREE, (LPARAM)&nmhdr);
                    LONG_PTR res = GetWindowLongPtr(page->HWindow, DWLP_MSGRESULT);
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, res);
                    return TRUE;
                }
                break;
            }

            case TVN_SELCHANGED:
            {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                CPropSheetPage* page = (CPropSheetPage*)pnmtv->itemNew.lParam;
                if (page != NULL)
                {
                    for (int i = 0; i < TPD->Count; i++)
                        if (TPD->At(i) == page)
                        {
                            SelectPage(i);
                            NMHDR nmhdr;
                            nmhdr.hwndFrom = HWindow;
                            nmhdr.idFrom = _TPD_IDC_TREE;
                            nmhdr.code = PSN_SETACTIVE;
                            SendMessage(page->HWindow, WM_NOTIFY, _TPD_IDC_TREE, (LPARAM)&nmhdr);
                            break;
                        }
                }
                break;
            }

            case TVN_ITEMEXPANDED:
            {
                LPNMTREEVIEW pnmtv = (LPNMTREEVIEW)lParam;
                CPropSheetPage* page = (CPropSheetPage*)pnmtv->itemNew.lParam;
                if (page != NULL && page->Expanded != NULL)
                    *page->Expanded = (pnmtv->itemNew.state & TVIS_EXPANDED) != 0;
                break;
            }
            }
        }
        break;
    }

    case WM_NCHITTEST:
    {
        // We only want to resize in the vertical direction
        LRESULT ht = DefWindowProc(HWindow, uMsg, wParam, lParam);
        switch (ht)
        {
        case HTBOTTOMLEFT:
            ht = HTBOTTOM;
            break;
        case HTBOTTOMRIGHT:
            ht = HTBOTTOM;
            break;
        case HTTOPLEFT:
            ht = HTTOP;
            break;
        case HTTOPRIGHT:
            ht = HTTOP;
            break;
        case HTLEFT:
            ht = HTBORDER;
            break;
        case HTRIGHT:
            ht = HTBORDER;
            break;
        }
        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, ht);
        return TRUE;
    }

    case WM_GETMINMAXINFO:
    {
        // We only want to resize in the vertical direction
        LPMINMAXINFO lpmmi = (LPMINMAXINFO)lParam;
        lpmmi->ptMinTrackSize.x = MinWindowSize.cx;
        lpmmi->ptMaxTrackSize.x = MinWindowSize.cx;
        lpmmi->ptMinTrackSize.y = MinWindowSize.cy;

        // Adjust the height: https://blogs.msdn.microsoft.com/oldnewthing/20150504-00/?p=44944
        // FIXME
        //MONITORINFO mi = { sizeof(mi) };
        //GetMonitorInfo(MonitorFromWindow(HWindow,
        //               MONITOR_DEFAULTTOPRIMARY), &mi);
        //lpmmi->ptMaxSize.y = mi.rcWork.bottom - mi.rcWork.top -
        //                     lpmmi->ptMaxPosition.y + rc.bottom;
        break;
    }

    case WM_SIZE:
    {
        RECT r;
        GetWindowRect(HWindow, &r);
        *WindowHeight = r.bottom - r.top;
        LayoutControls();
        break;
    }

    case WM_SYSCOLORCHANGE:
    {
        TreeView_SetBkColor(HTreeView, GetSysColor(COLOR_WINDOW));
        break;
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CTreePropHolderDlg::LayoutControls()
{
    if (ChildDialog->HWindow == NULL)
        return;

    RECT cRect;
    GetClientRect(HWindow, &cRect);

    int sepY = cRect.bottom - MarginSize.cy - ButtonSize.cy - MarginSize.cy - 1;

    GripSize.cx = GetSystemMetrics(SM_CXVSCROLL);
    GripSize.cy = GetSystemMetrics(SM_CYHSCROLL);

    HDWP hdwp = HANDLES(BeginDeferWindowPos(8));
    if (hdwp != NULL)
    {
        // treeview
        hdwp = HANDLES(DeferWindowPos(hdwp, HTreeView, NULL,
                                      MarginSize.cx,
                                      MarginSize.cy,
                                      TreeWidth,
                                      sepY - 2 * MarginSize.cy,
                                      SWP_NOZORDER));
        // caption
        int captionX = MarginSize.cx + TreeWidth + MarginSize.cx;
        HWND hCaption = GetDlgItem(HWindow, _TPD_IDC_CAPTION);
        hdwp = HANDLES(DeferWindowPos(hdwp, hCaption, NULL,
                                      captionX,
                                      MarginSize.cy,
                                      cRect.right - MarginSize.cx - captionX,
                                      CaptionHeight,
                                      SWP_NOZORDER));
        // child dialog
        int dlgX = MarginSize.cx + TreeWidth + MarginSize.cx;
        int dlgY = MarginSize.cy + CaptionHeight + MarginSize.cy;
        ChildDialogRect.left = dlgX;
        ChildDialogRect.top = dlgY;
        ChildDialogRect.right = cRect.right - MarginSize.cx;
        ChildDialogRect.bottom = sepY - MarginSize.cy;
        hdwp = HANDLES(DeferWindowPos(hdwp, ChildDialog->HWindow, NULL,
                                      ChildDialogRect.left,
                                      ChildDialogRect.top,
                                      ChildDialogRect.right - ChildDialogRect.left,
                                      ChildDialogRect.bottom - ChildDialogRect.top,
                                      SWP_NOZORDER));
        // separator
        HWND hSeparator = GetDlgItem(HWindow, _TPD_IDC_SEP);
        hdwp = HANDLES(DeferWindowPos(hdwp, hSeparator, NULL,
                                      MarginSize.cx,
                                      sepY,
                                      cRect.right - 2 * MarginSize.cx,
                                      1,
                                      SWP_NOZORDER));
        // OK button
        int buttonsX = cRect.right - (3 * ButtonSize.cx + 2 * ButtonMargin) - GripSize.cx / 2;
        int buttonsY = sepY + 1 + MarginSize.cy;
        HWND hOK = GetDlgItem(HWindow, _TPD_IDC_OK);
        hdwp = HANDLES(DeferWindowPos(hdwp, hOK, NULL,
                                      buttonsX,
                                      buttonsY,
                                      ButtonSize.cx,
                                      ButtonSize.cy,
                                      SWP_NOZORDER));
        // Cancel button
        HWND hCancel = GetDlgItem(HWindow, IDCANCEL);
        hdwp = HANDLES(DeferWindowPos(hdwp, hCancel, NULL,
                                      buttonsX + ButtonSize.cx + ButtonMargin,
                                      buttonsY,
                                      ButtonSize.cx,
                                      ButtonSize.cy,
                                      SWP_NOZORDER));
        // Help button
        HWND hHelp = GetDlgItem(HWindow, _TPD_IDC_HELP);
        hdwp = HANDLES(DeferWindowPos(hdwp, hHelp, NULL,
                                      buttonsX + 2 * ButtonSize.cx + 2 * ButtonMargin,
                                      buttonsY,
                                      ButtonSize.cx,
                                      ButtonSize.cy,
                                      SWP_NOZORDER));
        // Grip (resize)
        HWND hGrip = GetDlgItem(HWindow, _TPD_IDC_GRIP);
        hdwp = HANDLES(DeferWindowPos(hdwp, hGrip, NULL,
                                      cRect.right - GripSize.cx,
                                      cRect.bottom - GripSize.cy,
                                      GripSize.cx,
                                      GripSize.cy,
                                      SWP_NOZORDER));

        HANDLES(EndDeferWindowPos(hdwp));
        // hack: there seems to be a bug in the treeview/common controls: if the scrollbar appears due to the content,
        // the selected item is not redrawn, so it is cropped on the right; could it be related to full row
        // select a and still aero look; in any case, the redraw under W7 doesn't blink, we can probably afford it
        InvalidateRect(HTreeView, NULL, false);
    }
}

int CTreePropHolderDlg::BuildAndMeasureTree()
{
    int width = 0;
    for (int i = 0; i < TPD->Count; i++)
    {
        TVINSERTSTRUCT tvis;
        tvis.hParent = NULL;
        if (TPD->At(i)->ParentPage != NULL)
            tvis.hParent = TPD->At(i)->ParentPage->HTreeItem;
        tvis.hInsertAfter = TVI_LAST;
        tvis.item.mask = TVIF_TEXT | TVIF_STATE | TVIF_PARAM;
        tvis.item.pszText = TPD->At(i)->Title;
        tvis.item.cchTextMax = (int)_tcslen(TPD->At(i)->Title);
        tvis.item.state = 0;
        // WARNING: expandable items here must be expanded, otherwise subsequent TreeView_GetItemRect() will return FALSE
        // and random data in the RECT r rectangle
        if (TPD->At(i)->Expanded != NULL)
            tvis.item.state |= TVIS_EXPANDED;
        tvis.item.stateMask = tvis.item.state;
        tvis.item.lParam = (LPARAM)TPD->At(i);
        TPD->At(i)->HTreeItem = TreeView_InsertItem(HTreeView, &tvis);
        RECT r;
        // We should consider the return value of TreeView_GetItemRect() instead
        if (TreeView_GetItemRect(HTreeView, TPD->At(i)->HTreeItem, &r, TRUE) && r.right - r.left > width)
            width = r.right - r.left;
    }
    // We can now close non-expanded items
    for (int i = 0; i < TPD->Count; i++)
    {
        if (TPD->At(i)->Expanded != NULL && *TPD->At(i)->Expanded == FALSE)
            TreeView_Expand(HTreeView, TPD->At(i)->HTreeItem, TVE_COLLAPSE);
    }
    return width;
}

void CTreePropHolderDlg::EnableButtons()
{
}

BOOL CTreePropHolderDlg::SelectPage(int pageIndex)
{
    if (pageIndex < 0 || pageIndex >= TPD->Count)
        return FALSE;
    HWND HHideWindow = NULL;
    if (ChildDialog != NULL)
    {
        HHideWindow = ChildDialog->HWindow;
        ChildDialog = NULL;
    }

    if (pageIndex != CurrentPageIndex)
    {
        ChildDialog = TPD->At(pageIndex);
        if (ChildDialog->HWindow == NULL)
        {
            ChildDialog->SetParent(HWindow);
            ChildDialog->Create();
        }

        NMHDR nmhdr;
        nmhdr.hwndFrom = HWindow;
        nmhdr.idFrom = 0;
        nmhdr.code = PSN_SETACTIVE;
        SendMessage(ChildDialog->HWindow, WM_NOTIFY, 0, (LPARAM)&nmhdr);

        if (HHideWindow != NULL)
            SetWindowPos(HHideWindow, 0, 0, 0, 0, 0, SWP_NOZORDER | SWP_NOMOVE | SWP_NOSIZE | SWP_NOREDRAW | SWP_HIDEWINDOW);

        CaptionWindow->SetText(ChildDialog->Title);

        SetWindowPos(ChildDialog->HWindow, HTreeView,
                     ChildDialogRect.left, ChildDialogRect.top,
                     ChildDialogRect.right - ChildDialogRect.left,
                     ChildDialogRect.bottom - ChildDialogRect.top,
                     SWP_SHOWWINDOW);
        CurrentPageIndex = pageIndex;
        EnableButtons();
    }
    return TRUE;
}

void CTreePropHolderDlg::OnCtrlTab(BOOL shift)
{
    int pageIndex = CurrentPageIndex;
    pageIndex += shift ? -1 : 1;
    if (pageIndex < 0)
        pageIndex = TPD->Count - 1;
    if (pageIndex >= TPD->Count)
        pageIndex = 0;
    if (pageIndex != CurrentPageIndex)
    {
        if (TreeView_SelectItem(HTreeView, TPD->At(pageIndex)->HTreeItem))
        {
            HWND hFocus = GetFocus();
            if (hFocus != HTreeView)
            {
                HWND hFirst = GetNextDlgTabItem(ChildDialog->HWindow, NULL, FALSE);
                if (hFirst != NULL)
                    SetFocus(hFirst);
                else
                    SetFocus(HTreeView);
            }
        }
    }
}

int CTreePropHolderDlg::ExecuteIndirect(LPCDLGTEMPLATE hDialogTemplate)
{
    HWND hOldFocus = GetFocus();
    EnableWindow(Parent, FALSE);
    CreateDialogIndirectParam(Modul, hDialogTemplate, Parent,
                              (DLGPROC)CDialog::CDialogProc, (LPARAM)this);
    MSG msg;
    while (ExitButton == -1 && GetMessage(&msg, NULL, 0, 0))
    {
        CWindowsObject* wnd = WindowsManager.GetWindowPtr(GetActiveWindow());
        if ((msg.message == WM_KEYDOWN || msg.message == WM_KEYUP) &&
            msg.wParam == VK_TAB && (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0)
        {
            if (msg.message == WM_KEYDOWN)
                OnCtrlTab((GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
        }
        else if (wnd == NULL || !wnd->Is(otDialog) || !IsDialogMessage(wnd->HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    EnableWindow(Parent, TRUE);
    DestroyWindow(HWindow);
    SetFocus(hOldFocus);
    return ExitButton;
}

WORD* CTreePropDialog::lpdwAlign(WORD* lpIn)
{
    DWORD_PTR ul;
    ul = (DWORD_PTR)lpIn;
    ul += 3;
    ul >>= 2;
    ul <<= 2;
    return (WORD*)ul;
}

int WinLibCopyText(WCHAR* buf, const TCHAR* text, int bufLen)
{
#ifdef UNICODE
    lstrcpyn(buf, text, bufLen);
    return (int)wcslen(buf) + 1;
#else  // UNICODE
    return MultiByteToWideChar(CP_ACP, 0, text, -1, buf, bufLen);
#endif // UNICODE
}

int CTreePropDialog::AddItemEx(LPWORD& lpw, const TCHAR* className, WORD id, int x, int y, int cx, int cy,
                               UINT style, UINT exStyle, const TCHAR* text)
{
    lpw = lpdwAlign(lpw); // align DLGITEMTEMPLATEEX on DWORD boundary
    *(DWORD*)lpw = 0;     // helpID
    lpw += 2;
    *(DWORD*)lpw = exStyle; // exStyle
    lpw += 2;
    *(DWORD*)lpw = style; // style
    lpw += 2;
    *lpw++ = x;
    *lpw++ = y;
    *lpw++ = cx;
    *lpw++ = cy;
    *(DWORD*)lpw = id; // id
    lpw += 2;
    LPWSTR lpwsz = (LPWSTR)lpw;
    lpw += WinLibCopyText(lpwsz, className, 50);
    if (text == NULL)
        *lpw++ = 0; // no name
    else
    {
        lpwsz = (LPWSTR)lpw;
        lpw += WinLibCopyText(lpwsz, text, 50);
    }
    *lpw++ = 0; // no creation data
    return TRUE;
}

int CTreePropDialog::Execute(const TCHAR* buttonOK,
                             const TCHAR* buttonCancel,
                             const TCHAR* buttonHelp)
{
    if (Count > 0)
    {
        RECT maxPageRect;
        SetRectEmpty(&maxPageRect);

        // find out the maximum dimensions
        for (int i = 0; i < Count; i++)
        {
            At(i)->ParentDialog = this;
            HRSRC hrsrc = FindResource(Modul, MAKEINTRESOURCE(At(i)->ResID), RT_DIALOG);
            if (hrsrc == NULL)
            {
                TRACE_ET(_T("Unable to find resource for page number: ") << i);
                return 0;
            }
            HGLOBAL hglb = LoadResource(Modul, hrsrc);
            WORD* pageTemplate = (WORD*)LockResource(hglb);
            if (pageTemplate == NULL)
            {
                TRACE_ET(_T("Unable to find resource for page number: ") << i);
                return 0;
            }
            BOOL dlgEx = *pageTemplate /*dlgVer*/ == 1 && *(pageTemplate + 1) /*signature*/ == 0xffff; // DLGEX
            if (!dlgEx)
                TRACE_CT(_T("CTreePropDialog::Execute(): DLG is no longer supported! PageResID=") << At(i)->ResID);
            DWORD dlgStyle = 0;
            short dlgCX = 0;
            short dlgCY = 0;
            WCHAR* dlgTitle = NULL;

            dlgStyle = *(DWORD*)(pageTemplate + 6); // style
            dlgCX = *(short*)(pageTemplate + 11);   // cx
            dlgCY = *(short*)(pageTemplate + 12);   // cy
            WORD* t = pageTemplate + 13;            // menu, let's skip to the dialog class, and then to its title
            if (*t == 0)
                t++; // no menu
            else
            {
                if (*t == 0xffff)
                    t += 2; // menu ID
                else
                    t += wcslen((wchar_t*)t) + 1; // menu string
            }
            if (*t == 0)
                t++; // no dialog class
            else
            {
                if (*t == 0xffff)
                    t += 2; // ID of the dialog class
                else
                    t += wcslen((wchar_t*)t) + 1; // string of the dialog class
            }
            dlgTitle = (WCHAR*)t;

            if (At(i)->Title == NULL)
            {
                int len = 1;
                if (dlgStyle & WS_CAPTION)
                {
#ifdef UNICODE
                    len += (int)wcslen(dlgTitle) + 1;
#else  // UNICODE
                    len += WideCharToMultiByte(CP_ACP, 0, dlgTitle, -1, NULL, 0, NULL, NULL);
#endif // UNICODE
                }
                At(i)->Title = new TCHAR[len];
                if (len > 1)
                {
#ifdef UNICODE
                    lstrcpyn(At(i)->Title, dlgTitle, len);
#else  // UNICODE
                    WideCharToMultiByte(CP_ACP, 0, dlgTitle, -1, At(i)->Title, len, NULL, NULL);
#endif // UNICODE
                }
                At(i)->Title[len - 1] = 0;
            }

            if (dlgCX > maxPageRect.right)
                maxPageRect.right = dlgCX;
            if (dlgCY > maxPageRect.bottom)
                maxPageRect.bottom = dlgCY;
        }

        // distance from the bottom edge of the dialog to the bottom of the TreeView and ChildDialog
        int lowMargin = 2 * _TPD_TOPMARGIN + _TPD_BUTTON_H + _TPD_TOPMARGIN + _TPD_TOPMARGIN / 2;
        SIZE dialogSize;
        dialogSize.cx = _TPD_LEFTMARGIN + _TPD_TREE_W + _TPD_LEFTMARGIN + maxPageRect.right +
                        _TPD_LEFTMARGIN;
        dialogSize.cy = _TPD_TOPMARGIN + _TPD_CAPTION_H + _TPD_TOPMARGIN + maxPageRect.bottom +
                        lowMargin;

        // build a template for the dialog: DLG or DLGEX, depending on the page format, must be consistent,
        // otherwise, the control is pruned and the font of the pages differs from the rest of the tree property dialog

        HGLOBAL hgbl;

        LPWORD lpw;
        LPWSTR lpwsz;
        hgbl = GlobalAlloc(GMEM_ZEROINIT, 1024);
        if (!hgbl)
            return -1;
        lpw = (LPWORD)GlobalLock(hgbl); // Define a dialog box.
        *lpw++ = 1;
        *lpw++ = 0xffff;  // DLGEX
        *(DWORD*)lpw = 0; // helpID
        lpw += 2;
        *(DWORD*)lpw = 0; // exStyle
        lpw += 2;
        // style
        *(DWORD*)lpw = WS_VISIBLE | WS_POPUP | WS_BORDER | WS_SYSMENU | WS_CAPTION |
                       DS_SETFONT | DS_MODALFRAME | DS_CENTER | DS_FIXEDSYS | WS_SIZEBOX;
        lpw += 2;
        *lpw++ = 8; // cDlgItems (number of controls)
        *lpw++ = 0; // x
        *lpw++ = 0; // and
        *lpw++ = 0; // cx
        *lpw++ = 0; // cy
        *lpw++ = 0; // no menu
        *lpw++ = 0; // predefined dialog box class (by default)
        lpwsz = (LPWSTR)lpw;
        lpw += WinLibCopyText(lpwsz, Caption, 100); // title
        *lpw++ = 8;                                 // font size
        *lpw++ = FW_NORMAL;                         // font weight
        *(BYTE*)lpw = FALSE;                        // Is the font italic?
        *((BYTE*)lpw + 1) = ANSI_CHARSET;           // font charset
        lpw++;
        lpwsz = (LPWSTR)lpw; // font typeface
        lpw += WinLibCopyText(lpwsz, _T("MS Shell Dlg 2"), 50);

        BOOL appIsThemed = IsAppThemed();

        // TreeView
        AddItemEx(lpw, WC_TREEVIEW, _TPD_IDC_TREE,
                  0, 0, 0, 0,
                  WS_BORDER | WS_CHILD | WS_VISIBLE | WS_TABSTOP |
                      TVS_DISABLEDRAGDROP | TVS_HASBUTTONS | TVS_LINESATROOT | TVS_SHOWSELALWAYS |
                      (appIsThemed ? TVS_FULLROWSELECT : TVS_HASLINES),
                  0, NULL);
        // Caption
        AddItemEx(lpw, _T("static"), _TPD_IDC_CAPTION,
                  0, 0, 0, 0,
                  WS_CHILD | WS_VISIBLE, 0, NULL);
        // Static, which is replaced by a child dialog during initialization
        AddItemEx(lpw, _T("static"), _TPD_IDC_RECT,
                  0, 0, maxPageRect.right, maxPageRect.bottom,
                  WS_CHILD, 0, NULL);
        // Separator
        AddItemEx(lpw, _T("static"), _TPD_IDC_SEP,
                  0, 0, 0, 0,
                  WS_GROUP | WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, 0, NULL);
        // Bottom row of buttons
        AddItemEx(lpw, _T("button"), _TPD_IDC_OK,
                  0, 0, 0, 0,
                  WS_CHILD | WS_VISIBLE | BS_DEFPUSHBUTTON | WS_TABSTOP, 0, buttonOK);
        AddItemEx(lpw, _T("button"), IDCANCEL,
                  0, 0, 0, 0,
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, buttonCancel);
        AddItemEx(lpw, _T("button"), _TPD_IDC_HELP,
                  0, 0, 0, 0,
                  WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON | WS_TABSTOP, 0, buttonHelp);
        // Grip (resize)
        AddItemEx(lpw, _T("scrollbar"), _TPD_IDC_GRIP,
                  0, 0, 0, 0,
                  WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CLIPCHILDREN | SBS_SIZEBOX | SBS_SIZEBOXBOTTOMRIGHTALIGN, 0, _T(""));

        GlobalUnlock(hgbl);

        return Dialog.ExecuteIndirect((LPDLGTEMPLATE)hgbl);
    }
    else
    {
        TRACE_ET(_T("Incorrect call to CPropertyDialog::Execute."));
        return -1;
    }
}

int CTreePropDialog::GetCurSel()
{
    TVITEM item;
    item.hItem = TreeView_GetSelection(Dialog.HTreeView);
    item.mask = TVIF_PARAM;
    TreeView_GetItem(Dialog.HTreeView, &item);
    CPropSheetPage* page = (CPropSheetPage*)item.lParam;
    for (int i = 0; i < Count; i++)
        if (At(i) == page)
            return i;
    return -1;
}

int CTreePropDialog::Add(CPropSheetPage* page, CPropSheetPage* parent, BOOL* expanded)
{
    int ret = CPropertyDialog::Add(page);
    if (IsGood())
    {
        page->ParentPage = parent;
        if (expanded != NULL)
            page->Expanded = expanded;
    }
    return ret;
}
