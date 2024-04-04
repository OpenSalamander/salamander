// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "editwnd.h"
#include "stswnd.h"
#include "filesbox.h"
#include "zip.h"
#include "shellib.h"
#include "toolbar.h"

//*****************************************************************************
//
// Action Ball Lightning: moves from FILESBOX.CPP
//
//

void CFilesWindow::EndQuickSearch()
{
    CALL_STACK_MESSAGE_NONE
    QuickSearchMode = FALSE;
    QuickSearch[0] = 0;
    QuickSearchMask[0] = 0;
    SearchIndex = INT_MAX;
    HideCaret(ListBox->HWindow);
    DestroyCaret();
}

// Searching for the next/previous item. If skip = TRUE, it will skip the current item
BOOL CFilesWindow::QSFindNext(int currentIndex, BOOL next, BOOL skip, BOOL wholeString, char newChar, int& index)
{
    CALL_STACK_MESSAGE6("CFilesWindow::QSFindNext(%d, %d, %d, %d, %u)", currentIndex, next, skip, wholeString, newChar);
    int len = (int)strlen(QuickSearchMask);
    if (newChar != 0)
    {
        if (len >= MAX_PATH)
            return FALSE;
        QuickSearchMask[len] = newChar;
        len++;
        QuickSearchMask[len] = 0;
    }

    int delta = skip ? 1 : 0;

    int offset = 0;
    char mask[MAX_PATH];
    PrepareQSMask(mask, QuickSearchMask);

    int count = Dirs->Count + Files->Count;
    int dirCount = Dirs->Count;
    if (next)
    {
        int i;
        for (i = currentIndex + delta; i < count; i++)
        {
            char* name = i < dirCount ? Dirs->At(i).Name : Files->At(i - dirCount).Name;
            BOOL hasExtension = i < dirCount ? strchr(name, '.') != NULL : // Ext in the directory does not have to be set
                                    *Files->At(i - dirCount).Ext != 0;
            if (i == 0 && i < dirCount && strcmp(name, "..") == 0)
            {
                if (len == 0)
                {
                    QuickSearch[0] = 0;
                    index = i;
                    return TRUE;
                }
            }
            else
            {
                if (AgreeQSMask(name, hasExtension, mask, wholeString, offset))
                {
                    lstrcpyn(QuickSearch, name, offset + 1);
                    index = i;
                    return TRUE;
                }
            }
        }
    }
    else
    {
        int i;
        for (i = currentIndex - delta; i >= 0; i--)
        {
            char* name = i < dirCount ? Dirs->At(i).Name : Files->At(i - dirCount).Name;
            BOOL hasExtension = i < dirCount ? strchr(name, '.') != NULL : // Ext in the directory does not have to be set
                                    *Files->At(i - dirCount).Ext != 0;
            if (i == 0 && i < dirCount && strcmp(name, "..") == 0)
            {
                if (len == 0)
                {
                    QuickSearch[0] = 0;
                    index = i;
                    return TRUE;
                }
            }
            else
            {
                if (AgreeQSMask(name, hasExtension, mask, wholeString, offset))
                {
                    lstrcpyn(QuickSearch, name, offset + 1);
                    index = i;
                    return TRUE;
                }
            }
        }
    }

    if (newChar != 0)
    {
        len--;
        QuickSearchMask[len] = 0;
    }
    return FALSE;
}

// Searching for the next/previous selected item. If skip = TRUE, it will skip the current item
BOOL CFilesWindow::SelectFindNext(int currentIndex, BOOL next, BOOL skip, int& index)
{
    CALL_STACK_MESSAGE4("CFilesWindow::SelectFindNext(%d, %d, %d)", currentIndex, next, skip);

    int delta = skip ? 1 : 0;

    int count = Dirs->Count + Files->Count;
    int dirCount = Dirs->Count;
    if (next)
    {
        int i;
        for (i = currentIndex + delta; i < count; i++)
        {
            char* name = (i < dirCount) ? Dirs->At(i).Name : Files->At(i - dirCount).Name;
            BOOL sel = GetSel(i);
            if (sel)
            {
                index = i;
                return TRUE;
            }
        }
    }
    else
    {
        int i;
        for (i = currentIndex - delta; i >= 0; i--)
        {
            char* name = (i < dirCount) ? Dirs->At(i).Name : Files->At(i - dirCount).Name;
            BOOL sel = GetSel(i);
            if (sel)
            {
                index = i;
                return TRUE;
            }
        }
    }
    return FALSE;
}

BOOL CFilesWindow::IsNethoodFS()
{
    CPluginData* nethoodPlugin;
    return Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
           Plugins.GetFirstNethoodPluginFSName(NULL, &nethoodPlugin) &&
           GetPluginFS()->GetDLLName() == nethoodPlugin->DLLName;
}

void CFilesWindow::CtrlPageDnOrEnter(WPARAM key)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CtrlPageDnOrEnter(0x%IX)", key);
    if (key == VK_RETURN && (GetKeyState(VK_MENU) & 0x8000)) // properties
    {
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_PROPERTIES, 0);
    }
    else // execute
    {
        if (Dirs->Count + Files->Count == 0)
            RefreshDirectory();
        else
        {
            int index = GetCaretIndex();
            if (key != VK_NEXT || index >= 0 && index < Dirs->Count || // Enter either file or directory
                index >= Dirs->Count && index < Files->Count + Dirs->Count &&
                    (Files->At(index - Dirs->Count).Archive || IsNethoodFS())) // soubor archivu nebo soubor ve FS Nethoodu (ma servery vedene jako soubory a Entire Network jako adresar, aby byl panel dobre serazeny -> Ctrl+PageDown musi otvirat i servery a jejich shary, protoze to jsou "adresare")
            {
                Execute(index);
            }
        }
    }
}

void CFilesWindow::CtrlPageUpOrBackspace()
{
    CALL_STACK_MESSAGE1("CFilesWindow::CtrlPageUpOrBackspace()");
    if (Dirs->Count + Files->Count == 0)
        RefreshDirectory();
    else if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
    {
        Execute(0); // ".."
    }
}

void CFilesWindow::FocusShortcutTarget(CFilesWindow* panel)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FocusShortcutTarget()");
    int index = GetCaretIndex();
    if (index >= Dirs->Count + Files->Count)
        return;
    BOOL isDir = index < Dirs->Count;
    CFileData* file = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

    char shortName[MAX_PATH];
    strcpy(shortName, file->Name);

    char fullName[MAX_PATH];
    strcpy(fullName, GetPath());
    if (!SalPathAppend(fullName, file->Name, MAX_PATH))
    {
        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
        return;
    }

    //!!! Warning, Resolve can display a dialog and messages will start to be distributed
    // the panel may be refreshed, so it is not possible to access it from this moment
    // to file pointer

    BOOL invalid = FALSE;
    BOOL wrongPath = FALSE;
    BOOL mountPoint = FALSE;
    int repPointType;
    char junctionOrSymlinkTgt[MAX_PATH];
    strcpy(junctionOrSymlinkTgt, fullName);
    if (GetReparsePointDestination(junctionOrSymlinkTgt, junctionOrSymlinkTgt, MAX_PATH, &repPointType, FALSE))
    {
        // MOUNT POINT: I can't get this path into the panel (for example: \??\Volume{98c0ba30-71ff-11e1-9099-005056c00008}\)
        if (repPointType == 1 /* MOUNT POINT*/)
            mountPoint = TRUE;
        else
            panel->ChangeDir(junctionOrSymlinkTgt, -1, NULL, 3, NULL, TRUE, TRUE /* show full path in errors*/);
    }
    else
    {
        if (isDir)
            wrongPath = TRUE;
        else
        {
            IShellLink* link;
            if (CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER, IID_IShellLink, (LPVOID*)&link) == S_OK)
            {
                IPersistFile* fileInt;
                if (link->QueryInterface(IID_IPersistFile, (LPVOID*)&fileInt) == S_OK)
                {
                    HRESULT res = 2;
                    OLECHAR oleName[MAX_PATH];
                    MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, fullName, -1, oleName, MAX_PATH);
                    oleName[MAX_PATH - 1] = 0;
                    if (fileInt->Load(oleName, STGM_READ) == S_OK)
                    {
                        res = link->Resolve(HWindow, SLR_ANY_MATCH | SLR_UPDATE);
                        // if object is found, return NOERROR (0)
                        // if the dialog was displayed and the user clicked Cancel, it returns S_FALSE (1)
                        // if loading the link fails, it returns different errors (0x80004005)
                        if (res == NOERROR)
                        {
                            WIN32_FIND_DATA dummyData;
                            if (link->GetPath(fullName, MAX_PATH, &dummyData, SLGP_UNCPRIORITY) == NOERROR &&
                                fullName[0] != 0 && CheckPath(FALSE, fullName) == ERROR_SUCCESS)
                            {
                                panel->ChangeDir(fullName);
                            }
                            else
                            {                           // We can try to open links directly to servers in the Network plugin (Nethood)
                                BOOL linkIsNet = FALSE; // TRUE -> shortcut to the network -> ChangePathToPluginFS
                                char netFSName[MAX_PATH];
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
                                        if (*backslash == 0 && // we only take paths "\\\\", "\\\\server", "\\\\server\\"
                                            strlen(netFSName) + 1 + strlen(fullName) < MAX_PATH)
                                        {
                                            linkIsNet = TRUE; // o.k. let's try change-path-to-FS
                                            memmove(fullName + strlen(netFSName) + 1, fullName, strlen(fullName) + 1);
                                            memcpy(fullName, netFSName, strlen(netFSName));
                                            fullName[strlen(netFSName)] = ':';
                                        }
                                    }
                                }

                                if (linkIsNet)
                                    panel->ChangeDir(fullName);
                                else
                                    wrongPath = TRUE; // path cannot be acquired or is not available
                            }
                        }
                    }
                    fileInt->Release();
                    if (res != NOERROR && res != S_FALSE)
                        invalid = TRUE; // file is not a valid shortcut
                }
                link->Release();
            }
        }
    }
    if (mountPoint || invalid)
    {
        char errBuf[MAX_PATH + 300];
        sprintf(errBuf, LoadStr(mountPoint ? IDS_DIRLINK_MOUNT_POINT : IDS_SHORTCUT_INVALID),
                mountPoint ? junctionOrSymlinkTgt : shortName);
        SalMessageBox(HWindow, errBuf, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    if (wrongPath)
    {
        SalMessageBox(HWindow, LoadStr(isDir ? IDS_DIRLINK_WRONG_PATH : IDS_SHORTCUT_WRONG_PATH),
                      LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
}

void CFilesWindow::SetCaretIndex(int index, int scroll, BOOL forcePaint)
{
    CALL_STACK_MESSAGE4("CFilesWindow::SetCaretIndex(%d, %d, %d)", index, scroll, forcePaint);
    if (FocusedIndex != index)
    {
        BOOL normalProcessing = TRUE;
        if (!scroll && GetViewMode() == vmDetailed)
        {
            int newTopIndex = ListBox->PredictTopIndex(index);
            // Optimization comes into play only if TopIndex changes
            // and the cursor visually remains in the same place
            if (newTopIndex != ListBox->TopIndex &&
                index - newTopIndex == FocusedIndex - ListBox->TopIndex)
            {
                // we will use non-blinking scrolling: it scrolls only with the part without the cursor
                // then redraw the remaining items
                FocusedIndex = index;
                ListBox->EnsureItemVisible2(newTopIndex, FocusedIndex);
                normalProcessing = FALSE;
            }
        }
        if (normalProcessing)
        {
            int oldFocusIndex = FocusedIndex;
            FocusedIndex = index;
            ListBox->PaintItem(oldFocusIndex, DRAWFLAG_SELFOC_CHANGE);
            ListBox->EnsureItemVisible(FocusedIndex, TRUE, scroll, TRUE);
        }
    }
    else if (forcePaint)
        ListBox->EnsureItemVisible(FocusedIndex, TRUE, scroll, TRUE);
    ItemFocused(index);

    if (TrackingSingleClick)
    {
        POINT p;
        GetCursorPos(&p);
        ScreenToClient(GetListBoxHWND(), &p);
        SendMessage(GetListBoxHWND(), WM_MOUSEMOVE, 0, MAKELPARAM(p.x, p.y));
        KillTimer(GetListBoxHWND(), IDT_SINGLECLICKSELECT); // We don't want the focus to move here
    }
}

int CFilesWindow::GetCaretIndex()
{
    CALL_STACK_MESSAGE_NONE
    int index = FocusedIndex;
    int count = Files->Count + Dirs->Count;
    if (index >= count)
        index = count - 1;
    if (index < 0)
        index = 0;
    return index;
}

void CFilesWindow::SelectFocusedIndex()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SelectFocusedIndex()");
    if (Dirs->Count + Files->Count > 0)
    {
        int index = GetCaretIndex();
        SetSel(!GetSel(index), index);
        if (index + 1 >= 0 && index + 1 < Dirs->Count + Files->Count) // shift
            SetCaretIndex(index + 1, FALSE);
        else
            RedrawIndex(index);
        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
    }
}

void CFilesWindow::SetDropTarget(int index)
{
    CALL_STACK_MESSAGE_NONE
    if (DropTargetIndex != index)
    {
        int i = DropTargetIndex;
        DropTargetIndex = index;
        if (i != -1)
            RedrawIndex(i);
        if (DropTargetIndex != -1)
            RedrawIndex(DropTargetIndex);
    }
}

void CFilesWindow::SetSingleClickIndex(int index)
{
    CALL_STACK_MESSAGE_NONE
    DWORD pos = GetMessagePos();
    if (SingleClickIndex != index)
    {
        int i = SingleClickIndex;
        SingleClickIndex = index;
        if (i != -1)
            RedrawIndex(i);
        if (SingleClickIndex != -1)
            RedrawIndex(SingleClickIndex);
        if (index != -1)
        {
            if (GET_X_LPARAM(pos) != OldSingleClickMousePos.x || GET_Y_LPARAM(pos) != OldSingleClickMousePos.y)
                SetTimer(GetListBoxHWND(), IDT_SINGLECLICKSELECT, MouseHoverTime, NULL);
        }
        else
        {
            KillTimer(GetListBoxHWND(), IDT_SINGLECLICKSELECT);
        }
    }
    OldSingleClickMousePos.x = GET_X_LPARAM(pos);
    OldSingleClickMousePos.y = GET_Y_LPARAM(pos);
    if (index != -1)
        SetHandCursor();
    else
        SetCursor(LoadCursor(NULL, IDC_ARROW));
}

void CFilesWindow::DrawDragBox(POINT p)
{
    CALL_STACK_MESSAGE1("CFilesWindow::DrawDragBox()");
    FilesMap.DrawDragBox(p);
    DragBoxVisible = 1 - DragBoxVisible;
}

void CFilesWindow::EndDragBox()
{
    CALL_STACK_MESSAGE1("CFilesWindow::EndDragBox()");
    if (DragBox)
    {
        ReleaseCapture();
        DragBox = 0;
        DrawDragBox(OldBoxPoint);
        ScrollObject.EndScroll();
        FilesMap.DestroyMap();
    }
}

void CFilesWindow::SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult)
{
    CALL_STACK_MESSAGE_NONE
    __try
    {
        IContextMenu3* contextMenu3 = NULL;
        *plResult = 0;
        if (uMsg == WM_MENUCHAR)
        {
            if (SUCCEEDED(ContextSubmenuNew->GetMenu2()->QueryInterface(IID_IContextMenu3, (void**)&contextMenu3)))
            {
                contextMenu3->HandleMenuMsg2(uMsg, wParam, lParam, plResult);
                contextMenu3->Release();
                return;
            }
        }
        ContextSubmenuNew->GetMenu2()->HandleMenuMsg(uMsg, wParam, lParam);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 3))
    {
        MenuNewExceptionHasOccured++;
        if (ContextSubmenuNew->MenuIsAssigned())
            ContextSubmenuNew->Release();
    }
}

