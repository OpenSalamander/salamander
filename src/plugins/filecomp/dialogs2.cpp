// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

CColorsCfgButton CommonColors[2] =
    {
        {
            IDS_COMMONTEXT,    // TextID
            LINENUM_FG_NORMAL, // ColorLineNumFG
            LINENUM_BK_NORMAL, // ColorLineNumBK
            TEXT_FG_NORMAL,    // ColorTextFG
            TEXT_BK_NORMAL     // ColorTextBK
        },
        {
            IDS_SELECTEDTEXT,  // TextID
            -1,                // ColorLineNumFG
            -1,                // ColorLineNumBK
            TEXT_FG_SELECTION, // ColorTextFG
            TEXT_BK_SELECTION  // ColorTextBK
        }};

CColorsCfgButton LeftColors[4] =
    {
        {
            IDS_CHANGEDTEXT,        // TextID
            LINENUM_FG_LEFT_CHANGE, // ColorLineNumFG
            LINENUM_BK_LEFT_CHANGE, // ColorLineNumBK
            TEXT_FG_LEFT_CHANGE,    // ColorTextFG
            TEXT_BK_LEFT_CHANGE     // ColorTextBK
        },
        {
            IDS_FOCUSEDCHANGE,              // TextID
            LINENUM_FG_LEFT_CHANGE_FOCUSED, // ColorLineNumFG
            LINENUM_BK_LEFT_CHANGE_FOCUSED, // ColorLineNumBK
            TEXT_FG_LEFT_CHANGE_FOCUSED,    // ColorTextFG
            TEXT_BK_LEFT_CHANGE_FOCUSED     // ColorTextBK
        },
        {
            IDS_COMMONFOCUSED,    // TextID
            -1,                   // ColorLineNumFG
            -1,                   // ColorLineNumBK
            TEXT_FG_LEFT_FOCUSED, // ColorTextFG
            TEXT_BK_LEFT_FOCUSED  // ColorTextBK
        },
        {
            IDS_FOCUSEDFRAME,    // TextID
            -1,                  // ColorLineNumFG
            LINENUM_LEFT_BORDER, // ColorLineNumBK
            -1,                  // ColorTextFG
            TEXT_LEFT_BORDER     // ColorTextBK
        }};

CColorsCfgButton RightColors[4] =
    {
        {
            IDS_CHANGEDTEXT,         // TextID
            LINENUM_FG_RIGHT_CHANGE, // ColorLineNumFG
            LINENUM_BK_RIGHT_CHANGE, // ColorLineNumBK
            TEXT_FG_RIGHT_CHANGE,    // ColorTextFG
            TEXT_BK_RIGHT_CHANGE     // ColorTextBK
        },
        {
            IDS_FOCUSEDCHANGE,               // TextID
            LINENUM_FG_RIGHT_CHANGE_FOCUSED, // ColorLineNumFG
            LINENUM_BK_RIGHT_CHANGE_FOCUSED, // ColorLineNumBK
            TEXT_FG_RIGHT_CHANGE_FOCUSED,    // ColorTextFG
            TEXT_BK_RIGHT_CHANGE_FOCUSED     // ColorTextBK
        },
        {
            IDS_COMMONFOCUSED,     // TextID
            -1,                    // ColorLineNumFG
            -1,                    // ColorLineNumBK
            TEXT_FG_RIGHT_FOCUSED, // ColorTextFG
            TEXT_BK_RIGHT_FOCUSED  // ColorTextBK
        },
        {
            IDS_FOCUSEDFRAME,     // TextID
            -1,                   // ColorLineNumFG
            LINENUM_RIGHT_BORDER, // ColorLineNumBK
            -1,                   // ColorTextFG
            TEXT_RIGHT_BORDER     // ColorTextBK
        }};

CColorsCfgItem ColorItems[CD_NUMBER_OF_ITEMS] =
    {
        {IDS_COMMONCOLORS, 2, CommonColors},
        {IDS_LEFTCOLORS, 4, LeftColors},
        {IDS_RIGHTCOLORS, 4, RightColors}};

CPropPageColors::CPropPageColors(SALCOLOR* colors, DWORD* changeFlag,
                                 CPropPageGeneral* pageGeneral)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGCOLORS, IDD_CFGCOLORS, PSP_HASHELP, NULL)
{
    CALL_STACK_MESSAGE1("CColorsDialog::CColorsDialog(, )");
    Colors = colors;
    memcpy(TempColors, Colors, NUMBER_OF_COLORS * sizeof(SALCOLOR));
    HPalette = NULL;
    ChangeFlag = changeFlag;
    PageGeneral = pageGeneral;
    UpdateDefaultColors(TempColors, HPalette);
}

CPropPageColors::~CPropPageColors()
{
    CALL_STACK_MESSAGE1("CPropPageColors::~CPropPageColors()");
    if (HPalette)
        DeleteObject(HPalette);
    //MainWindowQueue.Remove(HWindow);
}

int DescriptionIDs[] = {IDS_DESCRIPTION1, IDS_DESCRIPTION2, IDS_DESCRIPTION3, IDS_DESCRIPTION4};
int LineNumIDs[] = {IDB_LINENUM1, IDB_LINENUM2, IDB_LINENUM3, IDB_LINENUM4};
int TextIDs[] = {IDB_TEXT1, IDB_TEXT2, IDB_TEXT3, IDB_TEXT4};

