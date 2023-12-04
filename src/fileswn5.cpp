// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "cfgdlg.h"
#include "mainwnd.h"
#include "plugins.h"
#include "fileswnd.h"
#include "filesbox.h"
#include "dialogs.h"
#include "snooper.h"
#include "worker.h"
#include "cache.h"
#include "usermenu.h"
#include "execute.h"
#include "pack.h"
#include "viewer.h"
#include "codetbl.h"
#include "find.h"
#include "menu.h"

//
// ****************************************************************************
// CFilesWindow
//

void CFilesWindow::Convert()
{
    CALL_STACK_MESSAGE1("CFilesWindow::Convert()");
    if (Dirs->Count + Files->Count == 0)
        return;
    BeginStopRefresh(); // cmuchal si da pohov

    if (!FilesActionInProgress)
    {
        FilesActionInProgress = TRUE;

        BOOL subDir;
        if (Dirs->Count > 0)
            subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
        else
            subDir = FALSE;

        CConvertFilesDlg convertDlg(HWindow, SelectionContainsDirectory());
        while (1)
        {
            if (convertDlg.Execute() == IDOK)
            {
                UpdateWindow(MainWindow->HWindow);
                if (convertDlg.CodeType == 0 && convertDlg.EOFType == 0)
                    break; // neni co delat

                CCriteriaData filter;
                filter.UseMasks = TRUE;
                filter.Masks.SetMasksString(convertDlg.Mask);
                int errpos = 0;
                if (!filter.Masks.PrepareMasks(errpos))
                    break; // chybna maska

                if (CheckPath(TRUE) != ERROR_SUCCESS) // slozila se cesta, na ktere mame pracovat
                {
                    FilesActionInProgress = FALSE;
                    EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    return;
                }

                CConvertData dlgData;

                dlgData.EOFType = convertDlg.EOFType;

                // inicializace objektu CodeTables probehla jiz v dialogu Convert
                if (!CodeTables.GetCode(dlgData.CodeTable, convertDlg.CodeType))
                {
                    // pokud se nepodari ziskat kodovaci tabulku nebo neni zadne kodovani vybrano,
                    // provedeme kodovani jedna ku jedne - tedy zadne
                    int i;
                    for (i = 0; i < 256; i++)
                        dlgData.CodeTable[i] = i;
                }

                //---  zjisteni jake adresare a soubory jsou oznacene
                int* indexes = NULL;
                CFileData* f = NULL;
                int count = GetSelCount();
                if (count != 0)
                {
                    indexes = new int[count];
                    if (indexes == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;
                    }
                    GetSelItems(count, indexes);
                }
                else // nic neni oznaceno
                {
                    int i = GetCaretIndex();
                    if (i < 0 || i >= Dirs->Count + Files->Count)
                    {
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;           // spatny index (zadne soubory)
                    }
                    if (i == 0 && subDir)
                    {
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;           // se ".." nepracujem
                    }
                    f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                }
                //---
                COperations* script = new COperations(1000, 500, NULL, NULL, NULL);
                if (script == NULL)
                    TRACE_E(LOW_MEMORY);
                else
                {
                    HWND hFocusedWnd = GetFocus();
                    CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
                    EnableWindow(MainWindow->HWindow, FALSE);

                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    BOOL res = BuildScriptMain(script, convertDlg.SubDirs ? atRecursiveConvert : atConvert,
                                               NULL, NULL, count, indexes, f, NULL, NULL, FALSE, &filter);
                    if (script->Count == 0)
                        res = FALSE;
                    // prohozeno kvuli umozneni aktivace hlavniho okna (nesmi byt disable), jinak prepina do jine app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();

                    // pokud je aktivni Salamander, zavolame SetFocus na zapamatovane okno (SetFocus nefunguje
                    // pokud je hl. okno disablovane - po deaktivaci/aktivaci disablovaneho hl. okna aktivni panel
                    // nema fokus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    // pripravime refresh neautomaticky refreshovanych adresaru
                    // zmena v adresari zobrazenem v panelu a pokud se pracovalo i v podadresarich, tak i v podadresarich
                    script->SetWorkPath1(GetPath(), convertDlg.SubDirs);

                    if (!res || !StartProgressDialog(script, LoadStr(IDS_CONVERTTITLE), NULL, &dlgData))
                    {
                        if (script->IsGood() && script->Count == 0)
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_NOFILE_MATCHEDMASK), LoadStr(IDS_INFOTITLE),
                                          MB_OK | MB_ICONINFORMATION);

                            // zadny ze souboru nemusel projit pres mask filtr
                            SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        }
                        UpdateWindow(MainWindow->HWindow);
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    else // odstraneni sel. indexu (necekame az operace dobehne, bezi v jinem threadu)
                    {
                        SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        UpdateWindow(MainWindow->HWindow);
                    }
                }
                //---
                if (indexes != NULL)
                    delete[] (indexes);
            }
            UpdateWindow(MainWindow->HWindow);
            break;
        }
        FilesActionInProgress = FALSE;
    }
    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::ChangeAttr(BOOL setCompress, BOOL compressed, BOOL setEncryption, BOOL encrypted)
{
    CALL_STACK_MESSAGE5("CFilesWindow::ChangeAttr(%d, %d, %d, %d)", setCompress, compressed, setEncryption, encrypted);
    if (Dirs->Count + Files->Count == 0)
        return;
    int selectedCount = GetSelCount();
    if (selectedCount == 0 || selectedCount == 1)
    {
        int index;
        if (selectedCount == 0)
            index = GetCaretIndex();
        else
            GetSelItems(1, &index);
        // focus na UpDiru -- neni co convertit
        if (Dirs->Count > 0 && index == 0 && strcmp(Dirs->At(0).Name, "..") == 0)
            return;
    }
    BeginStopRefresh(); // cmuchal si da pohov

    // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
    char temporarySelected[MAX_PATH];
    temporarySelected[0] = 0;
    if ((!setCompress || Configuration.CnfrmNTFSPress) &&
        (!setEncryption || Configuration.CnfrmNTFSCrypt))
    {
        SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);
    }

    if (Is(ptDisk))
    {
        if (!FilesActionInProgress)
        {
            FilesActionInProgress = TRUE;

            BOOL subDir;
            if (Dirs->Count > 0)
                subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
            else
                subDir = FALSE;

            DWORD attr, attrDiff;
            SYSTEMTIME timeModified;
            SYSTEMTIME timeCreated;
            SYSTEMTIME timeAccessed;
            if (!setCompress && !setEncryption)
            {
                int count = GetSelCount();
                if (count == 1 || count == 0)
                {
                    int index;
                    if (count == 0)
                        index = GetCaretIndex();
                    else
                        GetSelItems(1, &index);
                    if (index >= 0 && index < Files->Count + Dirs->Count)
                    {
                        CFileData* f = (index < Dirs->Count) ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                        if (strcmp(f->Name, "..") != 0)
                        {
                            BOOL isDir = index < Dirs->Count;

                            BOOL timeObtained = FALSE;

                            // zjistime casy souboru
                            char fileName[MAX_PATH];
                            strcpy(fileName, GetPath());
                            SalPathAppend(fileName, f->Name, MAX_PATH);

                            WIN32_FIND_DATA find;
                            HANDLE hFind = HANDLES_Q(FindFirstFile(fileName, &find));
                            if (hFind != INVALID_HANDLE_VALUE)
                            {
                                HANDLES(FindClose(hFind));

                                FILETIME ft;
                                if (FileTimeToLocalFileTime(&find.ftCreationTime, &ft) &&
                                    FileTimeToSystemTime(&ft, &timeCreated) &&
                                    FileTimeToLocalFileTime(&find.ftLastAccessTime, &ft) &&
                                    FileTimeToSystemTime(&ft, &timeAccessed) &&
                                    FileTimeToLocalFileTime(&find.ftLastWriteTime, &ft) &&
                                    FileTimeToSystemTime(&ft, &timeModified))
                                {
                                    timeObtained = TRUE;
                                }
                            }
                            if (!timeObtained)
                            {
                                // pokud se nam nepodarilo vytahnout cas ze souboru, predhodime alespon ten co mame
                                FILETIME ft;
                                if (!FileTimeToLocalFileTime(&f->LastWrite, &ft) ||
                                    !FileTimeToSystemTime(&ft, &timeModified))
                                {
                                    GetLocalTime(&timeModified); // cas ktery mame je invalidni, bereme aktualni cas
                                }
                                timeCreated = timeModified;
                                timeAccessed = timeModified;
                            }

                            attr = f->Attr;
                            attrDiff = 0;
                            count = -1;
                        }
                    }
                }
                if (count != -1)
                {
                    GetLocalTime(&timeModified);
                    timeAccessed = timeModified;
                    timeCreated = timeModified;
                    attr = 0;
                    attrDiff = 0;
                    BOOL first = TRUE;

                    int totalCount = Dirs->Count + Files->Count;
                    CFileData* f;
                    int i;
                    for (i = 0; i < totalCount; i++)
                    {
                        BOOL isDir = i < Dirs->Count;
                        f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                        if (i == 0 && isDir && strcmp(Dirs->At(0).Name, "..") == 0)
                            continue;
                        if (f->Selected == 1)
                        {
                            if (first)
                            {
                                attr = f->Attr;
                                first = FALSE;
                            }
                            else
                            {
                                if (f->Attr != attr)
                                    attrDiff |= f->Attr ^ attr;
                            }
                        }
                    }
                }
            }

            CChangeAttrDialog chDlg(HWindow, attr, attrDiff,
                                    SelectionContainsDirectory(), FileBasedCompression,
                                    FileBasedEncryption,
                                    &timeModified, &timeCreated, &timeAccessed);
            if (setCompress || setEncryption)
            {
                chDlg.Archive = 2;
                chDlg.ReadOnly = 2;
                chDlg.Hidden = 2;
                chDlg.System = 2;
                if (setCompress)
                {
                    chDlg.Compressed = compressed;
                    chDlg.Encrypted = compressed ? 0 : 2; // komprese vylucuje sifrovani : bez komprese muze sifrovani zustat jak je
                }
                else
                {
                    chDlg.Compressed = encrypted ? 0 : 2; // sifrovani vylucuje kompresi : bez sifrovani muze komprese zustat jak je
                    chDlg.Encrypted = encrypted;
                }
                chDlg.ChangeTimeModified = FALSE;
                chDlg.ChangeTimeCreated = FALSE;
                chDlg.ChangeTimeAccessed = FALSE;
                chDlg.RecurseSubDirs = TRUE;

                if (setCompress && Configuration.CnfrmNTFSPress || // zeptame se jestli kompresi/dekompresi provest
                    setEncryption && Configuration.CnfrmNTFSCrypt) // zeptame se jestli sifrovani/desifrovani provest
                {
                    char subject[MAX_PATH + 100];
                    char expanded[200];
                    int count = GetSelCount();
                    char path[MAX_PATH];
                    if (count > 1)
                    {
                        int totalCount = Dirs->Count + Files->Count;
                        int files = 0;
                        int dirs = 0;
                        CFileData* f;
                        int i;
                        for (i = 0; i < totalCount; i++)
                        {
                            BOOL isDir = i < Dirs->Count;
                            f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
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

                        ExpandPluralFilesDirs(expanded, 200, files, dirs, epfdmNormal, FALSE);
                    }
                    else
                    {
                        int index;
                        if (count == 0)
                            index = GetCaretIndex();
                        else
                            GetSelItems(1, &index);

                        BOOL isDir = index < Dirs->Count;
                        CFileData* f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);
                        AlterFileName(path, f->Name, -1, Configuration.FileNameFormat, 0, index < Dirs->Count);
                        lstrcpy(expanded, LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
                    }
                    int resTextID;
                    int resTitleID;
                    if (setCompress)
                    {
                        resTextID = compressed ? IDS_CONFIRM_NTFSCOMPRESS : IDS_CONFIRM_NTFSUNCOMPRESS;
                        resTitleID = compressed ? IDS_CONFIRM_NTFSCOMPRESS_TITLE : IDS_CONFIRM_NTFSUNCOMPRESS_TITLE;
                    }
                    else
                    {
                        resTextID = encrypted ? IDS_CONFIRM_NTFSENCRYPT : IDS_CONFIRM_NTFSDECRYPT;
                        resTitleID = encrypted ? IDS_CONFIRM_NTFSENCRYPT_TITLE : IDS_CONFIRM_NTFSDECRYPT_TITLE;
                    }
                    sprintf(subject, LoadStr(resTextID), expanded);
                    CTruncatedString str;
                    str.Set(subject, count > 1 ? NULL : path);
                    CMessageBox msgBox(HWindow, MSGBOXEX_YESNO | MSGBOXEX_ESCAPEENABLED | MSGBOXEX_ICONQUESTION | MSGBOXEX_SILENT,
                                       LoadStr(resTitleID), &str, NULL, NULL, NULL, 0, NULL, NULL, NULL, NULL);
                    if (msgBox.Execute() != IDYES)
                    {
                        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;
                    }
                    UpdateWindow(MainWindow->HWindow);
                }
            }
            if (setCompress || setEncryption || chDlg.Execute() == IDOK)
            {
                UpdateWindow(MainWindow->HWindow);

                if (CheckPath(TRUE) != ERROR_SUCCESS)
                {
                    // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                    UnselectItemWithName(temporarySelected);
                    FilesActionInProgress = FALSE;
                    EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    return;
                }

                CChangeAttrsData dlgData;
                dlgData.ChangeTimeModified = chDlg.ChangeTimeModified;
                if (dlgData.ChangeTimeModified)
                {
                    FILETIME ft;
                    SystemTimeToFileTime(&chDlg.TimeModified, &ft);
                    LocalFileTimeToFileTime(&ft, &dlgData.TimeModified);
                }
                dlgData.ChangeTimeCreated = chDlg.ChangeTimeCreated;
                if (dlgData.ChangeTimeCreated)
                {
                    FILETIME ft;
                    SystemTimeToFileTime(&chDlg.TimeCreated, &ft);
                    LocalFileTimeToFileTime(&ft, &dlgData.TimeCreated);
                }
                dlgData.ChangeTimeAccessed = chDlg.ChangeTimeAccessed;
                if (dlgData.ChangeTimeAccessed)
                {
                    FILETIME ft;
                    SystemTimeToFileTime(&chDlg.TimeAccessed, &ft);
                    LocalFileTimeToFileTime(&ft, &dlgData.TimeAccessed);
                }
                //---  zjisteni jake adresare a soubory jsou oznacene
                int* indexes = NULL;
                CFileData* f = NULL;
                int count = GetSelCount();
                if (count != 0)
                {
                    indexes = new int[count];
                    if (indexes == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;
                    }
                    GetSelItems(count, indexes);
                }
                else // nic neni oznaceno
                {
                    int i = GetCaretIndex();
                    if (i < 0 || i >= Dirs->Count + Files->Count)
                    {
                        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;           // spatny index (zadne soubory)
                    }
                    if (i == 0 && subDir)
                    {
                        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;           // se ".." nepracujem
                    }
                    f = (i < Dirs->Count) ? &Dirs->At(i) : &Files->At(i - Dirs->Count);
                }
                //---
                COperations* script = new COperations(1000, 500, NULL, NULL, NULL);
                if (script == NULL)
                    TRACE_E(LOW_MEMORY);
                else
                {
                    HWND hFocusedWnd = GetFocus();
                    CreateSafeWaitWindow(LoadStr(IDS_ANALYSINGDIRTREEESC), NULL, 1000, TRUE, MainWindow->HWindow);
                    EnableWindow(MainWindow->HWindow, FALSE);

                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    // zajisteni korektniho vztahu mezi Compressed a Encrypted
                    if (chDlg.Encrypted == 1)
                    {
                        if (chDlg.Compressed != 0)
                            TRACE_E("CFilesWindow::ChangeAttr(): unexpected value of chDlg.Compressed!");
                        chDlg.Compressed = 0;
                    }
                    else
                    {
                        if (chDlg.Compressed == 1)
                        {
                            if (chDlg.Encrypted != 0)
                                TRACE_E("CFilesWindow::ChangeAttr(): unexpected value of chDlg.Encrypted!");
                            chDlg.Encrypted = 0;
                        }
                    }

                    CAttrsData attrsData;
                    attrsData.AttrAnd = 0xFFFFFFFF;
                    attrsData.AttrOr = 0;
                    attrsData.SubDirs = chDlg.RecurseSubDirs;
                    attrsData.ChangeCompression = FALSE;
                    attrsData.ChangeEncryption = FALSE;
                    dlgData.ChangeCompression = FALSE;
                    dlgData.ChangeEncryption = FALSE;

                    if (chDlg.Archive == 0)
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_ARCHIVE);
                    if (chDlg.ReadOnly == 0)
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_READONLY);
                    if (chDlg.Hidden == 0)
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_HIDDEN);
                    if (chDlg.System == 0)
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_SYSTEM);
                    if (chDlg.Compressed == 0)
                    {
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_COMPRESSED);
                        attrsData.ChangeCompression = TRUE;
                        dlgData.ChangeCompression = TRUE;
                    }
                    if (chDlg.Encrypted == 0)
                    {
                        attrsData.AttrAnd &= ~(FILE_ATTRIBUTE_ENCRYPTED);
                        attrsData.ChangeEncryption = TRUE;
                        dlgData.ChangeEncryption = TRUE;
                    }

                    if (chDlg.Archive == 1)
                        attrsData.AttrOr |= FILE_ATTRIBUTE_ARCHIVE;
                    if (chDlg.ReadOnly == 1)
                        attrsData.AttrOr |= FILE_ATTRIBUTE_READONLY;
                    if (chDlg.Hidden == 1)
                        attrsData.AttrOr |= FILE_ATTRIBUTE_HIDDEN;
                    if (chDlg.System == 1)
                        attrsData.AttrOr |= FILE_ATTRIBUTE_SYSTEM;
                    if (chDlg.Compressed == 1)
                    {
                        attrsData.AttrOr |= FILE_ATTRIBUTE_COMPRESSED;
                        attrsData.ChangeCompression = TRUE;
                        dlgData.ChangeCompression = TRUE;
                    }
                    if (chDlg.Encrypted == 1)
                    {
                        attrsData.AttrOr |= FILE_ATTRIBUTE_ENCRYPTED;
                        attrsData.ChangeEncryption = TRUE;
                        dlgData.ChangeEncryption = TRUE;
                    }

                    script->ClearReadonlyMask = 0xFFFFFFFF;
                    BOOL res = BuildScriptMain(script, atChangeAttrs, NULL, NULL, count,
                                               indexes, f, &attrsData, NULL, FALSE, NULL);
                    if (script->Count == 0)
                        res = FALSE;
                    // prohozeno kvuli umozneni aktivace hlavniho okna (nesmi byt disable), jinak prepina do jine app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();

                    // pokud je aktivni Salamander, zavolame SetFocus na zapamatovane okno (SetFocus nefunguje
                    // pokud je hl. okno disablovane - po deaktivaci/aktivaci disablovaneho hl. okna aktivni panel
                    // nema fokus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    // pripravime refresh neautomaticky refreshovanych adresaru
                    // zmena v adresari zobrazenem v panelu a pokud se pracovalo i v podadresarich, tak i v podadresarich
                    script->SetWorkPath1(GetPath(), chDlg.RecurseSubDirs);

                    if (!res || !StartProgressDialog(script, LoadStr(IDS_CHANGEATTRSTITLE), &dlgData, NULL))
                    {
                        UpdateWindow(MainWindow->HWindow);
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    else // odstraneni sel. indexu (necekame az operace dobehne, bezi v jinem threadu)
                    {
                        SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        UpdateWindow(MainWindow->HWindow);
                    }
                }
                //---
                if (indexes != NULL)
                    delete[] (indexes);
                //---  pokud je aktivni nejaky okno salamandra, konci suspend mode
            }
            UpdateWindow(MainWindow->HWindow);
            FilesActionInProgress = FALSE;
        }
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_CHANGEATTRS)) // v panelu je FS
        {
            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            int panel = MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT;

            int count = GetSelCount();
            int selectedDirs = 0;
            if (count > 0)
            {
                // spocitame kolik adresaru je oznaceno (zbytek oznacenych polozek jsou soubory)
                int i;
                for (i = 0; i < Dirs->Count; i++) // ".." nemuzou byt oznaceny, test by byl zbytecny
                {
                    if (Dirs->At(i).Selected)
                        selectedDirs++;
                }
            }
            else
                count = 0;

            BOOL success = GetPluginFS()->ChangeAttributes(GetPluginFS()->GetPluginFSName(), HWindow,
                                                           panel, count - selectedDirs, selectedDirs);

            // opet zvysime prioritu threadu, operace dobehla
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            if (success) // uspech -> unselect
            {
                SetSel(FALSE, -1, TRUE);                        // explicitni prekresleni
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                UpdateWindow(MainWindow->HWindow);
            }
        }
    }
    // pokud jsme nejakou polozku vybrali, zase ji odvyberem
    UnselectItemWithName(temporarySelected);

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::FindFile()
{
    CALL_STACK_MESSAGE1("CFilesWindow::FindFile()");
    if (Is(ptDisk) && CheckPath(TRUE) != ERROR_SUCCESS)
        return;

    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
        GetPluginFS()->IsServiceSupported(FS_SERVICE_OPENFINDDLG))
    { // zkusime otevrit Find pro FS v panelu, pokud se povede, nema uz smysl otevirat standardni Find
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
        BOOL done = GetPluginFS()->OpenFindDialog(GetPluginFS()->GetPluginFSName(),
                                                  this == MainWindow->LeftPanel ? PANEL_LEFT : PANEL_RIGHT);
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        if (done)
            return;
    }

    if (SystemPolicies.GetNoFind() || SystemPolicies.GetNoShellSearchButton())
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

    OpenFindDialog(MainWindow->HWindow, Is(ptDisk) ? GetPath() : "");
}

