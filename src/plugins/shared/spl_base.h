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
#pragma pack(push, enter_include_spl_base) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// v debug verzi budeme testovat, jestli se neprekryvaji zdrojova a cilova pamet (pro memcpy se nesmi prekryvat)
#if defined(_DEBUG) && defined(TRACE_ENABLE)
#define memcpy _sal_safe_memcpy
#ifdef __cplusplus
extern "C"
{
#endif
    void* _sal_safe_memcpy(void* dest, const void* src, size_t count);
#ifdef __cplusplus
}
#endif
#endif // defined(_DEBUG) && defined(TRACE_ENABLE)

// nasledujici funkce nepadaji pri praci s neplatnou pameti (ani pri praci s NULL):
// lstrcpy, lstrcpyn, lstrlen a lstrcat (ty jsou definovane s priponou A nebo W, proto
// je primo neredefinujeme), v zajmu snazsiho odladeni chyb potrebujeme, aby padaly,
// protoze jinak se na chybu prijde pozdeji v miste, kde uz nemusi byt jasne, co ji
// zpusobilo
#define lstrcpyA _sal_lstrcpyA
#define lstrcpyW _sal_lstrcpyW
#define lstrcpynA _sal_lstrcpynA
#define lstrcpynW _sal_lstrcpynW
#define lstrlenA _sal_lstrlenA
#define lstrlenW _sal_lstrlenW
#define lstrcatA _sal_lstrcatA
#define lstrcatW _sal_lstrcatW
#ifdef __cplusplus
extern "C"
{
#endif
    LPSTR _sal_lstrcpyA(LPSTR lpString1, LPCSTR lpString2);
    LPWSTR _sal_lstrcpyW(LPWSTR lpString1, LPCWSTR lpString2);
    LPSTR _sal_lstrcpynA(LPSTR lpString1, LPCSTR lpString2, int iMaxLength);
    LPWSTR _sal_lstrcpynW(LPWSTR lpString1, LPCWSTR lpString2, int iMaxLength);
    int _sal_lstrlenA(LPCSTR lpString);
    int _sal_lstrlenW(LPCWSTR lpString);
    LPSTR _sal_lstrcatA(LPSTR lpString1, LPCSTR lpString2);
    LPWSTR _sal_lstrcatW(LPWSTR lpString1, LPCWSTR lpString2);
#ifdef __cplusplus
}
#endif

// puvodni SDK, ktere bylo soucasti VC6 melo hodnotu definovanou jako 0x00000040 (rok 1998, kdy se atribut jeste nepouzival, nastoupil az s W2K)
#if (FILE_ATTRIBUTE_ENCRYPTED != 0x00004000)
#pragma message(__FILE__ " ERROR: FILE_ATTRIBUTE_ENCRYPTED != 0x00004000. You have to install latest version of Microsoft SDK. This value has changed!")
#endif

class CSalamanderGeneralAbstract;
class CPluginDataInterfaceAbstract;
class CPluginInterfaceForArchiverAbstract;
class CPluginInterfaceForViewerAbstract;
class CPluginInterfaceForMenuExtAbstract;
class CPluginInterfaceForFSAbstract;
class CPluginInterfaceForThumbLoaderAbstract;
class CSalamanderGUIAbstract;
class CSalamanderSafeFileAbstract;
class CGUIIconListAbstract;

//
// ****************************************************************************
// CSalamanderDebugAbstract
//
// sada metod ze Salamandera, ktere se pouzivaji pro hledani chyb v debug i release verzi

// makro CALLSTK_MEASURETIMES - zapne mereni casu straveneho pri priprave call-stack hlaseni (meri se pomer proti
//                              celkovemu casu behu funkci)
//                              POZOR: nutne zapnout tez pro kazdy plugin zvlast
// makro CALLSTK_DISABLEMEASURETIMES - potlaci mereni casu straveneho pri priprave call-stack hlaseni v DEBUG/SDK/PB verzi

#if (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext
{
    DWORD PushesCounterStart;                      // start-stav pocitadla Pushu volanych v tomto threadu
    LARGE_INTEGER PushPerfTimeCounterStart;        // start-stav pocitadla casu straveneho v metodach Push volanych v tomto threadu
    LARGE_INTEGER IgnoredPushPerfTimeCounterStart; // start-stav pocitadla casu straveneho v nemerenych (ignorovanych) metodach Push volanych v tomto threadu
    LARGE_INTEGER StartTime;                       // "cas" Pushe tohoto call-stack makra
    DWORD_PTR PushCallerAddress;                   // adresa makra CALL_STACK_MESSAGE (adresa Pushnuti)
};
#else  // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)
struct CCallStackMsgContext;
#endif // (defined(_DEBUG) || defined(CALLSTK_MEASURETIMES)) && !defined(CALLSTK_DISABLEMEASURETIMES)

class CSalamanderDebugAbstract
{
public:
    // vypise 'file'+'line'+'str' TRACE_I na TRACE SERVER - pouze pri DEBUG/SDK/PB verzi Salamandera
    virtual void WINAPI TraceI(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceIW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // vypise 'file'+'line'+'str' TRACE_E na TRACE SERVER - pouze pri DEBUG/SDK/PB verzi Salamandera
    virtual void WINAPI TraceE(const char* file, int line, const char* str) = 0;
    virtual void WINAPI TraceEW(const WCHAR* file, int line, const WCHAR* str) = 0;

    // zaregistruje novy thread u TRACE (prideli Unique ID), 'thread'+'tid' vraci
    // _beginthreadex i CreateThread, nepovine (UID je pak -1)
    virtual void WINAPI TraceAttachThread(HANDLE thread, unsigned tid) = 0;

    // nastavi jmeno aktivniho threadu pro TRACE, nepovine (thread je oznacen jako "unknown")
    // POZOR: vyzaduje registraci threadu u TRACE (viz TraceAttachThread), jinak nic nedela
    virtual void WINAPI TraceSetThreadName(const char* name) = 0;
    virtual void WINAPI TraceSetThreadNameW(const WCHAR* name) = 0;

    // zavede do threadu veci potrebne pro CALL-STACK metody (viz Push a Pop nize),
    // ve vsech volanych metodach pluginu je mozne CALL_STACK metody pouzit primo,
    // tato metoda se pouziva pouze pro nove thready pluginu,
    // spousti funkci 'threadBody' s parametrem 'param', vraci vysledek funkce 'threadBody'
    virtual unsigned WINAPI CallWithCallStack(unsigned(WINAPI* threadBody)(void*), void* param) = 0;

    // uklada na CALL-STACK zpravu ('format'+'args' viz vsprintf), pri padu aplikace je
    // obsah CALL-STACKU vypsan do okna Bug Report ohlasujiciho pad aplikace
    virtual void WINAPI Push(const char* format, va_list args, CCallStackMsgContext* callStackMsgContext,
                             BOOL doNotMeasureTimes) = 0;

    // odstranuje z CALL-STACKU posledni zpravu, volani musi parovat s Push
    virtual void WINAPI Pop(CCallStackMsgContext* callStackMsgContext) = 0;

    // nastavi jmeno aktivniho threadu pro VC debugger
    virtual void WINAPI SetThreadNameInVC(const char* name) = 0;

    // vola TraceSetThreadName a SetThreadNameInVC pro 'name' (popis viz tyto dve metody)
    virtual void WINAPI SetThreadNameInVCAndTrace(const char* name) = 0;

    // Pokud jiz nejsme pripojeni na Trace Server, zkusi navazat spojeni (server
    // musi bezet). Jen SDK verze Salamandera (i Preview Buildy): pokud je povoleny
    // autostart serveru a server nebezi (napr. ho uzivatel ukoncil), zkusi ho pred
    // pripojenim nastartovat.
    virtual void WINAPI TraceConnectToServer() = 0;

    // vola se pro moduly, ve kterych se muzou hlasit memory leaky, pokud se detekuji memory leaky,
    // dojde k loadu "as image" (bez initu modulu) vsech takto registrovanych modulu (pri kontrole
    // memory leaku uz jsou tyto moduly unloadle), a pak teprve k vypisu memory leaku = jsou videt
    // jmena .cpp modulu misto "#File Error#"
    // mozne volat z libovolneho threadu
    virtual void WINAPI AddModuleWithPossibleMemoryLeaks(const char* fileName) = 0;
};

//
// ****************************************************************************
// CSalamanderRegistryAbstract
//
// sada metod Salamandera pro praci se systemovym registry,
// pouziva se v CPluginInterfaceAbstract::LoadConfiguration
// a CPluginInterfaceAbstract::SaveConfiguration

class CSalamanderRegistryAbstract
{
public:
    // vycisti klic 'key' od vsech podklicu a hodnot, vraci uspech
    virtual BOOL WINAPI ClearKey(HKEY key) = 0;

