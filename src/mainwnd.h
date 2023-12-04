// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define HOT_PATHS_COUNT 30

#define TASKBAR_ICON_ID 0x0000

extern const int SPLIT_LINE_WIDTH;
extern const int MIN_WIN_WIDTH;

struct CCommandLineParams;

// pokud uzivatel nechce vic instanci, pouze aktivujeme predchozi
BOOL CheckOnlyOneInstance(const CCommandLineParams* cmdLineParams);

// otevrenym oknum interniho vieweru a findu rozesle zpravu WM_USER_CFGCHANGED
void BroadcastConfigChanged();

// univerzalni callback pro message boxy
void CALLBACK MessageBoxHelpCallback(LPHELPINFO helpInfo);

//
// ****************************************************************************

class CEditWindow;
class CFilesWindow;
struct CUserMenuItem;
class CUserMenuItems;
class CViewerMasks;
class CEditorMasks;
class CHighlightMasks;
class CMainToolBar;
class CPluginsBar;
class CBottomToolBar;
class CUserMenuBar;
class CHotPathsBar;
class CDriveBar;
class CMenuPopup;
class CMenuBar;
class CMenuNew;
class CToolTip;
class CAnimate;

//****************************************************************************
//
// CToolTipWindow
//

class CToolTipWindow : public CWindow
{ // zobrazuje tool-tip nezavisle na pozici kurzoru
public:
    CToolTipWindow() : CWindow(ooStatic) { ToolWindow = NULL; }

    void SetToolWindow(HWND toolWindow) { ToolWindow = toolWindow; }

protected:
    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    HWND ToolWindow; // TTM_WINDOWFROMPOINT vraci stale toto okno
};

//****************************************************************************
//
// CHotPathItem
//

// slouzi pro konfiguracni dialog a udava maximalni zadatelnou delku CHotPathItem::Path
// nechavame rezervu, protoze cesta muze obsahovat dlouhe promenne, ktere se po "expanzi" smrsknou na par znaku,
// napriklad $[SystemDrive] -> C:
#define HOTPATHITEM_MAXPATH (4 * MAX_PATH)

struct CHotPathItem
{
    // Name a Path jsou alokovane, aby nezraly pamet (lide po nas chteji neomezeny pocet hot paths)
    // Navic v pripade Path by bylo MAX_PATH malo (escapovani + promenne)
    char* Name;   // jmeno, pod kterym cesta vystupuje v menu
    char* Path;   // cesta, escapovana (zdvojene '$' znaky) kvuli promennym jako $(SalDir), atd
    BOOL Visible; // bude cesta pritomna v ChangeDrive menu

    CHotPathItem()
    {
        Name = NULL;
        Path = NULL;
        Visible = TRUE;
    }

    void CopyFrom(const CHotPathItem* src)
    {
        Empty();
        Name = DupStr(src->Name);
        Path = DupStr(src->Path);
        Visible = src->Visible;
    }

    void Empty()
    {
        if (Name != NULL)
        {
            free(Name);
            Name = NULL;
        }
        if (Path != NULL)
        {
            free(Path);
            Path = NULL;
        }
        Visible = TRUE;
    }
};

//****************************************************************************
//
// CHotPathItems
//

class CHotPathItems
{
protected:
    CHotPathItem Items[HOT_PATHS_COUNT];

public:
    ~CHotPathItems()
    {
        Empty();
    }

    // dealokace drzenych dat
    void Empty()
    {
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
            Items[i].Empty();
    }

    // nastavi atributy
    void Set(DWORD index, const char* name, const char* path)
    {
        if (Items[index].Name != NULL)
            free(Items[index].Name);
        if (Items[index].Path != NULL)
            free(Items[index].Path);
        Items[index].Name = DupStr(name);
        Items[index].Path = DupStr(path);
    }

    void Set(DWORD index, const char* name, const char* path, BOOL visible)
    {
        Set(index, name, path);
        Items[index].Visible = visible;
    }

    void SetPath(DWORD index, const char* path)
    {
        if (Items[index].Path != NULL)
            free(Items[index].Path);
        Items[index].Path = DupStr(path);
    }

