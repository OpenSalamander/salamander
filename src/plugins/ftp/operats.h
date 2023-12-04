// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#pragma pack(push, enter_include_operats_h_dt) // aby byly vsechny struktury co nejmensi (rychlosti netreba, hlavne setrime mistem)
#pragma pack(1)

// vraci ukazatel na textovy popis chyby 'error'; je-li 'error' NO_ERROR, vraci
// text typu "unknown, maybe insufficient system resources", jinak vraci std.
// Windows popis chyby (pro jeho ulozeni pouziva buffer 'errBuf'+'errBufSize')
const char* GetWorkerErrorTxt(int error, char* errBuf, int errBufSize);

//
// ****************************************************************************
// CFTPQueueItem
//
// spolecny predek pro vsechny polozky v "FTP Queue"

enum CFTPQueueItemType
{
    fqitNone, // prazdna polozka (nutne nastavit na jeden z nasledujicich typu)

    fqitDeleteExploreDir,      // explore adresare pro delete (pozn.: linky na adresare mazeme jako celek, ucel operace se splni a nesmaze se nic "navic") (objekt tridy CFTPQueueItemDelExplore)
    fqitCopyResolveLink,       // download: kopirovani: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
    fqitMoveResolveLink,       // download: presun: zjisteni jestli jde o link na soubor nebo adresar (objekt tridy CFTPQueueItemCopyOrMove)
    fqitCopyExploreDir,        // download: explore adresare nebo linku na adresar pro kopirovani (objekt tridy CFTPQueueItemCopyMoveExplore)
    fqitMoveExploreDir,        // download: explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
    fqitMoveExploreDirLink,    // download: explore linku na adresar pro presun (po dokonceni smaze link na adresar) (objekt tridy CFTPQueueItemCopyMoveExplore)
    fqitChAttrsExploreDir,     // explore adresare pro zmenu atributu (prida i polozku pro zmenu atributu adresare) (objekt tridy CFTPQueueItemChAttrExplore)
    fqitChAttrsResolveLink,    // zmena atributu: zjisteni jestli jde o link na adresar (objekt tridy CFTPQueueItem)
    fqitChAttrsExploreDirLink, // explore linku na adresar pro zmenu atributu (objekt tridy CFTPQueueItem)
    fqitUploadCopyExploreDir,  // upload: explore adresare pro kopirovani (objekt tridy CFTPQueueItemCopyMoveUploadExplore)
    fqitUploadMoveExploreDir,  // upload: explore adresare pro presun (po dokonceni smaze adresar) (objekt tridy CFTPQueueItemCopyMoveUploadExplore)

    fqitLastResolveOrExploreItem, // jen ciselna konstanta pro rozliseni typu (priority zpracovani) polozek

    fqitDeleteLink,          // delete pro link (objekt tridy CFTPQueueItemDel)
    fqitDeleteFile,          // delete pro soubor (objekt tridy CFTPQueueItemDel)
    fqitDeleteDir,           // delete pro adresar (objekt tridy CFTPQueueItemDir)
    fqitCopyFileOrFileLink,  // download: kopirovani souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
    fqitMoveFileOrFileLink,  // download: presun souboru nebo linku na soubor (objekt tridy CFTPQueueItemCopyOrMove)
    fqitMoveDeleteDir,       // download: smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
    fqitMoveDeleteDirLink,   // download: smazani linku na adresar po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
    fqitChAttrsFile,         // zmena atributu souboru (pozn.: u linku se atributy menit nedaji) (objekt tridy CFTPQueueItemChAttr)
    fqitChAttrsDir,          // zmena atributu adresare (objekt tridy CFTPQueueItemChAttrDir)
    fqitUploadCopyFile,      // upload: kopirovani souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
    fqitUploadMoveFile,      // upload: presun souboru (objekt tridy CFTPQueueItemCopyOrMoveUpload)
    fqitUploadMoveDeleteDir, // upload: smazani adresare po presunuti jeho obsahu (objekt tridy CFTPQueueItemDir)
};

enum CFTPQueueItemState
{
    sqisNone,       // prazdny stav (nutne nastavit na jeden z nasledujicich stavu)
    sqisDone,       // polozka byla dokoncena
    sqisWaiting,    // ceka na zpracovani
    sqisProcessing, // probiha zpracovani polozky
    sqisDelayed,    // zpracovani polozky je odlozeno (ceka az se zpracuji "child" polozky - napr. smazani adresare az po vymazu vsech obsazenych souboru a adresaru)

    // zbyvajici stavy se pouzivaji pro ruzne vyjadreni chyby polozky, POZOR: musi byt na konci enumu !!! (duvod: pouzivaji se podminky: (item->GetItemState() >= sqisSkipped /* sqisSkipped, sqisFailed, sqisUserInputNeeded nebo sqisForcedToFail */))
    sqisSkipped,         // polozka byla skipnuta
    sqisFailed,          // polozka se nedokoncila kvuli chybe (failnula)
    sqisUserInputNeeded, // pro dokonceni polozky je nutny zasah uzivatele (mame dotaz pro uzivatele)
    sqisForcedToFail,    // polozka, ktera se dostala do chyboveho stavu kvuli chybam/skipum child polozek
};

// seznam problemu, ktere mohou nastat polozkam ve fronte; pro novou konstantu je potreba:
// - pridat osetreni do CFTPQueue::SolveErrorOnItem (operats1.cpp) nebo
//   je-li neresitelny, pridat do podminky v CFTPQueueItem::HasErrorToSolve (operats2.cpp)
// - pridat textovy popis do CFTPQueueItem::GetProblemDescr (operats2.cpp)
#define ITEMPR_OK 0                          // zadny problem nenastal
#define ITEMPR_LOWMEM 1                      // nedostatek systemovych prostredku (napr. pameti)
#define ITEMPR_CANNOTCREATETGTFILE 2         // problem "cilovy soubor nelze vytvorit nebo otevrit" (pouziva WinError)
#define ITEMPR_CANNOTCREATETGTDIR 3          // download: problem "cilovy adresar nelze vytvorit" (pouziva WinError)
#define ITEMPR_TGTFILEALREADYEXISTS 4        // problem "cilovy soubor jiz existuje"
#define ITEMPR_TGTDIRALREADYEXISTS 5         // problem "cilovy adresar jiz existuje"
#define ITEMPR_RETRYONCREATFILE 6            // problem "retry na souboru FTP klientem primo vytvorenem nebo prepsanem"
#define ITEMPR_RETRYONRESUMFILE 7            // problem "retry na souboru FTP klientem resumnutem"
#define ITEMPR_ASCIITRFORBINFILE 8           // problem "ASCII transfer mode pro binarni soubor"
#define ITEMPR_UNKNOWNATTRS 9                // problem "soubor/adresar ma nezname atributy, ktere neumime zachovat (jina prava nez 'r'+'w'+'x')"
#define ITEMPR_INVALIDPATHTODIR 10           // problem "cesta do adresare je prilis dlouha nebo syntakticky nespravna" (nastava pri explore-dir - jak zdrojovy, tak cilovy adresar) (neresitelny problem)
#define ITEMPR_UNABLETOCWD 11                // problem "chyba pri zmene pracovni cesty na serveru, odpoved serveru: %s" - zmena cesty do Path+Name nebo TgtPath+TgtName (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETOPWD 12                // problem "chyba pri zjistovani pracovni cesty na serveru, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_DIREXPLENDLESSLOOP 13         // problem "pruzkum tohoto adresare by znamenal zahajeni nekonecneho cyklu" (nastava pri explore-dir) (neresitelny problem)
#define ITEMPR_LISTENFAILURE 14              // problem "chyba pri priprave otevirani aktivniho datoveho spojeni: %s" (pouziva WinError)
#define ITEMPR_INCOMPLETELISTING 15          // problem "nelze nacist cely seznam souboru a adresaru ze serveru: %s" (pouziva WinError i ErrAllocDescr)
#define ITEMPR_UNABLETOPARSELISTING 16       // problem "neznamy format seznamu souboru a adresaru ze serveru"
#define ITEMPR_DIRISHIDDEN 17                // problem "adresar je skryty"
#define ITEMPR_DIRISNOTEMPTY 18              // problem "adresar neni prazdny"
#define ITEMPR_FILEISHIDDEN 19               // problem "soubor je skryty"
#define ITEMPR_INVALIDPATHTOLINK 20          // problem "plne jmeno linku je prilis dlouhe nebo syntakticky nespravne" (nastava pri resolve-link) (neresitelny problem)
#define ITEMPR_UNABLETORESOLVELNK 21         // problem "nelze rozpoznat jde-li o link na adresar nebo soubor, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETODELETEFILE 22         // problem "nelze smazat soubor, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETODELETEDIR 23          // problem "nelze smazat adresar, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETOCHATTRS 24            // problem "nelze zmenit atributy souboru/adresare, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETORESUME 25             // problem "unable to resume file transfer"
#define ITEMPR_RESUMETESTFAILED 26           // problem "unable to resume file transfer, unexpected tail of file (file has changed)"
#define ITEMPR_TGTFILEREADERROR 27           // problem "chyba cteni ciloveho souboru" (pouziva WinError)
#define ITEMPR_TGTFILEWRITEERROR 28          // problem "chyba zapisu ciloveho souboru" (pouziva WinError)
#define ITEMPR_INCOMPLETEDOWNLOAD 29         // problem "unable to retrieve file from server: %s" (pouziva WinError i ErrAllocDescr)
#define ITEMPR_UNABLETODELSRCFILE 30         // problem "Move: nelze smazat zdrojovy soubor, odpoved serveru: %s" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UPLOADCANNOTCREATETGTDIR 31   // upload: problem "cilovy adresar nelze vytvorit" (pokud FTPMayBeValidNameComponent() vrati FALSE, je ErrAllocDescr==NULL a WinError==NO_ERROR; pokud prekazi soubor/link, je WinError==ERROR_ALREADY_EXISTS; jinak pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UPLOADCANNOTLISTTGTPATH 32    // upload: problem "nelze vylistovat cilovou cestu" (nelze zjistit pripadne kolize jmen) (pouziva WinError i ErrAllocDescr)
#define ITEMPR_UPLOADTGTDIRALREADYEXISTS 33  // upload: problem "cilovy adresar nebo link na adresar jiz existuje"
#define ITEMPR_UPLOADCRDIRAUTORENFAILED 34   // upload: problem "cilovy adresar neumime vytvorit pod zadnym jmenem" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UPLOADCANNOTLISTSRCPATH 35    // upload: problem "nelze vylistovat zdrojovou cestu" (pouziva WinError)
#define ITEMPR_UNABLETOCWDONLYPATH 36        // problem "chyba pri zmene pracovni cesty na serveru, odpoved serveru: %s" - zmena cesty do Path nebo TgtPath (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UNABLETODELETEDISKDIR 37      // problem "nelze smazat adresar na disku, chyba:" (pouziva WinError)
#define ITEMPR_UPLOADCANNOTCREATETGTFILE 38  // upload: problem "cilovy soubor nelze vytvorit nebo otevrit" (pokud FTPMayBeValidNameComponent() vrati FALSE, je ErrAllocDescr==NULL a WinError==NO_ERROR; pokud prekazi adresar/link, je WinError==ERROR_ALREADY_EXISTS; jinak pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_UPLOADCANNOTOPENSRCFILE 39    // problem "zdrojovy soubor nelze otevrit" (pouziva WinError)
#define ITEMPR_UPLOADTGTFILEALREADYEXISTS 40 // upload: problem "cilovy soubor nebo link na soubor jiz existuje"
#define ITEMPR_SRCFILEINUSE 41               // problem "zdrojovy soubor nebo link je zamknuty jinou operaci"
#define ITEMPR_TGTFILEINUSE 42               // problem "cilovy soubor nebo link je zamknuty jinou operaci"
#define ITEMPR_SRCFILEREADERROR 43           // problem "chyba cteni zdrojoveho souboru" (pouziva WinError)
#define ITEMPR_INCOMPLETEUPLOAD 44           // problem "unable to store file to server: %s" (pouziva WinError i ErrAllocDescr)
#define ITEMPR_UNABLETODELETEDISKFILE 45     // problem "nelze smazat soubor na disku, chyba:" (pouziva WinError)
#define ITEMPR_UPLOADASCIIRESUMENOTSUP 46    // problem "resume v ASCII prenosovem rezimu neni podporovan (zkuste resume v binarnim rezimu)"
#define ITEMPR_UPLOADUNABLETORESUMEUNKSIZ 47 // problem "nelze resumnout soubor, protoze neni znama velikost ciloveho souboru"
#define ITEMPR_UPLOADUNABLETORESUMEBIGTGT 48 // problem "nelze resumnout soubor, protoze cilovy soubor je vetsi nez zdrojovy soubor"
#define ITEMPR_UPLOADFILEAUTORENFAILED 49    // upload: problem "cilovy soubor neumime vytvorit pod zadnym jmenem" (pouziva ErrAllocDescr k ulozeni odpovedi serveru - muze byt i viceradkova)
#define ITEMPR_SKIPPEDBYUSER 50              // po stisku Skip buttonu na waiting polozce v operacnim dialogu
#define ITEMPR_UPLOADTESTIFFINISHEDNOTSUP 51 // problem "nelze overit jestli se soubor uspesne uploadnul" (poslali jsme cely soubor + server "jen" neodpovedel, nejspis je soubor OK, ale nejsme schopni to otestovat - duvody: ASCII transfer mode nebo nemame velikost v bytech (ani listing ani prikaz SIZE))

enum CFTPQueueItemAction
{
    fqiaNone,                     // zadna vynucena akce (standardni chovani)
    fqiaUseAutorename,            // ma se pouzit autorename (use alternate name)
    fqiaUseExistingDir,           // ma se pouzit existujici adresar
    fqiaResume,                   // ma se resumnout existujici soubor
    fqiaResumeOrOverwrite,        // ma se resumnout nebo prepsat existujici soubor
    fqiaOverwrite,                // ma se prepsat existujici soubor
    fqiaReduceFileSizeAndResume,  // ma se zkratit a pak resumnout existujici soubor
    fqiaUploadForceAutorename,    // upload: kazdopadne se pouzije autorename (use alternate name)
    fqiaUploadContinueAutorename, // upload: pokracovani autorenamu (use alternate name)
    fqiaUploadTestIfFinished,     // upload: poslali jsme cely soubor + server "jen" neodpovedel, nejspis je soubor OK, otestujeme to
};

class CFTPQueueItemAncestor
{
private:
    CFTPQueueItemState State; // stav polozky (private je kvuli uhlidani zmeny counteru parent polozek pri zmenach stavu)

public:
    CFTPQueueItemAncestor() { State = sqisNone; }

    // vraci State; volat jen z kriticke sekce fronty nebo pred vlozenim do fronty !!!
    CFTPQueueItemState GetItemState() const { return State; }

    // interni nastaveni State: pouzivat jen pred pridanim polozky do fronty
    void SetStateInternal(CFTPQueueItemState state) { State = state; }

    // meni 'State' na 'state' a pripadne meni pocitadla parent polozky
    // POZOR: je mozne volat jen z kriticke sekce QueueCritSect !!!
    void ChangeStateAndCounters(CFTPQueueItemState state, CFTPOperation* oper,
                                CFTPQueue* queue);
};

class CFTPQueueItem : public CFTPQueueItemAncestor
{
public:
    // pristup k datum objektu:
    //   - neni-li jeste soucasti fronty (konstrukce): bez omezeni
    //   - je-li jiz ve fronte: pristup jen z kriticke sekce fronty

    static CRITICAL_SECTION NextItemUIDCritSect; // kriticka sekce pro pristup k NextItemUID
    static int NextItemUID;                      // globalni pocitadlo pro UID polozek (pristupovat jen v sekci NextItemUIDCritSect!)

    int UID;       // unikatni cislo polozky
    int ParentUID; // UID nadrazene polozky (-1=nadrazena je operace)

    CFTPQueueItemType Type; // typ polozky (pro pretypovani na spravny typ objektu)
    DWORD ProblemID;        // doplnuje State: viz konstanty ITEMPR_XXX
    DWORD WinError;         // muze obsahovat kod nastale Windows chyby (doplnuje ProblemID)
    char* ErrAllocDescr;    // muze obsahovat alokovany text s popisem chyby (doplnuje ProblemID)

    DWORD ErrorOccurenceTime; // "cas" vzniku chyby (pouziva se pro dodrzeni poradi reseni chyb podle jejich vzniku); -1 = zadna chyba nevznikla

    CFTPQueueItemAction ForceAction; // userem vynucena akce (napr. zadany autorename ze Solve Error dialogu)

    char* Path; // cesta ke zpracovavanemu souboru/adresari (lokalni cesta na serveru nebo Windowsova cesta)
    char* Name; // jmeno zpracovavaneho souboru/adresare (jmeno bez cesty)

public:
    CFTPQueueItem();
    virtual ~CFTPQueueItem();

    // vraci TRUE pokud jde o "explore" nebo "resolve" polozku
    // POZOR: volat jen z kriticke sekce fronty
    BOOL IsExploreOrResolveItem() const { return Type < fqitLastResolveOrExploreItem; }

    // vraci TRUE pokud jde o polozku ve stavu sqisFailed nebo sqisUserInputNeeded;
    // pouziva se misto HasErrorToSolve() pri ukladani ErrorOccurenceTime, protoze
    // v miste pouziti jeste neni nastaveny ProblemID a tudiz HasErrorToSolve()
    // nelze pouzit (takhle se oznaci pro zpracovani i neresitelne chyby, coz nevadi,
    // protoze je v SearchItemWithNewError preskocime)
    // POZOR: volat jen z kriticke sekce fronty
    BOOL IsItemInSimpleErrorState() const { return GetItemState() == sqisFailed || GetItemState() == sqisUserInputNeeded; }

    // nastavi zakladni parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItem(int parentUID, CFTPQueueItemType type, CFTPQueueItemState state,
                 DWORD problemID, const char* path, const char* name);

    // enabler pro tlacitka v operacnim dialogu: vraci TRUE pokud ma smysl tlacitko
    // "Solve Error"; v 'canSkip' (neni-li NULL) vraci TRUE pokud ma smysl
    // tlacitko "Skip"; v 'canRetry' (neni-li NULL) vraci TRUE pokud ma smysl
    // tlacitko "Retry"
    // POZOR: volat jen z kriticke sekce fronty
    BOOL HasErrorToSolve(BOOL* canSkip, BOOL* canRetry);

    // vraci textovy popis problemu vyjadreneho ProblemID + WinError + ErrAllocDescr
    // POZOR: volat jen z kriticke sekce fronty
    void GetProblemDescr(char* buf, int bufSize);
};

//
// ****************************************************************************
// CFTPQueueItemDir
//

class CFTPQueueItemDir : public CFTPQueueItem
{
public:
    int ChildItemsNotDone;  // pocet nedokoncenych "child" polozek (krome typu sqisDone)
    int ChildItemsSkipped;  // pocet skipnutych "child" polozek (typ sqisSkipped)
    int ChildItemsFailed;   // pocet failnutych "child" polozek (typ sqisFailed a sqisForcedToFail)
    int ChildItemsUINeeded; // pocet user-input-needed "child" polozek (typ sqisUserInputNeeded)

public:
    CFTPQueueItemDir();

    // nastavi pridane parametry polozky; vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetItemDir(int childItemsNotDone, int childItemsSkipped, int childItemsFailed,
                    int childItemsUINeeded);

    // nastavi zadane pocty k jednotlivym pocitadlum a pripadne zmeni stav polozky
    // (jen z sqisWaiting na sqisDelayed nebo sqisForcedToFail)
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty)
    void SetStateAndNotDoneSkippedFailed(int childItemsNotDone, int childItemsSkipped,
                                         int childItemsFailed, int childItemsUINeeded);

    // vraci stav polozky urceny podle pocitadel (sqisWaiting, sqisDelayed nebo sqisForcedToFail)
    CFTPQueueItemState GetStateFromCounters();
};

//
// ****************************************************************************
// CFTPQueueItemDel
//

class CFTPQueueItemDel : public CFTPQueueItem
{
public:
    unsigned IsHiddenFile : 1; // TRUE/FALSE = jde/nejde o skryty soubor nebo link (mozne potvrzovani vymazu, viz CFTPOperation::ConfirmDelOnHiddenFile); POZOR: po potvrzeni dotazu userem se prirazuje FALSE

public:
    CFTPQueueItemDel();

    // nastavi pridane parametry polozky; vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetItemDel(int isHiddenFile);
};

//
// ****************************************************************************
// CFTPQueueItemDelExplore
//

class CFTPQueueItemDelExplore : public CFTPQueueItem
{
public:
    unsigned IsTopLevelDir : 1; // TRUE/FALSE = jde/nejde o adresar primo oznaceny v panelu - problem "mazani neprazdnych adresaru" se tyka jen techto adresaru, viz CFTPOperation::ConfirmDelOnNonEmptyDir; POZOR: po potvrzeni dotazu userem se prirazuje FALSE
    unsigned IsHiddenDir : 1;   // TRUE/FALSE = jde/nejde o skryty adresar (mozne potvrzovani vymazu, viz CFTPOperation::ConfirmDelOnHiddenDir); POZOR: po potvrzeni dotazu userem se prirazuje FALSE

public:
    CFTPQueueItemDelExplore();

    // nastavi pridane parametry polozky; vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetItemDelExplore(int isTopLevelDir, int isHiddenDir);
};

//
// ****************************************************************************
// CFTPQueueItemCopyOrMove
//

// stav ciloveho souboru (konstanty pro CFTPQueueItemCopyOrMove::TgtFileState)
#define TGTFILESTATE_UNKNOWN 0     // stav souboru zatim nebyl zjisten
#define TGTFILESTATE_TRANSFERRED 1 // soubor byl uspesne prenesen (hodi se pokud se pri presunu nepodaril vymaz zdrojoveho souboru)
#define TGTFILESTATE_CREATED 2     // soubor byl FTP klientem primo vytvoren nebo resumnut s moznosti overwritu
#define TGTFILESTATE_RESUMED 3     // soubor byl FTP klientem resumnut bez moznosti overwritu (k overwritu muze dojit jen pokud jiz downloadla cast souboru je prilis mala, viz Config.ResumeMinFileSize)

class CFTPQueueItemCopyOrMove : public CFTPQueueItem
{
public:
    char* TgtPath; // cesta k cilovemu souboru (Windowsova cesta)
    char* TgtName; // jmeno ciloveho souboru (jmeno bez cesty)

    CQuadWord Size; // velikost souboru (CQuadWord(-1, -1) = velikost neni znama - napr. linky)

    CFTPDate Date; // datum zdrojoveho souboru (nastavi se po dokonceni operace na cilovem souboru); je-li Date.Day==0, jde o "prazdnou hodnotu" (datum se nema menit)
    CFTPTime Time; // cas zdrojoveho souboru (nastavi se po dokonceni operace na cilovem souboru); je-li Time.Hour==24, jde o "prazdnou hodnotu" (cas se nema menit)

    unsigned AsciiTransferMode : 1;           // TRUE/FALSE = ASCII/Binary transfer mode
    unsigned IgnoreAsciiTrModeForBinFile : 1; // TRUE/FALSE = ignoruj/kontroluj jestli se neprenasi v ASCII rezimu binarni soubor
    unsigned SizeInBytes : 1;                 // TRUE/FALSE = Size je v bytech/blocich
    unsigned TgtFileState : 2;                // viz konstanty TGTFILESTATE_XXX
    unsigned DateAndTimeValid : 1;            // TRUE/FALSE = Date+Time jsou platne a ma dojit k jejich nastaveni na cilovem souboru po dokonceni operace

public:
    CFTPQueueItemCopyOrMove();
    virtual ~CFTPQueueItemCopyOrMove();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemCopyOrMove(const char* tgtPath, const char* tgtName, const CQuadWord& size,
                           int asciiTransferMode, int sizeInBytes, int tgtFileState,
                           BOOL dateAndTimeValid, const CFTPDate& date, const CFTPTime& time);
};

//
// ****************************************************************************
// CFTPQueueItemCopyOrMoveUpload
//

// stav ciloveho souboru (konstanty pro CFTPQueueItemCopyOrMoveUpload::TgtFileState)
#define UPLOADTGTFILESTATE_UNKNOWN 0     // stav souboru zatim nebyl zjisten
#define UPLOADTGTFILESTATE_TRANSFERRED 1 // soubor byl uspesne prenesen (hodi se pokud se pri presunu nepodaril vymaz zdrojoveho souboru)
#define UPLOADTGTFILESTATE_CREATED 2     // soubor byl FTP klientem primo vytvoren nebo resumnut s moznosti overwritu
#define UPLOADTGTFILESTATE_RESUMED 3     // soubor byl FTP klientem resumnut bez moznosti overwritu (k overwritu muze dojit jen pokud jiz uploadla cast souboru je prilis mala, viz Config.ResumeMinFileSize)

class CFTPQueueItemCopyOrMoveUpload : public CFTPQueueItem
{
public:
    char* TgtPath; // cesta k cilovemu souboru (lokalni cesta na serveru)
    char* TgtName; // jmeno ciloveho souboru (jmeno bez cesty)

    CQuadWord Size;      // velikost souboru
    int AutorenamePhase; // pri auto-rename jde o fazi generovani jmen
    char* RenamedName;   // jmeno vytvorene auto-renamem: neni NULL jen kdyz prave probiha upload do noveho jmena

    CQuadWord SizeWithCRLF_EOLs; // velikost souboru pri pouziti CRLF koncu radek (nastavuje se jen pro fqiaUploadTestIfFinished)
    CQuadWord NumberOfEOLs;      // pocet koncu radek (nastavuje se jen pro fqiaUploadTestIfFinished)

    unsigned AsciiTransferMode : 1;           // TRUE/FALSE = ASCII/Binary transfer mode
    unsigned IgnoreAsciiTrModeForBinFile : 1; // TRUE/FALSE = ignoruj/kontroluj jestli se neprenasi v ASCII rezimu binarni soubor
    unsigned TgtFileState : 2;                // viz konstanty UPLOADTGTFILESTATE_XXX

public:
    CFTPQueueItemCopyOrMoveUpload();
    virtual ~CFTPQueueItemCopyOrMoveUpload();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemCopyOrMoveUpload(const char* tgtPath, const char* tgtName, const CQuadWord& size,
                                 int asciiTransferMode, int tgtFileState);
};

//
// ****************************************************************************
// CFTPQueueItemCopyMoveExplore
//

// stav ciloveho adresare (konstanty pro CFTPQueueItemCopyMoveExplore::TgtDirState)
#define TGTDIRSTATE_UNKNOWN 0 // stav adresare zatim nebyl zjisten
#define TGTDIRSTATE_READY 1   // cilovy adresar uz byl FTP klientem primo vytvoren nebo uz bylo odsouhlaseno pouziti existujiciho adresare

class CFTPQueueItemCopyMoveExplore : public CFTPQueueItem
{
public:
    char* TgtPath; // cesta k cilovemu adresari (Windowsova cesta)
    char* TgtName; // jmeno ciloveho adresare (jmeno bez cesty)

    unsigned TgtDirState : 1; // viz konstanty TGTDIRSTATE_XXX

public:
    CFTPQueueItemCopyMoveExplore();
    virtual ~CFTPQueueItemCopyMoveExplore();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemCopyMoveExplore(const char* tgtPath, const char* tgtName, int tgtDirState);
};

//
// ****************************************************************************
// CFTPQueueItemCopyMoveUploadExplore
//

// stav ciloveho adresare (konstanty pro CFTPQueueItemCopyMoveUploadExplore::TgtDirState)
#define UPLOADTGTDIRSTATE_UNKNOWN 0 // stav adresare zatim nebyl zjisten
#define UPLOADTGTDIRSTATE_READY 1   // cilovy adresar uz byl FTP klientem primo vytvoren nebo uz bylo odsouhlaseno pouziti existujiciho adresare

class CFTPQueueItemCopyMoveUploadExplore : public CFTPQueueItem
{
public:
    char* TgtPath; // cesta k cilovemu adresari (lokalni cesta na serveru)
    char* TgtName; // jmeno ciloveho adresare (jmeno bez cesty)

    unsigned TgtDirState : 1; // viz konstanty UPLOADTGTDIRSTATE_XXX

public:
    CFTPQueueItemCopyMoveUploadExplore();
    virtual ~CFTPQueueItemCopyMoveUploadExplore();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemCopyMoveUploadExplore(const char* tgtPath, const char* tgtName, int tgtDirState);
};

//
// ****************************************************************************
// CFTPQueueItemChAttr
//

class CFTPQueueItemChAttr : public CFTPQueueItem
{
public:
    WORD Attr;        // pozadovany mod (nutny prevod na retezec do octa-cisla)
    BYTE AttrErr;     // TRUE = neznamy atribut ma byt zachovan, coz neumime (FALSE = vse OK)
    char* OrigRights; // puvodni prava souboru (!=NULL jen pokud obsahuji neznama prava + pokud byl vubec nalezen sloupec s pravy)

public:
    CFTPQueueItemChAttr();
    virtual ~CFTPQueueItemChAttr();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemChAttr(WORD attr, const char* origRights, BYTE attrErr);
};

//
// ****************************************************************************
// CFTPQueueItemChAttrDir
//
// neni shodny s CFTPQueueItemChAttr, ma pocitadlo (dedi se od CFTPQueueItemDir)

