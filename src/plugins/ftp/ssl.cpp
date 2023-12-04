// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// Error codes returned by SSL_get_error()
#define SSL_ERROR_NONE 0
#define SSL_ERROR_SSL 1
#define SSL_ERROR_WANT_READ 2
#define SSL_ERROR_WANT_WRITE 3
#define SSL_ERROR_WANT_X509_LOOKUP 4
#define SSL_ERROR_SYSCALL 5 // look at error stack/return value/errno
#define SSL_ERROR_ZERO_RETURN 6
#define SSL_ERROR_WANT_CONNECT 7
#define SSL_ERROR_WANT_ACCEPT 8

// Return codes of SSL_read()
#define SSL_NOTHING 1
#define SSL_READING 2
#define SSL_WRITING 3
#define SSL_X509_LOOKUP 4

// Transferred from WinSocket
// #define WSAECONNCLOSED                 (WSABASEERR + 10000)

#define SSL_CTRL_OPTIONS 32

/* SSL_OP_ALL: various bug workarounds that should be rather harmless.
 *             This used to be 0x000FFFFFL before 0.9.7. */
#define SSL_OP_ALL 0x80000BFFL

#define SSL_OP_NO_SSLv2 0x01000000L

#define MAX_DER_CERT_SIZE 5120 // Is 5KB enough?

#define SizeOf(x) (sizeof(x) / sizeof(x[0]))

#ifndef CERT_VERIFY_REV_SERVER_OCSP_FLAG
#define CERT_VERIFY_REV_SERVER_OCSP_FLAG 8 // defined in SDK's newer than 2003
#endif

// Brought from ssl.h
#define SSL_ST_CONNECT 0x1000
#define SSL_ST_ACCEPT 0x2000
#define SSL_ST_MASK 0x0FFF
#define SSL_ST_INIT (SSL_ST_CONNECT | SSL_ST_ACCEPT)
#define SSL_ST_BEFORE 0x4000
#define SSL_ST_OK 0x03
#define SSL_ST_RENEGOTIATE (0x04 | SSL_ST_INIT)

#define SSL_CB_LOOP 0x01
#define SSL_CB_EXIT 0x02
#define SSL_CB_READ 0x04
#define SSL_CB_WRITE 0x08
#define SSL_CB_ALERT 0x4000 /* used in callback */
#define SSL_CB_READ_ALERT (SSL_CB_ALERT | SSL_CB_READ)
#define SSL_CB_WRITE_ALERT (SSL_CB_ALERT | SSL_CB_WRITE)
#define SSL_CB_ACCEPT_LOOP (SSL_ST_ACCEPT | SSL_CB_LOOP)
#define SSL_CB_ACCEPT_EXIT (SSL_ST_ACCEPT | SSL_CB_EXIT)
#define SSL_CB_CONNECT_LOOP (SSL_ST_CONNECT | SSL_CB_LOOP)
#define SSL_CB_CONNECT_EXIT (SSL_ST_CONNECT | SSL_CB_EXIT)
#define SSL_CB_HANDSHAKE_START 0x10
#define SSL_CB_HANDSHAKE_DONE 0x20

sSSLLib SSLLib;

static bool bSSLInited = false;

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

#define IMP_SYMBOL(name) {(void**)&SSLLib.name, #name},

typedef struct _SymbolInfo
{
    void** Addr;
    const char* Name;
} TSymbolInfo, *PSymbolInfo;

static TSymbolInfo SSLSymbols[] = {
    IMP_SYMBOL(SSL_get_error)
        IMP_SYMBOL(SSL_write)
            IMP_SYMBOL(SSL_read)
                IMP_SYMBOL(SSL_library_init)
                    IMP_SYMBOL(SSL_load_error_strings)
    //   IMP_SYMBOL(SSLv3_client_method)
    IMP_SYMBOL(SSLv23_client_method)
        IMP_SYMBOL(SSL_CTX_new)
            IMP_SYMBOL(SSL_CTX_free)
                IMP_SYMBOL(SSL_CTX_ctrl)
                    IMP_SYMBOL(SSL_new)
                        IMP_SYMBOL(SSL_free)
                            IMP_SYMBOL(SSL_set_fd)
                                IMP_SYMBOL(SSL_connect)
                                    IMP_SYMBOL(SSL_shutdown)
                                        IMP_SYMBOL(SSL_get_current_cipher)
                                            IMP_SYMBOL(SSL_CIPHER_get_version)
                                                IMP_SYMBOL(SSL_CIPHER_get_bits)
                                                    IMP_SYMBOL(SSL_CIPHER_get_name)
                                                        IMP_SYMBOL(SSL_get_verify_result)
                                                            IMP_SYMBOL(SSL_set_verify)
                                                                IMP_SYMBOL(SSL_pending)
                                                                    IMP_SYMBOL(SSL_get_peer_certificate)
                                                                        IMP_SYMBOL(SSL_get_peer_cert_chain)
                                                                            IMP_SYMBOL(SSL_COMP_get_compression_methods)
                                                                                IMP_SYMBOL(SSL_get1_session)
                                                                                    IMP_SYMBOL(SSL_set_session)
                                                                                        IMP_SYMBOL(SSL_SESSION_free)
                                                                                            IMP_SYMBOL(SSL_ctrl)
#ifdef _DEBUG
                                                                                                IMP_SYMBOL(SSL_state_string_long)
                                                                                                    IMP_SYMBOL(SSL_alert_type_string_long)
                                                                                                        IMP_SYMBOL(SSL_alert_desc_string_long)
                                                                                                            IMP_SYMBOL(SSL_set_info_callback)
#endif
                                                                                                                {NULL, NULL}};

