// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "snooper.h"
#include "zip.h"
#include "shellib.h"
#include "pack.h"

void CFilesWindow::PluginFSFilesAction(CPluginFSActionType type)
{
    CALL_STACK_MESSAGE2("CFilesWindow::PluginFSFilesAction(%d)", type);
    if (Dirs->Count + Files->Count == 0)
        return;
    if (!Is(ptPluginFS) || !GetPluginFS()->NotEmpty())
        return;
    int panel = MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT;
    CFilesWindow* target = (MainWindow->LeftPanel == this ? MainWindow->RightPanel : MainWindow->LeftPanel);
    BOOL unselect = FALSE;

    BeginSuspendMode(); // the snooper takes a break
    BeginStopRefresh(); // to suppress path change messages

    int count = GetSelCount();
    int selectedDirs = 0;
    if (count > 0)
    {
        // count how many directories are selected (the remaining selected items are files)
        int i;
        for (i = 0; i < Dirs->Count; i++) // ".." cannot be selected, the check would be useless
        {
            if (Dirs->At(i).Selected)
                selectedDirs++;
        }
    }
    else
        count = 0;

    char subject[MAX_PATH + 100 + 200];    // +200 is a reserve (Windows creates paths longer than MAX_PATH)
    char formatedFileName[MAX_PATH + 200]; // +200 is a reserve (Windows creates paths longer than MAX_PATH)
    char expanded[200];
    if (count <= 1) // one selected item or none
    {
        int i;
        if (count == 0)
            i = GetCaretIndex();
        else
            GetSelItems(1, &i);

        if (i < 0 || i >= Dirs->Count + Files->Count)
        {
            EndStopRefresh();
            EndSuspendMode(); // the snooper resumes now
            return;           // invalid index (no files)
        }
        BOOL isDir = i < Dirs->Count;
        CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
        lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
    }
    else
    {
        ExpandPluralFilesDirs(expanded, 200, count - selectedDirs, selectedDirs, epfdmNormal, FALSE);
    }

    int resID = 0;
    switch (type)
    {
    case atCopy:
        resID = IDS_COPYTO;
        break;
    case atMove:
        resID = IDS_MOVETO;
        break;
    case atDelete:
        resID = IDS_CONFIRM_DELETE;
        break;
    }
    CTruncatedString str;
    if (resID != 0)
    {
        // IDS_COPY/IDS_MOVE contain ampersands, cancel it
        char templ[200];
        lstrcpyn(templ, LoadStr(resID), 200);
        RemoveAmpersands(templ);
        sprintf(subject, templ, expanded);
        str.Set(subject, count > 1 ? NULL : formatedFileName);
    }

    switch (type)
    {
    case fsatMove:
    case fsatCopy:
    {
        if (type == fsatMove && GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) ||
            type == fsatCopy && GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS)) // "always true"
        {
            // lower thread's priority to "normal" (so that operations don't burden the machine too much)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            BOOL copy = (type == fsatCopy);
            BOOL operationMask = FALSE;
            BOOL cancelOrHandlePath = FALSE;

            char targetPath[2 * MAX_PATH];
            if (target->Is(ptDisk))
                target->GetGeneralPath(targetPath, 2 * MAX_PATH);
            else
                targetPath[0] = 0;

            BOOL ret = GetPluginFS()->CopyOrMoveFromFS(copy, 1, GetPluginFS()->GetPluginFSName(),
                                                       HWindow, panel, count - selectedDirs,
                                                       selectedDirs, targetPath, operationMask,
                                                       cancelOrHandlePath, NULL);
            while (!ret)
            {
                if (!cancelOrHandlePath) // standard dialog
                {
                    if (CCopyMoveDialog(HWindow, targetPath, 2 * MAX_PATH,
                                        LoadStr(copy ? IDS_COPY : IDS_MOVE), &str,
                                        copy ? IDD_COPYDIALOG : IDD_MOVEDIALOG,
                                        Configuration.CopyHistory, COPY_HISTORY_SIZE,
                                        TRUE)
                            .Execute() == IDOK)
                    {
                        UpdateWindow(MainWindow->HWindow);

                        operationMask = FALSE;
                        cancelOrHandlePath = FALSE;
                        ret = GetPluginFS()->CopyOrMoveFromFS(copy, 2, GetPluginFS()->GetPluginFSName(),
                                                              HWindow, panel, count - selectedDirs,
                                                              selectedDirs, targetPath, operationMask,
                                                              cancelOrHandlePath, NULL);
                    }
                    else
                    {
                        UpdateWindow(MainWindow->HWindow);
                        ret = TRUE;
                        cancelOrHandlePath = TRUE; // cancel operation
                    }
                }
                else // standard path processing
                {
                    // restore DefaultDir
                    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

                    // for disk paths flip '/' to '\\' and drop duplicate '\\'
                    if (targetPath[0] != 0 && targetPath[1] == ':' || // paths like X:...
                        (targetPath[0] == '/' || targetPath[0] == '\\') &&
                            (targetPath[1] == '/' || targetPath[1] == '\\')) // UNC paths
                    {                                                        // this is a disk path (absolute or relative) - turn all '/' into '\\' and remove duplicate '\\'
                        SlashesToBackslashesAndRemoveDups(targetPath);
                    }

                    char errTitle[200];
                    lstrcpyn(errTitle, LoadStr(copy ? IDS_ERRORCOPY : IDS_ERRORMOVE), 200);
                    BOOL pathError = FALSE;

                    int len = (int)strlen(targetPath);
                    BOOL backslashAtEnd = (len > 0 && targetPath[len - 1] == '\\'); // path ends with backslash -> must be a directory
                    BOOL mustBePath = (len == 2 && LowerCase[targetPath[0]] >= 'a' && LowerCase[targetPath[0]] <= 'z' &&
                                       targetPath[1] == ':'); // a path like "c:" must stay a path after expansion (not a file)

                    int pathType;
                    BOOL pathIsDir;
                    char* secondPart;
                    int error;
                    if (ParsePath(targetPath, pathType, pathIsDir, secondPart, errTitle, NULL, &error, MAX_PATH))
                    {
                        // instead of using a 'switch' statement, we use 'if' so that 'break' and 'continue' work properly
                        if (pathType == PATH_TYPE_WINDOWS) // Windows path (disk + UNC)
                        {
                            char* mask;
                            if (SalSplitWindowsPath(HWindow, LoadStr(copy ? IDS_COPY : IDS_MOVE),
                                                    errTitle, count, targetPath, secondPart, pathIsDir,
                                                    backslashAtEnd || mustBePath, NULL, NULL, mask))
                            {
                                if (!operationMask && mask != NULL &&
                                    (strcmp(mask, "*.*") == 0 || strcmp(mask, "*") == 0))
                                {              // masks not supported and mask is empty, cut it off
                                    *mask = 0; // double-null terminated
                                }
                                if (!operationMask && mask != NULL && *mask != 0) // mask exists but isn't allowed
                                {
                                    char* e = targetPath + strlen(targetPath); // fix 'targetPath' (merge 'targetPath' and 'mask')
                                    if (e > targetPath && *(e - 1) != '\\')
                                        *e++ = '\\';
                                    if (e != mask)
                                        memmove(e, mask, strlen(mask) + 1); // move the mask if necessary

                                    SalMessageBox(HWindow, LoadStr(IDS_FSCOPYMOVE_OPMASKSNOTSUP),
                                                  errTitle, MB_OK | MB_ICONEXCLAMATION);
                                    pathError = TRUE; // path error -> mode==4
                                }
                            }
                            else
                                pathError = TRUE; // path error -> mode==4
                        }
                        else
                        {
                            SalMessageBox(HWindow,
                                          LoadStr(pathType == PATH_TYPE_ARCHIVE ? IDS_FSCOPYMOVE_ONLYDISK_A : IDS_FSCOPYMOVE_ONLYDISK_FS),
                                          errTitle, MB_OK | MB_ICONEXCLAMATION);
                            if (pathType == PATH_TYPE_ARCHIVE && (backslashAtEnd || mustBePath))
                            {
                                SalPathAddBackslash(targetPath, 2 * MAX_PATH);
                            }
                            pathError = TRUE; // path error -> mode==4
                        }
                    }
                    else
                    {
                        if (error == SPP_INCOMLETEPATH) // additionally report an error for a relative path on FS
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_FSCOPYMOVE_INCOMPLETEPATH), errTitle,
                                          MB_OK | MB_ICONEXCLAMATION);
                        }
                        pathError = TRUE; // path error -> mode==4
                    }

                    operationMask = FALSE;
                    cancelOrHandlePath = FALSE;
                    ret = GetPluginFS()->CopyOrMoveFromFS(copy, pathError ? 4 : 3,
                                                          GetPluginFS()->GetPluginFSName(), HWindow, panel,
                                                          count - selectedDirs, selectedDirs, targetPath,
                                                          operationMask, cancelOrHandlePath, NULL);
                }
            }

            if (ret && !cancelOrHandlePath)
            {
                if (targetPath[0] != 0) // switch focus to 'targetPath'
                {
                    lstrcpyn(NextFocusName, targetPath, MAX_PATH);
                    // RefreshDirectory may not run - the source might not have changed - just to be safe, post a message
                    PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                }

                unselect = TRUE; // operation successful, deselect the source
            }

            // raise thread's priority again, operation finished
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        }
        break;
    }

    case fsatDelete:
    {
        if (GetPluginFS()->IsServiceSupported(FS_SERVICE_DELETE)) // "always true"
        {
            // lower thread's priority to "normal" (so that operations don't burden the machine too much)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            BOOL cancelOrError = FALSE;
            BOOL ret = GetPluginFS()->Delete(GetPluginFS()->GetPluginFSName(), 1, HWindow,
                                             panel, count - selectedDirs,
                                             selectedDirs, cancelOrError);
            if (!cancelOrError) // not a cancel/operation error
            {
                if (!ret)
                {
                    int res;
                    if (Configuration.CnfrmFileDirDel)
                    {                                                                                                           // ask only if the user wants it
                        HICON hIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 16777 : 161), // delete icon
                                                               IMAGE_ICON, 32, 32, IconLRFlags));
                        int myRes = CMessageBox(HWindow, MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT,
                                                LoadStr(IDS_CONFIRM_DELETE_TITLE), &str, NULL,
                                                NULL, hIcon, 0, NULL, NULL, NULL, NULL)
                                        .Execute();
                        HANDLES(DestroyIcon(hIcon));
                        res = (myRes == IDYES ? IDOK : IDCANCEL);
                        UpdateWindow(MainWindow->HWindow);
                    }
                    else
                        res = IDOK;

                    if (res == IDOK)
                    {
                        ret = GetPluginFS()->Delete(GetPluginFS()->GetPluginFSName(), 2, HWindow,
                                                    panel, count - selectedDirs,
                                                    selectedDirs, cancelOrError);
                    }
                }

                if (ret && !cancelOrError)
                    unselect = TRUE; // operation completed successfully
            }

            // raise thread's priority again, operation finished
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        }
        break;
    }

    case fsatCountSize:
    {
        break;
    }

    case fsatChangeAttrs:
    {
        break;
    }
    }

    if (unselect) // should items be deselected?
    {
        SetSel(FALSE, -1, TRUE);                        // explicit redraw
        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
        UpdateWindow(MainWindow->HWindow);
    }

    EndStopRefresh();
    EndSuspendMode(); // the snooper resumes now
}

