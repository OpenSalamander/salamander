// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "snooper.h"
#include "worker.h"
#include "pack.h"
#include "mapi.h"

//
// ****************************************************************************
// CFilesWindow
//

int DeleteThroughRecycleBinAux(SHFILEOPSTRUCT* fo)
{
    __try
    {
        return SHFileOperation(fo);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 5))
    {
        FGIExceptionHasOccured++;
    }
    return 1; // != 0
}

BOOL PathContainsValidComponents(char* path, BOOL cutPath)
{
    char* s = path;
    while (*s != 0)
    {
        char* slash = strchr(s, '\\');
        if (slash == NULL)
            s += strlen(s);
        else
            s = slash;
        if (s > path && (*(s - 1) <= ' ' || *(s - 1) == '.'))
        {
            if (cutPath)
                *s = 0;
            return FALSE;
        }
        if (slash != NULL)
            s++;
    }
    return TRUE;
}

BOOL CFilesWindow::DeleteThroughRecycleBin(int* selection, int selCount, CFileData* oneFile)
{
    CALL_STACK_MESSAGE2("CFilesWindow::DeleteThroughRecycleBin(, %d,)", selCount);

    int i = 0;
    char path[MAX_PATH];
    strcpy(path, GetPath());
    SalPathAddBackslash(path, MAX_PATH);
    int pathLen = (int)strlen(path);
    // Verify that the path does not contain components ending with a space or dot;
    // as this can confuse the Recycle Bin and cause it to delete from a different path (it quietly trims
    // those spaces or dots)
    if (!PathContainsValidComponents(path, TRUE))
    {
        char textBuf[2 * MAX_PATH + 200];
        sprintf(textBuf, LoadStr(IDS_RECYCLEBINERROR), path);
        SalMessageBox(MainWindow->HWindow, textBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // quick dirty bloody hack - Recycle Bin simply cannot handle names ending with a space or dot (it deletes a different name created by trimming those characters, which we definitely don't want)
    }
    CDynamicStringImp names;
    do
    {
        if (selCount > 0)
        {
            oneFile = (selection[i] < Dirs->Count) ? &Dirs->At(selection[i]) : &Files->At(selection[i] - Dirs->Count);
            i++;
        }
        if (oneFile->Name[oneFile->NameLen - 1] <= ' ' || oneFile->Name[oneFile->NameLen - 1] == '.')
        {
            char textBuf[2 * MAX_PATH + 200];
            sprintf(textBuf, LoadStr(IDS_RECYCLEBINERROR), oneFile->Name);
            SalMessageBox(MainWindow->HWindow, textBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            return FALSE; // quick dirty bloody hack - Recycle Bin simply cannot handle names ending with a space or dot (it deletes a different name created by trimming those characters, which we definitely do not want)
        }
        // oneFile points to the selected item or the caret item in the filebox
        if (!names.Add(path, pathLen) ||
            !names.Add(oneFile->Name, oneFile->NameLen + 1))
            return FALSE;
    } while (i < selCount);
    if (!names.Add("\0", 2))
        return FALSE; // two zeros for the case of an empty list ("always false")

    SetCurrentDirectory(GetPath()); // for faster operation

    CShellExecuteWnd shellExecuteWnd;
    SHFILEOPSTRUCT fo;
    fo.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: CFilesWindow::DeleteThroughRecycleBin");
    fo.wFunc = FO_DELETE;
    fo.pFrom = names.Text;
    fo.pTo = NULL;
    fo.fFlags = FOF_ALLOWUNDO;
    fo.fAnyOperationsAborted = FALSE;
    fo.hNameMappings = NULL;
    fo.lpszProgressTitle = "";
    // Perform the actual deletion - wonderfully simple, unfortunately it sometimes crashes ;-)
    CALL_STACK_MESSAGE1("CFilesWindow::DeleteThroughRecycleBin::SHFileOperation");
    BOOL ret = DeleteThroughRecycleBinAux(&fo) == 0;
    SetCurrentDirectoryToSystem();

    return FALSE; /*ret && !fo.fAnyOperationsAborted*/
    ;             // for now, we simply keep the selection, the return value does not work
}

void PluginFSConvertPathToExternal(char* path)
{
    char fsName[MAX_PATH];
    char* fsUserPart;
    int index;
    int fsNameIndex;
    if (IsPluginFSPath(path, fsName, (const char**)&fsUserPart) &&
        Plugins.IsPluginFS(fsName, index, fsNameIndex))
    {
        CPluginData* plugin = Plugins.Get(index);
        if (plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, FALSE)) // the plugin may not be loaded, let it load if needed
            plugin->GetPluginInterfaceForFS()->ConvertPathToExternal(fsName, fsNameIndex, fsUserPart);
    }
}

// countSizeMode - 0 normal calculation, 1 calculation for the selected item,
// 2 calculation for all subdirectories
void CFilesWindow::FilesAction(CActionType type, CFilesWindow* target, int countSizeMode)
{
    CALL_STACK_MESSAGE3("CFilesWindow::FilesAction(%d, , %d)", type, countSizeMode);
    if (Dirs->Count + Files->Count == 0)
        return;

    // restore DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BOOL invertRecycleBin = (GetKeyState(VK_SHIFT) & 0x8000) != 0;

    if (!FilesActionInProgress)
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return; // the source is always the current directory

        if (type != atCountSize && countSizeMode != 0)
            countSizeMode = 0; // just to be sure (only countSizeMode is checked later)

        FilesActionInProgress = TRUE;

        BeginSuspendMode(); // the snooper takes a break
        BeginStopRefresh(); // just to prevent path change notifications from being distributed

        //---  find out how many directories and files are selected
        BOOL subDir;
        if (Dirs->Count > 0)
            subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
        else
            subDir = FALSE;

        int* indexes = NULL;
        int files = 0;
        int dirs = 0;
        int count = GetSelCount();
        if (countSizeMode == 0 && count > 0)
        {
            indexes = new int[count];
            if (indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                FilesActionInProgress = FALSE;
                EndStopRefresh();
                EndSuspendMode(); // the snooper starts again now
                return;
            }
            else
            {
                GetSelItems(count, indexes);
                int i = count;
                while (i--)
                {
                    if (indexes[i] < Dirs->Count)
                        dirs++;
                    else
                        files++;
                }
            }
        }
        else
        {
            if (countSizeMode == 2) // calculate size of all subdirectories
            {
                count = Dirs->Count;
                if (subDir)
                    count--;
                if (count > 0)
                {
                    indexes = new int[count];
                    if (indexes == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh();
                        EndSuspendMode(); // the snooper starts again now
                        return;
                    }
                    else
                    {
                        int i = (subDir ? 1 : 0);
                        int j;
                        for (j = 0; j < count; j++)
                            indexes[j] = i++;
                    }
                }
            }
            else
                count = 0;
        }

#ifndef _WIN64
        if (Windows64Bit && (type == atMove || type == atDelete))
        {
            int oneIndex;
            if (count == 0)
            {
                oneIndex = GetCaretIndex();
                if (oneIndex == 0 && subDir)
                    oneIndex = -1;
            }
            char redirectedDir[MAX_PATH];
            if ((count > 0 || oneIndex != -1) &&
                ContainsWin64RedirectedDir(this, count > 0 ? indexes : &oneIndex,
                                           count > 0 ? count : 1, redirectedDir, FALSE))
            {
                char msg[300 + MAX_PATH];
                _snprintf_s(msg, _TRUNCATE,
                            LoadStr(type == atMove ? IDS_ERRMOVESELCONTW64ALIAS : IDS_ERRDELETESELCONTW64ALIAS),
                            redirectedDir);
                SalMessageBox(HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);

                if (indexes != NULL)
                    delete[] (indexes);
                FilesActionInProgress = FALSE;
                EndStopRefresh();
                EndSuspendMode(); // the snooper starts again now
                return;
            }
        }
#endif // _WIN64

        //---  build the target path for copy/move
        char path[2 * MAX_PATH + 200]; // +200 is a reserve (Windows can create paths longer than MAX_PATH)
        target->GetGeneralPath(path, 2 * MAX_PATH + 200);
        if (target->Is(ptDisk))
        {
            SalPathAppend(path, "*.*", 2 * MAX_PATH + 200);
        }
        else
        {
            if (target->Is(ptZIPArchive))
            {
                SalPathAddBackslash(path, 2 * MAX_PATH + 200);

                // if packing to the archive in the other panel is not possible, leave the path empty
                int format = PackerFormatConfig.PackIsArchive(target->GetZIPArchive());
                if (format != 0) // we found a supported archive
                {
                    if (!PackerFormatConfig.GetUsePacker(format - 1)) // no edit -> empty operation target
                    {
                        path[0] = 0;
                    }
                }
            }
            else
            {
                if (target->Is(ptPluginFS) && (type == atCopy || type == atMove))
                {
                    if (target->GetPluginFS()->NotEmpty() &&
                        (type == atCopy && target->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMDISKTOFS) ||
                         type == atMove && target->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMDISKTOFS)))
                    {
                        // // this is just a modification of the target path text in the plug-in -> no point in lowering the thread's priority
                        int selFiles = 0;
                        int selDirs = 0;
                        if (count > 0) // some files are selected
                        {
                            selFiles = files;
                            selDirs = count - files;
                        }
                        else // take the focused item
                        {
                            int index = GetCaretIndex();
                            if (index >= Dirs->Count)
                                selFiles = 1;
                            else
                                selDirs = 1;
                        }
                        if (!target->GetPluginFS()->CopyOrMoveFromDiskToFS(type == atCopy, 1,
                                                                           target->GetPluginFS()->GetPluginFSName(),
                                                                           HWindow, NULL, NULL, NULL,
                                                                           selFiles, selDirs, path, NULL))
                        {
                            path[0] = 0; // error while retrieving the target path
                        }
                        else
                        {
                            // convert the path to external format (before showing it in the dialog)
                            PluginFSConvertPathToExternal(path);
                        }
                    }
                    else
                    {
                        path[0] = 0; // no copy/move from disk to FS
                    }
                }
            }
        }
        if (path[0] != 0)
        {
            target->UserWorkedOnThisPath = TRUE; // default action = working with the path in the target panel
        }
        //---
        int recycle = 0;
        BOOL canUseRecycleBin = TRUE;
        if (type == atDelete)
        {
            if (MyGetDriveType(GetPath()) != DRIVE_FIXED)
            {
                recycle = 0;              // none
                canUseRecycleBin = FALSE; // it will not work because Windows does not support it
            }
            else
            {
                if (invertRecycleBin)
                {
                    if (Configuration.UseRecycleBin == 0)
                        recycle = 1; // all
                    else
                        recycle = 0; // none
                }
                else
                    recycle = Configuration.UseRecycleBin;
            }
        }

        CFileData* f = NULL;
        char formatedFileName[MAX_PATH + 200]; // +200 is a reserve (Windows can create paths longer than MAX_PATH)
        char expanded[200];
        BOOL deleteLink = FALSE;
        if (count <= 1) // one selected item or none
        {
            if (countSizeMode != 2) // not calculating all subdirectory sizes (count is the number of selected items)
            {
                int i;
                if (count == 0)
                    i = GetCaretIndex();
                else
                    GetSelItems(1, &i);

                if (i < 0 || i >= Dirs->Count + Files->Count || // invalid index (no files)
                    i == 0 && subDir)                           // we do not work with ".."
                {
                    if (indexes != NULL)
                        delete[] (indexes);
                    FilesActionInProgress = FALSE;
                    EndStopRefresh();
                    EndSuspendMode(); // thesnooper starts again now
                    return;
                }
                BOOL isDir = i < Dirs->Count;
                f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                expanded[0] = 0;
                if (type == atDelete && recycle != 1 && (f->Attr & FILE_ATTRIBUTE_REPARSE_POINT))
                { // it's a link (junction, symlink or volume mount point)
                    lstrcpyn(formatedFileName, GetPath(), MAX_PATH + 200);
                    ResolveSubsts(formatedFileName);
                    int repPointType;
                    if (SalPathAppend(formatedFileName, f->Name, MAX_PATH + 200) &&
                        GetReparsePointDestination(formatedFileName, NULL, 0, &repPointType, TRUE))
                    {
                        lstrcpy(expanded, LoadStr(repPointType == 1 /* MOUNT POINT */ ? IDS_QUESTION_VOLMOUNTPOINT : repPointType == 2 /* JUNCTION POINT */ ? IDS_QUESTION_JUNCTION
                                                                                                                                                            : IDS_QUESTION_SYMLINK));
                        deleteLink = TRUE;
                    }
                }
                AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);
                if (expanded[0] == 0)
                    lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
            else
                expanded[0] = 0; // not used
        }
        else // count-files in directories and individualfiles
        {
            ExpandPluralFilesDirs(expanded, 200, files, count - files, epfdmNormal, FALSE);
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
        {
            switch (recycle)
            {
            case 0:
                resID = IDS_CONFIRM_DELETE;
                break;
            case 1:
                break; // this message will not be shown because DeleteThroughRecycleBin performs the confirmation
            case 2:
                resID = deleteLink ? IDS_CONFIRM_DELETE : IDS_CONFIRM_DELETE2;
                break;
            }
            break;
        }
        }
        CTruncatedString str;
        char subject[MAX_PATH + 100 + 200]; // + 200 is a reserve (Windows can create paths longer than MAX_PATH)
        if (resID != 0)
        {
            sprintf(subject, LoadStr(resID), expanded);
            str.Set(subject, count > 1 ? NULL : formatedFileName);
        }

        //---
        int res;
        DWORD clusterSize = 0;
        CChangeCaseData changeCaseData;
        CCriteriaData criteria;
        if (CopyMoveOptions.Get() != NULL) // if they exist, pull the defaults
            criteria = *CopyMoveOptions.Get();
        CCriteriaData* criteriaPtr = NULL; // pointer to 'criteria'; if NULL, they are ignored
        BOOL copyToExistingDir = FALSE;
        char nextFocus[MAX_PATH + 200]; // + 200 is a reserve (Windows can create paths longer than MAX_PATH)
        nextFocus[0] = 0;
        char* mask = NULL;
        switch (type)
        {
        case atCopy:
        case atMove:
        {
            BOOL havePermissions = FALSE;
            BOOL supportsADS = IsPathOnVolumeSupADS(GetPath(), NULL);
            DWORD dummy1, flags;
            char dummy2[MAX_PATH];
            if (MyGetVolumeInformation(GetPath(), NULL, NULL, NULL, NULL, 0, NULL, &dummy1, &flags, dummy2, MAX_PATH))
                havePermissions = (flags & FS_PERSISTENT_ACLS) != 0;
            while (1)
            {
                if (strlen(path) >= 2 * MAX_PATH)
                    path[0] = 0; // the path is too long; not an ideal solution but I'm not up for a better one now :(
                res = (int)CCopyMoveMoreDialog(HWindow, path, 2 * MAX_PATH,
                                               (type == atCopy) ? LoadStr(IDS_COPY) : LoadStr(IDS_MOVE), &str,
                                               (type == atCopy) ? IDD_COPYDIALOG : IDD_MOVEDIALOG,
                                               Configuration.CopyHistory, COPY_HISTORY_SIZE,
                                               &criteria, havePermissions, supportsADS)
                          .Execute();
                if (!havePermissions)
                    criteria.CopySecurity = FALSE;
                if (res != IDOK)
                    break;
                criteriaPtr = criteria.IsDirty() ? &criteria : NULL;
                UpdateWindow(MainWindow->HWindow);

                if (!IsPluginFSPath(path) &&
                    (path[0] != 0 && path[1] == ':' ||                                             // paths like X:...
                     (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC paths
                     Is(ptDisk) || Is(ptZIPArchive)))                                              // disk/archive relative paths
                   // it's a disk path (absolute or relative) - convert all '/' to '\' and remove duplicate '\'
                    SlashesToBackslashesAndRemoveDups(path);
                }

                int len = (int)strlen(path);
                BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // path ends with a backslash -> must be a directory
                BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                                   path[1] == ':'); // a path like "c:" must remain a path (not a file) even after expansion

                int pathType;
                BOOL pathIsDir;
                char* secondPart;
                char textBuf[2 * MAX_PATH + 200];
                if (ParsePath(path, pathType, pathIsDir, secondPart,
                              type == atCopy ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                              count <= 1 ? nextFocus : NULL, NULL, 2 * MAX_PATH))
                {
                    // use 'if' instead of a 'switch' to ensure that 'break' and 'continue' work correctly
                    if (pathType == PATH_TYPE_WINDOWS) // Windows path (drive + UNC)
                    {
                        if (strlen(path) >= MAX_PATH)
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                          (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                          MB_OK | MB_ICONEXCLAMATION);
                            continue;
                        }

                        CFileData* dir = (count == 0) ? f : ((indexes[0] < Dirs->Count) ? &Dirs->At(indexes[0]) : &Files->At(indexes[0] - Dirs->Count));

                        if (SalSplitWindowsPath(HWindow, LoadStr(type == atCopy ? IDS_COPY : IDS_MOVE),
                                                LoadStr(type == atCopy ? IDS_ERRORCOPY : IDS_ERRORMOVE),
                                                count, path, secondPart, pathIsDir, backslashAtEnd || mustBePath,
                                                dir->Name, GetPath(), mask))
                        {
                            if (nextFocus[0] != 0 && secondPart[0] == 0)
                                copyToExistingDir = TRUE;
                            break; // exit the Copy/Move loop and perform the operation
                        }
                        else
                        {
                            continue; // back to the Copy/Move dialog
                        }
                    }
                    else
                    {
                        if (pathType == PATH_TYPE_ARCHIVE) // path into an archive
                        {
                            if (strlen(secondPart) >= MAX_PATH) // isn't the path inside the archive too long?
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                continue;
                            }

                            if (criteriaPtr != NULL && Configuration.CnfrmCopyMoveOptionsNS) // archives do not support options from the Copy/Move dialog
                            {
                                MSGBOXEX_PARAMS params;
                                memset(&params, 0, sizeof(params));
                                params.HParent = HWindow;
                                params.Flags = MB_OK | MB_ICONINFORMATION | MSGBOXEX_HINT;
                                params.Caption = LoadStr(IDS_INFOTITLE);
                                params.Text = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED);
                                params.CheckBoxText = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED_AGAIN);
                                int dontShow = !Configuration.CnfrmCopyMoveOptionsNS;
                                params.CheckBoxValue = &dontShow;
                                SalMessageBoxEx(&params);
                                Configuration.CnfrmCopyMoveOptionsNS = !dontShow;
                            }

                            CPanelTmpEnumData data;
                            int oneIndex = -1;
                            if (count > 0) // some files are selected
                            {
                                data.IndexesCount = count;
                                data.Indexes = indexes; // deallocated via 'indexes'
                            }
                            else // take the focused item
                            {
                                oneIndex = GetCaretIndex();
                                data.IndexesCount = 1;
                                data.Indexes = &oneIndex; // not deallocated
                            }
                            data.CurrentIndex = 0;
                            data.ZIPPath = GetZIPPath();
                            data.Dirs = Dirs;
                            data.Files = Files;
                            data.ArchiveDir = GetArchiveDir();
                            lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
                            data.EnumLastDir = NULL;
                            data.EnumLastIndex = -1;

                            //---  check if it's a zero-size file
                            BOOL nullFile;
                            BOOL hasPath = *secondPart != 0;
                            if (hasPath && !backslashAtEnd && !mustBePath) // check whether they used an operational mask -> we are not able to handle that
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_MOVECOPY_OPMASKSNOTSUP),
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                continue; // back to the copy/move dialog
                            }

                            *secondPart = 0; // 'path' holds the archive file name
                            BOOL haveSize = FALSE;
                            CQuadWord size;
                            DWORD err;
                            HANDLE hFile = HANDLES_Q(CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL));
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

                                //---  if it's a zero-size file, we must delete it; archivers can not handle them
                                DWORD nullFileAttrs;
                                if (nullFile)
                                {
                                    nullFileAttrs = SalGetFileAttributes(path);
                                    ClearReadOnlyAttr(path, nullFileAttrs); // so it’s possible to delete even read-only files
                                    DeleteFile(path);
                                }
                                //---  custom packing
                                SetCurrentDirectory(GetPath());
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                                if (PackCompress(HWindow, this, path, hasPath ? secondPart + 1 : "",
                                                 type == atMove, GetPath(), PanelEnumDiskSelection, &data))
                                {                   // packing succeeded
                                    if (nullFile && // a zero-size file might have a different compressed attribute; set the archive to the same
                                        nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                    {
                                        HANDLE hFile2 = HANDLES_Q(CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                                             0, NULL, OPEN_EXISTING,
                                                                             0, NULL));
                                        if (hFile2 != INVALID_HANDLE_VALUE)
                                        {
                                            // restore the 'compressed' flag; it simply doesn't work on FAT or FAT32
                                            USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                            ULONG length;
                                            DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                            sizeof(USHORT), NULL, 0, &length, FALSE);
                                            HANDLES(CloseHandle(hFile2));
                                            SetFileAttributes(path, nullFileAttrs);
                                        }
                                    }
                                    SetSel(FALSE, -1, TRUE);                        // explicit redraw
                                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                }
                                else
                                {
                                    if (nullFile) // it failed, we have to create it again
                                    {
                                        HANDLE hFile2 = HANDLES_Q(CreateFile(path, GENERIC_READ | GENERIC_WRITE,
                                                                             0, NULL, OPEN_ALWAYS,
                                                                             0, NULL));
                                        if (hFile2 != INVALID_HANDLE_VALUE)
                                        {
                                            if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                            {
                                                // restore the "compressed" flag; on FAT and FAT32 it simply doesn't work
                                                USHORT state = (nullFileAttrs & FILE_ATTRIBUTE_COMPRESSED) ? COMPRESSION_FORMAT_DEFAULT : COMPRESSION_FORMAT_NONE;
                                                ULONG length;
                                                DeviceIoControl(hFile2, FSCTL_SET_COMPRESSION, &state,
                                                                sizeof(USHORT), NULL, 0, &length, FALSE);
                                            }
                                            HANDLES(CloseHandle(hFile2));
                                            if (nullFileAttrs != INVALID_FILE_ATTRIBUTES)
                                                SetFileAttributes(path, nullFileAttrs);
                                        }
                                    }
                                }
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                                SetCurrentDirectoryToSystem();

                                UpdateWindow(MainWindow->HWindow);

                                //---  refresh of non-auto-refreshed directories
                                // change in the directory with the target archive (the archive file changes)
                                CutDirectory(path); // 'path' is the archive name -> must always succeed
                                MainWindow->PostChangeOnPathNotification(path, FALSE);
                                if (type == atMove)
                                {
                                    // changes on the source path (file/directory deletion should take place when moving a file into the archive
                                    MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                                }
                            }
                            else
                            {
                                sprintf(textBuf, LoadStr(IDS_FILEERRORFORMAT), path, GetErrorText(err));
                                SalMessageBox(HWindow, textBuf,
                                              (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                              MB_OK | MB_ICONEXCLAMATION);
                                if (hasPath)
                                    *secondPart = '\\'; // restore the path - we will edit it in the Copy/Move dialog
                                if (backslashAtEnd || mustBePath)
                                    SalPathAddBackslash(path, 2 * MAX_PATH + 200);
                                continue; // back to the copy/move dialog
                            }

                            if (indexes != NULL)
                                delete[] (indexes);
                            //---  if a Salamander window is active, suspend mode ends
                            EndStopRefresh();
                            EndSuspendMode();
                            FilesActionInProgress = FALSE;
                            return;
                        }
                        else
                        {
                            if (pathType == PATH_TYPE_FS) // file-system path
                            {
                                if (strlen(secondPart) >= MAX_PATH) // is the user's part of the FS path too long?
                                {
                                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGPATH),
                                                  (type == atCopy) ? LoadStr(IDS_ERRORCOPY) : LoadStr(IDS_ERRORMOVE),
                                                  MB_OK | MB_ICONEXCLAMATION);
                                    continue;
                                }

                                if (criteriaPtr != NULL && Configuration.CnfrmCopyMoveOptionsNS) // file systems do not support options from the Copy/Move dialog
                                {
                                    MSGBOXEX_PARAMS params;
                                    memset(&params, 0, sizeof(params));
                                    params.HParent = HWindow;
                                    params.Flags = MB_OK | MB_ICONINFORMATION | MSGBOXEX_HINT;
                                    params.Caption = LoadStr(IDS_INFOTITLE);
                                    params.Text = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED);
                                    params.CheckBoxText = LoadStr(IDS_MOVECOPY_OPTIONS_NOTSUPPORTED_AGAIN);
                                    int dontShow = !Configuration.CnfrmCopyMoveOptionsNS;
                                    params.CheckBoxValue = &dontShow;
                                    SalMessageBoxEx(&params);
                                    Configuration.CnfrmCopyMoveOptionsNS = !dontShow;
                                }

                                // prepare data to enumerate files and directories from the panel
                                CPanelTmpEnumData data;
                                int oneIndex = -1;
                                int selFiles = 0;
                                int selDirs = 0;
                                if (count > 0) // some files are selected
                                {
                                    selFiles = files;
                                    selDirs = count - files;
                                    data.IndexesCount = count;
                                    data.Indexes = indexes; // deallocated via 'indexes'
                                }
                                else // take the focused item
                                {
                                    oneIndex = GetCaretIndex();
                                    if (oneIndex >= Dirs->Count)
                                        selFiles = 1;
                                    else
                                        selDirs = 1;
                                    data.IndexesCount = 1;
                                    data.Indexes = &oneIndex; // not deallocated
                                }
                                data.CurrentIndex = 0;
                                data.ZIPPath = GetZIPPath();
                                data.Dirs = Dirs;
                                data.Files = Files;
                                data.ArchiveDir = GetArchiveDir();
                                lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
                                data.EnumLastDir = NULL;
                                data.EnumLastIndex = -1;

                                // obtain the file-system name
                                char fsName[MAX_PATH];
                                memcpy(fsName, path, (secondPart - path) - 1);
                                fsName[(secondPart - path) - 1] = 0;

                                // lower the thread priority to "normal" so the operation does not overload the machine
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                                // choose the FS that will perform the operation (order: active, detached, new)
                                BOOL invalidPath = FALSE;
                                BOOL unselect = FALSE;
                                char targetPath[2 * MAX_PATH];
                                CDetachedFSList* list = MainWindow->DetachedFSList;
                                int i;
                                for (i = -1; i < list->Count; i++)
                                {
                                    CPluginFSInterfaceEncapsulation* fs = NULL;
                                    if (i == -1) // first try the active FS in the target panel
                                    {
                                        if (target->Is(ptPluginFS))
                                            fs = target->GetPluginFS();
                                    }
                                    else
                                        fs = list->At(i); // then detached FS

                                    int fsNameIndex;
                                    if (fs != NULL && fs->NotEmpty() &&                          // interface is valid
                                        fs->IsFSNameFromSamePluginAsThisFS(fsName, fsNameIndex)) // the FS name comes from the same plugin (otherwise there's no point in trying)
                                    {
                                        BOOL invalidPathOrCancel;
                                        lstrcpyn(targetPath, path, 2 * MAX_PATH); // risk of exceeding 2 * MAX_PATH
                                        // convert the path to internal format
                                        fs->GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex,
                                                                                             targetPath + strlen(fsName) + 1);
                                        if (fs->CopyOrMoveFromDiskToFS(type == atCopy, 2, fs->GetPluginFSName(),
                                                                       HWindow, GetPath(), PanelEnumDiskSelection, &data,
                                                                       selFiles, selDirs, targetPath, &invalidPathOrCancel))
                                        {
                                            unselect = !invalidPathOrCancel;
                                            break; // done
                                        }
                                        else // error
                                        {
                                            // before the next use we must reset it (so it enumerates from the beginning again)
                                            data.Reset();

                                            if (invalidPathOrCancel)
                                            {
                                                // convert the path to external format (before displaying it in the dialog)
                                                PluginFSConvertPathToExternal(targetPath);
                                                strcpy(path, targetPath);
                                                invalidPath = TRUE;
                                                break; // we must go back to the copy/move dialog
                                            }
                                            // trying another FS
                                        }
                                    }
                                }
                                if (i == list->Count) // active and detached FS couldn't handle it, we will create a new FS
                                {
                                    int index;
                                    int fsNameIndex;
                                    if (Plugins.IsPluginFS(fsName, index, fsNameIndex)) // determine the plugin index
                                    {
                                        // obtain the plugin with the FS
                                        CPluginData* plugin = Plugins.Get(index);
                                        if (plugin != NULL)
                                        {
                                            // open a new FS
                                            // load the plugin before obtaining DLLName, Version and plugin interfaces
                                            CPluginFSInterfaceAbstract* auxFS = plugin->OpenFS(fsName, fsNameIndex);
                                            CPluginFSInterfaceEncapsulation pluginFS(auxFS, plugin->DLLName, plugin->Version,
                                                                                     plugin->GetPluginInterfaceForFS()->GetInterface(),
                                                                                     plugin->GetPluginInterface()->GetInterface(),
                                                                                     fsName, fsNameIndex, -1, 0, plugin->BuiltForVersion);
                                            if (pluginFS.NotEmpty())
                                            {
                                                Plugins.SetWorkingPluginFS(&pluginFS);
                                                BOOL invalidPathOrCancel;
                                                lstrcpyn(targetPath, path, 2 * MAX_PATH); // risk of exceeding 2 * MAX_PATH
                                                // convert the path to internal format
                                                pluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex,
                                                                                                          targetPath + strlen(fsName) + 1);
                                                if (pluginFS.CopyOrMoveFromDiskToFS(type == atCopy, 2,
                                                                                    pluginFS.GetPluginFSName(),
                                                                                    HWindow, GetPath(),
                                                                                    PanelEnumDiskSelection, &data,
                                                                                    selFiles, selDirs, targetPath,
                                                                                    &invalidPathOrCancel))
                                                { // done/cancel
                                                    unselect = !invalidPathOrCancel;
                                                }
                                                else // syntax error/plugin error
                                                {
                                                    if (invalidPathOrCancel)
                                                    {
                                                        // convert the path to external format (before displaying it in the dialog)
                                                        PluginFSConvertPathToExternal(targetPath);
                                                        strcpy(path, targetPath);
                                                        invalidPath = TRUE; // we must go back to the Copy/Move dialog
                                                    }
                                                    else // plugin error (new FS, but returns "requested operation cannot be performed on this FS" error)
                                                    {
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
                                            TRACE_E("Unexpected situation in CFilesWindow::FilesAction() - unable to work with plugin.");
                                    }
                                    else
                                    {
                                        TRACE_E("Unexpected situation in CFilesWindow::FilesAction() - file-system " << fsName << " was not found.");
                                    }
                                }

                                // raise the thread's priority again, the operation has finished
                                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                                SetCurrentDirectoryToSystem(); // in any case restore the current directory as well

                                if (invalidPath)
                                    continue; // back to the copy/move dialog

                                if (unselect) // unselect files/directories in the panel
                                {
                                    SetSel(FALSE, -1, TRUE);                        // explicit redraw
                                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                }

                                if (indexes != NULL)
                                    delete[] (indexes);
                                //---  if any Salamander window is active, suspend mode ends
                                EndStopRefresh();
                                EndSuspendMode();
                                FilesActionInProgress = FALSE;
                                return;
                            }
                        }
                    }
                }
            }
            break;
        }

        case atDelete:
        {
            if (Configuration.CnfrmFileDirDel && recycle != 1)
            {                                                                                                           // ask only if requested and if we don't use the SHFileOperation API for deletion
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
            break;
        }

        case atCountSize:
            res = IDOK;
            break;

        case atChangeCase:
        {
            CChangeCaseDlg dlg(HWindow, SelectionContainsDirectory());
            res = (int)dlg.Execute();
            UpdateWindow(MainWindow->HWindow);
            changeCaseData.FileNameFormat = dlg.FileNameFormat;
            changeCaseData.Change = dlg.Change;
            changeCaseData.SubDirs = dlg.SubDirs;
            break;
        }
        }
        if (res == IDOK && // begin disk operation
            CheckPath(TRUE) == ERROR_SUCCESS)
        {
            if (type == atDelete && recycle == 1)
            {
                if (DeleteThroughRecycleBin(indexes, count, f))
                {
                    SetSel(FALSE, -1, TRUE);                        // explicit redraw
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }

                //---  refresh of directories that are not auto-refreshed
                // change in the directory displayed in the panel and its subdirectories
                MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
            }
            else
            {
                COperations* script;
                if (type == atCopy || type == atMove)
                {
                    if (count <= 1)
                    {
                        char format[300];
                        sprintf(format, LoadStr(type == atCopy ? IDS_COPYDLGTITLE : IDS_MOVEDLGTITLE), expanded);
                        sprintf(subject, format, formatedFileName);
                    }
                    else
                        sprintf(subject, LoadStr(type == atCopy ? IDS_COPYDLGTITLE : IDS_MOVEDLGTITLE), expanded);
                    script = new COperations(1000, 500, DupStr(subject), DupStr(GetPath()), DupStr(path));
                }
                else
                    script = new COperations(1000, 500, NULL, NULL, NULL);
                if (script == NULL)
                    TRACE_E(LOW_MEMORY);
                else
                {
                    const char* caption;
                    switch (type)
                    {
                    case atCopy:
                    {
                        caption = LoadStr(IDS_COPY);
                        script->ShowStatus = TRUE;
                        script->IsCopyOperation = TRUE;
                        script->IsCopyOrMoveOperation = TRUE;
                        break;
                    }

                    case atMove:
                    {
                        BOOL sameRootPath = HasTheSameRootPath(GetPath(), path);
                        script->SameRootButDiffVolume = sameRootPath && !HasTheSameRootPathAndVolume(GetPath(), path);
                        script->ShowStatus = !sameRootPath || script->SameRootButDiffVolume;
                        script->IsCopyOperation = FALSE;
                        script->IsCopyOrMoveOperation = TRUE;
                        caption = LoadStr(IDS_MOVE);
                        break;
                    }

                    case atDelete:
                        caption = LoadStr(IDS_DELETE);
                        break;
                    case atChangeCase:
                        caption = LoadStr(IDS_CHANGECASE);
                        break;
                    default:
                        caption = "";
                    }
                    if (criteriaPtr != NULL && criteriaPtr->UseSpeedLimit)
                        script->SetSpeedLimit(TRUE, criteriaPtr->SpeedLimit);
                    char captionBuf[50];
                    lstrcpyn(captionBuf, caption, 50); // otherwise the LoadStr buffer gets overwritten before being copied to the dialog's local buffer
                    caption = captionBuf;
                    HWND hFocusedWnd = GetFocus();
                    CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
                    MainWindow->StartAnimate(); // example of  its usage
                    EnableWindow(MainWindow->HWindow, FALSE);

                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    script->InvertRecycleBin = invertRecycleBin;
                    script->CanUseRecycleBin = canUseRecycleBin;

                    char* auxTargetPath = NULL;
                    if (type == atCopy || type == atMove)
                        auxTargetPath = path;
                    BOOL res2 = BuildScriptMain(script, type, auxTargetPath, mask, count, indexes,
                                                f, NULL, &changeCaseData, countSizeMode != 0,
                                                criteriaPtr);
                    // if there's nothing to do, don't show the progress dialog
                    BOOL emptyScript = script->Count == 0 && type != atCountSize;

                    // swapped to allow activation of the main window (must not be disabled), otherwise it switches to another app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();
                    if (type == atCountSize) // additional directory sizes have been calculated
                    {
                        // perform sorting if counting was done across multiple selected directories or all directories
                        if (((countSizeMode == 0 && dirs > 1) || countSizeMode == 2) && SortType == stSize)
                        {
                            ChangeSortType(stSize, FALSE, TRUE);
                        }

                        //            DirectorySizesHolder.Store(this); // store directory names and sizes

                        RefreshListBox(-1, -1, FocusedIndex, FALSE, FALSE); // recalculate column widths
                    }

                    // if Salamander is active, call SetFocus on the remembered window (SetFocus does not work
                    // when the main window is disabled - after deactivating/activating the disabled main window the active panel
                    // does not have focus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    BOOL cancel = FALSE;
                    if (!emptyScript && res2 && (type == atCopy || type == atMove))
                    {
                        BOOL occupiedSpTooBig = script->OccupiedSpace != CQuadWord(0, 0) &&
                                                script->BytesPerCluster != 0 && // we have disk information
                                                script->OccupiedSpace > script->FreeSpace &&
                                                !IsSambaDrivePath(path); // Samba returns incorrect cluster size, so we can only rely on TotalFileSize

                        if (occupiedSpTooBig ||
                            script->BytesPerCluster != 0 && // we have disk information
                                script->TotalFileSize > script->FreeSpace)
                        {
                            char buf1[50];
                            char buf2[50];
                            char buf3[200];
                            sprintf(buf3, LoadStr(IDS_NOTENOUGHSPACE),
                                    NumberToStr(buf1, occupiedSpTooBig ? script->OccupiedSpace : script->TotalFileSize),
                                    NumberToStr(buf2, script->FreeSpace));
                            cancel = SalMessageBox(HWindow, buf3,
                                                   caption, MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED) != IDYES;
                        }
                    }

                    if (!cancel)
                    {
                        // prepare refresh of directories that are not auto-refreshed
                        if (!emptyScript && type != atCountSize)
                        {
                            if (type == atDelete || type == atChangeCase || type == atMove)
                            {
                                // change in the directory displayed in the panel and its subdirectories
                                script->SetWorkPath1(GetPath(), TRUE);
                            }
                            if (type == atCopy)
                            {
                                // change in the target directory and its subdirectories
                                script->SetWorkPath1(path, TRUE);
                            }
                            if (type == atMove)
                            {
                                // change in the target directory and its subdirectories
                                script->SetWorkPath2(path, TRUE);
                            }
                        }

                        if (!emptyScript &&
                            (!res2 || type == atCountSize ||
                             !StartProgressDialog(script, caption, NULL, NULL)))
                        {
                            if (res2 && type == atCountSize && countSizeMode == 0)
                            {
                                CSizeResultsDlg result(MainWindow->HWindow, script->TotalSize,
                                                       script->CompressedSize, script->OccupiedSpace,
                                                       script->FilesCount, script->DirsCount,
                                                       &script->Sizes);
                                result.Execute();
                            }
                            else
                                UpdateWindow(MainWindow->HWindow);
                            if (!script->IsGood())
                                script->ResetState();
                            FreeScript(script);
                        }
                        else // removing selected index
                        {
                            if (res2)
                            {
                                SetSel(FALSE, -1, TRUE);                        // explicit redraw
                                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                            }
                            if (!emptyScript && nextFocus[0] != 0)
                            {
                                strcpy(NextFocusName, nextFocus);
                                DontClearNextFocusName = TRUE;
                                if (type == atCopy && copyToExistingDir)
                                { // when copying to a directory, it is necessary (RefreshDirectory might not happen)
                                    PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0);
                                }
                            }
                            if (emptyScript) // if the script is empty we must deallocate it
                            {
                                if (!script->IsGood())
                                    script->ResetState();
                                FreeScript(script);
                            }
                            UpdateWindow(MainWindow->HWindow);
                        }
                    }
                    else
                    {
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    MainWindow->StopAnimate(); // example of its usage
                }
            }
        }
        //---
        if (indexes != NULL)
            delete[] (indexes);
        //---  if any Salamander window is active, suspend mode ends
        EndStopRefresh();
        EndSuspendMode();
        FilesActionInProgress = FALSE;
    }
}