static TSymbolInfo SSLUtilSymbols[] = {
    IMP_SYMBOL(SSLeay_version)
#ifdef _DEBUG
        IMP_SYMBOL(X509_verify_cert_error_string)
#endif
            IMP_SYMBOL(X509_NAME_oneline)
                IMP_SYMBOL(i2d_X509)
                    IMP_SYMBOL(X509_free)
                        IMP_SYMBOL(PKCS7_new)
                            IMP_SYMBOL(PKCS7_SIGNED_new)
                                IMP_SYMBOL(OBJ_nid2obj)
                                    IMP_SYMBOL(ASN1_INTEGER_set)
                                        IMP_SYMBOL(PKCS7_free)
                                            IMP_SYMBOL(i2d_PKCS7)
                                                IMP_SYMBOL(sk_new_null)
    //   IMP_SYMBOL(BIO_s_mem)
    //   IMP_SYMBOL(BIO_new)
    //   IMP_SYMBOL(BIO_free)
    //   IMP_SYMBOL(BIO_read)
    //   IMP_SYMBOL(ERR_clear_error)
    IMP_SYMBOL(ERR_error_string)
    //   IMP_SYMBOL(ERR_print_errors)
    IMP_SYMBOL(ERR_get_error)
    //   IMP_SYMBOL(ERR_get_state)
    IMP_SYMBOL(ERR_remove_state)
        IMP_SYMBOL(ENGINE_cleanup)
            IMP_SYMBOL(CONF_modules_finish)
                IMP_SYMBOL(CONF_modules_free)
                    IMP_SYMBOL(CONF_modules_unload)
                        IMP_SYMBOL(ERR_free_strings)
                            IMP_SYMBOL(sk_free)
                                IMP_SYMBOL(EVP_cleanup)
                                    IMP_SYMBOL(CRYPTO_cleanup_all_ex_data)
                                        IMP_SYMBOL(CRYPTO_num_locks)
                                            IMP_SYMBOL(CRYPTO_set_locking_callback)
                                                IMP_SYMBOL(RAND_seed){NULL, NULL}};

static bool LoadSymbols(LPCTSTR libName, LPCTSTR altLibName, HINSTANCE& hLib, PSymbolInfo pSymbols, int logUID)
{
    hLib = LoadLibrary(libName);
    if (!hLib && altLibName)
    {
        hLib = LoadLibrary(altLibName);
        if (hLib)
            libName = altLibName;
    }
    if (hLib)
        SalamanderDebug->AddModuleWithPossibleMemoryLeaks(libName);
    char buf[MAX_PATH + 400];
    if (!hLib)
    {
        sprintf(buf, "Err %u: Unable to load %s\r\n", GetLastError(), libName);
        Logs.LogMessage(logUID, buf, -1);
        return false;
    }
    while (pSymbols->Addr)
    {
        *pSymbols->Addr = GetProcAddress(hLib, pSymbols->Name);
        if (!*pSymbols->Addr)
        {
            sprintf(buf, "Err %u: Unable to find %s in %s\r\n", GetLastError(), pSymbols->Name, libName);
            Logs.LogMessage(logUID, buf, -1);
            return false;
        }
        pSymbols++;
    }
    return true;
} /* LoadSymbols */

static void AddNewLine(char*& buf, int& maxlen)
{
    if (maxlen > 0 && buf[0])
    {
        int len = (int)_tcslen(buf);
        maxlen -= len;
        buf += len;
        if (maxlen && (buf[-1] != '\n') && (buf[-1] != '\r'))
        {
            _tcscpy(buf, _T("\n"));
            maxlen--;
            buf++;
        }
    }
} /* AddNewLine */

static bool CheckCertificate(BYTE* pCert, int certLen, LPTSTR buf, int maxlen, const char* host,
                             LPCWSTR hostW)
{
    CALL_STACK_MESSAGE5("CheckCertificate(0x%p, %d, %s, %ls)", pCert, certLen, host, hostW);
    CERT_CHAIN_PARA ChainPara;
    PCCERT_CHAIN_CONTEXT pChainContext = NULL;
    CERT_CHAIN_POLICY_PARA PolicyPara;
    CERT_CHAIN_POLICY_STATUS PolicyStatus;
    HTTPSPolicyCallbackData polHttps;
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, pCert, certLen);
    WCHAR PeerName[128];

    if (maxlen > 0)
        buf[0] = 0;
    if (!pCertContext)
    {
        lstrcpyn(buf, SalamanderGeneral->GetErrorText(GetLastError()), maxlen);
        return false;
    }

    bool checkRevocation = true;

