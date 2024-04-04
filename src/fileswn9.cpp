// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <shlwapi.h>
#undef PathIsPrefix // otherwise collision with CSalamanderGeneral::PathIsPrefix

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "stswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "snooper.h"
#include "pack.h"
#include "shellib.h"
#include "cache.h"
extern "C"
{
#include "shexreg.h"
}
#include "salshlib.h"

// !!! do not use StdColumnsPrivate directly, use GetStdColumn() instead
CColumDataItem StdColumnsPrivate[STANDARD_COLUMNS_COUNT] =
    {
        // Flag, Name, Description, GetText function, Sort, Left, ID
        /*0*/ {0, IDS_COLUMN_NAME_NAME, IDS_COLUMN_DESC_NAME, NULL, 1, 1, COLUMN_ID_NAME},
        /*1*/ {VIEW_SHOW_EXTENSION, IDS_COLUMN_NAME_EXT, IDS_COLUMN_DESC_EXT, NULL, 1, 1, COLUMN_ID_EXTENSION},
        /*2*/ {VIEW_SHOW_DOSNAME, IDS_COLUMN_NAME_DOSNAME, IDS_COLUMN_DESC_DOSNAME, InternalGetDosName, 0, 1, COLUMN_ID_DOSNAME},
        /*3*/ {VIEW_SHOW_SIZE, IDS_COLUMN_NAME_SIZE, IDS_COLUMN_DESC_SIZE, InternalGetSize, 1, 0, COLUMN_ID_SIZE},
        /*4*/ {VIEW_SHOW_TYPE, IDS_COLUMN_NAME_TYPE, IDS_COLUMN_DESC_TYPE, InternalGetType, 0, 1, COLUMN_ID_TYPE},
        /*5*/ {VIEW_SHOW_DATE, IDS_COLUMN_NAME_DATE, IDS_COLUMN_DESC_DATE, NULL /* see below*/, 1, 0, COLUMN_ID_DATE},
        /*6*/ {VIEW_SHOW_TIME, IDS_COLUMN_NAME_TIME, IDS_COLUMN_DESC_TIME, NULL /* see below*/, 1, 0, COLUMN_ID_TIME},
        /*7*/ {VIEW_SHOW_ATTRIBUTES, IDS_COLUMN_NAME_ATTR, IDS_COLUMN_DESC_ATTR, InternalGetAttr, 1, 0, COLUMN_ID_ATTRIBUTES},
        /*8*/ {VIEW_SHOW_DESCRIPTION, IDS_COLUMN_NAME_DESC, IDS_COLUMN_DESC_DESC, InternalGetDescr, 0, 1, COLUMN_ID_DESCRIPTION},
};

CColumDataItem* GetStdColumn(int i, BOOL isDisk)
{
    if (i == 5 /* date*/)
        StdColumnsPrivate[5].GetText = isDisk ? InternalGetDateOnlyForDisk : InternalGetDate; // On disk, we use 1.1.1602 0:00:00.000 as an empty value (empty space in a column in the panel)
    else
    {
        if (i == 6 /* time*/)
            StdColumnsPrivate[6].GetText = isDisk ? InternalGetTimeOnlyForDisk : InternalGetTime; // On disk, we use 1.1.1602 0:00:00.000 as an empty value (empty space in a column in the panel)
    }
    return &StdColumnsPrivate[i];
}

BOOL CFilesWindow::ParsePath(char* path, int& type, BOOL& isDir, char*& secondPart,
                             const char* errorTitle, char* nextFocus, int* error, int pathBufSize)
{
    CALL_STACK_MESSAGE3("CFilesWindow::ParsePath(%s, , , , %s, ,)", path, errorTitle);

    const char* curArchivePath = NULL;
    char curPath[2 * MAX_PATH];
    GetGeneralPath(curPath, 2 * MAX_PATH);
    if (Is(ptZIPArchive))
    {
        SalPathAddBackslash(curPath, 2 * MAX_PATH);
        curArchivePath = GetZIPArchive();
    }

    return SalParsePath(HWindow, path, type, isDir, secondPart, errorTitle, nextFocus,
                        Is(ptDisk) || Is(ptZIPArchive), curPath, curArchivePath, error, pathBufSize);
}

int CFilesWindow::GetPanelCode()
{
    CALL_STACK_MESSAGE_NONE
    return (MainWindow != NULL && MainWindow->LeftPanel == this) ? PANEL_LEFT : PANEL_RIGHT;
}

void CFilesWindow::ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs)
{
    CALL_STACK_MESSAGE_NONE
    PathHistory->ClearPluginFSFromHistory(fs);
}

BOOL SafeInvokeCommand(IContextMenu2* menu, CMINVOKECOMMANDINFO& ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporarily lower the priority of the thread so that some confused shell extension does not eat up the CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    BOOL ret = FALSE;
    __try
    {
        ret = menu->InvokeCommand(&ici) == S_OK;
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 6))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
    return ret;
}