void CFilesWindow::RegisterDragDrop()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RegisterDragDrop()");
    CImpDropTarget* dropTarget = new CImpDropTarget(MainWindow->HWindow, DoCopyMove, this,
                                                    GetCurrentDir, this,
                                                    DropEnd, this,
                                                    MouseConfirmDrop, &Configuration.CnfrmDragDrop,
                                                    EnterLeaveDrop, this, UseOwnRutine,
                                                    DoDragDropOper, this, DoGetFSToFSDropEffect, this);
    if (dropTarget != NULL)
    {
        if (HANDLES(RegisterDragDrop(ListBox->HWindow, dropTarget)) != S_OK)
        {
            TRACE_E("RegisterDragDrop error.");
        }
        dropTarget->Release(); // RegisterDragDrop called AddRef()
    }
}

void CFilesWindow::RevokeDragDrop()
{
    CALL_STACK_MESSAGE_NONE
    HANDLES(RevokeDragDrop(ListBox->HWindow));
}

HWND CFilesWindow::GetListBoxHWND()
{
    CALL_STACK_MESSAGE_NONE
    return ListBox->HWindow;
}

HWND CFilesWindow::GetHeaderLineHWND()
{
    CALL_STACK_MESSAGE_NONE
    return ListBox->HeaderLine.HWindow;
}

int CFilesWindow::GetIndex(int x, int y, BOOL nearest, RECT* labelRect)
{
    CALL_STACK_MESSAGE_NONE
    return ListBox->GetIndex(x, y, nearest, labelRect);
}

void CFilesWindow::SetSel(BOOL select, int index, BOOL repaintDirtyItems)
{
    CALL_STACK_MESSAGE_NONE
    int totalCount = Dirs->Count + Files->Count;
    if (index < -1 || index >= totalCount)
    {
        TRACE_E("Access to index out of array bounds, index=" << index);
        return;
    }

    BYTE sel = select ? 1 : 0;
    BOOL repaint = FALSE;
    if (index == -1)
    {
        // select/unselect all
        int firstIndex = 0;
        if (Dirs->Count > 0)
        {
            const char* name = Dirs->At(0).Name;
            if (*name == '.' && *(name + 1) == '.' && *(name + 2) == 0)
                firstIndex = 1; // skip ".."
        }
        int i;
        for (i = firstIndex; i < totalCount; i++)
        {
            CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (item->Selected != sel)
            {
                item->Dirty = 1;
                item->Selected = sel;
            }
        }
        SelectedCount = select ? totalCount - firstIndex : 0;
    }
    else
    {
        CFileData* item;
        if (index < Dirs->Count)
        {
            if (index == 0)
            {
                const char* name = Dirs->At(0).Name;
                if (*name == '.' && *(name + 1) == '.' && *(name + 2) == 0)
                    return; // skip ".."
            }
            item = &Dirs->At(index);
        }
        else
            item = &Files->At(index - Dirs->Count);

        if (item->Selected != sel)
        {
            item->Dirty = 1;
            item->Selected = sel;
            SelectedCount += select ? 1 : -1;
            repaint = TRUE;
        }
    }
    if (repaintDirtyItems)
    {
        if (index == -1)
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_IGNORE_CLIPBOX); // DRAWFLAG_IGNORE_CLIPBOX: if there is a dialog above us, dirty items must be changed
        else if (repaint)
            RedrawIndex(index);
    }
}

void CFilesWindow::SetSel(BOOL select, CFileData* data)
{
    CALL_STACK_MESSAGE_NONE
    BYTE sel = select ? 1 : 0;
    if (data->Selected != sel)
    {
        data->Dirty = 1;
        data->Selected = sel;
        SelectedCount += select ? 1 : -1;
    }
}

BOOL CFilesWindow::GetSel(int index)
{
    CALL_STACK_MESSAGE_NONE
    if (index < 0 || index >= Dirs->Count + Files->Count)
    {
        TRACE_E("Access to index out of array bounds, index=" << index);
        return FALSE;
    }
    CFileData* item = (index < Dirs->Count) ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
    return (item->Selected == 1);
}

int CFilesWindow::GetSelItems(int itemsCountMax, int* items, BOOL /*focusedItemFirst*/)
{
    CALL_STACK_MESSAGE_NONE
    int index = 0;
    if (index >= itemsCountMax)
        return index;

    int firstItem = 0;
    /* pro znacnou vlnu nevole jsme tohle priblizeni se Exploreru zase vratili zpet (https://forum.altap.cz/viewtopic.php?t=3044)
  if (focusedItemFirst)
  {
    // fokusla polozka prijde do pole jako prvni: alespon u kontextovych menu pri vice oznacenych sharech v Networku (napr. na \\petr-pc) je to potreba pro spravnou funkci Map Network Drive
    int caretIndex = GetCaretIndex();
    if ((caretIndex < Dirs->Count ? Dirs->At(caretIndex) : Files->At(caretIndex - Dirs->Count)).Selected == 1) // zacneme od ni pouze v pripade, ze patri do seznamu vybranych polozek
      firstItem = caretIndex;
  }
*/
    int totalCount = Dirs->Count + Files->Count;
    // fill the first part of the list: from firstItem downwards
    int i;
    for (i = firstItem; i < totalCount; i++)
    {
        CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        if (item->Selected == 1)
        {
            items[index++] = i;
            if (index >= itemsCountMax)
                return index;
        }
    }
    // fill the second part of the list: from the first selected item towards firstItem
    for (i = 0; i < firstItem; i++) // Executed only if focusedItemFirst==TRUE
    {
        CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        if (item->Selected == 1)
        {
            items[index++] = i;
            if (index >= itemsCountMax)
                return index;
        }
    }
    return index;
}

BOOL CFilesWindow::SelectionContainsDirectory()
{
    CALL_STACK_MESSAGE_NONE
    if (GetSelCount() == 0)
    {
        int index = GetCaretIndex();
        if (index < Dirs->Count &&
            (index > 0 || strcmp(Dirs->At(0).Name, "..") != 0))
        {
            return TRUE;
        }
    }
    else
    {
        int start = 0;
        if (Dirs->Count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
            start = 1;
        int i;
        for (i = start; i < Dirs->Count; i++)
        {
            CFileData* item = &Dirs->At(i);
            if (item->Selected == 1)
                return TRUE;
        }
    }
    return FALSE;
}

BOOL CFilesWindow::SelectionContainsFile()
{
    CALL_STACK_MESSAGE_NONE
    if (GetSelCount() == 0)
    {
        int index = GetCaretIndex();
        if (index >= Dirs->Count)
        {
            return TRUE;
        }
    }
    else
    {
        int i;
        for (i = 0; i < Files->Count; i++)
        {
            CFileData* item = &Files->At(i);
            if (item->Selected == 1)
                return TRUE;
        }
    }
    return FALSE;
}

void CFilesWindow::SelectFocusedItemAndGetName(char* name, int nameSize)
{
    name[0] = 0;
    if (GetSelCount() == 0)
    {
        int deselectIndex = GetCaretIndex();
        if (deselectIndex >= 0 && deselectIndex < Dirs->Count + Files->Count)
        {
            BOOL isDir = deselectIndex < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(deselectIndex) : &Files->At(deselectIndex - Dirs->Count);

            int len = (int)strlen(f->Name);
            if (len >= nameSize)
            {
                TRACE_E("len > nameMax");
                len = nameSize - 1;
            }

            memcpy(name, f->Name, len);
            name[len] = 0;

            SetSel(TRUE, deselectIndex);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
        }
    }
}

void CFilesWindow::UnselectItemWithName(const char* name)
{
    if (name[0] != 0)
    {
        int count = Dirs->Count + Files->Count;
        int l = (int)strlen(name);
        int foundIndex = -1;
        int i;
        for (i = 0; i < count; i++)
        {
            CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (f->NameLen == (unsigned)l &&
                RegSetStrICmpEx(f->Name, f->NameLen, name, l, NULL) == 0)
            {
                if (RegSetStrCmpEx(f->Name, f->NameLen, name, l, NULL) == 0)
                {
                    SetSel(FALSE, i);
                    break;
                }
                else
                {
                    if (foundIndex == -1)
                        foundIndex = i;
                }
            }
        }
        if (i == count && foundIndex != -1)
            SetSel(FALSE, foundIndex);

        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
    }
}

BOOL CFilesWindow::SetSelRange(BOOL select, int firstIndex, int lastIndex)
{
    CALL_STACK_MESSAGE_NONE
    BOOL selChanged = FALSE;
    int totalCount = Dirs->Count + Files->Count;
    if (firstIndex < 0 || firstIndex >= totalCount ||
        lastIndex < 0 || lastIndex >= totalCount)
    {
        TRACE_E("Access to index out of array bounds.");
        return selChanged;
    }
    if (lastIndex < firstIndex)
    {
        // if the boundaries are swapped, we will ensure their correct order
        int tmp = lastIndex;
        lastIndex = firstIndex;
        firstIndex = tmp;
    }
    if (firstIndex == 0 && Dirs->Count > 0)
    {
        const char* name = Dirs->At(0).Name;
        if (*name == '.' && *(name + 1) == '.' && *(name + 2) == 0)
            firstIndex = 1; // skip ".."
    }
    int i;
    for (i = firstIndex; i <= lastIndex; i++)
    {
        CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        BYTE sel = select ? 1 : 0;
        if (item->Selected != sel)
        {
            item->Dirty = 1;
            item->Selected = sel;
            SelectedCount += select ? 1 : -1;
            selChanged = TRUE;
        }
    }
    return selChanged;
}

int CFilesWindow::GetSelCount()
{
    CALL_STACK_MESSAGE_NONE
#ifdef _DEBUG
    // Let's perform a consistency test
    int totalCount = Dirs->Count + Files->Count;
    int selectedCount = 0;
    int i;
    for (i = 0; i < totalCount; i++)
    {
        CFileData* item = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
        if (item->Selected == 1)
            selectedCount++;
    }
    if (selectedCount != SelectedCount)
        TRACE_E("selectedCount != SelectedCount");
#endif // _DEBUG
    return SelectedCount;
}

BOOL CFilesWindow::OnSysChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (MainWindow->DragMode || DragBox)
    {
        *lResult = 0;
        return TRUE;
    }
    if (SkipSysCharacter)
    {
        SkipSysCharacter = FALSE;
        *lResult = 0;
        return TRUE;
    }
    if (SkipCharacter)
    {
        *lResult = 0;
        return TRUE;
    }

    return FALSE;
}