    // vytvori nebo otevre existujici podklic 'name' klice 'key', vraci 'createdKey' a uspech;
    // ziskany klic ('createdKey') je nutne zavrit volanim CloseKey
    virtual BOOL WINAPI CreateKey(HKEY key, const char* name, HKEY& createdKey) = 0;

    // otevre existujici podklic 'name' klice 'key', vraci 'openedKey' a uspech
    // ziskany klic ('openedKey') je nutne zavrit volanim CloseKey
    virtual BOOL WINAPI OpenKey(HKEY key, const char* name, HKEY& openedKey) = 0;

    // zavre klic otevreny pres OpenKey nebo CreateKey
    virtual void WINAPI CloseKey(HKEY key) = 0;

    // smaze podklic 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteKey(HKEY key, const char* name) = 0;

    // nacte hodnotu 'name'+'type'+'buffer'+'bufferSize' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetValue(HKEY key, const char* name, DWORD type, void* buffer, DWORD bufferSize) = 0;

    // ulozi hodnotu 'name'+'type'+'data'+'dataSize' do klice 'key', pro retezce je mozne
    // zadat 'dataSize' == -1 -> vypocet delky retezce pomoci funkce strlen,
    // vraci uspech
    virtual BOOL WINAPI SetValue(HKEY key, const char* name, DWORD type, const void* data, DWORD dataSize) = 0;

    // smaze hodnotu 'name' klice 'key', vraci uspech
    virtual BOOL WINAPI DeleteValue(HKEY key, const char* name) = 0;

    // vytahne do 'bufferSize' potrebnou velikost pro hodnotu 'name'+'type' z klice 'key', vraci uspech
    virtual BOOL WINAPI GetSize(HKEY key, const char* name, DWORD type, DWORD& bufferSize) = 0;
};

//
// ****************************************************************************
// CSalamanderConnectAbstract
//
// sada metod Salamandera pro navazani pluginu do Salamandera
// (custom pack/unpack + panel archiver view/edit + file viewer + menu-items)

// konstanty pro CSalamanderConnectAbstract::AddMenuItem
#define MENU_EVENT_TRUE 0x0001                    // nastane vzdy
#define MENU_EVENT_DISK 0x0002                    // zdroj je windows adresar ("c:\path" nebo UNC)
#define MENU_EVENT_THIS_PLUGIN_ARCH 0x0004        // zdroj je archiv tohoto pluginu
#define MENU_EVENT_THIS_PLUGIN_FS 0x0008          // zdroj je file-system tohoto pluginu
#define MENU_EVENT_FILE_FOCUSED 0x0010            // focus je na souboru
#define MENU_EVENT_DIR_FOCUSED 0x0020             // focus je na adresari
#define MENU_EVENT_UPDIR_FOCUSED 0x0040           // focus je na ".."
#define MENU_EVENT_FILES_SELECTED 0x0080          // jsou oznaceny soubory
#define MENU_EVENT_DIRS_SELECTED 0x0100           // jsou oznaceny adresare
#define MENU_EVENT_TARGET_DISK 0x0200             // cil je windows adresar ("c:\path" nebo UNC)
#define MENU_EVENT_TARGET_THIS_PLUGIN_ARCH 0x0400 // cil je archiv tohoto pluginu
#define MENU_EVENT_TARGET_THIS_PLUGIN_FS 0x0800   // cil je file-system tohoto pluginu
#define MENU_EVENT_SUBDIR 0x1000                  // adresar neni root (obsahuje "..")
// fokus je na souboru, pro ktery tento plugin zajistuje "panel archiver view" nebo "panel archiver edit"
#define MENU_EVENT_ARCHIVE_FOCUSED 0x2000
// uz je k dispozici jen 0x4000 (masky se skladaji obe do DWORDu a predtim se maskuji 0x7FFF)

// urcuje pro ktereho uzivatele je polozka urcena
#define MENU_SKILLLEVEL_BEGINNER 0x0001     // urceno pro nejdulezitejsi polozky menu, urcene pro zacatecniky
#define MENU_SKILLLEVEL_INTERMEDIATE 0x0002 // nastavovat i mene frekventovanym prikazum; pro pokrocilejsi uzivatele
#define MENU_SKILLLEVEL_ADVANCED 0x0004     // nastavovat vsem prikazum (profici by meli mit v menu vse)
#define MENU_SKILLLEVEL_ALL 0x0007          // pomocna konstanta slucujici vsechny predesle

// makro pro pripravu 'HotKey' pro AddMenuItem()
// LOWORD - hot key (virtual key + modifikatory) (LOBYTE - virtual key, HIBYTE - modifikatory)
// mods: kombinace HOTKEYF_CONTROL, HOTKEYF_SHIFT, HOTKEYF_ALT
// examples: SALHOTKEY('A', HOTKEYF_CONTROL | HOTKEYF_SHIFT), SALHOTKEY(VK_F1, HOTKEYF_CONTROL | HOTKEYF_ALT | HOTKEYF_EXT)
//#define SALHOTKEY(vk,mods,cst) ((DWORD)(((BYTE)(vk)|((WORD)((BYTE)(mods))<<8))|(((DWORD)(BYTE)(cst))<<16)))
#define SALHOTKEY(vk, mods) ((DWORD)(((BYTE)(vk) | ((WORD)((BYTE)(mods)) << 8))))

// makro pro pripravu 'hotKey' pro AddMenuItem()
// rika Salamanderu, ze polozky menu bude obsahovat horkou klavesu (oddelenou znakem '\t')
// Salamander nebude v tomto pripade kricet pomoci TRACE_E a horkou klavesu zobrazi v menu Plugins
// POZOR: nejedna se o horkou klavesu, kterou by Salamander dorucil pluginu, jde skutecne pouze o napis
// pokud uzivatel priradi v Plugin Manageru tomuto commandu vlastni horkou klavesu, bude hint potlacen
#define SALHOTKEY_HINT ((DWORD)0x00020000)

class CSalamanderConnectAbstract
{
public:
    // pridani pluginu do seznamu pro "custom archiver pack",
    // 'title' je nazev custom packeru pro uzivatele, 'defaultExtension' je standardni pripona
    // pro nove archivy, pokud nejde o upgrade "custom pack" (nebo pridani celeho pluginu) a
    // 'update' je FALSE, je volani ignorovano; je-li 'update' TRUE, prepise se nastaveni na
    // nove hodnoty 'title' a 'defaultExtension' - nutna prevence proti opakovanemu 'update'==TRUE
    // (neustalemu prepisovani nastaveni)
    virtual void WINAPI AddCustomPacker(const char* title, const char* defaultExtension, BOOL update) = 0;

    // pridani pluginu do seznamu pro "custom archiver unpack",
    // 'title' je nazev custom unpackeru pro uzivatele, 'masks' jsou masky souboru archivu (hleda
    // se podle nich cim rozpakovavat dany archiv, oddelovac je ';' (escape sekvence pro ';' je
    // ";;") a pouzivaji se klasicky wildcards '*' a '?' plus '#' pro '0'..'9'), pokud nejde o upgrade
    // "custom unpack" (nebo pridani celeho pluginu) a 'update' je FALSE je volani ignorovano;
    // je-li 'update' TRUE, prepise se nastaveni na nove hodnoty 'title' a 'masks' - nutna prevence
    // proti opakovanemu 'update'==TRUE (neustalemu prepisovani nastaveni)
    virtual void WINAPI AddCustomUnpacker(const char* title, const char* masks, BOOL update) = 0;