class CFTPQueueItemChAttrDir : public CFTPQueueItemDir
{
public:
    WORD Attr;        // pozadovany mod (nutny prevod na retezec do octa-cisla)
    BYTE AttrErr;     // TRUE = neznamy atribut ma byt zachovan, coz neumime (FALSE = vse OK)
    char* OrigRights; // puvodni prava adresare (!=NULL jen pokud obsahuji neznama prava + pokud byl vubec nalezen sloupec s pravy)

public:
    CFTPQueueItemChAttrDir();
    virtual ~CFTPQueueItemChAttrDir();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    // POZOR: dedi se od CFTPQueueItemDir - VOLAT I CFTPQueueItemDir::SetItemDir() !!!
    void SetItemChAttrDir(WORD attr, const char* origRights, BYTE attrErr);
};

//
// ****************************************************************************
// CFTPQueueItemChAttrExplore
//

class CFTPQueueItemChAttrExplore : public CFTPQueueItem
{
public:
    char* OrigRights; // puvodni prava adresare pro vypocet Attr zkoumaneho adresare (!=NULL pokud byl nalezen sloupec s pravy)

public:
    CFTPQueueItemChAttrExplore();
    virtual ~CFTPQueueItemChAttrExplore();

    // nastavi pridane parametry polozky
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim polozky do fronty) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetItemChAttrExplore(const char* origRights);
};

//
// ****************************************************************************
// CFTPQueue
//

class CFTPQueue
{
protected:
    // kriticka sekce pro pristup k datum objektu
    // POZOR: z teto sekce se nesmi vstupovat do zadnych jinych kritickych sekci !!!
    CRITICAL_SECTION QueueCritSect;

    TIndirectArray<CFTPQueueItem> Items; // polozky fronty

    // jednomistna cache pro urychleni FindItemWithUID() - pozor: nemusi byt platna:
    int LastFoundUID;   // UID posledne nalezene polozky
    int LastFoundIndex; // index posledne nalezene polozky (vzdy nezaporne cislo, i po inicializaci)

    int FirstWaitingItemIndex;          // prvni polozka ve fronte (poli Items), ktera muze (ale nemusi) byt ve stavu sqisWaiting a pokud je GetOnlyExploreAndResolveItems==TRUE, zaroven muze byt "explore" a "resolve" (pred timto indexem uz proste nejsou)
    BOOL GetOnlyExploreAndResolveItems; // TRUE = zatim z fronty vracime jen "explore" a "resolve" polozky (FALSE jen kdyz zadna takova polozka neni ve "waiting" stavu) (Type < fqitLastResolveOrExploreItem)

    int ExploreAndResolveItemsCount;            // pocet polozek fronty typu "explore" a "resolve" (Type < fqitLastResolveOrExploreItem)
    int DoneOrSkippedItemsCount;                // pocet polozek fronty ve stavu sqisDone nebo sqisSkipped
    int WaitingOrProcessingOrDelayedItemsCount; // pocet polozek fronty ve stavu sqisWaiting, sqisProcessing nebo sqisDelayed

    CQuadWord DoneOrSkippedByteSize;                  // soucet znamych velikosti souboru v bytech polozek typu fqitCopyFileOrFileLink a fqitMoveFileOrFileLink ve stavu sqisDone nebo sqisSkipped
    CQuadWord DoneOrSkippedBlockSize;                 // soucet znamych velikosti souboru v blocich polozek typu fqitCopyFileOrFileLink a fqitMoveFileOrFileLink ve stavu sqisDone nebo sqisSkipped
    CQuadWord WaitingOrProcessingOrDelayedByteSize;   // soucet znamych velikosti souboru v bytech polozek typu fqitCopyFileOrFileLink a fqitMoveFileOrFileLink ve stavu sqisWaiting, sqisProcessing nebo sqisDelayed
    CQuadWord WaitingOrProcessingOrDelayedBlockSize;  // soucet znamych velikosti souboru v blocich polozek typu fqitCopyFileOrFileLink a fqitMoveFileOrFileLink ve stavu sqisWaiting, sqisProcessing nebo sqisDelayed
    CQuadWord DoneOrSkippedUploadSize;                // soucet velikosti souboru polozek typu fqitUploadCopyFile a fqitUploadMoveFile ve stavu sqisDone nebo sqisSkipped
    CQuadWord WaitingOrProcessingOrDelayedUploadSize; // soucet velikosti souboru polozek typu fqitUploadCopyFile a fqitUploadMoveFile ve stavu sqisWaiting, sqisProcessing nebo sqisDelayed
    int CopyUnknownSizeCount;                         // pocet polozek, ktere nemaji znamou velikost pri Copy/Move operaci (explore/delete-dir/resolve/copy-unknown-size-items)
    int CopyUnknownSizeCountIfUnknownBlockSize;       // pocet polozek, ktere nemaji znamou velikost pokud neni znama velikost bloku v bytech pri Copy/Move operaci (explore/delete-dir/resolve/copy-unknown-size-items)

    DWORD LastErrorOccurenceTime;      // "cas" vzniku posledni chyby (inicializovany na -1)
    DWORD LastFoundErrorOccurenceTime; // "cas" posledni nalezene polozky s chybou nebo "cas", pred kterym uz zadna polozka s chybou neexistuje

public:
    CFTPQueue();
    ~CFTPQueue();

    // prida do fronty novou polozku; vraci TRUE pri uspechu
    BOOL AddItem(CFTPQueueItem* newItem);

    // vraci pocet polozek ve fronte
    int GetCount();

    // pomocna metoda: upravi pocitadla ExploreAndResolveItemsCount, DoneOrSkippedItemsCount
    // a WaitingOrProcessingOrDelayedItemsCount podle polozky 'item'; 'add' je
    // TRUE/FALSE pokud se pridava/vyhazuje polozka 'item' z fronty
    // vola se pri pridani polozky do fronty a pri zmene State polozky
    // POZOR: je mozne volat jen z kriticke sekce QueueCritSect !!!
    void UpdateCounters(CFTPQueueItem* item, BOOL add);

    // metody pro zamknuti/odemknuti fronty pro volani vice operaci "najednou"
    // (jde jen o vstup/vystup z krit. sekce QueueCritSect)
    void LockForMoreOperations();
    void UnlockForMoreOperations();

    // nahradi polozku s UID 'itemUID' seznamem polozek 'items' (pocet polozek je 'itemsCount');
    // vraci TRUE pri uspechu (v tomto pripade jiz doslo k uvolneni polozky s UID 'itemUID')
    // POZOR: nutne pouzit LockForMoreOperations() a UnlockForMoreOperations(), protoze po volani
    //        ReplaceItemWithListOfItems je treba jeste pred pristupem dalsich threadu srovnat
    //        pocitadla (zajistit konzistenci dat)
    BOOL ReplaceItemWithListOfItems(int itemUID, CFTPQueueItem** items, int itemsCount);

    // pricte dane pocty k pocitadlum ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded
    // dir-polozky 'itemDirUID' (polozka musi byt potomek CFTPQueueItemDir); zaroven muze
    // zmenit stav dir-polozky (na sqisDelayed (provadi se child polozky), sqisWaiting (childy
    // jsou hotove, muze se provest dir-polozka) nebo sqisForcedToFail (nelze dokoncit childy,
    // doslo k chybam/skipum)), v tomto pripade zajisti zmenu pocitadel parenta dir-polozky
    // (funguje rekurzivne, takze pripadne zmeni pocitadla i parentu parentu dir-polozky, atd.)
    void AddToNotDoneSkippedFailed(int itemDirUID, int notDone, int skipped, int failed,
                                   int uiNeeded, CFTPOperation* oper);

    // vraci pocet polozek ve stavu sqisUserInputNeeded, sqisSkipped, sqisFailed nebo sqisForcedToFail
    // ve fronte; je-li 'onlyUINeeded' TRUE, plni pole 'UINeededArr' polozkami ve stavu
    // sqisUserInputNeeded, sqisSkipped, sqisFailed nebo sqisForcedToFail; neni-li 'focusedItemUID' -1,
    // vraci v 'indexInAll' index polozky s UID 'focusedItemUID' ve fronte + vraci
    // v 'indexInUIN' index polozky s UID 'focusedItemUID' v poli polozek ve stavu
    // sqisUserInputNeeded, sqisSkipped, sqisFailed nebo sqisForcedToFail; pokud nenajde
    // polozku s UID 'focusedItemUID', vraci 'indexInAll' i 'indexInUIN' rovno -1
    int GetUserInputNeededCount(BOOL onlyUINeeded, TDirectArray<DWORD>* UINeededArr,
                                int focusedItemUID, int* indexInAll, int* indexInUIN);

    // vraci UID polozky na indexu 'index', pri neplatnem indexu vraci -1
    int GetItemUID(int index);

    // vraci index polozky s UID 'itemUID', pri neznamem UID vraci -1
    int GetItemIndex(int itemUID);

    // vraci data pro zobrazeni polozky operace s indexem 'index' v listview v dialogu operace;
    // 'buf'+'bufSize' je buffer pro text vraceny v 'lvdi' (meni se tri cyklicky,
    // aby se splnily naroky LVN_GETDISPINFO); pokud index neni
    // platny, nedela nic (refresh listview uz je na ceste)
    void GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize);

    // enabler tlacitek v operacnim dialogu pro polozku na indexu 'index':
    // vraci TRUE pokud ma smysl "Solve Error"; vraci FALSE pro neplatny index;
    // v 'canSkip' (neni-li NULL) vraci TRUE pokud ma smysl "Skip";
    // v 'canRetry' (neni-li NULL) vraci TRUE pokud ma smysl "Retry"
    BOOL IsItemWithErrorToSolve(int index, BOOL* canSkip, BOOL* canRetry);

    // provede Skip polozky s UID 'UID' (zmena na stav "skipped"); vraci index
    // zmenene polozky nebo -1 pri chybe
    int SkipItem(int UID, CFTPOperation* oper);

    // provede Retry polozky s UID 'UID' (zmena na stav "waiting"); vraci index
    // zmenene polozky nebo -1 pri chybe
    int RetryItem(int UID, CFTPOperation* oper);

    // provede Solve Error polozky s UID 'UID'; vraci index zmenene polozky nebo
    // -1 pri vice zmenenych polozkach nebo -2 pri zadne zmene; 'oper' je operace,
    // do ktere polozka (a cela tato fronta) patri
    int SolveErrorOnItem(HWND parent, int UID, CFTPOperation* oper);

    // hleda ve fronte prvni polozku ve stavu sqisWaiting, vraci ukazatel na ni (nebo NULL
    // pokud takova polozka neexistuje); prepina nalezenou polozku do stavu sqisProcessing
    // (aby nemohla byt hned znovu nalezena)
    CFTPQueueItem* GetNextWaitingItem(CFTPOperation* oper);

    // vraci polozku 'item' do stavu sqisWaiting (worker ji nedokaze zpracovat, timto
    // umozni jinym workerum aby polozku zpracovali)
    void ReturnToWaitingItems(CFTPQueueItem* item, CFTPOperation* oper);

    // nastavuje stav polozky 'item'; 'errAllocDescr' je NULL nebo alokovany popis chyby
    // (volajici predava buffer alokovany funkci malloc, po volani teto metody je jiz
    // buffer pro volajiciho neplatny)
    void UpdateItemState(CFTPQueueItem* item, CFTPQueueItemState state, DWORD problemID,
                         DWORD winError, char* errAllocDescr, CFTPOperation* oper);

    // priradi 'renamedName' do RenamedName polozky 'item' (predchozi hodnotu RenamedName uvolni)
    void UpdateRenamedName(CFTPQueueItemCopyOrMoveUpload* item, char* renamedName);

    // priradi 'autorenamePhase' do AutorenamePhase polozky 'item'
    void UpdateAutorenamePhase(CFTPQueueItemCopyOrMoveUpload* item, int autorenamePhase);

    // priradi RenamedName do TgtName polozky 'item' (predchozi hodnotu TgtName uvolni)
    void ChangeTgtNameToRenamedName(CFTPQueueItemCopyOrMoveUpload* item);

    // priradi 'tgtName' do TgtName polozky 'item' (predchozi hodnotu TgtName uvolni)
    void UpdateTgtName(CFTPQueueItemCopyMoveExplore* item, char* tgtName);

    // priradi 'tgtName' do TgtName polozky 'item' (predchozi hodnotu TgtName uvolni)
    void UpdateTgtName(CFTPQueueItemCopyMoveUploadExplore* item, char* tgtName);

    // priradi 'tgtName' do TgtName polozky 'item' (predchozi hodnotu TgtName uvolni)
    void UpdateTgtName(CFTPQueueItemCopyOrMove* item, char* tgtName);

    // priradi 'tgtDirState' do TgtDirState polozky 'item'
    void UpdateTgtDirState(CFTPQueueItemCopyMoveExplore* item, unsigned tgtDirState);

    // priradi 'tgtDirState' do TgtDirState polozky 'item'
    void UpdateUploadTgtDirState(CFTPQueueItemCopyMoveUploadExplore* item, unsigned tgtDirState);

    // priradi 'forceAction' do ForceAction polozky 'item'
    void UpdateForceAction(CFTPQueueItem* item, CFTPQueueItemAction forceAction);

    // priradi 'tgtFileState' do TgtFileState polozky 'item'
    void UpdateTgtFileState(CFTPQueueItemCopyOrMove* item, unsigned tgtFileState);

    // priradi 'attrErr' do AttrErr polozky 'item'
    void UpdateAttrErr(CFTPQueueItemChAttrDir* item, BYTE attrErr);

    // priradi 'attrErr' do AttrErr polozky 'item'
    void UpdateAttrErr(CFTPQueueItemChAttr* item, BYTE attrErr);

    // priradi 'isHiddenDir' do IsHiddenDir polozky 'item'
    void UpdateIsHiddenDir(CFTPQueueItemDelExplore* item, BOOL isHiddenDir);

    // priradi 'isHiddenFile' do IsHiddenFile polozky 'item'
    void UpdateIsHiddenFile(CFTPQueueItemDel* item, BOOL isHiddenFile);

    // priradi 'size' do Size a 'sizeInBytes' do SizeInBytes polozky 'item' + upravi
    // TotalSizeInBytes a TotalSizeInBlocks v operaci ('oper')
    void UpdateFileSize(CFTPQueueItemCopyOrMove* item, CQuadWord const& size,
                        BOOL sizeInBytes, CFTPOperation* oper);

    // priradi 'asciiTransferMode' do AsciiTransferMode polozky 'item'
    void UpdateAsciiTransferMode(CFTPQueueItemCopyOrMove* item, BOOL asciiTransferMode);

    // priradi 'ignoreAsciiTrModeForBinFile' do IgnoreAsciiTrModeForBinFile polozky 'item'
    void UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMove* item, BOOL ignoreAsciiTrModeForBinFile);

    // pro upload: priradi 'asciiTransferMode' do AsciiTransferMode polozky 'item'
    void UpdateAsciiTransferMode(CFTPQueueItemCopyOrMoveUpload* item, BOOL asciiTransferMode);

    // pro upload: priradi 'ignoreAsciiTrModeForBinFile' do IgnoreAsciiTrModeForBinFile polozky 'item'
    void UpdateIgnoreAsciiTrModeForBinFile(CFTPQueueItemCopyOrMoveUpload* item, BOOL ignoreAsciiTrModeForBinFile);

    // priradi 'tgtFileState' do TgtFileState polozky 'item'
    void UpdateTgtFileState(CFTPQueueItemCopyOrMoveUpload* item, unsigned tgtFileState);

    // priradi 'size' do Size polozky 'item' + upravi TotalSizeInBytes v operaci ('oper')
    void UpdateFileSize(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& size,
                        CFTPOperation* oper);

    // nastavi SizeWithCRLF_EOLs a NumberOfEOLs polozky 'item'
    void UpdateTextFileSizes(CFTPQueueItemCopyOrMoveUpload* item, CQuadWord const& sizeWithCRLF_EOLs,
                             CQuadWord const& numberOfEOLs);

    // jen pro debugovaci ucely: overi pocitadla ChildItemsNotDone+ChildItemsSkipped+
    // ChildItemsFailed+ChildItemsUINeeded ve vsech polozkach a zaroven i v operaci,
    // pri chybe hazi TRACE_E
    void DebugCheckCounters(CFTPOperation* oper);

    // vraci progres na zaklade pomeru mezi hotovymi ('doneOrSkippedCount') a vsemi
    // polozkami ('totalCount'); v 'unknownSizeCount' vraci pocet nedokoncenych polozek
    // s neznamou velikosti; ve 'waitingCount' vraci pocet polozek, ktere cekaji na
    // zpracovani (waiting+delayed+processing)
    int GetSimpleProgress(int* doneOrSkippedCount, int* totalCount, int* unknownSizeCount,
                          int* waitingCount);

    // vraci informace pro download - copy/move progres; v 'downloaded' vraci soucet downloadlych
    // velikosti v bytech; v 'unknownSizeCount' vraci pocet nedokoncenych polozek s
    // neznamou velikosti; v 'totalWithoutErrors' vraci soucet velikosti (v bytech)
    // polozek, ktere nejsou v chybovem stavu (dotaz userovi je tez chybovy stav)
    void GetCopyProgressInfo(CQuadWord* downloaded, int* unknownSizeCount,
                             CQuadWord* totalWithoutErrors, int* errorsCount,
                             int* doneOrSkippedCount, int* totalCount, CFTPOperation* oper);

    // vraci informace pro upload - copy/move progres; v 'uploaded' vraci soucet uploadlych
    // velikosti v bytech; v 'unknownSizeCount' vraci pocet nedokoncenych polozek s
    // neznamou velikosti; v 'totalWithoutErrors' vraci soucet velikosti (v bytech)
    // polozek, ktere nejsou v chybovem stavu (dotaz userovi je tez chybovy stav)
    void GetCopyUploadProgressInfo(CQuadWord* uploaded, int* unknownSizeCount,
                                   CQuadWord* totalWithoutErrors, int* errorsCount,
                                   int* doneOrSkippedCount, int* totalCount, CFTPOperation* oper);

    // vraci TRUE pokud jsou vsechny polozky fronty ve stavu sqisDone
    BOOL AllItemsDone();

    // navysi hodnotu LastErrorOccurenceTime o jednu a vrati novou hodnotu LastErrorOccurenceTime;
    // POZOR: volat jen v kriticke sekci QueueCritSect !!!
    DWORD GiveLastErrorOccurenceTime() { return ++LastErrorOccurenceTime; }

    // hleda UID polozky, ktera potrebuje otevrit Solve Error dialog (objevila se
    // v ni "nova" (user ji jeste nevidel) chyba); vraci TRUE pokud se takovou
    // polozku podarilo nalezt, jeji UID se vraci v 'itemUID' + jeji index
    // ve fronte v 'itemIndex' (index se muze ihned zmenit, proto je treba ho
    // brat jen jako orientacni)
    BOOL SearchItemWithNewError(int* itemUID, int* itemIndex);

protected:
    // hleda polozku s UID 'UID'; vraci NULL jen pokud nebyla polozka nalezena
    // POZOR: volat jen v kriticke sekci QueueCritSect !!!
    CFTPQueueItem* FindItemWithUID(int UID);

    // aktualizuje GetOnlyExploreAndResolveItems a FirstWaitingItemIndex pri zmene polozky
    // na stav sqisWaiting; 'itemIndex' je index menene polozky; 'exploreOrResolveItem'
    // je vysledek metody IsExploreOrResolveItem() polozky
    // POZOR: volat jen v kriticke sekci QueueCritSect !!!
    void HandleFirstWaitingItemIndex(BOOL exploreOrResolveItem, int itemIndex);
};

//
// ****************************************************************************
// CFTPDiskThread
//

enum CFTPDiskWorkType
{
    fdwtNone,               // inicializacni hodnota
    fdwtCreateDir,          // vytvareni adresare
    fdwtCreateFile,         // vytvareni/otevirani souboru v situaci: stav souboru je neznamy
    fdwtRetryCreatedFile,   // vytvareni/otevirani souboru v situaci: soubor uz se povedlo vytvorit/prepsat/resumnout_s_moznosti_prepisu
    fdwtRetryResumedFile,   // vytvareni/otevirani souboru v situaci: soubor uz se povedlo resumnout
    fdwtCheckOrWriteFile,   // overovani obsahu nebo zapis do souboru (pri "resume" se kontroluje konec souboru, zapisuje se az za koncem souboru)
    fdwtCreateAndWriteFile, // neni-li soubor otevreny, vytvori ho (prepise pripadny existujici soubor) + zapise flush data do souboru (na pozici aktualniho seeku, takze normalne na konec souboru)
    fdwtListDir,            // vylistuje adresar na disku
    fdwtDeleteDir,          // smazani adresare
    fdwtOpenFileForReading, // otevreni zdrojoveho souboru na disku pro cteni (upload)
    fdwtReadFile,           // cteni casti souboru do bufferu (pro upload)
    fdwtReadFileInASCII,    // cteni casti souboru pro ASCII transfer mode (prevod vsech EOLu na CRLF) do bufferu (pro upload)
    fdwtDeleteFile,         // smazani souboru na disku (zdrojovy soubor pri upload-Move)
};

struct CDiskListingItem
{
    char* Name;     // jmeno souboru/adresare
    BOOL IsDir;     // TRUE pro adresar, FALSE pro soubor
    CQuadWord Size; // jen soubor: velikost (v bytech)

    CDiskListingItem(const char* name, BOOL isDir, const CQuadWord& size);
    ~CDiskListingItem()
    {
        if (Name != NULL)
            free(Name);
    }
    BOOL IsGood() { return Name != NULL; }
};

struct CFTPDiskWork
{
    int SocketMsg; // parametry pro postnuti zpravy pri dokonceni prace
    int SocketUID;
    DWORD MsgID;

    CFTPDiskWorkType Type; // typ prace na disku

    char Path[MAX_PATH]; // cilova cesta (napr. fdwtCheckOrWriteFile a fdwtCreateAndWriteFile nepouzivaji = "")
    char Name[MAX_PATH]; // IN/OUT cilove jmeno (jmeno se muze zmenit pri autorename) (napr. fdwtCheckOrWriteFile nepouziva = "") (u fdwtCreateAndWriteFile je zde tgt-full-file-name)

    CFTPQueueItemAction ForceAction; // userem vynucena akce (napr. zadany autorename ze Solve Error dialogu)

    BOOL AlreadyRenamedName; // TRUE pokud je Name jiz prejmenovane jmeno (jmeno zdrojoveho souboru je jine)

    unsigned CannotCreateDir : 2;    // viz konstanty CANNOTCREATENAME_XXX
    unsigned DirAlreadyExists : 2;   // viz konstanty DIRALREADYEXISTS_XXX
    unsigned CannotCreateFile : 2;   // viz konstanty CANNOTCREATENAME_XXX
    unsigned FileAlreadyExists : 3;  // viz konstanty FILEALREADYEXISTS_XXX
    unsigned RetryOnCreatedFile : 3; // viz konstanty RETRYONCREATFILE_XXX
    unsigned RetryOnResumedFile : 3; // viz konstanty RETRYONRESUMFILE_XXX

    // info pro fdwtCheckOrWriteFile:
    // testovat obsah souboru WorkFile od offsetu CheckFromOffset do WriteOrReadFromOffset (jsou-li shodne,
    // nic se netestuje), jestli se shoduje s obsahem bufferu FlushDataBuffer (data jsou od zacatku);
    // od offsetu WriteOrReadFromOffset v souboru WorkFile zapisovat FlushDataBuffer (data jsou od
    // (WriteOrReadFromOffset - CheckFromOffset)); ValidBytesInFlushDataBuffer je pocet bytu v bufferu FlushDataBuffer
    //
    // info pro fdwtReadFile+fdwtReadFileInASCII:
    // CheckFromOffset se nepouziva, soubor se cte od offsetu WriteOrReadFromOffset, cte se do bufferu FlushDataBuffer
    // (u fdwtReadFileInASCII dochazi navic ke konverzi LF -> CRLF), ve ValidBytesInFlushDataBuffer se vraci pocet
    // bytu umistenych do bufferu FlushDataBuffer (po dosazeni konce souboru zde bude nula), navic se
    // ve WriteOrReadFromOffset vraci novy offset v souboru (u fdwtReadFileInASCII dochazi ke konverzi LF -> CRLF,
    // takze novy offset nelze dopocitat z predchoziho offsetu a ValidBytesInFlushDataBuffer), u fdwtReadFileInASCII
    // se v EOLsInFlushDataBuffer vraci pocet koncu radek v bufferu FlushDataBuffer
    CQuadWord CheckFromOffset;
    CQuadWord WriteOrReadFromOffset;
    char* FlushDataBuffer;
    int ValidBytesInFlushDataBuffer;
    int EOLsInFlushDataBuffer;
    HANDLE WorkFile;

    // vysledek operace na disku se vraci v nasledujicich promennych:
    DWORD ProblemID;                               // neni-li ITEMPR_OK, jde o chybu ktera nastala
    DWORD WinError;                                // doplnuje nektere hodnoty ProblemID (pri ITEMPR_OK se ignoruje)
    CFTPQueueItemState State;                      // neni-li sqisNone, jde o pozadovany novy stav polozky
    char* NewTgtName;                              // neni-li NULL, jde o naalokovane nove jmeno (je nutna dealokace)
    HANDLE OpenedFile;                             // neni-li NULL, jde o nove otevreny handle souboru (je nutne ho zavrit)
    CQuadWord FileSize;                            // velikost souboru (u noveho nebo prepisovaneho souboru je nula)
    BOOL CanOverwrite;                             // TRUE pokud muze dojit k prepisu souboru (pouziva se pro rozliseni "resume" a "resume or overwrite")
    BOOL CanDeleteEmptyFile;                       // TRUE pokud muze dojit ke smazani prazdneho souboru (pouziva se pri cancelu/chybe polozky pro rozhodnuti zda smazat soubor nulove velikosti)
    TIndirectArray<CDiskListingItem>* DiskListing; // neni-li NULL (jen Type == fdwtListDir), jde o naalokovany listing

    void CopyFrom(CFTPDiskWork* work); // nakopiruje hodnoty z 'work' do 'this'
};

struct CFTPFileToClose
{
    char FileName[MAX_PATH]; // jmeno souboru
    HANDLE File;             // handle souboru, ktery mame zavrit
    BOOL DeleteIfEmpty;      // TRUE = pokud je zavirany soubor prazdny, smazeme ho
    BOOL SetDateAndTime;     // TRUE = pred zavrenim souboru nastavit 'Date'+'Time'
    CFTPDate Date;           // nastavovane datum posledniho zapisu do souboru; je-li Date.Day==0, jde o "prazdnou hodnotu"
    CFTPTime Time;           // nastavovany cas posledniho zapisu do souboru; je-li Time.Hour==24, jde o "prazdnou hodnotu"
    BOOL AlwaysDeleteFile;   // TRUE = smazat soubor po uzavreni
    CQuadWord EndOfFile;     // neni-li CQuadWord(-1, -1), jde o offset, na kterem se soubor orizne

    CFTPFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                    BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                    BOOL deleteFile, CQuadWord* setEndOfFile);
};

class CFTPDiskThread : public CThread
{
protected:
    HANDLE ContEvent; // "signaled" pokud je v poli Work nejaka prace nebo pokud se ma thread ukoncit

    // kriticka sekce pro pristup k datove casti objektu
    // POZOR: pristup do kritickych sekci konzultovat v souboru servers\critsect.txt !!!
    CRITICAL_SECTION DiskCritSect;

    TIndirectArray<CFTPDiskWork> Work;
    TIndirectArray<CFTPFileToClose> FilesToClose;
    BOOL ShouldTerminate;  // TRUE = thread se ma ukoncit
    BOOL WorkIsInProgress; // TRUE = probiha zpracovani polozky Work[0]

    int NextFileCloseIndex; // poradove cislo dalsiho zavirani souboru
    int DoneFileCloseIndex; // poradove cislo posledniho hotoveho zavreni souboru (-1 = zadny se zatim nezavrel)

    // kriticka sekce je bez synchronizace (pristup mimo DiskCritSect)
    HANDLE FileClosedEvent; // pulsne se po provedeni zavreni souboru (resi cekani na zavreni souboru)

public:
    CFTPDiskThread();
    ~CFTPDiskThread();

    BOOL IsGood() { return ContEvent != NULL; }

    void Terminate();

    // prida threadu praci, ukazatel 'work' musi byt platny az do prijeti zpravy o vysledku
    // nebo do volani CancelWork(); vraci uspech
    BOOL AddWork(CFTPDiskWork* work);

    // rusi praci 'work' pridanou do threadu; vraci TRUE pokud se prace jeste nezacala provadet
    // nebo pokud ji jeste lze prerusit (je rozpracovana a az dobehne, provede se k ni opacny
    // krok); je-li prace rozpracovana, vraci TRUE v 'workIsInProgress' (neni-li NULL); vraci
    // FALSE pokud jiz prace probehla (je hotova)
    BOOL CancelWork(const CFTPDiskWork* work, BOOL* workIsInProgress);

