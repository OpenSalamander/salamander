// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#include "precomp.h"
#include <time.h>

#include "cfgdlg.h"
#include "pwdmngr.h"
#include "plugins.h"
#include "spl_crypt.h"

const char* SALAMANDER_PWDMNGR_FREEID = "Free ID";
const char* SALAMANDER_PWDMNGR_USEMASTERPWD = "Use Master Password";
const char* SALAMANDER_PWDMNGR_MASTERPWD_VERIFIER = "Master Password Verifier";

CPasswordManager PasswordManager;

CSalamanderCryptAbstract* GetSalamanderCrypt();

/*  AES modes and parameter sizes
    Field lengths (in bytes) versus File Encryption Mode (0 < mode < 4)

    Mode KeyLen SaltLen  MACLen Overhead
       1     16       8      10       18
       2     24      12      10       22
       3     32      16      10       26

   The following macros assume that the mode value is correct.
*/
#define PASSWORD_MANAGER_AES_MODE 3 // DO NOT CHANGE; for example, CMasterPasswordVerifier is declared "hardcoded"

//****************************************************************************
//
// FillBufferWithRandomData
//

void FillBufferWithRandomData(BYTE* buf, int len)
{
    static unsigned calls = 0; // ensure a different random header each time

    if (++calls == 1)
        srand((unsigned)time(NULL) ^ (unsigned)_getpid());

    while (len--)
        *buf++ = (rand() >> 7) & 0xff;
}

//****************************************************************************
//
// ScramblePassword / UnscramblePassword
//
// Taken from the FTP plugin. Used in case the user does not set the master
// password and strong AES encryption is therefore not used.
//

unsigned char ScrambleTable[256] =
    {
        0, 223, 235, 233, 240, 185, 88, 102, 22, 130, 27, 53, 79, 125, 66, 201,
        90, 71, 51, 60, 134, 104, 172, 244, 139, 84, 91, 12, 123, 155, 237, 151,
        192, 6, 87, 32, 211, 38, 149, 75, 164, 145, 52, 200, 224, 226, 156, 50,
        136, 190, 232, 63, 129, 209, 181, 120, 28, 99, 168, 94, 198, 40, 238, 112,
        55, 217, 124, 62, 227, 30, 36, 242, 208, 138, 174, 231, 26, 54, 214, 148,
        37, 157, 19, 137, 187, 111, 228, 39, 110, 17, 197, 229, 118, 246, 153, 80,
        21, 128, 69, 117, 234, 35, 58, 67, 92, 7, 132, 189, 5, 103, 10, 15,
        252, 195, 70, 147, 241, 202, 107, 49, 20, 251, 133, 76, 204, 73, 203, 135,
        184, 78, 194, 183, 1, 121, 109, 11, 143, 144, 171, 161, 48, 205, 245, 46,
        31, 72, 169, 131, 239, 160, 25, 207, 218, 146, 43, 140, 127, 255, 81, 98,
        42, 115, 173, 142, 114, 13, 2, 219, 57, 56, 24, 126, 3, 230, 47, 215,
        9, 44, 159, 33, 249, 18, 93, 95, 29, 113, 220, 89, 97, 182, 248, 64,
        68, 34, 4, 82, 74, 196, 213, 165, 179, 250, 108, 254, 59, 14, 236, 175,
        85, 199, 83, 106, 77, 178, 167, 225, 45, 247, 163, 158, 8, 221, 61, 191,
        119, 16, 253, 105, 186, 23, 170, 100, 216, 65, 162, 122, 150, 176, 154, 193,
        206, 222, 188, 152, 210, 243, 96, 41, 86, 180, 101, 177, 166, 141, 212, 116};

BOOL InitUnscrambleTable = TRUE;
BOOL InitSRand = TRUE;
unsigned char UnscrambleTable[256];

#define SCRAMBLE_LENGTH_EXTENSION 50 // number of characters by which we must extend the buffer to fit the scramble

void ScramblePassword(char* password)
{
    // padding + length ones digit + length tens digit + length hundreds digit + password
    int len = (int)strlen(password);
    char* buf = (char*)malloc(len + SCRAMBLE_LENGTH_EXTENSION);
    if (InitSRand)
    {
        srand((unsigned)time(NULL));
        InitSRand = FALSE;
    }
    int padding = (((len + 3) / 17) * 17 + 17) - 3 - len;
    int i;
    for (i = 0; i < padding; i++)
    {
        int p = 0;
        while (p <= 0 || p > 255 || p >= '0' && p <= '9')
            p = (int)((double)rand() / ((double)RAND_MAX / 256.0));
        buf[i] = (unsigned char)p;
    }
    buf[padding] = '0' + (len % 10);
    buf[padding + 1] = '0' + ((len / 10) % 10);
    buf[padding + 2] = '0' + ((len / 100) % 10);
    strcpy(buf + padding + 3, password);
    char* s = buf;
    int last = 31;
    while (*s != 0)
    {
        last = (last + (unsigned char)*s) % 255 + 1;
        *s = ScrambleTable[last];
        s++;
    }
    strcpy(password, buf);
    memset(buf, 0, len + SCRAMBLE_LENGTH_EXTENSION); // wipe the memory that contained the password
    free(buf);
}

