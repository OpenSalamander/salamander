// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "dialogs.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "gui.h"
#include "shellib.h"

//****************************************************************************
//
// CViewerMasksItem
//

// toto cislo je stale zvetsovano - slouzi jako zdroj pro unikatni ID
DWORD ViewerHandlerID = 0;

CViewerMasksItem::CViewerMasksItem(const char* masks, const char* command, const char* arguments, const char* initDir,
                                   int viewerType, BOOL oldType)
{
    CALL_STACK_MESSAGE7("CViewerMasksItem(%s, %s, %s, %s, %d, %d)",
                        masks, command, arguments, initDir, viewerType, oldType);
    OldType = oldType;
    Masks = NULL;
    Command = Arguments = InitDir = NULL;
    ViewerType = viewerType;
    HandlerID = ViewerHandlerID++;
    Set(masks, command, arguments, initDir);
}

CViewerMasksItem::CViewerMasksItem()
{
    CALL_STACK_MESSAGE1("CViewerMasksItem()");
    Masks = NULL;
    Command = Arguments = InitDir = NULL;
    ViewerType = VIEWER_EXTERNAL;
    HandlerID = ViewerHandlerID++;
    OldType = FALSE;
    Set("", "", "\"$(Name)\"", "$(FullPath)");
}

CViewerMasksItem::CViewerMasksItem(CViewerMasksItem& item)
{
    CALL_STACK_MESSAGE1("CViewerMasksItem(&)");
    Masks = NULL;
    Command = Arguments = InitDir = NULL;
    ViewerType = item.ViewerType;
    OldType = item.OldType;
    HandlerID = item.HandlerID;
    Set(item.Masks->GetMasksString(), item.Command, item.Arguments, item.InitDir);
}

CViewerMasksItem::~CViewerMasksItem()
{
    if (Masks != NULL)
        delete Masks;
    if (Command != NULL)
        free(Command);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);
}

BOOL CViewerMasksItem::IsGood()
{
    return Masks != NULL && Command != NULL && Arguments != NULL && InitDir != NULL;
}

BOOL CViewerMasksItem::Set(const char* masks, const char* command, const char* arguments, const char* initDir)
{
    CALL_STACK_MESSAGE5("CViewerMasksItem::Set(%s, %s, %s, %s)", masks, command, arguments, initDir);

    char* commandName = (char*)malloc(strlen(command) + 1);
    char* argumentsName = (char*)malloc(strlen(arguments) + 1);
    char* initDirName = (char*)malloc(strlen(initDir) + 1);

    if (Masks == NULL)
        Masks = new CMaskGroup;
    if (Masks == NULL || commandName == NULL || argumentsName == NULL || initDirName == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }

    Masks->SetMasksString(masks);
    strcpy(commandName, command);
    strcpy(argumentsName, arguments);
    strcpy(initDirName, initDir);

    if (Command != NULL)
        free(Command);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);

    Command = commandName;
    Arguments = argumentsName;
    InitDir = initDirName;

    return TRUE;
}

