// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "mainwnd.h"
#include "stswnd.h"
#include "dialogs.h"
#include "shellib.h"
#include "pack.h"
#include "drivelst.h"
#include "snooper.h"
#include "zip.h"
#include "shiconov.h"

//
// ****************************************************************************
// CFilesWindow
//

int DeltaForTotalCount(int total)
{
    int delta = total / 10;
    if (delta < 1)
        delta = 1;
    else if (delta > 10000)
        delta = 10000;
    return delta;
}

#ifndef _WIN64

BOOL AddWin64RedirectedDir(const char* path, CFilesArray* dirs, WIN32_FIND_DATA* fileData,
                           int* index, BOOL* dirWithSameNameExists); // is further in this module

#endif // _WIN64

#ifndef IO_REPARSE_TAG_FILE_PLACEHOLDER
#define IO_REPARSE_TAG_FILE_PLACEHOLDER (0x80000015L) // winnt
#endif                                                // IO_REPARSE_TAG_FILE_PLACEHOLDER

// taken from: http://msdn.microsoft.com/en-us/library/windows/desktop/dn323738%28v=vs.85%29.aspx
BOOL IsFilePlaceholder(WIN32_FIND_DATA const* findData)
{
    return (findData->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) &&
           (findData->dwReserved0 == IO_REPARSE_TAG_FILE_PLACEHOLDER);
}