BOOL UnscramblePassword(char* password)
{
    if (InitUnscrambleTable)
    {
        int i;
        for (i = 0; i < 256; i++)
        {
            UnscrambleTable[ScrambleTable[i]] = i;
        }
        InitUnscrambleTable = FALSE;
    }

    char* backup = DupStr(password); // backup for TRACE_E

    char* s = password;
    int last = 31;
    while (*s != 0)
    {
        int x = (int)UnscrambleTable[(unsigned char)*s] - 1 - (last % 255);
        if (x <= 0)
            x += 255;
        *s = (char)x;
        last = (last + x) % 255 + 1;
        s++;
    }

    s = password;
    while (*s != 0 && (*s < '0' || *s > '9'))
        s++; // find the length of the password
    BOOL ok = FALSE;
    if (strlen(s) >= 3)
    {
        int len = (s[0] - '0') + 10 * (s[1] - '0') + 100 * (s[2] - '0');
        int total = (((len + 3) / 17) * 17 + 17);
        int passwordLen = (int)strlen(password);
        if (len >= 0 && total == passwordLen && total - (s - password) - 3 == len)
        {
            memmove(password, password + passwordLen - len, len + 1);
            ok = TRUE;
        }
    }
    if (!ok)
    {
        password[0] = 0; // some error occured; clear the password
        TRACE_E("Unable to unscramble password! scrambled=" << backup);
    }
    memset(backup, 0, lstrlen(backup)); // wipe the memory that contained the password
    free(backup);
    return ok;
}

//****************************************************************************
//
// CChangeMasterPassword
//

CChangeMasterPassword::CChangeMasterPassword(HWND hParent, CPasswordManager* pwdManager)
    : CCommonDialog(HLanguage, IDD_CHANGE_MASTERPWD, IDD_CHANGE_MASTERPWD, hParent)
{
    PwdManager = pwdManager;
}

void CChangeMasterPassword::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CChangeMasterPassword::Validate()");
    HWND hWnd;

    // if master password usage is enabled, we must verify that the user entered it correctly
    if (PwdManager->IsUsingMasterPassword() && ti.GetControl(hWnd, IDC_CHMP_CURRENTPWD))
    {
        char curPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_CHMP_CURRENTPWD, curPwd, SAL_AES_MAX_PWD_LENGTH);
        if (!PwdManager->VerifyMasterPassword(curPwd))
        {
            SalMessageBox(HWindow, LoadStr(IDS_WRONG_MASTERPASSWORD), LoadStr(IDS_WARNINGTITLE), MB_OK | MB_ICONEXCLAMATION);
            SetDlgItemText(HWindow, IDC_CHMP_CURRENTPWD, "");
            ti.ErrorOn(IDC_CHMP_CURRENTPWD);
            return;
        }
    }

    if (ti.GetControl(hWnd, IDC_CHMP_NEWPWD))
    {
        char newPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_CHMP_NEWPWD, newPwd, SAL_AES_MAX_PWD_LENGTH);
        if (newPwd[0] != 0 && !PwdManager->IsPasswordSecure(newPwd))
        {
            if (SalMessageBox(HWindow, LoadStr(IDS_INSECUREPASSWORD), LoadStr(IDS_WARNINGTITLE),
                              MB_YESNO | MB_ICONWARNING) == IDNO)
            {
                ti.ErrorOn(IDC_CHMP_NEWPWD);
                return;
            }
        }
    }
}

