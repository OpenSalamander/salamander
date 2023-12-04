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
#pragma pack(push, enter_include_spl_com) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// v pluginu je treba definovat promennou SalamanderVersion (int) a v SalamanderPluginEntry tuto
// promennou inicializovat:
// SalamanderVersion = salamander->GetVersion();

// globalni promenna s verzi Salamandera, ve kterem je tento plugin nacteny
extern int SalamanderVersion;

//
// ****************************************************************************
// CSalamanderDirectoryAbstract
//
// trida reprezentuje adresarovou strukturu - soubory a adresare na pozadovanych cestach, root cesta je "",
// oddelovace v ceste jsou backslashe ('\\')
//

// CQuadWord - 64-bitovy unsigned integer pro velikosti souboru
// triky:
//  -rychlejsi predani vstupniho parametru typu CQuadWord: const CQuadWord &
//  -priradit 64-bit integer: quadWord.Value = XXX;
//  -pocitat pomer velikosti: quadWord1.GetDouble() / quadWord2.GetDouble()  // ztrata presnosti pred delenim se projevi malo (max. 1e-15)
//  -oriznout na DWORD: (DWORD)quadWord.Value
//  -prevest (unsigned) __int64 na CQuadWord: CQuadWord().SetUI64(XXX)

struct CQuadWord
{
    union
    {
        struct
        {
            DWORD LoDWord;
            DWORD HiDWord;
        };
        unsigned __int64 Value;
    };

    // POZOR: nesmi sem prijit operator prirazeni ani konstruktor pro jeden DWORD,
    //        jinak bude pouziti 8-bytovych cisel zcela nekontrolovatelne (C++ vse
    //        vzajemne prevede, coz nemusi byt vzdy prave ono)

    CQuadWord() {}
    CQuadWord(DWORD lo, DWORD hi)
    {
        LoDWord = lo;
        HiDWord = hi;
    }
    CQuadWord(const CQuadWord& qw)
    {
        LoDWord = qw.LoDWord;
        HiDWord = qw.HiDWord;
    }

    CQuadWord& Set(DWORD lo, DWORD hi)
    {
        LoDWord = lo;
        HiDWord = hi;
        return *this;
    }
    CQuadWord& SetUI64(unsigned __int64 val)
    {
        Value = val;
        return *this;
    }
    CQuadWord& SetDouble(double val)
    {
        Value = (unsigned __int64)val;
        return *this;
    }

    CQuadWord& operator++()
    {
        ++Value;
        return *this;
    } // prefix ++
    CQuadWord& operator--()
    {
        --Value;
        return *this;
    } // prefix --

    CQuadWord operator+(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value + qw.Value;
        return qwr;
    }
    CQuadWord operator-(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value - qw.Value;
        return qwr;
    }
    CQuadWord operator*(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value * qw.Value;
        return qwr;
    }
    CQuadWord operator/(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value / qw.Value;
        return qwr;
    }
    CQuadWord operator%(const CQuadWord& qw) const
    {
        CQuadWord qwr;
        qwr.Value = Value % qw.Value;
        return qwr;
    }
    CQuadWord operator<<(const int num) const
    {
        CQuadWord qwr;
        qwr.Value = Value << num;
        return qwr;
    }
    CQuadWord operator>>(const int num) const
    {
        CQuadWord qwr;
        qwr.Value = Value >> num;
        return qwr;
    }

    CQuadWord& operator+=(const CQuadWord& qw)
    {
        Value += qw.Value;
        return *this;
    }
    CQuadWord& operator-=(const CQuadWord& qw)
    {
        Value -= qw.Value;
        return *this;
    }
    CQuadWord& operator*=(const CQuadWord& qw)
    {
        Value *= qw.Value;
        return *this;
    }
    CQuadWord& operator/=(const CQuadWord& qw)
    {
        Value /= qw.Value;
        return *this;
    }
    CQuadWord& operator%=(const CQuadWord& qw)
    {
        Value %= qw.Value;
        return *this;
    }
    CQuadWord& operator<<=(const int num)
    {
        Value <<= num;
        return *this;
    }
    CQuadWord& operator>>=(const int num)
    {
        Value >>= num;
        return *this;
    }

    BOOL operator==(const CQuadWord& qw) const { return Value == qw.Value; }
    BOOL operator!=(const CQuadWord& qw) const { return Value != qw.Value; }
    BOOL operator<(const CQuadWord& qw) const { return Value < qw.Value; }
    BOOL operator>(const CQuadWord& qw) const { return Value > qw.Value; }
    BOOL operator<=(const CQuadWord& qw) const { return Value <= qw.Value; }
    BOOL operator>=(const CQuadWord& qw) const { return Value >= qw.Value; }

    // prevod na double (pozor na ztratu presnosti u velkych cisel - double ma jen 15 platnych cislic)
    double GetDouble() const
    { // MSVC neumi konverzi unsigned __int64 na double, takze si musime pomoct sami
        if (Value < CQuadWord(0, 0x80000000).Value)
            return (double)(__int64)Value; // kladne cislo
        else
            return 9223372036854775808.0 + (double)(__int64)(Value - CQuadWord(0, 0x80000000).Value);
    }
};

#define QW_MAX CQuadWord(0xFFFFFFFF, 0xFFFFFFFF)

#define ICONOVERLAYINDEX_NOTUSED 15 // hodnota pro CFileData::IconOverlayIndex v pripade, ze ikona nema overlay

// zaznam kazdeho souboru a adresare v Salamanderovi (zakladni data o souboru/adresari)
struct CFileData // nesmi sem prijit destruktor !
{
    char* Name;                    // naalokovane jmeno souboru (bez cesty), nutne alokovat na heapu
                                   // Salamandera (viz CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    char* Ext;                     // ukazatel do Name za prvni tecku zprava (vcetne tecky na zacatku jmena,
                                   // na Windows se chape jako pripona, narozdil od UNIXu) nebo na konec
                                   // Name, pokud pripona neexistuje; je-li v konfiguraci nastaveno FALSE
                                   // pro SALCFG_SORTBYEXTDIRSASFILES, je v Ext pro adresare ukazatel na konec
                                   // Name (adresare nemaji pripony)
    CQuadWord Size;                // velikost souboru v bytech
    DWORD Attr;                    // atributy souboru - ORovane konstanty FILE_ATTRIBUTE_XXX
    FILETIME LastWrite;            // cas posledniho zapisu do souboru (UTC-based time)
    char* DosName;                 // naalokovane DOS 8.3 jmeno souboru, neni-li treba je NULL, nutne
                                   // alokovat na heapu Salamandera (viz CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    DWORD_PTR PluginData;          // pouziva plugin skrze CPluginDataInterfaceAbstract, Salamander ignoruje
    unsigned NameLen : 9;          // delka retezce Name (strlen(Name)) - POZOR: maximalni delka jmena je (MAX_PATH - 5)
    unsigned Hidden : 1;           // je hidden? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IsLink : 1;           // je link? (je-li 1, ikonka ma overlay linku) - standardni plneni viz CSalamanderGeneralAbstract::IsFileLink(CFileData::Ext), pri zobrazeni ma prednost pred IsOffline, ale IconOverlayIndex ma prednost
    unsigned IsOffline : 1;        // je offline? (je-li 1, ikonka ma overlay offline - cerne hodiny), pri zobrazeni ma IsLink i IconOverlayIndex prednost
    unsigned IconOverlayIndex : 4; // index icon-overlaye (pokud ikona nema zadny overlay, je zde hodnota ICONOVERLAYINDEX_NOTUSED), pri zobrazeni ma prednost pred IsLink a IsOffline

