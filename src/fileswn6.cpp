// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "dialogs.h"
#include "worker.h"
#include "cache.h"
#include "pack.h"
#include "shellib.h"
#include "filesbox.h"

// Helper variables for dialogs in BuildScriptXXX()
BOOL ConfirmADSLossAll = FALSE;
BOOL ConfirmADSLossSkipAll = FALSE;
BOOL ConfirmCopyLinkContentAll = FALSE;
BOOL ConfirmCopyLinkContentSkipAll = FALSE;
BOOL ErrReadingADSIgnoreAll = FALSE;
BOOL ErrFileSkipAll = FALSE;
BOOL ErrTooLongNameSkipAll = FALSE;
BOOL ErrTooLongDirNameSkipAll = FALSE;
BOOL ErrTooLongTgtNameSkipAll = FALSE;
BOOL ErrTooLongTgtDirNameSkipAll = FALSE;
BOOL ErrTooLongSrcDirNameSkipAll = FALSE;
BOOL ErrListDirSkipAll = FALSE;
BOOL ErrTooBigFileFAT32SkipAll = FALSE;
BOOL ErrGetFileSizeOfLnkTgtIgnAll = FALSE;

//
// ****************************************************************************
// CFilesWindow
//

// auxiliary variables for testing attempts to interrupt the script execution
DWORD LastTickCount;

void CFilesWindow::Activate(BOOL shares)
{
    CALL_STACK_MESSAGE_NONE
    //  TRACE_I("CFilesWindow::Activate");
    LastInactiveRefreshStart = LastInactiveRefreshEnd; // Deactivation clears the data about the last refresh in an inactive window
    BOOL needToRefreshIcons = InactWinOptimizedReading;
    if (Is(ptDisk) || Is(ptZIPArchive)) // disks and archives
    {
        if (!SkipOneActivateRefresh && (!GetNetworkDrive() || !Configuration.DrvSpecRemoteDoNotRefreshOnAct) ||
            InactiveRefreshTimerSet) // Deferred refresh when the window is inactive must be performed immediately upon window activation
        {
            DWORD checkPathRet;
            if ((checkPathRet = CheckPath(FALSE)) != ERROR_SUCCESS)
            {
                if (checkPathRet == ERROR_USER_TERMINATED) // user pressed ESC -> change to fixed drive
                {
                    if (MainWindow->LeftPanel == this)
                    {
                        if (!ChangeLeftPanelToFixedWhenIdleInProgress)
                            ChangeLeftPanelToFixedWhenIdle = TRUE;
                    }
                    else
                    {
                        if (!ChangeRightPanelToFixedWhenIdleInProgress)
                            ChangeRightPanelToFixedWhenIdle = TRUE;
                    }
                }
                else // Another error on the way, let's refresh
                {
                    HANDLES(EnterCriticalSection(&TimeCounterSection));
                    int t1 = MyTimeCounter++;
                    HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
                }
                needToRefreshIcons = FALSE;
            }
            else // the road looks o.k.
            {
                if (!AutomaticRefresh && !GetNetworkDrive() ||           // Manual disk refresh (excluding network disk)
                    GetNetworkDrive() &&                                 // Refresh is performed on each disk read.
                        !Configuration.DrvSpecRemoteDoNotRefreshOnAct || // activation, if not disabled (handled by Samba)
                    shares && !GetNetworkDrive() ||                      // rebuilding of shards (does not make sense on network disks)
                    InactiveRefreshTimerSet)                             // Deferred refresh when the window is inactive must be performed immediately upon window activation
                {
                    if (InactiveRefreshTimerSet)
                    {
                        //            TRACE_I("Refreshing on window activation (refresh in inactive window was delayed)");
                        KillTimer(HWindow, IDT_INACTIVEREFRESH);
                        InactiveRefreshTimerSet = FALSE;
                    }
                    HANDLES(EnterCriticalSection(&TimeCounterSection));
                    int t1 = MyTimeCounter++;
                    HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    PostMessage(HWindow, WM_USER_REFRESH_DIR_EX, FALSE, t1); // we know it's probably an unnecessary refresh
                    needToRefreshIcons = FALSE;
                }
                else // on automatically refreshed disks, we will restore at least disk-free-space
                {
                    RefreshDiskFreeSpace(FALSE, TRUE);
                }
            }
        }
    }
    else
    {
        if (Is(ptPluginFS)) // plug-in FS: we will send FSE_ACTIVATEREFRESH so that the plug-in can refresh
        {
            if (!SkipOneActivateRefresh)
                PostMessage(HWindow, WM_USER_REFRESH_PLUGINFS, 0, 0);
        }
    }
    if (needToRefreshIcons)
    {
        //    TRACE_I("Refreshing icons/thumbnails/icon-overlays (we have read only visible ones)");
        SleepIconCacheThread();
        InactWinOptimizedReading = FALSE;
        WakeupIconCacheThread();
    }
}

BOOL CFilesWindow::MakeFileList(HANDLE hFile)
{
    CALL_STACK_MESSAGE_NONE
    BOOL ret = TRUE;

    if (FilesActionInProgress)
        return FALSE;

    FilesActionInProgress = TRUE;

    int focusIndex = 0;
    int alloc;
    int count = GetSelCount();
    if (count == 0)
    {
        focusIndex = GetCaretIndex();
        alloc = 1;
    }
    else
        alloc = count;

    int* indexes;
    indexes = new int[alloc];
    if (indexes == NULL)
    {
        TRACE_E(LOW_MEMORY);
        FilesActionInProgress = FALSE;
        return FALSE;
    }
    else
    {
        if (count > 0)
            GetSelItems(count, indexes);
        else
            indexes[0] = focusIndex;

        int files = 0;
        int dirs = 0;
        CFileData* f;

        // In the first phase, we calculate the maximum widths of variables (if any uses $(name:max))
        int maxSizes[100];
        int maxSizesCount = 100;
        ZeroMemory(maxSizes, sizeof(maxSizes));
        int i;
        for (i = 0; i < alloc; i++)
        {
            if (indexes[i] >= 0 && indexes[i] < Dirs->Count + Files->Count)
            {
                f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);

                if (!ExpandMakeFileList(HWindow, Configuration.FileListHistory[0], &PluginData, f,
                                        indexes[i] < Dirs->Count, NULL, 0, TRUE, maxSizes, maxSizesCount,
                                        ValidFileData, GetPath(), i != 0))
                {
                    ret = FALSE;
                    goto exitus;
                }
            }
        }
        // we apply these widths in the second phase
        for (i = 0; i < alloc; i++)
        {
            if (indexes[i] >= 0 && indexes[i] < Dirs->Count + Files->Count)
            {
                f = (indexes[i] < Dirs->Count) ? &Dirs->At(indexes[i]) : &Files->At(indexes[i] - Dirs->Count);

                char buff[1000];
                buff[0] = 0;
                if (ExpandMakeFileList(HWindow, Configuration.FileListHistory[0], &PluginData, f,
                                       indexes[i] < Dirs->Count, buff, 1000, FALSE, maxSizes, maxSizesCount,
                                       ValidFileData, GetPath(), TRUE))
                {
                    DWORD len = (DWORD)strlen(buff);
                    if (len > 0)
                    {
                        DWORD written;
                        if (!WriteFile(hFile, buff, len, &written, NULL) || written != len)
                        {
                            DWORD err = GetLastError();
                            SalMessageBox(HWindow, GetErrorText(err),
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            ret = FALSE;
                            goto exitus;
                        }
                    }
                }
                else
                {
                    ret = FALSE;
                    goto exitus;
                }
            }
        }
    exitus:
        delete[] (indexes);
    }
    FilesActionInProgress = FALSE;
    return ret;
}

DWORD GetPathFlagsForCopyOp(const char* path, DWORD netFlag, DWORD fixedFlag)
{
    if (IsUNCPath(path))
        return netFlag;
    else
    {
        UINT drvType = MyGetDriveType(path);
        if (drvType == DRIVE_REMOTE)
            return netFlag;
        else if (drvType == DRIVE_FIXED || drvType == DRIVE_RAMDISK || drvType == DRIVE_CDROM)
            return fixedFlag;
        else if (drvType == DRIVE_REMOVABLE && UpperCase[path[0]] >= 'A' && UpperCase[path[0]] <= 'Z' && path[1] == ':' &&
                 GetDriveFormFactor(UpperCase[path[0]] - 'A' + 1) == 0 /* it's not a floppy*/)
        {
            return fixedFlag; // removable but not floppy, for example USB stick, camera via USB (e.g. FZ45) - we consider those as fixed, they are quite fast
        }
    }
    return 0;
}