    void SetVisible(DWORD index, BOOL visible)
    {
        Items[index].Visible = visible;
    }

    void GetName(int index, char* buffer, int bufferSize)
    {
        if (index < 0 || index >= HOT_PATHS_COUNT || Items[index].Name == NULL)
        {
            if (bufferSize > 0)
                *buffer = 0;
        }
        else
        {
            if (bufferSize > 0)
                lstrcpyn(buffer, Items[index].Name, bufferSize);
        }
    }

    void GetPath(int index, char* buffer, int bufferSize)
    {
        if (index < 0 || index >= HOT_PATHS_COUNT || Items[index].Path == NULL)
        {
            if (bufferSize > 0)
                *buffer = 0;
        }
        else
        {
            if (bufferSize > 0)
                lstrcpyn(buffer, Items[index].Path, bufferSize);
        }
    }

    int GetNameLen(int index)
    {
        if (index >= 0 && index < HOT_PATHS_COUNT && Items[index].Name != NULL)
            return lstrlen(Items[index].Name);
        else
            return 0;
    }

    int GetPathLen(int index)
    {
        if (index >= 0 && index < HOT_PATHS_COUNT && Items[index].Path != NULL)
            return lstrlen(Items[index].Path);
        else
            return 0;
    }

    // vrati index neprirazene hot path nebo -1 pokud jsou vsechny prirazene
    int GetUnassignedHotPathIndex();

    BOOL GetVisible(int index) { return Items[index].Visible; }
    BOOL CleanName(char* name); // oreze mezery a vrati TRUE, je-li name ok

    BOOL SwapItems(int index1, int index2); // prohodi dve polozky v poli

    // 'emptyItems' - prida i neprirazene cesty (pro assign)
    // 'emptyEcho' - pokud bude menu prazdne, volzi informaci, ze je prazdne
    // 'customize' - pripoji separator a customize
    // 'topSeparator' - pokud vlozi nejake cestu, soupne nad ne separator
    // 'forAssign' - volano z Assign Hot Path z context menu v directory line
    void FillHotPathsMenu(CMenuPopup* menu, int minCommand, BOOL emptyItems = FALSE, BOOL emptyEcho = TRUE,
                          BOOL customize = TRUE, BOOL topSeparator = FALSE, BOOL forAssign = FALSE);

    BOOL Save(HKEY hKey);     // ulozi cele pole
    BOOL Load(HKEY hKey);     // nacte cele pole
    BOOL Load1_52(HKEY hKey); // nacte cele pole verze 1.52

    void Load(CHotPathItems& source)
    {
        int i;
        for (i = 0; i < HOT_PATHS_COUNT; i++)
            Items[i].CopyFrom(&source.Items[i]);
    }
};

//*****************************************************************************
//
// UM_GetNextFileName - typ funkce, ktera postupne vraci nazvy souboru pro U.M.
//
// index - poradi dalsiho jmena (postupne jde od nuly vyse po jedne)
// path  - buffer pro cestu [MAX_PATH]
// name  - buffer pro jmeno [MAX_PATH]
// param - pomocny pointer pro user-data
//
// vraci uspech - pokracovat v ziskavani dalsich jmen? (vrati FALSE -> konec)

typedef BOOL (*UM_GetNextFileName)(int index, char* path, char* name, void* param);

struct CUMDataFromPanel
{
    CFilesWindow* Window;
    int* Index;
    int Count;

    CUMDataFromPanel(CFilesWindow* window)
    {
        Count = -1;
        Index = NULL;
        Window = window;
    }
    ~CUMDataFromPanel()
    {
        if (Index != NULL)
            delete[] (Index);
    }
};

BOOL GetNextFileFromPanel(int index, char* path, char* name, void* param);

struct CUserMenuAdvancedData;
struct IContextMenu2;

class CMainWindowAncestor : public CWindow
{
private:
    CFilesWindow* ActivePanel; // bud LeftPanel nebo RightPanel

public:
    // POZOR: volat az pokud nejde pozadavek resit pomoci FocusPanel a ChangePanel (hodi se
    // je-li EditMode TRUE); pri volani zjistit jestli je dost siroky panel, atd. (viz FocusPanel)
    void SetActivePanel(CFilesWindow* active)
    {
        ActivePanel = active;
    }
    CFilesWindow* GetActivePanel()
    {
        return ActivePanel;
    }
};

