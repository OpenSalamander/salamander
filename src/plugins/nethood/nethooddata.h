// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

/*
	Network Plugin for Open Salamander
	
	Copyright (c) 2008-2023 Milan Kase <manison@manison.cz>
	
	TODO:
	Open-source license goes here...
*/

#pragma once

/// Network neghborhood plugin data interface.
class CNethoodPluginDataInterface : public CPluginDataInterfaceAbstract
{
protected:
    enum
    {
        ColumnDataComment = 1,
    };

    TCHAR m_szRedirectPath[MAX_PATH];

    void SetupColumns(__in CSalamanderViewAbstract* pView);
    void AddDescriptionColumn(__in CSalamanderViewAbstract* pView);

    static void WINAPI GetCommentColumnText();

    static int WINAPI GetSimpleIconIndex();

    static int NodeTypeToIconIndex(__in CNethoodCacheNode::Type nodeType);

    static void RedirectUncPathToSalamander(
        __in int iPanel,
        __in PCTSTR pszUncPath);

public:
    /// Constructor.
    CNethoodPluginDataInterface();

    /// Destructor.
    ~CNethoodPluginDataInterface();

    void SetRedirectPath(__in PCTSTR pszUncPath)
    {
        StringCchCopy(m_szRedirectPath, COUNTOF(m_szRedirectPath), pszUncPath);
    }

    //----------------------------------------------------------------------
    // CPluginDataInterfaceAbstract

    /// \return The return value should be TRUE if ReleasePluginData method
    ///         should be called for every single file bound to this interface.
    ///         Otherwise the return value should be FALSE.
    virtual BOOL WINAPI CallReleaseForFiles();

    /// \return The return value should be TRUE if ReleasePluginData method
    ///         should be called for every single directory bound to this
    ///         interface. Otherwise the return value should be FALSE.
    virtual BOOL WINAPI CallReleaseForDirs();

    // uvolni data specificka pluginu (CFileData::PluginData) pro 'file' (soubor nebo
    // adresar - 'isDir' FALSE nebo TRUE; struktura vlozena do CSalamanderDirectoryAbstract
    // pri listovani archivu nebo FS); vola se pro vsechny soubory, pokud CallReleaseForFiles
    // vrati TRUE, a pro vsechny adresare, pokud CallReleaseForDirs vrati TRUE
    virtual void WINAPI ReleasePluginData(
        __in CFileData& file,
        __in BOOL isDir);

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
    virtual void WINAPI GetFileDataForUpDir(
        __in const char* archivePath,
        __inout CFileData& upDir);

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
    virtual BOOL WINAPI GetFileDataForNewDir(
        __in const char* dirName,
        __inout CFileData& dir);

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci image-list s jednoduchymi ikonami, behem kresleni polozek v panelu se
    // pomoci call-backu ziskava icon-index do tohoto image-listu; vola se vzdy po
    // ziskani noveho listingu (po volani CPluginFSInterfaceAbstract::ListCurrentPath),
    // takze je mozne image-list predelavat pro kazdy novy listing;
    // 'iconSize' urcuje pozadovanoy velikost ikon a jde o jednu z hodnot SALICONSIZE_xxx
    // destrukci image-listu si plugin zajisti pri dalsim volani GetSimplePluginIcons
    // nebo pri uvolneni celeho interfacu (v jeho destruktoru - volan z
    // CPluginInterfaceAbstract::ReleasePluginDataInterface)
    // pokud image-list nelze vytvorit, vraci NULL a aktualni plugin-icons-type
    // degraduje na pitSimple
    virtual HIMAGELIST WINAPI GetSimplePluginIcons(
        __in int iconSize);

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci TRUE, pokud pro dany soubor/adresar ('isDir' FALSE/TRUE) 'file'
    // ma byt pouzita jednoducha ikona; vraci FALSE, pokud se ma pro ziskani ikony volat
    // z threadu pro nacitani ikon metoda GetPluginIcon (nacteni ikony "na pozadi");
    // zaroven v teto metode muze byt predpocitan icon-index pro jednoduchou ikonu
    // (u ikon ctenych "na pozadi" se az do okamziku nacteni pouzivaji take jednoduche
    // ikony) a ulozen do CFileData (nejspise do CFileData::PluginData);
    // omezeni: z CSalamanderGeneralAbstract je mozne pouzivat jen metody, ktere lze
    // volat z libovolneho threadu (metody nezavisle na stavu panelu)
    virtual BOOL WINAPI HasSimplePluginIcon(
        CFileData& file,
        BOOL isDir);

    // jen pro FS s vlastnimi ikonami (pitFromPlugin):
    // vraci ikonu pro soubor nebo adresar 'file' nebo NULL pokud ikona nelze ziskat; vraci-li
    // v 'destroyIcon' TRUE, vola se pro uvolneni vracene ikony Win32 API funkce DestroyIcon;
    // 'iconSize' urcuje velikost pozadovane ikony a jde o jednu z hodnot SALICONSIZE_xxx
    // omezeni: jelikoz se vola z threadu pro nacitani ikon (neni to hlavni thread), lze z
    // CSalamanderGeneralAbstract pouzivat jen metody, ktere lze volat z libovolneho threadu
    virtual HICON WINAPI GetPluginIcon(
        __in const CFileData* file,
        __in int iconSize,
        __out BOOL& destroyIcon);

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
    virtual int WINAPI CompareFilesFromFS(
        __in const CFileData* file1,
        __in const CFileData* file2);

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
    virtual void WINAPI SetupView(
        __in BOOL leftPanel,
        __in CSalamanderViewAbstract* view,
        __in const char* archivePath,
        __in const CFileData* upperDir);

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
    virtual void WINAPI ColumnFixedWidthShouldChange(
        __in BOOL leftPanel,
        __in const CColumn* column,
        __in int newFixedWidth);

