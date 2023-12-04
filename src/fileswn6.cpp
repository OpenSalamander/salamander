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

// pomocne promenne pro dialogy v BuildScriptXXX()
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

// pomocne promenne pro testy pokusu o preruseni stavby scriptu
DWORD LastTickCount;

void CFilesWindow::Activate(BOOL shares)
{
    CALL_STACK_MESSAGE_NONE
    //  TRACE_I("CFilesWindow::Activate");
    LastInactiveRefreshStart = LastInactiveRefreshEnd; // aktivaci se rusi udaje o poslednim refreshi v neaktivnim okne
    BOOL needToRefreshIcons = InactWinOptimizedReading;
    if (Is(ptDisk) || Is(ptZIPArchive)) // disky a archivy
    {
        if (!SkipOneActivateRefresh && (!GetNetworkDrive() || !Configuration.DrvSpecRemoteDoNotRefreshOnAct) ||
            InactiveRefreshTimerSet) // odlozeny refresh pri neaktivnim okne se musi pri aktivaci okna provest ihned
        {
            DWORD checkPathRet;
            if ((checkPathRet = CheckPath(FALSE)) != ERROR_SUCCESS)
            {
                if (checkPathRet == ERROR_USER_TERMINATED) // user dal ESC -> zmena na fixed drive
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
                else // jina chyba na ceste, dame si refresh
                {
                    HANDLES(EnterCriticalSection(&TimeCounterSection));
                    int t1 = MyTimeCounter++;
                    HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
                }
                needToRefreshIcons = FALSE;
            }
            else // cesta vypada o.k.
            {
                if (!AutomaticRefresh && !GetNetworkDrive() ||           // manualni refresh disku (krom sitoveho disku)
                    GetNetworkDrive() &&                                 // u sit. disku provadime refresh pri kazde
                        !Configuration.DrvSpecRemoteDoNotRefreshOnAct || // aktivaci, neni-li to zakazano (resi Sambu)
                    shares && !GetNetworkDrive() ||                      // obnova sharu (na sit. discich nema vyznam)
                    InactiveRefreshTimerSet)                             // odlozeny refresh pri neaktivnim okne se musi pri aktivaci okna provest ihned
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
                    PostMessage(HWindow, WM_USER_REFRESH_DIR_EX, FALSE, t1); // vime, ze jde pravdepodobne o zbytecny refresh
                    needToRefreshIcons = FALSE;
                }
                else // na automaticky obnovovanych discich obnovime alespon disk-free-space
                {
                    RefreshDiskFreeSpace(FALSE, TRUE);
                }
            }
        }
    }
    else
    {
        if (Is(ptPluginFS)) // plug-in FS: zasleme FSE_ACTIVATEREFRESH, aby si plug-in mohl refreshnout
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

        // v prvni fazi napocitame maximalni sirky promennych (pokud nejaka pouziva $(name:max))
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
        // v druhe fazi tyto sirky aplikujeme
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
                 GetDriveFormFactor(UpperCase[path[0]] - 'A' + 1) == 0 /* neni to floppy */)
        {
            return fixedFlag; // removable ale neni to floppy, napr. USB stick, fotak pres USB (napr. FZ45) - ty bereme jako fixed, jsou dost rychle
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

        SetCurrentDirectory(source); // pro rychlejsi move (system to ma rad)

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

        //---  vytvoreni objektu scriptu
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
        // script->IsCopyOrMoveOperation = TRUE;   // zakomentovano, protoze tenhle Move nechceme pridavat do fronty Copy/Move operaci

        BOOL fastDirectoryMove = TRUE;          // Configuration.FastDirectoryMove;
        if (fastDirectoryMove &&                // fast-dir-move neni globalne vypnute
            HasTheSameRootPath(source, target)) // + v ramci jednoho drivu
        {
            UINT sourceType = DRIVE_REMOTE;
            if (source[0] != '\\') // neni to UNC cesta (ta je vzdy "remote")
            {
                char root[4] = " :\\";
                root[0] = source[0];
                sourceType = GetDriveType(root);
            }

            if (sourceType == DRIVE_REMOTE) // sitovy disk
            {                               // provedeme detekci NOVELLskych disku - nefunguje na nich fast-directory-move
                if (IsNOVELLDrive(source))
                    fastDirectoryMove = Configuration.NetwareFastDirMove;
            }
        }

        //---  inicializace testu na preruseni buildu
        LastTickCount = GetTickCount();

        //---  enumerace souboru/adresaru source adresare
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

            GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

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
                // W2K a novejsi: nasobek d1 * d2 * d3 nefungoval na DFS stromech, reportil Ludek.Vydra@k2atmitec.cz
                script->FreeSpace = MyGetDiskFreeSpace(targetDir);
            }

            BOOL scriptOK = TRUE; // vysledek stavby scriptu, uspech?
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
            // script je sestaven, nechame ho provest
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
            if (res == 0) // nalezeno
            {
                if (index != NULL)
                    *index = m;
                return TRUE;
            }
            else
            {
                if (res > 0)
                {
                    if (l == r || l > m - 1) // nenalezeno
                    {
                        if (index != NULL)
                            *index = m; // mel by byt na teto pozici
                        return FALSE;
                    }
                    r = m - 1;
                }
                else
                {
                    if (l == r) // nenalezeno
                    {
                        if (index != NULL)
                            *index = m + 1; // mel by byt az za touto pozici
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

    //---  inicializace testu na preruseni buildu
    LastTickCount = GetTickCount();

    BOOL fastDirectoryMove = TRUE; // Configuration.FastDirectoryMove;
    if (data->Count > 0)
    {
        char* name = data->At(0)->FileName;
        UINT sourceType = DRIVE_REMOTE;
        if (name != NULL && LowerCase[*name] >= 'a' && LowerCase[*name] <= 'z' &&
            *(name + 1) == ':') // neni UNC cesta (ta je vzdy "remote")
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

            if (fastDirectoryMove &&                   // fast-dir-move neni globalne vypnute
                !copy && sourceType == DRIVE_REMOTE && // + move operace + sitovy disk
                HasTheSameRootPath(name, targetDir))   // + v ramci jednoho drivu
            {                                          // provedeme detekci NOVELLskych disku - nefunguje na nich fast-directory-move
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

    // zjistime jestli je cilem vymenne medium (floppy, ZIP) -> pro urychleni se pouziva vetsi buffer
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
    char sourcePath[2 * MAX_PATH];     // + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    char lastSourcePath[2 * MAX_PATH]; // + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    lastSourcePath[0] = 0;
    BOOL sourceSupADS = FALSE;
    char targetPath[2 * MAX_PATH + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    char mapNameBuf[2 * MAX_PATH];       // + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
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
    TIndirectArray<char>* usedNames = NULL; // seznam vsech nove vytvorenych jmen (pouziva makeCopyOfName==TRUE)
    if (makeCopyOfName)
        usedNames = new TIndirectArray<char>(100, 50);

    DWORD d1, d2, d3, d4;
    if (MyGetDiskFreeSpace(targetPath, &d1, &d2, &d3, &d4))
    {
        script->BytesPerCluster = d1 * d2;
        // W2K a novejsi: nasobek d1 * d2 * d3 nefungoval na DFS stromech, reportil Ludek.Vydra@k2atmitec.cz
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
                if (IsTheSamePath(sourcePath, targetPath) && // "Copy of..." se dela jen pri shode cest
                    makeCopyOfName)                          // testneme, jestli nebude treba udelat "Copy of..." jmeno
                {
                    strcpy(targetName, s + 1); // do targetPath dame navrhovane cilove plne jmeno
                    BOOL isKnown;
                    // mapName tu musi byt NULL, jinak by data->MakeCopyOfName nemohlo byt TRUE
                    if ((isKnown = ContainsString(usedNames, targetName)) != 0 ||
                        SalGetFileAttributes(targetPath) != 0xFFFFFFFF)
                    { // jmeno jiz existuje, musime vygenerovat nove
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
                            char* ext = (attrs & FILE_ATTRIBUTE_DIRECTORY) ? NULL : strrchr(s + 1, '.'); // adresare nemaji pripony (okopirovane chovani Visty)
                            if (ext != NULL && strchr(ext, ' ') != NULL)
                                ext = NULL; // pripona s mezerou neni pripona (okopirovane chovani Visty)
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
                                        break; // "(cislo)" nalezeno
                                    }
                                }
                                else
                                {
                                    numBeg = NULL;
                                    break; // "(cislo)" tam neni
                                }
                            }

                        _VISTA_NEXT_1: // "name - Copy.ext" a "name - Copy (++val).ext"

                            if (++val > 1 && numBeg != NULL)
                            {
                                sprintf(number, "(%d)", val);
                                if (ext != NULL)
                                {
                                    if (numBeg < ext)
                                    {
                                        lstrcpyn(targetName + (numBeg - (s + 1)), number, (int)(1 + MAX_PATH - (numBeg - (s + 1))));                                 // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), numEnd, (int)min((DWORD)((ext - numEnd) + 1), 1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), hyphenCopy, (int)(1 + MAX_PATH - strlen(targetName)));                             // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), ext, (int)(1 + MAX_PATH - strlen(targetName)));                                    // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    }
                                    else
                                    {
                                        lstrcpyn(targetName + (ext - (s + 1)), hyphenCopy, (int)(1 + MAX_PATH - (ext - (s + 1))));                                // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), ext, (int)min((DWORD)((numBeg - ext) + 1), 1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), number, (int)(1 + MAX_PATH - strlen(targetName)));                              // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), numEnd, (int)(1 + MAX_PATH - strlen(targetName)));                              // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    }
                                }
                                else
                                {
                                    lstrcpyn(targetName + (numBeg - (s + 1)), number, (int)(1 + MAX_PATH - (numBeg - (s + 1))));     // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    lstrcpyn(targetName + strlen(targetName), numEnd, (int)(1 + MAX_PATH - strlen(targetName)));     // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    lstrcpyn(targetName + strlen(targetName), hyphenCopy, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                }
                            }
                            else
                            {
                                if (ext == NULL)
                                    lstrcpyn(targetName + len, hyphenCopy, 1 + MAX_PATH - len); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                else
                                    lstrcpyn(targetName + (ext - (s + 1)), hyphenCopy, (int)(1 + MAX_PATH - (ext - (s + 1)))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                if (val > 1)
                                {
                                    sprintf(number, " (%d)", val);
                                    lstrcpyn(targetName + strlen(targetName), number, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                }
                                if (ext != NULL)
                                    lstrcpyn(targetName + strlen(targetName), ext, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                            }

                            if (strlen(targetName) < MAX_PATH) // slozeni jmena probehlo o.k., jinak na to kasleme
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

                                _NEXT_1: // typ "copy (++val)*"

                                    lstrcpyn(targetName, copyOpenPar, MAX_PATH);
                                    sprintf(number, "%d)", ++val);
                                    lstrcpyn(targetName + strlen(targetName), number, (int)(MAX_PATH - strlen(targetName)));
                                    lstrcpyn(targetName + strlen(targetName), num + 1, (int)(1 + MAX_PATH - strlen(targetName))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    if (strlen(targetName) < MAX_PATH)                                                            // slozeni jmena probehlo o.k., jinak na to kasleme
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

                                _NEXT_2: // typ "* (++val)"

                                    sprintf(number, " (%d)", ++val);
                                    if (ext == NULL)
                                    {
                                        if (num2 == NULL)
                                            lstrcpyn(targetName + len, number, 1 + MAX_PATH - len); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        else
                                            lstrcpyn(targetName + (num2 - (s + 1)), number, (int)(1 + MAX_PATH - (num2 - (s + 1)))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    }
                                    else
                                    {
                                        if (num2 == NULL)
                                            lstrcpyn(targetName + (ext - (s + 1)), number, (int)(1 + MAX_PATH - (ext - (s + 1)))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        else
                                            lstrcpyn(targetName + (num2 - (s + 1)), number, (int)(1 + MAX_PATH - (num2 - (s + 1)))); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                        lstrcpyn(targetName + strlen(targetName), ext, 1 + MAX_PATH - (int)strlen(targetName));      // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                    }
                                    if (strlen(targetName) < MAX_PATH) // slozeni jmena probehlo o.k., jinak na to kasleme
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
                            _NEXT_3: // typ "copy of *", pak "copy (++val) of *"

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
                                lstrcpyn(targetName + strlen(targetName), s + 1, 1 + MAX_PATH - (int)strlen(targetName)); // "1 +" aby pri prilis dlouhem jmene byl vysledek == MAX_PATH
                                if (strlen(targetName) < MAX_PATH)                                                        // slozeni jmena probehlo o.k., jinak na to kasleme
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
                    *targetName = 0; // rekonstrukce targetPath

                    // ulozime si vsechna jmena nove vytvorenych souboru
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
                                err = ERROR_ACCESS_DENIED; // nejakou chybu ohlasit musime
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
        SetForegroundWindow(MainWindow->HWindow); // musi se aktivovat ihned po dropu
        BeginStopRefresh();                       // jinak prijde WM_ACTIVATEAPP, ktery ale neaktivuje...
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

            char caption[50]; // jinak dochazi k prepisu LoadStr bufferu jeste pred nakopirovanim do lokalniho bufferu dialogu
            if (copy)
                lstrcpyn(caption, LoadStr(IDS_COPY), 50);
            else
                lstrcpyn(caption, LoadStr(IDS_MOVE), 50);

            HWND hFocusedWnd = GetFocus();
            CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
            EnableWindow(MainWindow->HWindow, FALSE);

            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

            BOOL res = BuildScriptMain2(script, copy, targetPath, data);

            // prohozeno kvuli umozneni aktivace hlavniho okna (nesmi byt disable), jinak prepina do jine app
            EnableWindow(MainWindow->HWindow, TRUE);
            DestroySafeWaitWindow();

            // pokud je aktivni Salamander, zavolame SetFocus na zapamatovane okno (SetFocus nefunguje
            // pokud je hl. okno disablovane - po deaktivaci/aktivaci disablovaneho hl. okna aktivni panel
            // nema fokus)
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
                                        script->BytesPerCluster != 0 && // mame informace o disku
                                        script->OccupiedSpace > script->FreeSpace &&
                                        !IsSambaDrivePath(targetPath); // Samba vraci nesmyslny cluster-size, takze muzeme pocitat jedine s TotalFileSize
                if (occupiedSpTooBig ||
                    script->BytesPerCluster != 0 && // mame informace o disku
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

            // pripravime refresh neautomaticky refreshovanych adresaru
            // zmena v cilovem adresari a v jeho podadresarich
            script->SetWorkPath1(targetPath, TRUE);
            if (!copy) // move operace meni i zdroj operace
            {
                if (data->Count > 0)
                {
                    char* name = data->At(0)->FileName;
                    if (name != NULL)
                    {
                        char path[MAX_PATH];
                        lstrcpyn(path, name, MAX_PATH);
                        if (CutDirectory(path)) // predpokladame jeden zdrojovy adresar (jen operace z panelu, ne Findu)
                        {
                            // zmena ve zdrojovem adresari a v podadresarich
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
        //---  pokud se aktivovalo nejaky okno salamandra, konci suspend mode
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
    // count == 0, selection == NULL => oneFile ukazuje na aktualni soubor
    // jinak selection obsahuje indexy oznacenych count polozek ve fileboxu
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

    //---  inicializace testu na preruseni buildu
    LastTickCount = GetTickCount();

    //---  pokud kopirujem/presouvame z CD, budem cistit read-only attribut
    //     zaroven nastavime CurrentDirectory na pomalejsi medium
    BOOL fastDirectoryMove = TRUE; // Configuration.FastDirectoryMove;
    if (type == atCopy || type == atMove)
    {
        UINT sourceType = DRIVE_REMOTE;
        if (GetPath()[0] != '\\') // neni UNC cesta (ta je vzdy "remote")
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

        if (fastDirectoryMove &&                            // fast-dir-move neni globalne vypnute
            sourceType == DRIVE_REMOTE && type == atMove && // sitovy disk + move operace
            HasTheSameRootPath(GetPath(), targetPath))      // + v ramci jednoho drivu
        {                                                   // provedeme detekci NOVELLskych disku - nefunguje na nich fast-directory-move
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

        // zjistime jestli je cilem vymenne medium (floppy, ZIP) -> pro urychleni se pouziva vetsi buffer
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

    // maska se nesmi upravovat pres PrepareMask !!! viz MaskName()
    char nameMask[2 * MAX_PATH]; // + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    if (mask != NULL)
    {
        lstrcpyn(nameMask, mask, 2 * MAX_PATH);
        mask = nameMask;
    }

    // pristup k souborum je mnohem rychlejsi na aktualnim adresari/disku
    if (type != atMove && type != atCopy)
        SetCurrentDirectory(GetPath());

    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help

    char sourcePath[2 * MAX_PATH + 10]; // +rezerva pro masku ("\\*"), + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    strcpy(sourcePath, GetPath());

    BOOL sourceSupADS = FALSE;
    BOOL targetSupADS = FALSE;
    BOOL targetIsFAT32 = FALSE;
    CTargetPathState targetPathState = tpsUnknown;
    DWORD srcAndTgtPathsFlags = 0;        // flagy jen pro Copy a Move
    if (type == atMove || type == atCopy) // mimo Copy a Move to nema smysl zjistovat
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
                { // prava by kopirovat chtel, ale na cilove ceste nejsou podporovana, takze dame vedet, ze ma smulu (API funkce pro set-security zadne chyby nehlasi, proste idioti)
                    int res = SalMessageBox(HWindow, LoadStr(IDS_ACLNOTSUPPORTEDONTGTPATH), LoadStr(IDS_QUESTION),
                                            MB_YESNO | MB_ICONQUESTION | MSGBOXEX_ESCAPEENABLED);
                    UpdateWindow(MainWindow->HWindow);
                    if (res == IDNO || res == IDCANCEL) // pokud dal CANCEL nebo NO, koncime
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
        // zkusime jestli je jmeno souboru platne, pripadne zkusime jeste jeho DOS-jmeno
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
        *end = 0; // rekonstrukce sourcePath
    }

    if (selCount > 0 || oneFile != NULL)
    {
        if (type == atMove || type == atCopy) // mimo Copy a Move to nema smysl zjistovat
        {
            DWORD d1, d2, d3, d4;
            if (MyGetDiskFreeSpace(targetPath, &d1, &d2, &d3, &d4))
            {
                script->BytesPerCluster = d1 * d2;
                // W2K a novejsi: nasobek d1 * d2 * d3 nefungoval na DFS stromech, reportil Ludek.Vydra@k2atmitec.cz
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
            // oneFile ukazuje na oznacenou nebo caret polozku ve fileboxu
            if (oneFile->Attr & FILE_ATTRIBUTE_DIRECTORY) // jde o ptDisk
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
                else // change-case: oznacene adresare bez recurse-sub-dirs
                {    // convert: nerekurzivni + tyka se jen souboru -> s adresari neni co delat
                    if (type == atChangeCase)
                    {
                        COperation op;
                        op.OpFlags = 0; // zmena case = prejmenovani = invalidni jmena budeme hlasit (nejde jen o toleranci existujiciho)
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
                            if ((op.TargetName = BuildName(sourcePath, oneFile->Name)) == NULL) // problem prilis dlouheho jmena resi uz predchozi podminka
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

char ADSStreamsGlobalBuf[5000]; // do tohoto bufferu se vlozi jmena ADS (oddelene carkou), globalni je aby pri rekurzi nedosel stack

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

    if (bufSize > 0 && (StrICmp(listBuf, "Zone.Identifier") == 0 || // tento stream vytvari XP od s.p. 2 automaticky a je predurcen k ignorovani, tak jim nebudeme zatezovat usera
                        StrICmp(listBuf, "encryptable") == 0))      // tento stream se vyskytuje asi jen na thumbs.db, nikdo nevi co je zac, ale vytvareji ho okna, takze ho tez ignorujeme
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
    char finalName[2 * MAX_PATH + 200];                                      // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
    BOOL sourcePathIsNet = (srcAndTgtPathsFlags & OPFL_SRCPATH_IS_NET) != 0; // platny jen pro atCopy a atMove

    script->DirsCount++;
    COperation op;
    //---  je-li potreba vytvorit adresar targetPath + dirName (Copy a Move)
    char* sourceEnd = sourcePath + strlen(sourcePath);
    char *st, *s = dirName;
    if (*(sourceEnd - 1) != '\\')
    {
        *sourceEnd = '\\';
        st = sourceEnd + 1;
    }
    else
        st = sourceEnd;
    if (st - sourcePath + strlen(dirName) >= MAX_PATH - 2) // -2 zjisteno experimentalne (delsi cesta nejde listovat)
    {                                                      // data jsou na disku, coz neznamena, ze nemuzou byt delsi nez MAX_PATH
        *sourceEnd = 0;                                    // zrestaurovani sourcePath
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
            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
  nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
{MNTT_PB, 0
{MNTT_IT, IDS_MSGBOXBTN_SKIP
{MNTT_IT, IDS_MSGBOXBTN_SKIPALL
{MNTT_IT, IDS_MSGBOXBTN_FOCUS
{MNTT_PE, 0
};
*/
            sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                    DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                    DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL),
                    DIALOG_OK, LoadStr(IDS_MSGBOXBTN_FOCUS));
            params.AliasBtnNames = aliasBtnNames;
            int msgRes = SalMessageBoxEx(&params);
            if (msgRes != DIALOG_YES /* Skip */ && msgRes != DIALOG_NO /* Skip All */)
                skip = FALSE;
            if (msgRes == DIALOG_NO /* Skip All */)
                ErrTooLongSrcDirNameSkipAll = TRUE;
            if (msgRes == DIALOG_OK /* Focus */)
                MainWindow->PostFocusNameInPanel(PANEL_SOURCE, sourcePath, dirName);
        }
        return skip;
    }
    while (*s != 0)
        *st++ = *s++;
    *st = 0;
    //---  konstrukce cesty k targetDirName
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
            // Petr: trochu prasarna: maska *.* neprodukuje kopii zdrojoveho jmena, coz je problem pri kopirovani
            // adresaru s invalidnim jmenem, napr. "c   ..." + "*.*" = "c   ", takze si trochu pomuzeme a nechame
            // masku zmenit na NULL = primitivni textova kopie jmena
            char* opMask = mask != NULL && strcmp(mask, "*.*") == 0 ? NULL : mask;
            s2 = MaskName(finalName, 2 * MAX_PATH + 200, dirName, opMask);
            if (opMask != NULL)
                checkNewDirName = strcmp(s2, dirName) != 0;
        }
        else
            s2 = mapName;
        if (strlen(s2) + targetLen >= PATH_MAX_PATH)
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            *targetEnd = 0; // zrestaurovani targetPath
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
                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_IT, IDS_MSGBOXBTN_FOCUS
  {MNTT_PE, 0
};
*/
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                        DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL),
                        DIALOG_OK, LoadStr(IDS_MSGBOXBTN_FOCUS));
                params.AliasBtnNames = aliasBtnNames;
                int msgRes = SalMessageBoxEx(&params);
                if (msgRes != DIALOG_YES /* Skip */ && msgRes != DIALOG_NO /* Skip All */)
                    skip = FALSE;
                if (msgRes == DIALOG_NO /* Skip All */)
                    ErrTooLongTgtDirNameSkipAll = TRUE;
                if (msgRes == DIALOG_OK /* Focus */)
                    MainWindow->PostFocusNameInPanel(PANEL_SOURCE, sourcePath, sourceEnd + 1);
            }
            return skip;
        }
        strcpy(targetPath + targetLen, s2);
        targetPathState = GetTargetPathState(targetPathState, targetPath);
    }
    //---
    if (type == atDelete && (sourceDirAttr & FILE_ATTRIBUTE_REPARSE_POINT))
    { // mazani linku (volume-mount-pointy + junction-pointy + symlinky)
        op.Opcode = ocDeleteDirLink;
        op.OpFlags = 0;
        op.Size = DELETE_DIRLINK_SIZE;
        op.Attr = sourceDirAttr;
        if ((op.SourceName = BuildName(sourcePath, NULL)) == NULL)
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            return FALSE;
        }
        op.TargetName = NULL;
        *sourceEnd = 0; // zrestaurovani sourcePath
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
        if (res == IDNO || res == IDCANCEL) // pokud dal CANCEL nebo NO, koncime nebo preskocime adresar
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            if (targetEnd != NULL)
                *targetEnd = 0; // zrestaurovani targetPath
            return res == IDNO;
        }
    }
    //---
    if (type == atMove)
    {
        if (strcmp(sourcePath, targetPath) == 0) // neni co delat, zarveme
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            *targetEnd = 0; // zrestaurovani targetPath
            SalMessageBox(MainWindow->HWindow, LoadStr(IDS_CANNOTMOVEDIRTOITSELF),
                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        BOOL sameDisk;
        sameDisk = FALSE;
        if (fastDirectoryMove &&
            !script->CopySecurity && // pokud se maji kopirovat prava, neni mozne presunout kompletni adresar (na kazdem souboru je potreba nechat system refreshnout "inherited" prava)
            (script->CopyAttrs ||    // pokud kopirujeme atributy, jde heuristika o nastavovani Encrypted atributu stranou
             targetPathState != tpsEncryptedExisting &&
                 targetPathState != tpsEncryptedNotExisting) && // pokud se maji nastavovat Encrypted atributy, neni mozne presunout kompletni adresar (je treba zkontrolovat obsah adresare)
            (filterCriteria == NULL || !filterCriteria->UseMasks &&
                                           !filterCriteria->UseAdvanced && !filterCriteria->SkipEmptyDirs)) // pokud se maji filtrovat soubory nebo adresare, neni mozne presunout kompletni adresar
        {
            sameDisk = !script->SameRootButDiffVolume &&
                       HasTheSameRootPath(sourcePath, targetPath); // stejny disk (UNC i normal)
        }
        else
            sameDisk = (StrICmp(sourcePath, targetPath) == 0); // jen rename
        if (sameDisk)
        {
            if (StrICmp(sourcePath, targetPath) == 0 ||
                targetPathState == tpsEncryptedNotExisting || targetPathState == tpsNotEncryptedNotExisting) // cilovy adresar neexistuje
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

                    *sourceEnd = 0; // zrestaurovani sourcePath
                    *targetEnd = 0; // zrestaurovani targetPath
                    return FALSE;
                }
                if ((op.TargetName = BuildName(targetPath, NULL)) == NULL)
                {
                    free(op.SourceName);
                    goto _ERROR;
                }
                *sourceEnd = 0; // zrestaurovani sourcePath
                *targetEnd = 0; // zrestaurovani targetPath
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
    if (type == atCopy || type == atMove) // vytvoreni ciloveho adresare
    {
        srcAndTgtPathsFlags &= ~(OPFL_SRCPATH_IS_NET | OPFL_SRCPATH_IS_FAST);
        srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(sourcePath, OPFL_SRCPATH_IS_NET, OPFL_SRCPATH_IS_FAST);
        if (targetPathState == tpsEncryptedExisting || targetPathState == tpsNotEncryptedExisting) // cilovy adresar existuje, zjistime jeho flagy (jinak si nechame flagy z nadrazeneho ciloveho adresare)
        {
            srcAndTgtPathsFlags &= ~(OPFL_TGTPATH_IS_NET | OPFL_TGTPATH_IS_FAST);
            srcAndTgtPathsFlags |= GetPathFlagsForCopyOp(targetPath, OPFL_TGTPATH_IS_NET, OPFL_TGTPATH_IS_FAST);
        }

        BOOL dirCreated = FALSE;
        if (sourcePathSupADS)
        {
            if ((targetPathState == tpsEncryptedNotExisting || targetPathState == tpsNotEncryptedNotExisting) && // cilovy adresar neexistuje
                (targetPathSupADS || !ConfirmADSLossAll))                                                        // pokud se nemaji ADS ignorovat
            {
                if (script->BytesPerCluster == 0)
                    TRACE_E("How is it possible that script->BytesPerCluster is not yet set???");

                CQuadWord adsSize;
                CQuadWord adsOccupiedSpace;
                DWORD adsWinError;

            READADS_AGAIN:

                if (CheckFileOrDirADS(sourcePath, TRUE, &adsSize, NULL, NULL, NULL, &adsWinError,
                                      script->BytesPerCluster, &adsOccupiedSpace, NULL))
                { // zdrojovy adresar ma ADS, musime je okopirovat do ciloveho adresare
                    if (targetPathSupADS)
                    {
                        script->OccupiedSpace += adsOccupiedSpace;
                        script->TotalFileSize += adsSize;

                        op.Opcode = ocCreateDir;
                        op.OpFlags = OPFL_COPY_ADS | (checkNewDirName ? 0 : OPFL_IGNORE_INVALID_NAME);
                        if (!script->CopyAttrs && // pokud kopirujeme atributy, jde heuristika o nastavovani Encrypted atributu stranou
                            ((sourceDirAttr & FILE_ATTRIBUTE_ENCRYPTED) || targetPathState == tpsEncryptedExisting ||
                             targetPathState == tpsEncryptedNotExisting))
                        {
                            op.OpFlags |= OPFL_AS_ENCRYPTED;
                        }
                        if (type == atMove && !script->ShowStatus)
                            script->ShowStatus = TRUE; // pokud neni mozny move celeho adresare (duvody viz vyse) a je treba kopirovat ADS, je potreba zobrazit status
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
                    else // kopirovani na jiny FS nez NTFS (dotaz na oriznuti ADS)
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
                            ConfirmADSLossAll = TRUE; // tady break; nechybi
                        case IDYES:
                            break; // budeme ignorovat ADS, cim se nezkopiruji/nepresunou (cimz se zcela ztrati)

                        case IDB_SKIPALL:
                            ConfirmADSLossSkipAll = TRUE; // tady break; nechybi
                        case IDB_SKIP:
                        {
                            *sourceEnd = 0; // zrestaurovani sourcePath
                            *targetEnd = 0; // zrestaurovani targetPath
                            return TRUE;
                        }

                        case IDCANCEL:
                        {
                            *sourceEnd = 0; // zrestaurovani sourcePath
                            *targetEnd = 0; // zrestaurovani targetPath
                            return FALSE;
                        }
                        }
                    }
                }
                else // doslo k chybe nebo zadne ADS
                {
                    if (adsWinError != NO_ERROR &&                                                                    // doslo k chybe
                        (adsWinError != ERROR_INVALID_FUNCTION || StrNICmp(sourcePath, "\\\\tsclient\\", 11) != 0) && // cesty na lokalni disky v Terminal Serveru nepodporuji listovani ADS (jinak ADS podporuji, komedie)
                        (adsWinError != ERROR_INVALID_PARAMETER && adsWinError != ERROR_NO_MORE_ITEMS ||
                         !sourcePathIsNet)) // namounteny FAT/FAT32 disk nelze poznat na sitovem disku (napr. \\petr\f\drive_c) + NOVELL-NETWAREsky svazek prochazeny pres NDS - myslime si, ze je to NTFS a tudiz zkousime cist ADS, coz ohlasi tuto chybu
                    {
                        if ((sourceDirAttr & FILE_ATTRIBUTE_REPARSE_POINT) == 0) // nejde o link (u toho nemusi dojit ke kopirovani obsahu)
                        {
                            // nejprve zkusime, jestli dojde k chybe i pri listovani adresare - takovou chybu
                            // user snaze pochopi, proto ji zobrazime prednostne (pred chybou cteni ADS)
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
                                            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};
*/
                                            sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                                                    DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                                                    DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                                            params.AliasBtnNames = aliasBtnNames;
                                            int msgRes = SalMessageBoxEx(&params);
                                            if (msgRes != DIALOG_YES /* Skip */ && msgRes != DIALOG_NO /* Skip All */)
                                                skip = FALSE;
                                            if (msgRes == DIALOG_NO /* Skip All */)
                                                ErrListDirSkipAll = TRUE;
                                        }
                                        *sourceEnd = 0; // zrestaurovani sourcePath
                                        *targetEnd = 0; // zrestaurovani targetPath
                                        return skip;
                                    }
                                }
                                else
                                    HANDLES(FindClose(search));
                            }
                        }

                        // listovani adresare je bez problemu (nebo nastala neocekavana chyba), ohlasime chybu ADS
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
                            ErrReadingADSIgnoreAll = TRUE; // tady break; nechybi
                        case IDB_IGNORE:
                            break;

                        case IDCANCEL:
                        {
                            *sourceEnd = 0; // zrestaurovani sourcePath
                            *targetEnd = 0; // zrestaurovani targetPath
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
            if (!script->CopyAttrs && // pokud kopirujeme atributy, jde heuristika o nastavovani Encrypted atributu stranou
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
            *sourceEnd = 0; // zrestaurovani sourcePath
            return FALSE;
        }
        op.TargetName = (char*)(DWORD_PTR)((sourceDirAttr & attrsData->AttrAnd) | attrsData->AttrOr);
        script->Add(op);
        if (!script->IsGood())
        {
            script->ResetState();
            free(op.SourceName);
            *sourceEnd = 0; // zrestaurovani sourcePath
            return FALSE;
        }

        if (!attrsData->SubDirs)
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            return TRUE;
        }
    }

    BOOL copyMoveDirIsLink = FALSE;
    BOOL copyMoveSkipLinkContent = FALSE;
    if ((type == atCopy || type == atMove) && // pokud jde o link, zjistime, jestli obsah linku preskocit nebo zkopirovat
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
                    if (repPointType == 1 /* MOUNT POINT */)
                        strcpy_s(detailsTxt, LoadStr(IDS_VOLMOUNTPOINT));
                    else
                    {
                        sprintf_s(detailsTxt, LoadStr(repPointType == 2 /* JUNCTION POINT */ ? IDS_INFODLGTYPE9 : IDS_INFODLGTYPE10),
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
            ConfirmCopyLinkContentAll = TRUE; // tady break; nechybi
        case IDYES:
            break; // mame zkopirovat obsah linku do cile

        case IDB_SKIPALL:
            ConfirmCopyLinkContentSkipAll = TRUE; // tady break; nechybi
        case IDB_SKIP:
            copyMoveSkipLinkContent = TRUE;
            break; // mame preskocit obsah linku (nekopirujeme ho)

        case IDCANCEL:
        {
            *sourceEnd = 0; // zrestaurovani sourcePath
            *targetEnd = 0; // zrestaurovani targetPath
            return FALSE;
        }
        }
    }
    //---  konstrukce cesty k sourceDirName + zacatek hledani obsazenych souboru
    BOOL delDirectory = TRUE;       // mazat neprazdny adresar?
    BOOL delDirectoryReturn = TRUE; // navratova hodnota pri nemazani neprazdneho adresare
    BOOL canDelDirAfterMove = TRUE; // jen pro Move: FALSE = nepresouva se vsechno (filtr neco skipnul), nelze smazat zdrojovy adresar (nezustane prazdny)
    if (!copyMoveDirIsLink || !copyMoveSkipLinkContent)
    {
        WIN32_FIND_DATA f;
        strcpy(st, "\\*");
        HANDLE search = HANDLES_Q(FindFirstFile(sourcePath, &f));
        *st = 0; // odriznuti "\\*"
        if (search == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            if (err == ERROR_PATH_NOT_FOUND && type == atCountSize && dirDOSName != NULL && strcmp(dirName, dirDOSName) != 0)
            { // patch pro vypocet velikosti adresare, ke kteremu se musi pristupovat pres DOS-name, kdyz to neumime pres UNICODE jmeno (multibyte verze jmena po prevodu zpet na UNICODE neodpovida puvodnimu UNICODE jmenu adresare)
                lstrcpyn(finalName, sourcePath, 2 * MAX_PATH + 200);
                if (CutDirectory(finalName) &&
                    SalPathAppend(finalName, dirDOSName, 2 * MAX_PATH + 200) &&
                    SalPathAppend(finalName, "*", 2 * MAX_PATH + 200))
                {
                    search = HANDLES_Q(FindFirstFile(finalName, &f));
                    if (search != INVALID_HANDLE_VALUE)
                    {
                        strcpy(*sourceEnd == '\\' ? sourceEnd + 1 : sourceEnd, dirDOSName); // predelame sourcePath (dal se pouziva pro praci s nalezenymi soubory a adresari)
                        goto BROWSE_DIR;
                    }
                }
            }
            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)
            {
                sprintf(text, LoadStr(IDS_CANNOTREADDIR), sourcePath, GetErrorText(err));
                *sourceEnd = 0; // zrestaurovani sourcePath
                if (targetEnd != NULL)
                    *targetEnd = 0; // zrestaurovani targetPath
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
                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};
*/
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                            DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                            DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                    params.AliasBtnNames = aliasBtnNames;
                    int msgRes = SalMessageBoxEx(&params);
                    if (msgRes != DIALOG_YES /* Skip */ && msgRes != DIALOG_NO /* Skip All */)
                        skip = FALSE;
                    if (msgRes == DIALOG_NO /* Skip All */)
                        ErrListDirSkipAll = TRUE;
                }
                if (!skip)
                    return FALSE;
                UpdateWindow(MainWindow->HWindow);
            }
            else
            {
                *sourceEnd = 0; // zrestaurovani sourcePath
                if (targetEnd != NULL)
                    *targetEnd = 0; // zrestaurovani targetPath
            }
        }
        else
        {

        BROWSE_DIR:

            //---  prohledani adresare
            BOOL askDirDelete = (type == atDelete && firstLevelDir && Configuration.CnfrmNEDirDel);
            BOOL testFindNextErr = TRUE;
            do
            {
                if (f.cFileName[0] == '.' &&
                        (f.cFileName[1] == 0 || (f.cFileName[1] == '.' && f.cFileName[2] == 0)) ||
                    f.cFileName[0] == 0)
                    continue; // "." a ".." + prazdna jmena (vede na nekonecnou rekurzi)

                if (askDirDelete)
                {
                    sprintf(text, LoadStr(IDS_NONEMPTYDIRDELCONFIRM), sourcePath);
                    int res = SalMessageBox(MainWindow->HWindow, text, LoadStr(IDS_QUESTION),
                                            MB_YESNOCANCEL | MB_ICONQUESTION);
                    UpdateWindow(MainWindow->HWindow);
                    delDirectoryReturn = (res != IDCANCEL); // pokud nedal CANCEL, pokracujeme
                    delDirectory = (res == IDYES);
                    if (!delDirectory)
                    {
                        testFindNextErr = FALSE;
                        break;
                    }
                    askDirDelete = FALSE;
                }
                //---  nechce nekdo build scriptu prerusit ?
                if (GetTickCount() - LastTickCount > BS_TIMEOUT)
                {
                    if (UserWantsToCancelSafeWaitWindow())
                    {
                        MSG msg; // vyhodime nabufferovany ESC
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

                //---  build adresare nebo souboru
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
                        *sourceEnd = 0; // zrestaurovani sourcePath
                        if (targetEnd != NULL)
                            *targetEnd = 0; // zrestaurovani targetPath
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
                        canDelDirAfterMove = FALSE; // nepresouva se vsechno (filtr neco skipnul), nelze smazat zdrojovy adresar (nezustane prazdny)
                }
            } while (FindNextFile(search, &f));
            DWORD err = GetLastError();
            HANDLES(FindClose(search));

            *sourceEnd = 0; // zrestaurovani sourcePath
            if (targetEnd != NULL)
                *targetEnd = 0; // zrestaurovani targetPath

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
                    /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};
*/
                    sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                            DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                            DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                    params.AliasBtnNames = aliasBtnNames;
                    int msgRes = SalMessageBoxEx(&params);
                    if (msgRes != DIALOG_YES /* Skip */ && msgRes != DIALOG_NO /* Skip All */)
                        skip = FALSE;
                    if (msgRes == DIALOG_NO /* Skip All */)
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
        *sourceEnd = 0; // zrestaurovani sourcePath
        if (targetEnd != NULL)
            *targetEnd = 0; // zrestaurovani targetPath
    }
    //---  change-case: zmena nazvu az po dokonceni operaci uvnitr
    if (type == atChangeCase)
    {
        op.Opcode = ocMoveDir;
        op.OpFlags = 0; // zmena case = prejmenovani = invalidni jmena budeme hlasit (nejde jen o toleranci existujiciho)
        op.Size = MOVE_DIR_SIZE;
        op.Attr = sourceDirAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, dirName, NULL, &skip,
                                       &ErrTooLongDirNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        if ((op.TargetName = BuildName(sourcePath, dirName)) == NULL) // prilis dlouhe jmeno nehrozi, uz by se vyresilo v predchozi podmince
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
    // pokud tento adresar obsahuje nejakou skipnutou polozku, nelze smazat ani nadrazeny adresar
    // pokud jde jen o link na adresar, smazeme ho bez ohledu na to, jestli neco uvnitr zustalo
    if (!copyMoveDirIsLink && canDelUpperDirAfterMove != NULL && !canDelDirAfterMove)
        *canDelUpperDirAfterMove = FALSE;
    // pokud se nezkopirovalo ani nepresunulo nic uvnitr adresare a mame prenaset jen soubory,
    // zrusime vytvareni adresare (zbytecny prazdny adresar), pokud slo o link na adresar,
    // tak toho se tahle vec netyka (je to link a ne adresar)
    if (!copyMoveDirIsLink && (type == atCopy || type == atMove) && filterCriteria != NULL &&
        filterCriteria->SkipEmptyDirs && createDirIndex >= 0 &&
        createDirIndex == script->Count - 1)
    {
        free(script->At(createDirIndex).SourceName);
        free(script->At(createDirIndex).TargetName);
        script->Delete(createDirIndex);
        if (!script->IsGood())
            script->ResetState();
        // pokud se skipuje tento adresar, nelze smazat ani nadrazeny adresar
        if (canDelUpperDirAfterMove != NULL)
            *canDelUpperDirAfterMove = FALSE;
    }
    else
    {
        // mame-li zachovavat cas&datum adresaru, ulozime jeste operaci nastaveni datumu&casu
        // adresare (lze az po dokonceni zapisu podadresaru a souboru do tohoto adresare)
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

        // je-li potreba smazat adresar nebo link na adresar sourcePath + dirName (delete a move)
        if (copyMoveDirIsLink && type == atMove ||                                          // u linku neni canDelDirAfterMove podstatne (link jde smaznout vzdy)
            !copyMoveDirIsLink && type == atMove && canDelDirAfterMove || type == atDelete) // smazani zdrojoveho adresare nebo linku na adresar
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
                    skipTooLongSrcNameErr = TRUE; // chceme jeste pridat znacku pro skip vytvareni adresare
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

        // je-li potreba, ulozime jeste znacku pro skip vytvareni adresare
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
            *ignoreAll = TRUE; // tady break; nechybi
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
        if (!script->CopyAttrs && // pokud kopirujeme atributy, jde heuristika o nastavovani Encrypted atributu stranou
            ((sourceFileAttr & FILE_ATTRIBUTE_ENCRYPTED) || targetPathState == tpsEncryptedExisting ||
             targetPathState == tpsEncryptedNotExisting))
        {
            op.OpFlags |= OPFL_AS_ENCRYPTED; // pokud staci rename (move v ramci jednoho svazku), flag zase sestrelime
            if (type == atMove && !script->ShowStatus)
                script->ShowStatus = TRUE; // move s nastavenim encrypted atributu se dela pres kopirovani, tedy potrebujeme zobrazeni statusu
        }
        op.Attr = sourceFileAttr;
        BOOL skip;
        if ((op.SourceName = BuildName(sourcePath, fileName, fileDOSName, &skip,
                                       &ErrTooLongNameSkipAll, sourcePath)) == NULL)
        {
            return skip;
        }
        if (targetPathIsFAT32 && fileSizeLoc > CQuadWord(0xFFFFFFFF /* 4GB minus 1 Byte */, 0))
        { // prilis velky soubor pro FAT32 (varujeme usera, ze operace nejspis uspesne nedobehne)

        FAT_TOO_BIG_FILE:

            int msgRes = DIALOG_YES /* Skip */;
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
                /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   nechame pro tlacitka msgboxu resit kolize hotkeys tim, ze simulujeme, ze jde o menu
MENU_TEMPLATE_ITEM MsgBoxButtons[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MSGBOXBTN_SKIP
  {MNTT_IT, IDS_MSGBOXBTN_SKIPALL
  {MNTT_PE, 0
};
*/
                sprintf(aliasBtnNames, "%d\t%s\t%d\t%s",
                        DIALOG_YES, LoadStr(IDS_MSGBOXBTN_SKIP),
                        DIALOG_NO, LoadStr(IDS_MSGBOXBTN_SKIPALL));
                params.AliasBtnNames = aliasBtnNames;
                msgRes = SalMessageBoxEx(&params);
                if (msgRes == DIALOG_NO /* Skip All */)
                    ErrTooBigFileFAT32SkipAll = TRUE;
            }
            free(op.SourceName);
            return (msgRes == DIALOG_YES /* Skip */ || msgRes == DIALOG_NO /* Skip All */);
        }
        char finalName[2 * MAX_PATH + 200]; // + 200 je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        if (mapName == NULL)
        {
            // Petr: trochu prasarna: maska *.* neprodukuje kopii zdrojoveho jmena, coz je problem pri kopirovani
            // souboru s invalidnim jmenem, napr. "c   ..." + "*.*" = "c   ", takze si trochu pomuzeme a nechame
            // masku zmenit na NULL = primitivni textova kopie jmena
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
            if (type == atMove) // premisteni tam kde uz je ...
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
        // pokud staci rename (move v ramci jednoho svazku), flag OPFL_AS_ENCRYPTED zase sestrelime
        if (op.Opcode == ocMoveFile && (op.OpFlags & OPFL_AS_ENCRYPTED) &&
            (sourceFileAttr & FILE_ATTRIBUTE_ENCRYPTED) &&
            !script->SameRootButDiffVolume && HasTheSameRootPath(sourcePath, targetPath))
        {
            op.OpFlags &= ~OPFL_AS_ENCRYPTED;
        }
        if (type == atCopy || op.Opcode == ocMoveFile && (op.OpFlags & OPFL_AS_ENCRYPTED) ||
            script->SameRootButDiffVolume || !HasTheSameRootPath(sourcePath, targetPath))
        {
            // pokud cesta konci mezerou/teckou, je invalidni a nesmime provest kopii,
            // CreateFile mezery/tecky orizne a prekopiroval by se tak jiny soubor pripadne do jineho jmena
            BOOL invalidSrcName = FileNameIsInvalid(op.SourceName, TRUE);

            // optimalizace "overwrite older" pro kopirovani z pomale site na rychly lokalni disk
            // (cteni casu souboru na pomale siti je daleko rychlejsi, kdyz se cte souvisle listing
            // z cesty, nez kdyz se pak dotazujeme jeden soubor po druhem)
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
                        if (StrICmp(tgtName, dataOut.cFileName) == 0 &&                 // pokud nejde jen o shodu DOS-name (tam dojde ke zmene DOS-name a ne k prepisu)
                            (dataOut.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0) // pokud nejde o adresar (s tim overwrite-older nic nezmuze)
                        {
                            // orizneme casy na sekundy (ruzne FS ukladaji casy s ruznymi prestnostmi, takze dochazelo k "rozdilum" i mezi "shodnymi" casy)
                            FILETIME roundedInTime;
                            *(unsigned __int64*)&roundedInTime = *(unsigned __int64*)fileLastWriteTime - (*(unsigned __int64*)fileLastWriteTime % 10000000);
                            *(unsigned __int64*)&dataOut.ftLastWriteTime = *(unsigned __int64*)&dataOut.ftLastWriteTime - (*(unsigned __int64*)&dataOut.ftLastWriteTime % 10000000);

                            if (CompareFileTime(&roundedInTime, &dataOut.ftLastWriteTime) <= 0) // zdrojovy soubor neni novejsi nez cilovy soubor - skipneme copy operaci
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

            // linky: fileSizeLoc == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
            if ((sourceFileAttr & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
            {
                BOOL cancel;
                CQuadWord size;
                if (GetLinkTgtFileSize(HWindow, NULL, &op, &size, &cancel, &ErrGetFileSizeOfLnkTgtIgnAll))
                {
                    fileSizeLoc = size;
                    op.FileSize = fileSizeLoc;

                    // mame novou velikost souboru, tohle musime poresit znovu:
                    // prilis velky soubor pro FAT32 (varujeme usera, ze operace nejspis uspesne nedobehne)
                    if (targetPathIsFAT32 && fileSizeLoc > CQuadWord(0xFFFFFFFF /* 4GB minus 1 Byte */, 0))
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
                op.Size = COPY_MIN_FILE_SIZE; // nulove/male soubory trvaji aspon jako soubory s velikosti COPY_MIN_FILE_SIZE

            if (sourcePathSupADS &&                       // pokud je sance, ze najdeme ADS a
                (targetPathSupADS || !ConfirmADSLossAll)) // pokud se nemaji ADS ignorovat
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
                { // zdrojovy soubor ma ADS, musime je okopirovat do ciloveho souboru
                    if (targetPathSupADS)
                    {
                        op.OpFlags |= OPFL_COPY_ADS;
                        op.Size += adsSize;
                        script->OccupiedSpace += adsOccupiedSpace;
                        script->TotalFileSize += adsSize;
                    }
                    else // kopirovani na jiny FS nez NTFS (dotaz na oriznuti ADS)
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
                            ConfirmADSLossAll = TRUE; // tady break; nechybi
                        case IDYES:
                            break; // budeme ignorovat ADS, cimz se nezkopiruji/nepresunou (cimz se zcela ztrati)

                        case IDB_SKIPALL:
                            ConfirmADSLossSkipAll = TRUE; // tady break; nechybi
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
                else // doslo k chybe nebo zadne ADS
                {
                    if (invalidSrcName ||
                        adsWinError != NO_ERROR &&                                                                           // doslo k chybe
                            (adsWinError != ERROR_INVALID_FUNCTION || StrNICmp(op.SourceName, "\\\\tsclient\\", 11) != 0) && // cesty na lokalni disky v Terminal Serveru nepodporuji listovani ADS (jinak ADS podporuji, komedie)
                            (adsWinError != ERROR_INVALID_PARAMETER && adsWinError != ERROR_NO_MORE_ITEMS ||
                             (srcAndTgtPathsFlags & OPFL_SRCPATH_IS_NET) == 0)) // namounteny FAT/FAT32 disk nelze poznat na sitovem disku (napr. \\petr\f\drive_c) + NOVELL-NETWAREsky svazek prochazeny pres NDS - myslime si, ze je to NTFS a tudiz zkousime cist ADS, coz ohlasi tuto chybu
                    {
                        // nejprve zkusime, jestli dojde k chybe i pri otevreni souboru - takovou chybu
                        // user snaze pochopi, proto ji zobrazime prednostne (pred chybou cteni ADS)
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
                        if (!invalidSrcName && in != INVALID_HANDLE_VALUE) // otevirani souboru je bez problemu, ohlasime chybu ADS
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
                                ErrReadingADSIgnoreAll = TRUE; // tady break; nechybi
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
                        else // ohlasime chybu otevirani souboru
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
                                ErrFileSkipAll = TRUE; // tady break; nechybi
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
        if (script->BytesPerCluster == 0) // pri estimate space nehrozi
        {
            DWORD d1, d2, d3, d4;
            if (MyGetDiskFreeSpace(sourcePath, &d1, &d2, &d3, &d4))
                script->BytesPerCluster = d1 * d2;
        }

        char name[2 * MAX_PATH]; // + MAX_PATH je rezerva (Windows delaji cesty delsi nez MAX_PATH)
        int l = (int)strlen(sourcePath);
        memmove(name, sourcePath, l);
        if (name[l - 1] != '\\')
            name[l++] = '\\';
        memmove(name + l, fileName, 1 + strlen(fileName)); // name je vzdy < MAX_PATH
        CQuadWord s;
        DWORD err = NO_ERROR;
        if (FileBasedCompression && !onlySize &&                                         // pokud je komprimace vubec mozna
            (sourceFileAttr & (FILE_ATTRIBUTE_COMPRESSED | FILE_ATTRIBUTE_SPARSE_FILE))) // pokud je soubor komprimovany nebo ridky (sparse-file)
        {
            s.LoDWord = GetCompressedFileSize(name, &s.HiDWord);
            err = GetLastError();
            if (err == ERROR_FILE_NOT_FOUND && fileDOSName != NULL && strcmp(fileName, fileDOSName) != 0)
            {                                                            // patch pro vypocet velikosti souboru, ke kteremu se musi pristupovat pres DOS-name, kdyz to neumime pres UNICODE jmeno (multibyte verze jmena po prevodu zpet na UNICODE neodpovida puvodnimu UNICODE jmenu souboru)
                memmove(name + l, fileDOSName, 1 + strlen(fileDOSName)); // name je vzdy < MAX_PATH
                s.LoDWord = GetCompressedFileSize(name, &s.HiDWord);
                err = GetLastError();
                if (s.LoDWord == 0xFFFFFFFF && err != NO_ERROR)
                    memmove(name + l, fileName, 1 + strlen(fileName)); // (name je vzdy < MAX_PATH) - pro pripad chyby, hlaseni bude o plnem jmene a ne o DOS jmene
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
            s = fileSizeLoc; // nelze zjistit compressed-size, spokojime se s normalni velikosti
        }

        script->Sizes.Add(fileSizeLoc); // vystupni dialog je pripraven na pripad, kdy bude toto pole v chybovem stavu
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

        // linky: fileSizeLoc == 0, velikost souboru se musi ziskat pres GetLinkTgtFileSize() dodatecne
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
            op.Size = CONVERT_MIN_FILE_SIZE; // nulove/male soubory trvaji aspon jako soubory s velikosti CONVERT_MIN_FILE_SIZE
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
        // komprese: nulove/male soubory trvaji aspon jako soubory s velikosti COMPRESS_ENCRYPT_MIN_FILE_SIZE
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
        if ((op.TargetName = BuildName(sourcePath, fileName)) == NULL) // pokud je prilis dlouhe jmeno, projevi se to uz o podminku drive
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
    return FALSE; // nic jineho neumi
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

    // zjistime jestli umime do archivu zabalovat (je mozna editace souboru z archivu, jinak varujeme)
    if (edit)
    {
        int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
        if (format != 0) // "always-true" - nasli jsme podporovany archiv
        {
            if (!PackerFormatConfig.GetUsePacker(format - 1)) // nema edit?
            {
                if (SalMessageBox(HWindow, LoadStr(IDS_EDITPACKNOTSUPPORTED),
                                  LoadStr(IDS_QUESTION), MB_YESNO | MB_DEFBUTTON2 | MB_ICONQUESTION) != IDYES)
                {
                    return; // akce se rusi (user nechce editovat, kdyz nejde updatnout archiv)
                }
            }
        }
    }

    //---  ziskame plne dlouhe jmeno
    char dcFileName[2 * MAX_PATH]; // ZIP: jmeno pro disk-cache
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
        if (index != j) // neporovnavat dva stejne
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

    StrICpy(dcFileName, GetZIPArchive()); // jmeno souboru archivu se ma porovnavat "case-insensitive" (Windows file system), prevedeme ho proto vzdy na mala pismenka
    SalPathAppend(dcFileName, GetZIPPath(), 2 * MAX_PATH);
    SalPathAppend(dcFileName, f->Name, 2 * MAX_PATH);

    // nastaveni disk-cache pro plugin (std. hodnoty se zmeni jen u pluginu)
    char arcCacheTmpPath[MAX_PATH];
    arcCacheTmpPath[0] = 0;
    BOOL arcCacheOwnDelete = FALSE;
    BOOL arcCacheCacheCopies = TRUE;
    CPluginInterfaceAbstract* plugin = NULL; // != NULL pokud ma plugin sve vlastni mazani
    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
    if (format != 0) // nasli jsme podporovany archiv
    {
        format--;
        int index2 = PackerFormatConfig.GetUnpackerIndex(format);
        if (index2 < 0) // view: jde o interni zpracovani (plug-in)?
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
    if (!exists) // musime ho vypakovat
    {
        char* backSlash = strrchr(name, '\\');
        char tmpPath[MAX_PATH];
        memcpy(tmpPath, name, backSlash - name);
        tmpPath[backSlash - name] = 0;
        BeginStopRefresh(); // cmuchal si da pohov
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
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
        }
        else
        {
            SetCursor(oldCur);
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
            DiskCache.ReleaseName(dcFileName, FALSE); // nevypakovano, neni co cachovat
            EndStopRefresh();                         // ted uz zase cmuchal nastartuje
            return;
        }
    }

    // rozdeleni plneho jmena k souboru na cestu (buf) a jmeno (s)
    char buf[MAX_PATH];
    char* s = strrchr(name, '\\');
    if (s != NULL)
    {
        memcpy(buf, name, s - name);
        buf[s - name] = 0;
        s++;
    }

    // spusteni default polozky z kontextoveho menu (asociace)
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
    {                                                                         // tento soubor jeste nema v disk-cache 'lock' objekt ExecuteAssocEvent
        DiskCache.AssignName(dcFileName, ExecuteAssocEvent, FALSE, crtCache); // arcCacheCacheCopies nema vliv - cachuje se az do zavreni archivu, drive zapakovavat nebudeme
    }
    else
    { // je zbytecne pridavat tmp-souboru ten samy 'lock' objekt
        DiskCache.ReleaseName(dcFileName, FALSE);
    }
    AssocUsed = TRUE;
}
