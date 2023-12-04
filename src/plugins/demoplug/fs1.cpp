// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

// FS-name pridelene Salamanderem po loadu pluginu
char AssignedFSName[MAX_PATH] = "";
extern int AssignedFSNameLen = 0;

// image-list pro jednoduche ikony FS
HIMAGELIST DFSImageList = NULL;

// globalni promenne, do ktery si ulozim ukazatele na globalni promenne v Salamanderovi
// pro archiv i pro FS - promenne se sdileji
const CFileData** TransferFileData = NULL;
int* TransferIsDir = NULL;
char* TransferBuffer = NULL;
int* TransferLen = NULL;
DWORD* TransferRowData = NULL;
CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
DWORD* TransferActCustomData = NULL;

// pomocna promenna pro testy
CPluginFSInterfaceAbstract* LastDetachedFS = NULL;

// struktura pro predani dat z "Connect" dialogu do nove vytvoreneho FS
CConnectData ConnectData;

// ****************************************************************************
// SEKCE FILE SYSTEMU
// ****************************************************************************

BOOL InitFS()
{
    DFSImageList = ImageList_Create(16, 16, ILC_MASK | SalamanderGeneral->GetImageListColorFlags(), 2, 0);
    if (DFSImageList == NULL)
    {
        TRACE_E("Unable to create image list.");
        return FALSE;
    }
    ImageList_SetImageCount(DFSImageList, 2); // inicializace
    ImageList_SetBkColor(DFSImageList, SalamanderGeneral->GetCurrentColor(SALCOL_ITEM_BK_NORMAL));

    // ikony jsou na ruznych Windows ruzne, nejlepsi je ziskat je dynamicky (napr. ikonu adresare
    // ze systemoveho adresare); zde jednoduse pouzijeme jednu verzi ikony (napr. na W2K nesedi)
    ImageList_ReplaceIcon(DFSImageList, 0, HANDLES(LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_DIR))));
    ImageList_ReplaceIcon(DFSImageList, 1, HANDLES(LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FILE))));

    return TRUE;
}

void ReleaseFS()
{
    ImageList_Destroy(DFSImageList);
}

//
// ****************************************************************************
// CPluginInterfaceForFS
//

CPluginFSInterfaceAbstract* WINAPI
CPluginInterfaceForFS::OpenFS(const char* fsName, int fsNameIndex)
{

    // tady by se mel podle 'fsNameIndex' vytvaret pokazde jiny objekt FS...

    // v 'fsName' je videt, jak uzivatel napsal jmeno FS ("Ftp", "ftp", "FTP",
    // atd. - porad jde o jedno jmeno FS)

    ActiveFSCount++;
    return new CPluginFSInterface;
}

void WINAPI
CPluginInterfaceForFS::CloseFS(CPluginFSInterfaceAbstract* fs)
{
    CPluginFSInterface* dfsFS = (CPluginFSInterface*)fs; // aby se volal spravny destruktor

    if (dfsFS == LastDetachedFS)
        LastDetachedFS = NULL;
    ActiveFSCount--;
    if (dfsFS != NULL)
        delete dfsFS;
}

char ConnectPath[MAX_PATH] = "";
char** History = NULL;
int HistoryCount = 0;