    // pridani pluginu do seznamu pro "panel archiver view/edit",
    // 'extensions' jsou pripony archivu, ktere se timto pluginem maji zpracovavat
    // (oddelovac je ';' (zde nema ';' zadnou escape sekvenci) a pouziva se wildcard '#' pro
    // '0'..'9'), pokud je 'edit' TRUE, resi tento plugin "panel archiver view/edit", jinak jen
    // "panel archiver view", pokud nejde o upgrade "panel archiver view/edit" (nebo pridani
    // celeho pluginu) a 'updateExts' je FALSE je volani ignorovano; je-li 'updateExts' TRUE,
    // jde o pridani novych pripon archivu (zajisteni pritomnosti vsech pripon z 'extensions') - nutna
    // prevence proti opakovanemu 'updateExts'==TRUE (neustalemu ozivovani pripon z 'extensions')
    virtual void WINAPI AddPanelArchiver(const char* extensions, BOOL edit, BOOL updateExts) = 0;

    // vyhozeni pripony ze seznamu pro "panel archiver view/edit" (jen z polozek tykajicich se
    // tohoto pluginu), 'extension' je pripona archivu (jedina; pouziva se wildcard '#' pro '0'..'9'),
    // nutna prevence proti opakovanemu volani (neustalemu mazani 'extension')
    virtual void WINAPI ForceRemovePanelArchiver(const char* extension) = 0;

    // pridani pluginu do seznamu pro "file viewer",
    // 'masks' jsou pripony vieweru, ktere se timto pluginem maji zpracovavat
    // (oddelovac je ';' (escape sekvence pro ';' je ";;") a pouzivaji se wildcardy '*' a '?',
    // pokud mozno nepouzivat mezery + znak '|' je zakazany (inverzni masky nejsou povolene)),
    // pokud nejde o upgrade "file viewer" (nebo pridani celeho pluginu) a 'force' je FALSE,
    // je volani ignorovano; je-li 'force' TRUE, prida 'masks' vzdy (pokud jiz nejsou na
    // seznamu) - nutna prevence proti opakovanemu 'force'==TRUE (neustalemu pridavani 'masks')
    virtual void WINAPI AddViewer(const char* masks, BOOL force) = 0;

    // vyhozeni masky ze seznamu pro "file viewer" (jen z polozek tykajicich se tohoto pluginu),
    // 'mask' je pripona viewru (jedina; pouzivaji se wildcardy '*' a '?'), nutna prevence
    // proti opakovanemu volani (neustalemu mazani 'mask')
    virtual void WINAPI ForceRemoveViewer(const char* mask) = 0;

