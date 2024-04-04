// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "cfgdlg.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "snooper.h"
#include "shellib.h"
#include "pack.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"
#include "tasklist.h"
//#include "drivelst.h"

//
// ****************************************************************************
// UseOwnRoutine
//

BOOL UseOwnRutine(IDataObject* pDataObject)
{
    return DropSourcePanel != NULL || // it's dragging from us
           OurClipDataObject;         // or it's on our clipboard
}

//
// ****************************************************************************
// MouseConfirmDrop
//

BOOL MouseConfirmDrop(DWORD& effect, DWORD& defEffect, DWORD& grfKeyState)
{
    HMENU menu = CreatePopupMenu();
    if (menu != NULL)
    {
        /* Used for the export_mnu.py script, which generates salmenu.mnu for Translator
   to keep synchronized with the AppendMenu() calls below...
MENU_TEMPLATE_ITEM MouseDropMenu1[] =
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_DROPMOVE
	{MNTT_IT, IDS_DROPCOPY
	{MNTT_IT, IDS_DROPLINK
	{MNTT_IT, IDS_DROPCANCEL
	{MNTT_PE, 0
};
MENU_TEMPLATE_ITEM MouseDropMenu2[] =
{
	{MNTT_PB, 0
	{MNTT_IT, IDS_DROPUNKNOWN
	{MNTT_IT, IDS_DROPCANCEL
	{MNTT_PE, 0
};*/
        DWORD cmd = 4;
        char *item1 = NULL, *item2 = NULL, *item3 = NULL, *item4 = NULL;
        if (effect & DROPEFFECT_MOVE)
            item1 = LoadStr(IDS_DROPMOVE);
        if (effect & DROPEFFECT_COPY)
            item2 = LoadStr(IDS_DROPCOPY);
        if (effect & DROPEFFECT_LINK)
            item3 = LoadStr(IDS_DROPLINK);
        if (item1 == NULL && item2 == NULL && item3 == NULL)
            item4 = LoadStr(IDS_DROPUNKNOWN);

        if ((item1 == NULL || AppendMenu(menu, MF_ENABLED | MF_STRING, 1, item1)) &&
            (item2 == NULL || AppendMenu(menu, MF_ENABLED | MF_STRING, 2, item2)) &&
            (item3 == NULL || AppendMenu(menu, MF_ENABLED | MF_STRING, 3, item3)) &&
            (item4 == NULL || AppendMenu(menu, MF_ENABLED | MF_STRING | MF_DEFAULT,
                                         4, item4)) &&
            AppendMenu(menu, MF_SEPARATOR, 0, NULL) &&
            AppendMenu(menu, MF_ENABLED | MF_STRING | MF_DEFAULT, 5, LoadStr(IDS_DROPCANCEL)))
        {
            int defItem = 0;
            if (item1 != NULL && (defEffect & DROPEFFECT_MOVE))
                defItem = 1;
            if (item2 != NULL && (defEffect & DROPEFFECT_COPY))
                defItem = 2;
            if (item3 != NULL && (defEffect & DROPEFFECT_LINK))
                defItem = 3;
            if (defItem != 0)
            {
                MENUITEMINFO item;
                memset(&item, 0, sizeof(item));
                item.cbSize = sizeof(item);
                item.fMask = MIIM_STATE;
                item.fState = MFS_DEFAULT | MFS_ENABLED;
                SetMenuItemInfo(menu, defItem, FALSE, &item);
            }
            POINT p;
            GetCursorPos(&p);
            cmd = TrackPopupMenuEx(menu, TPM_RETURNCMD | TPM_LEFTALIGN | TPM_LEFTBUTTON, p.x, p.y, MainWindow->HWindow, NULL);
        }
        DestroyMenu(menu);
        switch (cmd)
        {
        case 1: // move
        {
            effect = defEffect = DROPEFFECT_MOVE;
            grfKeyState = 0;
            break;
        }

        case 2: // copy
        {
            effect = defEffect = DROPEFFECT_COPY;
            grfKeyState = 0;
            break;
        }

        case 3: // link
        {
            effect = defEffect = DROPEFFECT_LINK;
            grfKeyState = MK_SHIFT | MK_CONTROL;
            break;
        }

        case 0: // ESC
        case 5:
            return FALSE; // cancel
        }
    }
    return TRUE;
}

//
// ****************************************************************************
// DoCopyMove
//

BOOL DoCopyMove(BOOL copy, char* targetDir, CCopyMoveData* data, void* param)
{
    CFilesWindow* panel = (CFilesWindow*)param;

    CTmpDropData* tmp = new CTmpDropData;
    if (tmp != NULL)
    {
        tmp->Copy = copy;
        strcpy(tmp->TargetPath, targetDir);
        tmp->Data = data;
        PostMessage(panel->HWindow, WM_USER_DROPCOPYMOVE, (WPARAM)tmp, 0);
        return TRUE;
    }
    else
    {
        DestroyCopyMoveData(data);
        return FALSE;
    }
}

//
// ****************************************************************************
// DoDragDropOper
//

void DoDragDropOper(BOOL copy, BOOL toArchive, const char* archiveOrFSName, const char* archivePathOrUserPart,
                    CDragDropOperData* data, void* param)
{
    CFilesWindow* panel = (CFilesWindow*)param;
    CTmpDragDropOperData* tmp = new CTmpDragDropOperData;
    if (tmp != NULL)
    {
        tmp->Copy = copy;
        tmp->ToArchive = toArchive;
        BOOL ok = TRUE;
        if (archiveOrFSName == NULL)
        {
            if (toArchive)
            {
                if (panel->Is(ptZIPArchive))
                    archiveOrFSName = panel->GetZIPArchive();
                else
                {
                    TRACE_E("DoDragDropOper(): unexpected type of drop panel (should be archive)!");
                    ok = FALSE;
                }
            }
            else
            {
                if (panel->Is(ptPluginFS))
                    archiveOrFSName = panel->GetPluginFS()->GetPluginFSName();
                else
                {
                    TRACE_E("DoDragDropOper(): unexpected type of drop panel (should be FS)!");
                    ok = FALSE;
                }
            }
        }
        if (ok)
        {
            lstrcpyn(tmp->ArchiveOrFSName, archiveOrFSName, MAX_PATH);
            lstrcpyn(tmp->ArchivePathOrUserPart, archivePathOrUserPart, MAX_PATH);
            tmp->Data = data;
            PostMessage(panel->HWindow, WM_USER_DROPTOARCORFS, (WPARAM)tmp, 0);
            data = NULL;
            tmp = NULL;
        }
    }
    else
        TRACE_E(LOW_MEMORY);
    if (tmp != NULL)
        delete tmp;
    if (data != NULL)
        delete data;
}

//
// ****************************************************************************
// DoGetFSToFSDropEffect
//

void DoGetFSToFSDropEffect(const char* srcFSPath, const char* tgtFSPath,
                           DWORD allowedEffects, DWORD keyState,
                           DWORD* dropEffect, void* param)
{
    CFilesWindow* panel = (CFilesWindow*)param;
    DWORD orgEffect = *dropEffect;
    if (panel->Is(ptPluginFS) && panel->GetPluginFS()->NotEmpty())
    {
        panel->GetPluginFS()->GetDropEffect(srcFSPath, tgtFSPath, allowedEffects,
                                            keyState, dropEffect);
    }

    // if the file system did not respond or returned nonsense, we give priority to Copy
    if (*dropEffect != DROPEFFECT_COPY && *dropEffect != DROPEFFECT_MOVE &&
        *dropEffect != DROPEFFECT_NONE)
    {
        *dropEffect = orgEffect;
        if ((*dropEffect & DROPEFFECT_COPY) != 0)
            *dropEffect = DROPEFFECT_COPY;
        else
        {
            if ((*dropEffect & DROPEFFECT_MOVE) != 0)
                *dropEffect = DROPEFFECT_MOVE;
            else
                *dropEffect = DROPEFFECT_NONE; // error drop target
        }
    }
}

//
// ****************************************************************************
// GetCurrentDir
//

