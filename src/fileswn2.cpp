// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "usermenu.h"
#include "execute.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "stswnd.h"
#include "snooper.h"
#include "zip.h"
#include "pack.h"
#include "cache.h"
#include "toolbar.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "shellib.h"

//
// ****************************************************************************
// CFilesWindow
//

void CFilesWindow::HandsOff(BOOL off)
{
    CALL_STACK_MESSAGE2("CFilesWindow::HandsOff(%d)", off);
    if (GetMonitorChanges())
    {
        if (off)
        {
            DetachDirectory((CFilesWindow*)this);
        }
        else
        {
            ChangeDirectory((CFilesWindow*)this, GetPath(), MyGetDriveType(GetPath()) == DRIVE_REMOVABLE);
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
            RefreshDiskFreeSpace(TRUE, TRUE);
        }
    }
}

void CFilesWindow::Execute(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::Execute(%d)", index);
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (index < 0 || index >= Dirs->Count + Files->Count)
        return;

    char path[MAX_PATH];
    char fullName[MAX_PATH + 10];
    char doublePath[2 * MAX_PATH];
    WIN32_FIND_DATA data;

    BeginStopRefresh();

    if (Is(ptDisk))
    {
        if (index >= Dirs->Count) // file
        {
            if (CheckPath(FALSE) != ERROR_SUCCESS) // current path is not accessible
            {
                RefreshDirectory(); // refresh panel (e.g., asks for disk or changes path)
                EndStopRefresh();
                return;
            }

            CFileData* file = &Files->At(index - Dirs->Count);
            char* fileName = file->Name;
            char fullPath[MAX_PATH];
            char netFSName[MAX_PATH];
            netFSName[0] = 0;
            if (file->DosName != NULL)
            {
                lstrcpy(fullPath, GetPath());
                if (SalPathAppend(fullPath, file->Name, MAX_PATH) &&
                    SalGetFileAttributes(fullPath) == INVALID_FILE_ATTRIBUTES &&
                    GetLastError() == ERROR_FILE_NOT_FOUND)
                {
                    lstrcpy(fullPath, GetPath());
                    if (SalPathAppend(fullPath, file->DosName, MAX_PATH) &&
                        SalGetFileAttributes(fullPath) != INVALID_FILE_ATTRIBUTES)
                    { // when full name is not available (problem converting from multibyte to UNICODE), we'll use DOS name
                        fileName = file->DosName;
                    }
                }
            }
            BOOL linkIsDir = FALSE;  // TRUE -> shortcut to directory -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> shortcut to file -> test archive
            BOOL linkIsNet = FALSE;  // TRUE -> shortcut to network -> ChangePathToPluginFS
            DWORD err = ERROR_SUCCESS;
            if (StrICmp(file->Ext, "lnk") == 0) // is it not a directory shortcut?
            {
                strcpy(fullName, GetPath());
                if (!SalPathAppend(fullName, fileName, MAX_PATH))
                {
                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    UpdateWindow(HWindow);
                    EndStopRefresh();
                    return;
                }
                OLECHAR oleName[MAX_PATH];
                MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH);
                oleName[MAX_PATH - 1] = 0;

                HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                IShellLink* link;
                if (CoCreateInstance(CLSID_ShellLink, NULL,
                                     CLSCTX_INPROC_SERVER, IID_IShellLink,
                                     (LPVOID*)&link) == S_OK)
                {
                    IPersistFile* fileInt;
                    if (link->QueryInterface(IID_IPersistFile, (LPVOID*)&fileInt) == S_OK)
                    {
                        if (fileInt->Load(oleName, STGM_READ) == S_OK)
                        {
                            if (link->GetPath(fullName, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                            {                                     // the obtained path will be used for accessibility test, after Resolve it may change
                                err = CheckPath(FALSE, fullName); // fullName is a full path (shortcuts support no other)
                                if (err != ERROR_USER_TERMINATED) // if user didn't press ESC, ignore the error
                                {
                                    err = ERROR_SUCCESS; // Resolve may change the path, then we check again
                                }
                            }
                            if (err == ERROR_SUCCESS)
                            {
                                if (link->Resolve(HWindow, SLR_ANY_MATCH | SLR_UPDATE) == NOERROR)
                                {
                                    if (link->GetPath(fullName, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                                    {
                                        // final form of fullName - we verify if it is OK
                                        err = CheckPath(TRUE, fullName); // fullName is a full path (links support no other)
                                        if (err == ERROR_SUCCESS)
                                        {
                                            DWORD attr = SalGetFileAttributes(fullName); // obtained here because data.dwFileAttributes isn't filled
                                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                                            {
                                                linkIsDir = TRUE; // OK we try change-path-to-disk
                                            }
                                            else
                                            {
                                                linkIsFile = TRUE; // OK we check if it's an archive
                                            }
                                        }
                                    }
                                    else // links directly to servers, we can try to open them in Network plugin (Nethood)
                                    {
                                        if (Plugins.GetFirstNethoodPluginFSName(netFSName))
                                        {
                                            if (link->GetPath(fullName, MAX_PATH, NULL, SLGP_RAWPATH) != NOERROR)
                                            { // path is not stored in the link as text, only as an ID list
                                                fullName[0] = 0;
                                                ITEMIDLIST* pidl;
                                                if (link->GetIDList(&pidl) == S_OK && pidl != NULL)
                                                { // get the ID list and ask for the name of its last item, expect "\\\\server"
                                                    IMalloc* alloc;
                                                    if (SUCCEEDED(CoGetMalloc(1, &alloc)))
                                                    {
                                                        if (!GetSHObjectName(pidl, SHGDN_FORPARSING | SHGDN_FORADDRESSBAR, fullName, MAX_PATH, alloc))
                                                            fullName[0] = 0;
                                                        if (alloc->DidAlloc(pidl) == 1)
                                                            alloc->Free(pidl);
                                                        alloc->Release();
                                                    }
                                                }
                                            }
                                            if (fullName[0] == '\\' && fullName[1] == '\\' && fullName[2] != '\\')
                                            { // we check if it's a link to a server (path contains "\\\\server")
                                                char* backslash = fullName + 2;
                                                while (*backslash != 0 && *backslash != '\\')
                                                    backslash++;
                                                if (*backslash == '\\')
                                                    backslash++;
                                                if (*backslash == 0)  // we accept only paths "\\\\", "\\\\server", "\\\\server\\"
                                                    linkIsNet = TRUE; // OK let's try change-path-to-FS
                                            }
                                        }
                                    }
                                }
                                else
                                    err = ERROR_USER_TERMINATED; // in Windows "Missing Shortcut"
                            }
                        }
                        fileInt->Release();
                    }
                    link->Release();
                }
                SetCursor(oldCur);
            }
            if (err != ERROR_SUCCESS)
            {
                EndStopRefresh();
                return; // path error or aborted
            }
            if (linkIsDir || linkIsNet) // link points to network or directory, path is OK, we switch to it
            {
                TopIndexMem.Clear(); // long jump
                if (linkIsDir)
                    ChangePathToDisk(HWindow, fullName);
                else
                    ChangePathToPluginFS(netFSName, fullName);
                UpdateWindow(HWindow);
                EndStopRefresh();
                return;
            }

            if (PackerFormatConfig.PackIsArchive(linkIsFile ? fullName : fileName)) // is it an archive?
            {
                // backup data for TopIndexMem
                strcpy(path, GetPath());
                int topIndex = ListBox->GetTopIndex();

                if (!linkIsFile)
                {
                    // construction of full archive name for ChangePathToArchive
                    strcpy(fullName, GetPath());
                    if (!SalPathAppend(fullName, fileName, MAX_PATH))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                      MB_OK | MB_ICONEXCLAMATION);
                        UpdateWindow(HWindow);
                        EndStopRefresh();
                        return;
                    }
                }
                BOOL noChange;
                if (ChangePathToArchive(fullName, "", -1, NULL, FALSE, &noChange)) // entering the archive successfully
                {
                    if (linkIsFile)
                        TopIndexMem.Clear(); // long jump
                    else
                        TopIndexMem.Push(path, topIndex); // remember top index for return
                }
                else // archive is not accessible
                {
                    if (!noChange)
                        TopIndexMem.Clear(); // failure + not on original path -> long jump
                }
                UpdateWindow(HWindow);
                EndStopRefresh();
                return;
            }

            UserWorkedOnThisPath = TRUE;

            // the ExecuteAssociation below can change the panel path during recursive
            // calls (it contains a message loop), so we store the full file name here
            lstrcpy(fullPath, GetPath());
            if (!SalPathAppend(fullPath, fileName, MAX_PATH))
                fullPath[0] = 0;

            // launch of the default context menu item (association)
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            MainWindow->SetDefaultDirectories(); // to ensure the launching process inherits the correct current directories
            ExecuteAssociation(GetListBoxHWND(), GetPath(), fileName);

            // we add the file to history
            if (fullPath[0] != 0)
                MainWindow->FileHistory->AddFile(fhitOpen, 0, fullPath);

            SetCursor(oldCur);
        }
        else // directory
        {
            strcpy(path, GetPath());
            CFileData* dir = &Dirs->At(index);
            if (index == 0 && strcmp(dir->Name, "..") == 0) // ".. <Up>"
            {
                char* prevDir = NULL;
                if (!CutDirectory(path, &prevDir))
                {
                    if (path[0] == '\\' && path[1] == '\\')
                    {
                        char* s = path + 2;
                        while (*s != 0 && *s != '\\')
                            s++;
                        CPluginData* nethoodPlugin = NULL;
                        if (*s == '\\' && Plugins.GetFirstNethoodPluginFSName(doublePath, &nethoodPlugin))
                        {
                            *s++ = 0;
                            char* focusName = s;
                            while (*s != 0 && *s != '\\')
                                s++;
                            if (*s == '\\')
                                *s = 0;
                            nethoodPlugin->EnsureShareExistsOnServer(HWindow, this == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT,
                                                                     path + 2, focusName);
                            ChangePathToPluginFS(doublePath, path, -1, focusName);
                            if (Is(ptPluginFS))
                            {
                                TopIndexMem.Clear(); // if we didn't remain on a disk path (UNC root), it's a long jump
                                UpdateWindow(HWindow);
                            }
                        }
                    }
                    EndStopRefresh();
                    return; // nothing to shorten or we're already on Nethood path
                }
                int topIndex; // next top index, -1 -> invalid
                if (!TopIndexMem.FindAndPop(path, topIndex))
                    topIndex = -1;
                if (!ChangePathToDisk(HWindow, path, topIndex, prevDir))
                { // failed to shorten the path - long jump
                    TopIndexMem.Clear();
                }
            }
            else // subdirectory
            {
                // backup data for TopIndexMem (path + topIndex)
                int topIndex = ListBox->GetTopIndex();

                // backup of caret if a case of access-denied directory happens
                int caretIndex = GetCaretIndex();

                // new path
                strcpy(fullName, path);
                if (!SalPathAppend(fullName, dir->Name, MAX_PATH))
                {
                    SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    EndStopRefresh();
                    return;
                }

                // Vista: we handle unlistable junction points: change path to junction point target
                char junctTgtPath[MAX_PATH];
                int repPointType;
                if (GetPathDriveType() == DRIVE_FIXED && (dir->Attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
                    GetReparsePointDestination(fullName, junctTgtPath, MAX_PATH, &repPointType, TRUE) &&
                    repPointType == 2 /* JUNCTION POINT */ &&
                    SalPathAppend(fullName, "*", MAX_PATH + 10))
                {
                    WIN32_FIND_DATA fileData;
                    HANDLE search = HANDLES_Q(FindFirstFile(fullName, &fileData));
                    DWORD err = GetLastError();
                    CutDirectory(fullName);
                    if (search != INVALID_HANDLE_VALUE)
                        HANDLES(FindClose(search));
                    else
                    {
                        if (err == ERROR_ACCESS_DENIED)
                        {
                            TopIndexMem.Clear(); // long jump
                            ChangePathToDisk(HWindow, junctTgtPath);
                            UpdateWindow(HWindow);
                            EndStopRefresh();
                            return;
                        }
                    }
                }

                BOOL noChange;
                BOOL refresh = TRUE;
                if (ChangePathToDisk(HWindow, fullName, -1, NULL, &noChange, FALSE))
                {
                    TopIndexMem.Push(path, topIndex); // we remember top index for return
                }
                else // failure
                {
                    if (!IsTheSamePath(path, GetPath())) // we're not on the original path -> long jump
                    {                                    // the condition "!noChange" is not enough - it signals "path change or reload - access-denied-dir"
                        TopIndexMem.Clear();
                    }
                    else // path unchanged (immediately shortened back to original)
                    {
                        refresh = FALSE;
                        if (!noChange) // access-denied-dir: listing refreshed, but path unchanged
                        {              // we restore listbox to original indices and finish "refresh" after ChangePathToDisk
                            RefreshListBox(0, topIndex, caretIndex, FALSE, FALSE);
                        }
                    }
                }
                if (refresh)
                {
                    RefreshListBox(0, -1, -1, FALSE, FALSE);
                }
            }
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (index >= Dirs->Count)
            {
                UserWorkedOnThisPath = TRUE;
                ExecuteFromArchive(index); // file
            }
            else // directory
            {
                CFileData* dir = &Dirs->At(index);
                if (index == 0 && strcmp(dir->Name, "..") == 0) // ".. <Up>"
                {
                    if (GetZIPPath()[0] == 0) // we exit the archive
                    {
                        const char* s = strrchr(GetZIPArchive(), '\\'); // ZIP archive doesn't contain an extra '\\' at the end
                        if (s != NULL)                                  // "always true"
                        {
                            strcpy(path, s + 1); // prev-dir

                            int topIndex; // next top index, -1 -> invalid
                            if (!TopIndexMem.FindAndPop(GetPath(), topIndex))
                                topIndex = -1;

                            // actual path change
                            BOOL noChange;
                            if (!ChangePathToDisk(HWindow, GetPath(), topIndex, path, &noChange))
                            { // failed to shorten the path - reject-close-archive or long jump
                                if (!noChange)
                                    TopIndexMem.Clear(); // long jump
                                else
                                {
                                    if (topIndex != -1) // if top index could be retrieved
                                    {
                                        TopIndexMem.Push(GetPath(), topIndex); // we return top index for next time
                                    }
                                }
                            }
                        }
                    }
                    else // we're shortening path inside archive
                    {
                        // we split zip-path into new zip-path and prev-dir
                        strcpy(path, GetZIPPath());
                        char* prevDir;
                        char* s = strrchr(path, '\\'); // zip-path has no redundant backslashes (start/end)
                        if (s != NULL)                 // format: "beg-path\\dir"
                        {
                            *s = 0;
                            prevDir = s + 1;
                        }
                        else // format: "dir"
                        {
                            memmove(path + 1, path, strlen(path) + 1);
                            *path = 0;
                            prevDir = path + 1;
                        }

                        // we build shortened path to archive and obtain top index accordingly
                        strcpy(doublePath, GetZIPArchive());
                        SalPathAppend(doublePath, path, 2 * MAX_PATH);
                        int topIndex; // next top index, -1 -> invalid
                        if (!TopIndexMem.FindAndPop(doublePath, topIndex))
                            topIndex = -1;

                        // actual path change
                        if (!ChangePathToArchive(GetZIPArchive(), path, topIndex, prevDir)) // "always false"
                        {                                                                   // failed to shorten path - long jump
                            TopIndexMem.Clear();
                        }
                    }
                }
                else // subdirectory
                {
                    // backup data for TopIndexMem (doublePath + topIndex)
                    strcpy(doublePath, GetZIPArchive());
                    SalPathAppend(doublePath, GetZIPPath(), 2 * MAX_PATH);
                    int topIndex = ListBox->GetTopIndex();

                    // new path
                    strcpy(fullName, GetZIPPath());
                    if (!SalPathAppend(fullName, dir->Name, MAX_PATH))
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                                      MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
                        BOOL noChange;
                        if (ChangePathToArchive(GetZIPArchive(), fullName, -1, NULL, FALSE, &noChange)) // "always true"
                        {
                        TopIndexMem.Push(doublePath, topIndex); // we remember top index for return
                        }
                        else
                        {
                            if (!noChange)
                                TopIndexMem.Clear(); // failure + we're not on original path -> long jump
                        }
                    }
                }
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                BOOL isDir = index < Dirs->Count ? 1 : 0;
                if (isDir && index == 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                    isDir = 2; // up-dir
                CFileData* file = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                CPluginInterfaceForFSEncapsulation* ifaceForFS = GetPluginFS()->GetPluginInterfaceForFS();
                char fsNameBuf[MAX_PATH]; // GetPluginFS() may cease to exist, so we copy fsName to local buffer
                lstrcpyn(fsNameBuf, GetPluginFS()->GetPluginFSName(), MAX_PATH);
                ifaceForFS->ExecuteOnFS(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                        GetPluginFS()->GetInterface(), fsNameBuf,
                                        GetPluginFS()->GetPluginFSNameIndex(), *file, isDir);
            }
        }
    }
    UpdateWindow(HWindow);
    EndStopRefresh();
}

void CFilesWindow::ChangeSortType(CSortType newType, BOOL reverse, BOOL force)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ChangeSortType(%d, %d)", newType, reverse);
    if (!force && SortType == newType && !reverse)
        return;
    if (SortType != newType)
    {
        SortType = newType;
        ReverseSort = FALSE;

        //    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);    // only when sorting changes  // commented out, not sure why it's here: Petr
    }
    else
    {
        if (reverse)
        {
            ReverseSort = !ReverseSort;

            //      EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);  // only when sorting changes  // commented out, not sure why it's here: Petr
        }
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //---  storing the focused item and sorting of old items by name
    int focusIndex = GetCaretIndex();
    CFileData d1;
    if (focusIndex >= 0 && focusIndex < Dirs->Count + Files->Count)
        d1 = (focusIndex < Dirs->Count) ? Dirs->At(focusIndex) : Files->At(focusIndex - Dirs->Count);
    else
        d1.Name = NULL;
    //---  sorting
    if (UseSystemIcons || UseThumbnails)
        SleepIconCacheThread();
    SortDirectory();
    if (UseSystemIcons || UseThumbnails)
        WakeupIconCacheThread();
    //---  select items for focus + perform final sorting
    CLessFunction lessDirs;  // used to compare which is smaller; needed, see optimization in the search loop
    CLessFunction lessFiles; // used to compare which is smaller; needed, see optimization in the search loop
    switch (SortType)
    {
    case stName:
        lessDirs = lessFiles = LessNameExt;
        break;
    case stExtension:
        lessDirs = lessFiles = LessExtName;
        break;

    case stTime:
    {
        if (Configuration.SortDirsByName)
            lessDirs = LessNameExt;
        else
            lessDirs = LessTimeNameExt;
        lessFiles = LessTimeNameExt;
        break;
    }

    case stAttr:
        lessDirs = lessFiles = LessAttrNameExt;
        break;
    default: /*stSize*/
        lessDirs = lessFiles = LessSizeNameExt;
        break;
    }

    int i;
    int count;
    if (focusIndex < Dirs->Count) // we're searching for a directory
    {
        i = 0;
        count = Dirs->Count;
    }
    else
    {
        i = Dirs->Count;                    // we're searching for a file
        count = Dirs->Count + Files->Count; // there was a bug here; count wasn't initialized
    }

    if (d1.Name != NULL)
    {
        if (i == 0 && Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
        {
            if (strcmp(d1.Name, "..") == 0)
            {
                focusIndex = 0;
                i = count;
            }
            else
                i = 1; // we must skip ".."; it's not included
        }
        for (; i < count; i++)
        {
            if (i < Dirs->Count)
            {
                CFileData* d2 = &Dirs->At(i);
                if (!lessDirs(*d2, d1, ReverseSort)) // due to sorting this becomes TRUE only at the searched item
                {
                    if (!lessDirs(d1, *d2, ReverseSort))
                        focusIndex = i; // condition unnecessary; should be "always true"
                    break;
                }
            }
            else
            {
                CFileData* d2 = &Files->At(i - Dirs->Count);
                if (!lessFiles(*d2, d1, ReverseSort)) // due to sorting this becomes TRUE only at the searched item
                {
                    if (!lessFiles(d1, *d2, ReverseSort))
                        focusIndex = i; // condition unnecessary; should be "always true"
                    break;
                }
            }
        }
    }
    if (focusIndex >= count)
        focusIndex = count - 1;
    //---  using the acquired data for the final listbox setup
    SetCaretIndex(focusIndex, FALSE); // focus
    IdleRefreshStates = TRUE;         // force state-variables check on next Idle
    ListBox->PaintHeaderLine();
    RepaintListBox(DRAWFLAG_SKIP_VISTEST);
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
}

BOOL CFilesWindow::ChangeToRescuePathOrFixedDrive(HWND parent, BOOL* noChange, BOOL refreshListBox,
                                                  BOOL canForce, int tryCloseReason, int* failReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::ChangeToRescuePathOrFixedDrive(, , %d, %d, %d,)",
                        refreshListBox, canForce, tryCloseReason);
    BOOL noChangeUsed = FALSE;
    char ifPathIsInaccessibleGoTo[MAX_PATH];
    GetIfPathIsInaccessibleGoTo(ifPathIsInaccessibleGoTo);
    if (ifPathIsInaccessibleGoTo[0] == '\\' && ifPathIsInaccessibleGoTo[1] == '\\' ||
        ifPathIsInaccessibleGoTo[0] != 0 && ifPathIsInaccessibleGoTo[1] == ':')
    {
        int failReasonInt;
        BOOL ret = ChangePathToDisk(parent, ifPathIsInaccessibleGoTo, -1, NULL, noChange, refreshListBox,
                                    canForce, FALSE, &failReasonInt, TRUE, tryCloseReason);
        if (!ret && failReasonInt != CHPPFR_CANNOTCLOSEPATH)
            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
        if (ret || failReasonInt == CHPPFR_SHORTERPATH)
        {
            if (failReason != NULL)
                *failReason = CHPPFR_SUCCESS; // shortening the rescue path isn't considered a failure
            return TRUE;
        }
        if (failReasonInt == CHPPFR_CANNOTCLOSEPATH)
        {
            if (failReason != NULL)
                *failReason = failReasonInt;
            return FALSE; // the issue "cannot close the current path in the panel" won't be solved by switching to a fixed drive (we would just ask about disconnect twice)
        }
        noChangeUsed = TRUE;
    }
    else
    {
        if (!CriticalShutdown)
        {
            SalMessageBox(parent, LoadStr(IDS_INVALIDESCAPEPATH), LoadStr(IDS_ERRORCHANGINGDIR),
                          MB_OK | MB_ICONEXCLAMATION);
            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
        }
    }
    return ChangeToFixedDrive(parent, (!noChangeUsed || noChange != NULL && *noChange) ? noChange : NULL,
                              refreshListBox, canForce, failReason, tryCloseReason);
}

BOOL CFilesWindow::ChangeToFixedDrive(HWND parent, BOOL* noChange, BOOL refreshListBox, BOOL canForce,
                                      int* failReason, int tryCloseReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::ChangeToFixedDrive(, , %d, %d, , %d)",
                        refreshListBox, canForce, tryCloseReason);
    if (noChange != NULL)
        *noChange = TRUE;
    char sysDir[MAX_PATH];
    char root[4] = " :\\";
    if (GetWindowsDirectory(sysDir, MAX_PATH) != 0 && sysDir[0] != 0 && sysDir[1] == ':')
    {
        root[0] = sysDir[0];
        if (GetDriveType(root) == DRIVE_FIXED)
        {
            TopIndexMem.Clear(); // long jump
            return ChangePathToDisk(parent, root, -1, NULL, noChange, refreshListBox, canForce,
                                    FALSE, failReason, TRUE, tryCloseReason);
        }
    }
    DWORD disks = GetLogicalDrives();
    disks >>= 2; // skip A: and B:, during floppy formatting they sometimes become DRIVE_FIXED
    char d = 'C';
    while (d <= 'Z')
    {
        if (disks & 1)
        {
            root[0] = d;
            if (GetDriveType(root) == DRIVE_FIXED)
            {
                TopIndexMem.Clear(); // long jump
                return ChangePathToDisk(parent, root, -1, NULL, noChange, refreshListBox, canForce,
                                        FALSE, failReason, TRUE, tryCloseReason);
            }
        }
        disks >>= 1;
        d++;
    }
    if (failReason != NULL)
        *failReason = CHPPFR_INVALIDPATH;
    return FALSE;
}

void CFilesWindow::ConnectNet(BOOL readOnlyUNC, const char* netRootPath, BOOL changeToNewDrive,
                              char* newlyMappedDrive)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ConnectNet(%s, %d,)", netRootPath, changeToNewDrive);

    if (newlyMappedDrive != NULL)
        *newlyMappedDrive = 0;

    if (SystemPolicies.GetNoNetConnectDisconnect())
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
        params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
        params.Text = LoadStr(IDS_POLICIESRESTRICTION);
        params.ContextHelpId = IDH_GROUPPOLICY;
        params.HelpCallback = MessageBoxHelpCallback;
        SalMessageBoxEx(&params);
        return;
    }

    BeginStopRefresh(); // snooper takes a break

    DWORD disks = changeToNewDrive || newlyMappedDrive != NULL ? GetLogicalDrives() : 0;

    BOOL success;
    const char* netPath = netRootPath == NULL ? GetPath() : netRootPath;
    if (netPath[0] == '\\' && netPath[1] == '\\') // UNC path
    {
        CONNECTDLGSTRUCT cs;
        cs.cbStructure = sizeof(cs);
        cs.hwndOwner = HWindow;
        NETRESOURCE nr;
        memset(&nr, 0, sizeof(nr));
        char root[MAX_PATH];
        GetRootPath(root, netPath);
        root[strlen(root) - 1] = 0;
        nr.lpRemoteName = root;
        nr.dwType = RESOURCETYPE_DISK;
        cs.lpConnRes = &nr;
        cs.dwFlags = readOnlyUNC ? CONNDLG_RO_PATH : CONNDLG_USE_MRU;
        success = WNetConnectionDialog1(&cs) == WN_SUCCESS;
    }
    else
        success = WNetConnectionDialog(HWindow, RESOURCETYPE_DISK) == NO_ERROR;

    if ((changeToNewDrive || newlyMappedDrive != NULL) && success)
    {
        disks = (GetLogicalDrives() ^ disks);
        if (disks != 0)
        {
            char d = 'A';
            while ((disks >>= 1) != 0)
                d++;
            UpdateWindow(MainWindow->HWindow);
            if (d >= 'A' && d <= 'Z') // always true
            {
                if (newlyMappedDrive != NULL)
                    *newlyMappedDrive = d;
                if (changeToNewDrive)
                {
                    char root[4] = " :\\";
                    root[0] = d;
                    ChangePathToDisk(HWindow, root, -1, NULL, NULL, TRUE, FALSE, FALSE, NULL, FALSE);
                }
            }
        }
    }

    EndStopRefresh(); // the snooper will start again now
}