CHECK_CERT_AGAIN:

    memset(&ChainPara, 0, sizeof(ChainPara));
    ChainPara.cbSize = sizeof(ChainPara);
    if (!CertGetCertificateChain(NULL,
                                 pCertContext,
                                 NULL,
                                 NULL,
                                 &ChainPara,
                                 (checkRevocation ? CERT_CHAIN_REVOCATION_CHECK_CHAIN : 0) | // Revocation checking is done on all of the certificates in every chain.
                                     CERT_CHAIN_CACHE_END_CERT |                             // When this flag is set, the end certificate is cached, which might speed up the chain-building process. By default, the end certificate is not cached, and it would need to be verified each time a chain is built for it.
                                     CERT_CHAIN_DISABLE_AUTH_ROOT_AUTO_UPDATE,               // Inhibits the auto update of third-party roots from the Windows Update Web Server
                                 NULL,
                                 &pChainContext))
    {
        lstrcpyn(buf, SalamanderGeneral->GetErrorText(GetLastError()), maxlen);
        CertFreeCertificateContext(pCertContext);
        return false;
    }
    memset(&polHttps, 0, sizeof(HTTPSPolicyCallbackData));
    polHttps.cbStruct = sizeof(HTTPSPolicyCallbackData);
    polHttps.dwAuthType = AUTHTYPE_SERVER;
    polHttps.fdwChecks = 0;
    if (host != NULL)
        MultiByteToWideChar(CP_ACP, 0, host, -1, PeerName, SizeOf(PeerName));
    else
    {
        if (hostW != NULL)
            lstrcpynW(PeerName, hostW, SizeOf(PeerName));
    }
    polHttps.pwszServerName = PeerName;

    memset(&PolicyPara, 0, sizeof(PolicyPara));
    PolicyPara.cbSize = sizeof(PolicyPara);
    PolicyPara.pvExtraPolicyPara = &polHttps;

    memset(&PolicyStatus, 0, sizeof(PolicyStatus));
    PolicyStatus.cbSize = sizeof(PolicyStatus);

    if (!CertVerifyCertificateChainPolicy(CERT_CHAIN_POLICY_SSL,
                                          pChainContext, &PolicyPara, &PolicyStatus))
    {
        lstrcpyn(buf, SalamanderGeneral->GetErrorText(GetLastError()), maxlen);
        CertFreeCertificateChain(pChainContext);
        CertFreeCertificateContext(pCertContext);
        return false;
    }

    bool ok = true;

    if (PolicyStatus.dwError)
    {
        if (checkRevocation && PolicyStatus.dwError == CRYPT_E_NO_REVOCATION_CHECK)
        { // The revocation function was unable to check revocation for the certificate. OK, so try it without checking revocation (MS probably also skips it because they accept the same certificate at the same time on the same machine).
            CertFreeCertificateChain(pChainContext);
            pChainContext = NULL;
            checkRevocation = false;
            goto CHECK_CERT_AGAIN;
        }
        else
        {
            ok = false;
            lstrcpyn(buf, SalamanderGeneral->GetErrorText(PolicyStatus.dwError), maxlen);
        }
    }
    int res = CertVerifyTimeValidity(NULL, pCertContext->pCertInfo);
    if (res != 0)
    {
        ok = false;
        AddNewLine(buf, maxlen);
        lstrcpyn(buf, LoadStr((res < 0) ? IDS_SSL_ERR_NOTYETVALID : IDS_SSL_ERR_EXPIRED), maxlen);
    }
    // Whatever flag is used, revokation check fails on most servers :-/
    /*int i;
  for (i = 0; i < pChainContext->cChain; i++)
  {
    CERT_REVOCATION_STATUS  revStat;

    revStat.cbSize = sizeof(CERT_REVOCATION_STATUS);
    if (!CertVerifyRevocation(X509_ASN_ENCODING,
                              CERT_CONTEXT_REVOCATION_TYPE,
                              1,
                              (void**)&pChainContext->rgpChain[i]->rgpElement[0]->pCertContext,
                              //CERT_VERIFY_CACHE_ONLY_BASED_REVOCATION/
                              CERT_VERIFY_REV_SERVER_OCSP_FLAG
                              //CERT_VERIFY_REV_CHAIN_FLAG,
                              NULL,
                              &revStat))
    {
      ok = false;
      AddNewLine(buf, maxlen);
      lstrcpyn(buf, SalamanderGeneral->GetErrorText(revStat.dwError), maxlen);
      break;
    }
  }*/

    CertFreeCertificateChain(pChainContext);
    CertFreeCertificateContext(pCertContext);
    return ok;
} /* CheckCertificate */