void CPropPageColors::ItemChanged(int i)
{
    CALL_STACK_MESSAGE2("CPropPageColors::ItemChanged(%d)", i);
    int j;
    for (j = 0; j < ColorItems[i].ButtonsCount; j++)
    {
        if (j >= 4)
        {
            TRACE_E("j >= 4!");
            continue;
        }
        ShowWindow(GetDlgItem(HWindow, DescriptionIDs[j]), SW_SHOW);
        SetWindowText(GetDlgItem(HWindow, DescriptionIDs[j]), LoadStr(ColorItems[i].Buttons[j].TextID));

        if (ColorItems[i].Buttons[j].ColorLineNumBK == -1)
            ShowWindow(GetDlgItem(HWindow, LineNumIDs[j]), SW_HIDE);
        else
        {
            if (ColorItems[i].Buttons[j].ColorLineNumFG == -1)
                ColorButtons[j][LINENUM_COLOR_BTN]->SetColor(
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorLineNumBK]),
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorLineNumBK]));
            else
                ColorButtons[j][LINENUM_COLOR_BTN]->SetColor(
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorLineNumFG]),
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorLineNumBK]));

            ShowWindow(GetDlgItem(HWindow, LineNumIDs[j]), SW_SHOW);
        }

        if (ColorItems[i].Buttons[j].ColorTextBK == -1)
            ShowWindow(GetDlgItem(HWindow, TextIDs[j]), SW_HIDE);
        else
        {
            if (ColorItems[i].Buttons[j].ColorTextFG == -1)
                ColorButtons[j][TEXT_COLOR_BTN]->SetColor(
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorTextBK]),
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorTextBK]));
            else
                ColorButtons[j][TEXT_COLOR_BTN]->SetColor(
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorTextFG]),
                    GetPALETTERGB(TempColors[ColorItems[i].Buttons[j].ColorTextBK]));

            ShowWindow(GetDlgItem(HWindow, TextIDs[j]), SW_SHOW);
        }
    }
    for (; j < 4; j++)
    {
        ShowWindow(GetDlgItem(HWindow, DescriptionIDs[j]), SW_HIDE);
        ShowWindow(GetDlgItem(HWindow, LineNumIDs[j]), SW_HIDE);
        ShowWindow(GetDlgItem(HWindow, TextIDs[j]), SW_HIDE);
    }
}

void CPropPageColors::UpdateColors(int i)
{
    CALL_STACK_MESSAGE2("CPropPageColors::UpdateColors(%d)", i);
    UpdateDefaultColors(TempColors, HPalette);
    ItemChanged(i);
    Preview1->ColorsChanged();
    Preview2->ColorsChanged();
    if (UsePalette)
    {
        PostMessage(HWindow, WM_QUERYNEWPALETTE, 0, 0);
    }
}

void CPropPageColors::ChooseColor(int item, int colorFG, int colorBK, const RECT* btnRect)
{
    CALL_STACK_MESSAGE4("CPropPageColors::ChooseColor(%d, %d, %d,)", item,
                        colorFG, colorBK);
    BOOL automatic;
    HMENU hMenu, hSubMenu;
    if (colorFG == -1)
    {
        // vybirame jen jednu barvu
        hMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_CTX_1COLOR_MENU));
        hSubMenu = GetSubMenu(hMenu, 0);
    }
    else
    {
        // vybirame dve barvy
        hMenu = LoadMenu(HLanguage, MAKEINTRESOURCE(IDM_CTX_2COLOR_MENU));
        hSubMenu = GetSubMenu(hMenu, 0);
        automatic = GetFValue(TempColors[colorFG]) & SCF_DEFAULT;
        CheckMenuItem(hSubMenu, CM_CUSTOMTEXT, MF_BYCOMMAND | (automatic ? MF_UNCHECKED : MF_CHECKED));
        CheckMenuItem(hSubMenu, CM_AUTOTEXT, MF_BYCOMMAND | (automatic ? MF_CHECKED : MF_UNCHECKED));

        // podivame se, zda existuje defaultni hodnota k teto barve
        automatic = GetFValue(DefaultColors[colorFG]) & SCF_DEFAULT;
        EnableMenuItem(hSubMenu, CM_AUTOTEXT, MF_BYCOMMAND | (automatic ? MF_ENABLED : MF_GRAYED));
    }
    automatic = GetFValue(TempColors[colorBK]) & SCF_DEFAULT;
    CheckMenuItem(hSubMenu, CM_CUSTOMBACKGROUND, MF_BYCOMMAND | (automatic ? MF_UNCHECKED : MF_CHECKED));
    CheckMenuItem(hSubMenu, CM_AUTOBACKGROUND, MF_BYCOMMAND | (automatic ? MF_CHECKED : MF_UNCHECKED));

    // podivame se, zda existuje defaultni hodnota k teto barve
    automatic = GetFValue(DefaultColors[colorBK]) & SCF_DEFAULT;
    EnableMenuItem(hSubMenu, CM_AUTOBACKGROUND, MF_BYCOMMAND | (automatic ? MF_ENABLED : MF_GRAYED));

    int color = colorBK;
    TPMPARAMS tpmPar;
    tpmPar.cbSize = sizeof(tpmPar);
    tpmPar.rcExclude = *btnRect;
    switch (TrackPopupMenuEx(hSubMenu, TPM_TOPALIGN | TPM_RIGHTBUTTON | TPM_NONOTIFY | TPM_RETURNCMD,
                             btnRect->right, btnRect->top, HWindow, &tpmPar))
    {
    case CM_CUSTOMTEXT:
        color = colorFG; // jedeme dal...
    case CM_CUSTOMBACKGROUND:
    {
        CHOOSECOLOR cc;
        //memset(&cc, 0, sizeof(CHOOSECOLOR));
        cc.lStructSize = sizeof(CHOOSECOLOR);
        cc.hwndOwner = HWindow;
        cc.rgbResult = GetCOLORREF(TempColors[color]);
        cc.lpCustColors = CustomColors;
        cc.Flags = CC_ANYCOLOR | CC_FULLOPEN | CC_RGBINIT | CC_ENABLEHOOK /* | CC_SOLIDCOLOR*/;
        cc.lpfnHook = ComDlgHookProc;
        if (::ChooseColor(&cc))
        {
            SetSALCOROR(&TempColors[color], cc.rgbResult, 0);
        }
        break;
    }

    case CM_AUTOTEXT:
        SetFValue(TempColors[colorFG], SCF_DEFAULT);
        break;
    case CM_AUTOBACKGROUND:
        SetFValue(TempColors[colorBK], SCF_DEFAULT);
        break;
    }
    DestroyMenu(hMenu);
    UpdateColors(item);
}

