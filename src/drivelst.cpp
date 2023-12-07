// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "menu.h"
#include "drivelst.h"
#include "cfgdlg.h"
#include "dialogs.h"
#include "plugins.h"
#include "fileswnd.h"
#include "mainwnd.h"
#include "shellib.h"
#include "toolbar.h"
#include "shiconov.h"

CNBWNetAC3Thread NBWNetAC3Thread;

void GetNetworkDrives(DWORD& netDrives, char (*netRemotePath)[MAX_PATH])
{
    char buffer[10000];
    GetNetworkDrivesBody(netDrives, netRemotePath, buffer);
}

void GetNetworkDrivesBody(DWORD& netDrives, char (*netRemotePath)[MAX_PATH], char* buffer)
{
    CALL_STACK_MESSAGE1("GetNetworkDrives(,)");
    netDrives = 0; // bit array of network drives

    HANDLE hEnumNet;
    DWORD err = WNetOpenEnum(RESOURCE_REMEMBERED, RESOURCETYPE_DISK,
                             RESOURCEUSAGE_CONNECTABLE, NULL, &hEnumNet);
    if (err == ERROR_SUCCESS)
    {
        DWORD bufSize;
        DWORD entries = 0;
        NETRESOURCE* netSources = (NETRESOURCE*)buffer;
        while (1)
        {
            DWORD e = 0xFFFFFFFF; // as many as possible
            bufSize = 10000;
            err = WNetEnumResource(hEnumNet, &e, netSources, &bufSize);
            if (err == ERROR_SUCCESS && e > 0)
            {
                int i;
                for (i = 0; i < (int)e; i++) // we will process new data
                {
                    char* name = netSources[i].lpLocalName;
                    if (name != NULL)
                    {
                        char drv = LowerCase[name[0]];
                        if (drv >= 'a' && drv <= 'z' && name[1] == ':')
                        {
                            netDrives |= (1 << (drv - 'a'));
                            if (netRemotePath != NULL)
                            {
                                name = netSources[i].lpRemoteName;
                                if (name != NULL)
                                {
                                    int l = (int)strlen(name);
                                    if (l >= MAX_PATH)
                                        l = MAX_PATH - 1;
                                    memmove(netRemotePath[drv - 'a'], name, l);
                                    netRemotePath[drv - 'a'][l] = 0;
                                }
                                else
                                    netRemotePath[drv - 'a'][0] = 0;
                            }
                        }
                    }
                }
                entries += e;
            }
            else
                break;
        }
        WNetCloseEnum(hEnumNet);
    }
    else
    {
        if (err != ERROR_NO_NETWORK)
            SalMessageBox(MainWindow->HWindow, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                          MB_OK | MB_ICONEXCLAMATION);
    }
}

BOOL GetUserName(const char* drive, const char* remoteName, char* userName, DWORD userBufSize,
                 char* providerBuf, DWORD providerBufSize)
{
    CALL_STACK_MESSAGE5("GetUserName(%s, %s, , %u, , %u)", drive, remoteName, userBufSize, providerBufSize);
    HKEY network;
    LONG res = HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, "Network", 0, KEY_READ, &network));
    if (res != ERROR_SUCCESS)
        return FALSE; // well, nothing ...

    BOOL ret = FALSE;
    char keyName[MAX_PATH];
    DWORD keyNameSize, type;
    HKEY driveKey;

    keyName[0] = drive[0];
    keyName[1] = 0;
    if (HANDLES_Q(RegOpenKeyEx(network, keyName, 0, KEY_READ, &driveKey)) == ERROR_SUCCESS)
    {
        keyNameSize = MAX_PATH;
        type = REG_SZ;
        res = SalRegQueryValueEx(driveKey, "RemotePath", 0, &type,
                                 (unsigned char*)keyName, &keyNameSize);
        if (res == ERROR_SUCCESS && type == REG_SZ && IsTheSamePath(keyName, remoteName))
        {
            ret = TRUE; // we will announce success, even if the user name is not defined (the current user should be used)

            keyNameSize = userBufSize;
            type = REG_SZ;
            res = SalRegQueryValueEx(driveKey, "UserName", 0, &type,
                                     (unsigned char*)userName, &keyNameSize);
            if (res == ERROR_SUCCESS && type == REG_SZ && userName[0] != 0) // do we have a user?
                TRACE_I("Found user name: " << keyName << ", " << userName);
            else
                userName[0] = 0;

            keyNameSize = providerBufSize;
            type = REG_SZ;
            res = SalRegQueryValueEx(driveKey, "ProviderName", 0, &type,
                                     (unsigned char*)providerBuf, &keyNameSize);
            if (res != ERROR_SUCCESS || type != REG_SZ) // if we do not have a provider, we will reset the buffer
                providerBuf[0] = 0;
        }
        HANDLES(RegCloseKey(driveKey));
    }
    HANDLES(RegCloseKey(network));

    return ret;
}

struct CNBWNetAC3ThreadFParams
{
    DWORD err;
    NETRESOURCE netResource;
    LPTSTR lpPassword;
    LPTSTR lpUserName;
    DWORD dwFlags;

    char bufUserName[USERNAME_MAXLEN];
    char bufPassword[PASSWORD_MAXLEN];
    char bufLocalName[MAX_PATH];
    char bufRemoteName[MAX_PATH];

    DWORD errProviderCode;
    char errBuf[300];
    char errProviderName[200];
};

CNBWNetAC3ThreadFParams NBWNetAC3ThreadFParams;

DWORD WINAPI NBWNetAC3ThreadF(void* param)
{
    CALL_STACK_MESSAGE_NONE
    CNBWNetAC3ThreadFParams* data = &NBWNetAC3ThreadFParams;
    data->err = WNetAddConnection3(NULL /* we're not using CONNECT_INTERACTIVE, we're in another thread */,
                                   &data->netResource, data->lpPassword, data->lpUserName, data->dwFlags);
    memset(data->bufPassword, 0, sizeof(data->bufPassword));
    if (data->err == ERROR_EXTENDED_ERROR &&
        WNetGetLastError(&data->errProviderCode, data->errBuf, 300, data->errProviderName, 200) != NO_ERROR)
    {
        data->errProviderCode = 0;
        data->errBuf[0] = 0;
        data->errProviderName[0] = 0;
    }
    return 0;
}

BOOL NonBlockingWNetAddConnection3(DWORD& err, LPNETRESOURCE lpNetResource,
                                   LPTSTR lpPassword, LPTSTR lpUserName, DWORD dwFlags,
                                   DWORD* errProviderCode, char* errBuf, char* errProviderName)
{
    CALL_STACK_MESSAGE3("NonBlockingWNetAddConnection3(0x%X, , , , 0x%X, , ,)", err, dwFlags);

    // Prevent re-entrance
    static CCriticalSection cs;
    CEnterCriticalSection enterCS(cs);

    err = 0; // for sure
    if (errProviderCode != NULL)
        *errProviderCode = 0;
    if (errBuf != NULL)
        errBuf[0] = 0;
    if (errProviderName != NULL)
        errProviderName[0] = 0;

    // first all we will wait for the previous "calculation" to finish
    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - see help
    if (NBWNetAC3Thread.ShutdownArrived)
        return FALSE; // soft has already ended, no further action
    if (NBWNetAC3Thread.Thread != NULL)
    {
        while (1)
        {
            DWORD res = WaitForSingleObject(NBWNetAC3Thread.Thread, 200);
            if (res == WAIT_FAILED)
                return FALSE; // invalid thread handle (closed from outside when ending the soft)
            if (res != WAIT_TIMEOUT)
                break;
            if (UserWantsToCancelSafeWaitWindow())
                return FALSE;
        }
        if (NBWNetAC3Thread.ShutdownArrived)
            return FALSE; // soft has already ended, no further action
        NBWNetAC3Thread.Close();
    }

    // then we will set the parameters and try to start a new thread
    if (lpUserName != NULL)
    {
        lstrcpyn(NBWNetAC3ThreadFParams.bufUserName, lpUserName, USERNAME_MAXLEN);
        NBWNetAC3ThreadFParams.lpUserName = NBWNetAC3ThreadFParams.bufUserName;
    }
    else
        NBWNetAC3ThreadFParams.lpUserName = NULL;
    if (lpPassword != NULL)
    {
        lstrcpyn(NBWNetAC3ThreadFParams.bufPassword, lpPassword, PASSWORD_MAXLEN);
        NBWNetAC3ThreadFParams.lpPassword = NBWNetAC3ThreadFParams.bufPassword;
    }
    else
        NBWNetAC3ThreadFParams.lpPassword = NULL;
    NBWNetAC3ThreadFParams.netResource = *lpNetResource;
    if (lpNetResource->lpLocalName != NULL)
    {
        lstrcpyn(NBWNetAC3ThreadFParams.bufLocalName, lpNetResource->lpLocalName, MAX_PATH);
        NBWNetAC3ThreadFParams.netResource.lpLocalName = NBWNetAC3ThreadFParams.bufLocalName;
    }
    if (lpNetResource->lpRemoteName != NULL)
    {
        lstrcpyn(NBWNetAC3ThreadFParams.bufRemoteName, lpNetResource->lpRemoteName, MAX_PATH);
        NBWNetAC3ThreadFParams.netResource.lpRemoteName = NBWNetAC3ThreadFParams.bufRemoteName;
    }
    NBWNetAC3ThreadFParams.err = 0;
    NBWNetAC3ThreadFParams.dwFlags = dwFlags;
    NBWNetAC3ThreadFParams.errProviderCode = 0;
    NBWNetAC3ThreadFParams.errBuf[0] = 0;
    NBWNetAC3ThreadFParams.errProviderName[0] = 0;

    DWORD ThreadID;
    NBWNetAC3Thread.Set(HANDLES(CreateThread(NULL, 0, NBWNetAC3ThreadF, NULL, 0, &ThreadID)));
    if (NBWNetAC3Thread.Thread == NULL)
    {
        memset(NBWNetAC3ThreadFParams.bufPassword, 0, sizeof(NBWNetAC3ThreadFParams.bufPassword));
        TRACE_E("Unable to start add-net-connection-3 thread.");
        return FALSE; // error (simulation of ESC)
    }

    // we will wait for the "calculation" to finish so that we can return the result
    while (1)
    {
        DWORD res = WaitForSingleObject(NBWNetAC3Thread.Thread, 200);
        if (res == WAIT_FAILED)
            return FALSE; // invalid thread handle (closed from outside when ending the soft)
        if (res != WAIT_TIMEOUT)
            break;
        if (UserWantsToCancelSafeWaitWindow())
            return FALSE;
    }
    if (NBWNetAC3Thread.ShutdownArrived)
        return FALSE; // soft has already ended, no further action
    NBWNetAC3Thread.Close();

    if (errProviderCode != NULL)
        *errProviderCode = NBWNetAC3ThreadFParams.errProviderCode;
    if (errBuf != NULL)
        lstrcpyn(errBuf, NBWNetAC3ThreadFParams.errBuf, 300);
    if (errProviderName != NULL)
        lstrcpyn(errProviderName, NBWNetAC3ThreadFParams.errProviderName, 200);
    err = NBWNetAC3ThreadFParams.err;
    return TRUE;
}

BOOL IsLogonFailureErr(DWORD err)
{
    return err == ERROR_ACCESS_DENIED || // defense against e.g. "no files found"
           err == ERROR_LOGON_FAILURE ||
           err == ERROR_ACCOUNT_RESTRICTION ||
           err == ERROR_INVALID_LOGON_HOURS ||
           err == ERROR_INVALID_WORKSTATION ||
           err == ERROR_PASSWORD_EXPIRED ||
           err == ERROR_PASSWORD_MUST_CHANGE ||
           err == ERROR_ACCOUNT_DISABLED ||
           err == ERROR_LOGON_NOT_GRANTED ||
           err == ERROR_TRUST_FAILURE ||
           err == ERROR_NOLOGON_INTERDOMAIN_TRUST_ACCOUNT ||
           err == ERROR_NOLOGON_WORKSTATION_TRUST_ACCOUNT ||
           err == ERROR_NOLOGON_SERVER_TRUST_ACCOUNT ||
           err == ERROR_ILL_FORMED_PASSWORD ||
           err == ERROR_INVALID_PASSWORD ||
           err == ERROR_INVALID_PASSWORDNAME ||
           err == ERROR_BAD_USERNAME ||
           err == ERROR_NO_SUCH_USER ||
           err == ERROR_DOWNGRADE_DETECTED ||
           err == ERROR_EXTENDED_ERROR; // who knows what extended error this is, but "password expired" belongs here, so that it's a logon failure (at least user says Cancel)
}

BOOL IsBadUserNameOrPasswdErr(DWORD err)
{
    return err == ERROR_LOGON_FAILURE ||
           err == ERROR_INVALID_PASSWORD ||
           err == ERROR_BAD_USERNAME ||
           err == ERROR_NO_SUCH_USER;
}

BOOL CharIsAllowedInServerName(char c)
{
    switch (c)
    {
    case '`':
    case '~':
    case '!':
    case '@':
    case '#':
    case '$':
    case '%':
    case '^':
    case '&':
    case '*':
    case '(':
    case ')':
    case '=':
    case '+':
        //    case '_':          //  only warning appears for this character
    case '[':
    case ']':
    case '{':
    case '}':
    case '\\':
    case '|':
    case ';':
    case ':':
    case '.':
    case '\'':
    case '"':
    case ',':
    case '<':
    case '>':
    case '/':
    case '?':
    case ' ':
        return FALSE;
    default:
        return TRUE;
    }
}

BOOL IsAdminShareExtraLogonFailureErr(DWORD err, const char* root)
{
    if (err == ERROR_INVALID_NAME && root[0] == '\\' && root[1] == '\\')
    {
        const char* server = root + 2;
        root++;
        while (*++root != 0 && *root != '\\' && CharIsAllowedInServerName(*root))
            ;             // skip server name + simple test (incomplete, e.g. forbids dots in IPv4 and IPv6), whether it is really an invalid name (in that case we will not ask for username+password)
        if (*root == '.') // we will take care of IPv4 adresses (let's ignore IPv6)
        {
            const char* r = root;
            while (*++r != 0 && *r != '\\')
                ;
            if (r - server < 50)
            {
                char ip[51];
                lstrcpyn(ip, server, (int)((r - server) + 1));
                if (inet_addr(ip) != INADDR_NONE)
                    root = r; // this is an IP string (aa.bb.cc.dd)
            }
        }
        if (*root == '\\')
        {
            while (*++root != 0 && *root != '\\')
                ;
            if (*(root - 1) == '$')
                return TRUE; // admin share (\\server\share$ or \\server\share$\...)
        }
        else
        {
            if (*root != 0)
                TRACE_I("IsAdminShareExtraLogonFailureErr: invalid char: '" << *root << "'");
        }
    }
    return FALSE;
}

typedef struct _CREDUI_INFOA
{
    DWORD cbSize;
    HWND hwndParent;
    PCSTR pszMessageText;
    PCSTR pszCaptionText;
    HBITMAP hbmBanner;
} CREDUI_INFOA, *PCREDUI_INFOA;

#ifndef __SECHANDLE_DEFINED__
typedef struct _SecHandle
{
    ULONG_PTR dwLower;
    ULONG_PTR dwUpper;
} SecHandle, *PSecHandle;