void CFilesWindow::ViewFile(char* name, BOOL altView, DWORD handlerID, int enumFileNamesSourceUID,
                            int enumFileNamesLastFileIndex)
{
    CALL_STACK_MESSAGE6("CFilesWindow::ViewFile(%s, %d, %u, %d, %d)", name, altView, handlerID,
                        enumFileNamesSourceUID, enumFileNamesLastFileIndex);
    // overime si jestli je soubor na pristupne ceste
    char path[MAX_PATH + 10];
    if (name == NULL) // soubor z panelu
    {
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            if (CheckPath(TRUE) != ERROR_SUCCESS)
                return;
        }
    }
    else // soubor z windowsove cesty (Find + Alt+F11)
    {
        char* backSlash = strrchr(name, '\\');
        if (backSlash != NULL)
        {
            memcpy(path, name, backSlash - name);
            path[backSlash - name] = 0;
            if (CheckPath(TRUE, path) != ERROR_SUCCESS)
                return;
        }
    }

    BOOL addToHistory = name != NULL;
    // pokud jde o view/edit z panelu, ziskame plne dlouhe jmeno
    BOOL useDiskCache = FALSE;          // TRUE jen pro ZIP - pouziva disk-cache
    BOOL arcCacheCacheCopies = TRUE;    // cachovat kopie v disk-cache, pokud si plugin archivatoru nepreje jinak
    char dcFileName[3 * MAX_PATH + 50]; // ZIP: jmeno pro disk-cache
    if (name == NULL)
    {
        int i = GetCaretIndex();
        if (i >= Dirs->Count && i < Dirs->Count + Files->Count)
        {
            CFileData* f = &Files->At(i - Dirs->Count);
            if (Is(ptDisk))
            {
                if (enumFileNamesLastFileIndex == -1)
                    enumFileNamesLastFileIndex = i - Dirs->Count;
                lstrcpyn(path, GetPath(), MAX_PATH);
                if (GetPath()[strlen(GetPath()) - 1] != '\\')
                    strcat(path, "\\");
                char* s = path + strlen(path);
                if ((s - path) + f->NameLen >= MAX_PATH)
                {
                    if (f->DosName != NULL && strlen(f->DosName) + (s - path) < MAX_PATH)
                        strcpy(s, f->DosName);
                    else
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE),
                                      MB_OK | MB_ICONEXCLAMATION);
                        return;
                    }
                }
                else
                    strcpy(s, f->Name);
                // zkusime jestli je jmeno souboru platne, pripadne zkusime jeste jeho DOS-jmeno
                // (resi soubory dosazitelne jen pres Unicode nebo DOS-jmena)
                if (f->DosName != NULL && SalGetFileAttributes(path) == 0xffffffff)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME)
                    {
                        if (strlen(f->DosName) + (s - path) < MAX_PATH)
                        {
                            strcpy(s, f->DosName);
                            if (SalGetFileAttributes(path) == 0xffffffff) // stale chyba -> vracime se na long-name
                            {
                                if ((s - path) + f->NameLen < MAX_PATH)
                                    strcpy(s, f->Name);
                            }
                        }
                    }
                }
                name = path;
                addToHistory = TRUE;
            }
            else
            {
                if (Is(ptZIPArchive))
                {
                    useDiskCache = TRUE;
                    StrICpy(dcFileName, GetZIPArchive()); // jmeno souboru archivu se ma porovnavat "case-insensitive" (Windows file system), prevedeme ho proto vzdy na mala pismenka
                    if (GetZIPPath()[0] != 0)
                    {
                        if (GetZIPPath()[0] != '\\')
                            strcat(dcFileName, "\\");
                        strcat(dcFileName, GetZIPPath());
                    }
                    if (dcFileName[strlen(dcFileName) - 1] != '\\')
                        strcat(dcFileName, "\\");
                    strcat(dcFileName, f->Name);

                    // nastaveni disk-cache pro plugin (std. hodnoty se zmeni jen u pluginu)
                    char arcCacheTmpPath[MAX_PATH];
                    arcCacheTmpPath[0] = 0;
                    BOOL arcCacheOwnDelete = FALSE;
                    CPluginInterfaceAbstract* plugin = NULL; // != NULL pokud ma plugin sve vlastni mazani
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0) // nasli jsme podporovany archiv
                    {
                        format--;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0) // view: jde o interni zpracovani (plug-in)?
                        {
                            CPluginData* data = Plugins.Get(-index - 1);
                            if (data != NULL)
                            {
                                data->GetCacheInfo(arcCacheTmpPath, &arcCacheOwnDelete, &arcCacheCacheCopies);
                                if (arcCacheOwnDelete)
                                    plugin = data->GetPluginInterface()->GetInterface();
                            }
                        }
                    }

                    char nameInArchive[2 * MAX_PATH];
                    strcpy(nameInArchive, dcFileName + strlen(GetZIPArchive()) + 1);

                    // krome sebe sameho porovnavame soubor se vsemi ostatnimi a hledame case-sensitive shodne jmeno;
                    // pokud existuje, musime tyto dva soubory od sebe v disk-cache necim odlisit, zvolil jsem
                    // alokovanou adresu Name - v protilehlych panelech se stejnym archivem to disk-cache nepouzije,
                    // ale vzhledem k nepravdepodobnosti tohoto pripadu, je to dobre i tak az az
                    int x;
                    for (x = 0; x < Files->Count; x++)
                    {
                        if (i - Dirs->Count != x)
                        {
                            CFileData* f2 = &Files->At(x);
                            if (strcmp(f2->Name, f->Name) == 0)
                            {
                                sprintf(dcFileName + strlen(dcFileName), ":0x%p", f->Name);
                                break;
                            }
                        }
                    }

                    BOOL exists;
                    int errorCode;
                    char validTmpName[MAX_PATH];
                    validTmpName[0] = 0;
                    if (!SalIsValidFileNameComponent(f->Name))
                    {
                        lstrcpyn(validTmpName, f->Name, MAX_PATH);
                        SalMakeValidFileNameComponent(validTmpName);
                    }
                    name = (char*)DiskCache.GetName(dcFileName,
                                                    validTmpName[0] != 0 ? validTmpName : f->Name,
                                                    &exists, FALSE,
                                                    arcCacheTmpPath[0] != 0 ? arcCacheTmpPath : NULL,
                                                    plugin != NULL, plugin, &errorCode);
                    if (name == NULL)
                    {
                        if (errorCode == DCGNE_TOOLONGNAME)
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_UNPACKTOOLONGNAME),
                                          LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                        }
                        return;
                    }

                    if (!exists) // musime ho vypakovat
                    {
                        char* backSlash = strrchr(name, '\\');
                        char tmpPath[MAX_PATH];
                        memcpy(tmpPath, name, backSlash - name);
                        tmpPath[backSlash - name] = 0;
                        BeginStopRefresh(); // cmuchal si da pohov
                        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);
                        HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
                        BOOL renamingNotSupported = FALSE;
                        if (PackUnpackOneFile(this, GetZIPArchive(), PluginData.GetInterface(), nameInArchive, f, tmpPath,
                                              validTmpName[0] == 0 ? NULL : validTmpName,
                                              validTmpName[0] == 0 ? NULL : &renamingNotSupported))
                        {
                            SetCursor(oldCur);
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                            CQuadWord size(0, 0);
                            HANDLE file = HANDLES_Q(CreateFile(name, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE,
                                                               NULL, OPEN_EXISTING, 0, NULL));
                            if (file != INVALID_HANDLE_VALUE)
                            {
                                DWORD err;
                                SalGetFileSize(file, size, err); // chyby ignorujeme, velikost souboru neni az tak dulezita
                                HANDLES(CloseHandle(file));
                            }
                            DiskCache.NamePrepared(dcFileName, size);
                        }
                        else
                        {
                            SetCursor(oldCur);
                            if (renamingNotSupported) // aby se stejna hlaska nedoplnovala do hromady pluginu, vypiseme ji zde pro vsechny
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_UNPACKINVNAMERENUNSUP),
                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                            DiskCache.ReleaseName(dcFileName, FALSE); // nevypakovano, neni co cachovat
                            EndStopRefresh();                         // ted uz zase cmuchal nastartuje
                            return;
                        }
                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    }
                }
                else
                {
                    if (Is(ptPluginFS))
                    {
                        if (GetPluginFS()->NotEmpty() && // FS je v poradku a podporuje view-file
                            GetPluginFS()->IsServiceSupported(FS_SERVICE_VIEWFILE))
                        {
                            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                            CSalamanderForViewFileOnFS sal(altView, handlerID);
                            GetPluginFS()->ViewFile(GetPluginFS()->GetPluginFSName(), HWindow, &sal, *f);

                            // opet zvysime prioritu threadu, operace dobehla
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                        }
                        return; // view na FS uz je hotovy
                    }
                    else
                    {
                        TRACE_E("Incorrect call to CFilesWindow::ViewFile()");
                        return;
                    }
                }
            }
        }
        else
        {
            return;
        }
    }

    HANDLE lock = NULL;
    BOOL lockOwner = FALSE;
    ViewFileInt(HWindow, name, altView, handlerID, useDiskCache, lock, lockOwner, addToHistory,
                enumFileNamesSourceUID, enumFileNamesLastFileIndex);

    if (useDiskCache)
    {
        if (lock != NULL) // zajistime provazani mezi viewerem a disk-cache
        {
            DiskCache.AssignName(dcFileName, lock, lockOwner, arcCacheCacheCopies ? crtCache : crtDirect);
        }
        else // neotevrel se viewer nebo jen nema "lock" objekt - soubor zkusime aspon nechat v disk-cache
        {
            DiskCache.ReleaseName(dcFileName, arcCacheCacheCopies);
        }
    }
}

