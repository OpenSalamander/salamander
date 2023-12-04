// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CPluginFSInterface
//

BOOL CPluginFSInterface::ChangeAttributes(const char* fsName, HWND parent, int panel,
                                          int selectedFiles, int selectedDirs)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::ChangeAttributes(%s, , %d, %d, %d)",
                        fsName, panel, selectedFiles, selectedDirs);

    if (ControlConnection == NULL)
    {
        TRACE_E("Unexpected situation in CPluginFSInterface::ChangeAttributes(): ControlConnection == NULL!");
        return FALSE; // preruseni
    }

    // otestujeme jestli neni v panelu jen simple listing -> v tom pripade aspon zatim nic neumime
    CPluginDataInterfaceAbstract* pluginDataIface = SalamanderGeneral->GetPanelPluginData(panel);
    if (pluginDataIface != NULL && (void*)pluginDataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        return FALSE; // preruseni
    }

    // sestaveni popisu s cim se bude pracovat pro dialog Change Attributes
    char subjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(subjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, FALSE);
    char dlgSubjectSrc[MAX_PATH + 100];
    SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, panel,
                                                  selectedFiles, selectedDirs, NULL, FALSE, TRUE);
    char subject[MAX_PATH + 200];
    sprintf(subject, LoadStr(IDS_CHANGEATTRSONFTP), subjectSrc);

    DWORD attr = -1;
    DWORD attrDiff = 0;
    BOOL displayWarning = TRUE; // varovani, ze nejspis nejde o Unixovy server, takze chmod nebude slapat
    if (pluginDataIface != NULL && pluginDataIface != &SimpleListPluginDataInterface)
    { // zajima nas jen data iface typu CFTPListingPluginDataInterface
        CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
        int rightsCol = dataIface->FindRightsColumn();
        if (rightsCol != -1) // pokud Rights column existuje (nemusi byt Unixovy, to se resi dale)
        {
            displayWarning = FALSE;
            const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
            BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
            BOOL isDir = FALSE; // TRUE pokud 'f' je adresar
            int index = 0;
            while (1)
            {
                // vyzvedneme data o zpracovavanem souboru/adresari
                if (focused)
                    f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                else
                    f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                // zjistime atributy souboru/adresare
                if (f != NULL)
                {
                    char* rights = dataIface->GetStringFromColumn(*f, rightsCol);
                    DWORD actAttr;
                    if (GetAttrsFromUNIXRights(&actAttr, &attrDiff, rights)) // prevod stringu na cislo
                    {
                        if (attr == -1)
                            attr = actAttr; // prvni soubor/adresar
                        else
                        {
                            if (attr != actAttr) // pokud se lisi, ulozime v cem se lisi
                                attrDiff |= (attr ^ actAttr);
                        }
                    }
                    else // neni (normalni) Unix, kasleme na to
                    {
                        displayWarning = TRUE;
                        attr = -1;
                        attrDiff = 0;
                        break;
                    }
                }

                // zjistime jestli ma cenu pokracovat (pokud existuje jeste nejaka dalsi oznacena polozka)
                if (focused || f == NULL)
                    break;
            }
        }
    }

    if (!displayWarning || // pripadne zobrazime warning, ze nejde o UNIXovy server s tradicnim modelem prav (napr. ACL nepodporujeme)
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_CHATTRNOTUNIXSRV),
                                         LoadStr(IDS_FTPPLUGINTITLE),
                                         MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                             MB_ICONQUESTION | MB_DEFBUTTON2) == IDYES)
    {
        BOOL selDirs = selectedDirs > 0;
        if (selectedFiles == 0 && selectedDirs == 0)
            SalamanderGeneral->GetPanelFocusedItem(panel, &selDirs);
        CChangeAttrsDlg dlg(parent, subject, attr, attrDiff, selDirs);
        if (dlg.Execute() == IDOK)
        {
            BOOL failed = TRUE; // predpripravime error operace
            // zalozime objekt operace
            CFTPOperation* oper = new CFTPOperation;
            if (oper != NULL)
            {
                oper->SetEncryptControlConnection(ControlConnection->GetEncryptControlConnection());
                oper->SetEncryptDataConnection(ControlConnection->GetEncryptDataConnection());
                CCertificate* cert = ControlConnection->GetCertificate();
                oper->SetCertificate(cert);
                if (cert)
                    cert->Release();
                oper->SetCompressData(ControlConnection->GetCompressData());
                if (ControlConnection->InitOperation(oper)) // inicializace spojeni se serverem podle "control connection"
                {
                    oper->SetBasicData(dlgSubjectSrc, (AutodetectSrvType ? NULL : LastServerType));
                    char path[2 * MAX_PATH];
                    sprintf(path, "%s:", fsName);
                    int pathLen = (int)strlen(path);
                    MakeUserPart(path + pathLen, 2 * MAX_PATH - pathLen);
                    CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(Path);
                    oper->SetOperationChAttr(path, FTPGetPathDelimiter(pathType), TRUE, dlg.IncludeSubdirs,
                                             (WORD)dlg.AttrAndMask, (WORD)dlg.AttrOrMask,
                                             dlg.SelFiles, dlg.SelDirs, Config.OperationsUnknownAttrs);
                    int operUID;
                    if (FTPOperationsList.AddOperation(oper, &operUID))
                    {
                        BOOL ok = TRUE;
                        BOOL emptyQueue = FALSE;

                        // sestavime frontu polozek operace
                        CFTPQueue* queue = new CFTPQueue;
                        if (queue != NULL)
                        {
                            CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
                            if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
                                dataIface = NULL; // zajima nas jen data iface typu CFTPListingPluginDataInterface
                            int rightsCol = -1;   // index sloupce s pravy (pouziva se pro detekci linku)
                            if (dataIface != NULL)
                                rightsCol = dataIface->FindRightsColumn();
                            const CFileData* f = NULL; // ukazatel na soubor/adresar/link v panelu, ktery se ma zpracovat
                            BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
                            BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
                            int skippedItems = 0;  // pocet skipnutych polozek vlozenych do fronty
                            int uiNeededItems = 0; // pocet user-input-needed polozek vlozenych do fronty
                            int index = 0;
                            while (1)
                            {
                                // vyzvedneme data o zpracovavanem souboru
                                if (focused)
                                    f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                                else
                                    f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                                // zpracujeme soubor/adresar/link
                                if (f != NULL)
                                {
                                    CFTPQueueItemType type;
                                    CFTPQueueItemState state;
                                    DWORD problemID;
                                    BOOL skip;
                                    CFTPQueueItem* item = CreateItemForChangeAttrsOperation(f, isDir, rightsCol, dataIface,
                                                                                            &type, &ok, &state, &problemID,
                                                                                            &skippedItems, &uiNeededItems,
                                                                                            &skip, dlg.SelFiles,
                                                                                            dlg.SelDirs, dlg.IncludeSubdirs,
                                                                                            dlg.AttrAndMask, dlg.AttrOrMask,
                                                                                            Config.OperationsUnknownAttrs);
                                    if (item != NULL)
                                    {
                                        if (ok)
                                            item->SetItem(-1, type, state, problemID, Path, f->Name);
                                        if (!ok || !queue->AddItem(item)) // pridani operace do fronty
                                        {
                                            ok = FALSE;
                                            delete item;
                                        }
                                    }
                                    else
                                    {
                                        if (!skip) // jen pokud nejde o skipnuti polozky, ale o chybu nedostatku pameti
                                        {
                                            TRACE_E(LOW_MEMORY);
                                            ok = FALSE;
                                        }
                                    }
                                }
                                // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
                                if (!ok || focused || f == NULL)
                                    break;
                            }
                            int itemsCount = queue->GetCount();
                            emptyQueue = itemsCount == 0;
                            if (ok)
                                oper->SetChildItems(itemsCount, skippedItems, 0, uiNeededItems);
                            else
                            {
                                delete queue;
                                queue = NULL;
                            }
                        }
                        else
                        {
                            TRACE_E(LOW_MEMORY);
                            ok = FALSE;
                        }

                        if (ok) // mame naplnenou frontu s polozkama operace
                        {
                            if (!emptyQueue) // jen pokud neni prazdna fronta s polozkama operace
                            {
                                oper->SetQueue(queue); // nastavime operaci frontu jejich polozek
                                queue = NULL;
                                if (Config.ChAttrAddToQueue)
                                    failed = FALSE; // provest operaci pozdeji -> prozatim jde o uspech operace
                                else                // provest operaci v aktivni "control connection"
                                {
                                    // otevreme okno s progresem operace a spustime operaci
                                    if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, NULL))
                                        failed = FALSE; // uspech operace
                                    else
                                        ok = FALSE;
                                }
                            }
                            else
                            {
                                failed = FALSE; // uspech operace (ale neni co delat)
                                FTPOperationsList.DeleteOperation(operUID, TRUE);
                                delete queue;
                                queue = NULL;
                            }
                        }
                        if (!ok)
                            FTPOperationsList.DeleteOperation(operUID, TRUE);
                        oper = NULL; // operace uz je pridana v poli, nebudeme ji uvolnovat pres 'delete' (viz nize)
                    }
                }
                if (oper != NULL)
                    delete oper;
            }
            else
                TRACE_E(LOW_MEMORY);
            return !failed; // vraci uspech operace (TRUE = zrusit oznaceni v panelu)
        }
    }
    return FALSE; // preruseni
}