#define __SECHANDLE_DEFINED__
#endif // __SECHANDLE_DEFINED__

typedef PSecHandle PCtxtHandle;

typedef WINADVAPI DWORD(WINAPI* FT_CredUIPromptForCredentialsA)(
    PCREDUI_INFOA pUiInfo,
    PCSTR pszTargetName,
    PCtxtHandle pContext,
    DWORD dwAuthError,
    PSTR pszUserName,
    ULONG ulUserNameBufferSize,
    PSTR pszPassword,
    ULONG ulPasswordBufferSize,
    BOOL* save,
    DWORD dwFlags);

typedef WINADVAPI DWORD(WINAPI* FT_CredUIConfirmCredentialsA)(
    PCSTR pszTargetName,
    BOOL bConfirm);

/*
typedef WINADVAPI DWORD (WINAPI *FT_CredUIParseUserNameA)(
    CONST CHAR *userName,
    CHAR *user,
    ULONG userBufferSize,
    CHAR *domain,
    ULONG domainBufferSize);
*/

#define CREDUI_FLAGS_DO_NOT_PERSIST 0x00002      // Do not show "Save" checkbox, and do not persist credentials
#define CREDUI_FLAGS_EXPECT_CONFIRMATION 0x20000 // do not persist unless caller later confirms credential via CredUIConfirmCredential() api
#define CREDUI_FLAGS_GENERIC_CREDENTIALS 0x40000 // Credential is a generic credential

BOOL RestoreNetworkConnection(HWND parent, const char* name, const char* remoteName, DWORD* retErr, LPNETRESOURCE lpNetResource)
{
    CALL_STACK_MESSAGE3("RestoreNetworkConnection(, %s, %s, ,)", name, remoteName);

    BOOL ret = TRUE;
    char serverName[MAX_PATH];
    serverName[0] = 0;
    NETRESOURCE nsBuf;
    NETRESOURCE* ns = NULL;
    char userNameBuf[USERNAME_MAXLEN];
    char* userName = NULL;
    char providerBuf[MAX_PATH];

    if (lpNetResource != NULL)
    {
        if (!Windows7AndLater &&
            lpNetResource->lpRemoteName != NULL && lpNetResource->lpRemoteName[0] == '\\' &&
            lpNetResource->lpRemoteName[1] == '\\' && lpNetResource->lpRemoteName[2] != '\\' &&
            lpNetResource->lpRemoteName[2] != 0)
        {
            char* end = strchr(lpNetResource->lpRemoteName + 2, '\\');
            if (end == NULL || *(end + 1) == 0)
            {
                if (end == NULL)
                    lstrcpyn(serverName, lpNetResource->lpRemoteName + 2, MAX_PATH);
                else
                    lstrcpyn(serverName, lpNetResource->lpRemoteName + 2, (int)min(MAX_PATH, (end - (lpNetResource->lpRemoteName + 2)) + 1));
            }
        }
        if (serverName[0] == 0) // this is not a \\server or \\server\ variant, we will solve it by calling the original code
        {
            *retErr = WNetAddConnection2(lpNetResource, NULL, NULL, CONNECT_INTERACTIVE);
            return *retErr == NO_ERROR;
        }
        ns = lpNetResource;
        remoteName = ns->lpRemoteName;
    }
    else
    {
        nsBuf.dwType = RESOURCETYPE_DISK;
        nsBuf.lpLocalName = (char*)name;
        nsBuf.lpRemoteName = (char*)remoteName;
        nsBuf.lpProvider = NULL;
        ns = &nsBuf;

        if (remoteName[0] == '\\' && remoteName[1] == '\\' && remoteName[2] != '\\')
        {
            const char* end = strchr(remoteName + 2, '\\');
            if (end != NULL && *(end + 1) != '\\' && *(end + 1) != 0)
            {
                const char* last = strchr(end + 2, '\\');
                if (last == NULL || *(last + 1) == 0) // the own dialog under XP+Vista will be only shown for simple UNC paths: "\\server\share" and "\\server\\share\\"
                    lstrcpyn(serverName, remoteName + 2, (int)min(MAX_PATH, (end - (remoteName + 2)) + 1));
            }
        }

        if (name != NULL &&                                                                     // mapped paths only (for the drive letter)
            GetUserName(name, remoteName, userNameBuf, USERNAME_MAXLEN, providerBuf, MAX_PATH)) // unknown user name for the restored connection
        {
            if (userNameBuf[0] != 0)
                userName = userNameBuf;
            if (providerBuf[0] != 0)
                nsBuf.lpProvider = providerBuf; // knowledge of the provider greatly speeds up the network response (at least on XP)
        }
    }

    CEnterPasswdDialog dlg(parent, remoteName, userName);

    DWORD err;
    char* passwd = NULL;

    // XP+Vista: we will dynamically extract functions for getting username+password in the standard dialog (including the option to save to Credential Manager - see "Manage your network passwords" in User Accounts in Control Panel)
    // Windows 7: there is a new dialog again and I have not yet discovered the interface for it
    HMODULE credUIDLL = !Windows7AndLater ? HANDLES(LoadLibrary("CREDUI.DLL")) : NULL;
    FT_CredUIPromptForCredentialsA credUIPromptForCredentialsA = NULL;
    FT_CredUIConfirmCredentialsA credUIConfirmCredentialsA = NULL;
    //  FT_CredUIParseUserNameA credUIParseUserNameA = NULL;
    if (credUIDLL != NULL)
    {
        credUIPromptForCredentialsA = (FT_CredUIPromptForCredentialsA)GetProcAddress(credUIDLL, "CredUIPromptForCredentialsA"); // Min: XP
        credUIConfirmCredentialsA = (FT_CredUIConfirmCredentialsA)GetProcAddress(credUIDLL, "CredUIConfirmCredentialsA");       // Min: XP
                                                                                                                                //    credUIParseUserNameA = (FT_CredUIParseUserNameA)GetProcAddress(credUIDLL, "CredUIParseUserNameA"); // Min: XP
    }
    if (!Windows7AndLater &&
        (credUIPromptForCredentialsA == NULL || credUIConfirmCredentialsA == NULL /*||
       credUIParseUserNameA == NULL*/
         ))
    {
        //    TRACE_E("RestoreNetworkConnection(): unable to use CredUIPromptForCredentialsA, CredUIConfirmCredentialsA, or credUIParseUserNameA function");
        TRACE_E("RestoreNetworkConnection(): unable to use CredUIPromptForCredentialsA or CredUIConfirmCredentialsAfunction");
    }

    BOOL connectInteractive = FALSE; // TRUE: Windows 7 always, XP+Vista only if CREDUI.DLL is not available or it is not a simple UNC path: we will use the CONNECT_INTERACTIVE flag (can do with "network passwords") and therefore the blocking call WNetAddConnection3 (must be in the same thread with the parent)
    BOOL confirmCred = FALSE;        // TRUE: calling CredUIConfirmCredentialsA is needed

    //  char domain[DOMAIN_MAXLEN];
    CREDUI_INFOA uiInfo = {0};
    uiInfo.cbSize = sizeof(uiInfo);
    uiInfo.hwndParent = parent;

    char captionBuf[MAX_PATH + 100];
    captionBuf[0] = 0;
    char messageBuf[MAX_PATH + 100];
    messageBuf[0] = 0;

    if (name == NULL) // mapping UNC paths to "none"
    {
        if (!Windows7AndLater && // there's a new dialog again on Windows 7 and I haven't found the interface for it yet
            credUIPromptForCredentialsA != NULL && credUIConfirmCredentialsA != NULL /*&& credUIParseUserNameA != NULL*/ &&
            serverName[0] != 0) // own dialog for entering username+password we will show only for simple UNC paths (\\server\share), we will leave DFS and others to the system
        {
            BOOL save = FALSE;

            err = credUIPromptForCredentialsA(&uiInfo, serverName, NULL, 0, dlg.User, sizeof(dlg.User),
                                              dlg.Passwd, sizeof(dlg.Passwd), &save,
                                              CREDUI_FLAGS_EXPECT_CONFIRMATION);
            if (err == ERROR_CANCELLED)
                ret = FALSE; // user Canceled
            else
            {
                if (lpNetResource == NULL)
                    UpdateWindow(MainWindow->HWindow); // doesn't make sense to use from nethood
                if (err == NO_ERROR)                   // user confirmed with OK
                {
                    confirmCred = TRUE;
                    lstrcpyn(userNameBuf, dlg.User, USERNAME_MAXLEN);
                    userName = userNameBuf;
                    passwd = dlg.Passwd;
                    /* // there was a problem with trimming the domain from the username, they just couldn't log in because we made their domain name local
          if (credUIParseUserNameA(dlg.User, userNameBuf, USERNAME_MAXLEN, domain, DOMAIN_MAXLEN) == NO_ERROR)
          {
            confirmCred = TRUE;
            userName = userNameBuf;
            passwd = dlg.Passwd;
          }
          else
          {
            TRACE_E("RestoreNetworkConnection(): CredUIParseUserNameA failed for: " << dlg.User);
            userNameBuf[0] = 0;
            userName = NULL;
            credUIConfirmCredentialsA(serverName, FALSE);
            connectInteractive = TRUE;  // another error, let the system variant deal with it
            err = ERROR_BAD_USERNAME;
          }
*/
                }
                else
                    connectInteractive = TRUE; // another error, let the system variant deal with it
            }
            if (!confirmCred)
                memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
        }
        else
            connectInteractive = TRUE;
    }
    else
    {
        if (!Windows7AndLater)
        {
            _snprintf_s(captionBuf, _TRUNCATE, LoadStr(IDS_RECONNET_TITLE), remoteName);
            _snprintf_s(messageBuf, _TRUNCATE, LoadStr(IDS_RECONNET_TEXT), remoteName);
            uiInfo.pszMessageText = messageBuf;
            uiInfo.pszCaptionText = captionBuf;
        }
    }

    while (ret)
    {
        err = 0;
        DWORD errProviderCode = 0;
        char errBuf[300];
        errBuf[0] = 0;
        char errProviderName[200];
        errProviderName[0] = 0;

        if (connectInteractive) // let's WNetAddConnection3 show the standard window for entering user+password (also solves the Credential Manager)
        {
            err = WNetAddConnection3(parent, ns, passwd, userName, CONNECT_INTERACTIVE | (name != NULL ? CONNECT_UPDATE_PROFILE : 0)); // we will remember the non-anonymous
            memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
            passwd = NULL;

            if (err == ERROR_CANCELLED) // cancel
            {
                ret = FALSE;
                break;
            }
            if (err == ERROR_EXTENDED_ERROR &&
                WNetGetLastError(&errProviderCode, errBuf, 300, errProviderName, 200) != NO_ERROR)
            {
                errProviderCode = 0;
                errBuf[0] = 0;
                errProviderName[0] = 0;
            }
        }
        else
        {
            BOOL brk = FALSE;
            HCURSOR oldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));
            if (lpNetResource != NULL)
            {
                err = WNetAddConnection2(ns, passwd, userName, 0);

                if (err == ERROR_EXTENDED_ERROR &&
                    WNetGetLastError(&errProviderCode, errBuf, 300, errProviderName, 200) != NO_ERROR)
                {
                    errProviderCode = 0;
                    errBuf[0] = 0;
                    errProviderName[0] = 0;
                }
            }
            else
            {
                CreateSafeWaitWindow(LoadStr(IDS_TRYINGRECONNECTESC), NULL, 3000, TRUE, NULL);

                brk = !NonBlockingWNetAddConnection3(err, ns, passwd, userName,
                                                     name != NULL ? CONNECT_UPDATE_PROFILE : 0, // we will remember the non-anonymous
                                                     &errProviderCode, errBuf, errProviderName);

                DestroySafeWaitWindow();
            }
            SetCursor(oldCur);

            memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
            passwd = NULL;

            if (confirmCred) // we will report the result of the password verification (according to this, it will or will not be saved in the Credential Manager)
            {
                credUIConfirmCredentialsA(serverName, err == ERROR_SUCCESS);
                confirmCred = FALSE;
            }

            if (brk)
            {
                ret = FALSE; // ESC in NonBlockingWNetAddConnection3()
                err = ERROR_CANCELLED;
                break;
            }
        }

        if (err == ERROR_SESSION_CREDENTIAL_CONFLICT)
        {
            if (lpNetResource == NULL) // the error will be shown in nethood, we will not show it here
            {
                SalMessageBox(parent, LoadStr(IDS_CREDENTIALCONFLICT), LoadStr(IDS_NETWORKERROR),
                              MB_OK | MB_ICONEXCLAMATION);
            }
            ret = FALSE;
            break;
        }

        if (err == ERROR_ALREADY_ASSIGNED)
        {
            // probably a mistake - we are showing that the drive is not connected and yet it is connected -> let
            // rebuild drive bar and change drive menu
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            break; // return TRUE
        }

        if (IsLogonFailureErr(err))
        {
            if (err == ERROR_EXTENDED_ERROR && errBuf[0] != 0)
            {
                char buf[530];
                sprintf(buf, "%s%s(%u) %s", errProviderName, (errProviderName[0] != 0 ? ": " : ""), errProviderCode, errBuf);
                SalMessageBox(parent, buf, LoadStr(IDS_NETWORKERROR), MB_OK | MB_ICONEXCLAMATION | (lpNetResource != NULL ? MB_SETFOREGROUND : 0));
            }
            else
            {
                if (name == NULL || !IsBadUserNameOrPasswdErr(err))
                {
                    SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                                  MB_OK | MB_ICONEXCLAMATION | (lpNetResource != NULL ? MB_SETFOREGROUND : 0));
                }
            }

            BOOL newConnectInteractive = FALSE;
            if (!Windows7AndLater && // there's a new dialog again on Windows 7 and I haven't found the interface for it yet
                credUIPromptForCredentialsA != NULL && credUIConfirmCredentialsA != NULL /*&& credUIParseUserNameA != NULL*/ &&
                serverName[0] != 0) // own dialog for entering username+password we will show only for simple UNC paths (\\server\share), we will leave DFS and others to the system
            {
                BOOL save = FALSE;
                err = credUIPromptForCredentialsA(&uiInfo, serverName, NULL, 0, dlg.User, sizeof(dlg.User),
                                                  dlg.Passwd, sizeof(dlg.Passwd), &save,
                                                  (name != NULL ? CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_GENERIC_CREDENTIALS : CREDUI_FLAGS_EXPECT_CONFIRMATION));
                if (err == ERROR_CANCELLED)
                {
                    ret = FALSE; // user Canceled
                    break;
                }
                else
                {
                    if (lpNetResource == NULL)
                        UpdateWindow(MainWindow->HWindow); // doesn't make sense to use from nethood
                    if (err == NO_ERROR)                   // user confirmed with OK
                    {
                        if (name == NULL)
                            confirmCred = TRUE;
                        lstrcpyn(userNameBuf, dlg.User, USERNAME_MAXLEN);
                        userName = name != NULL && dlg.User[0] == 0 ? NULL : userNameBuf;
                        passwd = name != NULL && dlg.User[0] == 0 && dlg.Passwd[0] == 0 ? NULL : dlg.Passwd;
                        continue;
                        /*  // there was a problem with trimming the domain from the username, they just couldn't log in because we made their domain name local
            BOOL onlyUserName = name != NULL && strchr(dlg.User, '\\') == NULL && strchr(dlg.User, '@') == NULL;
            if (name != NULL && dlg.User[0] == 0 || onlyUserName ||
                credUIParseUserNameA(dlg.User, userNameBuf, USERNAME_MAXLEN, domain, DOMAIN_MAXLEN) == NO_ERROR)
            {
              if (name == NULL) confirmCred = TRUE;
              if (onlyUserName) lstrcpyn(userNameBuf, dlg.User, USERNAME_MAXLEN);
              userName = name != NULL && dlg.User[0] == 0 ? NULL : userNameBuf;
              passwd = name != NULL && dlg.User[0] == 0 && dlg.Passwd[0] == 0 ? NULL : dlg.Passwd;
              continue;
            }
            else
            {
              TRACE_E("RestoreNetworkConnection(): CredUIParseUserNameA failed for: " << dlg.User);
              userNameBuf[0] = 0;
              userName = NULL;
              if (name == NULL) credUIConfirmCredentialsA(serverName, FALSE);
              newConnectInteractive = TRUE;  // other error, let the system variant deal with it
              err = ERROR_BAD_USERNAME;
            }
*/
                    }
                    else
                        newConnectInteractive = TRUE; // other error, let the system variant deal with it
                }
                memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
                passwd = NULL;
            }
            else
                newConnectInteractive = TRUE;

            if (newConnectInteractive)
            {
                if (!connectInteractive) // let's try an interactive mode only if we haven't used it already (a defense against an infinite cycle)
                {
                    connectInteractive = TRUE;
                    continue;
                }
                else
                {
                    ret = FALSE;
                    break;
                }
            }
        }

        if (err != ERROR_SUCCESS && err != ERROR_DEVICE_ALREADY_REMEMBERED)
        {
            if (lpNetResource == NULL) // the error will be shown in nethood, we will not show it here
            {
                SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                              MB_OK | MB_ICONEXCLAMATION);
            }
            ret = FALSE;
            break;
        }
        else
        {
            // At Tomas Jelinek, after connecting to a non-accessible UNC drive, no notification
            // was sent and therefore the DriveBar was not rebuilt and the drive remained crossed out
            // at me on W2K, notification comes only after two seconds
            // why not to force it right away:
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            break; // returning TRUE
        }
    }
    memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
    if (credUIDLL != NULL)
        HANDLES(FreeLibrary(credUIDLL));
    if (retErr != NULL)
        *retErr = err;
    return ret;
}

