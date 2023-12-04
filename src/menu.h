// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern const char* WC_POPUPMENU;

#define UPDOWN_ARROW_WIDTH 9
#define UPDOWN_ARROW_HEIGHT 5
#define UPDOWN_ITEM_HEIGHT 12

class CMenuSharedResources;
class CMenuPopup;
class CMenuBar;
class CBitmap;

/*
Posilane zpravy:
  WM_INITMENUPOPUP
    hmenuPopup = (HMENU) wParam;         // handle to submenu
    uPos = (UINT) LOWORD(lParam);        // submenu item position
    fSystemMenu = (BOOL) HIWORD(lParam); // window menu flag

    Tato zprava je posilana pouze v pripade windows menu popupu.

  WM_USER_INITMENUPOPUP
  WM_USER_UNINITMENUPOPUP
    menuPopup = (CGUIMenuPopupAbstract*) wParam; // ukazatel na submenu
    uPos =      LOWORD(lParam);                // submenu item position
    uID =       HIWORD(lParam);                // submenu ID

    Tyto dve zpravy jsou posilane vzdy - i pro menu postavene nad windows
    menu popup.
*/

//*****************************************************************************
//
// CMenuWindowQueue
//

class CMenuWindowQueue
{
private:
    TDirectArray<HWND> Data;
    CRITICAL_SECTION DataCriticalSection; // kriticka sekce pro pristup k datum
    BOOL UsingData;

public:
    CMenuWindowQueue();
    ~CMenuWindowQueue();

    BOOL Add(HWND hWindow);    // prida polozku do fronty, vraci uspech
    void Remove(HWND hWindow); // odstrani polozku z fronty
    void DispatchCloseMenu();  // posle vsem otevrenym oknum menu zpravu WM_USER_CLOSEMENU
};

extern CMenuWindowQueue MenuWindowQueue;

//*****************************************************************************
//
// COldMenuHookTlsAllocator
//

class COldMenuHookTlsAllocator
{
public:
    COldMenuHookTlsAllocator();
    ~COldMenuHookTlsAllocator();

    HHOOK HookThread();
    void UnhookThread(HHOOK hOldHookProc);
};

extern COldMenuHookTlsAllocator OldMenuHookTlsAllocator;

//*****************************************************************************
//
// CMenuSharedResources
//
// Pro jeden strom sub menu existuje pouze jedna instance techto prostredku.
// Jsou zalozeny napriklad ve funkci Track.
// Vsechny sub menu pak pouze dostavaji ukazatel na tyto sdilene prostredky.
//

class CMenuSharedResources
{
public:
    // barvy
    COLORREF NormalBkColor;
    COLORREF SelectedBkColor;
    COLORREF NormalTextColor;
    COLORREF SelectedTextColor;
    COLORREF HilightColor;
    COLORREF GrayTextColor;

    // cache DC
    CBitmap* CacheBitmap;
    CBitmap* MonoBitmap;

    // temp DC
    HDC HTempMemDC;  // memory dc pro docasne presuny
    HDC HTemp2MemDC; // memory dc pro docasne presuny

    // fonts
    HFONT HNormalFont; // prubezne je vybran v HCacheMemoryDC
    HFONT HBoldFont;   // je vybiran pouze docasne

    // menu bitmaps
    HBITMAP HMenuBitmaps; // vytazeno ze systemu: poradi dle CMenuBitmapEnum
    int MenuBitmapWidth;

    // other
    HWND HParent;          // okno, ze ktereho bylo menu vyvolano
    int TextItemHeight;    // vyska polozky z textu
    BOOL BitmapsZoom;      // nasobek puvodni velikost bitmap
    DWORD ChangeTickCount; // hodnota GetTickCount z doby, kdy doslo ke zmene vybrane polozky
    POINT LastMouseMove;
    CMenuBar* MenuBar; // okno je aktivovano z MenuBar; jinak je rovno NULL
    DWORD SkillLevel;  // honodta pro retezec popupu -- urcuje, kerer itemy budou zobrazeny
    BOOL HideAccel;    // maji se skryt akceleratory

    const RECT* ExcludeRect; // tento obdelnik nesmime prekryt

    HANDLE HCloseEvent; // slouzi pro rozbehnuti message queue

public:
    CMenuSharedResources();
    ~CMenuSharedResources();

    BOOL Create(HWND hParent, int width, int height);
};

//*****************************************************************************
//
// CMenuItem
//

