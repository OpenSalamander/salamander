// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <shlwapi.h>
#undef PathIsPrefix // jinak kolize s CSalamanderGeneral::PathIsPrefix

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

// !!! nepouzivat primo StdColumnsPrivate, pouzivat GetStdColumn()
CColumDataItem StdColumnsPrivate[STANDARD_COLUMNS_COUNT] =
    {
        // Flag,                  Name,                    Description,             GetText function,   Sort, Left, ID
        /*0*/ {0, IDS_COLUMN_NAME_NAME, IDS_COLUMN_DESC_NAME, NULL, 1, 1, COLUMN_ID_NAME},
        /*1*/ {VIEW_SHOW_EXTENSION, IDS_COLUMN_NAME_EXT, IDS_COLUMN_DESC_EXT, NULL, 1, 1, COLUMN_ID_EXTENSION},
        /*2*/ {VIEW_SHOW_DOSNAME, IDS_COLUMN_NAME_DOSNAME, IDS_COLUMN_DESC_DOSNAME, InternalGetDosName, 0, 1, COLUMN_ID_DOSNAME},
        /*3*/ {VIEW_SHOW_SIZE, IDS_COLUMN_NAME_SIZE, IDS_COLUMN_DESC_SIZE, InternalGetSize, 1, 0, COLUMN_ID_SIZE},
        /*4*/ {VIEW_SHOW_TYPE, IDS_COLUMN_NAME_TYPE, IDS_COLUMN_DESC_TYPE, InternalGetType, 0, 1, COLUMN_ID_TYPE},
        /*5*/ {VIEW_SHOW_DATE, IDS_COLUMN_NAME_DATE, IDS_COLUMN_DESC_DATE, NULL /* viz nize */, 1, 0, COLUMN_ID_DATE},
        /*6*/ {VIEW_SHOW_TIME, IDS_COLUMN_NAME_TIME, IDS_COLUMN_DESC_TIME, NULL /* viz nize */, 1, 0, COLUMN_ID_TIME},
        /*7*/ {VIEW_SHOW_ATTRIBUTES, IDS_COLUMN_NAME_ATTR, IDS_COLUMN_DESC_ATTR, InternalGetAttr, 1, 0, COLUMN_ID_ATTRIBUTES},
        /*8*/ {VIEW_SHOW_DESCRIPTION, IDS_COLUMN_NAME_DESC, IDS_COLUMN_DESC_DESC, InternalGetDescr, 0, 1, COLUMN_ID_DESCRIPTION},
};