BOOL CheckAndRestoreNetworkConnection(HWND parent, const char drive, BOOL& pathInvalid)
{
    CALL_STACK_MESSAGE2("CheckAndRestoreNetworkConnection(, %u,)", drive);
    pathInvalid = FALSE;

    DWORD netDrives; // bit array of network drives
    char netRemotePath['z' - 'a' + 1][MAX_PATH];

    GetNetworkDrives(netDrives, netRemotePath);

    if (netDrives & (1 << (LowerCase[drive] - 'a'))) // disk existed, try to restore
    {
        char name[4] = " :";
        name[0] = drive;
        if (GetLogicalDrives() & (1 << (LowerCase[drive] - 'a'))) // theoretically there is nothing to do, it is accessible ... but on Vista after hibernation, the mapped disk can return the same error over and over again (e.g. "(31) device attached to system is not functioning"), we will discuss it only by accessing the UNC path, probably some MS error, but in Explorer it works, who knows what they are doing there
        {
            name[2] = '\\';
            name[3] = 0;
            DWORD err = NO_ERROR;
            char* netPath = netRemotePath[LowerCase[drive] - 'a'];
            if (netPath[0] == '\\' && netPath[1] == '\\' && strchr(netPath + 2, '\\') != NULL &&                                   // at least a primitive test of a valid UNC path
                (err = SalCheckPath(FALSE, name, ERROR_SUCCESS, TRUE, parent)) != ERROR_SUCCESS && err != ERROR_USER_TERMINATED && // mapped disk is not accessible
                (err = SalCheckPath(FALSE, netPath, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS &&                              // UNC is accessible
                (err = SalCheckPath(FALSE, name, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)                                   // now the mapped disk is accessible again
            {
                return TRUE;
            }
            pathInvalid = err == ERROR_USER_TERMINATED;
            return FALSE;
        }
        else
        {
            pathInvalid = !RestoreNetworkConnection(parent, name, netRemotePath[LowerCase[drive] - 'a']);
            return !pathInvalid;
        }
    }
    return FALSE;
}

DWORD_PTR SHGetFileInfoAux(LPCTSTR pszPath, DWORD dwFileAttributes, SHFILEINFO* psfi,
                           UINT cbFileInfo, UINT uFlags);

BOOL CheckAndConnectUNCNetworkPath(HWND parent, const char* UNCPath, BOOL& pathInvalid,
                                   BOOL donotReconnect)
{
    CALL_STACK_MESSAGE3("CheckAndConnectUNCNetworkPath(, %s, , %d)", UNCPath, donotReconnect);
    pathInvalid = FALSE;
    if (!IsUNCPath(UNCPath) || UNCPath[2] == '?')
        return FALSE; // no basic format UNC path

    char root[MAX_PATH + 4];
    char* s = root + GetRootPath(root, UNCPath);
    strcpy(s, "*.*");

    WIN32_FIND_DATA data;
    HANDLE h;
    if ((h = HANDLES_Q(FindFirstFile(root, &data))) != INVALID_HANDLE_VALUE)
    { // UNC root path is accessible, we will not do anything
        HANDLES(FindClose(h));
    }
    else // UNC root of the path is not listable
    {
        *(s - 1) = 0; // we will trim the backslash at the end of the root path
        DWORD err = GetLastError();
        BOOL trySharepoint = err == ERROR_BAD_NET_NAME; // try to call shell when error 67 occurs, so that it can make the path accessible
        if (!donotReconnect &&
            (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) // on XP, this error is returned if the user does not have access to the server or the share does not exist on the server, to distinguish these two errors, we call WNetAddConnection3
        {
            NETRESOURCE ns;
            ns.dwType = RESOURCETYPE_DISK;
            ns.lpLocalName = NULL;
            ns.lpRemoteName = root;
            ns.lpProvider = NULL;
            if (!NonBlockingWNetAddConnection3(err, &ns, NULL, NULL, 0, NULL, NULL, NULL))
                err = ERROR_PATH_NOT_FOUND; // ESC
        }

        if (IsLogonFailureErr(err) || // a defense against e.g. "no files found"
            IsAdminShareExtraLogonFailureErr(err, root))
        {
            if (donotReconnect)
                pathInvalid = TRUE; // we report the error directly
            else
            {
                // Petr: I commented out, Explorer or TC do not show any error before displaying the dialog for entering user + password; besides
                //       we now show a new error after trying to establish a connection (so that the user finds out that he has an expired password, etc.)
                //          SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                //                        MB_OK | MB_ICONEXCLAMATION);

                pathInvalid = !RestoreNetworkConnection(parent, NULL, root);
                return !pathInvalid;
            }
        }
        else
        {
            if (trySharepoint) // we will try to call shell when error 67 occurs, so that it can make the path accessible
            {
                SHFILEINFO fi;
                if (SHGetFileInfoAux(UNCPath, 0, &fi, sizeof(fi), SHGFI_ATTRIBUTES))
                    return TRUE;
            }
        }
    }
    return FALSE;
}

// ****************************************************************************
// tenhle kod je vzaty z Knowledge Base a slouzi k ziskani informaci o typu media
// floppy mechaniky (3.5", 5.25", 8")

/*
  GetDriveFormFactor returns the drive form factor.

  It returns 350 if the drive is a 3.5" floppy drive.
  It returns 525 if the drive is a 5.25" floppy drive.
  It returns 800 if the drive is a 8" floppy drive.
  It returns   1 if the drive supports removable media other than 3.5", 5.25", and 8" floppies.
  It returns   0 on error.

  iDrive is 1 for drive A:, 2 for drive B:, etc.
*/

DWORD GetDriveFormFactor(int iDrive)
{
    CALL_STACK_MESSAGE2("GetDriveFormFactor(%d)", iDrive);
    HANDLE h;
    char tsz[8];
    DWORD dwRc = 0;

    /*
     On Windows NT, use the technique described in the Knowledge
     Base article Q115828 and in the "FLOPPY" SDK sample.
  */
    sprintf(tsz, "\\\\.\\%c:", '@' + iDrive);
    h = HANDLES_Q(CreateFile(tsz, 0, FILE_SHARE_WRITE, 0, OPEN_EXISTING, 0, 0));
    if (h != INVALID_HANDLE_VALUE)
    {
        DISK_GEOMETRY Geom[20];
        DWORD cb;
        if (DeviceIoControl(h, IOCTL_STORAGE_GET_MEDIA_TYPES, 0, 0, Geom, sizeof(Geom), &cb, 0) && cb > 0)
        {
            switch (Geom[0].MediaType)
            {
            case F5_160_512:    // 5.25 160K  floppy
            case F5_180_512:    // 5.25 180K  floppy
            case F5_320_512:    // 5.25 320K  floppy
            case F5_320_1024:   // 5.25 320K  floppy
            case F5_360_512:    // 5.25 360K  floppy
            case F5_640_512:    // 5.25 640K  floppy
            case F5_720_512:    // 5.25 720K  floppy
            case F5_1Pt2_512:   // 5.25 1.2MB floppy
            case F5_1Pt23_1024: // 5.25 1.23MB floppy
                dwRc = 525;
                break;

            case F3_640_512:    // 3.5 640K   floppy
            case F3_720_512:    // 3.5 720K   floppy
            case F3_1Pt2_512:   // 3.5 1.2MB floppy
            case F3_1Pt23_1024: // 3.5 1.23MB floppy
            case F3_1Pt44_512:  // 3.5 1.44MB floppy
            case F3_2Pt88_512:  // 3.5 2.88MB floppy
            case F3_20Pt8_512:  // 3.5 20.8MB floppy
            case F3_32M_512:    // 3.5 32MB   floppy
            case F3_120M_512:   // 3.5 120MB  floppy
            case F3_128Mb_512:  // 3.5 120MB  magneto-optical (MO) media
            case F3_230Mb_512:  // 3.5 230MB  magneto-optical (MO) media
            case F3_200Mb_512:  // 3.5 200MB  floppy (HiFD)
            case F3_240M_512:   // 3.5 240MB  floppy (HiFD)
                dwRc = 350;
                break;

            case F8_256_128: // 8 256K floppy
                dwRc = 800;
                break;

            case RemovableMedia: // removable media other than a floppy disk
                dwRc = 1;
                break;
            }
        }
        HANDLES(CloseHandle(h));
    }
    return dwRc;
}

// ****************************************************************************

void DisplayMenuAux(IContextMenu2* contextMenu, CMINVOKECOMMANDINFO* ici)
{
    CALL_STACK_MESSAGE_NONE

    // temporary lower the thread priority so that some confused shell extension doesn't eat our CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        contextMenu->InvokeCommand(ici);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 1))
    {
        ICExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

void DisplayMenuAux2(IContextMenu2* contextMenu, HMENU h)
{
    CALL_STACK_MESSAGE_NONE
    // temporary lower the thread priority so that some confused shell extension doesn't eat our CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, no need to release
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        UINT flags = CMF_NORMAL | CMF_EXPLORE;
        // we will take care of pressing the shift key - extended context menu, under W2K there is Run as ...
#define CMF_EXTENDEDVERBS 0x00000100 // rarely used verbs
        BOOL shiftPressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        if (shiftPressed)
            flags |= CMF_EXTENDEDVERBS;

        contextMenu->QueryContextMenu(h, 0, 0, -1, flags);
    }
    __except (CCallStack::HandleException(GetExceptionInformation(), 2))
    {
        QCMExceptionHasOccured++;
    }

    SetThreadPriority(hThread, oldThreadPriority);
}

//*****************************************************************************
//
// CDrivesList
//

CDrivesList::CDrivesList(CFilesWindow* filesWindow, const char* currentPath,
                         CDriveTypeEnum* driveType, DWORD_PTR* driveTypeParam,
                         int* postCmd, void** postCmdParam, BOOL* fromContextMenu)
{
    CALL_STACK_MESSAGE_NONE
    Drives = new TDirectArray<CDriveData>(30, 30);
    MenuPopup = NULL;
    FilesWindow = filesWindow;
    DriveType = driveType;
    DriveTypeParam = driveTypeParam;
    lstrcpy(CurrentPath, currentPath);
    PostCmd = postCmd;
    PostCmdParam = postCmdParam;
    FromContextMenu = fromContextMenu;
    CachedDrivesMask = 0;
    CachedCloudStoragesMask = 0;

    // standardne nejde o post-cmd, proto nulujeme
    *PostCmd = 0;
    *PostCmdParam = NULL;
    *FromContextMenu = FALSE;
}

CDriveTypeEnum
CDrivesList::OwnGetDriveType(const char* rootPath)
{
    CALL_STACK_MESSAGE_NONE
    UINT dt = GetDriveType(rootPath);
    CDriveTypeEnum ret = drvtUnknow;
    switch (dt)
    {
    case DRIVE_REMOVABLE:
        ret = drvtRemovable;
        break;
    case DRIVE_NO_ROOT_DIR: // subst of which the directory was deleted (can also be remote, but we don't care about that)
    case DRIVE_FIXED:
        ret = drvtFixed;
        break;
    case DRIVE_REMOTE:
        ret = drvtRemote;
        break;
    case DRIVE_CDROM:
        ret = drvtCDROM;
        break;
    case DRIVE_RAMDISK:
        ret = drvtRAMDisk;
        break;
    }
    return ret;
}

void GetDisplayNameFromSystem(const char* root, char* volumeName, int volumeNameBufSize)
{
    CALL_STACK_MESSAGE2("GetDisplayNameFromSystem(%s)", root);

    SHFILEINFO fi;
    if (SHGetFileInfo(root, 0, &fi, sizeof(fi), SHGFI_DISPLAYNAME))
    {
        lstrcpyn(volumeName, fi.szDisplayName, volumeNameBufSize);
        char* s = strrchr(volumeName, '(');
        if (s != NULL)
        {
            while (s > volumeName && *(s - 1) == ' ')
                s--;
            *s = 0;
        }
    }
}

unsigned ReadCDVolNameThreadFBody(void* param) // directory accessibility test
{
    CALL_STACK_MESSAGE1("ReadCDVolNameThreadFBody()");
    UINT_PTR uid = (UINT_PTR)param;
    char root[MAX_PATH];
    root[0] = 0;
    char buf[MAX_PATH];
    buf[0] = 0;

    HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
    BOOL run = FALSE;
    if (uid == ReadCDVolNameReqUID) // someone is still waiting for an answer
    {
        lstrcpyn(root, ReadCDVolNameBuffer, MAX_PATH);
        run = TRUE;
    }
    HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));

    if (run)
    {
        DWORD dummy;
        char fileSystem[11];
        if (!GetVolumeInformation(root, buf, MAX_PATH, NULL, &dummy, &dummy, fileSystem, 10))
            buf[0] = 0; // error GetVolumeInformation
        if (buf[0] == 0)
            GetDisplayNameFromSystem(root, buf, MAX_PATH);

        HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
        if (uid == ReadCDVolNameReqUID) // someone is still waiting for an answer
            lstrcpyn(ReadCDVolNameBuffer, buf, MAX_PATH);
        HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));
    }
    return 0;
}

unsigned ReadCDVolNameThreadFEH(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    __try
    {
#endif // CALLSTK_DISABLE
        return ReadCDVolNameThreadFBody(param);
#ifndef CALLSTK_DISABLE
    }
    __except (CCallStack::HandleException(GetExceptionInformation()))
    {
        TRACE_I("Thread ReadCDVolName: calling ExitProcess(1).");
        //    ExitProcess(1);
        TerminateProcess(GetCurrentProcess(), 1); // harder exit (this one still calls something)
        return 1;
    }
#endif // CALLSTK_DISABLE
}

DWORD WINAPI ReadCDVolNameThreadF(void* param)
{
    CALL_STACK_MESSAGE_NONE
#ifndef CALLSTK_DISABLE
    CCallStack stack;
#endif // CALLSTK_DISABLE
    return ReadCDVolNameThreadFEH(param);
}

void SortPluginFSTimes(CPluginFSInterfaceEncapsulation** list, int left, int right)
{
    int i = left, j = right;
    CPluginFSInterfaceEncapsulation* pivot = list[(i + j) / 2];

    do
    {
        while (list[i]->GetPluginFSCreateTime() < pivot->GetPluginFSCreateTime() && i < right)
            i++;
        while (pivot->GetPluginFSCreateTime() < list[j]->GetPluginFSCreateTime() && j > left)
            j--;

        if (i <= j)
        {
            CPluginFSInterfaceEncapsulation* swap = list[i];
            list[i] = list[j];
            list[j] = swap;
            i++;
            j--;
        }
    } while (i <= j);

    if (left < j)
        SortPluginFSTimes(list, left, j);
    if (i < right)
        SortPluginFSTimes(list, i, right);
}

char* CreateIndexedDrvText(const char* driveText, int index)
{
    char* newText = (char*)malloc(strlen(driveText) + 15); // we will keep a reserve of 14 characters (" [-1234567890]")
    if (newText != NULL)
    {
        const char* s = driveText;
        while (*s != 0 && *s != '\t')
            s++;
        if (*s == '\t')
            s++;
        while (*s != 0 && *s != '\t')
            s++;
        memcpy(newText, driveText, s - driveText);
        sprintf(newText + (s - driveText), " [%d]%s", index, s);
    }
    return newText;
}

int GetIndexForDrvText(CPluginFSInterfaceEncapsulation** fsList, int count,
                       CPluginFSInterfaceAbstract* fsIface, int currentIndex)
{
    CPluginFSInterfaceEncapsulation* fs = NULL;
    int z;
    for (z = 0; z < count; z++) // found encapsulation FS (if there is a lack of memory, the indexes in fsList and Drives (from the firstFSIndex offset) may not match)
    {
        if (fsList[z]->GetInterface() == fsIface)
        {
            fs = fsList[z];
            break;
        }
    }
    if (fs != NULL) // selection of the index for the item (either from FS or we assign sequentially)
    {
        if (fs->GetChngDrvDuplicateItemIndex() < currentIndex)
            fs->SetChngDrvDuplicateItemIndex(currentIndex);
        else
            currentIndex = fs->GetChngDrvDuplicateItemIndex();
    }
    return currentIndex;
}

// based on 'hSrcIcon' creates a black and white version of the icon
// WARNING, SLOW, use with caution
HICON ConvertIcon16x16ToGray(HICON hSrcIcon)
{
    CIconList il;
    if (il.Create(16, 16, 1))
    {
        il.ReplaceIcon(0, hSrcIcon);
        CIconList ilGray;
        if (ilGray.CreateAsCopy(&il, TRUE))
        {
            return ilGray.GetIcon(0, TRUE);
        }
    }
    return NULL;
}

const char Base64Table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

// performs in-place decode from base64; returns success;
// except of the state when 'input_length' is zero, returns zero-terminated string
BOOL base64_decode(char* data, int input_length, int* output_length, const char* errPrefix)
{
    char decoding_table[256];
    memset(decoding_table, 0xFF, sizeof(decoding_table));
    for (int i = 0; i < 64; i++)
        decoding_table[(unsigned char)Base64Table[i]] = i;

    if (input_length % 4 != 0)
    {
        TRACE_E(errPrefix << "base64_decode(): invalid length of base64 encoded sequence, unable to decode!");
        return FALSE;
    }

    *output_length = input_length / 4 * 3;
    if (input_length > 0 && data[input_length - 1] == '=')
        (*output_length)--;
    if (input_length > 0 && data[input_length - 2] == '=')
        (*output_length)--;

    for (int i = 0, j = 0; i < input_length;)
    {
        DWORD sextet_a = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        DWORD sextet_b = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        DWORD sextet_c = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];
        DWORD sextet_d = data[i] == '=' ? 0 & i++ : decoding_table[data[i++]];

        if (sextet_a == 0xFF || sextet_b == 0xFF || sextet_c == 0xFF || sextet_d == 0xFF)
        {
            TRACE_E(errPrefix << "base64_decode(): invalid base64 encoded sequence, unable to decode!");
            return FALSE;
        }
        DWORD triple = (sextet_a << 3 * 6) + (sextet_b << 2 * 6) + (sextet_c << 1 * 6) + (sextet_d << 0 * 6);

        if (j < *output_length)
            data[j++] = (triple >> 2 * 8) & 0xFF;
        if (j < *output_length)
            data[j++] = (triple >> 1 * 8) & 0xFF;
        if (j < *output_length)
            data[j++] = (triple >> 0 * 8) & 0xFF;
    }
    if (*output_length > 0)
        data[*output_length] = 0;
    return TRUE;
}

// a path to the local Dropbox directory
char DropboxPath[MAX_PATH] = "";

void InitDropboxPath()
{
    static BOOL alreadyCalled = FALSE;
    if (!alreadyCalled) // it makes sense to find the path only once, then we just ignore it
    {
        DropboxPath[0] = 0;
        char sDbPath[MAX_PATH];
        BOOL cfgAlreadyFound = FALSE;
        if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT */, sDbPath) == S_OK)
        {
            if (SalPathAppend(sDbPath, "Dropbox\\host.db", MAX_PATH) && FileExists(sDbPath))
                cfgAlreadyFound = TRUE;
        }
        else
            TRACE_E("Cannot get value of CSIDL_APPDATA!");
        if (cfgAlreadyFound ||
            SHGetFolderPath(NULL, CSIDL_LOCAL_APPDATA, NULL, 0 /* SHGFP_TYPE_CURRENT */, sDbPath) == S_OK)
        {
            if (cfgAlreadyFound ||
                SalPathAppend(sDbPath, "Dropbox\\host.db", MAX_PATH) && FileExists(sDbPath))
            {
                HANDLE hFile = HANDLES_Q(CreateFile(sDbPath, GENERIC_READ,
                                                    FILE_SHARE_READ | FILE_SHARE_WRITE, NULL,
                                                    OPEN_EXISTING,
                                                    FILE_FLAG_SEQUENTIAL_SCAN,
                                                    NULL));
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    LARGE_INTEGER size;
                    if (GetFileSizeEx(hFile, &size) && size.QuadPart < 100000) // 100KB is enough for a stupid config file
                    {
                        char* buf = (char*)malloc(size.LowPart);
                        DWORD read;
                        if (ReadFile(hFile, buf, size.LowPart, &read, NULL) && read == size.LowPart)
                        {
                            char* secRow = buf;
                            char* end = buf + read;
                            while (secRow < end && *secRow != '\r' && *secRow != '\n')
                                secRow++;
                            if (secRow < end && *secRow == '\r')
                                secRow++;
                            if (secRow < end && *secRow == '\n')
                                secRow++;
                            char* secRowEnd = secRow;
                            while (secRowEnd < end && *secRowEnd != '\r' && *secRowEnd != '\n')
                                secRowEnd++;
                            if (secRow < secRowEnd) // I have a text 2. rows, where the base64 encoded searched path is
                            {
                                int pathLen;
                                if (base64_decode(secRow, (int)(secRowEnd - secRow), &pathLen, "Dropbox path: "))
                                {
                                    WCHAR widePath[MAX_PATH]; // we can't do longer paths anyway
                                    char mbPath[MAX_PATH];    // ANSI or UTF8 path
                                    if (ConvertA2U(secRow, -1, widePath, _countof(widePath), CP_UTF8) &&
                                        ConvertU2A(widePath, -1, mbPath, _countof(mbPath)))
                                    {
                                        TRACE_I("Dropbox path: " << mbPath);
                                        strcpy_s(DropboxPath, mbPath);
                                    }
                                    else
                                        TRACE_E("Dropbox path is too big or not convertible to ANSI string.");
                                }
                            }
                        }
                        else
                            TRACE_E("Unable to read Dropbox's configuration file: " << sDbPath);
                        free(buf);
                    }
                    else
                        TRACE_E("Dropbox's configuration file is too large: " << sDbPath);
                    HANDLES(CloseHandle(hFile));
                }
                else
                    TRACE_E("Cannot open Dropbox's configuration file: " << sDbPath);
            }
            else
                TRACE_I("Cannot find Dropbox's configuration file: " << sDbPath);
        }
        else
            TRACE_E("Cannot get value of CSIDL_LOCAL_APPDATA!");
    }
    alreadyCalled = TRUE;
}

