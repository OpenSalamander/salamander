object PreferencesDialog: TPreferencesDialog
  Left = 372
  Top = 204
  HelpType = htKeyword
  HelpKeyword = 'ui_sal_preferences'
  BorderStyle = bsDialog
  Caption = 'WinSCP Configuration'
  ClientHeight = 427
  ClientWidth = 380
  Color = clBtnFace
  ParentFont = True
  OldCreateOrder = True
  Position = poScreenCenter
  OnShow = FormShow
  DesignSize = (
    380
    427)
  PixelsPerInch = 96
  TextHeight = 13
  object HelpButton: TButton
    Left = 297
    Top = 393
    Width = 75
    Height = 25
    Anchors = [akRight, akBottom]
    Caption = 'Help'
    TabOrder = 3
    OnClick = HelpButtonClick
  end
  object PageControl: TPageControl
    Left = 0
    Top = 0
    Width = 380
    Height = 384
    HelpType = htKeyword
    ActivePage = ConfirmationsSheet
    Align = alTop
    Anchors = [akLeft, akTop, akRight, akBottom]
    TabIndex = 0
    TabOrder = 0
    OnChange = PageControlChange
    object ConfirmationsSheet: TTabSheet
      HelpType = htKeyword
      HelpKeyword = 'ui_sal_pref_confirmations'
      Caption = 'Confirmations'
      DesignSize = (
        372
        356)
      object ConfirmationGroup: TGroupBox
        Left = 5
        Top = 5
        Width = 362
        Height = 173
        Anchors = [akLeft, akTop, akRight]
        Caption = 'Confirmations'
        TabOrder = 0
        DesignSize = (
          362
          173)
        object DontConfirmDetachCheck: TCheckBox
          Left = 16
          Top = 24
          Width = 338
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = 'Always &keep connection, when leaving SFTP/SCP panel'
          TabOrder = 0
        end
        object ConfirmOverwritingCheck: TCheckBox
          Left = 16
          Top = 48
          Width = 330
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = '&Overwriting of files'
          TabOrder = 1
        end
        object ConfirmResumeCheck: TCheckBox
          Left = 16
          Top = 72
          Width = 330
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = 'Transfer &resuming'
          TabOrder = 2
        end
        object ConfirmUploadCheck: TCheckBox
          Left = 16
          Top = 144
          Width = 330
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = 'Display dialog box with options before &uploading'
          TabOrder = 5
          OnClick = ControlChange
        end
        object ConfirmCommandSessionCheck: TCheckBox
          Left = 16
          Top = 120
          Width = 330
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = 'Opening separate &shell session'
          TabOrder = 4
          OnClick = ControlChange
        end
        object ConfirmExitOnCompletionCheck: TCheckBox
          Left = 16
          Top = 96
          Width = 325
          Height = 17
          Anchors = [akLeft, akTop, akRight]
          Caption = 'Disconnect acknowledgement on o&peration completion'
          TabOrder = 3
          OnClick = ControlChange
        end
      end
    end
    object TransferSheet: TTabSheet
      HelpType = htKeyword
      HelpKeyword = 'ui_sal_pref_transfer'
      Caption = 'Transfer'
      ImageIndex = 1
      inline CopyParamsFrame: TCopyParamsFrame
        Left = -3
        Top = -3
        Width = 529
        Height = 356
        HelpType = htKeyword
        TabOrder = 0
      end
    end
    object QueueResumeSheet: TTabSheet
      HelpType = htKeyword
      HelpKeyword = 'ui_sal_pref_queue'
      Caption = 'Background && Resume'
      ImageIndex = 2
      DesignSize = (
        372
        356)
      object QueueGroup: TGroupBox
        Left = 5
        Top = 5
        Width = 362
        Height = 100
        Anchors = [akLeft, akTop, akRight]
        Caption = 'Background transfers'
        TabOrder = 0
        object QueueCheck: TCheckBox
          Left = 16
          Top = 24
          Width = 337
          Height = 17
          Caption = '&Transfer on background by default'
          TabOrder = 0
        end
        object RememberPasswordCheck: TCheckBox
          Left = 16
          Top = 72
          Width = 337
          Height = 17
          Caption = 'Remember &password of main session for background transfers'
          TabOrder = 2
        end
        object QueueNoConfirmationCheck: TCheckBox
          Left = 16
          Top = 48
          Width = 337
          Height = 17
          Caption = 'No &confirmations when transferring on background'
          TabOrder = 1
        end
      end
      object ResumeBox: TGroupBox
        Left = 5
        Top = 112
        Width = 362
        Height = 98
        Caption = 'Enable transfer resume/transfer to temporary filename for'
        TabOrder = 1
        object ResumeThresholdUnitLabel: TLabel
          Left = 208
          Top = 48
          Width = 16
          Height = 13
          Caption = 'KiB'
          FocusControl = ResumeThresholdEdit
        end
        object ResumeOnButton: TRadioButton
          Left = 11
          Top = 23
          Width = 334
          Height = 17
          Caption = 'A&ll files'
          TabOrder = 0
          OnClick = ControlChange
        end
        object ResumeSmartButton: TRadioButton
          Left = 11
          Top = 47
          Width = 102
          Height = 17
          Caption = 'Files abo&ve'
          TabOrder = 1
          OnClick = ControlChange
        end
        object ResumeOffButton: TRadioButton
          Left = 12
          Top = 71
          Width = 333
          Height = 17
          Caption = 'Di&sable'
          TabOrder = 3
          OnClick = ControlChange
        end
        object ResumeThresholdEdit: TUpDownEdit
          Left = 117
          Top = 44
          Width = 84
          Height = 21
          Alignment = taRightJustify
          Increment = 10
          MaxValue = 4194304
          TabOrder = 2
        end
      end
    end
    object PanelSheet: TTabSheet
      HelpType = htKeyword
      HelpKeyword = 'ui_sal_pref_panel'
      Caption = 'Panel'
      ImageIndex = 3
      DesignSize = (
        372
        356)
      object Label1: TLabel
        Left = 8
        Top = 8
        Width = 80
        Height = 13
        Caption = '&Columns in panel'
        FocusControl = ColumnsListView
      end
      object ColumnsListView: TListView
        Left = 8
        Top = 24
        Width = 273
        Height = 89
        Checkboxes = True
        Columns = <
          item
            AutoSize = True
          end>
        DragMode = dmAutomatic
        HideSelection = False
        ReadOnly = True
        ShowColumnHeaders = False
        TabOrder = 0
        ViewStyle = vsReport
        OnChange = ColumnsListViewChange
        OnDragDrop = ColumnsListViewDragDrop
        OnDragOver = ColumnsListViewDragOver
      end
      object MoveUpButton: TButton
        Left = 288
        Top = 33
        Width = 75
        Height = 25
        Anchors = [akLeft, akBottom]
        Caption = '&Up'
        TabOrder = 1
        OnClick = MoveUpButtonClick
      end
      object MoveDownButton: TButton
        Left = 288
        Top = 66
        Width = 75
        Height = 25
        Anchors = [akLeft, akBottom]
        Caption = '&Down'
        TabOrder = 2
        OnClick = MoveDownButtonClick
      end
    end
  end
  object OKBtn: TButton
    Left = 137
    Top = 393
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Caption = 'OK'
    Default = True
    ModalResult = 1
    TabOrder = 1
  end
  object CancelBtn: TButton
    Left = 217
    Top = 393
    Width = 75
    Height = 25
    Anchors = [akLeft, akBottom]
    Cancel = True
    Caption = 'Cancel'
    ModalResult = 2
    TabOrder = 2
  end
end