    // pridava polozky do menu Plugins/"jmeno pluginu" v Salamanderu, 'iconIndex' je index
    // ikony polozky (-1=zadna ikona; zadani bitmapy s ikonami viz
    // CSalamanderConnectAbstract::SetBitmapWithIcons; u separatoru se ignoruje), 'name' je
    // jmeno polozky (max. MAX_PATH - 1 znaku) nebo NULL jde-li o separator (parametry
    // 'state_or'+'state_and' v tomto pripade nemaji vyznam); 'hotKey' je horka klavesa
    // polozky ziskana pomoci makra SALHOTKEY; 'name' muze obsahovat hint pro horkou klavesu,
    // oddeleny znakem '\t', v promenne 'hotKey' musi byt v tomto pripade prirazena konstanta
    // SALHOTKEY_HINT, vice viz komentar k SALHOTKEY_HINT; 'id' je unikatni identifikacni
    // cislo polozky v ramci pluginu (u separatoru ma vyznam jen je-li 'callGetState' TRUE),
    // pokud je 'callGetState' TRUE, vola se pro zjisteni stavu polozky metoda
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (u separatoru ma vyznam jen stav
    // MENU_ITEM_STATE_HIDDEN, ostatni se ignoruji), jinak se k vypoctu stavu polozky (enabled/disabled)
    // pouziji 'state_or'+'state_and' - pri vypoctu stavu polozky se nejprve sestavi maska
    // ('eventMask') tak, ze se logicky sectou vsechny udalosti, ktere nastaly (udalosti viz
    // MENU_EVENT_XXX), polozka bude "enable" pokud bude nasl. vyraz TRUE:
    //   ('eventMask' & 'state_or') != 0 && ('eventMask' & 'state_and') == 'state_and',
    // parametr 'skillLevel' urcuje kterym urovnim uzivatele bude polozka (nebo separator)
    // zobrazena; hodnota obsahuje jednu nebo vice (ORovane) konstant MENU_SKILLLEVEL_XXX;
    // polozky v menu se updatuji pri kazdem loadu pluginu (mozna zmena polozek dle konfigurace)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddMenuItem
    virtual void WINAPI AddMenuItem(int iconIndex, const char* name, DWORD hotKey, int id, BOOL callGetState,
                                    DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // prida submenu do menu Plugins/"jmeno pluginu" v Salamanderu, 'iconIndex'
    // je index ikony submenu (-1=zadna ikona; zadani bitmapy s ikonami
    // viz CSalamanderConnectAbstract::SetBitmapWithIcons), 'name' je jmeno
    // submenu (max. MAX_PATH - 1 znaku), 'id' je unikatni identifikacni cislo polozky
    // menu v ramci pluginu (u submenu ma vyznam jen je-li 'callGetState' TRUE),
    // pokud je 'callGetState' TRUE, vola se pro zjisteni stavu submenu metoda
    // CPluginInterfaceForMenuExtAbstract::GetMenuItemState (vyznam maji jen stavy
    // MENU_ITEM_STATE_ENABLED a MENU_ITEM_STATE_HIDDEN, ostatni se ignoruji), jinak se
    // k vypoctu stavu polozky (enabled/disabled) pouziji 'state_or'+'state_and' - vypocet
    // stavu polozky viz CSalamanderConnectAbstract::AddMenuItem(), parametr 'skillLevel'
    // urcuje kterym urovnim uzivatele bude submenu zobrazeno, hodnota obsahuje jednu nebo
    // vice (ORovanych) konstant MENU_SKILLLEVEL_XXX, submenu se ukoncuje volanim
    // CSalamanderConnectAbstract::AddSubmenuEnd();
    // polozky v menu se updatuji pri kazdem loadu pluginu (mozna zmena polozek dle konfigurace)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddSubmenuStart
    virtual void WINAPI AddSubmenuStart(int iconIndex, const char* name, int id, BOOL callGetState,
                                        DWORD state_or, DWORD state_and, DWORD skillLevel) = 0;

    // ukonci submenu v menu Plugins/"jmeno pluginu" v Salamanderu, dalsi polozky budou
    // pridavany do vyssi (rodicovske) urovne menu;
    // polozky v menu se updatuji pri kazdem loadu pluginu (mozna zmena polozek dle konfigurace)
    // POZOR: pro "dynamic menu extension" se pouziva CSalamanderBuildMenuAbstract::AddSubmenuEnd
    virtual void WINAPI AddSubmenuEnd() = 0;

    // nastavuje polozku pro FS v Change Drive menu a v Drive barach; 'title' je jeji text,
    // 'iconIndex' je index jeji ikony (-1=zadna ikona; zadani bitmapy s ikonami viz
    // CSalamanderConnectAbstract::SetBitmapWithIcons), 'title' muze obsahovat az tri sloupce
    // vzajemne oddelene '\t' (viz Alt+F1/F2 menu); viditelnost polozky je mozne nastavit
    // z Plugins Manageru nebo primo z pluginu pomoci metody
    // CSalamanderGeneralAbstract::SetChangeDriveMenuItemVisibility
    virtual void WINAPI SetChangeDriveMenuItem(const char* title, int iconIndex) = 0;

    // informuje Salamandera, ze plugin umi nacitat thumbnaily ze souboru odpovidajicich
    // skupinove masce 'masks' (oddelovac je ';' (escape sekvence pro ';' je ";;") a pouzivaji
    // se wildcardy '*' a '?'); pro nacteni thumbnailu se vola
    // CPluginInterfaceForThumbLoaderAbstract::LoadThumbnail
    virtual void WINAPI SetThumbnailLoader(const char* masks) = 0;

    // nastavi bitmapu s ikonami pluginu; Salamander si obsah bitmapy kopiruje do internich
    // struktur, plugin je zodpovedny za destrukci bitmapy (ze strany Salamanadera se
    // bitmapa pouzije pouze behem teto funkce); pocet ikon se odvozuje ze
    // sirky bitmapy, ikony jsou vzdy 16x16 bodu; transparentni cast ikon tvori fialova
    // barva (RGB(255,0,255)), barevna hloubka bitmapy muze byt 4 nebo 8 bitu (16 nebo 256
    // barev), idealni je mit pripravene obe barevne varianty a vybirat z nich podle
    // vysledku metody CSalamanderGeneralAbstract::CanUse256ColorsBitmap()
    // POZOR: tato metoda je zastarala, nepodporuje alpha transparenci, pouzijte misto ni
    //        SetIconListForGUI()
    virtual void WINAPI SetBitmapWithIcons(HBITMAP bitmap) = 0;

    // nastavi index ikony pluginu, ktera se pro plugin pouziva v okne Plugins/Plugins Manager,
    // v menu Help/About Plugin a pripadne i pro submenu pluginu v menu Plugins (detaily
    // viz CSalamanderConnectAbstract::SetPluginMenuAndToolbarIcon()); pokud plugin tuto
    // metodu nezavola, pouzije se standardni Salamanderovska ikona pro plugin; 'iconIndex'
    // je nastavovany index ikony (zadani bitmapy s ikonami viz
    // CSalamanderConnectAbstract::SetBitmapWithIcons)
    virtual void WINAPI SetPluginIcon(int iconIndex) = 0;

    // nastavi index ikony pro submenu pluginu, ktera se pouziva pro submenu pluginu
    // v menu Plugins a pripadne i v horni toolbare pro drop-down button slouzici
    // k zobrazeni submenu pluginu; pokud plugin tuto metodu nezavola, pouzije se
    // pro submenu pluginu v menu Plugins ikona pluginu (nastaveni viz
    // CSalamanderConnectAbstract::SetPluginIcon) a v horni toolbare se neobjevi
    // button pro plugin; 'iconIndex' je nastavovany index ikony (-1=ma se pouzit ikona
    // pluginu, viz CSalamanderConnectAbstract::SetPluginIcon(); zadani bitmapy
    // s ikonami viz CSalamanderConnectAbstract::SetBitmapWithIcons);
    virtual void WINAPI SetPluginMenuAndToolbarIcon(int iconIndex) = 0;

    // nastavi bitmapu s ikonami pluginu; bitmapu je treba alokovat pomoci volani
    // CSalamanderGUIAbstract::CreateIconList() a nasledne vytvorit a naplnit pomoci
    // metod CGUIIconListAbstract interfacu; rozmery ikonek musi byt 16x16 bodu;
    // Salamander si objekt bitmapy prebira do sve spravy, plugin ji po zavolani
    // teto funkce nesmi destruovat; bitmapa se uklada do konfigurace Salamandera,
    // aby se ikony pri dalsim spusteni daly pouzivat bez loadu pluginu, proto do
    // ni vkladejte pouze potrebne ikony
    virtual void WINAPI SetIconListForGUI(CGUIIconListAbstract* iconList) = 0;
};

//
// ****************************************************************************
// CDynamicString
//
// dynamicky string: realokuje se podle potreby

class CDynamicString
{
public:
    // vraci TRUE pokud se retezec 'str' o delce 'len' podarilo pridat; je-li 'len' -1,
    // urci se 'len' jako "strlen(str)" (pridani bez koncove nuly); je-li 'len' -2,
    // urci se 'len' jako "strlen(str)+1" (pridani vcetne koncove nuly)
    virtual BOOL WINAPI Add(const char* str, int len = -1) = 0;
};

//
// ****************************************************************************
// CPluginInterfaceAbstract
//
// sada metod pluginu, ktere potrebuje Salamander pro praci s pluginem
//
// Pro vetsi prehlednost jsou oddelene casti pro:
// archivatory - viz CPluginInterfaceForArchiverAbstract,
// viewry - viz CPluginInterfaceForViewerAbstract,
// rozsireni menu - viz CPluginInterfaceForMenuExtAbstract,
// file-systemy - viz CPluginInterfaceForFSAbstract,
// nacitace thumbnailu - viz CPluginInterfaceForThumbLoaderAbstract.
// Casti jsou pripojeny k CPluginInterfaceAbstract pres CPluginInterfaceAbstract::GetInterfaceForXXX

// flagy oznacujici, ktere funkce plugin poskytuje (jake metody potomka
// CPluginInterfaceAbstract jsou v pluginu skutecne implementovane):
#define FUNCTION_PANELARCHIVERVIEW 0x0001     // metody pro "panel archiver view"
#define FUNCTION_PANELARCHIVEREDIT 0x0002     // metody pro "panel archiver edit"
#define FUNCTION_CUSTOMARCHIVERPACK 0x0004    // metody pro "custom archiver pack"
#define FUNCTION_CUSTOMARCHIVERUNPACK 0x0008  // metody pro "custom archiver unpack"
#define FUNCTION_CONFIGURATION 0x0010         // metoda Configuration
#define FUNCTION_LOADSAVECONFIGURATION 0x0020 // metody pro "load/save configuration"
#define FUNCTION_VIEWER 0x0040                // metody pro "file viewer"
#define FUNCTION_FILESYSTEM 0x0080            // metody pro "file system"
#define FUNCTION_DYNAMICMENUEXT 0x0100        // metody pro "dynamic menu extension"

// kody ruznych udalosti (a vyznam parametru 'param'), prijima metoda CPluginInterfaceAbstract::Event():
// doslo ke zmene barev (diky zmene systemovych barev / WM_SYSCOLORCHANGE nebo diky zmene v konfiguraci); plugin si
// muze vytahnout nove verze salamanderovskych barev pres CSalamanderGeneralAbstract::GetCurrentColor;
// pokud ma plugin file-system s ikonami typu pitFromPlugin, mel by prebarvit pozadi image-listu
// s jednoduchymi ikonami na barvu SALCOL_ITEM_BK_NORMAL; 'param' se zde ignoruje
#define PLUGINEVENT_COLORSCHANGED 0

// doslo ke zmene konfigurace Salamandera; plugin si muze vytahnout nove verze salamanderovskych
// konfiguracnich parametru pres CSalamanderGeneralAbstract::GetConfigParameter;
// 'param' se zde ignoruje
#define PLUGINEVENT_CONFIGURATIONCHANGED 1

// doslo k prohozeni leveho a praveho panelu (Swap Panels - Ctrl+U)
// 'param' se zde ignoruje
#define PLUGINEVENT_PANELSSWAPPED 2

// doslo ke zmene aktivniho panelu (prepnuti mezi panely)
// 'param' je PANEL_LEFT nebo PANEL_RIGHT - oznacuje aktivovany panel
#define PLUGINEVENT_PANELACTIVATED 3

// Salamander obdrzel WM_SETTINGCHANGE a na jeho zaklade pregeneroval fonty pro toolbars.
// Potom rozesila tento event vsem pluginu, aby mely moznost zavolat svym nastrojovym listam
// metodu SetFont();
// 'param' se zde ignoruje
#define PLUGINEVENT_SETTINGCHANGE 4

// kody udalosti v Password Manageru, prijima metoda CPluginInterfaceAbstract::PasswordManagerEvent():
#define PME_MASTERPASSWORDCREATED 1 // uzivatel vytvoril master password (je potreba zasifrovat hesla)
#define PME_MASTERPASSWORDCHANGED 2 // uzivatel zmenil master password (je potreba desifrovat a nasledne znovu zasifrovat hesla)
#define PME_MASTERPASSWORDREMOVED 3 // uzivatel zrusil master password (je potreba desifrovat hesla)

class CPluginInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginInterfaceEncapsulation)
    friend class CPluginInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // vola se jako reakce na tlacitko About v okne Plugins nebo prikaz z menu Help/About Plugins
    virtual void WINAPI About(HWND parent) = 0;

