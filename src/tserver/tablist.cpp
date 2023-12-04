// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <crtdbg.h>
#include <ostream>
#include <stdio.h>
#include <commctrl.h>
#include <limits.h>

#include "lstrfix.h"
#include "trace.h"
#include "messages.h"
#include "handles.h"
#include "array.h"
#include "str.h"
#include "winlib.h"
#include "sheets.h"
#include "tablist.h"
#include "tserver.h"
#include "dialog.h"
#include "dib.h"
#include "registry.h"
#include "config.h"

#include "tserver.rh"
#include "tserver.rh2"

// pro owner draw listbox
#define LEFT_MARGIN 5
#define RIGHT_MARGIN 6

#define TYPE_BITMAP_HEIGHT 14

#define GET_X_LPARAM(lp) ((int)(short)LOWORD(lp))
#define GET_Y_LPARAM(lp) ((int)(short)HIWORD(lp))

struct HeaderItem
{
    const WCHAR* Text;
    int Format;
};

HeaderItem HeaderItems[HEADER_ITEMS] = {{L"", LVCFMT_CENTER},
                                        {L"PID", LVCFMT_RIGHT},
                                        {L"UPID", LVCFMT_RIGHT},
                                        {L"PName", LVCFMT_LEFT},
                                        {L"TID", LVCFMT_RIGHT},
                                        {L"UTID", LVCFMT_RIGHT},
                                        {L"TName", LVCFMT_LEFT},
                                        {L"Date", LVCFMT_RIGHT},
                                        {L"Time", LVCFMT_RIGHT},
                                        {L"Counter [ms]", LVCFMT_RIGHT},
                                        {L"Module", LVCFMT_LEFT},
                                        {L"Line", LVCFMT_RIGHT},
                                        {L"Message", LVCFMT_LEFT}};

//****************************************************************************
//
// CListView
//

CListView::CListView()
    : CWindow(ooStatic)
{
    HToolTip = NULL;
    LastItem = -1;
    LastSubItem = -1;
}

CListView::~CListView()
{
    if (HToolTip != NULL)
        DestroyWindow(HToolTip);
}

void CListView::Attach(HWND hListView)
{
    AttachToWindow(hListView);

    HToolTip = CreateWindow(TOOLTIPS_CLASS, NULL, TTS_ALWAYSTIP,
                            CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                            HWindow, NULL, HInstance, NULL);
    SetWindowPos(HToolTip, HWND_TOPMOST, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOREDRAW | SWP_NOSIZE);
    SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_INITIAL, 0);
    SendMessage(HToolTip, TTM_SETDELAYTIME, TTDT_RESHOW, 0);

    TOOLINFO ti;
    ti.cbSize = sizeof(ti);
    ti.uFlags = 0;
    ti.hwnd = HWindow;
    ti.hinst = 0;
    ti.lpszText = (LPWSTR)L"";
    ti.uId = 0;
    ti.uFlags = TTF_TRANSPARENT;
    ti.rect.left = 0;     //GridRect.left;
    ti.rect.top = 0;      //GridRect.top;
    ti.rect.right = 100;  //GridRect.right;
    ti.rect.bottom = 100; //GridRect.bottom;
    ::SendMessage(HToolTip, TTM_ADDTOOL, 0, (LPARAM)(LPTOOLINFO)&ti);
}

