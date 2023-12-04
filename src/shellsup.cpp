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
// UseOwnRutine
//

BOOL UseOwnRutine(IDataObject* pDataObject)
{
    return DropSourcePanel != NULL || // bud se to tahne od nas
           OurClipDataObject;         // nebo je to od nas na clipboardu
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
        /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volani AppendMenu() dole...
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

    // pokud se FS nevyjadril nebo vratil blbost, dame prioritu Copy
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
                *dropEffect = DROPEFFECT_NONE; // chyba drop-targetu
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
    isTgtFile = FALSE; // zatim neni drop target file -> operaci muzeme resime i my
    tgtType = idtttWindows;
    RECT r;
    GetWindowRect(panel->GetListBoxHWND(), &r);
    int index = panel->GetIndex(pt.x - r.left, pt.y - r.top);
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS))
    {
        if (panel->Is(ptZIPArchive))
        {
            int format = PackerFormatConfig.PackIsArchive(panel->GetZIPArchive());
            if (format != 0) // nasli jsme podporovany archiv
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) &&
                        (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0 || // ma edit? + effect je copy nebo move?
                    index == 0 && panel->Dirs->Count > 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0 &&
                        (panel->GetZIPPath()[0] == 0 || panel->GetZIPPath()[0] == '\\' && panel->GetZIPPath()[1] == 0)) // drop na diskovou cestu
                {
                    tgtType = idtttArchive;
                    DWORD origEffect = *effect;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // orizneme effect na copy+move

                    if (index >= 0 && index < panel->Dirs->Count) // drop na adresari
                    {
                        panel->SetDropTarget(index);
                        int l = (int)strlen(panel->GetZIPPath());
                        memcpy(panel->DropPath, panel->GetZIPPath(), l);
                        if (index == 0 && strcmp(panel->Dirs->At(index).Name, "..") == 0)
                        {
                            if (l > 0 && panel->DropPath[l - 1] == '\\')
                                panel->DropPath[--l] = 0;
                            int backSlash = 0;
                            if (l == 0) // drop-path bude disk (".." vedou ven z archivu)
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
                                panel->SetDropTarget(-1); // schovat znacku
                                return NULL;
                            }
                            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
                        }
                        return panel->DropPath;
                    }
                    else
                    {
                        panel->SetDropTarget(-1); // schovat znacku
                        return panel->GetZIPPath();
                    }
                }
            }
        }
        else
        {
            if (panel->GetPluginFS()->NotEmpty())
            {
                if (srcType == 2 /* FS */) // drag&drop z FS na FS (libovolne FS mezi sebou, omezeni az v CPluginFSInterfaceAbstract::CopyOrMoveFromFS)
                {
                    tgtType = idtttFullPluginFSPath;
                    int l = (int)strlen(panel->GetPluginFS()->GetPluginFSName());
                    memcpy(panel->DropPath, panel->GetPluginFS()->GetPluginFSName(), l);
                    panel->DropPath[l++] = ':';
                    if (index >= 0 && index < panel->Dirs->Count) // drop na adresari
                    {
                        if (panel == DropSourcePanel) // drag&drop v ramci jednoho panelu
                        {
                            if (panel->GetSelCount() == 0 &&
                                    index == panel->GetCaretIndex() ||
                                panel->GetSel(index) != 0)
                            {                             // adresar sam do sebe
                                panel->SetDropTarget(-1); // schovat znacku (kopie pujde do akt. adresare, ne do fokusenyho podadresare)
                                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                                {
                                    tgtType = idtttWindows;
                                    return NULL; // bez modifikatoru zustava STOP kurzor (zabranuje nechtenemu kopirovani do aktualniho adresare)
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
                            { // zdrojovy FS muze ovlivnit povolene drop-effecty
                                DropSourcePanel->GetPluginFS()->GetAllowedDropEffects(1 /* drag-over-fs */, panel->DropPath,
                                                                                      effect);
                            }

                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }

                    panel->SetDropTarget(-1);                       // schovat znacku
                    if (panel == DropSourcePanel && effect != NULL) // drag&drop v ramci jednoho panelu
                    {
                        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        {
                            tgtType = idtttWindows;
                            return NULL; // bez modifikatoru zustava STOP kurzor (zabranuje nechtenemu kopirovani do aktualniho adresare)
                        }
                        *effect &= ~DROPEFFECT_MOVE;
                    }
                    if (panel->GetPluginFS()->GetCurrentPath(panel->DropPath + l))
                    {
                        if (DropSourcePanel != NULL && DropSourcePanel->Is(ptPluginFS) &&
                            DropSourcePanel->GetPluginFS()->NotEmpty() && effect != NULL)
                        { // zdrojovy FS muze ovlivnit povolene drop-effecty
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
                    *effect &= posEff; // orizneme effect na moznosti FS

                    if (index >= 0 && index < panel->Dirs->Count) // drop na adresari
                    {
                        if (panel->GetPluginFS()->GetFullName(panel->Dirs->At(index),
                                                              (index == 0 && strcmp(panel->Dirs->At(0).Name, "..") == 0) ? 2 : 1,
                                                              panel->DropPath, MAX_PATH))
                        {
                            panel->SetDropTarget(index);
                            return panel->DropPath;
                        }
                    }
                    panel->SetDropTarget(-1); // schovat znacku
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
        panel->SetDropTarget(-1); // schovat znacku
        return NULL;
    }

    if (index >= 0 && index < panel->Dirs->Count) // drop na adresari
    {
        if (panel == DropSourcePanel) // drag&drop v ramci jednoho panelu
        {
            if (panel->GetSelCount() == 0 &&
                    index == panel->GetCaretIndex() ||
                panel->GetSel(index) != 0)
            {                             // adresar sam do sebe
                panel->SetDropTarget(-1); // schovat znacku (kopie/shortcuta pujde do akt. adresare, ne do fokusenyho podadresare)
                if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                    return NULL; // bez modifikatoru zustava STOP kurzor (zabranuje nechtenemu kopirovani do aktualniho adresare)
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
                panel->SetDropTarget(-1); // schovat znacku
                return NULL;
            }
            strcpy(panel->DropPath + l, panel->Dirs->At(index).Name);
        }
        return panel->DropPath;
    }
    else
    {
        if (index >= panel->Dirs->Count && index < panel->Dirs->Count + panel->Files->Count)
        {                                 // drop na souboru
            if (panel == DropSourcePanel) // drag&drop v ramci jednoho panelu
            {
                if (panel->GetSelCount() == 0 &&
                        index == panel->GetCaretIndex() ||
                    panel->GetSel(index) != 0)
                {                             // soubor sam do sebe
                    panel->SetDropTarget(-1); // schovat znacku (kopie/shortcuta pujde do akt. adresare, ne do fokusenyho souboru)
                    if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
                        return NULL; // bez modifikatoru zustava STOP kurzor (zabranuje nechtenemu kopirovani do aktualniho adresare)
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
                panel->SetDropTarget(-1); // schovat znacku
                return NULL;
            }
            strcpy(fullName + l, file->Name);

            // jde-li o shortcutu, provedeme jeji analyzu
            BOOL linkIsDir = FALSE;  // TRUE -> short-cut na adresar -> ChangePathToDisk
            BOOL linkIsFile = FALSE; // TRUE -> short-cut na soubor -> test archivu
            char linkTgt[MAX_PATH];
            linkTgt[0] = 0;
            if (StrICmp(file->Ext, "lnk") == 0) // neni to short-cut adresare?
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
            if (linkIsDir) // link vede do adresare, cesta je o.k., prepneme se na ni
            {
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, linkTgt);
                return panel->DropPath;
            }

            int format = PackerFormatConfig.PackIsArchive(linkIsFile ? linkTgt : fullName);
            if (format != 0) // nasli jsme podporovany archiv
            {
                format--;
                if (PackerFormatConfig.GetUsePacker(format) && // ma edit?
                    (*effect & (DROPEFFECT_MOVE | DROPEFFECT_COPY)) != 0)
                {
                    tgtType = idtttArchiveOnWinPath;
                    *effect &= (DROPEFFECT_MOVE | DROPEFFECT_COPY); // orizneme effect na copy+move
                    panel->SetDropTarget(index);
                    strcpy(panel->DropPath, linkIsFile ? linkTgt : fullName);
                    return panel->DropPath;
                }
                panel->SetDropTarget(-1); // schovat znacku
                return NULL;
            }

            if (HasDropTarget(fullName))
            {
                isTgtFile = TRUE; // drop target file -> musi resit shell
                panel->SetDropTarget(index);
                strcpy(panel->DropPath, fullName);
                return panel->DropPath;
            }
        }
        panel->SetDropTarget(-1); // schovat znacku
    }

    if (panel == DropSourcePanel && effect != NULL) // drag&drop v ramci jednoho panelu
    {
        if (!rButton && (keyState & (MK_CONTROL | MK_SHIFT | MK_ALT)) == 0)
            return NULL; // bez modifikatoru zustava STOP kurzor (zabranuje nechtenemu kopirovani do aktualniho adresare)
        *effect &= ~DROPEFFECT_MOVE;
    }
    return panel->GetPath();
}

const char* GetCurrentDirClipboard(POINTL& pt, void* param, DWORD* effect, BOOL rButton,
                                   BOOL& isTgtFile, DWORD keyState, int& tgtType, int srcType)
{ // jednodussi verze predchoziho pro "paste" z clipboardu
    CFilesWindow* panel = (CFilesWindow*)param;
    isTgtFile = FALSE;
    tgtType = idtttWindows;
    if (panel->Is(ptZIPArchive) || panel->Is(ptPluginFS)) // do archivu a FS zatim ne
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

    panel->SetDropTarget(-1); // schovat znacku
    if (tgtType == idtttWindows &&
        !isFakeDataObject && (!ownRutine || shortcuts) && drop && // refresh panels
        (!MainWindow->LeftPanel->AutomaticRefresh ||
         !MainWindow->RightPanel->AutomaticRefresh ||
         MainWindow->LeftPanel->GetNetworkDrive() ||
         MainWindow->RightPanel->GetNetworkDrive()))
    {
        BOOL again = TRUE; // dokud soubory pribyvaji, nacitame
        int numLeft = MainWindow->LeftPanel->NumberOfItemsInCurDir;
        int numRight = MainWindow->RightPanel->NumberOfItemsInCurDir;
        while (again)
        {
            again = FALSE;
            Sleep(shortcuts ? 333 : 1000); // makaj v jinym threadu, nechame jim cas

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

        // nechame refreshnout panely
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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
{ // POZOR: pouziva se i z CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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
        // roky nam chodi pady pri volani IContextMenu2::GetCommandString()
        // pro chod programu neni toto volani zasadni, takze ho osetrime do try/except bloku
        ret = menu->GetCommandString(idCmd, uType, pReserved, pszName, cchMax);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 19))
    {
        ICExceptionHasOccured++;
    }
    return ret;
}

void ShellActionAux5(UINT flags, CFilesWindow* panel, HMENU h)
{ // POZOR: pouziva se i z CSalamanderGeneral::OpenNetworkContextMenu()
    CALL_STACK_MESSAGE_NONE

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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
{ // POZOR: pouziva se i z CSalamanderGeneral::OpenNetworkContextMenu()
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
            dropSource->Release(); // ten je nas, snad nespadne ;-)
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
    if (SalShExtSharedMemView != NULL) // sdilena pamet je k dispozici (pri chybe drag&drop neumime)
    {
        CALL_STACK_MESSAGE1("ShellAction::archive/FS::drag_files");

        // vytvorime "fake" adresar
        char fakeRootDir[MAX_PATH];
        char* fakeName;
        if (SalGetTempFileName(NULL, "SAL", fakeRootDir, FALSE))
        {
            fakeName = fakeRootDir + strlen(fakeRootDir);
            // jr: Nasel jsem na netu zminku "Did implementing "IPersistStream" and providing the undocumented
            // "OleClipboardPersistOnFlush" format solve the problem?" -- pro pripad, ze bychom se potrebovali
            // zbavit DROPFAKE metody
            if (SalPathAppend(fakeRootDir, "DROPFAKE", MAX_PATH))
            {
                if (CreateDirectory(fakeRootDir, NULL))
                {
                    // vytvorime objekty pro drag&drop
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
                            // inicializace sdilene pameti
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
                                LastWndFromGetData = NULL; // pro jistotu, kdyby se nevolalo fakeDataObject->GetData
                                hr = DoDragDrop(fakeDataObject, dropSource, allowedEffects, &dwEffect);
                                DropSourcePanel = NULL;
                                // precteme vysledky drag&dropu
                                // Poznamka: vraci dwEffect == 0 pri MOVE, proto zavadime obezlicku pres dropSource->LastEffect,
                                // duvody viz "Handling Shell Data Transfer Scenarios" sekce "Handling Optimized Move Operations":
                                // http://msdn.microsoft.com/en-us/library/windows/desktop/bb776904%28v=vs.85%29.aspx
                                // (zkracene: dela se optimalizovany Move, coz znamena ze se nedela kopie do cile nasledovana mazanim
                                //            originalu, aby zdroj nechtene nesmazal original (jeste nemusi byt presunuty), dostane
                                //            vysledek operace DROPEFFECT_NONE nebo DROPEFFECT_COPY)
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
                                        else // archivy + FS s Copy nebo Move (ne oboje) + FS s Copy+Move pri tazeni pravym tlacitkem, kdy vysledek z menu na pravem tlacitku neni ovlivnen zmenou kurzoru mysi (trik s Copy kurzorem pri Move effectu), takze vysledek bereme z copy-hooku (SalShExtSharedMemView->Operation)
                                            operation = SalShExtSharedMemView->Operation;
                                    }
                                    ReleaseMutex(SalShExtSharedMemMutex);

                                    if (!dropDone &&                 // nereaguje copy-hook nebo dal user Cancel v drop-menu (ukazuje se pri D&D pravym tlacitkem)
                                        dwEffect != DROPEFFECT_NONE) // detekce Cancelu: vzhledem k tomu, ze nezabral copy-hook, je vraceny drop-effect platny, tedy ho porovname na Cancel
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
                            fakeDataObject->Release(); // dataObject se uvolni pozdeji v ShellActionAux7
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
        // souradnice dle pozice mysi
        DWORD pos = GetMessagePos();
        pt->x = GET_X_LPARAM(pos);
        pt->y = GET_Y_LPARAM(pos);
    }
    else
    {
        if (useSelection)
        {
            // dle pozice pro kontextove menu
            panel->GetContextMenuPos(pt);
        }
        else
        {
            // levy horni roh panelu
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
            if (lastSep != -1 && lastSep == i + 1) // dva separatory po sobe, jeden smazeme, je nadbytecny
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
        // cteni klasickeho DLGTEMPLATE viz altap translator, pravdepodobne ale nebude potreba implementovat
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

// pokusi se nacist aclui.dll a vytahnout nazev dialogu ulozeneho s ID 103 (zalozka Security)
// v pripade uspechu naplni nazev dialogu do pageName a vrati TRUE; jinak vrati FALSE
BOOL GetACLUISecurityPageName(char* pageName, int pageNameMax)
{
    BOOL ret = FALSE;

    HINSTANCE hModule = LoadLibraryEx("aclui.dll", NULL, LOAD_LIBRARY_AS_DATAFILE);

    if (hModule != NULL)
    {
        HRSRC hrsrc = FindResource(hModule, MAKEINTRESOURCE(103), RT_DIALOG); // 103 - security zalozka
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
    { // bez souboru a adresaru -> neni co delat
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
        // dalsi cinnosti v archivu zatim neumime
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

    //  MainWindow->ReleaseMenuNew();  // Windows nejsou staveny na vic kontextovych menu

    BeginStopRefresh(); // zadne refreshe nepotrebujeme

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
            panel->GetSelItems(count, indexes, action == saContextMenu); // od tohoto jsme ustoupili (viz GetSelItems): pro kontextova menu zaciname od fokusle polozky a koncime polozku pres fokusem (je tam mezilehle vraceni na zacatek seznamu jmen) (system to tak dela taky, viz Add To Windows Media Player List na MP3 souborech)
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
            // pokud tahne jediny podadresar archivu, zjistime ktery (pro zmenu cesty
            // v directory-line a vlozeni do command-line)
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

            if (dropDone) // nechame provest operaci
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
                if (SalShExtSharedMemView != NULL) // sdilena pamet je k dispozici (pri chybe copy&paste neumime)
                {
                    CALL_STACK_MESSAGE1("ShellAction::archive::clipcopy_files");

                    // vytvorime "fake" adresar
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
                                DWORD prefferedDropEffect = DROPEFFECT_COPY; // DROPEFFECT_MOVE (pouzivali jsme pro ladici ucely)

                                // vytvorime objekty pro copy&paste
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
                                                    { // pri uspesnem ulozeni system vola fakeDataObject->AddRef() +
                                                        // ulozime default drop-effect
                                                        if (OpenClipboard(MainWindow->HWindow))
                                                        {
                                                            if (SetClipboardData(cfPrefDrop, effect) != NULL)
                                                                effect = NULL;
                                                            CloseClipboard();
                                                        }
                                                        else
                                                            TRACE_E("OpenClipboard() has failed!");

                                                        // uz jsou na clipboardu nase data (pri exitu Salama je musime zrusit)
                                                        OurDataOnClipboard = TRUE;

                                                        // inicializace sdilene pameti
                                                        WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
                                                        BOOL sharedMemOK = SalShExtSharedMemView->Size >= sizeof(CSalShExtSharedMem);
                                                        if (sharedMemOK)
                                                        {
                                                            SalShExtSharedMemView->DoPasteFromSalamander = TRUE;
                                                            SalShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // inicializujeme na 1 minutu pred zalozenim data-objektu
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

                                                            delFakeDir = FALSE; // vse je OK, fake-dir se uzije
                                                            fakeDataObject->SetCutOrCopyDone();
                                                        }
                                                        else
                                                            TRACE_E("Shared memory is too small!");
                                                        ReleaseMutex(SalShExtSharedMemMutex);

                                                        if (!sharedMemOK) // pokud neni mozne navazat komunikaci se salextx86.dll ani salextx64.dll, nema smysl nechavat data-object na clipboardu
                                                        {
                                                            OleSetClipboard(NULL);
                                                            OurDataOnClipboard = FALSE; // teoreticky zbytecne (melo by se nastavit v Release() fakeDataObjectu - o par radek nize)
                                                        }
                                                        // zmena clipboardu, overime to ...
                                                        IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                                                        IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

                                                        // pri COPY vymazeme flag CutToClip
                                                        if (panel->CutToClipChanged)
                                                            panel->ClearCutToClipFlag(TRUE);
                                                        CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                                                        // pri COPY vymazeme flag CutToClip i u druheho panelu
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
                                        fakeDataObject->Release(); // pokud je fakeDataObject na clipboardu, uvolni se s koncem aplikace nebo uvolnenim z clipboardu
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
        // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        int panelID = MainWindow->LeftPanel == panel ? PANEL_LEFT : PANEL_RIGHT;

        int selectedDirs = 0;
        if (count > 0)
        {
            // spocitame kolik adresaru je oznaceno (zbytek oznacenych polozek jsou soubory)
            int i;
            for (i = 0; i < panel->Dirs->Count; i++) // ".." nemuzou byt oznaceny, test by byl zbytecny
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
                // napocitame levy horni roh kontextoveho menu
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

                if (useSelection) // menu pro polozky v panelu (klik na polozce)
                {
                    panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                      panel->GetListBoxHWND(), p.x, p.y, fscmItemsInPanel,
                                                      panelID, count - selectedDirs, selectedDirs);
                }
                else
                {
                    if (onlyPanelMenu) // menu panelu (kliknuti za polozkami v panelu)
                    {
                        panel->GetPluginFS()->ContextMenu(panel->GetPluginFS()->GetPluginFSName(),
                                                          panel->GetListBoxHWND(), p.x, p.y, fscmPanel,
                                                          panelID, 0, 0);
                    }
                    else // menu pro aktualni cestu (kliknuti na change-drive buttonu)
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
                    // pokud tahne jediny podadresar FS, zjistime ktery (pro zmenu cesty
                    // v directory-line a vlozeni do command-line)
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

                    if (dropDone) // nechame provest operaci
                    {
                        char* p = DupStr(targetPath);
                        if (p != NULL)
                            PostMessage(panel->HWindow, WM_USER_DROPFROMFS, (WPARAM)p, operation);
                    }
                }
            }
        }

        // opet zvysime prioritu threadu, operace dobehla
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
        return; // pro jistotu dal nepustime jine typy panelu
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
                        // vynutime otevreni zalozky Security; bohuzel je treba predavat string pro danou lokalizaci OS
                        ici.lpParameters = pageName;
                        if (!GetACLUISecurityPageName(pageName, 200))
                            lstrcpy(pageName, "Security"); // pokud se nepodarilo nazev vytahnout, dame anglicke "Security" a v lokalizovanych verzich tise nebudeme fungovat
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

                    // zmena clipboardu, overime to ...
                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

                    BOOL repaint = FALSE;
                    if (panel->CutToClipChanged)
                    {
                        // pred CUT a COPY vymazeme flag CutToClip
                        panel->ClearCutToClipFlag(FALSE);
                        repaint = TRUE;
                    }
                    CFilesWindow* anotherPanel = MainWindow->LeftPanel == panel ? MainWindow->RightPanel : MainWindow->LeftPanel;
                    BOOL samePaths = panel->Is(ptDisk) && anotherPanel->Is(ptDisk) &&
                                     IsTheSamePath(panel->GetPath(), anotherPanel->GetPath());
                    if (anotherPanel->CutToClipChanged)
                    {
                        // pred CUT a COPY vymazeme flag CutToClip i u druheho panelu
                        anotherPanel->ClearCutToClipFlag(!samePaths);
                    }

                    if (action != saCopyToClipboard)
                    {
                        // v CUT pripade nastavime souboru bit CutToClip (ghosted)
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
                            if (samePaths) // oznacime soubor/adresar v druhem panelu (kvadr. slozitost, nezajem ...)
                            {
                                if (idx < panel->Dirs->Count) // hledame mezi adresari
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
                                else // hledame mezi soubory
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

                    // nastavime jeste preffered drop effect + puvod ze Salama
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

        // napocitame levy horni roh kontextoveho menu
        POINT pt;
        GetLeftTopCornert(&pt, posByMouse, useSelection, panel);

        if (panel->Is(ptZIPArchive))
        {
            if (useSelection) // jen pokud jde o polozky v panelu (ne o aktualni cestu v panelu)
            {
                // pokud je treba napocitat stavy prikazu, provedeme to (ArchiveMenu.UpdateItemsState je pouziva)
                MainWindow->OnEnterIdle();

                // necham nastavit stavy dle enableru a otevru menu
                ArchiveMenu.UpdateItemsState();
                DWORD cmd = ArchiveMenu.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                              pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                // vysledek posleme hlavnimu oknu
                if (cmd != 0)
                    PostMessage(MainWindow->HWindow, WM_COMMAND, cmd, 0);
            }
            else
            {
                if (onlyPanelMenu) // kontextove menu v panelu (za polozkami) -> jen paste
                {
                    // pokud je treba napocitat stavy prikazu, provedeme to (ArchivePanelMenu.UpdateItemsState je pouziva)
                    MainWindow->OnEnterIdle();

                    // necham nastavit stavy dle enableru a otevru menu
                    ArchivePanelMenu.UpdateItemsState();

                    // Pokud jde o paste typu "zmena adresare", zobrazime to do polozky Paste
                    char text[220];
                    char tail[50];
                    tail[0] = 0;

                    strcpy(text, LoadStr(IDS_ARCHIVEMENU_CLIPPASTE));

                    if (EnablerPastePath &&
                        (!panel->Is(ptDisk) || !EnablerPasteFiles) && // PasteFiles je prioritni
                        !EnablerPasteFilesToArcOrFS)                  // PasteFilesToArcOrFS je prioritni
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
                    // vysledek posleme hlavnimu oknu
                    if (cmd != 0)
                        PostMessage(MainWindow->HWindow, WM_COMMAND, cmd, 0);
                }
            }
        }
        else
        {
            BOOL uncRootPath = FALSE;
            if (panel->ContextMenu != NULL) // dostali jsme padacku zrejme zpusobenou rekurzivnim volanim pres message-loopu v contextPopup.Track (doslo k nulovani panel->ContextMenu, zrejme prave pri opusteni vnitrniho volani rekurze)
            {
                TRACE_E("ShellAction::context_menu: panel->ContextMenu must be NULL (probably forbidden recursive call)!");
            }
            else // ptDisk
            {
                HMENU h = CreatePopupMenu();

                UINT flags = CMF_NORMAL | CMF_EXPLORE;
                // osetrime stisknuty shift - rozsirene kontextove menu, pod W2K je tam napriklad Run as...
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
                            // bypass bugy shell-extensiony TortoiseHg: ma globalku s mapovanim ID polozek
                            // v menu a prikazu THg, tedy v nasem pripade, kdy se ziskavaji dve menu
                            // (panel->ContextMenu a panel->ContextSubmenuNew) dojde k premazani tohoto
                            // mapovani pozdeji ziskanym menu (volani QueryContextMenu), tedy prikazy z drive
                            // ziskaneho menu nejdou spoustet, v puvodni verzi slo o menu panel->ContextSubmenuNew,
                            // ve kterem jsou u paneloveho kontextoveho menu vsechny prikazy az na Open a Explore:
                            // k obejiti tohoto problemu vyuzijeme to, ze z menu panel->ContextMenu bereme
                            // jen Open a Explore, tedy prikazy woken, ktere touto chybou netrpi, takze
                            // staci jen ziskat menu (volat QueryContextMenu) od panel->ContextSubmenuNew jako
                            // druhe v poradi
                            // POZNAMKA: vzdycky to delat nemuzeme, protoze pokud se pridava jen menu New,
                            //           je naopak rozumejsi ziskat druhe v poradi menu panel->ContextMenu,
                            //           aby fungovaly jeho prikazy (napr. THg se do menu New vubec nepridava,
                            //           takze zadny problem nevznika)
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

                BOOL clipCopy = FALSE;     // jde o "nase copy"?
                BOOL clipCut = FALSE;      // jde o "nase cut"?
                BOOL cmdDelete = FALSE;    // jde o "nase delete"?
                BOOL cmdMapNetDrv = FALSE; // jde o "nase Map Network Drive"? (jen UNC root, nebudeme si komplikovat zivot)
                DWORD cmd = 0;             // cislo prikazu pro kontext. menu (10000 = "nas paste")
                char pastePath[MAX_PATH];  // buffer pro cestu, na kterou se provede "nas paste" (nastane-li)
                if (panel->ContextMenu != NULL && h != NULL)
                {
                    if (!alreadyHaveContextMenu)
                        ShellActionAux5(flags, panel, h);
                    RemoveUselessSeparatorsFromMenu(h);

                    char cmdName[2000]; // schvalne mame 2000 misto 200, shell-extensiony obcas zapisuji dvojnasobek (uvaha: unicode = 2 * "pocet znaku"), atp.
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
                                memset(&mi, 0, sizeof(mi)); // nutne zde
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_STATE | MIIM_TYPE | MIIM_ID | MIIM_SUBMENU;
                                mi.dwTypeData = itemName;
                                mi.cch = 500;
                                if (GetMenuItemInfo(h, i, TRUE, &mi))
                                {
                                    if (mi.hSubMenu == NULL && (mi.fType & MFT_SEPARATOR) == 0) // neni submenu ani separator
                                    {
                                        if (AuxGetCommandString(panel->ContextMenu, mi.wID, GCS_VERB, NULL, cmdName, 200) == NOERROR)
                                        {
                                            if (stricmp(cmdName, "explore") == 0 || stricmp(cmdName, "open") == 0)
                                            {
                                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                                                if (bckgndMenuInsert == 2)
                                                    break; // vic polozek odsud nepotrebujeme
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
                            if (bckgndMenuInsert > 0) // oddelime Explore + Open od zbytku menu
                            {
                                // separator
                                mi.cbSize = sizeof(mi);
                                mi.fMask = MIIM_TYPE;
                                mi.fType = MFT_SEPARATOR;
                                mi.dwTypeData = NULL;
                                InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);
                            }

                            /* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
   udrzovat synchronizovane s volanim InsertMenuItem() dole...
MENU_TEMPLATE_ITEM PanelBkgndMenu[] =
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MENU_EDIT_PASTE
  {MNTT_IT, IDS_PASTE_CHANGE_DIRECTORY
  {MNTT_IT, IDS_MENU_EDIT_PASTELINKS   
  {MNTT_PE, 0
};
*/

                            // pridame prikaz Paste (pokud jde o paste typu "zmena adresare", zobrazime to do polozky Paste)
                            char tail[50];
                            tail[0] = 0;
                            strcpy(itemName, LoadStr(IDS_MENU_EDIT_PASTE));
                            if (EnablerPastePath && !EnablerPasteFiles) // PasteFiles je prioritni
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

                            // pridame prikaz Paste Shortcuts
                            mi.cbSize = sizeof(mi);
                            mi.fMask = MIIM_STATE | MIIM_ID | MIIM_TYPE;
                            mi.fType = MFT_STRING;
                            mi.fState = EnablerPasteLinksOnDisk ? MFS_ENABLED : MFS_DISABLED;
                            mi.dwTypeData = LoadStr(IDS_MENU_EDIT_PASTELINKS);
                            mi.wID = 10001;
                            InsertMenuItem(bckgndMenu, bckgndMenuInsert++, TRUE, &mi);

                            // pokud uz tam neni, vlozime separator
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
                        // puvodne bylo pridani New polozky volano pred ShellActionAux5, ale
                        // pod Windows XP dochazelo pri zavolani ShellActionAux5 k smazani polozky
                        // New (v pripade, ze byla napred provedena Edit/Copy operace)
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

                    if (GetMenuItemCount(h) > 0) // ochrana proti zcela vyrezanemu menu
                    {
                        CMenuPopup contextPopup;
                        contextPopup.SetTemplateMenu(h);
                        cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                                 pt.x, pt.y, panel->GetListBoxHWND(), NULL);
                        //            pouze pro testovani -- zobrazime menu pres Windows API
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
                            if (useSelection) // paste do podadresare panel->GetPath()
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
                                    cmd = 10000; // prikaz bude proveden jinde
                                }
                            }
                            else // paste do panel->GetPath()
                            {
                                strcpy(pastePath, panel->GetPath());
                                cmd = 10000; // prikaz bude proveden jinde
                            }
                        }
                        clipCopy = (cmd < 5000 && stricmp(cmdName, "copy") == 0);
                        clipCut = (cmd < 5000 && stricmp(cmdName, "cut") == 0);
                        cmdDelete = useSelection && (cmd < 5000 && stricmp(cmdName, "delete") == 0);

                        // prikaz Map Network Drive je pod XP 40, pod W2K 43 a teprve pod Vistou ma definovane cmdName
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
                                panel->RenameFile(specialIndex); // v uvahu pripada jen disk (enablovani "Rename" neni potreba)
                            }
                            else
                            {
                                BOOL releaseLeft = FALSE;                  // odpojit levy panel od disku?
                                BOOL releaseRight = FALSE;                 // odpojit pravy panel od disku?
                                if (!useSelection && cmd < 5000 &&         // jde o kontextove menu pro adresar
                                    stricmp(cmdName, "properties") != 0 && // u properties neni nutne
                                    stricmp(cmdName, "find") != 0 &&       // u find neni nutne
                                    stricmp(cmdName, "open") != 0 &&       // u open neni nutne
                                    stricmp(cmdName, "explore") != 0 &&    // u explore neni nutne
                                    stricmp(cmdName, "link") != 0)         // u create-short-cut neni nutne
                                {
                                    char root[MAX_PATH];
                                    GetRootPath(root, panel->GetPath());
                                    if (strlen(root) >= strlen(panel->GetPath())) // menu pro cely disk - kvuli prikazum typu
                                    {                                             // "format..." musime "dat ruce pryc" od media
                                        CFilesWindow* win;
                                        int i;
                                        for (i = 0; i < 2; i++)
                                        {
                                            win = i == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
                                            if (HasTheSameRootPath(win->GetPath(), root)) // stejny disk (UNC i normal)
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
                                    SetCurrentDirectoryToSystem(); // aby sel odmapovat i disk z panelu
                                }
                                else
                                {
                                    SetCurrentDirectory(panel->GetPath()); // pro soubory obsahujici ve jmene mezery: aby fungovalo Open With i pro Microsoft Paint (blblo pod W2K - psalo "d:\documents.bmp was not found" pro soubor "D:\Documents and Settings\petr\My Documents\example.bmp")
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

                                panel->FocusFirstNewItem = TRUE; // jak pro WinZip a jeho archivy, tak pro menu New (funguje dobre jen pri autorefreshovani panelu)
                                if (cmd < 5000)
                                {
                                    BOOL changeToFixedDrv = cmd == 35; // "format" neni modalni, nutna zmena na fixed drive
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

                                    // cut/copy/paste chytame, ale pro jistotu provedeme stejne refresh enableru clipboardu
                                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

                                    if (releaseLeft && !changeToFixedDrv)
                                        MainWindow->LeftPanel->HandsOff(FALSE);
                                    if (releaseRight && !changeToFixedDrv)
                                        MainWindow->RightPanel->HandsOff(FALSE);

                                    //---  refresh neautomaticky refreshovanych adresaru
                                    // ohlasime zmenu v aktualnim adresari a jeho podadresarich (radsi, buh vi co se spustilo)
                                    MainWindow->PostChangeOnPathNotification(panel->GetPath(), TRUE);
                                }
                                else
                                {
                                    if (panel->ContextSubmenuNew->MenuIsAssigned()) // mohlo dojit k exceptione
                                    {
                                        AuxInvokeCommand2(panel, (CMINVOKECOMMANDINFO*)&ici);

                                        //---  refresh neautomaticky refreshovanych adresaru
                                        // ohlasime zmenu v aktualnim adresari (novy soubor/adresar lze vytvorit snad jen v nem)
                                        MainWindow->PostChangeOnPathNotification(panel->GetPath(), FALSE);
                                    }
                                }

                                if (GetLogicalDrives() < disks) // odmapovani
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

                if (cmd == 10000) // nase vlastni "paste" na pastePath
                {
                    if (!panel->ClipboardPaste(FALSE, FALSE, pastePath))
                        panel->ClipboardPastePath(); // klasicky paste selhal, nejspis mame jen zmenit aktualni cestu
                }
                else
                {
                    if (cmd == 10001) // nase vlastni "paste shortcuts" na pastePath
                    {
                        panel->ClipboardPaste(TRUE, FALSE, pastePath);
                    }
                    else
                    {
                        if (clipCopy) // nase vlastni "copy"
                        {
                            panel->ClipboardCopy(); // rekurzivni volani ShellAction
                        }
                        else
                        {
                            if (clipCut) // nase vlastni "cut"
                            {
                                panel->ClipboardCut(); // rekurzivni volani ShellAction
                            }
                            else
                            {
                                if (cmdDelete)
                                {
                                    PostMessage(MainWindow->HWindow, WM_COMMAND, CM_DELETEFILES, 0);
                                }
                                else
                                {
                                    if (cmdMapNetDrv) // jde o "nase Map Network Drive"? (jen UNC root, nebudeme si komplikovat zivot)
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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

extern DWORD ExecuteAssociationTlsIndex; // dovoli jen jedno volani zaroven (zamezi rekurzi) v kazdem threadu

void ExecuteAssociation(HWND hWindow, const char* path, const char* name)
{
    CALL_STACK_MESSAGE3("ExecuteAssociation(, %s, %s)", path, name);

    if (ExecuteAssociationTlsIndex == TLS_OUT_OF_INDEXES || // neni alokovany TLS (always false)
        TlsGetValue(ExecuteAssociationTlsIndex) == 0)       // nejde o rekurzivni volani
    {
        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // nove volani neni mozne
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)1);

        //  MainWindow->ReleaseMenuNew();  // Windows nejsou staveny na vic kontextovych menu

        if (Configuration.UseSalOpen)
        {
            // zkusime otevrit asociaci pres salopen.exe
            char execName[MAX_PATH + 200]; // + 200 je rezerva pro delsi jmena (blby Windows)
            strcpy(execName, path);
            if (SalPathAppend(execName, name, MAX_PATH + 200) && SalOpenExecute(hWindow, execName))
            {
                if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // uz je mozne nove volani
                    TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
                return; // hotovo, spustilo se to v procesu salopen.exe
            }

            // pokud selze salopen.exe, spoustime klasickym zpusobem (nebezpeci otevrenych handlu v adresari)
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
                if (cmd == -1) // nenasli jsme default item -> zkusime hledat jen mezi verby
                {
                    DestroyMenu(h);
                    h = CreatePopupMenu();
                    if (h != NULL)
                    {
                        ExecuteAssociationAux2(menu, h, CMF_VERBSONLY | CMF_DEFAULTONLY);

                        cmd = GetMenuDefaultItem(h, FALSE, GMDI_GOINTOPOPUPS);
                        if (cmd == -1)
                            cmd = 0; // zkusime "default verb" (index 0)
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

        if (ExecuteAssociationTlsIndex != TLS_OUT_OF_INDEXES) // uz je mozne nove volani
            TlsSetValue(ExecuteAssociationTlsIndex, (void*)0);
    }
    else
    {
        // TRACE_E("Attempt to call ExecuteAssociation() recursively! (skipping this call...)");
        // zeptame se, zda ma Salamander pokracovat nebo jestli ma vygenerovat bug report
        if (SalMessageBox(hWindow, LoadStr(IDS_SHELLEXTBREAK4), SALAMANDER_TEXT_VERSION,
                          MSGBOXEX_CONTINUEABORT | MB_ICONINFORMATION | MSGBOXEX_SETFOREGROUND) == IDABORT)
        { // breakneme se
            strcpy(BugReportReasonBreak, "Attempt to call ExecuteAssociation() recursively.");
            TaskList.FireEvent(TASKLIST_TODO_BREAK, GetCurrentProcessId());
            // zamrazime tento thread
            while (1)
                Sleep(1000);
        }
    }
}

// vraci TRUE, pokud je "bezpecne" poskytnout do shell extension jako parent okno specialni neviditelne okno,
// ktere pak muze shell extension napriklad sestrelit pres DestroyWindow (coz normalne zavre Explorera, ale shodilo Salama)
// existuji vyjimky, kdy je nutne predat jako parent hlavni okno Salamandera
BOOL CanUseShellExecuteWndAsParent(const char* cmdName)
{
    // pro Map Network Drive nejde pouzit shellExecuteWnd, jinak se to kousne (zdisabluji MainWindows->HWindow a okno Map Network Drive se neotevre)
    if (WindowsVistaAndLater && stricmp(cmdName, "connectNetworkDrive") == 0)
        return FALSE;

    // pod Windows 8 zlobilo Open With - pri volbe vlastniho programu se nezobrazil Open dialog
    // https://forum.altap.cz/viewtopic.php?f=16&t=6730 a https://forum.altap.cz/viewtopic.php?t=6782
    // problem je tam v tom, ze se kod vrati z invoke, ale pozdeji si MS sahnou na parent okno, ktere my uz mame zrusene
    // TODO: nabizelo by se reseni, kdy bychom ShellExecuteWnd nechavali zit (child okno, roztazene pres celou plochu Salama, uplne na jeho pozadi)
    // pouze bychom si vzdy pred jeho predanim overili, ze zije (ze ho nekdo nestrelil)
    // TODO2: tak jsem navrh cvicne zkusil a prave pod W8 a Open With to neslape, Open dialog neni modalni k nasemu hlavnimu oknu (pripadne Find okno)
    // zatim budeme v tomto pripade predavat hlavni okno Salamandera
    if (Windows8AndLater && stricmp(cmdName, "openas") == 0)
        return FALSE;

    // pro ostatni pripady (vetsina) lze pouzit ShellExecuteWnd
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

  BOOL ret = TRUE;    // POZOR: chybi podpora pro OneDriveBusinessStorages, pripadne dopsat !!!
  if (Windows8_1AndLater && OneDrivePath[0] != 0 && strlen(name) < MAX_PATH)
  {
    char path[MAX_PATH];
    char *cutName;
    strcpy_s(path, name);
    if (CutDirectory(path, &cutName) && SalPathIsPrefix(OneDrivePath, path)) // resime to jen pod OneDrive slozkou
    {
      BOOL makeOffline = FALSE;
      WIN32_FIND_DATA findData;
      HANDLE hFind = FindFirstFile(name, &findData);
      if (hFind != INVALID_HANDLE_VALUE)
      {
        makeOffline = IsFilePlaceholder(&findData);
        FindClose(hFind);
      }

      if (makeOffline)  // prevedu soubor na offline
      {
        // tak takhle debilne to nejde, je to asynchronni a ten prevod na offline (stazeni ze site)
        // klidne probehne az za minutu nebo vubec neprobehne, nemame nad tim zadnou kontrolu,
        // pockame az to pujde nejak pres Win32 API
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