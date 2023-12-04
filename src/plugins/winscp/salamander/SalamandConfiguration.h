// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
#include "CustomWinConfiguration.h"
//---------------------------------------------------------------------------
class CPluginInterface;
//---------------------------------------------------------------------------
const pcRights = 1;
const pcOwner = 2;
const pcGroup = 3;
const pcLinkTo = 4;
const pcLast = pcLinkTo;
extern const int ColumnNames[pcLast];
extern const int ColumnDescs[pcLast];
//---------------------------------------------------------------------------
class TSalamandConfiguration : public TCustomWinConfiguration
{
public:
    __fastcall TSalamandConfiguration(CPluginInterface* APlugin);

    virtual void __fastcall Default();
    virtual void __fastcall AskForMasterPasswordIfNotSet();
    void __fastcall RecryptPasswords();

    __property bool ConfirmDetach = {read = FConfirmDetach, write = FConfirmDetach};
    __property bool ConfirmUpload = {read = FConfirmUpload, write = FConfirmUpload};
    __property AnsiString QueueViewLayout = {read = FQueueViewLayout, write = FQueueViewLayout};
    __property AnsiString PanelColumns = {read = FPanelColumns, write = FPanelColumns};
    __property int LeftColumnWidth[int Index] = {read = GetColumnWidth, write = SetColumnWidth, index = 0};
    __property int RightColumnWidth[int Index] = {read = GetColumnWidth, write = SetColumnWidth, index = 1};
    __property int LeftColumnFixedWidth[int Index] = {read = GetColumnWidth, write = SetColumnWidth, index = 2};
    __property int RightColumnFixedWidth[int Index] = {read = GetColumnWidth, write = SetColumnWidth, index = 3};

    bool IteratePanelColumns(int& Column, bool& Show, void*& Iterator);

protected:
    virtual AnsiString __fastcall ModuleFileName();

    virtual void __fastcall SaveData(THierarchicalStorage* Storage, bool All);
    virtual void __fastcall LoadData(THierarchicalStorage* Storage);
    virtual bool __fastcall GetUseMasterPassword();
    virtual AnsiString __fastcall StronglyRecryptPassword(AnsiString Password, AnsiString Key);
    virtual AnsiString __fastcall DecryptPassword(AnsiString Password, AnsiString Key);

    __property AnsiString LeftColumnsWidths = {read = GetColumnsWidths, write = SetColumnsWidths, index = 0};
    __property AnsiString RightColumnsWidths = {read = GetColumnsWidths, write = SetColumnsWidths, index = 1};
    __property AnsiString LeftColumnsFixedWidths = {read = GetColumnsWidths, write = SetColumnsWidths, index = 2};
    __property AnsiString RightColumnsFixedWidths = {read = GetColumnsWidths, write = SetColumnsWidths, index = 3};

private:
    CPluginInterface* FPlugin;
    bool FConfirmDetach;
    bool FConfirmUpload;
    AnsiString FQueueViewLayout;
    AnsiString FPanelColumns;
    int FColumnsWidths[4][pcLast];

    int __fastcall GetColumnWidth(int Index, int Group);
    void __fastcall SetColumnWidth(int Index, int Group, int Value);
    AnsiString __fastcall GetColumnsWidths(int Group);
    void __fastcall SetColumnsWidths(int Group, AnsiString Value);
};
//---------------------------------------------------------------------------
#define SalamandConfiguration (dynamic_cast<TSalamandConfiguration*>(::Configuration))
//---------------------------------------------------------------------------