BOOL CViewerMasks::Load(CViewerMasks& source)
{
    CALL_STACK_MESSAGE1("CViewerMasks::Load()");
    CViewerMasksItem* item;
    DestroyMembers();
    int i;
    for (i = 0; i < source.Count; i++)
    {
        item = new CViewerMasksItem(*source[i]);
        if (!item->IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        Add(item);
        if (!IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    return TRUE;
}

//****************************************************************************
//
// CEditorMasksItem
//

// toto cislo je stale zvetsovano - slouzi jako zdroj pro unikatni ID
DWORD EditorHandlerID = 0;

CEditorMasksItem::CEditorMasksItem(char* masks, char* command, char* arguments, char* initDir)
{
    CALL_STACK_MESSAGE5("CEditorMasksItem(%s, %s, %s, %s)", masks, command, arguments, initDir);
    Masks = new CMaskGroup;
    Command = Arguments = InitDir = NULL;
    HandlerID = EditorHandlerID++;
    Set(masks, command, arguments, initDir);
}

CEditorMasksItem::CEditorMasksItem()
{
    CALL_STACK_MESSAGE1("CEditorMasksItem()");
    Masks = new CMaskGroup;
    Command = Arguments = InitDir = NULL;
    HandlerID = EditorHandlerID++;
    Set("", "", "\"$(Name)\"", "$(FullPath)");
}

CEditorMasksItem::CEditorMasksItem(CEditorMasksItem& item)
{
    CALL_STACK_MESSAGE1("CEditorMasksItem(&)");
    Masks = new CMaskGroup;
    Command = Arguments = InitDir = NULL;
    HandlerID = item.HandlerID;
    Set(item.Masks->GetMasksString(), item.Command, item.Arguments, item.InitDir);
}

CEditorMasksItem::~CEditorMasksItem()
{
    if (Masks != NULL)
        delete Masks;
    if (Command != NULL)
        free(Command);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);
}

BOOL CEditorMasksItem::Set(const char* masks, const char* command, const char* arguments, const char* initDir)
{
    CALL_STACK_MESSAGE5("CEditorMasksItem::Set(%s, %s, %s, %s)", masks, command, arguments, initDir);
    char* commandName = (char*)malloc(strlen(command) + 1);
    char* argumentsName = (char*)malloc(strlen(arguments) + 1);
    char* initDirName = (char*)malloc(strlen(initDir) + 1);
    if (commandName == NULL || argumentsName == NULL || initDirName == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    if (Masks != NULL)
        Masks->SetMasksString(masks);
    strcpy(commandName, command);
    strcpy(argumentsName, arguments);
    strcpy(initDirName, initDir);

    if (Command != NULL)
        free(Command);
    if (Arguments != NULL)
        free(Arguments);
    if (InitDir != NULL)
        free(InitDir);

    Command = commandName;
    Arguments = argumentsName;
    InitDir = initDirName;
    return TRUE;
}

BOOL CEditorMasksItem::IsGood()
{
    return Masks != NULL && Command != NULL && Arguments != NULL && InitDir != NULL;
}

BOOL CEditorMasks::Load(CEditorMasks& source)
{
    CALL_STACK_MESSAGE1("CEditorMasks::Load()");
    CEditorMasksItem* item;
    DestroyMembers();
    int i;
    for (i = 0; i < source.Count; i++)
    {
        item = new CEditorMasksItem(*source[i]);
        if (!item->IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
        Add(item);
        if (!IsGood())
        {
            delete item;
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// CCommonDialog
//

void CCommonDialog::NotifDlgJustCreated()
{
    ArrangeHorizontalLines(HWindow);
}

INT_PTR
CCommonDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // predejdeme refreshum panelu na pozadi otevrenych modalnich dialogu a messageboxu
        if (Modal && MainWindow != NULL && Parent != NULL && Parent == MainWindow->HWindow)
        {
            BeginStopRefresh(FALSE, TRUE); // cmuchal si da pohov
            CallEndStopRefresh = TRUE;
        }
        else
            CallEndStopRefresh = FALSE;

        // pri otevirani dialogu nastavime msgbox parenta pro plug-iny na tento dialog (jen hlavni thread)
        if (Modal && MainThreadID == GetCurrentThreadId())
        {
            HOldPluginMsgBoxParent = PluginMsgBoxParent;
            PluginMsgBoxParent = HWindow;
        }

        HWND hCenterBy;
        if (HCenterAgains != NULL)
            hCenterBy = HCenterAgains;
        else
            hCenterBy = Parent;

        if (hCenterBy != NULL)
            MultiMonCenterWindow(HWindow, hCenterBy, TRUE);
        else
            MultiMonCenterWindow(HWindow, NULL, FALSE);

        break;
    }

        /* j.r.: varianta pres VK_ESCAPE se mi zda lepsi, protoze kliknuti na IDCANCEL nezpusobi nastaveni promenne
    case WM_COMMAND:
    {
      if (LOWORD(wParam) == IDCANCEL) // opatreni, aby se neprerusoval listing v panelu po kazdem ESC
        WaitForESCReleaseBeforeTestingESC = TRUE;
      break;
    }
    */

    case WM_DESTROY:
    {
        if (GetKeyState(VK_ESCAPE) & 0x8000) // opatreni, aby se neprerusoval listing v panelu po kazdem ESC
            WaitForESCReleaseBeforeTestingESC = TRUE;

        // zavira se dialog - user v nem mohl zmenit clipboard
        // (hodit do nej text v editline,...), overime to ...
        IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
        IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

        // pri zavirani dialogu obnovime msgbox parenta pro plug-iny
        if (HOldPluginMsgBoxParent != NULL)
            PluginMsgBoxParent = HOldPluginMsgBoxParent;

        if (CallEndStopRefresh)
        {
            EndStopRefresh(TRUE, FALSE, TRUE); // ted uz zase cmuchal nastartuje
            CallEndStopRefresh = FALSE;
        }
        break;
    }
    }

    return CDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CCommonPropSheetPage
//

void CCommonPropSheetPage::NotifDlgJustCreated()
{
    ArrangeHorizontalLines(HWindow);
}

//
// ****************************************************************************
// CSizeResultsDlg
//

CSizeResultsDlg::CSizeResultsDlg(HWND parent, const CQuadWord& size, const CQuadWord& compressed,
                                 const CQuadWord& occupied, int files, int dirs, TDirectArray<CQuadWord>* sizes)
    : CCommonDialog(HLanguage, IDD_SIZERESULTS, IDD_SIZERESULTS, parent)
{
    Size = size;
    Compressed = compressed;
    Occupied = occupied;
    Files = files;
    Dirs = dirs;
    Sizes = sizes;
}

void CSizeResultsDlg::UpdateEstimate()
{
    char buf[100];
    SendDlgItemMessage(HWindow, IDC_EST_CLUSTER, WM_GETTEXT, 11, (LPARAM)buf);
    int bytesPerCluster = atoi(buf);

    if (Sizes != NULL && Sizes->IsGood() && bytesPerCluster > 0)
    {
        if (Sizes->Count != Files)
            TRACE_E("Sizes array is not consistent with number of files.");

        CQuadWord estimated(0, 0);
        CQuadWord s;
        int i;
        for (i = 0; i < Sizes->Count; i++)
        {
            s = Sizes->At(i);
            estimated += s - ((s - CQuadWord(1, 0)) % CQuadWord(bytesPerCluster, 0)) +
                         CQuadWord(bytesPerCluster - 1, 0);
        }

        SetWindowText(GetDlgItem(HWindow, IDC_EST_SIZE), PrintDiskSize(buf, estimated, 1));

        if (estimated == CQuadWord(0, 0))
            strcpy(buf, "0 %");
        else
        {
            sprintf(buf, "%-1.4lg %%", 100 * Size.GetDouble() / estimated.GetDouble());
            PointToLocalDecimalSeparator(buf, _countof(buf));
        }
        SetWindowText(GetDlgItem(HWindow, IDC_EST_UTIL), buf);

        EnableWindow(GetDlgItem(HWindow, IDC_EST_SIZE), TRUE);
        EnableWindow(GetDlgItem(HWindow, IDC_EST_UTIL), TRUE);
    }
    else
    {
        EnableWindow(GetDlgItem(HWindow, IDC_EST_SIZE), FALSE);
        EnableWindow(GetDlgItem(HWindow, IDC_EST_UTIL), FALSE);
        SetWindowText(GetDlgItem(HWindow, IDC_EST_SIZE), UnknownText);
        SetWindowText(GetDlgItem(HWindow, IDC_EST_UTIL), UnknownText);
    }
}

INT_PTR
CSizeResultsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSizeResultsDlg::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        GetDlgItemText(HWindow, IDS_OCCUPIED, UnknownText, 100); // vytahnu retezec "unknown" pro pozdejsi pouziti

        char buf[100];

        SetWindowText(GetDlgItem(HWindow, IDS_FILESCOUNT), NumberToStr(buf, CQuadWord(Files, 0)));
        SetWindowText(GetDlgItem(HWindow, IDS_DIRSCOUNT), NumberToStr(buf, CQuadWord(Dirs, 0)));

        if (Occupied != CQuadWord(-1, -1))
        {
            SetWindowText(GetDlgItem(HWindow, IDS_OCCUPIED), PrintDiskSize(buf, Occupied, 1));
            if (Occupied == CQuadWord(0, 0))
                strcpy(buf, "0 %");
            else
            {
                double result = 100 * Size.GetDouble() / Occupied.GetDouble();
                // patch pro 2GB sparse (ridky) soubor, kde se zobrazovala hodnota 3,052e+006 % misto 3051757,83 %
                // pro hodnoty vetsi nez 1000 zde zobrazuje lg exponencialni tvar cisla a pouzijeme lf
                // pro mensi hodnoty je lg vyhodnejsi, protoze zobrazi napriklad 100 misto 100.00
                if (result > 1000)
                    sprintf(buf, "%-1.2lf %%", result);
                else
                    sprintf(buf, "%-1.4lg %%", result);
                PointToLocalDecimalSeparator(buf, _countof(buf));
            }
            SetWindowText(GetDlgItem(HWindow, IDS_DISKUTILIZATION), buf);
        }
        else
        {
            EnableWindow(GetDlgItem(HWindow, IDS_OCCUPIED), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDS_DISKUTILIZATION), FALSE);
        }

        SetWindowText(GetDlgItem(HWindow, IDS_SIZE), PrintDiskSize(buf, Size, 1));
        if (Compressed != CQuadWord(-1, -1))
        {
            SetWindowText(GetDlgItem(HWindow, IDS_COMPSIZE), PrintDiskSize(buf, Compressed, 1));
            if (Size == CQuadWord(0, 0))
            {
                strcpy(buf, "100 %");
            }
            else
            {
                sprintf(buf, "%-1.4lg %%", 100 * Compressed.GetDouble() / Size.GetDouble());
                PointToLocalDecimalSeparator(buf, _countof(buf));
            }
            SetWindowText(GetDlgItem(HWindow, IDS_COMPRATIO), buf);
        }
        else
        {
            EnableWindow(GetDlgItem(HWindow, IDS_COMPSIZE), FALSE);
            EnableWindow(GetDlgItem(HWindow, IDS_COMPRATIO), FALSE);
        }

        // naplnime combobox

        DWORD clusterSize = 2048; // nejvic se to asi bude pouzivat pro CDcka
        CFilesWindow* panel = MainWindow->GetNonActivePanel();
        if (panel->Is(ptDisk))
        {
            DWORD sectorsPerCluster, bytesPerSector, numberOfFreeClusters, totalNumberOfClusters;
            if (MyGetDiskFreeSpace(MainWindow->GetNonActivePanel()->GetPath(),
                                   &sectorsPerCluster, &bytesPerSector,
                                   &numberOfFreeClusters, &totalNumberOfClusters))
            {
                clusterSize = sectorsPerCluster * bytesPerSector;
            }
        }

        HWND hCombo = GetDlgItem(HWindow, IDC_EST_CLUSTER);
        SendMessage(hCombo, CB_RESETCONTENT, 0, 0);
        SendMessage(hCombo, CB_LIMITTEXT, 11, 0);

        int selIndex = -1;
        DWORD arr[] = {512, 1024, 2048, 4096, 8192, 16384, 32768, 65536, 131072, 262144, (DWORD)-1};
        int i;
        for (i = 0; arr[i] != -1; i++)
        {
            itoa(arr[i], buf, 10);
            SendMessage(hCombo, CB_ADDSTRING, 0, (LPARAM)buf);
            if (clusterSize == arr[i])
                selIndex = i;
        }

        if (selIndex != -1)
            SendMessage(hCombo, CB_SETCURSEL, selIndex, 0);
        else
        {
            itoa(clusterSize, buf, 10);
            SendMessage(hCombo, WM_SETTEXT, 0, (LPARAM)buf);
        }

        if (Sizes == NULL || !Sizes->IsGood())
            EnableWindow(hCombo, FALSE);

        UpdateEstimate();

        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == CBN_SELCHANGE)
        {
            PostMessage(HWindow, WM_COMMAND, MAKELPARAM(0, CBN_EDITCHANGE), 0);
        }
        if (HIWORD(wParam) == CBN_EDITCHANGE)
        {
            UpdateEstimate();
        }
        break;
    }
    }

    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CSelectDialog
//

void CSelectDialog::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSelectDialog::Validate()");
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_FILEMASK))
    {
        if (ti.Type == ttDataFromWindow)
        {
            char buf[MAX_PATH];
            strcpy(buf, Mask); // zaloha
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)Mask);
            CMaskGroup mask(Mask);
            int errorPos;
            if (!mask.PrepareMasks(errorPos))
            {
                SalMessageBox(HWindow, LoadStr(IDS_INCORRECTSYNTAX), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
                SetFocus(hWnd);
                SendMessage(hWnd, CB_SETEDITSEL, 0, MAKELPARAM(errorPos, errorPos + 1));
                ti.ErrorOn(IDE_FILEMASK);
            }
            strcpy(Mask, buf); // obnova
        }
    }
}

