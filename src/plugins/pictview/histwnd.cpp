// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "pictview.h"
#include "dialogs.h"
#include "histwnd.h"

enum
{
    IDX_TB_OTHERCHANNELS = 28,
    IDX_TB_LUMINOSITY,
    IDX_TB_RED,
    IDX_TB_GREEN,
    IDX_TB_BLUE,
    IDX_TB_RGBSUM,
};

// menu item skill level ALL (zahrnuje beginned, intermediate a advanced)
#define MNTS_ALL (MNTS_B | MNTS_I | MNTS_A)

//****************************************************************************
//
// CHistogramControl
//

CHistogramControl::CHistogramControl(LPDWORD* data)
    : CWindow(ooStatic), Channel(RGBSum), ShowOtherChannels(TRUE)
{
    Colors[Luminosity] = 0xB4B4B4;
    Colors[RGBSum] = 0xB4B4B4;
    Colors[Red] = 0xB4B4B4;
    Colors[Green] = 0xB4B4B4;
    Colors[Blue] = 0xB4B4B4;
    Colors[Background] = 0x000000;

    Pen[Luminosity] = CreatePen(PS_SOLID, 0, 0x00FFFF);
    Pen[RGBSum] = CreatePen(PS_SOLID, 0, 0xFFFFFF);
    Pen[Red] = CreatePen(PS_SOLID, 0, 0x0000FF);
    Pen[Green] = CreatePen(PS_SOLID, 0, 0x00FF00);
    Pen[Blue] = CreatePen(PS_SOLID, 0, 0xFF4020);

    LightHPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DHILIGHT));
    FaceHPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DFACE));
    ShadowHPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DSHADOW));

    DWORD peak[5] = {0, 0, 0, 0, 0};
    double average = 0;
    int c;
    for (c = 0; c < 5; c++)
    {
        int i;
        for (i = 0; i < 256; i++)
        {
            peak[c] = max(peak[c], data[c][i]);
            average += data[c][i] / (double)(5 * 256);
        }
    }

    DWORD maxPeak = 0xFFFFFFFF; // maximalni peak (vsechny vyssi hodnoty se na nej oriznou)
                                //  for (c = 0; c < 5; c++)
                                //    if (maxPeak > (DWORD)(1.2 * peak[c])) maxPeak = (DWORD)(1.2 * peak[c]);
    maxPeak = (DWORD)(average * 4);
    if (maxPeak < 10)
        maxPeak = 10; // extremne male hodnoty se nam nehodi

    int d;
    for (d = 0; d < 5; d++)
    {
        int i;
        for (i = 0; i < 256; i++)
        {
            if (data[d][i] < maxPeak)
                Data[d][i] = double(data[d][i]) / double(maxPeak);
            else
                Data[d][i] = 1;
        }
    }
}

CHistogramControl::~CHistogramControl()
{
    int c;
    for (c = 0; c < 4; c++)
        DeleteObject(Pen[c]);
    DeleteObject(LightHPen);
    DeleteObject(FaceHPen);
    DeleteObject(ShadowHPen);
}

int CHistogramControl::SetChannel(int channel)
{
    // TODO prekreslit okno
    InvalidateRect(HWindow, NULL, FALSE);
    return Channel = channel;
}

BOOL CHistogramControl::SetShowOtherChannels(BOOL show)
{
    // TODO prekreslit okno
    InvalidateRect(HWindow, NULL, FALSE);
    return ShowOtherChannels = show;
}