    // prida threadu soubor k zavreni, je-li 'deleteIfEmpty' TRUE a soubor ma nulovou
    // velikost, dojde navic k jeho smazani; 'path'+'name' je plne jmeno souboru;
    // je-li 'setDateAndTime' TRUE, dojde pred zavrenim souboru k nastaveni casu zapisu
    // na 'date'+'time' (POZOR: je-li date->Day==0 resp. time->Hour==24, jde o
    // "prazdne hodnoty" u datumu resp. casu); je-li 'deleteFile' TRUE, smazeme soubor
    // ihned po jeho uzavreni; neni-li 'setEndOfFile' NULL, dojde po zavreni souboru
    // k oriznuti na offsetu 'setEndOfFile'; neni-li 'fileCloseIndex' NULL, vraci se
    // v nem poradove cislo zavreni souboru (pozdeji se na provedeni tohoto zavreni
    // da cekat, viz WaitForFileClose)
    BOOL AddFileToClose(const char* path, const char* name, HANDLE file, BOOL deleteIfEmpty,
                        BOOL setDateAndTime, const CFTPDate* date, const CFTPTime* time,
                        BOOL deleteFile, CQuadWord* setEndOfFile, int* fileCloseIndex);

    // ceka na zavreni souboru s poradovym cislem 'fileCloseIndex' nebo na timeout
    // ('timeout' v milisekundach nebo hodnota INFINITE = zadny timeout); vraci TRUE
    // pokud se dockal zavreni, FALSE pri timeoutu
    BOOL WaitForFileClose(int fileCloseIndex, DWORD timeout);

    virtual unsigned Body();
};

//
// ****************************************************************************
// CFTPWorker
//
// "control connection" zpracovavajici vybranou polozku operace (vybranou
// z fronty polozek operace)

class CFTPOperation;
struct CUploadWaitingWorker;

enum CFTPWorkerState
{
    fwsLookingForWork,      // nema praci a mel by si ji najit
    fwsSleeping,            // nema praci, zadna nebyla k dispozici, po zmene fronty polozek operace provest prechod na fwsLookingForWork
    fwsPreparing,           // ma praci, overi proveditelnost polozky operace, overi connectionu a provede prechod na fwsWorking
    fwsConnecting,          // ma praci, nema connectionu, snazi se pripojit na server (timeout = prechod na fwsWaitingForReconnect); pokud se nelze pripojit, vrati praci a provede prechod na fwsConnectionError; POZOR: pri ConnectAttemptNumber > 0 je nutne mit nastaveny duvod pro novy Connect v ErrorDescr
    fwsWaitingForReconnect, // ma praci, nema connectionu, ceka na dalsi pokus o pripojeni na server (prechod na fwsConnecting); popis chyby je v ErrorDescr
    fwsConnectionError,     // ma nebo nema praci (snazi se prace zbavit), nema connectionu, ceka na zasah uzivatele (zadani jmena+hesla, pokyn pro dalsi sadu reconnectu, atd.); popis chyby je v ErrorDescr
    fwsWorking,             // ma praci, ma connectionu, provadi praci (zpracovava polozku operace)
    fwsStopped,             // zastaveny, ceka se uz jen na dealokaci (worker si uz nesmi nabirat dalsi praci)
};

enum CFTPWorkerSubState // podstavy pro jednotlive stavy z CFTPWorkerState
{
    fwssNone, // zakladni stav kazdeho stavu z CFTPWorkerState

    // podstavy stavu fwsLookingForWork:
    fwssLookFWQuitSent, // po poslani prikazu "QUIT"

    // podstavy stavu fwsSleeping:
    fwssSleepingQuitSent, // po poslani prikazu "QUIT"

    // podstavy stavu fwsPreparing:
    fwssPrepQuitSent,                 // po poslani prikazu "QUIT"
    fwssPrepWaitForDisk,              // cekame na dokonceni diskove operace (soucast overeni proveditelnosti polozky)
    fwssPrepWaitForDiskAfterQuitSent, // po poslani prikazu "QUIT" + cekame na dokonceni diskove operace (soucast overeni proveditelnosti polozky)

    // podstavy stavu fwsConnecting:
    // fwssNone,                // zjistime jestli je potreba ziskavat IP adresu
    fwssConWaitingForIP,        // cekame na prijeti IP adresy (preklad ze jmenne adresy)
    fwssConConnect,             // provedeme Connect()
    fwssConWaitForConRes,       // cekame na vysledek Connect()
    fwssConWaitForPrompt,       // cekame na login prompt od serveru
    fwssConSendAUTH,            // send AUTH TLS before login script
    fwssConWaitForAUTHCmdRes,   // wait for response to AUTH TLS
    fwssConSendPBSZ,            // send PBSZ before login script
    fwssConWaitForPBSZCmdRes,   // wait for response to PBSZ
    fwssConSendPROT,            // send PROT before login script
    fwssConWaitForPROTCmdRes,   // wait for response to PROT
    fwssConSendNextScriptCmd,   // posleme dalsi prikaz z login skriptu
    fwssConWaitForScriptCmdRes, // cekame na vysledek prikazu z login skriptu
    fwssConSendMODEZ,           // send MODE Z after login script to enable compression
    fwssConWaitForMODEZCmdRes,  // wait for response to MODE Z
    fwssConSendInitCmds,        // posleme dalsi inicializacni prikaz (user-defined, viz CFTPOperation::InitFTPCommands) - POZOR: pred prvnim volanim nastavit NextInitCmd=0
    fwssConWaitForInitCmdRes,   // cekame na vysledek provadeneho inicializacniho prikazu
    fwssConSendSyst,            // zjistime system serveru (posleme prikaz "SYST")
    fwssConWaitForSystRes,      // cekame na system serveru (vysledek prikazu "SYST")
    fwssConReconnect,           // vyhodnotime jestli provest reconnect nebo ohlasit chybu workera (POZOR: na vstupu musi byt nastaven ErrorDescr)

    // podstavy stavu fwsWorking:
    fwssWorkStopped, // prace zastavena (byla-li otevrena connectiona, byl jiz poslan prikaz "QUIT"), ceka se jen na uvolneni workera (stejne pro vsechny typy praci)
    // fwssNone,                 // otestujeme ShouldStop + prejdeme do podstavu fwssWorkStartWork (stejne pro vsechny typy praci)
    fwssWorkStartWork,                         // zacatek prace (stejne pro vsechny typy praci)
    fwssWorkExplWaitForCWDRes,                 // explore-dir: cekame na vysledek "CWD" (zmena cesty do zkoumaneho adresare)
    fwssWorkExplWaitForPWDRes,                 // explore-dir: cekame na vysledek "PWD" (zjisteni pracovni cesty zkoumaneho adresare)
    fwssWorkExplWaitForPASVRes,                // explore-dir: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
    fwssWorkExplOpenActDataCon,                // explore-dir: otevreme aktivni data-connectionu
    fwssWorkExplWaitForListen,                 // explore-dir: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
    fwssWorkExplSetTypeA,                      // explore-dir: nastavime prenosovy rezim na ASCII
    fwssWorkExplWaitForPORTRes,                // explore-dir: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
    fwssWorkExplWaitForTYPERes,                // explore-dir: cekame na vysledek "TYPE" (prepnuti do ASCII rezimu prenosu dat)
    fwssWorkExplSendListCmd,                   // explore-dir: posleme prikaz LIST
    fwssWorkExplActivateDataCon,               // explore-dir: aktivujeme data-connectionu (tesne po poslani prikazu LIST)
    fwssWorkExplWaitForLISTRes,                // explore-dir: cekame na vysledek "LIST" (cekame na konec prenosu dat listingu)
    fwssWorkExplWaitForDataConFinish,          // explore-dir: cekame na ukonceni "data connection" (odpoved serveru na "LIST" uz prisla)
    fwssWorkExplProcessLISTRes,                // explore-dir: zpracovani vysledku "LIST" (az po ukonceni "data connection" a zaroven prijeti odpovedi serveru na "LIST")
    fwssWorkResLnkWaitForCWDRes,               // resolve-link: cekame na vysledek "CWD" (zmena cesty do zkoumaneho linku - podari-li se, je to link na adresar)
    fwssWorkSimpleCmdWaitForCWDRes,            // jednoduche prikazy (vse krome explore a resolve polozek): cekame na vysledek "CWD" (zmena cesty do adresare zpracovavaneho souboru/linku/podadresare)
    fwssWorkSimpleCmdStartWork,                // jednoduche prikazy (vse krome explore a resolve polozek): zacatek prace (pracovni adresar uz je nastaveny)
    fwssWorkDelFileWaitForDELERes,             // mazani souboru/linku: cekame na vysledek "DELE" (smazani souboru/linku)
    fwssWorkDelDirWaitForRMDRes,               // mazani adresare/linku: cekame na vysledek "RMD" (smazani adresare/linku)
    fwssWorkChAttrWaitForCHMODRes,             // zmena atributu: cekame na vysledek "SITE CHMOD" (zmena modu souboru/adresare, asi jen pro Unix)
    fwssWorkChAttrWaitForCHMODQuotedRes,       // zmena atributu (jmeno v uvozovkach): cekame na vysledek "SITE CHMOD" (zmena modu souboru/adresare, asi jen pro Unix)
    fwssWorkCopyWaitForPASVRes,                // copy/move souboru: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
    fwssWorkCopyOpenActDataCon,                // copy/move souboru: otevreme aktivni data-connectionu
    fwssWorkCopyWaitForListen,                 // copy/move souboru: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
    fwssWorkCopySetType,                       // copy/move souboru: nastavime pozadovany prenosovy rezim (ASCII / binary)
    fwssWorkCopyWaitForPORTRes,                // copy/move souboru: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
    fwssWorkCopyWaitForTYPERes,                // copy/move souboru: cekame na vysledek "TYPE" (prepnuti do ASCII / binary rezimu prenosu dat)
    fwssWorkCopyResumeFile,                    // copy/move souboru: pripadne zajistime resume souboru (poslani prikazu REST)
    fwssWorkCopyWaitForResumeRes,              // copy/move souboru: cekame na vysledek "REST" prikazu (resume souboru)
    fwssWorkCopyResumeError,                   // copy/move souboru: chyba prikazu "REST" (not implemented, atp.) nebo jiz dopredu vime, ze REST selze
    fwssWorkCopySendRetrCmd,                   // copy/move souboru: posleme prikaz RETR (zahajeni cteni souboru, pripadne od offsetu zadaneho pres Resume)
    fwssWorkCopyActivateDataCon,               // copy/move souboru: aktivujeme data-connectionu (tesne po poslani prikazu RETR)
    fwssWorkCopyWaitForRETRRes,                // copy/move souboru: cekame na vysledek "RETR" (cekame na konec cteni souboru)
    fwssWorkCopyWaitForDataConFinish,          // copy/move souboru: cekame na ukonceni "data connection" (odpoved serveru na "RETR" uz prisla)
    fwssWorkCopyFinishFlushData,               // copy/move souboru: zajistime dokonceni flushnuti dat z data-connectiony (ta je jiz zavrena)
    fwssWorkCopyFinishFlushDataAfterQuitSent,  // copy/move souboru: po poslani "QUIT" cekame na zavreni control-connectiony + cekame na dokonceni flushnuti dat na disk
    fwssWorkCopyProcessRETRRes,                // copy/move souboru: zpracovani vysledku "RETR" (az po ukonceni "data connection", flushnuti dat na disk a zaroven prijeti odpovedi serveru na "RETR")
    fwssWorkCopyDelayedAutoRetry,              // copy/move souboru: cekame WORKER_DELAYEDAUTORETRYTIMEOUT milisekund na auto-retry (aby mohly prijit vsechny neocekavane odpovedi ze serveru)
    fwssWorkCopyTransferFinished,              // copy/move souboru: soubor je prenesen, v pripade Move smazeme zdrojovy soubor
    fwssWorkCopyMoveWaitForDELERes,            // copy/move souboru: cekame na vysledek "DELE" (Move: smazani zdrojoveho souboru/linku po dokonceni prenosu souboru)
    fwssWorkCopyDone,                          // copy/move souboru: hotovo, zavreme soubor a jdeme na dalsi polozku
    fwssWorkUploadWaitForListing,              // upload copy/move souboru: cekame az jiny worker dokonci listovani cilove cesty na serveru (pro zjisteni kolizi)
    fwssWorkUploadResolveLink,                 // upload copy/move souboru: zjistime co je link (soubor/adresar), jehoz jmeno koliduje se jmenem ciloveho adresare/souboru na serveru
    fwssWorkUploadResLnkWaitForCWDRes,         // upload copy/move souboru: cekame na vysledek "CWD" (zmena cesty do zkoumaneho linku - podari-li se, je to link na adresar)
    fwssWorkUploadCreateDir,                   // upload copy/move souboru: vytvorime cilovy adresar na serveru - zacneme nastavenim cilove cesty
    fwssWorkUploadCrDirWaitForCWDRes,          // upload copy/move souboru: cekame na vysledek "CWD" (nastaveni cilove cesty)
    fwssWorkUploadCrDirWaitForMKDRes,          // upload copy/move souboru: cekame na vysledek "MKD" (vytvoreni ciloveho adresare)
    fwssWorkUploadCantCreateDirInvName,        // upload copy/move souboru: resime chybu "target directory cannot be created" (invalid name)
    fwssWorkUploadCantCreateDirFileEx,         // upload copy/move souboru: resime chybu "target directory cannot be created" (name already used for file or link to file)
    fwssWorkUploadDirExists,                   // upload copy/move souboru: resime chybu "target directory already exists"
    fwssWorkUploadDirExistsDirLink,            // stejny stav jako fwssWorkUploadDirExists: navic jen to, ze bylo prave provedeno CWD do ciloveho adresare (test jestli je link adresar nebo soubor)
    fwssWorkUploadAutorenameDir,               // upload copy/move souboru: reseni chyby vytvareni ciloveho adresare - autorename - zacneme nastavenim cilove cesty
    fwssWorkUploadAutorenDirWaitForCWDRes,     // upload copy/move souboru: autorename - cekame na vysledek "CWD" (nastaveni cilove cesty)
    fwssWorkUploadAutorenDirSendMKD,           // upload copy/move souboru: autorename - zkusi vygenerovat dalsi nove jmeno pro cilovy adresar a zkusi tento adresar vytvorit
    fwssWorkUploadAutorenDirWaitForMKDRes,     // upload copy/move souboru: autorename - cekame na vysledek "MKD" (vytvoreni ciloveho adresare pod novym jmenem)
    fwssWorkUploadGetTgtPath,                  // upload copy/move souboru: zjistime cestu do ciloveho adresare na serveru - zacneme zmenou cesty do nej
    fwssWorkUploadGetTgtPathWaitForCWDRes,     // upload copy/move souboru: cekame na vysledek "CWD" (nastaveni cesty do ciloveho adresare)
    fwssWorkUploadGetTgtPathSendPWD,           // upload copy/move souboru: posleme "PWD" (zjisteni cesty do ciloveho adresare)
    fwssWorkUploadGetTgtPathWaitForPWDRes,     // upload copy/move souboru: cekame na vysledek "PWD" (zjisteni cesty do ciloveho adresare)
    fwssWorkUploadListDiskDir,                 // upload copy/move souboru: jdeme vylistovat uploadovany adresar na disku
    fwssWorkUploadListDiskWaitForDisk,         // upload copy/move souboru: cekame na dokonceni diskove operace (listovani adresare)
    fwssWorkUploadListDiskWaitForDiskAftQuit,  // upload copy/move souboru: po poslani prikazu "QUIT" + cekame na dokonceni diskove operace (listovani adresare)
    fwssWorkUploadCantCreateFileInvName,       // upload copy/move souboru: resime chybu "target file cannot be created" (invalid name)
    fwssWorkUploadCantCreateFileDirEx,         // upload copy/move souboru: resime chybu "target file cannot be created" (name already used for directory or link to directory)
    fwssWorkUploadFileExists,                  // upload copy/move souboru: resime chybu "target file already exists"
    fwssWorkUploadNewFile,                     // upload copy/move souboru: cilovy soubor neexistuje, jdeme ho uploadnout
    fwssWorkUploadAutorenameFile,              // upload copy/move souboru: reseni chyby vytvareni ciloveho souboru - autorename
    fwssWorkUploadResumeFile,                  // upload copy/move souboru: problem "cilovy soubor existuje" - resume
    fwssWorkUploadTestIfFinished,              // upload copy/move souboru: poslali jsme cely soubor + server "jen" neodpovedel, nejspis je soubor OK, otestujeme to
    fwssWorkUploadResumeOrOverwriteFile,       // upload copy/move souboru: problem "cilovy soubor existuje" - resume or overwrite
    fwssWorkUploadOverwriteFile,               // upload copy/move souboru: problem "cilovy soubor existuje" - overwrite
    fwssWorkUploadFileSetTgtPath,              // upload souboru: nastaveni cilove cesty
    fwssWorkUploadFileSetTgtPathWaitForCWDRes, // upload souboru: cekame na vysledek "CWD" (nastaveni cilove cesty)
    fwssWorkUploadGenNewName,                  // upload souboru: autorename: generovani noveho jmena
    fwssWorkUploadLockFile,                    // upload souboru: otevreni souboru v FTPOpenedFiles
    fwssWorkUploadDelForOverwrite,             // upload souboru: pokud jde o overwrite a ma se pouzit napred delete, zavolame ho zde
    fwssWorkUploadDelForOverWaitForDELERes,    // upload souboru: cekani na vysledek DELE pred overwritem
    fwssWorkUploadFileAllocDataCon,            // upload souboru: alokace data-connectiony
    fwssWorkUploadGetFileSize,                 // upload souboru: resume: zjisteni velikosti souboru (pres prikaz SIZE nebo z listingu)
    fwssWorkUploadWaitForSIZERes,              // upload souboru: resume: cekani na odpoved na prikaz SIZE
    fwssWorkUploadGetFileSizeFromListing,      // upload souboru: resume: prikaz SIZE selhal (nebo neni implementovan), zjistime velikost souboru z listingu
    fwssWorkUploadTestFileSizeOK,              // upload copy/move souboru: po chybe uploadu test velikosti souboru vysel OK
    fwssWorkUploadTestFileSizeFailed,          // upload copy/move souboru: po chybe uploadu test velikosti souboru nevysel OK
    fwssWorkUploadWaitForPASVRes,              // upload copy/move souboru: cekame na vysledek "PASV" (zjisteni IP+port pro pasivni data-connectionu)
    fwssWorkUploadOpenActDataCon,              // upload copy/move souboru: otevreme aktivni data-connectionu
    fwssWorkUploadWaitForListen,               // upload copy/move souboru: cekame na otevreni "listen" portu (otevirame aktivni data-connectionu) - lokalniho nebo na proxy serveru
    fwssWorkUploadWaitForPORTRes,              // upload copy/move souboru: cekame na vysledek "PORT" (predani IP+port serveru pro aktivni data-connectionu)
    fwssWorkUploadSetType,                     // upload copy/move souboru: nastavime pozadovany prenosovy rezim (ASCII / binary)
    fwssWorkUploadWaitForTYPERes,              // upload copy/move souboru: cekame na vysledek "TYPE" (prepnuti do ASCII / binary rezimu prenosu dat)
    fwssWorkUploadSendSTORCmd,                 // upload copy/move souboru: posleme prikaz STOR/APPE (zahajeni ukladani souboru na server)
    fwssWorkUploadActivateDataCon,             // upload copy/move souboru: aktivujeme data-connectionu (tesne po poslani prikazu STOR)
    fwssWorkUploadWaitForSTORRes,              // upload copy/move souboru: cekame na vysledek "STOR/APPE" (cekame na konec uploadu souboru)
    fwssWorkUploadCopyTransferFinished,        // upload copy/move souboru: cilovy soubor je jiz uploadly, jestli jde o Move, zkusime jeste smaznout zdrojovy soubor na disku
    fwssWorkUploadDelFileWaitForDisk,          // upload copy/move souboru: cekame na dokonceni diskove operace (smazani zdrojoveho souboru)
    fwssWorkUploadDelFileWaitForDiskAftQuit,   // upload copy/move souboru: po poslani prikazu "QUIT" + cekame na dokonceni diskove operace (smazani zdrojoveho souboru)
    fwssWorkUploadWaitForDELERes,              // upload copy/move souboru: pderASCIIForBinaryFile: cekame na vysledek prikazu DELE (smazani ciloveho souboru)
    fwssWorkUploadCopyDone,                    // upload copy/move souboru: hotovo, jdeme na dalsi polozku
};

enum CFTPWorkerSocketEvent
{
    fwseConnect,       // [error, 0], otevreni spojeni na server
    fwseClose,         // [error, 0], socket byl uzavren
    fwseNewBytesRead,  // [error, 0], precteni dalsiho bloku dat do bufferu socketu
    fwseWriteDone,     // [error, 0], dokonceni zapisu bufferu (jen pokud metoda Write vratila 'allBytesWritten'==FALSE)
    fwseIPReceived,    // [IP, error], dostali jsme IP (pri prekladu jmenne adresy na IP)
    fwseTimeout,       // [0, 0], prijem timeru hlasi timeout posilani FTP prikazu (viz WORKER_TIMEOUTTIMERID)
    fwseWaitForCmdErr, // [0, 0], posilani prikazu na server selhalo, cekame jestli prijde FD_CLOSE, kdyz neprijde, zavreme socket "rucne"
};

enum CFTPWorkerEvent
{
    fweActivate, // (re)aktivace workera (zasila se po vytvoreni workera pro zahajeni cinnosti + po zmene stavu workera, aby se pred zpracovanim noveho stavu k "lizu" dostali i ostatni sockety (diky message-loope sockets-threadu))

    fweWorkerShouldStop,   // upozorneni workera na ukonceni (zasila se pokud ma worker otevrene spojeni a je v "idle" (ceka na zmenu fronty, reakci usera, reconnect, atp.) stavu - obe tyto podminky muze splnit: stav fwsSleeping nebo stav fwsWorking:fwssWorkUploadWaitForListing nebo stav fwsWorking:fwssWorkExplWaitForListen nebo stav fwsWorking:fwssWorkCopyWaitForListen nebo stav fwsWorking:fwssWorkUploadWaitForListen nebo stav fwsLookingForWork pri ShouldBePaused==TRUE)
    fweWorkerShouldPause,  // upozorneni workera na pause (zasila se resumnutemu (bezicimu) workerovi)
    fweWorkerShouldResume, // upozorneni workera na resume (zasila se pausnutemu workerovi)

    fweCmdReplyReceived, // prijem kompletni odpovedi (krome typu FTP_D1_MAYBESUCCESS) na poslany FTP prikaz (prijde az po zaslani vsech bytu FTP prikazu)
    fweCmdInfoReceived,  // prijem kompletni odpovedi typu FTP_D1_MAYBESUCCESS na poslany FTP prikaz (prijde az po zaslani vsech bytu FTP prikazu)
    fweCmdConClosed,     // spojeni bylo uzavreno behem provadeni prikazu (i pri/pred samotnym posilani prikazu + i z duvodu timeoutu); popis chyby je v ErrorDescr; jestli slo o timeout je v CommandReplyTimeout

    fweIPReceived,   // nastavili jsme zjistene IP do objektu operace (pri prekladu jmenne adresy na IP)
    fweIPRecFailure, // chyba pri zjistovani IP (pri prekladu jmenne adresy na IP); popis chyby je v ErrorDescr

    fweConTimeout, // timeout pro akce behem connectu na server (viz WORKER_CONTIMEOUTTIMID)

    fweConnected,      // pozadovane spojeni bylo navazano (prijem FD_CONNECT)
    fweConnectFailure, // chyba pri navazovani spojeni (prijem FD_CONNECT s chybou); popis chyby je v ErrorDescr

    fweReconTimeout, // timeout pro dalsi pokus o connect (viz WORKER_RECONTIMEOUTTIMID)

    fweNewLoginParams, // upozorneni workera na nove login parametry (password/account) (viz WORKER_NEWLOGINPARAMS)

    fweWakeUp, // "sleeping" worker by se mel probudit a jit hledat praci do fronty polozek (viz WORKER_WAKEUP)

    fweDiskWorkFinished,        // prace na disku se dokoncila (viz WORKER_DISKWORKFINISHED)
    fweDiskWorkWriteFinished,   // prace na disku - zapis - se dokoncila (viz WORKER_DISKWORKWRITEFINISHED)
    fweDiskWorkListFinished,    // prace na disku - listovani adresare - se dokoncila (viz WORKER_DISKWORKLISTFINISHED)
    fweDiskWorkReadFinished,    // prace na disku - cteni - se dokoncila (viz WORKER_DISKWORKREADFINISHED)
    fweDiskWorkDelFileFinished, // prace na disku - mazani souboru - se dokoncila (viz WORKER_DISKWORKDELFILEFINISHED)

    fweDataConConnectedToServer, // data-connection hlasi, ze doslo ke spojeni se serverem (viz WORKER_DATACON_CONNECTED)
    fweDataConConnectionClosed,  // data-connection hlasi, ze doslo k zavreni/preruseni spojeni se serverem (viz WORKER_DATACON_CLOSED)
    fweDataConFlushData,         // data-connection hlasi, ze jsou pripravena data ve flush-bufferu pro overeni/zapis na disk (viz WORKER_DATACON_FLUSHDATA)
    fweDataConListeningForCon,   // data-connection hlasi, ze doslo k otevreni "listen" portu (viz WORKER_DATACON_LISTENINGFORCON)

    fweDataConStartTimeout, // timeout pro cekani na otevreni data-connectiony po prijeti odpovedi serveru na RETR (viz WORKER_DATACONSTARTTIMID)

    fweDelayedAutoRetry, // timer pro zpozdene auto-retry (viz WORKER_DELAYEDAUTORETRYTIMID)

    fweTgtPathListingFinished, // UploadListingCache ziskala listing cilove cesty (nebo se dozvedela o chybe pri jeho stahovani) (viz WORKER_TGTPATHLISTINGFINISHED)

    fweUplDataConConnectedToServer, // upload data-connection hlasi, ze doslo ke spojeni se serverem (viz WORKER_UPLDATACON_CONNECTED)
    fweUplDataConConnectionClosed,  // upload data-connection hlasi, ze doslo k zavreni/preruseni spojeni se serverem (viz WORKER_UPLDATACON_CLOSED)
    fweUplDataConPrepareData,       // upload data-connection hlasi, ze je potreba pripravit dalsi data do flush-bufferu pro poslani na server (viz WORKER_UPLDATACON_PREPAREDATA)
    fweUplDataConListeningForCon,   // upload data-connection hlasi, ze doslo k otevreni "listen" portu (viz WORKER_UPLDATACON_LISTENINGFORCON)

    fweDataConListenTimeout, // timeout pro cekani na otevreni "listen" portu na proxy serveru (pri otevirani aktivni data-connectiony) (viz WORKER_LISTENTIMEOUTTIMID)
};

enum CFTPWorkerCmdState
{
    fwcsIdle, // zadny prikaz neprobiha (v tomto stavu provadime jen prijem neocekavanych zprav od serveru)

    // probiha posilani FTP prikazu, HandleSocketEvent() pripravuje pro HandleEvent()
    // fweCmdReplyReceived (prijem odpovedi na FTP prikaz, krome odpovedi typu FTP_D1_MAYBESUCCESS),
    // fweCmdInfoReceived (prijem odpovedi typu FTP_D1_MAYBESUCCESS) a fweCmdConClosed (error posilani
    // prikazu nebo vypadek spojeni behem posilani/cekani na prijem odpovedi nebo timeout - zavreni
    // spojeni z duvodu, ze jsme se nedockali odpovedi serveru na FTP prikaz)
    fwcsWaitForCmdReply,
    fwcsWaitForLoginPrompt, // stejna funkcnost jako fwcsWaitForCmdReply, rozlisujeme jen kvuli error-hlaskam

    // cekani na duvod chyby vznikle pri posilani prikazu (zapisu na socket): cekame na fwseClose,
    // kdyz neprijde zavreme socket "rucne" (na timeout fwseWaitForCmdErr); zaroven chytame error
    // hlasku ze socketu nebo aspon z fwseClose; kdyz nic nechytneme, vypiseme aspon WaitForCmdErrError
    fwcsWaitForCmdError,
};

enum CWorkerDataConnectionState
{
    wdcsDoesNotExist,         // neexistuje (WorkerDataCon == NULL && WorkerUploadDataCon == NULL)
    wdcsOnlyAllocated,        // jen alokovana (zatim neni pridana v SocketsThread)
    wdcsWaitingForConnection, // je v SocketsThread, ceka na spojeni se serverem (aktivni/pasivni)
    wdcsTransferingData,      // spojeni se serverem navazano, je mozne prenaset data
    wdcsTransferFinished,     // prenos dokoncen (uspesne/neuspesne), spojeni se serverem je uzavrene nebo se vubec nepodarilo ho navazat
};

enum CWorkerStatusType // typ status informaci ulozenych ve workeru (zobrazovanych v dialogu operace v listview Connections)
{
    wstNone,           // zadny status se nezobrazuje
    wstDownloadStatus, // zobrazuje se download status (ziskavani listingu + copy & move z FTP na disk)
    wstUploadStatus,   // zobrazuje se upload status (ziskavani listingu cilovych cest + copy & move z disku na FTP)
};

enum CFlushDataError // druhy chyb pri flushovani dat (Copy a Move operace)
{
    fderNone,               // zadna
    fderASCIIForBinaryFile, // ASCII transfer mode pouzity pro binarni soubor
    fderLowMemory,          // nedostatek pameti pro pridani prace do FTPDiskThread
    fderWriteError,         // chyba zjistena az pri zapisu na disk
};

