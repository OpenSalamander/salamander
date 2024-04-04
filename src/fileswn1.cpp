// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "dialogs.h"
#include "mainwnd.h"
#include "usermenu.h"
#include "plugins.h"
#include "fileswnd.h"
#include "stswnd.h"
#include "snooper.h"
#include "zip.h"
#include "shellib.h"
#include "pack.h"
#include "thumbnl.h"
#include "geticon.h"
#include "shiconov.h"

//
// ****************************************************************************
// CFilesWindowAncestor
//

CFilesWindowAncestor::CFilesWindowAncestor()
{
    CALL_STACK_MESSAGE_NONE
    Files = new CFilesArray;
    Dirs = new CFilesArray;
    SelectedCount = 0;

    Path[0] = 0;
    SuppressAutoRefresh = FALSE;
    PanelType = ptDisk;
    MonitorChanges = TRUE;
    DriveType = DRIVE_UNKNOWN;

    ArchiveDir = NULL;
    ZIPArchive[0] = 0;
    ZIPPath[0] = 0;

    PluginFS.Init(NULL, NULL, NULL, NULL, NULL, NULL, -1, 0, 0, 0);
    PluginFSDir = NULL;
    PluginIconsType = pitSimple;
    SimplePluginIcons = NULL;

    char buf[MAX_PATH];
    GetSystemDirectory(buf, MAX_PATH);
    GetRootPath(Path, buf);

    OnlyDetachFSListing = FALSE;
    NewFSFiles = NULL;
    NewFSDirs = NULL;
    NewFSPluginFSDir = NULL;
    NewFSIconCache = NULL;

    PluginIface = NULL;
    PluginIfaceLastIndex = -1;
}

CFilesWindowAncestor::~CFilesWindowAncestor()
{
    CALL_STACK_MESSAGE1("CFilesWindowAncestor::~CFilesWindowAncestor()");
    if (Files != NULL)
        delete Files;
    if (Dirs != NULL)
        delete Dirs;
    if (PluginFS.NotEmpty() || PluginData.NotEmpty() ||
        ArchiveDir != NULL || PluginFSDir != NULL)
    {
        TRACE_E("Unexpected situation in CFilesWindowAncestor::~CFilesWindowAncestor()");
    }
}

DWORD
CFilesWindowAncestor::CheckPath(BOOL echo, const char* path, DWORD err, BOOL postRefresh, HWND parent)
{
    CALL_STACK_MESSAGE5("CFilesWindowAncestor::CheckPath(%d, %s, 0x%X, %d, )", echo, path, err, postRefresh);

    parent = (parent == NULL) ? HWindow : parent;
    if (path == NULL)
        path = GetPath();

    return SalCheckPath(echo, path, err, postRefresh, parent);
}

void CFilesWindowAncestor::ReleaseListing()
{
    CALL_STACK_MESSAGE_NONE

        ((CFilesWindow*)this)
            ->VisibleItemsArray.InvalidateArr();
    ((CFilesWindow*)this)->VisibleItemsArraySurround.InvalidateArr();
    if (OnlyDetachFSListing)
    {
        // Remove the listing from the panel including icons
        Files = NewFSFiles;
        Dirs = NewFSDirs;
        SetPluginFSDir(NewFSPluginFSDir);
        PluginData.Init(NULL, NULL, NULL, NULL, 0);
        if (NewFSIconCache != NULL)
            ((CFilesWindow*)this)->IconCache = NewFSIconCache;
        ((CFilesWindow*)this)->SetValidFileData(GetPluginFSDir()->GetValidData());

        OnlyDetachFSListing = FALSE;
        NewFSFiles = NULL;
        NewFSDirs = NULL;
        NewFSPluginFSDir = NULL;
        NewFSIconCache = NULL;
    }
    else
    {
        ReleaseListingBody(PanelType, ArchiveDir, PluginFSDir, PluginData, Files, Dirs, FALSE);
    }
    SelectedCount = 0;
}

BOOL CFilesWindowAncestor::IsPathFromActiveFS(const char* fsName, char* fsUserPart, int& fsNameIndex,
                                              BOOL& convertPathToInternal)
{
    CALL_STACK_MESSAGE_NONE
    fsNameIndex = -1;
    if (Is(ptPluginFS) && PluginFS.NotEmpty())
    {
        if (Plugins.AreFSNamesFromSamePlugin(PluginFS.GetPluginFSName(), fsName, fsNameIndex)) // compare if the file systems are from the same plug-in
        {
            if (convertPathToInternal)
            {
                PluginFS.GetPluginInterfaceForFS()->ConvertPathToInternal(fsName, fsNameIndex, fsUserPart);
                convertPathToInternal = FALSE;
            }
            return PluginFS.IsOurPath(PluginFS.GetPluginFSNameIndex(), fsNameIndex, fsUserPart);
        }
    }
    return FALSE;
}

BOOL CFilesWindowAncestor::GetGeneralPath(char* buf, int bufSize, BOOL convertFSPathToExternal)
{
    CALL_STACK_MESSAGE_NONE
    if (bufSize == 0)
        return FALSE;
    BOOL ret = TRUE;
    char buf2[2 * MAX_PATH];
    if (Is(ptDisk))
    {
        int l = (int)strlen(GetPath());
        if (l >= bufSize)
        {
            l = bufSize - 1;
            ret = FALSE;
        }
        memcpy(buf, GetPath(), l);
        buf[l] = 0;
    }
    else
    {
        if (Is(ptZIPArchive))
        {
            strcpy(buf2, GetZIPArchive());
            if (GetZIPPath()[0] != 0)
            {
                if (GetZIPPath()[0] != '\\')
                    strcat(buf2, "\\");
                strcat(buf2, GetZIPPath());
            }
            int l = (int)strlen(buf2);
            if (l >= bufSize)
            {
                l = bufSize - 1;
                ret = FALSE;
            }
            memcpy(buf, buf2, l);
            buf[l] = 0;
        }
        else
        {
            if (Is(ptPluginFS))
            {
                strcpy(buf2, PluginFS.GetPluginFSName());
                strcat(buf2, ":");
                char* userPart = buf2 + strlen(buf2);
                if (PluginFS.NotEmpty() && PluginFS.GetCurrentPath(userPart))
                {
                    if (convertFSPathToExternal)
                    {
                        PluginFS.GetPluginInterfaceForFS()->ConvertPathToExternal(PluginFS.GetPluginFSName(),
                                                                                  PluginFS.GetPluginFSNameIndex(),
                                                                                  userPart);
                    }

                    int l = (int)strlen(buf2);
                    if (l >= bufSize)
                    {
                        l = bufSize - 1;
                        ret = FALSE;
                    }
                    memcpy(buf, buf2, l);
                    buf[l] = 0;
                }
                else
                {
                    buf[0] = 0;
                    ret = FALSE;
                }
            }
            else
            {
                buf[0] = 0;
                ret = FALSE;
            }
        }
    }
    return ret;
}

void CFilesWindowAncestor::SetPath(const char* path)
{
    CALL_STACK_MESSAGE2("CFilesWindowAncestor::SetPath(%s)", path);
    if (SuppressAutoRefresh && (!Is(ptDisk) || !IsTheSamePath(path, Path)))
        SuppressAutoRefresh = FALSE;
    DetachDirectory((CFilesWindow*)this);
    strcpy(Path, path);

    //--- detection of file-based compression/encryption and FAT32
    DWORD dummy1, flags;
    if ((Is(ptDisk) || Is(ptZIPArchive)) &&
        MyGetVolumeInformation(path, NULL, NULL, NULL, NULL, 0, NULL, &dummy1, &flags, NULL, 0))
    {
        ((CFilesWindow*)this)->FileBasedCompression = (flags & FS_FILE_COMPRESSION) != 0 && Is(ptDisk);
        ((CFilesWindow*)this)->FileBasedEncryption = (flags & FILE_SUPPORTS_ENCRYPTION) != 0 && Is(ptDisk);
        ((CFilesWindow*)this)->SupportACLS = (flags & FS_PERSISTENT_ACLS) != 0 && Is(ptDisk);
    }
    else
    {
        ((CFilesWindow*)this)->FileBasedCompression = FALSE;
        ((CFilesWindow*)this)->FileBasedEncryption = FALSE;
        ((CFilesWindow*)this)->SupportACLS = FALSE;
    }

    MonitorChanges = FALSE;
    DriveType = DRIVE_UNKNOWN;
    if (!Is(ptPluginFS)) // pluginFS ensures changes otherwise ...
    {
        DriveType = MyGetDriveType(Path);
        switch (DriveType)
        {
        case DRIVE_REMOVABLE:
        {
            BOOL isDriveFloppy = FALSE; // Floppy drives have their own configuration separate from other removable drives
            int drv = UpperCase[Path[0]] - 'A' + 1;
            if (drv >= 1 && drv <= 26) // for safety we will perform a "range-check"
            {
                DWORD medium = GetDriveFormFactor(drv);
                if (medium == 350 || medium == 525 || medium == 800 || medium == 1)
                    isDriveFloppy = TRUE;
            }
            MonitorChanges = isDriveFloppy ? Configuration.DrvSpecFloppyMon : Configuration.DrvSpecRemovableMon;
            break;
        }

        case DRIVE_REMOTE:
        {
            MonitorChanges = Configuration.DrvSpecRemoteMon;
            break;
        }

        case DRIVE_CDROM:
        {
            MonitorChanges = Configuration.DrvSpecCDROMMon;
            break;
        }

        default: // case DRIVE_FIXED:   // not only fixed, but also the others (RAM DISK, etc.)
        {
            MonitorChanges = Configuration.DrvSpecFixedMon;
            break;
        }
        }

        // handle auto-refresh suppression
        if (SuppressAutoRefresh)
            MonitorChanges = FALSE;

        if (MonitorChanges)
            AddDirectory((CFilesWindow*)this, Path, DriveType == DRIVE_REMOVABLE || DriveType == DRIVE_FIXED);
        else // if we don't monitor changes, snooper won't call SetAutomaticRefresh -> we'll do it here
        {
            ((CFilesWindow*)this)->SetAutomaticRefresh(FALSE, TRUE);
        }
    }
    else // ptPluginFS - we will not perform any refreshes - the plug-in manages refreshing on its own
    {
        ((CFilesWindow*)this)->SetAutomaticRefresh(TRUE, TRUE);
    }
}

CFilesArray*
CFilesWindowAncestor::GetArchiveDirFiles(const char* zipPath)
{
    CALL_STACK_MESSAGE_NONE
    if (zipPath == NULL)
        zipPath = ZIPPath;
    return ArchiveDir->GetFiles(zipPath);
}

CFilesArray*
CFilesWindowAncestor::GetArchiveDirDirs(const char* zipPath)
{
    CALL_STACK_MESSAGE_NONE
    if (zipPath == NULL)
        zipPath = ZIPPath;
    return ArchiveDir->GetDirs(zipPath);
}

