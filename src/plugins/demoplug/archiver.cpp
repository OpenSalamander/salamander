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

// shared interface for archiver plugin data
CArcPluginDataInterface ArcPluginDataInterface;

// ****************************************************************************
// ARCHIVER SECTION
// ****************************************************************************

//
// ****************************************************************************
// CArcPluginDataInterface
//

// callback invoked by Salamander to obtain text
// see spl_com.h / FColumnGetText for details
void WINAPI GetSzText()
{
    if (*TransferIsDir && !(*TransferFileData)->SizeValid)
    {
        CopyMemory(TransferBuffer, "Dir", 3);
        *TransferLen = 3;
    }
    else
        *TransferLen = sprintf(TransferBuffer, "%I64d", (*TransferFileData)->Size.Value);
}

void WINAPI
CArcPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                   const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    // adjust columns only in detailed mode
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        // try to find the standard Size column and insert after it; if it is not found,
        // append the column at the end
        int sizeIndex = view->GetColumnsCount();
        int i;
        for (i = 0; i < sizeIndex; i++)
            if (view->GetColumn(i)->ID == COLUMN_ID_SIZE)
            {
                sizeIndex = i + 1;
                break;
            }

        CColumn column;
        lstrcpy(column.Name, "Size2");
        lstrcpy(column.Description, "Size v jinem provedeni");
        column.GetText = GetSzText;
        column.SupportSorting = 1;
        column.LeftAlignment = 0;
        column.ID = COLUMN_ID_CUSTOM;
        column.Width = leftPanel ? LOWORD(Size2Width) : HIWORD(Size2Width);
        column.FixedWidth = leftPanel ? LOWORD(Size2FixedWidth) : HIWORD(Size2FixedWidth);
        view->InsertColumn(sizeIndex, &column);
    }
}

void WINAPI
CArcPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (leftPanel)
        Size2FixedWidth = MAKELONG(newFixedWidth, HIWORD(Size2FixedWidth));
    else
        Size2FixedWidth = MAKELONG(LOWORD(Size2FixedWidth), newFixedWidth);
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void WINAPI
CArcPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (leftPanel)
        Size2Width = MAKELONG(newWidth, HIWORD(Size2Width));
    else
        Size2Width = MAKELONG(LOWORD(Size2Width), newWidth);
}

BOOL WINAPI
CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                         CSalamanderDirectoryAbstract* dir,
                                         CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, , ,)", fileName);