void CFilesWindow::DisconnectNet()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DisconnectNet()");

    if (SystemPolicies.GetNoNetConnectDisconnect())
    {
        MSGBOXEX_PARAMS params;
        memset(&params, 0, sizeof(params));
        params.HParent = HWindow;
        params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
        params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
        params.Text = LoadStr(IDS_POLICIESRESTRICTION);
        params.ContextHelpId = IDH_GROUPPOLICY;
        params.HelpCallback = MessageBoxHelpCallback;
        SalMessageBoxEx(&params);
        return;
    }

    BeginSuspendMode(); // snooper takes a break

    SetCurrentDirectoryToSystem(); // allows unmapping a drive even from the panel

    // we disconnect from the mapped drive, otherwise it cannot be detached silently (system warns it's in use)
    BOOL releaseLeft = MainWindow->LeftPanel->GetNetworkDrive() &&    // network drive (ptDisk only)
                       MainWindow->LeftPanel->GetPath()[0] != '\\';   // not UNC
    BOOL releaseRight = MainWindow->RightPanel->GetNetworkDrive() &&  // network drive (ptDisk only)
                        MainWindow->RightPanel->GetPath()[0] != '\\'; // not UNC
    if (releaseLeft)
        MainWindow->LeftPanel->HandsOff(TRUE);
    if (releaseRight)
        MainWindow->RightPanel->HandsOff(TRUE);

    //  Under Windows XP the WNetDisconnectDialog is modeless. Users lost it behind Salamander
    //  and wondered why accelerators didn't work. When closing Salamander it crashed here
    //  because MainWindow was NULL;
    //  WNetDisconnectDialog(HWindow, RESOURCETYPE_DISK);

    CDisconnectDialog dlg(this);
    if (dlg.Execute() == IDCANCEL && dlg.NoConnection())
    {
        // dialog didn't appear because it contained zero resources -- show info
        SalMessageBox(HWindow, LoadStr(IDS_DISCONNECT_NODRIVES),
                      LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
    }

    if (releaseLeft)
        MainWindow->LeftPanel->HandsOff(FALSE);
    if (releaseRight)
        MainWindow->RightPanel->HandsOff(FALSE);

    if (MainWindow->LeftPanel->CheckPath(FALSE) != ERROR_SUCCESS)
        MainWindow->LeftPanel->ChangeToRescuePathOrFixedDrive(MainWindow->LeftPanel->HWindow);
    if (MainWindow->RightPanel->CheckPath(FALSE) != ERROR_SUCCESS)
        MainWindow->RightPanel->ChangeToRescuePathOrFixedDrive(MainWindow->RightPanel->HWindow);

    EndSuspendMode(); // the snooper will start again now
}

void CFilesWindow::DriveInfo()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DriveInfo()");
    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return;

        BeginStopRefresh(); // snooper takes a break

        CDriveInfo dlg(HWindow, GetPath());
        dlg.Execute();
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // the snooper will start again now
    }
    else
    {
        if (Is(ptPluginFS) &&
            GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_SHOWINFO))
        {
            GetPluginFS()->ShowInfoDialog(GetPluginFS()->GetPluginFSName(), HWindow);
            UpdateWindow(MainWindow->HWindow);
        }
    }
}

void CFilesWindow::ToggleDirectoryLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleDirectoryLine()");
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return;
    }
    if (DirectoryLine->HWindow != NULL) // turn off
    {
        DirectoryLine->ToggleToolBar();
        DestroyWindow(DirectoryLine->HWindow);
    }
    else // turn on
    {
        if (!DirectoryLine->Create(CWINDOW_CLASSNAME2,
                                   "",
                                   WS_CHILD | WS_CLIPSIBLINGS | WS_CLIPCHILDREN,
                                   0, 0, 0, 0,
                                   HWindow,
                                   (HMENU)IDC_DIRECTORYLINE,
                                   HInstance,
                                   DirectoryLine))
            TRACE_E("Unable to create directory-line.");
        IdleForceRefresh = TRUE;  // we force an update
        IdleRefreshStates = TRUE; // we force state-variable check on next idle
    }
    InvalidateRect(HWindow, NULL, TRUE);
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
    if (DirectoryLine->HWindow != NULL)
    {
        UpdateDriveIcon(TRUE);
        ShowWindow(DirectoryLine->HWindow, SW_SHOW);
    }
    // if the middle toolbar is displayed, give it a chance to position itself
    if (MainWindow->MiddleToolBar != NULL && MainWindow->MiddleToolBar->HWindow != NULL)
        MainWindow->LayoutWindows();
}

void CFilesWindow::ToggleStatusLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleStatusLine()");
    if (HWindow == NULL)
    {
        TRACE_E("HWindow == NULL");
        return;
    }
    if (StatusLine->HWindow != NULL) // turn off
        DestroyWindow(StatusLine->HWindow);
    else // turn on
    {
        if (!StatusLine->Create(CWINDOW_CLASSNAME2,
                                "",
                                WS_CHILD | WS_CLIPSIBLINGS,
                                0, 0, 0, 0,
                                HWindow,
                                (HMENU)IDC_STATUSLINE,
                                HInstance,
                                StatusLine))
            TRACE_E("Unable to create information-line.");
    }
    RECT r;
    GetClientRect(HWindow, &r);
    SendMessage(HWindow, WM_SIZE, SIZE_RESTORED,
                MAKELONG(r.right - r.left, r.bottom - r.top));
    if (StatusLine->HWindow != NULL)
        ShowWindow(StatusLine->HWindow, SW_SHOW);
}

void CFilesWindow::ToggleHeaderLine()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ToggleHeaderLine()");
    BOOL headerLine = !HeaderLineVisible;
    if (GetViewMode() == vmBrief)
        headerLine = FALSE;
    ListBox->SetMode(GetViewMode() == vmBrief ? vmBrief : vmDetailed, headerLine);
}

int CFilesWindow::GetViewTemplateIndex()
{
    return (int)(ViewTemplate - MainWindow->ViewTemplates.Items);
}

int CFilesWindow::GetNextTemplateIndex(BOOL forward, BOOL wrap)
{
    int oldIndex = GetViewTemplateIndex();
    int newIndex = oldIndex;
    int delta = forward ? 1 : -1;
    do
    {
        newIndex += delta;
        if (wrap)
        {
            if (forward)
            {
                if (newIndex > 9)
                    newIndex = 1; // the edge item was empty; jump to the other end of the list
            }
            else
            {
                if (newIndex < 1)
                    newIndex = 9; // the edge item was empty; jump to the other end of the list
            }
        }
        else
        {
            if (forward)
            {
                if (newIndex > 9)
                    newIndex = oldIndex; // the edge item was empty; return to the last valid one
            }
            else
            {
                if (newIndex < 1)
                    newIndex = oldIndex; // the edge item was empty; return to the last valid one
            }
        }
    } while (Parent->ViewTemplates.Items[newIndex].Name[0] == 0 && newIndex != oldIndex);
    return newIndex;
}

BOOL CFilesWindow::IsViewTemplateValid(int templateIndex)
{
    CALL_STACK_MESSAGE2("CFilesWindow::IsViewTemplateValid(%d)", templateIndex);
    if (templateIndex < 1) // tree is not supported yet
        return FALSE;
    CViewTemplate* newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    if (lstrlen(newTemplate->Name) == 0)
        return FALSE;
    return TRUE;
}

BOOL CFilesWindow::SelectViewTemplate(int templateIndex, BOOL canRefreshPath,
                                      BOOL calledFromPluginBeforeListing, DWORD columnValidMask,
                                      BOOL preserveTopIndex, BOOL salamanderIsStarting)
{
    CALL_STACK_MESSAGE5("CFilesWindow::SelectViewTemplate(%d, %d, %d, 0x%X)", templateIndex,
                        canRefreshPath, calledFromPluginBeforeListing, columnValidMask);

    if (templateIndex == 0)
        return FALSE;
    CViewTemplate* newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    if (lstrlen(newTemplate->Name) == 0)
    {
        // undefined view is not desired - we force the detailed view which always exists
        templateIndex = 2;
        newTemplate = &Parent->ViewTemplates.Items[templateIndex];
    }

    CViewModeEnum oldViewMode = GetViewMode();
    if (!calledFromPluginBeforeListing || ViewTemplate != newTemplate)
    {
        CViewModeEnum newViewMode;
        switch (templateIndex)
        {
            //      case 0: newViewMode = vmTree; break;
        case 1:
            newViewMode = vmBrief;
            break;
        case 3:
            newViewMode = vmIcons;
            break;
        case 4:
            newViewMode = vmThumbnails;
            break;
        case 5:
            newViewMode = vmTiles;
            break;
        default:
            newViewMode = vmDetailed;
            break;
        }
        ViewTemplate = newTemplate;

        BOOL headerLine = HeaderLineVisible;
        if (newViewMode != vmDetailed)
            headerLine = FALSE;
        ListBox->SetMode(newViewMode, headerLine);
    }

    // we build new columns
    BuildColumnsTemplate();
    CopyColumnsTemplateToColumns();
    DeleteColumnsWithoutData(columnValidMask);

    if (!calledFromPluginBeforeListing)
    {
        if (PluginData.NotEmpty())
        {
            CSalamanderView view(this);
            PluginData.SetupView(this == MainWindow->LeftPanel,
                                 &view, Is(ptZIPArchive) ? GetZIPPath() : NULL,
                                 Is(ptZIPArchive) ? GetArchiveDir()->GetUpperDir(GetZIPPath()) : NULL);
        }

        // once the view mode differs we must refresh (a different icon or thumbnail size is required)
        // the only exception is brief and detailed, which share the same icons
        BOOL needRefresh = oldViewMode != GetViewMode() &&
                           (oldViewMode != vmBrief || GetViewMode() != vmDetailed) &&
                           (oldViewMode != vmDetailed || GetViewMode() != vmBrief);
        if (canRefreshPath && needRefresh)
        {
            // until ReadDirectory we work with simple icons because the icon geometry changes
            TemporarilySimpleIcons = TRUE;

            // let it compute item sizes and ensure the focused item is visible
            RefreshListBox(0, -1, FocusedIndex, FALSE, FALSE);

            // perform a hard refresh
            HANDLES(EnterCriticalSection(&TimeCounterSection));
            int t1 = MyTimeCounter++;
            HANDLES(LeaveCriticalSection(&TimeCounterSection));
            PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }
        else
        {
            if (needRefresh)
            {
                if (!salamanderIsStarting)
                    TRACE_E("CFilesWindow::SelectViewTemplate(): unexpected situation: refresh is needed, but it's not allowed!");
                // for now at least: until the next refresh (and thus ReadDirectory) we'll work with simple icons because the icon geometry changes
                TemporarilySimpleIcons = TRUE;
            }
            // let it compute item sizes and ensure the focused item is visible
            // if 'preserveTopIndex' is TRUE, we must not let the panel scroll beyond the focus
            RefreshListBox(-1, preserveTopIndex ? ListBox->TopIndex : -1, FocusedIndex, FALSE, FALSE);
        }
    }

    // when the panel mode changes we must clear saved top-indices (incompatible data are being stored)
    if (oldViewMode != GetViewMode())
        TopIndexMem.Clear();

    return TRUE;
}

