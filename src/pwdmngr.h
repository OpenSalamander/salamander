// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

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
// Uloziste hesel. Pokud uzivatel zapne volbu "Use a master password",
// jsou hesla v konfiguraci ulozena sifrovana pomoci AES. Jinak jsou
// pouze scramblena metodou, kterou Petr zavedl puvodne v FTP clientu.
//
// Metody Password Manageru lze volat pouze z hlavniho threadu Salamandera.
// Planovana mista pristupu jsou: FTP connect, WinSCP connect, konfigurace
// Salamandera, Save/Load konfigurace Salamandera. Vse momentalne bezi v
// hlavnim threadu, takze nemusime resit konkurenci a zamykani manageru.

#pragma pack(push)
#pragma pack(1)
struct CMasterPasswordVerifier
{
    BYTE Salt[16];  // nahodny salt, mode==3
    BYTE Dummy[16]; // nahodna sifrovana data
    BYTE MAC[10];   // kontrolni zaznam, pomoci ktereho overime spravnost master password
};
#pragma pack(pop)

class CPasswordManager
{
private:
    BOOL UseMasterPassword;                          // uzivatel (nekdy) zadal master password, ktery byl pouzit pro zasifrovani dat, samotny 'MasterPassword' vsak muze byt ted NULL a bude nutne se na nej doptat
    char* PlainMasterPassword;                       // alokovane heslo (v otevrenem stavu) zakoncene nulou; NULL pokud ho uzivatel v ramci teto session nezadal; neuklada se do registry
    char* OldPlainMasterPassword;                    // docasne drzi stare 'PlainMasterPassword' behem volani Plugins.PasswordManagerEvent(), aby plugin mohl pozadat o rozsifrovani hesel
    CMasterPasswordVerifier* MasterPasswordVerifier; // slouzi pro overeni spravnosti master password; uklada se do registry; muze byt NULL

    CSalamanderCryptAbstract* SalamanderCrypt; // interface pro praci s Crypt knihovnou

public:
    CPasswordManager();
    ~CPasswordManager();

    BOOL IsPasswordSecure(const char* password); // posoudi, zda je heslo dostatecne silne, vraci TRUE pokud ano, jinak FALSE

    // nastavi master password, pokud je 'password' NULL nebo prazdnej retezec, vypne master password
    void SetMasterPassword(HWND hParent, const char* password);

    // slouzi pro vlozeni master password, ktery momentalne neni znamy v plain verzi
    BOOL EnterMasterPassword(const char* password);

    BOOL ChangeMasterPassword(HWND hParent);
    BOOL IsUsingMasterPassword() { return UseMasterPassword; }         // jsou hesla chranena pomoci AES/Master Password?
    BOOL IsMasterPasswordSet() { return PlainMasterPassword != NULL; } // zadal uzivatel v teto session Master Password?

    // pokud je zapnute pouzivani master password a ten jeste nebyl v teto session zadan, zobrazi okno pro jeho zadani
    // vraci FALSE pokud se v takovem pripade nepodari korektni master password zadat; ve vsech ostatnich pripadaech vraci TRUE
    // metodu je nutne zavolat vzdy pred volanim metod EncryptPassword/DecryptPassword, pokud je encrypt/encrypted == TRUE
    // pro zjednoduseni ji lze volat i v pripade, ze neni zapnute pouzivani master password (tise vrati TRUE)
    BOOL AskForMasterPassword(HWND hParent);

    void NotifyAboutMasterPasswordChange(HWND hParent);

    BOOL Save(HKEY hKey); // ulozi drzena hesla do Registry
    BOOL Load(HKEY hKey); // nacte hesla z Registry

    // 'encryptedPasswordSize' udava velikost bufferu, do ktereho bude ulozeno zasifrovane heslo; velikost musi byt o 50 znaku vetsi nez je delka 'plainPassword'

    // zasifruje plain text heslo do binarniho tvaru pomoci silne sifry (AES)
    // pred AES sifrovanim provede jeste scramble, kterym se pridava padding (posileni pro kratka hesla)
    // pokud volajici vyzaduje zasifrovani hesla pomoci AES ('encrypt'==TRUE), pred volanim metody musi zavolat AskForMasterPassword(), ktera musi vratit TRUE
    // 'plainPassword' je ukazatel na heslo v textove podobe, zakonecene nulou
    // 'encryptedPassword' vrati ukazatel na Salamanderem alokovany binarni buffer s zasifrovanym heslem; tento buffer je treba dealokovat pomoci CSalamanderGeneralAbstract::Free
    // 'encryptedPasswordSize' vrati velikost bufferu 'encryptedPassword' v bajtech
    // pokud je 'encrypt' TRUE, funkce ma heslo zasifrovat pomoci AES (bezpecne, chranene pomoci master password); pokud je FALSE, heslo bude pouze scramblene
    BOOL EncryptPassword(const char* plainPassword, BYTE** encryptedPassword, int* encryptedPasswordSize, BOOL encrypt);
    // 'plainPassword' je treba dealokovat pomoci CSalamanderGeneralAbstract::Free
    // pokud je 'plainPassword' NULL, pouze overi, zda lze heslo rozsifrovat
    BOOL DecryptPassword(const BYTE* encryptedPassword, int encryptedPasswordSize, char** plainPassword);
    // vraci TRUE, pokud jde o AES-sifrovane heslo, jinak vraci FALSE; rozhoduje se podle signatury na prvnim bajtu hesla
    BOOL IsPasswordEncrypted(const BYTE* encyptedPassword, int encyptedPasswordSize);

    // prida do pole Passwords nove heslo, vraci TRUE v pripade uspechu (zaroven naplni 'passwordID' hodnotou vetsi nez nula a mensi nez 0xffffffff), jinak FALSE
    // 'pluginDLLName' musi byt NULL, pokud heslo patri jadru Salamandera, jinak je plneno CPluginData
    // 'password' je heslo v otevrenem stavu
    //BOOL StorePassword(const char *pluginDLLName, const char *password, DWORD *passwordID); // volani musi predchazet uspesne AskForMasterPassword()
    //BOOL SetPassword(const char *pluginDLLName, DWORD passwordID, const char *password); // volani musi predchazet uspesne AskForMasterPassword()
    //BOOL GetPassword(const char *pluginDLLName, DWORD passwordID, char *password, int bufferLen); // volani musi predchazet uspesne AskForMasterPassword()
    //BOOL DeletePassword(const char *pluginDLLName, DWORD passwordID);

    // overi, zda 'password' odpovida heslu drzenemu v 'MasterPasswordVerifier'; vraci TRUE pokud odpovida, jinak FALSE
    BOOL VerifyMasterPassword(const char* password);

protected:
    // naalokuje a napocita 'MasterPasswordVerifier', ktery ukladame do registry pro naslednou verifikaci
    void CreateMasterPasswordVerifier(const char* password);
};

extern CPasswordManager PasswordManager;
