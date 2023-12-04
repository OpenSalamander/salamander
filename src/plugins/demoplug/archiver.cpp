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

// spolecny interface pro pluginova data archivatoru
CArcPluginDataInterface ArcPluginDataInterface;

// ****************************************************************************
// SEKCE ARCHIVERU
// ****************************************************************************

//
// ****************************************************************************
// CArcPluginDataInterface
//

// callback volany ze Salamandera pro ziskani textu
// popis viz. spl_com.h / FColumnGetText
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

    // sloupce upravujeme jen v detailed rezimu
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        // zkusime najit std. Size a zaradit se za nej; pokud ho nenajdeme,
        // zaradime se na konec
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

    // nastavime jaka data ve 'file' jsou platna
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
        file.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
    else
        file.Ext = file.Name + file.NameLen;
    file.Size = CQuadWord(666, 0);
    file.Attr = FILE_ATTRIBUTE_ARCHIVE;
    file.Hidden = 0;
    file.PluginData = 666; // zbytecne, jen tak pro formu

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

    // prida automaticky dva adresare "test" a "path" (protoze jeste neexistuji),
    // aby mohl pridat 'file'
    if (!dir->AddFile("test\\path", file, pluginData))
    {
        SalamanderGeneral->Free(file.Name);
        dir->Clear(pluginData);
        return FALSE;
    }

    // pridame jeste dva soubory
    file.Name = SalamanderGeneral->DupStr("test2.txt");
    if (file.Name == NULL)
    {
        dir->Clear(pluginData);
        return FALSE;
    }
    file.NameLen = strlen(file.Name);
    s = strrchr(file.Name, '.');
    if (s != NULL)
        file.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
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
        file.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
    else
        file.Ext = file.Name + file.NameLen;
    file.Size = CQuadWord(444, 0);
    file.Attr |= FILE_ATTRIBUTE_ENCRYPTED;
    file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);
    file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED; // none icon-overlay
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
        file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
    else
    {
        s = strrchr(file.Name, '.');
        if (s != NULL)
            file.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
        else
            file.Ext = file.Name + file.NameLen;
    }
    file.Size = CQuadWord(0, 0);
    file.Attr = FILE_ATTRIBUTE_DIRECTORY;
    file.Hidden = 0;
    file.PluginData = 666; // zbytecne, jen tak pro formu
    file.IsLink = 0;
    file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED; // none icon-overlay

    // zmeni data adresare "test" (vytvoren automaticky, viz predchozi AddFile) na 'file'
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

    // interni pakovace nejspis pouziji metodu salCalls->SafeCreateFile pro prime
    //   vypakovavani
    // pro ostatni je zde zpusob s vypakovanim do docasneho adresare a nasledny
    //   presun na spravnou pozici na disku (resi prepisy souboru, atd.):

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

            // vytvareni listu souboru, ktery se maji vypakovat
        }

        /*  // zopakujeme enumeraci (jen tak, ukazka "resetu" enumerace)
    totalSize = 0;
    next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
    while ((name = next(NULL, 0, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
    {
      totalSize += size;

      // vytvareni listu souboru, ktery se maji vypakovat
    }
*/

        BOOL delTempDir = TRUE;
        if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                             tmpExtractDir, totalSize, "Unpacking DemoPlug Archive"))
        {
            /*
      // ukazka progress dialogu s jednim progress metrem
      salamander->OpenProgressDialog("Unpacking DemoPlug Archive", FALSE, NULL, FALSE);
      salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(-1, -1));
      // provedeni rozpakovani - prubezne se volaji nasl. metody:
      salamander->ProgressDialogAddText("preparing data...", FALSE); // delayedPaint==FALSE, protoze nechceme cekat na timer a navic nebudeme volat ProgressAddSize
      Sleep(1000);  // simulace cinosti
      ret = TRUE;
      int c = 30;
      while (c--)
      {
        salamander->ProgressDialogAddText("test text", TRUE);  // delayedPaint==TRUE, abychom nebrzdili
        Sleep(50);  // simulace cinnosti
        if (!salamander->ProgressAddSize(1, TRUE))  // delayedPaint==TRUE, abychom nebrzdili
        {
          salamander->ProgressDialogAddText("canceling operation, please wait...", FALSE);
          salamander->ProgressEnableCancel(FALSE);
          Sleep(1000);  // simulace uklizeci cinosti
          ret = FALSE;
          break;   // preruseni akce
        }
      }
      Sleep(500);  // simulace cinosti
      salamander->CloseProgressDialog();
*/

            // ukazka progress dialogu s dvema progress metry
            salamander->OpenProgressDialog("Unpacking DemoPlug Archive", TRUE, NULL, FALSE);
            salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(90, 0));
            // provedeni rozpakovani - prubezne se volaji nasl. metody:
            salamander->ProgressDialogAddText("preparing data...", FALSE); // delayedPaint==FALSE, protoze nechceme cekat na timer a navic nebudeme volat ProgressAddSize
            Sleep(1000);                                                   // simulace cinosti
            ret = TRUE;
            int c = 90;
            while (c--)
            {
                salamander->ProgressDialogAddText("test text", TRUE); // delayedPaint==TRUE, abychom nebrzdili
                Sleep(50);                                            // simulace cinnosti
                if ((c + 1) % 30 == 0)                                // simulace "dalsi soubor"
                {
                    // salamander->ProgressSetTotalSize(CQuadWord(30, 0), CQuadWord(-1, -1)); // nastaveni "velikosti" dalsiho souboru (zakomentovano, protoze tady je zbytecne, at se nenastavuje porad dokola 30)
                    salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE); // delayedPaint==TRUE, abychom nebrzdili
                }
                if (!salamander->ProgressAddSize(1, TRUE)) // delayedPaint==TRUE, abychom nebrzdili
                {
                    salamander->ProgressDialogAddText("canceling operation, please wait...", FALSE);
                    salamander->ProgressEnableCancel(FALSE);
                    Sleep(1000); // simulace uklizeci cinosti
                    ret = FALSE;
                    break; // preruseni akce
                }
            }
            Sleep(500); // simulace cinosti
            salamander->CloseProgressDialog();

            // ret je pri uspechu TRUE, jinak zustava FALSE
            if (ret)
            {
                // soubory jsou vybalene v tmp-adresari, je treba je umistit
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

    // rozpakovani bez progress dialogu

    // misto rozpakovani jen vytvorime pokusny soubor
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
            return TRUE; // "rozpakovani" se povedlo
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
        if (errorOccured == SALENUM_ERROR) // sem SALENUM_CANCEL prijit nemuze
            TRACE_I("Not all files and directories from disk will be packed.");

        totalSize += size;

        // vytvareni listu souboru, ktery se maji zapakovat
    }
    if (errorOccured != SALENUM_SUCCESS)
    {
        TRACE_I("Not all files and directories from disk will be packed.");
        // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci (tlacitko Cancel)
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

        // provedeni pakovani
        salamander->ProgressDialogAddText("test text 1", FALSE);
        Sleep(500); // simulace cinosti
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // break;   // preruseni akce
        else
        {
            salamander->ProgressDialogAddText("test text 2", FALSE);
            Sleep(500); // simulace cinosti
            if (!salamander->ProgressAddSize(50, FALSE))
                ret = FALSE; // break;   // preruseni akce
            else
                ret = TRUE;
        }
        // ret je pri uspechu TRUE, jinak zustava FALSE
    }
    salamander->CloseProgressDialog();

    // POZOR: nezapomenout nastavit Archive atribut souboru (zmena souboru archivu -> je treba ho oznacit pro backup)

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

        // vytvareni listu souboru, ktery se maji smazat
    }

    salamander->ProgressSetTotalSize(CQuadWord(100, 0) /*totalSize*/, CQuadWord(-1, -1));

    // provedeni mazani
    salamander->ProgressDialogAddText("test text 1", FALSE);
    Sleep(500); // simulace cinosti
    if (!salamander->ProgressAddSize(50, FALSE))
        ret = FALSE; // break;   // preruseni akce
    else
    {
        salamander->ProgressDialogAddText("test text 2", FALSE);
        Sleep(500); // simulace cinosti
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // break;   // preruseni akce
        else
            ret = TRUE;
    }
    // ret je pri uspechu TRUE, jinak zustava FALSE

    salamander->CloseProgressDialog();

    // POZOR: nezapomenout nastavit Archive atribut souboru (zmena souboru archivu -> je treba ho oznacit pro backup)

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
        archiveVolumes->Add(fileName, -2); // FIXME: az plugin doucime multi-volume archivy, musime sem napridavat vsechny svazky archivu (aby se smazal kompletni archiv)
    salamander->OpenProgressDialog("Unpacking DemoPlug Archive", FALSE, NULL, FALSE);
    salamander->ProgressSetTotalSize(CQuadWord(100, 0), CQuadWord(-1, -1));

    // provedeni rozpakovani
    salamander->ProgressDialogAddText("test text 1", FALSE);
    Sleep(500); // simulace cinosti
    if (!salamander->ProgressAddSize(50, FALSE))
        ret = FALSE; // break;   // preruseni akce
    else
    {
        salamander->ProgressDialogAddText("test text 2", FALSE);
        Sleep(500); // simulace cinosti
        if (!salamander->ProgressAddSize(50, FALSE))
            ret = FALSE; // break;   // preruseni akce
        else
            ret = TRUE;
    }
    // ret je pri uspechu TRUE, jinak zustava FALSE

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
        return TRUE; // pri critical shutdown se na nic neptame

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
        tempPath[0] = 0; // chyba -> jdeme do systemoveho TEMPu
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
        // pridame masku (nevejde se = nema smysl nic hledat)
        if (SalamanderGeneral->SalPathAppend(tmpDir, "SAL*.tmp", 2 * MAX_PATH))
        {
            TIndirectArray<char> tmpDirs(10, 50);

            WIN32_FIND_DATA data;
            HANDLE find = HANDLES_Q(FindFirstFile(tmpDir, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                do
                { // zpracujeme vsechny nalezene adresare (chyby hledani ignorujeme)
                    if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) && strlen(data.cFileName) > 3)
                    {
                        char* s = data.cFileName + 3;
                        while (*s != 0 && *s != '.' &&
                               (*s >= '0' && *s <= '9' || *s >= 'a' && *s <= 'f' || *s >= 'A' && *s <= 'F'))
                            s++;
                        if (SalamanderGeneral->StrICmp(s, ".tmp") == 0) // odpovida "SAL" + hex-cislo + ".tmp" = je temer jiste nas adresar
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
  // test messageboxu (messageboxy pouzivat jen v krajnim pripade) - dela bordel
  // v pripade, ze ma nektery plugin otevreny nejaky vlastni modalni dialog (kosmeticke
  // vady: messagebox k tomuto dialogu neni modalni a po svem zavreni aktivuje
  // parenta dialogu)
  char buf[500];
  sprintf(buf, "File \"%s\" will be deleted.", fileName);
  SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                   buf, LoadStr(IDS_PLUGINNAME),
                                   MB_OK | MB_ICONINFORMATION);
*/

    // je-li critical shutdown, neni vhodna doba na pomale mazani souboru (brzo nas proces zabiji),
    // pri prvnim dalsim startu pluginu v prvnim spustenem Salamanderovi se to smaze "v klidu",
    // nic lepsiho asi nevymyslime
    if (SalamanderGeneral->IsCriticalShutdown())
        return;

    // ukazka pouziti wait-okenka
    static DWORD ti = 0; // cas zacatku mazani prvniho souboru z rady (pri vice najednou mazanych souborech)
    DWORD showTime = 1000;
    if (firstFile)
        ti = GetTickCount(); // zajistime, aby se wait-okno ukazovalo po jedne sekunde v ramci vsech mazani
    else
    {
        DWORD work = GetTickCount() - ti; // jak dlouho uz se maze (od mazani prvniho souboru z rady)
        if (work < 1000)
            showTime -= work;
        else
            showTime = 0;
    }
    SalamanderGeneral->CreateSafeWaitWindow("Deleting temporary file unpacked from archive, please wait...",
                                            LoadStr(IDS_PLUGINNAME), showTime, FALSE /* TRUE pokud umime cinnost prerusit */,
                                            SalamanderGeneral->GetMsgBoxParent());
    // simulace cinnosti (okenko je videt az po 1 sekunde)
    Sleep(2000);

    // obycejne mazani souboru
    SalamanderGeneral->ClearReadOnlyAttr(fileName);

    if (DeleteFile(fileName))
        TRACE_I("Temporary copy from disk-cache (" << fileName << ") was deleted.");
    else
        TRACE_I("Unable to delete temporary copy from disk-cache (" << fileName << ").");

    // zavreme wait-okenko, akce skoncila
    SalamanderGeneral->DestroySafeWaitWindow();
}

BOOL WINAPI
CPluginInterfaceForArchiver::PrematureDeleteTmpCopy(HWND parent, int copiesCount)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::PrematureDeleteTmpCopy(, %d)", copiesCount);

    // je-li critical shutdown, neni vhodna doba na pomale mazani souboru (brzo nas proces zabiji),
    // pri prvnim dalsim startu pluginu v prvnim spustenem Salamanderovi se to smaze "v klidu",
    // nic lepsiho asi nevymyslime
    if (SalamanderGeneral->IsCriticalShutdown())
        return FALSE; // pri critical shutdown se na nic neptame

    char buf[500];
    sprintf(buf, "%d temporary file(s) extracted from archive are still in use.\n"
                 "Do you want to delete them anyway?",
            copiesCount);
    return SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_PLUGINNAME),
                                            MB_YESNO | MB_ICONQUESTION) == IDYES;
}
