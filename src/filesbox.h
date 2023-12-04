// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// ****************************************************************************

class CFilesWindow;
class CFilesBox;

#define UPDATE_VERT_SCROLL 0x00000001
#define UPDATE_HORZ_SCROLL 0x00000002

enum CViewModeEnum
{
    vmBrief,      // nekolik sloupcu dat; pouze vodorovne rolovatko; spodni polozky jsou vzdy cele viditelne
    vmDetailed,   // jeden sloupec dat; zobrzene obe rolovatka; posledni radek nemusi byt cely viditelny
    vmIcons,      // velke ikony zleva doprava a pak shora dolu; pouze svisle rolovatko
    vmThumbnails, // nahledy zleva doprava a pak shora dolu; pouze svisle rolovatko
    vmTiles       // velke (48x48) ikony zleva doprava a pak shora dolu; pouze svisle rolovatko
};

//****************************************************************************
//
// CBottomBar
//

// drzi scrollbaru a obdelnicek za ni
class CBottomBar : public CWindow
{
protected:
    HWND HScrollBar;
    BOOL VertScrollSpace;   // drzet prostor pro svisle roovatko
    CFilesBox* RelayWindow; // pro predavani messages

public:
    CBottomBar();
    ~CBottomBar();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void LayoutChilds(); // napocita rozmery obdelniku a rozmisti child okna

    friend class CFilesBox;
};

//****************************************************************************
//
// CHeaderLine
//

enum CHeaderHitTestEnum
{
    hhtNone,    // plocha za polsedni polozkou
    hhtItem,    // polozka
    hhtDivider, // oodelovac polozky, ktera ma nastavitelnou sirku
};

class CHeaderLine : public CWindow
{
protected:
    CFilesBox* Parent;
    TDirectArray<CColumn>* Columns; // ukazatel do pole drzeneho v CFilesWindow
    int Width;
    int Height;
    int HotIndex;        // index sloupce, ktery je prave vysviceny
    BOOL HotExt;         // ma vyznam pouze je-li HotIndex==0 (Name)
    int DownIndex;       // index sloupce, kde doslo k LDOWN
    BOOL DownVisible;    // je prave stikly
    int DragIndex;       // index sloupce, jehoz sirka je nastavovana
    BOOL MouseIsTracked; // je mys sledovana pomoci TrackMouseEvent?
    int OldDragColWidth; // sirka sloupce pred zacatkem tazeni; ma vyznam pri tazeni sirky sloupce

public:
    CHeaderLine();
    ~CHeaderLine();

    // obehne pole Columns a nastavi sloupcum minimalni sirky
    // na zaklade nazvu sloupcu a jestli je podle sloupce mozne
    // radit (potom rezervuje prostor pro symbol sipky
    void SetMinWidths();

    // Urci, ve ktere casti se naleza bod [xPos, yPos] (v client souradnicich).
    // Pokud vrati hhtItem nebo hhtDivider, nastavi do promenne 'index'
    // index odpovidajiciho sloupce v poli Columns.
    // Pokud dojde na kliknuti na "Name", bude nastavena promenna 'extInName'
    // Je-li kliknuto na "Ext", bude rovna TRUE. Jinak bude FALSE.
    CHeaderHitTestEnum HitTest(int xPos, int yPos, int& index, BOOL& extInName);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PaintAllItems(HDC hDC, HRGN hUpdateRgn);
    void PaintItem(HDC hDC, int index, int x = -1); // pro 'x==-1' se napocita spravna pozice
    void PaintItem2(int index);
    void Cancel();

    friend class CFilesBox;
    friend class CFilesWindow;
};

//****************************************************************************
//
// CFilesBox
//

class CFilesBox : public CWindow
{
protected:
    CFilesWindow* Parent;
    HDC HPrivateDC; // kazdy panel ma sve DC
    HWND HVScrollBar;
    HWND HHScrollBar;
    CBottomBar BottomBar;
    CHeaderLine HeaderLine;

    RECT ClientRect;    // rozmery celeho okna
    RECT HeaderRect;    // poloha header controlu v ramci ClientRect
    RECT VScrollRect;   // poloha svisle scrollbary v ramci ClientRect
    RECT BottomBarRect; // poloha vodorovne scrollbary v ramci ClientRect
    RECT FilesRect;     // poloha vlastni kreslici polochy v ramci ClientRect

    int TopIndex;            // vmBrief || vmDetailed: index prvni zobrazene polozky
                             // vmIcons || vmThumbnails: odrolovani panelu v bodech
    int XOffset;             // X souradnice prvniho zobrazeneho bodu
    int ItemsCount;          // celkovy pocet polozek
    int EntireItemsInColumn; // pocet plne viditelnych polozek ve sloupci
    int ItemsInColumn;       // celkovy pocet radku
    int EntireColumnsCount;  // pocet plne viditelnych sloupcu - plati pouze v Brief rezimu
    int ColumnsCount;        // celkovy pocet sloupcu

    SCROLLINFO OldVertSI; // abychom nenastavovali scrollbar zbytecne
    SCROLLINFO OldHorzSI; // abychom nenastavovali scrollbar zbytecne

