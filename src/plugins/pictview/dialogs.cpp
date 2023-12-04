// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "lib/pvw32dll.h"
#include "renderer.h"
#include "pictview.h"
#include "dialogs.h"
#include "pictview.rh"
#include "pictview.rh2"
#include "lang\lang.rh"
#include "exif/exif.h"
#include "exif/libexif/exif-tag.h"
#include "utils.h"

// Enforces ANSI variant
#define ListView_SetItemTextA(hwndLV, i, iSubItem_, pszText_) \
    { \
        LV_ITEMA _ms_lvi; \
        _ms_lvi.iSubItem = iSubItem_; \
        _ms_lvi.pszText = pszText_; \
        SendMessageA((hwndLV), LVM_SETITEMTEXTA, (WPARAM)i, (LPARAM)(LV_ITEM FAR*)&_ms_lvi); \
    }

#define ListView_SetItemTextW(hwndLV, i, iSubItem_, pszText_) \
    { \
        LV_ITEMW _ms_lvi; \
        _ms_lvi.iSubItem = iSubItem_; \
        _ms_lvi.pszText = pszText_; \
        SendMessageW((hwndLV), LVM_SETITEMTEXTW, (WPARAM)i, (LPARAM)(LV_ITEM FAR*)&_ms_lvi); \
    }

//****************************************************************************
//
// CCommonDialog
//

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, hParent, origin)
{
}

CCommonDialog::CCommonDialog(HINSTANCE hInstance, int resID, int helpID, HWND hParent, CObjectOrigin origin)
    : CDialog(hInstance, resID, helpID, hParent, origin)
{
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc
    }
    }
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

// ****************************************************************************
//
// CCommonPropSheetPage
//

void CCommonPropSheetPage::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//****************************************************************************
//
// CAboutDialog
//

CAboutDialog::CAboutDialog(HWND hParent)
    : CCommonDialog(HLanguage, DLG_ABOUT, hParent)
{
}

INT_PTR
CAboutDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CAboutDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // dosadime aktualni verzi
        TCHAR buff[1000];
        TCHAR buff2[1000];

        SendDlgItemMessage(HWindow, IDC_ABOUT_ICON, STM_SETIMAGE, IMAGE_ICON, (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_WINDOW_ICON)));
        GetDlgItemText(HWindow, IDC_ABOUT_TITLE, buff, 1000);
        wsprintf(buff2, buff, VERSINFO_VERSION);
        SetDlgItemText(HWindow, IDC_ABOUT_TITLE, buff2);
        SetDlgItemText(HWindow, IDC_ABOUT_COPYRIGHT, VERSINFO_COPYRIGHT);

        // jmeno pluginu a verze budou tucne
        SalamanderGUI->AttachStaticText(HWindow, IDC_ABOUT_TITLE, STF_BOLD);

        // hyperlinky
        CGUIHyperLinkAbstract* hl;

        // zaridime promacknuti ramecku (nas stary resource workshop si neporadi s novejma stylama)
        HWND hChild = GetDlgItem(HWindow, IDC_ABOUT_FRAME);
        DWORD style = GetWindowLong(hChild, GWL_EXSTYLE);
        style |= WS_EX_CLIENTEDGE;
        SetWindowLong(hChild, GWL_EXSTYLE, style);
        RECT r;
        GetWindowRect(hChild, &r);
        SetWindowPos(hChild, NULL, 0, 0, r.right - r.left, r.bottom - r.top + 1, SWP_NOMOVE | SWP_NOZORDER);
        SetWindowPos(hChild, NULL, 0, 0, r.right - r.left, r.bottom - r.top, SWP_NOMOVE | SWP_NOZORDER);

        GetDlgItemText(HWindow, IDC_ABOUT_PVW32, buff, 1000);
        wsprintf(buff2, buff, PVW32DLL.Version);
        SetDlgItemText(HWindow, IDC_ABOUT_PVW32, buff2);
        // PVW32 bude tucne
        SalamanderGUI->AttachStaticText(HWindow, IDC_ABOUT_PVW32, STF_BOLD);

        hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_ABOUT_EMAIL, STF_UNDERLINE | STF_HYPERLINK_COLOR);
        if (hl != NULL)
            hl->SetActionOpen("mailto:support@pictview.com");

        hl = SalamanderGUI->AttachHyperLink(HWindow, IDC_ABOUT_WWW, STF_UNDERLINE | STF_HYPERLINK_COLOR);
        if (hl != NULL)
            hl->SetActionOpen("http://www.pictview.com/salamander");

        break;
    }
    case WM_COMMAND:
        break;
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CImgPropDialog
//

CImgPropDialog::CImgPropDialog(HWND hParent, const PVImageInfo* pvii, const char* comment, int frames)
    : CCommonDialog(HLanguage, DLG_IMGPROP, DLG_IMGPROP, hParent)
{
    PVII = pvii;
    Comment = comment;
    nFrames = frames;
}

INT_PTR
CImgPropDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CImgPropDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        TCHAR buffer[96];
        const TCHAR* s;
        TCHAR fmt[32];

        if (nFrames > 0)
        {
            SetDlgItemText(HWindow, IDC_IMGPROP_PAGENUM_LBL, LoadStr(IDS_FRAMES));
            _stprintf(buffer, _T("%d"), nFrames);
        }
        else
        {
            _stprintf(buffer, LoadStr(IDS_OF), PVII->CurrentImage + 1, PVII->NumOfImages);
        }
        SetDlgItemText(HWindow, IDC_IMGPROP_PAGENUM, buffer);
        CQuadWord qW(PVII->Width, 0);
        SalamanderGeneral->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_PIXELS), 1, &qW);
        _stprintf(buffer, fmt, PVII->Width);
        SetDlgItemText(HWindow, IDC_IMGPROP_WIDTH, buffer);
        CQuadWord qH(PVII->Height, 0);
        SalamanderGeneral->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_PIXELS), 1, &qH);
        _stprintf(buffer, fmt, PVII->Height);
        SetDlgItemText(HWindow, IDC_IMGPROP_HEIGHT, buffer);

        if (PVII->TotalBitDepth > 0)
        {
            s = buffer;
            if (PVII->ColorModel == PVCM_GRAYS)
            {
                _stprintf(buffer, LoadStr(IDS_IMGPROP_GRAY_NBIT), PVII->TotalBitDepth);
            }
            else
            {
                _stprintf(buffer, _T("TrueColor %uBit"), PVII->TotalBitDepth);
            }
        }
        else
            switch (PVII->Colors)
            {
            case PV_COLOR_HC15:
                s = _T("HiColor 15Bit");
                break;
            case PV_COLOR_HC16:
                s = _T("HiColor 16Bit");
                break;
            case PV_COLOR_TC24:
                s = _T("TrueColor 24Bit");
                break;
            case PV_COLOR_TC32:
                s = _T("TrueColor 32Bit");
                break;
            default:
                _stprintf(buffer, _T("%u"), PVII->Colors);
                s = buffer;
            }
        if (PVII->ColorModel == PVCM_CMYK)
        {
            s = _T("CMYK");
        }
        else if (PVII->ColorModel == PVCM_CIELAB)
        {
            s = _T("CIE Lab");
        }
        Static_SetText(GetDlgItem(HWindow, IDC_IMGPROP_COLORS), s);

        CQuadWord q1(PVII->BytesPerLine * PVII->Height, 0);
        SalamanderGeneral->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_BYTES), 1, &q1);
        _stprintf(buffer, fmt, PVII->BytesPerLine * PVII->Height);
        Static_SetText(GetDlgItem(HWindow, IDC_IMGPROP_SIZE), buffer);
        CQuadWord qFS(PVII->FileSize, 0);
        SalamanderGeneral->ExpandPluralString(fmt, sizeof(fmt), LoadStr(IDS_BYTES), 1, &qFS);
        _stprintf(buffer, fmt, PVII->FileSize);
        Static_SetText(GetDlgItem(HWindow, IDC_IMGPROP_FSIZE), buffer);

        if (PVII->VerDPI)
        {
            _stprintf(buffer, _T("%ux%u"), PVII->HorDPI, PVII->VerDPI);
            s = buffer;
        }
        else
        {
            s = LoadStr(IDS_UNKNOWN);
        }
        SetDlgItemText(HWindow, IDC_IMGPROP_DPI, s);

        SetDlgItemTextA(HWindow, IDC_IMGPROP_FMT, PVII->Info1); //PVW32DLL.PVGetErrorText(PVII->Format));
        SetDlgItemTextA(HWindow, IDC_IMGPROP_COMPR, PVW32DLL.PVGetErrorText(PVII->Compression));
        if (Comment)
        {
            LPSTR s2 = (char*)malloc(strlen(PVII->Info2) + 2 + strlen(Comment) + 1);
            if (s2)
            {
                sprintf(s2, "%s%s%s", PVII->Info2, *PVII->Info2 ? "\r\n" : "", Comment);
                SetDlgItemTextA(HWindow, IDC_IMGPROP_COMMENT, s2);
                free(s2);
            }
        }
        else
        {
            SetDlgItemTextA(HWindow, IDC_IMGPROP_COMMENT, PVII->Info2);
        }

        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CConfigPageAppearance
