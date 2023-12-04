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
    edmNone,    // netahneme nic
    edmCage,    // tahneme klec
    edmPreMove, // cekame na utrzeni pro tazeni prvku
    edmMove,    // tahneme prvky
    edmResize   // zmena rozmeru prvku
};

class CLayoutEditor : public CWindow
{
public:
    CBitmap* CacheBitmap; // obsahuje predrenderovany dialog; zobrazuje se z ni okno
    CBitmap* TempBitmap;  // pomocna bitmapa napriklada pro offscreen rendering pri tazeni klece
    CBitmap* CageBitmap;  // pomocna bitmapa do ktere se renderuje klec

    HWND HDialogHolder;

    HWND HDialog;
    int BaseDlgUnitX; // base dialog units
    int BaseDlgUnitY;

    RECT DialogRect;

    TIndirectArray<CDialogData> TransformStack;
    int CurrentDialog; // index prave editovaneho dialogu v TransformStack
    BOOL PreviewMode;  // pokud je TRUE, je na vrsku TransformStack preview (english nebo unchanged); pri ukonceni preview rezimu bude z TransformStack odstraneno
    BOOL PreviewModeShowExitHint;

    POINT MouseDownPoint; // souradnice kde doslo ke stisku tlacitka mysi
    int MouseMovedDeltaX; // o kolik jsme jiz controly posunuli (v Dialog Units)
    int MouseMovedDeltaY;
    BOOL ShiftPressedOnMouseDown; // byl pri zacatku tazeni stisknuty shift?
    CEdgeEnum ResizeEdge;         // kterou hranu behem tazeni mysi posouvame
    RECT OriginalResizeRect;      // puvodni rozmery prvku na zacatku resize
    int OldResizeDelta;

    int DialogOffsetX; // o kolik je (v bodech) na obrazovce posunuty pocatek client area dialogu
    int DialogOffsetY; // proti client area layout editoru

    CDraggingModeEnum DraggingMode; // v jakem rezimu tazeni jsme?

    int PostponedControlSelect;

    int RepeatArrow;     // kolikrat se ma opakovat nasledujici pohyb sipkou (ala unixove "vi", nastavuje se pred stiskem sipky napsanim cisla na klavesce)
    int LastRepeatArrow; // pamet posledni hodnoty RepeatArrow

    BOOL AlignToMode;     // je TRUE pokud uzivatel zadal prikaz Align To a nyni vybira prvek, ke kteremu se ma align provest
    BOOL KeepAlignToMode; // nastavuje se pred otevreni Align To dialogu, aby vznikle WM_CANCELMODE nezrusilo AlignToMode (zrusime po dialogu)

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

    // vrati pocet bajtu potrebych pro ulozeni TransformStack streamu, ktery predavame do dalsich instanci translatoru
    DWORD GetTransformStackStreamSize();
    // do bufferu 'stream' ulozi binarni reprezentaci pole TransformStack
    void WriteTransformStackStream(BYTE* stream);
    // z bufferu 'stream' nacte TransformStack a krok za krokem pomoci nej vytvori pole TransformStack
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
// Zobrazuje info o vybranych prvcich
//

class CLayoutInfo : public CWindow
{
private:
    HFONT HFont;
    int FontHeight;
    BOOL DataChanged; // je potreba prekreslit?
    CDialogData* DialogData;
    POINT MouseCurrent;
    BOOL ShowMouseDelta;
    int MouseDeltaX;
    int MouseDeltaY;

    CBitmap* InfoBitmap; // obsahuje predrenderovany info, abychom neblikali

public:
    CLayoutInfo();
    ~CLayoutInfo();

    void SetDialogInfo(const CDialogData* dialogData);
    void SetMouseCurrent(POINT* pt);
    void SetMouseDelta(BOOL showDelta, int deltaX = 0, int deltaY = 0);

    // vyhleda ve skupine, ktere rozmery (X/Y/W/H) jsou shodne a nastavi podle toho odpovidajici promenne
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
