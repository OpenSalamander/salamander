// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define PROGRESS_STATE_UNTRANSLATED 0x0000
#define PROGRESS_STATE_TRANSLATED 0x0001

#define COMBOBOX_BASE_HEIGHT 13 // dialog units

// Extract text from an edit window and preallocate the resulting string.
BOOL GetStringFromWindow(HWND hWindow, wchar_t** string);

wchar_t* dupstr(const wchar_t* s);

BOOL IsStyleStaticText(DWORD style, BOOL onlyLeft, BOOL onlyRight);
BOOL IsRadioOrCheckBox(int bt);

void FillControlsWithDummyValues(HWND hDlg);

void RemoveAmpersands(wchar_t* buff);

// Trim whitespace on both ends of 'str' and return the same pointer.
char* StripWS(char* str);

BOOL DecodeString(const wchar_t* iter, int len, wchar_t** string);
void EncodeString(wchar_t* iter, wchar_t** string);

// Return TRUE if the string contains translatable characters
// (and does not contain the text "_dummy_"—currently not used).
BOOL IsTranslatableControl(const wchar_t* text);

COLORREF GetSelectionColor(BOOL clipped);

BOOL __GetNextChar(wchar_t& charValue, wchar_t*& start, wchar_t* end);

#define PUT_WORD(ptr, w) (*(WORD*)(ptr) = (w))
#define GET_WORD(ptr) (*(WORD*)(ptr))
#define PUT_DWORD(ptr, d) (*(DWORD*)(ptr) = (d))
#define GET_DWORD(ptr) (*(DWORD*)(ptr))

enum CSearchDirection
{
    esdRight, // The order matters; used by SelectControlsGroup().
    esdLeft,
    esdDown,
    esdUp
};

enum CAlignToOperationEnum
{
    atoeMove,
    atoeResize
};

enum CAlignToPartEnum
{
    atpeTop,
    atpeRight,
    atpeBottom,
    atpeLeft,
    atpeHCenter,
    atpeVCenter
};

#pragma pack(push)
#pragma pack(4)
struct CAlignToParams
{
    CAlignToOperationEnum Operation; // Operation to execute (move or resize).
    BOOL MoveAsGroup;                // TRUE moves the selected items as a group, otherwise individually.
    CAlignToPartEnum SelPart;        // Which part of the selected elements the operation references.
    CAlignToPartEnum RefPart;        // Reference part on the anchor element.
    int HighlightIndex;              // Copy used when passing to another translator instance.
};
#pragma pack(pop)

typedef struct
{
    WORD dlgVer;
    WORD signature;
    DWORD helpID;
    DWORD exStyle;
    DWORD style;
    WORD cDlgItems;
    short x;
    short y;
    short cx;
    short cy;
    /*
  sz_Or_Ord menu;
  sz_Or_Ord windowClass;
  WCHAR     title[titleLen];
  WORD      pointsize;
  WORD      weight;
  BYTE      italic;
  BYTE      charset;
  WCHAR     typeface[stringLen];
  */
} DLGTEMPLATEEX;

//*****************************************************************************
//
// CStrData
//

class CData;

class CStrData
{
public:
    WORD ID;
    WORD TLangID;

    wchar_t* OStrings[16];
    wchar_t* TStrings[16];
    WORD TState[16];

public:
    CStrData();
    ~CStrData();

    // Load 16 strings for the specified table.
    BOOL LoadStrings(WORD group, const wchar_t* original, const wchar_t* translated, CData* data);

    //    BOOL GetTString(HWND hWindow, int index);
};

//*****************************************************************************
//
// CMenuData
//

struct CMenuItem
{
    wchar_t* OString; // Allocated original string.
    wchar_t* TString; // Allocated translated string.
    WORD ID;          // Menu ID (command items only; POPUP entries use zero).

    WORD Flags; // Start/end of a popup or a regular item.

    WORD State;        // translated/untranslated/...
    int Level;         // Nesting depth.
    int ConflictGroup; // Items with the same ConflictGroup are compared for hotkey conflicts; 0 is top level, 1 is the first nested level, etc.
};

class CMenuData
{
public:
    WORD ID;
    WORD TLangID;
    TDirectArray<CMenuItem> Items;
    int ConflictGroupsCount; // Total number of unique ConflictGroup values.

    BOOL IsEX; // MENUEX is only partially supported in MUI mode; Salamander does not use it so saving is not implemented.

public:
    CMenuData();
    ~CMenuData();

    // Parse menu resources.
    BOOL LoadMenu(LPCSTR original, LPCSTR translated, CData* data);
    BOOL LoadMenuEx(LPCSTR original, LPCSTR translated, CData* data);

    // Return the index of the item with the given ID or -1 if it is missing.
    int FindItemIndex(WORD id);
};

//*****************************************************************************
//
// CStringList
//

class CStringList
{
protected:
    TDirectArray<char*> Strings;

public:
    CStringList();
    ~CStringList();

    BOOL AddString(const char* string);
    int GetCount() { return Strings.Count; }
    BOOL GetStrings(char* buff, int size);
};

//*****************************************************************************
//
// CDialogData
//

