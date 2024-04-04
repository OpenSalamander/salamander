// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "stswnd.h"
#include "editwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "usermenu.h"
#include "mainwnd.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "execute.h"
#include "cache.h"
#include "toolbar.h"
#include "salinflt.h"
#include "menu.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "zip.h"

BOOL ImageDragging = FALSE;
BOOL ImageDraggingVisible = FALSE;
BOOL ShowCaretAfterDrop = FALSE;
int ImageDraggingVisibleLevel = 0;
int ImageDragX = INT_MAX;
int ImageDragY = INT_MAX;
int ImageDragW = INT_MAX;
int ImageDragH = INT_MAX;
int ImageDragDxHotspot = INT_MAX;
int ImageDragDyHotspot = INT_MAX;

//****************************************************************************
//
// CMainWindow
//

void RecursiveFindAndCopy(char* srcPath, char* dstPath, char** fromBuf, char** toBuf, int* freeBufNames);

void CMainWindow::ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs)
{
    DirHistory->ClearPluginFSFromHistory(fs);
}

void CMainWindow::DirHistoryAddPathUnique(int type, const char* pathOrArchiveOrFSName,
                                          const char* archivePathOrFSUserPart, HICON hIcon,
                                          CPluginFSInterfaceAbstract* pluginFS,
                                          CPluginFSInterfaceEncapsulation* curPluginFS)
{
    if (CanAddToDirHistory)
    {
        DirHistory->AddPathUnique(type, pathOrArchiveOrFSName, archivePathOrFSUserPart, hIcon,
                                  pluginFS, curPluginFS);
        if (LeftPanel != NULL)
            LeftPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
        if (RightPanel != NULL)
            RightPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
    }
    else
    {
        if (hIcon != NULL)
            HANDLES(DestroyIcon(hIcon));
    }
}

void CMainWindow::DirHistoryRemoveActualPath(CFilesWindow* panel)
{
    if (panel->Is(ptZIPArchive))
    {
        DirHistory->RemoveActualPath(1, panel->GetZIPArchive(), panel->GetZIPPath(), NULL, NULL);
    }
    else
    {
        if (panel->Is(ptDisk))
        {
            DirHistory->RemoveActualPath(0, panel->GetPath(), NULL, NULL, NULL);
        }
        else
        {
            if (panel->Is(ptPluginFS))
            {
                char curPath[MAX_PATH];
                if (panel->GetPluginFS()->NotEmpty() && panel->GetPluginFS()->GetCurrentPath(curPath))
                {
                    DirHistory->RemoveActualPath(2, panel->GetPluginFS()->GetPluginFSName(), curPath,
                                                 panel->GetPluginFS()->GetInterface(), panel->GetPluginFS());
                }
            }
        }
    }
    if (LeftPanel != NULL)
        LeftPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
    if (RightPanel != NULL)
        RightPanel->DirectoryLine->SetHistory(DirHistory->HasPaths());
}

void CMainWindow::GetSplitRect(RECT& r)
{
    r.left = SplitPositionPix;
    r.top = TopRebarHeight;
    r.right = SplitPositionPix + MainWindow->GetSplitBarWidth();
    r.bottom = WindowHeight - EditHeight - BottomToolBarHeight;
}

void CMainWindow::GetWindowSplitRect(RECT& r)
{
    GetClientRect(HWindow, &r);
    r.top = TopRebarHeight;
    r.bottom = WindowHeight - EditHeight - BottomToolBarHeight;
}

BOOL CMainWindow::PtInChild(HWND hChild, POINT p)
{
    if (hChild == NULL)
        return FALSE;
    RECT r;
    GetWindowRect(hChild, &r);
    MapWindowPoints(NULL, HWindow, (POINT*)&r, 2);
    return PtInRect(&r, p);
}

BOOL CMainWindow::CloseDetachedFS(HWND parent, CPluginFSInterfaceEncapsulation* detachedFS)
{
    CALL_STACK_MESSAGE1("CMainWindow::CloseDetachedFS()");
    BOOL dummy; // ignored return value
    if (!detachedFS->TryCloseOrDetach(CriticalShutdown, FALSE, dummy, FSTRYCLOSE_UNLOADCLOSEDETACHEDFS) &&
        !CriticalShutdown) // test closing, forceClose==TRUE only on "critical shutdown"
    {                      // Ask the user if they want to close it even against the will of the file system
        char path[2 * MAX_PATH];
        strcpy(path, detachedFS->GetPluginFSName());
        strcat(path, ":");
        char* s = path + strlen(path);
        if (!detachedFS->NotEmpty() || !detachedFS->GetCurrentPath(s))
            *s = 0; // unable to retrieve user-part

        char buf[2 * MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_FSFORCECLOSE), path);
        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user says "close"
        {
            detachedFS->TryCloseOrDetach(TRUE, FALSE, dummy, FSTRYCLOSE_UNLOADCLOSEDETACHEDFS);
        }
        else
            return FALSE; // user does not want to close disconnected FS
    }

    // close the file system
    CPluginInterfaceForFSEncapsulation plugin(detachedFS->GetPluginInterfaceForFS()->GetInterface(),
                                              detachedFS->GetPluginInterfaceForFS()->GetBuiltForVersion());
    if (plugin.NotEmpty())
    {
        detachedFS->ReleaseObject(parent);
        plugin.CloseFS(detachedFS->GetInterface());
    }
    else
        TRACE_E("Unexpected situation (2) in CMainWindow::CloseDetachedFS()");

    return TRUE; // FS is closed
}

BOOL CMainWindow::CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE1("CMainWindow::CanUnloadPlugin()");
    if (LeftPanel != NULL && !LeftPanel->CanUnloadPlugin(parent, plugin))
        return FALSE;
    if (RightPanel != NULL && !RightPanel->CanUnloadPlugin(parent, plugin))
        return FALSE;

    // we will find disconnected FS belonging to the 'plugin' plugin and try to close them
    int i;
    for (i = DetachedFSList->Count - 1; i >= 0; i--) // from the back, we will delete from the array (quadratic complexity)
    {
        CPluginFSInterfaceEncapsulation* detachedFS = DetachedFSList->At(i);
        if (detachedFS->GetPluginInterface() == plugin) // is the plug-in 'plugin'
        {
            if (CloseDetachedFS(parent, detachedFS))
            {
                DetachedFSList->Delete(i); // remove disconnected FS from DetachedFSList
                if (!DetachedFSList->IsGood())
                    DetachedFSList->ResetState();
            }
            else
                return FALSE; // unload cannot be performed (user does not want to close the disconnected FS plug-in)
        }
    }

    // Check if the data from the plugin is not in SalShExtPastedData (it could be there
    // passing panels leaving archives due to unloading of the plugin)
    if (!SalShExtPastedData.CanUnloadPlugin(parent, plugin))
        return FALSE; // cannot perform unload

    return TRUE; // unload is possible, all resources of the plug-in are released
}

