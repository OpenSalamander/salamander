// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern BOOL IsAlphaNumeric[256]; // pole TRUE/FALSE pro znaky (FALSE = neni pismeno ani cislice)
extern BOOL IsAlpha[256];

//****************************************************************************
//
// CSelection
//

class CSelection
{
private:
    int FocusX;  // X souradnice focusu
    int FocusY;  // Y souradnice focusu
    int AnchorX; // pokud je tazen select, druhy roh proti focusu
    int AnchorY; // pokud je tazen select, druhy roh proti focusu
    // v pripade, ze je vybrana pouze jedna polozka, je Anchor? == Focus?

    RECT Rect; // normalizovany obdelnik

public:
    CSelection();

    CSelection& operator=(const CSelection& s);

    // vrati TRUE, pokud bunka [x,y] lezi uvnitr vyberu
    BOOL Contains(int x, int y)
    {
        return x >= Rect.left && x <= Rect.right && y >= Rect.top && y <= Rect.bottom;
    }

    // vrati TRUE, pokud sloupec x lezi uvnitr vyberu
    BOOL ContainsColumn(int x)
    {
        return x >= Rect.left && x <= Rect.right;
    }

    // vrati TRUE, pokud radek y lezi uvnitr vyberu
    BOOL ContainsRow(int y)
    {
        return y >= Rect.top && y <= Rect.bottom;
    }

    // vrati TRUE, pokud bunka [x,y] odpovida focused bunce
    BOOL IsFocus(int x, int y)
    {
        return x == FocusX && y == FocusY;
    }

    void GetFocus(int* x, int* y)
    {
        *x = FocusX;
        *y = FocusY;
    }

    void GetAnchor(int* x, int* y)
    {
        *x = AnchorX;
        *y = AnchorY;
    }

    void GetNormalizedSelection(RECT* selection)
    {
        *selection = Rect;
    }

    void SetFocus(int x, int y)
    {
        FocusX = x;
        FocusY = y;
        Normalize();
    }

    void SetFocusAndAnchor(int x, int y)
    {
        FocusX = x;
        FocusY = y;
        AnchorX = x;
        AnchorY = y;
        Normalize();
    }

    void SetAnchor(int x, int y)
    {
        AnchorX = x;
        AnchorY = y;
        Normalize();
    }

    BOOL FocusIsAnchor()
    {
        return FocusX == AnchorX && FocusY == AnchorY;
    }

    // vrati TRUE, pokud bunka [x,y] mela v oldSelection jiny stav
    BOOL Changed(const CSelection* old, int x, int y)
    {
        // pokud se zmenil selected stav
        BOOL sel = x >= Rect.left && x <= Rect.right && y >= Rect.top && y <= Rect.bottom;
        BOOL oldSel = x >= old->Rect.left && x <= old->Rect.right && y >= old->Rect.top && y <= old->Rect.bottom;
        // nebo focused stav
        BOOL foc = FocusX == x && FocusY == y;
        BOOL oldFoc = old->FocusX == x && old->FocusY == y;
        // vratime TRUE; jinak FALSE
        return foc != oldFoc || sel != oldSel;
    }

    BOOL ChangedColumn(const CSelection* old, int x)
    {
        BOOL sel = x >= Rect.left && x <= Rect.right;
        BOOL oldSel = x >= old->Rect.left && x <= old->Rect.right;
        return sel != oldSel;
    }

    BOOL ChangedRow(const CSelection* old, int y)
    {
        BOOL sel = y >= Rect.top && y <= Rect.bottom;
        BOOL oldSel = y >= old->Rect.top && y <= old->Rect.bottom;
        return sel != oldSel;
    }

    // orizne vyber tak, aby nepresahoval sirku urcenou promennou maxX
    void Clip(int maxX)
    {
        if (FocusX > maxX)
            FocusX = maxX;
        if (AnchorX > maxX)
            AnchorX = maxX;
        Normalize();
    }

private:
    // napocita promennou Rect na zaklade Focus? a Anchor?
    // je treba zavolat po kazde zmene Focus? nebo Anchor?
    void Normalize();
};

//****************************************************************************
//
// CBookmark
//

struct CBookmark
{
public:
    int X; // X souradnice bookmarku
    int Y; // Y souradnice bookmarku
};

//****************************************************************************
//
// CBookmarkList
//

class CBookmarkList
{
private:
    TDirectArray<CBookmark> Bookmarks;

public:
    CBookmarkList();

    void Toggle(int x, int y);
    BOOL GetNext(int x, int y, int* newX, int* newY, BOOL next);
    BOOL IsMarked(int x, int y);
    void ClearAll();

    int GetCount() { return Bookmarks.Count; }

private:
    BOOL GetIndex(int x, int y, int* index);
};

//****************************************************************************
//
// CRendererWindow
//

#define UPDATE_VERT_SCROLL 0x00000001
#define UPDATE_HORZ_SCROLL 0x00000002

enum CDragSelectionMode
{
    dsmNormal,  // rolovani v obou smerech
    dsmColumns, // rolovani horiznotalne
    dsmRows,    // rolovani vertiklane
    dsmNone     // nerolujeme
};

class CViewerWindow;

class CRendererWindow : public CWindow
{
public:
    CDatabase Database; // rozhrani pro praci s otevrenou databazi
    CViewerWindow* Viewer;