INT_PTR CALLBACK ConnectDlgProc(HWND HWindow, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("ConnectDlgProc(, 0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        SalamanderGUI->ArrangeHorizontalLines(HWindow);

        // horizontalni i vertikalni vycentrovani dialogu k parentu
        HWND hParent = GetParent(HWindow);
        if (hParent != NULL)
            SalamanderGeneral->MultiMonCenterWindow(HWindow, hParent, TRUE);

        // vlozeni dat do dialogu
        SalamanderGeneral->LoadComboFromStdHistoryValues(GetDlgItem(HWindow, IDC_PATH),
                                                         History, HistoryCount);
        SendDlgItemMessage(HWindow, IDC_PATH, CB_LIMITTEXT, MAX_PATH - 1, 0);
        SendDlgItemMessage(HWindow, IDC_PATH, WM_SETTEXT, 0, (LPARAM)ConnectPath);

        SalamanderGeneral->InstallWordBreakProc(GetDlgItem(HWindow, IDC_PATH)); // instalujeme WordBreakProc do comboboxu

        return TRUE; // focus od std. dialogproc
    }

    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDOK:
        {
            // ziskani dat z dialogu
            SendDlgItemMessage(HWindow, IDC_PATH, WM_GETTEXT, MAX_PATH, (LPARAM)ConnectPath);
            SalamanderGeneral->AddValueToStdHistoryValues(History, HistoryCount, ConnectPath, FALSE);
        }
        case IDCANCEL:
        {
            EndDialog(HWindow, wParam);
            return TRUE;
        }
        }
        break;
    }
    }
    return FALSE; // not processed
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(int panel)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForFS::ExecuteChangeDriveMenuItem(%d)", panel);
    SalamanderGeneral->GetStdHistoryValues(SALHIST_CHANGEDIR, &History, &HistoryCount);
    while (1)
    {
        if (DialogBoxParam(HLanguage, MAKEINTRESOURCE(IDD_CONNECT),
                           SalamanderGeneral->GetMsgBoxParent(), ConnectDlgProc, NULL) == IDOK)
        {
            // nechame prekreslit hlavni okno (aby user cely connect nekoukal na zbytek po connect dialogu)
            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());

            // zmenime cestu v aktualnim panelu na AssignedFSName:ConnectPath
            ConnectData.UseConnectData = TRUE;
            lstrcpyn(ConnectData.UserPart, ConnectPath, MAX_PATH);
            int failReason;
            BOOL changeRes = SalamanderGeneral->ChangePanelPathToPluginFS(panel, AssignedFSName, "", &failReason);
            // POZNAMKA: pri uspechu vraci failReason==CHPPFR_SHORTERPATH (user-part cesty neni "")
            ConnectData.UseConnectData = FALSE;
            if (!changeRes && failReason == CHPPFR_INVALIDPATH)
                continue; // zopakujeme si zadani

            /*
      if (!SalamanderGeneral->ChangePanelPathToDisk(panel, ConnectPath, &failReason))
      {
        if (failReason == CHPPFR_INVALIDPATH) continue;  // zopakujeme si zadani
      }
*/
            /*
      if (!SalamanderGeneral->ChangePanelPathToArchive(panel, ConnectPath, "", &failReason))
      {
        if (failReason == CHPPFR_INVALIDPATH || failReason == CHPPFR_INVALIDARCHIVE) continue;  // zopakujeme si zadani
      }
*/
            /*
      if (LastDetachedFS != NULL &&
          !SalamanderGeneral->ChangePanelPathToDetachedFS(panel, LastDetachedFS, &failReason))
      {
        // opakovat zadani nema smysl
      }
*/
            /*
      char buf[MAX_PATH];
      if (SalamanderGeneral->GetLastWindowsPanelPath(panel, buf, MAX_PATH))
      {
        if (!SalamanderGeneral->ChangePanelPathToDisk(panel, buf, &failReason))
        {
          // opakovat zadani nema smysl
        }
      }
*/
            /*
      if (!SalamanderGeneral->ChangePanelPathToRescuePathOrFixedDrive(panel, &failReason))
      {
        // opakovat zadani nema smysl
      }
*/
            //      SalamanderGeneral->RefreshPanelPath(panel);
            //      SalamanderGeneral->PostRefreshPanelPath(panel);
            /*
      if (LastDetachedFS != NULL)
      {
        SalamanderGeneral->CloseDetachedFS(SalamanderGeneral->GetMsgBoxParent(), LastDetachedFS);
      }
*/
        }
        break;
    }
}