BOOL CFilesWindow::IsExtensionInSeparateColumn()
{
    return Columns.Count >= 2 && Columns.At(1).ID == COLUMN_ID_EXTENSION;
}

void CFilesWindow::RedrawIndex(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::RedrawIndex(%d)", index);
    if (index >= 0 && index < Dirs->Count + Files->Count)
        ListBox->PaintItem(index, DRAWFLAG_SELFOC_CHANGE);
    else if (Dirs->Count + Files->Count == 0)
        ListBox->PaintAllItems(NULL, 0); // we ensure the text about empty panel is drawn
}

void CFilesWindow::ItemFocused(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ItemFocused(%d)", index);
    if (GetSelCount() == 0 && index != LastFocus && index >= 0 &&
        index < Dirs->Count + Files->Count)
    {
        LastFocus = index;
        CFileData* f = (index < Dirs->Count) ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

        char buff[1000];
        DWORD varPlacements[100];
        int varPlacementsCount = 100;
        BOOL done = FALSE;
        if (Is(ptZIPArchive) || Is(ptPluginFS))
        {
            if (PluginData.NotEmpty())
            {
                if (PluginData.GetInfoLineContent(MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT,
                                                  f, index < Dirs->Count, 0, 0, TRUE, CQuadWord(0, 0), buff,
                                                  varPlacements, varPlacementsCount))
                {
                    done = TRUE;
                }
                else
                    varPlacementsCount = 100; // might have been corrupted
            }
        }

        if (done ||
            ExpandInfoLineItems(HWindow, Configuration.InfoLineContent, &PluginData, f,
                                index < Dirs->Count, buff, 1000,
                                varPlacements, &varPlacementsCount, ValidFileData, Is(ptDisk)))
        {
            if (StatusLine->SetText(buff))
                StatusLine->SetSubTexts(varPlacements, varPlacementsCount);
        }
    }
    IdleRefreshStates = TRUE; // we force state-variables check on next Idle
}

void CFilesWindow::SetValidFileData(DWORD validFileData)
{
    DWORD mask = 0xFFFFFFFF;
    if (!PluginData.NotEmpty())
        mask &= ~VALID_DATA_PL_SIZE & ~VALID_DATA_PL_DATE & ~VALID_DATA_PL_TIME;
    else
    {
        if (validFileData & VALID_DATA_SIZE)
            mask &= ~VALID_DATA_PL_SIZE;
        if (validFileData & VALID_DATA_DATE)
            mask &= ~VALID_DATA_PL_DATE;
        if (validFileData & VALID_DATA_TIME)
            mask &= ~VALID_DATA_PL_TIME;
    }
    ValidFileData = validFileData & mask;
}

BOOL CFilesWindow::PrepareCloseCurrentPath(HWND parent, BOOL canForce, BOOL canDetach, BOOL& detachFS,
                                           int tryCloseReason)
{
    CALL_STACK_MESSAGE4("CFilesWindow::PrepareCloseCurrentPath(, %d, %d, , %d)",
                        canForce, canDetach, tryCloseReason);
    char buf[2 * MAX_PATH + 100];

    if (Is(ptDisk))
    {
        detachFS = FALSE;
        return TRUE; // a disk path can always be closed
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            BOOL someFilesChanged = FALSE;
            if (AssocUsed)
            {
                // if the user didn't suppress it, we show info about closing an archive that contains edited files
                if (Configuration.CnfrmCloseArchive && !CriticalShutdown)
                {
                    char title[100];
                    char text[MAX_PATH + 500];
                    char checkText[200];
                    sprintf(title, LoadStr(IDS_INFOTITLE));
                    sprintf(text, LoadStr(IDS_ARCHIVECLOSEEDIT), GetZIPArchive());
                    sprintf(checkText, LoadStr(IDS_DONTSHOWAGAIN));
                    BOOL dontShow = !Configuration.CnfrmCloseArchive;

                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = parent;
                    params.Flags = MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT | MSGBOXEX_HINT;
                    params.Caption = title;
                    params.Text = text;
                    params.CheckBoxText = checkText;
                    params.CheckBoxValue = &dontShow;
                    SalMessageBoxEx(&params);

                    Configuration.CnfrmCloseArchive = !dontShow;
                }
                // pack modified files again (only if it's not a critical shutdown) we prepare them for next use
                UnpackedAssocFiles.CheckAndPackAndClear(parent, &someFilesChanged);
                // during critical shutdown we pretend updated files don't exist because there is no time to pack
                // them back to the archive. We must not delete them so the user can manually repack them after restart,
                // exception is when nothing was edited then everything may be deleted even during
                // critical shutdown (it's fast and not confusing for the user with unnecessary questions)
                if (!someFilesChanged || !CriticalShutdown)
                {
                    SetEvent(ExecuteAssocEvent); // start file cleanup
                    DiskCache.WaitForIdle();
                    ResetEvent(ExecuteAssocEvent); // finish file cleanup
                }
            }
            AssocUsed = FALSE;
            // if edited files might be in the disk cache or this archive isn't open
            // in the other panel, we'll remove its cached files; it will unpack again next time it's opened
            // (the archive might be edited in the meantime)
            CFilesWindow* another = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
            if (someFilesChanged || !another->Is(ptZIPArchive) || StrICmp(another->GetZIPArchive(), GetZIPArchive()) != 0)
            {
                StrICpy(buf, GetZIPArchive()); // the disk cache stores the archive name in lowercase (allows case-insensitive comparison of the name from Windows file system)
                DiskCache.FlushCache(buf);
            }

            // we call the plugin's CPluginInterfaceAbstract::CanCloseArchive
            BOOL canclose = TRUE;
            int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
            if (format != 0) // we found a supported archive
            {
                format--;
                BOOL userForce = FALSE;
                BOOL userAsked = FALSE;
                CPluginData* plugin = NULL;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: is it internal processing (plugin)?
                {
                    plugin = Plugins.Get(-index - 1);
                    if (plugin != NULL)
                    {
                        if (!plugin->CanCloseArchive(this, GetZIPArchive(), CriticalShutdown)) // it refuses to close
                        {
                            canclose = FALSE;
                            if (canForce) // we can ask the user whether to force it
                            {
                                sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                userAsked = TRUE;
                                if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                  MB_YESNO | MB_ICONQUESTION) == IDYES) // user chooses "Close"
                                {
                                    userForce = TRUE;
                                    plugin->CanCloseArchive(this, GetZIPArchive(), TRUE); // force==TRUE
                                    canclose = TRUE;
                                }
                            }
                        }
                    }
                }
                if (PackerFormatConfig.GetUsePacker(format)) // supports editing?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // is it internal processing (plugin)?
                    {
                        if (plugin != Plugins.Get(-index - 1)) // if view==edit, don't call again
                        {
                            plugin = Plugins.Get(-index - 1);
                            if (plugin != NULL)
                            {
                                if (!plugin->CanCloseArchive(this, GetZIPArchive(), userForce || CriticalShutdown)) // it refuses to close
                                {
                                    canclose = FALSE;
                                    if (canForce && !userAsked) // we can ask the user whether to force it
                                    {
                                        sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user chooses "Close"
                                        {
                                            plugin->CanCloseArchive(this, GetZIPArchive(), TRUE);
                                            canclose = TRUE;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            return canclose; // for now only the plugin's CPluginInterfaceAbstract::CanCloseArchive decides
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (!canForce && !CriticalShutdown) // we can't ask the user about forcing
                {
                    detachFS = FALSE; // to ensure a known value in case the plugin doesn't modify it
                    BOOL r = GetPluginFS()->TryCloseOrDetach(FALSE, canDetach, detachFS, tryCloseReason);
                    if (!r || !canDetach)
                        detachFS = FALSE; // verification/correction of the output value
                    return r;
                }
                else // forcing is allowed -> we must close, detaching isn't permitted
                {
                    if (GetPluginFS()->TryCloseOrDetach(CriticalShutdown, FALSE, detachFS, tryCloseReason) || // try closing without forceClose==TRUE (except during critical shutdown)
                        CriticalShutdown)
                    {
                        detachFS = FALSE;
                        return TRUE; // closed successfully
                    }
                    else // ask the user whether to close it against the FS's will
                    {
                        char path[2 * MAX_PATH];
                        GetGeneralPath(path, 2 * MAX_PATH);
                        sprintf(buf, LoadStr(IDS_FSFORCECLOSE), path);
                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user chooses "Close"
                        {
                            GetPluginFS()->TryCloseOrDetach(TRUE, FALSE, detachFS, tryCloseReason);
                            detachFS = FALSE;
                            return TRUE; // closed successfully
                        }
                        else
                        {
                            detachFS = FALSE;
                            return FALSE; // cannot close
                        }
                    }
                }
            }
            else
            {
                TRACE_E("Unexpected situation in CFilesWindow::PrepareCloseCurrentPath()");
                return FALSE;
            }
        }
    }
}

void CFilesWindow::CloseCurrentPath(HWND parent, BOOL cancel, BOOL detachFS, BOOL newPathIsTheSame,
                                    BOOL isRefresh, BOOL canChangeSourceUID)
{
    CALL_STACK_MESSAGE6("CFilesWindow::CloseCurrentPath(, %d, %d, %d, %d, %d)",
                        cancel, detachFS, newPathIsTheSame, isRefresh, canChangeSourceUID);

    if (Is(ptDisk))
    {
        if (!cancel)
        {
            if (UserWorkedOnThisPath)
            {
                const char* path = GetPath();
                // HICON hIcon = GetFileOrPathIconAux(path, FALSE, TRUE); // we retrieve the icon
                MainWindow->DirHistoryAddPathUnique(0, path, NULL, NULL /*hIcon*/, NULL, NULL);
                if (!newPathIsTheSame)
                    UserWorkedOnThisPath = FALSE;
            }

            if (!newPathIsTheSame)
            {
                // we're leaving the path
                HiddenNames.Clear();  // we release hidden names
                OldSelection.Clear(); // and old selection
            }

            if (!isRefresh && canChangeSourceUID)
                EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

            ReleaseListing();
            SetValidFileData(VALID_DATA_ALL);
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            if (!cancel)
            {
                if (UserWorkedOnThisPath)
                {
                    MainWindow->DirHistoryAddPathUnique(1, GetZIPArchive(), GetZIPPath(), NULL, NULL, NULL);
                    if (!newPathIsTheSame)
                        UserWorkedOnThisPath = FALSE;
                }

                if (!newPathIsTheSame)
                {
                    // we're leaving the path
                    HiddenNames.Clear();  // we release hidden names
                    OldSelection.Clear(); // and old selection
                }

                if (!isRefresh && canChangeSourceUID)
                    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

                // if the archive data seems useful to SalShExtPastedData, we pass them on
                if (SalShExtPastedData.WantData(GetZIPArchive(), GetArchiveDir(), PluginData,
                                                GetZIPArchiveDate(), GetZIPArchiveSize()))
                {
                    SetArchiveDir(NULL);
                    PluginData.Init(NULL, NULL, NULL, NULL, 0);
                }

                ReleaseListing();
                if (GetArchiveDir() != NULL)
                    delete GetArchiveDir();
                SetArchiveDir(NULL);
                SetPluginIface(NULL);
                // a couple more resets for better clarity
                SetZIPArchive("");
                SetZIPPath("");

                SetPanelType(ptDisk); // for security reasons (a disk has no PluginData, etc.)
                SetValidFileData(VALID_DATA_ALL);
            }
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (!cancel)
                {
                    BOOL sendDetachEvent = FALSE;
                    CPluginFSInterfaceEncapsulation* detachedFS = NULL;

                    char buf[MAX_PATH];
                    if (GetPluginFS()->GetCurrentPath(buf))
                    {
                        if (UserWorkedOnThisPath)
                        {
                            MainWindow->DirHistoryAddPathUnique(2, GetPluginFS()->GetPluginFSName(), buf, NULL,
                                                                GetPluginFS()->GetInterface(), GetPluginFS());
                            if (!newPathIsTheSame)
                                UserWorkedOnThisPath = FALSE;
                        }
                        if (!newPathIsTheSame)
                        {
                            // we're leaving the path
                            HiddenNames.Clear();  // we release hidden names
                            OldSelection.Clear(); // and old selection
                        }
                    }

                    if (detachFS) // detaching only -> add to MainWindow->DetachedFS
                    {
                        detachedFS = new CPluginFSInterfaceEncapsulation(GetPluginFS()->GetInterface(),
                                                                         GetPluginFS()->GetDLLName(),
                                                                         GetPluginFS()->GetVersion(),
                                                                         GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                                                         GetPluginFS()->GetPluginInterface(),
                                                                         GetPluginFS()->GetPluginFSName(),
                                                                         GetPluginFS()->GetPluginFSNameIndex(),
                                                                         GetPluginFS()->GetPluginFSCreateTime(),
                                                                         GetPluginFS()->GetChngDrvDuplicateItemIndex(),
                                                                         GetPluginFS()->GetBuiltForVersion());
                        MainWindow->DetachedFSList->Add(detachedFS);
                        if (!MainWindow->DetachedFSList->IsGood())
                        {
                            MainWindow->DetachedFSList->ResetState();
                            delete detachedFS;
                            detachedFS = NULL;
                        }
                        else
                            sendDetachEvent = TRUE; // the call mustn't happen here because the FS isn't detached yet (it's still in the panel)
                    }

                    if (!detachFS) // we're closing the FS; let it deallocate and display final messageboxes
                    {
                        GetPluginFS()->ReleaseObject(parent);
                    }

                    if (!isRefresh && canChangeSourceUID)
                        EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

                    ReleaseListing();
                    delete GetPluginFSDir();
                    SetPluginFSDir(NULL);
                    SetPluginIconsType(pitSimple);

                    if (SimplePluginIcons != NULL)
                    {
                        delete SimplePluginIcons;
                        SimplePluginIcons = NULL;
                    }

                    if (!detachFS) // we're closing the FS
                    {
                        CPluginInterfaceForFSEncapsulation plugin(GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                                                  GetPluginFS()->GetPluginInterfaceForFS()->GetBuiltForVersion());
                        if (plugin.NotEmpty())
                            plugin.CloseFS(GetPluginFS()->GetInterface());
                        else
                            TRACE_E("Unexpected situation (2) in CFilesWindow::CloseCurrentPath()");
                    }
                    SetPluginFS(NULL, NULL, NULL, NULL, NULL, NULL, -1, 0, 0, 0);
                    SetPluginIface(NULL);

                    SetPanelType(ptDisk); // for security reasons (a disk has no PluginData, etc.)
                    SetValidFileData(VALID_DATA_ALL);

                    if (sendDetachEvent && detachedFS != NULL /* always true */)
                        detachedFS->Event(FSE_DETACHED, GetPanelCode()); // we send notification about successful plugin detachment
                }
                else
                    GetPluginFS()->Event(FSE_CLOSEORDETACHCANCELED, GetPanelCode());
            }
            else
                TRACE_E("Unexpected situation (1) in CFilesWindow::CloseCurrentPath()");
        }
    }
}

void CFilesWindow::RefreshPathHistoryData()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RefreshPathHistoryData()");

    int index = GetCaretIndex();
    if (index >= 0 && index < Files->Count + Dirs->Count) // bounds check to prevent data inconsistency
    {
        int topIndex = ListBox->GetTopIndex();
        CFileData* file = index < Dirs->Count ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

        // we try to record a new top-index and focus-name
        if (Is(ptZIPArchive))
        {
            PathHistory->ChangeActualPathData(1, GetZIPArchive(), GetZIPPath(), NULL, NULL, topIndex, file->Name);
        }
        else
        {
            if (Is(ptDisk))
            {
                PathHistory->ChangeActualPathData(0, GetPath(), NULL, NULL, NULL, topIndex, file->Name);
            }
            else
            {
                if (Is(ptPluginFS))
                {
                    char curPath[MAX_PATH];
                    if (GetPluginFS()->NotEmpty() && GetPluginFS()->GetCurrentPath(curPath))
                    {
                        PathHistory->ChangeActualPathData(2, GetPluginFS()->GetPluginFSName(), curPath,
                                                          GetPluginFS()->GetInterface(), GetPluginFS(),
                                                          topIndex, file->Name);
                    }
                }
            }
        }
    }
}

void CFilesWindow::RemoveCurrentPathFromHistory()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RemoveCurrentPathFromHistory()");

    if (Is(ptZIPArchive))
    {
        PathHistory->RemoveActualPath(1, GetZIPArchive(), GetZIPPath(), NULL, NULL);
    }
    else
    {
        if (Is(ptDisk))
        {
            PathHistory->RemoveActualPath(0, GetPath(), NULL, NULL, NULL);
        }
        else
        {
            if (Is(ptPluginFS))
            {
                char curPath[MAX_PATH];
                if (GetPluginFS()->NotEmpty() && GetPluginFS()->GetCurrentPath(curPath))
                {
                    PathHistory->RemoveActualPath(2, GetPluginFS()->GetPluginFSName(), curPath,
                                                  GetPluginFS()->GetInterface(), GetPluginFS());
                }
            }
        }
    }
}

