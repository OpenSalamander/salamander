// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

//
// ****************************************************************************
// CPluginFSInterface
//

#define FILE_ATTRIBUTES_MASK (FILE_ATTRIBUTE_ARCHIVE | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_SYSTEM)

CPluginFSInterface::CPluginFSInterface()
{
    Path[0] = 0;
    PathError = FALSE;
    FatalError = FALSE;
}

void WINAPI
CPluginFSInterface::ReleaseObject(HWND parent)
{
    // if the FS is initialized, remove our copies of files in the disk cache when closing
    if (Path[0] != 0)
        EmptyCache();
}

BOOL WINAPI
CPluginFSInterface::GetRootPath(char* userPart)
{
    userPart[0] = '\\';
    userPart[1] = 0;
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
    lstrcpyn(buf, Path, bufSize); // if the path does not fit, the name certainly will not either (an error will be reported)
    if (isDir == 2)
        return SalamanderGeneral->CutDirectory(buf, NULL); // up-dir
    else
        return CRAPI::PathAppend(buf, file.Name, bufSize);
}

BOOL WINAPI
CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    if (Path[0] == 0)
        return FALSE; // translation is not possible, let Salamander report the error itself

    char root[MAX_PATH] = "\\"; //JR in Windows Mobile the root is always "\\"

    if (*path != '\\')
        strcpy(root, Path); // paths such as "path" inherit the current FS path

    success = CRAPI::PathAppend(root, path, MAX_PATH);
    if (success && (int)strlen(root) < 1) // shorter than the root is impossible (it would become a relative path again)
    {
        success = SalamanderGeneral->SalPathAddBackslash(root, MAX_PATH);
    }
    if (success)
        success = (int)(strlen(root) + strlen(fsName) + 1) < pathSize; // does it fit?
    if (success)
        sprintf(path, "%s:%s", fsName, root);
    else
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_PATHTOOLONG),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
    }
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return SalamanderGeneral->IsTheSamePath(Path, userPart);
}

BOOL WINAPI
CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    return TRUE; //JR REVIEW: Who else would it belong to?
}

BOOL WINAPI
CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                               const char* userPart, char* cutFileName, BOOL* pathWasCut,
                               BOOL forceRefresh, int mode)
{
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
        return FALSE; // ListCurrentPath failed due to memory, fatal error
    }

    if (forceRefresh)
        EmptyCache();

    char buf[2 * MAX_PATH + 100];
    char errBuf[MAX_PATH];
    errBuf[0] = 0;
    char path[MAX_PATH];
    int err = 0;

    lstrcpyn(path, userPart, MAX_PATH);

    BOOL fileNameAlreadyCut = FALSE;
    if (PathError) // error while listing the path (the user already saw the error in ListCurrentPath)
    {              // try to trim the path
        PathError = FALSE;
        if (!SalamanderGeneral->CutDirectory(path, NULL))
            return FALSE; // nowhere to shorten, fatal error
        fileNameAlreadyCut = TRUE;
        if (pathWasCut != NULL)
            *pathWasCut = TRUE;
    }
    while (1)
    {
        DWORD attr = CRAPI::GetFileAttributes(path, TRUE);
        if (attr != 0xFFFFFFFF && (attr & FILE_ATTRIBUTE_DIRECTORY) != 0) // success, use the path as current
        {
            if (errBuf[0] != 0) // if we have a message, print it here (it arose during trimming)
            {
                sprintf(buf, LoadStr(IDS_PATH_ERROR), userPart, errBuf);
                SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
            }
            strcpy(Path, path);
            return TRUE;
        }
        else // failure, try to shorten the path
        {
            err = CRAPI::GetLastError();

            if (mode != 3 && attr != 0xFFFFFFFF || // a file instead of a path -> report as an error
                mode != 1 && attr == 0xFFFFFFFF)   // non-existent path -> report as an error
            {
                if (attr != 0xFFFFFFFF)
                {
                    sprintf(errBuf, LoadStr(IDS_ERR_FILEINPATH));
                }
                else
                    SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);

                // if opening the FS is time-consuming and we want to adjust Change Directory (Shift+F7)
                // to behave like archives, comment out the following line with "break" for mode 3

                //JR try trimming only if RAPI reports that the path does not exist
                if (mode == 3 || err != ERROR_FILE_NOT_FOUND)
                    break;
            }

            char* cut;
            if (!SalamanderGeneral->CutDirectory(path, &cut)) // nowhere to shorten, fatal error
            {
                SalamanderGeneral->GetErrorText(err, errBuf, MAX_PATH);
                break;
            }
            else
            {
                if (pathWasCut != NULL)
                    *pathWasCut = TRUE;
                if (!fileNameAlreadyCut) // it can be a file name only during the first trim
                {
                    fileNameAlreadyCut = TRUE;
                    if (cutFileName != NULL && attr != 0xFFFFFFFF) // it is a file
                        lstrcpyn(cutFileName, cut, MAX_PATH);
                }
                else
                {
                    if (cutFileName != NULL)
                        *cutFileName = 0; // it can no longer be a file name
                }
            }
        }
    }

    sprintf(buf, LoadStr(IDS_PATH_ERROR), userPart, errBuf);
    SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
    PathError = FALSE;
    return FALSE; // fatal path error
}

BOOL WINAPI
CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData,
                                    int& iconsType, BOOL forceRefresh)
{
    if (forceRefresh)
        EmptyCache();

    CFileData file;

    iconsType = pitFromRegistry;

    char buf[2 * MAX_PATH + 100];
    char curPath[MAX_PATH + 4];
    strcpy(curPath, Path);
    CRAPI::PathAppend(curPath, "*", MAX_PATH + 4);
    char* name = curPath + strlen(curPath) - 3;

    DWORD count;
    RapiNS::LPCE_FIND_DATA pFindDataArray;
    if (!CRAPI::FindAllFiles(curPath, FAF_NAME | FAF_ATTRIBUTES | FAF_SIZE_LOW | FAF_SIZE_HIGH | FAF_LASTWRITE_TIME,
                             &count, &pFindDataArray, TRUE))
    {
        DWORD err = CRAPI::GetLastError();
        sprintf(buf, LoadStr(IDS_PATH_ERROR), Path, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
        PathError = TRUE;
        return FALSE;
    }

    DWORD i = 0;

    if (strcmp(Path, "\\") != 0) //JR If we are not at the root, add the ".." entry
    {
        file.Name = SalamanderGeneral->DupStr("..");
        if (file.Name == NULL)
            goto ONERROR;
        file.NameLen = 2;
        file.Ext = file.Name + file.NameLen;
        file.Size = CQuadWord(0, 0);
        file.Attr = FILE_ATTRIBUTE_DIRECTORY;
        file.LastWrite.dwLowDateTime = 0;
        file.LastWrite.dwHighDateTime = 0;
        file.Hidden = 0;
        file.DosName = NULL; // Windows Mobile has no 8.3 file names
        file.IsLink = 0;
        file.IsOffline = 0;

        if (!dir->AddDir(NULL, file, NULL))
            goto ONERROR;
    }

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    for (; i < count; i++)
    {
        RapiNS::CE_FIND_DATA& data = pFindDataArray[i];

        if (data.cFileName[0] != 0 &&
            (data.cFileName[0] != '.' || //JR Windows Mobile does not return "." or ".." paths, but handle it just in case
             (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
        {
            char cFileName[MAX_PATH];
            WideCharToMultiByte(CP_ACP, 0, data.cFileName, -1, cFileName, MAX_PATH, NULL, NULL);
            cFileName[MAX_PATH - 1] = 0;

            file.Name = SalamanderGeneral->DupStr(cFileName);
            if (file.Name == NULL)
                goto ONERROR;
            file.NameLen = strlen(file.Name);
            if (!sortByExtDirsAsFiles && (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                file.Ext = file.Name + file.NameLen; // directories have no extensions
            }
            else
            {
                char* s;
                s = strrchr(file.Name, '.');
                if (s != NULL)
                    file.Ext = s + 1; // ".cvspass" is treated as an extension in Windows
                else
                    file.Ext = file.Name + file.NameLen;
            }
            file.Size = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
            file.Attr = data.dwFileAttributes;
            file.Attr &= ~(FILE_ATTRIBUTE_COMPRESSED);

            file.LastWrite = data.ftLastWriteTime;
            file.Hidden = (file.Attr & FILE_ATTRIBUTE_HIDDEN) != 0;
            file.DosName = NULL; // Windows Mobile has no 8.3 file names
            file.IsOffline = 0;
            if (file.Attr & FILE_ATTRIBUTE_DIRECTORY)
                file.IsLink = 0;
            else
                file.IsLink = SalamanderGeneral->IsFileLink(file.Ext);

            if ((file.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && !dir->AddFile(NULL, file, NULL) ||
                (file.Attr & FILE_ATTRIBUTE_DIRECTORY) != 0 && !dir->AddDir(NULL, file, NULL))
            {
                goto ONERROR;
            }
        }
    }

    CRAPI::FreeBuffer(pFindDataArray);
    return TRUE;

ONERROR:
    TRACE_E("Low memory");
    if (file.Name != NULL)
        SalamanderGeneral->Free(file.Name);
    CRAPI::FreeBuffer(pFindDataArray);

    FatalError = TRUE;
    return FALSE;
}

BOOL WINAPI
CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    //JR The Windows Mobile plugin always disconnects
    detach = FALSE;
    return TRUE;
}

void WINAPI
CPluginFSInterface::Event(int event, DWORD param)
{
    switch (event)
    {
    case FSE_ACTIVATEREFRESH: // user activated Salamander (switched from another application)
        if (!CRAPI::CheckConnection())
        {
            int panel1 = param;
            int panel2 = (panel1 == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);

            if (CRAPI::ReInit())
            {
                SalamanderGeneral->PostRefreshPanelPath(panel1);
                if (SalamanderGeneral->GetPanelPluginFS(panel2) != NULL)
                    SalamanderGeneral->PostRefreshPanelPath(panel2);
            }
            else if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_YESNO_CONNETCLOSEPLUGIN), TitleWMobileQuestion,
                                                       MSGBOX_QUESTION) == IDYES)
            {
                SalamanderGeneral->DisconnectFSFromPanel(SalamanderGeneral->GetMainWindowHWND(), panel1);
                if (SalamanderGeneral->GetPanelPluginFS(panel2) != NULL)
                    SalamanderGeneral->DisconnectFSFromPanel(SalamanderGeneral->GetMainWindowHWND(), panel2);
            }
        }
        break;
    case FSE_CLOSEORDETACHCANCELED:
    case FSE_OPENED:
    case FSE_ATTACHED:
    case FSE_DETACHED:
        // no operation
        break;
    }
}

DWORD WINAPI
CPluginFSInterface::GetSupportedServices()
{
    return 0 |
           FS_SERVICE_CONTEXTMENU |
           //JR TODO: add FS_SERVICE_SHOWPROPERTIES |
           FS_SERVICE_CHANGEATTRS |
           FS_SERVICE_COPYFROMDISKTOFS |
           FS_SERVICE_MOVEFROMDISKTOFS |
           FS_SERVICE_MOVEFROMFS |
           FS_SERVICE_COPYFROMFS |
           FS_SERVICE_DELETE |
           FS_SERVICE_VIEWFILE |
           //JR TODO: FS_SERVICE_EDITFILE (not yet supported in Salamander) |
           FS_SERVICE_CREATEDIR |
           FS_SERVICE_ACCEPTSCHANGENOTIF |
           FS_SERVICE_QUICKRENAME |
           //JR TODO: FS_SERVICE_CALCULATEOCCUPIEDSPACE (not yet supported in Salamander) |
           FS_SERVICE_COMMANDLINE |
           //JR TODO: add FS_SERVICE_SHOWINFO |
           FS_SERVICE_GETFREESPACE |
           FS_SERVICE_GETFSICON |
           FS_SERVICE_GETNEXTDIRLINEHOTPATH;
    //         FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM;
}

/*
BOOL WINAPI
CPluginFSInterface::GetChangeDriveOrDisconnectItem(const char *fsName, char *&title, HICON &icon, BOOL &destroyIcon)
{
  char txt[2 * MAX_PATH + 102];
  // the text will be the FS path (Salamander format)
  txt[0] = '\t';
  strcpy(txt + 1, fsName);
  sprintf(txt + strlen(txt), ":%s\t", Path);
  // duplicate '&' characters so the path prints correctly
  SalamanderGeneral->DuplicateAmpersands(txt, 2 * MAX_PATH + 102);

  // append the free-space information
  CQuadWord space;
  GetFSFreeSpace(&space);
  if (space != CQuadWord(-1, -1)) SalamanderGeneral->PrintDiskSize(txt + strlen(txt), space, 0);

  title = SalamanderGeneral->DupStr(txt);
  if (title == NULL) return FALSE;  // low memory: no item will be added

  icon = GetFSIcon(destroyIcon);
  return TRUE;
}

*/

HICON WINAPI
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    destroyIcon = TRUE;
    // return LoadIcon(DLLInstance, MAKEINTRESOURCE(IDI_FS)); // colors glitch in Alt+F1/F2: As Other Panel
    return (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_FS),
                            IMAGE_ICON, 16, 16, SalamanderGeneral->GetIconLRFlags());
}