BOOL WINAPI
CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(HWND parent, int panel, int x, int y,
                                                      CPluginFSInterfaceAbstract* pluginFS,
                                                      const char* pluginFSName, int pluginFSNameIndex,
                                                      BOOL isDetachedFS, BOOL& refreshMenu,
                                                      BOOL& closeMenu, int& postCmd, void*& postCmdParam)
{
    CALL_STACK_MESSAGE7("CPluginInterfaceForFS::ChangeDriveMenuItemContextMenu(, %d, %d, %d, , %s, %d, %d, , , ,)",
                        panel, x, y, pluginFSName, pluginFSNameIndex, isDetachedFS);

    // vytvorime menu
    char** strings;
    static char buffShowInPanel[] = "&Show in Panel";
    static char buffDisconnect[] = "&Disconnect";
    char* strings1[] = {buffShowInPanel, buffDisconnect, NULL}; // polozky pro odpojeny plugin FS
    static char buffRefresh[] = "&Refresh";
    char* strings2[] = {buffRefresh, buffDisconnect, NULL}; // polozky pro aktivni plugin FS
    static char buffConnect[] = "Connect To...";
    char* strings3[] = {buffConnect, NULL}; // polozky pro FS
    if (pluginFS != NULL)
    {
        if (isDetachedFS)
            strings = strings1;
        else
            strings = strings2;
    }
    else
        strings = strings3;

    HMENU menu = CreatePopupMenu();
    MENUITEMINFO mi;
    memset(&mi, 0, sizeof(mi));
    mi.cbSize = sizeof(mi);
    mi.fMask = MIIM_TYPE | MIIM_ID;
    mi.fType = MFT_STRING;
    int i;
    for (i = 0; strings[i] != NULL; i++)
    {
        char* p = strings[i];
        mi.wID = i + 1;
        mi.dwTypeData = p;
        mi.cch = (UINT)strlen(p);
        InsertMenuItem(menu, i, TRUE, &mi);
    }
    DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                 x, y, parent, NULL);
    DestroyMenu(menu);
    if (cmd != 0) // user vybral z menu prikaz
    {
        refreshMenu = FALSE;
        closeMenu = FALSE;
        postCmd = 0;
        postCmdParam = NULL;

        if (pluginFS != NULL)
        {
            if (isDetachedFS) // polozka pro odpojeny plugin FS
            {
                switch (cmd)
                {
                case 1: // show in panel
                {
                    closeMenu = TRUE;
                    postCmd = 1;
                    postCmdParam = (void*)pluginFS;
                    break;
                }

                case 2: // disconnect
                {
                    // pokud FS v metode ReleaseObject otevira okna (zde se to nedeje), mel by se
                    // CloseDetachedFS volat az po zavreni change-drive menu
                    SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
                    refreshMenu = TRUE; // pokud se uzavrel, je treba refreshnout Change Drive menu
                    break;
                }
                }
            }
            else // polozka pro aktivni plugin FS
            {
                switch (cmd)
                {
                case 1: // refresh
                {
                    closeMenu = TRUE;
                    postCmd = 2;
                    break;
                }

                case 2: // disconnect
                {
                    closeMenu = TRUE;
                    postCmd = 4;
                    break;
                }
                }
            }
        }
        else // polozka pro FS
        {
            switch (cmd)
            {
            case 1: // connect to...
            {
                closeMenu = TRUE;
                postCmd = 3;
                break;
            }
            }
        }
        return TRUE;
    }
    else
        return FALSE; // cancel nebo chyba menu
}

void WINAPI
CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(int panel, int postCmd, void* postCmdParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForFS::ExecuteChangeDrivePostCommand(%d, %d,)", panel, postCmd);

    if (postCmd == 1) // polozka pro odpojeny FS: show in panel
    {
        SalamanderGeneral->ChangePanelPathToDetachedFS(panel,
                                                       (CPluginFSInterfaceAbstract*)postCmdParam,
                                                       NULL);
    }

    if (postCmd == 2) // polozka pro aktivni FS: refresh
    {
        SalamanderGeneral->RefreshPanelPath(panel);
    }

    if (postCmd == 3) // polozka pro FS: connect to...
    {
        ExecuteChangeDriveMenuItem(panel);
    }

    if (postCmd == 4) // polozka pro aktivni FS: disconnect
    {                 // pouzijeme PostMenuExtCommand, aby se disconnect spustil az v "sal-idle"
        int leftOrRightPanel = panel == PANEL_SOURCE ? SalamanderGeneral->GetSourcePanel() : panel;
        SalamanderGeneral->PostMenuExtCommand(leftOrRightPanel == PANEL_LEFT ? MENUCMD_DISCONNECT_LEFT : MENUCMD_DISCONNECT_RIGHT,
                                              TRUE); // ukazka prace s panelem 'panel' (pokud prijde prikaz z Drive bary)

        //    SalamanderGeneral->PostMenuExtCommand(MENUCMD_DISCONNECT_ACTIVE, TRUE);  // tento prikaz z Drive bary nehrozi, tedy staci tato implementace
    }
}

