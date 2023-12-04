// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_gui) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

////////////////////////////////////////////////////////
//                                                    //
// Prostor WM_APP + 200 az WM_APP + 399 je v const.h  //
// vyjmut z prostoru pouzivaneho pro interni zpravy   //
// Salamandera.                                       //
//                                                    //
////////////////////////////////////////////////////////

// menu messages
#define WM_USER_ENTERMENULOOP WM_APP + 200   // [0, 0] doslo ke vstupu do menu
#define WM_USER_LEAVEMENULOOP WM_APP + 201   // [0, 0] byl ukoncen rezim menu loop; tato message je sendnuta pred postnutim commandu
#define WM_USER_LEAVEMENULOOP2 WM_APP + 202  // [0, 0] byl ukoncen rezim menu loop; tato message je postnuta az po commandu
#define WM_USER_INITMENUPOPUP WM_APP + 204   // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_UNINITMENUPOPUP WM_APP + 205 // [(CGUIMenuPopupAbstract*)menuPopup, LOWORD(uPos), HIWORD(uID)]
#define WM_USER_CONTEXTMENU WM_APP + 206     // [(CGUIMenuPopupAbstract*)menuPopup, (BOOL)fromMouse \
                                             //   (pokud dojde mysi akci, je rovno TRUE (pouzit GetMessagePos); \
                                             //    pokud jde o klavesnicovou akci VK_APPS nebo Shift+F10, je rovno FALSE)] \
                                             // p.s. pokud vrati TRUE, dojde ke spusteni prikazu menu nebo otevreni submenu \
                                             // Pokud chceme v Salamu pretypovat menuPopup na CMenuPopup, \
                                             // pouzijeme (CMenuPopup*)(CGUIMenuPopupAbstract*)menuPopup.

// toolbar messages
#define WM_USER_TBDROPDOWN WM_APP + 220    // [HWND hToolBar, int buttonIndex]
#define WM_USER_TBRESET WM_APP + 222       // [HWND hToolBar, TOOLBAR_TOOLTIP *tt]
#define WM_USER_TBBEGINADJUST WM_APP + 223 // [HWND hToolBar, 0]
#define WM_USER_TBENDADJUST WM_APP + 224   // [HWND hToolBar, 0]
#define WM_USER_TBGETTOOLTIP WM_APP + 225  // [HWND hToolBar, 0]
#define WM_USER_TBCHANGED WM_APP + 226     // [HWND hToolBar, 0]
#define WM_USER_TBENUMBUTTON2 WM_APP + 227 // [HWND hToolBar, TLBI_ITEM_INFO2 *tii]

// tooltip messages
#define TOOLTIP_TEXT_MAX 5000          // maximalni delka retezce tool tipu (zprava WM_USER_TTGETTEXT)
#define WM_USER_TTGETTEXT WM_APP + 240 // [ID predany v SetCurrentToolTip, buffer omezeny TOOLTIP_TEXT_MAX]

// button pressed
#define WM_USER_BUTTON WM_APP + 244 // [(LO)WORD buttonID, (LO)WORD udalost byla vyvolana z klavesnice, pokud otevirame menu, vybrat prvni polozku]
// drop down of button pressed
#define WM_USER_BUTTONDROPDOWN WM_APP + 245 // [(LO)WORD buttonID, (LO)WORD udalost byla vyvolana z klavesnice, pokud otevirame menu, vybrat prvni polozku]

#define WM_USER_KEYDOWN WM_APP + 246 // [(LO)WORD ctrlID, DWORD virtual-key code]

//
// ****************************************************************************
// CGUIProgressBarAbstract
//

class CGUIProgressBarAbstract
{
public:
    // nastavuje progres, pripadne text uprostred
    //
    // existuje bezpecnejsi varianta SetProgress2(), podivejte se na ni nez pouzijete tuto metodu
    //
    // progres dokaze pracovat ve dvou rezimech:
    //   1) pro 'progress' >= 0 jde o klasicky teplomer 0% az 100%
    //      v tomto rezimu lze pomoci promenne 'text' nastavit vlastni text zobrazeny uprostred
    //      pokud je 'text' == NULL, zobrazi se uprostred standardni procenta
    //   2) pro 'progress' == -1 jde o neurcity stav, kdy maly obdelnicek jezdi tam a zpet
    //      pohyb se ridi pomoci metod SetSelfMoveTime(), SetSelfMoveSpeed() a Stop()
    //
    // prekresleni se provadi okamzite; u vetsiny operaci je vhodne data ukladat v parent
    // dialogu do cache a spustit si 100ms timer, na ktery teprve volat tuto metodu
    //
    // mozne volat z libovolneho threadu, thread s controlem musi bezet, jinak dojde k zablokovani
    // (pro doruceni hodnoty 'progress' controlu se pouziva SendMessage);
    virtual void WINAPI SetProgress(DWORD progress, const char* text) = 0;

    // ma vyznam v kombinaci s volanim SetProgress(-1)
    // urcuje kolik milisekund po zavolani SetProgress(-1) se jeste bude obdelnicek sam pohybovat
    // pokud v tuto dobu dojde k zavolani dalsiho SetProgress(-1), cas se pocita zase od zacatku
    // pokud je 'time'==0, posune se obdelnicek pouze jednou prave pri zavolani SetProgress(-1)
    // pro hodnotu 'time'==0xFFFFFFFF se bude obdelnicek posouvat do nekonecna (implicitni hodnota)
    virtual void WINAPI SetSelfMoveTime(DWORD time) = 0;

    // ma vyznam v kombinaci s volanim SetProgress(-1)
    // urcuje cas mezi posunutim obdelnicku v milisekundach
    // implicitni hodnota je 'moveTime'==50, coz znamena 20 pohybu za vterinu
    virtual void WINAPI SetSelfMoveSpeed(DWORD moveTime) = 0;

    // ma vyznam v kombinaci s volanim SetProgress(-1)
    // pokud se obdelnicek prave pohybuje (diky SetSelfMoveTime), bude zastaven
    virtual void WINAPI Stop() = 0;

    // nastavuje progres, pripadne text uprostred
    //
    // proti SetProgress() ma vyhodu v tom, ze pokud je 'progressCurrent' >= 'progressTotal',
    // nastavi progres primo: je-li 'progressTotal' 0 nastavi 0%, jinak 100% a neprovadi vypocet
    // (je nesmyslny + rve na nem RTC kvuli pretypovani), tento "nepovoleny" stav nastava
    // napr. pri zvetseni souboru behem operace nebo pri praci s linky na soubor - linky maji
    // nulovou velikost, ale pak jsou na nich data o velikosti nalinkovaneho souboru,
    // pokud si vypocet provedete sami, je nutne tento "nepovoleny" stav osetrit
    //
    // progres dokaze pracovat ve dvou rezimech (viz SetProgress()), touto metodou lze
    // nastavovat jen v rezimu 1):
    //   1) jde o klasicky teplomer 0% az 100%
    //      v tomto rezimu lze pomoci promenne 'text' nastavit vlastni text zobrazeny uprostred
    //      pokud je 'text' == NULL, zobrazi se uprostred standardni procenta
    //
    // prekresleni se provadi okamzite; u vetsiny operaci je vhodne data ukladat v parent
    // dialogu do cache a spustit si 100ms timer, na ktery teprve volat tuto metodu
    //
    // mozne volat z libovolneho threadu, thread s controlem musi bezet, jinak dojde k zablokovani
    // (pro doruceni hodnoty 'progress' controlu se pouziva SendMessage);
    virtual void WINAPI SetProgress2(const CQuadWord& progressCurrent, const CQuadWord& progressTotal,
                                     const char* text) = 0;

    // priklady pouziti:
    //
    // 1. obdelnicek chceme posouvat rucne, bez naseho prispeni se nepohybuje
    //
    //   SetSelfMoveTime(0)           // zakazeme samovolny pohyb
    //   SetProgress(-1, NULL)        // posuneme o jeden dilek
    //   ...
    //   SetProgress(-1, NULL)        // posuneme o jeden dilek
    //
    // 2. obdelnicek se ma pohybovat samostatne a do az do zavolani Stop
    //
    //   SetSelfMoveTime(0xFFFFFFFF)  // nekonecny pohyb
    //   SetSelfMoveSpeed(50)         // 20 pohybu za vterinu
    //   SetProgress(-1, NULL)        // nastartujeme obdelnicek
    //   ...                          // neco kutime
    //   Stop()                       // zastavime obdelnicek
    //
    // 3. obdelnicek se ma pohybovat omezenou dobu, po ktere se zastavi
    //   pokud do nej behem teto doby "drcneme", doba se obnovi
    //
    //   SetSelfMoveTime(1000)        // samovolne se pohybuje vterinu, pak se zastavi
    //   SetSelfMoveSpeed(50)         // 20 pohybu za vterinu
    //   SetProgress(-1, NULL)        // nastartujeme obdelnicek na dobu jedne vteriny
    //   ...
    //   SetProgress(-1, NULL)        // ozivime obdelnicek na dalsi vterinu
    //
    // 4. behem operace doslo k pausnuti a chceme to vizualizovat v progress bar
    //
    //   SetProgress(0, NULL)         // 0%
    //   SetProgress(100, NULL)       // 10%
    //   SetProgress(200, NULL)       // 20%
    //   SetProgress(300, "(paused)") // 30% -- misto "30 %" se zobrazi text "(paused)"
    //   ... (cekame na resume)
    //   SetProgress(300, NULL)       // 30% (text paused) zase vypneme a jedeme dal
    //   SetProgress(400, NULL)       // 40%
    //   ...
};

//
// ****************************************************************************
// CGUIStaticTextAbstract
//

#define STF_CACHED_PAINT 0x0000000001    // zobrazeni textu pojede pres cache (nebude blikat) \
                                         // POZOR: zobrazeni je radove pomalejsi nez bez tohoto flagu. \
                                         // Nepouzivat v pripade textu v dialogu, ktere se zobrazi \
                                         // jednou a pak zustavaji nemenne. \
                                         // Pouzivat u casto/rychle se menicich textu (provadena operace).