BOOL CFilesWindow::ClipboardPaste(BOOL onlyLinks, BOOL onlyTest, const char* pastePath)
{
    CALL_STACK_MESSAGE4("CFilesWindow::ClipboardPaste(%d, %d, %s)", onlyLinks, onlyTest, pastePath);
    IDataObject* dataObj;
    BOOL files = FALSE;       // Check if the data on the clipboard are actually files
    BOOL filesOnClip = FALSE; // Is there something of ours on the clipboard to "paste" into a file/directory?
                              //  TRACE_I("CFilesWindow::ClipboardPaste() called: " << (onlyLinks ? "links " : "") <<
                              //          (onlyTest ? "test " : "") << (pastePath != NULL ? pastePath : "(null)"));
    if (OleGetClipboard(&dataObj) == S_OK && dataObj != NULL)
    {
        if (!onlyLinks && !onlyTest && IsFakeDataObject(dataObj, NULL, NULL, 0) && SalShExtSharedMemView != NULL)
        { // Salamaner-like "fake" data object on clipboard -> paste file/directory from archive
            BOOL pasteFromOurData = FALSE;
            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
            if (SalShExtSharedMemView->DoPasteFromSalamander &&
                SalShExtSharedMemView->SalamanderMainWndPID == GetCurrentProcessId() &&
                SalShExtSharedMemView->SalamanderMainWndTID == GetCurrentThreadId() &&
                SalShExtSharedMemView->PastedDataID == SalShExtPastedData.GetDataID())
            {
                pasteFromOurData = TRUE;
                SalShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // Under W2K+, it is probably no longer necessary: we will set the time of the last GetData() back by a minute so that any subsequent Release of the data object runs smoothly
            }
            ReleaseMutex(SalShExtSharedMemMutex);

            if (pasteFromOurData)
            {
                DWORD pasteEffect = 0;
                if (OpenClipboard(HWindow))
                {
                    HANDLE handle = GetClipboardData(RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT));
                    if (handle != NULL)
                    {
                        DWORD* effect = (DWORD*)HANDLES(GlobalLock(handle));
                        if (effect != NULL)
                        {
                            pasteEffect = *effect;
                            HANDLES(GlobalUnlock(handle));
                        }
                    }
                    CloseClipboard();
                }
                else
                    TRACE_E("OpenClipboard() has failed!");
                SalShExtPastedData.SetLock(TRUE);
                dataObj->Release(); // there is still one instance left on the clipboard
                pasteEffect &= DROPEFFECT_COPY | DROPEFFECT_MOVE;
                if (pasteEffect == DROPEFFECT_COPY || pasteEffect == DROPEFFECT_MOVE)
                {
                    // move cleans the clipboard after itself (cut & paste), we will release the clipboard even before starting
                    // the operations themselves so that other applications can use it for further data transfers
                    if (pasteEffect == DROPEFFECT_MOVE)
                        OleSetClipboard(NULL);

                    // perform the Paste operation alone
                    SalShExtPastedData.DoPasteOperation(pasteEffect == DROPEFFECT_COPY,
                                                        pastePath != NULL ? pastePath : GetPath());

                    FocusFirstNewItem = TRUE; // if it's a new file, let the focus find it
                    if (pasteEffect != DROPEFFECT_COPY)
                    {
                        IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                        IdleCheckClipboard = TRUE; // we will also check the clipboard
                    }
                }
                else
                    TRACE_E("Paste: unexpected paste-effect: " << pasteEffect);

                SalShExtPastedData.SetLock(FALSE);
                PostMessage(MainWindow->HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // after unlocking, we may release the data if necessary
                return TRUE;
            }
        }

        BOOL ownRutine = FALSE; // Are we able to do it ourselves?
        IEnumFORMATETC* enumFormat;
        UINT cfIdList = RegisterClipboardFormat(CFSTR_SHELLIDLIST);
        UINT cfFileContent = RegisterClipboardFormat(CFSTR_FILECONTENTS);
        if (dataObj->EnumFormatEtc(DATADIR_GET, &enumFormat) == S_OK)
        {
            FORMATETC formatEtc;
            enumFormat->Reset();
            while (enumFormat->Next(1, &formatEtc, NULL) == S_OK)
            {
                if (formatEtc.cfFormat == cfIdList ||
                    formatEtc.cfFormat == cfFileContent && !onlyLinks) // file contents (saved during Copy from Windows Mobile (WinCE)) - cannot create shortcuts (not easily possible for file contents)
                {
                    files = TRUE;
                }
                if (formatEtc.cfFormat == CF_HDROP)
                    ownRutine = files = TRUE;
            }
            enumFormat->Release();
        }

        BOOL ourClipDataObject = FALSE;
        DWORD effect = 0;
        if (ownRutine) // if there is hope for self-execution, we will determine whether it is a copy or a move
        {
            DWORD dropEffect = 0;
            UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
            UINT cfSalDataObject = RegisterClipboardFormat(SALCF_IDATAOBJECT);
            if (onlyLinks || onlyTest)
                ownRutine = FALSE; // We don't know how to link and testing is not needed

            if (OpenClipboard(HWindow))
            {
                if (!onlyLinks && !onlyTest)
                {
                    HANDLE handle = GetClipboardData(cfPrefDrop);
                    if (handle != NULL)
                    {
                        DWORD* effectPtr = (DWORD*)HANDLES(GlobalLock(handle));
                        if (effectPtr != NULL)
                        {
                            dropEffect = *effectPtr;
                            HANDLES(GlobalUnlock(handle));
                        }
                    }
                }
                if (GetClipboardData(cfSalDataObject) != NULL)
                    ourClipDataObject = TRUE;
                else
                    ownRutine = FALSE; // if it is not our IDataObject, we will not perform our own operation
                CloseClipboard();
            }
            else
                TRACE_E("OpenClipboard() has failed!");

            if (!onlyLinks && !onlyTest && ownRutine)
            {
                effect = (dropEffect & (DROPEFFECT_COPY | DROPEFFECT_MOVE));
                if (effect == 0)
                    ownRutine = FALSE; // neither copy nor move - we don't know anything else
            }
        }
        filesOnClip = files && ourClipDataObject;

        if (ownRutine) // we will perform our own routine - copy or move
        {
            if (pastePath != NULL)
                strcpy(DropPath, pastePath);
            else
                strcpy(DropPath, GetPath());
            CImpDropTarget* dropTarget = new CImpDropTarget(MainWindow->HWindow, DoCopyMove, this,
                                                            GetCurrentDirClipboard, this,
                                                            DropEnd, this, NULL, NULL, NULL, NULL,
                                                            UseOwnRutine, DoDragDropOper, this,
                                                            NULL, NULL);
            if (dropTarget != NULL)
            {
                OurClipDataObject = ourClipDataObject;
                POINTL pt;
                pt.x = pt.y = 0;
                DWORD eff = effect;
                dropTarget->DragEnter(dataObj, 0, pt, &effect);
                effect = eff;
                dropTarget->DragOver(0, pt, &effect);
                effect = eff;
                dropTarget->Drop(dataObj, 0, pt, &eff);

                FocusFirstNewItem = TRUE; // if it's a new file, let the focus find it

                dropTarget->Release();
                OurClipDataObject = FALSE;
                if (effect == DROPEFFECT_MOVE)
                {
                    if (OpenClipboard(HWindow))
                    {
                        EmptyClipboard();
                        CloseClipboard();
                    }
                    else
                        TRACE_E("OpenClipboard() has failed!");
                }

                if (effect != DROPEFFECT_COPY)
                {
                    IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                    IdleCheckClipboard = TRUE; // we will also check the clipboard
                }

                // Refresh of the panel is done in DropCopyMove
            }
        }
        else
        {
            if (files && !onlyTest) // perform paste/pastelink
            {
                //        MainWindow->ReleaseMenuNew();  // Windows are not built for multiple context menus

                IContextMenu2* menu = CreateIContextMenu2(MainWindow->HWindow, GetPath());
                if (menu != NULL)
                {
                    OurClipDataObject = ourClipDataObject;
                    CShellExecuteWnd shellExecuteWnd;
                    CMINVOKECOMMANDINFO ici;
                    ici.cbSize = sizeof(CMINVOKECOMMANDINFO);
                    ici.fMask = 0;
                    ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: CFilesWindow::ClipboardPaste onlyLinks=%d", onlyLinks);
                    if (onlyLinks)
                        ici.lpVerb = "pastelink";
                    else
                        ici.lpVerb = "paste";
                    ici.lpParameters = NULL;
                    ici.lpDirectory = GetPath();
                    ici.nShow = SW_SHOWNORMAL;
                    ici.dwHotKey = 0;
                    ici.hIcon = 0;

                    if (onlyLinks)
                        PasteLinkIsRunning++; // I'd rather count on competing with more threads

                    SafeInvokeCommand(menu, ici);

                    if (onlyLinks && PasteLinkIsRunning > 0)
                        PasteLinkIsRunning--;

                    IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                    IdleCheckClipboard = TRUE; // we will also check the clipboard

                    FocusFirstNewItem = TRUE; // if it's a new file, let the focus find it

                    menu->Release();
                    OurClipDataObject = FALSE;
                }

                //--- refresh non-automatically refreshed directories
                // source directory (important for "move") is unknown, we will rely on refresh when
                // activation of the main window (is present for "copy" and "move", not present for "paste link");
                // Thanks to running the operation in another thread, there is no chance to ensure correct refresh,
                // so at least one of us will post, maybe it will work...;
                // 1/10 second to perform the operation or at least its part
                Sleep(100);
                // we will report a change in the target directory and its subdirectories
                MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                // we will post the refresh to both panels (if they are not auto-refreshed - that is, to the panel
                // with FS it won't be enough)
                if (!MainWindow->LeftPanel->AutomaticRefresh || !MainWindow->RightPanel->AutomaticRefresh)
                {
                    // another 1/3 second to perform the operation or at least part of it; at least one panel
                    // is not refreshed - "worth" spending user's time
                    Sleep(300);

                    HANDLES(EnterCriticalSection(&TimeCounterSection));
                    int t1 = MyTimeCounter++;
                    int t2 = MyTimeCounter++;
                    HANDLES(LeaveCriticalSection(&TimeCounterSection));
                    if (!MainWindow->LeftPanel->AutomaticRefresh)
                    {
                        PostMessage(MainWindow->LeftPanel->HWindow, WM_USER_REFRESH_DIR, 0, t1);
                    }
                    if (!MainWindow->RightPanel->AutomaticRefresh)
                    {
                        PostMessage(MainWindow->RightPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);
                    }
                }
            }
        }
        if (onlyTest && onlyLinks && files)
        {
            UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
            if (OpenClipboard(MainWindow->HWindow))
            {
                HANDLE h = GetClipboardData(cfPrefDrop);
                if (h != NULL)
                {
                    void* ptr = HANDLES(GlobalLock(h));
                    if (ptr != NULL)
                    {
                        files = (*((DWORD*)ptr) & DROPEFFECT_LINK) != 0;
                        HANDLES(GlobalUnlock(h));
                    }
                }
                CloseClipboard();
            }
            else
                TRACE_E("OpenClipboard() has failed!");
        }

        dataObj->Release();
    }

    if (!filesOnClip)
    {
        if (MainWindow->LeftPanel->CutToClipChanged)
            MainWindow->LeftPanel->ClearCutToClipFlag(TRUE);
        if (MainWindow->RightPanel->CutToClipChanged)
            MainWindow->RightPanel->ClearCutToClipFlag(TRUE);
    }

    return files;
}

BOOL CFilesWindow::ClipboardPasteToArcOrFS(BOOL onlyTest, DWORD* pasteDefEffect)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ClipboardPasteToArcOrFS(%d,)", onlyTest);
    if (pasteDefEffect != NULL)
        *pasteDefEffect = 0;
    BOOL ret = FALSE;
    IDataObject* dataObj;
    if (OleGetClipboard(&dataObj) == S_OK && dataObj != NULL)
    {
        CDragDropOperData* namesList = NULL;
        if (!onlyTest)
        {
            namesList = new CDragDropOperData;
            if (namesList == NULL)
                TRACE_E(LOW_MEMORY);
        }
        if (IsSimpleSelection(dataObj, namesList)) // we will determine if they are files/directories from the disk and at the same time if they are from one path
        {
            ret = TRUE;
            // we will determine whether it is a copy or a move
            DWORD dropEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE; // if we do not detect the effect, we implicitly take both (Copy will be used by default)
            UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
            if (OpenClipboard(HWindow))
            {
                HANDLE handle = GetClipboardData(cfPrefDrop);
                if (handle != NULL)
                {
                    DWORD* effect = (DWORD*)HANDLES(GlobalLock(handle));
                    if (effect != NULL)
                    {
                        dropEffect = *effect;
                        HANDLES(GlobalUnlock(handle));
                    }
                }
                CloseClipboard();
            }
            else
                TRACE_E("OpenClipboard() has failed!");
            dropEffect &= (DROPEFFECT_COPY | DROPEFFECT_MOVE);
            if (pasteDefEffect != NULL)
                *pasteDefEffect = dropEffect;

            if (!onlyTest && namesList != NULL) // attempt is not just a test, we will perform the desired operation
            {
                BOOL moveOper = FALSE;
                if (Is(ptZIPArchive)) // Paste into archive
                {
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0 &&                               // We found a supported archive
                        PackerFormatConfig.GetUsePacker(format - 1)) // edit?
                    {
                        if (dropEffect == (DROPEFFECT_COPY | DROPEFFECT_MOVE))
                            dropEffect = DROPEFFECT_COPY; // Copy has priority (it is safer)
                        if (dropEffect != 0)
                        {
                            DoDragDropOper(dropEffect == DROPEFFECT_COPY, TRUE, GetZIPArchive(),
                                           GetZIPPath(), namesList, this);
                            namesList = NULL;         // DoDragDropOper deallocates it, we won't do it here anymore
                            FocusFirstNewItem = TRUE; // if it's a new file, let the focus find it
                            moveOper = dropEffect == DROPEFFECT_MOVE;
                        }
                        else
                            TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unexpected paste-effect!");
                    }
                }
                else
                {
                    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty()) // Copy to file system
                    {
                        if ((dropEffect & DROPEFFECT_COPY) &&
                            GetPluginFS()->IsServiceSupported(FS_SERVICE_COPYFROMDISKTOFS))
                        {
                            dropEffect = DROPEFFECT_COPY;
                        }
                        else
                        {
                            if ((dropEffect & DROPEFFECT_MOVE) &&
                                GetPluginFS()->IsServiceSupported(FS_SERVICE_MOVEFROMDISKTOFS))
                            {
                                dropEffect = DROPEFFECT_MOVE;
                            }
                            else
                                dropEffect = 0;
                        }
                        if (dropEffect != 0)
                        {
                            char userPart[MAX_PATH];
                            if (GetPluginFS()->GetCurrentPath(userPart))
                            {
                                DoDragDropOper(dropEffect == DROPEFFECT_COPY, FALSE, GetPluginFS()->GetPluginFSName(),
                                               userPart, namesList, this);
                                namesList = NULL;         // DoDragDropOper deallocates it, we won't do it here anymore
                                FocusFirstNewItem = TRUE; // if it's a new file, let the focus find it
                                moveOper = dropEffect == DROPEFFECT_MOVE;
                            }
                            else
                                TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unable to get current path from FS!");
                        }
                        else
                            TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unexpected paste-effect!");
                    }
                }
                if (moveOper) // Cut & Paste: we need to clear the clipboard
                {
                    if (OpenClipboard(HWindow))
                    {
                        EmptyClipboard();
                        CloseClipboard();
                    }
                    else
                        TRACE_E("OpenClipboard() has failed!");
                    IdleRefreshStates = TRUE;  // During the next Idle, we will force the check of status variables
                    IdleCheckClipboard = TRUE; // we will also check the clipboard
                }
            }
        }
        if (namesList != NULL)
            delete namesList;
        dataObj->Release();
    }
    return ret;
}

