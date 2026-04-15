// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#pragma once

#ifdef _MSC_VER
#pragma pack(push, enter_include_spl_file) // so the structures are independent of the current packing alignment
#pragma pack(4)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

//****************************************************************************
//
// CSalamanderSafeFileAbstract
//
// Rodina metod SafeFile slouzi pro osetrenou praci se soubory. Metody kontroluji
// chybove stavy API volani a zobrazuji odpovidajici chybove hlasky. Chybove hlasky
// mohou obsahovat ruzne kombinace tlacitek. Od OK, pres Retry/Cancel, az po
// Retry/Skip/Skip all/Cancel. Kombinaci tlacitek urcuje volajici funkce jednim
// z parametru.
//
// Metody behem reseni problemovych stavu potrebuji znat nazev souboru, aby mohly
// zobrazit solidni chybovou hlasku. Zaroven potrebuji znat parametry oteviraneho
// souboru (jako je dwDesiredAccess, dwShareMode, atd), aby v pripade chyby mohly
// zavrit handle a znovu jej otevrit. Pokud napriklad dojde k preruseni na urovni
// sitove vrstvy behem operace ReadFile nebo WriteFile a uzivatel odstrani pricinu
// problemy a stiskne Retry, nelze znovu pouzit stary handle souboru. Je treba
// stary handle zavrit, soubor znovu otevrit, nastavit ukazovatko a operaci opakovat.
// Proto POZOR: metody SafeFileRead a SafeFileWrite pri reseni chybovych stavu mohou
// zmenit hodnotu SAFE_FILE::HFile.
//
// Z popsanych duvodu tedy nestacil klasicky HANDLE pro drzeni kontextu a nahrazuje
// jej struktura SAFE_FILE. V pripade metody SafeFileOpen je to nezbytny parametr,
// zatimco pro metody SafeFileCreate je tento parametr pouze [optional]. To je dano
// potrebou zachovat kompatibilni chovani metody SafeFileCreate pro sarsi pluginy.
//
// Metody podporujici tlacitka Skip all/Overwrite all maji parametr 'silentMask'.
// Jedna se o ukazatel na bitove pole slozene SILENT_SKIP_xxx a SILENT_OVERWRITE_xxx.
// Pokud je ukazatel ruzny od NULL, plni bitove pole dve funkce:
// (1) vstupni: pokud je nastaven odpovidajici bit, metoda v pripade chyby nezobrazi
//              chybove hlaseni a tise si odpovi bez interakce s uzivatelem.
// (2) vystupni: Pokud uzivatel odpovi na dotaz v pripade chyby tlacitkem Skip all
//               nebo Overwrite all, metoda nastavi prislusny bit v bitovem poli.
// Toto bitové pole slouží jako kontext předávaný do jednotlivých metod. Pro jednu
// logickou skupinu operací (například vybalování více souborů z archivu) předává
// volající stejné bitové pole, které na začátku inicializuje na hodnotu 0.
// Pripadne muze nektere bity v bitovem poli explicitne nastavit, aby potlacil
// prislusne dotazy.
// Salamander rezervuje cast bitoveho pole pro vnitrni stavy pluginu.
// Jedna se o jednickove bity v SILENT_RESERVED_FOR_PLUGINS.
//
// Pokud neni u ukazatelu predavanych do metody rozhrani specifikovano jinak,
// nesmeji mit hodnotu NULL.
//

struct SAFE_FILE
{
    HANDLE HFile;                // handle of the open file (note: it is managed under Salamander core HANDLES)
    char* FileName;              // full path of the open file
    HWND HParentWnd;             // hParent window handle from the SafeFileOpen/SafeFileCreate call; it is used
                                 // pokud je hParent v nasledujich volani nastaven na HWND_STORED
    DWORD dwDesiredAccess;       // > zaloha parametru pro API CreateFile
    DWORD dwShareMode;           // > for possible retries
    DWORD dwCreationDisposition; // > in case of read or write errors
    DWORD dwFlagsAndAttributes;  // >
    BOOL WholeFileAllocated;     // TRUE if SafeFileCreate preallocated the whole file
};

#define HWND_STORED ((HWND) - 1)

#define SAFE_FILE_CHECK_SIZE 0x00010000 // FIXME: verify that it does not conflict with BUTTONS_xxx