#define STF_BOLD 0x0000000002            // pro text bude pouzit tucny font
#define STF_UNDERLINE 0x0000000004       // pro text bude pouzit font s podtrzenim (pro spatnou citelnost \
                                         // pouzivat pouze pro HyperLink a specialni pripady)
#define STF_DOTUNDERLINE 0x0000000008    // text bude carkovane podtrzeny (pro spatnou citelnost \
                                         // pouzivat pouze pro HyperLink a specialni pripady)
#define STF_HYPERLINK_COLOR 0x0000000010 // barva textu bude urcena podle barvy hyperlinku
#define STF_END_ELLIPSIS 0x0000000020    // pokud bude text prilis dlouhy, bude ukoncen vypustkou "..."
#define STF_PATH_ELLIPSIS 0x0000000040   // pokud bude text prilis dlouhy, bude zkracen a bude do nej vlozena \
                                         // vypustka "..." tak, aby byl konec viditelny
#define STF_HANDLEPREFIX 0x0000000080    // znaky za '&' budou podtrzene; nelze pouzit s STF_END_ELLIPSIS nebo s STF_PATH_ELLIPSIS

class CGUIStaticTextAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
    //
    // Control lze v dialogu navstivit z klavesnice, pokud mu priradime styl WS_TABSTOP.
public:
    // nastavi text controlu; volani teto metody je rychlejsi a mene vypocetne narocne
    // nez nastavovani textu pomoci WM_SETTEXT; vraci TRUE v pripade uspechu, jinak FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // vrati text controlu; mozne volat z libovolneho threadu;
    // vrati NULL, pokud jeste nebylo volano SetText a static control byl bez textu
    virtual const char* WINAPI GetText() = 0;

    // nastavi znak pro oddeleni casti cesty; ma vyznam v pripade STF_PATH_ELLIPSIS;
    // implicitne je nastaveno na '\\';
    virtual void WINAPI SetPathSeparator(char separator) = 0;

    // priradi text, ktery bude zobrazen jako tooltip
    // vraci TRUE, pokud se podarilo naalokovat kopii textu, jinak vraci FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // priradi okno a id, kteremu se pri zobrazeni tooltipu zasle WM_USER_TTGETTEXT
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIHyperLinkAbstract
//

class CGUIHyperLinkAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
    //
    // Control lze v dialogu navstivit z klavesnice, pokud mu priradime styl WS_TABSTOP.
public:
    // nastavi text controlu; volani teto metody je rychlejsi a mene vypocetne narocne
    // nez nastavovani textu pomoci WM_SETTEXT; vraci TRUE v pripade uspechu, jinak FALSE
    virtual BOOL WINAPI SetText(const char* text) = 0;

    // vrati text controlu; mozne volat z libovolneho threadu
    // vrati NULL, pokud jeste nebylo volano SetText a static control byl bez textu
    virtual const char* WINAPI GetText() = 0;

    // priradi akci otevreni URL adresy (file="https://www.altap.cz") nebo
    // spusteni programu (file="C:\\TEST.EXE"); na parametr je volano
    // ShellExecute s 'open' commandem.
    virtual void WINAPI SetActionOpen(const char* file) = 0;

    // priradi akci PostCommand(WM_COMMAND, command, 0) do parent okna
    virtual void WINAPI SetActionPostCommand(WORD command) = 0;

    // priradi akci zobrazeni hintu a tooltipu 'text'
    // pokud je text NULL, je mozne tooltip priradit volanim metody
    // SetToolTipText nebo SetToolTip; metoda pak vraci vzdy TRUE
    // pokud je text ruzny od NULL, vraci metoda TRUE, pokud se podarilo
    // naalokovat kopii textu, jinak vraci FALSE
    // tooltip je mozne zobrazit kavesou Space/Up/Down (pokud je focus
    // na controlu) a kliknutim mysi; hint (tooltip) je pak zobrazen primo
    // pod textem a nezavre se dokud uzivatel neklikne mimo nej mysi nebo
    // nestiskne nejakou klavesu
    virtual BOOL WINAPI SetActionShowHint(const char* text) = 0;

    // priradi text, ktery bude zobrazen jako tooltip
    // vraci TRUE, pokud se podarilo naalokovat kopii textu, jinak vraci FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // priradi okno a id, kteremu se pri zobrazeni tooltipu zasle WM_USER_TTGETTEXT
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIButtonAbstract
//

class CGUIButtonAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
public:
    // priradi text, ktery bude zobrazen jako tooltip; vraci TRUE v pripade uspechu, jinak FALSE
    virtual BOOL WINAPI SetToolTipText(const char* text) = 0;

    // priradi okno a id, kteremu se pri zobrazeni tooltipu zasle WM_USER_TTGETTEXT
    virtual void WINAPI SetToolTip(HWND hNotifyWindow, DWORD id) = 0;
};

//
// ****************************************************************************
// CGUIColorArrowButtonAbstract
//

class CGUIColorArrowButtonAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
public:
    // nastavi barvu textu 'textColor' a barvu pozadi 'bkgndColor'
    virtual void WINAPI SetColor(COLORREF textColor, COLORREF bkgndColor) = 0;

    // nastavi barvu textu 'textColor'
    virtual void WINAPI SetTextColor(COLORREF textColor) = 0;

    // nastavi barvu pozadi 'bkgndColor'
    virtual void WINAPI SetBkgndColor(COLORREF bkgndColor) = 0;

    // vrati barvu textu
    virtual COLORREF WINAPI GetTextColor() = 0;

    // vrati barvu pozadi
    virtual COLORREF WINAPI GetBkgndColor() = 0;
};

//
// ****************************************************************************
// CGUIMenuPopupAbstract
//

#define MNTT_IT 1 // item
#define MNTT_PB 2 // popup begin
#define MNTT_PE 3 // popup end
#define MNTT_SP 4 // separator

#define MNTS_B 0x01 // skill level beginned
#define MNTS_I 0x02 // skill level intermediate
#define MNTS_A 0x04 // skill level advanced

struct MENU_TEMPLATE_ITEM
{
    int RowType;      // MNTT_*
    int TextResID;    // resource textu
    BYTE SkillLevel;  // MNTS_*
    DWORD ID;         // genrovany command
    short ImageIndex; // -1 = zadna ikonka
    DWORD State;
    DWORD* Enabler; // ridici promenna pro enablovani polozky
};

//
// constants
//

#define MENU_MASK_TYPE 0x00000001       // Retrieves or sets the 'Type' member.
#define MENU_MASK_STATE 0x00000002      // Retrieves or sets the 'State' member.
#define MENU_MASK_ID 0x00000004         // Retrieves or sets the 'ID' member.
#define MENU_MASK_SUBMENU 0x00000008    // Retrieves or sets the 'SubMenu' member.
#define MENU_MASK_CHECKMARKS 0x00000010 // Retrieves or sets the 'HBmpChecked' and 'HBmpUnchecked' members.
#define MENU_MASK_BITMAP 0x00000020     // Retrieves or sets the 'HBmpItem' member.
#define MENU_MASK_STRING 0x00000080     // Retrieves or sets the 'String' member.
#define MENU_MASK_IMAGEINDEX 0x00000100 // Retrieves or sets the 'ImageIndex' member.
#define MENU_MASK_ICON 0x00000200       // Retrieves or sets the 'HIcon' member.
#define MENU_MASK_OVERLAY 0x00000400    // Retrieves or sets the 'HOverlay' member.
#define MENU_MASK_CUSTOMDATA 0x00000800 // Retrieves or sets the 'CustomData' member.
#define MENU_MASK_ENABLER 0x00001000    // Retrieves or sets the 'Enabler' member.
#define MENU_MASK_SKILLLEVEL 0x00002000 // Retrieves or sets the 'SkillLevel' member.
#define MENU_MASK_FLAGS 0x00004000      // Retrieves or sets the 'Flags' member.

#define MENU_TYPE_STRING 0x00000001     // Displays the menu item using a text string.
#define MENU_TYPE_BITMAP 0x00000002     // Displays the menu item using a bitmap.
#define MENU_TYPE_SEPARATOR 0x00000004  // Specifies that the menu item is a separator.
#define MENU_TYPE_OWNERDRAW 0x00000100  // Assigns responsibility for drawing the menu item to the window that owns the menu.
#define MENU_TYPE_RADIOCHECK 0x00000200 // Displays selected menu items using a radio-button mark instead of a check mark if the HBmpChecked member is NULL.

#define MENU_FLAG_NOHOTKEY 0x00000001 // AssignHotKeys will skip this item

#define MENU_STATE_GRAYED 0x00000001  // Disables the menu item and grays it so that it cannot be selected.
#define MENU_STATE_CHECKED 0x00000002 // Checks the menu item.
#define MENU_STATE_DEFAULT 0x00000004 // Specifies that the menu item is the default. A menu can contain only one default menu item, which is displayed in bold.

#define MENU_LEVEL_BEGINNER 0x00000001
#define MENU_LEVEL_INTERMEDIATE 0x00000002
#define MENU_LEVEL_ADVANCED 0x00000004

#define MENU_POPUP_THREECOLUMNS 0x00000001
#define MENU_POPUP_UPDATESTATES 0x00000002 // pred otevrenim se zavola UpdateStates

