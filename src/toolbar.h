// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************
//
// CToolBarItem
//

class CToolBar;
class CTBCustomizeDialog;

class CToolBarItem
{
protected:
    DWORD Style;    // TLBI_STYLE_xxx
    DWORD State;    // TLBI_STATE_xxx
    DWORD ID;       // command id
    char* Text;     // allocated string
    int TextLen;    // length of string
    int ImageIndex; // Image index of the item. Set this member to -1 to
                    // indicate that the button does not have an image.
                    // The button layout will not include any space for
                    // a bitmap, only text.
    HICON HIcon;
    HICON HOverlay;
    DWORD CustomData; // FIXME_X64 - male pro ukazatel, neni nekdy potreba?
    int Width;        // width of item (computed if TLBI_STYLE_AUTOSIZE is set)

    char* Name; // name in customize dialog (valid during custimize session)

    // tyto hodnoty se pouzivaji pro optimalizovany pristup ke stavum polozek
    DWORD* Enabler; // Ukazuje na promennou, ktera ridi stav polozky.
                    // Hodnote ruzne od nuly odpovida nulovany bit TLBI_STATE_GRAYED.
                    // Nule odpovida nastaveny bit TLBI_STATE_GRAYED.

    // internal data
    int Height; // height of item
    int Offset; // position of item in whole toolbar

    WORD IconX; // umisteni jednotlivych prvku
    WORD TextX;
    WORD InnerX;
    WORD OutterX;

public:
    CToolBarItem();
    ~CToolBarItem();

    BOOL SetText(const char* text, int len = -1);

    friend class CToolBar;
    friend class CTBCustomizeDialog;
};

//*****************************************************************************
//
// CToolBar
//

class CBitmap;

class CToolBar : public CWindow, public CGUIToolBarAbstract
{
protected:
    TIndirectArray<CToolBarItem> Items;

    int Width; // rozmery celeho okna
    int Height;
    HFONT HFont;
    int FontHeight;
    HWND HNotifyWindow; // kam budeme dorucovat notifikace
    HIMAGELIST HImageList;
    HIMAGELIST HHotImageList;
    int ImageWidth; // rozmery jednoho obrazku z imagelistu
    int ImageHeight;
    DWORD Style;          // TLB_STYLE_xxx
    BOOL DirtyItems;      // nastala operace, ktera ovlivnuje rozlozeni polozek
                          // a je treba provest prepocet
    CBitmap* CacheBitmap; // pres tuto bitmapu kreslime ven
    CBitmap* MonoBitmap;  // pro grayed ikonky
    int CacheWidth;       // rozmery bitmapy
    int CacheHeight;
    int HotIndex; // -1 = zadny
    int DownIndex;
    BOOL DropPressed;
    BOOL MonitorCapture;
    BOOL RelayToolTip;
    TOOLBAR_PADDING Padding;
    BOOL HasIcon;       // pokud drzi nejakou ikonu, bude funkce GetNeededSpace() pocitat s jeji vyskou
    BOOL HasIconDirty;  // je potreba detekovat pritomnost ikony pro GetNeededSpace()?
    BOOL Customizing;   // je prave toolbara konfigurovana
    int InserMarkIndex; // -1 = zadny
    BOOL InserMarkAfter;
    BOOL MouseIsTracked;  // je mys sledovana pomoci TrackMouseEvent?
    DWORD DropDownUpTime; // cas v [ms], kdy byl odmackunt drop down, pro ochranu pred novym zamacknutim
    BOOL HelpMode;        // Salamander je Shift+F1 (ctx help) rezimu a toolbar by mel vysvitit i disabled polozky pod kurzorem

public:
    //
    // Vlastni metody
    //
    CToolBar(HWND hNotifyWindow, CObjectOrigin origin = ooAllocated);
    ~CToolBar();

    //
    // Implementace metod CGUIToolBarAbstract
    //

    virtual BOOL WINAPI CreateWnd(HWND hParent);
    virtual HWND WINAPI GetHWND() { return HWindow; }

    virtual int WINAPI GetNeededWidth(); // vrati rozmery, ktere budou pro okno potreba
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI SetFont();
    virtual BOOL WINAPI GetItemRect(int index, RECT& r); // vrati umisteni polozky ve screen souradnicich