#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::ListArchive", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    // define which data fields in 'file' are valid
    dir->SetValidData(VALID_DATA_EXTENSION |
                      VALID_DATA_DOSNAME |
                      VALID_DATA_SIZE |
                      VALID_DATA_TYPE |
                      VALID_DATA_DATE |
                      VALID_DATA_TIME |
                      VALID_DATA_ATTRIBUTES |
                      VALID_DATA_HIDDEN |
                      VALID_DATA_ISLINK |
                      VALID_DATA_ISOFFLINE |
                      VALID_DATA_ICONOVERLAY);

    pluginData = &ArcPluginDataInterface;

    CFileData file;

    file.Name = SalamanderGeneral->DupStr("test.dop");
    if (file.Name == NULL)
    {
        dir->Clear(pluginData);
        return FALSE;
    }
    file.NameLen = strlen(file.Name);
    char* s = strrchr(file.Name, '.');
    if (s != NULL)
        file.Ext = s + 1; // ".cvspass" is a Windows extension...
    else
        file.Ext = file.Name + file.NameLen;
    file.Size = CQuadWord(666, 0);
    file.Attr = FILE_ATTRIBUTE_ARCHIVE;
    file.Hidden = 0;
    file.PluginData = 666; // redundant, just for show

    SYSTEMTIME st;
    GetSystemTime(&st);
    SystemTimeToFileTime(&st, &file.LastWrite);
    /*
  SYSTEMTIME t;
  t.wYear = yr;
  if (t.wYear < 100)
  {
    if (t.wYear < 80) t.wYear += 2000;
    else t.wYear += 1900;
  }
  t.wMonth = mo;
  t.wDayOfWeek = 0;     // ignored
  t.wDay = dy;
  t.wHour = hh;
  t.wMinute = mm;
  t.wSecond = 0;
  t.wMilliseconds = 0;
  FILETIME lt;
  SystemTimeToFileTime(&t, &lt);                   // local time
  LocalFileTimeToFileTime(&lt, &dir.LastWrite);    // system time (universal time)
*/
    file.DosName = NULL;
    file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
    file.IsOffline = 0;
    file.IconOverlayIndex = 1; // icon-overlay: slow file

    // automatically adds two directories "test" and "path" (because they do not yet exist)
    // so that it can add 'file'
    if (!dir->AddFile("test\\path", file, pluginData))
    {
        SalamanderGeneral->Free(file.Name);
        dir->Clear(pluginData);
        return FALSE;
    }

    // add two more files
    file.Name = SalamanderGeneral->DupStr("test2.txt");
    if (file.Name == NULL)
    {
        dir->Clear(pluginData);
        return FALSE;
    }
    file.NameLen = strlen(file.Name);
    s = strrchr(file.Name, '.');
    if (s != NULL)
        file.Ext = s + 1; // ".cvspass" is a Windows extension...
    else
        file.Ext = file.Name + file.NameLen;
    file.Size = CQuadWord(555, 0);
    file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
    file.IconOverlayIndex = 0; // icon-overlay: shared
    if (!dir->AddFile("test\\path", file, pluginData))
    {
        SalamanderGeneral->Free(file.Name);
        dir->Clear(pluginData);
        return FALSE;
    }
    file.Name = SalamanderGeneral->DupStr("test3.txt");
    if (file.Name == NULL)
    {
        dir->Clear(pluginData);
        return FALSE;
    }
    file.NameLen = strlen(file.Name);
    s = strrchr(file.Name, '.');
    if (s != NULL)
        file.Ext = s + 1; // ".cvspass" is a Windows extension...
    else
        file.Ext = file.Name + file.NameLen;
    file.Size = CQuadWord(444, 0);
    file.Attr |= FILE_ATTRIBUTE_ENCRYPTED;
    file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
    file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED; // no icon overlay
    if (!dir->AddFile("test\\path", file, pluginData))
    {
        SalamanderGeneral->Free(file.Name);
        dir->Clear(pluginData);
        return FALSE;
    }

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);
    file.Name = SalamanderGeneral->DupStr("test");
    if (file.Name == NULL)
    {
        dir->Clear(pluginData);
        return FALSE;
    }
    file.NameLen = strlen(file.Name);
    if (!sortByExtDirsAsFiles)
        file.Ext = file.Name + file.NameLen; // directories have no extension
    else
    {
        s = strrchr(file.Name, '.');
        if (s != NULL)
            file.Ext = s + 1; // ".cvspass" is a Windows extension...
        else
            file.Ext = file.Name + file.NameLen;
    }
    file.Size = CQuadWord(0, 0);
    file.Attr = FILE_ATTRIBUTE_DIRECTORY;
    file.Hidden = 0;
    file.PluginData = 666; // redundant, just for show
    file.IsLink = 0;
    file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED; // no icon overlay

    // change the data of the "test" directory (created automatically, see the previous AddFile) to 'file'
    if (!dir->AddDir("", file, pluginData))
    {
        SalamanderGeneral->Free(file.Name);
        dir->Clear(pluginData);
        return FALSE;
    }

    return TRUE;
}

BOOL WINAPI
CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander,
                                           const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                           const char* targetDir, const char* archiveRoot,
                                           SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName,
                        targetDir, archiveRoot);
