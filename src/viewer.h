// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define VIEW_BUFFER_SIZE 60000 // 0.5 * VIEW_BUFFER_SIZE musi byt > max. delka \
                               // zobrazitelneho radku
#define BORDER_WIDTH 3         // oddeluje text od okraje okna
#define APROX_LINE_LEN 1000

#define FIND_TEXT_LEN 201                    // +1; POZOR: melo by byt shodne s GREP_TEXT_LEN
#define FIND_LINE_LEN 10000                  // musi byt > FIND_TEXT_LEN i max. delka radky pro REGEXP (pro GREP jine makro)
#define TEXT_MAX_LINE_LEN 10000              // pri delsi radce se ptame na prechod do hexa rezimu, musi byt <= FIND_LINE_LEN
#define RECOGNIZE_FILE_TYPE_BUFFER_LEN 10000 // kolik znaku ze zacatku souboru se ma pouzit pro rozpoznavani typu souboru (RecognizeFileType())

#define VIEWER_HISTORY_SIZE 30 // pocet pamatovanych stringu

// menu positions - pri zmene menu predelat !
#define VIEWER_FILE_MENU_INDEX 0         // v hlavnim menu viewru
#define VIEWER_FILE_MENU_OTHFILESINDEX 3 // v submenu File hlavniho menu viewru
#define VIEWER_EDIT_MENU_INDEX 1         // v hlavnim menu viewru
#define VIEW_MENU_INDEX 3                // v hlavnim menu viewru
#define CODING_MENU_INDEX 4              // v hlavnim menu viewru
#define OPTIONS_MENU_INDEX 5             // v hlavnim menu viewru

#define WM_USER_VIEWERREFRESH WM_APP + 201 // [0, 0] - ma se provest refresh

#ifndef INSIDE_SALAMANDER
char* LoadStr(int resID);
char* GetErrorText(DWORD error);
#endif // INSIDE_SALAMANDER

extern char* ViewerHistory[VIEWER_HISTORY_SIZE];

void HistoryComboBox(HWND hWindow, CTransferInfo& ti, int ctrlID, char* Text,
                     int textLen, BOOL hexMode, int historySize, char* history[],
                     BOOL changeOnlyHistory = FALSE);
void DoHexValidation(HWND edit, const int textLen);
void ConvertHexToString(char* text, char* hex, int& len);

int GetHexOffsetMode(unsigned __int64 fileSize, int& hexOffsetLength);
void PrintHexOffset(char* s, unsigned __int64 offset, int mode);

void GetDefaultViewerLogFont(LOGFONT* lf);

// ****************************************************************************

class CFindSetDialog : public CCommonDialog
{
public:
    int Forward, // forward/backward (1/0)
        WholeWords,
        CaseSensitive,
        HexMode,
        Regular;

    char Text[FIND_TEXT_LEN];

    CFindSetDialog(HINSTANCE modul, int resID, UINT helpID)
        : CCommonDialog(modul, resID, helpID, NULL, ooStatic)
    {
        Forward = TRUE;
        WholeWords = FALSE;
        CaseSensitive = FALSE;
        HexMode = FALSE;
        Regular = FALSE;
        Text[0] = 0;
    }

    CFindSetDialog& operator=(CFindSetDialog& d)
    {
        Forward = d.Forward;
        WholeWords = d.WholeWords;
        CaseSensitive = d.CaseSensitive;
        HexMode = d.HexMode;
        Regular = d.Regular;
        memmove(Text, d.Text, FIND_TEXT_LEN);
        return *this;
    }

    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    int CancelHexMode, // jen pro spravnou funkci tlacitka Cancel
        CancelRegular;
};

// ****************************************************************************

class CViewerGoToOffsetDialog : public CCommonDialog
{
public:
    CViewerGoToOffsetDialog(HWND parent, __int64* offset)
        : CCommonDialog(HLanguage, IDD_VIEWERGOTOOFFSET, IDD_VIEWERGOTOOFFSET, parent) { Offset = offset; }

    virtual void Validate(CTransferInfo& ti);
    virtual void Transfer(CTransferInfo& ti);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

protected:
    __int64* Offset;
};

