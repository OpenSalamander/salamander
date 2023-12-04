// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "splitcbn.h"
#include "splitcbn.rh"
#include "splitcbn.rh2"
#include "lang\lang.rh"
#include "dialogs.h"
#include "split.h"
#include "combine.h"

// *****************************************************************************
//
//  SPLIT DIALOG
//

#define STRICT_SYNTAX // nepovoli kraviny v COMBO_SIZE

namespace split
{

    static LPTSTR pszFileName;
    static CQuadWord qwFileSize;
    static LPTSTR pszTargetDir;
    static CQuadWord* pqwPartialSize;

    static HWND hDialog;
    static BOOL bDontHandleEdit = FALSE;

    static CQuadWord GetSizeOfLastPart(const CQuadWord& filesize, const CQuadWord& partsize)
    {
        CALL_STACK_MESSAGE3("GetSizeOfLastPart(%I64u, %I64u)", filesize.Value, partsize.Value);
        if (!partsize.Value)
        {
            TRACE_E("S/C.GetSizeOfLastPart: partsize == 0 ?!?");
            return CQuadWord(0, 0); // ochrana pred delenim nulou, nemelo by nastat
        }
        CQuadWord last;
        last.Value = filesize.Value % partsize.Value;
        return last.Value ? last : partsize;
    }

    static void OnSizeChange(const CQuadWord& size)
    {
        CALL_STACK_MESSAGE2("OnSizeChange(%I64u)", size.Value);
        bDontHandleEdit = TRUE;
        *pqwPartialSize = size;
        if (size != SIZE_AUTODETECT)
        {
            char text[100];
            SalamanderGeneral->PrintDiskSize(text, GetSizeOfLastPart(qwFileSize, size), 1);
            SetDlgItemText(hDialog, IDC_EDIT_LASTPART, text);
            if (size.Value)
            {
                sprintf(text, "%I64u", ((qwFileSize - CQuadWord(1, 0)) / size + CQuadWord(1, 0)).Value);
                SetDlgItemText(hDialog, IDC_EDIT_NUMBER, text);
            }
        }
        else
        {
            SetDlgItemText(hDialog, IDC_EDIT_LASTPART, "");
            SetDlgItemText(hDialog, IDC_EDIT_NUMBER, "");
        }
        bDontHandleEdit = FALSE;
    }

    static void OnComboSelChange()
    {
        CALL_STACK_MESSAGE1("OnComboSelChange()");
        int cursel = (int)SendMessage(GetDlgItem(hDialog, IDC_COMBO_SIZE), CB_GETCURSEL, 0, 0);
        if (cursel == CB_ERR)
            return;
        switch (cursel)
        {
        case 0:
            OnSizeChange(CQuadWord(1457664, 0));
            break; // 1.44 MB Floppy
        case 1:
            OnSizeChange(CQuadWord(730112, 0));
            break; // 720 KB Floppy
        case 2:
            OnSizeChange(CQuadWord(1213952, 0));
            break; // 1.2 MB Floppy
        case 3:
            OnSizeChange(CQuadWord(362496, 0));
            break; // 360 KB Floppy
        case 4:
            OnSizeChange(CQuadWord(100431872, 0));
            break; // 100 MB ZIP
        case 5:
            OnSizeChange(CQuadWord(250331136, 0));
            break; // 250 MB ZIP
        case 6:
            OnSizeChange(CQuadWord(125829120, 0));
            break; // 120 MB LS-120
        case 7:
            OnSizeChange(CQuadWord(650 * 1024 * 1024, 0));
            break; // 650 MB
        case 8:
            OnSizeChange(CQuadWord(700 * 1024 * 1024, 0));
            break; // 700 MB
        case 9:
            OnSizeChange(SIZE_AUTODETECT);
            break;
        }
        EnableWindow(GetDlgItem(hDialog, IDOK), TRUE);
    }

