// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

BOOL CRenamerDialog::ExportToTempFile()
{
    CALL_STACK_MESSAGE1("CRenamerDialog::ExportToTempFile()");

    // vytvorime jmeno tmp soubor
    if (!SG->SalGetTempFileName(NULL, "SAL", TempFile, FALSE, NULL) ||
        !SG->SalPathAppend(TempFile, "list.txt", MAX_PATH))
        return Error(IDS_CREATETEMP);

    // vytvorime/otevreme tmp soubor
    HANDLE file = CreateFile(TempFile, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        SG->CutDirectory(TempFile);
        SG->RemoveTemporaryDir(TempFile);
        return Error(IDS_CREATETEMP);
    }

    // nacteme text z controlu
    TBuffer<char> buffer;
    if (!buffer.Reserve(GetWindowTextLength(ManualEdit->HWindow) + 1))
    {
        SG->CutDirectory(TempFile);
        SG->RemoveTemporaryDir(TempFile);
        return Error(IDS_LOWMEM);
    }
    DWORD len = GetWindowText(ManualEdit->HWindow, buffer.Get(), (int)buffer.GetSize());

    DWORD written;
    BOOL b = WriteFile(file, buffer.Get(), len, &written, NULL) || written != len;

    CloseHandle(file);

    if (!b)
    {
        SG->CutDirectory(TempFile);
        SG->RemoveTemporaryDir(TempFile);
        Error(IDS_WRITETEMP);
    }

    return b;
}