void CSelectDialog::Transfer(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CSelectDialog::Transfer()");
    char** history = Configuration.SelectHistory;
    HWND hWnd;
    if (ti.GetControl(hWnd, IDE_FILEMASK))
    {
        if (ti.Type == ttDataToWindow)
        {
            LoadComboFromStdHistoryValues(hWnd, history, SELECT_HISTORY_SIZE);
            SendMessage(hWnd, CB_LIMITTEXT, MAX_PATH - 1, 0);
            SendMessage(hWnd, WM_SETTEXT, 0, (LPARAM)Mask);
        }
        else
        {
            SendMessage(hWnd, WM_GETTEXT, MAX_PATH, (LPARAM)Mask);
            AddValueToStdHistoryValues(history, SELECT_HISTORY_SIZE, Mask, FALSE);
        }
    }
}

INT_PTR
CSelectDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CSelectDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        InstallWordBreakProc(GetDlgItem(HWindow, IDE_FILEMASK)); // instalujeme WordBreakProc do comboboxu

        CHyperLink* hl = new CHyperLink(HWindow, IDC_FILEMASK_HINT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_MASKS_HINT));

        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CImportConfigDialog
//

CImportConfigDialog::CImportConfigDialog()
    : CCommonDialog(HLanguage, IDD_IMPORTCONFIG, NULL)
{
}