#define my_DEFINE_KNOWN_FOLDER(name, l, w1, w2, b1, b2, b3, b4, b5, b6, b7, b8) \
    EXTERN_C const GUID DECLSPEC_SELECTANY name = {l, w1, w2, {b1, b2, b3, b4, b5, b6, b7, b8}}

// {A52BBA46-E9E1-435f-B3D9-28DAA648C0F6}  // OneDriver folder from the system, introduced only since Windows 8.1
my_DEFINE_KNOWN_FOLDER(my_FOLDERID_SkyDrive, 0xa52bba46, 0xe9e1, 0x435f, 0xb3, 0xd9, 0x28, 0xda, 0xa6, 0x48, 0xc0, 0xf6);

// the path to the local OneDrive folder - Personal (only for personal accounts, for business accounts we have OneDriveBusinessStorages)
char OneDrivePath[MAX_PATH] = "";

// the paths to local OneDrive folders - Business (only for business accounts, for personal accounts we have OneDrivePath)
COneDriveBusinessStorages OneDriveBusinessStorages;

void COneDriveBusinessStorages::SortIn(COneDriveBusinessStorage* s)
{
    if (s != NULL)
    {
        for (int i = 0; i < Count; i++)
        {
            if (StrICmp(s->DisplayName, At(i)->DisplayName) <= 0)
            {
                Insert(i, s);
                return;
            }
        }
        Add(s);
    }
}

BOOL COneDriveBusinessStorages::Find(const char* displayName, const char** userFolder)
{
    if (displayName != NULL)
    {
        for (int i = 0; i < Count; i++)
        {
            if (StrICmp(displayName, At(i)->DisplayName) == 0)
            {
                if (userFolder != NULL)
                    *userFolder = At(i)->UserFolder;
                return TRUE;
            }
        }
    }
    if (userFolder != NULL)
        *userFolder = NULL;
    return FALSE;
}