BOOL CFilesWindow::OnChar(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (SkipCharacter || MainWindow->DragMode || DragBox || !IsWindowEnabled(MainWindow->HWindow))
    {
        *lResult = 0;
        return TRUE;
    }
    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;

    // if we are in QuickSearchEnterAlt mode, we need to set the focus to
    // buffer the command line and the letter there
    if (!controlPressed && !altPressed &&
        !QuickSearchMode &&
        wParam > 32 && wParam < 256 &&
        Configuration.QuickSearchEnterAlt)
    {
        if (MainWindow->EditWindow->IsEnabled())
        {
            SendMessage(MainWindow->HWindow, WM_COMMAND, CM_EDITLINE, 0);
            // send the character there
            HWND hEditLine = MainWindow->GetEditLineHWND(TRUE);
            if (hEditLine != NULL)
                PostMessage(hEditLine, WM_CHAR, wParam, lParam);
        }
        return FALSE;
    }

    if (wParam > 31 && wParam < 256 &&  // only normal characters
        Dirs->Count + Files->Count > 0) // at least 1 item
    {
        int index = FocusedIndex;
        // On a German keyboard, the slash is on Shift+7, so it conflicts with HotPaths
        // proto pro * v QS pouzijeme mimo lomitka take backslah a obetujeme tuto
        // low-frequency function
        //
        // 8/2006: German users continue to complain that access to QS is for them
        // slozity, protoze pri zapnute nemcine museji stisknout AltGr+\
    // maji vsak volnou klavesu '<', ktera mimochodem pri prepnuti na anglickou
        // On the keyboard, the backslash means backslash, so even the '<' character will be caught next to '\\' and '/'
        // Character '<' is also not allowed in the file name
        //
        //if (QuickSearchMode && (char)wParam == '\\')
        //{
        //  // when pressing the '\\' character during QS, jump to the first item that matches QuickSearchMask
        //  if (!QSFindNext(GetCaretIndex(), TRUE, FALSE, TRUE, (char)0, index))
        //    QSFindNext(GetCaretIndex(), FALSE, TRUE, TRUE, (char)0, index);
        //}
        //else
        //{
        if (!QSFindNext(GetCaretIndex(), TRUE, FALSE, FALSE, (char)wParam, index))
            QSFindNext(GetCaretIndex(), FALSE, TRUE, FALSE, (char)wParam, index);
        //}

        if (!QuickSearchMode) // initialization of search
        {
            if (GetViewMode() == vmDetailed)
                ListBox->OnHScroll(SB_THUMBPOSITION, 0);
            QuickSearchMode = TRUE;
            CreateCaret(ListBox->HWindow, NULL, CARET_WIDTH, CaretHeight);
            if (index != FocusedIndex)
                SetCaretIndex(index, FALSE);
            else
            {
                // Make sure the item is visible because it may be outside the visible area
                ListBox->EnsureItemVisible(index, FALSE, FALSE, FALSE);
            }
            SetQuickSearchCaretPos();
            ShowCaret(ListBox->HWindow);
        }
        else
        {
            BOOL showCaret = FALSE;
            if (index != FocusedIndex)
            {
                HideCaret(ListBox->HWindow);
                showCaret = TRUE;
                SetCaretIndex(index, FALSE);
            }
            SetQuickSearchCaretPos();
            if (showCaret)
                ShowCaret(ListBox->HWindow);
        }
    }
    return FALSE;
}

void CFilesWindow::GotoSelectedItem(BOOL next)
{
    int newFocusedIndex = FocusedIndex;

    if (next)
    {
        int index;
        BOOL found = SelectFindNext(GetCaretIndex(), TRUE, TRUE, index);
        if (found)
            newFocusedIndex = index;
    }
    else
    {
        int index;
        BOOL found = SelectFindNext(GetCaretIndex(), FALSE, TRUE, index);
        if (found)
            newFocusedIndex = index;
    }

    if (newFocusedIndex != FocusedIndex)
    {
        SetCaretIndex(newFocusedIndex, FALSE);
        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
    }
}