static bool ViewCertificate(HWND hParent, BYTE* pCertData, int CertDataLen, BYTE* pPKCS7Cert, int PKCS7CertLen, LPCWSTR pTitle)
{
    CALL_STACK_MESSAGE5("ViewCertificate(0x%p, 0x%p, %d, %ls)", hParent, pCertData, CertDataLen, pTitle);
    CRYPTUI_VIEWCERTIFICATE_STRUCTW cvi;
    PCCERT_CONTEXT pCertContext = CertCreateCertificateContext(X509_ASN_ENCODING | PKCS_7_ASN_ENCODING, pCertData, CertDataLen);
    if (!pCertContext)
    {
        const char* errStr = SalamanderGeneral->GetErrorText(GetLastError());
        SalamanderGeneral->SalMessageBox(hParent, errStr, LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONSTOP);
        return false;
    }

    // Put all other certificates provided by the server, possibly untrusted, to hCertStore
    CRYPT_INTEGER_BLOB blob = {(DWORD)PKCS7CertLen, pPKCS7Cert};
    HCERTSTORE hCertStore = CertOpenStore(CERT_STORE_PROV_PKCS7, PKCS_7_ASN_ENCODING, NULL, 0, &blob);

    memset(&cvi, 0, sizeof(cvi));
    cvi.dwSize = sizeof(cvi);
    cvi.hwndParent = hParent;
    cvi.dwFlags = CRYPTUI_DISABLE_EDITPROPERTIES;
    cvi.szTitle = pTitle;
    cvi.pCertContext = pCertContext;
    if (hCertStore)
    {
        cvi.cStores = 1;
        cvi.rghStores = &hCertStore;
    }

    if (!CryptUIDlgViewCertificateW(&cvi, NULL))
    {
        DWORD err = GetLastError();
        if (ERROR_CANCELLED != err)
        {
            const char* errStr = SalamanderGeneral->GetErrorText(err);
            CertFreeCertificateContext(pCertContext);
            if (hCertStore)
                CertCloseStore(hCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
            SalamanderGeneral->SalMessageBox(hParent, errStr, LoadStr(IDS_FTPPLUGINTITLE), MB_OK | MB_ICONSTOP);
            return false;
        }
    }
    if (hCertStore)
        CertCloseStore(hCertStore, CERT_CLOSE_STORE_FORCE_FLAG);
    CertFreeCertificateContext(pCertContext);
    return true;
} /* ViewCertificate */

#ifdef _DEBUG
static void InfoCallback(const SSL* s, int where, int ret)
{
    const char* str;
    int w;
    char buf[512];

    w = where & ~SSL_ST_MASK;
    buf[0] = 0;

    if (w & SSL_ST_CONNECT)
        str = "SSL_connect";
    else if (w & SSL_ST_ACCEPT)
        str = "SSL_accept";
    else
        str = "undefined";

    if (where & SSL_CB_LOOP)
    {
        sprintf(buf, "%s:%s\n", str, SSLLib.SSL_state_string_long(s));
    }
    else if (where & SSL_CB_ALERT)
    {
        str = (where & SSL_CB_READ) ? "read" : "write";
        sprintf(buf, "SSL3 alert %s:%s:%s\n", str,
                SSLLib.SSL_alert_type_string_long(ret),
                SSLLib.SSL_alert_desc_string_long(ret));
    }
    else if (where & SSL_CB_EXIT)
    {
        if (ret == 0)
        {
            sprintf(buf, "%s:failed in %s\n", str, SSLLib.SSL_state_string_long(s));
        }
        else if (ret < 0)
        {
            sprintf(buf, "%s:error %d in %s\n", str, ret, SSLLib.SSL_state_string_long(s));
        }
    }
    if (buf[0])
    {
        OutputDebugString(buf);
        TRACE_I(buf);
    }
}
#endif

void WriteSSLErrorStackToLog(int logUID, const char* errSrc)
{
    // log OpenSSL error stack
    int err2;
    char buffer[256]; // pisou: at least 120 bytes
    while ((err2 = SSLLib.ERR_get_error()) != 0)
    {
        SSLLib.ERR_error_string(err2, buffer);
        Logs.LogMessage(logUID, "SSL ERROR: ", -1);
        Logs.LogMessage(logUID, errSrc, -1);
        Logs.LogMessage(logUID, ": ", -1);
        Logs.LogMessage(logUID, buffer, -1);
        Logs.LogMessage(logUID, "\r\n", -1);
    }
}

BOOL CSocket::EncryptSocket(int logUID, int* sslErrorOccured, CCertificate** unverifiedCert,
                            int* errorID, char* errorBuf, int errorBufLen, CSocket* conForReuse)
{
    int err;
    SSL* Conn;
    char buffer[256], name[200];

    CALL_STACK_MESSAGE3("CSocket::EncryptSocket(%d, , , , , , 0x%p)", logUID, conForReuse);
    if (errorID != NULL)
        *errorID = -1;
    if (errorBufLen > 0)
        errorBuf[0] = 0;
    if (unverifiedCert != NULL)
        *unverifiedCert = NULL;
    if (sslErrorOccured != NULL)
        *sslErrorOccured = SSLConn != NULL ? SSLCONERR_NOERROR : SSLCONERR_DONOTRETRY;
    if (SSLConn != NULL)
        return TRUE; // socket has already been encrypted
    if (!bSSLInited)
        return FALSE;
    WriteSSLErrorStackToLog(logUID, "unknown source");
    Conn = SSLLib.SSL_new(SSLLib.Ctx);
    if (Conn)
    {
        HWND hWnd = SocketsThread->GetHiddenWindow();
        u_long argp = 0;

        WSAAsyncSelect(Socket, hWnd, 0, 0);
        err = ioctlsocket(Socket, FIONBIO, &argp);
        // Socket je pod x64 64-bit hodnota, ale lidi kolem OpenSSL se domnivaji, ze nikdy neprekroci 2^32
        // viz http://comments.gmane.org/gmane.comp.encryption.openssl.devel/13621
        // http://msdn.microsoft.com/en-us/library/ms724485%28VS.85%29.aspx
        // az to nastane a tady nam zacne podminka padat, treba jiz bude existoval x64 verze SSL
        if (Socket > 0x00000000ffffffff)
        {
            DWORD* crash = NULL;
            *crash = 0;
        }
        if (!SSLLib.SSL_set_fd(Conn, (int)Socket))
            WriteSSLErrorStackToLog(logUID, "SSL_set_fd");
        SSLLib.SSL_set_verify(Conn, 0, NULL);

#ifdef _DEBUG
        SSLLib.SSL_set_info_callback(Conn, InfoCallback);
        Logs.LogMessage(logUID, "SSL DEBUG INFO: See Trace Server for messages from information callback for this SSL connection.\r\n", -1);
#endif

        BOOL testReuseSSLSession = FALSE;
        if (conForReuse != NULL && conForReuse->SSLConn != NULL && conForReuse->ReuseSSLSession != 2 /* ne */)
        {
            SSL_SESSION* ssl_sessionid = SSLLib.SSL_get1_session(conForReuse->SSLConn); // sessionid.addref()
            if (ssl_sessionid == NULL)
                Logs.LogMessage(logUID, "SSL ERROR: SSL_get1_session returns NULL!\r\n", -1);
            else
            {
                if (!SSLLib.SSL_set_session(Conn, ssl_sessionid))
                    WriteSSLErrorStackToLog(logUID, "SSL_set_session");
                else
                    testReuseSSLSession = TRUE;
                SSLLib.SSL_SESSION_free(ssl_sessionid); // sessionid.release()
            }
        }

        TRACE_I("SSL_connect: begin");
        {
            CALL_STACK_MESSAGE1("CSocket::EncryptSocket::SSL_connect()");
            err = SSLLib.SSL_connect(Conn);
        }
        TRACE_I("SSL_connect: end");

        if (err > 0)
        {
            if (testReuseSSLSession)
            {
                if (SSLLib.SSL_session_reused(Conn))
                {
                    Logs.LogMessage(logUID, "SSL INFO: SSL session reused for data-connection\r\n", -1);
                    if (conForReuse->ReuseSSLSession == 0 /* zkusit */)
                        conForReuse->ReuseSSLSession = 1 /* ano */;
                }
                else // SSL session not reused
                {
                    if (conForReuse->ReuseSSLSession == 0 /* zkusit */) // do not try again for future data-connections (or it will fail)
                    {
                        Logs.LogMessage(logUID, "SSL INFO: SSL session was NOT reused, will not try for future data-connections...\r\n", -1);
                        conForReuse->ReuseSSLSession = 2 /* ne */;
                    }
                    else // try for all future data-cons to set ReuseSSLSessionFailed to TRUE and so reconnect ctrl-con (except if this is keep-alive data-con)
                    {
                        Logs.LogMessage(logUID, "SSL INFO: SSL session was NOT reused, it has expired in server session cache, reconnect of control connection is needed...\r\n", -1);
                        conForReuse->ReuseSSLSessionFailed = TRUE; // aby se data-connectiona otevrela je nejspis nutny reuse, ale ten hlasi chybu: jedine reseni je reconnect control-connectiony
                    }
                }
            }

            void* ssl_cipher;
            char* ssl_version;
            char* cipher_name;
            int ssl_bits;
            X509* peerCert;
            STACK_OF(X509) * certStack;
            BYTE *DERCert, *PKCS7Cert = NULL, *tmp;
            int DERCertLen, PKCS7CertLen;
            int verRes = SSLLib.SSL_get_verify_result(Conn);

            wsprintf(buffer, LoadStr(IDS_SSL_LOG_OSSL_CERT_VERIFY), verRes);
            Logs.LogMessage(logUID, buffer, -1);
#ifdef _DEBUG
            const char* str = SSLLib.X509_verify_cert_error_string(verRes);
#endif

            peerCert = SSLLib.SSL_get_peer_certificate(Conn);
            SSLLib.X509_NAME_oneline(peerCert->cert_info->subject, name, sizeof(name));
            wsprintf(buffer, LoadStr(IDS_SSL_LOG_SUBJECT), name);
            Logs.LogMessage(logUID, buffer, -1);
            SSLLib.X509_NAME_oneline(peerCert->cert_info->issuer, name, sizeof(name));
            wsprintf(buffer, LoadStr(IDS_SSL_LOG_ISSUER), name);
            Logs.LogMessage(logUID, buffer, -1);

            // Obtain entire certificate chain upto root certificate
            certStack = SSLLib.SSL_get_peer_cert_chain(Conn);
            if (certStack)
            {
                PKCS7* p7 = SSLLib.PKCS7_new();
                if (p7)
                {
                    PKCS7_SIGNED* p7s = SSLLib.PKCS7_SIGNED_new();
                    if (p7s)
                    {
                        p7->type = SSLLib.OBJ_nid2obj(NID_pkcs7_signed);
                        p7->d.sign = p7s;
                        p7s->contents->type = SSLLib.OBJ_nid2obj(NID_pkcs7_data);
                        SSLLib.ASN1_INTEGER_set(p7s->version, 1);
                        p7s->cert = certStack;
                        p7s->crl = SSLLib.sk_new_null();
                        PKCS7CertLen = SSLLib.i2d_PKCS7(p7, NULL);
                        tmp = PKCS7Cert = (BYTE*)malloc(PKCS7CertLen);
                        SSLLib.i2d_PKCS7(p7, &tmp);
                    }
                }
                p7->d.sign->cert = NULL; // Avoid freeing it
                SSLLib.PKCS7_free(p7);
            }

            DERCertLen = SSLLib.i2d_X509(peerCert, NULL);
            tmp = DERCert = (BYTE*)malloc(DERCertLen);
            SSLLib.i2d_X509(peerCert, &tmp);
            SSLLib.X509_free(peerCert);

            ssl_version = SSLLib.SSL_CIPHER_get_version(SSLLib.SSL_get_current_cipher(Conn));
            ssl_cipher = SSLLib.SSL_get_current_cipher(Conn);
            SSLLib.SSL_CIPHER_get_bits(ssl_cipher, &ssl_bits);
            cipher_name = SSLLib.SSL_CIPHER_get_name(ssl_cipher);
            wsprintf(buffer, LoadStr(IDS_SSL_LOG_ALGO), ssl_version, cipher_name, ssl_bits);
            Logs.LogMessage(logUID, buffer, -1);

            BOOL certAcceptedOrVerified = FALSE;
            if (pCertificate)
            {
                if (pCertificate->IsSame(DERCert, DERCertLen, PKCS7Cert, PKCS7CertLen))
                {
                    Logs.LogMessage(logUID, LoadStr(pCertificate->IsVerified() ? IDS_SSL_LOG_CERTVERIFIED : IDS_SSL_LOG_CERTACCEPTED), -1, TRUE);
                    certAcceptedOrVerified = TRUE;
                }
                else // Huh! The certificate has changed?????
                {
                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTCHANGED), -1, TRUE);
                    pCertificate->Release();
                    pCertificate = NULL;
                }
            }
            if (!certAcceptedOrVerified)
            {
                if (CheckCertificate(DERCert, DERCertLen, errorBuf, errorBufLen, HostAddress, NULL))
                {
                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTVERIFIED), -1, TRUE);
                    pCertificate = new CCertificate(DERCert, DERCertLen, PKCS7Cert, PKCS7CertLen, true, HostAddress);
                    certAcceptedOrVerified = TRUE; // Passed
                }
                else
                    Logs.LogMessage(logUID, LoadStr(IDS_SSL_LOG_CERTNOTVERIFIED), -1, TRUE);
            }
            if (!certAcceptedOrVerified)
            {
                // The certificate was not verified nor previously accepted by user, so user should accept
                // it before further using of this socket.
                if (unverifiedCert != NULL)
                    *unverifiedCert = new CCertificate(DERCert, DERCertLen, PKCS7Cert, PKCS7CertLen, false, HostAddress);
                else
                {
                    SSLLib.SSL_shutdown(Conn);
                    SSLLib.SSL_free(Conn);
                    if (PKCS7Cert)
                        free(PKCS7Cert);
                    free(DERCert);
                    if (sslErrorOccured != NULL)
                        *sslErrorOccured = SSLCONERR_UNVERIFIEDCERT; // The certificate was not verified nor previously accepted by user.
                    return FALSE;
                }
            }
            if (PKCS7Cert)
                free(PKCS7Cert);
            free(DERCert);
            WSAAsyncSelect(Socket, hWnd, Msg, FD_READ | FD_CLOSE | FD_WRITE);
            SSLConn = Conn;
            if (sslErrorOccured != NULL)
                *sslErrorOccured = SSLCONERR_NOERROR; // But the certificate must not be verified nor previously accepted by user.
            return TRUE;
        }
        else
        {
            /*      ERR_STATE *es = SSLLib.ERR_get_state();
      fd_set  fs;
      timeval tv = {0,10};
      FD_ZERO(&fs);
      FD_SET(Socket, &fs);
      select(1, NULL, NULL, &fs, &tv);*/
            err = SSLLib.SSL_get_error(Conn, err);
            //      err = SSLLib.ERR_get_error();
            //      err = GetLastError();

            sprintf(buffer, LoadStr(IDS_SSL_ERR_CONNECT_LOG), err, SSLLib.ERR_error_string(err, name));
            Logs.LogMessage(logUID, buffer, -1, TRUE);
            WriteSSLErrorStackToLog(logUID, "SSL_connect");

            if (errorID != NULL)
                *errorID = IDS_SSL_ERR_CONNECT;
            if (errorBufLen > 0)
                _snprintf_s(errorBuf, errorBufLen, _TRUNCATE, LoadStr(IDS_SSL_ERR_CONNECT_ERR), err, name);
            SSLLib.SSL_free(Conn);
            if (sslErrorOccured != NULL)
                *sslErrorOccured = SSLCONERR_CANRETRY;
        }
    }
    else
    {
        Logs.LogMessage(logUID, LoadStr(IDS_SSL_ERR_NEW_LOG), -1, TRUE);
        if (errorID != NULL)
            *errorID = IDS_SSL_ERR_NEW;
        WriteSSLErrorStackToLog(logUID, "SSL_new");
    }
    return FALSE;
}