BOOL WINAPI
CPluginInterfaceForFS::DisconnectFS(HWND parent, BOOL isInPanel, int panel,
                                    CPluginFSInterfaceAbstract* pluginFS,
                                    const char* pluginFSName, int pluginFSNameIndex)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForFS::DisconnectFS(, %d, %d, , %s, %d)",
                        isInPanel, panel, pluginFSName, pluginFSNameIndex);
    ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = TRUE; // potlacime zbytecne dotazy (user dal prikaz pro disconnect, jen ho provedeme)
    BOOL ret = FALSE;
    if (isInPanel)
    {
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        ret = SalamanderGeneral->GetPanelPluginFS(panel) != pluginFS;
    }
    else
    {
        ret = SalamanderGeneral->CloseDetachedFS(parent, pluginFS);
    }
    if (!ret)
        ((CPluginFSInterface*)pluginFS)->CalledFromDisconnectDialog = FALSE; // vypneme potlaceni zbytecnych dotazu
    return ret;
}

void WINAPI
CPluginInterfaceForFS::ExecuteOnFS(int panel, CPluginFSInterfaceAbstract* pluginFS,
                                   const char* pluginFSName, int pluginFSNameIndex,
                                   CFileData& file, int isDir)
{
#ifndef DEMOPLUG_QUIET
    char buf[300];
    sprintf(buf, "%s %s in %s panel", isDir ? (isDir == 2 ? "UpDir" : "Subdirectory") : "File", file.Name,
            panel == PANEL_LEFT ? "left" : "right");
    SalamanderGeneral->ShowMessageBox(buf, "FS Execute", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    CPluginFSInterface* fs = (CPluginFSInterface*)pluginFS;
    if (isDir) // podadresar nebo up-dir
    {
        char newPath[MAX_PATH];
        strcpy(newPath, fs->Path);
        if (isDir == 2) // up-dir
        {
            char* cutDir = NULL;
            if (SalamanderGeneral->CutDirectory(newPath, &cutDir)) // zkratime cestu o posl. komponentu
            {
                int topIndex; // pristi top-index, -1 -> neplatny
                if (!fs->TopIndexMem.FindAndPop(newPath, topIndex))
                    topIndex = -1;
                // zmenime cestu v panelu
                fs = NULL; // po ChangePanelPathToXXX uz ukazatel nemusi byt platny
                SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newPath, NULL,
                                                             topIndex, cutDir);
            }
        }
        else // podadresar
        {
            // zaloha udaju pro TopIndexMem (backupPath + topIndex)
            char backupPath[MAX_PATH];
            strcpy(backupPath, newPath);
            int topIndex = SalamanderGeneral->GetPanelTopIndex(panel);

            if (SalamanderGeneral->SalPathAppend(newPath, file.Name, MAX_PATH)) // nastavime cestu
            {
                // zmenime cestu v panelu
                fs = NULL; // po ChangePanelPathToXXX uz ukazatel nemusi byt platny
                if (SalamanderGeneral->ChangePanelPathToPluginFS(panel, pluginFSName, newPath))
                {
                    fs = (CPluginFSInterface*)SalamanderGeneral->GetPanelPluginFS(panel); // pro pripad zmeny FS v panelu musime vytahnout aktualni objekt
                    if (fs != NULL && fs == pluginFS)                                     // pokud jde o puvodni FS
                        fs->TopIndexMem.Push(backupPath, topIndex);                       // zapamatujeme top-index pro navrat
                }
            }
        }
    }
    else // soubor
    {
        SalamanderGeneral->SetUserWorkedOnPanelPath(panel);
        SalamanderGeneral->ExecuteAssociation(SalamanderGeneral->GetMainWindowHWND(), fs->Path, file.Name);
    }
}

//****************************************************************************
//
// CTopIndexMem
//