// ****************************************************************************

enum CViewType
{
    vtText,
    vtHex
};

class CViewerWindow : public CWindow
{
public:
    CViewerWindow(const char* fileName, CViewType type, const char* caption,
                  BOOL wholeCaption, CObjectOrigin origin, int enumFileNamesSourceUID,
                  int enumFileNamesLastFileIndex);
    ~CViewerWindow();

    void OpenFile(const char* file, const char* caption, BOOL wholeCaption); // neovlada Lock

    virtual BOOL Is(int type) { return type == otViewerWindow || CWindow::Is(type); }
    BOOL IsGood() { return Buffer != NULL && ViewerFont != NULL; }
    void InitFindDialog(CFindSetDialog& dlg)
    {
        FindDialog = dlg;
        if (FindDialog.Text[0] == 0)
            return;
        else
        {
            if (FindDialog.Regular)
            {
                RegExp.Set(FindDialog.Text, 0);
            }
            else
            {
                if (FindDialog.HexMode)
                {
                    char hex[FIND_TEXT_LEN];
                    int len;
                    ConvertHexToString(FindDialog.Text, hex, len);
                    SearchData.Set(hex, len, 0);
                }
                else
                    SearchData.Set(FindDialog.Text, 0);
            }
        }
    }

    HANDLE GetLockObject(); // objekt pro disk-cache - view ze ZIPu
    void CloseLockObject();

    void ConfigHasChanged(); // vola se po OK v konfiguracnim dialogu

    // vraci text pro Find - (null-terminated) oznaceny blok, 'buf' je min.
    // FIND_TEXT_LEN bytu; vraci TRUE pokud je buf naplnen (existuje blok, atd.); vraci pocet
    // zapsanych znaku bez null-terminatoru do 'len'
    BOOL GetFindText(char* buf, int& len);

protected:
    void FatalFileErrorOccured(DWORD repeatCmd = -1); // vola se pokud nastane chyba v souboru (nutny refresh/clear viewru)

    void OnVScroll();

    void CodeCharacters(unsigned char* start, unsigned char* end);
    // pokud je 'hFile' NULL, Prepare/LoadBefore/LoadBehind si soubor otevrou a zavrou
    // pokud 'hFile' ukazuje na promennou (na zacatku inicializovat jeji hodnotu na NULL),
    // metody si soubor otevrou a nastavi handle souboru do teto promenne (v pripade uspesneho otevreni).
    // Pri pristim volani jiz soubor neoteviraji a pouziji handle z teto promenne.
    // Pri odchodu handle nezaviraji, to uz si musi zajistit volajici. Jedna se o
    // optimalizaci pro sitove disky, kde opakovane otvirani/zavirani souboru ukrutne
    // zdrzovalo pri hledani.
    BOOL LoadBefore(HANDLE* hFile);
    BOOL LoadBehind(HANDLE* hFile);

    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode tu nevznika (nestava se TRUE)
    __int64 Prepare(HANDLE* hFile, __int64 offset, __int64 bytes, BOOL& fatalErr);

    void GoToEnd() { SeekY = MaxSeekY; }
    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je TRUE pokud se prepina do Hex rezimu
    void FileChanged(HANDLE file, BOOL testOnlyFileSize, BOOL& fatalErr, BOOL detectFileType,
                     BOOL* calledHeightChanged = NULL);
    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je TRUE pokud se prepina do Hex rezimu
    void HeightChanged(BOOL& fatalErr);
    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je TRUE pokud se prepina do Hex rezimu
    __int64 ZeroLineSize(BOOL& fatalErr, __int64* firstLineEndOff = NULL, __int64* firstLineCharLen = NULL);