enum CModifySelectionMode
{
    emsmSelectDialog,     // deselect controls and select the dialog itself
    emsmSelectControl,    // deselect the dialog and select a control; if hControl is NULL the selection is cleared
    emsmToggleControl,    // deselect the dialog and toggle the control selection state
    emsmSelectAllControls // deselect the dialog and select every control; hControl must be NULL
};

enum CResizeControlsEnum
{
    rceWidthLarge,
    rceWidthSmall,
    rceHeightLarge,
    rceHeightSmall
};

enum CEdgeEnum
{
    edgeLeft,
    edgeTop,
    edgeRight,
    edgeBottom
};

class CControl
{
public:
    WORD ID;

    DWORD Style;
    DWORD ExStyle;

    // Original
    short OX;
    short OY;
    short OCX;
    short OCY;

    // Translated
    short TX;
    short TY;
    short TCX;
    short TCY;

    wchar_t* ClassName;   // Allocated string.
    wchar_t* OWindowName; // Allocated string.
    wchar_t* TWindowName; // Allocated string.

    // DLGITEMTEMPLATEEX support
    DWORD ExHelpID;

    WORD State; // translated/untranslated/...

    BOOL Selected; // TRUE when the control is selected in the layout editor.

public:
    CControl();
    ~CControl();

    void Clean();

    // Deep copy.
    void LoadFrom(CControl* src);

    BOOL ShowInLVWithControls(int i);
    BOOL IsStaticText(BOOL onlyLeft = FALSE, BOOL onlyRight = FALSE) const;
    BOOL IsIcon() const;
    BOOL IsWhiteFrame() const;
    BOOL IsComboBox() const;
    BOOL IsGroupBox() const;
    BOOL IsEditBox() const;
    BOOL IsHorizLine() const;
    BOOL IsRadioOrCheckBox() const;
    BOOL IsRadioBox() const;
    BOOL IsCheckBox() const;
    BOOL IsPushButton() const;

    BOOL IsTContainedIn(CControl const* c) const;
    BOOL IsTVertContainedIn(CControl const* c) const;

    // Return the control rectangle in dialog units; for combo boxes return the collapsed size.
    void GetTRect(RECT* r, BOOL adjustZeroSize);

    // Return the distance from rectangle 'r' (dialog units) in the given direction.
    // Returns -1 when the control is not located in that direction.
    // When scanning to the right we compare the right edges, etc.
    int GetDistanceFromRect(const RECT* r, CSearchDirection direction);
};

// Adding a new transformation requires the following steps:
// 1. Add a value to CTransformationEnum.
// 2. Extend CTransformation::TryMergeWith(), which merges identical operations.
// 3. Update CTransformation::SetTransformationWithNoParam(), SetTransformationWithIntParam(),
//    or SetTransformationWith2IntParams() depending on the number of parameters.
// 4. Update CDialogData::ExecuteTransformation(), which performs the transformation.
enum CTransformationEnum
{
    eloInit,
    eloSelect,              // [] Modify the current selection.
    eloMove,                // [deltaX, deltaY] Move the control by the given offsets.
    eloResize,              // [edge, delta] Resize the control; 'edge' selects the side, 'delta' is the amount in dialog units.
    eloResizeDlg,           // [horizontal, delta] Resize the dialog; when 'horizontal' is TRUE adjust the width, otherwise the height.
    eloResizeControls,      // [resize] Resize command adjusts control width or height according to 'resize'.
    eloSizeToContent,       // [] Size to Content command.
    eloSetSize,             // [width, height] Set explicit control size; -1 keeps the current dimension.
    eloAlignTo,             // [] Align To command.
    eloAlignControls,       // [edge] Align command snaps controls to the specified edge.
    eloCenterControls,      // [horizontal] Center controls relative to each other horizontally or vertically.
    eloCenterControlsToDlg, // [horizontal] Center controls relative to the dialog horizontally or vertically.
    eloEqualSpacing,        // [horizontal, delta] Evenly distribute controls horizontally or vertically with spacing 'delta'.
    eloNormalizeLayout,     // [wide] Normalize layout; 'wide' TRUE switches to wide dialogs, otherwise standard layout.
    eloTemplateReset,       // [] Revert to the original template.
    eloPreviewLayout,       // [] Preview a different layout version.
};

class CTransformation
{
public:
    CTransformationEnum Transformation; // Type of transformation.
    void* Params;                       // Allocated parameters (shape depends on 'Transformation').
    int ParamsSize;                     // Number of bytes allocated for 'Params'.

public:
    CTransformation();
    ~CTransformation();

    void Clean();

    void CopyFrom(const CTransformation* src);

    BOOL TryMergeWith(CTransformation* newTrans);

    void SetTransformationWithNoParam(CTransformationEnum transformation);
    void SetTransformationWithIntParam(CTransformationEnum transformation, int param);
    void SetTransformationWith2IntParams(CTransformationEnum transformation, int param1, int param2);

    void SetIntParam(int param);
    void GetIntParam(int* param);
    void Set2IntParams(int param1, int param2);
    void Get2IntParams(int* param1, int* param2);
    void SetStructParam(void* param, int size);
    void GetStructParam(void* param, int size);

    DWORD GetStreamSize();
    DWORD WriteStream(BYTE* stream);
    DWORD ReadStream(const BYTE* stream);
};