// tyto flagy jsou v prubehu vetve modifikovany pro jednolive popupy
#define MENU_TRACK_SELECT 0x00000001 // If this flag is set, the function select item specified by SetSelectedItemIndex.
//#define MENU_TRACK_LEFTALIGN    0x00000000 // If this flag is set, the function positions the shortcut menu so that its left side is aligned with the coordinate specified by the x parameter.
//#define MENU_TRACK_TOPALIGN     0x00000000 // If this flag is set, the function positions the shortcut menu so that its top side is aligned with the coordinate specified by the y parameter.
//#define MENU_TRACK_HORIZONTAL   0x00000000 // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested horizontal alignment before the requested vertical alignment.
#define MENU_TRACK_CENTERALIGN 0x00000002  // If this flag is set, the function centers the shortcut menu horizontally relative to the coordinate specified by the x parameter.
#define MENU_TRACK_RIGHTALIGN 0x00000004   // Positions the shortcut menu so that its right side is aligned with the coordinate specified by the x parameter.
#define MENU_TRACK_VCENTERALIGN 0x00000008 // If this flag is set, the function centers the shortcut menu vertically relative to the coordinate specified by the y parameter.
#define MENU_TRACK_BOTTOMALIGN 0x00000010  // If this flag is set, the function positions the shortcut menu so that its bottom side is aligned with the coordinate specified by the y parameter.
#define MENU_TRACK_VERTICAL 0x00000100     // If the menu cannot be shown at the specified location without overlapping the excluded rectangle, the system tries to accommodate the requested vertical alignment before the requested horizontal alignment.
// spolecne flagy pro jednu vetev Track
#define MENU_TRACK_NONOTIFY 0x00001000  // If this flag is set, the function does not send notification messages when the user clicks on a menu item.
#define MENU_TRACK_RETURNCMD 0x00002000 // If this flag is set, the function returns the menu item identifier of the user's selection in the return value.
//#define MENU_TRACK_LEFTBUTTON   0x00000000 // If this flag is set, the user can select menu items with only the left mouse button.
#define MENU_TRACK_RIGHTBUTTON 0x00010000 // If this flag is set, the user can select menu items with both the left and right mouse buttons.
#define MENU_TRACK_HIDEACCEL 0x00100000   // Salamander 2.51 or later: If this flag is set, the acceleration keys will not be underlined (specify when menu is opened by mouse event).

class CGUIMenuPopupAbstract;

struct MENU_ITEM_INFO
{
    DWORD Mask;
    DWORD Type;
    DWORD State;
    DWORD ID;
    CGUIMenuPopupAbstract* SubMenu;
    HBITMAP HBmpChecked;
    HBITMAP HBmpUnchecked;
    HBITMAP HBmpItem;
    char* String;
    DWORD StringLen;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    ULONG_PTR CustomData;
    DWORD SkillLevel;
    DWORD* Enabler;
    DWORD Flags;
};

/*
Mask
  Members to retrieve or set. This member can be one or more of these values.

Type
  Typ polozky. Tato promenna muze nabivat jednu nebo vice hodnot:

   MENU_TYPE_OWNERDRAW    Vykresleni polozek zajisti okno vlasntici menu.
                          Pro kazdou polozku menu je zaslan dotaz WM_MEASUREITEM a
                          WM_DRAWITEM. Promenna TypeData obsahuje 32-buitovou
                          hodnotu definovanou aplikaci.

   MENU_TYPE_RADIOCHECK   Checked polozky jsou zobrazovany s teckou misto s fajfkou,
                          je-li HBmpChecked rovno NULL.

   MENU_TYPE_SEPARATOR    Horiznotalni oddelovaci cara. TypeData nema vyznam.

   MENU_TYPE_STRING       Polozka obsahuje textovy retezec. TypeData ukazuje na nulou
                          zakonceny retzec.

   MENU_TYPE_BITMAP       Polozka obsahuje bitmapu.

  Hodnoty MENU_TYPE_BITMAP, MENU_TYPE_SEPARATOR a MENU_TYPE_STRING nemohou byt pouzity spolecne.

State
  Stav polozky. Tato promenna muze nabivat jednu nebo vice hodnot:

   MENU_STATE_CHECKED     Polozka je checked.

   MENU_STATE_DEFAULT     Menu muze obsahovat pouze jednu default polozku. Je
                          vykreslna tucne.

   MENU_STATE_GRAYED      Zakaze polozku - bude sediva a nepujde vybrat.

SkillLevel
  Uzivatelska uroven polozky. Tato promena muze nabyvat jedne z hodnot:

   MENU_LEVEL_BEGINNER       zacatecnik - bude se zobrazovat vzdy
   MENU_LEVEL_INTERMEDIATE   stredni - bude se zobrazovat guru a strednim uzivatelum
   MENU_LEVEL_ADVANCED       pokrocily - bude se zobrazovat pouze guru uzivatelum

ID
  Aplikaci definovana 16-bitova hodnota, ktera identifikuje polozku menu.

SubMenu
  Ukazatel na popup menu pripojeneho na tutu polozku. Pokud tato polozka
  neotevira submenu, je SubMenu rovno NULL.

HBmpChecked
  Handle bitmapy, ktera je zobrazena pred polozkou v pripade, ze je polozka
  checked. Je-li tato promenna rovna NULL, pouzije
  se implicitni bitmapa. Jel-li nastaven bit MENU_TYPE_RADIOCHECK, je implicitni
  bitmapa tecka, jinak zatrznitko. Je-li ImageIndex ruzne od -1, nebude
  se tato bitmapa pouzivat.

HBmpUnchecked
  Handle bitmapy, ktera je zobrazena pred polozkou v pripade, ze neni polozka
  checked. Jeli tato promenna rovna NULL, nebude
  zobrazena zadna bitmapa. Je-li ImageIndex ruzne od -1, nebude
  se tato bitmapa pouzivat.

ImageIndex
  Index bitmapy v ImageListu CMenuPopup::HImageList. Bitmapa je vykreslena
  pred polozkou. V zavislosti na MENU_STATE_CHECKED a MENU_STATE_GRAYED.
  Je-li promenna rovna -1, nebude se vykreslovat.

Enabler
  Ukazatel na DWORD, ktery urcuje stav polozky: TRUE->enabled, FALSE->grayed.
  Pokud je NULL, polozka bude enabled.
*/

class CGUIMenuPopupAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
public:
    //
    // LoadFromTemplate
    //   Builds menu contents based on 'menuTemplate',
    //
    // Parameters
    //   'hInstance'
    //      [in] Handle to the module containing the string resources (MENU_TEMPLATE_ITEM::TextResID).
    //
    //   'menuTemplate'
    //      [in] Pointer to a menu template.
    //
    //      A menu template consists of two or more MENU_TEMPLATE_ITEM structures.
    //      'MENU_TEMPLATE_ITEM::RowType' of first structure must be MNTT_PB (popup begin).
    //      'MENU_TEMPLATE_ITEM::RowType' of last structure must be MNTT_PE (popup end).
    //
    //   'enablersOffset'
    //      [in] Pointer to array of enablers.
    //
    //      If this parameter is NULL, 'MENU_ITEM_INFO::Enabler' value is pointer to enabler
    //      variable. Otherwise 'MENU_ITEM_INFO::Enabler' is index to the enablers array.
    //      Zero index is reserved for "always enabled" item.
    //
    //   'hImageList'
    //      [in] Handle of image list that the menu will use to display menu items images
    //      that are in their default state.
    //
    //      If this parameter is NULL, no images will be displayed in the menu items.
    //
    //   'hHotImageList'
    //      [in] Handle of image list that the menu will use to display menu items images
    //      that are in their selected or checked state.
    //
    //      If this parameter is NULL, normal images will be displayed instead of hot images.
    //
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI LoadFromTemplate(HINSTANCE hInstance,
                                         const MENU_TEMPLATE_ITEM* menuTemplate,
                                         DWORD* enablersOffset,
                                         HIMAGELIST hImageList,
                                         HIMAGELIST hHotImageList) = 0;

    //
    // SetSelectedItemIndex
    //   Sets the item which will be selected when menu popup is displayed.
    //
    // Parameters
    //   'index'
    //      [in] The index to select.
    //      If this value is -1, none of the items will be selected.
    //      This index is only applied when method Track with MENU_TRACK_SELECT flag is called.
    //
    // See Also
    //   GetSelectedItemIndex
    //
    virtual void WINAPI SetSelectedItemIndex(int index) = 0;

    //
    // GetSelectedItemIndex
    //   Retrieves the currently selected item in the menu.
    //
    // Return Values
    //   Returns the index of the selected item, or -1 if no item is selected.
    //
    // See Also
    //   SetSelectedItemIndex
    //
    virtual int WINAPI GetSelectedItemIndex() = 0;

    //
    // SetTemplateMenu
    //   Assigns the Windows menu handle which will be used as template when menu popup is displayed.
    //
    // Parameters
    //   'hWindowsMenu'
    //      [in] Handle to the Windows menu handle to be used as template.
    //      If this value is NULL, Windows menu handle will not be used.
    //
    // See Also
    //   GetTemplateMenu
    //
    virtual void WINAPI SetTemplateMenu(HMENU hWindowsMenu) = 0;

    //
    // GetTemplateMenu
    //   Retrieves a handle to the Windows menu assigned as template.
    //
    // Return Values
    //   The return value is a handle to the Windows menu.
    //   If the object has no Windows menu template assigned, the return value is NULL.
    //
    // See Also
    //   SetTemplateMenu
    //
    virtual HMENU WINAPI GetTemplateMenu() = 0;

    //
    // GetSubMenu
    //   Retrieves a pointer to the submenu activated by the specified menu item.
    //
    // Parameters
    //   'position'
    //      [in] Specifies the zero-based position of the menu item that activates the submenu.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    // Return Values
    //   If the function succeeds, the return value is a pointer to the submenu activated by the menu item.
    //   If the menu item does not activate submenu, the return value is NULL.
    //

    virtual CGUIMenuPopupAbstract* WINAPI GetSubMenu(DWORD position, BOOL byPosition) = 0;

    //
    // InsertItem
    //   Inserts a new menu item at the specified position in a menu.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item before which to insert the new item.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in] Pointer to a MENU_ITEM_INFO structure that contains information about the new menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   SetItemInfo, GetItemInfo
    //
    virtual BOOL WINAPI InsertItem(DWORD position,
                                   BOOL byPosition,
                                   const MENU_ITEM_INFO* mii) = 0;

    //
    // SetItemInfo
    //   Changes information about a menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in] Pointer to a MENU_ITEM_INFO structure that contains information about the menu item
    //      and specifies which menu item attributes to change.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem, GetItemInfo
    //
    virtual BOOL WINAPI SetItemInfo(DWORD position,
                                    BOOL byPosition,
                                    const MENU_ITEM_INFO* mii) = 0;

    //
    // GetItemInfo
    //   Retrieves information about a menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to get information about.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'mii'
    //      [in/out] Pointer to a MENU_ITEM_INFO structure that contains information to retrieve
    //      and receives information about the menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI GetItemInfo(DWORD position,
                                    BOOL byPosition,
                                    MENU_ITEM_INFO* mii) = 0;

    //
    // SetStyle
    //   Sets the menu popup style.
    //
    // Parameters
    //   'style'
    //      [in] New menu style.
    //      This parameter can be a combination of menu popup styles MENU_POPUP_*.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI SetStyle(DWORD style) = 0;

    //
    // CheckItem
    //   Sets the state of the specified menu item's check-mark attribute to either checked or clear.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'checked'
    //      [in] Indicates whether the menu item will be checked or cleared.
    //      If this parameter is TRUE, sets the check-mark attribute to the selected state.
    //      If this parameter is FALSE, sets the check-mark attribute to the clear state.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckRadioItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckItem(DWORD position,
                                  BOOL byPosition,
                                  BOOL checked) = 0;

    //
    // CheckRadioItem
    //   Checks a specified menu item and makes it a radio item. At the same time, the method
    //   clears all other menu items in the associated group and clears the radio-item type
    //   flag for those items.
    //
    // Parameters
    //   'positionFirst'
    //      [in] Identifier or position of the first menu item in the group.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'positionLast'
    //      [in] Identifier or position of the last menu item in the group.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'positionCheck'
    //      [in] Identifier or position of the menu item to check.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'positionFirst', 'positionLast', and
    //      'positionCheck'. If this parameter is FALSE, the other parameters specify
    //      menu item identifiers. Otherwise, the other parameters specify the menu
    //      item positions.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckRadioItem(DWORD positionFirst,
                                       DWORD positionLast,
                                       DWORD positionCheck,
                                       BOOL byPosition) = 0;

    //
    // SetDefaultItem
    //   Sets the default menu item for the specified menu.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the new default menu item or –1 for no default item.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, CheckItem, CheckRadioItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI SetDefaultItem(DWORD position,
                                       BOOL byPosition) = 0;

    //
    // EnableItem
    //   Enables or disables the specified menu item.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the menu item to be enabled or disabled.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a menu item identifier. Otherwise, it is a zero-based menu item position.
    //
    //   'enabled'
    //      [in] Indicates whether the menu item will be enabled or disabled.
    //      If this parameter is TRUE, enables the menu item.
    //      If this parameter is FALSE, disables the menu item.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI EnableItem(DWORD position,
                                   BOOL byPosition,
                                   BOOL enabled) = 0;

    //
    // EnableItem
    //   Determines the number of items in the specified menu.
    //
    // Return Values
    //   The return value specifies the number of items in the menu.
    //
    virtual int WINAPI GetItemCount() = 0;

    //
    // RemoveAllItems
    //   Deletes all items from the menu popup.
    //   If the removed menu item opens submenu, this method frees the memory used by the submenu.
    //
    // See Also
    //   RemoveItemsRange
    //
    virtual void WINAPI RemoveAllItems() = 0;

    //
    // RemoveItemsRange
    //   Deletes items range from the menu popup.
    //
    // Parameters
    //   'firstIndex'
    //      [in] Specifies the first menu item to be deleted.
    //
    //   'lastIndex'
    //      [in] Specifies the last menu item to be deleted.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   RemoveAllItems
    //
    virtual BOOL WINAPI RemoveItemsRange(int firstIndex,
                                         int lastIndex) = 0;

    //
    // BeginModifyMode
    //   Allows changes of the opened menu popup.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EndModifyMode
    //
    virtual BOOL WINAPI BeginModifyMode() = 0;

    //
    // EndModifyMode
    //   Ends modify mode.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   BeginModifyMode
    //
    virtual BOOL WINAPI EndModifyMode() = 0;

    //
    // SetSkillLevel
    //   Sets user skill level for this menu popup.
    //
    // Parameters
    //   'skillLevel'
    //      [in] Specifies the user skill level.
    //      This parameter must be one or a combination of MENU_LEVEL_*.
    //
    virtual void WINAPI SetSkillLevel(DWORD skillLevel) = 0;

    //
    // FindItemPosition
    //   Retrieves the menu item position in the menu popup.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the identifier of the menu item whose position is to be retrieved.
    //
    // Return Values
    //   Zero-based position of the specified menu item.
    //   If menu item is not found, return value is -1.
    //
    virtual int WINAPI FindItemPosition(DWORD id) = 0;

    //
    // FillMenuHandle
    //   Inserts the menu items to the Windows menu popup.
    //
    // Parameters
    //   'hMenu'
    //      [in] Handle to the menu to be filled.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI FillMenuHandle(HMENU hMenu) = 0;

    //
    // GetStatesFromHWindowsMenu
    //   Applies Windows menu popup item states to the contained items.
    //
    // Parameters
    //   'hMenu'
    //      [in] Handle to the menu whose item states are to be retrieved.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetStatesFromHWindowsMenu(HMENU hMenu) = 0;

    //
    // SetImageList
    //   Sets the image list that the menu will use to display images in the items that
    //   are in their default state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //      If this parameter is NULL, no images will be displayed in the items.
    //
    //   'subMenu'
    //      [in] Specifies whether to set SubMenus image list to.
    //      If this parameter is TRUE, image list will be set also in all submenu items,
    //      otherwise image list will be set only in this menu popup.
    //
    // See Also
    //   SetHotImageList
    //
    virtual void WINAPI SetImageList(HIMAGELIST hImageList,
                                     BOOL subMenu) = 0;

    //
    // SetHotImageList
    //   Sets the image list that the menu will use to display images in the items that
    //   are in their hot or checked state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //      If this parameter is NULL, no images will be displayed in the items.
    //
    //   'subMenu'
    //      [in] Specifies whether to set SubMenus image list to.
    //      If this parameter is TRUE, image list will be set also in all submenu items,
    //      otherwise image list will be set only in this menu popup.
    //
    // See Also
    //   SetImageList
    //
    virtual void WINAPI SetHotImageList(HIMAGELIST hHotImageList,
                                        BOOL subMenu) = 0;

    //
    // Track
    //   Displays a shortcut menu at the specified location and tracks the selection of
    //   items on the shortcut menu.
    //
    // Parameters
    //   'trackFlags'
    //      [in] Specifies function options.
    //      This parameter can be a combination of MENU_TRACK_* flags.
    //
    //   'x'
    //      [in] Horizontal location of the shortcut menu, in screen coordinates.
    //
    //   'y'
    //      [in] Vertical location of the shortcut menu, in screen coordinates.
    //
    //   'hwnd'
    //      [in] Handle to the window that owns the shortcut menu. This window
    //      receives all messages from the menu. The window does not receive
    //      a WM_COMMAND message from the menu until the function returns.
    //
    //      If you specify MENU_TRACK_NONOTIFY in the 'trackFlags' parameter,
    //      the function does not send messages to the window identified by hwnd.
    //      However, you still have to pass a window handle in 'hwnd'. It can be
    //      any window handle from your application.
    //   'exclude'
    //      [in] Rectangle to exclude when positioning the window, in screen coordinates.
    //
    // Return Values
    //   If you specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the return
    //   value is the menu-item identifier of the item that the user selected. If the
    //   user cancels the menu without making a selection, or if an error occurs, then
    //   the return value is zero.
    //
    //   If you do not specify MENU_TRACK_RETURNCMD in the 'trackFlags' parameter, the
    //   return value is nonzero if the function succeeds and zero if it fails.
    //
    virtual DWORD WINAPI Track(DWORD trackFlags,
                               int x,
                               int y,
                               HWND hwnd,
                               const RECT* exclude) = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a item in the menu.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the item for which to retrieve information.
    //
    //   'rect'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index,
                                    RECT* rect) = 0;

    //
    // UpdateItemsState
    //   Updates state for all items with specified 'Enabler'.
    //   Call this method when enabler variables altered.
    //
    virtual void WINAPI UpdateItemsState() = 0;

    //
    // SetMinWidth
    //   Sets the minimum width to be used for menu popup.
    //
    // Parameters
    //   'minWidth'
    //      [in] Specifies the minimum width of the menu popup.
    //
    virtual void WINAPI SetMinWidth(int minWidth) = 0;

    //
    // SetPopupID
    //   Sets the ID for menu popup.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the ID of the menu popup.
    //
    virtual void WINAPI SetPopupID(DWORD id) = 0;

    //
    // GetPopupID
    //   Retrieves the ID for menu popup.
    //
    // Return Values
    //   Returns the ID for menu popup.
    //
    virtual DWORD WINAPI GetPopupID() = 0;

    //
    // AssignHotKeys
    //   Automatically assigns hot keys to the menu items that
    //   has not hot key already assigned.
    //
    virtual void WINAPI AssignHotKeys() = 0;
};

//
// ****************************************************************************
// CGUIMenuBarAbstract
//

class CGUIMenuBarAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
public:
    //
    // CreateWnd
    //   Creates child toolbar window.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the parent or owner window of the toolbar being created.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI CreateWnd(HWND hParent) = 0;

    //
    // GetHWND
    //   Retrieves Windows HWND value of the toolbar.
    //
    // Return Values
    //   The Windows HWND handle of the toolbar.
    //
    virtual HWND WINAPI GetHWND() = 0;

    //
    // GetNeededWidth
    //   Retrieves the total width of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed width for the toolbar.
    //
    // See Also
    //   GetNeededHeight
    //
    virtual int WINAPI GetNeededWidth() = 0;

    //
    // GetNeededHeight
    //   Retrieves the total height of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed height for the toolbar.
    //
    // See Also
    //   GetNeededWidth
    //
    virtual int WINAPI GetNeededHeight() = 0;

    //
    // SetFont
    //   Updates the font that a menubar is to use when drawing text.
    //   Call this method after receiving PLUGINEVENT_SETTINGCHANGE through CPluginInterface::Event().
    //
    virtual void WINAPI SetFont() = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a button in the toolbar.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the button for which to retrieve information.
    //
    //   'r'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index, RECT& r) = 0;

    // prepne menu do Menu mode (jako by user stisknul a pustil Alt)
    virtual void WINAPI EnterMenu() = 0;
    // vraci TRUE, pokud je menu v Menu mode
    virtual BOOL WINAPI IsInMenuLoop() = 0;
    // prepina menu do Help mode (Shift + F1)
    virtual void WINAPI SetHelpMode(BOOL helpMode) = 0;

    //
    // IsMenuBarMessage
    //   The IsMenuBarMessage method determines whether a message is intended for
    //   the menubar or menu and, if it is, processes the message.
    //
    // Parameters
    //   'lpMsg'
    //      [in] Pointer to an MSG structure that contains the message to be checked.
    //
    // Return Values
    //   If the message has been processed, the return value is nonzero.
    //   If the message has not been processed, the return value is zero.
    //
    virtual BOOL WINAPI IsMenuBarMessage(CONST MSG* lpMsg) = 0;
};

