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
#pragma pack(push, enter_include_spl_com) // so that the structures are independent of the current packing
#pragma pack(4)
#pragma warning(3 : 4706) // warning C4706: assignment within conditional expression
#endif                    // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a4
#endif // __BORLANDC__

// the plugin must define the SalamanderVersion variable (int) and initialize it in SalamanderPluginEntry:
// SalamanderVersion = salamander->GetVersion();

// global variable with the Salamander version in which this plugin is loaded
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

    // conversion to double (beware of precision loss for large numbers - double has only 15 significant digits)
    double GetDouble() const
    { // MSVC cannot convert unsigned __int64 to double, so we have to help it
        if (Value < CQuadWord(0, 0x80000000).Value)
            return (double)(__int64)Value; // positive number
        else
            return 9223372036854775808.0 + (double)(__int64)(Value - CQuadWord(0, 0x80000000).Value);
    }
};

#define QW_MAX CQuadWord(0xFFFFFFFF, 0xFFFFFFFF)

#define ICONOVERLAYINDEX_NOTUSED 15 // hodnota pro CFileData::IconOverlayIndex v pripade, ze ikona nema overlay

// record of each file and directory in Salamander (basic file/directory data)
struct CFileData // no destructor may be added here!
{
    char* Name;                    // allocated file name (without the path), must be allocated on
                                   // Salamander's heap (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    char* Ext;                     // ukazatel do Name za prvni tecku zprava (vcetne tecky na zacatku jmena,
                                   // na Windows se chape jako pripona, narozdil od UNIXu) nebo na konec
                                   // Name, pokud pripona neexistuje; je-li v konfiguraci nastaveno FALSE
                                   // pro SALCFG_SORTBYEXTDIRSASFILES, je v Ext pro adresare ukazatel na konec
                                   // Name (adresare nemaji pripony)
    CQuadWord Size;                // file size in bytes
    DWORD Attr;                    // file attributes - ORed FILE_ATTRIBUTE_XXX constants
    FILETIME LastWrite;            // last-write time of the file (UTC-based time)
    char* DosName;                 // allocated DOS 8.3 file name; if not needed it is NULL, must be
                                   // allocated on Salamander's heap (see CSalamanderGeneralAbstract::Alloc/Realloc/Free)
    DWORD_PTR PluginData;          // used by the plugin through CPluginDataInterfaceAbstract; Salamander ignores it
    unsigned NameLen : 9;          // delka retezce Name (strlen(Name)) - POZOR: maximalni delka jmena je (MAX_PATH - 5)
    unsigned Hidden : 1;           // je hidden? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IsLink : 1;           // je link? (je-li 1, ikonka ma overlay linku) - standardni plneni viz CSalamanderGeneralAbstract::IsFileLink(CFileData::Ext), pri zobrazeni ma prednost pred IsOffline, ale IconOverlayIndex ma prednost
    unsigned IsOffline : 1;        // je offline? (je-li 1, ikonka ma overlay offline - cerne hodiny), pri zobrazeni ma IsLink i IconOverlayIndex prednost
    unsigned IconOverlayIndex : 4; // icon overlay index (if the icon has no overlay, this is ICONOVERLAYINDEX_NOTUSED); when displayed, it takes precedence over IsLink and IsOffline

    // flags for Salamander internal use: they are cleared when added to CSalamanderDirectoryAbstract
    unsigned Association : 1;     // meaningful only for displaying 'simple icons' - icon of the associated file, otherwise 0
    unsigned Selected : 1;        // read-only selection flag (0 - item not selected, 1 - item selected)
    unsigned Shared : 1;          // is the directory shared? not used for files
    unsigned Archive : 1;         // is this an archive? used to display the archive icon in the panel
    unsigned SizeValid : 1;       // has the directory size already been computed?
    unsigned Dirty : 1;           // does this item need to be redrawn? (temporary only; the message queue must not be pumped between setting the bit and redrawing the panel, otherwise the icon may be redrawn by the icon reader and the bit reset; as a result the item will not be redrawn)
    unsigned CutToClip : 1;       // je CUT-nutej na clipboardu? (je-li 1, ikonka je pruhlednejsi o 50% - ghosted)
    unsigned IconOverlayDone : 1; // for icon-reader-thread use only: are we retrieving or have we already retrieved the icon overlay? (0 - no, 1 - yes)
};