//

CConfigPageAppearance::CConfigPageAppearance()
    : CCommonPropSheetPage(NULL, HLanguage, DLG_CFGPAGE_APPEARANCE, DLG_CFGPAGE_APPEARANCE, PSP_HASHELP, NULL)
{
}

void CConfigPageAppearance::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_CFGAPP_WINPOS_SAME, WINDOW_POS_SAME, G.WindowPos);
    ti.RadioButton(IDC_CFGAPP_WINPOS_LARGER, WINDOW_POS_LARGER, G.WindowPos);
    ti.RadioButton(IDC_CFGAPP_WINPOS_ANY, WINDOW_POS_ANY, G.WindowPos);

    ti.RadioButton(IDC_CFGAPP_ZOOM_WHOLE, eShrinkToFit, (int&)G.ZoomType);
    ti.RadioButton(IDC_CFGAPP_ZOOM_WIDTH, eShrinkToWidth, (int&)G.ZoomType);
    ti.RadioButton(IDC_CFGAPP_ZOOM_ORIG, eZoomOriginal, (int&)G.ZoomType);
    ti.RadioButton(IDC_CFGAPP_ZOOM_FSCREEN, eZoomFullScreen, (int&)G.ZoomType);

    ti.CheckBox(IDC_CFGAPP_PATH, G.bShowPathInTitle);
}

//****************************************************************************
//
// CConfigPageColors
//

CConfigPageColors::CConfigPageColors()
    : CCommonPropSheetPage(NULL, HLanguage, DLG_CFGPAGE_COLORS, DLG_CFGPAGE_COLORS, PSP_HASHELP, NULL)
{
    ButtonBackground = NULL;
    ButtonTransparent = NULL;
    ButtonFSBackground = NULL;
    ButtonFSTransparent = NULL;
}

void CConfigPageColors::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
        memcpy(G.Colors, Colors, sizeof(Colors));
}

INT_PTR
CConfigPageColors::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        memcpy(Colors, G.Colors, sizeof(Colors));

        ButtonBackground = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CFGCLR_BORDER, TRUE);
        if (ButtonBackground == NULL)
            return FALSE;
        ButtonBackground->SetColor(GetCOLORREF(Colors[vceBackground]), GetCOLORREF(Colors[vceBackground]));

        ButtonTransparent = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CFGCLR_BKGND, TRUE);
        if (ButtonTransparent == NULL)
            return FALSE;
        ButtonTransparent->SetColor(GetCOLORREF(Colors[vceTransparent]), GetCOLORREF(Colors[vceTransparent]));

        ButtonFSBackground = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CFGCLR_FS_BORDER, TRUE);
        if (ButtonFSBackground == NULL)
            return FALSE;
        ButtonFSBackground->SetColor(GetCOLORREF(Colors[vceFSBackground]), GetCOLORREF(Colors[vceFSBackground]));

        ButtonFSTransparent = SalamanderGUI->AttachColorArrowButton(HWindow, IDC_CFGCLR_FS_BKGND, TRUE);
        if (ButtonFSTransparent == NULL)
            return FALSE;
        ButtonFSTransparent->SetColor(GetCOLORREF(Colors[vceFSTransparent]), GetCOLORREF(Colors[vceFSTransparent]));

        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDC_CFGCLR_BORDER:
        case IDC_CFGCLR_BKGND:
        case IDC_CFGCLR_FS_BORDER:
        case IDC_CFGCLR_FS_BKGND:
        {
            SALCOLOR* clr;
            CGUIColorArrowButtonAbstract* button;
            if (LOWORD(wParam) == IDC_CFGCLR_BORDER)
            {
                clr = &Colors[vceBackground];
                button = ButtonBackground;
            }
            else if (LOWORD(wParam) == IDC_CFGCLR_BKGND)
            {
                clr = &Colors[vceTransparent];
                button = ButtonTransparent;
            }
            else if (LOWORD(wParam) == IDC_CFGCLR_FS_BORDER)
            {
                clr = &Colors[vceFSBackground];
                button = ButtonFSBackground;
            }
            else
            {
                clr = &Colors[vceFSTransparent];
                button = ButtonFSTransparent;
            }

            BOOL autoColor = GetFValue(*clr) & SCF_DEFAULT;
            CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
            if (popup != NULL)
            {
                MENU_ITEM_INFO mii;
                mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_STATE;
                mii.Type = MENU_TYPE_STRING | MENU_TYPE_RADIOCHECK;

                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertItem() dole...
MENU_TEMPLATE_ITEM ConfigPageColorsMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_CUSTOM_COLOR
  {MNTT_IT, IDS_AUTOMATIC_COLOR
  {MNTT_PE, 0
};
*/
                mii.String = LoadStr(IDS_CUSTOM_COLOR);
                mii.ID = 1;
                mii.State = 0;
                if (!autoColor)
                    mii.State = MENU_STATE_CHECKED;
                popup->InsertItem(-1, TRUE, &mii);

                mii.String = LoadStr(IDS_AUTOMATIC_COLOR);
                mii.ID = 2;
                mii.State = 0;
                if (autoColor)
                    mii.State = MENU_STATE_CHECKED;
                popup->InsertItem(-1, TRUE, &mii);

                RECT r;
                GetWindowRect(GetDlgItem(HWindow, LOWORD(wParam)), &r);

                DWORD flags = MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_SELECT;
                popup->SetSelectedItemIndex(0);
                DWORD cmd = popup->Track(flags, r.right, r.top, HWindow, &r);
                SalamanderGUI->DestroyMenuPopup(popup);

                if (cmd != 0)
                {
                    if (cmd == 1)
                    {
                        static COLORREF customColors[16];
                        CHOOSECOLOR cc;
                        ZeroMemory(&cc, sizeof(cc));
                        cc.lStructSize = sizeof(cc);
                        cc.hwndOwner = HWindow;
                        cc.lpCustColors = (LPDWORD)customColors;
                        cc.rgbResult = GetCOLORREF(*clr);
                        cc.Flags = CC_RGBINIT | CC_FULLOPEN | CC_SOLIDCOLOR;
                        if (ChooseColor(&cc) == TRUE)
                        {
                            *clr = 0;
                            SetRGBPart(clr, cc.rgbResult);
                        }
                    }
                    else
                        *clr = RGBF(0, 0, 0, SCF_DEFAULT);
                    if (HWindow != NULL) // pri zavreni z jineho threadu (shutdown / zavreni hl. okna Salama) uz dialog neexistuje
                    {
                        RebuildColors(Colors);
                        button->SetColor(GetCOLORREF(*clr), GetCOLORREF(*clr));
                    }
                }
            }
            return 0;
        }
        }
        break;
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CConfigPageKeyboard
//

CConfigPageKeyboard::CConfigPageKeyboard()
    : CCommonPropSheetPage(NULL, HLanguage, DLG_CFGPAGE_KEYBOARD, DLG_CFGPAGE_KEYBOARD, PSP_HASHELP, NULL)
{
}

void CConfigPageKeyboard::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_CFGKBD_PGDNUP_SCR, TRUE, G.PageDnUpScrolls);
    ti.RadioButton(IDC_CFGKBD_PGDNUP_PREV, FALSE, G.PageDnUpScrolls);
}