    // flagy pro interni pouziti v Salamanderovi: nuluji se pri pridani do CSalamanderDirectoryAbstract
    unsigned Association : 1;     // vyznam jen pro zobrazeni 'simple icons' - ikona asociovaneho souboru, jinak 0
    unsigned Selected : 1;        // read-only flag oznaceni (0 - polozka neoznacena, 1 - polozka oznacena)
    unsigned Shared : 1;          // je adresar sdileny? u souboru se nepouziva
    unsigned Archive : 1;         // jedna se o archiv? pouziva se pro zobrazovani ikony archivu v panelu
    unsigned SizeValid : 1;       // je u adresare napocitana jeho velikost?
    unsigned Dirty : 1;           // je potreba tuto polozku prekreslit? (pouze docasna platnost; mezi nastavenim bitu a prekreslenim panelu nesmi byt pumpovana message queue, jinak muze dojit k prekresleni ikonky (icon reader) a tim resetu bitu! v dusledku se neprekresli polozka)
    unsigned CutToClip : 1;       // je CUT-nutej na clipboardu? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IconOverlayDone : 1; // jen pro potreby icon-reader-threadu: ziskavame nebo uz jsme ziskavali icon-overlay? (0 - ne, 1 - ano)
};

// konstanty urcujici platnost dat, ktera jsou primo ulozena v CFileData (velikost, pripona, atd.)
// nebo se generuji z primo ulozenych dat automaticky (file-type se generuje z pripony);
// Name + NameLen jsou povinne (musi byt platne vzdy); platnost PluginData si ridi plugin sam
// (Salamander tento atribut ignoruje)
#define VALID_DATA_EXTENSION 0x0001   // pripona je ulozena v Ext (bez: vsechny Ext = konec Name)
#define VALID_DATA_DOSNAME 0x0002     // DOS name je ulozeno v DosName (bez: vsechny DosName = NULL)
#define VALID_DATA_SIZE 0x0004        // velikost v bytech je ulozena v Size (bez: vsechny Size = 0)
#define VALID_DATA_TYPE 0x0008        // file-type muze byt generovan z Ext (bez: negeneruje se)
#define VALID_DATA_DATE 0x0010        // datum modifikace (UTC-based) je ulozen v LastWrite (bez: vsechny datumy v LastWrite jsou 1.1.1602 v local time)
#define VALID_DATA_TIME 0x0020        // cas modifikace (UTC-based) je ulozen v LastWrite (bez: vsechny casy v LastWrite jsou 0:00:00 v local time)
#define VALID_DATA_ATTRIBUTES 0x0040  // atributy jsou ulozeny v Attr (ORovane Win32 API konstanty FILE_ATTRIBUTE_XXX) (bez: vsechny Attr = 0)
#define VALID_DATA_HIDDEN 0x0080      // "ghosted" priznak ikony je ulozen v Hidden (bez: vsechny Hidden = 0)
#define VALID_DATA_ISLINK 0x0100      // IsLink obsahuje 1 pokud jde o link, ikonka ma overlay linku (bez: vsechny IsLink = 0)
#define VALID_DATA_ISOFFLINE 0x0200   // IsOffline obsahuje 1 pokud jde o offline soubor/adresar, ikonka ma offline overlay (bez: vsechny IsOffline = 0)
#define VALID_DATA_PL_SIZE 0x0400     // ma smysl jen bez pouziti VALID_DATA_SIZE: plugin ma aspon pro nektere soubory/adresare ulozenou velikost v bytech (nekde v PluginData), pro ziskani teto velikosti Salamander vola CPluginDataInterfaceAbstract::GetByteSize()
#define VALID_DATA_PL_DATE 0x0800     // ma smysl jen bez pouziti VALID_DATA_DATE: plugin ma aspon pro nektere soubory/adresare ulozeny datum modifikace (nekde v PluginData), pro ziskani teto velikosti Salamander vola CPluginDataInterfaceAbstract::GetLastWriteDate()
#define VALID_DATA_PL_TIME 0x1000     // ma smysl jen bez pouziti VALID_DATA_TIME: plugin ma aspon pro nektere soubory/adresare ulozeny cas modifikace (nekde v PluginData), pro ziskani teto velikosti Salamander vola CPluginDataInterfaceAbstract::GetLastWriteTime()
#define VALID_DATA_ICONOVERLAY 0x2000 // IconOverlayIndex je index icon-overlaye (zadny overlay = hodnota ICONOVERLAYINDEX_NOTUSED) (bez: vsechny IconOverlayIndex = ICONOVERLAYINDEX_NOTUSED), zadani ikon viz CSalamanderGeneralAbstract::SetPluginIconOverlays

#define VALID_DATA_NONE 0 // pomocna konstanta - platne je jen Name a NameLen

#ifdef INSIDE_SALAMANDER
// VALID_DATA_ALL a VALID_DATA_ALL_FS_ARC jsou jen pro interni pouziti v Salamanderovi (jadre),
// pluginy si naORuji jen konstanty odpovidajici pluginem dodavanym datum (zamezi se tak problemum
// pri zavedeni dalsich konstant a jim odpovidajicim datum)
#define VALID_DATA_ALL 0xFFFF
#define VALID_DATA_ALL_FS_ARC (0xFFFF & ~VALID_DATA_ICONOVERLAY) // pro FS a archivy: vse krome icon-overlays
#endif                                                           // INSIDE_SALAMANDER

// Pokud je zapnuto skryvani hidden a system souboru a adresaru, nezobrazuji se v panelech polozky s
// Hidden==1 a Attr obsahujicim FILE_ATTRIBUTE_HIDDEN a/nebo FILE_ATTRIBUTE_SYSTEM.

// konstanty priznaku pro CSalamanderDirectoryAbstract:
// jmena souboru a adresaru (i v cestach) se maji porovnavat case-sensitive (bez tohoto flagu je
// porovnavani case-insensitive - standardni chovani ve Windows)
#define SALDIRFLAG_CASESENSITIVE 0x0001
// jmena podadresaru v ramci kazdeho adresare se nebudou testovat na duplicitu (tento
// test je casove narocny a je nutny jen v archivech, pokud se pridavaji polozky nejen
// do rootu - aby fungovalo napr. pridani "file1" na "dir1" nasledovane pridanim
// "dir1" - "dir1" se prida prvni operaci (automaticky se prida neexistujici cesta),
// druha operace uz jen obnovi udaje o "dir1" (nesmi ho pridat znovu))
#define SALDIRFLAG_IGNOREDUPDIRS 0x0002

class CPluginDataInterfaceAbstract;

class CSalamanderDirectoryAbstract
{
public:
    // vycisti cely objekt, pripravi ho pro dalsi pouziti; pokud 'pluginData' neni NULL, pouzije
    // se pro soubory a adresare k uvolneni dat specifickych pluginu (CFileData::PluginData);
    // nastavuje standardni hodnotu masky platnych dat (suma vsech VALID_DATA_XXX krome
    // VALID_DATA_ICONOVERLAY) a priznaku objektu (viz metoda SetFlags)
    virtual void WINAPI Clear(CPluginDataInterfaceAbstract* pluginData) = 0;