void CChangeMasterPassword::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataToWindow)
    {
        // limit the password length; see the AES library limitations
        SendDlgItemMessage(HWindow, IDC_CHMP_CURRENTPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);
        SendDlgItemMessage(HWindow, IDC_CHMP_NEWPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);
        SendDlgItemMessage(HWindow, IDC_CHMP_RETYPEPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);

        if (!PwdManager->IsUsingMasterPassword())
        {
            // remove the ES_PASSWORD style from the current password field so we can display the "not set" text
            HWND hEdit = GetDlgItem(HWindow, IDC_CHMP_CURRENTPWD);
            SendMessage(hEdit, EM_SETPASSWORDCHAR, 0, 0);
            SetWindowText(hEdit, LoadStr(IDS_MASTERPASSWORD_NOTSET));
            EnableWindow(hEdit, FALSE);
        }

        EnableControls();
    }
    else
    {
        if (PwdManager->IsUsingMasterPassword())
        {
            char oldPwd[SAL_AES_MAX_PWD_LENGTH + 1];
            GetDlgItemText(HWindow, IDC_CHMP_CURRENTPWD, oldPwd, SAL_AES_MAX_PWD_LENGTH);
            PwdManager->EnterMasterPassword(oldPwd); // validation passed, so this will succeed as well
        }

        char newPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_CHMP_NEWPWD, newPwd, SAL_AES_MAX_PWD_LENGTH);
        PwdManager->SetMasterPassword(HWindow, newPwd);
    }
}

void CChangeMasterPassword::EnableControls()
{
    // the new (and confirmation) password must match; otherwise, the OK button is disabled
    char newPwd[SAL_AES_MAX_PWD_LENGTH + 1];
    char retypedPwd[SAL_AES_MAX_PWD_LENGTH + 1];
    GetDlgItemText(HWindow, IDC_CHMP_NEWPWD, newPwd, SAL_AES_MAX_PWD_LENGTH);
    GetDlgItemText(HWindow, IDC_CHMP_RETYPEPWD, retypedPwd, SAL_AES_MAX_PWD_LENGTH);
    BOOL enableOK = (stricmp(newPwd, retypedPwd) == 0);
    if (enableOK && !PwdManager->IsUsingMasterPassword() && newPwd[0] == 0) // block OK when master password usage is disabled and both password fields are empty
        enableOK = FALSE;
    EnableWindow(GetDlgItem(HWindow, IDOK), enableOK);
}

INT_PTR
CChangeMasterPassword::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CChangeMasterPassword::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_COMMAND:
    {
        if (HIWORD(wParam) == EN_CHANGE && (LOWORD(wParam) == IDC_CHMP_NEWPWD || LOWORD(wParam) == IDC_CHMP_RETYPEPWD))
        {
            // the new (and confirmation) passwords must match; otherwise, disable the OK button
            EnableControls();
        }
        break;
    }
    }
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CEnterMasterPassword
//

CEnterMasterPassword::CEnterMasterPassword(HWND hParent, CPasswordManager* pwdManager)
    : CCommonDialog(HLanguage, IDD_ENTER_MASTERPWD, IDD_ENTER_MASTERPWD, hParent)
{
    PwdManager = pwdManager;
}

void CEnterMasterPassword::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CEnterMasterPassword::Validate()");
    HWND hWnd;

    if (ti.GetControl(hWnd, IDC_MPR_PASSWORD))
    {
        char curPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_MPR_PASSWORD, curPwd, SAL_AES_MAX_PWD_LENGTH);
        if (!PwdManager->VerifyMasterPassword(curPwd))
        {
            SalMessageBox(HWindow, LoadStr(IDS_WRONG_MASTERPASSWORD), LoadStr(IDS_WARNINGTITLE), MB_OK | MB_ICONEXCLAMATION);
            SetDlgItemText(HWindow, IDC_MPR_PASSWORD, "");
            ti.ErrorOn(IDC_MPR_PASSWORD);
            return;
        }
    }
}

void CEnterMasterPassword::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
    {
        char plainMasterPassword[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_MPR_PASSWORD, plainMasterPassword, SAL_AES_MAX_PWD_LENGTH);
        PwdManager->EnterMasterPassword(plainMasterPassword); // validation passed, so this will succeed as well
    }
}

INT_PTR
CEnterMasterPassword::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CEnterMasterPassword::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CRemoveMasterPassword
//

CRemoveMasterPassword::CRemoveMasterPassword(HWND hParent, CPasswordManager* pwdManager)
    : CCommonDialog(HLanguage, IDD_REMOVE_MASTERPWD, IDD_REMOVE_MASTERPWD, hParent)
{
    PwdManager = pwdManager;
}

void CRemoveMasterPassword::Validate(CTransferInfo& ti)
{
    CALL_STACK_MESSAGE1("CRemoveMasterPassword::Validate()");
    HWND hWnd;

    if (ti.GetControl(hWnd, IDC_RMP_CURRENTPWD))
    {
        char curPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_RMP_CURRENTPWD, curPwd, SAL_AES_MAX_PWD_LENGTH);
        if (!PwdManager->VerifyMasterPassword(curPwd))
        {
            SalMessageBox(HWindow, LoadStr(IDS_WRONG_MASTERPASSWORD), LoadStr(IDS_WARNINGTITLE), MB_OK | MB_ICONEXCLAMATION);
            SetDlgItemText(HWindow, IDC_RMP_CURRENTPWD, "");
            ti.ErrorOn(IDC_RMP_CURRENTPWD);
            return;
        }
    }
}