void FreeSSL(int loadStatus)
{
    if (bSSLInited || loadStatus != 0)
    {
        if (SSLLib.Locks)
        {
            SSLLib.CRYPTO_set_locking_callback(NULL);
            for (int i = 0; i < SSLLib.CRYPTO_num_locks(); i++)
                if (SSLLib.Locks[i])
                    CloseHandle(SSLLib.Locks[i]);
            free(SSLLib.Locks);
        }
        if (SSLLib.Ctx)
            SSLLib.SSL_CTX_free(SSLLib.Ctx);

        if (loadStatus == 0 || loadStatus == 2)
        {
            // Petr: OpenSSL nechavalo furu memory leaku, proto jsem pridal tento blok

            // thread-local cleanup
            SSLLib.ERR_remove_state(0);

            // thread-safe cleanup
            SSLLib.ENGINE_cleanup();
            SSLLib.CONF_modules_finish();
            SSLLib.CONF_modules_free();
            SSLLib.CONF_modules_unload(1);

            // global application exit cleanup (after all SSL activity is shutdown)
            SSLLib.ERR_free_strings();
            SSLLib.EVP_cleanup();
            SSLLib.CRYPTO_cleanup_all_ex_data();

            // stack s compression metodama asi nejde "legalne" uvolnit, tak se to resi rucne
            STACK_OF(SSL_COMP)* comp_sk = SSLLib.SSL_COMP_get_compression_methods();
            SSLLib.sk_free(CHECKED_STACK_OF(SSL_COMP, comp_sk));

            // Petr: konec bloku
        }

        if (SSLLib.hSSLLib)
            FreeLibrary(SSLLib.hSSLLib);
        if (SSLLib.hSSLUtilLib)
            FreeLibrary(SSLLib.hSSLUtilLib);
        bSSLInited = false;
    }
}