class CDialogData
{
public:
    WORD ID;
    WORD TLangID;

    DWORD Style;
    DWORD ExStyle;

    // Original
    short OX;
    short OY;
    short OCX;
    short OCY;

    // Translated
    short TX;
    short TY;
    short TCX;
    short TCY;

    wchar_t* MenuName;  // Allocated string.
    wchar_t* ClassName; // Allocated string.

    WORD FontSize;
    wchar_t* FontName; // Allocated string.

    // DLGTEMPLATEEX support
    BOOL IsEX;
    WORD ExDlgVer;
    WORD ExSignature;
    DWORD ExHelpID;
    WORD ExWeight;
    BYTE ExItalic;
    BYTE ExCharset;

    // Entry at index 0 contains the dialog caption and selection state.
    // Entries starting at index 1 describe the dialog controls.
    TIndirectArray<CControl> Controls;

    // Index of the reference element for the Align To command.
    int HighlightIndex; // -1: nothing, 0: dialog, >0: control index.

    // Transformations applied to the dialog (and its controls) to reach the current layout.
    // Used to pass edits to another Translator instance.
    CTransformation Transformation;

public:
    CDialogData();
    ~CDialogData();

    void Clean();

    // Parse dialog resources.
    BOOL LoadDialog(WORD* oBuff, WORD* tBuff, BOOL* showStyleWarning,
                    BOOL* showExStyleWarning, BOOL* showDlgEXWarning,
                    CData* data);

    // Build a template in the buffer for saving or previewing.
    // When addProgress == TRUE the translation states are stored as well.
    // Returns the number of bytes used.
    // Set 'forPreview' to TRUE when generating preview data so custom classes are replaced with registered ones.
    // Set 'extendDailog' to TRUE to stretch the dialog so it spans every control.
    DWORD PrepareTemplate(WORD* buff, BOOL addProgress, BOOL forPreview, BOOL extendDailog);

    // Add or remove styles set to logical 1 within the template buffer.
    void TemplateAddRemoveStyles(WORD* buff, DWORD addStyles, DWORD removeStyles);
    // Set coordinates in the template buffer.
    void TemplateSetPos(WORD* buff, short x, short y);

    // Return the index of the window with the requested ID, or -1 when missing.
    int FindControlIndex(WORD id, BOOL isDlgTitle);

    // Return the list-view index for Controls[controlIndex] that stores the dialog texts.
    // Return FALSE when the control is not present in that list view.
    BOOL GetControlIndexInLV(int controlIndex, int* indexLV);

    // Deep copy.
    void LoadFrom(CDialogData* src, BOOL keepLangID = FALSE);

    // Load the selection state.
    void LoadSelectionFrom(CDialogData* src);

    // Return TRUE when the layout differs from the original; used by the layout editor where only layout changes are possible.
    BOOL DoesLayoutChanged(CDialogData* orgDialogData);
    BOOL DoesLayoutChanged2(CDialogData* orgDialogData); // more robust version for importing older translations

    // Return the index of the neighbouring item relative to 'fromIndex' in direction 'dir'; return -1 when no such item exists.
    // A neighbouring item is the closest control intersecting the projection of 'fromIndex' in the requested direction.
    // When 'wideScan' is TRUE the scan band covers the full width/height of the dialog.
    int GetNearestControlInDirection(int fromIndex, CSearchDirection direction, BOOL wideScan);

    // Return TRUE when 'testIndex' belongs to the same group as 'fromIndex'; otherwise return FALSE.
    // A group corresponds to double-click selection (for example a row of buttons).
    BOOL BelongsToSameSelectionGroup(int fromIndex, int testIndex);

    // Return TRUE when both controls share the same top or left edge (depending on 'direction'); otherwise return FALSE.
    BOOL ControlsHasSameGroupEdge(int fromIndex, int testIndex, CSearchDirection direction);

    // Return TRUE when the dialog contains controls placed outside its bounds.
    BOOL HasOuterControls();

    // Return the bounding rectangle encompassing dialog controls.
    // Horizontal separators and controls outside the dialog bounds are ignored.
    // Return TRUE on success; otherwise return FALSE.
    BOOL GetControlsRect(BOOL ignoreSeparators, BOOL ignoreOuterControls, RECT* r);

    // Return TRUE when the layout can be normalized; otherwise return FALSE.
    // Normalization is not supported when outer controls protrude beyond the right edge of the dialog.
    // We have not encountered such dialogs, so the case was never implemented (even though it could be).
    BOOL CanNormalizeLayout();

    // Trim the layout to standard dimensions.
    void AdjustSeparatorsWidth(CStringList* errors);
    void NormalizeLayout(BOOL wide, CStringList* errors);

    // Populate 'footer' with the separator index (if present, it becomes the first entry) and with the indices
    // of buttons located on the bottom edge of the dialog (typically OK/Cancel/Yes/No/Skip/Overwrite/Help/...).
    // The method clears 'footer' before populating it.
    // Return TRUE when evenly spaced buttons (and the optional separator) were found and stored, or when no buttons were found.
    // Return FALSE when buttons were found but are not evenly spaced— a warning is displayed in that case.
    BOOL GetFooterSeparatorAndButtons(TDirectArray<DWORD>* footer);

