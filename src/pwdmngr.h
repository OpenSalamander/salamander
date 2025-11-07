// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

class CPasswordManager;

//****************************************************************************
//
// CChangeMasterPassword
//

class CChangeMasterPassword : public CCommonDialog
{
private:
    CPasswordManager* PwdManager;

public:
    CChangeMasterPassword(HWND hParent, CPasswordManager* pwdManager);

protected:
    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    void EnableControls();

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CEnterMasterPassword
//

class CEnterMasterPassword : public CCommonDialog
{
private:
    CPasswordManager* PwdManager;

public:
    CEnterMasterPassword(HWND hParent, CPasswordManager* pwdManager);

protected:
    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CRemoveMasterPassword
//

class CRemoveMasterPassword : public CCommonDialog
{
private:
    CPasswordManager* PwdManager;

public:
    CRemoveMasterPassword(HWND hParent, CPasswordManager* pwdManager);

protected:
    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

    INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//****************************************************************************
//
// CPasswordManager
//
// Password storage. When the user enables the "Use Master Password" option,
// configuration passwords are stored and encrypted with AES; otherwise they are only
// scrambled using Petr's original FTP client method.
//
// Password manager methods may be called only from Salamander's main thread.
// Planned access points are: FTP connect, WinSCP connect, the
// Salamander configuration and saving/loading Salamander’s configuration. Because all of them currently run in the
// main thread, so we don’t need to handle concurrency or locking of the manager.

#pragma pack(push)
#pragma pack(1)
struct CMasterPasswordVerifier
{
    BYTE Salt[16];  // random salt, mode == 3
    BYTE Dummy[16]; // random encrypted data
    BYTE MAC[10];   // verification record used to check the correctness of the master password
};
#pragma pack(pop)

class CPasswordManager
{
private:
    BOOL UseMasterPassword;                          // the user has (at some point) provided the master password used for data encryption; the plaintext value may later be NULL and must be requested again
    char* PlainMasterPassword;                       // allocated password (in plaintext) terminated by a null character; NULL if the user has not entered it during this session; not stored into the registry
    char* OldPlainMasterPassword;                    // temporarily holds the previous PlainMasterPassword during the call to Plugins.PasswordManagerEvent(), allowing the plug-in to request decryption of its passwords
    CMasterPasswordVerifier* MasterPasswordVerifier; // used to verify the correctness of the master password; stored in the registry; may be NULL

    CSalamanderCryptAbstract* SalamanderCrypt; // interface for the work with cryptographic library

public:
    CPasswordManager();
    ~CPasswordManager();

    BOOL IsPasswordSecure(const char* password); // returns TRUE when the password meets strength requirements, otherwise FALSE

    // sets the master password; if 'password' is NULL or an empty string, master password is turned off
    void SetMasterPassword(HWND hParent, const char* password);

    // used to provide the master password when it is not currently known in plaintext form
    BOOL EnterMasterPassword(const char* password);

    BOOL ChangeMasterPassword(HWND hParent);
    BOOL IsUsingMasterPassword() { return UseMasterPassword; }         // are the passwords protected using AES/Master Password?
    BOOL IsMasterPasswordSet() { return PlainMasterPassword != NULL; } // has the user entered the Master Password in this session?

    // when master password usage is enabled and the password has not been entered
    // in this session, it displays a dialog for entering it
    // returns FALSE if the correct master password cannot be entered in that situation; returns TRUE otherwise
    // this method needs to be called before calling the EncryptPassword/DecryptPassword methods, when it is encrypt/encrypted == TRUE
    // callers may invoke it even when master password usage is turned off (quietly returns TRUE)
    BOOL AskForMasterPassword(HWND hParent);

    void NotifyAboutMasterPasswordChange(HWND hParent);

    BOOL Save(HKEY hKey); // saves stored passwords to the Registry
    BOOL Load(HKEY hKey); // loads data from the registry

    // 'encryptedPasswordSize' specifies the buffer size for the encrypted password to be stored; the size must be 50 characters greater than the 'plainPassword' length

    // encrypts the plaintext password into the binary form using strong encryption (AES)
    // before AES encryption, it performs an additional scramble that adds padding (hardening short passwords)
    // if the caller requires AES password encryption ('encrypt' == TRUE), before calling the method he must call AskForMasterPassword() that must return TRUE
    // 'plainPassword' is the pointer to the zero-terminated password in text form
    // 'encryptedPassword' returns a pointer to a binary buffer allocated by Salamander with the encrypted password; this buffer must be deallocated using CSalamanderGeneralAbstract::Free
    // 'encryptedPasswordSize' returns the size of the 'encryptedPassword' buffer in bytes
    // if 'encrypt' is TRUE, the function encrypts the password using AES (safe, protected by the master password); if FALSE, the password is only scrambled
    BOOL EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt);
    // 'plainPassword' must be deallocated using CSalamanderGeneralAbstract::Free
    // if 'plainPassword' is NULL, only checks whether the password can be decrypted
    BOOL DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword);
    // returns TRUE for an AES-encrypted password, otherwise returns FALSE; the signature in the first byte of the password determines it
    BOOL IsPasswordEncrypted(const BYTE* encyptedPassword, int encyptedPasswordSize);

    // adds a new password to the Passwords array; returns TRUE if successful (also fills 'passwordID' with a value greater than zero and less than 0xffffffff), otherwise FALSE
    // 'pluginDLLName' must be NULL if the password belongs to the Salamander's core, otherwise it is filled by CPluginData
    // 'password' is the password in plain form
    //BOOL StorePassword(const char *pluginDLLName, const char *password, DWORD *passwordID); // the call must be preceded by a successful AskForMasterPassword()
    //BOOL SetPassword(const char *pluginDLLName, DWORD passwordID, const char *password); // the call must be preceded by a successful AskForMasterPassword()
    //BOOL GetPassword(const char *pluginDLLName, DWORD passwordID, char *password, int bufferLen); // the call must be preceded by a successful AskForMasterPassword()
    //BOOL DeletePassword(const char *pluginDLLName, DWORD passwordID);

    // verifies that 'password' matches the password stored in MasterPasswordVerifier; returns TRUE on success, otherwise FALSE
    BOOL VerifyMasterPassword(const char* password);

protected:
    // allocates and computes MasterPasswordVerifier, which is stored into the registry for subsequent verification
    void CreateMasterPasswordVerifier(const char* password);
};

extern CPasswordManager PasswordManager;
