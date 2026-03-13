// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

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
// UseOwnRutine
//

BOOL UseOwnRutine(IDataObject* pDataObject)
{
    return DropSourcePanel != NULL || // either it is being dragged from us
           OurClipDataObject;         // or it is ours on the clipboard
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
        /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
   keep synchronized with the AppendMenu() calls below...
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
};
*/
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

    // if the FS did not respond or returned nonsense, give priority to Copy
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
                *dropEffect = DROPEFFECT_NONE; // drop target error
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
    isTgtFile = FALSE; // drop target is not a file yet -> we can still handle the operation ourselves
    tgtType = idtttWindows;
    RECT r;
    GetWindowRect(panel->GetListBoxHWND(), &r);
    int index = panel->GetIndex(pt.x - r.left, pt.y - r.top);
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS))
    {
        if (panel->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(panel->GetZIPArchive());
            if (format != 0) // we have found a supported archive
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) &&
                        (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0 || // Edit available? + is the effect copy or move?
                    index == 0 && panel->Dirs->Count > 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0 &&
                        (panel->GetZIPPath()[0] == 0 || panel->GetZIPPath()[0] == '\\' && panel->GetZIPPath()[1] == 0)) // drop onto a disk path
                {
                    tgtType = idtttArchive;
                    DWORD origEffect = *effect;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // trim the effect to copy+move

                    if (index >= 0 && index < panel->Dirs->Count) // drop on a directory
                    {
                        panel->SetDropTarget(index);
                        int l = (int)strlen(panel->GetZIPPath());
                        memcpy(panel->DropPath, panel->GetZIPPath(), l);
                        if (index == 0 && strcmp(panel->Dirs->At(index).Name, "..") == 0)
                        {
                            if (l > 0 && panel->DropPath[l - 1] == '\\')
                                panel->DropPath[--l] = 0;
                            int backSlash = 0;
                            if (l == 0) // drop-path will be a drive (".." leads out of the archive)
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
                                panel->SetDropTarget(-1); // hide the marker
                                return NULL;
                            }
                            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
                        }
                        return panel->DropPath;
                    }
                    else
                    {
                        panel->SetDropTarget(-1); // hide the marker
                        return panel->GetZIPPath();
                    }
                }
            }
        }
        else
        {
            if (panel->GetPluginFS()->NotEmpty())
            {
                if (srcType == 2 /* FS */) // drag&drop from FS to FS (any FS among themselves, restrictions only in CPluginFSInterfaceAbstract::CopyOrMoveFromFS)
                {
                    tgtType = idtttFullPluginFSPath;
                    int l = (int)strlen(panel->GetPluginFS()->GetPluginFSName());
                    memcpy(panel->DropPath, panel->GetPluginFS()->GetPluginFSName(), l);
                    panel->DropPath[l++] = ':';
                    if (index >= 0 && index < panel->Dirs->Count) // drop on a directory
                    {
                        if (panel == DropSourcePanel) // drag&drop within a single panel
                        {
                            if (panel->GetSelCount() == 0 &&
                                    index == panel->GetCaretIndex() ||
                                panel->GetSel(index) != 0)
                            {                             // directory onto itself
                                panel->SetDropTarget(-1); // hide the marker (the copy goes to the active directory, not the focused subdirectory)
                                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                                {
                                    tgtType = idtttWindows;
                                    return NULL; // without modifiers, the stop cursor remains (prevents accidental copying into the current directory)
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
                            { // the source FS can influence the allowed drop effects
                                DropSourcePanel->GetPluginFS()->GetAllowedDropEffects(1 /* drag-over-fs */, panel->DropPath,
                                                                                      effect);
                            }

                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }

                    panel->SetDropTarget(-1);                       // hide the marker
                    if (panel == DropSourcePanel && effect != NULL) // drag&drop within a single panel
                    {
                        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        {
                            tgtType = idtttWindows;
                            return NULL; // without modifiers, the stop cursor remains (prevents accidental copying into the current directory)
                        }
                        *effect &= ~DROPEFFECT_MOVE;
                    }
                    if (panel->GetPluginFS()->GetCurrentPath(panel->DropPath + l))
                    {
                        if (DropSourcePanel != NULL && DropSourcePanel->Is(ptPluginFS) &&
                            DropSourcePanel->GetPluginFS()->NotEmpty() && effect != NULL)
                        { // the source FS can influence the allowed drop effects
                            DropSourcePanel->GetPluginFS()->GetAllowedDropEffects(1 /* drag-over-fs */, panel->DropPath,
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
                    *effect &= posEff; // trim the effect to the FS capabilities

                    if (index >= 0 && index < panel->Dirs->Count) // drop on a directory
                    {
                        if (panel->GetPluginFS()->GetFullName(panel->Dirs->At(index),
                                                              (index == 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0) ? 2 : 1,
                                                              panel->DropPath, MAX_PATH))
                        {
                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }
                    panel->SetDropTarget(-1); // hide the marker
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
        panel->SetDropTarget(-1); // hide the marker
        return NULL;
    }

    if (index >= 0 && index < panel->Dirs->Count) // drop on a directory
    {
        if (panel == DropSourcePanel) // drag&drop within a single panel
        {
            if (panel->GetSelCount() == 0 &&
                    index == panel->GetCaretIndex() ||
                panel->GetSel(index) != 0)
            {                             // directory onto itself
                panel->SetDropTarget(-1); // hide the marker (the copy/shortcut goes to the active directory, not the focused subdirectory)
                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                    return NULL; // without modifiers, the stop cursor remains (prevents accidental copying into the current directory)
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
                panel->SetDropTarget(-1); // hide the marker
                return NULL;
            }
            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
        }
        return panel->DropPath;
    }
    else
    {
        if (index >= panel->Dirs->Count && index < panel->Dirs->Count + panel->Files->Count)
        {                                 // drop on a file
            if (panel == DropSourcePanel) // drag&drop within a single panel
            {
                if (panel->GetSelCount() == 0 &&
                        index == panel->GetCaretIndex() ||
                    panel->GetSel(index) != 0)
                {                             // file onto itself
                    panel->SetDropTarget(-1); // hide the marker (the copy/shortcut goes to the active directory, not the focused file)
                    if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        return NULL; // without modifiers, the stop cursor remains (prevents accidental copying into the current directory)
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
                panel->SetDropTarget(-1); // hide the marker
                return NULL;
            }
            strcpy(fullName + l, file->Name);

            // if it is a shortcut, we need to analyze it
            BOOL linkIsDir = FALSE;  // TRUE -> shortcut to a directory -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> shortcut to a file -> test archive
            char linkTgt[MAX_PATH];
            linkTgt[0] = 0;
            if (StrICmp(file->Ext, "lnk") == 0) // is it not a directory shortcut?
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
            if (linkIsDir) // link points to a directory, the path is fine, switch to it
            {
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, linkTgt);
                return panel->DropPath;
            }

            int format = PackerFormatConfig.PackIsArchive(linkIsFile ? linkTgt : fullName);
            if (format != 0) // found a supported archive
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) && // is edit available?
                    (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0)
                {
                    tgtType = idtttArchiveOnWinPath;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // trim the effect to copy+move
                    panel->SetDropTarget(index);
                    strcpy(panel->DropPath, linkIsFile ? linkTgt : fullName);
                    return panel->DropPath;
                }
                panel->SetDropTarget(-1); // hide the marker
                return NULL;
            }

            if (HasDropTarget(fullName))
            {
                isTgtFile = TRUE; // drop target file -> must be handled by the shell
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, fullName);
                return panel->DropPath;
            }
        }
        panel->SetDropTarget(-1); // hide the marker
    }

    if (panel == DropSourcePanel && effect != NULL) // drag&drop within a single panel
    {
        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
            return NULL; // without modifiers, the stop cursor remains (prevents accidental copying into the current directory)
        *effect &= ~DROPEFFECT_MOVE;
    }
    return panel->GetPath();
}

const char* GetCurrentDirClipboard(POINTL& pt, void* param, DWORD* effect, BOOL rButton,
                                   BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType)
{ // simpler version of the previous one for "paste" from the clipboard
    CFilesWindow* panel = (CFilesWindow*)param;
    isTgtFile = FALSE;
    tgtType = idtttWindows;
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS)) // not into archives or FS yet
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

    panel->SetDropTarget(-1); // hide the marker
    if (tgtType == idtttWindows &&
        !isFakeDataObject && (!ownRutine || shortcuts) && drop && // refresh panels
        (!MainWindow->LeftPanel->AutomaticRefresh ||
         !MainWindow->RightPanel->AutomaticRefresh ||
         MainWindow->LeftPanel->GetNetworkDrive() ||
         MainWindow->RightPanel->GetNetworkDrive()))
    {
        BOOL again = TRUE; // keep loading while files are still appearing
        int numLeft = MainWindow->LeftPanel->NumberOfItemsInCurDir;
        int numRight = MainWindow->RightPanel->NumberOfItemsInCurDir;
        while (again)
        {
            again = FALSE;
            Sleep(shortcuts ? 333 : 1000); // they run in another thread, give them time

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

        // let the panels refresh
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

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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
{ // ATTENTION: also used from CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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
        // the program does not depend on this call, so wrap it in a try/except block
        ret = menu->GetCommandString(idCmd, uType, pReserved, pszName, cchMax);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 19))
    {
        ICExceptionHasOccured++;
    }
    return ret;
}

void ShellActionAux5(UINT flags, CFilesWindow* panel, HMENU h)
{ // ATTENTION: also used from CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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
{ // ATTENTION: also used from CSalamanderGeneral::OpenNetworkContextMenu()
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
            dropSource->Release(); // that one is ours, hopefully it will not crash ;-)
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
    if (SalShExtSharedMemView != NULL) // shared memory is available (when drag&drop fails we cannot handle it)
    {
        CALL_STACK_MESSAGE1("ShellAction::archive/FS::drag_files");

        // create a "fake" directory
        char fakeRootDir[MAX_PATH];
        char* fakeName;
        if (SalGetTempFileName(NULL, "SAL", fakeRootDir, FALSE))
        {
            fakeName = fakeRootDir + strlen(fakeRootDir);
            // jr: I found a note on the net "Did implementing "IPersistStream" and providing the undocumented
            // "OleClipboardPersistOnFlush" format solve the problem?" -- in case we need to
            // get rid of the DROPFAKE method
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
                            // initialize shared memory
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
                                LastWndFromGetData = NULL; // just in case fakeDataObject->GetData is not called
                                hr = DoDragDrop(fakeDataObject, dropSource, allowedEffects, &dwEffect);
                                DropSourcePanel = NULL;
                                // read the drag&drop results
                                // Note: returns dwEffect == 0 for MOVE, so we introduce a workaround via dropSource->LastEffect,
                                // see "Handling Shell Data Transfer Scenarios", section "Handling Optimized Move Operations" for the reasons:
                                // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
                                // (in short: an optimized Move is performed, which means no copy to the target followed by deletion
                                //            of the original is not used. This prevents the source from accidentally deleting the original (it might not have been moved yet), it gets
                                //            the operation result DROPEFFECT_NONE or DROPEFFECT_COPY)
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
                                        else // archives + FS with Copy or Move (not both) + FS with Copy+Move when dragging with the right button, where the result from the right-button menu is not affected by changing the mouse cursor (the trick with the Copy cursor for a Move effect), so we take the result from the copy-hook (SalShExtSharedMemView->Operation)
                                            operation = SalShExtSharedMemView->Operation;
                                    }
                                    ReleaseMutex(SalShExtSharedMemMutex);

                                    if (!dropDone &&                 // copy-hook does not respond or the user selected Cancel in the drop menu (shown for right-button drag&drop)
                                        dwEffect != DROPEFFECT_NONE) // Cancel detection: because the copy-hook did not kick in, the returned drop effect is valid, so compare it to Cancel
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
                            fakeDataObject->Release(); // dataObject is released later in ShellActionAux7
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
            // based on the position for the context menu
            panel->GetContextMenuPos(pt);
        }
        else
        {
            // top-left corner of the panel
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
            if (lastSep != -1 && lastSep == i + 1) // two separators in a row, remove one because it is redundant
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
        // reading the classic DLGTEMPLATE see Altap Translator, but it probably will not be necessary to implement
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
    buff += 1; // y
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

// it tries to load aclui.dll and retrieve the dialog name stored under ID 103 (Security tab)
// if successful, it fills pageName with the dialog name and returns TRUE; otherwise it returns FALSE
BOOL GetACLUISecurityPageName(char* pageName, int pageNameMax)
{
    BOOL ret = FALSE;

    HINSTANCE hModule = LoadLibraryEx("aclui.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (hModule != NULL)
    {
        HRSRC hrsrc = FindResource(hModule, MAKEINTRESOURCE(103), RT_DIALOG); // 103 - Security tab
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
        // we cannot do any more actions in the archive yet
        return;
    }
    if (panel->Is(ptPluginFS) && dragFiles &&
        (!SalShExtRegistered ||
         !panel->GetPluginFS()->NotEmpty() ||
         !panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) &&    // FS umi "move from FS"
             !panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS))) // FS umi "copy from FS"
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
            panel->GetSelItems(count, indexes, action == saContextMenu); // we abandoned this (see GetSelItems): for context menus we start from the focused item and end with the item before the focused one (there is an intermediate wrap-around to the beginning of the name list); the system does the same, see Add To Windows Media Player List on MP3 files
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
            // if a single archive subdirectory is dragged, determine which one it is (to change the path
            // in the directory line and insert it into the command line)
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
                                  DROPEFFECT_COPY, 1 /* archiv */, NULL, action == saLeftDragFiles);

            if (indexes != NULL)
                delete[] (indexes);
            EndStopRefresh();

            if (dropDone) // let the operation complete
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
                if (SalShExtSharedMemView != NULL) // shared memory is available (when copy&paste fails we cannot handle it)
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
                                DWORD prefferedDropEffect = DROPEFFECT_COPY; // DROPEFFECT_MOVE (we used it for debugging purposes)

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
                                                    { // on a successful save - the system calls fakeDataObject->AddRef() +
                                                        // store the default drop effect
                                                        if (OpenClipboard(MainWindow->HWindow))
                                                        {
                                                            if (SetClipboardData(cfPrefDrop, effect) != NULL)
                                                                effect = NULL;
                                                            CloseClipboard();
                                                        }
                                                        else
                                                            TRACE_E("OpenClipboard() has failed!");

                                                        // our data are already on the clipboard (we must clean them up when Salamander exits)
                                                        OurDataOnClipboard = TRUE;

                                                        // initialize shared memory
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

                                                            delFakeDir = FALSE; // everything is OK, the fake directory is used
                                                            fakeDataObject->SetCutOrCopyDone();
                                                        }
                                                        else
                                                            TRACE_E("Shared memory is too small!");
                                                        ReleaseMutex(SalShExtSharedMemMutex);

                                                        if (!sharedMemOK) // if communication with salextx86.dll or salextx64.dll cannot be established, there is no point in leaving the data object on the clipboard
                                                        {
                                                            OleSetClipboard(NULL);
                                                            OurDataOnClipboard = FALSE; // theoretically redundant (should be set in fakeDataObject::Release() a few lines below)
                                                        }
                                                        // clipboard change, verify it...
                                                        IdleRefreshStates = TRUE;  // force a check of the state variables on the next Idle
                                                        IdleCheckClipboard = TRUE; // also let it check the clipboard

                                                        // for COPY clear the CutToClip flag
                                                        if (panel->CutToClipChanged)
                                                            panel->ClearCutToClipFlag(TRUE);
                                                        CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                                                        // for COPY clear the CutToClip flag on the other panel as well
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
                                        fakeDataObject->Release(); // if fakeDataObject is on the clipboard, it is released when the application exits or when removed from the clipboard
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
        // lower the thread priority to "normal" (so the operations do not overload the machine)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        int panelID = MainWindow->LeftPanel == panel ? PANEL_LEFT : PANEL_RIGHT;

        int selectedDirs = 0;
        if (count > 0)
        {
            // count how many directories are selected (the remaining selected items are files)
            int i;
            for (i = 0; i < panel->Dirs->Count; i++) // ".." cannot be selected, testing would be pointless
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
                // calculate the top-left corner of the context menu
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

                if (useSelection) // menu for items in the panel (click on an item)
                {
                    panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                      panel->GetListBoxHWND(), p.x, p.y, fscmItemsInPanel,
                                                      panelID, count - selectedDirs, selectedDirs);
                }
                else
                {
                    if (onlyPanelMenu) // panel menu (click below the items in the panel)
                    {
                        panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                          panel->GetListBoxHWND(), p.x, p.y, fscmPanel,
                                                          panelID, 0, 0);
                    }
                    else // menu for the current path (click on the change-drive button)
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
                    (panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMFS) || // FS umi "move from FS"
                     panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMFS)))  // FS umi "copy from FS"
                {
                    // if it drags a single FS subdirectory, determine which one (to change the path
                    // in the directory line and insert it into the command line)
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
                    panel->GetPluginFS()->GetAllowedDropEffects(0 /* start */, NULL, &allowedEffects);
                    DoDragFromArchiveOrFS(panel, dropDone, targetPath, operation, realDraggedPath,
                                          allowedEffects, 2 /* FS */, srcFSPath, action == saLeftDragFiles);
                    panel->GetPluginFS()->GetAllowedDropEffects(2 /* end */, NULL, NULL);

                    if (dropDone) // let the operation complete
                    {
                        char* p = DupStr(targetPath);
                        if (p != NULL)
                            PostMessage(panel->HWindow, WM_USER_DROPFROMFS, (WPARAM)p, operation);
                    }
                }
            }
        }

        // raise the thread priority again, the operation has finished
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
        return; // for safety do not allow other panel types further
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
                        // force opening the Security tab; unfortunately we must pass the string for the OS localization
                        ici.lpParameters = pageName;
                        if (!GetACLUISecurityPageName(pageName, 200))
                            lstrcpy(pageName, "Security"); // if the name could not be retrieved, use the English "Security" and accept that localized versions will quietly not work
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

                    // clipboard change, verify it...
                    IdleRefreshStates = TRUE;  // force a check of the state variables on the next Idle
                    IdleCheckClipboard = TRUE; // also let it check the clipboard

                    BOOL repaint = FALSE;
                    if (panel->CutToClipChanged)
                    {
                        // before CUT and COPY clear the CutToClip flag
                        panel->ClearCutToClipFlag(FALSE);
                        repaint = TRUE;
                    }
                    CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                    BOOL samePaths = panel->Is(ptDisk) && anotherPanel->Is(ptDisk) &&
                                     IsTheSamePath(panel->GetPath(), anotherPanel->GetPath());
                    if (anotherPanel->CutToClipChanged)
                    {
                        // before CUT and COPY clear the CutToClip flag on the other panel as well
                        anotherPanel->ClearCutToClipFlag(!samePaths);
                    }

                    if (action != saCopyToClipboard)
                    {
                        // for CUT set the file's CutToClip bit (ghosted)
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
                            if (samePaths) // select the file/directory in the other panel (quadratic complexity, not great ...)
                            {
                                if (idx < panel->Dirs->Count) // search among directories
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
                                else // search among files
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

                    // we also set the preferred drop effect + origin from Salamander
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

        // calculate the top-left corner of the context menu
        POINT pt;
        GetLeftTopCornert(&pt, posByMouse, useSelection, panel);

        if (panel->Is(ptZIPArchive))
        {
            if (useSelection) // only when it concerns items in the panel (not the current path in the panel)
            {
                // if command states need to be computed, do it (ArchiveMenu.UpdateItemsState uses them)
                MainWindow->OnEnterIdle();

                // let the enabler set the states and open the menu
                ArchiveMenu.UpdateItemsState();
                DWORD cmd = ArchiveMenu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                              pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                // send the result to the main window
                if (cmd != 0)
                    PostMessage(MainWindow->HWindow, WM_COMMAND, cmd, 0);
            }
            else
            {
                if (onlyPanelMenu) // context menu in the panel (behind the items) -> paste only
                {
                    // if command states need to be computed, do it (ArchivePanelMenu.UpdateItemsState uses them)
                    MainWindow->OnEnterIdle();

                    // let the enabler set the states and open the menu
                    ArchivePanelMenu.UpdateItemsState();

                    // if this is a "change directory" type paste, show it in the Paste item
                    char text[220];
                    char tail[50];
                    tail[0] = 0;

                    strcpy(text, LoadStr(IDS_ARCHIVEMENU_CLIPPASTE));

                    if (EnablerPastePath &&
                        (!panel->Is(ptDisk) || !EnablerPasteFiles) && // PasteFiles has priority
                        !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS has priority
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
            if (panel->ContextMenu != NULL) // we got a crash likely caused by recursive calls via the message loop in contextPopup.Track (panel->ContextMenu was nulled, apparently when leaving the inner recursive call)
            {
                TRACE_E("ShellAction::context_menu: panel->ContextMenu must be NULL (probably forbidden recursive call)!");
            }
            else // ptDisk
            {
                HMENU h = CreatePopupMenu();

                UINT flags = CMF_NORMAL | CMF_EXPLORE;
                // handle the Shift key press – extended context menu, under W2K it contains for example Run as...
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
                            // work around a bug in the TortoiseHg shell extension: it has a global variable with mapping of item IDs
                            // in the THg menu and commands, i.e. in our case when two menus are obtained
                            // (panel->ContextMenu and panel->ContextSubmenuNew) so this mapping gets overwritten
                            // by a later obtained menu (QueryContextMenu call). As a result, commands from the menu obtained earlier
                            // cannot be executed; in the original version this affected panel->ContextSubmenuNew,
                            // which contains all panel context menu commands except Open and Explore:
                            // to work around this problem we take advantage of the fact that from panel->ContextMenu menu we only take
                            // Open and Explore, i.e. Windows commands unaffected by this bug, so
                            // it is enough to obtain the menu (call QueryContextMenu) from panel->ContextSubmenuNew as
                            // the second one in order
                            // NOTE: we cannot always do this, because if only the New menu is added,
                            //           it is reasonable to retrieve panel->ContextMenu as the second menu instead,
                            //           so that its commands work correctly (e.g. THg does not add anything to the New menu at all,
                            //           so the problem does not occur there)
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

                BOOL clipCopy = FALSE;     // is it "our copy"?
                BOOL clipCut = FALSE;      // is it "our cut"?
                BOOL cmdDelete = FALSE;    // is it "our delete"?
                BOOL cmdMapNetDrv = FALSE; // is this our "Map Network Drive"? (only for a UNC root, let us not complicate life)
                DWORD cmd = 0;             // command number for the context menu (10000 = "our paste")
                char pastePath[MAX_PATH];  // buffer for the path where "our paste" is executed (if it happens)
                if (panel->ContextMenu != NULL && h != NULL)
                {
                    if (!alreadyHaveContextMenu)
                        ShellActionAux5(flags, panel, h);
                    RemoveUselessSeparatorsFromMenu(h);

                    char cmdName[2000]; // we intentionally use 2000 instead of 200; shell extensions sometimes write twice as much (idea: Unicode = 2 * "number of characters"), etc.
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
                                memset(&mi, 0, sizeof(mi)); // necessary here
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_SUBMENU;
                                mi.dwTypeData = itemName;
                                mi.cch = 500;
                                if (GetMenuItemInfo(h, i, TRUE, &mi))
                                {
                                    if (mi.hSubMenu == NULL && (mi.fType & MFT_SEPARATOR) == 0) // not a submenu nor a separator
                                    {
                                        if (AuxGetCommandString(panel->ContextMenu, mi.wID, GCS_VERB, NULL, cmdName, 200) == NOERROR)
                                        {
                                            if (stricmp(cmdName, "explore") == 0 || stricmp(cmdName, "open") == 0)
                                            {
                                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                                                if (bckgndMenuInsert == 2)
                                                    break; // we don't need any more items from here
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
                            if (bckgndMenuInsert > 0) // separate Explore + Open from the rest of the menu
                            {
                                // separator
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_TYPE;
                                mi.fType = MFT_SEPARATOR;
                                mi.dwTypeData = NULL;
                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                            }

                            /* used by the export_mnu.py script, which generates salmenu.mnu for Translator
   keep synchronized with the InsertMenuItem() calls below...
MENU_TEMPLATE_ITEM PanelBkgndMenu[] =
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MENU_EDIT_PASTE
  {MNTT_IT, IDS_PASTE_CHANGE_DIRECTORY
  {MNTT_IT, IDS_MENU_EDIT_PASTELINKS   
  {MNTT_PE, 0
};
*/

                            // add the Paste command (if it is a "change directory" type paste, show it in the Paste item)
                            char tail[50];
                            tail[0] = 0;
                            strcpy(itemName, LoadStr(IDS_MENU_EDIT_PASTE));
                            if (EnablerPastePath && !EnablerPasteFiles) // PasteFiles has priority
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

                            // insert a separator if it is not there yet
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
                        // originally adding the New item was called before ShellActionAux5, but
                        // under Windows XP calling ShellActionAux5 caused the new item
                        // to be removed (when an Edit/Copy operation had been performed beforehand)
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

                    if (GetMenuItemCount(h) > 0) // protection against a completely cut-out menu
                    {
                        CMenuPopup contextPopup;
                        contextPopup.SetTemplateMenu(h);
                        cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                                 pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                        //            for testing only -- show the menu via the Windows API
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
                            if (useSelection) // paste into a subdirectory of panel->GetPath()
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
                                    cmd = 10000; // the command will be executed elsewhere
                                }
                            }
                            else // paste do panel->GetPath()
                            {
                                strcpy(pastePath, panel->GetPath());
                                cmd = 10000; // the command will be executed elsewhere
                            }
                        }
                        clipCopy = (cmd < 5000 && stricmp(cmdName, "copy") == 0);
                        clipCut = (cmd < 5000 && stricmp(cmdName, "cut") == 0);
                        cmdDelete = useSelection && (cmd < 5000 && stricmp(cmdName, "delete") == 0);

                        // Map Network Drive command is 40 on XP, 43 on W2K, and only on Vista does it have a defined cmdName
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
                                panel->RenameFile(specialIndex); // only a drive is relevant (enabling "Rename" is not necessary)
                            }
                            else
                            {
                                BOOL releaseLeft = FALSE;                  // disconnect the left panel from the drive?
                                BOOL releaseRight = FALSE;                 // disconnect the right panel from the drive?
                                if (!useSelection && cmd < 5000 &&         // this is a context menu for a directory
                                    stricmp(cmdName, "properties") != 0 && // not necessary for properties
                                    stricmp(cmdName, "find") != 0 &&       // not necessary for Find
                                    stricmp(cmdName, "open") != 0 &&       // not necessary for Open
                                    stricmp(cmdName, "explore") != 0 &&    // not necessary for Explore
                                    stricmp(cmdName, "link") != 0)         // not necessary for Create Shortcut
                                {
                                    char root[MAX_PATH];
                                    GetRootPath(root, panel->GetPath());
                                    if (strlen(root) >= strlen(panel->GetPath())) // menu for the whole drive - because of commands like
                                    {                                             // for "format..." we must "take our hands off" the media
                                        CFilesWindow* win;
                                        int i;
                                        for (i = 0; i < 2; i++)
                                        {
                                            win = i == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
                                            if (HasTheSameRootPath(win->GetPath(), root)) // the same drive (UNC and normal)
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
                                    SetCurrentDirectoryToSystem(); // so the drive can also be unmapped from the panel
                                }
                                else
                                {
                                    SetCurrentDirectory(panel->GetPath()); // for files whose names contain spaces: to make Open With work even for Microsoft Paint (under W2K it failed – reported "d:\documents.bmp was not found" for the file "D:\Documents and Settings\petr\My Documents\example.bmp")
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

                                panel->FocusFirstNewItem = TRUE; // for both WinZip and its archives and the New menu (works well only when the panel is auto-refreshing)
                                if (cmd < 5000)
                                {
                                    BOOL changeToFixedDrv = cmd == 35; // "format" is not modal, requires switching to a fixed drive
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

                                    // we intercept cut/copy/paste, but just to be safe we still execute the refresh of the clipboard enablers:
                                    IdleRefreshStates = TRUE;  // force a check of the state variables on the next Idle
                                    IdleCheckClipboard = TRUE; // also let it check the clipboard

                                    if (releaseLeft && !changeToFixedDrv)
                                        MainWindow->LeftPanel->HandsOff(FALSE);
                                    if (releaseRight && !changeToFixedDrv)
                                        MainWindow->RightPanel->HandsOff(FALSE);

                                    //---  refresh directories that are not refreshed automatically
                                    // report the change in the current directory and its subdirectories (just to be safe)
                                    MainWindow->PostChangeOnPathNotification(panel->GetPath(), TRUE);
                                }
                                else
                                {
                                    if (panel->ContextSubmenuNew->MenuIsAssigned()) // an exception might have occurred
                                    {
                                        AuxInvokeCommand2(panel, (CMINVOKECOMMANDINFO*)&ici);

                                        //---  refresh directories that are not refreshed automatically
                                        // report the change in the current directory (a new file/directory can probably only be created there)
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

                if (cmd == 10000) // our own "paste" on pastePath
                {
                    if (!panel->ClipboardPaste(FALSE, FALSE, pastePath))
                        panel->ClipboardPastePath(); // regular paste failed, we probably just need to change the current path
                }
                else
                {
                    if (cmd == 10001) // our own "paste shortcuts" on pastePath
                    {
                        panel->ClipboardPaste(TRUE, FALSE, pastePath);
                    }
                    else
                    {
                        if (clipCopy) // our own "copy"
                        {
                            panel->ClipboardCopy(); // recursive ShellAction call
                        }
                        else
                        {
                            if (clipCut) // our own "cut"
                            {
                                panel->ClipboardCut(); // recursive ShellAction call
                            }
                            else
                            {
                                if (cmdDelete)
                                {
                                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_DELETEFILES, 0);
                                }
                                else
                                {
                                    if (cmdMapNetDrv) // is this our "Map Network Drive"? (only for a UNC root, let us not complicate life)
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

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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

    // temporarily lower the thread priority so a confused shell extension does not eat the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release it
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

extern DWORD ExecuteAssociationTlsIndex; // allows only one call at a time (prevents recursion) in each thread

void ExecuteAssociation(HWND hWindow, const char* path, const char* name)
{
    CALL_STACK_MESSAGE3("ExecuteAssociation(, %s, %s)", path, name);

    if (ExecuteAssociationTlsIndex == TLS_OUT_OF_INDEXES || // TLS is not allocated (always false)
        TlsGetValue(ExecuteAssociationTlsIndex) == 0)       // this is not a recursive call
    {
        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // a new call is not possible
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)1);

        //  MainWindow->ReleaseMenuNew();  // Windows are not built for multiple context menus

        if (Configuration.UseSalOpen)
        {
            // try to open the association via salopen.exe
            char execName[MAX_PATH + 200]; // +200 is a reserve for longer names (silly Windows)
            strcpy(execName, path);
            if (SalPathAppend(execName, name, MAX_PATH + 200) && SalOpenExecute(hWindow, execName))
            {
                if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // a new call is possible now
                    TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
                return; // done, it was launched in the salopen.exe process
            }

            // if salopen.exe fails, launch the classic way (there is a risk of open handles in the directory)
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
                if (cmd == -1) // we did not find default item -> try searching only among verbs
                {
                    DestroyMenu(h);
                    h = CreatePopupMenu();
                    if (h != NULL)
                    {
                        ExecuteAssociationAux2(menu, h, CMF_VERBSONLY | CMF_DEFAULTONLY);

                        cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
                        if (cmd == -1)
                            cmd = 0; // try the "default verb" (index 0)
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

        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // a new call is possible now
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
    }
    else
    {
        // TRACE_E("Attempt to call ExecuteAssociation() recursively! (skipping this call...)");
        // ask whether Salamander should continue or generate a bug report
        if (SalMessageBox(hWindow, LoadStr(IDS_SHELLEXTBREAK4), SALAMANDER_TEXT_VERSION,
                          MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION | MSGBOXEX_SETFOREGROUND) == IDABORT)
        { // break into the debugger
            strcpy(BugReportReasonBreak, "Attempt to call ExecuteAssociation() recursively.");
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // freeze this thread
            while (1)
                Sleep(1000);
        }
    }
}

// returns TRUE if it is "safe" to provide a special invisible window as the parent window to the shell extension,
// which the shell extension could kill via DestroyWindow (normally it closes Explorer, but used to crash Salamander)
// there are exceptions when the Salamander main window must be passed as the parent
BOOL CanUseShellExecuteWndAsParent(const char* cmdName)
{
    // for Map Network Drive we cannot use shellExecuteWnd, otherwise it hangs (MainWindow->HWindow gets disabled and the Map Network Drive window does not open)
    if (WindowsVistaAndLater && stricmp(cmdName, "connectNetworkDrive") == 0)
        return FALSE;

    // under Windows 8 Open With misbehaved – when selecting a custom program, the Open dialog did not appear
    // https://forum.altap.cz/viewtopic.php?f=16&t=6730 a https://forum.altap.cz/viewtopic.php?t=6782
    // the problem is that the code returns from Invoke, but later MS accesses the parent window, which we have already destroyed
    // TODO: one possible solution would be to keep ShellExecuteWnd alive (a child window stretched over the entire Salamander area, completely in the background)
    // we would just have to verify before passing it that it still exists (no one destroyed it)
    // TODO2: I tried the idea and specifically under W8 with Open With it does not work; the Open dialog is not modal to our main window (or the Find window)
    // for now we will pass Salamander's main window in this case
    if (Windows8AndLater && stricmp(cmdName, "openas") == 0)
        return FALSE;

    // for the other cases (the majority) we can use ShellExecuteWnd
    return TRUE;
}

/*
//const char *EnumFileNamesFunction_OneFile(int index, void *param)
//{
//  return (const char *)param;
//}

BOOL MakeFileAvailOfflineIfOneDriveOnWin81(HWND parent, const char *name)
{
  CALL_STACK_MESSAGE2("MakeFileAvailOfflineIfOneDriveOnWin81(, %s)", name);

  BOOL ret = TRUE;    // ATTENTION: missing support for OneDriveBusinessStorages, add it if needed!!!
  if (Windows8_1AndLater && OneDrivePath[0] != 0 && strlen(name) < MAX_PATH)
  {
    char path[MAX_PATH];
    char *cutName;
    strcpy_s(path, name);
    if (CutDirectory(path, &cutName) && SalPathIsPrefix(OneDrivePath, path)) // handle it only under the OneDrive folder
    {
      BOOL makeOffline = FALSE;
      WIN32_FIND_DATA findData;
      HANDLE hFind = FindFirstFile(name, &findData);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        makeOffline = IsFilePlaceholder(&findData);
        FindClose(hFind);
      }

      if (makeOffline)  // convert the file to offline
      {
        // doing it this stupid way will not work; it is asynchronous and that offline conversion (download from the network)
        // may happen a minute later or not at all; we have no control over it,
        // wait until it can be done via some Win32 API
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
}
*/