void CTopIndexMem::Push(const char* path, int topIndex)
{
    // zjistime, jestli path navazuje na Path (path==Path+"\\jmeno")
    const char* s = path + strlen(path);
    if (s > path && *(s - 1) == '\\')
        s--;
    BOOL ok;
    if (s == path)
        ok = FALSE;
    else
    {
        if (s > path && *s == '\\')
            s--;
        while (s > path && *s != '\\')
            s--;

        int l = (int)strlen(Path);
        if (l > 0 && Path[l - 1] == '\\')
            l--;
        ok = s - path == l && SalamanderGeneral->StrNICmp(path, Path, l) == 0;
    }

    if (ok) // navazuje -> zapamatujeme si dalsi top-index
    {
        if (TopIndexesCount == TOP_INDEX_MEM_SIZE) // je potreba vyhodit z pameti prvni top-index
        {
            int i;
            for (i = 0; i < TOP_INDEX_MEM_SIZE - 1; i++)
                TopIndexes[i] = TopIndexes[i + 1];
            TopIndexesCount--;
        }
        strcpy(Path, path);
        TopIndexes[TopIndexesCount++] = topIndex;
    }
    else // nenavazuje -> prvni top-index v rade
    {
        strcpy(Path, path);
        TopIndexesCount = 1;
        TopIndexes[0] = topIndex;
    }
}

BOOL CTopIndexMem::FindAndPop(const char* path, int& topIndex)
{
    // zjistime, jestli path odpovida Path (path==Path)
    int l1 = (int)strlen(path);
    if (l1 > 0 && path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(Path);
    if (l2 > 0 && Path[l2 - 1] == '\\')
        l2--;
    if (l1 == l2 && SalamanderGeneral->StrNICmp(path, Path, l1) == 0)
    {
        if (TopIndexesCount > 0)
        {
            char* s = Path + strlen(Path);
            if (s > Path && *(s - 1) == '\\')
                s--;
            if (s > Path && *s == '\\')
                s--;
            while (s > Path && *s != '\\')
                s--;
            *s = 0;
            topIndex = TopIndexes[--TopIndexesCount];
            return TRUE;
        }
        else // tuto hodnotu jiz nemame (nebyla ulozena nebo mala pamet->byla vyhozena)
        {
            Clear();
            return FALSE;
        }
    }
    else // dotaz na jinou cestu -> vycistime pamet, doslo k dlouhemu skoku
    {
        Clear();
        return FALSE;
    }
}

//
// ****************************************************************************
// CPluginFSDataInterface
//

CPluginFSDataInterface::CPluginFSDataInterface(const char* path)
{
    strcpy(Path, path);
    SalamanderGeneral->SalPathAddBackslash(Path, MAX_PATH);
    Name = Path + strlen(Path);
}

HIMAGELIST WINAPI
CPluginFSDataInterface::GetSimplePluginIcons(int iconSize)
{
    return DFSImageList;
}

HICON WINAPI
CPluginFSDataInterface::GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon)
{
    lstrcpyn(Name, file->Name, (int)(MAX_PATH - (Name - Path)));
    HICON icon;
    if (!SalamanderGeneral->GetFileIcon(Path, FALSE, &icon, iconSize, FALSE, FALSE))
        icon = NULL;
    destroyIcon = TRUE;
    return icon; // icon or NULL (failure)
}

// globalni promenne pro nasledujici tri "get text" funkce
CFSData* FSdata;
SYSTEMTIME FSst;
FILETIME FSft;
int FSlen, FSlen2;

// callback volany ze Salamandera pro ziskani textu
// popis viz. spl_com.h / FColumnGetText
void WINAPI GetTypeText()
{
    FSdata = (CFSData*)((*TransferFileData)->PluginData);
    memcpy(TransferBuffer, FSdata->TypeName, (*TransferLen = (int)strlen(FSdata->TypeName)));
}

void WINAPI GetCreatedText()
{
    FileTimeToLocalFileTime(&((CFSData*)((*TransferFileData)->PluginData))->CreationTime, &FSft);
    FileTimeToSystemTime(&FSft, &FSst);
    FSlen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &FSst, NULL, TransferBuffer, 50) - 1;
    if (FSlen < 0)
        FSlen = sprintf(TransferBuffer, "%u.%u.%u", FSst.wDay, FSst.wMonth, FSst.wYear);
    *(TransferBuffer + FSlen++) = ' ';
    FSlen2 = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &FSst, NULL, TransferBuffer + FSlen, 50) - 1;
    if (FSlen2 < 0)
        FSlen2 = sprintf(TransferBuffer + FSlen, "%u:%02u:%02u", FSst.wHour, FSst.wMinute, FSst.wSecond);
    *TransferLen = FSlen + FSlen2;
}

