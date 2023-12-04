// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// SSL 2.0 has security flaws and is disabled in current Firefox, Windows, etc.
#define OPENSSL_NO_SSL2

#include <openssl/x509.h>
#ifndef __in
#define __in
#define __out_ecount(x)
#endif

//#include <CryptDlg.h>
#include <cryptuiapi.h>

#define SSLCONERR_NOERROR 0        // no error (also when SSL is not used)
#define SSLCONERR_CANRETRY 1       // unable to encrypt connection + retrying can help
#define SSLCONERR_DONOTRETRY 2     // unable to encrypt connection + it has no sense to retry
#define SSLCONERR_UNVERIFIEDCERT 3 // server's certificate was not verified nor previously accepted by user

class CCertificate
{
public:
    CCertificate(BYTE* pDERCert, int DERCertLen, BYTE* pPKCS7Cert, int PKCS7CertLen, bool bValid, LPCSTR host);
    LONG AddRef();
    LONG Release();
    void ShowCertificate(HWND hParent);
    bool CheckCertificate(LPTSTR buf, int maxlen);

    // POZOR: metoda meni data certifikatu, volajici si musi zajistit, ze se data nepouzivaji
    //        zaroven v jinem threadu (idealne volat dokud je tohle jediny odkaz na objekt)
    void SetVerified(bool verified) { bVerified = verified; };

    bool IsSame(BYTE* pDERCert, int DERCertLen, BYTE* pPKCS7Cert, int PKCS7CertLen);
    bool IsVerified() { return bVerified; };
    LPCWSTR GetHostName() { return Host; };

private:
    ~CCertificate();

    LONG nRefCount;
    BYTE *pDERData, *pPKCS7Data;
    int nDERDataLen, nPKCS7DataLen;
    bool bVerified; // false when accepted once, true if verified and valid
    LPWSTR Host;
};

