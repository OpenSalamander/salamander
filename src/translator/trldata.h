// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define PROGRESS_STATE_UNTRANSLATED 0x0000
#define PROGRESS_STATE_TRANSLATED 0x0001

#define COMBOBOX_BASE_HEIGHT 13 // dialog units

// z editwindow vytahne text a prealokuje string
BOOL GetStringFromWindow(HWND hWindow, wchar_t** string);

wchar_t* dupstr(const wchar_t* s);

BOOL IsStyleStaticText(DWORD style, BOOL onlyLeft, BOOL onlyRight);
BOOL IsRadioOrCheckBox(int bt);

void FillControlsWithDummyValues(HWND hDlg);

void RemoveAmpersands(wchar_t* buff);

// orizne white-spaces z obou stran primo v 'str' a vraci 'str'
char* StripWS(char* str);

BOOL DecodeString(const wchar_t* iter, int len, wchar_t** string);
void EncodeString(wchar_t* iter, wchar_t** string);

// vraci TRUE, pokud retezec obsahuje nejake prekladatelne znaky
// (a neobsahuje text "_dummy_" -- tohle jsem zatim vyradil)
BOOL IsTranslatableControl(const wchar_t* text);

COLORREF GetSelectionColor(BOOL clipped);

BOOL __GetNextChar(wchar_t& charValue, wchar_t*& start, wchar_t* end);

#define PUT_WORD(ptr, w) (*(WORD*)(ptr) = (w))
#define GET_WORD(ptr) (*(WORD*)(ptr))
#define PUT_DWORD(ptr, d) (*(DWORD*)(ptr) = (d))
#define GET_DWORD(ptr) (*(DWORD*)(ptr))

enum CSearchDirection
{
    esdRight, // POZOR, zalezi na poradi prvku, vyuziva se v SelectControlsGroup()
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
    CAlignToOperationEnum Operation; // jaka operace se ma provest (move / resize)
    BOOL MoveAsGroup;                // TRUE: zvolene polozky budou posouvany jako skupina, jinak individualne
    CAlignToPartEnum SelPart;        // ke ktere casti oznacenych prvku se operace vztahuje
    CAlignToPartEnum RefPart;        // vztazna cast na referencnim prvku
    int HighlightIndex;              // kopie pro predani do jine instance translatoru
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

    // nacist 16 retezcu
    BOOL LoadStrings(WORD group, const wchar_t* original, const wchar_t* translated, CData* data);

    //    BOOL GetTString(HWND hWindow, int index);
};

//*****************************************************************************
//
// CMenuData
//

struct CMenuItem
{
    wchar_t* OString; // alokovany retezec
    wchar_t* TString; // alokovany retezec
    WORD ID;          // pokud se nejdena o POPUP, command polozky

    WORD Flags; // zacatek/konec popupu nebo polozka

    WORD State;        // translated/untranslated/...
    int Level;         // stupen zanoreni
    int ConflictGroup; // polozky se stejnym ConflictGroup budou posuzovany z hlediska konfliktu horkych klaves; 0 odpovida top-level, 1 prvnimu zanorenemu, atd
};

class CMenuData
{
public:
    WORD ID;
    WORD TLangID;
    TDirectArray<CMenuItem> Items;
    int ConflictGroupsCount; // celkovy pocet unikatnich ConflictGroup

    BOOL IsEX; // MENUEX umime pouze "vyprasene" nacist v MUIMode, v Salamanderu ho nepouzivame, takze ukladani jsem nepsal

public:
    CMenuData();
    ~CMenuData();

    // parsuje resource
    BOOL LoadMenu(LPCSTR original, LPCSTR translated, CData* data);
    BOOL LoadMenuEx(LPCSTR original, LPCSTR translated, CData* data);

    // vrati index polozky s odpovidajicim id nebo -1, pokud neexistuje
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
    emsmSelectDialog,     // odvybere controly, vybere dialog
    emsmSelectControl,    // odvybere dialog, vybere prvek; pokud je hControll NULL, pouze zrusi vyber
    emsmToggleControl,    // odvybere dialog, zmeni vyber pro prvek
    emsmSelectAllControls // odvybere dialog, vybere vsechny controly; hControl musi byt NULL
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