// constants that specify the validity of data stored directly in CFileData (size, extension, etc.)
// or generated automatically from directly stored data (file type is generated from the extension);
// Name + NameLen are mandatory (must always be valid); PluginData validity is managed by the plugin itself
// (Salamander ignores this field)
#define VALID_DATA_EXTENSION 0x0001   // extension is stored in Ext (without it: all Ext values point to the end of Name)
#define VALID_DATA_DOSNAME 0x0002     // DOS name is stored in DosName (without it: all DosName values = NULL)
#define VALID_DATA_SIZE 0x0004        // the size in bytes is stored in Size (without it: all Size values are 0)
#define VALID_DATA_TYPE 0x0008        // file type can be generated from Ext (without it: it is not generated)
#define VALID_DATA_DATE 0x0010        // modification date (UTC-based) is stored in LastWrite (without it: all dates in LastWrite are 1.1.1602 in local time)
#define VALID_DATA_TIME 0x0020        // modification time (UTC-based) is stored in LastWrite (without it: all times in LastWrite are 0:00:00 in local time)
#define VALID_DATA_ATTRIBUTES 0x0040  // attributes are stored in Attr (ORed Win32 API FILE_ATTRIBUTE_XXX constants) (without it: all Attr values are 0)
#define VALID_DATA_HIDDEN 0x0080      // 'ghosted' icon flag is stored in Hidden (without it: all Hidden values are 0)
#define VALID_DATA_ISLINK 0x0100      // IsLink obsahuje 1 pokud jde o link, ikonka ma overlay linku (bez: vsechny IsLink = 0)
#define VALID_DATA_ISOFFLINE 0x0200   // IsOffline is 1 for an offline file/directory; the icon gets the offline overlay (without it: all IsOffline values are 0)
#define VALID_DATA_PL_SIZE 0x0400     // meaningful only when VALID_DATA_SIZE is not used: the plugin stores the size in bytes for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetByteSize() to retrieve it
#define VALID_DATA_PL_DATE 0x0800     // meaningful only when VALID_DATA_DATE is not used: the plugin stores the modification date for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetLastWriteDate() to retrieve it
#define VALID_DATA_PL_TIME 0x1000     // meaningful only when VALID_DATA_TIME is not used: the plugin stores the modification time for at least some files/directories (somewhere in PluginData); Salamander calls CPluginDataInterfaceAbstract::GetLastWriteTime() to retrieve it
#define VALID_DATA_ICONOVERLAY 0x2000 // IconOverlayIndex is the icon overlay index (no overlay = ICONOVERLAYINDEX_NOTUSED) (without it: all IconOverlayIndex values are ICONOVERLAYINDEX_NOTUSED); for assigning overlays see CSalamanderGeneralAbstract::SetPluginIconOverlays

#define VALID_DATA_NONE 0 // helper constant - only Name and NameLen are valid

#ifdef INSIDE_SALAMANDER
// VALID_DATA_ALL and VALID_DATA_ALL_FS_ARC are for Salamander core internal use only;
// plugins should OR only constants that correspond to data supplied by the plugin
// (to avoid problems when new constants and corresponding data are added)
#define VALID_DATA_ALL 0xFFFF
#define VALID_DATA_ALL_FS_ARC (0xFFFF & ~VALID_DATA_ICONOVERLAY) // for FS and archives: everything except icon overlays
#endif                                                           // INSIDE_SALAMANDER

// If hiding hidden and system files/directories is enabled, items with
// Hidden==1 and Attr containing FILE_ATTRIBUTE_HIDDEN and/or FILE_ATTRIBUTE_SYSTEM are not shown in panels.

