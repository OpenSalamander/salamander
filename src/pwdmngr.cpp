// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
#define PASSWORD_MANAGER_AES_MODE 3 // NEMENIT, napriklad CMasterPasswordVerifier je deklarovana "natvrdo"

//****************************************************************************
//
// FillBufferWithRandomData
//

void FillBufferWithRandomData(BYTE* buf, int len)
{
    static unsigned calls = 0; //ensure different random header each time

    if (++calls == 1)
        srand((unsigned)time(NULL) ^ (unsigned)_getpid());

    while (len--)
        *buf++ = (rand() >> 7) & 0xff;
}

//****************************************************************************
//
// ScramblePassword / UnscramblePassword
//
// Prevzato z FTP pluginu. Slouzi pro pripad, ze uzivatel nenastavi master
// password a nepouziva se tedy silne AES sifrovani.
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

#define SCRAMBLE_LENGTH_EXTENSION 50 // pocet znaku o ktere musime rozsirit buffer, aby se vesel scramble

void ScramblePassword(char* password)
{
    // padding + jednotky delky + desitky delky + stovky delky + password
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
    memset(buf, 0, len + SCRAMBLE_LENGTH_EXTENSION); // cisteni pameti obsahujici password
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

    char* backup = DupStr(password); // zaloha pro TRACE_E

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
        s++; // najdeme si delku passwordu
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
        password[0] = 0; // nejaka chyba, zrusime password
        TRACE_E("Unable to unscramble password! scrambled=" << backup);
    }
    memset(backup, 0, lstrlen(backup)); // cisteni pameti obsahujici password
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

    // pokud je zapnuto pouzivani master password, musime overit, ze ho uzivatel zadal spravne
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
        // limitujeme delku hesla, viz omezeni AES knihovny
        SendDlgItemMessage(HWindow, IDC_CHMP_CURRENTPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);
        SendDlgItemMessage(HWindow, IDC_CHMP_NEWPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);
        SendDlgItemMessage(HWindow, IDC_CHMP_RETYPEPWD, EM_LIMITTEXT, SAL_AES_MAX_PWD_LENGTH, 0);

        if (!PwdManager->IsUsingMasterPassword())
        {
            // sestrelim z current pwd ES_PASSWORD styl, abychom mohli zobrazit text (not set)
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
            PwdManager->EnterMasterPassword(oldPwd); // prosla validace, tak uz toto projde take
        }

        char newPwd[SAL_AES_MAX_PWD_LENGTH + 1];
        GetDlgItemText(HWindow, IDC_CHMP_NEWPWD, newPwd, SAL_AES_MAX_PWD_LENGTH);
        PwdManager->SetMasterPassword(HWindow, newPwd);
    }
}