    static void OnComboEditChange()
    {
        CALL_STACK_MESSAGE1("OnComboEditChange()");
        char text[100], number[100];
        GetDlgItemText(hDialog, IDC_COMBO_SIZE, text, 100);

        // vypusteni mezer a tabulatoru a nahrezeni carky teckou
        int i = 0, j = 0;
        while (text[i] && (strchr(" \t0123456789.,", text[i]) != NULL || (BYTE)text[i] == 0xA0))
        {
            if (text[i] != ' ' && text[i] != '\t' && (BYTE)text[i] != 0xA0)
                number[j++] = (text[i] == ',') ? '.' : text[i];
            i++;
        }
        number[j] = 0;

        j = i;
        while (text[j] && strchr(" \t()[]{}.;,", text[j]) == NULL)
            j++;
        text[j] = 0;

        // ziskani multiplikatoru
        DWORD multiplier = 1;
        BOOL ok = TRUE;
        if (!lstrcmpi(text + i, LoadStr(IDS_SIZE_KB)) || !lstrcmpi(text + i, LoadStr(IDS_SIZE_K)))
            multiplier = 1024;
        else if (!lstrcmpi(text + i, LoadStr(IDS_SIZE_MB)))
            multiplier = 1024 * 1024;
        else if (!lstrcmpi(text + i, LoadStr(IDS_SIZE_GB)))
            multiplier = 1024 * 1024 * 1024;
#ifdef STRICT_SYNTAX
        else if (text[i] && lstrcmpi(text + i, LoadStr(IDS_SIZE_B)) && lstrcmpi(text + i, LoadStr(IDS_BYTES)) && lstrcmpi(text + i, LoadStr(IDS_BYTE)))
            ok = FALSE;
#endif

        double size;
        if (ok && sscanf(number, "%lf", &size) == 1 && size > 0 && (size * multiplier < (double)0xffffffffffffffff))
        {
            OnSizeChange(CQuadWord().SetDouble(size * multiplier));
            EnableWindow(GetDlgItem(hDialog, IDOK), TRUE);
        }
        else
        {
            bDontHandleEdit = TRUE;
            SetDlgItemText(hDialog, IDC_EDIT_NUMBER, "?");
            SetDlgItemText(hDialog, IDC_EDIT_LASTPART, "?");
            EnableWindow(GetDlgItem(hDialog, IDOK), FALSE);
            bDontHandleEdit = FALSE;
        }
    }

    static void OnEditChange()
    {
        CALL_STACK_MESSAGE1("OnEditChange()");
        if (bDontHandleEdit)
            return;
        char text[100];
        GetDlgItemText(hDialog, IDC_EDIT_NUMBER, text, _countof(text));
        int parts = atol(text);
        if (CQuadWord(2 * parts, 0) > qwFileSize)
        {
            parts = (int)(qwFileSize.Value / 2);
            bDontHandleEdit = TRUE;
            SetDlgItemText(hDialog, IDC_EDIT_NUMBER, _ltoa(parts, text, 10));
            bDontHandleEdit = FALSE;
        }
        if (parts > 0)
        {
            CQuadWord size = (qwFileSize + CQuadWord(parts - 1, 0)) / CQuadWord(parts, 0); // zaokrouhlujeme nahoru
            *pqwPartialSize = size;
            SalamanderGeneral->NumberToStr(text, size);
            //SalamanderGeneral->PrintDiskSize(text, size, 1);
            SetDlgItemText(hDialog, IDC_COMBO_SIZE, text);
            SalamanderGeneral->PrintDiskSize(text, GetSizeOfLastPart(qwFileSize, size), 1);
            SetDlgItemText(hDialog, IDC_EDIT_LASTPART, text);
            EnableWindow(GetDlgItem(hDialog, IDOK), TRUE);
        }
        else
        {
            SetDlgItemText(hDialog, IDC_COMBO_SIZE, "");
            SetDlgItemText(hDialog, IDC_EDIT_LASTPART, "?");
            EnableWindow(GetDlgItem(hDialog, IDOK), FALSE);
        }
    }