    // nastaveni nove hodnoty "column->Width" - uzivatel mysi zmenil sirku pluginem pridaneho
    // sloupce v header-line; plugin by si mel ulozit novou hodnotu column->Width (je ulozena
    // i v 'newWidth'), aby pri nasledujicich volanich SetupView() mohl sloupec pridat uz se
    // spravne nastavenou Width; 'column' identifikuje sloupec, ktery se zmenil; 'leftPanel'
    // je TRUE pokud jde o sloupec z leveho panelu (FALSE pokud jde o sloupec z praveho panelu)
    virtual void WINAPI ColumnWidthWasChanged(
        __in BOOL leftPanel,
        __in const CColumn* column,
        __in int newWidth);

    // ziska obsah Information Line pro soubor/adresar ('isDir' TRUE/FALSE) 'file'
    // nebo oznacene soubory a adresare ('file' je NULL a pocty oznacenych souboru/adresaru
    // jsou v 'selectedFiles'/'selectedDirs') v panelu ('panel' je jeden z PANEL_XXX);
    // je-li 'displaySize' TRUE, je znama velikost vsech oznacenych adresaru (viz
    // CFileData::SizeValid; pokud neni nic oznaceneho, je zde TRUE); v 'selectedSize' je
    // soucet cisel CFileData::Size oznacenych souboru a adresaru (pokud neni nic oznaceneho,
    // je zde nula); 'buffer' je buffer pro vraceny text (velikost 1000 bytu); 'hotTexts'
    // je pole (velikost 100 DWORDu), ve kterem se vraci informace o poloze hot-textu, vzdy
    // spodni WORD obsahuje pozici hot-textu v 'buffer', horni WORD obsahuje delku hot-textu;
    // v 'hotTextsCount' se vraci pocet zapsanych hot-textu v poli 'hotTexts'; vraci TRUE
    // pokud je 'buffer' + 'hotTexts' + 'hotTextsCount' nastaveno, vraci FALSE pokud se
    // ma Information Line plnit standardnim zpusobem (jako na disku)
    virtual BOOL WINAPI GetInfoLineContent(
        __in int panel,
        __in const CFileData* file,
        __in BOOL isDir,
        __in int selectedFiles,
        __in int selectedDirs,
        __in BOOL displaySize,
        __in const CQuadWord& selectedSize,
        __out_bcount(1000) char* buffer,
        __out_ecount(100) DWORD* hotTexts,
        __out int& hotTextsCount);

    // jen pro archivy: uzivatel ulozil soubory/adresare z archivu na clipboard, ted zavira
    // archiv v panelu: pokud metoda vrati TRUE, tento objekt zustane otevreny (optimalizace
    // pripadneho Paste z clipboardu - archiv uz je vylistovany), pokud metoda vrati FALSE,
    // tento objekt se uvolni (pripadny Paste z clipboardu zpusobi listovani archivu, pak
    // teprve dojde k vybaleni vybranych souboru/adresaru); POZNAMKA: pokud je po zivotnost
    // objektu otevreny soubor archivu, metoda by mela vracet FALSE, jinak bude po celou
    // dobu "pobytu" dat na clipboardu soubor archivu otevreny (nepujde smazat, atd.)
    virtual BOOL WINAPI CanBeCopiedToClipboard();

    // jen pri zadani VALID_DATA_PL_SIZE do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je velikost souboru/adresare ('isDir' TRUE/FALSE) 'file' znama,
    // jinak vraci FALSE; velikost vraci v 'size'
    virtual BOOL WINAPI GetByteSize(
        __in const CFileData* file,
        __in BOOL isDir,
        __out CQuadWord* size);

    // jen pri zadani VALID_DATA_PL_DATE do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je datum souboru/adresare ('isDir' TRUE/FALSE) 'file' znamy,
    // jinak vraci FALSE; datum vraci v "datumove" casti struktury 'date' ("casova" cast
    // by mela zustat netknuta)
    virtual BOOL WINAPI GetLastWriteDate(
        __in const CFileData* file,
        __in BOOL isDir,
        __out SYSTEMTIME* date);

    // jen pri zadani VALID_DATA_PL_TIME do CSalamanderDirectoryAbstract::SetValidData():
    // vraci TRUE pokud je cas souboru/adresare ('isDir' TRUE/FALSE) 'file' znamy,
    // jinak vraci FALSE; cas vraci v "casove" casti struktury 'time' ("datumova" cast
    // by mela zustat netknuta)
    virtual BOOL WINAPI GetLastWriteTime(
        __in const CFileData* file,
        __in BOOL isDir,
        __out SYSTEMTIME* time);
};