BOOL CFilesWindow::MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                             const char* remapNameTo)
{
    CALL_STACK_MESSAGE5("CFilesWindow::MoveFiles(%s, %s, %s, %s)",
                        source, target, remapNameFrom, remapNameTo);
    if (!FilesActionInProgress)
    {
        if (CheckPath(TRUE, source) != ERROR_SUCCESS)
            return FALSE;

        FilesActionInProgress = TRUE;

        SetCurrentDirectory(source); // for faster move (system likes it)

        ConfirmADSLossAll = FALSE;
        ConfirmADSLossSkipAll = FALSE;
        ConfirmCopyLinkContentAll = FALSE;
        ConfirmCopyLinkContentSkipAll = FALSE;
        ErrReadingADSIgnoreAll = FALSE;
        ErrFileSkipAll = FALSE;
        ErrTooLongNameSkipAll = FALSE;
        ErrTooLongDirNameSkipAll = FALSE;
        ErrTooLongTgtNameSkipAll = FALSE;
        ErrTooLongTgtDirNameSkipAll = FALSE;
        ErrTooLongSrcDirNameSkipAll = FALSE;
        ErrListDirSkipAll = FALSE;
        ErrTooBigFileFAT32SkipAll = FALSE;
        ErrGetFileSizeOfLnkTgtIgnAll = FALSE;

        //--- creating script object
        COperations* script = new COperations(100, 50, NULL, NULL, NULL);
        if (script == NULL)
        {
            TRACE_E(LOW_MEMORY);
            FilesActionInProgress = FALSE;
            SetCurrentDirectoryToSystem();
            return FALSE;
        }
        script->RemapNameFrom = remapNameFrom;
        script->RemapNameFromLen = (int)strlen(remapNameFrom);
        script->RemapNameTo = remapNameTo;
        script->RemapNameToLen = (int)strlen(remapNameTo);

        BOOL sameRootPath = HasTheSameRootPath(source, target);
        script->SameRootButDiffVolume = sameRootPath && !HasTheSameRootPathAndVolume(source, target);
        script->ShowStatus = !sameRootPath || script->SameRootButDiffVolume;
        script->IsCopyOperation = FALSE;
        // script->IsCopyOrMoveOperation = TRUE;   // commented out because we do not want to add this Move to the queue of Copy/Move operations

        BOOL fastDirectoryMove = TRUE;          // Configuration.FastDirectoryMove;
        if (fastDirectoryMove &&                // fast-dir-move is not globally turned off
            HasTheSameRootPath(source, target)) // + within one drive
        {
            UINT sourceType = DRIVE_REMOTE;
            if (source[0] != '\\') // it is not a UNC path (that is always "remote")
            {
                char root[4] = " :\\";
                root[0] = source[0];
                sourceType = GetDriveType(root);
            }

            if (sourceType == DRIVE_REMOTE) // network disk
            {                               // we will perform detection of NOVELL disks - fast-directory-move does not work on them
                if (IsNOVELLDrive(source))
                    fastDirectoryMove = Configuration.NetwareFastDirMove;
            }
        }

        //--- initialization of the test for interrupting the build
        LastTickCount = GetTickCount();

        //--- enumeration of files/directories in the source directory
        char sourceDir[MAX_PATH + 4];
        int len = (int)strlen(source);
        if (source[len - 1] == '\\')
            len--;
        memcpy(sourceDir, source, len);
        sourceDir[len++] = '\\';
        strcpy(sourceDir + len, "*");

        WIN32_FIND_DATA file;
        HANDLE find = HANDLES_Q(FindFirstFile(sourceDir, &file));
        if (find == INVALID_HANDLE_VALUE)
        {
            FreeScript(script);
            FilesActionInProgress = FALSE;
            SetCurrentDirectoryToSystem();
            return FALSE;
        }
        else
        {
            sourceDir[len] = 0;

            CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help

            char targetDir[MAX_PATH];
            strcpy(targetDir, target);

            BOOL sourceSupADS = IsPathOnVolumeSupADS(sourceDir, NULL);
            BOOL targetIsFAT32 /*, targetSupEFS*/;
            BOOL targetSupADS = IsPathOnVolumeSupADS(targetDir, &targetIsFAT32);
            CTargetPathState targetPathState = GetTargetPathState(tpsUnknown, targetDir);
            DWORD srcAndTgtPathsFlags = GetPathFlagsForCopyOp(sourceDir, OPFL_SRCPATH_IS_NET, OPFL_SRCPATH_IS_FAST) |
                                        GetPathFlagsForCopyOp(targetDir, OPFL_TGTPATH_IS_NET, OPFL_TGTPATH_IS_FAST);

            script->TargetPathSupADS = targetSupADS;
            //      script->TargetPathSupEFS = targetSupEFS;

            DWORD d1, d2, d3, d4;
            if (MyGetDiskFreeSpace(targetDir, &d1, &d2, &d3, &d4))
            {
                script->BytesPerCluster = d1 * d2;
                // W2K and newer: the product of d1 * d2 * d3 did not work on DFS trees, reported by Ludek.Vydra@k2atmitec.cz
                script->FreeSpace = MyGetDiskFreeSpace(targetDir);
            }

            BOOL scriptOK = TRUE; // result of the script building, success?
            do
            {
                if (file.cFileName[0] != 0 &&
                    (file.cFileName[0] != '.' ||
                     (file.cFileName[1] != 0 && (file.cFileName[1] != '.' || file.cFileName[2] != 0))))
                {
                    if (file.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                    {
                        if (!BuildScriptDir(script, atMove, sourceDir, sourceSupADS, targetDir,
                                            targetPathState, targetSupADS, targetIsFAT32, NULL,
                                            file.cFileName, NULL, NULL, NULL, file.dwFileAttributes, NULL,
                                            TRUE, FALSE, fastDirectoryMove, NULL, NULL, &file.ftLastWriteTime,
                                            srcAndTgtPathsFlags))
                        {
                            scriptOK = FALSE;
                            break;
                        }
                    }
                    else
                    {
                        if (!BuildScriptFile(script, atMove, sourceDir, sourceSupADS, targetDir,
                                             targetPathState, targetSupADS, targetIsFAT32, NULL,
                                             file.cFileName, NULL,
                                             CQuadWord(file.nFileSizeLow, file.nFileSizeHigh),
                                             NULL, NULL, file.dwFileAttributes, NULL, FALSE, NULL,
                                             srcAndTgtPathsFlags))
                        {
                            scriptOK = FALSE;
                            break;
                        }
                    }
                }
            } while (FindNextFile(find, &file));
            HANDLES(FindClose(find));
            int i;
            for (i = 0; i < script->Count; i++)
                script->TotalSize += script->At(i).Size;
            SetCursor(oldCur);
            DestroySafeWaitWindow();
            // the script is compiled, let's run it
            if (script->Count != 0)
            {
                CProgressDialog dlg(HWindow, script, LoadStr(IDS_UNPACKTMPMOVE), NULL, NULL, FALSE, NULL);
                int res = 0;
                if (!scriptOK || (res = (int)dlg.Execute()) == IDABORT || res == 0 || res == -1)
                {
                    UpdateWindow(MainWindow->HWindow);
                    if (!script->IsGood())
                        script->ResetState();
                    FreeScript(script);
                    FilesActionInProgress = FALSE;
                    SetCurrentDirectoryToSystem();
                    return FALSE;
                }
                else
                    UpdateWindow(MainWindow->HWindow);
            }
            else
            {
                FreeScript(script);
                UpdateWindow(MainWindow->HWindow);
            }
            FilesActionInProgress = FALSE;
            SetCurrentDirectoryToSystem();
            return TRUE;
        }
    }
    return FALSE;
}

BOOL ContainsString(TIndirectArray<char>* usedNames, const char* name, int* index)
{
    CALL_STACK_MESSAGE_NONE
    if (usedNames != NULL)
    {
        if (usedNames->Count == 0)
        {
            if (index != NULL)
                *index = 0;
            return FALSE;
        }

        int l = 0, r = usedNames->Count - 1, m;
        while (1)
        {
            m = (l + r) / 2;
            char* hw = usedNames->At(m);
            int res = StrICmp(hw, name);
            if (res == 0) // found
            {
                if (index != NULL)
                    *index = m;
                return TRUE;
            }
            else
            {
                if (res > 0)
                {
                    if (l == r || l > m - 1) // not found
                    {
                        if (index != NULL)
                            *index = m; // should be at this position
                        return FALSE;
                    }
                    r = m - 1;
                }
                else
                {
                    if (l == r) // not found
                    {
                        if (index != NULL)
                            *index = m + 1; // should be after this position
                        return FALSE;
                    }
                    l = m + 1;
                }
            }
        }
    }
    return FALSE;
}

void AddStringToNames(TIndirectArray<char>* usedNames, const char* txt)
{
    CALL_STACK_MESSAGE_NONE
    char* str = DupStr(txt);
    if (str != NULL)
    {
        int index;
        if (ContainsString(usedNames, str, &index))
        {
            TRACE_E("Unexpected situation in AddStringToNames().");
            free(str);
        }
        else
        {
            usedNames->Insert(index, str);
            if (!usedNames->IsGood())
            {
                free(str);
                usedNames->ResetState();
            }
        }
    }
}

BOOL CFilesWindow::BuildScriptMain2(COperations* script, BOOL copy, char* targetDir,
                                    CCopyMoveData* data)
{
    CALL_STACK_MESSAGE3("CFilesWindow::BuildScriptMain2(, %d, %s, )", copy, targetDir);
    if (!script->IsGood())
        return FALSE;
    script->CompressedSize = CQuadWord(0, 0);
    script->ClearReadonlyMask = 0xFFFFFFFF;
    script->TotalSize = CQuadWord(0, 0);
    script->OccupiedSpace = CQuadWord(0, 0);
    script->TotalFileSize = CQuadWord(0, 0);

    ConfirmADSLossAll = FALSE;
    ConfirmADSLossSkipAll = FALSE;
    ConfirmCopyLinkContentAll = FALSE;
    ConfirmCopyLinkContentSkipAll = FALSE;
    ErrReadingADSIgnoreAll = FALSE;
    ErrFileSkipAll = FALSE;
    ErrTooLongNameSkipAll = FALSE;
    ErrTooLongDirNameSkipAll = FALSE;
    ErrTooLongTgtNameSkipAll = FALSE;
    ErrTooLongTgtDirNameSkipAll = FALSE;
    ErrTooLongSrcDirNameSkipAll = FALSE;
    ErrListDirSkipAll = FALSE;
    ErrTooBigFileFAT32SkipAll = FALSE;
    ErrGetFileSizeOfLnkTgtIgnAll = FALSE;

    char root[MAX_PATH];
    char fsName[MAX_PATH];
    DWORD dummy, flags;

    //--- initialization of the test for interrupting the build
    LastTickCount = GetTickCount();

    BOOL fastDirectoryMove = TRUE; // Configuration.FastDirectoryMove;
    if (data->Count > 0)
    {
        char* name = data->At(0)->FileName;
        UINT sourceType = DRIVE_REMOTE;
        if (name != NULL && LowerCase[*name] >= 'a' && LowerCase[*name] <= 'z' &&
            *(name + 1) == ':') // not a UNC path (that is always "remote")
        {
            sourceType = MyGetDriveType(name);
        }
        if (name != NULL)
        {
            if (sourceType == DRIVE_REMOTE || sourceType == DRIVE_REMOVABLE)
            {
                GetRootPath(root, name);
                SetCurrentDirectory(root);
            }
            else
                SetCurrentDirectory(targetDir);

            if (fastDirectoryMove &&                   // fast-dir-move is not globally turned off
                !copy && sourceType == DRIVE_REMOTE && // + move operation + network disk
                HasTheSameRootPath(name, targetDir))   // + within one drive
            {                                          // we will perform detection of NOVELL disks - fast-directory-move does not work on them
                if (IsNOVELLDrive(name))
                    fastDirectoryMove = Configuration.NetwareFastDirMove;
            }

            if (sourceType == DRIVE_REMOVABLE)
                script->RemovableSrcDisk = TRUE;

            if (Configuration.ClearReadOnly)
            {
                if (sourceType == DRIVE_REMOTE)
                {
                    GetRootPath(root, name);
                    fsName[0] = 0;
                    if (GetVolumeInformation(root, NULL, 0, NULL, &dummy, &flags, fsName, MAX_PATH) &&
                        StrICmp(fsName, "CDFS") == 0)
                    {
                        script->ClearReadonlyMask = ~(FILE_ATTRIBUTE_READONLY);
                    }
                }
                else
                {
                    if (sourceType == DRIVE_CDROM)
                        script->ClearReadonlyMask = ~(FILE_ATTRIBUTE_READONLY);
                }
            }
        }
    }

    // Check if the target is a removable medium (floppy, ZIP) -> a larger buffer is used for faster operation
    if (LowerCase[*targetDir] >= 'a' && LowerCase[*targetDir] <= 'z' &&
        *(targetDir + 1) == ':')
    {
        char root2[4] = " :\\";
        root2[0] = targetDir[0];
        UINT targetType = GetDriveType(root2);

        if (targetType == DRIVE_REMOVABLE)
            script->RemovableTgtDisk = TRUE;
    }

    CActionType type = (copy ? atCopy : atMove);
    char sourcePath[2 * MAX_PATH];     // + MAX_PATH is a reserve (Windows makes paths longer than MAX_PATH)
    char lastSourcePath[2 * MAX_PATH]; // + MAX_PATH is a reserve (Windows makes paths longer than MAX_PATH)
    lastSourcePath[0] = 0;
    BOOL sourceSupADS = FALSE;
    char targetPath[2 * MAX_PATH + 200]; // + 200 is a reserve (Windows makes paths longer than MAX_PATH)
    char mapNameBuf[2 * MAX_PATH];       // + MAX_PATH is a reserve (Windows makes paths longer than MAX_PATH)
    strcpy(targetPath, targetDir);
    SalPathAddBackslash(targetPath, 2 * MAX_PATH);
    BOOL targetIsFAT32 /*, targetSupEFS*/;
    BOOL targetSupADS = IsPathOnVolumeSupADS(targetPath, &targetIsFAT32);
    script->TargetPathSupADS = targetSupADS;
    DWORD srcAndTgtPathsFlags = GetPathFlagsForCopyOp(targetPath, OPFL_TGTPATH_IS_NET, OPFL_TGTPATH_IS_FAST);
    //  script->TargetPathSupEFS = targetSupEFS;
    CTargetPathState targetPathState = GetTargetPathState(tpsUnknown, targetPath);
    char* targetName = targetPath + strlen(targetPath);
    BOOL makeCopyOfName = data->MakeCopyOfName;
    TIndirectArray<char>* usedNames = NULL; // list of all newly created names (uses makeCopyOfName==TRUE)
    if (makeCopyOfName)
        usedNames = new TIndirectArray<char>(100, 50);

    DWORD d1, d2, d3, d4;
    if (MyGetDiskFreeSpace(targetPath, &d1, &d2, &d3, &d4))
    {
        script->BytesPerCluster = d1 * d2;
        // W2K and newer: the product of d1 * d2 * d3 did not work on DFS trees, reported by Ludek.Vydra@k2atmitec.cz
        script->FreeSpace = MyGetDiskFreeSpace(targetPath);
    }

    int i;
    for (i = 0; i < data->Count; i++)
    {
        char* fileName = data->At(i)->FileName;
        char* mapName = data->At(i)->MapName;

        DWORD attrs = SalGetFileAttributes(fileName);
        if (attrs != 0xFFFFFFFF)
        {
            char* s = fileName + strlen(fileName);
            while (--s >= fileName && *s != '\\')
                ;
            if (s > fileName)
            {
                memcpy(sourcePath, fileName, s - fileName);
                sourcePath[s - fileName] = 0;
                if (StrICmp(lastSourcePath, sourcePath) != 0)
                {
                    memcpy(lastSourcePath, fileName, s - fileName + 1);
                    lastSourcePath[s - fileName + 1] = 0;
                    sourceSupADS = IsPathOnVolumeSupADS(lastSourcePath, NULL);
                    srcAndTgtPathsFlags &= ~(OPFL_SRCPATH_IS_NET | OPFL_SRCPATH_IS_FAST);
                    srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(lastSourcePath, OPFL_SRCPATH_IS_NET, OPFL_SRCPATH_IS_FAST);
                    lastSourcePath[s - fileName] = 0;
                }
                if (IsTheSamePath(sourcePath, targetPath) && // "Copy of..." is only done when the paths match
                    makeCopyOfName)                          // test if it will be necessary to make a "Copy of..." name
                {
                    strcpy(targetName, s + 1); // put the proposed target full name into targetPath
                    BOOL isKnown;
                    // mapName must be NULL here, otherwise data->MakeCopyOfName could not be TRUE
                    if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                        SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                    { // name already exists, we need to generate a new one
                        if (!isKnown)
                            AddStringToNames(usedNames, targetName);
                        char copyTxt[100];
                        lstrcpyn(copyTxt, LoadStr(IDS_NEWNAME_COPY), 100);
                        char ofTxt[100];
                        lstrcpyn(ofTxt, LoadStr(IDS_NEWNAME_OF), 100);
                        char copyOpenPar[100];
                        lstrcpyn(copyOpenPar, copyTxt, 98);
                        lstrcpyn(copyOpenPar + strlen(copyOpenPar), " (", 100 - (int)strlen(copyOpenPar));
                        char hyphenCopy[100];
                        strcpy(hyphenCopy, " - ");
                        lstrcpyn(hyphenCopy + strlen(hyphenCopy), copyTxt, 100 - (int)strlen(hyphenCopy));
                        char number[20];
                        int val = 0;
                        if (WindowsVistaAndLater)
                        {
                            int len = (int)strlen(targetName);
                            char* ext = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? NULL : strrchr(s + 1, '.'); // directories do not have extensions (copied behavior of Vista)
                            if (ext != NULL && strchr(ext, ' ') != NULL)
                                ext = NULL; // Extension with space is not an extension (copied behavior of Vista)
                            char* numBeg = s + 1;
                            char* numEnd = NULL;
                            while (1)
                            {
                                while (*numBeg != 0 && *numBeg != '(')
                                    numBeg++;
                                if (*numBeg == '(')
                                {
                                    numEnd = numBeg + 1;
                                    while (*numEnd >= '0' && *numEnd <= '9')
                                        numEnd++;
                                    if (*numEnd != ')')
                                        numBeg = numEnd;
                                    else
                                    {
                                        numEnd++;
                                        break; // "(number)" found
                                    }
                                }
                                else
                                {
                                    numBeg = NULL;
                                    break; // "(number)" is not there
                                }
                            }

                        _VISTA_NEXT_1: // "name - Copy.ext" and "name - Copy (++val).ext"

                            if (++val > 1 && numBeg != NULL)
                            {
                                sprintf(number, "(%d)", val);
                                if (ext != NULL)
                                {
                                    if (numBeg < ext)
                                    {
                                        lstrcpyn(targetName + (numBeg - (s + 1)), number, (int)(1 + MAX_PATH - (numBeg - (s + 1))));                                 // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), numEnd, (int)min((DWORD)((ext - numEnd) + 1), 1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), hyphenCopy, (int)(1 + MAX_PATH - strlen(targetName)));                             // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), ext, (int)(1 + MAX_PATH - strlen(targetName)));                                    // "1 +" so that the result is == MAX_PATH for a too long name
                                    }
                                    else
                                    {
                                        lstrcpyn(targetName + (ext - (s + 1)), hyphenCopy, (int)(1 + MAX_PATH - (ext - (s + 1))));                                // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), ext, (int)min((DWORD)((numBeg - ext) + 1), 1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), number, (int)(1 + MAX_PATH - strlen(targetName)));                              // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), numEnd, (int)(1 + MAX_PATH - strlen(targetName)));                              // "1 +" so that the result is == MAX_PATH for a too long name
                                    }
                                }
                                else
                                {
                                    lstrcpyn(targetName + (numBeg - (s + 1)), number, (int)(1 + MAX_PATH - (numBeg - (s + 1))));     // "1 +" so that the result is == MAX_PATH for a too long name
                                    lstrcpyn(targetName + strlen(targetName), numEnd, (int)(1 + MAX_PATH - strlen(targetName)));     // "1 +" so that the result is == MAX_PATH for a too long name
                                    lstrcpyn(targetName + strlen(targetName), hyphenCopy, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                                }
                            }
                            else
                            {
                                if (ext == NULL)
                                    lstrcpyn(targetName + len, hyphenCopy, 1 + MAX_PATH - len); // "1 +" so that the result is == MAX_PATH for a too long name
                                else
                                    lstrcpyn(targetName + (ext - (s + 1)), hyphenCopy, (int)(1 + MAX_PATH - (ext - (s + 1)))); // "1 +" so that the result is == MAX_PATH for a too long name
                                if (val > 1)
                                {
                                    sprintf(number, " (%d)", val);
                                    lstrcpyn(targetName + strlen(targetName), number, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                                }
                                if (ext != NULL)
                                    lstrcpyn(targetName + strlen(targetName), ext, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                            }

                            if (strlen(targetName) < MAX_PATH) // Composition of the name went okay, otherwise we don't care about it
                            {
                                if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                                    SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                                {
                                    if (!isKnown)
                                        AddStringToNames(usedNames, targetName);
                                    goto _VISTA_NEXT_1;
                                }
                                else
                                {
                                    strcpy(mapNameBuf, targetName);
                                    mapName = mapNameBuf;
                                }
                            }
                        }
                        else
                        {
                            if (strncmp(s + 1, copyOpenPar, strlen(copyOpenPar)) == 0)
                            {
                                char* num = s + 1 + strlen(copyOpenPar);
                                while (*num >= '0' && *num <= '9')
                                    num++;
                                if (*num == ')')
                                {
                                    val = 1;

                                _NEXT_1: // type "copy (++val)*"

                                    lstrcpyn(targetName, copyOpenPar, MAX_PATH);
                                    sprintf(number, "%d)", ++val);
                                    lstrcpyn(targetName + strlen(targetName), number, (int)(MAX_PATH - strlen(targetName)));
                                    lstrcpyn(targetName + strlen(targetName), num + 1, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" so that the result is == MAX_PATH for a too long name
                                    if (strlen(targetName) < MAX_PATH)                                                            // Composition of the name went okay, otherwise we don't care about it
                                    {
                                        if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                                            SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                                        {
                                            if (!isKnown)
                                                AddStringToNames(usedNames, targetName);
                                            goto _NEXT_1;
                                        }
                                        else
                                        {
                                            strcpy(mapNameBuf, targetName);
                                            mapName = mapNameBuf;
                                        }
                                    }
                                }
                                else
                                {
                                    val = 1;
                                    int len = (int)strlen(targetName);
                                    char* ext = strrchr(num, '.');
                                    char* num2 = ext == NULL ? (s + 1 + len - 1) : (ext - 1);
                                    if (num2 > s && *num2 == ')')
                                    {
                                        while (--num2 > s && *num2 >= '0' && *num2 <= '9')
                                            ;
                                        if (num2 > s && *num2 == '(')
                                        {
                                            if (num2 - 1 > s && *(num2 - 1) == ' ')
                                                num2--;
                                        }
                                        else
                                            num2 = NULL;
                                    }
                                    else
                                        num2 = NULL;

                                _NEXT_2: // type "* (++val)"

                                    sprintf(number, " (%d)", ++val);
                                    if (ext == NULL)
                                    {
                                        if (num2 == NULL)
                                            lstrcpyn(targetName + len, number, 1 + MAX_PATH - len); // "1 +" so that the result is == MAX_PATH for a too long name
                                        else
                                            lstrcpyn(targetName + (num2 - (s + 1)), number, (int)(1 + MAX_PATH - (num2 - (s + 1)))); // "1 +" so that the result is == MAX_PATH for a too long name
                                    }
                                    else
                                    {
                                        if (num2 == NULL)
                                            lstrcpyn(targetName + (ext - (s + 1)), number, (int)(1 + MAX_PATH - (ext - (s + 1)))); // "1 +" so that the result is == MAX_PATH for a too long name
                                        else
                                            lstrcpyn(targetName + (num2 - (s + 1)), number, (int)(1 + MAX_PATH - (num2 - (s + 1)))); // "1 +" so that the result is == MAX_PATH for a too long name
                                        lstrcpyn(targetName + strlen(targetName), ext, 1 + MAX_PATH - (int)strlen(targetName));      // "1 +" so that the result is == MAX_PATH for a too long name
                                    }
                                    if (strlen(targetName) < MAX_PATH) // Composition of the name went okay, otherwise we don't care about it
                                    {
                                        if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                                            SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                                        {
                                            if (!isKnown)
                                                AddStringToNames(usedNames, targetName);
                                            goto _NEXT_2;
                                        }
                                        else
                                        {
                                            strcpy(mapNameBuf, targetName);
                                            mapName = mapNameBuf;
                                        }
                                    }
                                }
                            }
                            else
                            {
                            _NEXT_3: // type "copy of *", then "copy (++val) of *"

                                lstrcpyn(targetName, copyTxt, MAX_PATH);
                                lstrcpyn(targetName + strlen(targetName), " ", MAX_PATH - (int)strlen(targetName));
                                if (++val > 1)
                                {
                                    sprintf(number, "(%d) ", val);
                                    lstrcpyn(targetName + strlen(targetName), number, MAX_PATH - (int)strlen(targetName));
                                }
                                if (ofTxt[0] != ' ' || ofTxt[1] != 0)
                                {
                                    lstrcpyn(targetName + strlen(targetName), ofTxt, MAX_PATH - (int)strlen(targetName));
                                    lstrcpyn(targetName + strlen(targetName), " ", MAX_PATH - (int)strlen(targetName));
                                }
                                lstrcpyn(targetName + strlen(targetName), s + 1, 1 + MAX_PATH - (int)strlen(targetName)); // "1 +" so that the result is == MAX_PATH for a too long name
                                if (strlen(targetName) < MAX_PATH)                                                        // Composition of the name went okay, otherwise we don't care about it
                                {
                                    if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                                        SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                                    {
                                        if (!isKnown)
                                            AddStringToNames(usedNames, targetName);
                                        goto _NEXT_3;
                                    }
                                    else
                                    {
                                        strcpy(mapNameBuf, targetName);
                                        mapName = mapNameBuf;
                                    }
                                }
                            }
                        }
                    }
                    *targetName = 0; // reconstruction of targetPath

                    // we will save all the names of newly created files
                    if (usedNames != NULL)
                    {
                        AddStringToNames(usedNames, mapName == NULL ? s + 1 : mapName);
                    }
                }

                if (attrs & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!BuildScriptDir(script, type, sourcePath, sourceSupADS, targetPath,
                                        targetPathState, targetSupADS, targetIsFAT32, NULL,
                                        s + 1, NULL, NULL, mapName, attrs, NULL, TRUE, TRUE,
                                        fastDirectoryMove, NULL, NULL, NULL, srcAndTgtPathsFlags))
                    {
                        SetCurrentDirectoryToSystem();
                        if (usedNames != NULL)
                            delete usedNames;
                        return FALSE;
                    }
                }
                else
                {
                    HANDLE h = HANDLES_Q(CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                    NULL, OPEN_EXISTING,
                                                    FILE_ATTRIBUTE_NORMAL, NULL));
                    DWORD err = NO_ERROR;
                    if (h != INVALID_HANDLE_VALUE)
                    {
                        CQuadWord size;
                        BOOL haveSize = SalGetFileSize(h, size, err);
                        HANDLES(CloseHandle(h));
                        if (haveSize)
                        {
                            err = NO_ERROR;
                            if (!BuildScriptFile(script, type, sourcePath, sourceSupADS, targetPath,
                                                 targetPathState, targetSupADS, targetIsFAT32, NULL,
                                                 s + 1, NULL, size, NULL, mapName, attrs, NULL, TRUE,
                                                 NULL, srcAndTgtPathsFlags))
                            {
                                SetCurrentDirectoryToSystem();
                                if (usedNames != NULL)
                                    delete usedNames;
                                return FALSE;
                            }
                        }
                        else
                        {
                            if (err == NO_ERROR)
                                err = ERROR_ACCESS_DENIED; // We need to report some error
                        }
                    }
                    else
                        err = GetLastError();
                    if (err != NO_ERROR)
                    {
                        char message[MAX_PATH + 100];
                        sprintf(message, LoadStr(IDS_FILEERRORFORMAT), fileName, GetErrorText(err));
                        SetCurrentDirectoryToSystem();
                        SalMessageBox(HWindow, message, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        if (usedNames != NULL)
                            delete usedNames;
                        return FALSE;
                    }
                }
            }
            else
            {
                char message[MAX_PATH + 100];
                sprintf(message, LoadStr(IDS_FILEERRORFORMAT), fileName,
                        GetErrorText(ERROR_INVALID_DATA));
                SetCurrentDirectoryToSystem();
                SalMessageBox(HWindow, message, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                if (usedNames != NULL)
                    delete usedNames;
                return FALSE;
            }
        }
        else
        {
            char message[MAX_PATH + 100];
            sprintf(message, LoadStr(IDS_FILEERRORFORMAT), fileName,
                    GetErrorText(GetLastError()));
            //      SetCurrentDirectoryToSystem();
            SalMessageBox(HWindow, message, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            //      if (usedNames != NULL) delete usedNames;
            //      return FALSE;
        }
    }
    if (usedNames != NULL)
        delete usedNames;

    SetCurrentDirectoryToSystem();

    script->TotalSize = CQuadWord(0, 0);
    for (i = 0; i < script->Count; i++)
        script->TotalSize += script->At(i).Size;
    return TRUE;
}

void CFilesWindow::DropCopyMove(BOOL copy, char* targetPath, CCopyMoveData* data)
{
    CALL_STACK_MESSAGE3("CFilesWindow::DropCopyMove(%d, %s, )", copy, targetPath);
    if (!FilesActionInProgress)
    {
        FilesActionInProgress = TRUE;
        SetForegroundWindow(MainWindow->HWindow); // must be activated immediately after drop
        BeginStopRefresh();                       // otherwise WM_ACTIVATEAPP comes, which does not activate...
        COperations* script = new COperations(100, 50, NULL, NULL, NULL);
        if (script == NULL)
            TRACE_E(LOW_MEMORY);
        else
        {
            if (!copy && data->Count > 0)
            {
                char source[MAX_PATH];
                lstrcpyn(source, data->At(0)->FileName, MAX_PATH);
                CutDirectory(source);
                BOOL sameRootPath = HasTheSameRootPath(source, targetPath);
                script->SameRootButDiffVolume = sameRootPath && !HasTheSameRootPathAndVolume(source, targetPath);
                script->ShowStatus = !sameRootPath || script->SameRootButDiffVolume;
            }
            if (copy)
                script->ShowStatus = TRUE;
            script->IsCopyOperation = copy;
            script->IsCopyOrMoveOperation = TRUE;

            char caption[50]; // otherwise there is a transfer of the LoadStr buffer before copying it to the local buffer of the dialog
            if (copy)
                lstrcpyn(caption, LoadStr(IDS_COPY), 50);
            else
                lstrcpyn(caption, LoadStr(IDS_MOVE), 50);

            HWND hFocusedWnd = GetFocus();
            CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
            EnableWindow(MainWindow->HWindow, FALSE);

            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            BOOL res = BuildScriptMain2(script, copy, targetPath, data);

            // Swapped to enable the main window activation (must not be disabled), otherwise switches to another app
            EnableWindow(MainWindow->HWindow, TRUE);
            DestroySafeWaitWindow();

            // if Salamander is active, we call SetFocus on the remembered window (SetFocus does not work)
            // if the main window is disabled - after deactivation/activation of the disabled main window, the active panel
            // no focus)
            HWND hwnd = GetForegroundWindow();
            while (hwnd != NULL && hwnd != MainWindow->HWindow)
                hwnd = GetParent(hwnd);
            if (hwnd == MainWindow->HWindow)
                SetFocus(hFocusedWnd);

            SetCursor(oldCur);

            BOOL cancel = FALSE;
            if (res)
            {
                BOOL occupiedSpTooBig = script->OccupiedSpace != CQuadWord(0, 0) &&
                                        script->BytesPerCluster != 0 && // we have information about the disk
                                        script->OccupiedSpace > script->FreeSpace &&
                                        !IsSambaDrivePath(targetPath); // Samba returns nonsensical cluster-size, so we can only rely on TotalFileSize
                if (occupiedSpTooBig ||
                    script->BytesPerCluster != 0 && // we have information about the disk
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

            // Prepare a refresh of non-automatically refreshed directories
            // change in the target directory and its subdirectories
            script->SetWorkPath1(targetPath, TRUE);
            if (!copy) // move operation changes the source of the operation
            {
                if (data->Count > 0)
                {
                    char* name = data->At(0)->FileName;
                    if (name != NULL)
                    {
                        char path[MAX_PATH];
                        lstrcpyn(path, name, MAX_PATH);
                        if (CutDirectory(path)) // we assume one source directory (only operations from the panel, not from Find)
                        {
                            // change in the source directory and subdirectories
                            script->SetWorkPath2(path, TRUE);
                        }
                    }
                }
            }

            if (cancel || !res || !StartProgressDialog(script, caption, NULL, NULL))
            {
                UpdateWindow(MainWindow->HWindow);
                if (!script->IsGood())
                    script->ResetState();
                FreeScript(script);
            }
            else
            {
                UpdateWindow(MainWindow->HWindow);
            }
        }
        //--- if any Salamander window has been activated, end the suspend mode
        EndStopRefresh();
        FilesActionInProgress = FALSE;
    }
}

BOOL CFilesWindow::BuildScriptMain(COperations* script, CActionType type,
                                   char* targetPath, char* mask, int selCount,
                                   int* selection, CFileData* oneFile,
                                   CAttrsData* attrsData, CChangeCaseData* chCaseData,
                                   BOOL onlySize, CCriteriaData* filterCriteria)
{
    CALL_STACK_MESSAGE5("CFilesWindow::BuildScriptMain(, %d, %s, %s, %d, , , , , ,)",
                        type, targetPath, mask, selCount);
    // count == 0, selection == NULL => oneFile points to the current file
    // otherwise selection contains indexes of marked count items in the filebox
    if (!script->IsGood())
        return FALSE;
    script->TotalSize = CQuadWord(0, 0);
    script->CompressedSize = CQuadWord(0, 0);
    script->OccupiedSpace = CQuadWord(0, 0);
    script->TotalFileSize = CQuadWord(0, 0);

    ConfirmADSLossAll = FALSE;
    ConfirmADSLossSkipAll = FALSE;
    ConfirmCopyLinkContentAll = FALSE;
    ConfirmCopyLinkContentSkipAll = FALSE;
    ErrReadingADSIgnoreAll = FALSE;
    ErrFileSkipAll = FALSE;
    ErrTooLongNameSkipAll = FALSE;
    ErrTooLongDirNameSkipAll = FALSE;
    ErrTooLongTgtNameSkipAll = FALSE;
    ErrTooLongTgtDirNameSkipAll = FALSE;
    ErrTooLongSrcDirNameSkipAll = FALSE;
    ErrListDirSkipAll = FALSE;
    ErrTooBigFileFAT32SkipAll = FALSE;
    ErrGetFileSizeOfLnkTgtIgnAll = FALSE;

    char root[MAX_PATH];
    char fsName[MAX_PATH];

    //--- initialization of the test for interrupting the build
    LastTickCount = GetTickCount();

    //--- if we copy/move from a CD, we will clear the read-only attribute
    //     at the same time, we will set CurrentDirectory to a slower medium
    BOOL fastDirectoryMove = TRUE; // Configuration.FastDirectoryMove;
    if (type == atCopy || type == atMove)
    {
        UINT sourceType = DRIVE_REMOTE;
        if (GetPath()[0] != '\\') // not a UNC path (that is always "remote")
        {
            sourceType = MyGetDriveType(GetPath());
        }

        if (sourceType == DRIVE_REMOTE || sourceType == DRIVE_REMOVABLE)
        {
            SetCurrentDirectory(GetPath());
        }
        else
            SetCurrentDirectory(targetPath);

        if (sourceType == DRIVE_REMOVABLE)
            script->RemovableSrcDisk = TRUE;

        if (fastDirectoryMove &&                            // fast-dir-move is not globally turned off
            sourceType == DRIVE_REMOTE && type == atMove && // network disk + move operation
            HasTheSameRootPath(GetPath(), targetPath))      // + within one drive
        {                                                   // we will perform detection of NOVELL disks - fast-directory-move does not work on them
            if (IsNOVELLDrive(GetPath()))
                fastDirectoryMove = Configuration.NetwareFastDirMove;
        }

        script->ClearReadonlyMask = 0xFFFFFFFF;
        if (Configuration.ClearReadOnly)
        {
            if (sourceType == DRIVE_REMOTE)
            {
                GetRootPath(root, GetPath());
                DWORD dummy, flags;
                fsName[0] = 0;
                if (GetVolumeInformation(root, NULL, 0, NULL, &dummy, &flags, fsName, MAX_PATH) &&
                    StrICmp(fsName, "CDFS") == 0)
                {
                    script->ClearReadonlyMask = ~(FILE_ATTRIBUTE_READONLY);
                }
            }
            else
            {
                if (sourceType == DRIVE_CDROM)
                    script->ClearReadonlyMask = ~(FILE_ATTRIBUTE_READONLY);
            }
        }

        // Check if the target is a removable medium (floppy, ZIP) -> a larger buffer is used for faster operation
        if (LowerCase[*targetPath] >= 'a' && LowerCase[*targetPath] <= 'z' &&
            *(targetPath + 1) == ':')
        {
            char root2[4] = " :\\";
            root2[0] = targetPath[0];
            UINT targetType = GetDriveType(root2);

            if (targetType == DRIVE_REMOVABLE)
                script->RemovableTgtDisk = TRUE;
        }
    }
    else
        script->ClearReadonlyMask = 0xFFFFFFFF;

    // The mask must not be modified through PrepareMask !!! see MaskName()
    char nameMask[2 * MAX_PATH]; // + MAX_PATH is a reserve (Windows makes paths longer than MAX_PATH)
    if (mask != NULL)
    {
        lstrcpyn(nameMask, mask, 2 * MAX_PATH);
        mask = nameMask;
    }

    // Access to files is much faster in the current directory/disk
    if (type != atMove && type != atCopy)
        SetCurrentDirectory(GetPath());

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help

    char sourcePath[2 * MAX_PATH + 10]; // +reserve for mask ("\\*"), + MAX_PATH is a reserve (Windows make paths longer than MAX_PATH)
    strcpy(sourcePath, GetPath());

    BOOL sourceSupADS = FALSE;
    BOOL targetSupADS = FALSE;
    BOOL targetIsFAT32 = FALSE;
    CTargetPathState targetPathState = tpsUnknown;
    DWORD srcAndTgtPathsFlags = 0;        // Flags only for Copy and Move
    if (type == atMove || type == atCopy) // It doesn't make sense to investigate Copy and Move outside of n
    {
        sourceSupADS = (filterCriteria == NULL || !filterCriteria->IgnoreADS) &&
                       IsPathOnVolumeSupADS(sourcePath, NULL);
        //    BOOL targetSupEFS;
        targetSupADS = IsPathOnVolumeSupADS(targetPath, &targetIsFAT32);
        targetPathState = GetTargetPathState(targetPathState, targetPath);
        script->TargetPathSupADS = targetSupADS;
        //    script->TargetPathSupEFS = targetSupEFS;
        srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(sourcePath, OPFL_SRCPATH_IS_NET, OPFL_SRCPATH_IS_FAST) |
                               GetPathFlagsForCopyOp(targetPath, OPFL_TGTPATH_IS_NET, OPFL_TGTPATH_IS_FAST);
        script->SourcePathIsNetwork = (srcAndTgtPathsFlags & OPFL_SRCPATH_IS_NET) != 0;

        if (filterCriteria != NULL)
        {
            script->OverwriteOlder = filterCriteria->OverwriteOlder;
            script->CopySecurity = filterCriteria->CopySecurity;
            script->PreserveDirTime = filterCriteria->PreserveDirTime;
            script->CopyAttrs = filterCriteria->CopyAttrs;
            script->StartOnIdle = filterCriteria->StartOnIdle;

            if (script->CopySecurity)
            {
                DWORD dummy1, flags;
                char dummy2[MAX_PATH];
                if (MyGetVolumeInformation(targetPath, NULL, NULL, NULL, NULL, 0, NULL, &dummy1, &flags, dummy2, MAX_PATH) &&
                    (flags & FS_PERSISTENT_ACLS) == 0)
                { // He wanted to copy the rights, but they are not supported on the target path, so we'll let him know he's out of luck (API function for set-security doesn't report any errors, just idiots)
                    int res = SalMessageBox(HWindow, LoadStr(IDS_ACLNOTSUPPORTEDONTGTPATH), LoadStr(IDS_QUESTION),
                                            MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED);
                    UpdateWindow(MainWindow->HWindow);
                    if (res == IDNO || res == IDCANCEL) // if CANCEL or NO was entered, we stop
                        return FALSE;
                }
            }
        }
    }

    BOOL subDirectories = ((type != atChangeCase) || chCaseData->SubDirs) && type != atConvert;
    BOOL countSize = (type == atCountSize);
    CQuadWord oldTotalSize;

    char* useName = (oneFile != NULL ? oneFile->Name : NULL);
    char* useDOSName = (oneFile != NULL ? oneFile->DosName : NULL);
    if (type == atDelete && selCount <= 1 && oneFile != NULL && oneFile->DosName != NULL)
    {
        char* s = sourcePath + strlen(sourcePath);
        char* end = s;
        if (s > sourcePath && *(s - 1) != '\\')
            *s++ = '\\';
        strcpy(s, oneFile->Name);
        // Let's try if the file name is valid, if not, we'll try its DOS name
        // (resi soubory dosazitelne jen pres Unicode nebo DOS-jmena)
        if (SalGetFileAttributes(sourcePath) == 0xffffffff)
        {
            DWORD err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME)
            {
                strcpy(s, oneFile->DosName);
                if (SalGetFileAttributes(sourcePath) != 0xffffffff)
                {
                    useName = oneFile->DosName;
                    useDOSName = NULL;
                }
            }
        }
        *end = 0; // reconstruction of sourcePath
    }

    if (selCount > 0 || oneFile != NULL)
    {
        if (type == atMove || type == atCopy) // It doesn't make sense to investigate Copy and Move outside of n
        {
            DWORD d1, d2, d3, d4;
            if (MyGetDiskFreeSpace(targetPath, &d1, &d2, &d3, &d4))
            {
                script->BytesPerCluster = d1 * d2;
                // W2K and newer: the product of d1 * d2 * d3 did not work on DFS trees, reported by Ludek.Vydra@k2atmitec.cz
                script->FreeSpace = MyGetDiskFreeSpace(targetPath);
            }
        }

        int i = 0;
        do
        {
            if (selCount > 1 || oneFile == NULL)
            {
                oneFile = (selection[i] < Dirs->Count) ? &Dirs->At(selection[i]) : &Files->At(selection[i] - Dirs->Count);
                useName = oneFile->Name;
                useDOSName = oneFile->DosName;
            }
            i++;
            // oneFile points to the selected or caret item in the filebox
            if (oneFile->Attr & FILE_ATTRIBUTE_DIRECTORY) // it's about ptDisk
            {
                if (subDirectories)
                {
                    if (countSize)
                    {
                        oldTotalSize = script->TotalSize;
                    }
                    if (!BuildScriptDir(script, type, sourcePath, sourceSupADS, targetPath,
                                        targetPathState, targetSupADS, targetIsFAT32, mask,
                                        useName, useDOSName, attrsData, NULL, oneFile->Attr,
                                        chCaseData, TRUE, onlySize, fastDirectoryMove,
                                        filterCriteria, NULL, &oneFile->LastWrite,
                                        srcAndTgtPathsFlags))
                    {
                        SetCurrentDirectoryToSystem();
                        return FALSE;
                    }
                    if (countSize)
                    {
                        oneFile->SizeValid = 1;
                        oneFile->Size = script->TotalSize - oldTotalSize;
                    }
                }
                else // change-case: marked directories without recurse-sub-dirs
                {    // convert: non-recursive + only applies to files -> nothing to do with directories
                    if (type == atChangeCase)
                    {
                        COperation op;
                        op.OpFlags = 0; // change case = renaming = invalid names will be reported (not just tolerating existing ones)
                        op.Opcode = ocMoveDir;
                        op.Size = MOVE_DIR_SIZE;
                        op.Attr = oneFile->Attr;
                        BOOL skip;
                        if ((op.SourceName = BuildName(sourcePath, oneFile->Name, NULL, &skip,
                                                       &ErrTooLongDirNameSkipAll, sourcePath)) == NULL)
                        {
                            if (!skip)
                            {
                                SetCurrentDirectoryToSystem();
                                return FALSE;
                            }
                        }
                        else
                        {
                            if ((op.TargetName = BuildName(sourcePath, oneFile->Name)) == NULL) // the problem of a name that is too long is already solved by the previous condition
                            {
                                free(op.SourceName);
                                SetCurrentDirectoryToSystem();
                                return FALSE;
                            }
                            int offset = (int)strlen(op.SourceName) - oneFile->NameLen;
                            AlterFileName(op.TargetName + offset, op.SourceName + offset, -1,
                                          chCaseData->FileNameFormat, chCaseData->Change, TRUE);
                            BOOL sameName = strcmp(op.SourceName + offset, op.TargetName + offset) == 0;
                            if (!sameName)
                                script->Add(op);
                            if (sameName || !script->IsGood())
                            {
                                free(op.SourceName);
                                free(op.TargetName);
                                if (!sameName)
                                {
                                    script->ResetState();
                                    SetCurrentDirectoryToSystem();
                                    return FALSE;
                                }
                            }
                        }
                    }
                }
            }
            else
            {
                if (filterCriteria == NULL || filterCriteria->AgreeMasksAndAdvanced(oneFile))
                {
                    if (!BuildScriptFile(script, type, sourcePath, sourceSupADS, targetPath,
                                         targetPathState, targetSupADS, targetIsFAT32, mask,
                                         useName, useDOSName, oneFile->Size, attrsData, NULL,
                                         oneFile->Attr, chCaseData, onlySize, NULL,
                                         srcAndTgtPathsFlags))
                    {
                        SetCurrentDirectoryToSystem();
                        return FALSE;
                    }
                }
            }
        } while (i < selCount);
    }

    SetCurrentDirectoryToSystem();
    int i;
    for (i = 0; i < script->Count; i++)
        script->TotalSize += script->At(i).Size;
    return TRUE;
}

char ADSStreamsGlobalBuf[5000]; // Names of ADS will be inserted into this buffer (separated by commas), it is global to prevent stack overflow during recursion

void GetADSStreamsNames(char* listBuf, int bufSize, char* fileName, BOOL isDir)
{
    if (bufSize > 0)
        listBuf[0] = 0;
    wchar_t** streamNames;
    int streamNamesCount;
    BOOL lowMemory;
    if (CheckFileOrDirADS(fileName, isDir, NULL, &streamNames, &streamNamesCount, &lowMemory,
                          NULL, 0, NULL, NULL) &&
        !lowMemory && streamNames != NULL)
    {
        int size = bufSize;
        char* s = listBuf;
        int i;
        for (i = 0; size > 100 && i < streamNamesCount; i++)
        {
            wchar_t* str = streamNames[i];
            if (str[0] == L':')
                str++;
            wchar_t* end = str;
            while (*end != 0 && *end != L':')
                end++;
            int wr = 0;
            if ((wr = WideCharToMultiByte(CP_ACP, 0, str, (int)(end - str), s, size, NULL, NULL)) == 0)
            {
                *s++ = '?';
                *s = 0;
            }
            size -= wr;
            char* e = s + wr;
            while (s < e)
            {
                if (*s < ' ' && size > 3)
                {
                    memmove(s + 4, s + 1, wr);
                    size -= 3;
                    char buf[10];
                    sprintf(buf, "\\x%02X", (unsigned int)(unsigned char)*s);
                    memcpy(s, buf, 4);
                    s += 3;
                    e += 3;
                }
                s++;
                wr--;
            }
            *s = 0;
            if (size > 2 && i + 1 < streamNamesCount)
            {
                *s++ = ',';
                *s++ = ' ';
                *s = 0;
                size -= 2;
            }
            free(streamNames[i]);
        }
        free(streamNames);
    }

    if (bufSize > 0 && (StrICmp(listBuf, "Zone.Identifier") == 0 || // This stream is created automatically by XP from s.p. 2 and is intended to be ignored, so we won't burden the user with it
                        StrICmp(listBuf, "encryptable") == 0))      // This stream seems to occur only on thumbs.db, no one knows what it is, but windows create it, so we ignore it too
    {
        listBuf[0] = 0;
    }
}

BOOL CFilesWindow::BuildScriptDir(COperations* script, CActionType type, char* sourcePath,
                                  BOOL sourcePathSupADS, char* targetPath,
                                  CTargetPathState targetPathState, BOOL targetPathSupADS,
                                  BOOL targetPathIsFAT32, char* mask, char* dirName,
                                  char* dirDOSName, CAttrsData* attrsData, char* mapName,
                                  DWORD sourceDirAttr, CChangeCaseData* chCaseData, BOOL firstLevelDir,
                                  BOOL onlySize, BOOL fastDirectoryMove, CCriteriaData* filterCriteria,
                                  BOOL* canDelUpperDirAfterMove, FILETIME* sourceDirTime,
                                  DWORD srcAndTgtPathsFlags)
{
    SLOW_CALL_STACK_MESSAGE16("CFilesWindow::BuildScriptDir(, %d, %s, %d, %s, %d, %d, %d, %s, %s, , , %s, 0x%X, , %d, %d, %d, , , , 0x%X)",
                              type, sourcePath, sourcePathSupADS, targetPath,
                              targetPathState, targetPathSupADS, targetPathIsFAT32,
                              mask, dirName, mapName, sourceDirAttr, firstLevelDir, onlySize,
                              fastDirectoryMove, srcAndTgtPathsFlags);
    char text[2 * MAX_PATH + 100];
    char finalName[2 * MAX_PATH + 200];                                      // + 200 is a reserve (Windows makes paths longer than MAX_PATH)
    BOOL sourcePathIsNet = (srcAndTgtPathsFlags & OPFL_SRCPATH_IS_NET) != 0; // valid only for atCopy and atMove

    script->DirsCount++;
    COperation op;
    //--- if it is necessary to create a directory targetPath + dirName (Copy and Move)
    char* sourceEnd = sourcePath + strlen(sourcePath);
    char *st, *s = dirName;
    if (*(sourceEnd - 1) != '\\')
    {
        *sourceEnd = '\\';
        st = sourceEnd + 1;
    }
    else
        st = sourceEnd;
    if (st - sourcePath + strlen(dirName) >= MAX_PATH - 2) // -2 detected experimentally (longer path cannot be listed)
    {                                                      // data are on disk, which does not mean they cannot be longer than MAX_PATH
        *sourceEnd = 0;                                    // restoring sourcePath
        _snprintf_s(text, _TRUNCATE, LoadStr(IDS_NAMEISTOOLONG), dirName, sourcePath);
        BOOL skip = TRUE;
        if (!ErrTooLongSrcDirNameSkipAll)
        {
            MSGBOXEX_PARAMS params;
            memset(&params, 0, sizeof(params));
            params.HParent = HWindow;
            params.Flags = MSGBOXEX_YESNOOKCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
            params.Caption = LoadStr(IDS_ERRORBUILDINGSCRIPT);
            params.Text = text;
            char aliasBtnNames[200];
            /* is used for the export_mnu.py script, which generates salmenu.mnu for Translator
  we will let the collision of hotkeys for message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
{MNTT_PB, 0
{MNTT_IT, IDS_MSGBOXBTN_SKIP
{MNTT_IT, IDS_MSGBOXBTN_SKIPALL
{MNTT_IT, IDS_MSGBOXBTN_FOCUS
{MNTT_PE, 0
};*/
            sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                    DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                    DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL),
                    DIALOG_OK, LoadStr(IDS_MSGBOXBTN_FOCUS));
            params.AliasBtnNames = aliasBtnNames;
            int msgRes = SalMessageBoxEx(&params);
            if (msgRes != DIALOG_YES /* Skip*/ && msgRes != DIALOG_NO /* Skip All*/)
                skip = FALSE;
            if (msgRes == DIALOG_NO /* Skip All*/)
                ErrTooLongSrcDirNameSkipAll = TRUE;
            if (msgRes == DIALOG_OK /* Focus*/)
                MainWindow->PostFocusNameInPanel(PANEL_SOURCE, sourcePath, dirName);
        }
        return skip;
    }
    while (*s != 0)
        *st++ = *s++;
    *st = 0;
    //--- constructing the path to targetDirName
    char* targetEnd = NULL;
    BOOL checkNewDirName = FALSE;
    if (targetPath != NULL)
    {
        int targetLen = (int)strlen(targetPath);
        targetEnd = targetPath + targetLen;
        if (*(targetEnd - 1) != '\\')
        {
            *targetEnd = '\\';
            targetLen++;
        }
        char* s2;
        if (mapName == NULL)
        {
            // Petr: a bit of a mess: mask *.* does not produce a copy of the source name, which is a problem when copying
            // directory with an invalid name, for example "c   ..." + "*.*" = "c   ", so we will help ourselves a bit and let
            // set mask to NULL = primitive textual copy of the name
            char* opMask = mask != NULL && strcmp(mask, "*.*") == 0 ? NULL : mask;
            s2 = MaskName(finalName, 2 * MAX_PATH + 200, dirName, opMask);
            if (opMask != NULL)
                checkNewDirName = strcmp(s2, dirName) != 0;
        }
        else
            s2 = mapName;
        if (strlen(s2) + targetLen >= PATH_MAX_PATH)
        {
            *sourceEnd = 0; // restoring sourcePath
            *targetEnd = 0; // restoring targetPath
            _snprintf_s(text, _TRUNCATE, LoadStr(IDS_TOOLONGNAME2), targetPath, s2);
            BOOL skip = TRUE;
            if (!ErrTooLongTgtDirNameSkipAll)
            {
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = HWindow;
                params.Flags = MSGBOXEX_YESNOOKCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
                params.Caption = LoadStr(IDS_ERRORBUILDINGSCRIPT);
                params.Text = text;
                char aliasBtnNames[200];
                /* is used for the export_mnu.py script, which generates the salmenu.mnu for Translator
   we will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_IT, IDS_MSGBOXBTN_FOCUS
  {MNTT_PE, 0
};*/
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                        DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL),
                        DIALOG_OK, LoadStr(IDS_MSGBOXBTN_FOCUS));
                params.AliasBtnNames = aliasBtnNames;
                int msgRes = SalMessageBoxEx(&params);
                if (msgRes != DIALOG_YES /* Skip*/ && msgRes != DIALOG_NO /* Skip All*/)
                    skip = FALSE;
                if (msgRes == DIALOG_NO /* Skip All*/)
                    ErrTooLongTgtDirNameSkipAll = TRUE;
                if (msgRes == DIALOG_OK /* Focus*/)
                    MainWindow->PostFocusNameInPanel(PANEL_SOURCE, sourcePath, sourceEnd + 1);
            }
            return skip;
        }
        strcpy(targetPath + targetLen, s2);
        targetPathState = GetTargetPathState(targetPathState, targetPath);
    }
    //---
    if (type == atDelete && (sourceDirAttr & FILE_ATTRIBUTE_REPARSE_POINT))
    { // deleting links (volume-mount-points + junction-points + symlinks)
        op.Opcode = ocDeleteDirLink;
        op.OpFlags = 0;
        op.Size = DELETE_DIRLINK_SIZE;
        op.Attr = sourceDirAttr;
        if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
        {
            *sourceEnd = 0; // restoring sourcePath
            return FALSE;
        }
        op.TargetName = NULL;
        *sourceEnd = 0; // restoring sourcePath
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            return FALSE;
        }
        else
            return TRUE;
    }
    //---
    if (type == atDelete && Configuration.CnfrmSHDirDel &&
        (sourceDirAttr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
    {
        sprintf(text, LoadStr(IDS_DELETESHDIR), sourcePath);
        int res = SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_QUESTION),
                                MB_YESNOCANCEL | MB_ICONQUESTION);
        UpdateWindow(MainWindow->HWindow);
        if (res == IDNO || res == IDCANCEL) // if CANCEL or NO was entered, we will either end or skip the directory
        {
            *sourceEnd = 0; // restoring sourcePath
            if (targetEnd != NULL)
                *targetEnd = 0; // restoring targetPath
            return res == IDNO;
        }
    }
    //---
    if (type == atMove)
    {
        if (strcmp(sourcePath, targetPath) == 0) // There's nothing to do, let's close.
        {
            *sourceEnd = 0; // restoring sourcePath
            *targetEnd = 0; // restoring targetPath
            SalMessageBox(MainWindow->HWindow, LoadStr(IDS_CANNOTMOVEDIRTOITSELF),
                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        BOOL sameDisk;
        sameDisk = FALSE;
        if (fastDirectoryMove &&
            !script->CopySecurity && // if the rights are to be copied, it is not possible to move the entire directory (the system needs to refresh "inherited" rights on each file)
            (script->CopyAttrs ||    // When copying attributes, the heuristic is about setting the Encrypted attribute aside
             targetPathState != tpsEncryptedExisting &&
                 targetPathState != tpsEncryptedNotExisting) && // if Encrypted attributes are to be set, it is not possible to move the entire directory (the contents of the directory must be checked)
            (filterCriteria == NULL || !filterCriteria->UseMasks &&
                                           !filterCriteria->UseAdvanced && !filterCriteria->SkipEmptyDirs)) // if files or directories are to be filtered, it is not possible to move the entire directory
        {
            sameDisk = !script->SameRootButDiffVolume &&
                       HasTheSameRootPath(sourcePath, targetPath); // same disk (UNC and normal)
        }
        else
            sameDisk = (StrICmp(sourcePath, targetPath) == 0); // just rename
        if (sameDisk)
        {
            if (StrICmp(sourcePath, targetPath) == 0 ||
                targetPathState == tpsEncryptedNotExisting || targetPathState == tpsNotEncryptedNotExisting) // target directory does not exist
            {
                if (!script->FastMoveUsed)
                    script->FastMoveUsed = TRUE;
                op.Opcode = ocMoveDir;
                op.OpFlags = checkNewDirName ? 0 : OPFL_IGNORE_INVALID_NAME;
                op.Size = MOVE_DIR_SIZE;
                op.Attr = sourceDirAttr;
                if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
                {
                _ERROR:

                    *sourceEnd = 0; // restoring sourcePath
                    *targetEnd = 0; // restoring targetPath
                    return FALSE;
                }
                if ((op.TargetName = BuildName(targetPath, NULL)) == NULL)
                {
                    free(op.SourceName);
                    goto _ERROR;
                }
                *sourceEnd = 0; // restoring sourcePath
                *targetEnd = 0; // restoring targetPath
                script->Add(op);
                if (!script->IsGood())
                {
                    script->ResetState();
                    free(op.SourceName);
                    free(op.TargetName);
                    return FALSE;
                }
                else
                    return TRUE;
            }
        }
    }

    int createDirIndex = -1;
    CQuadWord dirStartTotalFileSize = script->TotalFileSize;
    if (type == atCopy || type == atMove) // creating the target directory
    {
        srcAndTgtPathsFlags &= ~(OPFL_SRCPATH_IS_NET | OPFL_SRCPATH_IS_FAST);
        srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(sourcePath, OPFL_SRCPATH_IS_NET, OPFL_SRCPATH_IS_FAST);
        if (targetPathState == tpsEncryptedExisting || targetPathState == tpsNotEncryptedExisting) // The target directory exists, we will determine its flags (otherwise we will inherit the flags from the parent target directory)
        {
            srcAndTgtPathsFlags &= ~(OPFL_TGTPATH_IS_NET | OPFL_TGTPATH_IS_FAST);
            srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(targetPath, OPFL_TGTPATH_IS_NET, OPFL_TGTPATH_IS_FAST);
        }

        BOOL dirCreated = FALSE;
        if (sourcePathSupADS)
        {
            if ((targetPathState == tpsEncryptedNotExisting || targetPathState == tpsNotEncryptedNotExisting) && // target directory does not exist
                (targetPathSupADS || !ConfirmADSLossAll))                                                        // if ADS should be ignored
            {
                if (script->BytesPerCluster == 0)
                    TRACE_E("How is it possible that script->BytesPerCluster is not yet set???");

                CQuadWord adsSize;
                CQuadWord adsOccupiedSpace;
                DWORD adsWinError;

            READADS_AGAIN:

                if (CheckFileOrDirADS(sourcePath, TRUE, &adsSize, NULL, NULL, NULL, &adsWinError,
                                      script->BytesPerCluster, &adsOccupiedSpace, NULL))
                { // Source directory has ADS, we need to copy them to the target directory
                    if (targetPathSupADS)
                    {
                        script->OccupiedSpace += adsOccupiedSpace;
                        script->TotalFileSize += adsSize;

                        op.Opcode = ocCreateDir;
                        op.OpFlags = OPFL_COPY_ADS | (checkNewDirName ? 0 : OPFL_IGNORE_INVALID_NAME);
                        if (!script->CopyAttrs && // When copying attributes, the heuristic is about setting the Encrypted attribute aside
                            ((sourceDirAttr & FILE_ATTRIBUTE_ENCRYPTED) || targetPathState == tpsEncryptedExisting ||
                             targetPathState == tpsEncryptedNotExisting))
                        {
                            op.OpFlags |= OPFL_AS_ENCRYPTED;
                        }
                        if (type == atMove && !script->ShowStatus)
                            script->ShowStatus = TRUE; // if moving the entire directory is not possible (reasons see above) and it is necessary to copy ADS, it is necessary to display the status
                        op.Size = CREATE_DIR_SIZE + adsSize;
                        op.Attr = sourceDirAttr;
                        if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
                            goto _ERROR;
                        if ((op.TargetName = BuildName(targetPath, NULL)) == NULL)
                        {
                            free(op.SourceName);
                            goto _ERROR;
                        }
                        createDirIndex = script->Add(op);
                        if (!script->IsGood())
                        {
                            free(op.SourceName);
                            free(op.TargetName);
                            script->ResetState();
                            goto _ERROR;
                        }
                        dirCreated = TRUE;
                    }
                    else // Copying to a file system other than NTFS (question about trimming ADS)
                    {
                        int res;
                        if (ConfirmADSLossAll)
                            res = IDYES;
                        else
                        {
                            if (ConfirmADSLossSkipAll)
                                res = IDB_SKIP;
                            else
                            {
                                GetADSStreamsNames(ADSStreamsGlobalBuf, 5000, sourcePath, TRUE);
                                if (ADSStreamsGlobalBuf[0] == 0)
                                    res = IDYES;
                                else
                                {
                                    res = (int)CConfirmADSLossDlg(HWindow, FALSE, sourcePath, ADSStreamsGlobalBuf, type == atMove).Execute();
                                }
                            }
                        }
                        switch (res)
                        {
                        case IDB_ALL:
                            ConfirmADSLossAll = TRUE; // here break; is not missing
                        case IDYES:
                            break; // we will ignore ADS, which will not be copied/moved (resulting in complete loss)

                        case IDB_SKIPALL:
                            ConfirmADSLossSkipAll = TRUE; // here break; is not missing
                        case IDB_SKIP:
                        {
                            *sourceEnd = 0; // restoring sourcePath
                            *targetEnd = 0; // restoring targetPath
                            return TRUE;
                        }

                        case IDCANCEL:
                        {
                            *sourceEnd = 0; // restoring sourcePath
                            *targetEnd = 0; // restoring targetPath
                            return FALSE;
                        }
                        }
                    }
                }
                else // an error occurred or no ADS
                {
                    if (adsWinError != NO_ERROR &&                                                                    // an error occurred
                        (adsWinError != ERROR_INVALID_FUNCTION || StrNICmp(sourcePath, "\\\\tsclient\\", 11) != 0) && // Local disk paths on Terminal Server do not support listing ADS (otherwise ADS are supported, what a comedy)
                        (adsWinError != ERROR_INVALID_PARAMETER && adsWinError != ERROR_NO_MORE_ITEMS ||
                         !sourcePathIsNet)) // A mounted FAT/FAT32 disk cannot be recognized on a network disk (e.g. \\petr\f\drive_c) + NOVELL-NETWARE volume accessed through NDS - we think it is NTFS and therefore we try to read ADS, which reports this error
                    {
                        if ((sourceDirAttr & FILE_ATTRIBUTE_REPARSE_POINT) == 0) // it's not about the link (there is no need to copy the content)
                        {
                            // First, we will try if an error occurs even when listing the directory - such an error
                            // user effort to understand, so we will display it preferentially (before the error of reading ADS)
                            lstrcpyn(finalName, sourcePath, 2 * MAX_PATH + 200);
                            if (SalPathAppend(finalName, "*", 2 * MAX_PATH + 200))
                            {
                                WIN32_FIND_DATA f;
                                HANDLE search = HANDLES_Q(FindFirstFile(finalName, &f));
                                if (search == INVALID_HANDLE_VALUE)
                                {
                                    DWORD err = GetLastError();
                                    if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
                                    {
                                        sprintf(text, LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err));
                                        BOOL skip = TRUE;
                                        if (!ErrListDirSkipAll)
                                        {
                                            MSGBOXEX_PARAMS params;
                                            memset(&params, 0, sizeof(params));
                                            params.HParent = MainWindow->HWindow;
                                            params.Flags = MB_YESNOCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
                                            params.Caption = LoadStr(IDS_ERRORTITLE);
                                            params.Text = text;
                                            char aliasBtnNames[200];
                                            /* Used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   We will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};*/
                                            sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                                                    DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                                                    DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                                            params.AliasBtnNames = aliasBtnNames;
                                            int msgRes = SalMessageBoxEx(&params);
                                            if (msgRes != DIALOG_YES /* Skip*/ && msgRes != DIALOG_NO /* Skip All*/)
                                                skip = FALSE;
                                            if (msgRes == DIALOG_NO /* Skip All*/)
                                                ErrListDirSkipAll = TRUE;
                                        }
                                        *sourceEnd = 0; // restoring sourcePath
                                        *targetEnd = 0; // restoring targetPath
                                        return skip;
                                    }
                                }
                                else
                                    HANDLES(FindClose(search));
                            }
                        }

                        // Listing directory is without any issues (or an unexpected error occurred), we will report the ADS error
                        int res;
                        if (ErrReadingADSIgnoreAll)
                            res = IDB_IGNORE;
                        else
                        {
                            res = (int)CErrorReadingADSDlg(HWindow, sourcePath, GetErrorText(adsWinError)).Execute();
                        }
                        switch (res)
                        {
                        case IDRETRY:
                            goto READADS_AGAIN;

                        case IDB_IGNOREALL:
                            ErrReadingADSIgnoreAll = TRUE; // here break; is not missing
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                            *sourceEnd = 0; // restoring sourcePath
                            *targetEnd = 0; // restoring targetPath
                            return FALSE;
                        }
                        }
                    }
                }
            }
        }
        if (!dirCreated)
        {
            op.Opcode = ocCreateDir;
            op.OpFlags = checkNewDirName ? 0 : OPFL_IGNORE_INVALID_NAME;
            if (!script->CopyAttrs && // When copying attributes, the heuristic is about setting the Encrypted attribute aside
                ((sourceDirAttr & FILE_ATTRIBUTE_ENCRYPTED) || targetPathState == tpsEncryptedExisting ||
                 targetPathState == tpsEncryptedNotExisting))
            {
                op.OpFlags |= OPFL_AS_ENCRYPTED;
            }
            op.Size = CREATE_DIR_SIZE;
            op.Attr = sourceDirAttr;
            if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
                goto _ERROR;
            if ((op.TargetName = BuildName(targetPath, NULL)) == NULL)
            {
                free(op.SourceName);
                goto _ERROR;
            }
            createDirIndex = script->Add(op);
            if (!script->IsGood())
            {
                script->ResetState();
                free(op.SourceName);
                free(op.TargetName);
                goto _ERROR;
            }
        }
    }

    if (type == atChangeAttrs)
    {
        op.Opcode = ocChangeAttrs;
        op.OpFlags = 0;
        op.Size = CHATTRS_FILE_SIZE;
        op.Attr = sourceDirAttr;
        if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
        {
            *sourceEnd = 0; // restoring sourcePath
            return FALSE;
        }
        op.TargetName = (char*)(DWORD_PTR)((sourceDirAttr & attrsData->AttrAnd) | attrsData->AttrOr);
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            *sourceEnd = 0; // restoring sourcePath
            return FALSE;
        }

        if (!attrsData->SubDirs)
        {
            *sourceEnd = 0; // restoring sourcePath
            return TRUE;
        }
    }

    BOOL copyMoveDirIsLink = FALSE;
    BOOL copyMoveSkipLinkContent = FALSE;
    if ((type == atCopy || type == atMove) && // when it comes to the link, we will determine whether to skip or copy the link content
        (sourceDirAttr & FILE_ATTRIBUTE_REPARSE_POINT))
    {
        copyMoveDirIsLink = TRUE;
        int res;
        if (ConfirmCopyLinkContentAll)
            res = IDYES;
        else
        {
            if (ConfirmCopyLinkContentSkipAll)
                res = IDB_SKIP;
            else
            {
                char detailsTxt[MAX_PATH + 200];
                char junctionOrSymlinkTgt[MAX_PATH];
                int repPointType;
                if (GetReparsePointDestination(sourcePath, junctionOrSymlinkTgt, MAX_PATH, &repPointType, FALSE))
                {
                    if (repPointType == 1 /* MOUNT POINT*/)
                        strcpy_s(detailsTxt, LoadStr(IDS_VOLMOUNTPOINT));
                    else
                    {
                        sprintf_s(detailsTxt, LoadStr(repPointType == 2 /* JUNCTION POINT*/ ? IDS_INFODLGTYPE9 : IDS_INFODLGTYPE10),
                                  junctionOrSymlinkTgt);
                        int len = (int)strlen(detailsTxt);
                        if (detailsTxt[0] == '(')
                            memmove(detailsTxt, detailsTxt + 1, --len + 1);
                        if (len > 0 && detailsTxt[len - 1] == ')')
                            detailsTxt[--len] = 0;
                    }
                }
                else
                    strcpy_s(detailsTxt, LoadStr(IDS_UNABLETORESOLVELINK));

                res = (int)CConfirmLinkTgtCopyDlg(HWindow, sourcePath, detailsTxt).Execute();
            }
        }
        switch (res)
        {
        case IDB_ALL:
            ConfirmCopyLinkContentAll = TRUE; // here break; is not missing
        case IDYES:
            break; // We need to copy the content of the link to the target

        case IDB_SKIPALL:
            ConfirmCopyLinkContentSkipAll = TRUE; // here break; is not missing
        case IDB_SKIP:
            copyMoveSkipLinkContent = TRUE;
            break; // we need to skip the content of the link (not copy it)

        case IDCANCEL:
        {
            *sourceEnd = 0; // restoring sourcePath
            *targetEnd = 0; // restoring targetPath
            return FALSE;
        }
        }
    }
    //--- constructing the path to sourceDirName + start of searching for occupied files
    BOOL delDirectory = TRUE;       // delete non-empty directory?
    BOOL delDirectoryReturn = TRUE; // Return value when not deleting a non-empty directory
    BOOL canDelDirAfterMove = TRUE; // only for Move: FALSE = not everything is moved (some files were skipped by the filter), source directory cannot be deleted (it will not remain empty)
    if (!copyMoveDirIsLink || !copyMoveSkipLinkContent)
    {
        WIN32_FIND_DATA f;
        strcpy(st, "\\*");
        HANDLE search = HANDLES_Q(FindFirstFile(sourcePath, &f));
        *st = 0; // removing "\\*"
        if (search == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            if (err == ERROR_PATH_NOT_FOUND && type == atCountSize && dirDOSName != NULL && strcmp(dirName, dirDOSName) != 0)
            { // patch for calculating the size of a directory that needs to be accessed via a DOS name when we cannot handle it via a UNICODE name (the multibyte version of the name after converting back to UNICODE does not match the original UNICODE name of the directory)
                lstrcpyn(finalName, sourcePath, 2 * MAX_PATH + 200);
                if (CutDirectory(finalName) &&
                    SalPathAppend(finalName, dirDOSName, 2 * MAX_PATH + 200) &&
                    SalPathAppend(finalName, "*", 2 * MAX_PATH + 200))
                {
                    search = HANDLES_Q(FindFirstFile(finalName, &f));
                    if (search != INVALID_HANDLE_VALUE)
                    {
                        strcpy(*sourceEnd == '\\' ? sourceEnd + 1 : sourceEnd, dirDOSName); // we will refactor the sourcePath (previously used for working with found files and directories)
                        goto BROWSE_DIR;
                    }
                }
            }
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
            {
                sprintf(text, LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err));
                *sourceEnd = 0; // restoring sourcePath
                if (targetEnd != NULL)
                    *targetEnd = 0; // restoring targetPath
                BOOL skip = TRUE;
                if (!ErrListDirSkipAll)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = MainWindow->HWindow;
                    params.Flags = MB_YESNOCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
                    params.Caption = LoadStr(IDS_ERRORTITLE);
                    params.Text = text;
                    char aliasBtnNames[200];
                    /* Used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   We will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};*/
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                            DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                            DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                    params.AliasBtnNames = aliasBtnNames;
                    int msgRes = SalMessageBoxEx(&params);
                    if (msgRes != DIALOG_YES /* Skip*/ && msgRes != DIALOG_NO /* Skip All*/)
                        skip = FALSE;
                    if (msgRes == DIALOG_NO /* Skip All*/)
                        ErrListDirSkipAll = TRUE;
                }
                if (!skip)
                    return FALSE;
                UpdateWindow(MainWindow->HWindow);
            }
            else
            {
                *sourceEnd = 0; // restoring sourcePath
                if (targetEnd != NULL)
                    *targetEnd = 0; // restoring targetPath
            }
        }
        else
        {

        BROWSE_DIR:

            //--- searching directory
            BOOL askDirDelete = (type == atDelete && firstLevelDir && Configuration.CnfrmNEDirDel);
            BOOL testFindNextErr = TRUE;
            do
            {
                if (f.cFileName[0] == '.' &&
                        (f.cFileName[1] == 0 || (f.cFileName[1] == '.' && f.cFileName[2] == 0)) ||
                    f.cFileName[0] == 0)
                    continue; // "." and ".." + empty names (leads to infinite recursion)

                if (askDirDelete)
                {
                    sprintf(text, LoadStr(IDS_NONEMPTYDIRDELCONFIRM), sourcePath);
                    int res = SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_QUESTION),
                                            MB_YESNOCANCEL | MB_ICONQUESTION);
                    UpdateWindow(MainWindow->HWindow);
                    delDirectoryReturn = (res != IDCANCEL); // if CANCEL was not given, continue
                    delDirectory = (res == IDYES);
                    if (!delDirectory)
                    {
                        testFindNextErr = FALSE;
                        break;
                    }
                    askDirDelete = FALSE;
                }
                //--- Does anyone want to interrupt the build script?
                if (GetTickCount() - LastTickCount > BS_TIMEOUT)
                {
                    if (UserWantsToCancelSafeWaitWindow())
                    {
                        MSG msg; // we discard the buffered ESC
                        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                            ;
                        int topIndex = ListBox->GetTopIndex();
                        int focusIndex = GetCaretIndex();
                        RefreshListBox(-1, topIndex, focusIndex, FALSE, FALSE);
                        int res = SalMessageBox(HWindow, LoadStr(IDS_CANCELOPERATION),
                                                LoadStr(IDS_QUESTION), MB_YESNO | MB_ICONQUESTION);
                        UpdateWindow(MainWindow->HWindow);
                        if (res == IDYES)
                            goto BUILD_ERROR;
                    }

                    LastTickCount = GetTickCount();
                }

                //--- build directory or file
                if (f.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
                {
                    if (!BuildScriptDir(script, copyMoveDirIsLink ? atCopy : type, sourcePath, sourcePathSupADS, targetPath,
                                        targetPathState, targetPathSupADS, targetPathIsFAT32,
                                        NULL, f.cFileName,
                                        f.cAlternateFileName[0] != 0 ? f.cAlternateFileName : NULL,
                                        attrsData, NULL, f.dwFileAttributes, chCaseData, FALSE,
                                        onlySize, fastDirectoryMove, filterCriteria, &canDelDirAfterMove,
                                        &f.ftLastWriteTime, srcAndTgtPathsFlags))
                    {
                    BUILD_ERROR:
                        HANDLES(FindClose(search));
                        *sourceEnd = 0; // restoring sourcePath
                        if (targetEnd != NULL)
                            *targetEnd = 0; // restoring targetPath
                        return FALSE;
                    }
                }
                else
                {
                    if (filterCriteria == NULL || filterCriteria->AgreeMasksAndAdvanced(&f))
                    {
                        if (!BuildScriptFile(script, copyMoveDirIsLink ? atCopy : type, sourcePath, sourcePathSupADS, targetPath,
                                             targetPathState, targetPathSupADS, targetPathIsFAT32,
                                             NULL, f.cFileName,
                                             f.cAlternateFileName[0] != 0 ? f.cAlternateFileName : NULL,
                                             CQuadWord(f.nFileSizeLow, f.nFileSizeHigh), attrsData, NULL,
                                             f.dwFileAttributes, chCaseData, onlySize, &f.ftLastWriteTime,
                                             srcAndTgtPathsFlags))
                            goto BUILD_ERROR;
                    }
                    else
                        canDelDirAfterMove = FALSE; // not everything is moved (filter skipped something), source directory cannot be deleted (will not remain empty)
                }
            } while (FindNextFile(search, &f));
            DWORD err = GetLastError();
            HANDLES(FindClose(search));

            *sourceEnd = 0; // restoring sourcePath
            if (targetEnd != NULL)
                *targetEnd = 0; // restoring targetPath

            if (testFindNextErr && err != ERROR_NO_MORE_FILES)
            {
                sprintf(text, LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err));
                BOOL skip = TRUE;
                if (!ErrListDirSkipAll)
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = MainWindow->HWindow;
                    params.Flags = MB_YESNOCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_DEFBUTTON3 | MSGBOXEX_SILENT;
                    params.Caption = LoadStr(IDS_ERRORTITLE);
                    params.Text = text;
                    char aliasBtnNames[200];
                    /* Used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   We will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};*/
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                            DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                            DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                    params.AliasBtnNames = aliasBtnNames;
                    int msgRes = SalMessageBoxEx(&params);
                    if (msgRes != DIALOG_YES /* Skip*/ && msgRes != DIALOG_NO /* Skip All*/)
                        skip = FALSE;
                    if (msgRes == DIALOG_NO /* Skip All*/)
                        ErrListDirSkipAll = TRUE;
                }
                if (!skip)
                    return FALSE;
                UpdateWindow(MainWindow->HWindow);
            }
        }
    }
    else
    {
        *sourceEnd = 0; // restoring sourcePath
        if (targetEnd != NULL)
            *targetEnd = 0; // restoring targetPath
    }
    //--- change-case: change the name after completing the operations inside
    if (type == atChangeCase)
    {
        op.Opcode = ocMoveDir;
        op.OpFlags = 0; // change case = renaming = invalid names will be reported (not just tolerating existing ones)
        op.Size = MOVE_DIR_SIZE;
        op.Attr = sourceDirAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, dirName, NULL, &skip,
                                       &ErrTooLongDirNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        if ((op.TargetName = BuildName(sourcePath, dirName)) == NULL) // Too long name is not a threat, it would have been resolved in the previous condition
        {
            free(op.SourceName);
            return FALSE;
        }
        int offset = (int)strlen(op.SourceName) - (int)strlen(dirName);
        AlterFileName(op.TargetName + offset, op.SourceName + offset, -1,
                      chCaseData->FileNameFormat, chCaseData->Change, TRUE);
        BOOL sameName = strcmp(op.SourceName + offset, op.TargetName + offset) == 0;
        if (!sameName)
            script->Add(op);
        if (sameName || !script->IsGood())
        {
            free(op.SourceName);
            free(op.TargetName);
            if (!sameName)
            {
                script->ResetState();
                return FALSE;
            }
        }
    }
    // If this directory contains any skipped item, the parent directory cannot be deleted either
    // If it's just a link to a directory, we delete it regardless of whether anything is left inside
    if (!copyMoveDirIsLink && canDelUpperDirAfterMove != NULL && !canDelDirAfterMove)
        *canDelUpperDirAfterMove = FALSE;
    // if nothing was copied or moved inside the directory and we are only transferring files,
    // we will cancel the creation of a directory (unnecessary empty directory) if it was a link to a directory,
    // so this does not concern this thing (it is a link and not a directory)
    if (!copyMoveDirIsLink && (type == atCopy || type == atMove) && filterCriteria != NULL &&
        filterCriteria->SkipEmptyDirs && createDirIndex >= 0 &&
        createDirIndex == script->Count - 1)
    {
        free(script->At(createDirIndex).SourceName);
        free(script->At(createDirIndex).TargetName);
        script->Delete(createDirIndex);
        if (!script->IsGood())
            script->ResetState();
        // If this directory is skipped, the parent directory cannot be deleted either
        if (canDelUpperDirAfterMove != NULL)
            *canDelUpperDirAfterMove = FALSE;
    }
    else
    {
        // If we are to preserve the time & date of the address book, we will also save the operation of setting the date & time.
        // directories (can be done only after completing the writing of subdirectories and files to this directory)
        if ((type == atCopy || type == atMove) &&
            filterCriteria != NULL && filterCriteria->PreserveDirTime &&
            createDirIndex >= 0 && createDirIndex < script->Count)
        {
            op.Opcode = ocCopyDirTime;
            op.OpFlags = 0;
            op.Size = CHATTRS_FILE_SIZE;
            op.SourceName = sourceDirTime != NULL ? (char*)(DWORD_PTR)sourceDirTime->dwLowDateTime : NULL;
            op.TargetName = DupStr(script->At(createDirIndex).TargetName);
            if (op.TargetName == NULL)
                return FALSE;
            op.Attr = sourceDirTime != NULL ? sourceDirTime->dwHighDateTime : 0;

            script->Add(op);
            if (!script->IsGood())
            {
                script->ResetState();
                return FALSE;
            }
        }

        // if it is necessary to delete a directory or a link to the directory sourcePath + dirName (delete and move)
        if (copyMoveDirIsLink && type == atMove ||                                          // u linku neni canDelDirAfterMove podstatne (link jde smaznout vzdy)
            !copyMoveDirIsLink && type == atMove && canDelDirAfterMove || type == atDelete) // deleting the source directory or link to the directory
        {
            if (type == atDelete && !delDirectory)
                return delDirectoryReturn; // CANCEL / NO

            op.Opcode = copyMoveDirIsLink && type == atMove ? ocDeleteDirLink : ocDeleteDir;
            op.OpFlags = 0;
            op.Size = copyMoveDirIsLink && type == atMove ? DELETE_DIRLINK_SIZE : DELETE_DIR_SIZE;
            op.Attr = sourceDirAttr;
            BOOL skip;
            BOOL skipTooLongSrcNameErr = FALSE;
            if ((op.SourceName = BuildName(sourcePath, dirName, NULL, &skip,
                                           &ErrTooLongDirNameSkipAll, sourcePath)) == NULL)
            {
                if (skip)
                    skipTooLongSrcNameErr = TRUE; // we want to add a tag for skipping directory creation
                else
                    return FALSE;
            }
            if (!skipTooLongSrcNameErr)
            {
                op.TargetName = NULL;
                script->Add(op);
                if (!script->IsGood())
                {
                    script->ResetState();
                    free(op.SourceName);
                    return FALSE;
                }
            }
        }

        // if needed, we will also save a flag for skipping directory creation
        if (type == atCopy || type == atMove)
        {
            op.Opcode = ocLabelForSkipOfCreateDir;
            op.OpFlags = 0;
            op.Size.SetUI64(0);
            CQuadWord dirSize = script->TotalFileSize - dirStartTotalFileSize;
            op.SourceName = (char*)(DWORD_PTR)dirSize.LoDWord;
            op.TargetName = (char*)(DWORD_PTR)dirSize.HiDWord;
            op.Attr = createDirIndex;

            script->Add(op);
            if (!script->IsGood())
            {
                script->ResetState();
                return FALSE;
            }
        }
    }
    return TRUE;
}