    // jen pro textove zobrazeni, pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je
    // TRUE pokud se prepina do Hex rezimu
    __int64 FindSeekBefore(__int64 seek, int lines, BOOL& fatalErr, __int64* firstLineEndOff = NULL,
                           __int64* firstLineCharLen = NULL, BOOL addLineIfSeekIsWrap = FALSE);
    // 'hFile' parametr viz komentar k Prepare(), pokud doslo k chybe cteni, je fatalErr == TRUE,
    // ExitTextMode tu nevznika (nestava se TRUE)
    BOOL FindNextEOL(HANDLE* hFile, __int64 seek, __int64 maxSeek, __int64& lineEnd, __int64& nextLineBegin, BOOL& fatalErr);
    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je TRUE pokud se prepina do Hex rezimu
    BOOL FindPreviousEOL(HANDLE* hFile, __int64 seek, __int64 minSeek, __int64& lineBegin,
                         __int64& previousLineEnd, BOOL allowWrap,
                         BOOL takeLineBegin, BOOL& fatalErr, int* lines, __int64* firstLineEndOff = NULL,
                         __int64* firstLineCharLen = NULL, BOOL addLineIfSeekIsWrap = FALSE);

    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode je TRUE pokud se prepina do Hex rezimu
    __int64 FindBegin(__int64 seek, BOOL& fatalErr);
    void ChangeType(CViewType type);

    void Paint(HDC dc);
    void SetScrollBar();
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    // postne WM_MOUSEMOVE (pouziva se k posunu konce bloku na nove souradnice mysi nebo
    // prepocitani konce bloku pri posunu view)
    void PostMouseMove();

    // posun view o radek nahoru
    BOOL ScrollViewLineUp(DWORD repeatCmd = -1, BOOL* scrolled = NULL, BOOL repaint = TRUE,
                          __int64* firstLineEndOff = NULL, __int64* firstLineCharLen = NULL);

    // posun view o radek dolu
    BOOL ScrollViewLineDown(BOOL fullRedraw = FALSE);

    // invalidate + pripadne update vybranych radek viewu
    void InvalidateRows(int minRow, int maxRow, BOOL update = TRUE);

    // pripadne posune OriginX, aby byla videt x-ova souradnice 'x' ve view
    void EnsureXVisibleInView(__int64 x, BOOL showPrevChar, BOOL& fullRedraw,
                              __int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE,
                              __int64 maxLineLen = -1);

    // zjisti maximalni delku viditelne radky ve view (textovy rezim: musi byt prekresleno,
    // jinak je neaktualni LineOffset)
    __int64 GetMaxVisibleLineLen(__int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE);

    // zjisti maximum pro OriginX pro soucasne view (textovy rezim: musi byt prekresleno,
    // jinak je neaktualni LineOffset)
    __int64 GetMaxOriginX(__int64 newFirstLineLen = -1, BOOL ignoreFirstLine = FALSE, __int64 maxLineLen = -1);

    // zjisti x-ovou souradnici 'x' pozice 'offset' v souboru na radku; neni-li 'lineInView' -1,
    // vezme se radka 'lineInView' z LineOffset a 'lineBegOff', 'lineCharLen' a 'lineEndOff' se
    // ignoruji
    BOOL GetXFromOffsetInText(__int64* x, __int64 offset, int lineInView, __int64 lineBegOff = -1,
                              __int64 lineCharLen = -1, __int64 lineEndOff = -1);

    // zjisti nejblizsi pozici 'offset' v souboru pro navrhovanou x-ovou souradnici 'suggestedX' v radku,
    // v 'x' vraci x-ovou souradnici pozice 'offset' v souboru na radku; neni-li 'lineInView' -1,
    // vezme se radka 'lineInView' z LineOffset a 'lineBegOff', 'lineCharLen' a 'lineEndOff' se
    // ignoruji
    BOOL GetOffsetFromXInText(__int64* x, __int64* offset, __int64 suggestedX, int lineInView,
                              __int64 lineBegOff = -1, __int64 lineCharLen = -1,
                              __int64 lineEndOff = -1);

    // vraci offset v souboru podle souradnic [x, y] na obrazovce; je-li 'leftMost'
    // FALSE je ruzny offset leve a prave pulky znaku, je-li TRUE pak kdekoliv na znaku
    // je stale stejny offset (offset tohoto znaku) - pouziva se pri detekci kliknuti do bloku;
    // je-li 'onHexNum' != NULL, a jde o hex rezim a [x, y] je na hex cislici nebo na znaku,
    // bude '*onHexNum'==TRUE
    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode tu nevznika (nestava se TRUE)
    BOOL GetOffset(__int64 x, __int64 y, __int64& offset, BOOL& fatalErr, BOOL leftMost = FALSE,
                   BOOL* onHexNum = NULL);

