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

//
// ****************************************************************************
// CDeleteProgressDlg
//

CDeleteProgressDlg::CDeleteProgressDlg(HWND parent, CObjectOrigin origin)
    : CCommonDialog(HLanguage, IDD_PROGRESSDLG, parent, origin)
{
    ProgressBar = NULL;
    WantCancel = FALSE;
    LastTickCount = 0;
    TextCache[0] = 0;
    TextCacheIsDirty = FALSE;
    ProgressCache = 0;
    ProgressCacheIsDirty = FALSE;
}

void CDeleteProgressDlg::Set(const char* fileName, DWORD progress, BOOL dalayedPaint)
{
    lstrcpyn(TextCache, fileName != NULL ? fileName : "", MAX_PATH);
    TextCacheIsDirty = TRUE;

    if (progress != ProgressCache)
    {
        ProgressCache = progress;
        ProgressCacheIsDirty = TRUE;
    }

    if (!dalayedPaint)
        FlushDataToControls();
}

void CDeleteProgressDlg::EnableCancel(BOOL enable)
{
    if (HWindow != NULL)
    {
        HWND cancel = GetDlgItem(HWindow, IDCANCEL);
        if (IsWindowEnabled(cancel) != enable)
        {
            EnableWindow(cancel, enable);
            if (enable)
                SetFocus(cancel);
            PostMessage(cancel, BM_SETSTYLE, enable ? BS_DEFPUSHBUTTON : BS_PUSHBUTTON, TRUE);

            MSG msg;
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // chvilku venujeme userovi ...
            {
                if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
                {
                    TranslateMessage(&msg);
                    DispatchMessage(&msg);
                }
            }
        }
    }
}

BOOL CDeleteProgressDlg::GetWantCancel()
{
    MSG msg;
    while (PeekMessage(&msg, NULL, 0, 0, TRUE)) // chvilku venujeme userovi ...
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // kazdych 100ms prekreslime zmenena data (text + progress bary)
    DWORD ticks = GetTickCount();
    if (ticks - LastTickCount > 100)
    {
        LastTickCount = ticks;
        FlushDataToControls();
    }

    return WantCancel;
}

void CDeleteProgressDlg::FlushDataToControls()
{
    if (HWindow != NULL)
    {
        if (TextCacheIsDirty)
        {
            SetDlgItemText(HWindow, IDT_FILENAME, TextCache);
            TextCacheIsDirty = FALSE;
        }

        if (ProgressCacheIsDirty)
        {
            ProgressBar->SetProgress(ProgressCache, NULL);
            ProgressCacheIsDirty = FALSE;
        }
    }
}

INT_PTR
CDeleteProgressDlg::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CPathDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // pouzijeme Salamanderovsky progress bar
        ProgressBar = SalamanderGUI->AttachProgressBar(HWindow, IDP_PROGRESSBAR);
        if (ProgressBar == NULL)
        {
            DestroyWindow(HWindow); // chyba -> neotevreme dialog
            return FALSE;           // konec zpracovani
        }

        break; // chci focus od DefDlgProc
    }

    case WM_COMMAND:
    {
        if (LOWORD(wParam) == IDCANCEL)
        {
            if (!WantCancel)
            {
                FlushDataToControls();

                if (SalamanderGeneral->SalMessageBox(HWindow, "Do you want to cancel delete?", "DFS",
                                                     MB_YESNO | MB_ICONQUESTION) == IDYES)
                {
                    WantCancel = TRUE;
                    EnableCancel(FALSE);
                }
            }
            return TRUE;
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//
// ****************************************************************************
// CPluginFSInterface
//

CPluginFSInterface::CPluginFSInterface()
{
    Path[0] = 0;
    PathError = FALSE;
    FatalError = FALSE;
    CalledFromDisconnectDialog = FALSE;
}

void WINAPI
CPluginFSInterface::ReleaseObject(HWND parent)
{
    if (Path[0] != 0) // pokud je FS inicializovan, zrusime pri zavirani nase kopie souboru v disk-cache
    {
        // sestavime unikatni jmeno rootu tohoto FS v disk-cache (zasahneme vsechny kopie souboru z tohoto FS)
        char uniqueFileName[2 * MAX_PATH];
        strcpy(uniqueFileName, AssignedFSName);
        strcat(uniqueFileName, ":");
        SalamanderGeneral->GetRootPath(uniqueFileName + strlen(uniqueFileName), Path);
        // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
        // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
        SalamanderGeneral->ToLowerCase(uniqueFileName);
        SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
    }
}

BOOL WINAPI
CPluginFSInterface::GetRootPath(char* userPart)
{
    if (Path[0] != 0)
        SalamanderGeneral->GetRootPath(userPart, Path);
    else
        userPart[0] = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetCurrentPath(char* userPart)
{
    strcpy(userPart, Path);
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    lstrcpyn(buf, Path, bufSize); // pokud se nevejde cesta, jmeno se urcite take nevejde (ohlasi chybu)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return SalamanderGeneral->SalPathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    if (Path[0] == 0)
        return FALSE; // preklad neni mozny, at si chybu ohlasi sam Salamander

    char root[MAX_PATH];
    int rootLen = SalamanderGeneral->GetRootPath(root, Path);
    if (*path != '\\')
        strcpy(root, Path); // cesty typu "path" prebiraji akt. cestu na FS
    // symboly "..", "." v ceste klidne nechame, odstrani se pozdeji; zaroven nebudeme
    // zjistovat platnost cesty ani jeji syntaxi
    success = SalamanderGeneral->SalPathAppend(root, path, MAX_PATH);
    if (success && (int)strlen(root) < rootLen) // kratsi nez root neni mozne (to by byla opet rel. cesta)
    {
        success = SalamanderGeneral->SalPathAddBackslash(root, MAX_PATH);
    }
    if (success)
        success = (int)(strlen(root) + strlen(fsName) + 1) < pathSize; // vejde se?
    if (success)
        sprintf(path, "%s:%s", fsName, root);
    else
    {
        SalamanderGeneral->SalMessageBox(parent, "Unable to finish operation because of too long path.",
                                         "DFS Error", MB_OK | MB_ICONEXCLAMATION);
    }
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return currentFSNameIndex == fsNameIndex && SalamanderGeneral->IsTheSamePath(Path, userPart);
}

BOOL WINAPI
CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    if (ConnectData.UseConnectData)
    { // uzivatel otevira novou connectionu z Connect dialogu - musime vratit FALSE (uzivatel muze
        // pozadovat druhy connect na stejnou cestu)
        return FALSE;
    }

    // umoznime prechody mezi jednotlivymi fs-names (aneb ignorujeme parametry
    // 'currentFSNameIndex' a 'fsNameIndex') - normalnejsi chovani je nejspis
    // presne opacne (napr. prechod z "FTP" na "HTTP" je nesmysl)

    // zahrajeme si na to, ze kazdy FS umi obslouzit jen jeden root
    return Path[0] == 0 || SalamanderGeneral->HasTheSameRootPath(Path, userPart);
}

BOOL WINAPI
CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                               const char* userPart, char* cutFileName, BOOL* pathWasCut,
                               BOOL forceRefresh, int mode)
{
    char buf[2 * MAX_PATH + 100];
#ifndef DEMOPLUG_QUIET
    _snprintf_s(buf, _TRUNCATE, "What should ChangePath return (No==FALSE)?\n\nPath: %s:%s", fsName, userPart);
    if (SalamanderGeneral->ShowMessageBox(buf, "DFS", MSGBOX_QUESTION) == IDNO)
    {
        FatalError = FALSE;
        PathError = FALSE;
        return FALSE;
    }
    if (forceRefresh)
    {
        SalamanderGeneral->ShowMessageBox("ChangePath should change path without any caching! (forceRefresh==TRUE)", "DFS", MSGBOX_INFO);
    }
#endif // DEMOPLUG_QUIET
    if (mode != 3 && (pathWasCut != NULL || cutFileName != NULL))
    {
        TRACE_E("Incorrect value of 'mode' in CPluginFSInterface::ChangePath().");
        mode = 3;
    }
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    if (cutFileName != NULL)
        *cutFileName = 0;
    if (FatalError)
    {
        FatalError = FALSE;
        return FALSE; // ListCurrentPath selhala kvuli pameti, fatal error
    }

    char errBuf[MAX_PATH];
    errBuf[0] = 0;
    char path[MAX_PATH];

    if (*userPart == 0 && ConnectData.UseConnectData) // data z dialogu Connect
    {
        userPart = ConnectData.UserPart;
        lstrcpyn(path, ConnectData.UserPart, MAX_PATH);
    }
    else
        lstrcpyn(path, userPart, MAX_PATH);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE); // update pred pouzitim SalGetFullName; prednost ma aktivni panel (at uz je tento FS pro aktivni nebo neaktivni panel)
    int err;
    if (SalamanderGeneral->SalGetFullName(path, &err))
    {
        BOOL fileNameAlreadyCut = FALSE;
        if (PathError) // chyba pri listovani cesty (chyba vypsana uzivateli uz v ListCurrentPath)
        {              // zkusime cestu oriznout
            PathError = FALSE;
            if (!SalamanderGeneral->CutDirectory(path, NULL))
                return FALSE; // neni kam zkracovat, fatal error
            fileNameAlreadyCut = TRUE;
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
        }
        while (1)
        {
            DWORD attr = SalamanderGeneral->SalGetFileAttributes(path);
#ifndef DEMOPLUG_QUIET
            if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0)
            {
                sprintf(buf, "Press No if you don't want path \"%s\" to exist.", path);
                if (SalamanderGeneral->ShowMessageBox(buf, "DFS", MSGBOX_QUESTION) == IDNO)
                    attr = 0xFFFFFFFF;
            }
#endif // DEMOPLUG_QUIET

            if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) // uspech, vybereme cestu jako aktualni
            {
                if (errBuf[0] != 0) // pokud mame hlaseni, vypiseme ho zde (vzniklo behem zkracovani)
                {
                    sprintf(buf, "Path: %s\nError: %s", userPart, errBuf);
                    SalamanderGeneral->ShowMessageBox(buf, "DFS Error", MSGBOX_ERROR);
                }
                strcpy(Path, path);

                // jen test timeru (hodi se napr. pro keep-connection-alive)
                //        SalamanderGeneral->KillPluginFSTimer(this, TRUE, 0);   // nejprve vycistime existujici timery pro tento FS
                //        SalamanderGeneral->AddPluginFSTimer(2000, this, 1234);

                return TRUE;
            }
            else // neuspech, zkusime cestu zkratit
            {
                err = GetLastError();

                if (mode != 3 && attr != 0xFFFFFFFF || // soubor misto cesty -> ohlasime jako chybu
                    mode != 1 && attr == 0xFFFFFFFF)   // neexistence cesty -> ohlasime jako chybu
                {
                    if (attr != 0xFFFFFFFF)
                    {
                        sprintf(errBuf, "The path specified contains path to a file. Unable to open file.");
                    }
                    else
                        SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);

                    // pokud je otevreni FS casove narocne a chceme upravit chovani Change Directory (Shift+F7)
                    // jako u archivu, staci zakomentovat nasledujici radek s prikazem "break" pro 'mode' 3
                    if (mode == 3)
                        break;
                }

                char* cut;
                if (!SalamanderGeneral->CutDirectory(path, &cut)) // neni kam zkracovat, fatal error
                {
                    SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);
                    break;
                }
                else
                {
                    if (pathWasCut != NULL)
                        *pathWasCut = TRUE;
                    if (!fileNameAlreadyCut) // jmeno souboru to muze byt jen pri prvnim oriznuti
                    {
                        fileNameAlreadyCut = TRUE;
                        if (cutFileName != NULL && attr != 0xFFFFFFFF) // jde o soubor
                            lstrcpyn(cutFileName, cut, MAX_PATH);
                    }
                    else
                    {
                        if (cutFileName != NULL)
                            *cutFileName = 0; // to uz byt jmeno souboru nemuze
                    }
                }
            }
        }
    }
    else
        SalamanderGeneral->GetGFNErrorText(err, errBuf, MAX_PATH);
    sprintf(buf, "Path: %s\nError: %s", userPart, errBuf);
    SalamanderGeneral->ShowMessageBox(buf, "DFS Error", MSGBOX_ERROR);
    PathError = FALSE;
    return FALSE; // fatal path error
}