void CFilesWindow::ClipboardCopy()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ClipboardCopy()");
    ShellAction(this, saCopyToClipboard);
}

void CFilesWindow::ClipboardCut()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ClipboardCut()");
    ShellAction(this, saCutToClipboard);
}

BOOL CFilesWindow::ClipboardPasteLinks(BOOL onlyTest)
{
    CALL_STACK_MESSAGE2("CFilesWindow::ClipboardPasteLinks(%d)", onlyTest);
    return ClipboardPaste(TRUE, onlyTest);
}

BOOL CFilesWindow::IsTextOnClipboard()
{
    CALL_STACK_MESSAGE1("CFilesWindow::IsTextOnClipboard()");
    BOOL text = FALSE;

    int attempt = 1;
    while (1)
    {
        if (OpenClipboard(HWindow))
        {
            HANDLE handle = GetClipboardData(CF_TEXT);
            if (handle != NULL)
            {
                char* path = (char*)HANDLES(GlobalLock(handle));
                if (path != NULL)
                {
                    text = TRUE;
                    HANDLES(GlobalUnlock(handle));
                }
            }
            CloseClipboard();
            break;
        }
        else
        {
            if (attempt++ <= 10)
                Sleep(10); // wait for the clipboard to be released (max. 100 ms)
            else
            {
                TRACE_E("OpenClipboard() has failed!");
                break;
            }
        }
    }
    return text;
}

BOOL CFilesWindow::PostProcessPathFromUser(HWND parent, char (&buff)[2 * MAX_PATH])
{
    if (!IsFileURLPath(buff) && IsPluginFSPath(buff))
        return TRUE; // processing will be left to the FS plugin

    // trim spaces at the beginning and at the end
    CutSpacesFromBothSides(buff);

    // We will remove the quotation marks, see https://forum.altap.cz/viewtopic.php?t=4160
    CutDoubleQuotesFromBothSides(buff);

    if (IsFileURLPath(buff)) // it's a URL: we will convert the URL (file://) to a Windows path
    {
        char path[MAX_PATH];
        DWORD pathLen = _countof(path);
        if (PathCreateFromUrl(buff, path, &pathLen, 0) == S_OK)
            strcpy_s(buff, path);
        else
        {
            SalMessageBox(parent, LoadStr(IDS_THEPATHISINVALID), LoadStr(IDS_ERRORCHANGINGDIR),
                          MB_OK | MB_ICONEXCLAMATION);
            return FALSE;
        }
        return TRUE;
    }
    else
    {
        if (IsPluginFSPath(buff))
            return TRUE; // processing will be left to the FS plugin
    }

    // expanding ENV variables (due to numerous requests on the forum and for our own needs)
    // if the ENV variable does not exist, it will have the opportunity to create a directory with the same name
    char expandedBuff[_countof(buff) + 1];
    DWORD auxRes = ExpandEnvironmentStrings(buff, expandedBuff, _countof(expandedBuff));
    if (auxRes == 0 || auxRes > _countof(buff))
    {
        TRACE_E("ExpandEnvironmentStrings failed.");
        return FALSE;
    }
    else
    {
        strcpy(buff, expandedBuff);
    }

    return TRUE;
}

void CFilesWindow::ClipboardPastePath()
{
    CALL_STACK_MESSAGE1("CFilesWindow::ClipboardPastePath()");
    char buff[2 * MAX_PATH];
    buff[0] = 0;
    BOOL changePath = FALSE;

    if (OpenClipboard(HWindow))
    {
        HANDLE handle = GetClipboardData(CF_UNICODETEXT);
        if (handle != NULL)
        {
            WCHAR* pathW = (WCHAR*)HANDLES(GlobalLock(handle));
            if (pathW != NULL)
            {
                while (*pathW != 0 && *pathW <= L' ')
                    pathW++; // Trim spaces+CR+LF before the path
                if (ConvertU2A(pathW, -1, buff, _countof(buff)))
                    changePath = TRUE;
                else
                {
                    if (buff[0] != 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                        changePath = TRUE; // small buffer, not handled (same as ANSI version)
                }
                HANDLES(GlobalUnlock(handle));
            }
        }
        if (!changePath) // probably not unicode, let's try ANSI (if it's unicode, ANSI is often broken - without diacritics)
        {
            handle = GetClipboardData(CF_TEXT);
            char* path = (char*)HANDLES(GlobalLock(handle));
            if (path != NULL)
            {
                while (*path != 0 && *path <= ' ')
                    path++; // Trim spaces+CR+LF before the path
                lstrcpyn(buff, path, _countof(buff));

                changePath = TRUE;
                HANDLES(GlobalUnlock(handle));
            }
        }
        CloseClipboard();
    }
    else
        TRACE_E("OpenClipboard() has failed!");
    if (changePath && PostProcessPathFromUser(HWindow, buff))
        ChangeDir(buff); // change path
}

void CFilesWindow::ChangeFilter(BOOL disable)
{
    CALL_STACK_MESSAGE1("CFilesWindow::ChangeFilter()");
    BeginStopRefresh(); // we do not need any refreshes
    if (disable || CFilterDialog(HWindow, &Filter, Configuration.FilterHistory, &FilterEnabled /*, &FilterInverse*/).Execute() == IDOK)
    {
        if (disable)
        {
            FilterEnabled = FALSE;
        }
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));
        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        UpdateFilterSymbol();
        DirectoryLineSetText();
        DirectoryLine->InvalidateIfNeeded();
    }
    UpdateWindow(MainWindow->HWindow);
    EndStopRefresh();
}

BOOL CFilesWindow::OnLButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
    LButtonDown.x = LOWORD(lParam);
    LButtonDown.y = HIWORD(lParam);
    LButtonDownTime = GetTickCount();
    DragBoxLeft = 1;
    FocusedSinceClick = FALSE;

    BOOL noBeginDrag = FALSE; // user clicks with the left button and adds with the right
    if (BeginDragDrop)
    {
        noBeginDrag = TRUE;
        ReleaseCapture();
        BeginDragDrop = 0;
    }
    if (DragBox)
    {
        EndDragBox();
        noBeginDrag = TRUE;
    }

    // Compatibility with Windows Explorer: clicking outside (in active and inactive panel)
    // means renaming confirmation; we consider other actions (Alt+Tab) (unlike Explorer)
    // for cancellation of renaming
    if (!MainWindow->DoQuickRename())
    {
        *lResult = 0;
        return TRUE;
    }

    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (Dirs->Count + Files->Count == 0)
    {
        if (GetFocus() != GetListBoxHWND())
            MainWindow->FocusPanel(this);
        if (Is(ptDisk))
            RefreshDirectory();
    }
    else
    {
        BOOL dontSetSince = FALSE;
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        const int index = GetIndex(x, y);
        if (index >= 0 && index < Dirs->Count + Files->Count)
        {
            if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                    (GetKeyState(VK_MENU) & 0x8000) == 0 &&
                    (GetKeyState(VK_SHIFT) & 0x8000) == 0 ||
                (GetKeyState(VK_SHIFT) & 0x8000) != 0 &&
                    (GetKeyState(VK_MENU) & 0x8000) == 0 &&
                    (GetKeyState(VK_CONTROL) & 0x8000) == 0)
            {
                if (GetFocus() != GetListBoxHWND())
                    MainWindow->FocusPanel(this);
                if (GetKeyState(VK_SHIFT) & 0x8000) // shift + lButton -> ext. sel.
                {
                    int first = GetCaretIndex();
                    int last = index;
                    if (first != last)
                    {
                        // Group marking / marking by item removed when Shift is pressed
                        SetSelRange(SelectItems, first, last);
                    }
                    FocusedSinceClick = (index == FocusedIndex);
                    dontSetSince = TRUE;
                    SetCaretIndex(index, TRUE);
                    if (first != last)
                        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                }
                else // ctrl+lbutton -> normal selection.
                {
                    BOOL select = GetSel(index);
                    SetSel(!select, index);
                    FocusedSinceClick = (index == FocusedIndex);
                    dontSetSince = TRUE;
                    SetCaretIndex(index, TRUE, TRUE);
                }
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            if (index == 0 && index < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0)
            {
                // hack for UpDir, where we want to allow dragging the cage (d&d doesn't make sense anyway)
                if (GetFocus() != GetListBoxHWND())
                    MainWindow->FocusPanel(this);
                if (!noBeginDrag)
                    BeginBoxSelect = 1;
            }
            else
            {
                int oldPT = PersistentTracking;
                PersistentTracking = TRUE;
                SetCapture(GetListBoxHWND());
                PersistentTracking = oldPT;
                BeginDragDrop = 1;
                DragDropLeftMouseBtn = TRUE;
            }
            if (TrackingSingleClick)
                SingleClickAnchorIndex = index;
            if (!dontSetSince)
                FocusedSinceClick = (index == FocusedIndex) && GetFocus() == GetListBoxHWND();
            SetCaretIndex(index, TRUE);
        }
        else
        {
            if (GetFocus() != GetListBoxHWND())
                MainWindow->FocusPanel(this);
            //      if (!Configuration.FullRowSelect) BeginBoxSelect = 1;
            if (!noBeginDrag)
                BeginBoxSelect = 1;
        }
    }
    *lResult = 0;
    return TRUE;
}