//****************************************************************************
//
// CConfigPageTools
//

CConfigPageTools::CConfigPageTools()
    : CCommonPropSheetPage(NULL, HLanguage, DLG_CFGPAGE_TOOLS, DLG_CFGPAGE_TOOLS, PSP_HASHELP, NULL)
{
}

void CConfigPageTools::Transfer(CTransferInfo& ti)
{
    TCHAR buf[8];
    int ratio = 5; // "Other"

    if ((G.SelectRatioX == 1) && (G.SelectRatioY == 1))
    {
        ratio = 1;
    }
    else if ((G.SelectRatioX == 4) && (G.SelectRatioY == 3))
    {
        ratio = 2;
    }
    else if ((G.SelectRatioX == 3) && (G.SelectRatioY == 2))
    {
        ratio = 3;
    }
    else if ((G.SelectRatioX == 16) && (G.SelectRatioY == 9))
    {
        ratio = 4;
    }
    ti.RadioButton(IDC_CFGTOOLS_SQUARE, 1, ratio);
    ti.RadioButton(IDC_CFGTOOLS_4_3, 2, ratio);
    ti.RadioButton(IDC_CFGTOOLS_3_2, 3, ratio);
    ti.RadioButton(IDC_CFGTOOLS_16_9, 4, ratio);
    ti.RadioButton(IDC_CFGTOOLS_OTHER, 5, ratio);

    switch (ratio)
    {
    case 1:
        G.SelectRatioX = G.SelectRatioY = 1;
        break;
    case 2:
        G.SelectRatioX = 4;
        G.SelectRatioY = 3;
        break;
    case 3:
        G.SelectRatioX = 3;
        G.SelectRatioY = 2;
        break;
    case 4:
        G.SelectRatioX = 16;
        G.SelectRatioY = 9;
        break;
    }
    _stprintf(buf, _T("%d"), G.SelectRatioX);
    ti.EditLine(IDC_CFGTOOLS_RATIO_X, buf, SizeOf(buf), FALSE);
    if (5 == ratio)
    {
        G.SelectRatioX = _ttoi(buf);
    }
    _stprintf(buf, _T("%d"), G.SelectRatioY);
    ti.EditLine(IDC_CFGTOOLS_RATIO_Y, buf, SizeOf(buf), FALSE);
    if (5 == ratio)
    {
        G.SelectRatioY = _ttoi(buf);
    }

    ti.CheckBox(IDC_CFGTOOLS_PIPHEX, G.PipetteInHex);

    if (ti.Type == ttDataToWindow)
    {
        EnableControls();
    }
}

void CConfigPageTools::Validate(CTransferInfo& ti)
{
    if (IsDlgButtonChecked(HWindow, IDC_CFGTOOLS_OTHER))
    {
        TCHAR buf[8];

        ti.EditLine(IDC_CFGTOOLS_RATIO_X, buf, SizeOf(buf), FALSE);
        if (_ttoi(buf) <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEPOSITIVE), LoadStr(IDS_ERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_CFGTOOLS_RATIO_X);
            return;
        }
        ti.EditLine(IDC_CFGTOOLS_RATIO_Y, buf, SizeOf(buf), FALSE);
        if (_ttoi(buf) <= 0)
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_MUSTBEPOSITIVE), LoadStr(IDS_ERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_CFGTOOLS_RATIO_Y);
        }
    }
}

void CConfigPageTools::EnableControls()
{
    BOOL bOther = IsDlgButtonChecked(HWindow, IDC_CFGTOOLS_OTHER) == BST_CHECKED;

    EnableWindow(GetDlgItem(HWindow, IDC_CFGTOOLS_RATIO_X), bOther);
    EnableWindow(GetDlgItem(HWindow, IDC_CFGTOOLS_RATIO_Y), bOther);
}

INT_PTR CConfigPageTools::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CConfigPageTools::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED)
        {
            WORD id = LOWORD(wParam);

            if ((id == IDC_CFGTOOLS_SQUARE) || (id == IDC_CFGTOOLS_4_3) || (id == IDC_CFGTOOLS_3_2) || (id == IDC_CFGTOOLS_16_9) || (id == IDC_CFGTOOLS_OTHER))
            {
                EnableControls();
            }
        }
        break;
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
//****************************************************************************
//
// CConfigPageAdvanced
//

CConfigPageAdvanced::CConfigPageAdvanced()
    : CCommonPropSheetPage(NULL, HLanguage, DLG_CFGPAGE_ADVANCED, DLG_CFGPAGE_ADVANCED, PSP_HASHELP, NULL)
{
}

void CConfigPageAdvanced::Transfer(CTransferInfo& ti)
{
    TCHAR buf[8];
    int saveVal;

    ti.CheckBox(IDC_CFGADV_CREATE_THUMBNAILS, G.IgnoreThumbnails);
    _stprintf(buf, _T("%u"), G.MaxThumbImgSize);
    ti.EditLine(IDC_CFGADV_THUMB_MP, buf, SizeOf(buf), FALSE);
    G.MaxThumbImgSize = _ttoi(buf);

    ti.CheckBox(IDC_CFGADV_AUTOROTATE, G.AutoRotate);
    saveVal = (G.DontShowAnymore & DSA_SAVE_SUCCESS) ? FALSE : TRUE;
    ti.CheckBox(IDC_CFGADV_SAVE, saveVal);
    G.DontShowAnymore &= ~DSA_SAVE_SUCCESS;
    if (!saveVal)
        G.DontShowAnymore |= DSA_SAVE_SUCCESS;
    ti.CheckBox(IDC_CFGADV_REMEMBER_PATH, G.Save.RememberPath);
}

//****************************************************************************
//
// CConfigDialog
//

// pomocny objekt pro centrovani konfiguracniho dialogu k parentovi
class CCenteredPropertyWindow : public CWindow
{
protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        switch (uMsg)
        {
        case WM_WINDOWPOSCHANGING:
        {
            WINDOWPOS* pos = (WINDOWPOS*)lParam;
            if (pos->flags & SWP_SHOWWINDOW)
            {
                HWND hParent = GetParent(HWindow);
                if (hParent != NULL)
                    SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);
            }
            break;
        }

        case WM_USER_CFGDLGDETACH: // mame se odpojit od dialogu (uz je vycentrovano)
        {
            DetachWindow();
            delete this; // trochu prasarna, ale uz se 'this' nikdo ani nedotkne, takze pohoda
            return 0;
        }
        }
        return CWindow::WindowProc(uMsg, wParam, lParam);
    }
};

#ifndef LPDLGTEMPLATEEX
#include <pshpack1.h>
typedef struct DLGTEMPLATEEX
{
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
} DLGTEMPLATEEX, *LPDLGTEMPLATEEX;
#include <poppack.h>
#endif // LPDLGTEMPLATEEX

// pomocny call-back pro centrovani konfiguracniho dialogu k parentovi a vyhozeni '?' buttonku z captionu
int CALLBACK CenterCallback(HWND HWindow, UINT uMsg, LPARAM lParam)
{
    if (uMsg == PSCB_INITIALIZED) // pripojime se na dialog
    {
        CCenteredPropertyWindow* wnd = new CCenteredPropertyWindow;
        if (wnd != NULL)
        {
            wnd->AttachToWindow(HWindow);
            if (wnd->HWindow == NULL)
                delete wnd; // okno neni pripojeny, zrusime ho uz tady
            else
            {
                PostMessage(wnd->HWindow, WM_USER_CFGDLGDETACH, 0, 0); // pro odpojeni CCenteredPropertyWindow od dialogu
            }
        }
    }
    if (uMsg == PSCB_PRECREATE) // odstraneni '?' buttonku z headeru property sheetu
    {
        // Remove the DS_CONTEXTHELP style from the dialog box template
        if (((LPDLGTEMPLATEEX)lParam)->signature == 0xFFFF)
            ((LPDLGTEMPLATEEX)lParam)->style &= ~DS_CONTEXTHELP;
        else
            ((LPDLGTEMPLATE)lParam)->style &= ~DS_CONTEXTHELP;
    }
    return 0;
}