class CCertificateErrDialog : public CCenteredDialog
{
protected:
    const char* ErrorStr;

public:
    CCertificateErrDialog(HWND hParent, const char* errorStr);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

#define SSL_CTRL_GET_SESSION_REUSED 8

#define SSL_session_reused(ssl) \
    SSL_ctrl((ssl), SSL_CTRL_GET_SESSION_REUSED, 0, NULL)

struct SSL_SESSION
{
}; // Petr: nepotrebujeme znat obsah struktury, pracuje se s ni vyhradne uvnitr OpenSSL

typedef int (*TSSL_get_error)(SSL* SSL, int ret);
typedef int (*TSSL_write)(SSL* SSL, const void* data, int Size);
typedef int (*TSSL_read)(SSL* SSL, void* data, int Size);
typedef int (*TSSL_library_init)();
typedef void (*TSSL_load_error_strings)();
//typedef void *(*TSSLv3_client_method)();
typedef void* (*TSSLv23_client_method)();
typedef SSL_CTX* (*TSSL_CTX_new)(void* SSL_METHOD);
typedef void (*TSSL_CTX_free)(void* SSL_CTX);
typedef int (*TSSL_CTX_ctrl)(void* SSL_CTX, int Cmd, int larg, void* parg);
typedef SSL* (*TSSL_new)(void* SSL_CTX);
typedef void (*TSSL_free)(void* SSL_CTX);
typedef int (*TSSL_set_fd)(SSL* SSL, int fd);
typedef int (*TSSL_connect)(SSL* SSL);
typedef int (*TSSL_shutdown)(SSL* SSL);
typedef void* (*TSSL_get_current_cipher)(SSL* SSL);
typedef char* (*TSSL_CIPHER_get_version)(void* SSL_CIPHER);
typedef int (*TSSL_CIPHER_get_bits)(void* SSL_CIPHER, int* alg_bits);
typedef char* (*TSSL_CIPHER_get_name)(void* SSL_CIPHER);
typedef long (*TSSL_get_verify_result)(void* SSL);
typedef void (*TSSL_set_verify)(SSL* SSL, int mode, void* Callback);
typedef char* (*TSSLeay_version)(int infoType);
typedef int (*TSSL_pending)(SSL* SSL);
typedef const char* (*TX509_verify_cert_error_string)(long n);
typedef X509* (*TSSL_get_peer_certificate)(const SSL* SSL);
typedef STACK_OF(X509) * (*TSSL_get_peer_cert_chain)(const SSL* s);
typedef STACK_OF(SSL_COMP) * (*TSSL_COMP_get_compression_methods)(void);
typedef SSL_SESSION* (*TSSL_get1_session)(SSL* ssl); /* obtain a reference count */
typedef void (*TSSL_SESSION_free)(SSL_SESSION* ses);
typedef long (*TSSL_ctrl)(SSL* ssl, int cmd, long larg, void* parg);
typedef int (*TSSL_set_session)(SSL* to, SSL_SESSION* session);
typedef const char* (*TSSL_state_string_long)(const SSL* s);
typedef const char* (*TSSL_alert_type_string_long)(int value);
typedef const char* (*TSSL_alert_desc_string_long)(int value);
typedef void (*TSSL_set_info_callback)(SSL* ctx, void (*cb)(const SSL* ssl, int type, int val));

//typedef X509_NAME *(*TX509_get_subject_name)(X509 *a); // rets a->cert_info->subject
//typedef X509_NAME *(*TX509_get_issuer_name)(X509 *a); // rets a->cert_info->issuer
typedef int (*Ti2d_X509)(X509* a, BYTE** buf);
typedef char* (*TX509_NAME_oneline)(X509_NAME* a, char* buf, int size);
//typedef BIO  *(*TBIO_s_mem)();
//typedef BIO  *(*TBIO_new)(void *type);
//typedef int   (*TBIO_free)(BIO *a);
//typedef int   (*TBIO_read)(BIO *b, void *data, int len);
typedef void (*TX509_free)(X509* cert);
typedef char* (*TERR_error_string)(unsigned long e, char* buf);
//typedef void  (*TERR_print_errors)(void *bio);
typedef unsigned long (*TERR_get_error)(void);
//typedef ERR_STATE *(*TERR_get_state)(void);

typedef void (*TERR_remove_state)(unsigned long pid);
typedef void (*TENGINE_cleanup)(void);
typedef void (*TCONF_modules_finish)(void);
typedef void (*TCONF_modules_free)(void);
typedef void (*TCONF_modules_unload)(int all);
typedef void (*TERR_free_strings)(void);
typedef void (*Tsk_free)(_STACK*);
typedef void (*TEVP_cleanup)(void);
typedef void (*TCRYPTO_cleanup_all_ex_data)(void);

typedef int (*TCRYPTO_num_locks)(void);
typedef void (*TCRYPTO_set_locking_callback)(void (*func)(int mode, int type, const char* file, int line));
typedef void (*TRAND_seed)(const void* buf, int num);

//typedef void  (*TERR_clear_error)();

typedef PKCS7* (*TPKCS7_new)(void);
typedef PKCS7_SIGNED* (*TPKCS7_SIGNED_new)();
typedef ASN1_OBJECT* (*TOBJ_nid2obj)(int n);
typedef int (*TASN1_INTEGER_set)(ASN1_INTEGER* a, long v);
typedef void (*TPKCS7_free)(PKCS7* p7);
typedef int (*Ti2d_PKCS7)(PKCS7* p7, BYTE** buf);
typedef stack_st_X509_CRL* (*Tsk_new_null)();

typedef struct sSSLLib
{
    HINSTANCE hSSLLib;
    HINSTANCE hSSLUtilLib;

    TSSL_get_error SSL_get_error;
    TSSL_write SSL_write;
    TSSL_read SSL_read;
    TSSL_library_init SSL_library_init;
    TSSL_load_error_strings SSL_load_error_strings;
    // TSSLv3_client_method     SSLv3_client_method;
    TSSLv23_client_method SSLv23_client_method;
    TSSL_CTX_new SSL_CTX_new;
    TSSL_CTX_free SSL_CTX_free;
    TSSL_CTX_ctrl SSL_CTX_ctrl;
    TSSL_new SSL_new;
    TSSL_free SSL_free;
    TSSL_set_fd SSL_set_fd;
    TSSL_connect SSL_connect;
    TSSL_shutdown SSL_shutdown;
    TSSL_get_current_cipher SSL_get_current_cipher;
    TSSL_CIPHER_get_version SSL_CIPHER_get_version;
    TSSL_CIPHER_get_bits SSL_CIPHER_get_bits;
    TSSL_CIPHER_get_name SSL_CIPHER_get_name;
    TSSL_get_verify_result SSL_get_verify_result;
    TSSL_set_verify SSL_set_verify;
    TSSLeay_version SSLeay_version;
    TSSL_pending SSL_pending;
    TSSL_get_peer_certificate SSL_get_peer_certificate;
    TSSL_get_peer_cert_chain SSL_get_peer_cert_chain;
    TSSL_COMP_get_compression_methods SSL_COMP_get_compression_methods;
    TSSL_get1_session SSL_get1_session;
    TSSL_set_session SSL_set_session;
    TSSL_SESSION_free SSL_SESSION_free;
    TSSL_ctrl SSL_ctrl;
#ifdef _DEBUG
    TSSL_state_string_long SSL_state_string_long;
    TSSL_alert_type_string_long SSL_alert_type_string_long;
    TSSL_alert_desc_string_long SSL_alert_desc_string_long;
    TSSL_set_info_callback SSL_set_info_callback;
    TX509_verify_cert_error_string X509_verify_cert_error_string;
#endif
    TX509_NAME_oneline X509_NAME_oneline;
    TX509_free X509_free;
    Ti2d_X509 i2d_X509;

    TPKCS7_new PKCS7_new;
    TPKCS7_SIGNED_new PKCS7_SIGNED_new;
    TOBJ_nid2obj OBJ_nid2obj;
    TASN1_INTEGER_set ASN1_INTEGER_set;
    TPKCS7_free PKCS7_free;
    Ti2d_PKCS7 i2d_PKCS7;
    Tsk_new_null sk_new_null;

    // TBIO_s_mem               BIO_s_mem;
    // TBIO_new                 BIO_new;
    // TBIO_free                BIO_free;
    //TBIO_read                BIO_read;
    // TERR_clear_error         ERR_clear_error;
    TERR_error_string ERR_error_string;
    // TERR_print_errors        ERR_print_errors;
    TERR_get_error ERR_get_error;
    // TERR_get_state           ERR_get_state;

    TERR_remove_state ERR_remove_state;
    TENGINE_cleanup ENGINE_cleanup;
    TCONF_modules_finish CONF_modules_finish;
    TCONF_modules_free CONF_modules_free;
    TCONF_modules_unload CONF_modules_unload;
    TERR_free_strings ERR_free_strings;
    Tsk_free sk_free;
    TEVP_cleanup EVP_cleanup;
    TCRYPTO_cleanup_all_ex_data CRYPTO_cleanup_all_ex_data;

    TCRYPTO_num_locks CRYPTO_num_locks;
    TCRYPTO_set_locking_callback CRYPTO_set_locking_callback;
    TRAND_seed RAND_seed;

    void* Meth;
    SSL_CTX* Ctx;
    HANDLE* Locks;
} sSSLLib;

extern sSSLLib SSLLib;

bool InitSSL(int logUID, int* errorID);
// loadStatus: 0 = lib was fully initialized,
//             1 = only DLLs may be loaded (probably not both DLLs are loaded),
//             2 = DLLs were loaded + lib init called (SSL_library_init() and others)
void FreeSSL(int loadStatus = 0);
void SSLThreadLocalCleanup();

int SSLtoWS2Error(int err);