BOOL CRenamerDialog::ImportFromTempFile()
{
    CALL_STACK_MESSAGE1("CRawEditValDialog::ImportFromTempFile()");
    // otevreme tmp soubor
    HANDLE file = CreateFile(TempFile, GENERIC_READ, FILE_SHARE_READ, NULL,
                             OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
        return Error(IDS_OPENTEMP);

    CQuadWord size;
    DWORD err;
    if (!SG->SalGetFileSize(file, size, err))
    {
        CloseHandle(file);
        return ErrorL(err, IDS_SIZEOFTEMP);
    }

    if (size.HiDWord > 0)
    {
        CloseHandle(file);
        return Error(IDS_LONGDATA);
    }

    TBuffer<char> buffer;
    if (!buffer.Reserve(size.LoDWord + 1))
    {
        CloseHandle(file);
        return Error(IDS_LOWMEM);
    }

    DWORD read;
    BOOL b = ReadFile(file, buffer.Get(), size.LoDWord, &read, NULL) || read != size.LoDWord;

    CloseHandle(file);

    if (b)
    {
        buffer.Get()[size.LoDWord] = 0;
        SendMessage(ManualEdit->HWindow, EM_SETSEL, 0, -1);
        SendMessage(ManualEdit->HWindow, EM_REPLACESEL, FALSE, LPARAM(buffer.Get()));
        // SendMessage(ManualEdit->HWindow, WM_SETTEXT, 0, LPARAM(buffer.Get()));
    }
    else
        Error(IDS_READTEMP);

    return b;
}

BOOL EscapeQuotes(const char* string, char* escaped)
{
    CALL_STACK_MESSAGE2("EscapeQuotes(%s, )", string);
    int c = 0;
    while (*string)
    {
        if (c >= 4095)
            return FALSE;
        if (*string == '"')
        {
            *escaped++ = '\\';
            c++;
        }
        *escaped++ = *string++;
        c++;
    }
    if (c == 4096)
        return FALSE;
    *escaped++ = 0;
    return TRUE;
}

BOOL CRenamerDialog::ExecuteCommand(const char* command)
{
    CALL_STACK_MESSAGE2("CRenamerDialog::ExecuteCommand(%s)", command);
    // vytvorime commandlinu
    char shell[MAX_PATH];
    if (!GetEnvironmentVariable("SHELL", shell, MAX_PATH))
    {
        if (!GetEnvironmentVariable("COMSPEC", shell, MAX_PATH))
            return FALSE;
    }

    BOOL sh = strlen(shell) && strstr(SG->SalPathFindFileName(shell), "sh") != NULL;

    char cmdLine[4096];
    if (sh)
    {
        char escaped[4096];

        if (!EscapeQuotes(command, escaped) ||
            !SalPrintf(cmdLine, 4096, "%s -c \"%s\"", shell, escaped))
            return Error(IDS_LONGDATA);
    }
    else
    {
        if (!SalPrintf(cmdLine, 4096, "%s /C %s", shell, command))
            return Error(IDS_LONGDATA);
    }

    char tempDir[MAX_PATH];
    char outName[MAX_PATH];
    char errName[MAX_PATH];
    HANDLE inPipeWr = INVALID_HANDLE_VALUE;
    HANDLE inPipeWrDup = INVALID_HANDLE_VALUE;
    HANDLE inPipeRd = INVALID_HANDLE_VALUE;
    HANDLE outFile = INVALID_HANDLE_VALUE;
    HANDLE errFile = INVALID_HANDLE_VALUE;
    SECURITY_ATTRIBUTES saAttr;
    TBuffer<char> buffer;
    CQuadWord size;
    BOOL ret = TRUE;

    // aby mohli byt handly dedeny
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.lpSecurityDescriptor = NULL;
    saAttr.bInheritHandle = TRUE;

    // vytvorime jmena pro tmp soubory
    if (!SG->SalGetTempFileName(NULL, "SAL", tempDir, FALSE, NULL) ||
        !SG->SalPathAppend(strcpy(outName, tempDir), "stdout", MAX_PATH) ||
        !SG->SalPathAppend(strcpy(errName, tempDir), "stderr", MAX_PATH))
        return Error(IDS_CREATETEMP);

    // vytvorime trubku pro vstup
    if (!CreatePipe(&inPipeRd, &inPipeWr, &saAttr, 0))
    {
        inPipeWr = INVALID_HANDLE_VALUE;
        inPipeRd = INVALID_HANDLE_VALUE;
        ret = Error(IDS_CREATEPIPE);
        goto LERROR;
    }
    if (!DuplicateHandle(GetCurrentProcess(), inPipeWr,
                         GetCurrentProcess(), &inPipeWrDup, 0,
                         FALSE,
                         DUPLICATE_SAME_ACCESS))
        return FALSE;
    CloseHandle(inPipeWr);
    inPipeWr = INVALID_HANDLE_VALUE;

    // vytvorime/otevreme tmp soubory pro vystup
    outFile = CreateFile(outName, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_TEMPORARY, NULL);
    errFile = CreateFile(errName, GENERIC_READ | GENERIC_WRITE,
                         FILE_SHARE_READ | FILE_SHARE_WRITE, &saAttr, CREATE_ALWAYS,
                         FILE_ATTRIBUTE_TEMPORARY, NULL);
    if (outFile == INVALID_HANDLE_VALUE ||
        errFile == INVALID_HANDLE_VALUE)
    {
        ret = Error(IDS_CREATETEMP);
        goto LERROR;
    }

    // nacteme text z controlu
    if (!buffer.Reserve(GetWindowTextLength(ManualEdit->HWindow) + 1))
    {
        SG->CutDirectory(TempFile);
        SG->RemoveTemporaryDir(TempFile);
        ret = Error(IDS_LOWMEM);
        goto LERROR;
    }
    DWORD len;
    len = GetWindowText(ManualEdit->HWindow, buffer.Get(), (int)buffer.GetSize());

    STARTUPINFO si;
    PROCESS_INFORMATION pi;
    memset(&si, 0, sizeof(STARTUPINFO));
    si.cb = sizeof(STARTUPINFO);
    si.lpTitle = cmdLine;
    si.dwFlags = STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
    si.wShowWindow = SW_SHOWMINIMIZED;
    si.hStdInput = inPipeRd;
    si.hStdOutput = outFile;
    si.hStdError = errFile;

    if (!CreateProcess(NULL, cmdLine, NULL, NULL, TRUE,
                       CREATE_DEFAULT_ERROR_MODE | NORMAL_PRIORITY_CLASS,
                       NULL, *Root ? Root : NULL, &si, &pi))
    {
        ret = Error(IDS_PROCESS);
        goto LERROR;
    }

    CloseHandle(inPipeRd);
    inPipeRd = INVALID_HANDLE_VALUE;

    DWORD written;
    if (!WriteFile(inPipeWrDup, buffer.Get(), len, &written, NULL) || written != len)
    {
        if (GetLastError() != ERROR_BROKEN_PIPE)
            ret = Error(IDS_WRITEPIPE);
    }

    CloseHandle(inPipeWrDup);
    inPipeWrDup = INVALID_HANDLE_VALUE;

    WaitForSingleObject(pi.hProcess, INFINITE);

    DWORD exitCode;
    GetExitCodeProcess(pi.hProcess, &exitCode);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    // overime, ze nenastala chyba
    size.HiDWord = 0;
    size.LoDWord = SetFilePointer(errFile, 0, LPLONG(&size.HiDWord), FILE_END);
    if (exitCode || size.LoDWord)
    {
        if (!buffer.Reserve(size.LoDWord + 1))
        {
            ret = Error(IDS_LOWMEM);
            goto LERROR;
        }

        LONG l;
        l = 0;
        SetFilePointer(errFile, 0, &l, FILE_BEGIN);

        DWORD read;
        if (ReadFile(errFile, buffer.Get(), size.LoDWord, &read, NULL) && read == size.LoDWord)
        {
            buffer.Get()[size.LoDWord] = 0;
            ret = CCommandErrorDialog(HWindow, cmdLine, exitCode, buffer.Get()).Execute() == IDOK;
            if (!ret)
                goto LERROR;
        }
        else
        {
            ret = Error(IDS_READTEMP);
            goto LERROR;
        }
    }

    if (!ret)
        goto LERROR;

    // nacteme text ze souboru
    size.HiDWord = 0;
    size.LoDWord = SetFilePointer(outFile, 0, LPLONG(&size.HiDWord), FILE_END);

    if (size.HiDWord > 0)
    {
        ret = Error(IDS_LONGDATA);
        goto LERROR;
    }

    if (!buffer.Reserve(size.LoDWord + 1))
    {
        ret = Error(IDS_LOWMEM);
        goto LERROR;
    }

    LONG l;
    l = 0;
    SetFilePointer(outFile, 0, &l, FILE_BEGIN);

    DWORD read;
    if (ReadFile(outFile, buffer.Get(), size.LoDWord, &read, NULL) && read == size.LoDWord)
    {
        buffer.Get()[size.LoDWord] = 0;
        SendMessage(ManualEdit->HWindow, EM_SETSEL, 0, -1);
        SendMessage(ManualEdit->HWindow, EM_REPLACESEL, TRUE, LPARAM(buffer.Get()));
        //SendMessage(ManualEdit->HWindow, WM_SETTEXT, 0, LPARAM(buffer.Get()));
    }
    else
    {
        ret = Error(IDS_READTEMP);
        goto LERROR;
    }

LERROR:
    if (inPipeWr != INVALID_HANDLE_VALUE)
        CloseHandle(inPipeWr);
    if (inPipeRd != INVALID_HANDLE_VALUE)
        CloseHandle(inPipeRd);
    if (inPipeWrDup != INVALID_HANDLE_VALUE)
        CloseHandle(inPipeWrDup);
    if (outFile != INVALID_HANDLE_VALUE)
        CloseHandle(outFile);
    if (errFile != INVALID_HANDLE_VALUE)
        CloseHandle(errFile);
    SG->RemoveTemporaryDir(tempDir);

    return ret;
}