void WINAPI
CPluginFSInterface::GetFSFreeSpace(CQuadWord* retValue)
{
    retValue->LoDWord = -1;
    retValue->HiDWord = -1;

    RapiNS::STORE_INFORMATION si;
    if (Path[0] != 0 && CRAPI::GetStoreInformation(&si))
    {
        retValue->LoDWord = si.dwFreeSize;
        retValue->HiDWord = 0;
    }
}

BOOL WINAPI
CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    const char* root = text; // pointer to the position after the root path

    while (*root != 0 && *root != ':')
        root++; //JR Skip 'FSNAME'
    if (*root == ':')
        root++; //JR Skip ':'
    if (*root == '\\')
        root++; //JR Skip '\\'

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
}

BOOL WINAPI
CPluginFSInterface::ExecuteCommandLine(HWND parent, char* command, int& selFrom, int& selTo)
{
    //JR First try the command in the current path if an absolute path is not provided
    char commandLine[MAX_PATH]; // CRAPI::CreateProcess cannot handle paths longer than MAX_PATH

    if (*command != '\\')
    {
        lstrcpyn(commandLine, Path, MAX_PATH);
        if (!CRAPI::PathAppend(commandLine, command, MAX_PATH))
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
    }

    if (*command == '\\' || !CRAPI::CreateProcess(commandLine, NULL))
    {
        // On failure, retry without the path, using only the user input
        if (lstrlen(command) >= MAX_PATH)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        if (!CRAPI::CreateProcess(command, NULL))
        {
            DWORD err = CRAPI::GetLastError();
            SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);

            return FALSE;
        }
    }

    *command = 0;
    return TRUE;
}

BOOL WINAPI
CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // request for the standard dialog

    // Verify the provided name syntactically
    char* s = newName;
    char buf[2 * MAX_PATH];
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (newName[0] == 0 || *s != 0)
    {
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(ERROR_INVALID_NAME),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        return FALSE; // invalid name; let the user correct it
    }

    // process the mask in newName
    SalamanderGeneral->MaskName(buf, 2 * MAX_PATH, file.Name, newName);
    lstrcpyn(newName, buf, MAX_PATH);

    // perform the rename operation
    char nameFrom[MAX_PATH];
    char nameTo[MAX_PATH];
    strcpy(nameFrom, Path);
    strcpy(nameTo, Path);
    if (!CRAPI::PathAppend(nameFrom, file.Name, MAX_PATH) ||
        !CRAPI::PathAppend(nameTo, newName, MAX_PATH))
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the mask adjustment
        return FALSE; // error -> show the standard dialog again
    }

    //JR TODO: ConfirmOverwrite + Delete

    if (!CRAPI::MoveFile(nameFrom, nameTo))
    {
        // potential overwrites are not handled here; treat them as errors as well
        DWORD err = CRAPI::GetLastError();
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the mask adjustment
        return FALSE; // error -> show the standard dialog again
    }
    else // operation succeeded - report the change on the path (triggers refresh) and return success
    {
        char cefsFileName[2 * MAX_PATH];
        if (SalamanderGeneral->StrICmp(nameFrom, nameTo) != 0)
        { // if it is more than just a case change (CEFS is case-insensitive)
            // remove the source file from the disk cache (the original name is no longer valid)
            sprintf(cefsFileName, "%s:%s", fsName, nameFrom);
            // disk names are case-insensitive while the disk cache is case-sensitive; converting
            // to lowercase makes the disk cache behave case-insensitively as well
            SalamanderGeneral->ToLowerCase(cefsFileName);
            SalamanderGeneral->RemoveOneFileFromCache(cefsFileName);
            // if overwriting is possible, the target should be removed from the disk cache as well ("file changed")
        }

        // change notification on Path (without subdirectories when renaming files)
        sprintf(cefsFileName, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(cefsFileName, isDir);

        return TRUE;
    }
}

void WINAPI
CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{

    // test whether the paths or at least their prefixes match (only paths on our FS have a chance;
    // disk paths and other FS paths in 'path' are excluded automatically because they can never
    // match 'fsName'+':' at the start of 'path2' below)
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
        SalamanderGeneral->PostRefreshPanelFS(this); // refresh the panel if the FS is visible there
}

BOOL WINAPI
CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    cancel = FALSE;
    if (mode == 1)
        return FALSE; // request for the standard dialog

    int type;
    BOOL isDir;
    char* secondPart;
    char nextFocus[MAX_PATH];
    char path[2 * MAX_PATH];
    int error;
    nextFocus[0] = 0;
    if (!SalamanderGeneral->SalParsePath(parent, newName, type, isDir, secondPart,
                                         TitleWMobileError, nextFocus,
                                         FALSE, NULL, NULL, &error, 2 * MAX_PATH))
    {
        if (error == SPP_EMPTYPATHNOTALLOWED) // empty string -> stop without performing the operation
        {
            cancel = TRUE;
            return TRUE; // return value no longer matters
        }

        if (error == SPP_INCOMLETEPATH) // relative FS path; build the absolute path manually
        {
            int errTextID;
            if (!SalamanderGeneral->SalGetFullName(newName, &errTextID, Path, nextFocus))
            {
                char errBuf[MAX_PATH];
                errBuf[0] = 0;
                SalamanderGeneral->GetGFNErrorText(errTextID, errBuf, MAX_PATH);
                SalamanderGeneral->ShowMessageBox(errBuf, TitleWMobileError, MSGBOX_ERROR);
                return FALSE;
            }

            strcpy(path, fsName);
            strcat(path, ":");

            if (strlen(fsName) + strlen(newName) + 1 >= 2 * MAX_PATH)
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                 TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
                // 'newName' is returned in its original form
                return FALSE; // error -> show the standard dialog again
            }
            else
                strcat(path, newName);

            strcpy(newName, path);
            secondPart = newName + strlen(fsName) + 1;
            type = PATH_TYPE_FS;
        }
        else
            return FALSE; // error -> show the standard dialog again
    }

    if (type != PATH_TYPE_FS)
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR1),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the path expansion
        return FALSE; // error -> show the standard dialog again
    }

    if ((secondPart - newName) - 1 != (int)strlen(fsName) ||
        SalamanderGeneral->StrNICmp(newName, fsName, (int)(secondPart - newName) - 1) != 0)
    { // not a CEFS path
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR2),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the path expansion
        return FALSE; // error -> show the standard dialog again
    }

    if (secondPart[0] != '\\')
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_SORRY_CREATEDIR3),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the path expansion
        return FALSE; // error -> show the standard dialog again
    }

    // remove any "." and ".." segments from the full path to this FS
    if (!SalamanderGeneral->SalRemovePointsFromPath(secondPart + 1))
    {
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                         TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is returned after expanding the path and possibly adjusting ".." and "."
        return FALSE; // error -> show the standard dialog again
    }

    // trim any redundant trailing backslash
    SalamanderGeneral->SalPathRemoveBackslash(secondPart);

    // finally create the directory

    if (!CRAPI::CreateDirectory(secondPart, NULL))
    {
        DWORD err = CRAPI::GetLastError();
        SalamanderGeneral->SalMessageBox(parent, SalamanderGeneral->GetErrorText(err),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'newName' is already returned after the path expansion
        return FALSE; // error -> show the standard dialog again
    }

    // operation succeeded - report the change on the path (triggers refresh)
    // notify about the change on the path (without subdirectories)
    SalamanderGeneral->CutDirectory(secondPart); // must succeed (cannot be the root)
    sprintf(path, "%s:%s", fsName, secondPart);
    SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
    strcpy(newName, nextFocus); // if only the directory name was provided, focus it in the panel

    return TRUE;
}