    wchar_t* ClassName;   // alokovany retezec
    wchar_t* OWindowName; // alokovany retezec
    wchar_t* TWindowName; // alokovany retezec

    // DLGITEMTEMPLATEEX support
    DWORD ExHelpID;

    WORD State; // translated/untranslated/...

    BOOL Selected; // TRUE pokud je prvek vybrany v layout editoru, jinak FALSE

public:
    CControl();
    ~CControl();

    void Clean();

    // hluboka kopie
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

    // vraci obdelnik prvku v dlg units; pro combobox vraci zabalenou velikost
    void GetTRect(RECT* r, BOOL adjustZeroSize);

    // vraci vzdalenost od obdelniku 'r' (v dlg units) ve smeru 'direction'
    // pokud prvek nelezi v danem smeru, vraci -1
    // pokud se zkouma smer vpravo, porovnavaji se vzdalenosti pravych stran obdelniku, atd.
    int GetDistanceFromRect(const RECT* r, CSearchDirection direction);
};

// pridani nove transformace:
// 1. doplnit novou polozku do 'CTransformationEnum'
// 2. doplnit do CTransformation::TryMergeWith() - metoda se stara o slouceni stejnych operaci
// 3. doplnit do CTransformation::SetTransformationWithNoParam() nebo SetTransformationWithIntParam() nebo SetTransformationWith2IntParams() - dle poctu parametru
// 4. doplnit do CDialogData::ExecuteTransformation() - stara se o provedeni transformace
enum CTransformationEnum
{
    eloInit,
    eloSelect,              // [] zmena Selection
    eloMove,                // [deltaX, deltaY] posun controlu o 'deltaX' a 'deltaY'
    eloResize,              // [edge, delta] zmena rozmeru controlu; 'edge' urcuje kterou hranou hybeme, 'delta' o kolik dlg units
    eloResizeDlg,           // [horizontal, delta] zmena rozmeru dialogu; pokud je 'horizontal' TRUE, meni se sirka, jinak vyska dialogu o 'delta' dlg units
    eloResizeControls,      // [resize] prikaz Resize upravi sirku nebo vysku prvku dle 'resize'
    eloSizeToContent,       // [] prikaz Size to Content
    eloSetSize,             // [width, height] zmena rozmeru controlu; 'width' a 'height' urcuji rozmery, pokud je hodnota -1, dany rozmer se nemeni
    eloAlignTo,             // [] prikaz Align To
    eloAlignControls,       // [edge] prikaz Align zarovna prvky na hranu 'edge'
    eloCenterControls,      // [horizontal] prikaz Center centruje prvky navzajem vodorovne nebo svisle dle 'horizontal'
    eloCenterControlsToDlg, // [horizontal] prikaz Center Controls To Dialog centruje prvky vuci dialogu vodorovne nebo svisle dle 'horizontal'
    eloEqualSpacing,        // [horizontal, delta] prikaz Equal Spacing usporada prvky vodorovne nebo svisle dle 'horizontal' tak, aby byly oddeleny konstantni vzdalenosti 'delta'
    eloNormalizeLayout,     // [wide] normalizuje layout standard dialogu; pokud je 'wide' TRUE, normalizuje na sirokouhly dialog, jinak na standardni
    eloTemplateReset,       // [] vraceni se k originalnimu template
    eloPreviewLayout,       // [] preview jine verze layoutu
};

class CTransformation
{
public:
    CTransformationEnum Transformation; // urcuje tranformaci
    void* Params;                       // alokovane parametry transformace (lisi se dle 'Transformation')
    int ParamsSize;                     // pocet alokovanych bajtu pro 'Params'

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

    wchar_t* MenuName;  // alokovany retezec
    wchar_t* ClassName; // alokovany retezec

    WORD FontSize;
    wchar_t* FontName; // alokovany retezec

    // DLGTEMPLATEEX support
    BOOL IsEX;
    WORD ExDlgVer;
    WORD ExSignature;
    DWORD ExHelpID;
    WORD ExWeight;
    BYTE ExItalic;
    BYTE ExCharset;