void CHistogramControl::Paint()
{
    int mag = (DC.GetRect().right - DC.GetRect().left) / 256;
    int height = max(
        DC.GetRect().bottom - DC.GetRect().top -
            SeparatorHeight - ToneBandHeight,
        0);
    int barHeight;
    HPEN oldPen;

    // vymazeme pozadi
    SetBkColor(DC, Colors[Background]);
    FastFillRect(DC, DC.GetRect());

    // vykreslime histogramu
    SetBkColor(DC, Colors[Channel]);
    int i;
    for (i = 0; i < 256; i++)
    {
        barHeight = int(height * Data[Channel][i] + 0.5);
        FastFillRect(DC, i * mag, height - barHeight, i * mag + mag, height);
    }

    if (ShowOtherChannels)
    {
        // projedeme ostatni kanaly a vykrelime po castech jejich krivky
        int c;
        for (c = 0; c < 5; c++)
        {
            if (c != Channel)
            {
                oldPen = HPEN(SelectObject(DC, Pen[c]));
                MoveToEx(DC, 0, height - int(height * Data[c][0] + 0.5), NULL);
                int j;
                for (j = 0; j < 256; j++)
                {
                    barHeight = int(height * Data[c][j] + 0.5);
                    LineTo(DC, j * mag + mag / 2, height - barHeight);
                }
                LineTo(DC, DC.GetRect().right - DC.GetRect().left, height - barHeight);
                SelectObject(DC, oldPen);
            }
        }
    }

    // vykreslime separator
    int top = height;
    oldPen = HPEN(SelectObject(DC, LightHPen));
    MoveToEx(DC, 0, top, NULL);
    LineTo(DC, DC.GetRect().right - DC.GetRect().left, top);
    top++;

    HPEN(SelectObject(DC, FaceHPen));
    MoveToEx(DC, 0, top, NULL);
    LineTo(DC, DC.GetRect().right - DC.GetRect().left, top);
    top++;

    HPEN(SelectObject(DC, ShadowHPen));
    MoveToEx(DC, 0, top, NULL);
    LineTo(DC, DC.GetRect().right - DC.GetRect().left, top);
    top++;
    SelectObject(DC, oldPen);

    // vykreslime tonalni pruh pod histogramem
    DWORD color;
    int j;
    for (j = 0; j < 256; j++)
    {
        switch (Channel)
        {
        case Luminosity:
            color = RGB((BYTE)j, (BYTE)j, (BYTE)j);
            break;
        case RGBSum:
            color = RGB((BYTE)j, (BYTE)j, (BYTE)j);
            break;
        case Red:
            color = RGB((BYTE)j, 0, 0);
            break;
        case Green:
            color = RGB(0, (BYTE)j, 0);
            break;
        case Blue:
            color = RGB(0, 0, (BYTE)j);
            break;
        }
        SetBkColor(DC, color);
        FastFillRect(DC, j * mag, top, j * mag + mag, DC.GetRect().bottom);
    }
}

LRESULT
CHistogramControl::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
        DC.SetWindow(HWindow);
        break;

    case WM_SIZE:
        DC.Update();
        break;

    case WM_PAINT:
        DC.BeginPaint();
        Paint();
        DC.EndPaint();
        break;

    case WM_ERASEBKGND:
        return 1;
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CHistogramWindow
//

CHistogramWindow::CConfiguration CHistogramWindow::Configuration;
LPCTSTR CHistogramWindow::RegistryName = _T("Histogram Configuration");

CHistogramWindow::CHistogramWindow(CViewerWindow* viewer, LPDWORD* data)
    : CWindow(ooStatic), Histogram(data), Viewer(viewer)
{
    Toolbar = NULL;
}