BOOL CFilesWindow::ReadDirectory(HWND parent, BOOL isRefresh)
{
    CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory()");

    //  TRACE_I("ReadDirectory: begin");

    //  MainWindow->ReleaseMenuNew();  // in case of it's about this directory
    HiddenDirsFilesReason = 0;
    HiddenDirsCount = HiddenFilesCount = 0;

    CutToClipChanged = FALSE; // forget cut-to-clip flags by this operation

    FocusFirstNewItem = FALSE;
    UseSystemIcons = FALSE;
    UseThumbnails = FALSE;
    Files->DestroyMembers();
    Dirs->DestroyMembers();
    VisibleItemsArray.InvalidateArr();
    VisibleItemsArraySurround.InvalidateArr();
    SelectedCount = 0;
    NeedRefreshAfterIconsReading = FALSE; // refresh would make no sense now (if needed, it will be set again during icon reading)
    NumberOfItemsInCurDir = 0;
    InactWinOptimizedReading = FALSE;

    // icon-cache cleanup
    SleepIconCacheThread();
    IconCache->Release();
    EndOfIconReadingTime = GetTickCount() - 10000;
    StopThumbnailLoading = FALSE; // icon-cache is cleaned, the period of impossibility of using data about "thumbnail-loaders" in icon-cache ends

    TemporarilySimpleIcons = FALSE;

    if (ColumnsTemplateIsForDisk != Is(ptDisk))
        BuildColumnsTemplate();                      // it's necessary to build template again when panel type changes
    CopyColumnsTemplateToColumns();                  // fetching standard columns from cache
    DeleteColumnsWithoutData();                      // removing columns for which we don't have data (empty values would be shown in them)
    GetPluginIconIndex = InternalGetPluginIconIndex; // setting standard callback (just returns zero)

    char fileName[MAX_PATH + 4];

    if (Is(ptDisk))
    {
        // setting icon size for IconCache
        CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
        IconCache->SetIconSize(iconSize);

        BOOL readThumbnails = (GetViewMode() == vmThumbnails);

        CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::disk1");
        // choosing plugins which can load thumbnails (for optimization)
        TIndirectArray<CPluginData> thumbLoaderPlugins(10, 10, dtNoDelete);
        TIndirectArray<CPluginData> foundThumbLoaderPlugins(10, 10, dtNoDelete); // the array for plugins which can load thumbnails for the current file
        if (readThumbnails)
        {
            if (thumbLoaderPlugins.IsGood())
                Plugins.AddThumbLoaderPlugins(thumbLoaderPlugins);
            if (!thumbLoaderPlugins.IsGood() ||
                !foundThumbLoaderPlugins.IsGood() || // error (not enough memory?)
                thumbLoaderPlugins.Count == 0)       // or we do not have any plugin which can load thumbnails -> we will not load thumbnails
            {
                if (!thumbLoaderPlugins.IsGood())
                    thumbLoaderPlugins.ResetState();
                if (!foundThumbLoaderPlugins.IsGood())
                    foundThumbLoaderPlugins.ResetState();
                readThumbnails = FALSE;
            }
        }
        UseThumbnails = readThumbnails;

        SetCurrentDirectory(GetPath()); // so that it works better

#ifndef _WIN64
        BOOL isWindows64BitDir = Windows64Bit && WindowsDirectory[0] != 0 && IsTheSamePath(GetPath(), WindowsDirectory);
#endif // _WIN64

        RefreshDiskFreeSpace(FALSE);

        Files->SetDeleteData(TRUE);
        Dirs->SetDeleteData(TRUE);

        if (WaitForESCReleaseBeforeTestingESC) // waiting for ESC release (so that listing is not interrupted
                                               // immediately - this ESC probably ended modal dialog/messagebox)
        {
            WaitForESCRelease();
            WaitForESCReleaseBeforeTestingESC = FALSE; // another waiting makes no sense
        }

        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help

        GetRootPath(fileName, GetPath());
        BOOL isRootPath = (strlen(GetPath()) <= strlen(fileName));

        //--- getting drive type (we will not bother network drives with getting shares)
        UINT drvType = MyGetDriveType(GetPath());
        BOOL testShares = drvType != DRIVE_REMOTE;
        if (testShares)
            Shares.PrepareSearch(GetPath());
        switch (drvType)
        {
        case DRIVE_REMOVABLE:
        {
            BOOL isDriveFloppy = FALSE; // floppies have their own configuration beside other removable drives
            int drv = UpperCase[fileName[0]] - 'A' + 1;
            if (drv >= 1 && drv <= 26) // doing "range-check" for sure
            {
                DWORD medium = GetDriveFormFactor(drv);
                if (medium == 350 || medium == 525 || medium == 800 || medium == 1)
                    isDriveFloppy = TRUE;
            }
            UseSystemIcons = isDriveFloppy ? !Configuration.DrvSpecFloppySimple : !Configuration.DrvSpecRemovableSimple;
            break;
        }

        case DRIVE_REMOTE:
        {
            UseSystemIcons = !Configuration.DrvSpecRemoteSimple;
            break;
        }

        case DRIVE_CDROM:
        {
            UseSystemIcons = !Configuration.DrvSpecCDROMSimple;
            break;
        }

        default: // case DRIVE_FIXED:   // not just fixed, but also the others (RAM DISK, etc.)
        {
            UseSystemIcons = !Configuration.DrvSpecFixedSimple;
            break;
        }
        }

        const char* s = GetPath();
        char* st = fileName;
        while (*s != 0)
            *st++ = *s++;
        if (s == GetPath())
        {
            SetCurrentDirectoryToSystem();
            DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
            //      TRACE_I("ReadDirectory: end");
            return FALSE; // empty string on input
        }
        if (*(st - 1) != '\\')
            *st++ = '\\';
        strcpy(st, "*");
        char* fileNameEnd = st;
        //--- preparing for reading icons
        if (UseSystemIcons)
        {
            IconCacheValid = FALSE;
            MSG msg; // we must destroy possible WM_USER_ICONREADING_END which would set IconCacheValid = TRUE
            while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                ;

            int i;
            for (i = 0; i < Associations.Count; i++)
            {
                if (Associations[i].GetIndex(iconSize) == -3)
                    Associations[i].SetIndex(-1, iconSize); // removing the flag "loaded icon"
            }
        }
        else
        {
            if (UseThumbnails)
            {
                IconCacheValid = FALSE;
                MSG msg; // we must destroy possible WM_USER_ICONREADING_END which would set IconCacheValid = TRUE
                while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                    ;
            }
        }
        //--- reading directory content
        BOOL upDir;
        BOOL UNCRootUpDir = FALSE;
        if (GetPath()[0] == '\\' && GetPath()[1] == '\\')
        {
            if (GetPath()[2] == '.' && GetPath()[3] == '\\' && GetPath()[4] != 0 && GetPath()[5] == ':') // "\\.\C:\" type path
            {
                upDir = strlen(GetPath()) > 7;
            }
            else // UNC path
            {
                const char* s2 = GetPath() + 2;
                while (*s2 != 0 && *s2 != '\\')
                    s2++;
                if (*s2 != 0)
                    s2++;
                while (*s2 != 0 && *s2 != '\\')
                    s2++;
                upDir = (*s2 == '\\' && *(s2 + 1) != 0);
                if (!upDir && Plugins.GetFirstNethoodPluginFSName())
                {
                    upDir = TRUE;
                    UNCRootUpDir = TRUE;
                }
            }
        }
        else
            upDir = strlen(GetPath()) > 3;

        CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::disk2");

        CIconData iconData;
        iconData.FSFileData = NULL;
        iconData.SetReadingDone(0); // just for the form
        BOOL addtoIconCache;
        CFileData file;
        // inicialization of structure members which will not be changed later
        file.PluginData = -1; // -1 just like that, ignored
        file.Selected = 0;
        file.SizeValid = 0;
        file.Dirty = 0; // unnecessary, just for the form
        file.CutToClip = 0;
        file.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
        file.IconOverlayDone = 0;
        int len;
#ifndef _WIN64
        int foundWin64RedirectedDirs = 0;
        BOOL isWin64RedirectedDir = FALSE;
#endif // _WIN64

    _TRY_AGAIN:

        // after 2000 ms we will show a window with a cancel prompt
        char buf[2 * MAX_PATH + 100];
        sprintf(buf, LoadStr(IDS_READINGPATHESC), GetPath());
        CreateSafeWaitWindow(buf, NULL, 2000, TRUE, MainWindow->HWindow);

        DWORD lastEscCheckTime;
        //lastEscCheckTime = GetTickCount() - 200;  // the first ESC will go immediately
        lastEscCheckTime = GetTickCount(); // the first ESC will go after 200 ms -- it's a protection
                                           // against the cancel prompt for listing after the user
                                           // has closed a dialog (e.g. Files/Security/*) by Esc and
                                           // in the panel there was a network drive (and during the
                                           // opened dialog the user switched to Salamander and back,
                                           // so that there was a refresh of the directory)

        BOOL isUpDir = FALSE;
        WIN32_FIND_DATA fileData;
        HANDLE search;
        search = HANDLES_Q(FindFirstFile(fileName, &fileData));
        if (search == INVALID_HANDLE_VALUE)
        {
            DWORD err = GetLastError();
            DestroySafeWaitWindow();
            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_NO_MORE_FILES)
            {
                if (!upDir)
                {
                    StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));
                    SetCurrentDirectoryToSystem();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    if (UseSystemIcons || UseThumbnails) // even though we don't have any icons, we need to start loading them (just to set IconCacheValid = TRUE)
                    {
                        if (IconCache->Count > 1)
                            IconCache->SortArray(0, IconCache->Count - 1, NULL);
                        WakeupIconCacheThread(); // start loading icons
                    }
                    //          TRACE_I("ReadDirectory: end");
                    return TRUE;
                }
            }
            else
            {
                SetCurrentDirectoryToSystem();
                RefreshListBox(0, -1, -1, FALSE, FALSE);
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                DirectoryLine->InvalidateIfNeeded();
                IdleRefreshStates = TRUE; // we will force checking of states of variables at the next Idle
                StatusLine->SetText("");
                UpdateWindow(HWindow);

                BOOL showErr = TRUE;
                if (err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_READY)
                {
                    DWORD attrs = SalGetFileAttributes(GetPath());
                    if (attrs != INVALID_FILE_ATTRIBUTES &&
                        (attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                        (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
                    {
                        showErr = FALSE;
                        char drive[MAX_PATH];
                        UINT drvType2;
                        if (GetPath()[0] == '\\' && GetPath()[1] == '\\')
                        {
                            drvType2 = DRIVE_REMOTE;
                            GetRootPath(drive, GetPath());
                            drive[strlen(drive) - 1] = 0; // we don't want the last '\\'
                        }
                        else
                        {
                            drive[0] = GetPath()[0];
                            drive[1] = 0;
                            drvType2 = MyGetDriveType(GetPath());
                        }
                        if (drvType2 != DRIVE_REMOTE)
                        {
                            GetCurrentLocalReparsePoint(GetPath(), CheckPathRootWithRetryMsgBox);
                            if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                            {
                                lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                                SalPathRemoveBackslash(drive);
                            }
                        }
                        else
                            GetRootPath(CheckPathRootWithRetryMsgBox, GetPath());
                        sprintf(buf, LoadStr(IDS_NODISKINDRIVE), drive);
                        int msgboxRes = (int)CDriveSelectErrDlg(parent, buf, GetPath()).Execute();
                        CheckPathRootWithRetryMsgBox[0] = 0;
                        UpdateWindow(MainWindow->HWindow);
                        if (msgboxRes == IDRETRY)
                            goto _TRY_AGAIN;
                    }
                }
                if (isRefresh &&
                    (err == ERROR_ACCESS_DENIED || err == ERROR_PATH_NOT_FOUND ||
                     err == ERROR_BAD_PATHNAME || err == ERROR_FILE_NOT_FOUND))
                { // when deleting a path shown in the panel, these errors are shown, which we don't want, we just silently shorten the path to the first existing one (unfortunately it's not caught earlier, because the path exists for some time after its deletion, something in Windows just didn't work out again)
                    //          TRACE_I("ReadDirectory(): silently ignoring FindFirstFile failure: " << GetErrorText(err));
                    showErr = FALSE;
                }
                if (showErr)
                    SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                //        TRACE_I("ReadDirectory: end");
                return FALSE;
            }
        }
        else
        {
            BOOL testFindNextErr;
            testFindNextErr = TRUE;
            do
            {
                NumberOfItemsInCurDir++;

                // test ESC - doesn't user want to interrupt reading?
                if (GetTickCount() - lastEscCheckTime >= 200) // 5 times per second
                {
                    if (UserWantsToCancelSafeWaitWindow())
                    {
                        MSG msg; // remove buffered ESC
                        while (PeekMessage(&msg, NULL, WM_KEYFIRST, WM_KEYLAST, PM_REMOVE))
                            ;

                        SetCurrentDirectoryToSystem();
                        RefreshListBox(0, -1, -1, FALSE, FALSE);

                        int resBut = SalMessageBox(parent, LoadStr(IDS_READDIRTERMINATED), LoadStr(IDS_QUESTION),
                                                   MB_YESNOCANCEL | MB_ICONQUESTION);
                        UpdateWindow(MainWindow->HWindow);

                        WaitForESCRelease();
                        WaitForESCReleaseBeforeTestingESC = FALSE; // another waiting makes no sense
                        GetAsyncKeyState(VK_ESCAPE);               // new init GetAsyncKeyState - see help

                        if (resBut == IDYES)
                        {
                            testFindNextErr = FALSE;
                            break; // finish reading
                        }
                        else
                        {
                            if (resBut == IDNO)
                            {
                                if (GetMonitorChanges()) // need to suppress monitoring of changes (autorefresh)
                                {
                                    DetachDirectory((CFilesWindow*)this);
                                    SetMonitorChanges(FALSE); // the changes won't be monitored anymore
                                }

                                SetSuppressAutoRefresh(TRUE);
                            }
                        }
                    }
                    lastEscCheckTime = GetTickCount();
                }

                st = fileData.cFileName;
                len = (int)strlen(st);
                BOOL isDir;
                isDir = (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
                isUpDir = (len == 2 && *st == '.' && *(st + 1) == '.');
                //--- handling of "." and ".." and hidden/system files (file "." is not ignored, FLAME spyware uses these files, so let them be visible)
                if (len == 0 || len == 1 && *st == '.' && isDir ||
                    ((isRootPath || !isDir ||
                      CQuadWord(fileData.ftLastWriteTime.dwLowDateTime, // date on ".." is older or equal to 1.1.1980, we better read it later "properly"
                                fileData.ftLastWriteTime.dwHighDateTime) <= CQuadWord(2148603904, 27846551)) &&
                     isUpDir))
                    continue;

                if (Configuration.NotHiddenSystemFiles &&
                    !IsFilePlaceholder(&fileData) && // placeholder is hidden, but Explorer shows it normally, so we will show it normally too
                    (fileData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)) &&
                    (len != 2 || *st != '.' || *(st + 1) != '.'))
                { // skip hidden/system file/directory
                    if (isDir)
                        HiddenDirsCount++;
                    else
                        HiddenFilesCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                    continue;
                }
                //--- applying filter to files
                if (FilterEnabled && !isDir)
                {
                    const char* ext = fileData.cFileName + len;
                    while (--ext >= fileData.cFileName && *ext != '.')
                        ;
                    if (ext < fileData.cFileName)
                        ext = fileData.cFileName + len; // ".cvspass" in Windows is an extension ...
                    else
                        ext++;
                    if (!Filter.AgreeMasks(fileData.cFileName, ext))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                        continue;
                    }
                }

                //--- if the name is occupied in the array HiddenNames, we will discard it
                if (HiddenNames.Contains(isDir, fileData.cFileName))
                {
                    if (isDir)
                        HiddenDirsCount++;
                    else
                        HiddenFilesCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                    continue;
                }

            ADD_ITEM: // to add ".."

                //--- name
                file.Name = (char*)malloc(len + 1); // allocation
                if (file.Name == NULL)
                {
                    if (search != NULL)
                    {
                        DestroySafeWaitWindow();
                        HANDLES(FindClose(search));
                    }
                    TRACE_E(LOW_MEMORY);
                    SetCurrentDirectoryToSystem();
                    Files->DestroyMembers();
                    Dirs->DestroyMembers();
                    VisibleItemsArray.InvalidateArr();
                    VisibleItemsArraySurround.InvalidateArr();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE;
                }
                memmove(file.Name, st, len + 1); // copy of text
                file.NameLen = len;
                //--- extension
                if (!Configuration.SortDirsByExt && (fileData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) // this is ptDisk
                {
                    file.Ext = file.Name + file.NameLen; // directories have no extension
                }
                else
                {
                    s = st + len;
                    while (--s >= st && *s != '.')
                        ;
                    if (s >= st)
                        file.Ext = file.Name + (s - st + 1); // ".cvspass" in Windows is an extension ...
                                                             //          if (s > st) file.Ext = file.Name + (s - st + 1);
                    else
                        file.Ext = file.Name + file.NameLen;
                }
                //--- others
                file.Size = CQuadWord(fileData.nFileSizeLow, fileData.nFileSizeHigh);
                file.Attr = fileData.dwFileAttributes;
                file.LastWrite = fileData.ftLastWriteTime;
                // placeholder is hidden, but Explorer shows it normally, so we will show it normally too (without ghosted icon)
                file.Hidden = (file.Attr & FILE_ATTRIBUTE_HIDDEN) && !IsFilePlaceholder(&fileData) ? 1 : 0;

                file.IsOffline = !isUpDir && (file.Attr & FILE_ATTRIBUTE_OFFLINE) ? 1 : 0;
                if (testShares && (file.Attr & FILE_ATTRIBUTE_DIRECTORY)) // this is ptDisk
                {
                    file.Shared = Shares.Search(file.Name);
                }
                else
                    file.Shared = 0;

                if (fileData.cAlternateFileName[0] != 0)
                {
                    int l = (int)strlen(fileData.cAlternateFileName) + 1;
                    file.DosName = (char*)malloc(l);
                    if (file.DosName == NULL)
                    {
                        free(file.Name);
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        TRACE_E(LOW_MEMORY);
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                    memmove(file.DosName, fileData.cAlternateFileName, l);
                }
                else
                    file.DosName = NULL;
                if (file.Attr & FILE_ATTRIBUTE_DIRECTORY) // this is ptDisk
                {
                    file.Association = 0;
                    file.Archive = 0;
#ifndef _WIN64
                    file.IsLink = (isWin64RedirectedDir || (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) || // CAUTION: pseudo-directory must have IsLink set, otherwise ContainsWin64RedirectedDir must be changed
                                   isWindows64BitDir && file.NameLen == 8 && StrICmp(file.Name, "system32") == 0)
                                      ? 1
                                      : 0; // system32 directory in 32-bit Salamander is link to SysWOW64 + win64 redirected-dir + volume mount point or junction point = show directory with link overlay
#else                                      // _WIN64
                    file.IsLink = (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // volume mount point or junction point = show directory with link overlay
#endif                                     // _WIN64
                    if (len == 2 && *st == '.' && *(st + 1) == '.')
                    { // handling ".."
                        if (GetPath()[3] != 0)
                            Dirs->Insert(0, file); // except of root...
                        else
                        {
                            if (file.Name != NULL)
                                free(file.Name);
                            if (file.DosName != NULL)
                                free(file.DosName);
                        }
                        addtoIconCache = FALSE;
                    }
                    else
                    {
                        Dirs->Add(file);
#ifndef _WIN64
                        addtoIconCache = isWin64RedirectedDir ? FALSE : TRUE;
#else  // _WIN64
                        addtoIconCache = TRUE;
#endif // _WIN64
                    }
                    if (!Dirs->IsGood())
                    {
                        Dirs->ResetState();
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }
                else
                {
                    if (s >= st) // an extension exists
                    {
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0;         // zeroes to the end
                        st = fileData.cFileName; // lowercase extension

                        if (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
                            file.IsLink = 1; // if the file is reparse-point (maybe it's not possible at all) = show it with link overlay
                        else
                        {
                            file.IsLink = (*(DWORD*)st == *(DWORD*)"lnk" ||
                                           *(DWORD*)st == *(DWORD*)"pif" ||
                                           *(DWORD*)st == *(DWORD*)"url")
                                              ? 1
                                              : 0;
                        }

                        if (PackerFormatConfig.PackIsArchive(file.Name, file.NameLen)) // is it an archive which we can process?
                        {
                            file.Association = 1;
                            file.Archive = 1;
                            addtoIconCache = FALSE;
                        }
                        else
                        {
                            file.Association = Associations.IsAssociated(st, addtoIconCache, iconSize);
                            file.Archive = 0;
                            if (*(DWORD*)st == *(DWORD*)"scr" || // few exceptions
                                *(DWORD*)st == *(DWORD*)"pif")
                            {
                                addtoIconCache = TRUE;
                            }
                            else
                            {
                                if (*(DWORD*)st == *(DWORD*)"lnk") // icons via link
                                {
                                    strcpy(fileData.cFileName, file.Name);
                                    char* ext2 = strrchr(fileData.cFileName, '.');
                                    if (ext2 != NULL) // ".cvspass" in Windows is an extesion
                                                      //                  if (ext2 != NULL && ext2 != fileData.cFileName)
                                    {
                                        *ext2 = 0;
                                        if (PackerFormatConfig.PackIsArchive(fileData.cFileName)) // is it a link to archive which we can process?
                                        {
                                            file.Association = 1;
                                            file.Archive = 1;
                                            addtoIconCache = FALSE;
                                        }
                                    }
                                    addtoIconCache = TRUE;
                                }
                            }
                        }
                    }
                    else
                    {
                        file.Association = 0;
                        file.Archive = 0;
                        file.IsLink = (fileData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) ? 1 : 0; // if the file is reparse-point (maybe it's not possible at all) = show it with link overlay
                        addtoIconCache = FALSE;
                    }

                    Files->Add(file);
                    if (!Files->IsGood())
                    {
                        Files->ResetState();
                        if (search != NULL)
                        {
                            DestroySafeWaitWindow();
                            HANDLES(FindClose(search));
                        }
                        SetCurrentDirectoryToSystem();
                        Files->DestroyMembers();
                        Dirs->DestroyMembers();
                        VisibleItemsArray.InvalidateArr();
                        VisibleItemsArraySurround.InvalidateArr();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }

                // at the file, we will check if it's necessary to load its thumbnail
                if (readThumbnails &&                              // thumbnail should be loaded
                    (file.Attr & FILE_ATTRIBUTE_DIRECTORY) == 0 && // (it is ptDisk, so using FILE_ATTRIBUTE_DIRECTORY is o.k.)
                    file.Archive == 0)                             // archive icon is preferred before thumbnail
                {
                    foundThumbLoaderPlugins.DestroyMembers();
                    int i;
                    for (i = 0; i < thumbLoaderPlugins.Count; i++)
                    {
                        CPluginData* p = thumbLoaderPlugins[i];
                        if (p->ThumbnailMasks.AgreeMasks(file.Name, file.Ext) &&
                            !p->ThumbnailMasksDisabled) // its unload/remove is not in progress
                        {
                            if (!p->GetLoaded()) // plugin needs to be loaded (possible change of mask for "thumbnail loader")
                            {
                                //                RefreshListBox(0, -1, -1, FALSE, FALSE); // replaced with ListBox->SetItemsCound + WM_USER_UPDATEPANEL, because it was blinking e.g. when adding the first *.doc file to a directory with images (Eroiica is loaded (for thumbnail *.doc))

                                // displaying "PictureView is not registered" dialog may occur -> in that case it's necessary
                                // to refresh listbox (otherwise we don't do any refresh, so that it doesn't blink with the panel)
                                // we protect listbox against errors caused by request for refresh (data is just being read from disk)
                                ListBox->SetItemsCount(0, 0, 0, TRUE); // TRUE - we will disable setting scrollbar
                                // If WM_USER_UPDATEPANEL is delivered, the panel will be redrawn and scrollbar will be set.
                                // Message loop can deliver it when message box (or dialog) is created.
                                // Otherwise the panel will behave as unchanged and the message will be removed from queue.
                                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);

                                BOOL cont = FALSE;
                                if (p->InitDLL(HWindow, FALSE, TRUE, FALSE) &&  // plugin loaded successfully
                                    p->ThumbnailMasks.GetMasksString()[0] != 0) // plugin is still "thumbnail loader"
                                {
                                    if (!p->ThumbnailMasks.AgreeMasks(file.Name, file.Ext) || // it can't do thumbnail for this file anymore
                                        p->ThumbnailMasksDisabled)                            // its unload/remove is in progress
                                    {
                                        cont = TRUE;
                                    }
                                }
                                else // can't load -> we will remove it from the list of probed plugins (prevent from repeating error messages)
                                {
                                    TRACE_I("Unable to use plugin " << p->Name << " as thumbnail loader.");
                                    thumbLoaderPlugins.Delete(i);
                                    readThumbnails = thumbLoaderPlugins.Count > 0;
                                    if (!thumbLoaderPlugins.IsGood())
                                        thumbLoaderPlugins.ResetState();
                                    else
                                        i--;
                                    cont = TRUE; // let's try our luck with another plugin
                                }

                                // cleanup message-queue from buffered WM_USER_UPDATEPANEL
                                MSG msg2;
                                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                                if (cont)
                                    continue;
                            }

                            foundThumbLoaderPlugins.Add(p);
                        }
                    }
                    if (foundThumbLoaderPlugins.IsGood())
                    {
                        if (foundThumbLoaderPlugins.Count > 0)
                        {
                            int size = len + 4;
                            size -= (size & 0x3); // size % 4 (alignment per four bytes)
                            int nameSize = size;
                            size += sizeof(CQuadWord) + sizeof(FILETIME);
                            size += (foundThumbLoaderPlugins.Count + 1) * sizeof(void*); // space for pointers to plugin interfaces + NULL at the end
                            iconData.NameAndData = (char*)malloc(size);
                            if (iconData.NameAndData != NULL)
                            {
                                memcpy(iconData.NameAndData, file.Name, len);
                                memset(iconData.NameAndData + len, 0, nameSize - len); // end of name is zeroed
                                // size is added + time of last write to file
                                *(CQuadWord*)(iconData.NameAndData + nameSize) = file.Size;
                                *(FILETIME*)(iconData.NameAndData + nameSize + sizeof(CQuadWord)) = file.LastWrite;
                                // add list of pointers to encapsulation of plugin interfaces for getting thumbnails
                                void** ifaces = (void**)(iconData.NameAndData + nameSize + sizeof(CQuadWord) + sizeof(FILETIME));
                                int i2;
                                for (i2 = 0; i2 < foundThumbLoaderPlugins.Count; i2++)
                                {
                                    *ifaces++ = foundThumbLoaderPlugins[i2]->GetPluginInterfaceForThumbLoader();
                                }
                                *ifaces = NULL;      // the end of list of plugin interfaces
                                iconData.SetFlag(4); // so far no unread thumbnail

                                // we have to allocate space for thumbnail, because it can't be done in the thread
                                iconData.SetIndex(IconCache->AllocThumbnail());

                                if (iconData.GetIndex() != -1)
                                {
                                    IconCache->Add(iconData);
                                    if (!IconCache->IsGood())
                                    {
                                        free(iconData.NameAndData);
                                        IconCache->ResetState();
                                    }
                                    else
                                        addtoIconCache = FALSE; // it's a thumbnail, it can't be an icon at the same time
                                }
                                else
                                    free(iconData.NameAndData);
                            }
                        }
                    }
                    else
                        foundThumbLoaderPlugins.ResetState();
                }

                // adding directory to IconCache -> we need to load icon
                if (UseSystemIcons && addtoIconCache)
                {
                    int size = len + 4;
                    size -= (size & 0x3); // size % 4 (alignment per four bytes)
                    iconData.NameAndData = (char*)malloc(size);
                    if (iconData.NameAndData != NULL)
                    {
                        memmove(iconData.NameAndData, file.Name, len);
                        memset(iconData.NameAndData + len, 0, size - len); // end of name is zeroed
                        iconData.SetFlag(0);                               // no not-loaded icon yet
                                                                           // need to allocate space for bitmaps, can't be done in thread
                        iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                        if (iconData.GetIndex() != -1)
                        {
                            IconCache->Add(iconData);
                            if (!IconCache->IsGood())
                            {
                                free(iconData.NameAndData);
                                IconCache->ResetState();
                            }
                        }
                        else
                            free(iconData.NameAndData);
                    }
                }
                if (search == NULL)
                {
                    testFindNextErr = FALSE;
#ifndef _WIN64
                    isWin64RedirectedDir = FALSE;
#endif                     // _WIN64
                    break; // the second pass (adding ".." or win64 redirected-dir)
                }
            } while (FindNextFile(search, &fileData));
            DWORD err = GetLastError();

            if (search != NULL) // the first pass
            {
                DestroySafeWaitWindow();
                HANDLES(FindClose(search));
            }

            if (testFindNextErr && err != ERROR_NO_MORE_FILES)
            {
                SetCurrentDirectoryToSystem();
                RefreshListBox(0, -1, -1, FALSE, FALSE);

                sprintf(buf, LoadStr(IDS_CANNOTREADDIR), GetPath(), GetErrorText(err));
                SalMessageBox(parent, buf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }

        if (upDir && (Dirs->Count == 0 || strcmp(Dirs->At(0).Name, "..") != 0))
        {
            upDir = FALSE;
            *(fileNameEnd - 1) = 0; // it's not logical, but times ".." are from current directory
            if (!UNCRootUpDir)
                search = HANDLES_Q(FindFirstFile(fileName, &fileData));
            else
                search = INVALID_HANDLE_VALUE;
            if (search == INVALID_HANDLE_VALUE)
            {
                fileData.dwFileAttributes = FILE_ATTRIBUTE_DIRECTORY; // this is ptDisk
                SYSTEMTIME ltNone;
                ltNone.wYear = 1602;
                ltNone.wMonth = 1;
                ltNone.wDay = 1;
                ltNone.wDayOfWeek = 2;
                ltNone.wHour = 0;
                ltNone.wMinute = 0;
                ltNone.wSecond = 0;
                ltNone.wMilliseconds = 0;
                FILETIME ft;
                SystemTimeToFileTime(&ltNone, &ft);
                LocalFileTimeToFileTime(&ft, &fileData.ftCreationTime);
                LocalFileTimeToFileTime(&ft, &fileData.ftLastAccessTime);
                LocalFileTimeToFileTime(&ft, &fileData.ftLastWriteTime);

                fileData.nFileSizeHigh = 0;
                fileData.nFileSizeLow = 0;
                fileData.dwReserved0 = fileData.dwReserved1 = 0;
            }
            else
                HANDLES(FindClose(search));
            search = NULL;                                              // the second/third pass
            fileData.dwFileAttributes |= FILE_ATTRIBUTE_DIRECTORY;      // this is ptDisk
            fileData.dwFileAttributes &= ~FILE_ATTRIBUTE_REPARSE_POINT; // need to remove flag FILE_ATTRIBUTE_REPARSE_POINT, otherwise link overlay will be on ".."
            strcpy(fileData.cFileName, "..");
            fileData.cAlternateFileName[0] = 0;
            st = fileData.cFileName;
            len = (int)strlen(st);
            isUpDir = TRUE;
            goto ADD_ITEM;
        }

#ifndef _WIN64

    FIND_NEXT_WIN64_REDIRECTEDDIR:

        BOOL dirWithSameNameExists;
        if (foundWin64RedirectedDirs < 10 &&
            AddWin64RedirectedDir(GetPath(), Dirs, &fileData, &foundWin64RedirectedDirs, &dirWithSameNameExists))
        {
            foundWin64RedirectedDirs++; // e.g. under system32 there can be 5, I've added some reserve to 10...

            if (Configuration.NotHiddenSystemFiles &&
                (fileData.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM)))
            { // skip hidden directory
                if (!dirWithSameNameExists)
                {
                    HiddenDirsCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                }
                goto FIND_NEXT_WIN64_REDIRECTEDDIR;
            }

            //--- if the name is occupied in the array HiddenNames, we will discard it
            if (HiddenNames.Contains(TRUE, fileData.cFileName))
            {
                if (!dirWithSameNameExists)
                {
                    HiddenDirsCount++;
                    HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                }
                goto FIND_NEXT_WIN64_REDIRECTEDDIR;
            }

            isWin64RedirectedDir = TRUE;
            search = NULL; // the second/third pass...
            st = fileData.cFileName;
            len = (int)strlen(st);
            isUpDir = FALSE;
            goto ADD_ITEM;
        }
        if (foundWin64RedirectedDirs >= 10)
            TRACE_E("CFilesWindow::ReadDirectory(): foundWin64RedirectedDirs >= 10 (there are more redirected-dirs?)");

#endif // _WIN64

        SetCurrentDirectoryToSystem();

        if (Files->Count + Dirs->Count == 0)
            StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));

        // sorting of Files and Dirs according to the current sorting method
        SortDirectory();

        if (UseSystemIcons || UseThumbnails)
        {
            if (IconCache->Count > 1)
                IconCache->SortArray(0, IconCache->Count - 1, NULL);
            WakeupIconCacheThread(); // start loading icons
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            RefreshDiskFreeSpace();

            if (PluginData.NotEmpty())
            {
                CSalamanderView view(this);
                PluginData.SetupView(this == MainWindow->LeftPanel, &view, GetZIPPath(),
                                     GetArchiveDir()->GetUpperDir(GetZIPPath()));
            }

            // setting of icon size for IconCache
            CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
            IconCache->SetIconSize(iconSize);

            CFilesArray* ZIPFiles = GetArchiveDirFiles();
            CFilesArray* ZIPDirs = GetArchiveDirDirs();

            Files->SetDeleteData(FALSE); // only a shallow copy of data
            Dirs->SetDeleteData(FALSE);  // only a shallow copy of data

            if (ZIPFiles != NULL && ZIPDirs != NULL)
            {
                // see comment in case of ptPluginFS
                Files->SetDelta(DeltaForTotalCount(ZIPFiles->Count));
                Dirs->SetDelta(DeltaForTotalCount(ZIPDirs->Count));

                int i;
                for (i = 0; i < ZIPFiles->Count; i++)
                {
                    CFileData* f = &ZIPFiles->At(i);
                    if (Configuration.NotHiddenSystemFiles &&
                        (f->Hidden || // both Hidden and Attr are nulled if they are invalid -> tests fail
                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                    { // skip hidden file/directory
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                        continue;
                    }

                    if (FilterEnabled && (!Filter.AgreeMasks(f->Name, f->Ext)))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                        continue;
                    }

                    //--- if the name is occupied in the array HiddenNames, we will discard it
                    if (HiddenNames.Contains(FALSE, f->Name))
                    {
                        HiddenFilesCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                        continue;
                    }

                    Files->Add(*f);
                }
                CFileData upDir;
                static char buffUp[] = "..";
                upDir.Name = buffUp; // free() won't be called, we can afford ".."
                upDir.Ext = upDir.Name + 2;
                upDir.Size = CQuadWord(0, 0);
                upDir.Attr = 0;
                upDir.LastWrite = GetZIPArchiveDate();
                upDir.DosName = NULL;
                upDir.PluginData = 0; // 0 just like that, plug-in will overwrite it with its value
                upDir.NameLen = 2;
                upDir.Hidden = 0;
                upDir.IsLink = 0;
                upDir.IsOffline = 0;
                upDir.Association = 0;
                upDir.Selected = 0;
                upDir.Shared = 0;
                upDir.Archive = 0;
                upDir.SizeValid = 0;
                upDir.Dirty = 0; // unnecessary, just for form
                upDir.CutToClip = 0;
                upDir.IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED;
                upDir.IconOverlayDone = 0;

                if (PluginData.NotEmpty())
                    PluginData.GetFileDataForUpDir(GetZIPPath(), upDir);

                Dirs->Add(upDir);
                for (i = 0; i < ZIPDirs->Count; i++)
                {
                    CFileData* f = &ZIPDirs->At(i);
                    if (Configuration.NotHiddenSystemFiles &&
                        (f->Hidden || // both Hidden and Attr are nulled if they are invalid -> tests fail
                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                    { // skip hidden file/directory
                        HiddenDirsCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                        continue;
                    }

                    //--- if the name is occupied in the array HiddenNames, we will discard it
                    if (HiddenNames.Contains(TRUE, f->Name))
                    {
                        HiddenDirsCount++;
                        HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                        continue;
                    }

                    Dirs->Add(*f);
                }
                if (!Files->IsGood() || !Dirs->IsGood())
                {
                    if (!Files->IsGood())
                        Files->ResetState();
                    if (!Dirs->IsGood())
                        Dirs->ResetState();
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE;
                }
            }
            else
            {
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                //        TRACE_I("ReadDirectory: end");
                return FALSE; // the directory has ceased to exist ...
            }

            // sorting of Files and Dirs according to the current sorting method
            SortDirectory();

            UseSystemIcons = !Configuration.UseSimpleIconsInArchives;
            if (UseSystemIcons)
            {
                // preparing for loading of icons
                IconCacheValid = FALSE;
                MSG msg; // need to remove any WM_USER_ICONREADING_END, which would set IconCacheValid = TRUE
                while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                    ;

                int i;
                for (i = 0; i < Associations.Count; i++)
                {
                    if (Associations[i].GetIndex(iconSize) == -3)
                        Associations[i].SetIndex(-1, iconSize);
                }

                // getting of necessary static icons (we won't unpack them)
                for (i = 0; i < Files->Count; i++)
                {
                    const char* iconLocation = NULL;
                    CFileData* f = &Files->At(i);

                    if (*f->Ext != 0) // an extension exists
                    {
                        /*
            if (PackerFormatConfig.PackIsArchive(f->Name))   // is it an archive which we can process?
            {
              f->Association = TRUE;
              f->Archive = TRUE;
            }
            else
            {
*/
                        char* s = f->Ext - 1;
                        char* st = fileName;
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0; // zeroes to the end
                        st = fileName;   // lowercase extension

                        f->Association = Associations.IsAssociatedStatic(st, iconLocation, iconSize);
                        f->Archive = FALSE;
                        /*
            }
*/
                    }
                    else
                    {
                        f->Association = FALSE;
                        f->Archive = FALSE;
                    }

                    if (iconLocation != NULL)
                    {
                        CIconData iconData;
                        iconData.FSFileData = NULL;
                        iconData.SetReadingDone(0); // just for form
                        int size = (int)strlen(iconLocation) + 4;
                        size -= (size & 0x3);                // size % 4  (alignment per four bytes)
                        const char* s = iconLocation + size; // skip alignment from zeros
                        int len = (int)strlen(s);
                        if (len > 0) // icon-location is not empty
                        {
                            int nameLen = f->NameLen + 4;
                            nameLen -= (nameLen & 0x3); // nameLen % 4  (alignment per four bytes)
                            iconData.NameAndData = (char*)malloc(nameLen + len + 1);
                            if (iconData.NameAndData != NULL)
                            {
                                memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // name +
                                memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zeroes alignment +
                                memcpy(iconData.NameAndData + nameLen, s, len + 1);                 // icon-location + '\0'

                                iconData.SetFlag(3); // not-loaded icon given by icon-location only
                                                     // we need to allocate space for bitmaps, can't be done in thread
                                iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                if (iconData.GetIndex() != -1)
                                {
                                    IconCache->Add(iconData);
                                    if (!IconCache->IsGood())
                                    {
                                        free(iconData.NameAndData);
                                        IconCache->ResetState();
                                    }
                                }
                                else
                                    free(iconData.NameAndData);
                            }
                        }
                    }
                }

                // waking up icon-reading
                if (IconCache->Count > 1)
                    IconCache->SortArray(0, IconCache->Count - 1, NULL);
                WakeupIconCacheThread(); // start to load icons
            }
            else
            {
                int i;
                for (i = 0; i < Files->Count; i++)
                {
                    CFileData* f = &Files->At(i);

                    if (*f->Ext != 0) // an extension exists
                    {
                        /*
            if (PackerFormatConfig.PackIsArchive(f->Name))   // is it an archive which we can process?
            {
              f->Association = TRUE;
              f->Archive = TRUE;
            }
            else
            {
*/
                        char* s = f->Ext - 1;
                        char buf[MAX_PATH];
                        char* st = buf;
                        while (*++s != 0)
                            *st++ = LowerCase[*s];
                        *(DWORD*)st = 0; // zeroes to the end
                        st = buf;        // lowercase extension

                        f->Association = Associations.IsAssociated(st);
                        f->Archive = FALSE;
                        /*
            }
*/
                    }
                    else
                    {
                        f->Association = FALSE;
                        f->Archive = FALSE;
                    }
                }
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                RefreshDiskFreeSpace();

                if (PluginData.NotEmpty())
                {
                    CSalamanderView view(this);
                    PluginData.SetupView(this == MainWindow->LeftPanel, &view, NULL, NULL);
                }

                // setting of icon size for IconCache
                CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
                IconCache->SetIconSize(iconSize);

                CFilesArray* FSFiles = GetFSFiles();
                CFilesArray* FSDirs = GetFSDirs();

                Files->SetDeleteData(FALSE); // only shallow copy of data
                Dirs->SetDeleteData(FALSE);  // only shallow copy of data

                if (FSFiles != NULL && FSDirs != NULL)
                {
                    // The Undelete plugin can show tens of thousands of files in one heap in the merged directory
                    // and the reallocation of CFilesArray after implicit 200 items then took several seconds.
                    // Because we know the number of items in advance, we can choose a better strategy for reallocations.
                    Files->SetDelta(DeltaForTotalCount(FSFiles->Count));
                    Dirs->SetDelta(DeltaForTotalCount(FSDirs->Count));

                    int i;
                    for (i = 0; i < FSFiles->Count; i++)
                    {
                        CFileData* f = &FSFiles->At(i);

                        if (Configuration.NotHiddenSystemFiles &&
                            (f->Hidden || // both Hidden and Attr are nulled if they are invalid -> tests fail
                             (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                        { // skip hidden file/directory
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                            continue;
                        }

                        if (FilterEnabled && !Filter.AgreeMasks(f->Name, f->Ext))
                        {
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_FILTER;
                            continue;
                        }

                        //--- if the name is occupied in the array HiddenNames, we will discard it
                        if (HiddenNames.Contains(FALSE, f->Name))
                        {
                            HiddenFilesCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                            continue;
                        }

                        Files->Add(*f);
                    }

                    for (i = 0; i < FSDirs->Count; i++)
                    {
                        CFileData* f = &FSDirs->At(i);
                        if (Configuration.NotHiddenSystemFiles &&
                            (f->Hidden || // both Hidden and Attr are nulled if they are invalid -> tests fail
                             (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                        { // skip hidden file/directory
                            HiddenDirsCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_ATTRIBUTE;
                            continue;
                        }

                        //--- if the name is occupied in the array HiddenNames, we will discard it
                        if (HiddenNames.Contains(TRUE, f->Name))
                        {
                            HiddenDirsCount++;
                            HiddenDirsFilesReason |= HIDDEN_REASON_HIDECMD;
                            continue;
                        }

                        Dirs->Add(*f);
                    }

                    if (!Files->IsGood() || !Dirs->IsGood())
                    {
                        if (!Files->IsGood())
                            Files->ResetState();
                        if (!Dirs->IsGood())
                            Dirs->ResetState();
                        DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                        //            TRACE_I("ReadDirectory: end");
                        return FALSE;
                    }
                }
                else
                {
                    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                    //          TRACE_I("ReadDirectory: end");
                    return FALSE; // the directory has ceased to exist ...
                }

                // if the panel is empty, we will set infoline to "No files found"
                if (Files->Count + Dirs->Count == 0)
                {
                    char buff[1000];
                    DWORD varPlacements[100];
                    int varPlacementsCount = 100;
                    if (PluginData.NotEmpty() &&
                        PluginData.GetInfoLineContent(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                                      NULL, FALSE, 0, 0, TRUE, CQuadWord(0, 0), buff,
                                                      varPlacements, varPlacementsCount))
                    {
                        if (StatusLine->SetText(buff))
                            StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
                    }
                    else
                        StatusLine->SetText(LoadStr(IDS_NOFILESFOUND));
                }

                // sorting of Files and Dirs according to the current sorting method
                SortDirectory();

                if (GetPluginIconsType() == pitFromRegistry)
                {
                    UseSystemIcons = TRUE; // for simplification of draw-item in panel

                    // preparing for loading of icons
                    IconCacheValid = FALSE;
                    MSG msg; // need to remove any WM_USER_ICONREADING_END, which would set IconCacheValid = TRUE
                    while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                        ;

                    int i;
                    for (i = 0; i < Associations.Count; i++)
                    {
                        if (Associations[i].GetIndex(iconSize) == -3)
                            Associations[i].SetIndex(-1, iconSize);
                    }

                    // getting of necessary static icons (we won't unpack anything)
                    for (i = 0; i < Files->Count; i++)
                    {
                        const char* iconLocation = NULL;
                        CFileData* f = &Files->At(i);

                        if (*f->Ext != 0) // an extension exists
                        {
                            /*
              if (PackerFormatConfig.PackIsArchive(f->Name))   // is it an archive which we can process?
              {
                f->Association = TRUE;
                f->Archive = TRUE;
              }
              else
              {
*/
                            char* s = f->Ext - 1;
                            char* st = fileName;
                            while (*++s != 0)
                                *st++ = LowerCase[*s];
                            *(DWORD*)st = 0; // zeroes to the end
                            st = fileName;   // lowercase extension

                            f->Association = Associations.IsAssociatedStatic(st, iconLocation, iconSize);
                            f->Archive = FALSE;
                            /*
              }
*/
                        }
                        else
                        {
                            f->Association = FALSE;
                            f->Archive = FALSE;
                        }

                        if (iconLocation != NULL)
                        {
                            CIconData iconData;
                            iconData.FSFileData = NULL;
                            iconData.SetReadingDone(0); // just for form
                            int size = (int)strlen(iconLocation) + 4;
                            size -= (size & 0x3);                // size % 4  (alignment per four bytes)
                            const char* s = iconLocation + size; // skip alignment from zeros
                            int len = (int)strlen(s);
                            if (len > 0) // icon-location is not empty
                            {
                                int nameLen = f->NameLen + 4;
                                nameLen -= (nameLen & 0x3); // nameLen % 4  (alignment per four bytes)
                                iconData.NameAndData = (char*)malloc(nameLen + len + 1);
                                if (iconData.NameAndData != NULL)
                                {
                                    memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // name +
                                    memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zeroes alignment +
                                    memcpy(iconData.NameAndData + nameLen, s, len + 1);                 // icon-location + '\0'

                                    iconData.SetFlag(3); // not-loaded icon given by icon-location only
                                                         // we need to allocate space for bitmaps, can't be done in thread
                                    iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                    if (iconData.GetIndex() != -1)
                                    {
                                        IconCache->Add(iconData);
                                        if (!IconCache->IsGood())
                                        {
                                            free(iconData.NameAndData);
                                            IconCache->ResetState();
                                        }
                                    }
                                    else
                                        free(iconData.NameAndData);
                                }
                            }
                        }
                    }

                    // waking up icon-reading
                    if (IconCache->Count > 1)
                        IconCache->SortArray(0, IconCache->Count - 1, NULL);
                    WakeupIconCacheThread(); // start to load icons
                }
                else
                {
                    if (GetPluginIconsType() == pitFromPlugin)
                    {
#ifdef _DEBUG // most likely no error, just it shouldn't happen theoretically
                        if (SimplePluginIcons != NULL)
                            TRACE_E("SimplePluginIcons is not NULL before GetSimplePluginIcons().");
#endif // _DEBUG
                        SimplePluginIcons = PluginData.GetSimplePluginIcons(iconSize);
                        if (SimplePluginIcons == NULL) // not a success -> degradation to pitSimple
                        {
                            SetPluginIconsType(pitSimple);
                        }
                    }

                    if (GetPluginIconsType() == pitSimple)
                    {
                        UseSystemIcons = FALSE; // for simplification of draw-item in panel

                        int i;
                        for (i = 0; i < Files->Count; i++)
                        {
                            CFileData* f = &Files->At(i);

                            if (*f->Ext != 0) // an extension exists
                            {
                                /*
                if (PackerFormatConfig.PackIsArchive(f->Name))   // is it an archive which we can process?
                {
                  f->Association = TRUE;
                  f->Archive = TRUE;
                }
                else
                {
  */
                                char* s = f->Ext - 1;
                                char buf[MAX_PATH];
                                char* st = buf;
                                while (*++s != 0)
                                    *st++ = LowerCase[*s];
                                *(DWORD*)st = 0; // zeroes to the end
                                st = buf;        // lowercase extension

                                f->Association = Associations.IsAssociated(st);
                                f->Archive = FALSE;
                                /*
                }
  */
                            }
                            else
                            {
                                f->Association = FALSE;
                                f->Archive = FALSE;
                            }
                        }
                    }
                    else
                    {
                        if (GetPluginIconsType() == pitFromPlugin)
                        {
                            UseSystemIcons = TRUE; // for simplification of draw-item in panel

                            // SimplePluginIcons is already set (code before GetPluginIconsType() == pitSimple)

                            // preparing for loading of icons
                            IconCacheValid = FALSE;
                            MSG msg; // need to remove any WM_USER_ICONREADING_END, which would set IconCacheValid = TRUE
                            while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
                                ;

                            // file/directories which don't have a simple icon will be addeed to icon-cache
                            {
                                CALL_STACK_MESSAGE1("CFilesWindow::ReadDirectory::FS-icons-from-plugin");

                                int count = FSFiles->Count + FSDirs->Count;
                                int debugCount = 0;
                                int i;
                                for (i = 0; i < count; i++)
                                {
                                    BOOL isDir = i < FSDirs->Count;
                                    CFileData* f = (isDir ? &FSDirs->At(i) : &FSFiles->At(i - FSDirs->Count));

                                    // need a pointer to CFileData for PluginFSDir, because moving occurs in Files and Dirs
                                    // e.g. when sorting (Files and Dirs are arrays of CFileData, not (CFileData *), that's why these problems exist)
                                    if (Configuration.NotHiddenSystemFiles &&
                                        (f->Hidden || //both Hidden a Attr jsou nulovane pokud jsou neplatne -> testy failnou
                                         (f->Attr & (FILE_ATTRIBUTE_HIDDEN | FILE_ATTRIBUTE_SYSTEM))))
                                    { // skip hidden file/directory
                                        continue;
                                    }
                                    if (!isDir)
                                    {
                                        if (FilterEnabled && !Filter.AgreeMasks(f->Name, f->Ext))
                                            continue;
                                    }
                                    //--- if the name is occupied in the array HiddenNames, we will discard it
                                    if (HiddenNames.Contains(isDir, f->Name))
                                        continue;
                                    debugCount++;

                                    if ((!isDir || i != 0 || strcmp(f->Name, "..") != 0) &&
                                        !PluginData.HasSimplePluginIcon(*f, isDir)) // add to icon-cache?
                                    {
                                        CIconData iconData;
                                        iconData.FSFileData = f;
                                        iconData.SetReadingDone(0); // just for form
                                        int nameLen = f->NameLen + 4;
                                        nameLen -= (nameLen & 0x3); // nameLen % 4  (alignment per four bytes)
                                        iconData.NameAndData = (char*)malloc(nameLen);
                                        if (iconData.NameAndData != NULL)
                                        {
                                            memcpy(iconData.NameAndData, f->Name, f->NameLen);                  // name +
                                            memset(iconData.NameAndData + f->NameLen, 0, nameLen - f->NameLen); // zeroes alignment

                                            iconData.SetFlag(0); // so far not loaded icon (plugin-icon)
                                                                 // need to allocate space for bitmaps, can't be done in thread
                                            iconData.SetIndex(IconCache->AllocIcon(NULL, NULL));
                                            if (iconData.GetIndex() != -1)
                                            {
                                                IconCache->Add(iconData);
                                                if (!IconCache->IsGood())
                                                {
                                                    free(iconData.NameAndData);
                                                    IconCache->ResetState();
                                                }
                                            }
                                            else
                                                free(iconData.NameAndData);
                                        }
                                    }
                                }
                                if (debugCount != Files->Count + Dirs->Count)
                                {
                                    TRACE_E("CFilesWindow::ReadDirectory(): unexpected situation: different count of filtered items "
                                            "for icon-reading ("
                                            << debugCount << ") and panel (" << Files->Count + Dirs->Count << ")!");
                                }
                            }

                            // waking up icon-reading
                            if (IconCache->Count > 1)
                                IconCache->SortArray(0, IconCache->Count - 1, &PluginData);
                            WakeupIconCacheThread(); // start to load icons
                        }
                        else
                            TRACE_E("Unexpected situation (1) in v CFilesWindow::ReadDirectory().");
                    }
                }
            }
            else
            {
                TRACE_E("Unexpected situation (2) in CFilesWindow::ReadDirectory().");
                DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
                // setting of icon size for IconCache
                CIconSizeEnum iconSize = GetIconSizeForCurrentViewMode();
                IconCache->SetIconSize(iconSize);
                //        TRACE_I("ReadDirectory: end");
                return FALSE;
            }
        }
    }
    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);

    // if the sizes of directories are stored for this path, we will assign them
    // to DirectorySizesHolder.Restore(this);

    //  TRACE_I("ReadDirectory: end");
    return TRUE;
}

// sorts array Dirs and Files independently on global variables
void SortFilesAndDirectories(CFilesArray* files, CFilesArray* dirs, CSortType sortType, BOOL reverseSort, BOOL sortDirsByName)
{
    CALL_STACK_MESSAGE1("SortDirectoryAux()");

    // CAUTION: must correspond to sort-code in RefreshDirectory, ChangeSortType and CompareDirectories !!!

    if (dirs->Count > 0)
    {
        BOOL hasRoot = (dirs->At(0).NameLen == 2 && dirs->At(0).Name[0] == '.' &&
                        dirs->At(0).Name[1] == '.'); // root directory
        int firstIndex = hasRoot ? 1 : 0;
        if (dirs->Count - firstIndex > 1) // if there's one item only, there's nothing to sort
        {
            switch (sortType)
            {
            case stName:
                SortNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stExtension:
                SortExtName(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stTime:
            {
                if (sortDirsByName)
                    SortNameExt(*dirs, firstIndex, dirs->Count - 1, FALSE);
                else
                    SortTimeNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            }
            case stSize:
                SortSizeNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            case stAttr:
                SortAttrNameExt(*dirs, firstIndex, dirs->Count - 1, reverseSort);
                break;
            }
        }
    }
    if (files->Count > 1)
    {
        switch (sortType)
        {
        case stName:
            SortNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stExtension:
            SortExtName(*files, 0, files->Count - 1, reverseSort);
            break;
        case stTime:
            SortTimeNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stSize:
            SortSizeNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        case stAttr:
            SortAttrNameExt(*files, 0, files->Count - 1, reverseSort);
            break;
        }
    }
}

void CFilesWindow::SortDirectory(CFilesArray* files, CFilesArray* dirs)
{
    CALL_STACK_MESSAGE1("CFilesWindow::SortDirectory()");

    if (files == NULL)
        files = Files;
    if (dirs == NULL)
        dirs = Dirs;
    SortFilesAndDirectories(files, dirs, SortType, ReverseSort, Configuration.SortDirsByName);

    // single-purpose monitors for changes of Configuration.SortUsesLocale and Configuration.SortDetectNumbers
    // variables for the method CFilesWindow::RefreshDirectory
    SortedWithRegSet = Configuration.SortUsesLocale;
    SortedWithDetectNum = Configuration.SortDetectNumbers;
    VisibleItemsArray.InvalidateArr();
    VisibleItemsArraySurround.InvalidateArr();
}

#ifndef _WIN64

BOOL IsWin64RedirectedDirAux(const char* subDir, const char* redirectedDir, const char* redirectedDirLastComp,
                             char* winDir, char* winDirEnd, char** lastSubDir, BOOL failIfDirWithSameNameExists)
{
    if (IsTheSamePath(subDir, redirectedDir))
    {
        strcpy(winDirEnd, redirectedDir);

        WIN32_FIND_DATA find;
        HANDLE h;
        if (failIfDirWithSameNameExists)
        {
            h = HANDLES_Q(FindFirstFile(winDir, &find));
            if (h != INVALID_HANDLE_VALUE)
            {
                HANDLES(FindClose(h));
                return FALSE; // this is not just a pseudo-directory, there is a directory with the same name, which means that e.g. context menu will work more or less normally
            }
        }

        strcat(winDirEnd, "\\*");
        h = HANDLES_Q(FindFirstFile(winDir, &find));
        if (h != INVALID_HANDLE_VALUE)
        {
            HANDLES(FindClose(h));
            if (lastSubDir != NULL)
            {
                strcpy(*lastSubDir + 1, redirectedDirLastComp != NULL ? redirectedDirLastComp : redirectedDir);
                *lastSubDir += strlen(*lastSubDir);
            }
            return TRUE;
        }
    }
    return FALSE;
}

BOOL IsWin64RedirectedDir(const char* path, char** lastSubDir, BOOL failIfDirWithSameNameExists)
{
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char winDir[MAX_PATH];
        strcpy(winDir, WindowsDirectory);
        int len = (int)strlen(winDir);
        if (len > 0 && winDir[len - 1] != '\\')
            strcpy(winDir + len++, "\\");
        if (StrNICmp(winDir, path, len) == 0)
        {
            const char* subDir = path + len;
            if (IsWin64RedirectedDirAux(subDir, "Sysnative", NULL, winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\catroot", "catroot", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\catroot2", "catroot2", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                Windows7AndLater && IsWin64RedirectedDirAux(subDir, "system32\\DriverStore", "DriverStore", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\drivers\\etc", "etc", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\LogFiles", "LogFiles", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists) ||
                IsWin64RedirectedDirAux(subDir, "system32\\spool", "spool", winDir, winDir + len, lastSubDir, failIfDirWithSameNameExists))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL ContainsWin64RedirectedDir(CFilesWindow* panel, int* indexes, int count, char* redirectedDir, BOOL onlyAdded)
{
    redirectedDir[0] = 0;
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char path[MAX_PATH];
        lstrcpyn(path, panel->GetPath(), MAX_PATH);
        char* pathEnd = path + strlen(path);
        int i;
        for (i = 0; i < count; i++)
        {
            if (indexes[i] >= 0 && indexes[i] < panel->Dirs->Count) // only subdirectories matter
            {
                CFileData* dir = &panel->Dirs->At(indexes[i]);
                if (dir->IsLink) // all pseudo-directories have IsLink set
                {
                    *pathEnd = 0;
                    if (SalPathAppend(path, dir->Name, MAX_PATH) &&
                        IsWin64RedirectedDir(path, NULL, onlyAdded))
                    {
                        lstrcpyn(redirectedDir, dir->Name, MAX_PATH);
                        return TRUE;
                    }
                }
            }
        }
    }
    return FALSE;
}

BOOL AddWin64RedirectedDirAux(const char* path, const char* subDir, const char* redirectedDirPrefix,
                              const char* redirectedDirLastComp, CFilesArray* dirs,
                              WIN32_FIND_DATA* fileData, BOOL* dirWithSameNameExists)
{
    if (IsTheSamePath(subDir, redirectedDirPrefix))
    {
        int deleteIndex = -1;
        int i;
        for (i = 0; i < dirs->Count; i++)
            if (StrICmp(dirs->At(i).Name, redirectedDirLastComp) == 0)
            {
                if (dirs->At(i).IsLink)
                    return FALSE; // this redirected-dir is already added
                deleteIndex = i;
                break;
            }

        char findPath[MAX_PATH];
        lstrcpyn(findPath, path, MAX_PATH);
        if (SalPathAppend(findPath, redirectedDirLastComp, MAX_PATH) &&
            SalPathAppend(findPath, "*", MAX_PATH))
        {
            HANDLE h;
            h = HANDLES_Q(FindFirstFile(findPath, fileData)); // find-data for redirected-dir can be obtained from the "." directory in the listing of redirected-dir
            if (h != INVALID_HANDLE_VALUE)
            {
                BOOL found = FALSE;
                do
                {
                    if (strcmp(fileData->cFileName, "..") == 0 &&
                        (fileData->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0) // "." directory
                    {
                        found = TRUE;
                        break;
                    }
                } while (FindNextFile(h, fileData));
                HANDLES(FindClose(h));
                if (found)
                {
                    if (deleteIndex != -1)
                        dirs->Delete(deleteIndex); // there's is a directory here, we will delete it, redirected-dir has priority (redirector ignores this directory)
                    lstrcpyn(fileData->cFileName, redirectedDirLastComp, MAX_PATH);
                    fileData->cAlternateFileName[0] = 0;

                    if (CutDirectory(findPath)) // find out if there's a directory with the same name as redirected-dir on the disk (it does not need to be in the 'dirs' array, e.g. because of the command "Hide Selected Names")
                    {
                        WIN32_FIND_DATA fd;
                        h = HANDLES_Q(FindFirstFile(findPath, &fd));
                        if (h != INVALID_HANDLE_VALUE)
                        {
                            HANDLES(FindClose(h));
                            if ((fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 &&
                                StrICmp(fd.cFileName, redirectedDirLastComp) == 0)
                            {
                                *dirWithSameNameExists = TRUE;
                            }
                        }
                    }

                    return TRUE;
                }
            }
        }
    }
    return FALSE;
}

BOOL AddWin64RedirectedDir(const char* path, CFilesArray* dirs, WIN32_FIND_DATA* fileData,
                           int* index, BOOL* dirWithSameNameExists)
{
    *dirWithSameNameExists = FALSE;
    if (Windows64Bit && WindowsDirectory[0] != 0)
    {
        char winDir[MAX_PATH];
        strcpy(winDir, WindowsDirectory);
        int len = (int)strlen(winDir);
        if (len > 0 && winDir[len - 1] != '\\')
            strcpy(winDir + len++, "\\");
        if ((int)strlen(path) + 1 == len)
            winDir[--len] = 0; // patch due to path == win-dir (discard ending backslash)
        if (StrNICmp(winDir, path, len) == 0)
        {
            const char* subDir = path + len;
            if (*index == 0 && AddWin64RedirectedDirAux(path, subDir, "", "Sysnative", dirs, fileData, dirWithSameNameExists) ||
                *index == 0 && AddWin64RedirectedDirAux(path, subDir, "system32\\drivers", "etc", dirs, fileData, dirWithSameNameExists) ||
                *index == 0 && AddWin64RedirectedDirAux(path, subDir, "system32", "catroot", dirs, fileData, dirWithSameNameExists) ||
                *index <= 1 && AddWin64RedirectedDirAux(path, subDir, "system32", "catroot2", dirs, fileData, dirWithSameNameExists) && (*index = 1) != 0 ||
                *index <= 2 && AddWin64RedirectedDirAux(path, subDir, "system32", "LogFiles", dirs, fileData, dirWithSameNameExists) && (*index = 2) != 0 ||
                *index <= 3 && AddWin64RedirectedDirAux(path, subDir, "system32", "spool", dirs, fileData, dirWithSameNameExists) && (*index = 3) != 0 ||
                Windows7AndLater && *index <= 4 && AddWin64RedirectedDirAux(path, subDir, "system32", "DriverStore", dirs, fileData, dirWithSameNameExists) && (*index = 4) != 0)
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

#endif // _WIN64

BOOL CFilesWindow::ChangeDir(const char* newDir, int suggestedTopIndex, const char* suggestedFocusName,
                             int mode, int* failReason, BOOL convertFSPathToInternal, BOOL showNewDirPathInErrBoxes)
{
    CALL_STACK_MESSAGE7("CFilesWindow::ChangeDir(%s, %d, %s, %d, , %d, %d)", newDir, suggestedTopIndex,
                        suggestedFocusName, mode, convertFSPathToInternal, showNewDirPathInErrBoxes);

    // backup the string (it could change during execution - e.g. Name from CFileData from panel)
    char backup[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup;
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    char absFSPath[MAX_PATH];
    char path[2 * MAX_PATH];
    char errBuf[3 * MAX_PATH + 100];
    GetGeneralPath(path, 2 * MAX_PATH, TRUE);
    BOOL sendDirectlyToPlugin = FALSE;
    CChangeDirDlg dlg(HWindow, path, MainWindow->GetActivePanel()->Is(ptPluginFS) ? &sendDirectlyToPlugin : NULL);

    // refresh DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    BOOL useStopRefresh = newDir == NULL;
    if (useStopRefresh)
        BeginStopRefresh(); // snooper will relax now

CHANGE_AGAIN:

    if (newDir != NULL || dlg.Execute() == IDOK)
    {
        if (newDir == NULL)
        {
            convertFSPathToInternal = TRUE; // after input from user, conversion to internal format is necessary
            // postprocessing will be done only for path, which is not to be sent directly to plugin
            if (!sendDirectlyToPlugin && !PostProcessPathFromUser(HWindow, path))
                goto CHANGE_AGAIN;
        }
        BOOL sendDirectlyToPluginLocal = sendDirectlyToPlugin;
        TopIndexMem.Clear(); // long jump

    CHANGE_AGAIN_NO_DLG:

        UpdateWindow(MainWindow->HWindow);
        if (newDir != NULL)
            lstrcpyn(path, newDir, 2 * MAX_PATH);
        else // focus and top-index setting won't be done for path from dialog
        {
            suggestedTopIndex = -1;
            suggestedFocusName = NULL;
        }

        char fsName[MAX_PATH];
        char* fsUserPart;
        if (!sendDirectlyToPluginLocal && IsPluginFSPath(path, fsName, (const char**)&fsUserPart))
        {
            if (strlen(fsUserPart) >= MAX_PATH) // plugins do not count with longer path
            {
                sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_TOOLONGPATH));
                SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                              MB_OK | MB_ICONEXCLAMATION);
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // snooper will be started again
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    return FALSE; // we're finished, it can't be repeated
                }
                goto CHANGE_AGAIN;
            }
            int index;
            int fsNameIndex;
            if (Plugins.IsPluginFS(fsName, index, fsNameIndex))
            {
                BOOL pluginFailure = FALSE;
                if (convertFSPathToInternal) // convert path to internal format
                {
                    CPluginData* plugin = Plugins.Get(index);
                    if (plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, TRUE)) // plugin does not have to be loaded, in which case we let it load
                        plugin->GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart);
                    else
                    {
                        pluginFailure = TRUE;
                        // TRACE_E("Unexpected situation in CFilesWindow::ChangeDir()");  // if the plugin is blocked by the user (if the registration key is missing)
                    }
                }

                // select FS for opening the path has this order: active FS, one of the detached FS, then new FS

                // CPluginData *p = Plugins.Get(index);
                // if (p != NULL) strcpy(fsName, );  // let the size of the letters be determined by the user, for conversion to the original size of the letters we would have to search the array p->FSNames
                int localFailReason;
                BOOL ret;
                BOOL done = FALSE;

                // if the path cannot be listed in the FS interface in the panel, we will try to find a detached FS interface,
                // which would be able to list the path (so that a new FS is not opened unnecessarily)
                int fsNameIndexDummy;
                BOOL convertPathToInternalDummy = FALSE;
                if (!Is(ptPluginFS) || // FS interface in the panel cannot list the path (ChangePathToPluginFS opens a new FS)
                    !IsPathFromActiveFS(fsName, fsUserPart, fsNameIndexDummy, convertPathToInternalDummy))
                {
                    CDetachedFSList* list = MainWindow->DetachedFSList;
                    int i;
                    for (i = 0; i < list->Count; i++)
                    {
                        if (list->At(i)->IsPathFromThisFS(fsName, fsUserPart))
                        {
                            done = TRUE;
                            // trying to change to needed path, at the same time we will connect the detached FS
                            ret = ChangePathToDetachedFS(i, suggestedTopIndex, suggestedFocusName, TRUE,
                                                         &localFailReason, fsName, fsUserPart, mode, mode == 3);

                            break;
                        }
                    }
                }

                if (!pluginFailure && !done)
                {
                    ret = ChangePathToPluginFS(fsName, fsUserPart, suggestedTopIndex, suggestedFocusName,
                                               FALSE, mode, NULL, TRUE, &localFailReason, FALSE, mode == 3);
                }
                else
                {
                    if (!done) // only if 'ret' and 'localFailReason' from ChangePathToDetachedFS() are not set yet
                    {
                        ret = FALSE;
                        localFailReason = CHPPFR_INVALIDPATH;
                    }
                }

                if (!ret && newDir == NULL && localFailReason == CHPPFR_INVALIDPATH)
                { // path error (can be caused by the path itself or by the plugin - it cannot be loaded - this is very unlikely)
                    // convert path to external format
                    CPluginData* plugin = Plugins.Get(index);
                    if (!pluginFailure && plugin != NULL && plugin->InitDLL(MainWindow->HWindow, FALSE, TRUE, FALSE)) // plugin does not have to be loaded, in which case we let it load
                        plugin->GetPluginInterfaceForFS()->ConvertPathToExternal(fsName, fsNameIndex, fsUserPart);
                    // else TRACE_E("Unexpected situation (2) in CFilesWindow::ChangeDir()");  // if the plugin is blocked by the user (if the registration key is missing)

                    goto CHANGE_AGAIN; // returning back to the dialog for entering the path
                }
                else
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // snooper will be started again
                    if (failReason != NULL)
                        *failReason = localFailReason;
                    return ret;
                }
            }
            else
            {
                sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_NOTPLUGINFS));
                SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                              MB_OK | MB_ICONEXCLAMATION);
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // snooper will be started again
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    return FALSE; // finished, cannot be repeated
                }
                goto CHANGE_AGAIN;
            }
        }
        else
        {
            if (!sendDirectlyToPluginLocal &&
                (path[0] != 0 && path[1] == ':' ||                                             // X: type paths
                 (path[0] == '/' || path[0] == '\\') && (path[1] == '/' || path[1] == '\\') || // UNC paths
                 Is(ptDisk) || Is(ptZIPArchive)))                                              // disk+archive of the relative path
            {                                                                                  // this is a disk path (absolute or relative) - turn all '/' to '\\' + remove duplicated '\\'
                SlashesToBackslashesAndRemoveDups(path);
            }

            int errTextID;
            const char* text = NULL;                 // caution: textFailReason must be set
            int textFailReason = CHPPFR_INVALIDPATH; // if text != NULL, it contains the code of error which occurred
            char curPath[2 * MAX_PATH];
            curPath[0] = 0;
            if (sendDirectlyToPluginLocal)
                errTextID = IDS_INCOMLETEFILENAME;
            else
            {
                //        MainWindow->GetActivePanel()->GetGeneralPath(curPath, 2 * MAX_PATH);
                //        if (!SalGetFullName(path, &errTextID, (MainWindow->GetActivePanel()->Is(ptDisk) ||
                //                            MainWindow->GetActivePanel()->Is(ptZIPArchive)) ? curPath : NULL))
                if (Is(ptDisk) || Is(ptZIPArchive))
                    GetGeneralPath(curPath, 2 * MAX_PATH); // because of FTP plugin - relative path in "target panel path" when connecting
            }
            BOOL callNethood = FALSE;
            if (sendDirectlyToPluginLocal ||
                !SalGetFullName(path, &errTextID, (Is(ptDisk) || Is(ptZIPArchive)) ? curPath : NULL, NULL,
                                &callNethood, 2 * MAX_PATH))
            {
                sendDirectlyToPluginLocal = FALSE;
                if ((errTextID == IDS_SERVERNAMEMISSING || errTextID == IDS_SHARENAMEMISSING) && callNethood)
                { // incomplete UNC path will be given to the first plugin which supports FS and called SalamanderGeneral->SetPluginIsNethood()
                    if (Plugins.GetFirstNethoodPluginFSName(absFSPath))
                    {
                        if (strlen(absFSPath) + 1 < MAX_PATH)
                            strcat(absFSPath, ":");
                        if (strlen(absFSPath) + strlen(path) < MAX_PATH)
                            strcat(absFSPath, path);
                        if (newDir != NULL)
                            newDir = absFSPath; // in case that the path is entered from outside
                        else
                            strcpy(path, absFSPath);
                        goto CHANGE_AGAIN_NO_DLG; // try to open the incomplete UNC path in the plugin
                    }
                }
                if (errTextID == IDS_EMPTYNAMENOTALLOWED)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // snooper will be started again
                    if (failReason != NULL)
                        *failReason = CHPPFR_SUCCESS;
                    return FALSE; // empty string, nothing to do
                }
                if (errTextID == IDS_INCOMLETEFILENAME) // relative path on FS
                {
                    if (MainWindow->GetActivePanel()->Is(ptPluginFS) &&
                        MainWindow->GetActivePanel()->GetPluginFS()->NotEmpty())
                    {
                        BOOL success = TRUE;
                        if ((int)strlen(path) < MAX_PATH)
                            strcpy(absFSPath, path);
                        else
                        {
                            sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), path, LoadStr(IDS_TOOLONGPATH));
                            SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                          MB_OK | MB_ICONEXCLAMATION);
                            success = FALSE;
                        }
                        if (!success ||
                            MainWindow->GetActivePanel()->GetPluginFS()->GetFullFSPath(HWindow,
                                                                                       MainWindow->GetActivePanel()->GetPluginFS()->GetPluginFSName(),
                                                                                       absFSPath, MAX_PATH, success))
                        {
                            if (success) // we have the absolute path
                            {
                                if (newDir != NULL)
                                    newDir = absFSPath; // in case that the path is entered from outside
                                else
                                    strcpy(path, absFSPath);
                                convertFSPathToInternal = FALSE; // the path is already in internal format, so we must not convert it to internal format again
                                goto CHANGE_AGAIN_NO_DLG;        // let's try the entered absolute path
                            }
                            else // the error has been displayed to the user
                            {
                                if (newDir != NULL)
                                {
                                    if (useStopRefresh)
                                        EndStopRefresh(); // snooper will be started again
                                    if (failReason != NULL)
                                        *failReason = CHPPFR_INVALIDPATH;
                                    return FALSE; // finished, cannot be repeated
                                }
                                goto CHANGE_AGAIN; // show the path in dialog again, so that the user can correct it
                            }
                        }
                    }
                }
                text = LoadStr(errTextID);
                textFailReason = CHPPFR_INVALIDPATH;
            }
            BOOL showErr = TRUE;
            if (text == NULL)
            {
                if (*path != 0 && path[1] == ':')
                    path[0] = UpperCase[path[0]]; // "c:" path will be "C:"
                char copy[2 * MAX_PATH + 10];
                int len = GetRootPath(copy, path);

                if (!CheckAndRestorePath(copy))
                {
                    if (newDir != NULL)
                    {
                        if (useStopRefresh)
                            EndStopRefresh(); // snooper will be started again
                        if (failReason != NULL)
                            *failReason = CHPPFR_INVALIDPATH;
                        return FALSE; // finished, cannot be repeated
                    }
                    goto CHANGE_AGAIN;
                }

                if (len < (int)strlen(path)) // not a root path
                {
                    char* s = path + len - 1;     // where to copy from (points to '\\')
                    char* end = s;                // the end of the copied part
                    char* st = copy + (s - path); // where to copy to
                    while (*end != 0)
                    {
                        while (*++end != 0 && *end != '\\')
                            ;
                        memcpy(st, s, end - s);
                        st[end - s] = 0;
                        s = end;

                    _TRY_AGAIN:

                        BOOL pathEndsWithSpaceOrDot;
                        DWORD copyAttr;
                        int copyLen;
                        copyLen = (int)strlen(copy);
                        if (copyLen >= MAX_PATH)
                        {
                            if (*end != 0 && !SalPathAppend(copy, end + 1, 2 * MAX_PATH)) // if the so far processed part of the path is enlengthened and the rest of the path does not fit, we will use the original form of the path
                                strcpy(copy, path);
                            text = LoadStr(IDS_TOOLONGPATH);
                            textFailReason = CHPPFR_INVALIDPATH;
                            break;
                        }
                        if (copyLen > 0 && (copy[copyLen - 1] <= ' ' || copy[copyLen - 1] == '.'))
                        {
                            copyAttr = SalGetFileAttributes(copy);
                            pathEndsWithSpaceOrDot = copyAttr != INVALID_FILE_ATTRIBUTES;
                        }
                        else
                        {
                            pathEndsWithSpaceOrDot = FALSE;
                        }

                        WIN32_FIND_DATA find;
                        HANDLE h;
                        if (!pathEndsWithSpaceOrDot)
                            h = HANDLES_Q(FindFirstFile(copy, &find));
                        else
                            h = INVALID_HANDLE_VALUE;
                        DWORD err;
                        if (h == INVALID_HANDLE_VALUE && !pathEndsWithSpaceOrDot)
                        {
                            err = GetLastError();
                            if (err != ERROR_FILE_NOT_FOUND && err != ERROR_PATH_NOT_FOUND &&
                                err != ERROR_BAD_PATHNAME && err != ERROR_INVALID_NAME)
                            { // if there's chance that the path contains a directory to which we don't have access (we will try if there are other components of the path accessible)
                                DWORD firstErr = err;
                                char* firstCopyEnd = st + strlen(st);
                                while (*end != 0)
                                {
                                    st += strlen(st); // the inaccessible component of the path will be only copied (it can remain as dos-name or with different letter size)
                                    while (*++end != 0 && *end != '\\')
                                        ;
                                    memcpy(st, s, end - s);
                                    st[end - s] = 0;
                                    s = end;
                                    if ((int)strlen(copy) >= MAX_PATH) // too long path, we're finished...
                                    {
                                        h = INVALID_HANDLE_VALUE;
                                        break;
                                    }
                                    else
                                    {
                                        h = HANDLES_Q(FindFirstFile(copy, &find));
                                        if (h != INVALID_HANDLE_VALUE)
                                            break; // we've found an accessible component, continuing...
                                        err = GetLastError();
                                        if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
                                            err == ERROR_BAD_PATHNAME || err == ERROR_INVALID_NAME)
                                            break; // an error in the path, we're finished...
                                    }
                                }
                                if (*end == 0 && h == INVALID_HANDLE_VALUE) // another accessible component not found, we will try if the current path can be listed
                                {
                                    if ((int)strlen(copy) < MAX_PATH && SalPathAppend(copy, "*.*", MAX_PATH + 10))
                                    {
                                        h = HANDLES_Q(FindFirstFile(copy, &find));
                                        CutDirectory(copy);
                                        if (h != INVALID_HANDLE_VALUE) // the path can be listed
                                        {
                                            HANDLES(FindClose(h));

                                            // changing the path to absolute windows path
                                            BOOL ret = ChangePathToDisk(HWindow, copy, suggestedTopIndex, suggestedFocusName,
                                                                        NULL, TRUE, FALSE, FALSE, failReason);
                                            if (useStopRefresh)
                                                EndStopRefresh(); // snooper will be started again
                                            return ret;
                                        }
                                    }
                                }
                                if (h == INVALID_HANDLE_VALUE) // accessible part of the path not found, we will report the first found error
                                {
                                    err = firstErr;
                                    *firstCopyEnd = 0;

                                    // not used, but we'd better zero them, so that it immediately crashes in case of a new code and it's figured out...
                                    st = NULL;
                                    end = NULL;
                                    s = NULL;
                                }
                            }
#ifndef _WIN64
                            else
                            {
                                if (IsWin64RedirectedDir(copy, &st, FALSE))
                                    continue;
                            }
#endif // _WIN64
                        }

                        if (h != INVALID_HANDLE_VALUE || pathEndsWithSpaceOrDot)
                        {
                            if (h != INVALID_HANDLE_VALUE)
                            {
                                HANDLES(FindClose(h));
                                int len2 = (int)strlen(find.cFileName); // must fit (only the size of letters is changed - result of FindFirstFile)
                                if ((int)strlen(st + 1) != len2)        // it does e.g. for "aaa  " returns "aaa", reproduce: Paste (text without quotes): "   "   %TEMP%\aaa   "   "
                                {
                                    TRACE_E("CFilesWindow::ChangeDir(): unexpected situation: FindFirstFile returned name with "
                                            "different length: \""
                                            << find.cFileName << "\" for \"" << (st + 1) << "\"");
                                }
                                memcpy(st + 1, find.cFileName, len2);
                                st += 1 + len2;
                                *st = 0;
                            }
                            else
                                st += strlen(st);

                            // copy containts the "translated" path
                            if (!pathEndsWithSpaceOrDot)
                                copyAttr = find.dwFileAttributes;
                            if ((copyAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                            { // file -> is it an archive?
                                if (PackerFormatConfig.PackIsArchive(copy))
                                {
                                    if ((int)strlen(*end != 0 ? end + 1 : end) >= MAX_PATH) // too long path in archive
                                    {
                                        if (!SalPathAppend(copy, end + 1, 2 * MAX_PATH)) // if the archive name is enlengthened and the rest of the path does not fit, we will use the original form of the path
                                            strcpy(copy, path);
                                        text = LoadStr(IDS_TOOLONGPATH);
                                        textFailReason = CHPPFR_INVALIDPATH;
                                        break;
                                    }
                                    else
                                    {
                                        if (*end != 0)
                                            end++;
                                        // changing the path to absolute path to archive
                                        int localFailReason;
                                        BOOL ret = ChangePathToArchive(copy, end, suggestedTopIndex, suggestedFocusName,
                                                                       FALSE, NULL, TRUE, &localFailReason, FALSE, TRUE);
                                        if (!ret && localFailReason == CHPPFR_SHORTERPATH)
                                        {
                                            sprintf(errBuf, LoadStr(IDS_PATHINARCHIVENOTFOUND), end);
                                            SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                                          MB_OK | MB_ICONEXCLAMATION);
                                        }
                                        if (failReason != NULL)
                                            *failReason = localFailReason;
                                        if (useStopRefresh)
                                            EndStopRefresh(); // snooper will be started again
                                        return ret;
                                    }
                                }
                                else
                                {
                                    char* name;
                                    char shortenedPath[MAX_PATH];
                                    strcpy(shortenedPath, copy);
                                    if (*end == 0 && CutDirectory(shortenedPath, &name)) // when the path does not end with '\\' (path to file)
                                    {
                                        // change of the path to absolute windows path + focus to the file
                                        ChangePathToDisk(HWindow, shortenedPath, -1, name, NULL, TRUE, FALSE, FALSE, failReason);
                                        if (useStopRefresh)
                                            EndStopRefresh(); // snooper will be started again
                                        if (failReason != NULL && *failReason == CHPPFR_SUCCESS)
                                            *failReason = CHPPFR_FILENAMEFOCUSED;
                                        return FALSE; // listing another path (file name is cut)
                                    }
                                    else
                                    {
                                        text = LoadStr(IDS_NOTARCHIVEPATH);
                                        textFailReason = CHPPFR_INVALIDARCHIVE;
                                        break;
                                    }
                                }
                            }
                        }
                        else
                        {
                            if (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND ||
                                err == ERROR_BAD_PATHNAME)
                            {
                                text = LoadStr(IDS_PATHNOTFOUND);
                            }
                            else
                            {
                                if (err == ERROR_INVALID_PARAMETER || err == ERROR_NOT_READY)
                                {
                                    char drive[MAX_PATH];
                                    lstrcpyn(drive, copy, MAX_PATH);
                                    if (CutDirectory(drive))
                                    {
                                        DWORD attrs = SalGetFileAttributes(drive);
                                        if (attrs != INVALID_FILE_ATTRIBUTES &&
                                            (attrs & FILE_ATTRIBUTE_DIRECTORY) &&
                                            (attrs & FILE_ATTRIBUTE_REPARSE_POINT))
                                        {
                                            UINT drvType;
                                            if (copy[0] == '\\' && copy[1] == '\\')
                                            {
                                                drvType = DRIVE_REMOTE;
                                                GetRootPath(drive, copy);
                                                drive[strlen(drive) - 1] = 0; // we don't want the last '\\'
                                            }
                                            else
                                            {
                                                drive[0] = copy[0];
                                                drive[1] = 0;
                                                drvType = MyGetDriveType(copy);
                                            }
                                            if (drvType != DRIVE_REMOTE)
                                            {
                                                GetCurrentLocalReparsePoint(copy, CheckPathRootWithRetryMsgBox);
                                                if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                                                {
                                                    lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                                                    SalPathRemoveBackslash(drive);
                                                }
                                            }
                                            else
                                                GetRootPath(CheckPathRootWithRetryMsgBox, copy);
                                            sprintf(errBuf, LoadStr(IDS_NODISKINDRIVE), drive);
                                            int msgboxRes = (int)CDriveSelectErrDlg(HWindow, errBuf, copy).Execute();
                                            CheckPathRootWithRetryMsgBox[0] = 0;
                                            UpdateWindow(MainWindow->HWindow);
                                            if (msgboxRes == IDRETRY)
                                                goto _TRY_AGAIN;
                                            showErr = FALSE;
                                        }
                                    }
                                }
                                text = GetErrorText(err);
                            }
                            textFailReason = CHPPFR_INVALIDPATH;
                            break;
                        }
                    }
                    strcpy(path, copy); // taking a new path
                }
            }

            if (text != NULL)
            {
                if (showErr)
                {
                    sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT), showNewDirPathInErrBoxes && newDir != NULL ? newDir : path, text);
                    SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                }
                if (newDir != NULL)
                {
                    if (useStopRefresh)
                        EndStopRefresh(); // snopper will be started again
                    if (failReason != NULL)
                        *failReason = textFailReason;
                    return FALSE; // finished, cannot be repeated
                }
                goto CHANGE_AGAIN;
            }
            else
            {
                // changing the path to absolute windows path
                BOOL ret = ChangePathToDisk(HWindow, path, suggestedTopIndex, suggestedFocusName,
                                            NULL, TRUE, FALSE, FALSE, failReason);
                if (useStopRefresh)
                    EndStopRefresh(); // snooper will be started again
                return ret;
            }
        }
    }
    UpdateWindow(MainWindow->HWindow);
    if (useStopRefresh)
        EndStopRefresh(); // snooper will be started again
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

BOOL CFilesWindow::ChangeDirLite(const char* newDir)
{
    int failReason;
    BOOL ret = ChangeDir(newDir, -1, NULL, 3, &failReason, TRUE);
    return ret || failReason == CHPPFR_SHORTERPATH || failReason == CHPPFR_FILENAMEFOCUSED ||
           failReason == CHPPFR_SUCCESS /* non-sense, but we are not bothered by it */;
}

BOOL CFilesWindow::ChangePathToDrvType(HWND parent, int driveType, const char* displayName)
{
    char path[MAX_PATH];
    const char* userFolderOneDrive = NULL;
    if (driveType == drvtOneDrive || driveType == drvtOneDriveBus)
    {
        InitOneDrivePath();
        if (driveType == drvtOneDrive && OneDrivePath[0] == 0 ||
            driveType == drvtOneDriveBus && !OneDriveBusinessStorages.Find(displayName, &userFolderOneDrive))
        { // OneDrive Personal/Business has been disconnected, we will refresh Drive bars, so that its icon disappears or is updated
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
            return FALSE;
        }
    }
    if (driveType == drvtMyDocuments && GetMyDocumentsOrDesktopPath(path, MAX_PATH) ||
        driveType == drvtGoogleDrive && ShellIconOverlays.GetPathForGoogleDrive(path, MAX_PATH) ||
        driveType == drvtDropbox && strcpy_s(path, DropboxPath) == 0 ||
        driveType == drvtOneDrive && strcpy_s(path, OneDrivePath) == 0 ||
        driveType == drvtOneDriveBus && (int)strlen(userFolderOneDrive) < MAX_PATH && strcpy_s(path, userFolderOneDrive) == 0)
    {
        return ChangePathToDisk(parent, path);
    }
    return FALSE;
}

void CFilesWindow::ChangeDrive(char drive)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ChangeDrive(%u)", drive);
    //--- DefaultDire refresh
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() != this);
    //---  possible disk selection from the dialog
    CFilesWindow* anotherPanel = (Parent->LeftPanel == this ? Parent->RightPanel : Parent->LeftPanel);
    if (drive == 0)
    {
        CDriveTypeEnum driveType = drvtUnknow; // dummy
        DWORD_PTR driveTypeParam = (Is(ptDisk) || Is(ptZIPArchive)) ? GetPath()[0] : 0;
        int postCmd;
        void* postCmdParam;
        BOOL fromContextMenu;
        if (CDrivesList(this, ((Is(ptDisk) || Is(ptZIPArchive)) ? GetPath() : ""),
                        &driveType, &driveTypeParam, &postCmd, &postCmdParam,
                        &fromContextMenu)
                    .Track() == FALSE ||
            fromContextMenu && postCmd == 0) // only close-menu executed from context menu
        {
            return;
        }

        if (!fromContextMenu && driveType != drvtPluginFS && driveType != drvtPluginCmd)
            TopIndexMem.Clear(); // long jump

        UpdateWindow(MainWindow->HWindow);

        char path[MAX_PATH];
        switch (driveType)
        {
        case drvtMyDocuments:
        case drvtGoogleDrive:
        case drvtDropbox:
        case drvtOneDrive:
        case drvtOneDriveBus:
        {
            ChangePathToDrvType(HWindow, driveType, driveType == drvtOneDriveBus ? (const char*)driveTypeParam : NULL);
            if (driveType == drvtOneDriveBus)
                free((char*)driveTypeParam);
            return;
        }

        case drvtOneDriveMenu:
            TRACE_E("CFilesWindow::ChangeDrive(): unexpected drive type: drvtOneDriveMenu");
            break;

        // is it Network?
        case drvtNeighborhood:
        {
            if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_CHANGEDRIVE),
                                   LoadStr(IDS_CHANGEDRIVETEXT), path, TRUE))
            {
                UpdateWindow(MainWindow->HWindow);
                ChangePathToDisk(HWindow, path);
                return;
            }
            else
                return;
        }

        // is it other Panel?
        case drvtOtherPanel:
        {
            ChangePathToOtherPanelPath();
            return;
        }

        // is it Hot Path?
        case drvtHotPath:
        {
            GotoHotPath((int)driveTypeParam);
            return;
        }

        // it's a normal path
        case drvtUnknow:
        case drvtRemovable:
        case drvtFixed:
        case drvtRemote:
        case drvtCDROM:
        case drvtRAMDisk:
        {
            drive = (char)LOWORD(driveTypeParam);

            UpdateWindow(MainWindow->HWindow);
            break;
        }

        case drvtPluginFS:
        {
            CPluginFSInterfaceAbstract* fs = (CPluginFSInterfaceAbstract*)driveTypeParam;
            CPluginInterfaceForFSEncapsulation* ifaceForFS = NULL;
            // need to verify that 'fs' is still a valid interface
            if (Is(ptPluginFS) && GetPluginFS()->GetInterface() == fs)
            { // select active FS - perform refresh
                if (!fromContextMenu)
                    RefreshDirectory();
                else
                    ifaceForFS = GetPluginFS()->GetPluginInterfaceForFS();
            }
            else
            {
                CDetachedFSList* list = MainWindow->DetachedFSList;
                int i;
                for (i = 0; i < list->Count; i++)
                {
                    if (list->At(i)->GetInterface() == fs)
                    { // select detached FS
                        if (!fromContextMenu)
                            ChangePathToDetachedFS(i);
                        else
                            ifaceForFS = list->At(i)->GetPluginInterfaceForFS();
                        break;
                    }
                }
            }

            if (ifaceForFS != NULL) // post-cmd from context menu of active/detached FS
            {
                ifaceForFS->ExecuteChangeDrivePostCommand(PANEL_SOURCE, postCmd, postCmdParam);
            }
            return;
        }

        case drvtPluginFSInOtherPanel:
            return; // illegal action (FS from other panel cannot be given to active panel)

        case drvtPluginCmd:
        {
            const char* dllName = (const char*)driveTypeParam;
            CPluginData* data = Plugins.GetPluginData(dllName);
            if (data != NULL) // plugin exists, we can execute the command
            {
                if (!fromContextMenu) // FS item command
                {
                    data->ExecuteChangeDriveMenuItem(PANEL_SOURCE);
                }
                else // post-cmd from context menu of FS item
                {
                    data->GetPluginInterfaceForFS()->ExecuteChangeDrivePostCommand(PANEL_SOURCE, postCmd, postCmdParam);
                }
            }
            return;
        }

        default:
        {
            TRACE_E("Unknown DriveType = " << driveType);
            return;
        }
        }
    }
    else
        TopIndexMem.Clear(); // long jump

    ChangePathToDisk(HWindow, DefaultDir[LowerCase[drive] - 'a'], -1, NULL,
                     NULL, TRUE, FALSE, FALSE, NULL, FALSE);
}