    // polozka na indexu 0 obsahuje nazev dialogu a Selected stav pro dialog
    // polozky od indexu 1 jsou jiz vlastni controly dialogu
    TIndirectArray<CControl> Controls;

    // index referencniho prvku pro prikaz AlignTo
    int HighlightIndex; // -1: nic, 0: dlg, >0: controly

    // transformace provedena nad dialogem (a controly), aby bylo dosazeno aktualniho stavu
    // slouzi k predani provedenych uprav do dalsi instance Translatoru
    CTransformation Transformation;

public:
    CDialogData();
    ~CDialogData();

    void Clean();

    // parsuje resource
    BOOL LoadDialog(WORD* oBuff, WORD* tBuff, BOOL* showStyleWarning,
                    BOOL* showExStyleWarning, BOOL* showDlgEXWarning,
                    CData* data);

    // vytvori v bufferu sablonu pro Save nebo pro preview
    // pokud je addProgress == TRUE, ukladaji se stavy prelozeni
    // vraci puzitou velikost v bajtech
    // pro preview nastavit 'forPreview' na TRUE, aby se custom classy nahradily za neco registrovaneho
    // pokud ma natahnout dialog pres vsechny controly, nastavit 'extendDailog' na TRUE
    DWORD PrepareTemplate(WORD* buff, BOOL addProgress, BOOL forPreview, BOOL extendDailog);

    // v bufferu sablony prida a odebira styly nastavene na logickou 1
    void TemplateAddRemoveStyles(WORD* buff, DWORD addStyles, DWORD removeStyles);
    // v bufferu sablony nastavi souradnice
    void TemplateSetPos(WORD* buff, short x, short y);

    // vrati index okna s odpovidajicim id nebo -1, pokud neexistuje
    int FindControlIndex(WORD id, BOOL isDlgTitle);

    // vraci index controlu Controls[controlIndex] v listview s texty pro dialog
    // pokud v tom listview control neni, vraci FALSE
    BOOL GetControlIndexInLV(int controlIndex, int* indexLV);

    // hluboka kopie
    void LoadFrom(CDialogData* src, BOOL keepLangID = FALSE);

    // nacte selection
    void LoadSelectionFrom(CDialogData* src);

    // vtati TRUE, pokud se layout zmenil proti originalu; urceno pro layout editor, kde muze dojit skutecne pouze k zmene layoutu
    BOOL DoesLayoutChanged(CDialogData* orgDialogData);
    BOOL DoesLayoutChanged2(CDialogData* orgDialogData); // robustnejsi verze slouzici pro import starych prekladu

    // vrati index sousedniho prvku (vuci prvku 'fromIndex') ve smeru 'dir' nebo -1 pokud zadny prvek nenalezne
    // za sousedni prvek se povazuje nejblizsi prvek zasahujici do prumetu prvku 'fromIndex' danym smerem
    // pokud je 'wideScan' TRUE, bude hledaci pruh rozsiren pres celou sirku/vysku dialogu
    int GetNearestControlInDirection(int fromIndex, CSearchDirection direction, BOOL wideScan);

    // vraci TRUE, pokud prvek 'testIndex' patri do stejne skupiny jako prvej 'fromIndex'; jinak vraci FALSE
    // stejnou skupinou se mysli vyber pomoc doubleclicku, napriklad tlacitek v rade
    BOOL BelongsToSameSelectionGroup(int fromIndex, int testIndex);

    // vraci TRUE, pokud oba prvky maji stejny horni nebo levy okraj (dle 'direction')
    // jinak vraci FALSE
    BOOL ControlsHasSameGroupEdge(int fromIndex, int testIndex, CSearchDirection direction);

    // vrati TRUE pokud dialog obsahuje control (nebo vice) za hranici dialogu
    BOOL HasOuterControls();

    // vrati rozmery obdelniku opsaneho kolem prvku dialogu
    // ignoruje vodorovne separatory a prvky lezici za hranici dialogu
    // vraci TRUE v pripade uspechu, jinek FALSE
    BOOL GetControlsRect(BOOL ignoreSeparators, BOOL ignoreOuterControls, RECT* r);