//class CTipOfTheDayDialog;
class CDetachedFSList;
class CPathHistory;

enum CMainWindowsHitTestEnum
{
    mwhteNone,
    mwhteTopRebar,      // je to v rebaru, ale na zadnem bandu
    mwhteMenu,          // v rebaru na bendu Menu
    mwhteTopToolbar,    // v rebaru na bendu TopToolbar
    mwhtePluginsBar,    // v rebaru na bendu PluginsBar
    mwhteMiddleToolbar, // Middle Bar na split bare
    mwhteUMToolbar,     // v rebaru na bendu UserMenuBar
    mwhteHPToolbar,     // v rebaru na bendu HotPathsBar
    mwhteDriveBar,      // v rebaru na bendu Drive Bar
    mwhteWorker,        // v rebaru na bendu Worker
    mwhteCmdLine,
    mwhteBottomToolbar,
    mwhteSplitLine,
    mwhteLeftDirLine,
    mwhteLeftHeaderLine,
    mwhteLeftStatusLine,
    mwhteLeftWorkingArea,
    mwhteRightDirLine,
    mwhteRightHeaderLine,
    mwhteRightStatusLine,
    mwhteRightWorkingArea,
};

struct CChangeNotifData
{
    char Path[MAX_PATH];
    BOOL IncludingSubdirs;
};

typedef TDirectArray<CChangeNotifData> CChangeNotifArray;

//
// ****************************************************************************
// CDynString
//
// pomocny objekt pro dynamicky alokovany string

struct CDynString
{
    char* Buffer;
    int Length;
    int Allocated;

    CDynString()
    {
        Buffer = NULL;
        Length = 0;
        Allocated = 0;
    }

    ~CDynString()
    {
        if (Buffer != NULL)
            free(Buffer);
    }

    BOOL Append(const char* str, int len); // vraci TRUE pri uspechu; je-li 'len' -1, bere se "len=strlen(str)"

    const char* GetString() const { return Buffer; }
};

// flagy pro metody CompareDirectories
#define COMPARE_DIRECTORIES_BYTIME 0x00000001       // porovname podle datumu a casu
#define COMPARE_DIRECTORIES_BYCONTENT 0x00000002    // porovname podle obsahu
#define COMPARE_DIRECTORIES_BYATTR 0x00000004       // porovname podle atributu
#define COMPARE_DIRECTORIES_SUBDIRS 0x00000008      // vcetne podadresaru
#define COMPARE_DIRECTORIES_SUBDIRS_ATTR 0x00000010 // atributy podadresaru
#define COMPARE_DIRECTORIES_BYSIZE 0x00000020       // porovname velikosti
#define COMPARE_DIRECTORIES_ONEPANELDIRS 0x00000040 // oznaci adresare existujici jen v jednom panelu (nezaleza dovnitr podadresaru)
#define COMPARE_DIRECTORIES_IGNFILENAMES 0x00000080 // ignorovat soubory, jejichz jmena odpovidajici masce Configuration.CompareIgnoreFilesMasks
#define COMPARE_DIRECTORIES_IGNDIRNAMES 0x00000100  // ignorovat adresare, jejichz jmena odpovidajici masce Configuration.CompareIgnoreDirsMasks

class CMainWindow : public CMainWindowAncestor
{
public:
    BOOL EditMode;             // aktivni editwindow, zbytek jen simuluje
    BOOL EditPermanentVisible; // editwindow je stale viditelne
    BOOL HelpMode;             // if TRUE, then Shift+F1 help mode is active

    CFilesWindow *LeftPanel,
        *RightPanel;
    CEditWindow* EditWindow;
    CMainToolBar* TopToolBar;
    CPluginsBar* PluginsBar;
    CMainToolBar* MiddleToolBar;
    CUserMenuBar* UMToolBar;
    CHotPathsBar* HPToolBar;
    CDriveBar* DriveBar;
    CDriveBar* DriveBar2;
    CBottomToolBar* BottomToolBar;
    //CAnimate       *AnimateBar;