BOOL WINAPI
CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData,
                                    int& iconsType, BOOL forceRefresh)
{
#ifndef DEMOPLUG_QUIET
    if (SalamanderGeneral->ShowMessageBox("What should ListCurrentPath return (No==FALSE)?",
                                          "DFS", MSGBOX_QUESTION) == IDNO)
    {
        PathError = TRUE; // simulace chyby cesty -> bude se zkracovat
        return FALSE;
    }
    if (forceRefresh)
    {
        SalamanderGeneral->ShowMessageBox("ListCurrentPath refreshes current path (should do it without any caching)! (forceRefresh==TRUE)", "DFS", MSGBOX_INFO);
    }
#endif // DEMOPLUG_QUIET

    CFileData file;
    WIN32_FIND_DATA data;

    pluginData = new CPluginFSDataInterface(Path);
    if (pluginData == NULL)
    {
        TRACE_E("Low memory");
        FatalError = TRUE;
        return FALSE;
    }
    iconsType = pitFromPlugin;

    char buf[2 * MAX_PATH + 100];
    char curPath[MAX_PATH + 4];
    SalamanderGeneral->GetRootPath(curPath, Path);
    BOOL isRootPath = strlen(Path) <= strlen(curPath);
    strcpy(curPath, Path);
    SalamanderGeneral->SalPathAppend(curPath, "*.*", MAX_PATH + 4);
    char* name = curPath + strlen(curPath) - 3;
    HANDLE find = HANDLES_Q(FindFirstFile(curPath, &data));

    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES) // jde o chybu
        {
            SalamanderGeneral->GetErrorText(err, curPath, MAX_PATH + 4);
            sprintf(buf, "Path: %s\nError: %s", Path, curPath);
            SalamanderGeneral->ShowMessageBox(buf, "DFS Error", MSGBOX_ERROR);
            PathError = TRUE;
            goto ERR_3;
        }
    }

    /*
  //j.r.
  dir->SetFlags(SALDIRFLAG_CASESENSITIVE | SALDIRFLAG_IGNOREDUPDIRS);
  //j.r.
*/

    // nastavime jaka data ve 'file' jsou platna
    dir->SetValidData(VALID_DATA_EXTENSION |
                      /*VALID_DATA_DOSNAME |*/
                      VALID_DATA_SIZE |
                      VALID_DATA_TYPE |
                      VALID_DATA_DATE |
                      VALID_DATA_TIME |
                      VALID_DATA_ATTRIBUTES |
                      VALID_DATA_HIDDEN |
                      VALID_DATA_ISLINK |
                      VALID_DATA_ISOFFLINE |
                      VALID_DATA_ICONOVERLAY);

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    while (find != INVALID_HANDLE_VALUE)
    {
        if (data.cFileName[0] != 0 && strcmp(data.cFileName, ".") != 0 && // "." nezpracovavame
            (!isRootPath || strcmp(data.cFileName, "..") != 0))           // ".." v rootu nezpracovavame
        {
            file.Name = SalamanderGeneral->DupStr(data.cFileName);
            if (file.Name == NULL)
                goto ERR_2;
            file.NameLen = strlen(file.Name);
            if (!sortByExtDirsAsFiles && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                file.Ext = file.Name + file.NameLen; // adresare nemaji pripony
            }
            else
            {
                char* s;
                s = strrchr(file.Name, '.');
                if (s != NULL)
                    file.Ext = s + 1; // ".cvspass" ve Windows je pripona ...
                else
                    file.Ext = file.Name + file.NameLen;
            }
            file.Size = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
            file.Attr = data.dwFileAttributes;
            file.LastWrite = data.ftLastWriteTime;
            file.Hidden = file.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            // IconOverlayIndex nastavujeme vzdy, maximalne ho Salamander vyignoruje
            // read-only = slow file icon-overlay, system = shared icon-overlay
            file.IconOverlayIndex = file.Attr & FILE_ATTRIBUTE_READONLY ? 1 : file.Attr & FILE_ATTRIBUTE_SYSTEM ? 0
                                                                                                                : ICONOVERLAYINDEX_NOTUSED;

            SHFILEINFO shfi;
            lstrcpyn(name, file.Name, MAX_PATH + 4 - (int)(name - curPath));
            BOOL isUpDir;
            isUpDir = strcmp(file.Name, "..") == 0;
            if (!isUpDir)
            {
                if (!SHGetFileInfo(curPath, 0, &shfi, sizeof(shfi), SHGFI_TYPENAME))
                {
                    strcpy(shfi.szTypeName, "(error)");
                }
            }
            else
                strcpy(shfi.szTypeName, "Go to Upper Directory");

            CFSData* extData;
            extData = new CFSData(data.ftCreationTime, data.ftLastAccessTime, shfi.szTypeName);
            if (extData == NULL || !extData->IsGood())
                goto ERR_1;
            file.PluginData = (DWORD_PTR)extData;

            /*      if (data.cAlternateFileName[0] != 0)
      {
        file.DosName = SalamanderGeneral->DupStr(data.cAlternateFileName);
        if (file.DosName == NULL) goto ERR_1;
      }
      else */
            file.DosName = NULL;

            if (file.Attr & FILE_ATTRIBUTE_DIRECTORY)
                file.IsLink = 0; // zjednodusime situaci (ignorujeme volume mount pointy a junction pointy)
            else
                file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);

            file.IsOffline = !isUpDir && (file.Attr & FILE_ATTRIBUTE_OFFLINE) ? 1 : 0;

            if ((file.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && !dir->AddFile(NULL, file, pluginData) ||
                (file.Attr & FILE_ATTRIBUTE_DIRECTORY) != 0 && !dir->AddDir(NULL, file, pluginData))
            {
                if (file.DosName != NULL)
                    SalamanderGeneral->Free(file.DosName);
            ERR_1:
                if (extData != NULL)
                    delete extData;
                SalamanderGeneral->Free(file.Name);
            ERR_2:
                TRACE_E("Low memory");
                dir->Clear(pluginData);
                HANDLES(FindClose(find));
                FatalError = TRUE;
            ERR_3:
                delete pluginData;
                return FALSE;
            }
        }

        /*
    //j.r.
    // kazde jmeno je vlozeno az trikrat s ruznym CASEm jednotlivych znaku
    static int cntr = 0;
    if (cntr > 1)
      cntr = 0;
    else
    {
      if (data.cFileName[cntr] != 0 && strcmp(data.cFileName, "..") != 0)
      {
        if (isupper(data.cFileName[cntr]))
          data.cFileName[cntr] = tolower(data.cFileName[cntr]);
        else
          data.cFileName[cntr] = toupper(data.cFileName[cntr]);
        cntr++;
        continue;
      }
      else
        cntr = 0;
    }
    //end j.r.
*/
        if (!FindNextFile(find, &data))
        {
            HANDLES(FindClose(find));
            break; // konec hledani
        }
    }

    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    if (CalledFromDisconnectDialog)
    {
        detach = FALSE; // kazdopadne FS chceme "zavrit"
        return TRUE;
    }
    if (!forceClose)
    {
        if (canDetach) // close+detach
        {
            int r = SalamanderGeneral->ShowMessageBox("What should TryCloseOrDetach return (Yes==close, No==detach, Cancel==reject)?",
                                                      "DFS", MSGBOX_EX_QUESTION);
            if (r == IDCANCEL)
                return FALSE;
            detach = r == IDNO;
            return TRUE;
        }
        else // close
        {
#ifdef DEMOPLUG_QUIET
            return TRUE;
#else  // DEMOPLUG_QUIET
            return SalamanderGeneral->ShowMessageBox("What should TryCloseOrDetach return (Yes==close, No==reject)?",
                                                     "DFS", MSGBOX_QUESTION) == IDYES;
#endif // DEMOPLUG_QUIET
        }
    }
    else // force close
    {
#ifndef DEMOPLUG_QUIET

        if (SalamanderGeneral->IsCriticalShutdown())
            return TRUE; // pri critical shutdown se na nic neptame

        SalamanderGeneral->ShowMessageBox("TryCloseOrDetach: FS is forced to close.",
                                          "DFS", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::Event(int event, DWORD param)
{
    char buf[MAX_PATH + 100];
    if (event == FSE_CLOSEORDETACHCANCELED)
    {
        sprintf(buf, "Close or detach of path \"%s\" was canceled (%s).", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    if (event == FSE_OPENED)
    {
        sprintf(buf, "Path \"%s\" was opened in %s panel.", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    if (event == FSE_DETACHED)
    {
        LastDetachedFS = this;

        sprintf(buf, "Path \"%s\" was detached (%s).", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    if (event == FSE_ATTACHED)
    {
        if (this == LastDetachedFS)
            LastDetachedFS = NULL;

        sprintf(buf, "Path \"%s\" was attached (%s).", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    if (event == FSE_ACTIVATEREFRESH) // uzivatel aktivoval Salamandera (prepnuti z jine aplikace)
    {
        // provedeme refresh cesty;
        // jsme uvnitr CPluginFSInterface, takze RefreshPanelPath nelze pouzit
        //    SalamanderGeneral->PostRefreshPanelPath((int)param);
        SalamanderGeneral->PostRefreshPanelFS(this);

        sprintf(buf, "Activate refresh on path \"%s\" (%s).", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    /*  // jen test prijmu udalosti "timer" po timeoutu timeru
  if (event == FSE_TIMER)
  {
    TRACE_I("CPluginFSInterface::Event(): timer event " << param);
    if (param == 1234)
    {
      SalamanderGeneral->AddPluginFSTimer(2000, this, 123456);
    }
    if (param == 123456)
    {
      SalamanderGeneral->AddPluginFSTimer(2000, this, 123452);
      SalamanderGeneral->AddPluginFSTimer(2000, this, 123452);
      SalamanderGeneral->AddPluginFSTimer(1500, this, 1234);
    }
  }
*/
}

DWORD WINAPI
CPluginFSInterface::GetSupportedServices()
{
    return FS_SERVICE_CONTEXTMENU |
           FS_SERVICE_SHOWPROPERTIES |
           FS_SERVICE_CHANGEATTRS |
           FS_SERVICE_COPYFROMDISKTOFS |
           FS_SERVICE_MOVEFROMDISKTOFS |
           FS_SERVICE_MOVEFROMFS |
           FS_SERVICE_COPYFROMFS |
           FS_SERVICE_DELETE |
           FS_SERVICE_VIEWFILE |
           FS_SERVICE_CREATEDIR |
           FS_SERVICE_ACCEPTSCHANGENOTIF |
           FS_SERVICE_QUICKRENAME |
           FS_SERVICE_COMMANDLINE |
           FS_SERVICE_SHOWINFO |
           FS_SERVICE_GETFREESPACE |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH |
           FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM |
           FS_SERVICE_GETPATHFORMAINWNDTITLE;
}

BOOL WINAPI
CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char* fsName, char*& title, HICON& icon, BOOL& destroyIcon)
{
    char txt[2 * MAX_PATH + 102];
    // text bude cesta na FS (v Salamanderovskem formatu)
    txt[0] = '\t';
    strcpy(txt + 1, fsName);
    sprintf(txt + strlen(txt), ":%s\t", Path);
    // zdvojime pripadne '&', aby se tisk cesty provedl korektne
    SalamanderGeneral->DuplicateAmpersands(txt, 2 * MAX_PATH + 102);
    // pripojime informaci o volnem miste
    CQuadWord space;
    SalamanderGeneral->GetDiskFreeSpace(&space, Path, NULL);
    if (space != CQuadWord(-1, -1))
        SalamanderGeneral->PrintDiskSize(txt + strlen(txt), space, 0);
    title = SalamanderGeneral->DupStr(txt);
    if (title == NULL)
        return FALSE; // low-memory, zadna polozka nebude

    SalamanderGeneral->GetRootPath(txt, Path);

    if (!SalamanderGeneral->GetFileIcon(txt, FALSE, &icon, SALICONSIZE_16, TRUE, TRUE))
        icon = NULL;
    //Presli jsme na vlastni implementaci (mensi pametova narocnost, fungujici XOR ikonky)
    //SHFILEINFO shi;
    //if (SHGetFileInfo(txt, 0, &shi, sizeof(shi),
    //                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE))
    //{
    //  icon = shi.hIcon;  // ikona se podarila ziskat
    //}
    //else icon = NULL;  // neni ikona
    destroyIcon = TRUE;
    return TRUE;
}

HICON WINAPI
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    char root[MAX_PATH];
    SalamanderGeneral->GetRootPath(root, Path);

    HICON icon;
    if (!SalamanderGeneral->GetFileIcon(root, FALSE, &icon, SALICONSIZE_16, TRUE, TRUE))
        icon = NULL;
    //Presli jsme na vlastni implementaci (mensi pametova narocnost, fungujici XOR ikonky)
    //SHFILEINFO shi;
    //if (SHGetFileInfo(root, 0, &shi, sizeof(shi),
    //                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE))
    //{
    //  icon = shi.hIcon;  // ikona se podarila ziskat
    //}
    //else icon = NULL;  // neni ikona (pouzije se standardni)
    destroyIcon = TRUE;
    return icon;
}

void WINAPI
CPluginFSInterface::GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                  DWORD allowedEffects, DWORD keyState, DWORD* dropEffect)
{                                                                                       // je-li na vyber mezi Copy a Move, rozhodneme pro Move pokud obe FS jsou DFS a cesty maji shodne rooty
    if ((*dropEffect & DROPEFFECT_MOVE) && *dropEffect != DROPEFFECT_MOVE &&            // jinak nema smysl nic zjistovat
        SalamanderGeneral->StrNICmp(srcFSPath, AssignedFSName, AssignedFSNameLen) == 0) // zajimaji nas jen cesty na nasem FS
    {
        const char* src = srcFSPath + AssignedFSNameLen + 1;
        const char* tgt = tgtFSPath + AssignedFSNameLen + 1;
        if (SalamanderGeneral->HasTheSameRootPath(src, tgt))
            *dropEffect = DROPEFFECT_MOVE;
    }
}

void WINAPI
CPluginFSInterface::GetFSFreeSpace(CQuadWord* retValue)
{
    if (Path[0] == 0)
        *retValue = CQuadWord(-1, -1);
    else
        SalamanderGeneral->GetDiskFreeSpace(retValue, Path, NULL);
}

BOOL WINAPI
CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    const char* root = text; // ukazatel za root cestu
    while (*root != 0 && *root != ':')
        root++;
    if (*root == ':')
    {
        root++;
        if (*root == '\\') // UNC cesta
        {
            root++;
            int c = 3;
            while (*root != 0)
            {
                if (*root == '\\' && --c == 0)
                    break;
                root++;
            }
        }
        else // normalni cesta
        {
            int c = 3;
            while (*++root != 0 && --c)
                ;
        }
    }
    const char* s = text + offset;
    const char* end = text + pathLen;
    if (s >= end)
        return FALSE;
    if (s < root)
        offset = (int)(root - text);
    else
    {
        if (*s == '\\')
            s++;
        while (s < end && *s != '\\')
            s++;
        offset = (int)(s - text);
    }
    return s < end;
}

void WINAPI
CPluginFSInterface::ShowInfoDialog(const char* fsName, HWND parent)
{
    CQuadWord f;
    GetFSFreeSpace(&f);
    char num[100];
    if (f != CQuadWord(-1, -1))
        SalamanderGeneral->PrintDiskSize(num, f, 1);
    else
        strcpy(num, "(unknown)");

    char buf[1000];
    sprintf(buf, "Path: %s:%s\nFree Space: %s", fsName, Path, num);
    SalamanderGeneral->SalMessageBox(parent, buf, "DFS Info", MB_OK | MB_ICONINFORMATION);
}

BOOL WINAPI
CPluginFSInterface::ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
{
    SalamanderGeneral->SalMessageBox(parent, command, "DFS Command", MB_OK | MB_ICONINFORMATION);
    command[0] = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                char* newName, BOOL& cancel)
{
    // pokud by si plugin oteviral dialog sam, mel by pouzit CSalamanderGeneralAbstract::AlterFileName
    // ('format' podle SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dialog

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "From: %s\nTo: %s", file.Name, newName);
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Quick Rename", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    // zkontrolujeme zadane jmeno (syntakticky)
    char* s = newName;
    char buf[2 * MAX_PATH];
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (newName[0] == 0 || *s != 0)
    {
        SalamanderGeneral->GetErrorText(ERROR_INVALID_NAME, buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // chybne jmeno, nechame opravit
    }

    // zpracovani masky v newName
    SalamanderGeneral->MaskName(buf, 2 * MAX_PATH, file.Name, newName);
    lstrcpyn(newName, buf, MAX_PATH);

    // provedeni operace prejmenovani
    char nameFrom[MAX_PATH];
    char nameTo[MAX_PATH];
    strcpy(nameFrom, Path);
    strcpy(nameTo, Path);
    if (!SalamanderGeneral->SalPathAppend(nameFrom, file.Name, MAX_PATH) ||
        !SalamanderGeneral->SalPathAppend(nameTo, newName, MAX_PATH))
    {
        SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                         "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (maskovani)
        return FALSE; // chyba -> std. dialog znovu
    }
    if (!MoveFile(nameFrom, nameTo))
    {
        // (pripadny prepis souboru tady neresime, oznacime ho taky za chybu...)
        SalamanderGeneral->GetErrorText(GetLastError(), buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (maskovani)
        return FALSE; // chyba -> std. dialog znovu
    }
    else // operace probehla dobre - hlaseni zmeny na ceste (vyvola refresh) + vracime uspech
    {
        if (SalamanderGeneral->StrICmp(nameFrom, nameTo) != 0)
        { // pokud nejde jen o zmenu velikosti pismen (DFS neni case-sensitive)
            // odstranime z disk-cache zdroj operace (puvodni jmeno jiz neni platne)
            char dfsFileName[2 * MAX_PATH];
            sprintf(dfsFileName, "%s:%s", fsName, nameFrom);
            // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
            // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
            SalamanderGeneral->ToLowerCase(dfsFileName);
            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
            // pokud je mozny i prepis souboru, mel by se z disk-cache odstranit i cil operace (nastala "zmena souboru")
        }

        // zmena na ceste Path (bez podadresaru jen u prejmenovani souboru)
        // POZOR: u bezneho pluginu by se tady mela posilat plna cesta na FS
        SalamanderGeneral->PostChangeOnPathNotification(Path, isDir);

        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{
#ifndef DEMOPLUG_QUIET
    char buf[MAX_PATH + 100];
    sprintf(buf, "Path: %s\nSubdirs: %s", path, includingSubdirs ? "yes" : "no");
    SalamanderGeneral->ShowMessageBox(buf, "DFS Change On Path Notification", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET

    // POZOR: u bezneho pluginu by se tady melo pracovat s cestami na FS
    // u DFS si to zjednodusime na praci s diskovymi cestami, protoze DFS
    // jen zpristupnuje diskovou cestu, priklad implementace pro WMobile nize

    // otestujeme shodnost cest nebo aspon jejich prefixu (zajimaji nas jen diskove cesty,
    // FS cesty v 'path' se vylouci automaticky, protoze se nemuzou nikdy shodovat s Path)
    char path1[MAX_PATH];
    char path2[MAX_PATH];
    lstrcpyn(path1, path, MAX_PATH);
    lstrcpyn(path2, Path, MAX_PATH);
    SalamanderGeneral->SalPathRemoveBackslash(path1);
    SalamanderGeneral->SalPathRemoveBackslash(path2);
    int len1 = (int)strlen(path1);
    BOOL refresh = !includingSubdirs && SalamanderGeneral->StrICmp(path1, path2) == 0 ||       // presna shoda
                   includingSubdirs && SalamanderGeneral->StrNICmp(path1, path2, len1) == 0 && // shoda prefixu
                       (path2[len1] == 0 || path2[len1] == '\\');
    if (!refresh && SalamanderGeneral->CutDirectory(path1))
    {
        SalamanderGeneral->SalPathRemoveBackslash(path1);
        // na NTFS se meni i datum posledniho podadresare v ceste (projevi se az po vstupu
        // do tohoto podadresare, treba to casem opravi)
        refresh = SalamanderGeneral->StrICmp(path1, path2) == 0;
    }
    if (refresh)
    {
        SalamanderGeneral->PostRefreshPanelFS(this); // pokud je FS v panelu, provedeme jeho refresh
    }

    // priklad implementace z pluginu WMobile:
    /*
  // otestujeme shodnost cest nebo aspon jejich prefixu (sanci maji jen cesty
  // na nas FS, diskove cesty a cesty na jine FS v 'path' se vylouci automaticky,
  // protoze se nemuzou nikdy shodovat s 'fsName'+':' na zacatku 'path2' nize)
  char path1[2 * MAX_PATH];
  char path2[2 * MAX_PATH];
  lstrcpyn(path1, path, 2 * MAX_PATH);
  sprintf(path2, "%s:%s", fsName, Path);
  SalamanderGeneral->SalPathRemoveBackslash(path1);
  SalamanderGeneral->SalPathRemoveBackslash(path2);
  int len1 = (int)strlen(path1);
  BOOL refresh = SalamanderGeneral->StrNICmp(path1, path2, len1) == 0 &&
                 (path2[len1] == 0 || includingSubdirs && path2[len1] == '\\');
  if (refresh)
    SalamanderGeneral->PostRefreshPanelFS(this);   // pokud je FS v panelu, provedeme jeho refresh
*/
}

BOOL WINAPI
CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dialog

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "New directory: %s", newName);
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Create Directory", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    SalamanderGeneral->SalUpdateDefaultDir(TRUE); // update pred pouzitim SalParsePath (pouziva interne SalGetFullName)

    char buf[MAX_PATH];
    int type;
    BOOL isDir;
    char* secondPart;
    char nextFocus[MAX_PATH];
    int error;
    nextFocus[0] = 0;
    if (!SalamanderGeneral->SalParsePath(parent, newName, type, isDir, secondPart,
                                         "DFS Create Directory Error", nextFocus,
                                         FALSE, NULL, NULL, &error, 2 * MAX_PATH))
    {
        if (error == SPP_EMPTYPATHNOTALLOWED) // prazdny retezec -> koncime bez provedeni operace
        {
            cancel = TRUE;
            return TRUE; // na navratove hodnote uz nezalezi
        }

        if (error == SPP_INCOMLETEPATH) // relativni cesta na FS, postavime absolutni cestu vlastnimi silami
        {
            // pro diskove cesty by bylo vyhodne pouzit SalGetFullName:
            // SalamanderGeneral->SalGetFullName(newName, &errTextID, Path, nextFocus) + osetrit chyby
            // pak uz by stacilo jen vlozit fs-name pred ziskanou cestu
            // tady si ale radsi predvedeme vlastni implementaci (pouziti SalRemovePointsFromPath a dalsich):

            nextFocus[0] = 0;
            char* s = strchr(newName, '\\');
            if (s == NULL || *(s + 1) == 0)
            {
                int l;
                if (s != NULL)
                    l = (int)(s - newName);
                else
                    l = (int)strlen(newName);
                if (l > MAX_PATH - 1)
                    l = MAX_PATH - 1;
                memcpy(nextFocus, newName, l);
                nextFocus[l] = 0;
            }

            char path[2 * MAX_PATH];
            strcpy(path, fsName);
            s = path + strlen(path);
            *s++ = ':';
            char* userPart = s;
            BOOL tooLong = FALSE;
            int rootLen = SalamanderGeneral->GetRootPath(s, Path);
            if (newName[0] == '\\') // "\\path" -> skladame root + newName
            {
                s += rootLen;
                int len = (int)strlen(newName + 1); // bez uvodniho '\\'
                if (len + rootLen >= MAX_PATH)
                    tooLong = TRUE;
                else
                {
                    memcpy(s, newName + 1, len);
                    *(s + len) = 0;
                }
            }
            else // "path" -> skladame Path + newName
            {
                int pathLen = (int)strlen(Path);
                if (pathLen < rootLen)
                    rootLen = pathLen;
                strcpy(s + rootLen, Path + rootLen); // root uz je tam nakopirovany
                tooLong = !SalamanderGeneral->SalPathAppend(s, newName, MAX_PATH);
            }

            if (tooLong)
            {
                SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                                 "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
                // 'newName' se vraci v puvodni verzi
                return FALSE; // chyba -> std. dialog znovu
            }

            strcpy(newName, path);
            secondPart = newName + (userPart - path);
            type = PATH_TYPE_FS;
        }
        else
            return FALSE; // chyba -> std. dialog znovu
    }

    if (type != PATH_TYPE_FS)
    {
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory on disk or archive path.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    if ((secondPart - newName) - 1 != (int)strlen(fsName) ||
        SalamanderGeneral->StrNICmp(newName, fsName, (int)(secondPart - newName) - 1) != 0)
    { // to neni DFS
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory on specified file-system.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    if (!SalamanderGeneral->HasTheSameRootPath(Path, secondPart))
    { // DFS maka jen v ramci jednoho disku (napr. u FTP se bude muset take kontrolovat
        // jestli uzivatel nechce vytvorit adresar na jinem serveru)
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory outside of currently opened drive.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }

    // v plne ceste na toto FS mohl uzivatel take pouzit "." a ".." - odstranime je
    int rootLen = SalamanderGeneral->GetRootPath(buf, secondPart);
    int secPartLen = (int)strlen(secondPart);
    if (secPartLen < rootLen)
        rootLen = secPartLen;
    if (!SalamanderGeneral->SalRemovePointsFromPath(secondPart + rootLen))
    {
        SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' se vraci po uprave (expanzi cesty) + mozne uprave nekterych ".." a "."
        return FALSE; // chyba -> std. dialog znovu
    }

    // orizneme zbytecny backslash
    int l = (int)strlen(secondPart);   // pocita se znovu, protoze SalRemovePointsFromPath mohla cestu zmenit
    if (l > 1 && secondPart[1] == ':') // typ cesty "c:\path"
    {
        if (l > 3) // neni root cesta
        {
            if (secondPart[l - 1] == '\\')
                secondPart[l - 1] = 0; // orez backslashe
        }
        else
        {
            secondPart[2] = '\\'; // root cesta, backslash nutny ("c:\")
            secondPart[3] = 0;
        }
    }
    else // UNC cesta
    {
        if (l > 0 && secondPart[l - 1] == '\\')
            secondPart[l - 1] = 0; // orez backslashe
    }

    // konecne vytvorime adresar
    DWORD err;
    if (!SalamanderGeneral->SalCreateDirectoryEx(secondPart, &err))
    {
        SalamanderGeneral->GetErrorText(err, buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Create Directory Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' uz se vraci po uprave (expanzi cesty)
        return FALSE; // chyba -> std. dialog znovu
    }
    else // operace probehla dobre - hlaseni zmeny na ceste (vyvola refresh) + vracime uspech
    {
        // zmena na ceste (bez podadresaru)
        SalamanderGeneral->CutDirectory(secondPart); // musi jit (nemuze byt root)
        // POZOR: u bezneho pluginu by se tady mela posilat plna cesta na FS
        SalamanderGeneral->PostChangeOnPathNotification(secondPart, FALSE);
        strcpy(newName, nextFocus); // pokud byl zadan primo jen nazev adresare, vyfokusime ho v panelu
        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
    // sestavime unikatni jmeno souboru pro disk-cache (standardni salamanderovsky format cesty)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, fsName);
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, Path);
    SalamanderGeneral->SalPathAppend(uniqueFileName + strlen(fsName) + 1, file.Name, MAX_PATH);
    // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
    // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
    SalamanderGeneral->ToLowerCase(uniqueFileName);

    // ziskame jmeno kopie souboru v disk-cache
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, file.Name, NULL, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // zjistime jestli je treba pripravit kopii souboru do disk-cache (download)
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // priprava kopie souboru (download) je nutna
    {
        const char* name = uniqueFileName + strlen(fsName) + 1;
        if (CopyFile(name, tmpFileName, TRUE)) // kopie se povedla
        {
            newFileOK = TRUE; // pokud nevyjde zjistovani velikosti souboru, zustane newFileSize nula (neni az tak dulezite)
            HANDLE hFile = HANDLES_Q(CreateFile(tmpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL, OPEN_EXISTING, 0, NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            { // chybu ignorujeme, velikost souboru neni az tak dulezita
                DWORD err;
                SalamanderGeneral->SalGetFileSize(hFile, newFileSize, err); // chyby ignorujeme
                HANDLES(CloseHandle(hFile));
            }
        }
        else // chyba kopirovani (downloadu)
        {
            DWORD err = GetLastError();
            char errorText[3 * MAX_PATH + 100];
            sprintf(errorText, "Unable to download file %s to disk file %s.\nError: ",
                    uniqueFileName, tmpFileName);
            SalamanderGeneral->GetErrorText(err, errorText + strlen(errorText), MAX_PATH);
            SalamanderGeneral->SalMessageBox(parent, errorText, "DFS Error", MB_OK | MB_ICONEXCLAMATION);
        }
    }

    // otevreme viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileOK || // viewer otevreme jen pokud je kopie souboru v poradku
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // pri chybe nulujeme "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // jeste musime zavolat FreeFileNameInCache do paru k AllocFileNameInCache (propojime
    // viewer a disk-cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, FALSE /* po zavreni vieweru se hned nesmazne */);
}

BOOL WINAPI
CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                           int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    // pokud by si plugin oteviral dialog sam, mel by pouzit CSalamanderGeneralAbstract::AlterFileName
    // ('format' podle SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    cancelOrError = FALSE;
    if (mode == 1)
        return FALSE; // zadost o standardni dotaz (je-li SALCFG_CNFRMFILEDIRDEL TRUE) - napoveda k sestaveni textu dotazu viz CPluginFSInterface::CopyOrMoveFromFS

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "Delete %d files and %d directories from %s panel.",
            selectedFiles, selectedDirs, (panel == PANEL_LEFT ? "left" : "right"));
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Delete", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    /*
  // ukazka pouziti wait-okenka - potreba napr. pri nacitani jmen, ktera se maji smazat
  // (priprava pro celkovy progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // najdeme hl. okno parenta (muze byt hl. okno Salamandera)
    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'
    EnableWindow(mainWnd, FALSE);

    BOOL retSuccess = FALSE;
    BOOL enableMainWnd = TRUE;
    CDeleteProgressDlg delDlg(mainWnd, ooStatic); // aby nemodalni dialog mohl byt na stacku, dame 'ooStatic'
    if (delDlg.Create() != NULL)                  // dialog se podarilo otevrit
    {
        SetForegroundWindow(delDlg.HWindow);

        delDlg.Set("reading directory tree...", 0, FALSE);
        Sleep(1500); // simulace cinnosti
        delDlg.Set("preparing data...", 0, FALSE);
        Sleep(500); // simulace cinnosti

        int i;
        for (i = 0; i <= 1000; i++)
        {
            if (delDlg.GetWantCancel())
            {
                delDlg.Set("canceling operation...", i, FALSE);
                Sleep(500); // simulace "cancel" cinnosti
                break;
            }

            char buf[100];
            sprintf(buf, "filename_%d.test", i);
            delDlg.Set(buf, i, TRUE); // delayedPaint == TRUE, abychom nebrzdili

            Sleep(20); // simulace cinnosti
        }
        retSuccess = (i > 1000);

        // enablujeme 'mainWnd' (jinak ho nemuze Windows vybrat jako foreground/aktivni okno)
        EnableWindow(mainWnd, TRUE);
        enableMainWnd = FALSE;

        DestroyWindow(delDlg.HWindow); // zavreme progress dialog
    }

    if (enableMainWnd)
    { // enablujeme 'mainWnd' (nenastala zmena foreground okna - progress se vubec neotevrel)
        EnableWindow(mainWnd, TRUE);
    }
    return retSuccess; // uspech jen pokud nedal Cancel a podaril se otevrit progress dialog

    /*
  // vyzvedneme hodnoty "Confirm on" z konfigurace
  BOOL ConfirmOnNotEmptyDirDelete, ConfirmOnSystemHiddenFileDelete, ConfirmOnSystemHiddenDirDelete;
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, 4, NULL);
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEDEL, &ConfirmOnSystemHiddenFileDelete, 4, NULL);
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHDIRDEL, &ConfirmOnSystemHiddenDirDelete, 4, NULL);

  char buf[2 * MAX_PATH];  // buffer pro texty chyb

  char fileName[MAX_PATH];   // buffer pro plne jmeno
  strcpy(fileName, Path);
  char *end = fileName + strlen(fileName);  // misto pro jmena z panelu
  if (end > fileName && *(end - 1) != '\\')
  {
    *end++ = '\\';
    *end = 0;
  }
  int endSize = MAX_PATH - (end - fileName);  // max. pocet znaku pro jmeno z panelu

  char dfsFileName[2 * MAX_PATH];   // buffer pro plne jmeno na DFS
  sprintf(dfsFileName, "%s:%s", fsName, fileName);
  char *endDFSName = dfsFileName + strlen(dfsFileName);  // misto pro jmena z panelu
  int endDFSNameSize = 2 * MAX_PATH - (endDFSName - dfsFileName); // max. pocet znaku pro jmeno z panelu

  const CFileData *f = NULL;  // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
  BOOL isDir = FALSE;         // TRUE pokud 'f' je adresar
  BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
  int index = 0;
  BOOL success = TRUE;        // FALSE v pripade chyby nebo preruseni uzivatelem
  BOOL skipAllSHFD = FALSE;   // skip all deletes of system or hidden files
  BOOL yesAllSHFD = FALSE;    // delete all system or hidden files
  BOOL skipAllSHDD = FALSE;   // skip all deletes of system or hidden dirs
  BOOL yesAllSHDD = FALSE;    // delete all system or hidden dirs
  BOOL skipAllErrors = FALSE; // skip all errors
  BOOL changeInSubdirs = FALSE;
  while (1)
  {
    // vyzvedneme data o zpracovavanem souboru
    if (focused) f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
    else f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

    // smazeme soubor/adresar
    if (f != NULL)
    {
      // sestaveni plnych jmen jmena, orez na MAX_PATH (2 * MAX_PATH) je teoreticky zbytecny,
      // prakticky bohuzel ne
      lstrcpyn(end, f->Name, endSize);
      lstrcpyn(endDFSName, f->Name, endDFSNameSize);

      if (isDir)
      {
        BOOL skip = FALSE;
        if (ConfirmOnSystemHiddenDirDelete &&
            (f->Attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
        {
          if (!skipAllSHDD && !yesAllSHDD)
          {
            int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, dfsFileName,
                                                        "Do you want to delete the directory with "
                                                        "SYSTEM or HIDDEN attribute?",
                                                        "Confirm Directory Delete");
            switch (res)
            {
              case DIALOG_ALL: yesAllSHDD = TRUE;
              case DIALOG_YES: break;

              case DIALOG_SKIPALL: skipAllSHDD = TRUE;
              case DIALOG_SKIP: skip = TRUE; break;

              default: success = FALSE; break; // DIALOG_CANCEL
            }
          }
          else  // skip all or delete all
          {
            if (skipAllSHDD) skip = TRUE;
          }
        }

        if (success && !skip)   // neni cancel ani skip
        {

          // zde by se melo resit ConfirmOnNotEmptyDirDelete + rekurzivni delete,
          // zaroven by se tu mel resit progress (pridani za smazani/skipnuti souboru/adresaru)
          // pro smazane soubory by se melo volat SalamanderGeneral->RemoveOneFileFromCache();

          changeInSubdirs = TRUE;   // zmeny mohly nastat i v podadresarich
        }
      }
      else
      {
        BOOL skip = FALSE;
        if (ConfirmOnSystemHiddenFileDelete &&
            (f->Attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
        {
          if (!skipAllSHFD && !yesAllSHFD)
          {
            int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, dfsFileName,
                                                        "Do you want to delete the file with "
                                                        "SYSTEM or HIDDEN attribute?",
                                                        "Confirm File Delete");
            switch (res)
            {
              case DIALOG_ALL: yesAllSHFD = TRUE;
              case DIALOG_YES: break;

              case DIALOG_SKIPALL: skipAllSHFD = TRUE;
              case DIALOG_SKIP: skip = TRUE; break;

              default: success = FALSE; break; // DIALOG_CANCEL
            }
          }
          else  // skip all or delete all
          {
            if (skipAllSHFD) skip = TRUE;
          }
        }

        if (success && !skip)   // neni cancel ani skip
        {
          BOOL skip = FALSE;
          while (1)
          {
            SalamanderGeneral->ClearReadOnlyAttr(fileName, f->Attr);  // aby sel smazat i read-only
            if (!DeleteFile(fileName))
            {
              if (!skipAllErrors)
              {
                SalamanderGeneral->GetErrorText(GetLastError(), buf, 2 * MAX_PATH);
                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, dfsFileName, buf, "DFS Delete Error");
                switch (res)
                {
                  case DIALOG_RETRY: break;

                  case DIALOG_SKIPALL: skipAllErrors = TRUE;
                  case DIALOG_SKIP: skip = TRUE; break;

                  default: success = FALSE; break; // DIALOG_CANCEL
                }
              }
              else skip = TRUE;
            }
            else
            {
              // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
              // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
              SalamanderGeneral->ToLowerCase(dfsFileName);
              // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
              SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
              break;   // uspesny delete
            }
            if (!success || skip) break;
          }

          if (success)
          {

            // zde by se mel resit progress (pridani za smazani/skipnuti jednoho souboru)

          }
        }
      }
    }

    // zjistime jestli ma cenu pokracovat (pokud neni chyba a existuje jeste nejaka dalsi oznacena polozka)
    if (!success || focused || f == NULL) break;
  }

  // zmena na ceste Path (bez podadresaru jen pokud se mazaly jen soubory)
  // POZOR: u bezneho pluginu by se tady mela posilat plna cesta na FS
  SalamanderGeneral->PostChangeOnPathNotification(Path, changeInSubdirs);
  return success;
*/
}

BOOL WINAPI DFS_IsTheSamePath(const char* path1, const char* path2)
{
    while (*path1 != 0 && LowerCase[*path1] == LowerCase[*path2])
    {
        path1++;
        path2++;
    }
    if (*path1 == '\\')
        path1++;
    if (*path2 == '\\')
        path2++;
    return *path1 == 0 && *path2 == 0;
}

enum CDFSPathError
{
    dfspeNone,
    dfspeServerNameMissing,
    dfspeShareNameMissing,
    dfspeRelativePath, // relativni cesty nejsou podporovany ("PATH", "\PATH", ani "C:PATH")
};

BOOL DFS_IsValidPath(const char* path, CDFSPathError* err)
{
    const char* s = path;
    if (err != NULL)
        *err = dfspeNone;
    if (*s == '\\' && *(s + 1) == '\\') // UNC (\\server\share\...)
    {
        s += 2;
        if (*s == 0 || *s == '\\')
        {
            if (err != NULL)
                *err = dfspeServerNameMissing;
        }
        else
        {
            while (*s != 0 && *s != '\\')
                s++; // prejeti servername
            if (*s == '\\')
                s++;
            if (*s == 0 || *s == '\\')
            {
                if (err != NULL)
                    *err = dfspeShareNameMissing;
            }
            else
                return TRUE; // cesta OK
        }
    }
    else // cesta zadana pomoci disku (c:\...)
    {
        if (LowerCase[*s] >= 'a' && LowerCase[*s] <= 'z' && *(s + 1) == ':' && *(s + 2) == '\\') // "c:\..."
        {
            return TRUE; // cesta OK
        }
        else
        {
            if (err != NULL)
                *err = dfspeRelativePath;
        }
    }
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                     int panel, int selectedFiles, int selectedDirs,
                                     char* targetPath, BOOL& operationMask,
                                     BOOL& cancelOrHandlePath, HWND dropTarget)
{
    // pokud by si plugin oteviral dialog sam, mel by pouzit CSalamanderGeneralAbstract::AlterFileName
    // ('format' podle SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    char path[2 * MAX_PATH];
    operationMask = FALSE;
    cancelOrHandlePath = FALSE;
    if (mode == 1) // prvni volani CopyOrMoveFromFS
    {
        /*
    // ukazka sestaveni nadpisu editline s cilem kopirovani
    // (pokud jde o "files" a "directories", lze take jen jednoduse pouzit
    //  SalamanderGeneral->GetCommonFSOperSourceDescr(subjectSrc, MAX_PATH + 100,
    //  panel, selectedFiles, selectedDirs, NULL, FALSE, FALSE) - nahrazuje nasl. cast kodu sestavujici subjectSrc)
    char subjectSrc[MAX_PATH + 100];
    if (selectedFiles + selectedDirs <= 1)  // jedna oznacena polozka nebo fokus
    {
      BOOL isDir;
      const CFileData *f;
      if (selectedFiles == 0 && selectedDirs == 0)
        f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
      else
      {
        int index = 0;
        f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);
      }
      int fileNameFormat;
      SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT, &fileNameFormat,
                                            sizeof(fileNameFormat), NULL);
      char formatedFileName[MAX_PATH];  // CFileData::Name je dlouhy max. MAX_PATH-5 znaku - limit dany Salamanderem
      SalamanderGeneral->AlterFileName(formatedFileName, f->Name, fileNameFormat, 0, isDir);
      _snprintf_s(subjectSrc, _TRUNCATE, isDir ? "directory \"%s\"" : "file \"%s\"", formatedFileName);
      subjectSrc[MAX_PATH + 100 - 1] = 0;
    }
    else  // nekolik adresaru a souboru
    {
      SalamanderGeneral->ExpandPluralFilesDirs(subjectSrc, MAX_PATH + 100, selectedFiles,
                                               selectedDirs, epfdmNormal, FALSE);
    }
    char subject[MAX_PATH + 200];
    sprintf(subject, "Copy %s from FS to", subjectSrc);
*/

        // pokud neni navrzena zadna cesta, zkusime jestli je v druhem panelu DFS, pripadne ji navrhneme
        if (*targetPath == 0)
        {
            int targetPanel = (panel == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);
            int type;
            char* fs;
            if (SalamanderGeneral->GetPanelPath(targetPanel, path, 2 * MAX_PATH, &type, &fs))
            {
                if (type == PATH_TYPE_FS && fs - path == (int)strlen(fsName) &&
                    SalamanderGeneral->StrNICmp(path, fsName, (int)(fs - path)) == 0)
                {
                    strcpy(targetPath, path);
                }
            }
        }
        // pokud je nejaka cesta navrzena, pridame k ni masku *.* (budeme zpracovavat operacni masky)
        if (*targetPath != 0)
        {
            SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_TARGET); // default akce = prace s cestou v cilovem panelu
        }

        return FALSE; // zadost o standardni dialog
    }

    if (mode == 4) // chyba pri std. Salamanderovskem zpracovani cilove cesty
    {
        // 'targetPath' obsahuje chybnou cestu, uzivatelovi byla chyba ohlasena, zbyva jen
        // ho nechat provest editaci cilove cesty
        return FALSE; // zadost o standardni dialog
    }

    const char* title = copy ? "DFS Copy" : "DFS Move";
    const char* errTitle = copy ? "DFS Copy Error" : "DFS Move Error";

#ifndef DEMOPLUG_QUIET
    if (mode == 2 || mode == 5)
    {
        char bufText[2 * MAX_PATH + 200];
        sprintf(bufText, "%s %d files and %d directories from %s panel to: %s",
                (copy ? "Copy" : "Move"), selectedFiles, selectedDirs,
                (panel == PANEL_LEFT ? "left" : "right"), targetPath);
        SalamanderGeneral->SalMessageBox(parent, bufText, title, MB_OK | MB_ICONINFORMATION);
    }
#endif // DEMOPLUG_QUIET

    char buf[3 * MAX_PATH + 100];
    char errBuf[MAX_PATH];
    char nextFocus[MAX_PATH];
    nextFocus[0] = 0;

    BOOL diskPath = TRUE;  // pri 'mode'==3 je v 'targetPath' windowsova cesta (FALSE = cesta na toto FS)
    char* userPart = NULL; // ukazatel do 'targetPath' na user-part FS cesty (pri 'diskPath' FALSE)
    BOOL rename = FALSE;   // je-li TRUE, jde o prejmenovani/kopirovani adresare do sebe sama

    if (mode == 2) // prisel retezec ze std. dialogu od usera
    {
        // sami zpracujeme relativni cesty (to Salamander nemuze udelat)
        if ((targetPath[0] != '\\' || targetPath[1] != '\\') && // neni UNC cesta
            (targetPath[0] == 0 || targetPath[1] != ':'))       // neni normalni diskova cesta
        {                                                       // neni windowsova cesta, ani archivova cesta
            userPart = strchr(targetPath, ':');
            if (userPart == NULL) // cesta nema fs-name, je tedy relativni
            {                     // relativni cesta s ':' zde neni mozna (nelze rozlisit od plne cesty na nejaky FS)

                // pro diskove cesty by bylo vyhodne pouzit SalGetFullName:
                // SalamanderGeneral->SalGetFullName(targetPath, &errTextID, Path, nextFocus) + osetrit chyby
                // pak uz by stacilo jen vlozit fs-name pred ziskanou cestu
                // tady si ale radsi predvedeme vlastni implementaci (pouziti SalRemovePointsFromPath a dalsich):

                char* s = strchr(targetPath, '\\');
                if (s == NULL || *(s + 1) == 0)
                {
                    int l;
                    if (s != NULL)
                        l = (int)(s - targetPath);
                    else
                        l = (int)strlen(targetPath);
                    if (l > MAX_PATH - 1)
                        l = MAX_PATH - 1;
                    memcpy(nextFocus, targetPath, l);
                    nextFocus[l] = 0;
                }

                strcpy(path, fsName);
                s = path + strlen(path);
                *s++ = ':';
                userPart = s;
                BOOL tooLong = FALSE;
                int rootLen = SalamanderGeneral->GetRootPath(s, Path);
                if (targetPath[0] == '\\') // "\\path" -> skladame root + newName
                {
                    s += rootLen;
                    int len = (int)strlen(targetPath + 1); // bez uvodniho '\\'
                    if (len + rootLen >= MAX_PATH)
                        tooLong = TRUE;
                    else
                    {
                        memcpy(s, targetPath + 1, len);
                        *(s + len) = 0;
                    }
                }
                else // "path" -> skladame Path + newName
                {
                    int pathLen = (int)strlen(Path);
                    if (pathLen < rootLen)
                        rootLen = pathLen;
                    strcpy(s + rootLen, Path + rootLen); // root uz je tam nakopirovany
                    tooLong = !SalamanderGeneral->SalPathAppend(s, targetPath, MAX_PATH);
                }

                if (tooLong)
                {
                    SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' se vraci v nezmenene podobe (co uzivatel zadal)
                    return FALSE; // chyba -> std. dialog znovu
                }

                strcpy(targetPath, path);
                userPart = targetPath + (userPart - path);
            }
            else
                userPart++;

            // FS cilova cesta ('targetPath' - plna cesta, 'userPart' - ukazatel do plne cesty na user-part)
            // na tomto miste muze plugin zpracovat FS cesty (jak sve vlastni, tak cizi)
            // Salamander zatim neumi tyto cesty zpracovat, casem mozna umozni sled zakladnich
            // operaci pres TEMP (napr. download z FTP do TEMPu, pak upload z TEMPu na FTP - pokud
            // lze udelat efektivneji (u FTP to lze), mel by to plugin zvladnout zde)

            if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
            { // je to DFS (jinak nechame zpracovat standardne Salamanderem)
                CDFSPathError err;
                BOOL invPath = !DFS_IsValidPath(userPart, &err);

                // v plne ceste na toto FS mohl uzivatel take pouzit "." a ".." - odstranime je
                int rootLen = 0;
                if (!invPath)
                {
                    rootLen = SalamanderGeneral->GetRootPath(buf, userPart);
                    int userPartLen = (int)strlen(userPart);
                    if (userPartLen < rootLen)
                        rootLen = userPartLen;
                }
                if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
                {
                    // navic by se dalo vypsat 'err' (pri 'invPath' TRUE), zde pro jednoduchost ignorujeme
                    SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' se vraci po uprave (expanzi cesty) + mozne uprave nekterych ".." a "."
                    return FALSE; // chyba -> std. dialog znovu
                }

                // orizneme zbytecny backslash
                int l = (int)strlen(userPart);
                BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
                if (l > 1 && userPart[1] == ':') // typ cesty "c:\path"
                {
                    if (l > 3) // neni root cesta
                    {
                        if (userPart[l - 1] == '\\')
                            userPart[l - 1] = 0; // orez backslashe
                    }
                    else
                    {
                        userPart[2] = '\\'; // root cesta, backslash nutny ("c:\")
                        userPart[3] = 0;
                    }
                }
                else // UNC cesta
                {
                    if (l > 0 && userPart[l - 1] == '\\')
                        userPart[l - 1] = 0; // orez backslashe
                }

                // rozanalyzovani cesty - nalezeni existujici casti, neexistujici casti a operacni masky
                //
                // - zjistit jaka cast cesty existuje a jestli je to soubor nebo adresar,
                //   podle vysledku vybrat o co jde:
                //   - zapis na cestu (prip. s neexistujici casti) s maskou - maska je posledni neexistujici cast cesty,
                //     za kterou jiz neni backslash (overit jestli u vice zdrojovych souboru/adresaru je
                //     v masce '*' nebo aspon '?', jinak nesmysl -> jen jedno cilove jmeno)
                //   - rucni "change-case" jmena podadresare pres Move (zapis na cestu, ktera je zaroven zdroj
                //     operace (je focusena/oznacena-jako-jedina v panelu); jmena se muzou lisit ve velikosti pismen)
                //   - zapis do archivu (v ceste je soubor archivu nebo to ani nemusi byt archiv, pak jde o
                //     chybu "Salamander nevi jak tento soubor otevrit")
                //   - prepis souboru (cela cesta je jen jmeno ciloveho souboru; nesmi koncit na backslash)

                // zjisteni kam az cesta existuje (rozdeleni na existujici a neexistujici cast)
                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                char* end = targetPath + strlen(targetPath);
                char* afterRoot = userPart + rootLen;
                char lastChar = 0;
                BOOL pathIsDir = TRUE;
                BOOL pathError = FALSE;

                // pokud je v ceste maska, odrizneme ji bez volani SalGetFileAttributes
                if (end > afterRoot) // jeste neni jen root
                {
                    char* end2 = end;
                    BOOL cut = FALSE;
                    while (*--end2 != '\\') // je jiste, ze aspon za root-cestou je jeden '\\'
                    {
                        if (*end2 == '*' || *end2 == '?')
                            cut = TRUE;
                    }
                    if (cut) // ve jmene je maska -> orizneme
                    {
                        end = end2;
                        lastChar = *end;
                        *end = 0;
                    }
                }

                while (end > afterRoot) // jeste neni jen root
                {
                    DWORD attrs = SalamanderGeneral->SalGetFileAttributes(userPart);
                    if (attrs != 0xFFFFFFFF) // tato cast cesty existuje
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // je to soubor
                        {
                            // existujici cesta nema obsahovat jmeno souboru (viz SalSplitGeneralPath), orizneme...
                            *end = lastChar;   // opravime 'targetPath'
                            pathIsDir = FALSE; // existujici cast cesty je soubor
                            while (*--end != '\\')
                                ;            // je jiste, ze aspon za root-cestou je jeden '\\'
                            lastChar = *end; // aby se nezrusila cesta
                            break;
                        }
                        else
                            break;
                    }
                    else
                    {
                        DWORD err2 = GetLastError();
                        if (err2 != ERROR_FILE_NOT_FOUND && err2 != ERROR_INVALID_NAME &&
                            err2 != ERROR_PATH_NOT_FOUND && err2 != ERROR_BAD_PATHNAME &&
                            err2 != ERROR_DIRECTORY) // divna chyba - jen vypiseme
                        {
                            sprintf(buf, "Path: %s\nError: %s", targetPath,
                                    SalamanderGeneral->GetErrorText(err2, errBuf, MAX_PATH));
                            SalamanderGeneral->SalMessageBox(parent, buf, errTitle, MB_OK | MB_ICONEXCLAMATION);
                            pathError = TRUE;
                            break; // ohlasime chybu
                        }
                    }

                    *end = lastChar; // obnova 'targetPath'
                    while (*--end != '\\')
                        ; // je jiste, ze aspon za root-cestou je jeden '\\'
                    lastChar = *end;
                    *end = 0;
                }
                *end = lastChar; // opravime 'targetPath'
                SetCursor(oldCur);

                if (!pathError) // rozdeleni probehlo bez chyby
                {
                    if (*end == '\\')
                        end++;

                    const char* dirName = NULL;
                    const char* curPath = NULL;
                    if (selectedFiles + selectedDirs <= 1)
                    {
                        const CFileData* f;
                        if (selectedFiles == 0 && selectedDirs == 0)
                            f = SalamanderGeneral->GetPanelFocusedItem(panel, NULL);
                        else
                        {
                            int index = 0;
                            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, NULL);
                        }
                        dirName = f->Name;

                        sprintf(path, "%s:%s", fsName, Path);
                        curPath = path;
                    }

                    char* mask;
                    char newDirs[MAX_PATH];
                    if (SalamanderGeneral->SalSplitGeneralPath(parent, title, errTitle, selectedFiles + selectedDirs,
                                                               targetPath, afterRoot, end, pathIsDir,
                                                               backslashAtEnd, dirName, curPath, mask, newDirs,
                                                               DFS_IsTheSamePath))
                    {
                        if (newDirs[0] != 0) // na cilove ceste je potreba vytvorit nejake podadresare
                        {
                            // POZN.: pokud neni podpora pro vytvareni podadresaru na cilove ceste, staci dat
                            //        'newDirs'==NULL v SalSplitGeneralPath(), chybu ohlasi uz SalSplitGeneralPath()

                            // POZN.: pokud by se vytvarela cesta, bylo by treba volat PostChangeOnPathNotification
                            //        (zpracuje se az pozdeji, takze nejlepe volat hned po vytvoreni cesty a ne az po
                            //        ukonceni cele operace)

                            SalamanderGeneral->SalMessageBox(parent, "Sorry, but creating of target path is not supported.",
                                                             errTitle, MB_OK | MB_ICONEXCLAMATION);
                            char* e = targetPath + strlen(targetPath); // oprava 'targetPath' (spojeni 'targetPath' a 'mask')
                            if (e > targetPath && *(e - 1) != '\\')
                                *e++ = '\\';
                            if (e != mask)
                                memmove(e, mask, strlen(mask) + 1); // je-li potreba, prisuneme masku
                            pathError = TRUE;
                        }
                        else
                        {
                            if (dirName != NULL && curPath != NULL && SalamanderGeneral->StrICmp(dirName, mask) == 0 &&
                                DFS_IsTheSamePath(targetPath, curPath))
                            {
                                // prejmenovani/kopirovani adresare do sebe sama (az na velikost pismen v nazvu) - "change-case"
                                // nelze povazovat za operacni masku (zadana cilova cesta existuje, rozdeleni na masku je
                                // vysledkem analyzy)

                                rename = TRUE;
                            }

                            /*
              // nasledujici kod osetruje situaci, kdy FS nepodporuje operacni masky
              if (mask != NULL && (strcmp(mask, "*.*") == 0 || strcmp(mask, "*") == 0))
              {  // nepodporuje masky a maska je prazdna, zarizneme ji
                *mask = 0;  // double-null terminated
              }
              if (!rename)  // u rename to neni chyba
              {
                if (mask != NULL && *mask != 0)  // maska existuje, ale neni povolena
                {
                  char *e = targetPath + strlen(targetPath);   // oprava 'targetPath' (spojeni 'targetPath' a 'mask')
                  if (e > targetPath && *(e - 1) != '\\') *e++ = '\\';
                  if (e != mask) memmove(e, mask, strlen(mask) + 1);  // je-li potreba, prisuneme masku

                  SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support operation masks (target "
                                                   "path must exist or end on backslash)", errTitle,
                                                   MB_OK | MB_ICONEXCLAMATION);
                  pathError = TRUE;
                }
              }
*/

                            if (!pathError)
                                diskPath = FALSE; // cestu na toto FS se podarilo rozanalyzovat
                        }
                    }
                    else
                        pathError = TRUE;
                }

                if (pathError)
                {
                    // 'targetPath' se vraci po uprave (expanzi cesty) + uprave ".." a "." + mozna pridane masky
                    return FALSE; // chyba -> std. dialog znovu
                }
            }
        }

        if (diskPath)
        {
            // windowsova cesta, cesta do archivu nebo na neznamy FS - pustime std. zpracovani
            operationMask = TRUE; // operacni masky podporujeme
            cancelOrHandlePath = TRUE;
            return FALSE; // nechame cestu zpracovat v Salamanderovi
        }
    }

    const char* opMask = NULL; // maska operace
    if (mode == 5)             // cil operace byl zadan pres drag&drop
    {
        // pokud jde o diskovou cestu, pak jen nastavime masku operace a pokracujeme (stejne s 'mode'==3);
        // pokud jde o cestu do archivu, vyhodime error "not supported"; pokud jde o cestu do DFS, dame
        // 'diskPath'=FALSE a napocitame 'userPart' (ukazuje na user-part DFS cesty); pokud jde o cestu
        // do jineho FS, vyhodime error "not supported"

        BOOL ok = FALSE;
        opMask = "*.*";
        int type;
        char* secondPart;
        BOOL isDir;
        if (targetPath[0] != 0 && targetPath[1] == ':' ||   // diskova cesta (C:\path)
            targetPath[0] == '\\' && targetPath[1] == '\\') // UNC cesta (\\server\share\path)
        {                                                   // pridame na konec backslash, aby slo k kazdem pripade o cestu (pri 'mode'==5 jde vzdy o cestu)
            SalamanderGeneral->SalPathAddBackslash(targetPath, MAX_PATH);
        }
        if (SalamanderGeneral->SalParsePath(parent, targetPath, type, isDir, secondPart,
                                            errTitle, NULL, FALSE,
                                            NULL, NULL, NULL, 2 * MAX_PATH))
        {
            switch (type)
            {
            case PATH_TYPE_WINDOWS:
            {
                if (*secondPart != 0)
                {
                    SalamanderGeneral->SalMessageBox(parent, "Target path doesn't exist. DFS doesn't support creating of target path.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                }
                else
                    ok = TRUE;
                break;
            }

            case PATH_TYPE_FS:
            {
                userPart = secondPart;
                if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                    SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
                { // je to DFS
                    diskPath = FALSE;
                    ok = TRUE;
                }
                else // jine FS, jen ohlasime "not supported"
                {
                    SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support copying nor moving to other "
                                                             "plugin file-systems.",
                                                     errTitle,
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
                break;
            }

            //case PATH_TYPE_ARCHIVE:
            default: // archiv, jen ohlasime "not supported"
            {
                SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support copying nor moving to archives.",
                                                 errTitle, MB_OK | MB_ICONEXCLAMATION);
                break;
            }
            }
        }
        if (!ok)
        {
            cancelOrHandlePath = TRUE;
            return TRUE;
        }
    }

    // 'mode' je 2, 3 nebo 5

    /*
  // ukazka pouziti wait-okenka - potreba napr. pri nacitani jmen, ktera se maji kopirovat
  // (priprava pro celkovy progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // vyzvedneme hodnoty "Confirm on" z konfigurace
    BOOL ConfirmOnFileOverwrite, ConfirmOnDirOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMDIROVER, &ConfirmOnDirOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);
    // pokud by se zde provadela analyza cesty s moznosti vytvareni neexistujicich podadresaru,
    // hodilo by se jeste SALCFG_CNFRMCREATEPATH (show "do you want to create target path?")

    // najdeme si masku operace (cilova cesta je v 'targetPath')
    if (opMask == NULL)
    {
        opMask = targetPath;
        while (*opMask != 0)
            opMask++;
        opMask++;
    }

    /*  // popis cile operace ziskaneho predchazejici casti kodu:
  if (diskPath)  // 'targetPath' je windowsova cesta, 'opMask' je operacni maska
  {
  }
  else   // 'targetPath' je cesta na toto FS ('userPart' ukazuje na user-part FS cesty), 'opMask' je operacni maska
  {
    // je-li 'rename' TRUE, jde o prejmenovani/kopirovani adresare do sebe sama
  }
*/

    // priprava bufferu pro jmena
    char sourceName[MAX_PATH]; // buffer pro plne jmeno na disku (zdroj operace lezi u DFS na disku)
    strcpy(sourceName, Path);
    char* endSource = sourceName + strlen(sourceName); // misto pro jmena z panelu
    if (endSource - sourceName < MAX_PATH - 1 && endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // max. pocet znaku pro jmeno z panelu

    char dfsSourceName[2 * MAX_PATH]; // buffer pro plne jmeno na DFS (pro hledani zdroje operace v disk-cache)
    sprintf(dfsSourceName, "%s:%s", fsName, sourceName);
    // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
    // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
    SalamanderGeneral->ToLowerCase(dfsSourceName);
    char* endDFSSource = dfsSourceName + strlen(dfsSourceName);                // misto pro jmena z panelu
    int endDFSSourceSize = 2 * MAX_PATH - (int)(endDFSSource - dfsSourceName); // max. pocet znaku pro jmeno z panelu

    char targetName[MAX_PATH]; // buffer pro plne jmeno na disku (pokud cil operace lezi na disku)
    targetName[0] = 0;
    char* endTarget = targetName;
    int endTargetSize = MAX_PATH;
    if (diskPath)
    {
        strcpy(targetName, targetPath);
        endTarget = targetName + strlen(targetName); // misto pro cilove jmeno
        if (endTarget - targetName < MAX_PATH - 1 && endTarget > targetName && *(endTarget - 1) != '\\')
        {
            *endTarget++ = '\\';
            *endTarget = 0;
        }
        endTargetSize = MAX_PATH - (int)(endTarget - targetName); // max. pocet znaku pro jmeno z panelu
    }

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;                     // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na zdrojove ceste (operace move)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich zdrojove cesty
    BOOL targetPathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na cilove ceste
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich cilove cesty

    HANDLE fileLock = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    if (fileLock == NULL)
    {
        DWORD err = GetLastError();
        TRACE_E("Unable to create fileLock event: " << SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH));
        cancelOrHandlePath = TRUE;
        return TRUE; // error/cancel
    }

    while (1)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme copy/move na souboru/adresari
        if (f != NULL)
        {
            // sestaveni plneho jmena, orez na MAX_PATH je teoreticky zbytecny, prakticky bohuzel ne
            lstrcpyn(endSource, f->Name, endSourceSize);
            lstrcpyn(endDFSSource, f->Name, endDFSSourceSize);
            // jmena na disku jsou "case-insensitive", disk-cache je "case-sensitive", prevod
            // na mala pismena zpusobi, ze se disk-cache bude chovat take "case-insensitive"
            SalamanderGeneral->ToLowerCase(endDFSSource);

            if (isDir) // adresar
            {
                // v DEMOPLUGinu operace s adresari neresime (rekurzivita - dva pristupy: postupne jeden
                // po druhem bez celkoveho progresu nebo sestaveni a interpretace skriptu s celkovym progresem)

                // zaroven by se tu mel resit progress (pridani za zpracovani/skipnuti souboru/adresaru)

                // hlaseni zmen na zdrojove i cilove ceste:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;
            }
            else // soubor
            {
                BOOL skip = FALSE;
                if (diskPath) // windowsova cilova cesta
                {
                    // slozime cilove jmeno - zjednodusene o test chyby "Can't finish operation because of too long name."
                    lstrcpyn(endTarget, SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, f->Name, opMask),
                             endTargetSize);

                    const char* tmpName;
                    BOOL fileFromCache = SalamanderGeneral->GetFileFromCache(dfsSourceName, tmpName, fileLock);
                    if (!fileFromCache) // soubor neni v disk-cache
                    {
                        // nakopirujeme soubor primo z DFS
                        // demoplugin neresi prepisy souboru, normalne by zde mel byt kod pro potvrzeni prepisu
                        // (pouziva promenne ConfirmOnFileOverwrite a ConfirmOnSystemHiddenFileOverwrite)
                        while (1)
                        {
                            if (!CopyFile(sourceName, targetName, TRUE))
                            {
                                if (!skipAllErrors)
                                {
                                    SalamanderGeneral->GetErrorText(GetLastError(), errBuf, MAX_PATH);
                                    sprintf(buf, "from: %s to: %s", dfsSourceName, targetName);
                                    int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, buf, errBuf, errTitle);
                                    switch (res)
                                    {
                                    case DIALOG_RETRY:
                                        break;

                                    case DIALOG_SKIPALL:
                                        skipAllErrors = TRUE;
                                    case DIALOG_SKIP:
                                        skip = TRUE;
                                        break;

                                    default:
                                        success = FALSE;
                                        break; // DIALOG_CANCEL
                                    }
                                }
                                else
                                    skip = TRUE;
                            }
                            else
                            {
                                targetPathChanged = TRUE;
                                break; // uspesne nakopirovano
                            }
                            if (!success || skip)
                                break;
                        }

                        // nejde-li o "move" (zdroj se nezrusi), neni skip ani cancel a cil je na windows ceste,
                        // dame soubor do disk-cache (pokud neni prilis veliky <= 1 MB - melo by byt konfigurovatelne,
                        // v demoplugu to ale neresime)
                        if (success && copy && !skip && f->Size <= CQuadWord(1048576, 0))
                        {
                            // nakopirujeme soubor do TEMP adresare, odkud jej presuneme do disk-cache
                            // chyby nehlasime, pouze nedojde k pridani do disk-cache
                            int err = 0;
                            char tmpName2[MAX_PATH];
                            if (SalamanderGeneral->SalGetTempFileName(NULL, "DFS", tmpName2, TRUE, NULL))
                            {
                                if (CopyFile(targetName, tmpName2, FALSE))
                                {
                                    BOOL alreadyExists;
                                    if (!SalamanderGeneral->MoveFileToCache(dfsSourceName, f->Name, NULL, tmpName2,
                                                                            f->Size, &alreadyExists))
                                    {
                                        err = alreadyExists ? 1 : 2;
                                    }
                                }
                                else
                                    err = 4;

                                if (err != 0) // chyba pri ukladani do disk-cache, odstranime soubor v TEMP adresari
                                {
                                    // aby slo smazat i nakopirovany soubor s read-only atributem
                                    SalamanderGeneral->ClearReadOnlyAttr(tmpName2);
                                    DeleteFile(tmpName2);
                                }
                            }
                            else
                                err = 3;
                            if (err != 0)
                            {
                                const char* s;
                                switch (err)
                                {
                                case 1:
                                    s = "already exists";
                                    break; // neni chyba, ale jen soubeh napr. View a Copy
                                case 2:
                                    s = "fatal error";
                                    break;
                                case 3:
                                    s = "unable to create file in TEMP directory";
                                    break;
                                default:
                                    s = "unable to copy file to TEMP directory";
                                    break;
                                }
                                TRACE_E("Unable to store file into disk-cache: " << s);
                            }
                        }
                    }
                    else // soubor je v disk-cache
                    {
                        // nakopirujeme soubor z disk-cache
                        // demoplugin neresi prepisy souboru, normalne by zde mel byt kod pro potvrzeni prepisu
                        // (pouziva promenne ConfirmOnFileOverwrite a ConfirmOnSystemHiddenFileOverwrite)
                        while (1)
                        {
                            if (!CopyFile(tmpName, targetName, TRUE))
                            {
                                if (!skipAllErrors)
                                {
                                    SalamanderGeneral->GetErrorText(GetLastError(), errBuf, MAX_PATH);
                                    sprintf(buf, "from: %s (in cache: %s) to: %s", dfsSourceName, tmpName, targetName);
                                    int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, buf, errBuf, errTitle);
                                    switch (res)
                                    {
                                    case DIALOG_RETRY:
                                        break;

                                    case DIALOG_SKIPALL:
                                        skipAllErrors = TRUE;
                                    case DIALOG_SKIP:
                                        skip = TRUE;
                                        break;

                                    default:
                                        success = FALSE;
                                        break; // DIALOG_CANCEL
                                    }
                                }
                                else
                                    skip = TRUE;
                            }
                            else
                            {
                                targetPathChanged = TRUE;
                                break; // uspesne nakopirovano
                            }
                            if (!success || skip)
                                break;
                        }

                        // odemkneme kopii souboru
                        SalamanderGeneral->UnlockFileInCache(fileLock);
                    }

                    if (success && !copy && !skip) // jde o "move" a soubor nebyl skipnuty -> smazeme zdrojovy soubor
                    {
                        // zrusime soubor na DFS
                        while (1)
                        {
                            // aby sel smazat i read-only
                            SalamanderGeneral->ClearReadOnlyAttr(sourceName, f->Attr);
                            if (!DeleteFile(sourceName))
                            {
                                if (!skipAllErrors)
                                {
                                    SalamanderGeneral->GetErrorText(GetLastError(), errBuf, MAX_PATH);
                                    int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, dfsSourceName, errBuf, errTitle);
                                    switch (res)
                                    {
                                    case DIALOG_RETRY:
                                        break;

                                    case DIALOG_SKIPALL:
                                        skipAllErrors = TRUE;
                                    case DIALOG_SKIP:
                                        skip = TRUE;
                                        break;

                                    default:
                                        success = FALSE;
                                        break; // DIALOG_CANCEL
                                    }
                                }
                                else
                                    skip = TRUE;
                            }
                            else
                            {
                                // odstranime z disk-cache kopii smazaneho souboru (je-li cachovany)
                                if (fileFromCache)
                                {
                                    SalamanderGeneral->RemoveOneFileFromCache(dfsSourceName);
                                }
                                sourcePathChanged = TRUE;
                                // subdirsOfSourcePathChanged = TRUE;

                                break; // uspesny delete
                            }
                            if (!success || skip)
                                break;
                        }
                    }
                }
                else // DFS cilova cesta
                {
                    // je-li 'rename' TRUE, jde o prejmenovani/kopirovani adresare do sebe sama

                    // v DEMOPLUGinu operace v ramci DFS neresime (nepouziva se disk-cache; komplet zavisi na FS)
                }

                // hlaseni zmen na zdrojove i cilove ceste:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;

                if (success)
                {

                    // zde by se mel resit progress (pridani za zpracovani/skipnuti jednoho souboru)
                }
            }
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }
    HANDLES(CloseHandle(fileLock));

    // zmena na zdrojove ceste Path (hlavne operace move)
    if (sourcePathChanged)
    {
        // POZOR: u bezneho pluginu by se tady mela posilat plna cesta na FS
        // (u DFS vyuzijeme toho, ze dela s diskovymi cestami a posleme jen
        // samotnou diskovou cestu, toto nelze u jinych FS pouzit)
        SalamanderGeneral->PostChangeOnPathNotification(Path, subdirsOfSourcePathChanged);
    }
    // zmena na cilove ceste targetPath (muze byt cesta na nas FS nebo na disk)
    if (targetPathChanged)
    {
        SalamanderGeneral->PostChangeOnPathNotification(targetPath, subdirsOfTargetPathChanged);
    }

    if (success)
        strcpy(targetPath, nextFocus); // uspech
    else
        cancelOrHandlePath = TRUE; // error/cancel
    return TRUE;                   // uspech nebo error/cancel
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromDiskToFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                           const char* sourcePath, SalEnumSelection2 next,
                                           void* nextParam, int sourceFiles, int sourceDirs,
                                           char* targetPath, BOOL* invalidPathOrCancel)
{
    if (invalidPathOrCancel != NULL)
        *invalidPathOrCancel = FALSE;

    if (mode == 1)
    {
        // pridame k cil. ceste masku *.* (budeme zpracovavat operacni masky)
        SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
        return TRUE;
    }

    char buf[3 * MAX_PATH + 100];
    char errBuf[MAX_PATH];
    const char* title = copy ? "DFS Copy" : "DFS Move";
    const char* errTitle = copy ? "DFS Copy Error" : "DFS Move Error";

#ifndef DEMOPLUG_QUIET
    if (mode == 2 || mode == 3)
    {
        char bufText[2 * MAX_PATH + 200];
        sprintf(bufText, "%s %d files and %d directories from disk path \"%s\" to FS path \"%s\"",
                (copy ? "Copy" : "Move"), sourceFiles, sourceDirs, sourcePath, targetPath);
        SalamanderGeneral->SalMessageBox(parent, bufText, title, MB_OK | MB_ICONINFORMATION);
    }
#endif // DEMOPLUG_QUIET

    if (mode == 2 || mode == 3)
    {
        // v 'targetPath' je neupravena cesta zadana uzivatelem (jedine co o ni vime je, ze je
        // z tohoto FS, jinak by tuto metodu Salamander nevolal)
        char* userPart = strchr(targetPath, ':') + 1; // v 'targetPath' musi byt fs-name + ':'

        CDFSPathError err;
        BOOL invPath = !DFS_IsValidPath(userPart, &err);

        // provedeme kontrolu, jestli je mozne provest operaci v tomto FS; zaroven v plne ceste
        // na toto FS mohl uzivatel pouzit "." a ".." - odstranime je
        int rootLen = 0;
        if (!invPath)
        {
            if (Path[0] != 0 && // nejde o nove otevreny FS (bez aktualni cesty)
                !SalamanderGeneral->HasTheSameRootPath(Path, userPart))
            {
                return FALSE; // DemoPlug: operaci nelze provest v tomto FS (jiny root disku)
            }

            rootLen = SalamanderGeneral->GetRootPath(buf, userPart);
            int userPartLen = (int)strlen(userPart);
            if (userPartLen < rootLen)
                rootLen = userPartLen;
        }
        if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
        {
            // navic by se dalo vypsat 'err' (pri 'invPath' TRUE), zde pro jednoduchost ignorujeme
            SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                             errTitle, MB_OK | MB_ICONEXCLAMATION);
            // 'targetPath' se vraci po mozne uprave nekterych ".." a "."
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return FALSE; // nechame uzivatele cestu opravit
        }

        // orizneme zbytecny backslash
        int l = (int)strlen(userPart);
        BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
        if (l > 1 && userPart[1] == ':') // typ cesty "c:\path"
        {
            if (l > 3) // neni root cesta
            {
                if (userPart[l - 1] == '\\')
                    userPart[l - 1] = 0; // orez backslashe
            }
            else
            {
                userPart[2] = '\\'; // root cesta, backslash nutny ("c:\")
                userPart[3] = 0;
            }
        }
        else // UNC cesta
        {
            if (l > 0 && userPart[l - 1] == '\\')
                userPart[l - 1] = 0; // orez backslashe
        }

        // rozanalyzovani cesty - nalezeni existujici casti, neexistujici casti a operacni masky
        //
        // - zjistit jaka cast cesty existuje a jestli je to soubor nebo adresar,
        //   podle vysledku vybrat o co jde:
        //   - zapis na cestu (prip. s neexistujici casti) s maskou - maska je posledni neexistujici cast cesty,
        //     za kterou jiz neni backslash (overit jestli u vice zdrojovych souboru/adresaru je
        //     v masce '*' nebo aspon '?', jinak nesmysl -> jen jedno cilove jmeno)
        //   - zapis do archivu (v ceste je soubor archivu nebo to ani nemusi byt archiv, pak jde o
        //     chybu "Salamander nevi jak tento soubor otevrit")
        //   - prepis souboru (cela cesta je jen jmeno ciloveho souboru; nesmi koncit na backslash)

        // zjisteni kam az cesta existuje (rozdeleni na existujici a neexistujici cast)
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        char* end = targetPath + strlen(targetPath);
        char* afterRoot = userPart + rootLen;
        char lastChar = 0;
        BOOL pathIsDir = TRUE;
        BOOL pathError = FALSE;

        // pokud je v ceste maska, odrizneme ji bez volani SalGetFileAttributes
        if (end > afterRoot) // jeste neni jen root
        {
            char* end2 = end;
            BOOL cut = FALSE;
            while (*--end2 != '\\') // je jiste, ze aspon za root-cestou je jeden '\\'
            {
                if (*end2 == '*' || *end2 == '?')
                    cut = TRUE;
            }
            if (cut) // ve jmene je maska -> orizneme
            {
                end = end2;
                lastChar = *end;
                *end = 0;
            }
        }

        while (end > afterRoot) // jeste neni jen root
        {
            DWORD attrs = SalamanderGeneral->SalGetFileAttributes(userPart);
            if (attrs != 0xFFFFFFFF) // tato cast cesty existuje
            {
                if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // je to soubor
                {
                    // existujici cesta nema obsahovat jmeno souboru (viz SalSplitGeneralPath), orizneme...
                    *end = lastChar;   // opravime 'targetPath'
                    pathIsDir = FALSE; // existujici cast cesty je soubor
                    while (*--end != '\\')
                        ;            // je jiste, ze aspon za root-cestou je jeden '\\'
                    lastChar = *end; // aby se nezrusila cesta
                    break;
                }
                else
                    break;
            }
            else
            {
                DWORD err2 = GetLastError();
                if (err2 != ERROR_FILE_NOT_FOUND && err2 != ERROR_INVALID_NAME &&
                    err2 != ERROR_PATH_NOT_FOUND && err2 != ERROR_BAD_PATHNAME &&
                    err2 != ERROR_DIRECTORY) // divna chyba - jen vypiseme
                {
                    sprintf(buf, "Path: %s\nError: %s", targetPath,
                            SalamanderGeneral->GetErrorText(err2, errBuf, MAX_PATH));
                    SalamanderGeneral->SalMessageBox(parent, buf, errTitle, MB_OK | MB_ICONEXCLAMATION);
                    pathError = TRUE;
                    break; // ohlasime chybu
                }
            }

            *end = lastChar; // obnova 'targetPath'
            while (*--end != '\\')
                ; // je jiste, ze aspon za root-cestou je jeden '\\'
            lastChar = *end;
            *end = 0;
        }
        *end = lastChar; // opravime 'targetPath'
        SetCursor(oldCur);

        char* opMask = NULL;
        if (!pathError) // rozdeleni probehlo bez chyby
        {
            if (*end == '\\')
                end++;

            char newDirs[MAX_PATH];
            if (SalamanderGeneral->SalSplitGeneralPath(parent, title, errTitle, sourceFiles + sourceDirs,
                                                       targetPath, afterRoot, end, pathIsDir,
                                                       backslashAtEnd, NULL, NULL, opMask, newDirs,
                                                       NULL /* 'isTheSamePathF' neni potreba*/))
            {
                if (newDirs[0] != 0) // na cilove ceste je potreba vytvorit nejake podadresare
                {
                    // POZN.: pokud neni podpora pro vytvareni podadresaru na cilove ceste, staci dat
                    //        'newDirs'==NULL v SalSplitGeneralPath(), chybu ohlasi uz SalSplitGeneralPath()

                    // POZN.: pokud by se vytvarela cesta, bylo by treba volat PostChangeOnPathNotification
                    //        (zpracuje se az pozdeji, takze nejlepe volat hned po vytvoreni cesty a ne az po
                    //        ukonceni cele operace)

                    SalamanderGeneral->SalMessageBox(parent, "Sorry, but creating of target path is not supported.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    char* e = targetPath + strlen(targetPath); // oprava 'targetPath' (spojeni 'targetPath' a 'opMask')
                    if (e > targetPath && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != opMask)
                        memmove(e, opMask, strlen(opMask) + 1); // je-li potreba, prisuneme masku
                    pathError = TRUE;
                }
                else
                {
                    /*
          // nasledujici kod osetruje situaci, kdy FS nepodporuje operacni masky
          if (opMask != NULL && (strcmp(opMask, "*.*") == 0 || strcmp(opMask, "*") == 0))
          {  // nepodporuje masky a maska je prazdna, zarizneme ji
            *opMask = 0;  // double-null terminated
          }
          if (opMask != NULL && *opMask != 0)  // maska existuje, ale neni povolena
          {
            char *e = targetPath + strlen(targetPath);   // oprava 'targetPath' (spojeni 'targetPath' a 'opMask')
            if (e > targetPath && *(e - 1) != '\\') *e++ = '\\';
            if (e != opMask) memmove(e, opMask, strlen(opMask) + 1);  // je-li potreba, prisuneme masku

            SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support operation masks (target "
                                             "path must exist or end on backslash)", errTitle,
                                             MB_OK | MB_ICONEXCLAMATION);
            pathError = TRUE;
          }
*/

                    // je-li 'pathError' FALSE, tak se podarilo rozanalyzovat cilovou cestu
                }
            }
            else
                pathError = TRUE;
        }

        if (pathError)
        {
            // 'targetPath' se vraci po uprave ".." a "." + mozna pridane masky
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return FALSE; // chyba v ceste - nechame usera opravit
        }

        /*
    // ukazka pouziti wait-okenka - potreba napr. pri nacitani jmen, ktera se maji kopirovat
    // (priprava pro celkovy progress) - nacteni struktury adresaru se provadi pri prvnim volani
    // funkce 'next' (pro enumFiles == 1 nebo 2)
    SalamanderGeneral->CreateSafeWaitWindow("Reading disk path structure, please wait...", NULL,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    Sleep(2000);  // simulate some work
    SalamanderGeneral->DestroySafeWaitWindow();
  */

        // vyzvedneme hodnoty "Confirm on" z konfigurace
        BOOL ConfirmOnFileOverwrite, ConfirmOnDirOverwrite, ConfirmOnSystemHiddenFileOverwrite;
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMDIROVER, &ConfirmOnDirOverwrite, 4, NULL);
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);
        // pokud by se zde provadela analyza cesty s moznosti vytvareni neexistujicich podadresaru,
        // hodilo by se jeste SALCFG_CNFRMCREATEPATH (show "do you want to create target path?")

        // popis cile operace ziskaneho predchazejici casti kodu:
        // 'targetPath' je cesta na toto FS ('userPart' ukazuje na user-part FS cesty), 'opMask' je operacni maska

        // priprava bufferu pro jmena
        char sourceName[MAX_PATH]; // buffer pro plne jmeno na disku
        strcpy(sourceName, sourcePath);
        char* endSource = sourceName + strlen(sourceName); // misto pro jmena z enumerace 'next'
        if (endSource > sourceName && *(endSource - 1) != '\\')
        {
            *endSource++ = '\\';
            *endSource = 0;
        }
        int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // max. pocet znaku pro jmeno z enumerace 'next'

        char targetName[MAX_PATH]; // buffer pro plne cilove jmeno na disku (cil operace lezi u DFS na disku)
        strcpy(targetName, userPart);
        char* endTarget = targetName + strlen(targetName); // misto pro cilove jmeno
        if (endTarget > targetName && *(endTarget - 1) != '\\')
        {
            *endTarget++ = '\\';
            *endTarget = 0;
        }
        int endTargetSize = MAX_PATH - (int)(endTarget - targetName); // max. pocet znaku pro cilove jmeno

        BOOL success = TRUE;                     // FALSE v pripade chyby nebo preruseni uzivatelem
        BOOL skipAllErrors = FALSE;              // skip all errors
        BOOL sourcePathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na zdrojove ceste (operace move)
        BOOL subdirsOfSourcePathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich zdrojove cesty
        BOOL targetPathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na cilove ceste
        BOOL subdirsOfTargetPathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich cilove cesty

        BOOL isDir;
        const char* name;
        const char* dosName; // dummy
        CQuadWord size;
        DWORD attr;
        FILETIME lastWrite;
        while ((name = next(NULL, 0, &dosName, &isDir, &size, &attr, &lastWrite, nextParam, NULL)) != NULL)
        { // provedeme copy/move na souboru/adresari
            // sestaveni plneho jmena, orez na MAX_PATH je teoreticky zbytecny, prakticky bohuzel ne
            lstrcpyn(endSource, name, endSourceSize);

            if (isDir) // adresar
            {
                // v DEMOPLUGinu operace s adresari neresime (rekurzivita - dva pristupy: postupne jeden
                // po druhem bez celkoveho progresu nebo sestaveni a interpretace skriptu s celkovym progresem)

                // zaroven by se tu mel resit progress (pridani za zpracovani/skipnuti souboru/adresaru)

                // hlaseni zmen na zdrojove i cilove ceste:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;
            }
            else // soubor
            {
                BOOL skip = FALSE;
                // slozime cilove jmeno - zjednodusene o test chyby "Can't finish operation because of too long name."
                // ('name' je jen z rootu zdrojove cesty - zadne podadresare - maskou upravime cele 'name')
                lstrcpyn(endTarget, SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, (char*)name, opMask),
                         endTargetSize);

                // nakopirujeme soubor primo na DFS
                // demoplugin neresi prepisy souboru, normalne by zde mel byt kod pro potvrzeni prepisu
                // (pouziva promenne ConfirmOnFileOverwrite a ConfirmOnSystemHiddenFileOverwrite)
                while (1)
                {
                    if (!CopyFile(sourceName, targetName, TRUE))
                    {
                        if (!skipAllErrors)
                        {
                            SalamanderGeneral->GetErrorText(GetLastError(), errBuf, MAX_PATH);
                            sprintf(buf, "from: %s to: %s:%s", sourceName, fsName, targetName);
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, buf, errBuf, errTitle);
                            switch (res)
                            {
                            case DIALOG_RETRY:
                                break;

                            case DIALOG_SKIPALL:
                                skipAllErrors = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break; // DIALOG_CANCEL
                            }
                        }
                        else
                            skip = TRUE;
                    }
                    else
                    {
                        targetPathChanged = TRUE;
                        break; // uspesne nakopirovano
                    }
                    if (!success || skip)
                        break;
                }

                if (success && !copy && !skip) // jde o "move" a soubor nebyl skipnuty -> smazeme zdrojovy soubor
                {
                    // zrusime soubor na disku
                    while (1)
                    {
                        // aby sel smazat i read-only
                        SalamanderGeneral->ClearReadOnlyAttr(sourceName, attr);

                        if (!DeleteFile(sourceName))
                        {
                            if (!skipAllErrors)
                            {
                                SalamanderGeneral->GetErrorText(GetLastError(), errBuf, MAX_PATH);
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, sourceName, errBuf, errTitle);
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                        {
                            sourcePathChanged = TRUE;
                            // subdirsOfSourcePathChanged = TRUE;

                            break; // uspesny delete
                        }
                        if (!success || skip)
                            break;
                    }
                }

                // hlaseni zmen na zdrojove i cilove ceste:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;

                if (success)
                {

                    // zde by se mel resit progress (pridani za zpracovani/skipnuti jednoho souboru)
                }
            }

            // zjistime jestli ma cenu pokracovat (pokud neni cancel)
            if (!success)
                break;
        }

        // zmena na zdrojove ceste (hlavne operace move)
        if (sourcePathChanged)
        {
            SalamanderGeneral->PostChangeOnPathNotification(sourcePath, subdirsOfSourcePathChanged);
        }
        // zmena na cilove ceste (melo by jit o cestu na FS - 'targetPath', u DFS vyjimka kvuli
        // tomu, ze DFS jen dela s diskovyma cestama: hlasime zmenu primo na diskove ceste 'userPart')
        if (targetPathChanged)
        {
            SalamanderGeneral->PostChangeOnPathNotification(userPart, subdirsOfTargetPathChanged);
        }

        if (success)
            return TRUE; // operace uspesne dokoncena
        else
        {
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return TRUE; // cancel
        }
    }

    return FALSE; // neznamy 'mode'
}

BOOL WINAPI
CPluginFSInterface::ChangeAttributes(const char* fsName, HWND parent, int panel,
                                     int selectedFiles, int selectedDirs)
{
    const char* title = "DFS Change Attributes";
    //  const char *errTitle = "DFS Change Attributes Error";

#ifndef DEMOPLUG_QUIET
    char bufText[100];
    sprintf(bufText, "Change attributes of %d files and %d directories.", selectedFiles, selectedDirs);
    SalamanderGeneral->SalMessageBox(parent, bufText, title, MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    // vybaleni vlastniho dialogu (neimplementovano - zadna zmena atributu se neprovadi, implementace zalezi na FS)
    MessageBox(parent, "Here should user specify how to change attributes. "
                       "It's not implemented in DemoPlug.",
               title, MB_OK | MB_ICONINFORMATION);

    /*
  // ukazka pouziti wait-okenka - potreba napr. pri nacitani jmen (priprava pro celkovy progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // priprava bufferu pro jmena
    char name[MAX_PATH]; // buffer pro plne jmeno na disku (zdroj operace lezi u DFS na disku)
    strcpy(name, Path);
    char* end = name + strlen(name); // misto pro jmena z panelu
    if (end > name && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - name); // max. pocet znaku pro jmeno z panelu

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;               // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL skipAllErrors = FALSE;        // skip all errors
    BOOL pathChanged = FALSE;          // TRUE, pokud doslo ke zmenam na ceste
    BOOL subdirsOfPathChanged = FALSE; // TRUE, pokud doslo i ke zmenam v podadresarich cesty

    while (1)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme operaci na souboru/adresari
        if (f != NULL)
        {
            // sestaveni plneho jmena, orez na MAX_PATH je teoreticky zbytecny, prakticky bohuzel ne
            lstrcpyn(end, f->Name, endSize);

            // provedeni zmeny atributu, zde neimplementovano

            // hlaseni zmen na zdrojove ceste:
            // pathChanged = TRUE;
            // subdirsOfPathChanged = TRUE;

            if (success)
            {

                // zde by se mel resit progress (pridani za zpracovani/skipnuti jednoho souboru)
            }
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    // zmena na zdrojove ceste Path
    if (pathChanged)
    {
        // POZOR: u bezneho pluginu by se tady mela posilat plna cesta na FS
        SalamanderGeneral->PostChangeOnPathNotification(Path, subdirsOfPathChanged);
    }

    //  return success;
    return FALSE; // cancel
}

void WINAPI
CPluginFSInterface::ShowProperties(const char* fsName, HWND parent, int panel,
                                   int selectedFiles, int selectedDirs)
{
    const char* title = "DFS Show Properties";
    //  const char *errTitle = "DFS Show Properties Error";

#ifndef DEMOPLUG_QUIET
    char bufText[100];
    sprintf(bufText, "Show properties of %d files and %d directories.", selectedFiles, selectedDirs);
    SalamanderGeneral->SalMessageBox(parent, bufText, title, MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    /*
  // ukazka pouziti wait-okenka - potreba napr. pri nacitani jmen (priprava pro celkovy progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NILL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // priprava bufferu pro jmena
    char name[MAX_PATH]; // buffer pro plne jmeno na disku (zdroj operace lezi u DFS na disku)
    strcpy(name, Path);
    char* end = name + strlen(name); // misto pro jmena z panelu
    if (end > name && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - name); // max. pocet znaku pro jmeno z panelu

    const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
    BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;        // FALSE v pripade chyby nebo preruseni uzivatelem
    BOOL skipAllErrors = FALSE; // skip all errors

    while (1)
    {
        // vyzvedneme data o zpracovavanem souboru
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // provedeme zjisteni vlastnosti na souboru/adresari
        if (f != NULL)
        {
            // sestaveni plneho jmena, orez na MAX_PATH je teoreticky zbytecny, prakticky bohuzel ne
            lstrcpyn(end, f->Name, endSize);

            // zjisteni vlastnosti, zde neimplementovano

            if (success)
            {

                // zde by se mel resit progress (pridani za zpracovani/skipnuti jednoho souboru)
            }
        }

        // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
        if (!success || focused || f == NULL)
            break;
    }

    if (success)
    {
        // vybaleni vlastniho okna (neimplementovano, implementace zalezi na FS)
        MessageBox(parent, "Here should be properties of selected files and directories. "
                           "It's not implemented in DemoPlug.",
                   title, MB_OK | MB_ICONINFORMATION);
    }
}

void WINAPI
CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                int panel, int selectedFiles, int selectedDirs)
{
#ifndef DEMOPLUG_QUIET
    char bufText[100];
    sprintf(bufText, "Show context menu (type %d).", (int)type);
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Context Menu", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    HMENU menu = CreatePopupMenu();
    if (menu == NULL)
    {
        TRACE_E("CPluginFSInterface::ContextMenu: Unable to create menu.");
        return;
    }
    MENUITEMINFO mi;
    char nameBuf[200];

    switch (type)
    {
    case fscmItemsInPanel: // kontextove menu pro polozky v panelu (oznacene/fokusle soubory a adresare)
    {
        int i = 0;

        // vlozeni prikazu Salamandera
        strcpy(nameBuf, "Always Command from DemoPlug Submenu");
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.wID = MENUCMD_ALWAYS;
        mi.dwTypeData = nameBuf;
        mi.cch = (UINT)strlen(nameBuf);
        mi.fState = MFS_ENABLED;
        InsertMenuItem(menu, i++, TRUE, &mi);

        int index = 0;
        int salCmd;
        BOOL enabled;
        int type2, lastType = sctyUnknown;
        while (SalamanderGeneral->EnumSalamanderCommands(&index, &salCmd, nameBuf, 200, &enabled, &type2))
        {
            if (type2 != lastType /*&& lastType != sctyUnknown*/) // vlozeni separatoru
            {
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE;
                mi.fType = MFT_SEPARATOR;
                InsertMenuItem(menu, i++, TRUE, &mi);
            }
            lastType = type2;

            // vlozeni prikazu Salamandera
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);
            mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
            mi.fType = MFT_STRING;
            mi.wID = salCmd + 1000; // vsechny prikazy Salamandera odsuneme o 1000, aby se daly rozlisit od nasich prikazu
            mi.dwTypeData = nameBuf;
            mi.cch = (UINT)strlen(nameBuf);
            mi.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
            InsertMenuItem(menu, i++, TRUE, &mi);
        }
        DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                     menuX, menuY, parent, NULL);
        if (cmd != 0) // user vybral z menu prikaz
        {
            if (cmd >= 1000)
            {
                if (SalamanderGeneral->GetSalamanderCommand(cmd - 1000, nameBuf, 200, &enabled, &type2))
                {
                    TRACE_I("Starting command: " << nameBuf);
                }

                SalamanderGeneral->PostSalamanderCommand(cmd - 1000);
            }
            else // nas vlastni prikaz
            {
                TRACE_I("Starting command: Always");
                SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // spusti se az v "sal-idle"
                                                                  /*
          SalamanderGeneral->PostMenuExtCommand(cmd, FALSE); // spusti az se doruci zprava hl. oknu
          // POZOR: po tomto prikazu uz nesmi dojit k otevreni zadneho okna s message-loopou,
          // jinak bude prikaz pluginu spusten jeste pred dokoncenim teto metody!
*/
            }
        }
        break;
    }

    case fscmPathInPanel: // kontextove menu pro aktualni cestu v panelu
    {
        int i = 0;

        // vlozeni prikazu Salamandera
        strcpy(nameBuf, "Menu For Actual Path: Always Command from DemoPlug Submenu");
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.wID = MENUCMD_ALWAYS;
        mi.dwTypeData = nameBuf;
        mi.cch = (UINT)strlen(nameBuf);
        mi.fState = MFS_ENABLED;
        InsertMenuItem(menu, i++, TRUE, &mi);

        strcpy(nameBuf, "&Disconnect");
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.wID = panel == PANEL_LEFT ? MENUCMD_DISCONNECT_LEFT : MENUCMD_DISCONNECT_RIGHT;
        mi.dwTypeData = nameBuf;
        mi.cch = (UINT)strlen(nameBuf);
        mi.fState = MFS_ENABLED;
        InsertMenuItem(menu, i++, TRUE, &mi);

        DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                     menuX, menuY, parent, NULL);
        if (cmd != 0)                                         // user vybral z menu prikaz
            SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // spusti se az v "sal-idle"
        break;
    }

    case fscmPanel: // kontextove menu pro panel
    {
        int i = 0;

        // vlozeni prikazu Salamandera
        strcpy(nameBuf, "Menu For Panel: Always Command from DemoPlug Submenu");
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.wID = MENUCMD_ALWAYS;
        mi.dwTypeData = nameBuf;
        mi.cch = (UINT)strlen(nameBuf);
        mi.fState = MFS_ENABLED;
        InsertMenuItem(menu, i++, TRUE, &mi);

        strcpy(nameBuf, "&Disconnect");
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
        mi.fType = MFT_STRING;
        mi.wID = panel == PANEL_LEFT ? MENUCMD_DISCONNECT_LEFT : MENUCMD_DISCONNECT_RIGHT;
        mi.dwTypeData = nameBuf;
        mi.cch = (UINT)strlen(nameBuf);
        mi.fState = MFS_ENABLED;
        InsertMenuItem(menu, i++, TRUE, &mi);

        DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                     menuX, menuY, parent, NULL);
        if (cmd != 0)                                         // user vybral z menu prikaz
            SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // spusti se az v "sal-idle"
        break;
    }
    }
    DestroyMenu(menu);
}