// extracts all subdirectories from 'path' directory and calls itself recursively
// adds files mapi
// returns TRUE, if everything succeeded; otherwise returns FALSE
BOOL EmailFilesAddDirectory(CSimpleMAPI* mapi, const char* path, BOOL* errGetFileSizeOfLnkTgtIgnAll)
{
    WIN32_FIND_DATA file;
    char myPath[MAX_PATH + 4];
    int l = (int)strlen(path);
    memmove(myPath, path, l);
    if (myPath[l - 1] != '\\')
        myPath[l++] = '\\';
    char* name = myPath + l;
    strcpy(name, "*");
    HANDLE find = HANDLES_Q(FindFirstFile(myPath, &file));
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            char text[2 * MAX_PATH + 100];
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            if (SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                SetCursor(LoadCursor(NULL, IDC_WAIT));
                return FALSE; // user wants to quit
            }
            SetCursor(LoadCursor(NULL, IDC_WAIT));
        }
        return TRUE; // user wants to continue
    }
    BOOL ok = TRUE;
    do
    {
        if (file.cFileName[0] != 0 &&
            (file.cFileName[0] != '.' ||
             (file.cFileName[1] != 0 && (file.cFileName[1] != '.' || file.cFileName[2] != 0))))
        {
            strcpy(name, file.cFileName);
            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!EmailFilesAddDirectory(mapi, myPath, errGetFileSizeOfLnkTgtIgnAll))
                {
                    ok = FALSE;
                    break;
                }
            }
            else
            {
                // links: size == 0, the file size must be obtained additionally via GetLinkTgtFileSize()
                BOOL cancel = FALSE;
                CQuadWord size(file.nFileSizeLow, file.nFileSizeHigh);
                if ((file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                    !GetLinkTgtFileSize(MainWindow->HWindow, myPath, NULL, &size, &cancel, errGetFileSizeOfLnkTgtIgnAll))
                {
                    size.Set(file.nFileSizeLow, file.nFileSizeHigh);
                }
                if (cancel || !mapi->AddFile(myPath, &size))
                {
                    ok = FALSE;
                    break;
                }
            }
        }
    } while (FindNextFile(find, &file));
    HANDLES(FindClose(find));
    return ok;
}