#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::UnpackArchive", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    // internal packers will probably use salCalls->SafeCreateFile for direct
    //   extraction
    // for the rest there is an approach that unpacks to a temporary directory and then
    //   moves the files to the correct location on disk (handles overwriting files, etc.):

    BOOL ret = FALSE;
    char tmpExtractDir[MAX_PATH];
    DWORD err;
    if (!SalamanderGeneral->SalGetTempFileName(targetDir, "Sal", tmpExtractDir, FALSE, &err))
    {
        char buf[100];
        sprintf(buf, "SalGetTempFileName() error: %u", err);
        TRACE_E(buf);
    }
    else
    {
        BOOL isDir;
        CQuadWord size;
        CQuadWord totalSize(0, 0);
        const char* name;
        const CFileData* fileData;
        while ((name = next(NULL, 0, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            totalSize += size;

            // building the list of files that should be extracted
        }

        /*  // repeat the enumeration (just as a demonstration of "resetting" the enumeration)
    totalSize = 0;
    next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
    while ((name = next(NULL, 0, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
    {
      totalSize += size;

      // building the list of files that should be extracted
    }
*/

        BOOL delTempDir = TRUE;
        if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                             tmpExtractDir, totalSize, "Unpacking DemoPlug Archive"))
        {
            /*
      // demonstration of a progress dialog with a single progress bar
      salamander->OpenProgressDialog("Unpacking DemoPlug Archive", FALSE, NULL, FALSE);
      salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(-1, -1));
      // performing the extraction - the following methods are called sequentially:
      salamander->ProgressDialogAddText("preparing data...", FALSE); // delayedPaint==FALSE because we do not want to wait for the timer and we are not going to call ProgressAddSize
      Sleep(1000);  // activity simulation
      ret = TRUE;
      int c = 30;
      while (c--)
      {
        salamander->ProgressDialogAddText("test text", TRUE);  // delayedPaint==TRUE so that we do not slow the UI down
        Sleep(50);  // activity simulation
        if (!salamander->ProgressAddSize(1, TRUE))  // delayedPaint==TRUE so that we do not slow the UI down
        {
          salamander->ProgressDialogAddText("canceling operation, please wait...", FALSE);
          salamander->ProgressEnableCancel(FALSE);
          Sleep(1000);  // cleanup simulation
          ret = FALSE;
          break;   // cancel the action
        }
      }
      Sleep(500);  // activity simulation
      salamander->CloseProgressDialog();
*/

            // demonstration of a progress dialog with two progress bars
            salamander->OpenProgressDialog("Unpacking DemoPlug Archive", TRUE, NULL, FALSE);
            salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(90, 0));
            // performing the extraction - the following methods are called repeatedly:
            salamander->ProgressDialogAddText("preparing data...", FALSE); // delayedPaint==FALSE because we do not want to wait for the timer and we are not going to call ProgressAddSize
            Sleep(1000);                                                   // activity simulation
            ret = TRUE;
            int c = 90;
            while (c--)
            {
                salamander->ProgressDialogAddText("test text", TRUE); // delayedPaint==TRUE so that we do not slow the UI down
                Sleep(50);                                            // activity simulation
                if ((c + 1) % 30 == 0)                                // simulating "another file"
                {
                    // salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(-1, -1)); // configuring the "size" of the next file (commented out here so it is not repeatedly set to 30)
                    salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE); // delayedPaint==TRUE so that we do not slow the UI down
                }
                if (!salamander->ProgressAddSize(1, TRUE)) // delayedPaint==TRUE so that we do not slow the UI down
                {
                    salamander->ProgressDialogAddText("canceling operation, please wait...", FALSE);
                    salamander->ProgressEnableCancel(FALSE);
                    Sleep(1000); // cleanup simulation
                    ret = FALSE;
                    break; // cancel the action
                }
            }
            Sleep(500); // activity simulation
            salamander->CloseProgressDialog();

            // ret is TRUE on success; otherwise it remains FALSE
            if (ret)
            {
                // the files are unpacked in the temporary directory and must be placed
                if (!salamander->MoveFiles(tmpExtractDir, targetDir, tmpExtractDir, fileName))
                    delTempDir = FALSE;
            }
        }

        if (delTempDir)
            SalamanderGeneral->RemoveTemporaryDir(tmpExtractDir);
    }

    return ret;
}

BOOL WINAPI
CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                           const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                           const char* nameInArchive, const CFileData* fileData,
                                           const char* targetDir, const char* newFileName,
                                           BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);