void CFilesWindow::InvalidateChangesInPanelWeHaveNewListing()
{
    NeedRefreshAfterEndOfSM = FALSE;
    NeedRefreshAfterIconsReading = FALSE;
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    RefreshAfterEndOfSMTime = MyTimeCounter++;
    RefreshAfterIconsReadingTime = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    PluginFSNeedRefreshAfterEndOfSM = FALSE;
    NeedIconOvrRefreshAfterIconsReading = FALSE;
    if (IconOvrRefreshTimerSet)
    {
        KillTimer(HWindow, IDT_ICONOVRREFRESH);
        IconOvrRefreshTimerSet = FALSE;
    }
    if (InactiveRefreshTimerSet)
    {
        //    TRACE_I("Have new listing, so killing INACTIVEREFRESH timer");
        KillTimer(HWindow, IDT_INACTIVEREFRESH);
        InactiveRefreshTimerSet = FALSE;
    }
}

BOOL CFilesWindow::ChangePathToDisk(HWND parent, const char* path, int suggestedTopIndex,
                                    const char* suggestedFocusName, BOOL* noChange,
                                    BOOL refreshListBox, BOOL canForce, BOOL isRefresh, int* failReason,
                                    BOOL shorterPathWarning, int tryCloseReason)
{
    CALL_STACK_MESSAGE9("CFilesWindow::ChangePathToDisk(, %s, %d, %s, , %d, %d, %d, , %d, %d)", path,
                        suggestedTopIndex, suggestedFocusName, refreshListBox, canForce, isRefresh,
                        shorterPathWarning, tryCloseReason);

    //TRACE_I("change-to-disk: begin");

    if (strlen(path) >= MAX_PATH - 2)
    {
        SalMessageBox(parent, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }

    // we make backup copies
    char backup[MAX_PATH];
    lstrcpyn(backup, path, MAX_PATH); // must be done before UpdateDefaultDir (it may point to DefaultDir[])
    char backup2[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup2, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup2;
    }

    // restore panel state info (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    // restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // if it's a relative path convert it to absolute
    int errTextID;
    //  if (!SalGetFullName(backup, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // for the FTP plugin - relative path in "target panel path" during connect
    {
        SalMessageBox(parent, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    path = backup;

    // start the waiting cursor
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // is it already waiting?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // no refreshes please -> they would cause recursion

    //  BOOL firstRun = TRUE;    // commented out because forceUpdate is disabled
    BOOL fixedDrive = FALSE;
    BOOL canTryUserRescuePath = FALSE; // allows using Configuration.IfPathIsInaccessibleGoTo right before the fixed-drive path
    BOOL openIfPathIsInaccessibleGoToCfg = FALSE;
    char ifPathIsInaccessibleGoTo[MAX_PATH];
    GetIfPathIsInaccessibleGoTo(ifPathIsInaccessibleGoTo);
    if ((ifPathIsInaccessibleGoTo[0] == '\\' && ifPathIsInaccessibleGoTo[1] == '\\' ||
         ifPathIsInaccessibleGoTo[0] != 0 && ifPathIsInaccessibleGoTo[1] == ':') &&
        !IsTheSamePath(path, ifPathIsInaccessibleGoTo))
    {
        canTryUserRescuePath = TRUE;
    }
    BOOL closeCalled = FALSE;
    // when changing within the same drive (archives included) we'll find a valid directory
    // even if it means switching to a "fixed-drive"
    BOOL forceUpdateInt = (Is(ptDisk) || Is(ptZIPArchive)) && HasTheSameRootPath(GetPath(), path);
    BOOL detachFS;
    if (PrepareCloseCurrentPath(parent, canForce, TRUE, detachFS, tryCloseReason))
    { // change within "ptDisk" or we can close the current path, we try to open a new one
        char changedPath[MAX_PATH];
        strcpy(changedPath, path);
        BOOL tryNet = !CriticalShutdown && ((!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath()));

    _TRY_AGAIN:

        DWORD err, lastErr;
        BOOL pathInvalid, cut;
        SalCheckAndRestorePathWithCut(parent, changedPath, tryNet, err, lastErr, pathInvalid, cut, FALSE);
        if (cut)
        { // invalidate proposed listbox settings (we'll list a different path)
            suggestedTopIndex = -1;
            suggestedFocusName = NULL;
        }

        if (!pathInvalid && err == ERROR_SUCCESS)
        {
            /*    // commented optimization for cases when the new path matches the old one -> unusual for disks...
      if (!forceUpdate && firstRun && Is(ptDisk) && IsTheSamePath(changedPath, GetPath()))
      {  // no reason to change the path
        CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE);  // "cancel" - remain on the current path
        EndStopRefresh();
        if (setWait) SetCursor(oldCur);
        if (IsTheSamePath(path, GetPath()))
        {
          return TRUE; // the new path matches the current path, nothing to do
        }
        else
        {
          // shortened path matches the current path
          // occurs for example when attempting to enter an inaccessible directory (immediate return)
          CheckPath(TRUE, path, lastErr, TRUE, parent);  // report the error that caused path shortening
          return FALSE;  // the requested path is not accessible
        }
      }
      firstRun = FALSE;
*/
            BOOL updateIcon;
            updateIcon = !Is(ptDisk) || // simple because forceUpdate is commented out
                         !HasTheSameRootPath(changedPath, GetPath());
            //      updateIcon = forceUpdate || !Is(ptDisk) || !HasTheSameRootPath(changedPath, GetPath());

            if (UseSystemIcons || UseThumbnails)
                SleepIconCacheThread();

            if (!closeCalled)
            { // executed only during the first pass, so we can use "Is(ptDisk)" and "GetPath()"
                BOOL samePath = (Is(ptDisk) && IsTheSamePath(GetPath(), changedPath));
                BOOL oldCanAddToDirHistory;
                if (samePath)
                {
                    // we won't record the closed path (to avoid adding it just because of a change-dir to the same path)
                    oldCanAddToDirHistory = MainWindow->CanAddToDirHistory;
                    MainWindow->CanAddToDirHistory = FALSE;
                }

                CloseCurrentPath(parent, FALSE, detachFS, samePath, isRefresh, !samePath); // success, we switch to the new path

                if (samePath)
                {
                    // return to the original path-history mode
                    MainWindow->CanAddToDirHistory = oldCanAddToDirHistory;
                }

                // we hide the throbber and security icon; we don't need them on a disk
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                closeCalled = TRUE;
            }
            //--- set the panel to a disk path
            SetPanelType(ptDisk);
            SetPath(changedPath);
            if (updateIcon ||
                !GetNetworkDrive()) // to ensure icons display correctly when switching to a mounted-volume (doesn't slow on local, so hoppefully no issues)
            {
                UpdateDriveIcon(FALSE);
            }
            if (noChange != NULL)
                *noChange = FALSE; // the listing was cleared
            forceUpdateInt = TRUE; // so now something must be able to load (otherwise the panel will remain empty)

            // we'll let the new path load its contents
            BOOL cannotList;
            cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
            if (isRefresh && !cannotList && GetMonitorChanges() && !AutomaticRefresh)
            {                                                                                                                // auto-refresh failure; we verify whether the directory displayed in the panel is being deleted (happened to me while deleting through the network from another machine) ... If ignored, the panel will never refresh (because auto-refresh is broken)
                Sleep(400);                                                                                                  // we take a break, so the deletion can proceed (so the directory becomes deleted enough to become unlistable)
                                                                                                                             //        TRACE_I("Calling CommonRefresh again... (unable to receive change notifications, first listing was OK, but maybe current directory is being deleted)");
                cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh); // repeat the listing; this one should fail
            }
            if (cannotList)
            { // the selected path can't be listed ("access denied" or low_memory) or it was alreadydeleted

            FIXED_DRIVE:

                BOOL change = FALSE;
                if (fixedDrive || !CutDirectory(changedPath)) // we attempt to shorten the path
                {
                    if (canTryUserRescuePath) // first we try the "rescue path" user wished for
                    {
                        canTryUserRescuePath = FALSE; // we won't try it more than once
                        openIfPathIsInaccessibleGoToCfg = TRUE;
                        fixedDrive = FALSE; // we'll allow switching to a fixed-drive (perhaps it was tried already but the user path had priority)
                        GetIfPathIsInaccessibleGoTo(changedPath);
                        shorterPathWarning = TRUE; // we want to see errors for the "rescue" path
                        change = TRUE;
                    }
                    else
                    {
                        if (openIfPathIsInaccessibleGoToCfg)
                            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;

                        // cannot shorten, we find the system or first fixed-drive (our "escape drive")
                        char sysDir[MAX_PATH];
                        char root[4] = " :\\";
                        BOOL done = FALSE;
                        if (GetWindowsDirectory(sysDir, MAX_PATH) != 0 && sysDir[0] != 0 && sysDir[1] == ':')
                        {
                            root[0] = sysDir[0];
                            if (GetDriveType(root) == DRIVE_FIXED)
                                done = TRUE;
                        }
                        if (!done)
                        {
                            DWORD disks = GetLogicalDrives();
                            disks >>= 2; // skip A: and B:, during floppy formatting they sometimes become DRIVE_FIXED
                            char d = 'C';
                            while (d <= 'Z')
                            {
                                if (disks & 1)
                                {
                                    root[0] = d;
                                    if (GetDriveType(root) == DRIVE_FIXED)
                                        break; // we have our "escape drive"
                                }
                                disks >>= 1;
                                d++;
                            }
                            if (d <= 'Z')
                                done = TRUE; // our "escape drive" was found
                        }
                        if (done)
                        {
                            if (LowerCase[root[0]] != LowerCase[changedPath[0]]) // prevention againts an infinite loop
                            {                                                    // UNC or another disk (like "c:\")
                                strcpy(changedPath, root);                       // we'll try our "escape drive"
                                change = TRUE;
                            }
                        }
                    }
                }
                else
                    change = TRUE; // shortened path

                if (change) // only for a new path; otherwise leave the panel empty (shouldn't happen, fixed drive is our safety net)
                {
                    // invalidate proposed listbox settings (we'll list a different path)
                    suggestedTopIndex = -1;
                    suggestedFocusName = NULL;
                    // the "new" path is changing (UserWorkedOnThisPath==TRUE may have survived in CloseCurrentPath)
                    UserWorkedOnThisPath = FALSE;
                    // we try listing the adjusted path
                    goto _TRY_AGAIN;
                }
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
            }
            else
            {
                // we just received a new listing; if there are any reported panel changes, we cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                if (lastErr != ERROR_SUCCESS && (!isRefresh || openIfPathIsInaccessibleGoToCfg) && shorterPathWarning)
                {                        // if it's not a refresh and messages about path-shortening are supposed to be shown ...
                    if (!refreshListBox) // we'll display a message; we must perform refresh-list-box
                    {
                        RefreshListBox(0, -1, -1, FALSE, FALSE);
                    }
                    // we report the error that caused the path to be shortened
                    char errBuf[2 * MAX_PATH + 100];
                    sprintf(errBuf, LoadStr(IDS_PATHERRORFORMAT),
                            openIfPathIsInaccessibleGoToCfg ? ifPathIsInaccessibleGoTo : path,
                            GetErrorText(lastErr));
                    SalMessageBox(parent, errBuf, LoadStr(IDS_ERRORCHANGINGDIR),
                                  MB_OK | MB_ICONEXCLAMATION);
                    if (openIfPathIsInaccessibleGoToCfg)
                        OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;
                }
                if (failReason != NULL)
                    *failReason = CHPPFR_SUCCESS;
            }
            //---  restore DefaultDir
            MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);
        }
        else
        {
            if (err == ERROR_NOT_READY) // if the drive isn't ready (removable media)
            {
                char text[100 + MAX_PATH];
                char drive[MAX_PATH];
                UINT drvType;
                if (changedPath[0] == '\\' && changedPath[1] == '\\')
                {
                    drvType = DRIVE_REMOTE;
                    GetRootPath(drive, changedPath);
                    drive[strlen(drive) - 1] = 0; // drop the trailing '\\'
                }
                else
                {
                    drive[0] = changedPath[0];
                    drive[1] = 0;
                    drvType = MyGetDriveType(changedPath);
                }
                if (drvType != DRIVE_REMOTE)
                {
                    GetCurrentLocalReparsePoint(changedPath, CheckPathRootWithRetryMsgBox);
                    if (strlen(CheckPathRootWithRetryMsgBox) > 3)
                    {
                        lstrcpyn(drive, CheckPathRootWithRetryMsgBox, MAX_PATH);
                        SalPathRemoveBackslash(drive);
                    }
                }
                else
                    GetRootPath(CheckPathRootWithRetryMsgBox, changedPath);
                sprintf(text, LoadStr(IDS_NODISKINDRIVE), drive);
                int msgboxRes = (int)CDriveSelectErrDlg(parent, text, changedPath).Execute();
                if (msgboxRes == IDCANCEL && CutDirectory(CheckPathRootWithRetryMsgBox))
                { // to allow entering the root when a volume is mounted (F:\DRIVE_CD -> F:\)
                    lstrcpyn(changedPath, CheckPathRootWithRetryMsgBox, MAX_PATH);
                    msgboxRes = IDRETRY;
                }
                CheckPathRootWithRetryMsgBox[0] = 0;
                UpdateWindow(MainWindow->HWindow);
                if (msgboxRes == IDRETRY)
                    goto _TRY_AGAIN;
            }
            else
            {
                if (!pathInvalid &&               // the user already knows the UNC path couldn't be revived
                    err != ERROR_USER_TERMINATED) // the user also knows about the abort (ESC)
                {
                    CheckPath(TRUE, changedPath, err, TRUE, parent); // other errors - just display the message
                }
            }

            if (forceUpdateInt && !fixedDrive) // if an update is needed, try also the fixed drive
            {
                fixedDrive = TRUE; // prevention of looping + switch to the fixed drive
                goto FIXED_DRIVE;
            }
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
        }

        if (!closeCalled)
            CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, stay on the original path
    }
    else
    {
        if (failReason != NULL)
            *failReason = CHPPFR_CANNOTCLOSEPATH;
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);
    BOOL ret = Is(ptDisk) && IsTheSamePath(GetPath(), path);
    if (!ret && failReason != NULL && *failReason == CHPPFR_SUCCESS)
    {
        *failReason = CHPPFR_SHORTERPATH;
    }
    //TRACE_I("change-to-disk: end");
    return ret;
}

BOOL CFilesWindow::ChangePathToArchive(const char* archive, const char* archivePath,
                                       int suggestedTopIndex, const char* suggestedFocusName,
                                       BOOL forceUpdate, BOOL* noChange, BOOL refreshListBox,
                                       int* failReason, BOOL isRefresh, BOOL canFocusFileName,
                                       BOOL isHistory)
{
    CALL_STACK_MESSAGE10("CFilesWindow::ChangePathToArchive(%s, %s, %d, %s, %d, , %d, , %d, %d, %d)",
                         archive, archivePath, suggestedTopIndex, suggestedFocusName,
                         forceUpdate, refreshListBox, isRefresh, canFocusFileName, isHistory);

    // we make backup copies
    char backup1[MAX_PATH];
    lstrcpyn(backup1, archive, MAX_PATH);
    char backup2[MAX_PATH];
    lstrcpyn(backup2, archivePath, MAX_PATH);
    archivePath = backup2;
    char backup3[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup3, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup3;
    }

    // restore panel state info (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // if the archive path is relative, convert it to absolute
    int errTextID;
    //  if (!SalGetFullName(backup1, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup1, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // consistent with ChangePathToDisk()
    {
        SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    archive = backup1;

    //---  start the waiting cursor
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // is it already waiting?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // no refreshes, please

    BOOL nullFile;         // TRUE if the archive is an empty file (size==0)
    FILETIME archiveDate;  // date and time of the archive file
    CQuadWord archiveSize; // size of the archive file

    char text[MAX_PATH + 500];
    char path[MAX_PATH];
    BOOL sameArch;
    BOOL checkPath = TRUE;
    BOOL forceUpdateInt = FALSE; // is path change required? (possibly even to disk)
    BOOL tryPathWithArchiveOnError = isHistory;
    if (!Is(ptZIPArchive) || StrICmp(GetZIPArchive(), archive) != 0) // not the archive or a different archive
    {

    _REOPEN_ARCHIVE:

        sameArch = FALSE;
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // the current path can be closed, try to open a new one
            // verify accessibility of the path containing the archive
            strcpy(path, archive);
            if (!CutDirectory(path, NULL))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangePathToArchive.");
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                tryPathWithArchiveOnError = FALSE; // meaningless error, ignore it

            ERROR_1:

                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, stay on the original path

                if (forceUpdateInt) // a path change is required; opening the archive failed, go back to disk
                {                   // we're certainly in an archive (it's a panel refresh of an archive)
                    // if possible, exit the archive (possibly all the way to the "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else
                {
                    if (tryPathWithArchiveOnError) // try changing to a path as close to the archive as possible
                        ChangePathToDisk(HWindow, path, -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);

                return FALSE;
            }

            // we skip testing network paths if we just accessed them
            BOOL tryNet = (!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath());
            DWORD err, lastErr;
            BOOL pathInvalid, cut;
            if (!SalCheckAndRestorePathWithCut(HWindow, path, tryNet, err, lastErr, pathInvalid, cut, FALSE) ||
                cut)
            { // path isn't accessible or it is truncated (the archive cannot be opened)
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                if (tryPathWithArchiveOnError)
                    tryPathWithArchiveOnError = (err == ERROR_SUCCESS && !pathInvalid); // shorter path is accessible, we'll try it
                if (!isRefresh)                                                         // during refresh path-shortening messages are not displayed
                {
                    sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(lastErr));
                    SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                goto ERROR_1;
            }

            if (PackerFormatConfig.PackIsArchive(archive)) // is it an archive?
            {
                // retrieve file info (does it exist?, size, date & time)
                DWORD err2 = NO_ERROR;
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    GetFileTime(file, NULL, NULL, &archiveDate);
                    SalGetFileSize(file, archiveSize, err2); // does it return "success"? - ignore, 'err2' is checked later
                    nullFile = archiveSize == CQuadWord(0, 0);
                    HANDLES(CloseHandle(file));
                }
                else
                    err2 = GetLastError();

                if (err2 != NO_ERROR)
                {
                    if (!isRefresh) // during refresh missing-path messages are not displayed
                        DialogError(HWindow, BUTTONS_OK, archive, GetErrorText(err2), LoadStr(IDS_ERROROPENINGFILE));
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDPATH;
                    goto ERROR_1; // error
                }

                CSalamanderDirectory* newArchiveDir = new CSalamanderDirectory(FALSE);

                // apply optimized adding to 'newArchiveDir'
                newArchiveDir->AllocAddCache();

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                CPluginDataInterfaceAbstract* pluginData = NULL;
                CPluginData* plugin = NULL;
                if (!nullFile)
                    CreateSafeWaitWindow(LoadStr(IDS_LISTINGARCHIVE), NULL, 2000, FALSE, MainWindow->HWindow);
                if (nullFile || PackList(this, archive, *newArchiveDir, pluginData, plugin))
                {
                    // free the cache so it does not linger in the object
                    newArchiveDir->FreeAddCache();

                    if (!nullFile)
                        DestroySafeWaitWindow();
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    BOOL isTheSamePath = FALSE; // TRUE = the path doesn't change
                    if (Is(ptZIPArchive) && StrICmp(GetZIPArchive(), archive) == 0)
                    {
                        char buf[MAX_PATH];
                        strcpy(buf, *archivePath == '\\' ? archivePath + 1 : archivePath);
                        char* end = buf + strlen(buf);
                        if (end > buf && *(end - 1) == '\\')
                            *--end = 0;

                        if (GetArchiveDir()->SalDirStrCmp(buf, GetZIPPath()) == 0)
                            isTheSamePath = TRUE;
                    }

                    // success, switch to the new path - because listing the archive can be time-consuming
                    // the path changes even if the target path doesn't exist - applies to
                    // Change Directory (Shift+F7) which otherwise wouldn't change it
                    CloseCurrentPath(HWindow, FALSE, detachFS, isTheSamePath, isRefresh, !isTheSamePath);

                    // we just received a new listing; if there are any reported panel changes, we cancel them
                    InvalidateChangesInPanelWeHaveNewListing();

                    // we hide the throbber and security icon; we don't want them in the archive
                    if (DirectoryLine != NULL)
                        DirectoryLine->HideThrobberAndSecurityIcon();

                    SetPanelType(ptZIPArchive);
                    SetPath(path);
                    UpdateDriveIcon(FALSE);
                    SetArchiveDir(newArchiveDir);
                    SetPluginIface(plugin != NULL ? plugin->GetPluginInterface()->GetInterface() : NULL);
                    SetZIPArchive(archive);
                    SetZIPArchiveDate(archiveDate);
                    SetZIPArchiveSize(archiveSize);
                    if (plugin != NULL)
                    {
                        PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                                        plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                    }
                    else
                        PluginData.Init(NULL, NULL, NULL, NULL, 0); // used only by plugins, not by Salamander
                    SetValidFileData(nullFile ? VALID_DATA_ALL_FS_ARC : GetArchiveDir()->GetValidData());
                    checkPath = FALSE;
                    if (noChange != NULL)
                        *noChange = FALSE;
                    // ZIPPath, Files and Dirs are set later once archivePath is set...
                    if (failReason != NULL)
                        *failReason = CHPPFR_SUCCESS;
                }
                else
                {
                    DestroySafeWaitWindow(); // nullFile must be FALSE, so the check is omitted...
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                    TRACE_I("Unable to open file " << archive << ".");
                    delete newArchiveDir;
                    if (failReason != NULL)
                        *failReason = CHPPFR_INVALIDARCHIVE;
                    goto ERROR_1;
                }
            }
            else
            {
                TRACE_I("File " << archive << " is no longer archive file.");
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDARCHIVE;
                goto ERROR_1;
            }
        }
        else // the current path cannot be closed
        {
            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);
            if (failReason != NULL)
                *failReason = CHPPFR_CANNOTCLOSEPATH;
            return FALSE;
        }
    }
    else // already opened archive
    {
        if (forceUpdate) // should we check whether the archive changed?
        {
            DWORD err;
            if ((err = CheckPath(!isRefresh)) == ERROR_SUCCESS) // no need to restore network connections here ...
            {
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, 0, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    SalGetFileSize(file, archiveSize, err);
                    nullFile = archiveSize == CQuadWord(0, 0);
                    FILETIME zipArchiveDate = GetZIPArchiveDate();
                    BOOL change = (err != NO_ERROR ||                                     // unable to retrieve size
                                   !GetFileTime(file, NULL, NULL, &archiveDate) ||        // unable to get date & time
                                   CompareFileTime(&archiveDate, &zipArchiveDate) != 0 || // date & time differ
                                   !IsSameZIPArchiveSize(archiveSize));                   // file size differs
                    HANDLES(CloseHandle(file));

                    if (change) // file changed
                    {
                        if (AssocUsed) // Is anything from the archive being edited?
                        {
                            // notify that there were changes and that editors should be closed
                            char buf[MAX_PATH + 200];
                            sprintf(buf, LoadStr(IDS_ARCHIVEREFRESHEDIT), GetZIPArchive());
                            SalMessageBox(HWindow, buf, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                        }
                        forceUpdateInt = TRUE; // nowhere to return, path change required (possibly back to disk)
                        goto _REOPEN_ARCHIVE;
                    }
                }
                else
                {
                    err = GetLastError(); // unable to open the archive file
                    if (!isRefresh)       // during refresh missing-path messages are not displayed
                    {
                        sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(err));
                        SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
            if (err != ERROR_SUCCESS) // switch to an existing path
            {
                if (err != ERROR_USER_TERMINATED)
                {
                    // if possible, exit the archive (possibly all the way to the "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else // user pressed ESC -> the path is probably inaccessible, we go straight to the "fixed-drive"
                {
                    ChangeToRescuePathOrFixedDrive(HWindow, noChange, refreshListBox);
                }

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                return FALSE;
            }
        }
        if (failReason != NULL)
            *failReason = CHPPFR_SUCCESS;
        sameArch = TRUE;
    }

    // we find a path in the archive that still exists (original or shortened)
    strcpy(path, *archivePath == '\\' ? archivePath + 1 : archivePath);
    char* end = path + strlen(path);
    if (end > path && *(end - 1) == '\\')
        *--end = 0;

    if (sameArch && GetArchiveDir()->SalDirStrCmp(path, GetZIPPath()) == 0) // the new path matches the current one
    {
        // call CommonRefresh so 'suggestedTopIndex' and 'suggestedFocusName' aren't ignored
        CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        return TRUE; // nothing to do
    }

    // save the current path in the archive
    char currentPath[MAX_PATH];
    strcpy(currentPath, GetZIPPath());

    SetZIPPath(path);
    BOOL ok = TRUE;
    char* fileName = NULL;
    BOOL useFileName = FALSE;
    while (path[0] != 0 && GetArchiveDirFiles() == NULL)
    {
        end = strrchr(path, '\\');
        useFileName = (canFocusFileName && suggestedFocusName == NULL && fileName == NULL); // allow focusing the file + no external focus + only for the first shortening
        if (end != NULL)
        {
            *end = 0;
            fileName = end + 1;
        }
        else
        {
            memmove(path + 1, path, strlen(path) + 1);
            fileName = path + 1;
            path[0] = 0;
        }
        SetZIPPath(path);
        ok = FALSE;

        if (!sameArch)
        {
            // the "new" path is changing (UserWorkedOnThisPath==TRUE may have survived in CloseCurrentPath)
            UserWorkedOnThisPath = FALSE;
        }
    }

    if (!useFileName && sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) == 0) // we're not focusing a file and the shortened path matches the current one
    {                                                                                              // occurs for example when attempting to enter an inaccessible directory (immediate return)
        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
        return FALSE; // the requested path is not accessible
    }

    if (!ok)
    {
        // we invalidate proposed listbox settings (we'll list a different path)
        suggestedTopIndex = -1;
        suggestedFocusName = NULL;
        if (failReason != NULL)
            *failReason = useFileName ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
    }

    // must succeed (at least the archive root always exists)
    CommonRefresh(HWindow, suggestedTopIndex, useFileName ? fileName : suggestedFocusName,
                  refreshListBox, TRUE, isRefresh);

    if (refreshListBox && !ok && useFileName && GetCaretIndex() == 0)
    { // attempt to focus a file name failed -> it wasn't a file name
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);

    // we add the path we just left (paths inside the archive don't close,
    // so DirHistoryAddPathUnique wasn't called yet) + only if it's not the current
    // path (happens only when focusing a file)
    if (sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) != 0)
    {
        if (UserWorkedOnThisPath)
        {
            MainWindow->DirHistoryAddPathUnique(1, GetZIPArchive(), currentPath, NULL, NULL, NULL);
            UserWorkedOnThisPath = FALSE;
        }

        // we're leaving the path
        HiddenNames.Clear();  // release hidden names
        OldSelection.Clear(); // and old selection
    }

    return ok;
}

BOOL CFilesWindow::ChangeAndListPathOnFS(const char* fsName, int fsNameIndex, const char* fsUserPart,
                                         CPluginFSInterfaceEncapsulation& pluginFS, CSalamanderDirectory* dir,
                                         CPluginDataInterfaceAbstract*& pluginData, BOOL& shorterPath,
                                         int& pluginIconsType, int mode, BOOL firstCall,
                                         BOOL* cancel, const char* currentPath, int currentPathFSNameIndex,
                                         BOOL forceUpdate, char* cutFileName, BOOL* keepOldListing)
{
    CALL_STACK_MESSAGE10("CFilesWindow::ChangeAndListPathOnFS(%s, %d, %s, , , , , , %d, %d, , %s, %d, %d, , %d)",
                         fsName, fsNameIndex, fsUserPart, mode, firstCall, currentPath, currentPathFSNameIndex,
                         forceUpdate, (keepOldListing != NULL && *keepOldListing));
    if (cutFileName != NULL)
        *cutFileName = 0;
    char bufFSUserPart[MAX_PATH];
    const char* origUserPart; // user-part to which we switch the path to
    int origFSNameIndex;
    if (fsUserPart == NULL) // detached FS, restoration of the listing...
    {
        if (!pluginFS.GetCurrentPath(bufFSUserPart))
        {
            TRACE_E("Unable to get current path from detached FS.");
            return FALSE;
        }
        origUserPart = bufFSUserPart;
        origFSNameIndex = pluginFS.GetPluginFSNameIndex();
    }
    else
    {
        origUserPart = fsUserPart;
        origFSNameIndex = fsNameIndex;
    }

    CSalamanderDirectory* workDir = dir;
    if (keepOldListing != NULL && *keepOldListing)
    {
        workDir = new CSalamanderDirectory(TRUE);
        if (workDir == NULL) // out of memory -> release the listing (the panel will be empty)
        {
            *keepOldListing = FALSE;
            workDir = dir;

            if (!firstCall)
            {
                // release listing data in the panel
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // assuming 'dir' is PluginFSDir
                workDir = dir = GetPluginFSDir(); // ReleaseListing() may only detach (see OnlyDetachFSListing)

                // secure the listbox from errors caused by the redraw request (we just cut the data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // If WM_USER_UPDATEPANEL is delivered the panel content and scrollbars will repaint.
                // The message loop may deliver it while a message box is created; otherwise the panel
                // remains unchanged and the message is removed from the queue.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
        }
    }
    else
    {
        if (!firstCall)
        {
            workDir->Clear(NULL); // unnecessary (should be empty), just to be safe
        }
    }

    BOOL ok = FALSE;
    char user[MAX_PATH];
    lstrcpyn(user, origUserPart, MAX_PATH);
    pluginData = NULL;
    shorterPath = FALSE;
    if (cancel != NULL)
        *cancel = FALSE; // new data
    // we will try to read the directory contents (the path may shorten progressively)
    BOOL useCutFileName = TRUE;
    char fsNameBuf[MAX_PATH];
    fsNameBuf[0] = 0;
    while (1)
    {
        if (cutFileName != NULL && *cutFileName != 0)
            useCutFileName = FALSE;
        BOOL pathWasCut = FALSE;

        char newFSName[MAX_PATH];
        lstrcpyn(newFSName, fsName, MAX_PATH);
        BOOL changePathRet = pluginFS.ChangePath(pluginFS.GetPluginFSNameIndex(), newFSName,
                                                 fsNameIndex, user, cutFileName,
                                                 cutFileName != NULL ? &pathWasCut : NULL,
                                                 forceUpdate, mode);
        if (changePathRet) // ChangePath doesn't return an error
        {
            if (StrICmp(newFSName, fsName) != 0) // fs-name change, verify the new fs-name
            {
                BOOL ok2 = FALSE;
                int index;
                int newFSNameIndex;
                if (Plugins.IsPluginFS(newFSName, index, newFSNameIndex))
                {
                    CPluginData* plugin = Plugins.Get(index);
                    if (plugin != NULL)
                    {
                        if (plugin->GetPluginInterface()->GetInterface() == pluginFS.GetPluginInterface())
                            ok2 = TRUE;
                        else
                            TRACE_E("CFilesWindow::ChangeAndListPathOnFS(): pluginFS.ChangePath() returned fs-name "
                                    "("
                                    << newFSName << ") from other plugin: " << plugin->DLLName);
                    }
                    else
                        TRACE_E("Second unexpected situation in CFilesWindow::ChangeAndListPathOnFS()");
                }
                else
                    TRACE_E("CFilesWindow::ChangeAndListPathOnFS(): pluginFS.ChangePath() returned unknown fs-name: " << newFSName);
                if (!ok2)
                    changePathRet = FALSE; // fs-name change failed; simulate a fatal error on the FS
                else                       // start using the new FS name (for the next loop pass)
                {
                    lstrcpyn(fsNameBuf, newFSName, MAX_PATH);
                    fsName = fsNameBuf;
                    fsNameIndex = newFSNameIndex;
                }
            }
            if (changePathRet) // store the used fs-name in 'pluginFS'
                pluginFS.SetPluginFS(fsName, fsNameIndex);
        }

        if (changePathRet)
        { // the path looks OK
            if (pathWasCut && cutFileName != NULL && *cutFileName == 0)
                useCutFileName = FALSE;
            if (firstCall) // path change within the FS, original listing is not released (would it suffice?)
            {
                if (!forceUpdate && currentPath != NULL &&
                    pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(),
                                           currentPathFSNameIndex, currentPath)) // path shortened back to the original path
                {                                                                // no reason to change the path, the original listing is enough
                    shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                    if (cancel != NULL)
                        *cancel = TRUE;                     // the original data was kept
                    pluginIconsType = GetPluginIconsType(); // unnecessary (won't be used) but remains the same
                    ok = TRUE;
                    break;
                }

                if (keepOldListing == NULL || !*keepOldListing) // not dead-code (used when allocating workDir fails)
                {
                    // release listing data in the panel
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();                 // assuming 'dir' is PluginFSDir
                    workDir = dir = GetPluginFSDir(); // ReleaseListing() may only detach (see OnlyDetachFSListing)

                    // secure the listbox from errors caused by the redraw request (we just cut the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // If WM_USER_UPDATEPANEL is delivered the panel content and scrollbars will repaint.
                    // The message loop may deliver it while a message box is created; otherwise the panel
                    // remains unchanged and the message is removed from the queue.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                firstCall = FALSE;
            }

            // attempt to list files and directories from the current path
            if (pluginFS.ListCurrentPath(workDir, pluginData, pluginIconsType, forceUpdate)) // succeeded ...
            {
                if (keepOldListing != NULL && *keepOldListing) // we already have the new listing; discard the old one
                {
                    // release listing data in the panel
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();       // we're assuming 'dir' is PluginFSDir
                    dir = GetPluginFSDir(); // in ReleaseListing() we may only detach (see OnlyDetachFSListing)

                    // secure the listbox from errors caused by the redraw request (we just cut the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // If WM_USER_UPDATEPANEL is delivered the panel content and scrollbars will repaint.
                    // The message loop may deliver it while a message box is created; otherwise the panel
                    // remains unchanged and the message is removed from the queue.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);

                    SetPluginFSDir(workDir); // we set the new listing
                    delete dir;
                    dir = workDir;
                }

                if (pluginIconsType != pitSimple &&
                    pluginIconsType != pitFromRegistry &&
                    pluginIconsType != pitFromPlugin) // verify it matches an allowed value
                {
                    TRACE_E("Invalid plugin-icons-type!");
                    pluginIconsType = pitSimple;
                }
                if (pluginIconsType == pitFromPlugin && pluginData == NULL) // not allowed, degrade the type
                {
                    TRACE_E("Plugin-icons-type is pitFromPlugin and plugin-data is NULL!");
                    pluginIconsType = pitSimple;
                }
                shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                ok = TRUE;
                break;
            }
            // we prepare dir for further use (release leftovers if the plugin left any)
            workDir->Clear(NULL);
            // path isn't o.k.; we'll try shortening it in the next cycle pass
            if (!pluginFS.GetCurrentPath(user))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangeAndListPathOnFS()");
                break;
            }
        }
        else // fatal error, abort
        {
            TRACE_I("Unable to open FS path " << fsName << ":" << origUserPart);

            if (firstCall && (keepOldListing == NULL || !*keepOldListing)) // not dead-code (used when allocating workDir fails)
            {
                // release listing data in the panel
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // we're assuming 'dir' is PluginFSDir
                workDir = dir = GetPluginFSDir(); // ReleaseListing() may only detach (see OnlyDetachFSListing)

                // secure the listbox from errors caused by the redraw request (we just cut the data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // If WM_USER_UPDATEPANEL is delivered the panel content and scrollbars will repaint.
                // The message loop may deliver it while a message box is created; otherwise the panel
                // remains unchanged and the message is removed from the queue.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
            useCutFileName = FALSE;
            break;
        }
    }

    if (dir != workDir)
        delete workDir; // 'workDir' wasn't used, free it

    // we try to find a file to focus in the FS listing - not perfect when the file is hidden
    // from the panel listing (e.g., "don't show hidden files" or filters) because it cannot
    // be focused and the user won't know about this "error" - but it's probably
    // not really an error if the file does exist, so we ignore it (same as with
    // disk paths)...
    if (ok && useCutFileName && cutFileName != NULL && *cutFileName != 0)
    {
        CFilesArray* files = dir->GetFiles("");
        unsigned cutFileNameLen = (int)strlen(cutFileName);
        int count = files->Count;
        int i;
        for (i = 0; i < count; i++)
        {
            CFileData* f = &(files->At(i));
            if (cutFileNameLen == f->NameLen &&
                StrICmpEx(f->Name, cutFileNameLen, cutFileName, cutFileNameLen) == 0)
                break;
        }
        if (i == count) // report error (the file to focus was not found)
        {
            char errText[MAX_PATH + 200];
            sprintf(errText, LoadStr(IDS_UNABLETOFOCUSFILEONFS), cutFileName);
            SalMessageBox(HWindow, errText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            *cutFileName = 0;
        }
    }

    if (!useCutFileName && cutFileName != NULL)
        *cutFileName = 0; // we do not want to use it -> reset the value
    return ok;
}

BOOL CFilesWindow::ChangePathToPluginFS(const char* fsName, const char* fsUserPart, int suggestedTopIndex,
                                        const char* suggestedFocusName, BOOL forceUpdate, int mode,
                                        BOOL* noChange, BOOL refreshListBox, int* failReason, BOOL isRefresh,
                                        BOOL canFocusFileName, BOOL convertPathToInternal)
{
    CALL_STACK_MESSAGE11("CFilesWindow::ChangePathToPluginFS(%s, %s, %d, %s, %d, %d, , %d, , %d, %d, %d)",
                         fsName, fsUserPart, suggestedTopIndex, suggestedFocusName, forceUpdate,
                         mode, refreshListBox, isRefresh, canFocusFileName, convertPathToInternal);
    //TRACE_I("change-to-fs: begin");

    // as a precaution if fsName points to an unchangeable string (GetPluginFS()->PluginFSName()), we create a backup copy
    char backup[MAX_PATH];
    lstrcpyn(backup, fsName, MAX_PATH);
    fsName = backup;

    if (noChange != NULL)
        *noChange = TRUE;
    if (mode != 3 && canFocusFileName)
    {
        TRACE_E("CFilesWindow::ChangePathToPluginFS() - incorrect use of 'mode' or 'canFocusFileName'.");
        canFocusFileName = FALSE;
    }

    if (strlen(fsUserPart) >= MAX_PATH)
    {
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        MessageBox(HWindow, LoadStr(IDS_TOOLONGPATH), LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
        return FALSE;
    }
    // make backup copies
    char backup2[MAX_PATH];
    lstrcpyn(backup2, fsUserPart, MAX_PATH);
    fsUserPart = backup2;
    char* fsUserPart2 = backup2;
    char backup3[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup3, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup3;
    }

    // restore panel state info (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //---  start the waiting cursor
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // is it already waiting?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // no refreshes, please

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];
    int fsNameIndex;
    if (!Is(ptPluginFS) || !IsPathFromActiveFS(fsName, fsUserPart2, fsNameIndex, convertPathToInternal))
    { // is not FS or the path is from a different FS (even within a single plug-in - one FS name)
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // the current path can be closed, attempt to open the new path
            int index;
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
            if (Plugins.IsPluginFS(fsName, index, fsNameIndex)) // find the plugin index
            {
                // obtain the plug-in containing the FS
                CPluginData* plugin = Plugins.Get(index);
                if (plugin != NULL)
                {
                    // open the new FS
                    // load the plug-in before obtaining DLLName, Version, and plugin interfaces
                    CPluginFSInterfaceAbstract* auxFS = plugin->OpenFS(fsName, fsNameIndex);
                    CPluginFSInterfaceEncapsulation pluginFS(auxFS, plugin->DLLName, plugin->Version,
                                                             plugin->GetPluginInterfaceForFS()->GetInterface(),
                                                             plugin->GetPluginInterface()->GetInterface(),
                                                             fsName, fsNameIndex, -1, 0, plugin->BuiltForVersion);
                    if (pluginFS.NotEmpty())
                    {
                        Plugins.SetWorkingPluginFS(&pluginFS);
                        if (convertPathToInternal) // convert the path to internal format
                            pluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart2);
                        // create a new object for the contents of the current file system path
                        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                        CPluginDataInterfaceAbstract* pluginData;
                        int pluginIconsType;
                        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // focus the file only if no other focus is proposed
                        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, pluginFS, newFSDir, pluginData,
                                                  shorterPath, pluginIconsType, mode, FALSE, NULL, NULL, -1,
                                                  FALSE, cutFileName, NULL))
                        {                    // success, the path (or subpath) was listed
                            if (shorterPath) // subpath?
                            {
                                // invalidate proposed listbox settings (we'll list a different path)
                                suggestedTopIndex = -1;
                                if (cutFileName != NULL && *cutFileName != 0)
                                    suggestedFocusName = cutFileName; // focus the file
                                else
                                    suggestedFocusName = NULL;
                                if (failReason != NULL)
                                {
                                    *failReason = cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
                                }
                            }
                            else
                            {
                                if (failReason != NULL)
                                    *failReason = CHPPFR_SUCCESS;
                            }

                            if (UseSystemIcons || UseThumbnails)
                                SleepIconCacheThread();

                            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, isRefresh, TRUE); // success, switch to the new path

                            // we just received a new listing; if there are any reported panel changes, we cancel them
                            InvalidateChangesInPanelWeHaveNewListing();

                            // we hide the throbber and security icon; if the new filesystem wants them it must re-enable them (e.g., in FSE_OPENED or FSE_PATHCHANGED)
                            if (DirectoryLine != NULL)
                                DirectoryLine->HideThrobberAndSecurityIcon();

                            SetPanelType(ptPluginFS);
                            SetPath(GetPath()); // detach the path from Snooper (stop monitoring changes on Path)
                            SetPluginFS(pluginFS.GetInterface(), plugin->DLLName, plugin->Version,
                                        plugin->GetPluginInterfaceForFS()->GetInterface(),
                                        plugin->GetPluginInterface()->GetInterface(),
                                        pluginFS.GetPluginFSName(), pluginFS.GetPluginFSNameIndex(),
                                        pluginFS.GetPluginFSCreateTime(), pluginFS.GetChngDrvDuplicateItemIndex(),
                                        plugin->BuiltForVersion);
                            SetPluginIface(plugin->GetPluginInterface()->GetInterface());
                            SetPluginFSDir(newFSDir);
                            PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                                            plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
                            SetPluginIconsType(pluginIconsType);
                            SetValidFileData(GetPluginFSDir()->GetValidData());

                            if (noChange != NULL)
                                *noChange = FALSE;

                            // refresh the panel
                            UpdateDriveIcon(FALSE); // get the icon for the current path from the plugin
                            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                            // notify the FS that it is finally opened
                            GetPluginFS()->Event(FSE_OPENED, GetPanelCode());
                            GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());

                            ok = TRUE;
                        }
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                        if (!ok)
                        {
                            delete newFSDir;
                            pluginFS.ReleaseObject(HWindow);
                            plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS.GetInterface());
                        }
                        Plugins.SetWorkingPluginFS(NULL);
                    }
                    else
                        TRACE_I("Plugin has refused to open FS (maybe it even does not start).");
                }
                else
                    TRACE_E("Unexpected situation in CFilesWindow::ChangePathToPluginFS()");
            }
            else
                TRACE_I("Plugin containing file-system name " << fsName << " is no longer available.");

            if (!ok)
                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, stay on the original path

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            //TRACE_I("change-to-fs: end");
            return ok ? !shorterPath : FALSE;
        }
        else // the current path cannot be closed
        {
            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            if (failReason != NULL)
                *failReason = CHPPFR_CANNOTCLOSEPATH;
            return FALSE;
        }
    }
    else
    {
        // note: convertPathToInternal must already be FALSE (the path was converted in IsPathFromActiveFS())

        // PluginFS matches fsName and the path fsUserPart2 can be verified on it
        BOOL samePath = GetPluginFS()->IsCurrentPath(GetPluginFS()->GetPluginFSNameIndex(), fsNameIndex, fsUserPart2);
        if (!forceUpdate && samePath) // the path is identical to the current path
        {
            // call CommonRefresh so 'suggestedTopIndex' and 'suggestedFocusName' aren't ignored
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            if (failReason != NULL)
                *failReason = CHPPFR_SUCCESS;
            //TRACE_I("change-to-fs: end");
            return TRUE; // nothing to do
        }

        // back up the current FS path (we'll try to select it again if an error occurs)
        BOOL currentPathOK = TRUE;
        char currentPath[MAX_PATH];
        if (!GetPluginFS()->GetCurrentPath(currentPath))
            currentPathOK = FALSE;
        char currentPathFSName[MAX_PATH];
        strcpy(currentPathFSName, GetPluginFS()->GetPluginFSName());
        int currentPathFSNameIndex = GetPluginFS()->GetPluginFSNameIndex();

        int originalTopIndex = ListBox->GetTopIndex();
        char originalFocusName[MAX_PATH];
        originalFocusName[0] = 0;
        if (FocusedIndex >= 0)
        {
            CFileData* file = NULL;
            if (FocusedIndex < Dirs->Count)
                file = &Dirs->At(FocusedIndex);
            else
            {
                if (FocusedIndex < Files->Count + Dirs->Count)
                    file = &Files->At(FocusedIndex - Dirs->Count);
            }
            if (file != NULL)
                lstrcpyn(originalFocusName, file->Name, MAX_PATH);
        }

        // attempt to change the path on the current FS
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        BOOL cancel;
        BOOL keepOldListing = TRUE;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // focus the file only if no other focus is proposed
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, *GetPluginFS(), GetPluginFSDir(),
                                  pluginData, shorterPath, pluginIconsType, mode, TRUE, &cancel,
                                  currentPathOK ? currentPath : NULL, currentPathFSNameIndex, forceUpdate,
                                  cutFileName, &keepOldListing))
        { // success, the path (or subpath) was listed
            if (failReason != NULL)
            {
                *failReason = shorterPath ? (cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH) : CHPPFR_SUCCESS;
            }

            if (!cancel) // only if new content was loaded (original content wasn't kept)
            {
                // we just received a new listing; if there are any reported panel changes, we cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                // we hide the throbber and security icon; if the FS still wants them it must enable them again (e.g., in FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // add the path we just left (paths inside the FS remain open,
                // so DirHistoryAddPathUnique hasn't been called yet)
                if (currentPathOK && (!samePath || samePath && shorterPath)) // the path has changed
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // leaving the path
                    HiddenNames.Clear();  // release hidden names
                    OldSelection.Clear(); // and old selection
                }

                if (shorterPath) // subpath?
                {
                    // invalidate proposed listbox settings (we'll list a different path)
                    suggestedTopIndex = -1;
                    if (cutFileName != NULL && *cutFileName != 0)
                        suggestedFocusName = cutFileName; // focus the file
                    else
                    {
                        suggestedFocusName = NULL;

                        // the new path shortened back to the original ("unlistable subdirectory"),
                        // keep topIndex and focusName from before the operation starts (so the user doesn't lose focus)
                        if (currentPathOK &&
                            GetPluginFS()->IsCurrentPath(GetPluginFS()->GetPluginFSNameIndex(),
                                                         currentPathFSNameIndex, currentPath))
                        {
                            suggestedTopIndex = originalTopIndex;
                            suggestedFocusName = originalFocusName[0] == 0 ? NULL : originalFocusName;
                        }
                    }
                }

                //        if (UseSystemIcons || UseThumbnails) SleepIconCacheThread();   // vola se v ChangeAndListPathOnFS

                // add new pluginData to the current (newly filled) PluginFSDir
                PluginData.Init(pluginData, GetPluginFS()->GetDLLName(),
                                GetPluginFS()->GetVersion(), GetPluginFS()->GetPluginInterface(),
                                GetPluginFS()->GetBuiltForVersion());
                SetPluginIconsType(pluginIconsType);
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                SetValidFileData(GetPluginFSDir()->GetValidData());

                if (noChange != NULL)
                    *noChange = FALSE;

                // clean the message queue from buffered WM_USER_UPDATEPANEL
                MSG msg2;
                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                // refresh the panel
                UpdateDriveIcon(FALSE); // get the icon for the current path from the plugin
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                // notify the FS that the path changed
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else
            {
                if (shorterPath && cutFileName != NULL && *cutFileName != 0 && refreshListBox) // the file needs to be focused
                {
                    int focusIndexCase = -1;
                    int focusIndexIgnCase = -1;
                    int i;
                    for (i = 0; i < Dirs->Count; i++)
                    { // for consistency with CommonRefresh we search directories first,
                        // then files (so it behaves the same in both cases)
                        if (StrICmp(Dirs->At(i).Name, cutFileName) == 0)
                        {
                            if (focusIndexIgnCase == -1)
                                focusIndexIgnCase = i;
                            if (strcmp(Dirs->At(i).Name, cutFileName) == 0)
                            {
                                focusIndexCase = i;
                                break;
                            }
                        }
                    }
                    if (i == Dirs->Count)
                    {
                        for (i = 0; i < Files->Count; i++)
                        {
                            if (StrICmp(Files->At(i).Name, cutFileName) == 0)
                            {
                                if (focusIndexIgnCase == -1)
                                    focusIndexIgnCase = i + Dirs->Count;
                                if (strcmp(Files->At(i).Name, cutFileName) == 0)
                                {
                                    focusIndexCase = i + Dirs->Count;
                                    break;
                                }
                            }
                        }
                    }

                    if (focusIndexIgnCase != -1) // at least one file was found with potencial case difference
                    {
                        SetCaretIndex(focusIndexCase != -1 ? focusIndexCase : focusIndexIgnCase, FALSE);
                    }
                }
            }

            ok = TRUE;
        }
        else // requested path is not accessible, try returning to the original path
        {
            if (noChange != NULL)
                *noChange = FALSE; // the listing will be cleared or changed
            if (!samePath &&       // if this isn't a refresh (changing to the same path)
                currentPathOK &&   // if the original path was retrieved successfully
                ChangeAndListPathOnFS(currentPathFSName, currentPathFSNameIndex, currentPath,
                                      *GetPluginFS(), GetPluginFSDir(),
                                      pluginData, shorterPath, pluginIconsType, mode,
                                      FALSE, NULL, NULL, -1, FALSE, NULL, &keepOldListing))
            { // success, the original path (or its subpath) was listed
                // invalidate proposed listbox settings (we'll list a different path)
                suggestedTopIndex = -1;
                suggestedFocusName = NULL;

                // the original path is accessible (no shortening was needed); keep the topIndex
                // and focusName from before the operation (so the user doesn't lose focus)
                if (!shorterPath)
                {
                    suggestedTopIndex = originalTopIndex;
                    suggestedFocusName = originalFocusName[0] == 0 ? NULL : originalFocusName;
                }

                // add the path we just left (paths inside the FS remain open,
                // so DirHistoryAddPathUnique hasn't been called yet)
                if (currentPathOK && shorterPath) // if shorterPath is FALSE, the path didn't change...
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // leaving the path
                    HiddenNames.Clear();  // release hidden names
                    OldSelection.Clear(); // and old selection
                }

                // we just received a new listing; if there are any reported panel changes, we cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                // we hide the throbber and security icon; if the FS still wants them it must enable them again (e.g., in FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                // add new pluginData to the current (newly filled) PluginFSDir
                PluginData.Init(pluginData, GetPluginFS()->GetDLLName(),
                                GetPluginFS()->GetVersion(), GetPluginFS()->GetPluginInterface(),
                                GetPluginFS()->GetBuiltForVersion());
                SetPluginIconsType(pluginIconsType);
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                SetValidFileData(GetPluginFSDir()->GetValidData());

                // clean the message queue from buffered WM_USER_UPDATEPANEL
                MSG msg2;
                PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);

                // refresh the panel
                UpdateDriveIcon(FALSE); // get the icon for the current path from the plugin
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
                if (failReason != NULL)
                    *failReason = mode == 3 ? CHPPFR_INVALIDPATH : CHPPFR_SHORTERPATH; // previously CHPPFR_SHORTERPATH only; but Shift+F7 from dfs:x:\zumpa to dfs:x:\zumpa\aaa just reported a path error and didn't return to the Shift+F7 dialog

                // notify the FS that the path changed
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else // show an empty panel, nothing can be read from the FS, switch to the fixed drive
            {
                if (keepOldListing)
                {
                    // release the old listing
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();
                    ReleaseListing();
                    // secure the listbox from errors caused by the redraw request (we just cut the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                }
                else // not dead-code, see 'workDir' allocation error in ChangeAndListPathOnFS()
                {
                    // clean the message queue from buffered WM_USER_UPDATEPANEL
                    MSG msg2;
                    PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);
                }

                // we hide the throbber and security icon; because we're leaving the FS...
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // necessary, cannot use "refreshListBox" - the panel would remain empty longer (message boxes may appear)
                SetPluginIconsType(pitSimple); // PluginData==NULL, pitFromPlugin isn't allowed even with an empty panel
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                // SetValidFileData(VALID_DATA_ALL_FS_ARC);   // keep the current value, no reason to change it
                CommonRefresh(HWindow, -1, NULL, TRUE, TRUE, isRefresh);

                SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                ChangeToRescuePathOrFixedDrive(HWindow, NULL, refreshListBox, FALSE, FSTRYCLOSE_CHANGEPATHFAILURE);

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                return FALSE;
            }
        }

        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        //TRACE_I("change-to-fs: end");
        return ok ? !shorterPath : FALSE;
    }
}

