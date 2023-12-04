// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#define CREATE_DIR_SIZE CQuadWord(4096, 0) // odhady narocnosti operaci (namereno ne-cachovane podle doby behu worker threadu)
#define MOVE_DIR_SIZE CQuadWord(5050, 0)
#define DELETE_DIR_SIZE CQuadWord(2400, 0)
#define DELETE_DIRLINK_SIZE CQuadWord(2400, 0)
#define MOVE_FILE_SIZE CQuadWord(6500, 0)
#define COPY_MIN_FILE_SIZE CQuadWord(4096, 0) // nesmi byt mensi nez 1 (duvod: jinak nebude slapat test alokace potrebneho mista pro soubor pred zahajenim kopirovani v DoCopyFile)
#define CONVERT_MIN_FILE_SIZE CQuadWord(4096, 0)
#define COMPRESS_ENCRYPT_MIN_FILE_SIZE CQuadWord(4096, 0)
#define DELETE_FILE_SIZE CQuadWord(2300, 0)
#define CHATTRS_FILE_SIZE CQuadWord(500, 0)
#define MAX_OP_FILESIZE 6500 // POZOR: maximalni hodnota z teto skupiny

// 4/2012 - zvetsuji buffer na desetinasobek, pri kterem se u velkych souboru po siti dostavame na prenosove
// rychlosti srovnatelne s Total Commander, coz jsou 2-3x lepsi casy nez s desetinovym bufferem
// testoval jsem na lokalnich discich i po siti a nevidim zadnou nevyhodu ve zvetseni bufferu
#define OPERATION_BUFFER (10 * 32768)          // 320KB buffer pro copy a move
#define REMOVABLE_DISK_COPY_BUFFER 65536       // 64KB buffer pro copy a move na vymennych mediich (floppy, ZIP)
#define ASYNC_COPY_BUF_SIZE_512KB (128 * 1024) // 128KB buffer pro soubory do 512KB
#define ASYNC_COPY_BUF_SIZE_2MB (256 * 1024)   // 256KB buffer pro soubory do 2MB
#define ASYNC_COPY_BUF_SIZE_8MB (512 * 1024)   // 512KB buffer pro soubory do 8MB
#define ASYNC_COPY_BUF_SIZE (1024 * 1024)      // maximalni velikost bufferu pro asynchronni copy (podle Explorera max. 1MB); POZOR: musi byt >= nez RETRYCOPY_TAIL_MINSIZE
#define ASYNC_SLOW_COPY_BUF_SIZE (8 * 1024)    // 8KB buffer pro pomale kopirovani (hlavne sitove disky pres VPN)
#define ASYNC_SLOW_COPY_BUF_MINBLOCKS 12

// POZOR: HIGH_SPEED_LIMIT musi byt vetsi nebo rovno nejvetsimu z predchozi skupiny (OPERATION_BUFFER,
//        REMOVABLE_DISK_COPY_BUFFER, ASYNC_COPY_BUF_SIZE)
#define HIGH_SPEED_LIMIT (1024 * 1024) // je-li speed-limit >= toto cislo, omezujeme rychlost tak, ze po preneseni (speed-limit / HIGH_SPEED_LIMIT_BRAKE_DIV) bytu vlozime brzdici Sleep (je-li treba)
#define HIGH_SPEED_LIMIT_BRAKE_DIV 10  // popis viz HIGH_SPEED_LIMIT

void InitWorker();
void ReleaseWorker();

struct CChangeAttrsData
{
    BOOL ChangeCompression;
    BOOL ChangeEncryption;

    BOOL ChangeTimeModified;
    FILETIME TimeModified;
    BOOL ChangeTimeCreated;
    FILETIME TimeCreated;
    BOOL ChangeTimeAccessed;
    FILETIME TimeAccessed;
};

struct CConvertData // data pro ocConvert
{
    char CodeTable[256];
    int EOFType;
};

class COperations;
struct CProgressDlgArrItem;

struct CStartProgressDialogData
{
    COperations* Script;
    const char* Caption;
    CChangeAttrsData* AttrsData;
    CConvertData* ConvertData;
    CProgressDlgArrItem* NewDlg;
    BOOL OperationWasStarted;
    HANDLE ContEvent;
    RECT MainWndRectClipR; // pouziva se pro centrovani progress dialogu bez parenta (backgroundove operace)
    RECT MainWndRectByR;   // pouziva se pro centrovani progress dialogu bez parenta (backgroundove operace)
};

