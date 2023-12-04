// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

extern HWND ProgressDialogActivateDrop;

//
// ****************************************************************************
// CZIPUnpackProgress
//

#define ZIP_UNPACK_NUMLINES 5

class CProgressBar;
class CStaticText;

class CZIPUnpackProgress : public CCommonDialog
{
protected:
    const char* RemapNameFrom; // mapovani jmen z tmp-adresare
    const char* RemapNameTo;   // na jmeno archivu, ze ktereho vybalujeme
    BOOL FileProgress;         // pro variantu s jednim progressem: TRUE="File:", FALSE="Total:"

public:
    CZIPUnpackProgress();
    CZIPUnpackProgress(const char* title, HWND parent, const CQuadWord& totalSize, CITaskBarList3* taskBarList3);

    void Init();

    void Set(const char* title, HWND parent, const CQuadWord& totalSize, BOOL fileProgress);
    void Set(const char* title, HWND parent, const CQuadWord& totalSize1, const CQuadWord& totalSize2);
    void SetTotal(const CQuadWord& total1, const CQuadWord& total2); // CQuadWord(-1, -1) znamena nenastavovat

    int AddSize(int size, BOOL delayedPaint);                                       // vraci "continue?"
    int SetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint); // vraci "continue?", size == CQuadWord(-1, -1) znamena "nenastavovat"

    void NewLine(const char* txt, BOOL delayedPaint);
    void EnableCancel(BOOL enable);

    void SetRemapNames(const char* nameFrom, const char* nameTo);
    void DoRemapNames(char* txt, int bufLen);

    void SetTaskBarList3(CITaskBarList3* taskBarList3);

protected:
    virtual INT_PTR DialogProc(UINT uMsg, WPARAM wParam, LPARAM lParam);

    BOOL HasTwoProgress(); // TRUE - dvou-progressovy, jinak FALSE

    void DispatchMessages(); // rozesle zpravy z fronty, dava prilezitost k prekreslni, klikani na tlacitko

    void FlushDataToControls(); // dirty data prenese do controlu (texty, progress bary)

    const char* Title;   // titulek - ukazatel do bufferu LoadStr (nepouzivat dlouho)
    BOOL Cancel;         // uzivatel stornoval operaci, dialog by mel co nejdrive koncit
    DWORD LastTickCount; // pro detekci ze uz je treba prekreslit zmenena data

    // pozor: Summary, Summary2 a Lines budou v pripade nedostatku pameti NULL; pocitat s tim v kodu dialogu
    CProgressBar* Summary;
    CProgressBar* Summary2;
    CStaticText* Lines[ZIP_UNPACK_NUMLINES];

    char LinesCache[ZIP_UNPACK_NUMLINES][300]; // sem se uladaji texty, ktere se pozdeji zobrazi
    int CacheIndex;                            // index do pole LinesCache, ktery se ma obsadit pristi radkou
    BOOL CacheIsDirty;                         // je treba prenest cache do obrazovky?

    CQuadWord TotalSize,
        ActualSize;
    CQuadWord TotalSize2,
        ActualSize2;
    BOOL SizeIsDirty;  // je treba prenest Size do obrazovky?
    BOOL Size2IsDirty; // je treba prenest Size2 do obrazovky?

    CITaskBarList3* TaskBarList3; // ukazatel na interface patrici hlavnimu oknu Salamandera
};

//
// ****************************************************************************
// CSalamanderForOperations
//

class CSalamanderForOperations : public CSalamanderForOperationsAbstract
{
protected:
    CFilesWindow* Panel;
    CZIPUnpackProgress UnpackProgress; // UnpackProgress dialog
    BOOL ProgressDialog2;              // TRUE = dvou-progressovy, FALSE = jedno-progressovy UnpackProgress dialog
    HWND FocusWnd;

    // kontrolni mechanismus
    DWORD ThreadID; // smeji nas volat pouze z threadu, ve kterem byl ukazatel na nas predan
    BOOL Destroyed; // pokud je TRUE, objek jiz byl destruovan

public:
    CSalamanderForOperations(CFilesWindow* panel);
    ~CSalamanderForOperations();