    HWND HTopRebar;
    CMenuBar* MenuBar;
    UINT TaskbarRestartMsg; // toto rozesila explorer, kdyz je potreba naplnit ikonky

    CToolTip* ToolTip; // vzdy existuje a je vytvoreny - vsechny controly pouzivaji tento jeden tooltip

    BOOL Created;

    CHotPathItems HotPaths;
    CViewTemplates ViewTemplates;

    CUserMenuItems* UserMenuItems;
    CViewerMasks* ViewerMasks;
    CRITICAL_SECTION ViewerMasksCS; // sekce pouzita jen pro synchronizaci pristupu do 'ViewerMasks' (jen zapis kdekoliv a cteni v jinem nez hlavnim threadu)
    CViewerMasks* AltViewerMasks;
    CEditorMasks* EditorMasks;
    CHighlightMasks* HighlightMasks;

    CDetachedFSList* DetachedFSList; // seznam "odpojenych" FS pro Alt+F1/F2 (pro opetovne pripojeni)

    CFileHistory* FileHistory; // historie souboru, nad kterymi probehlo View nebo Edit

    CPathHistory* DirHistory; // historie navstivenych adresaru
    BOOL CanAddToDirHistory;  // TRUE az po nahozeni Salamandera (aby se neregistrovaly zmeny cest pri nacitani konfigurace)

    CMenuNew* ContextMenuNew;          // pro obsluhu prikazu z menu New
    IContextMenu2* ContextMenuChngDrv; // pro obsluhu prikazu z Change Drive Menu

    char SelectionMask[MAX_PATH]; // maska pro select/deselect

    BOOL CanClose;                    // je mozne zavrit hl. okno? (probehl kompletni start aplikace?)
    BOOL CanCloseButInEndSuspendMode; // TRUE pokud CanClose bylo TRUE a je FALSE jen kvuli tomu, ze probiha message-loopa ve zpracovani WM_USER_END_SUSPMODE
    BOOL SaveCfgInEndSession;         // TRUE = ve WM_ENDSESSION se ma ulozit konfigurace
    BOOL WaitInEndSession;            // TRUE = ve WM_ENDSESSION se ma pockat na dokonceni diskovych operaci
    BOOL DisableIdleProcessing;       // TRUE = nebudeme provadet IDLE processing (soft jiz konci, jen by zdrzoval a vse komplikoval)
                                      //    CTipOfTheDayDialog *TipOfTheDayDialog;

    BOOL DragMode;
    int DragSplitX;

    // tuto funkci jsem zatim vyradil - implmenetace by znamenala modifikaci zobrazeneho menu
    //    HWND           DrivesControlHWnd;   // pokud je zobrazeno Alt+F1/F2 okno, je zde jeho handle, jinak NULL

    HWND HDisabledKeyboard; // handle okna, ktere zakazalo zpracovani klavesnice
                            // pokud dojde ke stisteni klavesy Escape, je tomuto
                            // oknu zaslana message WM_CANCELMODE

    int CmdShow; // to nam prislo do WinMain

    int ActivateSuspMode; // pocitadlo wm_activateapp aktivaci/deaktivaci, asi se ztraci zpravy ...

    RECT WindowRect; // aktualni pozice okna

    BOOL CaptionIsActive; // je caption hlavniho okna aktivni?

    // promenne tykajici se rozesilani zprav o zmenach na cestach (jak FS, tak diskove cesty)
    CChangeNotifArray ChangeNotifArray;    // pole se zpravami o zmenach na cestach
    CRITICAL_SECTION DispachChangeNotifCS; // kriticka sekce pro praci s ChangeNotifArray
    int LastDispachChangeNotifTime;        // cas posledniho rozeslani zprav
    BOOL NeedToResentDispachChangeNotif;   // TRUE = je treba znovu postnout WM_USER_DISPACHCHANGENOTIF

    BOOL DoNotLoadAnyPlugins; // TRUE = neloadit zadne pluginy (napr. thumbnail loadery); POZOR: menit pres SetDoNotLoadAnyPlugins()