struct CProgressData
{
    const char *Operation,
        *Source,
        *Preposition,
        *Target;
};

//
// ****************************************************************************
// CTransferSpeedMeter
//
// objekt pro vypocet rychlosti prenosu dat (prevzaty z FTP pluginu)

#define TRSPMETER_ACTSPEEDSTEP 200        // pro vypocet prenosove rychlosti: velikost kroku v milisekundach (nesmi byt 0)
#define TRSPMETER_ACTSPEEDNUMOFSTEPS 25   // pro vypocet prenosove rychlosti: pocet pouzitych kroku (vic kroku = jemnejsi zmeny rychlosti pri "vypadku" prvniho kroku ve fronte)
#define TRSPMETER_NUMOFSTOREDPACKETS 40   // pro vypocet prenosove rychlosti: kolik poslednich "paketu" se ma pamatovat (pri nizkych rychlostech se z nich pocita rychlost) (nesmi byt 0)
#define TRSPMETER_STPCKTSMININTERVAL 2000 // pro vypocet prenosove rychlosti: minimalni doba mezi prijetim prvniho a posledniho ulozeneho "paketu", kdy je lze pouzit pro vypocet rychlosti (nizke rychlosti)

class CTransferSpeedMeter
{
protected:
    // POZOR: pristup do objektu je mozny jen v kriticke sekci COperations::StatusCS

    // vypocet prenosove rychlosti:
    DWORD TransferedBytes[TRSPMETER_ACTSPEEDNUMOFSTEPS + 1]; // kruhova fronta s poctem bytu prenesenych v poslednich N krocich (cas. intervalech) + jeden "pracovni" krok navic (nascitava se v nem hodnota za akt. interval)
    int ActIndexInTrBytes;                                   // index posledniho (aktualniho) zaznamu v TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                          // casova hranice (v ms) posledniho zaznamu v TransferedBytes (do tohoto casu se nacitaji byty do posl. zaznamu)
    int CountOfTrBytesItems;                                 // pocet kroku v TransferedBytes (uzavrene + jeden "pracovni")

    DWORD LastPacketsSize[TRSPMETER_NUMOFSTOREDPACKETS + 1]; // kruhova fronta s velikosti poslednich N+1 "paketu"
    DWORD LastPacketsTime[TRSPMETER_NUMOFSTOREDPACKETS + 1]; // kruhova fronta s casem prijeti poslednich N+1 "paketu"
    int ActIndexInLastPackets;                               // index v LastPacketsSize a LastPacketsTime pro zapis dalsiho prijateho "paketu" (pri plne fronte je to zaroven index nejstarsiho "paketu")
    int CountOfLastPackets;                                  // pocet "paketu" v LastPacketsSize a LastPacketsTime (pocet platnych zaznamu)
    DWORD MaxPacketSize;                                     // velikost nejvetsiho paketu, ktery muzeme ocekavat

public:
    BOOL ResetSpeed; // TRUE = pravdepodobne by se pred dalsim merenim rychlosti mel resetnout merak (volat JustConnected) - pokles rychlosti byl prilis velky, zobrazovali jsme proto nulovou rychlost

public:
    CTransferSpeedMeter();

    // vynuluje objekt (priprava pro dalsi pouziti)
    // volani mozne z libovolneho threadu
    void Clear();

    // ve 'speed' (nesmi byt NULL) vraci rychlost spojeni v bytech za sekundu
    // volani mozne z libovolneho threadu
    void GetSpeed(CQuadWord* speed);

    // vola se v okamziku, kdy se ma zacit merit rychlost
    // volani mozne z libovolneho threadu
    void JustConnected();