BOOL CPluginFSInterface::RunOperation(HWND parent, int operUID, CFTPOperation* oper, HWND dropTargetWnd)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::RunOperation(, %d, ,)", operUID);

    BOOL ok = TRUE;

    CFTPWorker* workerWithCon = NULL; // pokud jsme predali connectionu, zde je ukazatel na to komu
    int i;
    for (i = 0; i < 1; i++) // FIXME: casem treba do konfigurace dame pocatecni pocet workeru operace: staci nahradit "1" za prislusny pocet...
    {
        CFTPWorker* newWorker = oper->AllocNewWorker();
        if (newWorker != NULL)
        {
            if (!SocketsThread->AddSocket(newWorker) ||   // pridani do sockets-threadu
                !newWorker->RefreshCopiesOfUIDAndMsg() || // obnovime kopie UID+Msg (doslo k jejich zmene)
                !oper->AddWorker(newWorker))              // pridani mezi workery operace
            {
                DeleteSocket(newWorker);
                ok = FALSE;
                break;
            }
            else // worker byl uspesne vytvoren a zarazen mezi workery operace
            {
                if (i == 0) // predani aktualni "control connection" do prvniho workera
                {
                    ControlConnection->GiveConnectionToWorker(newWorker, parent);
                    workerWithCon = newWorker;
                }
            }
        }
        else // error, canclujeme operaci
        {
            ok = FALSE;
            break;
        }
    }

    // otevreni okna operace
    if (ok)
    {
        BOOL success;
        if (!FTPOperationsList.ActivateOperationDlg(operUID, success, dropTargetWnd) || !success)
            ok = FALSE;
    }

    // pri chybe musime stopnout vsechny workery (nutne pred vymazem operace)
    if (!ok)
    {
        if (workerWithCon != NULL) // pripadne vratime zpet "control connection" z workera
            ControlConnection->GetConnectionFromWorker(workerWithCon);
        FTPOperationsList.StopWorkers(parent, operUID, -1 /* vsechny workery */);
    }

    return ok;
}

void CPluginFSInterface::GetConnectionFromWorker(CFTPWorker* workerWithCon)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetConnectionFromWorker()");
    if (ControlConnection != NULL)
        ControlConnection->GetConnectionFromWorker(workerWithCon);
}

void CPluginFSInterface::ActivateWelcomeMsg()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::ActivateWelcomeMsg()");
    if (ControlConnection != NULL)
        ControlConnection->ActivateWelcomeMsg(); // aktivujeme okno welcome-msg (z klavesnice nelze, aby ho user mohl vubec zavrit)
}

BOOL CPluginFSInterface::IsFTPS()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::IsFTPS()");
    return ControlConnection != NULL && ControlConnection->GetEncryptControlConnection() == 1;
}

BOOL CPluginFSInterface::ContainsConWithUID(int controlConUID)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::ContainsConWithUID()");
    return ControlConnection != NULL ? ControlConnection->GetUID() == controlConUID : FALSE;
}

BOOL CPluginFSInterface::ContainsHost(const char* host, int port, const char* user)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::ContainsHost()");
    return host != NULL && SalamanderGeneral->StrICmp(host, Host) == 0 && // stejny hostitel (je case-insensitive - Internetove konvence)
           Port == port &&                                                // shodny port
           user != NULL && strcmp(user, User) == 0;                       // stejne uzivatelske jmeno (je case-sensitive - Unixovy konta)
}

void CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                                  CSalamanderForViewFileOnFSAbstract* salamander,
                                  CFileData& file)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::ViewFile(%s, , , %s)", fsName, file.Name);

    parent = SalamanderGeneral->GetMsgBoxParent();
    if (ControlConnection == NULL)
    {
        TRACE_E("Unexpected situation in CPluginFSInterface::ViewFile(): ControlConnection == NULL!");
        return; // koncime
    }

    // otestujeme jestli neni v panelu jen simple listing -> v tom pripade aspon zatim nic neumime
    CPluginDataInterfaceAbstract* pluginDataIface = SalamanderGeneral->GetPanelPluginData(PANEL_SOURCE); // mame jistotu, ze je FS ve zdrojovem panelu
    if (pluginDataIface != NULL && (void*)pluginDataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        return; // koncime
    }

    BOOL doNotCacheDownload = !ControlConnection->GetUseListingsCache(); // FALSE = cachovat; TRUE = cachovat jen pro tento otevreny viewer (dalsi View bude soubor tahat znovu)

    // sestavime unikatni jmeno souboru pro disk-cache (standardni salamanderovsky format cesty)
    char uniqueFileName[FTP_USERPART_SIZE + 50]; // +50 je rezerva pro FS name; jmena v cache jsou case-sensitive
    strcpy(uniqueFileName, SalamanderGeneral->StrICmp(fsName, AssignedFSNameFTPS) == 0 ? AssignedFSNameFTPS : AssignedFSName);
    strcat(uniqueFileName, ":");
    int len = (int)strlen(uniqueFileName);
    if (doNotCacheDownload ||
        !GetFullName(file, 0 /* u View jde vzdy o soubor */, uniqueFileName + len, FTP_USERPART_SIZE + 50 - len))
    {
        doNotCacheDownload = TRUE;
    }

    // ziskame jmeno kopie souboru v disk-cache
    BOOL fileExists;
    const char* tmpFileName;
    char nameInCache[MAX_PATH];
    lstrcpyn(nameInCache, file.Name, MAX_PATH);
    if (GetFTPServerPathType(Path) == ftpsptOpenVMS)
        FTPVMSCutFileVersion(nameInCache, -1);
    SalamanderGeneral->SalMakeValidFileNameComponent(nameInCache);
    while (1)
    {
        if (doNotCacheDownload)
            sprintf(uniqueFileName + len, "%08X", GetTickCount());
        tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, nameInCache, NULL, fileExists);
        if (tmpFileName == NULL)
            return; // fatal error
        if (!doNotCacheDownload || !fileExists)
            break;

        // nemame cachovat + soubor jiz existuje (nepravdepodobne, ale stejne osetrime) - nutna zmena uniqueFileName
        Sleep(20);
        salamander->FreeFileNameInCache(uniqueFileName, fileExists, FALSE, CQuadWord(0, 0), NULL, FALSE, TRUE);
    }

    char logBuf[200 + MAX_PATH];
    _snprintf_s(logBuf, _TRUNCATE, LoadStr(fileExists ? IDS_LOGMSGVIEWCACHEDFILE : IDS_LOGMSGVIEWFILE), file.Name);
    ControlConnection->LogMessage(logBuf, -1, TRUE);

    // zjistime jestli je treba pripravit kopii souboru do disk-cache (download)
    BOOL newFileCreated = FALSE;
    BOOL newFileIncomplete = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // priprava kopie souboru (download) je nutna
    {
        TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
        int panel;
        BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);

        CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
        if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
            dataIface = NULL; // zajima nas jen data iface typu CFTPListingPluginDataInterface

        BOOL asciiMode = FALSE;
        char *name, *ext;      // pomocne promenne pro auto-detect-transfer-mode
        char buffer[MAX_PATH]; // pomocna promenna pro auto-detect-transfer-mode
        if (TransferMode == trmAutodetect)
        {
            if (dataIface != NULL) // na VMS si musime nechat jmeno "oriznout" na zaklad (cislo verze pri porovnani s maskami vadi)
                dataIface->GetBasicName(file, &name, &ext, buffer);
            else
            {
                name = file.Name;
                ext = file.Ext;
            }
            int dummy;
            if (Config.ASCIIFileMasks->PrepareMasks(dummy))
                asciiMode = Config.ASCIIFileMasks->AgreeMasks(name, ext);
            else
            {
                TRACE_E("Unexpected situation in CPluginFSInterface::ViewFile(): Config.ASCIIFileMasks->PrepareMasks() failed!");
                asciiMode = FALSE; // binarni rezim je kazdopadne mensi zlo
            }
        }
        else
            asciiMode = TransferMode == trmASCII;

        CQuadWord fileSizeInBytes;
        BOOL sizeInBytes;
        if (dataIface == NULL || !dataIface->GetSize(file, fileSizeInBytes, sizeInBytes) || !sizeInBytes)
            fileSizeInBytes.Set(-1, -1); // nezname velikost souboru

        ControlConnection->DownloadOneFile(parent, file.Name, fileSizeInBytes, asciiMode, Path,
                                           tmpFileName, &newFileCreated, &newFileIncomplete, &newFileSize,
                                           &TotalConnectAttemptNum, panel, notInPanel,
                                           User, USER_MAX_SIZE);
    }

    // otevreme viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileCreated || // viewer otevreme jen pokud je kopie souboru v poradku
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // pri chybe nulujeme "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // jeste musime zavolat FreeFileNameInCache do paru k AllocFileNameInCache (propojime
    // viewer a disk-cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileCreated,
                                    newFileSize, fileLock, fileLockOwner,
                                    doNotCacheDownload || newFileIncomplete);
}

BOOL CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::CreateDir(%s, %d, , %s,)", fsName, mode, newName);

    parent = SalamanderGeneral->GetMsgBoxParent();
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // nechame otevrit std. dialog

    if (mode == 2) // prislo jmeno ze std. dialogu v 'newName'
    {
        if (ControlConnection == NULL)
            TRACE_E("Unexpected situation in CPluginFSInterface::CreateDir(): ControlConnection == NULL!");
        else
        {
            char logBuf[200 + MAX_PATH];
            _snprintf_s(logBuf, _TRUNCATE, LoadStr(IDS_LOGMSGCREATEDIR), newName);
            ControlConnection->LogMessage(logBuf, -1, TRUE);

            TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
            int panel;
            BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
            char changedPath[FTP_MAX_PATH];
            changedPath[0] = 0;
            BOOL res = ControlConnection->CreateDir(changedPath, parent, newName, Path,
                                                    &TotalConnectAttemptNum, panel, notInPanel,
                                                    User, USER_MAX_SIZE);
            if (changedPath[0] != 0)
            {
                char postChangedPath[2 * MAX_PATH];
                sprintf(postChangedPath, "%s:", fsName);
                int len = (int)strlen(postChangedPath);
                MakeUserPart(postChangedPath + len, 2 * MAX_PATH - len, changedPath);
                SalamanderGeneral->PostChangeOnPathNotification(postChangedPath, TRUE | 0x02 /* soft refresh */);
            }
            if (res)
                return TRUE; // uspech, pristi refresh vyfokusi 'newName'
            else
                return FALSE; // vraci v 'newName' chybne jmeno adresare (otevre se znovu std. dialog)
        }
    }
    cancel = TRUE;
    return FALSE; // cancel
}