void WINAPI GetModifiedText()
{
    FileTimeToLocalFileTime(&(*TransferFileData)->LastWrite, &FSft);
    FileTimeToSystemTime(&FSft, &FSst);
    FSlen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &FSst, NULL, TransferBuffer, 50) - 1;
    if (FSlen < 0)
        FSlen = sprintf(TransferBuffer, "%u.%u.%u", FSst.wDay, FSst.wMonth, FSst.wYear);
    *(TransferBuffer + FSlen++) = ' ';
    FSlen2 = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &FSst, NULL, TransferBuffer + FSlen, 50) - 1;
    if (FSlen2 < 0)
        FSlen2 = sprintf(TransferBuffer + FSlen, "%u:%02u:%02u", FSst.wHour, FSst.wMinute, FSst.wSecond);
    *TransferLen = FSlen + FSlen2;
}

void WINAPI GetAccessedText()
{
    FileTimeToLocalFileTime(&((CFSData*)((*TransferFileData)->PluginData))->LastAccessTime, &FSft);
    FileTimeToSystemTime(&FSft, &FSst);
    FSlen = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &FSst, NULL, TransferBuffer, 50) - 1;
    if (FSlen < 0)
        FSlen = sprintf(TransferBuffer, "%u.%u.%u", FSst.wDay, FSst.wMonth, FSst.wYear);
    *(TransferBuffer + FSlen++) = ' ';
    FSlen2 = GetTimeFormat(LOCALE_USER_DEFAULT, 0, &FSst, NULL, TransferBuffer + FSlen, 50) - 1;
    if (FSlen2 < 0)
        FSlen2 = sprintf(TransferBuffer + FSlen, "%u:%02u:%02u", FSst.wHour, FSst.wMinute, FSst.wSecond);
    *TransferLen = FSlen + FSlen2;
}

int WINAPI PluginSimpleIconCallback()
{
    return *TransferIsDir ? 0 : 1;
}

void AddTypeColumn(BOOL leftPanel, CSalamanderViewAbstract* view, int& i, const CColumn* origTypeColumn)
{
    CColumn column = *origTypeColumn;
    strcpy(column.Name, "DFS Type");
    column.GetText = GetTypeText;
    column.CustomData = 4;
    column.ID = COLUMN_ID_CUSTOM;
    column.Width = leftPanel ? LOWORD(DFSTypeWidth) : HIWORD(DFSTypeWidth);
    column.FixedWidth = leftPanel ? LOWORD(DFSTypeFixedWidth) : HIWORD(DFSTypeFixedWidth);
    view->InsertColumn(++i, &column); // vlozime nas sloupec Type za originalni sloupec Type
}

void AddTimeColumns(BOOL leftPanel, CSalamanderViewAbstract* view, int& i)
{
    CColumn column;
    strcpy(column.Name, "Created");
    strcpy(column.Description, "Creation Time");
    column.GetText = GetCreatedText;
    column.CustomData = 1;
    column.SupportSorting = 1;
    column.LeftAlignment = 1;
    column.ID = COLUMN_ID_CUSTOM;
    column.Width = leftPanel ? LOWORD(CreatedWidth) : HIWORD(CreatedWidth);
    column.FixedWidth = leftPanel ? LOWORD(CreatedFixedWidth) : HIWORD(CreatedFixedWidth);
    view->InsertColumn(i++, &column);

    strcpy(column.Name, "Modified");
    strcpy(column.Description, "Last Write Time");
    column.GetText = GetModifiedText;
    column.CustomData = 2;
    column.Width = leftPanel ? LOWORD(ModifiedWidth) : HIWORD(ModifiedWidth);
    column.FixedWidth = leftPanel ? LOWORD(ModifiedFixedWidth) : HIWORD(ModifiedFixedWidth);
    view->InsertColumn(i++, &column);

    strcpy(column.Name, "Accessed");
    strcpy(column.Description, "Last Access Time");
    column.GetText = GetAccessedText;
    column.CustomData = 3;
    column.Width = leftPanel ? LOWORD(AccessedWidth) : HIWORD(AccessedWidth);
    column.FixedWidth = leftPanel ? LOWORD(AccessedFixedWidth) : HIWORD(AccessedFixedWidth);
    view->InsertColumn(i++, &column);
}