    // vola se po uskutecneni prenosu casti dat; v 'count' je o kolik dat slo; 'time' je
    // casu prenosu; 'maxPacketSize' je maximalni ocekavana velikost dat, ktera prijdou
    // dalsim volanim BytesReceived()
    void BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize);

    // upravi 'progressBufferLimit' podle aktualnich dat o prijatych paketech;
    // 'lastFileBlockCount' je pocet paketu, pres ktery nesmime jit (bereme jen souvisle
    // kopirovani jednoho souboru, 'lastFileBlockCount' je chranene pred pretecenim,
    // pocet > 1000000 znamena "hafo", kolik presne neni dulezite); 'lastFileStartTime'
    // je GetTickCount() z okamziku, kdy jsme zacali kopirovat posledni soubor
    void AdjustProgressBufferLimit(DWORD* progressBufferLimit, DWORD lastFileBlockCount,
                                   DWORD lastFileStartTime);
};

//
// ****************************************************************************
// CProgressSpeedMeter
//
// objekt pro vypocet rychlosti progresu - pro vypocet time-left

#define PRSPMETER_ACTSPEEDSTEP 500         // pro vypocet rychlosti progresu: velikost kroku v milisekundach (nesmi byt 0)
#define PRSPMETER_ACTSPEEDNUMOFSTEPS 60    // pro vypocet rychlosti progresu: pocet pouzitych kroku (vic kroku = jemnejsi zmeny rychlosti pri "vypadku" prvniho kroku ve fronte)
#define PRSPMETER_NUMOFSTOREDPACKETS 100   // pro vypocet rychlosti progresu: kolik poslednich "paketu" se ma pamatovat (pri nizkych rychlostech se z nich pocita rychlost) (nesmi byt 0)
#define PRSPMETER_STPCKTSMININTERVAL 10000 // pro vypocet rychlosti progresu: minimalni doba mezi prijetim prvniho a posledniho ulozeneho "paketu", kdy je lze pouzit pro vypocet rychlosti (nizke rychlosti)

class CProgressSpeedMeter
{
protected:
    // POZOR: pristup do objektu je mozny jen v kriticke sekci COperations::StatusCS

    // vypocet prenosove rychlosti:
    DWORD TransferedBytes[PRSPMETER_ACTSPEEDNUMOFSTEPS + 1]; // kruhova fronta s poctem bytu prenesenych v poslednich N krocich (cas. intervalech) + jeden "pracovni" krok navic (nascitava se v nem hodnota za akt. interval)
    int ActIndexInTrBytes;                                   // index posledniho (aktualniho) zaznamu v TransferedBytes
    DWORD ActIndexInTrBytesTimeLim;                          // casova hranice (v ms) posledniho zaznamu v TransferedBytes (do tohoto casu se nacitaji byty do posl. zaznamu)
    int CountOfTrBytesItems;                                 // pocet kroku v TransferedBytes (uzavrene + jeden "pracovni")

    DWORD LastPacketsSize[PRSPMETER_NUMOFSTOREDPACKETS + 1]; // kruhova fronta s velikosti poslednich N+1 "paketu"
    DWORD LastPacketsTime[PRSPMETER_NUMOFSTOREDPACKETS + 1]; // kruhova fronta s casem prijeti poslednich N+1 "paketu"
    int ActIndexInLastPackets;                               // index v LastPacketsSize a LastPacketsTime pro zapis dalsiho prijateho "paketu" (pri plne fronte je to zaroven index nejstarsiho "paketu")
    int CountOfLastPackets;                                  // pocet "paketu" v LastPacketsSize a LastPacketsTime (pocet platnych zaznamu)
    DWORD MaxPacketSize;                                     // velikost nejvetsiho paketu, ktery muzeme ocekavat

public:
    CProgressSpeedMeter();

    // vynuluje objekt (priprava pro dalsi pouziti)
    // volani mozne z libovolneho threadu
    void Clear();

    // ve 'speed' (nesmi byt NULL) vraci rychlost spojeni v bytech za sekundu
    // volani mozne z libovolneho threadu
    void GetSpeed(CQuadWord* speed);

    // vola se v okamziku, kdy se ma zacit merit rychlost
    // volani mozne z libovolneho threadu
    void JustConnected();

    // vola se po uskutecneni prenosu casti dat; v 'count' je o kolik dat slo; 'time' je
    // casu prenosu; 'maxPacketSize' je maximalni ocekavana velikost dat, ktera prijdou
    // dalsim volanim BytesReceived()
    void BytesReceived(DWORD count, DWORD time, DWORD maxPacketSize);
};