BOOL CFilesWindow::OnSysKeyDown(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
    *lResult = 0;
    if (MainWindow->HDisabledKeyboard != NULL)
    {
        if (wParam == VK_ESCAPE)
            PostMessage(MainWindow->HDisabledKeyboard, WM_CANCELMODE, 0, 0);
        return TRUE;
    }

    BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
    BOOL firstPress = (lParam & 0x40000000) == 0;
    // j.r.: Dusek found a problem when UP did not reflect to paru to DOWN
    // therefore I will trigger a test on the first press of the SHIFT key
    if (wParam == VK_SHIFT && firstPress && Dirs->Count + Files->Count > 0)
    {
        //    ShiftSelect = TRUE;
        SelectItems = !GetSel(FocusedIndex);
        //    TRACE_I("VK_SHIFT down");
    }

    BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
    BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;

    if (((Configuration.QuickSearchEnterAlt &&
          altPressed && !controlPressed)) &&
        wParam > 31 && wParam < 256 &&
        Dirs->Count + Files->Count > 0)
    {
        BYTE ks[256];
        GetKeyboardState(ks);
        ks[VK_CONTROL] = 0;
        WORD ch;
        int ret = ToAscii((UINT)wParam, 0, ks, &ch, 0);
        if (ret == 1)
        {
            SkipSysCharacter = TRUE;
            SendMessage(ListBox->HWindow, WM_CHAR, LOBYTE(ch), 0);
            return TRUE;
        }
    }

    SkipCharacter = FALSE;
    if (wParam == VK_ESCAPE)
    {
        if (MainWindow->DragMode)
        {
            PostMessage(MainWindow->HWindow, WM_CANCELMODE, 0, 0);
            return TRUE;
        }

        if (DragBox)
        {
            PostMessage(ListBox->HWindow, WM_CANCELMODE, 0, 0);
            return TRUE;
        }
    }

    if (MainWindow->DragMode || DragBox)
        return TRUE;
    if (!IsWindowEnabled(MainWindow->HWindow))
        return TRUE;

    if (wParam == VK_SPACE && !shiftPressed && !controlPressed && !altPressed)
    {                         // Spaces will be marked by a space character (names ' ' usually do not start with it)
        if (!QuickSearchMode) // middle of the name can be
        {
            SkipCharacter = TRUE;
            int index = GetCaretIndex();
            BOOL sel = GetSel(index);
            SetSel(!sel, index);
            if (!sel && Configuration.SpaceSelCalcSpace && GetCaretIndex() < Dirs->Count)
            {
                if (Is(ptDisk))
                {
                    UserWorkedOnThisPath = TRUE;
                    FilesAction(atCountSize, MainWindow->GetNonActivePanel(), 1);
                }
                else
                {
                    if (Is(ptZIPArchive))
                    {
                        UserWorkedOnThisPath = TRUE;
                        CalculateOccupiedZIPSpace(1);
                    }
                    else
                    {
                        if (Is(ptPluginFS))
                        {

                            // to append
                        }
                    }
                }
            }
            if (index + 1 >= 0 && index + 1 < Dirs->Count + Files->Count) // shift
                SetCaretIndex(index + 1, FALSE);
            else
                RedrawIndex(index);
            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            return TRUE;
        }
    }

    if (wParam == 0xBF && !shiftPressed && controlPressed && !altPressed) // Ctrl+'/'
    {
        SkipCharacter = TRUE;
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_DOSSHELL, 0);
        return TRUE;
    }

    if (wParam == 0xBB && !shiftPressed && controlPressed && !altPressed) // Ctrl+'+'
    {
        SkipCharacter = TRUE;
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_ACTIVESELECT, 0);
        return TRUE;
    }

    if (wParam == 0xBD && !shiftPressed && controlPressed && !altPressed) // Ctrl+'-'
    {
        SkipCharacter = TRUE;
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_ACTIVEUNSELECT, 0);
        return TRUE;
    }

    if (wParam == VK_INSERT)
    {
        if (!shiftPressed && controlPressed && !altPressed) // clipboard: copy
        {
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CLIPCOPY, 0);
            return TRUE;
        }

        if (shiftPressed && !controlPressed && !altPressed)
        { // clipboard: paste
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CLIPPASTE, 0);
            return TRUE;
        }

        if (!shiftPressed && !controlPressed && altPressed ||
            shiftPressed && !controlPressed && altPressed)
        { // clipboard: (full) name of focused item
            PostMessage(MainWindow->HWindow, WM_COMMAND, !shiftPressed ? CM_CLIPCOPYFULLNAME : CM_CLIPCOPYNAME, 0);
            return TRUE;
        }

        if (!shiftPressed && controlPressed && altPressed)
        { // clipboard: current full path
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CLIPCOPYFULLPATH, 0);
            return TRUE;
        }

        if (shiftPressed && controlPressed && !altPressed)
        { // clipboard: (full) UNC name of focused item
            PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CLIPCOPYUNCNAME, 0);
            return TRUE;
        }
    }

    if ((wParam == VK_F10 && shiftPressed || wParam == VK_APPS) &&
        Files->Count + Dirs->Count > 0)
    {
        PostMessage(MainWindow->HWindow, WM_COMMAND, CM_CONTEXTMENU, 0);
        return TRUE;
    }

    if (wParam == VK_NEXT && controlPressed && !altPressed && !shiftPressed ||
        wParam == VK_RETURN && (!controlPressed || altPressed)) // podporime AltGr+Enter (=Ctrl+Alt+Enter na CZ klavesce) pro Properties dlg. (Explorer to dela a lidi na foru to chteli)
    {
        SkipCharacter = TRUE;
        CtrlPageDnOrEnter(wParam);
        return TRUE;
    }

    if (wParam == VK_PRIOR && controlPressed && !altPressed && !shiftPressed)
    {
        CtrlPageUpOrBackspace();
        return TRUE;
    }

    if (controlPressed && !altPressed &&
        (wParam == VK_RETURN || wParam == VK_LBRACKET || wParam == VK_RBRACKET ||
         wParam == VK_SPACE))
    {
        SkipCharacter = TRUE;
        SendMessage(MainWindow->HWindow, WM_COMMAND, CM_EDITLINE, 0);
        if (MainWindow->EditWindow->IsEnabled())
            PostMessage(GetFocus(), uMsg, wParam, lParam);
        return TRUE;
    }

    if (wParam == VK_TAB)
    {
        MainWindow->ChangePanel();
        return TRUE;
    }

    if (altPressed && !controlPressed && !shiftPressed)
    {
        // change panel mode
        if (wParam >= '0' && wParam <= '9')
        {
            int index = (int)wParam - '0';
            if (index == 0)
                index = 9;
            else
                index--;
            if (IsViewTemplateValid(index))
                SelectViewTemplate(index, TRUE, FALSE);
            SkipSysCharacter = TRUE; // prevent beeping
            return TRUE;
        }
    }

    if (shiftPressed && !controlPressed && !altPressed ||
        !shiftPressed && controlPressed && !altPressed)
    {
        // change disk || hot key
        if (wParam >= 'A' && wParam <= 'Z')
        {
            SkipCharacter = TRUE;
            if (MainWindow->HandleCtrlLetter((char)wParam))
                return TRUE;
        }
    }

    //When Ctrl+Shift+= is pressed, wParam == 0xBB is sent, which is according to the SDK:
    //#define VK_OEM_PLUS       0xBB   // '+' any country
    if (wParam == VK_OEM_PLUS)
    {
        // assign to any empty hot path
        if (shiftPressed && controlPressed && !altPressed)
        {
            SkipCharacter = TRUE;
            if (MainWindow->GetActivePanel()->SetUnescapedHotPathToEmptyPos())
            {
                if (!Configuration.HotPathAutoConfig)
                    MainWindow->GetActivePanel()->DirectoryLine->FlashText();
            }
            return TRUE;
        }
    }

    if (wParam >= '0' && wParam <= '9')
    {
        BOOL exit = FALSE;
        // define hot path
        if (shiftPressed && controlPressed && !altPressed)
        {
            SkipCharacter = TRUE;
            MainWindow->GetActivePanel()->SetUnescapedHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
            if (!Configuration.HotPathAutoConfig)
                MainWindow->GetActivePanel()->DirectoryLine->FlashText();
            exit = TRUE;
        }
        // go to hot path
        if ((controlPressed && !shiftPressed) ||
            (Configuration.ShiftForHotPaths && !controlPressed && shiftPressed))
        {
            SkipCharacter = TRUE;
            if (altPressed)
                MainWindow->GetNonActivePanel()->GotoHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
            else
                MainWindow->GetActivePanel()->GotoHotPath((char)wParam == '0' ? 9 : (char)wParam - '1');
            exit = TRUE;
        }
        if (exit)
            return TRUE;
    }

    if (controlPressed)
    {
        BOOL networkNeighborhood = FALSE;
        BOOL asOtherPanel = FALSE;
        BOOL myDocuments = FALSE;

        switch (wParam)
        {
        case 0xC0: // ` || ~
        {
            if (shiftPressed)
                asOtherPanel = TRUE; // ~
            else
                networkNeighborhood = TRUE; // `
            break;
        }

        case 190: // .
        {
            asOtherPanel = TRUE;
            break;
        }

        case 188: // ,
        {
            networkNeighborhood = TRUE;
            break;
        }

        case 186: // ;
        {
            myDocuments = TRUE;
            break;
        }
        }

        if (asOtherPanel)
        {
            TopIndexMem.Clear(); // long jump
            ChangePathToOtherPanelPath();
            return TRUE;
        }
        if (networkNeighborhood)
        {
            char path[MAX_PATH];
            if (Plugins.GetFirstNethoodPluginFSName(path))
            {
                TopIndexMem.Clear(); // long jump
                ChangePathToPluginFS(path, "");
            }
            else
            {
                if (SystemPolicies.GetNoNetHood())
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
                    return TRUE;
                }

                if (GetTargetDirectory(HWindow, HWindow, LoadStr(IDS_CHANGEDRIVE),
                                       LoadStr(IDS_CHANGEDRIVETEXT), path, TRUE))
                {
                    TopIndexMem.Clear(); // long jump
                    UpdateWindow(MainWindow->HWindow);
                    ChangePathToDisk(HWindow, path);
                }
            }
            return TRUE;
        }
        if (myDocuments)
        {
            char path[MAX_PATH];
            if (GetMyDocumentsOrDesktopPath(path, MAX_PATH))
                ChangePathToDisk(HWindow, path);
            return TRUE;
        }
    }

    if (controlPressed && !shiftPressed && !altPressed && wParam == VK_BACKSLASH ||
        (!controlPressed && shiftPressed && !altPressed ||
         controlPressed && !shiftPressed && !altPressed) &&
            wParam == VK_BACK)
    { // Shift+backspace, Ctrl+backslash
        SkipCharacter = TRUE;
        MainWindow->GetActivePanel()->GotoRoot();
        return TRUE;
    }

    if (QuickSearchMode)
    {
        BOOL processed = TRUE;
        int newIndex = GetCaretIndex();
        switch (wParam)
        {
        case VK_SPACE:
        {
            return TRUE;
        }

        case VK_ESCAPE: // End of quick search mode via ESC
        {
            EndQuickSearch();
            return TRUE;
        }

        case VK_INSERT: // selecting / deselecting an item in a listbox + moving to the next one
        {
            EndQuickSearch();
            goto INSERT_KEY;
        }

        case VK_BACK: // backspace - delete a character in the quicksearch mask
        {
            if (QuickSearchMask[0] != 0)
            {
                int len = (int)strlen(QuickSearchMask) - 1; // remove character
                QuickSearchMask[len] = 0;

                int index;
                QSFindNext(GetCaretIndex(), FALSE, FALSE, FALSE, (char)0, index);
                SetQuickSearchCaretPos();
            }
            return TRUE;
        }

        case VK_LEFT: // left arrow - convert mask to string and remove a character
        {
            if (QuickSearch[0] != 0)
            {
                int len = (int)strlen(QuickSearch) - 1; // remove character
                QuickSearch[len] = 0;
                int len2 = (int)strlen(QuickSearchMask);
                if (len2 > 1 && !IsQSWildChar(QuickSearchMask[len2 - 1]) && !IsQSWildChar(QuickSearchMask[len2 - 2]))
                {
                    QuickSearchMask[len2 - 1] = 0;
                }
                else
                {
                    // In this case, we will discard "wild" characters and switch to regular searching, because
                    strcpy(QuickSearchMask, QuickSearch);
                }
                SetQuickSearchCaretPos();
            }
            return TRUE;
        }

        case VK_RIGHT:
        {
            if (FocusedIndex >= 0 && FocusedIndex < Dirs->Count + Files->Count)
            {
                char* name = (FocusedIndex < Dirs->Count) ? Dirs->At(FocusedIndex).Name : Files->At(FocusedIndex - Dirs->Count).Name;
                int len = (int)strlen(QuickSearch); // add character
                if ((FocusedIndex > Dirs->Count || FocusedIndex != 0 ||
                     strcmp(name, "..") != 0) &&
                    name[len] != 0)
                {
                    // if there is any more
                    QuickSearch[len] = name[len];
                    QuickSearch[len + 1] = 0;
                    int len2 = (int)strlen(QuickSearchMask);
                    QuickSearchMask[len2] = name[len];
                    QuickSearchMask[len2 + 1] = 0;
                    SetQuickSearchCaretPos();
                }
            }
            return TRUE;
        }

        case VK_UP: // search for the next match upwards
        {
            if (controlPressed && !shiftPressed && !altPressed)
                break; // We cannot scroll the panel content in quick-search mode
            int index;
            int lastIndex = GetCaretIndex();
            if (!controlPressed && shiftPressed && !altPressed)
            {
                SetSel(SelectItems, lastIndex, TRUE);
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            BOOL found;
            do
            {
                found = QSFindNext(lastIndex, FALSE, TRUE, FALSE, (char)0, index);
                if (found)
                {
                    lastIndex = index;
                    if (GetSel(lastIndex))
                        break;
                }
            } while (altPressed && found);
            if (found)
                newIndex = lastIndex;
            break;
        }

        case VK_DOWN: // search for the next match downwards
        {
            if (controlPressed && !shiftPressed && !altPressed)
                break; // We cannot scroll the panel content in quick-search mode
            int index;
            int lastIndex = GetCaretIndex();
            if (!controlPressed && shiftPressed && !altPressed)
            {
                SetSel(SelectItems, lastIndex, TRUE);
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            BOOL found;
            do
            {
                found = QSFindNext(lastIndex, TRUE, TRUE, FALSE, (char)0, index);
                if (found)
                {
                    lastIndex = index;
                    if (GetSel(lastIndex))
                        break;
                }
            } while (altPressed && found);
            if (found)
                newIndex = lastIndex;
            break;
        }

        case VK_PRIOR: // looking for the next page
        {
            int limit = GetCaretIndex() - ListBox->GetColumnsCount() * ListBox->GetEntireItemsInColumn() + 2;
            int index;
            newIndex = GetCaretIndex();
            BOOL found;
            do
            {
                if (!controlPressed && shiftPressed && !altPressed)
                {
                    SetSel(SelectItems, newIndex, TRUE);
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                }
                found = QSFindNext(newIndex, FALSE, TRUE, FALSE, (char)0, index);
                if (found)
                    newIndex = index;
                if (newIndex < limit)
                    break;
            } while (found);
            break;
        }

        case VK_NEXT: // searching on the previous page
        {
            int limit = GetCaretIndex() + ListBox->GetColumnsCount() * ListBox->GetEntireItemsInColumn() - 2;
            int index;
            newIndex = GetCaretIndex();
            BOOL found;
            do
            {
                if (!controlPressed && shiftPressed && !altPressed)
                {
                    SetSel(SelectItems, newIndex, TRUE);
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                }
                found = QSFindNext(newIndex, TRUE, TRUE, FALSE, (char)0, index);
                if (found)
                    newIndex = index;
                if (newIndex > limit)
                    break;
            } while (found);
            break;
        }

        case VK_HOME: // looking for the first
        {
            int index;
            if (!controlPressed && shiftPressed && !altPressed)
            {
                newIndex = GetCaretIndex();
                BOOL found;
                do
                {
                    SetSel(SelectItems, newIndex, TRUE);
                    found = QSFindNext(newIndex, FALSE, TRUE, FALSE, (char)0, index);
                    if (found)
                        newIndex = index;
                } while (found);
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            else
            {
                newIndex = 0;
                index = newIndex;
                BOOL found;
                BOOL skip = FALSE;
                do
                {
                    found = QSFindNext(index, TRUE, skip, FALSE, (char)0, index);
                    skip = TRUE;
                    if (found && GetSel(index))
                        break;
                } while (altPressed && found);
                if (found)
                    newIndex = index;
                else
                    newIndex = FocusedIndex; // Alt+Home without a selected item corresponding to quick searches
            }
            break;
        }

        case VK_END: // looking for the last
        {
            int index;
            if (!controlPressed && shiftPressed && !altPressed)
            {
                newIndex = GetCaretIndex();
                BOOL found;
                do
                {
                    SetSel(SelectItems, newIndex, TRUE);
                    found = QSFindNext(newIndex, TRUE, TRUE, FALSE, (char)0, index);
                    if (found)
                        newIndex = index;
                } while (found);
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            else
            {
                newIndex = Dirs->Count + Files->Count - 1;
                index = newIndex;
                BOOL found;
                BOOL skip = FALSE;
                do
                {
                    found = QSFindNext(index, FALSE, skip, FALSE, (char)0, index);
                    skip = TRUE;
                    if (found && GetSel(index))
                        break;
                } while (altPressed && found);
                if (found)
                    newIndex = index;
                else
                    newIndex = FocusedIndex; // Alt+End without the selected item corresponding to quick-searches
            }
            break;
        }

        default:
            processed = FALSE;
            break;
        }
        if (processed)
        {
            if (newIndex != FocusedIndex)
            {
                HideCaret(ListBox->HWindow);
                SetCaretIndex(newIndex, FALSE);
                SetQuickSearchCaretPos();
                ShowCaret(ListBox->HWindow);
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            }
            return TRUE;
        }
    }
    else // normal mode of the listbox
    {
        switch (wParam)
        {
        case VK_SPACE:
        {
            return TRUE; // space not wanted
        }

        case VK_INSERT: // selecting / deselecting an item in a listbox + moving to the next one
        {
        INSERT_KEY: // for jumping out of Quick Search mode

            if (!shiftPressed && !controlPressed && !altPressed)
                SelectFocusedIndex();
            IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
            return TRUE;
        }

        case VK_BACK:
        case VK_PRIOR:
        {
            if (wParam == VK_BACK && !controlPressed && !altPressed && !shiftPressed) // backspace
            {
                CtrlPageUpOrBackspace();
                return TRUE;
            }
        }
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NEXT:
        case VK_HOME:
        case VK_END:
        {
            if (controlPressed && shiftPressed && !altPressed)
            {
                // Focus on the focused directory/file in the second panel
                BOOL leftPanel = this == MainWindow->LeftPanel;
                if (wParam == VK_LEFT)
                {
                    SendMessage(MainWindow->HWindow, WM_COMMAND, leftPanel ? CM_OPEN_IN_OTHER_PANEL : CM_OPEN_IN_OTHER_PANEL_ACT, 0);
                    break;
                }

                if (wParam == VK_RIGHT)
                {
                    SendMessage(MainWindow->HWindow, WM_COMMAND, leftPanel ? CM_OPEN_IN_OTHER_PANEL_ACT : CM_OPEN_IN_OTHER_PANEL, 0);
                    break;
                }
            }

            if (controlPressed && !shiftPressed && !altPressed)
            {
                // scrolling the content while keeping the cursor position,
                // alternatively, quickly move left and right in Detailed mode
                if (wParam == VK_LEFT)
                {
                    if (GetViewMode() == vmBrief)
                        SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEUP, 0);
                    else if (GetViewMode() == vmDetailed)
                    {
                        int i;
                        for (i = 0; i < 5; i++) // let's do some macro programming
                            SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEUP, 0);
                    }
                    break;
                }

                if (wParam == VK_RIGHT)
                {
                    if (GetViewMode() == vmBrief)
                        SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
                    else if (GetViewMode() == vmDetailed)
                    {
                        int i;
                        for (i = 0; i < 5; i++) // let's do some macro programming
                            SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
                    }
                    break;
                }

                if (wParam == VK_UP)
                {
                    if (GetViewMode() == vmDetailed ||
                        GetViewMode() == vmIcons ||
                        GetViewMode() == vmThumbnails ||
                        GetViewMode() == vmTiles)
                        SendMessage(ListBox->HWindow, WM_VSCROLL, SB_LINEUP, 0);
                    break;
                }

                if (wParam == VK_DOWN)
                {
                    if (GetViewMode() == vmDetailed ||
                        GetViewMode() == vmIcons ||
                        GetViewMode() == vmThumbnails ||
                        GetViewMode() == vmTiles)
                        SendMessage(ListBox->HWindow, WM_VSCROLL, SB_LINEDOWN, 0);
                    break;
                }
            }

            int newFocusedIndex = FocusedIndex;

            BOOL forceSelect = FALSE; // to be able to select boundary elements via Shift+Up/Down

            if (wParam == VK_UP)
            {
                if (newFocusedIndex > 0)
                {
                    if (altPressed) // Petr: in all modes, we go to the previous selected item (for icons/thumbnails/tiles it may look strange, but I don't find it useful to search for the selected item only in the current column)
                    {
                        int index;
                        BOOL found = SelectFindNext(GetCaretIndex(), FALSE, TRUE, index);
                        if (found)
                            newFocusedIndex = index;
                    }
                    else
                    {
                        // move the cursor up by one line
                        switch (GetViewMode())
                        {
                        case vmDetailed:
                        case vmBrief:
                        {
                            newFocusedIndex--;
                            break;
                        }

                        case vmIcons:
                        case vmThumbnails:
                        case vmTiles:
                        {
                            newFocusedIndex -= ListBox->ColumnsCount;
                            if (newFocusedIndex < 0)
                            {
                                newFocusedIndex = 0;
                                if (!controlPressed && shiftPressed && !altPressed)
                                    forceSelect = TRUE;
                            }
                            break;
                        }
                        }
                    }
                }
                else if (!controlPressed && shiftPressed && !altPressed)
                    forceSelect = TRUE;
            }
            if (wParam == VK_DOWN)
            {
                if (newFocusedIndex < Dirs->Count + Files->Count - 1)
                {
                    if (altPressed) // Petr: in all modes we go to the next selected item (in icons/thumbnails/tiles it may look strange, but I don't find it useful to search for the selected item only in the current column)
                    {
                        int index;
                        BOOL found = SelectFindNext(GetCaretIndex(), TRUE, TRUE, index);
                        if (found)
                            newFocusedIndex = index;
                    }
                    else
                    {
                        switch (GetViewMode())
                        {
                        // move the cursor down by one line
                        case vmDetailed:
                        case vmBrief:
                        {
                            newFocusedIndex++;
                            break;
                        }

                        case vmIcons:
                        case vmThumbnails:
                        case vmTiles:
                        {
                            newFocusedIndex += ListBox->ColumnsCount;
                            if (newFocusedIndex >= Dirs->Count + Files->Count)
                            {
                                newFocusedIndex = Dirs->Count + Files->Count - 1;
                                if (!controlPressed && shiftPressed && !altPressed)
                                    forceSelect = TRUE;
                            }
                            break;
                        }
                        }
                    }
                }
                else if (!controlPressed && shiftPressed && !altPressed)
                    forceSelect = TRUE;
            }

            if (wParam == VK_PRIOR || wParam == VK_NEXT)
            {
                int c;
                switch (GetViewMode())
                {
                case vmDetailed:
                {
                    c = ListBox->GetEntireItemsInColumn() - 1;
                    break;
                }

                case vmBrief:
                {
                    c = ListBox->EntireColumnsCount * ListBox->EntireItemsInColumn;
                    break;
                }

                case vmIcons:
                case vmThumbnails:
                case vmTiles:
                {
                    c = ListBox->EntireItemsInColumn * ListBox->ColumnsCount;
                    break;
                }
                }
                if (c <= 0)
                    c = 1;

                if (wParam == VK_NEXT)
                {
                    newFocusedIndex += c;
                    if (newFocusedIndex >= Files->Count + Dirs->Count)
                    {
                        newFocusedIndex = Files->Count + Dirs->Count - 1;
                        if (!controlPressed && shiftPressed && !altPressed)
                            forceSelect = TRUE; // Shift+PgDn should also select the last item, see FAR and TC
                    }
                }
                else
                {
                    newFocusedIndex -= c;
                    if (newFocusedIndex < 0)
                    {
                        newFocusedIndex = 0;
                        if (!controlPressed && shiftPressed && !altPressed)
                            forceSelect = TRUE; // Shift+PgUp should also select the first item, see FAR and TC
                    }
                }
            }

            if (wParam == VK_LEFT)
            {
                switch (GetViewMode())
                {
                case vmDetailed:
                {
                    SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEUP, 0);
                    break;
                }

                // move the cursor one column to the left
                case vmBrief:
                {
                    // Ensure jumping to the zeroth item if we are in the zeroth column
                    newFocusedIndex -= ListBox->GetEntireItemsInColumn();
                    if (newFocusedIndex < 0)
                    {
                        newFocusedIndex = 0;
                        if (!controlPressed && shiftPressed && !altPressed)
                            forceSelect = TRUE;
                    }
                    break;
                }

                case vmIcons:
                case vmThumbnails:
                case vmTiles:
                {
                    if (newFocusedIndex > 0)
                        newFocusedIndex--;
                    else if (!controlPressed && shiftPressed && !altPressed)
                        forceSelect = TRUE;
                    break;
                }
                }
            }

            if (wParam == VK_RIGHT)
            {
                switch (GetViewMode())
                {
                case vmDetailed:
                {
                    SendMessage(ListBox->HWindow, WM_HSCROLL, SB_LINEDOWN, 0);
                    break;
                }

                // move the cursor one column to the left
                case vmBrief:
                {
                    // Jump to the last item if we are in the last column
                    newFocusedIndex += ListBox->GetEntireItemsInColumn();
                    if (newFocusedIndex >= Files->Count + Dirs->Count)
                    {
                        newFocusedIndex = Files->Count + Dirs->Count - 1;
                        if (!controlPressed && shiftPressed && !altPressed)
                            forceSelect = TRUE;
                    }
                    break;
                }

                case vmIcons:
                case vmThumbnails:
                case vmTiles:
                {
                    if (newFocusedIndex < Dirs->Count + Files->Count - 1)
                        newFocusedIndex++;
                    else if (!controlPressed && shiftPressed && !altPressed)
                        forceSelect = TRUE;
                    break;
                }
                }
            }
            if (wParam == VK_HOME)
            {
                if (altPressed)
                {
                    // We jump to the first highlighted item in the panel
                    int index;
                    BOOL found = SelectFindNext(0, TRUE, FALSE, index);
                    if (found)
                        newFocusedIndex = index;
                }
                else if (controlPressed && Files->Count > 0)
                {
                    newFocusedIndex = Dirs->Count;
                }
                else
                    newFocusedIndex = 0;
            }
            if (wParam == VK_END)
            {
                int tmpLast = max(Files->Count + Dirs->Count - 1, 0);
                if (altPressed)
                {
                    // We jump to the last selected item in the panel
                    int index;
                    BOOL found = SelectFindNext(tmpLast, FALSE, FALSE, index);
                    if (found)
                        newFocusedIndex = index;
                }
                else
                    newFocusedIndex = tmpLast;
            }

            if (newFocusedIndex != FocusedIndex || forceSelect)
            {
                if (shiftPressed) // shift+movement of selection
                {
                    BOOL select = SelectItems; // retrieve the state that was on the item when Shift was pressed
                    int targetIndex = newFocusedIndex;
                    if (wParam != VK_HOME && wParam != VK_END && !forceSelect)
                    {
                        if (newFocusedIndex > FocusedIndex)
                            targetIndex--;
                        else
                            targetIndex++;
                    }
                    // if any item has changed, we will refresh
                    if (SetSelRange(select, targetIndex, FocusedIndex))
                        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                }
                int oldIndex = FocusedIndex;
                SetCaretIndex(newFocusedIndex, FALSE);
                IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                if (shiftPressed && newFocusedIndex != oldIndex + 1 && newFocusedIndex != oldIndex - 1)
                    RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
            }
            *lResult = 0;
            return TRUE;
        }
        }
    }

    // give a chance to plugins; the same can be called from the command line
    if (Plugins.HandleKeyDown(wParam, lParam, this, MainWindow->HWindow))
    {
        *lResult = 0;
        return TRUE;
    }

    return FALSE;
}