#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::UnpackOneFile", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    if (newFileName != NULL)
    {
        *renamingNotSupported = TRUE;
        return FALSE;
    }

    // unpacking without a progress dialog

    // instead of unpacking we only create a test file
    char name[MAX_PATH];
    strcpy(name, targetDir);
    const char* lastComp = strrchr(nameInArchive, '\\');
    if (lastComp != NULL)
        lastComp++;
    else
        lastComp = nameInArchive;
    if (SalamanderGeneral->SalPathAppend(name, lastComp, MAX_PATH))
    {
        HANDLE file = HANDLES_Q(CreateFile(name, GENERIC_WRITE,
                                           FILE_SHARE_READ, NULL,
                                           CREATE_ALWAYS,
                                           FILE_FLAG_SEQUENTIAL_SCAN,
                                           NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
            ULONG written;
            WriteFile(file, "New File\r\n", 10, &written, NULL);
            HANDLES(CloseHandle(file));
            return TRUE; // the "unpacking" succeeded
        }
    }

    return FALSE;
}

BOOL WINAPI
CPluginInterfaceForArchiver::PackToArchive(CSalamanderForOperationsAbstract* salamander,
                                           const char* fileName, const char* archiveRoot,
                                           BOOL move, const char* sourcePath,
                                           SalEnumSelection2 next, void* nextParam)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::PackToArchive(, %s, %s, %d, %s, ,)", fileName,
                        archiveRoot, move, sourcePath);

#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::PackToArchive", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    BOOL isDir;
    const char* name;
    const char* dosName; // dummy
    CQuadWord size;
    DWORD attr;
    FILETIME lastWrite;
    CQuadWord totalSize(0, 0);
    int errorOccured;

    // open progress dialog
    salamander->OpenProgressDialog("Packing DemoPlug Archive", FALSE, NULL, FALSE);
    salamander->ProgressDialogAddText("reading directory tree...", FALSE);

    while ((name = next(SalamanderGeneral->GetMsgBoxParent(), 3, &dosName, &isDir, &size,
                        &attr, &lastWrite, nextParam, &errorOccured)) != NULL)
    {
        if (errorOccured == SALENUM_ERROR) // SALENUM_CANCEL cannot appear here
            TRACE_I("Not all files and directories from disk will be packed.");

        totalSize += size;

        // building the list of files that should be packed
    }
    if (errorOccured != SALENUM_SUCCESS)
    {
        TRACE_I("Not all files and directories from disk will be packed.");
        // check whether an error occurred and the user requested to cancel the operation (Cancel button)
        if (errorOccured == SALENUM_CANCEL)
        {
            salamander->CloseProgressDialog();
            return FALSE;
        }
    }

    char archivePath[MAX_PATH];
    char* s = (char*)strrchr(fileName, '\\');
    if (s != NULL)
    {
        memcpy(archivePath, fileName, s - fileName);
        archivePath[s - fileName] = 0;
    }
    else
        archivePath[0] = 0;

    BOOL ret = FALSE;
    if (archivePath[0] == 0 ||
        SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                         archivePath, totalSize, "Unpacking DemoPlug Archive"))
    {
        salamander->ProgressSetTotalSize(CQuadWord(100, 0) /*totalSize*/, CQuadWord(-1, -1));

        // perform the packing
        salamander->ProgressDialogAddText("test text 1", FALSE);
        Sleep(500); // activity simulation
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // cancel the action
        else
        {
            salamander->ProgressDialogAddText("test text 2", FALSE);
            Sleep(500); // activity simulation
            if (!salamander->ProgressAddSize(50, FALSE))
                ret = FALSE; // cancel the action
            else
                ret = TRUE;
        }
        // ret is TRUE on success; otherwise it remains FALSE
    }
    salamander->CloseProgressDialog();

    // NOTE: do not forget to set the Archive attribute on the file (the archive file changed -> it must be marked for backup)

    return ret;
}

BOOL WINAPI
CPluginInterfaceForArchiver::DeleteFromArchive(CSalamanderForOperationsAbstract* salamander,
                                               const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                               const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DeleteFromArchive(, %s, , %s, ,)",
                        fileName, archiveRoot);