CFilesArray*
CFilesWindowAncestor::GetFSFiles()
{
    CALL_STACK_MESSAGE_NONE
    return PluginFSDir->GetFiles("");
}

CFilesArray*
CFilesWindowAncestor::GetFSDirs()
{
    CALL_STACK_MESSAGE_NONE
    return PluginFSDir->GetDirs("");
}

CPluginData*
CFilesWindowAncestor::GetPluginDataForPluginIface()
{
    return Plugins.GetPluginData(PluginIface, &PluginIfaceLastIndex);
}

void CFilesWindowAncestor::SetZIPPath(const char* path)
{
    CALL_STACK_MESSAGE_NONE
    if (*path == '\\')
        path++; // '\\' will not be at the beginning of ZIPPath
    int l = (int)strlen(path);
    if (l > 0 && path[l - 1] == '\\')
        l--; // ZIPPath will not end with '\\'
    memcpy(ZIPPath, path, l);
    ZIPPath[l] = 0;
}

void CFilesWindowAncestor::SetZIPArchive(const char* archive)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(ZIPArchive, archive);
}

BOOL CFilesWindowAncestor::SamePath(CFilesWindowAncestor* other)
{
    CALL_STACK_MESSAGE_NONE
    int l1 = (int)strlen(Path);
    if (l1 > 0 && Path[l1 - 1] == '\\')
        l1--;
    int l2 = (int)strlen(other->Path);
    if (l2 > 0 && other->Path[l2 - 1] == '\\')
        l2--;
    return (PanelType == ptDisk || PanelType == ptZIPArchive) &&
           (other->PanelType == ptDisk || other->PanelType == ptZIPArchive) &&
           l1 == l2 && StrNICmp(Path, other->Path, l1) == 0;
}

//
// ****************************************************************************
// CFilesWindow
//