CConfigDialog::CConfigDialog(HWND parent)
    : CPropertyDialog(parent, HLanguage, LoadStr(IDS_CONFIGURATION_TITLE),
                      G.LastCfgPage, PSH_USECALLBACK | PSH_NOAPPLYNOW | PSH_HASHELP,
                      NULL, &G.LastCfgPage, CenterCallback)
{
    Add(&PageAppearance);
    Add(&PageColors);
    Add(&PageKeyboard);
    Add(&PageTools);
    Add(&PageAdvanced);
}

//****************************************************************************
//
// CZoomDialog
//

CZoomDialog::CZoomDialog(HWND hParent, int* zoom)
    : CCommonDialog(HLanguage, DLG_ZOOM, hParent)
{
    Zoom = zoom;
}

void TrailZeros(LPTSTR buff)
{
    LPTSTR s = buff + _tcslen(buff) - 1;
    while (s > buff && *s == '0')
    {
        *s = 0;
        s--;
    }
    if (*s == '.')
        *s = 0;
}

void CZoomDialog::Transfer(CTransferInfo& ti)
{
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_ZOOM_ZOOM))
    {
        if (ti.Type == ttDataToWindow)
        {
            TCHAR buff[10];
            int i;
            for (i = 0; PredefinedZooms[i] != -1; i++)
            {
                double valDouble = (double)PredefinedZooms[i] / 100.0;
                _stprintf(buff, _T("%1.2f"), valDouble);
                TrailZeros(buff);
                SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)buff);
            }
            // precisions of two digits, but we do not show trailing zeros
            _stprintf(buff, _T("%1.2f"), /*(double)*Zoom*/ ((double)*Zoom) / (ZOOM_SCALE_FACTOR / 100));
            TrailZeros(buff);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buff);
        }
        else
        {
            TCHAR buff[10];
            GetWindowText(hWnd, buff, SizeOf(buff));
            if (*buff)
            {
                // ignore empty input
                *Zoom = (int)(_tcstod(buff, NULL) * (ZOOM_SCALE_FACTOR / 100) /*100.0*/);
            }
        }
    }
}

void CleanNonNumericChars(HWND hWnd, BOOL bComboBox, BOOL bKeepSeparator, BOOL bAllowMinus)
{
    TCHAR buffer[16], *s = buffer;
    BOOL bChanged = FALSE;
    DWORD dwStart, dwEnd;

    GetWindowText(hWnd, buffer, SizeOf(buffer));
    SendMessage(hWnd, bComboBox ? CB_GETEDITSEL : EM_GETSEL,
                (WPARAM)&dwStart, (LPARAM)&dwEnd);
    if (dwStart >= dwEnd)
    {
        dwEnd = -1;
    }
    while (*s)
    {
        if (((*s < '0') || (*s > '9')) && (((*s != '.') && (*s != ',')) || !bKeepSeparator) && ((*s != '-') || !bAllowMinus))
        {
            _tcscpy(s, s + 1);
            if (s - buffer <= (int)dwEnd)
            {
                dwEnd--;
            }
            if (s - buffer <= (int)dwStart)
            {
                dwStart--;
            }
            bChanged = TRUE;
        }
        else
        {
            if ((*s == '.') || (*s == ','))
            {
                // Don't allow more than 1 separator
                bKeepSeparator = FALSE;
            }
            // Don't allow the minus sign on non-first position
            bAllowMinus = FALSE;
            s++;
        }
    }
    if (bChanged)
    {
        SetWindowText(hWnd, buffer);
        if ((int)dwEnd >= 0)
        {
            SendMessage(hWnd, bComboBox ? CB_SETEDITSEL : EM_SETSEL, 0, MAKELPARAM(dwStart, dwEnd));
        }
    }
}

INT_PTR
CZoomDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CZoomDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(HWindow, IDC_ZOOM_ZOOM, CB_LIMITTEXT, 7, 0);
        break;
    }
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_EDITUPDATE)
        {
            // This notification is sent before screen is updated
            // we have to remove all non-numeric characters
            CleanNonNumericChars((HWND)lParam, TRUE, TRUE);
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPageDialog
//

CPageDialog::CPageDialog(HWND hParent, DWORD* page, DWORD numOfPages)
    : CCommonDialog(HLanguage, DLG_PAGE, hParent)
{
    Page = page;
    NumOfPages = numOfPages;
}

void CPageDialog::Transfer(CTransferInfo& ti)
{
    HWND hWnd;
    if (ti.GetControl(hWnd, IDC_PAGE_PAGE))
    {
        if (ti.Type == ttDataToWindow)
        {
            TCHAR buff[10];
            DWORD i;
            for (i = 0; i < NumOfPages; i++)
            {
                _stprintf(buff, _T("%u"), i + 1);
                SendMessage(hWnd, CB_ADDSTRING, 0, (LPARAM)buff);
            }
            _stprintf(buff, _T("%u"), (*Page) + 1);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)buff);
        }
        else
        {
            TCHAR buff[10];
            GetWindowText(hWnd, buff, SizeOf(buff));
            int page = _ttoi(buff) - 1;
            if (page < 0)
                page = 0;
            if (page >= (int)NumOfPages)
                page = NumOfPages - 1;
            *Page = page;
        }
    }
}

INT_PTR
CPageDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPageDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(HWindow, IDC_PAGE_PAGE, CB_LIMITTEXT, 5, 0);
        break;
    }
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_EDITUPDATE)
        {
            // This notification is sent before screen is updated
            // we have to remove all non-numeric characters
            CleanNonNumericChars((HWND)lParam, TRUE, FALSE);
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCaptureDialog
//

CCaptureDialog::CCaptureDialog(HWND hParent)
    : CCommonDialog(HLanguage, DLG_CAPTURE, DLG_CAPTURE, hParent)
{
}

void CCaptureDialog::Validate(CTransferInfo& ti)
{
    BOOL hotKeyChecked = IsDlgButtonChecked(HWindow, IDC_CAPTURE_HOTKEY) == BST_CHECKED;

    if (hotKeyChecked)
    {
        // zkusime hotkey registrovat pro nase okno; pokud to vyjde, pozdeji
        // by nemel byt problem ji registrovat pro okno vieweru
        WORD hotKey = (WORD)SendDlgItemMessage(HWindow, IDC_CAPTURE_HOTKEY_VAL, HKM_GETHOTKEY, 0, 0);
        if (hotKey != 0)
        {
            if (G.CaptureAtomID == 0)
                G.CaptureAtomID = GlobalAddAtom(_T("PictViewCapture"));
            if (G.CaptureAtomID == 0)
                ti.ErrorOn(IDC_CAPTURE_HOTKEY_VAL);
            else
            {
                if (RegisterHotKey(HWindow, G.CaptureAtomID, HIBYTE(hotKey), LOBYTE(hotKey)) == 0)
                {
                    SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_BADHOTKEY), LoadStr(IDS_ERRORTITLE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    ti.ErrorOn(IDC_CAPTURE_HOTKEY_VAL);
                }
                else
                    UnregisterHotKey(HWindow, G.CaptureAtomID);
            }
        }
        else
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_BADHOTKEY), LoadStr(IDS_ERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_CAPTURE_HOTKEY_VAL);
        }
    }
    else
    {
        int dummy;
        ti.EditLine(IDC_CAPTURE_TIMER_VAL, dummy);
        if (ti.IsGood() && (dummy < 1 || dummy > 360))
        {
            SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_BADTIMER), LoadStr(IDS_ERRORTITLE),
                                             MB_OK | MB_ICONEXCLAMATION);
            ti.ErrorOn(IDC_CAPTURE_TIMER_VAL);
        }
    }
}

void CCaptureDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        // pokud uzivatel nejede s vice monitory, zakazeme virtual screen a osetrime hodnotu
        if (!MultipleMonitors(NULL))
        {
            EnableWindow(GetDlgItem(HWindow, IDC_CAPTURE_VIRTUAL), FALSE);
            if (G.CaptureScope == CAPTURE_SCOPE_VIRTUAL)
                G.CaptureScope = CAPTURE_SCOPE_DESKTOP;
        }
    }
    ti.RadioButton(IDC_CAPTURE_FULL, CAPTURE_SCOPE_DESKTOP, G.CaptureScope);
    ti.RadioButton(IDC_CAPTURE_APPL, CAPTURE_SCOPE_APPL, G.CaptureScope);
    ti.RadioButton(IDC_CAPTURE_WINDOW, CAPTURE_SCOPE_WINDOW, G.CaptureScope);
    ti.RadioButton(IDC_CAPTURE_CLIENT, CAPTURE_SCOPE_CLIENT, G.CaptureScope);
    ti.RadioButton(IDC_CAPTURE_VIRTUAL, CAPTURE_SCOPE_VIRTUAL, G.CaptureScope);

    ti.RadioButton(IDC_CAPTURE_HOTKEY, CAPTURE_TRIGGER_HOTKEY, G.CaptureTrigger);
    ti.RadioButton(IDC_CAPTURE_TIMER, CAPTURE_TRIGGER_TIMER, G.CaptureTrigger);

    ti.CheckBox(IDC_CAPTURE_CURSOR, G.CaptureCursor);

    if (ti.Type == ttDataToWindow)
        SendDlgItemMessage(HWindow, IDC_CAPTURE_HOTKEY_VAL, HKM_SETHOTKEY, G.CaptureHotKey, 0);
    else
        G.CaptureHotKey = (WORD)SendDlgItemMessage(HWindow, IDC_CAPTURE_HOTKEY_VAL, HKM_GETHOTKEY, 0, 0);

    ti.EditLine(IDC_CAPTURE_TIMER_VAL, G.CaptureTimer);

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CCaptureDialog::EnableControls()
{
    BOOL hotKey = IsDlgButtonChecked(HWindow, IDC_CAPTURE_HOTKEY) == BST_CHECKED;
    EnableWindow(GetDlgItem(HWindow, IDC_CAPTURE_HOTKEY_VAL), hotKey);
    EnableWindow(GetDlgItem(HWindow, IDC_CAPTURE_TIMER_VAL), !hotKey);
}

LRESULT CALLBACK GetMsgProc(int nCode, WPARAM wParam, LPARAM lParam)
{
    MSG* lpmsg;

    lpmsg = (MSG*)lParam;
    if (nCode < 0 || !(IsChild(G.hHookedDlg, lpmsg->hwnd)))
        return (CallNextHookEx(G.hHook, nCode, wParam, lParam));

    switch (lpmsg->message)
    {
    case WM_MOUSEMOVE:
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
        if (G.hTTWnd != NULL)
        {
            MSG msg;

            msg.lParam = lpmsg->lParam;
            msg.wParam = lpmsg->wParam;
            msg.message = lpmsg->message;
            msg.hwnd = lpmsg->hwnd;
            SendMessage(G.hTTWnd, TTM_RELAYEVENT, 0, (LPARAM)&msg);
        }
        break;
    }
    return (CallNextHookEx(G.hHook, nCode, wParam, lParam));
} /* GetMsgProc */

INT_PTR
CCaptureDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCaptureDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        if (!G.hHookedDlg)
        {
            // Do not hook again if multiple capture windows are open.
            // In such case, only one instance will have tooltips.
            // This is smaller problem than handling multiple hooks
            // or crashing because of wrong unhooking would be.
            G.hHookedDlg = HWindow;
            G.hTTWnd = CreateWindowEx(0, TOOLTIPS_CLASS, NULL,
                                      TTS_ALWAYSTIP, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
                                      CW_USEDEFAULT, HWindow, (HMENU)NULL, DLLInstance, NULL);
            if (G.hTTWnd)
            {
                TOOLINFO ti;
                static TwoWords ids[] = {
                    {IDC_CAPTURE_APPL, IDS_CAPTHINT_APPL},
                    {IDC_CAPTURE_WINDOW, IDS_CAPTHINT_WINDOW},
                    {IDC_CAPTURE_CLIENT, IDS_CAPTHINT_CLIENT},
                    {0, 0}};
                TwoWords* pid = ids;

                memset(&ti, 0, sizeof(TOOLINFO));
                ti.cbSize = sizeof(TOOLINFO);
                ti.uFlags = TTF_IDISHWND;
                ti.hwnd = HWindow;
                ti.hinst = HLanguage;
                do
                {
                    ti.uId = (UINT_PTR)GetDlgItem(HWindow, pid[0][0]);
                    ti.lpszText = (LPTSTR)pid[0][1];
                    SendMessage(G.hTTWnd, TTM_ADDTOOL, 0, (LPARAM)&ti);
                } while ((++pid)[0][0]);
                G.hHook = SetWindowsHookEx(WH_GETMESSAGE, GetMsgProc, NULL, GetCurrentThreadId());
            }
        }
        break;

    case WM_DESTROY:
        UnhookWindowsHookEx(G.hHook);
        G.hHookedDlg = NULL;
        break;

    case WM_COMMAND:
        if (HIWORD(wParam) == BN_CLICKED &&
            (LOWORD(wParam) == IDC_CAPTURE_HOTKEY ||
             LOWORD(wParam) == IDC_CAPTURE_TIMER))
        {
            EnableControls();
        }
        break;
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CRenameDialog
//

CRenameDialog::CRenameDialog(HWND hParent, LPTSTR path, int pathBufSize)
    : CCommonDialog(HLanguage, IDD_RENAMEDIALOG, hParent)
{
    Path = path;
    PathBufSize = pathBufSize;
    bFirstShow = TRUE;
}

void CRenameDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CRenameDialog::Transfer()");
    ti.EditLine(IDE_PATH, Path, PathBufSize);
}

INT_PTR
CRenameDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->SetSubjectTruncatedText(GetDlgItem(HWindow, IDS_SUBJECT), LoadStr(IDS_RENAME_TO),
                                               Path, FALSE, FALSE);
        break;
    }

    case WM_SHOWWINDOW:
        if (bFirstShow)
        {
            // Select only base filename part, if configured
            if (!G.bSelectWhole)
            {
                LPCTSTR ext = _tcsrchr(Path, '.');
                if (ext == Path)
                    ext = NULL; // ".cvspass" is extension in Windows, but Explorer selects whole name in such cases, so we do the same
                SendDlgItemMessage(HWindow, IDE_PATH, EM_SETSEL, 0, _tcsclen(Path) - (ext ? _tcsclen(ext) : 0));
            }
            bFirstShow = FALSE;
        }
        break;
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// COverwriteDlg
//

COverwriteDlg::COverwriteDlg(HWND parent, LPCTSTR sourceName, LPCTSTR sourceAttr,
                             LPCTSTR targetName, LPCTSTR targetAttr) : CCommonDialog(HLanguage, IDD_OVERWRITE, parent)
{
    SourceName = sourceName;
    SourceAttr = sourceAttr;
    TargetName = targetName;
    TargetAttr = targetAttr;
}

INT_PTR
COverwriteDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("COverwriteDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SetWindowText(GetDlgItem(HWindow, IDS_SOURCENAME), SourceName);
        SetWindowText(GetDlgItem(HWindow, IDS_TARGETNAME), TargetName);

        SetWindowText(GetDlgItem(HWindow, IDS_SOURCEATTR), SourceAttr);
        SetWindowText(GetDlgItem(HWindow, IDS_TARGETATTR), TargetAttr);
        break;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDNO || LOWORD(wParam) == IDYES)
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

//****************************************************************************
//
// CExifDialog
//

CExifDialog::CExifDialog(HWND hParent, LPCTSTR fileName, int format)
    : CCommonDialog(HLanguage, DLG_IMGEXIF, DLG_IMGEXIF, hParent),
      Items(50, 20),
      Highlights(20, 10)
{
    FileName = fileName;
    DisableNotification = FALSE;
    Format = format;
}

CExifDialog::~CExifDialog()
{
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        CExifItem* item = &Items[i];
        if (item->TagTitle != NULL)
            SalamanderGeneral->Free(item->TagTitle);
        if (item->TagDescription != NULL)
            SalamanderGeneral->Free(item->TagDescription);
        if (item->Value != NULL)
            SalamanderGeneral->Free(item->Value);
    }
}