const char* GetCurrentDir(POINTL& pt, void* param, DWORD* effect, BOOL rButton, BOOL& isTgtFile,
                          DWORD keyState, int& tgtType, int srcType)
{
    CFilesWindow* panel = (CFilesWindow*)param;
    isTgtFile = FALSE; // there is currently no drop target file -> we can also handle the operation
    tgtType = idtttWindows;
    RECT r;
    GetWindowRect(panel->GetListBoxHWND(), &r);
    int index = panel->GetIndex(pt.x - r.left, pt.y - r.top);
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS))
    {
        if (panel->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(panel->GetZIPArchive());
            if (format != 0) // We found a supported archive
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) &&
                        (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0 || // Does edit have an effect? Is it copy or move?
                    index == 0 && panel->Dirs->Count > 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0 &&
                        (panel->GetZIPPath()[0] == 0 || panel->GetZIPPath()[0] == '\\' && panel->GetZIPPath()[1] == 0)) // drop to disk path
                {
                    tgtType = idtttArchive;
                    DWORD origEffect = *effect;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // Trim effect on copy+move

                    if (index >= 0 && index < panel->Dirs->Count) // drop on directory
                    {
                        panel->SetDropTarget(index);
                        int l = (int)strlen(panel->GetZIPPath());
                        memcpy(panel->DropPath, panel->GetZIPPath(), l);
                        if (index == 0 && strcmp(panel->Dirs->At(index).Name, "..") == 0)
                        {
                            if (l > 0 && panel->DropPath[l - 1] == '\\')
                                panel->DropPath[--l] = 0;
                            int backSlash = 0;
                            if (l == 0) // drop-path will be the disk (".." leads out of the archive)
                            {
                                tgtType = idtttWindows;
                                *effect = origEffect;
                                l = (int)strlen(panel->GetZIPArchive());
                                memcpy(panel->DropPath, panel->GetZIPArchive(), l);
                                backSlash = 1;
                            }
                            char* s = panel->DropPath + l;
                            while (--s >= panel->DropPath && *s != '\\')
                                ;
                            if (s > panel->DropPath)
                                *(s + backSlash) = 0;
                            else
                                panel->DropPath[0] = 0;
                        }
                        else
                        {
                            if (l > 0 && panel->DropPath[l - 1] != '\\')
                                panel->DropPath[l++] = '\\';
                            if (l + panel->Dirs->At(index).NameLen >= MAX_PATH)
                            {
                                TRACE_E("GetCurrentDir(): too long file name!");
                                tgtType = idtttWindows;
                                panel->SetDropTarget(-1); // hide the tag
                                return NULL;
                            }
                            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
                        }
                        return panel->DropPath;
                    }
                    else
                    {
                        panel->SetDropTarget(-1); // hide the tag
                        return panel->GetZIPPath();
                    }
                }
            }
        }
        else
        {
            if (panel->GetPluginFS()->NotEmpty())
            {
                if (srcType == 2 /* File system*/) // drag&drop from one file system to another file system (any file systems among each other, restrictions only in CPluginFSInterfaceAbstract::CopyOrMoveFromFS)
                {
                    tgtType = idtttFullPluginFSPath;
                    int l = (int)strlen(panel->GetPluginFS()->GetPluginFSName());
                    memcpy(panel->DropPath, panel->GetPluginFS()->GetPluginFSName(), l);
                    panel->DropPath[l++] = ':';
                    if (index >= 0 && index < panel->Dirs->Count) // drop on directory
                    {
                        if (panel == DropSourcePanel) // drag&drop within one panel
                        {
                            if (panel->GetSelCount() == 0 &&
                                    index == panel->GetCaretIndex() ||
                                panel->GetSel(index) != 0)
                            {                             // directory itself into itself
                                panel->SetDropTarget(-1); // hide the tag (the copy will go to the current directory, not to the focused subdirectory)
                                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                                {
                                    tgtType = idtttWindows;
                                    return NULL; // Without a modifier, the STOP cursor remains (prevents unwanted copying to the current directory)
                                }
                                if (effect != NULL)
                                    *effect &= ~DROPEFFECT_MOVE;
                                if (panel->GetPluginFS()->GetCurrentPath(panel->DropPath + l))
                                    return panel->DropPath;
                                else
                                {
                                    tgtType = idtttWindows;
                                    return NULL;
                                }
                            }
                        }

                        if (panel->GetPluginFS()->GetFullName(panel->Dirs->At(index),
                                                              (index == 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0) ? 2 : 1,
                                                              panel->DropPath + l, MAX_PATH))
                        {
                            if (DropSourcePanel != NULL && DropSourcePanel->Is(ptPluginFS) &&
                                DropSourcePanel->GetPluginFS()->NotEmpty() && effect != NULL)
                            { // Source FS can affect allowed drop effects
                                DropSourcePanel->GetPluginFS()->GetAllowedDropEffects(1 /* drag-over-fs*/, panel->DropPath,
                                                                                      effect);
                            }

                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }

                    panel->SetDropTarget(-1);                       // hide the tag
                    if (panel == DropSourcePanel && effect != NULL) // drag&drop within one panel
                    {
                        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        {
                            tgtType = idtttWindows;
                            return NULL; // Without a modifier, the STOP cursor remains (prevents unwanted copying to the current directory)
                        }
                        *effect &= ~DROPEFFECT_MOVE;
                    }
                    if (panel->GetPluginFS()->GetCurrentPath(panel->DropPath + l))
                    {
                        if (DropSourcePanel != NULL && DropSourcePanel->Is(ptPluginFS) &&
                            DropSourcePanel->GetPluginFS()->NotEmpty() && effect != NULL)
                        { // Source FS can affect allowed drop effects
                            DropSourcePanel->GetPluginFS()->GetAllowedDropEffects(1 /* drag-over-fs*/, panel->DropPath,
                                                                                  effect);
                        }
                        return panel->DropPath;
                    }
                    else
                    {
                        tgtType = idtttWindows;
                        return NULL;
                    }
                }

                DWORD posEff = 0;
                if (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMDISKTOFS))
                    posEff |= DROPEFFECT_COPY;
                if (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMDISKTOFS))
                    posEff |= DROPEFFECT_MOVE;
                if ((*effect & posEff) != 0)
                {
                    tgtType = idtttPluginFS;
                    *effect &= posEff; // Trim effect to FS capabilities

                    if (index >= 0 && index < panel->Dirs->Count) // drop on directory
                    {
                        if (panel->GetPluginFS()->GetFullName(panel->Dirs->At(index),
                                                              (index == 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0) ? 2 : 1,
                                                              panel->DropPath, MAX_PATH))
                        {
                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }
                    panel->SetDropTarget(-1); // hide the tag
                    if (panel->GetPluginFS()->GetCurrentPath(panel->DropPath))
                        return panel->DropPath;
                    else
                    {
                        tgtType = idtttWindows;
                        return NULL;
                    }
                }
            }
        }
        panel->SetDropTarget(-1); // hide the tag
        return NULL;
    }

    if (index >= 0 && index < panel->Dirs->Count) // drop on directory
    {
        if (panel == DropSourcePanel) // drag&drop within one panel
        {
            if (panel->GetSelCount() == 0 &&
                    index == panel->GetCaretIndex() ||
                panel->GetSel(index) != 0)
            {                             // directory itself into itself
                panel->SetDropTarget(-1); // hide the tag (the copy/shortcut will go to the current directory, not to the focused subdirectory)
                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                    return NULL; // Without a modifier, the STOP cursor remains (prevents unwanted copying to the current directory)
                if (effect != NULL)
                    *effect &= ~DROPEFFECT_MOVE;
                return panel->GetPath();
            }
        }

        panel->SetDropTarget(index);
        int l = (int)strlen(panel->GetPath());
        memcpy(panel->DropPath, panel->GetPath(), l);
        if (strcmp(panel->Dirs->At(index).Name, "..") == 0)
        {
            char* s = panel->DropPath + l;
            if (l > 0 && *(s - 1) == '\\')
                s--;
            while (--s > panel->DropPath && *s != '\\')
                ;
            if (s > panel->DropPath)
                *(s + 1) = 0;
        }
        else
        {
            if (panel->GetPath()[l - 1] != '\\')
                panel->DropPath[l++] = '\\';
            if (l + panel->Dirs->At(index).NameLen >= MAX_PATH)
            {
                TRACE_E("GetCurrentDir(): too long file name!");
                panel->SetDropTarget(-1); // hide the tag
                return NULL;
            }
            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
        }
        return panel->DropPath;
    }
    else
    {
        if (index >= panel->Dirs->Count && index < panel->Dirs->Count + panel->Files->Count)
        {                                 // drop on file
            if (panel == DropSourcePanel) // drag&drop within one panel
            {
                if (panel->GetSelCount() == 0 &&
                        index == panel->GetCaretIndex() ||
                    panel->GetSel(index) != 0)
                {                             // file itself into itself
                    panel->SetDropTarget(-1); // hide the tag (the copy/shortcut will go to the current directory, not to the focused file)
                    if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        return NULL; // Without a modifier, the STOP cursor remains (prevents unwanted copying to the current directory)
                    if (effect != NULL)
                        *effect &= ~DROPEFFECT_MOVE;
                    return panel->GetPath();
                }
            }
            char fullName[MAX_PATH];
            int l = (int)strlen(panel->GetPath());
            memcpy(fullName, panel->GetPath(), l);
            if (fullName[l - 1] != '\\')
                fullName[l++] = '\\';
            CFileData* file = &(panel->Files->At(index - panel->Dirs->Count));
            if (l + file->NameLen >= MAX_PATH)
            {
                TRACE_E("GetCurrentDir(): too long file name!");
                panel->SetDropTarget(-1); // hide the tag
                return NULL;
            }
            strcpy(fullName + l, file->Name);

            // When it comes to shortcuts, we will analyze them
            BOOL linkIsDir = FALSE;  // TRUE -> short-cut to directory -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> shortcut to file -> test archive
            char linkTgt[MAX_PATH];
            linkTgt[0] = 0;
            if (StrICmp(file->Ext, "lnk") == 0) // Isn't this a shortcut to the directory?
            {
                IShellLink* link;
                if (CoCreateInstance(CLSID_ShellLink, NULL,
                                     CLSCTX_INPROC_SERVER, IID_IShellLink,
                                     (LPVOID*)&link) == S_OK)
                {
                    IPersistFile* fileInt;
                    if (link->QueryInterface(IID_IPersistFile, (LPVOID*)&fileInt) == S_OK)
                    {
                        OLECHAR oleName[MAX_PATH];
                        MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH);
                        oleName[MAX_PATH - 1] = 0;
                        if (fileInt->Load(oleName, STGM_READ) == S_OK &&
                            link->GetPath(linkTgt, MAX_PATH, NULL, SLGP_UNCPRIORITY) == NOERROR)
                        {
                            DWORD attr = SalGetFileAttributes(linkTgt);
                            if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_DIRECTORY))
                                linkIsDir = TRUE;
                            else
                                linkIsFile = TRUE;
                        }
                        fileInt->Release();
                    }
                    link->Release();
                }
            }
            if (linkIsDir) // the link leads to a directory, the path is okay, let's switch to it
            {
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, linkTgt);
                return panel->DropPath;
            }

            int format = PackerFormatConfig.PackIsArchive(linkIsFile ? linkTgt : fullName);
            if (format != 0) // We found a supported archive
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) && // edit?
                    (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0)
                {
                    tgtType = idtttArchiveOnWinPath;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // Trim effect on copy+move
                    panel->SetDropTarget(index);
                    strcpy(panel->DropPath, linkIsFile ? linkTgt : fullName);
                    return panel->DropPath;
                }
                panel->SetDropTarget(-1); // hide the tag
                return NULL;
            }

            if (HasDropTarget(fullName))
            {
                isTgtFile = TRUE; // drop target file -> must handle shell
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, fullName);
                return panel->DropPath;
            }
        }
        panel->SetDropTarget(-1); // hide the tag
    }

    if (panel == DropSourcePanel && effect != NULL) // drag&drop within one panel
    {
        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
            return NULL; // Without a modifier, the STOP cursor remains (prevents unwanted copying to the current directory)
        *effect &= ~DROPEFFECT_MOVE;
    }
    return panel->GetPath();
}

const char* GetCurrentDirClipboard(POINTL& pt, void* param, DWORD* effect, BOOL rButton,
                                   BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType)
{ // Simple version of the previous one for "paste" from clipboard
    CFilesWindow* panel = (CFilesWindow*)param;
    isTgtFile = FALSE;
    tgtType = idtttWindows;
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS)) // to the archive and not to the FS for now
    {
        //    if (panel->Is(ptZIPArchive)) tgtType = idtttArchive;
        //    else tgtType = idtttPluginFS;
        return NULL;
    }
    return panel->DropPath;
}

//
// ****************************************************************************
// DropEnd
//

int CountNumberOfItemsOnPath(const char* path)
{
    char s[MAX_PATH + 10];
    lstrcpyn(s, path, MAX_PATH + 10);
    if (SalPathAppend(s, "*.*", MAX_PATH + 10))
    {
        WIN32_FIND_DATA fileData;
        HANDLE search = HANDLES_Q(FindFirstFile(s, &fileData));
        if (search != INVALID_HANDLE_VALUE)
        {
            int num = 0;
            do
            {
                num++;
            } while (FindNextFile(search, &fileData));
            HANDLES(FindClose(search));
            return num;
        }
    }
    return 0;
}

void DropEnd(BOOL drop, BOOL shortcuts, void* param, BOOL ownRutine, BOOL isFakeDataObject, int tgtType)
{
    CFilesWindow* panel = (CFilesWindow*)param;
    if (drop && GetActiveWindow() == NULL)
        SetForegroundWindow(MainWindow->HWindow);
    if (drop)
        MainWindow->FocusPanel(panel);

    panel->SetDropTarget(-1); // hide the tag
    if (tgtType == idtttWindows &&
        !isFakeDataObject && (!ownRutine || shortcuts) && drop && // refresh panels
        (!MainWindow->LeftPanel->AutomaticRefresh ||
         !MainWindow->RightPanel->AutomaticRefresh ||
         MainWindow->LeftPanel->GetNetworkDrive() ||
         MainWindow->RightPanel->GetNetworkDrive()))
    {
        BOOL again = TRUE; // as long as files are being added, we load
        int numLeft = MainWindow->LeftPanel->NumberOfItemsInCurDir;
        int numRight = MainWindow->RightPanel->NumberOfItemsInCurDir;
        while (again)
        {
            again = FALSE;
            Sleep(shortcuts ? 333 : 1000); // Work in another thread, let them have some time

            if ((!MainWindow->LeftPanel->AutomaticRefresh || MainWindow->LeftPanel->GetNetworkDrive()) &&
                MainWindow->LeftPanel->Is(ptDisk))
            {
                int newNum = CountNumberOfItemsOnPath(MainWindow->LeftPanel->GetPath());
                again |= newNum != numLeft;
                numLeft = newNum;
            }
            if ((!MainWindow->RightPanel->AutomaticRefresh || MainWindow->RightPanel->GetNetworkDrive()) &&
                MainWindow->RightPanel->Is(ptDisk))
            {
                int newNum = CountNumberOfItemsOnPath(MainWindow->RightPanel->GetPath());
                again |= newNum != numRight;
                numRight = newNum;
            }
        }

        // let's refresh the panels
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        int t2 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        if (!MainWindow->LeftPanel->AutomaticRefresh || MainWindow->LeftPanel->GetNetworkDrive())
            PostMessage(MainWindow->LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
        if (!MainWindow->RightPanel->AutomaticRefresh || MainWindow->RightPanel->GetNetworkDrive())
            PostMessage(MainWindow->RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);
        MainWindow->RefreshDiskFreeSpace();
    }
}

void EnterLeaveDrop(BOOL enter, void* param)
{
    CFilesWindow* panel = (CFilesWindow*)param;
    if (enter)
        panel->DragEnter();
    else
        panel->DragLeave();
}

//
// ****************************************************************************
// SetClipCutCopyInfo
//

void SetClipCutCopyInfo(HWND hwnd, BOOL copy, BOOL salObject)
{
    UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
    UINT cfSalDataObject = RegisterClipboardFormat(SALCF_IDATAOBJECT);
    HANDLE effect = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DWORD)));
    HANDLE effect2 = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DWORD)));
    if (effect != NULL && effect2 != NULL)
    {
        DWORD* ef = (DWORD*)HANDLES(GlobalLock(effect));
        if (ef != NULL)
        {
            *ef = copy ? (DROPEFFECT_COPY | DROPEFFECT_LINK) : DROPEFFECT_MOVE;
            HANDLES(GlobalUnlock(effect));
            if (OpenClipboard(hwnd))
            {
                if (SetClipboardData(cfPrefDrop, effect) == NULL)
                    NOHANDLES(GlobalFree(effect));
                if (!salObject || SetClipboardData(cfSalDataObject, effect2) == NULL)
                    NOHANDLES(GlobalFree(effect2));
                CloseClipboard();
            }
            else
            {
                TRACE_E("OpenClipboard() has failed!");
                NOHANDLES(GlobalFree(effect));
                NOHANDLES(GlobalFree(effect2));
            }
        }
        else
        {
            NOHANDLES(GlobalFree(effect));
            NOHANDLES(GlobalFree(effect2));
        }
    }
}