CColumDataItem* GetStdColumn(int i, BOOL isDisk)
{
    if (i == 5 /* date */)
        StdColumnsPrivate[5].GetText = isDisk ? InternalGetDateOnlyForDisk : InternalGetDate; // na disku pouzivame 1.1.1602 0:00:00.000 jako prazdnou hodnotu (prazdne misto ve sloupci v panelu)
    else
    {
        if (i == 6 /* time */)
            StdColumnsPrivate[6].GetText = isDisk ? InternalGetTimeOnlyForDisk : InternalGetTime; // na disku pouzivame 1.1.1602 0:00:00.000 jako prazdnou hodnotu (prazdne misto ve sloupci v panelu)
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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
    BOOL files = FALSE;       // zjistime jestli data na clipboardu jsou vubec soubory
    BOOL filesOnClip = FALSE; // je na clipboardu neco naseho k "paste" souboru/adresaru?
                              //  TRACE_I("CFilesWindow::ClipboardPaste() called: " << (onlyLinks ? "links " : "") <<
                              //          (onlyTest ? "test " : "") << (pastePath != NULL ? pastePath : "(null)"));
    if (OleGetClipboard(&dataObj) == S_OK && dataObj != NULL)
    {
        if (!onlyLinks && !onlyTest && IsFakeDataObject(dataObj, NULL, NULL, 0) && SalShExtSharedMemView != NULL)
        { // Salamanderovsky "fake" data object na clipboardu -> paste souboru/adresaru z archivu
            BOOL pasteFromOurData = FALSE;
            WaitForSingleObject(SalShExtSharedMemMutex, INFINITE);
            if (SalShExtSharedMemView->DoPasteFromSalamander &&
                SalShExtSharedMemView->SalamanderMainWndPID == GetCurrentProcessId() &&
                SalShExtSharedMemView->SalamanderMainWndTID == GetCurrentThreadId() &&
                SalShExtSharedMemView->PastedDataID == SalShExtPastedData.GetDataID())
            {
                pasteFromOurData = TRUE;
                SalShExtSharedMemView->ClipDataObjLastGetDataTime = GetTickCount() - 60000; // pod W2K+ uz asi neni potreba: cas posledniho GetData() nastavime o minutu zpatky, aby pripadny nasledny Release data-objektu probehl hladce
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
                dataObj->Release(); // jeste zustava jedna instance na clipboardu
                pasteEffect &= DROPEFFECT_COPY | DROPEFFECT_MOVE;
                if (pasteEffect == DROPEFFECT_COPY || pasteEffect == DROPEFFECT_MOVE)
                {
                    // move po sobe cisti clipboard (cut&paste), clipboard uvolnime jeste pred spustenim
                    // samotne operace, aby ho mohly jine aplikace pouzivat pro dalsi prenosy dat
                    if (pasteEffect == DROPEFFECT_MOVE)
                        OleSetClipboard(NULL);

                    // provedeme samotny Paste
                    SalShExtPastedData.DoPasteOperation(pasteEffect == DROPEFFECT_COPY,
                                                        pastePath != NULL ? pastePath : GetPath());

                    FocusFirstNewItem = TRUE; // pokud to bude jeden novy soubor, at ho fokus najde
                    if (pasteEffect != DROPEFFECT_COPY)
                    {
                        IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                        IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard
                    }
                }
                else
                    TRACE_E("Paste: unexpected paste-effect: " << pasteEffect);

                SalShExtPastedData.SetLock(FALSE);
                PostMessage(MainWindow->HWindow, WM_USER_SALSHEXT_TRYRELDATA, 0, 0); // po odemceni pripadne provedeme uvolneni dat
                return TRUE;
            }
        }

        BOOL ownRutine = FALSE; // jsme to schopni udelat sami?
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
                    formatEtc.cfFormat == cfFileContent && !onlyLinks) // obsah souboru (uklada se pri Copy z Windows Mobile (WinCE)) - neumi delat shortcuty (na obsah souboru to neni dost dobre mozne)
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
        if (ownRutine) // pokud je nadeje na vlastni provedeni, zjistime jestli jde o copy nebo move
        {
            DWORD dropEffect = 0;
            UINT cfPrefDrop = RegisterClipboardFormat(CFSTR_PREFERREDDROPEFFECT);
            UINT cfSalDataObject = RegisterClipboardFormat(SALCF_IDATAOBJECT);
            if (onlyLinks || onlyTest)
                ownRutine = FALSE; // linky neumime a testu neni treba

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
                    ownRutine = FALSE; // pokud to neni nas IDataObject, neprovedeme vlastni operaci
                CloseClipboard();
            }
            else
                TRACE_E("OpenClipboard() has failed!");

            if (!onlyLinks && !onlyTest && ownRutine)
            {
                effect = (dropEffect & (DROPEFFECT_COPY | DROPEFFECT_MOVE));
                if (effect == 0)
                    ownRutine = FALSE; // ani copy, ani move - neumime nic jineho
            }
        }
        filesOnClip = files && ourClipDataObject;

        if (ownRutine) // provedeme vlastni rutinu - copy nebo move
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

                FocusFirstNewItem = TRUE; // pokud to bude jeden novy soubor, at ho fokus najde

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
                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard
                }

                // refresh panelu se provede v DropCopyMove
            }
        }
        else
        {
            if (files && !onlyTest) // provedeme paste/pastelink
            {
                //        MainWindow->ReleaseMenuNew();  // Windows nejsou staveny na vic kontextovych menu

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
                        PasteLinkIsRunning++; // radeji budu pocitat s konkurovanim si vice threadu

                    SafeInvokeCommand(menu, ici);

                    if (onlyLinks && PasteLinkIsRunning > 0)
                        PasteLinkIsRunning--;

                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

                    FocusFirstNewItem = TRUE; // pokud to bude jeden novy soubor, at ho fokus najde

                    menu->Release();
                    OurClipDataObject = FALSE;
                }

                //---  refresh neautomaticky refreshovanych adresaru
                // zdrojovy adresar (dulezity u "move") nezname, budeme spolehat na refresh pri
                // aktivaci hl. okna (u "copy" a "move" je, u "pastelink" neni);
                // diky spousteni operace v jinem threadu neni sance zajistit korektni refresh,
                // takze aspon jeden postneme, treba zabere...;
                // 1/10 sekundy na provedeni operace nebo aspon jeji casti
                Sleep(100);
                // ohlasime zmenu v cilovem adresari a v jeho podadresarich
                MainWindow->PostChangeOnPathNotification(GetPath(), TRUE);
                // postneme refreshe do obou panelu (pokud nejsou auto-refreshovane - tedy do panelu
                // s FS to nedojde)
                if (!MainWindow->LeftPanel->AutomaticRefresh || !MainWindow->RightPanel->AutomaticRefresh)
                {
                    // dalsi 1/3 sekundy na provedeni operace nebo aspon jeji casti; aspon jeden panel
                    // je nerefreshovany - "vyplati" se utracet cas usera
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
        if (IsSimpleSelection(dataObj, namesList)) // zjistime jestli jde o soubory/adresare z disku a zaroven jestli jsou z jedne cesty
        {
            ret = TRUE;
            // zjistime jestli jde o copy nebo move
            DWORD dropEffect = DROPEFFECT_COPY | DROPEFFECT_MOVE; // pokud nezjistime effect, bereme implicitne oba (prioritne se pouzije Copy)
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

            if (!onlyTest && namesList != NULL) // pokus nejde jen o test, provedeme pozadovanou operaci
            {
                BOOL moveOper = FALSE;
                if (Is(ptZIPArchive)) // Paste do archivu
                {
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0 &&                               // nasli jsme podporovany archiv
                        PackerFormatConfig.GetUsePacker(format - 1)) // ma edit?
                    {
                        if (dropEffect == (DROPEFFECT_COPY | DROPEFFECT_MOVE))
                            dropEffect = DROPEFFECT_COPY; // Copy ma prioritu (je bezpecnejsi)
                        if (dropEffect != 0)
                        {
                            DoDragDropOper(dropEffect == DROPEFFECT_COPY, TRUE, GetZIPArchive(),
                                           GetZIPPath(), namesList, this);
                            namesList = NULL;         // DoDragDropOper ho necha dealokovat, tady uz to delat nebudeme
                            FocusFirstNewItem = TRUE; // pokud to bude jeden novy soubor, at ho fokus najde
                            moveOper = dropEffect == DROPEFFECT_MOVE;
                        }
                        else
                            TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unexpected paste-effect!");
                    }
                }
                else
                {
                    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty()) // Paste na FS
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
                                namesList = NULL;         // DoDragDropOper ho necha dealokovat, tady uz to delat nebudeme
                                FocusFirstNewItem = TRUE; // pokud to bude jeden novy soubor, at ho fokus najde
                                moveOper = dropEffect == DROPEFFECT_MOVE;
                            }
                            else
                                TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unable to get current path from FS!");
                        }
                        else
                            TRACE_E("CFilesWindow::ClipboardPasteToArcOrFS(): unexpected paste-effect!");
                    }
                }
                if (moveOper) // Cut & Paste: musime vycistit clipboard
                {
                    if (OpenClipboard(HWindow))
                    {
                        EmptyClipboard();
                        CloseClipboard();
                    }
                    else
                        TRACE_E("OpenClipboard() has failed!");
                    IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
                    IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard
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
                Sleep(10); // pockame jestli se clipboard neuvolni (max. 100 ms)
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
        return TRUE; // zpracovani nechame na pluginu s FS

    // orizneme mezery na zacatku a na konci
    CutSpacesFromBothSides(buff);

    // orizneme uvozovky, viz https://forum.altap.cz/viewtopic.php?t=4160
    CutDoubleQuotesFromBothSides(buff);

    if (IsFileURLPath(buff)) // je to URL: prevedeme URL (file://) na wokeni cestu
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
            return TRUE; // zpracovani nechame na pluginu s FS
    }

    // expandujeme ENV promenne (na cetne zadosti na foru i pro nase potreby)
    // pokud ENV promenna neexistuje, dostane prilezitost stejnojmenny adresar
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
                    pathW++; // orizneme mezery+CR+LF pred cestou
                if (ConvertU2A(pathW, -1, buff, _countof(buff)))
                    changePath = TRUE;
                else
                {
                    if (buff[0] != 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER)
                        changePath = TRUE; // maly buffer, neresime (stejne jako ANSI varianta)
                }
                HANDLES(GlobalUnlock(handle));
            }
        }
        if (!changePath) // asi neni unicode, zkusime ANSI (kdyz unicode je, byva ANSI rozbite - bez diakritiky)
        {
            handle = GetClipboardData(CF_TEXT);
            char* path = (char*)HANDLES(GlobalLock(handle));
            if (path != NULL)
            {
                while (*path != 0 && *path <= ' ')
                    path++; // orizneme mezery+CR+LF pred cestou
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
        ChangeDir(buff); // zmenim cestu
}

void CFilesWindow::ChangeFilter(BOOL disable)
{
    CALL_STACK_MESSAGE1("CFilesWindow::ChangeFilter()");
    BeginStopRefresh(); // zadne refreshe nepotrebujeme
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
    KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow
    LButtonDown.x = LOWORD(lParam);
    LButtonDown.y = HIWORD(lParam);
    LButtonDownTime = GetTickCount();
    DragBoxLeft = 1;
    FocusedSinceClick = FALSE;

    BOOL noBeginDrag = FALSE; // user tahne levym tlacitkem a prida prave
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

    // kompatibilita s Windows Explorerem: kliknuti mimo (do aktivniho i neaktivniho panelu)
    // znamena potvrzeni prejmenovni; jine akce (Alt+Tab) povazujeme (narozdil od Explorera)
    // za storno prejmenovani
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
                        // skupinove odznaceni / oznaceni podle polozky sejmute pri stisku Shift
                        SetSelRange(SelectItems, first, last);
                    }
                    FocusedSinceClick = (index == FocusedIndex);
                    dontSetSince = TRUE;
                    SetCaretIndex(index, TRUE);
                    if (first != last)
                        RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                }
                else // ctrl+lbutton -> norml. sel.
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
                // hack pro UpDir, kde chceme umoznit tazeni klece (d&d stejne nema vyznam)
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
    KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow
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
    KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow
    BOOL selecting = ((GetKeyState(VK_CONTROL) & 0x8000) != 0) ^
                     (Configuration.PrimaryContextMenu == FALSE);

    BOOL noBeginDrag = FALSE; // user tahne levym tlacitkem a prida prave
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
                        // skupinove odznaceni / oznaceni podle prvni polozky
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
                    // hack pro UpDir, kde chceme umoznit tazeni klece (d&d stejne nema vyznam)
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
    KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow
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
        KillTimer(GetListBoxHWND(), IDT_SCROLL); // mohl by byt nahozeny
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

            if (GetSelCount() > 0) // existuje oznaceni?
            {
                if (index < Dirs->Count && !Dirs->At(index).Selected ||
                    index >= Dirs->Count && !Files->At(index - Dirs->Count).Selected)
                {                                                   // test jestli kliknul v oznaceni (jinak musime zrusit oznaceni)
                    SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                    PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                    UpdateWindow(MainWindow->HWindow);
                }
            }

            UserWorkedOnThisPath = TRUE;
            ShellAction(this, saContextMenu);
        }
        else
        {
            if (GetSelCount() > 0) // musime zrusit oznaceni (toto menu je pro panel - kliknuti za polozkami)
            {
                SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                UpdateWindow(MainWindow->HWindow);
            }

            UserWorkedOnThisPath = TRUE;
            ShellAction(this, saContextMenu, FALSE, TRUE, TRUE); // menu jen pro panel (kliknuti za polozkami)
        }
        *lResult = 0;
        return TRUE; // nechceme nechat nagenerovat WM_CONTEXTMENU
    }
    return FALSE;
}