void WINAPI
CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                             CSalamanderForViewFileOnFSAbstract* salamander,
                             CFileData& file)
{
    if (!CRAPI::CheckConnection())
        CRAPI::ReInit();

    // build a unique file name for the disk cache (standard Salamander path format)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, AssignedFSName);
    strcat(uniqueFileName, ":");
    strcat(uniqueFileName, Path);
    CRAPI::PathAppend(uniqueFileName + strlen(AssignedFSName) + 1, file.Name, MAX_PATH);
    // disk names are case-insensitive while the disk cache is case-sensitive; converting
    // to lowercase makes the disk cache behave case-insensitively as well
    SalamanderGeneral->ToLowerCase(uniqueFileName);

    // obtain the disk-cache copy name
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName, file.Name, NULL, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // determine whether a disk-cache copy of the file needs to be prepared (download)
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (!fileExists) // preparing a copy (download) is necessary
    {
        const char* name = uniqueFileName + strlen(AssignedFSName) + 1;

        HWND mainWnd = parent;
        HWND parentWin;
        while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
            mainWnd = parentWin;
        // disable 'mainWnd'

        CProgressDlg dlg(mainWnd, LoadStr(IDS_READ), LoadStr(IDS_READING), ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack

        dlg.Create();
        EnableWindow(mainWnd, FALSE);
        SetForegroundWindow(dlg.HWindow);

        dlg.Set(name, 0, TRUE);

        LPCTSTR errFileName = "";
        DWORD err = CRAPI::CopyFileToPC(name, tmpFileName, TRUE, &dlg, 0, 0, &errFileName);

        EnableWindow(mainWnd, TRUE);
        DestroyWindow(dlg.HWindow); // close the progress dialog

        if (err == 0) // the copy succeeded
        {
            newFileOK = TRUE; // if the size query fails, newFileSize stays zero (not critical)
            HANDLE hFile = HANDLES_Q(CreateFile(tmpFileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                NULL, OPEN_EXISTING, 0, NULL));
            if (hFile != INVALID_HANDLE_VALUE)
            { // ignore errors; the exact file size is not essential
                DWORD err2;
                SalamanderGeneral->SalGetFileSize(hFile, newFileSize, err2); // ignore errors
                HANDLES(CloseHandle(hFile));
            }
        }
        else if (err != -1)
        {
            char buf[2 * MAX_PATH + 100];
            sprintf(buf, LoadStr(IDS_PATH_ERROR), errFileName, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
            return;
        }
    }

    // open the viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!fileExists && !newFileOK || // open the viewer only if the file copy is ready
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // on error, reset "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // call FreeFileNameInCache to pair with AllocFileNameInCache (link the viewer and the disk cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, FALSE);
}

BOOL WINAPI
CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                           int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    cancelOrError = FALSE;
    if (mode == 1)
        return FALSE; // request for the standard prompt

    char buf[2 * MAX_PATH]; // buffer for error messages

    char rootPath[MAX_PATH], fileName[MAX_PATH], dfsFileName[2 * MAX_PATH];

    strcpy(rootPath, Path);

    // retrieve the "Confirm on" settings from the configuration
    BOOL ConfirmOnNotEmptyDirDelete, ConfirmOnSystemHiddenFileDelete, ConfirmOnSystemHiddenDirDelete;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEDEL, &ConfirmOnSystemHiddenFileDelete, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHDIRDEL, &ConfirmOnSystemHiddenDirDelete, 4, NULL);

    BOOL skipAllSHFD = FALSE;   // skip all deletes of system or hidden files
    BOOL yesAllSHFD = FALSE;    // delete all system or hidden files
    BOOL skipAllSHDD = FALSE;   // skip all deletes of system or hidden dirs
    BOOL yesAllSHDD = FALSE;    // delete all system or hidden dirs
    BOOL skipAllErrors = FALSE; // skip all errors

    BOOL success = TRUE; // becomes FALSE on error or user interruption
    BOOL changeInSubdirs = FALSE;

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    //JR load all files that will be deleted
    for (int block1 = 1;; block1++)
    {
        // retrieve data about the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // delete the file/directory
        //JR call FindAllFilesInTree even for individual files
        //JR this verifies the file still exists and gets its current attributes
        if (f != NULL)
            success = CRAPI::FindAllFilesInTree(rootPath, f->Name, array, block1, FALSE);

        // decide whether to continue (stop if there is an error or no additional selected item)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    if (!success)
        return FALSE;

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disable 'mainWnd'

    BOOL showProgressDialog = array.Count > 1;
    BOOL enableMainWnd = TRUE;
    CProgressDlg delDlg(mainWnd, LoadStr(IDS_DELETE), LoadStr(IDS_DELETING), ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack

    if (showProgressDialog)
    {
        EnableWindow(mainWnd, FALSE);
        delDlg.Create();
    }

    if (!showProgressDialog || delDlg.HWindow != NULL) // dialog opened successfully
    {
        if (showProgressDialog)
            SetForegroundWindow(delDlg.HWindow);

        int block = 0;
        int i;
        for (i = 0; success && i < array.Count; i++)
        {
            CFileInfo& fi = array[i];

            strcpy(fileName, rootPath);
            if (!CRAPI::PathAppend(fileName, fi.cFileName, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                                  TitleWMobileError, MSGBOX_ERROR);
                success = FALSE;
                break;
            }

            if (showProgressDialog)
            {
                float progress = ((float)i / (float)array.Count);
                delDlg.Set(fileName, (DWORD)(progress * 1000), TRUE); // delayedPaint == TRUE so we don't slow things down
            }

            if (showProgressDialog && delDlg.GetWantCancel())
            {
                success = FALSE;
                break;
            }

            if (ConfirmOnNotEmptyDirDelete && i < array.Count - 1)
            {
                //JR if a block contains more than one item, it represents a non-empty top-level directory
                if (fi.block != block && fi.block == array[i + 1].block)
                {
                    //JR the last path in the block is the directory itself
                    int j;
                    for (j = i + 1; j < array.Count && array[j].block == fi.block; j++)
                    {
                    }
                    j--;

                    sprintf(buf, LoadStr(IDS_YESNO_DELETENOEMPTYDIR), array[j].cFileName);
                    int res = SalamanderGeneral->ShowMessageBox(buf, TitleWMobileQuestion, MSGBOX_EX_QUESTION);
                    if (res == IDNO)
                    {
                        i = j; //JR skip the rest of the block
                        continue;
                    }
                    else if (res != IDYES)
                    {
                        success = FALSE;
                        break;
                    }
                }
            }

            block = fi.block;

            if (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
            {
                BOOL skip = FALSE;
                if (ConfirmOnSystemHiddenDirDelete &&
                    (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllSHDD && !yesAllSHDD)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, fileName,
                                                                    LoadStr(IDS_YESNO_DELETEHIDDENDIR), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            yesAllSHDD = TRUE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllSHDD = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break; // DIALOG_CANCEL
                        }
                    }
                    else // skip all or delete all
                    {
                        if (skipAllSHDD)
                            skip = TRUE;
                    }
                }

                if (success && !skip) // neither canceled nor skipped
                {
                    skip = FALSE;
                    while (1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::RemoveDirectory(fileName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                            sprintf(dfsFileName, "%s:%s", fsName, fileName);
                            // disk names are case-insensitive while the disk cache is case-sensitive; converting
                            // to lowercase makes the disk cache behave case-insensitively as well
                            SalamanderGeneral->ToLowerCase(dfsFileName);
                            // remove the deleted file's cache copy if it exists
                            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
                            changeInSubdirs = TRUE; // changes may also have occurred in subdirectories
                            break;                  // successful delete
                        }
                        if (!success || skip)
                            break;
                    }
                }
            }
            else
            {
                BOOL skip = FALSE;
                if (ConfirmOnSystemHiddenFileDelete &&
                    (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllSHFD && !yesAllSHFD)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, fileName,
                                                                    LoadStr(IDS_YESNO_DELETEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            yesAllSHFD = TRUE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllSHFD = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break; // DIALOG_CANCEL
                        }
                    }
                    else // skip all or delete all
                    {
                        if (skipAllSHFD)
                            skip = TRUE;
                    }
                }

                if (success && !skip) // neither canceled nor skipped
                {
                    skip = FALSE;
                    while (1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(fileName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::DeleteFile(fileName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                            sprintf(dfsFileName, "%s:%s", fsName, fileName);
                            // disk names are case-insensitive while the disk cache is case-sensitive; converting
                            // to lowercase makes the disk cache behave case-insensitively as well
                            SalamanderGeneral->ToLowerCase(dfsFileName);
                            // remove the deleted file's cache copy if it exists
                            SalamanderGeneral->RemoveOneFileFromCache(dfsFileName);
                            break; // successful delete
                        }
                        if (!success || skip)
                            break;
                    }
                }
            }
        }

        if (showProgressDialog)
        {
            // enable 'mainWnd' (otherwise Windows cannot make it the foreground/active window)
            EnableWindow(mainWnd, TRUE);
            enableMainWnd = FALSE;

            DestroyWindow(delDlg.HWindow); // close the progress dialog
        }
    }

    // enable 'mainWnd' if the foreground window never changed (the progress dialog never opened)
    if (showProgressDialog && enableMainWnd)
        EnableWindow(mainWnd, TRUE);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    // report the change on Path (no subdirectories when only files were deleted)
    sprintf(dfsFileName, "%s:%s", fsName, Path);
    SalamanderGeneral->PostChangeOnPathNotification(dfsFileName, changeInSubdirs);

    return success;
}

static BOOL WINAPI CEFS_IsTheSamePath(const char* path1, const char* path2)
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

static void GetFileData(const char* name, char (&buf)[100])
{
    buf[0] = '?';
    buf[1] = 0;
    char tmp[50];

    WIN32_FIND_DATA data;

    HANDLE find = FindFirstFile(name, &data);
    if (find == INVALID_HANDLE_VALUE)
        return;

    CQuadWord size(data.nFileSizeLow, data.nFileSizeHigh);
    SalamanderGeneral->NumberToStr(buf, size);

    FILETIME time;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&data.ftLastWriteTime, &time);
    FileTimeToSystemTime(&time, &st);

    if (!GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, tmp, 50))
        sprintf(tmp, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);

    strcat(buf, ", ");
    strcat(buf, tmp);

    if (!GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, tmp, 50))
        sprintf(tmp, "%u:%u:%u", st.wHour, st.wMinute, st.wSecond);

    strcat(buf, ", ");
    strcat(buf, tmp);

    FindClose(find);
}