void InitOneDrivePath()
{
    OneDrivePath[0] = 0; // we find out the path to OneDrive over and over again, because after the commend "Unlink OneDrive" (from OneDrive) we should stop showing it

    BOOL done = FALSE;
    if (WindowsVistaAndLater) // SHGetKnownFolderPath has existed since Vista
    {
        typedef HRESULT(WINAPI * FSHGetKnownFolderPath)(REFKNOWNFOLDERID rfid,
                                                        DWORD /* KNOWN_FOLDER_FLAG */ dwFlags,
                                                        HANDLE hToken,
                                                        PWSTR * ppszPath); // free *ppszPath with CoTaskMemFree
        FSHGetKnownFolderPath DynSHGetKnownFolderPath = (FSHGetKnownFolderPath)GetProcAddress(GetModuleHandle("shell32.dll"),
                                                                                              "SHGetKnownFolderPath");
        if (DynSHGetKnownFolderPath != NULL)
        {
            PWSTR path = NULL;
            if (DynSHGetKnownFolderPath(my_FOLDERID_SkyDrive, 0, NULL, &path) == S_OK && path != NULL)
            {
                if (path[0] != 0) // FOLDERID_SkyDrive was introduced in Windows 8.1 = we should not need to hunt it in the registry
                {
                    done = ConvertU2A(path, -1, OneDrivePath, _countof(OneDrivePath)) != 0;
                    if (!done)
                        OneDrivePath[0] = 0; // just for sync
                                             //else TRACE_I("OneDrive path (FOLDERID_SkyDrive): " << OneDrivePath);
                }
                CoTaskMemFree(path);
            }
        }
    }

    HKEY hKey;
    char path[MAX_PATH];
    for (int i = 0; !done && i < 2; i++) // theoretically only needed for Windows 8 and lower (from 8.1 we have FOLDERID_SkyDrive)
    {
        if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER,
                                   Windows8_1AndLater && !Windows10AndLater ? (i == 0 ? "Software\\Microsoft\\Windows\\CurrentVersion\\OneDrive" : // jen Win 8.1
                                                                                   "Software\\Microsoft\\Windows\\CurrentVersion\\SkyDrive")
                                                                            : (i == 0 ? "Software\\Microsoft\\OneDrive" : "Software\\Microsoft\\SkyDrive"), // krom Win 8.1
                                   0, KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            DWORD size = sizeof(path); // size in bytes
            DWORD type;
            if (SalRegQueryValueEx(hKey, "UserFolder", 0, &type, (BYTE*)path, &size) == ERROR_SUCCESS &&
                type == REG_SZ && size > 1)
            {
                //TRACE_I("OneDrive path (UserFolder): " << path);
                strcpy_s(OneDrivePath, path); // we have needed path
                done = TRUE;
            }
            HANDLES(RegCloseKey(hKey));
        }
    }

    // we will load OneDrive Business storages from the registry
    OneDriveBusinessStorages.DestroyMembers();
    if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\OneDrive\\Accounts",
                               0, KEY_READ, &hKey)) == ERROR_SUCCESS)
    {
        DWORD i = 0;
        while (1)
        { // enumarate all extensions one by one
            char keyName[300];
            DWORD keyNameLen = _countof(keyName);
            if (RegEnumKeyEx(hKey, i, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            { // opening the account key
                HKEY hAccount;
                if (StrICmp(keyName, "Personal") != 0 && // we don't want to read personal account here (see OneDrivePath)
                    HANDLES_Q(RegOpenKeyEx(hKey, keyName, 0, KEY_READ, &hAccount)) == ERROR_SUCCESS)
                { // we read all accounts except Personal (for now only "Business???", where ??? is a number)
                    char disp[ONEDRIVE_MAXBUSINESSDISPLAYNAME];
                    DWORD size = sizeof(disp); // size in bytes
                    DWORD type;
                    if (SalRegQueryValueEx(hAccount, "DisplayName", 0, &type, (BYTE*)disp, &size) == ERROR_SUCCESS &&
                        type == REG_SZ && size > 1)
                    {
                        size = sizeof(path); // size in bytes
                        if (SalRegQueryValueEx(hAccount, "UserFolder", 0, &type, (BYTE*)path, &size) == ERROR_SUCCESS &&
                            type == REG_SZ && size > 1)
                        { // we will collect everything that has DisplayName and UserFolder, no matter what it is, we will offer it to the user under "OneDrive"
                            //TRACE_I("OneDrive Business: DisplayName: " << disp << ", UserFolder: " << path);
                            OneDriveBusinessStorages.SortIn(new COneDriveBusinessStorage(DupStr(disp), DupStr(path)));
                        }
                    }
                    HANDLES(RegCloseKey(hAccount));
                }
            }
            else
                break; // end of enumeration (can also be a small buffer, but I don't consider it reasonable to handle)
            i++;
        }
        HANDLES(RegCloseKey(hKey));
    }
}

int GetOneDriveStorages()
{
    return (OneDrivePath[0] != 0 ? 1 : 0) + OneDriveBusinessStorages.Count;
}

void CDrivesList::AddToDrives(CDriveData& drv, int textResId, char hotkey, CDriveTypeEnum driveType,
                              BOOL getGrayIcons, HICON icon, BOOL destroyIcon, const char* itemText)
{
    const char* s = itemText != NULL ? itemText : LoadStr(textResId);
    // WARNING: for drvtOneDriveBus, DisplayName is later taken from drv.DriveText, when changing the text format, change it !!!
    char* txt = (char*)malloc(1 + (hotkey == 0 ? 0 : 1) + strlen(s) + 1);
    strcpy(txt, hotkey == 0 ? "\t" : " \t");
    if (hotkey != 0)
        *txt = hotkey;
    strcat(txt, s);
    drv.DriveType = driveType;
    drv.DriveText = txt;
    drv.Param = 0;
    drv.Accessible = TRUE;
    drv.DestroyIcon = destroyIcon;
    drv.HIcon = icon;
    if (getGrayIcons && !destroyIcon)
        TRACE_C("CDrivesList::AddToDrives(): unsupported combination of parameters"); // getGrayIcons is TRUE only for drive-bar and there is only one icon (button)
    if (getGrayIcons)
        drv.HGrayIcon = ConvertIcon16x16ToGray(drv.HIcon);
    else
        drv.HGrayIcon = NULL;
    drv.Shared = FALSE;
    Drives->Add(drv);
}

BOOL CDrivesList::BuildData(BOOL noTimeout, TDirectArray<CDriveData>* copyDrives,
                            DWORD copyCachedDrivesMask, BOOL getGrayIcons, BOOL forDriveBar)
{
    CALL_STACK_MESSAGE4("CDrivesList::BuildData(%d, , , %d, %d)", noTimeout, getGrayIcons, forDriveBar);

    int neighborhoodIndex = -1;
    int currentDiskIndex = -1;
    int currentFSIndex = -1;

    int i = 1;
    char root[10] = " :\\";

    // two lowest bit matches 'A', the second bit 'B', ...
    // available drives
    DWORD mask = GetLogicalDrives();

    CDriveData drv;
    drv.Param = 0;
    drv.DestroyIcon = TRUE; // these icons are allocated
    drv.PluginFS = NULL;    // just for sure
    drv.DLLName = NULL;     // just for sure
    drv.HGrayIcon = NULL;

    CDriveData drvSeparator;
    ZeroMemory(&drvSeparator, sizeof(drvSeparator));
    drvSeparator.DriveType = drvtSeparator;

    if (copyDrives != NULL) // optimization: data is just copied (e.g. it's the second Drives Bar, so we copy the data from the first Drives Bar, for which we got it from the system a few ms ago)
    {
        CachedDrivesMask = copyCachedDrivesMask;

        // I will add drives a..z
        int lastDiskDrv = -1;
        int x;
        for (x = 0; x < copyDrives->Count; x++)
        {
            CDriveData* driveData = &copyDrives->At(x);
            if (driveData->DriveType > drvtRAMDisk)
                break;
            if (driveData->DriveType != drvtSeparator)
                lastDiskDrv = x;
        }
        for (x = 0; x <= lastDiskDrv; x++)
        {
            CDriveData* driveData = &copyDrives->At(x);
            if (driveData->DriveType == drvtSeparator)
                Drives->Add(drvSeparator);
            else
            {
                drv.DriveType = driveData->DriveType;
                drv.DriveText = DupStr(driveData->DriveText);
                drv.Param = driveData->Param;
                drv.Accessible = driveData->Accessible;
                drv.Shared = driveData->Shared;
                root[0] = drv.DriveText[0];
                if (root[0] >= 'A' && root[0] <= 'Z')
                    i = 1 << (root[0] - 'A');
                else
                {
                    i = -1;
                    TRACE_E("CDrivesList::BuildData(): unexpected value of drv.DriveText");
                }
                int driveType = (mask & i) ? GetDriveType(root) : DRIVE_REMOTE;
                drv.HIcon = GetDriveIcon(root, driveType, drv.Accessible);
                drv.HGrayIcon = NULL;

                int index = Drives->Add(drv);
                if (LowerCase[Drives->At(index).DriveText[0]] == LowerCase[(char)*DriveTypeParam])
                    currentDiskIndex = index;
            }
        }
    }
    else
    {
        // remembered and not refreshed network drives will not be returned from GetLogicalDrives()
        DWORD netDrives; // bitove pole network disku
        char netRemotePath['z' - 'a' + 1][MAX_PATH];
        GetNetworkDrives(netDrives, netRemotePath);

        CachedDrivesMask = mask | netDrives; // a cache for a simple test of whether a disk has been added / disappeared

        // driver which should not be displayed to users
        DWORD noDrivesPolicy = SystemPolicies.GetNoDrives();
        noDrivesPolicy |= DRIVES_MASK & (~Configuration.VisibleDrives);

        char drive = 'A';

        CQuadWord freeSpace; // how much space we have on the disk

        Shares.PrepareSearch(""); // now we will search for drives roots

        BOOL separateNextDrive = FALSE; // before we insert drive, should we insert separator?

        // I will add drives a..z
        while (i != 0)
        {
            if (!(noDrivesPolicy & i) && ((mask & i) || (netDrives & i))) // disk is accessible
            {
                root[0] = drive;
                int driveType;
                if (mask & i)
                {
                    drv.DriveType = OwnGetDriveType(root);
                    driveType = GetDriveType(root);
                }
                else
                {
                    drv.DriveType = drvtRemote;
                    driveType = DRIVE_REMOTE;
                }
                drv.Shared = FALSE;
                drv.Accessible = (mask & i) != 0;
                char volumeName[MAX_PATH + 50];
                DWORD dummy;
                freeSpace = CQuadWord(-1, -1);
                switch (drv.DriveType)
                {
                case drvtRemovable: // diskettes, we will find out if it is 3.5 ", 5.25", 8" or unknown
                {
                    volumeName[0] = 0;
                    int drvIndex = drive - 'A' + 1;
                    if (drvIndex >= 1 && drvIndex <= 26) // we will do "range-check" for sure
                    {
                        DWORD medium = GetDriveFormFactor(drvIndex);
                        switch (medium)
                        {
                        case 350:
                            strcpy(volumeName, LoadStr(IDS_FLOPPY350));
                            break;
                        case 525:
                            strcpy(volumeName, LoadStr(IDS_FLOPPY525));
                            break;
                        case 800:
                            strcpy(volumeName, LoadStr(IDS_FLOPPY800));
                            break;
                        default:
                        {
                            GetDisplayNameFromSystem(root, volumeName, MAX_PATH);
                            if (volumeName[0] == 0)
                                strcpy(volumeName, LoadStr(IDS_REMOVABLE_DISK));
                            else
                                DuplicateAmpersands(volumeName, MAX_PATH);
                            break;
                        }
                        }
                    }
                    break;
                }

                case drvtFixed:
                case drvtRAMDisk:
                {
                    DWORD flags;
                    if (GetVolumeInformation(root, volumeName, MAX_PATH, NULL, &dummy, &flags, NULL, 0))
                    {
                        CQuadWord t;                              // total disk space
                        freeSpace = MyGetDiskFreeSpace(root, &t); // free disk space
                        // double '&' so that it is not displayed as an underline
                        DuplicateAmpersands(volumeName, MAX_PATH);
                        if (volumeName[0] == 0)
                            strcpy(volumeName, LoadStr(IDS_LOCAL_DISK));
                    }
                    else
                    {
                        volumeName[0] = 0;
                        freeSpace = CQuadWord(-1, -1);
                    }
                    break;
                }

                case drvtRemote:
                {
                    DWORD size = MAX_PATH;
                    char device[3] = " :";
                    device[0] = drive;
                    if (netDrives & i)
                    {
                        int l = (int)strlen(netRemotePath[drive - 'A']);
                        l = min(l, MAX_PATH - 1);
                        memmove(volumeName, netRemotePath[drive - 'A'], l);
                        volumeName[l] = 0;
                    }
                    else if (!drv.Accessible ||
                             WNetGetConnection(device, volumeName, &size) != NO_ERROR)
                    {
                        if (!GetSubstInformation(drive - 'A', volumeName, MAX_PATH))
                            volumeName[0] = 0;
                    }
                    // double '&' so that it is not displayed as an underline
                    DuplicateAmpersands(volumeName, MAX_PATH);
                    break;
                }

                case drvtCDROM:
                {
                    HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
                    UINT_PTR uid = ++ReadCDVolNameReqUID;
                    lstrcpyn(ReadCDVolNameBuffer, root, MAX_PATH);
                    HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));

                    // create a thread in which we will find out the volume_name of the CD drive
                    DWORD threadID;
                    HANDLE thread = HANDLES(CreateThread(NULL, 0, ReadCDVolNameThreadF,
                                                         (void*)uid, 0, &threadID));
                    if (thread != NULL && WaitForSingleObject(thread, noTimeout ? INFINITE : 500) == WAIT_OBJECT_0)

                    { // give it 500ms to find out the volume-name
                        HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
                        lstrcpyn(volumeName, ReadCDVolNameBuffer, MAX_PATH + 50);
                        HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));
                    }
                    else
                        volumeName[0] = 0;
                    if (thread != NULL)
                        AddAuxThread(thread, TRUE); // if the thread is not finished, we will kill it before closing the software
                    if (volumeName[0] == 0)
                        strcpy(volumeName, LoadStr(IDS_COMPACT_DISK));
                    else
                    {
                        // double '&' so that it is not displayed as an underline
                        DuplicateAmpersands(volumeName, MAX_PATH);
                    }
                    break;
                }

                default:
                    volumeName[0] = 0;
                }
                if (freeSpace != CQuadWord(-1, -1))
                {
                    char* p = volumeName + lstrlen(volumeName);
                    if (p == volumeName)
                        *p++ = '\t';
                    *p++ = '\t';
                    PrintDiskSize(p, freeSpace, 0);
                }
                if (volumeName[0] != 0)
                { // 'c: ' + volume + 0
                    drv.DriveText = (char*)malloc(2 + strlen(volumeName) + 1);
                    if (drv.DriveText == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        return FALSE;
                    }
                    strcpy(drv.DriveText, " \t");
                    drv.DriveText[0] = drive;
                    strcat(drv.DriveText, volumeName);
                }
                else
                {
                    drv.DriveText = (char*)malloc(2);
                    if (drv.DriveText == NULL)
                    {
                        TRACE_E(LOW_MEMORY);
                        return FALSE;
                    }
                    strcpy(drv.DriveText, " ");
                    drv.DriveText[0] = drive;
                }
                drv.HIcon = GetDriveIcon(root, driveType, drv.Accessible);
                drv.HGrayIcon = NULL;

                if (drv.DriveType != drvtRemote)
                {
                    drv.Shared = Shares.Search(root);
                }

                // we separate drives that the user wanted to separate
                if (separateNextDrive && Drives->Count > 0 && !IsLastItemSeparator())
                    Drives->Add(drvSeparator);
                // if there's no drive A or B in the system and the user has set the separator after A or B, we've shown it after C
                // that's why we have to drop the flag in any case, not just in the previous condition
                separateNextDrive = FALSE;

                int index = Drives->Add(drv);
                if (LowerCase[Drives->At(index).DriveText[0]] == LowerCase[(char)*DriveTypeParam])
                    currentDiskIndex = index;
            }
            drive++;
            separateNextDrive |= ((i & Configuration.SeparatedDrives) != 0);
            i <<= 1;
        }
    }

    // we will add disconnected and active FS
    drv.Accessible = FALSE;
    drv.Shared = FALSE;
    drv.DLLName = NULL;
    CDetachedFSList* list = MainWindow->DetachedFSList;
    CPluginFSInterfaceEncapsulation** fsList = (CPluginFSInterfaceEncapsulation**)malloc(sizeof(CPluginFSInterfaceEncapsulation*) * (list->Count + 2));
    if (fsList != NULL)
    {
        CPluginFSInterfaceEncapsulation** fsListItem = fsList;
        CPluginFSInterfaceEncapsulation* activePanelFS = NULL;
        if (FilesWindow->Is(ptPluginFS))
        {
            activePanelFS = FilesWindow->GetPluginFS();
            *fsListItem++ = activePanelFS;
        }
        CFilesWindow* otherPanel = MainWindow->LeftPanel == FilesWindow ? MainWindow->RightPanel : MainWindow->LeftPanel;
        CPluginFSInterfaceEncapsulation* nonactivePanelFS = NULL;
        if (otherPanel->Is(ptPluginFS))
        {
            nonactivePanelFS = otherPanel->GetPluginFS();
            *fsListItem++ = nonactivePanelFS;
        }
        for (i = 0; i < list->Count; i++)
            *fsListItem++ = list->At(i);
        int count = (int)(fsListItem - fsList);
        if (count > 1)
            SortPluginFSTimes(fsList, 0, count - 1);

        if (count > 0)
        {
            int firstFSIndex = Drives->Count;
            for (i = 0; i < count; i++)
            {
                CPluginFSInterfaceEncapsulation* fs = fsList[i];
                char* txt = NULL;
                HICON icon = NULL;
                BOOL destroyIcon = FALSE;
                if (fs->GetChangeDriveOrDisconnectItem(fs->GetPluginFSName(), txt, icon, destroyIcon))
                {
                    drv.DriveText = txt;
                    drv.HIcon = icon;
                    drv.HGrayIcon = NULL;
                    drv.DestroyIcon = destroyIcon;
                    drv.PluginFS = fs->GetInterface();
                    drv.DriveType = (fs == nonactivePanelFS ? drvtPluginFSInOtherPanel : drvtPluginFS);
                    int index = Drives->Add(drv);
                    if (fs == activePanelFS)
                        currentFSIndex = index; // active FS
                }
            }
            // we will check the uniqueness of the text of the items, possibly duplicate items will be indexed
            for (i = firstFSIndex; i < Drives->Count; i++)
            {
                BOOL freeDriveText = FALSE;
                char* driveText = Drives->At(i).DriveText;
                int currentIndex = 1;
                int x;
                for (x = i + 1; x < Drives->Count; x++)
                {
                    char* testedDrvText = Drives->At(x).DriveText;
                    if (StrICmp(driveText, testedDrvText) == 0) // a match -> we have to index the item
                    {
                        if (!freeDriveText) // first match found, we have to index the first occurrence of a duplicate item as well
                        {
                            currentIndex = GetIndexForDrvText(fsList, count, Drives->At(i).PluginFS, currentIndex);
                            Drives->At(i).DriveText = CreateIndexedDrvText(driveText, currentIndex++);
                            if (Drives->At(i).DriveText == NULL)
                                Drives->At(i).DriveText = driveText;
                            else
                                freeDriveText = TRUE;
                        }
                        currentIndex = GetIndexForDrvText(fsList, count, Drives->At(x).PluginFS, currentIndex);
                        Drives->At(x).DriveText = CreateIndexedDrvText(testedDrvText, currentIndex++);
                        if (Drives->At(x).DriveText == NULL)
                            Drives->At(x).DriveText = testedDrvText;
                        else
                            free(testedDrvText);
                    }
                }
                if (freeDriveText)
                    free(driveText);
            }
        }
        free(fsList);
    }
    drv.PluginFS = NULL; // just for sure
    drv.DLLName = NULL;  // just for sure
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);

    // I will add the separator if it is not the first item and if there is no separator yet
    if (Drives->Count > 0 && !IsLastItemSeparator())
        Drives->Add(drvSeparator);

    // adding Documents
    if (Configuration.ChangeDriveShowMyDoc)
    {
        AddToDrives(drv, IDS_MYDOCUMENTS, ';', drvtMyDocuments, getGrayIcons,
                    SalLoadIcon(ImageResDLL, 112, iconSize));
    }

    // adding Cloud Storages (Google Drive, etc.), if I find any...
    CachedCloudStoragesMask = 0;
    if (Configuration.ChangeDriveCloudStorage)
    {
        CSQLite3DynLoadBase* sqlite3_Dyn_InOut = NULL; // I somewhat expected that sqlite3 will be needed for Dropbox, too, which eventually did not happen (so this is here just in case)
        ShellIconOverlays.InitGoogleDrivePath(&sqlite3_Dyn_InOut, TRUE);
        if (ShellIconOverlays.HasGoogleDrivePath())
        {
            CachedCloudStoragesMask |= 0x01 /* Google Drive */;
            AddToDrives(drv, IDS_GOOGLEDRIVE, 0, drvtGoogleDrive, getGrayIcons,
                        SalLoadIcon(HInstance, IDI_GOOGLEDRIVE, iconSize));
        }

        InitDropboxPath();
        if (DropboxPath[0] != 0)
        {
            CachedCloudStoragesMask |= 0x02 /* Dropbox */;
            AddToDrives(drv, IDS_DROPBOX, 0, drvtDropbox, getGrayIcons,
                        SalLoadIcon(HInstance, IDI_DROPBOX, iconSize));
        }

        InitOneDrivePath();
        int c = GetOneDriveStorages();
        if (c == 1 && OneDrivePath[0] != 0)
            CachedCloudStoragesMask |= 0x04 /* only one OneDrive storage - Personal */;
        if (c == 1 && OneDrivePath[0] == 0)
            CachedCloudStoragesMask |= 0x08 /* only one OneDrive storage - Business */;
        if (c > 1)
            CachedCloudStoragesMask |= 0x10 /* more OneDrive storages - drop down menu on drive-bar */;
        char itemText[200 + ONEDRIVE_MAXBUSINESSDISPLAYNAME];
        HICON oneDriveIco = c == 0 ? NULL : SalLoadIcon(HInstance, IDI_ONEDRIVE, iconSize);
        BOOL destroyOneDriveIco = oneDriveIco != NULL;
        if (forDriveBar && c > 1) // data for drive-bar && more storages = let the user choose from the menu (drop down)
        {
            strcpy_s(itemText, LoadStr(IDS_ONEDRIVE));
            AddToDrives(drv, 0, 0, drvtOneDriveMenu, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
            destroyOneDriveIco = FALSE;
        }
        else // data for change drive menu || drive-bar && the only storage (we give a simple button on the drive-bar)
        {
            if (OneDrivePath[0] != 0) // personal
            {
                if (c == 1)
                    strcpy_s(itemText, LoadStr(IDS_ONEDRIVE)); // the only personal storage = we write only: OneDrive
                else
                    sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), LoadStr(IDS_ONEDRIVEPERSONAL));
                AddToDrives(drv, 0, 0, drvtOneDrive, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
                destroyOneDriveIco = FALSE;
            }
            for (int i = 0; i < OneDriveBusinessStorages.Count; i++) // business
            {                                                        // WARNING: for drvtOneDriveBus, DisplayName is later taken from drv.DriveText, when changing the text format, change it !!!
                sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), OneDriveBusinessStorages[i]->DisplayName);
                AddToDrives(drv, 0, 0, drvtOneDriveBus, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
                destroyOneDriveIco = FALSE;
            }
        }
        if (destroyOneDriveIco)
            TRACE_C("CDrivesList::BuildData(): OneDrive icon unused, should never happen");

        if (sqlite3_Dyn_InOut != NULL)
            delete sqlite3_Dyn_InOut; // release sqlite.dll which is no longer needed
    }

    // adding Network Neighborhood
    CPluginData* nethoodPlugin = NULL;
    BOOL existsNethoodPlugin = Plugins.GetFirstNethoodPluginFSName(NULL, &nethoodPlugin);
    if (!SystemPolicies.GetNoNetHood() && Configuration.ChangeDriveShowNet &&
        !existsNethoodPlugin)
    {
        AddToDrives(drv, IDS_NETWORKDRIVE, ',', drvtNeighborhood, getGrayIcons,
                    SalLoadIcon(ImageResDLL, 152, iconSize));
        neighborhoodIndex = Drives->Count - 1;
    }

    // adding FS commands from all plug-ins
    if (!Plugins.AddItemsToChangeDrvMenu(this, currentFSIndex,
                                         FilesWindow->GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                         getGrayIcons))
    {
        return FALSE;
    }

    // finding the index of the Nethood plug-in and setting it to neighborhoodIndex (we represent the Network item with everything)
    if (existsNethoodPlugin && nethoodPlugin != NULL && neighborhoodIndex == -1)
    {
        int i2;
        for (i2 = 0; i2 < Drives->Count; i2++)
        {
            CDriveData* item = &Drives->At(i2);
            if (item->DriveType == drvtPluginCmd && nethoodPlugin->DLLName == item->DLLName)
            {
                neighborhoodIndex = i2;
                break;
            }
        }
    }

    // determining the active item
    FocusIndex = -1;
    if (FilesWindow->Is(ptPluginFS))
    {
        if (currentFSIndex != -1)
            FocusIndex = currentFSIndex;
    }
    else
    {
        if (CurrentPath[0] != 0) // only if we have a path (it's not ptPluginFS)
        {
            if (currentDiskIndex != -1)
                FocusIndex = currentDiskIndex;
            if (FocusIndex == -1 && neighborhoodIndex != -1)
                FocusIndex = neighborhoodIndex;
        }
    }

    // adding Another Panel
    if (Configuration.ChangeDriveShowAnother)
    {
        // a variant with a string 'Another Panel Path' ('As Another Panel'?)
        char* s = LoadStr(IDS_ANOTHERPANEL);
        char* txt = (char*)malloc(2 + strlen(s) + 1);
        if (txt != NULL)
        {
            strcpy(txt, ".\t");
            strcat(txt, s);
            drv.DriveType = drvtOtherPanel;
            drv.DriveText = txt;
            drv.Accessible = TRUE;

            CFilesWindow* panel = MainWindow->GetNonActivePanel();
            if (panel->Is(ptDisk))
            {
                UINT type = MyGetDriveType(panel->GetPath());
                char root2[MAX_PATH];
                GetRootPath(root2, panel->GetPath());
                drv.HIcon = GetDriveIcon(root2, type, TRUE);
                drv.HGrayIcon = NULL;
                drv.DestroyIcon = TRUE; // these icons are allocated
            }
            else
            {
                if (panel->Is(ptZIPArchive))
                {
                    drv.HIcon = SalLoadIcon(ImageResDLL, 174, iconSize);
                    drv.HGrayIcon = NULL;
                    drv.DestroyIcon = TRUE; // this icon is allocated
                }
                else
                {
                    if (panel->Is(ptPluginFS))
                    {
                        BOOL destroyIcon;
                        HICON icon = panel->GetPluginFS()->GetFSIcon(destroyIcon);
                        if (icon != NULL) // defined by a plugin
                        {
                            drv.HIcon = icon;
                            drv.HGrayIcon = NULL;
                            drv.DestroyIcon = destroyIcon; // we are driven by the plugin
                        }
                        else // standard
                        {
                            drv.HIcon = SalLoadIcon(HInstance, IDI_PLUGINFS, iconSize);
                            drv.HGrayIcon = NULL;
                            drv.DestroyIcon = TRUE; // this icon is allocated
                        }
                    }
                }
            }
            drv.Shared = FALSE;
            Drives->Add(drv);
        }
        else
        {
            TRACE_E(LOW_MEMORY);
            return FALSE;
        }
    }

    // loading hot paths
    drv.DestroyIcon = FALSE;
    drv.HIcon = HFavoritIcon;
    drv.HGrayIcon = NULL;
    BOOL addSeparator = (Drives->Count > 0 && !IsLastItemSeparator()); // wed do not want two separators in a row
    for (i = 0; i < HOT_PATHS_COUNT; i++)
    {
        if (MainWindow->HotPaths.GetVisible(i))
        {
            char srcName[MAX_PATH];
            MainWindow->HotPaths.GetName(i, srcName, MAX_PATH);
            if (srcName[0] != 0 && MainWindow->HotPaths.GetPathLen(i) > 0)
            {
                if (addSeparator)
                {
                    // adding separator
                    Drives->Add(drvSeparator);
                    addSeparator = FALSE;
                }
                char text[MAX_PATH + 10];
                if (i < 10)
                    sprintf(text, "%d\t%s", i == 9 ? 0 : i + 1, srcName);
                else
                    sprintf(text, "\t%s", srcName);
                // double '&' so that it is not displayed as an underline
                DuplicateAmpersands(text + 2, MAX_PATH);

                char* alloc = DupStr(text);
                if (alloc != NULL)
                {
                    drv.DriveType = drvtHotPath;
                    drv.Param = i;
                    drv.DriveText = alloc;
                    drv.Accessible = TRUE;
                    drv.Shared = FALSE;
                    Drives->Add(drv);
                    drv.DestroyIcon = FALSE; // I won't clean up other icons anymore
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    return FALSE;
                }
            }
        }
    }

    // we don't want a separator at the end
    if (Drives->Count > 0 && IsLastItemSeparator())
        Drives->Delete(Drives->Count - 1);

    if (drv.DestroyIcon) // no hot path, we have to remove the icon here
    {
        HANDLES(DestroyIcon(drv.HIcon));
    }
    return TRUE;
}