BOOL CFilesWindow::IsDragDropSafe(int x, int y)
{
    // uzivatel nechce nas bezpecnejsi drag-and-drop
    if (!Configuration.UseDragDropMinTime)
        return TRUE;

    // povolime vytazeni mimo hlavni okno pro rychle dropnuti souboru
    POINT pt;
    pt.x = x;
    pt.y = y;
    ClientToScreen(HWindow, &pt);
    HWND hWndHit = WindowFromPoint(pt);
    HWND hTopHit = GetTopLevelParent(hWndHit);
    if (hWndHit != NULL && hTopHit != NULL && hTopHit != MainWindow->HWindow)
        return TRUE;

    // od zacatku tazeni ubehlo dost casu
    if ((int)(GetTickCount() - LButtonDownTime) > Configuration.DragDropMinTime)
        return TRUE;

    return FALSE;
}

void CFilesWindow::OfferArchiveUpdateIfNeededAux(HWND parent, int textID, BOOL* archMaybeUpdated)
{
    *archMaybeUpdated = FALSE;
    if (AssocUsed) // pokud user editoval soubory z archivu, musime je pred operaci s archivem updatnout, jinak bysme makali se starymi verzemi editovanych souboru, ktere jsou ulozene primo v archivu
    {
        // zobrazime info o nutnosti updatu archivu, ve kterem jsou editovane soubory
        char text[MAX_PATH + 500];
        sprintf(text, LoadStr(textID), GetZIPArchive());
        SalMessageBox(parent, text, LoadStr(IDS_INFOTITLE),
                      MSGBOXEX_OK | MSGBOXEX_ICONINFORMATION | MSGBOXEX_SILENT);
        // zapakujeme zmenene soubory, pripravime pro dalsi pouziti
        BOOL someFilesChanged;
        UnpackedAssocFiles.CheckAndPackAndClear(parent, &someFilesChanged, archMaybeUpdated);
        SetEvent(ExecuteAssocEvent); // spustime uklid souboru
        DiskCache.WaitForIdle();
        ResetEvent(ExecuteAssocEvent); // ukoncime uklid souboru

        // pokud muzou byt v diskcache editovane soubory, vyhodime je, radsi at
        // se pri pristupu na ne znovu vybali
        if (someFilesChanged)
        {
            char buf[MAX_PATH];
            StrICpy(buf, GetZIPArchive()); // v disk-cache je jmeno archivu malymi pismeny (umozni case-insensitive porovnani jmena z Windows file systemu)
            DiskCache.FlushCache(buf);
        }
        AssocUsed = FALSE;
    }
}

