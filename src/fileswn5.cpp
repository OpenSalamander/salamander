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
    BeginStopRefresh(); // He was snoring in his sleep

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
                    break; // nothing to do

                CCriteriaData filter;
                filter.UseMasks = TRUE;
                filter.Masks.SetMasksString(convertDlg.Mask);
                int errpos = 0;
                if (!filter.Masks.PrepareMasks(errpos))
                    break; // incorrect mask

                if (CheckPath(TRUE) != ERROR_SUCCESS) // The path has been laid out for us to work on
                {
                    FilesActionInProgress = FALSE;
                    EndStopRefresh(); // now he's sniffling again, he'll start up
                    return;
                }

                CConvertData dlgData;

                dlgData.EOFType = convertDlg.EOFType;

                // Object CodeTables has already been initialized in the Convert dialog
                if (!CodeTables.GetCode(dlgData.CodeTable, convertDlg.CodeType))
                {
                    // if it is not possible to obtain the encoding table or no encoding is selected,
                    // we will perform one-to-one encoding - that is, none
                    int i;
                    for (i = 0; i < 256; i++)
                        dlgData.CodeTable[i] = i;
                }

                //--- finding out which directories and files are marked
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
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;
                    }
                    GetSelItems(count, indexes);
                }
                else // nothing is marked
                {
                    int i = GetCaretIndex();
                    if (i < 0 || i >= Dirs->Count + Files->Count)
                    {
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;           // bad index (no files)
                    }
                    if (i == 0 && subDir)
                    {
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;           // I do not work with ".."
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
                    // Swapped to enable the main window activation (must not be disabled), otherwise switches to another app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();

                    // if Salamander is active, we call SetFocus on the remembered window (SetFocus does not work)
                    // if the main window is disabled - after deactivation/activation of the disabled main window, the active panel
                    // no focus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    // Prepare a refresh of non-automatically refreshed directories
                    // change in the directory displayed in the panel and if work was also done in subdirectories, then also in subdirectories
                    script->SetWorkPath1(GetPath(), convertDlg.SubDirs);

                    if (!res || !StartProgressDialog(script, LoadStr(IDS_CONVERTTITLE), NULL, &dlgData))
                    {
                        if (script->IsGood() && script->Count == 0)
                        {
                            SalMessageBox(HWindow, LoadStr(IDS_NOFILE_MATCHEDMASK), LoadStr(IDS_INFOTITLE),
                                          MB_OK | MB_ICONINFORMATION);

                            // none of the files had to pass through the mask filter
                            SetSel(FALSE, -1, TRUE);                        // explicit override
                            PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        }
                        UpdateWindow(MainWindow->HWindow);
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    else // Removing the selected index (we do not wait for the operation to finish, it is running in another thread)
                    {
                        SetSel(FALSE, -1, TRUE);                        // explicit override
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
    EndStopRefresh(); // now he's sniffling again, he'll start up
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
        // focus on UpDir -- nothing to convert
        if (Dirs->Count > 0 && index == 0 && strcmp(Dirs->At(0).Name, "..") == 0)
            return;
    }
    BeginStopRefresh(); // He was snoring in his sleep

    // if no item is selected, we will select the one under focus and save its name
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

                            // get file times
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
                                // if we failed to extract the time from the file, we will at least use the one we have
                                FILETIME ft;
                                if (!FileTimeToLocalFileTime(&f->LastWrite, &ft) ||
                                    !FileTimeToSystemTime(&ft, &timeModified))
                                {
                                    GetLocalTime(&timeModified); // the time we have is invalid, we take the current time
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
                    chDlg.Encrypted = compressed ? 0 : 2; // Compression excludes encryption: without compression, encryption can remain as it is
                }
                else
                {
                    chDlg.Compressed = encrypted ? 0 : 2; // Encryption excludes compression: without encryption, compression can remain as it is
                    chDlg.Encrypted = encrypted;
                }
                chDlg.ChangeTimeModified = FALSE;
                chDlg.ChangeTimeCreated = FALSE;
                chDlg.ChangeTimeAccessed = FALSE;
                chDlg.RecurseSubDirs = TRUE;

                if (setCompress && Configuration.CnfrmNTFSPress || // ask if compression/decompression should be performed
                    setEncryption && Configuration.CnfrmNTFSCrypt) // ask if encryption/decryption should be performed
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
                        // if we have selected an item, we will deselect it again
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
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
                    // if we have selected an item, we will deselect it again
                    UnselectItemWithName(temporarySelected);
                    FilesActionInProgress = FALSE;
                    EndStopRefresh(); // now he's sniffling again, he'll start up
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
                //--- finding out which directories and files are marked
                int* indexes = NULL;
                CFileData* f = NULL;
                int count = GetSelCount();
                if (count != 0)
                {
                    indexes = new int[count];
                    if (indexes == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        // if we have selected an item, we will deselect it again
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;
                    }
                    GetSelItems(count, indexes);
                }
                else // nothing is marked
                {
                    int i = GetCaretIndex();
                    if (i < 0 || i >= Dirs->Count + Files->Count)
                    {
                        // if we have selected an item, we will deselect it again
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;           // bad index (no files)
                    }
                    if (i == 0 && subDir)
                    {
                        // if we have selected an item, we will deselect it again
                        UnselectItemWithName(temporarySelected);
                        FilesActionInProgress = FALSE;
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                        return;           // I do not work with ".."
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

                    // Ensuring a correct relationship between Compressed and Encrypted
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
                    // Swapped to enable the main window activation (must not be disabled), otherwise switches to another app
                    EnableWindow(MainWindow->HWindow, TRUE);
                    DestroySafeWaitWindow();

                    // if Salamander is active, we call SetFocus on the remembered window (SetFocus does not work)
                    // if the main window is disabled - after deactivation/activation of the disabled main window, the active panel
                    // no focus)
                    HWND hwnd = GetForegroundWindow();
                    while (hwnd != NULL && hwnd != MainWindow->HWindow)
                        hwnd = GetParent(hwnd);
                    if (hwnd == MainWindow->HWindow)
                        SetFocus(hFocusedWnd);

                    SetCursor(oldCur);

                    // Prepare a refresh of non-automatically refreshed directories
                    // change in the directory displayed in the panel and if work was also done in subdirectories, then also in subdirectories
                    script->SetWorkPath1(GetPath(), chDlg.RecurseSubDirs);

                    if (!res || !StartProgressDialog(script, LoadStr(IDS_CHANGEATTRSTITLE), &dlgData, NULL))
                    {
                        UpdateWindow(MainWindow->HWindow);
                        if (!script->IsGood())
                            script->ResetState();
                        FreeScript(script);
                    }
                    else // Removing the selected index (we do not wait for the operation to finish, it is running in another thread)
                    {
                        SetSel(FALSE, -1, TRUE);                        // explicit override
                        PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                        UpdateWindow(MainWindow->HWindow);
                    }
                }
                //---
                if (indexes != NULL)
                    delete[] (indexes);
                //--- if any Salamander window is active, exit suspend mode
            }
            UpdateWindow(MainWindow->HWindow);
            FilesActionInProgress = FALSE;
        }
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_CHANGEATTRS)) // FS is in the panel
        {
            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            int panel = MainWindow->LeftPanel == this ? PANEL_LEFT : PANEL_RIGHT;

            int count = GetSelCount();
            int selectedDirs = 0;
            if (count > 0)
            {
                // we will count how many directories are marked (the rest of the marked items are files)
                int i;
                for (i = 0; i < Dirs->Count; i++) // ".." cannot be marked, the test would be pointless
                {
                    if (Dirs->At(i).Selected)
                        selectedDirs++;
                }
            }
            else
                count = 0;

            BOOL success = GetPluginFS()->ChangeAttributes(GetPluginFS()->GetPluginFSName(), HWindow,
                                                           panel, count - selectedDirs, selectedDirs);

            // increase the thread priority again, operation completed
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            if (success) // success -> unselect
            {
                SetSel(FALSE, -1, TRUE);                        // explicit override
                PostMessage(HWindow, WM_USER_SELCHANGED, 0, 0); // sel-change notify
                UpdateWindow(MainWindow->HWindow);
            }
        }
    }
    // if we have selected an item, we will deselect it again
    UnselectItemWithName(temporarySelected);

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