// pulls the selection from the panel and processes every item:
// if it's a directory, calls EmailFilesAddDirectory for it
// if it's a file, adds it to mapi
// if everything succeeds, creates the email
void CFilesWindow::EmailFiles()
{
    CALL_STACK_MESSAGE1("CFilesWindow::EmailFiles()");
    if (Dirs->Count + Files->Count == 0)
        return;
    if (!Is(ptDisk))
        return;

    BeginStopRefresh(); // the snooper takes a break

    if (!FilesActionInProgress)
    {
        FilesActionInProgress = TRUE;

        CSimpleMAPI* mapi = new CSimpleMAPI;
        if (mapi != NULL)
        {
            if (mapi->Init(HWindow))
            {
                BOOL send = TRUE;
                int selCount = GetSelCount();
                int alloc = (selCount == 0 ? 1 : selCount);

                int* indexes;
                indexes = new int[alloc];
                if (indexes == NULL)
                {
                    TRACE_E(LOW_MEMORY);
                }
                else
                {
                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    if (selCount > 0)
                        GetSelItems(selCount, indexes);
                    else
                        indexes[0] = GetCaretIndex();

                    CFileData* f;
                    char path[MAX_PATH];
                    int l = (int)strlen(GetPath());
                    memmove(path, GetPath(), l);
                    if (path[l - 1] != '\\')
                        path[l++] = '\\';
                    char* name = path + l;
                    BOOL errGetFileSizeOfLnkTgtIgnAll = FALSE;
                    for (int i = 0; i < alloc; i++)
                    {
                        if (indexes[i] >= 0 && indexes[i] < Dirs->Count + Files->Count)
                        {
                            BOOL isDir = indexes[i] < Dirs->Count;
                            f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);
                            strcpy(name, f->Name);
                            if (isDir)
                            {
                                if (!EmailFilesAddDirectory(mapi, path, &errGetFileSizeOfLnkTgtIgnAll))
                                {
                                    send = FALSE;
                                    break;
                                }
                            }
                            else
                            {
                                // links: f->Size == 0, file size must be obtained additionally via GetLinkTgtFileSize()
                                BOOL cancel = FALSE;
                                CQuadWord size = f->Size;
                                if ((f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0 &&
                                    !GetLinkTgtFileSize(HWindow, path, NULL, &size, &cancel, &errGetFileSizeOfLnkTgtIgnAll))
                                {
                                    size = f->Size;
                                }
                                if (cancel || !mapi->AddFile(path, &size))
                                {
                                    send = FALSE;
                                    break;
                                }
                            }
                        }
                    }
                    delete[] (indexes);
                    SetCursor(oldCur);
                }
                if (send && mapi->GetFilesCount() == 0)
                {
                    // no file to send; display information and exit
                    SalMessageBox(HWindow, LoadStr(IDS_WANTEMAIL_NOFILE), LoadStr(IDS_INFOTITLE),
                                  MB_OK | MB_ICONINFORMATION);
                    send = FALSE;
                }
                if (send)
                {
                    int ret = IDYES;
                    BOOL dontShow = !Configuration.CnfrmSendEmail;
                    if (!dontShow)
                    {
                        char expanded[200];
                        char totalSize[100];
                        char text[300];
                        ExpandPluralFilesDirs(expanded, 200, mapi->GetFilesCount(), 0, epfdmNormal, FALSE);
                        CQuadWord size;
                        mapi->GetTotalSize(&size);
                        PrintDiskSize(totalSize, size, 0);
                        sprintf(text, LoadStr(IDS_CONFIRM_EMAIL), expanded, totalSize);

                        MSGBOXEX_PARAMS params;
                        memset(&params, 0, sizeof(params));
                        params.HParent = HWindow;
                        params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                        params.Caption = LoadStr(IDS_QUESTION);
                        params.Text = text;
                        params.CheckBoxText = LoadStr(IDS_DONTSHOWAGAINSE);
                        params.CheckBoxValue = &dontShow;
                        ret = SalMessageBoxEx(&params);
                        Configuration.CnfrmSendEmail = !dontShow;
                    }
                    if (ret == IDYES)
                    {
                        SimpleMAPISendMail(mapi); // own thread; handles destruction of the 'mapi' object
                    }
                    else
                        delete mapi;
                }
                else
                    delete mapi;
            }
            else
                delete mapi;
        }
        else
            TRACE_E(LOW_MEMORY);
        FilesActionInProgress = FALSE;
    }
    EndStopRefresh(); // the snooper starts again now
}