void CPropPageColors::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPropPageColors::Transfer()");
    if (ti.Type == ttDataFromWindow)
    {
        if (memcmp(Colors, TempColors, sizeof(SALCOLOR) * NUMBER_OF_COLORS) != 0)
            *ChangeFlag |= CC_COLORS;
        memcpy(Colors, TempColors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
    }
}

INT_PTR
CPropPageColors::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPropPageColors::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        HWND combo = GetDlgItem(HWindow, IDC_ITEMS);
        int i;
        for (i = 0; i < 4; i++)
        {
            ColorButtons[i][LINENUM_COLOR_BTN] = SalGUI->AttachColorArrowButton(HWindow, LineNumIDs[i], TRUE);
            ColorButtons[i][TEXT_COLOR_BTN] = SalGUI->AttachColorArrowButton(HWindow, TextIDs[i], TRUE);
            // #pragma message(__FILE__ "(280) : Honzo, az tohle implementujes, tak nasledujici kod odkomentuj. Dik, Lukas.")
            // j.r. FIXME: zatim jsem na to nemel cas, hazim pragmu do komentare, at nestrasi v buildu
            // l.c., 13.2.2011 jen jsem na zkousku tu pragmu zas odkomentoval, estli jsi to nahodou mezitim nenakodil:)
            // j.r., 23.3.2011 nizka priorita, zatim nebudeme resit
            // ColorButtons[i][LINENUM_COLOR_BTN]->SetPalette(&HPalette, &UsePalette);
            // ColorButtons[i][TEXT_COLOR_BTN]->SetPalette(&HPalette, &UsePalette);
        }

        int j;
        for (j = 0; j < CD_NUMBER_OF_ITEMS; j++)
        {
            SendMessage(combo, CB_ADDSTRING, 0, (LPARAM)LoadStr(ColorItems[j].TextID));
        }
        SendMessage(combo, CB_SETCURSEL, 0, 0);
        ItemChanged(0);

        Preview1 = new CPreviewWindow(HWindow, IDS_PREVIEW1, TempColors, 0, HPalette);
        Preview2 = new CPreviewWindow(HWindow, IDS_PREVIEW2, TempColors, 1, HPalette);

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_ITEMS:
        {
            if (HIWORD(wParam) == CBN_SELCHANGE)
            {
                LRESULT i = SendMessage(GetDlgItem(HWindow, IDC_ITEMS), CB_GETCURSEL, 0, 0);
                if (i != CB_ERR)
                    ItemChanged(int(i));
                return 0;
            }
            return 0;
        }

        case IDB_LINENUM1:
        case IDB_LINENUM2:
        case IDB_LINENUM3:
        case IDB_LINENUM4:
        {
            LRESULT i = SendMessage(GetDlgItem(HWindow, IDC_ITEMS), CB_GETCURSEL, 0, 0);
            int colorFG = ColorItems[i].Buttons[LOWORD(wParam) - IDB_LINENUM1].ColorLineNumFG;
            int colorBK = ColorItems[i].Buttons[LOWORD(wParam) - IDB_LINENUM1].ColorLineNumBK;
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            ChooseColor(int(i), colorFG, colorBK, &r);

            return 0;
        }

        case IDB_TEXT1:
        case IDB_TEXT2:
        case IDB_TEXT3:
        case IDB_TEXT4:
        {
            LRESULT i = SendMessage(GetDlgItem(HWindow, IDC_ITEMS), CB_GETCURSEL, 0, 0);
            int colorFG = ColorItems[i].Buttons[LOWORD(wParam) - IDB_TEXT1].ColorTextFG;
            int colorBK = ColorItems[i].Buttons[LOWORD(wParam) - IDB_TEXT1].ColorTextBK;
            RECT r;
            GetWindowRect((HWND)lParam, &r);

            ChooseColor(int(i), colorFG, colorBK, &r);

            return 0;
        }

        case IDB_DEFAULT:
        {
            memcpy(TempColors, DefaultColors, sizeof(SALCOLOR) * NUMBER_OF_COLORS);
            LRESULT i = SendMessage(GetDlgItem(HWindow, IDC_ITEMS), CB_GETCURSEL, 0, 0);
            UpdateColors(int(i));
            return 0;
        }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (((NMHDR*)lParam)->code == PSN_SETACTIVE) // aktivace stranky
        {
            // nastavime font pro preview
            Preview1->SetFont(PageGeneral->GetCurrentFont());
            Preview2->SetFont(PageGeneral->GetCurrentFont());
            break;
        }
    }

    case WM_SYSCOLORCHANGE:
    {
        LRESULT i = SendMessage(GetDlgItem(HWindow, IDC_ITEMS), CB_GETCURSEL, 0, 0);
        UpdateColors(int(i));
        // #pragma message(__FILE__ "(375) : Honzo, az overis, ze se v CGUIColorArrowButton obnovuji pera automaticky, tak nasleduji komentar vymaz (nezamenovat s odkomentuj:). Dik Lukas")
        // j.r. FIXME: zatim jsem na to nemel cas, hazim pragmu do komentare, at nestrasi v buildu
        // l.c., 13.2.2011 jen jsem na zkousku tu pragmu zas odkomentoval, estli jsi to nahodou mezitim nenakodil:)
        // j.r., 23.3.2011 nizka priorita, zatim nebudeme resit
        // for (i = 0; i < 3; i++)
        // {
        //   ColorButtons[i][LINENUM_COLOR_BTN]->ColorsChanged();
        //   ColorButtons[i][TEXT_COLOR_BTN]->ColorsChanged();
        // }
        if (UsePalette)
        {
            PostMessage(HWindow, WM_QUERYNEWPALETTE, 0, 0);
        }
        break;
    }

    case WM_QUERYNEWPALETTE:
    {
        if (UsePalette)
        {
            HDC dc = GetDC(HWindow);
            HPALETTE oldPalette = SelectPalette(dc, HPalette, FALSE);
            RealizePalette(dc);
            SelectPalette(dc, oldPalette, FALSE);
            ReleaseDC(HWindow, dc);
            int i;
            for (i = 0; i < 3; i++)
            {
                HWND wnd = GetDlgItem(HWindow, LineNumIDs[i]);
                InvalidateRect(wnd, NULL, FALSE);
                UpdateWindow(wnd);
                wnd = GetDlgItem(HWindow, TextIDs[i]);
                InvalidateRect(wnd, NULL, FALSE);
                UpdateWindow(wnd);
                // ColorButtons[i][LINENUM_COLOR_BTN]->RePaint();
                // ColorButtons[i][TEXT_COLOR_BTN]->RePaint();
            }
            Preview1->RePaint();
            Preview2->RePaint();
            return TRUE;
        }
        return FALSE;
    }

    case WM_PALETTECHANGED:
    {
        if (UsePalette && (HWND)wParam != HWindow)
        {
            HDC dc = GetDC(HWindow);
            HPALETTE oldPalette = SelectPalette(dc, HPalette, FALSE);
            RealizePalette(dc);
            SelectPalette(dc, oldPalette, FALSE);
            ReleaseDC(HWindow, dc);

            int i;
            for (i = 0; i < 3; i++)
            {
                HWND wnd = GetDlgItem(HWindow, LineNumIDs[i]);
                InvalidateRect(wnd, NULL, FALSE);
                UpdateWindow(wnd);
                wnd = GetDlgItem(HWindow, TextIDs[i]);
                InvalidateRect(wnd, NULL, FALSE);
                UpdateWindow(wnd);
                // ColorButtons[i][LINENUM_COLOR_BTN]->RePaint();
                // ColorButtons[i][TEXT_COLOR_BTN]->RePaint();
            }
            Preview1->RePaint();
            Preview2->RePaint();
        }
    }
    }

    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPreviewWindow
//

struct CPreviewWindowScript
{
    int TextID;
    int LineNumFGColor;
    int LineNumBKColor;
    int TextColor;
    int BkgndColor;
    int SideSpecific;
};

#define PREVIEW_SCRIPT_LEN 7

CPreviewWindowScript PreviewScript[PREVIEW_SCRIPT_LEN] =
    {
        {IDS_COMMONTEXT, LINENUM_FG_NORMAL, LINENUM_BK_NORMAL, TEXT_FG_NORMAL, TEXT_BK_NORMAL, 0},
        {IDS_SELECTEDTEXT, LINENUM_FG_NORMAL, LINENUM_BK_NORMAL, TEXT_FG_SELECTION, TEXT_BK_SELECTION, 0},
        {IDS_CHANGEDTEXT, LINENUM_FG_LEFT_CHANGE, LINENUM_BK_LEFT_CHANGE, TEXT_FG_LEFT_CHANGE, TEXT_BK_LEFT_CHANGE, 1},
        {IDS_COMMONTEXT, LINENUM_FG_NORMAL, LINENUM_BK_NORMAL, TEXT_FG_NORMAL, TEXT_BK_NORMAL, 0},
        {IDS_FOCUSEDCHANGE2, LINENUM_FG_LEFT_CHANGE_FOCUSED, LINENUM_BK_LEFT_CHANGE_FOCUSED, TEXT_FG_LEFT_CHANGE_FOCUSED, TEXT_BK_LEFT_CHANGE_FOCUSED, 1},
        {IDS_COMMONFOCUSED2, LINENUM_FG_LEFT_CHANGE_FOCUSED, LINENUM_BK_LEFT_CHANGE_FOCUSED, TEXT_FG_LEFT_FOCUSED, TEXT_BK_LEFT_FOCUSED, 1},
        {IDS_COMMONTEXT, LINENUM_FG_NORMAL, LINENUM_BK_NORMAL, TEXT_FG_NORMAL, TEXT_BK_NORMAL, 0}};

CPreviewWindow::CPreviewWindow(HWND hDlg, int ctrlID, SALCOLOR* colors, int side,
                               HPALETTE& hPalette, CObjectOrigin origin)
    : CWindow(hDlg, ctrlID, origin), HPalette(hPalette)
{
    CALL_STACK_MESSAGE3("CPreviewWindow::CPreviewWindow(, %d, , %d, , )", ctrlID,
                        side);
    Colors = colors;
    Side = side;

    HFont = NULL;

    HLineNumBorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[LINENUM_LEFT_BORDER + Side]));
    HBorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[TEXT_LEFT_BORDER + Side]));
}

CPreviewWindow::~CPreviewWindow()
{
    CALL_STACK_MESSAGE1("CPreviewWindow::~CPreviewWindow()");
    if (HLineNumBorderPen)
        DeleteObject(HLineNumBorderPen);
    if (HBorderPen)
        DeleteObject(HBorderPen);
    if (HFont)
        DeleteObject(HFont);
}

void CPreviewWindow::ColorsChanged()
{
    CALL_STACK_MESSAGE1("CPreviewWindow::ColorsChanged()");
    if (HLineNumBorderPen)
        DeleteObject(HLineNumBorderPen);
    if (HBorderPen)
        DeleteObject(HBorderPen);
    HLineNumBorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[LINENUM_LEFT_BORDER + Side]));
    HBorderPen = CreatePen(PS_SOLID, 0, GetPALETTERGB(Colors[TEXT_LEFT_BORDER + Side]));
    RePaint();
}

void CPreviewWindow::RePaint()
{
    CALL_STACK_MESSAGE_NONE
    InvalidateRect(HWindow, NULL, FALSE);
    UpdateWindow(HWindow);
}