CImportConfigDialog::~CImportConfigDialog()
{
}

extern const char* SalamanderConfigurationVersions[SALCFG_ROOTS_COUNT];

void CImportConfigDialog::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        char buff[5000];
        char buff2[5000];

        // CAPTION: Welcome to %s
        GetWindowText(HWindow, buff, 5000);
        _snprintf_s(buff2, _TRUNCATE, buff, SALAMANDER_TEXT_VERSION);
        SetWindowText(HWindow, buff2);

        // COMBOBOX Import Configuration
        SendDlgItemMessage(HWindow, IDC_IMPORTCONFIG, CB_ADDSTRING, 0, (LPARAM)LoadStr(IDS_IMPORTCFG_DEFCFG));
        int selIndex = 0; // pokud nebude nic lepsiho, bude polozka default
        int i;
        for (i = 0; i < SALCFG_ROOTS_COUNT; i++)
        {
            if (ConfigurationExist[i])
            {
                // zjistime, zda jde o "Open Salamander", "Altap Salamander" nebo o stary "Servant Salamander"
                BOOL openSalamander = StrIStr(SalamanderConfigurationRoots[i], "Open Salamander") != NULL;
                BOOL altapSalamander = StrIStr(SalamanderConfigurationRoots[i], "Altap Salamander") != NULL;
                const char* name = openSalamander    ? "Open Salamander %s"
                                   : altapSalamander ? "Altap Salamander %s"
                                                     : "Servant Salamander %s";
                sprintf(buff, name, SalamanderConfigurationVersions[i]);
                SendDlgItemMessage(HWindow, IDC_IMPORTCONFIG, CB_ADDSTRING, 0, (LPARAM)buff);
                if (selIndex == 0)
                    selIndex = 1; // posledni konfigurace bude default
            }
        }
        if (selIndex == 0) // neni z ceho vybirat, disablujeme combobox
        {
            EnableWindow(GetDlgItem(HWindow, IDC_IMPORTCONFIG), FALSE);
        }
        SendDlgItemMessage(HWindow, IDC_IMPORTCONFIG, CB_SETCURSEL, selIndex, NULL);

        // LISTVIEW Remove Configuratuion
        HWND hListView = GetDlgItem(HWindow, IDC_REMOVECONFIG);
        selIndex = -1;
        int index = 0;
        for (i = 0; i < SALCFG_ROOTS_COUNT; i++)
        {
            if (ConfigurationExist[i])
            {
                LVITEM lvi;
                lvi.mask = LVIF_TEXT | LVIF_STATE;
                lvi.iItem = index;
                lvi.iSubItem = 0;
                lvi.state = 0;

                // zjistime, zda jde o "Open Salamander", "Altap Salamander" nebo o stary "Servant Salamander"
                BOOL openSalamander = StrIStr(SalamanderConfigurationRoots[i], "Open Salamander") != NULL;
                BOOL altapSalamander = StrIStr(SalamanderConfigurationRoots[i], "Altap Salamander") != NULL;
                const char* name = openSalamander    ? "Open Salamander %s"
                                   : altapSalamander ? "Altap Salamander %s"
                                                     : "Servant Salamander %s";
                sprintf(buff, name, SalamanderConfigurationVersions[i]);
                lvi.pszText = buff;
                ListView_InsertItem(hListView, &lvi);
                index++;
                if (selIndex == -1)
                {
                    DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
                    ListView_SetItemState(hListView, 0, state, state);
                    selIndex = 0;
                }
            }
        }
    }
    else
    {
        // COMBOBOX Import Configuration
        int sel = (int)SendDlgItemMessage(HWindow, IDC_IMPORTCONFIG, CB_GETCURSEL, 0, NULL);
        if (sel > 0)
        {
            sel--; // prvni je Don't import
            int index = 0;
            int i;
            for (i = 0; i < SALCFG_ROOTS_COUNT; i++)
            {
                if (ConfigurationExist[i])
                {
                    if (sel == index)
                    {
                        IndexOfConfigurationToLoad = i;
                        break;
                    }
                    index++;
                }
            }
        }

        // LISTVIEW Remove Configuratuion
        HWND hListView = GetDlgItem(HWindow, IDC_REMOVECONFIG);
        int itemsCount = ListView_GetItemCount(hListView);
        int index = 0;
        int i;
        for (i = 0; i < SALCFG_ROOTS_COUNT; i++)
        {
            if (ConfigurationExist[i])
            {
                DWORD state = ListView_GetItemState(hListView, index, LVIS_STATEIMAGEMASK);
                if (state == INDEXTOSTATEIMAGEMASK(2))
                    DeleteConfigurations[i] = TRUE;
                index++;
            }
        }
    }
}