//
// ****************************************************************************
// CGUIToolBarAbstract
//

// Toolbar Styles

#define TLB_STYLE_IMAGE 0x00000001      // budou zobrazovany ikonky s ImageIndex != -1 \
                                        // zaroven GetNeededSpace bude pocitat s vyskou ikonek
#define TLB_STYLE_TEXT 0x00000002       // budou zobrazovany u polozek s nastavenym TLBI_STYLE_SHOWTEXT \
                                        // zaroven GetNeededSpace bude pocitat s velikosti fontu
#define TLB_STYLE_ADJUSTABLE 0x00000004 // lze toolbar konfigurovat?
#define TLB_STYLE_VERTICAL 0x00000008   // tlacitka jsou pod sebou, separatory vodorovne, vylucuje se s TLB_STYLE_TEXT, \
                                        // protoze svisle texty nejsou podporovany

// Toolbar Item Masks
#define TLBI_MASK_ID 0x00000001         // Retrieves or sets the 'ID' member.
#define TLBI_MASK_CUSTOMDATA 0x00000002 // Retrieves or sets the 'CustomData' member.
#define TLBI_MASK_IMAGEINDEX 0x00000004 // Retrieves or sets the 'ImageIndex' member.
#define TLBI_MASK_ICON 0x00000008       // Retrieves or sets the 'HIcon' member.
#define TLBI_MASK_STATE 0x00000010      // Retrieves or sets the 'State' member.
#define TLBI_MASK_TEXT 0x00000020       // Retrieves or sets the 'Text' member.
#define TLBI_MASK_TEXTLEN 0x00000040    // Retrieves the 'TextLen' member.
#define TLBI_MASK_STYLE 0x00000080      // Retrieves or sets the 'Style' member.
#define TLBI_MASK_WIDTH 0x00000100      // Retrieves or sets the 'Width' member.
#define TLBI_MASK_ENABLER 0x00000200    // Retrieves or sets the 'Enabler' member.
#define TLBI_MASK_OVERLAY 0x00000800    // Retrieves or sets the 'HOverlay' member.

// Toolbar Item Styles
#define TLBI_STYLE_CHECK 0x00000001 // Creates a dual-state push button that toggles between \
                                    // the pressed and nonpressed states each time the user \
                                    // clicks it. The button has a different background color \
                                    // when it is in the pressed state.

#define TLBI_STYLE_RADIO 0x00000002 // Pokud neni pri kliknuti TLBI_STATE_CHECKED, prepne se \
                                    // do tohoto stavu. Pokud uz tam je, zustane tam.

#define TLBI_STYLE_DROPDOWN 0x00000004 // Creates a drop-down style button that can display a \
                                       // list when the button is clicked. Instead of the \
                                       // WM_COMMAND message used for normal buttons, \
                                       // drop-down buttons send a WM_USER_TBDROPDOWN notification. \
                                       // An application can then have the notification handler \
                                       // display a list of options.

#define TLBI_STYLE_NOPREFIX 0x00000008 // Specifies that the button text will not have an \
                                       // accelerator prefix associated with it.

#define TLBI_STYLE_SEPARATOR 0x00000010 // Creates a separator, providing a small gap between \
                                        // button groups. A button that has this style does not \
                                        // receive user input.

#define TLBI_STYLE_SHOWTEXT 0x00000020 // Specifies that button text should be displayed. \
                                       // All buttons can have text, but only those buttons \
                                       // with the BTNS_SHOWTEXT button style will display it. \
                                       // This style must be used with the TLB_STYLE_TEXT style.

#define TLBI_STYLE_WHOLEDROPDOWN 0x00000040 // Specifies that the button will have a drop-down arrow, \
                                            // but not as a separate section.

#define TLBI_STYLE_SEPARATEDROPDOWN 0x00000080 // Specifies that the button will have a drop-down arrow, \
                                               // in separated section.

#define TLBI_STYLE_FIXEDWIDTH 0x00000100 // Sirka teto polozky neni automaticky pocitana.

// Toolbar Item States
#define TLBI_STATE_CHECKED 0x00000001         // The button has the TLBI_STYLE_CHECK style and is being clicked.
#define TLBI_STATE_GRAYED 0x00000002          // The button is grayed and cannot receive user input.
#define TLBI_STATE_PRESSED 0x00000004         // The button is being clicked.
#define TLBI_STATE_DROPDOWNPRESSED 0x00000008 // The drop down is being clicked.

struct TLBI_ITEM_INFO2
{
    DWORD Mask;
    DWORD Style;
    DWORD State;
    DWORD ID;
    char* Text;
    int TextLen;
    int Width;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    DWORD CustomData; // FIXME_X64 - male pro ukazatel, neni nekdy potreba?
    DWORD* Enabler;

    DWORD Index;
    char* Name;
    int NameLen;
};

/*
Mask
  TLBI_MASK_*

Style
  TLBI_STYLE_*

State
  TLBI_STATE_*

ID
  Command identifier associated with the button.
  This identifier is used in a WM_COMMAND message when the button is chosen.

Text
  Text string displayed in the toolbar item.

TextLen
  Length of the toolbar item text, when information is received.

Width
  Width of the toolbar item text.

ImageIndex
  Zero-based index of the button image in the image list.

HIcon
  Handle to the icon to display instead of image list image.
  Icon will not be destroyet.

CustomData
  Application-defined value associated with the toolbar item.

Enabler
  Pointer to the item enabler. Used in the UpdateItemsState.
  0 -> item is TLBI_STATE_GRAYED; else item is enabled

Index
  For enumeration items in customize dialog.

Name
  Name in customize dialog.

NameLen
  Name len in customize dialog.
*/

struct TOOLBAR_PADDING // The padding values are used to create a blank area
{
    WORD ToolBarVertical; // blank area above and below the button
    WORD ButtonIconText;  // blank area between icon and text
    WORD IconLeft;        // blank area before icon
    WORD IconRight;       // blank area behind icon
    WORD TextLeft;        // blank area before text
    WORD TextRight;       // blank area behind text
};

struct TOOLBAR_TOOLTIP
{
    HWND HToolBar;    // okno, ktere se dotazuje na tooltip
    DWORD ID;         // ID buttonu, pro ktere je pozadovan tooltip
    DWORD Index;      // index buttonu, pro ktere je pozadovan tooltip
    DWORD CustomData; // custom data buttonu, pokud jsou definovany // FIXME_X64 - male pro ukazatel, neni nekdy potreba?
    char* Buffer;     // tento buffer je treba naplnit, maximalni pocet znaku je TOOLTIP_TEXT_MAX
                      // implicitne message je na nulty znak vlozen terminator
};

class CGUIToolBarAbstract
{
    // Vsechny metody je mozne volat pouze z threadu parent okna, ve kterem
    // byl objekt pripojen na windows control a ziskan ukazatel na toto rozhrani.
public:
    //
    // CreateWnd
    //   Creates child toolbar window.
    //
    // Parameters
    //   'hParent'
    //      [in] Handle to the parent or owner window of the toolbar being created.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI CreateWnd(HWND hParent) = 0;

    //
    // GetHWND
    //   Retrieves Windows HWND value of the toolbar.
    //
    // Return Values
    //   The Windows HWND handle of the toolbar.
    //
    virtual HWND WINAPI GetHWND() = 0;

    //
    // GetNeededWidth
    //   Retrieves the total width of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed width for the toolbar.
    //
    // See Also
    //   GetNeededHeight
    //
    virtual int WINAPI GetNeededWidth() = 0;

    //
    // GetNeededHeight
    //   Retrieves the total height of all of the visible buttons and separators
    //   in the toolbar.
    //
    // Return Values
    //   Returns the needed height for the toolbar.
    //
    // See Also
    //   GetNeededWidth
    //
    virtual int WINAPI GetNeededHeight() = 0;

    //
    // SetFont
    //   Updates the font that a menubar is to use when drawing text.
    //   Call this method after receiving PLUGINEVENT_SETTINGCHANGE through CPluginInterface::Event().
    //
    virtual void WINAPI SetFont() = 0;

    //
    // GetItemRect
    //   Retrieves the bounding rectangle of a button in the toolbar.
    //
    // Parameters
    //   'index'
    //      [in] Zero-based index of the button for which to retrieve information.
    //
    //   'r'
    //      [out] Address of a RECT structure that receives the screen coordinates
    //      of the bounding rectangle.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI GetItemRect(int index, RECT& r) = 0;