    DWORD SHChangeNotifyRegisterID; // vraceno funkci SHChangeNotifyRegister
    BOOL IgnoreWM_SETTINGCHANGE;

    BOOL LockedUI;
    HWND LockedUIToolWnd;
    char* LockedUIReason;

    CITaskBarList3 TaskBarList3; // pro ovladani progress na taskbar od W7

protected:
    int WindowWidth, // kvuli zmene splitu
        WindowHeight,
        TopRebarHeight,
        BottomToolBarHeight,
        EditHeight,
        PanelsHeight,
        SplitPositionPix,
        DragAnchorX;
    double SplitPosition,        // akualni split position (0..1)
        BeforeZoomSplitPosition, // split position pret Zoomovanim panelu
        DragSplitPosition;       // zobrazeno v tooltipu
    CToolTipWindow ToolTipWindow;

    BOOL FirstActivateApp; // WM_ACTIVATEAPP vyuziva tuto promennou pri spusteni

    BOOL IdleStatesChanged;    // je nastavovani metodou CheckAndSet()
    BOOL PluginsStatesChanged; // je treba rebuildnout plugin bar

public:
    CMainWindow();
    ~CMainWindow();

    void EnterViewerMasksCS() { HANDLES(EnterCriticalSection(&ViewerMasksCS)); }
    void LeaveViewerMasksCS() { HANDLES(LeaveCriticalSection(&ViewerMasksCS)); }
    BOOL GetViewersAssoc(int wantedViewerType, CDynString* strViewerMasks); // pomocna metoda: posbira vsechny masky asociovane pro viewer-type 'wantedViewerType'; vraci TRUE pri uspechu (pri dostatku pameti pro string)

    void ClearHistory(); // promaze vsechny historie

    void GetSplitRect(RECT& r);

    BOOL IsGood();

    // rozesle informaci o zmene na ceste 'path' (je-li 'includingSubdirs' TRUE, jsou zmeny
    // mozne i v podadresarich); informace se rozdistribuuje do panelu a do vsech otevrenych FS
    // z plug-inu (panely i FS muzou reagovat refreshnutim obsahu);
    // mozne volat z lib. threadu
    void PostChangeOnPathNotification(const char* path, BOOL includingSubdirs);

    // tyto funkce nedopadnou, pokud neni splnena podminka CFilesWindow::CanBeFocused
    void ChangePanel(BOOL force = FALSE);                                   // cti EditMode; aktivuje neaktivni panel; (pokud je force==TRUE, ignoruje ZOOM)
    void FocusPanel(CFilesWindow* focus, BOOL testIfMainWndActive = FALSE); // sejme EditMode, protoze do panelu umisti focus
    void FocusLeftPanel();                                                  // vola FocusPanel pro levy panel

    // porovna adresare v levem a pravem panelu
    void CompareDirectories(DWORD flags); // flags je kombinaci COMPARE_DIRECTORIES_xxx

    // zajisti volani DirHistory->AddPathUnique a zaroven spravne nastaveni SetHistory panelu
    void DirHistoryAddPathUnique(int type, const char* pathOrArchiveOrFSName,
                                 const char* archivePathOrFSUserPart, HICON hIcon,
                                 CPluginFSInterfaceAbstract* pluginFS,
                                 CPluginFSInterfaceEncapsulation* curPluginFS);

    // zajisti volani DirHistory->RemoveActualPath a zaroven spravne nastaveni SetHistory panelu
    void DirHistoryRemoveActualPath(CFilesWindow* panel);

    // vraci TRUE pokud se podarilo uzavrit odpojene FS (vola TryCloseOrDetach, ReleaseObject a pak CloseFS),
    // pokud plug-in nechce FS zavrit, pta se usera jestli zavrit nasilne (ala canForce==TRUE)
    BOOL CloseDetachedFS(HWND parent, CPluginFSInterfaceEncapsulation* detachedFS);

    // vraci TRUE, pokud jiz plug-in neni Salamanderem pouzivan -> muze se unloadnout
    BOOL CanUnloadPlugin(HWND parent, CPluginInterfaceAbstract* plugin);

