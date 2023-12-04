// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include <iostream>
#define _WINSOCK_DEPRECATED_NO_WARNINGS
#include <winsock2.h>
#include <string>

using namespace std;

const char* SERVER_NAME = "reports.altap.cz";

BOOL CreateHTTPOutput(CUploadParams* uploadParams, char** buffer, int* bufferSize)
{
    BOOL ret = FALSE;
    char fileNameOnly[MAX_PATH];
    const char* p = strrchr(uploadParams->FileName, '\\');
    if (p == NULL)
        p = uploadParams->FileName;
    else
        p++;
    lstrcpy(fileNameOnly, p);

    HANDLE hFile = CreateFile(uploadParams->FileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile != INVALID_HANDLE_VALUE)
    {
        DWORD fileSize = GetFileSize(hFile, NULL);
        if (fileSize != INVALID_FILE_SIZE)
        {
            int allocatedSize = (int)(fileSize + 2000); // rezerva pro HTTP stringy
            char* header = (char*)malloc(allocatedSize);
            ZeroMemory(header, allocatedSize);
            if (header != NULL)
            {
                char* s = header;
                sprintf(s, "POST /upload.php HTTP/1.1\r\n");
                s += strlen(s);
                sprintf(s, "Host: %s\r\n", SERVER_NAME);
                s += strlen(s);
                sprintf(s, "Connection: Keep-Alive\r\n");
                s += strlen(s);
                sprintf(s, "Content-Type: multipart/form-data; boundary=---------------------------90721038027008\r\n");
                s += strlen(s);
                sprintf(s, "Content-Length: %d\r\n", allocatedSize);
                s += strlen(s); // snad nebude tato nepresnost zlobit, uvidime
                sprintf(s, "\r\n");
                s += strlen(s);
                sprintf(s, "-----------------------------90721038027008\r\n");
                s += strlen(s);
                sprintf(s, "Content-Disposition: form-data; name=\"altapfile\"; filename=\"%s\"\r\n", fileNameOnly);
                s += strlen(s); // "protechfile"
                sprintf(s, "Content-Type: application/octet-stream\r\n");
                s += strlen(s);
                sprintf(s, "\r\n");
                s += strlen(s);
                int headerSize = (int)strlen(header);
                DWORD dwBytesRead;
                if (ReadFile(hFile, s, fileSize, &dwBytesRead, NULL) && dwBytesRead == fileSize)
                {
                    s += fileSize;
                    sprintf(s, "\r\n");
                    s += strlen(s);
                    sprintf(s, "-----------------------------90721038027008--\r\n");
                    s += strlen(s);

                    *buffer = header;
                    *bufferSize = (int)(s - header) - 1;
                    ret = TRUE;
                }
                else
                    sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_READING_FILE, HLanguage), uploadParams->FileName);
            }
            else
                sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_OUT_OF_MEMORY, HLanguage));
        }
        else
            sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_FILE_SIZE, HLanguage), uploadParams->FileName);

        CloseHandle(hFile);
    }
    else
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_FILE_OPEN, HLanguage), uploadParams->FileName);
    return ret;
}

// z PHP dle http://php.net/manual/en/features.file-upload.errors.php
#define UPLOAD_ERR_OK 0
#define UPLOAD_ERR_INI_SIZE 1
#define UPLOAD_ERR_FORM_SIZE 2
#define UPLOAD_ERR_PARTIAL 3
#define UPLOAD_ERR_NO_FILE 4
#define UPLOAD_ERR_NO_TMP_DIR 5
#define UPLOAD_ERR_CANT_WRITE 6
#define UPLOAD_ERR_EXTENSION 7

BOOL GetFilesError(int err, CUploadParams* uploadParams)
{
    uploadParams->ErrorMessage[0] = 0;
    switch (err)
    {
    case UPLOAD_ERR_OK:
    {
        return TRUE;
    }

    case UPLOAD_ERR_INI_SIZE:
    case UPLOAD_ERR_FORM_SIZE:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_INI_SIZE, HLanguage), err);
        break;
    }

    case UPLOAD_ERR_PARTIAL:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_PARTIAL, HLanguage), err);
        break;
    }

    case UPLOAD_ERR_NO_FILE:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_NO_FILE, HLanguage), err);
        break;
    }

    case UPLOAD_ERR_NO_TMP_DIR:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_NO_TMP_DIR, HLanguage), err);
        break;
    }

    case UPLOAD_ERR_CANT_WRITE:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_CANT_WRITE, HLanguage), err);
        break;
    }

    case UPLOAD_ERR_EXTENSION:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_EXTENSION, HLanguage), err);
        break;
    }

    default:
    {
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_ERR_UNKNOWN, HLanguage), err);
        break;
    }
    }
    return FALSE;
}