void CDrivesList::DestroyDrives(TDirectArray<CDriveData>* drives)
{
    CALL_STACK_MESSAGE1("CDrivesList::DestroyDrives()");
    int i;
    for (i = 0; i < drives->Count; i++)
    {
        if (drives->At(i).DriveType != drvtSeparator)
        {
            if (drives->At(i).DriveText != NULL)
                free(drives->At(i).DriveText);
            if (drives->At(i).DestroyIcon && drives->At(i).HIcon != NULL)
            {
                HANDLES(DestroyIcon(drives->At(i).HIcon)); // via GetDriveIcon
                drives->At(i).HIcon = NULL;
            }
            if (drives->At(i).DestroyIcon && drives->At(i).HGrayIcon != NULL)
            {
                HANDLES(DestroyIcon(drives->At(i).HGrayIcon));
                drives->At(i).HGrayIcon = NULL;
            }
        }
    }
    drives->DetachMembers();
}

void CDrivesList::DestroyData()
{
    DestroyDrives(Drives);
}

BOOL CDrivesList::LoadMenuFromData()
{
    CALL_STACK_MESSAGE1("CDrivesList::LoadMenuFromData()");
    MENU_ITEM_INFO mii;
    int i;
    for (i = 0; i < Drives->Count; i++)
    {
        CDriveData* item = &Drives->At(i);
        mii.ID = i + 1;
        if (item->DriveType == drvtSeparator)
        {
            mii.Mask = MENU_MASK_TYPE;
            mii.Type = MENU_TYPE_SEPARATOR;
        }
        else
        {
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_ID | MENU_MASK_ICON | MENU_MASK_OVERLAY |
                       MENU_MASK_STATE | MENU_MASK_STRING;
            mii.Type = MENU_TYPE_STRING;
            mii.HIcon = item->HIcon;
            mii.HOverlay = NULL;
            if (item->Shared)
                mii.HOverlay = HSharedOverlays[ICONSIZE_16];
            mii.String = item->DriveText;
            mii.State = 0;
            if (i == FocusIndex) // if FocusIndex==-1, nothing is marked
                mii.State = MENU_STATE_CHECKED;
            if (item->DriveType == drvtPluginFSInOtherPanel)
                mii.State |= MFS_DISABLED | MFS_GRAYED;
        }
        if (!MenuPopup->InsertItem(i, TRUE, &mii))
            return FALSE;
    }
    return TRUE;
}

BOOL CDrivesList::ExecuteItem(int index, HWND hwnd, const RECT* exclude, BOOL* fromDropDown)
{
    if (fromDropDown != NULL)
        *fromDropDown = FALSE;
    if (index < 0 || index >= Drives->Count)
    {
        TRACE_E("index=" << index);
        return FALSE;
    }
    BOOL ret = TRUE;
    CDriveData* item = &Drives->At(index);
    // transfer outside
    CDriveTypeEnum dt = item->DriveType;
    *DriveType = dt;
    switch (dt)
    {
    case drvtOneDriveBus:
    {
        // WARNING: for drvtOneDriveBus, DisplayName is taken from drv.DriveText here, when changing the text format, change it !!!
        const char* s = item->DriveText;
        const char* end = s + strlen(s);
        if (*s != '\t' && *s != 0)
            s++; // hot key
        if (*s == '\t')
            s++;
        s += strlen(LoadStr(IDS_ONEDRIVE)) + 3 /* " - " */;
        if (s <= end)
            *DriveTypeParam = (DWORD_PTR)DupStr(s); // DisplayName
        else
            TRACE_C("CDrivesList::ExecuteItem(): Unexpected format of drv.DriveText");
        break;
    }

    case drvtOneDriveMenu: // opening the drop down menu for OneDrive storage selection
    {
        if (fromDropDown != NULL)
            *fromDropDown = TRUE;
        InitOneDrivePath();
        int c = GetOneDriveStorages();
        if (c <= 1)
        { // OneDrive is not in drop down anymore, we will refresh both Drive bars, so that the icons disappear or update
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
            ret = FALSE;
        }
        else
        {
            CMenuPopup menu;
            MENU_ITEM_INFO mii;
            mii.Mask = MENU_MASK_TYPE | MENU_MASK_STRING | MENU_MASK_ID;
            mii.Type = MENU_TYPE_STRING;

            char itemText[200 + ONEDRIVE_MAXBUSINESSDISPLAYNAME];
            if (OneDrivePath[0] != 0) // personal
            {
                sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), LoadStr(IDS_ONEDRIVEPERSONAL));
                mii.String = itemText;
                mii.ID = 1;
                menu.InsertItem(-1, TRUE, &mii);
            }
            for (int i = 0; i < OneDriveBusinessStorages.Count; i++) // business
            {
                sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), OneDriveBusinessStorages[i]->DisplayName);
                mii.String = itemText;
                mii.ID = i + 2;
                menu.InsertItem(-1, TRUE, &mii);
            }

            int cmd = menu.Track(MENU_TRACK_RETURNCMD, exclude->left, exclude->bottom, hwnd, exclude);
            if (cmd > 0)
            {
                if (cmd == 1)
                    *DriveType = drvtOneDrive;
                else
                {
                    mii.Mask = MENU_MASK_STRING;
                    mii.String = itemText;
                    mii.StringLen = _countof(itemText);
                    if (menu.GetItemInfo(cmd, FALSE, &mii))
                    {
                        const char* s = mii.String;
                        const char* end = s + strlen(s);
                        s += strlen(LoadStr(IDS_ONEDRIVE)) + 3 /* " - " */;
                        if (s <= end)
                        {
                            *DriveType = drvtOneDriveBus;
                            *DriveTypeParam = (DWORD_PTR)DupStr(s); // DisplayName
                        }
                        else
                            TRACE_C("CDrivesList::ExecuteItem(): Unexpected format of mii.String");
                    }
                    else
                        ret = FALSE;
                }
            }
            else
                ret = FALSE;
        }
        break;
    }

    case drvtHotPath:
    {
        *DriveTypeParam = item->Param;
        break;
    }

    case drvtRemovable:
    case drvtFixed:
    case drvtRemote:
    case drvtCDROM:
    case drvtRAMDisk:
    {
        *DriveTypeParam = item->DriveText[0];

        // try to revive
        if (!item->Accessible)
        {
            char name[3] = " :";
            char remoteName[MAX_PATH];
            strcpy(remoteName, item->DriveText + 2);
            name[0] = item->DriveText[0];

            if (!RestoreNetworkConnection(FilesWindow->HWindow, name, remoteName))
                ret = FALSE;
        }

        break;
    }

    case drvtPluginFS:             // a plug-in item: opened FS (active/disconnected)
    case drvtPluginFSInOtherPanel: // this should never come, we will stop it later (no action)
    {
        *DriveTypeParam = (DWORD_PTR)item->PluginFS;
        break;
    }

    case drvtPluginCmd: // a plug-in item: FS command
    {
        *DriveTypeParam = (DWORD_PTR)item->DLLName;
        break;
    }
    }
    return ret;
}