void WINAPI
CPluginFSDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                  const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    view->SetPluginSimpleIconCallback(PluginSimpleIconCallback);

    if (view->GetViewMode() == VIEW_MODE_DETAILED) // upravime sloupce
    {
        // vyhodime sloupce Date + Time a na jejich pozici vlozime Created/Modified/Accessed
        // a najdeme sloupec Type a nahradime ho nasim sloupcem Type
        int count = view->GetColumnsCount();
        BOOL timeColumnsDone = FALSE;
        //    BOOL typeColumnDone = FALSE;
        int i;
        for (i = 0; i < count; i++)
        {
            const CColumn* c = view->GetColumn(i);
            if (c->ID == COLUMN_ID_TYPE)
            { // provedeme zamenu sloupce Type za nas
                //        typeColumnDone = TRUE;
                AddTypeColumn(leftPanel, view, i, c);
                count = view->GetColumnsCount(); // pocet sloupcu se zmenil
                continue;                        // pokracujeme s hledanim na dalsim 'i'
            }
            if (c->ID == COLUMN_ID_DATE || c->ID == COLUMN_ID_TIME)
            { // vyhodime Date a Time, vlozime Created/Modified/Accessed
                view->DeleteColumn(i);
                if (!timeColumnsDone)
                {
                    timeColumnsDone = TRUE;
                    AddTimeColumns(leftPanel, view, i); // 'i' se zvetsi o pocet pridanych sloupcu
                }
                i--;                             // jeden jsme smazali, vracime se na stejne 'i'
                count = view->GetColumnsCount(); // pocet sloupcu se zmenil
                continue;
            }
        }

        /*
    if (!typeColumnDone)  // sloupec Type zobrazime v kazdem pripade
    {
      view->InsertStandardColumn(count, COLUMN_ID_TYPE);  // vytahneme standardni sloupec Type
      const CColumn *c = view->GetColumn(count);
      if (c != NULL && c->ID == COLUMN_ID_TYPE)
      {   // provedeme zamenu sloupce Type za nas
        AddTypeColumn(leftPanel, view, count, c);
        count = view->GetColumnsCount();
      }
    }
*/
        if (!timeColumnsDone) // sloupce casu zobrazime v kazdem pripade
        {
            AddTimeColumns(leftPanel, view, count);
            count = view->GetColumnsCount();
        }
    }
}

void WINAPI
CPluginFSDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (leftPanel)
    {
        switch (column->CustomData)
        {
        case 1:
            CreatedFixedWidth = MAKELONG(newFixedWidth, HIWORD(CreatedFixedWidth));
            break;
        case 2:
            ModifiedFixedWidth = MAKELONG(newFixedWidth, HIWORD(ModifiedFixedWidth));
            break;
        case 3:
            AccessedFixedWidth = MAKELONG(newFixedWidth, HIWORD(AccessedFixedWidth));
            break;
        case 4:
            DFSTypeFixedWidth = MAKELONG(newFixedWidth, HIWORD(DFSTypeFixedWidth));
            break;
        }
    }
    else
    {
        switch (column->CustomData)
        {
        case 1:
            CreatedFixedWidth = MAKELONG(LOWORD(CreatedFixedWidth), newFixedWidth);
            break;
        case 2:
            ModifiedFixedWidth = MAKELONG(LOWORD(ModifiedFixedWidth), newFixedWidth);
            break;
        case 3:
            AccessedFixedWidth = MAKELONG(LOWORD(AccessedFixedWidth), newFixedWidth);
            break;
        case 4:
            DFSTypeFixedWidth = MAKELONG(LOWORD(DFSTypeFixedWidth), newFixedWidth);
            break;
        }
    }
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void WINAPI
CPluginFSDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (leftPanel)
    {
        switch (column->CustomData)
        {
        case 1:
            CreatedWidth = MAKELONG(newWidth, HIWORD(CreatedWidth));
            break;
        case 2:
            ModifiedWidth = MAKELONG(newWidth, HIWORD(ModifiedWidth));
            break;
        case 3:
            AccessedWidth = MAKELONG(newWidth, HIWORD(AccessedWidth));
            break;
        case 4:
            DFSTypeWidth = MAKELONG(newWidth, HIWORD(DFSTypeWidth));
            break;
        }
    }
    else
    {
        switch (column->CustomData)
        {
        case 1:
            CreatedWidth = MAKELONG(LOWORD(CreatedWidth), newWidth);
            break;
        case 2:
            ModifiedWidth = MAKELONG(LOWORD(ModifiedWidth), newWidth);
            break;
        case 3:
            AccessedWidth = MAKELONG(LOWORD(AccessedWidth), newWidth);
            break;
        case 4:
            DFSTypeWidth = MAKELONG(LOWORD(DFSTypeWidth), newWidth);
            break;
        }
    }
}

