// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "checksum.h"
#include "checksum.rh"
#include "checksum.rh2"
#include "lang\lang.rh"
#include "misc.h"

BOOL Error(HWND hParent, int lastErr, int title, int error, ...)
{
    CALL_STACK_MESSAGE4("Error(, %d, %d, %d, ...)", lastErr, title, error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        size_t l = strlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, (DWORD)(1024 - l), NULL);
    }
    SalamanderGeneral->SalMessageBox(hParent, buf, LoadStr(title), MSGBOXEX_OK | MSGBOXEX_ICONEXCLAMATION);

    return FALSE;
}

BOOL SafeReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToRead, DWORD* pnBytesRead, char* fileName,
                  HWND parent, BOOL* skippedReadError, BOOL* skipAllReadErrors)
{
    if (skippedReadError != NULL)
        *skippedReadError = FALSE;
    while (!ReadFile(hFile, lpBuffer, nBytesToRead, pnBytesRead, NULL))
    {
        if (skippedReadError != NULL && *skipAllReadErrors)
        {
            *skippedReadError = TRUE;
            return FALSE;
        }
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        DWORD buttons = skippedReadError == NULL ? BUTTONS_RETRYCANCEL : BUTTONS_RETRYSKIPCANCEL;
        int result = SalamanderGeneral->DialogError(parent, buttons, fileName, error, LoadStr(IDS_READERROR));
        switch (result)
        {
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            return FALSE;

        case DIALOG_RETRY:
            continue;

        case DIALOG_SKIPALL:
            *skipAllReadErrors = TRUE;
        case DIALOG_SKIP:
            *skippedReadError = TRUE;
            return FALSE;
        }
    }
    return TRUE;
}

BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten,
                   char* fileName, HWND parent)
{
    while (!WriteFile(hFile, lpBuffer, nBytesToWrite, pnBytesWritten, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(parent, BUTTONS_RETRYCANCEL, fileName, error,
                                           LoadStr(IDS_WRITEERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}

BOOL SafeOpenCreateFile(LPCTSTR fileName, DWORD desiredAccess, DWORD shareMode, DWORD creationDisposition,
                        DWORD flagsAndAttributes, HANDLE* hFile, BOOL* skip, int* silent, HWND parent)
{
    CALL_STACK_MESSAGE6("SafeOpenCreateFile(%s, 0x%X, 0x%X, 0x%X, 0x%X, , , )", fileName, desiredAccess,
                        shareMode, creationDisposition, flagsAndAttributes);

    while ((*hFile = CreateFile(fileName, desiredAccess, shareMode, NULL, creationDisposition,
                                flagsAndAttributes, NULL)) == INVALID_HANDLE_VALUE &&
           ((silent != NULL) ? !*silent : 1))
    {
        int lastErr = GetLastError();
        char error[1024];
        strcpy(error, LoadStr(IDS_ERROROPENING));
        strcat(error, ": ");
        size_t l = strlen(error);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error + l, (DWORD)(1024 - l), NULL);
        int result;
        if (skip == NULL)
            result = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYCANCEL, fileName, error, NULL);
        else
            result = SalamanderGeneral->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fileName, error, NULL);
        switch (result)
        {
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            return FALSE;

        case DIALOG_RETRY:
            continue;

        case DIALOG_SKIPALL:
            *silent = 1;
        case DIALOG_SKIP:
            if (skip != NULL)
                *skip = TRUE;
            return TRUE;
        }
    }
    if (skip != NULL)
        *skip = (*hFile == INVALID_HANDLE_VALUE);
    return TRUE;
}

void GetFirstWord(char* str, int& pos, int& len, char delimitChar)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        // CALL_STACK_MESSAGE4("GetFirstWord(, %d, %d, %d)", pos, len, delimitChar);
        pos = 0;
    while (str[pos] && ((BYTE)str[pos] <= ' '))
        pos++;
    len = pos;
    while (str[len] && ((BYTE)str[len] > ' ') && str[len] != delimitChar)
        len++;
    len -= pos;
}

void GetLastWord(char* str, int& pos, int& len, char delimitChar)
{
    CALL_STACK_MESSAGE_NONE // frekventovana funkce
        // CALL_STACK_MESSAGE4("GetLastWord(, %d, %d, %d)", pos, len, delimitChar);
        len = (int)strlen(str);
    while (len > 0 && ((BYTE)str[len - 1] <= ' '))
        len--;
    pos = len;
    while (pos > 0 && ((BYTE)str[pos - 1] > ' ') && str[pos - 1] != delimitChar)
        pos--;
    len -= pos;
}

BYTE hex(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    return 0;
}

BOOL IsHex(const char* str, int len)
{
    CALL_STACK_MESSAGE2("IsHex(, %d)", len);
    for (int i = 0; i < len; i++, str++)
        if (!((*str >= '0' && *str <= '9') || (*str >= 'A' && *str <= 'F') || (*str >= 'a' && *str <= 'f')))
            return FALSE;
    return TRUE;
}