    // The function assumes the indices in 'footer' are sorted by the controls' X positions from left to right.
    void AdjustFooter(BOOL wide, TDirectArray<DWORD>* footer);

    // Copy dialog and control dimensions from the Original set to the Translated one.
    void LoadOriginalLayout();

    // Selection-related methods.
    // TODO: the following methods originally belonged to CSelectedControls, which handled selection management.
    // That concept proved impractical—selection should participate in undo—so the methods moved here.
    // Renaming them to something more descriptive than the SelCtrls prefix would still be helpful.

    int GetSelectedControlsCount();

    CControl* SelCtrlsGetCurrentControl();
    int SelCtrlsGetCurrentControlIndex();
    int SelCtrlsGetFirstControlIndex();
    HWND SelCtrlsGetCurrentControlHandle(HWND hDialog);

    int GetControlIndex(HWND hDialog, HWND hControl);
    HWND GetControlHandle(HWND hDialog, int index);

    void CtrlsMove(BOOL selectedOnly, int deltaX, int deltaY);
    void SelCtrlsResize(CEdgeEnum edge, int delta, const RECT* originalR = NULL);
    void DialogResize(BOOL horizontal, int delta, const RECT* originalR = NULL);
    void SelCtrlsSetSize(int width, int height);
    void DialogSetSize(int width, int height);
    void SelCtrlsResize(CResizeControlsEnum resize);
    void SelCtrlsAlign(CEdgeEnum edge);
    void SelCtrlsCenter(BOOL horizontal);
    void SelCtrlsCenterToDialog(BOOL horizontal);
    void SortControlsByEdge(TDirectArray<DWORD>* indexes, BOOL sortByLeftEdge);
    void SelCtrlsEqualSpacing(BOOL horizontal, int delta);
    void SelCtrlsSizeToContent(HWND hDialog);
    void SelCtrlsAlignTo(const CAlignToParams* params);

    BOOL SelCtrlsGetCurrentControlRect(RECT* r);

    void GetCurrentControlResizeDelta(const RECT* originalR, CEdgeEnum edge, int* deltaX, int* deltaY);
    void GetDialogResizeDelta(const RECT* originalR, CEdgeEnum edge, int* deltaX, int* deltaY);

    BOOL SelCtrlsContains(HWND hDialog, HWND hControl);
    BOOL SelCtrlsContainsControlWithIndex(int index);

    //void SelCtrlsEnlargeSelectionBox(RECT *box);

    void PaintSelection(HWND hDialog, HDC hDC);
    void OutlineControls(HWND hDialog, HDC hDC);
    void PaintHighlight(HWND hDialog, HDC hDC);

    void GetChildRectPx(HWND hParent, HWND hChild, RECT* r);
    // Return the bounding rectangle around selected controls.
    BOOL SelCtrlsGetOuterRectPx(HWND hDialog, RECT* r);
    BOOL SelCtrlsGetOuterRect(RECT* r);

    // metody menici selection
    void SelectControlsGroup(HWND hDialog, HWND hControl, BOOL keepSelection, BOOL forceVertical);
    void ModifySelection(HWND hDialog, CModifySelectionMode mode, HWND hControl);
    void SelectControlsByCage(BOOL add, const RECT* cageR);
    void SelectNextPrevControl(BOOL next); // reakce na Tab/Shift+Tab
    void SelectControlByID(int id);

    void SetHighlightControl(int index);

    BOOL IsDialogSelected();

    void ExecuteTransformation(HWND hDlg = NULL, CStringList* errors = NULL);

    void StoreSelectionToTransformation();
    void LoadSelectionFromTransformation();

protected:
    void ClearSelection();
    void SetDialogSelected(BOOL value);
};

//*****************************************************************************
//
// CData
//

struct CSLGSignature
{
public:
    WORD LanguageID;            // language identifier
    wchar_t Author[500];        // name of translation author
    wchar_t Web[500];           // web of translation author
    wchar_t Comment[500];       // comment for translation author
    wchar_t HelpDir[100];       // Name of the directory with help files (the Slovak translation may reuse the Czech help).
    BOOL HelpDirExist;          // Does the SLG contain the HelpDir entry? (Salamander .slg only.)
    wchar_t SLGIncomplete[200]; // Empty = Salamander and plugins are fully translated; otherwise link shown on first launch of this language (recruiting translators).
    BOOL SLGIncompleteExist;    // Does the SLG contain the SLGIncomplete entry? (Salamander .slg only.)

    // CRC of the SLT file last imported from or exported to by this SLG file
    // Possible values:
    // "not found" - the variable does not exist in the SLG,
    // "none" - official English version,
    // "" - modified version that has not been imported/exported to SLT,
    // "00000000 SLT" - immediately after exporting to or importing from SLT,
    // "00000000" - modified version that has been imported/exported to SLT
    wchar_t CRCofImpSLT[100];

public:
    CSLGSignature()
    {
        Clean();
    }