    // CheckItem
    //   Sets the state of the specified button's attribute to either checked or normal.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'checked'
    //      [in] Indicates whether the button will be checked or cleared.
    //      If this parameter is TRUE, sets the button to the checked state.
    //      If this parameter is FALSE, sets the button to the normal state.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   EnableItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI CheckItem(DWORD position,
                                  BOOL byPosition,
                                  BOOL checked) = 0;

    //
    // EnableItem
    //   Enables or disables the specified button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to be enabled or disabled.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'enabled'
    //      [in] Indicates whether the button will be enabled or disabled.
    //      If this parameter is TRUE, enables the button.
    //      If this parameter is FALSE, disables the button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   CheckItem, InsertItem, SetItemInfo
    //
    virtual BOOL WINAPI EnableItem(DWORD position,
                                   BOOL byPosition,
                                   BOOL enabled) = 0;

    //
    // ReplaceImage
    //   Replaces an existing bitmap with a new bitmap.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'hIcon'
    //      [in] Handle to the icon that contains the bitmap and mask for the new image.
    //
    //   'normal'
    //      [in] Specifies whether to replace normal image list icon.
    //
    //   'hot'
    //      [in] Specifies whether to replace hot image list icon.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    virtual BOOL WINAPI ReplaceImage(DWORD position,
                                     BOOL byPosition,
                                     HICON hIcon,
                                     BOOL normal,
                                     BOOL hot) = 0;

    //
    // FindItemPosition
    //   Retrieves the button position in the toolbar.
    //
    // Parameters
    //   'id'
    //      [in] Specifies the identifier of the button whose position is to be retrieved.
    //
    // Return Values
    //   Zero-based position of the specified button.
    //   If button is not found, return value is -1.
    //
    virtual int WINAPI FindItemPosition(DWORD id) = 0;

    //
    // SetImageList
    //   Sets the image list that the toolbar will use to display images in the button
    //   that are in their default state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //
    // See Also
    //   GetImageList, GetHotImageList, SetHotImageList
    //
    virtual void WINAPI SetImageList(HIMAGELIST hImageList) = 0;

    //
    // GetImageList
    //   Retrieves the image list that a toolbar uses to display buttons
    //   in their default state.
    //
    // Return Values
    //   Returns the handle to the image list, or NULL if no image list is set.
    //
    // See Also
    //   SetImageList, GetHotImageList, SetHotImageList
    //
    virtual HIMAGELIST WINAPI GetImageList() = 0;

    //
    // SetHotImageList
    //   Sets the image list that the toolbar will use to display images in the button
    //   that are in their hot state.
    //
    // Parameters
    //   'hImageList'
    //      [in] Handle to the image list that will be set.
    //
    // See Also
    //   SetImageList, GetImageList, SetHotImageList
    //
    virtual void WINAPI SetHotImageList(HIMAGELIST hImageList) = 0;

    //
    // GetHotImageList
    //   Retrieves the image list that a toolbar uses to display hot buttons.
    //
    // Return Values
    //   Returns the handle to the image list that the toolbar uses to display hot
    //   buttons, or NULL if no hot image list is set.
    //
    // See Also
    //   SetImageList, GetImageList, SetHotImageList
    //
    virtual HIMAGELIST WINAPI GetHotImageList() = 0;

    //
    // SetStyle
    //   Sets the styles for the toolbar.
    //
    // Parameters
    //   'style'
    //      [in] Value specifying the styles to be set for the toolbar.
    //      This parameter can be a combination of TLB_STYLE_* styles.
    //
    // See Also
    //   GetStyle
    //
    virtual void WINAPI SetStyle(DWORD style) = 0;

    //
    // GetStyle
    //   Retrieves the styles currently in use for the toolbar.
    //
    // Return Values
    //   Returns a DWORD value that is a combination of TLB_STYLE_* styles.
    //
    // See Also
    //   SetStyle
    //
    virtual DWORD WINAPI GetStyle() = 0;

    //
    // RemoveItem
    //   Deletes a button from the toolbar.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to delete.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   RemoveAllItems
    //
    virtual BOOL WINAPI RemoveItem(DWORD position,
                                   BOOL byPosition) = 0;

    //
    // RemoveAllItems
    //   Deletes all buttons from the toolbar.
    //
    // See Also
    //   RemoveItem
    //
    virtual void WINAPI RemoveAllItems() = 0;

    //
    // GetItemCount
    //   Retrieves a count of the buttons currently in the toolbar.
    //
    // Return Values
    //   Returns the count of the buttons.
    //
    virtual int WINAPI GetItemCount() = 0;

    //
    // Customize
    //   Displays the Customize Toolbar dialog box.
    //
    virtual void WINAPI Customize() = 0;

    //
    // SetPadding
    //   Sets the padding for a toolbar control.
    //
    // Parameters
    //   'padding'
    //      [in] Address of a TOOLBAR_PADDING structure that contains
    //      the new padding information.
    //
    // See Also
    //   GetPadding
    //

    virtual void WINAPI SetPadding(const TOOLBAR_PADDING* padding) = 0;

    //
    // GetPadding
    //   Retrieves the padding for the toolbar.
    //
    // Parameters
    //   'padding'
    //      [out] Address of a TOOLBAR_PADDING structure that will receive
    //      the padding information.
    //
    // See Also
    //   SetPadding
    //
    virtual void WINAPI GetPadding(TOOLBAR_PADDING* padding) = 0;

    //
    // UpdateItemsState
    //   Updates state for all items with specified 'Enabler'.
    //   Call this method when enabler variables altered.
    //
    virtual void WINAPI UpdateItemsState() = 0;

    //
    // HitTest
    //   Determines where a point lies in the toolbar.
    //
    // Parameters
    //   'xPos'
    //      [in] The x-coordinate of the hit test.
    //
    //   'yPos'
    //      [in] The y-coordinate of the hit test.
    //
    // Return Values
    //   Returns an integer value. If the return value is zero or a positive value,
    //   it is the zero-based index of the nonseparator item in which the point lies.
    //   If the return value is negative, the point does not lie within a button.
    //
    // Remarks
    //   The coordinates are relative to the toolbar's client area.
    //
    virtual int WINAPI HitTest(int xPos,
                               int yPos) = 0;

    //
    // InsertMarkHitTest
    //   Retrieves the insertion mark information for a point in the toolbar.
    //
    // Parameters
    //   'xPos'
    //      [in] The x-coordinate of the hit test.
    //
    //   'yPos'
    //      [in] The y-coordinate of the hit test.
    //
    //   'index'
    //      [out] Zero-based index of the button with insertion mark.
    //      If this member is -1, there is no insertion mark.
    //
    //   'after'
    //      [out] Defines where the insertion mark is in relation to 'index'.
    //      If the value is FALSE, the insertion mark is to the left of the specified button.
    //      If the value is TRUE, the insertion mark is to the right of the specified button.
    //
    // Return Values
    //   Returns TRUE if the point is an insertion mark, or FALSE otherwise.
    //
    // Remarks
    //   The coordinates are relative to the toolbar's client area.
    //
    // See Also
    //   SetInsertMark
    //
    virtual BOOL WINAPI InsertMarkHitTest(int xPos,
                                          int yPos,
                                          int& index,
                                          BOOL& after) = 0;

    //
    // SetInsertMark
    //   Sets the current insertion mark for the toolbar.
    //
    // Parameters
    //   'index'
    //      [out] Zero-based index of the button with insertion mark.
    //      If this member is -1, there is no insertion mark.
    //
    //   'after'
    //      [out] Defines where the insertion mark is in relation to 'index'.
    //      If the value is FALSE, the insertion mark is to the left of the specified button.
    //      If the value is TRUE, the insertion mark is to the right of the specified button.
    //
    // See Also
    //   InsertMarkHitTest
    //
    virtual void WINAPI SetInsertMark(int index,
                                      BOOL after) = 0;

    //
    // SetHotItem
    //   Sets the hot item in the toolbar.
    //
    // Parameters
    //   'index'
    //      [out] Zero-based index of the item that will be made hot.
    //      If this value is -1, none of the items will be hot.
    //
    // Return Values
    //   Returns the index of the previous hot item, or -1 if there was no hot item.
    //
    virtual int WINAPI SetHotItem(int index) = 0;

    //
    // SetHotItem
    //   Updates the toolbar graphic handles.
    //
    virtual void WINAPI OnColorsChanged() = 0;

    //
    // InsertItem2
    //   Inserts a new button at the specified position in a toolbar.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button before which to insert the new button.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'tii'
    //      [in] Pointer to a TLBI_ITEM_INFO2 structure that contains information about the
    //      new button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   SetItemInfo2, GetItemInfo2
    virtual BOOL WINAPI InsertItem2(DWORD position,
                                    BOOL byPosition,
                                    const TLBI_ITEM_INFO2* tii) = 0;

    //
    // SetItemInfo2
    //   Changes information about a button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to change.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'mii'
    //      [in] Pointer to a TLBI_ITEM_INFO2 structure that contains information about the button
    //      and specifies which button attributes to change.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem2, GetItemInfo2
    virtual BOOL WINAPI SetItemInfo2(DWORD position,
                                     BOOL byPosition,
                                     const TLBI_ITEM_INFO2* tii) = 0;

    //
    // GetItemInfo2
    //   Retrieves information about a button.
    //
    // Parameters
    //   'position'
    //      [in] Identifier or position of the button to get information about.
    //      The meaning of this parameter depends on the value of 'byPosition'.
    //
    //   'byPosition'
    //      [in] Value specifying the meaning of 'position'. If this parameter is FALSE, 'position'
    //      is a button identifier. Otherwise, it is a zero-based button position.
    //
    //   'mii'
    //      [in/out] Pointer to a TLBI_ITEM_INFO2 structure that contains information to retrieve
    //      and receives information about the button.
    //
    // Return Values
    //   Returns TRUE if successful, or FALSE otherwise.
    //
    // See Also
    //   InsertItem2, SetItemInfo2
    virtual BOOL WINAPI GetItemInfo2(DWORD position,
                                     BOOL byPosition,
                                     TLBI_ITEM_INFO2* tii) = 0;
};

//
// ****************************************************************************
// CGUIIconListAbstract
//
// Nas interni 32-bitovy image list. 8 bitu na kazdy RGB kanal a 8 bitu alfa transparence.

class CGUIIconListAbstract
{
public:
    // vytvori image list s rozmerem ikony 'imageWidth' x 'imageHeight' a poctem ikon
    // 'imageCount'; pomoci volani metody ReplaceIcon() je potom potreba image list naplnit;
    // vraci TRUE v pripade uspechu, jinak FALSE
    virtual BOOL WINAPI Create(int imageWidth, int imageHeight, int imageCount) = 0;

    // vytvori se na zaklade dodaneho windows image listu ('hIL'); 'requiredImageSize' urcuje
    // rozmer ikony, pokud je -1, pouziji se rozmery z 'hIL'; v pripade uspechu vraci TRUE,
    // jinak FALSE
    virtual BOOL WINAPI CreateFromImageList(HIMAGELIST hIL, int requiredImageSize) = 0;