BOOL GetLinkTgtFileSize(HWND parent, const char* fileName, COperation* op, CQuadWord* size,
                        BOOL* cancel, BOOL* ignoreAll)
{
    *cancel = FALSE;
    if (fileName == NULL)
        fileName = op->SourceName;

READLINKTGTSIZE_AGAIN:

    DWORD err;
    if (SalGetFileSize2(fileName, *size, &err))
        return TRUE;
    else
    {
        int res;
        if (*ignoreAll)
            res = IDB_IGNORE;
        else
        {
            res = (int)CErrorReadingADSDlg(parent, fileName, GetErrorText(err),
                                           LoadStr(IDS_ERRORGETTINGLINKTGTSIZE))
                      .Execute();
        }
        switch (res)
        {
        case IDRETRY:
            goto READLINKTGTSIZE_AGAIN;

        case IDB_IGNOREALL:
            *ignoreAll = TRUE; // here break; is not missing
        case IDB_IGNORE:
            break;

        case IDCANCEL:
        {
            if (op != NULL)
            {
                free(op->SourceName);
                if (op->TargetName != NULL)
                    free(op->TargetName);
            }
            *cancel = TRUE;
            break;
        }
        }
        return FALSE;
    }
}

BOOL CFilesWindow::BuildScriptFile(COperations* script, CActionType type, char* sourcePath,
                                   BOOL sourcePathSupADS, char* targetPath,
                                   CTargetPathState targetPathState, BOOL targetPathSupADS,
                                   BOOL targetPathIsFAT32, char* mask, char* fileName,
                                   char* fileDOSName, const CQuadWord& fileSize,
                                   CAttrsData* attrsData, char* mapName, DWORD sourceFileAttr,
                                   CChangeCaseData* chCaseData, BOOL onlySize,
                                   FILETIME* fileLastWriteTime, DWORD srcAndTgtPathsFlags)
{
    SLOW_CALL_STACK_MESSAGE14("CFilesWindow::BuildScriptFile(, %d, %s, %d, %s, %d, %d, %d, %s, %s, , , , %s, 0x%X, , %d, , 0x%X)",
                              type, sourcePath, sourcePathSupADS, targetPath, targetPathState, targetPathSupADS,
                              targetPathIsFAT32, mask, fileName, mapName, sourceFileAttr, onlySize, srcAndTgtPathsFlags);

    script->FilesCount++;
    CQuadWord fileSizeLoc = fileSize;
    char message[2 * MAX_PATH + 200];
    COperation op;
    switch (type)
    {
    case atCopy:
    case atMove:
    {
        op.Opcode = (type == atCopy) ? ocCopyFile : ocMoveFile;
        op.FileSize = fileSizeLoc;
        op.OpFlags = srcAndTgtPathsFlags;
        if (!script->CopyAttrs && // When copying attributes, the heuristic is about setting the Encrypted attribute aside
            ((sourceFileAttr & FILE_ATTRIBUTE_ENCRYPTED) || targetPathState == tpsEncryptedExisting ||
             targetPathState == tpsEncryptedNotExisting))
        {
            op.OpFlags |= OPFL_AS_ENCRYPTED; // if just renaming (moving within one volume), we will drop the flag again
            if (type == atMove && !script->ShowStatus)
                script->ShowStatus = TRUE; // When moving with the setting of the encrypted attribute, it is done by copying, so we need to display the status.
        }
        op.Attr = sourceFileAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, fileDOSName, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        if (targetPathIsFAT32 && fileSizeLoc > CQuadWord(0xFFFFFFFF /* 4GB minus 1 Byte*/, 0))
        { // Too large file for FAT32 (warning the user that the operation will most likely not complete successfully)

        FAT_TOO_BIG_FILE:

            int msgRes = DIALOG_YES /* Skip*/;
            if (!ErrTooBigFileFAT32SkipAll)
            {
                _snprintf_s(message, _TRUNCATE, LoadStr(IDS_FILEISTOOBIGFORFAT32), op.SourceName);
                MSGBOXEX_PARAMS params;
                memset(&params, 0, sizeof(params));
                params.HParent = MainWindow->HWindow;
                params.Flags = MB_YESNOCANCEL | MB_ICONEXCLAMATION | MSGBOXEX_SILENT;
                params.Caption = LoadStr(IDS_ERRORTITLE);
                params.Text = message;
                char aliasBtnNames[200];
                /* Used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   We will let the collision of hotkeys for the message box buttons be solved by simulating that it is a menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};*/
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                        DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                params.AliasBtnNames = aliasBtnNames;
                msgRes = SalMessageBoxEx(&params);
                if (msgRes == DIALOG_NO /* Skip All*/)
                    ErrTooBigFileFAT32SkipAll = TRUE;
            }
            free(op.SourceName);
            return (msgRes == DIALOG_YES /* Skip*/ || msgRes == DIALOG_NO /* Skip All*/);
        }
        char finalName[2 * MAX_PATH + 200]; // + 200 is a reserve (Windows makes paths longer than MAX_PATH)
        if (mapName == NULL)
        {
            // Petr: a bit of a mess: mask *.* does not produce a copy of the source name, which is a problem when copying
            // souboru s invalidnim jmenem, napr. "c   ..." + "*.*" = "c   ", takze si trochu pomuzeme a nechame
            // set mask to NULL = primitive textual copy of the name
            char* opMask = mask != NULL && strcmp(mask, "*.*") == 0 ? NULL : mask;
            if ((op.TargetName = BuildName(targetPath,
                                           MaskName(finalName, 2 * MAX_PATH + 200, fileName, opMask),
                                           NULL, &skip, &ErrTooLongTgtNameSkipAll, sourcePath)) == NULL)
            {
                free(op.SourceName);
                return skip;
            }
        }
        else
        {
            if ((op.TargetName = BuildName(targetPath, mapName, NULL, &skip,
                                           &ErrTooLongTgtNameSkipAll, sourcePath)) == NULL)
            {
                free(op.SourceName);
                return skip;
            }
        }
        if (type == atMove && strcmp(op.SourceName, op.TargetName) == 0 ||
            type == atCopy && StrICmp(op.SourceName, op.TargetName) == 0)
        {
            free(op.SourceName);
            free(op.TargetName);
            if (type == atMove) // move to where it already is ...
            {
                SalMessageBox(MainWindow->HWindow, LoadStr(IDS_CANNOTMOVEFILETOITSELF),
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
                SalMessageBox(MainWindow->HWindow, LoadStr(IDS_CANNOTCOPYFILETOITSELF),
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            return FALSE;
        }
        // if just renaming (moving within one volume) is enough, we will again unset the OPFL_AS_ENCRYPTED flag
        if (op.Opcode == ocMoveFile && (op.OpFlags & OPFL_AS_ENCRYPTED) &&
            (sourceFileAttr & FILE_ATTRIBUTE_ENCRYPTED) &&
            !script->SameRootButDiffVolume && HasTheSameRootPath(sourcePath, targetPath))
        {
            op.OpFlags &= ~OPFL_AS_ENCRYPTED;
        }
        if (type == atCopy || op.Opcode == ocMoveFile && (op.OpFlags & OPFL_AS_ENCRYPTED) ||
            script->SameRootButDiffVolume || !HasTheSameRootPath(sourcePath, targetPath))
        {
            // if the path ends with a space/dot, it is invalid and we must not make a copy,
            // CreateFile trims spaces/dots and would thus copy another file possibly to a different name
            BOOL invalidSrcName = FileNameIsInvalid(op.SourceName, TRUE);

            // Optimization "overwrite older" for copying from slow network to fast local disk
            // reading file time on a slow network is much faster when reading a continuous listing
            // from the path, rather than querying each file one by one)
            if (!invalidSrcName && (srcAndTgtPathsFlags & OPFL_TGTPATH_IS_NET) == 0 && script->OverwriteOlder && fileLastWriteTime != NULL)
            {
                BOOL invalidTgtName = FileNameIsInvalid(op.TargetName, TRUE);
                if (!invalidTgtName)
                {
                    HANDLE find;
                    WIN32_FIND_DATA dataOut;
                    find = HANDLES_Q(FindFirstFile(op.TargetName, &dataOut));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));

                        const char* tgtName = SalPathFindFileName(op.TargetName);
                        if (StrICmp(tgtName, dataOut.cFileName) == 0 &&                 // if it's not just about matching the DOS name (there will be a change in the DOS name, not just an overwrite)
                            (dataOut.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // if it's not a directory (the overwrite-older won't change anything)
                        {
                            // We will truncate times to seconds (different file systems store times with different precisions, so there were "differences" even between "identical" times)
                            FILETIME roundedInTime;
                            *(unsigned __int64*)&roundedInTime = *(unsigned __int64*)fileLastWriteTime - (*(unsigned __int64*)fileLastWriteTime % 10000000);
                            *(unsigned __int64*)&dataOut.ftLastWriteTime = *(unsigned __int64*)&dataOut.ftLastWriteTime - (*(unsigned __int64*)&dataOut.ftLastWriteTime % 10000000);

                            if (CompareFileTime(&roundedInTime, &dataOut.ftLastWriteTime) <= 0) // source file is not newer than the target file - skipping the copy operation
                            {
                                free(op.SourceName);
                                free(op.TargetName);
                                return TRUE;
                            }
                            op.OpFlags |= OPFL_OVERWROLDERALRTESTED;
                        }
                    }
                }
            }

            // links: fileSizeLoc == 0, file size must be obtained via GetLinkTgtFileSize() additionally
            if ((sourceFileAttr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                BOOL cancel;
                CQuadWord size;
                if (GetLinkTgtFileSize(HWindow, NULL, &op, &size, &cancel, &ErrGetFileSizeOfLnkTgtIgnAll))
                {
                    fileSizeLoc = size;
                    op.FileSize = fileSizeLoc;

                    // We have a new file size, we need to solve this again:
                    // Too large file for FAT32 (warning the user that the operation will most likely not complete successfully)
                    if (targetPathIsFAT32 && fileSizeLoc > CQuadWord(0xFFFFFFFF /* 4GB minus 1 Byte*/, 0))
                    {
                        free(op.TargetName);
                        op.TargetName = NULL;

                        goto FAT_TOO_BIG_FILE;
                    }
                }
                if (cancel)
                    return FALSE;
            }

            if (fileSizeLoc >= COPY_MIN_FILE_SIZE)
                op.Size = fileSizeLoc;
            else
                op.Size = COPY_MIN_FILE_SIZE; // Zero/Small files take at least as long as files with size COPY_MIN_FILE_SIZE

            if (sourcePathSupADS &&                       // if there is a chance we will find ADS and
                (targetPathSupADS || !ConfirmADSLossAll)) // if ADS should be ignored
            {
                CQuadWord adsSize;
                CQuadWord adsOccupiedSpace;
                DWORD adsWinError;
                BOOL onlyDiscardableStreams;

            READFILEADS_AGAIN:

                if (!invalidSrcName &&
                    CheckFileOrDirADS(op.SourceName, FALSE, &adsSize, NULL, NULL, NULL, &adsWinError,
                                      script->BytesPerCluster, &adsOccupiedSpace,
                                      &onlyDiscardableStreams))
                { // source file has ADS, we need to copy them to the target file
                    if (targetPathSupADS)
                    {
                        op.OpFlags |= OPFL_COPY_ADS;
                        op.Size += adsSize;
                        script->OccupiedSpace += adsOccupiedSpace;
                        script->TotalFileSize += adsSize;
                    }
                    else // Copying to a file system other than NTFS (question about trimming ADS)
                    {
                        int res;
                        if (ConfirmADSLossAll || onlyDiscardableStreams)
                            res = IDYES;
                        else
                        {
                            if (ConfirmADSLossSkipAll)
                                res = IDB_SKIP;
                            else
                            {
                                GetADSStreamsNames(ADSStreamsGlobalBuf, 5000, op.SourceName, FALSE);
                                if (ADSStreamsGlobalBuf[0] == 0)
                                    res = IDYES;
                                else
                                {
                                    res = (int)CConfirmADSLossDlg(HWindow, TRUE, op.SourceName, ADSStreamsGlobalBuf, type == atMove).Execute();
                                }
                            }
                        }
                        switch (res)
                        {
                        case IDB_ALL:
                            ConfirmADSLossAll = TRUE; // here break; is not missing
                        case IDYES:
                            break; // we will ignore ADS, which will not be copied/moved (resulting in complete loss)

                        case IDB_SKIPALL:
                            ConfirmADSLossSkipAll = TRUE; // here break; is not missing
                        case IDB_SKIP:
                        {
                            free(op.SourceName);
                            free(op.TargetName);
                            return TRUE;
                        }

                        case IDCANCEL:
                        {
                            free(op.SourceName);
                            free(op.TargetName);
                            return FALSE;
                        }
                        }
                    }
                }
                else // an error occurred or no ADS
                {
                    if (invalidSrcName ||
                        adsWinError != NO_ERROR &&                                                                           // an error occurred
                            (adsWinError != ERROR_INVALID_FUNCTION || StrNICmp(op.SourceName, "\\\\tsclient\\", 11) != 0) && // Local disk paths on Terminal Server do not support listing ADS (otherwise ADS are supported, what a comedy)
                            (adsWinError != ERROR_INVALID_PARAMETER && adsWinError != ERROR_NO_MORE_ITEMS ||
                             (srcAndTgtPathsFlags & OPFL_SRCPATH_IS_NET) == 0)) // A mounted FAT/FAT32 disk cannot be recognized on a network disk (e.g. \\petr\f\drive_c) + NOVELL-NETWARE volume accessed through NDS - we think it is NTFS and therefore we try to read ADS, which reports this error
                    {
                        // First, we will try if an error occurs even when opening the file - such an error
                        // user effort to understand, so we will display it preferentially (before the error of reading ADS)
                        HANDLE in;
                        if (!invalidSrcName)
                        {
                            in = HANDLES_Q(CreateFile(op.SourceName, GENERIC_READ,
                                                      FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                      OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL));
                        }
                        else
                        {
                            in = INVALID_HANDLE_VALUE;
                        }
                        if (!invalidSrcName && in != INVALID_HANDLE_VALUE) // File opening is without any issues, we will report an ADS error
                        {
                            HANDLES(CloseHandle(in));

                            int res;
                            if (ErrReadingADSIgnoreAll)
                                res = IDB_IGNORE;
                            else
                            {
                                res = (int)CErrorReadingADSDlg(HWindow, op.SourceName, GetErrorText(adsWinError)).Execute();
                            }
                            switch (res)
                            {
                            case IDRETRY:
                                goto READFILEADS_AGAIN;

                            case IDB_IGNOREALL:
                                ErrReadingADSIgnoreAll = TRUE; // here break; is not missing
                            case IDB_IGNORE:
                                break;

                            case IDCANCEL:
                            {
                                free(op.SourceName);
                                free(op.TargetName);
                                return FALSE;
                            }
                            }
                        }
                        else // reporting an error opening a file
                        {
                            DWORD err = GetLastError();
                            if (invalidSrcName)
                                err = ERROR_INVALID_NAME;
                            int res;
                            if (ErrFileSkipAll)
                                res = IDB_SKIP;
                            else
                            {
                                res = (int)CFileErrorDlg(HWindow, LoadStr(IDS_ERROROPENINGFILE), op.SourceName,
                                                         GetErrorText(err))
                                          .Execute();
                            }
                            switch (res)
                            {
                            case IDRETRY:
                                goto READFILEADS_AGAIN;

                            case IDB_SKIPALL:
                                ErrFileSkipAll = TRUE; // here break; is not missing
                            case IDB_SKIP:
                            {
                                free(op.SourceName);
                                free(op.TargetName);
                                return TRUE;
                            }

                            case IDCANCEL:
                            {
                                free(op.SourceName);
                                free(op.TargetName);
                                return FALSE;
                            }
                            }
                        }
                    }
                }
            }

            if (script->BytesPerCluster == 0)
                TRACE_E("How is it possible that script->BytesPerCluster is not yet set???");
            else
            {
                script->OccupiedSpace += fileSizeLoc - ((fileSizeLoc - CQuadWord(1, 0)) % CQuadWord(script->BytesPerCluster, 0)) +
                                         CQuadWord(script->BytesPerCluster - 1, 0);
            }
            script->TotalFileSize += fileSizeLoc;
        }
        else
        {
            op.Size = MOVE_FILE_SIZE;
            if (!script->FastMoveUsed)
                script->FastMoveUsed = TRUE;
        }

        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            free(op.TargetName);
            return FALSE;
        }
        else
            return TRUE;
    }

    case atDelete:
    {
        op.Opcode = ocDeleteFile;
        op.OpFlags = 0;
        op.Size = DELETE_FILE_SIZE;
        op.Attr = sourceFileAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, fileDOSName, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        op.TargetName = NULL;
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            return FALSE;
        }
        else
            return TRUE;
    }

    case atCountSize:
    {
        if (script->BytesPerCluster == 0) // no risk of space estimate
        {
            DWORD d1, d2, d3, d4;
            if (MyGetDiskFreeSpace(sourcePath, &d1, &d2, &d3, &d4))
                script->BytesPerCluster = d1 * d2;
        }

        char name[2 * MAX_PATH]; // + MAX_PATH is a reserve (Windows makes paths longer than MAX_PATH)
        int l = (int)strlen(sourcePath);
        memmove(name, sourcePath, l);
        if (name[l - 1] != '\\')
            name[l++] = '\\';
        memmove(name + l, fileName, 1 + strlen(fileName)); // name is always less than MAX_PATH
        CQuadWord s;
        DWORD err = NO_ERROR;
        if (FileBasedCompression && !onlySize &&                                         // if compression is possible at all
            (sourceFileAttr & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE))) // if the file is compressed or sparse
        {
            s.LoDWord = GetCompressedFileSize(name, &s.HiDWord);
            err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND && fileDOSName != NULL && strcmp(fileName, fileDOSName) != 0)
            {                                                            // patch for calculating the size of a file that needs to be accessed via a DOS name when we cannot handle it via a UNICODE name (the multibyte version of the name after converting back to UNICODE does not match the original UNICODE file name)
                memmove(name + l, fileDOSName, 1 + strlen(fileDOSName)); // name is always less than MAX_PATH
                s.LoDWord = GetCompressedFileSize(name, &s.HiDWord);
                err = GetLastError();
                if (s.LoDWord == 0xFFFFFFFF && err != NO_ERROR)
                    memmove(name + l, fileName, 1 + strlen(fileName)); // (name is always < MAX_PATH) - in case of an error, the message will be about the full name and not the DOS name
            }
        }
        else
        {
            s = fileSizeLoc;
        }
        if (s.LoDWord == 0xFFFFFFFF && err != NO_ERROR)
        {
            if (!script->SkipAllCountSizeErrors)
            {
                sprintf(message, LoadStr(IDS_GETCOMPRFILESIZEERROR), name, GetErrorText(err));
                script->SkipAllCountSizeErrors = SalMessageBox(HWindow, message, LoadStr(IDS_ERRORTITLE),
                                                               MB_YESNO | MB_ICONEXCLAMATION) == IDYES;
                UpdateWindow(MainWindow->HWindow);
            }
            s = fileSizeLoc; // unable to determine compressed size, we will settle for normal size
        }

        script->Sizes.Add(fileSizeLoc); // output dialog is prepared for the case when this field is in an error state
        script->TotalSize += fileSizeLoc;
        if (script->BytesPerCluster != 0)
        {
            script->OccupiedSpace += s - ((s - CQuadWord(1, 0)) % CQuadWord(script->BytesPerCluster, 0)) +
                                     CQuadWord(script->BytesPerCluster - 1, 0);
        }
        else
        {
            script->OccupiedSpace += s;
        }
        script->TotalFileSize += s;
        script->CompressedSize += s;

        return TRUE;
    }

    case atRecursiveConvert:
    case atConvert:
    {
        op.Opcode = ocConvert;
        op.OpFlags = 0;
        op.Attr = sourceFileAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, NULL, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        op.TargetName = NULL;

        // links: fileSizeLoc == 0, file size must be obtained via GetLinkTgtFileSize() additionally
        if ((sourceFileAttr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
        {
            BOOL cancel;
            CQuadWord size;
            if (GetLinkTgtFileSize(HWindow, NULL, &op, &size, &cancel, &ErrGetFileSizeOfLnkTgtIgnAll))
                fileSizeLoc = size;
            if (cancel)
                return FALSE;
        }

        if (fileSizeLoc >= CONVERT_MIN_FILE_SIZE)
            op.Size = fileSizeLoc;
        else
            op.Size = CONVERT_MIN_FILE_SIZE; // Empty/small files take at least as long as files with size CONVERT_MIN_FILE_SIZE
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            return FALSE;
        }
        else
            return TRUE;
    }

    case atChangeAttrs:
    {
        op.Opcode = ocChangeAttrs;
        op.OpFlags = 0;
        op.Attr = sourceFileAttr;
        // Compression: zero/small files take at least as long as files with size COMPRESS_ENCRYPT_MIN_FILE_SIZE
        op.Size = (attrsData->ChangeCompression || attrsData->ChangeEncryption) ? max(fileSizeLoc, COMPRESS_ENCRYPT_MIN_FILE_SIZE) : CHATTRS_FILE_SIZE;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, NULL, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        op.TargetName = (char*)(DWORD_PTR)((SalGetFileAttributes(op.SourceName) & attrsData->AttrAnd) | attrsData->AttrOr);
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            return FALSE;
        }
        else
            return TRUE;
    }

    case atChangeCase:
    {
        op.Opcode = ocMoveFile;
        op.FileSize = fileSizeLoc;
        op.OpFlags = 0;
        op.Size = MOVE_FILE_SIZE;
        op.Attr = sourceFileAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, NULL, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        if ((op.TargetName = BuildName(sourcePath, fileName)) == NULL) // if the name is too long, it will show in the condition earlier
        {
            free(op.SourceName);
            return FALSE;
        }
        int offset = (int)strlen(op.SourceName) - (int)strlen(fileName);
        AlterFileName(op.TargetName + offset, op.SourceName + offset, -1,
                      chCaseData->FileNameFormat, chCaseData->Change, FALSE);
        BOOL sameName = strcmp(op.SourceName + offset, op.TargetName + offset) == 0;
        if (!sameName)
            script->Add(op);
        if (sameName || !script->IsGood())
        {
            free(op.SourceName);
            free(op.TargetName);
            if (!script->IsGood())
                script->ResetState();
            return sameName;
        }
        else
            return TRUE;
    }
    }
    return FALSE; // does not know anything else
}