BOOL CFilesWindow::ChangePathToDetachedFS(int fsIndex, int suggestedTopIndex,
                                          const char* suggestedFocusName, BOOL refreshListBox,
                                          int* failReason, const char* newFSName,
                                          const char* newUserPart, int mode, BOOL canFocusFileName)
{
    CALL_STACK_MESSAGE9("CFilesWindow::ChangePathToDetachedFS(%d, %d, %s, %d, , %s, %s, %d, %d)", fsIndex,
                        suggestedTopIndex, suggestedFocusName, refreshListBox, newFSName, newUserPart,
                        mode, canFocusFileName);

    char backup[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup;
    }
    if (newUserPart == NULL || newFSName == NULL)
    {
        newUserPart = NULL;
        newFSName = NULL;
    }
    char backup2[MAX_PATH];
    if (newUserPart != NULL)
    {
        lstrcpyn(backup2, newUserPart, MAX_PATH);
        newUserPart = backup2;
    }
    char backup3[MAX_PATH];
    if (newFSName != NULL)
    {
        lstrcpyn(backup3, newFSName, MAX_PATH);
        newFSName = backup3;
    }

    // restore panel state info (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // obtain FS interface encapsulation from DetachedFSList
    if (!MainWindow->DetachedFSList->IsGood())
    { // the array must be valid so the later Delete call succeeds
        TRACE_E("DetachedFSList array returns error, unable to finish operation.");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    if (fsIndex < 0 || fsIndex >= MainWindow->DetachedFSList->Count)
    {
        TRACE_E("Invalid index of detached FS: fsIndex=" << fsIndex);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    CPluginFSInterfaceEncapsulation* pluginFS = MainWindow->DetachedFSList->At(fsIndex);

    // retrieve fs-name of the detached FS
    char fsName[MAX_PATH];
    int fsNameIndex;
    if (newFSName != NULL) // if we must switch to a new fs-name, find out whether it exists and obtain its fs-name-index
    {
        strcpy(fsName, newFSName);
        int i;
        if (!Plugins.IsPluginFS(fsName, i, fsNameIndex)) // "always false" (the plugin was not unloaded; fs-name could not disappear)
        {
            TRACE_E("CFilesWindow::ChangePathToDetachedFS(): unexpected situation: requested FS was not found! fs-name=" << newFSName);
            newUserPart = NULL;
            newFSName = NULL;
        }
    }
    if (newFSName == NULL)
    {
        strcpy(fsName, pluginFS->GetPluginFSName());
        fsNameIndex = pluginFS->GetPluginFSNameIndex();
    }
    if (mode == -1)
        mode = newUserPart == NULL ? 1 : 2 /* refresh nebo history */;

    if (mode != 3 && canFocusFileName)
    {
        TRACE_E("CFilesWindow::ChangePathToDetachedFS() - incorrect use of 'mode' or 'canFocusFileName'.");
        canFocusFileName = FALSE;
    }

    CPluginData* plugin = Plugins.GetPluginData(pluginFS->GetPluginInterfaceForFS()->GetInterface());
    if (plugin == NULL)
    {
        TRACE_E("Unexpected situation in CFilesWindow::ChangePathToDetachedFS.");
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }

    //---  start the waiting cursor
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // is it already waiting?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // no refreshes, please

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];

    // not a FS path or the path is from another FS (even within the same plugin - one FS name)
    BOOL detachFS;
    if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
    { // the current path can be closed, attempt to open the new path
        // create a new object for the file system contents of the current path
        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
        BOOL closeDetachedFS = FALSE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // focus the file only if no other focus is proposed
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, newUserPart, *pluginFS, newFSDir, pluginData,
                                  shorterPath, pluginIconsType, mode,
                                  FALSE, NULL, NULL, -1, FALSE, cutFileName, NULL))
        {                    // success, the path (or subpath) was listed
            if (shorterPath) // subpath?
            {
                // invalidate proposed listbox settings (we'll list a different path)
                suggestedTopIndex = -1;
                if (cutFileName != NULL && *cutFileName != 0)
                    suggestedFocusName = cutFileName; // focus the file
                else
                    suggestedFocusName = NULL;
                if (failReason != NULL)
                {
                    *failReason = cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
                }
            }
            else
            {
                if (failReason != NULL)
                    *failReason = CHPPFR_SUCCESS;
            }

            if (UseSystemIcons || UseThumbnails)
                SleepIconCacheThread();

            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, FALSE, TRUE); // success, switch to the new path

            // we just received a new listing; if there are any reported panel changes, we cancel them
            InvalidateChangesInPanelWeHaveNewListing();

            // we hide the throbber and security icon; if the detached FS wants them it must enable them (e.g., in FSE_ATTACHED or FSE_PATHCHANGED)
            if (DirectoryLine != NULL)
                DirectoryLine->HideThrobberAndSecurityIcon();

            SetPanelType(ptPluginFS);
            SetPath(GetPath()); // detach the path from Snooper (stop monitoring changes on Path)
            SetPluginFS(pluginFS->GetInterface(), plugin->DLLName, plugin->Version,
                        plugin->GetPluginInterfaceForFS()->GetInterface(),
                        plugin->GetPluginInterface()->GetInterface(),
                        pluginFS->GetPluginFSName(), pluginFS->GetPluginFSNameIndex(),
                        pluginFS->GetPluginFSCreateTime(), pluginFS->GetChngDrvDuplicateItemIndex(),
                        plugin->BuiltForVersion);
            SetPluginIface(plugin->GetPluginInterface()->GetInterface());
            SetPluginFSDir(newFSDir);
            PluginData.Init(pluginData, plugin->DLLName, plugin->Version,
                            plugin->GetPluginInterface()->GetInterface(), plugin->BuiltForVersion);
            SetPluginIconsType(pluginIconsType);
            SetValidFileData(GetPluginFSDir()->GetValidData());

            // refresh the panel
            UpdateDriveIcon(FALSE); // get the icon for the current path from the plugin
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox);

            // notify the FS that it is attached again
            GetPluginFS()->Event(FSE_ATTACHED, GetPanelCode());
            GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());

            ok = TRUE;
        }
        else
        {
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
            closeDetachedFS = TRUE;
        }
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (!ok)
        {
            delete newFSDir;
            CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, FALSE, FALSE); // failure, stay on the original path
        }

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        if (ok)
        {
            // successful attachment, remove the FS from the detached list (Delete cannot fail)
            MainWindow->DetachedFSList->Delete(fsIndex);
            if (!MainWindow->DetachedFSList->IsGood())
                MainWindow->DetachedFSList->ResetState();
        }
        else
        {
            if (closeDetachedFS) // no path can be opened on the FS anymore, close it
            {
                BOOL dummy;
                if (pluginFS->TryCloseOrDetach(FALSE, FALSE, dummy, FSTRYCLOSE_ATTACHFAILURE))
                { // ask the user whether to close it or keep it (after a fatal ChangePath() error)
                    pluginFS->ReleaseObject(HWindow);
                    plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS->GetInterface());
                    // remove the FS from the detached list (Delete cannot fail)
                    MainWindow->DetachedFSList->Delete(fsIndex);
                    if (!MainWindow->DetachedFSList->IsGood())
                        MainWindow->DetachedFSList->ResetState();
                }
            }
        }

        return ok ? !shorterPath : FALSE;
    }
    else // the current path cannot be closed
    {
        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        if (failReason != NULL)
            *failReason = CHPPFR_CANNOTCLOSEPATH;
        return FALSE;
    }
}