void CMainWindow::MakeFileList()
{
    CALL_STACK_MESSAGE1("CMainWindow::MakeFileList()");

    BOOL files = FALSE; // cursor on a file or directory or selection
    BOOL upDir = FALSE; // presence of ".."

    CFilesWindow* panel = GetActivePanel();

    upDir = (panel->Dirs->Count != 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0);
    int caret = panel->GetCaretIndex();
    if (caret >= 0)
    {
        if (caret == 0)
        {
            if (!upDir)
                files = (panel->Dirs->Count + panel->Files->Count > 0);
            else
            {
                int count = panel->GetSelCount();
                if (count == 1)
                {
                    files = (panel->GetSel(0) == FALSE);
                }
                else
                    files = (count > 0);
            }
        }
        else
            files = TRUE;
    }

    if (!files)
        return;

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    BeginStopRefresh(); // He was snoring in his sleep

    CFileListDialog dlg(HWindow);
    if (dlg.Execute() == IDOK)
    {
        char fileName[MAX_PATH];

        switch (Configuration.FileListDestination)
        {
        case 0: // clipboard
        case 1: // viewer
        {
            if (!SalGetTempFileName(NULL, "MFL", fileName, TRUE))
            {
                DWORD err = GetLastError();
                char errorText[200 + 2 * MAX_PATH];
                sprintf(errorText, "%s\n\n%s", LoadStr(IDS_ERRORCREATINGTMPFILE),
                        GetErrorText(err));
                SalMessageBox(HWindow, errorText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                fileName[0] = 0;
            }
            break;
        }

        case 2: // file
        {
            strcpy(fileName, Configuration.FileListName);
            int errTextID;
            if (!SalGetFullName(fileName, &errTextID, GetActivePanel()->Is(ptDisk) ? GetActivePanel()->GetPath() : NULL, panel->NextFocusName))
            {
                SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                fileName[0] = 0;
            }
            break;
        }

        default:
        {
            TRACE_E("Unknown destination!");
            fileName[0] = 0;
        }
        }

        if (fileName[0] != 0)
        {
            BOOL append = (Configuration.FileListDestination == 2 && Configuration.FileListAppend);
            HANDLE hFile = HANDLES_Q(CreateFile(fileName, GENERIC_WRITE | GENERIC_READ,
                                                FILE_SHARE_READ, NULL,
                                                append ? OPEN_ALWAYS : CREATE_ALWAYS,
                                                FILE_FLAG_RANDOM_ACCESS,
                                                NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            {
                // set the pointer
                SetFilePointer(hFile, 0, NULL, append ? FILE_END : FILE_BEGIN);

                // write into the file data - for each file/directory insert one record
                BOOL deleteFile = TRUE;
                if (panel->MakeFileList(hFile))
                {
                    panel->SetSel(FALSE, -1, TRUE);                        // explicit override
                    PostMessage(panel->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                    deleteFile = FALSE;
                }

                if (!deleteFile && Configuration.FileListDestination == 0) // clipboard
                {
                    DWORD fileSize = GetFileSize(hFile, NULL);
                    if (fileSize != INVALID_FILE_SIZE && fileSize > 0)
                    {
                        SetFilePointer(hFile, 0, NULL, FILE_BEGIN);
                        char* buff = (char*)malloc(fileSize);
                        if (buff != NULL)
                        {
                            DWORD read;
                            if (ReadFile(hFile, buff, fileSize, &read, NULL))
                            {
                                CopyTextToClipboard(buff, fileSize, FALSE, NULL);
                            }
                            else
                            {
                                DWORD err = GetLastError();
                                SalMessageBox(HWindow, GetErrorText(err), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            free(buff);
                        }
                        else
                            TRACE_E(LOW_MEMORY);
                    }
                }
                HANDLES(CloseHandle(hFile));

                // if the target was clipboard, we will delete the tmp file
                if (deleteFile || Configuration.FileListDestination == 0) // clipboard
                    DeleteFile(fileName);
                else
                {
                    if (Configuration.FileListDestination == 1) // viewer
                    {
                        // the file will be shown in an internal browser, which will subsequently download the file
                        CSalamanderPluginInternalViewerData viewerData;
                        viewerData.Size = sizeof(viewerData);
                        viewerData.FileName = fileName;
                        viewerData.Mode = 0; // text mode
                        char title[200];
                        lstrcpyn(title, LoadStr(IDS_MAKEFILELIST_OUTPUT), 200);
                        viewerData.Caption = title;
                        viewerData.WholeCaption = TRUE;
                        int error;
                        if (!ViewFileInPluginViewer(NULL, &viewerData, TRUE, NULL, "mfl.txt", error))
                        {
                            // file is deleted even in case of failure
                        }
                    }
                }

                if (Configuration.FileListDestination == 2) // file
                {
                    //--- refresh non-automatically refreshed directories
                    // change in the directory where the list of files was created
                    CutDirectory(fileName);
                    MainWindow->PostChangeOnPathNotification(fileName, FALSE);
                }
            }
            else
            {
                DWORD err = GetLastError();
                char message[MAX_PATH + 100];
                sprintf(message, LoadStr(IDS_FILEERRORFORMAT), fileName, GetErrorText(err));
                SalMessageBox(HWindow, message, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
    EndStopRefresh(); // now he's sniffling again, he'll start up
}

// description see mainwnd.h
BOOL GetNextFileFromPanel(int index, char* path, char* name, void* param)
{
    CALL_STACK_MESSAGE2("GetNextFileFromPanel(%d, , ,)", index);
    CUMDataFromPanel* data = (CUMDataFromPanel*)param;
    if (data->Count == -1) // data acquisition
    {
        BOOL upDir = (data->Window->Dirs->Count != 0 &&
                      strcmp(data->Window->Dirs->At(0).Name, "..") == 0);
        data->Count = data->Window->GetSelCount();
        if (data->Count < 0)
            data->Count = 0;
        if (data->Count == 0) // without marking -> we take focus
        {
            index = data->Window->GetCaretIndex();
            data->Index = NULL;
            strcpy(path, data->Window->GetPath());
            if (index < 0 || index >= data->Window->Dirs->Count + data->Window->Files->Count ||
                index == 0 && upDir)
            {
                name[0] = 0; // for up-dir or for the first item of an empty panel, the name will be empty...
            }
            else // copy the name to others
            {
                CFileData* f = &((index < data->Window->Dirs->Count) ? data->Window->Dirs->At(index) : data->Window->Files->At(index - data->Window->Dirs->Count));
                strcpy(name, f->Name);
            }
            return TRUE;
        }
        data->Index = new int[data->Count];
        if (data->Index == NULL)
            return FALSE; // error
        data->Window->GetSelItems(data->Count, data->Index);
    }
    if (index >= 0 && index < data->Count)
    {
        CFileData* f = &((data->Index[index] < data->Window->Dirs->Count) ? data->Window->Dirs->At(data->Index[index]) : data->Window->Files->At(data->Index[index] - data->Window->Dirs->Count));
        strcpy(path, data->Window->GetPath());
        strcpy(name, f->Name);
        return TRUE;
    }
    else
    {
        if (data->Index != NULL)
        {
            delete[] (data->Index);
            data->Index = NULL;
        }
        data->Window->SetSel(FALSE, -1, TRUE);                        // explicit override
        PostMessage(data->Window->HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
        return FALSE;
    }
}

BOOL CheckIfCanBeExecuted(BOOL buildBat, int commandLen, int argumentsLen)
{
    /*  MEASURED VALUES:
  Batak:
    W2K: 2041 including the name of the exaction, space, and parameters
    XP64/XP: 8185 including the name of the exaction, space, and parameters
    Win7/Vista: 32776 including the name of the exaction, space, and parameters

  ShellExecuteEx:
    XP/XP64/Vista/W2K: 2080 including the name of the exaction (without quotes), space, and parameters
    Win7: 32764 including the name of the exaction (without quotes), space, and parameters*/

    // WARNING: if you run a .bat file that contains the execution of .exe and passing all parameters (%*),
    //        It can happen with a longer .exe name than .bat that the "too long name" error occurs even when adhering to the limit here
    //        of the specified limit, the limit is exceeded after reaching the .exe parameter (this problem already
    //        bych neresil, musely by se prochazet .bat soubory, atd. a to je proste uchylarna)

    int cmdLineLen = commandLen + argumentsLen + 1; // +1 for the space between the command and the arguments
    if (buildBat)                                   // launching via .bat file
    {
        if (WindowsVistaAndLater)
            return cmdLineLen <= 8191; // Vista/Win7: technically it is 32776, but only 8191 works (for longer parameters, probably due to a bug in woken, characters will be deleted, tested on Vista and Win7)
        return cmdLineLen <= 8185;     // XP/XP64
    }
    else // launching via ShellExecuteEx
    {
        if (Windows7AndLater)
            return cmdLineLen <= 32764; // Win7
        return cmdLineLen <= 2080;      // W2K/XP/XP64/Vista
    }
}

//*****************************************************************************
//
// ExpandCommand2
//
// parent - parent window (for error dialogs)
// cmd - buffer for the translated command
// cmdSize - size of the 'cmd' buffer
// args - buffer for receiving arguments
// argsSize - size of the 'args' buffer
// buildBat - if TRUE, the arguments will be inserted into 'cmd'
// initDir - buffer for the path to which it should navigate for execution
// initDirSize - length of the initDir buffer
// item         - user-menu item
// path - long path to the file
// longName     - long filename
// fileNameUsed - returns TRUE if a file name or path was used during argument expansion
// userMenuAdvancedData - values of advanced parameters for User Menu: Arguments array
// ignoreEnvVarNotFoundOrTooLong - see description in ExpandVarString
//
// returns the success of the operation

BOOL ExpandCommand2(HWND parent,
                    char* cmd, int cmdSize,
                    char* args, int argsSize, BOOL buildBat,
                    char* initDir, int initDirSize,
                    CUserMenuItem* item,
                    const char* path,
                    const char* longName,
                    BOOL* fileNameUsed,
                    CUserMenuAdvancedData* userMenuAdvancedData,
                    BOOL ignoreEnvVarNotFoundOrTooLong)
{
    CALL_STACK_MESSAGE5("ExpandCommand2(, , %d, , %d, , %s, %s, )",
                        cmdSize, initDirSize, path, longName);

    *fileNameUsed = FALSE;
    char command[MAX_PATH];
    if (ExpandCommand(parent, item->UMCommand, command, MAX_PATH, ignoreEnvVarNotFoundOrTooLong))
    {
        char fileName[MAX_PATH];
        if (path[0] != 0)
        {
            int l = (int)strlen(path);
            if (path[l - 1] == '\\')
                l--;
            memcpy(fileName, path, l);

            char dosName[MAX_PATH];
            if (longName[0] != 0)
            {
                if (l + 1 + lstrlen(longName) > MAX_PATH - 1)
                {
                    SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE),
                                  MB_OK | MB_ICONEXCLAMATION);
                    goto EXIT;
                }
                fileName[l++] = '\\';
                strcpy(fileName + l, longName);
                if (GetShortPathName(fileName, dosName, MAX_PATH) == 0)
                {
                    TRACE_E("GetShortPathName() failed");
                    dosName[0] = 0;
                }
            }
            else
            {
                if (l == 2 && fileName[1] == ':') // we need to add '\\' after the normal root path
                {
                    fileName[l++] = '\\';
                }
                fileName[l] = 0;
                if (GetShortPathName(fileName, dosName, MAX_PATH) == 0)
                {
                    TRACE_E("GetShortPathName() failed");
                    dosName[0] = 0;
                }
                else
                {
                    SalPathAddBackslash(dosName, MAX_PATH);
                }
                SalPathAddBackslash(fileName, MAX_PATH);
            }

            char expArguments[USRMNUARGS_MAXLEN];
            if (ExpandUserMenuArguments(parent, fileName, dosName, item->Arguments, expArguments,
                                        USRMNUARGS_MAXLEN, fileNameUsed, userMenuAdvancedData,
                                        ignoreEnvVarNotFoundOrTooLong) &&
                ExpandInitDir(parent, fileName, dosName, item->InitDir, initDir, initDirSize,
                              ignoreEnvVarNotFoundOrTooLong))
            {
                int len = (int)strlen(command);
                int lArgs = (int)strlen(expArguments);
                if (CheckIfCanBeExecuted(buildBat, len, lArgs))
                {
                    if (!buildBat) // launching via ShellExecuteEx: we need to provide the arguments separately
                    {
                        if (len + 1 <= cmdSize && lArgs + 1 <= argsSize)
                        {
                            memcpy(cmd, command, len + 1);
                            memcpy(args, expArguments, lArgs + 1);
                            return TRUE;
                        }
                    }
                    else // Launching via .bat: arguments should be appended after the command
                    {
                        if (len + lArgs + 2 <= cmdSize)
                        {
                            memcpy(cmd, command, len);
                            cmd[len] = ' ';
                            memcpy(cmd + len + 1, expArguments, lArgs + 1);
                            args[0] = 0;
                            return TRUE;
                        }
                    }
                }
                SalMessageBox(parent, LoadStr(IDS_USRMNUTOOLONGCMDORARGS), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
            }
        }
        else
        {
            int len = (int)strlen(command);
            if (len + 1 < cmdSize)
            {
                memcpy(cmd, command, len + 1);
                args[0] = 0;
                initDir[0] = 0;
                return TRUE;
            }
            else
            {
                SalMessageBox(parent, LoadStr(IDS_USRMNUTOOLONGCMDORARGS), LoadStr(IDS_ERRORTITLE),
                              MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
EXIT:
    cmd[0] = 0;
    args[0] = 0;
    initDir[0] = 0;
    return FALSE;
}

/*  void RemoveRedundantBackslahes(char *text)
{
  if (text == NULL)
  {
    TRACE_E("Unexpected situation in RemoveRedundantBackslahes().");
    return;
  }
  if (strlen(text) < 3)
    return;
  char *s = text + 2;
  char *d = s;
  while (*s != 0) 
  {
    *d = *s;
    if (*s == '\\')
    {
      while (*s == '\\') s++;
      d++;
    }
    else
    {
      s++;
      d++;
    }
  }
  *d = 0;
}*/

void CMainWindow::UserMenu(HWND parent, int itemIndex, UM_GetNextFileName getNextFile, void* data,
                           CUserMenuAdvancedData* userMenuAdvancedData)
{
    CALL_STACK_MESSAGE2("CMainWindow::UserMenu(%d, ,)", itemIndex);
    if (itemIndex >= 0 && itemIndex < UserMenuItems->Count)
    {
        UpdateWindow(parent);

        int errorPos1, errorPos2;
        CUserMenuValidationData userMenuValidationData;
        BOOL ok = TRUE;
        if (ValidateUserMenuArguments(parent, UserMenuItems->At(itemIndex)->Arguments, errorPos1, errorPos2,
                                      &userMenuValidationData))
        {
            if (userMenuValidationData.UsesListOfSelNames && userMenuAdvancedData->ListOfSelNames[0] == 0)
            {
                SalMessageBox(parent, LoadStr(userMenuAdvancedData->ListOfSelNamesIsEmpty ? IDS_EMPTYLISTOFSELNAMES : IDS_TOOLONGLISTOFSELNAMES),
                              LoadStr(IDS_USERMENUERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
            if (ok && userMenuValidationData.UsesListOfSelFullNames && userMenuAdvancedData->ListOfSelFullNames[0] == 0)
            {
                SalMessageBox(parent, LoadStr(userMenuAdvancedData->ListOfSelFullNamesIsEmpty ? IDS_EMPTYLISTOFSELFULLNAMES : IDS_TOOLONGLISTOFSELFULLNAMES),
                              LoadStr(IDS_USERMENUERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
            if (ok && userMenuValidationData.UsesFullPathLeft && userMenuAdvancedData->FullPathLeft[0] == 0)
            {
                SalMessageBox(parent, LoadStr(IDS_NOTDEFFULLPATHLEFT),
                              LoadStr(IDS_USERMENUERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
            if (ok && userMenuValidationData.UsesFullPathRight && userMenuAdvancedData->FullPathRight[0] == 0)
            {
                SalMessageBox(parent, LoadStr(IDS_NOTDEFFULLPATHRIGHT),
                              LoadStr(IDS_USERMENUERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
            if (ok && userMenuValidationData.UsesFullPathInactive && userMenuAdvancedData->FullPathInactive[0] == 0)
            {
                SalMessageBox(parent, LoadStr(IDS_NOTDEFFULLPATHINACTIVE),
                              LoadStr(IDS_USERMENUERROR), MB_OK | MB_ICONEXCLAMATION);
                ok = FALSE;
            }
            if (ok && userMenuValidationData.UsedCompareType != 0)
            {
                if ((userMenuValidationData.UsedCompareType == 6 /* file-or-dir-left-right*/ ||
                     userMenuValidationData.UsedCompareType == 7 /* file-or-dir-active-inactive*/) &&
                    userMenuAdvancedData->CompareName1[0] == 0 &&
                    userMenuAdvancedData->CompareName2[0] == 0)
                { // we don't know if it wants to compare files or directories, we need to ask (the selection dialog for names differs for files/directories)
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = parent;
                    params.Flags = MB_YESNO | MB_ICONQUESTION | MSGBOXEX_SILENT;
                    params.Caption = LoadStr(IDS_QUESTION);
                    params.Text = LoadStr(IDS_COMPAREFILESORDIRS);
                    char aliasBtnNames[200];
                    /* is used for the export_mnu.py script, which generates the salmenu.mnu for Translator
we will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_FILES
  {MNTT_IT, IDS_MSGBOXBTN_DIRS
  {MNTT_PE, 0
};*/
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                            DIALOG_YES, LoadStr(IDS_MSGBOXBTN_FILES),
                            DIALOG_NO, LoadStr(IDS_MSGBOXBTN_DIRS));
                    params.AliasBtnNames = aliasBtnNames;
                    userMenuAdvancedData->CompareNamesAreDirs = (SalMessageBoxEx(&params) == DIALOG_NO);
                }

                BOOL swapNames = FALSE;
                BOOL clearNames = FALSE;
                BOOL comparingFiles = TRUE;
                switch (userMenuValidationData.UsedCompareType)
                {
                case 1: // file-left-right
                {
                    if (userMenuAdvancedData->CompareNamesReversed)
                        swapNames = TRUE;
                    if (userMenuAdvancedData->CompareNamesAreDirs)
                        clearNames = TRUE;
                    break;
                }

                case 2: // file-active-inactive
                {
                    if (userMenuAdvancedData->CompareNamesAreDirs)
                        clearNames = TRUE;
                    break;
                }

                case 3: // dir-left-right
                {
                    comparingFiles = FALSE;
                    if (userMenuAdvancedData->CompareNamesReversed)
                        swapNames = TRUE;
                    if (!userMenuAdvancedData->CompareNamesAreDirs)
                        clearNames = TRUE;
                    break;
                }

                case 4: // dir-active-inactive
                {
                    comparingFiles = FALSE;
                    if (!userMenuAdvancedData->CompareNamesAreDirs)
                        clearNames = TRUE;
                    break;
                }

                case 6: // file-or-dir-left-right
                {
                    comparingFiles = !userMenuAdvancedData->CompareNamesAreDirs;
                    if (userMenuAdvancedData->CompareNamesReversed)
                        swapNames = TRUE;
                    break;
                }

                case 7: // file-or-dir-active-inactive
                {
                    comparingFiles = !userMenuAdvancedData->CompareNamesAreDirs;
                    break;
                }
                }
                if (clearNames)
                {
                    userMenuAdvancedData->CompareName1[0] = 0;
                    userMenuAdvancedData->CompareName2[0] = 0;
                }
                else
                {
                    if (swapNames)
                    {
                        char swap[MAX_PATH];
                        lstrcpyn(swap, userMenuAdvancedData->CompareName1, MAX_PATH);
                        lstrcpyn(userMenuAdvancedData->CompareName1, userMenuAdvancedData->CompareName2, MAX_PATH);
                        lstrcpyn(userMenuAdvancedData->CompareName2, swap, MAX_PATH);
                    }
                }
                if (Configuration.CnfrmShowNamesToCompare ||
                    userMenuAdvancedData->CompareName1[0] == 0 ||
                    userMenuAdvancedData->CompareName2[0] == 0)
                {
                    CCompareArgsDlg dlg(parent, comparingFiles, userMenuAdvancedData->CompareName1,
                                        userMenuAdvancedData->CompareName2, &Configuration.CnfrmShowNamesToCompare);
                    if (dlg.Execute() != IDOK)
                        ok = FALSE;
                }
            }
        }
        else
            ok = FALSE;
        if (ok)
        {
            BOOL buildBat = UserMenuItems->At(itemIndex)->ThroughShell;
            BOOL batNotEmpty = FALSE;
            char* batName;
            HANDLE file;
            char batUniqueName[50]; // we need a unique name for the batak in the cache
            DWORD lastErr;

        _TRY_AGAIN:

            sprintf(batUniqueName, "Usermenu %X", GetTickCount());
            if (buildBat)
            {
                BOOL exists;
                batName = (char*)DiskCache.GetName(batUniqueName, "usermenu.bat", &exists, TRUE, NULL, FALSE, NULL, NULL);
                if (batName == NULL) // error (if 'exists' is TRUE -> fatal, otherwise "file already exists")
                {
                    if (!exists) // file exists -> almost impossible, but we will handle it anyway
                    {
                        Sleep(100);
                        goto _TRY_AGAIN;
                    }
                    return; // fatal error
                }
                file = HANDLES_Q(CreateFile(batName, GENERIC_WRITE, 0, NULL, CREATE_NEW,
                                            FILE_ATTRIBUTE_TEMPORARY, NULL));
                if (file == INVALID_HANDLE_VALUE)
                {
                    lastErr = GetLastError();
                    goto ERROR_LABEL;
                }
            }

            // build .bat
            int index;
            index = 0;
            char cmdLine[USRMNUCMDLINE_MAXLEN];
            char arguments[USRMNUARGS_MAXLEN];
            char initDir[MAX_PATH];
            char prevInitDir[MAX_PATH];
            initDir[0] = 0;
            arguments[0] = 0;
            char path[MAX_PATH], name[MAX_PATH];
            BOOL error;
            error = FALSE;
            BOOL skipErrorMessage;
            skipErrorMessage = FALSE;
            DWORD written;
            BOOL fileNameUsed;
            BOOL firstRound;
            firstRound = TRUE;
            while (getNextFile(index, path, name, data))
            {
                strcpy(prevInitDir, initDir);
                BOOL expandOK = ExpandCommand2(parent,
                                               cmdLine, USRMNUCMDLINE_MAXLEN,
                                               arguments, USRMNUARGS_MAXLEN, buildBat, // if we go through the batch, we leave
                                               initDir, MAX_PATH,                      // pour arguments into cmdLine
                                               UserMenuItems->At(itemIndex),
                                               path, name, &fileNameUsed,
                                               userMenuAdvancedData,
                                               !firstRound);
                if (!expandOK)
                {
                    error = TRUE;
                    skipErrorMessage = TRUE;
                    break;
                }
                if (expandOK && (firstRound || fileNameUsed || strcmp(initDir, prevInitDir) != 0)) // we block the execution of a constant command for all items (although it is a user error, but it happens too often)
                {
                    if (buildBat) // building a .bat file
                    {
                        char initDirOEM[MAX_PATH];
                        char cmdLineOEM[USRMNUCMDLINE_MAXLEN];
                        CharToOem(initDir, initDirOEM);
                        CharToOem(cmdLine, cmdLineOEM);
                        batNotEmpty = TRUE;
                        if ((initDirOEM[0] != 0 &&
                                 (initDirOEM[1] == ':' && // "@C:"
                                  (!WriteFile(file, "@", 1, &written, NULL) ||
                                   !WriteFile(file, initDirOEM, 2, &written, NULL) ||
                                   !WriteFile(file, "\r\n", 2, &written, NULL))) ||
                             (initDirOEM[1] == ':' && // "@cd C:\\path"
                              (!WriteFile(file, "@cd \"", 5, &written, NULL) ||
                               !WriteFile(file, initDirOEM, (DWORD)strlen(initDirOEM), &written, NULL) ||
                               !WriteFile(file, "\"\r\n", 3, &written, NULL)))) ||
                            !WriteFile(file, "call ", 5, &written, NULL) ||
                            !WriteFile(file, cmdLineOEM, (DWORD)strlen(cmdLineOEM), &written, NULL) ||
                            !WriteFile(file, "\r\n", 2, &written, NULL))
                        {
                            error = TRUE;
                            break;
                        }
                    }
                    else // Prime execution
                    {
                        // The original launching via CreateProcess did not allow running screensavers (*.SCR)
                        // and items from the control panel (*.cpl) and people constantly complained
                        //
                        // Let's try it via ShellExecuteEx -- it looks like it's working ;-)
                        // in addition, launch police will be treated

                        // we will set the correct default directories for individual disks
                        MainWindow->SetDefaultDirectories((initDir[0] != 0) ? initDir : NULL);

                        // abychom chodili se starejma konfiguracema, odebereme znak " ze zacatku a konce cmdLine
                        int cmdLen = (int)strlen(cmdLine);
                        if (cmdLen > 1 && cmdLine[0] == '\"' && cmdLine[cmdLen - 1] == '\"')
                        {
                            memmove(cmdLine, cmdLine + 1, cmdLen - 2);
                            cmdLine[cmdLen - 2] = 0;
                        }
                        // It is better not to break slashes so that we do not destroy any file paths
                        //RemoveRedundantBackslashes(cmdLine); // ShellExecuteEx doesn't like multiple backslashes, "$(SalDir)\salamand.exe"

                        CShellExecuteWnd shellExecuteWnd;
                        SHELLEXECUTEINFO sei;
                        memset(&sei, 0, sizeof(SHELLEXECUTEINFO));
                        sei.cbSize = sizeof(SHELLEXECUTEINFO);
                        sei.hwnd = shellExecuteWnd.Create(parent, "SEW: CMainWindow::UserMenu"); // handle to any message boxes that the system might produce while executing
                        sei.lpFile = cmdLine;
                        sei.lpParameters = arguments;
                        sei.lpDirectory = (initDir[0] != 0) ? initDir : NULL;
                        sei.nShow = SW_SHOWNORMAL;

                        if (!ShellExecuteEx(&sei))
                        {
                            DWORD err = GetLastError();
                            char buff[4 * MAX_PATH];
                            if (strlen(cmdLine) > 2 * MAX_PATH) // "always false" (argumenty jsou v 'arguments'): pro vypis chybove hlasky prilis dlouhou prikazovou radku zkratime
                                strcpy(cmdLine + 2 * MAX_PATH - 4, "...");
                            sprintf(buff, LoadStr(IDS_EXECERROR), cmdLine, GetErrorText(err));
                            SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            break;
                        }
                    }
                }
                index++;
                firstRound = FALSE;
                if (userMenuValidationData.MustHandleItemsAsGroup)
                    break; // In this mode, only one command is executed for all selected items
            }

            if (buildBat)
            {
                lastErr = GetLastError();
                DWORD size;
                size = GetFileSize(file, NULL);
                HANDLES(CloseHandle(file));

                DiskCache.NamePrepared(batUniqueName, CQuadWord(size, 0));

                if (!error) // run .bat
                {
                    if (batNotEmpty)
                    {
                        MainWindow->SetDefaultDirectories((initDir[0] != 0) ? initDir : NULL);

                        STARTUPINFO si;
                        memset(&si, 0, sizeof(STARTUPINFO));
                        si.cb = sizeof(STARTUPINFO);
                        si.lpTitle = LoadStr(IDS_COMMANDSHELL);
                        si.dwFlags = STARTF_USESHOWWINDOW;
                        POINT p;
                        if (UserMenuItems->At(itemIndex)->UseWindow &&
                            MultiMonGetDefaultWindowPos(MainWindow->HWindow, &p))
                        {
                            // if the main window is on a different monitor, we should also open it there
                            // window being created and preferably at the default position (same as on primary)
                            si.dwFlags |= STARTF_USEPOSITION;
                            si.dwX = p.x;
                            si.dwY = p.y;
                        }
                        si.wShowWindow = (UserMenuItems->At(itemIndex)->UseWindow ? SW_SHOWNORMAL : SW_HIDE);

                        PROCESS_INFORMATION pi;

                        GetEnvironmentVariable("COMSPEC", cmdLine, USRMNUCMDLINE_MAXLEN - 20);
                        AddDoubleQuotesIfNeeded(cmdLine, USRMNUCMDLINE_MAXLEN - 10); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)
                        if (!UserMenuItems->At(itemIndex)->UseWindow || UserMenuItems->At(itemIndex)->CloseShell)
                            strcat(cmdLine, " /C "); // Run the command + close immediately after it finishes
                        else
                            strcat(cmdLine, " /K "); // Run command + do not close after finishing

                        char* s = cmdLine + strlen(cmdLine);
                        if ((s - cmdLine) + strlen(batName) < USRMNUCMDLINE_MAXLEN - 2)
                            sprintf(s, "\"%s\"", batName);
                        else
                            strcpy(cmdLine, batName);

                        if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                                                   CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                                                   NULL, NULL, &si, &pi)))
                        {
                            DWORD err = GetLastError();
                            char buff[4 * MAX_PATH];
                            if (strlen(cmdLine) > 2 * MAX_PATH)
                                strcpy(cmdLine + 2 * MAX_PATH, "..."); // shorten for the sake of it (probably will never be needed)
                            sprintf(buff, LoadStr(IDS_EXECERROR), cmdLine, GetErrorText(err));
                            DiskCache.ReleaseName(batUniqueName, FALSE);
                            SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        else
                        {
                            DiskCache.AssignName(batUniqueName, pi.hProcess, TRUE, crtDirect);
                            //            HANDLES(CloseHandle(pi.hProcess));   // handles DiskCache
                            HANDLES(CloseHandle(pi.hThread));
                        }
                    }
                    else // It is not worth running an empty .BAT file (due to lack of memory and other crazy errors)
                    {
                        DiskCache.ReleaseName(batUniqueName, FALSE);
                    }
                }
                else
                {
                ERROR_LABEL:

                    DiskCache.ReleaseName(batUniqueName, FALSE);
                    if (!skipErrorMessage)
                    {
                        SalMessageBox(parent, GetErrorText(lastErr), LoadStr(IDS_ERRORTITLE),
                                      MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
        }
    }
    UpdateWindow(parent);
}

void CMainWindow::SetDefaultDirectories(const char* curPath)
{
    CALL_STACK_MESSAGE2("CMainWindow::SetDefaultDirectories(%s)", curPath);
    //--- Restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);
    //--- setting up the environment
    char name[4] = "= :";
    const char* dir;
    char d;
    for (d = 'a'; d <= 'z'; d++)
    {
        name[1] = d;
        if (curPath != NULL && d == LowerCase[curPath[0]]) // UNC paths are not counted
            dir = curPath;
        else
            dir = DefaultDir[d - 'a'];

        if (dir[1] == ':' && dir[2] == '\\' && dir[3] == 0)
            SetEnvironmentVariable(name, NULL);
        else
            SetEnvironmentVariable(name, dir);
    }
}

BOOL CMainWindow::HandleCtrlLetter(char c)
{
    CALL_STACK_MESSAGE2("CMainWindow::HandleCtrlLetter(%u)", c);
    if ((GetKeyState(VK_SHIFT) & 0x8000) != 0)
    { // change disk via Shift+letter
        GetActivePanel()->ChangeDrive(c);
    }
    else // NC + Windows Ctrl+? hotkeys
    {
        WPARAM cmd;
        switch (c) // converts everything to upper-case here
        {
        case 'A':
            cmd = CM_ACTIVESELECTALL;
            break;

        case 'C': // copy
        case 'X': // cut
        {
            BOOL files = FALSE;
            if (GetActivePanel() != NULL)
            {
                if (GetActivePanel()->GetCaretIndex() == 0)
                {
                    if (0 == GetActivePanel()->Dirs->Count ||
                        strcmp(GetActivePanel()->Dirs->At(0).Name, "..") != 0)
                    {
                        files = GetActivePanel()->Dirs->Count + GetActivePanel()->Files->Count > 0;
                    }
                    else
                    {
                        int count = GetActivePanel()->GetSelCount();
                        if (count == 1)
                        {
                            int index;
                            GetActivePanel()->GetSelItems(1, &index);
                            files = index != 0;
                        }
                        else
                            files = count > 0;
                    }
                }
                else
                    files = GetActivePanel()->GetCaretIndex() > 0;
            }
            if (!files)
                return FALSE; // cut a copy cannot be performed
            cmd = (c == 'C') ? CM_CLIPCOPY : CM_CLIPCUT;
            break;
        }

        case 'D':
            cmd = CM_ACTIVEUNSELECTALL;
            break;
        case 'E':
            cmd = CM_EMAILFILES;
            break;
        case 'F':
            cmd = CM_FINDFILE;
            break;
        case 'G':
            cmd = CM_ACTIVE_CHANGEDIR;
            break;
        case 'H':
            cmd = CM_TOGGLEHIDDENFILES;
            break;
        case 'I':
            cmd = CM_LAST_PLUGIN_CMD;
            break;
        case 'K':
            cmd = CM_CONVERTFILES;
            break;
        case 'L':
            cmd = CM_DRIVEINFO;
            break;
        case 'M':
            cmd = CM_FILELIST;
            break;
        case 'N':
            cmd = CM_TOGGLEELASTICSMART;
            break;
        case 'P':
            cmd = CM_SEC_PERMISSIONS;
            break;
        case 'Q':
            cmd = CM_OCCUPIEDSPACE;
            break;
        case 'R':
            cmd = CM_ACTIVEREFRESH;
            break;
        case 'S':
            cmd = CM_CLIPPASTELINKS;
            break;
        case 'T':
            cmd = CM_AFOCUSSHORTCUT;
            break;
        case 'U':
            cmd = CM_SWAPPANELS;
            break;
        case 'V':
            cmd = CM_CLIPPASTE;
            break;
        case 'W':
            cmd = CM_RESELECT;
            break;

        default:
            return FALSE;
        }
        SendMessage(HWindow, WM_COMMAND, cmd, 0);
    }
    return TRUE;
}

void CMainWindow::ChangePanel(BOOL force)
{
    CALL_STACK_MESSAGE1("CMainWindow::ChangePanel()");

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (IsIconic(HWindow))
        return;

    CFilesWindow* p1 = GetActivePanel();
    CFilesWindow* p2 = GetNonActivePanel();

    BOOL change = FALSE;
    if (force || p2->CanBeFocused())
        change = TRUE;
    else
    {
        // if the panel is ZOOMed, we minimize it and ZOOM the other one
        if (IsPanelZoomed(TRUE) || IsPanelZoomed(FALSE))
        {
            if (IsPanelZoomed(TRUE))
                SplitPosition = 0.0;
            else
                SplitPosition = 1.0;
            LayoutWindows();
            change = TRUE;
        }
    }

    if (change)
    {
        SetActivePanel(p2);

        // Ensure redrawing of the active header of the panel
        if (p1->DirectoryLine != NULL)
            p1->DirectoryLine->InvalidateAndUpdate(FALSE);
        if (p2->DirectoryLine != NULL)
            p2->DirectoryLine->InvalidateAndUpdate(FALSE);

        UpdateDriveBars(); // Press the correct disk in the drive bar

        //    ReleaseMenuNew();
        if (EditMode)
        {
            p1->RedrawIndex(p1->FocusedIndex);
            int i = p2->GetCaretIndex();
            i = max(i, 0);
            p2->SetCaretIndex(i, TRUE);
            p2->RedrawIndex(i);
        }
        else
        {
            if (GetFocus() != p2->GetListBoxHWND())
                SetFocus(p2->GetListBoxHWND());
            int i = p2->GetCaretIndex();
            i = max(i, 0);
            p2->SetCaretIndex(i, FALSE);
        }
        EditWindowSetDirectory();
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
        MainWindow->UpdateDefaultDir(TRUE);

        // we will send this news to all loaded plugins
        Plugins.Event(PLUGINEVENT_PANELACTIVATED, p2 == LeftPanel ? PANEL_LEFT : PANEL_RIGHT);
    }
}

void CMainWindow::FocusPanel(CFilesWindow* focus, BOOL testIfMainWndActive)
{
    CALL_STACK_MESSAGE2("CMainWindow::FocusPanel(, %d)", testIfMainWndActive);
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    if (!IsIconic(HWindow) && !focus->CanBeFocused())
        focus = ((focus == LeftPanel) ? RightPanel : LeftPanel);

    if (GetFocus() != focus->GetListBoxHWND())
    {
        if (!testIfMainWndActive || GetForegroundWindow() == HWindow) // Focus only if the main window is active (FTP plugin with a non-modal Welcome Message window focused the panel when the command line was turned off -> deactivation of Welcome Message)
            SetFocus(focus->GetListBoxHWND());
        else
            focus->OnSetFocus(FALSE); // simulation of focus in the panel
    }

    CFilesWindow* old = GetActivePanel();
    SetActivePanel(focus);

    UpdateDriveBars(); // Press the correct disk in the drive bar

    // Ensure redrawing of the active header of the panel
    if (old != focus)
    {
        // We have activated a different panel, we will let it set the enablers for it
        RefreshCommandStates();
        // Fixes a bug (which was still present in 2.5b10) where users had one panel active, with focus on UpDir
        // and then right-clicked on the file in the passive panel and selected DELETE from the context menu
        // nothing happened because the EnablerFilesDelete enabler was FALSE (not updated for the new panel)
        // more at https://forum.altap.cz/viewtopic.php?t=181

        // let's repaint the directory line of both windows
        if (old->DirectoryLine != NULL)
            old->DirectoryLine->InvalidateAndUpdate(FALSE);
        if (focus->DirectoryLine != NULL)
            focus->DirectoryLine->InvalidateAndUpdate(FALSE);
        //    ReleaseMenuNew();
        EditWindowSetDirectory();
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
        // We will send this news to the loaded plugins
        Plugins.Event(PLUGINEVENT_PANELACTIVATED, focus == LeftPanel ? PANEL_LEFT : PANEL_RIGHT);
    }
    //--- Restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);
}

void CMainWindow::ShowCommandLine()
{
    CALL_STACK_MESSAGE1("CMainWindow::ShowCommandLine()");
    if (EditWindow == NULL || EditWindow->HWindow != NULL)
        return;

    if (!EditWindow->Create(HWindow, IDC_EDITWINDOW))
        TRACE_E("Unable to create EditWindow.");
    else
    {
        LayoutWindows();
        EditWindow->RestoreContent();
        ShowWindow(EditWindow->HWindow, SW_SHOW);
        if (EditWindow->IsEnabled())
            SetFocus(EditWindow->HWindow);
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
    }
}

void CMainWindow::HideCommandLine(BOOL storeContent, BOOL focusPanel)
{
    if (EditWindow == NULL || EditWindow->HWindow == NULL)
        return;

    if (storeContent)
        EditWindow->StoreContent();

    DestroyWindow(EditWindow->HWindow);
    if (focusPanel)
        FocusPanel(GetActivePanel());
    LayoutWindows();
    IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
}

//****************************************************************************
//
// Image Drag functions
//

void ImageDragBegin(int width, int height, int dxHotspot, int dyHotspot)
{
    if (ImageDragging)
        TRACE_E("ImageDragging == TRUE - this should never happen");
    ImageDragW = width;
    ImageDragH = height;
    ImageDragDxHotspot = dxHotspot;
    ImageDragDyHotspot = dyHotspot;
    ImageDragging = TRUE;
}

void ImageDragEnd()
{
    if (!ImageDragging)
        TRACE_E("ImageDragging == FALSE - this should never happen");
    ImageDragX = INT_MAX;
    ImageDragY = INT_MAX;
    ImageDragW = INT_MAX;
    ImageDragH = INT_MAX;
    ImageDragging = FALSE;
}

BOOL ImageDragInterfereRect(const RECT* rect)
{
    if (!ImageDraggingVisible)
        return FALSE;
    if (ImageDragX == INT_MAX || ImageDragY == INT_MAX)
    {
        TRACE_E("ImageDragX == INT_MAX || ImageDragY == INT_MAX");
        return TRUE; // for safety
    }
    if (ImageDragW == INT_MAX || ImageDragH == INT_MAX)
    {
        TRACE_E("ImageDragW == INT_MAX || ImageDragH == INT_MAX");
        return TRUE; // for safety
    }
    RECT r;
    r.left = ImageDragX - ImageDragDxHotspot;
    r.top = ImageDragY - ImageDragDyHotspot;
    r.right = r.left + ImageDragW;
    r.bottom = r.top + ImageDragH;
    RECT dstR;
    IntersectRect(&dstR, rect, &r);
    return !IsRectEmpty(&dstR);
}

void ImageDragEnter(int x, int y)
{
    CALL_STACK_MESSAGE3("ImageDragEnter(%d, %d)", x, y);
    if (ImageDraggingVisible)
        TRACE_E("ImageDraggingVisible == TRUE - this should never happen");
    if (!ImageDragging)
        TRACE_E("ImageDragging == FALSE - this should never happen");
    ImageDragX = x;
    ImageDragY = y;
    ShowCaretAfterDrop = MainWindow->EditWindow->HideCaret();
    ImageList_DragEnter(MainWindow->HWindow, x - MainWindow->WindowRect.left, y - MainWindow->WindowRect.top);
    ImageDraggingVisible = TRUE;
    ImageDraggingVisibleLevel = 1;
}

void ImageDragMove(int x, int y)
{
    CALL_STACK_MESSAGE3("ImageDragMove(%d, %d)", x, y);
    if (!ImageDragging)
        TRACE_E("ImageDragging == FALSE - this should never happen");
    ImageDragX = x;
    ImageDragY = y;
    ImageList_DragMove(x - MainWindow->WindowRect.left, y - MainWindow->WindowRect.top);
}

void ImageDragLeave()
{
    CALL_STACK_MESSAGE1("ImageDragLeave()");
    if (!ImageDragging)
        TRACE_E("ImageDragging == FALSE - this should never happen");
    ImageList_DragLeave(MainWindow->HWindow);
    ImageDraggingVisible = FALSE;
    ImageDraggingVisibleLevel = 0;
    ImageDragX = INT_MAX;
    ImageDragY = INT_MAX;
    if (ShowCaretAfterDrop)
    {
        MainWindow->EditWindow->ShowCaret();
        ShowCaretAfterDrop = FALSE;
    }
}

void ImageDragShow(BOOL show)
{
    CALL_STACK_MESSAGE2("ImageDragShow(%d)", show);
    if (!ImageDragging)
        TRACE_E("ImageDragging == FALSE - this should never happen");
    ImageDraggingVisibleLevel += show ? 1 : -1;

    if (show && ImageDraggingVisibleLevel == 1)
    {
        ImageList_DragShowNolock(TRUE);
        ImageDraggingVisible = TRUE;
    }
    if (!show && ImageDraggingVisibleLevel == 0)
    {
        ImageList_DragShowNolock(FALSE);
        ImageDraggingVisible = FALSE;
    }
}

//****************************************************************************
//
// Context Help (Shift+F1) support
//

/////////////////////////////////////////////////////////////////////////////
// useful message ranges

#define WM_SYSKEYFIRST WM_SYSKEYDOWN
#define WM_SYSKEYLAST WM_SYSDEADCHAR

#define WM_NCMOUSEFIRST WM_NCMOUSEMOVE
#define WM_NCMOUSELAST WM_NCMBUTTONDBLCLK

HWND GetParentOwner(HWND hWnd)
{
    CALL_STACK_MESSAGE_NONE
    // return parent in the Windows sense
    return (GetWindowLongPtr(hWnd, GWL_STYLE) & WS_CHILD) ? GetParent(hWnd) : GetWindow(hWnd, GW_OWNER);
}

HWND GetTopLevelParent(HWND hWindow)
{
    CALL_STACK_MESSAGE_NONE
    HWND hWndParent = hWindow;
    HWND hWndT;
    while ((hWndT = GetParentOwner(hWndParent)) != NULL)
        hWndParent = hWndT;

    return hWndParent;
}

BOOL IsDescendant(HWND hWndParent, HWND hWndChild)
{
    CALL_STACK_MESSAGE_NONE
    // helper for detecting whether child descendent of parent
    //  (works with owned popups as well)
    if (!IsWindow(hWndParent))
    {
        TRACE_E("hWndParent is not window");
        return FALSE;
    }
    if (!IsWindow(hWndChild))
    {
        TRACE_E("hWndChild is not window");
        return FALSE;
    }

    do
    {
        if (hWndParent == hWndChild)
            return TRUE;

        hWndChild = GetParentOwner(hWndChild);
    } while (hWndChild != NULL);

    return FALSE;
}

DWORD
CMainWindow::MapClientArea(POINT point)
{
    DWORD dwContext = 0;

    CMainWindowsHitTestEnum hit = HitTest(point.x, point.y);
    CToolBar* toolbar = NULL;

    switch (hit)
    {
    case mwhteMenu:
        dwContext = IDH_MENUBAR;
        break;

    case mwhteTopToolbar:
    {
        dwContext = IDH_TOPTOOLBAR;
        toolbar = TopToolBar;
        break;
    }

    case mwhtePluginsBar:
        dwContext = IDH_PLUGINSBAR;
        break;

    case mwhteMiddleToolbar:
    {
        dwContext = IDH_MIDDLETOOLBAR;
        toolbar = MiddleToolBar;
        break;
    }

    case mwhteUMToolbar:
        dwContext = IDH_UMTOOLBAR;
        break;

    case mwhteHPToolbar:
        dwContext = IDH_HPTOOLBAR;
        break;

    case mwhteDriveBar:
        dwContext = IDH_DRIVEBAR;
        break;

    case mwhteCmdLine:
        dwContext = IDH_COMMANDLINE;
        break;

    case mwhteBottomToolbar:
    {
        dwContext = IDH_BOTTOMTOOLBAR;
        toolbar = BottomToolBar;
        break;
    }

    case mwhteSplitLine:
        dwContext = IDH_SPLITBAR;
        break;

    case mwhteLeftDirLine:
    {
        dwContext = IDH_DIRECTORYLINE;

        if (LeftPanel->DirectoryLine->ToolBar != NULL &&
            LeftPanel->DirectoryLine->ToolBar->HWindow != NULL)
            toolbar = LeftPanel->DirectoryLine->ToolBar;
        break;
    }

    case mwhteRightDirLine:
    {
        dwContext = IDH_DIRECTORYLINE;

        if (RightPanel->DirectoryLine->ToolBar != NULL &&
            RightPanel->DirectoryLine->ToolBar->HWindow != NULL)
            toolbar = RightPanel->DirectoryLine->ToolBar;
        break;
    }

    case mwhteLeftHeaderLine:
    case mwhteRightHeaderLine:
        dwContext = IDH_HEADERLINE;
        break;

    case mwhteLeftStatusLine:
    case mwhteRightStatusLine:
        dwContext = IDH_INFOLINE;
        break;

    case mwhteLeftWorkingArea:
    case mwhteRightWorkingArea:
        dwContext = IDH_WORKINGAREA;
        break;
    }

    // we extract the ID of the button that the user clicked on
    if (toolbar != NULL)
    {
        POINT p;
        p = point;
        ScreenToClient(toolbar->HWindow, &p);
        int index = toolbar->HitTest(p.x, p.y);
        if (index != -1)
        {
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_ID;
            if (toolbar->GetItemInfo2(index, TRUE, &tii))
                dwContext = tii.ID;
        }
    }
    return dwContext;
}

DWORD
CMainWindow::MapNonClientArea(int iHit)
{
    DWORD dwContext = 0;
    switch (iHit)
    {
        /*      case HTBORDER:
    case HTBOTTOM:
    case HTBOTTOMLEFT:
    case HTBOTTOMRIGHT:
    case HTLEFT:
    case HTRIGHT:
    case HTTOP:
    case HTTOPLEFT:
    case HTTOPRIGHT:
    case HTCAPTION:
    case HTREDUCE:
    case HTZOOM:*/

    case HTMINBUTTON:
    case HTMAXBUTTON:
    case HTCLOSE:
        dwContext = IDH_MINMAXCLOSEBTNS;
        break;
    }
    return dwContext;
}

BOOL CMainWindow::CanEnterHelpMode()
{
    CALL_STACK_MESSAGE1("CMainWindow::CanEnterHelpMode()");
    if (HelpMode == HELP_ACTIVE) // already in help mode?
        return FALSE;

    if (HHelpCursor == NULL)
    {
        HHelpCursor = LoadCursor(NULL, IDC_HELP);
        if (HHelpCursor == NULL)
            return FALSE;
    }

    return TRUE;
}

void CMainWindow::OnContextHelp()
{
    CALL_STACK_MESSAGE1("CMainWindow::OnContextHelp()");
    // don't enter twice, and don't enter if initialization fails
    if (HelpMode == HELP_ACTIVE || !CanEnterHelpMode())
        return;

    // don't enter help mode with pending WM_USER_EXITHELPMODE message
    MSG msg;
    if (PeekMessage(&msg, HWindow, WM_USER_EXITHELPMODE, WM_USER_EXITHELPMODE, PM_REMOVE | PM_NOYIELD))
        return;

    BOOL bHelpMode = HelpMode;
    if (HelpMode != HELP_INACTIVE && HelpMode != HELP_ENTERING)
        return;
    HelpMode = HELP_ACTIVE;

    if (bHelpMode == HELP_INACTIVE)
    {
        // need to delay help startup until later
        PostMessage(HWindow, WM_COMMAND, CM_HELP_CONTEXT, 0);
        HelpMode = HELP_ENTERING;
        return;
    }

    IdleRefreshStates = TRUE; // trigger idle update
    OnEnterIdle();            // let's redraw the toolbar

    if (HelpMode != HELP_ACTIVE)
        return;

    MenuBar->SetHelpMode(TRUE);

    // bottom toolbar to standard state
    BottomToolBar->SetState(btbsNormal);
    BottomToolBar->UpdateItemsState();

    // if someone is monitoring the mouse, I will stop monitoring
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_QUERY;
    if (TrackMouseEvent(&tme) && tme.hwndTrack != NULL)
        SendMessage(tme.hwndTrack, WM_MOUSELEAVE, 0, 0);

    DWORD dwContext = 0;
    POINT point;

    GetCursorPos(&point);
    SetHelpCapture(point, NULL);
    LONG lIdleCount = 0;

    BOOL first = TRUE;

    HWND hDirtyWindow = NULL;
    while (HelpMode)
    {
        if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
        {
            if (!ProcessHelpMsg(msg, &dwContext, &hDirtyWindow))
                break;
            if (dwContext != 0)
                return;
        }
        else
        {
            if (first)
            {
                // we buffer the mouse movement to fall into the toolbar if the cursor is above disabled buttons
                POINT p;
                GetCursorPos(&p);
                ScreenToClient(HWindow, &p);
                PostMessage(HWindow, WM_MOUSEMOVE, 0, MAKELPARAM(point.x, point.y));
                // I originally had this buffering before the loop, but it was causing issues
                // if a tooltip was displayed for a disabled button and I pressed Shift+F1, when turning off
                // tooltip was delivered WM_MOUSELEAVE (PeekMessage sends messages, see MSDN)
                // so the button under the cursor was rendered as enabled, but right after that
                // again as disabled -- I'll wait for this nonsense until all messages are ready
                // delivered and MOUSEMOVE will be captured 100%

                first = FALSE;
            }
            else
            {
                WaitMessage();
            }
        }
    }
    if (hDirtyWindow != NULL)
        SendMessage(hDirtyWindow, WM_USER_HELP_MOUSELEAVE, 0, 0);

    MenuBar->SetHelpMode(FALSE);
    HelpMode = HELP_INACTIVE;
    ReleaseCapture();

    // make sure the cursor is set appropriately
    SetCapture(HWindow);
    ReleaseCapture();

    if (dwContext != 0)
        OpenHtmlHelp(NULL, HWindow, HHCDisplayContext, dwContext, FALSE);

    IdleRefreshStates = TRUE; // trigger idle update
}

HWND CMainWindow::SetHelpCapture(POINT point, BOOL* pbDescendant)
// set or release capture, depending on where the mouse is
// also assign the proper cursor to be displayed.
{
    CALL_STACK_MESSAGE1("CMainWindow::SetHelpCapture(,)");
    if (!HelpMode)
        return NULL;

    HWND hWndCapture = GetCapture();
    HWND hWndHit = WindowFromPoint(point);
    HWND hTopHit = GetTopLevelParent(hWndHit);
    HWND hTopActive = GetTopLevelParent(GetActiveWindow());
    BOOL bDescendant = FALSE;
    DWORD hCurTask = GetCurrentThreadId();
    DWORD hTaskHit = hWndHit != NULL ? GetWindowThreadProcessId(hWndHit, NULL) : NULL;

    if (hTopActive == NULL || hWndHit == GetDesktopWindow())
    {
        if (hWndCapture == HWindow)
            ReleaseCapture();
        SetCursor(HHelpCursor);
    }
    else if (hTopActive == NULL ||
             hWndHit == NULL || hCurTask != hTaskHit ||
             !IsDescendant(HWindow, hWndHit))
    {
        if (hCurTask != hTaskHit)
            hWndHit = NULL;
        if (hWndCapture == HWindow)
            ReleaseCapture();
    }
    else
    {
        bDescendant = TRUE;
        if (hTopActive != hTopHit)
            hWndHit = NULL;
        else
        {
            if (hWndCapture != HWindow)
                SetCapture(HWindow);
            SetCursor(HHelpCursor);
        }
    }
    if (pbDescendant != NULL)
        *pbDescendant = bDescendant;
    return hWndHit;
}

BOOL CMainWindow::ProcessHelpMsg(MSG& msg, DWORD* pContext, HWND* hDirtyWindow)
{
    CALL_STACK_MESSAGE4("CMainWindow::ProcessHelpMsg(0x%X, 0x%IX, 0x%IX,)", msg.message, msg.wParam, msg.lParam);

    if (pContext == NULL)
    {
        TRACE_E("pContext == NULL");
        return FALSE;
    }
    if (msg.message == WM_USER_EXITHELPMODE ||
        (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE))
    {
        PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
        return FALSE;
    }

    POINT point;
    if ((msg.message >= WM_MOUSEFIRST && msg.message <= WM_MOUSELAST) ||
        (msg.message >= WM_NCMOUSEFIRST && msg.message <= WM_NCMOUSELAST))
    {
        BOOL bDescendant;
        HWND hWndHit = SetHelpCapture(msg.pt, &bDescendant);
        if (hWndHit == NULL)
        {
            PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE); // eat the message
            return TRUE;
        }

        if (bDescendant)
        {
            if (msg.message != WM_LBUTTONDOWN)
            {
                // Hit one of our owned windows -- eat the message.
                PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);

                // let us know the numbers of those interested in highlighting items during Shift+F1 mode
                if (msg.message == WM_MOUSEMOVE)
                {
                    if (*hDirtyWindow != NULL && *hDirtyWindow != hWndHit)
                        SendMessage(*hDirtyWindow, WM_USER_HELP_MOUSELEAVE, 0, 0);
                    *hDirtyWindow = hWndHit; // LEAVE needs to be sent to this window

                    POINT p = msg.pt;
                    ScreenToClient(hWndHit, &p);
                    SendMessage(hWndHit, WM_USER_HELP_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
                }
                return TRUE;
            }
            int iHit = (int)SendMessage(hWndHit, WM_NCHITTEST, 0,
                                        MAKELONG(msg.pt.x, msg.pt.y));
            if (iHit == HTSYSMENU)
            {
                if (GetCapture() != HWindow)
                {
                    TRACE_E("GetCapture() != HWindow");
                    return FALSE;
                }
                ReleaseCapture();
                // the message we peeked changes into a non-client because
                // of the release capture.
                GetMessage(&msg, NULL, WM_NCLBUTTONDOWN, WM_NCLBUTTONDOWN);
                DispatchMessage(&msg);
                GetCursorPos(&point);
                SetHelpCapture(point, NULL);
            }
            else if (iHit == HTCLIENT)
            {
                if (hWndHit == MenuBar->HWindow)
                {
                    PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
                    ReleaseCapture();
                    POINT p = msg.pt;
                    ScreenToClient(hWndHit, &p);
                    msg.lParam = MAKELPARAM(p.x, p.y);
                    msg.hwnd = hWndHit;
                    msg.message = WM_MOUSEMOVE;
                    DispatchMessage(&msg);
                    msg.message = WM_LBUTTONDOWN;
                    DispatchMessage(&msg);
                    GetCursorPos(&point);
                    SetHelpCapture(point, NULL);
                }
                else
                {
                    *pContext = MapClientArea(msg.pt);
                    PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
                    return FALSE;
                }
            }
            else
            {
                *pContext = MapNonClientArea(iHit);
                PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);
                return FALSE;
            }
        }
        else
        {
            // Hit one of our app's windows (or desktop) -- dispatch the message.
            PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE);

            // Dispatch mouse messages that hit the desktop!
            DispatchMessage(&msg);
        }
    }
    else if (msg.message == WM_SYSCOMMAND ||
             (msg.message >= WM_KEYFIRST && msg.message <= WM_KEYLAST))
    {
        if (GetCapture() != NULL)
        {
            ReleaseCapture();
            MSG msg2;
            while (PeekMessage(&msg2, NULL, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE | PM_NOYIELD))
                ;
        }
        if (PeekMessage(&msg, NULL, msg.message, msg.message, PM_NOREMOVE))
        {
            GetMessage(&msg, NULL, msg.message, msg.message);

            // we will ensure sending messages to our menu (bypassing the need for keyboard hooks)
            // Supporting entering the menu via Alt/F10/Alt+letter during help mode
            if (MenuBar == NULL || !MenuBar->IsMenuBarMessage(&msg))
            {
                TranslateMessage(&msg);
                if (msg.message == WM_SYSCOMMAND ||
                    (msg.message >= WM_SYSKEYFIRST &&
                     msg.message <= WM_SYSKEYLAST))
                {
                    // only dispatch system keys and system commands
                    DispatchMessage(&msg);
                }
            }
        }
        GetCursorPos(&point);
        SetHelpCapture(point, NULL);
    }
    else
    {
        // allow all other messages to go through (capture still set)
        if (PeekMessage(&msg, NULL, msg.message, msg.message, PM_REMOVE))
            DispatchMessage(&msg);
    }

    return TRUE;
}

void CMainWindow::ExitHelpMode()
{
    CALL_STACK_MESSAGE1("CMainWindow::ExitHelpMode()");
    // if not in help mode currently, this is a no-op
    if (!HelpMode)
        return;

    // only post new WM_EXITHELPMODE message if one doesn't already exist
    //  in the queue.
    MSG msg;
    if (!PeekMessage(&msg, HWindow, WM_USER_EXITHELPMODE, WM_USER_EXITHELPMODE, PM_REMOVE | PM_NOYIELD))
        PostMessage(HWindow, WM_USER_EXITHELPMODE, 0, 0);

    // release capture if this window has it
    if (GetCapture() == HWindow)
        ReleaseCapture();

    HelpMode = HELP_INACTIVE;
    IdleRefreshStates = TRUE; // trigger idle update
}

void CMainWindow::UpdateDriveBars()
{
    if (DriveBar == NULL || DriveBar2 == NULL)
        return;

    if (DriveBar->HWindow == NULL)
        return;

    if (DriveBar2->HWindow == NULL)
    {
        // if we have only one drive bar, it applies to the active panel
        DriveBar->SetCheckedDrive(GetActivePanel());
    }
    else
    {
        DriveBar->SetCheckedDrive(LeftPanel);
        DriveBar2->SetCheckedDrive(RightPanel);
    }
}

void CMainWindow::CancelPanelsUI()
{
    LeftPanel->CancelUI();
    RightPanel->CancelUI();
}

BOOL CMainWindow::QuickRenameWindowActive()
{
    return (LeftPanel->IsQuickRenameActive() || RightPanel->IsQuickRenameActive());
}

BOOL CMainWindow::DoQuickRename()
{
    if (LeftPanel->IsQuickRenameActive())
        return LeftPanel->HandeQuickRenameWindowKey(VK_RETURN);
    if (RightPanel->IsQuickRenameActive())
        return RightPanel->HandeQuickRenameWindowKey(VK_RETURN);
    return TRUE; // OK
}

//
// ****************************************************************************
// LockUI
//

void CMainWindow::LockUI(BOOL lock, HWND hToolWnd, const char* lockReason)
{
    if (LockedUI && lock)
    {
        TRACE_E("CMainWindow::LockUI(): main window is already locked! Ignoring this request...");
        return;
    }
    if (!LockedUI && !lock)
    {
        TRACE_E("CMainWindow::LockUI(): main window is not locked! Ignoring this request...");
        return;
    }

    LockedUI = lock;
    if (lock)
    {
        LockedUIToolWnd = hToolWnd;
        if (lockReason != NULL)
            LockedUIReason = DupStr(lockReason);
    }
    else
    {
        LockedUIToolWnd = NULL;
        if (LockedUIReason != NULL)
            free(LockedUIReason);
    }

    if (HTopRebar != NULL)
        EnableWindow(HTopRebar, !lock);
    if (MiddleToolBar != NULL && MiddleToolBar->HWindow != NULL)
        EnableWindow(MiddleToolBar->HWindow, !lock);
    if (BottomToolBar != NULL && BottomToolBar->HWindow != NULL)
        EnableWindow(BottomToolBar->HWindow, !lock);
    LeftPanel->LockUI(lock);
    RightPanel->LockUI(lock);
}

void CMainWindow::BringLockedUIToolWnd()
{
    if (LockedUIToolWnd != NULL)
        SetWindowPos(LockedUIToolWnd, HWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOSENDCHANGING | SWP_NOREDRAW);
}

// ****************************************************************************

CFilesWindow*
CMainWindow::GetPanel(int panel)
{
    switch (panel)
    {
    case PANEL_SOURCE:
        return GetActivePanel();
    case PANEL_TARGET:
        return GetNonActivePanel();
    case PANEL_LEFT:
        return LeftPanel;
    case PANEL_RIGHT:
        return RightPanel;
    default:
        TRACE_E("Invalid panel (PANEL_XXX) constant: " << panel);
        return NULL;
    }
}

void CMainWindow::PostFocusNameInPanel(int panel, const char* path, const char* name)
{
    CALL_STACK_MESSAGE4("CMainWindow::FocusNameInPanel(%d, %s, %s)", panel, path, name);
    CFilesWindow* p = GetPanel(panel);
    if (p != NULL)
    {
        static char pathBackup[MAX_PATH + 200];
        static char nameBackup[MAX_PATH + 200];
        lstrcpyn(pathBackup, path, MAX_PATH + 200);
        lstrcpyn(nameBackup, name, MAX_PATH + 200);
        PostMessage(p->HWindow, WM_USER_FOCUSFILE, (WPARAM)nameBackup, (LPARAM)pathBackup);
    }
}