BOOL ViewFileInt(HWND parent, const char* name, BOOL altView, DWORD handlerID, BOOL returnLock,
                 HANDLE& lock, BOOL& lockOwner, BOOL addToHistory, int enumFileNamesSourceUID,
                 int enumFileNamesLastFileIndex)
{
    BOOL success = FALSE;
    lock = NULL;
    lockOwner = FALSE;

    // ziskani plneho DOS jmena
    char dosName[MAX_PATH];
    if (GetShortPathName(name, dosName, MAX_PATH) == 0)
    {
        TRACE_E("GetShortPathName() failed");
        dosName[0] = 0;
    }

    // najdeme jmeno souboru a zjistime jestli ma priponu - potrebne pro masky
    const char* namePart = strrchr(name, '\\');
    if (namePart == NULL)
    {
        TRACE_E("Invalid parameter for ViewFileInt(): " << name);
        return FALSE;
    }
    namePart++;
    const char* tmpExt = strrchr(namePart, '.');
    //if (tmpExt == NULL || tmpExt == namePart) tmpExt = namePart + lstrlen(namePart); // ".cvspass" neni pripona ...
    if (tmpExt == NULL)
        tmpExt = namePart + lstrlen(namePart); // ".cvspass" ve Windows je pripona ...
    else
        tmpExt++;

    // pozice pro viewery
    WINDOWPLACEMENT place;
    place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(MainWindow->HWindow, &place);
    // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
    // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
    RECT monitorRect;
    RECT workRect;
    MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
    OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
               workRect.top - monitorRect.top);

    // pokud jsme volani napriklad z findu a hlavni okno je minimalizovane,
    // nechceme minimalizovany viewer
    if (place.showCmd == SW_MINIMIZE || place.showCmd == SW_SHOWMINIMIZED ||
        place.showCmd == SW_SHOWMINNOACTIVE)
        place.showCmd = SW_SHOWNORMAL;

    // najdeme spravny viewer a spustime ho
    CViewerMasks* masks = (altView ? MainWindow->AltViewerMasks : MainWindow->ViewerMasks);
    CViewerMasksItem* viewer = NULL;

    if (handlerID != 0xFFFFFFFF)
    {
        // pokusim se najit viewer s odpovidajicim HandlerID
        int j;
        for (j = 0; viewer == NULL && j < 2; j++)
        {
            CViewerMasks* masks2 = (j == 0 ? MainWindow->ViewerMasks : MainWindow->AltViewerMasks);
            int i;
            for (i = 0; viewer == NULL && i < masks2->Count; i++)
            {
                if (masks2->At(i)->HandlerID == handlerID)
                    viewer = masks2->At(i);
            }
        }
    }

    if (viewer == NULL)
    {
        int i;
        for (i = 0; i < masks->Count; i++)
        {
            int err;
            if (masks->At(i)->Masks->PrepareMasks(err))
            {
                if (masks->At(i)->Masks->AgreeMasks(namePart, tmpExt))
                {
                    viewer = masks->At(i);

                    if (viewer != NULL && viewer->ViewerType != VIEWER_EXTERNAL &&
                        viewer->ViewerType != VIEWER_INTERNAL)
                    { // jen plug-inove viewery
                        CPluginData* plugin = Plugins.Get(-viewer->ViewerType - 1);
                        if (plugin != NULL && plugin->SupportViewer)
                        {
                            if (!plugin->CanViewFile(name))
                                continue; // zkusime najit dalsi viewer, tenhle to nebere
                        }
                        else
                            TRACE_E("Unexpected error (before CanViewFile) in (Alt)ViewerMasks (invalid ViewerType).");
                    }
                    break; // vse OK, jdeme otevrit viewer
                }
            }
            else
                TRACE_E("Unexpected error in group mask.");
        }
    }

    if (viewer != NULL)
    {
        //    if (MakeFileAvailOfflineIfOneDriveOnWin81(parent, name))
        //    {
        if (addToHistory)
            MainWindow->FileHistory->AddFile(fhitView, viewer->HandlerID, name); // pridame soubor do historie

        switch (viewer->ViewerType)
        {
        case VIEWER_EXTERNAL:
        {
            char expCommand[MAX_PATH];
            char expArguments[MAX_PATH];
            char expInitDir[MAX_PATH];
            if (ExpandCommand(parent, viewer->Command, expCommand, MAX_PATH, FALSE) &&
                ExpandArguments(parent, name, dosName, viewer->Arguments, expArguments, MAX_PATH, NULL) &&
                ExpandInitDir(parent, name, dosName, viewer->InitDir, expInitDir, MAX_PATH, FALSE))
            {
                if (SystemPolicies.GetMyRunRestricted() &&
                    !SystemPolicies.GetMyCanRun(expCommand))
                {
                    MSGBOXEX_PARAMS params;
                    memset(&params, 0, sizeof(params));
                    params.HParent = parent;
                    params.Flags = MSGBOXEX_OK | MSGBOXEX_HELP | MSGBOXEX_ICONEXCLAMATION;
                    params.Caption = LoadStr(IDS_POLICIESRESTRICTION_TITLE);
                    params.Text = LoadStr(IDS_POLICIESRESTRICTION);
                    params.ContextHelpId = IDH_GROUPPOLICY;
                    params.HelpCallback = MessageBoxHelpCallback;
                    SalMessageBoxEx(&params);
                    break;
                }

                STARTUPINFO si;
                PROCESS_INFORMATION pi;
                memset(&si, 0, sizeof(STARTUPINFO));
                si.cb = sizeof(STARTUPINFO);
                si.dwX = place.rcNormalPosition.left;
                si.dwY = place.rcNormalPosition.top;
                si.dwXSize = place.rcNormalPosition.right - place.rcNormalPosition.left;
                si.dwYSize = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
                si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE |
                              STARTF_USESHOWWINDOW;
                si.wShowWindow = SW_SHOWNORMAL;

                char cmdLine[2 * MAX_PATH];
                lstrcpyn(cmdLine, expCommand, 2 * MAX_PATH);
                AddDoubleQuotesIfNeeded(cmdLine, 2 * MAX_PATH); // CreateProcess chce mit jmeno s mezerama v uvozovkach (jinak zkousi ruzny varianty, viz help)
                int len = (int)strlen(cmdLine);
                int lArgs = (int)strlen(expArguments);
                if (len + lArgs + 2 <= 2 * MAX_PATH)
                {
                    cmdLine[len] = ' ';
                    memcpy(cmdLine + len + 1, expArguments, lArgs + 1);

                    MainWindow->SetDefaultDirectories();

                    if (expInitDir[0] == 0) // nemelo by se vubec stat
                    {
                        strcpy(expInitDir, name);
                        CutDirectory(expInitDir);
                    }
                    if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                                               NORMAL_PRIORITY_CLASS, NULL, expInitDir, &si, &pi)))
                    {
                        DWORD err = GetLastError();
                        char buff[4 * MAX_PATH];
                        sprintf(buff, LoadStr(IDS_ERROREXECVIEW), expCommand, GetErrorText(err));
                        SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                    }
                    else
                    {
                        success = TRUE;
                        if (returnLock)
                        {
                            lock = pi.hProcess;
                            lockOwner = TRUE;
                        }
                        else
                            HANDLES(CloseHandle(pi.hProcess));
                        HANDLES(CloseHandle(pi.hThread));
                    }
                }
                else
                {
                    char buff[4 * MAX_PATH];
                    sprintf(buff, LoadStr(IDS_ERROREXECVIEW), expCommand, LoadStr(IDS_TOOLONGNAME));
                    SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
            }
            break;
        }

        case VIEWER_INTERNAL:
        {
            if (Configuration.SavePosition &&
                Configuration.WindowPlacement.length != 0)
            {
                place = Configuration.WindowPlacement;
                // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
                // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
                RECT monitorRect2;
                RECT workRect2;
                MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect2, &monitorRect2);
                OffsetRect(&place.rcNormalPosition, workRect2.left - monitorRect2.left,
                           workRect2.top - monitorRect2.top);
                MultiMonEnsureRectVisible(&place.rcNormalPosition, TRUE);
            }

            HANDLE lockAux = NULL;
            BOOL lockOwnerAux = FALSE;
            if (OpenViewer(name, vtText,
                           place.rcNormalPosition.left,
                           place.rcNormalPosition.top,
                           place.rcNormalPosition.right - place.rcNormalPosition.left,
                           place.rcNormalPosition.bottom - place.rcNormalPosition.top,
                           place.showCmd,
                           returnLock, &lockAux, &lockOwnerAux, NULL,
                           enumFileNamesSourceUID, enumFileNamesLastFileIndex))
            {
                success = TRUE;
                if (returnLock && lockAux != NULL)
                {
                    lock = lockAux;
                    lockOwner = lockOwnerAux;
                }
            }
            break;
        }

        default: // plug-ins
        {
            HANDLE lockAux = NULL;
            BOOL lockOwnerAux = FALSE;

            CPluginData* plugin = Plugins.Get(-viewer->ViewerType - 1);
            if (plugin != NULL && plugin->SupportViewer)
            {
                if (plugin->ViewFile(name, place.rcNormalPosition.left, place.rcNormalPosition.top,
                                     place.rcNormalPosition.right - place.rcNormalPosition.left,
                                     place.rcNormalPosition.bottom - place.rcNormalPosition.top,
                                     place.showCmd, Configuration.AlwaysOnTop,
                                     returnLock, &lockAux, &lockOwnerAux,
                                     enumFileNamesSourceUID, enumFileNamesLastFileIndex))
                {
                    success = TRUE;
                    if (returnLock && lockAux != NULL)
                    {
                        lock = lockAux;
                        lockOwner = lockOwnerAux;
                    }
                }
            }
            else
                TRACE_E("Unexpected error in (Alt)ViewerMasks (invalid ViewerType).");
            break;
        }
        }
        //    }
    }
    else
    {
        char buff[MAX_PATH + 300];
        int textID = altView ? IDS_CANT_VIEW_FILE_ALT : IDS_CANT_VIEW_FILE;
        sprintf(buff, LoadStr(textID), name);
        SalMessageBox(parent, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
    }
    return success;
}

