// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
        return FALSE; // cancellation
    }

    // test whether the panel has only a simple listing -> in that case we cannot do anything yet
    CPluginDataInterfaceAbstract* pluginDataIface = SalamanderGeneral->GetPanelPluginData(panel);
    if (pluginDataIface != NULL && (void*)pluginDataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        return FALSE; // cancellation
    }

    // build a description of what will be processed for the Change Attributes dialog
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
    BOOL displayWarning = TRUE; // warning that this is probably not a Unix server, so chmod will not work
    if (pluginDataIface != NULL && pluginDataIface != &SimpleListPluginDataInterface)
    { // we only care about data iface objects of type CFTPListingPluginDataInterface
        CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
        int rightsCol = dataIface->FindRightsColumn();
        if (rightsCol != -1) // if the Rights column exists (it does not have to be Unix, that is handled later)
        {
            displayWarning = FALSE;
            const CFileData* f = NULL; // pointer to the file/directory in the panel that should be processed
            BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
            BOOL isDir = FALSE; // TRUE if 'f' is a directory
            int index = 0;
            while (1)
            {
                // fetch data about the processed file/directory
                if (focused)
                    f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                else
                    f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                // determine attributes of the file/directory
                if (f != NULL)
                {
                    char* rights = dataIface->GetStringFromColumn(*f, rightsCol);
                    DWORD actAttr;
                    if (GetAttrsFromUNIXRights(&actAttr, &attrDiff, rights)) // convert the string to a number
                    {
                        if (attr == -1)
                            attr = actAttr; // first file/directory
                        else
                        {
                            if (attr != actAttr) // if they differ, store the differences
                                attrDiff |= (attr ^ actAttr);
                        }
                    }
                    else // not a (normal) Unix system, give up
                    {
                        displayWarning = TRUE;
                        attr = -1;
                        attrDiff = 0;
                        break;
                    }
                }

                // determine whether it makes sense to continue (if there is another selected item)
                if (focused || f == NULL)
                    break;
            }
        }
    }

    if (!displayWarning || // optionally display a warning that this is not a UNIX server with the traditional rights model (e.g. we do not support ACL)
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
            BOOL failed = TRUE; // pre-initialize the operation error
            // create the operation object
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
                if (ControlConnection->InitOperation(oper)) // initialize the connection to the server according to the "control connection"
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

                        // build the queue of operation items
                        CFTPQueue* queue = new CFTPQueue;
                        if (queue != NULL)
                        {
                            CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
                            if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
                                dataIface = NULL; // we only care about data iface objects of type CFTPListingPluginDataInterface
                            int rightsCol = -1;   // index of the column with rights (used to detect links)
                            if (dataIface != NULL)
                                rightsCol = dataIface->FindRightsColumn();
                            const CFileData* f = NULL; // pointer to the file/directory/link in the panel that should be processed
                            BOOL isDir = FALSE;        // TRUE if 'f' is a directory
                            BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
                            int skippedItems = 0;  // number of skipped items inserted into the queue
                            int uiNeededItems = 0; // number of user-input-needed items inserted into the queue
                            int index = 0;
                            while (1)
                            {
                                // fetch data about the processed file
                                if (focused)
                                    f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
                                else
                                    f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

                                // process the file/directory/link
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
                                        if (!ok || !queue->AddItem(item)) // add the operation to the queue
                                        {
                                            ok = FALSE;
                                            delete item;
                                        }
                                    }
                                    else
                                    {
                                        if (!skip) // only if this is not skipping the item but a low-memory error
                                        {
                                            TRACE_E(LOW_MEMORY);
                                            ok = FALSE;
                                        }
                                    }
                                }
                                // determine whether it makes sense to continue (if there is no error and another selected item exists)
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

                        if (ok) // the queue with operation items has been filled
                        {
                            if (!emptyQueue) // only if the queue of operation items is not empty
                            {
                                oper->SetQueue(queue); // set the queue of its items for the operation
                                queue = NULL;
                                if (Config.ChAttrAddToQueue)
                                    failed = FALSE; // perform the operation later -> for now the operation is successful
                                else                // perform the operation in the active "control connection"
                                {
                                    // open the operation progress window and start the operation
                                    if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, NULL))
                                        failed = FALSE; // operation succeeded
                                    else
                                        ok = FALSE;
                                }
                            }
                            else
                            {
                                failed = FALSE; // operation succeeded (but there is nothing to do)
                                FTPOperationsList.DeleteOperation(operUID, TRUE);
                                delete queue;
                                queue = NULL;
                            }
                        }
                        if (!ok)
                            FTPOperationsList.DeleteOperation(operUID, TRUE);
                        oper = NULL; // the operation is already added in the array, do not free it with 'delete' (see below)
                    }
                }
                if (oper != NULL)
                    delete oper;
            }
            else
                TRACE_E(LOW_MEMORY);
            return !failed; // return the operation success (TRUE = clear the selection in the panel)
        }
    }
    return FALSE; // cancellation
}