enum CPrepareDataError // druhy chyb pri priprave dat pro poslani na server (upload: Copy a Move operace)
{
    pderNone,               // zadna
    pderASCIIForBinaryFile, // ASCII transfer mode pouzity pro binarni soubor
    pderLowMemory,          // nedostatek pameti pro pridani prace do FTPDiskThread
    pderReadError,          // chyba zjistena uz pri cteni z disku
};

enum CUploadType
{
    utNone,                  // zadny upload souboru neprobiha
    utNewFile,               // upload do noveho souboru (pred uploadem neexistoval)
    utAutorename,            // upload do noveho souboru pojmenovaneho automaticky tak, aby se nic neprepsalo
    utResumeFile,            // resume stavajiciho souboru na serveru (append zatim neuploadnute casti souboru)
    utResumeOrOverwriteFile, // viz utResumeFile + pokud nelze soubor resumnout, prepise se
    utOverwriteFile,         // prepis stavajiciho souboru na serveru
    utOnlyTestFileSize,      // testuje jestli je soubor komplet uploadnuty (na zaklade shody velikosti souboru)
};

#define FTPWORKER_ERRDESCR_BUFSIZE 200             // velikost bufferu CFTPWorker::ErrorDescr
#define FTPWORKER_BYTESTOWRITEONSOCKETPREALLOC 512 // kolik bytu se ma predalokovat pro zapis (aby dalsi zapis zbytecne nealokoval treba kvuli 1 bytovemu presahu)
#define FTPWORKER_BYTESTOREADONSOCKET 1024         // po minimalne kolika bytech se ma cist socket (take alokovat buffer pro prectena data)
#define FTPWORKER_BYTESTOREADONSOCKETPREALLOC 512  // kolik bytu se ma predalokovat pro cteni (aby dalsi cteni okamzite nealokovalo znovu)

#define WORKER_ACTIVATE 10                   // ID zpravy postnute workerovi v okamziku, kdy se ma aktivovat
#define WORKER_SHOULDSTOP 11                 // ID zpravy postnute workerovi v "idle" stavu (upozorneni ze ma koncit)
#define WORKER_NEWLOGINPARAMS 12             // ID zpravy postnute workerovi v okamziku, kdy jsou k dispozici nove login parametry (password/account)
#define WORKER_WAKEUP 13                     // ID zpravy postnute workerovi v "idle" stavu, kdyz jsou ve fronte k dispozici nove polozky
#define WORKER_DISKWORKFINISHED 14           // ID zpravy postnute workerovi v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku
#define WORKER_DATACON_CONNECTED 15          // ID zpravy postnute workerovi v okamziku, kdy v data-connection dojde ke spojeni se serverem
#define WORKER_DATACON_CLOSED 16             // ID zpravy postnute workerovi v okamziku, kdy v data-connection dojde k zavreni/preruseni spojeni se serverem
#define WORKER_DATACON_FLUSHDATA 17          // ID zpravy postnute workerovi v okamziku, kdy jsou v data-connection pripravena data ve flush-bufferu pro overeni/zapis na disk
#define WORKER_DISKWORKWRITEFINISHED 18      // ID zpravy postnute workerovi v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku - zapis
#define WORKER_TGTPATHLISTINGFINISHED 19     // ID zpravy postnute workerovi v okamziku, kdy UploadListingCache ziska listing cilove cesty (nebo se dozvi o chybe pri jeho stahovani)
#define WORKER_DISKWORKLISTFINISHED 20       // ID zpravy postnute workerovi v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku - listovani adresare
#define WORKER_UPLDATACON_CONNECTED 21       // ID zpravy postnute workerovi v okamziku, kdy v upload-data-connection dojde ke spojeni se serverem
#define WORKER_UPLDATACON_CLOSED 22          // ID zpravy postnute workerovi v okamziku, kdy v upload-data-connection dojde k zavreni/preruseni spojeni se serverem
#define WORKER_UPLDATACON_PREPAREDATA 23     // ID zpravy postnute workerovi v okamziku, kdy je potreba pripravit dalsi data do flush-bufferu pro poslani na server (do upload data-connectiony)
#define WORKER_DISKWORKREADFINISHED 24       // ID zpravy postnute workerovi v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku - cteni (pro upload)
#define WORKER_DISKWORKDELFILEFINISHED 25    // ID zpravy postnute workerovi v okamziku, kdy FTPDiskThread dokonci zadanou praci na disku - mazani souboru
#define WORKER_SHOULDPAUSE 26                // ID zpravy postnute resumnutemu workerovi (upozorneni ze se ma pausnout)
#define WORKER_SHOULDRESUME 27               // ID zpravy postnute pausnutemu workerovi (upozorneni ze se ma resumnout)
#define WORKER_DATACON_LISTENINGFORCON 28    // ID zpravy postnute workerovi v okamziku, kdy v data-connection dojde k otevreni "listen" portu
#define WORKER_UPLDATACON_LISTENINGFORCON 29 // ID zpravy postnute workerovi v okamziku, kdy v upload-data-connection dojde k otevreni "listen" portu

#define WORKER_TIMEOUTTIMERID 30        // ID timeru zajistujiciho detekci timeoutu pro posilani FTP prikazu na server
#define WORKER_CMDERRORTIMERID 31       // ID timeru umoznujiciho cekani na typ chyby, ktera nastala pri posilani prikazu na server (viz fwseWaitForCmdErr)
#define WORKER_CONTIMEOUTTIMID 32       // ID timeru zajistujiciho detekci timeoutu pro zjistovani IP adresy, connect a login prompt
#define WORKER_RECONTIMEOUTTIMID 33     // ID timeru pro cekani na dalsi pokus o connect (reconnect)
#define WORKER_STATUSUPDATETIMID 34     // ID timeru pro periodicky update statusu
#define WORKER_DATACONSTARTTIMID 35     // ID timeru pro detekci timeoutu pri cekani na otevreni data-connectiony po prijeti odpovedi serveru na RETR (WarFTPD absolutne nelogicky posle 226 jeste pred nasim accept socketu data-connectiony)
#define WORKER_DELAYEDAUTORETRYTIMID 36 // ID timeru pro zpozdeni auto-retry (napr. Quick&Easy FTPD vraci na RETR 426 a vzapeti jeste 220 - kdyz se retryne hned, dojde k posunu odpovedi (220 se vezme k nasledujicimu prikazu misto odpovedi na tento prikaz a cele to jde do zahuby))
#define WORKER_LISTENTIMEOUTTIMID 37    // ID timeru zajistujiciho detekci timeoutu pro otevirani "listen" portu na proxy serveru (pri otevirani aktivni data-connectiony)

#define WORKER_STATUSUPDATETIMEOUT 1000     // doba v milisekundach, za kterou periodicky dochazi k updatu statusu ve workerovi (a progresu workera a celkoveho progresu v dialogu operace) - POZOR: vazba na OPERDLG_STATUSMINIDLETIME
#define WORKER_DELAYEDAUTORETRYTIMEOUT 5000 // doba v milisekundach, za kterou se provede auto-retry (viz WORKER_DELAYEDAUTORETRYTIMID)

class CUploadDataConnectionSocket;

class CFTPWorker : public CSocket
{
protected:
    // kriticka sekce pro pristup k socketove casti objektu je CSocket::SocketCritSect
    // POZOR: v teto sekci nesmi dojit ke vnoreni do SocketsThread->CritSect (nesmi se volat metody SocketsThread)

    int ControlConnectionUID; // pokud ma worker connectionu z panelu, je zde UID objektu socketu z panelu (jinak je zde -1)

    BOOL HaveWorkingPath;                     // TRUE pokud je WorkingPath platne
    char WorkingPath[FTP_MAX_PATH];           // aktualni pracovni cesta na FTP serveru (muze jit i jen o posledni retezec poslany pres CWD se "success" navratovou hodnotou - z rychlostnich duvodu nedelame po kazdem CWD jeste PWD)
    CCurrentTransferMode CurrentTransferMode; // aktualni prenosovy rezim na FTP serveru (jen pamet pro posledni FTP prikaz "TYPE")

    BOOL EventConnectSent; // TRUE jen pokud jiz byla generovana udalost fwseConnect (resi prichod FD_READ pred FD_CONNECT)

    char* BytesToWrite;            // buffer pro byty, ktere se nezapsaly ve Write (zapisou se po prijeti FD_WRITE)
    int BytesToWriteCount;         // pocet platnych bytu v bufferu 'BytesToWrite'
    int BytesToWriteOffset;        // pocet jiz odeslanych bytu v bufferu 'BytesToWrite'
    int BytesToWriteAllocatedSize; // alokovana velikost bufferu 'BytesToWrite'

    char* ReadBytes;            // buffer pro prectene byty ze socketu (ctou se po prijeti FD_READ)
    int ReadBytesCount;         // pocet platnych bytu v bufferu 'ReadBytes'
    int ReadBytesOffset;        // pocet jiz zpracovanych (preskocenych) bytu v bufferu 'ReadBytes'
    int ReadBytesAllocatedSize; // alokovana velikost bufferu 'ReadBytes'

    CDataConnectionSocket* WorkerDataCon;             // NULL, jinak data-connectiona prave vyuzivana timto workerem (stav viz WorkerDataConState)
    CUploadDataConnectionSocket* WorkerUploadDataCon; // NULL, jinak data-connectiona prave vyuzivana timto workerem pro upload (stav viz WorkerDataConState)

    // kriticka sekce pro pristup k datove casti objektu (data k zobrazeni v dialozich)
    // POZOR: pristup do kritickych sekci konzultovat v souboru servers\critsect.txt !!!
    CRITICAL_SECTION WorkerCritSect;

    int CopyOfUID; // kopie CSocket::UID dostupna v kriticke sekci WorkerCritSect
    int CopyOfMsg; // kopie CSocket::Msg dostupna v kriticke sekci WorkerCritSect

    int ID;                                      // ID workeru (ukazuje se ve sloupci ID v listview Connections v dialogu operace)
    int LogUID;                                  // UID logu pro tohoto workera (-1 pokud neni log zalozen); POZOR: je v sekci WorkerCritSect a ne SocketCritSect !!!
    CFTPWorkerState State;                       // stav workera, viz CFTPWorkerState
    CFTPWorkerSubState SubState;                 // stav uvnitr stavu workera (podstav pro kroky zpracovani kazdeho stavu State), viz CFTPWorkerSubState
    CFTPQueueItem* CurItem;                      // jen pro cteni dat: zpracovavana polozka (ve stavu sqisProcessing), NULL=worker nema praci; zapis provadet pres Queue a CurItem->UID
    char ErrorDescr[FTPWORKER_ERRDESCR_BUFSIZE]; // textovy popis chyby, neobsahuje CR ani LF a na konci nema tecku, zajisteni techto podminek viz CorrectErrorDescr(); zobrazuje se u fwsWaitingForReconnect a fwsConnectionError, plni se pri chybach viz CFTPWorkerEvent
    int ConnectAttemptNumber;                    // cislo soucasneho pokusu o navazani spojeni, pred zcela prvnim pokusem je zde nula (v okamziku navazani spojeni se nastavuje na jednicku)
    CCertificate* UnverifiedCertificate;         // SSL: pokud selze pokus o navazani spojeni diky neznamemu neduveryhodnemu certifikatu, spojeni se uzavre a certifikat je zde (zobrazime ho uzivateli v Solve Error dialogu)

    DWORD ErrorOccurenceTime; // "cas" vzniku chyby (pouziva se pro dodrzeni poradi reseni chyb podle jejich vzniku); -1 = zadna chyba nevznikla

    BOOL ShouldStop;     // TRUE jakmile se ceka na ukonceni workera (po volani InformAboutStop())
    BOOL SocketClosed;   // TRUE/FALSE: socket "control connection" workera je zavreny/otevreny
    BOOL ShouldBePaused; // TRUE pokud si uzivatel preje pozastavit praci tohoto workera (po volani InformAboutPause(TRUE))

    CFTPWorkerCmdState CommandState; // stav workeru souvisejici s posilanim prikazu (viz CFTPWorkerCmdState)
    BOOL CommandTransfersData;       // TRUE = poslany prikaz slouzi k prenosu dat (je upraveny timeout podle aktivity na data-connection - WorkerDataCon+WorkerUploadDataCon)
    BOOL CommandReplyTimeout;        // platne jen pro prave poslanou udalost fweCmdConClosed: TRUE = timeout cekani na odpoved (nasilne zavreni spojeni), FALSE = spojeni nezavrel klient (zavrel server nebo chyba spojeni)

    DWORD WaitForCmdErrError; // chyba posledniho Write(), dalsi popis viz fwcsWaitForCmdError

    BOOL CanDeleteSocket;    // FALSE = worker je jeste ve WorkersList operace, nelze ho zrusit po predani socketu do "control connection" panelu
    BOOL ReturnToControlCon; // TRUE = worker vraci socket do "control connection" panelu, nelze ho nechat zrusit v DeleteWorkers

    BOOL ReceivingWakeup; // TRUE = tomuto workerovi byla postnuta message WORKER_WAKEUP (po doruceni prejde na FALSE)

    const char* ProxyScriptExecPoint; // aktualni prikaz proxy skriptu (NULL = prvni prikaz); POZOR: neni urceno ke cteni, ale jen pro predavani do CFTPOperation::PrepareNextScriptCmd()
    int ProxyScriptLastCmdReply;      // odpoved na posledni poslany prikaz z proxy skriptu (-1 = neexistuje)

    BOOL DiskWorkIsUsed; // TRUE pokud je DiskWork vlozeny v FTPDiskThread

    HANDLE OpenedFile;                   // cilovy soubor pro copy/move operace
    CQuadWord OpenedFileSize;            // aktualni velikost souboru 'OpenedFile'
    CQuadWord OpenedFileOriginalSize;    // puvodni velikost souboru 'OpenedFile'
    BOOL CanDeleteEmptyFile;             // TRUE pokud muze dojit ke smazani prazdneho souboru (pouziva se pri cancelu/chybe polozky pro rozhodnuti zda smazat soubor nulove velikosti)
    CQuadWord OpenedFileCurOffset;       // aktualni offset v souboru 'OpenedFile' (do jakeho mista se maji zapsat/otestovat data z data-connectiony)
    CQuadWord OpenedFileResumedAtOffset; // Resume offset v souboru 'OpenedFile' (offset poslany prikazem REST - odkud checkujeme/zapisujeme)
    BOOL ResumingOpenedFile;             // TRUE = provadime Resume downloadu (existujici cast jen kontrolujeme, pak teprve zapisujeme); FALSE = provadime Overwrite downloadu (jen zapisujeme)

    HANDLE OpenedInFile;                     // zdrojovy soubor pro copy/move operace (upload)
    CQuadWord OpenedInFileSize;              // velikost souboru 'OpenedInFile' zjistena pri otevreni souboru
    CQuadWord OpenedInFileCurOffset;         // aktualni offset v souboru 'OpenedInFile' (z jakeho mista se maji precist data ze souboru pro zapis na data-connectionu)
    CQuadWord OpenedInFileSizeWithCRLF_EOLs; // velikost dosud prectene casti souboru z disku s CRLF konci radku (upload textoveho souboru)
    CQuadWord OpenedInFileNumberOfEOLs;      // pocet koncu radku v dosud prectene casti souboru z disku (upload textoveho souboru)
    CQuadWord FileOnServerResumedAtOffset;   // Resume offset odkud se cte zdrojovy soubor pro prikaz APPE
    BOOL ResumingFileOnServer;               // TRUE = provadime Resume uploadu; FALSE = provadime Overwrite uploadu

    int LockedFileUID; // UID souboru zamceneho (v FTPOpenedFiles) pro praci na polozce v tomto workerovi (0 = zadny soubor neni zamceny)

    CWorkerDataConnectionState WorkerDataConState; // stav data-connectiony (viz WorkerDataCon+WorkerUploadDataCon)
    BOOL DataConAllDataTransferred;                // TRUE pokud jiz doslo k zavreni data-connectiony a ze serveru prisla nebo na server byla odeslana vsechna data

    SYSTEMTIME StartTimeOfListing; // cas, kdy jsme zacali porizovat listing (tesne pred alokaci data-connectiony)
    DWORD StartLstTimeOfListing;   // IncListingCounter() z okamziku, kdy jsme zacali porizovat listing (tesne pred alokaci data-connectiony)
    int ListCmdReplyCode;          // vysledek prikazu "LIST"/"RETR" ulozeny pro pozdejsi zpracovani (viz fwssWorkExplProcessLISTRes/fwssWorkCopyProcessRETRRes)
    char* ListCmdReplyText;        // vysledek prikazu "LIST"/"RETR" ulozeny pro pozdejsi zpracovani (viz fwssWorkExplProcessLISTRes/fwssWorkCopyProcessRETRRes)

    CWorkerStatusType StatusType;   // typ status informaci ulozenych ve workeru (zobrazovanych v dialogu operace v listview Connections)
    DWORD StatusConnectionIdleTime; // pri StatusType == wstDownloadStatus/wstUploadStatus: cas v sekundach od posledniho prijmu dat
    DWORD StatusSpeed;              // pri StatusType == wstDownloadStatus/wstUploadStatus: rychlost spojeni v bytech za sekundu
    CQuadWord StatusTransferred;    // pri StatusType == wstDownloadStatus/wstUploadStatus: kolik se jiz downloadlo/uploadlo bytu
    CQuadWord StatusTotal;          // pri StatusType == wstDownloadStatus/wstUploadStatus: celkova velikost downloadu/uploadu v bytech - neni-li znama, je zde CQuadWord(-1, -1)

    DWORD LastTimeEstimation; // -1==neplatny, jinak zaokrouhleny pocet sekund do konce prace s aktualni polozkou

    CFlushDataError FlushDataError;     // kod chyby, ktera nastala pri flushovani dat (Copy a Move operace)
    CPrepareDataError PrepareDataError; // kod chyby, ktera nastala pri priprave dat (upload: Copy a Move operace)

    BOOL UploadDirGetTgtPathListing; // jen pri zpracovani upload-dir-explore nebo upload-file polozek: TRUE = ma se tahat listing cilove cesty

    int UploadAutorenamePhase;              // upload: aktualni faze generovani jmen pro cilovy adresar/soubor (viz FTPGenerateNewName()); 0 = pocatek autorename procesu; -1 = slo o posledni fazi generovani, typove jine jmeno proste neumime vygenerovat
    char UploadAutorenameNewName[MAX_PATH]; // upload: buffer pro posledni vygenerovane jmeno pro cilovy adresar/soubor

    CUploadType UploadType; // typ uploadu (podle stavu ciloveho souboru): new, resume, resume or overwrite, overwrite, autorename

    BOOL UseDeleteForOverwrite; // upload: TRUE = pred STOR volat DELE (resi na unixu prepis souboru jineho usera - zapsat do souboru nejde, ale smazat jde a pak jde vytvorit)

    // data bez potreby kriticke sekce (jen pro cteni + platna celou zivotnost objektu):
    CFTPOperation* Oper; // operace, ke ktere worker patri (worker ma kratsi zivotnost nez operace)
    CFTPQueue* Queue;    // fronta operace, ke ktere worker patri (worker ma kratsi zivotnost nez fronta operace)

    // data bez potreby kriticke sekce (pouziva se jen v sockets-threadu):
    int IPRequestUID; // pocitadlo pro odliseni jednotlivych dotazu na zjisteni IP adresy (muze se volat se opakovane, vysledky by se motaly) (pouziva se jen v sockets threadu, neni nutna synchronizace)
    int NextInitCmd;  // poradove cislo dalsiho posilaneho prikazu v retezci CFTPOperation::InitFTPCommands - 0 = prvni prikaz

    // data bez potreby kriticke sekce (zapisuje se jen v sockets-threadu a disk-threadu, synchronizace pres FTPDiskThread a DiskWorkIsUsed):
    CFTPDiskWork DiskWork; // data prace, ktera se zadava objektu FTPDiskThread (thread provadejici operace na disku)

public:
    CFTPWorker(CFTPOperation* oper, CFTPQueue* queue, const char* host, unsigned short port,
               const char* user);
    ~CFTPWorker();

    // vraci ID (v kriticke sekci WorkerCritSect)
    int GetID();

    // nastavi ID (v kriticke sekci WorkerCritSect)
    void SetID(int id);

    // vraci State (v kriticke sekci WorkerCritSect)
    CFTPWorkerState GetState();

    // vraci ShouldBePaused (v kriticke sekci WorkerCritSect); v 'isWorking' (nesmi byt
    // NULL) vraci TRUE pokud worker pracuje (nespi, neceka na usera a neni ukonceny);
    BOOL IsPaused(BOOL* isWorking);

    // nastavi 'CopyOfUID' a 'CopyOfMsg' (v kriticke sekci CSocket::SocketCritSect a
    // WorkerCritSect); vraci vzdy TRUE
    BOOL RefreshCopiesOfUIDAndMsg();

    // vraci kopii CSocket::UID (v kriticke sekci WorkerCritSect)
    int GetCopyOfUID();

    // vraci kopii CSocket::Msg (v kriticke sekci WorkerCritSect)
    int GetCopyOfMsg();

    // vraci LogUID (v kriticke sekci WorkerCritSect)
    int GetLogUID();

    // vraci ShouldStop (v kriticke sekci WorkerCritSect)
    BOOL GetShouldStop();

    // vraci data pro listview Connections v dialogu operace
    void GetListViewData(LVITEM* itemData, char* buf, int bufSize);

    // vraci TRUE pokud je mozne, ze worker potrebuje od usera vyresit chybu (stav
    // fwsConnectionError (nutne reseni chyby) a fwsWaitingForReconnect (mozna je
    // potreba zmenit login parametry - napr. pri zapnuti "Always try to reconnect"
    // a zadani spatneho hesla))
    BOOL HaveError();

    // vraci TRUE pokud worker potrebuje od usera vyresit chybu (viz HaveError()),
    // pokud je worker ve stavu fwsWaitingForReconnect, zmeni se do stavu fwsConnectionError;
    // pokud vraci TRUE, vraci i text chyby v 'buf' (buffer o velikosti 'bufSize') a pokud
    // je chyba zpusobena neduveryhodnym serverovym certifikatem, vraci ho
    // v 'unverifiedCertificate' (neni-li NULL; volajici ma na starosti pripadne uvolneni
    // certifikatu jeho metodou Release()); pokud je potreba po volani metody postnout
    // fweActivate, vraci v 'postActivate' TRUE;
    BOOL GetErrorDescr(char* buf, int bufSize, BOOL* postActivate,
                       CCertificate** unverifiedCertificate);

    // pouziva se pro dotaz na moznost zruseni workera z metod CReturningConnections (lze jen
    // pokud uz jsme se o vymaz pokusili z DeleteWorkers); vraci TRUE je-li zruseni mozne
    BOOL CanDeleteFromRetCons();

    // pouziva se pro dotaz na moznost zruseni workera z DeleteWorkers (lze jen pokud se
    // connectiona z workera nevraci do panelu nebo pokud uz jsme se o vymaz pokusili
    // z metod CReturningConnections); vraci TRUE je-li zruseni mozne
    BOOL CanDeleteFromDelWorkers();

    // informuje workera o tom, ze se ma snazit stopnout; vraci TRUE pokud si worker
    // preje zavrit datovou connectionu nebo postnout WORKER_SHOULDSTOP (fweWorkerShouldStop)
    // volanim CloseDataConnectionOrPostShouldStop() po dokonceni teto metody
    // POZOR: muze se volat pro jednoho workera nekolikrat za sebou (provest kontrolu
    //        jestli uz bylo volano, pripadne neprovadet zadnou akci a vracet FALSE)!
    BOOL InformAboutStop();

    // zavira datovou connectionu (vola se mimo vsechny kriticke sekce, aby se dal bez
    // obtizi zavolat CloseSocket() + DeleteSocket()) nebo postne WORKER_SHOULDSTOP
    // (fweWorkerShouldStop)
    void CloseDataConnectionOrPostShouldStop();

    // informuje workera o tom, ze se ma snazit o pause/resume; vraci TRUE pokud si worker
    // preje postnout WORKER_SHOULDPAUSE nebo WORKER_SHOULDRESUME (fweWorkerShouldPause
    // nebo fweWorkerShouldResume) volanim PostShouldPauseOrResume() po dokonceni teto
    // metody; 'pause' je TRUE/FALSE pokud jde o pause/resume
    // POZOR: muze se volat pro jednoho workera nekolikrat za sebou (provest kontrolu
    //        jestli uz bylo volano, pripadne neprovadet zadnou akci a vracet FALSE)!
    BOOL InformAboutPause(BOOL pause);

    // postne WORKER_SHOULDPAUSE nebo WORKER_SHOULDRESUME (fweWorkerShouldPause
    // nebo fweWorkerShouldResume) podle stavu ShouldBePaused; vola se mimo vsechny
    // kriticke sekce
    void PostShouldPauseOrResume();

    // zjisti jestli je socket "control connection" workera zavreny a neexistuje
    // "data-connection" workera; vraci TRUE pokud je socket "control connection"
    // zavreny a "data-connection" neexistuje (workera je mozne zrusit)
    BOOL SocketClosedAndDataConDoesntExist();

    // zjisti jestli ma worker praci v disk-threadu; vraci FALSE pokud
    // nema praci (workera je mozne zrusit)
    BOOL HaveWorkInDiskThread();

    // pripravi workera na zruseni (zavira tvrde "control connection" i "data connection");
    // vola se mimo vsechny kriticke sekce nebo v jedine kriticke sekci CSocketsThread::CritSect,
    // aby se dal bez obtizi zavolat CloseSocket()
    void ForceClose();

    // priprava workera na zruseni (canceluje praci v disk-threadu)
    void ForceCloseDiskWork();

    // uvolni data workeru (vrati zpracovavanou polozku operace zpet do fronty);
    // v 'uploadFirstWaitingWorker' je aktualizovany seznam workeru cekajicich na
    // WORKER_TGTPATHLISTINGFINISHED
    void ReleaseData(CUploadWaitingWorker** uploadFirstWaitingWorker);

    // aktivuje workera - postne socketu WORKER_ACTIVATE
    // POZOR: vstupuje do sekci CSocketsThread::CritSect a CSocket::SocketCritSect !!!
    void PostActivateMsg();

    // vyprazdni buffery pro cteni a zapis (hodi se napr. pred znovu-otevrenim pripojeni)
    void ResetBuffersAndEvents();

    // zapise na socket (provede "send") byty z bufferu 'buffer' o delce 'bytesToWrite'
    // (je-li 'bytesToWrite' -1, zapise strlen(buffer) bytu); pri chybe vraci FALSE a
    // je-li znamy kod Windows chyby, vraci ho v 'error' (neni-li NULL); vraci-li TRUE,
    // alespon cast bytu z bufferu byla uspesne zapsana; v 'allBytesWritten' (nesmi byt
    // NULL) se vraci TRUE pokud probehl zapis celeho bufferu; vraci-li 'allBytesWritten'
    // FALSE, je pred dalsim volanim metody Write nutne pockat na udalost fwseWriteDone
    // (jakmile dojde, je zapis hotovy)
    BOOL Write(const char* buffer, int bytesToWrite, DWORD* error, BOOL* allBytesWritten);

    // pomocna metoda pro detekovani jestli jiz je v bufferu 'ReadBytes' cela odpoved
    // od FTP serveru; vraci TRUE pri uspechu - v 'reply' (nesmi byt NULL) vraci ukazatel
    // na zacatek odpovedi, v 'replySize' (nesmi byt NULL) vraci delku odpovedi,
    // v 'replyCode' (neni-li NULL) vraci FTP kod odpovedi nebo -1 pokud odpoved nema
    // zadny kod (nezacina na triciferne cislo); pokud jeste neni odpoved kompletni, vraci
    // FALSE - dalsi volani ReadFTPReply ma smysl az po prijeti udalosti fwseNewBytesRead
    // POZOR: nutne volat z kriticke sekce SocketCritSect (jinak by se buffer 'ReadBytes'
    // mohl libovolne zmenit)
    BOOL ReadFTPReply(char** reply, int* replySize, int* replyCode = NULL);

    // pomocna metoda pro uvolneni odpovedi od FTP serveru (o delce 'replySize') z bufferu
    // 'ReadBytes'
    // POZOR: nutne volat z kriticke sekce SocketCritSect (jinak by se buffer 'ReadBytes'
    // mohl libovolne zmenit)
    void SkipFTPReply(int replySize);

    // pomocna metoda pro cteni chybovych hlasek z bufferu ReadBytes (socket uz muze byt
    // davno zavreny); hlasky zobrazuje do Logu a je-li ErrorDescr prazdne, naplni do nej
    // prvni chybovou hlasku
    // POZOR: nutne volat z kriticke sekce SocketCritSect (jinak by se buffer 'ReadBytes'
    // mohl libovolne zmenit) a kriticke sekce WorkerCritSect
    void ReadFTPErrorReplies();

    // zjisti jestli je worker ve stavu "sleeping" a pokud je (vraci TRUE), zjisti jeste
    // jestli ma otevrenou connectionu (v 'hasOpenedConnection' vraci TRUE) a jestli uz
    // ceka na doruceni zpravy WORKER_WAKEUP aneb uz ho nekdo budi (v 'receivingWakeup'
    // vraci TRUE)
    BOOL IsSleeping(BOOL* hasOpenedConnection, BOOL* receivingWakeup);