BOOL CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                     char* newName, BOOL& cancel)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::QuickRename(%s, %d, , , %d, %s,)", fsName, mode, isDir, newName);

    parent = SalamanderGeneral->GetMsgBoxParent();
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // nechame otevrit std. dialog

    if (mode == 2) // prislo jmeno ze std. dialogu v 'newName'
    {
        if (ControlConnection == NULL)
            TRACE_E("Unexpected situation in CPluginFSInterface::QuickRename(): ControlConnection == NULL!");
        else
        {
            int renameAction = 1; // 1 = prejmenovat, 2 = neprejmenovat a vratit jmeno k editaci, 3 = cancel
            CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(Path);
            BOOL isVMS = pathType == ftpsptOpenVMS; // zjistime jestli nejde nahodou o VMS listing

            // pripravime si hlasku do logu, vypiseme ji jen pokud dojde k prejmenovani
            char logBuf[200 + 2 * MAX_PATH];
            _snprintf_s(logBuf, _TRUNCATE, LoadStr(IDS_LOGMSGQUICKRENAME), file.Name, newName);

            // zpracovani masky v newName (vynechavame pokud nejde o masku (neobsahuje '*' ani '?') - aby slo prejmenovat na "test^.")
            if (strchr(newName, '*') != NULL || strchr(newName, '?') != NULL)
            {
                char targetName[2 * MAX_PATH];
                SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, file.Name, newName);
                lstrcpyn(newName, targetName, MAX_PATH);
            }

            if (!Config.AlwaysOverwrite)
            {
                BOOL tgtFileExists = FALSE; // prejmenovani prepise existujici soubor
                BOOL tgtDirExists = FALSE;  // prejmenovani nejspis selze, protoze jiz existuje adresar tohoto jmena

                BOOL caseSensitive = FTPIsCaseSensitive(pathType);

                // najdeme cim se parsoval listing
                CServerTypeList* serverTypeList = Config.LockServerTypeList();
                int serverTypeListCount = serverTypeList->Count;
                BOOL err = TRUE; // TRUE = nejsme schopni zjistit jestli nedojde k prepisu ciloveho souboru
                int i;
                for (i = 0; i < serverTypeListCount; i++)
                {
                    CServerType* serverType = serverTypeList->At(i);
                    const char* s = serverType->TypeName;
                    if (*s == '*')
                        s++;
                    if (SalamanderGeneral->StrICmp(LastServerType, s) == 0)
                    {
                        // nasli jsme serverType uspesne pouzity pro listovani, jdeme na parsovani listingu
                        if (!ParseListing(NULL, NULL, serverType, &err, isVMS, newName, caseSensitive,
                                          &tgtFileExists, &tgtDirExists))
                            err = TRUE;
                        break;
                    }
                }
                Config.UnlockServerTypeList();

                if (tgtFileExists || tgtDirExists || err)
                {
                    int res = SalamanderGeneral->SalMessageBox(parent, LoadStr(tgtFileExists ? (!isDir ? IDS_RENAME_FILEEXISTS : IDS_RENAME_FILEEXISTS2) : tgtDirExists ? IDS_RENAME_DIREXISTS
                                                                                                                                                                        : IDS_RENAME_UNABLETOGETLIST),
                                                               LoadStr(IDS_FTPPLUGINTITLE),
                                                               MB_YESNOCANCEL | (tgtFileExists && !isDir ? 0 : MB_DEFBUTTON2) | MB_ICONQUESTION);

                    if (res == IDNO)
                        renameAction = 2;
                    else
                    {
                        if (res == IDCANCEL)
                            renameAction = 3;
                    }
                }
            }

            if (renameAction == 1) // prejmenovat
            {
                ControlConnection->LogMessage(logBuf, -1, TRUE);

                TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
                int panel;
                BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
                char changedPath[FTP_MAX_PATH];
                changedPath[0] = 0;

                BOOL res = ControlConnection->QuickRename(changedPath, parent, file.Name, newName, Path,
                                                          &TotalConnectAttemptNum, panel, notInPanel,
                                                          User, USER_MAX_SIZE, isVMS, isDir);
                if (changedPath[0] != 0)
                {
                    char postChangedPath[2 * MAX_PATH];
                    sprintf(postChangedPath, "%s:", fsName);
                    int len = (int)strlen(postChangedPath);
                    MakeUserPart(postChangedPath + len, 2 * MAX_PATH - len, changedPath);
                    SalamanderGeneral->PostChangeOnPathNotification(postChangedPath, TRUE | 0x02 /* soft refresh */);
                }
                if (res)
                    return TRUE; // uspech, pristi refresh vyfokusi 'newName'
                else
                    return FALSE; // vraci v 'newName' chybne jmeno (otevre se znovu std. dialog)
            }
            else
            {
                if (renameAction == 2)
                    return FALSE; // neprejmenovat a vratit v 'newName' existujici jmeno (otevre se znovu std. dialog)
                else              // cancel
                {
                    cancel = TRUE;
                    return FALSE;
                }
            }
        }
    }
    cancel = TRUE;
    return FALSE; // cancel
}

CFTPQueueItem* CreateItemForCopyOrMoveUploadOperation(const char* name, BOOL isDir, const CQuadWord* size,
                                                      CFTPQueueItemType* type, int transferMode,
                                                      CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                      const char* targetName, CQuadWord* totalSize,
                                                      BOOL isVMS)
{
    CFTPQueueItem* item = NULL;
    *type = fqitNone;
    if (isDir) // adresar
    {
        *type = copy ? fqitUploadCopyExploreDir : fqitUploadMoveExploreDir;
        item = new CFTPQueueItemCopyMoveUploadExplore;
        if (item != NULL)
        {
            ((CFTPQueueItemCopyMoveUploadExplore*)item)->SetItemCopyMoveUploadExplore(targetPath, targetName, UPLOADTGTDIRSTATE_UNKNOWN);
        }
    }
    else // soubor
    {
        BOOL asciiTransferMode;
        if (transferMode == trmAutodetect)
        {
            char buffer[MAX_PATH];
            if (isVMS) // na VMS si musime nechat jmeno "oriznout" na zaklad (cislo verze pri porovnani s maskami vadi)
            {
                lstrcpyn(buffer, name, MAX_PATH);
                FTPVMSCutFileVersion(buffer, -1);
                name = buffer;
            }

            const char* ext = strrchr(name, '.');
            //      if (ext == NULL || ext == name) ext = name + strlen(name);   // ".cvspass" ve Windows je pripona ...
            if (ext == NULL)
                ext = name + strlen(name);
            else
                ext++;
            asciiTransferMode = oper->IsASCIIFile(name, ext);
        }
        else
            asciiTransferMode = transferMode == trmASCII;

        *type = copy ? fqitUploadCopyFile : fqitUploadMoveFile;
        item = new CFTPQueueItemCopyOrMoveUpload;
        *totalSize += *size;
        if (item != NULL)
        {
            ((CFTPQueueItemCopyOrMoveUpload*)item)->SetItemCopyOrMoveUpload(targetPath, targetName, *size, asciiTransferMode, UPLOADTGTFILESTATE_UNKNOWN);
        }
    }
    return item;
}