    // PROGRESS DIALOG: dialog obsahuje jeden/dva ('twoProgressBars' FALSE/TRUE) progress-metry
    // otevre progress-dialog s titulkem 'title'; 'parent' je parent okno progress-dialogu (je-li
    // NULL, pouzije se hlavni okno); pokud obsahuje jen jeden progress-metr, muze byt popsan
    // jako "File" ('fileProgress' je TRUE) nebo "Total" ('fileProgress' je FALSE)
    virtual void WINAPI OpenProgressDialog(const char* title, BOOL twoProgressBars, HWND parent, BOOL fileProgress);
    // vypise text 'txt' (i nekolik radku - provadi se rozpad na radky) do progress-dialogu
    virtual void WINAPI ProgressDialogAddText(const char* txt, BOOL delayedPaint);
    // neni-li 'totalSize1' CQuadWord(-1, -1), nastavi 'totalSize1' jako 100 procent prvniho progress-metru,
    // neni-li 'totalSize2' CQuadWord(-1, -1), nastavi 'totalSize2' jako 100 procent druheho progress-metru
    // (pro progress-dialog s jednim progress-metrem je povinne 'totalSize2' CQuadWord(-1, -1))
    virtual void WINAPI ProgressSetTotalSize(const CQuadWord& totalSize1, const CQuadWord& totalSize2);
    // neni-li 'size1' CQuadWord(-1, -1), nastavi velikost 'size1' (size1/total1*100 procent) na prvnim progress-metru,
    // neni-li 'size2' CQuadWord(-1, -1), nastavi velikost 'size2' (size2/total2*100 procent) na druhem progress-metru
    // (pro progress-dialog s jednim progress-metrem je povinne 'size2' CQuadWord(-1, -1)), vraci informaci jestli ma
    // akce pokracovat (FALSE = konec)
    virtual BOOL WINAPI ProgressSetSize(const CQuadWord& size1, const CQuadWord& size2, BOOL delayedPaint);
    // prida (pripadne k oboum progress-metrum) velikost 'size' (size/total*100 procent progressu),
    // vraci informaci jestli ma akce pokracovat (FALSE = konec)
    virtual BOOL WINAPI ProgressAddSize(int size, BOOL delayedPaint);
    // enabluje/disabluje tlacitko Cancel
    virtual void WINAPI ProgressEnableCancel(BOOL enable);
    // vraci HWND dialogu progressu (hodi se pri vypisu chyb a dotazu pri otevrenem progress-dialogu)
    virtual HWND WINAPI ProgressGetHWND() { return UnpackProgress.HWindow; }
    // zavre progress-dialog
    virtual void WINAPI CloseProgressDialog();

    // presune vsechny soubory ze 'source' adresare do 'target' adresare,
    // navic premapovava predpony zobrazovanych jmen ('remapNameFrom' -> 'remapNameTo')
    // vraci uspech operace
    virtual BOOL WINAPI MoveFiles(const char* source, const char* target, const char* remapNameFrom,
                                  const char* remapNameTo);
};

//
// ****************************************************************************
// CSalamanderDirectory
//

class CSalamanderDirectory;

// CSalamanderDirectoryAddCache slouzi pro optimalizaci pridavani souboru
// do CSalamanderDirectory (metoda AddFile)
struct CSalamanderDirectoryAddCache
{
    int PathLen;               // pocet platnych znaku v 'Path'
    char Path[MAX_PATH];       // cachovana cesta
    CSalamanderDirectory* Dir; // ukazatel na CSalamanderDirectory, do ktereho jsou pridavany soubory a adresare s cestou 'Path'
};

class CSalamanderDirectory : public CSalamanderDirectoryAbstract
{
protected:
    CFilesArray Dirs;                              // jmena podadresaru (obsah je v SalamDirs na stejnem indexu)
    TDirectArray<CSalamanderDirectory*> SalamDirs; // ukazatele na CSalamanderDirectory (ukazatele jsou NULL az do okamziku prvniho pristupu, pak se teprve alokuji objekty)
    CFilesArray Files;                             // jmena souboru
    DWORD ValidData;                               // maska platnosti dat z CFileData
    DWORD Flags;                                   // priznaky objektu (viz SALDIRFLAG_XXX)
    BOOL IsForFS;                                  // TRUE jde-li o sal-dir pro FS, FALSE jde-li o sal-dir pro archivy
    CSalamanderDirectoryAddCache* AddCache;        // pokud je ruzny od NULL, slouzi k optimalizaci pridavani souboru metodou AddFile; jinak se nepouziva

public:
    CSalamanderDirectory(BOOL isForFS, DWORD validData = VALID_DATA_ALL_FS_ARC, DWORD flags = -1 /* nastavi se podle isForFS */);
    ~CSalamanderDirectory();