// silentMask bit flags
// skip section
#define SILENT_SKIP_FILE_NAMEUSED 0x00000001 // skips files that cannot be created because a \\
                                             // a directory with the same name already exists (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_DIR_NAMEUSED 0x00000002  // skips directories that cannot be created because a \\
                                             // a file with the same name already exists (old CNFRM_MASK_NAMEUSED)
#define SILENT_SKIP_FILE_CREATE 0x00000004   // skips files that cannot be created for another reason (old CNFRM_MASK_ERRCREATEFILE)
#define SILENT_SKIP_DIR_CREATE 0x00000008    // skips directories that cannot be created for another reason (old CNFRM_MASK_ERRCREATEDIR)
#define SILENT_SKIP_FILE_EXIST 0x00000010    // skips files that already exist (old CNFRM_MASK_FILEOVERSKIP) \\
                                             // mutually exclusive with SILENT_OVERWRITE_FILE_EXIST
#define SILENT_SKIP_FILE_SYSHID 0x00000020   // skips existing System/Hidden files (old CNFRM_MASK_SHFILEOVERSKIP) \\
                                             // mutually exclusive with SILENT_OVERWRITE_FILE_SYSHID
#define SILENT_SKIP_FILE_READ 0x00000040     // skips files for which a read error occurred
#define SILENT_SKIP_FILE_WRITE 0x00000080    // skips files for which a write error occurred
#define SILENT_SKIP_FILE_OPEN 0x00000100     // skips files that cannot be opened

// overwrite section
#define SILENT_OVERWRITE_FILE_EXIST 0x00001000  // overwrites files that already exist (old CNFRM_MASK_FILEOVERYES) \\
                                                // mutually exclusive with SILENT_SKIP_FILE_EXIST
#define SILENT_OVERWRITE_FILE_SYSHID 0x00002000 // overwrites existing System/Hidden files (old CNFRM_MASK_SHFILEOVERYES) \\
                                                // mutually exclusive with SILENT_SKIP_FILE_SYSHID
#define SILENT_RESERVED_FOR_PLUGINS 0xFFFF0000  // this range is reserved for plugin-specific flags

class CSalamanderSafeFileAbstract
{
public:
    //
    // SafeFileOpen
    //   Otevre existujici soubor.
    //
    // Parameters
    //   'file'
    //      [out] Ukazatel na strukturu 'SAFE_FILE' ktera obdrzi informace o otevrenem
    //      souboru. Tato struktura slouzi jako kontext pro ostatni metody z rodiny
    //      SafeFile. Hodnoty struktury maji vyznam pouze v pripade, ze SafeFileOpen
    //      vratila TRUE. Pro zavreni souboru je treba zavolat metodu SafeFileClose.
    //
    //   'fileName'
    //      [in] Ukazatel na retezec zakonceny nulou, ktery obsahuje nazev oteviraneho
    //      souboru.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwCreationDisposition'
    //   'dwFlagsAndAttributes'
    //      [in] viz API CreateFile.
    //
    //   'hParent'
    //      [in] Handle okna, ke kteremu budou modalne zobrazovany chybove hlasky.
    //
    //   'flags'
    //      [in] Jedna z hodnot BUTTONS_xxx, urcuje tlacitka zobrazena v chybovych hlaskach.
    //
    //   'pressedButton'
    //      [out] Ukazatel na promennou, ktera obdrzi stisknute tlacitko behem chybove
    //      hlasky. Promenna ma vyznam pouze v pripade, ze metoda SafeFileOpen vrati FALSE,
    //      jinak jeji hodnota neni definovana. Vraci jednu z hodnot DIALOG_xxx.
    //      V pripade chyb vraci hodnotu DIALOG_CANCEL.
    //      Pokud je diky 'silentMask' ignorovana nektera chybova hlaska, vraci hodnotu
    //      odpovidajiciho tlacitka (napriklad DIALOG_SKIP nebo DIALOG_YES).
    //
    //      'pressedButton' muze byt NULL (napriklad pro BUTTONS_OK nebo BUTTONS_RETRYCANCEL
    //      nema vyznam testovat stisknute tlacitko).
    //
    //   'silentMask'
    //      [in/out] Ukazatel na promennou obsahujici bitove pole hodnot SILENT_xxx.
    //      Pro metodu SafeFileOpen ma vyznam pouze hodnota SILENT_SKIP_FILE_OPEN.
    //
    //      Pokud je v bitovem poli nastaven bit SILENT_SKIP_FILE_OPEN, zaroven by
    //      zobrazena hlaska mela tlacitko Skip (rizeno parametrem 'flags') a zaroven
    //      dojde k chybe behem otevirani souboru, bude chybova hlaska potlacena.
    //      SafeFileOpen pak vrati FALSE a pokud je 'pressedButton' ruzne od NULL,
    //      nastavi do nej hodnotu DIALOG_SKIP.
    //
    // Return Values
    //   Vraci TRUE v pripade uspesneho otevreni souboru. Struktura 'file' je inicializovana
    //   a pro zavreni souboru je treba zavolat SafeFileClose.
    //
    //   V pripade chyby vraci FALSE a nastavi hodnoty promennych 'pressedButton'
    //   a 'silentMask', jsou-li ruzne od NULL.
    //
    // Remarks
    //   Metodu lze volat z libovolneho threadu.
    //
    virtual BOOL WINAPI SafeFileOpen(SAFE_FILE* file,
                                     const char* fileName,
                                     DWORD dwDesiredAccess,
                                     DWORD dwShareMode,
                                     DWORD dwCreationDisposition,
                                     DWORD dwFlagsAndAttributes,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask) = 0;