void SSLThreadLocalCleanup()
{
    if (bSSLInited)
        SSLLib.ERR_remove_state(0);
}

static void LockingCallback(int mode, int type, const char* file, int line)
{
    if (mode & CRYPTO_LOCK)
    {
        WaitForSingleObject(SSLLib.Locks[type], INFINITE);
    }
    else
    {
        ReleaseMutex(SSLLib.Locks[type]);
    }
}

bool InitSSL(int logUID, int* errorID)
{
    if (errorID != NULL)
        *errorID = -1;
    if (bSSLInited)
        return true;

    CALL_STACK_MESSAGE2("InitSSL(%d,)", logUID);

    memset(&SSLLib, 0, sizeof(SSLLib));

    bool ret = false;
    char dir[MAX_PATH];
    int loadStatus = 1;
    if (GetModuleFileName(NULL, dir, _countof(dir)) &&
        SalamanderGeneral->CutDirectory(dir) &&
        SalamanderGeneral->SalPathAppend(dir, "utils", _countof(dir)))
    {
        ret = true;
        char* s = dir + strlen(dir);
        if (!SalamanderGeneral->SalPathAppend(dir, "libeay32.dll", _countof(dir)) ||
            !LoadSymbols(dir, NULL, SSLLib.hSSLUtilLib, SSLUtilSymbols, logUID))
        {
            ret = false;
        }
        *s = 0;
        if (!ret ||
            !SalamanderGeneral->SalPathAppend(dir, "ssleay32.dll", _countof(dir)) ||
            !LoadSymbols(dir, NULL /*_T("libssl32.dll")*/, SSLLib.hSSLLib, SSLSymbols, logUID))
        {
            ret = false;
        }
    }

    if (ret)
    {
        loadStatus = 2;
        sprintf(dir, "SSL INFO: Version: %s \r\nSSL INFO: Compile flags: ", SSLLib.SSLeay_version(SSLEAY_VERSION));
        Logs.LogMessage(logUID, dir, -1);
        Logs.LogMessage(logUID, SSLLib.SSLeay_version(SSLEAY_CFLAGS), -1);
        Logs.LogMessage(logUID, "\r\n", -1);
        // NOTE: There are no unload counterparts for SSL_library_init & SSL_load_error_strings
        SSLLib.SSL_library_init();
        SSLLib.SSL_load_error_strings();
        DWORD seed = GetTickCount();
        SSLLib.RAND_seed(&seed, sizeof(seed));
        int locksCount = SSLLib.CRYPTO_num_locks();
        SSLLib.Locks = (HANDLE*)malloc(locksCount * sizeof(HANDLE));
        if (SSLLib.Locks)
        {
            for (int i = 0; i < locksCount; i++)
            {
                SSLLib.Locks[i] = !ret ? NULL : CreateMutex(NULL, FALSE, NULL);
                if (ret && SSLLib.Locks[i] == NULL)
                    ret = false;
            }
        }
        else
            ret = false;
        if (!ret)
        {
            sprintf(dir, "SSL Err: Unable to alloc %d locks\r\n", locksCount);
            Logs.LogMessage(logUID, dir, -1);
        }

        if (ret)
        {
            SSLLib.CRYPTO_set_locking_callback(/*(void (*)(int,int,const char *,int))*/ LockingCallback);

            // NOTE: the pointer returned SSLv23_client_method is not to be freed
            //
            // SSLv23_client_method() is default method used in OpenSLL.exe and CURL.
            // Unsafe SSL2 protocol is disabled using OPENSSL_NO_SSL2 define.
            // SSLv3_client_method() didn't work with wedos server: https://forum.altap.cz/viewtopic.php?f=2&t=6667
            //    SSLLib.Meth = SSLLib.SSLv3_client_method();
            SSLLib.Meth = SSLLib.SSLv23_client_method();
            if (SSLLib.Meth)
            {
                SSLLib.Ctx = SSLLib.SSL_CTX_new(SSLLib.Meth);
                if (SSLLib.Ctx)
                {
                    /* also switch on all the interoperability and bug
           * workarounds so that we will communicate with people
           * that cannot read poorly written specs :-)
           */
                    SSLLib.SSL_CTX_ctrl(SSLLib.Ctx, SSL_CTRL_OPTIONS, SSL_OP_ALL, NULL);
                    bSSLInited = true;
                    return true;
                }
            } // if Meth <> NULL then
        }
    }
    FreeSSL(loadStatus);
    memset(&SSLLib, 0, sizeof(SSLLib)); // clean up, everything is freed
    if (errorID != NULL)
        *errorID = IDS_SSL_ERR_OPENSSLNOTFOUND;
    Logs.LogMessage(logUID, LoadStr(IDS_SSL_ERR_OPENSSLNOTFOUND), -1);
    Logs.LogMessage(logUID, "\r\n", -1);
    return false;
} /* InitSSL */

