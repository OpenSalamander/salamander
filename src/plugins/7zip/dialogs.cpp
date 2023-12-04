// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "7zip.h"
#include "dialogs.h"
#include "7zip.rh"
#include "7zip.rh2"
#include "lang\lang.rh"

//****************************************************************************
//
// CCommonDialog
//

struct CResDataPair
{
    UINT ResId;
    DWORD Data;
};

CResDataPair CompressLevel[] =
    {
        {IDS_STORE, COMPRESS_LEVEL_STORE},
        {IDS_FASTEST, COMPRESS_LEVEL_FASTEST},
        {IDS_FAST, COMPRESS_LEVEL_FAST},
        {IDS_NORMAL, COMPRESS_LEVEL_NORMAL},
        {IDS_MAXIMUM, COMPRESS_LEVEL_MAXIMUM},
        {IDS_ULTRA, COMPRESS_LEVEL_ULTRA}};

CResDataPair CompressMethod[] =
    {
        {IDS_METHOD_LZMA, CCompressParams::LZMA},
        //  { IDS_METHOD_LZMA2, CCompressParams::LZMA2 },
        {IDS_METHOD_PPMD, CCompressParams::PPMd},
};

int LZMADictSize[] =
    {64, 1024, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536};

int LZMAWordSize[] =
    {8, 12, 16, 24, 32, 48, 64, 96, 128, 192, 256, 273};

int PPMdDictSize[] =
    {1024, 2048, 3072, 4096, 6144, 8192, 12288, 16384, 24576, 32768, 49152, 65536, 98304, 131072, 196608, 262144,
     393216, 524288, 786432, 1048576, 1572864};

int PPMdWordSize[] =
    {2, 3, 4, 5, 6, 7, 8, 10, 12, 14, 16, 20, 24, 28, 32};

void FormatBytes(LPTSTR buffer, int bytes)
{
    if (bytes >= 1024)
    {
        _stprintf(buffer, _T("%d M"), bytes / 1024);
    }
    else
    {
        _stprintf(buffer, _T("%d K"), bytes);
    }
}

void SetComboCurSelData(HWND hWnd, DWORD data)
{
    //  TRACE_I("SetComboCurSelData");

    // pocet polozek
    int itemCount = (int)SendMessage(hWnd, CB_GETCOUNT, (WPARAM)0, (LPARAM)0);
    int i;
    for (i = 0; i < itemCount; i++)
    {
        DWORD itemData = (DWORD)SendMessage(hWnd, CB_GETITEMDATA, (WPARAM)i, (LPARAM)0);
        if (itemData == data)
            SendMessage(hWnd, CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
    }
}

//
// CCompressParamsDlg
//

CCompressParamsDlg::CCompressParamsDlg(CCompressParams* compressParams, int idCfgCompressLevel, int idCfgCompressMethod,
                                       int idCfgDictSize, int idCfgWordSize, int idCfgSolidArchive)
{
    IDCfgCompressLevel = idCfgCompressLevel;
    IDCfgCompressMethod = idCfgCompressMethod;
    IDCfgDictSize = idCfgDictSize;
    IDCfgWordSize = idCfgWordSize;
    IDCfgSolidArchive = idCfgSolidArchive;

    CompressParams = compressParams;
}

void CCompressParamsDlg::Transfer(CTransferInfo& ti)
{
    int i;

    // compression level combo-box
    if (ti.Type == ttDataToWindow)
    {
        // pocet polozek
        int itemCount = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETCOUNT, (WPARAM)0, (LPARAM)0);
        for (i = 0; i < itemCount; i++)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETITEMDATA, (WPARAM)i, (LPARAM)0);
            if (data == (DWORD)CompressParams->CompressLevel)
            {
                SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
            }
        }
    }
    else if (ti.Type == ttDataFromWindow)
    {
        int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
        if (curSel != CB_ERR)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
            CompressParams->CompressLevel = data;
        }
    }

    // set compression method
    if (ti.Type == ttDataToWindow)
    {
        // pocet polozek
        int itemCount = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETCOUNT, (WPARAM)0, (LPARAM)0);
        for (i = 0; i < itemCount; i++)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETITEMDATA, (WPARAM)i, (LPARAM)0);
            if (data == (DWORD)CompressParams->Method)
            {
                SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
                // poslat si notifikaci :)
                SendMessage(HWindow, WM_COMMAND, MAKEWPARAM(IDCfgCompressMethod, CBN_SELENDOK), (LPARAM)GetDlgItem(HWindow, IDCfgCompressMethod));
            }
        }
    }
    else if (ti.Type == ttDataFromWindow)
    {
        int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
        if (curSel != CB_ERR)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
            CompressParams->Method = (CCompressParams::EMethod)data;
        }
    }

    // set dictionary size
    if (ti.Type == ttDataToWindow)
    {
        // pocet polozek
        int itemCount = (int)SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_GETCOUNT, (WPARAM)0, (LPARAM)0);
        for (i = 0; i < itemCount; i++)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_GETITEMDATA, (WPARAM)i, (LPARAM)0);
            if (data == (DWORD)CompressParams->DictSize)
                SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
        }
    }
    else if (ti.Type == ttDataFromWindow)
    {
        int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
        if (curSel != CB_ERR)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
            CompressParams->DictSize = data;
        }
    }

    // set word size
    if (ti.Type == ttDataToWindow)
    {
        // pocet polozek
        int itemCount = (int)SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_GETCOUNT, (WPARAM)0, (LPARAM)0);
        for (i = 0; i < itemCount; i++)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_GETITEMDATA, (WPARAM)i, (LPARAM)0);
            if (data == (DWORD)CompressParams->WordSize)
                SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_SETCURSEL, (WPARAM)i, (LPARAM)0);
        }
    }
    else if (ti.Type == ttDataFromWindow)
    {
        int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
        if (curSel != CB_ERR)
        {
            DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
            CompressParams->WordSize = data;
        }
    }

    // set create solid archive
    ti.CheckBox(IDCfgSolidArchive, CompressParams->SolidArchive);
}

