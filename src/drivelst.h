// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************

#define ONEDRIVE_MAXBUSINESSDISPLAYNAME 500

struct COneDriveBusinessStorage
{
    char* DisplayName;
    char* UserFolder;

    COneDriveBusinessStorage(char* displayName, char* userFolder) // ulozi si alokovane parametry
    {
        DisplayName = displayName;
        UserFolder = userFolder;
    }

    ~COneDriveBusinessStorage()
    {
        if (DisplayName != NULL)
            free(DisplayName);
        if (UserFolder != NULL)
            free(UserFolder);
    }
};

class COneDriveBusinessStorages : public TIndirectArray<COneDriveBusinessStorage>
{
public:
    COneDriveBusinessStorages() : TIndirectArray<COneDriveBusinessStorage>(1, 20) {}

    void SortIn(COneDriveBusinessStorage* s);
    BOOL Find(const char* displayName, const char** userFolder);
};

extern char DropboxPath[MAX_PATH];
extern char OneDrivePath[MAX_PATH];                        // jen pro personal ucet
extern COneDriveBusinessStorages OneDriveBusinessStorages; // jen pro business ucty

void InitOneDrivePath();
int GetOneDriveStorages();

//*****************************************************************************
//
// CDrivesList
//

// POZOR: v CDrivesList::BuildData se pouziva test "DriveType > drvtRAMDisk", pri zmenach
//        enumu je s tim nutne pocitat !!!
enum CDriveTypeEnum
{
    drvtSeparator,            // This item is Separator (for Alt+F1/2 menu)
    drvtUnknow,               // The drive type cannot be determined
    drvtRemovable,            // The disk can be removed from the drive
    drvtFixed,                // The disk cannot be removed from the drive
    drvtRemote,               // The drive is a remote (network) drive
    drvtCDROM,                // The drive is a CD-ROM drive
    drvtRAMDisk,              // The drive is a RAM disk
    drvtMyDocuments,          // The drive is a Documents
    drvtGoogleDrive,          // polozka je Cloud Storage: Google Drive
    drvtDropbox,              // polozka je Cloud Storage: Dropbox
    drvtOneDrive,             // polozka je Cloud Storage: OneDrive - Personal
    drvtOneDriveBus,          // polozka je Cloud Storage: OneDrive - Business
    drvtOneDriveMenu,         // polozka je Cloud Storage: vyber z vice OneDrive storages (jen v DriveBar, do Change Drive menu se nedava)
    drvtNeighborhood,         // The drive is a Network
    drvtOtherPanel,           // The drive is a other panel
    drvtHotPath,              // The drive is a hot path
    drvtPluginFS,             // polozka z plug-inu: otevreny FS (aktivni/odpojeny)
    drvtPluginCmd,            // polozka z plug-inu: prikaz FS
    drvtPluginFSInOtherPanel, // polozka z plug-inu: FS otevreny ve vedlejsim panelu
};

struct CDriveData
{
    CDriveTypeEnum DriveType; // pokud je drvtSeparator, ostatni polozky nejsou platne
    char* DriveText;
    DWORD Param;      // zatim pro Hot Paths - jeji index
    BOOL Accessible;  // u drvtRemote: FALSE - sedy symbol
    BOOL Shared;      // je disk sdileny?
    HICON HIcon;      // symbol drivu
    HICON HGrayIcon;  // cernobila verze symbolu drivu
    BOOL DestroyIcon; // ma se pri uklidu uvolnit HIcon?

    // jen pro drvtPluginFS a drvtPluginFSInOtherPanel: interface FS (nemusi byt platny, overovat)
    CPluginFSInterfaceAbstract* PluginFS;
    // jen pro drvtPluginCmd: ukazatel prevzaty z CPluginData plug-inu (jednoznacna identifikace
    // plug-inu - DLLName se alokuje jen jednou; nemusi byt platny, overovat); nestaci ukazatel
    // na CPluginData, protoze pri pridani/ubrani plug-inu by nebyl platny (realokace pole - viz array.h)
    const char* DLLName;
};

class CMenuPopup;
class CFilesWindow;
class CDriveBar;

class CDrivesList
{
protected:
    CFilesWindow* FilesWindow;
    CDriveTypeEnum* DriveType;
    DWORD_PTR* DriveTypeParam; // x64: refrencovana hodnota musi umet drzet ukazatel, takze DWORD_PTR
    int* PostCmd;              // post-cmd pro plug-in FS kontextove menu
    void** PostCmdParam;       // post-cmd-parameter pro plug-in FS kontextove menu
    BOOL* FromContextMenu;     // je nastaveno na TRUE pokud prikaz menu spustilo kontextove menu
    char CurrentPath[MAX_PATH];
    TDirectArray<CDriveData>* Drives;
    CMenuPopup* MenuPopup;
    int FocusIndex; // ktera polozka z pole Drives ma byt focused

    DWORD CachedDrivesMask;        // bitove pole disku, ktere jsme ziskali pri poslednim BuildData()
    DWORD CachedCloudStoragesMask; // bitove pole cloud storages, ktere jsme ziskali pri poslednim BuildData()

public:
    // vstup:
    //   driveType = dummy
    //   driveTypeParam = pismeno disku, ktery chceme aktivovat (nebo 0 (PluginFS) nebo '\\' (UNC))
    CDrivesList(CFilesWindow* filesWindow, const char* currentPath, CDriveTypeEnum* driveType,
                DWORD_PTR* driveTypeParam, int* postCmd, void** postCmdParam, BOOL* fromContextMenu);
    ~CDrivesList()
    {
        delete Drives;
        Drives = NULL;
    }