#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::DeleteFromArchive",
                                      LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    salamander->OpenProgressDialog("Deleting DemoPlug Archive Entries", FALSE, NULL, FALSE);
    salamander->ProgressDialogAddText("reading directory tree...", FALSE);

    BOOL ret = FALSE;
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    const char* name;
    const CFileData* fileData;
    while ((name = next(NULL, 0, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
    {
        totalSize += size;

        // building the list of files that should be deleted
    }

    salamander->ProgressSetTotalSize(CQuadWord(100, 0) /*totalSize*/, CQuadWord(-1, -1));

    // perform the deletion
    salamander->ProgressDialogAddText("test text 1", FALSE);
    Sleep(500); // activity simulation
    if (!salamander->ProgressAddSize(50, FALSE))
        ret = FALSE; // cancel the action
    else
    {
        salamander->ProgressDialogAddText("test text 2", FALSE);
        Sleep(500); // activity simulation
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // cancel the action
        else
            ret = TRUE;
    }
    // ret is TRUE on success; otherwise it remains FALSE

    salamander->CloseProgressDialog();

    // NOTE: do not forget to set the Archive attribute on the file (the archive file changed -> it must be marked for backup)

    return ret;
}

/*
void EnumAllItems(CSalamanderDirectoryAbstract const *dir, char *path, int pathBufSize)
{
  int count = dir->GetFilesCount();
  int i;
  for (i = 0; i < count; i++)
  {
    CFileData const *file = dir->GetFile(i);
    TRACE_I("EnumAllItems(): file: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
  }
  count = dir->GetDirsCount();
  int pathLen = strlen(path);
  for (i = 0; i < count; i++)
  {
    CFileData const *file = dir->GetDir(i);
    TRACE_I("EnumAllItems(): directory: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
    SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize);
    CSalamanderDirectoryAbstract const *subDir = dir->GetSalDir(i);
    EnumAllItems(subDir, path, pathBufSize);
    path[pathLen] = 0;
  }
}
*/

BOOL WINAPI
CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, const char* mask,
                                                const char* targetDir, BOOL delArchiveWhenDone,
                                                CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)", fileName,
                        mask, targetDir, delArchiveWhenDone);
#ifndef DEMOPLUG_QUIET
    SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::UnpackWholeArchive",
                                      LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    /*
  CSalamanderDirectoryAbstract *dir = SalamanderGeneral->AllocSalamanderDirectory(FALSE);
  if (dir != NULL)
  {
    CPluginDataInterfaceAbstract *pluginData = NULL;
    if (ListArchive(salamander, fileName, dir, pluginData))
    {
      char path[MAX_PATH];
      path[0] = 0;
      EnumAllItems(dir, path, MAX_PATH);
      dir->Clear(pluginData);
      if (pluginData != NULL) PluginInterface.ReleasePluginDataInterface(pluginData);
    }
    SalamanderGeneral->FreeSalamanderDirectory(dir);
  }
*/

    BOOL ret = FALSE;
    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2); // FIXME: once the plugin learns multi-volume archives we must add all archive volumes here (to delete the entire archive)
    salamander->OpenProgressDialog("Unpacking DemoPlug Archive", FALSE, NULL, FALSE);
    salamander->ProgressSetTotalSize(CQuadWord(100, 0), CQuadWord(-1, -1));

    // perform the unpacking
    salamander->ProgressDialogAddText("test text 1", FALSE);
    Sleep(500); // activity simulation
    if (!salamander->ProgressAddSize(50, FALSE))
        ret = FALSE; // cancel the action
    else
    {
        salamander->ProgressDialogAddText("test text 2", FALSE);
        Sleep(500); // activity simulation
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // cancel the action
        else
            ret = TRUE;
    }
    // ret is TRUE on success; otherwise it remains FALSE

    salamander->CloseProgressDialog();

    return ret;
}

BOOL WINAPI
CPluginInterfaceForArchiver::CanCloseArchive(CSalamanderForOperationsAbstract* salamander,
                                             const char* fileName, BOOL force, int panel)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::CanCloseArchive(, %s, %d, %d)",
                        fileName, force, panel);
#ifdef DEMOPLUG_QUIET
    return TRUE;
