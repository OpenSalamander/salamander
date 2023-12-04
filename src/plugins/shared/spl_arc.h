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
#pragma pack(push, enter_include_spl_arc) // aby byly struktury nezavisle na nastavenem zarovnavani
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

class CSalamanderDirectoryAbstract;
class CSalamanderForOperationsAbstract;
class CPluginDataInterfaceAbstract;

//
// ****************************************************************************
// CPluginInterfaceForArchiverAbstract
//

class CPluginInterfaceForArchiverAbstract
{
#ifdef INSIDE_SALAMANDER
private: // ochrana proti nespravnemu primemu volani metod (viz CPluginInterfaceForArchiverEncapsulation)
    friend class CPluginInterfaceForArchiverEncapsulation;
#else  // INSIDE_SALAMANDER
public:
#endif // INSIDE_SALAMANDER

    // funkce pro "panel archiver view"; vola se pro nacteni obsahu archivu 'fileName';
    // obsah se plni do objektu 'dir'; Salamander zjisti obsah
    // pluginem pridanych sloupcu pomoci interfacu 'pluginData' (pokud plugin sloupce
    // nepridava, vraci 'pluginData'==NULL); vraci TRUE pri uspesnem nacteni obsahu archivu,
    // pokud vrati FALSE, navratova hodnota 'pluginData' se ignoruje (data v 'dir' je potreba
    // uvolnit pomoci 'dir.Clear(pluginData)', jinak se uvolni jen Salamanderovska cast dat);
    // 'salamander' je sada uzitecnych metod vyvezenych ze Salamandera,
    // POZOR: soubor fileName take nemusi existovat (pokud je otevren v panelu a odjinud smazan),
    // ListArchive neni volan pro soubory nulove delky, ty maji automaticky prazdny obsah,
    // pri pakovani do takovych souboru se soubor pred volanim PackToArchive smaze (pro
    // kompatibilitu s externimi pakovaci)
    virtual BOOL WINAPI ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                    CSalamanderDirectoryAbstract* dir,
                                    CPluginDataInterfaceAbstract*& pluginData) = 0;

    // funkce pro "panel archiver view", vola se pri pozadavku na rozpakovani souboru/adresaru
    // z archivu 'fileName' do adresare 'targetDir' z cesty v archivu 'archiveRoot'; 'pluginData'
    // je interface pro praci s informacemi o souborech/adresarich, ktere jsou specificke pluginu
    // (napr. data z pridanych sloupcu; jde o stejny interface, ktery vraci metoda ListArchive
    // v parametru 'pluginData' - takze muze byt i NULL); soubory/adresare jsou zadany enumeracni
    // funkci 'next' jejimz parametrem je 'nextParam'; vraci TRUE pri uspesnem rozpakovani (nebyl
    // pouzit Cancel, mohl byt pouzit Skip) - zdroj operace v panelu je odznacen, jinak vraci
    // FALSE (neprovede se odznaceni); 'salamander' je sada uzitecnych metod vyvezenych ze
    // Salamandera
    virtual BOOL WINAPI UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                      const char* archiveRoot, SalEnumSelection next,
                                      void* nextParam) = 0;

    // funkce pro "panel archiver view", vola se pri pozadavku na rozpakovani jednoho souboru pro view/edit
    // z archivu 'fileName' do adresare 'targetDir'; jmeno souboru v archivu je 'nameInArchive';
    // 'pluginData' je interface pro praci s informacemi o souboru, ktere jsou specificke pluginu
    // (napr. data z pridanych sloupcu; jde o stejny interface, ktery vraci metoda ListArchive
    // v parametru 'pluginData' - takze muze byt i NULL); 'fileData' je ukazatel na strukturu CFileData
    // vypakovavaneho souboru (strukturu sestavil plugin pri listovani archivu); 'newFileName' (neni-li
    // NULL) je nove jmeno pro rozbalovany soubor (pouziva se pokud puvodni jmeno z archivu neni mozne
    // vybalit na disk (napr. "aux", "prn", atd.)); do 'renamingNotSupported' (jen neni-li 'newFileName'
    // NULL) zapsat TRUE pokud plugin nepodporuje prejmenovani pri vybalovani (standardni chybova hlaska
    // "renaming not supported" se zobrazi ze Salamandera); vraci TRUE pri uspesnem rozpakovani souboru
    // (soubor je na zadane ceste, nebyl pouzit Cancel ani Skip), 'salamander' je sada uzitecnych metod
    // vyvezenych ze Salamandera
    virtual BOOL WINAPI UnpackOneFile(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      CPluginDataInterfaceAbstract* pluginData, const char* nameInArchive,
                                      const CFileData* fileData, const char* targetDir,
                                      const char* newFileName, BOOL* renamingNotSupported) = 0;

    // funkce pro "panel archiver edit" a "custom archiver pack", vola se pri pozadavku na zapakovani
    // souboru/adresaru do archivu 'fileName' na cestu 'archiveRoot', soubory/adresare jsou zadany
    // zdrojovou cestou 'sourcePath' a enumeracni funkci 'next' s parametrem 'nextParam',
    // je-li 'move' TRUE, zapakovane soubory/adresare by mely byt odstranene z disku, vraci TRUE
    // pokud se podari zapakovat/odstranit vsechny soubory/adresare (nebyl pouzit Cancel, mohl byt
    // pouzit Skip) - zdroj operace v panelu je odznacen, jinak vraci FALSE (neprovede se odznaceni),
    // 'salamander' je sada uzitecnych metod vyvezenych ze Salamandera
    virtual BOOL WINAPI PackToArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                      const char* archiveRoot, BOOL move, const char* sourcePath,
                                      SalEnumSelection2 next, void* nextParam) = 0;

    // funkce pro "panel archiver edit", vola se pri pozadavku na smazani souboru/adresaru z archivu
    // 'fileName'; soubory/adresare jsou zadany cestou 'archiveRoot' a enumeracni funkci 'next'
    // s parametrem 'nextParam'; 'pluginData' je interface pro praci s informacemi o souborech/adresarich,
    // ktere jsou specificke pluginu (napr. data z pridanych sloupcu; jde o stejny interface, ktery vraci
    // metoda ListArchive v parametru 'pluginData' - takze muze byt i NULL); vraci TRUE pokud se
    // podari smazat vsechny soubory/adresare (nebyl pouzit Cancel, mohl byt pouzit Skip) - zdroj
    // operace v panelu je odznacen, jinak vraci FALSE (neprovede se odznaceni); 'salamander' je sada
    // uzitecnych metod vyvezenych ze Salamandera
    virtual BOOL WINAPI DeleteFromArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                          CPluginDataInterfaceAbstract* pluginData, const char* archiveRoot,
                                          SalEnumSelection next, void* nextParam) = 0;

    // funkce pro "custom archiver unpack"; vola se pri pozadavku na rozbaleni souboru/adresaru z archivu
    // 'fileName' do adresare 'targetDir'; soubory/adresare jsou zadany maskou 'mask'; vraci TRUE pokud
    // se podari rozbalit vsechny soubory/adresare (nebyl pouzit Cancel, mohl byt pouzit Skip);
    // je-li 'delArchiveWhenDone' TRUE, je potreba napridavat do 'archiveVolumes' vsechny svazky archivu
    // (vcetne null-terminatoru; neni-li vicesvazkovy, bude tam jen 'fileName'), vrati-li se z teto funkce
    // TRUE (uspesne rozbaleni), dojde nasledne ke smazani vsech souboru z 'archiveVolumes';
    // 'salamander' je sada uzitecnych metod vyvezenych ze Salamandera
    virtual BOOL WINAPI UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                           const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                           CDynamicString* archiveVolumes) = 0;

    // funkce pro "panel archiver view/edit", vola se pred zavrenim panelu s archivem
    // POZOR: pokud se nepodari otevrit novou cestu, archiv muze v panelu zustat (nezavisle na tom,
    //        co vrati CanCloseArchive); metodu tedy nelze pouzit pro destrukci kontextu;
    //        je urcena napriklad pro optimalizaci operace Delete z archivu, kdy muze pri
    //        jeho opousteni nabidnou "setreseni" archivu
    //        pro destrukci kontextu lze pouzit metodu CPluginInterfaceAbstract::ReleasePluginDataInterface,
    //        viz dokument archivatory.txt
    // 'fileName' je jmeno archivu; 'salamander' je sada uzitecnych metod vyvezenych ze Salamandera;
    // 'panel' oznacuje panel, ve kterem je archiv otevreny (PANEL_LEFT nebo PANEL_RIGHT);
    // vraci TRUE pokud je zavreni mozne, je-li 'force' TRUE, vraci TRUE vzdy; pokud probiha
    // critical shutdown (vice viz CSalamanderGeneralAbstract::IsCriticalShutdown), nema
    // smysl se usera na cokoliv ptat
    virtual BOOL WINAPI CanCloseArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                        BOOL force, int panel) = 0;

    // zjisti pozadovane nastaveni disk-cache (disk-cache se pouziva pro docasne kopie
    // souboru pri otevirani souboru z archivu ve viewerech, editorech a pres systemove
    // asociace); normalne (podari-li se po volani naalokovat kopii 'tempPath') se vola
    // jen jednou a to pred prvnim pouzitim disk-cache (napr. pred prvnim otevrenim
    // souboru z archivu ve vieweru/editoru); pokud vraci FALSE, pouziva
    // se standardni nastaveni (soubory v TEMP adresari, kopie se mazou pomoci Win32
    // API funkce DeleteFile() az pri prekroceni limitni velikosti cache nebo pri zavreni archivu)
    // a vsechny ostatni navratove hodnoty se ignoruji; vraci-li TRUE, pouzivaji se nasledujici
    // navratove hodnoty: neni-li 'tempPath' (buffer o velikosti MAX_PATH) prazdny retezec, budou
    // se vsechny docasne kopie vypakovane pluginem z archivu ukladat do podadresaru teto cesty
    // (tyto podadresare zrusi disk-cache pri ukonceni Salamandera, ale pluginu nic nebrani
    // v jejich smazani drive, napr. pri svem unloadu; zaroven je vhodne pri loadu prvni instance
    // pluginu (nejen v ramci jednoho spusteneho Salamandera) promazat podadresare "SAL*.tmp" na teto
    // ceste - resi problemy vznikle zamknutymi soubory a pady softu) + je-li 'ownDelete' TRUE,
    // bude se pro mazani kopii volat metoda DeleteTmpCopy a PrematureDeleteTmpCopy + je-li
    // 'cacheCopies' FALSE, budou se kopie mazat hned jakmile se uvolni (napr. jakmile se zavre
    // viewer), je-li 'cacheCopies' TRUE, budou se kopie mazat az pri prekroceni limitni velikosti
    // cache nebo pri zavreni archivu
    virtual BOOL WINAPI GetCacheInfo(char* tempPath, BOOL* ownDelete, BOOL* cacheCopies) = 0;

    // pouziva se jen pokud metoda GetCacheInfo vraci v parametru 'ownDelete' TRUE:
    // smaze docasnou kopii vypakovanou z tohoto archivu (pozor na read-only soubory,
    // u nich je treba nejprve zmenit atributy, a pak jdou teprve smazat), pokud mozno
    // by nemelo zobrazovat zadna okna (uzivatel cinnost primo nevyvolal, muze ho rusit
    // v jine cinnosti), pri delsich akcich se hodi vyuziti wait-okenka (viz
    // CSalamanderGeneralAbstract::CreateSafeWaitWindow); 'fileName' je jmeno souboru
    // s kopii; pokud se maze vic souboru najednou (muze nastat napr. po zavreni
    // editovaneho archivu), je 'firstFile' TRUE pro prvni soubor a FALSE pro ostatni
    // soubory (pouziva se ke korektnimu zobrazeni wait-okenka - viz DEMOPLUG)
    //
    // POZOR: vola se v hl. threadu na zaklade doruceni zpravy z disk-cache hl. oknu - posila se
    // zprava o potrebe uvolnit docasnou kopii (typicky v okamziku zavreni vieweru nebo
    // "rozeditovaneho" archivu v panelu), tedy muze dojit k opakovanemu vstupu do pluginu
    // (pokud zpravu distribuuje message-loopa uvnitr pluginu), dalsi vstup do DeleteTmpCopy
    // je vylouceny, protoze do ukonceni volani DeleteTmpCopy disk-cache zadne dalsi zpravy
    // neposila
    virtual void WINAPI DeleteTmpCopy(const char* fileName, BOOL firstFile) = 0;

    // pouziva se jen pokud metoda GetCacheInfo vraci v parametru 'ownDelete' TRUE:
    // pri unloadu pluginu zjisti, jestli se ma volat DeleteTmpCopy pro kopie, ktere jsou
    // jeste pouzivane (napr. otevrene ve vieweru) - vola se jen pokud takove kopie
    // existuji; 'parent' je parent pripadneho messageboxu s dotazem uzivateli (pripadne
    // doporuceni, aby user zavrel vsechny soubory z archivu, aby je plugin mohl smazat);
    // 'copiesCount' je pocet pouzivanych kopii souboru z archivu; vraci TRUE pokud se
    // ma volat DeleteTmpCopy, pokud vrati FALSE, kopie zustanou na disku; pokud probiha
    // critical shutdown (vice viz CSalamanderGeneralAbstract::IsCriticalShutdown), nema
    // smysl se usera na cokoliv ptat a provadet zdlouhave akce (napr. shredovani souboru)
    // POZNAMKA: behem provadeni PrematureDeleteTmpCopy je zajisteno, ze nedojde
    // k volani DeleteTmpCopy
    virtual BOOL WINAPI PrematureDeleteTmpCopy(HWND parent, int copiesCount) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_arc)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