void CCompressParamsDlg::FillCompressLevelCombo()
{
    SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_RESETCONTENT, 0, 0);

    int i;
    for (i = 0; i < sizeof(CompressLevel) / sizeof(CResDataPair); i++)
    {
        // vlozit polozku
        int res = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_ADDSTRING, 0, (LPARAM)LoadStr(CompressLevel[i].ResId));
        if (res != CB_ERR)
            // nastavit data
            SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_SETITEMDATA, (WPARAM)res, (LPARAM)CompressLevel[i].Data);
    }
}

void CCompressParamsDlg::FillCompressMethodCombo()
{
    SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_RESETCONTENT, 0, 0);

    int i;
    for (i = 0; i < sizeof(CompressMethod) / sizeof(CResDataPair); i++)
    {
        // vlozit polozku
        int res = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_ADDSTRING, 0, (LPARAM)LoadStr(CompressMethod[i].ResId));
        if (res != CB_ERR)
            // nastavit data
            SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_SETITEMDATA, (WPARAM)res, (LPARAM)CompressMethod[i].Data);
    }
}

void CCompressParamsDlg::FillDictSizeCombo(int values[], int size)
{
    SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_RESETCONTENT, 0, 0);

    int i;
    for (i = 0; i < size; i++)
    {
        // vlozit polozku
        TCHAR buffer[64];
        FormatBytes(buffer, values[i]);
        int res = (int)SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_ADDSTRING, 0, (LPARAM)buffer);
        if (res != CB_ERR)
            // nastavit data
            SendMessage(GetDlgItem(HWindow, IDCfgDictSize), CB_SETITEMDATA, (WPARAM)res, (LPARAM)values[i]);
    }
}

void CCompressParamsDlg::FillWordSizeCombo(int values[], int size)
{
    SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_RESETCONTENT, 0, 0);

    int i;
    for (i = 0; i < size; i++)
    {
        // vlozit polozku
        TCHAR buffer[64];
        _stprintf(buffer, _T("%d"), values[i]);
        int res = (int)SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_ADDSTRING, 0, (LPARAM)buffer);
        if (res != CB_ERR)
            // nastavit data
            SendMessage(GetDlgItem(HWindow, IDCfgWordSize), CB_SETITEMDATA, (WPARAM)res, (LPARAM)values[i]);
    }
}