    // pridani prvku do pole Drives (pouziva se behem BuildData z Plugins)
    void AddDrive(CDriveData& drv, int& index)
    {
        index = Drives->Add(drv);
    }

    const CMenuPopup* GetMenuPopup() { return MenuPopup; }

    BOOL Track();

    // naleje tlacitka do toolbary; bar2 ridi jejich IDcka pokud je FALSE, budou od
    // CM_DRIVEBAR_MIN, jinak od CM_DRIVEBAR2_MIN
    BOOL FillDriveBar(CDriveBar* driveBar, BOOL bar2);

    // tudy FilesWindow predava informaci o tom, ze user rclicknul na polozce
    // 'posByMouse' udava, jestli mame vybalit menu na souradnicich mysi nebo
    // pod vybranou polozkou; 'panel' udava se kterym panelem se pracuje (pri dvou
    // Drive barach to muze byt i neaktivni panel - z Change Drive menu je vzdy PANEL_SOURCE,
    // z Drive bary je PANEL_LEFT nebo PANEL_RIGHT); neni-li 'pluginFSDLLName' NULL
    // a vybaluje se kontextove menu pro polozku FS, vraci se v nem jmeno DLLka pluginu (spise
    // SPLka); vraci TRUE pokud se ma spustit prikaz, na kterem se vybalovalo kontextove menu
    // (FALSE nedela nic); 'itemIndex' udava pro kterou plozku se vybali kontextove menu,
    // 'posByMouse' pak musi byt = TRUE; pokud je 'itemIndex' -1, ziska se polozka z menu
    BOOL OnContextMenu(BOOL posByMouse, int itemIndex, int panel, const char** pluginFSDLLName);

    // Tudy je pozadano o nove nacteni polozek do menu. Predpokladem je, ze
    // menu je zobrazene a program je v metode Track.
    BOOL RebuildMenu();

    // je-li 'noTimeout' TRUE, ceka se na CD volume label neomezene (jinak jen 500ms);
    // neni-li 'copyDrives' NULL, data se z nej jen zkopiruji (misto ziskavani dat ze systemu)
    // pokud je 'getGrayIcons' TRUE, ziska se pro vybrane polozky cernobila verze ikony
    // a nastavi se promenna 'HGrayIcon', jinak bude NULL
    BOOL BuildData(BOOL noTimeout, TDirectArray<CDriveData>* copyDrives = NULL,
                   DWORD copyCachedDrivesMask = 0, BOOL getGrayIcons = FALSE, BOOL forDriveBar = FALSE);
    void DestroyData();
    void DestroyDrives(TDirectArray<CDriveData>* drives);

    // pomocna funkce pro pridani polozky do Drives
    void AddToDrives(CDriveData& drv, int textResId, char hotkey, CDriveTypeEnum driveType,
                     BOOL getGrayIcons, HICON icon, BOOL destroyIcon = TRUE, const char* itemText = NULL);

    // nastavi *DriveType a *DriveTypeParam na zaklade indexu
    // vrati FALSE, pokud je index mimo rozsah nebo se cestu nepodarilo ozivit
    BOOL ExecuteItem(int index, HWND hwnd, const RECT* exclude, BOOL* fromDropDown);

    // do bufferu text vlozit tooltip odpovidajici drivu urcenemu promennou index
    BOOL GetDriveBarToolTip(int index, char* text);

    // jen pro Drive bary: prohleda data a pokud najde polozku s cestou z panelu 'panel', nastavi
    // 'index' na jeji index a vrati TRUE, jinak vrati FALSE
    BOOL FindPanelPathIndex(CFilesWindow* panel, DWORD* index);

    // vraci bitove pole disku, jak bylo ziskano pri poslednim BuildData()
    // pokud BuildData() jeste neprobehlo, vraci 0
    // lze pouzit pro rychlou detekci, zda nedoslo k nejake zmene disku
    DWORD GetCachedDrivesMask();

    // vraci bitove pole dostupnych cloud storages, jak bylo ziskano pri poslednim BuildData()
    // pokud BuildData() jeste neprobehlo, vraci 0
    // lze pouzit pro rychlou detekci, zda nedoslo k nejake zmene dostupnosti cloud storages
    DWORD GetCachedCloudStoragesMask();

    TDirectArray<CDriveData>* GetDrives() { return Drives; }

    TDirectArray<CDriveData>* AllocNewDrivesAndGetCurrentDrives()
    {
        TDirectArray<CDriveData>* ret = Drives;
        Drives = new TDirectArray<CDriveData>(30, 30);
        return ret;
    }

protected:
    BOOL LoadMenuFromData();

    CDriveTypeEnum OwnGetDriveType(const char* rootPath); // prelozi system DriveType na nas CDriveTypeEnum

    // pomocna metoda; pomaha predchazet dvoum separatorum za sebou
    BOOL IsLastItemSeparator();
};

struct CNBWNetAC3Thread
{
    HANDLE Thread;
    BOOL ShutdownArrived;

    CNBWNetAC3Thread()
    {
        Thread = NULL;
        ShutdownArrived = FALSE;
    }

    void Set(HANDLE thread) { Thread = thread; }
    void Close(BOOL shutdown = FALSE)
    {
        if (Thread != NULL)
        {
            AddAuxThread(Thread, TRUE); // thread muze bezet jen pri ukoncovani softu, timto ho nechame zabit
            Thread = NULL;
        }
        if (shutdown)
            ShutdownArrived = TRUE;
    }
};

extern CNBWNetAC3Thread NBWNetAC3Thread;