class CMenuItem
{
protected:
    DWORD Type;
    DWORD State;
    DWORD ID;
    CMenuPopup* SubMenu;
    HBITMAP HBmpChecked;
    HBITMAP HBmpUnchecked;
    HBITMAP HBmpItem;
    char* String;
    int ImageIndex;
    HICON HIcon;
    HICON HOverlay;
    ULONG_PTR CustomData;
    DWORD SkillLevel; // MENU_LEVEL_BEGINNER, MENU_LEVEL_INTERMEDIATE, MENU_LEVEL_ADVANCED
    // tyto hodnoty se pouzivaji pro optimalizovany pristup je stavum polozek
    DWORD* Enabler; // Ukazuje na promennou, ktera ridi stav polozky.
                    // Hodnote ruzne od nuly odpovida nulovany bit MENU_STATE_GRAYED.
                    // Nule odpovida nastaveny bit MENU_STATE_GRAYED.
    DWORD Flags;    // MENU_FLAG_xxx
    DWORD Temp;     // pomocna promenna pro nektere metody

    // vypocitane hodnoty
    int Height;
    int MinWidth;
    int YOffset;

    const char* ColumnL1; // text prvniho sloupce
    int ColumnL1Len;      // pocet znaku
    int ColumnL1Width;
    int ColumnL1X;
    const char* ColumnL2; // text druheho sloupce (muze byt rovno NULL)
    int ColumnL2Len;      // pocet znaku
    int ColumnL2Width;
    int ColumnL2X;
    const char* ColumnR; // text praveho sloupce (muze byt rovno NULL)
    int ColumnRLen;      // pocet znaku
    int ColumnRWidth;
    int ColumnRX;

public:
    CMenuItem();
    ~CMenuItem();

    BOOL SetText(const char* text, int len = -1);

    // projede retezec TypeData a podle oddelovacu a promenne threeCol
    // nastavi promenne ColumnL1 - ColumnR, ColumnL1Len - ColumnRLen,
    // ColumnL1Width - ColumnRWidth
    void DecodeSubTextLenghtsAndWidths(CMenuSharedResources* sharedRes, BOOL threeCol);

    friend class CMenuPopup;
    friend class CMenuBar;
};

//*****************************************************************************
//
// CMenuPopup
//

enum CMenuBitmapEnum
{
    menuBitmapArrowR,
    //  menuBitmapArrowL,
    //  menuBitmapArrowU,
    //  menuBitmapArrowD
};

enum CMenuPopupHittestEnum
{
    mphItem,            // na polozce, userData = index polozky
    mphUpArrow,         // na sipce Up
    mphDownArrow,       // na sipce Down
    mphBorderOrOutside, // na ramecku nebo mimo
    //  mphOutside, // mimo okno
};

/*
Items
  Seznam polozek obsazenych v pop-up menu.

HParent
  Okno, kteremu se budou dorucovat notifikacni message.

HImageList
  Ikonky zobrazovane pred polozkama. Ikona je urcovana promennou
  CMenuItem::ImageIndex.

HWindowsMenu
  Handle windowsackeho popup menu. Pred otevrenim tohoto submenu
  se provede enumerace jeho polozek. Ty jsou pak transformovany do docasneho
  objetku CMenuPopup. Po zavreni tohoto submenu je docasny objekt zrusen.
  Pro takove menu jsou posilany notifikacni message WM_INITPOPUP, WM_DRAWITEM
  a WM_MEASUREITEM.
*/

class CMenuPopup : public CWindow, public CGUIMenuPopupAbstract
{
protected:
    TIndirectArray<CMenuItem> Items;
    HMENU HWindowsMenu;