INT_PTR
CImportConfigDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // pod W2K pri spousteni pres shortcut s MAXIMIZED nastavenim
        // se dialog zobrazoval maximalizovany; SC_RESTORE na to zabira
        INT_PTR ret = CCommonDialog::DialogProc(uMsg, wParam, lParam);
        SendMessage(HWindow, WM_SYSCOMMAND, SC_RESTORE, 0);

        // Checkboxes pro listview
        HWND hListView = GetDlgItem(HWindow, IDC_REMOVECONFIG);
        DWORD exFlags = LVS_EX_FULLROWSELECT | LVS_EX_CHECKBOXES;
        DWORD origFlags = ListView_GetExtendedListViewStyle(hListView);
        ListView_SetExtendedListViewStyle(hListView, origFlags | exFlags); // 4.71

        // naleju sloupec Name do listview se sloupci
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_FMT;
        char buff[] = "aa";
        lvc.pszText = buff;
        lvc.fmt = LVCFMT_LEFT;
        lvc.iSubItem = 0;
        ListView_InsertColumn(hListView, 0, &lvc);
        ListView_SetColumnWidth(hListView, 0, LVSCW_AUTOSIZE);

        return ret;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CLanguageSelectorDialog
//

CLanguageSelectorDialog::CLanguageSelectorDialog(HWND hParent, char* slgName, const char* pluginName)
    : CCommonDialog(NULL, pluginName == NULL ? IDD_SLGSELECTOR : IDD_SLGSELECTORPLUG, hParent), Items(5, 5)
{
    Web = NULL;
    SLGName = slgName;
    OpenedFromConfiguration = hParent != NULL && pluginName == NULL;
    OpenedForPlugin = pluginName != NULL;
    HListView = NULL;
    PluginName = pluginName;
    ExitButtonLabel[0] = 0;
}

CLanguageSelectorDialog::~CLanguageSelectorDialog()
{
    int i;
    for (i = 0; i < Items.Count; i++)
        Items[i].Free();
}

int CLanguageSelectorDialog::Execute()
{
    HINSTANCE hTmpLanguage = NULL;
    if (OpenedFromConfiguration || OpenedForPlugin)
    {
        // template vytahneme z bezici jazykove verze
        Modul = HLanguage;
    }
    else
    {
        // nacteme template z nejlepsiho dosupneho SLG
        int index = GetPreferredLanguageIndex(SLGName);
        char path[MAX_PATH];
        GetModuleFileName(HInstance, path, MAX_PATH);
        sprintf(strrchr(path, '\\') + 1, "lang\\%s", Items[index].FileName);
        hTmpLanguage = HANDLES(LoadLibrary(path));
        if (hTmpLanguage != NULL)
            Modul = hTmpLanguage;
    }
    if (!LoadString(Modul, IDS_SELLANGEXITBUTTON, ExitButtonLabel, 100))
        strcpy(ExitButtonLabel, "Exit");
    int ret = (int)CCommonDialog::Execute();
    if (hTmpLanguage != NULL)
    {
        Modul = NULL;
        HANDLES(FreeLibrary(hTmpLanguage));
    }

    return ret;
}

