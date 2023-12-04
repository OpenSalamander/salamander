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
    netDrives = 0; // bitove pole network disku

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
                for (i = 0; i < (int)e; i++) // zpracujem nove data
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
        return FALSE; // tak nic ...

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
            ret = TRUE; // ohlasime uspech, i kdyz uzivatelske jmeno neni definovane (ma se pouzit aktualni user)

            keyNameSize = userBufSize;
            type = REG_SZ;
            res = SalRegQueryValueEx(driveKey, "UserName", 0, &type,
                                     (unsigned char*)userName, &keyNameSize);
            if (res == ERROR_SUCCESS && type == REG_SZ && userName[0] != 0) // mame usera ?
                TRACE_I("Found user name: " << keyName << ", " << userName);
            else
                userName[0] = 0;

            keyNameSize = providerBufSize;
            type = REG_SZ;
            res = SalRegQueryValueEx(driveKey, "ProviderName", 0, &type,
                                     (unsigned char*)providerBuf, &keyNameSize);
            if (res != ERROR_SUCCESS || type != REG_SZ) // pokud nemame providera, vynulujeme buffer
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
    data->err = WNetAddConnection3(NULL /* nepouzivame CONNECT_INTERACTIVE, jsme v jinem threadu */,
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

    err = 0; // pro jistotu
    if (errProviderCode != NULL)
        *errProviderCode = 0;
    if (errBuf != NULL)
        errBuf[0] = 0;
    if (errProviderName != NULL)
        errProviderName[0] = 0;

    // nejdrive pockame na dokonceni predchoziho "vypoctu"
    GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
    if (NBWNetAC3Thread.ShutdownArrived)
        return FALSE; // soft uz je ukonceny, zadna dalsi akce
    if (NBWNetAC3Thread.Thread != NULL)
    {
        while (1)
        {
            DWORD res = WaitForSingleObject(NBWNetAC3Thread.Thread, 200);
            if (res == WAIT_FAILED)
                return FALSE; // invalidni handle threadu (zavreno zvenci pri ukoncovani softu)
            if (res != WAIT_TIMEOUT)
                break;
            if (UserWantsToCancelSafeWaitWindow())
                return FALSE;
        }
        if (NBWNetAC3Thread.ShutdownArrived)
            return FALSE; // soft uz je ukonceny, zadna dalsi akce
        NBWNetAC3Thread.Close();
    }

    // pak nastavime parametry a zkusime spustit novy thread
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
        return FALSE; // chyba (simulace ESC)
    }

    // pockame na dokonceni "vypoctu", abychom mohli vratit vysledek
    while (1)
    {
        DWORD res = WaitForSingleObject(NBWNetAC3Thread.Thread, 200);
        if (res == WAIT_FAILED)
            return FALSE; // invalidni handle threadu (zavreno zvenci pri ukoncovani softu)
        if (res != WAIT_TIMEOUT)
            break;
        if (UserWantsToCancelSafeWaitWindow())
            return FALSE;
    }
    if (NBWNetAC3Thread.ShutdownArrived)
        return FALSE; // soft uz je ukonceny, zadna dalsi akce
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
    return err == ERROR_ACCESS_DENIED || // obrana proti napr. "no files found"
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
           err == ERROR_EXTENDED_ERROR; // cert vi co je to za extended chybu, ale pada sem "password expired", takze to obecne povazujeme za logon-failure (maximalne da user Cancel)
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
        //    case '_':     // pri tomto znaku se objevi jen warning
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
            ;             // preskok jmena serveru + jednoduchy test (neuplny, napr. zakazuje tecky i v IPv4 a IPv6), jestli skutecne nejde o invalid name (v tom pripade se nebudeme ptat na jmeno+heslo)
        if (*root == '.') // osetrime jeste IPv4 adresy (na IPv6 kasleme)
        {
            const char* r = root;
            while (*++r != 0 && *r != '\\')
                ;
            if (r - server < 50)
            {
                char ip[51];
                lstrcpyn(ip, server, (int)((r - server) + 1));
                if (inet_addr(ip) != INADDR_NONE)
                    root = r; // jde o IP string (aa.bb.cc.dd)
            }
        }
        if (*root == '\\')
        {
            while (*++root != 0 && *root != '\\')
                ;
            if (*(root - 1) == '$')
                return TRUE; // admin share (\\server\share$ nebo \\server\share$\...)
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
        if (serverName[0] == 0) // nejde o variantu \\server ani \\server\, vyresime to volanim puvodniho kodu
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
                if (last == NULL || *(last + 1) == 0) // vlastni dialog pod XP+Vista ukazeme jen pro proste UNC cesty: "\\\\server\share" a "\\\\server\\share\\"
                    lstrcpyn(serverName, remoteName + 2, (int)min(MAX_PATH, (end - (remoteName + 2)) + 1));
            }
        }

        if (name != NULL &&                                                                     // jen mapovane cesty (na pismenko drivu)
            GetUserName(name, remoteName, userNameBuf, USERNAME_MAXLEN, providerBuf, MAX_PATH)) // nezname uzivatelske jmeno pro restorovanou connectionu
        {
            if (userNameBuf[0] != 0)
                userName = userNameBuf;
            if (providerBuf[0] != 0)
                nsBuf.lpProvider = providerBuf; // znalost providera uzasne zrychluje sitovou odezvu (aspon na XP)
        }
    }

    CEnterPasswdDialog dlg(parent, remoteName, userName);

    DWORD err;
    char* passwd = NULL;

    // XP+Vista: dynamicky vytahneme funkce pro ziskani username+password ve standardnim dialogu (vcetne moznosti ulozit do Credential Manageru - viz "Manage your network passwords" v User Accounts v Control Panelu)
    // Windows 7: maji zase novy dialog a zatim jsem k nemu neobjevil rozhrani
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

    BOOL connectInteractive = FALSE; // TRUE: Windows 7 vzdy, XP+Vista jen pokud neni k dispozici CREDUI.DLL nebo nejde o prostou UNC cestu: pouzijeme flag CONNECT_INTERACTIVE (umi delat s "network passwords") a tedy nutne i blokujici volani WNetAddConnection3 (musi byt s parentem ve stejnem threadu)
    BOOL confirmCred = FALSE;        // TRUE: je potreba volat CredUIConfirmCredentialsA

    //  char domain[DOMAIN_MAXLEN];
    CREDUI_INFOA uiInfo = {0};
    uiInfo.cbSize = sizeof(uiInfo);
    uiInfo.hwndParent = parent;

    char captionBuf[MAX_PATH + 100];
    captionBuf[0] = 0;
    char messageBuf[MAX_PATH + 100];
    messageBuf[0] = 0;

    if (name == NULL) // mapovani UNC cest na "none"
    {
        if (!Windows7AndLater && // na Windows 7 je zase novy dialog a zatim jsem k nemu neobjevil rozhrani
            credUIPromptForCredentialsA != NULL && credUIConfirmCredentialsA != NULL /*&& credUIParseUserNameA != NULL*/ &&
            serverName[0] != 0) // vlastni dialog zadani jmena a hesla ukazeme jen pro proste UNC cesty (\\server\share), DFS a dalsi prenechame systemu
        {
            BOOL save = FALSE;

            err = credUIPromptForCredentialsA(&uiInfo, serverName, NULL, 0, dlg.User, sizeof(dlg.User),
                                              dlg.Passwd, sizeof(dlg.Passwd), &save,
                                              CREDUI_FLAGS_EXPECT_CONFIRMATION);
            if (err == ERROR_CANCELLED)
                ret = FALSE; // user dal Cancel
            else
            {
                if (lpNetResource == NULL)
                    UpdateWindow(MainWindow->HWindow); // pro pouziti z nethoodu nema smysl
                if (err == NO_ERROR)                   // user dal OK
                {
                    confirmCred = TRUE;
                    lstrcpyn(userNameBuf, dlg.User, USERNAME_MAXLEN);
                    userName = userNameBuf;
                    passwd = dlg.Passwd;
                    /* // tady byl problem s orezem domeny z username, proste se jim neslo logovat, protoze jsme z jejich domenoveho jmena udelali lokalni
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
            connectInteractive = TRUE;  // jina chyba, at si s tim poradi systemova varianta
            err = ERROR_BAD_USERNAME;
          }
*/
                }
                else
                    connectInteractive = TRUE; // jina chyba, at si s tim poradi systemova varianta
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

        if (connectInteractive) // nechame WNetAddConnection3 ukazat standardni okenko pro zadani user+password (resi i Credential Managera)
        {
            err = WNetAddConnection3(parent, ns, passwd, userName, CONNECT_INTERACTIVE | (name != NULL ? CONNECT_UPDATE_PROFILE : 0)); // neanonymni si pamatujeme
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
                                                     name != NULL ? CONNECT_UPDATE_PROFILE : 0, // neanonymni si pamatujeme
                                                     &errProviderCode, errBuf, errProviderName);

                DestroySafeWaitWindow();
            }
            SetCursor(oldCur);

            memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
            passwd = NULL;

            if (confirmCred) // nahlasime vysledek overeni hesla (podle toho se bude nebo nebude neukladat do Credential Manageru)
            {
                credUIConfirmCredentialsA(serverName, err == ERROR_SUCCESS);
                confirmCred = FALSE;
            }

            if (brk)
            {
                ret = FALSE; // ESC v NonBlockingWNetAddConnection3()
                err = ERROR_CANCELLED;
                break;
            }
        }

        if (err == ERROR_SESSION_CREDENTIAL_CONFLICT)
        {
            if (lpNetResource == NULL) // chyba se ukaze v nethoodu, tady ji ukazovat nebudeme
            {
                SalMessageBox(parent, LoadStr(IDS_CREDENTIALCONFLICT), LoadStr(IDS_NETWORKERROR),
                              MB_OK | MB_ICONEXCLAMATION);
            }
            ret = FALSE;
            break;
        }

        if (err == ERROR_ALREADY_ASSIGNED)
        {
            // asi renonc - ukazujeme, ze drive neni pripojeny a pritom uz pripojeny je -> nechame
            // rebuildnout drive bar i change drive menu
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            break; // vracime TRUE
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
            if (!Windows7AndLater && // na Windows 7 je zase novy dialog a zatim jsem k nemu neobjevil rozhrani
                credUIPromptForCredentialsA != NULL && credUIConfirmCredentialsA != NULL /*&& credUIParseUserNameA != NULL*/ &&
                serverName[0] != 0) // vlastni dialog zadani jmena a hesla ukazeme jen pro proste UNC cesty (\\server\share), DFS a dalsi prenechame systemu
            {
                BOOL save = FALSE;
                err = credUIPromptForCredentialsA(&uiInfo, serverName, NULL, 0, dlg.User, sizeof(dlg.User),
                                                  dlg.Passwd, sizeof(dlg.Passwd), &save,
                                                  (name != NULL ? CREDUI_FLAGS_DO_NOT_PERSIST | CREDUI_FLAGS_GENERIC_CREDENTIALS : CREDUI_FLAGS_EXPECT_CONFIRMATION));
                if (err == ERROR_CANCELLED)
                {
                    ret = FALSE; // user dal Cancel
                    break;
                }
                else
                {
                    if (lpNetResource == NULL)
                        UpdateWindow(MainWindow->HWindow); // pro pouziti z nethoodu nema smysl
                    if (err == NO_ERROR)                   // user dal OK
                    {
                        if (name == NULL)
                            confirmCred = TRUE;
                        lstrcpyn(userNameBuf, dlg.User, USERNAME_MAXLEN);
                        userName = name != NULL && dlg.User[0] == 0 ? NULL : userNameBuf;
                        passwd = name != NULL && dlg.User[0] == 0 && dlg.Passwd[0] == 0 ? NULL : dlg.Passwd;
                        continue;
                        /*  // tady byl problem s orezem domeny z username, proste se jim neslo logovat, protoze jsme z jejich domenoveho jmena udelali lokalni
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
              newConnectInteractive = TRUE;  // jina chyba, at si s tim poradi systemova varianta
              err = ERROR_BAD_USERNAME;
            }
*/
                    }
                    else
                        newConnectInteractive = TRUE; // jina chyba, at si s tim poradi systemova varianta
                }
                memset(dlg.Passwd, 0, sizeof(dlg.Passwd));
                passwd = NULL;
            }
            else
                newConnectInteractive = TRUE;

            if (newConnectInteractive)
            {
                if (!connectInteractive) // zkusime interaktivni rezim jen pokud jsme v nem uz nejeli (obrana proti nekonecnemu cyklu)
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
            if (lpNetResource == NULL) // chyba se ukaze v nethoodu, tady ji ukazovat nebudeme
            {
                SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                              MB_OK | MB_ICONEXCLAMATION);
            }
            ret = FALSE;
            break;
        }
        else
        {
            // u Tomase Jelinka po pripojeni se na non-accessible UNC drive nedoslo
            // k zaslani notifikace a tedy k rebuildu DriveBar a zustal tam preskrtly drive
            // u me na W2K notifikace chodi az po dvou vterinach
            // proc si ji tedy nevynutit obratem:
            if (MainWindow != NULL && MainWindow->HWindow != NULL)
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            break; // vracime TRUE
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

    DWORD netDrives; // bitove pole network disku
    char netRemotePath['z' - 'a' + 1][MAX_PATH];

    GetNetworkDrives(netDrives, netRemotePath);

    if (netDrives & (1 << (LowerCase[drive] - 'a'))) // disk existoval, zkusime obnovu
    {
        char name[4] = " :";
        name[0] = drive;
        if (GetLogicalDrives() & (1 << (LowerCase[drive] - 'a'))) // teoreticky neni co delat, je pristupny... ale na Viste po hibernaci dokaze mapovany disk vracet stale stejnou chybu (napr. "(31) device attached to system is not functioning"), probereme ho az pristupem na UNC cestu, zrejme nejaka MS chyba, ale v Exploreru jim to funguje, cert vi co tam delaji
        {
            name[2] = '\\';
            name[3] = 0;
            DWORD err = NO_ERROR;
            char* netPath = netRemotePath[LowerCase[drive] - 'a'];
            if (netPath[0] == '\\' && netPath[1] == '\\' && strchr(netPath + 2, '\\') != NULL &&                                   // aspon primitivni test na validni UNC cestu
                (err = SalCheckPath(FALSE, name, ERROR_SUCCESS, TRUE, parent)) != ERROR_SUCCESS && err != ERROR_USER_TERMINATED && // mapovany disk neni pristupny
                (err = SalCheckPath(FALSE, netPath, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS &&                              // UNC je pristupna
                (err = SalCheckPath(FALSE, name, ERROR_SUCCESS, TRUE, parent)) == ERROR_SUCCESS)                                   // ted uz je pristupny i mapovany disk
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
        return FALSE; // neni UNC cesta zakladniho formatu

    char root[MAX_PATH + 4];
    char* s = root + GetRootPath(root, UNCPath);
    strcpy(s, "*.*");

    WIN32_FIND_DATA data;
    HANDLE h;
    if ((h = HANDLES_Q(FindFirstFile(root, &data))) != INVALID_HANDLE_VALUE)
    { // root cesta UNC je pristupna, neni co resit
        HANDLES(FindClose(h));
    }
    else // root UNC cesty je nelistovatelny
    {
        *(s - 1) = 0; // odrizneme backslash na konci root cesty
        DWORD err = GetLastError();
        BOOL trySharepoint = err == ERROR_BAD_NET_NAME; // zkusit pri chybe 67 zavolat shell, aby si cestu zpristupnil
        if (!donotReconnect &&
            (err == ERROR_FILE_NOT_FOUND || err == ERROR_PATH_NOT_FOUND)) // na XPckach se vraci tato chyba pokud user nema pristup na server nebo pokud dany share na serveru neexistuje, pro rozliseni techto dvou chyb volame WNetAddConnection3
        {
            NETRESOURCE ns;
            ns.dwType = RESOURCETYPE_DISK;
            ns.lpLocalName = NULL;
            ns.lpRemoteName = root;
            ns.lpProvider = NULL;
            if (!NonBlockingWNetAddConnection3(err, &ns, NULL, NULL, 0, NULL, NULL, NULL))
                err = ERROR_PATH_NOT_FOUND; // ESC
        }

        if (IsLogonFailureErr(err) || // obrana proti napr. "no files found"
            IsAdminShareExtraLogonFailureErr(err, root))
        {
            if (donotReconnect)
                pathInvalid = TRUE; // rovnou hlasime chybu
            else
            {
                // Petr: zakomentoval jsem, Explorer ani TC zadnou chybu pred zobrazenim dialogu pro zadani usera + passwordu neukazuji; navic
                //       my ted nove chybu ukazujeme po pokusu o navazani spojeni (aby se user dozvedel, ze ma expirnuty password, atd.)
                //          SalMessageBox(parent, GetErrorText(err), LoadStr(IDS_NETWORKERROR),
                //                        MB_OK | MB_ICONEXCLAMATION);

                pathInvalid = !RestoreNetworkConnection(parent, NULL, root);
                return !pathInvalid;
            }
        }
        else
        {
            if (trySharepoint) // zkusime pri chybe 67 zavolat shell, aby si cestu zpristupnil
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
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

    // docasne snizime prioritu threadu, aby nam nejaka zmatena shell extension nesezrala CPU
    HANDLE hThread = GetCurrentThread(); // pseudo-handle, neni treba uvolnovat
    int oldThreadPriority = GetThreadPriority(hThread);
    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    __try
    {
        UINT flags = CMF_NORMAL | CMF_EXPLORE;
        // osetrime stisknuty shift - rozsirene kontextove menu, pod W2K je tam napriklad Run as...
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
    case DRIVE_NO_ROOT_DIR: // subst, kteremu byl vymazan adresar (muze byt i remote, ale na to kasleme)
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

unsigned ReadCDVolNameThreadFBody(void* param) // test pristupnosti adresare
{
    CALL_STACK_MESSAGE1("ReadCDVolNameThreadFBody()");
    UINT_PTR uid = (UINT_PTR)param;
    char root[MAX_PATH];
    root[0] = 0;
    char buf[MAX_PATH];
    buf[0] = 0;

    HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
    BOOL run = FALSE;
    if (uid == ReadCDVolNameReqUID) // jeste nekdo ceka na odpoved
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
        if (uid == ReadCDVolNameReqUID) // jeste nekdo ceka na odpoved
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
        TerminateProcess(GetCurrentProcess(), 1); // tvrdsi exit (tenhle jeste neco vola)
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
    char* newText = (char*)malloc(strlen(driveText) + 15); // nechame si rezervu 14 znaku (" [-1234567890]")
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
    for (z = 0; z < count; z++) // dohledane zapouzdreni FS (pri chybe nedostatku pameti nemusi sedet indexy v fsList a Drives (od offsetu firstFSIndex))
    {
        if (fsList[z]->GetInterface() == fsIface)
        {
            fs = fsList[z];
            break;
        }
    }
    if (fs != NULL) // vyber indexu pro polozku (bud z FS nebo pridelime sekvencne)
    {
        if (fs->GetChngDrvDuplicateItemIndex() < currentIndex)
            fs->SetChngDrvDuplicateItemIndex(currentIndex);
        else
            currentIndex = fs->GetChngDrvDuplicateItemIndex();
    }
    return currentIndex;
}

// na zaklade 'hSrcIcon' vytvori cernobilou verzi ikony
// POZOR, POMALE, pouzivat s rozvahou
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

// provede in-place decode z base64; vraci uspech;
// krome stavu, kdy je 'input_length' nula, vraci nulou terminovany string
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

// cesta k lokalnimu adresari Dropboxu
char DropboxPath[MAX_PATH] = "";

void InitDropboxPath()
{
    static BOOL alreadyCalled = FALSE;
    if (!alreadyCalled) // zjistovat cestu ma smysl jen poprve, pak uz jen ignorujeme
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
                    if (GetFileSizeEx(hFile, &size) && size.QuadPart < 100000) // 100KB je az az na blbej konfigurak
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
                            if (secRow < secRowEnd) // mam text 2. radky, kde je base64 zakodovana hledana cesta
                            {
                                int pathLen;
                                if (base64_decode(secRow, (int)(secRowEnd - secRow), &pathLen, "Dropbox path: "))
                                {
                                    WCHAR widePath[MAX_PATH]; // delsi cesty stejne neumime
                                    char mbPath[MAX_PATH];    // ANSI nebo UTF8 cesta
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

// {A52BBA46-E9E1-435f-B3D9-28DAA648C0F6}  // OneDrive slozka ze systemu, zavedli az od Windows 8.1
my_DEFINE_KNOWN_FOLDER(my_FOLDERID_SkyDrive, 0xa52bba46, 0xe9e1, 0x435f, 0xb3, 0xd9, 0x28, 0xda, 0xa6, 0x48, 0xc0, 0xf6);

// cesta k lokalnimu adresari OneDrive - Personal (jen pro personal ucet, pro business ucty mame OneDriveBusinessStorages)
char OneDrivePath[MAX_PATH] = "";

// cesty k lokalnim adresarum OneDrive - Business (jen pro business ucty, pro personal ucet mame OneDrivePath)
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
    OneDrivePath[0] = 0; // cestu k OneDrive zjistujeme dokola, protoze po prikazu "Unlink OneDrive" (z OneDrive) bysme ho meli prestat ukazovat

    BOOL done = FALSE;
    if (WindowsVistaAndLater) // SHGetKnownFolderPath existuje od Visty
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
                if (path[0] != 0) // FOLDERID_SkyDrive zavedli ve Windows 8.1 = uz by nemelo byt potreba to lovit v registry
                {
                    done = ConvertU2A(path, -1, OneDrivePath, _countof(OneDrivePath)) != 0;
                    if (!done)
                        OneDrivePath[0] = 0; // jen pro sychr
                                             //else TRACE_I("OneDrive path (FOLDERID_SkyDrive): " << OneDrivePath);
                }
                CoTaskMemFree(path);
            }
        }
    }

    HKEY hKey;
    char path[MAX_PATH];
    for (int i = 0; !done && i < 2; i++) // teoreticky potreba jen pro Windows 8 a nizsi (od 8.1 mame FOLDERID_SkyDrive)
    {
        if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER,
                                   Windows8_1AndLater && !Windows10AndLater ? (i == 0 ? "Software\\Microsoft\\Windows\\CurrentVersion\\OneDrive" : // jen Win 8.1
                                                                                   "Software\\Microsoft\\Windows\\CurrentVersion\\SkyDrive")
                                                                            : (i == 0 ? "Software\\Microsoft\\OneDrive" : "Software\\Microsoft\\SkyDrive"), // krom Win 8.1
                                   0, KEY_READ, &hKey)) == ERROR_SUCCESS)
        {
            DWORD size = sizeof(path); // velikost v bytech
            DWORD type;
            if (SalRegQueryValueEx(hKey, "UserFolder", 0, &type, (BYTE*)path, &size) == ERROR_SUCCESS &&
                type == REG_SZ && size > 1)
            {
                //TRACE_I("OneDrive path (UserFolder): " << path);
                strcpy_s(OneDrivePath, path); // mame pozadovanou cestu
                done = TRUE;
            }
            HANDLES(RegCloseKey(hKey));
        }
    }

    // nacteme z registry OneDrive Business storage
    OneDriveBusinessStorages.DestroyMembers();
    if (HANDLES_Q(RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\OneDrive\\Accounts",
                               0, KEY_READ, &hKey)) == ERROR_SUCCESS)
    {
        DWORD i = 0;
        while (1)
        { // postupne enumnuti vsech pripon
            char keyName[300];
            DWORD keyNameLen = _countof(keyName);
            if (RegEnumKeyEx(hKey, i, keyName, &keyNameLen, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
            { // otevreni klice uctu
                HKEY hAccount;
                if (StrICmp(keyName, "Personal") != 0 && // personal ucet tady cist nechceme (viz OneDrivePath)
                    HANDLES_Q(RegOpenKeyEx(hKey, keyName, 0, KEY_READ, &hAccount)) == ERROR_SUCCESS)
                { // cteme vsechny ucty mimo Personal (zatim asi jen "Business???", kde ??? je poradove cislo)
                    char disp[ONEDRIVE_MAXBUSINESSDISPLAYNAME];
                    DWORD size = sizeof(disp); // velikost v bytech
                    DWORD type;
                    if (SalRegQueryValueEx(hAccount, "DisplayName", 0, &type, (BYTE*)disp, &size) == ERROR_SUCCESS &&
                        type == REG_SZ && size > 1)
                    {
                        size = sizeof(path); // velikost v bytech
                        if (SalRegQueryValueEx(hAccount, "UserFolder", 0, &type, (BYTE*)path, &size) == ERROR_SUCCESS &&
                            type == REG_SZ && size > 1)
                        { // posbirame vse co ma DisplayName a UserFolder, at uz je to cokoliv nabidneme to userovi pod "OneDrive"
                            //TRACE_I("OneDrive Business: DisplayName: " << disp << ", UserFolder: " << path);
                            OneDriveBusinessStorages.SortIn(new COneDriveBusinessStorage(DupStr(disp), DupStr(path)));
                        }
                    }
                    HANDLES(RegCloseKey(hAccount));
                }
            }
            else
                break; // konec enumerace (muze byt i maly buffer, ale to nepovazuju za rozumny osetrovat)
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
    // POZOR: pro drvtOneDriveBus se DisplayName pozdeji taha z drv.DriveText, pri zmene formatu textu predelat !!!
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
        TRACE_C("CDrivesList::AddToDrives(): unsupported combination of parameters"); // getGrayIcons je TRUE jen pro drive-bar a tam je jen jedna ikona (tlacitko)
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

    // nejnizsi bit odpovida 'A', druhy bit 'B', ...
    // dostupne disky
    DWORD mask = GetLogicalDrives();

    CDriveData drv;
    drv.Param = 0;
    drv.DestroyIcon = TRUE; // tyto ikony jsou alokovane
    drv.PluginFS = NULL;    // jen pro poradek
    drv.DLLName = NULL;     // jen pro poradek
    drv.HGrayIcon = NULL;

    CDriveData drvSeparator;
    ZeroMemory(&drvSeparator, sizeof(drvSeparator));
    drvSeparator.DriveType = drvtSeparator;

    if (copyDrives != NULL) // optimalizace: data se jen zkopiruji (napr. jde o druhou Drives Baru, takze data pro ni zkopirujeme z prvni Drives Bary, pro kterou jsme je pred par ms ziskali ze systemu)
    {
        CachedDrivesMask = copyCachedDrivesMask;

        // pridam disky a..z
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
        // zapamatovane a neobnovene sitove disky nebudou vraceny z GetLogicalDrives()
        DWORD netDrives; // bitove pole network disku
        char netRemotePath['z' - 'a' + 1][MAX_PATH];
        GetNetworkDrives(netDrives, netRemotePath);

        CachedDrivesMask = mask | netDrives; // cache pro snadny test, zda pribyl/zmizel disk

        // disky, ktere userum nemame ukazovat
        DWORD noDrivesPolicy = SystemPolicies.GetNoDrives();
        noDrivesPolicy |= DRIVES_MASK & (~Configuration.VisibleDrives);

        char drive = 'A';

        CQuadWord freeSpace; // kolik mame na disku mista

        Shares.PrepareSearch(""); // ted budeme hledat rooty disku

        BOOL separateNextDrive = FALSE; // nez vlozime drive, mame vlozit separator?

        // pridam disky a..z
        while (i != 0)
        {
            if (!(noDrivesPolicy & i) && ((mask & i) || (netDrives & i))) // disk je pristupny
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
                case drvtRemovable: // diskety, zjistime jestli to je 3.5", 5.25", 8" nebo neznama
                {
                    volumeName[0] = 0;
                    int drvIndex = drive - 'A' + 1;
                    if (drvIndex >= 1 && drvIndex <= 26) // pro jistotu provedeme "range-check"
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
                        // zdvojime '&', aby se nezobrazovalo jako podtrzeni
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
                    // zdvojime '&', aby se nezobrazovalo jako podtrzeni
                    DuplicateAmpersands(volumeName, MAX_PATH);
                    break;
                }

                case drvtCDROM:
                {
                    HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
                    UINT_PTR uid = ++ReadCDVolNameReqUID;
                    lstrcpyn(ReadCDVolNameBuffer, root, MAX_PATH);
                    HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));

                    // nahodime thread, ve kterem zjistime volume_name CD drivu
                    DWORD threadID;
                    HANDLE thread = HANDLES(CreateThread(NULL, 0, ReadCDVolNameThreadF,
                                                         (void*)uid, 0, &threadID));
                    if (thread != NULL && WaitForSingleObject(thread, noTimeout ? INFINITE : 500) == WAIT_OBJECT_0)
                    { // dame mu 500ms na zjisteni volume-name
                        HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
                        lstrcpyn(volumeName, ReadCDVolNameBuffer, MAX_PATH + 50);
                        HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));
                    }
                    else
                        volumeName[0] = 0;
                    if (thread != NULL)
                        AddAuxThread(thread, TRUE); // pokud by thread nestihl dobehnout, vykillujeme ho pred uzavrenim softu
                    if (volumeName[0] == 0)
                        strcpy(volumeName, LoadStr(IDS_COMPACT_DISK));
                    else
                    {
                        // zdvojime '&', aby se nezobrazovalo jako podtrzeni
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

                // separujeme drivy, u kterych si to uzivatel pral
                if (separateNextDrive && Drives->Count > 0 && !IsLastItemSeparator())
                    Drives->Add(drvSeparator);
                // pokud v systemu neni mechanika A a B a uzivatel si nastavil separator za A nebo B, zobrazili jsme ho za C
                // proto musime shodit flag v kazdem pripade, nejen v predchozi podmince
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

    // pridame odpojene a aktivni FS
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
                        currentFSIndex = index; // aktivni FS
                }
            }
            // zkontrolujeme unikatnost textu polozek, pripadne duplicitni polozky oindexujeme
            for (i = firstFSIndex; i < Drives->Count; i++)
            {
                BOOL freeDriveText = FALSE;
                char* driveText = Drives->At(i).DriveText;
                int currentIndex = 1;
                int x;
                for (x = i + 1; x < Drives->Count; x++)
                {
                    char* testedDrvText = Drives->At(x).DriveText;
                    if (StrICmp(driveText, testedDrvText) == 0) // shoda -> musime polozku oindexovat
                    {
                        if (!freeDriveText) // nalezena prvni shoda, musime oindexovat i prvni vyskyt duplicitni polozky
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
    drv.PluginFS = NULL; // jen pro poradek
    drv.DLLName = NULL;  // jen pro poradek
    int iconSize = GetIconSizeForSystemDPI(ICONSIZE_16);

    // pridam separator, pokud nebude prvni a pokud uz tam jeden neni
    if (Drives->Count > 0 && !IsLastItemSeparator())
        Drives->Add(drvSeparator);

    // pridam Documents
    if (Configuration.ChangeDriveShowMyDoc)
    {
        AddToDrives(drv, IDS_MYDOCUMENTS, ';', drvtMyDocuments, getGrayIcons,
                    SalLoadIcon(ImageResDLL, 112, iconSize));
    }

    // pridam Cloud Storages (Google Drive, atd.), teda pokud nejaky najdu...
    CachedCloudStoragesMask = 0;
    if (Configuration.ChangeDriveCloudStorage)
    {
        CSQLite3DynLoadBase* sqlite3_Dyn_InOut = NULL; // nejak jsem ocekaval, ze sqlite3 bude potreba i pro Dropbox, coz se nakonec nesplnilo (takze tahle opicarna je tu jaksi do foroty)
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
        if (forDriveBar && c > 1) // data pro drive-bar && vice storages = nechame usera vybrat z menu (drop down)
        {
            strcpy_s(itemText, LoadStr(IDS_ONEDRIVE));
            AddToDrives(drv, 0, 0, drvtOneDriveMenu, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
            destroyOneDriveIco = FALSE;
        }
        else // data pro change drive menu || drive-bar && jediny storage (dame jednoduche tlacitko na drive-bar)
        {
            if (OneDrivePath[0] != 0) // personal
            {
                if (c == 1)
                    strcpy_s(itemText, LoadStr(IDS_ONEDRIVE)); // jediny personal storage = piseme jen: OneDrive
                else
                    sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), LoadStr(IDS_ONEDRIVEPERSONAL));
                AddToDrives(drv, 0, 0, drvtOneDrive, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
                destroyOneDriveIco = FALSE;
            }
            for (int i = 0; i < OneDriveBusinessStorages.Count; i++) // business
            {                                                        // POZOR: pro drvtOneDriveBus se DisplayName pozdeji taha z drv.DriveText, pri zmene formatu textu predelat !!!
                sprintf_s(itemText, "%s - %s", LoadStr(IDS_ONEDRIVE), OneDriveBusinessStorages[i]->DisplayName);
                AddToDrives(drv, 0, 0, drvtOneDriveBus, getGrayIcons, oneDriveIco, destroyOneDriveIco, itemText);
                destroyOneDriveIco = FALSE;
            }
        }
        if (destroyOneDriveIco)
            TRACE_C("CDrivesList::BuildData(): OneDrive icon unused, should never happen");

        if (sqlite3_Dyn_InOut != NULL)
            delete sqlite3_Dyn_InOut; // uvolneni jiz nepotrebneho sqlite.dll
    }

    // pridam Okolni Pocitace
    CPluginData* nethoodPlugin = NULL;
    BOOL existsNethoodPlugin = Plugins.GetFirstNethoodPluginFSName(NULL, &nethoodPlugin);
    if (!SystemPolicies.GetNoNetHood() && Configuration.ChangeDriveShowNet &&
        !existsNethoodPlugin)
    {
        AddToDrives(drv, IDS_NETWORKDRIVE, ',', drvtNeighborhood, getGrayIcons,
                    SalLoadIcon(ImageResDLL, 152, iconSize));
        neighborhoodIndex = Drives->Count - 1;
    }

    // pridam prikazy FS ze vsech plug-inu
    if (!Plugins.AddItemsToChangeDrvMenu(this, currentFSIndex,
                                         FilesWindow->GetPluginFS()->GetPluginInterfaceForFS()->GetInterface(),
                                         getGrayIcons))
    {
        return FALSE;
    }

    // dohledam index Nethood pluginu a nastavim ho do neighborhoodIndex (zastupujeme Network polozku se vsim vsudy)
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

    // urcim aktivni polozku
    FocusIndex = -1;
    if (FilesWindow->Is(ptPluginFS))
    {
        if (currentFSIndex != -1)
            FocusIndex = currentFSIndex;
    }
    else
    {
        if (CurrentPath[0] != 0) // jen pokud mame cestu (neni to ptPluginFS)
        {
            if (currentDiskIndex != -1)
                FocusIndex = currentDiskIndex;
            if (FocusIndex == -1 && neighborhoodIndex != -1)
                FocusIndex = neighborhoodIndex;
        }
    }

    // pridam Another Panel
    if (Configuration.ChangeDriveShowAnother)
    {
        // varianta s retezcem 'Another Panel Path'
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
                drv.DestroyIcon = TRUE; // tyto ikony jsou alokovane
            }
            else
            {
                if (panel->Is(ptZIPArchive))
                {
                    drv.HIcon = SalLoadIcon(ImageResDLL, 174, iconSize);
                    drv.HGrayIcon = NULL;
                    drv.DestroyIcon = TRUE; // tato ikona je alokovana
                }
                else
                {
                    if (panel->Is(ptPluginFS))
                    {
                        BOOL destroyIcon;
                        HICON icon = panel->GetPluginFS()->GetFSIcon(destroyIcon);
                        if (icon != NULL) // plug-inem definovana
                        {
                            drv.HIcon = icon;
                            drv.HGrayIcon = NULL;
                            drv.DestroyIcon = destroyIcon; // ridime se podle pluginu
                        }
                        else // standardni
                        {
                            drv.HIcon = SalLoadIcon(HInstance, IDI_PLUGINFS, iconSize);
                            drv.HGrayIcon = NULL;
                            drv.DestroyIcon = TRUE; // tato ikona je alokovana
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

    // naleju hot paths
    drv.DestroyIcon = FALSE;
    drv.HIcon = HFavoritIcon;
    drv.HGrayIcon = NULL;
    BOOL addSeparator = (Drives->Count > 0 && !IsLastItemSeparator()); // nechceme dva separatory za sebou
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
                    // pridam separator
                    Drives->Add(drvSeparator);
                    addSeparator = FALSE;
                }
                char text[MAX_PATH + 10];
                if (i < 10)
                    sprintf(text, "%d\t%s", i == 9 ? 0 : i + 1, srcName);
                else
                    sprintf(text, "\t%s", srcName);
                // zdvojime '&', aby se nezobrazovalo jako podtrzeni
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
                    drv.DestroyIcon = FALSE; // ostatni ikony uz nebudu uklizet
                }
                else
                {
                    TRACE_E(LOW_MEMORY);
                    return FALSE;
                }
            }
        }
    }

    // nechceme na konci samotny separator
    if (Drives->Count > 0 && IsLastItemSeparator())
        Drives->Delete(Drives->Count - 1);

    if (drv.DestroyIcon) // zadna hot-path, musime tu ikonu oddelat tady
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
                HANDLES(DestroyIcon(drives->At(i).HIcon)); // pres GetDriveIcon
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
            if (i == FocusIndex) // je-li FocusIndex==-1 neoznaci se nic
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
    // transfer ven
    CDriveTypeEnum dt = item->DriveType;
    *DriveType = dt;
    switch (dt)
    {
    case drvtOneDriveBus:
    {
        // POZOR: pro drvtOneDriveBus se zde taha DisplayName z drv.DriveText, pri zmene formatu textu predelat !!!
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

    case drvtOneDriveMenu: // otevreme drop down menu pro vyber OneDrive storage
    {
        if (fromDropDown != NULL)
            *fromDropDown = TRUE;
        InitOneDrivePath();
        int c = GetOneDriveStorages();
        if (c <= 1)
        { // OneDrive uz neni na drop down, provedeme refresh obou Drive bar, at zmizi nebo se updatnou ikony
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

        // pokus o oziveni
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

    case drvtPluginFS:             // polozka z plug-inu: otevreny FS (aktivni/odpojeny)
    case drvtPluginFSInOtherPanel: // nikdy by nemelo prijit, pripadne to stopneme pozdeji (zadna akce)
    {
        *DriveTypeParam = (DWORD_PTR)item->PluginFS;
        break;
    }

    case drvtPluginCmd: // polozka z plug-inu: prikaz FS
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

    // zasynchronizujeme drive bars, pokud je to potreba
    MainWindow->RebuildDriveBarsIfNeeded(TRUE, GetCachedDrivesMask(), // pro urychleni pouzijeme nase napocitane masky
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
    FilesWindow->OpenedDrivesList = this; // aby nam FilesWindow dorucilo notifikace
    DWORD cmd = MenuPopup->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_SELECT | MENU_TRACK_VERTICAL,
                                 buttonRect.left, buttonRect.bottom,
                                 FilesWindow->HWindow, &buttonRect);
    FilesWindow->OpenedDrivesList = NULL;

    // nechame nastavit navratove promenne
    if (cmd != 0)
        if (!ExecuteItem(cmd - 1, NULL, NULL, NULL))
            cmd = 0;

    DestroyData();
    delete MenuPopup;
    MenuPopup = NULL;

    // jeste jednou zasynchronizujeme drive bars (mohlo dojit ke zmene behem otevreneho menu)
    MainWindow->RebuildDriveBarsIfNeeded(TRUE, GetCachedDrivesMask(), // pro urychleni pouzijeme nase napocitane masky
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
    return FALSE; // nechceme fs, ...
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
    case drvtRemovable: // diskety, zjistime jestli to je 3.5", 5.25", 8" nebo neznama
    {
        root[0] = item->DriveText[0];
        volumeName[0] = 0;
        int drv = item->DriveText[0] - 'A' + 1;
        if (drv >= 1 && drv <= 26) // pro jistotu provedeme "range-check"
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

        // nahodime thread, ve kterem zjistime volume_name CD drivu
        DWORD threadID;
        HANDLE thread = HANDLES(CreateThread(NULL, 0, ReadCDVolNameThreadF,
                                             (void*)uid, 0, &threadID));
        if (thread != NULL && WaitForSingleObject(thread, 500) == WAIT_OBJECT_0)
        { // dame mu 500ms na zjisteni volume-name
            HANDLES(EnterCriticalSection(&ReadCDVolNameCS));
            lstrcpyn(volumeName, ReadCDVolNameBuffer, MAX_PATH + 50);
            HANDLES(LeaveCriticalSection(&ReadCDVolNameCS));
        }
        else
            volumeName[0] = 0;
        if (thread != NULL)
            AddAuxThread(thread, TRUE); // pokud by thread nestihl dobehnout, vykillujeme ho pred uzavrenim softu
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
        // orizneme prvni sloupec ve jmene polozky
        const char* p = item->DriveText;
        while (*p != 0 && *p != '\t')
            p++;
        if (*p == '\t')
        {
            const char* e = p + 1; // orizneme pripadny treti sloupec ve jmene polozky (muze byt za druhym TABem)
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

    // na promennou MenuPopup se smi pristupovat pouze je-li posByMouse == FALSE

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

    // naleznu vybranou polozku
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
        { // absolutni cesta na disk nebo sit (UNC)
            SlashesToBackslashesAndRemoveDups(path);
            int type;
            BOOL isDir;
            char* secondPart;
            if (!SalParsePath(MainWindow->HWindow, path, type, isDir, secondPart, LoadStr(IDS_ERRORTITLE),
                              NULL, FALSE, NULL, NULL, NULL, 2 * MAX_PATH) ||
                type != PATH_TYPE_WINDOWS || // nejde o windowsovou cestu
                !isDir || *secondPart != 0)  // cesta k souboru (ne adresari) nebo cast cesty neexistuje
            {
                return FALSE; // kontextove menu umime jen pro windowsovy cesty (ne pro cesty do archivu)
            }
        }
        else
            return FALSE; // pro jine typy cest (relativni cesty nebo pluginove FS) neumime kontextove menu
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
            // musime overit, ze 'fs' je stale platny interface
            if (FilesWindow->Is(ptPluginFS) && FilesWindow->GetPluginFS()->GetInterface() == fs)
            { // vyber aktivniho FS - provedeme refresh
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
                    { // vyber odpojeneho FS
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

            char pluginFSNameBuf[MAX_PATH]; // 'pluginFS' muze prestat existovat, radsi dame 'fsName' do lokalniho bufferu
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
                    if (MenuPopup != NULL) // hrozi refresh seznamu polozek change drive menu behem otevreneho context menu, takze hrozi i zmena fokusu na jinou polozku, nez pro kterou jsme ukazovali menu a tim by se post-prikaz poslal jinam nez potrebujeme
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
                        if (postCmd != 0) // zavreni Change Drive menu + spusteni prikazu postCmd
                        {
                            *PostCmd = postCmd;
                            *PostCmdParam = postCmdParam;
                            *FromContextMenu = TRUE;
                            return TRUE;
                        }
                        else // pouhe zavreni Change Drive menu
                        {
                            *PostCmd = 0; // zbytecne (je nastaveno z konstruktoru), jen pro prehlednost
                            *FromContextMenu = TRUE;
                            return TRUE;
                        }
                    }
                }
                if (refreshMenu) // plug-in si zada refresh menu
                {
                    PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);
                }
            }
        }
        return FALSE;
    }

    case drvtPluginFSInOtherPanel: // zadne menu neotevreme (disablovana polozka menu)
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
        // pro Documents, Network, As Other Panel a Cloud Storages neumime kontextove menu
        return FALSE;
    }

    default:
    {
        TRACE_E("Unhandled DriveType = " << dt);
        return FALSE;
    }
    }

    //p.s. zakomentovano, aby sel odpojit i prave nepristupny sitovy disk (delsi cekani se toleruje)
    //  if (Drives->At(selectedIndex).DriveType == drvtRemote &&
    //      Drives->At(selectedIndex).Accessible &&   // nepristupny umoznime odpojit, ostatni kontrolujeme
    //      MainWindow->GetActivePanel()->CheckPath(TRUE, path) != ERROR_SUCCESS) return FALSE;

    //  MainWindow->ReleaseMenuNew();  // Windows nejsou staveny na vic kontextovych menu

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

        // POZOR: behem vybaleneho context menu muze dojit k refreshi tohoto objektu (napr. user vlozi
        //        USB stick nebo se odmapuje sitovy disk), je s tim potreba pocitat pri pristupu do dat
        //        tohoto objektu (napr. Drives muze obsahovat jina data)

        if (cmd != 0)
        {
            BOOL releaseLeft = FALSE;  // odpojit levy panel od disku?
            BOOL releaseRight = FALSE; // odpojit pravy panel od disku?

            char cmdName[2000]; // schvalne mame 2000 misto 200, shell-extensiony obcas zapisuji dvojnasobek (uvaha: unicode = 2 * "pocet znaku"), atp.
            if (AuxGetCommandString(MainWindow->ContextMenuChngDrv, cmd, GCS_VERB, NULL, cmdName, 200) != NOERROR)
            {
                cmdName[0] = 0;
            }
            if (stricmp(cmdName, "properties") != 0 && // u properties neni nutne
                stricmp(cmdName, "find") != 0 &&       // u find neni nutne
                stricmp(cmdName, "open") != 0 &&       // u open neni nutne
                stricmp(cmdName, "explore") != 0 &&    // u explore neni nutne
                stricmp(cmdName, "link") != 0)         // u create-short-cut neni nutne
            {
                CFilesWindow* win;
                int i;
                for (i = 0; i < 2; i++)
                {
                    win = i == 0 ? MainWindow->LeftPanel : MainWindow->RightPanel;
                    if (HasTheSameRootPath(win->GetPath(), path)) // stejny disk (normal i UNC)
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

            BOOL changeToFixedDrv = cmd == 35; // "format" neni modalni, nutna zmena na fixed drive
            if (releaseLeft)
            {
                if (changeToFixedDrv)
                {
                    MainWindow->LeftPanel->ChangeToFixedDrive(MainWindow->LeftPanel->HWindow);
                    // POZOR: je treba provest invalidate okna, abychom porusili cachovanou bitmapu Alt+F1/2 menu
                    // jinak dochazelo k zobrazeni stare casti panelu pri teto situaci:
                    // v levem panelu je disk S:; Alt+F1, right click na S, Format
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

            // z kontextovyho menu jde zmenit clipboard, overime to ...
            IdleRefreshStates = TRUE;  // pri pristim Idle vynutime kontrolu stavovych promennych
            IdleCheckClipboard = TRUE; // nechame kontrolovat take clipboard

            UpdateWindow(MainWindow->HWindow);
            if (releaseLeft && !changeToFixedDrv)
                MainWindow->LeftPanel->HandsOff(FALSE);
            if (releaseRight && !changeToFixedDrv)
                MainWindow->RightPanel->HandsOff(FALSE);

            if (!selectedIndexAccessible) // odmapovani zapamatovaneho neaktivniho sitoveho spojeni?
                PostMessage(MainWindow->HWindow, WM_USER_DRIVES_CHANGE, 0, 0);

            /*
// notifikace bude dorucena pres WM_DEVICECHANGE
      if (GetLogicalDrives() < disks) // odmapovani?
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
    // pozadame menu o modifikaci
    if (MenuPopup->BeginModifyMode())
    {
        // sestrelime stara data
        DestroyData();
        // poridime nova
        BuildData(TRUE); // nelze pouzit timeout, cteni labelu CD trva dlouho (1,5s) - system notifikuje drive nez ma nacteno
        // vykopneme z menu polozky
        MenuPopup->RemoveAllItems();
        // naleje nove
        LoadMenuFromData();
        // nechame menu prekreslit
        MenuPopup->EndModifyMode();
    }
    return TRUE;
}

BOOL CDrivesList::FindPanelPathIndex(CFilesWindow* panel, DWORD* index)
{
    if (panel->Is(ptPluginFS))
    {
        if (!panel->GetPluginFS()->IsServiceSupported(FS_SERVICE_GETCHANGEDRIVEORDISCONNECTITEM))
        { // hledame jen pluginove FS bez vlastni polozky v Change Drive menu (tyto polozky totiz v Drive barach nejsou)
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
                return FALSE; // cesta typu "\\.\C:\"
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