    CViewModeEnum ViewMode;
    BOOL HeaderLineVisible; // je zobrazena header line?

    int ItemHeight; // vyska polozky
    int ItemWidth;

    int ThumbnailWidth; // rozmery thumbnailu, rizene promennou CConfiguration::ThumbnailSize
    int ThumbnailHeight;

    // slouzi pro akumulaci mikrokroku pri otaceni koleckem mysi, viz
    // http://msdn.microsoft.com/en-us/library/ms997498.aspx (Best Practices for Supporting Microsoft Mouse and Keyboard Devices)
    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal

public:
    CFilesBox(CFilesWindow* parent);

    // nastavi pocet polozek v panelu
    // count             - pocet polozek v panelu
    // suggestedXOffset  - novy XOffset
    // suggestedTopIndex - novy TopIndex
    // disableSBUpdate   - pokud je FALSE, nebude zmena promitnuta do scrollbar
    void SetItemsCount(int count, int xOffset, int topIndex, BOOL disableSBUpdate);
    void SetItemsCount2(int count); // jednoducha verze, pouze nastavi pocet a zavola UpdateInternalData()
    int GetEntireItemsInColumn();

    int GetColumnsCount();

    void SetMode(CViewModeEnum mode, BOOL headerLine);

    int GetItemWidth() { return ItemWidth; }
    int GetItemHeight() { return ItemHeight; }
    void SetItemWidthHeight(int width, int height);

    int GetTopIndex() { return TopIndex; }
    int GetXOffset() { return XOffset; }

    // vraci index polozky na souradnicich 'x' a 'y', ridi se 'nearest'
    // pokud je 'labelRect' != NULL, vrati v nem polohu ramecku kolem textu
    // pokud nenajde zadnou polozku odpovidajici kriteriim, vraci INT_MAX
    int GetIndex(int x, int y, BOOL nearest = FALSE, RECT* labelRect = NULL);

    void PaintAllItems(HRGN hUpdateRgn, DWORD drawFlags);
    void PaintItem(int index, DWORD drawFlags);

    void PaintHeaderLine(); // pokud je zobrazen, bude prekreslen

    // v 'rect' vrati obsany obdelnik kolem polozky 'index'
    // vrati TRUE, pokud je obdelnik alespon castecne viditelny
    BOOL GetItemRect(int index, RECT* rect);

    // forcePaint - i v pripade, ze nedojde k rolovani bude polozka prekreslena
    // scroll: FALSE - polozka bude plne viditelna; TRUE - polozka bude alespon castecne viditelna
    // selFocChangeOnly: pokud nedojde k odrolovani, bude pri kresleni polozky pouzit flag DRAWFLAG_SELFOC_CHANGE
    void EnsureItemVisible(int index, BOOL forcePaint = FALSE, BOOL scroll = FALSE, BOOL selFocChangeOnly = FALSE);

    // vrati novy TopIndex, ktery by nastavila metoda EnsureItemVisible pro scroll == FALSE
    int PredictTopIndex(int index);

    // specialni verze funkce: odroluje pouze s casti obrazovky, ktera neobsahuje
    // polozku index; potom vykresli zbyle polozky
    void EnsureItemVisible2(int newTopIndex, int index);

    // vraci TRUE pokud je polozka index alespon castecne viditelna; neni-li 'isFullyVisible' NULL,
    // vraci v nem TRUE pokud je polozka cela videt (FALSE = neni videt nebo jen castecne)
    BOOL IsItemVisible(int index, BOOL* isFullyVisible);

    // vraci index prvni alespon castecne viditelne polozky a pocet alespon
    // castecne viditelnych polozek (urcuje vsechny alespon castecne viditelne
    // polozky - pouziva se pri priorizaci nacitani ikonek a thumbnailu)
    void GetVisibleItems(int* firstIndex, int* count);

    CHeaderLine* GetHeaderLine() { return &HeaderLine; }

    // nuluje akumulator, ktery pouzivame pri detekci mouse wheel; po vynulovani se pri dalsim otacenim zacina nacisto
    // MS doporucuji v Best Practices for Supporting Microsoft Mouse and Keyboard Devices (http://msdn.microsoft.com/en-us/library/ms997498.aspx)
    // nulovat akumulator pri zmene otaceni kolecka nebo focusu okna
    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }

protected:
    void LayoutChilds(BOOL updateAndCheck = TRUE);                               // napocita rozmery obdelniku a rozmisti child okna
    void SetupScrollBars(DWORD flags = UPDATE_VERT_SCROLL | UPDATE_HORZ_SCROLL); // nastavi info scrollbaram
    BOOL ShowHideChilds();                                                       // na zaklade promenne Mode zobrazi/zhasne childy; pri zmene vrati TRUE, jinak vrati FALSE
    void UpdateInternalData();
    void CheckAndCorrectBoundaries(); // zajisti, aby byly rolovatka v mozne poloze, pripadne je upravi

    void OnHScroll(int scrollCode, int pos);
    void OnVScroll(int scrollCode, int pos);

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CFilesWindow;
    friend class CFilesMap;
    friend class CScrollPanel;
    friend class CBottomBar;
    friend class CHeaderLine;
};
