//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include <Common.h>

#include "Authenticate.h"

#include <VCLCommon.h>
#include <TextsWin.h>
#include <Terminal.h>
#include <CoreMain.h>
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "PasswordEdit"
#ifndef NO_RESOURCES
#pragma resource "*.dfm"
#endif
//---------------------------------------------------------------------------
__fastcall TAuthenticateForm::TAuthenticateForm(TComponent * Owner)
  : TForm(Owner), FSessionData(NULL)
{
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Init(TSessionData * SessionData)
{
  FSessionData = SessionData;

  UseSystemSettings(this);
  FShowAsModalStorage = NULL;
  FFocusControl = NULL;

  FPromptParent = InstructionsLabel->Parent;
  FPromptLeft = InstructionsLabel->Left;
  FPromptTop = InstructionsLabel->Top;
  FPromptRight = FPromptParent->ClientWidth - InstructionsLabel->Width - FPromptLeft;
  FPromptEditGap = PromptEdit1->Top - PromptLabel1->Top - PromptLabel1->Height;
  FPromptsGap = PromptLabel2->Top - PromptEdit1->Top - PromptEdit1->Height;

  ClientHeight = 270;

  ClearLog();
}
//---------------------------------------------------------------------------
__fastcall TAuthenticateForm::~TAuthenticateForm()
{
  ReleaseAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::ShowAsModal()
{
  ::ShowAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::HideAsModal()
{
  ::HideAsModal(this, FShowAsModalStorage);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::WMNCCreate(TWMNCCreate & Message)
{
  // bypass TForm::WMNCCreate to prevent disabling "resize"
  // (which is done for bsDialog, see comments in CreateParams)
  DefaultHandler(&Message);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Dispatch(void * AMessage)
{
  TMessage & Message = *reinterpret_cast<TMessage *>(AMessage);
  if (Message.Msg == WM_NCCREATE)
  {
    WMNCCreate(*reinterpret_cast<TWMNCCreate *>(AMessage));
  }
  else
  {
    TForm::Dispatch(AMessage);
  }
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::CreateParams(TCreateParams & Params)
{
  TForm::CreateParams(Params);

  // Allow resizing of the window, even if this is bsDialog.
  // This makes it more close to bsSizeable, but bsSizeable cannot for some
  // reason receive focus, if window is shown atop non-main window
  // (like editor)
  Params.Style = Params.Style | WS_THICKFRAME;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::FormShow(TObject * /*Sender*/)
{
  AdjustControls();

  if (FFocusControl != NULL)
  {
    ActiveControl = FFocusControl;
  }
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::ClearLog()
{
  // TListItems::Clear() does nothing without allocated handle
  LogView->HandleNeeded();
  LogView->Items->Clear();
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Log(const AnsiString Message)
{
  TListItem * Item = LogView->Items->Add();
  Item->Caption = Message;
  Item->MakeVisible(false);
  LogView->Repaint();
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::AdjustControls()
{
  if (FStatus.IsEmpty())
  {
    Caption = FSessionData->SessionName;
  }
  else
  {
    Caption = FORMAT("%s - %s", (FStatus, FSessionData->SessionName));
  }
}
//---------------------------------------------------------------------------
TLabel * __fastcall TAuthenticateForm::GenerateLabel(int Current, AnsiString Caption)
{
  TLabel * Result = new TLabel(this);
  Result->Parent = FPromptParent;

  Result->Anchors = TAnchors() << akLeft << akTop << akRight;
  Result->WordWrap = true;
  Result->AutoSize = false;

  Result->Top = Current;
  Result->Left = FPromptLeft;
  Result->Caption = Caption;
  int Width = FPromptParent->ClientWidth - FPromptLeft - FPromptRight;
  Result->Width = Width;
  Result->AutoSize = true;

  return Result;
}
//---------------------------------------------------------------------------
TCustomEdit * __fastcall TAuthenticateForm::GenerateEdit(int Current, bool Echo, int MaxLen)
{
  TCustomEdit * Result = (Echo ? static_cast<TCustomEdit *>(new TEdit(this)) :
    static_cast<TCustomEdit *>(new TPasswordEdit(this)));
  Result->Parent = FPromptParent;

  Result->Anchors = TAnchors() << akLeft << akTop << akRight;
  Result->Top = Current;
  Result->Left = FPromptLeft;
  Result->Width = FPromptParent->ClientWidth - FPromptLeft - FPromptRight;
  ((TEdit *)Result)->MaxLength = MaxLen;

  return Result;
}
//---------------------------------------------------------------------------
TList * __fastcall TAuthenticateForm::GeneratePrompt(AnsiString Instructions,
  TStrings * Prompts, TStrings * Results)
{
  while (FPromptParent->ControlCount > 0)
  {
    delete FPromptParent->Controls[0];
  }
  TList * Result = new TList;

  int Current = FPromptTop;

  if (!Instructions.IsEmpty())
  {
    TLabel * Label = GenerateLabel(Current, Instructions);
    Current += Label->Height + FPromptsGap;
  }

  assert(Prompts->Count == Results->Count);
  for (int Index = 0; Index < Prompts->Count; Index++)
  {
    if (Index > 0)
    {
      Current += FPromptEditGap;
    }

    TLabel * Label = GenerateLabel(Current, Prompts->Strings[Index]);
    Current += Label->Height + FPromptEditGap;

    TCustomEdit * Edit = GenerateEdit(Current, bool(Prompts->Objects[Index]),
      int(Results->Objects[Index]));
    Result->Add(Edit);
    Label->FocusControl = Edit;
    Current += Edit->Height;
  }

  FPromptParent->ClientHeight = Current;

  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TAuthenticateForm::PromptUser(TPromptKind Kind, AnsiString Name,
  AnsiString Instructions, TStrings * Prompts, TStrings * Results, bool ForceLog,
  bool StoredCredentialsTried)
{

  bool Result;
  TList * Edits = GeneratePrompt(Instructions, Prompts, Results);

  try
  {
    bool ShowSavePasswordPanel = false;
    TSessionData * Data = NULL;
    if (((Kind == pkPassword) || (Kind == pkTIS) || (Kind == pkCryptoCard) ||
         (Kind == pkKeybInteractive)) &&
        (Prompts->Count == 1) && !bool(Prompts->Objects[0]) &&
        !FSessionData->Name.IsEmpty() &&
        StoredCredentialsTried)
    {
      Data = dynamic_cast<TSessionData *>(StoredSessions->FindByName(FSessionData->Name));
      ShowSavePasswordPanel = (Data != NULL) && !Data->Password.IsEmpty();
    }

    SavePasswordCheck->Checked = false;
    SavePasswordPanel->Visible = ShowSavePasswordPanel;

    if (PasswordPanel->AutoSize)
    {
      PasswordPanel->AutoSize = false;
      PasswordPanel->AutoSize = true;
    }
    PasswordPanel->Realign();

    assert(Results->Count == Edits->Count);
    for (int Index = 0; Index < Edits->Count; Index++)
    {
      TCustomEdit * Edit = reinterpret_cast<TCustomEdit *>(Edits->Items[Index]);
      Edit->Text = Results->Strings[Index];
    }

    Result = Execute(Name, PasswordPanel,
      ((Edits->Count > 0) ?
         reinterpret_cast<TWinControl *>(Edits->Items[0]) :
         static_cast<TWinControl *>(PasswordOKButton)),
      PasswordOKButton, PasswordCancelButton, true, false, ForceLog);
    if (Result)
    {
      for (int Index = 0; Index < Edits->Count; Index++)
      {
        TCustomEdit * Edit = reinterpret_cast<TCustomEdit *>(Edits->Items[Index]);
        Results->Strings[Index] = Edit->Text;
      }

      if (SavePasswordCheck->Checked)
      {
        assert(Data != NULL);
        assert(Results->Count >= 1);
        Data->Password = Results->Strings[0];
        // modified only, explicit
        StoredSessions->Save(false, true);
      }
    }
  }
  __finally
  {
    delete Edits;
  }

  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::Banner(const AnsiString & Banner,
  bool & NeverShowAgain, int Options)
{
  BannerMemo->Lines->Text = Banner;
  NeverShowAgainCheck->Visible = FLAGCLEAR(Options, boDisableNeverShowAgain);
  NeverShowAgainCheck->Checked = NeverShowAgain;
  bool Result = Execute(LoadStr(AUTHENTICATION_BANNER), BannerPanel, BannerCloseButton,
    BannerCloseButton, BannerCloseButton, false, true, false);
  if (Result)
  {
    NeverShowAgain = NeverShowAgainCheck->Checked;
  }
}
//---------------------------------------------------------------------------
bool __fastcall TAuthenticateForm::Execute(AnsiString Status, TPanel * Panel,
  TWinControl * FocusControl, TButton * DefaultButton, TButton * CancelButton,
  bool FixHeight, bool Zoom, bool ForceLog)
{
  TAlign Align = Panel->Align;
  try
  {
    assert(FStatus.IsEmpty());
    FStatus = Status;
    DefaultButton->Default = true;
    CancelButton->Cancel = true;

    if (Zoom)
    {
      Panel->Align = alClient;
    }

    if (ForceLog || Visible)
    {
      if (ClientHeight < Panel->Height)
      {
        ClientHeight = Panel->Height;
      }
      // Panel being hidden gets not realigned automatically, even if it
      // has Align property set
      Panel->Top = ClientHeight - Panel->Height;
      Panel->Show();
      TCursor PrevCursor = Screen->Cursor;
      try
      {
        if (Zoom)
        {
          LogView->Hide();
        }
        else
        {
          if (LogView->Items->Count > 0)
          {
            TListItem * Item = LogView->ItemFocused;
            if (Item == NULL)
            {
              Item = LogView->Items->Item[LogView->Items->Count - 1];
            }
            Item->MakeVisible(false);
          }
        }
        Screen->Cursor = crDefault;

        if (!Visible)
        {
          assert(ForceLog);
          ShowAsModal();
        }

        ActiveControl = FocusControl;
        ModalResult = mrNone;
        AdjustControls();
        do
        {
          Application->HandleMessage();
        }
        while (!Application->Terminated && (ModalResult == mrNone));
      }
      __finally
      {
        Panel->Hide();
        Screen->Cursor = PrevCursor;
        if (Zoom)
        {
          LogView->Show();
        }
        Repaint();
      }
    }
    else
    {
      int PrevHeight = ClientHeight;
      int PrevMinHeight = Constraints->MinHeight;
      int PrevMaxHeight = Constraints->MaxHeight;
      try
      {
        Constraints->MinHeight = 0;
        ClientHeight = Panel->Height;
        if (FixHeight)
        {
          Constraints->MinHeight = Height;
          Constraints->MaxHeight = Height;
        }
        LogView->Hide();
        Panel->Show();
        FFocusControl = FocusControl;

        ShowModal();
      }
      __finally
      {
        FFocusControl = NULL;
        ClientHeight = PrevHeight;
        Constraints->MinHeight = PrevMinHeight;
        Constraints->MaxHeight = PrevMaxHeight;
        Panel->Hide();
        LogView->Show();
      }
    }
  }
  __finally
  {
    Panel->Align = Align;
    DefaultButton->Default = false;
    CancelButton->Cancel = false;
    FStatus = "";
    AdjustControls();
  }

  return (ModalResult != mrCancel);
}
//---------------------------------------------------------------------------
void __fastcall TAuthenticateForm::HelpButtonClick(TObject * /*Sender*/)
{
  FormHelp(this);
}
//---------------------------------------------------------------------------