BOOL CFilesWindow::OnLButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (BeginDragDrop)
    {
        int oldPT = PersistentTracking;
        PersistentTracking = TRUE;
        ReleaseCapture();
        PersistentTracking = oldPT;
        KillTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN);
    }

    if (BeginDragDrop && GetFocus() != GetListBoxHWND())
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int index = GetIndex(x, y);
        MainWindow->FocusPanel(this);
        if (index >= 0 && index < Dirs->Count + Files->Count)
            SetCaretIndex(index, TRUE);
    }
    if (DragBox)
        EndDragBox();

    if (BeginDragDrop && FocusedSinceClick &&
        Configuration.ClickQuickRename && !Configuration.SingleClick)
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        RECT labelRect;
        int index = GetIndex(x, y, FALSE, &labelRect);
        if (index >= 0 && index < Dirs->Count + Files->Count && index == FocusedIndex &&
            x >= labelRect.left && x < labelRect.right &&
            y >= labelRect.top && y < labelRect.bottom)
        {
            QuickRenameIndex = index;
            QuickRenameRect = labelRect;
            QuickRenameTimer = SetTimer(GetListBoxHWND(), IDT_QUICKRENAMEBEGIN,
                                        GetDoubleClickTime() + 10, NULL);
        }
    }

    if (TrackingSingleClick)
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int index = GetIndex(x, y);
        MainWindow->FocusPanel(this);
        if (TrackingSingleClick && SingleClickAnchorIndex != -1 &&
            index >= 0 && index < Dirs->Count + Files->Count &&
            SingleClickAnchorIndex == index)
        {
            TrackingSingleClick = 0;
            SetSingleClickIndex(-1);
            PostMessage(GetListBoxHWND(), WM_LBUTTONDBLCLK, wParam, lParam);
        }
    }

    BeginDragDrop = 0;
    BeginBoxSelect = 0;

    return FALSE;
}

BOOL CFilesWindow::OnLButtonDblclk(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
    FocusedSinceClick = FALSE;
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (GetFocus() != GetListBoxHWND())
        MainWindow->FocusPanel(this);
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    int index = GetIndex(x, y);
    if (index >= 0 && index < Dirs->Count + Files->Count)
    {
        SetCaretIndex(index, TRUE);
        if ((GetKeyState(VK_MENU) & 0x8000) != 0 &&
            (GetKeyState(VK_SHIFT) & 0x8000) == 0 &&
            (GetKeyState(VK_CONTROL) & 0x8000) == 0)
        {
            CtrlPageDnOrEnter(VK_RETURN);
        }
        else
        {
            Execute(index);
        }
    }
    *lResult = 0;
    return TRUE;
}

BOOL CFilesWindow::OnRButtonDown(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
    BOOL selecting = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ^
                     (Configuration.PrimaryContextMenu == FALSE);

    BOOL noBeginDrag = FALSE; // user clicks with the left button and adds with the right
    if (BeginDragDrop)
    {
        noBeginDrag = TRUE;
        ReleaseCapture();
        BeginDragDrop = 0;
    }
    if (DragBox)
    {
        EndDragBox();
        noBeginDrag = TRUE;
    }

    LButtonDown.x = LOWORD(lParam);
    LButtonDown.y = HIWORD(lParam);
    LButtonDownTime = GetTickCount();
    DragBoxLeft = 0;
    MainWindow->CancelPanelsUI(); // cancel QuickSearch and QuickEdit
    if (Dirs->Count + Files->Count == 0)
    {
        if (GetFocus() != GetListBoxHWND())
            MainWindow->FocusPanel(this);
    }
    else
    {
        int x = LOWORD(lParam);
        int y = HIWORD(lParam);
        int index = GetIndex(x, y);
        if (index >= 0 && index < Dirs->Count + Files->Count)
        {
            if (selecting)
            {
                if (GetFocus() != GetListBoxHWND())
                    MainWindow->FocusPanel(this);

                if (GetKeyState(VK_SHIFT) & 0x8000) // shift + rButton -> ext. sel.
                {
                    int first = GetCaretIndex();
                    int last = index;
                    if (first != last)
                    {
                        // group identification / identification based on the first item
                        SetSelRange(!GetSel(first), first, last);
                    }
                    SetCaretIndex(last, TRUE, TRUE);
                    if (first != last)
                        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                }
                else
                {
                    DragSelect = TRUE;
                    SetCapture(GetListBoxHWND());
                    BOOL select = GetSel(index);
                    SelectItems = !select;
                    SetSel(!select, index);
                    SetCaretIndex(index, TRUE, TRUE);
                    ScrollObject.BeginScroll();
                }
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
            }
            else
            {
                if (index == 0 && index < Dirs->Count && strcmp(Dirs->At(0).Name, "..") == 0)
                {
                    // hack for UpDir, where we want to allow dragging the cage (d&d doesn't make sense anyway)
                    if (GetFocus() != GetListBoxHWND())
                        MainWindow->FocusPanel(this);
                    if (!noBeginDrag)
                        BeginBoxSelect = 1;
                }
                else
                {
                    int oldPT = PersistentTracking;
                    PersistentTracking = TRUE;
                    SetCapture(GetListBoxHWND());
                    PersistentTracking = oldPT;
                    BeginDragDrop = 1;
                    DragDropLeftMouseBtn = FALSE;
                }
                SetCaretIndex(index, TRUE);
            }
        }
        else
        {
            if (GetFocus() != GetListBoxHWND())
                MainWindow->FocusPanel(this);
            if (!noBeginDrag /*&& !Configuration.FullRowSelect*/)
                BeginBoxSelect = 1;
        }
    }
    *lResult = 0;
    return TRUE;
}

BOOL CFilesWindow::OnRButtonUp(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
    int x = LOWORD(lParam);
    int y = HIWORD(lParam);
    int index = GetIndex(x, y);

    if (BeginDragDrop)
    {
        int oldPT = PersistentTracking;
        PersistentTracking = TRUE;
        ReleaseCapture();
        PersistentTracking = oldPT;
        KillTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN);
    }

    int mx = abs(LButtonDown.x - x);
    int my = abs(LButtonDown.y - y);

    BOOL contextMenu = (mx <= GetSystemMetrics(SM_CXDRAG) || my < GetSystemMetrics(SM_CYDRAG)) && !DragSelect || DragBox;

    if (DragSelect)
    {
        ScrollObject.EndScroll();
        ReleaseCapture();
        KillTimer(GetListBoxHWND(), IDT_SCROLL); // could be thrown
        DragSelect = FALSE;
    }
    if (DragBox)
        EndDragBox();
    if (BeginDragDrop && GetFocus() != GetListBoxHWND())
    {
        if (GetFocus() != GetListBoxHWND())
            MainWindow->FocusPanel(this);
        if (index >= 0 && index < Dirs->Count + Files->Count)
            SetCaretIndex(index, TRUE);
    }
    BeginDragDrop = 0;
    BeginBoxSelect = 0;
    if (contextMenu)
    {
        if (GetFocus() != GetListBoxHWND())
            MainWindow->FocusPanel(this);
        if (index >= 0 && index < Dirs->Count + Files->Count)
        {
            SetCaretIndex(index, TRUE);

            if (GetSelCount() > 0) // Is there a label?
            {
                if (index < Dirs->Count && !Dirs->At(index).Selected ||
                    index >= Dirs->Count && !Files->At(index - Dirs->Count).Selected)
                {                                                   // test if clicked in selection (otherwise we need to cancel the selection)
                    SetSel(FALSE, -1, TRUE);                        // explicit override
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }
            }

            UserWorkedOnThisPath = TRUE;
            ShellAction(this, saContextMenu);
        }
        else
        {
            if (GetSelCount() > 0) // We need to remove the label (this menu is for the panel - click behind the items)
            {
                SetSel(FALSE, -1, TRUE);                        // explicit override
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                UpdateWindow(MainWindow->HWindow);
            }

            UserWorkedOnThisPath = TRUE;
            ShellAction(this, saContextMenu, FALSE, TRUE, TRUE); // menu only for panel (clicking behind items)
        }
        *lResult = 0;
        return TRUE; // we do not want to let WM_CONTEXTMENU be generated
    }
    return FALSE;
}

BOOL CFilesWindow::IsDragDropSafe(int x, int y)
{
    // user does not want our safer drag-and-drop
    if (!Configuration.UseDragDropMinTime)
        return TRUE;

    // Allow dragging outside the main window for quick file dropping
    POINT pt;
    pt.x = x;
    pt.y = y;
    ClientToScreen(HWindow, &pt);
    HWND hWndHit = WindowFromPoint(pt);
    HWND hTopHit = GetTopLevelParent(hWndHit);
    if (hWndHit != NULL && hTopHit != NULL && hTopHit != MainWindow->HWindow)
        return TRUE;

    // It's been quite a while since the beginning of the pull
    if ((int)(GetTickCount() - LButtonDownTime) > Configuration.DragDropMinTime)
        return TRUE;

    return FALSE;
}

void CFilesWindow::OfferArchiveUpdateIfNeededAux(HWND parent, int textID, BOOL* archMaybeUpdated)
{
    *archMaybeUpdated = FALSE;
    if (AssocUsed) // if the user edited files from the archive, we need to update them before operating with the archive, otherwise we would be working with old versions of edited files that are stored directly in the archive
    {
        // Display information about the need to update the archive in which edited files are stored
        char text[MAX_PATH + 500];
        sprintf(text, LoadStr(textID), GetZIPArchive());
        SalMessageBox(parent, text, LoadStr(IDS_INFOTITLE),
                      MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT);
        // we will package the modified files, prepare them for further use
        BOOL someFilesChanged;
        UnpackedAssocFiles.CheckAndPackAndClear(parent, &someFilesChanged, archMaybeUpdated);
        SetEvent(ExecuteAssocEvent); // start file cleanup
        DiskCache.WaitForIdle();
        ResetEvent(ExecuteAssocEvent); // Finish cleaning up files

        // If files in the disk cache can be edited, we will discard them, better safe than sorry
        // unpacks them again when accessed
        if (someFilesChanged)
        {
            char buf[MAX_PATH];
            StrICpy(buf, GetZIPArchive()); // the name of the archive in the disk cache is in lowercase (allows case-insensitive comparison of names from the Windows file system)
            DiskCache.FlushCache(buf);
        }
        AssocUsed = FALSE;
    }
}

