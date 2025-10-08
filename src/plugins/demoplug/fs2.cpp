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
            while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) // give the user a brief timeslice ...
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
    while (PeekMessage(&msg, NULL, 0, 0, TRUE)) // give the user a brief timeslice ...
    {
        if (!IsWindow(HWindow) || !IsDialogMessage(HWindow, &msg))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    // repaint changed data (text + progress bars) every 100 ms
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
        // use the Salamander-styled progress bar
        ProgressBar = SalamanderGUI->AttachProgressBar(HWindow, IDP_PROGRESSBAR);
        if (ProgressBar == NULL)
        {
            DestroyWindow(HWindow); // error -> do not show the dialog
            return FALSE;           // stop processing
        }

        break; // let DefDlgProc handle focus
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
    if (Path[0] != 0) // if the FS is initialized, remove our disk-cache copies when closing
    {
        // build a unique name for this FS root in the disk cache (covers all files from this FS)
        char uniqueFileName[2 * MAX_PATH];
        strcpy(uniqueFileName, AssignedFSName);
        strcat(uniqueFileName, ":");
        SalamanderGeneral->GetRootPath(uniqueFileName + strlen(uniqueFileName), Path);
        // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
        // to lowercase makes the disk cache behave case-insensitively as well
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
    lstrcpyn(buf, Path, bufSize); // if the path does not fit, the name certainly will not (Salamander will report an error)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return SalamanderGeneral->SalPathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    if (Path[0] == 0)
        return FALSE; // cannot translate the path, let Salamander report the error

    char root[MAX_PATH];
    int rootLen = SalamanderGeneral->GetRootPath(root, Path);
    if (*path != '\\')
        strcpy(root, Path); // paths such as "path" take over the current FS path
    // we can leave ".." and "." in the path, they will be removed later; we also do not
    // validate the path or its syntax here
    success = SalamanderGeneral->SalPathAppend(root, path, MAX_PATH);
    if (success && (int)strlen(root) < rootLen) // cannot be shorter than the root (that would be a relative path)
    {
        success = SalamanderGeneral->SalPathAddBackslash(root, MAX_PATH);
    }
    if (success)
        success = (int)(strlen(root) + strlen(fsName) + 1) < pathSize; // does it fit?
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
    { // the user is opening a new connection from the Connect dialog - return FALSE (they might
        // request a second connection to the same path)
        return FALSE;
    }

    // allow transitions between individual FS names (in other words ignore
    // 'currentFSNameIndex' and 'fsNameIndex'); normally the opposite would be
    // expected (for example switching from "FTP" to "HTTP" makes no sense)

    // pretend that each FS can handle only a single root
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
        return FALSE; // ListCurrentPath failed because of low memory, fatal error
    }

    char errBuf[MAX_PATH];
    errBuf[0] = 0;
    char path[MAX_PATH];

    if (*userPart == 0 && ConnectData.UseConnectData) // data from the Connect dialog
    {
        userPart = ConnectData.UserPart;
        lstrcpyn(path, ConnectData.UserPart, MAX_PATH);
    }
    else
        lstrcpyn(path, userPart, MAX_PATH);

    SalamanderGeneral->SalUpdateDefaultDir(TRUE); // refresh before SalGetFullName; prefer the active panel regardless of which panel this FS serves
    int err;
    if (SalamanderGeneral->SalGetFullName(path, &err))
    {
        BOOL fileNameAlreadyCut = FALSE;
        if (PathError) // an error occurred while listing the path (already reported in ListCurrentPath)
        {              // try to shorten the path
            PathError = FALSE;
            if (!SalamanderGeneral->CutDirectory(path, NULL))
                return FALSE; // nothing left to shorten, fatal error
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

            if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) // success, pick the path as current
            {
                if (errBuf[0] != 0) // if we have a message from shortening, display it now
                {
                    sprintf(buf, "Path: %s\nError: %s", userPart, errBuf);
                    SalamanderGeneral->ShowMessageBox(buf, "DFS Error", MSGBOX_ERROR);
                }
                strcpy(Path, path);

                // timer test only (useful for keep-connection-alive for example)
                //        SalamanderGeneral->KillPluginFSTimer(this, TRUE, 0);   // clear existing timers for this FS first
                //        SalamanderGeneral->AddPluginFSTimer(2000, this, 1234);

                return TRUE;
            }
            else // failure, try to shorten the path
            {
                err = GetLastError();

                if (mode != 3 && attr != 0xFFFFFFFF || // a file instead of a path -> report as an error
                    mode != 1 && attr == 0xFFFFFFFF)   // the path does not exist -> report as an error
                {
                    if (attr != 0xFFFFFFFF)
                    {
                        sprintf(errBuf, "The path specified contains path to a file. Unable to open file.");
                    }
                    else
                        SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);

                    // if opening the FS is time-consuming and we want Change Directory (Shift+F7)
                    // to behave like in archives, comment out the "break" line below for mode 3
                    if (mode == 3)
                        break;
                }

                char* cut;
                if (!SalamanderGeneral->CutDirectory(path, &cut)) // nothing left to shorten, fatal error
                {
                    SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);
                    break;
                }
                else
                {
                    if (pathWasCut != NULL)
                        *pathWasCut = TRUE;
                    if (!fileNameAlreadyCut) // only the first truncation can still be the filename
                    {
                        fileNameAlreadyCut = TRUE;
                        if (cutFileName != NULL && attr != 0xFFFFFFFF) // it is a file
                            lstrcpyn(cutFileName, cut, MAX_PATH);
                    }
                    else
                    {
                        if (cutFileName != NULL)
                            *cutFileName = 0; // this can no longer be a filename
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
        PathError = TRUE; // simulate a path error -> ChangePath will start shortening it
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
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES) // an actual error occurred
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

    // declare which fields in 'file' are valid
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
        if (data.cFileName[0] != 0 && strcmp(data.cFileName, ".") != 0 && // skip "."
            (!isRootPath || strcmp(data.cFileName, "..") != 0))           // skip ".." at the root
        {
            file.Name = SalamanderGeneral->DupStr(data.cFileName);
            if (file.Name == NULL)
                goto ERR_2;
            file.NameLen = strlen(file.Name);
            if (!sortByExtDirsAsFiles && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                file.Ext = file.Name + file.NameLen; // directories have no extension
            }
            else
            {
                char* s;
                s = strrchr(file.Name, '.');
                if (s != NULL)
                    file.Ext = s + 1; // Windows treats ".cvspass" as an extension...
                else
                    file.Ext = file.Name + file.NameLen;
            }
            file.Size = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
            file.Attr = data.dwFileAttributes;
            file.LastWrite = data.ftLastWriteTime;
            file.Hidden = file.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            // always set IconOverlayIndex; Salamander will ignore it if it doesn't apply
            // read-only = slow file overlay, system = shared overlay
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
                file.IsLink = 0; // simplify things (ignore volume mount points and junction points)
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
    // each name is inserted up to three times with different character casing
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
            break; // end of enumeration
        }
    }

    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    if (CalledFromDisconnectDialog)
    {
        detach = FALSE; // we want to close the FS in any case
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
            return TRUE; // do not ask anything during a critical shutdown

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

    if (event == FSE_ACTIVATEREFRESH) // the user activated Salamander (switched from another application)
    {
        // refresh the path;
        // we are inside CPluginFSInterface, so RefreshPanelPath cannot be used
        //    SalamanderGeneral->PostRefreshPanelPath((int)param);
        SalamanderGeneral->PostRefreshPanelFS(this);

        sprintf(buf, "Activate refresh on path \"%s\" (%s).", Path, (param == PANEL_LEFT ? "left" : "right"));
#ifdef DEMOPLUG_QUIET
        TRACE_I("DemoPlug: " << buf);
#else  // DEMOPLUG_QUIET
        SalamanderGeneral->ShowMessageBox(buf, "FS Event", MSGBOX_INFO);
#endif // DEMOPLUG_QUIET
    }

    /*  // simple test of receiving the "timer" event after the timer expires
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
    // the text will be the FS path (in Salamander format)
    txt[0] = '\t';
    strcpy(txt + 1, fsName);
    sprintf(txt + strlen(txt), ":%s\t", Path);
    // double any '&' characters so the path prints correctly
    SalamanderGeneral->DuplicateAmpersands(txt, 2 * MAX_PATH + 102);
    // append information about free space
    CQuadWord space;
    SalamanderGeneral->GetDiskFreeSpace(&space, Path, NULL);
    if (space != CQuadWord(-1, -1))
        SalamanderGeneral->PrintDiskSize(txt + strlen(txt), space, 0);
    title = SalamanderGeneral->DupStr(txt);
    if (title == NULL)
        return FALSE; // low-memory, no item will be shown

    SalamanderGeneral->GetRootPath(txt, Path);

    if (!SalamanderGeneral->GetFileIcon(txt, FALSE, &icon, SALICONSIZE_16, TRUE, TRUE))
        icon = NULL;
    // switched to our own implementation (lower memory use, working XOR icons)
    //SHFILEINFO shi;
    //if (SHGetFileInfo(txt, 0, &shi, sizeof(shi),
    //                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE))
    //{
    //  icon = shi.hIcon;  // icon successfully retrieved
    //}
    //else icon = NULL;  // no icon available
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
    // switched to our own implementation (lower memory use, working XOR icons)
    //SHFILEINFO shi;
    //if (SHGetFileInfo(root, 0, &shi, sizeof(shi),
    //                  SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE))
    //{
    //  icon = shi.hIcon;  // icon successfully retrieved
    //}
    //else icon = NULL;  // no icon available (the standard one will be used)
    destroyIcon = TRUE;
    return icon;
}

void WINAPI
CPluginFSInterface::GetDropEffect(const char* srcFSPath, const char* tgtFSPath,
                                  DWORD allowedEffects, DWORD keyState, DWORD* dropEffect)
{                                                                                       // if Copy and Move are both available, choose Move when both FS instances share the same root
    if ((*dropEffect & DROPEFFECT_MOVE) && *dropEffect != DROPEFFECT_MOVE &&            // otherwise there is no point in checking
        SalamanderGeneral->StrNICmp(srcFSPath, AssignedFSName, AssignedFSNameLen) == 0) // only paths on our FS are relevant
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
    const char* root = text; // pointer past the root portion of the path
    while (*root != 0 && *root != ':')
        root++;
    if (*root == ':')
    {
        root++;
        if (*root == '\\') // UNC path
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
        else // standard path
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
    // if the plugin opens its own dialog, it should use CSalamanderGeneralAbstract::AlterFileName
    // ('format' according to SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // request the standard dialog

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "From: %s\nTo: %s", file.Name, newName);
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Quick Rename", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    // validate the entered name (syntactically)
    char* s = newName;
    char buf[2 * MAX_PATH];
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (newName[0] == 0 || *s != 0)
    {
        SalamanderGeneral->GetErrorText(ERROR_INVALID_NAME, buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // invalid name, let the user fix it
    }

    // apply the mask in newName
    SalamanderGeneral->MaskName(buf, 2 * MAX_PATH, file.Name, newName);
    lstrcpyn(newName, buf, MAX_PATH);

    // perform the rename operation
    char nameFrom[MAX_PATH];
    char nameTo[MAX_PATH];
    strcpy(nameFrom, Path);
    strcpy(nameTo, Path);
    if (!SalamanderGeneral->SalPathAppend(nameFrom, file.Name, MAX_PATH) ||
        !SalamanderGeneral->SalPathAppend(nameTo, newName, MAX_PATH))
    {
        SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                         "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned after adjustment (mask applied)
        return FALSE; // error -> show the standard dialog again
    }
    if (!MoveFile(nameFrom, nameTo))
    {
        // (overwriting is not handled here; treat it as an error as well)
        SalamanderGeneral->GetErrorText(GetLastError(), buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Quick Rename Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned after adjustment (mask applied)
        return FALSE; // error -> show the standard dialog again
    }
    else // operation succeeded - report the change on the path (trigger refresh) and report success
    {
        if (SalamanderGeneral->StrICmp(nameFrom, nameTo) != 0)
        { // if it is more than just a case change (DFS is not case-sensitive)
            // remove the source of the operation from the disk cache (the original name is no longer valid)
            char dfsFileName[2 * MAX_PATH];
            sprintf(dfsFileName, "%s:%s", fsName, nameFrom);
            // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
            // to lowercase makes the disk cache behave case-insensitively as well
            SalamanderGeneral->ToLowerCase(dfsFileName);
            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
            // if overwriting is possible, the destination should also be removed from the disk cache (a "file change" happened)
        }

        // report a change on the 'Path' path (without subdirectories when renaming files)
        // NOTE: a typical plugin should send the full FS path here
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

    // WARNING: a regular plugin should work with FS paths here
    // for DFS we simplify the logic to operate on disk paths because DFS
    // only exposes a disk path; see the WMobile implementation below

    // test whether the paths match or at least share a prefix (only disk paths matter;
    // FS paths in 'path' are excluded automatically because they can never match Path)
    char path1[MAX_PATH];
    char path2[MAX_PATH];
    lstrcpyn(path1, path, MAX_PATH);
    lstrcpyn(path2, Path, MAX_PATH);
    SalamanderGeneral->SalPathRemoveBackslash(path1);
    SalamanderGeneral->SalPathRemoveBackslash(path2);
    int len1 = (int)strlen(path1);
    BOOL refresh = !includingSubdirs && SalamanderGeneral->StrICmp(path1, path2) == 0 ||       // exact match
                   includingSubdirs && SalamanderGeneral->StrNICmp(path1, path2, len1) == 0 && // prefix match
                       (path2[len1] == 0 || path2[len1] == '\\');
    if (!refresh && SalamanderGeneral->CutDirectory(path1))
    {
        SalamanderGeneral->SalPathRemoveBackslash(path1);
        // on NTFS the last subdirectory's timestamp changes as well (it shows only after entering
        // that subdirectory; hopefully it will be fixed someday)
        refresh = SalamanderGeneral->StrICmp(path1, path2) == 0;
    }
    if (refresh)
    {
        SalamanderGeneral->PostRefreshPanelFS(this); // refresh the panel if this FS is displayed there
    }

    // example of an implementation from the WMobile plugin:
    /*
  // test whether the paths match or at least share a prefix (only paths on our FS qualify;
  // disk paths and paths on other FSs in 'path' are excluded automatically,
  // because they can never match 'fsName'+':' at the beginning of 'path2' below)
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
    SalamanderGeneral->PostRefreshPanelFS(this);   // refresh the panel if this FS is displayed there
*/
}

BOOL WINAPI
CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // request for the standard dialog

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "New directory: %s", newName);
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Create Directory", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    SalamanderGeneral->SalUpdateDefaultDir(TRUE); // update before using SalParsePath (internally uses SalGetFullName)

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
        if (error == SPP_EMPTYPATHNOTALLOWED) // empty string -> abort without performing the operation
        {
            cancel = TRUE;
            return TRUE; // the return value no longer matters
        }

        if (error == SPP_INCOMLETEPATH) // relative path on the FS; build an absolute path manually
        {
            // for disk paths it would be better to use SalGetFullName:
            // SalamanderGeneral->SalGetFullName(newName, &errTextID, Path, nextFocus) + handle errors
            // then it would be enough to prepend the FS name to the resolved path
            // here we will demonstrate our own implementation (using SalRemovePointsFromPath and others):

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
            if (newName[0] == '\\') // "\\path" -> concatenate root + newName
            {
                s += rootLen;
                int len = (int)strlen(newName + 1); // without the leading '\\'
                if (len + rootLen >= MAX_PATH)
                    tooLong = TRUE;
                else
                {
                    memcpy(s, newName + 1, len);
                    *(s + len) = 0;
                }
            }
            else // "path" -> concatenate Path + newName
            {
                int pathLen = (int)strlen(Path);
                if (pathLen < rootLen)
                    rootLen = pathLen;
                strcpy(s + rootLen, Path + rootLen); // the root is already copied there
                tooLong = !SalamanderGeneral->SalPathAppend(s, newName, MAX_PATH);
            }

            if (tooLong)
            {
                SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                                 "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
                // 'newName' is returned unchanged
                return FALSE; // error -> show the standard dialog again
            }

            strcpy(newName, path);
            secondPart = newName + (userPart - path);
            type = PATH_TYPE_FS;
        }
        else
            return FALSE; // error -> show the standard dialog again
    }

    if (type != PATH_TYPE_FS)
    {
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory on disk or archive path.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned already adjusted (expanded path)
        return FALSE; // error -> show the standard dialog again
    }

    if ((secondPart - newName) - 1 != (int)strlen(fsName) ||
        SalamanderGeneral->StrNICmp(newName, fsName, (int)(secondPart - newName) - 1) != 0)
    { // not a DFS path
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory on specified file-system.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned already adjusted (expanded path)
        return FALSE; // error -> show the standard dialog again
    }

    if (!SalamanderGeneral->HasTheSameRootPath(Path, secondPart))
    { // DFS operates only within a single drive (e.g. FTP would also have to check
        // whether the user is trying to create a directory on another server)
        SalamanderGeneral->SalMessageBox(parent, "Sorry, but this plugin is not able "
                                                 "to create directory outside of currently opened drive.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned already adjusted (expanded path)
        return FALSE; // error -> show the standard dialog again
    }

    // the full path on this FS may contain "." and ".." - remove them
    int rootLen = SalamanderGeneral->GetRootPath(buf, secondPart);
    int secPartLen = (int)strlen(secondPart);
    if (secPartLen < rootLen)
        rootLen = secPartLen;
    if (!SalamanderGeneral->SalRemovePointsFromPath(secondPart + rootLen))
    {
        SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                         "DFS Create Directory", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned already adjusted (expanded path) and with any ".." and "." normalized
        return FALSE; // error -> show the standard dialog again
    }

    // trim the redundant backslash
    int l = (int)strlen(secondPart);   // recompute because SalRemovePointsFromPath may have changed the path
    if (l > 1 && secondPart[1] == ':') // path type "c:\path"
    {
        if (l > 3) // not a root path
        {
            if (secondPart[l - 1] == '\\')
                secondPart[l - 1] = 0; // trim trailing backslashes
        }
        else
        {
            secondPart[2] = '\\'; // root path, backslash required ("c:\")
            secondPart[3] = 0;
        }
    }
    else // UNC path
    {
        if (l > 0 && secondPart[l - 1] == '\\')
            secondPart[l - 1] = 0; // trim trailing backslashes
    }

    // finally create the directory
    DWORD err;
    if (!SalamanderGeneral->SalCreateDirectoryEx(secondPart, &err))
    {
        SalamanderGeneral->GetErrorText(err, buf, 2 * MAX_PATH);
        SalamanderGeneral->SalMessageBox(parent, buf, "DFS Create Directory Error", MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned already adjusted (expanded path)
        return FALSE; // error -> show the standard dialog again
    }
    else // operation succeeded - report the path change (triggers refresh) and return success
    {
        // change on the path (without subdirectories)
        SalamanderGeneral->CutDirectory(secondPart); // must succeed (cannot be root)
        // NOTE: a typical plugin should send the full FS path here
        SalamanderGeneral->PostChangeOnPathNotification(secondPart, FALSE);
        strcpy(newName, nextFocus); // if only the directory name was entered, focus it in the panel
        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
    // build a unique file name for the disk cache (standard Salamander path format)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, fsName);
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, Path);
    SalamanderGeneral->SalPathAppend(uniqueFileName + strlen(fsName) + 1, file.Name, MAX_PATH);
    // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
    // to lowercase makes the disk cache behave case-insensitively as well
    SalamanderGeneral->ToLowerCase(uniqueFileName);

    // obtain the cache copy name
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, file.Name, NULL, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // determine whether a copy needs to be prepared in the disk cache (download)
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // preparing the file copy (download) is necessary
    {
        const char* name = uniqueFileName + strlen(fsName) + 1;
        if (CopyFile(name, tmpFileName, TRUE)) // the copy succeeded
        {
            newFileOK = TRUE; // if determining the file size fails, newFileSize stays zero (not too important)
            HANDLE hFile = HANDLES_Q(CreateFile(tmpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL, OPEN_EXISTING, 0, NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            { // ignore errors; the exact file size is not essential
                DWORD err;
                SalamanderGeneral->SalGetFileSize(hFile, newFileSize, err); // ignore errors
                HANDLES(CloseHandle(hFile));
            }
        }
        else // copy (download) failed
        {
            DWORD err = GetLastError();
            char errorText[3 * MAX_PATH + 100];
            sprintf(errorText, "Unable to download file %s to disk file %s.\nError: ",
                    uniqueFileName, tmpFileName);
            SalamanderGeneral->GetErrorText(err, errorText + strlen(errorText), MAX_PATH);
            SalamanderGeneral->SalMessageBox(parent, errorText, "DFS Error", MB_OK | MB_ICONEXCLAMATION);
        }
    }

    // open the viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileOK || // open the viewer only if the file copy is OK
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // on failure reset the "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // call FreeFileNameInCache to pair with AllocFileNameInCache (connects
    // the viewer and the disk cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, FALSE /* do not delete immediately after closing the viewer */);
}

BOOL WINAPI
CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                           int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    // if the plugin opened the dialog itself, it should use CSalamanderGeneralAbstract::AlterFileName
    // ('format' according to SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    cancelOrError = FALSE;
    if (mode == 1)
        return FALSE; // request the standard prompt (if SALCFG_CNFRMFILEDIRDEL is TRUE) - see CPluginFSInterface::CopyOrMoveFromFS for how to build the question text

#ifndef DEMOPLUG_QUIET
    char bufText[2 * MAX_PATH + 100];
    sprintf(bufText, "Delete %d files and %d directories from %s panel.",
            selectedFiles, selectedDirs, (panel == PANEL_LEFT ? "left" : "right"));
    SalamanderGeneral->SalMessageBox(parent, bufText, "DFS Delete", MB_OK | MB_ICONINFORMATION);
#endif // DEMOPLUG_QUIET

    /*
  // example of using the wait window - useful e.g. when reading names slated for deletion
  // (preparation for overall progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // find the parent's top-level window (may be Salamander's main window)
    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disable 'mainWnd'
    EnableWindow(mainWnd, FALSE);

    BOOL retSuccess = FALSE;
    BOOL enableMainWnd = TRUE;
    CDeleteProgressDlg delDlg(mainWnd, ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack
    if (delDlg.Create() != NULL)                  // the dialog opened successfully
    {
        SetForegroundWindow(delDlg.HWindow);

        delDlg.Set("reading directory tree...", 0, FALSE);
        Sleep(1500); // simulate activity
        delDlg.Set("preparing data...", 0, FALSE);
        Sleep(500); // simulate activity

        int i;
        for (i = 0; i <= 1000; i++)
        {
            if (delDlg.GetWantCancel())
            {
                delDlg.Set("canceling operation...", i, FALSE);
                Sleep(500); // simulate the "cancel" action
                break;
            }

            char buf[100];
            sprintf(buf, "filename_%d.test", i);
            delDlg.Set(buf, i, TRUE); // delayedPaint == TRUE so we do not slow things down

            Sleep(20); // simulate activity
        }
        retSuccess = (i > 1000);

        // re-enable 'mainWnd' (otherwise Windows cannot bring it to the foreground)
        EnableWindow(mainWnd, TRUE);
        enableMainWnd = FALSE;

        DestroyWindow(delDlg.HWindow); // close the progress dialog
    }

    if (enableMainWnd)
    { // re-enable 'mainWnd' (no foreground change occurred - the progress never opened)
        EnableWindow(mainWnd, TRUE);
    }
    return retSuccess; // success only if Cancel was not pressed and the progress dialog opened

    /*
  // fetch the "Confirm on" configuration values
  BOOL ConfirmOnNotEmptyDirDelete, ConfirmOnSystemHiddenFileDelete, ConfirmOnSystemHiddenDirDelete;
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, 4, NULL);
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEDEL, &ConfirmOnSystemHiddenFileDelete, 4, NULL);
  SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHDIRDEL, &ConfirmOnSystemHiddenDirDelete, 4, NULL);

  char buf[2 * MAX_PATH];  // buffer for error texts

  char fileName[MAX_PATH];   // buffer for the full name
  strcpy(fileName, Path);
  char *end = fileName + strlen(fileName);  // space reserved for names from the panel
  if (end > fileName && *(end - 1) != '\\')
  {
    *end++ = '\\';
    *end = 0;
  }
  int endSize = MAX_PATH - (end - fileName);  // maximum number of characters available for a panel name

  char dfsFileName[2 * MAX_PATH];   // buffer for the full DFS name
  sprintf(dfsFileName, "%s:%s", fsName, fileName);
  char *endDFSName = dfsFileName + strlen(dfsFileName);  // space reserved for names from the panel
  int endDFSNameSize = 2 * MAX_PATH - (endDFSName - dfsFileName); // maximum number of characters available for a panel name

  const CFileData *f = NULL;  // pointer to the file/directory in the panel to process
  BOOL isDir = FALSE;         // TRUE if 'f' is a directory
  BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
  int index = 0;
  BOOL success = TRUE;        // FALSE if an error occurs or the user cancels
  BOOL skipAllSHFD = FALSE;   // skip all deletes of system or hidden files
  BOOL yesAllSHFD = FALSE;    // delete all system or hidden files
  BOOL skipAllSHDD = FALSE;   // skip all deletes of system or hidden dirs
  BOOL yesAllSHDD = FALSE;    // delete all system or hidden dirs
  BOOL skipAllErrors = FALSE; // skip all errors
  BOOL changeInSubdirs = FALSE;
  while (1)
  {
    // fetch data for the file being processed
    if (focused) f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
    else f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

    // delete the file/directory
    if (f != NULL)
    {
      // assemble the full names; trimming to MAX_PATH (2 * MAX_PATH) is theoretically unnecessary
      // but unfortunately required in practice
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

        if (success && !skip)   // not canceled and not skipped
        {

          // handle ConfirmOnNotEmptyDirDelete plus recursive delete here,
          // also update the progress (after deleting/skipping files/directories)
          // deleted files should call SalamanderGeneral->RemoveOneFileFromCache();

          changeInSubdirs = TRUE;   // changes may also occur in subdirectories
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

        if (success && !skip)   // not canceled and not skipped
        {
          BOOL skip = FALSE;
          while (1)
          {
            SalamanderGeneral->ClearReadOnlyAttr(fileName, f->Attr);  // allow deletion of read-only items
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
              // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
              // to lowercase makes the disk cache behave case-insensitively as well
              SalamanderGeneral->ToLowerCase(dfsFileName);
              // remove the deleted file's copy from the disk cache (if it is cached)
              SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
              break;   // delete succeeded
            }
            if (!success || skip) break;
          }

          if (success)
          {

            // update the progress here (after deleting/skipping a single file)

          }
        }
      }
    }

    // check whether it makes sense to continue (if there is no error and another selected item exists)
    if (!success || focused || f == NULL) break;
  }

  // change on the Path path (without subdirectories if only files were deleted)
  // NOTE: a typical plugin should send the full FS path here
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
    dfspeRelativePath, // relative paths are not supported ("PATH", "\PATH", or "C:PATH")
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
                s++; // skip the server name
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
    else // path specified via a drive (c:\...)
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
    // if the plugin opened the dialog itself, it should use CSalamanderGeneralAbstract::AlterFileName
    // ('format' according to SalamanderGeneral->GetConfigParameter(SALCFG_FILENAMEFORMAT))
    char path[2 * MAX_PATH];
    operationMask = FALSE;
    cancelOrHandlePath = FALSE;
    if (mode == 1) // first call to CopyOrMoveFromFS
    {
        /*
    // example of composing the edit-line title with the copy target
    // (if the subject is "files" and "directories", you can simply call
    //  SalamanderGeneral->GetCommonFSOperSourceDescr(subjectSrc, MAX_PATH + 100,
    //  panel, selectedFiles, selectedDirs, NULL, FALSE, FALSE), which replaces the code below building subjectSrc)
    char subjectSrc[MAX_PATH + 100];
    if (selectedFiles + selectedDirs <= 1)  // a single selected item or the focused one
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
      char formatedFileName[MAX_PATH];  // CFileData::Name is at most MAX_PATH-5 characters - Salamander's limit
      SalamanderGeneral->AlterFileName(formatedFileName, f->Name, fileNameFormat, 0, isDir);
      _snprintf_s(subjectSrc, _TRUNCATE, isDir ? "directory \"%s\"" : "file \"%s\"", formatedFileName);
      subjectSrc[MAX_PATH + 100 - 1] = 0;
    }
    else  // multiple directories and files
    {
      SalamanderGeneral->ExpandPluralFilesDirs(subjectSrc, MAX_PATH + 100, selectedFiles,
                                               selectedDirs, epfdmNormal, FALSE);
    }
    char subject[MAX_PATH + 200];
    sprintf(subject, "Copy %s from FS to", subjectSrc);
*/

        // if no path is proposed, check whether the other panel has DFS mounted and suggest it
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
        // if a path is proposed, append the *.* mask (operation masks will be processed)
        if (*targetPath != 0)
        {
            SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_TARGET); // default action = work with the target panel path
        }

        return FALSE; // request for the standard dialog
    }

    if (mode == 4) // error in the standard Salamander processing of the destination path
    {
        // 'targetPath' contains an invalid path; the user was notified, just let them edit it
        return FALSE; // request for the standard dialog
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

    BOOL diskPath = TRUE;  // for mode==3 'targetPath' is a Windows path (FALSE = a path on this FS)
    char* userPart = NULL; // pointer within 'targetPath' to the FS user-part (used when diskPath is FALSE)
    BOOL rename = FALSE;   // TRUE means rename/copy of a directory onto itself

    if (mode == 2) // the user entered a path in the standard dialog
    {
        // resolve relative paths ourselves (Salamander cannot do that)
        if ((targetPath[0] != '\\' || targetPath[1] != '\\') && // not an UNC path
            (targetPath[0] == 0 || targetPath[1] != ':'))       // not a standard drive path
        {                                                       // so it is neither Windows nor archive syntax
            userPart = strchr(targetPath, ':');
            if (userPart == NULL) // the path has no FS name, therefore it is relative
            {                     // a relative path containing ':' is not allowed (it would look like a full FS path)

                // For disk paths it would be better to call SalGetFullName:
                // SalamanderGeneral->SalGetFullName(targetPath, &errTextID, Path, nextFocus) plus error handling.
                // Then we would only need to prepend the FS name to the resulting path.
                // Here we deliberately demonstrate a custom implementation (using SalRemovePointsFromPath, etc.):

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
                if (targetPath[0] == '\\') // "\\path" -> build root + newName
                {
                    s += rootLen;
                    int len = (int)strlen(targetPath + 1); // skip the leading '\\'
                    if (len + rootLen >= MAX_PATH)
                        tooLong = TRUE;
                    else
                    {
                        memcpy(s, targetPath + 1, len);
                        *(s + len) = 0;
                    }
                }
                else // "path" -> combine Path + newName
                {
                    int pathLen = (int)strlen(Path);
                    if (pathLen < rootLen)
                        rootLen = pathLen;
                    strcpy(s + rootLen, Path + rootLen); // the root is already copied in front
                    tooLong = !SalamanderGeneral->SalPathAppend(s, targetPath, MAX_PATH);
                }

                if (tooLong)
                {
                    SalamanderGeneral->SalMessageBox(parent, "Can't finish operation because of too long name.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    // return 'targetPath' unchanged (exactly as the user entered it)
                    return FALSE; // error -> re-open the standard dialog
                }

                strcpy(targetPath, path);
                userPart = targetPath + (userPart - path);
            }
            else
                userPart++;

            // FS target path ('targetPath' is the full path, 'userPart' points to the FS-specific segment)
            // This is the place where the plugin can process FS paths (its own or foreign ones).
            // Salamander cannot work with these paths yet; perhaps it will one day orchestrate basic operations
            // via TEMP (for example download from FTP into TEMP, then upload from TEMP to FTP - if a faster way
            // exists, such as native FTP transfers, the plugin should handle it here).

            if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
            { // this is DFS (otherwise let Salamander handle it normally)
                CDFSPathError err;
                BOOL invPath = !DFS_IsValidPath(userPart, &err);

                // The full path on this FS might still contain "." or ".." - strip them
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
                    // optionally 'err' could be displayed when invPath is TRUE; we ignore it here for simplicity
                    SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    // return 'targetPath' after expansion (some ".." and "." may be adjusted)
                    return FALSE; // error -> re-open the standard dialog
                }

                // trim any superfluous trailing backslash
                int l = (int)strlen(userPart);
                BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
                if (l > 1 && userPart[1] == ':') // a drive path such as "c:\path"
                {
                    if (l > 3) // not just the root
                    {
                        if (userPart[l - 1] == '\\')
                            userPart[l - 1] = 0; // drop the trailing backslash
                    }
                    else
                    {
                        userPart[2] = '\\'; // for a root path keep the backslash ("c:\")
                        userPart[3] = 0;
                    }
                }
                else // UNC path
                {
                    if (l > 0 && userPart[l - 1] == '\\')
                        userPart[l - 1] = 0; // drop the trailing backslash
                }

                // Analyze the path: find the existing and missing parts plus the operation mask.
                // Determine what portion already exists and whether it is a file or a directory,
                // then decide what kind of action this is:
                //   - writing to a path (possibly with a missing part) with an operation mask;
                //     the mask is the last non-existent segment of the path without a trailing backslash
                //     (for multiple source items ensure the mask contains '*' or at least '?', otherwise
                //     only a single destination name makes sense)
                //   - manual change of directory name case via Move (writing to the path that is also
                //     the source of the operation, i.e. focused/selected as the only item in the panel);
                //     names may differ only by letter casing
                //   - writing into an archive (the path contains an archive file or something else, in
                //     which case the error is "Salamander does not know how to open this file")
                //   - overwriting a file (the entire path is just the target file name; it must not end with a backslash)

                // Determine how much of the path exists (split it into existing and non-existing parts)
                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                char* end = targetPath + strlen(targetPath);
                char* afterRoot = userPart + rootLen;
                char lastChar = 0;
                BOOL pathIsDir = TRUE;
                BOOL pathError = FALSE;

                // If the path contains a mask, cut it off without calling SalGetFileAttributes
                if (end > afterRoot) // there is more than the root
                {
                    char* end2 = end;
                    BOOL cut = FALSE;
                    while (*--end2 != '\\') // at least one backslash must follow after the root
                    {
                        if (*end2 == '*' || *end2 == '?')
                            cut = TRUE;
                    }
                    if (cut) // the name contains a mask -> trim it
                    {
                        end = end2;
                        lastChar = *end;
                        *end = 0;
                    }
                }

                while (end > afterRoot) // there is still more than the root
                {
                    DWORD attrs = SalamanderGeneral->SalGetFileAttributes(userPart);
                    if (attrs != 0xFFFFFFFF) // this part of the path exists
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // it is a file
                        {
                            // An existing path must not contain a file name (see SalSplitGeneralPath); trim it.
                            *end = lastChar;   // restore 'targetPath'
                            pathIsDir = FALSE; // the existing part of the path is a file
                            while (*--end != '\\')
                                ;            // there is at least one backslash after the root
                            lastChar = *end; // keep the path intact
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
                            err2 != ERROR_DIRECTORY) // unexpected error -> report it
                        {
                            sprintf(buf, "Path: %s\nError: %s", targetPath,
                                    SalamanderGeneral->GetErrorText(err2, errBuf, MAX_PATH));
                            SalamanderGeneral->SalMessageBox(parent, buf, errTitle, MB_OK | MB_ICONEXCLAMATION);
                            pathError = TRUE;
                            break; // report the error
                        }
                    }

                    *end = lastChar; // restore 'targetPath'
                    while (*--end != '\\')
                        ; // there is guaranteed to be at least one backslash after the root
                    lastChar = *end;
                    *end = 0;
                }
                *end = lastChar; // repair 'targetPath'
                SetCursor(oldCur);

                if (!pathError) // splitting succeeded without errors
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
                        if (newDirs[0] != 0) // the target path needs new subdirectories created
                        {
                            // NOTE: if creating subdirectories on the target path is not supported,
                            //       pass newDirs==NULL to SalSplitGeneralPath(); it will report the error itself

                            // NOTE: if the path were created here, PostChangeOnPathNotification would have to be
                            //       called (it is processed later, so ideally call it immediately after creating the
                            //       path rather than at the end of the operation)

                            SalamanderGeneral->SalMessageBox(parent, "Sorry, but creating of target path is not supported.",
                                                             errTitle, MB_OK | MB_ICONEXCLAMATION);
                            char* e = targetPath + strlen(targetPath); // repair 'targetPath' (join 'targetPath' and 'mask')
                            if (e > targetPath && *(e - 1) != '\\')
                                *e++ = '\\';
                            if (e != mask)
                                memmove(e, mask, strlen(mask) + 1); // slide the mask into place when needed
                            pathError = TRUE;
                        }
                        else
                        {
                            if (dirName != NULL && curPath != NULL && SalamanderGeneral->StrICmp(dirName, mask) == 0 &&
                                DFS_IsTheSamePath(targetPath, curPath))
                            {
                                // rename/copy of a directory onto itself (differing only by letter case) – "change-case".
                                // Do not treat this as an operation mask (the supplied target path exists; splitting into
                                // the mask is the result of the analysis).

                                rename = TRUE;
                            }

                            /*
              // the following code handles the situation when the FS does not support operation masks
              if (mask != NULL && (strcmp(mask, "*.*") == 0 || strcmp(mask, "*") == 0))
              {  // masks are unsupported and the mask is empty -> cut it off
                *mask = 0;  // double-null terminated
              }
              if (!rename)  // for rename this is not an error
              {
                if (mask != NULL && *mask != 0)  // the mask exists but is not allowed
                {
                  char *e = targetPath + strlen(targetPath);   // fix 'targetPath' (join 'targetPath' and 'mask')
                  if (e > targetPath && *(e - 1) != '\\') *e++ = '\\';
                  if (e != mask) memmove(e, mask, strlen(mask) + 1);  // shift the mask if needed

                  SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support operation masks (target "
                                                   "path must exist or end on backslash)", errTitle,
                                                   MB_OK | MB_ICONEXCLAMATION);
                  pathError = TRUE;
                }
              }
*/

                            if (!pathError)
                                diskPath = FALSE; // the path for this FS was successfully analyzed
                        }
                    }
                    else
                        pathError = TRUE;
                }

                if (pathError)
                {
                    // return 'targetPath' after adjustment (expansion of the path and possible tweaks to ".." and ".")
                    return FALSE; // error -> re-open the standard dialog
                }
            }
        }

        if (diskPath)
        {
            // Windows path, archive path, or an unknown FS -> let Salamander handle the standard processing
            operationMask = TRUE; // operation masks are supported
            cancelOrHandlePath = TRUE;
            return FALSE; // let Salamander process the path
        }
    }

    const char* opMask = NULL; // operation mask
    if (mode == 5)             // the operation target was specified via drag & drop
    {
        // If this is a disk path, set the operation mask and continue (same as mode==3).
        // For an archive path, show "not supported"; for a DFS path set diskPath=FALSE and compute userPart
        // (points into the DFS user-part). For other FS paths report "not supported".

        BOOL ok = FALSE;
        opMask = "*.*";
        int type;
        char* secondPart;
        BOOL isDir;
        if (targetPath[0] != 0 && targetPath[1] == ':' ||   // disk path (C:\path)
            targetPath[0] == '\\' && targetPath[1] == '\\') // UNC path (\\server\share\path)
        {                                                   // ensure the trailing backslash so it's always a path (mode 5 always passes a path)
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
                else // another FS -> report "not supported"
                {
                    SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support copying nor moving to other "
                                                             "plugin file-systems.",
                                                     errTitle,
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
                break;
            }

            //case PATH_TYPE_ARCHIVE:
            default: // archive -> report "not supported"
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

    // 'mode' is 2, 3, or 5

    /*
  // example of using the wait window - useful e.g. when reading names that should be copied
  // (preparation for overall progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // fetch the "Confirm on" configuration values
    BOOL ConfirmOnFileOverwrite, ConfirmOnDirOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMDIROVER, &ConfirmOnDirOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);
    // if path analysis with optional creation of missing subdirectories were performed here,
    // SALCFG_CNFRMCREATEPATH would also come in handy (show "do you want to create target path?")

    // determine the operation mask (the destination path is stored in 'targetPath')
    if (opMask == NULL)
    {
        opMask = targetPath;
        while (*opMask != 0)
            opMask++;
        opMask++;
    }

    /*  // description of the operation destination gathered in the previous code:
  if (diskPath)  // 'targetPath' is a Windows path, 'opMask' is the operation mask
  {
  }
  else   // 'targetPath' is a path on this FS ('userPart' points to the FS user-part path), 'opMask' is the operation mask
  {
    // if 'rename' is TRUE we are renaming/copying a directory into itself
  }
*/

    // prepare buffers for names
    char sourceName[MAX_PATH]; // buffer with the full disk name (DFS operations use disk files)
    strcpy(sourceName, Path);
    char* endSource = sourceName + strlen(sourceName); // space reserved for names from the panel
    if (endSource - sourceName < MAX_PATH - 1 && endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // maximum number of characters available for a panel name

    char dfsSourceName[2 * MAX_PATH]; // full DFS name buffer (used when looking up the source in the disk cache)
    sprintf(dfsSourceName, "%s:%s", fsName, sourceName);
    // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
    // to lowercase makes the disk cache behave case-insensitively as well
    SalamanderGeneral->ToLowerCase(dfsSourceName);
    char* endDFSSource = dfsSourceName + strlen(dfsSourceName);                // space reserved for names from the panel
    int endDFSSourceSize = 2 * MAX_PATH - (int)(endDFSSource - dfsSourceName); // maximum number of characters available for a panel name

    char targetName[MAX_PATH]; // buffer with the full disk name (when the target lies on disk)
    targetName[0] = 0;
    char* endTarget = targetName;
    int endTargetSize = MAX_PATH;
    if (diskPath)
    {
        strcpy(targetName, targetPath);
        endTarget = targetName + strlen(targetName); // space reserved for the destination name
        if (endTarget - targetName < MAX_PATH - 1 && endTarget > targetName && *(endTarget - 1) != '\\')
        {
            *endTarget++ = '\\';
            *endTarget = 0;
        }
        endTargetSize = MAX_PATH - (int)(endTarget - targetName); // maximum number of characters available for a panel name
    }

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;                     // FALSE if an error occurs or the user cancels
    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE if the source path changed (move operation)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE if source subdirectories changed as well
    BOOL targetPathChanged = FALSE;          // TRUE if the target path changed
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE if target subdirectories changed as well

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
        // fetch data for the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // perform the copy/move on the file or directory
        if (f != NULL)
        {
            // assemble the full name; trimming to MAX_PATH is theoretically unnecessary but unfortunately required
            lstrcpyn(endSource, f->Name, endSourceSize);
            lstrcpyn(endDFSSource, f->Name, endDFSSourceSize);
            // filenames on disk are case-insensitive, the disk cache is case-sensitive, converting
            // to lowercase makes the disk cache behave case-insensitively as well
            SalamanderGeneral->ToLowerCase(endDFSSource);

            if (isDir) // directory
            {
                // DEMOPLUG does not implement directory operations (recursion would need either
                // processing items sequentially without overall progress or scripting with total progress tracking)

                // progress reporting should also be handled here (count processed/skipped files/directories)

                // report changes on the source and destination paths:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;
            }
            else // file
            {
                BOOL skip = FALSE;
                if (diskPath) // Windows destination path
                {
                    // compose the destination name - simplified without handling the "Can't finish operation because of too long name" error
                    lstrcpyn(endTarget, SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, f->Name, opMask),
                             endTargetSize);

                    const char* tmpName;
                    BOOL fileFromCache = SalamanderGeneral->GetFileFromCache(dfsSourceName, tmpName, fileLock);
                    if (!fileFromCache) // the file is not in the disk cache
                    {
                        // copy the file directly from DFS
                        // the demo plug-in does not handle overwriting files; real code should confirm overwrites here
                        // (the ConfirmOnFileOverwrite and ConfirmOnSystemHiddenFileOverwrite flags apply)
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
                                break; // copied successfully
                            }
                            if (!success || skip)
                                break;
                        }

                        // if this is not a move (the source remains), nothing was skipped or canceled, and the destination is a Windows path,
                        // add the file to the disk cache (if it is not larger than 1 MB - ideally configurable,
                        // which the demo plug-in leaves unimplemented
                        if (success && copy && !skip && f->Size <= CQuadWord(1048576, 0))
                        {
                            // copy the file into the TEMP directory and move it to the disk cache
                            // errors are ignored; the file simply is not cached
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

                                if (err != 0) // disk-cache save failed, remove the TEMP file
                                {
                                    // clear the read-only attribute so the temporary copy can be deleted
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
                                    break; // not an error, just a concurrency case (e.g. View and Copy)
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
                    else // the file is stored in the disk cache
                    {
                        // copy the file from the disk cache
                        // the demo plug-in does not handle overwriting files; real code should confirm overwrites here
                        // (the ConfirmOnFileOverwrite and ConfirmOnSystemHiddenFileOverwrite flags apply)
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
                                break; // copied successfully
                            }
                            if (!success || skip)
                                break;
                        }

                        // unlock the cached file copy
                        SalamanderGeneral->UnlockFileInCache(fileLock);
                    }

                    if (success && !copy && !skip) // this is a move and the file was not skipped -> delete the source file
                    {
                        // delete the file on the DFS
                        while (1)
                        {
                            // allow deletion of read-only items
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
                                // remove the deleted file's copy from the disk cache (if it is cached)
                                if (fileFromCache)
                                {
                                    SalamanderGeneral->RemoveOneFileFromCache(dfsSourceName);
                                }
                                sourcePathChanged = TRUE;
                                // subdirsOfSourcePathChanged = TRUE;

                                break; // delete succeeded
                            }
                            if (!success || skip)
                                break;
                        }
                    }
                }
                else // DFS destination path
                {
                    // if 'rename' is TRUE we are renaming/copying a directory into itself

                    // DEMOPLUG does not implement operations within DFS (no disk cache; entirely up to the FS)
                }

                // report changes on the source and destination paths:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;

                if (success)
                {

                    // progress handling belongs here (add after processing/skipping a single file)
                }
            }
        }

        // determine whether it makes sense to continue (if not canceled and another selected item exists)
        if (!success || focused || f == NULL)
            break;
    }
    HANDLES(CloseHandle(fileLock));

    // change on the source path 'Path' (mainly for move operations)
    if (sourcePathChanged)
    {
        // NOTE: a typical plugin should send the full FS path here
        // (for DFS we leverage the fact it works with disk paths and send only
        // the raw disk path; this cannot be used for other FS types)
        SalamanderGeneral->PostChangeOnPathNotification(Path, subdirsOfSourcePathChanged);
    }
    // change on the destination path 'targetPath' (may be a path on our FS or on disk)
    if (targetPathChanged)
    {
        SalamanderGeneral->PostChangeOnPathNotification(targetPath, subdirsOfTargetPathChanged);
    }

    if (success)
        strcpy(targetPath, nextFocus); // success
    else
        cancelOrHandlePath = TRUE; // error/cancel
    return TRUE;                   // success or error/cancel
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
        // append the *.* mask to the destination path (operation masks will be processed)
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
        // 'targetPath' contains the raw path entered by the user (all we know is that it
        // belongs to this FS, otherwise Salamander would not call this method)
        char* userPart = strchr(targetPath, ':') + 1; // v 'targetPath' musi byt fs-name + ':'

        CDFSPathError err;
        BOOL invPath = !DFS_IsValidPath(userPart, &err);

        // check whether the operation can be performed in this FS; also remove any "." and ".."
        // that the user may have used in the full path on this FS
        int rootLen = 0;
        if (!invPath)
        {
            if (Path[0] != 0 && // not a newly opened FS (it has a current path)
                !SalamanderGeneral->HasTheSameRootPath(Path, userPart))
            {
                return FALSE; // DemoPlug: the operation cannot be performed in this FS (different disk root)
            }

            rootLen = SalamanderGeneral->GetRootPath(buf, userPart);
            int userPartLen = (int)strlen(userPart);
            if (userPartLen < rootLen)
                rootLen = userPartLen;
        }
        if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
        {
            // additionally we could display 'err' when 'invPath' is TRUE; ignored here for simplicity
            SalamanderGeneral->SalMessageBox(parent, "The path specified is invalid.",
                                             errTitle, MB_OK | MB_ICONEXCLAMATION);
            // 'targetPath' is returned after any ".." and "." adjustments
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return FALSE; // let the user correct the path
        }

        // trim the redundant backslash
        int l = (int)strlen(userPart);
        BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
        if (l > 1 && userPart[1] == ':') // path type "c:\path"
        {
            if (l > 3) // not a root path
            {
                if (userPart[l - 1] == '\\')
                    userPart[l - 1] = 0; // trim trailing backslashes
            }
            else
            {
                userPart[2] = '\\'; // root path, backslash required ("c:\")
                userPart[3] = 0;
            }
        }
        else // UNC path
        {
            if (l > 0 && userPart[l - 1] == '\\')
                userPart[l - 1] = 0; // trim trailing backslashes
        }

        // analyze the path - find the existing part, the missing part, and the operation mask
        //
        // - determine which part of the path exists and whether it is a file or directory,
        //   then decide what the operation is:
        //   - write to the path (possibly with a missing segment) using a mask - the mask is the last nonexistent
        //     part of the path without a trailing backslash (verify that multiple source files/directories
        //     have '*' or at least '?' in the mask; otherwise it makes no sense -> only one destination name)
        //   - write into an archive (the path contains an archive file or it may not even be an archive,
        //     resulting in the "Salamander does not know how to open this file" error)
        //   - overwrite a file (the entire path is just the destination file name; must not end with a backslash)

        // detect how much of the path already exists (split into existing and non-existing segments)
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        char* end = targetPath + strlen(targetPath);
        char* afterRoot = userPart + rootLen;
        char lastChar = 0;
        BOOL pathIsDir = TRUE;
        BOOL pathError = FALSE;

        // if the path contains a mask, trim it without calling SalGetFileAttributes
        if (end > afterRoot) // not down to just the root yet
        {
            char* end2 = end;
            BOOL cut = FALSE;
            while (*--end2 != '\\') // there is guaranteed to be at least one '\\' past the root
            {
                if (*end2 == '*' || *end2 == '?')
                    cut = TRUE;
            }
            if (cut) // the name contains a mask -> trim it
            {
                end = end2;
                lastChar = *end;
                *end = 0;
            }
        }

        while (end > afterRoot) // not down to just the root yet
        {
            DWORD attrs = SalamanderGeneral->SalGetFileAttributes(userPart);
            if (attrs != 0xFFFFFFFF) // this part of the path exists
            {
                if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // it is a file
                {
                    // an existing path must not include a file name (see SalSplitGeneralPath) -> trim it
                    *end = lastChar;   // restore 'targetPath'
                    pathIsDir = FALSE; // the existing part of the path is a file
                    while (*--end != '\\')
                        ;            // there is guaranteed to be at least one '\\' past the root
                    lastChar = *end; // keep the path intact
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
                    err2 != ERROR_DIRECTORY) // unusual error - just display it
                {
                    sprintf(buf, "Path: %s\nError: %s", targetPath,
                            SalamanderGeneral->GetErrorText(err2, errBuf, MAX_PATH));
                    SalamanderGeneral->SalMessageBox(parent, buf, errTitle, MB_OK | MB_ICONEXCLAMATION);
                    pathError = TRUE;
                    break; // report the error
                }
            }

            *end = lastChar; // restore 'targetPath'
            while (*--end != '\\')
                ; // there is guaranteed to be at least one '\\' past the root
            lastChar = *end;
            *end = 0;
        }
        *end = lastChar; // fix 'targetPath'
        SetCursor(oldCur);

        char* opMask = NULL;
        if (!pathError) // the split succeeded without errors
        {
            if (*end == '\\')
                end++;

            char newDirs[MAX_PATH];
            if (SalamanderGeneral->SalSplitGeneralPath(parent, title, errTitle, sourceFiles + sourceDirs,
                                                       targetPath, afterRoot, end, pathIsDir,
                                                       backslashAtEnd, NULL, NULL, opMask, newDirs,
                                                       NULL /* 'isTheSamePathF' not needed */))
            {
                if (newDirs[0] != 0) // the destination path needs new subdirectories created
                {
                    // NOTE: if creating subdirectories on the destination path is unsupported, just pass
                    //       'newDirs'==NULL to SalSplitGeneralPath(); it will report the error itself

                    // NOTE: if the path were created here, PostChangeOnPathNotification would have to be called
                    //       (handled later, so ideally call it right after the path is created, not after
                    //       the entire operation finishes)

                    SalamanderGeneral->SalMessageBox(parent, "Sorry, but creating of target path is not supported.",
                                                     errTitle, MB_OK | MB_ICONEXCLAMATION);
                    char* e = targetPath + strlen(targetPath); // fix 'targetPath' (join 'targetPath' and 'opMask')
                    if (e > targetPath && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != opMask)
                        memmove(e, opMask, strlen(opMask) + 1); // shift the mask if needed
                    pathError = TRUE;
                }
                else
                {
                    /*
          // the following code handles the situation when the FS does not support operation masks
          if (opMask != NULL && (strcmp(opMask, "*.*") == 0 || strcmp(opMask, "*") == 0))
          {  // masks are unsupported and the mask is empty -> cut it off
            *opMask = 0;  // double-null terminated
          }
          if (opMask != NULL && *opMask != 0)  // the mask exists but is not allowed
          {
            char *e = targetPath + strlen(targetPath);   // fix 'targetPath' by joining it with 'opMask'
            if (e > targetPath && *(e - 1) != '\\') *e++ = '\\';
            if (e != opMask) memmove(e, opMask, strlen(opMask) + 1);  // shift the mask if necessary

            SalamanderGeneral->SalMessageBox(parent, "DFS doesn't support operation masks (target "
                                             "path must exist or end on backslash)", errTitle,
                                             MB_OK | MB_ICONEXCLAMATION);
            pathError = TRUE;
          }
*/

                    // if 'pathError' is FALSE, the target path was successfully analyzed
                }
            }
            else
                pathError = TRUE;
        }

        if (pathError)
        {
            // 'targetPath' is returned after resolving ".." and "." plus any appended mask
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return FALSE; // path error - let the user correct it
        }

        /*
    // example of using the wait window - useful when reading the names to copy
    // (preparation for overall progress) - the directory structure is read on the first call to
    // the 'next' function (for enumFiles == 1 or 2)
    SalamanderGeneral->CreateSafeWaitWindow("Reading disk path structure, please wait...", NULL,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    Sleep(2000);  // simulate some work
    SalamanderGeneral->DestroySafeWaitWindow();
  */

        // load the "Confirm on" configuration values
        BOOL ConfirmOnFileOverwrite, ConfirmOnDirOverwrite, ConfirmOnSystemHiddenFileOverwrite;
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMDIROVER, &ConfirmOnDirOverwrite, 4, NULL);
        SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);
        // if path analysis with optional creation of missing subdirectories were performed here,
        // we would also use SALCFG_CNFRMCREATEPATH (show "do you want to create target path?")

        // description of the operation destination gathered above:
        // 'targetPath' is a path on this FS ('userPart' points to the FS user-part path), 'opMask' is the operation mask

        // prepare buffers for names
        char sourceName[MAX_PATH]; // buffer for the full on-disk name
        strcpy(sourceName, sourcePath);
        char* endSource = sourceName + strlen(sourceName); // space for names provided by 'next'
        if (endSource > sourceName && *(endSource - 1) != '\\')
        {
            *endSource++ = '\\';
            *endSource = 0;
        }
        int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // maximum number of characters for a 'next' name

        char targetName[MAX_PATH]; // buffer for the full destination name on disk (DFS works with disk files)
        strcpy(targetName, userPart);
        char* endTarget = targetName + strlen(targetName); // space reserved for the destination name
        if (endTarget > targetName && *(endTarget - 1) != '\\')
        {
            *endTarget++ = '\\';
            *endTarget = 0;
        }
        int endTargetSize = MAX_PATH - (int)(endTarget - targetName); // maximum number of characters for the destination name

        BOOL success = TRUE;                     // FALSE if an error occurs or the user cancels
        BOOL skipAllErrors = FALSE;              // skip all errors
        BOOL sourcePathChanged = FALSE;          // TRUE if the source path changed (move operation)
        BOOL subdirsOfSourcePathChanged = FALSE; // TRUE if source subdirectories changed as well
        BOOL targetPathChanged = FALSE;          // TRUE if the target path changed
        BOOL subdirsOfTargetPathChanged = FALSE; // TRUE if target subdirectories changed as well

        BOOL isDir;
        const char* name;
        const char* dosName; // dummy
        CQuadWord size;
        DWORD attr;
        FILETIME lastWrite;
        while ((name = next(NULL, 0, &dosName, &isDir, &size, &attr, &lastWrite, nextParam, NULL)) != NULL)
        { // perform the copy/move on a file or directory
            // assemble the full name; trimming to MAX_PATH is theoretically unnecessary but unfortunately required
            lstrcpyn(endSource, name, endSourceSize);

            if (isDir) // directory
            {
                // DEMOPLUG does not implement directory operations (recursion would need either
                // processing items sequentially without overall progress or scripting with total progress tracking)

                // progress reporting should also be handled here (count processed/skipped files/directories)

                // reporting changes on the source and destination paths:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;
            }
            else // file
            {
                BOOL skip = FALSE;
                // compose the target name - simplified without handling the "Can't finish operation because of too long name" error
                // ('name' comes only from the root of the source path - no subdirectories - we apply the mask to the entire 'name')
                lstrcpyn(endTarget, SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, (char*)name, opMask),
                         endTargetSize);

                // copy the file directly to the DFS
                // the demo plug-in does not handle overwriting files; real code should confirm overwrites here
                // (the ConfirmOnFileOverwrite and ConfirmOnSystemHiddenFileOverwrite flags apply)
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
                        break; // copied successfully
                    }
                    if (!success || skip)
                        break;
                }

                if (success && !copy && !skip) // we are doing a move and the file was not skipped -> delete the source file
                {
                    // remove the file from disk
                    while (1)
                    {
                        // allow deletion of read-only items
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

                            break; // delete succeeded
                        }
                        if (!success || skip)
                            break;
                    }
                }

                // reporting changes on the source and destination paths:
                // sourcePathChanged = !copy;
                // subdirsOfSourcePathChanged = TRUE;
                // targetPathChanged = TRUE;
                // subdirsOfTargetPathChanged = TRUE;

                if (success)
                {

                    // progress handling belongs here (add after processing/skipping a single file)
                }
            }

            // determine whether it makes sense to continue (if not canceled)
            if (!success)
                break;
        }

        // changes on the source path (especially for move operations)
        if (sourcePathChanged)
        {
            SalamanderGeneral->PostChangeOnPathNotification(sourcePath, subdirsOfSourcePathChanged);
        }
        // changes on the destination path (normally 'targetPath' on the FS, but DFS uses disk paths,
        // so we report the change directly on the disk path 'userPart')
        if (targetPathChanged)
        {
            SalamanderGeneral->PostChangeOnPathNotification(userPart, subdirsOfTargetPathChanged);
        }

        if (success)
            return TRUE; // operation finished successfully
        else
        {
            if (invalidPathOrCancel != NULL)
                *invalidPathOrCancel = TRUE;
            return TRUE; // cancellation requested
        }
    }

    return FALSE; // unknown 'mode'
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

    // show the custom dialog (not implemented - no attribute changes are performed; depends on the FS)
    MessageBox(parent, "Here should user specify how to change attributes. "
                       "It's not implemented in DemoPlug.",
               title, MB_OK | MB_ICONINFORMATION);

    /*
  // example of using the wait window - useful when reading names (preparation for overall progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NULL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // prepare a buffer for names
    char name[MAX_PATH]; // buffer with the full disk name (DFS operations use disk files)
    strcpy(name, Path);
    char* end = name + strlen(name); // space reserved for names from the panel
    if (end > name && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - name); // maximum number of characters available for a panel name

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;               // FALSE if an error occurs or the user cancels
    BOOL skipAllErrors = FALSE;        // skip all errors
    BOOL pathChanged = FALSE;          // TRUE if the path changed
    BOOL subdirsOfPathChanged = FALSE; // TRUE if subdirectories of the path changed as well

    while (1)
    {
        // fetch data for the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // perform the operation on the file or directory
        if (f != NULL)
        {
            // assemble the full name; trimming to MAX_PATH is theoretically unnecessary but unfortunately required
            lstrcpyn(end, f->Name, endSize);

            // performing the attribute change is not implemented here

            // reporting changes on the source path:
            // pathChanged = TRUE;
            // subdirsOfPathChanged = TRUE;

            if (success)
            {

                // progress handling belongs here (add after processing/skipping a single file)
            }
        }

        // determine whether it makes sense to continue (if not canceled and if more items are selected)
        if (!success || focused || f == NULL)
            break;
    }

    // change on the source path 'Path'
    if (pathChanged)
    {
        // NOTE: a typical plugin should send the full FS path here
        SalamanderGeneral->PostChangeOnPathNotification(Path, subdirsOfPathChanged);
    }

    //  return success;
    return FALSE; // cancellation requested
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
  // example of using the wait window - useful e.g. when reading names (preparation for overall progress)
  SalamanderGeneral->CreateSafeWaitWindow("Reading DFS path structure, please wait...", NILL,
                                          500, FALSE, SalamanderGeneral->GetMainWindowHWND());
  Sleep(2000);  // simulate some work
  SalamanderGeneral->DestroySafeWaitWindow();
*/

    // prepare buffers for names
    char name[MAX_PATH]; // buffer with the full disk name (DFS operations use disk files)
    strcpy(name, Path);
    char* end = name + strlen(name); // space reserved for names from the panel
    if (end > name && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - name); // maximum number of characters available for a panel name

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;        // FALSE if an error occurs or the user cancels
    BOOL skipAllErrors = FALSE; // skip all errors

    while (1)
    {
        // fetch data for the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // perform the property query on the file or directory
        if (f != NULL)
        {
            // assemble the full name; trimming to MAX_PATH is theoretically unnecessary but unfortunately required
            lstrcpyn(end, f->Name, endSize);

            // retrieving the attributes is not implemented here

            if (success)
            {

                // progress handling belongs here (add after processing/skipping a single file)
            }
        }

        // determine whether it makes sense to continue (if not canceled and another selected item exists)
        if (!success || focused || f == NULL)
            break;
    }

    if (success)
    {
        // show the actual dialog (not implemented; depends on the FS)
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
    case fscmItemsInPanel: // context menu for panel items (selected/focused files and directories)
    {
        int i = 0;

        // insert Salamander commands
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
            if (type2 != lastType /*&& lastType != sctyUnknown*/) // insert a separator
            {
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE;
                mi.fType = MFT_SEPARATOR;
                InsertMenuItem(menu, i++, TRUE, &mi);
            }
            lastType = type2;

            // insert Salamander commands
            memset(&mi, 0, sizeof(mi));
            mi.cbSize = sizeof(mi);
            mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
            mi.fType = MFT_STRING;
            mi.wID = salCmd + 1000; // shift Salamander commands by 1000 so they differ from ours
            mi.dwTypeData = nameBuf;
            mi.cch = (UINT)strlen(nameBuf);
            mi.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
            InsertMenuItem(menu, i++, TRUE, &mi);
        }
        DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                     menuX, menuY, parent, NULL);
        if (cmd != 0) // the user selected a command from the menu
        {
            if (cmd >= 1000)
            {
                if (SalamanderGeneral->GetSalamanderCommand(cmd - 1000, nameBuf, 200, &enabled, &type2))
                {
                    TRACE_I("Starting command: " << nameBuf);
                }

                SalamanderGeneral->PostSalamanderCommand(cmd - 1000);
            }
            else // our own command
            {
                TRACE_I("Starting command: Always");
                SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // execute later in "sal-idle"
                                                                  /*
          SalamanderGeneral->PostMenuExtCommand(cmd, FALSE); // run once the main window receives the message
          // WARNING: after this call no window with a message loop may open,
          // otherwise the plugin command runs before this method finishes!
*/
            }
        }
        break;
    }

    case fscmPathInPanel: // context menu for the current path in the panel
    {
        int i = 0;

        // insert Salamander commands
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
        if (cmd != 0)                                         // the user selected a command from the menu
            SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // execute later in "sal-idle"
        break;
    }

    case fscmPanel: // context menu for the panel
    {
        int i = 0;

        // insert Salamander commands
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
        if (cmd != 0)                                         // the user selected a command from the menu
            SalamanderGeneral->PostMenuExtCommand(cmd, TRUE); // execute later in "sal-idle"
        break;
    }
    }
    DestroyMenu(menu);
}
