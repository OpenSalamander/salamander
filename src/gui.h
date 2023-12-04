// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//****************************************************************************
//
// CProgressBar
//
// Trida je vzdy alokovana (CObjectOrigin origin = ooAllocated)

class CProgressBar : public CWindow
{
public:
    // hDlg je parent window (dialog nebo okno)
    // ctrlID je ID child okna
    CProgressBar(HWND hDlg, int ctrlID);
    ~CProgressBar();

    // SetProgress lze volat z libovolneho threadu, vnitrne se zasila WM_USER_SETPROGRESS
    // thread progress bary vsak musi bezet
    void SetProgress(DWORD progress, const char* text = NULL);
    void SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                      const char* text = NULL);

    void SetSelfMoveTime(DWORD time);
    void SetSelfMoveSpeed(DWORD moveTime);
    void Stop();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hDC);

    void MoveBar();

protected:
    int Width, Height;
    DWORD Progress;
    CBitmap* Bitmap;     // bitmapa pro memDC -> cache paintu
    int BarX;            // Xova souradnice obdelniku pro neznamy progress (pro Progress==-1)
    BOOL MoveBarRight;   // jede obdelnik vpravo?
    DWORD SelfMoveTime;  // 0: po zavolani SetProgress(-1) se obdelnicek pohne pouze o jeden dilek (0 je implicitni hodnota)
                         // vic jak 0: cas v [ms], po ktery se jeste budeme hybat po zavolani SetProgress(-1)
    DWORD SelfMoveTicks; // ulozena hodnota GetTickCount() behem posledniho zavolani SetSelfMoveTime()
    DWORD SelfMoveSpeed; // rychlost pohybu obdelnicku: hodnota je v [ms] a udava cas, po kterem se obdelnicek pohne
                         // minimum je 10ms, default hodnota je 50ms -- tedy 20 posunu za vterinu
                         // pozor na nizke hodnoty, samotna animace pak dokaze znatelne vytizit procesor
    BOOL TimerIsRunning; // bezi timer?
    char* Text;          // pokud je ruzny od NULL, bude zobrazen misto cisla
    HFONT HFont;         // font pro progress bar
};

//****************************************************************************
//
// CStaticText
//
// Trida je vzdy alokovana (CObjectOrigin origin = ooAllocated)

class CStaticText : public CWindow
{
public:
    // hDlg je parent window (dialog nebo okno)
    // ctrlID je ID child okna
    // flags je kombinaci hodnot z rodiny STF_* (shared\spl_gui.h)
    CStaticText(HWND hDlg, int ctrlID, DWORD flags);
    ~CStaticText();

    // nastavi Text, vraci TRUE pri uspechu a FALSE pri nedostatku pameti
    BOOL SetText(const char* text);

    // pozor, vraceny Text muze byt NULL
    const char* GetText() { return Text; }

    // nastavi Text (pokud zacina nebo konci na mezeru, da do dvojitych uvozovek),
    // vraci TRUE pri uspechu a FALSE pri nedostatku pameti
    BOOL SetTextToDblQuotesIfNeeded(const char* text);

    // na nekterych filesystemech muze byt jiny oddelovac casti cesty
    // musi byt ruzny od '\0';
    void SetPathSeparator(char separator);

    // priradi text, ktery bude zobrazen jako tooltip
    BOOL SetToolTipText(const char* text);

    // priradi okno a id, kteremu se pri zobrazeni tooltipu zasle WM_USER_TTGETTEXT
    void SetToolTip(HWND hNotifyWindow, DWORD id);

    // pokud je nastaveno na TRUE, tooltip lze vyvolat kliknutim na text nebo
    // stisknutim klaves Up/Down/Space, ma-li control focus
    // tootltip se pak zobrazi tesne pod textem a zustane zobrazen
    // implicitne je nastaveno na FALSE
    void EnableHintToolTip(BOOL enable);

    //    void UpdateControl();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void PrepareForPaint();

    BOOL TextHitTest(POINT* screenCursorPos);
    int GetTextXOffset(); // na zaklade promennych Alignment, Width a TextWidth vraci X posunuti textu
    void DrawFocus(HDC hDC);

    BOOL ToolTipAssigned();

    BOOL ShowHint();