void CFilesWindow::EditFile(char* name, DWORD handlerID)
{
    CALL_STACK_MESSAGE3("CFilesWindow::EditFile(%s, %u)", name, handlerID);
    if (!Is(ptDisk) && name == NULL)
    {
        TRACE_E("Incorrect call to CFilesWindow::EditFile()");
        return;
    }

    // overime si jestli je soubor na pristupne ceste
    char path[MAX_PATH + 10];
    if (name == NULL)
    {
        if (CheckPath(TRUE) != ERROR_SUCCESS)
            return;
    }
    else
    {
        char* backSlash = strrchr(name, '\\');
        if (backSlash != NULL)
        {
            memcpy(path, name, backSlash - name);
            path[backSlash - name] = 0;
            if (CheckPath(TRUE, path) != ERROR_SUCCESS)
                return;
        }
    }

    BOOL addToHistory = name != NULL && Is(ptDisk);

    // pokud jde o view/edit z panelu, ziskame plne dlouhe jmeno
    if (name == NULL)
    {
        int i = GetCaretIndex();
        if (i >= Dirs->Count && i < Dirs->Count + Files->Count)
        {
            CFileData* f = &Files->At(i - Dirs->Count);
            if (Is(ptDisk))
            {
                lstrcpyn(path, GetPath(), MAX_PATH);
                if (GetPath()[strlen(GetPath()) - 1] != '\\')
                    strcat(path, "\\");
                char* s = path + strlen(path);
                if ((s - path) + f->NameLen >= MAX_PATH)
                {
                    if (f->DosName != NULL && strlen(f->DosName) + (s - path) < MAX_PATH)
                        strcpy(s, f->DosName);
                    else
                    {
                        SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORTITLE),
                                      MB_OK | MB_ICONEXCLAMATION);
                        return;
                    }
                }
                else
                    strcpy(s, f->Name);
                // zkusime jestli je jmeno souboru platne, pripadne zkusime jeste jeho DOS-jmeno
                // (resi soubory dosazitelne jen pres Unicode nebo DOS-jmena)
                if (f->DosName != NULL && SalGetFileAttributes(path) == 0xffffffff)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME)
                    {
                        if (strlen(f->DosName) + (s - path) < MAX_PATH)
                        {
                            strcpy(s, f->DosName);
                            if (SalGetFileAttributes(path) == 0xffffffff) // stale chyba -> vracime se na long-name
                            {
                                if ((s - path) + f->NameLen < MAX_PATH)
                                    strcpy(s, f->Name);
                            }
                        }
                    }
                }
                name = path;
                addToHistory = TRUE;
            }
        }
        else
        {
            return;
        }
    }

    // ziskani plneho DOS jmena
    char dosName[MAX_PATH];
    if (GetShortPathName(name, dosName, MAX_PATH) == 0)
    {
        TRACE_I("GetShortPathName() failed.");
        dosName[0] = 0;
    }

    // najdeme jmeno souboru a zjistime jestli ma priponu - potrebne pro masky
    char* namePart = strrchr(name, '\\');
    if (namePart == NULL)
    {
        TRACE_E("Invalid parameter CFilesWindow::EditFile(): " << name);
        return;
    }
    namePart++;
    char* tmpExt = strrchr(namePart, '.');
    //if (tmpExt == NULL || tmpExt == namePart) tmpExt = namePart + lstrlen(namePart); // ".cvspass" neni pripona ...
    if (tmpExt == NULL)
        tmpExt = namePart + lstrlen(namePart); // ".cvspass" ve Windows je pripona ...
    else
        tmpExt++;

    // pozice pro editory
    WINDOWPLACEMENT place;
    place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(MainWindow->HWindow, &place);
    // GetWindowPlacement cti Taskbar, takze pokud je Taskbar nahore nebo vlevo,
    // jsou hodnoty posunute o jeho rozmery. Provedeme korekci.
    RECT monitorRect;
    RECT workRect;
    MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
    OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
               workRect.top - monitorRect.top);
    // pokud jsme volani napriklad z findu a hlavni okno je minimalizovane,
    // nechceme minimalizovany editor
    if (place.showCmd == SW_MINIMIZE || place.showCmd == SW_SHOWMINIMIZED ||
        place.showCmd == SW_SHOWMINNOACTIVE)
        place.showCmd = SW_SHOWNORMAL;

    // najdeme spravny editor a spustime ho
    CEditorMasks* masks = MainWindow->EditorMasks;

    CEditorMasksItem* editor = NULL;

    if (handlerID != 0xFFFFFFFF)
    {
        // pokusim se najit editor s odpovidajicim HandlerID
        int i;
        for (i = 0; editor == NULL && i < masks->Count; i++)
        {
            if (masks->At(i)->HandlerID == handlerID)
                editor = masks->At(i);
        }
    }

    if (editor == NULL)
    {
        int i;
        for (i = 0; i < masks->Count; i++)
        {
            int err;
            if (masks->At(i)->Masks->PrepareMasks(err))
            {
                if (masks->At(i)->Masks->AgreeMasks(namePart, tmpExt))
                {
                    editor = masks->At(i);
                    break;
                }
            }
            else
                TRACE_E("Unexpected error in group mask");
        }
    }

    if (editor != NULL)
    {
        if (addToHistory)
            MainWindow->FileHistory->AddFile(fhitEdit, editor->HandlerID, name); // pridame soubor do historie

        char expCommand[MAX_PATH];
        char expArguments[MAX_PATH];
        char expInitDir[MAX_PATH];
        if (ExpandCommand(HWindow, editor->Command, expCommand, MAX_PATH, FALSE) &&
            ExpandArguments(HWindow, name, dosName, editor->Arguments, expArguments, MAX_PATH, NULL) &&
            ExpandInitDir(HWindow, name, dosName, editor->InitDir, expInitDir, MAX_PATH, FALSE))
        {
            if (SystemPolicies.GetMyRunRestricted() &&
                !SystemPolicies.GetMyCanRun(expCommand))
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

            STARTUPINFO si;
            PROCESS_INFORMATION pi;
            memset(&si, 0, sizeof(STARTUPINFO));
            si.cb = sizeof(STARTUPINFO);
            si.dwX = place.rcNormalPosition.left;
            si.dwY = place.rcNormalPosition.top;
            si.dwXSize = place.rcNormalPosition.right - place.rcNormalPosition.left;
            si.dwYSize = place.rcNormalPosition.bottom - place.rcNormalPosition.top;
            si.dwFlags |= STARTF_USEPOSITION | STARTF_USESIZE |
                          STARTF_USESHOWWINDOW;
            si.wShowWindow = SW_SHOWNORMAL;

            char cmdLine[2 * MAX_PATH];
            lstrcpyn(cmdLine, expCommand, 2 * MAX_PATH);
            AddDoubleQuotesIfNeeded(cmdLine, 2 * MAX_PATH); // CreateProcess chce mit jmeno s mezerama v uvozovkach (jinak zkousi ruzny varianty, viz help)
            int len = (int)strlen(cmdLine);
            int lArgs = (int)strlen(expArguments);
            if (len + lArgs + 2 <= 2 * MAX_PATH)
            {
                cmdLine[len] = ' ';
                memcpy(cmdLine + len + 1, expArguments, lArgs + 1);

                MainWindow->SetDefaultDirectories();

                if (expInitDir[0] == 0) // nemelo by se vubec stat
                {
                    strcpy(expInitDir, name);
                    CutDirectory(expInitDir);
                }
                if (!HANDLES(CreateProcess(NULL, cmdLine, NULL, NULL, FALSE,
                                           NORMAL_PRIORITY_CLASS, NULL, expInitDir, &si, &pi)))
                {
                    DWORD err = GetLastError();
                    char buff[4 * MAX_PATH];
                    sprintf(buff, LoadStr(IDS_ERROREXECEDIT), expCommand, GetErrorText(err));
                    SalMessageBox(HWindow, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                }
                else
                {
                    HANDLES(CloseHandle(pi.hProcess));
                    HANDLES(CloseHandle(pi.hThread));
                }
            }
            else
            {
                char buff[4 * MAX_PATH];
                sprintf(buff, LoadStr(IDS_ERROREXECEDIT), expCommand, LoadStr(IDS_TOOLONGNAME));
                SalMessageBox(HWindow, buff, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
            }
        }
    }
    else
    {
        char buff[MAX_PATH + 300];
        sprintf(buff, LoadStr(IDS_CANT_EDIT_FILE), name);
        SalMessageBox(HWindow, buff, LoadStr(IDS_ERRORTITLE),
                      MB_OK | MB_ICONEXCLAMATION);
    }
}

void CFilesWindow::EditNewFile()
{
    CALL_STACK_MESSAGE1("CFilesWindow::EditNewFile()");
    BeginStopRefresh(); // cmuchal si da pohov

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(TRUE);

    char path[MAX_PATH];
    if (Configuration.UseEditNewFileDefault)
        lstrcpyn(path, Configuration.EditNewFileDefault, MAX_PATH);
    else
        lstrcpyn(path, LoadStr(IDS_EDITNEWFILE_DEFAULTNAME), MAX_PATH);
    CTruncatedString subject;
    subject.Set(LoadStr(IDS_NEWFILENAME), NULL);

    BOOL first = TRUE;

    while (1)
    {
        CEditNewFileDialog dlg(HWindow, path, MAX_PATH, &subject, Configuration.EditNewHistory, EDITNEW_HISTORY_SIZE);

        // Cast lidi chce zakladat vzdy .txt a vyhovuje jim, ze prepisi pouze priponu; cast lidi zaklada ruzne soubory a chteji prepisovat cely nazev,
        // takze jsme ustoupili a zavedli pro Edit New File vlastni polozku v konfiguraci.
        // ------------------
        // pro EditNew nema "chytrej" vyber pouze nazvu smysl, protoze lidi meni i priponu, viz nase forum:
        // https://forum.altap.cz/viewtopic.php?t=2655
        // -----------------
        // Od Windows Vista MS zavedli velice zadanou vec: quick rename implicitne oznaci pouze nazev bez tecky a pripony
        // stejny kod je zde ctyrikrat
        if (!Configuration.EditNewSelectAll)
        {
            int selectionEnd = -1;
            if (first)
            {
                const char* dot = strrchr(path, '.');
                if (dot != NULL && dot > path) // sice plati, ze ".cvspass" ve Windows je pripona, ale Explorer pro ".cvspass" oznacuje cele jmeno, tak to delame take
                                               //      if (dot != NULL)
                    selectionEnd = (int)(dot - path);
                dlg.SetSelectionEnd(selectionEnd);
                first = FALSE; // po chybe jiz dostaneme plny nazev souboru, takze ho oznacime komplet
            }
        }

        if (dlg.Execute() == IDOK)
        {
            UpdateWindow(MainWindow->HWindow);

            // ocistime jmeno od nezadoucich znaku na zacatku a konci,
            // delame to jen u posledni komponenty, predesle bud jiz existuji a je to fuk (poresi si
            // to system) nebo se pri vytvareni okontroluji a pripadne se zarve chyba (jen nedojde
            // k ocisteni z nasi strany, at se user taky snazi, je to podprahovka)
            char* lastCompName = strrchr(path, '\\');
            MakeValidFileName(lastCompName != NULL ? lastCompName + 1 : path);

            char* errText;
            int errTextID;
            //      if (SalGetFullName(path, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
            //                         MainWindow->GetActivePanel()->GetPath() : NULL, NextFocusName))
            if (SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, NextFocusName)) // kvuli konzistenci s ChangePathToDisk()
            {
                char checkPath[MAX_PATH];
                strcpy(checkPath, path);
                CutDirectory(checkPath);
                if (CheckPath(TRUE, checkPath) != ERROR_SUCCESS)
                {
                    EndStopRefresh(); // ted uz zase cmuchal nastartuje
                    return;
                }

                BOOL invalidName = FileNameInvalidForManualCreate(path);
                HANDLE hFile = INVALID_HANDLE_VALUE;
                if (invalidName)
                    SetLastError(ERROR_INVALID_NAME);
                else
                {
                    hFile = SalCreateFileEx(path, GENERIC_READ | GENERIC_WRITE, 0, FILE_ATTRIBUTE_NORMAL, NULL);
                    HANDLES_ADD_EX(__otQuiet, hFile != INVALID_HANDLE_VALUE, __htFile, __hoCreateFile, hFile, GetLastError(), TRUE);
                }
                BOOL editExisting = FALSE;
                if (hFile == INVALID_HANDLE_VALUE && GetLastError() == ERROR_FILE_EXISTS)
                {
                    if (SalMessageBox(HWindow, LoadStr(IDS_EDITNEWALREADYEX), LoadStr(IDS_QUESTION),
                                      MB_YESNO | MB_ICONEXCLAMATION | MSGBOXEX_ESCAPEENABLED) == IDYES)
                    {
                        editExisting = TRUE;
                    }
                    else
                        break;
                }
                if (hFile != INVALID_HANDLE_VALUE || editExisting)
                {
                    if (!editExisting)
                        HANDLES(CloseHandle(hFile));

                    EditFile(path);

                    // zmena jen v adresari, ve kterem se vytvoril soubor
                    MainWindow->PostChangeOnPathNotification(checkPath, FALSE);

                    break;
                }
                else
                    errText = GetErrorText(GetLastError());
            }
            else
                errText = LoadStr(errTextID);
            SalMessageBox(HWindow, errText, LoadStr(IDS_ERRORTITLE),
                          MB_OK | MB_ICONEXCLAMATION);
        }
        else
            break;
    }
    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

// na zaklade dostupnych vieweru naplni popup
void CFilesWindow::FillViewWithMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillViewWithMenu()");

    // sestrelime existujici polozky
    popup->RemoveAllItems();

    // vytahnu seznam indexu vieweru
    TDirectArray<CViewerMasksItem*> items(50, 10);
    if (!FillViewWithData(&items))
        return;

    MENU_ITEM_INFO mii;
    char buff[1024];
    int i;
    for (i = 0; i < items.Count; i++)
    {
        CViewerMasksItem* item = items[i];

        int imgIndex = -1; // no icon
        if (item->ViewerType < 0)
        {
            int pluginIndex = -item->ViewerType - 1;
            CPluginData* plugin = Plugins.Get(pluginIndex);
            lstrcpy(buff, plugin->Name);
            if (plugin->PluginIconIndex != -1) // plugin ma ikonku
                imgIndex = pluginIndex;
        }
        if (item->ViewerType == VIEWER_EXTERNAL)
            sprintf(buff, LoadStr(IDS_VIEWWITH_EXTERNAL), item->Command);
        if (item->ViewerType == VIEWER_INTERNAL)
            lstrcpy(buff, LoadStr(IDS_VIEWWITH_INTERNAL));

        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID | MENU_MASK_IMAGEINDEX;
        mii.Type = MENU_TYPE_STRING;
        mii.String = buff;
        mii.ID = CM_VIEWWITH_MIN + i;
        mii.ImageIndex = imgIndex;
        if (mii.ID > CM_VIEWWITH_MAX)
        {
            TRACE_E("mii.ID > CM_VIEWWITH_MAX");
            break;
        }
        popup->InsertItem(-1, TRUE, &mii);
    }
    if (popup->GetItemCount() == 0)
    {
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE;
        mii.Type = MENU_TYPE_STRING;
        mii.State = MENU_STATE_GRAYED;
        mii.String = LoadStr(IDS_VIEWWITH_EMPTY);
        popup->InsertItem(-1, TRUE, &mii);
    }
    else
        popup->AssignHotKeys();
}