void IconThreadThreadFBodyAux(const char* path, SHFILEINFO& shi, CIconSizeEnum iconSize)
{
    CALL_STACK_MESSAGE_NONE
    __try
    {
        // do not let the default icon be returned; in case of failure, apply simple icons
        if (!GetFileIcon(path, FALSE, &shi.hIcon, iconSize, FALSE, FALSE))
            shi.hIcon = NULL;

        //We switched to our own implementation (lower memory requirements, working XOR icons)
        //Additionally, it does not support obtaining EXTRALARGE and JUMBO icons, you need to access the system image list
        //SHGetFileInfo(path, 0, &shi, sizeof(shi),
        //              SHGFI_ICON | SHGFI_SMALLICON | SHGFI_SHELLICONSIZE);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 4))
    {
        FGIExceptionHasOccured++;
        shi.hIcon = NULL;
    }
}

unsigned IconThreadThreadFBody(void* parameter)
{
    CALL_STACK_MESSAGE1("IconThreadThreadFBody()");

    SetThreadNameInVCAndTrace("IconsReader");
    TRACE_I("Begin");
    CFilesWindow* window = (CFilesWindow*)parameter;

    // for shell extensions to work pulling icons through IconHandler and other COM/OLE stuff
    if (OleInitialize(NULL) != S_OK)
        TRACE_E("Error in OleInitialize.");

    IShellIconOverlayIdentifier** iconReadersIconOverlayIds = ShellIconOverlays.CreateIconReadersIconOverlayIds();

    HANDLE handles[2];
    handles[0] = window->ICEventTerminate;
    handles[1] = window->ICEventWork;
    DWORD wait = WAIT_TIMEOUT;
    BOOL run = TRUE;
    BOOL firstRound = TRUE; // REFRESH is sent in case of an error, but only the first time

    CSalamanderThumbnailMaker thumbMaker(window);

    while (run)
    {
        if (wait == WAIT_TIMEOUT) // otherwise wait is already filled from work mode
            wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);

        switch (wait)
        {
        case WAIT_OBJECT_0 + 1: // work
        {
            CALL_STACK_MESSAGE1("IconThreadThreadFBody::work");
            window->IconCacheValid = FALSE; // Needed for refreshing (sleep/wake-up icon of the reader), otherwise sets the main thread

            // The original 200ms delay is probably unnecessarily long, shortening it to 20ms
            // 20ms was not enough, the thread was able to start just by pressing Enter when entering the directory
            // petr: the main thread is redrawn with priority because it is at a higher priority + when he was here
            //       this sleep, the overlay icons (e.g. Tortoise SVN) blink even more than they do now
            // Give the main thread a moment for drawing and possibly a quick interruption when changing directories
            // (now used only as a "pause" in which RefreshDirectory() manages to insert new icons into the icon-cache, see 'WaitBeforeReadingIcons')
            if (window->WaitBeforeReadingIcons > 0)
                Sleep(window->WaitBeforeReadingIcons);
            if (window->WaitOneTimeBeforeReadingIcons > 0)
            {
                DWORD time = window->WaitOneTimeBeforeReadingIcons;
                window->WaitOneTimeBeforeReadingIcons = 0;
                Sleep(time); // we are waiting before starting to read icon-overlays, during this waiting all notifications about changes from Tortoise SVN should arrive (see IconOverlaysChangedOnPath())
            }

            HANDLES(EnterCriticalSection(&window->ICSleepSection));

            // Should we start a new job (wake-up -> sleep -> wake-up) or terminate?
            wait = WaitForMultipleObjects(2, handles, FALSE, 0);

            //        BOOL postRefresh = FALSE;
            if (wait == WAIT_TIMEOUT && !window->ICStopWork)
            {
                //          TRACE_I("Start reading.");
                window->ICWorking = TRUE;

                CIconSizeEnum iconSize = window->GetIconSizeForCurrentViewMode();

                CIconList* iconList;
                int iconListIndex;
                SHFILEINFO shi; // for historical reasons (SHGetFileInfo) shi.hIcon is used for all types of icons

                // Preparing full path to loaded files/directories (only for window->Is(ptDisk))
                char path[MAX_PATH + 10];
                path[0] = 0;
                WCHAR wPath[MAX_PATH + 10];
                wPath[0] = 0;
                char* name = path;
                WCHAR* wName = wPath;
                BOOL pathIsInvalid = FALSE;
                BOOL isGoogleDrivePath = FALSE;
                if (window->Is(ptDisk))
                {
                    int l = (int)strlen(window->GetPath());
                    memmove(path, window->GetPath(), l);
                    if (path[l - 1] != '\\')
                        path[l++] = '\\';
                    name = path + l; // pointer to the location of the full name
                    *name = 0;
                    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, path, l, wPath, MAX_PATH + 10);
                    wName = wPath + l;
                    *wName = 0;
                    pathIsInvalid = !PathContainsValidComponents(path, FALSE);
                    if (pathIsInvalid)
                        TRACE_I("Path contains invalid components, shell cannot read icons from such paths! Path: " << path);
                    isGoogleDrivePath = ShellIconOverlays.IsGoogleDrivePath(path);
                }

                BOOL readOnlyVisibleItems = window->InactWinOptimizedReading; // Refresh from snooper in inactive window: we only read visible icons/thumbnails/overlays, saving machine time (we are "in the background")
                                                                              //          if (readOnlyVisibleItems) TRACE_I("Refresh in inactive window, reading only visible icons...");
                BOOL readOnlyVisibleItemsDueToUMI = FALSE;                    // description below
                if (!readOnlyVisibleItems && UserMenuIconBkgndReader.IsReadingIcons())
                {
                    //            TRACE_I("Reading of usermenu icons is in progress, reading only visible icons...");
                    readOnlyVisibleItems = TRUE;
                    readOnlyVisibleItemsDueToUMI = TRUE;
                }

                BOOL readThumbnails = window->UseThumbnails; // should we try to load thumbnails?

                if (window->StopThumbnailLoading)
                    readThumbnails = FALSE; // Unwanted wake-up - let's at least suppress the loading of thumbnails

                BOOL pluginFSIconsFromPlugin = window->Is(ptPluginFS) &&
                                               window->GetPluginIconsType() == pitFromPlugin;
                BOOL pluginFSIconsFromRegistry = window->Is(ptPluginFS) &&
                                                 window->GetPluginIconsType() == pitFromRegistry;

                BOOL waitBeforeFirstReadIcon = FALSE; // TRUE only when jumping to SECOND_ROUND:
                BOOL repeatedRound = FALSE;           // TRUE when reloading icons/thumbnails due to ongoing reading of usermenu icons

            SECOND_ROUND: // if an icon cannot be read from the disk, a second attempt will be made in the end

                DWORD wanted = -1;                                 // invalid -> does nothing, then sleeps
                if (window->Is(ptDisk) || pluginFSIconsFromPlugin) // disk + FS/icons-from-plugin
                {
                    wanted = 0; // first load new icons, then old ones
                }
                else
                {
                    if (window->Is(ptZIPArchive) || pluginFSIconsFromRegistry) // archive + FS/icons-from-registry
                    {
                        wanted = 3; // our icons are defined by their icon-location
                    }
                    else
                        TRACE_E("Unexpected situation.");
                }
                // Before starting work, we will set FALSE to "ReadingDone" for all items in the icon-cache and to "IconOverlayDone" for all items in the panel
                int x;
                if (!repeatedRound)
                    for (x = 0; x < window->IconCache->Count; x++)
                        window->IconCache->At(x).SetReadingDone(0);
                if (firstRound && !repeatedRound)
                {
                    for (x = 0; x < window->Files->Count; x++)
                        window->Files->At(x).IconOverlayDone = 0;
                    for (x = 0; x < window->Dirs->Count; x++)
                        window->Dirs->At(x).IconOverlayDone = 0;
                }

                BOOL failed = FALSE;
                BOOL destroyPluginIcon = TRUE;

                int selectMode = 1;
                // 1 = sequential pass (VisibleItemsArray.IsArrValid() == FALSE),
                // 2 = iteration according to VisibleItemsArray,
                // 3 = traversal according to VisibleItemsArraySurround,
                // 4 = sequential pass (VisibleItemsArray.IsArrValid() == TRUE)

                BOOL canReadIconOverlays = firstRound && window->Is(ptDisk) && iconReadersIconOverlayIds != NULL;
                BOOL readIconOverlaysNow = FALSE; // TRUE = overlays are being read, FALSE = icons + thumbnails are being read

                //          TRACE_I("wanted=" << wanted << ", selectMode=" << selectMode);

                int lastVisArrVersion = -1;
                BOOL someNameSkipped = FALSE;
                int thumbnailFlag = 0;
                int i = 0;
                while (1)
                {
                    BOOL callWaitForObjects = TRUE;                                                                        // only optimization - during the search for an item (which takes almost no time), WaitForMultipleObjects is not called
                    if (i < (readIconOverlaysNow ? window->Files->Count + window->Dirs->Count : window->IconCache->Count)) // loading an icon from a file/directory or determining an icon overlay for a file/directory
                    {
                        CIconData* iconData = readIconOverlaysNow ? NULL : &window->IconCache->At(i);

                        BOOL skipName = FALSE;
                        if (selectMode == 1)
                        {
                            int visArrVer;
                            if (window->VisibleItemsArray.IsArrValid(&visArrVer))
                            {
                                i = 0;
                                lastVisArrVersion = visArrVer;
                                selectMode = 2;
                                //                  TRACE_I("selectMode=" << selectMode);
                                readIconOverlaysNow = FALSE;
                                //                  TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                continue;
                            }
                        }
                        else
                        {
                            if (selectMode == 2 || selectMode == 3)
                            {
                                int visArrVer;
                                BOOL visArrValid;
                                BOOL cont;
                                if (selectMode == 2)
                                {
                                    if (readIconOverlaysNow)
                                        cont = window->VisibleItemsArray.ArrContainsIndex(i, &visArrValid, &visArrVer);
                                    else
                                        cont = window->VisibleItemsArray.ArrContains(iconData->NameAndData,
                                                                                     &visArrValid, &visArrVer);
                                }
                                else
                                {
                                    if (readIconOverlaysNow)
                                        cont = window->VisibleItemsArraySurround.ArrContainsIndex(i, &visArrValid, &visArrVer);
                                    else
                                        cont = window->VisibleItemsArraySurround.ArrContains(iconData->NameAndData,
                                                                                             &visArrValid, &visArrVer);
                                }
                                if (!cont && visArrValid && visArrVer == lastVisArrVersion)
                                    skipName = TRUE;
                                else
                                {
                                    if (!visArrValid)
                                    {
                                        i = 0;
                                        selectMode = 1;
                                        //                      TRACE_I("selectMode=" << selectMode);
                                        readIconOverlaysNow = FALSE;
                                        //                      TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                        continue;
                                    }
                                    else
                                    {
                                        if (visArrVer != lastVisArrVersion)
                                        {
                                            i = 0;
                                            lastVisArrVersion = visArrVer;
                                            selectMode = 2;
                                            //                        TRACE_I("selectMode=" << selectMode);
                                            readIconOverlaysNow = FALSE;
                                            //                        TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                            continue;
                                        }
                                    }
                                }
                            }
                            else // selectMode == 4
                            {
                                int visArrVer;
                                if (window->VisibleItemsArray.IsArrValid(&visArrVer) && visArrVer != lastVisArrVersion)
                                {
                                    i = 0;
                                    lastVisArrVersion = visArrVer;
                                    selectMode = 2;
                                    //                    TRACE_I("selectMode=" << selectMode);
                                    readIconOverlaysNow = FALSE;
                                    //                    TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                                    continue;
                                }
                            }
                        }

                        if (!skipName)
                        {
                            if (readIconOverlaysNow) // new icons/thumbnails for the selected area (see 'selectMode') are loaded, now we are loading icon overlays
                            {
                                CFileData* fileData = i < window->Dirs->Count ? &window->Dirs->At(i) : &window->Files->At(i - window->Dirs->Count);
                                if (fileData->IconOverlayDone == 0 && (i > 0 || strcmp(fileData->Name, "..") != 0))
                                {
                                    fileData->IconOverlayDone = 1; // Let's mark that we have already retrieved this icon-overlay, so we don't do it unnecessarily again

                                    char fileName[MAX_PATH];
                                    DWORD fileAttrs = fileData->Attr;
                                    memcpy(fileName, fileData->Name, fileData->NameLen + 1);
                                    int minPriority = 100;
                                    if (i >= window->Dirs->Count && fileData->IsLink || // file is a link
                                        fileData->IsOffline ||                          // file or directory is offline (slow)
                                        i < window->Dirs->Count && fileData->Shared)    // directory is encrypted
                                    {
                                        minPriority = 9; // Overlays for link sharing slow files (offline) have priority 10, so we only consider overlays with higher priority (lower number than 10)
                                    }

                                    if (window->ICSleep)
                                        goto GO_SLEEP_MODE;
                                    HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                    // Load the icon from a file, the icon reader can go into sleep mode during loading
                                    *name = 0;
                                    //                    TRACE_I("Getting icon overlay index for: " << fileName << "...");
                                    SLOW_CALL_STACK_MESSAGE5("IconThreadThreadFBody::GetIconOverlayIndex(%s%s, 0x%08X, %d)",
                                                             path, fileName, fileAttrs, isGoogleDrivePath);
                                    DWORD iconOverlayIndex = ShellIconOverlays.GetIconOverlayIndex(wPath, wName, path, name,
                                                                                                   fileName, fileAttrs,
                                                                                                   minPriority, iconReadersIconOverlayIds,
                                                                                                   isGoogleDrivePath);
                                    //                    TRACE_I("Getting icon overlay index is done.");

                                    HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                    if (window->ICSleep)
                                        goto GO_SLEEP_MODE; // panel wants to go into sleep mode now

                                    CFileData* fileDataCheck = i < window->Dirs->Count ? &window->Dirs->At(i) : i < window->Files->Count + window->Dirs->Count ? &window->Files->At(i - window->Dirs->Count)
                                                                                                                                                               : NULL;
                                    if (fileData != fileDataCheck || strcmp(fileName, fileData->Name) != 0)
                                    {
                                        if (fileData != fileDataCheck)
                                            TRACE_E("IconThreadThreadFBody::GetIconOverlayIndex: PRUSER!!! (fileData != fileDataCheck)");
                                        else
                                            TRACE_E("IconThreadThreadFBody::GetIconOverlayIndex: PRUSER!!! (file name changed)");
                                    }
                                    else
                                    {
                                        BOOL redraw = fileData->IconOverlayIndex != iconOverlayIndex;
                                        fileData->IconOverlayIndex = iconOverlayIndex;

                                        int visArrVer;
                                        BOOL visArrValid;
                                        if (redraw && // It is necessary to redraw the index (change the icon overlay)
                                            (window->VisibleItemsArray.ArrContainsIndex(i, &visArrValid, &visArrVer) || !visArrValid))
                                        { // If we know that the item is visible or if we know nothing about the visibility of items, we let the index be redrawn
                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX2, i, 0);
                                        }
                                    }
                                }
                                else
                                    callWaitForObjects = FALSE; // no work -> no delays
                            }
                            else
                            {
                                if (iconData->GetReadingDone() == 0 &&
                                    iconData->GetFlag() == wanted)
                                {
                                    iconData->SetReadingDone(1);    // Let's mark that we have already worked with this icon, so we don't unnecessarily try it again during this cycle
                                    if (wanted == 0 || wanted == 2) // loading icons directly from a file or from a plug-in
                                    {
                                        if (!pluginFSIconsFromPlugin) // icon on disk
                                        {
                                            if (strlen(iconData->NameAndData) + (name - path) < MAX_PATH)
                                            {
                                                strcpy(name, iconData->NameAndData);

                                                if (window->ICSleep)
                                                    goto GO_SLEEP_MODE;
                                                HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                                if (waitBeforeFirstReadIcon)
                                                {
                                                    waitBeforeFirstReadIcon = FALSE;
                                                    //                            TRACE_I("Waiting 500ms before reading first icon in second round to have bigger chance to succeed.");
                                                    Sleep(500); // let's take a moment here (before the second attempt to load the icon)
                                                }

                                                // Load the icon from a file, the icon reader can go into sleep mode during loading
                                                CALL_STACK_MESSAGE3("IconThreadThreadFBody::GetFileIcon(%s, %d)", path, iconSize);

                                                if (!pathIsInvalid)
                                                {
                                                    //                            TRACE_I("Getting icon for: " << name << "...");
                                                    IconThreadThreadFBodyAux(path, shi, iconSize);
                                                    if (shi.hIcon == NULL)
                                                        TRACE_I("Unable to get icon from: " << path);
                                                    //                            else
                                                    //                              TRACE_I("Getting icon is done.");
                                                }
                                                else
                                                {
                                                    shi.hIcon = NULL;
                                                }

                                                HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                            }
                                            else
                                            {
                                                shi.hIcon = NULL;
                                                *name = 0;
                                                TRACE_I("Too long filename to get icon from: " << path << iconData->NameAndData);
                                            }
                                        }
                                        else // Icon in the plug-in FS - loading is uninterruptible (risk of canceling PluginData)
                                        {
                                            const CFileData* f = iconData->GetFSFileData();
                                            if (f != NULL)
                                            {
                                                shi.hIcon = window->PluginData.GetPluginIcon(f, iconSize, destroyPluginIcon);
                                                if (shi.hIcon == NULL)
                                                {
                                                    TRACE_I("Unable to get icon from FS item: " << iconData->NameAndData);
                                                }
                                            }
                                            else
                                            {
                                                shi.hIcon = NULL;
                                                TRACE_E("Unexpected error: Icon Cache doesn't contain FSFileData for item from FS with "
                                                        "pitFromPlugin icon type! Item: "
                                                        << iconData->NameAndData);
                                            }
                                        }
                                    }
                                    else
                                    {
                                        if (wanted == 3) // loading icons from icon-location
                                        {
                                            shi.hIcon = NULL;
                                            char* nameAndData = iconData->NameAndData;
                                            int size = (int)strlen(nameAndData) + 4;
                                            size -= (size & 0x3);         // size % 4 (alignment to four bytes)
                                            char* s = nameAndData + size; // skip zero alignment
                                            BOOL doExtractIcons = FALSE;
                                            BOOL doLoadImage = FALSE;
                                            int index = -1;
                                            char* num = strrchr(s, ','); // the icon number is behind the last comma
                                            if (num != NULL)
                                            {
                                                *num = 0;
                                                index = atoi(num + 1);
                                                if (strlen(s) < MAX_PATH)
                                                {
                                                    strcpy(path, s);
                                                    doExtractIcons = TRUE;
                                                    //                            TRACE_I("ExtractIcons for: " << nameAndData << "...");
                                                }
                                                else
                                                    TRACE_I("Too long filename to get icon from: " << s << ", " << index);
                                                *num = ',';
                                            }
                                            else
                                            {
                                                if (strlen(s) < MAX_PATH)
                                                {
                                                    strcpy(path, s);
                                                    doLoadImage = TRUE;
                                                    //                            TRACE_I("LoadImage for: " << nameAndData << "...");
                                                }
                                                else
                                                    TRACE_I("Too long filename to get icon from: " << s);
                                            }

                                            if (window->ICSleep)
                                                goto GO_SLEEP_MODE;
                                            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                                            if (waitBeforeFirstReadIcon)
                                            {
                                                waitBeforeFirstReadIcon = FALSE;
                                                //                          TRACE_I("Waiting 500ms before reading first icon in second round to have bigger chance to succeed.");
                                                Sleep(500); // let's take a moment here (before the second attempt to load the icon)
                                            }

                                            if (doExtractIcons)
                                            {
                                                // load an icon from a file (extracts from resources according to the index), icon-reader
                                                // may go into sleep mode during loading
                                                CALL_STACK_MESSAGE4("IconThreadThreadFBody::ExtractIcons(%s, %d, %d, ...)", path, index, IconSizes[iconSize]);
                                                if (ExtractIcons(path, index, IconSizes[iconSize], IconSizes[iconSize], &shi.hIcon, NULL, 1, IconLRFlags) != 1)
                                                {
                                                    TRACE_I("Unable to get icon from: " << path << ", " << index);
                                                    shi.hIcon = NULL;
                                                }
                                                //                          else
                                                //                            TRACE_I("ExtractIcons is done.");
                                            }

                                            if (doLoadImage)
                                            {
                                                {
                                                    // we will load an icon from a file (most likely .ico), icon-reader during loading
                                                    // can go to sleep mode
                                                    CALL_STACK_MESSAGE2("IconThreadThreadFBody::LoadImage(%s)", path);
                                                    shi.hIcon = (HICON)NOHANDLES(LoadImage(NULL, path, IMAGE_ICON, IconSizes[iconSize], IconSizes[iconSize],
                                                                                           LR_LOADFROMFILE | IconLRFlags));
                                                    //                            TRACE_I("LoadImage " << (shi.hIcon == NULL ? "has failed, now trying ExtractIcons..." : "is done."));
                                                }
                                                if (shi.hIcon == NULL) // It didn't work through LoadImage, let's try ExtractIcons instead (for example, an icon without an index from zipfldr.dll under XP: a .zip archive packed in a .7z archive)
                                                {
                                                    // Load the first icon from the file, the icon-reader can go into sleep mode during loading
                                                    CALL_STACK_MESSAGE3("IconThreadThreadFBody::ExtractIcons(%s, (0), %d, ...)", path, IconSizes[iconSize]);
                                                    if (ExtractIcons(path, 0, IconSizes[iconSize], IconSizes[iconSize], &shi.hIcon, NULL, 1, IconLRFlags) != 1)
                                                    {
                                                        TRACE_I("Unable to get first icon from: " << path);
                                                        shi.hIcon = NULL;
                                                    }
                                                    //                            else
                                                    //                              TRACE_I("ExtractIcons is done.");
                                                }
                                            }

                                            HANDLES(EnterCriticalSection(&window->ICSleepSection));
                                        }
                                        else // wanted == 4 or 6; loading thumbnails from the plug-in ("thumbnail loader")
                                        {
                                            shi.hIcon = NULL; // Prevention against incorrect deallocation of the icon (no memory leak occurs)

                                            char* s = iconData->NameAndData;
                                            int len = (int)strlen(s);
                                            int size = len + 4;
                                            size -= (size & 0x3); // size % 4 (alignment to four bytes)
                                            if (strlen(s) + (name - path) < MAX_PATH)
                                            {
                                                strcpy(name, s);

                                                //                          TRACE_I("Load thumbnail for: " << name << "...");
                                                CPluginInterfaceForThumbLoaderEncapsulation** loader;
                                                loader = (CPluginInterfaceForThumbLoaderEncapsulation**)(s + size + sizeof(CQuadWord) + sizeof(FILETIME));
                                                while (*loader != NULL)
                                                {
                                                    int thumbnailSize = window->GetThumbnailSize();
                                                    thumbMaker.Clear(thumbnailSize);
                                                    CALL_STACK_MESSAGE3("IconThreadThreadFBody::LoadThumbnail(%s, %d)", path, wanted == 4);
                                                    if ((*loader)->LoadThumbnail(path, thumbnailSize, thumbnailSize, &thumbMaker, wanted == 4))
                                                    {
                                                        thumbnailFlag = wanted == 4 /* first round of loading thumbnails*/ ? (thumbMaker.IsOnlyPreview() ? 6 /* low-quality/smaller*/ : 5 /* quality*/) : 5 /* in the second round, all obtained thumbnails are of good quality*/;
                                                        thumbMaker.HandleIncompleteImages();
                                                        break; // thumbnail may be loaded (in any case, do not try another plugin)
                                                    }
                                                    loader++; // Let's try another plugin in line, maybe it will load the thumbnail
                                                }
                                                if (*loader == NULL)
                                                    thumbMaker.Clear(); // failed thumbnail -> let's do a cleanup instead
                                                                        //                          TRACE_I("Load thumbnail is done.");
                                            }
                                            else
                                            {
                                                *name = 0;
                                                TRACE_I("Too long filename to get thumbnail from: " << path << s);
                                                thumbMaker.Clear();
                                            }
                                        }
                                    }

                                    if (window->ICSleep) // panel wants to go into sleep mode now
                                    {
                                        thumbMaker.Clear(); // thumbnail will no longer be needed

                                        // if it is not an icon from a plug-in that does not want the icon to be removed, we will remove the icon
                                        if (shi.hIcon != NULL && (!pluginFSIconsFromPlugin || destroyPluginIcon))
                                        {
                                            ::NOHANDLES(DestroyIcon(shi.hIcon));
                                        }
                                        goto GO_SLEEP_MODE;
                                    }

                                    if (wanted <= 3) // we were getting the icon
                                    {
                                        if (shi.hIcon == NULL)
                                            failed = TRUE;
                                        else
                                        {
                                            if (window->IconCache->GetIcon(iconData->GetIndex(),
                                                                           &iconList, &iconListIndex))
                                            {
                                                HANDLES(EnterCriticalSection(&window->ICSectionUsingIcon));

                                                iconList->ReplaceIcon(iconListIndex, shi.hIcon);
                                                iconData->SetFlag(1); // It is already loaded

                                                HANDLES(LeaveCriticalSection(&window->ICSectionUsingIcon));

                                                // find the index of the item we loaded the icon for

                                                if (pluginFSIconsFromPlugin) // pitFromPlugin: let the plugin compare items itself (must be a comparison without any two items in the listing matching)
                                                {
                                                    const CFileData* file = iconData->GetFSFileData();
                                                    if (file != NULL)
                                                    {
                                                        CPluginDataInterfaceEncapsulation* dataIface = &window->PluginData;
                                                        CFilesArray* arr = window->Dirs;
                                                        int z;
                                                        for (z = 0; z < arr->Count; z++)
                                                        {
                                                            if (dataIface->CompareFilesFromFS(file, &arr->At(z)) == 0)
                                                            {
                                                                PostMessage(window->HWindow, WM_USER_REFRESHINDEX, z, 0);
                                                                break;
                                                            }
                                                        }
                                                        if (z == window->Dirs->Count) // it was not a directory
                                                        {
                                                            arr = window->Files;
                                                            int j;
                                                            for (j = 0; j < arr->Count; j++)
                                                            {
                                                                if (dataIface->CompareFilesFromFS(file, &arr->At(j)) == 0)
                                                                {
                                                                    PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                                window->Dirs->Count + j, 0);
                                                                    break;
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                                else // no duplicate names are threatened (or are not an obstacle as in the case of archives, where
                                                {    // different icons cannot have the same names)
                                                    char* name2 = iconData->NameAndData;
                                                    CFilesArray* arr = window->Dirs;
                                                    int z;
                                                    for (z = 0; z < arr->Count; z++)
                                                    {
                                                        if (strcmp(name2, arr->At(z).Name) == 0)
                                                        {
                                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX, z, 0);
                                                            break;
                                                        }
                                                    }
                                                    if (z == window->Dirs->Count) // it was not a directory
                                                    {
                                                        arr = window->Files;
                                                        int j;
                                                        for (j = 0; j < arr->Count; j++)
                                                        {
                                                            if (strcmp(name2, arr->At(j).Name) == 0)
                                                            {
                                                                PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                            window->Dirs->Count + j, 0);
                                                                break;
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                            // if it is not an icon from a plug-in that does not want the icon to be removed, we will remove the icon
                                            if (!pluginFSIconsFromPlugin || destroyPluginIcon)
                                            {
                                                ::NOHANDLES(DestroyIcon(shi.hIcon));
                                            }
                                        }
                                    }
                                    else // we were getting the thumbnail
                                    {
                                        if (thumbMaker.ThumbnailReady())
                                        {
                                            CThumbnailData* thumbnailData;
                                            if (window->IconCache->GetThumbnail(iconData->GetIndex(),
                                                                                &thumbnailData))
                                            {
                                                BOOL thumbnailCreated = FALSE;

                                                HANDLES(EnterCriticalSection(&window->ICSectionUsingThumb));
                                                thumbMaker.TransformThumbnail();
                                                if (thumbMaker.RenderToThumbnailData(thumbnailData))
                                                {
                                                    iconData->SetFlag(thumbnailFlag); // It is already loaded
                                                    if (thumbnailFlag == 6 /* Low-quality/smaller thumbnail in the first round of loading thumbnails*/)
                                                        iconData->SetReadingDone(0); // the second round of reading will follow, so if it is not "done"
                                                    thumbnailCreated = TRUE;
                                                }
                                                HANDLES(LeaveCriticalSection(&window->ICSectionUsingThumb));

                                                if (thumbnailCreated)
                                                {
                                                    // find the index of the file (directories do not have thumbnails) to which we have loaded the thumbnail
                                                    char* name2 = iconData->NameAndData;
                                                    int z;
                                                    for (z = 0; z < window->Files->Count; z++)
                                                    {
                                                        if (strcmp(name2, window->Files->At(z).Name) == 0)
                                                        {
                                                            PostMessage(window->HWindow, WM_USER_REFRESHINDEX,
                                                                        window->Dirs->Count + z, 0);
                                                            break;
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                        thumbMaker.Clear(); // thumbnail will no longer be needed
                                    }
                                }
                                else
                                    callWaitForObjects = FALSE; // no work -> no delays
                            }
                        }
                        else
                        {
                            someNameSkipped = TRUE;     // skipped at least one name
                            callWaitForObjects = FALSE; // no work -> no delays
                        }
                    }
                    else
                    {
                        if (canReadIconOverlays && !readIconOverlaysNow)
                        { // now we are going to read icon overlays
                            i = 0;
                            readIconOverlaysNow = TRUE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                            continue;
                        }
                        else
                        {
                            readIconOverlaysNow = FALSE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                        }

                        if (!readOnlyVisibleItems && (selectMode == 2 || selectMode == 3))
                        {
                            i = 0;
                            selectMode++;
                            //                TRACE_I("selectMode=" << selectMode);
                            continue;
                        }

                        // The first round of reading icons is behind us, so all icon overlays are already loaded -> we prevent unnecessary attempts to read them again
                        canReadIconOverlays = FALSE;

                        // loading order: new icons, new thumbnails, old icons, old thumbnails
                        BOOL done = FALSE; // TRUE == break, we have already read
                        switch (wanted)
                        {
                        case 0: // We have already loaded the new icons
                        {
                            // if thumbnails are to be read and it is the first round of reading (plugins do not work)
                            // randomly like a system, so they won't load for the first time = they will never load), we will load
                            // new thumbnails (wanted == 4)
                            if (readThumbnails && firstRound)
                                wanted = 4;
                            else
                                wanted = 2; // otherwise we will restore (reload) old (retrieved) icons
                            break;
                        }

                        case 4: // We have already loaded the new thumbnails.
                        {
                            wanted = 2; // restore (reload) old (retrieved) icons
                            break;
                        }

                        case 2: // we have already loaded the old icons
                        {
                            if (readThumbnails && firstRound)
                                wanted = 6; // refresh (reload) old (retrieved + low quality/smaller) thumbnails
                            else
                                done = TRUE;
                            break;
                        }

                        default:
                            done = TRUE;
                            break;
                        }
                        if (done)
                            break; // done - wanted 0 and 2 or 0, 4, 2 and 6 or just 3 or -1 (error)

                        //              TRACE_I("wanted=" << wanted);

                        if (selectMode == 4)
                        {
                            i = 0;
                            selectMode = 2;
                            //                TRACE_I("selectMode=" << selectMode);
                            readIconOverlaysNow = FALSE;
                            //                TRACE_I("readIconOverlaysNow=" << readIconOverlaysNow);
                            continue;
                        }

                        i = -1;                     // so that 'i' reaches zero
                        callWaitForObjects = FALSE; // no work -> no delays
                    }

                    i++;
                    if (callWaitForObjects)
                    {
                        wait = WaitForMultipleObjects(2, handles, FALSE, 0);
                        // We will not ignore the "work" signal, because every "sleep->wake-up" means starting work from the beginning
                        if (wait != WAIT_TIMEOUT)
                            break; // Process the wait event
                    }
                    // else wait = WAIT_TIMEOUT;  // unnecessary, wait is already equal to WAIT_TIMEOUT
                }
                repeatedRound = FALSE;

                if (wait == WAIT_TIMEOUT && readOnlyVisibleItemsDueToUMI)
                { // Not all icons need to be loaded due to the priority provided for icons in the user menu (they are read before icons outside the visible area of the panel)
                    if (UserMenuIconBkgndReader.IsReadingIcons())
                    {
                        //              TRACE_I("Visible icons done, giving priority to usermenu icons...");
                        while (1)
                        {
                            if (window->ICSleep)
                                goto GO_SLEEP_MODE;
                            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

                            wait = WaitForMultipleObjects(2, handles, FALSE, 100); // give time for loading usermenu icons

                            HANDLES(EnterCriticalSection(&window->ICSleepSection));
                            if (window->ICSleep)
                                goto GO_SLEEP_MODE; // panel wants to go into sleep mode now

                            if (wait != WAIT_TIMEOUT)
                            {
                                //                  TRACE_I("Handling event...");
                                break; // Process the wait event
                            }
                            int visArrVer; // we will check if the visible area of the panel has changed, if so, we need to go read the icons
                            if (someNameSkipped && window->VisibleItemsArray.IsArrValid(&visArrVer) && visArrVer != lastVisArrVersion)
                            {
                                //                  TRACE_I("Change of visible items array...");
                                break;
                            }
                            if (!UserMenuIconBkgndReader.IsReadingIcons())
                            {
                                //                  TRACE_I("Usermenu icons done...");
                                break; // if the usermenu icons are already finished, we will read the icons in the panel
                            }
                        }
                    }
                    if (wait == WAIT_TIMEOUT) // there is a reason to repeat reading icons (change of visible area or completion of reading icons usermenu)
                    {
                        if (!UserMenuIconBkgndReader.IsReadingIcons()) // If the usermenu icons are ready, we will read icons outside the visible desktop
                        {
                            //                if (someNameSkipped) TRACE_I("Usermenu icons done, going to read the rest of icons in panel...");
                            readOnlyVisibleItems = FALSE;
                            readOnlyVisibleItemsDueToUMI = FALSE;
                        }
                        //              else
                        //                if (someNameSkipped) TRACE_I("Going to reread visible icons in panel...");
                        if (someNameSkipped)
                        {
                            repeatedRound = TRUE; // It's about an extra cycle (we don't want the icon overlays to be read again)
                            goto SECOND_ROUND;
                        }
                        //              else
                        //                TRACE_I("All items in panel are visible, so no reason to reread icons...");
                    }
                }

                if (wait == WAIT_TIMEOUT) // work is done -> inform the main thread
                {
                    if (window->Is(ptDisk) && failed && firstRound)
                    {                                   // Let's do it again (not all icons were successful)
                        firstRound = FALSE;             // just one extra lap
                        waitBeforeFirstReadIcon = TRUE; // to prevent the icon from being read immediately again (low chance of success)
                                                        //              TRACE_I("Going to second round of reading (some icons have not been read in the first round).");
                        goto SECOND_ROUND;
                        // postRefresh = TRUE;
                    }
                    else
                        firstRound = TRUE;

                    //            TRACE_I("Stop reading.");
                    // Send a notification about finishing loading icons in the panel
                    if (window->HWindow == NULL ||
                        !PostMessage(window->HWindow, WM_USER_ICONREADING_END, 0, 0))
                    { // something went wrong ("always false"), let's set IconCacheValid = TRUE here
                        window->IconCacheValid = TRUE;
                    }

                    //            if (window->HWindow != NULL)  // prubezne prekreslovani staci
                    //              InvalidateRect(window->HWindow, NULL, TRUE);
                }
                else
                {

                GO_SLEEP_MODE:

                    // interrupt (sleep-icon-cache-thread or new job or terminate)
                    firstRound = TRUE;
                    //            TRACE_I("Reading terminated.");
                }

                window->ICWorking = FALSE;
            }

            window->ICSleep = FALSE;
            HANDLES(LeaveCriticalSection(&window->ICSleepSection));

            /*    // replaced with goto SECOND_ROUND (freezes on network drive when reading the entire content again)
        if (postRefresh)  // moved Sleep(500) out of critical section - unnecessarily stiff ...
        {
          HANDLES(EnterCriticalSection(&TimeCounterSection));  // take the time when refresh is needed
          int t1 = MyTimeCounter++;
          HANDLES(LeaveCriticalSection(&TimeCounterSection));
          Sleep(500);  // a moment to breathe
          PostMessage(window->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        }*/

            break;
        }

        default: // terminate
        {
            run = FALSE;
            break;
        }
        }
    }

    ShellIconOverlays.ReleaseIconReadersIconOverlayIds(iconReadersIconOverlayIds);

    OleUninitialize();

    TRACE_I("End");
    return 0;
}

unsigned IconThreadThreadFEH(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return IconThreadThreadFBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread IconReader: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // tvrdší exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI IconThreadThreadF(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return IconThreadThreadFEH(param);
}

CFilesWindow::CFilesWindow(CMainWindow* parent)
    : Columns(20, 10), ColumnsTemplate(20, 10), VisibleItemsArray(FALSE), VisibleItemsArraySurround(TRUE)
{
    CALL_STACK_MESSAGE1("CFilesWindow::CFilesWindow()");
    NarrowedNameColumn = FALSE;
    FullWidthOfNameCol = 0;
    WidthOfMostOfNames = 0;
    ColumnsTemplateIsForDisk = FALSE; // just zeroing, it will be set in BuildColumnsTemplate()
    StopThumbnailLoading = FALSE;
    UserWorkedOnThisPath = FALSE;

    UnpackedAssocFiles.SetPanel(this);
    QuickRenameWindow.SetPanel(this);

    FilesMap.SetPanel(this);
    ScrollObject.SetPanel(this);
    HiddenDirsFilesReason = 0;
    HiddenDirsCount = HiddenFilesCount = 0;
    IconCacheValid = FALSE;
    InactWinOptimizedReading = FALSE;
    WaitBeforeReadingIcons = 0;
    WaitOneTimeBeforeReadingIcons = 0;
    EndOfIconReadingTime = GetTickCount() - 10000;
    ICEventTerminate = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    ICEventWork = HANDLES(CreateEvent(NULL, FALSE, FALSE, NULL));
    ICSleep = FALSE;
    ICWorking = FALSE;
    ICStopWork = FALSE;
    HANDLES(InitializeCriticalSection(&ICSleepSection));
    HANDLES(InitializeCriticalSection(&ICSectionUsingIcon));
    HANDLES(InitializeCriticalSection(&ICSectionUsingThumb));
    DWORD ThreadID;
    IconCacheThread = NULL;
    if (ICEventTerminate != NULL && ICEventWork != NULL)
        IconCacheThread = HANDLES(CreateThread(NULL, 0, IconThreadThreadF, this, 0, &ThreadID));
    if (ICEventTerminate == NULL ||
        ICEventWork == NULL ||
        IconCacheThread == NULL)
    {
        TRACE_E("Unable to start icon-reader thread.");
        IconCache = NULL;
    }
    else
    {
        //    SetThreadPriority(IconCacheThread, THREAD_PRIORITY_IDLE); // loading doesn't work afterwards
        IconCache = new CIconCache();
    }

    OpenedDrivesList = NULL;

    Parent = parent;
    ViewTemplate = &parent->ViewTemplates.Items[2]; // detailed view
    BuildColumnsTemplate();
    CopyColumnsTemplateToColumns();
    ListBox = NULL;
    StatusLine = NULL;
    DirectoryLine = NULL;
    StatusLineVisible = TRUE;
    DirectoryLineVisible = TRUE;
    HeaderLineVisible = TRUE;

    SortType = stName;
    ReverseSort = FALSE;
    SortedWithRegSet = FALSE;    // The initial state does not matter, it will be set in the SortDirectory() method
    SortedWithDetectNum = FALSE; // The initial state does not matter, it will be set in the SortDirectory() method
    LastFocus = INT_MAX;
    SetValidFileData(VALID_DATA_ALL);
    AutomaticRefresh = TRUE;
    NextFocusName[0] = 0;
    DontClearNextFocusName = FALSE;
    LastRefreshTime = 0;
    FilesActionInProgress = FALSE;
    CanDrawItems = TRUE;
    FileBasedCompression = FALSE;
    FileBasedEncryption = FALSE;
    SupportACLS = FALSE;
    DeviceNotification = NULL;
    ContextMenu = NULL;
    ContextSubmenuNew = new CMenuNew;
    UseSystemIcons = FALSE;
    UseThumbnails = FALSE;
    NeedRefreshAfterEndOfSM = FALSE;
    RefreshAfterEndOfSMTime = 0;
    PluginFSNeedRefreshAfterEndOfSM = FALSE;
    SmEndNotifyTimerSet = FALSE;
    RefreshDirExTimerSet = FALSE;
    RefreshDirExLParam = 0;
    InactiveRefreshTimerSet = FALSE;
    InactRefreshLParam = 0;
    LastInactiveRefreshStart = LastInactiveRefreshEnd = 0;

    NeedRefreshAfterIconsReading = FALSE;
    RefreshAfterIconsReadingTime = 0;

    PathHistory = new CPathHistory();

    DontDrawIndex = -1;
    DrawOnlyIndex = -1;

    FocusFirstNewItem = FALSE;

    ExecuteAssocEvent = HANDLES(CreateEvent(NULL, TRUE, FALSE, NULL));
    AssocUsed = FALSE;

    FilterEnabled = FALSE;
    Filter.SetMasksString("*.*");
    int errPos;
    Filter.PrepareMasks(errPos);

    QuickSearchMode = FALSE;
    CaretHeight = 1; // dummy
    QuickSearch[0] = 0;
    QuickSearchMask[0] = 0;
    SearchIndex = INT_MAX;
    FocusedIndex = 0;
    FocusVisible = FALSE;

    DropTargetIndex = -1;
    SingleClickIndex = -1;
    SingleClickAnchorIndex = -1;
    GetCursorPos(&OldSingleClickMousePos);

    TrackingSingleClick = FALSE;
    DragBox = FALSE;
    DragBoxVisible = FALSE;
    ScrollingWindow = FALSE;

    SkipCharacter = FALSE;
    SkipSysCharacter = FALSE;

    //  ShiftSelect = FALSE;
    DragSelect = FALSE;
    BeginDragDrop = FALSE;
    DragDropLeftMouseBtn = FALSE;
    BeginBoxSelect = FALSE;
    PersistentTracking = FALSE;

    TrackingSingleClick = 0;

    CutToClipChanged = FALSE;

    PerformingDragDrop = FALSE;

    GetPluginIconIndex = InternalGetPluginIconIndex;

    EnumFileNamesSourceUID = -1;

    TemporarilySimpleIcons = FALSE;
    NumberOfItemsInCurDir = 0;

    NeedIconOvrRefreshAfterIconsReading = FALSE;
    LastIconOvrRefreshTime = GetTickCount() - ICONOVR_REFRESH_PERIOD;
    IconOvrRefreshTimerSet = FALSE;
}

CFilesWindow::~CFilesWindow()
{
    CALL_STACK_MESSAGE1("CFilesWindow::~CFilesWindow()");

    if (DeviceNotification != NULL)
        TRACE_E("CFilesWindow::~CFilesWindow(): unexpected situation: DeviceNotification != NULL");

    ClearHistory();

    if (PathHistory != NULL)
        delete PathHistory;

    if (IconCacheThread != NULL)
    {
        SetEvent(ICEventTerminate); // Load icons terminate now!
        if (WaitForSingleObject(IconCacheThread, 1000) == WAIT_TIMEOUT)
        { // Wait a second for a clean exit, then force kill (window will deallocate)
            TRACE_E("Terminating Icon Thread");
            TerminateThread(IconCacheThread, 666);
            WaitForSingleObject(IconCacheThread, INFINITE); // Wait until the thread actually finishes, sometimes it takes quite a while
        }
        HANDLES(CloseHandle(IconCacheThread));
    }

    HANDLES(DeleteCriticalSection(&ICSectionUsingThumb));
    HANDLES(DeleteCriticalSection(&ICSectionUsingIcon));
    HANDLES(DeleteCriticalSection(&ICSleepSection));
    if (ICEventTerminate != NULL)
        HANDLES(CloseHandle(ICEventTerminate));
    if (ICEventWork != NULL)
        HANDLES(CloseHandle(ICEventWork));

    if (IconCache != NULL)
        delete IconCache;
    if (ContextSubmenuNew != NULL)
        delete ContextSubmenuNew;
    if (ExecuteAssocEvent != NULL)
        HANDLES(CloseHandle(ExecuteAssocEvent));
}

void CFilesWindow::ClearHistory()
{
    if (PathHistory != NULL)
        PathHistory->ClearHistory();

    OldSelection.Clear();
}

void CFilesWindow::SleepIconCacheThread()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SleepIconCacheThread()");
    ICSleep = TRUE;          // for interrupting the loop of loading icons (ICSleepSection may not leave at all)
    ICStopWork = TRUE;       // for interrupting the loop of loading icons if ICStopWork has already been processed
    ResetEvent(ICEventWork); // for interrupting the loop of loading icons if ICStopWork has not been processed yet
    // Wait until the icon-reader enters the part where it can switch to sleep mode
    HANDLES(EnterCriticalSection(&ICSleepSection));
    ICSleep = ICWorking; // TRUE only if icon-reader is hanging in SHGetFileInfo
    HANDLES(LeaveCriticalSection(&ICSleepSection));
}

void CFilesWindow::WakeupIconCacheThread()
{
    CALL_STACK_MESSAGE_NONE
    ICStopWork = FALSE;    // so that work is not interrupted right at the beginning
    SetEvent(ICEventWork); // Switch to work mode, we don't expect a response
    MSG msg;               // we need to handle the possible WM_USER_ICONREADING_END, which would set IconCacheValid = TRUE
    while (PeekMessage(&msg, HWindow, WM_USER_ICONREADING_END, WM_USER_ICONREADING_END, PM_REMOVE))
        ;
}

BOOL CFilesWindow::CheckAndRestorePath(const char* path)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CheckAndRestorePath(%s)", path);

    // We will not test network paths if we have not just accessed them.
    BOOL tryNet = (!Is(ptDisk) && !Is(ptZIPArchive)) || !HasTheSameRootPath(path, GetPath());

    return SalCheckAndRestorePath(HWindow, path, tryNet);
}

BOOL CFilesWindow::CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin)
{
    CALL_STACK_MESSAGE1("CFilesWindow::CanUnloadPlugin()");

    if (Is(ptDisk))
    {
        if (UseThumbnails && // thumbnails are being loaded
            !IconCacheValid) // icon-reader has not finished loading yet
        {
            CPluginData* p = Plugins.GetPluginData(plugin);
            if (p != NULL) // "always true"
            {
                if (p->ThumbnailMasks.GetMasksString()[0] != 0)
                { // It's a plugin that provides thumbnails - we're not sure if it does
                    // this panel, but it cannot be excluded, so we have to stop reading icons
                    SleepIconCacheThread();
                    p->ThumbnailMasksDisabled = TRUE; // During the unload/remove of the plugin, this plugin cannot be used to load thumbnails
                    StopThumbnailLoading = TRUE;      // in case WakeupIconCacheThread is called from somewhere (data about "thumbnail loaders" in icon cache cannot be used)
                    UseThumbnails = FALSE;            // to prevent unwanted wake-up of the icon-reader (calling WakeupIconCacheThread())
                    if (!CriticalShutdown)
                    {
                        HANDLES(EnterCriticalSection(&TimeCounterSection));
                        int t1 = MyTimeCounter++;
                        HANDLES(LeaveCriticalSection(&TimeCounterSection));
                        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1); // We will take care of the new icon-cache filling (ideally it will happen after unloading/removing the plugin).
                    }
                }
            }
            else
                TRACE_E("CFilesWindow::CanUnloadPlugin(): Unexpected situation!");
        }
    }
    else
    {
        BOOL used = FALSE;
        if ((Is(ptZIPArchive) || Is(ptPluginFS)) &&
            PluginData.NotEmpty() && PluginData.GetPluginInterface() == plugin)
            used = TRUE;
        else
        { // FS does not have to use PluginData, so we still need to test PluginFS
            if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
                GetPluginFS()->GetPluginInterface() == plugin)
                used = TRUE;
            else
            {
                if (Is(ptZIPArchive))
                { // The archive does not have to use PluginData, so we still need to test the associations of the archive
                    // This part is important only for terminating the Salamander - otherwise the plug-in would be calm
                    // could be unloaded during the use of the archiver (each archiver function loads its plug-in)
                    // WARNING: The exception are icon overlays from the plugin, they would stop being drawn after unloading the plugin
                    //        (when unloading the plugin, we release its array of icon-overlays)
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0) // We found a supported archive
                    {
                        format--;
                        CPluginData* data;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0) // view: is it internal processing (plug-in)?
                        {
                            data = Plugins.Get(-index - 1);
                            if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                                used = TRUE;
                        }
                        if (PackerFormatConfig.GetUsePacker(format)) // edit?
                        {
                            index = PackerFormatConfig.GetPackerIndex(format);
                            if (index < 0) // Is it about internal processing (plug-in)?
                            {
                                data = Plugins.Get(-index - 1);
                                if (data != NULL && data->GetPluginInterface()->GetInterface() == plugin)
                                    used = TRUE;
                            }
                        }
                    }
                }
            }
        }
        if (used)
        {
            if (Is(ptZIPArchive) || Is(ptPluginFS)) // archive -> we just leave it; FS -> we return to the last disk path
            {
                char path[MAX_PATH];
                strcpy(path, GetPath());

                DWORD err, lastErr;
                BOOL pathInvalid, cut;
                BOOL tryNet = FALSE; // no more unnecessary delays with the network...
                if (SalCheckAndRestorePathWithCut(HWindow, path, tryNet, err, lastErr, pathInvalid, cut, TRUE))
                { // Switch to a path that should load without any issues
                    ChangePathToDisk(parent, path, -1, NULL, NULL, TRUE, TRUE, FALSE, NULL, FALSE, FSTRYCLOSE_UNLOADCLOSEFS);
                }
                else // original path (nor its subpath) is not accessible -> we are going to fixed-drive (cannot be called
                     // directly ChangePathToDisk, because otherwise an error is displayed - for example, "X: not ready")
                {
                    ChangeToRescuePathOrFixedDrive(parent, NULL, TRUE, TRUE, FSTRYCLOSE_UNLOADCLOSEFS);
                }
                if (!Is(ptDisk))
                {
                    return FALSE; // Changing the path to the disk failed, unload is not possible
                }
            }
        }
    }
    return TRUE;
}

void CFilesWindow::RedrawFocusedIndex()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RedrawFocusedIndex()");
    RedrawIndex(FocusedIndex);
}

void CFilesWindow::DirectoryLineSetText()
{
    CALL_STACK_MESSAGE1("CFilesWindow::DirectoryLineSetText()");
    char ZIPbuf[2 * MAX_PATH];
    const char* path = NULL;
    if (Is(ptZIPArchive))
    {
        strcpy(ZIPbuf, GetZIPArchive());
        if (GetZIPPath()[0] != 0)
        {
            if (GetZIPPath()[0] != '\\')
                strcat(ZIPbuf, "\\");
            strcat(ZIPbuf, GetZIPPath());
        }
        path = ZIPbuf;
        PathHistory->AddPath(1, GetZIPArchive(), GetZIPPath(), NULL, NULL);
    }
    else
    {
        if (Is(ptDisk))
        {
            PathHistory->AddPath(0, GetPath(), NULL, NULL, NULL);
            path = GetPath();
        }
        else
        {
            if (Is(ptPluginFS))
            {
                int l = (int)strlen(GetPluginFS()->GetPluginFSName());
                memcpy(ZIPbuf, GetPluginFS()->GetPluginFSName(), l);
                ZIPbuf[l++] = ':';
                if (!GetPluginFS()->NotEmpty() || !GetPluginFS()->GetCurrentPath(ZIPbuf + l))
                    ZIPbuf[l] = 0;
                else
                {
                    PathHistory->AddPath(2, GetPluginFS()->GetPluginFSName(), ZIPbuf + l,
                                         GetPluginFS()->GetInterface(), GetPluginFS());
                }
                path = ZIPbuf;
            }
        }
    }

    if (path == NULL)
        return;

    if (FilterEnabled)
    {
        char buf[3 * MAX_PATH]; // zip path (2x) + filter (1x) = 3x MAX_PATH
        int pathLen = (int)strlen(path);
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            int l = pathLen;
            memcpy(buf, path, l);
            if (buf[l - 1] != '\\')
                buf[l++] = '\\';
            lstrcpyn(buf + l, Filter.GetMasksString(), MAX_PATH);
        }
        else
        {
            if (Is(ptPluginFS))
            {
                int l = pathLen;
                memcpy(buf, path, l);
                buf[l++] = ':';
                //        if (FilterInverse) buf[l++] = '-';
                lstrcpyn(buf + l, Filter.GetMasksString(), MAX_PATH);
            }
        }
        DirectoryLine->SetText(buf, pathLen);
    }
    else
    {
        DirectoryLine->SetText(path);
    }
}

void CFilesWindow::SelectUnselect(BOOL forceIncludeDirs, BOOL select, BOOL showMaskDlg)
{
    CALL_STACK_MESSAGE4("CFilesWindow::SelectUnselect(%d, %d, %d)", forceIncludeDirs, select, showMaskDlg);
    if (showMaskDlg)
    {
        BeginStopRefresh(); // He was snoring in his sleep
    }
    if (!showMaskDlg || CSelectDialog(HLanguage, select ? IDD_SELECTMASK : IDD_DESELECTMASK,
                                      select ? IDD_SELECTMASK : IDD_DESELECTMASK /* helpID*/,
                                      HWindow, MainWindow->SelectionMask)
                                .Execute() == IDOK)
    {
        BOOL includeDirs = Configuration.IncludeDirs | forceIncludeDirs;
        const char* maskStr = showMaskDlg ? MainWindow->SelectionMask : "*.*";
        CMaskGroup mask(maskStr);
        int err;
        if (mask.PrepareMasks(err))
        {
            int dirsCount = Dirs->Count;
            int count = dirsCount + Files->Count;
            int start;
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                start = 1;
            else
                start = 0;
            int i = includeDirs ? start : Dirs->Count;
            BOOL changed = FALSE;
            for (; i < count; i++)
            {
                CFileData* d = (i < dirsCount) ? &Dirs->At(i) : &Files->At(i - dirsCount);
                if (!showMaskDlg || mask.AgreeMasks(d->Name, i < dirsCount ? NULL : d->Ext)) // in case *.* we will not call agree mask
                {
                    SetSel(select, d);
                    changed = TRUE;
                }
            }
            if (changed)
            {
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            }
            else
                SalMessageBox(HWindow, LoadStr(IDS_NOMATCHESFOUND), LoadStr(IDS_INFOTITLE),
                              MB_OK | MB_ICONINFORMATION);
        }
    }
    if (showMaskDlg)
    {
        UpdateWindow(MainWindow->HWindow);
        EndStopRefresh(); // now he's sniffling again, he'll start up
    }
}

void CFilesWindow::InvertSelection(BOOL forceIncludeDirs)
{
    CALL_STACK_MESSAGE2("CFilesWindow::InvertSelection(%d)", forceIncludeDirs);
    BOOL includeDirs = Configuration.IncludeDirs | forceIncludeDirs;
    int count = GetSelCount();
    int firstIndex = 0;
    if (includeDirs)
    {
        if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
            firstIndex = 1;
    }
    else
    {
        firstIndex = Dirs->Count;
    }

    int lastIndex = Dirs->Count + Files->Count - 1;
    if (firstIndex <= lastIndex)
    {
        int i;
        for (i = firstIndex; i <= lastIndex; i++)
        {
            CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            SetSel(item->Selected != 1, item);
        }
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
    }
}

void CFilesWindow::SelectUnselectByFocusedItem(BOOL select, BOOL byName)
{
    CALL_STACK_MESSAGE3("CFilesWindow::SelectUnselectByFocusedItem(%d, %d)", select, byName);
    if (FocusedIndex >= 0 && FocusedIndex < Dirs->Count + Files->Count)
    {

        //    if (!byName && FocusedIndex < Dirs->Count)
        //    {
        //
        //    }

        BOOL isDir = FocusedIndex < Dirs->Count;
        const CFileData* focusedItem = isDir ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);

        int firstIndex = 0;
        if (Configuration.IncludeDirs)
        {
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                firstIndex = 1;
        }
        else
        {
            firstIndex = Dirs->Count;
        }
        int lastIndex = Dirs->Count + Files->Count - 1;
        int lastSelectdCount = SelectedCount;
        const char* focusedStr = byName ? focusedItem->Name : (isDir ? "" : focusedItem->Ext);
        int focusedLen = byName ? (isDir ? focusedItem->NameLen : (int)(focusedItem->Ext - focusedItem->Name)) : (isDir ? 0 : (int)lstrlen(focusedItem->Ext));
        if (!isDir && byName && *focusedItem->Ext != 0)
            focusedLen--; // skip '.'
        int i;
        for (i = firstIndex; i <= lastIndex; i++)
        {
            BOOL itemIsDir = i < Dirs->Count;
            CFileData* item = itemIsDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            const char* str = byName ? item->Name : (itemIsDir ? "" : item->Ext);
            int len = byName ? (itemIsDir ? item->NameLen : (int)(item->Ext - item->Name)) : (itemIsDir ? 0 : (int)lstrlen(item->Ext));
            if (!itemIsDir && byName && *item->Ext != 0)
                len--; // skip '.'
            if (len == focusedLen && StrNICmp(str, focusedStr, len) == 0)
                SetSel(select, item);
        }
        if (SelectedCount != lastSelectdCount)
        {
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
    }
}

void CFilesWindow::StoreGlobalSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::StoreGlobalSelection()");
    int count = GetSelCount();
    if (count != 0)
    {
        BeginStopRefresh(); // He was snoring in his sleep

        BOOL clipboard = FALSE;
        CSaveSelectionDialog dlg(HWindow, &clipboard);
        if (dlg.Execute() == IDOK)
        {
            int totalCount = Dirs->Count + Files->Count;
            if (clipboard)
            {
                // we need to put the list on the clipboard

                // calculate the required size of the buffer (name1CRLFname2CRLF...nameNCRLF)
                DWORD size = 0;
                int i;
                for (i = 0; i < totalCount; i++)
                {
                    CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                    if (f->Selected)
                        size += f->NameLen + 2; // nameCRLF
                }
                if (size > 0)
                {
                    char* buff = (char*)malloc(size);
                    if (buff != NULL)
                    {
                        char* p = buff;
                        for (i = 0; i < totalCount; i++)
                        {
                            CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                            if (f->Selected)
                            {
                                memcpy(p, f->Name, f->NameLen);
                                p += f->NameLen;
                                memcpy(p, "\r\n", 2);
                                p += 2;
                            }
                        }
                        CopyTextToClipboard(buff, size);
                        free(buff);
                    }
                    else
                        TRACE_E(LOW_MEMORY);
                }
            }
            else
            {
                // we need to put the list into GlobalSelection
                GlobalSelection.Clear();
                int i;
                for (i = 0; i < totalCount; i++)
                {
                    CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                    if (f->Selected)
                    {
                        if (!GlobalSelection.Add(i < Dirs->Count, f->Name))
                            break; // low memory
                    }
                }
                GlobalSelection.Sort();
            }
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
        }
        UpdateWindow(MainWindow->HWindow);

        EndStopRefresh(); // now he's sniffling again, he'll start up
    }
}