BOOL CPluginFSInterface::CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                                const char* sourcePath, SalEnumSelection2 next,
                                                void* nextParam, int sourceFiles, int sourceDirs,
                                                char* targetPath, BOOL* invalidPathOrCancel)
{
    if (invalidPathOrCancel != NULL)
        *invalidPathOrCancel = TRUE;

    if (mode == 1)
    {
        // zjistime jestli nebezi nejaka operace, ktera by mohla poskodit aktualni listing v panelu
        // (tento test provedeme jeste tesne pred pouzitim listingu pro upload-listing-cache, ale
        // pokud by takova operace stihla skoncit pred timto druhym testem, uz bysme poskozeni
        // nezjistili); toto neresi pripad, kdy takova operace skonci behem drag&dropu (behem tazeni
        // mysi) z disku do panelu (jde o relativne kratkou dobu, takze to proste neresime)
        CFTPServerPathType pathType = GetFTPServerPathType(Path);
        if (!PathListingMayBeOutdated && FTPOperationsList.CanMakeChangesOnPath(User, Host, Port, Path, pathType, -1))
            PathListingMayBeOutdated = TRUE;

        // pridame k cil. ceste masku *.* nebo * (budeme zpracovavat operacni masky)
        FTPAddOperationMask(pathType, targetPath, 2 * MAX_PATH, sourceFiles == 0);
        return TRUE;
    }

    if (mode == 2 || mode == 3)
    {
        // v 'targetPath' je neupravena cesta zadana uzivatelem (jedine co o ni vime je, ze je
        // na FTP, jinak by tuto metodu Salamander nevolal)
        int isFTPS = SalamanderGeneral->StrNICmp(targetPath, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 &&
                     targetPath[AssignedFSNameLenFTPS] == ':';

        // overime, zda bude mozne rozsifrovat pripadne heslo pro default proxy (do SetConnectionParameters() smime vstoupit pouze v pripade, ze je to mozne)
        if (!Config.FTPProxyServerList.EnsurePasswordCanBeDecrypted(SalamanderGeneral->GetMsgBoxParent(), Config.DefaultProxySrvUID))
        {
            return FALSE; // fatal error
        }

        char* userPart = strchr(targetPath, ':') + 1; // v 'targetPath' musi byt fs-name + ':'
        char newUserPart[FTP_USERPART_SIZE + 1];
        lstrcpyn(newUserPart, userPart, FTP_USERPART_SIZE);
        char *u, *host, *p, *path, *password;
        char firstCharOfPath = '/';
        int userLength = 0;
        if (ControlConnection != NULL)
            userLength = FTPGetUserLength(User);
        FTPSplitPath(newUserPart, &u, &password, &host, &p, &path, &firstCharOfPath, userLength);
        if (password != NULL && *password == 0)
            password = NULL;
        char user[USER_MAX_SIZE];
        if (u == NULL || *u == 0)
            strcpy(user, FTP_ANONYMOUS);
        else
            lstrcpyn(user, u, USER_MAX_SIZE);
        int port = IPPORT_FTP;
        if (p != NULL && *p != 0)
            port = atoi(p);

        if (ControlConnection == NULL) // otevreni spojeni (otevreni cesty na FTP server)
        {
            TotalConnectAttemptNum = 1; // otevirame spojeni = 1. pokus o otevreni spojeni

            ControlConnection = new CControlConnectionSocket;
            if (ControlConnection == NULL || !ControlConnection->IsGood())
            {
                if (ControlConnection != NULL) // nedostatek systemovych prostredku pro alokaci objektu
                {
                    DeleteSocket(ControlConnection);
                    ControlConnection = NULL;
                }
                else
                    TRACE_E(LOW_MEMORY);
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                return TRUE;                                   // fatal error
            }

            AutodetectSrvType = TRUE; // pouzivame automatickou detekci typu serveru
            LastServerType[0] = 0;

            if (host == NULL || *host == 0)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_HOSTNAMEMISSING),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                return FALSE;                                  // fatal error
            }

            lstrcpyn(Host, host, HOST_MAX_SIZE);
            Port = port;
            lstrcpyn(User, user, USER_MAX_SIZE);
            Path[0] = 0;

            char anonymousPasswd[PASSWORD_MAX_SIZE];
            Config.GetAnonymousPasswd(anonymousPasswd, PASSWORD_MAX_SIZE);

            if (strcmp(user, FTP_ANONYMOUS) == 0 && password == NULL)
                password = anonymousPasswd;
            ControlConnection->SetConnectionParameters(Host, Port, User, HandleNULLStr(password),
                                                       Config.UseListingsCache, NULL, Config.PassiveMode,
                                                       NULL, Config.KeepAlive, Config.KeepAliveSendEvery,
                                                       Config.KeepAliveStopAfter, Config.KeepAliveCommand,
                                                       -2 /* default proxy server */,
                                                       isFTPS, isFTPS, Config.CompressData);
            TransferMode = Config.TransferMode;

            // pripojime se na server
            ControlConnection->SetStartTime();
            if (!ControlConnection->StartControlConnection(SalamanderGeneral->GetMsgBoxParent(),
                                                           User, USER_MAX_SIZE, FALSE, RescuePath,
                                                           FTP_MAX_PATH, &TotalConnectAttemptNum,
                                                           NULL, FALSE, -1, FALSE))
            { // nepodarilo se pripojit, uvolnime objekt socketu (signalizuje stav "nikdy nebylo pripojeno")
                DeleteSocket(ControlConnection);
                ControlConnection = NULL;
                Logs.RefreshListOfLogsInLogsDlg();
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                return TRUE;                                   // cancel
            }
            lstrcpyn(HomeDir, RescuePath, FTP_MAX_PATH); // ulozime si aktualni cestu po loginu na server (home-dir)
        }
        else // overime, jestli je cilova cesta na server, ktery je otevreny v tomto FS
        {
            if (isFTPS != ControlConnection->GetEncryptControlConnection() ||  // ma/nema byt FTPS, ale neni/je
                strcmp(user, User) != 0 ||                                     // jine uzivatelske jmeno (je case-sensitive - Unixovy konta)
                host == NULL || SalamanderGeneral->StrICmp(host, Host) != 0 || // jiny hostitel (je case-insensitive - Internetove konvence - casem mozna lepsi test IP adres)
                port != Port)                                                  // jiny port
            {
                if (invalidPathOrCancel != NULL)
                    *invalidPathOrCancel = FALSE;
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
                return FALSE;                                  // je potreba najit jiny FS
            }
            ControlConnection->SetStartTime();
        }

        char tgtPath[FTP_MAX_PATH];
        char mask[MAX_PATH];
        strcpy(mask, "*");
        if (path != NULL)
        {
            BOOL isSpecRootPath = FALSE;
            tgtPath[0] = firstCharOfPath;
            lstrcpyn(tgtPath + 1, path, FTP_MAX_PATH - 1);
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password

            // zjistime typ cesty + pripadne preskocime '/' nebo '\\' na zacatku cesty (za host-name)
            CFTPServerPathType pathType = ftpsptEmpty;
            if (HomeDir[0] == 0 || HomeDir[0] != '/' && HomeDir[0] != '\\')
            { // preskakovani '/' nebo '\\' na zacatku cesty zkousime jen pokud na ne nezacina home-dir serveru (vysledek PWD po loginu)
                pathType = GetFTPServerPathType(tgtPath + 1);
                if (pathType == ftpsptOpenVMS || pathType == ftpsptMVS || pathType == ftpsptIBMz_VM ||
                    pathType == ftpsptOS2 && GetFTPServerPathType("") == ftpsptOS2) // OS/2 cesty se pletou s unixovou cestou "/C:/path", proto rozlisujeme OS/2 cesty i jen podle SYST-reply
                {                                                                   // VMS + MVS + IBM_z/VM + OS/2 nemaji '/' ani '\\' na zacatku cesty
                    memmove(tgtPath, tgtPath + 1, strlen(tgtPath) + 1);             // vyhodime znak '/' nebo '\\' ze zacatku cesty
                    if (tgtPath[0] == 0)                                            // obecny root -> doplnime podle typu systemu
                    {
                        isSpecRootPath = TRUE;
                        if (pathType == ftpsptOpenVMS)
                            lstrcpyn(tgtPath, "[000000]", FTP_MAX_PATH);
                        else
                        {
                            if (pathType == ftpsptMVS)
                                lstrcpyn(tgtPath, "''", FTP_MAX_PATH);
                            else
                            {
                                if (pathType == ftpsptIBMz_VM)
                                {
                                    if (HomeDir[0] == 0 || !FTPGetIBMz_VMRootPath(tgtPath, FTP_MAX_PATH, HomeDir))
                                    {
                                        lstrcpyn(tgtPath, "/", FTP_MAX_PATH); // testovany server podporoval Unixovy root "/", mozna se nekdo ozve, pak budeme resit dale...
                                    }
                                }
                                else
                                {
                                    if (pathType == ftpsptOS2)
                                    {
                                        if (HomeDir[0] == 0 || !FTPGetOS2RootPath(tgtPath, FTP_MAX_PATH, HomeDir))
                                        {
                                            lstrcpyn(tgtPath, "/", FTP_MAX_PATH); // zkusime aspon Unixovy root "/", nic jineho neumime, mozna se nekdo ozve, pak budeme resit dale...
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
                else
                    pathType = GetFTPServerPathType(tgtPath);
            }
            else
                pathType = GetFTPServerPathType(tgtPath);

            if (pathType == ftpsptEmpty || pathType == ftpsptUnknown)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_INVALIDPATH),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                return FALSE; // invalid path
            }

            // pokud jde o root (jen specialni pripady, dalsi typy root cest projdou dale), nemaji dalsi upravy
            // a analyza cesty smysl - pouzijeme cestu jak je + masku "*"
            if (!isSpecRootPath)
            {
                // pokud cesta konci separatorem, chapeme ji jako cestu bez masky (napr. "/pub/dir/" nebo
                // "PUB$DEVICE:[PUB.VMS.]"), jinak pokracujeme v analyze cesty
                if (!FTPPathEndsWithDelimiter(pathType, tgtPath))
                {
                    char cutTgtPath[FTP_MAX_PATH];
                    lstrcpyn(cutTgtPath, tgtPath, FTP_MAX_PATH);
                    char cutMask[MAX_PATH];
                    BOOL cutMaybeFileName = FALSE;
                    if (FTPCutDirectory(pathType, cutTgtPath, FTP_MAX_PATH, cutMask, MAX_PATH, &cutMaybeFileName))
                    { // pokud se da cast cesty oriznout, budeme zjistovat jestli nejde o masku (jinak to je nejspis root-cesta, pouzijeme masku "*")
                        char cutTgtPathIBMz_VM[FTP_MAX_PATH];
                        cutTgtPathIBMz_VM[0] = 0;
                        char cutMaskIBMz_VM[MAX_PATH];
                        cutMaskIBMz_VM[0] = 0;
                        BOOL done = FALSE;
                        if (pathType == ftpsptIBMz_VM)
                        {
                            lstrcpyn(cutTgtPathIBMz_VM, tgtPath, FTP_MAX_PATH);
                            if (FTPIBMz_VmCutTwoDirectories(cutTgtPathIBMz_VM, FTP_MAX_PATH, cutMaskIBMz_VM, MAX_PATH))
                            {
                                char* sep = strchr(cutMaskIBMz_VM, '.');
                                char* ast = strchr(cutMaskIBMz_VM, '*');
                                char* exc = strchr(cutMaskIBMz_VM, '?');
                                if (ast != NULL && ast < sep || exc != NULL && exc < sep)
                                { // odrizla cast obsahuje '*' nebo '?' (wildcardy) pred '.' (urcite maska pro soubor napr. "*.*")
                                    lstrcpyn(tgtPath, cutTgtPathIBMz_VM, FTP_MAX_PATH);
                                    lstrcpyn(mask, cutMaskIBMz_VM, MAX_PATH);
                                    done = TRUE;
                                }
                            }
                            else
                            {
                                cutTgtPathIBMz_VM[0] = 0;
                                cutMaskIBMz_VM[0] = 0;
                            }
                        }
                        if (!done)
                        {
                            if (cutTgtPathIBMz_VM[0] == 0 && // potrebujeme testnout jestli je v 'cutMaskIBMz_VM' maska
                                    (strchr(cutMask, '*') != NULL || strchr(cutMask, '?') != NULL) ||
                                pathType == ftpsptOpenVMS && cutMaybeFileName)
                            { // odrizla cast obsahuje '*' nebo '?' (wildcardy) nebo jde o VMS jmeno souboru (musi byt maska, cilova cesta je cesta k tomuto souboru)
                                lstrcpyn(tgtPath, cutTgtPath, FTP_MAX_PATH);
                                lstrcpyn(mask, cutMask, MAX_PATH);
                            }
                            else
                            {
                                TotalConnectAttemptNum = 1; // zahajeni uzivatelem pozadovane akce -> je-li treba znovu pripojit, jde o 1. pokus reconnect
                                int panel;
                                BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);
                                BOOL success = FALSE;
                                char replyBuf[700];
                                if (strchr(cutMask, '*') != NULL || strchr(cutMask, '?') != NULL ||
                                    ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                                             SalamanderGeneral->GetMsgBoxParent(),
                                                                             tgtPath, User, USER_MAX_SIZE,
                                                                             &success, replyBuf, 700, NULL, &TotalConnectAttemptNum,
                                                                             NULL, FALSE, NULL))
                                {
                                    if (!success) // pokud je 'tgtPath' platna cesta, je maska "*"; jinak pokracujeme
                                    {
                                        if (ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                                                     SalamanderGeneral->GetMsgBoxParent(),
                                                                                     cutTgtPath, User, USER_MAX_SIZE,
                                                                                     &success, replyBuf, 700, NULL, &TotalConnectAttemptNum,
                                                                                     NULL, FALSE, NULL))
                                        {
                                            if (success) // 'cutTgtPath' je platna cesta - maska je 'cutMask'
                                            {
                                                lstrcpyn(tgtPath, cutTgtPath, FTP_MAX_PATH);
                                                lstrcpyn(mask, cutMask, MAX_PATH);
                                            }
                                            else // jinak pokracujeme
                                            {
                                                if (cutTgtPathIBMz_VM[0] != 0)
                                                {
                                                    if (ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                                                                 SalamanderGeneral->GetMsgBoxParent(),
                                                                                                 cutTgtPathIBMz_VM, User, USER_MAX_SIZE,
                                                                                                 &success, replyBuf, 700, NULL, &TotalConnectAttemptNum,
                                                                                                 NULL, FALSE, NULL))
                                                    {
                                                        if (success) // 'cutTgtPathIBMz_VM' je platna cesta - maska je 'cutMaskIBMz_VM'
                                                        {
                                                            lstrcpyn(tgtPath, cutTgtPathIBMz_VM, FTP_MAX_PATH);
                                                            lstrcpyn(mask, cutMaskIBMz_VM, MAX_PATH);
                                                            done = TRUE;
                                                        }
                                                    }
                                                    else // nelze navazat spojeni (i pokud si user nepreje reconnect)
                                                    {
                                                        return TRUE; // cancel
                                                    }
                                                }
                                                if (!done) // vypis chyby cesty userovi
                                                {
                                                    char errBuf[900 + FTP_MAX_PATH];
                                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR),
                                                                (cutTgtPathIBMz_VM[0] != 0 ? cutTgtPathIBMz_VM : cutTgtPath), replyBuf);
                                                    SalamanderGeneral->ShowMessageBox(errBuf, LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                                                    return FALSE; // invalid path
                                                }
                                            }
                                        }
                                        else // nelze navazat spojeni (i pokud si user nepreje reconnect)
                                        {
                                            return TRUE; // cancel
                                        }
                                    }
                                }
                                else // nelze navazat spojeni (i pokud si user nepreje reconnect)
                                {
                                    return TRUE; // cancel
                                }
                            }
                        }
                    }
                }
            }
        }
        else // cilova cesta je home-dir
        {
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // mazeme pamet, ve ktere se objevil password
            if (HomeDir[0] == 0)                           // home-dir neni definovan (nektere servery nuti nejprve volat CWD, az pak ma PWD co vracet)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_HOMEDIRNOTDEFINED),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                return FALSE; // invalid path
            }
            lstrcpyn(tgtPath, HomeDir, FTP_MAX_PATH);
        }

        // presun/kopie vice souboru/adresaru do jednoho jmena (budou se prepisovat) je nejspis nesmysl
        if (sourceFiles + sourceDirs > 1 && strchr(mask, '*') == NULL && strchr(mask, '?') == NULL)
        {
            if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_COPYMOVE_NONSENSE),
                                                 LoadStr(IDS_FTPPLUGINTITLE),
                                                 MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
            {
                return FALSE; // invalid path
            }
        }

        // cesta je rozanalyzovana, zacneme operaci:
        // 'tgtPath' je cilova cesta, 'mask' je operacni maska
        BOOL success = FALSE; // predpripravime cancel/error operace

        char dlgSubjectSrc[MAX_PATH + 100];
        if (sourceFiles + sourceDirs <= 1) // jedna oznacena polozka
        {
            BOOL isDir;
            const char* name = next(parent, 0, NULL, &isDir, NULL, NULL, NULL, nextParam, NULL);
            if (name != NULL)
            {
                SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, -1,
                                                              sourceFiles, sourceDirs, name, isDir, TRUE);
            }
            else
            {
                TRACE_E("Unexpected situation in CPluginFSInterface::CopyOrMoveFromDiskToFS()!");
                dlgSubjectSrc[0] = 0;
            }
            next(NULL, -1, NULL, NULL, NULL, NULL, NULL, nextParam, NULL); // reset enumerace
        }
        else // nekolik adresaru a souboru
        {
            SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, -1,
                                                          sourceFiles, sourceDirs, NULL, FALSE, TRUE);
        }

        // zalozime objekt operace
        CFTPOperation* oper = new CFTPOperation;
        if (oper != NULL)
        {
            oper->SetEncryptControlConnection(ControlConnection->GetEncryptControlConnection());
            oper->SetEncryptDataConnection(ControlConnection->GetEncryptDataConnection());
            CCertificate* cert = ControlConnection->GetCertificate();
            oper->SetCertificate(cert);
            if (cert)
                cert->Release();
            oper->SetCompressData(ControlConnection->GetCompressData());
            if (ControlConnection->InitOperation(oper)) // inicializace spojeni se serverem podle "control connection"
            {
                oper->SetBasicData(dlgSubjectSrc, (AutodetectSrvType ? NULL : LastServerType));
                char targetPath2[2 * MAX_PATH];
                sprintf(targetPath2, "%s:", fsName);
                int targetPathLen = (int)strlen(targetPath2);
                MakeUserPart(targetPath2 + targetPathLen, 2 * MAX_PATH - targetPathLen, tgtPath);
                char asciiFileMasks[MAX_GROUPMASK];
                Config.ASCIIFileMasks->GetMasksString(asciiFileMasks);
                CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(tgtPath);
                BOOL is_AS_400_QSYS_LIB_Path = pathType == ftpsptAS400 &&
                                               FTPIsPrefixOfServerPath(ftpsptAS400, "/QSYS.LIB", tgtPath);
                if (oper->SetOperationCopyMoveUpload(copy, sourcePath, '\\', !copy,
                                                     copy ? FALSE : (sourceDirs > 0),
                                                     targetPath2, FTPGetPathDelimiter(pathType),
                                                     TRUE, sourceDirs > 0, asciiFileMasks,
                                                     TransferMode == trmAutodetect, TransferMode == trmASCII,
                                                     Config.UploadCannotCreateFile,
                                                     Config.UploadCannotCreateDir,
                                                     Config.UploadFileAlreadyExists,
                                                     Config.UploadDirAlreadyExists,
                                                     Config.UploadRetryOnCreatedFile,
                                                     Config.UploadRetryOnResumedFile,
                                                     Config.UploadAsciiTrModeButBinFile))
                {
                    int operUID;
                    if (FTPOperationsList.AddOperation(oper, &operUID))
                    {
                        BOOL ok = TRUE;

                        // sestavime frontu polozek operace
                        CFTPQueue* queue = new CFTPQueue;
                        if (queue != NULL)
                        {
                            CQuadWord totalSize(0, 0); // celkova velikost (v bytech nebo blocich)
                            BOOL isDir;
                            const char* name;
                            const char* dosName; // dummy
                            CQuadWord size;
                            DWORD attr; // dummy
                            BOOL useMask = strchr(mask, '*') != NULL || strchr(mask, '?') != NULL;
                            char linkName[MAX_PATH];
                            lstrcpyn(linkName, sourcePath, _countof(linkName));
                            SalamanderGeneral->SalPathAddBackslash(linkName, _countof(linkName));
                            char* linkNameEnd = linkName + strlen(linkName);
                            BOOL ignoreAll = FALSE;
                            while ((name = next(parent, 0, &dosName, &isDir, &size, &attr, NULL, nextParam, NULL)) != NULL)
                            {
                                // vytvorime cilove jmeno podle operacni masky (vynechavame pokud nejde
                                // o masku (neobsahuje '*' ani '?') - aby slo prejmenovat na "test^.")
                                char targetName[2 * MAX_PATH];
                                if (useMask)
                                    SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, name, mask);
                                else
                                    lstrcpyn(targetName, mask, 2 * MAX_PATH);
                                if (is_AS_400_QSYS_LIB_Path)
                                    FTPAS400AddFileNamePart(targetName);

                                // linky: size == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
                                BOOL cancel = FALSE;
                                if (!isDir && (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                                    linkNameEnd - linkName + strlen(name) < _countof(linkName))
                                { // jde o link na soubor a jmeno linku neni prilis dlouhe (to se pripadne ohlasi jinde)
                                    CQuadWord linkSize;
                                    strcpy(linkNameEnd, name);
                                    if (SalamanderGeneral->GetLinkTgtFileSize(parent, linkName, &linkSize, &cancel, &ignoreAll))
                                        size = linkSize;
                                }

                                CFTPQueueItemType type;
                                CFTPQueueItem* item = cancel ? NULL : CreateItemForCopyOrMoveUploadOperation(name, isDir, &size, &type, TransferMode, oper, copy, tgtPath, targetName, &totalSize, pathType == ftpsptOpenVMS);
                                if (item != NULL)
                                {
                                    if (ok)
                                        item->SetItem(-1, type, sqisWaiting, ITEMPR_OK, sourcePath, name);
                                    if (!ok || !queue->AddItem(item)) // pridani operace do fronty
                                    {
                                        ok = FALSE;
                                        delete item;
                                    }
                                }
                                else
                                {
                                    if (!cancel)
                                        TRACE_E(LOW_MEMORY);
                                    ok = FALSE;
                                }
                                // zjistime jestli ma cenu pokracovat (pokud neni chyba)
                                if (!ok)
                                    break;
                            }
                            if (ok)
                            {
                                oper->SetChildItems(queue->GetCount(), 0, 0, 0);
                                oper->AddToTotalSize(totalSize, TRUE);
                            }
                            else
                            {
                                delete queue;
                                queue = NULL;
                            }
                        }
                        else
                        {
                            TRACE_E(LOW_MEMORY);
                            ok = FALSE;
                        }

                        if (ok) // mame naplnenou frontu s polozkama operace
                        {
                            // naplnime UploadListingCache soucasnym obsahem panelu (pokud je cilova cesta v panelu
                            // a panel obsahuje nepreruseny, neporuseny a up-to-date listing)
                            int panel;
                            if (SalamanderGeneral->GetPanelWithPluginFS(this, panel) &&
                                FTPIsTheSameServerPath(pathType, Path, tgtPath) &&
                                !PathListingIsIncomplete && !PathListingIsBroken &&
                                !PathListingMayBeOutdated && PathListing != NULL &&
                                !FTPOperationsList.CanMakeChangesOnPath(User, Host, Port, Path, pathType, operUID))
                            {
                                char* welcomeReply = ControlConnection->AllocServerFirstReply();
                                char* systReply = ControlConnection->AllocServerSystemReply();
                                if (welcomeReply != NULL && systReply != NULL)
                                {
                                    UploadListingCache.AddOrUpdateListing(User, Host, Port, Path, pathType,
                                                                          PathListing, PathListingLen,
                                                                          PathListingDate, PathListingStartTime,
                                                                          FALSE, welcomeReply, systReply,
                                                                          AutodetectSrvType ? NULL : LastServerType);
                                }
                                if (welcomeReply != NULL)
                                    SalamanderGeneral->Free(welcomeReply);
                                if (systReply != NULL)
                                    SalamanderGeneral->Free(systReply);
                            }

                            oper->SetQueue(queue); // nastavime operaci frontu jejich polozek
                            queue = NULL;
                            // FIXME: asi neni kam dat checkbox "only add to queue": if (Config.UploadAddToQueue) success = TRUE;  // provest operaci pozdeji -> prozatim jde o uspech operace
                            // else // provest operaci v aktivni "control connection"
                            // {
                            // otevreme okno s progresem operace a spustime operaci
                            if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, NULL))
                                success = TRUE; // uspech operace
                            else
                                ok = FALSE;
                            // }
                        }
                        if (!ok)
                            FTPOperationsList.DeleteOperation(operUID, TRUE);
                        oper = NULL; // operace uz je pridana v poli, nebudeme ji uvolnovat pres 'delete' (viz nize)
                    }
                }
            }
            if (oper != NULL)
                delete oper;
        }
        else
            TRACE_E(LOW_MEMORY);

        if (success && invalidPathOrCancel != NULL)
            *invalidPathOrCancel = FALSE; // vracime "uspech" (jinak "chyba/cancel")
        return TRUE;
    }
    return FALSE; // neznamy 'mode'
}

void CPluginFSInterface::ShowSecurityInfo(HWND hParent)
{
    if (ControlConnection)
    {
        CCertificate* cert = ControlConnection->GetCertificate();
        if (cert != NULL)
        {
            cert->ShowCertificate(hParent);

            char errBuf[300];
            int panel;
            if (SalamanderGeneral->GetPanelWithPluginFS(this, panel))
            { // uzivatel mohl certifikat importovat nebo naopak smazat z MS storu, overime stav a ukazeme v panelu
                bool verified = cert->CheckCertificate(errBuf, 300);
                cert->SetVerified(verified);
                SalamanderGeneral->ShowSecurityIcon(panel, TRUE, verified,
                                                    LoadStr(verified ? IDS_SSL_SECURITY_OK : IDS_SSL_SECURITY_UNVERIFIED));
            }
            cert->Release();
        }
    }
}