BOOL CFilesWindow::OnSysKeyUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    /*    if (wParam == VK_SHIFT)
  {
    ShiftSelect = FALSE;
    TRACE_I("VK_SHIFT up");
  }*/
    SkipCharacter = FALSE;
    return FALSE;
}

// next WM_SETFOCUS in the panel will only be invalidated - not executed immediately
BOOL CacheNextSetFocus = FALSE;

void CFilesWindow::OnSetFocus(BOOL focusVisible)
{
    CALL_STACK_MESSAGE_NONE
    BOOL hideCommandLine = FALSE;
    if (Parent->EditMode)
    {
        Parent->EditMode = FALSE;
        if (Parent->GetActivePanel() != this)
            Parent->GetActivePanel()->RedrawIndex(Parent->GetActivePanel()->FocusedIndex);
        if (!Parent->EditPermanentVisible)
            hideCommandLine = TRUE;
    }
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    FocusVisible = focusVisible;
    // change: instead of immediate redraw, we will only invalidate the area;
    // the focus will not be so aggressive when switching to Salama and will wait,
    // when the other items will be drawn
    if (Dirs->Count + Files->Count == 0) // Ensure rendering text on an empty panel
        InvalidateRect(ListBox->HWindow, &ListBox->FilesRect, FALSE);
    else
    {
        if (CacheNextSetFocus)
        {
            CacheNextSetFocus = FALSE;
            RECT r;
            if (ListBox->GetItemRect(FocusedIndex, &r))
                InvalidateRect(ListBox->HWindow, &r, FALSE);
        }
        else
            RedrawIndex(FocusedIndex);
    }
    if (hideCommandLine)
    {
        // command line was displayed only temporarily - we will turn it off
        Parent->HideCommandLine(TRUE, FALSE); // we will save the content and disable focusing of the panel
    }
}

void CFilesWindow::OnKillFocus(HWND hwndGetFocus)
{
    CALL_STACK_MESSAGE_NONE
    if (Parent->EditWindowKnowHWND(hwndGetFocus))
        Parent->EditMode = TRUE;
    if (QuickSearchMode)
        EndQuickSearch();
    FocusVisible = FALSE;
    // change: instead of immediate redraw, we will only invalidate the area;
    // the focus will not be so aggressive when switching to Salama and will wait,
    // when the other items will be drawn
    if (Dirs->Count + Files->Count == 0) // Ensure rendering text on an empty panel
        InvalidateRect(ListBox->HWindow, &ListBox->FilesRect, FALSE);
    else
    {
        RedrawIndex(FocusedIndex);
        UpdateWindow(ListBox->HWindow);
    }
}

void CFilesWindow::RepaintIconOnly(int index)
{
    CALL_STACK_MESSAGE_NONE
    if (index == -1)
        ListBox->PaintAllItems(NULL, DRAWFLAG_ICON_ONLY);
    else
        ListBox->PaintItem(index, DRAWFLAG_ICON_ONLY);
}

void ReleaseListingBody(CPanelType oldPanelType, CSalamanderDirectory*& oldArchiveDir,
                        CSalamanderDirectory*& oldPluginFSDir,
                        CPluginDataInterfaceEncapsulation& oldPluginData,
                        CFilesArray*& oldFiles, CFilesArray*& oldDirs, BOOL dealloc)
{
    CALL_STACK_MESSAGE2("ReleaseListingBody(, , , , , , %d)", dealloc);
    CSalamanderDirectory* dir = NULL;
    if (oldPanelType == ptZIPArchive)
        dir = oldArchiveDir;
    else
    {
        if (oldPanelType == ptPluginFS)
            dir = oldPluginFSDir;
    }
    if (oldPluginData.NotEmpty())
    {
        // Release plugin data for individual files and directories
        BOOL releaseFiles = oldPluginData.CallReleaseForFiles();
        BOOL releaseDirs = oldPluginData.CallReleaseForDirs();
        if (releaseFiles || releaseDirs)
        {
            if (dir != NULL)
                dir->ReleasePluginData(oldPluginData, releaseFiles, releaseDirs); // archive or file system
            else
            {
                if (oldPanelType == ptDisk)
                {
                    if (releaseFiles)
                        oldPluginData.ReleaseFilesOrDirs(oldFiles, FALSE);
                    if (releaseDirs)
                        oldPluginData.ReleaseFilesOrDirs(oldDirs, TRUE);
                }
            }
        }

        // Release the interface oldPluginData
        CPluginInterfaceEncapsulation plugin(oldPluginData.GetPluginInterface(), oldPluginData.GetBuiltForVersion());
        plugin.ReleasePluginDataInterface(oldPluginData.GetInterface());
        oldPluginData.Init(NULL, NULL, NULL, NULL, 0);
    }
    if (dealloc)
    {
        if (dir != NULL)
            delete dir; // let's release the "standard" (Salamander-style) data listing
        // Let's release shallow copies (archive, file system) or "standard" (Salamander-style) data listings (disk)
        delete oldFiles;
        delete oldDirs;
        oldFiles = NULL; // Just in case someone still wants to use it, so it doesn't crash
        oldDirs = NULL;  // Just in case someone still wants to use it, so it doesn't crash
    }
    else
    {
        if (dir != NULL)
            dir->Clear(NULL); // let's release the "standard" (Salamander-style) data listing
        // Let's release shallow copies (archive, file system) or "standard" (Salamander-style) data listings (disk)
        oldFiles->DestroyMembers();
        oldDirs->DestroyMembers();
    }
}

BOOL CFilesWindow::IsCaseSensitive()
{
    if (Is(ptZIPArchive))
        return (GetArchiveDir()->GetFlags() & SALDIRFLAG_CASESENSITIVE) != 0;
    else if (Is(ptPluginFS))
        return (GetPluginFSDir()->GetFlags() & SALDIRFLAG_CASESENSITIVE) != 0;
    return FALSE; // ptDisk
}

BOOL AreTheSameDirs(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData1,
                    const CFileData* f1, CPluginDataInterfaceEncapsulation* pluginData2,
                    const CFileData* f2)
{
    // It is not necessary to check the validity through 'validFileData' for the compared data in the condition.
    // because the data is "zeroed" if it is invalid
    if (strcmp(f1->Name, f2->Name) == 0 &&
        f1->Attr == f2->Attr &&
        (f1->DosName == f2->DosName || f1->DosName != NULL && f2->DosName != NULL &&
                                           strcmp(f1->DosName, f2->DosName) == 0) &&
        f1->Hidden == f2->Hidden &&
        f1->IsLink == f2->IsLink &&
        f1->IsOffline == f2->IsOffline)
    {
        SYSTEMTIME st1;
        BOOL validDate1, validTime1;
        GetFileDateAndTimeFromPanel(validFileData, pluginData1, f1, TRUE, &st1, &validDate1, &validTime1);
        SYSTEMTIME st2;
        BOOL validDate2, validTime2;
        GetFileDateAndTimeFromPanel(validFileData, pluginData2, f2, TRUE, &st2, &validDate2, &validTime2);
        if (validDate1 == validDate2 && validTime1 == validTime2 && // must be valid for the same parts of the structures
            (!validDate1 ||
             st1.wYear == st2.wYear &&
                 st1.wMonth == st2.wMonth &&
                 st1.wDay == st2.wDay) &&
            (!validTime1 ||
             st1.wHour == st2.wHour &&
                 st1.wMinute == st2.wMinute &&
                 st1.wSecond == st2.wSecond &&
                 st1.wMilliseconds == st2.wMilliseconds))
        {
            return TRUE;
        }
    }
    return FALSE;
}

BOOL AreTheSameFiles(DWORD validFileData, CPluginDataInterfaceEncapsulation* pluginData1,
                     const CFileData* f1, CPluginDataInterfaceEncapsulation* pluginData2,
                     const CFileData* f2)
{
    // It is not necessary to check the validity through 'validFileData' for the compared data in the condition.
    // because the data is "zeroed" if it is invalid
    if (strcmp(f1->Name, f2->Name) == 0 &&
        f1->Attr == f2->Attr &&
        (f1->DosName == f2->DosName || f1->DosName != NULL && f2->DosName != NULL &&
                                           strcmp(f1->DosName, f2->DosName) == 0) &&
        f1->Hidden == f2->Hidden &&
        f1->IsLink == f2->IsLink &&
        f1->IsOffline == f2->IsOffline)
    {
        SYSTEMTIME st1;
        BOOL validDate1, validTime1;
        GetFileDateAndTimeFromPanel(validFileData, pluginData1, f1, FALSE, &st1, &validDate1, &validTime1);
        SYSTEMTIME st2;
        BOOL validDate2, validTime2;
        GetFileDateAndTimeFromPanel(validFileData, pluginData2, f2, FALSE, &st2, &validDate2, &validTime2);
        if (validDate1 == validDate2 && validTime1 == validTime2 && // must be valid for the same parts of the structures
            (!validDate1 ||
             st1.wYear == st2.wYear &&
                 st1.wMonth == st2.wMonth &&
                 st1.wDay == st2.wDay) &&
            (!validTime1 ||
             st1.wHour == st2.wHour &&
                 st1.wMinute == st2.wMinute &&
                 st1.wSecond == st2.wSecond &&
                 st1.wMilliseconds == st2.wMilliseconds))
        {
            BOOL validSize1;
            CQuadWord size1;
            GetFileSizeFromPanel(validFileData, pluginData1, f1, FALSE, &size1, &validSize1);
            BOOL validSize2;
            CQuadWord size2;
            GetFileSizeFromPanel(validFileData, pluginData2, f2, FALSE, &size2, &validSize2);
            if (validSize1 == validSize2 && (!validSize1 || size1 == size2))
            {
                return TRUE;
            }
        }
    }
    return FALSE;
}