BOOL CFilesWindow::FillViewWithData(TDirectArray<CViewerMasksItem*>* items)
{
    // merge probehne pres normalni i alternate viewers
    int type;
    for (type = 0; type < 2; type++)
    {
        CViewerMasks* masks;
        if (type == 0)
            masks = MainWindow->ViewerMasks;
        else
            masks = MainWindow->AltViewerMasks;

        int i;
        for (i = 0; i < masks->Count; i++)
        {
            CViewerMasksItem* item = masks->At(i);

            BOOL alreadyAdded = FALSE; // nechceme v seznamu jednu polozku vicekrat

            int j;
            for (j = 0; j < items->Count; j++)
            {
                CViewerMasksItem* oldItem = items->At(j);

                if (item->ViewerType == VIEWER_EXTERNAL)
                {
                    if (stricmp(item->Command, oldItem->Command) == 0 &&
                        stricmp(item->Arguments, oldItem->Arguments) == 0 &&
                        stricmp(item->InitDir, oldItem->InitDir) == 0)
                    {
                        alreadyAdded = TRUE;
                        break;
                    }
                }
                else
                {
                    if (item->ViewerType == oldItem->ViewerType)
                    {
                        alreadyAdded = TRUE;
                        break;
                    }
                }
            }

            if (!alreadyAdded)
            {
                items->Add(masks->At(i));
                if (!items->IsGood())
                {
                    items->ResetState();
                    return FALSE;
                }
            }
        }
    }
    return TRUE;
}

void CFilesWindow::OnViewFileWith(int index)
{
    BeginStopRefresh(); // cmuchal si da pohov

    // vytahnu seznam indexu vieweru
    TDirectArray<CViewerMasksItem*> items(50, 10);
    if (!FillViewWithData(&items))
    {
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
        return;
    }

    if (index < 0 || index >= items.Count)
    {
        TRACE_E("index=" << index);
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
        return;
    }

    ViewFile(NULL, FALSE, items[index]->HandlerID, Is(ptDisk) ? EnumFileNamesSourceUID : -1,
             -1 /* index podle fokusu */);

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::ViewFileWith(char* name, HWND hMenuParent, const POINT* menuPoint, DWORD* handlerID,
                                int enumFileNamesSourceUID, int enumFileNamesLastFileIndex)
{
    CALL_STACK_MESSAGE5("CFilesWindow::ViewFileWith(%s, , , %s, %d, %d)", name,
                        (handlerID == NULL ? "NULL" : "non-NULL"), enumFileNamesSourceUID,
                        enumFileNamesLastFileIndex);
    BeginStopRefresh(); // cmuchal si da pohov
    if (handlerID != NULL)
        *handlerID = 0xFFFFFFFF;

    CMenuPopup contextPopup(CML_FILES_VIEWWITH);
    FillViewWithMenu(&contextPopup);
    DWORD cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                   menuPoint->x, menuPoint->y, hMenuParent, NULL);
    if (cmd >= CM_VIEWWITH_MIN && cmd <= CM_VIEWWITH_MAX)
    {
        // vytahnu seznam indexu vieweru
        TDirectArray<CViewerMasksItem*> items(50, 10);
        if (!FillViewWithData(&items))
        {
            EndStopRefresh(); // ted uz zase cmuchal nastartuje
            return;
        }

        int index = cmd - CM_VIEWWITH_MIN;
        if (handlerID == NULL)
            ViewFile(name, FALSE, items[index]->HandlerID, enumFileNamesSourceUID, enumFileNamesLastFileIndex);
        else
            *handlerID = items[index]->HandlerID;
    }

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::FillEditWithMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillEditWithMenu()");

    // sestrelime existujici polozky
    popup->RemoveAllItems();

    // vytahnu seznam indexu editoru
    CEditorMasks* masks = MainWindow->EditorMasks;

    MENU_ITEM_INFO mii;
    char buff[1024];

    int i;
    for (i = 0; i < masks->Count; i++)
    {
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
        mii.Type = MENU_TYPE_STRING;
        mii.String = buff;
        mii.ID = CM_EDITWITH_MIN + i;
        if (mii.ID > CM_EDITWITH_MAX)
        {
            TRACE_E("mii.ID > CM_EDITWITH_MAX");
            break;
        }

        CEditorMasksItem* item = masks->At(i);

        // pokud maji uzivatele vic radku masek asociovanych s jednim viewerem/editorem,
        // zaradime polozku do seznamu pouze jednou
        BOOL alreadyAdded = FALSE;
        int j;
        for (j = 0; j < i; j++)
        {
            CEditorMasksItem* oldItem = masks->At(j);
            if (stricmp(item->Command, oldItem->Command) == 0 &&
                stricmp(item->Arguments, oldItem->Arguments) == 0 &&
                stricmp(item->InitDir, oldItem->InitDir) == 0)
            {
                alreadyAdded = TRUE;
                break;
            }
        }
        if (!alreadyAdded)
        {
            sprintf(buff, LoadStr(IDS_EDITWITH_EXTERNAL), item->Command);
            popup->InsertItem(-1, TRUE, &mii);
        }
    }
    if (popup->GetItemCount() == 0)
    {
        mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_STATE;
        mii.Type = MENU_TYPE_STRING;
        mii.State = MENU_STATE_GRAYED;
        mii.String = LoadStr(IDS_EDITWITH_EMPTY);
        popup->InsertItem(-1, TRUE, &mii);
    }
    else
        popup->AssignHotKeys();
}