    // *********************************************************************************
    // metody rozhrani CSalamanderDirectoryAbstract
    // *********************************************************************************
    virtual void WINAPI Clear(CPluginDataInterfaceAbstract* pluginData);
    virtual void WINAPI SetValidData(DWORD validData);
    virtual void WINAPI SetFlags(DWORD flags);
    virtual BOOL WINAPI AddFile(const char* path, CFileData& file, CPluginDataInterfaceAbstract* pluginData);
    virtual BOOL WINAPI AddDir(const char* path, CFileData& dir, CPluginDataInterfaceAbstract* pluginData);

    virtual int WINAPI GetFilesCount() const;
    virtual int WINAPI GetDirsCount() const;
    virtual CFileData const* WINAPI GetFile(int i) const;
    virtual CFileData const* WINAPI GetDir(int i) const;
    virtual CSalamanderDirectoryAbstract const* WINAPI GetSalDir(int i) const;
    virtual void WINAPI SetApproximateCount(int files, int dirs);

    // *********************************************************************************
    // pomocne metody (nepristupne z pluginu)
    // *********************************************************************************

    // pro optimalizaci metody AddFile
    void AllocAddCache();
    void FreeAddCache();

    // podle Flags bud StrICmp nebo strcmp (StrCmpEx) - rozliseni case sensitive/insensitive porovnani
    int SalDirStrCmp(const char* s1, const char* s2);
    int SalDirStrCmpEx(const char* s1, int l1, const char* s2, int l2);

    // vola pro vsechny soubory (je-li 'releaseFiles' TRUE) a pro vsechny adresare
    // (je-li 'releaseDirs' TRUE) 'pluginData'.ReleaseFilesOrDirs (uvolneni dat plug-inu)
    void ReleasePluginData(CPluginDataInterfaceEncapsulation& pluginData, BOOL releaseFiles,
                           BOOL releaseDirs);

    // vraci adresare ze zadane cesty (relativni k tomuto salamander-adresari)
    CFilesArray* GetDirs(const char* path);
    // vraci soubory ze zadane cesty (relativni k tomuto salamander-adresari)
    CFilesArray* GetFiles(const char* path);

    // vraci nadrazeny adresar pro cestu 'path' (pro root a nezname cesty vraci NULL)
    const CFileData* GetUpperDir(const char* path);

    // vraci soucet velikosti vsech obsazenych souboru, pozor: nutne vynulovat pocitadla
    CQuadWord GetSize(int* dirsCount = NULL, int* filesCount = NULL, TDirectArray<CQuadWord>* sizes = NULL);
    // vraci velikost adresare - soucet vsech souboru v nem, pozor: nutne vynulovat pocitadla
    CQuadWord GetDirSize(const char* path, const char* dirName, int* dirsCount = NULL,
                         int* filesCount = NULL, TDirectArray<CQuadWord>* sizes = NULL);
    // vraci salamander-dir pro zadany adresar; je-li 'readOnly' TRUE, nesmi dojit k zapisu
    // do vraceneho salamander-dir objektu
    CSalamanderDirectory* GetSalamanderDir(const char* path, BOOL readOnly);
    // vraci salamander-dir pro zadany index adresare; nesmi dojit k zapisu do vraceneho
    // salamander-dir objektu
    CSalamanderDirectory* GetSalamanderDir(int i);
    // vraci index adresare zadaneho jmenem
    int GetIndex(const char* dir);
    // je na tomto indexu adresar?
    BOOL IsDirectory(int i) { return i >= 0 && i < Dirs.Count; }
    // je na tomto indexu soubor?
    BOOL IsFile(int i) { return i >= Dirs.Count && i < Dirs.Count + Files.Count; }
    // vraci soubor pro zadany index
    CFileData* GetFileEx(int i)
    {
        if (i >= Dirs.Count && i < Dirs.Count + Files.Count)
            return &Files[i - Dirs.Count];
        else
            return NULL;
    }
    // vraci adresar pro zadany index
    CFileData* GetDirEx(int i)
    {
        if (i >= 0 && i < Dirs.Count)
            return &Dirs[i];
        else
            return NULL;
    }

    DWORD GetValidData() { return ValidData; }

    DWORD GetFlags() { return Flags; }

protected:
    // pomocna metoda: alokuje objekt salamader-dir na indexu 'index' v poli SalamDirs,
    // vraci ukazatel na objekt (nebo NULL pri chybe)
    CSalamanderDirectory* AllocSalamDir(int index);