BOOL CDrivesList::Track()
{
    CALL_STACK_MESSAGE1("CDrivesList::Track()");
    RECT r;
    GetWindowRect(FilesWindow->HWindow, &r);
    int dirHeight = MainWindow->GetDirectoryLineHeight();

    RECT buttonRect;
    buttonRect = r;
    buttonRect.bottom = buttonRect.top + dirHeight;
    buttonRect.right = buttonRect.left + dirHeight;

    MenuPopup = new CMenuPopup;
    if (MenuPopup == NULL)
    {
        TRACE_E(LOW_MEMORY);
        return FALSE;
    }
    MenuPopup->SetStyle(MENU_POPUP_THREECOLUMNS);
    if (!BuildData(FALSE))
    {
        delete MenuPopup;
        MenuPopup = NULL;
        return FALSE;
    }

    // synchronize drive bars, if needed
    MainWindow->RebuildDriveBarsIfNeeded(TRUE, GetCachedDrivesMask(), // to speed up, we will use our cached masks
                                         TRUE, GetCachedCloudStoragesMask());

    if (!LoadMenuFromData())
    {
        DestroyData();
        delete MenuPopup;
        MenuPopup = NULL;
        return FALSE;
    }

    if (FocusIndex != -1)
        MenuPopup->SetSelectedItemIndex(FocusIndex);
    FilesWindow->OpenedDrivesList = this; // so that FilesWindow deliver us notifications
    DWORD cmd = MenuPopup->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_SELECT | MENU_TRACK_VERTICAL,
                                 buttonRect.left, buttonRect.bottom,
                                 FilesWindow->HWindow, &buttonRect);
    FilesWindow->OpenedDrivesList = NULL;

    // let the returning variable be set
    if (cmd != 0)
        if (!ExecuteItem(cmd - 1, NULL, NULL, NULL))
            cmd = 0;

    DestroyData();
    delete MenuPopup;
    MenuPopup = NULL;

    // synchronize drive bars once more (it could have changed during the menu)
    MainWindow->RebuildDriveBarsIfNeeded(TRUE, GetCachedDrivesMask(), // to speed up, we will use our cached masks
                                         TRUE, GetCachedCloudStoragesMask());

    return cmd != 0;
}

BOOL IncludeDriveInDriveBar(CDriveTypeEnum dt)
{
    switch (dt)
    {
    case drvtUnknow:
    case drvtRemovable:
    case drvtFixed:
    case drvtRemote:
    case drvtCDROM:
    case drvtRAMDisk:
    case drvtMyDocuments:
    case drvtGoogleDrive:
    case drvtDropbox:
    case drvtOneDrive:
    case drvtOneDriveBus:
    case drvtOneDriveMenu:
    case drvtNeighborhood:
    case drvtPluginCmd:
        return TRUE;
    }
    return FALSE; // we don't want fs, ...
}

BOOL CDrivesList::FillDriveBar(CDriveBar* driveBar, BOOL bar2)
{
    driveBar->DestroyImageLists();
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);
    driveBar->HDrivesIcons = ImageList_Create(iconSize, iconSize, GetImageListColorFlags() | ILC_MASK, 0, 1);
    driveBar->HDrivesIconsGray = ImageList_Create(iconSize, iconSize, GetImageListColorFlags() | ILC_MASK, 0, 1);

    BOOL insertSeparator = FALSE;
    int imageIndex = 0;
    int i;
    for (i = 0; i < Drives->Count; i++)
    {
        CDriveData* item = &Drives->At(i);
        if (!IncludeDriveInDriveBar(item->DriveType))
        {
            if (item->DriveType == drvtSeparator)
                insertSeparator = TRUE;
            continue;
        }

        if (insertSeparator)
        {
            TLBI_ITEM_INFO2 tii;
            tii.Mask = TLBI_MASK_STYLE;
            tii.Style = TLBI_STYLE_SEPARATOR;
            driveBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
            insertSeparator = FALSE;
        }

        char buff[80];
        TLBI_ITEM_INFO2 tii;
        tii.Mask = TLBI_MASK_STYLE | TLBI_MASK_IMAGEINDEX | TLBI_MASK_OVERLAY | TLBI_MASK_ID;
        tii.Style = item->DriveType == drvtOneDriveMenu ? TLBI_STYLE_WHOLEDROPDOWN | TLBI_STYLE_DROPDOWN : TLBI_STYLE_NOPREFIX;
        if (item->DriveType != drvtMyDocuments && item->DriveType != drvtNeighborhood && item->DriveType != drvtPluginCmd &&
            item->DriveType != drvtGoogleDrive && item->DriveType != drvtDropbox && item->DriveType != drvtOneDrive &&
            item->DriveType != drvtOneDriveBus && item->DriveType != drvtOneDriveMenu)
        {
            tii.Mask |= TLBI_MASK_TEXT;
            tii.Style |= TLBI_STYLE_SHOWTEXT;
            buff[0] = item->DriveText[0];
            buff[1] = 0;
            tii.Text = buff;
        }
        ImageList_AddIcon(driveBar->HDrivesIcons, item->HIcon);
        ImageList_AddIcon(driveBar->HDrivesIconsGray, item->HGrayIcon == NULL ? item->HIcon : item->HGrayIcon);
        tii.ImageIndex = imageIndex++;
        tii.HOverlay = NULL;
        if (item->Shared)
            tii.HOverlay = HSharedOverlays[ICONSIZE_16];
        tii.ID = (bar2 ? CM_DRIVEBAR2_MIN : CM_DRIVEBAR_MIN) + i;
        driveBar->InsertItem2(0xFFFFFFFF, TRUE, &tii);
    }

    driveBar->SetImageList(driveBar->HDrivesIconsGray);
    driveBar->SetHotImageList(driveBar->HDrivesIcons);

    return TRUE;
}

BOOL CDrivesList::GetDriveBarToolTip(int index, char* text)
{
    if (index < 0 || index >= Drives->Count)
    {
        TRACE_E("index=" << index);
        return FALSE;
    }
    BOOL ret = TRUE;

    text[0] = 0;

    char volumeName[MAX_PATH + 50];
    CQuadWord freeSpace;
    char freeSpaceText[100];
    char root[10] = " :\\";

    CDriveData* item = &Drives->At(index);
    switch (item->DriveType)
    {
    case drvtRemovable: // diskettes, we will find out if it is 3.5", 5.25", 8" or unknown
    {
        root[0] = item->DriveText[0];
        volumeName[0] = 0;
        int drv = item->DriveText[0] - 'A' + 1;
        if (drv >= 1 && drv <= 26) // we will do "range-check" for sure
        {
            DWORD medium = GetDriveFormFactor(drv);
            switch (medium)
            {
            case 350:
                strcpy(volumeName, LoadStr(IDS_FLOPPY350));
                break;
            case 525:
                strcpy(volumeName, LoadStr(IDS_FLOPPY525));
                break;
            case 800:
                strcpy(volumeName, LoadStr(IDS_FLOPPY800));
                break;
            default:
            {
                GetDisplayNameFromSystem(root, volumeName, MAX_PATH);
                if (volumeName[0] == 0)
                    strcpy(volumeName, LoadStr(IDS_REMOVABLE_DISK));

                break;
            }
            }
        }
        strcpy(text, volumeName);
        break;
    }

    case drvtFixed:
    case drvtRAMDisk:
    {
        root[0] = item->DriveText[0];
        DWORD dummy, flags;
        if (GetVolumeInformation(root, volumeName, MAX_PATH, NULL, &dummy, &flags, NULL, 0))
        {
            CQuadWord t;                              // total disk space
            freeSpace = MyGetDiskFreeSpace(root, &t); // free disk space
            PrintDiskSize(freeSpaceText, freeSpace, 0);
            if (volumeName[0] == 0)
                strcpy(volumeName, LoadStr(IDS_LOCAL_DISK));
            sprintf(text, "%s (%s)", volumeName, freeSpaceText);
        }
        break;
    }

    case drvtCDROM:
    {
        root[0] = item->DriveText[0];
        HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
        UINT_PTR uid = ++ReadCDVolNameReqUID;
        lstrcpyn(ReadCDVolNameBuffer, root, MAX_PATH);
        HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));

        // create thread, in which we will find out the volume_name of the CD drive
        DWORD threadID;
        HANDLE thread = HANDLES(CreateThread(NULL, 0, ReadCDVolNameThreadF,
                                             (void*)uid, 0, &threadID));
        if (thread != NULL && WaitForSingleObject(thread, 500) == WAIT_OBJECT_0)
        { // give it 500ms to find out the volume-name
            HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
            lstrcpyn(volumeName, ReadCDVolNameBuffer, MAX_PATH + 50);
            HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));
        }
        else
            volumeName[0] = 0;
        if (thread != NULL)
            AddAuxThread(thread, TRUE); // if the thread is still running, we will kill it before closing the program
        if (volumeName[0] == 0)
            strcpy(volumeName, LoadStr(IDS_COMPACT_DISK));

        strcpy(text, volumeName);
        break;
    }

    case drvtRemote:
    {
        if (strlen(item->DriveText) > 2)
        {
            strcpy(text, item->DriveText + 2);
            RemoveAmpersands(text);
        }
        break;
    }

    case drvtMyDocuments:
        strcpy(text, LoadStr(IDS_MYDOCUMENTS));
        break;
    case drvtGoogleDrive:
        strcpy(text, LoadStr(IDS_GOOGLEDRIVE));
        break;
    case drvtDropbox:
        strcpy(text, LoadStr(IDS_DROPBOX));
        break;
    case drvtOneDrive:
    case drvtOneDriveMenu:
        strcpy(text, LoadStr(IDS_ONEDRIVE));
        break;
    case drvtNeighborhood:
        strcpy(text, LoadStr(IDS_NETWORKDRIVE));
        break;

    case drvtOneDriveBus:
    {
        const char* s = item->DriveText;
        if (*s != '\t' && *s != 0)
            s++; // hot key
        if (*s == '\t')
            s++;
        lstrcpyn(text, s, TOOLTIP_TEXT_MAX);
        break;
    }

    case drvtPluginCmd:
    {
        // trim the first column in the item name
        const char* p = item->DriveText;
        while (*p != 0 && *p != '\t')
            p++;
        if (*p == '\t')
        {
            const char* e = p + 1; // trim the potential third column in the item name (can be after the second TAB)
            while (*e != 0 && *e != '\t')
                e++;
            lstrcpyn(text, p + 1, (int)(e - (p + 1) + 1));
        }
        break;
    }
    }
    return TRUE;
}