    void Clean()
    {
        LanguageID = 0x0409;
        lstrcpyW(Author, L"Open Salamander");
        lstrcpyW(Web, L"www.altap.cz");
        lstrcpyW(Comment, L"");
        lstrcpyW(HelpDir, L"");
        lstrcpyW(SLGIncomplete, L"");
        lstrcpyW(CRCofImpSLT, L"not found");
        HelpDirExist = FALSE;
        SLGIncompleteExist = FALSE;
    }

    void SLTDataChanged()
    {
        if (wcscmp(CRCofImpSLT, L"none") == 0) // convert "official EN version" to "modified version that has not gone through SLT"
            lstrcpyW(CRCofImpSLT, L"");
        else
        {
            int len = wcslen(CRCofImpSLT);
            if (len > 4 && wcscmp(CRCofImpSLT + len - 4, L" SLT") == 0) // convert "just after export/import to SLT" to "modified version processed via SLT"
                CRCofImpSLT[len - 4] = 0;
        }
    }

    BOOL IsSLTDataAfterImportOrExport()
    {
        int len = wcslen(CRCofImpSLT);
        return len > 4 && wcscmp(CRCofImpSLT + len - 4, L" SLT") == 0; // is it "just after export/import to SLT"?
    }

    void SetCRCofImpSLTIfFound(wchar_t* crc)
    {
        if (wcscmp(CRCofImpSLT, L"not found") != 0) // update the stored value when the variable exists in the SLG
            lstrcpynW(CRCofImpSLT, crc, _countof(CRCofImpSLT));
    }

    BOOL IsSLTDataChanged()
    {
        if (wcscmp(CRCofImpSLT, L"") == 0) // modified version that has not been imported/exported to SLT
            return TRUE;
        wchar_t* s = CRCofImpSLT;
        while (*s >= L'0' && *s <= L'9' || *s >= L'a' && *s <= L'f' || *s >= L'A' && *s <= L'F')
            s++;
        if (*s == 0 && (s - CRCofImpSLT) == 8) // CRC without the " SLT" suffix: modified version that was imported/exported to SLT
            return TRUE;
        return FALSE;
    }
};

struct CProgressItem
{
    DWORD ID;
    WORD State;
};

enum CCheckLstItemType
{
    cltPropPgSameSize,       // verify that every property page has the same size
    cltMultiTextControl,     // verify that all listed strings fit into the dialog control (including the inline dialog text)
    cltMultiTextControlBold, // same as cltMultiTextControl but measured with the bold font
    cltDropDownButton,       // verify that the button is wide enough to display the drop-down glyph (down arrow on the right)
    cltMoreButton,           // verify that the button is wide enough to display the more-menu glyph (double down arrows on the right)
    cltComboBox,             // verify that the combo box fits all specified strings
    cltAboutDialogTitle,     // special handling for About dialogs
    cltProgressDialogStatus, // special handling for the Progress dialog
    cltStringsWithCSV,       // verify that the number of commas matches in the original and translated text
};

struct CCheckLstItem
{
    CCheckLstItemType Type;
    WORD DialogID;             // unused by cltPropPgSameSize and cltStringsWithCSV
    WORD ControlID;            // unused by cltPropPgSameSize and cltStringsWithCSV
    TDirectArray<WORD> IDList; // unused by cltDropDownButton, cltMoreButton, cltAboutDialogTitle, and cltProgressDialogStatus

    CCheckLstItem() : IDList(5, 5)
    {
        Type = cltPropPgSameSize;
        DialogID = ControlID = 0;
    }

    BOOL ContainsDlgID(WORD dlgID);
};

enum CIgnoreLstItemType
{
    iltOverlap,            // two dialog controls overlap
    iltClip,               // dialog control text is clipped because the control is too short
    iltSmIcon,             // icon size: icon is small; use 10x10 dialog units instead of the default 20x20
    iltHotkeysInDlg,       // conflicting hotkeys for controls in a dialog
    iltTooClose,           // too close to the dialog edge
    iltMisaligned,         // two dialog controls are misaligned
    iltDiffSized,          // two dialog controls differ slightly in size
    iltDiffSpacing,        // dialog control spacing is irregular among neighbouring controls
    iltNotStdSize,         // dialog control has a non-standard size
    iltIncorPlLbl,         // misaligned label and its control
    iltMisColAtEnd,        // missing ':' at the end of the string
    iltInconTxtEnds,       // dialog control whose text ending should not be validated
    iltInconStrEnds,       // string ending should not be validated
    iltInconStrBegs,       // string beginning should not be validated
    iltInconCtrlChars,     // inconsistent control characters (\r, \n, \t)
    iltInconHotKeys,       // inconsistent hotkeys ('&')
    iltProgressBar,        // indicates a progress bar (not static text)
    iltInconFmtSpecif,     // inconsistent format specifiers (%d, %f, etc.)
    iltInconFmtSpecifCtrl, // dialog control may contain inconsistent format specifiers (%d, %f, etc.)
};

struct CIgnoreLstItem
{
    CIgnoreLstItemType Type;
    WORD DialogID;
    WORD ControlID1; // used for all types except iltMisColAtEnd, iltInconStrEnds, iltInconStrBegs, iltInconCtrlChars, iltInconHotKeys, iltInconFmtSpecif
    WORD ControlID2; // used only for iltOverlap, iltHotkeysInDlg, iltMisaligned, iltDiffSized, and iltIncorPlLbl