    // vraci TRUE, pokud lze layout normalizovat, jinak vraci FALSE
    // normalizace neni podporovana, pokud existuji vnejsi prvky lezici mimo pravou
    // stranu dialogu - takove dialogy jsem u nas nenasel, takze nebyla potreba
    // tuto situaci resit (i kdyz by to urcite slo vymyslet)
    BOOL CanNormalizeLayout();

    // ucese layout do standardnich rozmeru
    void AdjustSeparatorsWidth(CStringList* errors);
    void NormalizeLayout(BOOL wide, CStringList* errors);

    // naplni pole 'footer' indexem separatoru (pokud existuje, bude prvni polozkou pole) a indexama
    // tlacitek umistenych na spodni hrande dialogu (casto jde o OK/Cancel/Yes/No/Skip/Overwrite/Help/...)
    // na zacatku vycisti pole 'footer'
    // vraci TRUE v pripade, ze stejnomerne rozmistena tlacitka (a pripadne separator) byla nalezena a vlozena do pole
    // pripadne pokud zadna tlacitka nalezena nebyla; vraci FALSE pokud tlacitka nalezena byla, ale nejsou stejnomerne
    // rozmistena - bude zobrazeno varovnai
    BOOL GetFooterSeparatorAndButtons(TDirectArray<DWORD>* footer);

    // funkce prepoklada, ze indexy v poli 'footer' jsou serazeny podle x-ovych pozic prvku zleva doprava
    void AdjustFooter(BOOL wide, TDirectArray<DWORD>* footer);

    // prenese rozmery dialogu a controlu z Original do Translated
    void LoadOriginalLayout();

    // selection related methods
    // TODO: nasledujici skupina metod patrila puvodni objektu CSelectedControls pro spravu vybranych polozek
    // postupne se ukazalo, ze jde o spatnou koncepci, protoze vyber by napriklad mel byt soucasti undo, takze
    // jsme metody prenesli sem
    // bylo by dobre je lepe/vice logicky pojmenovat misto prefixu SelCtrls

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
    // vrati opsany obdelnik kolem vybranych polozek
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
    wchar_t HelpDir[100];       // nazev adresare s help soubory (slovensky preklad Salamandera muze pouzivat ceske helpy)
    BOOL HelpDirExist;          // je v SLG obsazena polozka HelpDir? (jen u Salamanderiho .slg)
    wchar_t SLGIncomplete[200]; // prazdne = preklad Salama i pluginu je kompletni, jinak link, kam smerovat uzivatele pri prvnim spusteni Salama v teto jazykove verzi (rekrutujeme nove prekladatele)
    BOOL SLGIncompleteExist;    // je v SLG obsazena polozka SLGIncomplete? (jen u Salamanderiho .slg)

