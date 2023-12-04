// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//----------------------------------------------------------------------------
#pragma once
//----------------------------------------------------------------------------
#include <vcl\System.hpp>
#include <vcl\Windows.hpp>
#include <vcl\SysUtils.hpp>
#include <vcl\Classes.hpp>
#include <vcl\Graphics.hpp>
#include <vcl\StdCtrls.hpp>
#include <vcl\Forms.hpp>
#include <vcl\Controls.hpp>
#include <vcl\Buttons.hpp>
#include <vcl\ExtCtrls.hpp>
#include <UpDownEdit.hpp>
#include <ComCtrls.hpp>
#include "CopyParams.h"
//----------------------------------------------------------------------------
class TPreferencesDialog : public TForm
{
    __published : TButton* OKBtn;
    TButton* CancelBtn;
    TPageControl* PageControl;
    TTabSheet* ConfirmationsSheet;
    TGroupBox* ConfirmationGroup;
    TCheckBox* DontConfirmDetachCheck;
    TCheckBox* ConfirmOverwritingCheck;
    TCheckBox* ConfirmResumeCheck;
    TTabSheet* TransferSheet;
    TTabSheet* QueueResumeSheet;
    TCopyParamsFrame* CopyParamsFrame;
    TGroupBox* QueueGroup;
    TCheckBox* QueueCheck;
    TCheckBox* RememberPasswordCheck;
    TGroupBox* ResumeBox;
    TLabel* ResumeThresholdUnitLabel;
    TRadioButton* ResumeOnButton;
    TRadioButton* ResumeSmartButton;
    TRadioButton* ResumeOffButton;
    TUpDownEdit* ResumeThresholdEdit;
    TCheckBox* ConfirmUploadCheck;
    TTabSheet* PanelSheet;
    TListView* ColumnsListView;
    TLabel* Label1;
    TButton* MoveUpButton;
    TButton* MoveDownButton;
    TCheckBox* QueueNoConfirmationCheck;
    TCheckBox* ConfirmCommandSessionCheck;
    TButton* HelpButton;
    TCheckBox* ConfirmExitOnCompletionCheck;
    void __fastcall ControlChange(TObject* Sender);
    void __fastcall FormShow(TObject* Sender);
    void __fastcall PageControlChange(TObject* Sender);
    void __fastcall ColumnsListViewDragOver(TObject* Sender, TObject* Source,
                                            int X, int Y, TDragState State, bool& Accept);
    void __fastcall ColumnsListViewDragDrop(TObject* Sender, TObject* Source,
                                            int X, int Y);
    void __fastcall ColumnsListViewChange(TObject* Sender, TListItem* Item,
                                          TItemChange Change);
    void __fastcall MoveUpButtonClick(TObject* Sender);
    void __fastcall MoveDownButtonClick(TObject* Sender);
    void __fastcall HelpButtonClick(TObject* Sender);

public:
    virtual __fastcall TPreferencesDialog(TComponent* AOwner);

    bool __fastcall Execute();

    __property TPreferencesMode PreferencesMode = {read = FPreferencesMode, write = SetPreferencesMode};

protected:
    virtual void __fastcall Dispatch(void* Message);

private:
    TPreferencesMode FPreferencesMode;

    void __fastcall UpdateControls();
    void __fastcall SetPreferencesMode(TPreferencesMode value);
    void __fastcall ColumnsListViewMoveTo(int Index);
    void __fastcall WMHelp(TWMHelp& Message);
};
//----------------------------------------------------------------------------