enum COperationCode
{
    ocCopyFile,
    ocMoveFile,
    ocDeleteFile,
    ocCreateDir,
    ocMoveDir,
    ocDeleteDir,
    ocDeleteDirLink,
    ocChangeAttrs, // POZOR: pozadovane atributy jsou ulozeny v TargetName (pouziti bez ohledu na typ, proste DWORD)
    ocCountSize,
    ocConvert,
    ocLabelForSkipOfCreateDir, // znacka, ke ktere se ma skipnout skript pri skipu na ocCreateDirXXX; POZOR: v SourceName a TargetName je LO- a HI-DWORD souctu velikosti souboru (vcetne ADS) obsazenych ve skipovanem adresari; POZOR: v Attr je index ocCreateDirXXX v poli COperations pro skipovany adresar
    ocCopyDirTime,             // Move/Copy: pri filterCriteria->PreserveDirTime==TRUE: kopirovani casu&datumu adresare; POZOR: lastWrite je ulozen v SourceName a Attr (pouziti bez ohledu na typ, proste dva DWORDy)
};

#define OPFL_OVERWROLDERALRTESTED 0x00000001 // test na skipnuti u "overwrite older, skip other existing" uz se delal
#define OPFL_AS_ENCRYPTED 0x00000002         // cilovy soubor/adresar by mel mit nastaveny atribut Encrypted
#define OPFL_COPY_ADS 0x00000004             // zkopirovat i ADS souboru/adresare
#define OPFL_SRCPATH_IS_NET 0x00000008       // zdrojova cesta je sitova
#define OPFL_SRCPATH_IS_FAST 0x00000010      // zdrojova cesta je disk, disk na USB, flashka, flash-card-reader, CD, DVD nebo ram-disk (nejde o: sit a disketu)
#define OPFL_TGTPATH_IS_NET 0x00000020       // cilova cesta je sitova
#define OPFL_TGTPATH_IS_FAST 0x00000040      // cilova cesta je disk, disk na USB, flashka, flash-card-reader, CD, DVD nebo ram-disk (nejde o: sit a disketu)
#define OPFL_IGNORE_INVALID_NAME 0x00000080  // skipnout test na validitu jmena (pouziva se u adresaru: nemenili jsme nazev = nerveme, ze je invalidni)

struct COperation
{
    COperationCode Opcode;
    CQuadWord Size;
    CQuadWord FileSize; // velikost souboru, platne jen pro ocCopyFile a ocMoveFile
    char *SourceName,
        *TargetName;
    DWORD Attr;
    DWORD OpFlags; // kombinace OPFL_xxx, viz vyse
};

class COperations : public TDirectArray<COperation>
{
public:
    CQuadWord TotalSize;      // POZOR: neni velikost souboru v bytech (je zde velikost pouzitelna jen pro progress)
    CQuadWord CompressedSize; // soucet velikosti souboru po kompresi
    CQuadWord OccupiedSpace;  // zabrane misto na disku
    CQuadWord TotalFileSize;  // soucet velikosti souboru na disku
    CQuadWord FreeSpace;      // volne misto na disku (copy, move) pro kontrolu
    DWORD BytesPerCluster;    // pro vypocet obsazeneho mista

    // velikosti jednotlivych souboru pro odhad pri zadane velikosti clusteru
    TDirectArray<CQuadWord> Sizes;

    DWORD ClearReadonlyMask; // pro automaticke cisteni read-only flagu z CD-ROMu
    BOOL InvertRecycleBin;   // invertovat pouziti RecycleBinu

    int FilesCount;
    int DirsCount;

    const char* RemapNameFrom; // jen pro vypisy na obrazovku:
    int RemapNameFromLen;      // mapovani jmen pro MoveFiles (From -> To)
    const char* RemapNameTo;
    int RemapNameToLen;

    BOOL RemovableTgtDisk;      // jde o zapis na vymenne medium?
    BOOL RemovableSrcDisk;      // jde o cteni z vymenneho media?
    BOOL CanUseRecycleBin;      // lze pouzivat Recycle Bin? (jen local-fixed-drives)
    BOOL SameRootButDiffVolume; // TRUE pokud jde o Move mezi cestami se stejnym rootem, ale ruznymi svazky (aspon jedna cesta s junction-pointem)
    BOOL TargetPathSupADS;      // TRUE pokud cil kopirovani/presouvani podporuje ADS (je nutne mazat ADSka souboru (nebo cele soubory) pred prepisem)
                                //    BOOL TargetPathSupEFS;       // TRUE pokud cil kopirovani/presouvani podporuje EFS (aneb trochu mene obecne: je NTFS a ne FAT)