    // zadani masky platnych dat, podle ktere se urcuje, ktera data z CFileData jsou platna
    // a ktera se maji pouze "nulovat" (viz komentar k VALID_DATA_XXX); maska 'validData'
    // obsahuje ORovane hodnoty VALID_DATA_XXX; standardni hodnota masky je suma vsech
    // VALID_DATA_XXX krome VALID_DATA_ICONOVERLAY; masku platnych dat je potreba nastavit
    // pred volanim AddFile/AddDir
    virtual void WINAPI SetValidData(DWORD validData) = 0;

    // nastaveni priznaku pro tento objekt; 'flags' je kombinace ORovanych priznaku SALDIRFLAG_XXX,
    // standardni hodnota priznaku objektu je nula pro archivatory (zadny priznak neni nastaven)
    // a SALDIRFLAG_IGNOREDUPDIRS pro file-systemy (smi se pridavat jen do rootu, test na duplicitu
    // adresaru je zbytecny)
    virtual void WINAPI SetFlags(DWORD flags) = 0;

    // prida soubor na zadanou cestou (relativni k tomuto "salamander-adresari"), vraci uspech
    // retezec path se pouziva jen uvnitr funkce, obsah struktury file se pouziva i mimo funkci
    // (neuvolnovat pamet naalokovanou pro promenne uvnitr struktury)
    // v pripade neuspechu, je treba obsah struktury file uvolnit;
    // parametr 'pluginData' neni NULL jen pro archivy (FS pouzivaji jen prazdne 'path' (==NULL));
    // neni-li 'pluginData' NULL, pouziva se 'pluginData' pri zakladani novych adresaru (pokud
    // 'path' neexistuje), viz CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // kontrola unikatnosti jmena souboru na ceste 'path' se neprovadi
    virtual BOOL WINAPI AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData) = 0;

    // prida adresar na zadanou cestu (relativni k tomuto "salamander-adresari"), vraci uspech
    // retezec path se pouziva jen uvnitr funkce, obsah struktury file se pouziva i mimo funkci
    // (neuvolnovat pamet naalokovanou pro promenne uvnitr struktury)
    // v pripade neuspechu, je treba obsah struktury file uvolnit;
    // parametr 'pluginData' neni NULL jen pro archivy (FS pouzivaji jen prazdne 'path' (==NULL));
    // neni-li 'pluginData' NULL, pouziva se pri zakladani novych adresaru (pokud 'path' neexistuje),
    // viz CPluginDataInterfaceAbstract::GetFileDataForNewDir;
    // kontrola unikatnosti jmena adresare na ceste 'path' se provadi, dochazi-li k pridani
    // jiz existujiciho adresare, dojde k uvolneni puvodnich dat (neni-li 'pluginData' NULL, vola
    // se pro uvolneni dat i CPluginDataInterfaceAbstract::ReleasePluginData) a ulozeni dat z 'dir'
    // (je nutne pro obnovu dat adresaru, ktere se vytvori automaticky pri neexistenci 'path');
    // specialita pro FS (nebo objekt alokovany pres CSalamanderGeneralAbstract::AllocSalamanderDirectory
    // s 'isForFS'==TRUE): je-li dir.Name "..", je adresar pridan jako up-dir (muze byt jen jeden,
    // zobrazuje se vzdy na zacatku listingu a ma specialni ikonu)
    virtual BOOL WINAPI AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData) = 0;

    // vraci pocet souboru v objektu
    virtual int WINAPI GetFilesCount() const = 0;

    // vraci pocet adresaru v objektu
    virtual int WINAPI GetDirsCount() const = 0;

    // vraci soubor z indexu 'index', vracena data lze pouzit jen pro cteni
    virtual CFileData const* WINAPI GetFile(int index) const = 0;

    // vraci adresar z indexu 'index', vracena data lze pouzit jen pro cteni
    virtual CFileData const* WINAPI GetDir(int index) const = 0;

    // vraci objekt CSalamanderDirectory pro adresar z indexu 'index', vraceny objekt lze
    // pouzit jen pro cteni (objekty pro prazdne adresare nejsou alokovany, vraci se jeden
    // globalni prazdny objekt - zmena tohoto objektu by se projevila globalne)
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int index) const = 0;

    // Pluginu umoznuje predem sdelit predpokladany pocet souboru a adresaru v tomto adresari.
    // Salamander si upravi realokacni strategii tak, aby pridavani prvku prilis nebrzdilo.
    // Ma smysl volat pro adresare obsahujici tisice souboru nebo adresaru. V pripade desitek
    // tisic uz je zavolani teto metody temer nutnost, jinak realokace zaberou nekolik vterin.
    // 'files' a 'dirs' tedy vyjadruji priblizny celkovy pocet souboru a adresaru.
    // Pokud je nektera z hodnot -1, bude ji Salamander ignorovat.
    // Metodu ma vyznam volat pouze pokud je adresar prazdny, tedy nebylo volano AddFile nebo AddDir.
    virtual void WINAPI SetApproximateCount(int files, int dirs) = 0;
};

//
// ****************************************************************************
// SalEnumSelection a SalEnumSelection2
//

// konstanty vracene z SalEnumSelection a SalEnumSelection2 v parametru 'errorOccured'
#define SALENUM_SUCCESS 0 // chyba nenastala
#define SALENUM_ERROR 1   // nastala chyba a uzivatel si preje pokracovat v operaci (vynechaly se jen chybne soubory/adresare)
#define SALENUM_CANCEL 2  // nastala chyba a uzivatel si preje zrusit operaci

// enumerator, vraci jmena souboru, konci vracenim NULL;
// 'enumFiles' == -1 -> reset enumerace (po tomto volani zacina enumerace opet od zacatku), vsechny
//                      dalsi parametry (az na 'param') jsou ignorovany, nema navratove hodnoty (dava
//                      vse na nulu)
// 'enumFiles' == 0 -> enumerace souboru a podadresaru jen z korene
// 'enumFiles' == 1 -> enumerace vsech souboru a podadresaru
// 'enumFiles' == 2 -> enumerace vsech podadresaru, soubory jen z korene;
// k chybe muze dojit jen pri 'enumFiles' == 1 nebo 'enumFiles' == 2 ('enumFiles' == 0 nekompletuje
// jmena a cesty); 'parent' je parent pripadnych messageboxu s chybami (NULL znamena nezobrazovat
// chyby); v 'isDir' (neni-li NULL) vraci TRUE pokud jde o adresar; v 'size' (neni-li NULL) vraci
// velikost souboru (u adresaru se vraci velikost jen pri 'enumFiles' == 0 - jinak je nulova);
// neni-li 'fileData' NULL, vraci se v nem ukazatel na strukturu CFileData vraceneho
// souboru/adresare (pokud enumerator vraci NULL, vraci se v 'fileData' take NULL);
// 'param' je parametr 'nextParam' predavany spolu s ukazatelem na funkci tohoto
// typu; v 'errorOccured' (neni-li NULL) se vraci SALENUM_ERROR, pokud se pri sestavovani vracenych
// jmen narazilo na prilis dlouhe jmeno a uzivatel se rozhodl preskocit jen chybne soubory/adresare,
// POZOR: chyba se netyka prave vraceneho jmena, to je OK; v 'errorOccured' (neni-li NULL) se vraci
// SALENUM_CANCEL pokud se pri chybe uzivatel rozhodl pro zruseni operace (cancel), zaroven
// enumerator vraci NULL (konci); v 'errorOccured' (neni-li NULL) se vraci SALENUM_SUCCESS pokud
// zadna chyba nenastala
typedef const char*(WINAPI* SalEnumSelection)(HWND parent, int enumFiles, BOOL* isDir, CQuadWord* size,
                                              const CFileData** fileData, void* param, int* errorOccured);