BOOL AnalyzeResponse(const char* str, int strLen, CUploadParams* uploadParams)
{
    // vyhledame nasi odpoved z php skriptu, ve tvaru <response>X</response>, kde X je
    // error z http://php.net/manual/en/features.file-upload.errors.php
    const char* TAG_OPEN = "<response>";
    const char* TAG_CLOSE = "</response>";
    const char* tagOpen = strstr(str, TAG_OPEN);
    if (tagOpen != NULL)
    {
        const char* numBegin = tagOpen + strlen(TAG_OPEN);
        const char* num = numBegin;
        while (*num >= '0' && *num <= '9' && num - numBegin < 10 && *num != 0)
            num++;
        if (num > numBegin)
        {
            const char* tagClose = strstr(num, TAG_CLOSE);
            if (tagClose == num)
            {
                char buff[10];
                lstrcpyn(buff, numBegin, (int)(num - numBegin + 1));
                int number = atoi(buff);
                return GetFilesError(number, uploadParams);
            }
            else
                sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SYNTAX_ERROR_CLOSE, HLanguage));
        }
        else
            sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SYNTAX_ERROR_VALUE, HLanguage));
    }
    else
        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SYNTAX_ERROR_OPEN, HLanguage));
    return FALSE;
}

DWORD WINAPI UploadThreadF(void* param)
{
    CUploadParams* uploadParams = (CUploadParams*)param;
    uploadParams->Result = FALSE;

    char* buffer;
    int bufferSize;
    if (CreateHTTPOutput(uploadParams, &buffer, &bufferSize))
    {
        // initialize Winsock
        WSADATA wsaData = {0};
        int iResult = 0;
        iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
        if (iResult == 0)
        {
            // resolve host name
            struct hostent* remoteHost;
            remoteHost = gethostbyname(SERVER_NAME);
            if (remoteHost != NULL)
            {
                struct sockaddr_in addr;
                addr.sin_family = AF_INET;
                addr.sin_port = htons(80);
                addr.sin_addr.s_addr = *(unsigned long*)remoteHost->h_addr;

                SOCKET connectSocket;
                connectSocket = socket(AF_INET, SOCK_STREAM, 0);
                if (connectSocket != INVALID_SOCKET)
                {
                    iResult = connect(connectSocket, (SOCKADDR*)&addr, sizeof(addr));
                    if (iResult != SOCKET_ERROR)
                    {
                        iResult = send(connectSocket, buffer, bufferSize, 0);
                        if (iResult != SOCKET_ERROR)
                        {
                            // shutdown the connection since no more data will be sent
                            iResult = shutdown(connectSocket, SD_SEND);
                            if (iResult != SOCKET_ERROR)
                            {
                                // Receive until the peer closes the connection
                                char recvbuf[4096];
                                ZeroMemory(recvbuf, sizeof(recvbuf));
                                char* recvptr = recvbuf;
                                do
                                {
                                    iResult = recv(connectSocket, recvptr, sizeof(recvbuf) - (int)(recvptr - recvbuf) - 1, 0);
                                    if (iResult > 0)
                                        recvptr += iResult;
                                    else if (iResult == 0)
                                        uploadParams->Result = AnalyzeResponse(recvbuf, (int)(recvptr - recvbuf), uploadParams);
                                    else
                                        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_RECV, HLanguage), WSAGetLastError());
                                } while (iResult > 0);
                            }
                            else
                                sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_SHUTDOWN, HLanguage), WSAGetLastError());
                        }
                        else
                            sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_SEND, HLanguage), WSAGetLastError());
                    }
                    else
                        sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_CONNECT, HLanguage), WSAGetLastError());

                    closesocket(connectSocket);
                }
                else
                    sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_SOCKET, HLanguage), WSAGetLastError());
            }
            else
                sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_HOST, HLanguage), WSAGetLastError());

            WSACleanup();
        }
        else
            sprintf(uploadParams->ErrorMessage, LoadStr(IDS_SALMON_SOCK_ERR_INIT, HLanguage), iResult);

        free(buffer);
    }
    return EXIT_SUCCESS;
}

HANDLE HUploadThread = NULL;

BOOL StartUploadThread(CUploadParams* params)
{
    if (HUploadThread != NULL)
        return FALSE;
    DWORD id;
    HUploadThread = CreateThread(NULL, 0, UploadThreadF, params, 0, &id);
    return HUploadThread != NULL;
}

BOOL IsUploadThreadRunning()
{
    if (HUploadThread == NULL)
        return FALSE;
    DWORD res = WaitForSingleObject(HUploadThread, 0);
    if (res != WAIT_TIMEOUT)
    {
        CloseHandle(HUploadThread);
        HUploadThread = NULL;
        return FALSE;
    }
    return TRUE;
}