void CFilesWindow::CalculateDirSizes()
{
    CALL_STACK_MESSAGE1("CFilesWindow::CalculateDirSizes()");
    if (Is(ptDisk))
    {
        FilesAction(atCountSize, MainWindow->GetNonActivePanel(), 2);
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            CalculateOccupiedZIPSpace(2);
        }
    }
}

void CFilesWindow::ExecuteFromArchive(int index, BOOL edit, HWND editWithMenuParent,
                                      const POINT* editWithMenuPoint)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ExecuteFromArchive(%d, %d)", index, edit);
    if (CheckPath(TRUE) != ERROR_SUCCESS)
        return;

    // Let's find out if we can pack into the archive (file editing from the archive is possible, otherwise we warn)
    if (edit)
    {
        int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
        if (format != 0) // "always-true" - we found a supported archive
        {
            if (!PackerFormatConfig.GetUsePacker(format - 1)) // Doesn't have edit?
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_EDITPACKNOTSUPPORTED),
                                  LoadStr(IDS_QUESTION), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
                {
                    return; // Action is canceled (user does not want to edit when the archive cannot be updated)
                }
            }
        }
    }

    //--- we will obtain the full long name
    char dcFileName[2 * MAX_PATH]; // ZIP: name for disk cache
    CFileData* f = &Files->At(index - Dirs->Count);

    if (!SalIsValidFileNameComponent(f->Name))
    {
        SalMessageBox(HWindow, LoadStr(IDS_UNABLETOEDITINVFILES),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    int count = Dirs->Count + Files->Count;
    int j;
    for (j = 0; j < count; j++)
    {
        if (index != j) // Do not compare two identical ones
        {
            CFileData* f2 = j < Dirs->Count ? &Dirs->At(j) : &Files->At(j - Dirs->Count);
            if (f2->NameLen == f->NameLen &&
                StrNICmp(f->Name, f2->Name, f2->NameLen) == 0)
            {
                SalMessageBox(HWindow, LoadStr(IDS_UNABLETOEDITDUPFILES),
                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                return;
            }
        }
    }

    StrICpy(dcFileName, GetZIPArchive()); // The name of the archive file should be compared "case-insensitive" (Windows file system), so we will always convert it to lowercase
    SalPathAppend(dcFileName, GetZIPPath(), 2 * MAX_PATH);
    SalPathAppend(dcFileName, f->Name, 2 * MAX_PATH);

    // setting disk cache for plugin (default values are changed only for the plugin)
    char arcCacheTmpPath[MAX_PATH];
    arcCacheTmpPath[0] = 0;
    BOOL arcCacheOwnDelete = FALSE;
    BOOL arcCacheCacheCopies = TRUE;
    CPluginInterfaceAbstract* plugin = NULL; // != NULL if the plugin has its own deletion
    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
    if (format != 0) // We found a supported archive
    {
        format--;
        int index2 = PackerFormatConfig.GetUnpackerIndex(format);
        if (index2 < 0) // view: is it internal processing (plug-in)?
        {
            CPluginData* data = Plugins.Get(-index2 - 1);
            if (data != NULL)
            {
                data->GetCacheInfo(arcCacheTmpPath, &arcCacheOwnDelete, &arcCacheCacheCopies);
                if (arcCacheOwnDelete)
                    plugin = data->GetPluginInterface()->GetInterface();
            }
        }
    }

    BOOL exists;
    CQuadWord fileSize = CQuadWord(-1, -1);
    FILETIME lastWrite;
    DWORD attr = -1;
    memset(&lastWrite, 0, sizeof(lastWrite));
    int errorCode;
    char* name = (char*)DiskCache.GetName(dcFileName, f->Name, &exists, FALSE,
                                          arcCacheTmpPath[0] != 0 ? arcCacheTmpPath : NULL,
                                          plugin != NULL, plugin, &errorCode);
    if (name == NULL)
    {
        if (errorCode == DCGNE_TOOLONGNAME)
        {
            SalMessageBox(HWindow, LoadStr(IDS_UNPACKTOOLONGNAME),
                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        }
        return;
    }
    char dosName[14];
    dosName[0] = 0;
    WIN32_FIND_DATA data;
    if (!exists) // we need to unpack it
    {
        char* backSlash = strrchr(name, '\\');
        char tmpPath[MAX_PATH];
        memcpy(tmpPath, name, backSlash - name);
        tmpPath[backSlash - name] = 0;
        BeginStopRefresh(); // He was snoring in his sleep
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
        if (PackUnpackOneFile(this, GetZIPArchive(), PluginData.GetInterface(),
                              dcFileName + strlen(GetZIPArchive()) + 1, f, tmpPath,
                              NULL, NULL))
        {
            SetCursor(oldCur);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            HANDLE find = HANDLES_Q(FindFirstFile(name, &data));
            if (find != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(find));
                fileSize = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
                lastWrite = data.ftLastWriteTime;
                attr = data.dwFileAttributes;
                if (data.cAlternateFileName[0] != 0)
                    strcpy(dosName, data.cAlternateFileName);
            }

            DiskCache.NamePrepared(dcFileName, fileSize);
            EndStopRefresh(); // now he's sniffling again, he'll start up
        }
        else
        {
            SetCursor(oldCur);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
            DiskCache.ReleaseName(dcFileName, FALSE); // not unpacked, nothing to cache
            EndStopRefresh();                         // now he's sniffling again, he'll start up
            return;
        }
    }

    // Splitting the full file name into path (buf) and name (s)
    char buf[MAX_PATH];
    char* s = strrchr(name, '\\');
    if (s != NULL)
    {
        memcpy(buf, name, s - name);
        buf[s - name] = 0;
        s++;
    }

    // launching the default item from the context menu (association)
    if (edit)
    {
        if (editWithMenuParent != NULL && editWithMenuPoint != NULL)
        {
            EditFileWith(name, editWithMenuParent, editWithMenuPoint);
        }
        else
            EditFile(name);
    }
    else
    {
        if (s != NULL)
        {
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            MainWindow->SetDefaultDirectories();
            ExecuteAssociation(GetListBoxHWND(), buf, s);
            SetCursor(oldCur);
        }
    }

    if (fileSize == CQuadWord(-1, -1))
    {
        HANDLE find = HANDLES_Q(FindFirstFile(name, &data));
        if (find != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(find));
            fileSize = CQuadWord(data.nFileSizeLow, data.nFileSizeHigh);
            lastWrite = data.ftLastWriteTime;
            attr = data.dwFileAttributes;
            if (data.cAlternateFileName[0] != 0)
                strcpy(dosName, data.cAlternateFileName);
        }
    }

    if (UnpackedAssocFiles.AddFile(GetZIPArchive(), GetZIPPath(), buf, s, dosName, lastWrite, fileSize, attr))
    {                                                                         // this file does not yet have the 'lock' object ExecuteAssocEvent in the disk cache
        DiskCache.AssignName(dcFileName, ExecuteAssocEvent, FALSE, crtCache); // arcCacheCacheCopies has no effect - it will be cached until the archive is closed, we will not pack it earlier
    }
    else
    { // It is unnecessary to add the same 'lock' object to the temporary file
        DiskCache.ReleaseName(dcFileName, FALSE);
    }
    AssocUsed = TRUE;
}