void CRemoveMasterPassword::Transfer(CTransferInfo& ti)
{
    if (ti.Type == ttDataFromWindow)
    {
        char plainMasterPassword[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_RMP_CURRENTPWD, plainMasterPassword, SAL_AES_MAX_PWD_LENGTH);
        // pass the password to the password manager; plugins need it for the pending event
        PwdManager->EnterMasterPassword(plainMasterPassword); // validation passed, so this will succeed as well
    }
}

INT_PTR
CRemoveMasterPassword::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CRemoveMasterPassword::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    return CCommonDialog::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CCfgPageSecurity
//

CCfgPageSecurity::CCfgPageSecurity()
    : CCommonPropSheetPage(NULL, HLanguage, IDD_CFGPAGE_SECURITY, IDD_CFGPAGE_SECURITY, PSP_USETITLE, NULL)
{
}

void CCfgPageSecurity::Transfer(CTransferInfo& ti)
{
}

void CCfgPageSecurity::EnableControls()
{
    BOOL useMasterPwd = IsDlgButtonChecked(HWindow, IDC_SEC_ENABLE_MASTERPWD);
    EnableWindow(GetDlgItem(HWindow, IDC_SEC_CHANGE_MASTERPWD), useMasterPwd);
}

INT_PTR
CCfgPageSecurity::DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    CALL_STACK_MESSAGE4("CCfgPageSecurity::DialogProc(0x%X, 0x%IX, 0x%IX)", uMsg, wParam, lParam);
    switch (uMsg)
    {
    case WM_INITDIALOG:
    {
        // bypass Transfer(); this is a special handling of a checkbox
        CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, PasswordManager.IsUsingMasterPassword() ? BST_CHECKED : BST_UNCHECKED);

        EnableControls();
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SEC_ENABLE_MASTERPWD)
        {
            // checkbox was clicked
            EnableControls();

            // if the user checked the "Use Master Password" option, display the change password dialog
            BOOL useMasterPwd = IsDlgButtonChecked(HWindow, IDC_SEC_ENABLE_MASTERPWD);
            if (useMasterPwd)
            {
                // the user enabled the option
                CChangeMasterPassword dlg(HWindow, &PasswordManager);
                if (dlg.Execute() == IDOK)
                {
                    PasswordManager.NotifyAboutMasterPasswordChange(HWindow);
                }
                else
                {
                    // if the user selected Cancel, turn off the option that was just being enabled
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_UNCHECKED);
                }
            }
            else
            {
                // the user disabled the option
                CRemoveMasterPassword dlg(HWindow, &PasswordManager);
                if (dlg.Execute() == IDOK)
                {
                    PasswordManager.SetMasterPassword(HWindow, NULL);
                    PasswordManager.NotifyAboutMasterPasswordChange(HWindow);
                }
                else
                {
                    // if the user cancels, restore the option that was being disabled
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_CHECKED);
                }
            }
            EnableControls(); // CheckDlgButton() does not send notifications, so we must call it manually
        }

        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SEC_CHANGE_MASTERPWD)
        {
            CChangeMasterPassword dlg(HWindow, &PasswordManager);
            // if the user reset the password, uncheck the checkbox
            if (dlg.Execute() == IDOK)
            {
                if (!PasswordManager.IsUsingMasterPassword())
                {
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_UNCHECKED);
                    SetFocus(GetDlgItem(HWindow, IDC_SEC_ENABLE_MASTERPWD)); // focus must move away from the button we are about to disable
                    EnableControls();                                        // CheckDlgButton() does not send notifications, so we must call it manually
                }
                PasswordManager.NotifyAboutMasterPasswordChange(HWindow);
            }
        }
        break;
    }
    }

    return CCommonPropSheetPage::DialogProc(uMsg, wParam, lParam);
}

//****************************************************************************
//
// CPasswordManager
//

// signature values for passwords stored in the binary form
#define PWDMNGR_SIGNATURE_SCRAMBLED 1 // the password is only scrambled; obtaining the plain text password does not require the master password
#define PWDMNGR_SIGNATURE_ENCRYPTED 2 // the password is scrambled and then AES encrypted; it requires the master password

CPasswordManager::CPasswordManager()
{
    UseMasterPassword = FALSE;
    PlainMasterPassword = NULL;
    OldPlainMasterPassword = NULL;
    MasterPasswordVerifier = NULL;

    SalamanderCrypt = GetSalamanderCrypt();
}