    static INT_PTR CALLBACK SplitDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CALL_STACK_MESSAGE4("SplitDlgProc( , 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
        char text[MAX_PATH + 100];

        switch (uMsg)
        {
        case WM_INITDIALOG:
        {
            hDialog = hWnd;
            SalamanderGUI->ArrangeHorizontalLines(hWnd);
            CenterWindow(hWnd);

            SendDlgItemMessage(hWnd, IDC_SPLIT_ICON, STM_SETIMAGE, IMAGE_ICON,
                               (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_SPLIT)));

            char number[50];
            sprintf(text, LoadStr(IDS_SPLITTITLE), SalamanderGeneral->NumberToStr(number, qwFileSize));
            SalamanderGUI->SetSubjectTruncatedText(GetDlgItem(hWnd, IDC_STATIC_TITLE), text,
                                                   pszFileName, FALSE, FALSE);

            SetDlgItemText(hWnd, IDC_EDIT_DIR, pszTargetDir);
            CheckDlgButton(hWnd, IDC_RADIO_SIZE, BST_CHECKED);
            SendMessage(hWnd, WM_COMMAND, IDC_RADIO_SIZE, 0);
            SendMessage(GetDlgItem(hWnd, IDC_EDIT_NUMBER), EM_SETLIMITTEXT, 3, 0);

            HWND h = GetDlgItem(hWnd, IDC_COMBO_SIZE);
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_144FLOPPY));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_720FLOPPY));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_12FLOPPY));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_360FLOPPY));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_100MB_ZIP));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_250MB_ZIP));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_120MB_LS120));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_650MB_CDR));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_700MB_CDR));
            SendMessage(h, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_AUTODETECT));
            SendMessage(h, CB_SETCURSEL, 0, 0);
            OnComboSelChange();
            SetFocus(h);

            /*HWND h = GetDlgItem(hWnd, IDC_EDITNUMBER);
      LONG style = GetWindowLong(h, GWL_STYLE);
      SetWindowLong(h, GWL_STYLE, style | WS_VSCROLL);*/
            return FALSE;
        }

        case WM_HELP:
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_SPLIT, FALSE);
            return TRUE; // F1 nenechame propadnout do parenta ani pokud nezobrazujeme help
        }

        case WM_COMMAND:
            switch (LOWORD(wParam))
            {
            case IDHELP:
            {
                if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                    SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_SPLIT, FALSE);
                return TRUE;
            }

            case IDOK:
                GetDlgItemText(hWnd, IDC_EDIT_DIR, pszTargetDir, MAX_PATH);
                EndDialog(hWnd, TRUE);
                break;

            case IDCANCEL:
                EndDialog(hWnd, FALSE);
                break;

            case IDC_RADIO_NUMBER:
            case IDC_RADIO_SIZE:
            {
                BOOL bSize = (LOWORD(wParam) == IDC_RADIO_SIZE);
                EnableWindow(GetDlgItem(hWnd, IDC_COMBO_SIZE), bSize);
                EnableWindow(GetDlgItem(hWnd, IDC_EDIT_NUMBER), !bSize);
                if (!bSize)
                    if (*pqwPartialSize == SIZE_AUTODETECT)
                        SetDlgItemText(hWnd, IDC_EDIT_NUMBER, "1");
                    else
                        OnEditChange();
                break;
            }

            case IDC_COMBO_SIZE:
                switch (HIWORD(wParam))
                {
                case CBN_EDITCHANGE:
                    OnComboEditChange();
                    break;
                case CBN_SELCHANGE:
                    OnComboSelChange();
                    break;
                }
                break;

            case IDC_EDIT_NUMBER:
                if (HIWORD(wParam) == EN_CHANGE)
                    OnEditChange();
                break;

            case IDC_BUTTON_BROWSE:
            {
                HWND parent = SalamanderGeneral->GetMsgBoxParent();
                GetDlgItemText(hWnd, IDC_EDIT_DIR, text, MAX_PATH);
                if (SalamanderGeneral->GetTargetDirectory(hWnd, parent, LoadStr(IDS_SPLIT),
                                                          LoadStr(IDS_SELECTDIR), text, FALSE, text))
                    SetDlgItemText(hWnd, IDC_EDIT_DIR, text);
                break;
            }

            case IDC_BUTTON_CONFIG:
            {
                BOOL oldSplitToOther = configSplitToOther;
                BOOL oldSplitToSubdir = configSplitToSubdir;
                ConfigDialog(hWnd);
                if (oldSplitToOther != configSplitToOther || oldSplitToSubdir != configSplitToSubdir)
                {
                    GetTargetDir(text, pszFileName, TRUE);
                    SetDlgItemText(hWnd, IDC_EDIT_DIR, text);
                }
                break;
            }
            }
            return TRUE;

        default:
            return FALSE;
        }
    }

} // namespace split
using namespace split;

BOOL SplitDialog(LPTSTR fileName, CQuadWord& fileSize, LPTSTR targetDir,
                 CQuadWord* partialSize, HWND hParent)
{
    CALL_STACK_MESSAGE4("SplitDialog(%s, %I64u, %s, , )", fileName, fileSize.Value, targetDir);
    pszFileName = fileName;
    qwFileSize = fileSize;
    pszTargetDir = targetDir;
    pqwPartialSize = partialSize;
    return (BOOL)DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_SPLIT), hParent, SplitDlgProc, 0);
}

// *****************************************************************************
//
//  COMBINE DIALOG
//

namespace combine
{

    static TIndirectArray<char>* files;
    static LPTSTR targetName;
    static BOOL bOrigCrcFound;
    static UINT32 origCrc;
    static UINT uDragMsg;
    static HWND hDialog, hLB;
    static int DragIndex = -1;
    static int currentIndex = -1;
    static HICON hFileIcon;
    static CSalamanderForOperationsAbstract* salamander;

    struct ITEMDATA
    {
        int index;
        //HICON hIcon;
        char text[MAX_PATH];
    };

    static BOOL AddFile(LPTSTR fullName, BOOL bUpdateArray = TRUE)
    {
        CALL_STACK_MESSAGE3("AddFile(%s, %ld)", fullName, bUpdateArray);

        if (bUpdateArray)
        {
            char* dup = _strdup(fullName);
            if (dup == NULL)
            {
                SalamanderGeneral->SalMessageBox(hDialog, LoadStr(IDS_OUTOFMEM), LoadStr(IDS_COMBINE),
                                                 MB_OK | MB_ICONEXCLAMATION);
                return FALSE;
            }
            files->Add(dup);
        }

        char dir[MAX_PATH];
        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, dir, MAX_PATH, NULL, NULL);
        const char* name = SalamanderGeneral->SalPathFindFileName(fullName);
        ITEMDATA* pid = new ITEMDATA;
        if ((name - fullName - 1) == (int)strlen(dir) && !_memicmp(dir, fullName, name - fullName - 1))
            strcpy(pid->text, name);
        else
            strcpy(pid->text, fullName);
        pid->index = (int)SendMessage(hLB, LB_GETCOUNT, 0, 0) - 1;
        SendMessage(hLB, LB_INSERTSTRING, pid->index, 1);
        /*SHFILEINFO sfi;
  SHGetFileInfo(fullName, 0, &sfi, sizeof(sfi), SHGFI_ICON | SHGFI_SMALLICON);
  pid->hIcon = sfi.hIcon;*/
        SendMessage(hLB, LB_SETITEMDATA, pid->index, (LPARAM)pid);