BOOL CALLBACK ExifEnumProc(DWORD tagNum, const char* tagTitle, const char* tagDescription,
                           const char* value, LPARAM lParam)
{
    CExifDialog* dlg = (CExifDialog*)lParam;
    CExifItem item;

    item.Tag = tagNum;
    item.TagTitle = SalamanderGeneral->DupStr(tagTitle);
    item.TagDescription = SalamanderGeneral->DupStr(tagDescription);
    item.Value = SalamanderGeneral->DupStr(value);
    if (item.TagTitle != NULL &&
        item.TagDescription != NULL &&
        item.Value != NULL)
    {
        return dlg->AddItem(&item);
    }
    if (item.TagTitle != NULL)
        SalamanderGeneral->Free(item.TagTitle);
    if (item.TagDescription != NULL)
        SalamanderGeneral->Free(item.TagDescription);
    if (item.Value != NULL)
        SalamanderGeneral->Free(item.Value);
    return TRUE;
}

BOOL CExifDialog::AddItem(CExifItem* item)
{
    int index = Items.Add(*item);
    if (!Items.IsGood())
    {
        Items.ResetState();
        return FALSE;
    }
    return TRUE;
}

void CExifDialog::InitLayout(int id[], int n, int m)
{
    CALL_STACK_MESSAGE3("CExifDialog::InitLayout( , %d, %d)", n, m);

    RECT client, rc, rc2;
    GetClientRect(HWindow, &client);
    GetWindowRect(HWindow, &rc);
    MinX = rc.right - rc.left;
    MinY = rc.bottom - rc.top;
    GetWindowRect(HListView, &rc);
    ListX = client.right - (rc.right - rc.left);
    ListY = client.bottom - (rc.bottom - rc.top);

    GetWindowRect(GetDlgItem(HWindow, IDC_IMGEXIF_INFO), &rc2);
    InfoHeight = rc2.bottom - rc2.top;
    InfoBorder = rc2.top - rc.bottom;
    ScreenToClient(HWindow, (LPPOINT)&rc2);
    InfoX = rc2.left;

    GetClientRect(HWindow, &client);
    int i;
    for (i = 0; i < n; i++)
    {
        GetWindowRect(GetDlgItem(HWindow, id[i]), &rc);
        ScreenToClient(HWindow, (LPPOINT)&rc);
        CtrlX[i] = (i < m) ? client.right - rc.left : rc.left;
        CtrlY[i] = client.bottom - rc.top;
    }
}

void CExifDialog::RecalcLayout(int cx, int cy, int id[], int n, int m)
{
    CALL_STACK_MESSAGE5("CExifDialog::RecalcLayout(%d, %d, , %d, %d)", cx, cy, n, m);

    SetWindowPos(HListView, NULL, 0, 0, cx - ListX, cy - ListY, SWP_NOZORDER | SWP_NOMOVE);
    SetWindowPos(GetDlgItem(HWindow, IDC_IMGEXIF_INFO), NULL,
                 InfoX, 5 + cy - ListY + InfoBorder, cx - ListX, InfoHeight, SWP_NOZORDER);
    int i;
    for (i = 0; i < n; i++)
        SetWindowPos(GetDlgItem(HWindow, id[i]), 0, (i < m) ? cx - CtrlX[i] : CtrlX[i],
                     cy - CtrlY[i], 0, 0, SWP_NOZORDER | SWP_NOSIZE);
}

void CExifDialog::InitListView()
{
    // naleju do listview sloupce Language a Path
    TCHAR buff[100];
    LVCOLUMN lvc;
    lvc.mask = LVCF_TEXT | LVCF_SUBITEM;
    lvc.pszText = buff;
    lvc.iSubItem = 0;
    _tcscpy(buff, LoadStr(IDS_EXIF_TAG));
    ListView_InsertColumn(HListView, 0, &lvc);

    lvc.iSubItem = 1;
    _tcscpy(buff, LoadStr(IDS_EXIF_VALUE));
    ListView_InsertColumn(HListView, 1, &lvc);
}

int CExifDialog::GetHighlightIndex(DWORD tag)
{
    int i;
    for (i = 0; i < Highlights.Count; i++)
        if (Highlights[i] == tag)
            return i;
    return -1;
}

void CExifDialog::ToggleHighlight(DWORD tag)
{
    int index = GetHighlightIndex(tag);
    if (index == -1)
    {
        Highlights.Add(tag);
        if (!Highlights.IsGood())
            Highlights.ResetState();
    }
    else
    {
        Highlights.Delete(index);
    }
}

void CExifDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
    {
        ExifHighlights.DetachMembers();
        int i;
        for (i = 0; i < Highlights.Count; i++)
            ExifHighlights.Add(Highlights[i]);
        if (!ExifHighlights.IsGood())
            ExifHighlights.ResetState();
        ExifGroupHighlights = IsDlgButtonChecked(HWindow, IDC_IMGEXIF_GROUP) == BST_CHECKED;
    }
}

void CExifDialog::FillListView()
{
    SendMessage(HListView, WM_SETREDRAW, FALSE, 0);

    int lastIndex = (int)GetFocusedItemLParam();
    ListView_DeleteAllItems(HListView);

    BOOL grouped = IsDlgButtonChecked(HWindow, IDC_IMGEXIF_GROUP) == BST_CHECKED;

    int index = 0;
    int focusIndex = 0;
    int j;
    for (j = 0; j < (grouped ? 2 : 1); j++)
    {
        int i;
        for (i = 0; i < Items.Count; i++)
        {
            CExifItem* item = &Items[i];
            int hghIndex = GetHighlightIndex(item->Tag);
            if (!grouped || (j == 0 && hghIndex != -1) || (j == 1 && hghIndex == -1))
            {
                // v prvni fazi (j==0) pridame highlighted polozky
                // v druhe fazi (j==1) pridame non-highlighted polozky
                LVITEM lvi;
                lvi.mask = LVIF_PARAM;
                lvi.iItem = index;
                lvi.iSubItem = 0;
                lvi.lParam = i;
                ListView_InsertItem(HListView, &lvi);
                ListView_SetItemTextA(HListView, index, 0, item->TagTitle);
                if ((item->Tag >= EXIF_TAG_XP_TITLE) && (item->Tag <= EXIF_TAG_XP_SUBJECT))
                {
                    wchar_t translW[1024];
                    static CONVERTUTF8TOUCS2 ConvertUTF8ToUCS2 = NULL;

                    if (!ConvertUTF8ToUCS2)
                    {
                        ConvertUTF8ToUCS2 = (CONVERTUTF8TOUCS2)GetProcAddress(EXIFLibrary, "ConvertUTF8ToUCS2");
                    }
                    if (ConvertUTF8ToUCS2)
                    {
                        ConvertUTF8ToUCS2(item->Value, translW);
                        // Note: MBCS is never longer than UTF8 representation -> result always fits & is NULL-terminated
                        //              WideCharToMultiByte(CP_ACP, 0, translW, -1, transl, BUF_SIZE, NULL, NULL);
                        ListView_SetItemTextW(HListView, index, 1, translW);
                    }
                }
                else
                {
                    ListView_SetItemTextA(HListView, index, 1, item->Value);
                }

                if (lvi.lParam == lastIndex)
                {
                    focusIndex = index;
                }
                index++;
            }
        }
    }

    DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_SetItemState(HListView, focusIndex, state, state);
    ListView_EnsureVisible(HListView, focusIndex, FALSE);

    SendMessage(HListView, WM_SETREDRAW, TRUE, 0);
}

LPARAM CExifDialog::GetFocusedItemLParam()
{
    int index = ListView_GetNextItem(HListView, -1, LVNI_FOCUSED);
    if (index < 0 || index >= Items.Count)
        return -1;
    LVITEM lvi;
    lvi.mask = LVIF_PARAM;
    lvi.iItem = index;
    lvi.iSubItem = 0;
    ListView_GetItem(HListView, &lvi);
    return lvi.lParam;
}