void CChangeMasterPassword::EnableControls()
{
    // nove (a kontrolni) heslo museji byt shodne, jinak zakazeme OK tlacitko
    char newPwd[SAL_AES_MAX_PWD_LENGTH + 1];
    char retypedPwd[SAL_AES_MAX_PWD_LENGTH + 1];
    GetDlgItemText(HWindow, IDC_CHMP_NEWPWD, newPwd, SAL_AES_MAX_PWD_LENGTH);
    GetDlgItemText(HWindow, IDC_CHMP_RETYPEPWD, retypedPwd, SAL_AES_MAX_PWD_LENGTH);
    BOOL enableOK = (stricmp(newPwd, retypedPwd) == 0);
    if (enableOK && !PwdManager->IsUsingMasterPassword() && newPwd[0] == 0) // OK nesmime dovolit, pokud neni zadan MP a zaroven jsou obe hesla prazdna
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
            // nove (a kontrolni) heslo museji byt shodne, jinak zakazeme OK tlacitko
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
        PwdManager->EnterMasterPassword(plainMasterPassword); // prosla validace, tak uz toto projde take
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
        // heslo musime predat password manageru, bude potreba pro event zasilany pluginum
        PwdManager->EnterMasterPassword(plainMasterPassword); // prosla validace, tak uz toto projde take
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
        // obejdu Transfer(), jde o specialni ovladani checkboxu
        CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, PasswordManager.IsUsingMasterPassword() ? BST_CHECKED : BST_UNCHECKED);

        EnableControls();
        break;
    }

    case WM_COMMAND:
    {
        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SEC_ENABLE_MASTERPWD)
        {
            // kliknuti na checkbox
            EnableControls();

            // pokud uzivatel zaskrtnul volbu "Pouzivat Master Password", zobrazime dialog pro zmenu hesla
            BOOL useMasterPwd = IsDlgButtonChecked(HWindow, IDC_SEC_ENABLE_MASTERPWD);
            if (useMasterPwd)
            {
                // uzivatel volbu zapnul
                CChangeMasterPassword dlg(HWindow, &PasswordManager);
                if (dlg.Execute() == IDOK)
                {
                    PasswordManager.NotifyAboutMasterPasswordChange(HWindow);
                }
                else
                {
                    // pokud uzivatel zadal Cancel, vypneme prave zapinanou volbu
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_UNCHECKED);
                }
            }
            else
            {
                // uzivatel volbu vypnul
                CRemoveMasterPassword dlg(HWindow, &PasswordManager);
                if (dlg.Execute() == IDOK)
                {
                    PasswordManager.SetMasterPassword(HWindow, NULL);
                    PasswordManager.NotifyAboutMasterPasswordChange(HWindow);
                }
                else
                {
                    // pokud uzivatel zadal Cancel, vypneme zapneme vypinanou volbu
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_CHECKED);
                }
            }
            EnableControls(); // CheckDlgButton() nerozesle notifikace, musime se zavolat sami
        }

        if (HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDC_SEC_CHANGE_MASTERPWD)
        {
            CChangeMasterPassword dlg(HWindow, &PasswordManager);
            // pokud uzivatel resetnul heslo, vypneme checkbox
            if (dlg.Execute() == IDOK)
            {
                if (!PasswordManager.IsUsingMasterPassword())
                {
                    CheckDlgButton(HWindow, IDC_SEC_ENABLE_MASTERPWD, BST_UNCHECKED);
                    SetFocus(GetDlgItem(HWindow, IDC_SEC_ENABLE_MASTERPWD)); // focus musi mimo tlacitko, ktere za chvili zakazeme
                    EnableControls();                                        // CheckDlgButton() nerozesle notifikace, musime se zavolat sami
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

// signatura binarne drzenych hesel
#define PWDMNGR_SIGNATURE_SCRAMBLED 1 // heslo je pouze scrambled, pro ziskani plain text hesla neni potreba master password
#define PWDMNGR_SIGNATURE_ENCRYPTED 2 // heslo je scrabled a navic AES sifrovane, vyzaduje master password

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

    // heslo vzdy protahneme scramblem, cimz odstinime mozny maly pocet znaku
    char* scrambledPassword = (char*)malloc(lstrlen(plainPassword) + SCRAMBLE_LENGTH_EXTENSION); // rezerva pro scrambleni (password se tim prodluzuje)
    lstrcpy(scrambledPassword, plainPassword);
    ScramblePassword(scrambledPassword);
    int scrambledPasswordLen = (int)strlen(scrambledPassword);

    if (encrypt)
    {
        *encryptedPassword = (BYTE*)malloc(1 + 16 + scrambledPasswordLen + 10);       // signatura + AES-SALT + pocet scrambled znaku + AES-MAC
        **encryptedPassword = PWDMNGR_SIGNATURE_ENCRYPTED;                            // prvni znak nese signaturu
        FillBufferWithRandomData(*encryptedPassword + 1, 16);                         // SALT
        memcpy(*encryptedPassword + 1 + 16, scrambledPassword, scrambledPasswordLen); // pak nasleduje scrambled heslo bez terminatoru

        CSalAES aes;
        WORD dummy; // zbytecna slabina, ignorujeme
        int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, PlainMasterPassword, strlen(PlainMasterPassword), *encryptedPassword + 1, &dummy);
        if (ret != SAL_AES_ERR_GOOD_RETURN)
            TRACE_E("CPasswordManager::EncryptPassword(): unexpected state, ret=" << ret);       // nemelo by nastat
        SalamanderCrypt->AESEncrypt(&aes, *encryptedPassword + 1 + 16, scrambledPasswordLen);    // scrambled heslo nechame prejet AESem
        SalamanderCrypt->AESEnd(&aes, *encryptedPassword + 1 + 16 + scrambledPasswordLen, NULL); // ukladame vcetne MACu pro jednotliva hesla, kdyby se nam rozjela konfigurace
        *encryptedPasswordSize = 1 + 16 + scrambledPasswordLen + 10;                             // zapiseme celkovou delku
    }
    else
    {
        *encryptedPassword = (BYTE*)malloc(1 + scrambledPasswordLen);            // signatura + pocet scrambled znaku bez terminatoru
        **encryptedPassword = PWDMNGR_SIGNATURE_SCRAMBLED;                       // prvni znak nese signaturu
        memcpy(*encryptedPassword + 1, scrambledPassword, scrambledPasswordLen); // nasleduje scrambled heslo bez NULL terminatoru
        *encryptedPasswordSize = 1 + scrambledPasswordLen;                       // zapiseme celkovou delku
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
    // pokud je heslo sifrovane pomoci AES a my nezname master password, selzeme
    BOOL encrypted = IsPasswordEncrypted(encryptedPassword, encryptedPasswordSize);
    if (encrypted && (!UseMasterPassword || PlainMasterPassword == NULL) && OldPlainMasterPassword == NULL)
    {
        TRACE_I("CPasswordManager::DecryptPassword(): Master Password was not entered. Call AskForMasterPassword() first.");
        return FALSE;
    }
    if (encrypted && (encryptedPasswordSize < 1 + 16 + 1 + 10)) // na vlastni heslo musi byt alespon jeden znak (signatura + SALT + heslo + MAC)
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

    BYTE* tmpBuff = (BYTE*)malloc(encryptedPasswordSize + 1); // +1 pro terminator, abychom mohli po AES zavolat unscramble
    memcpy(tmpBuff, encryptedPassword, encryptedPasswordSize);
    tmpBuff[encryptedPasswordSize] = 0; // terminator pro unscramble

    int pwdOffset = 1; // signatura
    if (encrypted)
    {
        // napred data rozsifrujeme pomoci AES
        CSalAES aes;
        WORD dummy;                                                                                                                                 // zbytecna slabina, ignorujeme
        int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, plainMasterPassword, strlen(plainMasterPassword), tmpBuff + 1, &dummy); // salt lezi za signaturou na 16 bajtech
        if (ret != SAL_AES_ERR_GOOD_RETURN)
            TRACE_E("CPasswordManager::DecryptPassword(): unexpected state, ret=" << ret);        // nemelo by nastat
        SalamanderCrypt->AESDecrypt(&aes, tmpBuff + 1 + 16, encryptedPasswordSize - 1 - 16 - 10); // rozsifrujeme heslo
        BYTE mac[10];                                                                             // pro overeni spravnosti master password nam poslouzi MAC
        SalamanderCrypt->AESEnd(&aes, mac, NULL);
        if (memcmp(mac, &tmpBuff[encryptedPasswordSize - 10], 10) != 0)
        {
            memset(tmpBuff, 0, encryptedPasswordSize); // nulujeme buffer s plain heslem
            free(tmpBuff);

            if (plainMasterPassword == OldPlainMasterPassword && UseMasterPassword && PlainMasterPassword != NULL)
            { // resi situaci, kdy mame heslo zasifrovane novym master passwordem (heslo nelze desifrovat puvodnim master passwordem, ale novym ano, tedy hlaska, ze heslo nejde desifrovat je matouci, protoze kdyz uzivatel zkusi heslo desifrovat novym master passwordem, povede se to a tedy uzivatel nema sanci najit to nedesifrovatelne heslo)
                plainMasterPassword = PlainMasterPassword;
                goto TRY_DECRYPT_AGAIN;
            }

            TRACE_I("CPasswordManager::DecryptPassword(): wrong master password, password cannot be decrypted!");
            return FALSE;
        }
        pwdOffset += 16;                         // preskocime AES-SALT
        tmpBuff[encryptedPasswordSize - 10] = 0; // terminator pro unscramble (na miste prvniho znak MAC)
    }
    // vnitrne jsou data scrambled, preskocime signaturu a pripadne AES-SALT
    if (!UnscramblePassword((char*)tmpBuff + pwdOffset))
    {
        memset(tmpBuff, 0, encryptedPasswordSize); // nulujeme buffer s plain heslem
        free(tmpBuff);
        return FALSE;
    }

    if (plainPassword != NULL)
    {
        *plainPassword = DupStr((char*)tmpBuff + pwdOffset);
    }

    memset(tmpBuff, 0, encryptedPasswordSize); // nulujeme buffer s plain heslem
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
        // pokud je nastaven master password, po dobu behu teto metody ho prehodime do OldPlainMasterPassword,
        // aby mely pluginy moznost rozsifrovat jim zasifrovana hesla
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
        // zruseni master password
        UseMasterPassword = FALSE;
        Plugins.PasswordManagerEvent(hParent, PME_MASTERPASSWORDREMOVED);
    }
    else
    {
        // nastaveni/zmena master password
        UseMasterPassword = TRUE;
        PlainMasterPassword = DupStr(password);
        CreateMasterPasswordVerifier(PlainMasterPassword);
        Plugins.PasswordManagerEvent(hParent, OldPlainMasterPassword == NULL ? PME_MASTERPASSWORDCREATED : PME_MASTERPASSWORDCHANGED);
    }

    // vlakno se nam vratilo z volani Plugins.PasswordManagerEvent(), muzeme zahodit OldPlainMasterPassword
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
        // pokud se nam snazi znovu vlozit aktualni heslo, tise to ignorujeme
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
    // naalokujeme strukturu pro verifier
    CMasterPasswordVerifier* mpv;
    mpv = new CMasterPasswordVerifier;

    // Salt a Dummy hodnoty budou nahodne
    FillBufferWithRandomData(mpv->Salt, 16);
    FillBufferWithRandomData(mpv->Dummy, 16);

    CSalAES aes;
    WORD dummy; // zbytecna slabina, ignorujeme
    int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, password, strlen(password), mpv->Salt, &dummy);
    if (ret != SAL_AES_ERR_GOOD_RETURN)
        TRACE_E("CPasswordManager::CreateMasterPasswordVerifier(): unexpected state, ret=" << ret); // nemelo by nastat
    SalamanderCrypt->AESEncrypt(&aes, mpv->Dummy, 16);                                              // Dummy nechame zasifrovat
    SalamanderCrypt->AESEnd(&aes, mpv->MAC, NULL);                                                  // MAC si ulozime pro vlastni verifikaci

    // ulozime alokovanou strukturu
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

    // pokud drzime master password v otevrenem stavu, muzeme provest jednoduche porovnani
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
    WORD dummy; // zbytecna slabina, ignorujeme
    int ret = SalamanderCrypt->AESInit(&aes, PASSWORD_MANAGER_AES_MODE, password, strlen(password), mpv.Salt, &dummy);
    if (ret != SAL_AES_ERR_GOOD_RETURN)
        TRACE_E("CPasswordManager::VerifyMasterPassword(): unexpected state, ret=" << ret); // nemelo by nastat
    SalamanderCrypt->AESDecrypt(&aes, mpv.Dummy, 16);                                       // Dummy nechame rozsifrovat
    SalamanderCrypt->AESEnd(&aes, mpv.MAC, NULL);                                           // MAC budeme kontrolovat
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

    // konfigurace password managera
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
    // konfigurace password managera
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
    // pokud se master password nepouziva, vratime FALSE
    if (!UseMasterPassword)
        return FALSE;

    // na master password doptame (i v pripade, ze ho jiz zname -- volajici si to mohl overit predem pomoci IsMasterPasswordSet())
    CEnterMasterPassword dlg(hParent, this);
    return dlg.Execute() == IDOK; // pokud ho uzivatel spravne zadal, vratime TRUE, jinak FALSE
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