void CFilesWindow::RefreshDirectory(BOOL probablyUselessRefresh, BOOL forceReloadThumbnails, BOOL isInactiveRefresh)
{
    CALL_STACK_MESSAGE1("CFilesWindow::RefreshDirectory()");
    //  if (QuickSearchMode) EndQuickSearch();   // we will try to make the quick search mode survive refresh

#ifdef _DEBUG
    char t_path[2 * MAX_PATH];
    GetGeneralPath(t_path, 2 * MAX_PATH);
    TRACE_I("RefreshDirectory: " << (MainWindow->LeftPanel == this ? "left" : "right") << ": " << t_path);
#endif // _DEBUG

    // clock initialization
    BOOL setWait = (GetCursor() != LoadCursor(NULL, IDC_WAIT)); // Is it waiting already?
    HCURSOR oldCur;
    if (setWait)
        oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
    // due to recursive calls occurring on non-automatically refreshed discs
    // (directly posts the message WM_USER_REFRESH)
    BeginStopRefresh();

    BOOL focusFirstNewItem = FocusFirstNewItem; // the first ReadDirectory resets it, so backup
    BOOL cutToClipChanged = CutToClipChanged;   // the first ReadDirectory resets it, so backup

    // Save the top-index and xoffset to prevent unnecessary "jumps" in the panel content
    int topIndex = ListBox->GetTopIndex();
    int xOffset = ListBox->GetXOffset();

    // we will remember if the old listing is FS with custom icons (pitFromPlugin)
    BOOL pluginFSIconsFromPlugin = Is(ptPluginFS) && GetPluginIconsType() == pitFromPlugin;

    // Save the data of the focused item - we will find it later (and focus it)
    BOOL ensureFocusIndexVisible = FALSE;
    BOOL wholeItemVisible = FALSE; // We only need partial visibility of the item
    int focusIndex = GetCaretIndex();
    BOOL focusIsDir = focusIndex < Dirs->Count;
    CFileData focusData; // Shallow copy of data from the old focus (valid until the old listing is canceled)
    if (focusIndex >= 0 && focusIndex < Dirs->Count + Files->Count)
    {
        focusData = (focusIndex < Dirs->Count) ? Dirs->At(focusIndex) : Files->At(focusIndex - Dirs->Count);
        ensureFocusIndexVisible = ListBox->IsItemVisible(focusIndex, &wholeItemVisible);
    }
    else
        focusData.Name = NULL; // focus will not be ...

    // As for the FS, it is necessary to prepare objects for the new listing separately (they will be attached later)
    OnlyDetachFSListing = Is(ptPluginFS);

    // We back up the icon cache for later transfer of loaded icons to the new icon cache
    BOOL oldIconCacheValid = IconCacheValid;
    BOOL oldInactWinOptimizedReading = InactWinOptimizedReading;
    CIconCache* oldIconCache = NULL;
    if (UseSystemIcons || UseThumbnails)
        SleepIconCacheThread();    // stop the icon reader (IconCache/Files/Dirs can be changed)
    if (!TemporarilySimpleIcons && // if we are not supposed to use IconCache, we will not transfer anything from it to the new cache (e.g. when switching views to/from Thumbnails)
        (UseSystemIcons || UseThumbnails))
    {
        oldIconCache = IconCache;
        IconCache = new CIconCache(); // create a new one for a new listing
        if (IconCache == NULL)
        {
            TRACE_E(LOW_MEMORY);
            IconCache = oldIconCache;
            oldIconCache = NULL;
        }

        if (OnlyDetachFSListing)
        {
            if (oldIconCache != NULL)
            {
                NewFSIconCache = IconCache;
                IconCache = oldIconCache; // Until the listing is disconnected, we will use the old icon-cache
            }
            else
                NewFSIconCache = NULL; // out of memory - we will use only the old icon-cache
        }
    }

    // to synchronize the old and new version of the listing, we need items sorted by
    // names ascending - when sorting differently, a temporary change must occur
    //
    // SortedWithRegSet contains the state of the Configuration.SortUsesLocale variable from the last
    // calling SortDirectory(). SortedWithDetectNum contains the state of the variable
    // Configuration.SortDetectNumbers from the last call of SortDirectory().
    // If there has been a change in configuration in the meantime, the following must be enforced
    // sorting, otherwise subsequent synchronization of two listings would not work.
    BOOL changeSortType;
    BOOL sortDirectoryPostponed = FALSE;
    CSortType currentSortType;
    BOOL currentReverseSort;
    if (SortType != stName || ReverseSort || SortedWithRegSet != Configuration.SortUsesLocale ||
        SortedWithDetectNum != Configuration.SortDetectNumbers)
    {
        changeSortType = TRUE;
        currentReverseSort = ReverseSort;
        currentSortType = SortType;
        // At the same time, we define conditions for sorting a new listing, which will be triggered by CommonRefresh
        ReverseSort = FALSE;
        SortType = stName;
        if (!OnlyDetachFSListing)
            SortDirectory(); // sort the old listing
        else
        {
            // We will sort the FS listings later so that they can be drawn well in the panel for now (so that the indexes fit).
            sortDirectoryPostponed = TRUE;
        }
    }
    else
        changeSortType = FALSE;

    // backup old listing
    CPanelType oldPanelType = GetPanelType();                                  // the type of panel can also change (e.g. inaccessible path)
    CSalamanderDirectory* oldArchiveDir = GetArchiveDir();                     // data archive
    CSalamanderDirectory* oldPluginFSDir = GetPluginFSDir();                   // data FS
    CFilesArray* oldFiles = Files;                                             // shared data (except for shallow copies of disks)
    CFilesArray* oldDirs = Dirs;                                               // shared data (except for shallow copies of disks)
    DWORD oldValidFileData = ValidFileData;                                    // Backup of the mask of valid data (can be calculated, but that is unnecessarily complicated)
    CPluginDataInterfaceEncapsulation oldPluginData(PluginData.GetInterface(), // data archive/FS
                                                    PluginData.GetDLLName(),
                                                    PluginData.GetVersion(),
                                                    PluginData.GetPluginInterface(),
                                                    PluginData.GetBuiltForVersion());

    // disconnect the old listing from the panel (so it is not canceled during the refresh (change) of the path)
    BOOL lowMem = FALSE;
    BOOL refreshDir = FALSE;
    if (OnlyDetachFSListing)
    {
        NewFSFiles = new CFilesArray;
        NewFSDirs = new CFilesArray;
        if (NewFSFiles != NULL && NewFSDirs != NULL)
        {
            NewFSPluginFSDir = new CSalamanderDirectory(TRUE);
            lowMem = (NewFSPluginFSDir == NULL);
        }
        else
            lowMem = TRUE;
    }
    else
    {
        VisibleItemsArray.InvalidateArr();
        VisibleItemsArraySurround.InvalidateArr();
        Files = new CFilesArray;
        Dirs = new CFilesArray;
        if (Files != NULL && Dirs != NULL)
        {
            PluginData.Init(NULL, NULL, NULL, NULL, 0);
            switch (oldPanelType)
            {
            case ptZIPArchive:
            {
                SetArchiveDir(new CSalamanderDirectory(FALSE, oldArchiveDir->GetValidData(),
                                                       oldArchiveDir->GetFlags()));
                lowMem = GetArchiveDir() == NULL;
                break;
            }
            }
        }
        else
            lowMem = TRUE;
    }

    if (lowMem)
    {

    _LABEL_1:

        if (OnlyDetachFSListing)
        {
            if (NewFSFiles != NULL)
                delete NewFSFiles;
            if (NewFSDirs != NULL)
                delete NewFSDirs;
        }
        else
        {
            if (Files != NULL)
                delete Files;
            if (Dirs != NULL)
                delete Dirs;
        }
        SetArchiveDir(oldArchiveDir);
        SetPluginFSDir(oldPluginFSDir);
        VisibleItemsArray.InvalidateArr();
        VisibleItemsArraySurround.InvalidateArr();
        Files = oldFiles;
        Dirs = oldDirs;
        PluginData.Init(oldPluginData.GetInterface(), oldPluginData.GetDLLName(),
                        oldPluginData.GetVersion(), oldPluginData.GetPluginInterface(),
                        oldPluginData.GetBuiltForVersion());
        if (Is(ptZIPArchive))
            SetValidFileData(GetArchiveDir()->GetValidData());
        if (Is(ptPluginFS))
            SetValidFileData(GetPluginFSDir()->GetValidData());

        if (oldIconCache != NULL)
        {
            if (OnlyDetachFSListing)
                delete NewFSIconCache;
            else
                delete IconCache;
            IconCache = oldIconCache;
        }

        OnlyDetachFSListing = FALSE;
        NewFSFiles = NULL;
        NewFSDirs = NULL;
        NewFSPluginFSDir = NULL;
        NewFSIconCache = NULL;

    _LABEL_2:

        WaitBeforeReadingIcons = 0;

        DontClearNextFocusName = FALSE;

        // Return to the user-selected sorting
        if (changeSortType)
        {
            ReverseSort = currentReverseSort;
            SortType = currentSortType;
            // if (!sortDirectoryPostponed) SortDirectory();  // this doesn't work, new listings are also coming here (via _LABEL_2), not just oldFiles+oldDirs, moreover there is a risk of changing the sort parameters (let's rather resort even old listings)
            SortDirectory();
        }

        if (UseSystemIcons || UseThumbnails)
            WakeupIconCacheThread(); // wake-up up to SortDirectory()

        if (refreshDir) // refresh panel (after jumping to _LABEL_1)
        {
            RefreshListBox(xOffset, topIndex, focusIndex, FALSE, FALSE);
        }

        EndStopRefresh();
        if (setWait)
            SetCursor(oldCur);
        return; // Output without performing a refresh (error exit)
    }

    int oldSelectedCount = SelectedCount;
    BOOL clearWMUpdatePanel = FALSE;
    if (!OnlyDetachFSListing)
    {
        // Secure the listbox against errors caused by a request for a redraw (we have just modified the data)
        ListBox->SetItemsCount(0, 0, 0, TRUE); // TRUE - disable scrollbar setting
        SelectedCount = 0;

        // If WM_USER_UPDATEPANEL is delivered, the content of the panel will be redrawn and set
        // scrollbars. It can be delivered by the message loop when creating a message box.
        // Otherwise, the panel will appear unchanged and the message will be removed from the queue.
        PostMessage(HWindow, WM_USER_UPDATEPANEL, 0, 0);
        clearWMUpdatePanel = TRUE;
    }

    // From now on, closed paths will not be remembered (so that a path is not added just because of a refresh)
    BOOL oldCanAddToDirHistory = MainWindow->CanAddToDirHistory;
    MainWindow->CanAddToDirHistory = FALSE;

    // good mess: we need to postpone the start of reading icons in the icon-reader (this is triggered inside ChangePathToDisk, etc.),
    // so that we can stop reading icons a few lines below (to take over old versions of icons+thumbnails
    // and secondly, so that we can completely skip reading icons (icons loaded in the previous round will be used) in case that
    // is 'probablyUselessRefresh' TRUE and we don't notice any changes in the directory); another place where this piglet
    // It uses it when 'isInactiveRefresh' is TRUE, sets InactWinOptimizedReading to TRUE; cleaner solution
    // in this case, it would not make sense to start reading icons at all (not to perform WakeupIconCacheThread()), but that looks significant
    // non-trivial, that's why we're hacking it with Sleep in the icon-reader like this
    //
    // WARNING: before leaving this function, we must set WaitBeforeReadingIcons back to zero !!!
    WaitBeforeReadingIcons = 30;

    // Refresh routes (change to the same one with forceUpdate TRUE)
    BOOL noChange;
    BOOL result;
    char buf1[MAX_PATH];
    switch (GetPanelType())
    {
    case ptDisk:
    {
        result = ChangePathToDisk(HWindow, GetPath(), -1, NULL, &noChange, FALSE, FALSE, TRUE);
        break;
    }

    case ptZIPArchive:
    {
        result = ChangePathToArchive(GetZIPArchive(), GetZIPPath(), -1, NULL, TRUE, &noChange,
                                     FALSE, NULL, TRUE);

        // Path valid + unchanged, we will perform a common-refresh for Ctrl+H (requires read-dir)
        if (result && noChange)
        {
            // we will retrieve the original listing data
            CSalamanderDirectory* newArcDir = GetArchiveDir();
            SetArchiveDir(oldArchiveDir);
            PluginData.Init(oldPluginData.GetInterface(), oldPluginData.GetDLLName(),
                            oldPluginData.GetVersion(), oldPluginData.GetPluginInterface(),
                            oldPluginData.GetBuiltForVersion());
            SetValidFileData(GetArchiveDir()->GetValidData());
            // Let's leave the IconCache new so we don't lose the old one with loaded icons
            // Leave Files and Dirs new to allow synchronization to be performed (select a cut-to-clip)

            CommonRefresh(HWindow, -1, NULL, FALSE, TRUE, TRUE); // without refreshing the listbox
            noChange = FALSE;                                    // Change in Files+Dirs

            // the newly allocated listing needs to be released, unlike the original one (we will perform a swap)
            oldArchiveDir = newArcDir;
            oldPluginData.Init(NULL, NULL, NULL, NULL, 0);
        }
        break;
    }

    case ptPluginFS:
    {
        if (GetPluginFS()->NotEmpty() && GetPluginFS()->GetCurrentPath(buf1))
        {
            result = ChangePathToPluginFS(GetPluginFS()->GetPluginFSName(), buf1, -1, NULL, TRUE,
                                          1 /*refresh*/, &noChange, FALSE, NULL, TRUE);
        }
        else // should not occur if the plug-in is intelligently written
        {
            ChangeToFixedDrive(HWindow, &noChange, FALSE); // Let's end the quick-search quietly, it would end anyway
            result = FALSE;
        }
        break;
    }

    default:
    {
        TRACE_E("Unexpected situation (unknown type of plugin) in CFilesWindow::RefreshDirectory().");
        result = FALSE;
        noChange = TRUE;
        break;
    }
    }

    // transition back to the original path memory mode
    MainWindow->CanAddToDirHistory = oldCanAddToDirHistory;

    if (clearWMUpdatePanel)
    {
        // clean the message queue from buffered WM_USER_UPDATEPANEL
        MSG msg2;
        PeekMessage(&msg2, HWindow, WM_USER_UPDATEPANEL, WM_USER_UPDATEPANEL, PM_REMOVE);
    }

    if (!result || noChange) // refresh failed or is unnecessary
    {
        if (noChange) // return old listing (no replacement was created), perform cleanup (sort-dir, etc.)
        {
            if (!OnlyDetachFSListing)
            {
                // restore the listbox settings (return the original data)
                ListBox->SetItemsCount(oldFiles->Count + oldDirs->Count, 0, topIndex == -1 ? 0 : topIndex, FALSE);
                SelectedCount = oldSelectedCount;

                refreshDir = TRUE;
            }

            CutToClipChanged = cutToClipChanged;

            if (oldPanelType == ptZIPArchive)
                delete GetArchiveDir();
            if (oldPanelType == ptPluginFS)
            {
                if (OnlyDetachFSListing)
                    delete NewFSPluginFSDir;
                else
                    delete GetPluginFSDir();
            }

            goto _LABEL_1;
        }
        else // Let's release the old listing, perform cleanup (sort-dir, etc.)
        {
            MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
            TopIndexMem.Clear();          // long jump

            EnumFileNamesChangeSourceUID(HWindow, &EnumFileNamesSourceUID);

            ReleaseListingBody(oldPanelType, oldArchiveDir, oldPluginFSDir, oldPluginData,
                               oldFiles, oldDirs, TRUE);

            if (oldIconCache != NULL)
            {
                delete oldIconCache;
                oldIconCache = NULL; // to avoid unnecessary wake-up of the icon-reader
            }

            topIndex = -1;   // different listing -> ignore
            focusIndex = -1; // different listing -> ignore
            refreshDir = TRUE;

            goto _LABEL_2;
        }
    }
    if (OnlyDetachFSListing)
        TRACE_E("FATAL ERROR: New listing didn't use prealocated objects???");

    // perform deferred sorting of FS listing
    if (sortDirectoryPostponed)
    {
        if (ReverseSort || SortType != stName)
            TRACE_E("FATAL ERROR: Unexpected change of sort type!");
        SortDirectory(oldFiles, oldDirs);
        sortDirectoryPostponed = FALSE;
    }

    // We have a new version of the listing of the same path, let's enrich it with parts of the old listing
    // !!! WARNING about refresh in the archive that has not changed - oldFiles and oldDirs point to
    // ArchiveDir+PluginData (see above at ChangePathToArchive), oldArchiveDir+oldPluginData
    // are empty

    // main window is inactive, only icons/thumbnails/overlays from the visible part of the panel are being loaded, saving machine time
    if (isInactiveRefresh)
        InactWinOptimizedReading = TRUE;

    // if there were cut-to-clip flags on the old listing, they will probably be on the new one as well
    CutToClipChanged = cutToClipChanged;

    // Transfer old versions of icons to the icon cache of the new listing
    BOOL transferIconsAndThumbnailsAsNew = FALSE; // TRUE = transfer as "new/loaded" icons and thumbnails (not as "old", which will be loaded again)
    BOOL iconReaderIsSleeping = FALSE;
    BOOL iconCacheBackuped = oldIconCache != NULL;
    if (iconCacheBackuped)
    {
        // We can only use the old cache if the geometry of the icons has not changed
        if (oldIconCache->GetIconSize() == IconCache->GetIconSize())
        {
            if (UseSystemIcons || UseThumbnails) // if the new listing does not have icons, it does not make sense to transfer them from the old cache
            {
                SleepIconCacheThread(); // we will collect the IconCache for the icon reader
                iconReaderIsSleeping = TRUE;
                BOOL newPluginFSIconsFromPlugin = Is(ptPluginFS) && GetPluginIconsType() == pitFromPlugin;
                if (pluginFSIconsFromPlugin && newPluginFSIconsFromPlugin)
                {                                                                // old and new listing are FS with their own icons -> transferring old icons makes sense + we need to pass 'dataIface'
                    IconCache->GetIconsAndThumbsFrom(oldIconCache, &PluginData); // we will put old versions of icons and thumbnails into it
                }
                else
                {
                    if (!pluginFSIconsFromPlugin && !newPluginFSIconsFromPlugin)
                    { // old and new listing have nothing in common with FS with their own icons -> transferring old icons makes sense
                        if (probablyUselessRefresh &&
                            Dirs->Count == oldDirs->Count && Files->Count == oldFiles->Count &&
                            ValidFileData == oldValidFileData)
                        {
                            int i;
                            for (i = 0; i < Dirs->Count; i++)
                            {
                                if (!AreTheSameDirs(ValidFileData, &PluginData, &(Dirs->At(i)), &oldPluginData, &(oldDirs->At(i))))
                                    break;
                            }
                            if (i == Dirs->Count)
                            {
                                for (i = 0; i < Files->Count; i++)
                                {
                                    if (!AreTheSameFiles(ValidFileData, &PluginData, &(Files->At(i)), &oldPluginData, &(oldFiles->At(i))))
                                        break;
                                }
                                if (i == Files->Count)
                                    transferIconsAndThumbnailsAsNew = TRUE;
                            }
                        }
                        // we will put old versions of icons and thumbnails into it
                        IconCache->GetIconsAndThumbsFrom(oldIconCache, NULL, transferIconsAndThumbnailsAsNew,
                                                         forceReloadThumbnails);
                    }
                }
            }
        }
        delete oldIconCache;
    }

    WaitBeforeReadingIcons = 0;

    DontClearNextFocusName = FALSE;

    // Transfer the labels, cut-to-clip-flag, icon-overlay-index, and directory sizes from old items to new ones
    //  SetSel(FALSE, -1); // newly loaded, so the Selected flag is 0 everywhere; SelectedCount is also 0
    int oldCount = oldDirs->Count + oldFiles->Count;
    int count = Dirs->Count + Files->Count;
    if (count != oldCount + 1 && focusFirstNewItem)
        focusFirstNewItem = FALSE; // one item has not been added

    // if 'caseSensitive' is TRUE, we require exact (case sensitive) name matching
    // during the following synchronization of the flag
    BOOL caseSensitive = IsCaseSensitive();

    int firstNewItemIsDir = -1; // -1 (unknown), 0 (is a file), 1 (is a directory)
    int i = 0;
    if (i < Dirs->Count) // skip ".." (up-dir symbol) in new data
    {
        CFileData* newData = &Dirs->At(i);
        if (newData->NameLen == 2 && newData->Name[0] == '.' && newData->Name[1] == '.')
            i++;
    }
    int j = 0;
    if (j < oldDirs->Count) // skip ".." (up-dir symbol) in old data
    {
        CFileData* oldData = &oldDirs->At(j);
        if (oldData->NameLen == 2 && oldData->Name[0] == '.' && oldData->Name[1] == '.')
            j++;
    }
    for (; j < oldDirs->Count; j++) // first the directories
    {
        CFileData* oldData = &oldDirs->At(j);
        if (focusFirstNewItem || oldData->Selected || oldData->SizeValid || oldData->CutToClip ||
            oldData->IconOverlayIndex != ICONOVERLAYINDEX_NOTUSED)
        {
            while (i < Dirs->Count)
            {
                CFileData* newData = &Dirs->At(i);
                if (!LessNameExtIgnCase(*newData, *oldData, FALSE)) // new >= old
                {
                    if (LessNameExtIgnCase(*oldData, *newData, FALSE))
                        break; // new > old -> new != old
                    else       // new == old
                    {
                        // we will find a possible exact match between names that are the same except for the case
                        int ii = i;
                        BOOL exactMatch = FALSE; // TRUE if the old and new name match case sensitive
                        while (ii < Dirs->Count)
                        {
                            CFileData* newData2 = &Dirs->At(ii);
                            if (!LessNameExt(*newData2, *oldData, FALSE)) // new >= old
                            {
                                if (!LessNameExt(*oldData, *newData2, FALSE)) // old == new (exactly) - prefer over ignoring case match
                                {
                                    exactMatch = TRUE;
                                    if (ii > i) // We skipped at least one item (it was smaller than the old item we were looking for)
                                    {
                                        if (focusFirstNewItem) // new item found
                                        {
                                            strcpy(NextFocusName, newData->Name);
                                            firstNewItemIsDir = 1 /* is a directory*/;
                                            focusFirstNewItem = FALSE;
                                        }
                                        i = ii;
                                        newData = newData2;
                                    }
                                }
                                break; // in any case, we end with searching for exact matches
                            }
                            ii++;
                        }

                        if (!caseSensitive || exactMatch)
                        {
                            // transfer values from the old to the new item
                            if (oldData->Selected)
                                SetSel(TRUE, newData);
                            newData->SizeValid = oldData->SizeValid;
                            if (newData->SizeValid)
                                newData->Size = oldData->Size;
                            newData->CutToClip = oldData->CutToClip;
                            newData->IconOverlayIndex = oldData->IconOverlayIndex;
                        }
                        i++;
                        break;
                    }
                }
                else // new < old
                {
                    if (focusFirstNewItem) // new item found
                    {
                        strcpy(NextFocusName, newData->Name);
                        firstNewItemIsDir = 1 /* is a directory*/;
                        focusFirstNewItem = FALSE;
                    }
                }
                i++;
            }
            if (i >= Dirs->Count)
                break; // end of searching for marked items
        }
    }
    if (focusFirstNewItem && i == Dirs->Count - 1) // new item found
    {
        strcpy(NextFocusName, Dirs->At(i).Name);
        firstNewItemIsDir = 1 /* is a directory*/;
        focusFirstNewItem = FALSE;
    }

    i = 0;
    for (j = 0; j < oldFiles->Count; j++) // files after directories
    {
        CFileData* oldData = &oldFiles->At(j);
        if (focusFirstNewItem || oldData->Selected || oldData->CutToClip ||
            oldData->IconOverlayIndex != ICONOVERLAYINDEX_NOTUSED)
        {
            while (i < Files->Count)
            {
                CFileData* newData = &Files->At(i);
                if (!LessNameExtIgnCase(*newData, *oldData, FALSE)) // new >= old
                {
                    if (LessNameExtIgnCase(*oldData, *newData, FALSE))
                        break; // new > old -> new != old
                    else       // new == old (ignore case)
                    {
                        // we will find a possible exact match between names that are the same except for the case
                        int ii = i;
                        BOOL exactMatch = FALSE; // TRUE if the old and new name match case sensitive
                        while (ii < Files->Count)
                        {
                            CFileData* newData2 = &Files->At(ii);
                            if (!LessNameExt(*newData2, *oldData, FALSE)) // new >= old
                            {
                                if (!LessNameExt(*oldData, *newData2, FALSE)) // old == new (exactly) - prefer over ignoring case match
                                {
                                    exactMatch = TRUE;
                                    if (ii > i) // We skipped at least one item (it was smaller than the old item we were looking for)
                                    {
                                        if (focusFirstNewItem) // new item found
                                        {
                                            if (!Is(ptDisk) || (newData->Attr & FILE_ATTRIBUTE_TEMPORARY) == 0) // on disk we ignore tmp files (they disappear right away), see https://forum.altap.cz/viewtopic.php?t=2496
                                            {
                                                strcpy(NextFocusName, newData->Name);
                                                firstNewItemIsDir = 0 /* is a file*/;
                                            }
                                            focusFirstNewItem = FALSE;
                                        }
                                        i = ii;
                                        newData = newData2;
                                    }
                                }
                                break; // in any case, we end with searching for exact matches
                            }
                            ii++;
                        }

                        if (!caseSensitive || exactMatch)
                        {
                            // transfer values from the old to the new item
                            if (oldData->Selected)
                                SetSel(TRUE, newData);
                            newData->CutToClip = oldData->CutToClip;
                            newData->IconOverlayIndex = oldData->IconOverlayIndex;
                        }
                        i++;
                        break;
                    }
                }
                else // new < old
                {
                    if (focusFirstNewItem) // new item found
                    {
                        if (!Is(ptDisk) || (newData->Attr & FILE_ATTRIBUTE_TEMPORARY) == 0) // on disk we ignore tmp files (they disappear right away), see https://forum.altap.cz/viewtopic.php?t=2496
                        {
                            strcpy(NextFocusName, newData->Name);
                            firstNewItemIsDir = 0 /* is a file*/;
                        }
                        focusFirstNewItem = FALSE;
                    }
                }
                i++;
            }
            if (i >= Files->Count)
                break; // end of searching for marked items
        }
    }
    if (focusFirstNewItem && i == Files->Count - 1) // new item found
    {
        if (!Is(ptDisk) || (Files->At(i).Attr & FILE_ATTRIBUTE_TEMPORARY) == 0) // on disk we ignore tmp files (they disappear right away), see https://forum.altap.cz/viewtopic.php?t=2496
        {
            strcpy(NextFocusName, Files->At(i).Name);
            firstNewItemIsDir = 0 /* is a file*/;
        }
        focusFirstNewItem = FALSE;
    }

    // Return to the user-selected sorting
    if (changeSortType)
    {
        if (!iconReaderIsSleeping && (UseSystemIcons || UseThumbnails))
            SleepIconCacheThread(); // Before changing Files/Dirs, it is necessary to sleep the icon-thread (if it is not already sleeping)
        ReverseSort = currentReverseSort;
        SortType = currentSortType;
        SortDirectory();
        if (!iconReaderIsSleeping && (UseSystemIcons || UseThumbnails))
            WakeupIconCacheThread();
    }

    if (iconCacheBackuped && (UseSystemIcons || UseThumbnails)) // wake-up up to SortDirectory()
    {
        if (!oldIconCacheValid ||             // if reading of icons has not finished
            oldInactWinOptimizedReading ||    // if only the icons from the visible part of the panel were read
            !transferIconsAndThumbnailsAsNew) // if the listing has changed
        {
            WakeupIconCacheThread(); // Let's have it load all icons again (currently showing old versions)
        }
        else
            IconCacheValid = TRUE; // We will not start the icon-reader, icons/thumbnails/overlays are loaded, there is nothing to solve (preventing infinite cycle of refresh in case of a network drive from which at least one icon cannot be loaded - if we start the icon-reader, it will try to load it, which will trigger a refresh again and we will cycle).
    }

    // find the index of the item for focus
    BOOL foundFocus = FALSE;
    if (NextFocusName[0] != 0)
    {
        MainWindow->CancelPanelsUI();       // cancel QuickSearch and QuickEdit
        int l = (int)strlen(NextFocusName); // Trim trailing spaces
        while (l > 0 && NextFocusName[l - 1] <= ' ')
            l--;
        NextFocusName[l] = 0;
        int off = 0; // Trim leading spaces
        while (off < l && NextFocusName[off] <= ' ')
            off++;
        if (off != 0)
            memmove(NextFocusName, NextFocusName + off, l - off + 1);
        l -= off;

        int found = -1;
        for (i = 0; i < count; i++)
        {
            CFileData* f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (f->NameLen == (unsigned)l &&
                StrICmpEx(f->Name, f->NameLen, NextFocusName, l) == 0 &&
                (firstNewItemIsDir == -1 /* we do not know what it is*/ ||
                 firstNewItemIsDir == 0 /* is a file*/ && i >= Dirs->Count ||
                 firstNewItemIsDir == 1 /* is a directory*/ && i < Dirs->Count))
            {
                foundFocus = TRUE;
                ensureFocusIndexVisible = TRUE;
                wholeItemVisible = TRUE;                                  // we want complete visibility of the new item
                if (StrCmpEx(f->Name, f->NameLen, NextFocusName, l) == 0) // found: exact
                {
                    focusIndex = i;
                    break;
                }
                if (found == -1)
                    found = i;
            }
        }
        if (i == count && found != -1)
            focusIndex = found; // found: ignore-case
        NextFocusName[0] = 0;
    }

    // First, we search for the old focus in the new listing (based on the case sensitivity of the current listing)
    if (!foundFocus && focusData.Name != NULL)
    {
        if (focusIsDir)
        {
            count = Dirs->Count;
            for (i = 0; i < count; i++)
            {
                CFileData* d2 = &Dirs->At(i);
                if (StrICmpEx(d2->Name, d2->NameLen, focusData.Name, focusData.NameLen) == 0 &&
                    (!caseSensitive || StrCmpEx(d2->Name, d2->NameLen, focusData.Name, focusData.NameLen) == 0))
                {
                    focusIndex = i;
                    foundFocus = TRUE;
                    break;
                }
            }
        }
        else
        {
            count = Files->Count;
            for (i = 0; i < count; i++)
            {
                CFileData* d2 = &Files->At(i);
                if (StrICmpEx(d2->Name, d2->NameLen, focusData.Name, focusData.NameLen) == 0 &&
                    (!caseSensitive || StrCmpEx(d2->Name, d2->NameLen, focusData.Name, focusData.NameLen) == 0))
                {
                    focusIndex = Dirs->Count + i;
                    foundFocus = TRUE;
                    break;
                }
            }
        }
    }

    // If we did not find an old focus (it is not in the list), the position of the focus is searched according to the current sorting
    if (!foundFocus)
    {
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

        if (focusData.Name != NULL) // we need to find focus
        {
            if (focusIsDir)
            {
                i = 0;
                count = Dirs->Count;
                if (count > 0 && strcmp(Dirs->At(0).Name, "..") == 0)
                    i = 1; // ".." we need to skip, it's not included
                for (; i < count; i++)
                {
                    CFileData* d2 = &Dirs->At(i);
                    if (!lessDirs(*d2, focusData, ReverseSort)) // thanks to sorting, TRUE will be at the first larger item
                    {
                        focusIndex = i;
                        break;
                    }
                }
            }
            else
            {
                count = Files->Count;
                for (i = 0; i < count; i++)
                {
                    CFileData* d2 = &Files->At(i);
                    if (!lessFiles(*d2, focusData, ReverseSort)) // thanks to sorting, TRUE will be at the first larger item
                    {
                        focusIndex = Dirs->Count + i;
                        break;
                    }
                }
                count += Dirs->Count;
            }
        }
        MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
        if (focusIndex >= count)
            focusIndex = max(0, count - 1);
    }

    // release of the backed up old listing
    ReleaseListingBody(oldPanelType, oldArchiveDir, oldPluginFSDir, oldPluginData,
                       oldFiles, oldDirs, TRUE);

    // hiding the cursor in quick-search mode before drawing
    if (QuickSearchMode)
        HideCaret(ListBox->HWindow);

    // refresh panel
    RefreshListBox(xOffset, topIndex, focusIndex, ensureFocusIndexVisible, wholeItemVisible);

    // Restoring cursor position and displaying cursor in quick-search mode after drawing
    if (QuickSearchMode)
    {
        SetQuickSearchCaretPos();
        ShowCaret(ListBox->HWindow);
    }

    EndStopRefresh();
    if (setWait)
        SetCursor(oldCur);
}