    virtual BOOL WINAPI CheckItem(DWORD position, BOOL byPosition, BOOL checked);
    virtual BOOL WINAPI EnableItem(DWORD position, BOOL byPosition, BOOL enabled);

    // pokud je prirazen image list, vlozi do nej na odpovidajici misto ikonu
    // promenne normal a hot urcuji, ktere imagelisty budou ovlivneny
    virtual BOOL WINAPI ReplaceImage(DWORD position, BOOL byPosition, HICON hIcon, BOOL normal = TRUE, BOOL hot = FALSE);

    virtual int WINAPI FindItemPosition(DWORD id);

    virtual void WINAPI SetImageList(HIMAGELIST hImageList);
    virtual HIMAGELIST WINAPI GetImageList();

    virtual void WINAPI SetHotImageList(HIMAGELIST hImageList);
    virtual HIMAGELIST WINAPI GetHotImageList();

    // styl toolbary
    virtual void WINAPI SetStyle(DWORD style);
    virtual DWORD WINAPI GetStyle();

    virtual BOOL WINAPI RemoveItem(DWORD position, BOOL byPosition);
    virtual void WINAPI RemoveAllItems();

    virtual int WINAPI GetItemCount() { return Items.Count; }

    // vyvola konfiguracni dialog
    virtual void WINAPI Customize();

    virtual void WINAPI SetPadding(const TOOLBAR_PADDING* padding);
    virtual void WINAPI GetPadding(TOOLBAR_PADDING* padding);

    // obehne vsechny polozky a pokud maji nastaveny ukazatel 'EnablerData'
    // porovna hodnoty (na kterou ukazuje) se skutecnym stavem polozky.
    // Pokud se stav lisi, zmeni ho.
    virtual void WINAPI UpdateItemsState();

    // pokud je bod nad nekterou z polozek (ne nad separatororem), vrati jeji index.
    // jinak vrati zaporne cislo
    virtual int WINAPI HitTest(int xPos, int yPos);

    // vraci TRUE, pokud je na pozice na rozhrani polozky; pak take nastavi 'index'
    // na tuto polozku a promennout 'after', ktera udava, jestli jde o levou nebo
    // pravou stranu polozky. Pokud je pod nad nejakou polozkou, vrati FALSE.
    // Pokud bod nelezi nad zadnou plozkou, vrati TRUE a 'index' nastavi na -1.
    virtual BOOL WINAPI InsertMarkHitTest(int xPos, int yPos, int& index, BOOL& after);

    // nastavi InsertMark na pozici index (pred nebo za)
    // pokud je index == -1, odstrani InsertMark
    virtual void WINAPI SetInsertMark(int index, BOOL after);

    // Sets the hot item in a toolbar. Returns the index of the previous hot item, or -1 if there was no hot item.
    virtual int WINAPI SetHotItem(int index);

    // mohlo dojit ke zmene barevne hloubky obrazovky; je treba prebuildit CacheBitmap
    virtual void WINAPI OnColorsChanged();

    virtual BOOL WINAPI InsertItem2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii);
    virtual BOOL WINAPI SetItemInfo2(DWORD position, BOOL byPosition, const TLBI_ITEM_INFO2* tii);
    virtual BOOL WINAPI GetItemInfo2(DWORD position, BOOL byPosition, TLBI_ITEM_INFO2* tii);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DrawDropDown(HDC hDC, int x, int y, BOOL grayed);
    void DrawItem(int index);
    void DrawItem(HDC hDC, int index);
    void DrawAllItems(HDC hDC);

    void DrawInsertMark(HDC hDC);

    // vraci TRUE, pokud je na pozici polozka; pak take nastavi 'index'
    // jinak vraci FALSE
    // pokud uzivatel kliknul na drop down, nastavi 'dropDown' na TRUE
    BOOL HitTest(int xPos, int yPos, int& index, BOOL& dropDown);

    // obehne vsechny polozky a napocit si k nim 'MinWidth' a 'XOffset'
    // ridi se (a nastavuje) DirtyItems
    // vrati TRUE, pokud doslo k prekresleni vsech polozek
    BOOL Refresh();

    friend class CTBCustomizeDialog;
};

//*****************************************************************************
//
// CTBCustomizeDialog
//

