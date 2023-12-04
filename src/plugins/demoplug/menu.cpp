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

// ****************************************************************************
// SEKCE MENU
// ****************************************************************************

DWORD WINAPI
CPluginInterfaceForMenuExt::GetMenuItemState(int id, DWORD eventMask)
{
    if (id == MENUCMD_DOPFILES)
    {
        // musi byt na disku nebo v nasem pluginu
        if ((eventMask & (MENU_EVENT_DISK | MENU_EVENT_THIS_PLUGIN_ARCH)) == 0)
            return 0;

        // vezmeme bud oznacene soubory nebo soubor pod fokusem
        const CFileData* file = NULL;
        BOOL isDir;
        if ((eventMask & MENU_EVENT_FILES_SELECTED) == 0)
        {
            if ((eventMask & MENU_EVENT_FILE_FOCUSED) == 0)
                return 0;
            file = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir); // selected neni nic -> enumerace failne
        }

        BOOL ret = TRUE;
        int count = 0;

        char mask[10];
        SalamanderGeneral->PrepareMask(mask, "*.dop");

        int index = 0;
        if (file == NULL)
            file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
        while (file != NULL)
        {
            if (!isDir && SalamanderGeneral->AgreeMask(file->Name, mask, *file->Ext != 0))
                count++;
            else
            {
                ret = FALSE;
                break;
            }
            file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
        }

        return (count != 0 && ret) ? MENU_ITEM_STATE_ENABLED : 0; // vsechny oznacene soubory jsou *.dop
    }
    else
    {
        if (id == MENUCMD_SEP || id == MENUCMD_HIDDENITEM) // osetrime separator a polozku schovavajici se na stisk Shiftu
        {
            return MENU_ITEM_STATE_ENABLED |
                   ((GetKeyState(VK_SHIFT) & 0x8000) ? MENU_ITEM_STATE_HIDDEN : 0);
        }
        else
            TRACE_E("Unexpected call to CPluginInterfaceForMenuExt::GetMenuItemState()");
    }
    return 0;
}

struct CTestItem
{
    int a;
    char b[100];
    CTestItem(int i)
    {
        a = i;
        b[0] = 0;
    }
    ~CTestItem()
    {
        a = 0;
    }
};

struct CDEMOPLUGOperFromDiskData
{
    CQuadWord* RetSize;
    HWND Parent;
    BOOL Success;
};

void WINAPI DEMOPLUGOperationFromDisk(const char* sourcePath, SalEnumSelection2 next,
                                      void* nextParam, void* param)
{
    CDEMOPLUGOperFromDiskData* data = (CDEMOPLUGOperFromDiskData*)param;
    CQuadWord* retSize = data->RetSize;

    data->Success = TRUE; // zatim hlasime uspech operace

    BOOL isDir;
    const char* name;
    const char* dosName; // dummy
    CQuadWord size;
    DWORD attr;
    FILETIME lastWrite;
    CQuadWord totalSize(0, 0);
    int errorOccured;
    while ((name = next(data->Parent, 3, &dosName, &isDir, &size, &attr, &lastWrite,
                        nextParam, &errorOccured)) != NULL)
    {
        if (errorOccured == SALENUM_ERROR) // sem SALENUM_CANCEL prijit nemuze
            data->Success = FALSE;
        if (!isDir)
            totalSize += size;
    }
    if (errorOccured != SALENUM_SUCCESS)
    {
        data->Success = FALSE;
        // nastala chyba pri enumeraci a uzivatel si preje ukonceni operace (cancel)
        // if (errorOccured == SALENUM_CANCEL);
    }

    *retSize = totalSize;
}

