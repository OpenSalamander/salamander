// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

bool SelectConversion(int x, int y, HWND wnd, CCompareOptions& options, int file)
{
    enum
    {
        idAutomatic = 1,
        idASCII8,
        idASCII8NoInputEnc,
        idASCII8AutoInputEnc,
        idUTF8,
        idUTF16LE,
        idUTF16BE,
        idUTF32LE,
        idUTF32BE,
        idFirstASCII8InputEnc,
    };
    MENU_TEMPLATE_ITEM menuTemplate[] =
        {
            {MNTT_PB, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
            // auto
            {MNTT_IT, IDS_ENCMENU_AUTO, MNTS_B | MNTS_I | MNTS_A, idAutomatic, -1, 0, NULL},

            // ASCII-8
            {MNTT_PB, IDS_ENCMENU_ASCII8, MNTS_B | MNTS_I | MNTS_A, idASCII8, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_ASCII8NOINPUTENC, MNTS_B | MNTS_I | MNTS_A, idASCII8NoInputEnc, -1, 0, NULL},
            {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_ASCII8AUTOINPUTENC, MNTS_B | MNTS_I | MNTS_A, idASCII8AutoInputEnc, -1, 0, NULL},
            //{MNTT_SP,	-1,				MNTS_B|MNTS_I|MNTS_A, 0,                        -1, 0, NULL},
            {MNTT_PE},

            // UNICODE
            {MNTT_PB, IDS_ENCMENU_UNICODE, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_UTF8, MNTS_B | MNTS_I | MNTS_A, idUTF8, -1, 0, NULL},
            {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_UTF16LE, MNTS_B | MNTS_I | MNTS_A, idUTF16LE, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_UTF16BE, MNTS_B | MNTS_I | MNTS_A, idUTF16BE, -1, 0, NULL},
            {MNTT_SP, -1, MNTS_B | MNTS_I | MNTS_A, 0, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_UTF32LE, MNTS_B | MNTS_I | MNTS_A, idUTF32LE, -1, 0, NULL},
            {MNTT_IT, IDS_ENCMENU_UTF32BE, MNTS_B | MNTS_I | MNTS_A, idUTF32BE, -1, 0, NULL},
            {MNTT_PE},
            {MNTT_PE}, // terminator
        };

    CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
    menu->LoadFromTemplate(HLanguage, menuTemplate, NULL, NULL, NULL);

    // add ASCII-8 input conversions to the table
    CGUIMenuPopupAbstract* menuASCII8 = menu->GetSubMenu(idASCII8, FALSE);

    int index = 0;
    const char* name;
    while (SG->EnumConversionTables(wnd, &index, &name, NULL))
    {
        MENU_ITEM_INFO mii;
        if (name)
        {
            // table
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_STRING;
            mii.Type = MENU_TYPE_STRING;
            mii.ID = idFirstASCII8InputEnc + index - 1;
            mii.String = (char*)name;
            mii.StringLen = int(strlen(name));
        }
        else
        {
            // separator
            mii.Mask = MENU_MASK_TYPE;
            mii.Type = MENU_TYPE_SEPARATOR;
        }
        menuASCII8->InsertItem(menuASCII8->GetItemCount(), TRUE, &mii);
    }

    int id;
    bool ret = true;
    switch (id = menu->Track(MENU_TRACK_NONOTIFY | MENU_TRACK_RETURNCMD, x, y, wnd, NULL))
    {
    case 0:
        ret = false;
        break;

    case idAutomatic:
        options.Encoding[file] = CTextFileReader::encUnknown;
        break;

    case idASCII8:
        break;

    case idASCII8NoInputEnc:
        options.Encoding[file] = CTextFileReader::encASCII8;
        options.PerformASCII8InputEnc[file] = 0;
        break;

    case idASCII8AutoInputEnc:
        options.Encoding[file] = CTextFileReader::encASCII8;
        options.PerformASCII8InputEnc[file] = 1;
        options.ASCII8InputEncTableName[file][0] = 0;
        break;

    case idUTF8:
        options.Encoding[file] = CTextFileReader::encUTF8;
        break;

    case idUTF16LE:
        options.Encoding[file] = CTextFileReader::encUTF16;
        options.Endians[file] = CTextFileReader::endianLittle;
        break;

    case idUTF16BE:
        options.Encoding[file] = CTextFileReader::encUTF16;
        options.Endians[file] = CTextFileReader::endianBig;
        break;

    case idUTF32LE:
        options.Encoding[file] = CTextFileReader::encUTF32;
        options.Endians[file] = CTextFileReader::endianLittle;
        break;

    case idUTF32BE:
        options.Encoding[file] = CTextFileReader::encUTF32;
        options.Endians[file] = CTextFileReader::endianBig;
        break;

    default:
    {
        options.Encoding[file] = CTextFileReader::encASCII8;
        options.PerformASCII8InputEnc[file] = 1;
        MENU_ITEM_INFO mii;
        mii.Mask = MENU_MASK_STRING;
        mii.String = options.ASCII8InputEncTableName[file];
        mii.StringLen = sizeof(options.ASCII8InputEncTableName[file]);
        menuASCII8->GetItemInfo(id, FALSE, &mii);
    }
    }

    SalGUI->DestroyMenuPopup(menu);

    return ret;
};

// ****************************************************************************
//
// CAdvancedOptionsDialog
//

int crlfIDs[] = {IDC_CRLF1, IDC_CRLF2};
int crIDs[] = {IDC_CR1, IDC_CR2};
int lfIDs[] = {IDC_LF1, IDC_LF2};
int nullIDs[] = {IDC_NULL1, IDC_NULL2};

CAdvancedOptionsDialog::CAdvancedOptionsDialog(HWND parent, CCompareOptions* options,
                                               BOOL* setDefault)
    : CCommonDialog(IDD_ADVANCEDOPTIONS, IDD_ADVANCEDOPTIONS, parent)
{
    CALL_STACK_MESSAGE_NONE
    InOutOptions = options;
    SetDefault = setDefault;
    Options = *options;
}

void CAdvancedOptionsDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CAdvancedOptionsDialog::Transfer()");
    // ignores
    ti.CheckBox(IDC_IGNOREALLSPACE, Options.IgnoreAllSpace);
    ti.CheckBox(IDC_IGNORESPACECHANGE, Options.IgnoreSpaceChange);
    //ti.CheckBox(IDC_IGNOREBLANKS, Options->IgnoreBlankLines);
    ti.CheckBox(IDC_IGNORECASE, Options.IgnoreCase);
    // file type
    int type = 0;
    if (Options.ForceText)
        type = 1;
    if (Options.ForceBinary)
        type = 2;
    ti.RadioButton(IDR_AUTO, 0, type);
    ti.RadioButton(IDR_ALWAYSTEXT, 1, type);
    ti.RadioButton(IDR_ALWAYSBINARY, 2, type);
    switch (type)
    {
    case 0:
        Options.ForceText = 0;
        Options.ForceBinary = 0;
        break;
    case 1:
        Options.ForceText = 1;
        Options.ForceBinary = 0;
        break;
    case 2:
        Options.ForceText = 0;
        Options.ForceBinary = 1;
        break;
    }
    // line ends
    BOOL select;
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            select = Options.EolConversion[i] & CTextFileReader::eolCRLF;
            ti.CheckBox(crlfIDs[i], select);
            select = Options.EolConversion[i] & CTextFileReader::eolCR;
            ti.CheckBox(crIDs[i], select);
            select = TRUE;
            ti.CheckBox(lfIDs[i], select);
            select = Options.EolConversion[i] & CTextFileReader::eolNULL;
            ti.CheckBox(nullIDs[i], select);
        }
    }
    else
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            Options.EolConversion[i] = 0;
            ti.CheckBox(crlfIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolCRLF;
            ti.CheckBox(crIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolCR;
            // ti.CheckBox(IDC_LF + i, select);
            // if (select) Options.EolConversion[i] |= RF_LF;
            ti.CheckBox(nullIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolNULL;
        }
    }

    ti.CheckBox(IDC_NORMALIZATION_FORM, Options.NormalizationForm);
    if (!PNormalizeString)
        EnableWindow(GetDlgItem(HWindow, IDC_NORMALIZATION_FORM), FALSE);

    ti.CheckBox(IDC_USEINFUTURE, *SetDefault);

    if (ti.Type == ttDataFromWindow)
        *InOutOptions = Options;
    else
    {
        UpdateEncodingInfo();
        SalGUI->AttachButton(HWindow, IDB_LEFTENC, BTF_RIGHTARROW);
        SalGUI->AttachButton(HWindow, IDB_RIGHTENC, BTF_RIGHTARROW);
    }
}