CPasswordManager::~CPasswordManager()
{
    if (PlainMasterPassword != NULL)
    {
        free(PlainMasterPassword);
        PlainMasterPassword = NULL;
    }
    if (MasterPasswordVerifier != NULL)
    {
        delete MasterPasswordVerifier;
        MasterPasswordVerifier = NULL;
    }
}

BOOL CPasswordManager::IsPasswordSecure(const char* password)
{
    int l = (int)strlen(password);
    int a = 0, b = 0, c = 0, d = 0;

    while (*password)
    {
        if (*password >= 'a' && *password <= 'z')
            a = 1;
        else if (*password >= 'A' && *password <= 'Z')
            b = 1;
        else if (*password >= '0' && *password <= '9')
            c = 1;
        else
            d = 1;

        password++;
    }
    return l >= 6 && (a + b + c + d) >= 2;
}

BOOL CPasswordManager::EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt)
{
    if (encryptedPassword != NULL)
        *encryptedPassword = NULL;
    if (encryptedPasswordSize != NULL)
        *encryptedPasswordSize = 0;
    if (encrypt && (!UseMasterPassword || PlainMasterPassword == NULL))
    {
        TRACE_E("CPasswordManager::EncryptPassword(): Unexpected situation, Master Password was not entered. Call AskForMasterPassword() first.");
        return FALSE;
    }
    if (plainPassword == NULL || encryptedPassword == NULL || encryptedPasswordSize == NULL)
    {
        TRACE_E("CPasswordManager::EncryptPassword(): plainPassword == NULL || encryptedPassword == NULL || encryptedPasswordSize == NULL!");
        return FALSE;
    }

    // always scramble the password to mitigate the risk of a short password length
    char* scrambledPassword = (char*)malloc(lstrlen(plainPassword) + SCRAMBLE_LENGTH_EXTENSION); // reserve room for scrambling (the password becomes longer)
    lstrcpy(scrambledPassword, plainPassword);
    ScramblePassword(scrambledPassword);
    int scrambledPasswordLen = (int)strlen(scrambledPassword);

    if (encrypt)
    {
        *encryptedPassword = (BYTE*)malloc(1 + 16 + scrambledPasswordLen + 10);       // signature + AES salt + number of scrambled characters + AES MAC
        **encryptedPassword = PWDMNGR_SIGNATURE_ENCRYPTED;                            // the first character carries the signature
        FillBufferWithRandomData(*encryptedPassword + 1, 16);                         // fill the salt
        memcpy(*encryptedPassword + 1 + 16, scrambledPassword, scrambledPasswordLen); // followed by the scrambled password without the terminator

        CSalAES aes;
        WORD dummy; // unnecessary weakness, ignored
        int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, PlainMasterPassword, strlen(PlainMasterPassword), *encryptedPassword + 1, &dummy);
        if (ret != SAL_AES_ERR_GOOD_RETURN)
            TRACE_E("CPasswordManager::EncryptPassword(): unexpected state, ret=" << ret);       // should not happen
        SalamanderCrypt->AESEncrypt(&aes, *encryptedPassword + 1 + 16, scrambledPasswordLen);    // run the scrambled password through AES encryption
        SalamanderCrypt->AESEnd(&aes, *encryptedPassword + 1 + 16 + scrambledPasswordLen, NULL); // store including the MAC for individual passwords in case the configuration gets out of sync
        *encryptedPasswordSize = 1 + 16 + scrambledPasswordLen + 10;                             // record the total length
    }
    else
    {
        *encryptedPassword = (BYTE*)malloc(1 + scrambledPasswordLen);            // signature + number of scrambled characters without the terminator
        **encryptedPassword = PWDMNGR_SIGNATURE_SCRAMBLED;                       // the first character carries the signature
        memcpy(*encryptedPassword + 1, scrambledPassword, scrambledPasswordLen); // followed by the scrambled password without the NULL terminator
        *encryptedPasswordSize = 1 + scrambledPasswordLen;                       // record the total length
    }
    free(scrambledPassword);

    return TRUE;
}

/*
extern "C"
{
void mytrace(const char *txt)
{
  TRACE_I(txt);
}
}
*/