BOOL CLanguageSelectorDialog::GetSLGName(char* path, int index)
{
    if (index >= Items.Count)
        return FALSE;
    lstrcpy(path, Items[index].FileName);
    return TRUE;
}

BOOL CLanguageSelectorDialog::SLGNameExists(const char* slgName)
{
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        if (StrICmp(Items[i].FileName, slgName) == 0)
            return TRUE;
    }
    return FALSE;
}

void CLanguageSelectorDialog::FillControls()
{
    int index = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
    if (index != -1)
    {
        SetDlgItemTextW(HWindow, IDC_SLG_AUTHOR, Items[index].AuthorW);
        SetDlgItemText(HWindow, IDC_SLG_WEB, Items[index].Web);
        SetDlgItemTextW(HWindow, IDC_SLG_COMMENT, Items[index].CommentW);
        if (PluginName == NULL)
            SetDlgItemText(HWindow, IDC_SLG_HELPDIR, Items[index].HelpDir);
        if (Web != NULL)
        {
            char buff[300];
            sprintf(buff, "http://%s", Items[index].Web);
            Web->SetActionOpen(buff);
        }
    }
}

void CLanguageSelectorDialog::LoadListView()
{
    char buff[500];
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        LVITEM lvi;
        lvi.mask = 0;
        lvi.iItem = i;
        lvi.iSubItem = 0;
        ListView_InsertItem(HListView, &lvi);

        Items[i].GetLanguageName(buff, 200);
        ListView_SetItemText(HListView, i, 0, buff);
        sprintf(buff, "lang\\%s", Items[i].FileName);
        ListView_SetItemText(HListView, i, 1, buff);
    }

    int preferredIndex = GetPreferredLanguageIndex(SLGName);
    DWORD state = LVIS_SELECTED | LVIS_FOCUSED;
    ListView_SetItemState(HListView, preferredIndex, state, state);
    ListView_EnsureVisible(HListView, preferredIndex, FALSE);

    FillControls();
}

void CLanguageSelectorDialog::Transfer(CTransferInfo& ti)
{
    if (PluginName != NULL) // tenhle checkbox ukazeme jedine pri vyberu alternativniho jazyka pro plugin
        ti.CheckBox(IDC_USESAMESLGINOTHERPLUGINS, Configuration.UseAsAltSLGInOtherPlugins);

    if (ti.Type == ttDataToWindow)
    {
        LoadListView();

        // nechceme vodorovny scrollbar, takze napred plnime polozky a potom teprve nastavujeme sirky sloupcu
        RECT r;
        GetClientRect(HListView, &r);
        ListView_SetColumnWidth(HListView, 0, r.right / 1.6);
        ListView_SetColumnWidth(HListView, 1, LVSCW_AUTOSIZE_USEHEADER);
    }
    else
    {
        int index = ListView_GetNextItem(HListView, -1, LVIS_FOCUSED);
        if (index != -1)
        {
            lstrcpy(SLGName, Items[index].FileName);
            if (PluginName != NULL) // alternativni jmeno jazyka ukladame jen pri vyberu alternativniho jazyka pro plugin
            {
                if (Configuration.UseAsAltSLGInOtherPlugins)
                    lstrcpy(Configuration.AltPluginSLGName, SLGName);
                else
                    Configuration.AltPluginSLGName[0] = 0;
            }
        }
    }
}

BOOL CLanguageSelectorDialog::Initialize(const char* slgSearchPath, HINSTANCE pluginDLL)
{
    char path[MAX_PATH];
    if (slgSearchPath == NULL)
    {
        GetModuleFileName(NULL, path, MAX_PATH);
        lstrcpy(strrchr(path, '\\') + 1, "lang\\*.slg");
    }
    else
        lstrcpyn(path, slgSearchPath, MAX_PATH);

    WIN32_FIND_DATA file;
    HANDLE hFind = HANDLES_Q(FindFirstFile(path, &file));
    if (hFind != INVALID_HANDLE_VALUE)
    {
        do
        {
            char* point = strrchr(file.cFileName, '.');
            if (point != NULL && stricmp(point + 1, "slg") == 0) // vracelo nam to *.slg*
            {
                CLanguage lang;
                if (lang.Init(file.cFileName, pluginDLL))
                {
                    Items.Add(lang);
                    if (!Items.IsGood())
                    {
                        Items.ResetState();
                        lang.Free();
                        return FALSE;
                    }
                }
            }
        } while (FindNextFile(hFind, &file));
        HANDLES(FindClose(hFind));
    }
    return TRUE;
}