// CSalamanderDirectoryAbstract flag constants:
// file and directory names (including path components) are compared case-sensitively with this flag
// (without it, comparison is case-insensitive - the standard Windows behavior)
#define SALDIRFLAG_CASESENSITIVE 0x0001
// subdirectory names within each directory are not checked for duplicates (this
// check is time-consuming and is needed only in archives when items are added not only
// to the root - e.g. so that adding 'file1' to 'dir1' followed by adding
// 'dir1' works correctly: the first operation adds 'dir1' automatically as a missing path,
// and the second operation only refreshes the data for 'dir1' instead of adding it again)
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

    // sets the valid data mask that determines which CFileData fields are valid
    // and which should only be zeroed (see the VALID_DATA_XXX comment); the 'validData' mask
    // contains ORed VALID_DATA_XXX values; the default mask is the sum of all
    // VALID_DATA_XXX values except VALID_DATA_ICONOVERLAY; the valid data mask must be set
    // before calling AddFile/AddDir
    virtual void WINAPI SetValidData(DWORD validData) = 0;

    // sets flags for this object; 'flags' is a combination of ORed SALDIRFLAG_XXX flags;
    // the default object flag value is zero for archivers (no flags set)
    // and SALDIRFLAG_IGNOREDUPDIRS for file systems (they may add only to the root, so duplicate
    // directory checks are unnecessary)
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

    // returns the number of files in the object
    virtual int WINAPI GetFilesCount() const = 0;

    // returns the number of directories in the object
    virtual int WINAPI GetDirsCount() const = 0;

    // returns the file at index 'index'; the returned data is read-only
    virtual CFileData const* WINAPI GetFile(int index) const = 0;

    // returns the directory at index 'index'; the returned data is read-only
    virtual CFileData const* WINAPI GetDir(int index) const = 0;

    // returns the CSalamanderDirectory object for the directory at 'index'; the returned object is read-only
    // (objects for empty directories are not allocated, so a single global empty object is returned
    // - modifying that object would affect it globally)
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int index) const = 0;

    // Lets the plugin tell Salamander the expected number of files and directories in this directory in advance.
    // Salamander adjusts its reallocation strategy so adding items does not slow down too much.
    // This is worth calling for directories containing thousands of files or directories. For tens of
    // thousands, calling this method is almost necessary, otherwise reallocations can take several seconds.
    // 'files' and 'dirs' therefore specify the approximate total number of files and directories.
    // If either value is -1, Salamander ignores it.
    // The method is useful only while the directory is empty, before AddFile or AddDir has been called.
    virtual void WINAPI SetApproximateCount(int files, int dirs) = 0;
};

//
// ****************************************************************************
// SalEnumSelection a SalEnumSelection2
//

// constants returned by SalEnumSelection and SalEnumSelection2 in the 'errorOccured' parameter
#define SALENUM_SUCCESS 0 // no error occurred
#define SALENUM_ERROR 1   // an error occurred and the user chose to continue the operation (only the faulty files/directories were skipped)
#define SALENUM_CANCEL 2  // an error occurred and the user wants to cancel the operation

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

// panel view modes
#define VIEW_MODE_TREE 1
#define VIEW_MODE_BRIEF 2
#define VIEW_MODE_DETAILED 3
#define VIEW_MODE_ICONS 4
#define VIEW_MODE_THUMBNAILS 5
#define VIEW_MODE_TILES 6

#define TRANSFER_BUFFER_MAX 1024 // buffer size for transferring column contents from the plugin to Salamander
#define COLUMN_NAME_MAX 30
#define COLUMN_DESCRIPTION_MAX 100

// Column identifiers. Plugin-inserted columns have ID==COLUMN_ID_CUSTOM.
// Salamander standard columns have the other IDs.
#define COLUMN_ID_CUSTOM 0 // column provided by the plugin - the plugin is responsible for storing its data
#define COLUMN_ID_NAME 1   // left-aligned, supports FixedWidth
// zarovnano vlevo, podporuje FixedWidth; samostatny sloupec "Ext", muze byt jen na indexu==1;
// pokud sloupec neexistuje a v datech panelu (viz CSalamanderDirectoryAbstract::SetValidData())
// se nastavi VALID_DATA_EXTENSION, je sloupec "Ext" zobrazen ve sloupci "Name"
#define COLUMN_ID_EXTENSION 2
#define COLUMN_ID_DOSNAME 3     // left-aligned
#define COLUMN_ID_SIZE 4        // right-aligned
#define COLUMN_ID_TYPE 5        // left-aligned, supports FixedWidth
#define COLUMN_ID_DATE 6        // right-aligned
#define COLUMN_ID_TIME 7        // right-aligned
#define COLUMN_ID_ATTRIBUTES 8  // right-aligned
#define COLUMN_ID_DESCRIPTION 9 // left-aligned, supports FixedWidth