void CFilesWindow::OfferArchiveUpdateIfNeeded(HWND parent, int textID, BOOL* archMaybeUpdated)
{
    BeginStopRefresh(); // we do not need any refreshes

    OfferArchiveUpdateIfNeededAux(parent, textID, archMaybeUpdated);

    CFilesWindow* otherPanel = MainWindow->LeftPanel == this ? MainWindow->RightPanel : MainWindow->LeftPanel;
    BOOL otherPanelArchMaybeUpdated = FALSE;
    if (otherPanel->Is(ptZIPArchive) && StrICmp(GetZIPArchive(), otherPanel->GetZIPArchive()) == 0)
    { // The same archive is also in the second panel, we need to update it as well
        otherPanel->OfferArchiveUpdateIfNeededAux(parent, textID, &otherPanelArchMaybeUpdated);
        if (otherPanelArchMaybeUpdated)
            *archMaybeUpdated = TRUE;
    }
    if (*archMaybeUpdated)
    {
        // a bit of a mess: the user pressed e.g. F5 and instead we force an update of the archive and his
        // refresh in the panel ... F5 then must be pressed again (could be solved with PostMessage(uMsg,
        // wParam, lParam), but he should rather check what is selected in the panel first and then proceed
        // press F5 again) ... it's a very marginal thing, so hopefully this will be enough
        HANDLES(EnterCriticalSection(&TimeCounterSection));
        int t1 = MyTimeCounter++;
        int t2 = MyTimeCounter++;
        HANDLES(LeaveCriticalSection(&TimeCounterSection));

        PostMessage(HWindow, WM_USER_REFRESH_DIR, 0, t1);
        if (otherPanelArchMaybeUpdated)
            PostMessage(otherPanel->HWindow, WM_USER_REFRESH_DIR, 0, t2);
    }

    EndStopRefresh();
}

BOOL CFilesWindow::OnMouseMove(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    *lResult = 0;
    if (BeginDragDrop && (wParam & (MK_LBUTTON | MK_RBUTTON))) // "Dragging" for drag&drop
    {
        int x = abs(LButtonDown.x - (short)LOWORD(lParam));
        int y = abs(LButtonDown.y - (short)HIWORD(lParam));
        if (x > GetSystemMetrics(SM_CXDRAG) || y > GetSystemMetrics(SM_CYDRAG)) // shift
        {
            // Condition "!PerformingDragDrop" added after AS 2.52 due to the state reported by ehter:
            // There must be two files in the directory (one EXE downloaded using IE, containing security ADS) and some other file
            // the second file is loaded onto the EXE through d&d, which pops up a security window, however, it is NON-MODAL and has a message loop
            // takze nam hlavni vlakno Salamandera uvazne v ShellAction(); potom druhy drag&drop vedl na pad
            // Using this patch, we will disable the new d&d, just like Windows Explorer does
            // The solution is not clean because Salamander runs only seemingly (the main thread is not in our application loop)
            if (!PerformingDragDrop)
            {
                if (Is(ptZIPArchive) && AssocUsed) // if the user edited files from the archive, we need to update them before operating with the archive, otherwise we would be working with old versions of edited files that are stored directly in the archive
                {
                    BOOL archMaybeUpdated;
                    OfferArchiveUpdateIfNeeded(MainWindow->HWindow, IDS_ARCHIVECLOSEEDIT3, &archMaybeUpdated);
                }
                else
                {
                    if (IsDragDropSafe((short)LOWORD(lParam), (short)HIWORD(lParam))) // min. time
                    {
                        if (TrackingSingleClick)
                        {
                            TrackingSingleClick = 0;
                            SetSingleClickIndex(-1);
                        }
                        BeginDragDrop = 0;
                        ReleaseCapture();
                        KillTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN);

                        // if certain items are marked and we tear one of the unmarked ones,
                        // shoot down select
                        if (!GetSel(GetCaretIndex()) && GetSelCount() > 0)
                        {
                            SetSel(FALSE, -1, TRUE);
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                        }

                        PerformingDragDrop = TRUE;
                        HIMAGELIST hDragIL = NULL;
                        int dxHotspot, dyHotspot;
                        int imgWidth, imgHeight;
                        hDragIL = CreateDragImage(LButtonDown.x, LButtonDown.y, dxHotspot, dyHotspot, imgWidth, imgHeight);
                        ImageList_BeginDrag(hDragIL, 0, dxHotspot, dyHotspot);
                        ImageDragBegin(imgWidth, imgHeight, dxHotspot, dyHotspot);

                        UserWorkedOnThisPath = TRUE;
                        ShellAction(this, DragDropLeftMouseBtn ? saLeftDragFiles : saRightDragFiles);

                        ImageDragEnd();
                        ImageList_EndDrag();
                        ImageList_Destroy(hDragIL);
                        PerformingDragDrop = FALSE;

                        IdleRefreshStates = TRUE; // During the next Idle, we will force the check of status variables
                    }
                    else
                    {
                        if (Configuration.UseDragDropMinTime)
                        {
                            SetTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN, Configuration.DragDropMinTime - (GetTickCount() - LButtonDownTime) + 10, NULL);
                        }
                    }
                }
            }
        }
        return TRUE;
    }

    if (BeginBoxSelect && (wParam & (MK_LBUTTON | MK_RBUTTON))) // "opening" selection box
    {
        short xPos = (short)LOWORD(lParam);
        short yPos = (short)HIWORD(lParam);
        int x = abs(LButtonDown.x - xPos);
        int y = abs(LButtonDown.y - yPos);
        if (x > GetSystemMetrics(SM_CXDRAG) || y > GetSystemMetrics(SM_CYDRAG))
        {
            BeginBoxSelect = 0;
            if (FilesMap.CreateMap())
            {
                if (TrackingSingleClick)
                {
                    TrackingSingleClick = 0;
                    SetSingleClickIndex(-1);
                }
                SetCapture(GetListBoxHWND());
                OldBoxPoint.x = xPos;
                OldBoxPoint.y = yPos;
                FilesMap.SetAnchor(LButtonDown.x, LButtonDown.y);
                DrawDragBox(OldBoxPoint);
                DragBox = 1;
                FilesMap.SetPoint(xPos, yPos);
                ScrollObject.BeginScroll();
            }
        }
        return TRUE;
    }

    if (DragBox && (wParam & (MK_LBUTTON | MK_RBUTTON)))
    {
        DrawDragBox(OldBoxPoint);
        short xPos = (short)LOWORD(lParam);
        short yPos = (short)HIWORD(lParam);
        OldBoxPoint.x = xPos;
        OldBoxPoint.y = yPos;
        DrawDragBox(OldBoxPoint);
        FilesMap.SetPoint(xPos, yPos);
        return TRUE;
    }

    if (DragSelect && (wParam & MK_RBUTTON))
    {
        int x = (short)LOWORD(lParam), y = (short)HIWORD(lParam);
        int count = Dirs->Count + Files->Count;
        if (count == 0)
            return TRUE;
        int index = GetIndex(x, y, TRUE);
        if (index >= 0 && index < count)
        {
            int caret = GetCaretIndex();
            BOOL select = GetSel(index);
            if (index != caret || select != (int)SelectItems)
            {
                SetSelRange(SelectItems, caret, index);
                RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                SetCaretIndex(index, TRUE);
            }
        }
        return TRUE;
    }

    if (Configuration.SingleClick)
    {
        if (!TrackingSingleClick)
        {
            SingleClickAnchorIndex = -1;
        }
        int x = (short)LOWORD(lParam);
        int y = (short)HIWORD(lParam);
        int index = -1;
        if (x >= ListBox->FilesRect.left && x < ListBox->FilesRect.right &&
            y >= ListBox->FilesRect.top && y < ListBox->FilesRect.bottom)
        {
            if (TrackingSingleClick == 0)
            {
                TRACKMOUSEEVENT tme;
                tme.cbSize = sizeof(tme);
                tme.dwFlags = TME_LEAVE;
                tme.hwndTrack = GetListBoxHWND();
                TrackMouseEvent(&tme);
                TrackingSingleClick = 1;
            }
            index = GetIndex(x, y);
            if (index < 0 || index >= Dirs->Count + Files->Count)
                index = -1;
        }
        else
        {
            SendMessage(GetListBoxHWND(), WM_MOUSELEAVE, 0, 0);
            TrackingSingleClick = 0;
        }
        SetSingleClickIndex(index);
    }
    return FALSE;
}

BOOL CFilesWindow::CanBeFocused()
{
    CALL_STACK_MESSAGE_NONE
    RECT r = ListBox->FilesRect;
    return (r.right - r.left >= MIN_PANELWIDTH);
}