    DWORD Flags;         // flagy pro chovani controlu
    char* Text;          // alokovany text
    int TextLen;         // delka retezce
    char* Text2;         // alokovany text obshujici vypustku; pouziva se se pouze s STF_END_ELLIPSIS nebo STF_PATH_ELLIPSIS
    int Text2Len;        // delka Text2
    int* AlpDX;          // pole delek substringu; pouziva se se pouze s STF_END_ELLIPSIS nebo STF_PATH_ELLIPSIS
    int TextWidth;       // sirka textu v bodech
    int TextHeight;      // vyska textu v bodech
    int Allocated;       // velikost alokovaneho bufferu 'Text' a 'AlpDX'
    int Width, Height;   // rozmery staticu
    CBitmap* Bitmap;     // cache pro kresleni; pouziva se pouze s STF_CACHED_PAINT
    HFONT HFont;         // handlu fontu pouzivany ro kresleni textu
    BOOL DestroyFont;    // pokud je HFont alokovany, je TRUE, jinak je FALSE
    BOOL ClipDraw;       // je treba clipnout kresleni, protoze bychom vylezli ven
    BOOL Text2Draw;      // budeme kreslit z bufferu obsahujici vypustku
    int Alignment;       // 0=left, 1=center, 2=right
    char PathSeparator;  // oddelovac casti cesty; implicitne '\\'
    BOOL MouseIsTracked; // instalovali jsme hlidani opusteni mysi
    // podpora pro tooltips
    char* ToolTipText; // retezec, ktery bude zobrazen jako nas tooltip
    HWND HToolTipNW;   // notifikacni okno
    DWORD ToolTipID;   // a ID pod kterym se ma tool tip dotazat na text
    BOOL HintMode;     // mame tooltip zobrazovat jako Hint?
    WORD UIState;      // zobrazovani akceleratoru
};

//****************************************************************************
//
// CHyperLink
//

class CHyperLink : public CStaticText
{
public:
    // hDlg je parent window (dialog nebo okno)
    // ctrlID je ID child okna
    // flags je kombinaci hodnot z rodiny STF_* (shared\spl_gui.h)
    CHyperLink(HWND hDlg, int ctrlID, DWORD flags = STF_UNDERLINE | STF_HYPERLINK_COLOR);

    void SetActionOpen(const char* file);
    void SetActionPostCommand(WORD command);
    BOOL SetActionShowHint(const char* text);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    void OnContextMenu(int x, int y);
    BOOL ExecuteIt();

protected:
    char File[MAX_PATH]; // pokud je ruzny od 0, predava se do ShellExecute
    WORD Command;        // pokud je ruzny od 0, posti se pri akci
    HWND HDialog;        // parent dialog
};

//****************************************************************************
//
// CColorRectangle
//
// vykresli celou plochu objektu barvou Color
// kombinovat s WS_EX_CLIENTEDGE
//

class CColorRectangle : public CWindow
{
protected:
    COLORREF Color;

public:
    CColorRectangle(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated);

    void SetColor(COLORREF color);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void PaintFace(HDC hdc);
};

//****************************************************************************
//
// CColorGraph
//

class CColorGraph : public CWindow
{
protected:
    HBRUSH Color1Light;
    HBRUSH Color1Dark;
    HBRUSH Color2Light;
    HBRUSH Color2Dark;

    RECT ClientRect;
    double UsedProc;

public:
    CColorGraph(HWND hDlg, int ctrlID, CObjectOrigin origin = ooAllocated);
    ~CColorGraph();

    void SetColor(COLORREF color1Light, COLORREF color1Dark,
                  COLORREF color2Light, COLORREF color2Dark);

    void SetUsed(double used); // used = <0, 1>

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
    virtual void PaintFace(HDC hdc);
};

//****************************************************************************
//
// CBitmapButton
//

class CButton : public CWindow
{
protected:
    DWORD Flags;
    BOOL DropDownPressed;
    BOOL Checked;
    BOOL ButtonPressed;
    BOOL Pressed;
    BOOL DefPushButton;
    BOOL Captured;
    BOOL Space;
    RECT ClientRect;
    // podpora pro tooltips
    BOOL MouseIsTracked;  // instalovali jsme hlidani opusteni mysi
    char* ToolTipText;    // retezec, ktery bude zobrazen jako nas tooltip
    HWND HToolTipNW;      // notifikacni okno
    DWORD ToolTipID;      // a ID pod kterym se ma tool tip dotazat na text
    DWORD DropDownUpTime; // cas v [ms], kdy byl odmackunt drop down, pro ochranu pred novym zamacknutim
    // podpora pro XP Theme
    BOOL Hot;
    WORD UIState; // zobrazovani akceleratoru

public:
    CButton(HWND hDlg, int ctrlID, DWORD flags, CObjectOrigin origin = ooAllocated);
    ~CButton();

