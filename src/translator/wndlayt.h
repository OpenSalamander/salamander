// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* LAYOUTWINDOW_NAME;

class CBitmap;

//*****************************************************************************
//
// CLayoutEditor
//

enum CDraggingModeEnum
{
    edmNone,    // nothing is being dragged
    edmCage,    // dragging the selection cage
    edmPreMove, // waiting for the drag threshold before moving controls
    edmMove,    // dragging controls
    edmResize   // resizing a control
};

class CLayoutEditor : public CWindow
{
public:
    CBitmap* CacheBitmap; // holds the pre-rendered dialog; the window draws from it
    CBitmap* TempBitmap;  // helper bitmap used for off-screen rendering when dragging the cage
    CBitmap* CageBitmap;  // helper bitmap that stores the cage rendering

    HWND HDialogHolder;

    HWND HDialog;
    int BaseDlgUnitX; // base dialog units
    int BaseDlgUnitY;

    RECT DialogRect;

    TIndirectArray<CDialogData> TransformStack;
    int CurrentDialog; // index of the dialog currently edited in the TransformStack
    BOOL PreviewMode;  // when TRUE the top of TransformStack holds a preview (English or unchanged); it is removed once preview mode ends
    BOOL PreviewModeShowExitHint;

    POINT MouseDownPoint; // coordinates where the mouse button was pressed
    int MouseMovedDeltaX; // how far the controls have been moved (in dialog units)
    int MouseMovedDeltaY;
    BOOL ShiftPressedOnMouseDown; // was Shift held when the drag started?
    CEdgeEnum ResizeEdge;         // which edge is being moved during the mouse drag
    RECT OriginalResizeRect;      // original control rectangle at the start of resizing
    int OldResizeDelta;

    int DialogOffsetX; // offset (in pixels) of the dialog client origin on screen
    int DialogOffsetY; // relative to the layout editor client area

    CDraggingModeEnum DraggingMode; // which dragging mode is active

    int PostponedControlSelect;

    int RepeatArrow;     // repeat count for the next arrow-key move (vi-style: set by typing a number before the arrow)
    int LastRepeatArrow; // remembers the previous RepeatArrow value

    BOOL AlignToMode;     // TRUE when Align To is active and the user is selecting the reference control
    BOOL KeepAlignToMode; // set before opening the Align To dialog so WM_CANCELMODE does not cancel AlignToMode (cleared afterwards)

private:
    CDialogData* OriginalDialogData;

public:
    CLayoutEditor();
    ~CLayoutEditor();

    void UpdateBuffer();

    BOOL RebuildDialog();

    void PaintSelection();

    void Open(CDialogData* dialogData, int selectedControlID);
    BOOL Close();

    void OnUndo();
    void OnRedo();
    BOOL HasUndo();
    BOOL HasRedo();

    void NormalizeLayout(BOOL wide);
    void BeginPreviewLayout(BOOL original, BOOL showExitHint);
    void EndPreviewLayout();
    void ResetTemplateToOriginal();
    void AlignControls(CEdgeEnum edge);
    void ResizeControls(CResizeControlsEnum resize);
    void SetSize();
    void ControlsSizeToContent();
    void CenterControls(BOOL horizontal);
    void CenterControlsToDialog(BOOL horizontal);
    void EqualSpacing(BOOL horizontal, int delta);

    void SetAlignToMode(BOOL active);

    void TestDialogBox();

    void SelectAllControls(BOOL select);

    void NextPrevControl(BOOL next);

    void PaintSelectionGuidelines(HDC hDC);

    void ApplyAlignTo(const CAlignToParams* params);

    BOOL StartNewTranslatorWithLayoutEditor();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawCage(POINT pt);
    void PaintCachedBitmap();

    CDialogData* NewTransformation(CTransformation* transformation);
    void NewTransformationExecuteRebuildDlg(CTransformation* transformation, HWND hDlg = NULL, CStringList* errors = NULL);

    // return the byte size needed to store the TransformStack stream shared with other Translator instances
    DWORD GetTransformStackStreamSize();
    // write the binary representation of TransformStack into 'stream'
    void WriteTransformStackStream(BYTE* stream);
    // load TransformStack from 'stream' and rebuild it step by step
    void LoadTransformStackStream(const BYTE* stream);

    BOOL HitTest(const POINT* p, CEdgeEnum* edge);
    int GetOurChildWindowIndex(POINT pt);
    HWND GetOurChildWindow(POINT pt);

    void PostMouseMove();

    static BOOL CALLBACK EditDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
    static BOOL CALLBACK TestDialogProcW(HWND hwndDlg, UINT uMsg, WPARAM wParam, LPARAM lParam);
};

class CLayoutEditorHolder : public CWindow
{
public:
    CLayoutEditorHolder();
};

//*****************************************************************************
//
// CLayoutInfo
//
// Shows information about the selected controls
//

class CLayoutInfo : public CWindow
{
private:
    HFONT HFont;
    int FontHeight;
    BOOL DataChanged; // requires repainting?
    CDialogData* DialogData;
    POINT MouseCurrent;
    BOOL ShowMouseDelta;
    int MouseDeltaX;
    int MouseDeltaY;

    CBitmap* InfoBitmap; // holds pre-rendered info to avoid flicker

public:
    CLayoutInfo();
    ~CLayoutInfo();

    void SetDialogInfo(const CDialogData* dialogData);
    void SetMouseCurrent(POINT* pt);
    void SetMouseDelta(BOOL showDelta, int deltaX = 0, int deltaY = 0);

    // examine the selection and report which dimensions (X/Y/W/H) match across controls
    BOOL GetSameProperties(BOOL* sameTX, BOOL* sameTY, BOOL* sameTCX, BOOL* sameTCY, BOOL* sameRight, BOOL* sameBottom);

    BOOL GetOuterSpace(RECT* outerSpace);

    int GetNeededHeight();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hDC);
};

//*****************************************************************************
//
// CLayoutWindow
//

class CLayoutWindow : public CWindow
{
public:
    CLayoutEditorHolder DialogHolder;
    CLayoutEditor LayoutEditor;
    CLayoutInfo LayoutInfo;

public:
    CLayoutWindow();

    void SetupMenu(HMENU hMenu);

    void OnInitMenu(HMENU hMenu);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL OpenLayoutEditorForProject();
};

extern CLayoutWindow* LayoutWindow;