void FillContextMenuFromButtons(CGUIMenuPopupAbstract* popup, HWND hWindow, int* buttonsID)
{
    MENU_ITEM_INFO mii;
    TCHAR buff[1024];
    mii.String = buff;
    BOOL nextDefault = FALSE;
    while (*buttonsID != 0)
    {
        if (*buttonsID == -2)
        {
            nextDefault = TRUE;
        }
        else
        {
            if (*buttonsID == -1)
            {
                // separator
                mii.Mask = MENU_MASK_TYPE;
                mii.Type = MENU_TYPE_SEPARATOR;
            }
            else
            {
                // button
                mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_STATE;
                mii.Type = MENU_TYPE_STRING;
                mii.State = 0;
                mii.ID = *buttonsID;
                HWND hButton = GetDlgItem(hWindow, *buttonsID);
                GetWindowText(hButton, buff, SizeOf(buff));
                if (!IsWindowEnabled(hButton))
                    mii.State |= MENU_STATE_GRAYED;
            }
            if (nextDefault)
            {
                mii.State |= MENU_STATE_DEFAULT;
                nextDefault = FALSE;
            }
            popup->InsertItem(-1, TRUE, &mii);
        }
        buttonsID++;
    }
}

void CExifDialog::OnContextMenu(int x, int y)
{
    HWND hListView = HListView;
    DWORD selCount = ListView_GetSelectedCount(hListView);
    if (selCount < 1)
        return;

    // vytahame texty z tlacitek a naplnime context menu
    int ids[] = {-2, IDC_IMGEXIF_HIGHLIGHT, // -2 -> next item will be default
                 IDC_IMGEXIF_COPY,
                 -1, // -1 -> separator
                 IDC_IMGEXIF_GROUP,
                 0}; // 0 -> terminator
    CGUIMenuPopupAbstract* popup = SalamanderGUI->CreateMenuPopup();
    if (popup != NULL)
    {
        FillContextMenuFromButtons(popup, HWindow, ids);
        DWORD cmd = popup->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                 x, y, HWindow, NULL);
        SalamanderGUI->DestroyMenuPopup(popup);
        if (cmd != 0)
        {
            if (cmd == IDC_IMGEXIF_GROUP)
            {
                BOOL grouped = IsDlgButtonChecked(HWindow, IDC_IMGEXIF_GROUP) == BST_CHECKED;
                CheckDlgButton(HWindow, IDC_IMGEXIF_GROUP, grouped ? BST_UNCHECKED : BST_CHECKED);
            }
            PostMessage(HWindow, WM_COMMAND, MAKELPARAM(cmd, BN_CLICKED), 0);
        }
    }
}

void GetListViewContextMenuPos(HWND hListView, POINT* p)
{
    if (ListView_GetItemCount(hListView) == 0)
    {
        p->x = 0;
        p->y = 0;
        ClientToScreen(hListView, p);
        return;
    }
    int focIndex = ListView_GetNextItem(hListView, -1, LVNI_FOCUSED);
    if (focIndex != -1)
    {
        if ((ListView_GetItemState(hListView, focIndex, LVNI_SELECTED) & LVNI_SELECTED) == 0)
            focIndex = ListView_GetNextItem(hListView, -1, LVNI_SELECTED);
    }
    RECT cr;
    GetClientRect(hListView, &cr);
    RECT r;
    ListView_GetItemRect(hListView, 0, &r, LVIR_LABEL);
    p->x = r.left;
    if (p->x < 0)
        p->x = 0;
    if (focIndex != -1)
        ListView_GetItemRect(hListView, focIndex, &r, LVIR_BOUNDS);
    if (focIndex == -1 || r.bottom < 0 || r.bottom > cr.bottom)
        r.bottom = 0;
    p->y = r.bottom;
    ClientToScreen(hListView, p);
}