int CLanguageSelectorDialog::GetPreferredLanguageIndex(const char* selectSLGName, BOOL exactMatch)
{
    WORD langID = GetUserDefaultUILanguage();

    WORD primaryID = PRIMARYLANGID(langID);
    int localeIndex = -1;        // index odpovidajici loaklizaci podle uzivatele
    int primarylocaleIndex = -1; // index odpovidajici loaklizaci podle primarniho jazyka uzivatele
    int englishIndex = -1;       // index souboru "english.slg"
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        if (selectSLGName != NULL && stricmp(Items[i].FileName, selectSLGName) == 0)
            return i;
        if (localeIndex == -1 && Items[i].LanguageID == langID)
            localeIndex = i;
        if (primarylocaleIndex == -1 && PRIMARYLANGID(Items[i].LanguageID) == primaryID)
            primarylocaleIndex = i;
        if (stricmp(Items[i].FileName, "english.slg") == 0)
            englishIndex = i;
    }
    if (localeIndex == -1)
    {
        // pokud jsme nenasli jazyk presne odovidajici nastaveni usera
        if (primarylocaleIndex != -1)
        {
            // pokusime se priradit alespon primarni jazyk
            localeIndex = primarylocaleIndex;
        }
        else
        {
            if (!exactMatch)
            {
                if (englishIndex != -1)
                {
                    // neni-li ani ten, preferujeme anglickou verzi
                    localeIndex = englishIndex;
                }
                else
                {
                    // jinak uz berem co nam prijde pod ruku
                    localeIndex = 0;
                }
            }
        }
    }
    return localeIndex;
}