void CFilesWindow::FindFile()
{
    CALL_STACK_MESSAGE1("CFilesWindow::FindFile()");
    if (Is(ptDisk) && CheckPath(TRUE) != ERROR_SUCCESS)
        return;

    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
        GetPluginFS()->IsServiceSupported(FS_SERVICE_OPENFINDDLG))
    { // Let's try to open Find for FS in the panel, if successful, there is no point in opening the standard Find anymore
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
    // we will verify if the file is on an accessible path
    char path[MAX_PATH + 10];
    if (name == NULL) // file from panel
    {
        if (Is(ptDisk) || Is(ptZIPArchive))
        {
            if (CheckPath(TRUE) != ERROR_SUCCESS)
                return;
        }
    }
    else // file from the Windows path (Find + Alt+F11)
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
    // when it comes to view/edit from the panel, we get the full long name
    BOOL useDiskCache = FALSE;          // TRUE only for ZIP - uses disk cache
    BOOL arcCacheCacheCopies = TRUE;    // Cache copies in disk cache, unless the archiver plugin specifies otherwise
    char dcFileName[3 * MAX_PATH + 50]; // ZIP: name for disk cache
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
                // Let's try if the file name is valid, if not, we'll try its DOS name
                // (resi soubory dosazitelne jen pres Unicode nebo DOS-jmena)
                if (f->DosName != NULL && SalGetFileAttributes(path) == 0xffffffff)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME)
                    {
                        if (strlen(f->DosName) + (s - path) < MAX_PATH)
                        {
                            strcpy(s, f->DosName);
                            if (SalGetFileAttributes(path) == 0xffffffff) // still error -> returning to long-name
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
                    StrICpy(dcFileName, GetZIPArchive()); // The name of the archive file should be compared "case-insensitive" (Windows file system), so we will always convert it to lowercase
                    if (GetZIPPath()[0] != 0)
                    {
                        if (GetZIPPath()[0] != '\\')
                            strcat(dcFileName, "\\");
                        strcat(dcFileName, GetZIPPath());
                    }
                    if (dcFileName[strlen(dcFileName) - 1] != '\\')
                        strcat(dcFileName, "\\");
                    strcat(dcFileName, f->Name);

                    // setting disk cache for plugin (default values are changed only for the plugin)
                    char arcCacheTmpPath[MAX_PATH];
                    arcCacheTmpPath[0] = 0;
                    BOOL arcCacheOwnDelete = FALSE;
                    CPluginInterfaceAbstract* plugin = NULL; // != NULL if the plugin has its own deletion
                    int format = PackerFormatConfig.PackIsArchive(GetZIPArchive());
                    if (format != 0) // We found a supported archive
                    {
                        format--;
                        int index = PackerFormatConfig.GetUnpackerIndex(format);
                        if (index < 0) // view: is it internal processing (plug-in)?
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

                    // we compare the file with all others and look for a case-sensitive matching name;
                    // if it exists, we need to distinguish these two files in the disk cache, I chose
                    // allocated address Name - in opposite panels with the same archive, the disk cache will not be used,
                    // but given the unlikelihood of this case, it's still good until then
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

                    if (!exists) // we need to unpack it
                    {
                        char* backSlash = strrchr(name, '\\');
                        char tmpPath[MAX_PATH];
                        memcpy(tmpPath, name, backSlash - name);
                        tmpPath[backSlash - name] = 0;
                        BeginStopRefresh(); // He was snoring in his sleep
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
                                SalGetFileSize(file, size, err); // ignore errors, file size is not that important
                                HANDLES(CloseHandle(file));
                            }
                            DiskCache.NamePrepared(dcFileName, size);
                        }
                        else
                        {
                            SetCursor(oldCur);
                            if (renamingNotSupported) // to prevent the same sound from being duplicated in a pile of plugins, we will print it here for everyone
                            {
                                SalMessageBox(HWindow, LoadStr(IDS_UNPACKINVNAMERENUNSUP),
                                              LoadStr(IDS_ERRORTITLE), MB_OK | MB_ICONEXCLAMATION);
                            }
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                            DiskCache.ReleaseName(dcFileName, FALSE); // not unpacked, nothing to cache
                            EndStopRefresh();                         // now he's sniffling again, he'll start up
                            return;
                        }
                        EndStopRefresh(); // now he's sniffling again, he'll start up
                    }
                }
                else
                {
                    if (Is(ptPluginFS))
                    {
                        if (GetPluginFS()->NotEmpty() && // FS is okay and supports view-file
                            GetPluginFS()->IsServiceSupported(FS_SERVICE_VIEWFILE))
                        {
                            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

                            CSalamanderForViewFileOnFS sal(altView, handlerID);
                            GetPluginFS()->ViewFile(GetPluginFS()->GetPluginFSName(), HWindow, &sal, *f);

                            // increase the thread priority again, operation completed
                            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
                        }
                        return; // view on the file system is ready
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
        if (lock != NULL) // we will ensure the binding between the viewer and the disk cache
        {
            DiskCache.AssignName(dcFileName, lock, lockOwner, arcCacheCacheCopies ? crtCache : crtDirect);
        }
        else // The viewer did not open or just does not have a "lock" object - let's try at least leaving the file in disk cache
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

    // obtaining the full DOS name
    char dosName[MAX_PATH];
    if (GetShortPathName(name, dosName, MAX_PATH) == 0)
    {
        TRACE_E("GetShortPathName() failed");
        dosName[0] = 0;
    }

    // find the file name and check if it has an extension - necessary for masks
    const char* namePart = strrchr(name, '\\');
    if (namePart == NULL)
    {
        TRACE_E("Invalid parameter for ViewFileInt(): " << name);
        return FALSE;
    }
    namePart++;
    const char* tmpExt = strrchr(namePart, '.');
    //if (tmpExt == NULL || tmpExt == namePart) tmpExt = namePart + lstrlen(namePart); // ".cvspass" is not an extension ...
    if (tmpExt == NULL)
        tmpExt = namePart + lstrlen(namePart); // ".cvspass" in Windows is an extension ...
    else
        tmpExt++;

    // position for viewery
    WINDOWPLACEMENT place;
    place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(MainWindow->HWindow, &place);
    // GetWindowPlacement reads Taskbar, so if the Taskbar is at the top or on the left,
    // the values are shifted by its dimensions. We will perform the correction.
    RECT monitorRect;
    RECT workRect;
    MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
    OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
               workRect.top - monitorRect.top);

    // if we are called for example from find and the main window is minimized,
    // we do not want a minimized viewer
    if (place.showCmd == SW_MINIMIZE || place.showCmd == SW_SHOWMINIMIZED ||
        place.showCmd == SW_SHOWMINNOACTIVE)
        place.showCmd = SW_SHOWNORMAL;

    // find the correct viewer and start it
    CViewerMasks* masks = (altView ? MainWindow->AltViewerMasks : MainWindow->ViewerMasks);
    CViewerMasksItem* viewer = NULL;

    if (handlerID != 0xFFFFFFFF)
    {
        // try to find a viewer with a matching HandlerID
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
                    { // only plug-in viewers
                        CPluginData* plugin = Plugins.Get(-viewer->ViewerType - 1);
                        if (plugin != NULL && plugin->SupportViewer)
                        {
                            if (!plugin->CanViewFile(name))
                                continue; // Let's try to find another viewer, this one is not working
                        }
                        else
                            TRACE_E("Unexpected error (before CanViewFile) in (Alt)ViewerMasks (invalid ViewerType).");
                    }
                    break; // all OK, let's open the viewer
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
            MainWindow->FileHistory->AddFile(fhitView, viewer->HandlerID, name); // add file to history

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
                AddDoubleQuotesIfNeeded(cmdLine, 2 * MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)
                int len = (int)strlen(cmdLine);
                int lArgs = (int)strlen(expArguments);
                if (len + lArgs + 2 <= 2 * MAX_PATH)
                {
                    cmdLine[len] = ' ';
                    memcpy(cmdLine + len + 1, expArguments, lArgs + 1);

                    MainWindow->SetDefaultDirectories();

                    if (expInitDir[0] == 0) // It shouldn't happen at all
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
                // GetWindowPlacement reads Taskbar, so if the Taskbar is at the top or on the left,
                // the values are shifted by its dimensions. We will perform the correction.
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

    // we will verify if the file is on an accessible path
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

    // when it comes to view/edit from the panel, we get the full long name
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
                // Let's try if the file name is valid, if not, we'll try its DOS name
                // (resi soubory dosazitelne jen pres Unicode nebo DOS-jmena)
                if (f->DosName != NULL && SalGetFileAttributes(path) == 0xffffffff)
                {
                    DWORD err = GetLastError();
                    if (err == ERROR_FILE_NOT_FOUND || err == ERROR_INVALID_NAME)
                    {
                        if (strlen(f->DosName) + (s - path) < MAX_PATH)
                        {
                            strcpy(s, f->DosName);
                            if (SalGetFileAttributes(path) == 0xffffffff) // still error -> returning to long-name
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

    // obtaining the full DOS name
    char dosName[MAX_PATH];
    if (GetShortPathName(name, dosName, MAX_PATH) == 0)
    {
        TRACE_I("GetShortPathName() failed.");
        dosName[0] = 0;
    }

    // find the file name and check if it has an extension - necessary for masks
    char* namePart = strrchr(name, '\\');
    if (namePart == NULL)
    {
        TRACE_E("Invalid parameter CFilesWindow::EditFile(): " << name);
        return;
    }
    namePart++;
    char* tmpExt = strrchr(namePart, '.');
    //if (tmpExt == NULL || tmpExt == namePart) tmpExt = namePart + lstrlen(namePart); // ".cvspass" is not an extension ...
    if (tmpExt == NULL)
        tmpExt = namePart + lstrlen(namePart); // ".cvspass" in Windows is an extension ...
    else
        tmpExt++;

    // position for editors
    WINDOWPLACEMENT place;
    place.length = sizeof(WINDOWPLACEMENT);
    GetWindowPlacement(MainWindow->HWindow, &place);
    // GetWindowPlacement reads Taskbar, so if the Taskbar is at the top or on the left,
    // the values are shifted by its dimensions. We will perform the correction.
    RECT monitorRect;
    RECT workRect;
    MultiMonGetClipRectByRect(&place.rcNormalPosition, &workRect, &monitorRect);
    OffsetRect(&place.rcNormalPosition, workRect.left - monitorRect.left,
               workRect.top - monitorRect.top);
    // if we are called for example from find and the main window is minimized,
    // We do not want a minimized editor
    if (place.showCmd == SW_MINIMIZE || place.showCmd == SW_SHOWMINIMIZED ||
        place.showCmd == SW_SHOWMINNOACTIVE)
        place.showCmd = SW_SHOWNORMAL;

    // find the correct editor and run it
    CEditorMasks* masks = MainWindow->EditorMasks;

    CEditorMasksItem* editor = NULL;

    if (handlerID != 0xFFFFFFFF)
    {
        // Trying to find an editor with the corresponding HandlerID
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
            MainWindow->FileHistory->AddFile(fhitEdit, editor->HandlerID, name); // add file to history

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
            AddDoubleQuotesIfNeeded(cmdLine, 2 * MAX_PATH); // CreateProcess wants the name with spaces in quotes (otherwise it tries various variants, see help)
            int len = (int)strlen(cmdLine);
            int lArgs = (int)strlen(expArguments);
            if (len + lArgs + 2 <= 2 * MAX_PATH)
            {
                cmdLine[len] = ' ';
                memcpy(cmdLine + len + 1, expArguments, lArgs + 1);

                MainWindow->SetDefaultDirectories();

                if (expInitDir[0] == 0) // It shouldn't happen at all
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
    BeginStopRefresh(); // He was snoring in his sleep

    // Restore DefaultDir
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

        // Some people always want to create .txt files and are satisfied with just changing the extension; some people create various files and want to overwrite the entire name.
        // So we backed down and introduced a custom item for Edit New File in the configuration.
        // ------------------
        // For EditNew, there is no "smart" selection, only the name makes sense, because people also change the extension, see our forum:
        // https://forum.altap.cz/viewtopic.php?t=2655
        // -----------------
        // Since Windows Vista, Microsoft introduced a very useful feature: quick rename implicitly selects only the name without the dot and extension
        // the same code is here four times
        if (!Configuration.EditNewSelectAll)
        {
            int selectionEnd = -1;
            if (first)
            {
                const char* dot = strrchr(path, '.');
                if (dot != NULL && dot > path) // While it is true that ".cvspass" in Windows is an extension, Explorer marks the entire name for ".cvspass", so we do the same
                                               //      if (dot != NULL)
                    selectionEnd = (int)(dot - path);
                dlg.SetSelectionEnd(selectionEnd);
                first = FALSE; // After an error, we will receive the full file name, so we will mark it completely
            }
        }

        if (dlg.Execute() == IDOK)
        {
            UpdateWindow(MainWindow->HWindow);

            // clean the name from unwanted characters at the beginning and end,
            // we are doing this only at the last component, the previous ones either already exist and it doesn't matter (it will take care of it
            // to the system) or are checked during creation and an error is thrown if necessary (just in case
            // to clean up on our part, let the user also make an effort, it's subconscious)
            char* lastCompName = strrchr(path, '\\');
            MakeValidFileName(lastCompName != NULL ? lastCompName + 1 : path);

            char* errText;
            int errTextID;
            //      if (SalGetFullName(path, &errTextID, MainWindow->GetActivePanel()->Is(ptDisk) ?
            //                         MainWindow->GetActivePanel()->GetPath() : NULL, NextFocusName))
            if (SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, NextFocusName)) // for consistency with ChangePathToDisk()
            {
                char checkPath[MAX_PATH];
                strcpy(checkPath, path);
                CutDirectory(checkPath);
                if (CheckPath(TRUE, checkPath) != ERROR_SUCCESS)
                {
                    EndStopRefresh(); // now he's sniffling again, he'll start up
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

                    // change only in the directory where the file was created
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
    EndStopRefresh(); // now he's sniffling again, he'll start up
}

// based on available viewers, fill the popup
void CFilesWindow::FillViewWithMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillViewWithMenu()");

    // Remove existing items
    popup->RemoveAllItems();

    // retrieve a list of viewer indexes
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
            if (plugin->PluginIconIndex != -1) // plugin has an icon
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
    // merge will happen through both normal and alternate viewers
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

            BOOL alreadyAdded = FALSE; // We do not want one item to appear multiple times in the list

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
    BeginStopRefresh(); // He was snoring in his sleep

    // retrieve a list of viewer indexes
    TDirectArray<CViewerMasksItem*> items(50, 10);
    if (!FillViewWithData(&items))
    {
        EndStopRefresh(); // now he's sniffling again, he'll start up
        return;
    }

    if (index < 0 || index >= items.Count)
    {
        TRACE_E("index=" << index);
        EndStopRefresh(); // now he's sniffling again, he'll start up
        return;
    }

    ViewFile(NULL, FALSE, items[index]->HandlerID, Is(ptDisk) ? EnumFileNamesSourceUID : -1,
             -1 /* index by focus*/);

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

void CFilesWindow::ViewFileWith(char* name, HWND hMenuParent, const POINT* menuPoint, DWORD* handlerID,
                                int enumFileNamesSourceUID, int enumFileNamesLastFileIndex)
{
    CALL_STACK_MESSAGE5("CFilesWindow::ViewFileWith(%s, , , %s, %d, %d)", name,
                        (handlerID == NULL ? "NULL" : "non-NULL"), enumFileNamesSourceUID,
                        enumFileNamesLastFileIndex);
    BeginStopRefresh(); // He was snoring in his sleep
    if (handlerID != NULL)
        *handlerID = 0xFFFFFFFF;

    CMenuPopup contextPopup(CML_FILES_VIEWWITH);
    FillViewWithMenu(&contextPopup);
    DWORD cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                   menuPoint->x, menuPoint->y, hMenuParent, NULL);
    if (cmd >= CM_VIEWWITH_MIN && cmd <= CM_VIEWWITH_MAX)
    {
        // retrieve a list of viewer indexes
        TDirectArray<CViewerMasksItem*> items(50, 10);
        if (!FillViewWithData(&items))
        {
            EndStopRefresh(); // now he's sniffling again, he'll start up
            return;
        }

        int index = cmd - CM_VIEWWITH_MIN;
        if (handlerID == NULL)
            ViewFile(name, FALSE, items[index]->HandlerID, enumFileNamesSourceUID, enumFileNamesLastFileIndex);
        else
            *handlerID = items[index]->HandlerID;
    }

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

void CFilesWindow::FillEditWithMenu(CMenuPopup* popup)
{
    CALL_STACK_MESSAGE1("CFilesWindow::FillEditWithMenu()");

    // Remove existing items
    popup->RemoveAllItems();

    // retrieve a list of editor indexes
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

        // if a user has multiple rows of masks associated with one viewer/editor,
        // add item to the list only once
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
    BeginStopRefresh(); // He was snoring in his sleep

    // retrieve a list of viewer indexes
    // retrieve a list of editor indexes
    CEditorMasks* masks = MainWindow->EditorMasks;

    if (index < 0 || index >= masks->Count)
    {
        TRACE_E("index=" << index);
        EndStopRefresh(); // now he's sniffling again, he'll start up
        return;
    }

    EditFile(NULL, masks->At(index)->HandlerID);

    EndStopRefresh(); // now he's sniffling again, he'll start up
}

void CFilesWindow::EditFileWith(char* name, HWND hMenuParent, const POINT* menuPoint, DWORD* handlerID)
{
    CALL_STACK_MESSAGE3("CFilesWindow::EditFileWith(%s, , , %s)", name,
                        (handlerID == NULL ? "NULL" : "non-NULL"));
    BeginStopRefresh(); // He was snoring in his sleep
    if (handlerID != NULL)
        *handlerID = 0xFFFFFFFF;

    // retrieve a list of editor indexes
    CEditorMasks* masks = MainWindow->EditorMasks;

    // create menu
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

    EndStopRefresh(); // now he's sniffling again, he'll start up
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
    // Trimming spaces at the beginning and spaces and dots at the end of the name, Explorer does that
    // and people were pushing for it, as they also want it, see https://forum.altap.cz/viewtopic.php?f=16&t=5891
    // and https://forum.altap.cz/viewtopic.php?f=2&t=4210
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
    // Trimming spaces at the beginning and end of a name
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
    BeginStopRefresh(); // He was snoring in his sleep

    char path[2 * MAX_PATH], nextFocus[MAX_PATH];
    path[0] = 0;
    nextFocus[0] = 0;

    // Restore DefaultDir
    MainWindow->UpdateDefaultDir(MainWindow->GetActivePanel() == this);

    if (Is(ptDisk)) // creating a directory on disk
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

            // In disk paths, we switch '/' to '\\' and eliminate double backslashes
            SlashesToBackslashesAndRemoveDups(path);

            // clean the name from unwanted characters at the beginning and end,
            // we are doing this only at the last component, the previous ones either already exist and it doesn't matter (it will take care of it
            // to the system) or are checked during creation and an error is thrown if necessary (just in case
            // to clean up on our part, let the user also make an effort, it's subconscious)
            char* lastCompName = strrchr(path, '\\');
            MakeValidFileName(lastCompName != NULL ? lastCompName + 1 : path);

            int errTextID;
            if (!SalGetFullName(path, &errTextID, Is(ptDisk) ? GetPath() : NULL, nextFocus) ||
                strlen(path) >= PATH_MAX_PATH)
            {
                if (strlen(path) >= PATH_MAX_PATH)
                    errTextID = IDS_TOOLONGPATH;
                /* and in case of an empty string we want an error message
        if (errTextID == IDS_EMPTYNAMENOTALLOWED)
        {
          EndStopRefresh(); // now cmuchal will start again
          return; // empty string, nothing to do
        }*/
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
                    // change only in the directory where the first directory was created (subdirectories did not exist, reporting changes in them does not make sense)
                    CutDirectory(newDir);
                    MainWindow->PostChangeOnPathNotification(newDir, FALSE);
                }

                //--- creating path
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

                        // change only in the directory where the directory was created
                        MainWindow->PostChangeOnPathNotification(checkPath, FALSE);

                        EndStopRefresh(); // now he's sniffling again, he'll start up
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
            GetPluginFS()->IsServiceSupported(FS_SERVICE_CREATEDIR)) // FS is in the panel
        {
            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            char newName[2 * MAX_PATH];
            newName[0] = 0;
            BOOL cancel = FALSE;
            BOOL ret = GetPluginFS()->CreateDir(GetPluginFS()->GetPluginFSName(), 1, HWindow, newName, cancel);
            if (!cancel) // it's not a cancel operation
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
                        // open standard dialog
                        if (dlg.Execute() == IDOK)
                        {
                            strcpy(newName, path);
                            ret = GetPluginFS()->CreateDir(GetPluginFS()->GetPluginFSName(), 2, HWindow, newName, cancel);
                            if (ret || cancel)
                                break; // it's not an error (cancel or success)
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

                if (ret && !cancel) // Operation successfully completed
                {
                    lstrcpyn(NextFocusName, newName, MAX_PATH); // Ensure focus of the new name after refresh
                }
            }

            // increase the thread priority again, operation completed
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
        }
    }

    UpdateWindow(MainWindow->HWindow);
    EndStopRefresh(); // now he's sniffling again, he'll start up
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

        // clean the name from unwanted characters at the beginning and end
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
            // Are we changing the path to the second panel?
            if (otherPanelPathLen >= pathLen &&
                StrNICmp(path, otherPanel->GetPath(), pathLen) == 0 &&
                (otherPanelPathLen == pathLen ||
                 otherPanel->GetPath()[pathLen] == '\\'))
            {
                otherPanel->HandsOff(TRUE);
                handsOFF = TRUE;
            }

            *mayChange = TRUE;

            // we will try renaming first from a long name and in case of problems yet
            // from DOS name (handles files/directories accessible only through Unicode or DOS names)
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
                if (StrICmp(path, tgtPath) != 0 && // if it's not just about change-case
                    (err == ERROR_FILE_EXISTS ||   // we will check if it's just a rewrite of the DOS file name
                     err == ERROR_ALREADY_EXISTS))
                {
                    WIN32_FIND_DATA data;
                    HANDLE find = HANDLES_Q(FindFirstFile(tgtPath, &data));
                    if (find != INVALID_HANDLE_VALUE)
                    {
                        HANDLES(FindClose(find));
                        const char* tgtName = SalPathFindFileName(tgtPath);
                        if (StrICmp(tgtName, data.cAlternateFileName) == 0 && // match only for two names
                            StrICmp(tgtName, data.cFileName) != 0)            // (full name is different)
                        {
                            // rename ("clean up") the file/directory with a conflicting long name to a temporary 8.3 name (does not require an extra long name)
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
                                if (tmpName[0] != 0) // if the conflict file has been "cleaned up", we will try to move
                                {                    // file again, then we return the "cleaned up" file its original name
                                    BOOL moveDone = SalMoveFile(path, tgtPath);
                                    if (!SalMoveFile(tmpName, origFullName))
                                    { // this can obviously happen, incomprehensible, but Windows will create a file with the name 'tgtPath' (dos name) named origFullName
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
                    StrICmp(path, tgtPath) != 0) // overwrite file?
                {
                    DWORD inAttr = SalGetFileAttributes(path);
                    DWORD outAttr = SalGetFileAttributes(tgtPath);

                    if ((inAttr & FILE_ATTRIBUTE_DIRECTORY) == 0 &&
                        (outAttr & FILE_ATTRIBUTE_DIRECTORY) == 0)
                    { // only if both files
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
                                ClearReadOnlyAttr(tgtPath); // to be deleted ...
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
        return; // bad index

    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    if (i == 0 && subDir)
        return; // I do not work with ".."

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

    if (Is(ptDisk)) // renaming on disk
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

        BeginSuspendMode(); // He was snoring in his sleep

        BOOL mayChange = FALSE;
        while (1)
        {
            // if no item is selected, we will select the one under focus and save its name
            char temporarySelected[MAX_PATH];
            SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            // Since Windows Vista, Microsoft introduced a very useful feature: quick rename implicitly selects only the name without the dot and extension
            // the same code is here four times
            if (!Configuration.QuickRenameSelectAll)
            {
                int selectionEnd = -1;
                if (!isDir)
                {
                    const char* dot = strrchr(formatedFileName, '.');
                    if (dot != NULL && dot > formatedFileName) // While it is true that ".cvspass" in Windows is an extension, Explorer marks the entire name for ".cvspass", so we do the same
                                                               //        if (dot != NULL)
                        selectionEnd = (int)(dot - formatedFileName);
                }
                dlg.SetSelectionEnd(selectionEnd);
            }

            int dlgRes = (int)dlg.Execute();

            // if we have selected an item, we will deselect it again
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

        // refresh non-automatically refreshed directories
        if (mayChange)
        {
            // change in the directory displayed in the panel and if the directory was renamed, also in subdirectories
            MainWindow->PostChangeOnPathNotification(GetPath(), isDir);
        }

        // if any Salamander window is active, exit suspend mode
        EndSuspendMode();
    }
    else
    {
        if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
            GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // FS is in the panel
        {
            BeginSuspendMode(); // He was snoring in his sleep

            // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

            char newName[MAX_PATH];
            newName[0] = 0;
            BOOL cancel = FALSE;

            // if no item is selected, we will select the one under focus and save its name
            char temporarySelected[MAX_PATH];
            SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

            BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 1, HWindow, *f, isDir, newName, cancel);

            // if we have selected an item, we will deselect it again
            UnselectItemWithName(temporarySelected);

            if (!cancel) // it's not a cancel operation
            {
                if (!ret)
                {
                    while (1)
                    {
                        // open standard dialog
                        // if no item is selected, we will select the one under focus and save its name
                        SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

                        // Since Windows Vista, Microsoft introduced a very useful feature: quick rename implicitly selects only the name without the dot and extension
                        // the same code is here four times
                        if (!Configuration.QuickRenameSelectAll)
                        {
                            int selectionEnd = -1;
                            if (!isDir)
                            {
                                const char* dot = strrchr(formatedFileName, '.');
                                if (dot != NULL && dot > formatedFileName) // While it is true that ".cvspass" in Windows is an extension, Explorer marks the entire name for ".cvspass", so we do the same
                                                                           //        if (dot != NULL)
                                    selectionEnd = (int)(dot - formatedFileName);
                            }
                            dlg.SetSelectionEnd(selectionEnd);
                        }

                        int dlgRes = (int)dlg.Execute();

                        // if we have selected an item, we will deselect it again
                        UnselectItemWithName(temporarySelected);

                        if (dlgRes == IDOK)
                        {
                            strcpy(newName, formatedFileName);
                            ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 2, HWindow, *f, isDir, newName, cancel);
                            if (ret || cancel)
                                break; // it's not an error (cancel or success)
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

                if (ret && !cancel) // Operation successfully completed
                {
                    strcpy(NextFocusName, newName); // Ensure focus of the new name after refresh
                }
            }

            // increase the thread priority again, operation completed
            SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

            // if any Salamander window is active, exit suspend mode
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
    // measure the length of the text
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

    // we do not want to exceed the panel boundary
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

/*  // I encountered a problem with sorting, Vista leaves items in their place, but Salamander needs them to be sorted
// so for now I'm putting this matter on hold
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
}*/

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
        return; // bad index

    BOOL subDir;
    if (Dirs->Count > 0)
        subDir = (strcmp(Dirs->At(0).Name, "..") == 0);
    else
        subDir = FALSE;
    if (index == 0 && subDir)
        return; // I do not work with ".."

    CFileData* f = NULL;
    BOOL isDir = index < Dirs->Count;
    f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

    char formatedFileName[MAX_PATH];
    AlterFileName(formatedFileName, f->Name, -1, Configuration.FileNameFormat, 0, isDir);

    // Since Windows Vista, Microsoft introduced a very useful feature: quick rename implicitly selects only the name without the dot and extension
    // the same code is here four times
    int selectionEnd = -1;
    if (!Configuration.QuickRenameSelectAll)
    {
        if (!isDir)
        {
            const char* dot = strrchr(formatedFileName, '.');
            if (dot != NULL && dot > formatedFileName) // While it is true that ".cvspass" in Windows is an extension, Explorer marks the entire name for ".cvspass", so we do the same
                                                       //    if (dot != NULL)
                selectionEnd = (int)(dot - formatedFileName);
        }
    }

    // Regarding the FS, we need to first call QuickRename in mode=1
    // allow the file system to open its own rename dialog
    if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
        GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // FS is in the panel
    {
        BeginSuspendMode(); // He was snoring in his sleep

        // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

        char newName[MAX_PATH];
        newName[0] = 0;
        BOOL cancel = FALSE;

        // if no item is selected, we will select the one under focus and save its name
        char temporarySelected[MAX_PATH];
        SelectFocusedItemAndGetName(temporarySelected, MAX_PATH);

        BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 1, HWindow, *f, isDir, newName, cancel);

        // if we have selected an item, we will deselect it again
        UnselectItemWithName(temporarySelected);

        if (ret && !cancel) // Operation successfully completed
        {
            strcpy(NextFocusName, newName); // Ensure focus of the new name after refresh
        }

        // increase the thread priority again, operation completed
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        // if any Salamander window is active, exit suspend mode
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

    BeginSuspendMode(TRUE); // He was snoring in his sleep

    // font like on the panel
    SendMessage(hWnd, WM_SETFONT, (WPARAM)Font, 0);
    int leftMargin = LOWORD(SendMessage(hWnd, EM_GETMARGINS, 0, 0));
    if (leftMargin < 2)
        SendMessage(hWnd, EM_SETMARGINS, EC_LEFTMARGIN, 2);

    //SendMessage(hWnd, EM_SETSEL, 0, -1); // select all
    // We can only select the name without the dot and extension
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
        // if any Salamander window is active, exit suspend mode
        EndSuspendMode(TRUE);

        // we don't want looping thanks to WM_KILLFOCUS and the like
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
        return TRUE; // bad index
    CFileData* f = NULL;
    BOOL isDir = index < Dirs->Count;
    f = isDir ? &Dirs->At(index) : &Files->At(index - Dirs->Count);

    QuickRenameWindow.SetCloseEnabled(FALSE);

    HWND hWnd = QuickRenameWindow.HWindow;
    char newName[MAX_PATH];
    GetWindowText(hWnd, newName, MAX_PATH);

    // Lower the priority of the thread to "normal" (to prevent operations from overloading the machine)
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_NORMAL);

    BOOL tryAgain = FALSE;
    BOOL mayChange = FALSE;
    if (Is(ptDisk))
    {
        // When it comes to in-place rename and the user has not changed the name, we should not
        // attempting to rename, because the user may be on a CD-ROM or other read-only disk
        // and then we displayed the error message Access is denied. The user does not have
        // Mouse has the option to escape, so it has to reach for the Escape key.
        // Explorer behaves exactly in this new way.
        if (strcmp(f->Name, newName) != 0)
            RenameFileInternal(f, newName, &mayChange, &tryAgain);
    }
    else if (Is(ptPluginFS) && GetPluginFS()->NotEmpty() &&
             GetPluginFS()->IsServiceSupported(FS_SERVICE_QUICKRENAME)) // FS is in the panel
    {
        // open standard dialog
        BOOL cancel;
        BOOL ret = GetPluginFS()->QuickRename(GetPluginFS()->GetPluginFSName(), 2, HWindow, *f, isDir, newName, cancel);
        if (!ret && !cancel)
        {
            tryAgain = TRUE;
            SetWindowText(hWnd, newName);
        }
        else
        {
            if (ret && !cancel) // Operation successfully completed
            {
                strcpy(NextFocusName, newName); // Ensure focus of the new name after refresh
            }
        }
    }

    // increase the thread priority again, operation completed
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

    // refresh non-automatically refreshed directories
    if (mayChange)
    {
        // change in the directory displayed in the panel and if the directory was renamed, also in subdirectories
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
            SkipNextCharacter = FALSE; // prevent beeping
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
            // Since Windows Vista, SelectAll works by default, so we will leave select all on them
            if (!WindowsVistaAndLater)
            {
                BOOL controlPressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
                BOOL altPressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
                BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
                if (controlPressed && !shiftPressed && !altPressed)
                {
                    SendMessage(HWindow, EM_SETSEL, 0, -1);
                    SkipNextCharacter = TRUE; // prevent beeping
                    return 0;
                }
            }
        }
        break;
    }
    }
    return CWindow::WindowProc(uMsg, wParam, lParam);
}