    // pro Copy/Move operace
    BOOL IsCopyOrMoveOperation; // TRUE = jde o Copy/Move operaci (budeme ji pridavat do fronty diskovych Copy/Move operaci)
    BOOL OverwriteOlder;        // prepsat starsi a preskocit novejsi bez ptani
    BOOL CopySecurity;          // zachovat NTFS prava, FALSE = don't care = nic se nema extra resit, na vysledku nam nezalezi
    BOOL CopyAttrs;             // zachovat Archive, Encrypt a Compress atributy, FALSE = don't care = nic se nema extra resit, na vysledku nam nezalezi
    BOOL PreserveDirTime;       // zachovat datumy a casy adresaru (pouziva se pri Move: detekujeme jestli se nahodou nemeni cas, pokud ano, opravujeme ho "rucne", dela napr. na Sambe)
    BOOL StartOnIdle;           // ma se spustit az nic jineho nepobezi
    BOOL SourcePathIsNetwork;   // TRUE = zdrojova cesta je sitova (UNC nebo mapovany disk)

    // pro status radek v progress dialogu (jen Copy a Move)
    BOOL ShowStatus;       // ma se pod druhym progress-barem zobrazovat status operace (rychlost kopirovani, atd.)
    BOOL IsCopyOperation;  // TRUE = copy, FALSE = move
    BOOL FastMoveUsed;     // dela se "rename" aspon jednoho souboru nebo adresare? (pak nema smysl zobrazovat celkovou velikost presouvanych dat)
    BOOL ChangeSpeedLimit; // TRUE = bude se mozna menit speed-limit (worker by mel dobehnout do stavu, kde je to snadne)

    BOOL SkipAllCountSizeErrors; // maji se skipnout vsechny dalsi count-size-errory?

    char WorkPath1[MAX_PATH];  // jde-li o neprazdny retezec, je to prvni cesta, na ktere se pracovalo (pouziva se pro hlaseni zmen)
    BOOL WorkPath1InclSubDirs; // TRUE/FALSE = vcetne/bez podadresaru (prvni cesta)
    char WorkPath2[MAX_PATH];  // jde-li o neprazdny retezec, je to druha cesta, na ktere se pracovalo (pouziva se pro hlaseni zmen)
    BOOL WorkPath2InclSubDirs; // TRUE/FALSE = vcetne/bez podadresaru (druha cesta)

    char* WaitInQueueSubject; // text pro stav "waiting in queue": titulek dialogu
    char* WaitInQueueFrom;    // text pro stav "waiting in queue": horni radek (From)
    char* WaitInQueueTo;      // text pro stav "waiting in queue": dolni radek (To)

private:
    // pro status radek v progress dialogu (jen Copy a Move)
    CRITICAL_SECTION StatusCS;              // kriticka sekce pro pristup k TransferSpeedMeter, ProgressSpeedMeter a
    CTransferSpeedMeter TransferSpeedMeter; // merak pro prenos dat (Read/WriteFile)
    CProgressSpeedMeter ProgressSpeedMeter; // merak pro vypocet "time left" (meri i rychlosti vytvareni adresaru, kopirovani prazdnych souboru, atd. - pracuje se stejnymi velikostmi operaci jako progress)
    CQuadWord TransferredFileSize;          // kolik bytu uz bylo realne prekopirovano/preneseno (konecny soucet by mel vyjit TotalFileSize, ovsem pokud se nezmeni data na disku)
    CQuadWord ProgressSize;                 // progres vyjadreny v prekopirovanych/prenesenych "bytech" (pracuje se stejnymi velikostmi operaci jako progress)