    // grafika
    HPEN HGrayPen;
    HPEN HLtGrayPen;
    HPEN HSelectionPen;
    HPEN HBlackPen;
    HFONT HFont; // zakladni font
    HICON HDeleteIcon;
    HICON HMarkedIcon;
    int RowHeight;           // vyska radku
    int CharAvgWidth;        // prumerna sirka znaku
    int TopTextMargin;       // posunuti textu v ramci radku
    int LeftTextMargin;      // posunuti textu v ramci radku
    int Width;               // client width
    int Height;              // client height
    int RowsOnPage;          // pocet plne viditelnych radek na strance
    int TopIndex;            // index prvni zobrazene radky
    int XOffset;             // X souradnice prvniho zobrazeneho bodu
    CSelection Selection;    // umisteni focusu a vyberu
    CSelection OldSelection; // stare umisteni focusu a vyberu
    CBookmarkList Bookmarks; // Bookmarky

    CDragSelectionMode DragMode; // prave mysi tahneme selection
    DWORD_PTR ScrollTimerID;

    int DragColumn;       // pokud je ruzne od -1, tahneme sirku tohoto viditelneho sloupce
    int DragColumnOffset; // ma vyznam pouze je-li DragColumn != -1

    BOOL Creating; // okno se vytvari -- zatim nemazat pozadi

    BOOL AutoSelect;
    char Coding[210];
    char DefaultCoding[210];
    BOOL UseCodeTable; // ma se prekodovavat podle CodeTable?
    // CodeTable ma vyznam pouze je-li UseCodeTable rovno TRUE
    char CodeTable[256]; // kodovaci tabulka

protected:
    int EnumFilesSourceUID;    // UID zdroje pro enumeraci souboru ve vieweru
    int EnumFilesCurrentIndex; // index aktualniho souboru ve vieweru ve zdroji

    // slouzi pro akumulaci mikrokroku pri otaceni koleckem mysi, viz
    // http://msdn.microsoft.com/en-us/library/ms997498.aspx (Best Practices for Supporting Microsoft Mouse and Keyboard Devices)
    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal

public:
    CRendererWindow(int enumFilesSourceUID, int enumFilesCurrentIndex);
    ~CRendererWindow();

    void OnFileOpen();
    void OnFileReOpen();
    void OnGoto();

    BOOL OpenFile(const char* name, BOOL useDefaultConfig);

    // nastavi info scrollbaram
    void SetupScrollBars(DWORD update = UPDATE_VERT_SCROLL | UPDATE_HORZ_SCROLL);

    void RebuildGraphics(); // volame pri zmene konfigurace

    // nakopci aktualni selection na clipboard
    void CopySelectionToClipboard();

    // nastavi Selection tak, aby byl pres vsechny bunky a prekresli obrazovku
    void SelectAll();

    // pokud je conversion == NULL, bude nastaveno "Don't Convert"
    void SelectConversion(const char* conversion);

    // vola dialog pro spravu sloupcu
    void ColumnsWasChanged();

    // Bookmarks
    void OnToggleBookmark();
    void OnNextBookmark(BOOL next);
    void OnClearBookmarks();
    int GetBookmarkCount() { return Bookmarks.GetCount(); }

    // Find
    void Find(BOOL forward, BOOL wholeWords,
              CSalamanderBMSearchData* bmSearchData,
              CSalamanderREGEXPSearchData* regexpSearchData);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PaintTopMargin(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly);
    void PaintBody(HDC hDC, HRGN hUpdateRgn, const RECT* clipRect, BOOL selChangeOnly);
    void Paint(HDC hDC, HRGN hUpdateRgn, BOOL selChangeOnly);

    void EnsureColumnIsVisible(int x);
    void EnsureRowIsVisible(int y);

    void CreateGraphics();  // inicializace handlu
    void ReleaseGraphics(); // jejich destrukce

    // [x,y] jsou client souradnice okna; vrati [column,row] - souradnice bunky
    // pokud je getNearest TRUE, vrati nejblizsi bunku i v pripade, ze neni primo pod bodem
    // vraci TRUE, pokud nasel bunku a nastavil promenne column a row
    BOOL HitTest(int x, int y, int* column, int* row, BOOL getNearest);
    BOOL HitTestRow(int y, int* row, BOOL getNearest);
    BOOL HitTestColumn(int x, int* column, BOOL getNearest);
    // vrati TRUE, pokud je x priblizne nad delicim sloupcem
    // column a offset mohou byt NULL
    BOOL HitTestColumnSplit(int x, int* column, int* offset);

    // zahaji editaci bunky pod focusem
    //    void OnEditCell();

    // najde sloupec s indexem visibleIndex a pokud ho najde,
    // nastavi index dle jeho umisteni v poli Database.Column a xPos na
    // jeho x souradnici a vrati TRUE
    // jinak vrati FALSE
    BOOL GetColumnInfo(int visibleIndex, int* index, int* xPos);

    // zapina a vypina casovac a stavove promenne behem tazeni bloku
    void BeginSelectionDrag(CDragSelectionMode mode);
    void EndSelectionDrag();
    void OnTimer(WPARAM wParam);

    // ukoncuje tazeni sirky sloupce
    void EndColumnDrag();

    void OnHScroll(int scrollCode, int pos);
    void OnVScroll(int scrollCode, int pos);

    // volame po zmene velikosti okna, aby nedoslo k nesmyslnemu odrolovani
    void CheckAndCorrectBoundaries();

    // pripravi buffer a necha Salamander rozpoznat kodovou stranku
    void RecognizeCodePage();
    // konverti znaky v bufferu text
    void CodeCharacters(char* text, size_t textLen);

    void SetViewerTitle();

    // vrati bod nad focusem nebo vybranym blokem
    void GetContextMenuPos(POINT* p);

    // na souradnici 'p' vybali kontextove menu pro oznaceny blok
    void OnContextMenu(const POINT* p);

    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }
    void ResetMouseWheelAccumulatorHandler(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CViewerWindow;
};