    // vola se pri zavirani FS - v dir-historii jsou ulozene FS ifacy, ktere je po zavreni potreba
    // NULLovat (aby nahodou nedoslo ke shode jen diky alokaci FS ifacu na stejnou adresu)
    void ClearPluginFSFromHistory(CPluginFSInterfaceAbstract* fs);

    void SaveConfig(HWND parent = NULL); // parent: NULL = MainWindow->HWindow
    BOOL LoadConfig(BOOL importingOldConfig, const CCommandLineParams* cmdLineParams);
    void SavePanelConfig(CFilesWindow* panel, HKEY hSalamander, const char* reg);
    void LoadPanelConfig(char* panelPath, CFilesWindow* panel, HKEY hSalamander, const char* reg);
    void DeleteOldConfigurations(BOOL* deleteConfigurations, BOOL autoImportConfig,
                                 const char* autoImportConfigFromKey, BOOL doNotDeleteImportedCfg);

    void UserMenu(HWND parent, int itemIndex, UM_GetNextFileName getNextFile, void* data,
                  CUserMenuAdvancedData* userMenuAdvancedData);

    // nastavi hot path 'path' s indexem 'index'; vstupuje validni cesta, bez zdvojenych '$' nebo bez promennych
    void SetUnescapedHotPath(int index, const char* path);

    // do 'buffer' o velikosti 'bufferSize' expanduje hot path s indexem 'index'
    // 'hParent' -- k tomuto oknu se budou zobrazovat chyby behem expanze cesty; pokud je NULL, chyby se nezobrazuji
    // vraci TRUE, pokud se cestu podarilo ziskat, jinak FALSE
    BOOL GetExpandedHotPath(HWND hParent, int index, char* buffer, int bufferSize);

    // vrati index neprirazene hot path nebo -1 pokud jsou vsechny prirazene
    int GetUnassignedHotPathIndex();

    void SetFont();
    void SetEnvFont();

    void RefreshDiskFreeSpace();
    void RefreshDirs();

    // obnovi DefaultDir podle cest v panelech, je-li 'activePrefered' bude mit prednost
    // cesta v aktivnim panelu (zapise se pozdeji do DefaultDir), jinak ma prednost cesta
    // v neaktivnim panelu
    void UpdateDefaultDir(BOOL activePrefered);
    void SetDefaultDirectories(const char* curPath = NULL);

    HWND GetActivePanelHWND();
    int GetDirectoryLineHeight();

    CFilesWindow* GetOtherPanel(CFilesWindow* panel)
    {
        return panel == LeftPanel ? RightPanel : LeftPanel;
    }

    BOOL EditWindowKnowHWND(HWND hwnd);
    void EditWindowSetDirectory(); // nastavi text pred command-line a zaroven ji enabluje/disabluje
    HWND GetEditLineHWND(BOOL disableSkip = FALSE);

    // vraci TRUE, pokud byla klavesa zpracovana
    BOOL HandleCtrlLetter(char c); // Ctrl+pismenko hotkeys

    void LayoutWindows();
    BOOL ToggleTopToolBar(BOOL storePos = TRUE);
    BOOL TogglePluginsBar(BOOL storePos = TRUE);
    BOOL ToggleMiddleToolBar();
    BOOL ToggleBottomToolBar();
    BOOL ToggleUserMenuToolBar(BOOL storePos = TRUE);
    BOOL ToggleHotPathsBar(BOOL storePos = TRUE);
    // pokud je 'twoDriveBars' rovno TRUE, uzivatel chce zapnout/vypnout dve listy
    // jinak pouze jednu listu
    BOOL ToggleDriveBar(BOOL twoDriveBars, BOOL storePos = TRUE);

    void ToggleToolBarGrips();

    BOOL InsertMenuBand();
    BOOL CreateAndInsertWorkerBand();
    BOOL InsertTopToolbarBand();
    BOOL InsertPluginsBarBand();
    BOOL InsertUMToolbarBand();
    BOOL InsertHPToolbarBand();
    BOOL InsertDriveBarBand(BOOL twoDriveBars);
    void StoreBandsPos();

    //    void ReleaseMenuNew();

    void UpdateBottomToolBar();

    void AddTrayIcon(BOOL updateIcon = FALSE);
    void RemoveTrayIcon();
    void SetTrayIconText(const char* text);