void CListView::GotoError(int nVirtKey)
{
    int i;
    int select = -1;
    switch (nVirtKey)
    {
    case VK_HOME:
    {
        for (i = 0; i < Data.Messages.Count; i++)
            if (IsErrorMsg(Data.Messages[i].Type))
            {
                select = i;
                break;
            }
        break;
    }

    case VK_END:
    {
        for (i = Data.Messages.Count - 1; i >= 0; i--)
            if (IsErrorMsg(Data.Messages[i].Type))
            {
                select = i;
                break;
            }
        break;
    }

    case VK_UP:
    {
        int curSelect = ListView_GetNextItem(HWindow, -1, LVIS_SELECTED);
        if (curSelect == -1)
            curSelect = Data.Messages.Count - 1;
        else
            curSelect--;
        for (i = curSelect; i >= 0; i--)
            if (IsErrorMsg(Data.Messages[i].Type))
            {
                select = i;
                break;
            }
        break;
    }

    case VK_DOWN:
    {
        int curSelect = ListView_GetNextItem(HWindow, -1, LVIS_SELECTED);
        if (curSelect == -1)
            curSelect = 0;
        else
            curSelect++;
        for (i = curSelect; i < Data.Messages.Count; i++)
            if (IsErrorMsg(Data.Messages[i].Type))
            {
                select = i;
                break;
            }
        break;
    }
    }
    if (select != -1)
    {
        ListView_SetItemState(HWindow, select, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(HWindow, select, FALSE);
    }
}

void RemoveEOLs(WCHAR* buff)
{
    WCHAR* end = buff + wcslen(buff);
    WCHAR* s = buff;
    while (s < end)
    {
        WCHAR* eol = s;
        while (*s == L'\r' || *s == L'\n')
            s++;
        if (eol < s)
        {
            memmove(eol, s, sizeof(WCHAR) * ((end - s) + 1));
            end -= (s - eol);
            s = eol;
        }
        s++;
    }
}

LRESULT CListView::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_KEYDOWN:
    {
        int nVirtKey = (int)wParam;
        if (nVirtKey == VK_UP || nVirtKey == VK_DOWN ||
            nVirtKey == VK_HOME || nVirtKey == VK_END)
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0)
            {
                GotoError(nVirtKey);
                return 0;
            }
        }
        if (nVirtKey == VK_SPACE)
        {
            TabList->SwitchDeltaMode();
        }
        break;
    }

    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    {
        if (HToolTip != NULL)
        {
            MSG msg;
            msg.wParam = wParam;
            msg.lParam = lParam;
            msg.message = uMsg;
            msg.hwnd = HWindow;
            ::SendMessage(HToolTip, TTM_RELAYEVENT, 0, (LPARAM)(LPMSG)&msg);
        }

        if (HToolTip != NULL && uMsg == WM_MOUSEMOVE)
        {
            LVHITTESTINFO hti;
            hti.pt.x = LOWORD(lParam);
            hti.pt.y = HIWORD(lParam);

            if (ListView_SubItemHitTest(HWindow, &hti) != -1 && (hti.iItem != LastItem || hti.iSubItem != LastSubItem))
            {
                WCHAR buff[1000];
                buff[0] = 0;

                int index = TabList->ColumnIndex[hti.iSubItem];
                TabList->GetText(hti.iItem, index, buff, _countof(buff), TRUE);

                LastWidth = ListView_GetStringWidth(HWindow, buff);

                LastItem = hti.iItem;
                LastSubItem = hti.iSubItem;

                RECT rect;
                ListView_GetSubItemRect(HWindow, hti.iItem, hti.iSubItem, LVIR_BOUNDS, &rect);

                TOOLINFO ti;
                ti.cbSize = sizeof(ti);
                ti.uFlags = 0;
                ti.hwnd = HWindow;
                ti.hinst = 0;
                ti.uId = 0;
                ti.uFlags = TTF_TRANSPARENT;
                ti.rect = rect;

                RECT cr;
                GetClientRect(HWindow, &cr);
                if (rect.right > cr.right)
                    rect.right = cr.right;

                if (LastWidth > rect.right - rect.left - 12)
                {
                    RemoveEOLs(buff);
                    ti.lpszText = buff;
                }
                else
                {
                    ti.lpszText = (LPWSTR)L"";
                    LastItem = -1;
                }
                ::SendMessage(HToolTip, TTM_SETTOOLINFO, 0, (LPARAM)(LPTOOLINFO)&ti);
            }
        }

        break;
    }

    case WM_NOTIFY:
    {
        NMHDR* nmhdr = (NMHDR*)lParam;

        if (nmhdr->code == TTN_SHOW && nmhdr->hwndFrom == HToolTip)
        {
            RECT rect;
            ListView_GetSubItemRect(HWindow, LastItem, LastSubItem, LVIR_BOUNDS, &rect);

            POINT pt;
            int width;
            if (WindowsVistaAndLater)
            {
                width = LastWidth + 11;
                pt.x = rect.left;
                pt.y = rect.top;
            }
            else
            {
                width = LastWidth + 7;
                pt.x = rect.left + 3;
                pt.y = rect.top - 1;
            }
            ClientToScreen(HWindow, &pt);

            int cx = GetSystemMetrics(SM_CXFULLSCREEN);
            if (pt.x + width > cx)
                pt.x = cx - width;

            SetWindowPos(HToolTip, HWND_TOPMOST,
                         pt.x,
                         pt.y,
                         width, rect.bottom - rect.top + 2,
                         SWP_NOACTIVATE);
            return TRUE;
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CTabList
//

CTabList::CTabList(CObjectOrigin origin)
    : CWindow(origin)
{
    HListView = NULL;
    HImageList = ImageList_LoadImage(HInstance,
                                     MAKEINTRESOURCE(BMP_TYPE),
                                     14,
                                     0,
                                     RGB(128, 0, 128),
                                     IMAGE_BITMAP,
                                     0);
    AnchorIndex = -1;
    CounterAnchor = 0;
    DeltaMode = FALSE;
}

CTabList::~CTabList()
{
    if (HImageList != NULL)
        ImageList_Destroy(HImageList);
}

void CTabList::RedrawListView()
{
    InvalidateRect(HListView, NULL, TRUE);
    UpdateWindow(HListView);
}

void CTabList::SetCount(int count)
{
    BOOL scrollToLatest = ConfigData.ScrollToLatestMessage;
    BOOL focusLatest = FALSE;
    if (!scrollToLatest) // pokud scroll neni zaply naporad, budeme scrollovat jen je-li kurzor na posledni polozce
    {
        int curSelect = ListView_GetNextItem(HListView, -1, LVIS_SELECTED);
        int lvCount = ListView_GetItemCount(HListView);
        if (curSelect != -1 && curSelect + 1 == lvCount || lvCount == 0)
        {
            scrollToLatest = TRUE;
            focusLatest = TRUE;
        }
    }

    ListView_SetItemCountEx(HListView,
                            count,
                            /*(count > 0 ? LVSICF_NOINVALIDATEALL : 0) |*/ //p.s.: pri dosazeni limitniho poctu messages dojde k prepisu (pri posunu) dat a nedojde k prekresleni
                            LVSICF_NOSCROLL);
    if (count == 0)
    {
        UpdateWindow(HListView);
    }
    else if (scrollToLatest)
    {
        if (focusLatest)
            ListView_SetItemState(HListView, count - 1, LVIS_FOCUSED | LVIS_SELECTED, LVIS_FOCUSED | LVIS_SELECTED);
        ListView_EnsureVisible(HListView, count - 1, FALSE);
    }
}

HWND CTabList::GetListViewHWND()
{
    return HListView;
}

void CTabList::BuildHeader()
{
    LV_COLUMN lvc;

    // odstranim stare sloupce
    while (ListView_DeleteColumn(HListView, 0) == TRUE)
        ;

    lvc.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
    int index = 0;
    CounterColumnIndex = -1;
    MessageColumnIndex = -1;
    for (int i = 0; i < HEADER_ITEMS; i++) // vytvorim sloupce
    {
        if (*(&(ConfigData.ViewColumnVisible_Type) + i))
        {
            if (i == 0)
                ListView_SetImageList(HListView, HImageList, LVSIL_SMALL);

            lvc.pszText = (LPWSTR)HeaderItems[i].Text;
            lvc.fmt = HeaderItems[i].Format;

            lvc.iSubItem = i;
            lvc.cx = *(&(ConfigData.ViewColumnWidth_Type) + i);
            ListView_InsertColumn(HListView, index, &lvc);
            lvc.mask |= LVCF_SUBITEM;
            ColumnIndex[index] = i;
            if (i == 9)
                CounterColumnIndex = index;
            if (i == 12)
                MessageColumnIndex = index;
            index++;
        }
    }
}

void CTabList::SwitchDeltaMode()
{
    DeltaMode = !DeltaMode;
    GetCounterRect();
    RepaintCounterColumn();
}

void CTabList::GetCounterRect()
{
    if (CounterColumnIndex == -1)
        return;
    RECT r;
    HWND hHeader = ListView_GetHeader(HListView);

    ListView_GetSubItemRect(HListView, 0, CounterColumnIndex, LVIR_BOUNDS, &r);

    CounterColumnRect.left = r.left;
    CounterColumnRect.right = r.right;
    ListView_GetOrigin(HListView, &r);

    GetWindowRect(hHeader, &r);
    CounterColumnRect.top = r.bottom - r.top;
    GetClientRect(HListView, &r);
    CounterColumnRect.bottom = r.bottom;
}

void CTabList::GetHeaderWidths()
{
    int index = 0;
    for (int i = 0; i < HEADER_ITEMS; i++) // vytvorim sloupce
    {
        if (*(&(ConfigData.ViewColumnVisible_Type) + i))
        {
            int width = ListView_GetColumnWidth(HListView, index);
            index++;
            *(&(ConfigData.ViewColumnWidth_Type) + i) = width;
        }
    }
}

void CTabList::RepaintCounterColumn()
{
    if (CounterColumnIndex == -1)
        return;
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    if (index != AnchorIndex)
    {
        if (index == -1)
            CounterAnchor = 0;
        else
            CounterAnchor = Data.Messages[index].Counter;
        InvalidateRect(HListView, &CounterColumnRect, FALSE);
        UpdateWindow(HListView);
    }
}

int CTabList::GetSelectedIndex()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
    return index;
}

void CTabList::GetText(int iItem, int index, WCHAR* buff, int buffMax, BOOL preferEndOfText)
{
    switch (index)
    {
    case 1:
    {
        swprintf_s(buff, buffMax, L"%d", Data.Messages[iItem].ProcessID);
        break;
    }

    case 2:
    {
        swprintf_s(buff, buffMax, L"%d", Data.Messages[iItem].UniqueProcessID);
        break;
    }

    case 3:
    {
        Data.GetProcessName(Data.Messages[iItem].UniqueProcessID, buff, buffMax);
        break;
    }

    case 4:
    {
        swprintf_s(buff, buffMax, L"%d", Data.Messages[iItem].ThreadID);
        break;
    }

    case 5:
    {
        swprintf_s(buff, buffMax, L"%d", Data.Messages[iItem].UniqueThreadID);
        break;
    }

    case 6:
    {
        Data.GetThreadName(Data.Messages[iItem].UniqueProcessID,
                           Data.Messages[iItem].UniqueThreadID,
                           buff, buffMax);
        break;
    }

    case 7:
    {
        GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE,
                      &Data.Messages[iItem].Time, NULL, buff,
                      buffMax);
        break;
    }

    case 8:
    {
        GetTimeFormat(LOCALE_USER_DEFAULT, TIME_FORCE24HOURFORMAT,
                      &Data.Messages[iItem].Time,
                      L"hh':'mm':'ss", buff, buffMax);
        SWPrintFToEnd_s(buff, buffMax, L".%03d", Data.Messages[iItem].Time.wMilliseconds);
        break;
    }

    case 9:
    {
        double d = Data.Messages[iItem].Counter;
        if (DeltaMode)
            d -= CounterAnchor;

        swprintf_s(buff, buffMax, L"%.3lf", d);
        break;
    }

    case 10:
    {
        wcscpy_s(buff, buffMax, Data.Messages[iItem].File);
        break;
    }

    case 11:
    {
        swprintf_s(buff, buffMax, L"%d", Data.Messages[iItem].Line);
        break;
    }

    case 12: // pozor na osetreni DBLCLK, je vazano na konstantu 12!
    {
        const WCHAR* s = Data.Messages[iItem].Message;
        if (preferEndOfText) // chceme vratit konec textu (napr. do tooltipu)
        {
            int len = wcslen(s);
            if (len + 1 > buffMax)
            {
                s += len - (buffMax - 1);
                if (IS_LOW_SURROGATE(*s))
                    s++; // UTF-16 muze obsahovat surrogate pairs, pripadny 2. znak paru preskocime (at text nezaciname v druhe pulce znaku)
            }
        }
        lstrcpyn(buff, s, buffMax);
        break;
    }
    }
}