void CHistogramWindow::Show()
{
    CreateEx(/*WS_EX_TOOLWINDOW*/ 0,
             CWINDOW_CLASSNAME, LoadStr(IDS_HIST_HISTOGRAM),
             WS_POPUP | WS_CLIPCHILDREN | WS_CAPTION | WS_SYSMENU | WS_THICKFRAME,
             0, 0, 100, 100, Viewer->HWindow, NULL,
             DLLInstance, this);

    Viewer->HHistogramWindow = HWindow; // chceme notifikace o zmene fontu, protoze mame toolbar

    MSG msg;
    BOOL ret = TRUE;
    while (HWindow != NULL && // pri zavreni z jineho threadu (shutdown / zavreni hl. okna Salama) uz okno neexistuje
           IsWindow(HWindow) && (ret = GetMessage(&msg, NULL, 0, 0)) != 0)
    {
        if (ret == -1)
            break;
        // zpracujeme zpravu
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    Viewer->HHistogramWindow = NULL;
}

void CHistogramWindow::LoadConfiguration(HKEY key,
                                         CSalamanderRegistryAbstract* registry)
{
    CConfiguration cfg;

    if (registry->GetValue(key, RegistryName, REG_BINARY, &cfg, sizeof(cfg)) &&
        cfg.Version == CConfiguration::CurrentVersion)
        Configuration = cfg;
}

void CHistogramWindow::SaveConfiguration(HKEY key,
                                         CSalamanderRegistryAbstract* registry)
{
    registry->SetValue(key, RegistryName, REG_BINARY, &Configuration,
                       sizeof(Configuration));
}

BOOL CHistogramWindow::Init()
{
    // vytvorime toolbaru
    Toolbar = SalamanderGUI->CreateToolBar(HWindow);
    if (!Toolbar)
        return FALSE;
    Toolbar->SetStyle(TLB_STYLE_IMAGE | TLB_STYLE_TEXT);
    Toolbar->CreateWnd(HWindow);
    Toolbar->SetImageList(Viewer->HGrayToolBarImageList);
    Toolbar->SetHotImageList(Viewer->HHotToolBarImageList);

    // vyber kanalu
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_ID | TLBI_MASK_TEXT | TLBI_MASK_STYLE;
    tii.Style = TLBI_STYLE_DROPDOWN | TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_SHOWTEXT;
    tii.Text = LoadStr(IDS_HIST_LUMINOSITY);
    tii.ID = cmdChannel;
    Toolbar->InsertItem2(0xFFFFFFFF, TRUE, &tii);

    // separator
    tii.Mask = TLBI_MASK_STYLE;
    tii.Style = TLBI_STYLE_SEPARATOR;
    Toolbar->InsertItem2(0xFFFFFFFF, TRUE, &tii);

    // prepina zobrazeni ostatnich kanalu
    tii.Mask = TLBI_MASK_IMAGEINDEX | TLBI_MASK_ID | TLBI_MASK_STYLE;
    tii.Style = TLBI_STYLE_CHECK;
    tii.ImageIndex = IDX_TB_OTHERCHANNELS;
    tii.ID = cmdOtherChannels;
    Toolbar->InsertItem2(0xFFFFFFFF, TRUE, &tii);

    // vytvorime histogram
    Histogram.CreateEx(WS_EX_STATICEDGE, CWINDOW_CLASSNAME, _T(""),
                       WS_CHILD | WS_VISIBLE,
                       0, Toolbar->GetNeededHeight(), 10, 10, HWindow, NULL,
                       DLLInstance, &Histogram);
    Histogram.SetChannel(Configuration.Channel);
    Histogram.SetShowOtherChannels(Configuration.ShowOtherChannels);

    RECT wr, cr;
    GetWindowRect(HWindow, &wr);
    GetClientRect(HWindow, &cr);
    FrameWidth = wr.right - wr.left - (cr.right - cr.left);
    int frameHeight = wr.bottom - wr.top - (cr.bottom - cr.top);

    GetWindowRect(Histogram.HWindow, &wr);
    GetClientRect(Histogram.HWindow, &cr);
    FrameWidth += wr.right - wr.left - (cr.right - cr.left);
    frameHeight += wr.bottom - wr.top - (cr.bottom - cr.top);

    UpdateToolbar();
    ShowWindow(Toolbar->GetHWND(), SW_SHOW);

    int width = Configuration.HistogramWidth + FrameWidth;
    int height =
        Toolbar->GetNeededHeight() +
        Configuration.HistogramHeight + frameHeight;

    RECT r;
    GetWindowRect(Viewer->HWindow, &wr);
    if (Configuration.WindowX < 0)
    {
        r.left = (wr.left + wr.right - width) / 2;
        r.top = (wr.top + wr.bottom - height) / 2;
    }
    else
    {
        r.left = Configuration.WindowX;
        r.top = Configuration.WindowY;
    }
    r.right = r.left + width;
    r.bottom = r.top + height;
    SalamanderGeneral->MultiMonEnsureRectVisible(&r, FALSE);

    SetWindowPos(HWindow, NULL, r.left, r.top, width, height,
                 SWP_NOZORDER | SWP_SHOWWINDOW);

    EnableWindow(Viewer->HWindow, FALSE);

    return TRUE;
}

void CHistogramWindow::Destroy()
{
    RECT r;

    GetWindowRect(HWindow, &r);
    Configuration.WindowX = r.left;
    Configuration.WindowY = r.top;

    GetClientRect(Histogram.HWindow, &r);
    Configuration.HistogramHeight = r.bottom - r.top;
    Configuration.HistogramWidth = r.right - r.left;

    Configuration.Channel = Histogram.GetChannel();
    Configuration.ShowOtherChannels = Histogram.GetShowOtherChannels();

    if (Toolbar)
        SalamanderGUI->DestroyToolBar(Toolbar);
    if (Histogram.HWindow)
        DestroyWindow(Histogram.HWindow);
}

void CHistogramWindow::Layout()
{
    RECT wr, cr;
    GetWindowRect(HWindow, &wr);
    GetClientRect(HWindow, &cr);

    HDWP hdwp = BeginDeferWindowPos(2);

    hdwp = DeferWindowPos(hdwp, Toolbar->GetHWND(), NULL, 0, 0,
                          max(cr.right - cr.left, 0),
                          Toolbar->GetNeededHeight(),
                          SWP_NOZORDER | SWP_NOACTIVATE);

    hdwp = DeferWindowPos(hdwp, Histogram.HWindow, NULL, 0, 0,
                          max(cr.right - cr.left, 0),
                          max(cr.bottom - Toolbar->GetNeededHeight(), 0),
                          SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE);

    EndDeferWindowPos(hdwp);
}

void CHistogramWindow::UpdateToolbar()
{
    TLBI_ITEM_INFO2 tii;
    tii.Mask = TLBI_MASK_TEXT;
    tii.Text = LoadStr(IDS_HIST_LUMINOSITY + Histogram.GetChannel());
    Toolbar->SetItemInfo2(cmdChannel, FALSE, &tii);
    Toolbar->CheckItem(cmdOtherChannels, FALSE, Histogram.GetShowOtherChannels());
}

LRESULT
CHistogramWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CREATE:
    {
        if (!Init())
            return -1;
        break;
    }

    case WM_SIZING:
    {
        LPRECT r = LPRECT(lParam);
        int width =
            max((r->right - r->left - FrameWidth + 128) & ~int(0xFF), 256) + FrameWidth;
        switch (wParam)
        {
        case WMSZ_BOTTOMLEFT:
        case WMSZ_LEFT:
        case WMSZ_TOPLEFT:
            r->left = r->right - width;
        default:
            r->right = r->left + width;
        }
        return TRUE;
    }

    case WM_WINDOWPOSCHANGED:
    {
        WINDOWPOS* lpwndpos = (WINDOWPOS*)lParam;
        if (!(lpwndpos->flags & SWP_NOSIZE))
        {
            Layout();
        }
        break;
    }

    case WM_KEYDOWN:
    {
        int cmd = -1;
        switch (wParam)
        {
        case 'L':
            cmd = cmdLuminosity;
            break;
        case 'A':
            cmd = cmdRGBSum;
            break;
        case 'R':
            cmd = cmdRed;
            break;
        case 'G':
            cmd = cmdGreen;
            break;
        case 'B':
            cmd = cmdBlue;
            break;
        case 'O':
            cmd = cmdOtherChannels;
            break;
        case VK_ESCAPE:
            PostMessage(HWindow, WM_CLOSE, 0, 0);
            break;
        }
        if (cmd > 0)
        {
            PostMessage(HWindow, WM_COMMAND, cmd, 0);
            return 0;
        }
        break;
    }

    case WM_COMMAND:
    {
        int cmd = LOWORD(wParam);
        switch (cmd)
        {
        case cmdLuminosity:
        case cmdRed:
        case cmdGreen:
        case cmdBlue:
        case cmdRGBSum:
            Histogram.SetChannel(cmd - cmdChannel);
            UpdateToolbar();
            break;
        case cmdOtherChannels:
            Histogram.SetShowOtherChannels(!Histogram.GetShowOtherChannels());
            UpdateToolbar();
            break;
        }
        break;
    }

    case WM_USER_TBGETTOOLTIP:
    {
        TOOLBAR_TOOLTIP* tt = (TOOLBAR_TOOLTIP*)lParam;
        if (tt->ID == cmdOtherChannels)
        {
            _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_OTHERCHANELS));
            return TRUE;
        }
        else
            switch (Histogram.GetChannel())
            {
            case CHistogramControl::Luminosity:
                _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_LUMINOSITY));
                return TRUE;
            case CHistogramControl::RGBSum:
                _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_RGBSUM));
                return TRUE;
            case CHistogramControl::Red:
                _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_RED));
                return TRUE;
            case CHistogramControl::Green:
                _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_GREEN));
                return TRUE;
            case CHistogramControl::Blue:
                _tcscpy(tt->Buffer, LoadStr(IDS_HIST_TIP_BLUE));
                return TRUE;
            }
        break;
    }

    case WM_USER_TBDROPDOWN:
    {
        if (Toolbar != NULL && Toolbar->GetHWND() == (HWND)wParam)
        {
            int index = (int)lParam;
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_ID;
            if (!Toolbar->GetItemInfo2(index, TRUE, &tii))
                return 0;

            RECT r;
            Toolbar->GetItemRect(index, r);

            switch (tii.ID)
            {
            case cmdChannel:
            {
                // vytvorime menu
                MENU_TEMPLATE_ITEM templ[] =
                    {
                        {MNTT_PB, 0, 0, 0, -1, 0, NULL},
                        {MNTT_IT, IDS_HIST_MENU_LUMINOSITY, MNTS_ALL, cmdLuminosity, IDX_TB_LUMINOSITY, 0, NULL},
                        {MNTT_IT, IDS_HIST_MENU_RGBSUM, MNTS_ALL, cmdRGBSum, IDX_TB_RGBSUM, 0, NULL},
                        {MNTT_IT, IDS_HIST_MENU_RED, MNTS_ALL, cmdRed, IDX_TB_RED, 0, NULL},
                        {MNTT_IT, IDS_HIST_MENU_GREEN, MNTS_ALL, cmdGreen, IDX_TB_GREEN, 0, NULL},
                        {MNTT_IT, IDS_HIST_MENU_BLUE, MNTS_ALL, cmdBlue, IDX_TB_BLUE, 0, NULL},
                        {MNTT_PE, 0, 0, 0, -1, 0, NULL}};

                CGUIMenuPopupAbstract* menu = SalamanderGUI->CreateMenuPopup();
                if (!menu)
                    return TRUE;
                if (!menu->LoadFromTemplate(HLanguage, templ, NULL, Viewer->HHotToolBarImageList, NULL))
                {
                    SalamanderGUI->DestroyMenuPopup(menu);
                    return TRUE;
                }
                DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD, r.left, r.bottom, HWindow, &r);
                SalamanderGUI->DestroyMenuPopup(menu);
                if (cmd > 0)
                    PostMessage(HWindow, WM_COMMAND, cmd, 0);
                break;
            }
            }
        }
        return 0;
    }

    case WM_USER_SETTINGCHANGE:
    {
        if (Toolbar != NULL)
            Toolbar->SetFont();
        return 0;
    }

    case WM_CLOSE:
    {
        EnableWindow(Viewer->HWindow, TRUE);
        break;
    }

    case WM_DESTROY:
    {
        Destroy();
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