    // CRC SLT souboru, ze ktereho se naposledy importilo nebo do ktereho se naposledy exportilo
    // z tohoto SLG souboru, hodnoty:
    // "not found" - promenna v SLG vubec neni,
    // "none" - EN verze,
    // "" - zmenena verze, ktera nebyla importena/exportena do SLT,
    // "00000000 SLT" - tesne po exportu do SLT nebo importu z SLT,
    // "00000000" - zmenena verze, ktera byla importena/exportena do SLT
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
        if (wcscmp(CRCofImpSLT, L"none") == 0) // z "EN verze" udelame "zmenenou verzi, ktera nebyla importena/exportena do SLT"
            lstrcpyW(CRCofImpSLT, L"");
        else
        {
            int len = wcslen(CRCofImpSLT);
            if (len > 4 && wcscmp(CRCofImpSLT + len - 4, L" SLT") == 0) // z "tesne po exportu do SLT nebo importu z SLT" udelame "zmenenou verzi, ktera byla importena/exportena do SLT"
                CRCofImpSLT[len - 4] = 0;
        }
    }

    BOOL IsSLTDataAfterImportOrExport()
    {
        int len = wcslen(CRCofImpSLT);
        return len > 4 && wcscmp(CRCofImpSLT + len - 4, L" SLT") == 0; // je "tesne po exportu do SLT nebo importu z SLT"?
    }

    void SetCRCofImpSLTIfFound(wchar_t* crc)
    {
        if (wcscmp(CRCofImpSLT, L"not found") != 0) // pokud promenna v SLG je, nastavime ji novou hodnotu
            lstrcpynW(CRCofImpSLT, crc, _countof(CRCofImpSLT));
    }

    BOOL IsSLTDataChanged()
    {
        if (wcscmp(CRCofImpSLT, L"") == 0) // zmenena verze, ktera nebyla importena/exportena do SLT
            return TRUE;
        wchar_t* s = CRCofImpSLT;
        while (*s >= L'0' && *s <= L'9' || *s >= L'a' && *s <= L'f' || *s >= L'A' && *s <= L'F')
            s++;
        if (*s == 0 && (s - CRCofImpSLT) == 8) // CRC bez koncovky " SLT": zmenena verze, ktera byla importena/exportena do SLT
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
    cltPropPgSameSize,       // kontrola, ze jsou vsechny property pages stejne velike
    cltMultiTextControl,     // kontrola, jestli se do daneho controlu v dialogu vejdou vsechny uvedene texty (text primo v dialogu se kontroluje take)
    cltMultiTextControlBold, // cltMultiTextControl, ale omeruje se BOLD fontem
    cltDropDownButton,       // kontrola, jestli je tlacitko dost velke, aby se vesel symbol drop-down menu (sipka dolu vpravo na tlacitku)
    cltMoreButton,           // kontrola, jestli je tlacitko dost velke, aby se vesel symbol more menu (dve sipky dolu vpravo na tlacitku)
    cltComboBox,             // kontrola, jestli je combo box dost velky pro vsechny uvedene texty
    cltAboutDialogTitle,     // specialitka pro About dialogy
    cltProgressDialogStatus, // specialitka pro Progress dialog
    cltStringsWithCSV,       // kontrola, ze sedi pocet carek v originalnim a prelozenem textu
};

struct CCheckLstItem
{
    CCheckLstItemType Type;
    WORD DialogID;             // cltPropPgSameSize+cltStringsWithCSV nepouziva
    WORD ControlID;            // cltPropPgSameSize+cltStringsWithCSV nepouziva
    TDirectArray<WORD> IDList; // cltDropDownButton+cltMoreButton+cltAboutDialogTitle+cltProgressDialogStatus nepouzivaji

    CCheckLstItem() : IDList(5, 5)
    {
        Type = cltPropPgSameSize;
        DialogID = ControlID = 0;
    }

    BOOL ContainsDlgID(WORD dlgID);
};

enum CIgnoreLstItemType
{
    iltOverlap,            // kolize dvou controlu v dialogu
    iltClip,               // text controlu v dialogu je oriznuty, control je pro nej kratky
    iltSmIcon,             // velikost ikony: ikona je mala, tedy pouzit velikost 10x10 dlg-units, misto std. 20x20
    iltHotkeysInDlg,       // kolize dvou hotkeys controlu v dialogu
    iltTooClose,           // prilis blizko okraji dialogu
    iltMisaligned,         // dva controly v dialogu nejsou zarovnane
    iltDiffSized,          // dva controly v dialogu se mirne lisi velikosti
    iltDiffSpacing,        // control v dialogu je nepravidelne umisteny mezi ostatnimi controly
    iltNotStdSize,         // control v dialogu je nestandardne veliky
    iltIncorPlLbl,         // nespravne zarovnany label a jeho control
    iltMisColAtEnd,        // chybejici ':' na konci stringu
    iltInconTxtEnds,       // control v dialogu jehoz zakonceni textu se nema kontrolovat
    iltInconStrEnds,       // zakonceni stringu se nema kontrolovat
    iltInconStrBegs,       // zacatek stringu se nema kontrolovat
    iltInconCtrlChars,     // nekonzistentni control-chars (\r, \n, \t)
    iltInconHotKeys,       // nekonzistentni hotkeys ('&')
    iltProgressBar,        // jde o progress bar (nejde o static text)
    iltInconFmtSpecif,     // nekonzistentni format-specifiers (%d, %f, atd.)
    iltInconFmtSpecifCtrl, // control v dialogu muze mit nekonzistentni format-specifiers (%d, %f, atd.)
};