    BOOL FindDir(const char* path, const char*& s, int& i, const CFileData& file,
                 CPluginDataInterfaceAbstract* pluginData, const char* archivePath);

    // metody AddFileInt a AddDirInt vraceji v pripade uspechu ukazatel na CSalamanderDirectory,
    // do ktereho byla polozka pridana; jinak vraceji NULL
    CSalamanderDirectory* AddFileInt(const char* path, CFileData& file,
                                     CPluginDataInterfaceAbstract* pluginData,
                                     const char* archivePath);
    CSalamanderDirectory* AddDirInt(const char* path, CFileData& dir,
                                    CPluginDataInterfaceAbstract* pluginData,
                                    const char* archivePath);
};

// zjisti volne misto na ceste path a pokud neni >= totalSize zepta se jestli chce user pokracovat
BOOL TestFreeSpace(HWND parent, const char* path, const CQuadWord& totalSize, const char* messageTitle);

//
// ****************************************************************************
// CPackerConfig
//

// typ polozky v tabulce custom pakovacu
struct SPackCustomPacker
{
    const char* CopyArgs[2];
    const char* MoveArgs[2];
    int Title[2];
    const char* Ext;
    BOOL SupLN;
    BOOL Ansi;
    const char* Exe;
};

// typ polozky v tabulce custom rozpakovavacu
struct SPackCustomUnpacker
{
    const char* Args;
    int Title;
    const char* Ext;
    BOOL SupLN;
    BOOL Ansi;
    const char* Exe;
};

// tabulky custom pakovacu
extern SPackCustomPacker CustomPackers[];
extern SPackCustomUnpacker CustomUnpackers[];

#define CUSTOMPACKER_EXTERNAL 0

class CPackerConfigData
{
public:
    char* Title; // jmeno pro usera
    char* Ext;   // standardni pripona (bez tecky)
    int Type;    // interni (-1, -2, ...; popis viz CPlugins)/externi (0; platnost dalsich polozek)
                 // dodatek viz OldType nize

    // data pro externi pakovace
    char* CmdExecCopy;
    char* CmdArgsCopy;
    BOOL SupportMove;
    char* CmdExecMove;
    char* CmdArgsMove;
    BOOL SupportLongNames;
    BOOL NeedANSIListFile;

    // pomocna promenna pro zjisteni typu udaju - TRUE = stare -> 'Type' (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL OldType;

public:
    CPackerConfigData()
    {
        Empty();
    }

    ~CPackerConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (Ext != NULL)
            free(Ext);
        if (OldType && Type == 1 ||
            !OldType && Type == CUSTOMPACKER_EXTERNAL)
        {
            if (CmdExecCopy != NULL)
                free(CmdExecCopy);
            if (CmdArgsCopy != NULL)
                free(CmdArgsCopy);
            if (CmdExecMove != NULL)
                free(CmdExecMove);
            if (CmdArgsMove != NULL)
                free(CmdArgsMove);
        }
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Title = NULL;
        Ext = NULL;
        Type = 1;
        CmdExecCopy = NULL;
        CmdArgsCopy = NULL;
        SupportMove = FALSE;
        CmdExecMove = NULL;
        CmdArgsMove = NULL;
        SupportLongNames = FALSE;
        NeedANSIListFile = FALSE;
    }

    BOOL IsValid()
    {
        if (Title == NULL || Ext == NULL)
            return FALSE;
        if ((OldType && Type == 1 || !OldType && Type == CUSTOMPACKER_EXTERNAL) &&
            (CmdExecCopy == NULL || CmdArgsCopy == NULL))
            return FALSE;
        if (SupportMove && (CmdExecMove == NULL || CmdArgsMove == NULL))
            return FALSE;
        return TRUE;
    }
};

class CPackerConfig
{
public:
    BOOL Move; // move nebo copy do archivu?

protected:
    int PreferedPacker;
    TIndirectArray<CPackerConfigData> Packers; // pole informaci o pakovacich, polozky typu (CPackerConfigData *)

public:
    CPackerConfig(/*BOOL disableDefaultValues = FALSE*/);
    void InitializeDefaultValues(); // j.r. nahrazuje puvodni volani konstruktoru
    BOOL Load(CPackerConfig& src);

    void DeleteAllPackers() { Packers.DestroyMembers(); }

    int AddPacker(BOOL toFirstIndex = FALSE); // vrati index zalozene polozky nebo -1 pri chybe
    void AddDefault(int SalamVersion);        // prida archivery nove od verze SalamVersion