void CFilesWindow::RefreshDiskFreeSpace(BOOL check, BOOL doNotRefreshOtherPanel)
{
    CALL_STACK_MESSAGE3("CFilesWindow::RefreshDiskFreeSpace(%d, %d)", check, doNotRefreshOtherPanel);
    if (Is(ptDisk))
    {
        if (!check || CheckPath(FALSE) == ERROR_SUCCESS)
        { // only if the path is accessible
            CQuadWord r = MyGetDiskFreeSpace(GetPath());
            DirectoryLine->SetSize(r);

            if (!doNotRefreshOtherPanel)
            {
                // if the other panel uses a path with the same root, we refresh 
                // disk-free-space there as well (it is not perfect - ideally we would 
                // test whether both paths are on the same volume, but that would be too slow;
                // this simplification should be more than enough for normal use)
                CFilesWindow* otherPanel = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
                if (otherPanel->Is(ptDisk) && HasTheSameRootPath(GetPath(), otherPanel->GetPath()))
                    otherPanel->RefreshDiskFreeSpace(TRUE, TRUE /* otherwise we'd recurse endlessly */);
            }
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            DirectoryLine->SetSize(CQuadWord(-1, -1)); // for archives free space is meaningless, hide the value
        }
        else
        {
            if (Is(ptPluginFS))
            {
                CQuadWord r;
                GetPluginFS()->GetFSFreeSpace(&r); // get the FS free space information from the plugin
                DirectoryLine->SetSize(r);
            }
        }
    }
}