    // vola se pred unloadem pluginu (prirozene jen pokud vratil SalamanderPluginEntry
    // tento objekt a ne NULL), vraci TRUE pokud unload muze probehnout,
    // 'parent' je parent messageboxu, 'force' je TRUE pokud se nebere ohled na navratovou
    // hodnotu, pokud vraci TRUE, nebude se tento objekt a vsechny ostatni z nej ziskane
    // dale pouzivat a dojde k unloadu pluginu; pokud probiha critical shutdown (vice viz
    // CSalamanderGeneralAbstract::IsCriticalShutdown), nema smysl se usera na cokoliv ptat
    // (uz neotvirat zadna okna)
    // POZOR!!! Je nutne ukoncit vsechny thready pluginu (pokud Release vrati TRUE, vola se
    // FreeLibrary na pluginove .SPL => kod pluginu se odmapuje z pameti => thready pak uz
    // nemaji co spoustet => obvykle nevypadne ani bug-report, ani Windows exception info)
    virtual BOOL WINAPI Release(HWND parent, BOOL force) = 0;

    // funkce pro load defaultni konfigurace a pro "load/save configuration" (load ze soukromeho klice
    // pluginu v registry), 'parent' je parent messageboxu, je-li 'regKey' == NULL, jde o
    // defaultni konfiguraci, 'registry' je objekt pro praci s registry, tato metoda se vola vzdy
    // po SalamanderPluginEntry a pred ostatnimi volanimi (vola se load ze soukromeho klice, je-li
    // tato funkce pluginem poskytovana a klic v registry existuje, jinak vola jen load defaultni
    // konfigurace)
    virtual void WINAPI LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // funkce pro "load/save configuration", vola se pro ulozeni konfigurace pluginu do jeho soukromeho
    // klice v registry, 'parent' je parent messageboxu, 'registry' je objekt pro praci s registry,
    // uklada-li Salamander konfiguraci, vola take tuto metodu (je-li pluginem poskytovana); Salamander
    // tez nabizi ukladani konfigurace pluginu pri jeho unloadu (napr. rucne z Plugins Manageru),
    // v tomto pripade se ulozeni provede jen pokud v registry existuje klic Salamandera
    virtual void WINAPI SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry) = 0;

    // vola se jako reakce na tlacitko Configurate v okne Plugins
    virtual void WINAPI Configuration(HWND parent) = 0;

    // vola se pro navazani pluginu do Salamandera, vola se az po LoadConfiguration,
    // 'parent' je parent messageboxu, 'salamander' je sada metod pro navazovani pluginu

    /*  PRAVIDLA PRO IMPLEMENTACI METODY CONNECT
        (pluginy musi mit ulozenou verzi konfigurace - viz DEMOPLUGin,
         promenna ConfigVersion a konstanta CURRENT_CONFIG_VERSION; nize je
         nazorny PRIKLAD pridani pripony "dmp2" do DEMOPLUGinu):

      -s kazdou zmenou je potreba zvysit cislo verze konfigurace - CURRENT_CONFIG_VERSION
       (v prvni verzi metody Connect je CURRENT_CONFIG_VERSION=1)
      -do zakladni casti (pred podminky "if (ConfigVersion < YYY)"):
        -se napise kod pro instalaci pluginu (uplne prvni load pluginu):
         viz metody CSalamanderConnectAbstract
        -pri upgradech je nutne aktualizovat seznamy pripon pro instalaci pro "custom archiver
         unpack" (AddCustomUnpacker), "panel archiver view/edit" (AddPanelArchiver),
         "file viewer" (AddViewer), polozky menu (AddMenuItem), atd.
        -u volani AddPanelArchiver a AddViewer nechame 'updateExts' a 'force' FALSE
         (jinak bysme uzivateli nutili nejen nove, ale i stare pripony, ktere uz treba
         rucne smazal)
        -u volani AddCustomPacker/AddCustomUnpacker dame do parametru 'update' podminku
         "ConfigVersion < XXX", kde XXX je cislo posledni verze, kde se menily
         pripony pro custom packery/unpackery (obe volani je potreba posuzovat zvlast;
         zde pro jednoduchost vnutime uzivateli vsechny pripony, pokud si nejake promazal
         nebo pridal, ma smulu, bude to muset udelat rucne znovu)
        -AddMenuItem, SetChangeDriveMenuItem a SetThumbnailLoader funguje pri kazdem loadu
         pluginu stejne (instalace/upgrady se nelisi - vzdy se zacina na zelene louce)
      -jen pri upgradech: do casti pro upgrady (za zakladni casti):
        -pridame podminku "if (ConfigVersion < XXX)", kde XXX je nova hodnota
         konstanty CURRENT_CONFIG_VERSION + pridame komentar od teto verze;
         v tele teto podminky zavolame:
          -pokud pribyly pripony pro "panel archiver", zavolame
           "AddPanelArchiver(PPP, EEE, TRUE)", kde PPP jsou jen nove pripony oddelene
           strednikem a EEE je TRUE/FALSE ("panel view+edit"/"jen panel view")
          -pokud pribyly pripony pro "viewer", zavolame "AddViewer(PPP, TRUE)",
           kde PPP jsou jen nove pripony oddelene strednikem
          -pokud se maji smazat nejake stare pripony pro "viewer", zavolame
           pro kazdou takovou priponu PPP "ForceRemoveViewer(PPP)"
          -pokud se maji smazat nejake stare pripony pro "panel archiver", zavolame
           pro kazdou takovou priponu PPP "ForceRemovePanelArchiver(PPP)"

      KONTROLA: po techto upravach doporucuji vyzkouset, jestli to funguje, jak ma,
                staci nakompilovat plugin a zkusit ho naloadit do Salama, melo by
                dojit k automatickemu upgradu z predesle verze (bez potreby
                vyhozeni a pridani pluginu):
                -viz menu Options/Configuration:
                  -Viewery jsou na strance Viewers: najdete pridane pripony,
                   zkontrolujte, ze odebrane pripony jiz neexistuji
                  -Panel Archivers jsou na strance Archives Associations in Panels:
                   najdete pridane pripony
                  -Custom Unpackers jsou na strance Unackers in Unpack Dialog Box:
                   najdete vas plugin a zkontrolujte, jestli je seznam masek OK
                -zkontrolovat novou podobu submenu pluginu (v menu Plugins)
                -zkontrolovat novou podobu Change Drive menu (Alt+F1/F2)
                -zkontrolovat v Plugins Manageru (v menu Plugins) masky thumbnaileru:
                 fokusnout vas plugin, pak zkontrolovat editbox "Thumbnails"
              +nakonec muzete jeste taky zkusit vyhodit a pridat plugin, jestli
               funguje "instalace" pluginu: kontrola viz vsechny predesle body

      POZNAMKA: pri pridani pripon pro "panel archiver" je tez potreba doplnit
                seznam pripon v parametru 'extensions' metody SetBasicPluginData

      PRIKLAD PRIDANI PRIPONY "dmp2" VIEWERU A ARCHIVERU:
        (radky zacinajici na "-" byly odstraneny, radky zacinajici na "+" pridany,
         symbol "=====" na zacatku radky znaci preruseni souvisleho useku kodu)
        Prehled zmen:
          -zvysila se verze konfigurace z 2 na 3:
            -pridany komentar k verzi 3
            -zvyseni CURRENT_CONFIG_VERSION na 3
          -pridani pripony "dmp2" do parametru 'extensions' SetBasicPluginData
           (protoze pridavame priponu "dmp2" pro "panel archiver")
          -pridani masky "*.dmp2" do AddCustomUnpacker + zvyseni verze z 1 na 3
           v podmince (protoze pridavame priponu "dmp2" pro "custom unpacker")
          -pridani pripony "dmp2" do AddPanelArchiver (protoze pridavame priponu
           "dmp2" pro "panel archiver")
          -pridani masky "*.dmp2" do AddViewer (protoze pridavame priponu "dmp2"
           pro "viewer")
          -pridani podminky pro upgrade na verzi 3 + komentar tohoto upgradu,
           telo podminky:
            -volani AddPanelArchiver pro priponu "dmp2" s 'updateExts' TRUE
             (protoze pridavame priponu "dmp2" pro "panel archiver")
            -volani AddViewer pro masku "*.dmp2" s 'force' TRUE (protoze
             pridavame priponu "dmp2" pro "viewer")
=====
  // ConfigVersion: 0 - zadna konfigurace se z Registry nenacetla (jde o instalaci pluginu),
  //                1 - prvni verze konfigurace
  //                2 - druha verze konfigurace (pridane nejake hodnoty do konfigurace)
+ //                3 - treti verze konfigurace (pridani pripony "dmp2")

  int ConfigVersion = 0;
- #define CURRENT_CONFIG_VERSION 2
+ #define CURRENT_CONFIG_VERSION 3
  const char *CONFIG_VERSION = "Version";
=====
  // nastavime zakladni informace o pluginu
  salamander->SetBasicPluginData("Salamander Demo Plugin",
                                 FUNCTION_PANELARCHIVERVIEW | FUNCTION_PANELARCHIVEREDIT |
                                 FUNCTION_CUSTOMARCHIVERPACK | FUNCTION_CUSTOMARCHIVERUNPACK |
                                 FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION |
                                 FUNCTION_VIEWER | FUNCTION_FILESYSTEM,
                                 "2.0",
                                 "Copyright © 1999-2023 Open Salamander Authors",
                                 "This plugin should help you to make your own plugins.",
-                                "DEMOPLUG", "dmp", "dfs");
+                                "DEMOPLUG", "dmp;dmp2", "dfs");
=====
  void WINAPI
  CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract *salamander)
  {
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // zakladni cast:
    salamander->AddCustomPacker("DEMOPLUG (Plugin)", "dmp", FALSE);
-   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp", ConfigVersion < 1);
+   salamander->AddCustomUnpacker("DEMOPLUG (Plugin)", "*.dmp;*.dmp2", ConfigVersion < 3);
-   salamander->AddPanelArchiver("dmp", TRUE, FALSE);
+   salamander->AddPanelArchiver("dmp;dmp2", TRUE, FALSE);
-   salamander->AddViewer("*.dmp", FALSE);
+   salamander->AddViewer("*.dmp;*.dmp2", FALSE);
===== (vynechal jsem pridavani polozek do menu, nastavovani ikon a masek thumbnailu)
    // cast pro upgrady:
+   if (ConfigVersion < 3)   // verze 3: pridani pripony "dmp2"
+   {
+     salamander->AddPanelArchiver("dmp2", TRUE, TRUE);
+     salamander->AddViewer("*.dmp2", TRUE);
+   }
  }
=====
    */
    virtual void WINAPI Connect(HWND parent, CSalamanderConnectAbstract* salamander) = 0;