void CFilesWindow::RefreshVisibleItemsArray()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RefreshVisibleItemsArray()");

    if (!VisibleItemsArray.IsArrValid(NULL))
        VisibleItemsArray.RefreshArr(this);
    if (!VisibleItemsArraySurround.IsArrValid(NULL))
        VisibleItemsArraySurround.RefreshArr(this);
}

void CFilesWindow::DragDropToArcOrFS(CTmpDragDropOperData* data)
{
    CALL_STACK_MESSAGE1("CFilesWindow::DragDropToArcOrFS()");
    if (data->Data->Names.Count == 0)
        return; // nothing to do
    if (data->Data->SrcPath[0] == 0)
    {
        SalMessageBox(HWindow, LoadStr(IDS_SRCPATHUNICODEONLY),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return;
    }
    if (data->Data->Names.Count > 1) // sort file and directory names for faster array searching
        SortNames((char**)data->Data->Names.GetData(), 0, data->Data->Names.Count - 1);

    int* nameFound = NULL; // each name in data->Data->Names has TRUE/FALSE here (found/not found on disk)
    nameFound = (int*)malloc(sizeof(int) * data->Data->Names.Count);
    if (nameFound == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return;
    }
    memset(nameFound, 0, sizeof(int) * data->Data->Names.Count);

    CSalamanderDirectory* baseDir = new CSalamanderDirectory(TRUE); // container for files and directories from the source disk
    if (baseDir == NULL)
    {
        TRACE_E(LOW_MEMORY);
        if (nameFound != NULL)
            free(nameFound);
        return;
    }

    // load complete data about files and directories, their names are in data->Data
    char path[MAX_PATH + 10];
    lstrcpyn(path, data->Data->SrcPath, MAX_PATH);
    char* end = path + strlen(path);
    SalPathAppend(path, "*", MAX_PATH + 10);
    char text[2 * MAX_PATH + 100];
    WIN32_FIND_DATA file;
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    *end = 0; // fix the path
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            if (nameFound != NULL)
                free(nameFound);
            delete baseDir;
            return;
        }
    }
    else
    {
        BOOL ok = TRUE;
        CFileData newF;       // we no longer work with these items
        newF.PluginData = -1; // -1 is arbitrary, thevalue will be ignored
        newF.Association = 0;
        newF.Selected = 0;
        newF.Shared = 0;
        newF.Archive = 0;
        newF.SizeValid = 0;
        newF.Dirty = 0; // unnecessary, just for formality
        newF.CutToClip = 0;
        newF.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        newF.IconOverlayDone = 0;
        BOOL testFindNextErr = TRUE;

        do
        {
            if (file.cFileName[0] == 0 || file.cFileName[0] == '.' && (file.cFileName[1] == 0 ||
                                                                       (file.cFileName[1] == '.' && file.cFileName[2] == 0)))
                continue; // "." and ".."

            int foundIndex;
            if (ContainsString(&data->Data->Names, file.cFileName, &foundIndex))
            {
                if (nameFound[foundIndex] == FALSE)
                    nameFound[foundIndex] = TRUE;
                else // duplicate = all names are processed (some may not have been selected); if it causes issues, handle it with case-sensitive comparison first
                    TRACE_E("CFilesWindow::DragDropToArcOrFS(): duplicate names found! (names are compared case-insensitive)");
            }
            else
                continue; // user is not interested in this file/directory (name wasn't in the data object)

            newF.Name = DupStr(file.cFileName);
            newF.DosName = NULL;
            if (newF.Name == NULL)
            {
                ok = FALSE;
                testFindNextErr = FALSE;
                break;
            }
            newF.NameLen = strlen(newF.Name);
            if (!Configuration.SortDirsByExt && (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // directory, certainly a disk
            {
                newF.Ext = newF.Name + newF.NameLen; // directories have no extensions
            }
            else
            {
                newF.Ext = strrchr(newF.Name, '.');
                if (newF.Ext == NULL)
                    newF.Ext = newF.Name + newF.NameLen; // ".cvspass" in Windows is an extension ...
                                                         //      if (newF.Ext == NULL || newF.Ext == newF.Name) newF.Ext = newF.Name + newF.NameLen;
                else
                    newF.Ext++;
            }

            if (file.cAlternateFileName[0] != 0)
            {
                newF.DosName = DupStr(file.cAlternateFileName);
                if (newF.DosName == NULL)
                {
                    free(newF.Name);
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
            }

            newF.Size = CQuadWord(file.nFileSizeLow, file.nFileSizeHigh);
            newF.Attr = file.dwFileAttributes;
            newF.LastWrite = file.ftLastWriteTime;
            newF.Hidden = newF.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            newF.IsOffline = newF.Attr & FILE_ATTRIBUTE_OFFLINE ? 1 : 0;

            if (newF.Attr & FILE_ATTRIBUTE_DIRECTORY) // directory, certainly a disk
            {
                newF.IsLink = (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // volume mount point or junction point = display directory with link overlay
            }
            else
            {
                if (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT)
                    newF.IsLink = 1; // if the file is a reparse point (might not be even possible) = display it with a link overlay
                else
                    newF.IsLink = IsFileLink(newF.Ext);
            }

            if ((newF.Attr & FILE_ATTRIBUTE_DIRECTORY) && !baseDir->AddDir("", newF, NULL) ||     // directory, certainly a disk
                (newF.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && !baseDir->AddFile("", newF, NULL)) // file
            {
                free(newF.Name);
                if (newF.DosName != NULL)
                    free(newF.DosName);
                ok = FALSE;
                testFindNextErr = FALSE;
                break;
            }
        } while (FindNextFile(find, &file));
        DWORD err = GetLastError();
        HANDLES(FindClose(find));

        if (testFindNextErr && err != ERROR_NO_MORE_FILES)
        {
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            if (nameFound != NULL)
                free(nameFound);
            delete baseDir;
            return;
        }

        if (!ok)
        {
            if (nameFound != NULL)
                free(nameFound);
            delete baseDir;
            return;
        }
    }

    // check if we found all files and directories (i.e., found all names)
    BOOL cancel = FALSE;
    int i;
    for (i = 0; i < data->Data->Names.Count; i++)
    {
        if (nameFound[i] == FALSE)
        {
            if (SalMessageBox(HWindow, LoadStr(IDS_SRCFILESNOTFOUND), LoadStr(IDS_QUESTION),
                              MB_YESNO | MSGBOXEX_ESCAPEENABLED | MB_ICONQUESTION) == IDNO)
            {
                cancel = TRUE;
            }
            break;
        }
    }

    if (!cancel && !FilesActionInProgress &&
        CheckPath(TRUE, data->Data->SrcPath) == ERROR_SUCCESS)
    { // perform the operation itself
        FilesActionInProgress = TRUE;

        BeginSuspendMode(); // the snooper takes a break
        BeginStopRefresh(); // to suppress path change messages

        CPanelTmpEnumData dataEnum;
        dataEnum.Dirs = baseDir->GetDirs("");
        dataEnum.Files = baseDir->GetFiles("");
        dataEnum.IndexesCount = dataEnum.Dirs->Count + dataEnum.Files->Count;
        for (i = 0; i < dataEnum.IndexesCount; i++)
            nameFound[i] = i;
        dataEnum.Indexes = nameFound;
        lstrcpyn(dataEnum.WorkPath, data->Data->SrcPath, MAX_PATH);
        dataEnum.EnumLastIndex = -1;

        if (dataEnum.IndexesCount > 0)
        {
            if (data->ToArchive)
            {
                //---  check whether it is a zero-length file
                BOOL nullFile;
                BOOL haveSize = FALSE;
                CQuadWord size;
                DWORD err;
                HANDLE hFile = HANDLES_Q(CreateFile(data->ArchiveOrFSName, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL));
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    haveSize = SalGetFileSize(hFile, size, err);
                    HANDLES(CloseHandle(hFile));
                }
                else
                    err = GetLastError();
                if (haveSize)
                {
                    nullFile = (size == CQuadWord(0, 0));

                    //---  if it is a zero-length file we must delete it, archivers can't handle them
                    DWORD nullFileAttrs;
                    if (nullFile)
                    {
                        nullFileAttrs = SalGetFileAttributes(data->ArchiveOrFSName);
                        ClearReadOnlyAttr(data->ArchiveOrFSName, nullFileAttrs); // to allow deletion even if read-only
                        DeleteFile(data->ArchiveOrFSName);
                    }
                    //---  actual packing
                    SetCurrentDirectory(data->Data->SrcPath);
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    if (PackCompress(HWindow, this, data->ArchiveOrFSName, data->ArchivePathOrUserPart,
                                     !data->Copy, data->Data->SrcPath, PanelEnumDiskSelection, &dataEnum))
                    {                   // packing succeeded
                        if (nullFile && // zero-length file might have had a different compressed attribute, set archive accordingly
                            nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                        {
                            HANDLE hFile2 = HANDLES_Q(CreateFile(data->ArchiveOrFSName, GENERIC_READ | GENERIC_WRITE,
                                                                 0, NULL, OPEN_EXISTING,
                                                                 0, NULL));
                            if (hFile2 != INVALID_HANDLE_VALUE)
                            {
                                // restore the "compressed" flag; on FAT and FAT32 it simply won't succeed
                                USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                ULONG length;
                                DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                sizeof(USHORT), NULL, 0, &length, FALSE);
                                HANDLES(CloseHandle(hFile2));
                                SetFileAttributes(data->ArchiveOrFSName, nullFileAttrs);
                            }
                        }
                    }
                    else
                    {
                        if (nullFile) // operation failed, we must recreate it
                        {
                            HANDLE hFile2 = HANDLES_Q(CreateFile(data->ArchiveOrFSName, GENERIC_READ | GENERIC_WRITE,
                                                                 0, NULL, OPEN_ALWAYS,
                                                                 0, NULL));
                            if (hFile2 != INVALID_HANDLE_VALUE)
                            {
                                if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                {
                                    // restore the "compressed" flag; on FAT and FAT32 it simply won't succeed
                                    USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                    ULONG length;
                                    DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                    sizeof(USHORT), NULL, 0, &length, FALSE);
                                }
                                HANDLES(CloseHandle(hFile2));
                                if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                    SetFileAttributes(data->ArchiveOrFSName, nullFileAttrs);
                            }
                        }
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                    SetCurrentDirectoryToSystem();

                    UpdateWindow(MainWindow->HWindow);

                    //---  refresh non-automatically refreshed directories
                    // change in the directory with the target archive (archive file is modified)
                    lstrcpyn(text, data->ArchiveOrFSName, MAX_PATH);
                    CutDirectory(text); // 'text' is the archive name -> must always succeed
                    MainWindow->PostChangeOnPathNotification(text, FALSE);
                    if (!data->Copy)
                    {
                        // changes on the source path (when moving files to the archive,
                        // files/directories should have been deleted)
                        MainWindow->PostChangeOnPathNotification(data->Data->SrcPath, TRUE);
                    }
                }
                else
                {
                    sprintf(text, LoadStr(IDS_FILEERRORFORMAT), data->ArchiveOrFSName, GetErrorText(err));
                    SalMessageBox(HWindow, text,
                                  data->Copy ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                  MB_OK | MB_ICONEXCLAMATION);
                }
            }
            else // to FS
            {
                int selFiles = dataEnum.Files->Count;
                int selDirs = dataEnum.Dirs->Count;

                // lower thread's priority to "normal" (so that operations don't burden the machine too much)
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                // select the FS that performs the operation (priority: active, then new)
                char targetPath[2 * MAX_PATH];
                BOOL done = FALSE;
                CPluginFSInterfaceEncapsulation* fs = NULL;
                if (Is(ptPluginFS))
                    fs = GetPluginFS();

                int fsNameIndex;
                if (fs != NULL && fs->NotEmpty() &&                                         // interface is valid
                    fs->IsFSNameFromSamePluginAsThisFS(data->ArchiveOrFSName, fsNameIndex)) // FS name is from the same plugin (otherwise it's not worth trying)
                {
                    BOOL invalidPathOrCancel;
                    _snprintf_s(targetPath, _TRUNCATE, "%s:%s", data->ArchiveOrFSName, data->ArchivePathOrUserPart);
                    if (fs->CopyOrMoveFromDiskToFS(data->Copy, 3, fs->GetPluginFSName(),
                                                   HWindow, data->Data->SrcPath,
                                                   PanelEnumDiskSelection, &dataEnum,
                                                   selFiles, selDirs, targetPath, &invalidPathOrCancel))
                    {
                        done = TRUE; // finished/canceled/error (either way,no point in trying a new FS)
                    }
                    else
                    {
                        // reset before next use (so enumeration starts again from the beginning)
                        dataEnum.Reset();

                        if (invalidPathOrCancel)
                            done = TRUE; // invalid path + user cannot fix it, ending
                                         // else ; // we should try a new FS
                    }
                }
                if (!done) // active FS failed, create a new FS
                {
                    int index;
                    int fsNameIndex2;
                    if (Plugins.IsPluginFS(data->ArchiveOrFSName, index, fsNameIndex2)) // determine plugin index
                    {
                        // obtain the plug-in associated with the FS
                        CPluginData* plugin = Plugins.Get(index);
                        if (plugin != NULL)
                        {
                            // open a new FS
                            // load the plug-in before obtaining DLLName, Version and plugin interfaces
                            CPluginFSInterfaceAbstract* auxFS = plugin->OpenFS(data->ArchiveOrFSName, fsNameIndex2);
                            CPluginFSInterfaceEncapsulation pluginFS(auxFS, plugin->DLLName, plugin->Version,
                                                                     plugin->GetPluginInterfaceForFS()->GetInterface(),
                                                                     plugin->GetPluginInterface()->GetInterface(),
                                                                     data->ArchiveOrFSName, fsNameIndex2, -1, 0,
                                                                     plugin->BuiltForVersion);
                            if (pluginFS.NotEmpty())
                            {
                                Plugins.SetWorkingPluginFS(&pluginFS);
                                BOOL invalidPathOrCancel;
                                _snprintf_s(targetPath, _TRUNCATE, "%s:%s", data->ArchiveOrFSName, data->ArchivePathOrUserPart);
                                if (!pluginFS.CopyOrMoveFromDiskToFS(data->Copy, 3, pluginFS.GetPluginFSName(),
                                                                     HWindow, data->Data->SrcPath,
                                                                     PanelEnumDiskSelection, &dataEnum,
                                                                     selFiles, selDirs, targetPath,
                                                                     &invalidPathOrCancel))
                                { // syntax error/plugin error
                                    if (!invalidPathOrCancel)
                                    { // plugin error (new FS, but returns error 'this FS cannot perform the requested operation')
                                        TRACE_E("CopyOrMoveFromDiskToFS on new (empty) FS may not return error 'unable to process operation'.");
                                    }
                                }

                                pluginFS.ReleaseObject(HWindow);
                                plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS.GetInterface());
                                Plugins.SetWorkingPluginFS(NULL);
                            }
                            else
                                TRACE_E("Plugin has refused to open FS (maybe it even does not start).");
                        }
                        else
                            TRACE_E("Unexpected situation in CFilesWindow::DragDropToArcOrFS() - unable to work with plugin.");
                    }
                    else
                    {
                        TRACE_E("Unexpected situation in CFilesWindow::DragDropToArcOrFS() - file-system " << data->ArchiveOrFSName << " was not found.");
                    }
                }

                // raise thread's priority again, operation finished
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                SetCurrentDirectoryToSystem(); // restore current directory in any case
            }
        }

        EndStopRefresh();
        EndSuspendMode();

        FilesActionInProgress = FALSE;
    }
    else
    {
        if (FilesActionInProgress)
            TRACE_E("Unexpected situation in CFilesWindow::DragDropToArcOrFS(): FilesActionInProgress is TRUE!");
    }

    // release data
    if (nameFound != NULL)
        free(nameFound);
    delete baseDir;
}

//****************************************************************************
//
// CVisibleItemsArray
//

CVisibleItemsArray::CVisibleItemsArray(BOOL surroundArr)
{
    HANDLES(InitializeCriticalSection(&Monitor));
    ArrVersionNum = 0;
    ArrIsValid = FALSE;
    ArrNames = NULL;
    ArrNamesCount = 0;
    ArrNamesAllocated = 0;
    SurroundArr = surroundArr;
    FirstVisibleItem = -1;
    LastVisibleItem = -1;
}

CVisibleItemsArray::~CVisibleItemsArray()
{
    HANDLES(DeleteCriticalSection(&Monitor));
    if (ArrNamesAllocated > 0 && ArrNames != NULL)
        free(ArrNames);
    ArrNamesCount = 0;
    ArrNamesAllocated = 0;
}

BOOL CVisibleItemsArray::IsArrValid(int* versionNum)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE1("CVisibleItemsArray::IsArrValid()");
    HANDLES(EnterCriticalSection(&Monitor));
    if (versionNum != NULL)
        *versionNum = ArrVersionNum;
    BOOL ret = ArrIsValid;
    HANDLES(LeaveCriticalSection(&Monitor));
    return ret;
}

void CVisibleItemsArray::InvalidateArr()
{
    CALL_STACK_MESSAGE1("CVisibleItemsArray::InvalidateArr()");
    HANDLES(EnterCriticalSection(&Monitor));
    ArrIsValid = FALSE;
    ArrNamesCount = 0;
    FirstVisibleItem = -1;
    LastVisibleItem = -1;
    HANDLES(LeaveCriticalSection(&Monitor));
}

void SortNamesCS(char** names, int left, int right)
{
    int i = left, j = right;
    char* pivot = names[(i + j) / 2];

    do
    {
        while (strcmp(names[i], pivot) < 0 && i < right)
            i++;
        while (strcmp(pivot, names[j]) < 0 && j > left)
            j--;

        if (i <= j)
        {
            char* swap = names[i];
            names[i] = names[j];
            names[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        SortNamesCS(names, left, j);
    if (i < right)
        SortNamesCS(names, i, right);
}

void CVisibleItemsArray::RefreshArr(CFilesWindow* panel)
{
    CALL_STACK_MESSAGE1("CVisibleItemsArray::RefreshArr()");
    HANDLES(EnterCriticalSection(&Monitor));
    int firstIndex, count;
    panel->ListBox->GetVisibleItems(&firstIndex, &count);

    if (SurroundArr)
    {
        int origFirstIndex = firstIndex;
        if (firstIndex > 0)
            firstIndex -= count;
        if (firstIndex < 0)
            firstIndex = 0;
        count = count * 2 + (origFirstIndex - firstIndex);
    }

    int dirsCount = panel->Dirs->Count;
    int filesCount = panel->Files->Count;
    if (firstIndex + count > filesCount + dirsCount)
        count = filesCount + dirsCount - firstIndex;
    if (count < 0)
        count = 0;
    if (count > ArrNamesAllocated)
    {
        char** n = (char**)realloc(ArrNames, count * sizeof(char*));
        if (n != NULL)
        {
            ArrNames = n;
            ArrNamesAllocated = count;
        }
        else
        {
            TRACE_E(LOW_MEMORY);
            if (ArrNames != NULL)
                free(ArrNames);
            ArrNamesAllocated = 0;
            ArrNamesCount = 0;
        }
    }
    if (count <= ArrNamesAllocated)
    {
        // TRACE_I("VisibleItemsArray: firstIndex=" << firstIndex << ", count=" << count);
        int end = firstIndex + count;
        int x = 0;
        int i;
        for (i = firstIndex; i < end; i++)
        {
            CFileData* f = &(i < dirsCount ? panel->Dirs->At(i) : panel->Files->At(i - dirsCount));
            ArrNames[x++] = f->Name;
            //#ifdef _DEBUG
            //      if (i == firstIndex) TRACE_I("VisibleItemsArray: first=" << f->Name);
            //      if (i + 1 == end) TRACE_I("VisibleItemsArray: last=" << f->Name);
            //#endif // _DEBUG
        }
        ArrNamesCount = count;
        if (ArrNamesCount > 1)
            SortNamesCS(ArrNames, 0, ArrNamesCount - 1);
        ArrIsValid = TRUE;
        ArrVersionNum++;
        FirstVisibleItem = firstIndex;
        LastVisibleItem = firstIndex + count - 1;
    }
    HANDLES(LeaveCriticalSection(&Monitor));
}

BOOL CVisibleItemsArray::ArrContains(const char* name, BOOL* isArrValid, int* versionNum)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CVisibleItemsArray::ArrContains()");
    HANDLES(EnterCriticalSection(&Monitor));
    if (versionNum != NULL)
        *versionNum = ArrVersionNum;
    if (isArrValid != NULL)
        *isArrValid = ArrIsValid;
    if (!ArrIsValid || ArrNamesCount <= 0)
    {
        HANDLES(LeaveCriticalSection(&Monitor));
        return FALSE;
    }

    int l = 0, r = ArrNamesCount - 1, m;
    while (1)
    {
        m = (l + r) / 2;
        int res = strcmp(name, ArrNames[m]);
        if (res == 0)
        {
            HANDLES(LeaveCriticalSection(&Monitor));
            return TRUE; // found
        }
        else
        {
            if (res < 0)
            {
                if (l == r || l > m - 1)
                {
                    HANDLES(LeaveCriticalSection(&Monitor));
                    return FALSE; // not found
                }
                r = m - 1;
            }
            else
            {
                if (l == r)
                {
                    HANDLES(LeaveCriticalSection(&Monitor));
                    return FALSE; // not found
                }
                l = m + 1;
            }
        }
    }
}

BOOL CVisibleItemsArray::ArrContainsIndex(int index, BOOL* isArrValid, int* versionNum)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CVisibleItemsArray::ArrContainsIndex()");
    HANDLES(EnterCriticalSection(&Monitor));
    if (versionNum != NULL)
        *versionNum = ArrVersionNum;
    if (isArrValid != NULL)
        *isArrValid = ArrIsValid;
    if (!ArrIsValid || ArrNamesCount <= 0)
    {
        HANDLES(LeaveCriticalSection(&Monitor));
        return FALSE;
    }

    BOOL ret = index >= FirstVisibleItem && index <= LastVisibleItem;
    HANDLES(LeaveCriticalSection(&Monitor));
    return ret;
}

//****************************************************************************
//
// CCriteriaData
//

CCriteriaData::CCriteriaData()
{
    Reset();
}

void CCriteriaData::Reset()
{
    OverwriteOlder = FALSE;
    StartOnIdle = FALSE;
    CopySecurity = FALSE;
    CopyAttrs = FALSE;
    PreserveDirTime = FALSE;
    IgnoreADS = FALSE;
    SkipEmptyDirs = FALSE;
    UseMasks = FALSE;
    Masks.SetMasksString("*.*");
    UseAdvanced = FALSE;
    Advanced.Reset();
    UseSpeedLimit = FALSE;
    SpeedLimit = 1;
}

CCriteriaData&
CCriteriaData::operator=(const CCriteriaData& s)
{
    OverwriteOlder = s.OverwriteOlder;
    StartOnIdle = s.StartOnIdle;
    CopySecurity = s.CopySecurity;
    CopyAttrs = s.CopyAttrs;
    PreserveDirTime = s.PreserveDirTime;
    IgnoreADS = s.IgnoreADS;
    SkipEmptyDirs = s.SkipEmptyDirs;
    UseMasks = s.UseMasks;
    Masks = s.Masks;
    UseAdvanced = s.UseAdvanced;
    memmove(&Advanced, &s.Advanced, sizeof(Advanced));
    UseSpeedLimit = s.UseSpeedLimit;
    SpeedLimit = s.SpeedLimit;

    return *this;
}

BOOL CCriteriaData::IsDirty()
{
    return OverwriteOlder || StartOnIdle || CopySecurity || CopyAttrs ||
           PreserveDirTime || IgnoreADS || SkipEmptyDirs || UseMasks ||
           UseAdvanced || UseSpeedLimit;
}

BOOL CCriteriaData::AgreeMasksAndAdvanced(const CFileData* file)
{
    if (UseMasks && !Masks.AgreeMasks(file->Name, file->Ext))
        return FALSE;

    if (UseAdvanced)
    {
        if (!Advanced.Test(file->Attr, &file->Size, &file->LastWrite))
            return FALSE;
    }

    return TRUE;
}

BOOL CCriteriaData::AgreeMasksAndAdvanced(const WIN32_FIND_DATA* file)
{
    if (UseMasks && !Masks.AgreeMasks(file->cFileName, NULL))
        return FALSE;

    if (UseAdvanced)
    {
        CQuadWord size(file->nFileSizeLow, file->nFileSizeHigh);
        if (!Advanced.Test(file->dwFileAttributes, &size, &file->ftLastWriteTime))
            return FALSE;
    }

    return TRUE;
}

const char* CRITERIADATA_OVERWRITEOLDER_REG = "Overwrite Older";
const char* CRITERIADATA_STARTONIDLE_REG = "Start On Idle";
const char* CRITERIADATA_COPYSECURITY_REG = "Copy Security";
const char* CRITERIADATA_COPYATTRIBUTES_REG = "Copy Attributes";
const char* CRITERIADATA_PRESERVEDIRTIME_REG = "Preserve Dir Time";
const char* CRITERIADATA_IGNOREADS_REG = "Ignore ADS";
const char* CRITERIADATA_SKIPEMPTYDIRS_REG = "Skip Empty Dirs";
const char* CRITERIADATA_USENAMEMASK_REG = "Use Name Masks";
const char* CRITERIADATA_NAMEMASKS_REG = "Name Masks";
const char* CRITERIADATA_USESPEEDLIMIT_REG = "Use Speed Limit";
const char* CRITERIADATA_SPEEDLIMIT_REG = "Speed Limit";
const char* CRITERIADATA_USEADVANCED_REG = "Use Advanced";

BOOL CCriteriaData::Save(HKEY hKey)
{
    // to optimize for registry size, only non-default values are saved.
    // this requires clearing the destination key before writing.
    CCriteriaData def;

    if (OverwriteOlder != def.OverwriteOlder)
        SetValue(hKey, CRITERIADATA_OVERWRITEOLDER_REG, REG_DWORD, &OverwriteOlder, sizeof(DWORD));
    if (StartOnIdle != def.StartOnIdle)
        SetValue(hKey, CRITERIADATA_STARTONIDLE_REG, REG_DWORD, &StartOnIdle, sizeof(DWORD));
    if (CopySecurity != def.CopySecurity)
        SetValue(hKey, CRITERIADATA_COPYSECURITY_REG, REG_DWORD, &CopySecurity, sizeof(DWORD));
    if (CopyAttrs != def.CopyAttrs)
        SetValue(hKey, CRITERIADATA_COPYATTRIBUTES_REG, REG_DWORD, &CopyAttrs, sizeof(DWORD));
    if (PreserveDirTime != def.PreserveDirTime)
        SetValue(hKey, CRITERIADATA_PRESERVEDIRTIME_REG, REG_DWORD, &PreserveDirTime, sizeof(DWORD));
    if (IgnoreADS != def.IgnoreADS)
        SetValue(hKey, CRITERIADATA_IGNOREADS_REG, REG_DWORD, &IgnoreADS, sizeof(DWORD));
    if (SkipEmptyDirs != def.SkipEmptyDirs)
        SetValue(hKey, CRITERIADATA_SKIPEMPTYDIRS_REG, REG_DWORD, &SkipEmptyDirs, sizeof(DWORD));
    if (UseMasks != def.UseMasks)
        SetValue(hKey, CRITERIADATA_USENAMEMASK_REG, REG_DWORD, &UseMasks, sizeof(DWORD));
    if (strcmp(Masks.GetMasksString(), def.Masks.GetMasksString()) != 0)
        SetValue(hKey, CRITERIADATA_NAMEMASKS_REG, REG_SZ, Masks.GetMasksString(), -1);
    if (UseSpeedLimit != def.UseSpeedLimit)
        SetValue(hKey, CRITERIADATA_USESPEEDLIMIT_REG, REG_DWORD, &UseSpeedLimit, sizeof(DWORD));
    if (SpeedLimit != def.SpeedLimit)
        SetValue(hKey, CRITERIADATA_SPEEDLIMIT_REG, REG_DWORD, &SpeedLimit, sizeof(DWORD));
    if (UseAdvanced != def.UseAdvanced)
        SetValue(hKey, CRITERIADATA_USEADVANCED_REG, REG_DWORD, &UseAdvanced, sizeof(DWORD));

    Advanced.Save(hKey);

    return TRUE;
}

BOOL CCriteriaData::Load(HKEY hKey)
{
    GetValue(hKey, CRITERIADATA_OVERWRITEOLDER_REG, REG_DWORD, &OverwriteOlder, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_STARTONIDLE_REG, REG_DWORD, &StartOnIdle, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_COPYSECURITY_REG, REG_DWORD, &CopySecurity, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_COPYATTRIBUTES_REG, REG_DWORD, &CopyAttrs, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_PRESERVEDIRTIME_REG, REG_DWORD, &PreserveDirTime, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_IGNOREADS_REG, REG_DWORD, &IgnoreADS, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_SKIPEMPTYDIRS_REG, REG_DWORD, &SkipEmptyDirs, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_USENAMEMASK_REG, REG_DWORD, &UseMasks, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_NAMEMASKS_REG, REG_SZ, Masks.GetWritableMasksString(), MAX_GROUPMASK);
    GetValue(hKey, CRITERIADATA_USESPEEDLIMIT_REG, REG_DWORD, &UseSpeedLimit, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_SPEEDLIMIT_REG, REG_DWORD, &SpeedLimit, sizeof(DWORD));
    GetValue(hKey, CRITERIADATA_USEADVANCED_REG, REG_DWORD, &UseAdvanced, sizeof(DWORD));

    Advanced.Load(hKey);

    return TRUE;
}

//****************************************************************************
//
// CCopyMoveOptions
//

void CCopyMoveOptions::Set(const CCriteriaData* item)
{
    // for now, only one item (default) or none is held
    if (Items.Count > 0)
        Items.DestroyMembers();
    if (item != NULL)
    {
        CCriteriaData* itemCopy = new CCriteriaData();
        *itemCopy = *item;
        Items.Add(itemCopy);
    }
}

const CCriteriaData*
CCopyMoveOptions::Get()
{
    if (Items.Count == 1)
        return Items[0];
    else
        return NULL;
}

BOOL CCopyMoveOptions::Save(HKEY hKey)
{
    ClearKey(hKey);
    HKEY subKey;
    char buf[30];
    int i;
    for (i = 0; i < Items.Count; i++)
    {
        itoa(i + 1, buf, 10);
        if (CreateKey(hKey, buf, subKey))
        {
            Items[i]->Save(subKey);
            CloseKey(subKey);
        }
        else
            break;
    }
    return TRUE;
}

BOOL CCopyMoveOptions::Load(HKEY hKey)
{
    HKEY subKey;
    char buf[30];
    int i = 1;
    strcpy(buf, "1");
    Items.DestroyMembers();
    while (OpenKey(hKey, buf, subKey) && i == 1) // for now read only the first item
    {
        CCriteriaData* item = new CCriteriaData();
        item->Load(subKey);
        Items.Add(item);
        itoa(++i, buf, 10);
        CloseKey(subKey);
    }
    return TRUE;
}

CCopyMoveOptions CopyMoveOptions;