void CFilesWindow::RestoreGlobalSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RestoreGlobalSelection()");

    BOOL clipboardValid = IsTextOnClipboard();
    BOOL globalValid = GlobalSelection.GetCount() > 0;
    if (clipboardValid || globalValid)
    {
        BeginStopRefresh(); // He was snoring in his sleep

        CLoadSelectionOperation operation = lsoCOPY;
        BOOL clipboard = !globalValid;
        CLoadSelectionDialog dlg(HWindow, &operation, &clipboard, clipboardValid, globalValid);
        if (dlg.Execute() == IDOK)
        {
            CNames* selection = &GlobalSelection;
            CNames clipboardSelection;
            if (clipboard)
            {
                clipboardSelection.LoadFromClipboard(HWindow);
                clipboardSelection.Sort();
                selection = &clipboardSelection;
            }

            int count = Files->Count + Dirs->Count;
            int i;
            for (i = 0; i < count; i++)
            {
                BOOL isDir = i < Dirs->Count;
                CFileData* file = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (clipboard)
                    isDir = FALSE; // if we go through the clipboard, everything is in Files
                switch (operation)
                {
                case lsoCOPY:
                {
                    SetSel(selection->Contains(isDir, file->Name), file);
                    break;
                }

                case lsoOR:
                {
                    if (selection->Contains(isDir, file->Name))
                        SetSel(TRUE, file);
                    break;
                }

                case lsoDIFF:
                {
                    if (file->Selected)
                        SetSel(!selection->Contains(isDir, file->Name), file);
                    break;
                }

                case lsoAND:
                {
                    SetSel(file->Selected && selection->Contains(isDir, file->Name), file);
                    break;
                }

                default:
                {
                    TRACE_E("Unknown operation: " << operation);
                    break;
                }
                }
            }
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        }
        UpdateWindow(MainWindow->HWindow);
        EndStopRefresh(); // now he's sniffling again, he'll start up
    }
}