    // naleje menu polozkama UserMenuItems
    // customize urcuje, jestli se ma pripojit nabidka pro konfiguraci
    void FillUserMenu(CMenuPopup* menu, BOOL customize = TRUE);
    // interni rekurzivni funkce pro plneni
    void FillUserMenu2(CMenuPopup* menu, int* iterator, int max);

    // naplni menu View modama
    // 'popup' popup, ktery budeme plnit
    // 'firstIndex' index do 'popup', od ktere je treba polozky vkladat
    // 'type' 0=TopToolbar||MiddleToolBar, 1=LeftMenu/LeftToolbar, 2=RightMenu/RightToolbar
    void FillViewModeMenu(CMenuPopup* popup, int firstIndex, int type);

    void MakeFileList();

    // pomocna metoda pro SetTitle; 'text' musi mit delku minimalne 2 * MAX_PATH
    void GetFormatedPathForTitle(char* text);

    // pokud je 'text' == NULL, bude nastaven standardni obsah
    void SetWindowTitle(const char* text = NULL);

    // nastavi ikonku hlavniho okna podle MainWindowIconIndex
    void SetWindowIcon();

    void ShowCommandLine();
    void HideCommandLine(BOOL storeContent = FALSE, BOOL focusPanel = TRUE);

    void GetWindowSplitRect(RECT& r);
    BOOL PtInChild(HWND hChild, POINT p);
    void OnWmContextMenu(HWND hWnd, int xPos, int yPos);

    CFilesWindow* GetNonActivePanel()
    {
        return (GetActivePanel() == LeftPanel) ? RightPanel : LeftPanel;
    }

    virtual LRESULT WindowProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    void SafeHandleMenuNewMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);

    void OnEnterIdle();
    void RefreshCommandStates();
    inline void CheckAndSet(DWORD* target, DWORD source)
    {
        if (*target != source)
        {
            *target = source;
            IdleStatesChanged = TRUE;
        }
    }

    // doslo ke zmene barev nebo barevne hloubky obrazovky; uz jsou vytvorene nove imagelisty
    // pro tooblary a je treba je priradit controlum, ktere je pouzivaji
    // reloadUMIcons urcuje, zda se znovu nactou ikony pro UserMenu, POZOR muze byt drasticky pomale,
    // pokud polozky UM lezi na sitovem disku (napriklad 1500ms na ikonu)
    void OnColorsChanged(BOOL reloadUMIcons);

    // informuje hlavni okno, ze doslo ke zmene pluginu (load pluginu, pridani/odstraneni
    // pluginu, ... akce vedouci ke zmene Plugin Bar)
    // metoda pouze uspini promennou, k vlastnimu rebuildu toolbary dojde az v IDLE
    void OnPluginsStateChanged()
    {
        PluginsStatesChanged = TRUE;
    }

    // Context Help (Shift+F1) support
    BOOL CanEnterHelpMode();
    void OnContextHelp();
    HWND SetHelpCapture(POINT point, BOOL* pbDescendant);
    BOOL ProcessHelpMsg(MSG& msg, DWORD* pContext, HWND* hDirtyWindow); // hDirtyWindow: vraci okno, kteremu jsme zaslali WM_USER_HELP_MOUSEMOVE a je mu potreba zaslat WM_USER_HELP_MOUSELEAVE
    void ExitHelpMode();
    DWORD MapClientArea(POINT point);
    DWORD MapNonClientArea(int iHit);

    CMainWindowsHitTestEnum HitTest(int xPos, int yPos); // scree souradnice

    // podle cest v panelech zamackne tlacitka v drive bars
    void UpdateDriveBars();

    // pokud drive bar ma jinou bitovou masku disku nez 'drivesMask' nebo je-li 'checkCloudStorages' TRUE
    // a drive bar ma jinou bitovou masku cloudu nez 'cloudStoragesMask', bude pregenerovana
    void RebuildDriveBarsIfNeeded(BOOL useDrivesMask, DWORD drivesMask, BOOL checkCloudStorages,
                                  DWORD cloudStoragesMask);

    // nastavuje DoNotLoadAnyPlugins + pri FALSE rozesila refreshe panelum loadicim thumbnaily
    void SetDoNotLoadAnyPlugins(BOOL doNotLoad);

    // na zaklade 'show' zobrazi nebo zhasne dve drive bary
    void ShowHideTwoDriveBarsInternal(BOOL show);

    // vrati sirku split bary v bodech (pokud je zobrazena Middle Bar, bude o ni rozsirena)
    int GetSplitBarWidth();

    void StartAnimate();
    void StopAnimate();

    void CancelPanelsUI();          // ukonci pripadny QuickSearch a QuickRename
    BOOL QuickRenameWindowActive(); // vraci TRUE, pokud je v nekterem z panelu aktivni QuickRenameWindow

    // prejmenovat podle QuickRenameWindow; vraci TRUE v pripade uspech nebo pokud zadne prejmenovani neprobiha
    // pokud vrati FALSE, je treba okenko nezavirat (nevolat CancelPanelsUI) a nemenit focus (je v editline)
    BOOL DoQuickRename();

    // vraci TRUE, pokud je panel maximalizovan na ukor druheho panelu
    // a prikaz Zoom povede k nastaveni obou panelu v pomeru 50 ku 50 procentum
    BOOL IsPanelZoomed(BOOL leftPanel);

    // prepne Smart Column mode pro dany panel
    void ToggleSmartColumnMode(CFilesWindow* panel);
    // vrati Smart Column mode (TRUE/FALSE) pro dany panel
    BOOL GetSmartColumnMode(CFilesWindow* panel);

    // pripojeni a odpojeni hlavniho okna k odberu notifikaci o zmenach v shellu (pridani/odebrani disku)
    BOOL SHChangeNotifyInitialize();
    BOOL SHChangeNotifyRelease();
    BOOL OnAssociationsChangedNotification(BOOL showWaitWnd);

    void SafeHandleMenuChngDrvMsg2(UINT uMsg, WPARAM wParam, LPARAM lParam, LRESULT* plResult);

    void ApplyCommandLineParams(const CCommandLineParams* params, BOOL setActivePanelAndPanelPaths = TRUE);

    void LockUI(BOOL lock, HWND hToolWnd, const char* lockReason);
    BOOL HasLockedUI() { return LockedUI; }
    void BringLockedUIToolWnd();

    CFilesWindow* GetPanel(int panel);
    void PostFocusNameInPanel(int panel, const char* path, const char* name);

    friend void CMainWindow_RefreshCommandStates(CMainWindow* obj);
};