    //
    // SafeFileCreate
    //   Vytvori novy soubor vcetne cesty, pokud jiz neexistuje. Pokud jiz soubor existuje,
    //   nabidne jeho prepsani. Metoda je primarne urcena pro vytvareni souboru a adresaru
    //   vybalovanych z archivu.
    //
    // Parameters
    //   'fileName'
    //      [in] Ukazatel na retezec zakonceny nulou, ktery specifikuje nazev
    //      vytvareneho souboru.
    //
    //   'dwDesiredAccess'
    //   'dwShareMode'
    //   'dwFlagsAndAttributes'
    //      [in] viz API CreateFile.
    //
    //   'isDir'
    //      [in] Urcuje, zda posledni slozka cesty 'fileName' ma byt adresar (TRUE)
    //      nebo soubor (FALSE). Pokud je 'isDir' TRUE, budou ignorovany promenne
    //      'dwDesiredAccess', 'dwShareMode', 'dwFlagsAndAttributes', 'srcFileName',
    //      'srcFileInfo' a 'file'.
    //
    //   'hParent'
    //      [in] Handle okna, ke kteremu budou modalne zobrazovany chybove hlasky.
    //
    //   'srcFileName'
    //      [in] Ukazatel na retezec zakonceny nulou, ktery specifikuje nazev
    //      zdrojoveho souboru. Tento nazev bude zobrazen spolecne s velikosti
    //      a casem ('srcFileInfo') v dotazu na prepsani existujiciho souboru,
    //      pokud jiz soubor 'fileName' existuje.
    //      'srcFileName' muze byt NULL, potom je 'srcFileInfo' ignorovano.
    //      V tomto pripade zobrazi pripadny dotaz na prepsani obsahovat na miste
    //      zdrojoveho souboru text "a newly created file".
    //
    //   'srcFileInfo'
    //      [in] Ukazatel na retezec zakonceny nulou, ktery obsahuje velikost, datum
    //      a cas zdrojoveho souboru. Tyto informace budou zobrazeny spolecne s nazvem
    //      zdrojoveho souboru 'srcFileName' v dotazu na prepsani existujiciho souboru.
    //      Format: "velikost, datum, cas".
    //      Velikost ziskame pomoci CSalamanderGeneralAbstract::NumberToStr,
    //      datum pomoci GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, ...
    //      a cas pomoci GetTimeFormat(LOCALE_USER_DEFAULT, 0, ...
    //      Viz implementace metody GetFileInfo v pluginu UnFAT.
    //      'srcFileInfo' muze byt NULL, pokud je take 'srcFileName' NULL.
    //
    //    'silentMask'
    //      [in/out] Ukazatel na bitove pole slozene ze SILENT_SKIP_xxx a SILENT_OVERWRITE_xxx,
    //      viz uvod na zacatku tohoto souboru. Pokud je 'silentMask' NULL, bude ignorovano.
    //      Metoda SafeFileCreate testuje a nastavuje tyto konstanty:
    //        SILENT_SKIP_FILE_NAMEUSED
    //        SILENT_SKIP_DIR_NAMEUSED
    //        SILENT_OVERWRITE_FILE_EXIST
    //        SILENT_SKIP_FILE_EXIST
    //        SILENT_OVERWRITE_FILE_SYSHID
    //        SILENT_SKIP_FILE_SYSHID
    //        SILENT_SKIP_DIR_CREATE
    //        SILENT_SKIP_FILE_CREATE
    //
    //      Pokud je 'srcFileName' ruzne od NULL, tedy jedna se o COPY/MOVE operaci, plati:
    //        Je-li v konfiguraci Salamandera (stranka Confirmations) vypnuta volba
    //        "Confirm on file overwrite", chova se metoda jako-by 'silentMask' obsahovalo
    //        SILENT_OVERWRITE_FILE_EXIST.
    //        Je-li vypnuto "Confirm on system or hidden file overwrite", chova se metoda
    //        jako-by 'silentMask' obsahovalo SILENT_OVERWRITE_FILE_SYSHID.
    //
    //    'allowSkip'
    //      [in] Specifikuje, zda dotazy a chybove hlasky budou obsahovat take tlacitka "Skip"
    //      a "Skip all"
    //
    //    'skipped'
    //      [out] Vraci TRUE v pripade, ze uzivatel v dotazu nebo chybove hlasce kliknul na
    //      tlacitko "Skip" nebo "Skip all". Jinak vraci FALSE. Promenna 'skipped' muze byt NULL.
    //      Promenna ma vyznam pouze v pripade, ze SafeFileCreate vrati INVALID_HANDLE_VALUE.
    //
    //    'skipPath'
    //      [out] Ukazatel na buffer, ktery obdrzi cestu, kterou si uzivatel v nekterem z
    //      dotazu pral preskocit tlacitkem "Skip" nebo "Skip all". Velikost bufferu je
    //      dana promennou skipPathMax, ktera nebude prekrocena. Cesta bude zakoncena nulou.
    //      Na zacatku metody SafeFileCreate je do bufferu nastaven prazdny retezec.
    //      'skipPath' muze byt NULL, 'skipPathMax' je potom ignorovano.
    //
    //    'skipPathMax'
    //      [in] Velikost bufferu 'skipPath' ve znacich. Musi byt nastavena je-li 'skipPath'
    //      ruzna od NULL.
    //
    //    'allocateWholeFile'
    //      [in/out] Ukazatel na CQuadWord udavajici velikost, na kterou by se mel soubor
    //      predalokovat pomoci funkce SetEndOfFile. Pokud je ukazatel NULL, bude ignorovan
    //      a SafeFileCreate se o predalokaci nebude pokouset. Pokud je ukazatel ruzny od
    //      NULL, pokusi se funkce o predalokovani. Pozadovana velikost musi byt vetsi nez
    //      CQuadWord(2, 0) a mensi nez CQuadWord(0, 0x80000000) (8EB).
    //
    //      Pokud ma SafeFileCreate zaroven provest test (mechanismu predalokace nemusi byt
    //      vzdy funkcni), musi byt nastaven nejvyssi bit velikosti, tedy k hodnote pricteno
    //      CQuadWord(0, 0x80000000).
    //
    //      Pokud se soubor podari vytvorit (funkce SafeFileCreate vrati handle ruzny od
    //      INVALID_HANDLE_VALUE), bude promenna 'allocateWholeFile' nastavena na jednu z
    //      z nasledujicich hodnot:
    //       CQuadWord(0, 0x80000000): soubor se nepodarilo predalokovat a behem pristiho
    //                                 volani SafeFileCreate pro soubory do stejne destinace
    //                                 by mela byt 'allocateWholeFile' NULL
    //       CQuadWord(0, 0):          soubor se nepodarilo predalokovat, ale neni to nic
    //                                 fatalniho a pri dalsim volanim SafeFileCreate pro
    //                                 soubory s touto destinaci muzete zadat jejich predalokovani
    //       jina:                     predalokovani probehlo korektne
    //                                 V tomto pripade je nastavena SAFE_FILE::WholeFileAllocated
    //                                 na TRUE a behem SafeFileClose se zavola SetEndOfFile pro
    //                                 zkraceni souboru a zamezeni ukladani zbytecnych dat.
    //
    //    'file'
    //      [out] Ukazatel na strukturu 'SAFE_FILE' ktera obdrzi informace o otevrenem
    //      souboru. Tato struktura slouzi jako kontext pro ostatni metody z rodiny
    //      SafeFile. Hodnoty struktury maji vyznam pouze v pripade, ze SafeFileCreate
    //      vratila hodnotu ruznou od INVALID_HANDLE_VALUE. Pro zavreni souboru je treba
    //      zavolat metodu SafeFileClose. Pokud je 'file' ruzne od NULL, zaradi
    //      SafeFileCreate vytvoreny handle do HANDLES Salamandera. Pokud je 'file' NULL,
    //      handle nebude do HANDLES zarazen. Pokud je 'isDir' TRUE, je promenna 'file'
    //      ignorovana.
    //
    // Return Values
    //   Pokud je 'isDir' TRUE, vraci v pripade uspechu hodnotu ruznou od INVALID_HANDLE_VALUE.
    //   Pozor, nejedna se o platny handle vytvoreneho adresare. V pripade neuspechu vraci
    //   INVALID_HANDLE_VALUE a nastavuje promenne 'silentMask', 'skipped' a 'skipPath'.
    //
    //   Pokud je 'isDir' FALSE, vraci v pripade uspechu handle vytvoreneho souboru a pokud
    //   je 'file' ruzne od NULL, plni strukturu SAFE_FILE.
    //   V pripade neuspechu vraci INVALID_HANDLE_VALUE a nastavuje promenne 'silentMask',
    //   'skipped' a 'skipPath'.
    //
    // Remarks
    //   Metodu lze volat pouze z hlavniho threadu. (muze volat API FlashWindow(MainWindow),
    //   ktere musi byt zavolano z threadu okna, jinak zpusobi deadlock)
    //
    virtual HANDLE WINAPI SafeFileCreate(const char* fileName,
                                         DWORD dwDesiredAccess,
                                         DWORD dwShareMode,
                                         DWORD dwFlagsAndAttributes,
                                         BOOL isDir,
                                         HWND hParent,
                                         const char* srcFileName,
                                         const char* srcFileInfo,
                                         DWORD* silentMask,
                                         BOOL allowSkip,
                                         BOOL* skipped,
                                         char* skipPath,
                                         int skipPathMax,
                                         CQuadWord* allocateWholeFile,
                                         SAFE_FILE* file) = 0;