INT_PTR
CLanguageSelectorDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {

        // JRY: pro AS 2.53, kery jde s cestinou, nemcinou a anglictinou je pro ostatni jazyky posleme na forum do sekce
        //     "Translations" https://forum.altap.cz/viewforum.php?f=23 - treba to nekoho namotivuje a pujde svuj preklad vytvorit

        // zatim zadna stranka pro download jazyku neexistuje, tak tohle tlacitko zablokujeme
        // EnableWindow(GetDlgItem(HWindow, IDB_GETMORELANGS), FALSE);

        if (!OpenedFromConfiguration && !OpenedForPlugin)
        {
            // do titulku dame nazev programu, protoze je to prvniokno, ktere user spatri
            SetWindowText(HWindow, MAINWINDOW_NAME);
        }
        else
        {
            if (PluginName != NULL)
            {
                // do titulku dame nazev pluginu, aby user pochopil pro co vybira jazyk
                char buf[200];
                _snprintf_s(buf, _TRUNCATE, "%s: ", PluginName);
                buf[99] = 0; // vyuzijeme jen 100 znaku pro nazev pluginu, at neco zbyde na puvodni titulek dialogu
                int len = (int)strlen(buf);
                if (GetWindowText(HWindow, buf + len, 200 - len))
                    SetWindowText(HWindow, buf);
            }
        }
        if (!OpenedFromConfiguration && PluginName == NULL) // tlacitko Cancel predelame na Exit
            SetDlgItemText(HWindow, IDCANCEL, ExitButtonLabel);
        if (PluginName != NULL) // zakazeme close
            EnableMenuItem(GetSystemMenu(HWindow, FALSE), SC_CLOSE, MF_BYCOMMAND | MF_GRAYED);

        Web = new CHyperLink(HWindow, IDC_SLG_WEB, STF_HYPERLINK_COLOR);

        HListView = GetDlgItem(HWindow, IDC_SLG_LIST);

        DWORD exFlags = LVS_EX_FULLROWSELECT;
        DWORD origFlags = ListView_GetExtendedListViewStyle(HListView);
        ListView_SetExtendedListViewStyle(HListView, origFlags | exFlags); // 4.71

        // naleju do listview sloupce Language a Path
        char buff[100];
        LVCOLUMN lvc;
        lvc.mask = LVCF_TEXT | LVCF_SUBITEM;
        lvc.pszText = buff;
        lvc.iSubItem = 0;
        GetDlgItemText(HWindow, IDC_SLG_DESCR, buff, 100);
        DestroyWindow(GetDlgItem(HWindow, IDC_SLG_DESCR));
        ListView_InsertColumn(HListView, 0, &lvc);

        lvc.iSubItem = 1;
        GetDlgItemText(HWindow, IDC_SLG_PATH, buff, 100);
        DestroyWindow(GetDlgItem(HWindow, IDC_SLG_PATH));
        ListView_InsertColumn(HListView, 1, &lvc);

        // pod W2K pri spousteni pres shortcut s MAXIMIZED nastavenim
        // se dialog zobrazoval maximalizovany; SC_RESTORE na to zabira
        INT_PTR ret = CCommonDialog::DialogProc(uMsg, wParam, lParam);
        SendMessage(HWindow, WM_SYSCOMMAND, SC_RESTORE, 0);
        return ret;
    }

    case WM_COMMAND:
    {
        if (PluginName != NULL && LOWORD(wParam) == IDCANCEL)
            return 0;
        if (LOWORD(wParam) == IDB_GETMORELANGS)
            ShellExecute(HWindow, "open", "https://forum.altap.cz/viewforum.php?f=23", NULL, NULL, SW_SHOWNORMAL);
        if (LOWORD(wParam) == IDB_REFRESHLANGS)
        {
            ListView_DeleteAllItems(HListView);
            int i;
            for (i = 0; i < Items.Count; i++)
                Items[i].Free();
            Items.DestroyMembers();
            Initialize();
            if (GetLanguagesCount() == 0) // nehrozi, protoze tenhle dialog je loadnuty z .slg modulu (toto .slg tedy nejde smazat)
            {
                MessageBox(HWindow, "Unable to find any language file (.SLG) in subdirectory LANG.\n"
                                    "Please reinstall Open Salamander.",
                           SALAMANDER_TEXT_VERSION, MB_OK | MB_ICONERROR);
                TRACE_E("CLanguageSelectorDialog: unexpected situation (no language file): calling ExitProcess(667).");
                //          ExitProcess(667);
                TerminateProcess(GetCurrentProcess(), 667); // tvrdsi exit (tenhle jeste neco vola)
            }
            LoadListView();
        }
        break;
    }

    case WM_NOTIFY:
    {
        if (wParam == IDC_SLG_LIST)
        {
            LPNMHDR nmh = (LPNMHDR)lParam;
            switch (nmh->code)
            {
            case NM_DBLCLK:
            {
                LVHITTESTINFO ht;
                DWORD pos = GetMessagePos();
                ht.pt.x = GET_X_LPARAM(pos);
                ht.pt.y = GET_Y_LPARAM(pos);
                ScreenToClient(HListView, &ht.pt);
                ListView_HitTest(HListView, &ht);
                int index = ListView_GetNextItem(HListView, -1, LVNI_SELECTED);
                if (index != -1 && ht.iItem == index)
                {
                    PostMessage(HWindow, WM_COMMAND, MAKELPARAM(IDOK, BN_CLICKED),
                                (LPARAM)GetDlgItem(HWindow, IDOK));
                    return 0;
                }
                break;
            }

            case LVN_ITEMCHANGED:
            {
                FillControls();
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
// CSkillLevelDialog
//

CSkillLevelDialog::CSkillLevelDialog(HWND hParent, int* level)
    : CCommonDialog(HLanguage, IDD_SKILLLEVEL, IDD_SKILLLEVEL, hParent)
{
    Level = level;
}

void CSkillLevelDialog::Transfer(CTransferInfo& ti)
{
    ti.RadioButton(IDC_SL_BEGINNER, SKILL_LEVEL_BEGINNER, *Level);
    ti.RadioButton(IDC_SL_INTERMEDIATE, SKILL_LEVEL_INTERMEDIATE, *Level);
    ti.RadioButton(IDC_SL_ADVANCED, SKILL_LEVEL_ADVANCED, *Level);
}

//****************************************************************************
//
// CCompareArgsDlg
//

CCompareArgsDlg::CCompareArgsDlg(HWND parent, BOOL comparingFiles, char* compareName1,
                                 char* compareName2, int* cnfrmShowNamesToCompare)
    : CCommonDialog(HLanguage, IDD_USERMENUCOMPAREARGS, comparingFiles ? IDH_USERMENUCOMPAREARGS_F : IDH_USERMENUCOMPAREARGS_D, parent)
{
    ComparingFiles = comparingFiles;
    CompareName1 = compareName1;
    CompareName2 = compareName2;
    CnfrmShowNamesToCompare = cnfrmShowNamesToCompare;
}

void CCompareArgsDlg::Validate(CTransferInfo& ti)
{
    char buf[MAX_PATH];
    ti.EditLine(IDE_UMC_NAME1, buf, MAX_PATH);
    if (buf[0] == 0)
    {
        SalMessageBox(HWindow, LoadStr(IDS_FF_EMPTYSTRING), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_UMC_NAME1);
        return;
    }
    ti.EditLine(IDE_UMC_NAME2, buf, MAX_PATH);
    if (buf[0] == 0)
    {
        SalMessageBox(HWindow, LoadStr(IDS_FF_EMPTYSTRING), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        ti.ErrorOn(IDE_UMC_NAME2);
        return;
    }
}

void CCompareArgsDlg::Transfer(CTransferInfo& ti)
{
    ti.EditLine(IDE_UMC_NAME1, CompareName1, MAX_PATH);
    ti.EditLine(IDE_UMC_NAME2, CompareName2, MAX_PATH);

    int c = !*CnfrmShowNamesToCompare;
    ti.CheckBox(IDC_UMC_SHOWTHISDLG, c);
    *CnfrmShowNamesToCompare = !c;
}

INT_PTR
CCompareArgsDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        if (!ComparingFiles)
        {
            SetWindowText(HWindow, LoadStr(IDS_USERMENUCOMPAREARGSTITLE));
            SetDlgItemText(HWindow, IDT_UMC_NAME1, LoadStr(IDS_USERMENUCOMPAREARG1));
            SetDlgItemText(HWindow, IDT_UMC_NAME2, LoadStr(IDS_USERMENUCOMPAREARG2));
        }
        CHyperLink* hl = new CHyperLink(HWindow, IDT_UMC_HOWTOREVERT, STF_DOTUNDERLINE);
        if (hl != NULL)
            hl->SetActionShowHint(LoadStr(IDS_UMCCONFIRMHOWTOREV));
        break;
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDB_UMC_BROWSENAME1:
        case IDB_UMC_BROWSENAME2:
        {
            int editID = LOWORD(wParam) == IDB_UMC_BROWSENAME1 ? IDE_UMC_NAME1 : IDE_UMC_NAME2;
            if (ComparingFiles)
                BrowseCommand(HWindow, editID, IDS_ALLFILTER);
            else
            {
                char path[MAX_PATH];
                GetDlgItemText(HWindow, editID, path, MAX_PATH);
                if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_BROWSEUMCDIRTITLE),
                                       LoadStr(IDS_BROWSEUMCDIRTEXT), path, FALSE, path))
                {
                    SetDlgItemText(HWindow, editID, path);
                }
            }
            return TRUE;
        }
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}