    // data pro speed-limit, pouzivaji se jen v sekci StatusCS
    BOOL UseSpeedLimit;             // TRUE = pouzivame speed-limit
    DWORD SpeedLimit;               // hodnota speed-limitu (v bytech za vterinu), POZOR: nikdy nesmi byt nula!
    DWORD SleepAfterWrite;          // kolik ms se ma cekat po paketu o velikosti LastBufferLimit; -1 = hodnotu je potreba vypocitat (po prvnim paketu)
    int LastBufferLimit;            // velikost paketu, POZOR: nikdy nesmi byt nula!
    DWORD LastSetupTime;            // GetTickCount() z okamziku posledniho vypoctu parametru speed-limitu + pripadneho dobrzdeni
    CQuadWord BytesTrFromLastSetup; // kolik bytu bylo preneseno od okamziku LastSetupTime

    // jen pro asynchroni kopirovani: data pro omezovac velikosti bufferu (aby se hybal progress, nesmi byt buffer moc velky), pouzivaji se jen v sekci StatusCS
    BOOL UseProgressBufferLimit;  // TRUE = ma se pouzivat omezovac velikosti bufferu (asynchroni kopirovani)
    DWORD ProgressBufferLimit;    // limit velikosti bufferu pro kopirovani, zajistuje rozumnou frekvenci udaju pro progres
    DWORD LastProgBufLimTestTime; // GetTickCount() z okamziku posledniho testu velikosti ProgressBufferLimit
    DWORD LastFileBlockCount;     // kolik bloku uz se prekopirovalo od zacatku posledniho souboru (POZOR: je chranene pred pretecenim, pocet > 1000000 znamena "hafo", kolik presne neni dulezite)
    DWORD LastFileStartTime;      // GetTickCount() z okamziku, kdy jsme zacali kopirovat posledni soubor

public:
    COperations(int base, int delta, char* waitInQueueSubject, char* waitInQueueFrom, char* waitInQueueTo);
    ~COperations() { HANDLES(DeleteCriticalSection(&StatusCS)); }

    void SetWorkPath1(const char* path, BOOL inclSubDirs)
    {
        lstrcpyn(WorkPath1, path, MAX_PATH);
        WorkPath1InclSubDirs = inclSubDirs;
    }

    void SetWorkPath2(const char* path, BOOL inclSubDirs)
    {
        lstrcpyn(WorkPath2, path, MAX_PATH);
        WorkPath2InclSubDirs = inclSubDirs;
    }

    void SetTFS(const CQuadWord& TFS);
    void SetTFSandProgressSize(const CQuadWord& TFS, const CQuadWord& pSize,
                               int* limitBufferSize = NULL, int bufferSize = 0);
    void AddBytesToSpeedMetersAndTFSandPS(DWORD bytesCount, BOOL onlyToProgressSpeedMeter,
                                          int bufferSize, int* limitBufferSize = NULL,
                                          DWORD maxPacketSize = 0);
    void GetNewBufSize(int* limitBufferSize, int bufferSize);
    void AddBytesToTFSandSetProgressSize(const CQuadWord& bytesCount, const CQuadWord& pSize);
    void AddBytesToTFS(const CQuadWord& bytesCount);
    void GetTFS(CQuadWord* TFS);
    void GetTFSandResetTrSpeedIfNeeded(CQuadWord* TFS);
    void SetProgressSize(const CQuadWord& pSize);
    void CalcLimitBufferSize(int* limitBufferSize, int bufferSize);

    void EnableProgressBufferLimit(BOOL useProgressBufferLimit);
    void SetFileStartParams();

    void GetStatus(CQuadWord* transferredFileSize, CQuadWord* transferSpeed,
                   CQuadWord* progressSize, CQuadWord* progressSpeed,
                   BOOL* useSpeedLimit, DWORD* speedLimit);
    void InitSpeedMeters(BOOL operInProgress);
    BOOL GetTFSandProgressSize(CQuadWord* transferredFileSize, CQuadWord* progressSize);

    void SetSpeedLimit(BOOL useSpeedLimit, DWORD speedLimit);
    void GetSpeedLimit(BOOL* useSpeedLimit, DWORD* speedLimit);
};

class COperationsQueue // fronta diskovych Copy/Move operaci
{
protected:
    CRITICAL_SECTION QueueCritSect; // kriticka sekce objektu