int SSLtoWS2Error(int err)
{
    switch (err)
    {
    case SSL_ERROR_WANT_READ:
        return WSAEWOULDBLOCK;
    case SSL_ERROR_WANT_WRITE:
        return WSAEWOULDBLOCK;
        //    case SSL_ERROR_SYSCALL:     return WSAECONNCLOSED;
        //    case SSL_ERROR_ZERO_RETURN: return WSAECONNCLOSED;
    default:
        return err;
    }
}

//////////////////////// CCertificateErrDialog ////////////////////////
CCertificateErrDialog::CCertificateErrDialog(HWND hParent, const char* errorStr)
    : CCenteredDialog(HLanguage, IDD_CERTIFICATE, hParent), ErrorStr(errorStr)
{
}

INT_PTR CCertificateErrDialog::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCertificateErrDialog::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
        SetDlgItemText(HWindow, IDT_CERTIFICATE_ERROR, ErrorStr);
        break;
    case WM_COMMAND:
        switch (LOWORD(wParam))
        {
        case IDB_CERTIFICATE_VIEW:
            if (HIWORD(wParam) == BN_CLICKED)
            {
                EndDialog(HWindow, IDB_CERTIFICATE_VIEW);
            }
            break;
        }
    }
    return CCenteredDialog::DialogProc(uMsg, wParam, lParam);
}