BOOL WINAPI
CPluginInterfaceForMenuExt::ExecuteMenuItem(CSalamanderForOperationsAbstract* salamander,
                                            HWND parent, int id, DWORD eventMask)
{
    switch (id)
    {
    case MENUCMD_ALWAYS:
    {
        /*
      // test arraylt.h - TDirectArray
      TDirectArray<DWORD> arr(10, 5);
      int i;
      for (i = 0; i < 20; i++)
        arr.Add(i);
      arr.Delete(5);
      arr.Delete(10);
      arr.Delete(15);
      arr.Insert(5, 50);
      for (i = 0; i < arr.Count; i++)
      {
        int test = arr[i];
        if (test == 19)
        {
          TRACE_I("last item in array");
        }
      }

      // test arraylt.h - TIndirectArray
      TIndirectArray<CTestItem> arr2(10, 5);   // automaticky dtDelete (na prvky se ma volat delete)
      for (i = 0; i < 20; i++)
        arr2.Add(new CTestItem(i));
      arr2.Delete(5);
      arr2.Delete(10);
      arr2.Delete(15);
      arr2.Insert(5, new CTestItem(50));
      for (i = 0; i < arr2.Count; i++)
      {
        CTestItem *test = arr2[i];
        if (test != NULL && test->a == 19)
        {
          TRACE_I("last item in array2");
        }
      }
//      arr2.DestroyMembers();   // pokud nezavolame, destrukce probehne automaticky v destruktoru arr2
*/
        /*      // select-all
      SalamanderGeneral->SelectAllPanelItems(PANEL_SOURCE, TRUE, TRUE);
*/
        /*
      // fokus prvni selectly polozky nebo prvni polozky
      int index = 0;
      BOOL isDir;
      const CFileData *file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
      if (file == NULL)
      {
        index = 0;
        file = SalamanderGeneral->GetPanelItem(PANEL_SOURCE, &index, &isDir);
      }
      if (file != NULL)
      {
        SalamanderGeneral->SetPanelFocusedItem(PANEL_SOURCE, file, TRUE);
      }
*/
        /*
      // invert oznaceni
      const CFileData *file;
      int index = 0;
      while (1)
      {
        file = SalamanderGeneral->GetPanelItem(PANEL_SOURCE, &index, NULL);
        if (file == NULL) break;
        SalamanderGeneral->SelectPanelItem(PANEL_SOURCE, file, !file->Selected);
      }
      SalamanderGeneral->RepaintChangedItems(PANEL_SOURCE);
*/
        /*
      char diskSize[100];
      SalamanderGeneral->PrintDiskSize(diskSize, 123456, 1);

      BOOL b1 = SalamanderGeneral->HasTheSameRootPath("c:\\path1", "c:\\path2");

      BOOL b2 = SalamanderGeneral->IsTheSamePath("c:\\path", "c:\\path\\");

      char root[MAX_PATH];
      int len = SalamanderGeneral->GetRootPath(root, "\\\\server\\share\\test\\path");

      char path[MAX_PATH];
      strcpy(path, "\\\\server\\share\\test\\path");
      char *cutDir;
      BOOL b3 = SalamanderGeneral->CutDirectory(path, &cutDir);
      strcpy(path, "\\\\server\\share\\test\\path");
      SalamanderGeneral->SalPathAppend(path, "new", MAX_PATH);
      strcpy(path, "\\\\server\\share\\test\\path");
      SalamanderGeneral->SalPathAddBackslash(path, MAX_PATH);
      strcpy(path, "\\\\server\\share\\test\\path\\");
      SalamanderGeneral->SalPathRemoveBackslash(path);
      strcpy(path, "\\\\server\\share\\test\\file");
      SalamanderGeneral->SalPathStripPath(path);
      strcpy(path, "\\\\server\\share\\test\\file.ext");
      SalamanderGeneral->SalPathAddExtension(path, ".txt", MAX_PATH);
      SalamanderGeneral->SalPathRemoveExtension(path);
      SalamanderGeneral->SalPathAddExtension(path, ".txt", MAX_PATH);
      strcpy(path, "\\\\server\\share\\test\\file.ext");
      SalamanderGeneral->SalPathRenameExtension(path, ".txt", MAX_PATH);
      strcpy(path, "\\\\server\\share\\test\\file.ext");
      const char *name = SalamanderGeneral->SalPathFindFileName(path);

      strcpy(path, "path\\ignore\\..\\.\\name");
      int errTextID;
      char nextFocus[MAX_PATH];
      SalamanderGeneral->SalUpdateDefaultDir(TRUE);
      if (!SalamanderGeneral->SalGetFullName(path, &errTextID, "c:\\zumpa", nextFocus))
      {
        char buf[200];
        SalamanderGeneral->GetGFNErrorText(errTextID, buf, 200);
      }

      char buffer[50];
      char *res = SalamanderGeneral->NumberToStr(buffer, 1234567);
*/
        // test IsPluginInstalled
        //      BOOL installed = SalamanderGeneral->IsPluginInstalled("ieviewer\\ieviewer.spl");
        /*
      // test ViewFileInPluginViewer
//      CSalamanderPluginViewerData viewerData;
      CSalamanderPluginInternalViewerData viewerData;
      char fileNameBuf[MAX_PATH];
      viewerData.Size = sizeof(viewerData);
      viewerData.FileName = fileNameBuf;
      DWORD error;
      if (SalamanderGeneral->SalGetTempFileName(NULL, "view", fileNameBuf, TRUE, &error))
      {
        HANDLE file = HANDLES_Q(CreateFile(viewerData.FileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
        if (file != INVALID_HANDLE_VALUE)
        {
          const char *content = "This is a text file\r\nwhich was made for test\r\npurposes only.";
          DWORD written;
          WriteFile(file, content, strlen(content), &written, NULL);
          HANDLES(CloseHandle(file));

          int err;
//          BOOL ok = SalamanderGeneral->ViewFileInPluginViewer("ieviewer\\ieviewer.spl",
//                                                              &viewerData, FALSE, NULL, NULL, err);
//          BOOL ok = SalamanderGeneral->ViewFileInPluginViewer("ieviewer\\ieviewer.spl",
//                                                              &viewerData, TRUE, NULL, "test.txt", err);
//          BOOL ok = SalamanderGeneral->ViewFileInPluginViewer(NULL, &viewerData, FALSE, NULL, NULL, err);
          viewerData.Mode = 1;  // hex mode
          viewerData.Caption = "My file test.txt";
          viewerData.WholeCaption = FALSE; // nechame pripojit standardni nazev vieweru
          BOOL ok = SalamanderGeneral->ViewFileInPluginViewer(NULL, &viewerData, TRUE, NULL, "test.txt", err);
//          DeleteFile(viewerData.FileName);   // pokud se nepouziva disk-cache, je potreba soubor mazat
        }
      }
*/
        /*
      char path[MAX_PATH];
      BOOL havePath = SalamanderGeneral->GetTargetDirectory(parent, parent, "Change Directory",
                                                            "Select the directory you want to visit.",
                                                            path, FALSE, "C:\\");
*/
        /*
      int index = 0;
      const char *name, *table;
      while (SalamanderGeneral->EnumConversionTables(parent, &index, &name, &table))
      {
        TRACE_I("Conversion: " << (name == NULL ? "separator" : name));
      }
*/
        /*
      char codePage[101];
      SalamanderGeneral->GetWindowsCodePage(NULL, codePage);
      if (codePage[0] != 0)  // jen pokud je WindowsCodePage znama
      {
        char conversion[200];
        strcpy(conversion, "ISO-8859-2");  // puvodni kodovani (zde treba iso-2)
        strcat(conversion, codePage);  // pripojime cilove kodovani
        char table[256];
        if (SalamanderGeneral->GetConversionTable(NULL, table, conversion))
        {
          char buf[20] = "ľlu»oučký kůň";   // zde je text pro konverzi

          // test RecognizeFileType: ozkousime jestli se 'buf' rozpozna jako text s kodovou strankou iso-2
          BOOL isText;
          char recCodePage[101];
          SalamanderGeneral->RecognizeFileType(NULL, buf, strlen(buf), FALSE, &isText, recCodePage);

          char *s = buf;
          while (*s != 0)   // provedeme konverzi
          {
            *s = table[(unsigned char)*s];
            s++;
          }
          TRACE_I("After conversion: " << buf);   // vypiseme vysledek
        }
      }
*/
        /*
      // pro jednoduchost neosetrujeme jestli je neco oznacene nebo aspon fokus neni na up-dir symbolu
      CQuadWord size = CQuadWord(-1, -1);  // error
      CDEMOPLUGOperFromDiskData data;
      data.RetSize = &size;
      data.Parent = parent;
      data.Success = FALSE;
      SalamanderGeneral->CallPluginOperationFromDisk(PANEL_SOURCE, DEMOPLUGOperationFromDisk, &data);
      if (data.Success) TRACE_I("Selection size is " << size.GetDouble() << " bytes.");
      else TRACE_I("Not all files and directories were processed! Processed part of selection has size of " << size.GetDouble() << " bytes.");
*/
        /*
      char path[MAX_PATH];
      lstrcpyn(path, "F:\\DRIVE_D", MAX_PATH);
      CQuadWord total;
      CQuadWord space;
      SalamanderGeneral->GetDiskFreeSpace(&space, path, &total);
      TRACE_I("Disk free space: " << space.GetDouble() << " from " << total.GetDouble() << " bytes.");
      DWORD a, b, donot_use_1, donot_use_2;
      if (SalamanderGeneral->SalGetDiskFreeSpace(path, &a, &b, &donot_use_1, &donot_use_2))
        TRACE_I("Disk parameters: " << a << ", " << b);
      char rootOrCurReparsePoint[MAX_PATH];
      char volumeNameBuffer[200];
      DWORD volumeSerialNumber;
      DWORD maximumComponentLength;
      DWORD fileSystemFlags;
      char fileSystemNameBuffer[100];
      if (SalamanderGeneral->SalGetVolumeInformation(path, rootOrCurReparsePoint, volumeNameBuffer, 200,
                                                     &volumeSerialNumber, &maximumComponentLength,
                                                     &fileSystemFlags, fileSystemNameBuffer, 100))
      {
        TRACE_I("Disk info (" << path << "): " << rootOrCurReparsePoint << ", " << volumeNameBuffer << ", " <<
                volumeSerialNumber << ", " << maximumComponentLength << ", " << fileSystemFlags <<
                ", " << fileSystemNameBuffer);
      }
      TRACE_I("Disk type: " << SalamanderGeneral->SalGetDriveType(path));
*/
        /*
      const char *text = "Tento text je text, ve kterem se bude vyhledavat vzorek. Text muze byt libovolne dlouhy.";
      int textLen = strlen(text);
      CSalamanderBMSearchData *bm = SalamanderGeneral->AllocSalamanderBMSearchData();
      if (bm != NULL)
      {
        bm->Set("text", SASF_CASESENSITIVE | SASF_FORWARD);   // vzorek 'text' (case-sensitive), hledame dopredu
        if (bm->IsGood())
        {
          int found = -2;
          while (found != -1)
          {
            found = bm->SearchForward(text, textLen, max(found + 1, 0));
            if (found != -1) TRACE_I("boyer-moore - found: " << found);
          }
        }
        SalamanderGeneral->FreeSalamanderBMSearchData(bm);
      }
      CSalamanderREGEXPSearchData *regexp = SalamanderGeneral->AllocSalamanderREGEXPSearchData();
      if (regexp != NULL)
      {
        if (regexp->Set("(^| )t[^ ]*", 0))  // slova (predchazi mezera nebo zacatek radky) zacinajici
        {                                   // na 't'/'T' (neni case-sensitive)), hledame odzadu
          regexp->SetLine(text, text + textLen);
          int found = textLen;
          while (found != -1)
          {
            int foundLen;
            found = regexp->SearchBackward(found, foundLen);
            if (found != -1) TRACE_I("regexp - found: " << found << ", len = " << foundLen);
          }
        }
        else   // chyba - regularniho vyrazu (nesedi zavorky, atd.) nebo nedostatek pameti
        {
          SalamanderGeneral->ShowMessageBox(regexp->GetLastErrorText(), "Regular Expression Error",
                                            MSGBOX_ERROR);
        }
        SalamanderGeneral->FreeSalamanderREGEXPSearchData(regexp);
      }
*/
        /*
      BOOL again = TRUE;
      while (again)
      {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = parent;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT;
        params.Caption = LoadStr(IDS_PLUGINNAME);
        params.Text = "Command Always. Notice silent opening of this box.";
        params.CheckBoxText = "Show this box again?";
        params.CheckBoxValue = &again;
        SalamanderGeneral->SalMessageBoxEx(&params);
      }
*/
        /*
      char *buf = "Chceme spocitat CRC32 tohoto strasne dlouheho textu.";
      char *end = buf + strlen(buf);
      char *s = buf;
      DWORD crc32 = 0;  // init na nulu
      while (s < end)
      {
        // volani po 10 bytech je zde jen pro priklad pouziti - davkove zpracovani se hodi napr.
        // pri cteni souboru - jinak cim vetsi buffer se preda, tim je celkova doba vypoctu kratsi
        int size = min(end - s, 10);
        crc32 = SalamanderGeneral->UpdateCrc32(s, size, crc32);
        s += size;
      }
*/
        /*
      int selectedFiles, selectedDirs;
      BOOL work = SalamanderGeneral->GetPanelSelection(PANEL_SOURCE, &selectedFiles, &selectedDirs);
*/
        /*
      const char *s1 = "Chleba";
      const char *s2 = "cihla";
      //const char *s2 = "cHlEbA";
      int l1 = strlen(s1);
      int l2 = strlen(s2);
      int res;
      BOOL numericalyEqual;
      res = SalamanderGeneral->RegSetStrICmp(s1, s2);
      res = SalamanderGeneral->RegSetStrICmpEx(s1, l1, s2, l2, &numericalyEqual);
      res = SalamanderGeneral->RegSetStrCmp(s1, s2);
      res = SalamanderGeneral->RegSetStrCmpEx(s1, l1, s2, l2, &numericalyEqual);
*/
        /*
      char fileComp[MAX_PATH];
      BOOL ok;
      strcpy(fileComp, "prn");
      ok = SalamanderGeneral->SalIsValidFileNameComponent(fileComp);
      SalamanderGeneral->SalMakeValidFileNameComponent(fileComp);
      strcpy(fileComp, "ahoj:");
      ok = SalamanderGeneral->SalIsValidFileNameComponent(fileComp);
      SalamanderGeneral->SalMakeValidFileNameComponent(fileComp);
*/
        /*
      char masks[MAX_GROUPMASK];
      if (SalamanderGeneral->GetFilterFromPanel(PANEL_SOURCE, masks, MAX_GROUPMASK))
      {
        TRACE_I("Filter in source panel: " << masks);
      }
*/
        /*
      char firstCreatedDir[MAX_PATH];
      BOOL ok = SalamanderGeneral->CheckAndCreateDirectory("C:\\test\\test_dir\\second_dir",
                                                           NULL, FALSE, NULL, 0,
                                                           firstCreatedDir);
*/
        /*
      GetAsyncKeyState(VK_ESCAPE);  // init GetAsyncKeyState - viz help
      HWND waitWndParent = SalamanderGeneral->GetMsgBoxParent();
      SalamanderGeneral->CreateSafeWaitWindow("Waiting for ESC key, press ESC key...", LoadStr(IDS_PLUGINNAME),
                                              1000, TRUE, waitWndParent);
      while (1)
      {
        Sleep(100);  // simulace nejakeho cekani (treba na ukonceni threadu)

        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == waitWndParent ||
            SalamanderGeneral->GetSafeWaitWindowClosePressed())
        {
          MSG msg;   // vyhodime nabufferovany ESC
          while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));

          SalamanderGeneral->ShowSafeWaitWindow(FALSE);
          if (SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(),
                                               "Do you really want to end this waiting?",
                                               LoadStr(IDS_PLUGINNAME),
                                               MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                               MB_ICONQUESTION) == IDYES)
          {
            break;
          }
          else UpdateWindow(waitWndParent);  // at nekoukame na zbytky messageboxu
          SalamanderGeneral->ShowSafeWaitWindow(TRUE);
        }
      }
      SalamanderGeneral->DestroySafeWaitWindow();
*/

        /*
      // password mananger
      CSalamanderPasswordManagerAbstract *passwordManager = NULL;
      passwordManager = SalamanderGeneral->GetSalamanderPasswordManager();
      if (passwordManager != NULL)
      {
        // show safe/unsafe password storage
        if (passwordManager->IsUsingMasterPassword())
          TRACE_I("The Password Manager is using the Master Password, stored password are AES-protected.");
        else
          TRACE_I("The Master Password is not set! Stored passwords are not protected.");

        // encrypt (or scramble only) plain text password
        const char *plainPassword = "PlainTextPwd123";
        BYTE *encryptedPassword = NULL;
        int encryptedPasswordSize = 0;
        BOOL encrypt = FALSE; // FALSE: scramble only; TRUE: encrypted with AES

        if (passwordManager->IsUsingMasterPassword() &&
            (passwordManager->IsMasterPasswordSet() || passwordManager->AskForMasterPassword(parent)))
        {
          encrypt = TRUE; // use AES
        }

        if (passwordManager->EncryptPassword(plainPassword, &encryptedPassword, &encryptedPasswordSize, encrypt))
        {
          char *decryptedPassword;
          if (passwordManager->DecryptPassword(encryptedPassword, encryptedPasswordSize, &decryptedPassword))
          {
            TRACE_I("Decrypted password: " << decryptedPassword);
            memset(decryptedPassword, 0, lstrlen(decryptedPassword));  // clear memory with plain password
            SalamanderGeneral->Free(decryptedPassword);
          }

          if (!encrypt) memset(encryptedPassword, 0, encryptedPasswordSize);  // clear memory with scrambled password
          SalamanderGeneral->Free(encryptedPassword);
        }
      }
*/

        /*
      // GetFocusedItemMenuPos() sample -- display context menu for focused item in active panel
      POINT p;
      SalamanderGeneral->GetFocusedItemMenuPos(&p);
      HMENU hMenu = CreatePopupMenu();
      AppendMenu(hMenu, MF_STRING | MF_ENABLED, 1, "Item 1");
      AppendMenu(hMenu, MF_STRING | MF_ENABLED, 2, "Item 2");
      AppendMenu(hMenu, MF_STRING | MF_ENABLED, 3, "Item 3");
      BOOL cmd = TrackPopupMenu(hMenu, TPM_RETURNCMD | TPM_LEFTALIGN, p.x, p.y, 0, SalamanderGeneral->GetMainWindowHWND(), NULL);
      DestroyMenu(hMenu);
      if (cmd != 0)
      {
      // handle commands
      }
*/

        /*
      // example of SafeWaitWindow usage including Escape/Close-click handling
      GetAsyncKeyState(VK_ESCAPE);  // init GetAsyncKeyState - see msdn documentation
      HWND waitWndParent = SalamanderGeneral->GetMainWindowHWND();
      SalamanderGeneral->CreateSafeWaitWindow("aaa", "bbb", 0, TRUE, waitWndParent);
      int i;
      for (i = 0; i < 100; i++) // for+Sleep() -- pretend some work; for this example only
      {
        Sleep(100); 
        BOOL cancel = FALSE;
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8001) && GetForegroundWindow() == waitWndParent)
        {
          MSG msg;   // remove ESC key from buffer
          while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE));
          cancel = TRUE;
        }
        else
        {
          cancel = SalamanderGeneral->GetSafeWaitWindowClosePressed();
        }
  
        if (cancel)
        {
          SalamanderGeneral->ShowSafeWaitWindow(FALSE);
          if (SalamanderGeneral->SalMessageBox(waitWndParent,
                                               "Are you sure?",
                                               LoadStr(IDS_PLUGINNAME),
                                               MB_YESNO | MSGBOXEX_ESCAPEENABLED |
                                               MB_ICONQUESTION) == IDYES)
          {
            break;
          }
          SalamanderGeneral->ShowSafeWaitWindow(TRUE);
        }
      }
      SalamanderGeneral->DestroySafeWaitWindow();
*/

        //      SalamanderGeneral->PostOpenPackDlgForThisPlugin(0);
        //      SalamanderGeneral->PostOpenUnpackDlgForThisPlugin(NULL);

        SalamanderGeneral->ShowMessageBox("Always", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)
        break;
    }

    case MENUCMD_DIR:
        SalamanderGeneral->ShowMessageBox("Directory", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;
    case MENUCMD_ARCFILE:
        SalamanderGeneral->ShowMessageBox("Archive File", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;
    case MENUCMD_FILEONDISK:
        SalamanderGeneral->ShowMessageBox("File on Disk", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;
    case MENUCMD_ARCFILEONDISK:
        SalamanderGeneral->ShowMessageBox("Archive File on Disk", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;

    case MENUCMD_DOPFILES:
    {
        // zjistime jestli jde o select nebo fokus
        BOOL focus = FALSE;
        if ((eventMask & MENU_EVENT_FILES_SELECTED) == 0)
        {
            if ((eventMask & MENU_EVENT_FILE_FOCUSED) == 0)
                return FALSE;
            focus = TRUE;
        }

        // akce bude dvoufazova - priprava + provadeni
        int count = 0;
        BOOL ret = TRUE;
        int stage;
        for (stage = 0; stage < 2; stage++)
        {
            if (stage == 1) // faze provadeni
            {
                salamander->OpenProgressDialog("Command \"*.D&OP File(s)\"", FALSE, NULL, FALSE);
                salamander->ProgressSetTotalSize(CQuadWord(count, 0), CQuadWord(-1, -1));
            }

            int index = 0;
            const CFileData* file;
            BOOL isDir;
            if (!focus)
                file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
            else
                file = SalamanderGeneral->GetPanelFocusedItem(PANEL_SOURCE, &isDir);
            while (file != NULL)
            {
                // akce nad souborem 'file'
                if (stage == 0)
                    count++; // priprava - napocteme pocet souboru
                else         // provadeni - zahybeme progressem
                {
                    salamander->ProgressDialogAddText(file->Name, FALSE);
                    Sleep(500); // simulace cinosti
                    if (!salamander->ProgressAddSize(1, FALSE))
                    {
                        salamander->ProgressDialogAddText("canceling operation, please wait...", FALSE);
                        salamander->ProgressEnableCancel(FALSE);
                        Sleep(1000); // simulace uklizeci cinosti
                        ret = FALSE; // Cancel -> nebudeme odznacovat
                        break;       // preruseni akce
                    }
                }
                if (!focus)
                    file = SalamanderGeneral->GetPanelSelectedItem(PANEL_SOURCE, &index, &isDir);
                else
                    break;
            }

            if (stage == 1) // faze provadeni
            {
                Sleep(500); // simulace cinosti
                salamander->CloseProgressDialog();
            }
        }
        SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_SOURCE); // tento prikaz povazujeme za praci s cestou (objevi se v Alt+F12)
        return ret;
    }

    case MENUCMD_FILESDIRSINARC:
        SalamanderGeneral->ShowMessageBox("File(s) and/or Directory(ies) in Archive", LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
        break;

    case MENUCMD_ENTERDISKPATH: // ukazka postupu, jak se osetruje vstup cesty k adresari/souboru
    {
        // navrh retezce - zde jen windowsova cesta v cilovem panelu (jinak prazdny retezec)
        char path[MAX_PATH];
        path[0] = 0;
        int type;
        if (SalamanderGeneral->GetPanelPath(PANEL_TARGET, path, MAX_PATH, &type, NULL))
        {
            if (type != PATH_TYPE_WINDOWS)
                path[0] = 0; // bereme jen diskove cesty
        }

        // aktualni cesta je potreba pro prevody relativnich cest na absolutni
        BOOL curPathIsDisk = FALSE;
        char curPath[MAX_PATH];
        curPath[0] = 0;
        if (SalamanderGeneral->GetPanelPath(PANEL_SOURCE, curPath, MAX_PATH, &type, NULL))
        {
            if (type != PATH_TYPE_WINDOWS)
                curPath[0] = 0; // bereme jen diskove cesty
            else
                curPathIsDisk = TRUE;
        }

        SalamanderGeneral->SalUpdateDefaultDir(TRUE); // update pred pouzitim SalParsePath

        BOOL filePath = FALSE; // TRUE/FALSE = cesta k souboru/adresari
        BOOL success = FALSE;  // TRUE = 'path' obsahuje cestu k souboru/adresari (nenastala zadna chyba)
        while (1)              // budeme se ptat dokud nebude cesta o.k. nebo user neda cancel
        {
            CPathDialog dlg(parent, path, &filePath);
            if (dlg.Execute() == IDOK)
            {
                // rozpoznani zadane cesty
                int len = (int)strlen(path);
                BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // cesta konci na backslash -> nutne adresar
                BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                                   path[1] == ':'); // cesta typu "c:" musi byt i po expanzi cesta (ne soubor)
                int pathType;
                BOOL pathIsDir;
                char* secondPart;
                if (SalamanderGeneral->SalParsePath(parent, path, pathType, pathIsDir, secondPart,
                                                    "Path Error", NULL, curPathIsDisk, curPath,
                                                    NULL, NULL, MAX_PATH))
                {
                    if (pathType == PATH_TYPE_WINDOWS) // Windows cesta (disk + UNC)
                    {
                        if (pathIsDir) // existujici cast cesty je adresar
                        {
                            if (*secondPart != 0) // je zde i neexistujici cast cesty
                            {
                                if (filePath) // cesta k souboru, zkontrolujeme jestli v neexistujici ceste nejsou i podadresare
                                {
                                    char* s = secondPart;
                                    while (*s != 0 && *s != '\\')
                                        s++;
                                    if (*s == '\\') // obsahuje i podadresare, neumime, ohlasime chybu
                                    {
                                        SalamanderGeneral->SalMessageBox(parent, "Unable to create the file specified, because its path doesn't exist.",
                                                                         "Path Error", MB_OK | MB_ICONEXCLAMATION);
                                        continue; // dame si dalsi kolo
                                    }
                                }
                                else
                                {
                                    // error - cesta musi existovat
                                    SalamanderGeneral->SalMessageBox(parent, "The path specified doesn't exist.",
                                                                     "Path Error", MB_OK | MB_ICONEXCLAMATION);
                                    continue; // dame si dalsi kolo
                                }
                            }
                        }
                        else // prepis souboru - 'secondPart' ukazuje na jmeno souboru v ceste 'path'
                        {
                            if (!filePath)
                            {
                                // error - cesta k souboru a ne k adresari
                                SalamanderGeneral->SalMessageBox(parent, "Unable to create the path specified, name has already been used for a file.",
                                                                 "Path Error", MB_OK | MB_ICONEXCLAMATION);
                                continue; // dame si dalsi kolo
                            }
                        }

                        success = TRUE; // 'path' je pouzitelna
                        break;
                    }
                    else // FS/archivova cesta
                    {
                        if (pathType == PATH_TYPE_ARCHIVE && (backslashAtEnd || mustBePath)) // pridame odstraneny backslash
                        {
                            SalamanderGeneral->SalPathAddBackslash(path, MAX_PATH);
                        }
                        // error - cesty na FS a do archivu nejsou podporovany; dame si druhe kolo
                        SalamanderGeneral->SalMessageBox(parent, "File-system and archive paths are not supported here.",
                                                         "Path Error", MB_OK | MB_ICONEXCLAMATION);
                    }
                }
                else
                {
                    // uz byl vypsany error z SalParsePath; dame si druhe kolo
                }
            }
            else
                break; // cancel
        }

        if (success) // 'path' je cesta k souboru/adresari, provedeme s ni pozadovanou akci
        {
            // v DemoPluginu se nic neprovadi
        }

        return FALSE; // neodznacujeme polozky v panelu
    }

    case MENUCMD_ALLUSERS:
    case MENUCMD_INTADVUSERS:
    case MENUCMD_ADVUSERS:
    {
        SalamanderGeneral->SalMessageBox(parent, "Hello there!",
                                         "User's skill level demonstration", MB_OK | MB_ICONINFORMATION);
        break;
    }

    case MENUCMD_SHOWCONTROLS:
    {
        CCtrlExampleDialog dlg(parent);
        dlg.Execute();
        break;
    }

    case MENUCMD_CHECKDEMOPLUGTMPDIR:
    {
        ClearTEMPIfNeeded(SalamanderGeneral->GetMsgBoxParent());
        return FALSE; // neodznacovat
    }

    case MENUCMD_DISCONNECT_LEFT:
    case MENUCMD_DISCONNECT_RIGHT:
    case MENUCMD_DISCONNECT_ACTIVE:
    {
        int panel = PANEL_SOURCE;
        switch (id)
        {
        case MENUCMD_DISCONNECT_LEFT:
            panel = PANEL_LEFT;
            break;
        case MENUCMD_DISCONNECT_RIGHT:
            panel = PANEL_RIGHT;
            break;
        case MENUCMD_DISCONNECT_ACTIVE:
            panel = PANEL_SOURCE;
            break;
        }
        SalamanderGeneral->DisconnectFSFromPanel(parent, panel);
        return FALSE; // neodznacovat
    }

    default:
        SalamanderGeneral->ShowMessageBox("Unknown command.", LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        break;
    }
    return FALSE; // neodznacovat polozky v panelu
}

BOOL WINAPI
CPluginInterfaceForMenuExt::HelpForMenuItem(HWND parent, int id)
{
    int helpID = 0;
    switch (id)
    {
    case MENUCMD_ENTERDISKPATH:
        helpID = IDH_ENTERDISKPATH;
        break;
    }
    if (helpID != 0)
        SalamanderGeneral->OpenHtmlHelp(parent, HHCDisplayContext, helpID, FALSE);
    return helpID != 0;
}

void WINAPI
CPluginInterfaceForMenuExt::BuildMenu(HWND parent, CSalamanderBuildMenuAbstract* salamander)
{
#ifdef ENABLE_DYNAMICMENUEXT
    salamander->AddMenuItem(0, "E&nter Disk Path", SALHOTKEY('Z', HOTKEYF_CONTROL | HOTKEYF_SHIFT), MENUCMD_ENTERDISKPATH, FALSE, MENU_EVENT_TRUE, MENU_EVENT_DISK, MENU_SKILLLEVEL_ALL);
    salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
    salamander->AddMenuItem(-1, "&Disconnect", 0, MENUCMD_DISCONNECT_ACTIVE, FALSE, MENU_EVENT_TRUE, MENU_EVENT_THIS_PLUGIN_FS, MENU_SKILLLEVEL_ALL);
    static int cycle = 0; // this menu has variant content: when opened for the first time, it has only two items, when opened for the second time it has five items, etc.
    if (cycle % 4 >= 1)
    {
        salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
        salamander->AddMenuItem(-1, "&Always", 0, MENUCMD_ALWAYS, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE, MENU_SKILLLEVEL_ALL);
        salamander->AddMenuItem(-1, "D&irectory", 0, MENUCMD_DIR, FALSE, MENU_EVENT_TRUE, MENU_EVENT_DIR_FOCUSED, MENU_SKILLLEVEL_ALL);
        salamander->AddMenuItem(-1, "A&rchive File", 0, MENUCMD_ARCFILE, FALSE, MENU_EVENT_TRUE, MENU_EVENT_ARCHIVE_FOCUSED, MENU_SKILLLEVEL_ALL);
    }
    if (cycle % 4 >= 2)
    {
        salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
        salamander->AddMenuItem(-1, "&File on Disk", 0, MENUCMD_FILEONDISK, FALSE,
                                MENU_EVENT_TRUE, MENU_EVENT_DISK | MENU_EVENT_FILE_FOCUSED, MENU_SKILLLEVEL_ALL);
        salamander->AddMenuItem(-1, "Ar&chive File on Disk", 0, MENUCMD_ARCFILEONDISK, FALSE, MENU_EVENT_TRUE,
                                MENU_EVENT_DISK | MENU_EVENT_ARCHIVE_FOCUSED, MENU_SKILLLEVEL_ALL);
        salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
        salamander->AddMenuItem(-1, "*.D&OP File(s)", 0, MENUCMD_DOPFILES, TRUE, 0, 0, MENU_SKILLLEVEL_ALL);
        salamander->AddMenuItem(-1, "Fil&e(s) and/or Directory(ies) in Archive", 0, MENUCMD_FILESDIRSINARC, FALSE,
                                MENU_EVENT_FILE_FOCUSED | MENU_EVENT_DIR_FOCUSED |
                                    MENU_EVENT_FILES_SELECTED | MENU_EVENT_DIRS_SELECTED,
                                MENU_EVENT_THIS_PLUGIN_ARCH, MENU_SKILLLEVEL_ALL);
    }
    if (cycle % 4 >= 3)
    {
        salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
        salamander->AddSubmenuStart(-1, "Skill Level Demo", 0, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                    MENU_SKILLLEVEL_BEGINNER | MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
        salamander->AddMenuItem(-1, "For Beginning, Intermediate, and Advanced Users", 0, MENUCMD_ALLUSERS, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                MENU_SKILLLEVEL_BEGINNER | MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
        salamander->AddSubmenuStart(0, "Intermediate and Advanced", 0, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                    MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
        salamander->AddMenuItem(-1, "For Intermediate and Advanced Users", 0, MENUCMD_INTADVUSERS, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
        salamander->AddMenuItem(0, "For Advanced Users", 0, MENUCMD_ADVUSERS, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                MENU_SKILLLEVEL_ADVANCED);
        salamander->AddSubmenuEnd();
        salamander->AddSubmenuEnd();
        salamander->AddMenuItem(-1, NULL, 0, 0, FALSE, 0, 0, MENU_SKILLLEVEL_ALL); // separator
        salamander->AddMenuItem(-1, "&Controls provided by Open Salamander...", 0, MENUCMD_SHOWCONTROLS, FALSE, MENU_EVENT_TRUE, MENU_EVENT_TRUE,
                                MENU_SKILLLEVEL_BEGINNER | MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
        salamander->AddMenuItem(-1, NULL, 0, MENUCMD_SEP, TRUE, 0, 0, MENU_SKILLLEVEL_ADVANCED); // separator
        salamander->AddMenuItem(-1, "Press Shift key when opening menu to hide this item", 0, MENUCMD_HIDDENITEM, TRUE, 0, 0, MENU_SKILLLEVEL_ADVANCED);
    }
    cycle++;

    CGUIIconListAbstract* iconList = SalamanderGUI->CreateIconList();
    iconList->Create(16, 16, 1);
    HICON hIcon = (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS), IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
    iconList->ReplaceIcon(0, hIcon);
    DestroyIcon(hIcon);
    salamander->SetIconListForMenu(iconList); // o destrukci iconlistu se postara Salamander
#endif                                        // ENABLE_DYNAMICMENUEXT
}