    CIgnoreLstItem(CIgnoreLstItemType type, WORD dialogID, WORD controlID1, WORD controlID2)
    {
        Type = type;
        DialogID = dialogID;
        ControlID1 = controlID1;
        ControlID2 = controlID2;
    }
};

class CSalMenuSection
{
private:
    char* TemplateName; // allocated template name

public:
    WORD SectionDialogID;         // dialog associated with the section (0 if none)
    WORD SectionControlDialogID;  // dialog that owns the associated control (0 if none)
    WORD SectionControlControlID; // control ID linked with the section (0 if none)
    TDirectArray<WORD> StringsID; // string IDs belonging to the section

public:
    CSalMenuSection() : StringsID(20, 20)
    {
        TemplateName = NULL;
        SectionDialogID = 0;
        SectionControlDialogID = 0;
        SectionControlControlID = 0;
    }

    ~CSalMenuSection()
    {
        if (TemplateName != NULL)
        {
            free(TemplateName);
            TemplateName = NULL;
        }
    }

    BOOL SetTemplateName(const char* templateName)
    {
        if (TemplateName != NULL)
        {
            free(TemplateName);
            TemplateName = NULL;
        }
        if (templateName != NULL)
        {
            TemplateName = (char*)malloc(strlen(templateName) + 1);
            if (TemplateName == NULL)
                return FALSE;
            lstrcpy(TemplateName, templateName);
        }
        return TRUE;
    }
    const char* GetTemplateName() { return TemplateName; }
};

struct CSalMenuDataParserState;

enum CProjectSectionEnum
{
    pseNone,
    pseFiles,
    pseSettings,
    pseDummy, // ignore this section
    pseDialogsTranslation,
    pseMenusTranslation,
    pseStringsTranslation,
    pseRelayout
};

enum CTranslationTypeEnum
{
    tteDialogs,
    tteMenus,
    tteStrings
};

struct CBookmark
{
    DWORD TreeItem;
    WORD TextItem;
};

class CMenuPreview
{
public:
    TDirectArray<DWORD> Lines; // output window lines for which the list should be shown
    TDirectArray<wchar_t*> Texts;

public:
    CMenuPreview() : Lines(10, 10), Texts(10, 10)
    {
    }
    ~CMenuPreview()
    {
        for (int i = 0; i < Texts.Count; i++)
            free(Texts[i]);
        Lines.DestroyMembers();
        Texts.DestroyMembers();
    }
    void AddText(const wchar_t* text)
    {
        Texts.Add(dupstr(text));
    }
    void AddLine(DWORD line)
    {
        Lines.Add(line);
    }
    BOOL HasLine(DWORD line)
    {
        for (int i = 0; i < Lines.Count; i++)
            if (Lines[i] == line)
                return TRUE;
        return FALSE;
    }
    void LoadFrom(CMenuPreview* s)
    {
        for (int i = 0; i < Texts.Count; i++)
            free(Texts[i]);
        Lines.DestroyMembers();
        Texts.DestroyMembers();

        for (int i = 0; i < s->Lines.Count; i++)
            AddLine(s->Lines[i]);
        for (int i = 0; i < s->Texts.Count; i++)
            AddText(s->Texts[i]);
    }
};

class CData
{
private:
    TDirectArray<CProgressItem> DialogsTranslationStates;
    TDirectArray<CProgressItem> MenusTranslationStates;
    TDirectArray<CProgressItem> StringsTranslationStates;

    TDirectArray<CBookmark> Bookmarks; // dialog IDs that need to be relayouted (after importing an old configuration)

public:
    TIndirectArray<CStrData> StrData;
    TIndirectArray<CMenuData> MenuData;
    TIndirectArray<CDialogData> DlgData;
    TIndirectArray<CSalMenuSection> SalMenuSections;
    TIndirectArray<CIgnoreLstItem> IgnoreLstItems;
    TIndirectArray<CCheckLstItem> CheckLstItems;

    TDirectArray<WORD> Relayout; // dialog IDs that need to be relayouted (after importing an old configuration)

    TIndirectArray<CMenuPreview> MenuPreview; // used to resolve menu conflicts

    // variables used during enumeration
    BOOL EnumReturn;
    HINSTANCE HTranModule;
    BOOL Importing;
    BOOL ShowStyleWarning;
    BOOL ShowExStyleWarning;
    BOOL ShowDlgEXWarning;
    BOOL MUIMode;      // importing (or already imported) MUI packages for the Windows 7 localization
    DWORD MUIDialogID; // unique ID that matters only when 'MUIMode' is TRUE
    DWORD MUIMenuID;
    DWORD MUIStringID;

    CSLGSignature SLGSignature;

    CVersionInfo VersionInfo;