    // pole OperDlgs a OperPaused maji stejny pocet prvku a stejne indexovani (jedna operace ma jeden index v obou polich)
    TDirectArray<HWND> OperDlgs;    // pole typu HWND: dialogy operaci ve fronte
    TDirectArray<DWORD> OperPaused; // pole typu int: stav operace ve fronte: 2/1/0 = "manually-paused"/"auto-paused"/"running"

public:
    COperationsQueue() : OperDlgs(5, 10), OperPaused(5, 10)
    {
        HANDLES(InitializeCriticalSection(&QueueCritSect));
    }
    ~COperationsQueue()
    {
        if (OperDlgs.Count > 0 || OperPaused.Count > 0)
            TRACE_E("~COperationsQueue(): unexpected situation: operation queue is not empty!");
        HANDLES(DeleteCriticalSection(&QueueCritSect));
    }

    // prida operaci do fronty; vraci TRUE pri uspechu, jinak se pridani nepodarilo (malo pameti);
    // 'dlg' je handle okna dialogu operace; 'startOnIdle' je TRUE pokud ma dojit ke spusteni
    // operace az nic jineho nepobezi; ve 'startPaused' (nesmi byt NULL) vraci TRUE pokud
    // se ma pridana operace spustit v "paused" rezimu, jinak se spousti v "running" rezimu
    BOOL AddOperation(HWND dlg, BOOL startOnIdle, BOOL* startPaused);

    // vyhodi operaci z fronty (operace se dokoncila); je-li 'doNotResume' FALSE, postne
    // "resume" prvni operace ve fronte v pripade, ze vsechny operace z fronty jsou "paused";
    // neni-li 'foregroundWnd' NULL, ulozi se do nej handle dialogu operace, ktery je potreba
    // aktivovat (pokud neni potreba nic aktivovat, hodnota se nemeni)
    void OperationEnded(HWND dlg, BOOL doNotResume, HWND* foregroundWnd);

    // nastavi operaci 'dlg' stav na 'paused' (2/1/0 = "manually-paused"/"auto-paused"/"running")
    void SetPaused(HWND dlg, BOOL paused);

    // presune operaci 'dlg' na konec seznamu + nastavi ji stav na "auto-paused"
    void AutoPauseOperation(HWND dlg, HWND* foregroundWnd);

    // vraci TRUE pokud ve fronte neni zadna operace
    BOOL IsEmpty();

    // vraci aktualni pocet operaci ve fronte
    int GetNumOfOperations();
};

extern COperationsQueue OperationsQueue; // fronta diskovych Copy/Move operaci

HANDLE StartWorker(COperations* script, HWND hDlg, CChangeAttrsData* attrsData,
                   CConvertData* convertData, HANDLE wContinue, HANDLE workerNotSuspended,
                   BOOL* cancelWorker, int* operationProgress, int* summaryProgress);

void FreeScript(COperations* script);

//
// File information classes and Io Status block (see NTDDK.H)
//
typedef enum _FILE_INFORMATION_CLASS
{
    FileDirectoryInformation = 1,
    FileFullDirectoryInformation, // 2
    FileBothDirectoryInformation, // 3
    FileBasicInformation,         // 4  wdm
    FileStandardInformation,      // 5  wdm
    FileInternalInformation,      // 6
    FileEaInformation,            // 7
    FileAccessInformation,        // 8
    FileNameInformation,          // 9
    FileRenameInformation,        // 10
    FileLinkInformation,          // 11
    FileNamesInformation,         // 12
    FileDispositionInformation,   // 13
    FilePositionInformation,      // 14 wdm
    FileFullEaInformation,        // 15
    FileModeInformation,          // 16
    FileAlignmentInformation,     // 17
    FileAllInformation,           // 18
    FileAllocationInformation,    // 19
    FileEndOfFileInformation,     // 20 wdm
    FileAlternateNameInformation, // 21
    FileStreamInformation,        // 22
    FilePipeInformation,          // 23
    FilePipeLocalInformation,     // 24
    FilePipeRemoteInformation,    // 25
    FileMailslotQueryInformation, // 26
    FileMailslotSetInformation,   // 27
    FileCompressionInformation,   // 28
    FileObjectIdInformation,      // 29
    FileCompletionInformation,    // 30
    FileMoveClusterInformation,   // 31
    FileQuotaInformation,         // 32
    FileReparsePointInformation,  // 33
    FileNetworkOpenInformation,   // 34
    FileAttributeTagInformation,  // 35
    FileTrackingInformation,      // 36
    FileMaximumInformation
} FILE_INFORMATION_CLASS,
    *PFILE_INFORMATION_CLASS;