    RECT WindowRect;
    int TotalHeight; // celkova vyska menu; nemusi byt cela zobrazena
    int Width;       // rozmery client area
    int Height;
    int TopItemY;           // y souradnice prvni polozky
    BOOL UpArrowVisible;    // je zobrazena sipka nahoru?
    BOOL UpDownTimerRunnig; // bezi nam timer?
    BOOL DownArrowVisible;  // je zobrazena sipka dolu?
    DWORD Style;            // MENU_POPUP_xxxx
    DWORD TrackFlags;       // MENU_TRACK_xxxx
    CMenuSharedResources* SharedRes;
    CMenuPopup* OpenedSubMenu; // je-li otevreny nejaky submenu, ukazuje na nej
    CMenuPopup* FirstPopup;    // pokud nejde o prvni okno, ukazuje na nej; v pripade prvniho okna ukazuje na sebe samo
    int SelectedItemIndex;     // -1 == zadna
    BOOL SelectedByMouse;      // TRUE->ByMouse FALSE->ByKeyboard
    HIMAGELIST HImageList;
    HIMAGELIST HHotImageList;
    int ImageWidth; // rozmery jednoho obrazku z HImageList
    int ImageHeight;
    DWORD ID;                  // kopie ID z CMenuItem
    BOOL Closing;              // bylo zavolano HideAll a hned jak bude mozne, koncime
    int MinWidth;              // pri rozvrhovani sirky nebude sirka mensi, nez tato hodnota
    BOOL ModifyMode;           // pokud je menu zobrazeno, neni mozne nad nim provadet zmeny, neni-li v ModifyMode
    DWORD SkillLevel;          // urcuje, ktere polozky budou v tomto popupu zobrazeny
    int MouseWheelAccumulator; // vertical

public:
    //
    // Vlastni metody
    //

    CMenuPopup(DWORD id = 0);
    BOOL LoadFromTemplate2(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList, HIMAGELIST hHotImageList, int* addedRows);

    //
    // Implementace metod CGUIMenuPopupAbstract
    //

    virtual BOOL WINAPI LoadFromTemplate(HINSTANCE hInstance, const MENU_TEMPLATE_ITEM* menuTemplate, DWORD* enablersOffset, HIMAGELIST hImageList = NULL, HIMAGELIST hHotImageList = NULL);

    virtual void WINAPI SetSelectedItemIndex(int index); // slouzi k prednastaveni vybrane polozky (musi byt nastaven flag MENU_TRACK_SELECT, jinak se nepouzije)
    virtual int WINAPI GetSelectedItemIndex() { return SelectedItemIndex; }

    virtual void WINAPI SetTemplateMenu(HMENU hWindowsMenu) { HWindowsMenu = hWindowsMenu; }
    virtual HMENU WINAPI GetTemplateMenu() { return HWindowsMenu; }

    virtual CGUIMenuPopupAbstract* WINAPI GetSubMenu(DWORD position, BOOL byPosition);

    // The InsertItem method inserts a new menu item into a menu, moving other items
    // down the menu.
    //
    // Parameters:
    //
    // 'position'     [in] Identifier or position of the menu item before which to insert
    //                the new item. The meaning of this parameter depends on the
    //                value of 'byPosition'.
    //
    // 'byPosition'   [in] Value specifying the meaning of 'position'. If this parameter is FALSE,
    //                'position' is a menu item identifier. Otherwise, it is a menu item position.
    //                If 'byPosition' is TRUE and 'position' is -1, the new menu item is appended
    //                to the end of the menu.
    //
    // 'mii'          [in] Pointer to a MENU_ITEM_INFO structure that contains information about
    //                the new menu item.
    virtual BOOL WINAPI InsertItem(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii);

    virtual BOOL WINAPI SetItemInfo(DWORD position, BOOL byPosition, const MENU_ITEM_INFO* mii);
    virtual BOOL WINAPI GetItemInfo(DWORD position, BOOL byPosition, MENU_ITEM_INFO* mii);
    virtual BOOL WINAPI SetStyle(DWORD style); // rodina MENU_POPUP_xxxxx
    virtual BOOL WINAPI CheckItem(DWORD position, BOOL byPosition, BOOL checked);
    virtual BOOL WINAPI CheckRadioItem(DWORD positionFirst, DWORD positionLast, DWORD positionCheck, BOOL byPosition);
    virtual BOOL WINAPI SetDefaultItem(DWORD position, BOOL byPosition);
    virtual BOOL WINAPI EnableItem(DWORD position, BOOL byPosition, BOOL enabled);
    virtual int WINAPI GetItemCount() { return Items.Count; }

    virtual void WINAPI RemoveAllItems();
    virtual BOOL WINAPI RemoveItemsRange(int firstIndex, int lastIndex);

    // umoznuje provadet zmeny nad otevrenym menu popupem
    virtual BOOL WINAPI BeginModifyMode(); // zahajeni editacniho rezimu
    virtual BOOL WINAPI EndModifyMode();   // ukonceni rezimu - menu se prekresli

    // urci polozky, kter budou v menu zobrazeny
    // 'skillLevel' muze byt jedna z hodnot MENU_LEVEL_BEGINNER, MENU_LEVEL_INTERMEDIATE a MENU_LEVEL_ADVANCED
    virtual void WINAPI SetSkillLevel(DWORD skillLevel);