// enumerator, vraci jmena souboru, konci vracenim NULL;
// 'enumFiles' == -1 -> reset enumerace (po tomto volani zacina enumerace opet od zacatku), vsechny
//                      dalsi parametry (az na 'param') jsou ignorovany, nema navratove hodnoty (dava
//                      vse na nulu)
// 'enumFiles' == 0 -> enumerace souboru a podadresaru jen z korene
// 'enumFiles' == 1 -> enumerace vsech souboru a podadresaru
// 'enumFiles' == 2 -> enumerace vsech podadresaru, soubory jen z korene;
// 'enumFiles' == 3 -> enumerace vsech souboru a podadresaru + symbolicke linky na soubory maji
//                     velikost ciloveho souboru (pri 'enumFiles' == 1 maji velikost linku, coz je snad
//                     vzdy nula); POZOR: 'enumFiles' musi zustat 3 pro vsechna volani enumeratoru;
// k chybe muze dojit jen pri 'enumFiles' == 1, 2 nebo 3 ('enumFiles' == 0 vubec
// nepracuje s diskem ani nekompletuje jmena a cesty); 'parent' je parent pripadnych messageboxu
// s chybami (NULL znamena nezobrazovat chyby); v 'dosName' (neni-li NULL) vraci DOSovy nazev
// (8.3; jen pokud existuje, jinak NULL); v 'isDir' (neni-li NULL) vraci TRUE pokud jde o adresar;
// v 'size' (neni-li NULL) vraci velikost souboru (u adresaru nulu); v 'attr' (neni-li NULL)
// vraci atributy souboru/adresare; v 'lastWrite' (neni-li NULL) vraci cas posledniho zapisu
// do souboru/adresare; 'param' je parametr 'nextParam' predavany spolu s ukazatelem na funkci
// tohoto typu; v 'errorOccured' (neni-li NULL) se vraci SALENUM_ERROR, pokud doslo behem cteni
// dat z disku k chybe nebo se pri sestavovani vracenych jmen narazilo na prilis dlouhe jmeno
// a uzivatel se rozhodl preskocit jen chybne soubory/adresare, POZOR: chyba se netyka prave
// vraceneho jmena, to je OK; v 'errorOccured' (neni-li NULL) se vraci SALENUM_CANCEL pokud se
// pri chybe uzivatel rozhodl pro zruseni operace (cancel), zaroven enumerator vraci NULL (konci);
// v 'errorOccured' (neni-li NULL) se vraci SALENUM_SUCCESS pokud zadna chyba nenastala
typedef const char*(WINAPI* SalEnumSelection2)(HWND parent, int enumFiles, const char** dosName,
                                               BOOL* isDir, CQuadWord* size, DWORD* attr,
                                               FILETIME* lastWrite, void* param, int* errorOccured);

//
// ****************************************************************************
// CSalamanderViewAbstract
//
// sada metod Salamandera pro praci se sloupci v panelu (vypinani/zapinani/pridavani/nastavovani)

// rezimy pohledu panelu
#define VIEW_MODE_TREE 1
#define VIEW_MODE_BRIEF 2
#define VIEW_MODE_DETAILED 3
#define VIEW_MODE_ICONS 4
#define VIEW_MODE_THUMBNAILS 5
#define VIEW_MODE_TILES 6

#define TRANSFER_BUFFER_MAX 1024 // velikost bufferu pro prenos obsahu sloupcu z pluginu do Salamandera
#define COLUMN_NAME_MAX 30
#define COLUMN_DESCRIPTION_MAX 100

// Identifikatory sloupcu. Sloupce vlozene pluginem maji nastaveno ID==COLUMN_ID_CUSTOM.
// Standardni sloupce Salamandera maji ostatni ID.
#define COLUMN_ID_CUSTOM 0 // sloupec je poskytovan pluginem - o ulozeni jeho dat se postara plugin
#define COLUMN_ID_NAME 1   // zarovnano vlevo, podporuje FixedWidth
// zarovnano vlevo, podporuje FixedWidth; samostatny sloupec "Ext", muze byt jen na indexu==1;
// pokud sloupec neexistuje a v datech panelu (viz CSalamanderDirectoryAbstract::SetValidData())
// se nastavi VALID_DATA_EXTENSION, je sloupec "Ext" zobrazen ve sloupci "Name"
#define COLUMN_ID_EXTENSION 2
#define COLUMN_ID_DOSNAME 3     // zarovnano vlevo
#define COLUMN_ID_SIZE 4        // zarovnano vpravo
#define COLUMN_ID_TYPE 5        // zarovnano vlevo, podporuje FixedWidth
#define COLUMN_ID_DATE 6        // zarovnano vpravo
#define COLUMN_ID_TIME 7        // zarovnano vpravo
#define COLUMN_ID_ATTRIBUTES 8  // zarovnano vpravo
#define COLUMN_ID_DESCRIPTION 9 // zarovnano vlevo, podporuje FixedWidth

// Callback pro naplneni bufferu znakama, ktere se maji zobrazit v prislusnem sloupci.
// Z duvodu optimalizace funkce nedostava/nevraci promenne prostrednictvim parametru,
// ale prostrednictvim globalni promennych (CSalamanderViewAbstract::GetTransferVariables).
typedef void(WINAPI* FColumnGetText)();

// Callback pro ziskani indexu jednoduchych ikon pro FS s vlastnimi ikonami (pitFromPlugin).
// Z duvodu optimalizace funkce nedostava/nevraci promenne prostrednictvim parametru,
// ale prostrednictvim globalni promennych (CSalamanderViewAbstract::GetTransferVariables).
// Z globalnich promennych callback vyuziva jen TransferFileData a TransferIsDir.
typedef int(WINAPI* FGetPluginIconIndex)();

// sloupec muze vzniknout dvema zpusoby:
// 1) Sloupec vytvoril Salamander na zaklade sablony aktualniho pohledu.
//    V tomto pripade ukazatel 'GetText' (na plnici funkci) ukazuje do Salamandera
//    a ziskava texty standardne z CFileData.
//    Hodnota promenne 'ID' je ruzna od COLUMN_ID_CUSTOM.
//
// 2) Sloupec pridal plugin na zaklade svych potreb.
//    'GetText' ukazuje do pluginu a 'ID' je rovno COLUMN_ID_CUSTOM.

struct CColumn
{
    char Name[COLUMN_NAME_MAX]; // "Name", "Ext", "Size", ... nazev sloupce, pod
                                // kterym sloupec vystupuje v pohledu a v menu
                                // Nesmi obsahovat prazdny retezec.
                                // POZOR: Muze obsahovat (za prvnim null-terminatorem)
                                // i nazev sloupce "Ext" - toto nastava pokud neexistuje
                                // samostatny sloupec "Ext" a v datech panelu (viz
                                // CSalamanderDirectoryAbstract::SetValidData()) se
                                // nastavi VALID_DATA_EXTENSION. Pro spojeni dvou
                                // retezcu poslouzi CSalamanderGeneralAbstract::AddStrToStr().