//
// ****************************************************************************
// C__MainWindowCS
//
// zajisteni pristupu k promenne MainWindow mimo hlavni thread, pouziti:
// if (MainWindowCS.LockIfNotClosed())
// {
//
//   tady muzeme makat s MainWindow (zadny blokujici veci, pokud mozno jen vytahnout data a zmizet)...
//
//   MainWindowCS.Unlock();
// }

class C__MainWindowCS
{
protected:
    CRITICAL_SECTION cs;
    BOOL IsClosed;

public:
    C__MainWindowCS()
    {
        HANDLES(InitializeCriticalSection(&cs));
        IsClosed = FALSE;
    }
    ~C__MainWindowCS() { HANDLES(DeleteCriticalSection(&cs)); }

    void SetClosed()
    {
        HANDLES(EnterCriticalSection(&cs));
        IsClosed = TRUE;
        HANDLES(LeaveCriticalSection(&cs));
    }

    BOOL LockIfNotClosed()
    {
        HANDLES(EnterCriticalSection(&cs));
        if (!IsClosed)
            return TRUE;
        HANDLES(LeaveCriticalSection(&cs));
        return FALSE;
    }

    void Unlock() { HANDLES(LeaveCriticalSection(&cs)); }
};

extern C__MainWindowCS MainWindowCS;

//
// ****************************************************************************

// Ochrana pred ShellExtensions, ktere nam strileji MainWindow pres DestroyWindow
// pokud je CanDestroyMainWindow==FALSE a MainWindow obdrzi WM_DESTROY, spusti
// reportici mechanismus.
extern BOOL CanDestroyMainWindow;

extern CMainWindow* MainWindow;