void CFilesWindow::LayoutListBoxChilds()
{
    CALL_STACK_MESSAGE_NONE
    ListBox->LayoutChilds(FALSE);
}

void CFilesWindow::RepaintListBox(DWORD drawFlags)
{
    CALL_STACK_MESSAGE_NONE
    ListBox->PaintAllItems(NULL, drawFlags);
}

void CFilesWindow::EnsureItemVisible(int index)
{
    CALL_STACK_MESSAGE_NONE
    ListBox->EnsureItemVisible(index, FALSE);
}

void CFilesWindow::SetQuickSearchCaretPos()
{
    CALL_STACK_MESSAGE1("CFilesWindow::SetQuickSearchCaretPos()");
    if (!QuickSearchMode || FocusedIndex < 0 || FocusedIndex >= Dirs->Count + Files->Count)
        return;

    SIZE s;
    int isDir = 0;
    CFileData* file;
    if (FocusedIndex < Dirs->Count)
    {
        file = &Dirs->At(FocusedIndex);
        isDir = FocusedIndex != 0 || strcmp(file->Name, "..") != 0 ? 1 : 2 /* UP-DIR*/;
    }
    else
        file = &Files->At(FocusedIndex - Dirs->Count);
    char formatedFileName[MAX_PATH];
    AlterFileName(formatedFileName, file->Name, -1,
                  Configuration.FileNameFormat, 0,
                  FocusedIndex < Dirs->Count);

    int qsLen = (int)strlen(QuickSearch);
    int preLen = isDir && !Configuration.SortDirsByExt ? file->NameLen : (int)(file->Ext - file->Name);
    char* ss;
    BOOL ext = FALSE;
    int offset = 0;
    if ((!isDir || Configuration.SortDirsByExt) && GetViewMode() == vmDetailed &&
        IsExtensionInSeparateColumn() && file->Ext[0] != 0 && file->Ext > file->Name + 1 && // Exception for names like ".htaccess", they are displayed in the Name column even though they are extensions
        qsLen >= preLen)
    {
        ss = formatedFileName + preLen;
        qsLen -= preLen;
        offset = Columns[0].Width + 4 - (3 + IconSizes[ICONSIZE_16]);
        ext = TRUE;
    }
    else
    {
        ss = formatedFileName;
    }

    HDC hDC = ListBox->HPrivateDC;
    HFONT hOldFont;
    if (TrackingSingleClick && SingleClickIndex == FocusedIndex)
        hOldFont = (HFONT)SelectObject(hDC, FontUL);
    else
        hOldFont = (HFONT)SelectObject(hDC, Font);

    GetTextExtentPoint32(hDC, ss, qsLen, &s);

    RECT r;
    if (ListBox->GetItemRect(FocusedIndex, &r))
    {
        // It's all because quick search is not case sensitive
        int x = 0, y = 0;
        switch (GetViewMode())
        {
        case vmBrief:
        case vmDetailed:
        {
            x = r.left + 3 + IconSizes[ICONSIZE_16] + s.cx + offset;
            y = r.top + 2;
            // if the width of the column Name is manually restricted, we ensure stopping
            // centered at maximum width; otherwise it will spill into the next columns
            if (GetViewMode() == vmDetailed && !ext && x >= (int)Columns[0].Width)
                x = Columns[0].Width - 3;
            x -= ListBox->XOffset;
            break;
        }

        case vmThumbnails:
        case vmIcons:
        {
            // for now, I ignore the situation where the cursor should be on the second line
            int iconH = 2 + 32;
            if (GetViewMode() == vmThumbnails)
            {
                iconH = 3 + ListBox->ThumbnailHeight + 2;
            }
            iconH += 3 + 2 - 1;

            // WARNING: maintain consistency with CFilesBox::GetIndex
            char buff[1024];                           // Target buffer for strings
            int maxWidth = ListBox->ItemWidth - 4 - 1; // -1 to avoid touching
            char* out1 = buff;
            int out1Len = 512;
            int out1Width;
            char* out2 = buff + 512;
            int out2Len = 512;
            int out2Width;
            SplitText(hDC, formatedFileName, file->NameLen, &maxWidth,
                      out1, &out1Len, &out1Width,
                      out2, &out2Len, &out2Width);
            //maxWidth += 4;

            if (s.cx > out1Width)
                s.cx = out1Width;
            x = r.left + (ListBox->ItemWidth - out1Width) / 2 + s.cx - 1;
            y = r.top + iconH;
            break;
        }

        case vmTiles:
        {
            // WARNING: maintain consistency with CFilesBox::GetIndex, see the call to GetTileTexts
            //        int itemWidth = rect.right - rect.left; // item width
            int maxTextWidth = ListBox->ItemWidth - TILE_LEFT_MARGIN - IconSizes[ICONSIZE_48] - TILE_LEFT_MARGIN - 4;
            int widthNeeded = 0;

            char buff[3 * 512]; // Target buffer for strings
            char* out0 = buff;
            int out0Len;
            char* out1 = buff + 512;
            int out1Len;
            char* out2 = buff + 1024;
            int out2Len;
            HDC hDC2 = ItemBitmap.HMemDC;
            HFONT hOldFont2 = (HFONT)SelectObject(hDC2, Font);
            GetTileTexts(file, isDir, hDC2, maxTextWidth, &widthNeeded,
                         out0, &out0Len, out1, &out1Len, out2, &out2Len,
                         ValidFileData, &PluginData, Is(ptDisk));
            SelectObject(hDC2, hOldFont2);
            widthNeeded += 5;

            int visibleLines = 1; // the name is definitely visible
            if (out1[0] != 0)
                visibleLines++;
            if (out2[0] != 0)
                visibleLines++;
            int textH = visibleLines * FontCharHeight + 4;

            int iconW = IconSizes[ICONSIZE_48];
            int iconH = IconSizes[ICONSIZE_48];

            if (s.cx > maxTextWidth)
                s.cx = maxTextWidth;

            x = r.left + iconW + 7 + s.cx;
            y = r.top + (iconH - textH) / 2 + 6;

            break;
        }

        default:
            TRACE_E("GetViewMode()=" << GetViewMode());
        }
        SetCaretPos(x, y);
    }
    else
        TRACE_E("Caret is outside of FilesRect");
    SelectObject(hDC, hOldFont);
}

