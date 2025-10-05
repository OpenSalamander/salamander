// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include <wininet.h>

#include "checkver.h"
#include "checkver.rh"
#include "checkver.rh2"
#include "lang\lang.rh"

const char* SCRIPT_URL_FTP_EN = "ftp://ftp.altap.cz/pub/altap/salamand/checkver/salupdate40_en.txt";
const char* SCRIPT_URL_FTP_CZ = "ftp://ftp.altap.cz/pub/altap/salamand/checkver/salupdate40_cz.txt";
const char* SCRIPT_URL_HTTP = "https://www.altap.cz/salupdate40/";
const char* SCRIPT_URL_HTTP_AFTERINSTALL = "https://www.altap.cz/salupdatenew40/";

const char* AGENT_NAME = "Open Salamander CheckVer Plugin";

// limitation - may be called from only one thread; otherwise buffer overwrites are not handled
const char* GetInetErrorText(DWORD dError)
{
    static char tempErrorText[1024];
    tempErrorText[0] = 0;

    DWORD count = FormatMessage(FORMAT_MESSAGE_FROM_HMODULE, GetModuleHandle("wininet.dll"), dError, 0,
                                tempErrorText, 1024, NULL);

    if (count > 0)
    {
        // trim garbage on the right
        char* p = tempErrorText + count - 1;
        while (p > tempErrorText && (*p == '\n' || *p == '\r' || *p == ' '))
        {
            *p = 0;
            p--;
        }
    }
    else
        lstrcpy(tempErrorText, "Unable to get error message");
    return tempErrorText;
    /*
  // hopefully we will not need this (considering the trivial internet usage)
  sprintf(szTemp, "%s error code: %d\nMessage: %s\n", szCallFunc, dError, strName);
  int response;

  if (dError == ERROR_INTERNET_EXTENDED_ERROR)
  {
    InternetGetLastResponseInfo(&dwIntError, NULL, &dwLength);
    if (dwLength)
    {
      if (!(szBuffer = (char *) LocalAlloc(LPTR, dwLength)))
      {
        lstrcat(szTemp, "Unable to allocate memory to display Internet error code. Error code: ");
        lstrcat(szTemp, _itoa(GetLastError(), szBuffer, 10));
        lstrcat(szTemp, "\n");

        response = MessageBox(hErr, (LPSTR)szTemp,"Error", MB_OK);
        return FALSE;
      }

      if (!InternetGetLastResponseInfo (&dwIntError, (LPTSTR) szBuffer, &dwLength))
      {
        lstrcat(szTemp, "Unable to get Internet error. Error code: ");
        lstrcat(szTemp, _itoa(GetLastError(), szBuffer, 10));
        lstrcat(szTemp, "\n");
        response = MessageBox(hErr, (LPSTR)szTemp, "Error", MB_OK);
        return FALSE;
      }

      if (!(szBufferFinal = (char *) LocalAlloc(LPTR, (strlen(szBuffer) + strlen(szTemp) + 1))))
      {
        lstrcat(szTemp, "Unable to allocate memory. Error code: ");
        lstrcat(szTemp, _itoa (GetLastError(), szBuffer, 10));
        lstrcat(szTemp, "\n");
        response = MessageBox(hErr, (LPSTR)szTemp, "Error", MB_OK);
        return FALSE;
      }

      lstrcpy(szBufferFinal, szTemp);
      lstrcat(szBufferFinal, szBuffer);
      LocalFree(szBuffer);
      response = MessageBox(hErr, (LPSTR)szBufferFinal, "Error", MB_OK);
      LocalFree(szBufferFinal);
    }
  }
  else
  {
    response = MessageBox(hErr, (LPSTR)szTemp,"Error",MB_OK);
  }

  return response;
*/
}

void IncMainDialogID()
{
    // neither callbacks nor trace must end up here - called from a thread which may run
    // when Salamander has long since exited
    EnterCriticalSection(&MainDialogIDSection);
    MainDialogID++;
    LeaveCriticalSection(&MainDialogIDSection);
}