void CAdvancedOptionsDialog::UpdateEncodingInfo()
{
    char buffer[200];
    /*  bool forceText = 
    SendDlgItemMessage(HWindow, IDR_ALWAYSTEXT, BM_GETCHECK, 0, 0) == BST_CHECKED;
  bool forceBinary = 
    SendDlgItemMessage(HWindow, IDR_ALWAYSBINARY, BM_GETCHECK, 0, 0) == BST_CHECKED;*/
    int f;
    for (f = 0; f < 2; ++f)
    {
        /*    if (!forceText && !forceBinary)
      strcpy(buffer, LoadStr(IDS_ENCMENU_AUTO));
    elif (forceBinary)
      buffer[0] = 0;
    else*/
        {
            switch (Options.Encoding[f])
            {
            case CTextFileReader::encUnknown:
                strcpy(buffer, LoadStr(IDS_ENCMENU_AUTO));
                break;

            case CTextFileReader::encASCII8:
                strcpy(buffer, "ASCII-8");
                if (Options.PerformASCII8InputEnc[f])
                {
                    if (Options.ASCII8InputEncTableName[f][0])
                    {
                        strcat(buffer, ", ");
                        strcat(buffer, Options.ASCII8InputEncTableName[f]);
                    }
                    else
                        strcat(buffer, LoadStr(IDS_AUTOINPUTENC));
                }
                else
                    strcat(buffer, LoadStr(IDS_NOINPUTENC));

                break;

            case CTextFileReader::encUTF8:
                strcpy(buffer, "UTF-8");
                break;

            case CTextFileReader::encUTF16:
                strcpy(buffer, "UTF-16");
                if (Options.Endians[f] == CTextFileReader::endianBig)
                    strcat(buffer, ", big endian");
                break;

            case CTextFileReader::encUTF32:
                strcpy(buffer, "UTF-32");
                if (Options.Endians[f] == CTextFileReader::endianBig)
                    strcat(buffer, ", big endian");
                break;
            }
        }
        SetDlgItemText(HWindow, IDE_LEFTENC + f, buffer);
    }
}