    // uvolni interface 'pluginData', ktery Salamander ziskal z pluginu pomoci volani
    // CPluginInterfaceForArchiverAbstract::ListArchive nebo
    // CPluginFSInterfaceAbstract::ListCurrentPath; pred timto volanim jeste
    // dojde k uvolneni dat souboru a adresaru (CFileData::PluginData) pomoci metod
    // CPluginDataInterfaceAbstract
    virtual void WINAPI ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData) = 0;

    // vraci interface archivatoru; plugin musi vracet tento interface, pokud ma
    // alespon jednu z nasledujicich funkci (viz SetBasicPluginData): FUNCTION_PANELARCHIVERVIEW,
    // FUNCTION_PANELARCHIVEREDIT, FUNCTION_CUSTOMARCHIVERPACK a/nebo FUNCTION_CUSTOMARCHIVERUNPACK;
    // pokud plugin archivator neobsahuje, vraci NULL
    virtual CPluginInterfaceForArchiverAbstract* WINAPI GetInterfaceForArchiver() = 0;

    // vraci interface vieweru; plugin musi vracet tento interface, pokud ma funkci
    // (viz SetBasicPluginData) FUNCTION_VIEWER; pokud plugin viewer neobsahuje, vraci NULL
    virtual CPluginInterfaceForViewerAbstract* WINAPI GetInterfaceForViewer() = 0;

    // vraci interface rozsireni menu; plugin musi vracet tento interface, pokud pridava
    // polozky do menu (viz CSalamanderConnectAbstract::AddMenuItem) nebo pokud ma
    // funkci (viz SetBasicPluginData) FUNCTION_DYNAMICMENUEXT; v opacnem pripade vraci NULL
    virtual CPluginInterfaceForMenuExtAbstract* WINAPI GetInterfaceForMenuExt() = 0;

    // vraci interface file-systemu; plugin musi vracet tento interface, pokud ma funkci
    // (viz SetBasicPluginData) FUNCTION_FILESYSTEM; pokud plugin file-system neobsahuje, vraci NULL
    virtual CPluginInterfaceForFSAbstract* WINAPI GetInterfaceForFS() = 0;

    // vraci interface nacitace thumbnailu; plugin musi vracet tento interface, pokud informoval
    // Salamandera, ze umi nacitat thumbnaily (viz CSalamanderConnectAbstract::SetThumbnailLoader);
    // pokud plugin neumi nacitat thumbnaily, vraci NULL
    virtual CPluginInterfaceForThumbLoaderAbstract* WINAPI GetInterfaceForThumbLoader() = 0;

    // prijem ruznych udalosti, viz kody udalosti PLUGINEVENT_XXX; vola se jen pokud je plugin
    // nacteny; 'param' je parametr udalosti
    // POZOR: muze se zavolat kdykoliv po dokonceni entry-pointu pluginu (SalamanderPluginEntry)
    virtual void WINAPI Event(int event, DWORD param) = 0;

    // uzivatel si preje, aby byly smazany vsechny historie (spustil Clear History z konfigurace
    // ze stranky History); historii se tu mysli vse, co vznika samocinne z uzivatelem zadanych
    // hodnot (napr. seznam textu spustenych v prikazove radce, seznam aktualnich cest na
    // jednotlivych drivech, atd.); nepatri sem seznamy tvorene userem - napr. hot-paths, user-menu,
    // atd.; 'parent' je parent pripadnych messageboxu; po ulozeni konfigurace nesmi historie
    // zustat v registry; pokud ma plugin otevrena okna obsahujici historie (comboboxy), musi
    // promazat historie take tam
    virtual void WINAPI ClearHistory(HWND parent) = 0;

    // prijem informace o zmene na ceste 'path' (je-li 'includingSubdirs' TRUE, tak
    // zahrnuje i zmenu v podadresarich cesty 'path'); teto metody je mozne vyuzit napr.
    // k invalidovani/cisteni cache souboru/adresaru; POZNAMKA: pro pluginove file-systemy (FS)
    // existuje metoda CPluginFSInterfaceAbstract::AcceptChangeOnPathNotification()
    virtual void WINAPI AcceptChangeOnPathNotification(const char* path, BOOL includingSubdirs) = 0;

    // tato metoda se vola jen u pluginu, ktery pouziva Password Manager (viz
    // CSalamanderGeneralAbstract::SetPluginUsesPasswordManager()):
    // informuje plugin o zmenach v Password Manageru; 'parent' je parent pripadnych
    // messageboxu/dialogu; 'event' obsahuje udalost, viz PME_XXX
    virtual void WINAPI PasswordManagerEvent(HWND parent, int event) = 0;
};

//
// ****************************************************************************
// CSalamanderPluginEntryAbstract
//
// sada metod ze Salamandera, ktere se pouzivaji v SalamanderPluginEntry