DWORD
GetMainDialogID()
{
    // neither callbacks nor trace must end up here - called from a thread which may run
    // when Salamander has long since exited
    EnterCriticalSection(&MainDialogIDSection);
    DWORD id = MainDialogID;
    LeaveCriticalSection(&MainDialogIDSection);
    return id;
}

struct CTDData
{
    DWORD MainDialogID;
    BOOL FirstLoadAfterInstall;
    HANDLE Continue;
};

DWORD WINAPI ThreadDownload(void* param)
{
    CTDData* data = (CTDData*)param;
    DWORD dialogID = data->MainDialogID;
    BOOL firstLoadAfterInstall = data->FirstLoadAfterInstall;
    SetEvent(data->Continue); // let the main thread continue; from this point on the data are invalid (=NULL)
    data = NULL;

    // lock the DLL to prevent it from being unloaded while this function runs
    char buff[MAX_PATH];
    GetModuleFileName(DLLInstance, buff, MAX_PATH);
    HINSTANCE hLock = LoadLibrary(buff);

    BOOL exit = FALSE;

    DWORD errorCode = 0;
    HINTERNET hSession = NULL;
    HINTERNET hUrl = NULL;
    BOOL bResult = FALSE;
    DWORD dwBytesRead = 0;

    // is the main dialog still present and is it the one that opened us?
    if (dialogID == GetMainDialogID() && !exit)
    {
        AddLogLine(LoadStr(IDS_INET_PROTOCOL), FALSE);
        AddLogLine(LoadStr(IDS_INET_INIT), FALSE);
        errorCode = InternetAttemptConnect(0);
        if (errorCode != ERROR_SUCCESS)
        {
            EnterCriticalSection(&MainDialogIDSection);
            if (dialogID == MainDialogID)
            {
                char buff2[1024];
                sprintf(buff2, LoadStr(IDS_INET_INIT_FAILED), GetInetErrorText(errorCode));
                AddLogLine(buff2, TRUE);
            }
            LeaveCriticalSection(&MainDialogIDSection);
            exit = TRUE;
        }
    }

    if (dialogID == GetMainDialogID() && !exit)
    {
        hSession = InternetOpen(AGENT_NAME, INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hSession == NULL)
        {
            EnterCriticalSection(&MainDialogIDSection);
            if (dialogID == MainDialogID)
            {
                DWORD err = GetLastError();
                char buff2[1024];
                sprintf(buff2, LoadStr(IDS_INET_INIT_FAILED), GetInetErrorText(err));
                AddLogLine(buff2, TRUE);
            }
            LeaveCriticalSection(&MainDialogIDSection);
            exit = TRUE;
        }
    }

    if (dialogID == GetMainDialogID() && !exit)
    {
        AddLogLine(LoadStr(IDS_INET_CONNECT), FALSE);

        const char* scriptURL_FTP = strcmp(LoadStr(IDS_UPDATE_SCRIPT_LANG), "CZ") == 0 ? SCRIPT_URL_FTP_CZ : SCRIPT_URL_FTP_EN;

        switch (InternetProtocol)
        {
        case inetpFTPPassive:
        {
            // FTP - passive mode (gets through firewalls more often than standard FTP, but slower)
            hUrl = InternetOpenUrl(hSession, scriptURL_FTP, NULL, 0,
                                   INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD | INTERNET_FLAG_PASSIVE, 0);
            break;
        }

        case inetpFTP:
        {
            // FTP - standard mode
            hUrl = InternetOpenUrl(hSession, scriptURL_FTP, NULL, 0,
                                   INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD, 0);
            break;
        }

        default:
        {
            // p.s. adjusted to allow version checking behind a firewall (tested at SPS)
            // HTTP (experimentally much faster than FTP-passive at SPS, probably due to HTTP caching,
            //       usually passes through firewalls)
            char scriptURL[200];
            _snprintf_s(scriptURL, _TRUNCATE, "%s?version=%s&lang=%s",
                        firstLoadAfterInstall ? SCRIPT_URL_HTTP_AFTERINSTALL : SCRIPT_URL_HTTP,
                        SalamanderTextVersion, LoadStr(IDS_UPDATE_SCRIPT_LANG));
            hUrl = InternetOpenUrl(hSession, scriptURL, NULL, 0,
                                   INTERNET_FLAG_DONT_CACHE | INTERNET_FLAG_RELOAD, 0);
            break;
        }
        }

        if (hUrl == NULL)
        {
            EnterCriticalSection(&MainDialogIDSection);
            if (dialogID == MainDialogID)
            {
                DWORD err = GetLastError();
                char buff2[1024];
                sprintf(buff2, LoadStr(IDS_INET_CONNECT_FAILED), GetInetErrorText(err));
                AddLogLine(buff2, TRUE);
            }
            LeaveCriticalSection(&MainDialogIDSection);
            exit = TRUE;
        }
    }

    if (dialogID == GetMainDialogID() && !exit)
    {
        AddLogLine(LoadStr(IDS_INET_READ), FALSE);
        bResult = InternetReadFile(hUrl, LoadedScript, LOADED_SCRIPT_MAX, &dwBytesRead);
        if (!bResult)
        {
            EnterCriticalSection(&MainDialogIDSection);
            if (dialogID == MainDialogID)
            {
                DWORD err = GetLastError();
                char buff2[1024];
                sprintf(buff2, LoadStr(IDS_INET_READ_FAILED), GetInetErrorText(err));
                AddLogLine(buff2, TRUE);
            }
            LeaveCriticalSection(&MainDialogIDSection);
            exit = TRUE;
        }
    }

    if (hUrl != NULL)
        InternetCloseHandle(hUrl);
    if (hSession != NULL)
        InternetCloseHandle(hSession);

    EnterCriticalSection(&MainDialogIDSection);
    DWORD id = MainDialogID;
    if (dialogID == GetMainDialogID())
    {
        if (!exit)
        {
            AddLogLine(LoadStr(IDS_INET_SUCCESS), FALSE);
            LoadedScriptSize = dwBytesRead;
        }
        else
            LoadedScriptSize = 0;
        PostMessage(HMainDialog, WM_USER_DOWNLOADTHREAD_EXIT, !exit, 0); // thread ends; data are loaded
        FreeLibrary(hLock);                                              // release the lock
        LeaveCriticalSection(&MainDialogIDSection);
        return 0; // let the thread die naturally
    }
    else
    {
        LeaveCriticalSection(&MainDialogIDSection);
        // we were killed from the outside - after FreeLibrary the last lock on the SPL may be removed
        // (Salamander may no longer be running) and there would be nowhere to return to,
        // therefore call this function:
        FreeLibraryAndExitThread(hLock, 3666);
        return 0; // we never return here, but the compiler cannot know that :-)
    }
}

HANDLE
StartDownloadThread(BOOL firstLoadAfterInstall)
{
    CTDData data;
    data.MainDialogID = GetMainDialogID();
    data.FirstLoadAfterInstall = firstLoadAfterInstall;
    data.Continue = CreateEvent(NULL, FALSE, FALSE, NULL);

    if (data.Continue == NULL)
    {
        TRACE_E("Unable to create Continue event.");
        return NULL;
    }

    DWORD threadID;
    HANDLE hThread = CreateThread(NULL, 0, ThreadDownload, &data, 0, &threadID);
    if (hThread == NULL)
        TRACE_E("Unable to create Check Version Download thread.");
    else // wait until the thread takes the data
        WaitForSingleObject(data.Continue, INFINITE);

    CloseHandle(data.Continue);

    return hThread;
}