    // file paths
    char ProjectFile[MAX_PATH];
    char SourceFile[MAX_PATH];
    char TargetFile[MAX_PATH];
    char IncludeFile[MAX_PATH]; // all resource headers merged into a single file
    char SalMenuFile[MAX_PATH];
    char IgnoreLstFile[MAX_PATH];
    char CheckLstFile[MAX_PATH];
    char SalamanderExeFile[MAX_PATH];
    char ExportFile[MAX_PATH]; // export as text: consumed by the spellchecker
    char ExportAsTextArchiveFile[MAX_PATH];
    char FullSourceFile[MAX_PATH];
    char FullTargetFile[MAX_PATH];
    char FullIncludeFile[MAX_PATH]; // all resource headers merged into a single file
    char FullSalMenuFile[MAX_PATH];
    char FullIgnoreLstFile[MAX_PATH];
    char FullCheckLstFile[MAX_PATH];
    char FullSalamanderExeFile[MAX_PATH];
    char FullExportFile[MAX_PATH];
    char FullExportAsTextArchiveFile[MAX_PATH];

    // tree state
    BOOL OpenStringTables;
    BOOL OpenMenuTables;
    BOOL OpenDialogTables;
    DWORD SelectedTreeItem;

protected:
    BOOL Dirty;

public:
    CData();

    void Clean();

    BOOL LoadProject(const char* fileName);
    BOOL SaveProject();

    BOOL Load(const char* original, const char* translated, BOOL import);
    BOOL Save();

    // Load salmenu.mnu generated from the sources by Petr's Perl script
    // the file stores our internal menu structures defined with MENU_TEMPLATE_ITEM
    BOOL LoadSalMenu(const char* fileName);
    BOOL ProcessSalMenuLine(const char* line, const char* lineEnd, int row, CSalMenuDataParserState* parserState);

    // Load ignore.lst: describes overlaps and clipping issues that should be ignored
    BOOL LoadIgnoreLst(const char* fileName);
    BOOL ProcessIgnoreLstLine(const char* line, const char* lineEnd, int row);

    // Load check.lst: items that must be validated (for example matching property-page sizes)
    BOOL LoadCheckLst(const char* fileName);
    BOOL ProcessCheckLstLine(const char* line, const char* lineEnd, int row);

    // Load an older translation and apply it to the current data
    // When 'trlPropOnly' is TRUE only translation properties are loaded
    BOOL Import(const char* project, BOOL trlPropOnly, BOOL onlyDlgLayouts);

    // Dump the translated portion of data into a text file
    // so that the proofreader can check the grammar
    BOOL Export(const char* fileName);

    // Export translated strings to a Unicode file that we can store/merge in CVS
    // the same file can be imported back later
    BOOL ExportAsTextArchive(const char* fileName, BOOL withoutVerInfo);
    BOOL ImportTextArchive(const char* fileName, BOOL testOnly); // if 'testOnly' is TRUE, only verify that the file content maps 1:1 to our data

    // Export dialog/control sizes; used when modifying .rc modules containing
    // resource sources for the English language version compiled from the .rc
    BOOL ExportDialogsAndControlsSizes(const char* fileName);

    // Fetch Microsoft MUI language packages for Windows 7 (CAB files)
    BOOL LoadMUIPackages(const char* originalMUI, const char* translatedMUI);

    // Validate data; return TRUE when it is safe to save, otherwise FALSE
    //    BOOL Validate();

    // Populate the tree used to pick the edited resource
    void FillTree();

    void UpdateNode(HTREEITEM hItem);
    void UpdateSelectedNode();
    void UpdateAllNodes(); // update translated states in the tree view
    void UpdateTexts();    // update the list view in the Texts window

    // populate the Texts window
    void FillTexts(DWORD lParam);

    void SetDirty(BOOL dirty = TRUE);
    BOOL IsDirty() { return Dirty; }