        SendMessage(hLB, LB_SETCURSEL, currentIndex = pid->index, 0);
        return TRUE;
    }

    static void MoveItem(int oldindex, int newindex)
    {
        CALL_STACK_MESSAGE3("MoveItem(%ld, %ld)", oldindex, newindex);
        if (oldindex < newindex)
            newindex--;
        if (oldindex == newindex)
            return;

        char* olditem = (*files)[oldindex];
        char** data = (char**)files->GetData();
        if (oldindex > newindex)
            memmove(data + newindex + 1, data + newindex, (oldindex - newindex) * sizeof(char*));
        else
            memmove(data + oldindex, data + oldindex + 1, (newindex - oldindex) * sizeof(char*));
        (*files)[newindex] = olditem;

        SendMessage(hLB, WM_SETREDRAW, FALSE, 0);
        ITEMDATA* pid = (ITEMDATA*)SendMessage(hLB, LB_GETITEMDATA, oldindex, 0);
        SendMessage(hLB, LB_DELETESTRING, oldindex, 0);
        SendMessage(hLB, LB_INSERTSTRING, newindex, 1);
        SendMessage(hLB, LB_SETITEMDATA, newindex, (LPARAM)pid);
        SendMessage(hLB, LB_SETCURSEL, newindex, 0);
        SendMessage(hLB, WM_SETREDRAW, TRUE, 0);
        currentIndex = newindex;
    }

    static HWND ClearDefaultStyle(int id)
    {
        HWND hCtrl = GetDlgItem(hDialog, id);
        LONG style = GetWindowLong(hCtrl, GWL_STYLE);
        SetWindowLong(hCtrl, GWL_STYLE, style & ~BS_DEFPUSHBUTTON);
        InvalidateRect(hCtrl, NULL, FALSE);
        return hCtrl;
    }

    static void EnableButtons()
    {
        CALL_STACK_MESSAGE1("EnableButtons()");
        EnableWindow(ClearDefaultStyle(IDC_BUTTON_UP),
                     currentIndex != LB_ERR && currentIndex > 0 && currentIndex < files->Count);
        EnableWindow(ClearDefaultStyle(IDC_BUTTON_DOWN),
                     currentIndex != LB_ERR && currentIndex < files->Count - 1);
        EnableWindow(ClearDefaultStyle(IDC_BUTTON_REMOVE),
                     currentIndex != LB_ERR && currentIndex < files->Count);
        EnableWindow(GetDlgItem(hDialog, IDC_BUTTON_CRC),
                     files->Count ? TRUE : FALSE);
        SendMessage(hDialog, DM_SETDEFID, IDOK, 0);
    }

    static void OnRemove(int index)
    {
        CALL_STACK_MESSAGE2("OnRemove(%ld)", index);
        files->Delete(index);
        delete (ITEMDATA*)SendMessage(hLB, LB_GETITEMDATA, index, 0);
        SendMessage(hLB, LB_DELETESTRING, index, 0);
        if (index && index >= files->Count)
            index--;
        SendMessage(hLB, LB_SETCURSEL, currentIndex = index, 0);
        EnableButtons();
    }

    static void OnAdd()
    {
        CALL_STACK_MESSAGE1("OnAdd()");
        OPENFILENAME ofn;
        char* filenames = new char[MAX_PATH * 100];
        if (filenames == NULL)
        {
            SalamanderGeneral->SalMessageBox(hDialog, LoadStr(IDS_OUTOFMEM), LoadStr(IDS_COMBINE),
                                             MB_OK | MB_ICONEXCLAMATION);
            return;
        }
        filenames[0] = 0;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hDialog;
        ofn.hInstance = HLanguage;
        char filter[100];
        strcpy(filter, LoadStr(IDS_ADDFILTER));
        memcpy(filter + strlen(filter) + 1, "*.*\0\0", 5);
        ofn.lpstrFilter = filter;
        ofn.lpstrCustomFilter = NULL;
        ofn.lpstrFile = filenames;
        ofn.nMaxFile = MAX_PATH * 100;
        ofn.lpstrTitle = LoadStr(IDS_ADDTITLE);
        char initdir[MAX_PATH];
        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, initdir, MAX_PATH, NULL, NULL);
        ofn.lpstrInitialDir = initdir;
        ofn.Flags = OFN_ALLOWMULTISELECT | OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_READONLY | OFN_NOCHANGEDIR;

        if (SalamanderGeneral->SafeGetOpenFileName(&ofn))
        {
            SendMessage(hLB, WM_SETREDRAW, FALSE, 0);
            int i = ofn.nFileOffset;
            if (i > 0)
                filenames[i - 1] = 0;
            while (filenames[i])
            {
                char fullname[MAX_PATH];
                strcpy(fullname, filenames);
                if (!SalamanderGeneral->SalPathAppend(fullname, filenames + i, MAX_PATH))
                {
                    SalamanderGeneral->SalMessageBox(hDialog, LoadStr(IDS_TOOLONGNAME2), LoadStr(IDS_COMBINE),
                                                     MB_OK | MB_ICONEXCLAMATION);
                    delete[] filenames;
                    return;
                }
                if (!AddFile(fullname))
                {
                    delete[] filenames;
                    return;
                }
                while (filenames[i])
                    i++;
                i++;
            }
            SendMessage(hLB, WM_SETREDRAW, TRUE, 0);
        }

        delete[] filenames;
    }

    static void OnBrowse()
    {
        CALL_STACK_MESSAGE1("OnBrowse()");
        OPENFILENAME ofn;
        char filename[MAX_PATH];
        filename[0] = 0;
        ZeroMemory(&ofn, sizeof(ofn));
        ofn.lStructSize = sizeof(ofn);
        ofn.hwndOwner = hDialog;
        ofn.hInstance = HLanguage;
        char filter[100];
        strcpy(filter, LoadStr(IDS_ADDFILTER));
        memcpy(filter + strlen(filter) + 1, "*.*\0\0", 5);
        ofn.lpstrFilter = filter;
        ofn.lpstrCustomFilter = NULL;
        ofn.lpstrFile = filename;
        ofn.nMaxFile = MAX_PATH;
        ofn.lpstrTitle = LoadStr(IDS_BROWSETITLE);
        char initdir[MAX_PATH];
        SalamanderGeneral->GetPanelPath(PANEL_SOURCE, initdir, MAX_PATH, NULL, NULL);
        ofn.lpstrInitialDir = initdir;
        ofn.Flags = OFN_PATHMUSTEXIST | OFN_EXPLORER | OFN_OVERWRITEPROMPT | OFN_NOCHANGEDIR;
        if (SalamanderGeneral->SafeGetSaveFileName(&ofn))
            SetDlgItemText(hDialog, IDC_EDIT_TARGET, filename);
    }

    static void OnCRC(HWND parent)
    {
        CALL_STACK_MESSAGE1("OnCRC( )");
        CRCDialog(*files, bOrigCrcFound, origCrc, parent, salamander);
    }

    static void UpdateHorizontalScrollbar(HWND hWnd)
    {
        int minWidth = 0;
        HFONT hFont = (HFONT)SendMessage(hWnd, WM_GETFONT, 0, 0);
        HDC hDC = GetDC(hWnd);
        HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
        int cnt = (int)SendMessage(hWnd, LB_GETCOUNT, 0, 0);

        int i;
        for (i = 0; i < cnt; i++)
        {
            SIZE sz;
            ITEMDATA* pid = (ITEMDATA*)SendMessage(hLB, LB_GETITEMDATA, i, 0);

            if (pid && (pid->index >= 0))
            {
                GetTextExtentPoint32(hDC, pid->text, (int)strlen(pid->text), &sz);
                if (sz.cx > minWidth)
                    minWidth = sz.cx;
            }
        }
        SendMessage(hLB, LB_SETHORIZONTALEXTENT, 23 /*icon*/ + 5 /*reasonable border*/ + minWidth, 0);
        SelectObject(hDC, hOldFont);
        ReleaseDC(hWnd, hDC);
    }

    static INT_PTR CALLBACK CombineDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        CALL_STACK_MESSAGE4("CombineDlgProc( , 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
        switch (uMsg)
        {
        case WM_INITDIALOG:
        {
            SalamanderGUI->ArrangeHorizontalLines(hWnd);
            CenterWindow(hDialog = hWnd);

            hLB = GetDlgItem(hWnd, IDC_LIST_FILES);
            MakeDragList(hLB);
            uDragMsg = RegisterWindowMessage(DRAGLISTMSGSTRING);

            ITEMDATA* pid = new ITEMDATA;
            pid->index = -1;
            SendMessage(hLB, LB_INSERTSTRING, 0, 1);
            SendMessage(hLB, LB_SETITEMDATA, 0, (LPARAM)pid);

            int i;
            for (i = 0; i < files->Count; i++)
                AddFile((*files)[i], FALSE);

            int minHeight = 16;
            HFONT hFont = (HFONT)SendMessage(hLB, WM_GETFONT, 0, 0);
            HDC hDC = GetDC(hWnd);
            TEXTMETRIC tm;
            HFONT hOldFont = (HFONT)SelectObject(hDC, hFont);
            GetTextMetrics(hDC, &tm);
            minHeight = max(tm.tmHeight, minHeight);
            SelectObject(hDC, hOldFont);
            ReleaseDC(hWnd, hDC);
            minHeight += 2;
            SendMessage(hLB, LB_SETITEMHEIGHT, 0, minHeight);

            SendMessage(hLB, LB_SETCURSEL, files->Count, 0);
            SendMessage(hLB, LB_SETCURSEL, 0, 0);
            SendMessage(hLB, LB_SETCURSEL, currentIndex = -1, 0);
            UpdateHorizontalScrollbar(hLB);

            HWND hEdit = GetDlgItem(hWnd, IDC_EDIT_TARGET);
            SetWindowText(hEdit, targetName);
            SetFocus(GetDlgItem(hWnd, IDC_EDIT_TARGET));
            SendMessage(hEdit, EM_SETSEL, 0, -1);

            hFileIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FILE), IMAGE_ICON,
                                         0, 0, LR_DEFAULTCOLOR);

            EnableButtons();
            return FALSE;
        }

        case WM_DESTROY:
        {
            int count = (int)SendMessage(hLB, LB_GETCOUNT, 0, 0);
            int i;
            for (i = 0; i < count; i++)
                delete (ITEMDATA*)SendMessage(hLB, LB_GETITEMDATA, i, 0);
            DestroyIcon(hFileIcon);
            return TRUE;
        }

        case WM_HELP:
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_COMBINE, FALSE);
            return TRUE; // F1 nenechame propadnout do parenta ani pokud nezobrazujeme help
        }

        case WM_COMMAND:
        {
            switch (LOWORD(wParam))
            {
            case IDHELP:
                if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                    SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_COMBINE, FALSE);
                break;

            case IDOK:
                GetDlgItemText(hWnd, IDC_EDIT_TARGET, targetName, MAX_PATH);
                EndDialog(hWnd, TRUE);
                break;

            case IDCANCEL:
                EndDialog(hWnd, FALSE);
                break;

            case IDC_BUTTON_UP:
                MoveItem(currentIndex, currentIndex - 1);
                EnableButtons();
                break;

            case IDC_BUTTON_DOWN:
                MoveItem(currentIndex, currentIndex + 2);
                EnableButtons();
                break;

            case IDC_BUTTON_ADD:
                OnAdd();
                UpdateHorizontalScrollbar(GetDlgItem(hWnd, IDC_LIST_FILES));
                EnableButtons();
                break;

            case IDC_BUTTON_REMOVE:
                OnRemove(currentIndex);
                UpdateHorizontalScrollbar(GetDlgItem(hWnd, IDC_LIST_FILES));
                break;

            case IDC_BUTTON_BROWSE2:
                OnBrowse();
                break;

            case IDC_BUTTON_CRC:
                OnCRC(hWnd);
                break;

            case IDC_LIST_FILES:
                if (HIWORD(wParam) == LBN_SELCHANGE)
                {
                    currentIndex = (int)SendMessage(hLB, LB_GETCURSEL, 0, 0);
                    EnableButtons();
                }
                break;
            }
            return TRUE;
        }

        case WM_VKEYTOITEM:
        {
            if (LOWORD(wParam) == VK_DELETE)
            {
                if (HIWORD(wParam) < files->Count)
                {
                    OnRemove(HIWORD(wParam));
                    EnableButtons();
                }
                return -2;
            }
            else
                return -1;
        }

        case WM_DRAWITEM:
        {
            DRAWITEMSTRUCT* dis = (DRAWITEMSTRUCT*)lParam;
            if (dis->itemID == -1)
                return TRUE;
            HDC hDC = dis->hDC;
            RECT r = dis->rcItem;
            ITEMDATA* pid = (ITEMDATA*)dis->itemData;
            BOOL selected = (dis->itemState & ODS_SELECTED);
            BOOL focused = selected && GetFocus() == GetDlgItem(hWnd, dis->CtlID);

            if (selected)
                FillRect(hDC, &r, (HBRUSH)(COLOR_HIGHLIGHT + 1));
            else
                FillRect(hDC, &r, (HBRUSH)(COLOR_WINDOW + 1));

            if (pid->index != -1)
            {
                DrawIconEx(hDC, r.left + 2, r.top + 1, /*pid->hIcon*/ hFileIcon, 16, 16, 0, NULL, DI_NORMAL);
                r.left += 21;
                SetTextColor(hDC, GetSysColor(selected ? COLOR_HIGHLIGHTTEXT : COLOR_WINDOWTEXT));
                SetBkMode(hDC, TRANSPARENT);
                DrawText(hDC, pid->text, -1, &r, DT_SINGLELINE | DT_LEFT | DT_VCENTER);
                r.left -= 21;
            }
            if (focused)
            {
                SetTextColor(hDC, RGB(0, 0, 0));
                DrawFocusRect(hDC, &r);
            }

            return TRUE;
        }

        default:
        {
            if (uMsg == uDragMsg && wParam == IDC_LIST_FILES)
            {
                DRAGLISTINFO* pdli = (DRAGLISTINFO*)lParam;
                switch (pdli->uNotification)
                {
                case DL_BEGINDRAG:
                {
                    DragIndex = LBItemFromPt(hLB, pdli->ptCursor, TRUE);
                    if (DragIndex < files->Count)
                        SetWindowLongPtr(hWnd, DWLP_MSGRESULT, TRUE);
                    else
                    {
                        DragIndex = -1;
                        SetWindowLongPtr(hWnd, DWLP_MSGRESULT, FALSE);
                    }
                    break;
                }

                case DL_DRAGGING:
                {
                    DrawInsert(hWnd, hLB, LBItemFromPt(hLB, pdli->ptCursor, TRUE));
                    SetCursor(LoadCursor(DLLInstance, MAKEINTRESOURCE(IDC_DRAG)));
                    SetWindowLongPtr(hWnd, DWLP_MSGRESULT, 0);
                    break;
                }

                case DL_DROPPED:
                {
                    int index;
                    if (DragIndex != -1 && (index = LBItemFromPt(hLB, pdli->ptCursor, TRUE)) != -1)
                    {
                        MoveItem(DragIndex, index);
                        DrawInsert(hWnd, hLB, -1);
                        EnableButtons();
                    }
                    break;
                }

                case DL_CANCELDRAG:
                {
                    DrawInsert(hWnd, hLB, -1);
                    DragIndex = -1;
                    break;
                }
                }
                return TRUE;
            }
            else
                return FALSE;
        }
        }
    }

} // namespace combine
using namespace combine;