//////////////////////// CCertificate ////////////////////////
CCertificate::CCertificate(BYTE* pDERCert, int DERCertLen, BYTE* pPKCS7Cert, int PKCS7CertLen, bool bValid, LPCSTR host)
{
    CALL_STACK_MESSAGE7("CCertificate::ctor(0x%p, %d, 0x%p, %d, %d, %s)", pDERCert, DERCertLen, pPKCS7Cert, PKCS7CertLen, bValid, host);
    bVerified = bValid;
    pDERData = (BYTE*)malloc(DERCertLen);
    if (pDERData)
    {
        nDERDataLen = DERCertLen;
        memcpy(pDERData, pDERCert, nDERDataLen);
    }
    else
    {
        nDERDataLen = 0;
    }
    pPKCS7Data = (BYTE*)malloc(PKCS7CertLen);
    if (pPKCS7Data)
    {
        nPKCS7DataLen = PKCS7CertLen;
        memcpy(pPKCS7Data, pPKCS7Cert, nPKCS7DataLen);
    }
    else
    {
        nPKCS7DataLen = 0;
    }
    if (host)
    {
        Host = (LPWSTR)malloc(sizeof(WCHAR) * (strlen(host) + 1));
        if (Host)
        {
            LPWSTR out = Host;
            while (*host)
                *out++ = *host++;
            *out = 0;
        }
    }
    else
    {
        Host = NULL;
    }
    nRefCount = 1;
}

CCertificate::~CCertificate()
{
    if (Host)
        free(Host);
    if (pDERData)
        free(pDERData);
    if (pPKCS7Data)
        free(pPKCS7Data);
}

LONG CCertificate::AddRef()
{
    return InterlockedIncrement(&nRefCount);
}

LONG CCertificate::Release()
{
    LONG ret = InterlockedDecrement(&nRefCount);

    if (!ret)
        delete this;
    return ret;
}

void CCertificate::ShowCertificate(HWND hParent)
{
    ViewCertificate(hParent, pDERData, nDERDataLen, pPKCS7Data, nPKCS7DataLen, Host);
}

bool CCertificate::CheckCertificate(LPTSTR buf, int maxlen)
{
    return ::CheckCertificate(pDERData, nDERDataLen, buf, maxlen, NULL, Host);
}

bool CCertificate::IsSame(BYTE* pDERCert, int DERCertLen, BYTE* pPKCS7Cert, int PKCS7CertLen)
{
    if (DERCertLen != nDERDataLen)
        return false;
    if (PKCS7CertLen != nPKCS7DataLen)
        return false;
    if (memcmp(pDERData, pDERCert, nDERDataLen))
        return false;
    return memcmp(pPKCS7Data, pPKCS7Cert, nPKCS7DataLen) ? false : true;
}