BOOL CPluginFSInterface::RunOperation(HWND parent, int operUID, CFTPOperation* oper, HWND dropTargetWnd)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::RunOperation(, %d, ,)", operUID);

    BOOL ok = TRUE;

    CFTPWorker* workerWithCon = NULL; // if we passed the connection, this points to who received it
    int i;
    for (i = 0; i < 1; i++) // FIXME: eventually we may place the initial number of operation workers into the configuration: just replace "1" with the appropriate count...
    {
        CFTPWorker* newWorker = oper->AllocNewWorker();
        if (newWorker != NULL)
        {
            if (!SocketsThread->AddSocket(newWorker) ||   // add it to the sockets thread
                !newWorker->RefreshCopiesOfUIDAndMsg() || // refresh copies of UID+Msg (they changed)
                !oper->AddWorker(newWorker))              // add it among the operation workers
            {
                DeleteSocket(newWorker);
                ok = FALSE;
                break;
            }
            else // the worker was successfully created and placed among the operation workers
            {
                if (i == 0) // hand over the current "control connection" to the first worker
                {
                    ControlConnection->GiveConnectionToWorker(newWorker, parent);
                    workerWithCon = newWorker;
                }
            }
        }
        else // error, cancel the operation
        {
            ok = FALSE;
            break;
        }
    }

    // open the operation window
    if (ok)
    {
        BOOL success;
        if (!FTPOperationsList.ActivateOperationDlg(operUID, success, dropTargetWnd) || !success)
            ok = FALSE;
    }

    // in case of an error we must stop all workers (necessary before deleting the operation)
    if (!ok)
    {
        if (workerWithCon != NULL) // optionally take back the "control connection" from the worker
            ControlConnection->GetConnectionFromWorker(workerWithCon);
        FTPOperationsList.StopWorkers(parent, operUID, -1 /* all workers */);
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
        ControlConnection->ActivateWelcomeMsg(); // activate the welcome-msg window (the keyboard cannot do it, so the user can actually close it)
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
    return host != NULL && SalamanderGeneral->StrICmp(host, Host) == 0 && // same host (case-insensitive - Internet conventions)
           Port == port &&                                                // identical port
           user != NULL && strcmp(user, User) == 0;                       // same user name (case-sensitive - Unix accounts)
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
        return; // we are done
    }

    // test whether the panel contains only a simple listing -> in that case we cannot do anything yet
    CPluginDataInterfaceAbstract* pluginDataIface = SalamanderGeneral->GetPanelPluginData(PANEL_SOURCE); // we are sure the FS is in the source panel
    if (pluginDataIface != NULL && (void*)pluginDataIface == (void*)&SimpleListPluginDataInterface)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_NEEDPARSEDLISTING),
                                         LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONINFORMATION);
        return; // we are done
    }

    BOOL doNotCacheDownload = !ControlConnection->GetUseListingsCache(); // FALSE = cache it; TRUE = cache only for this open viewer (the next View will download the file again)

    // build a unique file name for the disk cache (standard Salamander path format)
    char uniqueFileName[FTP_USERPART_SIZE + 50]; // +50 is a reserve for the FS name; cache names are case-sensitive
    strcpy(uniqueFileName, SalamanderGeneral->StrICmp(fsName, AssignedFSNameFTPS) == 0 ? AssignedFSNameFTPS : AssignedFSName);
    strcat(uniqueFileName, ":");
    int len = (int)strlen(uniqueFileName);
    if (doNotCacheDownload ||
        !GetFullName(file, 0 /* View always works with a file */, uniqueFileName + len, FTP_USERPART_SIZE + 50 - len))
    {
        doNotCacheDownload = TRUE;
    }

    // obtain the name of the file copy in the disk cache
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

        // no caching + the file already exists (unlikely, but handled anyway) - we must change uniqueFileName
        Sleep(20);
        salamander->FreeFileNameInCache(uniqueFileName, fileExists, FALSE, CQuadWord(0, 0), NULL, FALSE, TRUE);
    }

    char logBuf[200 + MAX_PATH];
    _snprintf_s(logBuf, _TRUNCATE, LoadStr(fileExists ? IDS_LOGMSGVIEWCACHEDFILE : IDS_LOGMSGVIEWFILE), file.Name);
    ControlConnection->LogMessage(logBuf, -1, TRUE);

    // determine whether a copy of the file needs to be prepared in the disk cache (download)
    BOOL newFileCreated = FALSE;
    BOOL newFileIncomplete = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // preparing a copy of the file (download) is required
    {
        TotalConnectAttemptNum = 1; // start of a user-requested action -> if reconnecting is needed, this is the first reconnect attempt
        int panel;
        BOOL notInPanel = !SalamanderGeneral->GetPanelWithPluginFS(this, panel);

        CFTPListingPluginDataInterface* dataIface = (CFTPListingPluginDataInterface*)pluginDataIface;
        if (dataIface != NULL && (void*)dataIface == (void*)&SimpleListPluginDataInterface)
            dataIface = NULL; // we only care about data iface objects of type CFTPListingPluginDataInterface

        BOOL asciiMode = FALSE;
        char *name, *ext;      // helper variables for auto-detect-transfer-mode
        char buffer[MAX_PATH]; // helper variable for auto-detect-transfer-mode
        if (TransferMode == trmAutodetect)
        {
            if (dataIface != NULL) // on VMS we must have the name trimmed to the base (the version number would break mask comparison)
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
                asciiMode = FALSE; // the binary mode is still the lesser evil
            }
        }
        else
            asciiMode = TransferMode == trmASCII;

        CQuadWord fileSizeInBytes;
        BOOL sizeInBytes;
        if (dataIface == NULL || !dataIface->GetSize(file, fileSizeInBytes, sizeInBytes) || !sizeInBytes)
            fileSizeInBytes.Set(-1, -1); // the file size is unknown

        ControlConnection->DownloadOneFile(parent, file.Name, fileSizeInBytes, asciiMode, Path,
                                           tmpFileName, &newFileCreated, &newFileIncomplete, &newFileSize,
                                           &TotalConnectAttemptNum, panel, notInPanel,
                                           User, USER_MAX_SIZE);
    }

    // open the viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileCreated || // open the viewer only if the copy of the file is fine
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // on error reset the "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // we still have to call FreeFileNameInCache as a pair to AllocFileNameInCache (link
    // the viewer and the disk cache)
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
        return FALSE; // let the standard dialog open

    if (mode == 2) // a name came from the standard dialog in 'newName'
    {
        if (ControlConnection == NULL)
            TRACE_E("Unexpected situation in CPluginFSInterface::CreateDir(): ControlConnection == NULL!");
        else
        {
            char logBuf[200 + MAX_PATH];
            _snprintf_s(logBuf, _TRUNCATE, LoadStr(IDS_LOGMSGCREATEDIR), newName);
            ControlConnection->LogMessage(logBuf, -1, TRUE);

            TotalConnectAttemptNum = 1; // start of a user-requested action -> if reconnecting is needed, this is the first reconnect attempt
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
                return TRUE; // success, the next refresh will focus on 'newName'
            else
                return FALSE; // returns the incorrect directory name in 'newName' (the standard dialog opens again)
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
        return FALSE; // let the standard dialog open

    if (mode == 2) // a name came from the standard dialog in 'newName'
    {
        if (ControlConnection == NULL)
            TRACE_E("Unexpected situation in CPluginFSInterface::QuickRename(): ControlConnection == NULL!");
        else
        {
            int renameAction = 1; // 1 = rename, 2 = do not rename and return the name for editing, 3 = cancel
            CFTPServerPathType pathType = ControlConnection->GetFTPServerPathType(Path);
            BOOL isVMS = pathType == ftpsptOpenVMS; // determine whether this might be a VMS listing

            // prepare the message for the log, print it only if the rename actually happens
            char logBuf[200 + 2 * MAX_PATH];
            _snprintf_s(logBuf, _TRUNCATE, LoadStr(IDS_LOGMSGQUICKRENAME), file.Name, newName);

            // process the mask in newName (skip if it is not a mask (contains neither '*' nor '?') - so that renaming to "test^." works)
            if (strchr(newName, '*') != NULL || strchr(newName, '?') != NULL)
            {
                char targetName[2 * MAX_PATH];
                SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, file.Name, newName);
                lstrcpyn(newName, targetName, MAX_PATH);
            }

            if (!Config.AlwaysOverwrite)
            {
                BOOL tgtFileExists = FALSE; // the rename would overwrite an existing file
                BOOL tgtDirExists = FALSE;  // the rename would most likely fail, because a directory with that name already exists

                BOOL caseSensitive = FTPIsCaseSensitive(pathType);

                // find which parser handled the listing
                CServerTypeList* serverTypeList = Config.LockServerTypeList();
                int serverTypeListCount = serverTypeList->Count;
                BOOL err = TRUE; // TRUE = we cannot determine whether the target file will be overwritten
                int i;
                for (i = 0; i < serverTypeListCount; i++)
                {
                    CServerType* serverType = serverTypeList->At(i);
                    const char* s = serverType->TypeName;
                    if (*s == '*')
                        s++;
                    if (SalamanderGeneral->StrICmp(LastServerType, s) == 0)
                    {
                        // we found the serverType successfully used for listing, now parse the listing
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

            if (renameAction == 1) // rename
            {
                ControlConnection->LogMessage(logBuf, -1, TRUE);

                TotalConnectAttemptNum = 1; // start of a user-requested action -> if reconnecting is needed, this is the first reconnect attempt
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
                    return TRUE; // success, the next refresh will focus on 'newName'
                else
                    return FALSE; // returns an incorrect name in 'newName' (the standard dialog opens again)
            }
            else
            {
                if (renameAction == 2)
                    return FALSE; // do not rename and return the existing name in 'newName' (the standard dialog opens again)
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
    if (isDir) // directory
    {
        *type = copy ? fqitUploadCopyExploreDir : fqitUploadMoveExploreDir;
        item = new CFTPQueueItemCopyMoveUploadExplore;
        if (item != NULL)
        {
            ((CFTPQueueItemCopyMoveUploadExplore*)item)->SetItemCopyMoveUploadExplore(targetPath, targetName, UPLOADTGTDIRSTATE_UNKNOWN);
        }
    }
    else // file
    {
        BOOL asciiTransferMode;
        if (transferMode == trmAutodetect)
        {
            char buffer[MAX_PATH];
            if (isVMS) // on VMS we must have the name trimmed to the base (the version number would break mask comparison)
            {
                lstrcpyn(buffer, name, MAX_PATH);
                FTPVMSCutFileVersion(buffer, -1);
                name = buffer;
            }

            const char* ext = strrchr(name, '.');
            //      if (ext == NULL || ext == name) ext = name + strlen(name);   // ".cvspass" is a file extension in Windows ...
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
        // find out whether an operation that could damage the current listing in the panel is running
        // (we perform this test again right before using the listing for the upload-listing-cache, but
        // if such an operation finished before this second test, we would not detect the damage);
        // this does not solve the case when such an operation finishes during drag&drop (while dragging
        // the mouse) from disk to the panel (it is a relatively short time, so we simply ignore it)
        CFTPServerPathType pathType = GetFTPServerPathType(Path);
        if (!PathListingMayBeOutdated && FTPOperationsList.CanMakeChangesOnPath(User, Host, Port, Path, pathType, -1))
            PathListingMayBeOutdated = TRUE;

        // add the *.* or * mask to the target path (we will process operation masks)
        FTPAddOperationMask(pathType, targetPath, 2 * MAX_PATH, sourceFiles == 0);
        return TRUE;
    }

    if (mode == 2 || mode == 3)
    {
        // 'targetPath' contains the raw path entered by the user (the only thing we know about it
        // is that it points to the FTP, otherwise Salamander would not call this method)
        int isFTPS = SalamanderGeneral->StrNICmp(targetPath, AssignedFSNameFTPS, AssignedFSNameLenFTPS) == 0 &&
                     targetPath[AssignedFSNameLenFTPS] == ':';

        // verify whether it will be possible to decrypt a potential password for the default proxy (we may enter SetConnectionParameters() only if it is possible)
        if (!Config.FTPProxyServerList.EnsurePasswordCanBeDecrypted(SalamanderGeneral->GetMsgBoxParent(), Config.DefaultProxySrvUID))
        {
            return FALSE; // fatal error
        }

        char* userPart = strchr(targetPath, ':') + 1; // 'targetPath' must contain fs-name + ':'
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

        if (ControlConnection == NULL) // open the connection (open the path on the FTP server)
        {
            TotalConnectAttemptNum = 1; // opening the connection = first attempt to open the connection

            ControlConnection = new CControlConnectionSocket;
            if (ControlConnection == NULL || !ControlConnection->IsGood())
            {
                if (ControlConnection != NULL) // insufficient system resources for allocating the object
                {
                    DeleteSocket(ControlConnection);
                    ControlConnection = NULL;
                }
                else
                    TRACE_E(LOW_MEMORY);
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared
                return TRUE;                                   // fatal error
            }

            AutodetectSrvType = TRUE; // use automatic detection of the server type
            LastServerType[0] = 0;

            if (host == NULL || *host == 0)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_HOSTNAMEMISSING),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared
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

            // connect to the server
            ControlConnection->SetStartTime();
            if (!ControlConnection->StartControlConnection(SalamanderGeneral->GetMsgBoxParent(),
                                                           User, USER_MAX_SIZE, FALSE, RescuePath,
                                                           FTP_MAX_PATH, &TotalConnectAttemptNum,
                                                           NULL, FALSE, -1, FALSE))
            { // connection failed, release the socket object (signals the "never connected" state)
                DeleteSocket(ControlConnection);
                ControlConnection = NULL;
                Logs.RefreshListOfLogsInLogsDlg();
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared
                return TRUE;                                   // cancel
            }
            lstrcpyn(HomeDir, RescuePath, FTP_MAX_PATH); // store the current path after logging in to the server (home dir)
        }
        else // verify whether the target path is on the server opened in this FS
        {
            if (isFTPS != ControlConnection->GetEncryptControlConnection() ||  // should be FTPS or not, but the state differs
                strcmp(user, User) != 0 ||                                     // different user name (case-sensitive - Unix accounts)
                host == NULL || SalamanderGeneral->StrICmp(host, Host) != 0 || // different host (case-insensitive - Internet conventions - maybe test IP addresses later)
                port != Port)                                                  // different port
            {
                if (invalidPathOrCancel != NULL)
                    *invalidPathOrCancel = FALSE;
                memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared
                return FALSE;                                  // need to find another FS
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
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared

            // determine the path type and optionally skip '/' or '\\' at the beginning of the path (after the host name)
            CFTPServerPathType pathType = ftpsptEmpty;
            if (HomeDir[0] == 0 || HomeDir[0] != '/' && HomeDir[0] != '\\')
            { // we try skipping '/' or '\\' at the beginning of the path only if the server home dir does not start with them (the PWD result after login)
                pathType = GetFTPServerPathType(tgtPath + 1);
                if (pathType == ftpsptOpenVMS || pathType == ftpsptMVS || pathType == ftpsptIBMz_VM ||
                    pathType == ftpsptOS2 && GetFTPServerPathType("") == ftpsptOS2) // OS/2 paths clash with the Unix path "/C:/path", so we distinguish OS/2 paths even just by the SYST reply
                {                                                                   // VMS + MVS + IBM_z/VM + OS/2 do not have '/' or '\\' at the beginning of the path
                    memmove(tgtPath, tgtPath + 1, strlen(tgtPath) + 1);             // remove the '/' or '\\' character from the start of the path
                    if (tgtPath[0] == 0)                                            // generic root -> fill in according to the system type
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
                                        lstrcpyn(tgtPath, "/", FTP_MAX_PATH); // tested server supported the Unix root "/", someone might report otherwise and we will handle it later...
                                    }
                                }
                                else
                                {
                                    if (pathType == ftpsptOS2)
                                    {
                                        if (HomeDir[0] == 0 || !FTPGetOS2RootPath(tgtPath, FTP_MAX_PATH, HomeDir))
                                        {
                                            lstrcpyn(tgtPath, "/", FTP_MAX_PATH); // try at least the Unix root "/", we cannot do anything else, someone might report otherwise and we will handle it later...
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

            // if this is a root (only specific cases, other types of root paths continue further), no more adjustments
            // nor path analysis make sense - use the path as is plus the "*" mask
            if (!isSpecRootPath)
            {
                // if the path ends with a separator, treat it as a path without a mask (e.g. "/pub/dir/" or
                // "PUB$DEVICE:[PUB.VMS.]"); otherwise continue with path analysis
                if (!FTPPathEndsWithDelimiter(pathType, tgtPath))
                {
                    char cutTgtPath[FTP_MAX_PATH];
                    lstrcpyn(cutTgtPath, tgtPath, FTP_MAX_PATH);
                    char cutMask[MAX_PATH];
                    BOOL cutMaybeFileName = FALSE;
                    if (FTPCutDirectory(pathType, cutTgtPath, FTP_MAX_PATH, cutMask, MAX_PATH, &cutMaybeFileName))
                    { // if a part of the path can be trimmed, we will determine whether it is a mask (otherwise it is probably a root path, use the "*" mask)
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
                                { // the trimmed part contains '*' or '?' (wildcards) before '.' (definitely a file mask such as "*.*")
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
                            if (cutTgtPathIBMz_VM[0] == 0 && // we need to test whether 'cutMaskIBMz_VM' contains a mask
                                    (strchr(cutMask, '*') != NULL || strchr(cutMask, '?') != NULL) ||
                                pathType == ftpsptOpenVMS && cutMaybeFileName)
                            { // the trimmed part contains '*' or '?' (wildcards) or it is a VMS file name (must be a mask, the target path is the path to that file)
                                lstrcpyn(tgtPath, cutTgtPath, FTP_MAX_PATH);
                                lstrcpyn(mask, cutMask, MAX_PATH);
                            }
                            else
                            {
                                TotalConnectAttemptNum = 1; // start of a user-requested action -> if reconnecting is needed, this is the first reconnect attempt
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
                                    if (!success) // if 'tgtPath' is a valid path, the mask is "*"; otherwise continue
                                    {
                                        if (ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                                                     SalamanderGeneral->GetMsgBoxParent(),
                                                                                     cutTgtPath, User, USER_MAX_SIZE,
                                                                                     &success, replyBuf, 700, NULL, &TotalConnectAttemptNum,
                                                                                     NULL, FALSE, NULL))
                                        {
                                            if (success) // 'cutTgtPath' is a valid path - the mask is 'cutMask'
                                            {
                                                lstrcpyn(tgtPath, cutTgtPath, FTP_MAX_PATH);
                                                lstrcpyn(mask, cutMask, MAX_PATH);
                                            }
                                            else // otherwise continue
                                            {
                                                if (cutTgtPathIBMz_VM[0] != 0)
                                                {
                                                    if (ControlConnection->SendChangeWorkingPath(notInPanel, panel == PANEL_LEFT,
                                                                                                 SalamanderGeneral->GetMsgBoxParent(),
                                                                                                 cutTgtPathIBMz_VM, User, USER_MAX_SIZE,
                                                                                                 &success, replyBuf, 700, NULL, &TotalConnectAttemptNum,
                                                                                                 NULL, FALSE, NULL))
                                                    {
                                                        if (success) // 'cutTgtPathIBMz_VM' is a valid path - the mask is 'cutMaskIBMz_VM'
                                                        {
                                                            lstrcpyn(tgtPath, cutTgtPathIBMz_VM, FTP_MAX_PATH);
                                                            lstrcpyn(mask, cutMaskIBMz_VM, MAX_PATH);
                                                            done = TRUE;
                                                        }
                                                    }
                                                    else // connection cannot be established (even if the user does not want to reconnect)
                                                    {
                                                        return TRUE; // cancel
                                                    }
                                                }
                                                if (!done) // show the path error to the user
                                                {
                                                    char errBuf[900 + FTP_MAX_PATH];
                                                    _snprintf_s(errBuf, _TRUNCATE, LoadStr(IDS_CHANGEWORKPATHERROR),
                                                                (cutTgtPathIBMz_VM[0] != 0 ? cutTgtPathIBMz_VM : cutTgtPath), replyBuf);
                                                    SalamanderGeneral->ShowMessageBox(errBuf, LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                                                    return FALSE; // invalid path
                                                }
                                            }
                                        }
                                        else // connection cannot be established (even if the user does not want to reconnect)
                                        {
                                            return TRUE; // cancel
                                        }
                                    }
                                }
                                else // connection cannot be established (even if the user does not want to reconnect)
                                {
                                    return TRUE; // cancel
                                }
                            }
                        }
                    }
                }
            }
        }
        else // the target path is the home dir
        {
            memset(newUserPart, 0, FTP_USERPART_SIZE + 1); // wipe the memory where the password appeared
            if (HomeDir[0] == 0)                           // home dir is not defined (some servers require calling CWD first before PWD returns anything)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_HOMEDIRNOTDEFINED),
                                                  LoadStr(IDS_FTPERRORTITLE), MSGBOX_ERROR);
                return FALSE; // invalid path
            }
            lstrcpyn(tgtPath, HomeDir, FTP_MAX_PATH);
        }

        // moving/copying multiple files/directories into one name (they would overwrite each other) is probably nonsense
        if (sourceFiles + sourceDirs > 1 && strchr(mask, '*') == NULL && strchr(mask, '?') == NULL)
        {
            if (SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_COPYMOVE_NONSENSE),
                                                 LoadStr(IDS_FTPPLUGINTITLE),
                                                 MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
            {
                return FALSE; // invalid path
            }
        }

        // the path is analyzed, start the operation:
        // 'tgtPath' is the target path, 'mask' is the operation mask
        BOOL success = FALSE; // pre-initialize cancel/error state of the operation

        char dlgSubjectSrc[MAX_PATH + 100];
        if (sourceFiles + sourceDirs <= 1) // one selected item
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
            next(NULL, -1, NULL, NULL, NULL, NULL, NULL, nextParam, NULL); // reset enumeration
        }
        else // several directories and files
        {
            SalamanderGeneral->GetCommonFSOperSourceDescr(dlgSubjectSrc, MAX_PATH + 100, -1,
                                                          sourceFiles, sourceDirs, NULL, FALSE, TRUE);
        }

        // create the operation object
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
            if (ControlConnection->InitOperation(oper)) // initialize the connection to the server according to the "control connection"
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

                        // build the queue of operation items
                        CFTPQueue* queue = new CFTPQueue;
                        if (queue != NULL)
                        {
                            CQuadWord totalSize(0, 0); // total size (in bytes or blocks)
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
                                // create the target name according to the operation mask (skip if it is not
                                // a mask (contains neither '*' nor '?') - so that renaming to "test^." works)
                                char targetName[2 * MAX_PATH];
                                if (useMask)
                                    SalamanderGeneral->MaskName(targetName, 2 * MAX_PATH, name, mask);
                                else
                                    lstrcpyn(targetName, mask, 2 * MAX_PATH);
                                if (is_AS_400_QSYS_LIB_Path)
                                    FTPAS400AddFileNamePart(targetName);

                                // links: size == 0, the file size must be obtained via GetLinkTgtFileSize() later
                                BOOL cancel = FALSE;
                                if (!isDir && (attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                                    linkNameEnd - linkName + strlen(name) < _countof(linkName))
                                { // this is a link to a file and the link name is not too long (otherwise reported elsewhere)
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
                                    if (!ok || !queue->AddItem(item)) // add the operation to the queue
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
                                // determine whether it makes sense to continue (if there is no error)
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

                        if (ok) // the queue with operation items has been filled
                        {
                            // populate the UploadListingCache with the current panel contents (if the target path is in the panel
                            // and the panel contains an uninterrupted, intact, and up-to-date listing)
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

                            oper->SetQueue(queue); // set the queue of its items for the operation
                            queue = NULL;
                            // FIXME: there is probably no place for an "only add to queue" checkbox: if (Config.UploadAddToQueue) success = TRUE;  // perform the operation later -> for now the operation is successful
                            // else // perform the operation in the active "control connection"
                            // {
                            // open the operation progress window and start the operation
                            if (RunOperation(SalamanderGeneral->GetMsgBoxParent(), operUID, oper, NULL))
                                success = TRUE; // operation succeeded
                            else
                                ok = FALSE;
                            // }
                        }
                        if (!ok)
                            FTPOperationsList.DeleteOperation(operUID, TRUE);
                        oper = NULL; // the operation is already added in the array, do not free it with 'delete' (see below)
                    }
                }
            }
            if (oper != NULL)
                delete oper;
        }
        else
            TRACE_E(LOW_MEMORY);

        if (success && invalidPathOrCancel != NULL)
            *invalidPathOrCancel = FALSE; // report "success" (otherwise "error/cancel")
        return TRUE;
    }
    return FALSE; // unknown 'mode'
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
            { // the user might have imported the certificate or deleted it from the MS store, verify the state and show it in the panel
                bool verified = cert->CheckCertificate(errBuf, 300);
                cert->SetVerified(verified);
                SalamanderGeneral->ShowSecurityIcon(panel, TRUE, verified,
                                                    LoadStr(verified ? IDS_SSL_SECURITY_OK : IDS_SSL_SECURITY_UNVERIFIED));
            }
            cert->Release();
        }
    }
}
