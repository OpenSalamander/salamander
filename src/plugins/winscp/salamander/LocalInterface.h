// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------------
#pragma once
//---------------------------------------------------------------------------
class TSalamandForm : public TForm
{
public:
    __fastcall virtual TSalamandForm(TComponent* AOwner);
    __fastcall TSalamandForm(TComponent* AOwner, int Dummy);

protected:
    DYNAMIC void __fastcall DoShow(void);

private:
    void LocalSystemSettings();
    TForm* FPreviousForm;
};
//---------------------------------------------------------------------------
#define TForm TSalamandForm
//---------------------------------------------------------------------------