BOOL CPasswordManager::DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword)
{
    if (plainPassword != NULL)
        *plainPassword = NULL;
    if (encryptedPassword == NULL || encryptedPasswordSize == 0)
    {
        TRACE_E("CPasswordManager::DecryptPassword(): encryptedPassword == NULL || encryptedPasswordSize == 0!");
        return FALSE;
    }
    // if the password is encrypted with AES and we do not know the master password, we fail
    BOOL encrypted = IsPasswordEncrypted(encryptedPassword, encryptedPasswordSize);
    if (encrypted && (!UseMasterPassword || PlainMasterPassword == NULL) && OldPlainMasterPassword == NULL)
    {
        TRACE_I("CPasswordManager::DecryptPassword(): Master Password was not entered. Call AskForMasterPassword() first.");
        return FALSE;
    }
    if (encrypted && (encryptedPasswordSize < 1 + 16 + 1 + 10)) // the password itself must contain at least one character (signature + SALT + password + MAC)
    {
        TRACE_E("CPasswordManager::DecryptPassword(): stored password is too small, probably corrupted!");
        return FALSE;
    }

    const char* plainMasterPassword;
    plainMasterPassword = NULL;
    if (OldPlainMasterPassword != NULL)
        plainMasterPassword = OldPlainMasterPassword;
    else
        plainMasterPassword = PlainMasterPassword;

TRY_DECRYPT_AGAIN:

    BYTE* tmpBuff = (BYTE*)malloc(encryptedPasswordSize + 1); // +1 for the terminator so we can call unscramble after AES 
    memcpy(tmpBuff, encryptedPassword, encryptedPasswordSize);
    tmpBuff[encryptedPasswordSize] = 0; // terminator required by UnscramblePassword

    int pwdOffset = 1; // signature
    if (encrypted)
    {
        // decrypt the data with AES first
        CSalAES aes;
        WORD dummy;                                                                                                                                 // unnecessary weakness; ignore it
        int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, plainMasterPassword, strlen(plainMasterPassword), tmpBuff + 1, &dummy); // the salt follows the signature in 16 bytes
        if (ret != SAL_AES_ERR_GOOD_RETURN)
            TRACE_E("CPasswordManager::DecryptPassword(): unexpected state, ret=" << ret);        // should not happen
        SalamanderCrypt->AESDecrypt(&aes, tmpBuff + 1 + 16, encryptedPasswordSize - 1 - 16 - 10); // decrypt the password
        BYTE mac[10];                                                                             // MAC is used to verify the correctness of the master password
        SalamanderCrypt->AESEnd(&aes, mac, NULL);
        if (memcmp(mac, &tmpBuff[encryptedPasswordSize - 10], 10) != 0)
        {
            memset(tmpBuff, 0, encryptedPasswordSize); // clear the buffer that held the plain password
            free(tmpBuff);

            if (plainMasterPassword == OldPlainMasterPassword && UseMasterPassword && PlainMasterPassword != NULL)
            { // handle the case where the password is encrypted with the new master password
              // (the password cannot be decrypted with the old master password, but can with the new one,
              // therefore, the message that the password cannot be decrypted would be misleading,
              // because when the user tries to decrypt it with the new master password, it succeeds
              // meaning the user has no way to identify the undecryptable password)
                plainMasterPassword = PlainMasterPassword;
                goto TRY_DECRYPT_AGAIN;
            }

            TRACE_I("CPasswordManager::DecryptPassword(): wrong master password, password cannot be decrypted!");
            return FALSE;
        }
        pwdOffset += 16;                         // skip the AES salt
        tmpBuff[encryptedPasswordSize - 10] = 0; // terminator for unscramble (placed over the first MAC byte)
    }
    // data are stored scrambled; skip the signature and optional AES salt
    if (!UnscramblePassword((char*)tmpBuff + pwdOffset))
    {
        memset(tmpBuff, 0, encryptedPasswordSize); // clear the buffer that held the plain password
        free(tmpBuff);
        return FALSE;
    }

    if (plainPassword != NULL)
    {
        *plainPassword = DupStr((char*)tmpBuff + pwdOffset);
    }

    memset(tmpBuff, 0, encryptedPasswordSize); // clear the buffer that held the plain password
    free(tmpBuff);

    return TRUE;
}

BOOL CPasswordManager::IsPasswordEncrypted(const BYTE* encryptedPassword, int encryptedPasswordSize)
{
    if (encryptedPassword != NULL && encryptedPasswordSize > 0 && *encryptedPassword == PWDMNGR_SIGNATURE_ENCRYPTED)
        return TRUE;
    else
        return FALSE;
}