    char Description[COLUMN_DESCRIPTION_MAX]; // Tooltip v header line
                                              // Nesmi obsahovat prazdny retezec.
                                              // POZOR: Muze obsahovat (za prvnim null-terminatorem)
                                              // i popis sloupce "Ext" - toto nastava pokud neexistuje
                                              // samostatny sloupec "Ext" a v datech panelu (viz
                                              // CSalamanderDirectoryAbstract::SetValidData()) se
                                              // nastavi VALID_DATA_EXTENSION. Pro spojeni dvou
                                              // retezcu poslouzi CSalamanderGeneralAbstract::AddStrToStr().

    FColumnGetText GetText; // callback pro ziskani textu (popis u deklatace typu FColumnGetText)

    // FIXME_X64 - male pro ukazatel, neni nekdy potreba?
    DWORD CustomData; // Neni pouzivana Salamanderem;  plugin ji muze
                      // vyuzit pro rozliseni svych pridanych sloupcu.

    unsigned SupportSorting : 1; // je sloupec mozne radit?

    unsigned LeftAlignment : 1; // pro TRUE je sloupec zarovnavan vlevo; jinak vpravo

    unsigned ID : 4; // identifikator sloupce
                     // Pro standardni sloupce poskytovane Salamanderem
                     // obsahuje hodnoty ruzne od COLUMN_ID_CUSTOM.
                     // Pro sloupce pridane pluginem obsahuje vzdy
                     // hodnotu COLUMN_ID_CUSTOM.

    // Promenne Width a FixedWidth muzou byt zmeneny uzivatelem behem prace s panelem.
    // Standardni sloupce poskytovane Salamanderem maji zajisteno ukladani/nacitani
    // techto hodnot.
    // Hodnoty techto promennych pro sloupce poskytovane pluginem je treba ulozit/nacist
    // v ramci pluginu.
    // Sloupce, jejichz sirku pocita Salamander na zaklade obsahu a uzivatel ji nemuze
    // menit, nazyvame 'elasticke'. Sloupce, pro ktere muze uzivatel nastavit sirku nazyvame
    // 'pevne'/'fixed'.
    unsigned Width : 16;     // Sirka sloupce v pripade, ze je v rezimu pevne (nastavitelne) sirky.
    unsigned FixedWidth : 1; // Je sloupec v rezimu pevne (nastavitelne) sirky?

    // pracovni promenne (nikam se neukladaji a neni treba je inicializovat)
    // jsou urcene pro interni potreby Salamandera a pluginy je ignoruji,
    // protoze jejich obsah neni pri volani pluginu zaruceny
    unsigned MinWidth : 16; // Minimalni sirka, na kterou muze byt sloupce smrsten.
                            // Je pocitana na zaklade nazvu sloupce a jeho raditelnosti
                            // tak, aby byla hlavicka sloupce vzdy viditelna
};

// Plugin prostrednictvim tohoto rozhrani muze pri zmene cesty zmenit rezim
// zobrazeni v panelu. Veskera prace se sloupci se tyka jen vsech detailed rezimu
// (Detailed + Types + tri volitelne rezimy Alt+8/9/0). Pri zmene cesty dostane
// plugin standardni sadu sloupcu nagenerovanou na zaklade sablony aktualniho
// pohledu. Plugin muze tuto sadu modifikovat. Modifikace neni trvaleho razu
// a pri pristi zmene cesty obdrzi plugin opet standardni sadu sloupcu. Muze tak
// napriklad odstranit nektery ze std. sloupcu. Pred novym plnenim std. sloupci
// dostane plugin prilezitost ulozeni informaci o svych sloupcich (COLUMN_ID_CUSTOM).
// Muze tak ulozit jejich 'Width' a 'FixedWidth', ktere uzivatel mohl v panelu
// nastavit (viz ColumnFixedWidthShouldChange() a ColumnWidthWasChanged() v interfacu
// CPluginDataInterfaceAbstract). Pokud plugin zmeni rezim pohledu, zmena je trvala
// (napr. prepnuti na rezim Thumbnails zustane i po opusteni pluginove cesty).

class CSalamanderViewAbstract
{
public:
    // -------------- panel ----------------

    // vraci rezim, ve kterem je zobrazen panel (tree/brief/detailed/icons/thumbnails/tiles)
    // vraci jednu z hodnot VIEW_MODE_xxxx (rezim Detailed, Types a tri volitelne rezimy jsou
    // vsechny VIEW_MODE_DETAILED)
    virtual DWORD WINAPI GetViewMode() = 0;

    // Nastavi rezim panelu na 'viewMode'. Pokud jde o nektery z detailed rezimu, muze
    // odstranit nektere ze standardnich sloupcu (viz. 'validData'). Proto je vhodne tuto
    // funkci volat jako prvni - pred ostatnimi funkcemi z toho ifacu, ktere modifikuji
    // sloupce.
    //
    // 'viewMode' je jedna z hodnot VIEW_MODE_xxxx
    // Rezim panelu nelze zmenit ani na Types ani na jeden ze tri volitelnych detailed rezimu
    // (vsechny zastupuje konstanta VIEW_MODE_DETAILED pouzita pro Detailed rezim panelu).
    // Ovsem pokud je zrovna jeden z techto ctyr rezimu v panelu zvoleny a 'viewMode' je
    // VIEW_MODE_DETAILED, zustane tento rezim zvoleny (aneb neprepne se na Detailed rezim).
    // Zmena rezimu panelu je trvala (pretrva i po opusteni pluginove cesty).
    //
    // 'validData' informuje o tom, jaka data si plugin preje zobrazit v detailed rezimu, hodnota
    // se ANDuje s maskou platnych dat zadanou pomoci CSalamanderDirectoryAbstract::SetValidData
    // (nema smysl zobrazovat sloupce s "nulovanymi" hodnotami).
    virtual void WINAPI SetViewMode(DWORD viewMode, DWORD validData) = 0;

    // Vyzvedne ze Salamandera umisteni promennych, ktere nahrazuji parametry callbacku
    // CColumn::GetText. Na strane Salamandera se jedna o globalni promenne. Plugin si
    // ukazatele na ne ulozi do vlastnich globalnich promennych.
    //
    // promenne:
    //   transferFileData        [IN]     data, na jejichz zaklade se ma vykreslit polozka
    //   transferIsDir           [IN]     rovno 0, pokud jde o soubor (lezi v poli Files),
    //                                    rovno 1, pokud jde o adresar (lezi v poli Dirs),
    //                                    rovno 2, pokud jde o up-dir symbol
    //   transferBuffer          [OUT]    sem se nalejou data, maximalne TRANSFER_BUFFER_MAX znaku
    //                                    neni treba je terminovat nulou
    //   transferLen             [OUT]    pred navratem z callbacku se do teto promenne nastavi
    //                                    pocet naplnenych znaku bez terminatoru (terminator neni
    //                                    treba do bufferu zapisovat)
    //   transferRowData         [IN/OUT] ukazuje na DWORD, ktery je vzdy pred kreslenim sloupcu
    //                                    pro kazdy radek nulovan; lze pouzit pro optimalizace
    //                                    Salamander ma vyhrazene bity 0x00000001 az 0x00000008.
    //                                    Ostatni bity jsou k dispozici pro plugin.
    //   transferPluginDataIface [IN]     plugin-data-interface panelu, do ktereho se polozka
    //                                    vykresluje (patri k (*transferFileData)->PluginData)
    //   transferActCustomData   [IN]     CustomData sloupce, pro ktery se ziskava text (pro ktery
    //                                    se vola callback)
    virtual void WINAPI GetTransferVariables(const CFileData**& transferFileData,
                                             int*& transferIsDir,
                                             char*& transferBuffer,
                                             int*& transferLen,
                                             DWORD*& transferRowData,
                                             CPluginDataInterfaceAbstract**& transferPluginDataIface,
                                             DWORD*& transferActCustomData) = 0;

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // Nastavi callback pro ziskani indexu jednoduchych ikon (viz
    // CPluginDataInterfaceAbstract::GetSimplePluginIcons). Pokud tento callback
    // plugin nenastavi, bude se vykreslovat vzdy jen ikona z indexu 0.
    // Z globalnich promennych callback vyuziva jen TransferFileData a TransferIsDir.
    virtual void WINAPI SetPluginSimpleIconCallback(FGetPluginIconIndex callback) = 0;