class CTBCustomizeDialog : public CCommonDialog
{
    enum TBCDDragMode
    {
        tbcdDragNone,
        tbcdDragAvailable,
        tbcdDragCurrent,
    };

protected:
    TDirectArray<TLBI_ITEM_INFO2> AllItems; // vsechny dostupne polozky
    CToolBar* ToolBar;
    HWND HAvailableLB;
    HWND HCurrentLB;
    DWORD DragNotify;
    TBCDDragMode DragMode;
    int DragIndex;

public:
    CTBCustomizeDialog(CToolBar* toolBar);
    ~CTBCustomizeDialog();
    BOOL Execute();

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void DestroyItems();
    BOOL EnumButtons(); // pomoci notifikace WM_USER_TBENUMBUTTON2 naplni pole Items vsema tlacitkama, ktere muze toolbar drzet

    BOOL PresentInToolBar(DWORD id);      // je tento command v toolbare ?
    BOOL FindIndex(DWORD id, int* index); // vyhleda command v AllItems
    void FillLists();                     // naplni oba listboxy

    void EnableControls();
    void MoveItem(int srcIndex, int tgtIndex);
    void OnAdd();
    void OnRemove();
    void OnUp();
    void OnDown();
    void OnReset();
};

//*****************************************************************************
//
// CMainToolBar
//
// Toolbar, ktery jde konfigurovat, nese tlacitka s commandy. Lezi na vrsku
// Salama a nad kazdym panelem.
//

enum CMainToolBarType
{
    mtbtTop,
    mtbtMiddle,
    mtbtLeft,
    mtbtRight,
};

class CMainToolBar : public CToolBar
{
protected:
    CMainToolBarType Type;

public:
    CMainToolBar(HWND hNotifyWindow, CMainToolBarType type, CObjectOrigin origin = ooStatic);

    BOOL Load(const char* data);
    BOOL Save(char* data);

    // je treba vratit tooltip
    void OnGetToolTip(LPARAM lParam);
    // pri konfiguraci plni konfiguracni dialog polozkama
    BOOL OnEnumButton(LPARAM lParam);
    // uzivatel stisknul reset v konfiguracnim dialogu - nalejeme default sestavu
    void OnReset();

    void SetType(CMainToolBarType type);

protected:
    // do 'tii' naplni data pro poozku 'tbbeIndex' a vrati TRUE
    // pokud polozka neni uplna (zruseny prikaz), vrati FALSE
    BOOL FillTII(int tbbeIndex, TLBI_ITEM_INFO2* tii, BOOL fillName); // 'buttonIndex' je z rodiny TBBE_xxxx; -1 = separator
};

//*****************************************************************************
//
// CBottomToolBar
//
// toolbar ve spodni casti Salamandera - obsahuje napovedu pro F1-F12 v
// kombinaci s Ctrl, Alt a Shift
//

enum CBottomTBStateEnum
{
    btbsNormal,
    btbsAlt,
    btbsCtrl,
    btbsShift,
    btbsCtrlShift,
    //  btbsCtrlAlt,
    btbsAltShift,
    //  btbsCtrlAltShift,
    btbsMenu,
    btbsCount
};

class CBottomToolBar : public CToolBar
{
public:
    CBottomToolBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    virtual BOOL WINAPI CreateWnd(HWND hParent);

    // vola se pri kazde zmene modifikatoru (Ctrl,Alt,Shift) - obehne naplnenou
    // toolbaru a nastavi ji texty a ID
    BOOL SetState(CBottomTBStateEnum state);

    // inicializuje staticke pole, ze ktereho pak budeme toolbaru krmit
    static BOOL InitDataFromResources();

    void OnGetToolTip(LPARAM lParam);

    virtual void WINAPI SetFont();

protected:
    CBottomTBStateEnum State;

    // interni funkce volana z InitDataFromResources
    static BOOL InitDataResRow(CBottomTBStateEnum state, int textResID);

    // pro kazde tlacitko najde nedelsi text a podle nej nastavi tlacitku sirku
    BOOL SetMaxItemWidths();
};

//*****************************************************************************
//
// CUserMenuBar
//

class CUserMenuBar : public CToolBar
{
public:
    CUserMenuBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    // vytahne z UserMenu polozky a naleje buttony do toolbary
    BOOL CreateButtons();