BOOL CDrivesList::OnContextMenu(BOOL posByMouse, int itemIndex, int panel, const char** pluginFSDLLName)
{
    CALL_STACK_MESSAGE4("CDrivesList::DisplayMenu(%d, %d, %d)", posByMouse, itemIndex, panel);

    if (pluginFSDLLName != NULL)
        *pluginFSDLLName = NULL;

    // for the MenuPopup variable, access is only allowed if posByMouse == FALSE

    int selectedIndex;
    if (itemIndex == -1 || !posByMouse)
    {
        if (MenuPopup == NULL)
        {
            TRACE_E("CDrivesList::OnContextMenu(): Incorrect call.");
            return FALSE;
        }
        selectedIndex = MenuPopup->GetSelectedItemIndex();
        if (selectedIndex == -1)
        {
            TRACE_E("selectedIndex == -1");
            return FALSE;
        }
    }
    else
        selectedIndex = itemIndex;

    // finding a selected item
    RECT selectedIndexRect = {0};
    if (MenuPopup != NULL)
        MenuPopup->GetItemRect(selectedIndex, &selectedIndexRect);
    char path[2 * MAX_PATH];
    CDriveTypeEnum dt = Drives->At(selectedIndex).DriveType;
    switch (dt)
    {
    case drvtUnknow:
    case drvtRemovable:
    case drvtFixed:
    case drvtRemote:
    case drvtCDROM:
    case drvtRAMDisk:
    {
        strcpy(path, " :\\");
        path[0] = Drives->At(selectedIndex).DriveText[0];
        break;
    }

    case drvtHotPath:
    {
        if (!MainWindow->GetExpandedHotPath(MainWindow->HWindow, Drives->At(selectedIndex).Param, path, 2 * MAX_PATH))
            return FALSE;
        if (LowerCase[path[0]] >= 'a' && LowerCase[path[0]] <= 'z' && path[1] == ':' && (path[2] == '\\' || path[2] == '/') ||
            (path[0] == '\\' || path[0] == '/') && (path[1] == '\\' || path[1] == '/'))
        { // absolute path on disk or network (UNC)
            SlashesToBackslashesAndRemoveDups(path);
            int type;
            BOOL isDir;
            char* secondPart;
            if (!SalParsePath(MainWindow->HWindow, path, type, isDir, secondPart, LoadStr(IDS_ERRORTITLE),
                              NULL, FALSE, NULL, NULL, NULL, 2 * MAX_PATH) ||
                type != PATH_TYPE_WINDOWS || // not a windows path
                !isDir || *secondPart != 0)  // the path to a file (not a directory) or a part of the path does not exist
            {
                return FALSE; // we can do the context menu only for windows paths (not for archives)
            }
        }
        else
            return FALSE; // we can't do the context menu for other types of paths (relative, FS plugin)
        break;
    }

    case drvtPluginFS:
    case drvtPluginCmd:
    {
        CPluginFSInterfaceAbstract* pluginFS = NULL;
        const char* pluginFSName = NULL;
        int pluginFSNameIndex = -1;
        BOOL isDetachedFS = FALSE;
        BOOL refreshMenu;
        BOOL closeMenu;
        int postCmd;
        void* postCmdParam;

        CPluginData* pluginData = NULL;

        if (dt == drvtPluginFS)
        {
            CPluginFSInterfaceAbstract* fs = Drives->At(selectedIndex).PluginFS;
            // we need to verify, that 'fs' is still a valid interface
            if (FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->GetInterface() == fs)
            { // active FS selection - we will do refresh
                pluginData = Plugins.GetPluginData(FilesWindow->GetPluginFS()->GetPluginInterfaceForFS()->GetInterface());
                pluginFS = fs;
                pluginFSName = FilesWindow->GetPluginFS()->GetPluginFSName();
                pluginFSNameIndex = FilesWindow->GetPluginFS()->GetPluginFSNameIndex();
            }
            else
            {
                CDetachedFSList* list = MainWindow->DetachedFSList;
                int i;
                for (i = 0; i < list->Count; i++)
                {
                    if (list->At(i)->GetInterface() == fs)
                    { // disconnected FS selection
                        pluginData = Plugins.GetPluginData(list->At(i)->GetPluginInterfaceForFS()->GetInterface());
                        pluginFS = fs;
                        pluginFSName = list->At(i)->GetPluginFSName();
                        pluginFSNameIndex = list->At(i)->GetPluginFSNameIndex();
                        isDetachedFS = TRUE;
                        break;
                    }
                }
            }
        }
        else // drvtPluginCmd
        {
            pluginData = Plugins.GetPluginData(Drives->At(selectedIndex).DLLName);
            if (pluginFSDLLName != NULL)
                *pluginFSDLLName = Drives->At(selectedIndex).DLLName;
        }

        if (pluginData != NULL)
        {
            POINT p;
            if (posByMouse)
            {
                DWORD pos = GetMessagePos();
                p.x = GET_X_LPARAM(pos);
                p.y = GET_Y_LPARAM(pos);
            }
            else
            {
                p.x = selectedIndexRect.left;
                p.y = selectedIndexRect.bottom;
            }

            char pluginFSNameBuf[MAX_PATH]; // 'pluginFS' may cease to exist, so we put 'fsName' into a local buffer
            if (pluginFSName != NULL)
                lstrcpyn(pluginFSNameBuf, pluginFSName, MAX_PATH);
            if (pluginData->ChangeDriveMenuItemContextMenu(MainWindow->HWindow, panel, p.x, p.y, pluginFS,
                                                           pluginFSName != NULL ? pluginFSNameBuf : NULL,
                                                           pluginFSName != NULL ? pluginFSNameIndex : -1,
                                                           isDetachedFS, refreshMenu,
                                                           closeMenu, postCmd, postCmdParam))
            {
                if (closeMenu)
                {
                    BOOL failed = FALSE;
                    if (MenuPopup != NULL) // item list of change drive menu refresh can occur during the opened context menu, so the focus can change to another item than the one for which we opened the menu and thus the post-command would be sent to another place than we need
                    {
                        selectedIndex = MenuPopup->GetSelectedItemIndex();
                        if (!(selectedIndex >= 0 && selectedIndex < Drives->Count &&
                              (dt == drvtPluginCmd && Drives->At(selectedIndex).DLLName == pluginData->DLLName ||
                               dt == drvtPluginFS && Drives->At(selectedIndex).PluginFS == pluginFS)))
                        {
                            int i;
                            for (i = 0; i < Drives->Count; i++)
                            {
                                if (dt == drvtPluginCmd && Drives->At(i).DLLName == pluginData->DLLName ||
                                    dt == drvtPluginFS && Drives->At(i).PluginFS == pluginFS)
                                {
                                    MenuPopup->SetSelectedItemIndex(i);
                                    break;
                                }
                            }
                            if (i == Drives->Count)
                                failed = TRUE;
                        }
                    }
                    if (!failed)
                    {
                        if (postCmd != 0) // closing Change Drive menu + executing postCmd
                        {
                            *PostCmd = postCmd;
                            *PostCmdParam = postCmdParam;
                            *FromContextMenu = TRUE;
                            return TRUE;
                        }
                        else // just closing Change Drive menu
                        {
                            *PostCmd = 0; // not needed (set from constructor), just for clarity
                            *FromContextMenu = TRUE;
                            return TRUE;
                        }
                    }
                }
                if (refreshMenu) // plug-in asks for menu refresh
                {
                    PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
                }
            }
        }
        return FALSE;
    }

    case drvtPluginFSInOtherPanel: // no menu will be opened (menu item disabled)
    {
        return FALSE;
    }

    case drvtNeighborhood:
    case drvtOtherPanel:
    case drvtMyDocuments:
    case drvtGoogleDrive:
    case drvtDropbox:
    case drvtOneDrive:
    case drvtOneDriveBus:
    case drvtOneDriveMenu:
    {
        // for Documents, Network, As Other Panel a Cloud Storages we can't do context menu)
        return FALSE;
    }

    default:
    {
        TRACE_E("Unhandled DriveType = " << dt);
        return FALSE;
    }
    }

    // commented out so that we can disconnect a network drive that is not accessible at the moment (longer waiting is tolerated)
    //  if (Drives->At(selectedIndex).DriveType == drvtRemote &&
    //      Drives->At(selectedIndex).Accessible &&   // we will allow to disconnect unaccessible network drives, we verify the others
    //      MainWindow->GetActivePanel()->CheckPath(TRUE, path) != ERROR_SUCCESS) return FALSE;

    //  MainWindow->ReleaseMenuNew();  // windows weren't built for more context menus

    BOOL selectedIndexAccessible = Drives->At(selectedIndex).Accessible;

    if (MainWindow->ContextMenuChngDrv != NULL)
    {
        TRACE_E("ContextMenuChngDrv already exist! Releasing...");
        MainWindow->ContextMenuChngDrv->Release();
        MainWindow->ContextMenuChngDrv = NULL;
    }
    MainWindow->ContextMenuChngDrv = CreateIContextMenu2(MainWindow->HWindow, path);
    HMENU h = CreatePopupMenu();
    if (MainWindow->ContextMenuChngDrv != NULL && h != NULL)
    {
        DisplayMenuAux2(MainWindow->ContextMenuChngDrv, h);
        RemoveUselessSeparatorsFromMenu(h);

        POINT pt;
        if (posByMouse)
        {
            DWORD pos = GetMessagePos();
            pt.x = GET_X_LPARAM(pos);
            pt.y = GET_Y_LPARAM(pos);
        }
        else
        {
            pt.x = selectedIndexRect.left;
            pt.y = selectedIndexRect.bottom;
        }
        CMenuPopup contextPopup;
        contextPopup.SetTemplateMenu(h);
        DWORD cmd = contextPopup.Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON,
                                       pt.x, pt.y, MainWindow->HWindow, NULL);

        // WARNING: during the expanded context menu, the object can be refreshed (e.g. user inserts
        // a USB stick or disconnects a network drive), we have to count on it when accessing the
        // data of this object (e.g. Drives can contain different data)

        if (cmd != 0)
        {
            BOOL releaseLeft = FALSE;  // disconnect left panel from disk?
            BOOL releaseRight = FALSE; // disconnect right panel from disk?

            char cmdName[2000]; // we have 2000 instead of 200 on purpose, shell extensions sometimes write double (consideration: unicode = 2 * "number of characters"), etc.
            if (AuxGetCommandString(MainWindow->ContextMenuChngDrv, cmd, GCS_VERB, NULL, cmdName, 200) != NOERROR)
            {
                cmdName[0] = 0;
            }
            if (stricmp(cmdName, "properties") != 0 && // not mandatory for properties
                stricmp(cmdName, "find") != 0 &&       // not mandatory for find
                stricmp(cmdName, "open") != 0 &&       // not mandatory for open
                stricmp(cmdName, "explore") != 0 &&    // not mandatory for explore
                stricmp(cmdName, "link") != 0)         // not mandatory for create-short-cut
            {
                CFilesWindow* win;
                int i;
                for (i = 0; i < 2; i++)
                {
                    win = i == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
                    if (HasTheSameRootPath(win->GetPath(), path)) // identical disk (both normal and UNC)
                    {
                        if (i == 0)
                            releaseLeft = TRUE;
                        else
                            releaseRight = TRUE;
                    }
                }
            }

            SetCurrentDirectoryToSystem();
            DWORD disks = GetLogicalDrives();

            CShellExecuteWnd shellExecuteWnd;
            CMINVOKECOMMANDINFOEX ici;
            ZeroMemory(&ici, sizeof(CMINVOKECOMMANDINFOEX));
            ici.cbSize = sizeof(CMINVOKECOMMANDINFOEX);
            ici.fMask = CMIC_MASK_PTINVOKE;
            ici.hwnd = shellExecuteWnd.Create(MainWindow->HWindow, "SEW: CDrivesList::OnContextMenu cmd=%d cmdName=%s", cmd, cmdName);
            ici.lpVerb = MAKEINTRESOURCE(cmd);
            ici.lpDirectory = path;
            ici.nShow = SW_SHOWNORMAL;
            if (MenuPopup != NULL)
            {
                ici.ptInvoke.x = selectedIndexRect.left;
                ici.ptInvoke.y = selectedIndexRect.bottom;
            }
            else
            {
                ici.ptInvoke.x = pt.x;
                ici.ptInvoke.y = pt.y;
            }

            BOOL changeToFixedDrv = cmd == 35; // "format" is not modal, we need to change to fixed drive
            if (releaseLeft)
            {
                if (changeToFixedDrv)
                {
                    MainWindow->LeftPanel->ChangeToFixedDrive(MainWindow->LeftPanel->HWindow);
                    // WARNING: we have to invalidate the window, so that the cached bitmap of Alt+F1/2 menu is broken
                    // otherwise, the old part of the panel was displayed in this situation:
                    // there is disk S: in the left panel; Alt+F1, right click on S, Format
                    InvalidateRect(MainWindow->LeftPanel->HWindow, NULL, TRUE);
                }
                else
                    MainWindow->LeftPanel->HandsOff(TRUE);
            }
            if (releaseRight)
            {
                if (changeToFixedDrv)
                {
                    MainWindow->RightPanel->ChangeToFixedDrive(MainWindow->RightPanel->HWindow);
                    InvalidateRect(MainWindow->RightPanel->HWindow, NULL, TRUE);
                }
                else
                    MainWindow->RightPanel->HandsOff(TRUE);
            }

            DisplayMenuAux(MainWindow->ContextMenuChngDrv, (CMINVOKECOMMANDINFO*)&ici);

            // it's possible to change clipboard from context menu, we will check it ...
            IdleRefreshStates = TRUE;  // we will force the check of the state variables at the next Idle
            IdleCheckClipboard = TRUE; // we will the clipboard to be checked as well

            UpdateWindow(MainWindow->HWindow);
            if (releaseLeft && !changeToFixedDrv)
                MainWindow->LeftPanel->HandsOff(FALSE);
            if (releaseRight && !changeToFixedDrv)
                MainWindow->RightPanel->HandsOff(FALSE);

            if (!selectedIndexAccessible) // unmap the remembered inactive network connection?
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            /*
// the notification will be delivered through WM_DEVICECHANGE
      if (GetLogicalDrives() < disks) // unmapping?
      {
        PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
        TRACE_I("sending 1");
      }
*/
        }
    }
    if (MainWindow->ContextMenuChngDrv != NULL)
    {
        MainWindow->ContextMenuChngDrv->Release();
        MainWindow->ContextMenuChngDrv = NULL;
    }
    if (h != NULL)
        DestroyMenu(h);

    return FALSE;
}

BOOL CDrivesList::RebuildMenu()
{
    CALL_STACK_MESSAGE1("CDrivesList::RebuildMenu()");
    if (MenuPopup == NULL)
    {
        TRACE_E("MenuPopup == NULL");
        return FALSE;
    }
    // asking menu for modification
    if (MenuPopup->BeginModifyMode())
    {
        // remove old data
        DestroyData();
        // get new ones
        BuildData(TRUE); // timeout can't be used, reading CD labels takes long time (1,5s) - system notifies the driver earlier than it's loaded
        // remove items from menu
        MenuPopup->RemoveAllItems();
        // get new items
        LoadMenuFromData();
        // let menu to be redrawn
        MenuPopup->EndModifyMode();
    }
    return TRUE;
}

BOOL CDrivesList::FindPanelPathIndex(CFilesWindow* panel, DWORD* index)
{
    if (panel->Is(ptPluginFS))
    {
        if (!panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM))
        { // searching for a plug-in FS without its own item in Change Drive menu (these items are not in Drive bars)
            int i;
            for (i = 0; i < Drives->Count; i++)
            {
                CDriveData* item = &Drives->At(i);
                if (item->DriveType == drvtPluginCmd && panel->GetPluginFS()->GetDLLName() == item->DLLName)
                {
                    *index = i;
                    return TRUE;
                }
            }
        }
    }
    else
    {
        const char* path = panel->GetPath();
        if (path[0] == '\\' && path[1] == '\\')
        {
            if (path[2] == '.' && path[3] == '\\' && path[4] != 0 && path[5] == ':')
                return FALSE; // "\\.\C:\" type path
            CPluginData* nethoodPlugin = NULL;
            Plugins.GetFirstNethoodPluginFSName(NULL, &nethoodPlugin);
            int i;
            for (i = 0; i < Drives->Count; i++)
            {
                CDriveData* item = &Drives->At(i);
                if (nethoodPlugin != NULL)
                {
                    if (item->DriveType == drvtPluginCmd && nethoodPlugin->DLLName == item->DLLName)
                    {
                        *index = i;
                        return TRUE;
                    }
                }
                else
                {
                    if (item->DriveType == drvtNeighborhood)
                    {
                        *index = i;
                        return TRUE;
                    }
                }
            }
        }
        else
        {
            int i;
            for (i = 0; i < Drives->Count; i++)
            {
                CDriveData* item = &Drives->At(i);

                switch (item->DriveType)
                {
                case drvtUnknow:
                case drvtRemovable:
                case drvtFixed:
                case drvtRemote:
                case drvtCDROM:
                case drvtRAMDisk:
                {
                    if (LowerCase[path[0]] == LowerCase[item->DriveText[0]])
                    {
                        *index = i;
                        return TRUE;
                    }
                    break;
                }
                }
            }
        }
    }
    return FALSE;
}

BOOL CDrivesList::IsLastItemSeparator()
{
    if (Drives->Count < 1)
        return FALSE;
    CDriveData* item = &Drives->At(Drives->Count - 1);
    return item->DriveType == drvtSeparator;
}

DWORD
CDrivesList::GetCachedDrivesMask()
{
    return CachedDrivesMask;
}

DWORD
CDrivesList::GetCachedCloudStoragesMask()
{
    return CachedCloudStoragesMask;
}
