// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

char LastExportPath[MAX_PATH];

BOOL ExportKey(LPWSTR fullName)
{
    CALL_STACK_MESSAGE1("ExportKey()");
    char file[MAX_PATH];
    strcpy(file, LastExportPath);
    SG->SalPathAddBackslash(file, MAX_PATH);
    while (1)
    {
        BOOL direct = FALSE;
        CExportDialog dlg(GetParent(), fullName, file, &direct);
        if (dlg.Execute() != IDOK)
            return FALSE;

        // separate the user part from the FS path
        if (!direct)
        {
            if (!RemoveFSNameFromPath(fullName))
            {
                Error(IDS_NOTREGEDTPATH);
                continue; // show the dialog again
            }
            if (!wcslen(fullName))
            {
                Error(IDS_BADPATH);
                continue; // show the dialog again
            }
        }

        if (wcscmp(fullName, L"\\") != 0)
        {
            RemoveTrailingSlashes(fullName);
            if (!wcslen(fullName))
            {
                Error(IDS_BADPATH);
                continue; // show the dialog again
            }
        }

        LPWSTR key;
        int root;
        if (!ParseFullPath(fullName, key, root))
        {
            Error(IDS_BADPATH);
            continue; // show the dialog again
        }

        // verify that the key exists
        if (root != -1)
        {
            HKEY hKey;
            int err = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, KEY_READ, &hKey);
            if (err != ERROR_SUCCESS)
            {
                ErrorL(err, IDS_OPEN);
                continue; // show the dialog again
            }
            RegCloseKey(hKey);
        }

        // verify that the target file does not exist
        DWORD attr = SG->SalGetFileAttributes(file);
        if (attr != -1)
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY)
            {
                Error(IDS_FILENAMEISDIR);
                continue; // show the dialog again
            }
            if (SG->DialogQuestion(GetParent(), BUTTONS_YESNOCANCEL, file, LoadStr(IDS_OVERWRITE), LoadStr(IDS_OVERWRITETITLE)) != DIALOG_YES)
                continue; // show the dialog again
            SG->ClearReadOnlyAttr(file);
            if (!DeleteFile(file))
            {
                Error(IDS_REPLACEERROR);
                continue; // show the dialog again
            }
        }

        SG->CutDirectory(strcpy(LastExportPath, file));

        char command[4096];
        if (root != -1) // regedit.exe can do "export all", so we'll use it for this task even after XP
        {
            // starting with XP we invoke the reg.exe command line, see https://forum.altap.cz/viewtopic.php?f=24&t=5682
            // the advantage of reg.exe is that from Vista onward it does not require UAC elevation for exports
            char sysdir[MAX_PATH];
            if (!GetSystemDirectory(sysdir, MAX_PATH))
                *sysdir = 0;
            else
                SG->SalPathAddBackslash(sysdir, MAX_PATH);
            SalPrintf(command, 4096, "\"%sreg.exe\" EXPORT \"%ls\" \"%s\"", sysdir,
                      *fullName == L'\\' ? fullName + 1 : fullName, file);
        }
        else
        {
            char windir[MAX_PATH];
            if (!GetWindowsDirectory(windir, MAX_PATH))
                *windir = 0;
            else
                SG->SalPathAddBackslash(windir, MAX_PATH);
            if (root != -1)
                SalPrintf(command, 4096, "\"%sregedit.exe\" /e \"%s\" \"%ls\"", windir, file,
                          *fullName == L'\\' ? fullName + 1 : fullName);
            else
                SalPrintf(command, 4096, "\"%sregedit.exe\" /e \"%s\"", windir, file);
        }

        STARTUPINFO si;
        PROCESS_INFORMATION pi;
        memset(&si, 0, sizeof(STARTUPINFO));
        si.cb = sizeof(STARTUPINFO);
        si.lpTitle = NULL;
        si.dwFlags = STARTF_USESHOWWINDOW;
        si.wShowWindow = SW_HIDE;

        if (!CreateProcess(NULL, command, NULL, NULL, FALSE, CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                           NULL, NULL, &si, &pi))
            return Error(IDS_PROCESS2, (root != -1) ? "reg.exe" : "regedit.exe");

        SG->CreateSafeWaitWindow(LoadStr(IDS_EXPORTING), LoadStr(IDS_PLUGINNAME), 500, FALSE, SG->GetMainWindowHWND());

        WaitForSingleObject(pi.hProcess, INFINITE);

        SG->DestroySafeWaitWindow();

        attr = SG->SalGetFileAttributes(file);
        if (attr == -1)
            Error(IDS_BADEXPORT);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);

        // announce the change on the path (our file was added)
        char changedPath[MAX_PATH];
        lstrcpyn(changedPath, file, MAX_PATH);
        SG->CutDirectory(changedPath);
        SG->PostChangeOnPathNotification(changedPath, FALSE);

        break;
    }
    return TRUE;
}