void CFilesWindow::StoreSelection()
{
    CALL_STACK_MESSAGE1("CFilesWindow::StoreSelection()");
    OldSelection.Clear();
    int count = GetSelCount();
    if (count != 0)
    {
        OldSelection.SetCaseSensitive(IsCaseSensitive());
        int totalCount = Files->Count + Dirs->Count;
        int i;
        for (i = 0; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (f->Selected)
            {
                if (!OldSelection.Add(isDir, f->Name))
                    break; // low memory
            }
        }
        OldSelection.Sort();
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
    }
}

void CFilesWindow::Reselect()
{
    CALL_STACK_MESSAGE1("CFilesWindow::Reselect()");
    int count = Files->Count + Dirs->Count;
    int i;
    for (i = 0; i < count; i++)
    {
        BOOL isDir = i < Dirs->Count;
        CFileData* file = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        if (OldSelection.Contains(isDir, file->Name))
            SetSel(TRUE, file);
        else
            SetSel(FALSE, file);
    }
    RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
}

void CFilesWindow::ShowHideNames(int mode)
{
    BOOL refreshPanel = FALSE;
    switch (mode)
    {
    case 0: // show all
    {
        if (HiddenNames.GetCount() > 0)
        {
            HiddenNames.Clear();
            refreshPanel = TRUE;
        }
        break;
    }

    case 1: // hide selected names
    {
        int count = GetSelCount();
        if (count > 0)
        {
            int totalCount = Files->Count + Dirs->Count;
            int startIndex = 0;
            if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0) // We do not want ".." in the array
                startIndex = 1;
            int i;
            for (i = 0; i < totalCount; i++)
            {
                BOOL isDir = i < Dirs->Count;
                CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                if (f->Selected)
                {
                    if (!HiddenNames.Add(isDir, f->Name))
                        break; // low_memory, we will not continue
                    refreshPanel = TRUE;
                }
            }
        }
        break;
    }

    case 2: // hide unselected name
    {
        int totalCount = Files->Count + Dirs->Count;
        int startIndex = 0;
        if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0) // We do not want ".." in the array
            startIndex = 1;
        int i;
        for (i = startIndex; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = (isDir) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (!f->Selected)
            {
                if (!HiddenNames.Add(isDir, f->Name))
                    break; // low_memory, we will not continue
                refreshPanel = TRUE;
            }
        }
        break;
    }

    default:
    {
        TRACE_E("ShowHideNames: unknown mode=" << mode);
    }
    }

    if (refreshPanel)
    {
        if (mode == 1 || mode == 2)
            HiddenNames.SetCaseSensitive(IsCaseSensitive());
        HiddenNames.Sort();
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
    }
}