BOOL CFilesWindow::OnTimer(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (wParam == IDT_SINGLECLICKSELECT)
    {
        POINT p;
        GetCursorPos(&p);
        if (TrackingSingleClick && !SnooperSuspended && WindowFromPoint(p) == GetListBoxHWND())
        {
            ScreenToClient(GetListBoxHWND(), &p);
            const int index = GetIndex(p.x, p.y);
            if (index >= 0 && index < Dirs->Count + Files->Count)
            {
                if (MainWindow->EditMode)
                {
                    CFilesWindow* activePanel = MainWindow->GetActivePanel();

                    if (CanBeFocused())
                    {
                        SetCaretIndex(index, TRUE);
                        if (activePanel != this)
                            MainWindow->ChangePanel();
                    }
                }
                else
                {
                    // catch labeling
                    if ((GetKeyState(VK_CONTROL) & 0x8000) != 0 &&
                            (GetKeyState(VK_MENU) & 0x8000) == 0 &&
                            (GetKeyState(VK_SHIFT) & 0x8000) == 0 ||
                        (GetKeyState(VK_SHIFT) & 0x8000) != 0 &&
                            (GetKeyState(VK_MENU) & 0x8000) == 0 &&
                            (GetKeyState(VK_CONTROL) & 0x8000) == 0)
                    {
                        if (GetKeyState(VK_SHIFT) & 0x8000) // shift + lButton -> ext. sel.
                        {
                            int first = GetCaretIndex();
                            int last = index;
                            if (first != last)
                            {
                                // Group marking / marking by item removed when Shift is pressed
                                SetSelRange(SelectItems, first, last);
                                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                            }
                            SetCaretIndex(last, TRUE);
                            if (first != last)
                                RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                        }
                        else // ctrl+lbutton -> normal selection.
                        {
                            BOOL select = GetSel(index);
                            SetSel(!select, index);
                            SetCaretIndex(index, TRUE, TRUE);
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                        }
                    }
                    else
                    {
                        SetCaretIndex(index, TRUE);
                    }
                    if (GetFocus() != GetListBoxHWND())
                        MainWindow->FocusPanel(this);
                }
            }
        }
        KillTimer(GetListBoxHWND(), IDT_SINGLECLICKSELECT);
        return FALSE;
    }
    if (wParam == IDT_QUICKRENAMEBEGIN)
    {
        KillQuickRenameTimer(); // prevent possible opening of QuickRenameWindow
        if (GetForegroundWindow() == MainWindow->HWindow &&
            MainWindow->GetActivePanel() == this)
        {
            QuickRenameBegin(QuickRenameIndex, &QuickRenameRect);
        }
        return FALSE;
    }
    if (wParam == IDT_PANELSCROLL)
    {
        ScrollObject.OnWMTimer();
        return FALSE;
    }
    if (wParam == IDT_DRAGDROPTESTAGAIN)
    {
        KillTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN);
    }
    else
    {
        if (wParam != IDT_SCROLL)
            return FALSE;
    }
    POINT p;
    GetCursorPos(&p);
    ScreenToClient(GetListBoxHWND(), &p);
    lParam = MAKELONG(p.x, p.y);
    wParam = MK_RBUTTON;
    return OnMouseMove(wParam, lParam, lResult);
}

BOOL CFilesWindow::OnCaptureChanged(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (!PersistentTracking && TrackingSingleClick)
    {
        TrackingSingleClick = 0;
        SetSingleClickIndex(-1);
    }
    return FALSE;
}

BOOL CFilesWindow::OnCancelMode(WPARAM wParam, LPARAM lParam, LRESULT* lResult)
{
    CALL_STACK_MESSAGE_NONE
    if (GetCapture() == GetListBoxHWND())
        ReleaseCapture();
    if (BeginDragDrop)
        BeginDragDrop = 0;
    if (TrackingSingleClick)
    {
        TrackingSingleClick = 0;
        SetSingleClickIndex(-1);
    }
    EndDragBox();
    return FALSE;
}

// returns TRUE if the DIB contains any alpha byte other than 0
BOOL ContainsAlpha(void* lpBits, int width, int height)
{
    int row;
    for (row = 0; row < height; row++)
    {
        DWORD* imagePtr = (DWORD*)lpBits + row * width;
        int col;
        for (col = 0; col < width; col++)
        {
            DWORD argb = *imagePtr;
            BYTE alpha = (BYTE)((argb & 0xFF000000) >> 24);

            if (alpha != 0)
            {
                return TRUE;
            }

            imagePtr++;
        }
    }
    return FALSE;
}

// copies the mask to the DIB in order to identify transparent/non-transparent areas and sets the alpha channel accordingly
BOOL SetAlphaByMask(void* lpBits, int width, int height, HDC hMaskDC)
{
    HDC hMemMaskDC = HANDLES(CreateCompatibleDC(NULL));
    BITMAPINFOHEADER bmhdr;
    memset(&bmhdr, 0, sizeof(bmhdr));
    bmhdr.biSize = sizeof(bmhdr);
    bmhdr.biWidth = width;
    bmhdr.biHeight = -height; // top-down
    bmhdr.biPlanes = 1;
    bmhdr.biBitCount = 32;
    bmhdr.biCompression = BI_RGB;
    void* lpMemMaskBits = NULL;
    HBITMAP hMemMaskBmp = HANDLES(CreateDIBSection(hMemMaskDC, (CONST BITMAPINFO*)&bmhdr,
                                                   DIB_RGB_COLORS, &lpMemMaskBits, NULL, 0));
    if (hMemMaskBmp == NULL || hMemMaskBmp == NULL)
    {
        HANDLES(DeleteDC(hMemMaskDC));
        return FALSE;
    }
    HBITMAP hOldMemMaskBmp = (HBITMAP)SelectObject(hMemMaskDC, hMemMaskBmp);
    BitBlt(hMemMaskDC, 0, 0, width, height, hMaskDC, 0, 0, SRCCOPY);
    SelectObject(hMemMaskDC, hOldMemMaskBmp);

    int row;
    for (row = 0; row < height; row++)
    {
        DWORD* imagePtr = (DWORD*)lpBits + row * width;
        DWORD* maskPtr = (DWORD*)lpMemMaskBits + row * width;
        int col;
        for (col = 0; col < width; col++)
        {
            if (*maskPtr == 0)
                *imagePtr |= 0xFF000000;
            imagePtr++;
            maskPtr++;
        }
    }

    HANDLES(DeleteDC(hMemMaskDC));
    HANDLES(DeleteObject(hMemMaskBmp));
    return TRUE;
}

HIMAGELIST
CFilesWindow::CreateDragImage(int cursorX, int cursorY, int& dxHotspot, int& dyHotspot, int& imgWidth, int& imgHeight)
{
    CALL_STACK_MESSAGE3("CFilesWindow::CreateDragImage(%d, %d, , , )", cursorX, cursorY);
    char buff[MAX_PATH];
    int selCount = GetSelCount();
    int iconWidth = 0;
    int itemIndex = 0;
    int totalCount = Dirs->Count + Files->Count;
    BOOL trimWidth = FALSE;
    if (selCount != 0)
    {
        int files = 0;
        int dirs = 0;
        int i;
        for (i = 0; i < totalCount; i++)
        {
            BOOL isDir = i < Dirs->Count;
            CFileData* f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
            if (i == 0 && isDir && strcmp(Dirs->At(0).Name, "..") == 0)
                continue;
            if (f->Selected == 1)
            {
                if (isDir)
                    dirs++;
                else
                    files++;
            }
        }
        ExpandPluralFilesDirs(buff, MAX_PATH, files, dirs, epfdmSelected, FALSE);
    }
    else
    {
        itemIndex = GetIndex(cursorX, cursorY);
        if (itemIndex < 0 || itemIndex >= totalCount)
        {
            TRACE_E("Invalid itemIndex= " << itemIndex);
            return NULL;
        }
        if (GetViewMode() == vmBrief || GetViewMode() == vmDetailed)
        {
            iconWidth = 1 + IconSizes[ICONSIZE_16] + 1 + 2;
            CFileData* f = (itemIndex < Dirs->Count) ? &Dirs->At(itemIndex) : &Files->At(itemIndex - Dirs->Count);
            AlterFileName(buff, f->Name, -1, Configuration.FileNameFormat, 0, itemIndex < Dirs->Count);
            trimWidth = TRUE;
        }
        else
        {
            // Icons || Thumbnails
            iconWidth = ListBox->ItemWidth;
            buff[0] = 0;
        }
    }
    int buffLen = lstrlen(buff);

    int width;
    int height = ListBox->ItemHeight;
    HDC hDC = HANDLES(CreateCompatibleDC(NULL));
    ;
    HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
    SIZE sz;
    GetTextExtentPoint32(hDC, buff, buffLen, &sz);
    width = iconWidth + sz.cx;
    if (GetViewMode() == vmDetailed && trimWidth)
        width = Columns[0].Width; // if the column is shortened, we do not want other columns to fall into the pulled image

    BITMAPINFOHEADER bmhdr;
    memset(&bmhdr, 0, sizeof(bmhdr));
    bmhdr.biSize = sizeof(bmhdr);
    bmhdr.biWidth = width;
    bmhdr.biHeight = -height; // top-down
    bmhdr.biPlanes = 1;
    bmhdr.biBitCount = 32;
    bmhdr.biCompression = BI_RGB;
    void* lpBits = NULL;
    HBITMAP hBmp = HANDLES(CreateDIBSection(hDC, (CONST BITMAPINFO*)&bmhdr,
                                            DIB_RGB_COLORS, &lpBits, NULL, 0));
    if (hBmp == NULL || lpBits == NULL)
    {
        HANDLES(DeleteDC(hDC));
        return FALSE;
    }

    HBITMAP hOldBmp = (HBITMAP)SelectObject(hDC, hBmp);

    // create a bitmap with a mask
    HBITMAP hMaskBmp = HANDLES(CreateBitmap(width, height, 1, 1, NULL));

    HDC hMaskDC = HANDLES(CreateCompatibleDC(NULL));
    HBITMAP hOldMaskBitmap = (HBITMAP)SelectObject(hMaskDC, hMaskBmp);

    // background underlay
    RECT r;
    r.left = 0;
    r.top = 0;
    r.right = width;
    r.bottom = height;
    FillRect(hDC, &r, HNormalBkBrush);
    PatBlt(hMaskDC, r.left, r.top, r.right, r.bottom, WHITENESS);

    if (selCount > 0)
    {
        int oldBkMode = SetBkMode(hDC, OPAQUE);
        int oldTextColor = SetTextColor(hDC, GetCOLORREF(CurrentColors[ITEM_FG_FOCUSED]));
        int oldBkColor = SetBkColor(hDC, GetCOLORREF(CurrentColors[ITEM_BK_FOCUSED]));
        r.left = iconWidth;
        DrawText(hDC, buff, buffLen, &r, DT_LEFT | DT_SINGLELINE | DT_TOP | DT_NOPREFIX);
        SetTextColor(hDC, oldTextColor);
        SelectObject(hDC, hOldFont);
        hOldFont = (HFONT)SelectObject(hMaskDC, Font);
        SetTextColor(hMaskDC, RGB(0, 0, 0));
        SetBkColor(hMaskDC, RGB(0, 0, 0));
        DrawText(hMaskDC, buff, buffLen, &r, DT_LEFT | DT_SINGLELINE | DT_TOP | DT_NOPREFIX);
        SelectObject(hMaskDC, hOldFont);
        SetTextColor(hDC, oldTextColor);
        SetBkColor(hDC, oldBkColor);
        SetBkMode(hDC, oldBkMode);
    }
    else
        SelectObject(hDC, hOldFont);

    if (selCount == 0)
    {
        r.left = 0;
        int oldXOffset = ListBox->XOffset;
        if (GetViewMode() == vmBrief || GetViewMode() == vmDetailed)
        {
            ListBox->XOffset = 0; // influences the shift of items in detailed mode
            DrawBriefDetailedItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP);
            DrawBriefDetailedItem(hMaskDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_MASK | DRAWFLAG_SKIP_VISTEST);
            if (ContainsAlpha(lpBits, width, height))
            {
                // if the icon had an alpha channel, we will set the entire image as opaque and then redraw only the icon
                SetAlphaByMask(lpBits, width, height, hMaskDC);
                //        currently unresolved, alpha channel of the body will have white specks
                //        the problem is that some data (such as old icons or thumbnails) do not come with an alpha channel
                //        and they will be completely transparent
                //        DrawBriefDetailedItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP | DRAWFLAG_ICON_ONLY);
            }
            ListBox->XOffset = oldXOffset;
            if (GetViewMode() == vmBrief) // I don't know what is in the brief in XOffset, so let's make sure it's not a wrong dxHotspot
                oldXOffset = 0;
        }
        else if (GetViewMode() == vmIcons || GetViewMode() == vmThumbnails)
        {
            // Icons || Thumbnails
            CIconSizeEnum iconSize = (GetViewMode() == vmIcons) ? ICONSIZE_32 : ICONSIZE_48;
            DrawIconThumbnailItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP, iconSize);
            DrawIconThumbnailItem(hMaskDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_MASK | DRAWFLAG_SKIP_VISTEST, iconSize);
            if (ContainsAlpha(lpBits, width, height))
            {
                // if the icon had an alpha channel, we will set the entire image as opaque and then redraw only the icon
                SetAlphaByMask(lpBits, width, height, hMaskDC);
                //        DrawIconThumbnailItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP | DRAWFLAG_ICON_ONLY | DRAWFLAG_SKIP_FRAME, iconSize);
            }
        }
        else if (GetViewMode() == vmTiles)
        {
            // Tiles
            CIconSizeEnum iconSize = ICONSIZE_48;
            DrawTileItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP, iconSize);
            DrawTileItem(hMaskDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_MASK | DRAWFLAG_SKIP_VISTEST, iconSize);
            if (ContainsAlpha(lpBits, width, height))
            {
                // if the icon had an alpha channel, we will set the entire image as opaque and then redraw only the icon
                SetAlphaByMask(lpBits, width, height, hMaskDC);
                //        DrawTileItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP | DRAWFLAG_ICON_ONLY, iconSize);
            }
        }
        RECT itemRect;
        ListBox->GetItemRect(itemIndex, &itemRect);
        dxHotspot = cursorX - itemRect.left + oldXOffset;
        dyHotspot = cursorY - itemRect.top;
    }
    else
    {
        dxHotspot = -15;
        dyHotspot = 0;
    }

    int bitsPerPixel = GetCurrentBPP(hMaskDC);
    if (bitsPerPixel <= 8)
    {
        // custom dither
        // It would be better to dither only the area of the icon/thumbnail (without disturbing the text)
        // for now I will take the whole picture
        SelectObject(hMaskDC, HDitherBrush);
        SetBkColor(hMaskDC, RGB(0, 0, 0));
        SetTextColor(hMaskDC, RGB(255, 255, 255));
        PatBlt(hMaskDC, 0, 0, width, height, 0x00AF0229); // Ternary Raster Operations 'ROP_DPno'
    }

    imgWidth = width;
    imgHeight = height;
    HIMAGELIST himl = ImageList_Create(width, height, GetImageListColorFlags() | ILC_MASK, 1, 0);

    /*    // debugging display of image+mask into main window
  HDC hWndDC = GetWindowDC(MainWindow->HWindow);
  BitBlt(hWndDC, 0, 0, r.right, r.bottom, hDC, 0, 0, SRCCOPY);
  BitBlt(hWndDC, 0, r.bottom, r.right, r.bottom, hMaskDC, 0, 0, SRCCOPY);
  ReleaseDC(MainWindow->HWindow, hWndDC);*/

    SelectObject(hMaskDC, hOldMaskBitmap);
    SelectObject(hDC, hOldBmp); // Release the bitmap before adding it to the imagelist
    ImageList_Add(himl, hBmp, hMaskBmp);

    HANDLES(DeleteDC(hDC));
    HANDLES(DeleteObject(hBmp));
    HANDLES(DeleteDC(hMaskDC));
    HANDLES(DeleteObject(hMaskBmp));
    return himl;
}