typedef struct _IO_STATUS_BLOCK
{
    union
    {
        NTSTATUS Status;
        PVOID Pointer;
    };
    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

typedef NTSTATUS(__stdcall* NTQUERYINFORMATIONFILE)(
    IN HANDLE FileHandle,
    OUT PIO_STATUS_BLOCK IoStatusBlock,
    OUT PVOID FileInformation,
    IN ULONG Length,
    IN FILE_INFORMATION_CLASS FileInformationClass);

extern NTQUERYINFORMATIONFILE DynNtQueryInformationFile;

typedef VOID(__stdcall* PIO_APC_ROUTINE)(
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved);

typedef NTSTATUS(__stdcall* NTFSCONTROLFILE)(
    _In_ HANDLE FileHandle,
    _In_opt_ HANDLE Event,
    _In_opt_ PIO_APC_ROUTINE ApcRoutine,
    _In_opt_ PVOID ApcContext,
    _Out_ PIO_STATUS_BLOCK IoStatusBlock,
    _In_ ULONG FsControlCode,
    _In_opt_ PVOID InputBuffer,
    _In_ ULONG InputBufferLength,
    _Out_opt_ PVOID OutputBuffer,
    _In_ ULONG OutputBufferLength);

extern NTFSCONTROLFILE DynNtFsControlFile;

//
// MessageId: STATUS_BUFFER_OVERFLOW
//
// MessageText:
//
//  {Buffer Overflow}
//  The data was too large to fit into the specified buffer.
//
#define STATUS_BUFFER_OVERFLOW ((NTSTATUS)0x80000005L)

#pragma pack(4)
typedef struct
{
    ULONG NextEntry;
    ULONG NameLength;
    LARGE_INTEGER Size;
    LARGE_INTEGER AllocationSize;
    USHORT Name[1];
} FILE_STREAM_INFORMATION, *PFILE_STREAM_INFORMATION;
#pragma pack()

// zjisti jak je to s alternate-data-streamy (ADS) souboru/adresare ('isDir' je FALSE/TRUE)
// 'fileName', ma smysl volat jen na NTFS discich; neni-li 'adsSize' NULL, vraci se v nem
// soucet velikosti vsech ADS; neni-li 'streamNames' NULL, vraci se v nem alokovane pole
// Unicodovych jmen vsech ADS (krome defaultniho ADS) - prvky pole jsou alokovane, volajici
// musi zajistit jejich dealokaci i dealokaci samotneho pole - pole jmen se vraci jen
// pokud nenastala zadna chyba (viz 'lowMemory' a 'winError') a zaroven byly nalezeny ADS
// (funkce vraci TRUE); neni-li 'streamNamesCount' NULL, vraci se v nem pocet prvku v poli
// 'streamNames'; neni-li 'lowMemory' NULL, vraci se v nem TRUE pokud dojde k chybe diky
// nedostatku pameti (muze vratit TRUE jen pokud neni 'streamNames' NULL); neni-li
// 'winError' NULL, vraci se v nem kod windowsove chyby (NO_ERROR v pripade, ze zadna
// nenastala - pokud nastane windows chyba, funkce vzdy vraci FALSE); funkce vraci TRUE
// pokud soubor/adresar obsahuje nejake ADS, jinak vraci FALSE; 'bytesPerCluster' je
// velikost clusteru pro vypocet mista na disku zabraneho ADS (0 = neznama velikost);
// v 'adsOccupiedSpace' (neni-li NULL) vraci velikost mista na disku zabraneho ADS;
// v 'onlyDiscardableStreams'  (neni-li NULL) vraci TRUE pokud byly nalezeny jen
// ADS, ktere se muzou bez dotazu zahodit (zatim jen thumbnaily z W2K)
BOOL CheckFileOrDirADS(const char* fileName, BOOL isDir, CQuadWord* adsSize, wchar_t*** streamNames,
                       int* streamNamesCount, BOOL* lowMemory, DWORD* winError,
                       DWORD bytesPerCluster, CQuadWord* adsOccupiedSpace,
                       BOOL* onlyDiscardableStreams);