struct CIgnoreLstItem
{
    CIgnoreLstItemType Type;
    WORD DialogID;
    WORD ControlID1; // pouziva vse krome: iltMisColAtEnd, iltInconStrEnds, iltInconStrBegs, iltInconCtrlChars, iltInconHotKeys,iltInconFmtSpecif
    WORD ControlID2; // pouziva jen Type iltOverlap, iltHotkeysInDlg, iltMisaligned, iltDiffSized a iltIncorPlLbl

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
    char* TemplateName; // alokovany nazev sablony

public:
    WORD SectionDialogID;         // dialog, se kterym je sekce svazana (0 pokud neni)
    WORD SectionControlDialogID;  // control, se kterym je sekce svazana: ID dialogu s controlem (0 pokud neni)
    WORD SectionControlControlID; // control, se kterym je sekce svazana: ID controlu (0 pokud neni)
    TDirectArray<WORD> StringsID; // ID stringu sekce

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
    pseDummy, // sekci ignorujeme
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
    TDirectArray<DWORD> Lines; // pro ktere radky output okna se ma seznam zobrazit
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

    TDirectArray<CBookmark> Bookmarks; // ID dialugu, ktere je treba znovu layoutovat (po importu stare konfigurace)

public:
    TIndirectArray<CStrData> StrData;
    TIndirectArray<CMenuData> MenuData;
    TIndirectArray<CDialogData> DlgData;
    TIndirectArray<CSalMenuSection> SalMenuSections;
    TIndirectArray<CIgnoreLstItem> IgnoreLstItems;
    TIndirectArray<CCheckLstItem> CheckLstItems;

    TDirectArray<WORD> Relayout; // ID dialugu, ktere je treba znovu layoutovat (po importu stare konfigurace)

    TIndirectArray<CMenuPreview> MenuPreview; // slouzi k reseni kolizi v menu

    // promenne pro enumaraci
    BOOL EnumReturn;
    HINSTANCE HTranModule;
    BOOL Importing;
    BOOL ShowStyleWarning;
    BOOL ShowExStyleWarning;
    BOOL ShowDlgEXWarning;
    BOOL MUIMode;      // importujeme (nebo to jiz probehlo) MUI baliky pro lokalizaci W7
    DWORD MUIDialogID; // unikatni ID, maji vyznam pouze pokud je 'MUIMode' TRUE
    DWORD MUIMenuID;
    DWORD MUIStringID;

    CSLGSignature SLGSignature;

    CVersionInfo VersionInfo;

    // cesta k souborum
    char ProjectFile[MAX_PATH];
    char SourceFile[MAX_PATH];
    char TargetFile[MAX_PATH];
    char IncludeFile[MAX_PATH]; // vsechna RH sloucena do jednoho souboru
    char SalMenuFile[MAX_PATH];
    char IgnoreLstFile[MAX_PATH];
    char CheckLstFile[MAX_PATH];
    char SalamanderExeFile[MAX_PATH];
    char ExportFile[MAX_PATH]; // export as text: pro spellchecker
    char ExportAsTextArchiveFile[MAX_PATH];
    char FullSourceFile[MAX_PATH];
    char FullTargetFile[MAX_PATH];
    char FullIncludeFile[MAX_PATH]; // vsechna RH sloucena do jednoho souboru
    char FullSalMenuFile[MAX_PATH];
    char FullIgnoreLstFile[MAX_PATH];
    char FullCheckLstFile[MAX_PATH];
    char FullSalamanderExeFile[MAX_PATH];
    char FullExportFile[MAX_PATH];
    char FullExportAsTextArchiveFile[MAX_PATH];

    // stav stromu
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

    // nacte salmenu.mnu, ktere Petr generuje perl skriptem ze zdrojaku
    // soubor obsahuje struktury nasich internich menu definovanych pomoci MENU_TEMPLATE_ITEM
    BOOL LoadSalMenu(const char* fileName);
    BOOL ProcessSalMenuLine(const char* line, const char* lineEnd, int row, CSalMenuDataParserState* parserState);