// flagy informujici o duvodu loadu pluginu (viz metoda CSalamanderPluginEntryAbstract::GetLoadInformation)
#define LOADINFO_INSTALL 0x0001          // prvni load pluginu (instalace do Salamandera)
#define LOADINFO_NEWSALAMANDERVER 0x0002 // nova verze Salamandera (doinstalace vsech pluginu z \
                                         // podadresare plugins), loadi vsechny pluginy (mozny \
                                         // upgrade vsech)
#define LOADINFO_NEWPLUGINSVER 0x0004    // zmena v souboru plugins.ver (instalace/upgrade pluginu), \
                                         // pro jednoduchost loadi vsechny pluginy (mozny upgrade \
                                         // vsech)
#define LOADINFO_LOADONSTART 0x0008      // load probehl, protoze byl nalezen flag "load on start"

class CSalamanderPluginEntryAbstract
{
public:
    // vraci verzi Salamandera, viz spl_vers.h, konstanty LAST_VERSION_OF_SALAMANDER a REQUIRE_LAST_VERSION_OF_SALAMANDER
    virtual int WINAPI GetVersion() = 0;

    // vraci "parent" okno Salamandera (parent pro messageboxy)
    virtual HWND WINAPI GetParentWindow() = 0;

    // vraci ukazatel na interface k debugovacim funkcim Salamandera,
    // interface je platny po celou dobu existence pluginu (nejen v ramci
    // "SalamanderPluginEntry" funkce) a jde pouze o odkaz, takze se neuvolnuje
    virtual CSalamanderDebugAbstract* WINAPI GetSalamanderDebug() = 0;

    // nastaveni zakladnich dat o pluginu (data, ktera si o pluginu spolu se jmenem DLL souboru
    // Salamander pamatuje), nutne volat, jinak nemuze byt plugin pripojen;
    // 'pluginName' je jmeno pluginu; 'functions' obsahuje naORovane vsechny funkce, ktere plugin
    // podporuje (viz konstanty FUNCTION_XXX); 'version'+'copyright'+'description' jsou data pro
    // uzivatele zobrazovana v okne Plugins; 'regKeyName' je navrhovany nazev soukromeho klice
    // pro ulozeni konfigurace v registry (bez FUNCTION_LOADSAVECONFIGURATION je ignorovan);
    // 'extensions' jsou zakladni pripony (napr. jen "ARJ"; "A01", atd. uz ne) zpracovavanych
    // archivu oddelene ';' (zde nema ';' zadnou escape sekvenci) - Salamander tyto pripony pouziva
    // jen pri hledani nahrady za odstranene panelove archivatory (nastava pri odstraneni pluginu;
    // resi se problem "co se ted postara o priponu XXX, kdyz byl puvodni asociovany archivator
    // odstranen v ramci pluginu PPP?") (bez FUNCTION_PANELARCHIVERVIEW a bez FUNCTION_PANELARCHIVEREDIT
    // je ignorovan); 'fsName' je navrhovane jmeno (ziskani prideleneho jmena se provede pomoci
    // CSalamanderGeneralAbstract::GetPluginFSName) file systemu (bez FUNCTION_FILESYSTEM je
    // ignorovan, povolene znaky jsou 'a-zA-Z0-9_+-', min. delka 2 znaky), pokud plugin potrebuje
    // vic jmen file systemu, muze pouzit metodu CSalamanderPluginEntryAbstract::AddFSName;
    // vraci TRUE pri uspesnem prijeti dat
    virtual BOOL WINAPI SetBasicPluginData(const char* pluginName, DWORD functions,
                                           const char* version, const char* copyright,
                                           const char* description, const char* regKeyName = NULL,
                                           const char* extensions = NULL, const char* fsName = NULL) = 0;

    // vraci ukazatel na interface k obecne pouzitelnym funkcim Salamandera,
    // interface je platny po celou dobu existence pluginu (nejen v ramci
    // "SalamanderPluginEntry" funkce) a jde pouze o odkaz, takze se neuvolnuje
    virtual CSalamanderGeneralAbstract* WINAPI GetSalamanderGeneral() = 0;

    // vraci informace spojene s loadem pluginu; informace jsou vraceny v DWORD hodnote
    // jako logicky soucet flagu LOADINFO_XXX (pro test pritomnosti flagu pouzijte
    // podminku: (GetLoadInformation() & LOADINFO_XXX) != 0)
    virtual DWORD WINAPI GetLoadInformation() = 0;

    // nahraje modul s jazykove zavislymi resourcy (SLG-cko); vzdy zkusi nahrat modul
    // stejneho jazyku v jakem prave bezi Salamander, pokud takovy modul nenajde (nebo
    // nesouhlasi verze), necha uzivatele vybrat alternativni modul (pokud existuje vic
    // nez jedna alternativa + pokud uz nema ulozeny uzivateluv vyber z minuleho loadu
    // pluginu); pokud nenajde zadny modul, vraci NULL -> plugin by se mel ukoncit;
    // 'parent' je parent messageboxu s chybami a dialogu pro vyber alternativniho
    // jazykoveho modulu; 'pluginName' je jmeno pluginu (aby uzivatel tusil o jaky plugin
    // se jedna pri chybovem hlaseni nebo vyberu alternativniho jazykoveho modulu)
    // POZOR: tuto metodu je mozne volat jen jednou; ziskany handle jazykoveho modulu
    //        se uvolni automaticky pri unloadu pluginu
    virtual HINSTANCE WINAPI LoadLanguageModule(HWND parent, const char* pluginName) = 0;

    // vraci ID aktualniho jazyku zvoleneho pro prostredi Salamandera (napr. english.slg =
    // English (US) = 0x409, czech.slg = Czech = 0x405)
    virtual WORD WINAPI GetCurrentSalamanderLanguageID() = 0;

    // vraci ukazatel na interface poskytujici upravene Windows controly pouzivane
    // v Salamanderovi, interface je platny po celou dobu existence pluginu (nejen
    // v ramci "SalamanderPluginEntry" funkce) a jde pouze o odkaz, takze se neuvolnuje
    virtual CSalamanderGUIAbstract* WINAPI GetSalamanderGUI() = 0;

    // vraci ukazatel na interface pro komfortni praci se soubory,
    // interface je platny po celou dobu existence pluginu (nejen v ramci
    // "SalamanderPluginEntry" funkce) a jde pouze o odkaz, takze se neuvolnuje
    virtual CSalamanderSafeFileAbstract* WINAPI GetSalamanderSafeFile() = 0;

    // nastavi URL, ktere se ma zobrazit v okne Plugins Manager jako home-page pluginu;
    // hodnotu si Salamander udrzuje az do pristiho loadu pluginu (URL se zobrazuje i pro
    // nenaloadene pluginy); pri kazdem loadu pluginu je URL nutne nastavit znovu, jinak
    // se zadne URL nezobrazi (obrana proti drzeni neplatneho URL home-page)
    virtual void WINAPI SetPluginHomePageURL(const char* url) = 0;

    // pridani dalsiho jmena file systemu; bez FUNCTION_FILESYSTEM v parametru 'functions'
    // pri volani metody SetBasicPluginData vraci tato metoda vzdy jen chybu;
    // 'fsName' je navrhovane jmeno (ziskani prideleneho jmena se provede pomoci
    // CSalamanderGeneralAbstract::GetPluginFSName) file systemu (povolene znaky jsou
    // 'a-zA-Z0-9_+-', min. delka 2 znaky); v 'newFSNameIndex' (nesmi byt NULL) se
    // vraci index nove pridaneho jmena file systemu; vraci TRUE v pripade uspechu;
    // vraci FALSE pri fatalni chybe - v tomto pripade se 'newFSNameIndex' ignoruje
    // omezeni: nesmi se volat pred metodou SetBasicPluginData
    virtual BOOL WINAPI AddFSName(const char* fsName, int* newFSNameIndex) = 0;
};