// Callback that fills the buffer with characters to be shown in the corresponding column.
// For optimization, the function does not receive/return variables through parameters,
// but through global variables (CSalamanderViewAbstract::GetTransferVariables).
typedef void(WINAPI* FColumnGetText)();

// Callback pro ziskani indexu jednoduchych ikon pro FS s vlastnimi ikonami (pitFromPlugin).
// Z duvodu optimalizace funkce nedostava/nevraci promenne prostrednictvim parametru,
// ale prostrednictvim globalni promennych (CSalamanderViewAbstract::GetTransferVariables).
// Z globalnich promennych callback vyuziva jen TransferFileData a TransferIsDir.
typedef int(WINAPI* FGetPluginIconIndex)();

// A column can be created in two ways:
// 1) The column was created by Salamander from the current view template.
//    In this case, 'GetText' (the fill function pointer) points into Salamander
//    and retrieves text from CFileData in the standard way.
//    The 'ID' field is different from COLUMN_ID_CUSTOM.
//
// 2) The column was added by the plugin for its own needs.
//    'GetText' points into the plugin and 'ID' equals COLUMN_ID_CUSTOM.

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

    FColumnGetText GetText; // callback used to obtain text (see the FColumnGetText declaration)

    // FIXME_X64 - too small for a pointer, is it ever needed?
    DWORD CustomData; // Not used by Salamander; the plugin can
                      // use it to distinguish the columns added by the plugin.

    unsigned SupportSorting : 1; // can the column be sorted?

    unsigned LeftAlignment : 1; // TRUE means the column is left-aligned; otherwise right-aligned

    unsigned ID : 4; // column identifier
                     // For standard columns provided by Salamander,
                     // it contains values other than COLUMN_ID_CUSTOM.
                     // For columns added by the plugin, it always contains
                     // the value COLUMN_ID_CUSTOM.

    // The Width and FixedWidth variables can be changed by the user while working with the panel.
    // Salamander stores/loads these values for standard columns it provides.
    // Values of these variables for plugin-provided columns must be stored/loaded
    // by the plugin itself.
    // Columns whose width Salamander calculates from their content and the user cannot
    // change are called 'elastic'. Columns whose width the user can set are called
    // 'fixed'.
    unsigned Width : 16;     // Sirka sloupce v pripade, ze je v rezimu pevne (nastavitelne) sirky.
    unsigned FixedWidth : 1; // is the column in fixed (user-adjustable) width mode?

    // working variables (not stored anywhere and need not be initialized)
    // are reserved for Salamander's internal use and are ignored by plugins,
    // because their contents are not guaranteed when plugin code is called
    unsigned MinWidth : 16; // minimum width to which the column can be shrunk.
                            // It is computed from the column name and its sortability
                            // so that the column header is always visible
};

// Through this interface, the plugin can change the panel display mode when the path changes.
// All column operations apply only to the detailed modes
// (Detailed + Types + the three custom modes Alt+8/9/0). When the path changes, the
// plugin gets a standard set of columns generated from the current view template.
// The plugin can modify this set. The modification is not permanent,
// and on the next path change the plugin again receives the standard set of columns. It can,
// for example, remove one of the standard columns. Before the standard columns are rebuilt,
// the plugin gets a chance to store information about its own columns (COLUMN_ID_CUSTOM).
// It can store their 'Width' and 'FixedWidth', which the user may have changed in the panel
// (see ColumnFixedWidthShouldChange() and ColumnWidthWasChanged() in
// CPluginDataInterfaceAbstract). If the plugin changes the display mode, the change is permanent
// (e.g. switching to Thumbnails remains in effect even after leaving the plugin path).

class CSalamanderViewAbstract
{
public:
    // -------------- panel ----------------