INT_PTR
CAdvancedOptionsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_LEFTENC:
        case IDB_RIGHTENC:
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, LOWORD(wParam)), &r);
            if (SelectConversion(r.right, r.top, HWindow, Options, LOWORD(wParam) == IDB_LEFTENC ? 0 : 1))
            {
                SendDlgItemMessage(HWindow, IDR_AUTO, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
                SendDlgItemMessage(HWindow, IDR_ALWAYSTEXT, BM_SETCHECK, (WPARAM)BST_CHECKED, 0);
                SendDlgItemMessage(HWindow, IDR_ALWAYSBINARY, BM_SETCHECK, (WPARAM)BST_UNCHECKED, 0);
            }
            UpdateEncodingInfo();
            return TRUE;
        }
        case IDR_AUTO:
        case IDR_ALWAYSBINARY:
        case IDR_ALWAYSTEXT:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                UpdateEncodingInfo();
            }
            break;
        }
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

// ****************************************************************************
//
// CPropPageDefaultOptions
//

CPropPageDefaultOptions::CPropPageDefaultOptions(CCompareOptions* options, DWORD* changeFlag)
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGDEFOPTIONS, IDD_CFGDEFOPTIONS, PSP_HASHELP, NULL)
{
    CALL_STACK_MESSAGE_NONE
    InOutOptions = options;
    ChangeFlag = changeFlag;
    Options = *options;
    oldOptions = Options;
}

