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

        // vyseparujeme z FS path user part
        if (!direct)
        {
            if (!RemoveFSNameFromPath(fullName))
            {
                Error(IDS_NOTREGEDTPATH);
                continue; // zobrazime dialog znova
            }
            if (!wcslen(fullName))
            {
                Error(IDS_BADPATH);
                continue; // zobrazime dialog znova
            }
        }

        if (wcscmp(fullName, L"\\") != 0)
        {
            RemoveTrailingSlashes(fullName);
            if (!wcslen(fullName))
            {
                Error(IDS_BADPATH);
                continue; // zobrazime dialog znova
            }
        }

        LPWSTR key;
        int root;
        if (!ParseFullPath(fullName, key, root))
        {
            Error(IDS_BADPATH);
            continue; // zobrazime dialog znova
        }

        // overime, ze klic existuje
        if (root != -1)
        {
            HKEY hKey;
            int err = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, KEY_READ, &hKey);
            if (err != ERROR_SUCCESS)
            {
                ErrorL(err, IDS_OPEN);
                continue; // zobrazime dialog znova
            }
            RegCloseKey(hKey);
        }

        // overime, zda cilovy soubor neexistuje
        DWORD attr = SG->SalGetFileAttributes(file);
        if (attr != -1)
        {
            if (attr & FILE_ATTRIBUTE_DIRECTORY)
            {
                Error(IDS_FILENAMEISDIR);
                continue; // zobrazime dialog znova
            }
            if (SG->DialogQuestion(GetParent(), BUTTONS_YESNOCANCEL, file, LoadStr(IDS_OVERWRITE), LoadStr(IDS_OVERWRITETITLE)) != DIALOG_YES)
                continue; // zobrazime dialog znova
            SG->ClearReadOnlyAttr(file);
            if (!DeleteFile(file))
            {
                Error(IDS_REPLACEERROR);
                continue; // zobrazime dialog znova
            }
        }

        SG->CutDirectory(strcpy(LastExportPath, file));

        char command[4096];
        if (root != -1) // regedit.exe umi "export all", takze ho pro tuto ulohu pouzijeme i od XP dal
        {
            // od XP zavolame command line reg.exe, viz https://forum.altap.cz/viewtopic.php?f=24&t=5682
            // vyhoda reg.exe je, ze od Vista dal nevyzaduje UAC eskalaci pro exporty
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

        // ohlasime zmenu na ceste (pribyl nas soubor)
        char changedPath[MAX_PATH];
        lstrcpyn(changedPath, file, MAX_PATH);
        SG->CutDirectory(changedPath);
        SG->PostChangeOnPathNotification(changedPath, FALSE);

        break;
    }
    return TRUE;
}