void CFilesWindow::GetContextMenuPos(POINT* p)
{
    CALL_STACK_MESSAGE_NONE
    RECT r;
    if (!ListBox->GetItemRect(FocusedIndex, &r))
    {
        GetWindowRect(GetListBoxHWND(), &r);
        p->x = r.left;
        p->y = r.top;
        return;
    }
    p->x = r.left + 18;
    p->y = r.bottom;
    ClientToScreen(GetListBoxHWND(), p);
}

void GetCommonFileTypeStr(char* buf, int* resLen, const char* ext)
{
    char uppercaseExt[MAX_PATH];
    char* d = uppercaseExt;
    char* end = uppercaseExt + MAX_PATH - 1;
    while (d < end && *ext != 0 && *ext != ' ')
        *d++ = UpperCase[*ext++];
    *d = 0;
    if (*ext == 0 && uppercaseExt[0] != 0)
    { // we have the entire extension in uppercase (no spaces and shorter than MAX_PATH) + it is not empty
        *resLen = _snprintf_s(buf, TRANSFER_BUFFER_MAX, _TRUNCATE, CommonFileTypeName2, uppercaseExt);
        if (*resLen < 0)
            *resLen = TRANSFER_BUFFER_MAX - 1; // _snprintf_s reports truncation to the buffer size
    }
    else
    {
        memcpy(buf, CommonFileTypeName, CommonFileTypeNameLen + 1);
        *resLen = CommonFileTypeNameLen;
    }
}