    // vraci offset 'offset' v souboru na x-ove souradnici 'x' v radce zacinajici na offsetu
    // 'lineBegOff' o delce 'lineCharLen' zobrazenych znaku (expandovane taby, v hex rezimu se
    // nepouziva, dat na 0) a koncici na offsetu 'lineEndOff' (v hex rezimu se nepouziva, dat na 0);
    // v 'offsetX' vraci x-ovou souradnici 'offset' (udelane jen pro textovy rezim, nemusi se
    // shodovat s 'x'); je-li 'onHexNum' != NULL, a jde o hex rezim a na x-ove souradnici 'x' je
    // hexa cislice nebo znak, bude '*onHexNum'==TRUE; pokud doslo k chybe cteni, je fatalErr == TRUE,
    // ExitTextMode tu nevznika (nestava se TRUE); je-li 'getXFromOffset' TRUE (umime jen pro
    // textovy rezim view, 'offset', 'offsetX' a 'onHexNum' dat NULL), vraci misto offsetu
    // souradnici X (ve 'foundX') znaku na offsetu 'findOffset'
    BOOL GetOffsetOrXAbs(__int64 x, __int64* offset, __int64* offsetX, __int64 lineBegOff, __int64 lineCharLen,
                         __int64 lineEndOff, BOOL& fatalErr, BOOL* onHexNum, BOOL getXFromOffset = FALSE,
                         __int64 findOffset = -1, __int64* foundX = NULL);

    // u velke selectiony (nad 100MB) se zepta usera, jestli vazne chce alokovat selectionu
    // pro D&D nebo clipboard
    BOOL CheckSelectionIsNotTooBig(HWND parent, BOOL* msgBoxDisplayed = NULL);

    // pokud doslo k chybe cteni, je fatalErr == TRUE, ExitTextMode tu nevznika (nestava se TRUE)
    HGLOBAL GetSelectedText(BOOL& fatalErr); // text pro operace s clipboardem a drag&drop

    void SetToolTipOffset(__int64 offset);

    void SetViewerCaption();

    // nastaveni CodeType + UseCodeTable + CodeTable + titulku okna
    // POZOR: je nutne, aby CodeTables.Valid(c) vracelo TRUE
    void SetCodeType(int c);

    BOOL CreateViewerBrushs();
    void ReleaseViewerBrushs();
    void SetViewerFont();

    void ResetMouseWheelAccumulator()
    {
        MouseWheelAccumulator = 0;
        MouseHWheelAccumulator = 0;
    }

    void ReleaseMouseDrag();

    void FindNewSeekY(__int64 newSeekY, BOOL& fatalErr);

    // interne vola SalMessageBox, jen pro nej zablokuje Paint (jen maze pozadi vieweru, nesaha na soubor)
    int SalMessageBoxViewerPaintBlocked(HWND hParent, LPCTSTR lpText, LPCTSTR lpCaption, UINT uType);

    unsigned char* Buffer; // Buffer o velikosti VIEW_BUFFER_SIZE
    char* FileName;        // aktualne prohlizeny soubor
    __int64 Seek,          // offset 0. bytu v Bufferu v souboru
        Loaded,            // pocet platnych bytu v Bufferu
        OriginX,           // prvni zobrazeny sloupec (ve znacich)
        SeekY,             // seek prvniho zobrazeneho radku
        MaxSeekY,          // seek konce prohlizeneho souboru
        FileSize,          // velikost souboru
        ViewSize,          // velikost zobrazovaneho useku souboru (v bytech)
        FirstLineSize,     // velikost 1. zobrazene radky (v bytech)
        LastLineSize;      // velikost posledni cele zobrazene radky (v bytech)