void CPropPageDefaultOptions::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CPropPageDefaultOptions::Transfer()");

    // ignores
    ti.CheckBox(IDC_IGNOREALLSPACE, Options.IgnoreAllSpace);
    ti.CheckBox(IDC_IGNORESPACECHANGE, Options.IgnoreSpaceChange);
    //ti.CheckBox(IDC_IGNOREBLANKS, Options->IgnoreBlankLines);
    ti.CheckBox(IDC_IGNORECASE, Options.IgnoreCase);
    // file type
    int type = 0;
    if (Options.ForceText)
        type = 1;
    if (Options.ForceBinary)
        type = 2;
    ti.RadioButton(IDR_AUTO, 0, type);
    ti.RadioButton(IDR_ALWAYSTEXT, 1, type);
    ti.RadioButton(IDR_ALWAYSBINARY, 2, type);
    switch (type)
    {
    case 0:
        Options.ForceText = 0;
        Options.ForceBinary = 0;
        break;
    case 1:
        Options.ForceText = 1;
        Options.ForceBinary = 0;
        break;
    case 2:
        Options.ForceText = 0;
        Options.ForceBinary = 1;
        break;
    }
    ti.CheckBox(IDC_NORMALIZATION_FORM, Options.NormalizationForm);
    // line ends
    BOOL select;
    if (ti.Type == ttDataToWindow)
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            select = Options.EolConversion[i] & CTextFileReader::eolCRLF;
            ti.CheckBox(crlfIDs[i], select);
            select = Options.EolConversion[i] & CTextFileReader::eolCR;
            ti.CheckBox(crIDs[i], select);
            select = TRUE;
            ti.CheckBox(lfIDs[i], select);
            select = Options.EolConversion[i] & CTextFileReader::eolNULL;
            ti.CheckBox(nullIDs[i], select);
        }
    }
    else
    {
        int i;
        for (i = 0; i < 2; i++)
        {
            Options.EolConversion[i] = 0;
            ti.CheckBox(crlfIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolCRLF;
            ti.CheckBox(crIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolCR;
            // ti.CheckBox(lfIDs[i], select);
            // if (select) Options->EolConversion[i] |= RF_LF;
            ti.CheckBox(nullIDs[i], select);
            if (select)
                Options.EolConversion[i] |= CTextFileReader::eolNULL;
        }
        if (memcmp(&Options, &oldOptions, sizeof(Options)) != 0)
            *ChangeFlag |= CC_DEFOPTIONS;
    }

    if (!PNormalizeString)
        EnableWindow(GetDlgItem(HWindow, IDC_NORMALIZATION_FORM), FALSE);

    if (ti.Type == ttDataFromWindow)
        *InOutOptions = Options;
    else
    {
        UpdateEncodingInfo();
        SalGUI->AttachButton(HWindow, IDB_LEFTENC, BTF_RIGHTARROW);
        SalGUI->AttachButton(HWindow, IDB_RIGHTENC, BTF_RIGHTARROW);
    }
}

void CPropPageDefaultOptions::UpdateEncodingInfo()
{
    char buffer[200];
    int f;
    for (f = 0; f < 2; ++f)
    {
        switch (Options.Encoding[f])
        {
        case CTextFileReader::encUnknown:
            strcpy(buffer, LoadStr(IDS_ENCMENU_AUTO));
            break;

        case CTextFileReader::encASCII8:
            strcpy(buffer, "ASCII-8");
            if (Options.PerformASCII8InputEnc[f])
            {
                if (Options.ASCII8InputEncTableName[f][0])
                {
                    strcat(buffer, ", ");
                    strcat(buffer, Options.ASCII8InputEncTableName[f]);
                }
                else
                    strcat(buffer, LoadStr(IDS_AUTOINPUTENC));
            }
            else
                strcat(buffer, LoadStr(IDS_NOINPUTENC));

            break;

        case CTextFileReader::encUTF8:
            strcpy(buffer, "UTF-8");
            break;

        case CTextFileReader::encUTF16:
            strcpy(buffer, "UTF-16");
            if (Options.Endians[f] == CTextFileReader::endianBig)
                strcat(buffer, ", big endian");
            break;

        case CTextFileReader::encUTF32:
            strcpy(buffer, "UTF-32");
            if (Options.Endians[f] == CTextFileReader::endianBig)
                strcat(buffer, ", big endian");
            break;
        }
        SetDlgItemText(HWindow, IDE_LEFTENC + f, buffer);
    }
}

INT_PTR
CPropPageDefaultOptions::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_LEFTENC:
        case IDB_RIGHTENC:
        {
            RECT r;
            GetWindowRect(GetDlgItem(HWindow, LOWORD(wParam)), &r);
            SelectConversion(r.right, r.top, HWindow, Options, LOWORD(wParam) == IDB_LEFTENC ? 0 : 1);
            UpdateEncodingInfo();
            return TRUE;
        }
        }
    }
    }
    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}
