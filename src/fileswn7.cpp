// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "zip.h"
#include "pack.h"

//
// ****************************************************************************
// CFilesWindow
//

CPanelTmpEnumData::CPanelTmpEnumData()
{
    Indexes = NULL;
    CurrentIndex = 0;
    IndexesCount = 0;
    ZIPPath = NULL;
    Dirs = NULL;
    Files = NULL;
    ArchiveDir = NULL;
    WorkPath[0] = 0;
    EnumLastDir = NULL;
    EnumLastIndex = 0;
    EnumLastPath[0] = 0;
    EnumTmpFileName[0] = 0;
    DiskDirectoryTree = NULL;
    EnumLastDosPath[0] = 0;
    EnumTmpDosFileName[0] = 0;
    FilesCountReturnedFromWP = 0;
}

CPanelTmpEnumData::~CPanelTmpEnumData()
{
    if (DiskDirectoryTree != NULL)
        delete DiskDirectoryTree;
}

void CPanelTmpEnumData::Reset()
{
    CurrentIndex = 0;
    EnumLastDir = NULL;
    EnumLastIndex = -1;
    EnumLastPath[0] = 0;
    EnumTmpFileName[0] = 0;
    EnumLastDosPath[0] = 0;
    EnumTmpDosFileName[0] = 0;
    FilesCountReturnedFromWP = 0;
}