    // SafeFileClose
    //   Closes the file and frees the data allocated in the 'file' structure.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a successful
    //      call to SafeFileCreate or SafeFileOpen.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual void WINAPI SafeFileClose(SAFE_FILE* file) = 0;

    //
    // SafeFileSeek
    //   Nastavi ukazovatko v otevrenem souboru.
    //
    // Parameters
    //   'file'
    //      [in] Ukazatel na strukturu 'SAFE_FILE', ktera byla inicializovana
    //      volanim metody SafeFileOpen nebo SafeFileCreate.
    //
    //   'distance'
    //      [in/out] Pocet bajtu, o kolik se ma posunou ukazovatko v souboru.
    //      V pripade uspechu obdrzi hodnotu nove pozice ukazovatka.
    //
    //      Hodnota CQuadWord::Value se interpretuje jako signed a to pro
    //      vsechny tri hodnoty 'moveMethod' (pozor na chybu v MSDN u SetFilePointerEx,
    //      kde tvrdi, ze hodnota je pro FILE_BEGIN unsigned). Pokud tedy chceme
    //      couvat od akualniho mista (FILE_CURRENT) nebo od konce (FILE_END) souboru,
    //      nastavime CQuadWord::Value na zaporne cislo. Do promenne CQuadWord::Value
    //      lze primo priradit napriklad __int64.
    //
    //      Vracena hodnota je absolutni pozice od zacatku souboru a jeji hodnoty budou
    //      od 0 do 2^63. Soubory nad 2^63 zadne ze soucasnych Windows nepodporuji.
    //
    //   'moveMethod'
    //      [in] Vychozi pozice pro ukazovatko. Muze byt jedna z hodnot:
    //           FILE_BEGIN, FILE_CURRENT nebo FILE_END.
    //
    //   'error'
    //      [out] Ukazatel na promennou DWORD, ktera v pripade chyby bude obsahovat
    //      hodnotu vracenou z GetLastError(). 'error' muze byt NULL.
    //
    // Return Values
    //   V pripade uspechu vraci TRUE a hodnota promenne 'distance' je nastavena
    //   na novou pozici ukazovatka v souboru.
    //
    //   V pripade chyby vraci FALSE a nastavi hodnotu 'error' na GetLastError,
    //   je-li 'error' ruzna od NULL. Chybu nezobrazuje, k tomu slouzi SafeFileSeekMsg.
    //
    // Remarks
    //   Metoda vola API SetFilePointer, takze pro ni plati omezeni teto funkce.
    //
    //   Neni chybou nastavit ukazovatko za konec souboru. Velikost souboru se
    //   nezvetsi dokud nezavolate SetEndOfFile nebo SafeFileWrite. Viz API SetFilePointer.
    //
    //   Metodu lze pouzit pro ziskani velikosti souboru, pokud nastavime hodnotu
    //   'distance' na 0 a 'moveMethod' na FILE_END. Vracena hodnota 'distance' bude
    //   velikost souboru.
    //
    //   Metodu lze volat z libovolneho threadu.
    //
    virtual BOOL WINAPI SafeFileSeek(SAFE_FILE* file,
                                     CQuadWord* distance,
                                     DWORD moveMethod,
                                     DWORD* error) = 0;

