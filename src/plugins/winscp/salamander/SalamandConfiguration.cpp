// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "Salamand.h"
#include "SalamandConfiguration.h"
#include "Salamander.rh"

#include <Common.h>
#include <Security.h>
#include <TextsWin.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
//---------------------------------------------------------------------------
const int ColumnNames[] =
    {SAL_RIGHTS_COL, SAL_OWNER_COL, SAL_GROUP_COL, SAL_LINK_TO_COL};
const int ColumnDescs[] =
    {SAL_RIGHTS_DESC, SAL_OWNER_DESC, SAL_GROUP_DESC, SAL_LINK_TO_DESC};
#define PWDMNGR_SIGNATURE_ENCRYPTED '\x02'
//---------------------------------------------------------------------------
__fastcall TSalamandConfiguration::TSalamandConfiguration(
    CPluginInterface* APlugin) : TCustomWinConfiguration()
{
    FPlugin = APlugin;
    Default();
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::Default()
{
    TCustomWinConfiguration::Default();

    OperationProgressOnTop = false;

    FConfirmDetach = true;
    FConfirmUpload = true;
    FQueueViewLayout = "70,170,170,80,80";
    FPanelColumns = "1,1;2,1;3,1;4,0";
    memset(FColumnsWidths, 0, sizeof(FColumnsWidths));
}
//---------------------------------------------------------------------------
// duplicated from core\configuration.cpp
#define LASTELEM(ELEM) \
    ELEM.SubString(ELEM.LastDelimiter(".>") + 1, ELEM.Length() - ELEM.LastDelimiter(".>"))
#define BLOCK(KEY, CANCREATE, BLOCK) \
    if (Storage->OpenSubKey(KEY, CANCREATE, true)) \
        try \
        { \
            BLOCK \
        } \
        __finally \
        { \
            Storage->CloseSubKey(); \
        }
#define REGCONFIG(CANCREATE) \
    BLOCK("Salamander", CANCREATE, \
          KEY(Bool, ConfirmDetach); \
          KEY(Bool, ConfirmUpload); \
          KEY(String, QueueViewLayout); \
          KEY(String, PanelColumns); \
          KEY(String, LeftColumnsWidths); \
          KEY(String, RightColumnsWidths); \
          KEY(String, LeftColumnsFixedWidths); \
          KEY(String, RightColumnsFixedWidths););
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::SaveData(THierarchicalStorage* Storage,
                                                 bool All)
{
    TCustomWinConfiguration::SaveData(Storage, All);

    assert(!PanelColumns.IsEmpty());

// duplicated from core\configuration.cpp
#define KEY(TYPE, VAR) Storage->Write##TYPE(LASTELEM(AnsiString(#VAR)), VAR)
    REGCONFIG(true);
#undef KEY
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::LoadData(THierarchicalStorage* Storage)
{
    TCustomWinConfiguration::LoadData(Storage);

// duplicated from core\configuration.cpp
#define KEY(TYPE, VAR) VAR = Storage->Read##TYPE(LASTELEM(AnsiString(#VAR)), VAR)
#pragma warn - eas
    REGCONFIG(false);
#pragma warn + eas
#undef KEY

    bool Columns[pcLast];
    memset(&Columns, 0, sizeof(Columns));

    void* Iterator = NULL;
    int Column;
    bool Show;
    while (IteratePanelColumns(Column, Show, Iterator))
    {
        Columns[Column - 1] = true;
    }

    for (Column = 1; Column <= LENOF(Columns); Column++)
    {
        if (!Columns[Column - 1])
        {
            if (!FPanelColumns.IsEmpty())
            {
                FPanelColumns += ";";
            }
            FPanelColumns += FORMAT("%d,%d", (Column, 0));
        }
    }
}
//---------------------------------------------------------------------------
bool TSalamandConfiguration::IteratePanelColumns(int& Column, bool& Show,
                                                 void*& Iterator)
{
    AnsiString* Columns;
    if (Iterator == NULL)
    {
        Columns = new AnsiString(PanelColumns);
        Iterator = Columns;
    }
    else
    {
        Columns = static_cast<AnsiString*>(Iterator);
    }

    bool Result;

    do
    {
        if (Columns->IsEmpty())
        {
            delete Columns;
            Iterator = NULL;
            Result = false;
        }
        else
        {
            try
            {
                AnsiString ColStr = CutToChar(*Columns, ';', true);
                Column = StrToInt(CutToChar(ColStr, ',', true));
                Show = StrToInt(CutToChar(ColStr, ',', true));
                Result = true;
            }
            catch (...)
            {
                Result = false;
            }
        }
    } while (Result && ((Column < 1) || (Column > pcLast)));

    return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TSalamandConfiguration::ModuleFileName()
{
    char FileName[MAX_PATH];
    if (GetModuleFileName(FPlugin->GetDLLInstance(), FileName, sizeof(FileName)) == 0)
    {
        assert(false);
    }
    return AnsiString(FileName);
}
//---------------------------------------------------------------------------
int __fastcall TSalamandConfiguration::GetColumnWidth(int Index, int Group)
{
    return FColumnsWidths[Group][Index - 1];
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::SetColumnWidth(int Index, int Group, int Value)
{
    FColumnsWidths[Group][Index - 1] = Value;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TSalamandConfiguration::GetColumnsWidths(int Group)
{
    AnsiString Result;
    for (int Index = 0; Index < pcLast; Index++)
    {
        if (Index > 0)
        {
            Result += ";";
        }
        Result += IntToStr(FColumnsWidths[Group][Index]);
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::SetColumnsWidths(int Group, AnsiString Value)
{
    memset(FColumnsWidths[Group], 0, sizeof(FColumnsWidths[Group]));
    int Index = 0;
    while (!Value.IsEmpty() && (Index < pcLast))
    {
        FColumnsWidths[Group][Index] = StrToIntDef(CutToChar(Value, ';', true), 0);
        Index++;
    }
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::RecryptPasswords()
{
    TCustomWinConfiguration::RecryptPasswords();
    FPlugin->RecryptPasswords();
}
//---------------------------------------------------------------------------
bool __fastcall TSalamandConfiguration::GetUseMasterPassword()
{
    return FPlugin->GetSalamanderGeneral()->GetSalamanderPasswordManager()->IsUsingMasterPassword();
}
//---------------------------------------------------------------------------
AnsiString __fastcall TSalamandConfiguration::StronglyRecryptPassword(AnsiString Password, AnsiString Key)
{
    AnsiString Dummy;
    AnsiString Result;
    CSalamanderPasswordManagerAbstract* PasswordManager = FPlugin->GetSalamanderGeneral()->GetSalamanderPasswordManager();
    if (GetExternalEncryptedPassword(Password, Dummy) ||
        !PasswordManager->IsMasterPasswordSet())
    {
        // already-strongly encrypted
        // or no master password set, so we cannot strongly-encrypt it
        Result = Password;
    }
    else
    {
        Password = TCustomWinConfiguration::DecryptPassword(Password, Key);
        if (!Password.IsEmpty())
        {
            BYTE* Encrypted;
            int EncryptedSize;
            if (!PasswordManager->EncryptPassword(Password.c_str(), &Encrypted, &EncryptedSize, TRUE))
            {
                assert(false);
                Abort();
            }
            Result = AnsiString(reinterpret_cast<char*>(Encrypted), EncryptedSize);
            assert(Result.Length() > 1);
            assert(Result[1] == PWDMNGR_SIGNATURE_ENCRYPTED);
            Result.Delete(1, 1);
            FPlugin->GetSalamanderGeneral()->Free(Encrypted);
            Result = SetExternalEncryptedPassword(Result);
        }
    }
    return Result;
}
//---------------------------------------------------------------------------
AnsiString __fastcall TSalamandConfiguration::DecryptPassword(AnsiString Password, AnsiString Key)
{
    AnsiString Result;
    if (GetExternalEncryptedPassword(Password, Result))
    {
        CSalamanderPasswordManagerAbstract* PasswordManager = FPlugin->GetSalamanderGeneral()->GetSalamanderPasswordManager();
        bool HaveMasterPassword = true;
        if (!PasswordManager->IsMasterPasswordSet())
        {
            if (!PasswordManager->AskForMasterPassword(FPlugin->ParentWindow()))
            {
                // prompt for master password was either cancelled, then we abort
                // below after decrupting fails.
                // or we are in process of clearing master password (AskForMasterPassword
                // automatically fails) and decrypting below should succeed
                HaveMasterPassword = false;
            }
        }
        Result.Insert(PWDMNGR_SIGNATURE_ENCRYPTED, 1);
        char* Decrypted;
        if (!PasswordManager->DecryptPassword(
                reinterpret_cast<unsigned char*>(Result.c_str()), Result.Length(), &Decrypted))
        {
            if (HaveMasterPassword)
            {
                throw Exception(LoadStr(DECRYPT_PASSWORD_ERROR));
            }
            else
            {
                Abort();
            }
        }
        Result = reinterpret_cast<char*>(Decrypted);
        FPlugin->GetSalamanderGeneral()->Free(Decrypted);
    }
    else
    {
        Result = TCustomWinConfiguration::DecryptPassword(Password, Key);
    }
    return Result;
}
//---------------------------------------------------------------------------
void __fastcall TSalamandConfiguration::AskForMasterPasswordIfNotSet()
{
    CSalamanderPasswordManagerAbstract* PasswordManager = FPlugin->GetSalamanderGeneral()->GetSalamanderPasswordManager();
    if (!PasswordManager->IsMasterPasswordSet() &&
        !PasswordManager->AskForMasterPassword(FPlugin->ParentWindow()))
    {
        Abort();
    }
}