void CFilesWindow::SetupListBoxScrollBars()
{
    CALL_STACK_MESSAGE_NONE
    ListBox->OldVertSI.cbSize = 0; // force call to SetScrollInfo
    ListBox->OldHorzSI.cbSize = 0;
    ListBox->SetupScrollBars(UPDATE_HORZ_SCROLL | UPDATE_VERT_SCROLL);
}

void CFilesWindow::RefreshForConfig()
{
    CALL_STACK_MESSAGE1("CFilesWindow::RefreshForConfig()");
    if (Is(ptZIPArchive))
    { // At the archive, we will ensure a refresh by damaging the archive stamp
        SetZIPArchiveSize(CQuadWord(-1, -1));
    }

    // hard refresh
    HANDLES(EnterCriticalSection(&TimeCounterSection));
    int t1 = MyTimeCounter++;
    HANDLES(LeaveCriticalSection(&TimeCounterSection));
    SendMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
}

// the colors or color depths of the screen have changed; new imagelists have been created
// for toolbars and they need to be assigned to the controls that use them
void CFilesWindow::OnColorsChanged()
{
    if (DirectoryLine != NULL && DirectoryLine->ToolBar != NULL)
    {
        DirectoryLine->ToolBar->SetImageList(HGrayToolBarImageList);
        DirectoryLine->ToolBar->SetHotImageList(HHotToolBarImageList);
        DirectoryLine->OnColorsChanged();
        // We need to insert a disk icon into the new toolbar
        UpdateDriveIcon(FALSE);
    }

    if (StatusLine != NULL)
    {
        StatusLine->OnColorsChanged();
    }

    if (IconCache != NULL)
    {
        SleepIconCacheThread();
        IconCache->ColorsChanged();
        if (UseSystemIcons || UseThumbnails)
            WakeupIconCacheThread();
    }
}

CViewModeEnum
CFilesWindow::GetViewMode()
{
    if (ListBox == NULL)
    {
        TRACE_E("ListBox == NULL");
        return vmDetailed;
    }
    return ListBox->ViewMode;
}

CIconSizeEnum
CFilesWindow::GetIconSizeForCurrentViewMode()
{
    CViewModeEnum viewMode = GetViewMode();
    switch (viewMode)
    {
    case vmBrief:
    case vmDetailed:
        return ICONSIZE_16;

    case vmIcons:
        return ICONSIZE_32;

    case vmTiles:
    case vmThumbnails:
        return ICONSIZE_48;

    default:
        TRACE_E("GetIconSizeForCurrentViewMode() unknown ViewMode=" << viewMode);
        return ICONSIZE_16;
    }
}