    // ------------- columns ---------------

    // vraci pocet sloupcu v panelu (vzdy minimalne jeden, protoze nazev bude vzdy zobrazen)
    virtual int WINAPI GetColumnsCount() = 0;

    // vraci ukazatel na sloupec (pouze pro cteni)
    // 'index' udava, ktery ze sloupcu bude vracen; pokud sloupec 'index' neexistuje, vraci NULL
    virtual const CColumn* WINAPI GetColumn(int index) = 0;

    // Vlozi sloupec na pozici 'index'. Na pozici 0 je vzdy umisten sloupec Name,
    // pokud je zobrazen sloupec Ext, bude na pozici 1. Jinak lze sloupec umistit
    // libovolne. Struktura 'column' bude prekopirovana do vnitrnich struktur
    // Salamandera. Vraci TRUE pokud byl sloupec vlozen.
    virtual BOOL WINAPI InsertColumn(int index, const CColumn* column) = 0;

    // Vlozi standardni sloupec s ID 'id' na pozici 'index'. Na pozici 0 je vzdy
    // umisten sloupec Name, pokud je vkladan sloupec Ext, musi to byt na pozici 1.
    // Jinak lze sloupec umistit libovolne. 'id' je jedna z hodnot COLUMN_ID_xxxx,
    // mimo COLUMN_ID_CUSTOM a COLUMN_ID_NAME.
    virtual BOOL WINAPI InsertStandardColumn(int index, DWORD id) = 0;

    // Nastavi nazev a popis sloupce (nesmi byt prazdne retezce ani NULL). Delky
    // retezu se omezi na COLUMN_NAME_MAX a COLUMN_DESCRIPTION_MAX. Vraci uspech.
    // POZOR: Jmeno a popis sloupce "Name" muzou obsahovat (vzdy za prvnim
    // null-terminatorem) i jmeno a popis sloupce "Ext" - toto nastava pokud
    // neexistuje samostatny sloupec "Ext" a v datech panelu (viz
    // CSalamanderDirectoryAbstract::SetValidData()) se nastavi VALID_DATA_EXTENSION.
    // V tomto pripade je potreba nastavovat dvojite retezce (s dvoumi
    // null-terminatory) - viz CSalamanderGeneralAbstract::AddStrToStr().
    virtual BOOL WINAPI SetColumnName(int index, const char* name, const char* description) = 0;

    // Odstrani sloupec na pozici 'index'. Lze odstranit jak sloupce pridane pluginem,
    // tak standardni sloupce Salamandera. Nelze odstranit sloupec 'Name', ktery je vzdy
    // na indexu 0. Pozor pri odstranovani sloupce 'Ext', pokud je v datech pluginu
    // (viz CSalamanderDirectoryAbstract::SetValidData()) VALID_DATA_EXTENSION, musi
    // se jmeno+popis sloupce 'Ext' objevit u sloupce 'Name'.
    virtual BOOL WINAPI DeleteColumn(int index) = 0;
};

//
// ****************************************************************************
// CPluginDataInterfaceAbstract
//
// sada metod pluginu, ktere potrebuje Salamander pro ziskani specifickych dat
// pluginu do pluginem pridanych sloupcu (pracuje s CFileData::PluginData)

class CPluginInterfaceAbstract;