void CPasswordManager::SetMasterPassword(HWND hParent, const char* password)
{
    if (OldPlainMasterPassword != NULL)
    {
        TRACE_E("CPasswordManager::SetMasterPassword() unexpected situation, OldPlainMasterPassword != NULL");
    }

    if (PlainMasterPassword != NULL)
    {
        // if a master password is set, during this method we move it into OldPlainMasterPassword,
        // so that plugins can decrypt the passwords that were encrypted for them
        OldPlainMasterPassword = PlainMasterPassword;
        PlainMasterPassword = NULL;
    }

    if (MasterPasswordVerifier != NULL)
    {
        delete MasterPasswordVerifier;
        MasterPasswordVerifier = NULL;
    }

    if (password == NULL || *password == 0)
    {
        // master password removed
        UseMasterPassword = FALSE;
        Plugins.PasswordManagerEvent(hParent, PME_MASTERPASSWORDREMOVED);
    }
    else
    {
        // master password set/changed
        UseMasterPassword = TRUE;
        PlainMasterPassword = DupStr(password);
        CreateMasterPasswordVerifier(PlainMasterPassword);
        Plugins.PasswordManagerEvent(hParent, OldPlainMasterPassword == NULL ? PME_MASTERPASSWORDCREATED : PME_MASTERPASSWORDCHANGED);
    }

    // the thread has returned from calling Plugins.PasswordManagerEvent(), so we can discard OldPlainMasterPassword
    if (OldPlainMasterPassword != NULL)
    {
        free(OldPlainMasterPassword);
        OldPlainMasterPassword = NULL;
    }
}

BOOL CPasswordManager::EnterMasterPassword(const char* password)
{
    if (!UseMasterPassword)
    {
        TRACE_E("CPasswordManager::EnterMasterPassword(): Unexpected situation, Master Password is not used.");
        return FALSE;
    }
    if (PlainMasterPassword != NULL)
    {
        // if an attempt is made to insert the current password again, silently ignore it
        if (strcmp(PlainMasterPassword, password) == 0)
            return TRUE;

        TRACE_E("CPasswordManager::EnterMasterPassword(): Unexpected situation, Master Password is already entered.");
        return FALSE;
    }
    if (!VerifyMasterPassword(password))
    {
        TRACE_E("CPasswordManager::EnterMasterPassword(): Wrong master password.");
        return FALSE;
    }

    PlainMasterPassword = DupStr(password);

    return TRUE;
}

void CPasswordManager::CreateMasterPasswordVerifier(const char* password)
{
    // allocate the structure for the verifier
    CMasterPasswordVerifier* mpv;
    mpv = new CMasterPasswordVerifier;

    // fill Salt and Dummy with random data
    FillBufferWithRandomData(mpv->Salt, 16);
    FillBufferWithRandomData(mpv->Dummy, 16);

    CSalAES aes;
    WORD dummy; // unnecessary weakness; ignore it
    int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, password, strlen(password), mpv->Salt, &dummy);
    if (ret != SAL_AES_ERR_GOOD_RETURN)
        TRACE_E("CPasswordManager::CreateMasterPasswordVerifier(): unexpected state, ret=" << ret); // should not happen
    SalamanderCrypt->AESEncrypt(&aes, mpv->Dummy, 16);                                              // encrypt Dummy
    SalamanderCrypt->AESEnd(&aes, mpv->MAC, NULL);                                                  // store the MAC for later internal verification

    // store the allocated verifier
    if (MasterPasswordVerifier != NULL)
        delete MasterPasswordVerifier;
    MasterPasswordVerifier = mpv;
}

BOOL CPasswordManager::VerifyMasterPassword(const char* password)
{
    if (!UseMasterPassword)
    {
        TRACE_E("CPasswordManager::VerifyMasterPassword() Using of Master Password is turned off in Salamanader configuration.");
        return FALSE;
    }

    // if the plaintext master password is cached, we can perform a simple comparison
    if (PlainMasterPassword != NULL)
    {
        return (strcmp(PlainMasterPassword, password) == 0);
    }

    if (MasterPasswordVerifier == NULL)
    {
        TRACE_E("CPasswordManager::VerifyMasterPassword() unexpected situation, MasterPasswordVerifier==NULL.");
        return FALSE;
    }

    CMasterPasswordVerifier mpv;
    memcpy(&mpv, MasterPasswordVerifier, sizeof(CMasterPasswordVerifier));

    CSalAES aes;
    WORD dummy; // unnecessary weakness; ignore it
    int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, password, strlen(password), mpv.Salt, &dummy);
    if (ret != SAL_AES_ERR_GOOD_RETURN)
        TRACE_E("CPasswordManager::VerifyMasterPassword(): unexpected state, ret=" << ret); // should not happen
    SalamanderCrypt->AESDecrypt(&aes, mpv.Dummy, 16);                                       // let it decrypt Dummy
    SalamanderCrypt->AESEnd(&aes, mpv.MAC, NULL);                                           // we will check the MAC
    return (memcmp(mpv.MAC, MasterPasswordVerifier->MAC, sizeof(mpv.MAC)) == 0);
}

