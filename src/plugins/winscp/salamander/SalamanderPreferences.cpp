// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//---------------------------------------------------------------------
#define NO_WIN32_LEAN_AND_MEAN
#include <vcl.h>
#pragma hdrstop

#include "Salamander.h"
#include "WinInterface.h"
#include "SalamanderPreferences.h"
#include "SalamandConfiguration.h"
#include "VCLCommon.h"
#include <CoreMain.h>
//---------------------------------------------------------------------
#pragma link "CopyParams"
#ifndef NO_RESOURCES
#pragma resource "*.dfm"
#endif
//---------------------------------------------------------------------
bool __fastcall DoPreferencesDialog(TPreferencesMode APreferencesMode,
                                    TPreferencesDialogData* /*DialogData*/)
{
    bool Result;
    TPreferencesDialog* PreferencesDialog = new TPreferencesDialog(Application);
    try
    {
        PreferencesDialog->PreferencesMode = APreferencesMode;
        Result = PreferencesDialog->Execute();
    }
    __finally
    {
        delete PreferencesDialog;
    }
    return Result;
}
//---------------------------------------------------------------------
__fastcall TPreferencesDialog::TPreferencesDialog(TComponent* AOwner)
    : TForm(AOwner)
{
    FPreferencesMode = pmDefault;
    UseSystemSettings(this);
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::SetPreferencesMode(TPreferencesMode value)
{
    if (PreferencesMode != value)
    {
        assert((value == pmDefault) || (value == pmTransfer));

        FPreferencesMode = value;
    }
}
//---------------------------------------------------------------------
bool __fastcall TPreferencesDialog::Execute()
{
    DontConfirmDetachCheck->Checked = !SalamandConfiguration->ConfirmDetach;
    ConfirmOverwritingCheck->Checked = Configuration->ConfirmOverwriting;
    ConfirmResumeCheck->Checked = Configuration->ConfirmResume;
    ConfirmExitOnCompletionCheck->Checked = CustomWinConfiguration->ConfirmExitOnCompletion;
    ConfirmCommandSessionCheck->Checked = GUIConfiguration->ConfirmCommandSession;
    ConfirmUploadCheck->Checked = SalamandConfiguration->ConfirmUpload;

    QueueCheck->Checked = GUIConfiguration->DefaultCopyParam.Queue;
    QueueNoConfirmationCheck->Checked = GUIConfiguration->DefaultCopyParam.QueueNoConfirmation;
    RememberPasswordCheck->Checked = GUIConfiguration->QueueRememberPassword;

    ResumeOnButton->Checked = GUIConfiguration->DefaultCopyParam.ResumeSupport == rsOn;
    ResumeSmartButton->Checked = GUIConfiguration->DefaultCopyParam.ResumeSupport == rsSmart;
    ResumeOffButton->Checked = GUIConfiguration->DefaultCopyParam.ResumeSupport == rsOff;
    ResumeThresholdEdit->Value = GUIConfiguration->DefaultCopyParam.ResumeThreshold / 1024;

    CopyParamsFrame->Params = GUIConfiguration->DefaultCopyParam;

    // keep own count as ColumnsListView->Items->Count report 0 when the
    // control (its tab) was never shown
    int ColumnsListViewCount = 0;
    void* Iterator = NULL;
    int Column;
    bool Show;
    ColumnsListView->Clear();
    while (SalamandConfiguration->IteratePanelColumns(Column, Show, Iterator))
    {
        TListItem* Item = ColumnsListView->Items->Add();
        Item->Caption = LoadStr(ColumnNames[Column - 1]);
        Item->Checked = Show;
        Item->Data = reinterpret_cast<void*>(Column);
        ColumnsListViewCount++;
        if (ColumnsListViewCount == 1)
        {
            Item->Focused = true;
            Item->Selected = true;
        }
    }

    UpdateControls();

    CopyParamsFrame->BeforeExecute();

    bool Result = (ShowModal() != mrCancel);
    if (Result)
    {
        CopyParamsFrame->AfterExecute();

        TGUICopyParamType CopyParam = GUIConfiguration->DefaultCopyParam;

        SalamandConfiguration->ConfirmDetach = !DontConfirmDetachCheck->Checked;
        Configuration->ConfirmOverwriting = ConfirmOverwritingCheck->Checked;
        Configuration->ConfirmResume = ConfirmResumeCheck->Checked;
        CustomWinConfiguration->ConfirmExitOnCompletion = ConfirmExitOnCompletionCheck->Checked;
        GUIConfiguration->ConfirmCommandSession = ConfirmCommandSessionCheck->Checked;
        SalamandConfiguration->ConfirmUpload = ConfirmUploadCheck->Checked;

        CopyParam.Queue = QueueCheck->Checked;
        CopyParam.QueueNoConfirmation = QueueNoConfirmationCheck->Checked;
        GUIConfiguration->QueueRememberPassword = RememberPasswordCheck->Checked;

        // overwrites only TCopyParamType fields
        CopyParam = CopyParamsFrame->Params;
        if (ResumeOnButton->Checked)
            CopyParam.ResumeSupport = rsOn;
        if (ResumeSmartButton->Checked)
            CopyParam.ResumeSupport = rsSmart;
        if (ResumeOffButton->Checked)
            CopyParam.ResumeSupport = rsOff;
        CopyParam.ResumeThreshold = ResumeThresholdEdit->Value * 1024;

        GUIConfiguration->DefaultCopyParam = CopyParam;

        // ColumnsListView->Items is empty when the control (its tab) was never shown
        if (ColumnsListView->Items->Count > 0)
        {
            AnsiString PanelColumns;
            for (int i = 0; i < ColumnsListViewCount; i++)
            {
                if (!PanelColumns.IsEmpty())
                {
                    PanelColumns += ";";
                }
                TListItem* Item = ColumnsListView->Items->Item[i];
                PanelColumns += IntToStr(reinterpret_cast<int>(Item->Data)) + "," +
                                IntToStr(Item->Checked);
            }
            SalamandConfiguration->PanelColumns = PanelColumns;
        }
    }

    return Result;
}
//---------------------------------------------------------------------
void __fastcall TPreferencesDialog::UpdateControls()
{
    EnableControl(ResumeThresholdEdit, ResumeSmartButton->Checked);
    EnableControl(MoveUpButton, (ColumnsListView->ItemFocused != NULL) &&
                                    (ColumnsListView->ItemFocused->Index > 0));
    EnableControl(MoveDownButton, (ColumnsListView->ItemFocused != NULL) &&
                                      (ColumnsListView->ItemFocused->Index < ColumnsListView->Items->Count - 1));
}
//---------------------------------------------------------------------
void __fastcall TPreferencesDialog::ControlChange(TObject* /*Sender*/)
{
    UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::FormShow(TObject* /*Sender*/)
{
    switch (PreferencesMode)
    {
    case pmTransfer:
        PageControl->ActivePage = TransferSheet;
        break;

    default:
        PageControl->ActivePage = ConfirmationsSheet;
        break;
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::PageControlChange(TObject* /*Sender*/)
{
    ResetSystemSettings(this);
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::ColumnsListViewDragOver(
    TObject* Sender, TObject* Source, int /*X*/, int /*Y*/, TDragState /*State*/,
    bool& Accept)
{
    if (Source == Sender)
    {
        Accept = (ColumnsListView->ItemFocused != ColumnsListView->DropTarget);
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::ColumnsListViewDragDrop(
    TObject* Sender, TObject* Source, int /*X*/, int /*Y*/)
{
    if ((Source == Sender) &&
        (ColumnsListView->DropTarget != NULL) &&
        (ColumnsListView->ItemFocused != ColumnsListView->DropTarget))
    {
        ColumnsListViewMoveTo(ColumnsListView->DropTarget->Index);
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::ColumnsListViewMoveTo(int Index)
{
    AnsiString Caption = ColumnsListView->ItemFocused->Caption;
    bool Checked = ColumnsListView->ItemFocused->Checked;
    void* Data = ColumnsListView->ItemFocused->Data;
    ColumnsListView->ItemFocused->Delete();
    TListItem* NewItem = ColumnsListView->Items->Insert(Index);
    NewItem->Caption = Caption;
    NewItem->Checked = Checked;
    NewItem->Data = Data;
    NewItem->Focused = true;
    NewItem->Selected = true;
    ColumnsListView->SetFocus();
    UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::ColumnsListViewChange(TObject* /*Sender*/,
                                                          TListItem* /*Item*/, TItemChange /*Change*/)
{
    UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::MoveUpButtonClick(TObject* /*Sender*/)
{
    if (ColumnsListView->ItemIndex > 0)
    {
        ColumnsListViewMoveTo(ColumnsListView->ItemIndex - 1);
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::MoveDownButtonClick(TObject* /*Sender*/)
{
    if (ColumnsListView->ItemIndex < ColumnsListView->Items->Count - 1)
    {
        ColumnsListViewMoveTo(ColumnsListView->ItemIndex + 1);
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::Dispatch(void* Message)
{
    TMessage* M = reinterpret_cast<TMessage*>(Message);
    assert(M);
    if (M->Msg == WM_HELP)
    {
        WMHelp(*((TWMHelp*)Message));
    }
    else
    {
        TForm::Dispatch(Message);
    }
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::WMHelp(TWMHelp& Message)
{
    assert(Message.HelpInfo != NULL);

    if (Message.HelpInfo->iContextType == HELPINFO_WINDOW)
    {
        // invoke help for active page (not for whole form), regardless of focus
        // (e.g. even if focus is on control outside pagecontrol)
        Message.HelpInfo->hItemHandle = PageControl->ActivePage->Handle;
    }
    TForm::Dispatch(&Message);
}
//---------------------------------------------------------------------------
void __fastcall TPreferencesDialog::HelpButtonClick(TObject* /*Sender*/)
{
    FormHelp(this);
}
//---------------------------------------------------------------------------