BOOL WINAPI
CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                     int panel, int selectedFiles, int selectedDirs,
                                     char* targetPath, BOOL& operationMask,
                                     BOOL& cancelOrHandlePath, HWND dropTarget)
{
    char path[2 * MAX_PATH];
    operationMask = FALSE;
    cancelOrHandlePath = FALSE;
    if (mode == 1) // first call to CopyOrMoveFromFS
    {
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
        // If a path was suggested, append the *.* mask (we will process operation masks)
        if (*targetPath != 0)
        {
            SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
            SalamanderGeneral->SetUserWorkedOnPanelPath(PANEL_TARGET); // default action = work with the path in the target panel
        }
        return FALSE; // request for the standard dialog
    }

    if (mode == 4) // error during the standard Salamander processing of the target path
    {
        // 'targetPath' contains an invalid path, the user has already been informed, so we just
        // let them edit the destination path again
        return FALSE; // request for the standard dialog
    }

    char buf[3 * MAX_PATH + 100];
    char nextFocus[MAX_PATH];
    nextFocus[0] = 0;

    BOOL diskPath = TRUE;  // when 'mode'==3 'targetPath' holds a Windows path (FALSE = path on this FS)
    char* userPart = NULL; // pointer into 'targetPath' to the user portion of the FS path (when 'diskPath' is FALSE)
    BOOL rename = FALSE;   // TRUE means renaming/copying a directory into itself

    if (mode == 2) // a string arrived from the standard dialog entered by the user
    {
        // Handle relative paths ourselves (Salamander cannot do that)
        if ((targetPath[0] != '\\' || targetPath[1] != '\\') && // not a UNC path
            (targetPath[0] == 0 || targetPath[1] != ':'))       // not a normal disk path
        {                                                       // neither a Windows path, nor an archive path
            userPart = strchr(targetPath, ':');
            if (userPart == NULL) // path does not contain an FS name, so it is relative
            {                     // a relative path with ':' is not allowed here (cannot be distinguished from an absolute path to some FS)

                // For disk paths we could use SalGetFullName:
                // SalamanderGeneral->SalGetFullName(targetPath, &errTextID, Path, nextFocus) + handle the errors
                // After that it would be enough to prepend the FS name to the obtained path
                // but instead we demonstrate our own implementation (using SalRemovePointsFromPath and others):

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
                int rootLen = 1;
                *s = '\\';
                if (targetPath[0] == '\\') // "\\path" -> compose root + newName
                {
                    s += rootLen;
                    int len = (int)strlen(targetPath + 1); // without the leading '\\'
                    if (len + rootLen >= MAX_PATH)
                        tooLong = TRUE;
                    else
                    {
                        memcpy(s, targetPath + 1, len);
                        *(s + len) = 0;
                    }
                }
                else // "path" -> compose Path + newName
                {
                    int pathLen = (int)strlen(Path);
                    if (pathLen < rootLen)
                        rootLen = pathLen;
                    strcpy(s + rootLen, Path + rootLen); // root was already copied there
                    tooLong = !CRAPI::PathAppend(s, targetPath, MAX_PATH);
                }

                if (tooLong)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' is returned unchanged (as entered by the user)
                    return FALSE; // error -> reopen the standard dialog
                }

                strcpy(targetPath, path);
                userPart = targetPath + (userPart - path);
            }
            else
                userPart++;

            // FS destination path ('targetPath' - full path, 'userPart' - pointer inside the full path to the user part)
            // At this point the plugin can handle FS paths (both its own and foreign ones)
            // Salamander cannot process these paths yet; in the future it might support a basic
            // sequence of operations via TEMP (for example download from FTP to TEMP, then upload
            // from TEMP back to FTP - if it can be done more efficiently, as with FTP, the plugin should handle it here)

            if ((userPart - targetPath) - 1 == (int)strlen(fsName) &&
                SalamanderGeneral->StrNICmp(targetPath, fsName, (int)(userPart - targetPath) - 1) == 0)
            { // it is CEFS (otherwise let Salamander process it normally)
                BOOL invPath = (userPart[0] != '\\');

                int rootLen = 0;
                if (!invPath)
                {
                    rootLen = 1;
                    int userPartLen = (int)strlen(userPart);
                    if (userPartLen < rootLen)
                        rootLen = userPartLen;
                }

                // The full path to this FS may also contain "." and ".." entered by the user - remove them
                if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
                {
                    // Additionally we could display 'err' (when 'invPath' is TRUE); ignored here for simplicity
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                    // 'targetPath' is returned after the modification (path expansion) + possible adjustment of some ".." and "."
                    return FALSE; // error -> reopen the standard dialog
                }

                // Trim the unnecessary backslash
                int l = (int)strlen(userPart);
                BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
                if (l > 1 && userPart[l - 1] == '\\') // path of the form "\path\"
                    userPart[l - 1] = 0;              // remove the trailing backslash

                // Analyse the path - locate the existing part, the missing part, and the operation mask
                //
                // - Determine which portion exists and whether it is a file or a directory,
                //   then choose what action applies:
                //   - write to the path (possibly with a missing portion) with a mask - the mask is the last missing part
                //     after which there is no backslash (verify that when there are multiple source files/directories the mask
                //     contains '*' or at least '?'; otherwise it is nonsense -> there would be only one target name)
                //   - manual "change-case" of a subdirectory name via Move (writing to a path that is simultaneously the
                //     source of the operation because it is focused/selected as the only item in the panel); the names can
                //     differ only by letter case)
                //   - writing into an archive (the path contains an archive file, or it might not even be an archive, resulting
                //     in the "Salamander does not know how to open this file" error)
                //   - overwriting a file (the entire path is just the target file name; must not end with a backslash)

                // Determine how far the path exists (split into existing and missing parts)
                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                char* end = targetPath + strlen(targetPath);
                char* afterRoot = userPart + 1; // JR root = '\\'
                char lastChar = 0;
                BOOL pathIsDir = TRUE;
                BOOL pathError = FALSE;

                // If the path contains a mask, cut it off without calling GetFileAttributes
                if (end > afterRoot) // still more than just the root
                {
                    char* end2 = end;
                    BOOL cut = FALSE;
                    while (*--end2 != '\\') // there is guaranteed to be at least one '\\' after the root path
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

                while (end > afterRoot) // still more than just the root
                {
                    DWORD attrs = CRAPI::GetFileAttributes(userPart);
                    if (attrs != 0xFFFFFFFF) // this portion of the path exists
                    {
                        if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // it is a file
                        {
                            // An existing path must not include a file name (see SalSplitGeneralPath); trim it...
                            *end = lastChar;   // restore 'targetPath'
                            pathIsDir = FALSE; // the existing part of the path is a file
                            while (*--end != '\\')
                                ;            // there is guaranteed to be at least one '\\' beyond the root path
                            lastChar = *end; // so the path remains valid
                            break;
                        }
                        else
                            break;
                    }
                    else
                    {
                        DWORD err = CRAPI::GetLastError();
                        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                            err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                            err != ERROR_DIRECTORY) // unexpected error - just report it
                        {
                            sprintf(buf, LoadStr(IDS_PATH_ERROR), targetPath, SalamanderGeneral->GetErrorText(err));
                            SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                            pathError = TRUE;
                            break; // report the error
                        }
                    }

                    *end = lastChar; // restore 'targetPath'
                    while (*--end != '\\')
                        ; // there is guaranteed to be at least one '\\' after the root path
                    lastChar = *end;
                    *end = 0;
                }
                *end = lastChar; // restore 'targetPath'
                SetCursor(oldCur);

                if (!pathError) // the split finished without errors
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

                    char newDirs[MAX_PATH], *mask;
                    if (SalamanderGeneral->SalSplitGeneralPath(parent, TitleWMobile, TitleWMobileError, selectedFiles + selectedDirs,
                                                               targetPath, afterRoot, end, pathIsDir,
                                                               backslashAtEnd, dirName, curPath, mask, newDirs,
                                                               CEFS_IsTheSamePath))
                    {
                        if (newDirs[0] != 0) // need to create some subdirectories on the target path
                        {
                            if (!CRAPI::CheckAndCreateDirectory(userPart, parent, true, NULL, 0, NULL))
                            {
                                char* e = targetPath + strlen(targetPath); // restore 'targetPath' (join 'targetPath' and 'mask')
                                if (e > targetPath && *(e - 1) != '\\')
                                    *e++ = '\\';
                                if (e != mask)
                                    memmove(e, mask, strlen(mask) + 1); // move the mask if needed
                                pathError = TRUE;
                            }
                        }
                        else if (dirName != NULL && curPath != NULL && SalamanderGeneral->StrICmp(dirName, mask) == 0 &&
                                 CEFS_IsTheSamePath(targetPath, curPath))
                        {
                            // Renaming/copying a directory into itself (differing only by letter case) - "change-case"
                            // cannot be treated as an operation mask (the specified target path exists; splitting it into a mask is
                            // the result of the analysis)

                            rename = TRUE;
                        }
                    }
                    else
                        pathError = TRUE;
                }

                if (pathError)
                {
                    // 'targetPath' is returned after the modification (path expansion) + the ".." and "." cleanup + the mask that may have been added
                    return FALSE; // error -> reopen the standard dialog
                }
                diskPath = FALSE; // path on this FS successfully analysed
            }
        }

        if (diskPath)
        {
            // Windows path, path into an archive, or to an unknown FS - let the standard processing handle it
            operationMask = TRUE; // operation masks are supported
            cancelOrHandlePath = TRUE;
            return FALSE; // let Salamander process the path
        }
    }

    const char* opMask = NULL; // operation mask
    if (mode == 5)             // operation target specified via drag&drop
    {
        // If it is a disk path, just set the operation mask and continue (same as with 'mode'==3);
        // if it is a path into an archive, throw a "not supported" error; for a CEFS path set
        // 'diskPath'=FALSE and compute 'userPart' (points to the user portion of the CEFS path); for
        // a path to another FS, throw a "not supported" error

        BOOL ok = FALSE;
        opMask = "*.*";
        int type;
        char* secondPart;
        BOOL isDir;
        if (targetPath[0] != 0 && targetPath[1] == ':' ||   // disk path (C:\path)
            targetPath[0] == '\\' && targetPath[1] == '\\') // UNC path (\\server\share\path)
        {                                                   // append a trailing backslash so it is always treated as a path (for 'mode'==5 it is always a path)
            SalamanderGeneral->SalPathAddBackslash(targetPath, MAX_PATH);
        }
        if (SalamanderGeneral->SalParsePath(parent, targetPath, type, isDir, secondPart,
                                            TitleWMobileError, NULL, FALSE,
                                            NULL, NULL, NULL, 2 * MAX_PATH))
        {
            switch (type)
            {
            case PATH_TYPE_WINDOWS:
            {
                if (*secondPart != 0)
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_TARGETPATHNOEXISTS),
                                                     TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
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
                { // je to CEFS
                    diskPath = FALSE;
                    ok = TRUE;
                }
                else // different FS, just report "not supported"
                {
                    SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_COPYTOOTHERFS), TitleWMobileError,
                                                     MB_OK | MB_ICONEXCLAMATION);
                }
                break;
            }

            //case PATH_TYPE_ARCHIVE:
            default: // archive, just report "not supported"
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_COPYTOARCHIVES),
                                                 TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
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

    // 'mode' is 2, 3 or 5

    // Determine the operation mask (the target path is in 'targetPath')
    if (opMask == NULL)
    {
        opMask = targetPath;
        while (*opMask != 0)
            opMask++;
        opMask++;
    }

    // Prepare buffers for names
    char sourceName[MAX_PATH]; // buffer for the full disk name (the source resides on disk for CEFS)
    strcpy(sourceName, Path);
    char* endSource = sourceName + strlen(sourceName); // space for names from the panel
    if (endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // maximum number of characters for a panel name

    char cefsSourceName[2 * MAX_PATH]; // buffer for the full CEFS name (for locating the operation source in the disk cache)
    sprintf(cefsSourceName, "%s:%s", fsName, sourceName);
    // Disk names are case-insensitive, the disk cache is case-sensitive; converting
    // to lowercase makes the disk cache behave case-insensitively as well
    SalamanderGeneral->ToLowerCase(cefsSourceName);
    char* endCEFSSource = cefsSourceName + strlen(cefsSourceName);                // space for names from the panel
    int endCEFSSourceSize = 2 * MAX_PATH - (int)(endCEFSSource - cefsSourceName); // maximum number of characters for a panel name

    char targetName[MAX_PATH]; // buffer for the full disk name (if the target resides on disk)
    targetName[0] = 0;
    char* endTarget = targetName;
    int endTargetSize = MAX_PATH;

    if (diskPath) // Windows target path
        strcpy(targetName, targetPath);
    else
        strcpy(targetName, userPart);

    endTarget = targetName + strlen(targetName); // space for the destination name
    if (endTarget > targetName && *(endTarget - 1) != '\\')
    {
        *endTarget++ = '\\';
        *endTarget = 0;
    }
    endTargetSize = MAX_PATH - (int)(endTarget - targetName); // maximum number of characters for a panel name

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;                     // FALSE in case of an error or user cancellation
    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE if the source path changed (move operation)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE if subdirectories of the source path changed
    BOOL targetPathChanged = FALSE;          // TRUE if the target path changed
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE if subdirectories of the target path changed
    BOOL skipAllOverwrite = FALSE;
    BOOL skipAllOverwriteSystemHidden = FALSE;

    rename = !copy && CEFS_IsTheSamePath(sourceName, targetName);

    // Retrieve the "Confirm on" values from the configuration
    BOOL ConfirmOnFileOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    while (1)
    {
        // Retrieve data about the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // Perform copy/move on the file/directory
        if (f != NULL)
        {
            if (rename)
            {
                CFileInfo fi;
                strcpy(fi.cFileName, f->Name);
                fi.dwFileAttributes = f->Attr;
                fi.size = 100; // JR Renaming takes roughly the same time regardless of size
                fi.block = 0;

                array.Add(fi);
                if (array.State != etNone)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                    TRACE_E("Low memory");
                    success = false;
                }
            }
            else
                success = CRAPI::FindAllFilesInTree(Path, f->Name, array, 0, TRUE);
        }

        // Determine whether it makes sense to continue (when not cancelled and another selected item exists)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // disablujeme 'mainWnd'

    CProgress2Dlg dlg(mainWnd, LoadStr(copy ? IDS_COPY : IDS_MOVE), LoadStr(copy ? IDS_COPYING : IDS_MOVING), LoadStr(IDS_TO), ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack

    dlg.Create();
    EnableWindow(mainWnd, FALSE);
    SetForegroundWindow(dlg.HWindow);

    INT64 totalsize = 0, copied = 0;
    int i;
    for (i = 0; i < array.Count; i++)
        totalsize += (array[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 100 : array[i].size;

    for (/*int*/ i = 0; i < array.Count; i++)
    {
        CFileInfo& fi = array[i];

        char* targetFile = SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, fi.cFileName, opMask);

        if ((int)strlen(fi.cFileName) >= endSourceSize || (int)strlen(targetFile) >= endTargetSize)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        lstrcpyn(endSource, fi.cFileName, endSourceSize);
        lstrcpyn(endCEFSSource, fi.cFileName, endCEFSSourceSize);
        // Disk names are case-insensitive, the disk cache is case-sensitive; converting
        // to lowercase makes the disk cache behave case-insensitively as well
        SalamanderGeneral->ToLowerCase(endCEFSSource);

        // Compose the target name - simplified without the LoadStr(IDS_ERR_NAMETOOLONG) error check
        lstrcpyn(endTarget, targetFile, endTargetSize);

        isDir = (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (copy && SalamanderGeneral->StrICmp(sourceName, targetName) == 0)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(isDir ? IDS_ERR_COPYDIRTOITSELF : IDS_ERR_COPYFILETOITSELF), TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        dlg.Set(sourceName, targetName, FALSE);

        BOOL skip = FALSE;
        BOOL fileMoved = FALSE;
        if (isDir && !rename)
        {
            if (fi.block == -1)
            {
                while (1)
                {
                    if (!(diskPath ? SalamanderGeneral->CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL) : CRAPI::CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL)))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = diskPath ? GetLastError() : CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, targetName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                        subdirsOfTargetPathChanged = TRUE;
                        break; // successfully created directory
                    }
                } // end of while(1)
            }
        }
        else
        {
            DWORD attr = 0xFFFFFFFF;

            if (!rename || SalamanderGeneral->StrICmp(sourceName, targetName) != 0) // Not a simple change in letter case
            {
                if (diskPath)
                    attr = SalamanderGeneral->SalGetFileAttributes(targetName);
                else
                    attr = CRAPI::GetFileAttributes(targetName);
            }

            if (attr != 0xFFFFFFFF)
            {
                if (!skipAllOverwrite)
                {
                    if (ConfirmOnFileOverwrite)
                    {
                        char sourceData[100], targetData[100];

                        CRAPI::GetFileData(sourceName, sourceData);
                        if (diskPath)
                            GetFileData(targetName, targetData);
                        else
                            CRAPI::GetFileData(targetName, targetData);

                        int res = SalamanderGeneral->DialogOverwrite(parent, BUTTONS_YESALLSKIPCANCEL, targetName, targetData, sourceName, sourceData);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllOverwrite = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break;
                        }
                    }
                }
                else
                    skip = TRUE;

                if (success && !skip && ConfirmOnSystemHiddenFileOverwrite && (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllOverwriteSystemHidden)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, targetName,
                                                                    LoadStr(IDS_YESNO_OVERWRITEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnSystemHiddenFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;

                        case DIALOG_SKIPALL:
                            skipAllOverwriteSystemHidden = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;

                        default:
                            success = FALSE;
                            break;
                        }
                    }
                    else
                        skip = TRUE;
                }
            }

            if (success && !skip)
            {
                while (1)
                {
                    DWORD err = 0;
                    LPCTSTR errFileName = "";
                    if (diskPath) // JR Windows destination path
                    {
                        if (attr != 0xFFFFFFFF &&
                            (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY)))
                            SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                        err = CRAPI::CopyFileToPC(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                    }
                    else
                    {
                        if (attr != 0xFFFFFFFF &&
                            (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY)))
                            CRAPI::SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                        if (copy)
                        {
                            err = CRAPI::CopyFile(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                        }
                        else
                        {
                            if (attr != 0xFFFFFFFF)
                                CRAPI::DeleteFile(targetName);

                            if (!CRAPI::MoveFile(sourceName, targetName))
                                err = CRAPI::GetLastError();
                            else if (dlg.GetWantCancel())
                                err = -1;
                            if (err == 0)
                                fileMoved = TRUE;
                        }
                    }

                    if (err != 0)
                    {
                        if (err == -1) // JR cancelled by the user
                            success = FALSE;
                        else if (!skipAllErrors)
                        {
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, errFileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                        if (fileMoved)
                            sourcePathChanged = TRUE;
                        break; // copied successfully
                    }

                    if (!success || skip)
                        break;
                } // end of while(1)
            }
        }

        if (success && !copy && !skip && !fileMoved) // it is a "move" and the file was not skipped -> delete the source file
        {
            while (1)
            {
                if (isDir)
                {
                    if (fi.block != -1)
                    {
                        if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                            CRAPI::SetFileAttributes(sourceName, FILE_ATTRIBUTE_ARCHIVE);

                        if (!CRAPI::RemoveDirectory(sourceName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = CRAPI::GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, cefsSourceName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                            SalamanderGeneral->RemoveFilesFromCache(cefsSourceName);

                            sourcePathChanged = TRUE;
                            subdirsOfSourcePathChanged = TRUE;
                            break; // successful RemoveDirectory
                        }
                    }
                    else
                        break;
                }
                else
                {
                    // remove the file on CEFS
                    if (fi.dwFileAttributes & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                        CRAPI::SetFileAttributes(sourceName, FILE_ATTRIBUTE_ARCHIVE);

                    if (!CRAPI::DeleteFile(sourceName))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, cefsSourceName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                        SalamanderGeneral->RemoveOneFileFromCache(cefsSourceName);

                        sourcePathChanged = TRUE;
                        break; // successful delete
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        if (success)
        {
            copied += isDir ? 100 : fi.size;

            float progress = (totalsize ? ((float)copied / (float)totalsize) : 0) * 1000;
            dlg.SetProgress((DWORD)progress, 0, FALSE);
        }
        else
            break; // Determine whether it makes sense to continue (when not cancelled)
    }

    EnableWindow(mainWnd, TRUE);
    DestroyWindow(dlg.HWindow); // close the progress dialog

    // Change on the source path Path (primarily move operations)
    if (sourcePathChanged)
    {
        sprintf(path, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(path, subdirsOfSourcePathChanged);
    }

    // Change on the target path 'targetPath'
    if (targetPathChanged)
        SalamanderGeneral->PostChangeOnPathNotification(targetPath, subdirsOfTargetPathChanged);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    if (success)
        strcpy(targetPath, nextFocus); // success
    else
        cancelOrHandlePath = TRUE; // error/cancel
    return TRUE;                   // success or error/cancel handled
}

static BOOL FindAllFilesInTree(LPCTSTR rootPath, char (&path)[MAX_PATH], LPCTSTR fileName, CFileInfoArray& array, BOOL dirFirst, int block)
{
    HANDLE find = INVALID_HANDLE_VALUE;

    WIN32_FIND_DATA data;
    char fullPath[MAX_PATH];
    strcpy(fullPath, rootPath);
    if (!SalamanderGeneral->SalPathAppend(fullPath, path, MAX_PATH) ||

        !SalamanderGeneral->SalPathAppend(fullPath, fileName, MAX_PATH))
        goto ONERROR_TOOLONG;

    find = FindFirstFile(fullPath, &data);
    if (find == INVALID_HANDLE_VALUE)
    {
        DWORD err = GetLastError();
        if (err == ERROR_NO_MORE_FILES || err == ERROR_FILE_NOT_FOUND)
            return TRUE; // JR empty directory, stop

        char buf[2 * MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_PATH_ERROR), fullPath, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, TitleWMobileError, MSGBOX_ERROR);
        return FALSE;
    }

    for (;;)
    {
        // JR TODO: This does not work!
        if (SalamanderGeneral->GetSafeWaitWindowClosePressed())
        {
            if (SalamanderGeneral->ShowMessageBox(LoadStr(IDS_YESNO_CANCEL), TitleWMobileQuestion,
                                                  MSGBOX_QUESTION) == IDYES)
                goto ONERROR;
        }

        if (data.cFileName[0] != 0 &&
            (data.cFileName[0] != '.' || // JR Windows Mobile does not return "." and ".." paths, but handle it just in case
             (data.cFileName[1] != 0 && (data.cFileName[1] != '.' || data.cFileName[2] != 0))))
        {
            CFileInfo fi;
            strcpy(fi.cFileName, path);
            if (!SalamanderGeneral->SalPathAppend(fi.cFileName, data.cFileName, MAX_PATH))
                goto ONERROR_TOOLONG;

            fi.dwFileAttributes = data.dwFileAttributes;

            if ((data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY))
            {
                fi.size = 0;

                if (dirFirst)
                {
                    fi.block = -1;
                    array.Add(fi);
                    if (array.State != etNone)
                    {
                        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                        TRACE_E("Low memory");
                        goto ONERROR;
                    }
                }

                int len = (int)strlen(path);
                if (!SalamanderGeneral->SalPathAppend(path, data.cFileName, MAX_PATH))
                    goto ONERROR_TOOLONG;

                if (!FindAllFilesInTree(rootPath, path, "*.*", array, dirFirst, block))
                    goto ONERROR; // JR The error has already been reported

                path[len] = 0;
            }
            else
                fi.size = data.nFileSizeLow;

            fi.block = block;
            array.Add(fi);
            if (array.State != etNone)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                TRACE_E("Low memory");
                goto ONERROR;
            }
        }

        if (!FindNextFile(find, &data))
        {
            if (GetLastError() == ERROR_NO_MORE_FILES)
                break; // JR Everything is fine, stop

            DWORD err = GetLastError();
            SalamanderGeneral->ShowMessageBox(SalamanderGeneral->GetErrorText(err), TitleWMobileError, MSGBOX_ERROR);
            FindClose(find);
            return FALSE;
        }
    }

    FindClose(find);
    return TRUE;

ONERROR_TOOLONG:
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                      TitleWMobileError, MSGBOX_ERROR);
ONERROR:
    if (find != INVALID_HANDLE_VALUE)
        FindClose(find);
    return FALSE;
}

static BOOL FindAllFilesInTree(const char* rootPath, const char* fileName, CFileInfoArray& array, BOOL dirFirst, int block)
{
    char path[MAX_PATH];
    path[0] = 0;

    return FindAllFilesInTree(rootPath, path, fileName, array, dirFirst, block);
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
        // Add the *.* mask to the target path (we will process operation masks)
        SalamanderGeneral->SalPathAppend(targetPath, "*.*", 2 * MAX_PATH);
        return TRUE;
    }

    char cefsFileName[2 * MAX_PATH];
    char buf[3 * MAX_PATH + 100];

    if (mode != 2 && mode != 3)
        return FALSE; // unknown 'mode'

    // 'targetPath' contains the raw path entered by the user (all we know is that it belongs
    // to this FS, otherwise Salamander would not call this method)
    char* userPart = strchr(targetPath, ':') + 1; // 'targetPath' must contain the FS name + ':'

    BOOL invPath = (userPart[0] != '\\');

    // Check whether the operation can be executed on this FS; the user might also have used
    // "." and ".." in the full path to this FS - remove them
    int rootLen = 0;
    if (!invPath)
    {
        rootLen = 1;
        int userPartLen = (int)strlen(userPart);
        if (userPartLen < rootLen)
            rootLen = userPartLen;
    }

    if (invPath || !SalamanderGeneral->SalRemovePointsFromPath(userPart + rootLen))
    {
        // Additionally we could display 'err' (when 'invPath' is TRUE); ignored here for simplicity
        SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_INVALIDPATH),
                                         TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
        // 'targetPath' is returned after possibly adjusting some ".." and "."
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return FALSE; // let the user correct the path
    }

    // Trim the unnecessary backslash
    int l = (int)strlen(userPart);
    BOOL backslashAtEnd = l > 0 && userPart[l - 1] == '\\';
    if (l > 1 && userPart[l - 1] == '\\') // path of the form "\path\"
        userPart[l - 1] = 0;              // remove the trailing backslash

    // Analyse the path - locate the existing part, the missing part, and the operation mask
    //
    // - Determine which portion exists and whether it is a file or a directory,
    //   then select what applies:
    //   - write to the path (possibly with a missing portion) with a mask - the mask is the last missing part
    //     after which there is no backslash (verify that when multiple source files/directories are involved the mask
    //     contains '*' or at least '?'; otherwise it is nonsense -> only one target name is possible)
    //   - write into an archive (the path contains an archive file, or it may not even be an archive, resulting in
    //     the "Salamander does not know how to open this file" error)
    //   - overwrite a file (the entire path is just the target file name; must not end with a backslash)

    // Determine how far the path exists (split into existing and missing parts)
    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    char* end = targetPath + strlen(targetPath);
    char* afterRoot = userPart + rootLen;
    char lastChar = 0;
    BOOL pathIsDir = TRUE;
    BOOL pathError = FALSE;

    // If the path contains a mask, cut it off without calling GetFileAttributes
    if (end > afterRoot) // still more than just the root
    {
        char* end2 = end;
        BOOL cut = FALSE;
        while (*--end2 != '\\') // there is guaranteed to be at least one '\\' after the root path
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

    while (end > afterRoot) // still more than just the root
    {
        DWORD attrs = CRAPI::GetFileAttributes(userPart);
        if (attrs != 0xFFFFFFFF) // this portion of the path exists
        {
            if ((attrs & FILE_ATTRIBUTE_DIRECTORY) == 0) // it is a file
            {
                // An existing path must not include a file name (see SalSplitGeneralPath); trim it...
                *end = lastChar;   // restore 'targetPath'
                pathIsDir = FALSE; // the existing part of the path is a file
                while (*--end != '\\')
                    ;            // there is guaranteed to be at least one '\\' beyond the root path
                lastChar = *end; // so the path remains valid
                break;
            }
            else
                break;
        }
        else
        {
            DWORD err = CRAPI::GetLastError();
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_INVALID_NAME &&
                err != ERROR_PATH_NOT_FOUND && err != ERROR_BAD_PATHNAME &&
                err != ERROR_DIRECTORY) // unexpected error - just report it
            {
                sprintf(buf, LoadStr(IDS_PATH_ERROR), targetPath, SalamanderGeneral->GetErrorText(err));
                SalamanderGeneral->SalMessageBox(parent, buf, TitleWMobile, MB_OK | MB_ICONEXCLAMATION);
                pathError = TRUE;
                break; // report the error
            }
        }

        *end = lastChar; // restore 'targetPath'
        while (*--end != '\\')
            ; // there is guaranteed to be at least one '\\' after the root path
        lastChar = *end;
        *end = 0;
    }
    *end = lastChar; // restore 'targetPath'
    SetCursor(oldCur);

    char* opMask = NULL;
    if (!pathError) // the split finished without errors
    {
        if (*end == '\\')
            end++;

        char newDirs[MAX_PATH];
        if (SalamanderGeneral->SalSplitGeneralPath(parent, TitleWMobile, TitleWMobileError, sourceFiles + sourceDirs,
                                                   targetPath, afterRoot, end, pathIsDir,
                                                   backslashAtEnd, NULL, NULL, opMask, newDirs,
                                                   NULL /* 'isTheSamePathF' not needed */))
        {
            if (newDirs[0] != 0) // need to create some subdirectories on the target path
            {
                if (!CRAPI::CheckAndCreateDirectory(userPart, parent, true, NULL, 0, NULL))
                {
                    char* e = targetPath + strlen(targetPath); // restore 'targetPath' (join 'targetPath' and 'opMask')
                    if (e > targetPath && *(e - 1) != '\\')
                        *e++ = '\\';
                    if (e != opMask)
                        memmove(e, opMask, strlen(opMask) + 1); // move the mask if needed
                    pathError = TRUE;
                }
            }
        }
        else
            pathError = TRUE;
    }

    if (pathError)
    {
        // 'targetPath' is returned after cleaning up ".." and "." + any mask that was added
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return FALSE; // path error - let the user fix it
    }

    // Description of the operation target obtained in the preceding code:
    // 'targetPath' is a path on this FS ('userPart' points to the user part of the FS path), 'opMask' is the operation mask

    // Prepare buffers for names
    char sourceName[MAX_PATH]; // buffer for the full disk name
    strcpy(sourceName, sourcePath);
    char* endSource = sourceName + strlen(sourceName); // space for names from the 'next' enumeration
    if (endSource > sourceName && *(endSource - 1) != '\\')
    {
        *endSource++ = '\\';
        *endSource = 0;
    }
    int endSourceSize = MAX_PATH - (int)(endSource - sourceName); // maximum number of characters for a 'next' name

    char targetName[MAX_PATH]; // buffer for the full target disk name (the operation target resides on disk for CEFS)
    strcpy(targetName, userPart);
    char* endTarget = targetName + strlen(targetName); // space for the destination name
    if (endTarget > targetName && *(endTarget - 1) != '\\')
    {
        *endTarget++ = '\\';
        *endTarget = 0;
    }
    int endTargetSize = MAX_PATH - (int)(endTarget - targetName); // maximum number of characters for the destination name

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());
    CFileInfoArray array(10, 10);

    BOOL success = TRUE; // FALSE in case of an error or user cancellation

    BOOL isDir;
    const char* name;
    const char* dosName; // dummy
    CQuadWord size;
    DWORD attr1;
    FILETIME lastWrite;
    int errorOccured;

    while ((name = next(parent, 0, &dosName, &isDir, &size, &attr1, &lastWrite, nextParam, &errorOccured)) != NULL)
    { // perform copy/move on a file/directory
        success = FindAllFilesInTree(sourcePath, name, array, TRUE, 0);

        // Determine whether it makes sense to continue (when not cancelled)
        if (!success)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    BOOL skipAllErrors = FALSE;              // skip all errors
    BOOL sourcePathChanged = FALSE;          // TRUE if the source path changed (move operation)
    BOOL subdirsOfSourcePathChanged = FALSE; // TRUE if subdirectories of the source path changed
    BOOL targetPathChanged = FALSE;          // TRUE if the target path changed
    BOOL subdirsOfTargetPathChanged = FALSE; // TRUE if subdirectories of the target path changed
    BOOL skipAllOverwrite = FALSE;
    BOOL skipAllOverwriteSystemHidden = FALSE;

    // Retrieve the "Confirm on" values from the configuration
    BOOL ConfirmOnFileOverwrite, ConfirmOnSystemHiddenFileOverwrite;
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
    SalamanderGeneral->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // Disable 'mainWnd'

    CProgress2Dlg dlg(mainWnd, LoadStr(copy ? IDS_COPY : IDS_MOVE), LoadStr(copy ? IDS_COPYING : IDS_MOVING), LoadStr(IDS_TO), ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack

    dlg.Create();
    EnableWindow(mainWnd, FALSE);
    SetForegroundWindow(dlg.HWindow);

    INT64 totalsize = 0, copied = 0;
    int i;
    for (i = 0; i < array.Count; i++)
        totalsize += (array[i].dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? 100 : array[i].size;

    for (/*int*/ i = 0; i < array.Count; i++)
    {
        CFileInfo& fi = array[i];

        char* targetFile = SalamanderGeneral->MaskName(buf, 3 * MAX_PATH + 100, fi.cFileName, opMask);

        if ((int)strlen(fi.cFileName) >= endSourceSize || (int)strlen(targetFile) >= endTargetSize)
        {
            SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        // Construct the full name; trimming to MAX_PATH is theoretically redundant, but unfortunately needed in practice
        lstrcpyn(endSource, fi.cFileName, endSourceSize);

        // Compose the target name - simplified without the LoadStr(IDS_ERR_NAMETOOLONG) error check
        // ('name' covers only the root of the source path - no subdirectories - adjust the entire 'name' with the mask)
        lstrcpyn(endTarget, targetFile, endTargetSize);

        isDir = (fi.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;

        if (SalamanderGeneral->StrICmp(sourceName, targetName) == 0)
        {
            SalamanderGeneral->SalMessageBox(parent,
                                             LoadStr(copy ? (isDir ? IDS_ERR_COPYDIRTOITSELF : IDS_ERR_COPYFILETOITSELF) : (isDir ? IDS_ERR_MOVEDIRTOITSELF : IDS_ERR_MOVEFILETOITSELF)),
                                             TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
            success = FALSE;
            break;
        }

        dlg.Set(sourceName, targetName, FALSE);

        BOOL skip = FALSE;
        if (isDir)
        {
            if (fi.block == -1)
            {
                while (1)
                {
                    if (!CRAPI::CheckAndCreateDirectory(targetName, parent, true, NULL, 0, NULL))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = CRAPI::GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, targetName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                        subdirsOfTargetPathChanged = TRUE;
                        break; // successfully created directory
                    }
                } // end of while(1)
            }
        }
        else
        {
            // copy the file directly to CEFS

            DWORD attr = CRAPI::GetFileAttributes(targetName);

            if (attr != 0xFFFFFFFF)
            {
                if (!skipAllOverwrite)
                {
                    if (ConfirmOnFileOverwrite)
                    {
                        char sourceData[100], targetData[100];

                        GetFileData(sourceName, sourceData);
                        CRAPI::GetFileData(targetName, targetData);

                        int res = SalamanderGeneral->DialogOverwrite(parent, BUTTONS_YESALLSKIPCANCEL, targetName, targetData, sourceName, sourceData);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;
                        case DIALOG_SKIPALL:
                            skipAllOverwrite = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;
                        default:
                            success = FALSE;
                            break;
                        }
                    }
                }
                else
                    skip = TRUE;

                if (success && !skip && ConfirmOnSystemHiddenFileOverwrite && (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN)))
                {
                    if (!skipAllOverwriteSystemHidden)
                    {
                        int res = SalamanderGeneral->DialogQuestion(parent, BUTTONS_YESALLSKIPCANCEL, targetName,
                                                                    LoadStr(IDS_YESNO_OVERWRITEHIDDENFILE), TitleWMobileQuestion);
                        switch (res)
                        {
                        case DIALOG_ALL:
                            ConfirmOnSystemHiddenFileOverwrite = FALSE;
                        case DIALOG_YES:
                            break;
                        case DIALOG_SKIPALL:
                            skipAllOverwriteSystemHidden = TRUE;
                        case DIALOG_SKIP:
                            skip = TRUE;
                            break;
                        default:
                            success = FALSE;
                            break;
                        }
                    }
                    else
                        skip = TRUE;
                }
            }
            else
                attr = 0;

            if (success && !skip)
            {
                while (1)
                {
                    if (attr & (FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_READONLY))
                        CRAPI::SetFileAttributes(targetName, FILE_ATTRIBUTE_ARCHIVE);

                    LPCTSTR errFileName = "";
                    DWORD err = CRAPI::CopyFileToCE(sourceName, targetName, FALSE, &dlg, copied, totalsize, &errFileName);
                    if (err != 0)
                    {
                        if (err == -1) // JR cancelled by the user
                            success = FALSE;
                        else if (!skipAllErrors)
                        {
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, errFileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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

                        sprintf(cefsFileName, "%s:%s", fsName, targetName);
                        SalamanderGeneral->ToLowerCase(cefsFileName);
                        SalamanderGeneral->RemoveOneFileFromCache(cefsFileName);

                        break; // copied successfully
                    }
                    if (!success || skip)
                        break;
                }
            }
        }

        if (success && !copy && !skip) // it is a "move" and the file was not skipped -> delete the source file
        {

            // remove the file on disk
            while (1)
            {
                if (isDir)
                {
                    if (fi.block != -1)
                    {
                        SalamanderGeneral->ClearReadOnlyAttr(sourceName, fi.dwFileAttributes);

                        if (!RemoveDirectory(sourceName))
                        {
                            if (!skipAllErrors)
                            {
                                DWORD err = GetLastError();
                                int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, sourceName,
                                                                         SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                            subdirsOfSourcePathChanged = TRUE;
                            break; // successful RemoveDirectory
                        }
                    }
                    else
                        skip = TRUE;
                }
                else
                {
                    SalamanderGeneral->ClearReadOnlyAttr(sourceName, fi.dwFileAttributes);

                    if (!DeleteFile(sourceName))
                    {
                        if (!skipAllErrors)
                        {
                            DWORD err = GetLastError();
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, sourceName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                        break; // successful delete
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        if (success)
        {
            copied += isDir ? 100 : fi.size;

            float progress = (totalsize ? ((float)copied / (float)totalsize) : 0) * 1000;
            dlg.SetProgress((DWORD)progress, 0, FALSE);
        }
        else
            break; // Determine whether it makes sense to continue (when not cancelled)
    }

    EnableWindow(mainWnd, TRUE);
    DestroyWindow(dlg.HWindow); // close the progress dialog

    // Change on the source path (primarily move operations), can only be a disk path
    if (sourcePathChanged)
        SalamanderGeneral->PostChangeOnPathNotification(sourcePath, subdirsOfSourcePathChanged);

    // Change on the target path (should be FS - 'targetPath', but for CEFS it is the disk path 'userPart')
    if (targetPathChanged)
    {
        sprintf(cefsFileName, "%s:%s", fsName, userPart);
        SalamanderGeneral->PostChangeOnPathNotification(cefsFileName, subdirsOfTargetPathChanged);
    }

    SalamanderGeneral->RestoreFocusInSourcePanel();

    if (success)
        return TRUE; // operation finished successfully
    else
    {
        if (invalidPathOrCancel != NULL)
            *invalidPathOrCancel = TRUE;
        return TRUE; // cancel
    }
}

BOOL WINAPI
CPluginFSInterface::ChangeAttributes(const char* fsName, HWND parent, int panel,
                                     int selectedFiles, int selectedDirs)
{
    // Prepare buffers for names
    char fileName[MAX_PATH]; // buffer for the full disk name (the source resides on disk for DFS)
    strcpy(fileName, Path);
    char* end = fileName + strlen(fileName); // space for names from the panel
    if (end > fileName && *(end - 1) != '\\')
    {
        *end++ = '\\';
        *end = 0;
    }
    int endSize = MAX_PATH - (int)(end - fileName); // maximum number of characters for a panel name

    const CFileData* f = NULL; // pointer to the file/directory in the panel to process
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0, count = 0;
    BOOL success = TRUE;        // FALSE in case of an error or user cancellation
    BOOL skipAllErrors = FALSE; // skip all errors

    RapiNS::CE_FIND_DATA findData;
    DWORD attr = 0, attrDiff = 0;
    SYSTEMTIME timeModified, timeCreated, timeAccessed;
    BOOL selectedDirectory = FALSE;

    // JR phase 1 - determine the current attributes
    while (1)
    {
        // Retrieve data about the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // Perform the operation on the file/directory
        if (f != NULL)
        {

            if ((int)strlen(f->Name) >= endSize)
            {
                SalamanderGeneral->SalMessageBox(parent, LoadStr(IDS_ERR_NAMETOOLONG),
                                                 TitleWMobileError, MB_OK | MB_ICONEXCLAMATION);
                success = FALSE;
                break;
            }

            lstrcpyn(end, f->Name, endSize);

            BOOL skip = FALSE;
            while (1)
            {
                if (count == 0)
                {
                    HANDLE handle = CRAPI::FindFirstFile(fileName, &findData);
                    if (handle == INVALID_HANDLE_VALUE)
                    {
                        if (skipAllErrors)
                            skip = TRUE;
                        else
                        {
                            DWORD err = CRAPI::GetLastError();
                            if (err == ERROR_NO_MORE_FILES)
                                err = ERROR_FILE_NOT_FOUND;
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                    }
                    else
                    {
                        CRAPI::FindClose(handle);

                        DWORD attrib = findData.dwFileAttributes;
                        if (attrib & FILE_ATTRIBUTE_DIRECTORY)
                            selectedDirectory = TRUE;

                        attrib &= FILE_ATTRIBUTES_MASK;
                        attr |= attrib;

                        FILETIME time;
                        FileTimeToLocalFileTime(&findData.ftLastWriteTime, &time);
                        FileTimeToSystemTime(&time, &timeModified);
                        FileTimeToLocalFileTime(&findData.ftCreationTime, &time);
                        FileTimeToSystemTime(&time, &timeCreated);
                        FileTimeToLocalFileTime(&findData.ftLastAccessTime, &time);
                        FileTimeToSystemTime(&time, &timeAccessed);

                        count++;
                        break;
                    }
                }
                else
                {
                    DWORD attrib = CRAPI::GetFileAttributes(fileName);
                    if (attr == 0xFFFFFFFF)
                    {
                        if (skipAllErrors)
                            skip = TRUE;
                        else
                        {
                            DWORD err = CRAPI::GetLastError();
                            //if (err == ERROR_NO_MORE_FILES) err = ERROR_FILE_NOT_FOUND;
                            int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                     SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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
                    }
                    else
                    {
                        if (attrib & FILE_ATTRIBUTE_DIRECTORY)
                            selectedDirectory = TRUE;

                        attrib &= FILE_ATTRIBUTES_MASK;
                        attrDiff |= (attr ^ attrib);
                        attr |= attrib;

                        count++;
                        break;
                    }
                }
                if (!success || skip)
                    break;
            }
        }

        // Determine whether it makes sense to continue (when not cancelled and another selected item exists)
        if (!success || focused || f == NULL)
            break;
    }

    char path[2 * MAX_PATH];
    if (!success || count == 0)
    {
        // JR The file/directory was probably deleted already
        if (count == 0)
        {
            sprintf(path, "%s:%s", fsName, Path);
            SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
        }
        return FALSE;
    }

    if (count > 1)
    {
        GetSystemTime(&timeModified);
        timeCreated = timeModified;
        timeAccessed = timeModified;
    }

    CChangeAttrDialog dlgAttr(parent, ooStatic, attr, attrDiff, selectedDirectory, &timeModified, &timeCreated, &timeAccessed);
    if (dlgAttr.Execute() != IDOK)
        return FALSE;

    SalamanderGeneral->CreateSafeWaitWindow(LoadStr(IDS_WAIT_READINGDIRTREE), TitleWMobile,
                                            500, FALSE, SalamanderGeneral->GetMainWindowHWND());

    CFileInfoArray array(count, 10);

    // JR phase 2 - load all files whose attributes will be modified

    index = 0;
    for (;;)
    {
        // Retrieve data about the file being processed
        if (focused)
            f = SalamanderGeneral->GetPanelFocusedItem(panel, &isDir);
        else
            f = SalamanderGeneral->GetPanelSelectedItem(panel, &index, &isDir);

        // Delete the file/directory
        // JR Call FindAllFilesInTree even for individual files
        // JR This verifies that they still exist
        if (f != NULL)
        {

            if (dlgAttr.RecurseSubDirs)
            {
                *end = 0; // JR => fileName contains rootPath
                success = CRAPI::FindAllFilesInTree(fileName, f->Name, array, 0, FALSE);
            }
            else
            {
                CFileInfo fi;
                strcpy(fi.cFileName, f->Name);
                fi.dwFileAttributes = f->Attr;

                array.Add(fi);
                if (array.State != etNone)
                {
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORYLOW), TitleWMobileError, MSGBOX_ERROR);
                    TRACE_E("Low memory");
                    success = false;
                }
            }
        }

        // Determine whether it makes sense to continue (when no error occurred and another selected item exists)
        if (!success || focused || f == NULL)
            break;
    }

    SalamanderGeneral->DestroySafeWaitWindow();

    if (!success || array.Count == 0)
    {
        // JR The file/directory was probably deleted already
        if (array.Count == 0)
        {
            sprintf(path, "%s:%s", fsName, Path);
            SalamanderGeneral->PostChangeOnPathNotification(path, FALSE);
        }
        return FALSE;
    }

    BOOL pathChanged = FALSE;                      // TRUE if the path changed
    BOOL changeInSubdirs = dlgAttr.RecurseSubDirs; // TRUE if changes occurred in subdirectories as well
    skipAllErrors = FALSE;                         // skip all errors

    // JR phase 3 - apply attributes

    HWND mainWnd = parent;
    HWND parentWin;
    while ((parentWin = GetParent(mainWnd)) != NULL && IsWindowEnabled(parentWin))
        mainWnd = parentWin;
    // Disable 'mainWnd'

    BOOL showProgressDialog = array.Count > 1;
    BOOL enableMainWnd = TRUE;
    CProgressDlg delDlg(mainWnd, LoadStr(IDS_ATTRIBUTES), LoadStr(IDS_CHANGING), ooStatic); // use 'ooStatic' so the modeless dialog can live on the stack

    if (showProgressDialog)
    {
        EnableWindow(mainWnd, FALSE);
        delDlg.Create();
    }

    if (!showProgressDialog || delDlg.HWindow != NULL) // dialog opened successfully
    {
        if (showProgressDialog)
            SetForegroundWindow(delDlg.HWindow);

        int block = 0;
        int i;
        for (i = 0; success && i < array.Count; i++)
        {
            CFileInfo& fi = array[i];

            *end = 0;
            if (!CRAPI::PathAppend(fileName, fi.cFileName, MAX_PATH))
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_PATHTOOLONG),
                                                  TitleWMobileError, MSGBOX_ERROR);
                success = FALSE;
                break;
            }

            if (showProgressDialog)
            {
                float progress = ((float)i / (float)array.Count);
                delDlg.Set(fileName, (DWORD)(progress * 1000), TRUE); // delayedPaint == TRUE so we do not slow things down
            }

            if (showProgressDialog && delDlg.GetWantCancel())
            {
                success = FALSE;
                break;
            }

            BOOL skip = FALSE;
            while (1)
            {
                DWORD attr2 = fi.dwFileAttributes;

                DWORD err = 0;
                if (dlgAttr.Archive == 0)
                    attr2 &= ~FILE_ATTRIBUTE_ARCHIVE;
                if (dlgAttr.Archive == 1)
                    attr2 |= FILE_ATTRIBUTE_ARCHIVE;
                if (dlgAttr.ReadOnly == 0)
                    attr2 &= ~FILE_ATTRIBUTE_READONLY;
                if (dlgAttr.ReadOnly == 1)
                    attr2 |= FILE_ATTRIBUTE_READONLY;
                if (dlgAttr.System == 0)
                    attr2 &= ~FILE_ATTRIBUTE_SYSTEM;
                if (dlgAttr.System == 1)
                    attr2 |= FILE_ATTRIBUTE_SYSTEM;
                if (dlgAttr.Hidden == 0)
                    attr2 &= ~FILE_ATTRIBUTE_HIDDEN;
                if (dlgAttr.Hidden == 1)
                    attr2 |= FILE_ATTRIBUTE_HIDDEN;

                if (fi.dwFileAttributes != attr2)
                {
                    if (!CRAPI::SetFileAttributes(fileName, attr2))
                        err = CRAPI::GetLastError();
                }

                if (err == 0 && (attr2 & FILE_ATTRIBUTE_DIRECTORY) == 0) // JR Apparently timestamps cannot be changed for directories
                {
                    err = CRAPI::SetFileTime(fileName,
                                             dlgAttr.ChangeTimeCreated ? &dlgAttr.TimeCreated : NULL,
                                             dlgAttr.ChangeTimeAccessed ? &dlgAttr.TimeAccessed : NULL,
                                             dlgAttr.ChangeTimeModified ? &dlgAttr.TimeModified : NULL);
                }

                if (err == 0)
                {
                    pathChanged = TRUE;
                    break; // JR succeeded, continue
                }
                else
                {
                    if (!skipAllErrors)
                    {
                        int res = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName,
                                                                 SalamanderGeneral->GetErrorText(err), TitleWMobileError);
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

                if (!success || skip)
                    break;
            }
        }

        if (showProgressDialog)
        {
            // Enable 'mainWnd' (otherwise Windows cannot select it as the foreground/active window)
            EnableWindow(mainWnd, TRUE);
            enableMainWnd = FALSE;

            DestroyWindow(delDlg.HWindow); // close the progress dialog
        }
    }

    // Enable 'mainWnd' (no foreground change occurred - the progress dialog never opened)
    if (showProgressDialog && enableMainWnd)
        EnableWindow(mainWnd, TRUE);

    SalamanderGeneral->RestoreFocusInSourcePanel();

    // Change on the source path Path
    if (pathChanged)
    {
        sprintf(path, "%s:%s", fsName, Path);
        SalamanderGeneral->PostChangeOnPathNotification(path, changeInSubdirs);
    }

    return success;
}

void WINAPI
CPluginFSInterface::ShowProperties(const char* fsName, HWND parent, int panel,
                                   int selectedFiles, int selectedDirs)
{
}

void WINAPI
CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int menutype,
                                int panel, int selectedFiles, int selectedDirs)
{

    HMENU menu = CreatePopupMenu();
    if (menu == NULL)
    {
        TRACE_E("CPluginFSInterface::ContextMenu: Unable to create menu.");
        return;
    }
    MENUITEMINFO mi;
    char nameBuf[200];

    switch (menutype)
    {
    case fscmPathInPanel:  // context menu for the current path in the panel
    case fscmPanel:        // context menu for the panel
    case fscmItemsInPanel: // context menu for panel items (selected/focused files and directories)
    {
        // insert Salamander commands
        int i = 0;
        int index = 0;
        int salCmd;
        BOOL enabled;
        int type, lastType = sctyUnknown;
        while (SalamanderGeneral->EnumSalamanderCommands(&index, &salCmd, nameBuf, 200, &enabled, &type))
        {
            if ((menutype == fscmItemsInPanel && type != sctyForCurrentPath && type != sctyForConnectedDrivesAndFS ||
                 menutype == fscmPanel && (type == sctyForCurrentPath || type == sctyForConnectedDrivesAndFS)) &&
                salCmd != SALCMD_CHANGECASE &&
                salCmd != SALCMD_EMAIL && salCmd != SALCMD_EDITNEWFILE)
            {
                if (type != lastType && lastType != sctyUnknown) // insert a separator
                {
                    memset(&mi, 0, sizeof(mi));
                    mi.cbSize = sizeof(mi);
                    mi.fMask = MIIM_TYPE;
                    mi.fType = MFT_SEPARATOR;
                    InsertMenuItem(menu, i++, TRUE, &mi);
                }
                lastType = type;

                // insert Salamander commands
                memset(&mi, 0, sizeof(mi));
                mi.cbSize = sizeof(mi);
                mi.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
                mi.fType = MFT_STRING;
                mi.wID = salCmd + 1000;
                mi.dwTypeData = nameBuf;
                mi.cch = (UINT)strlen(nameBuf);
                mi.fState = enabled ? MFS_ENABLED : MFS_DISABLED;
                InsertMenuItem(menu, i++, TRUE, &mi);
            }
        }
        if (i > 0)
        {
            DWORD cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_RIGHTBUTTON,
                                         menuX, menuY, parent, NULL);
            if (cmd >= 1000)
                SalamanderGeneral->PostSalamanderCommand(cmd - 1000);
        }
    }
    break;
    }
}

void CPluginFSInterface::EmptyCache()
{
    // Build a unique name for this FS root in the disk cache (touch all cached copies of files from this FS)
    char uniqueFileName[2 * MAX_PATH];
    strcpy(uniqueFileName, AssignedFSName);
    strcat(uniqueFileName, ":\\");
    // Disk names are case-insensitive, the disk cache is case-sensitive; converting
    // to lowercase makes the disk cache behave case-insensitively as well
    SalamanderGeneral->ToLowerCase(uniqueFileName);
    SalamanderGeneral->RemoveFilesFromCache(uniqueFileName);
}