//
// ****************************************************************************
// ShellAction
//

const char* EnumFileNames(int index, void* param)
{
    CTmpEnumData* data = (CTmpEnumData*)param;
    if (data->Indexes[index] >= 0 &&
        data->Indexes[index] < data->Panel->Dirs->Count + data->Panel->Files->Count)
    {
        return (data->Indexes[index] < data->Panel->Dirs->Count) ? data->Panel->Dirs->At(data->Indexes[index]).Name : data->Panel->Files->At(data->Indexes[index] - data->Panel->Dirs->Count).Name;
    }
    else
        return NULL;
}

const char* EnumOneFileName(int index, void* param)
{
    return index == 0 ? (char*)param : NULL;
}

void AuxInvokeCommand2(CFilesWindow* panel, CMINVOKECOMMANDINFO* ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        panel->ContextSubmenuNew->GetMenu2()->InvokeCommand(ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 17))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void AuxInvokeCommand(CFilesWindow* panel, CMINVOKECOMMANDINFO* ici)
{ // WARNING: also used in CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        panel->ContextMenu->InvokeCommand(ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 18))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void AuxInvokeAndRelease(IContextMenu2* menu, CMINVOKECOMMANDINFO* ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        menu->InvokeCommand(ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 19))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);

    __try
    {
        menu->Release();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        RelExceptionHasOccured++;
    }
}

HRESULT AuxGetCommandString(IContextMenu2* menu, UINT_PTR idCmd, UINT uType, UINT* pReserved, LPSTR pszName, UINT cchMax)
{
    CALL_STACK_MESSAGE_NONE
    HRESULT ret = E_UNEXPECTED;
    __try
    {
        // for years we have been getting crashes when calling IContextMenu2::GetCommandString()
        // This function call is not essential for the program flow, so we will handle it in a try/except block
        ret = menu->GetCommandString(idCmd, uType, pReserved, pszName, cchMax);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 19))
    {
        ICExceptionHasOccured++;
    }
    return ret;
}