void CFilesWindow::SetAutomaticRefresh(BOOL value, BOOL force)
{
    CALL_STACK_MESSAGE_NONE
    if (force || AutomaticRefresh != value)
    {
        AutomaticRefresh = value;
        /* // "vyhozeni" refresh tag from dir-line
    // it was crashing here; called on a destroyed object
    if (DirectoryLine != NULL)                       
      DirectoryLine->SetAutomatic(AutomaticRefresh);*/
    }
}

void CFilesWindow::GotoRoot()
{
    CALL_STACK_MESSAGE1("CFilesWindow::GotoRoot()");
    TopIndexMem.Clear(); // long jump

    char root[MAX_PATH];
    if (Is(ptDisk) || Is(ptZIPArchive))
    {
        if (Is(ptZIPArchive) && GetZIPPath()[0] != 0) // we are not in the root of the archive -> let's go into it
        {
            ChangePathToArchive(GetZIPArchive(), "");
        }
        else // go to the root of the Windows path
        {
            if (IsUNCRootPath(GetPath()) && Plugins.GetFirstNethoodPluginFSName(root))
            {
                ChangePathToPluginFS(root, "");
            }
            else
            {
                GetRootPath(root, GetPath());
                if (root[0] == '\\')
                    root[strlen(root) - 1] = 0; // UNC will not end with '\\'
                ChangePathToDisk(HWindow, root);
            }
        }
    }
    else
    {
        if (Is(ptPluginFS))
        {
            if (GetPluginFS()->GetRootPath(root))
            {
                char fsname[MAX_PATH];
                strcpy(fsname, GetPluginFS()->GetPluginFSName()); // in case of change, local copy of the name
                ChangePathToPluginFS(fsname, root);
            }
        }
    }
}