INT_PTR
CExifDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CImgPropDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    static int ids[] = {IDC_IMGEXIF_HIGHLIGHT, IDC_IMGEXIF_COPY, IDOK, IDCANCEL, IDHELP, IDC_IMGEXIF_GROUP};
    static unsigned char exifHdr[] = {'E', 'x', 'i', 'f', 0, 0};

    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        DisableNotification = TRUE;

        CALL_STACK_MESSAGE2("EXIFGetInfo() %s", FileName);
        CheckDlgButton(HWindow, IDC_IMGEXIF_GROUP, ExifGroupHighlights);
        int i;
        for (i = 0; i < ExifHighlights.Count; i++)
            Highlights.Add(ExifHighlights[i]);
        if (!Highlights.IsGood())
            Highlights.ResetState();

        HListView = GetDlgItem(HWindow, IDC_IMGEXIF_LIST);
        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        InitListView();

        InitLayout(ids, 6, 5);
        if (G.ExifDlgWidth < MinX)
            G.ExifDlgWidth = MinX;
        if (G.ExifDlgHeight < MinY)
            G.ExifDlgHeight = MinY;
        SetWindowPos(HWindow, NULL, 0, 0, G.ExifDlgWidth, G.ExifDlgHeight, SWP_NOZORDER | SWP_NOMOVE);
        SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);

        EXIFGETVERSION getVer = (EXIFGETVERSION)GetProcAddress(EXIFLibrary, "EXIFGetVersion");
        EXIFGETINFO getInfo = (EXIFGETINFO)GetProcAddress(EXIFLibrary, "EXIFGetInfo");
        if (getVer != NULL && getInfo != NULL)
        {
            DWORD exifVer = getVer();
            CALL_STACK_MESSAGE2("EXIFGetInfo() EXIF.DLL version %u", exifVer);
            if (Format == PVF_TIFF)
            {
#define MAX_EXIF_SIZE 60000 /* Pentax .PEF need nearly 60KB */
                char* buf = (char*)malloc(MAX_EXIF_SIZE + 6);
                FILE* f;
                int size;

                if (buf)
                {
                    f = _tfopen(FileName, _T("rb"));
                    if (f)
                    {
                        // fake APP1/EXIF marker
                        memcpy(buf, exifHdr, 6);
                        size = 6 + (int)fread(buf + 6, 1, MAX_EXIF_SIZE, f);
                        if ((((short*)buf)[4] == 'OR') || (((short*)buf)[4] == 'SR'))
                        {
                            // Olympus Digital Camera Raw *.ORF are TIFFs w/ screwed up header
                            ((short*)buf)[4] = 0x2A;
                        }
                        getInfo(buf, size, ExifEnumProc, (LPARAM)this);
                        fclose(f);
                    }
                    free(buf);
                }
            }
            else
            {
#ifdef _UNICODE
                char FileNameA[_MAX_PATH];

                WideCharToMultiByte(CP_ACP, 0, FileName, -1, FileNameA, sizeof(FileNameA), NULL, NULL);
                FileNameA[sizeof(FileNameA) - 1] = 0;
                getInfo(FileNameA, 0, ExifEnumProc, (LPARAM)this);
#else
                getInfo(FileName, 0, ExifEnumProc, (LPARAM)this);
#endif
            }
            DisableNotification = FALSE;
            FillListView();
            ListView_SetColumnWidth(HListView, 0, LVSCW_AUTOSIZE);
            ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
        }
        else
        {
            TRACE_E("Nenalezeny pozadovane exporty v EXIF.DLL");
        }
        break;
    }

    case WM_SIZE:
    {
        RecalcLayout(LOWORD(lParam), HIWORD(lParam), ids, 6, 5);
        break;
    }

    case WM_SIZING:
    {
        RECT* lprc = (RECT*)lParam;
        SIZE size = {lprc->right - lprc->left, lprc->bottom - lprc->top};
        if (size.cx < MinX)
        {
            if (wParam == WMSZ_LEFT || wParam == WMSZ_TOPLEFT || wParam == WMSZ_BOTTOMLEFT)
                lprc->left = lprc->right - MinX;
            else
                lprc->right = lprc->left + MinX;
        }
        if (size.cy < MinY)
        {
            if (wParam == WMSZ_TOP || wParam == WMSZ_TOPLEFT || wParam == WMSZ_TOPRIGHT)
                lprc->top = lprc->bottom - MinY;
            else
                lprc->bottom = lprc->top + MinY;
        }
        break;
    }

    case WM_COMMAND:
    {
        if (!DisableNotification && HIWORD(wParam) == BN_CLICKED)
        {
            switch (LOWORD(wParam))
            {
            case IDC_IMGEXIF_GROUP:
            {
                FillListView();
                return 0;
            }

            case IDC_IMGEXIF_HIGHLIGHT:
            {
                ToggleHighlight(Items[(int)GetFocusedItemLParam()].Tag);
                BOOL grouped = IsDlgButtonChecked(HWindow, IDC_IMGEXIF_GROUP) == BST_CHECKED;
                FillListView();
                return 0;
            }

            case IDC_IMGEXIF_COPY:
            {
                const char* text = Items[(int)GetFocusedItemLParam()].Value;
                SalamanderGeneral->CopyTextToClipboard(text, (int)strlen(text), FALSE, HWindow);
                return 0;
            }
            }
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (DisableNotification)
            break;
        if (wParam == IDC_IMGEXIF_LIST)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_CUSTOMDRAW:
            {
                LPNMLVCUSTOMDRAW cd = (LPNMLVCUSTOMDRAW)lParam;

                if (cd->nmcd.dwDrawStage == CDDS_PREPAINT)
                {
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NOTIFYITEMDRAW);
                    return TRUE;
                }

                if (cd->nmcd.dwDrawStage == CDDS_ITEMPREPAINT)
                {
                    // pozadame si o zaslani notifikace CDDS_ITEMPREPAINT | CDDS_SUBITEM
                    SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NOTIFYSUBITEMDRAW);
                    return TRUE;
                }

                if (cd->nmcd.dwDrawStage == (CDDS_ITEMPREPAINT | CDDS_SUBITEM))
                {
                    // cd->iSubItem == 1 &&
                    if (GetHighlightIndex(Items[(int)cd->nmcd.lItemlParam].Tag) != -1)
                    {
                        cd->clrTextBk = RGB(255, 255, 0);
                        SetWindowLongPtr(HWindow, DWLP_MSGRESULT, CDRF_NEWFONT);
                        return TRUE;
                    }
                    break;
                }

                break;
            }

            case LVN_ITEMCHANGED:
            {
                LPNMLISTVIEW nmhi = (LPNMLISTVIEW)nmh;
                if (nmhi->uNewState & LVIS_FOCUSED)
                {
                    int index = (int)GetFocusedItemLParam();
                    if (index >= 0 && index < Items.Count)
                    {
                        SetDlgItemTextA(HWindow, IDC_IMGEXIF_INFO, Items[index].TagDescription);
                        BOOL highlighted = GetHighlightIndex(Items[index].Tag) != -1;
                        CheckDlgButton(HWindow, IDC_IMGEXIF_HIGHLIGHT, highlighted ? BST_CHECKED : BST_UNCHECKED);
                    }
                }
                break;
            }

            case NM_DBLCLK:
            {
                PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDC_IMGEXIF_HIGHLIGHT, BN_CLICKED), 0);
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
                return 0;
            }
            }
        }
        break;
    }

    case WM_DESTROY:
    {
        RECT rc;
        GetWindowRect(HWindow, &rc);
        G.ExifDlgWidth = rc.right - rc.left;
        G.ExifDlgHeight = rc.bottom - rc.top;
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCopyToDlg
//

// RadioButtons
int COPYTO_RB_ID[COPYTO_LINES] = {IDC_COPYTO_RB_1, IDC_COPYTO_RB_2, IDC_COPYTO_RB_3, IDC_COPYTO_RB_4, IDC_COPYTO_RB_5};
// EditLines
int COPYTO_EL_ID[COPYTO_LINES] = {IDC_COPYTO_EL_1, IDC_COPYTO_EL_2, IDC_COPYTO_EL_3, IDC_COPYTO_EL_4, IDC_COPYTO_EL_5};
// BRowse
int COPYTO_BR_ID[COPYTO_LINES] = {IDC_COPYTO_BR_1, IDC_COPYTO_BR_2, IDC_COPYTO_BR_3, IDC_COPYTO_BR_4, IDC_COPYTO_BR_5};

CCopyToDlg::CCopyToDlg(HWND hParent, LPCTSTR srcName, LPTSTR dstName)
    : CCommonDialog(HLanguage, IDD_COPYTO, IDD_COPYTO, hParent)
{
    SrcName = srcName;
    DstName = dstName;
}

void CCopyToDlg::Validate(CTransferInfo& ti)
{
    int line;
    int i;
    for (i = 0; i < COPYTO_LINES; i++)
        ti.RadioButton(COPYTO_RB_ID[i], i, line);

    TCHAR buff[MAX_PATH];
    ti.EditLine(COPYTO_EL_ID[line], buff, SizeOf(buff));

    TCHAR path[MAX_PATH];
    _tcscpy(path, buff);

    // do srcDir vlozime plnou cestu k prohlizenemu souboru a odrizneme nazev
    TCHAR srcDir[MAX_PATH];
    lstrcpyn(srcDir, SrcName, SizeOf(srcDir));
    LPTSTR namePart = (LPTSTR)_tcsrchr(srcDir, '\\');
    if (namePart == NULL)
    {
        TRACE_E("Unexpected situation: cannot find the file name!");
        ti.ErrorOn(COPYTO_EL_ID[line]);
        return;
    }
    *namePart = 0;
    namePart++;

    int errTextID;
    if (!SalGetFullName(path, &errTextID, srcDir))
    {
        TCHAR errBuf[MAX_PATH] = _T("");
        if (errTextID == GFN_EMPTYNAMENOTALLOWED)
            _tcscpy(errBuf, LoadStr(IDS_SPECIFYPATH)); // pro prazdny string dosadime intuitivnejsi hlasku
        else
            SalamanderGeneral->GetGFNErrorText(errTextID, errBuf, MAX_PATH);
        SalamanderGeneral->SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORTITLE),
                                         MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(COPYTO_EL_ID[line]);
    }

    // pripravime navratovku
    if (ti.IsGood())
    {
        SalamanderGeneral->SalPathAppend(path, namePart, MAX_PATH);
        _tcscpy(DstName, path);
    }
}

void CCopyToDlg::Transfer(CTransferInfo& ti)
{
    int i;
    for (i = 0; i < COPYTO_LINES; i++)
    {
        ti.RadioButton(COPYTO_RB_ID[i], i, G.CopyToLastIndex);

        TCHAR buff[MAX_PATH];
        if (ti.Type == ttDataToWindow)
        {
            if (G.CopyToDestinations[i] != NULL)
                lstrcpyn(buff, G.CopyToDestinations[i], SizeOf(buff));
            else
                buff[0] = 0;
        }
        ti.EditLine(COPYTO_EL_ID[i], buff, SizeOf(buff));
        if (ti.Type == ttDataFromWindow)
        {
            if (G.CopyToDestinations[i] != NULL)
            {
                SalamanderGeneral->Free(G.CopyToDestinations[i]);
                G.CopyToDestinations[i] = NULL;
            }
            if (buff[0] != 0)
                G.CopyToDestinations[i] = SalamanderGeneral->DupStr(buff);
        }
    }

    if (ti.Type == ttDataToWindow)
        EnableControls();
}

void CCopyToDlg::EnableControls()
{
    int i;
    for (i = 0; i < COPYTO_LINES; i++)
    {
        BOOL enabled = IsDlgButtonChecked(HWindow, COPYTO_RB_ID[i]) == BST_CHECKED;
        EnableWindow(GetDlgItem(HWindow, COPYTO_EL_ID[i]), enabled);
        EnableWindow(GetDlgItem(HWindow, COPYTO_BR_ID[i]), enabled);
    }
}

INT_PTR
CCopyToDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED)
            EnableControls();

        int i;
        for (i = 0; i < COPYTO_LINES; i++)
        {
            if (LOWORD(wParam) == COPYTO_BR_ID[i])
            {
                TCHAR path[MAX_PATH];
                GetDlgItemText(HWindow, COPYTO_EL_ID[i], path, MAX_PATH);
                if (SalamanderGeneral->GetTargetDirectory(HWindow, HWindow, path,
                                                          LoadStr(IDS_SELECTTARGETDIR), path,
                                                          FALSE, path))
                {
                    SetDlgItemText(HWindow, COPYTO_EL_ID[i], path);
                }
            }
        }

        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