void ShellActionAux5(UINT flags, CFilesWindow* panel, HMENU h)
{ // WARNING: also used in CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        panel->ContextMenu->QueryContextMenu(h, 0, 0, 4999, flags);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 20))
    {
        QCMExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void ShellActionAux6(CFilesWindow* panel)
{ // WARNING: also used in CSalamanderGeneral::OpenNetworkContextMenu()
    __try
    {
        if (panel->ContextMenu != NULL)
            panel->ContextMenu->Release();
        panel->ContextMenu = NULL;
        if (panel->ContextSubmenuNew->MenuIsAssigned())
            panel->ContextSubmenuNew->Release();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        RelExceptionHasOccured++;
    }
}

void ShellActionAux7(IDataObject* dataObject, CImpIDropSource* dropSource)
{
    __try
    {
        if (dropSource != NULL)
            dropSource->Release(); // That's ours, hopefully it won't fall ;-)
        if (dataObject != NULL)
            dataObject->Release();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        RelExceptionHasOccured++;
    }
}

void DoDragFromArchiveOrFS(CFilesWindow* panel, BOOL& dropDone, char* targetPath, int& operation,
                           char* realDraggedPath, DWORD allowedEffects,
                           int srcType, const char* srcFSPath, BOOL leftMouseButton)
{
    if (SalShExtSharedMemView != NULL) // shared memory is available (we cannot handle drag&drop errors)
    {
        CALL_STACK_MESSAGE1("ShellAction::archive/FS::drag_files");

        // create a "fake" directory
        char fakeRootDir[MAX_PATH];
        char* fakeName;
        if (SalGetTempFileName(NULL, "SAL", fakeRootDir, FALSE))
        {
            fakeName = fakeRootDir + strlen(fakeRootDir);
            // jr: I found a mention on the internet "Did implementing "IPersistStream" and providing the undocumented
            // "OleClipboardPersistOnFlush" format solve the problem?" -- in case we need to
            // remove the DROPFAKE method
            if (SalPathAppend(fakeRootDir, "DROPFAKE", MAX_PATH))
            {
                if (CreateDirectory(fakeRootDir, NULL))
                {
                    // create objects for drag&drop
                    *fakeName = 0;
                    IDataObject* dataObject = CreateIDataObject(MainWindow->HWindow, fakeRootDir,
                                                                1, EnumOneFileName, fakeName + 1);
                    BOOL dragFromPluginFSWithCopyAndMove = allowedEffects == (DROPEFFECT_MOVE | DROPEFFECT_COPY);
                    CImpIDropSource* dropSource = new CImpIDropSource(dragFromPluginFSWithCopyAndMove);
                    if (dataObject != NULL && dropSource != NULL)
                    {
                        CFakeDragDropDataObject* fakeDataObject = new CFakeDragDropDataObject(dataObject, realDraggedPath,
                                                                                              srcType, srcFSPath);
                        if (fakeDataObject != NULL)
                        {
                            // Initialization of shared memory
                            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                            BOOL sharedMemOK = SalShExtSharedMemView->Size >= sizeof(CSalShExtSharedMem);
                            if (sharedMemOK)
                            {
                                if (SalShExtSharedMemView->DoDragDropFromSalamander)
                                    TRACE_E("Drag&drop from archive/FS: SalShExtSharedMemView->DoDragDropFromSalamander is TRUE, this should never happen here!");
                                SalShExtSharedMemView->DoDragDropFromSalamander = TRUE;
                                *fakeName = '\\';
                                lstrcpyn(SalShExtSharedMemView->DragDropFakeDirName, fakeRootDir, MAX_PATH);
                                SalShExtSharedMemView->DropDone = FALSE;
                            }
                            ReleaseMutex(SalShExtSharedMemMutex);

                            if (sharedMemOK)
                            {
                                DWORD dwEffect;
                                HRESULT hr;
                                DropSourcePanel = panel;
                                LastWndFromGetData = NULL; // just in case, in case fakeDataObject->GetData is not called
                                hr = DoDragDrop(fakeDataObject, dropSource, allowedEffects, &dwEffect);
                                DropSourcePanel = NULL;
                                // read the results of drag & drop
                                // Note: returns dwEffect == 0 for MOVE, therefore we introduce a workaround through dropSource->LastEffect,
                                // refer to "Handling Shell Data Transfer Scenarios" section "Handling Optimized Move Operations":
                                // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
                                // (abbreviated: an optimized Move is being performed, which means no copy is made to the target followed by deletion
                                //            originalu, so that the source does not unintentionally delete the original (which may not have been moved yet), receives
                                //            result of the operation DROPEFFECT_NONE or DROPEFFECT_COPY)
                                if (hr == DRAGDROP_S_DROP && dropSource->LastEffect != DROPEFFECT_NONE)
                                {
                                    WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                                    dropDone = SalShExtSharedMemView->DropDone;
                                    SalShExtSharedMemView->DoDragDropFromSalamander = FALSE;
                                    if (dropDone)
                                    {
                                        lstrcpyn(targetPath, SalShExtSharedMemView->TargetPath, 2 * MAX_PATH);
                                        if (leftMouseButton && dragFromPluginFSWithCopyAndMove)
                                            operation = (dropSource->LastEffect & DROPEFFECT_MOVE) ? SALSHEXT_MOVE : SALSHEXT_COPY;
                                        else // archives + FS with Copy or Move (not both) + FS with Copy+Move when dragging with the right mouse button, where the result from the right-click menu is not affected by the change of the mouse cursor (trick with Copy cursor during Move effect), so we take the result from the copy-hook (SalShExtSharedMemView->Operation)
                                            operation = SalShExtSharedMemView->Operation;
                                    }
                                    ReleaseMutex(SalShExtSharedMemMutex);

                                    if (!dropDone &&                 // does not respond to the copy-hook or another user Cancel in the drop-down menu (appears when right-clicking during D&D)
                                        dwEffect != DROPEFFECT_NONE) // Cancel detection: since the copy-hook did not work, the returned drop-effect is valid, so we compare it to Cancel
                                    {
                                        SalMessageBox(MainWindow->HWindow, LoadStr(IDS_SHEXT_NOTLOADEDYET),
                                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                                    }
                                }
                                else
                                {
                                    WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                                    SalShExtSharedMemView->DoDragDropFromSalamander = FALSE;
                                    ReleaseMutex(SalShExtSharedMemMutex);
                                }
                            }
                            else
                                TRACE_E("Shared memory is too small!");
                            fakeDataObject->Release(); // dataObject will be released later in ShellActionAux7
                        }
                        else
                            TRACE_E(LOW_MEMORY);
                    }

                    ShellActionAux7(dataObject, dropSource);
                }
                else
                    TRACE_E("Unable to create fake directory in TEMP for drag&drop from archive/FS: unable to create subdir!");
            }
            else
                TRACE_E("Unable to create fake directory in TEMP for drag&drop from archive/FS: too long name!");
            *fakeName = 0;
            RemoveTemporaryDir(fakeRootDir);
        }
        else
            TRACE_E("Unable to create fake directory in TEMP for drag&drop from archive/FS!");
    }
}

void GetLeftTopCornert(POINT* pt, BOOL posByMouse, BOOL useSelection, CFilesWindow* panel)
{
    if (posByMouse)
    {
        // coordinates according to the mouse position
        DWORD pos = GetMessagePos();
        pt->x = GET_X_LPARAM(pos);
        pt->y = GET_Y_LPARAM(pos);
    }
    else
    {
        if (useSelection)
        {
            // according to the position for the context menu
            panel->GetContextMenuPos(pt);
        }
        else
        {
            // top left corner of the panel
            RECT r;
            GetWindowRect(panel->GetListBoxHWND(), &r);
            pt->x = r.left;
            pt->y = r.top;
        }
    }
}

void RemoveUselessSeparatorsFromMenu(HMENU h)
{
    int miCount = GetMenuItemCount(h);
    MENUITEMINFO mi;
    int lastSep = -1;
    int i;
    for (i = miCount - 1; i >= 0; i--)
    {
        memset(&mi, 0, sizeof(mi));
        mi.cbSize = sizeof(mi);
        mi.fMask = MIIM_TYPE;
        if (GetMenuItemInfo(h, i, TRUE, &mi) && (mi.fType & MFT_SEPARATOR))
        {
            if (lastSep != -1 && lastSep == i + 1) // two separators in a row, we will delete one, it is redundant
                DeleteMenu(h, i, MF_BYPOSITION);
            lastSep = i;
        }
    }
}

#define GET_WORD(ptr) (*(WORD*)(ptr))
#define GET_DWORD(ptr) (*(DWORD*)(ptr))

BOOL ResourceGetDialogName(WCHAR* buff, int buffSize, char* name, int nameMax)
{
    DWORD style = GET_DWORD(buff);
    buff += 2; // dlgVer + signature
    if (style != 0xffff0001)
    {
        TRACE_E("ResourceGetDialogName(): resource is not DLGTEMPLATEEX!");
        // reading classic DLGTEMPLATE as seen in Altap Translator, but probably won't need to implement
        return FALSE;
    }

    //  typedef struct {
    //    WORD dlgVer;     // Specifies the version number of the extended dialog box template. This member must be 1.
    //    WORD signature;  // Indicates whether a template is an extended dialog box template. If signature is 0xFFFF, this is an extended dialog box template.
    //    DWORD helpID;
    //    DWORD exStyle;
    //    DWORD style;
    //    WORD cDlgItems;
    //    short x;
    //    short y;
    //    short cx;
    //    short cy;
    //    sz_Or_Ord menu;
    //    sz_Or_Ord windowClass;
    //    WCHAR title[titleLen];
    //    WORD pointsize;
    //    WORD weight;
    //    BYTE italic;
    //    BYTE charset;
    //    WCHAR typeface[stringLen];
    //  } DLGTEMPLATEEX;

    buff += 2; // helpID
    buff += 2; // exStyle
    buff += 2; // style
    buff += 1; // cDlgItems
    buff += 1; // x
    buff += 1; // and
    buff += 1; // cx
    buff += 1; // cy

    // menu name
    switch (GET_WORD(buff))
    {
    case 0x0000:
    {
        buff++;
        break;
    }

    case 0xffff:
    {
        buff += 2;
        break;
    }

    default:
    {
        buff += wcslen(buff) + 1;
        break;
    }
    }

    // class name
    switch (GET_WORD(buff))
    {
    case 0x0000:
    {
        buff++;
        break;
    }

    case 0xffff:
    {
        buff += 2;
        break;
    }

    default:
    {
        buff += wcslen(buff) + 1;
        break;
    }
    }

    // window name
    WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, buff, (int)wcslen(buff) + 1, name, nameMax, NULL, NULL);

    return TRUE;
}

// try to load aclui.dll and retrieve the name of the dialog saved with ID 103 (Security tab)
// If successful, fills the dialog title into pageName and returns TRUE; otherwise returns FALSE
BOOL GetACLUISecurityPageName(char* pageName, int pageNameMax)
{
    BOOL ret = FALSE;

    HINSTANCE hModule = LoadLibraryEx("aclui.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (hModule != NULL)
    {
        HRSRC hrsrc = FindResource(hModule, MAKEINTRESOURCE(103), RT_DIALOG); // 103 - security tab
        if (hrsrc != NULL)
        {
            int size = SizeofResource(hModule, hrsrc);
            if (size > 0)
            {
                HGLOBAL hglb = LoadResource(hModule, hrsrc);
                if (hglb != NULL)
                {
                    LPVOID data = LockResource(hglb);
                    if (data != NULL)
                        ret = ResourceGetDialogName((WCHAR*)data, size, pageName, pageNameMax);
                }
            }
            else
                TRACE_E("GetACLUISecurityPageName() invalid Security dialog box resource.");
        }
        else
            TRACE_E("GetACLUISecurityPageName() cannot find Security dialog box.");
        FreeLibrary(hModule);
    }
    else
        TRACE_E("GetACLUISecurityPageName() cannot load aclui.dll");

    return ret;
}

void ShellAction(CFilesWindow* panel, CShellAction action, BOOL useSelection,
                 BOOL posByMouse, BOOL onlyPanelMenu)
{
    CALL_STACK_MESSAGE5("ShellAction(, %d, %d, %d, %d)", action, useSelection, posByMouse, onlyPanelMenu);
    if (panel->QuickSearchMode)
        panel->EndQuickSearch();
    if (panel->Dirs->Count + panel->Files->Count == 0 && useSelection)
    { // without files and directories -> nothing to do
        return;
    }

    BOOL dragFiles = action == saLeftDragFiles || action == saRightDragFiles;
    if (panel->Is(ptZIPArchive) && action != saContextMenu &&
        (!dragFiles && action != saCopyToClipboard || !SalShExtRegistered))
    {
        if (dragFiles && !SalShExtRegistered)
        {
            TRACE_E("Drag&drop from archives is not possible, shell extension utils\\salextx86.dll or utils\\salextx64.dll is missing!");
        }
        if (action == saCopyToClipboard && !SalShExtRegistered)
            TRACE_E("Copy&paste from archives is not possible, shell extension utils\\salextx86.dll or utils\\salextx64.dll is missing!");
        // We do not yet know how to perform other activities in the archive
        return;
    }
    if (panel->Is(ptPluginFS) && dragFiles &&
        (!SalShExtRegistered ||
         !panel->GetPluginFS()->NotEmpty() ||
         !panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) &&    // FS can "move from FS"
             !panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS))) // FS can "copy from FS"
    {
        if (!SalShExtRegistered)
            TRACE_E("Drag&drop from file-systems is not possible, shell extension utils\\salextx86.dll or utils\\salextx64.dll is missing!");
        if (!panel->GetPluginFS()->NotEmpty())
            TRACE_E("Unexpected situation in ShellAction(): panel->GetPluginFS() is empty!");
        return;
    }

    //  MainWindow->ReleaseMenuNew();  // Windows are not built for multiple context menus

    BeginStopRefresh(); // we do not need any refreshes

    int* indexes = NULL;
    int index = 0;
    int count = 0;
    if (useSelection)
    {
        BOOL subDir;
        if (panel->Dirs->Count > 0)
            subDir = (strcmp(panel->Dirs->At(0).Name, "..") == 0);
        else
            subDir = FALSE;

        count = panel->GetSelCount();
        if (count != 0)
        {
            indexes = new int[count];
            panel->GetSelItems(count, indexes, action == saContextMenu); // we have backed off from this (see GetSelItems): for the context menu, we start from the focused item and end with the item after the focus (there is an intermediate return to the beginning of the list of names) (the system does it too, see Add To Windows Media Player List on MP3 files)
        }
        else
        {
            index = panel->GetCaretIndex();
            if (subDir && index == 0)
            {
                EndStopRefresh();
                return;
            }
        }
    }
    else
        index = -1;

    char targetPath[2 * MAX_PATH];
    targetPath[0] = 0;
    char realDraggedPath[2 * MAX_PATH];
    realDraggedPath[0] = 0;
    if (panel->Is(ptZIPArchive) && SalShExtRegistered)
    {
        if (dragFiles)
        {
            // if there is only one subdirectory in the archive, we will determine which one (for changing the path
            // in directory-line and insertion into command-line)
            int i = -1;
            if (count == 1)
                i = indexes[0];
            else if (count == 0)
                i = index;
            if (i >= 0 && i < panel->Dirs->Count)
            {
                realDraggedPath[0] = 'D';
                lstrcpyn(realDraggedPath + 1, panel->GetZIPArchive(), 2 * MAX_PATH);
                SalPathAppend(realDraggedPath, panel->GetZIPPath(), 2 * MAX_PATH);
                SalPathAppend(realDraggedPath, panel->Dirs->At(i).Name, 2 * MAX_PATH);
            }
            else
            {
                if (i >= 0 && i >= panel->Dirs->Count && i < panel->Dirs->Count + panel->Files->Count)
                {
                    realDraggedPath[0] = 'F';
                    lstrcpyn(realDraggedPath + 1, panel->GetZIPArchive(), 2 * MAX_PATH);
                    SalPathAppend(realDraggedPath, panel->GetZIPPath(), 2 * MAX_PATH);
                    SalPathAppend(realDraggedPath, panel->Files->At(i - panel->Dirs->Count).Name, 2 * MAX_PATH);
                }
            }

            BOOL dropDone = FALSE;
            int operation = SALSHEXT_NONE;
            DoDragFromArchiveOrFS(panel, dropDone, targetPath, operation, realDraggedPath,
                                  DROPEFFECT_COPY, 1 /* archive*/, NULL, action == saLeftDragFiles);

            if (indexes != NULL)
                delete[] (indexes);
            EndStopRefresh();

            if (dropDone) // let's perform the operation
            {
                char* p = DupStr(targetPath);
                if (p != NULL)
                    PostMessage(panel->HWindow, WM_USER_DROPUNPACK, (WPARAM)p, operation);
            }

            return;
        }
        else
        {
            if (action == saCopyToClipboard)
            {
                if (SalShExtSharedMemView != NULL) // shared memory is available (we cannot handle copy&paste errors)
                {
                    CALL_STACK_MESSAGE1("ShellAction::archive::clipcopy_files");

                    // create a "fake" directory
                    char fakeRootDir[MAX_PATH];
                    char* fakeName;
                    if (SalGetTempFileName(NULL, "SAL", fakeRootDir, FALSE))
                    {
                        BOOL delFakeDir = TRUE;
                        fakeName = fakeRootDir + strlen(fakeRootDir);
                        if (SalPathAppend(fakeRootDir, "CLIPFAKE", MAX_PATH))
                        {
                            if (CreateDirectory(fakeRootDir, NULL))
                            {
                                DWORD prefferedDropEffect = DROPEFFECT_COPY; // DROPEFFECT_MOVE (used for debugging purposes)

                                // create objects for copy&paste
                                *fakeName = 0;
                                IDataObject* dataObject = CreateIDataObject(MainWindow->HWindow, fakeRootDir,
                                                                            1, EnumOneFileName, fakeName + 1);
                                if (dataObject != NULL)
                                {
                                    *fakeName = '\\';
                                    CFakeCopyPasteDataObject* fakeDataObject = new CFakeCopyPasteDataObject(dataObject, fakeRootDir);
                                    if (fakeDataObject != NULL)
                                    {
                                        UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
                                        HANDLE effect = NOHANDLES(GlobalAlloc(GMEM_MOVEABLE | GMEM_DDESHARE, sizeof(DWORD)));
                                        if (effect != NULL)
                                        {
                                            DWORD* ef = (DWORD*)HANDLES(GlobalLock(effect));
                                            if (ef != NULL)
                                            {
                                                *ef = prefferedDropEffect;
                                                HANDLES(GlobalUnlock(effect));

                                                if (SalShExtPastedData.SetData(panel->GetZIPArchive(), panel->GetZIPPath(),
                                                                               panel->Files, panel->Dirs,
                                                                               panel->IsCaseSensitive(),
                                                                               (count == 0) ? &index : indexes,
                                                                               (count == 0) ? 1 : count))
                                                {
                                                    BOOL clearSalShExtPastedData = TRUE;
                                                    if (OleSetClipboard(fakeDataObject) == S_OK)
                                                    { // after successful saving, the system calls fakeDataObject->AddRef() +
                                                        // store default drop-effect
                                                        if (OpenClipboard(MainWindow->HWindow))
                                                        {
                                                            if (SetClipboardData(cfPrefDrop, effect) != NULL)
                                                                effect = NULL;
                                                            CloseClipboard();
                                                        }
                                                        else
                                                            TRACE_E("OpenClipboard() has failed!");

                                                        // our data is now on the clipboard (we need to delete it when exiting Salama)
                                                        OurDataOnClipboard = TRUE;

                                                        // Initialization of shared memory
                                                        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                                                        BOOL sharedMemOK = SalShExtSharedMemView->Size >= sizeof(CSalShExtSharedMem);
                                                        if (sharedMemOK)
                                                        {
                                                            SalShExtSharedMemView->DoPasteFromSalamander = TRUE;
                                                            SalShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // initialize to 1 minute before creating the data object
                                                            *fakeName = '\\';
                                                            lstrcpyn(SalShExtSharedMemView->PasteFakeDirName, fakeRootDir, MAX_PATH);
                                                            SalShExtSharedMemView->SalamanderMainWndPID = GetCurrentProcessId();
                                                            SalShExtSharedMemView->SalamanderMainWndTID = GetCurrentThreadId();
                                                            SalShExtSharedMemView->SalamanderMainWnd = (UINT64)(DWORD_PTR)MainWindow->HWindow;
                                                            SalShExtSharedMemView->PastedDataID++;
                                                            SalShExtPastedData.SetDataID(SalShExtSharedMemView->PastedDataID);
                                                            clearSalShExtPastedData = FALSE;
                                                            SalShExtSharedMemView->PasteDone = FALSE;
                                                            lstrcpyn(SalShExtSharedMemView->ArcUnableToPaste1, LoadStr(IDS_ARCUNABLETOPASTE1), 300);
                                                            lstrcpyn(SalShExtSharedMemView->ArcUnableToPaste2, LoadStr(IDS_ARCUNABLETOPASTE2), 300);

                                                            delFakeDir = FALSE; // everything is OK, fake-dir is used
                                                            fakeDataObject->SetCutOrCopyDone();
                                                        }
                                                        else
                                                            TRACE_E("Shared memory is too small!");
                                                        ReleaseMutex(SalShExtSharedMemMutex);

                                                        if (!sharedMemOK) // If it is not possible to establish communication with salextx86.dll or salextx64.dll, there is no point in leaving the data object on the clipboard
                                                        {
                                                            OleSetClipboard(NULL);
                                                            OurDataOnClipboard = FALSE; // theoretically unnecessary (should be set in the Release() of the fakeDataObject - a few lines below)
                                                        }
                                                        // change clipboard, we will verify it ...
                                                        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                                                        IdleCheckClipboard = TRUE; // we will also check the clipboard

                                                        // When copying, we clear the CutToClip flag
                                                        if (panel->CutToClipChanged)
                                                            panel->ClearCutToClipFlag(TRUE);
                                                        CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                                                        // When copying, we also clear the CutToClip flag in the second panel
                                                        if (anotherPanel->CutToClipChanged)
                                                            anotherPanel->ClearCutToClipFlag(TRUE);
                                                    }
                                                    else
                                                        TRACE_E("Unable to set data object to clipboard (copy&paste from archive)!");
                                                    if (clearSalShExtPastedData)
                                                        SalShExtPastedData.Clear();
                                                }
                                            }
                                            if (effect != NULL)
                                                NOHANDLES(GlobalFree(effect));
                                        }
                                        else
                                            TRACE_E(LOW_MEMORY);
                                        fakeDataObject->Release(); // if the fakeDataObject is on the clipboard, release it with the end of the application or by releasing it from the clipboard
                                    }
                                    else
                                        TRACE_E(LOW_MEMORY);
                                }
                                ShellActionAux7(dataObject, NULL);
                            }
                            else
                                TRACE_E("Unable to create fake directory in TEMP for copy&paste from archive: unable to create subdir!");
                        }
                        else
                            TRACE_E("Unable to create fake directory in TEMP for copy&paste from archive: too long name!");
                        *fakeName = 0;
                        if (delFakeDir)
                            RemoveTemporaryDir(fakeRootDir);
                    }
                    else
                        TRACE_E("Unable to create fake directory in TEMP for copy&paste from archive!");
                }
                if (indexes != NULL)
                    delete[] (indexes);
                EndStopRefresh();
                return;
            }
        }
    }

    if (panel->Is(ptPluginFS))
    {
        // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        int panelID = MainWindow->LeftPanel == panel ? PANEL_LEFT : PANEL_RIGHT;

        int selectedDirs = 0;
        if (count > 0)
        {
            // we will count how many directories are marked (the rest of the marked items are files)
            int i;
            for (i = 0; i < panel->Dirs->Count; i++) // ".." cannot be marked, the test would be pointless
            {
                if (panel->Dirs->At(i).Selected)
                    selectedDirs++;
            }
        }

        if (action == saProperties && useSelection &&
            panel->GetPluginFS()->NotEmpty() &&
            panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_SHOWPROPERTIES)) // show-properties
        {
            panel->GetPluginFS()->ShowProperties(panel->GetPluginFS()->GetPluginFSName(),
                                                 panel->HWindow, panelID,
                                                 count - selectedDirs, selectedDirs);
        }
        else
        {
            if (action == saContextMenu &&
                panel->GetPluginFS()->NotEmpty() &&
                panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_CONTEXTMENU)) // context-menu
            {
                // calculate the top left corner of the context menu
                POINT p;
                if (posByMouse)
                {
                    DWORD pos = GetMessagePos();
                    p.x = GET_X_LPARAM(pos);
                    p.y = GET_Y_LPARAM(pos);
                }
                else
                {
                    if (useSelection)
                    {
                        panel->GetContextMenuPos(&p);
                    }
                    else
                    {
                        RECT r;
                        GetWindowRect(panel->GetListBoxHWND(), &r);
                        p.x = r.left;
                        p.y = r.top;
                    }
                }

                if (useSelection) // menu for items in the panel (click on item)
                {
                    panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                      panel->GetListBoxHWND(), p.x, p.y, fscmItemsInPanel,
                                                      panelID, count - selectedDirs, selectedDirs);
                }
                else
                {
                    if (onlyPanelMenu) // menu panel (clicking behind items in the panel)
                    {
                        panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                          panel->GetListBoxHWND(), p.x, p.y, fscmPanel,
                                                          panelID, 0, 0);
                    }
                    else // menu for the current path (clicking on the change-drive button)
                    {
                        panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                          panel->GetListBoxHWND(), p.x, p.y, fscmPathInPanel,
                                                          panelID, 0, 0);
                    }
                }
            }
            else
            {
                if (dragFiles && SalShExtRegistered &&
                    panel->GetPluginFS()->NotEmpty() &&
                    (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) || // FS can "move from FS"
                     panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS)))  // FS can "copy from FS"
                {
                    // if there is only one subdirectory FS, we will determine which one (for changing the path
                    // in directory-line and insertion into command-line)
                    int i = -1;
                    if (count == 1)
                        i = indexes[0];
                    else if (count == 0)
                        i = index;
                    if (i >= 0 && i < panel->Dirs->Count)
                    {
                        realDraggedPath[0] = 'D';
                        strcpy(realDraggedPath + 1, panel->GetPluginFS()->GetPluginFSName());
                        strcat(realDraggedPath, ":");
                        int l = (int)strlen(realDraggedPath);
                        if (!panel->GetPluginFS()->GetFullName(panel->Dirs->At(i), 1, realDraggedPath + l, 2 * MAX_PATH - l))
                            realDraggedPath[0] = 0;
                    }
                    else
                    {
                        if (i >= 0 && i >= panel->Dirs->Count && i < panel->Dirs->Count + panel->Files->Count)
                        {
                            realDraggedPath[0] = 'F';
                            strcpy(realDraggedPath + 1, panel->GetPluginFS()->GetPluginFSName());
                            strcat(realDraggedPath, ":");
                            int l = (int)strlen(realDraggedPath);
                            if (!panel->GetPluginFS()->GetFullName(panel->Files->At(i - panel->Dirs->Count),
                                                                   0, realDraggedPath + l, 2 * MAX_PATH - l))
                            {
                                realDraggedPath[0] = 0;
                            }
                        }
                    }

                    BOOL dropDone = FALSE;
                    int operation = SALSHEXT_NONE;
                    DWORD allowedEffects = (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) ? DROPEFFECT_MOVE : 0) |
                                           (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS) ? DROPEFFECT_COPY : 0);
                    char srcFSPath[2 * MAX_PATH];
                    strcpy(srcFSPath, panel->GetPluginFS()->GetPluginFSName());
                    strcat(srcFSPath, ":");
                    if (!panel->GetPluginFS()->GetCurrentPath(srcFSPath + strlen(srcFSPath)))
                        srcFSPath[0] = 0;
                    panel->GetPluginFS()->GetAllowedDropEffects(0 /* start*/, NULL, &allowedEffects);
                    DoDragFromArchiveOrFS(panel, dropDone, targetPath, operation, realDraggedPath,
                                          allowedEffects, 2 /* File system*/, srcFSPath, action == saLeftDragFiles);
                    panel->GetPluginFS()->GetAllowedDropEffects(2 /* end*/, NULL, NULL);

                    if (dropDone) // let's perform the operation
                    {
                        char* p = DupStr(targetPath);
                        if (p != NULL)
                            PostMessage(panel->HWindow, WM_USER_DROPFROMFS, (WPARAM)p, operation);
                    }
                }
            }
        }

        // increase the thread priority again, operation completed
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        if (indexes != NULL)
            delete[] (indexes);
        EndStopRefresh();
        return;
    }

    if (!panel->Is(ptDisk) && !panel->Is(ptZIPArchive))
    {
        if (indexes != NULL)
            delete[] (indexes);
        EndStopRefresh();
        return; // for safety reasons, we will not allow other types of panels
    }