class CPluginDataInterfaceAbstract
{
#ifdef INSIDE_SALAMANDER
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginDataInterfaceEncapsulation)
    friend class CPluginDataInterfaceEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // vraci TRUE pokud se ma zavolat metoda ReleasePluginData pro vsechny soubory vazane
    // k tomuto interfacu, jinak vraci FALSE
    virtual BOOL WINAPI CallReleaseForFiles() = 0;

    // vraci TRUE pokud se ma zavolat metoda ReleasePluginData pro vsechny adresare vazane
    // k tomuto interfacu, jinak vraci FALSE
    virtual BOOL WINAPI CallReleaseForDirs() = 0;

    // uvolni data specificka pluginu (CFileData::PluginData) pro 'file' (soubor nebo
    // adresar - 'isDir' FALSE nebo TRUE; struktura vlozena do CSalamanderDirectoryAbstract
    // pri listovani archivu nebo FS); vola se pro vsechny soubory, pokud CallReleaseForFiles
    // vrati TRUE, a pro vsechny adresare, pokud CallReleaseForDirs vrati TRUE
    virtual void WINAPI ReleasePluginData(CFileData& file, BOOL isDir) = 0;

    // jen pro data archivu (pro FS se nedoplnuje up-dir symbol):
    // pozmenuje navrhovany obsah up-dir symbolu (".." nahore v panelu); 'archivePath'
    // je cesta v archivu, pro kterou je symbol urcen; v 'upDir' vstupuji navrzena
    // data symbolu: jmeno ".." (nemenit), date&time archivu, zbytek nulovany;
    // v 'upDir' vystupuji zmeny pluginu, predevsim by mel zmenit 'upDir.PluginData',
    // ktery bude vyuzivan na up-dir symbolu pri ziskavani obsahu pridanych sloupcu;
    // pro 'upDir' se nebude volat ReleasePluginData, jakekoliv potrebne uvolnovani
    // je mozne provest vzdy pri dalsim volani GetFileDataForUpDir nebo pri uvolneni
    // celeho interfacu (v jeho destruktoru - volan z
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    virtual void WINAPI GetFileDataForUpDir(const char* archivePath, CFileData& upDir) = 0;

    // jen pro data archivu (FS pouziva jen root cestu v CSalamanderDirectoryAbstract):
    // pri pridavani souboru/adresare do CSalamanderDirectoryAbstract se muze stat, ze
    // zadana cesta neexistuje a je ji tedy potreba vytvorit, jednotlive adresare teto
    // cesty se tvori automaticky a tato metoda umoznuje pluginu pridat sva specificka
    // data (pro sve sloupce) k temto vytvarenym adresarum; 'dirName' je plna cesta
    // pridavaneho adresare v archivu; v 'dir' vstupuji navrhovana data: jmeno adresare
    // (alokovane na heapu Salamandera), date&time prevzaty od pridavaneho souboru/adresare,
    // zbytek nulovany; v 'dir' vystupuji zmeny pluginu, predevsim by mel zmenit
    // 'dir.PluginData'; vraci TRUE pokud se pridani dat pluginu povedlo, jinak FALSE;
    // pokud vrati TRUE, bude 'dir' uvolnen klasickou cestou (Salamanderovska cast +
    // ReleasePluginData) a to bud az pri kompletnim uvolneni listingu nebo jeste behem
    // jeho tvorby v pripade, ze bude ten samy adresar pridan pomoci
    // CSalamanderDirectoryAbstract::AddDir (premazani automatickeho vytvoreni pozdejsim
    // normalnim pridanim); pokud vrati FALSE, bude z 'dir' uvolnena jen Salamanderovska cast
    virtual BOOL WINAPI GetFileDataForNewDir(const char* dirName, CFileData& dir) = 0;

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci image-list s jednoduchymi ikonami, behem kresleni polozek v panelu se
    // pomoci call-backu ziskava icon-index do tohoto image-listu; vola se vzdy po
    // ziskani noveho listingu (po volani CPluginFSInterfaceAbstract::ListCurrentPath),
    // takze je mozne image-list predelavat pro kazdy novy listing;
    // 'iconSize' urcuje pozadovanou velikost ikon a jde o jednu z hodnot SALICONSIZE_xxx
    // destrukci image-listu si plugin zajisti pri dalsim volani GetSimplePluginIcons
    // nebo pri uvolneni celeho interfacu (v jeho destruktoru - volan z
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    // pokud image-list nelze vytvorit, vraci NULL a aktualni plugin-icons-type
    // degraduje na pitSimple
    virtual HIMAGELIST WINAPI GetSimplePluginIcons(int iconSize) = 0;

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci TRUE, pokud pro dany soubor/adresar ('isDir' FALSE/TRUE) 'file'
    // ma byt pouzita jednoducha ikona; vraci FALSE, pokud se ma pro ziskani ikony volat
    // z threadu pro nacitani ikon metoda GetPluginIcon (nacteni ikony "na pozadi");
    // zaroven v teto metode muze byt predpocitan icon-index pro jednoduchou ikonu
    // (u ikon ctenych "na pozadi" se az do okamziku nacteni pouzivaji take jednoduche
    // ikony) a ulozen do CFileData (nejspise do CFileData::PluginData);
    // omezeni: z CSalamanderGeneralAbstract je mozne pouzivat jen metody, ktere lze
    // volat z libovolneho threadu (metody nezavisle na stavu panelu)
    virtual BOOL WINAPI HasSimplePluginIcon(CFileData& file, BOOL isDir) = 0;

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci ikonu pro soubor nebo adresar 'file' nebo NULL pokud ikona nelze ziskat; vraci-li
    // v 'destroyIcon' TRUE, vola se pro uvolneni vracene ikony Win32 API funkce DestroyIcon;
    // 'iconSize' urcuje velikost pozadovane ikony a jde o jednu z hodnot SALICONSIZE_xxx
    // omezeni: jelikoz se vola z threadu pro nacitani ikon (neni to hlavni thread), lze z
    // CSalamanderGeneralAbstract pouzivat jen metody, ktere lze volat z libovolneho threadu
    virtual HICON WINAPI GetPluginIcon(const CFileData* file, int iconSize, BOOL& destroyIcon) = 0;

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // porovna 'file1' (muze jit o soubor i adresar) a 'file2' (muze jit o soubor i adresar),
    // nesmi pro zadne dve polozky listingu vratit, ze jsou shodne (zajistuje jednoznacne
    // prirazeni vlastni ikony k souboru/adresari); pokud nehrozi duplicitni jmena v listingu
    // cesty (obvykly pripad), lze jednoduse implementovat jako:
    // {return strcmp(file1->Name, file2->Name);}
    // vraci cislo mensi nez nula pokud 'file1' < 'file2', nulu pokud 'file1' == 'file2' a
    // cislo vetsi nez nula pokud 'file1' > 'file2';
    // omezeni: jelikoz se vola i z threadu pro nacitani ikon (nejen z hlavniho threadu), lze
    // z CSalamanderGeneralAbstract pouzivat jen metody, ktere lze volat z libovolneho threadu
    virtual int WINAPI CompareFilesFromFS(const CFileData* file1, const CFileData* file2) = 0;

    // slouzi k nastaveni parametru pohledu, tato metoda je zavolana vzdy pred zobrazenim noveho
    // obsahu panelu (pri zmene cesty) a pri zmene aktualniho pohledu (i rucni zmena sirky
    // sloupce); 'leftPanel' je TRUE pokud jde o levy panel (FALSE pokud jde o pravy panel);
    // 'view' je interface pro modifikaci pohledu (nastaveni rezimu, prace se
    // sloupci); jde-li o data archivu, obsahuje 'archivePath' soucasnou cestu v archivu,
    // pro data FS je 'archivePath' NULL; jde-li o data archivu, je 'upperDir' ukazatel na
    // nadrazeny adresar (je-li soucasna cesta root archivu, je 'upperDir' NULL), pro data
    // FS je vzdy NULL;
    // POZOR: behem volani teto metody nesmi dojit k prekresleni panelu (muze se zde zmenit
    //        velikost ikon, atd.), takze zadne messageloopy (zadne dialogy, atd.)!
    // omezeni: z CSalamanderGeneralAbstract je mozne pouzivat jen metody, ktere lze
    //          volat z libovolneho threadu (metody nezavisle na stavu panelu)
    virtual void WINAPI SetupView(BOOL leftPanel, CSalamanderViewAbstract* view,
                                  const char* archivePath, const CFileData* upperDir) = 0;

    // nastaveni nove hodnoty "column->FixedWidth" - uzivatel pouzil kontextove menu
    // na pluginem pridanem sloupci v header-line > "Automatic Column Width"; plugin
    // by si mel ulozit novou hodnotu column->FixedWidth ulozenou v 'newFixedWidth'
    // (je to vzdy negace column->FixedWidth), aby pri nasledujicich volanich SetupView() mohl
    // sloupec pridat uz se spravne nastavenou FixedWidth; zaroven pokud se zapina pevna
    // sirka sloupce, mel by si plugin nastavit soucasnou hodnotu "column->Width" (aby
    // se timto zapnutim pevne sirky nezmenila sirka sloupce) - idealni je zavolat
    // "ColumnWidthWasChanged(leftPanel, column, column->Width)"; 'column' identifikuje
    // sloupec, ktery se ma zmenit; 'leftPanel' je TRUE pokud jde o sloupec z leveho
    // panelu (FALSE pokud jde o sloupec z praveho panelu)
    virtual void WINAPI ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column,
                                                     int newFixedWidth) = 0;

    // nastaveni nove hodnoty "column->Width" - uzivatel mysi zmenil sirku pluginem pridaneho
    // sloupce v header-line; plugin by si mel ulozit novou hodnotu column->Width (je ulozena
    // i v 'newWidth'), aby pri nasledujicich volanich SetupView() mohl sloupec pridat uz se
    // spravne nastavenou Width; 'column' identifikuje sloupec, ktery se zmenil; 'leftPanel'
    // je TRUE pokud jde o sloupec z leveho panelu (FALSE pokud jde o sloupec z praveho panelu)
    virtual void WINAPI ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column,
                                              int newWidth) = 0;

    // ziska obsah Information Line pro soubor/adresar ('isDir' TRUE/FALSE) 'file'
    // nebo oznacene soubory a adresare ('file' je NULL a pocty oznacenych souboru/adresaru
    // jsou v 'selectedFiles'/'selectedDirs') v panelu ('panel' je jeden z PANEL_XXX);
    // vola se i pri prazdnem listingu (tyka se jen FS, u archivu nemuze nastat, 'file' je NULL,
    // 'selectedFiles' a 'selectedDirs' jsou 0); je-li 'displaySize' TRUE, je znama velikost
    // vsech oznacenych adresaru (viz CFileData::SizeValid; pokud neni nic oznaceneho, je zde
    // TRUE); v 'selectedSize' je soucet cisel CFileData::Size oznacenych souboru a adresaru
    // (pokud neni nic oznaceneho, je zde nula); 'buffer' je buffer pro vraceny text (velikost
    // 1000 bytu); 'hotTexts' je pole (velikost 100 DWORDu), ve kterem se vraci informace o poloze
    // hot-textu, vzdy spodni WORD obsahuje pozici hot-textu v 'buffer', horni WORD obsahuje
    // delku hot-textu; v 'hotTextsCount' je velikost pole 'hotTexts' (100) a vraci se v nem pocet
    // zapsanych hot-textu v poli 'hotTexts'; vraci TRUE pokud je 'buffer' + 'hotTexts' +
    // 'hotTextsCount' nastaveno, vraci FALSE pokud se ma Information Line plnit standardnim
    // zpusobem (jako na disku)
    virtual BOOL WINAPI GetInfoLineContent(int panel, const CFileData* file, BOOL isDir, int selectedFiles,
                                           int selectedDirs, BOOL displaySize, const CQuadWord& selectedSize,
                                           char* buffer, DWORD* hotTexts, int& hotTextsCount) = 0;

    // jen pro archivy: uzivatel ulozil soubory/adresare z archivu na clipboard, ted zavira
    // archiv v panelu: pokud metoda vrati TRUE, tento objekt zustane otevreny (optimalizace
    // pripadneho Paste z clipboardu - archiv uz je vylistovany), pokud metoda vrati FALSE,
    // tento objekt se uvolni (pripadny Paste z clipboardu zpusobi listovani archivu, pak
    // teprve dojde k vybaleni vybranych souboru/adresaru); POZNAMKA: pokud je po zivotnost
    // objektu otevreny soubor archivu, metoda by mela vracet FALSE, jinak bude po celou
    // dobu "pobytu" dat na clipboardu soubor archivu otevreny (nepujde smazat, atd.)
    virtual BOOL WINAPI CanBeCopiedToClipboard() = 0;

    // jen pri zadani VALID_DATA_PL_SIZE do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je velikost souboru/adresare ('isDir' TRUE/FALSE) 'file' znama,
    // jinak vraci FALSE; velikost vraci v 'size'
    virtual BOOL WINAPI GetByteSize(const CFileData* file, BOOL isDir, CQuadWord* size) = 0;

    // jen pri zadani VALID_DATA_PL_DATE do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je datum souboru/adresare ('isDir' TRUE/FALSE) 'file' znamy,
    // jinak vraci FALSE; datum vraci v "datumove" casti struktury 'date' ("casova" cast
    // by mela zustat netknuta)
    virtual BOOL WINAPI GetLastWriteDate(const CFileData* file, BOOL isDir, SYSTEMTIME* date) = 0;

    // jen pri zadani VALID_DATA_PL_TIME do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je cas souboru/adresare ('isDir' TRUE/FALSE) 'file' znamy,
    // jinak vraci FALSE; cas vraci v "casove" casti struktury 'time' ("datumova" cast
    // by mela zustat netknuta)
    virtual BOOL WINAPI GetLastWriteTime(const CFileData* file, BOOL isDir, SYSTEMTIME* time) = 0;
};