void CFilesWindow::OnEditFileWith(int index)
{
    BeginStopRefresh(); // cmuchal si da pohov

    // vytahnu seznam indexu vieweru
    // vytahnu seznam indexu editoru
    CEditorMasks* masks = MainWindow->EditorMasks;

    if (index < 0 || index >= masks->Count)
    {
        TRACE_E("index=" << index);
        EndStopRefresh(); // ted uz zase cmuchal nastartuje
        return;
    }

    EditFile(NULL, masks->At(index)->HandlerID);

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::EditFileWith(char* name, HWND hMenuParent, const POINT* menuPoint, DWORD* handlerID)
{
    CALL_STACK_MESSAGE3("CFilesWindow::EditFileWith(%s, , , %s)", name,
                        (handlerID == NULL ? "NULL" : "non-NULL"));
    BeginStopRefresh(); // cmuchal si da pohov
    if (handlerID != NULL)
        *handlerID = 0xFFFFFFFF;

    // vytahnu seznam indexu editoru
    CEditorMasks* masks = MainWindow->EditorMasks;

    // vytvorim menu
    CMenuPopup contextPopup;
    FillEditWithMenu(&contextPopup);
    DWORD cmd = contextPopup.Track(MENU_TRACK_NONOTIFY | MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                   menuPoint->x, menuPoint->y, hMenuParent, NULL);
    if (cmd >= CM_EDITWITH_MIN && cmd <= CM_EDITWITH_MAX)
    {
        int index = cmd - CM_EDITWITH_MIN;
        if (handlerID == NULL)
            EditFile(name, masks->At(index)->HandlerID);
        else
            *handlerID = masks->At(index)->HandlerID;
    }

    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

BOOL FileNameInvalidForManualCreate(const char* path)
{
    const char* name = strrchr(path, '\\');
    if (name != NULL)
    {
        name++;
        int nameLen = (int)strlen(name);
        return nameLen > 0 && (*name <= ' ' || name[nameLen - 1] <= ' ' || name[nameLen - 1] == '.');
    }
    return FALSE;
}

BOOL MakeValidFileName(char* path)
{
    // oriznuti mezer na zacatku a mezer a tecek na konci jmena, dela to tak Explorer
    // a lidi tlacili, ze to tak taky chteji, viz https://forum.altap.cz/viewtopic.php?f=16&t=5891
    // a https://forum.altap.cz/viewtopic.php?f=2&t=4210
    BOOL ch = FALSE;
    char* n = path;
    while (*n != 0 && *n <= ' ')
        n++;
    if (n > path)
    {
        memmove(path, n, strlen(n) + 1);
        ch = TRUE;
    }
    n = path + strlen(path);
    while (n > path && (*(n - 1) <= ' ' || *(n - 1) == '.'))
        n--;
    if (*n != 0)
    {
        *n = 0;
        ch = TRUE;
    }
    return ch;
}

BOOL CutSpacesFromBothSides(char* path)
{
    // oriznuti mezer na zacatku a na konci jmena
    BOOL ch = FALSE;
    char* n = path;
    while (*n != 0 && *n <= ' ')
        n++;
    if (n > path)
    {
        memmove(path, n, strlen(n) + 1);
        ch = TRUE;
    }
    n = path + strlen(path);
    while (n > path && (*(n - 1) <= ' '))
        n--;
    if (*n != 0)
    {
        *n = 0;
        ch = TRUE;
    }
    return ch;
}

BOOL CutDoubleQuotesFromBothSides(char* path)
{
    int len = (int)strlen(path);
    if (len >= 2 && path[0] == '"' && path[len - 1] == '"')
    {
        memmove(path, path + 1, len - 2);
        path[len - 2] = 0;
        return TRUE;
    }
    return FALSE;
}

void CFilesWindow::CreateDir(CFilesWindow* target)
{
    CALL_STACK_MESSAGE1("CFilesWindow::CreateDir()");
    BeginStopRefresh(); // cmuchal si da pohov

    char path[2 * MAX_PATH], nextFocus[MAX_PATH];
    path[0] = 0;
    nextFocus[0] = 0;

    // obnova DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    if (Is(ptDisk)) // vytvoreni adresare na disku
    {
        CTruncatedString subject;
        subject.Set(LoadStr(IDS_CREATEDIRECTORY_TEXT), NULL);
        CCopyMoveDialog dlg(HWindow, path, MAX_PATH, LoadStr(IDS_CREATEDIRECTORY_TITLE),
                            &subject, IDD_CREATEDIRDIALOG,
                            Configuration.CreateDirHistory, CREATEDIR_HISTORY_SIZE,
                            FALSE);

    CREATE_AGAIN:

        if (dlg.Execute() == IDOK)
        {
            UpdateWindow(MainWindow->HWindow);

            // u diskovych cest preklopime '/' na '\\' a eliminujeme zdvojene backslashe
            SlashesToBackslashesAndRemoveDups(path);

            // ocistime jmeno od nezadoucich znaku na zacatku a konci,
            // delame to jen u posledni komponenty, predesle bud jiz existuji a je to fuk (poresi si
            // to system) nebo se pri vytvareni okontroluji a pripadne se zarve chyba (jen nedojde
            // k ocisteni z nasi strany, at se user taky snazi, je to podprahovka)
            char* lastCompName = strrchr(path, '\\');
            MakeValidFileName(lastCompName != NULL ? lastCompName + 1 : path);

            int errTextID;
            if (!SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus) ||
                strlen(path) >= PATH_MAX_PATH)
            {
                if (strlen(path) >= PATH_MAX_PATH)
                    errTextID = IDS_TOOLONGPATH;
                /* i v pripade prazdneho retezce chceme chybovou hlasku
        if (errTextID == IDS_EMPTYNAMENOTALLOWED)
        {
          EndStopRefresh(); // ted uz zase cmuchal nastartuje
          return; // prazdny string, neni co delat
        }
        */
                SalMessageBox(HWindow, LoadStr(errTextID), LoadStr(IDS_ERRORCREATINGDIR),
                              MB_OK | MB_ICONEXCLAMATION);
                goto CREATE_AGAIN;
            }
            else
            {
                char checkPath[MAX_PATH];
                GetRootPath(checkPath, path);
                if (CheckPath(TRUE, checkPath) != ERROR_SUCCESS)
                    goto CREATE_AGAIN;
                strcpy(checkPath, path);
                CutDirectory(checkPath);
                char newDir[MAX_PATH];
                if (!CheckAndCreateDirectory(checkPath, HWindow, FALSE, NULL, 0, newDir, TRUE, TRUE))
                    goto CREATE_AGAIN;
                if (newDir[0] != 0)
                {
                    // zmena jen v adresari, ve kterem se vytvoril prvni adresar (dalsi adresare neexistovaly, zmenu v nich nema smysl hlasit)
                    CutDirectory(newDir);
                    MainWindow->PostChangeOnPathNotification(newDir, FALSE);
                }

                //---  vytvoreni cesty
                while (1)
                {
                    HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

                    DWORD err;
                    BOOL invalidName = FileNameInvalidForManualCreate(path);
                    if (!invalidName && SalCreateDirectoryEx(path, &err))
                    {
                        SetCursor(oldCur);
                        if (nextFocus[0] != 0)
                            strcpy(NextFocusName, nextFocus);

                        // zmena jen v adresari, ve kterem se vytvoril adresar
                        MainWindow->PostChangeOnPathNotification(checkPath, FALSE);

                        EndStopRefresh(); // ted uz zase cmuchal nastartuje
                        return;
                    }
                    else
                    {
                        if (invalidName)
                            err = ERROR_INVALID_NAME;
                        SetCursor(oldCur);

                        CFileErrorDlg dlg2(HWindow, LoadStr(IDS_ERRORCREATINGDIR), path, GetErrorText(err), FALSE, IDD_ERROR3);
                        dlg2.Execute();
                        goto CREATE_AGAIN;
                    }
                }
            }
        }
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_CREATEDIR)) // v panelu je FS
        {
            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            char newName[2 * MAX_PATH];
            newName[0] = 0;
            BOOL cancel = FALSE;
            BOOL ret = GetPluginFS()->CreateDir(GetPluginFS()->GetPluginFSName(), 1, HWindow, newName, cancel);
            if (!cancel) // nejde o cancel operace
            {
                if (!ret)
                {
                    CTruncatedString subject;
                    subject.Set(LoadStr(IDS_CREATEDIRECTORY_TEXT), NULL);
                    CCopyMoveDialog dlg(HWindow, path, 2 * MAX_PATH, LoadStr(IDS_CREATEDIRECTORY_TITLE),
                                        &subject, IDD_CREATEDIRDIALOG,
                                        Configuration.CreateDirHistory, CREATEDIR_HISTORY_SIZE,
                                        FALSE);
                    while (1)
                    {
                        // otevreme standardni dialog
                        if (dlg.Execute() == IDOK)
                        {
                            strcpy(newName, path);
                            ret = GetPluginFS()->CreateDir(GetPluginFS()->GetPluginFSName(), 2, HWindow, newName, cancel);
                            if (ret || cancel)
                                break; // nejde o chybu (cancel nebo uspech)
                            strcpy(path, newName);
                        }
                        else
                        {
                            WaitForESCRelease();
                            cancel = TRUE;
                            break;
                        }
                    }
                }

                if (ret && !cancel) // uspesne dokoncena operace
                {
                    lstrcpyn(NextFocusName, newName, MAX_PATH); // zajistime fokus noveho jmena po refreshi
                }
            }

            // opet zvysime prioritu threadu, operace dobehla
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        }
    }

    UpdateWindow(MainWindow->HWindow);
    EndStopRefresh(); // ted uz zase cmuchal nastartuje
}

