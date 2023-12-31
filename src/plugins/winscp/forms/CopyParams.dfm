object CopyParamsFrame: TCopyParamsFrame
  Left = 0
  Top = 0
  Width = 377
  Height = 355
  HelpType = htKeyword
  TabOrder = 0
  object CommonPropertiesGroup: TGroupBox
    Left = 192
    Top = 156
    Width = 178
    Height = 95
    Caption = 'Common options'
    TabOrder = 3
    DesignSize = (
      178
      95)
    object SpeedLabel2: TLabel
      Left = 11
      Top = 68
      Width = 69
      Height = 13
      Caption = '&Speed (KiB/s):'
      FocusControl = SpeedCombo
    end
    object PreserveTimeCheck: TCheckBox
      Left = 12
      Top = 19
      Width = 161
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Preserve timesta&mp'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnClick = ControlChange
    end
    object CommonCalculateSizeCheck: TCheckBox
      Left = 12
      Top = 44
      Width = 161
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&Calculate total size'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      OnClick = ControlChange
    end
    object SpeedCombo: THistoryComboBox
      Left = 89
      Top = 64
      Width = 80
      Height = 21
      AutoComplete = False
      ItemHeight = 13
      TabOrder = 2
      Text = 'SpeedCombo'
      OnExit = SpeedComboExit
      Items.Strings = (
        'Unlimited'
        '1024'
        '512'
        '256'
        '128'
        '64'
        '32'
        '16'
        '8')
    end
  end
  object LocalPropertiesGroup: TGroupBox
    Left = 192
    Top = 254
    Width = 178
    Height = 43
    Caption = 'Download options'
    TabOrder = 4
    DesignSize = (
      178
      43)
    object PreserveReadOnlyCheck: TCheckBox
      Left = 12
      Top = 17
      Width = 160
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Preserve rea&d-only'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
    end
  end
  object RemotePropertiesGroup: TGroupBox
    Left = 8
    Top = 156
    Width = 177
    Height = 141
    Caption = 'Upload options'
    TabOrder = 2
    object PreserveRightsCheck: TCheckBox
      Left = 12
      Top = 18
      Width = 156
      Height = 17
      Caption = 'Set pe&rmissions:'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 0
      OnClick = ControlChange
    end
    object RightsEdit: TComboEdit
      Left = 30
      Top = 39
      Width = 109
      Height = 21
      ButtonHint = 'Configure permissions'
      ClickKey = 16397
      ParentShowHint = False
      ShowHint = True
      TabOrder = 1
      Text = 'RightsEdit'
      OnButtonClick = RightsEditButtonClick
      OnExit = RightsEditExit
      OnContextPopup = RightsEditContextPopup
    end
    object IgnorePermErrorsCheck: TCheckBox
      Left = 12
      Top = 67
      Width = 156
      Height = 17
      Caption = 'Ign&ore permission errors'
      ParentShowHint = False
      ShowHint = True
      TabOrder = 2
      OnClick = ControlChange
    end
    object ClearArchiveCheck: TCheckBox
      Left = 12
      Top = 94
      Width = 156
      Height = 17
      Caption = 'Clear '#39'Archi&ve'#39' attribute'
      TabOrder = 3
    end
  end
  object ChangeCaseGroup: TGroupBox
    Left = 247
    Top = 8
    Width = 123
    Height = 146
    Caption = 'Filename modification'
    TabOrder = 1
    DesignSize = (
      123
      146)
    object CCLowerCaseShortButton: TRadioButton
      Left = 8
      Top = 94
      Width = 110
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Lower case &8.3'
      TabOrder = 3
    end
    object CCNoChangeButton: TRadioButton
      Left = 8
      Top = 19
      Width = 110
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&No change'
      TabOrder = 0
    end
    object CCUpperCaseButton: TRadioButton
      Left = 8
      Top = 44
      Width = 110
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&Upper case'
      TabOrder = 1
    end
    object CCLowerCaseButton: TRadioButton
      Left = 8
      Top = 69
      Width = 110
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Lo&wer case'
      TabOrder = 2
    end
    object ReplaceInvalidCharsCheck: TCheckBox
      Left = 8
      Top = 120
      Width = 105
      Height = 17
      Caption = 'Replace '#39'\:*?'#39' ...'
      TabOrder = 4
      OnClick = ControlChange
    end
  end
  object TransferModeGroup: TGroupBox
    Left = 8
    Top = 8
    Width = 231
    Height = 146
    Caption = 'Transfer mode'
    TabOrder = 0
    DesignSize = (
      231
      146)
    object AsciiFileMaskLabel: TLabel
      Left = 10
      Top = 99
      Width = 167
      Height = 13
      Anchors = [akLeft, akTop, akRight]
      Caption = 'Transfer following &files in text mode:'
      FocusControl = AsciiFileMaskCombo
    end
    object TMTextButton: TRadioButton
      Left = 7
      Top = 22
      Width = 219
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&Text (plain text, html, scripts, ...)'
      TabOrder = 0
      OnClick = ControlChange
    end
    object TMBinaryButton: TRadioButton
      Left = 7
      Top = 48
      Width = 219
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&Binary (archives, doc, ...)'
      TabOrder = 1
      OnClick = ControlChange
    end
    object TMAutomaticButton: TRadioButton
      Left = 7
      Top = 74
      Width = 219
      Height = 17
      Anchors = [akLeft, akTop, akRight]
      Caption = '&Automatic'
      TabOrder = 2
      OnClick = ControlChange
    end
    object AsciiFileMaskCombo: THistoryComboBox
      Left = 9
      Top = 115
      Width = 213
      Height = 21
      AutoComplete = False
      Anchors = [akLeft, akTop, akRight]
      ItemHeight = 13
      MaxLength = 1000
      TabOrder = 3
      Text = 'AsciiFileMaskCombo'
      OnExit = ValidateMaskComboExit
    end
  end
  object OtherGroup: TGroupBox
    Left = 8
    Top = 300
    Width = 362
    Height = 46
    Caption = 'Other'
    TabOrder = 5
    DesignSize = (
      362
      46)
    object ExclusionFileMaskLabel: TLabel
      Left = 90
      Top = 20
      Width = 28
      Height = 13
      Caption = 'mas&k:'
      FocusControl = ExcludeFileMaskCombo
    end
    object ExcludeFileMaskCombo: THistoryComboBox
      Left = 136
      Top = 16
      Width = 217
      Height = 21
      AutoComplete = False
      Anchors = [akLeft, akTop, akRight]
      ItemHeight = 13
      MaxLength = 3000
      TabOrder = 1
      Text = 'ExcludeFileMaskCombo'
      OnExit = ValidateMaskComboExit
    end
    object NegativeExcludeCombo: TComboBox
      Left = 10
      Top = 16
      Width = 76
      Height = 21
      Style = csDropDownList
      ItemHeight = 13
      TabOrder = 0
      Items.Strings = (
        'Exclude'
        'Include')
    end
  end
  object ExcludeFileMaskHintText: TStaticText
    Left = 303
    Top = 338
    Width = 54
    Height = 17
    Alignment = taCenter
    Caption = 'mask hints'
    TabOrder = 6
    TabStop = True
  end
end