#else  // DEMOPLUG_QUIET

    if (SalamanderGeneral->IsCriticalShutdown())
        return TRUE; // during a critical shutdown we do not ask any questions

    return force && SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::CanCloseArchive (can close).\n"
                                                      "Return is forced to TRUE.",
                                                      LoadStr(IDS_PLUGINNAME),
                                                      MSGBOX_INFO) == IDOK ||
           SalamanderGeneral->ShowMessageBox("CPluginInterfaceForArchiver::CanCloseArchive (can close).\n"
                                             "What should it return?",
                                             LoadStr(IDS_PLUGINNAME),
                                             MSGBOX_QUESTION) == IDYES;
#endif // DEMOPLUG_QUIET
}

void GetMyDocumentsPath(char* path)
{
    path[0] = 0;
    ITEMIDLIST* pidl = NULL;
    if (SHGetSpecialFolderLocation(NULL, CSIDL_PERSONAL, &pidl) == NOERROR)
    {
        if (!SHGetPathFromIDList(pidl, path))
            path[0] = 0;
        IMalloc* alloc;
        if (SUCCEEDED(CoGetMalloc(1, &alloc)))
        {
            alloc->Free(pidl);
            alloc->Release();
        }
    }
}

BOOL WINAPI
CPluginInterfaceForArchiver::GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::GetCacheInfo()");
    GetMyDocumentsPath(tempPath);
    if (tempPath[0] == 0 ||
        !SalamanderGeneral->SalPathAppend(tempPath, "DemoPlug Temporary Copies", MAX_PATH))
    {
        tempPath[0] = 0; // error -> fall back to the system TEMP
    }
    *ownDelete = TRUE;
    *cacheCopies = FALSE;
    return TRUE;
}

void ClearTEMPIfNeeded(HWND parent)
{
    char tmpDir[2 * MAX_PATH];
    GetMyDocumentsPath(tmpDir);
    if (tmpDir[0] != 0 &&
        SalamanderGeneral->SalPathAppend(tmpDir, "DemoPlug Temporary Copies", 2 * MAX_PATH))
    {
        SalamanderGeneral->SalPathAddBackslash(tmpDir, 2 * MAX_PATH);
        char* tmpDirEnd = tmpDir + strlen(tmpDir);
        // add the mask (if it does not fit, there is no point in searching)
        if (SalamanderGeneral->SalPathAppend(tmpDir, "SAL*.tmp", 2 * MAX_PATH))
        {
            TIndirectArray<char> tmpDirs(10, 50);

            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(tmpDir, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                { // process all found directories (ignore search errors)
                    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strlen(data.cFileName) > 3)
                    {
                        char* s = data.cFileName + 3;
                        while (*s != 0 && *s != '.' &&
                               (*s >= '0' && *s <= '9' || *s >= 'a' && *s <= 'f' || *s >= 'A' && *s <= 'F'))
                            s++;
                        if (SalamanderGeneral->StrICmp(s, ".tmp") == 0) // matches "SAL" + hex number + ".tmp" = almost certainly our directory
                        {
                            char* tmp = SalamanderGeneral->DupStr(data.cFileName);
                            if (tmp != NULL)
                            {
                                tmpDirs.Add(tmp);
                                if (!tmpDirs.IsGood())
                                {
                                    SalamanderGeneral->Free(tmp);
                                    tmpDirs.ResetState();
                                }
                            }
                        }
                    }
                } while (FindNextFile(find, &data));
                HANDLES(FindClose(find));
            }

            if (tmpDirs.IsGood() && tmpDirs.Count > 0)
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = parent;
                params.Flags = MSGBOXEX_ABORTRETRYIGNORE | MSGBOXEX_ICONQUESTION | MSGBOXEX_DEFBUTTON3;
                params.Caption = LoadStr(IDS_PLUGINNAME);
                char buf[300];
                char buf2[300];
                CQuadWord qwCount(tmpDirs.Count, 0);
                SalamanderGeneral->ExpandPluralString(buf2, 300,
                                                      "{!}Do you want to delete %d temporary director{y|1|ies} used "
                                                      "by previous instances of DemoPlug plugin?",
                                                      1, &qwCount);
                _snprintf_s(buf, _TRUNCATE, buf2, tmpDirs.Count);
                params.Text = buf;
                char aliasBtnNames[300];
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                        DIALOG_ABORT, "&Yes",
                        DIALOG_RETRY, "&No",
                        DIALOG_IGNORE, "&Focus");
                params.AliasBtnNames = aliasBtnNames;
                int ret = SalamanderGeneral->SalMessageBoxEx(&params);
                if (ret == DIALOG_ABORT) // yes
                {
                    for (int i = 0; i < tmpDirs.Count; i++)
                    {
                        lstrcpyn(tmpDirEnd, tmpDirs[i], 2 * MAX_PATH - (int)(tmpDirEnd - tmpDir));
                        SalamanderGeneral->RemoveTemporaryDir(tmpDir);
                    }
                }
                if (ret == IDIGNORE) // focus
                {
                    lstrcpyn(tmpDirEnd, tmpDirs[0], 2 * MAX_PATH - (int)(tmpDirEnd - tmpDir));
                    SalamanderGeneral->FocusNameInPanel(PANEL_SOURCE, tmpDir, "");
                }
            }
        }
    }
    else
        TRACE_E("DemoPlug: Unable to clear TEMP directory: TEMP directory not defined!");
}