void CFilesWindow::RenameFileInternal(CFileData* f, const char* formatedFileName, BOOL* mayChange, BOOL* tryAgain)
{
    *tryAgain = TRUE;
    const char* s = formatedFileName;
    while (*s != 0 && *s != '\\' && *s != '/' && *s != ':' &&
           *s >= 32 && *s != '<' && *s != '>' && *s != '|' && *s != '"')
        s++;
    if (formatedFileName[0] != 0 && *s == 0)
    {
        char finalName[2 * MAX_PATH];
        MaskName(finalName, 2 * MAX_PATH, f->Name, formatedFileName);

        // ocistime jmeno od nezadoucich znaku na zacatku a konci
        MakeValidFileName(finalName);

        int l = (int)strlen(GetPath());
        char tgtPath[MAX_PATH];
        memmove(tgtPath, GetPath(), l);
        if (GetPath()[l - 1] != '\\')
            tgtPath[l++] = '\\';
        if (strlen(finalName) + l < MAX_PATH && (f->NameLen + l < MAX_PATH ||
                                                 f->DosName != NULL && strlen(f->DosName) + l < MAX_PATH))
        {
            strcpy(tgtPath + l, finalName);
            char path[MAX_PATH];
            strcpy(path, GetPath());
            char* end = path + l;
            if (*(end - 1) != '\\')
                *--end = '\\';
            if (f->NameLen + l < MAX_PATH)
                strcpy(path + l, f->Name);
            else
                strcpy(path + l, f->DosName);

            BOOL ret = FALSE;

            BOOL handsOFF = FALSE;
            CFilesWindow* otherPanel = MainWindow->GetNonActivePanel();
            int otherPanelPathLen = (int)strlen(otherPanel->GetPath());
            int pathLen = (int)strlen(path);
            // menime cestu druhemu panelu?
            if (otherPanelPathLen >= pathLen &&
                StrNICmp(path, otherPanel->GetPath(), pathLen) == 0 &&
                (otherPanelPathLen == pathLen ||
                 otherPanel->GetPath()[pathLen] == '\\'))
            {
                otherPanel->HandsOff(TRUE);
                handsOFF = TRUE;
            }

            *mayChange = TRUE;

            // zkusime prejmenovani nejdrive z dlouheho jmena a v pripade problemu jeste
            // z DOS-jmena (resi soubor/adresare dosazitelne jen pres Unicode nebo DOS-jmena)
            BOOL moveRet = SalMoveFile(path, tgtPath);
            DWORD err = 0;
            if (!moveRet)
            {
                err = GetLastError();
                if ((err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME) &&
                    f->DosName != NULL)
                {
                    strcpy(path + l, f->DosName);
                    moveRet = SalMoveFile(path, tgtPath);
                    if (!moveRet)
                        err = GetLastError();
                    strcpy(path + l, f->Name);
                }
            }

            if (moveRet)
            {

            REN_OPERATION_DONE:

                strcpy(NextFocusName, tgtPath + l);
                ret = TRUE;
            }
            else
            {
                if (StrICmp(path, tgtPath) != 0 && // pokud nejde jen o change-case
                    (err == ERROR_FILE_EXISTS ||   // zkontrolujeme, jestli nejde jen o prepis dosoveho jmena souboru
                     err == ERROR_ALREADY_EXISTS))
                {
                    WIN32_FIND_DATA data;
                    HANDLE find = HANDLES_Q(FindFirstFile(tgtPath, &data));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));
                        const char* tgtName = SalPathFindFileName(tgtPath);
                        if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // shoda jen pro dos name
                            StrICmp(tgtName, data.cFileName) != 0)            // (plne jmeno je jine)
                        {
                            // prejmenujeme ("uklidime") soubor/adresar s konfliktnim dos name do docasneho nazvu 8.3 (nepotrebuje extra dos name)
                            char tmpName[MAX_PATH + 20];
                            lstrcpyn(tmpName, tgtPath, MAX_PATH);
                            CutDirectory(tmpName);
                            SalPathAddBackslash(tmpName, MAX_PATH + 20);
                            char* tmpNamePart = tmpName + strlen(tmpName);
                            char origFullName[MAX_PATH];
                            if (SalPathAppend(tmpName, data.cFileName, MAX_PATH))
                            {
                                strcpy(origFullName, tmpName);
                                DWORD num = (GetTickCount() / 10) % 0xFFF;
                                while (1)
                                {
                                    sprintf(tmpNamePart, "sal%03X", num++);
                                    if (SalMoveFile(origFullName, tmpName))
                                        break;
                                    DWORD e = GetLastError();
                                    if (e != ERROR_FILE_EXISTS && e != ERROR_ALREADY_EXISTS)
                                    {
                                        tmpName[0] = 0;
                                        break;
                                    }
                                }
                                if (tmpName[0] != 0) // pokud se podarilo "uklidit" konfliktni soubor, zkusime presun
                                {                    // souboru znovu, pak vratime "uklizenemu" souboru jeho puvodni jmeno
                                    BOOL moveDone = SalMoveFile(path, tgtPath);
                                    if (!SalMoveFile(tmpName, origFullName))
                                    { // toto se zjevne muze stat, nepochopitelne, ale Windows vytvori misto 'tgtPath' (dos name) soubor se jmenem origFullName
                                        TRACE_I("CFilesWindow::RenameFileInternal(): Unexpected situation: unable to rename file from tmp-name to original long file name! " << origFullName);
                                        if (moveDone)
                                        {
                                            if (SalMoveFile(tgtPath, path))
                                                moveDone = FALSE;
                                            if (!SalMoveFile(tmpName, origFullName))
                                                TRACE_E("CFilesWindow::RenameFileInternal(): Fatal unexpected situation: unable to rename file from tmp-name to original long file name! " << origFullName);
                                        }
                                    }

                                    if (moveDone)
                                        goto REN_OPERATION_DONE;
                                }
                            }
                            else
                                TRACE_E("CFilesWindow::RenameFileInternal(): Original full file name is too long, unable to bypass only-dos-name-overwrite problem!");
                        }
                    }
                }
                if ((err == ERROR_ALREADY_EXISTS ||
                     err == ERROR_FILE_EXISTS) &&
                    StrICmp(path, tgtPath) != 0) // prepsat soubor ?
                {
                    DWORD inAttr = SalGetFileAttributes(path);
                    DWORD outAttr = SalGetFileAttributes(tgtPath);

                    if ((inAttr & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                        (outAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    { // jen pokud jsou oba soubory
                        HANDLE in = HANDLES_Q(CreateFile(path, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                         OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                                         NULL));
                        HANDLE out = HANDLES_Q(CreateFile(tgtPath, 0, FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                          OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL,
                                                          NULL));
                        if (in != INVALID_HANDLE_VALUE && out != INVALID_HANDLE_VALUE)
                        {
                            char iAttr[101], oAttr[101];
                            GetFileOverwriteInfo(iAttr, _countof(iAttr), in, path);
                            GetFileOverwriteInfo(oAttr, _countof(oAttr), out, tgtPath);
                            HANDLES(CloseHandle(in));
                            HANDLES(CloseHandle(out));

                            COverwriteDlg dlg(HWindow, tgtPath, oAttr, path, iAttr, TRUE);
                            int res = (int)dlg.Execute();

                            switch (res)
                            {
                            case IDCANCEL:
                                ret = TRUE;
                            case IDNO:
                                err = ERROR_SUCCESS;
                                break;

                            case IDYES:
                            {
                                ClearReadOnlyAttr(tgtPath); // aby sel smazat ...
                                if (!DeleteFile(tgtPath) || !SalMoveFile(path, tgtPath))
                                    err = GetLastError();
                                else
                                {
                                    err = ERROR_SUCCESS;
                                    ret = TRUE;
                                    strcpy(NextFocusName, tgtPath + l);
                                }
                                break;
                            }
                            }
                        }
                        else
                        {
                            if (in == INVALID_HANDLE_VALUE)
                                TRACE_E("Unable to open file " << path);
                            else
                                HANDLES(CloseHandle(in));
                            if (out == INVALID_HANDLE_VALUE)
                                TRACE_E("Unable to open file " << tgtPath);
                            else
                                HANDLES(CloseHandle(out));
                        }
                    }
                }

                if (err != ERROR_SUCCESS)
                    SalMessageBox(HWindow, GetErrorText(err), LoadStr(IDS_ERRORRENAMINGFILE),
                                  MB_OK | MB_ICONEXCLAMATION);
            }
            if (handsOFF)
                otherPanel->HandsOff(FALSE);
            *tryAgain = !ret;
        }
        else
            SalMessageBox(HWindow, LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_ERRORRENAMINGFILE),
                          MB_OK | MB_ICONEXCLAMATION);
    }
    else
        SalMessageBox(HWindow, GetErrorText(ERROR_INVALID_NAME), LoadStr(IDS_ERRORRENAMINGFILE),
                      MB_OK | MB_ICONEXCLAMATION);
}

void CFilesWindow::RenameFile(int specialIndex)
{
    CALL_STACK_MESSAGE2("CFilesWindow::RenameFile(%d)", specialIndex);

    int i;
    if (specialIndex != -1)
        i = specialIndex;
    else
        i = GetCaretIndex();
    if (i < 0 || i >= Dirs->Count + Files->Count)
        return; // spatny index

    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    if (i == 0 && subDir)
        return; // se ".." nepracujem

    CFileData* f = NULL;
    BOOL isDir = i < Dirs->Count;
    f = isDir ? &Dirs->At(i) : &Files->At(i - Dirs->Count);

    char formatedFileName[MAX_PATH];
    AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

    char buff[200];
    sprintf(buff, LoadStr(IDS_RENAME_TO), LoadStr(isDir ? IDS_QUESTION_DIRECTORY : IDS_QUESTION_FILE));
    CTruncatedString subject;
    subject.Set(buff, formatedFileName);
    CCopyMoveDialog dlg(HWindow, formatedFileName, MAX_PATH, LoadStr(IDS_RENAME_TITLE),
                        &subject, IDD_RENAMEDIALOG, Configuration.QuickRenameHistory,
                        QUICKRENAME_HISTORY_SIZE, FALSE);

    if (Is(ptDisk)) // prejmenovani na disku
    {
#ifndef _WIN64
        if (Windows64Bit && isDir)
        {
            char path[MAX_PATH];
            lstrcpyn(path, GetPath(), MAX_PATH);
            if (SalPathAppend(path, f->Name, MAX_PATH) && IsWin64RedirectedDir(path, NULL, FALSE))
            {
                char msg[300 + MAX_PATH];
                _snprintf_s(msg, _TRUNCATE, LoadStr(IDS_ERRRENAMINGW64ALIAS), f->Name);
                SalMessageBox(MainWindow->HWindow, msg, LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                return;
            }
        }
#endif // _WIN64

        BeginSuspendMode(); // cmuchal si da pohov

        BOOL mayChange = FALSE;
        while (1)
        {
            // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
            char temporarySelected[MAX_PATH];
            SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            // Od Windows Vista MS zavedli velice zadanou vec: quick rename implicitne oznaci pouze nazev bez tecky a pripony
            // stejny kod je zde ctyrikrat
            if (!Configuration.QuickRenameSelectAll)
            {
                int selectionEnd = -1;
                if (!isDir)
                {
                    const char* dot = strrchr(formatedFileName, '.');
                    if (dot != NULL && dot > formatedFileName) // sice plati, ze ".cvspass" ve Windows je pripona, ale Explorer pro ".cvspass" oznacuje cele jmeno, tak to delame take
                                                               //        if (dot != NULL)
                        selectionEnd = (int)(dot - formatedFileName);
                }
                dlg.SetSelectionEnd(selectionEnd);
            }

            int dlgRes = (int)dlg.Execute();

            // pokud jsme nejakou polozku vybrali, zase ji odvyberem
            UnselectItemWithName(temporarySelected);

            if (dlgRes == IDOK)
            {
                UpdateWindow(MainWindow->HWindow);

                BOOL tryAgain;
                RenameFileInternal(f, formatedFileName, &mayChange, &tryAgain);
                if (!tryAgain)
                    break;
            }
            else
                break;
        }

        // refresh neautomaticky refreshovanych adresaru
        if (mayChange)
        {
            // zmena v adresari zobrazenem v panelu a pokud se prejmenovaval adresar, tak i v podadresarich
            MainWindow->PostChangeOnPathNotification(GetPath(), isDir);
        }

        // pokud je aktivni nejaky okno salamandra, konci suspend mode
        EndSuspendMode();
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // v panelu je FS
        {
            BeginSuspendMode(); // cmuchal si da pohov

            // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            char newName[MAX_PATH];
            newName[0] = 0;
            BOOL cancel = FALSE;

            // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
            char temporarySelected[MAX_PATH];
            SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 1, HWindow, *f, isDir, newName, cancel);

            // pokud jsme nejakou polozku vybrali, zase ji odvyberem
            UnselectItemWithName(temporarySelected);

            if (!cancel) // nejde o cancel operace
            {
                if (!ret)
                {
                    while (1)
                    {
                        // otevreme standardni dialog
                        // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
                        SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

                        // Od Windows Vista MS zavedli velice zadanou vec: quick rename implicitne oznaci pouze nazev bez tecky a pripony
                        // stejny kod je zde ctyrikrat
                        if (!Configuration.QuickRenameSelectAll)
                        {
                            int selectionEnd = -1;
                            if (!isDir)
                            {
                                const char* dot = strrchr(formatedFileName, '.');
                                if (dot != NULL && dot > formatedFileName) // sice plati, ze ".cvspass" ve Windows je pripona, ale Explorer pro ".cvspass" oznacuje cele jmeno, tak to delame take
                                                                           //        if (dot != NULL)
                                    selectionEnd = (int)(dot - formatedFileName);
                            }
                            dlg.SetSelectionEnd(selectionEnd);
                        }

                        int dlgRes = (int)dlg.Execute();

                        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
                        UnselectItemWithName(temporarySelected);

                        if (dlgRes == IDOK)
                        {
                            strcpy(newName, formatedFileName);
                            ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 2, HWindow, *f, isDir, newName, cancel);
                            if (ret || cancel)
                                break; // nejde o chybu (cancel nebo uspech)
                            strcpy(formatedFileName, newName);
                        }
                        else
                        {
                            WaitForESCRelease();
                            cancel = TRUE;
                            break;
                        }
                    }
                }

                if (ret && !cancel) // uspesne dokoncena operace
                {
                    strcpy(NextFocusName, newName); // zajistime fokus noveho jmena po refreshi
                }
            }

            // opet zvysime prioritu threadu, operace dobehla
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // pokud je aktivni nejaky okno salamandra, konci suspend mode
            EndSuspendMode();
        }
    }
}