    // nastavi ReceivingWakeup (v kriticke sekci WorkerCritSect)
    void SetReceivingWakeup(BOOL receivingWakeup);

    // predani polozky z 'sourceWorker' do tohoto workera a uvedeni do stavu fwsPreparing,
    // 'sourceWorker' se uvede do stavu fwsLookingForWork (z neho nejspis prejde do fwsSleeping)
    void GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // prida do 'downloaded' velikost (v bytech) prave downloadeneho souboru v tomto workerovi
    void AddCurrentDownloadSize(CQuadWord* downloaded);

    // prida do 'uploaded' velikost (v bytech) prave uploadeneho souboru v tomto workerovi
    void AddCurrentUploadSize(CQuadWord* uploaded);

    // je-li worker ve stavu fwsConnectionError, vraci ErrorOccurenceTime (v kriticke
    // sekci WorkerCritSect); jinak vraci -1
    DWORD GetErrorOccurenceTime();

    // ******************************************************************************************
    // metody volane v "sockets" threadu (na zaklade prijmu zprav od systemu nebo jinych threadu)
    //
    // POZOR: volane v sekci SocketsThread->CritSect, mely by se provadet co nejrychleji (zadne
    //        cekani na vstup usera, atd.)
    // ******************************************************************************************

    // prijem vysledku volani GetHostByAddress; je-li 'ip' == INADDR_NONE jde o chybu a v 'err'
    // muze byt chybovy kod (pokud 'err' != 0)
    virtual void ReceiveHostByAddress(DWORD ip, int hostUID, int err);

    // prijem udalosti pro tento socket (FD_READ, FD_WRITE, FD_CLOSE, atd.); 'index' je
    // index socketu v poli SocketsThread->Sockets (pouziva se pro opakovane posilani
    // zprav pro socket)
    virtual void ReceiveNetEvent(LPARAM lParam, int index);

    // prijem vysledku ReceiveNetEvent(FD_CLOSE) - neni-li 'error' NO_ERROR, jde
    // o kod Windowsove chyby (prisla s FD_CLOSE nebo vznikla behem zpracovani FD_CLOSE)
    virtual void SocketWasClosed(DWORD error);

    // prijem timeru s ID 'id' a parametrem 'param'
    virtual void ReceiveTimer(DWORD id, void* param);

    // prijem postnute zpravy s ID 'id' a parametrem 'param'
    virtual void ReceivePostMessage(DWORD id, void* param);

protected:
    // zpracovani vsech typu udalosti na socketu workeru; vola se jen v sockets threadu, takze
    // soubezne volani nehrozi; vola se v jedine kriticke sekci CSocketsThread::CritSect
    void HandleSocketEvent(CFTPWorkerSocketEvent event, DWORD data1, DWORD data2);

    // zpracovani udalosti workeru (aktivace, posilani prikazu, atd.); vola se jen v sockets
    // threadu, takze soubezne volani nehrozi; vola se v kritickych sekcich
    // CSocketsThread::CritSect a CSocket::SocketCritSect; 'reply'+'replySize'+'replyCode'
    // obsahuji odpoved serveru (jen pri fweCmdReplyReceived a fweCmdInfoReceived, jinak
    // jsou NULL+0+0) - po dokonceni metody HandleEvent() se vola SkipFTPReply(replySize)
    void HandleEvent(CFTPWorkerEvent event, char* reply, int replySize, int replyCode);

    // jen pomocna metoda pro lepsi orientaci v HandleEvent()
    void HandleEventInPreparingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                     BOOL& reportWorkerChange);

    // jen pomocna metoda pro lepsi orientaci v HandleEvent()
    void HandleEventInConnectingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                      BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                      int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                      int replyCode, BOOL& operStatusMaybeChanged);

    // jen pomocna metoda pro lepsi orientaci v HandleEvent()
    void HandleEventInWorkingState(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                   BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                   int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                   int replyCode);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState a HandleEvent()
    void HandleEventInWorkingState2(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL* listingNotAccessible);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState a HandleEvent()
    void HandleEventInWorkingState3(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    char* buf, char* errBuf, int& cmdLen, BOOL& sendCmd,
                                    char* reply, int replySize, int replyCode, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState a HandleEvent()
    void HandleEventInWorkingState4(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL& quitCmdWasSent);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState a HandleEvent()
    void HandleEventInWorkingState5(CFTPWorkerEvent event, BOOL& sendQuitCmd, BOOL& postActivate,
                                    BOOL& reportWorkerChange, char* buf, char* errBuf, char* host,
                                    int& cmdLen, BOOL& sendCmd, char* reply, int replySize,
                                    int replyCode, char* ftpPath, char* errText,
                                    BOOL& conClosedRetryItem, BOOL& lookForNewWork,
                                    BOOL& handleShouldStop, BOOL& quitCmdWasSent);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState()
    BOOL ParseListingToFTPQueue(TIndirectArray<CFTPQueueItem>* ftpQueueItems,
                                const char* allocatedListing, int allocatedListingLen,
                                CServerType* serverType, BOOL* lowMem,
                                BOOL isVMS, BOOL isAS400, int transferMode, CQuadWord* totalSize,
                                BOOL* sizeInBytes, BOOL selFiles, BOOL selDirs,
                                BOOL includeSubdirs, DWORD attrAndMask,
                                DWORD attrOrMask, int operationsUnknownAttrs,
                                int operationsHiddenFileDel, int operationsHiddenDirDel);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void OpenActDataCon(CFTPWorkerSubState waitForListen, char* errBuf,
                        BOOL& conClosedRetryItem, BOOL& lookForNewWork);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void WaitForListen(CFTPWorkerEvent event, BOOL& handleShouldStop, char* errBuf,
                       char* buf, int& cmdLen, BOOL& sendCmd, BOOL& conClosedRetryItem,
                       CFTPWorkerSubState waitForPORTRes);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void WaitForPASVRes(CFTPWorkerEvent event, char* reply, int replySize, int replyCode,
                        BOOL& handleShouldStop, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CFTPWorkerSubState setType, CFTPWorkerSubState openActDataCon);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void WaitForPORTRes(CFTPWorkerEvent event, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CFTPWorkerSubState setType);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void SetTypeA(BOOL& handleShouldStop, char* errBuf, char* buf, int& cmdLen,
                  BOOL& sendCmd, BOOL& nextLoop, CCurrentTransferMode trMode,
                  BOOL asciiTrMode, CFTPWorkerSubState waitForTYPERes,
                  CFTPWorkerSubState trModeAlreadySet);

    // jen pomocna metoda pro lepsi orientaci v HandleEventInWorkingState2()
    void WaitForTYPERes(CFTPWorkerEvent event, int replyCode, BOOL& nextLoop, BOOL& conClosedRetryItem,
                        CCurrentTransferMode trMode, CFTPWorkerSubState trModeAlreadySet);

    // upravi text v bufferu ErrorDescr tak, ze: neobsahuje CR ani LF a na konci nema tecku
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    void CorrectErrorDescr();

    // inicializuje polozky struktury 'DiskWork'
    // POZOR: volat jen uvnitr kriticke sekce CSocket::SocketCritSect + vstupuje do sekce
    //        CFTPOperation::OperCritSect !!!
    void InitDiskWork(DWORD msgID, CFTPDiskWorkType type, const char* path, const char* name,
                      CFTPQueueItemAction forceAction, BOOL alreadyRenamedName,
                      char* flushDataBuffer, CQuadWord const* checkFromOffset,
                      CQuadWord const* writeOrReadFromOffset, int validBytesInFlushDataBuffer,
                      HANDLE workFile);

    // vrati zpracovavanou polozku 'CurItem' zpet do fronty (vraci ji do stavu "waiting", aby ji
    // mohl zpracovat jiny worker)
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    void ReturnCurItemToQueue();

    // zavira otevreny soubor 'OpenedFile' (jen je-li otevreny, jinak neprovadi nic);
    // 'transferAborted' je TRUE pokud doslo k preruseni prenosu souboru (krome zavreni
    // muze dojit i k vymazu prazdneho souboru), jinak je FALSE (soubor byl uspesne prenesen);
    // je-li 'setDateAndTime' TRUE, dojde pred zavrenim souboru k nastaveni casu zapisu
    // na 'date'+'time' (POZOR: je-li date->Day==0 resp. time->Hour==24, jde o
    // "prazdne hodnoty" u datumu resp. casu); je-li 'deleteFile' TRUE, dojde k vymazu
    // souboru ihned po jeho uzavreni; neni-li 'setEndOfFile' NULL, dojde po zavreni
    // souboru k oriznuti na offsetu 'setEndOfFile'
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    void CloseOpenedFile(BOOL transferAborted, BOOL setDateAndTime, const CFTPDate* date,
                         const CFTPTime* time, BOOL deleteFile, CQuadWord* setEndOfFile);

    // zavira otevreny soubor 'OpenedInFile' (jen je-li otevreny, jinak neprovadi nic);
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    void CloseOpenedInFile();

    // hlasi zavreni socketu workera nebo cancel/dokonceni prace workera v disk-threadu,
    // viz WorkerMayBeClosedEvent
    void ReportWorkerMayBeClosed();

    // zpracuje chybu (viz FlushDataError), ktera nastala pri flushovani dat
    // (Copy a Move operace) a vynuluje FlushDataError; vraci TRUE pokud nejaka
    // chyba nastala a byla zpracovana; vraci FALSE pokud zadna chyba nenastala
    // a je mozne normalne pokracovat
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    BOOL HandleFlushDataError(CFTPQueueItemCopyOrMove* curItem, BOOL& lookForNewWork);

    // zpracuje chybu (viz PrepareDataError), ktera nastala pri priprave dat
    // (upload: Copy a Move operace) a vynuluje PrepareDataError; vraci TRUE pokud nejaka
    // chyba nastala a byla zpracovana; vraci FALSE pokud zadna chyba nenastala
    // a je mozne normalne pokracovat
    // POZOR: volat jen uvnitr kriticke sekce WorkerCritSect !!!
    BOOL HandlePrepareDataError(CFTPQueueItemCopyOrMoveUpload* curItem, BOOL& lookForNewWork);

    friend class CControlConnectionSocket;
};

//
// ****************************************************************************
// CFTPWorkersList
//

class CFTPWorkersList
{
protected:
    // kriticka sekce pro pristup k datum objektu
    // POZOR: pristup do kritickych sekci konzultovat v souboru servers\critsect.txt !!!
    CRITICAL_SECTION WorkersListCritSect;

    TIndirectArray<CFTPWorker> Workers; // pole workeru
    int NextWorkerID;                   // pocitadlo pro ID workeru (zobrazuje se v listview Connections v dialogu operace)

    DWORD LastFoundErrorOccurenceTime; // "cas" posledniho nalezeneho workera s chybou nebo "cas", pred kterym uz zadny worker s chybou neexistuje

public:
    CFTPWorkersList();
    ~CFTPWorkersList();

    // prida do pole noveho workera; vraci TRUE pri uspechu
    // POZOR: nemelo by dojit k volani behem provadeni ActivateWorkers(), PostLoginChanged(),
    //        PostNewWorkAvailable()
    BOOL AddWorker(CFTPWorker* newWorker);

    // informuje workery o tom, ze se maji snazit stopnout; pracuje se vsemi workery
    // operace (je-li 'workerInd' -1) nebo jen s workerem na indexu 'workerInd'; je
    // nutne volat opakovane dokud vraci TRUE (zpracovani je davkove); 'victims'+'maxVictims'
    // je pole pro workery, kteri potrebuji zavrit datove spojeni nebo postnout WORKER_SHOULDSTOP
    // (fweWorkerShouldStop) volanim CloseDataConnectionOrPostShouldStop() po dokonceni teto
    // metody; ve 'foundVictims' vstupuje pocet obsazenych polozek pole 'victims' a vraci se
    // aktualizovany pocet obsazenych polozek
    BOOL InformWorkersAboutStop(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // informuje workery o tom, ze se maji snazit o pause/resume; pracuje se vsemi workery
    // operace (je-li 'workerInd' -1) nebo jen s workerem na indexu 'workerInd'; je
    // nutne volat opakovane dokud vraci TRUE (zpracovani je davkove); 'victims'+'maxVictims'
    // je pole pro workery, kteri potrebuji postnout WORKER_SHOULDPAUSE nebo WORKER_SHOULDRESUME
    // (fweWorkerShouldPause nebo fweWorkerShouldResume) volanim PostShouldPauseOrResume() po
    // dokonceni teto metody; ve 'foundVictims' vstupuje pocet obsazenych polozek pole 'victims'
    // a vraci se aktualizovany pocet obsazenych polozek; v 'pause' je TRUE pokud se ma provest
    // pause, FALSE pokud se ma provest resume
    BOOL InformWorkersAboutPause(int workerInd, CFTPWorker** victims, int maxVictims,
                                 int* foundVictims, BOOL pause);

    // zjisti jestli uz vsichni workeri (je-li 'workerInd' -1) nebo worker na indexu
    // 'workerInd' ma zavreny socket a nema rozdelanou praci v disk-threadu; vraci
    // TRUE pokud jsou sockety zavrene (disconnected) a workeri nemaji praci
    // v disk-threadu
    BOOL CanCloseWorkers(int workerInd);

    // donuti vsechny workery operace (je-li 'workerInd' -1) nebo workera na indexu
    // 'workerInd' urychlene zavrit socket a cancelovat praci v disk-threadu (pokud uz
    // je prace hotova, necha workera zpracovat jeji vysledky, nemelo by brzdit); je
    // nutne volat opakovane dokud vraci TRUE (zpracovani je davkove); 'victims'+'maxVictims'
    // je pole pro workery, kterym se ma zavolat metoda ForceClose() po dokonceni
    // teto metody; ve 'foundVictims' vstupuje pocet obsazenych polozek pole 'victims'
    // a vraci se aktualizovany pocet obsazenych polozek
    BOOL ForceCloseWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // zrusi postupne vsechny workery operace (je-li 'workerInd' -1) nebo zrusi
    // workera na indexu 'workerInd'; je nutne volat opakovane dokud vraci TRUE
    // (zpracovani je davkove); 'victims'+'maxVictims' je pole pro sockety,
    // ktere se maji zavrit volanim DeleteSocket() po dokonceni
    // teto metody; ve 'foundVictims' vstupuje pocet obsazenych polozek pole
    // 'victims' a vraci se aktualizovany pocet obsazenych polozek;
    // v 'uploadFirstWaitingWorker' je aktualizovany seznam workeru cekajicich na
    // WORKER_TGTPATHLISTINGFINISHED
    BOOL DeleteWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims,
                       CUploadWaitingWorker** uploadFirstWaitingWorker);

    // vraci pocet workeru
    int GetCount();

    // vraci index prvniho workera, ktery hlasi chybu (stav fwsConnectionError);
    // pokud zadny worker chybu nehlasi, vraci -1
    int GetFirstErrorIndex();

    // vraci index workera s ID 'workerID'; pokud neni nalezen, vraci -1
    int GetWorkerIndex(int workerID);

    // vraci data pro zobrazeni workera s indexem 'index' v listview v dialogu operace;
    // 'buf'+'bufSize' je buffer pro text vraceny v 'lvdi' (meni se tri cyklicky,
    // aby se splnily naroky LVN_GETDISPINFO); pokud index neni
    // platny, nedela nic (refresh listview uz je na ceste)
    void GetListViewDataFor(int index, NMLVDISPINFO* lvdi, char* buf, int bufSize);

    // vraci ID workeru na indexu 'index'; -1 = neplatny index
    int GetWorkerID(int index);

    // vraci UID logu workeru na indexu 'index'; -1 = neplatny index nebo worker nema log
    int GetLogUID(int index);

    // vraci TRUE pokud je mozne, ze worker na indexu 'index' potrebuje od usera vyresit
    // chybu (viz CFTPWorker::HaveError()); pri neplatnem indexu vraci FALSE
    BOOL HaveError(int index);

    // vraci TRUE pokud je worker na indexu 'index' pausnuty (viz CFTPWorker::IsPaused());
    // v 'isWorking' (nesmi byt NULL) vraci TRUE pokud worker pracuje (nespi, neceka na
    // usera a neni ukonceny);
    // pri neplatnem indexu vraci FALSE (i v 'isWorking')
    BOOL IsPaused(int index, BOOL* isWorking);

    // vraci TRUE pokud aspon jeden worker pracuje (nespi, neceka na usera a neni ukonceny);
    // v 'someIsWorkingAndNotPaused' (nesmi byt NULL) vraci TRUE pokud aspon
    // jeden worker pracuje (nespi, neceka na usera a neni ukonceny) a neni pausnuty
    BOOL SomeWorkerIsWorking(BOOL* someIsWorkingAndNotPaused);

    // vraci TRUE pokud worker na indexu 'index' potrebuje od usera vyresit chybu
    // (viz HaveError()), pokud je tento worker ve stavu fwsWaitingForReconnect, zmeni se
    // do stavu fwsConnectionError; pokud vraci TRUE, vraci i text chyby v 'buf' (buffer
    // o velikosti 'bufSize') a pokud je chyba zpusobena neduveryhodnym serverovym certifikatem,
    // vraci ho v 'unverifiedCertificate' (neni-li NULL; volajici ma na starosti pripadne
    // uvolneni certifikatu jeho metodou Release()); pri neplatnem indexu vraci FALSE
    // POZOR: vstupuje do sekce CSocketsThread::CritSect !!!
    BOOL GetErrorDescr(int index, char* buf, int bufSize, CCertificate** unverifiedCertificate);

    // aktivuje vsechny workery (postne socketu workera WORKER_ACTIVATE)
    // POZOR: nepouziva plne sekci WorkersListCritSect, nemusi se provest pro workera
    //        pridaneho behem provadeni metody (viz AddWorker()) !!!
    // POZOR: vstupuje do sekci CSocketsThread::CritSect a CSocket::SocketCritSect !!!
    void ActivateWorkers();

    // informuje vybrane workery s chybou o zmene login parametru (ma se provest novy pokus
    // o connect); je-li 'workerID' -1, informuje vsechny workery ve stavu fwsConnectionError;
    // neni-li 'workerID' -1, informuje workera s ID 'workerID' (je-li ve stavu fwsConnectionError)
    // POZOR: nepouziva plne sekci WorkersListCritSect, nemusi se provest pro workera
    //        pridaneho behem provadeni metody (viz AddWorker()) !!!
    // POZOR: vstupuje do sekci CSocketsThread::CritSect !!!
    void PostLoginChanged(int workerID);

    // informuje workery ve stavu "sleeping" o existenci nove prace (impulz pro hledani
    // prace ve fronte polozek); je-li 'onlyOneItem' TRUE, jde jen o jednu polozku
    // (staci informovat jednoho workera), jinak jde o vic polozek (informovat vsechny
    // workery); informovani workeru = postnuti WORKER_WAKEUP;
    // POZOR: nepouziva plne sekci WorkersListCritSect, nemusi se provest pro workera
    //        pridaneho behem provadeni metody (viz AddWorker()) !!!
    // POZOR: vstupuje do sekce CSocketsThread::CritSect !!!
    void PostNewWorkAvailable(BOOL onlyOneItem);

    // pokusi se najit "sleeping" workera s otevrenou connectionou, pri uspechu
    // vraci TRUE a preda do nej polozku z 'sourceWorker' a uvede ho do stavu
    // fwsPreparing (+ postne mu fweActivate), 'sourceWorker' se uvede do stavu
    // fwsSleeping
    // POZOR: vstupuje do sekce CSocketsThread::CritSect !!!
    BOOL GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // prida do 'downloaded' velikost (v bytech) prave downloadenych souboru ze vsech workeru
    void AddCurrentDownloadSize(CQuadWord* downloaded);

    // prida do 'uploaded' velikost (v bytech) prave uploadenych souboru ze vsech workeru
    void AddCurrentUploadSize(CQuadWord* uploaded);

    // hleda index workera, ktery potrebuje otevrit Solve Error dialog (objevila se
    // v nem "nova" (user ji jeste nevidel) chyba); 'lastErrorOccurenceTime' je "cas"
    // prideleny posledni chybe (pouziva se pro zrychleny test jestli ma smysl vubec
    // hledat "novou" chybu); vraci TRUE pokud se takoveho workera podarilo nalezt,
    // jeho index se vraci v 'index'
    BOOL SearchWorkerWithNewError(int* index, DWORD lastErrorOccurenceTime);

    // vraci TRUE pokud je seznam workeru prazdny nebo vsichni workeri maji koncit
    BOOL EmptyOrAllShouldStop();

    // vraci TRUE pokud je aspon jeden worker ve stavu cekani na uzivatele
    BOOL AtLeastOneWorkerIsWaitingForUser();
};

//
// ****************************************************************************
// CTransferSpeedMeter
//
// objekt pro vypocet rychlosti prenosu dat v data-connectione

#define DATACON_ACTSPEEDSTEP 1000     // pro vypocet prenosove rychlosti: velikost kroku v milisekundach (nesmi byt 0)
#define DATACON_ACTSPEEDNUMOFSTEPS 60 // pro vypocet prenosove rychlosti: pocet pouzitych kroku (vic kroku = jemnejsi zmeny rychlosti pri "vypadku" prvniho kroku ve fronte)

class CTransferSpeedMeter
{
protected:
    CRITICAL_SECTION TransferSpeedMeterCS; // kriticka sekce pro pristup k datum objektu

    // vypocet prenosove rychlosti:
    DWORD TransferedBytes[DATACON_ACTSPEEDNUMOFSTEPS + 1]; // kruhova fronta s poctem bytu prenesenych v poslednich N krocich (cas. intervalech) + jeden "pracovni" krok navic (nascitava se v nem hodnota za akt. interval)
    int ActIndexInTrBytes;                                 // index posledniho (aktualniho) zaznamu v TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                        // casova hranice (v ms) posledniho zaznamu v TransferedBytes (do tohoto casu se nacitaji byty do posl. zaznamu)
    int CountOfTrBytesItems;                               // pocet kroku v TransferedBytes (uzavrene + jeden "pracovni")

    DWORD LastTransferTime; // GetTickCount z okamziku posledniho volani BytesReceived

public:
    CTransferSpeedMeter();
    ~CTransferSpeedMeter();

    // vynuluje objekt (priprava pro dalsi pouziti)
    // volani mozne z libovolneho threadu
    void Clear();

    // vraci rychlost spojeni v bytech za sekundu; v 'transferIdleTime' (muze byt NULL)
    // vraci cas v sekundach od posledniho prijmu dat
    // volani mozne z libovolneho threadu
    DWORD GetSpeed(DWORD* transferIdleTime);

    // vola se v okamziku navazani spojeni (aktivniho/pasivniho)
    // volani mozne z libovolneho threadu
    void JustConnected();

    // vola se po uskutecneni prenosu casti dat; v 'count' je o kolik dat slo; 'time' je
    // casu prenosu
    void BytesReceived(DWORD count, DWORD time);
};

//
// ****************************************************************************
// CSynchronizedDWORD
//
// DWORD se synchronizovanym pristupem pro pouziti z vice threadu

class CSynchronizedDWORD
{
private:
    CRITICAL_SECTION ValueCS; // kriticka sekce pro pristup k datum objektu
    DWORD Value;

public:
    CSynchronizedDWORD();
    ~CSynchronizedDWORD();

    void Set(DWORD value);
    DWORD Get();
};

//
// ****************************************************************************
// CFTPOperation
//
// objekt s informacemi o operaci jako celku (parametry pripojeni, typ operace, atd.)

enum CFTPOperationType
{
    fotNone,         // prazdna operace (nutne nastavit na jeden z nasledujicich typu)
    fotDelete,       // operace mazani
    fotCopyDownload, // operace kopirovani ze serveru
    fotMoveDownload, // operace presunu ze serveru
    fotChangeAttrs,  // operace zmeny atributu
    fotCopyUpload,   // operace kopirovani na server
    fotMoveUpload,   // operace presunu na server
};

// jak resit problem "cilovy soubor/adresar nelze vytvorit"
#define CANNOTCREATENAME_USERPROMPT 0 // zeptat se uzivatele
#define CANNOTCREATENAME_AUTORENAME 1 // pouzit automaticke prejmenovani
#define CANNOTCREATENAME_SKIP 2       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "cilovy soubor jiz existuje"
#define FILEALREADYEXISTS_USERPROMPT 0 // zeptat se uzivatele
#define FILEALREADYEXISTS_AUTORENAME 1 // pouzit automaticke prejmenovani
#define FILEALREADYEXISTS_RESUME 2     // resumnout (pridat na konec souboru - soubor se muze jedine zvetsit)
#define FILEALREADYEXISTS_RES_OVRWR 3  // resumnout nebo prepsat (pokud nejde resume, soubor se smaze + vytvori znovu)
#define FILEALREADYEXISTS_OVERWRITE 4  // prepsat (smazat + vytvorit znovu)
#define FILEALREADYEXISTS_SKIP 5       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "cilovy adresar jiz existuje"
#define DIRALREADYEXISTS_USERPROMPT 0 // zeptat se uzivatele
#define DIRALREADYEXISTS_AUTORENAME 1 // pouzit automaticke prejmenovani
#define DIRALREADYEXISTS_JOIN 2       // vyuzit existujici adresar jako cilovy adresar (spojit adresare)
#define DIRALREADYEXISTS_SKIP 3       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "retry na souboru FTP klientem primo vytvorenem nebo prepsanem"
#define RETRYONCREATFILE_USERPROMPT 0 // zeptat se uzivatele
#define RETRYONCREATFILE_AUTORENAME 1 // pouzit automaticke prejmenovani
#define RETRYONCREATFILE_RESUME 2     // resumnout (pridat na konec souboru - soubor se muze jedine zvetsit)
#define RETRYONCREATFILE_RES_OVRWR 3  // resumnout nebo prepsat (pokud nejde resume, soubor se smaze + vytvori znovu)
#define RETRYONCREATFILE_OVERWRITE 4  // prepsat (smazat + vytvorit znovu)
#define RETRYONCREATFILE_SKIP 5       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "retry na souboru FTP klientem resumnutem"
#define RETRYONRESUMFILE_USERPROMPT 0 // zeptat se uzivatele
#define RETRYONRESUMFILE_AUTORENAME 1 // pouzit automaticke prejmenovani
#define RETRYONRESUMFILE_RESUME 2     // resumnout (pridat na konec souboru - soubor se muze jedine zvetsit)
#define RETRYONRESUMFILE_RES_OVRWR 3  // resumnout nebo prepsat (pokud nejde resume, soubor se smaze + vytvori znovu)
#define RETRYONRESUMFILE_OVERWRITE 4  // prepsat (smazat + vytvorit znovu)
#define RETRYONRESUMFILE_SKIP 5       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "ASCII transfer mode pro binarni soubor"
#define ASCIITRFORBINFILE_USERPROMPT 0 // zeptat se uzivatele
#define ASCIITRFORBINFILE_IGNORE 1     // ignorovat, user vi co dela
#define ASCIITRFORBINFILE_INBINMODE 2  // zmenit transfer mode na binary a zacit prenos znovu (predpokladem je, ze se jeste do souboru nezapsalo)
#define ASCIITRFORBINFILE_SKIP 3       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit situaci "mazani neprazdneho adresare"
#define NONEMPTYDIRDEL_USERPROMPT 0 // zeptat se uzivatele
#define NONEMPTYDIRDEL_DELETEIT 1   // smazeme ho bez ptani
#define NONEMPTYDIRDEL_SKIP 2       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit situaci "mazani hidden souboru"
#define HIDDENFILEDEL_USERPROMPT 0 // zeptat se uzivatele
#define HIDDENFILEDEL_DELETEIT 1   // smazeme ho bez ptani
#define HIDDENFILEDEL_SKIP 2       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit situaci "mazani hidden adresare"
#define HIDDENDIRDEL_USERPROMPT 0 // zeptat se uzivatele
#define HIDDENDIRDEL_DELETEIT 1   // smazeme ho bez ptani
#define HIDDENDIRDEL_SKIP 2       // skip (operaci proste neprovedeme)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

// jak resit problem "soubor/adresar ma nezname atributy, ktere neumime zachovat (jina prava nez 'r'+'w'+'x')"
#define UNKNOWNATTRS_USERPROMPT 0 // zeptat se uzivatele
#define UNKNOWNATTRS_IGNORE 1     // ignorovat, user vi co dela (nastavit atributy co nejblizsi pozadovanym)
#define UNKNOWNATTRS_SKIP 2       // na tomto souboru/adresari atributy nebudeme menit (skipnuti souboru/adresare)
// POZOR: pri pridani hodnoty je nutne zkontrolovat bitovy rozsah v CFTPOperation !!!

class CExploredPaths
{
protected:
    TIndirectArray<char> Paths; // format cest: short s delkou cesty + samotny text cesty + null-terminator

public:
    CExploredPaths() : Paths(100, 500) {}

    // vraci TRUE pokud cesta 'path' uz je v 'Paths';
    // POZOR: vychazi z predpokladu, ze 'path' je cesta vracena serverem, takze se cesty
    // porovnavaji jen jako case-sensitive retezce (neorezavaji se slashe/backslashe/tecky,
    // atd.) - k detekci cyklu muze dojit az pri jeho druhem pruchodu, pro nase ucely to staci
    BOOL ContainsPath(const char* path);