BOOL CombineDialog(TIndirectArray<char>& f, LPTSTR t, BOOL b, UINT32 c, HWND hParent,
                   CSalamanderForOperationsAbstract* sal)
{
    CALL_STACK_MESSAGE4("CombineDialog( , %s, %ld, %X, , )", t, b, c);
    files = &f;
    targetName = t;
    bOrigCrcFound = b;
    origCrc = c;
    salamander = sal;
    uDragMsg = 0xffffffff;
    return (BOOL)DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_COMBINE), hParent, CombineDlgProc, 0);
}

// *****************************************************************************
//
//  CONFIG DIALOG
//

static INT_PTR CALLBACK ConfigDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ConfigDlgProc( , 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        SalamanderGUI->ArrangeHorizontalLines(hWnd);
        CenterWindow(hWnd);
        CheckDlgButton(hWnd, IDC_CHECK_INCLUDE, configIncludeFileExt ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_CREATE, configCreateBatchFile ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_SPLITOTHER, configSplitToOther ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_COMBINEOTHER, configCombineToOther ? BST_CHECKED : BST_UNCHECKED);
        CheckDlgButton(hWnd, IDC_CHECK_SPLITSUB, configSplitToSubdir /*&& configSplitToOther*/ ? BST_CHECKED : BST_UNCHECKED);
        //EnableWindow(GetDlgItem(hWnd, IDC_CHECK_SPLITSUB), configSplitToOther);
        return TRUE;

    case WM_HELP:
    {
        if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
            SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_CONFIG, FALSE);
        return TRUE; // F1 nenechame propadnout do parenta ani pokud nezobrazujeme help
    }

    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDHELP:
            if ((GetKeyState(VK_CONTROL) & 0x8000) == 0 && (GetKeyState(VK_SHIFT) & 0x8000) == 0)
                SalamanderGeneral->OpenHtmlHelp(hWnd, HHCDisplayContext, IDD_CONFIG, FALSE);
            break;

        case IDOK:
            configIncludeFileExt = IsDlgButtonChecked(hWnd, IDC_CHECK_INCLUDE) == BST_CHECKED;
            configCreateBatchFile = IsDlgButtonChecked(hWnd, IDC_CHECK_CREATE) == BST_CHECKED;
            configSplitToOther = IsDlgButtonChecked(hWnd, IDC_CHECK_SPLITOTHER) == BST_CHECKED;
            configCombineToOther = IsDlgButtonChecked(hWnd, IDC_CHECK_COMBINEOTHER) == BST_CHECKED;
            configSplitToSubdir = IsDlgButtonChecked(hWnd, IDC_CHECK_SPLITSUB) == BST_CHECKED;
            // FALL THROUGH
        case IDCANCEL:
            EndDialog(hWnd, 0);
            return TRUE;

            /*case IDC_CHECK_SPLITOTHER: 
        {
          BOOL splitother = IsDlgButtonChecked(hWnd, IDC_CHECK_SPLITOTHER) == BST_CHECKED;
          if (!splitother) CheckDlgButton(hWnd, IDC_CHECK_SPLITSUB, BST_UNCHECKED);
          EnableWindow(GetDlgItem(hWnd, IDC_CHECK_SPLITSUB), splitother);
          return TRUE;
        }*/
        }
        return TRUE;
    }
    return FALSE;
}