    // The FindItemPosition method finds a menu item position.
    //
    // Parameters:
    //
    // 'id'           [in] Identifier of the menu item
    //
    // Return Values:
    //
    // If the method succeeds, the return value is zero base index of the menu item.
    //
    // If menu item is not found, return value is -1.
    virtual int WINAPI FindItemPosition(DWORD id);

    virtual BOOL WINAPI FillMenuHandle(HMENU hMenu);
    virtual BOOL WINAPI GetStatesFromHWindowsMenu(HMENU hMenu);
    virtual void WINAPI SetImageList(HIMAGELIST hImageList, BOOL subMenu = FALSE); // pokud je subMenu==TRUE, nastavi se hadle i do submenu
    virtual HIMAGELIST WINAPI GetImageList();
    virtual void WINAPI SetHotImageList(HIMAGELIST hHotImageList, BOOL subMenu = FALSE);
    virtual HIMAGELIST WINAPI GetHotImageList();

    // The TrackPopupMenuEx function displays a shortcut menu at the specified location
    // and tracks the selection of items on the shortcut menu. The shortcut menu can
    // appear anywhere on the screen.
    //
    // Parameters:
    //
    // 'trackFlags'   [in] Use one of the following flags to specify how the function
    //                positions the shortcut menu horizontally: MENU_TRACK_xxxx
    //
    // 'x'            [in] Horizontal location of the shortcut menu, in screen coordinates.
    //
    // 'y'            [in] Vertical location of the shortcut menu, in screen coordinates.
    //
    // 'hwnd'         [in] Handle to the window that owns the shortcut menu. This window
    //                receives all messages from the menu. The window does not receive a
    //                WM_COMMAND message from the menu until the function returns.
    //                If you specify TPM_NONOTIFY in the fuFlags parameter, the function
    //                does not send messages to the window identified by hwnd. However,
    //                you still have to pass a window handle in hwnd. It can be any window
    //                handle from your application.
    //
    // 'exclude'      [in] Rectangle to exclude when positioning the menu, in screen
    //                coordinates. This parameter can be NULL.
    //
    // Return Values:
    //   If you specify TPM_RETURNCMD in the 'flags' parameter, the return value is the
    //   menu-item identifier of the item that the user selected. If the user cancels
    //   the menu without making a selection, or if an error occurs, then the return
    //   value is zero.
    //
    //   If you do not specify TPM_RETURNCMD in the 'flags' parameter, the return value
    //   is nonzero if the function succeeds and zero if it fails.
    virtual DWORD WINAPI Track(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude);

    virtual BOOL WINAPI GetItemRect(int index, RECT* rect); // vrati obsany obdelnik kolem polozky v screen souradnicich

    // obehne vsechny polozky a pokud maji nastaveny ukazatel 'EnablerData'
    // porovnaja hodnotu (na kterou ukazuji) se skutecnym stavem polozky.
    // Pokud se stav lisi, zmeni ho.
    virtual void WINAPI UpdateItemsState();

    virtual void WINAPI SetMinWidth(int minWidth);

    virtual void WINAPI SetPopupID(DWORD id);
    virtual DWORD WINAPI GetPopupID();
    virtual void WINAPI AssignHotKeys();

protected:
    void Cleanup(); // inicializuje objekt
    BOOL LoadFromHandle();
    void LayoutColumns(); // probehne polozky a podle jejich rozmeru nastavi hodnoty
    DWORD GetOwnerDrawItemState(const CMenuItem* item, BOOL selected);
    void DrawCheckBitmapVista(HDC hDC, CMenuItem* item, int yOffset, BOOL selected); // veze s alpha blendem
    void DrawCheckBitmap(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);      // check marky dodane uzivatelem (HBmpChecked a HBmpUnchecked)
    void DrawCheckImage(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);       // standardni checkmarky, ImageIndex, HIcon
    void DrawCheckMark(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);        // vola odpovidajici funkci
    void DrawItem(HDC hDC, CMenuItem* item, int yOffset, BOOL selected);             // vykresli jednu polozku
    void DrawUpDownItem(HDC hDC, BOOL up);                                           // vykresli polozku obsahujici sipku nahoru nebo dolu
    CMenuPopupHittestEnum HitTest(const POINT* point, int* userData);