    // return the state for the specified ID
    TDirectArray<CProgressItem>* GetTranslationStates(CTranslationTypeEnum type);
    void CleanTranslationStates();
    BOOL AddTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, WORD state);
    WORD QueryTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, const wchar_t* oStr, const wchar_t* tStr);
    BOOL QueryTranslationStateIndex(CTranslationTypeEnum type, WORD id1, WORD id2, int* index);

    void CleanRelayout();
    void AddRelayout(WORD id);
    BOOL RemoveRelayout(WORD id);

    // search data using criteria from Config
    void Find();

    // find untranslated strings
    void FindUntranslated(BOOL* completelyUntranslated = NULL);

    // find dialogs that require relayout after importing the original translation
    void FindDialogsForRelayout();

    // find dialogs with controls positioned outside the dialog bounds
    void FindDialogsWithOuterControls();

    // find conflicting hotkeys; results are written to the Output window
    void ValidateTranslation(HWND hParent);

    // look for control-ID collisions within a dialog; called after loading the project
    // dump results into the Output window
    void LookForIdConflicts();

    // mark all modified strings as Translated
    void MarkChangedTextsAsTranslated();

    // reset every dialog to the original layout
    void ResetDialogsLayoutsToOriginal();

    int FindStrData(WORD id);
    int FindMenuData(WORD id);
    int FindDialogData(WORD id);

    BOOL FindStringWithID(WORD id, int* index = NULL, int* subIndex = NULL, int* lvIndex = NULL);
    BOOL GetStringWithID(WORD id, wchar_t* buf, int bufSize);

    // find CheckLstItems entries for the specified dialogID/controlID (anyControl==TRUE = any control in the dialog)
    BOOL GetItemFromCheckLst(WORD dialogID, WORD controlID, CCheckLstItem** multiTextOrComboItem,
                             CCheckLstItem** checkLstItem, BOOL anyControl = FALSE);

    // find cltStringsWithCSV entries in CheckLstItems and ensure 'strID' is not listed
    BOOL IsStringWithCSV(WORD strID);

    // search IgnoreLstItems for the issue and return TRUE when it should be ignored
    // (was found)
    BOOL IgnoreProblem(CIgnoreLstItemType type, WORD dialogID, WORD controlID1, WORD controlID2);

    // determine whether the icon size should be ignored and the small 16x16 size used
    BOOL IgnoreIconSizeIconIsSmall(WORD dialogID, WORD controlID);

    // determine whether the hotkey conflict between controlID1 and controlID2 should be ignored
    // within dialog 'dialogID'
    BOOL IgnoreDlgHotkeysConflict(WORD dialogID, WORD controlID1, WORD controlID2);

    // determine whether a control being too close to the dialog edge should be ignored
    BOOL IgnoreTooCloseToDlgFrame(WORD dialogID, WORD controlID);

    // determine whether the control is a progress bar (ignore it being a static)
    BOOL IgnoreStaticItIsProgressBar(WORD dialogID, WORD controlID);

    // determine whether irregular placement among other controls should be ignored
    BOOL IgnoreDifferentSpacing(WORD dialogID, WORD controlID);

    // determine whether a control with a non-standard size should be ignored
    BOOL IgnoreNotStdSize(WORD dialogID, WORD controlID);

    // determine whether a missing colon at the end of the string should be ignored
    BOOL IgnoreMissingColonAtEnd(WORD stringID);

    // determine whether the ending of a control text in a dialog should be skipped
    // (e.g., multi-line text split across multiple static controls)
    BOOL IgnoreInconTxtEnds(WORD dialogID, WORD controlID);

    // determine whether a string ending should be validated
    BOOL IgnoreInconStringEnds(WORD stringID);

    // determine whether a string beginning should be validated
    BOOL IgnoreInconStringBegs(WORD stringID);

    // determine whether control characters (\r, \n, \t) should be validated
    BOOL IgnoreInconCtrlChars(WORD stringID);

    // determine whether hotkeys ('&') should be validated
    BOOL IgnoreInconHotKeys(WORD stringID);

    // determine whether format specifiers (%d, %f, etc.) should be validated
    BOOL IgnoreInconFmtSpecif(WORD stringID);

    // determine whether format specifiers (%d, %f, etc.) should be ignored for a control
    BOOL IgnoreInconFmtSpecifCtrl(WORD dialogID, WORD controlID);

    void ToggleBookmark(DWORD treeItem, WORD textItem);
    const CBookmark* GotoBookmark(BOOL next, DWORD treeItem, WORD textItem);
    void ClearBookmarks();
    int GetBookmarksCount();
    BOOL FindBookmark(DWORD treeItem, WORD textItem);

    // return TRUE when the loaded project language is English
    BOOL IsEnglishProject();

protected:
    BOOL EnumResources(HINSTANCE hSrcModule, HINSTANCE hDstModule, BOOL import);
    BOOL CheckIdentifiers(BOOL reportAsErrors);

    //    BOOL LoadSLGData(HINSTANCE hModule, const char *resName, LPVOID buff, int buffSize, BOOL string);
    //    BOOL SaveSLGData(HANDLE hUpdateRes, const char *resName, LPVOID buff, int buffSize, BOOL string);
    BOOL LoadSLGSignature(HINSTANCE hModule);
    BOOL SaveSLGSignature(HANDLE hUpdateRes);
    BOOL SaveModuleName(HANDLE hUpdateRes);
    BOOL SaveStrings(HANDLE hUpdateRes);
    BOOL SaveMenus(HANDLE hUpdateRes);
    BOOL SaveDialogs(HANDLE hUpdateRes);

    BOOL StringsAddTranslationStates();
    BOOL MenusAddTranslationStates();
    BOOL DialogsAddTranslationStates();

    BOOL CompareStrings(const wchar_t* _string, const wchar_t* _pattern);

    // project loading/saving
    BOOL ProcessProjectLine(CProjectSectionEnum* section, const char* line, int row);
    BOOL WriteProjectLine(HANDLE hFile, const char* line);

    BOOL WriteUnicodeBOM(HANDLE hFile);
    BOOL VerifyBOM(WORD bom);
    BOOL WriteUTF8BOM(HANDLE hFile, DWORD* fileCRC32);
    BOOL WriteUnicodeTextLine(HANDLE hFile, const wchar_t* line);
    BOOL WriteUTF8TextLine(HANDLE hFile, const wchar_t* line, DWORD* fileCRC32);
};

extern CData Data;

BOOL IsControlClipped(const CDialogData* dialog, const CControl* control, HWND hDlg,
                      int* idealSizeX = NULL, int* idealSizeY = NULL, CCheckLstItem* multiTextOrComboItem = NULL,
                      CCheckLstItem* checkLstItem = NULL);

void BufferKey(WORD vk, BOOL bExtended = FALSE, BOOL down = TRUE, BOOL up = TRUE);

// 'p' is a point in screen coordinates; return the index of the child window at that position
// return NULL when no window is found
HWND GetChildWindow(POINT p, HWND hDialog, int* index = NULL);