void CFilesWindow::CancelUI()
{
    if (QuickSearchMode)
        EndQuickSearch();
    QuickRenameEnd();
}

BOOL CFilesWindow::IsQuickRenameActive()
{
    return QuickRenameWindow.HWindow != NULL;
}

void CFilesWindow::AdjustQuickRenameRect(const char* text, RECT* r)
{
    // namerim delku textu
    HDC hDC = HANDLES(GetDC(ListBox->HWindow));
    HFONT hOldFont = (HFONT)SelectObject(hDC, Font);
    SIZE sz;
    GetTextExtentPoint32(hDC, text, (int)strlen(text), &sz);
    TEXTMETRIC tm;
    GetTextMetrics(hDC, &tm);
    SelectObject(hDC, hOldFont);
    HANDLES(ReleaseDC(ListBox->HWindow, hDC));

    int minWidth = QuickRenameRect.right - QuickRenameRect.left + 2;
    int minHeight = QuickRenameRect.bottom - QuickRenameRect.top;

    int optimalWidth = sz.cx + 4 + tm.tmHeight;

    r->left--;

    r->right = r->left + optimalWidth;

    if (r->right - r->left < minWidth)
        r->right = r->left + minWidth;

    // nechceme presahnout hranici panelu
    RECT maxR = ListBox->FilesRect;
    if (r->left < maxR.left)
        r->left = maxR.left;
    if (r->right > maxR.right)
        r->right = maxR.right;
}

void CFilesWindow::AdjustQuickRenameWindow()
{
    if (!IsQuickRenameActive())
    {
        //    TRACE_E("QuickRenameWindow is not active.");
        return;
    }

    RECT r;
    GetWindowRect(QuickRenameWindow.HWindow, &r);
    MapWindowPoints(NULL, HWindow, (POINT*)&r, 2);

    char buff[3 * MAX_PATH];
    GetWindowText(QuickRenameWindow.HWindow, buff, 3 * MAX_PATH);
    AdjustQuickRenameRect(buff, &r);
    SetWindowPos(QuickRenameWindow.HWindow, NULL, 0, 0,
                 r.right - r.left, r.bottom - r.top,
                 SWP_NOMOVE | SWP_NOZORDER);
}

/*
// narazil jsem na problem s razenim, vista necha polozky na svem miste, ale salamander je potrebuje zaradit
// takze vec zatim davam k ledu
void
CFilesWindow::QuickRenameOnIndex(int index)
{
  if (index >= 0 && index < Dirs->Count + Files->Count)
  {
    QuickRenameIndex = index;
    SetCaretIndex(index, FALSE);

    RECT r;
    if (ListBox->GetItemRect(index, &r))
    {
      ListBox->GetIndex(r.left, r.top, FALSE, &QuickRenameRect);
      QuickRenameIndex = index;
      QuickRenameBegin(index, &QuickRenameRect);
    }
  }
}
*/

void CFilesWindow::QuickRenameBegin(int index, const RECT* labelRect)
{
    CALL_STACK_MESSAGE2("CFilesWindow::QuickRenameBegin(%d, )", index);

    if (!(Is(ptDisk) || Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
                            GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)))
        return;

    if (QuickRenameWindow.HWindow != NULL)
    {
        TRACE_E("Quick Rename is already active");
        return;
    }

    if (index < 0 || index >= Dirs->Count + Files->Count)
        return; // spatny index

    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    if (index == 0 && subDir)
        return; // se ".." nepracujem

    CFileData* f = NULL;
    BOOL isDir = index < Dirs->Count;
    f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

    char formatedFileName[MAX_PATH];
    AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

    // Od Windows Vista MS zavedli velice zadanou vec: quick rename implicitne oznaci pouze nazev bez tecky a pripony
    // stejny kod je zde ctyrikrat
    int selectionEnd = -1;
    if (!Configuration.QuickRenameSelectAll)
    {
        if (!isDir)
        {
            const char* dot = strrchr(formatedFileName, '.');
            if (dot != NULL && dot > formatedFileName) // sice plati, ze ".cvspass" ve Windows je pripona, ale Explorer pro ".cvspass" oznacuje cele jmeno, tak to delame take
                                                       //    if (dot != NULL)
                selectionEnd = (int)(dot - formatedFileName);
        }
    }

    // pokud jde o FS, musime napred volat QuickRename v mode=1
    // tedy umoznit file systemu otevrit vlastni rename dialog
    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
        GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // v panelu je FS
    {
        BeginSuspendMode(); // cmuchal si da pohov

        // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        char newName[MAX_PATH];
        newName[0] = 0;
        BOOL cancel = FALSE;

        // pokud neni vybrana zadna polozka, vybereme tu pod focusem a ulozime jeji jmeno
        char temporarySelected[MAX_PATH];
        SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

        BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 1, HWindow, *f, isDir, newName, cancel);

        // pokud jsme nejakou polozku vybrali, zase ji odvyberem
        UnselectItemWithName(temporarySelected);

        if (ret && !cancel) // uspesne dokoncena operace
        {
            strcpy(NextFocusName, newName); // zajistime fokus noveho jmena po refreshi
        }

        // opet zvysime prioritu threadu, operace dobehla
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        // pokud je aktivni nejaky okno salamandra, konci suspend mode
        EndSuspendMode();

        if (cancel || ret)
            return;
    }

    RECT r = *labelRect;
    AdjustQuickRenameRect(formatedFileName, &r);

    HWND hWnd = QuickRenameWindow.CreateEx(0,
                                           "edit",
                                           formatedFileName,
                                           WS_BORDER | WS_CHILD | WS_CLIPSIBLINGS | ES_AUTOHSCROLL | ES_LEFT,
                                           r.left, r.top, r.right - r.left, r.bottom - r.top,
                                           GetListBoxHWND(),
                                           NULL,
                                           HInstance,
                                           &QuickRenameWindow);
    if (hWnd == NULL)
    {
        TRACE_E("Cannot create QuickRenameWindow");
        return;
    }

    BeginSuspendMode(TRUE); // cmuchal si da pohov

    // font jako ma panel
    SendMessage(hWnd, WM_SETFONT, (WPARAM)Font, 0);
    int leftMargin = LOWORD(SendMessage(hWnd, EM_GETMARGINS, 0, 0));
    if (leftMargin < 2)
        SendMessage(hWnd, EM_SETMARGINS, EC_LEFTMARGIN, 2);

    //SendMessage(hWnd, EM_SETSEL, 0, -1); // select all
    // umime vyberat pouze nazev bez tecky a pripony
    SendMessage(hWnd, EM_SETSEL, 0, selectionEnd);

    ShowWindow(hWnd, SW_SHOW);
    SetFocus(hWnd);
    return;
}

void CFilesWindow::QuickRenameEnd()
{
    CALL_STACK_MESSAGE1("CFilesWindow::QuickRenameEnd()");
    if (QuickRenameWindow.HWindow != NULL && QuickRenameWindow.GetCloseEnabled())
    {
        // pokud je aktivni nejaky okno salamandra, konci suspend mode
        EndSuspendMode(TRUE);

        // nechceme zacykleni diky WM_KILLFOCUS a spol.
        BOOL old = QuickRenameWindow.GetCloseEnabled();
        QuickRenameWindow.SetCloseEnabled(FALSE);

        DestroyWindow(QuickRenameWindow.HWindow);

        QuickRenameWindow.SetCloseEnabled(old);
    }
}

BOOL CFilesWindow::HandeQuickRenameWindowKey(WPARAM wParam)
{
    CALL_STACK_MESSAGE2("CFilesWindow::HandeQuickRenameWindowKey(0x%IX)", wParam);

    if (wParam == VK_ESCAPE)
    {
        QuickRenameEnd();
        return TRUE;
    }

    int index = GetCaretIndex();
    if (index < 0 || index >= Dirs->Count + Files->Count)
        return TRUE; // spatny index
    CFileData* f = NULL;
    BOOL isDir = index < Dirs->Count;
    f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

    QuickRenameWindow.SetCloseEnabled(FALSE);

    HWND hWnd = QuickRenameWindow.HWindow;
    char newName[MAX_PATH];
    GetWindowText(hWnd, newName, MAX_PATH);

    // snizime prioritu threadu na "normal" (aby operace prilis nezatezovaly stroj)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    BOOL tryAgain = FALSE;
    BOOL mayChange = FALSE;
    if (Is(ptDisk))
    {
        // Pokud jde o in-place rename a uzivatel nazev nezmenil, nemeli bychom se
        // pokouset o prejmenovani, protoze uzivatel muze byt na CD-ROM nebo jinem R/O disku
        // a my potom zobrazovali chybovou hlasku Access is denied. Uzivatel pritom nema
        // mysi moznost operaci Escapnout, takze musi sahnout na Escape klavesu.
        // Explorer se chova presne timto novym zpusobem.
        if (strcmp(f->Name, newName) != 0)
            RenameFileInternal(f, newName, &mayChange, &tryAgain);
    }
    else if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
             GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // v panelu je FS
    {
        // otevreme standardni dialog
        BOOL cancel;
        BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 2, HWindow, *f, isDir, newName, cancel);
        if (!ret && !cancel)
        {
            tryAgain = TRUE;
            SetWindowText(hWnd, newName);
        }
        else
        {
            if (ret && !cancel) // uspesne dokoncena operace
            {
                strcpy(NextFocusName, newName); // zajistime fokus noveho jmena po refreshi
            }
        }
    }

    // opet zvysime prioritu threadu, operace dobehla
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // refresh neautomaticky refreshovanych adresaru
    if (mayChange)
    {
        // zmena v adresari zobrazenem v panelu a pokud se prejmenovaval adresar, tak i v podadresarich
        MainWindow->PostChangeOnPathNotification(GetPath(), isDir);
    }

    QuickRenameWindow.SetCloseEnabled(TRUE);
    if (!tryAgain)
    {
        QuickRenameEnd();
        //    if (wParam == VK_TAB)
        //    {
        //      BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        //      PostMessage(HWindow, WM_USER_RENAME_NEXT_ITEM, !shiftPressed, 0);
        //    }
        return TRUE;
    }
    else
    {
        SetFocus(QuickRenameWindow.HWindow);
        return FALSE;
    }
}

void CFilesWindow::KillQuickRenameTimer()
{
    if (QuickRenameTimer != 0)
    {
        KillTimer(GetListBoxHWND(), QuickRenameTimer);
        QuickRenameTimer = 0;
    }
}

//****************************************************************************
//
// CQuickRenameWindow
//

CQuickRenameWindow::CQuickRenameWindow()
    : CWindow(ooStatic)
{
    FilesWindow = NULL;
    CloseEnabled = TRUE;
    SkipNextCharacter = FALSE;
}

void CQuickRenameWindow::SetPanel(CFilesWindow* filesWindow)
{
    FilesWindow = filesWindow;
}

void CQuickRenameWindow::SetCloseEnabled(BOOL closeEnabled)
{
    CloseEnabled = closeEnabled;
}

BOOL CQuickRenameWindow::GetCloseEnabled()
{
    return CloseEnabled;
}

LRESULT
CQuickRenameWindow::WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_CHAR:
    {
        if (SkipNextCharacter)
        {
            SkipNextCharacter = FALSE; // zamezime pipnuti
            return FALSE;
        }

        if (wParam == VK_ESCAPE || wParam == VK_RETURN /*|| wParam == VK_TAB*/)
        {
            FilesWindow->HandeQuickRenameWindowKey(wParam);
            return 0;
        }
        break;
    }

    case WM_KEYDOWN:
    {
        if (wParam == 'A')
        {
            // od Windows Vista uz SelectAll standardne funguje, takze tam nechame select all na nich
            if (!WindowsVistaAndLater)
            {
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (controlPressed && !shiftPressed && !altPressed)
                {
                    SendMessage(HWindow, EM_SETSEL, 0, -1);
                    SkipNextCharacter = TRUE; // zamezime pipnuti
                    return 0;
                }
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