void CCompressParamsDlg::OnChangeMethod()
{
    //  TRACE_I("OnChangeMethod");

    int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
    if (curSel != CB_ERR)
    {
        DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
        CCompressParams::EMethod method = (CCompressParams::EMethod)data;
        switch (method)
        {
        case CCompressParams::LZMA:
        case CCompressParams::LZMA2:
            FillDictSizeCombo(LZMADictSize, sizeof(LZMADictSize) / sizeof(int));
            FillWordSizeCombo(LZMAWordSize, sizeof(LZMAWordSize) / sizeof(int));
            // set defaults
            OnChangeLevel();
            //        SetComboCurSelData(GetDlgItem(HWindow, IDC_CFG_DICT_SIZE), 1024);
            //        SetComboCurSelData(GetDlgItem(HWindow, IDC_CFG_WORD_SIZE), 32);
            break;

        case CCompressParams::PPMd:
            FillDictSizeCombo(PPMdDictSize, sizeof(PPMdDictSize) / sizeof(int));
            FillWordSizeCombo(PPMdWordSize, sizeof(PPMdWordSize) / sizeof(int));
            // set defaults
            OnChangeLevel();
            //        SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 16384);
            //        SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 6);
            break;
        }
    }
}

void CCompressParamsDlg::OnChangeLevel()
{
    // kdyz se meni compression level, tak se meni i default hodnota pro dict size podle compression level, ale jen u LZMA
    int curSel = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
    if (curSel != CB_ERR)
    {
        DWORD data = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressMethod), CB_GETITEMDATA, (WPARAM)curSel, (LPARAM)0);
        CCompressParams::EMethod method = (CCompressParams::EMethod)data;

        int curSel2 = (int)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETCURSEL, (WPARAM)0, (LPARAM)0);
        if (curSel2 != CB_ERR)
        {
            DWORD level = (DWORD)SendMessage(GetDlgItem(HWindow, IDCfgCompressLevel), CB_GETITEMDATA, (WPARAM)curSel2, (LPARAM)0);

            EnableWindow(GetDlgItem(HWindow, IDCfgCompressMethod), level != COMPRESS_LEVEL_STORE);
            EnableWindow(GetDlgItem(HWindow, IDCfgDictSize), level != COMPRESS_LEVEL_STORE);
            EnableWindow(GetDlgItem(HWindow, IDCfgWordSize), level != COMPRESS_LEVEL_STORE);

            switch (method)
            {
            case CCompressParams::LZMA:
            case CCompressParams::LZMA2:
                switch (level)
                {
                case COMPRESS_LEVEL_FASTEST:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 64);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 32);
                    break;

                case COMPRESS_LEVEL_FAST:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 1024);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 32);
                    break;

                case COMPRESS_LEVEL_NORMAL:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 16384);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 32);
                    break;

                case COMPRESS_LEVEL_MAXIMUM:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 32768);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 64);
                    break;

                case COMPRESS_LEVEL_ULTRA:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 65536);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 64);
                    break;
                }
                break;

            case CCompressParams::PPMd:
                switch (level)
                {
                case COMPRESS_LEVEL_FASTEST:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 4096);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 4);
                    break;

                case COMPRESS_LEVEL_FAST:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 4096);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 4);
                    break;

                case COMPRESS_LEVEL_NORMAL:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 16384);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 6);
                    break;

                case COMPRESS_LEVEL_MAXIMUM:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 65536);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 16);
                    break;

                case COMPRESS_LEVEL_ULTRA:
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), 196608);
                    SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), 32);
                    break;
                }
                break;
            }
        }
    }
}

