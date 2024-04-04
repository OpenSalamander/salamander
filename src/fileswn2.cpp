// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
                RefreshDirectory(); // restore the panel (e.g. return the floppy disk or change the path)
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
                    { // When the full name is not available (issue with converting back from multibyte to UNICODE), we will use the DOS name
                        fileName = file->DosName;
                    }
                }
            }
            BOOL linkIsDir = FALSE;  // TRUE -> short-cut to directory -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> shortcut to file -> test archive
            BOOL linkIsNet = FALSE;  // TRUE -> shortcut to the network -> ChangePathToPluginFS
            DWORD err = ERROR_SUCCESS;
            if (StrICmp(file->Ext, "lnk") == 0) // Isn't this a shortcut to the directory?
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
                            {                                     // Exercise pulled path will serve to test accessibility, after Resolve we will pull it out again
                                err = CheckPath(FALSE, fullName); // fullName is a full path (other links are not supported)
                                if (err != ERROR_USER_TERMINATED) // if the user did not use ESC, we ignore the error
                                {
                                    err = ERROR_SUCCESS; // Resolve can change the path, then we will run the test again
                                }
                            }
                            if (err == ERROR_SUCCESS)
                            {
                                if (link->Resolve(HWindow, SLR_ANY_MATCH | SLR_UPDATE) == NOERROR)
                                {
                                    if (link->GetPath(fullName, MAX_PATH, &data, SLGP_UNCPRIORITY) == NOERROR)
                                    {
                                        // final form of fullName - we will test if it's okay.
                                        err = CheckPath(TRUE, fullName); // fullName is a full path (other links are not supported)
                                        if (err == ERROR_SUCCESS)
                                        {
                                            DWORD attr = SalGetFileAttributes(fullName); // we are getting here because data.dwFileAttributes simply does not fill, bad luck
                                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                                            {
                                                linkIsDir = TRUE; // o.k. let's try change-path-to-disk
                                            }
                                            else
                                            {
                                                linkIsFile = TRUE; // o.k. let's see if it's not an archive
                                            }
                                        }
                                    }
                                    else // We can try to open links directly to servers in the Network plugin (Nethood)
                                    {
                                        if (Plugins.GetFirstNethoodPluginFSName(netFSName))
                                        {
                                            if (link->GetPath(fullName, MAX_PATH, NULL, SLGP_RAWPATH) != NOERROR)
                                            { // The path is not stored in the link as text, but only as an ID list
                                                fullName[0] = 0;
                                                ITEMIDLIST* pidl;
                                                if (link->GetIDList(&pidl) == S_OK && pidl != NULL)
                                                { // we will obtain the ID list and inquire about the name of the last ID in the list, expecting "\\\\server"
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
                                            { // Let's try if it's not a link to the server (containing the path "\\\\server")
                                                char* backslash = fullName + 2;
                                                while (*backslash != 0 && *backslash != '\\')
                                                    backslash++;
                                                if (*backslash == '\\')
                                                    backslash++;
                                                if (*backslash == 0)  // we only take paths "\\\\", "\\\\server", "\\\\server\\"
                                                    linkIsNet = TRUE; // o.k. let's try change-path-to-FS
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
                return; // path error or interruption
            }
            if (linkIsDir || linkIsNet) // the link leads to a network or directory, the path is okay, let's switch to it
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

            if (PackerFormatConfig.PackIsArchive(linkIsFile ? fullName : fileName)) // Isn't it an archive?
            {
                // Backup data for TopIndexMem
                strcpy(path, GetPath());
                int topIndex = ListBox->GetTopIndex();

                if (!linkIsFile)
                {
                    // Constructs the full name of the archive for ChangePathToArchive
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
                if (ChangePathToArchive(fullName, "", -1, NULL, FALSE, &noChange)) // managed to get into the archive
                {
                    if (linkIsFile)
                        TopIndexMem.Clear(); // long jump
                    else
                        TopIndexMem.Push(path, topIndex); // remember the top index for return
                }
                else // archive is not accessible
                {
                    if (!noChange)
                        TopIndexMem.Clear(); // failure + we are not on the original path -> long jump
                }
                UpdateWindow(HWindow);
                EndStopRefresh();
                return;
            }

            UserWorkedOnThisPath = TRUE;

            // The ExecuteAssociation function below can change the path in the panel recursively
            // call (contains a message loop), so we will save the full file name here
            lstrcpy(fullPath, GetPath());
            if (!SalPathAppend(fullPath, fileName, MAX_PATH))
                fullPath[0] = 0;

            // launching the default item from the context menu (association)
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            MainWindow->SetDefaultDirectories(); // so that the starting process inherits the correct working directory
            ExecuteAssociation(GetListBoxHWND(), GetPath(), fileName);

            // add file to history
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
                                TopIndexMem.Clear(); // if we did not stay on the disk path (in the root UNC), it is a long jump
                                UpdateWindow(HWindow);
                            }
                        }
                    }
                    EndStopRefresh();
                    return; // There is no need to cut or are we already on the Nethood road
                }
                int topIndex; // next top-index, -1 -> invalid
                if (!TopIndexMem.FindAndPop(path, topIndex))
                    topIndex = -1;
                if (!ChangePathToDisk(HWindow, path, topIndex, prevDir))
                { // failed to shorten the path - long jump
                    TopIndexMem.Clear();
                }
            }
            else // subdirectory
            {
                // Backup data for TopIndexMem (path + topIndex)
                int topIndex = ListBox->GetTopIndex();

                // Backup the directory in case of access-denied
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

                // Vista: resolving non-listable junction points: changing the path to the target of junction points
                char junctTgtPath[MAX_PATH];
                int repPointType;
                if (GetPathDriveType() == DRIVE_FIXED && (dir->Attr & FILE_ATTRIBUTE_REPARSE_POINT) &&
                    GetReparsePointDestination(fullName, junctTgtPath, MAX_PATH, &repPointType, TRUE) &&
                    repPointType == 2 /* JUNCTION POINT*/ &&
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
                    TopIndexMem.Push(path, topIndex); // remember the top index for return
                }
                else // failure
                {
                    if (!IsTheSamePath(path, GetPath())) // we are not on the original path -> long jump
                    {                                    // Condition "!noChange" is not enough - it says "change of path or its reloading - access-denied-dir"
                        TopIndexMem.Clear();
                    }
                    else // no change in the path occurred (the path immediately shortened back to the original)
                    {
                        refresh = FALSE;
                        if (!noChange) // access-denied-dir: listing refreshed, but the path is the same
                        {              // return the listbox to the original indexes and finish the "refresh" after ChangePathToDisk
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
                    if (GetZIPPath()[0] == 0) // we will leave the archive
                    {
                        const char* s = strrchr(GetZIPArchive(), '\\'); // zip-archive does not contain unnecessary '\\' at the end
                        if (s != NULL)                                  // "always true"
                        {
                            strcpy(path, s + 1); // prev-dir

                            int topIndex; // next top-index, -1 -> invalid
                            if (!TopIndexMem.FindAndPop(GetPath(), topIndex))
                                topIndex = -1;

                            // lonely change of path
                            BOOL noChange;
                            if (!ChangePathToDisk(HWindow, GetPath(), topIndex, path, &noChange))
                            { // Failed to shorten the path - reject-close-archive or long jump
                                if (!noChange)
                                    TopIndexMem.Clear(); // long jump
                                else
                                {
                                    if (topIndex != -1) // if the top index was successfully obtained
                                    {
                                        TopIndexMem.Push(GetPath(), topIndex); // return top-index for next time
                                    }
                                }
                            }
                        }
                    }
                    else // shortening the path inside the archive
                    {
                        // split the zip-path into a new zip-path and prev-dir
                        strcpy(path, GetZIPPath());
                        char* prevDir;
                        char* s = strrchr(path, '\\'); // zip-path does not contain unnecessary backslashes (beginning/end)
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

                        // we will build a shortened path to the archive and obtain the top index according to it
                        strcpy(doublePath, GetZIPArchive());
                        SalPathAppend(doublePath, path, 2 * MAX_PATH);
                        int topIndex; // next top-index, -1 -> invalid
                        if (!TopIndexMem.FindAndPop(doublePath, topIndex))
                            topIndex = -1;

                        // lonely change of path
                        if (!ChangePathToArchive(GetZIPArchive(), path, topIndex, prevDir)) // "always false"
                        {                                                                   // failed to shorten the path - long jump
                            TopIndexMem.Clear();
                        }
                    }
                }
                else // subdirectory
                {
                    // Backup data for TopIndexMem (doublePath + topIndex)
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
                            TopIndexMem.Push(doublePath, topIndex); // remember the top index for return
                        }
                        else
                        {
                            if (!noChange)
                                TopIndexMem.Clear(); // failure + we are not on the original path -> long jump
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
                char fsNameBuf[MAX_PATH]; // GetPluginFS() may not exist, so let's put fsName into a local buffer instead
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

        //    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);    // jen pri zmene razeni  // zakomentovano, nevim proc to tu je: Petr
    }
    else
    {
        if (reverse)
        {
            ReverseSort = !ReverseSort;

            //      EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);  // only when sorting changes  // commented out, I don't know why it's here: Petr
        }
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //--- saving the focus item and sorting old items by name
    int focusIndex = GetCaretIndex();
    CFileData d1;
    if (focusIndex >= 0 && focusIndex < Dirs->Count + Files->Count)
        d1 = (focusIndex < Dirs->Count) ? Dirs->At(focusIndex) : Files->At(focusIndex - Dirs->Count);
    else
        d1.Name = NULL;
    //--- sorting
    if (UseSystemIcons || UseThumbnails)
        SleepIconCacheThread();
    SortDirectory();
    if (UseSystemIcons || UseThumbnails)
        WakeupIconCacheThread();
    //--- select item for focus + finally sorting
    CLessFunction lessDirs;  // for comparison what is smaller; necessary, see optimization in the search loop
    CLessFunction lessFiles; // for comparison what is smaller; necessary, see optimization in the search loop
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
    if (focusIndex < Dirs->Count) // looking for directory
    {
        i = 0;
        count = Dirs->Count;
    }
    else
    {
        i = Dirs->Count;                    // searching for file
        count = Dirs->Count + Files->Count; // There was an error here; count was not initialized
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
                i = 1; // ".." we need to skip, it's not included
        }
        for (; i < count; i++)
        {
            if (i < Dirs->Count)
            {
                CFileData* d2 = &Dirs->At(i);
                if (!lessDirs(*d2, d1, ReverseSort)) // thanks to sorting, TRUE will be at the desired item
                {
                    if (!lessDirs(d1, *d2, ReverseSort))
                        focusIndex = i; // Condition unnecessary, should be "always true"
                    break;
                }
            }
            else
            {
                CFileData* d2 = &Files->At(i - Dirs->Count);
                if (!lessFiles(*d2, d1, ReverseSort)) // thanks to sorting, TRUE will be at the desired item
                {
                    if (!lessFiles(d1, *d2, ReverseSort))
                        focusIndex = i; // Condition unnecessary, should be "always true"
                    break;
                }
            }
        }
    }
    if (focusIndex >= count)
        focusIndex = count - 1;
    //--- using the acquired data to finally set up the listbox
    SetCaretIndex(focusIndex, FALSE); // focus
    IdleRefreshStates = TRUE;         // During the next Idle, we will force the check of status variables
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
                *failReason = CHPPFR_SUCCESS; // Shortening "rescue" paths is not considered a failure
            return TRUE;
        }
        if (failReasonInt == CHPPFR_CANNOTCLOSEPATH)
        {
            if (failReason != NULL)
                *failReason = failReasonInt;
            return FALSE; // the problem "cannot close the current path in the panel" will not be solved by switching to fixed-drive (we will just stupidly ask for disconnect twice in a row)
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
    disks >>= 2; // skip A: and B: when formatting disks, DRIVE_FIXED is becoming somewhere
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

    BeginStopRefresh(); // He was snoring in his sleep

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

    EndStopRefresh(); // now he's sniffling again, he'll start up
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

    BeginSuspendMode(); // He was snoring in his sleep

    SetCurrentDirectoryToSystem(); // to unmap the disk from the panel

    // we will disconnect from the mapped disk, otherwise it will not be possible to disconnect "silently" (the system warns that it is in use)
    BOOL releaseLeft = MainWindow->LeftPanel->GetNetworkDrive() &&    // network disk (only ptDisk)
                       MainWindow->LeftPanel->GetPath()[0] != '\\';   // not UNC
    BOOL releaseRight = MainWindow->RightPanel->GetNetworkDrive() &&  // network disk (only ptDisk)
                        MainWindow->RightPanel->GetPath()[0] != '\\'; // not UNC
    if (releaseLeft)
        MainWindow->LeftPanel->HandsOff(TRUE);
    if (releaseRight)
        MainWindow->RightPanel->HandsOff(TRUE);

    //  Under Windows XP, the WNetDisconnectDialog dialog is non-modal. Users are stuck behind Salamander.
    //  and they wondered why the accelerators were not working. Then Salamander crashed in this method during closing
    //  because MainWindow was NULL;
    //  WNetDisconnectDialog(HWindow, RESOURCETYPE_DISK);

    CDisconnectDialog dlg(this);
    if (dlg.Execute() == IDCANCEL && dlg.NoConnection())
    {
        // the dialog did not even appear because it contained 0 resources -- we will display info
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

    EndSuspendMode(); // now he's sniffling again, he'll start up
}

void CFilesWindow::DriveInfo()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DriveInfo()");
    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return;

        BeginStopRefresh(); // He was snoring in his sleep

        CDriveInfo dlg(HWindow, GetPath());
        dlg.Execute();
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // now he's sniffling again, he'll start up
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
        IdleForceRefresh = TRUE;  // force update
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
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
    // if the middle toolbar is displayed, we give it a chance to position itself
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
                    newIndex = 1; // The end item was zero, let's jump to the other side of the list
            }
            else
            {
                if (newIndex < 1)
                    newIndex = 9; // The end item was zero, let's jump to the other side of the list
            }
        }
        else
        {
            if (forward)
            {
                if (newIndex > 9)
                    newIndex = oldIndex; // The boundary item was zero, we return to the last valid one
            }
            else
            {
                if (newIndex < 1)
                    newIndex = oldIndex; // The boundary item was zero, we return to the last valid one
            }
        }
    } while (Parent->ViewTemplates.Items[newIndex].Name[0] == 0 && newIndex != oldIndex);
    return newIndex;
}

BOOL CFilesWindow::IsViewTemplateValid(int templateIndex)
{
    CALL_STACK_MESSAGE2("CFilesWindow::IsViewTemplateValid(%d)", templateIndex);
    if (templateIndex < 1) // we cannot do tree yet
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
        // We do not want an undefined view - we will force the detailed one, which definitely exists
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

    // build new columns
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

        // When the view-mode changes, it is necessary to refresh (load a different size)
        // icons or thumbnails) - the only exception is brief and detailed, they have the same icons
        BOOL needRefresh = oldViewMode != GetViewMode() &&
                           (oldViewMode != vmBrief || GetViewMode() != vmDetailed) &&
                           (oldViewMode != vmDetailed || GetViewMode() != vmBrief);
        if (canRefreshPath && needRefresh)
        {
            // Until we reach the ReadDirectory, we will go above simple icons, because the geometry of the icons is changing
            TemporarilySimpleIcons = TRUE;

            // we will calculate the dimensions of items and ensure the visibility of the focused item
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
                // For now at least: until the next refresh (and thus ReadDirectory) we will focus on simple icons, because the geometry of the icons is changing
                TemporarilySimpleIcons = TRUE;
            }
            // we will calculate the dimensions of items and ensure the visibility of the focused item
            // if 'preserveTopIndex' is TRUE, we should not let the panel scroll beyond the focus
            RefreshListBox(-1, preserveTopIndex ? ListBox->TopIndex : -1, FocusedIndex, FALSE, FALSE);
        }
    }

    // When changing the panel mode, we need to clear the memory of the top index (incompatible data is stored)
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
        ListBox->PaintAllItems(NULL, 0); // Ensure rendering text on an empty panel
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
                    varPlacementsCount = 100; // could be damaged
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
    IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
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
        return TRUE; // the path on the disk can always be closed
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            BOOL someFilesChanged = FALSE;
            if (AssocUsed)
            {
                // if the user did not suppress it, we will display information about closing the archive containing the edited files
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
                // we will package the modified files (unless it is a critical shutdown), prepare for further use
                UnpackedAssocFiles.CheckAndPackAndClear(parent, &someFilesChanged);
                // During critical shutdown, we pretend that the updated files do not exist, we would pack them back into the archive
                // It is not ready, but we must not delete it, after the start the user must have a chance to update the files
                // manually pack into the archive; the exception is if nothing has been edited, everything can be deleted even then
                // critical shutdown (this is fast and we won't bother the user with unnecessary questions at startup)
                if (!someFilesChanged || !CriticalShutdown)
                {
                    SetEvent(ExecuteAssocEvent); // start file cleanup
                    DiskCache.WaitForIdle();
                    ResetEvent(ExecuteAssocEvent); // Finish cleaning up files
                }
            }
            AssocUsed = FALSE;
            // if files in the disk cache can be edited or if this archive is not open
            // and in the second panel, we will delete its cached files, it will be recreated upon next opening
            // unpack again (archive may have been edited in the meantime)
            CFilesWindow* another = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
            if (someFilesChanged || !another->Is(ptZIPArchive) || StrICmp(another->GetZIPArchive(), GetZIPArchive()) != 0)
            {
                StrICpy(buf, GetZIPArchive()); // the name of the archive in the disk cache is in lowercase (allows case-insensitive comparison of names from the Windows file system)
                DiskCache.FlushCache(buf);
            }

            // call the plug-in's CPluginInterfaceAbstract::CanCloseArchive
            BOOL canclose = TRUE;
            int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
            if (format != 0) // We found a supported archive
            {
                format--;
                BOOL userForce = FALSE;
                BOOL userAsked = FALSE;
                CPluginData* plugin = NULL;
                int index = PackerFormatConfig.GetUnpackerIndex(format);
                if (index < 0) // view: is it internal processing (plug-in)?
                {
                    plugin = Plugins.Get(-index - 1);
                    if (plugin != NULL)
                    {
                        if (!plugin->CanCloseArchive(this, GetZIPArchive(), CriticalShutdown)) // he doesn't want to close it
                        {
                            canclose = FALSE;
                            if (canForce) // We can ask the user if they want to force?
                            {
                                sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                userAsked = TRUE;
                                if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                  MB_YESNO | MB_ICONQUESTION) == IDYES) // user says "close"
                                {
                                    userForce = TRUE;
                                    plugin->CanCloseArchive(this, GetZIPArchive(), TRUE); // force==TRUE
                                    canclose = TRUE;
                                }
                            }
                        }
                    }
                }
                if (PackerFormatConfig.GetUsePacker(format)) // edit?
                {
                    index = PackerFormatConfig.GetPackerIndex(format);
                    if (index < 0) // Is it about internal processing (plug-in)?
                    {
                        if (plugin != Plugins.Get(-index - 1)) // if view==edit, do not call again
                        {
                            plugin = Plugins.Get(-index - 1);
                            if (plugin != NULL)
                            {
                                if (!plugin->CanCloseArchive(this, GetZIPArchive(), userForce || CriticalShutdown)) // he doesn't want to close it
                                {
                                    canclose = FALSE;
                                    if (canForce && !userAsked) // We can ask the user if they want to force?
                                    {
                                        sprintf(buf, LoadStr(IDS_ARCHIVEFORCECLOSE), GetZIPArchive());
                                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user says "close"
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

            return canclose; // Currently, I am only deciding on the plug-in CPluginInterfaceAbstract::CanCloseArchive
        }
        else
        {
            if (Is(ptPluginFS))
            {
                if (!canForce && !CriticalShutdown) // We cannot ask the user for force
                {
                    detachFS = FALSE; // if the plug-in did not touch it, so that the known value would be there
                    BOOL r = GetPluginFS()->TryCloseOrDetach(FALSE, canDetach, detachFS, tryCloseReason);
                    if (!r || !canDetach)
                        detachFS = FALSE; // Check/correct the output value
                    return r;
                }
                else // We can force -> we must close, disconnecting is not allowed
                {
                    if (GetPluginFS()->TryCloseOrDetach(CriticalShutdown, FALSE, detachFS, tryCloseReason) || // test closing without forceClose == TRUE (except for critical shutdown)
                        CriticalShutdown)
                    {
                        detachFS = FALSE;
                        return TRUE; // closing o.k.
                    }
                    else // Ask the user if they want to close it even against the will of the file system
                    {
                        char path[2 * MAX_PATH];
                        GetGeneralPath(path, 2 * MAX_PATH);
                        sprintf(buf, LoadStr(IDS_FSFORCECLOSE), path);
                        if (SalMessageBox(parent, buf, LoadStr(IDS_QUESTION),
                                          MB_YESNO | MB_ICONQUESTION) == IDYES) // user says "close"
                        {
                            GetPluginFS()->TryCloseOrDetach(TRUE, FALSE, detachFS, tryCloseReason);
                            detachFS = FALSE;
                            return TRUE; // closing o.k.
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
                // HICON hIcon = GetFileOrPathIconAux(path, FALSE, TRUE); // extract the icon
                MainWindow->DirHistoryAddPathUnique(0, path, NULL, NULL /*hIcon*/, NULL, NULL);
                if (!newPathIsTheSame)
                    UserWorkedOnThisPath = FALSE;
            }

            if (!newPathIsTheSame)
            {
                // leaving the road
                HiddenNames.Clear();  // release hidden names
                OldSelection.Clear(); // and the old selection
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
                    // leaving the road
                    HiddenNames.Clear();  // release hidden names
                    OldSelection.Clear(); // and the old selection
                }

                if (!isRefresh && canChangeSourceUID)
                    EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

                // if the archive data fits SalShExtPastedData, we pass it to it
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
                // Some additional unnecessary zeroing out, just for better clarity
                SetZIPArchive("");
                SetZIPPath("");

                SetPanelType(ptDisk); // for security reasons (the disk has no PluginData, etc.)
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
                            // leaving the road
                            HiddenNames.Clear();  // release hidden names
                            OldSelection.Clear(); // and the old selection
                        }
                    }

                    if (detachFS) // just disconnecting -> adding to MainWindow->DetachedFS
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
                            sendDetachEvent = TRUE; // The call must not be made directly here, because the FS is not yet disconnected (it is still in the panel)
                    }

                    if (!detachFS) // Close the file system, let it deallocate + print the last message boxes
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

                    if (!detachFS) // closing file system
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

                    SetPanelType(ptDisk); // for security reasons (the disk has no PluginData, etc.)
                    SetValidFileData(VALID_DATA_ALL);

                    if (sendDetachEvent && detachedFS != NULL /* always true*/)
                        detachedFS->Event(FSE_DETACHED, GetPanelCode()); // Provide a message about successful plug-in disconnection
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
    if (index >= 0 && index < Files->Count + Dirs->Count) // if the data is not initialized
    {
        int topIndex = ListBox->GetTopIndex();
        CFileData* file = index < Dirs->Count ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

        // Let's try to write a new top-index and focus-name
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

    // let's make backup copies
    char backup[MAX_PATH];
    lstrcpyn(backup, path, MAX_PATH); // must be done before UpdateDefaultDir (can be a pointer to DefaultDir[])
    char backup2[MAX_PATH];
    if (suggestedFocusName != NULL)
    {
        lstrcpyn(backup2, suggestedFocusName, MAX_PATH);
        suggestedFocusName = backup2;
    }

    // we restore the data about the panel state (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // when it comes to the relative path, we will convert it to absolute
    int errTextID;
    //  if (!SalGetFullName(backup, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // for FTP plugin - relative path in "target panel path" when connecting
    {
        SalMessageBox(parent, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    path = backup;

    // clock initialization
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // we do not want any refreshes -> they would mean recursion

    //  BOOL firstRun = TRUE;    // commented out to disable forceUpdate
    BOOL fixedDrive = FALSE;
    BOOL canTryUserRescuePath = FALSE; // Enables the use of Configuration.IfPathIsInaccessibleGoTo just before the fixed-drive path
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
    // when it comes to changes within one disk (including archives), we will find a valid directory at any cost
    // Changes to "fixed-drive"
    BOOL forceUpdateInt = (Is(ptDisk) || Is(ptZIPArchive)) && HasTheSameRootPath(GetPath(), path);
    BOOL detachFS;
    if (PrepareCloseCurrentPath(parent, canForce, TRUE, detachFS, tryCloseReason))
    { // Change within "ptDisk" or the current path will be closed, let's try to open a new path
        char changedPath[MAX_PATH];
        strcpy(changedPath, path);
        BOOL tryNet = !CriticalShutdown && ((!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath()));

    _TRY_AGAIN:

        DWORD err, lastErr;
        BOOL pathInvalid, cut;
        SalCheckAndRestorePathWithCut(parent, changedPath, tryNet, err, lastErr, pathInvalid, cut, FALSE);
        if (cut)
        { // Disable the proposed settings of the listbox (we will navigate a different path)
            suggestedTopIndex = -1;
            suggestedFocusName = NULL;
        }

        if (!pathInvalid && err == ERROR_SUCCESS)
        {
            /*    // commented optimization for the case when the new path is the same as the old one -> unusual for disks...
      if (!forceUpdate && firstRun && Is(ptDisk) && IsTheSamePath(changedPath, GetPath()))
      {  // no reason to change the path
        CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE);  // "cancel" - we stay on this path
        EndStopRefresh();
        if (setWait) SetCursor(oldCur);
        if (IsTheSamePath(path, GetPath()))
        {
          return TRUE; // the new path matches the current path, nothing to do
        }
        else
        {
          // the shortened path matches the current path
          // occurs, for example, when trying to enter an inaccessible directory (immediate return back)
          CheckPath(TRUE, path, lastErr, TRUE, parent);  // report the error that led to shortening the path
          return FALSE;  // the requested path is not accessible
        }
      }
      firstRun = FALSE;*/
            BOOL updateIcon;
            updateIcon = !Is(ptDisk) || // Simple because of commenting forceUpdate
                         !HasTheSameRootPath(changedPath, GetPath());
            //      updateIcon = forceUpdate || !Is(ptDisk) || !HasTheSameRootPath(changedPath, GetPath());

            if (UseSystemIcons || UseThumbnails)
                SleepIconCacheThread();

            if (!closeCalled)
            { // It enters here only on the first pass, so it is possible to use "Is(ptDisk)" and "GetPath()"
                BOOL samePath = (Is(ptDisk) && IsTheSamePath(GetPath(), changedPath));
                BOOL oldCanAddToDirHistory;
                if (samePath)
                {
                    // We will not remember the closed path (so that the path does not increase just because of a change-dir to the same path)
                    oldCanAddToDirHistory = MainWindow->CanAddToDirHistory;
                    MainWindow->CanAddToDirHistory = FALSE;
                }

                CloseCurrentPath(parent, FALSE, detachFS, samePath, isRefresh, !samePath); // success, let's move on to a new path

                if (samePath)
                {
                    // transition back to the original path memory mode
                    MainWindow->CanAddToDirHistory = oldCanAddToDirHistory;
                }

                // Hide the throbber and security icon, we don't need them on disk
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                closeCalled = TRUE;
            }
            //--- set the panel to the "disk" path
            SetPanelType(ptDisk);
            SetPath(changedPath);
            if (updateIcon ||
                !GetNetworkDrive()) // to display icons correctly when switching to mounted-volume (locally it does not slow down, so hopefully no problem)
            {
                UpdateDriveIcon(FALSE);
            }
            if (noChange != NULL)
                *noChange = FALSE; // Listing has been released
            forceUpdateInt = TRUE; // now something must be loaded (otherwise: empty panel)

            // load the content of the new path
            BOOL cannotList;
            cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
            if (isRefresh && !cannotList && GetMonitorChanges() && !AutomaticRefresh)
            {                                                                                                                // error in auto-refresh, we will verify if it's not due to deleting the directory displayed in the panel (it happened to me when deleting over the network from another machine) ... the unpleasant part is that if we ignore this error, the panel content simply won't refresh anymore (because auto-refresh is not working)
                Sleep(400);                                                                                                  // Let's take a short break so that the directory deletion can proceed (so that it's already deleted enough not to need to scroll, good humus...)
                                                                                                                             //        TRACE_I("Calling CommonRefresh again... (unable to receive change notifications, first listing was OK, but maybe current directory is being deleted)");
                cannotList = !CommonRefresh(parent, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh); // Let's repeat the listing, this should fail now
            }
            if (cannotList)
            { // Selected path cannot be listed ("access denied" or low_memory) or has already been deleted

            FIXED_DRIVE:

                BOOL change = FALSE;
                if (fixedDrive || !CutDirectory(changedPath)) // we will try to shorten the path
                {
                    if (canTryUserRescuePath) // First, we will try the "escape route" that the user desires
                    {
                        canTryUserRescuePath = FALSE; // we will not try it again
                        openIfPathIsInaccessibleGoToCfg = TRUE;
                        fixedDrive = FALSE; // Allow changing to fixed-drive (maybe it has been tried before, but the user's path was preferred)
                        GetIfPathIsInaccessibleGoTo(changedPath);
                        shorterPathWarning = TRUE; // we want to see errors "rescue" paths
                        change = TRUE;
                    }
                    else
                    {
                        if (openIfPathIsInaccessibleGoToCfg)
                            OpenCfgToChangeIfPathIsInaccessibleGoTo = TRUE;

                        // cannot be shortened, we will find a system or first fixed drive (our "escape disk")
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
                            disks >>= 2; // skip A: and B: when formatting disks, DRIVE_FIXED is becoming somewhere
                            char d = 'C';
                            while (d <= 'Z')
                            {
                                if (disks & 1)
                                {
                                    root[0] = d;
                                    if (GetDriveType(root) == DRIVE_FIXED)
                                        break; // we have our "leak disk"
                                }
                                disks >>= 1;
                                d++;
                            }
                            if (d <= 'Z')
                                done = TRUE; // Our "escape disk" was found
                        }
                        if (done)
                        {
                            if (LowerCase[root[0]] != LowerCase[changedPath[0]]) // Prevention of infinite loop
                            {                                                    // UNC or another disk (like "c:\")
                                strcpy(changedPath, root);                       // Let's try our "escape disk"
                                change = TRUE;
                            }
                        }
                    }
                }
                else
                    change = TRUE; // shortened path

                if (change) // only for a new road, otherwise we leave the panel empty (hopefully there is no danger, fixed-drive ensures this)
                {
                    // Disable the proposed settings of the listbox (we will navigate a different path)
                    suggestedTopIndex = -1;
                    suggestedFocusName = NULL;
                    // there is a change in the "new" path (CloseCurrentPath could survive UserWorkedOnThisPath == TRUE)
                    UserWorkedOnThisPath = FALSE;
                    // let's try to list the modified path
                    goto _TRY_AGAIN;
                }
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
            }
            else
            {
                // We have just received a new listing, if any changes are reported in the panel, we will cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                if (lastErr != ERROR_SUCCESS && (!isRefresh || openIfPathIsInaccessibleGoToCfg) && shorterPathWarning)
                {                        // if it is not about refresh and messages about shortcut should be displayed ...
                    if (!refreshListBox) // we will be displaying messages, we need to refresh the list box
                    {
                        RefreshListBox(0, -1, -1, FALSE, FALSE);
                    }
                    // reporting an error that led to a shortcut
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
            //--- Restore DefaultDir
            MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);
        }
        else
        {
            if (err == ERROR_NOT_READY) // when it comes to unprepared mechanics (removable medium)
            {
                char text[100 + MAX_PATH];
                char drive[MAX_PATH];
                UINT drvType;
                if (changedPath[0] == '\\' && changedPath[1] == '\\')
                {
                    drvType = DRIVE_REMOTE;
                    GetRootPath(drive, changedPath);
                    drive[strlen(drive) - 1] = 0; // we do not need the last '\\'
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
                { // to be able to get to the root of the path when a volume is mounted (F:\DRIVE_CD -> F:\)
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
                if (!pathInvalid &&               // about failure when reviving UNC path, user already knows
                    err != ERROR_USER_TERMINATED) // the user already knows about the interruption (ESC)
                {
                    CheckPath(TRUE, changedPath, err, TRUE, parent); // other errors - only print the error
                }
            }

            if (forceUpdateInt && !fixedDrive) // if an update is necessary, we will try fixed-drive
            {
                fixedDrive = TRUE; // defense against cycling + change to fixed-drive
                goto FIXED_DRIVE;
            }
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
        }

        if (!closeCalled)
            CloseCurrentPath(parent, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, we will stay on the original path
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

    // let's make backup copies
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

    // we restore the data about the panel state (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (noChange != NULL)
        *noChange = TRUE;

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    // if the archive is a relative path, we will convert it to an absolute one
    int errTextID;
    //  if (!SalGetFullName(backup1, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
    //                      MainWindow->GetActivePanel()->GetPath() : NULL))
    if (!SalGetFullName(backup1, &errTextID, Is(ptDisk) ? GetPath() : NULL)) // consistency with ChangePathToDisk()
    {
        SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORCHANGINGDIR),
                      MB_OK | MB_ICONEXCLAMATION);
        if (failReason != NULL)
            *failReason = CHPPFR_INVALIDPATH;
        return FALSE;
    }
    archive = backup1;

    //--- clock initialization
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // We do not want any refreshes

    BOOL nullFile;         // TRUE if the archive is an empty file (size==0)
    FILETIME archiveDate;  // date and time of the archive file
    CQuadWord archiveSize; // size of the archive file

    char text[MAX_PATH + 500];
    char path[MAX_PATH];
    BOOL sameArch;
    BOOL checkPath = TRUE;
    BOOL forceUpdateInt = FALSE; // change of path necessary? (possibly also to disk)
    BOOL tryPathWithArchiveOnError = isHistory;
    if (!Is(ptZIPArchive) || StrICmp(GetZIPArchive(), archive) != 0) // not an archive or another archive
    {

    _REOPEN_ARCHIVE:

        sameArch = FALSE;
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // Will close the current path, we will try to open a new path
            // test accessibility of the path on which the archive lies
            strcpy(path, archive);
            if (!CutDirectory(path, NULL))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangePathToArchive.");
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                tryPathWithArchiveOnError = FALSE; // meaningless error, we are not solving

            ERROR_1:

                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, we will stay on the original path

                if (forceUpdateInt) // necessary change of path; it didn't work to the archive, we're going to disk
                {                   // we are definitely in the archive (this is about "refresh" of the archive in the panel)
                    // if possible, we will exit the archive (or possibly to the "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else
                {
                    if (tryPathWithArchiveOnError) // Let's try changing the path to the path closest to the archive
                        ChangePathToDisk(HWindow, path, -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }

                EndStopRefresh();
                if (setWait)
                    SetCursor(oldCur);

                return FALSE;
            }

            // We will not test network paths if we have not just accessed them.
            BOOL tryNet = (!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath());
            DWORD err, lastErr;
            BOOL pathInvalid, cut;
            if (!SalCheckAndRestorePathWithCut(HWindow, path, tryNet, err, lastErr, pathInvalid, cut, FALSE) ||
                cut)
            { // path is not accessible or is truncated (archive cannot be opened)
                if (failReason != NULL)
                    *failReason = CHPPFR_INVALIDPATH;
                if (tryPathWithArchiveOnError)
                    tryPathWithArchiveOnError = (err == ERROR_SUCCESS && !pathInvalid); // Shorter path is accessible, let's try it
                if (!isRefresh)                                                         // Messages about shortening the path are not displayed during refresh
                {
                    sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(lastErr));
                    SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                goto ERROR_1;
            }

            if (PackerFormatConfig.PackIsArchive(archive)) // Is it an archive?
            {
                // retrieve information about the file (exists?, size, date&time)
                DWORD err2 = NO_ERROR;
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    GetFileTime(file, NULL, NULL, &archiveDate);
                    SalGetFileSize(file, archiveSize, err2); // returns "success?" - ignore, test later 'err2'
                    nullFile = archiveSize == CQuadWord(0, 0);
                    HANDLES(CloseHandle(file));
                }
                else
                    err2 = GetLastError();

                if (err2 != NO_ERROR)
                {
                    if (!isRefresh) // Messages about the non-existence of the path are not displayed during refresh
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
                    // Let's clear the cache so the object doesn't unnecessarily shake
                    newArchiveDir->FreeAddCache();

                    if (!nullFile)
                        DestroySafeWaitWindow();
                    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    BOOL isTheSamePath = FALSE; // TRUE = no change in path
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

                    // Success, let's move on to a new path - due to the possible time-consuming nature of browsing
                    // the archive will change the path even if the destination path does not exist - applies to commands
                    // Change Directory (Shift+F7), which otherwise does not change the path in this situation
                    CloseCurrentPath(HWindow, FALSE, detachFS, isTheSamePath, isRefresh, !isTheSamePath);

                    // We have just received a new listing, if any changes are reported in the panel, we will cancel them
                    InvalidateChangesInPanelWeHaveNewListing();

                    // Hide the throbber and security icon, we don't care about them in the archives
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
                        PluginData.Init(NULL, NULL, NULL, NULL, 0); // only plugins are used, Salamander does not
                    SetValidFileData(nullFile ? VALID_DATA_ALL_FS_ARC : GetArchiveDir()->GetValidData());
                    checkPath = FALSE;
                    if (noChange != NULL)
                        *noChange = FALSE;
                    // ZIPPath, Files and Dirs will be set later, once the archivePath is set ...
                    if (failReason != NULL)
                        *failReason = CHPPFR_SUCCESS;
                }
                else
                {
                    DestroySafeWaitWindow(); // nullFile must be FALSE, so the test is missing...
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
        else // current path cannot be closed
        {
            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);
            if (failReason != NULL)
                *failReason = CHPPFR_CANNOTCLOSEPATH;
            return FALSE;
        }
    }
    else // already open archive
    {
        if (forceUpdate) // It should be checked whether the archive has changed?
        {
            DWORD err;
            if ((err = CheckPath(!isRefresh)) == ERROR_SUCCESS) // there is no need to refresh the network connection ...
            {
                HANDLE file = HANDLES_Q(CreateFile(archive, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                   NULL, OPEN_EXISTING, 0, NULL));
                if (file != INVALID_HANDLE_VALUE)
                {
                    SalGetFileSize(file, archiveSize, err);
                    nullFile = archiveSize == CQuadWord(0, 0);
                    FILETIME zipArchiveDate = GetZIPArchiveDate();
                    BOOL change = (err != NO_ERROR ||                                     // unable to obtain size
                                   !GetFileTime(file, NULL, NULL, &archiveDate) ||        // unable to obtain date&time
                                   CompareFileTime(&archiveDate, &zipArchiveDate) != 0 || // or the date and time differ
                                   !IsSameZIPArchiveSize(archiveSize));                   // or file size differs
                    HANDLES(CloseHandle(file));

                    if (change) // file has changed
                    {
                        if (AssocUsed) // Do we have anything from the edited archive?
                        {
                            // we announce that there have been changes, so you should close the editors
                            char buf[MAX_PATH + 200];
                            sprintf(buf, LoadStr(IDS_ARCHIVEREFRESHEDIT), GetZIPArchive());
                            SalMessageBox(HWindow, buf, LoadStr(IDS_INFOTITLE), MB_OK | MB_ICONINFORMATION);
                        }
                        forceUpdateInt = TRUE; // There is no going back, change of course necessary (possibly also to disk)
                        goto _REOPEN_ARCHIVE;
                    }
                }
                else
                {
                    err = GetLastError(); // Cannot open archive file
                    if (!isRefresh)       // Messages about the non-existence of the path are not displayed during refresh
                    {
                        sprintf(text, LoadStr(IDS_FILEERRORFORMAT), archive, GetErrorText(err));
                        SalMessageBox(HWindow, text, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                }
            }
            if (err != ERROR_SUCCESS) // Let's switch to the existing path
            {
                if (err != ERROR_USER_TERMINATED)
                {
                    // if possible, we will exit the archive (or possibly to the "fixed-drive")
                    ChangePathToDisk(HWindow, GetPath(), -1, NULL, noChange, refreshListBox, FALSE, isRefresh);
                }
                else // user pressed ESC -> the path is probably inaccessible, let's go straight to "fixed-drive"
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

    // find a path in the archive that still exists (original or shortened)
    strcpy(path, *archivePath == '\\' ? archivePath + 1 : archivePath);
    char* end = path + strlen(path);
    if (end > path && *(end - 1) == '\\')
        *--end = 0;

    if (sameArch && GetArchiveDir()->SalDirStrCmp(path, GetZIPPath()) == 0) // new path matches the current path
    {
        // to prevent 'suggestedTopIndex' and 'suggestedFocusName' from being ignored, we call CommonRefresh
        CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        return TRUE; // nothing to do
    }

    // Save the current path in the archive
    char currentPath[MAX_PATH];
    strcpy(currentPath, GetZIPPath());

    SetZIPPath(path);
    BOOL ok = TRUE;
    char* fileName = NULL;
    BOOL useFileName = FALSE;
    while (path[0] != 0 && GetArchiveDirFiles() == NULL)
    {
        end = strrchr(path, '\\');
        useFileName = (canFocusFileName && suggestedFocusName == NULL && fileName == NULL); // file focus is possible + not focus from outside + only first abbreviation
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
            // there is a change in the "new" path (CloseCurrentPath could survive UserWorkedOnThisPath == TRUE)
            UserWorkedOnThisPath = FALSE;
        }
    }

    if (!useFileName && sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) == 0) // We are not focusing on the file and the shortened path matches the current path
    {                                                                                              // Occurs, for example, when trying to access an inaccessible directory (immediate return back)
        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
        return FALSE; // requested path is not available
    }

    if (!ok)
    {
        // Disable the proposed settings of the listbox (we will navigate a different path)
        suggestedTopIndex = -1;
        suggestedFocusName = NULL;
        if (failReason != NULL)
            *failReason = useFileName ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH;
    }

    // must succeed (at least the root of the archive always exists)
    CommonRefresh(HWindow, suggestedTopIndex, useFileName ? fileName : suggestedFocusName,
                  refreshListBox, TRUE, isRefresh);

    if (refreshListBox && !ok && useFileName && GetCaretIndex() == 0)
    { // attempt to select file name failed -> it was not a file name
        if (failReason != NULL)
            *failReason = CHPPFR_SHORTERPATH;
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);

    // we add the path we just left (inside the archive, paths are not closed,
    // so DirHistoryAddPathUnique has not been called yet) + only when it's not the current
    // path (only occurs when we focus on the file)
    if (sameArch && GetArchiveDir()->SalDirStrCmp(currentPath, GetZIPPath()) != 0)
    {
        if (UserWorkedOnThisPath)
        {
            MainWindow->DirHistoryAddPathUnique(1, GetZIPArchive(), currentPath, NULL, NULL, NULL);
            UserWorkedOnThisPath = FALSE;
        }

        // leaving the road
        HiddenNames.Clear();  // release hidden names
        OldSelection.Clear(); // and the old selection
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
    const char* origUserPart; // user-part, which path should we change to
    int origFSNameIndex;
    if (fsUserPart == NULL) // It's about a disconnected file system, recovering the listing...
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
        if (workDir == NULL) // out of memory -> release listing (will be an empty panel)
        {
            *keepOldListing = FALSE;
            workDir = dir;

            if (!firstCall)
            {
                // Release data listing in panel
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // we assume that 'dir' is PluginFSDir
                workDir = dir = GetPluginFSDir(); // in ReleaseListing() it can also just detach (see OnlyDetachFSListing)

                // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                // scrollbars. It can be delivered by the message loop when creating a message box.
                // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
        }
    }
    else
    {
        if (!firstCall)
        {
            workDir->Clear(NULL); // unnecessary (should be empty), just to be sure
        }
    }

    BOOL ok = FALSE;
    char user[MAX_PATH];
    lstrcpyn(user, origUserPart, MAX_PATH);
    pluginData = NULL;
    shorterPath = FALSE;
    if (cancel != NULL)
        *cancel = FALSE; // new data
    // we will attempt to load the contents of the "directory" (the path may gradually shorten)
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
        if (changePathRet) // ChangePath does not return an error
        {
            if (StrICmp(newFSName, fsName) != 0) // change fs-name, we need to verify the new fs-name
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
                    changePathRet = FALSE; // failed to change fs-name, simulating fatal error on FS
                else                       // we will start using a new name FS (for the next iteration of the loop)
                {
                    lstrcpyn(fsNameBuf, newFSName, MAX_PATH);
                    fsName = fsNameBuf;
                    fsNameIndex = newFSNameIndex;
                }
            }
            if (changePathRet) // Save the used fs-name to 'pluginFS'
                pluginFS.SetPluginFS(fsName, fsNameIndex);
        }

        if (changePathRet)
        { // the road looks o.k.
            if (pathWasCut && cutFileName != NULL && *cutFileName == 0)
                useCutFileName = FALSE;
            if (firstCall) // Change of path within the file system, the original path listing is not released (would it be enough?)
            {
                if (!forceUpdate && currentPath != NULL &&
                    pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(),
                                           currentPathFSNameIndex, currentPath)) // path shortened to original path
                {                                                                // There is no reason to change the route, the original listing is sufficient
                    shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                    if (cancel != NULL)
                        *cancel = TRUE;                     // we kept the original data
                    pluginIconsType = GetPluginIconsType(); // unnecessary (will not be used), but remains the same
                    ok = TRUE;
                    break;
                }

                if (keepOldListing == NULL || !*keepOldListing) // not dead-code (used in case of allocation error workDir)
                {
                    // Release data listing in panel
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();                 // we assume that 'dir' is PluginFSDir
                    workDir = dir = GetPluginFSDir(); // in ReleaseListing() it can also just detach (see OnlyDetachFSListing)

                    // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                    // scrollbars. It can be delivered by the message loop when creating a message box.
                    // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
                }
                firstCall = FALSE;
            }

            // we will try to list files and directories from the current path
            if (pluginFS.ListCurrentPath(workDir, pluginData, pluginIconsType, forceUpdate)) // succeeded ...
            {
                if (keepOldListing != NULL && *keepOldListing) // We have a new listing now, we will cancel the old one
                {
                    // Release data listing in panel
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();

                    ReleaseListing();       // we assume that 'dir' is PluginFSDir
                    dir = GetPluginFSDir(); // in ReleaseListing() it can also just detach (see OnlyDetachFSListing)

                    // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                    // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                    // scrollbars. It can be delivered by the message loop when creating a message box.
                    // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                    PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);

                    SetPluginFSDir(workDir); // set up a new listing
                    delete dir;
                    dir = workDir;
                }

                if (pluginIconsType != pitSimple &&
                    pluginIconsType != pitFromRegistry &&
                    pluginIconsType != pitFromPlugin) // we will verify if it hit the allowed value
                {
                    TRACE_E("Invalid plugin-icons-type!");
                    pluginIconsType = pitSimple;
                }
                if (pluginIconsType == pitFromPlugin && pluginData == NULL) // it wouldn't work, we are degrading the type
                {
                    TRACE_E("Plugin-icons-type is pitFromPlugin and plugin-data is NULL!");
                    pluginIconsType = pitSimple;
                }
                shorterPath = !pluginFS.IsCurrentPath(pluginFS.GetPluginFSNameIndex(), origFSNameIndex, origUserPart);
                ok = TRUE;
                break;
            }
            // Prepare the directory for the next use (if the plug-in left something behind, release it)
            workDir->Clear(NULL);
            // the path is not okay, we will try to shorten it in the next cycle pass
            if (!pluginFS.GetCurrentPath(user))
            {
                TRACE_E("Unexpected situation in CFilesWindow::ChangeAndListPathOnFS()");
                break;
            }
        }
        else // fatal error, we are ending
        {
            TRACE_I("Unable to open FS path " << fsName << ":" << origUserPart);

            if (firstCall && (keepOldListing == NULL || !*keepOldListing)) // not dead-code (used in case of allocation error workDir)
            {
                // Release data listing in panel
                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                ReleaseListing();                 // we assume that 'dir' is PluginFSDir
                workDir = dir = GetPluginFSDir(); // in ReleaseListing() it can also just detach (see OnlyDetachFSListing)

                // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                ListBox->SetItemsCount(0, 0, 0, TRUE);
                SelectedCount = 0;
                // When WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
                // scrollbars. It can be delivered by the message loop when creating a message box.
                // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
                PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
            }
            useCutFileName = FALSE;
            break;
        }
    }

    if (dir != workDir)
        delete workDir; // 'workDir' se nepouzil, uvolnime ho

    // try to find the file to focus on in the listing from the FS - it's not perfect if the file is hidden
    // from the listing in the panel (e.g. via "do not show hidden files" or via filters), then it cannot
    // Apartment in the panel focused and the user does not find out about this "error" - however, it probably doesn't report an error either
    // cannot, when the file actually exists, so let's forget about it (just like with
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
        if (i == count) // print an error (file for focusing was not found)
        {
            char errText[MAX_PATH + 200];
            sprintf(errText, LoadStr(IDS_UNABLETOFOCUSFILEONFS), cutFileName);
            SalMessageBox(HWindow, errText, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            *cutFileName = 0;
        }
    }

    if (!useCutFileName && cutFileName != NULL)
        *cutFileName = 0; // we do not want to use -> we reset
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

    // in case fsName points to a mutable string (GetPluginFS()->PluginFSName()), we will make a backup copy
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
    // let's make backup copies
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

    // we restore the data about the panel state (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    if (!isRefresh)
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    //--- clock initialization
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // We do not want any refreshes

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];
    int fsNameIndex;
    if (!Is(ptPluginFS) || !IsPathFromActiveFS(fsName, fsUserPart2, fsNameIndex, convertPathToInternal))
    { // if the file system is not FS or the path is from a different FS (even within one plug-in - one FS name)
        BOOL detachFS;
        if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
        { // Will close the current path, we will try to open a new path
            int index;
            if (failReason != NULL)
                *failReason = CHPPFR_INVALIDPATH;
            if (Plugins.IsPluginFS(fsName, index, fsNameIndex)) // find out the index of the plugin
            {
                // obtain plug-in with FS
                CPluginData* plugin = Plugins.Get(index);
                if (plugin != NULL)
                {
                    // open new file system
                    // load the plug-in before obtaining DLLName, Version, and plug-in interface
                    CPluginFSInterfaceAbstract* auxFS = plugin->OpenFS(fsName, fsNameIndex);
                    CPluginFSInterfaceEncapsulation pluginFS(auxFS, plugin->DLLName, plugin->Version,
                                                             plugin->GetPluginInterfaceForFS()->GetInterface(),
                                                             plugin->GetPluginInterface()->GetInterface(),
                                                             fsName, fsNameIndex, -1, 0, plugin->BuiltForVersion);
                    if (pluginFS.NotEmpty())
                    {
                        Plugins.SetWorkingPluginFS(&pluginFS);
                        if (convertPathToInternal) // Convert the path to internal format
                            pluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart2);
                        // create a new object for the content of the current file system path
                        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                        CPluginDataInterfaceAbstract* pluginData;
                        int pluginIconsType;
                        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // Focus on the file only if no other focus is suggested
                        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, pluginFS, newFSDir, pluginData,
                                                  shorterPath, pluginIconsType, mode, FALSE, NULL, NULL, -1,
                                                  FALSE, cutFileName, NULL))
                        {                    // success, the path (or subpath) was listed
                            if (shorterPath) // subpath?
                            {
                                // Disable the proposed settings of the listbox (we will navigate a different path)
                                suggestedTopIndex = -1;
                                if (cutFileName != NULL && *cutFileName != 0)
                                    suggestedFocusName = cutFileName; // File focus
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

                            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, isRefresh, TRUE); // success, let's move on to a new path

                            // We have just received a new listing, if any changes are reported in the panel, we will cancel them
                            InvalidateChangesInPanelWeHaveNewListing();

                            // Hide the throbber and security icon, if the new FS requires them, it must turn them on (for example in FSE_OPENED or FSE_PATHCHANGED)
                            if (DirectoryLine != NULL)
                                DirectoryLine->HideThrobberAndSecurityIcon();

                            SetPanelType(ptPluginFS);
                            SetPath(GetPath()); // Disconnecting the path from Snooper (end of tracking changes to Path)
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

                            // refresh panel
                            UpdateDriveIcon(FALSE); // obtain an icon from the plug-in for the current path
                            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                            // we will inform the FS that it is finally open
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
                CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, isRefresh, FALSE); // failure, we will stay on the original path

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            //TRACE_I("change-to-fs: end");
            return ok ? !shorterPath : FALSE;
        }
        else // current path cannot be closed
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
        // note: convertPathToInternal must now be FALSE (the path was converted in IsPathFromActiveFS())

        // PluginFS responds fsName and the path fsUserPart2 can be verified on it
        BOOL samePath = GetPluginFS()->IsCurrentPath(GetPluginFS()->GetPluginFSNameIndex(), fsNameIndex, fsUserPart2);
        if (!forceUpdate && samePath) // path is identical to the current path
        {
            // to prevent 'suggestedTopIndex' and 'suggestedFocusName' from being ignored, we call CommonRefresh
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, FALSE, isRefresh);

            EndStopRefresh();
            if (setWait)
                SetCursor(oldCur);

            if (failReason != NULL)
                *failReason = CHPPFR_SUCCESS;
            //TRACE_I("change-to-fs: end");
            return TRUE; // nothing to do
        }

        // We back up the current journey to FS (in case of an error, we will try to choose it again)
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

        // let's try to change the path on the current file system
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        BOOL cancel;
        BOOL keepOldListing = TRUE;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // Focus on the file only if no other focus is suggested
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, fsUserPart2, *GetPluginFS(), GetPluginFSDir(),
                                  pluginData, shorterPath, pluginIconsType, mode, TRUE, &cancel,
                                  currentPathOK ? currentPath : NULL, currentPathFSNameIndex, forceUpdate,
                                  cutFileName, &keepOldListing))
        { // success, the path (or subpath) was listed
            if (failReason != NULL)
            {
                *failReason = shorterPath ? (cutFileName != NULL && *cutFileName != 0 ? CHPPFR_FILENAMEFOCUSED : CHPPFR_SHORTERPATH) : CHPPFR_SUCCESS;
            }

            if (!cancel) // only if new content has been loaded (the original content has not remained loaded)
            {
                // We have just received a new listing, if any changes are reported in the panel, we will cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                // schovame throbbera a ikonu zabezpeceni, jestli o ne FS stoji i nadale, musi si je znovu zapnout (napr. v FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // add the path we just left (inside the FS paths are not closed,
                // so DirHistoryAddPathUnique has not been called yet)
                if (currentPathOK && (!samePath || samePath && shorterPath)) // the path has changed
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // leaving the road
                    HiddenNames.Clear();  // release hidden names
                    OldSelection.Clear(); // and the old selection
                }

                if (shorterPath) // subpath?
                {
                    // Disable the proposed settings of the listbox (we will navigate a different path)
                    suggestedTopIndex = -1;
                    if (cutFileName != NULL && *cutFileName != 0)
                        suggestedFocusName = cutFileName; // File focus
                    else
                    {
                        suggestedFocusName = NULL;

                        // the new path was shortened to the original path ("non-listable subdirectory"), we will keep
                        // topIndex and focusName before starting the operation (so the user doesn't lose focus)
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

                // We add new pluginData to the existing (newly filled) PluginFSDir
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

                // refresh panel
                UpdateDriveIcon(FALSE); // obtain an icon from the plug-in for the current path
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);

                // notify the file system that the path has changed
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else
            {
                if (shorterPath && cutFileName != NULL && *cutFileName != 0 && refreshListBox) // it is necessary to focus on the file
                {
                    int focusIndexCase = -1;
                    int focusIndexIgnCase = -1;
                    int i;
                    for (i = 0; i < Dirs->Count; i++)
                    { // for consistency with CommonRefresh we first look in directories,
                        // then in files (so that it behaves the same in both cases)
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

                    if (focusIndexIgnCase != -1) // at least one file with a possible difference in letter case was found
                    {
                        SetCaretIndex(focusIndexCase != -1 ? focusIndexCase : focusIndexIgnCase, FALSE);
                    }
                }
            }

            ok = TRUE;
        }
        else // requested path is not available, let's try to return to the original path
        {
            if (noChange != NULL)
                *noChange = FALSE; // the listing will be cleaned or modified
            if (!samePath &&       // if it's not a refresh (change to the same path)
                currentPathOK &&   // if the original path was successfully obtained
                ChangeAndListPathOnFS(currentPathFSName, currentPathFSNameIndex, currentPath,
                                      *GetPluginFS(), GetPluginFSDir(),
                                      pluginData, shorterPath, pluginIconsType, mode,
                                      FALSE, NULL, NULL, -1, FALSE, NULL, &keepOldListing))
            { // success, the original path (or its subpath) was enumerated
                // Disable the proposed settings of the listbox (we will navigate a different path)
                suggestedTopIndex = -1;
                suggestedFocusName = NULL;

                // the original path is accessible (we didn't have to shorten), we will keep the topIndex
                // and focusName before starting the operation (so that the user does not lose focus)
                if (!shorterPath)
                {
                    suggestedTopIndex = originalTopIndex;
                    suggestedFocusName = originalFocusName[0] == 0 ? NULL : originalFocusName;
                }

                // add the path we just left (inside the FS paths are not closed,
                // so DirHistoryAddPathUnique has not been called yet)
                if (currentPathOK && shorterPath) // if there is no shorterPath, the path has not changed...
                {
                    if (UserWorkedOnThisPath)
                    {
                        MainWindow->DirHistoryAddPathUnique(2, currentPathFSName, currentPath, NULL,
                                                            GetPluginFS()->GetInterface(), GetPluginFS());
                        UserWorkedOnThisPath = FALSE;
                    }
                    // leaving the road
                    HiddenNames.Clear();  // release hidden names
                    OldSelection.Clear(); // and the old selection
                }

                // We have just received a new listing, if any changes are reported in the panel, we will cancel them
                InvalidateChangesInPanelWeHaveNewListing();

                // schovame throbbera a ikonu zabezpeceni, jestli o ne FS stoji i nadale, musi si je znovu zapnout (napr. v FSE_PATHCHANGED)
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                if (UseSystemIcons || UseThumbnails)
                    SleepIconCacheThread();

                // We add new pluginData to the existing (newly filled) PluginFSDir
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

                // refresh panel
                UpdateDriveIcon(FALSE); // obtain an icon from the plug-in for the current path
                CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox, TRUE, isRefresh);
                if (failReason != NULL)
                    *failReason = mode == 3 ? CHPPFR_INVALIDPATH : CHPPFR_SHORTERPATH; // There was only CHPPFR_SHORTERPATH here, but Shift+F7 from dfs:x:\zumpa to dfs:x:\zumpa\aaa only reported a path error and did not return to the Shift+F7 dialog

                // notify the file system that the path has changed
                GetPluginFS()->Event(FSE_PATHCHANGED, GetPanelCode());
            }
            else // display an empty panel, nothing can be loaded from the FS, switch to fixed-drive
            {
                if (keepOldListing)
                {
                    // release old listing
                    if (UseSystemIcons || UseThumbnails)
                        SleepIconCacheThread();
                    ReleaseListing();
                    // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
                    ListBox->SetItemsCount(0, 0, 0, TRUE);
                    SelectedCount = 0;
                }
                else // There is no dead code, see the allocation error 'workDir' in ChangeAndListPathOnFS()
                {
                    // clean the message queue from buffered WM_USER_UPDATEPANEL
                    MSG msg2;
                    PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);
                }

                // We hide the throbber and security icon because we are leaving the full screen...
                if (DirectoryLine != NULL)
                    DirectoryLine->HideThrobberAndSecurityIcon();

                // It is necessary not to use 'refreshListBox' - the panel will be empty for a "longer" period of time (+ message boxes are at risk)
                SetPluginIconsType(pitSimple); // PluginData==NULL, pitFromPlugin cannot be done even with an empty panel
                if (SimplePluginIcons != NULL)
                {
                    delete SimplePluginIcons;
                    SimplePluginIcons = NULL;
                }
                // SetValidFileData(VALID_DATA_ALL_FS_ARC);   // keep the current value, no reason to change
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

    // we restore the data about the panel state (top-index + focused-name) before potentially closing this path
    RefreshPathHistoryData();

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit

    // we will obtain encapsulation of the FS interface from DetachedFSList
    if (!MainWindow->DetachedFSList->IsGood())
    { // for a later Delete to work, the array must be okay.
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

    // get fs-name of disconnected FS
    char fsName[MAX_PATH];
    int fsNameIndex;
    if (newFSName != NULL) // if we are switching to a new fs-name, we need to determine if it exists and what fs-name-index it has
    {
        strcpy(fsName, newFSName);
        int i;
        if (!Plugins.IsPluginFS(fsName, i, fsNameIndex)) // "always false" (plugin did not unload, fs-name could not disappear)
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
        mode = newUserPart == NULL ? 1 : 2 /* refresh or history*/;

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

    //--- clock initialization
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    BeginStopRefresh(); // We do not want any refreshes

    BOOL ok = FALSE;
    BOOL shorterPath;
    char cutFileNameBuf[MAX_PATH];

    // if the file system is not FS or the path is from a different FS (even within one plug-in - one FS name)
    BOOL detachFS;
    if (PrepareCloseCurrentPath(HWindow, FALSE, TRUE, detachFS, FSTRYCLOSE_CHANGEPATH))
    { // Will close the current path, we will try to open a new path
        // create a new object for the content of the current file system path
        CSalamanderDirectory* newFSDir = new CSalamanderDirectory(TRUE);
        BOOL closeDetachedFS = FALSE;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        CPluginDataInterfaceAbstract* pluginData;
        int pluginIconsType;
        char* cutFileName = canFocusFileName && suggestedFocusName == NULL ? cutFileNameBuf : NULL; // Focus on the file only if no other focus is suggested
        if (ChangeAndListPathOnFS(fsName, fsNameIndex, newUserPart, *pluginFS, newFSDir, pluginData,
                                  shorterPath, pluginIconsType, mode,
                                  FALSE, NULL, NULL, -1, FALSE, cutFileName, NULL))
        {                    // success, the path (or subpath) was listed
            if (shorterPath) // subpath?
            {
                // Disable the proposed settings of the listbox (we will navigate a different path)
                suggestedTopIndex = -1;
                if (cutFileName != NULL && *cutFileName != 0)
                    suggestedFocusName = cutFileName; // File focus
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

            CloseCurrentPath(HWindow, FALSE, detachFS, FALSE, FALSE, TRUE); // success, let's move on to a new path

            // We have just received a new listing, if any changes are reported in the panel, we will cancel them
            InvalidateChangesInPanelWeHaveNewListing();

            // Hide the throbber and security icon, if they are disconnected from the FS, they must turn them on (e.g. in FSE_ATTACHED or FSE_PATHCHANGED)
            if (DirectoryLine != NULL)
                DirectoryLine->HideThrobberAndSecurityIcon();

            SetPanelType(ptPluginFS);
            SetPath(GetPath()); // Disconnecting the path from Snooper (end of tracking changes to Path)
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

            // refresh panel
            UpdateDriveIcon(FALSE); // obtain an icon from the plug-in for the current path
            CommonRefresh(HWindow, suggestedTopIndex, suggestedFocusName, refreshListBox);

            // notify the file system that it is connected again
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
            CloseCurrentPath(HWindow, TRUE, detachFS, FALSE, FALSE, FALSE); // failure, we will stay on the original path
        }

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);

        if (ok)
        {
            // successful connection, we will remove the FS from the list of disconnected (success of Delete is certain)
            MainWindow->DetachedFSList->Delete(fsIndex);
            if (!MainWindow->DetachedFSList->IsGood())
                MainWindow->DetachedFSList->ResetState();
        }
        else
        {
            if (closeDetachedFS) // It is no longer possible to open any path on the file system, let's close it
            {
                BOOL dummy;
                if (pluginFS->TryCloseOrDetach(FALSE, FALSE, dummy, FSTRYCLOSE_ATTACHFAILURE))
                { // Ask the user if they want to close it or leave it (in the state after a fatal error ChangePath())
                    pluginFS->ReleaseObject(HWindow);
                    plugin->GetPluginInterfaceForFS()->CloseFS(pluginFS->GetInterface());
                    // remove FS from the list of disconnected (success of Delete is certain)
                    MainWindow->DetachedFSList->Delete(fsIndex);
                    if (!MainWindow->DetachedFSList->IsGood())
                        MainWindow->DetachedFSList->ResetState();
                }
            }
        }

        return ok ? !shorterPath : FALSE;
    }
    else // current path cannot be closed
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
                // if there is a path with the same root in the adjacent panel, we will refresh
                // disk-free-space in the adjacent panel (not perfect - ideal would be
                // test if the paths are on the same bundle, but that would be too slow,
                // this simplification will be more than enough for ordinary use)
                CFilesWindow* otherPanel = (MainWindow->LeftPanel == this) ? MainWindow->RightPanel : MainWindow->LeftPanel;
                if (otherPanel->Is(ptDisk) && HasTheSameRootPath(GetPath(), otherPanel->GetPath()))
                    otherPanel->RefreshDiskFreeSpace(TRUE, TRUE /* otherwise infinite recursion*/);
            }
        }
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            DirectoryLine->SetSize(CQuadWord(-1, -1)); // There is no free space in the archive, it makes sense to hide the data
        }
        else
        {
            if (Is(ptPluginFS))
            {
                CQuadWord r;
                GetPluginFS()->GetFSFreeSpace(&r); // we will get free space on the file system from the plug-in
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
    { // we have an upper-case extension (does not contain spaces and is not longer than MAX_PATH) + is not empty
        *resLen = _snprintf_s(buf, TRANSFER_BUFFER_MAX, _TRUNCATE, CommonFileTypeName2, uppercaseExt);
        if (*resLen < 0)
            *resLen = TRANSFER_BUFFER_MAX - 1; // _snprintf_s reports truncation to buffer size
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

    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow

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
        // minimum width (for example for an empty panel) for the user to be able to hit UpDir
        if (max.cx < 4 * IconSizes[ICONSIZE_16])
            max.cx = 4 * IconSizes[ICONSIZE_16];
        Columns[0].Width = max.cx; // Width of the column 'Name'
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
            w = 48; // the width was not sufficient for 32x32
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

        // we will determine which columns are actually displayed (the plugin could have modified the columns)
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

        DWORD attrSkipCache[10]; // optimizing the extraction of attribute width
        int attrSkipCacheCount = 0;
        ZeroMemory(&attrSkipCache, sizeof(attrSkipCache));

        CQuadWord maxSize(0, 0);

        BOOL computeDate = autoWidthColumns & VIEW_SHOW_DATE;
        if (computeDate && (totalCount > 20))
        {
            // check if we are able to predict widths
            if (GetLocaleInfo(LOCALE_USER_DEFAULT, LOCALE_SSHORTDATE, text, 50) != 0)
            {
                // Check if the date format does not contain bundles (dddd || MMMM),
                // which are textual consequences: (Monday || May)
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
                                    f->Ext[0] != 0 && f->Ext > f->Name + 1; // Exception for names like ".htaccess", they are displayed in the Name column even though they are extensions
            if (Columns[0].FixedWidth == 0 || (autoWidthColumns & VIEW_SHOW_EXTENSION) && extIsInExtColumn)
            {
                AlterFileName(formatedFileName, f->Name, f->NameLen, // preparation of the formatted name also for calculating the width of the separate Ext column
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
            //--- get name
            if ((autoWidthColumns & VIEW_SHOW_DOSNAME) && f->DosName != NULL)
            {
                GetTextExtentPoint32(dc, f->DosName, (int)strlen(f->DosName), &act);
                act.cx += SPACE_WIDTH;
                if (columnWidthDosName < act.cx)
                    columnWidthDosName = act.cx;
            }
            //--- size
            if ((autoWidthColumns & VIEW_SHOW_SIZE) &&
                (!isDir || f->SizeValid)) // files and directories with valid calculated size
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
                // Prepare data for cache !!! Additional measured attributes may need to be aligned here
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
                        // we haven't measured this combination yet
                        GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
                        act.cx += SPACE_WIDTH;
                        if (columnWidthAttr < act.cx)
                            columnWidthAttr = act.cx;
                        if (attrSkipCacheCount < 10)
                        {
                            // There is still space, I will add the item to the cache
                            attrSkipCache[attrSkipCacheCount] = mask;
                            attrSkipCacheCount++;
                        }
                        else
                            attrSkipCache[0] = mask; // put her in first place
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
                    if (f->Ext[0] != 0) // there is an extension
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
                            if (src != NULL) // if it's not an empty string
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
                    if (!dirTypeDone) // only if we haven't counted it yet
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

            case SIZE_FORMAT_KB: // Attention, the same code is in another place, search for this constant
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
            st.wHour = 10; // morning
            if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, text, 50) == 0)
                sprintf(text, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
            GetTextExtentPoint32(dc, text, (int)strlen(text), &act);
            act.cx += SPACE_WIDTH;
            if (columnWidthTime < act.cx)
                columnWidthTime = act.cx;
            st.wHour = 23; // afternoon
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
            WidthOfMostOfNames = (DWORD)(1.2 * nameColWidths[(DWORD)(totalCount * 0.85)]); // we add 20% width, both extra long names will be better visible and whole names close to the chosen limit will be shown
            if (WidthOfMostOfNames * 1.2 >= FullWidthOfNameCol)
                WidthOfMostOfNames = FullWidthOfNameCol; // if expanding by 44% (1.2*1.2) is enough to fit all the names, we will do it
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
                    // ask plugins
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
                        TransferAssocIndex = -2; // possibly unnecessary - if InternalGetType() cannot be called
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
            // Check boundaries to prevent width from going below the minimum value of the header line
            if (column->Width < column->MinWidth)
                column->Width = column->MinWidth;

            totalWidth += column->Width;
        }

        // we will treat the Smart Mode column Name
        BOOL leftPanel = (MainWindow->LeftPanel == this);
        if (Columns[0].FixedWidth == 0 &&
            (leftPanel && ViewTemplate->LeftSmartMode || !leftPanel && ViewTemplate->RightSmartMode) &&
            ListBox->FilesRect.right - ListBox->FilesRect.left > 0) // only if the files-box is already initialized
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
        // if TopIndex is not recommended or is required
        // visibility of focusIndex, calculate new TopIndex
        // -- more readable version with support for vmIcons and vmThumbnails
        // -- change for partially visible items: TopIndex was recalculated earlier
        //    and an unnecessary jerk occurred; now we leave TopIndex unchanged

        BOOL findTopIndex = TRUE; // TRUE - we will search for TopIndex; FALSE - we will leave the current one
        if (suggestedTopIndex != -1)
        {
            // let's calculate EntireItemsInColumn
            ListBox->SetItemsCount2(Files->Count + Dirs->Count);
            if (ensureFocusIndexVisible)
            {
                // we need to ensure the visibility of focus
                switch (ListBox->ViewMode)
                {
                case vmBrief:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // focus lies in front of the panel, we need to find a better TopIndex

                    int cols = (ListBox->FilesRect.right - ListBox->FilesRect.left +
                                ListBox->ItemWidth - 1) /
                               ListBox->ItemWidth;
                    if (cols < 1)
                        cols = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex + ListBox->EntireItemsInColumn)
                            break; // focus lies behind the panel, we need to find a better TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + cols * ListBox->EntireItemsInColumn <=
                            suggestedFocusIndex)
                            break; // focus lies behind the panel, we need to find a better TopIndex
                    }

                    // focus is at least partially visible, suppress searching for TopIndex
                    findTopIndex = FALSE;
                    break;
                }

                case vmDetailed:
                {
                    if (suggestedFocusIndex < suggestedTopIndex)
                        break; // the focus is above the panel, we need to find a better TopIndex

                    int rows = (ListBox->FilesRect.bottom - ListBox->FilesRect.top +
                                ListBox->ItemHeight - 1) /
                               ListBox->ItemHeight;
                    if (rows < 1)
                        rows = 1;

                    if (wholeItemVisible)
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex + 1) // we do not want partial visibility, hence the +1
                            break;                                               // focus lies beneath the panel, we need to find a better TopIndex
                    }
                    else
                    {
                        if (suggestedTopIndex + rows <= suggestedFocusIndex)
                            break; // focus lies beneath the panel, we need to find a better TopIndex
                    }

                    // focus is fully visible, we suppress the search for TopIndex
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
                            break; // the focus is above the panel, we need to find a better TopIndex

                        if (suggestedBottom > suggestedTopIndex +
                                                  ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus lies beneath the panel, we need to find a better TopIndex
                    }
                    else
                    {
                        if (suggestedBottom <= suggestedTopIndex)
                            break; // the focus is above the panel, we need to find a better TopIndex

                        if (suggestedTop >= suggestedTopIndex +
                                                ListBox->FilesRect.bottom - ListBox->FilesRect.top)
                            break; // focus lies beneath the panel, we need to find a better TopIndex
                    }

                    // focus is at least partially visible, suppress searching for TopIndex
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
        /*      // if TopIndex is not recommended or focusIndex visibility is required,
    // calculate a new TopIndex
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
    }*/
    }
    else
    {
        // p.s. patch the situation when suggestedTopIndex != -1 and suggestedFocusIndex == -1 (e.g. Back in history)
        // to the location where the originally focused file in the panel no longer exists)
        if (ensureFocusIndexVisible && suggestedTopIndex != -1) // focus should be visible
        {
            suggestedTopIndex = -1; // Cannot set the top index (focus would not be visible)
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

    IdleRefreshStates = TRUE;                       // During the next Idle, we will force the check of status variables
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
    // calculate the width of the displayed columns (excluding the NAME column)
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