void ConfigDialog(HWND hParent)
{
    CALL_STACK_MESSAGE1("ConfigDialog()");
    DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONFIG), hParent, ConfigDlgProc, 0);
}

// *****************************************************************************
//
//  CRC DIALOG
//

static BOOL bCrcFound;
static UINT32 originalCrc, calcCrc;

static INT_PTR CALLBACK CRCDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRCDlgProc( , 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->ArrangeHorizontalLines(hWnd);
        CenterWindow(hWnd);
        char text[100];
        sprintf(text, LoadStr(IDS_CRCHEX), calcCrc);
        SetDlgItemText(hWnd, IDC_EDIT_CRC1, text);
        sprintf(text, LoadStr(IDS_CRCDEC), calcCrc);
        SetDlgItemText(hWnd, IDC_EDIT_CRC2, text);

        // !POZOR! ziskanou ikonu je treba ve WM_DESTROY destruovat
        HICON icon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_WARN), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        SendDlgItemMessage(hWnd, IDC_ICON_WARN, STM_SETIMAGE, IMAGE_ICON, (LPARAM)icon);
        icon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_OK), IMAGE_ICON, 16, 16, LR_DEFAULTCOLOR);
        SendDlgItemMessage(hWnd, IDC_ICON_OK, STM_SETIMAGE, IMAGE_ICON, (LPARAM)icon);

        if (bCrcFound)
        {
            sprintf(text, LoadStr(IDS_CRCHEX), originalCrc);
            SetDlgItemText(hWnd, IDC_EDIT_CRC3, text);
            sprintf(text, LoadStr(IDS_CRCDEC), originalCrc);
            SetDlgItemText(hWnd, IDC_EDIT_CRC4, text);
            ShowWindow(GetDlgItem(hWnd, calcCrc == originalCrc ? IDC_ICON_OK : IDC_ICON_WARN), SW_SHOW);
        }
        else
            SetDlgItemText(hWnd, IDC_EDIT_CRC3, LoadStr(IDS_CRCNOTFOUND));

        return TRUE;
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
            EndDialog(hWnd, 0);
        return TRUE;
    }

    case WM_DESTROY:
    {
        // ikona ziskana bez flagu LR_SHARED se musi destruovat
        HICON hIcon = (HICON)SendDlgItemMessage(hWnd, IDC_ICON_WARN, STM_SETIMAGE, IMAGE_ICON, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);
        hIcon = (HICON)SendDlgItemMessage(hWnd, IDC_ICON_OK, STM_SETIMAGE, IMAGE_ICON, NULL);
        if (hIcon != NULL)
            DestroyIcon(hIcon);
        return FALSE;
    }

    default:
        return FALSE;
    }
}