//
// ****************************************************************************
// FSalamanderPluginEntry
//
// Open Salamander 1.6 or Later Plugin Entry Point Function Type,
// tuto funkci plugin vyvazi jako "SalamanderPluginEntry" a Salamander ji vola
// pro pripojeni pluginu v okamziku loadu pluginu
// vraci interface pluginu v pripade uspesneho pripojeni, jinak NULL,
// interface pluginu se uvolnuje volanim jeho metody Release pred unloadem pluginu

typedef CPluginInterfaceAbstract*(WINAPI* FSalamanderPluginEntry)(CSalamanderPluginEntryAbstract* salamander);

//
// ****************************************************************************
// FSalamanderPluginGetReqVer
//
// Open Salamander 2.5 Beta 2 or Later Plugin Get Required Version of Salamander Function Type,
// tuto funkci plugin vyvazi jako "SalamanderPluginGetReqVer" a Salamander ji vola
// jako prvni funkci pluginu (pred "SalamanderPluginGetSDKVer" a "SalamanderPluginEntry")
// v okamziku loadu pluginu;
// vraci verzi Salamandera, pro kterou je plugin staven (nejstarsi verze, do ktere lze plugin nacist)

typedef int(WINAPI* FSalamanderPluginGetReqVer)();

//
// ****************************************************************************
// FSalamanderPluginGetSDKVer
//
// Open Salamander 2.52 beta 2 (PB 22) or Later Plugin Get SDK Version Function Type,
// tuto funkci plugin volitelne vyvazi jako "SalamanderPluginGetSDKVer" a Salamander
// ji zkousi volat jako druhou funkci pluginu (pred "SalamanderPluginEntry")
// v okamziku loadu pluginu;
// vraci verzi SDK, pouziteho pro stavbu pluginu (informuje Salamandera, ktere metody
// plugin poskytuje); exportovat "SalamanderPluginGetSDKVer" ma smysl jen pokud vraci
// "SalamanderPluginGetReqVer" mensi cislo nez LAST_VERSION_OF_SALAMANDER; je vhodne
// vracet primo LAST_VERSION_OF_SALAMANDER

typedef int(WINAPI* FSalamanderPluginGetSDKVer)();

// ****************************************************************************
// SalIsWindowsVersionOrGreater
//
// Based on SDK 8.1 VersionHelpers.h
// Indicates if the current OS version matches, or is greater than, the provided
// version information. This function is useful in confirming a version of Windows
// Server that doesn't share a version number with a client release.
// http://msdn.microsoft.com/en-us/library/windows/desktop/dn424964%28v=vs.85%29.aspx
//

#ifdef __BORLANDC__
inline void* SecureZeroMemory(void* ptr, int cnt)
{
    char* vptr = (char*)ptr;
    while (cnt)
    {
        *vptr++ = 0;
        cnt--;
    }
    return ptr;
}
#endif // __BORLANDC__

inline BOOL SalIsWindowsVersionOrGreater(WORD wMajorVersion, WORD wMinorVersion, WORD wServicePackMajor)
{
    OSVERSIONINFOEXW osvi;
    DWORDLONG const dwlConditionMask = VerSetConditionMask(VerSetConditionMask(VerSetConditionMask(0,
                                                                                                   VER_MAJORVERSION, VER_GREATER_EQUAL),
                                                                               VER_MINORVERSION, VER_GREATER_EQUAL),
                                                           VER_SERVICEPACKMAJOR, VER_GREATER_EQUAL);

    SecureZeroMemory(&osvi, sizeof(osvi)); // nahrada za memset (nevyzaduje RTLko)
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    osvi.dwMajorVersion = wMajorVersion;
    osvi.dwMinorVersion = wMinorVersion;
    osvi.wServicePackMajor = wServicePackMajor;
    return VerifyVersionInfoW(&osvi, VER_MAJORVERSION | VER_MINORVERSION | VER_SERVICEPACKMAJOR, dwlConditionMask) != FALSE;
}

// Find Windows version using bisection method and VerifyVersionInfo.
// Author:   M1xA, www.m1xa.com
// Licence:  MIT
// Version:  1.0
// https://bitbucket.org/AnyCPU/findversion/src/ebdec778fdbcdee67ac9a4d520239e134e047d8d/include/findversion.h?at=default
// Tested on: Windows 2000 .. Windows 8.1.
//
// WARNING: This function is ***SLOW_HACK***, use SalIsWindowsVersionOrGreater() instead (if you can).

#define M1xA_FV_EQUAL 0
#define M1xA_FV_LESS -1
#define M1xA_FV_GREAT 1
#define M1xA_FV_MIN_VALUE 0
#define M1xA_FV_MINOR_VERSION_MAX_VALUE 16
inline int M1xA_testValue(OSVERSIONINFOEX* value, DWORD verPart, DWORDLONG eq, DWORDLONG gt)
{
    if (VerifyVersionInfo(value, verPart, eq) == FALSE)
    {
        if (VerifyVersionInfo(value, verPart, gt) == TRUE)
            return M1xA_FV_GREAT;
        return M1xA_FV_LESS;
    }
    else
        return M1xA_FV_EQUAL;
}

#define M1xA_findPartTemplate(T) \
    inline BOOL M1xA_findPart##T(T* part, DWORD partType, OSVERSIONINFOEX* ret, T a, T b) \
    { \
        int funx = M1xA_FV_EQUAL; \
\
        DWORDLONG const eq = VerSetConditionMask(0, partType, VER_EQUAL); \
        DWORDLONG const gt = VerSetConditionMask(0, partType, VER_GREATER); \
\
        T* p = part; \
\
        *p = (T)((a + b) / 2); \
\
        while ((funx = M1xA_testValue(ret, partType, eq, gt)) != M1xA_FV_EQUAL) \
        { \
            switch (funx) \
            { \
            case M1xA_FV_GREAT: \
                a = *p; \
                break; \
            case M1xA_FV_LESS: \
                b = *p; \
                break; \
            } \
\
            *p = (T)((a + b) / 2); \
\
            if (*p == a) \
            { \
                if (M1xA_testValue(ret, partType, eq, gt) == M1xA_FV_EQUAL) \
                    return TRUE; \
\
                *p = b; \
\
                if (M1xA_testValue(ret, partType, eq, gt) == M1xA_FV_EQUAL) \
                    return TRUE; \
\
                a = 0; \
                b = 0; \
                *p = 0; \
            } \
\
            if (a == b) \
            { \
                *p = 0; \
                return FALSE; \
            } \
        } \
\
        return TRUE; \
    }
M1xA_findPartTemplate(DWORD)
    M1xA_findPartTemplate(WORD)
        M1xA_findPartTemplate(BYTE)

            inline BOOL SalGetVersionEx(OSVERSIONINFOEX* osVer, BOOL versionOnly)
{
    BOOL ret = TRUE;
    ZeroMemory(osVer, sizeof(OSVERSIONINFOEX));
    osVer->dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (!versionOnly)
    {
        ret &= M1xA_findPartDWORD(&osVer->dwPlatformId, VER_PLATFORMID, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
    }
    ret &= M1xA_findPartDWORD(&osVer->dwMajorVersion, VER_MAJORVERSION, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
    ret &= M1xA_findPartDWORD(&osVer->dwMinorVersion, VER_MINORVERSION, osVer, M1xA_FV_MIN_VALUE, M1xA_FV_MINOR_VERSION_MAX_VALUE);
    if (!versionOnly)
    {
        ret &= M1xA_findPartDWORD(&osVer->dwBuildNumber, VER_BUILDNUMBER, osVer, M1xA_FV_MIN_VALUE, MAXDWORD);
        ret &= M1xA_findPartWORD(&osVer->wServicePackMajor, VER_SERVICEPACKMAJOR, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartWORD(&osVer->wServicePackMinor, VER_SERVICEPACKMINOR, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartWORD(&osVer->wSuiteMask, VER_SUITENAME, osVer, M1xA_FV_MIN_VALUE, MAXWORD);
        ret &= M1xA_findPartBYTE(&osVer->wProductType, VER_PRODUCT_TYPE, osVer, M1xA_FV_MIN_VALUE, MAXBYTE);
    }
    return ret;
}

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_base)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