void CFilesWindow::UpdateFilterSymbol()
{
    CALL_STACK_MESSAGE_NONE
    DirectoryLine->SetHidden(HiddenFilesCount, HiddenDirsCount);
}

void CFilesWindow::UpdateDriveIcon(BOOL check)
{
    CALL_STACK_MESSAGE2("CFilesWindow::UpdateDriveIcon(%d)", check);
    if (Is(ptDisk))
    {
        if (!check || CheckPath(FALSE) == ERROR_SUCCESS)
        { // only if the path is accessible
            if (DirectoryLine->HWindow != NULL)
            {
                UINT type = MyGetDriveType(GetPath());
                char root[MAX_PATH];
                GetRootPath(root, GetPath());
                HICON hIcon = GetDriveIcon(root, type, TRUE);
                DirectoryLine->SetDriveIcon(hIcon);
                HANDLES(DestroyIcon(hIcon));
            }
            // 2.5RC3: the button in drive bars must be set even if directory line is disabled
            MainWindow->UpdateDriveBars(); // press the correct disk in drive bar
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (DirectoryLine->HWindow != NULL)
            {
                HICON hIcon = LoadArchiveIcon(IconSizes[ICONSIZE_16], IconSizes[ICONSIZE_16], IconLRFlags);
                DirectoryLine->SetDriveIcon(hIcon);
                HANDLES(DestroyIcon(hIcon));
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (DirectoryLine->HWindow != NULL)
                {
                    BOOL destroyIcon;
                    HICON icon = GetPluginFS()->GetFSIcon(destroyIcon);
                    if (icon != NULL) // defined by plugin
                    {
                        DirectoryLine->SetDriveIcon(icon);
                        if (destroyIcon)
                            HANDLES(DestroyIcon(icon));
                    }
                    else // standard
                    {
                        icon = SalLoadIcon(HInstance, IDI_PLUGINFS, IconSizes[ICONSIZE_16]);
                        DirectoryLine->SetDriveIcon(icon);
                        HANDLES(DestroyIcon(icon));
                    }
                }
                // 2.5RC3: the button in drive bars must be set even if directory line is disabled
                MainWindow->UpdateDriveBars(); // press the correct disk in drive bar
            }
        }
    }
}