    void ToggleLabels();
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    virtual void WINAPI SetInsertMark(int index, BOOL after);
    virtual int WINAPI SetHotItem(int index);

    void OnGetToolTip(LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CHotPathsBar
//

class CHotPathsBar : public CToolBar
{
public:
    CHotPathsBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);

    // vytahne z HotPaths polozky a naleje buttony do toolbary
    BOOL CreateButtons();

    void ToggleLabels();
    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    //    void SetInsertMark(int index, BOOL after);
    //    int SetHotItem(int index);

    void OnGetToolTip(LPARAM lParam);

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CDriveBar
//

class CDrivesList;

class CDriveBar : public CToolBar
{
protected:
    // navratove hodnoty pro List
    DWORD DriveType;
    DWORD_PTR DriveTypeParam;
    int PostCmd;
    void* PostCmdParam;
    BOOL FromContextMenu;
    CDrivesList* List;

    // cache: obsahuje ?: nebo \\ pro UNC nebo prazdny retezec
    char CheckedDrive[3];

public:
    // ikony pluginu chceme zobrazovat cernobile, takze je musime drzet v image listech
    HIMAGELIST HDrivesIcons;
    HIMAGELIST HDrivesIconsGray;

public:
    CDriveBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CDriveBar();

    void DestroyImageLists();

    // vyhaze existujici a naleje nova tlacitka;
    // neni-li 'copyDrivesListFrom' NULL, maji se data o discich kopirovat misto znovu ziskavat
    // 'copyDrivesListFrom' muze odkazovat i na volany objekt
    BOOL CreateDriveButtons(CDriveBar* copyDrivesListFrom);

    virtual int WINAPI GetNeededHeight();

    void OnGetToolTip(LPARAM lParam);

    // user clicknul na tlacitku s commandem id
    void Execute(DWORD id);

    // zamackne ikonku odpovidajici ceste; pokud takovou nenalezne, nebude
    // zamackla zadna; promenna force vyradi cache
    void SetCheckedDrive(CFilesWindow* panel, BOOL force = FALSE);

    // pokud prijde notifikace o pridani/odstraneni disku, je treba znovu naplnit seznam;
    // neni-li 'copyDrivesListFrom' NULL, maji se data o discich kopirovat misto znovu ziskavat
    // 'copyDrivesListFrom' muze odkazovat i na volany objekt
    void RebuildDrives(CDriveBar* copyDrivesListFrom = NULL);

    // je treba vybalit context menu; poozka se urci z GetMessagePos; vrati TRUE,
    // pokud bylo trefeno tlacitko a vybalilo se menu; jinak vrati FALSE
    BOOL OnContextMenu();

    // vraci bitove pole disku, jak bylo ziskano pri poslednim List->BuildData()
    // pokud BuildData() jeste neprobehlo, vraci 0
    // lze pouzit pro rychlou detekci, zda nedoslo k nejake zmene disku
    DWORD GetCachedDrivesMask();

    // vraci bitove pole dostupnych cloud storages, jak bylo ziskano pri poslednim List->BuildData()
    // pokud BuildData() jeste neprobehlo, vraci 0
    // lze pouzit pro rychlou detekci, zda nedoslo k nejake zmene dostupnosti cloud storages
    DWORD GetCachedCloudStoragesMask();

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

//*****************************************************************************
//
// CPluginsBar
//

class CPluginsBar : public CToolBar
{
protected:
    // ikonky reprezentujici pluginy, vyvorene pomoci CPlugins::CreateIconsList
    HIMAGELIST HPluginsIcons;
    HIMAGELIST HPluginsIconsGray;

public:
    CPluginsBar(HWND hNotifyWindow, CObjectOrigin origin = ooStatic);
    ~CPluginsBar();

    void DestroyImageLists();

    // vyhaze existujici a naleje nova tlacitka
    BOOL CreatePluginButtons();

    virtual int WINAPI GetNeededHeight();

    virtual void WINAPI Customize();

    void OnGetToolTip(LPARAM lParam);

    //  protected:
    //    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);
};

extern void PrepareToolTipText(char* buff, BOOL stripHotKey);

extern void GetSVGIconsMainToolbar(CSVGIcon** svgIcons, int* svgIconsCount);