    // priradi text, ktery bude zobrazen jako tooltip
    BOOL SetToolTipText(const char* text);

    // priradi okno a id, kteremu se pri zobrazeni tooltipu zasle WM_USER_TTGETTEXT
    void SetToolTip(HWND hNotifyWindow, DWORD id);

    DWORD GetFlags();
    void SetFlags(DWORD flags, BOOL updateWindow);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    virtual void PaintFace(HDC hdc, const RECT* rect, BOOL enabled);

    int HitTest(LPARAM lParam); // vraci 0: nikde; 1: tlacitko; 2: drop down
    void PaintFrame(HDC hDC, const RECT* r, BOOL down);
    void PaintDrop(HDC hDC, const RECT* r, BOOL enabled);
    int GetDropPartWidth();

    void RePaint();
    void NotifyParent(WORD notify);

    BOOL ToolTipAssigned();
};

//****************************************************************************
//
// CColorArrowButton
//
// pozadi s textem, za kterym je jeste sipka - slouzi pro rozbaleni menu
//

class CColorArrowButton : public CButton
{
protected:
    COLORREF TextColor;
    COLORREF BkgndColor;
    BOOL ShowArrow;

public:
    CColorArrowButton(HWND hDlg, int ctrlID, BOOL showArrow, CObjectOrigin origin = ooAllocated);

    void SetColor(COLORREF textColor, COLORREF bkgndColor);
    //    void     SetColor(COLORREF color);

    void SetTextColor(COLORREF textColor);
    void SetBkgndColor(COLORREF bkgndColor);

    COLORREF GetTextColor() { return TextColor; }
    COLORREF GetBkgndColor() { return BkgndColor; }

protected:
    virtual void PaintFace(HDC hdc, const RECT* rect, BOOL enabled);
};

//****************************************************************************
//
// CToolbarHeader
//

//#define TOOLBARHDR_USE_SVG

class CToolBar;

class CToolbarHeader : public CWindow
{
protected:
    CToolBar* ToolBar;
#ifdef TOOLBARHDR_USE_SVG
    HIMAGELIST HEnabledImageList;
    HIMAGELIST HDisabledImageList;
#else
    HIMAGELIST HHotImageList;
    HIMAGELIST HGrayImageList;
#endif
    DWORD ButtonMask;   // pouzita tlacitka
    HWND HNotifyWindow; // kam posilam comandy
    WORD UIState;       // zobrazovani akceleratoru

public:
    CToolbarHeader(HWND hDlg, int ctrlID, HWND hAlignWindow, DWORD buttonMask);

    void EnableToolbar(DWORD enableMask);
    void CheckToolbar(DWORD checkMask);
    void SetNotifyWindow(HWND hWnd) { HNotifyWindow = hWnd; }

protected:
#ifdef TOOLBARHDR_USE_SVG
    void CreateImageLists(HIMAGELIST* enabled, HIMAGELIST* disabled);
#endif

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void OnPaint(HDC hDC, BOOL hideAccel, BOOL prefixOnly);
};

//****************************************************************************
//
// CAnimate
//