    BOOL FindNextItemIndex(int fromIndex, BOOL topToDown, int* index);
    inline CMenuPopup* FindActivePopup();       // najde posledni otevreny popup; vrati ukazatel na objekt
    inline CMenuPopup* FindPopup(HWND hWindow); // hleda od nas az po posledniho childa; vrati ukazatel na objekt nnebo NULL
    inline void DoDispatchMessage(MSG* msg, BOOL* leaveMenu, DWORD* retValue, BOOL* dispatchLater);
    void OnTimerTimeout();
    void CheckSelectedPath(CMenuPopup* terminator); // probehne celou vetev a nastavi SelectedItemy tak, aby vedly k poslednimu popupu

    // k Track pridava [in] menuBar
    //                 [in] delayedMsg
    //                 [in] dispatchDelayedMsg: Ma se delayedMsg dorucit po navratu z teto metodu?
    //
    DWORD TrackInternal(DWORD trackFlags, int x, int y, HWND hwnd, const RECT* exclude,
                        CMenuBar* menuBar, MSG& delayedMsg, BOOL& dispatchDelayedMsg);

    void CloseOpenedSubmenu();
    void HideAll();

    void PaintAllItems(HRGN hUpdateRgn);

    void OnKeyRight(BOOL* leaveMenu);
    void OnKeyReturn(BOOL* leaveMenu, DWORD* retValue);
    void OnChar(char key, BOOL* leaveMenu, DWORD* retValue);
    int FindNextItemIndex(int firstIndex, char key);

    // pro navigaci pomoci PgDn/PgUp, hleda index prvni polozky
    // za separatorem; pokud je 'down' TRUE, hleda smerem dolu
    // od polozky 'firstIndex', jinak smerem nahoru
    int FindGroupIndex(int firstIndex, BOOL down);

    // pokud je 'byMouse' TRUE, jde o zmenu pomoci mysi, jinak jde o zmenu z klavesnice
    // select nastaveny z klavesnice "drzi", pokud uzivatel pohybuje mysi mimo popupy
    void SelectNewItemIndex(int newItemIndex, BOOL byMouse);

    void EnsureItemVisible(int index); // pokud polozka lezi mimo zobrazenou oblast, zajisti
                                       // odrolovani a vykresleni polozek tak, aby byla cela
                                       // viditelna

    void OnMouseWheel(WPARAM wParam, LPARAM lParam);

    // x, y jsou souradnice leveho horniho rohu okna
    // submenuItemPos slouzi k zaslani notifikace aplikaci
    BOOL CreatePopupWindow(CMenuPopup* firstPopup, int x, int y, int submenuItemPos, const RECT* exclude);

    // Vrati handle popup window pod kurzorem; pokud je pod kurzorem child window,
    // bude dohledan jeho parent.
    //
    // Zavedeno kvuli PicaView, ktere vklada do kontextoveho menu child window,
    // do ktereho renderuje se spozdenim obrazek. V Salamu 2.0 pri najeti na takovy
    // obrazek doslo k odvybrani polozky v menu, protoze WindowFromPoint vratilo
    // jine okno nez popup.
    HWND PopupWindowFromPoint(POINT point);

    void ResetMouseWheelAccumulator() { MouseWheelAccumulator = 0; }

    LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    friend class CMenuItem;
    friend class CMenuBar;
};

//*****************************************************************************
//
// CMenuBar
//

class CMenuBar : public CWindow, public CGUIMenuBarAbstract
{
protected:
    CMenuPopup* Menu;
    int Width; // rozmery celeho okna
    int Height;
    HFONT HFont;
    int FontHeight;
    int HotIndex;       // polozka, ktera je bud vysunuta nebo zamackla (zadna = -1)
    HWND HNotifyWindow; // kam budeme dorucovat notifikace
    BOOL MenuLoop;      // jsou rozbalovana submenu
    DWORD RetValue;     // ktery command mame poslat do okna aplikace
    MSG DelayedMsg;
    BOOL DispatchDelayedMsg;
    BOOL HotIndexIsTracked; // je otevren popup pod HotIndex?
    BOOL HandlingVK_MENU;
    BOOL WheelDuringMenu;
    POINT LastMouseMove;
    BOOL Closing;        // bylo zavolano WM_USER_CLOSEMENU hned jak bude mozne, koncime
    HANDLE HCloseEvent;  // slouzi pro rozbehnuti message queue
    BOOL MouseIsTracked; // je mys sledovana pomoci TrackMouseEvent?
    BOOL HelpMode;       // jsme v rezimu Context Help (Shift+F1)?