void CFilesWindow::OfferArchiveUpdateIfNeeded(HWND parent, int textID, BOOL* archMaybeUpdated)
{
    BeginStopRefresh(); // zadne refreshe nepotrebujeme

    OfferArchiveUpdateIfNeededAux(parent, textID, archMaybeUpdated);

    CFilesWindow* otherPanel = MainWindow->LeftPanel == this ? MainWindow->RightPanel : MainWindow->LeftPanel;
    BOOL otherPanelArchMaybeUpdated = FALSE;
    if (otherPanel->Is(ptZIPArchive) && StrICmp(GetZIPArchive(), otherPanel->GetZIPArchive()) == 0)
    { // stejny archiv je i v druhem panelu, musime provest i jeho update
        otherPanel->OfferArchiveUpdateIfNeededAux(parent, textID, &otherPanelArchMaybeUpdated);
        if (otherPanelArchMaybeUpdated)
            *archMaybeUpdated = TRUE;
    }
    if (*archMaybeUpdated)
    {
        // trochu prasarna: user dal napr. F5 a my mu misto toho forcneme update archivu a jeho
        // refresh v panelu ... F5 pak musi dat znovu (dalo by se to resit PostMessage(uMsg,
        // wParam, lParam), ale at si radsi zkontroluje, co ma oznaceno v panelu a pak da teprve
        // znovu F5) ... je to hodne okrajova vec, tak snad to bude takhle stacit
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
    if (BeginDragDrop && (wParam & (MK_LBUTTON | MK_RBUTTON))) // "trhani" pro drag&drop
    {
        int x = abs(LButtonDown.x - (short)LOWORD(lParam));
        int y = abs(LButtonDown.y - (short)HIWORD(lParam));
        if (x > GetSystemMetrics(SM_CXDRAG) || y > GetSystemMetrics(SM_CYDRAG)) // posun
        {
            // podminka "!PerformingDragDrop" pridana po AS 2.52 kvuli stavu reportenemu od ehtera:
            // v adresari je treba mit dva soubory (jedno EXE stazene pomoci IE, aby obsahovalo security ADS) a nejaky druhy soubor
            // ten druhy soubor se pres d&d natahne na EXE, cimz vyskoci security okno, ktere je vsak NEMODALNI a ma message loop
            // takze nam hlavni vlakno Salamandera uvazne v ShellAction(); potom druhy drag&drop vedl na pad
            // pomoci tohoto patche novy d&d zakazeme, stejne jako to dela Windows Explorer
            // reseni to neni ciste, protoze Salamander bezi pouze zdanlive (hlavni vlakno neni v nasi aplikacni smycce)
            if (!PerformingDragDrop)
            {
                if (Is(ptZIPArchive) && AssocUsed) // pokud user editoval soubory z archivu, musime je pred operaci s archivem updatnout, jinak bysme makali se starymi verzemi editovanych souboru, ktere jsou ulozene primo v archivu
                {
                    BOOL archMaybeUpdated;
                    OfferArchiveUpdateIfNeeded(MainWindow->HWindow, IDS_ARCHIVECLOSEEDIT3, &archMaybeUpdated);
                }
                else
                {
                    if (IsDragDropSafe((short)LOWORD(lParam), (short)HIWORD(lParam))) // min. cas
                    {
                        if (TrackingSingleClick)
                        {
                            TrackingSingleClick = 0;
                            SetSingleClickIndex(-1);
                        }
                        BeginDragDrop = 0;
                        ReleaseCapture();
                        KillTimer(GetListBoxHWND(), IDT_DRAGDROPTESTAGAIN);

                        // pokud jsou oznacene urcite polozky a trhame za nekterou z neoznacenych,
                        // sestrelime select
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

                        IdleRefreshStates = TRUE; // pri pristim Idle vynutime kontrolu stavovych promennych
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

    if (BeginBoxSelect && (wParam & (MK_LBUTTON | MK_RBUTTON))) // "otevirame" selection box
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
                    // odchytim oznacovani
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
                                // skupinove odznaceni / oznaceni podle polozky sejmute pri stisku Shift
                                SetSelRange(SelectItems, first, last);
                                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0);
                            }
                            SetCaretIndex(last, TRUE);
                            if (first != last)
                                RepaintListBox(DRAWFLAG_DIRTY_ONLY | DRAWFLAG_SKIP_VISTEST);
                        }
                        else // ctrl+lbutton -> norml. sel.
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
        KillQuickRenameTimer(); // zamezime pripadnemu otevreni QuickRenameWindow
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

// vraci TRUE pokud DIB obsahuje nejaky alfa bajt jiny nez 0
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

// prekopiruje masku do DIBu, aby bylo mozne najit pruhledna/nepruhledna mista a nastavi podle toho alfa kanal
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
        width = Columns[0].Width; // pokud je sloupec zkraceny, nechceme aby do tazeneho obrazku propadly dalsi sloupce

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

    // vytvorim bitmapu s maskou
    HBITMAP hMaskBmp = HANDLES(CreateBitmap(width, height, 1, 1, NULL));

    HDC hMaskDC = HANDLES(CreateCompatibleDC(NULL));
    HBITMAP hOldMaskBitmap = (HBITMAP)SelectObject(hMaskDC, hMaskBmp);

    // podmazu pozadi
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
            ListBox->XOffset = 0; // ovlivnuje posunuti polozky v detailed rezimu
            DrawBriefDetailedItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP);
            DrawBriefDetailedItem(hMaskDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_MASK | DRAWFLAG_SKIP_VISTEST);
            if (ContainsAlpha(lpBits, width, height))
            {
                // pokud byla ikonka s alpha kanalem, nechame cely obrazek nastavit jako nepruhledny a nasledne prekreslime pouze ikonku
                SetAlphaByMask(lpBits, width, height, hMaskDC);
                //        zatim nedoresene, alpha-kanal body budou mit bile otrepy
                //        problem je v tom, ze nektera data (napriklad stare ikonky nebo thumbnaily) neprichazeji s alpha kanalem
                //        a budou tak zcela pruhledne
                //        DrawBriefDetailedItem(hDC, itemIndex, &r, DRAWFLAG_NO_FRAME | DRAWFLAG_NO_STATE | DRAWFLAG_SKIP_VISTEST | DRAWFLAG_DRAGDROP | DRAWFLAG_ICON_ONLY);
            }
            ListBox->XOffset = oldXOffset;
            if (GetViewMode() == vmBrief) // nevim, co je v brief v XOffsetu, tak sep ojistime, aby nebyl spatny dxHotspot
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
                // pokud byla ikonka s alpha kanalem, nechame cely obrazek nastavit jako nepruhledny a nasledne prekreslime pouze ikonku
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
                // pokud byla ikonka s alpha kanalem, nechame cely obrazek nastavit jako nepruhledny a nasledne prekreslime pouze ikonku
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
        // vlastni dither
        // lepsi by bylo ditherovat pouze oblast ikonky/thumbnailu (neporusit text)
        // zatim vemu obrazek cely
        SelectObject(hMaskDC, HDitherBrush);
        SetBkColor(hMaskDC, RGB(0, 0, 0));
        SetTextColor(hMaskDC, RGB(255, 255, 255));
        PatBlt(hMaskDC, 0, 0, width, height, 0x00AF0229); // Ternary Raster Operations 'ROP_DPno'
    }

    imgWidth = width;
    imgHeight = height;
    HIMAGELIST himl = ImageList_Create(width, height, GetImageListColorFlags() | ILC_MASK, 1, 0);

    /*
  // ladici zobrazeni obrazku+masky do main window
  HDC hWndDC = GetWindowDC(MainWindow->HWindow);
  BitBlt(hWndDC, 0, 0, r.right, r.bottom, hDC, 0, 0, SRCCOPY);
  BitBlt(hWndDC, 0, r.bottom, r.right, r.bottom, hMaskDC, 0, 0, SRCCOPY);
  ReleaseDC(MainWindow->HWindow, hWndDC);
*/

    SelectObject(hMaskDC, hOldMaskBitmap);
    SelectObject(hDC, hOldBmp); // uvolnim bitmapu pred pridani do imagelistu
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

    nestingLevel++; // pozor, zaroven slouzi k ovladani chybove hlasky, musime inkrementovat zde

    strcpy(buff, path);
    SalPathAddBackslash(buff, 2 * MAX_PATH);

    if (buff[0] == '\\' && buff[1] == '\\')
    {
        // cesta uz je v UNC tvaru
        strcat(buff, name); // pripojime nazev focusene polozky
        return CopyTextToClipboard(buff);
    }

    // pokud jde o adresar, pripojime jej k ceste
    if (isDir)
        strcat(buff, name);

    // pokusime se prevest lokalni cestu na UNC

    // podivame se po sdilenych adresari, zda nektery z nich neni soucasti cesty
    if (Shares.GetUNCPath(buff, uncPath, 2 * MAX_PATH))
    {
        if (!isDir)
        {
            // pripojime nazev souboru
            SalPathAddBackslash(uncPath, 2 * MAX_PATH); // na konci chceme backslash
            strcat(uncPath, name);
        }
        return CopyTextToClipboard(uncPath);
    }

    // mohlo by se jednat o mapovany disk
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
        if (SalGetFullName(uncPath)) // root "c:\\", ostatni bez '\\' na konci
            return CopyTextToClipboard(uncPath);
    }

    // pokud nejde o UNC, mohlo by se jednat o SUBST drive
    // 10 zanoreni musi stacit i pro mega uchylaky
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

    // pouze na nejvyssi levelu zobrazuje chybovou hlasku
    if (nestingLevel == 1)
    {
        // podivame se po hidden sharech, zda nektery z nich neni soucasti cesty
        CShares allShares(FALSE);
        allShares.Refresh();
        if (allShares.GetUNCPath(buff, uncPath, 2 * MAX_PATH))
        {
            if (!isDir)
            {
                // pripojime nazev souboru
                SalPathAddBackslash(uncPath, 2 * MAX_PATH); // na konci chceme backslash
                strcat(uncPath, name);
            }
            return CopyTextToClipboard(uncPath);
        }

        // selhaly vsechny moznosti -- vzdavame to a vypisujeme chybovou hlasku
        // cesta nemuze byt konvertovana do UNC, nejedna se ani o sdileny ani o mapovany disk
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
        return FALSE; // up-dir nebereme
    if (FocusedIndex < 0 || FocusedIndex >= Files->Count + Dirs->Count)
        return FALSE; // nesmyslny index nebereme

    char buff[2 * MAX_PATH];
    buff[0] = 0;

    if (mode == cfnmUNC)
    {
        // full UNC name
        // pokusime se prevest klasicke jmeno na UNC tvar
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            // vytahneme aktualni cestu v panelu
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

// pripravime predlohu pro promennou 'Columns'

BOOL CFilesWindow::BuildColumnsTemplate()
{
    CALL_STACK_MESSAGE1("CFilesWindow::BuildColumnsTemplate()");
    // vyprazdnime existujici predlohu
    ColumnsTemplate.DetachMembers();

    ColumnsTemplateIsForDisk = Is(ptDisk);

    if (ViewTemplate->Mode == VIEW_MODE_BRIEF)
    {
        // v brief modu sice nebude zobrazena headerline, ale zajistime pridani sloupce
        // Name, aby fungovaly testy typu CFilesWindow::IsExtensionInSeparateColumn
        CColumn column;
        column.CustomData = 0;
        lstrcpy(column.Name, LoadStr(IDS_COLUMN_NAME_NAME));
        column.Name[strlen(column.Name) + 1] = 0; // dve nuly na konci (text "Ext" je za jmenem, tak at se to nepodela, kdyby ho to tam hledalo)
        lstrcpy(column.Description, LoadStr(IDS_COLUMN_DESC_NAME));
        column.Description[strlen(column.Description) + 1] = 0; // dve nuly na konci (popis "Ext" je za jmenem, tak at se to nepodela, kdyby ho to tam hledalo)
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

    // vyberu s sablony pohledy odpovidajici konfiguracni pole
    CColumnConfig* colCfg = ViewTemplate->Columns;

    // napridavam viditelne sloupce podle sablony
    int i;
    for (i = 0; i < STANDARD_COLUMNS_COUNT; i++)
    {
        item = GetStdColumn(i, Is(ptDisk));
        // sloupec Name (i==0) je vzdy viditelny
        if (i == 0 || ViewTemplate->Flags & item->Flag)
        {
            lstrcpy(column.Name, LoadStr(item->NameResID));
            lstrcpy(column.Description, LoadStr(item->DescResID));
            if (i == 0) // sloupec "Name"
            {
                if ((ViewTemplate->Flags & VIEW_SHOW_EXTENSION) == 0) // "Ext" je soucasti sloupce "Name", jmeno+popis sloupce "Ext" je za terminujici nulou jmena+popisu
                {
                    AddStrToStr(column.Name, COLUMN_NAME_MAX, ColExtStr);
                    AddStrToStr(column.Description, COLUMN_DESCRIPTION_MAX, LoadStr(IDS_COLUMN_DESC_EXT));
                }
                else // jen tak pro sychr udelame stringy double-null-terminated
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
            column.MinWidth = 0; // dummy - bude prepsana pri dimenzovani HeaderLine

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
            break; // jmeno je povinne (nelze ho smazat)
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
            break; // description se zatim nepouziva
        }
        if (delColumn)
        {
            Columns.Delete(i);
            if (!Columns.IsGood())
                Columns.ResetState(); // nemuze failnout, jen se neredukovalo pole
        }
    }
}

void CFilesWindow::OnHeaderLineColWidthChanged()
{
    CALL_STACK_MESSAGE1("CFilesWindow::OnHeaderLineColWidthChanged()");
    // prenesu data z panelu do sablony
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
        if (column->ID == COLUMN_ID_CUSTOM) // jde o sloupec pridany pluginem
        {
            if (column->FixedWidth && PluginData.NotEmpty()) // jen neelasticke sloupce + "always true"
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
    // nechame vytvorit novou predlohu (sloupce uz jsou modifikovany)
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