void CRCDialog(TIndirectArray<char>& files, BOOL bf, UINT32 oc, HWND parent,
               CSalamanderForOperationsAbstract* salamander)
{
    CALL_STACK_MESSAGE3("CRCDialog( , %ld, %X, , )", bf, oc);

    UINT32 cc;
    if (!CombineFiles(files, NULL, TRUE, FALSE, cc, FALSE, NULL, parent, salamander))
        return;

    bCrcFound = bf;
    originalCrc = oc;
    calcCrc = cc;

    DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CRC), parent, CRCDlgProc, 0);
}

// *****************************************************************************
//
//  FILE CRC DIALOG
//

/*static BOOL CALLBACK FileCRCDlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
  CALL_STACK_MESSAGE4("FileCRCDlgProc( , 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
  switch (uMsg)
  {
    case WM_INITDIALOG:
    {
      SalamanderGUI->ArrangeHorizontalLines(hWnd);
      CenterWindow(hWnd);
      char text[MAX_PATH];
      sprintf(text, LoadStr(IDS_FILECRCTITLE), 
        SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, NULL)->Name);
      SetDlgItemText(hWnd, IDC_STATIC_CRCTITLE, text);
      sprintf(text, LoadStr(IDS_CRCHEX), lParam);
      SetDlgItemText(hWnd, IDC_EDIT_CRC1, text);
      sprintf(text, LoadStr(IDS_CRCDEC), lParam);
      SetDlgItemText(hWnd, IDC_EDIT_CRC2, text);

      SendDlgItemMessage(hWnd, IDC_ICON_OK, STM_SETIMAGE, IMAGE_ICON,
                         (LPARAM)LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_OK)));

      return TRUE;
    }

    case WM_COMMAND:
    {
      if (LOWORD(wParam) == IDCANCEL) EndDialog(hWnd, 0);
      return TRUE;
    }

    default:
      return FALSE;
  }
}


void FileCRCDialog(HWND parent, CSalamanderForOperationsAbstract* salamander)
{
  CALL_STACK_MESSAGE1("FileCRCDialog( , )");
  UINT32 crc;
  if (!CalculateFileCRC(crc, parent, salamander)) return;
  DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_FILECRC), parent, FileCRCDlgProc, crc);
}*/