void CPasswordManager::NotifyAboutMasterPasswordChange(HWND hParent)
{
    BOOL set = IsUsingMasterPassword();
    SalMessageBox(hParent, LoadStr(set ? IDS_MASTERPASSWORD_SET : IDS_MASTERPASSWORD_REMOVED), LoadStr(IDS_MASTERPASSWORD_CHANGED_TITLE),
                  MB_OK | (set ? MB_ICONINFORMATION : MB_ICONWARNING));
}

BOOL CPasswordManager::Save(HKEY hKey)
{
    BOOL ret = TRUE;

    // password manager configuration data
    if (ret)
        ret &= SetValue(hKey, SALAMANDER_PWDMNGR_USEMASTERPWD, REG_DWORD, &UseMasterPassword, sizeof(UseMasterPassword));
    if (UseMasterPassword)
    {
        if (ret)
            ret &= SetValue(hKey, SALAMANDER_PWDMNGR_MASTERPWD_VERIFIER, REG_BINARY, MasterPasswordVerifier, sizeof(CMasterPasswordVerifier));
    }
    else
    {
        DeleteValue(hKey, SALAMANDER_PWDMNGR_MASTERPWD_VERIFIER);
    }

    return TRUE;
}

BOOL CPasswordManager::Load(HKEY hKey)
{
    BOOL ret = TRUE;
    // password manager configuration data
    if (ret)
        ret &= GetValue(hKey, SALAMANDER_PWDMNGR_USEMASTERPWD, REG_DWORD, &UseMasterPassword, sizeof(UseMasterPassword));
    if (UseMasterPassword)
    {
        MasterPasswordVerifier = new CMasterPasswordVerifier;
        if (ret)
            ret &= GetValue(hKey, SALAMANDER_PWDMNGR_MASTERPWD_VERIFIER, REG_BINARY, MasterPasswordVerifier, sizeof(CMasterPasswordVerifier));
    }

    return TRUE;
}

BOOL CPasswordManager::AskForMasterPassword(HWND hParent)
{
    // return FALSE if master password usage is disabled
    if (!UseMasterPassword)
        return FALSE;

    // prompt for the master password (even if cached; the caller might have verified it beforehand using IsMasterPasswordSet())
    CEnterMasterPassword dlg(hParent, this);
    return dlg.Execute() == IDOK; // return TRUE if the user entered it correctly, otherwise FALSE
}

//****************************************************************************
//
// CSalamanderPasswordManager (called by plugins)
//

BOOL CSalamanderPasswordManager::IsUsingMasterPassword()
{
    CALL_STACK_MESSAGE_NONE
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::IsUsingMasterPassword() only from main thread!");
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.IsUsingMasterPassword();
}

BOOL CSalamanderPasswordManager::IsMasterPasswordSet()
{
    CALL_STACK_MESSAGE_NONE
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::IsMasterPasswordSet() only from main thread!");
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.IsMasterPasswordSet();
}

BOOL CSalamanderPasswordManager::AskForMasterPassword(HWND hParent)
{
    CALL_STACK_MESSAGE1("CSalamanderPasswordManager::AskForMasterPassword()");
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::AskForMasterPassword() only from main thread!");
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.AskForMasterPassword(hParent);
}

BOOL CSalamanderPasswordManager::EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt)
{
    CALL_STACK_MESSAGE1("CSalamanderPasswordManager::EncryptPassword()");
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::EncryptPassword() only from main thread!");
        if (encryptedPassword != NULL)
            *encryptedPassword = NULL;
        if (encryptedPasswordSize != NULL)
            *encryptedPasswordSize = 0;
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.EncryptPassword(plainPassword, encryptedPassword, encryptedPasswordSize, encrypt);
}

BOOL CSalamanderPasswordManager::DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword)
{
    CALL_STACK_MESSAGE1("CSalamanderPasswordManager::DecryptPassword()");
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::DecryptPassword() only from main thread!");
        if (plainPassword != NULL)
            *plainPassword = NULL;
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.DecryptPassword(encryptedPassword, encryptedPasswordSize, plainPassword);
}

BOOL CSalamanderPasswordManager::IsPasswordEncrypted(const BYTE* encryptedPassword, int encryptedPasswordSize)
{
    CALL_STACK_MESSAGE1("CSalamanderPasswordManager::IsPasswordEncrypted()");
#ifdef _DEBUG
    if (MainThreadID != GetCurrentThreadId())
    {
        TRACE_E("You can call CSalamanderPasswordManager::IsPasswordEncrypted() only from main thread!");
        return FALSE;
    }
#endif // _DEBUG
    return PasswordManager.IsPasswordEncrypted(encryptedPassword, encryptedPasswordSize);
}