/*
class CAnimate: public CWindow
{
  protected:
    HBITMAP          HBitmap;             // bitmapa ze ktere tahame jednotliva policka animace
    int              FramesCount;         // pocet policek v bitmape
    int              FirstLoopFrame;      // pokud jedeme ve smycce, z konce prechazime na toto policko
    SIZE             FrameSize;           // rozmer policka v bodech
    CRITICAL_SECTION GDICriticalSection;  // kriticka sekce pro pristup ke GDI prostredkum
    CRITICAL_SECTION DataCriticalSection; // kriticka sekce pro pristup k datum
    HANDLE           HThread;
    HANDLE           HRunEvent;           // pokud je signed, animacni thread bezi
    HANDLE           HTerminateEvent;     // pokud je signed, thread se ukonci
    COLORREF         BkColor;

    // ridici promenne, prijdou ke slovu kdyz HRunEvent signed
    BOOL             SleepThread;         // thread se ma uspat, HRunEvent bude resetnut

    int              CurrentFrame;        // zero-based index prave zobrazeneho policka
    int              NestedCount;
    BOOL             MouseIsTracked;      // instalovali jsme hlidani opusteni mysi

  public:
    // 'hBitmap'          je bitmapa ze ktere vykreslujeme policka pri animaci; 
    //                    policka musi byt pod sebou a musi mit konstantni vysku
    // 'framesCount'      udava celkovy pocet policek v bitmape
    // 'firstLoopFrame'   zero-based index policka, kam se pri cyklicke
    //                    animaci vracime po dosazeni konce
    CAnimate(HBITMAP hBitmap, int framesCount, int firstLoopFrame, COLORREF bkColor, CObjectOrigin origin = ooAllocated);
    BOOL IsGood();                // dopadnul dobre konstruktor?

    void Start();                 // pokud neanimujeme, zacneme
    void Stop();                  // zastavi animaci a zobrazi uvodni policko
    void GetFrameSize(SIZE *sz);  // vraci rozmer v bodech potrebny pro zobrazeni policka

  protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void Paint(HDC hdc = NULL);   // zobraz soucasne policko; pokud je hdc NULL, vytahni si DC okna
    void FirstFrame();            // nastav Frame na uvodni policko
    void NextFrame();             // nastav Frame na dalsi policko; preskakuj uvodni sekvenci

    // tela threadu
    static unsigned ThreadF(void *param);
    static unsigned AuxThreadEH(void *param);
    static DWORD WINAPI AuxThreadF(void *param);

    // ThreadF bude friend, aby mohl pristupovat na nase data
    friend static unsigned ThreadF(void *param);
};
*/

//
//  ****************************************************************************
// ChangeToArrowButton
//

BOOL ChangeToArrowButton(HWND hParent, int ctrlID);

//
//  ****************************************************************************
// ChangeToIconButton
//

BOOL ChangeToIconButton(HWND hParent, int ctrlID, int iconID);

//
//  ****************************************************************************
// VerticalAlignChildToChild
//
// slouzi k zarovnani "browse" tlacitka za editline / combobox (v resource workshopu je problem se trefit s tlacitkem za combobox)
// upravi velikost a pozici child okna 'alignID' tak, aby sedelo ve stejne vysce (a bylo stejne vysoke) jako child 'toID'
void VerticalAlignChildToChild(HWND hParent, int alignID, int toID);

//
//  ****************************************************************************
// CondenseStaticTexts
//
// sesune static texty tak, ze budou tesne navazovat - vzdalenost mezi nimi bude
// sire mezery dialog fontu; 'staticsArr' je pole IDcek staticu ukoncene nulou
void CondenseStaticTexts(HWND hWindow, int* staticsArr);

//
//  ****************************************************************************
// ArrangeHorizontalLines
//
// najde horizontalni cary a dorazi je zprava na text, na ktery navazuji
// navic najde checkboxy a radioboxy, ktere tvori labely groupboxum a zkrati
// je podle jejich textu a aktualniho pisma v dialogu (eliminuje zbytecne
// mezery vznikle kvuli ruznym DPI obrazovky)
void ArrangeHorizontalLines(HWND hWindow);

//
//  ****************************************************************************
// GetWindowFontHeight
//
// pro hWindow ziska aktualni font a vrati jeho vysku
int GetWindowFontHeight(HWND hWindow);

//
//  ****************************************************************************
// GetWindowFontHeight
//
// vytvori imagelist obsahujici dva stavy checkboxu (unchecked a checked)
// a vrati jeho handle; 'itemSize' je sirka a vyska jedne polozky v bodech
HIMAGELIST CreateCheckboxImagelist(int itemSize);

//
//  ****************************************************************************
// SalLoadIcon
//
// nacte ikonu urcenou 'hInst' a 'iconName', vrati jeji handle nebo NULL v pripade
// chyby; 'iconSize' udava pozadovanou velikost ikony; funkce je High DPI ready
// a vrati jeho handle; 'itemSize' je sirka a vyska jedne polozky v bodech
//
// Poznamka: stare API LoadIcon() neumi ikony vetsich velikosti, proto zavadime tuto
// funkci, ktera cte ikony pomoci noveho LoadIconWithScaleDown()
HICON SalLoadIcon(HINSTANCE hInst, LPCTSTR iconName, CIconSizeEnum iconSize);