BOOL CopyUNCPathToClipboard(const char* path, const char* name, BOOL isDir, HWND hMessageParent, int nestingLevel)
{
    char buff[2 * MAX_PATH];
    char uncPath[2 * MAX_PATH];

    nestingLevel++; // attention, also serves to control the error message, we must increment here

    strcpy(buff, path);
    SalPathAddBackslash(buff, 2 * MAX_PATH);

    if (buff[0] == '\\' && buff[1] == '\\')
    {
        // the path is now in UNC format
        strcat(buff, name); // attach the name of the focused item
        return CopyTextToClipboard(buff);
    }

    // when it comes to the directory, we will append it to the path
    if (isDir)
        strcat(buff, name);

    // attempt to convert a local path to UNC

    // Let's look for shared directories to see if any of them are part of the path
    if (Shares.GetUNCPath(buff, uncPath, 2 * MAX_PATH))
    {
        if (!isDir)
        {
            // attach the file name
            SalPathAddBackslash(uncPath, 2 * MAX_PATH); // at the end we want a backslash
            strcat(uncPath, name);
        }
        return CopyTextToClipboard(uncPath);
    }

    // it could be a mapped disk
    char localRoot[3];
    localRoot[0] = buff[0];
    localRoot[1] = ':';
    localRoot[2] = 0;
    DWORD uncPathSize = sizeof(uncPath);
    if (WNetGetConnection(localRoot, uncPath, &uncPathSize) == NO_ERROR)
    {
        SalPathAddBackslash(uncPath, 2 * MAX_PATH);
        if (strlen(buff) > 3)
        {
            strcat(uncPath, buff + 3);
            SalPathAddBackslash(uncPath, 2 * MAX_PATH);
        }
        if (!isDir)
            strcat(uncPath, name);
        if (SalGetFullName(uncPath)) // root "c:\\", others without '\\' at the end
            return CopyTextToClipboard(uncPath);
    }

    // if it's not a UNC, it could be a SUBST drive
    // 10 dives should be enough even for mega slackers
    if (nestingLevel < 10 && buff[0] != '\\' && buff[1] == ':')
    {
        char target[MAX_PATH];
        if (GetSubstInformation(toupper(buff[0]) - 'A', target, MAX_PATH))
        {
            SalPathAddBackslash(target, MAX_PATH);
            strcat(target, path + 3);
            if (CopyUNCPathToClipboard(target, name, isDir, hMessageParent, nestingLevel))
                return TRUE;
        }
    }

    // Displays an error message only at the highest level
    if (nestingLevel == 1)
    {
        // Let's look for hidden shares to see if any of them are part of the path
        CShares allShares(FALSE);
        allShares.Refresh();
        if (allShares.GetUNCPath(buff, uncPath, 2 * MAX_PATH))
        {
            if (!isDir)
            {
                // attach the file name
                SalPathAddBackslash(uncPath, 2 * MAX_PATH); // at the end we want a backslash
                strcat(uncPath, name);
            }
            return CopyTextToClipboard(uncPath);
        }

        // all options failed -- we are giving up and printing an error message
        // Path cannot be converted to UNC, it is neither a shared nor a mapped disk
        strcpy(buff, path);
        SalPathAddBackslash(buff, 2 * MAX_PATH);
        strcat(buff, name);

        wsprintf(uncPath, LoadStr(IDS_CANNOT_CREATE_UNC_NAME), buff);
        SalMessageBox(hMessageParent, uncPath, LoadStr(IDS_INFOTITLE),
                      MB_OK | MB_ICONINFORMATION);
    }
    return FALSE;
}

BOOL CFilesWindow::CopyFocusedNameToClipboard(CCopyFocusedNameModeEnum mode)
{
    CALL_STACK_MESSAGE2("CFilesWindow::CopyFocusedNameToClipboard(%d)", mode);

    if (FocusedIndex == 0 && FocusedIndex < Dirs->Count &&
        strcmp(Dirs->At(0).Name, "..") == 0)
        return FALSE; // we don't take up-dir
    if (FocusedIndex < 0 || FocusedIndex >= Files->Count + Dirs->Count)
        return FALSE; // we do not take nonsensical index

    char buff[2 * MAX_PATH];
    buff[0] = 0;

    if (mode == cfnmUNC)
    {
        // full UNC name
        // attempt to convert a classic name to UNC format
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            // get the current path in the panel
            GetGeneralPath(buff, 2 * MAX_PATH);
            SalPathAddBackslash(buff, 2 * MAX_PATH);

            CFileData* item = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);
            char itemName[MAX_PATH];
            AlterFileName(itemName, item->Name, -1, Configuration.FileNameFormat, 0, FocusedIndex < Dirs->Count);

            if (CopyUNCPathToClipboard(buff, itemName, FocusedIndex < Dirs->Count, MainWindow->HWindow))
                return TRUE;
        }
        return FALSE;
    }

    if (mode == cfnmFull)
    {
        // full name
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            GetGeneralPath(buff, 2 * MAX_PATH);
            SalPathAddBackslash(buff, 2 * MAX_PATH);
        }
    }

    CFileData* file = (FocusedIndex < Dirs->Count) ? &Dirs->At(FocusedIndex) : &Files->At(FocusedIndex - Dirs->Count);
    if (Is(ptDisk) || Is(ptZIPArchive) || Is(ptPluginFS) && mode == cfnmShort)
    {
        char fileName[MAX_PATH];
        AlterFileName(fileName, file->Name, -1, Configuration.FileNameFormat, 0, FocusedIndex < Dirs->Count);
        int l = (int)strlen(buff);
        lstrcpyn(buff + l, fileName, 2 * MAX_PATH - l);
        return CopyTextToClipboard(buff);
    }
    else
    {
        if (mode == cfnmFull && Is(ptPluginFS) && GetPluginFS()->NotEmpty())
        {
            strcpy(buff, GetPluginFS()->GetPluginFSName());
            strcat(buff, ":");
            int l = (int)strlen(buff);
            if (GetPluginFS()->GetFullName(*file, FocusedIndex < Dirs->Count ? 1 : 0, buff + l, 2 * MAX_PATH - l))
            {
                GetPluginFS()->GetPluginInterfaceForFS()->ConvertPathToExternal(GetPluginFS()->GetPluginFSName(),
                                                                                GetPluginFS()->GetPluginFSNameIndex(),
                                                                                buff + l);
                return CopyTextToClipboard(buff);
            }
        }
    }
    return FALSE;
}