void CFilesWindow::RefreshListBox(int suggestedXOffset,
                                  int suggestedTopIndex, int suggestedFocusIndex,
                                  BOOL ensureFocusIndexVisible, BOOL wholeItemVisible)
{
    CALL_STACK_MESSAGE6("CFilesWindow::RefreshListBox(%d, %d, %d, %d, %d)", suggestedXOffset,
                        suggestedTopIndex, suggestedFocusIndex, ensureFocusIndexVisible, wholeItemVisible);

    //TRACE_I("refreshlist: begin");

    KillQuickRenameTimer(); // prevent a potential QuickRenameWindow opening

    NarrowedNameColumn = FALSE;
    FullWidthOfNameCol = 0;
    WidthOfMostOfNames = 0;

    HDC dc = HANDLES(GetDC(GetListBoxHWND()));
    HFONT of = (HFONT)SelectObject(dc, Font);
    SIZE act;

    char formatedFileName[MAX_PATH];
    switch (GetViewMode())
    {
    case vmBrief:
    {
        SIZE max;
        max.cx = 0;
        act.cy = 0;
        int i;
        for (i = 0; i < Dirs->Count; i++)
        {
            CFileData* f = &Dirs->At(i);
            AlterFileName(formatedFileName, f->Name, f->NameLen,
                          Configuration.FileNameFormat, 0, TRUE);
            GetTextExtentPoint32(dc, formatedFileName, f->NameLen, &act);
            if (max.cx < act.cx)
                max.cx = act.cx;
        }
        for (i = 0; i < Files->Count; i++)
        {
            CFileData* f = &Files->At(i);
            AlterFileName(formatedFileName, f->Name, f->NameLen,
                          Configuration.FileNameFormat, 0, FALSE);
            GetTextExtentPoint32(dc, formatedFileName, f->NameLen, &act);
            if (max.cx < act.cx)
                max.cx = act.cx;
        }
        max.cy = act.cy;
        CaretHeight = (short)act.cy;

        max.cx += 2 * IconSizes[ICONSIZE_16];
        // minimal width (e.g., for an empty panel) so the user can hit UpDir
        if (max.cx < 4 * IconSizes[ICONSIZE_16])
            max.cx = 4 * IconSizes[ICONSIZE_16];
        Columns[0].Width = max.cx; // width of the 'Name' column
        max.cy += 4;
        if (max.cy < IconSizes[ICONSIZE_16] + 1)
            max.cy = IconSizes[ICONSIZE_16] + 1;
        ListBox->SetItemWidthHeight(max.cx, max.cy);
        break;
    }

    case vmIcons:
    {
        int w = IconSizes[ICONSIZE_32];
        int h = IconSizes[ICONSIZE_32];
        w += Configuration.IconSpacingHorz;
        h += Configuration.IconSpacingVert;
        ListBox->SetItemWidthHeight(w, h);
        break;
    }

    case vmThumbnails:
    {
        int w = ListBox->ThumbnailWidth + 2;
        int h = ListBox->ThumbnailHeight + 2;
        w += Configuration.ThumbnailSpacingHorz;
        h += Configuration.IconSpacingVert;
        ListBox->SetItemWidthHeight(w, h);
        break;
    }

    case vmTiles:
    {
        int w = IconSizes[ICONSIZE_48];
        int h = IconSizes[ICONSIZE_48];
        if (w < 48)
            w = 48; // for 32x32 the width was insufficient
        w += (int)(2.5 * (double)w);
        h += Configuration.TileSpacingVert;
        int textH = 3 * FontCharHeight + 4;
        ListBox->SetItemWidthHeight(w, max(textH, h));
        break;
    }

    case vmDetailed:
    {
        // Detailed view
        int columnWidthName = 0;
        int columnWidthExt = 0;
        int columnWidthDosName = 0;
        int columnWidthSize = 0;
        int columnWidthType = 0;
        int columnWidthDate = 0;
        int columnWidthTime = 0;
        int columnWidthAttr = 0;
        int columnWidthDesc = 0;

        // determine which columns are really visible (the plugin may have modified them)
        BOOL extColumnIsVisible = FALSE;
        DWORD autoWidthColumns = 0;
        int i;
        for (i = 0; i < Columns.Count; i++)
        {
            CColumn* column = &Columns[i];

            if (column->ID == COLUMN_ID_EXTENSION)
                extColumnIsVisible = TRUE;
            if (column->FixedWidth == 0)
            {
                switch (column->ID)
                {
                case COLUMN_ID_EXTENSION:
                    autoWidthColumns |= VIEW_SHOW_EXTENSION;
                    break;
                case COLUMN_ID_DOSNAME:
                    autoWidthColumns |= VIEW_SHOW_DOSNAME;
                    break;
                case COLUMN_ID_SIZE:
                    autoWidthColumns |= VIEW_SHOW_SIZE;
                    break;
                case COLUMN_ID_TYPE:
                    autoWidthColumns |= VIEW_SHOW_TYPE;
                    break;
                case COLUMN_ID_DATE:
                    autoWidthColumns |= VIEW_SHOW_DATE;
                    break;
                case COLUMN_ID_TIME:
                    autoWidthColumns |= VIEW_SHOW_TIME;
                    break;
                case COLUMN_ID_ATTRIBUTES:
                    autoWidthColumns |= VIEW_SHOW_ATTRIBUTES;
                    break;
                case COLUMN_ID_DESCRIPTION:
                    autoWidthColumns |= VIEW_SHOW_DESCRIPTION;
                    break;
                }
            }
        }

        i = 0;

        int dirsCount = Dirs->Count;
        int totalCount = Files->Count + Dirs->Count;
        if (dirsCount > 0)
        {
            GetTextExtentPoint32(dc, DirColumnStr, DirColumnStrLen, &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthSize < act.cx)
                columnWidthSize = act.cx;
        }
        else
            act.cx = act.cy = 0;

        char text[50];

        DWORD attrSkipCache[10]; // optimization of attribute-width measurement
        int attrSkipCacheCount = 0;
        ZeroMemory(&attrSkipCache, sizeof(attrSkipCache));

        CQuadWord maxSize(0, 0);

        BOOL computeDate = autoWidthColumns & VIEW_SHOW_DATE;
        if (computeDate && (totalCount > 20))
        {
            // determine whether we can estimate the widths
            if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, text, 50) != 0)
            {
                // check if the date format contains words (dddd || MMMM),
                // which would be rendered as text: (Monday || May)
                if (strstr(text, "dddd") == NULL && strstr(text, "MMMM") == NULL)
                {
                    SYSTEMTIME st;
                    st.wMilliseconds = 0;
                    st.wMinute = 59;
                    st.wSecond = 59;
                    st.wHour = 10;
                    st.wYear = 2000;
                    st.wMonth = 12;
                    st.wDay = 24;
                    st.wDayOfWeek = 0; // Sunday
                    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 50) == 0)
                        sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                    GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
                    act.cx += SPACE_WIDTH;
                    if (columnWidthDate < act.cx)
                        columnWidthDate = act.cx;
                    computeDate = FALSE;
                }
            }
        }

        int* nameColWidths = NULL;
        if (totalCount > 0)
            nameColWidths = (int*)malloc(totalCount * sizeof(int));

        BOOL dirTypeDone = FALSE;
        int nameLen;
        for (i = 0; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - dirsCount);
            //--- name
            BOOL extIsInExtColumn = extColumnIsVisible && (!isDir || Configuration.SortDirsByExt) &&
                                    f->Ext[0] != 0 && f->Ext > f->Name + 1; // exception for names like ".htaccess"; they appear in Name even though they are extensions
            if (Columns[0].FixedWidth == 0 || (autoWidthColumns & VIEW_SHOW_EXTENSION) && extIsInExtColumn)
            {
                AlterFileName(formatedFileName, f->Name, f->NameLen, // preparation of the formatted name to also compute the width of the separate Ext column
                              Configuration.FileNameFormat, 0, isDir);
                if (Columns[0].FixedWidth == 0)
                {
                    nameLen = extIsInExtColumn ? (int)(f->Ext - f->Name - 1) : f->NameLen;

                    GetTextExtentPoint32(dc, formatedFileName, nameLen, &act);
                    act.cx += 1 + IconSizes[ICONSIZE_16] + 1 + 2 + SPACE_WIDTH;
                    if (columnWidthName < act.cx)
                        columnWidthName = act.cx;
                    if (nameColWidths != NULL)
                        nameColWidths[i] = act.cx;
                }
            }
            //--- extension
            if ((autoWidthColumns & VIEW_SHOW_EXTENSION) && extIsInExtColumn)
            {
                GetTextExtentPoint32(dc, formatedFileName + (int)(f->Ext - f->Name), (int)(f->NameLen - (f->Ext - f->Name)), &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthExt < act.cx)
                    columnWidthExt = act.cx;
            }
            //--- dosname
            if ((autoWidthColumns & VIEW_SHOW_DOSNAME) && f->DosName != NULL)
            {
                GetTextExtentPoint32(dc, f->DosName, (int)strlen(f->DosName), &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthDosName < act.cx)
                    columnWidthDosName = act.cx;
            }
            //--- size
            if ((autoWidthColumns & VIEW_SHOW_SIZE) &&
                (!isDir || f->SizeValid)) // files and directories with a valid calculated size
            {
                if (f->Size > maxSize)
                    maxSize = f->Size;
            }
            //--- date || time
            if (computeDate)
            {
                SYSTEMTIME st;
                FILETIME ft;
                int len;
                if (!FileTimeToLocalFileTime(&f->LastWrite, &ft) ||
                    !FileTimeToSystemTime(&ft, &st))
                {
                    len = sprintf(text, LoadStr(IDS_INVALID_DATEORTIME));
                }
                else
                {
                    len = GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, text, 50) - 1;
                    if (len < 0)
                        len = sprintf(text, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
                }
                GetTextExtentPoint32(dc, text, len, &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthDate < act.cx)
                    columnWidthDate = act.cx;
            }

            if (autoWidthColumns & VIEW_SHOW_ATTRIBUTES)
            {
                //--- attr
                // prepare data for the cache !!! additional measured attributes may need hooking here
                DWORD mask = f->Attr & (FILE_ATTRIBUTE_READONLY | FILE_ATTRIBUTE_HIDDEN |
                                        FILE_ATTRIBUTE_SYSTEM | FILE_ATTRIBUTE_ARCHIVE |
                                        FILE_ATTRIBUTE_TEMPORARY | FILE_ATTRIBUTE_COMPRESSED |
                                        FILE_ATTRIBUTE_ENCRYPTED);
                if (mask != 0)
                {
                    if (mask != attrSkipCache[0] && mask != attrSkipCache[1] &&
                        mask != attrSkipCache[2] && mask != attrSkipCache[3] &&
                        mask != attrSkipCache[4] && mask != attrSkipCache[5] &&
                        mask != attrSkipCache[6] && mask != attrSkipCache[7] &&
                        mask != attrSkipCache[8] && mask != attrSkipCache[9])
                    {
                        GetAttrsString(text, f->Attr);
                        // this combination has not been measured yet
                        GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
                        act.cx += SPACE_WIDTH;
                        if (columnWidthAttr < act.cx)
                            columnWidthAttr = act.cx;
                        if (attrSkipCacheCount < 10)
                        {
                            // still space left, add the item to the cache
                            attrSkipCache[attrSkipCacheCount] = mask;
                            attrSkipCacheCount++;
                        }
                        else
                            attrSkipCache[0] = mask; // put it in the first position
                    }
                }
            }

            if (autoWidthColumns & VIEW_SHOW_TYPE)
            {
                //--- file-type
                if (!isDir) // it is a file
                {
                    char buf[TRANSFER_BUFFER_MAX];
                    BOOL commonFileType = TRUE;
                    if (f->Ext[0] != 0) // extension exists
                    {
                        char* dst = buf;
                        char* src = f->Ext;
                        while (*src != 0)
                            *dst++ = LowerCase[*src++];
                        *((DWORD*)dst) = 0;
                        int index;
                        if (Associations.GetIndex(buf, index))
                        {
                            src = Associations[index].Type;
                            if (src != NULL) // if it is not an empty string
                            {
                                commonFileType = FALSE;
                                GetTextExtentPoint32(dc, src, (int)strlen(src), &act);
                                act.cx += SPACE_WIDTH;
                                if (columnWidthType < act.cx)
                                    columnWidthType = act.cx;
                            }
                        }
                    }
                    if (commonFileType)
                    {
                        int resLen;
                        GetCommonFileTypeStr(buf, &resLen, f->Ext);
                        GetTextExtentPoint32(dc, buf, resLen, &act);
                        act.cx += SPACE_WIDTH;
                        if (columnWidthType < act.cx)
                            columnWidthType = act.cx;
                    }
                }
                else // it is a directory
                {
                    if (!dirTypeDone) // only if we have not computed it yet
                    {
                        if (i == 0 && isDir && strcmp(f->Name, "..") == 0)
                        {
                            GetTextExtentPoint32(dc, UpDirTypeName, UpDirTypeNameLen, &act);
                        }
                        else
                        {
                            dirTypeDone = TRUE;
                            GetTextExtentPoint32(dc, FolderTypeName, FolderTypeNameLen, &act);
                        }
                        act.cx += SPACE_WIDTH;
                        if (columnWidthType < act.cx)
                            columnWidthType = act.cx;
                    }
                }
            }
        }

        // size
        if (autoWidthColumns & VIEW_SHOW_SIZE)
        {
            int numLen;
            switch (Configuration.SizeFormat)
            {
            case SIZE_FORMAT_BYTES:
            {
                numLen = NumberToStr2(text, maxSize);
                break;
            }

            case SIZE_FORMAT_KB: // note: the same code appears elsewhere, search for this constant
            {
                PrintDiskSize(text, maxSize, 3);
                numLen = (int)strlen(text);
                break;
            }

            case SIZE_FORMAT_MIXED:
            {
                sprintf(text, "1023 GB"); // worst case scenario
                numLen = (int)strlen(text);
                break;
            }
            }
            GetTextExtentPoint32(dc, text, numLen, &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthSize < act.cx)
                columnWidthSize = act.cx;
        }
        // time
        if (autoWidthColumns & VIEW_SHOW_TIME)
        {
            SYSTEMTIME st;
            st.wYear = 2000;
            st.wMonth = 1;
            st.wDayOfWeek = 6;
            st.wDay = 1;
            st.wMilliseconds = 0;
            st.wMinute = 59;
            st.wSecond = 59;
            st.wHour = 10; // morning (AM)
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthTime < act.cx)
                columnWidthTime = act.cx;
            st.wHour = 23; // afternoon (PM)
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthTime < act.cx)
                columnWidthTime = act.cx;
        }

        ListBox->HeaderLine.SetMinWidths();

        FullWidthOfNameCol = (WORD)columnWidthName;

        if (nameColWidths != NULL && Columns[0].FixedWidth == 0 && totalCount > 0)
        {
            if (totalCount > 1)
                IntSort(nameColWidths, 0, totalCount - 1);
            WidthOfMostOfNames = (DWORD)(1.2 * nameColWidths[(DWORD)(totalCount * 0.85)]); // add 20% extra width so very long names are more visible and names near the threshold show fully
            if (WidthOfMostOfNames * 1.2 >= FullWidthOfNameCol)
                WidthOfMostOfNames = FullWidthOfNameCol; // if expanding by 44% (1.2*1.2) is enough to show all names, do it
        }
        if (nameColWidths != NULL)
            free(nameColWidths);

        TransferPluginDataIface = PluginData.GetInterface();
        int totalWidth = 0;
        for (i = 0; i < Columns.Count; i++)
        {
            CColumn* column = &Columns[i];

            if (column->FixedWidth == 0)
            {
                switch (column->ID)
                {
                case COLUMN_ID_NAME:
                    column->Width = (WORD)columnWidthName;
                    break;
                case COLUMN_ID_EXTENSION:
                    column->Width = (WORD)columnWidthExt;
                    break;
                case COLUMN_ID_DOSNAME:
                    column->Width = (WORD)columnWidthDosName;
                    break;
                case COLUMN_ID_SIZE:
                    column->Width = (WORD)columnWidthSize;
                    break;
                case COLUMN_ID_TYPE:
                    column->Width = (WORD)columnWidthType;
                    break;
                case COLUMN_ID_DATE:
                    column->Width = (WORD)columnWidthDate;
                    break;
                case COLUMN_ID_TIME:
                    column->Width = (WORD)columnWidthTime;
                    break;
                case COLUMN_ID_ATTRIBUTES:
                    column->Width = (WORD)columnWidthAttr;
                    break;
                case COLUMN_ID_DESCRIPTION:
                    column->Width = (WORD)columnWidthDesc;
                    break;
                case COLUMN_ID_CUSTOM:
                {
                    TransferActCustomData = column->CustomData;
                    int columnMaxWidth = column->MinWidth;
                    // ask the plugin
                    int j;
                    for (j = 0; j < totalCount; j++)
                    {
                        if (j < Dirs->Count)
                        {
                            TransferFileData = &Dirs->At(j);
                            TransferIsDir = (j == 0 && strcmp(TransferFileData->Name, "..") == 0) ? 2 : 1;
                        }
                        else
                        {
                            TransferFileData = &Files->At(j - dirsCount);
                            TransferIsDir = 0;
                        }
                        TransferRowData = 0;
                        TransferAssocIndex = -2; // maybe unnecessary if InternalGetType() cannot be called
                        column->GetText();
                        if (TransferLen > 0)
                        {
                            GetTextExtentPoint32(dc, TransferBuffer, TransferLen, &act);
                            act.cx += SPACE_WIDTH;
                            if (act.cx > columnMaxWidth)
                                columnMaxWidth = act.cx;
                        }
                        else
                            act.cx = 0;
                    }
                    column->Width = columnMaxWidth;
                    break;
                }

                default:
                    TRACE_E("Unknown type of column");
                    break;
                }
            }
            // keep boundaries so the width never drops below header-line minimum
            if (column->Width < column->MinWidth)
                column->Width = column->MinWidth;

            totalWidth += column->Width;
        }

        // handle Smart Mode for the Name column
        BOOL leftPanel = (MainWindow->LeftPanel == this);
        if (Columns[0].FixedWidth == 0 &&
            (leftPanel && ViewTemplate->LeftSmartMode || !leftPanel && ViewTemplate->RightSmartMode) &&
            ListBox->FilesRect.right - ListBox->FilesRect.left > 0) // only if the files-box has already been initialized
        {
            CColumn* column = &Columns[0];
            int narrow = totalWidth - (ListBox->FilesRect.right - ListBox->FilesRect.left);
            if (narrow > 0)
            {
                DWORD minWidth = WidthOfMostOfNames;
                if (minWidth > (DWORD)(0.75 * (ListBox->FilesRect.right - ListBox->FilesRect.left)))
                    minWidth = (DWORD)(0.75 * (ListBox->FilesRect.right - ListBox->FilesRect.left));
                if (minWidth < column->MinWidth)
                    minWidth = column->MinWidth;
                DWORD newWidth = max((int)(column->Width - narrow), (int)minWidth);
                NarrowedNameColumn = column->Width > newWidth;
                totalWidth -= column->Width - newWidth;
                column->Width = newWidth;
            }
        }

        CaretHeight = (short)FontCharHeight;

        int cy = FontCharHeight + 4;
        if (cy < IconSizes[ICONSIZE_16] + 1)
            cy = IconSizes[ICONSIZE_16] + 1;
        ListBox->SetItemWidthHeight(totalWidth, cy);
        break;
    }
    }

    SelectObject(dc, of);
    HANDLES(ReleaseDC(GetListBoxHWND(), dc));

    LastFocus = INT_MAX;
    FocusedIndex = 0;
    if (suggestedFocusIndex != -1)
    {
        FocusedIndex = suggestedFocusIndex;
        // if no TopIndex is suggested or focus visibility 
        // is required, compute a new TopIndex
        // -- clearer version with support for vmIcons and vmThumbnails
        // -- change for partially visible items: previously TopIndex was recalculated
        //    and causing unnecessary jumps; now we keep TopIndex unchanged

        BOOL findTopIndex = TRUE; // TRUE - search for TopIndex; FALSE - keep the current one
        if (suggestedTopIndex != -1)
        {
            // let EntireItemsInColumn be computed
            ListBox->SetItemsCount2(Files->Count + Dirs->Count);
            if (ensureFocusIndexVisible)
            {
                // we must ensure the focus is visible
                switch (ListBox->ViewMode)
                {
                case vmBrief:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // focus lies before the panel, we must find a better TopIndex

                    int cols = (ListBox->FilesRect.right - ListBox->FilesRect.left +
                                ListBox->ItemWidth - 1) /
                               ListBox->ItemWidth;
                    if (cols < 1)
                        cols = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex + ListBox->EntireItemsInColumn)
                            break; // focus lies past the panel, we must find a better TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex)
                            break; // focus lies past the panel, must find a better TopIndex
                    }

                    // focus is at least partially visible, skip TopIndex search
                    findTopIndex = FALSE;
                    break;
                }

                case vmDetailed:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // focus lies above the panel, we must find a better TopIndex

                    int rows = (ListBox->FilesRect.bottom - ListBox->FilesRect.top +
                                ListBox->ItemHeight - 1) /
                               ListBox->ItemHeight;
                    if (rows < 1)
                        rows = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex + 1) // avoid partial visibility, hence the +1
                            break;                                               // focus lies below the panel, we must find a better TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex)
                            break; // focus lies below the panel, we must find a better TopIndex
                    }

                    // focus is fully visible, skip TopIndex search
                    findTopIndex = FALSE;
                    break;
                }

                case vmIcons:
                case vmThumbnails:
                case vmTiles:
                {
                    int suggestedTop = ListBox->FilesRect.top + (suggestedFocusIndex /
                                                                 ListBox->ColumnsCount) *
                                                                    ListBox->ItemHeight;
                    int suggestedBottom = suggestedTop + ListBox->ItemHeight;

                    if (wholeItemVisible)
                    {
                        if (suggestedTop < suggestedTopIndex)
                            break; // focus is above the panel; we must find a better TopIndex

                        if (suggestedBottom > suggestedTopIndex +
                                                  ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus is below the panel; we must find a better TopIndex
                    }
                    else
                    {
                        if (suggestedBottom <= suggestedTopIndex)
                            break; // focus is above the panel; we must find a better TopIndex

                        if (suggestedTop >= suggestedTopIndex +
                                                ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus is below the panel; we must find a better TopIndex
                    }

                    // focus is at least partially visible, skip TopIndex search
                    findTopIndex = FALSE;
                    break;
                }
                }
            }
            else
                findTopIndex = FALSE;
        }
        if (findTopIndex)
        {
            ListBox->SetItemsCount2(Files->Count + Dirs->Count);
            suggestedTopIndex = ListBox->PredictTopIndex(suggestedFocusIndex);
        }
        /*
    // if no TopIndex is suggested or focusIndex visibility 
    // is required, compute a new TopIndex
    if (suggestedTopIndex == -1 || (ensureFocusIndexVisible &&
                                    (suggestedFocusIndex < suggestedTopIndex ||
                                     ListBox->Mode == vmDetailed &&
                                     suggestedTopIndex + ListBox->EntireItemsInColumn <= suggestedFocusIndex ||
                                     ListBox->Mode != vmDetailed &&
                                     suggestedTopIndex + ((ListBox->FilesRect.right -
                                      ListBox->FilesRect.left) / ListBox->ItemWidth) *
                                      ListBox->EntireItemsInColumn <= suggestedFocusIndex)
                                    ))
    {
      ListBox->ItemsCount = Files->Count + Dirs->Count;
      ListBox->UpdateInternalData();
      suggestedTopIndex = ListBox->PredictTopIndex(suggestedFocusIndex);
    }
*/
    }
    else
    {
        // note: patch situation when suggestedTopIndex != -1 and suggestedFocusIndex == -1
        // (e.g., Back in history to a place where the previously focused file no longer exists)
        if (ensureFocusIndexVisible && suggestedTopIndex != -1) // focus must be visible
        {
            suggestedTopIndex = -1; // cannot set top-index (the focus wouldn't be visible)
        }
    }

    if (suggestedXOffset == -1)
    {
        suggestedXOffset = 0;
        if (ListBox->ViewMode == vmDetailed)
            suggestedXOffset = ListBox->GetXOffset();
    }
    if (suggestedTopIndex == -1)
    {
        suggestedTopIndex = 0;
    }

    ListBox->SetItemsCount(Files->Count + Dirs->Count, suggestedXOffset, suggestedTopIndex, FALSE);
    ListBox->PaintHeaderLine();
    if (ListBox->HVScrollBar != NULL)
        UpdateWindow(ListBox->HVScrollBar);
    if (ListBox->HHScrollBar != NULL)
        UpdateWindow(ListBox->BottomBar.HWindow);
    ListBox->PaintAllItems(NULL, 0);

    IdleRefreshStates = TRUE;                       // force state-variable check on next idle
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                                                    //TRACE_I("refreshlist: end");
}

int CFilesWindow::GetResidualColumnWidth(int nameColWidth)
{
    CALL_STACK_MESSAGE1("CFilesWindow::GetResidualColumnWidth()");
    if (GetViewMode() == vmBrief)
        return 0;

    int colsWidth = 0;
    int colNameWidth = nameColWidth;
    // sum the width of visible columns (excluding the NAME column)
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        CColumn* column = &Columns[i];
        if (column->ID != COLUMN_ID_NAME)
            colsWidth += column->Width;
        else
        {
            if (colNameWidth == 0)
                colNameWidth = column->Width;
        }
    }

    int residual = ListBox->FilesRect.right - ListBox->FilesRect.left;
    residual -= colsWidth;
    if (residual < 0)
        residual = 0;
    if (residual > colNameWidth)
        residual = colNameWidth;
    return residual;
}