    // ulozi cestu 'path' do seznamu 'Paths';
    // vraci FALSE pokud 'path' uz je v tomto seznamu (nedostatek pameti se v teto funkci ignoruje)
    // POZOR: vychazi z predpokladu, ze 'path' je cesta vracena serverem, takze se cesty
    // porovnavaji jen jako case-sensitive retezce (neorezavaji se slashe/backslashe/tecky,
    // atd.) - k detekci cyklu muze dojit az pri jeho druhem pruchodu, pro nase ucely to staci
    BOOL AddPath(const char* path);

protected:
    // vraci "nalezeno ?" a index polozky nebo kam se ma vlozit (razene pole)
    BOOL GetPathIndex(const char* path, int pathLen, int& index);
};

enum COperationState
{
    opstNone,                 // prazdna hodnota (inicializace promennych)
    opstInProgress,           // operace jeste probiha (existuji polozky ve stavech sqisWaiting, sqisProcessing, sqisDelayed nebo sqisUserInputNeeded)
    opstSuccessfullyFinished, // operace byla uspesne dokoncena (vsechny polozky jsou ve stavu sqisDone)
    opstFinishedWithSkips,    // operace byla uspesne dokoncena, ale se skipy (polozky jsou ve stavech sqisDone nebo sqisSkipped)
    opstFinishedWithErrors    // operace byla dokoncena, ale s chybami (polozky jsou ve stavech sqisDone, sqisSkipped, sqisFailed nebo sqisForcedToFail)
};

#define SMPLCMD_APPROXBYTESIZE 1000 // priblizna velikost zpracovani jedne polozky v bytech pro mereni rychlosti operaci Delete a ChangeAttrs

class CFTPOperation
{
public:
    static int NextOrdinalNumber;                // globalni pocitadlo pro OrdinalNumber operace (pristupovat jen v sekci NextOrdinalNumberCS!)
    static CRITICAL_SECTION NextOrdinalNumberCS; // kriticka sekce pro NextOrdinalNumber

protected:
    // kriticka sekce pro pristup k datum objektu
    // POZOR: pristup do kritickych sekci konzultovat v souboru servers\critsect.txt !!!
    CRITICAL_SECTION OperCritSect;

    int UID;           // unikatni cislo operace (index v poli CFTPOperationsList::Operations + vazba pro polozky v Queue)
    int OrdinalNumber; // poradove cislo operace (umisteni v poli CFTPOperationsList::Operations neni podle casu vzniku operace)

    CFTPQueue* Queue; // fronta polozek operace

    CFTPWorkersList WorkersList; // pole workeru ("control connections" zpracovavajici polozky operace z fronty)

    COperationDlg* OperationDlg; // objekt dialogu operace (bezi ve vlastnim threadu)
    HANDLE OperationDlgThread;   // handle threadu, ve kterem bezel/bezi naposledy otevreny dialog operace

    CFTPProxyServer* ProxyServer;          // NULL = "not used (direct connection)"
    const char* ProxyScriptText;           // text proxy skriptu (existuje i kdyz se nepouziva proxy server); POZOR: muze ukazovat na ProxyServer->ProxyScript (aneb text je platny jen do dealokace ProxyServer)
    const char* ProxyScriptStartExecPoint; // radek s prvnim prikazem (radek za radkem s "connect to:")
    char* ConnectToHost;                   // "host" podle proxy scriptu
    unsigned short ConnectToPort;          // "port" podle proxy scriptu
    DWORD HostIP;                          // IP adresa FTP serveru 'Host' (==INADDR_NONE pokud neni IP zname) (pouziva se jen pro SOCKS4 proxy server)

    char* Host;                   // hostitel (FTP server) (NULL=neni znamy)
    unsigned short Port;          // port, na kterem bezi FTP server (-1=neni znamy)
    char* User;                   // uzivatel (NULL=neni znamy) POZOR: anonymous je zde jiz ve stringu
    char* Password;               // heslo (NULL=neni zname) POZOR: anonymous heslo (email) je zde jiz ve stringu
    char* Account;                // account-info (viz FTP prikaz "ACCT") (NULL=neni zname)
    BOOL RetryLoginWithoutAsking; // TRUE = worker ma zkouset reconnect i u "error" odpovedi serveru (kod "5xx"); FALSE = reconnect jen pro "transient-error" odpovedi (kod "4xx"); nastavuje az user pri reseni problemu connectiony (workera)
    char* InitFTPCommands;        // seznam FTP prikazu, ktere se maji poslat serveru hned po pripojeni (NULL=zadne prikazy)
    BOOL UsePassiveMode;          // TRUE/FALSE = pasive/active data connection mode
    BOOL SizeCmdIsSupported;      // FALSE = na prikaz SIZE jsme dostali odpoved serveru "not supported" (nema smysl zkouset znovu)
    char* ListCommand;            // prikaz ziskani listingu cesty na tomto serveru (NULL="LIST")
    DWORD ServerIP;               // IP adresa FTP/Proxy serveru 'ConnectToHost' (==INADDR_NONE dokud neni IP zname)
    char* ServerSystem;           // system serveru (odpoved na prikaz SYST) - muze byt i NULL
    char* ServerFirstReply;       // prvni odpoved serveru (obsahuje casto verzi FTP serveru) - muze byt i NULL
    BOOL UseListingsCache;        // TRUE = user si u tohoto spojeni preje ukladat listingy do cache
    char* ListingServerType;      // typ serveru pro parsovani listingu: NULL = autodetekce; jinak jmeno typu serveru (bez prip. uvodni '*'; pokud prestane existovat, prepne se na autodetekci)
    BOOL EncryptControlConnection;
    BOOL EncryptDataConnection;
    int CompressData;
    CCertificate* pCertificate;

    CExploredPaths ExploredPaths; // seznam prozkoumanych cest (cesty, na kterych workeri uz delali "explore-dir")

    int ReportChangeInWorkerID; // -2 = zadne zmeny (je treba postnout dialogu operace WM_APP_WORKERCHANGEREP), -1 = zmena ve vice nez jednom workerovi, jinak ID zmeneneho workera (typ zmen viz ReportWorkerChange())
    BOOL ReportProgressChange;  // TRUE = doslo k updatu status informaci workera, bude se menit progress v dialogu operace (viz ReportWorkerChange())
    int ReportChangeInItemUID;  // -3 = zadne zmeny (je treba postnout dialogu operace WM_APP_ITEMCHANGEREP), -2 = jedna zmena (ulozena v ReportChangeInItemUID2), -1 = zmena ve vice nez dvou polozkach, jinak UID druhe zmenene polozky (typ zmen viz ReportItemChange()) - prvni zmenena polozka je v ReportChangeInItemUID2
    int ReportChangeInItemUID2; // UID prvni zmenene polozky (platnost viz ReportChangeInItemUID)

    BOOL OperStateChangedPosted;           // TRUE = dialogu operace jsme jiz postnuli WM_APP_OPERSTATECHANGE, cekame az zareaguje (zavola GetOperationState(TRUE))
    COperationState LastReportedOperState; // posledni stav operace vraceny metodou GetOperationState(TRUE)

    DWORD OperationStart; // GetTickCount() z okamziku spusteni operace (pri preruseni a opetovnem rozbehu dojde k posunu o "idle" cas, aby vychazel celkovy elapsed-time)
    DWORD OperationEnd;   // GetTickCount() z okamziku dokonceni (i s chybami) operace (-1 = neplatny - operace jeste bezi)

    CFTPOperationType Type; // typ operace
    char* OperationSubject; // s cim operace pracuje ("file "test.txt"", "3 files and 1 directory", atp.)

    int ChildItemsNotDone;  // pocet nedokoncenych "child" polozek (krome typu sqisDone)
    int ChildItemsSkipped;  // pocet skipnutych "child" polozek (typ sqisSkipped)
    int ChildItemsFailed;   // pocet failnutych "child" polozek (typ sqisFailed a sqisForcedToFail)
    int ChildItemsUINeeded; // pocet user-input-needed "child" polozek (typ sqisFailed a sqisForcedToFail)

    char* SourcePath;                 // zdrojova cesta operace (plna cesta, pripadne vcetne fs-name)
    char SrcPathSeparator;            // nejvic pouzivany oddelovac zdrojove cesty ('/', '.', atp.)
    BOOL SrcPathCanChange;            // TRUE pokud se na zdrojove ceste maji hlasit zmeny po dokonceni operace
    BOOL SrcPathCanChangeInclSubdirs; // je-li SrcPathCanChange TRUE, je zde ulozeno jestli se zmeny tykaji i podadresaru dane cesty

    DWORD LastErrorOccurenceTime; // "cas" vzniku posledni chyby (inicializovany na -1)

    // ****************************************************************************
    // pro Change Attributes (+ ma jeste dalsi sekci):
    // ****************************************************************************

    WORD AttrAnd; // AND maska pro vypocet pozadovanych atributu (modu) souboru/adresaru (new_attr = (cur_attr & AttrAnd) | AttrOr)
    WORD AttrOr;  // OR maska pro vypocet pozadovanych atributu (modu) souboru/adresaru (new_attr = (cur_attr & AttrAnd) | AttrOr)

    // ****************************************************************************
    // pro Copy a Move (download i upload):
    // ****************************************************************************

    char* TargetPath;                 // cilova cesta operace (plna cesta, pripadne vcetne fs-name)
    char TgtPathSeparator;            // nejvic pouzivany oddelovac cilove cesty ('/', '.', atp.)
    BOOL TgtPathCanChange;            // TRUE pokud se na cilove ceste maji hlasit zmeny po dokonceni operace
    BOOL TgtPathCanChangeInclSubdirs; // je-li TgtPathCanChange TRUE, je zde ulozeno jestli se zmeny tykaji i podadresaru dane cesty

    CSalamanderMaskGroup* ASCIIFileMasks; // pro automaticky rezim prenosu souboru - masky pro "ascii" rezim (ostatni budou "binar") (NULL=neni znama)

    CQuadWord TotalSizeInBytes; // soucet velikosti kopirovanych/presouvanych souboru u nichz je znama velikost v bytech
    // platne jen pro download:
    CQuadWord TotalSizeInBlocks; // soucet velikosti kopirovanych/presouvanych souboru u nichz je znama velikost v blocich

    // platne jen pro download:
    // udaje pro vypocet priblizne velikosti bloku (u VMS, MVS a dalsich serveru pouzivajicich
    // bloky - jinde se ignoruje)
    CQuadWord BlkSizeTotalInBytes;  // celkova velikost dosud ziskanych souboru v bytech (bereme jen soubory s velikosti aspon dva bloky)
    CQuadWord BlkSizeTotalInBlocks; // celkova velikost dosud ziskanych souboru v blocich (bereme jen soubory s velikosti aspon dva bloky)
    DWORD BlkSizeActualValue;       // soucasna hodnota realne velikosti bloku (pro odhad progressu) (-1=neni znama)

    BOOL ResumeIsNotSupported;         // TRUE = FTP prikaz REST/APPE vraci permanentni chybu (napr. "not implemented")
    BOOL DataConWasOpenedForAppendCmd; // TRUE = pouziti FTP prikazu APPE vedlo k otevreni data-connectiony se serverem (na 99.9% je append funkcni/implementovany)

    unsigned AutodetectTrMode : 1;     // TRUE/FALSE = autodetekce podle ASCIIFileMasks / vyber podle UseAsciiTransferMode
    unsigned UseAsciiTransferMode : 1; // TRUE/FALSE = ASCII/Binary transfer mode (pouziva se jen pri AutodetectTrMode==FALSE)

    // platne jen pro download:
    // userem preferovane zpusoby reseni nasledujicich problemu
    unsigned CannotCreateFile : 2;      // viz konstanty CANNOTCREATENAME_XXX
    unsigned CannotCreateDir : 2;       // viz konstanty CANNOTCREATENAME_XXX
    unsigned FileAlreadyExists : 3;     // viz konstanty FILEALREADYEXISTS_XXX
    unsigned DirAlreadyExists : 2;      // viz konstanty DIRALREADYEXISTS_XXX
    unsigned RetryOnCreatedFile : 3;    // viz konstanty RETRYONCREATFILE_XXX
    unsigned RetryOnResumedFile : 3;    // viz konstanty RETRYONRESUMFILE_XXX
    unsigned AsciiTrModeButBinFile : 2; // viz konstanty ASCIITRFORBINFILE_XXX

    // platne jen pro upload:
    // userem preferovane zpusoby reseni nasledujicich problemu
    unsigned UploadCannotCreateFile : 2;      // viz konstanty CANNOTCREATENAME_XXX
    unsigned UploadCannotCreateDir : 2;       // viz konstanty CANNOTCREATENAME_XXX
    unsigned UploadFileAlreadyExists : 3;     // viz konstanty FILEALREADYEXISTS_XXX
    unsigned UploadDirAlreadyExists : 2;      // viz konstanty DIRALREADYEXISTS_XXX
    unsigned UploadRetryOnCreatedFile : 3;    // viz konstanty RETRYONCREATFILE_XXX
    unsigned UploadRetryOnResumedFile : 3;    // viz konstanty RETRYONRESUMFILE_XXX
    unsigned UploadAsciiTrModeButBinFile : 2; // viz konstanty ASCIITRFORBINFILE_XXX

    // ****************************************************************************
    // pro Delete:
    // ****************************************************************************

    // userem preferovane chovani v nasledujicich situacich
    unsigned ConfirmDelOnNonEmptyDir : 2; // viz konstanty NONEMPTYDIRDEL_XXX
    unsigned ConfirmDelOnHiddenFile : 2;  // viz konstanty HIDDENFILEDEL_XXX
    unsigned ConfirmDelOnHiddenDir : 2;   // viz konstanty HIDDENDIRDEL_XXX

    // ****************************************************************************
    // dalsi pro Change Attributes:
    // ****************************************************************************

    unsigned ChAttrOfFiles : 1; // TRUE/FALSE = menit/nemenit atributy souboru
    unsigned ChAttrOfDirs : 1;  // TRUE/FALSE = menit/nemenit atributy adresaru

    // userem preferovane zpusoby reseni nasledujicich problemu
    unsigned UnknownAttrs : 2; // viz konstanty UNKNOWNATTRS_XXX

    // data bez potreby kriticke sekce (ma vlastni synchronizaci + pouziva se jen v objektech s kratsi zivotnosti):
    // globalni speed-meter operace (mereni celkove rychlosti data-connection vsech workeru)
    CTransferSpeedMeter GlobalTransferSpeedMeter;

    // data bez potreby kriticke sekce (ma vlastni synchronizaci + pouziva se jen v objektech s kratsi zivotnosti):
    // globalni objekt pro ukladani casu posledni aktivity na skupine data-connection (vsech
    // data-connection workeru FTP operace)
    CSynchronizedDWORD GlobalLastActivityTime;

public:
    CFTPOperation();
    ~CFTPOperation();

    // ziska UID (v kriticke sekci)
    int GetUID();

    // nastavi UID (v kriticke sekci)
    void SetUID(int uid);

