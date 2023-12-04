// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define USRMNUARGS_MAXLEN 32772    // velikost bufferu (+1 proti maximalni delce stringu) (=32776 (Vista/Win7 pres .bat) - 5 ("C:\\a ") + 1)
#define USRMNUCMDLINE_MAXLEN 32777 // velikost bufferu (+1 proti maximalni delce stringu)

//****************************************************************************
//
// CUserMenuIconBkgndReader
//

struct CUserMenuIconData
{
    char FileName[MAX_PATH];  // jmeno souboru, ze ktereho mame cist ikonu z indexu IconIndex (pres ExtractIconEx())
    DWORD IconIndex;          // viz komentar k FileName
    char UMCommand[MAX_PATH]; // jmeno souboru, ze ktereho mame cist ikonu (pres GetFileOrPathIconAux())

    HICON LoadedIcon; // NULL = nenactena ikona, jinak handle nactene ikony

    CUserMenuIconData(const char* fileName, DWORD iconIndex, const char* umCommand);
    ~CUserMenuIconData();

    void Clear();
};

class CUserMenuIconDataArr : public TIndirectArray<CUserMenuIconData>
{
protected:
    DWORD IRThreadID; // unikatni ID threadu pro nacitani techto ikon

public:
    CUserMenuIconDataArr() : TIndirectArray<CUserMenuIconData>(50, 50) { IRThreadID = 0; }

    void SetIRThreadID(DWORD id) { IRThreadID = id; }
    DWORD GetIRThreadID() { return IRThreadID; }

    HICON GiveIconForUMI(const char* fileName, DWORD iconIndex, const char* umCommand);
};

class CUserMenuIconBkgndReader
{
protected:
    BOOL SysColorsChanged; // pomocna promenna pro zjisteni zmeny systemovych barev od okamziku otevreni cfg dialogu

    CRITICAL_SECTION CS;       // sekce pro pristup k datum objektu
    DWORD IconReaderThreadUID; // generator unikatnich ID threadu pro cteni ikon
    BOOL CurIRThreadIDIsValid; // TRUE = thread bezi a CurIRThreadID je platne
    DWORD CurIRThreadID;       // unikatni ID threadu (viz IconReaderThreadUID), ve kterem se ctou ikony pro aktualni verzi user-menu
    BOOL AlreadyStopped;       // TRUE = uz zadne cteni ikon, hlavni okno se zavrelo/zavira

    int UserMenuIconsInUse;                            // > 0: ikony z user menu jsou prave v otevrenem menu, nemuzeme je hned updatnout na nove ikony; muze byt nejvys 2 (cfg Salama + Find: user menu)
    CUserMenuIconDataArr* UserMenuIIU_BkgndReaderData; // uschovna novych ikon pri UserMenuIconsInUse > 0
    DWORD UserMenuIIU_ThreadID;                        // uschovna ID threadu (pro zjisteni aktualnosti dat) pri UserMenuIconsInUse > 0

public:
    CUserMenuIconBkgndReader();
    ~CUserMenuIconBkgndReader();

    // hlavni okno se zavira = uz nechceme prijimat zadna data o ikonach v user menu
    void EndProcessing();

    // POZOR: uvnitr se dealokuje 'bkgndReaderData'
    void StartBkgndReadingIcons(CUserMenuIconDataArr* bkgndReaderData);

    BOOL IsCurrentIRThreadID(DWORD threadID);

    BOOL IsReadingIcons();

    // POZOR: po volani teto funkce je za uvolneni 'bkgndReaderData' zodpovedny tento objekt
    void ReadingFinished(DWORD threadID, CUserMenuIconDataArr* bkgndReaderData);

    // vstup/vystup do sekce, kde se pouzivaji ikony z user menu a tedy neni je mozne
    // behem provadeni teto sekce updatit (hlavne jde o otevreni user menu)
    void BeginUserMenuIconsInUse();
    void EndUserMenuIconsInUse();

    // pokud se nacetly ikony pro jiz neaktualni user menu, vraci FALSE, jinak:
    // pokud jsou ikony prave v otevrenem menu (viz UserMenuIconsInUse), vraci FALSE;
    // pokud nejsou ikony prave v otevrenem menu, vraci TRUE a POZOR: neopusti CS,
    // aby byl blokovany pristup z ostatnich threadu (hlavne pristup k user menu z threadu
    // Findu), pro vystup z CS po updatu ikon se pouziva LeaveCSAfterUMIconsUpdate()
    BOOL EnterCSIfCanUpdateUMIcons(CUserMenuIconDataArr** bkgndReaderData, DWORD threadID);
    void LeaveCSAfterUMIconsUpdate();

    void ResetSysColorsChanged() { SysColorsChanged = FALSE; }
    void SetSysColorsChanged() { SysColorsChanged = TRUE; }
    BOOL HasSysColorsChanged() { return SysColorsChanged; }
};

extern CUserMenuIconBkgndReader UserMenuIconBkgndReader;

//****************************************************************************
//
// CUserMenuItem
//

enum CUserMenuItemType
{
    umitItem,         // klasicka polozka
    umitSubmenuBegin, // oznacuje zacatek popupu
    umitSubmenuEnd,   // oznacuje konec popupu
    umitSeparator     // oznacuje konec popupu
};

struct CUserMenuItem
{
    char *ItemName,
        *UMCommand,
        *Arguments,
        *InitDir,
        *Icon;

    int ThroughShell,
        CloseShell,
        UseWindow,
        ShowInToolbar;

    CUserMenuItemType Type;

    HICON UMIcon;

    CUserMenuItem(char* name, char* umCommand, char* arguments, char* initDir, char* icon,
                  int throughShell, int closeShell, int useWindow, int showInToolbar,
                  CUserMenuItemType type, CUserMenuIconDataArr* bkgndReaderData);

    CUserMenuItem();

    CUserMenuItem(CUserMenuItem& item, CUserMenuIconDataArr* bkgndReaderData);

    ~CUserMenuItem();

    // pokusi se vytahnout handle ikony v tomto poradi
    // a) promenne Icon
    // b) SHGetFileInfo
    // c) veme default ze systemu
    // cteni ikon na pozadi: je-li 'bkgndReaderData' NULL, cteme hned, jinak se ikony
    // ctou na pozadi - je-li 'getIconsFromReader' FALSE, sbirame do 'bkgndReaderData',
    // co nacist, je-li TRUE, ikony uz jsou nactene a jen prevezmeme handly nactenych
    // ikon z 'bkgndReaderData'
    BOOL GetIconHandle(CUserMenuIconDataArr* bkgndReaderData, BOOL getIconsFromReader);

    // prohleda ItemName na & a vrati HotKey a TRUE kdyz ji najde
    BOOL GetHotKey(char* key);

    BOOL Set(char* name, char* umCommand, char* arguments, char* initDir, char* icon);
    void SetType(CUserMenuItemType type);
    BOOL IsGood() { return ItemName != NULL && UMCommand != NULL &&
                           Arguments != NULL && InitDir != NULL && Icon != NULL; }
};

//****************************************************************************
//
// CUserMenuItems
//

class CUserMenuItems : public TIndirectArray<CUserMenuItem>
{
public:
    CUserMenuItems(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CUserMenuItem>(base, delta, dt) {}

    // nakopci seznam ze 'source'
    BOOL LoadUMI(CUserMenuItems& source, BOOL readNewIconsOnBkgnd);

    // hleda posledni (zaviraci) polozku submenu adresovaneho promennou 'index'
    // pokud ukonecni nenajde, vrati -1
    int GetSubmenuEndIndex(int index);
};
