// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CConfiguration
//

#define RECENT_PROJECTS_COUNT 30

#define FIND_HISTORY_SIZE 30 // find dialog

class CConfiguration
{
public:
    WINDOWPLACEMENT FrameWindowPlacement;
    WINDOWPLACEMENT TreeWindowPlacement;
    WINDOWPLACEMENT RHWindowPlacement;
    WINDOWPLACEMENT TextWindowPlacement;
    WINDOWPLACEMENT OutWindowPlacement;
    WINDOWPLACEMENT PreviewWindowPlacement;

    int ColumnNameWidth;
    int ColumnTranslatedWidth;
    int ColumnOriginalWidth;

    char RecentProjects[RECENT_PROJECTS_COUNT][MAX_PATH];
    char LastProject[MAX_PATH];

    char LastMUIOriginal[MAX_PATH];
    char LastMUITranslated[MAX_PATH];

    BOOL FindTexts;
    BOOL FindSymbols;
    BOOL FindOriginal;
    BOOL FindWords;
    BOOL FindCase;
    BOOL FindIgnoreAmpersand;
    BOOL FindIgnoreDash;
    wchar_t* FindHistory[FIND_HISTORY_SIZE];

    BOOL ReloadProjectAtStartup;

    BOOL PreviewExtendDialogs;
    BOOL PreviewOutlineControls;
    BOOL PreviewFillControls;

    // layout editor
    BOOL EditorOutlineControls;
    BOOL EditorFillControls;

    // Validate Translation
    BOOL ValidateDlgHotKeyConflicts;
    BOOL ValidateMenuHotKeyConflicts;
    BOOL ValidatePrintfSpecifier;
    BOOL ValidateControlChars;
    BOOL ValidateStringsWithCSV;
    BOOL ValidatePluralStr;
    BOOL ValidateTextEnding;
    BOOL ValidateHotKeys;
    BOOL ValidateLayout;
    BOOL ValidateClipping;
    BOOL ValidateControlsToDialog;
    BOOL ValidateMisalignedControls;
    BOOL ValidateDifferentSizedControls;
    BOOL ValidateDifferentSpacedControls;
    BOOL ValidateStandardControlsSize;
    BOOL ValidateLabelsToControlsSpacing;
    BOOL ValidateSizesOfPropPages;
    BOOL ValidateDialogControlsID;
    BOOL ValidateHButtonSpacing;

    BOOL AgreementConfirmed;

public:
    CConfiguration();
    ~CConfiguration();

    BOOL Save();
    BOOL Load();

    void AddRecentProject(const char* fileName);
    void RemoveRecentProject(int index);
    int FindRecentProject(const char* fileName);

    // 1 = vse TRUE, 2 = layout TRUE, zbytek FALSE
    void SetValidateAll(int mode = 1);
};

extern CConfiguration Config;