    // vytvori se na zaklade dodaneho PNG resource; 'hInstance' a 'lpBitmapName' specifikuji resource,
    // 'imageWidth' udava sirku jedne ikony v bodech; v pripade uspechu vraci TRUE, jinak FALSE
    // poznamka: PNG musi byt pruh ikon jeden radek vysoky
    // poznamka: PNG je vhodne komprimovat pomoci PNGSlim, viz https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromPNG(HINSTANCE hInstance, LPCTSTR lpBitmapName, int imageWidth) = 0;

    // nahradi ikonu na danem indexu ikonou 'hIcon'; v pripade uspechu vraci TRUE, jinak FALSE
    virtual BOOL WINAPI ReplaceIcon(int index, HICON hIcon) = 0;

    // vytvori ikonu z daneho indexu a vrati jeji handle; volajici je zodpovedny za jeji destrukci
    // (volani DestroyIcon(hIcon)); v pripade neuspechu vraci NULL
    virtual HICON WINAPI GetIcon(int index) = 0;

    // vytvori se na zaklade PNG podaneho v pameti; 'rawPNG' je ukazatel na pamet obsahujici PNG
    // (napriklad nactene ze souboru) a 'rawPNGSize' urcuje velikost pameti obsazene PNG v bajtech,
    // 'imageWidth' udava sirku jedne ikony v bodech; v pripade uspechu vraci TRUE, jinak FALSE
    // poznamka: PNG musi byt pruh ikon jeden radek vysoky
    // poznamka: PNG je vhodne komprimovat pomoci PNGSlim, viz https://forum.altap.cz/viewtopic.php?f=15&t=3278
    virtual BOOL WINAPI CreateFromRawPNG(const void* rawPNG, DWORD rawPNGSize, int imageWidth) = 0;

    // vytvori se jako kopie jineho (vytvoreneho) icon listu; pokud je 'grayscale' TRUE,
    // provede se zaroven konverze na cernobilou verzi; v pripade uspechu vraci TRUE, jinak FALSE
    virtual BOOL WINAPI CreateAsCopy(const CGUIIconListAbstract* iconList, BOOL grayscale) = 0;

    // vytvori HIMAGELIST, vraci jeho handle nebo NULL v pripade neuspechu
    // vraceny imagelist je po pouziti treba destruovat pomoci API ImageList_Destroy()
    virtual HIMAGELIST WINAPI GetImageList() = 0;
};

//
// ****************************************************************************
// CGUIToolbarHeaderAbstract
//
// Pomocna lista umistena nad seznamem (napr. konfigurace HotPaths, UserMenu),
// ktera muze vpravo obsahovat toolbar s tlacitky pro ovladadni seznamu.
//
// Vsechny metody je mozne volat pouze z threadu okna, ve kterem
// byl objekt pripojen na windows control.
//

// Bitove masky pro EnableToolbar() a CheckToolbar()
#define TLBHDRMASK_MODIFY 0x01
#define TLBHDRMASK_NEW 0x02
#define TLBHDRMASK_DELETE 0x04
#define TLBHDRMASK_SORT 0x08
#define TLBHDRMASK_UP 0x10
#define TLBHDRMASK_DOWN 0x20

// Identifikace tlacitek pro WM_COMMAND, viz SetNotifyWindow()
#define TLBHDR_MODIFY 1
#define TLBHDR_NEW 2
#define TLBHDR_DELETE 3
#define TLBHDR_SORT 4
#define TLBHDR_UP 5
#define TLBHDR_DOWN 6
// Pocet polozek
#define TLBHDR_COUNT 6

class CGUIToolbarHeaderAbstract
{
public:
    // standardne jsou vsechna tlacitka povolena; po zavolani teto metody budou povolena
    // pouze tlacitka odpovidajici masce 'enableMask', ktera se sklada z jedne nebo vice
    // (sectenych) TLBHDRMASK_xxx hodnot
    virtual void WINAPI EnableToolbar(DWORD enableMask) = 0;

    // standardne jsou vsechna tlacitka nezamacknuta; po zavolani teto metody budou zamacknuta
    // tlacitka odpovidajici masce 'checkMask', ktera se sklada z jedne nebo vice
    // (sectenych) TLBHDRMASK_xxx hodnot
    virtual void WINAPI CheckToolbar(DWORD checkMask) = 0;

    // zavolanim teto metody volajici specifikuje okno 'hWnd', kteremu budou doruceny
    // WM_COMMAND od ToolbarHeader; LOWORD(wParam) bude obsahovat 'ctrlID' z volani
    // AttachToolbarHeader() a LOWORD(wParam) je jedna z hodnot TLBHDR_xxx (dle tlacitka,
    // na ktere uzivatel kliknul)
    // poznamka: tuto metodu je treba volat pouze ve specialnich situacich, kdy se zpravy
    // maji dorucovat do jineho nez parent okna, kam se zpravy dorucuji implicitne
    virtual void WINAPI SetNotifyWindow(HWND hWnd) = 0;
};

//
// ****************************************************************************
// CSalamanderGUIAbstract
//

#define BTF_CHECKBOX 0x00000001    // tlacitko se chova jako checkbox
#define BTF_DROPDOWN 0x00000002    // tlacitko vzadu obsahuje drop down cast, posila WM_USER_BUTTONDROPDOWN message do parenta
#define BTF_LBUTTONDOWN 0x00000004 // tlacitko reaguje na LBUTTONDOWN a posila WM_USER_BUTTON
#define BTF_RIGHTARROW 0x00000008  // tlacitko ma na konci sipku ukazujici vpravo
#define BTF_MORE 0x00000010        // tlacitko ma na konci symbol pro rozbaleni dialogu