    // nastavi udaje o spojeni na FTP server - vyuzivaji vsechny operace; vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetConnection(CFTPProxyServer* proxyServer, const char* host, unsigned short port,
                       const char* user, const char* password, const char* account,
                       const char* initFTPCommands, BOOL usePassiveMode,
                       const char* listCommand, DWORD serverIP,
                       const char* serverSystem, const char* serverFirstReply,
                       BOOL useListingsCache, DWORD hostIP);

    // nastavi zakladni udaje operace - vyuzivaji vsechny operace
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetBasicData(char* operationSubject, const char* listingServerType);

    // nastavi tento objekt pro operaci Delete (pouziva se az po volani SetConnection() a
    // SetBasicData())
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetOperationDelete(const char* sourcePath, char srcPathSeparator,
                            BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                            int confirmDelOnNonEmptyDir, int confirmDelOnHiddenFile,
                            int confirmDelOnHiddenDir);

    // nastavi tento objekt pro operaci Copy/Move ze serveru (pouziva se az po volani
    // SetConnection() a SetBasicData()); vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetOperationCopyMoveDownload(BOOL isCopy, const char* sourcePath, char srcPathSeparator,
                                      BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                      const char* targetPath, char tgtPathSeparator,
                                      BOOL tgtPathCanChange, BOOL tgtPathCanChangeInclSubdirs,
                                      const char* asciiFileMasks, int autodetectTrMode,
                                      int useAsciiTransferMode, int cannotCreateFile, int cannotCreateDir,
                                      int fileAlreadyExists, int dirAlreadyExists, int retryOnCreatedFile,
                                      int retryOnResumedFile, int asciiTrModeButBinFile);

    // nastavi tento objekt pro operaci Change Attributes (pouziva se az po volani SetConnection()
    // a SetBasicData())
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    void SetOperationChAttr(const char* sourcePath, char srcPathSeparator,
                            BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                            WORD attrAnd, WORD attrOr, int chAttrOfFiles, int chAttrOfDirs,
                            int unknownAttrs);

    // nastavi tento objekt pro operaci Copy/Move na server (pouziva se az po volani
    // SetConnection() a SetBasicData()); vraci TRUE pri uspechu
    // POZOR: nepouziva kritickou sekci pro pristup k datum (muze se volat jen pred
    //        pridanim operace do FTPOperationsList) + nesmi se volat opakovane (ocekava
    //        inicializovane hodnoty atributu objektu)
    BOOL SetOperationCopyMoveUpload(BOOL isCopy, const char* sourcePath, char srcPathSeparator,
                                    BOOL srcPathCanChange, BOOL srcPathCanChangeInclSubdirs,
                                    const char* targetPath, char tgtPathSeparator,
                                    BOOL tgtPathCanChange, BOOL tgtPathCanChangeInclSubdirs,
                                    const char* asciiFileMasks, int autodetectTrMode,
                                    int useAsciiTransferMode, int uploadCannotCreateFile,
                                    int uploadCannotCreateDir, int uploadFileAlreadyExists,
                                    int uploadDirAlreadyExists, int uploadRetryOnCreatedFile,
                                    int uploadRetryOnResumedFile, int uploadAsciiTrModeButBinFile);

    // nastavi Queue (v kriticke sekci)
    void SetQueue(CFTPQueue* queue);

    // vraci hodnotu Queue (v kriticke sekci)
    CFTPQueue* GetQueue();

    // alokuje noveho workera (predava mu operaci, frontu, host+user+port); pri nedostatku
    // pameti vraci NULL
    CFTPWorker* AllocNewWorker();

    // posle do logu s UID 'logUID' hlavicku operace (popis operace)
    void SendHeaderToLog(int logUID);

    // nastavi ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded (v kriticke sekci)
    void SetChildItems(int notDone, int skipped, int failed, int uiNeeded);

    // pricte dane pocty k pocitadlum ChildItemsNotDone+ChildItemsSkipped+ChildItemsFailed+ChildItemsUINeeded
    // (v kriticke sekci); zaroven muze zmenit stav operace (provadi se / hotova / nedobehla
    // kvuli chybam+skipum); 'onlyUINeededOrFailedToSkipped' je TRUE pokud jde jen o zmenu stavu
    // z sqisUserInputNeeded/sqisFailed na sqisSkipped (tato zmena urcite nevyzaduje refresh listingu - operace
    // zustava v "dokoncenem" stavu bez nutnosti refreshu)
    void AddToNotDoneSkippedFailed(int notDone, int skipped, int failed, int uiNeeded,
                                   BOOL onlyUINeededOrFailedToSkipped);

    // zjisti jestli jmeno odpovida hromadne masce ASCIIFileMasks; 'name'+'ext' jsou ukazatele
    // na jmeno a priponu (nebo konec jmena), oba jsou do jednoho bufferu; vraci TRUE pokud
    // jmeno odpovida hromadne masce
    BOOL IsASCIIFile(const char* name, const char* ext);

    // pricita/odcita 'size' k/od 'TotalSizeInBytes' (je-li 'sizeInBytes' TRUE) nebo k/od 'TotalSizeInBlocks'
    // (je-li 'sizeInBytes' FALSE); celkova velikost operace je soucet velikosti v bytech
    // a v blocich (jedna operace muze mit teoreticky velikosti jak v bytech, tak v blocich - kazdy
    // "adresar" totiz muze mit jiny format listingu, viz napr. MVS)
    void AddToTotalSize(const CQuadWord& size, BOOL sizeInBytes);
    void SubFromTotalSize(const CQuadWord& size, BOOL sizeInBytes);

    // nastavi OperationDlg (v kriticke sekci)
    void SetOperationDlg(COperationDlg* operDlg);

    // aktivuje nebo otevre dialog operace (viz OperationDlg); vraci uspech operace
    BOOL ActivateOperationDlg(HWND dropTargetWnd);

    // zavre dialog operace (je-li otevren) a vraci handle threadu, ve kterem byl otevren
    // posledni dialog (NULL pokud dialog otevren vubec nebyl)
    void CloseOperationDlg(HANDLE* dlgThread);

    // vola dialog operace z WM_INITDIALOG; 'dlg' je dialog (hodnota v OperationDlg nemusi
    // byt platna, pokud se jiz stihl zavolat CloseOperationDlg()); vraci uspech (FALSE =
    // fatal error -> zavrit dialog)
    BOOL InitOperDlg(COperationDlg* dlg);

    // prida do WorkersList noveho workera; vraci TRUE pri uspechu
    BOOL AddWorker(CFTPWorker* newWorker);

    // vola se v okamziku, kdy mohlo dojit k pausnuti nebo resumnuti nebo zastaveni operace:
    // po pridani workera nebo po resume workera, po stopnuti workera nebo po pausnuti
    // workera nebo po rozeslani "should-stop"; pokud doslo k resumnuti operace: nuluje
    // merak rychlosti a spousti mereni celkoveho casu operace; pokud doslo k pausnuti
    // operace nebo jejimu zastaveni (ceka se uz jen na ukonceni workera): zastavuje mereni
    // celkoveho casu operace
    void OperationStatusMaybeChanged();

    // viz CFTPWorkersList::InformWorkersAboutStop
    BOOL InformWorkersAboutStop(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // viz CFTPWorkersList::InformWorkersAboutPause
    BOOL InformWorkersAboutPause(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims, BOOL pause);

    // viz CFTPWorkersList::CanCloseWorkers
    BOOL CanCloseWorkers(int workerInd);

    // viz CFTPWorkersList::ForceCloseWorkers
    BOOL ForceCloseWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims);

    // viz CFTPWorkersList::DeleteWorkers
    BOOL DeleteWorkers(int workerInd, CFTPWorker** victims, int maxVictims, int* foundVictims,
                       CUploadWaitingWorker** uploadFirstWaitingWorker);

    // vraci TRUE pokud je znama IP adresa FTP/proxy serveru (vraci se v 'serverIP', 'host'+'hostBufSize'
    // se v tomto pripade ignoruje); vraci FALSE pokud IP adresa serveru zatim neni znama, v tomto
    // pripade vraci jmenou adresu FTP/proxy serveru v bufferu 'host' o velikosti 'hostBufSize'
    BOOL GetServerAddress(DWORD* serverIP, char* host, int hostBufSize);

    // nastavi (v krit. sekci) 'ServerIP'
    void SetServerIP(DWORD serverIP);

    // vraci IP FTP/proxy serveru v 'serverIP', port FTP/proxy serveru v 'port', jmenou adresu
    // FTP serveru v bufferu 'host' (min. velikost HOST_MAX_SIZE), typ proxy serveru v 'proxyType',
    // IP FTP serveru v 'hostIP' (pouziva se jen pro SOCKS4, jinak INADDR_NONE), port FTP serveru
    // v 'hostPort'; username a password pro proxy server v 'proxyUser' (min. velikost USER_MAX_SIZE)
    // a 'proxyPassword' (min. velikost PASSWORD_MAX_SIZE)
    void GetConnectInfo(DWORD* serverIP, unsigned short* port, char* host,
                        CFTPProxyServerType* proxyType, DWORD* hostIP, unsigned short* hostPort,
                        char* proxyUser, char* proxyPassword);

    // ulozi do bufferu 'buf' (o velikosti 'bufSize') hlasku do logu
    void GetConnectLogMsg(BOOL isReconnect, char* buf, int bufSize, int attemptNumber, const char* dateTime);

    // je-li ServerSystem==NULL, nastavi ServerSystem na retezec 'reply'
    // o delce 'replySize'
    void SetServerSystem(const char* reply, int replySize);

    // je-li ServerFirstReply==NULL, nastavi ServerFirstReply na retezec 'reply'
    // o delce 'replySize'
    void SetServerFirstReply(const char* reply, int replySize);

    // pripravi text dalsiho prikazu z proxy skriptu pro server a pro Log (jde o zapouzdrene volani
    // ProcessProxyScript); 'errDescrBuf' je buffer 300 znaku pro popis chyby; navratove hodnoty:
    // - chyba ve skriptu: funkce vraci FALSE + pozice chyby se vraci v '*proxyScriptExecPoint' + popis
    //   chyby je v 'errDescrBuf'
    // - chybejici hodnota promenne: funkce vraci TRUE a v 'needUserInput' take TRUE, popis
    //   chybejici promenne je v 'errDescrBuf' (hodnota '*proxyScriptExecPoint' se nemeni)
    // - uspesne zjisteni jaky prikaz poslat na server: vraci TRUE a v 'buf' je prikaz (vcetne
    //   CRLF na konci), v 'logBuf' je text do logu (heslo se nahrazuje slovem "(hidden)");
    //   '*proxyScriptExecPoint' ukazuje na zacatek dalsiho radku skriptu
    // - konec scriptu: vraci TRUE a 'buf' je prazdny retezec, '*proxyScriptExecPoint' ukazuje
    //   na konec skriptu
    BOOL PrepareNextScriptCmd(char* buf, int bufSize, char* logBuf, int logBufSize, int* cmdLen,
                              const char** proxyScriptExecPoint, int proxyScriptLastCmdReply,
                              char* errDescrBuf, BOOL* needUserInput);

    // vytvori strukturu CFTPProxyForDataCon (je NULL pokud je 'ProxyServer'==NULL), pri
    // nedostatku pameti vraci metoda FALSE
    BOOL AllocProxyForDataCon(CFTPProxyForDataCon** newDataConProxyServer);

    // vraci hodnotu RetryLoginWithoutAsking (v krit. sekci)
    BOOL GetRetryLoginWithoutAsking();

    // vraci v 'buf' o velikosti 'bufSize' obsah retezece InitFTPCommands (v krit. sekci)
    void GetInitFTPCommands(char* buf, int bufSize);

    // ziska info pro Login Error dialog (otevira se na "Solve Error" button v Connections listview)
    void GetLoginErrorDlgInfo(char* user, int userBufSize, char* password, int passwordBufSize,
                              char* account, int accountBufSize, BOOL* retryLoginWithoutAsking,
                              BOOL* proxyUsed, char* proxyUser, int proxyUserBufSize,
                              char* proxyPassword, int proxyPasswordBufSize);

    // ulozi nove hodnoty z Login Error dialogu (otevira se na "Solve Error" button v Connections listview)
    void SetLoginErrorDlgInfo(const char* password, const char* account, BOOL retryLoginWithoutAsking,
                              BOOL proxyUsed, const char* proxyUser, const char* proxyPassword);

    // vola se pro ohlaseni zmeny ve workerovi (hlasi se jen zmeny projevujici se zmenou
    // zobrazeni workera v listview Connections v dialogu operace - jde o promenne workera:
    // ShouldStop, State a CurItem); 'reportProgressChange' je TRUE pokud je zmena workera
    // spojena se zmenou progresu (statusu)
    void ReportWorkerChange(int workerID, BOOL reportProgressChange);

    // vola dialog operace po hlaseni zmeny pres ReportWorkerChange(), vraci ID zmeneneho workera
    // nebo -1 pokud se jich zmenilo vice; v 'reportProgressChange' (neni-li NULL) vraci TRUE pokud
    // slo o zmenu workera spojenou se zmenou progresu (statusu)
    int GetChangedWorker(BOOL* reportProgressChange);

    // jen vola CFTPWorkersList::PostNewWorkAvailable()
    // POZOR: vstupuje do sekce CSocketsThread::CritSect !!!
    void PostNewWorkAvailable(BOOL onlyOneItem);

    // jen vola CFTPWorkersList::GiveWorkToSleepingConWorker()
    // POZOR: vstupuje do sekce CSocketsThread::CritSect !!!
    BOOL GiveWorkToSleepingConWorker(CFTPWorker* sourceWorker);

    // vola se pro ohlaseni zmeny polozky (hlasi se jen zmeny projevujici se zmenou
    // zobrazeni polozky v listview Operations v dialogu operace - jde o promenne polozky:
    // Name, Path, Type, State, Size, SizeInBytes, TgtName, TgtPath, AsciiTransferMode,
    // Attr, ProblemID, WinError, ErrAllocDescr, OrigRights);
    // je-li 'itemUID' -1, hlasi se zmena vice polozek (dojde k prekresleni vsech polozek)
    void ReportItemChange(int itemUID);

    // vola dialog operace po hlaseni zmeny pres ReportItemChange(), vraci UID zmenenych polozek
    // (pri jedne zmene je v 'secondUID' -1) nebo dvojici (-1, -1) pokud se jich zmenilo vice nez dve
    void GetChangedItems(int* firstUID, int* secondUID);

    // plni defaultni hodnoty chovani diskovych operaci zadane pro operaci
    void GetDiskOperDefaults(CFTPDiskWork* diskWork);

    // vraci CannotCreateDir (v kriticke sekci)
    int GetCannotCreateDir();

    // vraci DirAlreadyExists (v kriticke sekci)
    int GetDirAlreadyExists();

    // vraci CannotCreateFile (v kriticke sekci)
    int GetCannotCreateFile();

    // vraci FileAlreadyExists (v kriticke sekci)
    int GetFileAlreadyExists();

    // vraci RetryOnCreatedFile (v kriticke sekci)
    int GetRetryOnCreatedFile();

    // vraci RetryOnResumedFile (v kriticke sekci)
    int GetRetryOnResumedFile();

    // vraci AsciiTrModeButBinFile (v kriticke sekci)
    int GetAsciiTrModeButBinFile();

    // vraci UploadCannotCreateDir (v kriticke sekci)
    int GetUploadCannotCreateDir();

    // vraci UploadDirAlreadyExists (v kriticke sekci)
    int GetUploadDirAlreadyExists();

    // vraci UploadCannotCreateFile (v kriticke sekci)
    int GetUploadCannotCreateFile();

    // vraci UploadFileAlreadyExists (v kriticke sekci)
    int GetUploadFileAlreadyExists();

    // vraci UploadRetryOnCreatedFile (v kriticke sekci)
    int GetUploadRetryOnCreatedFile();

    // vraci UploadRetryOnResumedFile (v kriticke sekci)
    int GetUploadRetryOnResumedFile();

    // vraci UploadAsciiTrModeButBinFile (v kriticke sekci)
    int GetUploadAsciiTrModeButBinFile();

    // vraci UnknownAttrs (v kriticke sekci)
    int GetUnknownAttrs();

    // vraci ResumeIsNotSupported (v kriticke sekci)
    BOOL GetResumeIsNotSupported();

    // vraci DataConWasOpenedForAppendCmd (v kriticke sekci)
    BOOL GetDataConWasOpenedForAppendCmd();

    BOOL GetEncryptControlConnection() { return EncryptControlConnection; }
    BOOL GetEncryptDataConnection() { return EncryptDataConnection; }
    int GetCompressData() { return CompressData; }
    CCertificate* GetCertificate(); // POZOR: vraci certifikat az po volani jeho AddRef(), tedy volajici je zodpovedny za uvolneni pomoci volani Release() certifikatu

    void SetEncryptDataConnection(BOOL encryptDataConnection) { EncryptDataConnection = encryptDataConnection; }
    void SetEncryptControlConnection(BOOL encryptControlConnection) { EncryptControlConnection = encryptControlConnection; }
    void SetCompressData(int compressData) { CompressData = compressData; }
    void SetCertificate(CCertificate* certificate);

    // nastavuje CannotCreateDir (v kriticke sekci)
    void SetCannotCreateDir(int value);

    // nastavuje DirAlreadyExists (v kriticke sekci)
    void SetDirAlreadyExists(int value);

    // nastavuje CannotCreateFile (v kriticke sekci)
    void SetCannotCreateFile(int value);

    // nastavuje FileAlreadyExists (v kriticke sekci)
    void SetFileAlreadyExists(int value);

    // nastavuje RetryOnCreatedFile (v kriticke sekci)
    void SetRetryOnCreatedFile(int value);

    // nastavuje RetryOnResumedFile (v kriticke sekci)
    void SetRetryOnResumedFile(int value);

    // nastavuje AsciiTrModeButBinFile (v kriticke sekci)
    void SetAsciiTrModeButBinFile(int value);

    // nastavuje UploadCannotCreateDir (v kriticke sekci)
    void SetUploadCannotCreateDir(int value);

    // nastavuje UploadDirAlreadyExists (v kriticke sekci)
    void SetUploadDirAlreadyExists(int value);

    // nastavuje UploadCannotCreateFile (v kriticke sekci)
    void SetUploadCannotCreateFile(int value);

    // nastavuje UploadFileAlreadyExists (v kriticke sekci)
    void SetUploadFileAlreadyExists(int value);

    // nastavuje UploadRetryOnCreatedFile (v kriticke sekci)
    void SetUploadRetryOnCreatedFile(int value);

    // nastavuje UploadRetryOnResumedFile (v kriticke sekci)
    void SetUploadRetryOnResumedFile(int value);

    // nastavuje UploadAsciiTrModeButBinFile (v kriticke sekci)
    void SetUploadAsciiTrModeButBinFile(int value);

    // nastavuje UnknownAttrs (v kriticke sekci)
    void SetUnknownAttrs(int value);

    // nastavuje ResumeIsNotSupported (v kriticke sekci)
    void SetResumeIsNotSupported(BOOL value);

    // nastavuje DataConWasOpenedForAppendCmd (v kriticke sekci)
    void SetDataConWasOpenedForAppendCmd(BOOL value);

    // zjisti typ cesty na FTP serveru (vola v kriticke sekci ::GetFTPServerPathType()
    // s parametry 'ServerSystem' a 'ServerFirstReply')
    CFTPServerPathType GetFTPServerPathType(const char* path);

    // zjisti jestli 'ServerSystem' obsahuje jmeno 'systemName'
    BOOL IsServerSystem(const char* systemName);

    // vraci TRUE pokud cesta 'path' uz je v seznamu prozkoumanych cest (viz 'ExploredPaths');
    // POZOR: vychazi z predpokladu, ze 'path' je cesta vracena serverem, takze se cesty
    // porovnavaji jen jako case-sensitive retezce (neorezavaji se slashe/backslashe/tecky,
    // atd.) - k detekci cyklu muze dojit az pri jeho druhem pruchodu, pro nase ucely to staci
    BOOL IsAlreadyExploredPath(const char* path);

    // ulozi cestu 'path' do seznamu prozkoumanych cest (viz 'ExploredPaths');
    // vraci FALSE pokud 'path' uz je v tomto seznamu (nedostatek pameti se v teto
    // funkci ignoruje)
    // POZOR: vychazi z predpokladu, ze 'path' je cesta vracena serverem, takze se cesty
    // porovnavaji jen jako case-sensitive retezce (neorezavaji se slashe/backslashe/tecky,
    // atd.) - k detekci cyklu muze dojit az pri jeho druhem pruchodu, pro nase ucely to staci
    BOOL AddToExploredPaths(const char* path);

    // vraci UsePassiveMode (v krit. sekci)
    BOOL GetUsePassiveMode();

    // nastavuje UsePassiveMode (v krit. sekci)
    void SetUsePassiveMode(BOOL usePassiveMode);

    // vraci SizeCmdIsSupported (v krit. sekci)
    BOOL GetSizeCmdIsSupported();

    // nastavuje SizeCmdIsSupported (v krit. sekci)
    void SetSizeCmdIsSupported(BOOL sizeCmdIsSupported);

    // vraci prikaz pro listovani (je-li ListCommand==NULL, vraci "LIST\r\n"), prikaz uz ma
    // na konci CRLF; 'buf' (nesmi byt NULL) je buffer o velikosti 'bufSize'
    void GetListCommand(char* buf, int bufSize);

    // vraci UseListingsCache (v krit. sekci)
    BOOL GetUseListingsCache();

    // vraci User (v krit. sekci); je-li User==NULL, vraci se FTP_ANONYMOUS
    void GetUser(char* buf, int bufSize);

    // vraci (v krit. sekci) naalokovany retezec s odpovedi serveru na prikaz SYST
    // (atribut 'ServerSystemReply'); pri chybe alokace vraci NULL
    char* AllocServerSystemReply();

    // vraci (v krit. sekci) naalokovany retezec s prvni odpovedi serveru
    // (atribut 'ServerFirstReply'); pri chybe alokace vraci NULL
    char* AllocServerFirstReply();

    // vraci (v krit. sekci) typ serveru pro parsovani listingu: vraci FALSE pokud se
    // ma provest autodetekce typu serveru (v 'buf' vraci prazdny retezec), pokud vrati
    // TRUE, je v 'buf' (o velikosti aspon SERVERTYPE_MAX_SIZE) jmeno pozadovaneho
    // typu serveru
    BOOL GetListingServerType(char* buf);

    // vraci (v krit. sekci) rezim prenosu zrekonstruovany z AutodetectTrMode
    // a UseAsciiTransferMode
    int GetTransferMode();

    // vraci (v krit. sekci) parametry operace Change Attributes
    void GetParamsForChAttrsOper(BOOL* selFiles, BOOL* selDirs, BOOL* includeSubdirs,
                                 DWORD* attrAndMask, DWORD* attrOrMask,
                                 int* operationsUnknownAttrs);

    // zvysi pocitadla v item-dir polozce 'itemDir' (polozka musi byt potomek CFTPQueueItemDir)
    // nebo v teto operaci (je-li 'itemDir' -1); muze zmenit stav item-dir polozky nebo operace;
    // pokud meni stav item-dir polozky, zmeni pocitadla v jejim parentovi (funguje rekurzivne,
    // takze zajisti vsechny potrebne zmeny stavu a pocitadel); 'onlyUINeededOrFailedToSkipped'
    // je TRUE pokud jde jen o zmenu stavu z sqisUserInputNeeded/sqisFailed na sqisSkipped
    // (tato zmena urcite nevyzaduje refresh listingu - operace zustava v "dokoncenem" stavu
    // bez nutnosti refreshu)
    // POZOR: pracuje v kritickych sekcich CFTPOperation::OperCritSect a CFTPQueue::QueueCritSect
    void AddToItemOrOperationCounters(int itemDir, int childItemsNotDone,
                                      int childItemsSkipped, int childItemsFailed,
                                      int childItemsUINeeded, BOOL onlyUINeededOrFailedToSkipped);

    // vraci (v krit. sekci) parametry operace Delete; ignoruje NULLove parametry
    void GetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                int* confirmDelOnHiddenDir);

    // nastavuje (v krit. sekci) parametry operace Delete; ignoruje (nenastavuje) NULLove parametry
    void SetParamsForDeleteOper(int* confirmDelOnNonEmptyDir, int* confirmDelOnHiddenFile,
                                int* confirmDelOnHiddenDir);

    // vola se pro ohlaseni zmeny stavu operace
    void ReportOperationStateChange();

    // ziskani stavu operace; je-li 'calledFromSetupCloseButton' TRUE, jde o volani z metody
    // SetupCloseButton (z dialogu operace), jde o reakci na postnutou notifikaci o zmene stavu
    // operace - v tomto pripade se povoli postnuti dalsi notifikace
    COperationState GetOperationState(BOOL calledFromSetupCloseButton);

    // jen pro debugovaci ucely: vraci stavy pocitadel (v kriticke sekci)
    void DebugGetCounters(int* childItemsNotDone, int* childItemsSkipped,
                          int* childItemsFailed, int* childItemsUINeeded);

    // rozesle po Salamanderovi zpravy o zmenach na cestach (zdrojove a pripadne i cilove);
    // vola se az po dokonceni nebo preruseni operace, aby nedochazelo ke "zbytecnym"
    // refreshum (brzdilo by operaci); 'softRefresh' je TRUE pokud se nebudou zavirat
    // spojeni workeru (nedojde ke vraceni spojeni do panelu, tedy refresh nechame udelat
    // az kdyz se user prepne zpet do hlavniho okna Salamandera - okamzity refresh by
    // nutne vyvolal dotaz na reconnect, coz zbytecne prudi)
    void PostChangeOnPathNotifications(BOOL softRefresh);

    // vraci globalni speed-meter operace (mereni celkove rychlosti data-connection vsech workeru)
    CTransferSpeedMeter* GetGlobalTransferSpeedMeter() { return &GlobalTransferSpeedMeter; }

    // vraci globalni objekt pro ukladani casu posledni aktivity na data-connectionach vsech workeru
    CSynchronizedDWORD* GetGlobalLastActivityTime() { return &GlobalLastActivityTime; }

    // vraci (v krit. sekci) typ operace (viz 'Type')
    CFTPOperationType GetOperationType();

    // vraci progres na zaklade souctu velikosti hotovych a skipnutych souboru v bytech a
    // celkove velikosti v bytech (jinak vraci -1); v 'unknownSizeCount' vraci pocet
    // nedokoncenych operaci s neznamou velikosti v bytech; v 'errorsCount' vraci pocet
    // polozek s chybou nebo dotazem; ve 'waiting' vraci soucet velikosti (v bytech)
    // polozek, ktere cekaji na zpracovani (waiting+delayed+processing)
    int GetCopyProgress(CQuadWord* downloaded, CQuadWord* total, CQuadWord* waiting,
                        int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                        int* totalCount, CFTPQueue* queue);

    // vraci progres na zaklade souctu velikosti hotovych a skipnutych souboru v bytech a
    // celkove velikosti v bytech (jinak vraci -1); v 'unknownSizeCount' vraci pocet
    // nedokoncenych operaci s neznamou velikosti v bytech; v 'errorsCount' vraci pocet
    // polozek s chybou nebo dotazem; ve 'waiting' vraci soucet velikosti (v bytech)
    // polozek, ktere cekaji na zpracovani (waiting+delayed+processing)
    int GetCopyUploadProgress(CQuadWord* uploaded, CQuadWord* total, CQuadWord* waiting,
                              int* unknownSizeCount, int* errorsCount, int* doneOrSkippedCount,
                              int* totalCount, CFTPQueue* queue);

    // prida odpovidajici si dvojici velikosti v bytech a blocich; udaje se pouzivaji pro vypocet
    // priblizne velikosti bloku (u VMS, MVS a dalsich serveru pouzivajicich bloky se pouziva pro
    // odhad celkove velikosti downloadu v bytech na zaklade zname velikosti v blocich)
    void AddBlkSizeInfo(CQuadWord const& sizeInBytes, CQuadWord const& sizeInBlocks);

    // vypocet priblizne velikosti v bytech na zaklade velikosti v blocich; vraci FALSE pokud
    // jeste neni znama velikost bloku a neni tudiz mozne provest prepocet na byty
    BOOL GetApproxByteSize(CQuadWord* sizeInBytes, CQuadWord const& sizeInBlocks);

    // vraci TRUE pokud je znama priblizna velikost bloku v bytech
    BOOL IsBlkSizeKnown();

    // vraci jak dlouho operace bezi/bezela
    DWORD GetElapsedSeconds();

    // vraci TRUE pokud behem poslednich WORKER_STATUSUPDATETIMEOUT milisekund doslo
    // k nejake aktivite na data-connectionach workeru (tyka se listovani i downloadu)
    BOOL GetDataActivityInLastPeriod();

    // v 'buf' o velikosti 'bufSize' vraci TargetPath
    void GetTargetPath(char* buf, int bufSize);

    // navysi LastErrorOccurenceTime o jednu a vraci novou hodnotu LastErrorOccurenceTime
    DWORD GiveLastErrorOccurenceTime();

    // hleda index workera, ktery potrebuje otevrit Solve Error dialog (objevila se
    // v nem "nova" (user ji jeste nevidel) chyba); vraci TRUE pokud se takoveho workera
    // podarilo nalezt, jeho index se vraci v 'index'
    BOOL SearchWorkerWithNewError(int* index);

    // zjisti jestli tato operace muze provest zmeny na ceste 'path' typu 'pathType' na
    // serveru 'user'+'host'+'port'; 'userLength' je nula pokud netusime, jak dlouhe je
    // uzivatelske jmeno nebo pokud v nem nejsou "zakazane" znaky, jinak je to ocekavana
    // delka uzivatelskeho jmena
    BOOL CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                              const char* path, CFTPServerPathType pathType,
                              int userLength);

    // zjisti jestli je mezi operacemi upload na server 'user'+'host'+'port';
    // 'user' je NULL pokud jde o anonymous pripojeni; 'userLength' je nula pokud
    // netusime, jak dlouhe je uzivatelske jmeno nebo pokud v nem nejsou "zakazane"
    // znaky, jinak je to ocekavana delka uzivatelskeho jmena
    BOOL IsUploadingToServer(const char* user, const char* host, unsigned short port,
                             int userLength);

    // vraci 'Host'+'User'+'Port'; 'host' (neni-li NULL) je buffer pro 'Host' o velikosti
    // HOST_MAX_SIZE; 'user' (neni-li NULL) je buffer pro 'User' o velikosti USER_MAX_SIZE;
    // v 'port' (neni-li NULL) vraci 'Port'
    void GetUserHostPort(char* user, char* host, unsigned short* port);
};

//
// ****************************************************************************
// CFTPOperationsList
//

enum CWorkerWaitSatisfiedReason // udalosti hlidane v CFTPOperationsList::WaitForFinishOrESC
{
    wwsrEsc,                 // ESC
    wwsrTimeout,             // timeout
    wwsrWorkerSocketClosure, // doslo k zavreni socketu nejakeho workera
};

class CFTPOperationsList
{
protected:
    // kriticka sekce pro pristup k datum objektu
    // POZOR: pristup do kritickych sekci konzultovat v souboru servers\critsect.txt !!!
    CRITICAL_SECTION OpListCritSect;

    // seznam existujicich operaci; UID operace je index do tohoto pole,
    // takze: PRVKY POLE NELZE POSOUVAT (vymaz se resi prepisem indexu na NULL)
    TIndirectArray<CFTPOperation> Operations;
    int FirstFreeIndexInOperations; // nejnizsi volny index uvnitr pole Operations (-1 = zadny)

    BOOL CallOK; // pomocna promenna pro StartCall() a EndCall()

public:
    CFTPOperationsList();
    ~CFTPOperationsList();

    // vraci TRUE pokud v seznamu neni zadna operace
    BOOL IsEmpty();

    // prida do pole novou operaci (pracuje s FirstFreeIndexInOperations); v 'newuid' (neni-li NULL)
    // vraci UID pridane operace; vraci TRUE pri uspechu
    BOOL AddOperation(CFTPOperation* newOper, int* newuid);

    // zavre vsechny otevrene dialogy operaci a pocka na dokonceni vsech threadu dialogu
    void CloseAllOperationDlgs();

    // ukonci a zrusi vybrane workery operaci; 'parent' je "foreground" okno threadu (po
    // stisku ESC se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne
    // napriklad v jine aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent()
    // nebo pluginem otevreny dialog); je-li 'operUID' -1, pracuje se vsemi workery vsech
    // operaci; neni-li 'operUID' -1 a je-li 'workerInd' -1, prauje se vsemi workery
    // operace s UID 'operUID'; neni-li 'operUID' ani 'workerInd' -1, pracuje s workerem
    // na indexu 'workerInd' operace s UID 'operUID'
    // POZOR: vstupuje do sekci CSocketsThread::CritSect a CSocket::SocketCritSect !!!
    void StopWorkers(HWND parent, int operUID, int workerInd);

    // pausne nebo resumne vybrane workery operaci; 'parent' je "foreground" okno threadu (po
    // stisku ESC se pouziva pro zjisteni jestli byl stisknut ESC v tomto okne a ne
    // napriklad v jine aplikaci; u hl. threadu jde o SalamanderGeneral->GetMsgBoxParent()
    // nebo pluginem otevreny dialog); je-li 'operUID' -1, pracuje se vsemi workery vsech
    // operaci; neni-li 'operUID' -1 a je-li 'workerInd' -1, prauje se vsemi workery
    // operace s UID 'operUID'; neni-li 'operUID' ani 'workerInd' -1, pracuje s workerem
    // na indexu 'workerInd' operace s UID 'operUID'; je-li 'pause' TRUE maji se vybrani
    // workeri pausnout, jinak se maji resumnout
    // POZOR: vstupuje do sekci CSocketsThread::CritSect a CSocket::SocketCritSect !!!
    void PauseWorkers(HWND parent, int operUID, int workerInd, BOOL pause);

    // smaze z pole operaci s UID 'uid' (nastavuje FirstFreeIndexInOperations);
    // predem nutne zavrit dialog operace a zrusit vsechny workery operace;
    // je-li 'doNotPostChangeOnPathNotifications' TRUE, nevola metoda
    // PostChangeOnPathNotifications (FALSE jen v pripade Cancelu operace);
    // POZOR: vstupuje take do sekce CUploadListingCache::UploadLstCacheCritSect !!!
    void DeleteOperation(int uid, BOOL doNotPostChangeOnPathNotifications);

    // zjisti jestli nektera z operaci muze provest zmeny na ceste 'path' typu 'pathType' na
    // serveru 'user'+'host'+'port'; neni-li 'ignoreOperUID' -1, jde o UID operace, kterou
    // ma ignorovat (jde o upload operaci, kterou jsme prave zalozili)
    BOOL CanMakeChangesOnPath(const char* user, const char* host, unsigned short port,
                              const char* path, CFTPServerPathType pathType, int ignoreOperUID);

    // zjisti jestli je mezi operacemi upload na server 'user'+'host'+'port'
    BOOL IsUploadingToServer(const char* user, const char* host, unsigned short port);

    // ***************************************************************************************
    // pomocne metody pro volani metod objektu typu CFTPOperation; objekty jsou identifikovane
    // pres sva UID zadana v prvnim parametru pomocnych metod ('operUID'); pomocne metody
    // vraci TRUE pokud doslo k volani metody objektu ('operUID' je platne UID operace)
    // ***************************************************************************************

    // jen pomocne metody pro snazsi implementaci nasledujiciho oddilu metod
    BOOL StartCall(int operUID);
    BOOL EndCall();

    BOOL ActivateOperationDlg(int operUID, BOOL& success, HWND dropTargetWnd);
    BOOL CloseOperationDlg(int operUID, HANDLE* dlgThread);

protected:
    // ceka na zavreni socketu nejakeho workeru (navic: monitoruje ESC, ma timeout a obsahuje
    // message-loop); 'parent' je "foreground" okno threadu (po stisku ESC se pouziva pro zjisteni
    // jestli byl stisknut ESC v tomto okne a ne napriklad v jine aplikaci; u hl. threadu jde
    // o SalamanderGeneral->GetMsgBoxParent() nebo pluginem otevreny dialog); 'milliseconds' je
    // cas v ms, po ktery se ma cekat na zavreni socketu, po teto dobe vraci metoda
    // 'reason'==wwsrTimeout, je-li 'milliseconds' INFINITE, ma se cekat bez casoveho
    // omezeni; neni-li 'waitWnd' NULL, sleduje stisk close buttonu ve wait-okenku 'waitWnd';
    // v 'reason' se vraci duvod ukonceni teto metody (jeden z wwsrXXX; wwsrEsc pokud user
    // stiskl ESC); pokud prijde Windows message WM_CLOSE, nastavi se do 'postWM_CLOSE' TRUE
    // a dojde k aktivaci okna 'parent' (aby user videl na co ceka); 'lastWorkerMayBeClosedState'
    // je pomocna promenna, pred cyklem inicializovat na -1, pro volani v cyklu nemenit
    // mozne volat z libovolneho threadu
    void WaitForFinishOrESC(HWND parent, int milliseconds, CWaitWindow* waitWnd,
                            CWorkerWaitSatisfiedReason& reason, BOOL& postWM_CLOSE,
                            int& lastWorkerMayBeClosedState);
};

//
// ****************************************************************************
// CUIDArray
//

class CUIDArray
{
protected:
    // kriticka sekce pro pristup k datum objektu
    CRITICAL_SECTION UIDListCritSect;
    TDirectArray<DWORD> Data;

public:
    CUIDArray(int base, int delta);
    ~CUIDArray();

    // pridani UID do pole, vraci uspech
    BOOL Add(int uid);

    // vraci prvni prvek z pole v 'uid'; vraci uspech
    BOOL GetFirstUID(int& uid);
};

//
// ****************************************************************************
// CReturningConnections
//

struct CReturningConnectionData
{
    int ControlConUID;         // do jake "control connection" v panelu spojeni vracime
    CFTPWorker* WorkerWithCon; // worker s vracenym spojenim

    CReturningConnectionData(int controlConUID, CFTPWorker* workerWithCon)
    {
        ControlConUID = controlConUID;
        WorkerWithCon = workerWithCon;
    }
};

class CReturningConnections
{
protected:
    // kriticka sekce pro pristup k datum objektu
    CRITICAL_SECTION RetConsCritSect;
    TIndirectArray<CReturningConnectionData> Data;

public:
    CReturningConnections(int base, int delta);
    ~CReturningConnections();

    // pridani prvku do pole, vraci uspech
    BOOL Add(int controlConUID, CFTPWorker* workerWithCon);

    // vraci prvni prvek z pole (chyba: vraci -1 a NULL); vraci uspech
    BOOL GetFirstCon(int* controlConUID, CFTPWorker** workerWithCon);

    // zrusi vsechny workery, kteri zatim nestihli predat connectionu do panelu (jsou v poli Data)
    // volat jen bez vnoreni do zadnych krit. sekci (vstupuje do CSocketsThread::CritSect)
    void CloseData();
};

//
// ****************************************************************************
// CUploadListingCache
//
// cache listingu cest na serverech - pouziva se pri uploadu pro zjistovani,
// jestli cilovy soubor/adresar jiz existuje

enum CUploadListingItemType
{
    ulitFile,      // soubor
    ulitDirectory, // adresar
    ulitLink,      // link (na adresar nebo soubor)
};

#define UPLOADSIZE_UNKNOWN CQuadWord(-1, -1)    // neznama velikost (na listingu neni velikost v bytech)
#define UPLOADSIZE_NEEDUPDATE CQuadWord(-2, -1) // pro zjisteni velikosti je potreba opatrit novy listing (napr. behem uploadu nebo po uploadu v ASCII rezimu)

struct CUploadListingItem // data jednoho souboru/adresare/linku v listingu
{
    // pristup k datum objektu v krit. sekci CUploadListingCache::UploadLstCacheCritSect
    // POZNAMKA: pred pridanim do CUploadListingCache je pristup bez krit. sekce

    CUploadListingItemType ItemType; // soubor/adresar/link
    char* Name;                      // jmeno polozky
    CQuadWord ByteSize;              // jen pro soubory: velikost polozky v bytech, specialni hodnoty viz UPLOADSIZE_XXX
};

enum CUploadListingChangeType
{
    ulctDelete,       // mazani jmena
    ulctCreateDir,    // vytvoreni adresare
    ulctStoreFile,    // zacatek uploadu souboru (muze byt i prepis souboru/linku)
    ulctFileUploaded, // dokonceni uploadu souboru (mohl byt i prepis souboru/linku)
};

struct CUploadListingChange // zmena v listingu (aby se po kazde zmene netahal znovu listing, provadime po kazdem FTP prikazu ocekavanou zmenu listingu nad cachovanym listingem)
{
    CUploadListingChange* NextChange; // dalsi zmena v listingu (vyuziva se behem stahovani listingu - zmeny se provedou az po stazeni listingu)
    CUploadListingChangeType Type;    // typ zmeny
    DWORD ChangeTime;                 // IncListingCounter() z okamziku zmeny (okamzik prijeti odpovedi serveru na prikaz menici listing)

    char* Name;         // ulctDelete: jmeno mazaneho souboru/linku/adresare; ulctCreateDir: jmeno vytvareneho adresare; ulctStoreFile+ulctFileUploaded: jmeno uploadeneho souboru
    CQuadWord FileSize; // ulctFileUploaded: velikost uploadleho souboru

    CUploadListingChange(DWORD changeTime, CUploadListingChangeType type, const char* name,
                         const CQuadWord* fileSize = NULL);
    ~CUploadListingChange(); // uvolneni dat, ale POZOR: nesmi uvolnit NextChange
    BOOL IsGood() { return Name != NULL; }
};

struct CUploadWaitingWorker // seznam workeru cekajicich na dokonceni (nebo chybu) listingu cesty
{
    CUploadWaitingWorker* NextWorker;
    int WorkerMsg;
    int WorkerUID;
};

enum CUploadListingState
{
    ulsReady,                      // listing je pripraveny k pouziti (z pohledu Salamandera je aktualni)
    ulsInProgress,                 // prave ho nejaky worker stahuje ze serveru
    ulsInProgressButObsolete,      // prave ho nejaky worker stahuje ze serveru, ale uz mame z jineho zdroje (panel) listing ve stavu ulsReady (po vyignorovani vysledku z workeru se nahodi stav ulsReady)
    ulsInProgressButMayBeOutdated, // prave ho nejaky worker stahuje ze serveru, ale uz vime, ze listing asi nebude aktualni (v adresari probehla neznama zmena nebo se behem zmeny rozpadlo spojeni)
    ulsNotAccessible,              // listing je "neziskatelny" (napr. server hlasi "access denied" nebo listing nejde rozparsovat)
};

struct CUploadPathListing // listing pro jednu konkretni cestu na serveru
{
public:
    // pristup k datum objektu v krit. sekci CUploadListingCache::UploadLstCacheCritSect
    // POZNAMKA: pred pridanim do CUploadListingCache je pristup bez krit. sekce

    char* Path;                  // cachovana cesta (lokalni na serveru)
    CFTPServerPathType PathType; // typ cachovane cesty