INT_PTR
CCompressParamsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        FillCompressLevelCombo();
        FillCompressMethodCombo();

        SetComboCurSelData(GetDlgItem(HWindow, IDCfgDictSize), CompressParams->DictSize);
        SetComboCurSelData(GetDlgItem(HWindow, IDCfgWordSize), CompressParams->WordSize);

        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_SELENDOK && LOWORD(wParam) == IDC_CFG_COMPRESS_METHOD)
        {
            OnChangeMethod();
            OnChangeLevel();
        }
        else if (HIWORD(wParam) == CBN_SELENDOK && LOWORD(wParam) == IDC_CFG_COMPRESS_LEVEL)
        {
            OnChangeLevel();
        }
        break;
    }

    } // switch

    return TRUE;
}

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
        // horizontalni i vertikalni vycentrovani dialogu k parentu
        if (Parent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, Parent, TRUE);
        break; // chci focus od DefDlgProc

    case WM_COMMAND:
        break;

    } // switch
    return CDialog::DialogProc(uMsg, wParam, lParam);
}

void CCommonDialog::NotifDlgJustCreated()
{
    SalamanderGUI->ArrangeHorizontalLines(HWindow);
}

//****************************************************************************
//
// CConfigurationDialog
//

CConfigurationDialog::CConfigurationDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_CONFIGURATION, IDD_CONFIGURATION, hParent),
      CompressParamsDlg(&Cfg.CompressParams, IDC_CFG_COMPRESS_LEVEL, IDC_CFG_COMPRESS_METHOD, IDC_CFG_DICT_SIZE, IDC_CFG_WORD_SIZE,
                        IDC_CFG_SOLID_ARCHIVE)
{
}

void CConfigurationDialog::Transfer(CTransferInfo& ti)
{
    //  int i;

    ti.CheckBox(IDC_CFG_SHOWEXTENDEDOPTIONS, Cfg.ShowExtendedOptions);
    ti.CheckBox(IDC_CFG_EXTENDEDLISTINFO, Cfg.ExtendedListInfo);
    ti.CheckBox(IDC_CFG_LISTINFOPACKEDSIZE, Cfg.ListInfoPackedSize);
    ti.CheckBox(IDC_CFG_LISTINFOMETHOD, Cfg.ListInfoMethod);

    CompressParamsDlg.Transfer(ti);
}

INT_PTR
CConfigurationDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        EnableWindow(GetDlgItem(HWindow, IDC_CFG_LISTINFOPACKEDSIZE), Cfg.ExtendedListInfo);
        EnableWindow(GetDlgItem(HWindow, IDC_CFG_LISTINFOMETHOD), Cfg.ExtendedListInfo);

        CompressParamsDlg.HWindow = HWindow;
        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_CFG_EXTENDEDLISTINFO)
        {
            BOOL enable = IsDlgButtonChecked(HWindow, IDC_CFG_EXTENDEDLISTINFO) == BST_CHECKED;

            EnableWindow(GetDlgItem(HWindow, IDC_CFG_LISTINFOPACKEDSIZE), enable);
            EnableWindow(GetDlgItem(HWindow, IDC_CFG_LISTINFOMETHOD), enable);
            if (!enable)
            {
                CheckDlgButton(HWindow, IDC_CFG_LISTINFOPACKEDSIZE, BST_UNCHECKED);
                CheckDlgButton(HWindow, IDC_CFG_LISTINFOMETHOD, BST_UNCHECKED);
            }
        }
        break;
    }

    } // switch

    CompressParamsDlg.DialogProc(uMsg, wParam, lParam);

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CExtOptionsDialog
//

CExtOptionsDialog::CExtOptionsDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_NEWARCHIVE, IDD_NEWARCHIVE, hParent),
      CompressParamsDlg(&CompressParams, IDC_CFG_COMPRESS_LEVEL, IDC_CFG_COMPRESS_METHOD, IDC_CFG_DICT_SIZE, IDC_CFG_WORD_SIZE,
                        IDC_CFG_SOLID_ARCHIVE)
{
    Encrypt = FALSE;

    Archive[0] = '\0';
    Password[0] = '\0';
    ConfirmedPassword[0] = '\0';

    NotAgain = FALSE;

    Title = NULL;
}