    // nastavi atributy; kdyz se neco posere, vyradi prvek z pole, zdestroji ho a vrati FALSE
    // old == TRUE -> 'type' je ve stare konvenci (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL SetPacker(int index, int type, const char* title, const char* ext, BOOL old,
                   BOOL supportLongNames = FALSE, BOOL supportMove = FALSE,
                   const char* cmdExecCopy = NULL, const char* cmdArgsCopy = NULL,
                   const char* cmdExecMove = NULL, const char* cmdArgsMove = NULL,
                   BOOL needANSIListFile = FALSE);
    BOOL SetPackerTitle(int index, const char* title);
    void SetPackerType(int index, int type) { Packers[index]->Type = type; }
    void SetPackerOldType(int index, BOOL oldType) { Packers[index]->OldType = oldType; }
    void SetPackerSupMove(int index, BOOL supMove) { Packers[index]->SupportMove = supMove; }
    int GetPackersCount() { return Packers.Count; } // vrati pocet polozek v poli
                                                    //    BOOL SwapPackers(int index1, int index2);         // prohodi dve polozky v poli
    BOOL MovePacker(int srcIndex, int dstIndex);    // posune polozku
    void DeletePacker(int index);
    void SetPackerCmdExecCopy(int index, const char* cmd)
    {
        if (Packers[index]->CmdExecCopy)
            free(Packers[index]->CmdExecCopy);
        Packers[index]->CmdExecCopy = DupStr(cmd);
    }
    void SetPackerCmdExecMove(int index, const char* cmd)
    {
        if (Packers[index]->CmdExecMove)
            free(Packers[index]->CmdExecMove);
        Packers[index]->CmdExecMove = DupStr(cmd);
    }

    int GetPackerType(int index) { return Packers[index]->Type; }
    BOOL GetPackerOldType(int index) { return Packers[index]->OldType; }
    const char* GetPackerTitle(int index) { return Packers[index]->Title; }
    const char* GetPackerExt(int index) { return Packers[index]->Ext; }
    BOOL GetPackerSupLongNames(int index) { return Packers[index]->SupportLongNames; }
    BOOL GetPackerSupMove(int index) { return Packers[index]->SupportMove; }
    const char* GetPackerCmdExecCopy(int index) { return Packers[index]->CmdExecCopy; }
    const char* GetPackerCmdArgsCopy(int index) { return Packers[index]->CmdArgsCopy; }
    const char* GetPackerCmdExecMove(int index) { return Packers[index]->CmdExecMove; }
    const char* GetPackerCmdArgsMove(int index) { return Packers[index]->CmdArgsMove; }
    BOOL GetPackerNeedANSIListFile(int index) { return Packers[index]->NeedANSIListFile; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);

    int GetPreferedPacker() // vraci -1 pokud neni zadny prefered
    {
        return (PreferedPacker < Packers.Count) ? PreferedPacker : -1;
    }
    void SetPreferedPacker(int i) { PreferedPacker = i; }

    BOOL ExecutePacker(CFilesWindow* panel, const char* zipFile, BOOL move,
                       const char* sourcePath, SalEnumSelection2 next, void* param);
};

//
// ****************************************************************************
// CUnpackerConfig
//

#define CUSTOMUNPACKER_EXTERNAL 0

class CUnpackerConfigData
{
public:
    char* Title; // jmeno pro usera
    char* Ext;   // seznam standardnich pripon oddelenych strednikem
    int Type;    // interni (-1, -2, ...; popis viz CPlugins)/externi (0; platnost dalsich polozek)
                 // dodatek viz OldType nize

    // data pro externi pakovace
    char* CmdExecExtract;
    char* CmdArgsExtract;
    BOOL SupportLongNames;
    BOOL NeedANSIListFile;

    // pomocna promenna pro zjisteni typu udaju - TRUE = stare -> 'Type' (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL OldType;

public:
    CUnpackerConfigData()
    {
        Empty();
    }

    ~CUnpackerConfigData()
    {
        Destroy();
    }

    void Destroy()
    {
        if (Title != NULL)
            free(Title);
        if (Ext != NULL)
            free(Ext);
        if (OldType && Type == 1 ||
            !OldType && Type == CUSTOMUNPACKER_EXTERNAL)
        {
            if (CmdExecExtract != NULL)
                free(CmdExecExtract);
            if (CmdArgsExtract != NULL)
                free(CmdArgsExtract);
        }
        Empty();
    }