    // nacte ignore.lst: obsahuje prekryvy a orezy, ktere se maji ignorovat
    BOOL LoadIgnoreLst(const char* fileName);
    BOOL ProcessIgnoreLstLine(const char* line, const char* lineEnd, int row);

    // nacte check.lst: obsahuje veci, ktere se maji zkontrolovat (napr. shoda velikosti property-pages)
    BOOL LoadCheckLst(const char* fileName);
    BOOL ProcessCheckLstLine(const char* line, const char* lineEnd, int row);

    // natahne starou verzi prekladu a aplikuje ji na soucasna data
    // pokud je 'trlPropOnly' TRUE, nactou se pouze translation propoerties
    BOOL Import(const char* project, BOOL trlPropOnly, BOOL onlyDlgLayouts);

    // naleje prelozenou cast dat do textoveho souboru,
    // aby bylo mozne provest kontrolu gramatiky
    BOOL Export(const char* fileName);

    // export prelozenych textu do unicode souboru, ktery jsme schopny drzet/mergovat na CVSku
    // nasledne ho muzeme zase importovat zpet
    BOOL ExportAsTextArchive(const char* fileName, BOOL withoutVerInfo);
    BOOL ImportTextArchive(const char* fileName, BOOL testOnly); // pokud je 'testOnly' TRUE, pouze se provede kontrola zda data v souboru lze 1:1 premapovat na nase data

    // export velikosti dialogu a controlu, pouziva se pro zmenu .rc modulu obsahujicich
    // zdrojaky resourcu pro anglickou jazykovou verzi, ktera se z .rc kompiluje
    BOOL ExportDialogsAndControlsSizes(const char* fileName);

    // nasosne MS MUI jazykova balicky pro Win7 (CABy)
    BOOL LoadMUIPackages(const char* originalMUI, const char* translatedMUI);

    // zkontroluje data; pokud je bude mozne ulozit, vrati TRUE; jinak vrati FALSE
    //    BOOL Validate();

    // naplni strom pro volbu editovaneho resourcu
    void FillTree();

    void UpdateNode(HTREEITEM hItem);
    void UpdateSelectedNode();
    void UpdateAllNodes(); // nastavime translated stavy na treeview
    void UpdateTexts();    // update list view v okne Texts

    // naplni okno texts
    void FillTexts(DWORD lParam);

    void SetDirty(BOOL dirty = TRUE);
    BOOL IsDirty() { return Dirty; }