    __int64 StartSelection,           // seek zacatku oznaceni (od toho znaku vcetne) (-1 = zadna selectiona jeste nebyla)
        EndSelection;                 // seek konce oznaceni (az k tomu znaku ne vcetne) (-1 = zadna selectiona jeste nebyla)
    int TooBigSelAction;              // D&D nebo auto-/copy-to-clip bloku nad 100MB: 0 = zeptat se, 1 = ANO, 2 = NE
    int EndSelectionRow;              // y offset radky, ve ktere je EndSelection(relativni k client area)
                                      // validni pouze pri tazeni bloku; slouzi k optimalizaci paintu
                                      // pokud je -1, optimalizace se neprovede
    __int64 EndSelectionPrefX;        // preferovana x-ova souradnice pri tazeni konce bloku pres Shift+sipky nahoru/dolu (-1 = neni)
    TDirectArray<__int64> LineOffset; // pole s offsety zacatku + koncu (bez EOLu) radek + delkama v zobrazenych znacich (pro kazdou radku trojice cisel)
    BOOL WrapIsBeforeFirstLine;       // jen u text-view pri wrap rezimu: pred prvni radkou view je wrap (a ne EOL)
    BOOL MouseDrag;                   // tahne blok mysi ?
    BOOL ChangingSelWithShiftKey;     // meni oznaceni pres Shift+klavesu (sipky, End, Home)

    CFindSetDialog FindDialog;
    CSearchData SearchData;
    CRegularExpression RegExp;
    __int64 FindOffset,              // seek od ktereho hledat
        LastFindSeekY,               // seek zacatku prvniho radku obrazovky po hledani, pro detekci pohybu sem-tam
        LastFindOffset;              // seek od ktereho se ma hledat (nastaveny po hledani), pro detekci pohybu sem-tam
    BOOL ResetFindOffsetOnNextPaint; // TRUE = nastavit FindOffset pri nasledujicim WM_PAINT (az po vykresleni okna se zjisti velikost viditelne stranky a lze nastavit FindOffset i pro hledani pozpatku)
    BOOL SelectionIsFindResult;      // TRUE = oznaceni vzniklo jako vysledek findu

    int DefViewMode; // 0 = Auto-Select, 1 = Text, 2 = Hex
    CViewType Type;  // typ zobrazeni
    BOOL EraseBkgnd; // aby se jednou nazacatku smazalo pozadi

    int Width,  // sirka okna (v bodech)
        Height; // vyska okna (v bodech)

    BOOL EnablePaint; // kvuli rekurzivnimu volani Paintu pri chybach: FALSE = jen maze pozadi vieweru (nesaha na soubor)

    BOOL ScrollToSelection; // pri dalsim kresleni se odsune na pozici oznaceni (OriginX)

    double ScrollScaleX,  // koeficient horizontalni scroll-bary
        ScrollScaleY;     // koeficient vertikalni scroll-bary
    BOOL EnableSetScroll; // behem dragu nebudu refreshovat udaje na scrollbare

    __int64 ToolTipOffset; // hex mode: offset v souboru (zobrazuje se v tooltipu)
    HWND HToolTip;         // okno tooltipu

    HANDLE Lock; // handle pro disk-cache

    BOOL WrapText; // lokalni kopie Configuration.WrapText

    BOOL CodePageAutoSelect;  // lokalni kopie Configuration.CodePageAutoSelect
    char DefaultConvert[200]; // lokalni kopie Configuration.DefaultConvert

    BOOL ExitTextMode;  // TRUE = je treba urychlene ukoncit zpracovani akt. zpravy, prepiname do hex
                        //        modu (soubor se pro textovy rezim nehodi, nema EOLs)
    BOOL ForceTextMode; // TRUE = za kazdou cenu to chce mit user v textovem rezimu (pocka si)

    int CodeType;        // ciselna identifikace kodovani pamet objektu CodeTables pro toto okno vieweru
    BOOL UseCodeTable;   // ma se prekodovavat podle CodeTable?
    char CodeTable[256]; // kodovaci tabulka

    char CurrentDir[MAX_PATH]; // cesta pro open dialog