void CPreviewWindow::SetFont(LOGFONT* font)
{
    CALL_STACK_MESSAGE1("CPreviewWindow::SetFont()");
    // nacteme font
    if (HFont)
        DeleteObject(HFont);
    HDC hdc = GetDC(NULL);
    HFont = CreateFontIndirect(font);
    HFONT oldFont = (HFONT)SelectObject(hdc, HFont);
    TEXTMETRIC tm;
    GetTextMetrics(hdc, &tm);
    FontHeight = tm.tmHeight + tm.tmExternalLeading;
    FontWidth = tm.tmAveCharWidth;
    SelectObject(hdc, oldFont);
    ReleaseDC(NULL, hdc);
}

LRESULT
CPreviewWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPreviewWindow::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg,
                        wParam, lParam);
    switch (uMsg)
    {
    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(HWindow, &ps);
        HPALETTE oldPalette;
        if (UsePalette)
            oldPalette = SelectPalette(dc, HPalette, FALSE);
        HFONT oldFont = (HFONT)SelectObject(dc, HFont);

        RECT cr;
        GetClientRect(HWindow, &cr);

        IntersectClipRect(dc, cr.left, cr.top, cr.right, cr.bottom);

        RECT r1; // cislo radky
        r1.left = 0;
        r1.right = (2 + 1) * FontWidth + BORDER_WIDTH;
        r1.bottom = 0; // abychom meli pozdeji platna data, kdyby kreslici smycka vubec neprobehla
        RECT r2;       // vlastni radek textu
        r2.left = r1.right;
        r2.right = cr.right;
        int text_y;
        int side;
        HPEN hOldPen;

        int lineNum = 26;
        int i;
        for (i = 0; i < PREVIEW_SCRIPT_LEN; i++, lineNum++)
        {
            text_y = r1.top = r2.top = i * FontHeight;
            r1.bottom = r2.bottom = r1.top + FontHeight;
            side = PreviewScript[i].SideSpecific * Side;

            if (PreviewScript[i].TextColor == TEXT_FG_LEFT_CHANGE_FOCUSED ||
                i > 0 && PreviewScript[i - 1].TextColor == TEXT_FG_LEFT_FOCUSED)
            {
                // vykreslime cast ohraniceni aktivni difference na hornim okraji radku
                // hrana ve sloupci s cisly radek
                hOldPen = (HPEN)SelectObject(dc, HLineNumBorderPen);
                MoveToEx(dc, r1.left, r1.top, NULL);
                LineTo(dc, r1.right, r1.top);
                // hrana ve casti s textem souboru
                (HPEN) SelectObject(dc, HBorderPen);
                LineTo(dc, r2.right, r1.top);
                SelectObject(dc, hOldPen);
                r1.top++;
                r2.top++;
            }

            // nakreslime cislo radky
            char num[2 + 1];
            sprintf(num, "%*d", 2, lineNum);
            SetTextColor(dc, GetPALETTERGB(Colors[PreviewScript[i].LineNumFGColor + side]));
            SetBkColor(dc, GetPALETTERGB(Colors[PreviewScript[i].LineNumBKColor + side]));
            ExtTextOut(dc, BORDER_WIDTH, text_y, ETO_OPAQUE | ETO_CLIPPED, &r1, num, 2, NULL);

            // nakreslime text
            SetTextColor(dc, GetPALETTERGB(Colors[PreviewScript[i].TextColor + side]));
            SetBkColor(dc, GetPALETTERGB(Colors[PreviewScript[i].BkgndColor + side]));
            char* text = LoadStr(PreviewScript[i].TextID);
            ExtTextOut(dc, r2.left, text_y, ETO_OPAQUE | ETO_CLIPPED, &r2, text, UINT(strlen(text)), NULL);
        }

        if (r1.bottom < cr.bottom)
        {
            r1.top = r2.top = r1.bottom;
            r1.bottom = r2.bottom = cr.bottom;
            SetBkColor(dc, GetPALETTERGB(Colors[LINENUM_BK_NORMAL]));
            ExtTextOut(dc, 0, 0, ETO_OPAQUE | ETO_CLIPPED, &r1, NULL, 0, NULL);
            SetBkColor(dc, GetPALETTERGB(Colors[TEXT_BK_NORMAL]));
            ExtTextOut(dc, 0, 0, ETO_OPAQUE | ETO_CLIPPED, &r2, NULL, 0, NULL);
        }

        SelectObject(dc, oldFont);
        if (UsePalette)
            SelectPalette(dc, oldPalette, FALSE);
        EndPaint(HWindow, &ps);

        return 0;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CButton
//

// CButton::CButton(HWND hDlg, int ctrlID, CObjectOrigin origin, BOOL checkBox)
//  : CWindow(hDlg, ctrlID, origin)
// {
//   CALL_STACK_MESSAGE3("CButton::CButton(, %d, %d, )", ctrlID, checkBox);
//   CheckBox = checkBox;
//   Checked = FALSE;
//   Highlighted = FALSE;
//   DefPushButton = FALSE;
//   Captured = FALSE;
//   Space = FALSE;
//   GetClientRect(HWindow, &ClientRect);
//   HBitmap = NULL;
//   HCopyBitmap = NULL;
//   AllocateBitmap();
//   WndFramePen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_WINDOWFRAME));
//   BtnShadowPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
//   BtnHilightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHIGHLIGHT));
//   Btn3DLightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DLIGHT));
// }
//
// CButton::~CButton()
// {
//   CALL_STACK_MESSAGE1("CButton::~CButton()");
//   if (HBitmap != NULL) DeleteObject(HBitmap);
//   if (HCopyBitmap != NULL) DeleteObject(HCopyBitmap);
//   if (WndFramePen != NULL) DeleteObject(WndFramePen);
//   if (BtnShadowPen != NULL) DeleteObject(BtnShadowPen);
//   if (BtnHilightPen != NULL) DeleteObject(BtnHilightPen);
//   if (Btn3DLightPen != NULL) DeleteObject(Btn3DLightPen);
// }
//
// void
// CButton::AllocateBitmap()
// {
//   CALL_STACK_MESSAGE1("CButton::AllocateBitmap()");
//   HDC dc = GetDC(HWindow);
//   if (dc != NULL)
//   {
//     if (HBitmap != NULL) DeleteObject(HBitmap);
//     HBitmap = CreateCompatibleBitmap(dc, ClientRect.right, ClientRect.bottom);
//     ReleaseDC(HWindow, dc);
//   }
// }
//
// BOOL
// CButton::CreateCopyBitmap(HDC hMemoryDC, HBITMAP oldBitmap)
// {
//   CALL_STACK_MESSAGE1("CButton::CreateCopyBitmap()");
//   HDC hdc = GetDC(NULL);
//
//   if (HCopyBitmap != NULL && (CopyWidth != ClientRect.right || CopyHeight != ClientRect.bottom))
//   {
//     DeleteObject(HCopyBitmap);
//     HCopyBitmap = NULL;
//   }
//
//   if (HCopyBitmap == NULL)
//   {
//     CopyWidth = ClientRect.right;
//     CopyHeight = ClientRect.bottom;
//     HCopyBitmap = CreateCompatibleBitmap(hdc, CopyWidth, CopyHeight);
//   }
//
//   HDC hMemDC = CreateCompatibleDC(hdc);
//   HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemDC, HCopyBitmap);
//
//   HBITMAP hOld = (HBITMAP)SelectObject(hMemoryDC, oldBitmap);
//
//   HIMAGELIST hImageList = ImageList_Create(CopyWidth, CopyHeight, SG->GetImageListColorFlags() | ILC_MASK, 1, 0);
//   ImageList_AddMasked(hImageList, HBitmap, RGB(192, 192, 192));
//   RECT r;
//   r.left = 0;
//   r.top = 0;
//   r.right = CopyWidth;
//   r.bottom = CopyHeight;
//   FillRect(hMemDC, &r, (HBRUSH)GetStockObject(WHITE_BRUSH));
//   ImageList_Draw(hImageList, 0, hMemDC, 0, 0, ILD_TRANSPARENT);
//   ImageList_Destroy(hImageList);
//
//   SelectObject(hMemoryDC, hOld);
//
//   SelectObject(hMemDC, hOldBitmap);
//   DeleteDC(hMemDC);
//
//   ReleaseDC(NULL, hdc);
//   return TRUE;
// }
//
// void
// CButton::ColorsChanged()
// {
//   CALL_STACK_MESSAGE1("CButton::ColorsChanged()");
//   if (WndFramePen != NULL) DeleteObject(WndFramePen);
//   if (BtnShadowPen != NULL) DeleteObject(BtnShadowPen);
//   if (BtnHilightPen != NULL) DeleteObject(BtnHilightPen);
//   if (Btn3DLightPen != NULL) DeleteObject(Btn3DLightPen);
//   WndFramePen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_WINDOWFRAME));
//   BtnShadowPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNSHADOW));
//   BtnHilightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_BTNHIGHLIGHT));
//   Btn3DLightPen = CreatePen(PS_SOLID, 0, GetSysColor(COLOR_3DLIGHT));
//   RePaint();
// }
//
// void
// CButton::RePaint()
// {
//   CALL_STACK_MESSAGE_NONE
//   InvalidateRect(HWindow, NULL, FALSE);
//   UpdateWindow(HWindow);
// }
//
// void
// CButton::NotifyParent(WORD notify)
// {
//   CALL_STACK_MESSAGE2("CButton::NotifyParent(0x%p)", notify);
//   int id = (int)GetMenu(HWindow);
//   PostMessage(GetParent(HWindow), WM_COMMAND,
//               (WPARAM)(id | notify >> 16), (LPARAM)HWindow);
// }
//
// LRESULT
// CButton::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
// {
//   CALL_STACK_MESSAGE4("CButton::WindowProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
//   switch (uMsg)
//   {
//     case WM_GETDLGCODE:
//     {
//       DWORD ret = DLGC_BUTTON;
//       if (DefPushButton)
//         ret |= DLGC_DEFPUSHBUTTON;
//       else
//         ret |= DLGC_UNDEFPUSHBUTTON;
//       return ret;
//     }
//
//     case WM_SETFOCUS:
//     {
//       RePaint();
//       if (GetWindowLong(HWindow, GWL_STYLE) & BS_NOTIFY) NotifyParent(BN_SETFOCUS);
//       return 0;
//     }
//
//     case WM_KILLFOCUS:
//     {
//       if (Captured)
//       {
//         ReleaseCapture();
//         Captured = FALSE;
//         Highlighted = FALSE;
//       }
//       if (Space)
//       {
//         Space = FALSE;
//         Highlighted = FALSE;
//       }
//       RePaint();
//       if (GetWindowLong(HWindow, GWL_STYLE) & BS_NOTIFY) NotifyParent(BN_KILLFOCUS);
//       return 0;
//     }
//
//     case WM_ENABLE:
//     {
//       RePaint();
//       return 0;
//     }
//
//     case WM_SIZE:
//     {
//       GetClientRect(HWindow, &ClientRect);
//       AllocateBitmap();
//       break;
//     }
//
//     case WM_PAINT:
//     {
//       PAINTSTRUCT ps;
//       BeginPaint(HWindow, &ps);
//
//       HDC dc = NULL;
//       BOOL memDC;
//       HBITMAP oldBmp;
//       if (HBitmap != NULL)
//       {
//         dc = CreateCompatibleDC(ps.hdc);
//         if (dc != NULL)
//         {
//           oldBmp = (HBITMAP)SelectObject(dc, HBitmap);
//           memDC = TRUE;
//         }
//       }
//       if (dc == NULL)
//       {
//         dc = ps.hdc;
//         memDC = FALSE;
//       }
//
//       BOOL enabled = IsWindowEnabled(HWindow);
//       BOOL down = enabled && (Highlighted || CheckBox && Checked);
//       if (enabled)
//       {
//         HBRUSH hBrush = (HBRUSH)(COLOR_BTNFACE + 1);
//         if (!Highlighted && CheckBox && Checked) hBrush = HDitheredBrush;
//         FillRect(dc, &ClientRect, hBrush);
//
//         RECT r = ClientRect;
//         r.left += 4;
//         r.top += 4;
//         r.right -= 4;
//         r.bottom -= 4;
//         if (down)
//         {
//           r.left++;
//           r.top++;
//           r.right++;
//           r.bottom++;
//         }
//         PaintFace(dc, &r);
//       }
//       else
//       {
//         FillRect(dc, &ClientRect, (HBRUSH)GetStockObject(WHITE_BRUSH));
//         RECT r = ClientRect;
//         r.left += 4;
//         r.top += 4;
//         r.right -= 4;
//         r.bottom -= 4;
//         PaintFace(dc, &r);
//         if (memDC)
//         {
//           CreateCopyBitmap(dc, oldBmp);
//           FillRect(dc, &ClientRect, (HBRUSH)(COLOR_BTNFACE + 1));
//           DrawState(dc, NULL, NULL, (LPARAM) HCopyBitmap, 0, 0, 0,
//                     0, 0, DST_BITMAP | DSS_DISABLED);
//         }
//       }
//
//       RECT clR = ClientRect;
//       if (DefPushButton)
//       {
//         HPEN hOldPen = (HPEN)SelectObject(dc, WndFramePen);
//         HBRUSH hOldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
//         Rectangle(dc, ClientRect.left, ClientRect.top, ClientRect.right, ClientRect.bottom);
//         SelectObject(dc, hOldBrush);
//         SelectObject(dc, hOldPen);
//         InflateRect(&clR, -1, -1);
//       }
//       if (!down)
//       {
//         clR.right--;
//         clR.bottom--;
//         // svetla vlevo a nahore
//         HPEN hOldPen = (HPEN)SelectObject(dc, BtnHilightPen);
//         MoveToEx(dc, clR.left, clR.bottom - 1, NULL);
//         LineTo(dc, clR.left, clR.top);
//         LineTo(dc, clR.right, clR.top);
//         // trochu tmavsi uvnitr
//         SelectObject(dc, Btn3DLightPen);
//         MoveToEx(dc, clR.left + 1, clR.bottom - 2, NULL);
//         LineTo(dc, clR.left + 1, clR.top + 1);
//         LineTo(dc, clR.right - 1, clR.top + 1);
//         // tmava vpravo a dole
//         SelectObject(dc, BtnShadowPen);
//         MoveToEx(dc, clR.right - 1, clR.top + 1, NULL);
//         LineTo(dc, clR.right - 1, clR.bottom - 1);
//         LineTo(dc, clR.left, clR.bottom - 1);
//         // nejtmavsi uplne venku
//         SelectObject(dc, WndFramePen);
//         MoveToEx(dc, clR.left, clR.bottom, NULL);
//         LineTo(dc, clR.right, clR.bottom);
//         LineTo(dc, clR.right, -1);
//         SelectObject(dc, hOldPen);
//       }
//       else
//       {
//         HPEN hOldPen = (HPEN)SelectObject(dc, BtnShadowPen);
//         HBRUSH hOldBrush = (HBRUSH)SelectObject(dc, GetStockObject(NULL_BRUSH));
//         Rectangle(dc, clR.left, clR.top, clR.right, clR.bottom);
//         SelectObject(dc, hOldBrush);
//         SelectObject(dc, hOldPen);
//       }
//       if (GetFocus() == HWindow)
//       {
//         RECT r = ClientRect;
//         InflateRect(&r, -4, -4);
//         if (down)
//         {
//           r.left++;
//           r.top++;
//           r.right++;
//           r.bottom++;
//         }
//         int oldColor = SetTextColor(dc, GetSysColor(COLOR_BTNFACE));
//         int oldBkColor = SetBkColor(dc, RGB(0, 0, 0));
//         DrawFocusRect(dc, &r);
//         SetTextColor(dc, oldBkColor);
//         SetTextColor(dc, oldColor);
//       }
//
//       if (memDC)
//       {
//         BitBlt(ps.hdc, 0, 0, ClientRect.right, ClientRect.bottom, dc, 0, 0, SRCCOPY);
//         SelectObject(dc, oldBmp);
//         DeleteDC(dc);
//       }
//
//       EndPaint(HWindow, &ps);
//       return 0;
//     }
//
//     case WM_KEYDOWN:
//     {
//       if ((int) wParam == VK_SPACE)
//       {
//         Space = TRUE;
//         Highlighted = TRUE;
//         RePaint();
//       }
//       else if (Space)
//       {
//         Space = FALSE;
//         Highlighted = FALSE;
//         RePaint();
//       }
//       return 0;
//     }
//
//     case WM_KEYUP:
//     {
//       if (Space && wParam == VK_SPACE)
//       {
//         Space = FALSE;
//         Highlighted = FALSE;
//         if (CheckBox) Checked = !Checked;
//         RePaint();
//         NotifyParent(BN_CLICKED);
//       }
//       return 0;
//     }
//
//     case WM_LBUTTONDBLCLK:
//     case WM_LBUTTONDOWN:
//     {
//       if (!Captured)
//       {
//         Highlighted = TRUE;
//         Captured = TRUE;
//         SetCapture(HWindow);
//         RePaint();
//         if (GetFocus() != HWindow) SetFocus(HWindow);
//       }
//       return 0;
//     }
//
//     case WM_LBUTTONUP:
//     {
//       if (Captured)
//       {
//         ReleaseCapture();
//         Captured = FALSE;
//         Highlighted = FALSE;
//
//         POINT p;
//         p.x = LOWORD(lParam);
//         p.y = HIWORD(lParam);
//         BOOL in = PtInRect(&ClientRect, p);
//         if (in && CheckBox) Checked = !Checked;
//         RePaint();
//         if (in) NotifyParent(BN_CLICKED);
//       }
//       return 0;
//     }
//
//     case WM_MOUSEMOVE:
//     {
//       if (Captured)
//       {
//         POINT p;
//         p.x = LOWORD(lParam);
//         p.y = HIWORD(lParam);
//         BOOL highlight;
//         if (PtInRect(&ClientRect, p)) highlight = TRUE;
//         else highlight = FALSE;
//         if (highlight != Highlighted)
//         {
//           Highlighted = highlight;
//           RePaint();
//         }
//       }
//       return 0;
//     }
//
//     case BM_SETSTATE:
//     {
//       BOOL highlight = (wParam != 0);
//       if (highlight != Highlighted)
//       {
//         Highlighted = highlight;
//         RePaint();
//       }
//       return 0;
//     }
//
//     case BM_GETSTATE:
//     {
//       int state = 0;
//       if (GetFocus() == HWindow) state |= BST_FOCUS;
//       if (Highlighted) state |= BST_PUSHED;
//       return state;
//     }
//
//     case BM_SETCHECK:
//     {
//       BOOL checked = (wParam == BST_CHECKED);
//       if (checked != Checked)
//       {
//         Checked = checked;
//         RePaint();
//       }
//     }
//
//     case BM_GETCHECK:
//     {
//       if (Checked) return BST_CHECKED;
//       return BST_UNCHECKED;
//     }
//
//     case BM_SETSTYLE:
//     {
//       WORD dwStyle = LOWORD(wParam);
//       BOOL fRedraw = LOWORD(lParam);
//       DefPushButton = FALSE;
//       if (dwStyle & BS_DEFPUSHBUTTON)
//         DefPushButton = TRUE;
//       if (fRedraw)
//         RePaint();
//       return 0;
//     }
//   }
//   return CWindow::WindowProc(uMsg, wParam, lParam);
// }

//****************************************************************************
//
// CTextArrowButton
//
// klasicky text, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

// CTextArrowButton::CTextArrowButton(HWND hDlg, int ctrlID, CObjectOrigin origin)
//  : CButton(hDlg, ctrlID, origin)
// {
// }
//
// void
// CTextArrowButton::PaintFace(HDC hdc, const RECT *rect)
// {
//   CALL_STACK_MESSAGE_NONE
//   RECT r = *rect;
//
//   // vytahnu text tlacitka
//   char buff[200];
//   GetWindowText(HWindow, buff, 199);
//
//   // vytahnu aktualni font
//   HFONT hFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
//
//   r.right -= 8;
//
//   HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
//   int oldBkMode = SetBkMode(hdc, TRANSPARENT);
//   int oldTextColor = SetTextColor(hdc, GetSysColor(COLOR_BTNTEXT));
//   RECT r2 = r;
//   r2.top --;
//   DrawText(hdc, buff, -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
//   SetTextColor(hdc, oldTextColor);
//   SetBkMode(hdc, oldBkMode);
//   SelectObject(hdc, hOldFont);
//
//   HICON hIcon = LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_BROWSE));
//   DrawIcon(hdc, r.right - 12, r.top + (r.bottom - r.top) / 2 - 15, hIcon);
// }

//****************************************************************************
//
// CColorArrowButton
//
// pozadi s textem, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

// CColorArrowButton::CColorArrowButton(HWND hDlg, int ctrlID, HPALETTE &hPalette, CObjectOrigin origin)
//  : CButton(hDlg, ctrlID, origin), HPalette(hPalette)
// {
//   CALL_STACK_MESSAGE_NONE
//   TextColor = RGB(0, 0, 0);
//   BkgndColor = RGB(255, 255, 255);
// }
//
//
// void
// CColorArrowButton::SetColor(COLORREF textColor, COLORREF bkgndColor)
// {
//   CALL_STACK_MESSAGE_NONE
//   TextColor = textColor;
//   BkgndColor = bkgndColor;
//   RePaint();
// }
//
//
// void
// CColorArrowButton::SetColor(COLORREF color)
// {
//   CALL_STACK_MESSAGE_NONE
//   SetColor(color, color);
// }
//
// void
// CColorArrowButton::SetTextColor(COLORREF textColor)
// {
//   CALL_STACK_MESSAGE_NONE
//   SetColor(textColor, BkgndColor);
// }
//
// void
// CColorArrowButton::SetBkgndColor(COLORREF bkgndColor)
// {
//   CALL_STACK_MESSAGE_NONE
//   SetColor(TextColor, bkgndColor);
// }
//
// void
// CColorArrowButton::PaintFace(HDC hdc, const RECT *rect)
// {
//   CALL_STACK_MESSAGE1("CColorArrowButton::PaintFace(, )");
//   HPALETTE oldPalette;
//   if (UsePalette) oldPalette = SelectPalette(hdc, HPalette, FALSE);
//
//   RECT r = *rect;
//
//   // vytahnu text tlacitka
//   char buff[200];
//   GetWindowText(HWindow, buff, 199);
//
//   // vytahnu aktualni font
//   HFONT hFont = (HFONT)SendMessage(HWindow, WM_GETFONT, 0, 0);
//
//   r.right -= 8;
//
//   HBRUSH hBrush = CreateSolidBrush(BkgndColor);
//   HPEN hOldPen = (HPEN)SelectObject(hdc, WndFramePen);
//   HBRUSH hOldBrush = (HBRUSH)SelectObject(hdc, hBrush);
//   Rectangle(hdc, r.left, r.top, r.right, r.bottom);
//   SelectObject(hdc, hOldBrush);
//   SelectObject(hdc, hOldPen);
//   DeleteObject(hBrush);
//
//   HFONT hOldFont = (HFONT)SelectObject(hdc, hFont);
//   int oldBkMode = SetBkMode(hdc, TRANSPARENT);
//   int oldTextColor = ::SetTextColor(hdc, TextColor);
//   DrawText(hdc, buff, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
//   ::SetTextColor(hdc, oldTextColor);
//   SetBkMode(hdc, oldBkMode);
//   SelectObject(hdc, hOldFont);
//
//   HICON hIcon = LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_BROWSE));
//   DrawIcon(hdc, rect->right - 19, r.top + (r.bottom - r.top) / 2 - 15, hIcon);
//
//   if (UsePalette) SelectPalette(hdc, oldPalette, FALSE);
// }