//
// ****************************************************************************
// CSalamanderForOperationsAbstract
//
// sada metod ze Salamandera pro podporu provadeni operaci, platnost interfacu je
// omezena na metodu, ktere je interface predan jako parametr; tedy lze volat pouze
// z tohoto threadu a v teto metode (objekt je na stacku, takze po navratu zanika)

class CSalamanderForOperationsAbstract
{
public:
    // PROGRESS DIALOG: dialog obsahuje jeden/dva ('twoProgressBars' FALSE/TRUE) progress-metry
    // otevre progress-dialog s titulkem 'title'; 'parent' je parent okno progress-dialogu (je-li
    // NULL, pouzije se hlavni okno); pokud obsahuje jen jeden progress-metr, muze byt popsan
    // jako "File" ('fileProgress' je TRUE) nebo "Total" ('fileProgress' je FALSE)
    //
    // dialog nebezi ve vlastnim threadu; pro jeho fungovani (tlacitko Cancel + vnitrni timer)
    // je treba obcas vyprazdni message queue; to zajistuji metody ProgressDialogAddText,
    // ProgressAddSize a ProgressSetSize
    //
    // protoze real-time zobrazovani textu a zmen v progress bare silne zdrzuje, maji
    // metody ProgressDialogAddText, ProgressAddSize a ProgressSetSize parametr
    // 'delayedPaint'; ten by mel byt TRUE pro vsechny rychle se menici texty a hodnoty;
    // metody si pak ulozi texty a zobrazi je az po doruceni vnitrniho timeru dialogu;
    // 'delayedPaint' nastavime na FALSE pro inicializacni/koncove texty typu "preparing data..."
    // nebo "canceling operation...", po jejiz zobrazeni nedame dialogu prilezitost k distribuci
    // zprav (timeru); pokud je u takove operace pravdepodobne, ze bude trvat dlouho, meli
    // bychom behem teto doby dialog "obcerstvovat" volanim ProgressAddSize(CQuadWord(0, 0), TRUE)
    // a podle jeji navratove hodnoty akci pripadne predcasne ukoncit
    virtual void WINAPI OpenProgressDialog(const char* title, BOOL twoProgressBars,
                                           HWND parent, BOOL fileProgress) = 0;
    // vypise text 'txt' (i nekolik radku - provadi se rozpad na radky) do progress-dialogu
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint) = 0;
    // neni-li 'totalSize1' CQuadWord(-1, -1), nastavi 'totalSize1' jako 100 procent prvniho progress-metru,
    // neni-li 'totalSize2' CQuadWord(-1, -1), nastavi 'totalSize2' jako 100 procent druheho progress-metru
    // (pro progress-dialog s jednim progress-metrem je povinne 'totalSize2' CQuadWord(-1, -1))
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2) = 0;
    // neni-li 'size1' CQuadWord(-1, -1), nastavi velikost 'size1' (size1/total1*100 procent) na prvnim progress-metru,
    // neni-li 'size2' CQuadWord(-1, -1), nastavi velikost 'size2' (size2/total2*100 procent) na druhem progress-metru
    // (pro progress-dialog s jednim progress-metrem je povinne 'size2' CQuadWord(-1, -1)), vraci informaci jestli ma
    // akce pokracovat (FALSE = konec)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint) = 0;
    // prida (pripadne k oboum progress-metrum) velikost 'size' (size/total*100 procent progressu),
    // vraci informaci jestli ma akce pokracovat (FALSE = konec)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint) = 0;
    // enabluje/disabluje tlacitko Cancel
    virtual void WINAPI ProgressEnableCancel(BOOL enable) = 0;
    // vraci HWND dialogu progressu (hodi se pri vypisu chyb a dotazu pri otevrenem progress-dialogu)
    virtual HWND WINAPI ProgressGetHWND() = 0;
    // zavre progress-dialog
    virtual void WINAPI CloseProgressDialog() = 0;

    // presune vsechny soubory ze 'source' adresare do 'target' adresare,
    // navic premapovava predpony zobrazovanych jmen ('remapNameFrom' -> 'remapNameTo')
    // vraci uspech operace
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_com)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