    BOOL WaitForViewerRefresh;   // TRUE - ceka se na WM_USER_VIEWERREFRESH, ostatni prikazy se skipnou
    __int64 LastSeekY;           // SeekY pred chybou
    __int64 LastOriginX;         // OriginX pred chybou
    DWORD RepeatCmdAfterRefresh; // prikaz k zopakovani po refreshi (-1 = zadny prikaz)

    char* Caption;     // neni-li NULL, obsahuje navrhovany caption okna vieweru
    BOOL WholeCaption; // ma vyznam pokud je Caption != NULL. TRUE -> v titulku
                       // vieweru bude zobrazen pouze retezec Caption; FALSE -> za
                       // Caption se pripoji standardni " - Viewer".

    BOOL CanSwitchToHex,           // TRUE pokud je mozne prepnuti do "hex" pri vice nez 10000 znacich na radku
        CanSwitchQuietlyToHex,     // TRUE pokud se neni treba na prepnuti ptat (pri loadu souboru)
        FindingSoDonotSwitchToHex; // TRUE pokud se ma blokovat prepnuti do "hex" pri vice nez 10000 znacich na radku (behem hledani v textu je to nezadouci)

    int HexOffsetLength; // hex-mode: pocet znaku v offsetu (v prvnim sloupci zleva)

    // GDI objekty (kazdy thread ma sve)
    HBRUSH BkgndBrush;    // solid brush barvy pozadi okna
    HBRUSH BkgndBrushSel; // solid brush barvy pozadi okna - selected text

    CBitmap Bitmap;
    HFONT ViewerFont;

    int EnumFileNamesSourceUID;     // UID naseho zdroje pro enumeraci jmen ve vieweru
    int EnumFileNamesLastFileIndex; // index prave prohlizeneho souboru

    WPARAM VScrollWParam; // wParam z WM_VSCROLL/SB_THUMB*; -1 pokud neprobiha tazeni
    WPARAM VScrollWParamOld;

    int MouseWheelAccumulator;  // vertical
    int MouseHWheelAccumulator; // horizontal
};

// ****************************************************************************

BOOL InitializeViewer();
void ReleaseViewer();
void ClearViewerHistory(BOOL dataOnly); // promaze historie; pro dataOnly==FALSE maze combbox Find dialogu (je-li)
void UpdateViewerColors(SALCOLOR* colors);

extern const char* CVIEWERWINDOW_CLASSNAME; // trida okna vieweru

extern CWindowQueue ViewerWindowQueue; // seznam vsech oken vieweru

extern CFindSetDialog GlobalFindDialog; // pro inicializaci noveho okna vieweru

extern BOOL UseCustomViewerFont; // pokud je TRUE, pouzije se struktura ViewerLogFont ulozena v konfiguraci; jinak default hodnoty
extern LOGFONT ViewerLogFont;
extern HMENU ViewerMenu;
extern HACCEL ViewerTable;
extern int CharWidth, // sirka znaku (v bodech)
    CharHeight;       // vyska znaku (v bodech)

// Vista: font fixedsys obsahuje znaky, ktere nemaji ocekavanou sirku (i kdyz je to fixed-width font), proto
// omerujeme jednotlive znaky a ty ktere nemaji spravnou sirku mapujeme na nahradni znak o spravne sirce
extern CRITICAL_SECTION ViewerFontMeasureCS; // kriticka sekce pro omerovani fontu
extern BOOL ViewerFontMeasured;              // TRUE = uz jsme omerovali font vznikly z ViewerLogFont; FALSE = je potreba provest omereni fontu
extern BOOL ViewerFontNeedsMapping;          // TRUE = je nutne pouzivat ViewerFontMapping; FALSE = font je OK, mapovani neni potreba
extern char ViewerFontMapping[256];          // premapovani na znaky, ktere maji ocekavanou fixni sirku

extern HANDLE ViewerContinue; // pomocny event - cekani na rozebehnuti threadu message-loopy

BOOL OpenViewer(const char* name, CViewType mode, int left, int top, int width, int height,
                UINT showCmd, BOOL returnLock, HANDLE* lock, BOOL* lockOwner,
                CSalamanderPluginViewerData* viewerData, int enumFileNamesSourceUID,
                int enumFileNamesLastFileIndex);