BOOL CFilesWindow::OpenFocusedInOtherPanel(BOOL activate)
{
    CFilesWindow* otherPanel = (this == MainWindow->LeftPanel) ? MainWindow->RightPanel : MainWindow->LeftPanel;
    if (otherPanel == NULL)
        return FALSE;

    // allow opening the up-dir
    //  if (FocusedIndex == 0 && FocusedIndex < Dirs->Count &&
    //      strcmp(Dirs->At(0).Name, "..") == 0) return FALSE;   // do not handle the up-dir
    if (FocusedIndex < 0 || FocusedIndex >= Files->Count + Dirs->Count)
        return FALSE; // ignore invalid index

    char buff[2 * MAX_PATH];
    buff[0] = 0;

    CFileData* file = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);

    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        GetGeneralPath(buff, 2 * MAX_PATH);
        BOOL nethoodPath = FALSE;
        if (FocusedIndex == 0 && 0 < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0 &&
            IsUNCRootPath(buff)) // up-dir on a UNC root path => switch to Nethood
        {
            char* s = buff + 2;
            while (*s != 0 && *s != '\\')
                s++;
            CPluginData* nethoodPlugin = NULL;
            char doublePath[2 * MAX_PATH];
            if (*s == '\\' && Plugins.GetFirstNethoodPluginFSName(doublePath, &nethoodPlugin))
            {
                nethoodPath = TRUE;
                *s = 0;
                int len = (int)strlen(doublePath);
                memmove(buff + len + 1, buff, strlen(buff) + 1);
                memcpy(buff, doublePath, len);
                buff[len] = ':';
            }
        }
        if (!nethoodPath)
        {
            SalPathAddBackslash(buff, 2 * MAX_PATH);
            int l = (int)strlen(buff);
            lstrcpyn(buff + l, file->Name, 2 * MAX_PATH - l);
        }
    }
    else if (Is(ptPluginFS) && GetPluginFS()->NotEmpty())
    {
        strcpy(buff, GetPluginFS()->GetPluginFSName());
        strcat(buff, ":");
        int l = (int)strlen(buff);
        int isDir;
        if (FocusedIndex == 0 && 0 < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0)
            isDir = 2;
        else
            isDir = (FocusedIndex < Dirs->Count ? 1 : 0);
        if (!GetPluginFS()->GetFullName(*file, isDir, buff + l, 2 * MAX_PATH - l))
            buff[0] = 0;
    }

    if (buff[0] != 0)
    {
        int failReason;
        BOOL ret = otherPanel->ChangeDir(buff, -1, NULL, 3 /* change-dir */, &failReason, FALSE);
        if (activate && this == MainWindow->GetActivePanel())
        {
            if (ret || (failReason == CHPPFR_SUCCESS ||
                        failReason == CHPPFR_SHORTERPATH ||
                        failReason == CHPPFR_FILENAMEFOCUSED))
            {
                // if the path changed in the other panel, activate the panel
                MainWindow->ChangePanel();
                return TRUE;
            }
        }
    }
    return FALSE;
}

void CFilesWindow::ChangePathToOtherPanelPath()
{
    CFilesWindow* panel = (this == MainWindow->LeftPanel) ? MainWindow->RightPanel : MainWindow->LeftPanel;
    if (panel == NULL)
        return;

    if (panel->Is(ptDisk))
    {
        ChangePathToDisk(HWindow, panel->GetPath());
    }
    else
    {
        if (panel->Is(ptZIPArchive))
        {
            ChangePathToArchive(panel->GetZIPArchive(), panel->GetZIPPath());
        }
        else
        {
            if (panel->Is(ptPluginFS))
            {
                char path[2 * MAX_PATH];
                lstrcpyn(path, panel->GetPluginFS()->GetPluginFSName(), MAX_PATH - 1);
                strcat(path, ":");
                if (panel->GetPluginFS()->GetCurrentPath(path + strlen(path)))
                    ChangeDir(path, -1, NULL, 3 /*change-dir*/, NULL, FALSE);
            }
        }
    }
}