const char* _PanelSalEnumSelection(int enumFiles, const char** dosName, BOOL* isDir, CQuadWord* size,
                                   const CFileData** fileData, void* param, HWND parent, int* errorOccured)
{
    SLOW_CALL_STACK_MESSAGE2("_PanelSalEnumSelection(%d, , , , , , ,)", enumFiles);
    CPanelTmpEnumData* data = (CPanelTmpEnumData*)param;
    if (dosName != NULL)
        *dosName = NULL;
    if (isDir != NULL)
        *isDir = FALSE;
    if (size != NULL)
        *size = CQuadWord(0, 0);
    if (fileData != NULL)
        *fileData = NULL;

    if (enumFiles == -1)
    {
        data->Reset();
        return NULL;
    }

    static char errText[1000];
    const char* curZIPPath;
    if (data->DiskDirectoryTree == NULL)
        curZIPPath = data->ZIPPath;
    else
        curZIPPath = "";

ENUM_NEXT:

    if (data->CurrentIndex >= data->IndexesCount)
        return NULL;
    if (enumFiles == 0)
    {
        int i = data->Indexes[data->CurrentIndex++];
        BOOL localIsDir = i < data->Dirs->Count;
        if (isDir != NULL)
            *isDir = localIsDir;
        CFileData* f = &(localIsDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
        if (localIsDir)
        {
            if (size != NULL)
                *size = data->ArchiveDir->GetDirSize(data->ZIPPath, f->Name);
        }
        else
        {
            if (size != NULL)
                *size = f->Size;
            data->FilesCountReturnedFromWP++; // Just to be safe, in case someone changes 'enumFiles' between 0 and 3
        }
        if (fileData != NULL)
            *fileData = f;
        return f->Name;
    }
    else
    {
        if (data->EnumLastDir == NULL) // it is necessary to "open" the directory
        {
            int i = data->Indexes[data->CurrentIndex];
            BOOL localIsDir = i < data->Dirs->Count;
            if (isDir != NULL)
                *isDir = localIsDir;
            CFileData* f = &(localIsDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
            if (localIsDir)
            {
                int zipPathLen = (int)strlen(curZIPPath);
                if (zipPathLen + (zipPathLen > 0 ? 1 : 0) + f->NameLen >= MAX_PATH)
                { // too long journey
                    if (errorOccured != NULL)
                        *errorOccured = SALENUM_ERROR;
                    _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG), f->Name, curZIPPath);
                    if (parent != NULL &&
                        SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                      MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                    {
                        if (errorOccured != NULL)
                            *errorOccured = SALENUM_CANCEL;
                        return NULL; // cancel, we are finishing
                    }
                    data->CurrentIndex++; // skip the directory with a long name and continue with the next directory or file
                    goto ENUM_NEXT;
                }
                else
                {
                    memcpy(data->EnumLastPath, curZIPPath, zipPathLen);
                    if (zipPathLen > 0)
                        data->EnumLastPath[zipPathLen++] = '\\';
                    strcpy(data->EnumLastPath + zipPathLen, f->Name);
                    if (data->DiskDirectoryTree != NULL)
                    { // if EnumLastDosPath is used, zipPathLen will be equal to 0
                        strcpy(data->EnumLastDosPath, (f->DosName == NULL) ? f->Name : f->DosName);
                    }
                    if (data->DiskDirectoryTree == NULL)
                        data->EnumLastDir = data->ArchiveDir;
                    else
                        data->EnumLastDir = data->DiskDirectoryTree;
                    data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(data->EnumLastPath, TRUE);
                    if (data->EnumLastDir == NULL)
                        return NULL; // probably directory "..", otherwise unexpected error
                    data->EnumLastIndex = 0;
                    goto FIND_NEXT;
                }
            }
            else
            {
                if (size != NULL)
                {
                    *size = f->Size;
                    if (enumFiles == 3 && data->DiskDirectoryTree != NULL && (f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    { // we need to take the determined size of the target file link from data->DiskDirectoryTree
                        CFileData const* f2 = data->DiskDirectoryTree->GetFile(data->FilesCountReturnedFromWP);
                        if (f2 != NULL && strcmp(f2->Name, f->Name) == 0) // always true
                            *size = f2->Size;
                        else
                        {
                            TRACE_E("_PanelSalEnumSelection(): unexpected situation: file \"" << f->Name << "\" not found or different in data->DiskDirectoryTree");
                        }
                    }
                }
                data->CurrentIndex++;
                if (data->DiskDirectoryTree != NULL)
                {
                    if (dosName != NULL)
                        *dosName = (f->DosName == NULL) ? f->Name : f->DosName;
                }
                if (fileData != NULL)
                    *fileData = f;
                data->FilesCountReturnedFromWP++;
                return f->Name;
            }
        }
        else
        {
            data->EnumLastIndex++; // move to the next item

        FIND_NEXT: // find another file in the tree

            while (1)
            {
                if (data->EnumLastDir->IsDirectory(data->EnumLastIndex)) // directory -> descending
                {
                    CFileData* f = data->EnumLastDir->GetDirEx(data->EnumLastIndex);
                    BOOL tooLong1 = strlen(data->EnumLastPath) + 1 + f->NameLen >= MAX_PATH;
                    BOOL tooLong2 = data->DiskDirectoryTree != NULL && strlen(data->EnumLastDosPath) + 1 +
                                                                               (f->DosName == NULL ? f->NameLen : strlen(f->DosName)) >=
                                                                           MAX_PATH;
                    if (tooLong1 || tooLong2)
                    { // too long journey
                        if (errorOccured != NULL)
                            *errorOccured = SALENUM_ERROR;
                        _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG),
                                    (tooLong1 || f->DosName == NULL ? f->Name : f->DosName),
                                    (tooLong1 ? data->EnumLastPath : data->EnumLastDosPath));
                        if (parent != NULL &&
                            SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                          MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                        {
                            if (errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                            return NULL; // cancel, we are finishing
                        }
                        data->EnumLastIndex++; // skip the directory with a long name and continue with the next directory or file (or exit the directory)
                    }
                    else
                    {
                        strcat(data->EnumLastPath, "\\");
                        strcat(data->EnumLastPath, f->Name);
                        if (data->DiskDirectoryTree != NULL)
                        {
                            strcat(data->EnumLastDosPath, "\\");
                            strcat(data->EnumLastDosPath, (f->DosName == NULL) ? f->Name : f->DosName);
                        }
                        data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(data->EnumLastIndex);
                        data->EnumLastIndex = 0;
                    }
                }
                else
                {
                    if (data->EnumLastDir->IsFile(data->EnumLastIndex)) // file -> found
                    {
                        if (enumFiles == 2)
                            goto ENUM_NEXT; // files from subdirectories no, just subdirectories

                        CFileData* f = data->EnumLastDir->GetFileEx(data->EnumLastIndex);
                        int zipPathLen = (int)strlen(curZIPPath);
                        BOOL tooLong1 = strlen(data->EnumLastPath) - (zipPathLen + (zipPathLen > 0 ? 1 : 0)) +
                                            1 + f->NameLen >=
                                        MAX_PATH;
                        BOOL tooLong2 = data->DiskDirectoryTree != NULL && strlen(data->EnumLastDosPath) + 1 +
                                                                                   (f->DosName == NULL ? f->NameLen : strlen(f->DosName)) >=
                                                                               MAX_PATH;
                        if (tooLong1 || tooLong2)
                        { // too long journey
                            if (errorOccured != NULL)
                                *errorOccured = SALENUM_ERROR;
                            _snprintf_s(errText, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG),
                                        (tooLong1 || f->DosName == NULL ? f->Name : f->DosName),
                                        (tooLong1 ? data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0) : data->EnumLastDosPath));
                            if (parent != NULL &&
                                SalMessageBox(parent, errText, LoadStr(IDS_ERRORTITLE),
                                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
                            {
                                if (errorOccured != NULL)
                                    *errorOccured = SALENUM_CANCEL;
                                return NULL; // cancel, we are finishing
                            }
                            data->EnumLastIndex++; // skip the file with a long name and continue with the next file (or output from the directory)
                        }
                        else
                        {
                            if (isDir != NULL)
                                *isDir = FALSE;
                            if (size != NULL)
                                *size = f->Size;
                            strcpy(data->EnumTmpFileName, data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0));
                            strcat(data->EnumTmpFileName, "\\");
                            strcat(data->EnumTmpFileName, f->Name);
                            if (data->DiskDirectoryTree != NULL)
                            {
                                strcpy(data->EnumTmpDosFileName, data->EnumLastDosPath); // if used, zipPathLen == 0
                                strcat(data->EnumTmpDosFileName, "\\");
                                strcat(data->EnumTmpDosFileName, (f->DosName == NULL) ? f->Name : f->DosName);
                                if (dosName != NULL)
                                    *dosName = data->EnumTmpDosFileName;
                            }
                            if (fileData != NULL)
                                *fileData = f;
                            return data->EnumTmpFileName;
                        }
                    }
                    else // we are at the end of the directory -> output
                    {
                        // divide the path into directory and subdirectory
                        const char* dir;
                        char* subDir = strrchr(data->EnumLastPath, '\\');
                        char* subDirDos = NULL;
                        if (data->DiskDirectoryTree != NULL)
                            subDirDos = strrchr(data->EnumLastDosPath, '\\');
                        if (subDir != NULL)
                        {
                            dir = data->EnumLastPath;
                            *subDir++ = 0;
                            if (data->DiskDirectoryTree != NULL)
                                *subDirDos++ = 0;
                        }
                        else // we definitely stepped out of the tree
                        {
                            dir = "";
                            subDir = data->EnumLastPath;
                            if (data->DiskDirectoryTree != NULL)
                                subDirDos = data->EnumLastDosPath;
                        }
                        // Check if we haven't already stepped out of the tree
                        if (strlen(dir) == strlen(curZIPPath))
                        {
                            if (fileData != NULL) // find CFileData for the subdirectory we are leaving
                            {
                                int i = data->Indexes[data->CurrentIndex];
                                if (i < data->Dirs->Count)
                                    *fileData = &data->Dirs->At(i);
                                else
                                    TRACE_E("Unexpected situation in _PanelSalEnumSelection.");
                            }

                            data->CurrentIndex++; // we have already gone through this directory
                            data->EnumLastDir = NULL;
                            data->EnumLastIndex = -1;

                            if (isDir != NULL)
                                *isDir = TRUE;
                            if (size != NULL)
                                *size = CQuadWord(0, 0);
                            if (data->DiskDirectoryTree != NULL)
                            {
                                if (dosName != NULL)
                                    *dosName = subDirDos;
                            }
                            return subDir; // return the directory when exiting
                        }
                        // finish the output and perform the shift
                        if (data->DiskDirectoryTree == NULL)
                            data->EnumLastDir = data->ArchiveDir;
                        else
                            data->EnumLastDir = data->DiskDirectoryTree;
                        data->EnumLastDir = data->EnumLastDir->GetSalamanderDir(dir, TRUE);
                        data->EnumLastIndex = data->EnumLastDir->GetIndex(subDir);

                        if (fileData != NULL) // find CFileData for the subdirectory we are leaving
                        {
                            *fileData = data->EnumLastDir->GetDirEx(data->EnumLastIndex);
                        }

                        if (isDir != NULL)
                            *isDir = TRUE;
                        if (size != NULL)
                            *size = CQuadWord(0, 0);
                        int zipPathLen = (int)strlen(curZIPPath);
                        strcpy(data->EnumTmpFileName, data->EnumLastPath + zipPathLen + (zipPathLen > 0 ? 1 : 0));
                        strcat(data->EnumTmpFileName, "\\");
                        strcat(data->EnumTmpFileName, subDir);
                        if (data->DiskDirectoryTree != NULL)
                        {
                            strcpy(data->EnumTmpDosFileName, data->EnumLastDosPath); // if used, zipPathLen == 0
                            strcat(data->EnumTmpDosFileName, "\\");
                            strcat(data->EnumTmpDosFileName, subDirDos);
                            if (dosName != NULL)
                                *dosName = data->EnumTmpDosFileName;
                        }
                        return data->EnumTmpFileName; // return the directory when exiting
                    }
                }
            }
        }
    }
    return NULL;
}

const char* WINAPI PanelSalEnumSelection(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                         const CFileData** fileData, void* param, int* errorOccured)
{
    CALL_STACK_MESSAGE_NONE
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    if (enumFiles == 3)
    {
        TRACE_E("PanelSalEnumSelection(): invalid parameter: enumFiles==3, changing to 1");
        enumFiles = 1;
    }
    return _PanelSalEnumSelection(enumFiles, NULL, isDir, size, fileData, param, parent, errorOccured);
}

void CFilesWindow::UnpackZIPArchive(CFilesWindow* target, BOOL deleteOp, const char* tgtPath)
{
    CALL_STACK_MESSAGE3("CFilesWindow::UnpackZIPArchive(, %d, %s)", deleteOp, tgtPath);
    if (Files->Count + Dirs->Count == 0)
        return;

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BeginStopRefresh(); // He was snoring in his sleep

    //--- obtaining files and directories we will work with
    char subject[MAX_PATH + 100]; // text for Unpack dialog (what is being unpacked)
    char path[MAX_PATH + 200];
    char expanded[200];
    CPanelTmpEnumData data;
    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    data.IndexesCount = GetSelCount();
    if (data.IndexesCount > 1) // valid label
    {
        int files = 0; // number of selected files
        data.Indexes = new int[data.IndexesCount];
        if (data.Indexes == NULL)
        {
            TRACE_E(LOW_MEMORY);
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;
        }
        else
        {
            GetSelItems(data.IndexesCount, data.Indexes);
            int i = data.IndexesCount;
            while (i--)
            {
                BOOL isDir = data.Indexes[i] < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(data.Indexes[i]) : &Files->At(data.Indexes[i] - Dirs->Count);
                if (!isDir)
                    files++;
            }
        }
        // assemble subject for the dialogue
        ExpandPluralFilesDirs(expanded, 200, files, data.IndexesCount - files, epfdmNormal, FALSE);
    }
    else // we are taking the selected file or directory
    {
        int index;
        if (data.IndexesCount == 0)
            index = GetCaretIndex();
        else
            GetSelItems(1, &index);

        if (subDir && index == 0)
        {
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;           // nothing to do
        }
        else
        {
            data.Indexes = new int[1];
            if (data.Indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;
            }
            else
            {
                data.Indexes[0] = index;
                data.IndexesCount = 1;
                // assemble subject for the dialogue
                BOOL isDir = index < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                AlterFileName(path, f->Name, -1, Configuration.FileNameFormat, 0, index < Dirs->Count);
                lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
        }
    }
    sprintf(subject, LoadStr(deleteOp ? IDS_CONFIRM_DELETEFROMARCHIVE : IDS_COPYFROMARCHIVETO), expanded);
    CTruncatedString str;
    str.Set(subject, data.IndexesCount > 1 ? NULL : path);

    data.CurrentIndex = 0;
    data.ZIPPath = GetZIPPath();
    data.Dirs = Dirs;
    data.Files = Files;
    data.ArchiveDir = GetArchiveDir();
    data.EnumLastDir = NULL;
    data.EnumLastIndex = -1;

    char changesRoot[MAX_PATH]; // directory from which disk changes are considered
    changesRoot[0] = 0;

    if (!deleteOp) // copy
    {
        //--- obtaining the target directory
        if (target != NULL && target->Is(ptDisk))
        {
            strcpy(path, target->GetPath());

            target->UserWorkedOnThisPath = TRUE; // default action = working with the path in the destination panel
        }
        else
            path[0] = 0;

        CCopyMoveDialog dlg(HWindow, path, MAX_PATH, LoadStr(IDS_UNPACKCOPY), &str, IDD_COPYDIALOG,
                            Configuration.CopyHistory, COPY_HISTORY_SIZE, TRUE);

    _DLG_AGAIN:

        if (tgtPath != NULL || dlg.Execute() == IDOK)
        {
            if (tgtPath != NULL)
                lstrcpyn(path, tgtPath, MAX_PATH);
            UpdateWindow(MainWindow->HWindow);
            //--- In disk paths, we switch '/' to '\\' and discard duplicated '\\'
            if (!IsPluginFSPath(path) &&
                (path[0] != 0 && path[1] == ':' ||                                             // paths of type X:...
                 (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC path
                 Is(ptDisk) || Is(ptZIPArchive)))                                              // disk+archive relative paths
            {                                                                                  // It's about the file path (absolute or relative) - we turn all '/' into '\\' and discard doubled '\\'
                SlashesToBackslashesAndRemoveDups(path);
            }
            //--- adjusting the entered path -> absolute, without '.' and '..'

            int len = (int)strlen(path);
            BOOL backslashAtEnd = (len > 0 && path[len - 1] == '\\'); // path ends with a backslash -> directory required
            BOOL mustBePath = (len == 2 && LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' &&
                               path[1] == ':'); // A path of type "c:" must be a path even after expansion (not a file)

            int pathType;
            BOOL pathIsDir;
            char* secondPart;
            char textBuf[2 * MAX_PATH + 200];
            if (ParsePath(path, pathType, pathIsDir, secondPart, LoadStr(IDS_ERRORCOPY), NULL, NULL, MAX_PATH))
            {
                // misto konstrukce 'switch' pouzijeme 'if', aby fungovali 'break' + 'continue'
                if (pathType == PATH_TYPE_WINDOWS) // Windows path (disk + UNC)
                {
                    char newDirs[MAX_PATH]; // if it is created for the operation of the directory, we remember its name (in case of an error, we can delete it)
                    newDirs[0] = 0;

                    if (pathIsDir) // existing part of the path is a directory
                    {
                        if (*secondPart != 0) // there is also a non-existent part of the path
                        {
                            if (!backslashAtEnd && !mustBePath) // new path must end with a backslash, otherwise it is considered an operation mask (not supported)
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_UNPACK_OPMASKSNOTSUP), LoadStr(IDS_ERRORCOPY),
                                              MB_OK | MB_ICONEXCLAMATION);
                                if (tgtPath != NULL)
                                {
                                    UpdateWindow(MainWindow->HWindow);
                                    delete[] (data.Indexes);
                                    EndStopRefresh();
                                    return;
                                }
                                goto _DLG_AGAIN;
                            }

                            // create those new directories
                            strcpy(newDirs, path);

                            if (Configuration.CnfrmCreatePath) // ask if the path should be created
                            {
                                BOOL dontShow = FALSE;
                                sprintf(textBuf, LoadStr(IDS_MOVECOPY_CREATEPATH), newDirs);

                                MSGBOXEX_PARAMS params;
                                memset(&params, 0, sizeof(params));
                                params.HParent = HWindow;
                                params.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_HINT;
                                params.Caption = LoadStr(IDS_UNPACKCOPY);
                                params.Text = textBuf;
                                params.CheckBoxText = LoadStr(IDS_MOVECOPY_CREATEPATH_CNFRM);
                                params.CheckBoxValue = &dontShow;
                                BOOL cont = (SalMessageBoxEx(&params) != IDYES);
                                Configuration.CnfrmCreatePath = !dontShow;
                                if (cont)
                                {
                                    SalPathAddBackslash(path, MAX_PATH + 200);
                                    if (tgtPath != NULL)
                                    {
                                        UpdateWindow(MainWindow->HWindow);
                                        delete[] (data.Indexes);
                                        EndStopRefresh();
                                        return;
                                    }
                                    goto _DLG_AGAIN;
                                }
                            }

                            BOOL ok = TRUE;
                            char* st = newDirs + (secondPart - path);
                            char* firstSlash = NULL;
                            while (1)
                            {
                                BOOL invalidPath = *st != 0 && *st <= ' ';
                                char* slash = strchr(st, '\\');
                                if (slash != NULL)
                                {
                                    if (slash > st && (*(slash - 1) <= ' ' || *(slash - 1) == '.'))
                                        invalidPath = TRUE;
                                    *slash = 0;
                                    if (firstSlash == NULL)
                                        firstSlash = slash;
                                }
                                else
                                {
                                    if (*st != 0)
                                    {
                                        char* end = st + strlen(st) - 1;
                                        if (*end <= ' ' || *end == '.')
                                            invalidPath = TRUE;
                                    }
                                }
                                if (invalidPath || !CreateDirectory(newDirs, NULL))
                                {
                                    sprintf(textBuf, LoadStr(IDS_CREATEDIRFAILED), newDirs);
                                    SalMessageBox(HWindow, textBuf, LoadStr(IDS_ERRORCOPY), MB_OK | MB_ICONEXCLAMATION);
                                    ok = FALSE;
                                    break;
                                }
                                if (slash != NULL)
                                    *slash = '\\';
                                else
                                    break; // that was the last '\\'
                                st = slash + 1;
                            }

                            // finding old paths (where new directories were created)
                            memcpy(changesRoot, path, secondPart - path);
                            changesRoot[secondPart - path] = 0;

                            if (!ok)
                            {
                                //--- refresh non-automatically refreshed directories
                                // If directory creation failed, we will report changes immediately (user may
                                // choose a completely different path next time); the newly created path is not disturbed (almost dead-code)
                                MainWindow->PostChangeOnPathNotification(changesRoot, TRUE);

                                SalPathAddBackslash(path, MAX_PATH + 200);
                                if (tgtPath != NULL)
                                {
                                    UpdateWindow(MainWindow->HWindow);
                                    delete[] (data.Indexes);
                                    EndStopRefresh();
                                    return;
                                }
                                goto _DLG_AGAIN;
                            }
                            if (firstSlash != NULL)
                                *firstSlash = 0; // the name of the first created directory comes to newDirs
                        }
                    }
                    else // copy file - 'secondPart' points to the file name in the path 'path'
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_UNPACK_OPMASKSNOTSUP), LoadStr(IDS_ERRORCOPY),
                                      MB_OK | MB_ICONEXCLAMATION);
                        if (backslashAtEnd || mustBePath)
                            SalPathAddBackslash(path, MAX_PATH + 200);
                        if (tgtPath != NULL)
                        {
                            UpdateWindow(MainWindow->HWindow);
                            delete[] (data.Indexes);
                            EndStopRefresh();
                            return;
                        }
                        goto _DLG_AGAIN;
                    }

                    // if no new directories are being created, changes start at the target path
                    if (changesRoot[0] == 0)
                        strcpy(changesRoot, path);

                    //--- custom unpacking
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    if (PackUncompress(MainWindow->HWindow, this, GetZIPArchive(), PluginData.GetInterface(),
                                       path, GetZIPPath(), PanelSalEnumSelection, &data))
                    {                        // unpacking was successful
                        if (tgtPath == NULL) // if it's not about drag&drop (deselection is not done there)
                        {
                            SetSel(FALSE, -1, TRUE);                        // explicit override
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        }
                    }
                    else
                    {
                        if (newDirs[0] != 0)
                            RemoveEmptyDirs(newDirs);
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    if (GetForegroundWindow() == MainWindow->HWindow) // for some incomprehensible reason, the focus disappears from the panel when dragging and dropping into Explorer, we will return it there
                        RestoreFocusInSourcePanel();
                }
                else
                {
                    SalMessageBox(HWindow, LoadStr(IDS_UNPACK_ONLYDISK), LoadStr(IDS_ERRORCOPY),
                                  MB_OK | MB_ICONEXCLAMATION);
                    if (pathType == PATH_TYPE_ARCHIVE && (backslashAtEnd || mustBePath))
                    {
                        SalPathAddBackslash(path, MAX_PATH + 200);
                    }
                    if (tgtPath != NULL)
                    {
                        UpdateWindow(MainWindow->HWindow);
                        delete[] (data.Indexes);
                        EndStopRefresh();
                        return;
                    }
                    goto _DLG_AGAIN;
                }
            }
            else
            {
                if (tgtPath != NULL)
                {
                    UpdateWindow(MainWindow->HWindow);
                    delete[] (data.Indexes);
                    EndStopRefresh();
                    return;
                }
                goto _DLG_AGAIN;
            }

            //--- refresh non-automatically refreshed directories
            // change in the target path and its subdirectories (creating new directories and extracting
            // file/directory)
            MainWindow->PostChangeOnPathNotification(changesRoot, TRUE);
            // change in the directory where the archive is located (unpacking should not occur, but let's refresh just in case)
            MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
        }
    }
    else // delete
    {
        //--- we will ask if he means it seriously
        HICON hIcon = (HICON)HANDLES(LoadImage(Shell32DLL, MAKEINTRESOURCE(WindowsVistaAndLater ? 16777 : 161), // delete icon
                                               IMAGE_ICON, 32, 32, IconLRFlags));
        if (!Configuration.CnfrmFileDirDel ||
            CMessageBox(HWindow, MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_SILENT,
                        LoadStr(IDS_CONFIRM_DELETE_TITLE), &str, NULL,
                        NULL, hIcon, 0, NULL, NULL, NULL, NULL)
                    .Execute() == IDYES)
        {
            UpdateWindow(MainWindow->HWindow);
            //--- finding non-empty directories - possible queries for deletion
            BOOL cancel = FALSE;
            if (Configuration.CnfrmNEDirDel)
            {
                int i;
                for (i = 0; i < data.IndexesCount; i++)
                {
                    if (data.Indexes[i] < Dirs->Count)
                    {
                        int dirsCount = 0;
                        int filesCount = 0;
                        GetArchiveDir()->GetDirSize(GetZIPPath(), Dirs->At(data.Indexes[i]).Name,
                                                    &dirsCount, &filesCount);
                        if (dirsCount + filesCount > 0)
                        {
                            char name[2 * MAX_PATH];
                            strcpy(name, GetZIPArchive());
                            if (GetZIPPath()[0] != 0)
                            {
                                if (GetZIPPath()[0] != '\\')
                                    strcat(name, "\\");
                                strcat(name, GetZIPPath());
                            }
                            strcat(name, "\\");
                            strcat(name, Dirs->At(data.Indexes[i]).Name);

                            char text[2 * MAX_PATH + 100];
                            sprintf(text, LoadStr(IDS_NONEMPTYDIRDELCONFIRM), name);
                            int res = SalMessageBox(HWindow, text, LoadStr(IDS_QUESTION),
                                                    MB_YESNOCANCEL | MB_ICONQUESTION);
                            if (res == IDCANCEL)
                            {
                                cancel = TRUE;
                                break;
                            }
                            if (res == IDNO)
                            {
                                memmove(data.Indexes + i, data.Indexes + i + 1, (data.IndexesCount - i - 1) * sizeof(int));
                                data.IndexesCount--;
                                i--;
                            }
                        }
                    }
                }
                if (data.IndexesCount == 0)
                    cancel = TRUE;
            }
            //--- custom deletion
            if (!cancel)
            {
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                if (PackDelFromArc(MainWindow->HWindow, this, GetZIPArchive(), PluginData.GetInterface(),
                                   GetZIPPath(), PanelSalEnumSelection, &data))
                {                                                   // deletion successful
                    SetSel(FALSE, -1, TRUE);                        // explicit override
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                //--- refresh non-automatically refreshed directories
                // change in the directory where the archive is located
                MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
            }
        }
        HANDLES(DestroyIcon(hIcon));
    }

    UpdateWindow(MainWindow->HWindow);
    delete[] (data.Indexes);

    //--- if any Total Commander window is active, exit suspend mode
    EndStopRefresh();
}