void CFilesWindow::GotoHotPath(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::GotoHotPath(%d)", index);
    if (index < 0 || index >= HOT_PATHS_COUNT)
        return;
    //--- switch to hot-path
    char path[2 * MAX_PATH];
    if (MainWindow->GetExpandedHotPath(HWindow, index, path, 2 * MAX_PATH))
        ChangeDir(path);
}

void CFilesWindow::SetUnescapedHotPath(int index)
{
    CALL_STACK_MESSAGE2("CFilesWindow::SetUnescapedHotPath(%d)", index);
    if (index < 0 || index >= HOT_PATHS_COUNT)
        return;
    char path[2 * MAX_PATH];
    GetGeneralPath(path, 2 * MAX_PATH, TRUE);
    MainWindow->SetUnescapedHotPath(index, path);
}

BOOL CFilesWindow::SetUnescapedHotPathToEmptyPos()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SetUnescapedHotPathToEmptyPos()");
    int index = MainWindow->GetUnassignedHotPathIndex();
    if (index != -1)
    {
        char path[2 * MAX_PATH];
        GetGeneralPath(path, 2 * MAX_PATH, TRUE);
        MainWindow->SetUnescapedHotPath(index, path);
        return TRUE;
    }
    return FALSE;
}

#ifndef _WIN64