#ifndef _WIN64
    char redirectedDir[MAX_PATH];
    char msg[300 + MAX_PATH];
#endif // _WIN64
    switch (action)
    {
    case saPermissions:
    case saProperties:
    {
        CALL_STACK_MESSAGE1("ShellAction::properties");
        if (useSelection)
        {
#ifndef _WIN64
            if (ContainsWin64RedirectedDir(panel, (count == 0) ? &index : indexes, (count == 0) ? 1 : count, redirectedDir, TRUE))
            {
                _snprintf_s(msg, _TRUNCATE, LoadStr(IDS_ERROPENPROPSELCONTW64ALIAS), redirectedDir);
                SalMessageBox(MainWindow->HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
#endif // _WIN64
                CTmpEnumData data;
                data.Indexes = (count == 0) ? &index : indexes;
                data.Panel = panel;
                IContextMenu2* menu = CreateIContextMenu2(MainWindow->HWindow, panel->GetPath(),
                                                          (count == 0) ? 1 : count,
                                                          EnumFileNames, &data);
                if (menu != NULL)
                {
                    CShellExecuteWnd shellExecuteWnd;
                    CMINVOKECOMMANDINFOEX ici;
                    ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
                    ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
                    ici.fMask = CMIC_MASK_PTINVOKE;
                    ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: ShellAction::properties");
                    ici.lpVerb = "properties";
                    char pageName[200];
                    if (action == saPermissions)
                    {
                        // Force opening the Security tab; unfortunately, a string for the specific OS localization needs to be passed
                        ici.lpParameters = pageName;
                        if (!GetACLUISecurityPageName(pageName, 200))
                            lstrcpy(pageName, "Security"); // if we failed to extract the name, we will use the English "Security" and in localized versions we will quietly not work
                    }
                    ici.lpDirectory = panel->GetPath();
                    ici.nShow = SW_SHOWNORMAL;
                    GetLeftTopCornert(&ici.ptInvoke, posByMouse, useSelection, panel);

                    AuxInvokeAndRelease(menu, (CMINVOKECOMMANDINFO*)&ici);
                }
#ifndef _WIN64
            }
#endif // _WIN64
        }
        break;
    }

    case saCopyToClipboard:
    case saCutToClipboard:
    {
        CALL_STACK_MESSAGE1("ShellAction::copy_cut_clipboard");
        if (useSelection)
        {
#ifndef _WIN64
            if (action == saCutToClipboard &&
                ContainsWin64RedirectedDir(panel, (count == 0) ? &index : indexes, (count == 0) ? 1 : count, redirectedDir, FALSE))
            {
                _snprintf_s(msg, _TRUNCATE, LoadStr(IDS_ERRCUTSELCONTW64ALIAS), redirectedDir);
                SalMessageBox(MainWindow->HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
            else
            {
#endif // _WIN64
                CTmpEnumData data;
                data.Indexes = (count == 0) ? &index : indexes;
                data.Panel = panel;
                IContextMenu2* menu = CreateIContextMenu2(MainWindow->HWindow, panel->GetPath(), (count == 0) ? 1 : count,
                                                          EnumFileNames, &data);
                if (menu != NULL)
                {
                    CShellExecuteWnd shellExecuteWnd;
                    CMINVOKECOMMANDINFO ici;
                    ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
                    ici.fMask = 0;
                    ici.lpVerb = (action == saCopyToClipboard) ? "copy" : "cut";
                    ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: ShellAction::copy_cut_clipboard verb=%s", ici.lpVerb);
                    ici.lpParameters = NULL;
                    ici.lpDirectory = panel->GetPath();
                    ici.nShow = SW_SHOWNORMAL;
                    ici.dwHotKey = 0;
                    ici.hIcon = 0;

                    AuxInvokeAndRelease(menu, &ici);

                    // change clipboard, we will verify it ...
                    IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                    IdleCheckClipboard = TRUE; // we will also check the clipboard

                    BOOL repaint = FALSE;
                    if (panel->CutToClipChanged)
                    {
                        // before CUT and COPY we clear the CutToClip flag
                        panel->ClearCutToClipFlag(FALSE);
                        repaint = TRUE;
                    }
                    CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                    BOOL samePaths = panel->Is(ptDisk) && anotherPanel->Is(ptDisk) &&
                                     IsTheSamePath(panel->GetPath(), anotherPanel->GetPath());
                    if (anotherPanel->CutToClipChanged)
                    {
                        // before CUT and COPY we clear the CutToClip flag also in the second panel
                        anotherPanel->ClearCutToClipFlag(!samePaths);
                    }

                    if (action != saCopyToClipboard)
                    {
                        // In the CUT case, we will set the CutToClip (ghosted) bit of the file
                        int idxCount = count;
                        int* idxs = (idxCount == 0) ? &index : indexes;
                        if (idxCount == 0)
                            idxCount = 1;
                        int i;
                        for (i = 0; i < idxCount; i++)
                        {
                            int idx = idxs[i];
                            CFileData* f = (idx < panel->Dirs->Count) ? &panel->Dirs->At(idx) : &panel->Files->At(idx - panel->Dirs->Count);
                            f->CutToClip = 1;
                            f->Dirty = 1;
                            if (samePaths) // mark the file/directory in the second panel (quadratic complexity, disinterest ...)
                            {
                                if (idx < panel->Dirs->Count) // searching among directories
                                {
                                    int total = anotherPanel->Dirs->Count;
                                    int k;
                                    for (k = 0; k < total; k++)
                                    {
                                        CFileData* f2 = &anotherPanel->Dirs->At(k);
                                        if (StrICmp(f->Name, f2->Name) == 0)
                                        {
                                            f2->CutToClip = 1;
                                            f2->Dirty = 1;
                                            break;
                                        }
                                    }
                                }
                                else // searching among files
                                {
                                    int total = anotherPanel->Files->Count;
                                    int k;
                                    for (k = 0; k < total; k++)
                                    {
                                        CFileData* f2 = &anotherPanel->Files->At(k);
                                        if (StrICmp(f->Name, f2->Name) == 0)
                                        {
                                            f2->CutToClip = 1;
                                            f2->Dirty = 1;
                                            break;
                                        }
                                    }
                                }
                            }
                        }
                        panel->CutToClipChanged = TRUE;
                        if (samePaths)
                            anotherPanel->CutToClipChanged = TRUE;
                        repaint = TRUE;
                    }

                    if (repaint)
                        panel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                    if (samePaths)
                        anotherPanel->RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);

                    // set preferred drop effect + origin from Salama
                    SetClipCutCopyInfo(panel->HWindow, action == saCopyToClipboard, TRUE);
                }
#ifndef _WIN64
            }
#endif // _WIN64
        }
        break;
    }

    case saLeftDragFiles:
    case saRightDragFiles:
    {
        CALL_STACK_MESSAGE1("ShellAction::drag_files");
        if (useSelection)
        {
            CTmpEnumData data;
            data.Indexes = (count == 0) ? &index : indexes;
            data.Panel = panel;
            IDataObject* dataObject = CreateIDataObject(MainWindow->HWindow, panel->GetPath(),
                                                        (count == 0) ? 1 : count,
                                                        EnumFileNames, &data);
            CImpIDropSource* dropSource = new CImpIDropSource(FALSE);

            if (dataObject != NULL && dropSource != NULL)
            {
                DWORD dwEffect;
                HRESULT hr;
                DropSourcePanel = panel;
                hr = DoDragDrop(dataObject, dropSource, DROPEFFECT_MOVE | DROPEFFECT_LINK | DROPEFFECT_COPY, &dwEffect);
                DropSourcePanel = NULL;
            }

            ShellActionAux7(dataObject, dropSource);
        }
        break;
    }

    case saContextMenu:
    {
        CALL_STACK_MESSAGE1("ShellAction::context_menu");

        // calculate the top left corner of the context menu
        POINT pt;
        GetLeftTopCornert(&pt, posByMouse, useSelection, panel);

        if (panel->Is(ptZIPArchive))
        {
            if (useSelection) // only when it comes to items in the panel (not the current path in the panel)
            {
                // If it is necessary to calculate the states of commands, we will do it (ArchiveMenu.UpdateItemsState is used for this)
                MainWindow->OnEnterIdle();

                // Set states according to the enabler and open the menu
                ArchiveMenu.UpdateItemsState();
                DWORD cmd = ArchiveMenu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                              pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                // send the result to the main window
                if (cmd != 0)
                    PostMessage(MainWindow->HWindow, WM_COMMAND, cmd, 0);
            }
            else
            {
                if (onlyPanelMenu) // context menu in the panel (behind items) -> just paste
                {
                    // If it is necessary to calculate the states of commands, we will do it (ArchivePanelMenu.UpdateItemsState is used for this)
                    MainWindow->OnEnterIdle();

                    // Set states according to the enabler and open the menu
                    ArchivePanelMenu.UpdateItemsState();

                    // When it comes to a paste of type "change directory", we will display it in the Paste item
                    char text[220];
                    char tail[50];
                    tail[0] = 0;

                    strcpy(text, LoadStr(IDS_ARCHIVEMENU_CLIPPASTE));

                    if (EnablerPastePath &&
                        (!panel->Is(ptDisk) || !EnablerPasteFiles) && // PasteFiles is a priority
                        !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS is a priority
                    {
                        char* p = strrchr(text, '\t');
                        if (p != NULL)
                            strcpy(tail, p);
                        else
                            p = text + strlen(text);

                        sprintf(p, " (%s)%s", LoadStr(IDS_PASTE_CHANGE_DIRECTORY), tail);
                    }

                    MENU_ITEM_INFO mii;
                    mii.Mask = MENU_MASK_STRING;
                    mii.String = text;
                    ArchivePanelMenu.SetItemInfo(CM_CLIPPASTE, FALSE, &mii);

                    DWORD cmd = ArchivePanelMenu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                                       pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                    // send the result to the main window
                    if (cmd != 0)
                        PostMessage(MainWindow->HWindow, WM_COMMAND, cmd, 0);
                }
            }
        }
        else
        {
            BOOL uncRootPath = FALSE;
            if (panel->ContextMenu != NULL) // We received a crash apparently caused by a recursive call through the message loop in contextPopup.Track (resulting in resetting panel->ContextMenu, presumably right when exiting the internal recursion call).
            {
                TRACE_E("ShellAction::context_menu: panel->ContextMenu must be NULL (probably forbidden recursive call)!");
            }
            else // ptDisk
            {
                HMENU h = CreatePopupMenu();

                UINT flags = CMF_NORMAL | CMF_EXPLORE;
                // we will handle the pressed shift - extended context menu, under W2K there is for example Run as...
#define CMF_EXTENDEDVERBS 0x00000100 // rarely used verbs
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (shiftPressed)
                    flags |= CMF_EXTENDEDVERBS;

                if (useSelection && count <= 1)
                    flags |= CMF_CANRENAME;

                BOOL alreadyHaveContextMenu = FALSE;

                if (onlyPanelMenu)
                {
#ifndef _WIN64
                    if (IsWin64RedirectedDir(panel->GetPath(), NULL, TRUE))
                    {
                        SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ERROPENMENUFORW64ALIAS),
                                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
#endif // _WIN64
                        panel->ContextMenu = CreateIContextMenu2(MainWindow->HWindow, panel->GetPath());
                        if (panel->ContextMenu != NULL && h != NULL)
                        {
                            // bypass bugs in the TortoiseHg shell extensions: it has a global variable with item ID mappings
                            // in the menu and the THg command, in our case, when two menus are obtained
                            // (panel->ContextMenu a panel->ContextSubmenuNew) dojde k premazani tohoto
                            // mapping of later obtained menu (calling QueryContextMenu), i.e. commands from before
                            // The acquired menus cannot be launched, in the original version it was about the menu panel->ContextSubmenuNew,
                            // where all commands are in the panel context menu except for Open and Explore:
                            // To tackle this issue, we will take advantage of the fact that we are taking from the menu panel->ContextMenu
                            // only Open and Explore, therefore the commands woken, which do not suffer from this error, so
                            // just get the menu (call QueryContextMenu) from panel->ContextSubmenuNew as
                            // second in order
                            // NOTE: we cannot always do this because if only the New menu is added,
                            //           On the contrary, it is more understandable to obtain the second in order from the menu panel->ContextMenu,
                            //           to make its commands work (for example, THg is not added to the New menu at all,
                            //           so no problem arises)
                            ShellActionAux5(flags, panel, h);
                            alreadyHaveContextMenu = TRUE;
                        }
                        GetNewOrBackgroundMenu(MainWindow->HWindow, panel->GetPath(), panel->ContextSubmenuNew, 5000, 6000, TRUE);
                        uncRootPath = IsUNCRootPath(panel->GetPath());
#ifndef _WIN64
                    }
#endif // _WIN64
                }
                else
                {
                    if (useSelection)
                    {
#ifndef _WIN64
                        if (ContainsWin64RedirectedDir(panel, (count == 0) ? &index : indexes, (count == 0) ? 1 : count, redirectedDir, TRUE))
                        {
                            _snprintf_s(msg, _TRUNCATE, LoadStr(IDS_ERROPENMENUSELCONTW64ALIAS), redirectedDir);
                            SalMessageBox(MainWindow->HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        else
                        {
#endif // _WIN64
                            CTmpEnumData data;
                            data.Indexes = (count == 0) ? &index : indexes;
                            data.Panel = panel;
                            panel->ContextMenu = CreateIContextMenu2(MainWindow->HWindow, panel->GetPath(), (count == 0) ? 1 : count,
                                                                     EnumFileNames, &data);
#ifndef _WIN64
                        }
#endif // _WIN64
                    }
                    else
                    {
#ifndef _WIN64
                        if (IsWin64RedirectedDir(panel->GetPath(), NULL, TRUE))
                        {
                            SalMessageBox(MainWindow->HWindow, LoadStr(IDS_ERROPENMENUFORW64ALIAS),
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        else
                        {
#endif // _WIN64
                            panel->ContextMenu = CreateIContextMenu2(MainWindow->HWindow, panel->GetPath());
                            GetNewOrBackgroundMenu(MainWindow->HWindow, panel->GetPath(), panel->ContextSubmenuNew, 5000, 6000, FALSE);
                            uncRootPath = IsUNCRootPath(panel->GetPath());
#ifndef _WIN64
                        }
#endif // _WIN64
                    }
                }

                BOOL clipCopy = FALSE;     // Is it about "our copy"?
                BOOL clipCut = FALSE;      // Is it about "our cut"?
                BOOL cmdDelete = FALSE;    // Is it "our delete"?
                BOOL cmdMapNetDrv = FALSE; // Is it about "our Map Network Drive"? (just UNC root, let's not complicate our lives)
                DWORD cmd = 0;             // command number for context menu (10000 = "our paste")
                char pastePath[MAX_PATH];  // buffer for the path to which "our paste" will be performed (if it occurs)
                if (panel->ContextMenu != NULL && h != NULL)
                {
                    if (!alreadyHaveContextMenu)
                        ShellActionAux5(flags, panel, h);
                    RemoveUselessSeparatorsFromMenu(h);

                    char cmdName[2000]; // intentionally we have 2000 instead of 200, shell extensions sometimes write double (consideration: unicode = 2 * "number of characters"), etc.
                    if (onlyPanelMenu)
                    {
                        if (panel->ContextSubmenuNew->MenuIsAssigned())
                        {
                            HMENU bckgndMenu = panel->ContextSubmenuNew->GetMenu();
                            int bckgndMenuInsert = 0;
                            if (useSelection)
                                TRACE_E("Unexpected value in 'useSelection' (TRUE) in ShellAction(saContextMenu).");
                            int miCount = GetMenuItemCount(h);
                            MENUITEMINFO mi;
                            char itemName[500];
                            int i;
                            for (i = 0; i < miCount; i++)
                            {
                                memset(&mi, 0, sizeof(mi)); // required here
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_SUBMENU;
                                mi.dwTypeData = itemName;
                                mi.cch = 500;
                                if (GetMenuItemInfo(h, i, TRUE, &mi))
                                {
                                    if (mi.hSubMenu == NULL && (mi.fType & MFT_SEPARATOR) == 0) // There is no submenu or separator
                                    {
                                        if (AuxGetCommandString(panel->ContextMenu, mi.wID, GCS_VERB, NULL, cmdName, 200) == NOERROR)
                                        {
                                            if (stricmp(cmdName, "explore") == 0 || stricmp(cmdName, "open") == 0)
                                            {
                                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                                                if (bckgndMenuInsert == 2)
                                                    break; // we don't need more items from here
                                            }
                                        }
                                    }
                                }
                                else
                                {
                                    DWORD err = GetLastError();
                                    TRACE_E("Unable to get item information from menu: " << GetErrorText(err));
                                }
                            }
                            if (bckgndMenuInsert > 0) // Separate Explore + Open from the rest of the menu
                            {
                                // separator
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_TYPE;
                                mi.fType = MFT_SEPARATOR;
                                mi.dwTypeData = NULL;
                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                            }

                            /* is used for the export_mnu.py script, which generates the salmenu.mnu for the Translator
   to keep synchronized with the InsertMenuItem() calls below...
MENU_TEMPLATE_ITEM PanelBkgndMenu[] =
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MENU_EDIT_PASTE
  {MNTT_IT, IDS_PASTE_CHANGE_DIRECTORY
  {MNTT_IT, IDS_MENU_EDIT_PASTELINKS   
  {MNTT_PE, 0
};*/

                            // add the Paste command (if it is a "change directory" type of paste, display it in the Paste item)
                            char tail[50];
                            tail[0] = 0;
                            strcpy(itemName, LoadStr(IDS_MENU_EDIT_PASTE));
                            if (EnablerPastePath && !EnablerPasteFiles) // PasteFiles is a priority
                            {
                                char* p = strrchr(itemName, '\t');
                                if (p != NULL)
                                    strcpy(tail, p);
                                else
                                    p = itemName + strlen(itemName);

                                sprintf(p, " (%s)%s", LoadStr(IDS_PASTE_CHANGE_DIRECTORY), tail);
                            }
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
                            mi.fType = MFT_STRING;
                            mi.fState = EnablerPastePath || EnablerPasteFiles ? MFS_ENABLED : MFS_DISABLED;
                            mi.dwTypeData = itemName;
                            mi.wID = 10000;
                            InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);

                            // add the Paste Shortcuts command
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
                            mi.fType = MFT_STRING;
                            mi.fState = EnablerPasteLinksOnDisk ? MFS_ENABLED : MFS_DISABLED;
                            mi.dwTypeData = LoadStr(IDS_MENU_EDIT_PASTELINKS);
                            mi.wID = 10001;
                            InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);

                            // if it's not there already, we insert a separator
                            MENUITEMINFO mi2;
                            memset(&mi2, 0, sizeof(mi2));
                            mi2.cbSize = sizeof(mi);
                            mi2.fMask = MIIM_TYPE;
                            if (!GetMenuItemInfo(bckgndMenu, bckgndMenuInsert, TRUE, &mi2) ||
                                (mi2.fType & MFT_SEPARATOR) == 0)
                            {
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_TYPE;
                                mi.fType = MFT_SEPARATOR;
                                mi.dwTypeData = NULL;
                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                            }

                            DestroyMenu(h);
                            h = bckgndMenu;
                        }
                    }
                    else
                    {
                        // Originally, adding a new item was called before ShellActionAux5, but
                        // Under Windows XP, an error occurred when calling ShellActionAux5 to delete an item
                        // New (in case an Edit/Copy operation was performed beforehand)
                        if (panel->ContextSubmenuNew->MenuIsAssigned())
                        {
                            MENUITEMINFO mi;

                            // separator
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_TYPE;
                            mi.fType = MFT_SEPARATOR;
                            mi.dwTypeData = NULL;
                            InsertMenuItem(h, -1, TRUE, &mi);

                            // New submenu
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_STATE | MIIM_SUBMENU | MIIM_TYPE;
                            mi.fType = MFT_STRING;
                            mi.fState = MFS_ENABLED;
                            mi.hSubMenu = panel->ContextSubmenuNew->GetMenu();
                            mi.dwTypeData = LoadStr(IDS_MENUNEWTITLE);
                            InsertMenuItem(h, -1, TRUE, &mi);
                        }
                    }

                    if (GetMenuItemCount(h) > 0) // Protection against completely cut menu
                    {
                        CMenuPopup contextPopup;
                        contextPopup.SetTemplateMenu(h);
                        cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                                 pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                        //            for testing purposes only -- display menu using Windows API
                        //            cmd = TrackPopupMenuEx(h, TPM_RETURNCMD | TPM_LEFTALIGN |
                        //                                   TPM_LEFTBUTTON, pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                    }
                    else
                        cmd = 0;
                    if (cmd != 0)
                    {
                        CALL_STACK_MESSAGE1("ShellAction::context_menu::exec0");
                        if (cmd < 5000)
                        {
                            if (AuxGetCommandString(panel->ContextMenu, cmd, GCS_VERB, NULL, cmdName, 200) != NOERROR)
                            {
                                cmdName[0] = 0;
                            }
                        }
                        if (cmd == 10000 || cmd == 10001)
                            strcpy(pastePath, panel->GetPath());
                        if (cmd < 5000 && stricmp(cmdName, "paste") == 0 && count <= 1)
                        {
                            if (useSelection) // paste into the subdirectory panel->GetPath()
                            {
                                int specialIndex;
                                if (count == 1) // select
                                {
                                    panel->GetSelItems(1, &specialIndex);
                                }
                                else
                                    specialIndex = panel->GetCaretIndex(); // focus
                                if (specialIndex >= 0 && specialIndex < panel->Dirs->Count)
                                {
                                    const char* subdir = panel->Dirs->At(specialIndex).Name;
                                    strcpy(pastePath, panel->GetPath());
                                    char* s = pastePath + strlen(pastePath);
                                    if (s > pastePath && *(s - 1) != '\\')
                                        *s++ = '\\';
                                    strcpy(s, subdir);
                                    cmd = 10000; // command will be executed elsewhere
                                }
                            }
                            else // paste into panel->GetPath()
                            {
                                strcpy(pastePath, panel->GetPath());
                                cmd = 10000; // command will be executed elsewhere
                            }
                        }
                        clipCopy = (cmd < 5000 && stricmp(cmdName, "copy") == 0);
                        clipCut = (cmd < 5000 && stricmp(cmdName, "cut") == 0);
                        cmdDelete = useSelection && (cmd < 5000 && stricmp(cmdName, "delete") == 0);

                        // the Map Network Drive command is defined under XP 40, under W2K 43, and only under Vista does it have cmdName defined
                        cmdMapNetDrv = uncRootPath && (stricmp(cmdName, "connectNetworkDrive") == 0 ||
                                                       !WindowsVistaAndLater && cmd == 40);

                        if (cmd != 10000 && cmd != 10001 && !clipCopy && !clipCut && !cmdDelete && !cmdMapNetDrv)
                        {
                            if (cmd < 5000 && stricmp(cmdName, "rename") == 0)
                            {
                                int specialIndex;
                                if (count == 1) // select
                                {
                                    panel->GetSelItems(1, &specialIndex);
                                }
                                else
                                    specialIndex = -1;           // focus
                                panel->RenameFile(specialIndex); // Only the disk is considered (enabling "Rename" is not necessary)
                            }
                            else
                            {
                                BOOL releaseLeft = FALSE;                  // disconnect the left panel from the disk?
                                BOOL releaseRight = FALSE;                 // disconnect the right panel from the disk?
                                if (!useSelection && cmd < 5000 &&         // It's about a context menu for a directory
                                    stricmp(cmdName, "properties") != 0 && // it is not necessary for properties
                                    stricmp(cmdName, "find") != 0 &&       // u find is not necessary
                                    stricmp(cmdName, "open") != 0 &&       // u open is not necessary
                                    stricmp(cmdName, "explore") != 0 &&    // exploring is not necessary
                                    stricmp(cmdName, "link") != 0)         // Creating a shortcut is not necessary
                                {
                                    char root[MAX_PATH];
                                    GetRootPath(root, panel->GetPath());
                                    if (strlen(root) >= strlen(panel->GetPath())) // menu for the entire disk - for commands like
                                    {                                             // "format..." we need to "take hands off" from media
                                        CFilesWindow* win;
                                        int i;
                                        for (i = 0; i < 2; i++)
                                        {
                                            win = i == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
                                            if (HasTheSameRootPath(win->GetPath(), root)) // same disk (UNC and normal)
                                            {
                                                if (i == 0)
                                                    releaseLeft = TRUE;
                                                else
                                                    releaseRight = TRUE;
                                            }
                                        }
                                    }
                                }

                                CALL_STACK_MESSAGE1("ShellAction::context_menu::exec1");
                                if (!useSelection || count == 0 && index < panel->Dirs->Count ||
                                    count == 1 && indexes[0] < panel->Dirs->Count)
                                {
                                    SetCurrentDirectoryToSystem(); // to unmap the disk from the panel
                                }
                                else
                                {
                                    SetCurrentDirectory(panel->GetPath()); // for files containing spaces in their names: to make Open With work for Microsoft Paint as well (it was buggy under W2K - it would say "d:\documents.bmp was not found" for the file "D:\Documents and Settings\petr\My Documents\example.bmp")
                                }

                                DWORD disks = GetLogicalDrives();

                                CShellExecuteWnd shellExecuteWnd;
                                CMINVOKECOMMANDINFOEX ici;
                                ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
                                ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
                                ici.fMask = CMIC_MASK_PTINVOKE;
                                if (CanUseShellExecuteWndAsParent(cmdName))
                                    ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: ShellAction::context_menu cmd=%d", cmd);
                                else
                                    ici.hwnd = MainWindow->HWindow;
                                if (cmd < 5000)
                                    ici.lpVerb = MAKEINTRESOURCE(cmd);
                                else
                                    ici.lpVerb = MAKEINTRESOURCE(cmd - 5000);
                                ici.lpDirectory = panel->GetPath();
                                ici.nShow = SW_SHOWNORMAL;
                                ici.ptInvoke = pt;

                                panel->FocusFirstNewItem = TRUE; // for WinZip and its archives, as well as for the New menu (works well only with auto-refresh of the panel)
                                if (cmd < 5000)
                                {
                                    BOOL changeToFixedDrv = cmd == 35; // "format" is not modal, necessary change to fixed drive
                                    if (releaseLeft)
                                    {
                                        if (changeToFixedDrv)
                                        {
                                            MainWindow->LeftPanel->ChangeToFixedDrive(MainWindow->LeftPanel->HWindow);
                                        }
                                        else
                                            MainWindow->LeftPanel->HandsOff(TRUE);
                                    }
                                    if (releaseRight)
                                    {
                                        if (changeToFixedDrv)
                                        {
                                            MainWindow->RightPanel->ChangeToFixedDrive(MainWindow->RightPanel->HWindow);
                                        }
                                        else
                                            MainWindow->RightPanel->HandsOff(TRUE);
                                    }

                                    AuxInvokeCommand(panel, (CMINVOKECOMMANDINFO*)&ici);

                                    // cut/copy/paste we catch, but just to be sure we will still perform the same refresh of the clipboard enabler
                                    IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                                    IdleCheckClipboard = TRUE; // we will also check the clipboard

                                    if (releaseLeft && !changeToFixedDrv)
                                        MainWindow->LeftPanel->HandsOff(FALSE);
                                    if (releaseRight && !changeToFixedDrv)
                                        MainWindow->RightPanel->HandsOff(FALSE);

                                    //--- refresh non-automatically refreshed directories
                                    // we report a change in the current directory and its subdirectories (better, who knows what started)
                                    MainWindow->PostChangeOnPathNotification(panel->GetPath(), TRUE);
                                }
                                else
                                {
                                    if (panel->ContextSubmenuNew->MenuIsAssigned()) // an exception could occur
                                    {
                                        AuxInvokeCommand2(panel, (CMINVOKECOMMANDINFO*)&ici);

                                        //--- refresh non-automatically refreshed directories
                                        // we report a change in the current directory (a new file/directory can probably only be created in it)
                                        MainWindow->PostChangeOnPathNotification(panel->GetPath(), FALSE);
                                    }
                                }

                                if (GetLogicalDrives() < disks) // unmapping
                                {
                                    if (MainWindow->LeftPanel->CheckPath(FALSE) != ERROR_SUCCESS)
                                        MainWindow->LeftPanel->ChangeToRescuePathOrFixedDrive(MainWindow->LeftPanel->HWindow);
                                    if (MainWindow->RightPanel->CheckPath(FALSE) != ERROR_SUCCESS)
                                        MainWindow->RightPanel->ChangeToRescuePathOrFixedDrive(MainWindow->RightPanel->HWindow);
                                }
                            }
                        }
                    }
                }
                {
                    CALL_STACK_MESSAGE1("ShellAction::context_menu::release");
                    ShellActionAux6(panel);
                    if (h != NULL)
                        DestroyMenu(h);
                }

                if (cmd == 10000) // our own "paste" to pastePath
                {
                    if (!panel->ClipboardPaste(FALSE, FALSE, pastePath))
                        panel->ClipboardPastePath(); // classic paste failed, we probably just need to change the current path
                }
                else
                {
                    if (cmd == 10001) // our own "paste shortcuts" to pastePath
                    {
                        panel->ClipboardPaste(TRUE, FALSE, pastePath);
                    }
                    else
                    {
                        if (clipCopy) // our own "copy"
                        {
                            panel->ClipboardCopy(); // recursive call of ShellAction
                        }
                        else
                        {
                            if (clipCut) // our own "cut"
                            {
                                panel->ClipboardCut(); // recursive call of ShellAction
                            }
                            else
                            {
                                if (cmdDelete)
                                {
                                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_DELETEFILES, 0);
                                }
                                else
                                {
                                    if (cmdMapNetDrv) // Is it about "our Map Network Drive"? (just UNC root, let's not complicate our lives)
                                    {
                                        panel->ConnectNet(TRUE);
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        break;
    }
    }

    if (count != 0)
        delete[] (indexes);

    EndStopRefresh();
}

const char* ReturnNameFromParam(int, void* param)
{
    return (const char*)param;
}

void ExecuteAssociationAux(IContextMenu2* menu, CMINVOKECOMMANDINFO& ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        menu->InvokeCommand(&ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 21))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void ExecuteAssociationAux2(IContextMenu2* menu, HMENU h, DWORD flags)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        menu->QueryContextMenu(h, 0, 0, -1, flags);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 22))
    {
        QCMExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void ExecuteAssociationAux3(IContextMenu2* menu)
{
    __try
    {
        menu->Release();
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        RelExceptionHasOccured++;
    }
}

extern DWORD ExecuteAssociationTlsIndex; // Allows only one call at a time (prevents recursion) in each thread

void ExecuteAssociation(HWND hWindow, const char* path, const char* name)
{
    CALL_STACK_MESSAGE3("ExecuteAssociation(, %s, %s)", path, name);

    if (ExecuteAssociationTlsIndex == TLS_OUT_OF_INDEXES || // TLS is not allocated (always false)
        TlsGetValue(ExecuteAssociationTlsIndex) == 0)       // it's not a recursive call
    {
        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // new call is not possible
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)1);

        //  MainWindow->ReleaseMenuNew();  // Windows are not built for multiple context menus

        if (Configuration.UseSalOpen)
        {
            // let's try to open the association via salopen.exe
            char execName[MAX_PATH + 200]; // + 200 is a reserve for longer names (stupid Windows)
            strcpy(execName, path);
            if (SalPathAppend(execName, name, MAX_PATH + 200) && SalOpenExecute(hWindow, execName))
            {
                if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // It is now possible to make a new call
                    TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
                return; // done, it started in the salopen.exe process
            }

            // if salopen.exe fails, we run it in the traditional way (danger of open handles in the directory)
        }

        IContextMenu2* menu = CreateIContextMenu2(hWindow, path, 1,
                                                  ReturnNameFromParam, (void*)name);
        if (menu != NULL)
        {
            CALL_STACK_MESSAGE1("ExecuteAssociation::1");
            HMENU h = CreatePopupMenu();
            if (h != NULL)
            {
                DWORD flags = CMF_DEFAULTONLY | ((GetKeyState(VK_SHIFT) & 0x8000) ? CMF_EXPLORE : 0);
                ExecuteAssociationAux2(menu, h, flags);

                UINT cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
                if (cmd == -1) // We did not find the default item -> let's try searching only among verbs
                {
                    DestroyMenu(h);
                    h = CreatePopupMenu();
                    if (h != NULL)
                    {
                        ExecuteAssociationAux2(menu, h, CMF_VERBSONLY | CMF_DEFAULTONLY);

                        cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
                        if (cmd == -1)
                            cmd = 0; // Let's try the "default verb" (index 0)
                    }
                }
                if (cmd != -1)
                {
                    CShellExecuteWnd shellExecuteWnd;
                    CMINVOKECOMMANDINFO ici;
                    ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
                    ici.fMask = 0;
                    ici.hwnd = shellExecuteWnd.Create(hWindow, "SEW: ExecuteAssociation cmd=%d", cmd);
                    ici.lpVerb = MAKEINTRESOURCE(cmd);
                    ici.lpParameters = NULL;
                    ici.lpDirectory = path;
                    ici.nShow = SW_SHOWNORMAL;
                    ici.dwHotKey = 0;
                    ici.hIcon = 0;

                    CALL_STACK_MESSAGE1("ExecuteAssociation::2");
                    ExecuteAssociationAux(menu, ici);
                }
                DestroyMenu(h);
            }
            CALL_STACK_MESSAGE1("ExecuteAssociation::3");
            ExecuteAssociationAux3(menu);
        }

        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // It is now possible to make a new call
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
    }
    else
    {
        // TRACE_E("Attempt to call ExecuteAssociation() recursively! (skipping this call...)");
        // ask whether Salamander should continue or generate a bug report
        if (SalMessageBox(hWindow, LoadStr(IDS_SHELLEXTBREAK4), SALAMANDER_TEXT_VERSION,
                          MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION | MSGBOXEX_SETFOREGROUND) == IDABORT)
        { // Let's break
            strcpy(BugReportReasonBreak, "Attempt to call ExecuteAssociation() recursively.");
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // Freeze this thread
            while (1)
                Sleep(1000);
        }
    }
}

// returns TRUE if it is "safe" to provide a special invisible window as the parent window to a shell extension,
// which can then be shot down by a shell extension via DestroyWindow (which normally closes Explorer, but Salam crashed)
// There are exceptions when it is necessary to pass the main window of Salamander as a parent
BOOL CanUseShellExecuteWndAsParent(const char* cmdName)
{
    // shellExecuteWnd cannot be used for Map Network Drive, otherwise it will freeze (disabling MainWindows->HWindow and the Map Network Drive window will not open)
    if (WindowsVistaAndLater && stricmp(cmdName, "connectNetworkDrive") == 0)
        return FALSE;

    // Under Windows 8, Open With misbehaved - when choosing a custom program, the Open dialog did not appear
    // https://forum.altap.cz/viewtopic.php?f=16&t=6730 and https://forum.altap.cz/viewtopic.php?t=6782
    // The problem is that the code returns from the invoke, but later MS reaches for the parent window that we have already destroyed
    // TODO: It would be a solution to let ShellExecuteWnd live (child window, stretched across the entire Salama surface, completely in its background)
    // we would always verify that he is alive (that he has not been shot) before handing it over
    // TODO2: so I tried the exercise design and right under W8 and Open With it doesn't work, the Open dialog is not modal to our main window (or the Find window)
    // for now, we will be passing the main window of Salamander in this case
    if (Windows8AndLater && stricmp(cmdName, "openas") == 0)
        return FALSE;

    // for other cases (most of them) ShellExecuteWnd can be used
    return TRUE;
}

/*  //const char *EnumFileNamesFunction_OneFile(int index, void *param)
//{
//  return (const char *)param;
//}

BOOL MakeFileAvailOfflineIfOneDriveOnWin81(HWND parent, const char *name)
{
  CALL_STACK_MESSAGE2("MakeFileAvailOfflineIfOneDriveOnWin81(, %s)", name);

  BOOL ret = TRUE;    // WARNING: missing support for OneDriveBusinessStorages, needs to be added !!!
  if (Windows8_1AndLater && OneDrivePath[0] != 0 && strlen(name) < MAX_PATH)
  {
    char path[MAX_PATH];
    char *cutName;
    strcpy_s(path, name);
    if (CutDirectory(path, &cutName) && SalPathIsPrefix(OneDrivePath, path)) // handling only under OneDrive folder
    {
      BOOL makeOffline = FALSE;
      WIN32_FIND_DATA findData;
      HANDLE hFind = FindFirstFile(name, &findData);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        makeOffline = IsFilePlaceholder(&findData);
        FindClose(hFind);
      }

      if (makeOffline)  // convert file to offline
      {
        // it doesn't work like this, it's asynchronous and the conversion to offline (download from the network)
        // can take up to a minute or not happen at all, we have no control over it,
        // we will wait until it can be done somehow through Win32 API
//        IContextMenu2 *menu = CreateIContextMenu2(parent, path, 1, EnumFileNamesFunction_OneFile, cutName);
//        if (menu != NULL)
//        {
//          CShellExecuteWnd shellExecuteWnd;
//          CMINVOKECOMMANDINFO ici;
//          ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
//          ici.fMask = 0;
//          ici.hwnd = shellExecuteWnd.Create(parent, "SEW: MakeFileAvailOfflineIfOneDriveOnWin81");
//          ici.lpVerb = "MakeAvailableOffline";
//          ici.lpParameters = NULL;
//          ici.lpDirectory = path;
//          ici.nShow = SW_SHOWNORMAL;
//          ici.dwHotKey = 0;
//          ici.hIcon = 0;
//
//          TRACE_I("SafeInvokeCommand");
//          ret = SafeInvokeCommand(menu, ici);
//
//          menu->Release();
//        }
      }
    }
  }
  return ret;
}*/