void CFilesWindow::DeleteFromZIPArchive()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DeleteFromZIPArchive()");
    UnpackZIPArchive(NULL, TRUE); // It's almost the same
}

BOOL _ReadDirectoryTree(HWND parent, char (&path)[MAX_PATH], char* name, CSalamanderDirectory* dir,
                        int* errorOccured, BOOL getLinkTgtFileSize, BOOL* errGetFileSizeOfLnkTgtIgnAll,
                        int* containsDirLinks, char* linkName)
{
    CALL_STACK_MESSAGE4("_ReadDirectoryTree(, %s, %s, , , %d, , ,)", path, name, getLinkTgtFileSize);
    char* end = path + strlen(path);
    char text[2 * MAX_PATH + 100];
    if ((end - path) + (*(end - 1) != '\\' ? 1 : 0) + strlen(name) + 2 >= _countof(path))
        return TRUE; // too long path: we will continue, we will show the error when enumerating
    if (*(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    strcpy(end, name);
    strcat(end, "\\*");

    WIN32_FIND_DATA file;
    HANDLE find = HANDLES_Q(FindFirstFile(path, &file));
    *end = 0; // repair the road
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
        {
            if (errorOccured != NULL)
                *errorOccured = SALENUM_ERROR;
            strcpy(end, name);
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            *end = 0; // repair the road
            if (parent != NULL &&
                SalMessageBox(parent, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                if (errorOccured != NULL)
                    *errorOccured = SALENUM_CANCEL;
                return FALSE; // user wants to quit
            }
        }
        return TRUE; // user wants to continue
    }
    else
    {
        strcpy(end, name);
        char* end2 = end + strlen(end);
        BOOL ok = TRUE;
        CFileData newF; // we no longer work with these items
        if (dir != NULL)
        {
            newF.PluginData = -1; // -1 is just ignored
            newF.Association = 0;
            newF.Selected = 0;
            newF.Shared = 0;
            newF.Archive = 0;
            newF.SizeValid = 0;
            newF.Dirty = 0; // unnecessary, just for the sake of it
            newF.CutToClip = 0;
            newF.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
            newF.IconOverlayDone = 0;
        }
        else
            memset(&newF, 0, sizeof(newF));
        BOOL testFindNextErr = TRUE;

        do
        {
            if (file.cFileName[0] == 0 ||
                file.cFileName[0] == '.' &&
                    (file.cFileName[1] == 0 || (file.cFileName[1] == '.' && file.cFileName[2] == 0)))
                continue; // "." and ".."

            static DWORD lastBreakCheck = 0;
            if (containsDirLinks != NULL && GetTickCount() - lastBreakCheck > 200)
            {
                lastBreakCheck = GetTickCount();
                if (UserWantsToCancelSafeWaitWindow())
                {
                    *containsDirLinks = 2; // After interruption, we simulate an error to immediately terminate the search
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
            }

            if (dir != NULL)
            {
                newF.Size = CQuadWord(file.nFileSizeLow, file.nFileSizeHigh);

                BOOL cancel = FALSE;
                if (getLinkTgtFileSize &&
                    (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 &&   // it's about the file
                    (file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0) // it's about the link
                {                                                                // When using a symlink to a file, we determine the size of the target file
                    CQuadWord size;
                    if (SalPathAppend(path, file.cFileName, _countof(path)))
                    { // only if the path is not too long (we will show any errors during enumeration)
                        if (GetLinkTgtFileSize(parent, path, NULL, &size, &cancel, errGetFileSizeOfLnkTgtIgnAll))
                            newF.Size = size;
                        else
                        {
                            if (cancel && errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                        }
                    }
                    *end2 = 0; // restoring 'path' to its original state
                }

                newF.Name = !cancel ? DupStr(file.cFileName) : NULL;
                newF.DosName = NULL;
                if (cancel || newF.Name == NULL)
                {
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
                newF.NameLen = strlen(newF.Name);
                if (!Configuration.SortDirsByExt && (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // directory, it must be a disk
                {
                    newF.Ext = newF.Name + newF.NameLen; // directories do not have extensions
                }
                else
                {
                    newF.Ext = strrchr(newF.Name, '.');
                    if (newF.Ext == NULL)
                        newF.Ext = newF.Name + newF.NameLen; // ".cvspass" in Windows is an extension ...
                                                             //        if (newF.Ext == NULL || newF.Ext == newF.Name) newF.Ext = newF.Name + newF.NameLen;
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

                newF.Attr = file.dwFileAttributes;
                newF.LastWrite = file.ftLastWriteTime;
                newF.Hidden = newF.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
                newF.IsOffline = newF.Attr & FILE_ATTRIBUTE_OFFLINE ? 1 : 0;
            }

            if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) // directory, it must be a disk
            {
                CSalamanderDirectory* salDir = NULL;
                if (dir != NULL)
                {
                    newF.IsLink = (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // volume mount point or junction point = display directory with link overlay
                    BOOL addDirOK = dir->AddDir("", newF, NULL);
                    if (addDirOK)
                        salDir = dir->GetSalamanderDir(newF.Name, FALSE); // allocate sal-dir for writing
                    else
                    {
                        free(newF.Name);
                        if (newF.DosName != NULL)
                            free(newF.DosName);
                    }
                    if (salDir == NULL)
                    {
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
                else // just looking for the 1st link to the directory
                {
                    if ((file.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                    {
                        *containsDirLinks = 1; // After finding, we simulate an error to immediately terminate the search
                        *end2 = 0;
                        _snprintf_s(linkName, MAX_PATH, _TRUNCATE, "%s\\%s", path, file.cFileName); // we can safely cut, just the text to the message
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
                if (!_ReadDirectoryTree(parent, path, file.cFileName, salDir, errorOccured, getLinkTgtFileSize,
                                        errGetFileSizeOfLnkTgtIgnAll, containsDirLinks, linkName))
                {
                    ok = FALSE;
                    testFindNextErr = FALSE;
                    break;
                }
            }
            else // file
            {
                if (dir != NULL)
                {
                    if (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT)
                        newF.IsLink = 1; // if the file is a reparse-point (which may not be possible at all) = display it with a link overlay
                    else
                        newF.IsLink = IsFileLink(newF.Ext);

                    if (!dir->AddFile("", newF, NULL))
                    {
                        free(newF.Name);
                        if (newF.DosName != NULL)
                            free(newF.DosName);
                        ok = FALSE;
                        testFindNextErr = FALSE;
                        break;
                    }
                }
            }
        } while (FindNextFile(find, &file));
        DWORD err = GetLastError();
        HANDLES(FindClose(find));
        *end = 0; // repair the road

        if (testFindNextErr && err != ERROR_NO_MORE_FILES)
        {
            if (errorOccured != NULL)
                *errorOccured = SALENUM_ERROR;
            strcpy(end, name);
            sprintf(text, LoadStr(IDS_CANNOTREADDIR), path, GetErrorText(err));
            *end = 0; // repair the road
            if (parent != NULL &&
                SalMessageBox(parent, text, LoadStr(IDS_ERRORTITLE),
                              MB_OKCANCEL | MB_ICONEXCLAMATION) == IDCANCEL)
            {
                if (errorOccured != NULL)
                    *errorOccured = SALENUM_CANCEL;
                return FALSE; // user wants to quit
            }
        }

        if (!ok)
        {
            if (errorOccured != NULL && *errorOccured == SALENUM_SUCCESS)
                *errorOccured = SALENUM_ERROR;
            return FALSE;
        }
    }
    return TRUE;
}

CSalamanderDirectory* ReadDirectoryTree(HWND parent, CPanelTmpEnumData* data, int* errorOccured,
                                        BOOL getLinkTgtFileSize, int* containsDirLinks, char* linkName)
{
    CALL_STACK_MESSAGE2("ReadDirectoryTree(, , , %d, ,)", getLinkTgtFileSize);
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;
    if (containsDirLinks != NULL)
        *containsDirLinks = 0;
    BOOL cancel = FALSE;
    if (data->CurrentIndex >= data->IndexesCount || data->WorkPath[0] == 0)
    {
        TRACE_E("Unexpected situation in ReadDirectoryTree().");
        if (errorOccured != NULL)
            *errorOccured = SALENUM_ERROR;
        return NULL; // nothing to do
    }

    BOOL errGetFileSizeOfLnkTgtIgnAll = parent == NULL; // quiet mode = do not display error, return success

    CSalamanderDirectory* dir = containsDirLinks == NULL ? new CSalamanderDirectory(TRUE) : NULL;
    if (dir == NULL && containsDirLinks == NULL)
    {
        if (errorOccured != NULL)
            *errorOccured = SALENUM_ERROR;
        return NULL; // out of memory
    }

    int index = data->CurrentIndex;
    CFileData newF;
    if (dir != NULL)
    {
        newF.PluginData = -1; // -1 is just ignored
        newF.Association = 0;
        newF.Selected = 0;
        newF.Shared = 0;
        newF.Archive = 0;
        newF.SizeValid = 0;
        newF.Dirty = 0; // unnecessary, just for the sake of it
        newF.CutToClip = 0;
        newF.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        newF.IconOverlayDone = 0;
    }
    else
        memset(&newF, 0, sizeof(newF));
    while (index < data->IndexesCount)
    {
        int i = data->Indexes[index++];
        BOOL isDir = i < data->Dirs->Count;
        CFileData* f = &(isDir ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));

        // to be on the safe side, let's skip ".." (a bug report came from "1.6 beta 5" on this topic; I don't understand how
        // it could have come to this)
        if (f->Name[0] == '.' && f->Name[1] == '.' && f->Name[2] == 0)
            continue;

        if (dir != NULL)
        {
            newF.Name = DupStr(f->Name);
            newF.DosName = NULL;
            if (newF.Name == NULL)
                goto RETURN_ERROR;
            if (f->DosName != NULL)
            {
                newF.DosName = DupStr(f->DosName);
                if (newF.DosName == NULL)
                    goto RETURN_ERROR;
            }
            newF.NameLen = f->NameLen;
            newF.Ext = newF.Name + (f->Ext - f->Name);
            newF.Size = f->Size;
            newF.Attr = f->Attr;
            newF.LastWrite = f->LastWrite;
            newF.Hidden = f->Hidden;
            newF.IsLink = f->IsLink;
            newF.IsOffline = f->IsOffline;
        }

        char path[MAX_PATH];
        if (isDir) // directory
        {
            CSalamanderDirectory* salDir = NULL;
            if (dir != NULL)
            {
                BOOL addDirOK = dir->AddDir("", newF, NULL);
                if (addDirOK)
                {
                    newF.Name = NULL; // It is already in the directory, do not call free() on it - in case of the error below
                    newF.DosName = NULL;
                    salDir = dir->GetSalamanderDir(f->Name, FALSE); // allocate sal-dir for writing
                }
                if (salDir == NULL)
                    goto RETURN_ERROR;
            }
            else
            {
                if ((f->Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0) // found link to directory, finishing...
                {
                    *containsDirLinks = 1;
                    strcpy(path, data->WorkPath);
                    SalPathRemoveBackslash(path);
                    _snprintf_s(linkName, MAX_PATH, _TRUNCATE, "%s\\%s", path, f->Name); // we can safely cut, just the text to the message
                    break;
                }
            }

            strcpy(path, data->WorkPath);
            if (!_ReadDirectoryTree(parent, path, f->Name, salDir, errorOccured, getLinkTgtFileSize,
                                    &errGetFileSizeOfLnkTgtIgnAll, containsDirLinks, linkName))
            {
                goto RETURN_ERROR;
            }
        }
        else // file
        {
            if (dir != NULL)
            {
                if (getLinkTgtFileSize && (newF.Attr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
                { // When using a symlink to a file, we determine the size of the target file
                    CQuadWord size;
                    strcpy(path, data->WorkPath);
                    if (SalPathAppend(path, newF.Name, _countof(path)))
                    { // only if the path is not too long (we will show any errors during enumeration)
                        if (GetLinkTgtFileSize(parent, path, NULL, &size, &cancel, &errGetFileSizeOfLnkTgtIgnAll))
                            newF.Size = size;
                        else
                        {
                            if (cancel && errorOccured != NULL)
                                *errorOccured = SALENUM_CANCEL;
                        }
                    }
                }
                if (cancel || !dir->AddFile("", newF, NULL))
                {
                RETURN_ERROR:

                    if (errorOccured != NULL && *errorOccured == SALENUM_SUCCESS)
                        *errorOccured = SALENUM_ERROR;
                    if (newF.Name != NULL)
                        free(newF.Name);
                    if (newF.DosName != NULL)
                        free(newF.DosName);
                    if (dir != NULL)
                        delete dir;
                    return NULL;
                }
            }
        }
    }
    return dir;
}

const char* WINAPI PanelEnumDiskSelection(HWND parent, int enumFiles, const char** dosName, BOOL* isDir,
                                          CQuadWord* size, DWORD* attr, FILETIME* lastWrite, void* param,
                                          int* errorOccured)
{
    CALL_STACK_MESSAGE_NONE
    CPanelTmpEnumData* data = (CPanelTmpEnumData*)param;
    if (errorOccured != NULL)
        *errorOccured = SALENUM_SUCCESS;

    if (enumFiles == -1)
    {
        if (dosName != NULL)
            *dosName = NULL;
        if (isDir != NULL)
            *isDir = FALSE;
        if (size != NULL)
            *size = CQuadWord(0, 0);
        if (attr != NULL)
            *attr = 0;
        if (lastWrite != NULL)
            memset(lastWrite, 0, sizeof(FILETIME));
        data->Reset();
        return NULL;
    }

    if (enumFiles > 0)
    {
        if (data->DiskDirectoryTree == NULL)
        {
            data->DiskDirectoryTree = ReadDirectoryTree(parent, data, errorOccured, enumFiles == 3, NULL, NULL);
            if (data->DiskDirectoryTree == NULL)
                return NULL; // error, we are finishing
        }
        const CFileData* f = NULL;
        const char* ret = _PanelSalEnumSelection(enumFiles, dosName, isDir, size, &f, data, parent, errorOccured);
        if (ret != NULL)
        {
            if (f != NULL)
            {
                if (attr != NULL)
                    *attr = f->Attr;
                if (lastWrite != NULL)
                    *lastWrite = f->LastWrite;
            }
            else
            {
                if (attr != NULL)
                    *attr = 0;
                if (lastWrite != NULL)
                    memset(lastWrite, 0, sizeof(FILETIME));
            }
        }
        return ret;
    }
    else
    {
        if (data->CurrentIndex >= data->IndexesCount)
            return NULL;
        int i = data->Indexes[data->CurrentIndex++];
        if (isDir != NULL)
            *isDir = i < data->Dirs->Count;
        CFileData* f = &(i < data->Dirs->Count ? data->Dirs->At(i) : data->Files->At(i - data->Dirs->Count));
        if (dosName != NULL)
            *dosName = (f->DosName == NULL) ? f->Name : f->DosName;
        if (size != NULL)
            *size = f->Size;
        if (attr != NULL)
            *attr = f->Attr;
        if (lastWrite != NULL)
            *lastWrite = f->LastWrite;
        return f->Name;
    }
}

void CFilesWindow::Pack(CFilesWindow* target, int pluginIndex, const char* pluginName, int delFilesAfterPacking)
{
    CALL_STACK_MESSAGE4("CFilesWindow::Pack(, %d, %s, %d)", pluginIndex, pluginName, delFilesAfterPacking);
    if (Files->Count + Dirs->Count == 0)
        return;

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    BeginStopRefresh(); // He was snoring in his sleep

    //--- obtaining files and directories we will work with
    char subject[MAX_PATH + 100]; // text for Unpack dialog (what is being unpacked)
    char path[MAX_PATH];
    char text[1000];
    BOOL nameByItem;
    CPanelTmpEnumData data;
    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    data.IndexesCount = GetSelCount();
    char expanded[MAX_PATH + 100];
    int files = 0;             // number of selected files
    if (data.IndexesCount > 1) // valid label
    {
        nameByItem = FALSE;
        data.Indexes = new int[data.IndexesCount];
        if (data.Indexes == NULL)
        {
            TRACE_E(LOW_MEMORY);
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;
        }
        else
        {
            GetSelItems(data.IndexesCount, data.Indexes);
            int i = data.IndexesCount;
            while (i--)
            {
                BOOL isDir = data.Indexes[i] < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(data.Indexes[i]) : &Files->At(data.Indexes[i] - Dirs->Count);
                if (!isDir)
                    files++;
            }
        }
        // assemble subject for the dialogue
        ExpandPluralFilesDirs(expanded, MAX_PATH + 100, files, data.IndexesCount - files, epfdmNormal, FALSE);
    }
    else // we are taking the selected file or directory
    {
        int index;
        if (data.IndexesCount == 0)
        {
            index = GetCaretIndex();
            nameByItem = TRUE; // for compatibility with Sal2.0
        }
        else
        {
            GetSelItems(1, &index);
            nameByItem = FALSE; // for compatibility with Sal2.0
        }

        // note on compatibility with 2.0
        // soucasna implementace je sice nelogicka, protoze se Salamander v pripade jednoho oznaceneho
        // file behaves differently than in the case of a single focused file, but it has one advantage:
        // the user has the option to choose a default file name

        if (subDir && index == 0)
        {
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;           // nothing to do
        }
        else
        {
            data.Indexes = new int[1];
            if (data.Indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;
            }
            else
            {
                data.Indexes[0] = index;
                data.IndexesCount = 1;
                // assemble subject for the dialogue
                BOOL isDir = index < Dirs->Count;
                if (!isDir)
                    files = 1;
                CFileData* f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                AlterFileName(path, f->Name, -1, Configuration.FileNameFormat, 0, index < Dirs->Count);
                strcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
            }
        }
    }
    sprintf(subject, LoadStr(IDS_PACKTOARCHIVE), expanded);
    CTruncatedString str;
    str.Set(subject, data.IndexesCount > 1 ? NULL : path);

    data.CurrentIndex = 0;
    data.ZIPPath = GetZIPPath();
    data.Dirs = Dirs;
    data.Files = Files;
    data.ArchiveDir = GetArchiveDir();
    lstrcpyn(data.WorkPath, GetPath(), MAX_PATH);
    data.EnumLastDir = NULL;
    data.EnumLastIndex = -1;

    //--- then we pack it into a new file, we ask for its name
    char fileBuf[MAX_PATH];    // file to be packaged
    char fileBufAlt[MAX_PATH]; // Alternative name that will be in the Pack dialog combobox

    if (nameByItem) // If there is only one file/directory, the archive takes its name
    {
        char* ext = strrchr(path, '.');
        if (data.Indexes[0] < Dirs->Count || ext == NULL) // ".cvspass" in Windows is an extension ...
                                                          //  if (data.Indexes[0] < Dirs->Count || ext == NULL || ext == path)
        {                                                 // subdirectory or without extension
            strcpy(fileBuf, path);
            strcat(fileBuf, ".");
        }
        else
        {
            memcpy(fileBuf, path, ext + 1 - path);
            fileBuf[ext + 1 - path] = 0;
        }
    }
    else
    {
        // build default archive name
        const char* end = GetPath() + strlen(GetPath());
        if (end > GetPath() && *(end - 1) == '\\')
            end--;
        const char* dir = end;
        char root[MAX_PATH];
        GetRootPath(root, GetPath());
        const char* min = GetPath() + strlen(root);
        while (dir > min && *(dir - 1) != '\\')
            dir--;
        if (dir < end)
        {
            memcpy(fileBuf, dir, end - dir);
            fileBuf[end - dir] = '.';
            fileBuf[end - dir + 1] = 0;
        }
        else
            strcpy(fileBuf, LoadStr(IDS_NEW_ARCHIVE));
    }

    if (pluginIndex != -1)
    {
        int i;
        for (i = 0; i < PackerConfig.GetPackersCount(); i++)
        {
            if (PackerConfig.GetPackerType(i) == -pluginIndex - 1)
            {
                PackerConfig.SetPreferedPacker(i);
                break;
            }
        }
        if (i == PackerConfig.GetPackersCount()) // The requested plugin was not found
        {
            sprintf(subject, LoadStr(IDS_PLUGINPACKERNOTFOUND), pluginName);
            SalMessageBox(HWindow, subject, LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
            delete[] (data.Indexes);
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;
        }
    }

    if (PackerConfig.GetPreferedPacker() == -1)
    { // if there is no preferred one, we will select the first one (so that users don't stare at an empty combo)
        PackerConfig.SetPreferedPacker(0);
    }
    if (PackerConfig.GetPreferedPacker() != -1) // necessary even after Set (did not have to spread -> still returns -1)
        strcat(fileBuf, PackerConfig.GetPackerExt(PackerConfig.GetPreferedPacker()));

    strcpy(fileBufAlt, fileBuf);

    if (target->Is(ptDisk))
    {
        // Based on the configuration, we will modify one of the paths to go to the target panel
        char* buff = Configuration.UseAnotherPanelForPack ? fileBuf : fileBufAlt;

        if (Configuration.UseAnotherPanelForPack)
            target->UserWorkedOnThisPath = TRUE; // default action = working with the path in the destination panel

        int l = (int)strlen(target->GetPath());
        if (l > 0 && target->GetPath()[l - 1] == '\\')
            l--;
        int ll = (int)strlen(buff);
        if (l + 2 + ll < MAX_PATH)
        {
            memmove(buff + l + 1, buff, ll + 1);
            buff[l] = '\\';
            memcpy(buff, target->GetPath(), l);
        }
    }

    // if no item is selected, we will select the one under focus and save its name
    char temporarySelected[MAX_PATH];
    temporarySelected[0] = 0;
    SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

    if (delFilesAfterPacking == 1)
        PackerConfig.Move = TRUE;
    if (delFilesAfterPacking == 0 || // Petr: change the default: user must always enable deletion, it is too dangerous
        delFilesAfterPacking == 2)
    {
        PackerConfig.Move = FALSE;
    }

    BOOL first = TRUE;

_PACK_AGAIN:

    CPackDialog dlg(HWindow, fileBuf, fileBufAlt, &str, &PackerConfig);

    // Since Windows Vista, Microsoft introduced a very useful feature: quick rename implicitly selects only the name without the dot and extension
    // the same code is still in second place
    int selectionEnd = -1;
    if (first)
    {
        if (!Configuration.QuickRenameSelectAll)
        {
            const char* dot = strrchr(fileBuf, '.');
            if (dot != NULL && dot > fileBuf) // While it is true that ".cvspass" in Windows is an extension, Explorer marks the entire name for ".cvspass", so we do the same
                                              //    if (dot != NULL)
                selectionEnd = (int)(dot - fileBuf);
            dlg.SetSelectionEnd(selectionEnd);
        }
        first = FALSE; // After an error, we will receive the full file name, so we will mark it completely
    }

    if (dlg.Execute() == IDOK)
    {
        UpdateWindow(MainWindow->HWindow);
        //--- we will change the archive name to full-name
        int errTextID;
        BOOL empty = FALSE;
        char nextFocus[MAX_PATH];
        nextFocus[0] = 0;
        if (SalGetFullName(fileBuf, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus))
        {
            //--- searching for a link to a directory in the packaging source, as it cannot be combined with "delete files after packing"
            BOOL performPack = TRUE;
            if (PackerConfig.Move)
            {
                int containsDirLinks = 0;
                SetCurrentDirectory(GetPath());
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help
                CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 3000, TRUE, NULL);

                // Let's try to find the 1st link in the directory, if we find it, simulate an error to stop the search
                char linkName[MAX_PATH];
                linkName[0] = 0;
                ReadDirectoryTree(NULL /* silent mode*/, &data, NULL, FALSE, &containsDirLinks, linkName);

                DestroySafeWaitWindow();

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                SetCurrentDirectoryToSystem();

                // contains a link to a directory = cannot be combined with "delete files after packing":
                // I can't handle a situation where one file from a directory fails to be packed (it is enough if
                // the file was locked or I didn't have the rights to it), where I went through the link - delete the whole
                // The link is incorrect because it will not be visible that something failed to wrap and delete everything except for
                // processing one file per link pass is also bad because it affects the original content
                // directory where they will report it as a bug, because it is unexpected
                if (containsDirLinks == 1)
                {
                    _snprintf_s(text, _TRUNCATE, LoadStr(IDS_DELFILESAFTERPACKINGNOLINKS), linkName);
                    SalMessageBox(HWindow, text, LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
                    PackerConfig.Move = FALSE;
                    goto _PACK_AGAIN;
                }
                if (containsDirLinks == 2) // user interrupted loading (ESC or clicked on the close button of the waiting window)
                    performPack = FALSE;
            }

            //--- confirmation of adding (updating) to an existing archive
            if (performPack && Configuration.CnfrmAddToArchive && FileExists(fileBuf))
            {
                BOOL dontShow = !Configuration.CnfrmAddToArchive;

                char filesDirs[200];
                ExpandPluralFilesDirs(filesDirs, 200, files, data.IndexesCount - files, epfdmNormal, FALSE);

                char buff[3 * MAX_PATH];
                sprintf(buff, LoadStr(IDS_CONFIRM_ADDTOARCHIVE), "%s", filesDirs);

                char* namePart = strrchr(fileBuf, '\\');
                if (namePart != NULL)
                    namePart++;
                else
                    namePart = fileBuf;
                CTruncatedString str2;
                str2.Set(buff, namePart);

                char alias[200];
                sprintf(alias, "%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_CONFIRM_ADDTOARCHIVE_ADD),
                        DIALOG_NO, LoadStr(IDS_CONFIRM_ADDTOARCHIVE_OVER));
                CMessageBox msgBox(HWindow,
                                   MSGBOXEX_YESNOCANCEL | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT | MSGBOXEX_HINT,
                                   LoadStr(IDS_QUESTION),
                                   &str2,
                                   LoadStr(IDS_CONFIRM_ADDTOARCHIVE_NOASK),
                                   &dontShow,
                                   NULL, 0, NULL, alias, NULL, NULL);
                int msgBoxRed = msgBox.Execute();
                performPack = (msgBoxRed == IDYES);
                if (msgBoxRed == IDNO) // OVERWRITE
                {
                    ClearReadOnlyAttr(fileBuf); // to be deleted ...
                    if (!DeleteFile(fileBuf))
                    {
                        DWORD err;
                        err = GetLastError();
                        SalMessageBox(HWindow, GetErrorText(err), LoadStr(IDS_ERROROVERWRITINGFILE), MB_OK | MB_ICONEXCLAMATION);
                        // we will fall into _PACK_AGAIN
                    }
                    else
                        performPack = TRUE;
                }
                Configuration.CnfrmAddToArchive = !dontShow;
                if (!performPack)
                    goto _PACK_AGAIN;
            }

            //--- custom packaging
            if (performPack)
            {
                SetCurrentDirectory(GetPath());
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                if (PackerConfig.ExecutePacker(this, fileBuf, PackerConfig.Move, GetPath(),
                                               PanelEnumDiskSelection, &data))
                { // packing was successful
                    // if (nextFocus[0] != 0) strcpy(NextFocusName, nextFocus);
                    FocusFirstNewItem = TRUE; // archive renamed with focus and plug-in (e.g. SFX -> archive.exe)

                    SetSel(FALSE, -1, TRUE);                        // explicit override
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                }
                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                SetCurrentDirectoryToSystem();

                //--- refresh non-automatically refreshed directories
                // changes in the directory where the resulting archive is located
                CutDirectory(fileBuf); // It doesn't have to work, but we don't handle this case (refresh doesn't matter)
                MainWindow->PostChangeOnPathNotification(fileBuf, FALSE);
                // Move from disk to archive -> also change on disk (delete file/directory)
                if (PackerConfig.Move)
                {
                    // changes in the current directory in the panel including its subdirectories
                    MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                }
            }
        }
        else
        {
            SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_PACKTITLE), MB_OK | MB_ICONEXCLAMATION);
            goto _PACK_AGAIN;
        }
    }

    // if we have selected an item, we will deselect it again
    UnselectItemWithName(temporarySelected);

    UpdateWindow(MainWindow->HWindow);
    delete[] (data.Indexes);

    //--- if any Total Commander window is active, exit suspend mode
    EndStopRefresh();
}

void CFilesWindow::Unpack(CFilesWindow* target, int pluginIndex, const char* pluginName, const char* unpackMask)
{
    CALL_STACK_MESSAGE4("CFilesWindow::Unpack(, %d, %s, %s)", pluginIndex, pluginName, unpackMask);

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    int i = GetCaretIndex();
    if (i >= Dirs->Count && i < Dirs->Count + Files->Count)
    {
        BeginStopRefresh();

        CFileData* file = &Files->At(i - Dirs->Count);
        char path[MAX_PATH];
        char pathAlt[MAX_PATH]; // alternative path that will be in the combobox of the UnPack dialog
        char mask[MAX_PATH];
        char subject[MAX_PATH + 100];
        path[0] = 0;
        pathAlt[0] = 0;
        if (target->Is(ptDisk))
        {
            if (Configuration.UseAnotherPanelForUnpack)
            {
                target->UserWorkedOnThisPath = TRUE; // default action = working with the path in the destination panel
                strcpy(path, target->GetPath());
            }
            else
                strcpy(pathAlt, target->GetPath());
        }
        char fileName[MAX_PATH];
        AlterFileName(fileName, file->Name, -1, Configuration.FileNameFormat, 0, FALSE);
        if (Configuration.UseSubdirNameByArchiveForUnpack)
        {
            int i2;
            for (i2 = 0; i2 < 2; i2++) // for path and pathAlt
            {
                char* buff = (i2 == 0) ? path : pathAlt;
                int l = (int)strlen(buff);
                if (l + (l > 0 && buff[l - 1] != '\\' ? 1 : 0) + (file->Ext - file->Name) - (*file->Ext == 0 ? 0 : 1) < MAX_PATH)
                {
                    if (l > 0 && buff[l - 1] != '\\')
                        buff[l++] = '\\';
                    memcpy(buff + l, fileName, file->Ext - file->Name);
                    buff[l + (file->Ext - file->Name) - (*file->Ext == 0 ? 0 : 1)] = 0;
                }
                else
                    TRACE_E("CFilesWindow::Unpack(): too long path to add archive name!");
            }
        }
        if (unpackMask != NULL)
            lstrcpyn(mask, unpackMask, MAX_PATH);
        else
            strcpy(mask, "*.*");
        CTruncatedString str;
        str.Set(LoadStr(IDS_UNPACKARCHIVE), fileName);

        if (pluginIndex != -1)
        {
            int i2;
            for (i2 = 0; i2 < UnpackerConfig.GetUnpackersCount(); i2++)
            {
                if (UnpackerConfig.GetUnpackerType(i2) == -pluginIndex - 1)
                {
                    UnpackerConfig.SetPreferedUnpacker(i2);
                    break;
                }
            }
            if (i2 == UnpackerConfig.GetUnpackersCount()) // The requested plugin was not found
            {
                sprintf(subject, LoadStr(IDS_PLUGINUNPACKERNOTFOUND), pluginName);
                SalMessageBox(HWindow, subject, LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;
            }
        }
        else // select unpacker based on extension
        {
            CMaskGroup tmpmask;
            if (UnpackerConfig.GetPreferedUnpacker() == -1)
            { // if there is no preferred one, we will select the first one (so that users don't stare at an empty combo)
                UnpackerConfig.SetPreferedUnpacker(0);
            }
            if (UnpackerConfig.GetPreferedUnpacker() != -1)
            {
                tmpmask.SetMasksString(UnpackerConfig.GetUnpackerExt(UnpackerConfig.GetPreferedUnpacker()), TRUE);
            }
            else
            {
                tmpmask.SetMasksString("", TRUE);
            }
            int errpos = 0;
            tmpmask.PrepareMasks(errpos);
            if (!tmpmask.AgreeMasks(file->Name, file->Ext))
            {
                int i2;
                for (i2 = 0; i2 < UnpackerConfig.GetUnpackersCount(); i2++)
                {
                    tmpmask.SetMasksString(UnpackerConfig.GetUnpackerExt(i2), TRUE);
                    tmpmask.PrepareMasks(errpos);
                    if (tmpmask.AgreeMasks(file->Name, file->Ext))
                    {
                        UnpackerConfig.SetPreferedUnpacker(i2);
                        break;
                    }
                }
            }
        }
        BOOL delArchiveWhenDone = FALSE;
    DO_AGAIN:

        if (CUnpackDialog(HWindow, path, pathAlt, mask, &str, &UnpackerConfig, &delArchiveWhenDone).Execute() == IDOK)
        {
            UpdateWindow(MainWindow->HWindow);
            //--- we will change the archive name to full-name
            int errTextID;
            char nextFocus[MAX_PATH];
            nextFocus[0] = 0;
            const char* text = NULL;
            if (!SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus))
            {
                if (errTextID == IDS_EMPTYNAMENOTALLOWED)
                    strcpy(path, GetPath());
                else
                    text = LoadStr(errTextID);
            }
            if (text == NULL)
            {
                int l = (int)strlen(GetPath());
                if (l > 0 && GetPath()[l - 1] == '\\')
                    l--;
                memcpy(subject, GetPath(), l);
                sprintf(subject + l, "\\%s", file->Name);
                char newDir[MAX_PATH];
                if (CheckAndCreateDirectory(path, NULL, TRUE, NULL, 0, newDir, FALSE, TRUE))
                {
                    // start the unpacker
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                    CDynamicStringImp archiveVolumes;
                    if (!UnpackerConfig.ExecuteUnpacker(MainWindow->HWindow, this, subject, mask,
                                                        path, delArchiveWhenDone,
                                                        delArchiveWhenDone ? &archiveVolumes : NULL))
                    {
                        if (newDir[0] != 0)
                            RemoveEmptyDirs(newDir);
                    }
                    else // Unpacking was successful (no Cancel, Skip might have been)
                    {
                        if (delArchiveWhenDone && archiveVolumes.Length > 0)
                        {
                            char* name = archiveVolumes.Text;
                            BOOL skipAll = FALSE;
                            do
                            {
                                if (*name != 0)
                                {
                                    while (1)
                                    {
                                        ClearReadOnlyAttr(name); // to be able to delete even read-only...
                                        if (!DeleteFile(name) && !skipAll)
                                        {
                                            DWORD err = GetLastError();
                                            if (err == ERROR_FILE_NOT_FOUND)
                                                break; // if the user managed to delete the file themselves, everything is OK
                                            int res = (int)CFileErrorDlg(MainWindow->HWindow, LoadStr(IDS_ERRORDELETINGFILE), name, GetErrorText(err)).Execute();
                                            if (res == IDB_SKIPALL)
                                                skipAll = TRUE;
                                            if (res == IDB_SKIPALL || res == IDB_SKIP)
                                                break;           // skip
                                            if (res == IDCANCEL) // cancel
                                            {
                                                name = archiveVolumes.Text + archiveVolumes.Length - 1;
                                                nextFocus[0] = 0; // Leave the cursor on the archive, do not jump to the directory with the unpacked archive
                                                break;
                                            }
                                            // IDRETRY will let us try another pass through the loop
                                        }
                                        else
                                            break; // deleted
                                    }
                                }
                                name = name + strlen(name) + 1;
                            } while (name - archiveVolumes.Text < archiveVolumes.Length);
                        }
                        if (nextFocus[0] != 0)
                        {
                            strcpy(NextFocusName, nextFocus);
                            PostMessage(HWindow, WM_USER_DONEXTFOCUS, 0, 0); // it's about the directory, must be
                        }
                    }
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    //--- refresh non-automatically refreshed directories
                    // Change in the directory where the archive is located (should not occur during unpacking, but
                    // let's refresh instead)
                    MainWindow->PostChangeOnPathNotification(GetPath(), FALSE);
                    if (newDir[0] != 0) // new subdirectories were being created on the way
                    {
                        CutDirectory(newDir); // should always go (path to the first newly created directory)
                        // changes in the directory where the first new subdirectory was created
                        MainWindow->PostChangeOnPathNotification(newDir, TRUE);
                    }
                    else
                    {
                        // changes in the directory where it was unpacked
                        MainWindow->PostChangeOnPathNotification(path, TRUE);
                    }
                }
                else
                {
                    if (newDir[0] != 0)
                    {
                        CutDirectory(newDir); // should always go (path to the first newly created directory)

                        //--- refresh non-automatically refreshed directories
                        // If directory creation failed, we will report changes immediately (user may
                        // choose a completely different path next time); the newly created path is not disturbed (almost dead-code)
                        MainWindow->PostChangeOnPathNotification(newDir, TRUE);
                    }
                    goto DO_AGAIN;
                }
            }
            else
            {
                SalMessageBox(HWindow, text, LoadStr(IDS_ERRORUNPACK), MB_OK | MB_ICONEXCLAMATION);
                goto DO_AGAIN;
            }
        }
        UpdateWindow(MainWindow->HWindow);

        //--- if any Total Commander window is active, exit suspend mode
        EndStopRefresh();
    }
}

// countSizeMode - 0 normal calculation, 1 calculation for selected item, 2 calculation for all subdirectories
void CFilesWindow::CalculateOccupiedZIPSpace(int countSizeMode)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CalculateOccupiedZIPSpace(%d)", countSizeMode);
    if (Is(ptZIPArchive) && (ValidFileData & VALID_DATA_SIZE)) // only if CFileData::Size is valid (sizes defined via plugin-data are highly unlikely for archives, so we are not addressing them in this function for now...)
    {
        BeginStopRefresh(); // cmuchal will wait

        TDirectArray<CQuadWord> sizes(200, 400);
        CQuadWord totalSize(0, 0); // calculated size
        int files = 0;
        int dirs = 0;

        int selIndex = -1;
        BOOL upDir;
        if (Dirs->Count > 0)
            upDir = (strcmp(Dirs->At(0).Name, "..") == 0);
        else
            upDir = FALSE;
        int count = GetSelCount();
        if (countSizeMode == 0 && count != 0 || countSizeMode == 2) // valid label
        {
            if (countSizeMode == 2)
                count = Dirs->Count - (upDir ? 1 : 0);
            int* indexes = new int[count];
            if (indexes == NULL)
            {
                TRACE_E(LOW_MEMORY);
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;
            }
            else
            {
                if (countSizeMode == 2)
                {
                    int i = (upDir ? 1 : 0);
                    int j;
                    for (j = 0; j < count; j++)
                        indexes[j] = i++;
                }
                else
                    GetSelItems(count, indexes);
                int i = count;
                while (i--)
                {
                    CFileData* f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);
                    if (indexes[i] < Dirs->Count)
                    {
                        f->SizeValid = 1;
                        f->Size = GetArchiveDir()->GetDirSize(GetZIPPath(), f->Name, &dirs, &files, &sizes);
                        totalSize += f->Size;
                        dirs++;
                    }
                    else
                    {
                        sizes.Add(f->Size); // error handling is handled up to the output dialog level
                        totalSize += f->Size;
                        files++;
                    }
                }
            }
            delete[] (indexes);
        }
        else // we are taking the selected file or directory
        {
            selIndex = GetCaretIndex();
            if (upDir && selIndex == 0)
            {
                EndStopRefresh(); // now he's sniffling again, he'll start up
                return;           // nothing to do
            }
            else
            {
                if (countSizeMode == 0)
                {
                    SetSel(TRUE, selIndex);
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                }
                CFileData* f = (selIndex < Dirs->Count) ? &Dirs->At(selIndex) : &Files->At(selIndex - Dirs->Count);
                if (selIndex < Dirs->Count)
                {
                    f->SizeValid = 1;
                    f->Size = GetArchiveDir()->GetDirSize(GetZIPPath(), f->Name, &dirs, &files, &sizes);
                    totalSize += f->Size;
                    dirs++;
                }
                else
                {
                    sizes.Add(f->Size); // error handling is handled up to the output dialog level
                    totalSize += f->Size;
                    files++;
                }
            }
        }

        // we advise if we have counted over more than one marked directory or over all
        if (((countSizeMode == 0 && dirs > 1) || countSizeMode == 2) && SortType == stSize)
        {
            ChangeSortType(stSize, FALSE, TRUE);
        }
        RefreshListBox(-1, -1, FocusedIndex, FALSE, FALSE); // conversion of column widths
        if (countSizeMode == 0)
        {
            CSizeResultsDlg(HWindow, totalSize, CQuadWord(-1, -1), CQuadWord(-1, -1),
                            files, dirs, &sizes)
                .Execute();
            //      CZIPSizeResultsDlg(HWindow, totalSize, files, dirs).Execute();
        }
        if (selIndex != -1 && countSizeMode == 0)
        {
            SetSel(FALSE, selIndex);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // now he's sniffling again, he'll start up
    }
}

void CFilesWindow::AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE3("CFilesWindow::AcceptChangeOnPathNotification(%s, %d)",
                        path, includingSubdirs);

    BOOL refresh = FALSE;
    if ((Is(ptDisk) || Is(ptZIPArchive)) && (!AutomaticRefresh || GetNetworkDrive()))
    {
        // we will test the equality of paths or at least their prefixes (we are only interested in disk paths,
        // FS paths in 'path' are automatically excluded because they can never match GetPath()
        char path1[MAX_PATH];
        char path2[MAX_PATH];
        lstrcpyn(path1, path, MAX_PATH);
        lstrcpyn(path2, GetPath(), MAX_PATH); // there is a path to the archive at the archive
        SalPathRemoveBackslash(path1);
        SalPathRemoveBackslash(path2);
        int len1 = (int)strlen(path1);
        refresh = !includingSubdirs && StrICmp(path1, path2) == 0 ||       // exact match
                  includingSubdirs && StrNICmp(path1, path2, len1) == 0 && // prefix match
                      (path2[len1] == 0 || path2[len1] == '\\');
        if (Is(ptDisk) && !refresh && CutDirectory(path1)) // It doesn't make sense near the archive
        {
            SalPathRemoveBackslash(path1);
            // on NTFS, even the date of the last subdirectory in the path changes (unfortunately only after entering
            // into this subdirectory, but it may need to be fixed over time, so we will proactively refresh it)
            refresh = StrICmp(path1, path2) == 0;
        }
        if (refresh)
        {
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty()) // send notification to FS
        {
            // Section EnterPlugin+LeavePlugin must be exported up to here (not in the encapsulation of interfaces)
            EnterPlugin();
            GetPluginFS()->AcceptChangeOnPathNotification(GetPluginFS()->GetPluginFSName(), path, includingSubdirs);
            LeavePlugin();
        }
    }

    if (Is(ptDisk) && !refresh &&           // only disks have free space (archives do not and FS is handled elsewhere)
        HasTheSameRootPath(path, GetPath()) // same root -> possibly changing the amount of free disk space

        /* && (!AutomaticRefresh ||  // commented out because when changes occur in subdirectories on auto-refreshed path, notifications do not arrive and thus the information about free space remained invalid
       !IsTheSamePath(path, GetPath()))*/
        ) // this path is not monitored for changes -> refresh definitely won't come
    {
        RefreshDiskFreeSpace(TRUE, TRUE);
    }
}

void CFilesWindow::IconOverlaysChangedOnPath(const char* path)
{
    //  if ((int)(GetTickCount() - NextIconOvrRefreshTime) < 0)
    //    TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: skipping notification for: " << path);
    if ((int)(GetTickCount() - NextIconOvrRefreshTime) >= 0 &&             // refresh icon-overlays will occur at the time of NextIconOvrRefreshTime, before this moment it does not make sense to track changes
        !IconOvrRefreshTimerSet && !NeedIconOvrRefreshAfterIconsReading && // refresh icon-overlays is not yet planned
        Configuration.EnableCustomIconOverlays && Is(ptDisk) &&
        (UseSystemIcons || UseThumbnails) && IconCache != NULL &&
        IsTheSamePath(path, GetPath()))
    {
        DWORD elapsed = GetTickCount() - LastIconOvrRefreshTime;
        if (elapsed < ICONOVR_REFRESH_PERIOD) // We will wait before the next refresh of the icon overlay so that we don't do it too often
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: setting timer for refresh");
            if (SetTimer(HWindow, IDT_ICONOVRREFRESH, max(200, ICONOVR_REFRESH_PERIOD - elapsed), NULL))
            {
                IconOvrRefreshTimerSet = TRUE;
                return;
            }
            // if there is a timer error, we will try to refresh immediately...
        }
        // refresh try to perform immediately (do not come too soon after the previous)
        if (!IconCacheValid) // we will execute it after the icons finish loading (they may, but do not have to be loaded correctly)
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: delaying refresh till end of reading of icons");
            NeedIconOvrRefreshAfterIconsReading = TRUE;
        }
        else // refresh immediately
        {
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: doing refresh: sleeping icon reader");
            SleepIconCacheThread();
            WaitOneTimeBeforeReadingIcons = 200; // po tuto dobu ceka icon-reader pred zahajenim nacitani icon-overlays, takze dalsi notifikace pro tento panel od Tortoise SVN, ktere prijdou v nasledujicich 200ms neni potreba resit...
            LastIconOvrRefreshTime = GetTickCount();
            NextIconOvrRefreshTime = LastIconOvrRefreshTime + WaitOneTimeBeforeReadingIcons;
            WakeupIconCacheThread();
            // TRACE_I("CFilesWindow::IconOverlaysChangedOnPath: doing refresh: icon reader is awake again");
        }
    }
}