    // tyto dve promenne slouzi pro kooperaci MenuBar a MenuPopup
    // jsou nastavovany v CMenuPopup::TrackInternal a urcuji dalsi chovani
    // MenuBar po zavreni Popupu
    int IndexToOpen;     // pokud bude nastavena na -1, nema se otevrit dalsi Popup,
                         // jinak obsahuje index popupu, ktery se ma otevrit
    BOOL OpenWithSelect; // ma se v otevrenem menu vybrat prvni polozka?
    BOOL OpenByMouse;    // otevirano pomoci mysi nebo klavesnice?
    BOOL ExitMenuLoop;   // Pokud je TRUE, ukoncime MenuLoop
    BOOL HelpMode2;      // obdrzeli jmse WM_USER_HELP_MOUSEMOVE a cekame na WM_USER_HELP_MOUSELEAVE? (musime vysvecovat polozku pod kurzorem)
    WORD UIState;        // zobrazovani akceleratoru
    BOOL ForceAccelVisible;

public:
    //
    // Vlastni metody
    //
    CMenuBar(CMenuPopup* menu, HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CMenuBar();

    //
    // Implementace metod CGUIMenuBarAbstract
    //

    virtual BOOL WINAPI CreateWnd(HWND hParent);
    virtual HWND WINAPI GetHWND() { return HWindow; }

    virtual int WINAPI GetNeededWidth();                 // vrati sirku, ktera bude pro okno potreba
    virtual int WINAPI GetNeededHeight();                // vrati vysku, ktera bude pro okno potreba
    virtual void WINAPI SetFont();                       // vytahne font pro menu bar ze systemu
    virtual BOOL WINAPI GetItemRect(int index, RECT& r); // vrati umisteni polozky ve screen souradnicich
    virtual void WINAPI EnterMenu();                     // user stisknul VK_MENU
    virtual BOOL WINAPI IsInMenuLoop() { return MenuLoop; }
    virtual void WINAPI SetHelpMode(BOOL helpMode) { HelpMode = helpMode; }

    // If the message is translated, the return value is TRUE.
    virtual BOOL WINAPI IsMenuBarMessage(CONST MSG* lpMsg);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawItem(int index);
    void DrawItem(HDC hDC, int index, int x);
    void DrawAllItems(HDC hDC);
    void RefreshMinWidths(); // obehne vsechny polozky a napocit si k nim 'MinWidth'

    void TrackHotIndex();                                                  // zamackne HotIndex a zavola TrackPopup; vrati se po jeho zavreni
    void EnterMenuInternal(int index, BOOL openWidthSelect, BOOL byMouse); // byMouse rika, zda jde o otevreni pres mys nebo klavesnici

    // vraci TRUE, pokud je na pozici polozka; pak take nastavi 'index'
    // jinak vraci FALSE
    BOOL HitTest(int xPos, int yPos, int& index);

    // prohleda vlozene submenu a vrati TRUE, pokud mezi nima najde nejaky s horkou
    // klavesou 'hotKey'; zaroven vrati jeho index
    BOOL HotKeyIndexLookup(char hotKey, int& itemIndex);

    friend class CMenuPopup;
};

BOOL InitializeMenu();
void ReleaseMenu();

extern CMenuPopup MainMenu;
extern CMenuPopup ArchiveMenu;
extern CMenuPopup ArchivePanelMenu;

BOOL BuildSalamanderMenus();           // sestavi globalni menu pro Salamandera
BOOL BuildFindMenu(CMenuPopup* popup); // sestavi instanci menu pro find

// Prida do 'popup' polozky vytvorene na zaklade pole 'buttonsID'.
// 'hWindow' je parent tlacitek, na ktere odkazuji konstanty pole 'buttonsID'.
// Pole 'buttonsID' muze obsahovat libovolne mnozstvi cisel zakoncene 0.
// Cislo -1 je vyhrazene pro separator a cislo -2 pro default polozku (nasledujici
// polozka bude mit nastaven default state). Ostatni cisla jsou povazovana
// za id tlacitek. Jejich text je vytazen a pridan do menu. Zaroven je
// vytazen jejich Enabled stav, ktery je take promitnut do polozky v menu.
void FillContextMenuFromButtons(CMenuPopup* popup, HWND hWindow, int* buttonsID);