    //
    // SafeFileSeekMsg
    //   Nastavi ukazovatko v otevrenem souboru. Pokud dojde k chybe, zobrazi ji.
    //
    // Parameters
    //   'file'
    //   'distance'
    //   'moveMethod'
    //      Viz komentar u SafeFileSeek.
    //
    //   'hParent'
    //      [in] Handle okna, ke kteremu budou modalne zobrazovany chybove hlasky.
    //      Pokud je rovno HWND_STORED, pouzije se 'hParent' z volani SafeFileOpen/SafeFileCreate.
    //
    //   'flags'
    //      [in] Jedna z hodnot BUTTONS_xxx, urcuje tlacitka zobrazena v chybovem hlaseni.
    //
    //   'pressedButton'
    //      [out] Ukazatel na promennou, ktera obdrzi stisknute tlacitko behem chybove
    //      hlasky. Promenna ma vyznam pouze v pripade, ze metoda SafeFileSeekMsg vrati FALSE.
    //      'pressedButton' muze byt NULL (napriklad pro BUTTONS_OK nema vyznam testovat
    //      stisknute tlacitko)
    //
    //   'silentMask'
    //      [in/out] Ukazatel na promennou obsahujici bitove pole hodnot SILENT_SKIP_xxx.
    //      Podrobnosti viz komentar u SafeFileOpen.
    //      SafeFileSeekMsg testuje a nastavuje bit SILENT_SKIP_FILE_READ je-li
    //      'seekForRead' TRUE nebo SILENT_SKIP_FILE_WRITE, je-li 'seekForRead' FALSE;
    //
    //   'seekForRead'
    //      [in] Rika metode, za jakym ucelem jsme provadeli seek v souboru. Metoda pouzije
    //      tuto promennou pouze v pripade chyby. Urcuje, ktery z bitu se pouzije pro
    //      'silentMask' a jaky bude titulek chybove hlasky: "Error Reading File" nebo
    //      "Error Writing File".
    //
    // Return Values
    //   V pripade uspechu vraci TRUE a hodnota promenne 'distance' je nastavena
    //   na novou pozici ukazovatka v souboru.
    //
    //   V pripade chyby vraci FALSE a nastavi hodnoty promennych 'pressedButton'
    //   a silentMask, jsou-li ruzne od NULL.
    //
    // Remarks
    //   Viz metoda SafeFileSeek.
    //
    //   Metodu lze volat z libovolneho threadu.
    //
    virtual BOOL WINAPI SafeFileSeekMsg(SAFE_FILE* file,
                                        CQuadWord* distance,
                                        DWORD moveMethod,
                                        HWND hParent,
                                        DWORD flags,
                                        DWORD* pressedButton,
                                        DWORD* silentMask,
                                        BOOL seekForRead) = 0;