    // returns the panel display mode (tree/brief/detailed/icons/thumbnails/tiles)
    // returns one of the VIEW_MODE_xxxx values (Detailed, Types, and the three custom modes
    // all use VIEW_MODE_DETAILED)
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

    // returns the number of columns in the panel (always at least one, because Name is always shown)
    virtual int WINAPI GetColumnsCount() = 0;

    // returns a pointer to the column (read-only)
    // 'index' specifies which column is returned; returns NULL if the column at 'index' does not exist
    virtual const CColumn* WINAPI GetColumn(int index) = 0;

    // Inserts a column at position 'index'. The Name column is always at position 0,
    // and if the Ext column is shown, it is at position 1. Otherwise the column may be inserted
    // at any position. The 'column' structure is copied into Salamander's internal structures.
    // Returns TRUE if the column was inserted.
    virtual BOOL WINAPI InsertColumn(int index, const CColumn* column) = 0;

    // Inserts the standard column with ID 'id' at position 'index'. The Name column is always
    // at position 0; if the Ext column is inserted, it must be at position 1.
    // Otherwise the column may be inserted at any position. 'id' is one of the COLUMN_ID_xxxx values,
    // except COLUMN_ID_CUSTOM and COLUMN_ID_NAME.
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

    // Removes the column at position 'index'. Both plugin-added columns
    // and Salamander's standard columns can be removed. The Name column, which is always
    // at index 0, cannot be removed. Be careful when removing the Ext column: if plugin data
    // (via CSalamanderDirectoryAbstract::SetValidData()) includes VALID_DATA_EXTENSION,
    // the Ext column name+description must appear under the Name column.
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

    // archive data only (FS does not add an up-dir symbol):
    // modifies the proposed contents of the up-dir symbol ('..' at the top of the panel); 'archivePath'
    // is the archive path the symbol is for; 'upDir' receives the proposed symbol
    // data: the name '..' (do not change it), the archive date&time, everything else zeroed;
    // 'upDir' returns plugin changes, mainly 'upDir.PluginData',
    // which is used by the up-dir symbol when obtaining the contents of added columns;
    // ReleasePluginData will not be called for 'upDir'; any necessary cleanup
    // can always be done on the next call to GetFileDataForUpDir or when the
    // entire interface is released (in its destructor - called from
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
    // writes text 'txt' (even multiple lines - it is split into lines) to the progress dialog
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint) = 0;
    // if 'totalSize1' is not CQuadWord(-1, -1), sets 'totalSize1' as 100 percent of the first progress bar;
    // if 'totalSize2' is not CQuadWord(-1, -1), sets 'totalSize2' as 100 percent of the second progress bar
    // (for a progress dialog with a single progress bar, 'totalSize2' must be CQuadWord(-1, -1))
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2) = 0;
    // if 'size1' is not CQuadWord(-1, -1), sets 'size1' on the first progress bar (size1/total1*100 percent);
    // if 'size2' is not CQuadWord(-1, -1), sets 'size2' on the second progress bar (size2/total2*100 percent)
    // (for a progress dialog with a single progress bar, 'size2' must be CQuadWord(-1, -1)); returns whether the
    // action should continue (FALSE = stop)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint) = 0;
    // adds 'size' to one or both progress bars (size/total*100 percent progress),
    // returns whether the action should continue (FALSE = stop)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint) = 0;
    // enables/disables the Cancel button
    virtual void WINAPI ProgressEnableCancel(BOOL enable) = 0;
    // returns the progress dialog HWND (useful for showing errors and prompts while the progress dialog is open)
    virtual HWND WINAPI ProgressGetHWND() = 0;
    // closes the progress dialog
    virtual void WINAPI CloseProgressDialog() = 0;

    // moves all files from the 'source' directory to the 'target' directory,
    // and also remaps displayed name prefixes ('remapNameFrom' -> 'remapNameTo')
    // returns whether the operation succeeded
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo) = 0;
};

#ifdef _MSC_VER
#pragma pack(pop, enter_include_spl_com)
#endif // _MSC_VER
#ifdef __BORLANDC__
#pragma option -a
#endif // __BORLANDC__