    void Empty()
    {
        OldType = FALSE;
        Title = NULL;
        Ext = NULL;
        Type = 1;
        CmdExecExtract = NULL;
        CmdArgsExtract = NULL;
        SupportLongNames = FALSE;
        NeedANSIListFile = FALSE;
    }

    BOOL IsValid()
    {
        if (Title == NULL || Ext == NULL)
            return FALSE;
        if ((OldType && Type == 1 || !OldType && Type == CUSTOMUNPACKER_EXTERNAL) &&
            (CmdExecExtract == NULL || CmdArgsExtract == NULL))
            return FALSE;
        return TRUE;
    }
};

class CUnpackerConfig
{
protected:
    int PreferedUnpacker;
    TIndirectArray<CUnpackerConfigData> Unpackers; // pole informaci o pakovacich, polozky typu (CUnpackerConfigData *)

public:
    CUnpackerConfig(/*BOOL disableDefaultValues = FALSE*/);
    void InitializeDefaultValues(); // j.r. nahrazuje puvodni volani konstruktoru
    BOOL Load(CUnpackerConfig& src);

    void DeleteAllUnpackers() { Unpackers.DestroyMembers(); }

    int AddUnpacker(BOOL toFirstIndex = FALSE); // vrati index zalozene polozky nebo -1 pri chybe
    void AddDefault(int SalamVersion);          // prida archivery nove od verze SalamVersion

    // nastavi atributy; kdyz se neco posere, vyradi prvek z pole, zdestroji ho a vrati FALSE
    // old == TRUE -> 'type' je ve stare konvenci (0 ZIP, 1 external, 2 TAR, 3 PAK)
    BOOL SetUnpacker(int index, int type, const char* title, const char* ext, BOOL old,
                     BOOL supportLongNames = FALSE,
                     const char* cmdExecExtract = NULL, const char* cmdArgsExtract = NULL,
                     BOOL needANSIListFile = FALSE);
    BOOL SetUnpackerTitle(int index, const char* title);
    void SetUnpackerType(int index, int type) { Unpackers[index]->Type = type; }
    void SetUnpackerOldType(int index, BOOL oldType) { Unpackers[index]->OldType = oldType; }
    int GetUnpackersCount() { return Unpackers.Count; } // vrati pocet polozek v poli
                                                        //    BOOL SwapUnpackers(int index1, int index2);         // prohodi dve polozky v poli
    BOOL MoveUnpacker(int srcIndex, int dstIndex);      // posune polozku

    void DeleteUnpacker(int index);

    int GetUnpackerType(int index) { return Unpackers[index]->Type; }
    BOOL GetUnpackerOldType(int index) { return Unpackers[index]->OldType; }
    const char* GetUnpackerTitle(int index) { return Unpackers[index]->Title; }
    const char* GetUnpackerExt(int index) { return Unpackers[index]->Ext; }
    BOOL GetUnpackerSupLongNames(int index) { return Unpackers[index]->SupportLongNames; }
    const char* GetUnpackerCmdExecExtract(int index) { return Unpackers[index]->CmdExecExtract; }
    const char* GetUnpackerCmdArgsExtract(int index) { return Unpackers[index]->CmdArgsExtract; }
    BOOL GetUnpackerNeedANSIListFile(int index) { return Unpackers[index]->NeedANSIListFile; }

    BOOL Save(int index, HKEY hKey);
    BOOL Load(HKEY hKey);

    int GetPreferedUnpacker() // vraci -1 pokud neni zadny prefered
    {
        return (PreferedUnpacker < Unpackers.Count) ? PreferedUnpacker : -1;
    }
    void SetPreferedUnpacker(int i) { PreferedUnpacker = i; }

    BOOL ExecuteUnpacker(HWND parent, CFilesWindow* panel, const char* zipFile, const char* mask,
                         const char* targetDir, BOOL delArchiveWhenDone, CDynamicString* archiveVolumes);
};

extern CPackerConfig PackerConfig;
extern CUnpackerConfig UnpackerConfig;

int DialogError(HWND parent, DWORD flags, const char* fileName,
                const char* error, const char* title);
int DialogOverwrite(HWND parent, DWORD flags, const char* fileName1, const char* fileData1,
                    const char* fileName2, const char* fileData2);
int DialogQuestion(HWND parent, DWORD flags, const char* fileName,
                   const char* question, const char* title);

BOOL ViewFileInPluginViewer(const char* pluginSPL,
                            CSalamanderPluginViewerData* pluginData,
                            BOOL useCache, const char* rootTmpPath,
                            const char* fileNameInCache, int& error);