    // SafeFileGetSize
    //   Returns the file size.
    //
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'fileSize'
    //      [out] Pointer to a CQuadWord structure that receives the file size.
    //
    //   'error'
    //      [out] Pointer to a DWORD variable that receives the value returned
    //      by GetLastError() on failure. 'error' may be NULL.
    //
    // Return Values
    //   Returns TRUE on success and sets 'fileSize'.
    //   Returns FALSE on failure and sets 'error' if it is not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileGetSize(SAFE_FILE* file,
                                        CQuadWord* fileSize,
                                        DWORD* error) = 0;

    // SafeFileRead
    //   Reads data from the file starting at the current file pointer. After the
    //   operation completes, the file pointer is advanced by the number of bytes
    //   read. This method supports only synchronous reads, so it does not return
    //   until the data is read or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'lpBuffer'
    //      [out] Pointer to the buffer that receives the data read from the file.
    //
    //   'nNumberOfBytesToRead'
    //      [in] Specifies how many bytes to read from the file.
    //
    //   'lpNumberOfBytesRead'
    //      [out] Points to a variable that receives the actual number of bytes read
    //      into the buffer.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //      If set to HWND_STORED, the 'hParent' from SafeFileOpen/SafeFileCreate is used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values, optionally ORed with SAFE_FILE_CHECK_SIZE;
    //      determines the buttons shown in error messages. If the SAFE_FILE_CHECK_SIZE bit is set,
    //      SafeFileRead treats failure to read the requested number of bytes as an error and
    //      shows an error message. Without this bit, it behaves like the ReadFile API.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   On success, returns TRUE and sets 'lpNumberOfBytesRead' to the number of bytes read.
    //
    //   On failure, returns FALSE and sets 'pressedButton' and 'silentMask' if they are not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileRead(SAFE_FILE* file,
                                     LPVOID lpBuffer,
                                     DWORD nNumberOfBytesToRead,
                                     LPDWORD lpNumberOfBytesRead,
                                     HWND hParent,
                                     DWORD flags,
                                     DWORD* pressedButton,
                                     DWORD* silentMask) = 0;

    // SafeFileWrite
    //   Writes data to the file at the current file pointer. After the operation
    //   completes, the file pointer is advanced by the number of bytes written.
    //   This method supports only synchronous writes, so it does not return
    //   until the data is written or an error occurs.
    //
    // Parameters
    //   'file'
    //      [in] Pointer to a 'SAFE_FILE' structure initialized by a call to
    //      SafeFileOpen or SafeFileCreate.
    //
    //   'lpBuffer'
    //      [in] Pointer to the buffer containing the data to write to the file.
    //
    //   'nNumberOfBytesToWrite'
    //      [in] Specifies how many bytes to write from the buffer to the file.
    //
    //   'lpNumberOfBytesWritten'
    //      [out] Points to a variable that receives the actual number of bytes written.
    //
    //   'hParent'
    //      [in] Handle of the window to which error messages are shown modally.
    //      If set to HWND_STORED, the 'hParent' from SafeFileOpen/SafeFileCreate is used.
    //
    //   'flags'
    //      [in] One of the BUTTONS_xxx values; determines the buttons shown in error messages.
    //
    //   'pressedButton'
    //   'silentMask'
    //      See SafeFileOpen.
    //
    // Return Values
    //   On success, returns TRUE and sets 'lpNumberOfBytesWritten' to the number of bytes written.
    //
    //   On failure, returns FALSE and sets 'pressedButton' and 'silentMask' if they are not NULL.
    //
    // Remarks
    //   This method can be called from any thread.
    virtual BOOL WINAPI SafeFileWrite(SAFE_FILE* file,
                                      LPVOID lpBuffer,
                                      DWORD nNumberOfBytesToWrite,
                                      LPDWORD lpNumberOfBytesWritten,
                                      HWND hParent,
                                      DWORD flags,
                                      DWORD* pressedButton,
                                      DWORD* silentMask) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_file)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