#ifndef LVS_EX_DOUBLEBUFFER
#define LVS_EX_DOUBLEBUFFER 0x00010000
#endif

LRESULT
CTabList::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        RECT r;
        GetClientRect(HWindow, &r);

        HListView = CreateWindowEx(WS_EX_CLIENTEDGE,
                                   L"SysListView32",
                                   L"",
                                   WS_VISIBLE | WS_CHILD | WS_VSCROLL | WS_HSCROLL | WS_CLIPSIBLINGS |
                                       LVS_REPORT | LVS_OWNERDATA | LVS_SHOWSELALWAYS | LVS_SINGLESEL |
                                       LVS_NOSORTHEADER,
                                   r.left, r.top, r.right, r.bottom,
                                   HWindow,
                                   NULL, // hMenu
                                   HInstance,
                                   0);

        DWORD exStyle = LVS_EX_FULLROWSELECT;
        if (WindowsVistaAndLater)
            exStyle |= LVS_EX_DOUBLEBUFFER;
        ListView_SetExtendedListViewStyleEx(HListView, exStyle, exStyle);

        ListView.TabList = this;
        ListView.Attach(HListView);

        BuildHeader();
        break;
    }

    case WM_DESTROY:
    {
        ListView.DetachWindow();
        GetHeaderWidths();
        break;
    }

    case WM_ERASEBKGND:
    {
        return TRUE;
    }

    case WM_SIZE:
    {
        RECT r;
        GetClientRect(HWindow, &r);
        // umistim listvie
        if (HListView != NULL)
            SetWindowPos(HListView, NULL, 0, 0, r.right, r.bottom, SWP_NOMOVE);
        break;
    }

    case WM_USER_HEADER_CHANGE:
    {
        BuildHeader();
        return 0;
    }

    case WM_NOTIFY:
    {
        if (((LPNMHDR)lParam)->code == HDN_ENDTRACK)
        {
            TOOLINFO ti;
            ti.cbSize = sizeof(ti);
            ti.uFlags = 0;
            ti.hwnd = HWindow;
            ti.hinst = 0;
            ti.uId = 0;
            ti.uFlags = TTF_TRANSPARENT;
            ti.lpszText = (LPWSTR)L"";
            ::SendMessage(ListView.HToolTip, TTM_SETTOOLINFO, 0, (LPARAM)(LPTOOLINFO)&ti);
            ListView.LastItem = -1;
            break;
        }

        if (((LPNMHDR)lParam)->hwndFrom == HListView)
        {
            switch (((LPNMHDR)lParam)->code)
            {
            case NM_DBLCLK:
            {
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if (index != -1)
                {
                    LVHITTESTINFO ht;
                    DWORD pos = GetMessagePos();
                    ht.pt.x = GET_X_LPARAM(pos);
                    ht.pt.y = GET_Y_LPARAM(pos);
                    ScreenToClient(HListView, &ht.pt);
                    index = ListView_HitTest(HListView, &ht);
                    if (index != -1 && ht.iItem == index)
                    {
                        RECT r;
                        if (MessageColumnIndex != -1)
                            ListView_GetSubItemRect(HListView, index, MessageColumnIndex, LVIR_LABEL, &r);
                        if (MessageColumnIndex != -1 && PtInRect(&r, ht.pt))
                            MainWindow->ShowMessageDetails();
                        else
                            Data.GotoEditor(index);
                    }
                }
                return 0;
            }

            case LVN_ITEMCHANGED:
            {
                if (DeltaMode)
                {
                    GetCounterRect();
                    RepaintCounterColumn();
                }
                break;
            }

            case LVN_KEYDOWN:
            {
                NMLVKEYDOWN* nmkd = (NMLVKEYDOWN*)lParam;
                switch (nmkd->wVKey)
                {
                case VK_RETURN:
                {
                    int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                    if (index != -1)
                        Data.GotoEditor(index);
                    break;
                }

                case VK_ESCAPE:
                {
                    MainWindow->Activate();
                    break;
                }

                case VK_DELETE:
                {
                    MainWindow->ClearAllMessages();
                    break;
                }

                case VK_TAB:
                {
                    MainWindow->ShowMessageDetails();
                    break;
                }
                }
                return 0;
            }

            case LVN_GETDISPINFO:
            {
                LV_DISPINFO* info = (LV_DISPINFO*)lParam;
                int iItem = info->item.iItem;
                int iSubItem = info->item.iSubItem;
                int index = ColumnIndex[iSubItem];

                if (index == 0)
                    info->item.mask |= LVIF_IMAGE;
                else
                    info->item.mask |= LVIF_TEXT;

                //            WCHAR buff[1000];
                //            buff[0] = 0;
                //            info->item.pszText = buff;
                //            info->item.pszText[0] = 0;

                if (index == 0)
                {
                    info->item.mask |= LVIF_IMAGE;
                    info->item.iImage = IsErrorMsg(Data.Messages[iItem].Type) ? 1 : 0;
                }
                else
                {
                    GetText(iItem, index, info->item.pszText, info->item.cchTextMax);
                }
                break;
            }

            case NM_CUSTOMDRAW:
            {
                LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;

                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                {
                    return CDRF_NOTIFYITEMDRAW;
                }

                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    // pozadame si o zaslani notifikace CDDS_ITEMPREPAINT | CDDS_SUBITEM
                    return CDRF_NOTIFYSUBITEMDRAW;
                }

                // ERRORS vykreslime cervene
                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
                {
                    int index = cd->nmcd.dwItemSpec;
                    if (index >= 0 && index < Data.Messages.Count)
                    {
                        if (IsErrorMsg(Data.Messages[index].Type))
                        {
                            cd->clrText = RGB(255, 0, 0);
                            return CDRF_NEWFONT;
                        }
                    }
                    break;
                }

                break;
            }
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