    // vrati stav pro dane ID
    TDirectArray<CProgressItem>* GetTranslationStates(CTranslationTypeEnum type);
    void CleanTranslationStates();
    BOOL AddTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, WORD state);
    WORD QueryTranslationState(CTranslationTypeEnum type, WORD id1, WORD id2, const wchar_t* oStr, const wchar_t* tStr);
    BOOL QueryTranslationStateIndex(CTranslationTypeEnum type, WORD id1, WORD id2, int* index);

    void CleanRelayout();
    void AddRelayout(WORD id);
    BOOL RemoveRelayout(WORD id);

    // prohleda data na zaklade kriterii v Config
    void Find();

    // hleda neprelozene retezce
    void FindUntranslated(BOOL* completelyUntranslated = NULL);

    // hleda dialogy, ktere je potreba po importu puvodniho prekladu znovu layoutovat
    void FindDialogsForRelayout();

    // hleda dialogy obsahujici controly lezici za hranici dialogu
    void FindDialogsWithOuterControls();

    // hleda konflikty horkych klaves; vysledek laduje do okna Output
    void ValidateTranslation(HWND hParent);

    // hleda kolize IDecek v dialogu, vola se po nacteni projektu
    // vysledky naleje do output window
    void LookForIdConflicts();

    // vsechny zmenene texty oznaci jako Translated
    void MarkChangedTextsAsTranslated();

    // vsechny dialogy nastavi na layout originalu
    void ResetDialogsLayoutsToOriginal();

    int FindStrData(WORD id);
    int FindMenuData(WORD id);
    int FindDialogData(WORD id);

    BOOL FindStringWithID(WORD id, int* index = NULL, int* subIndex = NULL, int* lvIndex = NULL);
    BOOL GetStringWithID(WORD id, wchar_t* buf, int bufSize);

    // najde v CheckLstItems polozky pro zadany dialogID a controlID (anyControl==TRUE = libovolny control z dialogu)
    BOOL GetItemFromCheckLst(WORD dialogID, WORD controlID, CCheckLstItem** multiTextOrComboItem,
                             CCheckLstItem** checkLstItem, BOOL anyControl = FALSE);

    // najde v CheckLstItems polozky cltStringsWithCSV a overi, jestli 'strID' neni v jejich seznamu IDcek
    BOOL IsStringWithCSV(WORD strID);

    // hleda problem v poli IgnoreLstItems, vraci TRUE pokud se ma problem ignorovat
    // (byl nalezen)
    BOOL IgnoreProblem(CIgnoreLstItemType type, WORD dialogID, WORD controlID1, WORD controlID2);

    // zjisti, jestli se ma ignorovat velikost ikony a ma se pouzit rozmer male ikony (16x16)
    BOOL IgnoreIconSizeIconIsSmall(WORD dialogID, WORD controlID);

    // zjisti, jestli se ma ignorovat konflikt hotkeys mezi controly 'controlID1' a 'controlID2'
    // v dialogu 'dialogID'
    BOOL IgnoreDlgHotkeysConflict(WORD dialogID, WORD controlID1, WORD controlID2);

    // zjisti, jestli ma ignorovat, ze je control prilis blizko okraji dialogu
    BOOL IgnoreTooCloseToDlgFrame(WORD dialogID, WORD controlID);

    // zjisti, jestli jde o progress bar (je potreba ignorovat, ze jde o static)
    BOOL IgnoreStaticItIsProgressBar(WORD dialogID, WORD controlID);

    // zjisti, jestli ma ignorovat, ze je control nepravidelne umisten mezi ostatnimi controly v dialogu
    BOOL IgnoreDifferentSpacing(WORD dialogID, WORD controlID);

    // zjisti, jestli ma ignorovat, ze je control nestandardne veliky
    BOOL IgnoreNotStdSize(WORD dialogID, WORD controlID);

    // zjisti, jestli ma ignorovat, ze stringu chybi dvojtecka na konci
    BOOL IgnoreMissingColonAtEnd(WORD stringID);

    // zjisti, jestli se nema kontrolovat zakonceni textu controlu v dialogu
    // (napr. viceradkovy text rozdeleny do jednotlivych staticu po radcich)
    BOOL IgnoreInconTxtEnds(WORD dialogID, WORD controlID);

    // zjisti, jestli se u stringu ma kontrolovat zakonceni
    BOOL IgnoreInconStringEnds(WORD stringID);

    // zjisti, jestli se u stringu ma kontrolovat zacatek
    BOOL IgnoreInconStringBegs(WORD stringID);

    // zjisti, jestli se u stringu maji kontrolovat control-chars (\r, \n, \t)
    BOOL IgnoreInconCtrlChars(WORD stringID);

    // zjisti, jestli se u stringu maji kontrolovat hotkeys ('&')
    BOOL IgnoreInconHotKeys(WORD stringID);

    // zjisti, jestli se u stringu maji kontrolovat format-specifiers (%d, %f, atd.)
    BOOL IgnoreInconFmtSpecif(WORD stringID);

    // zjisti, jestli ma ignorovat format-specifiers (%d, %f, atd.) u controlu
    BOOL IgnoreInconFmtSpecifCtrl(WORD dialogID, WORD controlID);

    void ToggleBookmark(DWORD treeItem, WORD textItem);
    const CBookmark* GotoBookmark(BOOL next, DWORD treeItem, WORD textItem);
    void ClearBookmarks();
    int GetBookmarksCount();
    BOOL FindBookmark(DWORD treeItem, WORD textItem);

    // vraci TRUE, pokud ma nacteny projekt jako jazyk English
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

    // nacitani / ukladani projektu
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

// p je bod v screen souradnich, pro ktery je vracen index child window
// pokud neni nalezeno zadne okno, vraci se NULL
HWND GetChildWindow(POINT p, HWND hDialog, int* index = NULL);