void CExtOptionsDialog::Transfer(CTransferInfo& ti)
{
    ti.CheckBox(IDC_NA_ENCRYPTFILES, Encrypt);
    ti.EditLine(IDC_NA_PASSWORD, Password, PASSWORD_LEN);
    ti.EditLine(IDC_NA_CONFIRMPASSWORD, ConfirmedPassword, PASSWORD_LEN);

    ti.CheckBox(IDC_NA_NOTAGAIN, NotAgain);

    CompressParamsDlg.Transfer(ti);
}

BOOL CExtOptionsDialog::CreateChilds()
{
    CGUIStaticTextAbstract* text;

    text = SalamanderGUI->AttachStaticText(HWindow, IDC_NA_ARCHIVE, STF_PATH_ELLIPSIS);
    if (text == NULL)
        return FALSE;

    return TRUE;
}

INT_PTR
CExtOptionsDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (Title != NULL)
            SetWindowText(HWindow, Title);

        if (!CreateChilds())
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }

        // set archive name
        SetWindowText(GetDlgItem(HWindow, IDC_NA_ARCHIVE), Archive);

        // defaultne soubory nekryptujeme
        EnableWindow(GetDlgItem(HWindow, IDC_NA_PASSWORD), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_NA_PASSWORD_TXT), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_NA_CONFIRMPASSWORD), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_NA_CONFIRMPASSWORD_TXT), FALSE);

        CompressParamsDlg.HWindow = HWindow;

        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDOK)
        {
            // zkontrolovat hesla
            if (TransferData(ttDataFromWindow))
                if (Encrypt)
                {
                    if (lstrlen(Password) <= 0)
                    {
                        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_EMPTYPASSWORD), LoadStr(IDS_ERROR),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        return TRUE;
                    }

                    if (lstrcmp(Password, ConfirmedPassword) != 0)
                    {
                        SalamanderGeneral->SalMessageBox(HWindow, LoadStr(IDS_PASSWORDSNOTMATCH), LoadStr(IDS_ERROR),
                                                         MB_OK | MB_ICONEXCLAMATION);
                        return TRUE;
                    }
                }
        }
        else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_NA_ENCRYPTFILES)
        {
            BOOL enable = IsDlgButtonChecked(HWindow, IDC_NA_ENCRYPTFILES) == BST_CHECKED;

            EnableWindow(GetDlgItem(HWindow, IDC_NA_PASSWORD), enable);
            EnableWindow(GetDlgItem(HWindow, IDC_NA_PASSWORD_TXT), enable);
            EnableWindow(GetDlgItem(HWindow, IDC_NA_CONFIRMPASSWORD), enable);
            EnableWindow(GetDlgItem(HWindow, IDC_NA_CONFIRMPASSWORD_TXT), enable);
        }
        else if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SHOW_CONFIG)
        {
            if (TransferData(ttDataFromWindow))
            {
                Config.ShowExtendedOptions = !GetNotAgain();
                PluginInterface.Configuration(HWindow);
                NotAgain = !Config.ShowExtendedOptions;
                TransferData(ttDataToWindow);
            }
        }
        break;
    }
    } // switch

    CompressParamsDlg.DialogProc(uMsg, wParam, lParam);

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CEnterPasswordDialog
//

CEnterPasswordDialog::CEnterPasswordDialog(HWND hParent)
    : CCommonDialog(HLanguage, IDD_ENTERPASSWORD, hParent)
{
    Password[0] = '\0';
    //  FileName[0] = '\0';
}

void CEnterPasswordDialog::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDC_PASSWORD, Password, PASSWORD_LEN);
}

INT_PTR
CEnterPasswordDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SendDlgItemMessage(HWindow, IDC_LOCK_ICON, STM_SETIMAGE, IMAGE_ICON,
                           (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_LOCK)));
        break;
    }

    case WM_COMMAND:
    {
        /*
      // priprava pro skip a skip_all
      switch (LOWORD(wParam))
      {
        case IDC_SKIP: return EndDialog(HWindow, DIALOG_SKIP);
        case IDC_SKIPALL: return EndDialog(HWindow, DIALOG_SKIPALL);
      }
*/
        break;
    }

    } // switch

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