    CUploadListingState ListingState;         // stav listingu
    DWORD ListingStartTime;                   // IncListingCounter() z okamziku poslani prikazu LIST na server (zahajeni listovani)
    CUploadListingChange* FirstChange;        // jen v ulsInProgress: prvni zmena v listingu (zmeny se ukladaji jen behem stahovani listingu nekterym workerem)
    CUploadListingChange* LastChange;         // jen v ulsInProgress: posledni zmena v listingu (za tuto se ulozi dalsi zmena)
    DWORD LatestChangeTime;                   // IncListingCounter() z okamziku posledni zmeny listingu (pouziva se pri zjistovani jestli je mozne listing updatnout novym listingem - jen pokud je LatestChangeTime mensi nez cas zahajeni listovani noveho listingu)
    CUploadWaitingWorker* FirstWaitingWorker; // jen ve stavech ulsInProgress*: seznam workeru cekajicich na dokonceni (nebo chybu) listingu cesty
    BOOL FromPanel;                           // TRUE = listing prevzaty z panelu (muze byt stary, pri pochybnosti o aktualnosti listingu se provede jeho refresh)

    TIndirectArray<CUploadListingItem> ListingItem; // pole polozek listingu

public:
    CUploadPathListing(const char* path, CFTPServerPathType pathType,
                       CUploadListingState listingState, DWORD listingStartTime,
                       BOOL fromPanel);
    ~CUploadPathListing();
    BOOL IsGood() { return Path != NULL; }

    // uvolni listing z ListingItem
    void ClearListingItems();

    // uvolni seznam zmen (viz FirstChange, LastChange)
    void ClearListingChanges();

    // najde polozku s CUploadListingItem::Name == 'name'; vraci TRUE pri uspechu a 'index'
    // je index nalezene polozky; vraci FALSE pri neuspechu a 'index' je misto, kam zaradit
    // pripadnou novou polozku s CUploadListingItem::Name == 'name'
    BOOL FindItem(const char* name, int& index);

    // parsuje listing 'pathListing'+'pathListingLen'+'pathListingDate'; 'welcomeReply' (nesmi
    // byt NULL) je prvni odpoved serveru (obsahuje casto verzi FTP serveru);
    // 'systReply' (nesmi byt NULL) je system serveru (odpoved na prikaz SYST);
    // 'suggestedListingServerType' je typ serveru pro parsovani listingu: NULL = autodetekce,
    // jinak jmeno typu serveru (bez prip. uvodni '*', pokud prestane existovat, prepne se
    // na autodetekci); neni-li 'lowMemory' NULL, vraci se v nem TRUE pokud je nedostatek
    // pameti; vraci TRUE pokud se podarilo rozparsovat cely listing a objekt je
    // naplneny novymi polozkami
    BOOL ParseListing(const char* pathListing, int pathListingLen, const CFTPDate& pathListingDate,
                      CFTPServerPathType pathType, const char* welcomeReply, const char* systReply,
                      const char* suggestedListingServerType, BOOL* lowMemory);

    // hlaseni zmeny: vytvoreni adresare; 'newDir' je uz jen jedno jmeno adresare;
    // v 'lowMem' (nesmi byt NULL) vraci TRUE pri nedostatku pameti (dojde k zneplatneni
    // tohoto listingu); v 'dirCreated' (nesmi byt NULL) vraci TRUE pokud v listingu
    // adresar 'newDir' neexistoval a byl uspesne pridan
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportCreateDirs()
    void ReportCreateDir(const char* newDir, BOOL* dirCreated, BOOL* lowMem);

    // popis viz CUploadListingCache::ReportDelete(); v 'invalidateNameDir' (nesmi byt NULL)
    // se vraci TRUE pokud je potreba adresar 'name' zneplatnit (muze jit o adresar nebo
    // link na adresar); v 'lowMem' (nesmi byt NULL) vraci TRUE pri nedostatku pameti pro
    // pridani zmeny do fronty zmen (jen ve stavu ulsInProgress)
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportDelete()
    void ReportDelete(const char* name, BOOL* invalidateNameDir, BOOL* lowMem);

    // popis viz CUploadListingCache::ReportStoreFile(); v 'lowMem' (nesmi byt NULL)
    // vraci TRUE pri nedostatku pameti (dojde k zneplatneni tohoto listingu)
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportStoreFile()
    void ReportStoreFile(const char* name, BOOL* lowMem);

    // popis viz CUploadListingCache::ReportFileUploaded(); v 'lowMem' (nesmi byt NULL)
    // vraci TRUE pri nedostatku pameti pro pridani zmeny do fronty zmen (jen ve stavu
    // ulsInProgress)
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportFileUploaded()
    void ReportFileUploaded(const char* name, const CQuadWord& fileSize, BOOL* lowMem);

    // provede nad listingem zmenu 'change'; vraci uspech (neuspech = nutne zrusit cely listing)
    BOOL CommitChange(CUploadListingChange* change);

    // prida workera 'workerMsg'+'workerUID' do fronty workeru, kteri cekaji na dokonceni
    // (nebo chybu) listovani teto cesty; vraci FALSE jen pri nedostatku pameti
    BOOL AddWaitingWorker(int workerMsg, int workerUID);

    // vsem workerum ze seznamu FirstWaitingWorker: je-li 'uploadFirstWaitingWorker' NULL,
    // dojde k rozeslani zprav WORKER_TGTPATHLISTINGFINISHED, jinak je 'uploadFirstWaitingWorker'
    // seznam, kam se maji tyto workeri pridat
    // POZOR: je-li 'uploadFirstWaitingWorker' NULL, musi se volat v sekci CSocketsThread::CritSect!
    void InformWaitingWorkers(CUploadWaitingWorker** uploadFirstWaitingWorker);

private:
    // prida polozku na konec pole; POZOR: pred hledanim v poli nutne volat SortItems()
    BOOL AddItemDoNotSort(CUploadListingItemType itemType, const char* name, const CQuadWord& byteSize);

    // seradi pole podle CUploadListingItem::Name
    void SortItems();

    // jen pomocna metoda pro ParseListing()
    BOOL ParseListingToArray(const char* pathListing, int pathListingLen, const CFTPDate& pathListingDate,
                             CServerType* serverType, BOOL* lowMem, BOOL isVMS);

    // pridani 'ch' do seznamu zmen
    void AddChange(CUploadListingChange* ch);

    // vlozi polozku na index 'index'
    BOOL InsertNewItem(int index, CUploadListingItemType itemType, const char* name,
                       const CQuadWord& byteSize);
};

struct CUploadListingsOnServer // listingy cest na jednom serveru
{
public:
    // pristup k datum objektu v krit. sekci CUploadListingCache::UploadLstCacheCritSect
    // POZNAMKA: pred pridanim do CUploadListingCache je pristup bez krit. sekce

    char* User;          // user-name, NULL==anonymous
    char* Host;          // host-address (nesmi byt NULL)
    unsigned short Port; // port na kterem bezi FTP server

    TIndirectArray<CUploadPathListing> Listing; // pole listingu cest na serveru

#ifdef _DEBUG
    static int FoundPathIndexesInCache; // kolik hledanych cest chytla cache
    static int FoundPathIndexesTotal;   // kolik bylo celkem hledanych cest
#endif

protected:
#define FOUND_PATH_IND_CACHE_SIZE 5 // musi byt aspon 1
    int FoundPathIndexes[FOUND_PATH_IND_CACHE_SIZE];

public:
    CUploadListingsOnServer(const char* user, const char* host, unsigned short port);
    ~CUploadListingsOnServer();
    BOOL IsGood() { return Host != NULL; }

    // popis viz CUploadListingCache::AddOrUpdateListing();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::AddOrUpdateListing()
    BOOL AddOrUpdateListing(const char* path, CFTPServerPathType pathType,
                            const char* pathListing, int pathListingLen,
                            const CFTPDate& pathListingDate, DWORD listingStartTime,
                            BOOL onlyUpdate, const char* welcomeReply,
                            const char* systReply, const char* suggestedListingServerType);

    // popis viz CUploadListingCache::RemoveNotAccessibleListings();
    void RemoveNotAccessibleListings();

    // popis viz CUploadListingCache::InvalidatePathListing
    void InvalidatePathListing(const char* path, CFTPServerPathType pathType);

    // popis viz CUploadListingCache::IsListingFromPanel
    BOOL IsListingFromPanel(const char* path, CFTPServerPathType pathType);

    // prida prazdny listing cesty 'path'+'dirName' typu 'pathType' ve stavu 'listingState';
    // 'dirName' muze byt i NULL (prida se listing cesty 'path'); 'doNotCheckIfPathIsKnown'
    // je TRUE, pokud vime, ze cesta neni v poli 'Listings'; vraci ukazatel na novy
    // listing pri uspechu, jinak vraci NULL
    CUploadPathListing* AddEmptyListing(const char* path, const char* dirName,
                                        CFTPServerPathType pathType,
                                        CUploadListingState listingState,
                                        BOOL doNotCheckIfPathIsKnown);

    // najde polozku s CUploadPathListing::Path == 'path'; vraci TRUE pri uspechu a 'index'
    // je index nalezene polozky; vraci FALSE pri neuspechu
    BOOL FindPath(const char* path, CFTPServerPathType pathType, int& index);

    // popis viz CUploadListingCache::ReportCreateDirs();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportCreateDirs()
    void ReportCreateDirs(const char* workPath, CFTPServerPathType pathType, const char* newDirs,
                          BOOL unknownResult);

    // popis viz CUploadListingCache::ReportRename();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportRename()
    void ReportRename(const char* workPath, CFTPServerPathType pathType,
                      const char* fromName, const char* newName, BOOL unknownResult);

    // popis viz CUploadListingCache::ReportDelete();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportDelete()
    void ReportDelete(const char* workPath, CFTPServerPathType pathType, const char* name,
                      BOOL unknownResult);

    // popis viz CUploadListingCache::ReportStoreFile();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportStoreFile()
    void ReportStoreFile(const char* workPath, CFTPServerPathType pathType, const char* name);

    // popis viz CUploadListingCache::ReportFileUploaded();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportFileUploaded()
    void ReportFileUploaded(const char* workPath, CFTPServerPathType pathType, const char* name,
                            const CQuadWord& fileSize, BOOL unknownResult);

    // popis viz CUploadListingCache::ReportUnknownChange();
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ReportUnknownChange()
    void ReportUnknownChange(const char* workPath, CFTPServerPathType pathType);

    // provede zneplatneni listingu na indexu 'index' (napr. po "nezname zmene" v tomto listingu)
    void InvalidateListing(int index);

    // popis viz CUploadListingCache::GetListing()
    // POZOR: nevolat primo, vola se pres CUploadListingCache::GetListing()
    BOOL GetListing(const char* path, CFTPServerPathType pathType, int workerMsg,
                    int workerUID, BOOL* listingInProgress, BOOL* notAccessible,
                    BOOL* getListing, const char* name, CUploadListingItem** existingItem,
                    BOOL* nameExists);

    // popis viz CUploadListingCache::ListingFailed()
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ListingFailed()
    // POZOR: je-li 'uploadFirstWaitingWorker' NULL, musi se volat v sekci CSocketsThread::CritSect!
    void ListingFailed(const char* path, CFTPServerPathType pathType,
                       BOOL listingIsNotAccessible,
                       CUploadWaitingWorker** uploadFirstWaitingWorker,
                       BOOL* listingOKErrorIgnored);

    // popis viz CUploadListingCache::ListingFinished()
    // POZOR: nevolat primo, vola se pres CUploadListingCache::ListingFinished()
    // POZOR: musi se volat v sekci CSocketsThread::CritSect!
    BOOL ListingFinished(const char* path, CFTPServerPathType pathType,
                         const char* pathListing, int pathListingLen,
                         const CFTPDate& pathListingDate, const char* welcomeReply,
                         const char* systReply, const char* suggestedListingServerType);
};

class CUploadListingCache
{
protected:
    CRITICAL_SECTION UploadLstCacheCritSect;                  // kriticka sekce objektu
    TIndirectArray<CUploadListingsOnServer> ListingsOnServer; // pole serveru, na kterych mame cachovane listingy

public:
    CUploadListingCache();
    ~CUploadListingCache();

    // pridani nebo update listingu z panelu (pred zahajenim upload operace a po refreshi
    // v panelu behem upload operace); 'user'+'host'+'port' popisuje server; 'path' je
    // lokalni cesta na serveru typu 'pathType'; 'pathListing'+'pathListingLen' je text
    // listingu; 'pathListingDate' je datum porizeni listingu (potrebne pro "year_or_time");
    // 'listingStartTime' je IncListingCounter() z okamziku poslani prikazu LIST na server
    // (zahajeni listovani); je-li 'onlyUpdate' TRUE, provede se jedine update listingu,
    // ktery uz je v cache; 'welcomeReply' (nesmi byt NULL) je prvni odpoved serveru (obsahuje
    // casto verzi FTP serveru); 'systReply' (nesmi byt NULL) je system serveru (odpoved na
    // prikaz SYST); 'suggestedListingServerType' je typ serveru pro parsovani listingu:
    // NULL = autodetekce, jinak jmeno typu serveru (bez prip. uvodni '*', pokud prestane
    // existovat, prepne se na autodetekci); vraci FALSE pokud listing nelze parsovat nebo
    // pri nedostatku pameti nebo pri invalidnich parametrech; vraci TRUE pokud byl listing
    // pridan nebo updatnut nebo nebyl updatnut kvuli 'onlyUpdate'==TRUE nebo proto, ze
    // update neni potreba (aneb vraci TRUE, pokud je mozne listing cesty v cache pouzivat)
    BOOL AddOrUpdateListing(const char* user, const char* host, unsigned short port,
                            const char* path, CFTPServerPathType pathType,
                            const char* pathListing, int pathListingLen,
                            const CFTPDate& pathListingDate, DWORD listingStartTime,
                            BOOL onlyUpdate, const char* welcomeReply,
                            const char* systReply, const char* suggestedListingServerType);

    // odstrani z cache listingy ze serveru 'user'+'host'+'port'
    void RemoveServer(const char* user, const char* host, unsigned short port);

    // zneplatni listing cesty - pri pristim pokusu o ziskani listingu teto cesty dojde
    // k listovani cesty na serveru; 'user'+'host'+'port' popisuje server; 'path' je
    // zneplatnovana cesta typu 'pathType'
    void InvalidatePathListing(const char* user, const char* host, unsigned short port,
                               const char* path, CFTPServerPathType pathType);

    // zjisti, jestli je listing cesty prevzaty z panelu (v tom pripade vraci TRUE);
    // 'user'+'host'+'port' popisuje server; 'path' je hledana cesta typu 'pathType'
    BOOL IsListingFromPanel(const char* user, const char* host, unsigned short port,
                            const char* path, CFTPServerPathType pathType);

    // odstrani z cache "neziskatelne" listingy ze serveru 'user'+'host'+'port'
    void RemoveNotAccessibleListings(const char* user, const char* host, unsigned short port);

    // hlaseni zmeny: vytvoreni adresaru (napr. na VMS lze vytvorit vic adresaru najednou);
    // 'user'+'host'+'port' popisuje server; 'workPath' je pracovni cesta typu 'pathType';
    // 'newDirs' je parametr prikazu pro vytvoreni adresare (muze byt jeden i vic adresaru
    // relativne nebo s absolutni cestou); 'unknownResult' je FALSE pokud byly adresare
    // vytvoreny, TRUE pokud neni znamy vysledek (je treba zneplatnit prislusne listingy)
    void ReportCreateDirs(const char* user, const char* host, unsigned short port,
                          const char* workPath, CFTPServerPathType pathType, const char* newDirs,
                          BOOL unknownResult);

    // hlaseni zmeny: prejmenovani (i presun) souboru/adresare;
    // 'user'+'host'+'port' popisuje server; 'workPath' je pracovni cesta typu 'pathType';
    // 'fromName' je jmeno (bez cesty) prejmenovavaneho souboru/adresare/linku; 'newName'
    // je cilove jmeno (zadava user - muze byt relativni nebo vcetne cesty); 'unknownResult'
    // je FALSE pokud prejmenovani normalne probehlo, TRUE pokud neni znamy vysledek (je
    // treba zneplatnit prislusne listingy)
    void ReportRename(const char* user, const char* host, unsigned short port,
                      const char* workPath, CFTPServerPathType pathType,
                      const char* fromName, const char* newName, BOOL unknownResult);

    // hlaseni zmeny: smazani souboru/linku/adresare;
    // 'user'+'host'+'port' popisuje server; 'workPath' je pracovni cesta typu 'pathType';
    // 'name' je parametr prikazu mazani (jen jmeno bez cesty); 'unknownResult' je FALSE
    // pokud smazani probehlo uspesne, TRUE pokud neni znamy vysledek (je treba zneplatnit
    // prislusny listing)
    void ReportDelete(const char* user, const char* host, unsigned short port,
                      const char* workPath, CFTPServerPathType pathType, const char* name,
                      BOOL unknownResult);

    // hlaseni zmeny: zacatek uploadu souboru (muze byt i prepis/append(resume) souboru/linku) - pokud soubor
    // nebo link jeste neexistuje, vytvori se soubor se jmenem 'name'; velikost souboru se
    // nastavi na UPLOADSIZE_NEEDUPDATE; 'user'+'host'+'port' popisuje server; 'workPath' je
    // pracovni cesta typu 'pathType'; 'name' je jmeno souboru/linku (jen jmeno bez cesty)
    void ReportStoreFile(const char* user, const char* host, unsigned short port,
                         const char* workPath, CFTPServerPathType pathType, const char* name);

    // hlaseni zmeny: dokonceni uploadu souboru (mohl byt i prepis/append(resume) souboru/linku) - nastavi
    // uploadlemu souboru velikost (po ReportStoreFile je velikost rovna UPLOADSIZE_NEEDUPDATE);
    // 'user'+'host'+'port' popisuje server; 'workPath' je pracovni cesta typu 'pathType';
    // 'name' je jmeno souboru/linku (jen jmeno bez cesty); 'fileSize' je velikost souboru;
    // 'unknownResult' je FALSE pokud upload probehl uspesne, TRUE pokud neni znamy vysledek
    // (je treba zneplatnit prislusny listing)
    void ReportFileUploaded(const char* user, const char* host, unsigned short port,
                            const char* workPath, CFTPServerPathType pathType, const char* name,
                            const CQuadWord& fileSize, BOOL unknownResult);

    // hlaseni zmeny: neznama zmena (pouziva se po poslani zvoleneho commandu usera na
    // server), je treba zneplatnit listing pracovni cesty;
    // 'user'+'host'+'port' popisuje server; 'workPath' je pracovni cesta typu 'pathType'
    void ReportUnknownChange(const char* user, const char* host, unsigned short port,
                             const char* workPath, CFTPServerPathType pathType);

    // opatreni listingu cesty 'path' (typu 'pathType') na serveru 'user'+'host'+'port'
    // z cache - pokud v cache jeste neni, prida se, pokud uz se taha, pockame;
    // pokud cesta neni v cache, prida ji ve stavu ulsInProgress, vrati TRUE v 'getListing'
    // a TRUE v 'listingInProgress'; pokud je listing cesty "neziskatelny", vrati TRUE
    // v 'notAccessible' a FALSE v 'listingInProgress'; pokud je cesta v cache
    // v nekterem ze stavu ulsInProgress*, vrati FALSE v 'getListing', TRUE
    // v 'listingInProgress' a po dokonceni (nebo pri chybe) stahovani listingu
    // postne WORKER_TGTPATHLISTINGFINISHED workerovi 'workerMsg'+'workerUID';
    // pokud je cesta v cache ve stavu ulsReady, vrati FALSE v 'notAccessible',
    // FALSE v 'listingInProgress', polozku se jmenem 'name' vraci v alokovane
    // strukture 'existingItem' (NULL pokud polozka tohoto jmena nebyla nalezena)
    // a TRUE/FALSE v 'nameExists' pokud polozka se jmenem 'name' byla/nebyla
    // nalezena; vraci FALSE jen pri nedostatku pameti
    BOOL GetListing(const char* user, const char* host, unsigned short port,
                    const char* path, CFTPServerPathType pathType, int workerMsg,
                    int workerUID, BOOL* listingInProgress, BOOL* notAccessible,
                    BOOL* getListing, const char* name, CUploadListingItem** existingItem,
                    BOOL* nameExists);

    // ohlasi cache chybu pri ziskavani listingu cesty 'path' (typu 'pathType') na
    // serveru 'user'+'host'+'port' z workeru; 'listingIsNotAccessible' je TRUE
    // pokud je listing "neziskatelny" (dalsi pokusy o ziskani nemaji smysl),
    // FALSE pokud jde jen o chybu spojeni (dalsi pokusy maji smysl);
    // je-li 'uploadFirstWaitingWorker' NULL, dojde k rozeslani zprav
    // WORKER_TGTPATHLISTINGFINISHED, jinak je 'uploadFirstWaitingWorker' seznam,
    // kam se maji pridat workeri, kterym se ma poslat WORKER_TGTPATHLISTINGFINISHED;
    // v 'listingOKErrorIgnored' (neni-li NULL) se vraci TRUE pokud byl listing
    // ziskan jinym zpusobem a tato chyba se tedy muze ignorovat
    // POZOR: je-li 'uploadFirstWaitingWorker' NULL, musi se volat v sekci CSocketsThread::CritSect!
    void ListingFailed(const char* user, const char* host, unsigned short port,
                       const char* path, CFTPServerPathType pathType,
                       BOOL listingIsNotAccessible,
                       CUploadWaitingWorker** uploadFirstWaitingWorker,
                       BOOL* listingOKErrorIgnored);

    // ohlasi cache dokonceni listovani cesty 'path' (typu 'pathType') na serveru
    // 'user'+'host'+'port' z workeru; 'pathListing'+'pathListingLen' je text
    // listingu; 'pathListingDate' je datum porizeni listingu (potrebne pro "year_or_time");
    // 'welcomeReply' (nesmi byt NULL) je prvni odpoved serveru (obsahuje casto verzi
    // FTP serveru); 'systReply' (nesmi byt NULL) je system serveru (odpoved na
    // prikaz SYST); 'suggestedListingServerType' je typ serveru pro parsovani listingu:
    // NULL = autodetekce, jinak jmeno typu serveru (bez prip. uvodni '*', pokud prestane
    // existovat, prepne se na autodetekci); vraci FALSE jen pri nedostatku pameti
    // POZOR: musi se volat v sekci CSocketsThread::CritSect!
    BOOL ListingFinished(const char* user, const char* host, unsigned short port,
                         const char* path, CFTPServerPathType pathType,
                         const char* pathListing, int pathListingLen,
                         const CFTPDate& pathListingDate, const char* welcomeReply,
                         const char* systReply, const char* suggestedListingServerType);

protected:
    // volat jen z krit. sekce UploadLstCacheCritSect; vraci server z ListingsOnServer
    // nebo NULL pokud server nebyl nalezen; neni-li 'index' NULL, vraci se v nem index
    // nalezeneho serveru (nenalezen - vraci -1)
    CUploadListingsOnServer* FindServer(const char* user, const char* host,
                                        unsigned short port, int* index);
};

//
// ****************************************************************************
// CFTPOpenedFiles
//
// protoze FTP servery neresi, jestli uz je soubor otevreny nebo ne (je mozne provadet
// upload/download/delete jedineho souboru z libovolneho poctu spojeni najednou - vysledek
// je pak dost zavisly na tom, jak je server implementovany), kontrolujeme aspon to,
// jestli v ramci teto instance Salamandera jiz soubor neni otevreny (uzamknuty jinou
// operaci)

enum CFTPFileAccessType
{
    ffatRead,   // pri downloadu
    ffatWrite,  // pri uploadu (muze dojit i k mazani souboru pred prepisem nebo po chybnem ulozeni)
    ffatDelete, // pri mazani
    ffatRename, // pri prejmenovani (na stare i nove jmeno)
};

class CFTPOpenedFile
{
protected:
    int UID; // UID tohoto otevreneho souboru

    CFTPFileAccessType AccessType; // z jakeho duvodu je soubor otevren (uzamknut)

    char User[USER_MAX_SIZE];    // user-name
    char Host[HOST_MAX_SIZE];    // host-address
    unsigned short Port;         // port na kterem bezi FTP server
    char Path[FTP_MAX_PATH];     // cesta k otevrenemu souboru (lokalni na serveru)
    CFTPServerPathType PathType; // typ cachovane cesty
    char Name[MAX_PATH];         // jmeno otevreneho souboru

public:
    CFTPOpenedFile(int myUID, const char* user, const char* host, unsigned short port,
                   const char* path, CFTPServerPathType pathType, const char* name,
                   CFTPFileAccessType accessType);

    // nastaveni dat do objektu
    void Set(int myUID, const char* user, const char* host, unsigned short port,
             const char* path, CFTPServerPathType pathType, const char* name,
             CFTPFileAccessType accessType);

    // porovna tento otevreny soubor a soubor dany parametry metody; vraci TRUE pokud
    // jde o stejny soubor
    BOOL IsSameFile(const char* user, const char* host, unsigned short port,
                    const char* path, CFTPServerPathType pathType, const char* name);

    BOOL IsUID(int uid) { return uid == UID; }

    // vraci TRUE pokud neni mozne otevrit tento soubor s pristupem 'accessType'
    // (zjistuje podle aktualniho pristupu 'AccessType'); vraci FALSE, pokud
    // je dalsi otevreni tohoto souboru mozne (napr. druhy download)
    BOOL IsInConflictWith(CFTPFileAccessType accessType);
};

class CFTPOpenedFiles
{
protected:
    CRITICAL_SECTION FTPOpenedFilesCritSect; // kriticka sekce objektu

    TIndirectArray<CFTPOpenedFile> OpenedFiles;      // vsechny prave ted otevrene soubory
    TIndirectArray<CFTPOpenedFile> AllocatedObjects; // jen sklad alokovanych nevyuzitych objektu CFTPOpenedFile (jen optimalizace, aby se porad nealokovalo a nedealokovalo)
    int NextOpenedFileUID;                           // UID pro dalsi otevirany soubor

public:
    CFTPOpenedFiles();
    ~CFTPOpenedFiles();

    // zkontroluje jestli je soubor mozne otevrit s pristupem 'accessType', pokud ano:
    // prida ho mezi otevrene soubory a vrati TRUE a 'newUID' pridaneho souboru, jinak
    // (i pri nedostatku pameti): vrati FALSE
    BOOL OpenFile(const char* user, const char* host, unsigned short port,
                  const char* path, CFTPServerPathType pathType, const char* name,
                  int* newUID, CFTPFileAccessType accessType);

    // zavre otevreny soubor s UID 'UID'
    void CloseFile(int UID);
};

//
// ****************************************************************************

// vytvori polozku pro Delete operaci pro soubor/adresar 'f'
CFTPQueueItem* CreateItemForDeleteOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                            CFTPListingPluginDataInterface* dataIface,
                                            CFTPQueueItemType* type, BOOL* ok, BOOL isTopLevelDir,
                                            int hiddenFileDel, int hiddenDirDel,
                                            CFTPQueueItemState* state, DWORD* problemID,
                                            int* skippedItems, int* uiNeededItems);

// vytvori polozku pro Copy nebo Move operaci pro soubor/adresar 'f'
CFTPQueueItem* CreateItemForCopyOrMoveOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                CFTPListingPluginDataInterface* dataIface,
                                                CFTPQueueItemType* type, int transferMode,
                                                CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                const char* targetName, CQuadWord* size,
                                                BOOL* sizeInBytes, CQuadWord* totalSize);

// vytvori polozku pro operaci Change Attributes pro soubor/adresar 'f'
CFTPQueueItem* CreateItemForChangeAttrsOperation(const CFileData* f, BOOL isDir, int rightsCol,
                                                 CFTPListingPluginDataInterface* dataIface,
                                                 CFTPQueueItemType* type, BOOL* ok,
                                                 CFTPQueueItemState* state, DWORD* problemID,
                                                 int* skippedItems, int* uiNeededItems,
                                                 BOOL* skip, BOOL selFiles,
                                                 BOOL selDirs, BOOL includeSubdirs,
                                                 DWORD attrAndMask, DWORD attrOrMask,
                                                 int operationsUnknownAttrs);

// vytvori polozku pro Copy nebo Move z disku na FS pro soubor/adresar 'name'
CFTPQueueItem* CreateItemForCopyOrMoveUploadOperation(const char* name, BOOL isDir, const CQuadWord* size,
                                                      CFTPQueueItemType* type, int transferMode,
                                                      CFTPOperation* oper, BOOL copy, const char* targetPath,
                                                      const char* targetName, CQuadWord* totalSize,
                                                      BOOL isVMS);

//
// ****************************************************************************

extern HANDLE WorkerMayBeClosedEvent;             // generuje pulz v okamziku zavreni socketu workera
extern int WorkerMayBeClosedState;                // navysuje se s kazdym zavrenim socketu workera
extern CRITICAL_SECTION WorkerMayBeClosedStateCS; // kriticka sekce pro pristup k WorkerMayBeClosedState

extern CFTPOperationsList FTPOperationsList; // vsechny operace na FTP
extern CUIDArray CanceledOperations;         // pole UID operaci, ktere se maji zrusit (po prijeti prikazu FTPCMD_CANCELOPERATION)

extern CReturningConnections ReturningConnections; // pole s vracenymi spojenimi (z workeru do panelu)

extern CFTPDiskThread* FTPDiskThread; // thread zajistujici diskove operace (duvod: neblokujici volani)

extern CUploadListingCache UploadListingCache; // cache listingu cest na serverech - pouziva se pri uploadu pro zjistovani, jestli cilovy soubor/adresar jiz existuje

extern CFTPOpenedFiles FTPOpenedFiles; // seznam souboru prave ted otevrenych z tohoto Salamandera na FTP serverech

#pragma pack(pop, enter_include_operats_h_dt)