BOOL CFilesWindow::CopyCurrentPathToClipboard()
{
    CALL_STACK_MESSAGE1("CFilesWindow::CopyCurrentPathToClipboard()");

    char buff[2 * MAX_PATH];
    buff[0] = 0;
    GetGeneralPath(buff, 2 * MAX_PATH, TRUE);
    return CopyTextToClipboard(buff);
}

void AddStrToStr(char* dstStr, int dstBufSize, const char* srcStr)
{
    int l = (int)strlen(srcStr);
    int dstStrLen = (int)strlen(dstStr);
    if (dstStrLen + 1 + l + 1 > dstBufSize)
    {
        int half = dstBufSize / 2 - 1;
        if (l <= half)
            dstStrLen = dstBufSize - 2 - l;
        else
        {
            if (dstStrLen <= half)
                l = dstBufSize - 2 - dstStrLen;
            else
            {
                l = half;
                dstStrLen = half;
            }
        }
    }
    dstStr[dstStrLen] = 0;
    lstrcpyn(dstStr + dstStrLen + 1, srcStr, l + 1);
}

// prepare a template for the variable 'Columns'

BOOL CFilesWindow::BuildColumnsTemplate()
{
    CALL_STACK_MESSAGE1("CFilesWindow::BuildColumnsTemplate()");
    // empty the existing template
    ColumnsTemplate.DetachMembers();

    ColumnsTemplateIsForDisk = Is(ptDisk);

    if (ViewTemplate->Mode == VIEW_MODE_BRIEF)
    {
        // In brief mode, the headerline will not be displayed, but we will ensure the addition of a column
        // Name, for the tests like CFilesWindow::IsExtensionInSeparateColumn to work
        CColumn column;
        column.CustomData = 0;
        lstrcpy(column.Name, LoadStr(IDS_COLUMN_NAME_NAME));
        column.Name[strlen(column.Name) + 1] = 0; // two zeros at the end (the text "Ext" is behind the name, so that it doesn't mess up if it searches for it there)
        lstrcpy(column.Description, LoadStr(IDS_COLUMN_DESC_NAME));
        column.Description[strlen(column.Description) + 1] = 0; // two zeros at the end (the description "Ext" is after the name, so that it doesn't mess up if it looks for it there)
        column.GetText = NULL;
        column.SupportSorting = 1;
        column.LeftAlignment = 1;
        column.ID = COLUMN_ID_NAME;
        column.Width = 100;    // dummy
        column.FixedWidth = 0; // dummy
        column.MinWidth = 0;   // dummy

        ColumnsTemplate.Add(column);
        if (!ColumnsTemplate.IsGood())
        {
            ColumnsTemplate.ResetState();
            return FALSE;
        }
        return TRUE;
    }

    CColumn column;
    column.CustomData = 0;
    CColumDataItem* item;

    BOOL leftPanel = (this == MainWindow->LeftPanel);

    // select views matching the configuration array with templates
    CColumnConfig* colCfg = ViewTemplate->Columns;

    // adding visible columns according to the template
    int i;
    for (i = 0; i < STANDARD_COLUMNS_COUNT; i++)
    {
        item = GetStdColumn(i, Is(ptDisk));
        // column Name (i==0) is always visible
        if (i == 0 || ViewTemplate->Flags & item->Flag)
        {
            lstrcpy(column.Name, LoadStr(item->NameResID));
            lstrcpy(column.Description, LoadStr(item->DescResID));
            if (i == 0) // column "Name"
            {
                if ((ViewTemplate->Flags & VIEW_SHOW_EXTENSION) == 0) // "Ext" is part of the "Name" column, the name+description of the "Ext" column is after the terminating zero of the name+description
                {
                    AddStrToStr(column.Name, COLUMN_NAME_MAX, ColExtStr);
                    AddStrToStr(column.Description, COLUMN_DESCRIPTION_MAX, LoadStr(IDS_COLUMN_DESC_EXT));
                }
                else // Just for fun, let's make double-null-terminated strings
                {
                    column.Name[strlen(column.Name) + 1] = 0;
                    column.Description[strlen(column.Description) + 1] = 0;
                }
            }
            column.GetText = item->GetText;
            column.SupportSorting = item->SupportSorting;
            column.LeftAlignment = item->LeftAlignment;
            column.ID = item->ID;
            column.Width = leftPanel ? colCfg[i].LeftWidth : colCfg[i].RightWidth;
            column.FixedWidth = leftPanel ? colCfg[i].LeftFixedWidth : colCfg[i].RightFixedWidth;
            column.MinWidth = 0; // dummy - will be overwritten during HeaderLine dimensioning

            ColumnsTemplate.Add(column);
            if (!ColumnsTemplate.IsGood())
            {
                ColumnsTemplate.ResetState();
                return FALSE;
            }
        }
    }

    return TRUE;
}

BOOL CFilesWindow::CopyColumnsTemplateToColumns()
{
    CALL_STACK_MESSAGE1("CFilesWindow::CopyColumnsTemplateToColumns()");
    Columns.DetachMembers();
    int count = ColumnsTemplate.Count;
    if (count > 0)
    {
        Columns.Add(&ColumnsTemplate.At(0), count);
        if (!Columns.IsGood())
        {
            Columns.ResetState();
            return FALSE;
        }
    }
    return TRUE;
}

void CFilesWindow::DeleteColumnsWithoutData(DWORD columnValidMask)
{
    columnValidMask &= ValidFileData;
    int i;
    for (i = Columns.Count - 1; i >= 0; i--)
    {
        CColumn* c = &Columns[i];
        BOOL delColumn = FALSE;
        switch (c->ID)
        {
        case COLUMN_ID_NAME:
            break; // name is required (cannot be deleted)
        case COLUMN_ID_EXTENSION:
            delColumn = (columnValidMask & VALID_DATA_EXTENSION) == 0;
            break;
        case COLUMN_ID_DOSNAME:
            delColumn = (columnValidMask & VALID_DATA_DOSNAME) == 0;
            break;
        case COLUMN_ID_SIZE:
            delColumn = (columnValidMask & VALID_DATA_SIZE) == 0;
            break;
        case COLUMN_ID_TYPE:
            delColumn = (columnValidMask & VALID_DATA_TYPE) == 0;
            break;
        case COLUMN_ID_DATE:
            delColumn = (columnValidMask & VALID_DATA_DATE) == 0;
            break;
        case COLUMN_ID_TIME:
            delColumn = (columnValidMask & VALID_DATA_TIME) == 0;
            break;
        case COLUMN_ID_ATTRIBUTES:
            delColumn = (columnValidMask & VALID_DATA_ATTRIBUTES) == 0;
            break;
        case COLUMN_ID_DESCRIPTION:
            delColumn = TRUE;
            break; // description is not used yet
        }
        if (delColumn)
        {
            Columns.Delete(i);
            if (!Columns.IsGood())
                Columns.ResetState(); // cannot fail, only the array was not reduced
        }
    }
}

void CFilesWindow::OnHeaderLineColWidthChanged()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OnHeaderLineColWidthChanged()");
    // transfer data from the panel to the template
    BOOL pluginColMaybeChanged = FALSE;
    BOOL leftPanel = (this == MainWindow->LeftPanel);
    int i;
    for (i = 0; i < Columns.Count; i++)
    {
        CColumn* column = &Columns[i];
        int colIndex = -1;
        switch (column->ID)
        {
        case COLUMN_ID_NAME:
            colIndex = 0;
            break;
        case COLUMN_ID_EXTENSION:
            colIndex = 1;
            break;
        case COLUMN_ID_DOSNAME:
            colIndex = 2;
            break;
        case COLUMN_ID_SIZE:
            colIndex = 3;
            break;
        case COLUMN_ID_TYPE:
            colIndex = 4;
            break;
        case COLUMN_ID_DATE:
            colIndex = 5;
            break;
        case COLUMN_ID_TIME:
            colIndex = 6;
            break;
        case COLUMN_ID_ATTRIBUTES:
            colIndex = 7;
            break;
        case COLUMN_ID_DESCRIPTION:
            colIndex = 8;
            break;
        }
        if (column->ID == COLUMN_ID_CUSTOM) // It's about the column added by the plugin
        {
            if (column->FixedWidth && PluginData.NotEmpty()) // only non-elastic columns + "always true"
            {
                PluginData.ColumnWidthWasChanged(leftPanel, column, column->Width);
                pluginColMaybeChanged = TRUE;
            }
        }
        else
        {
            if (colIndex != -1) // "always true"
            {
                if (leftPanel)
                    ViewTemplate->Columns[colIndex].LeftWidth = column->Width;
                else
                    ViewTemplate->Columns[colIndex].RightWidth = column->Width;
            }
            else
                TRACE_E("CFilesWindow::OnHeaderLineColWidthChanged(): unexpected column-ID!");
        }
    }
    // we will create a new template (columns are already modified)
    BuildColumnsTemplate();
}

CHeaderLine*
CFilesWindow::GetHeaderLine()
{
    if (ListBox == NULL)
        return NULL;
    else
        return ListBox->GetHeaderLine();
}