struct CFSInfoLineData
{
    const char* Name;
    const char* Type;
};

const char* WINAPI FSInfoLineFile(HWND parent, void* param)
{
    CFSInfoLineData* data = (CFSInfoLineData*)param;
    return data->Name;
}

const char* WINAPI FSInfoLineType(HWND parent, void* param)
{
    CFSInfoLineData* data = (CFSInfoLineData*)param;
    return data->Type;
}

CSalamanderVarStrEntry FSInfoLine[] =
    {
        {"File", FSInfoLineFile},
        {"Type", FSInfoLineType},
        {NULL, NULL}};

BOOL WINAPI
CPluginFSDataInterface::GetInfoLineContent(int panel, const CFileData* file, BOOL isDir,
                                           int selectedFiles, int selectedDirs, BOOL displaySize,
                                           const CQuadWord& selectedSize, char* buffer,
                                           DWORD* hotTexts, int& hotTextsCount)
{
    if (file != NULL)
    {
        CFSInfoLineData data;
        data.Name = file->Name;
        data.Type = ((CFSData*)file->PluginData)->TypeName;
        hotTextsCount = 100;
        if (!SalamanderGeneral->ExpandVarString(SalamanderGeneral->GetMsgBoxParent(),
                                                "$(File): $(Type)", buffer, 1000, FSInfoLine,
                                                &data, FALSE, hotTexts, &hotTextsCount))
        {
            strcpy(buffer, "Error!");
            hotTextsCount = 0;
        }
        return TRUE;
    }
    else
    {
        if (selectedFiles == 0 && selectedDirs == 0) // Information Line pro prazdny panel
        {
            // return FALSE;  // text at vypise Salamander
            strcpy(buffer, "No items found");
            hotTextsCount = 0;
            return TRUE;
        }
        // return FALSE;  // pocty oznacenych souboru a adresaru at vypise Salamander
        if (displaySize)
        {
            /*
      // testneme soucet jeste jednou
      CQuadWord mySize(0, 0);
      int index = 0;
      const CFileData *file = NULL;
      while ((file = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL)) != NULL)
      {
        mySize += file->Size;
      }
      if (mySize != selectedSize) TRACE_E("Unexpected situation in CPluginFSDataInterface::GetInfoLineContent().");
*/

            char num[100];
            SalamanderGeneral->PrintDiskSize(num, selectedSize, 0);
            // pro jednoduchost nepouzivame "plural" stringy (viz SalamanderGeneral->ExpandPluralString())
            sprintf(buffer, "Selected (<%s>): <%d> files and <%d> directories", num, selectedFiles, selectedDirs);

            /*    // ukazka pouziti standardniho retezce
      SalamanderGeneral->ExpandPluralBytesFilesDirs(buffer, 1000, selectedSize, selectedFiles,
                                                    selectedDirs, TRUE);
*/
        }
        else
            sprintf(buffer, "Selected: <%d> files and <%d> directories", selectedFiles, selectedDirs);
        return SalamanderGeneral->LookForSubTexts(buffer, hotTexts, &hotTextsCount);
    }
}

//
// ****************************************************************************
// CFSData
//

CFSData::CFSData(const FILETIME& creationTime, const FILETIME& lastAccessTime, const char* type)
{
    CreationTime = creationTime;
    LastAccessTime = lastAccessTime;
    TypeName = SalamanderGeneral->DupStr(type);
}