class CSalamanderGUIAbstract
{
public:
    ///////////////////////////////////////////////////////////////////////////
    //
    // ProgressBar
    //
    // Slouzi k zobrazeni stavu operace v procentech uz vykonane prace.
    // Ma vyznam u operaci, ktere mohou zabrat vetsi mnostvi casu. Tam je
    // progress lepsi nez pouhy kurzor WAIT.
    //
    // pripoji Salamanderovsky progress bar na Windows control (tento control urcuje pozici
    // progress bary); 'hParent' je handle parent okna (dialog nebo okno); ctrlID je ID
    // Windows controlu; pri uspesnem pripojeni vraci rozhrani progress bary,
    // jinak vraci NULL; rozhrani je platne az do okamziku destrukce (doruceni WM_DESTROY)
    // Windows controlu; po pripojeni je progress bar nastaven na 0%;
    // frame si kresli ve sve rezimu, takze controlu neprirazovat flagy SS_WHITEFRAME | WS_BORDER
    virtual CGUIProgressBarAbstract* WINAPI AttachProgressBar(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // StaticText
    //
    // Slouzi pro zobrazeni nestandardnich textu v dialogu (tucny, podtrzeny),
    // textu ktere se rychle meni a blikaly by nebo textu s nepredvidatelnou delkou,
    // ktere je treba inteligentne zkratit.
    //
    // pripoji Salamanderovsky StaticText na Windows control (tento control urcuje pozici
    // StaticTextu); 'hParent' je handle parent okna (dialog nebo okno); ctrlID je ID;
    // 'flags' je z rodiny STF_*, muze byt 0 nebo libovolna kombinace hodnot;
    // Windows controlu; pri uspesnem pripojeni vraci rozhrani StaticTextu,
    // jinak vraci NULL; rozhrani je platne az do okamziku destrukce (doruceni WM_DESTROY)
    // Windows controlu; po pripojeni je vytazen text a zarovnani z Windows controlu.
    // testovano na Windows controlu "STATIC"
    virtual CGUIStaticTextAbstract* WINAPI AttachStaticText(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // HyperLink
    //
    // Slouzi pro zobrazeni hyperlinku. Po kliknuti je mozne otevrit URL adresu
    // nebo spustit soubor (SetActionOpen), nebo nechat postnout command zpet
    // do dialogu (SetActionPostCommand).
    // Control je pristupny pres klavesu TAB (muze mit focus), ale je treba mu
    // nastavit WS_TABSTOP. Akci pak vyvolame klavesou Space. Pravym tlacitkem
    // nebo pomoci Shift+F10 lze vyvolat menu s moznosti nakopirovat text controlu
    // na clipboard.
    //
    // pripoji Salamanderovsky HyperLink na Windows control (tento control urcuje pozici
    // HyperLinku); 'hParent' je handle parent okna (dialog nebo okno); ctrlID je ID Windows controlu;
    // 'flags' je z rodiny STF_*, muze byt 0 nebo libovolna kombinace hodnot;
    // doporucena kombinace pro HyperLink je STF_UNDERLINE | STF_HYPERLINK_COLOR
    // pri uspesnem pripojeni vraci rozhrani HyperLinku, jinak vraci NULL; rozhrani je
    // platne az do okamziku destrukce (doruceni WM_DESTROY) Windows controlu; po pripojeni
    // je vytazen text a zarovnani z Windows controlu.
    // testovano na Windows controlu "STATIC"
    virtual CGUIHyperLinkAbstract* WINAPI AttachHyperLink(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Button
    //
    // Slouzi pro vytvoreni tlacitka s textem nebo ikonou. Tlacitko muze obsahovat sipku
    // vpravo nebo drop-down sipku. Viz flagy BTF_xxx.
    //
    // pripoji Salamanderovsky TextArrowButton na Windows control (tento control urcuje pozici,
    // text nebo ikonu a generovany command); 'hParent' je handle parent okna (dialog nebo okno);
    // ctrlID je ID Windows controlu;
    // pri uspesnem pripojeni vraci rozhrani CGUIButtonAbstract, jinak vraci NULL; rozhrani je
    // platne az do okamziku destrukce (doruceni WM_DESTROY) Windows controlu;
    // Testovano na Windows controlu "BUTTON".
    virtual CGUIButtonAbstract* WINAPI AttachButton(HWND hParent, int ctrlID, DWORD flags) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ColorArrowButton
    //
    // Slouzi pro vytvoreni tlacitka s barevnym obdelnickem, ktery naselduje sipka smerujici vpravo.
    // (pokud je showArrow==TRUE)
    // V obdelnicku je zobrazen text, ktery muze mit prirazenou jinou barvu nez barva pozadi obdelnicku.
    // Pouziva se v konfiguracich barev, kde dokaze zobrazit jednu nebo dve barvy.
    // Po stisku je vybaleno popup menu s moznosti zvolit, kterou barvu konfigurujeme..
    //
    // pripoji Salamanderovsky ColorArrowButton na Windows control (tento control urcuje pozici,
    // text a command ColorArrowButtonu); 'hParent' je handle parent okna (dialog nebo okno);
    // ctrlID je ID Windows controlu;
    // pri uspesnem pripojeni vraci rozhrani ColorArrowButtonu, jinak vraci NULL; rozhrani je
    // platne az do okamziku destrukce (doruceni WM_DESTROY) Windows controlu;
    // Testovano na Windows controlu "BUTTON".
    virtual CGUIColorArrowButtonAbstract* WINAPI AttachColorArrowButton(HWND hParent, int ctrlID, BOOL showArrow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ChangeToArrowButton
    //
    // Slouzi pro vytvoreni tlacitka s sipkou smerujici vpravo umistene uprostred
    // tlacitka. Vklada se za vstupni pole a po stisku je vedle tlacitka vybaleno
    // popup menu obsahujici polozky vlozitelne do vstupniho pole (forma napovedy).
    //
    // Meni styl tlacitka, aby mohlo drzet ikonku s sipkou a potom tuto ikonku
    // prirazuje. Nepripojuje zadny Salamanderovsky objekt ke controlu, protoze
    // vse zvladne operacni system. Vraci TRUE v pripade uspechul, jinak FALSE.
    // Text tlacitka je ignorovan.
    // Testovano na Windows controlu "BUTTON".
    virtual BOOL WINAPI ChangeToArrowButton(HWND hParent, int ctrlID) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuPopup
    //
    // Slouzi pro vytvoreni prazdneho popup menu. Vraci ukazatel na iface nebo
    // NULL pri chybe.
    virtual CGUIMenuPopupAbstract* WINAPI CreateMenuPopup() = 0;
    // Slouzi pro uvolneni alokovaneho menu.
    virtual BOOL WINAPI DestroyMenuPopup(CGUIMenuPopupAbstract* popup) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // MenuBar
    //
    // Slouzi pro vytvoreni menu bar; polozky 'menu' budou zobrazeny v menu bar,
    // jejich childy budou submenu; 'hNotifyWindow' identifikuje okno, kteremu
    // budou zasilany commandy a notifikace. Vraci ukazatel na iface nebo NULL
    // pri chybe.
    virtual CGUIMenuBarAbstract* WINAPI CreateMenuBar(CGUIMenuPopupAbstract* menu, HWND hNotifyWindow) = 0;
    // Slouzi pro uvolneni alokovane menu bar. Zaroven destruuje okno.
    virtual BOOL WINAPI DestroyMenuBar(CGUIMenuBarAbstract* menuBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar support
    //
    // Slouzi pro vytvoreni neaktivni (sede) verze bitmap pro menu nebo toolbar.
    // Ze zdrojove bitmapy 'hSource' vytvori bitmapu ve stupnich sedi 'hGrayscale'
    // a cernobilou masku 'hMask'. Barva 'transparent' je povazovana za pruhlednou.
    // Pri uspechu vraci TRUE  a 'hGrayscale' a 'hMask'; pri chybe vraci FALSE.
    virtual BOOL WINAPI CreateGrayscaleAndMaskBitmaps(HBITMAP hSource, COLORREF transparent,
                                                      HBITMAP& hGrayscale, HBITMAP& hMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolBar
    //
    // Slouzi pro vytvoreni tool bar; 'hNotifyWindow' identifikuje okno, kteremu
    // budou zasilany commandy a notifikace. Vraci ukazatel na iface nebo NULL
    // pri chybe.
    virtual CGUIToolBarAbstract* WINAPI CreateToolBar(HWND hNotifyWindow) = 0;
    // Slouzi pro uvolneni alokovane tool bar. Zaroven destruuje okno.
    virtual BOOL WINAPI DestroyToolBar(CGUIToolBarAbstract* toolBar) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip
    //
    // Tato metoda spusti casovac a pokud do jeho vyprseni neni zavolana znovu
    // pozada okno 'hNotifyWindow' o text pomoci zpravy WM_USER_TTGETTEXT,
    // ktery pak zobrazi pod kurzor na jeho aktualnich souradnicich.
    // Promenna 'id' slouzi k rozliseni oblasti pri komunikaci s oknem 'hNotifyWindow'.
    // Pokud bude tato metoda zavolana vicekrat se stejnym parametrem 'id', budou
    // se tyto dalsi volani ignorovat.
    // Hodnota 0 parametru 'hNotifyWindow' je vyhrazena pro zhasnuti okna a preruseni
    // beziciho casovace.
    virtual void WINAPI SetCurrentToolTip(HWND hNotifyWindow, DWORD id) = 0;
    // potlaci zobrazeni tooltipu na aktualnich souradnicich mysi
    // uzitecne volat pri aktivaci okna, ve kterem se tooltipy pouzivaji
    // nebude tak dochazet k nechtenemu zobrazeni tooltipu
    virtual void WINAPI SuppressToolTipOnCurrentMousePos() = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // XP Visual Styles
    //
    // Pokud je zavolana pod operacnim system podporujicim vizualni styly,
    // vola SetWindowTheme(hWindow, L" ", L" ") pro zakazani vizualnich stylu
    // pro okno 'hWindow'
    // vraci TRUE, pokud operacni system podporuje vizualni styly, jinak vraci FALSE
    virtual BOOL WINAPI DisableWindowVisualStyles(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // IconList
    //
    // Dve metody pro alokaci a destrukci objektu IconList slouziciho pro drzeni
    // 32bpp ikonek (3 x 8 bitu pro barvu a 8 bitu pro alfa transparenci)
    // Dalsi operace nad IconListem viz popis CGUIIconListAbstract
    virtual CGUIIconListAbstract* WINAPI CreateIconList() = 0;
    virtual BOOL WINAPI DestroyIconList(CGUIIconListAbstract* iconList) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolTip support
    //
    // Prohleda 'buf' na prvni vyskyt znaku '\t'. Pokud je 'stripHotKey' TRUE, ukonci
    // se na tomto znaku retezec. Jinak se na jeho misto vlozi mezera a zbytek
    // textu se ozavorkuje. Buffer 'buf' musi byt pri 'stripHotKey' FALSE dost velky
    // na to, aby se text v bufferu mohl prodlouzit o dva znaky (uzavorkovani).
    virtual void WINAPI PrepareToolTipText(char* buf, BOOL stripHotKey) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // Subject with file/dir name truncated if needed
    //
    // Nastavi text vznikly jako sprintf(, subjectFormatString, fileName) do staticu 'subjectWnd'.
    // Formatovaci retezec 'subjectFormatString' musi obsahovat prave jedno '%s' (na miste vlozeni
    // 'fileName'). Pokud by text presahl delku staticu, dojde k jeho zkraceni tim, ze se zkrati
    // 'fileName'. Navic provede konverzi 'fileName' podle SALCFG_FILENAMEFORMAT (aby se shodoval
    // s tim, jak je 'fileName' zobrazeno v panelu) pomoci CSalamanderGeneralAbstract::AlterFileName.
    // Pokud jde o soubor, bude 'isDir' FALSE, jinak TRUE. Ma-li static 'subjectWnd' SS_NOPREFIX,
    // bude 'duplicateAmpersands' FALSE, jinak TRUE (zdvoji druhy a dalsi ampersandy ('&'), prvni
    // ampersand oznacuje hotkey v subjectu a musi ho obsahovat 'subjectFormatString' pred '%s').
    // Priklad pouziti: SetSubjectTruncatedText(GetDlgItem(HWindow, IDS_SUBJECT), "&Rename %s to",
    //                                          file->Name, fileIsDir, TRUE)
    // lze volat z libovolneho threadu
    virtual void WINAPI SetSubjectTruncatedText(HWND subjectWnd, const char* subjectFormatString, const char* fileName,
                                                BOOL isDir, BOOL duplicateAmpersands) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ToolbarHeader
    //
    // Slouzi pro vytvoreni hlavicky nad seznamem (at uz listview nebo listbox), ktera obsahuje
    // textovy popis a skupinu tlacitek na prave strane. Priklad lze videt v konfiguraci Salamandera,
    // viz Hot Paths nebo User Menu. 'hParent' je handle dialogu, 'ctrlID' je ID static textu,
    // kolem ktereho bude ToolbarHeader vytvoren, 'hAlignWindow' je handle seznamu, ke kteremu
    // bud hlavicka prisazena, 'buttonMask' je jedna nebo (soucet) vice hodnot TLBHDRMASK_xxx
    // a udava, ktera tlacitka budou v hlavicce zobrazeny.
    virtual CGUIToolbarHeaderAbstract* WINAPI AttachToolbarHeader(HWND hParent, int ctrlID, HWND hAlignWindow, DWORD buttonMask) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // ArrangeHorizontalLines
    //
    // Najde v dialogu 'hWindow' horizontalni cary a dorazi je zprava na static text
    // nebo checkbox nebo radiobox, na ktery navazuji. Navic najde checkboxy a
    // radioboxy, ktere tvori labely groupboxum a zkrati je podle jejich textu a
    // aktualniho pisma v dialogu. Eliminuje zbytecne mezery vznikle kvuli ruznym
    // DPI obrazovky.
    virtual void WINAPI ArrangeHorizontalLines(HWND hWindow) = 0;

    ///////////////////////////////////////////////////////////////////////////
    //
    // GetWindowFontHeight
    //
    // pro 'hWindow' ziska aktualni font pomoci WM_GETFONT a vrati jeho vysku
    // pomoci GetObject()
    virtual int WINAPI GetWindowFontHeight(HWND hWindow) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_gui)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