void WINAPI
CPluginInterfaceForArchiver::DeleteTmpCopy(const char* fileName, BOOL firstFile)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DeleteTmpCopy(%s, %d)", fileName, firstFile);

    /*
  // message box test (message boxes should be used only in an extreme situation) - it makes a mess
  // if another plugin has its own modal dialog open (cosmetic issue: the message box for this dialog is not modal
  // and after closing it activates the dialog's parent)
  char buf[500];
  sprintf(buf, "File \"%s\" will be deleted.", fileName);
  SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                   buf, LoadStr(IDS_PLUGINNAME),
                                   MB_OK | MB_ICONINFORMATION);
*/

    // if this is a critical shutdown it is not a good time for slow file deletion (our process will be killed soon),
    // at the first subsequent plugin start in the first Salamander instance this will be deleted "calmly",
    // we probably cannot come up with anything better
    if (SalamanderGeneral->IsCriticalShutdown())
        return;

    // demonstration of using the wait window
    static DWORD ti = 0; // time when deletion of the first file in the batch started (when deleting multiple files at once)
    DWORD showTime = 1000;
    if (firstFile)
        ti = GetTickCount(); // ensure that the wait window shows up after one second across the entire deletion batch
    else
    {
        DWORD work = GetTickCount() - ti; // how long the deletion has been running (since the first file in the batch)
        if (work < 1000)
            showTime -= work;
        else
            showTime = 0;
    }
    SalamanderGeneral->CreateSafeWaitWindow("Deleting temporary file unpacked from archive, please wait...",
                                            LoadStr(IDS_PLUGINNAME), showTime, FALSE /* TRUE if we can cancel the operation */,
                                            SalamanderGeneral->GetMsgBoxParent());
    // activity simulation (the window becomes visible after one second)
    Sleep(2000);

    // regular file deletion
    SalamanderGeneral->ClearReadOnlyAttr(fileName);

    if (DeleteFile(fileName))
        TRACE_I("Temporary copy from disk-cache (" << fileName << ") was deleted.");
    else
        TRACE_I("Unable to delete temporary copy from disk-cache (" << fileName << ").");

    // close the wait window; the action finished
    SalamanderGeneral->DestroySafeWaitWindow();
}

BOOL WINAPI
CPluginInterfaceForArchiver::PrematureDeleteTmpCopy(HWND parent, int copiesCount)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::PrematureDeleteTmpCopy(, %d)", copiesCount);

    // if this is a critical shutdown it is not a good time for slow file deletion (our process will be killed soon),
    // at the first subsequent plugin start in the first Salamander instance this will be deleted "calmly",
    // we probably cannot come up with anything better
    if (SalamanderGeneral->IsCriticalShutdown())
        return FALSE; // during a critical shutdown we do not ask any questions

    char buf[500];
    sprintf(buf, "%d temporary file(s) extracted from archive are still in use.\n"
                 "Do you want to delete them anyway?",
            copiesCount);
    return SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_PLUGINNAME),
                                            MB_YESNO | MB_ICONQUESTION) == IDYES;
}