BOOL AreNextPathComponents(const char* relPath, const char* nextComp)
{
    int len = (int)strlen(nextComp);
    return StrNICmp(relPath, nextComp, len) == 0 && (relPath[len] == '\\' || relPath[len] == 0);
}

#endif // _WIN64

void CFilesWindow::OpenActiveFolder()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OpenActiveFolder()");
    if (Is(ptDisk) && CheckPath(TRUE) != ERROR_USER_TERMINATED)
    {
        UserWorkedOnThisPath = TRUE;
        const char* path = GetPath();

#ifndef _WIN64
        // we will replace "C:\\Windows\\sysnative\\*" with "C:\\Windows\\system32\\*", 64-bit
        // proces Exploreru o "sysnative" nic nevi, nebudeme s tim otravovat lidi,
        // at the same time, we will replace "C:\\Windows\\system32\\*" with "C:\\Windows\\SysWOW64\\*"
        // (except for the group of directories excluded from the redirector, which are redirected back to System32)
        char dirName[MAX_PATH];
        dirName[0] = 0;
        if (Windows64Bit && WindowsDirectory[0] != 0)
        {
            BOOL done = FALSE;
            lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
            if (SalPathAppend(dirName, "Sysnative", MAX_PATH))
            {
                int len = (int)strlen(dirName);
                if (StrNICmp(path, dirName, len) == 0 && (path[len] == '\\' || path[len] == 0))
                {
                    lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                    SalPathAppend(dirName, "System32", MAX_PATH); // When Sysnative is happy, System32 fits in too
                    memmove(dirName + strlen(dirName), path + len, strlen(path + len) + 1);
                    path = dirName;
                    done = TRUE;
                }
            }
            if (!done)
            {
                lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                if (SalPathAppend(dirName, "System32", MAX_PATH))
                {
                    int len = (int)strlen(dirName);
                    if (StrNICmp(path, dirName, len) == 0 && (path[len] == '\\' || path[len] == 0))
                    {
                        // Check if it is not a directory excluded from the redirector
                        if (path[len] == '\\' &&
                            (AreNextPathComponents(path + len + 1, "catroot") ||
                             AreNextPathComponents(path + len + 1, "catroot2") ||
                             Windows7AndLater && AreNextPathComponents(path + len + 1, "DriverStore") ||
                             AreNextPathComponents(path + len + 1, "drivers\\etc") ||
                             AreNextPathComponents(path + len + 1, "LogFiles") ||
                             AreNextPathComponents(path + len + 1, "spool")))
                        {
                            done = TRUE;
                        }
                        if (!done)
                        {
                            lstrcpyn(dirName, WindowsDirectory, MAX_PATH);
                            SalPathAppend(dirName, "SysWOW64", MAX_PATH); // When System32 is happy, SysWOW64 fits in too
                            memmove(dirName + strlen(dirName), path + len, strlen(path + len) + 1);
                            path = dirName;
                        }
                    }
                }
            }
        }
#endif // _WIN64

        char itemName[MAX_PATH];
        itemName[0] = 0;
        if (FocusedIndex < Dirs->Count + Files->Count)
        {
            CFileData* item = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);
            // hack for people who need to focus on the Unicode name in Explorer, let's try it via short name
            AlterFileName(itemName, item->DosName != NULL ? item->DosName : item->Name, -1, Configuration.FileNameFormat, 0, FocusedIndex < Dirs->Count);
            if (FocusedIndex < Dirs->Count && FocusedIndex == 0 && strcmp(itemName, "..") == 0)
                itemName[0] = 0;
        }

        OpenFolderAndFocusItem(HWindow, path, itemName);
    }
    else if (Is(ptPluginFS) &&
             GetPluginFS()->NotEmpty() &&
             GetPluginFS()->IsServiceSupported(FS_SERVICE_OPENACTIVEFOLDER))
    {
        UserWorkedOnThisPath = TRUE;
        GetPluginFS()->OpenActiveFolder(GetPluginFS()->GetPluginFSName(), HWindow);
    }
}

BOOL CFilesWindow::CommonRefresh(HWND parent, int suggestedTopIndex, const char* suggestedFocusName,
                                 BOOL refreshListBox, BOOL readDirectory, BOOL isRefresh)
{
    CALL_STACK_MESSAGE6("CFilesWindow::CommonRefresh(, %d, %s, %d, %d, %d)", suggestedTopIndex,
                        suggestedFocusName, refreshListBox, readDirectory, isRefresh);

    //TRACE_I("common refresh: begin");
    if (readDirectory) // if only the top-index and focus-name need to be projected, this is not necessary (it can only harm, so we don't call)
    {
        DirectoryLineSetText();
        if (Parent->GetActivePanel() == this)
        {
            Parent->EditWindowSetDirectory();
        }
    }

    //TRACE_I("read directory: begin");
    BOOL ret = FALSE;
    if (!readDirectory || ReadDirectory(parent, isRefresh))
        ret = TRUE;
    else
    {
        if (Is(ptDisk) || Is(ptZIPArchive))
            DetachDirectory(this); // something went wrong
    }
    //TRACE_I("read directory: begin");

    if (refreshListBox)
    {
        // Search for the item I should select
        int suggestedFocusIndex = -1;
        int suggestedFocusIndexIgnCase = -1;
        if (suggestedFocusName != NULL)
        {
            int i;
            for (i = 0; i < Dirs->Count; i++)
            {
                if (StrICmp(Dirs->At(i).Name, suggestedFocusName) == 0)
                {
                    if (suggestedFocusIndexIgnCase == -1)
                        suggestedFocusIndexIgnCase = i;
                    if (strcmp(Dirs->At(i).Name, suggestedFocusName) == 0)
                    {
                        suggestedFocusIndex = i;
                        break; // found exact desired name
                    }
                }
            }
            if (suggestedFocusIndex == -1) // we are also looking among files (e.g. when returning from a ZIP archive)
            {
                for (i = 0; i < Files->Count; i++)
                {
                    if (StrICmp(Files->At(i).Name, suggestedFocusName) == 0)
                    {
                        if (suggestedFocusIndexIgnCase == -1)
                            suggestedFocusIndexIgnCase = i + Dirs->Count;
                        if (strcmp(Files->At(i).Name, suggestedFocusName) == 0)
                        {
                            suggestedFocusIndex = i + Dirs->Count;
                            break; // found exact desired name
                        }
                    }
                }
            }
            // if the exact desired name was not found, we will use a name that matches except for the case (if applicable)
            if (suggestedFocusIndex == -1)
                suggestedFocusIndex = suggestedFocusIndexIgnCase;
        }

        //TRACE_I("refresh listbox: begin");
        RefreshListBox(0, suggestedTopIndex, suggestedFocusIndex, TRUE, !isRefresh);
        //TRACE_I("refresh listbox: end");
    }

    DirectoryLine->InvalidateIfNeeded();
    //TRACE_I("common refresh: end");
    return ret;
}
