// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

//*****************************************************************************

#define ONEDRIVE_MAXBUSINESSDISPLAYNAME 500

struct COneDriveBusinessStorage
{
    char* DisplayName;
    char* UserFolder;

    COneDriveBusinessStorage(char* displayName, char* userFolder) // saves allocated parameters
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
extern char OneDrivePath[MAX_PATH];                        // for personal account only
extern COneDriveBusinessStorages OneDriveBusinessStorages; // for business accounts only

void InitOneDrivePath();
int GetOneDriveStorages();

//*****************************************************************************
//
// CDrivesList
//

// WARNING: in CDrivesList::BuildData test "DriveType > drvtRAMDisk" is used, so
//          it is necessary to bear this in mind when changing the enum !!!

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
    drvtGoogleDrive,          // the item is a Cloud Storage: Google Drive
    drvtDropbox,              // the item is a Cloud Storage: Dropbox
    drvtOneDrive,             // the item is a Cloud Storage: OneDrive - Personal
    drvtOneDriveBus,          // the item is a Cloud Storage: OneDrive - Business
    drvtOneDriveMenu,         // the item is a Cloud Storage: selection from multiple OneDrive storages (only in DriveBar, not added to Change Drive menu)
    drvtNeighborhood,         // The drive is a Network
    drvtOtherPanel,           // The drive is a other panel
    drvtHotPath,              // The drive is a hot path
    drvtPluginFS,             // the plugin item: opened FS (active/disconnected)
    drvtPluginCmd,            // the plugin item: FS command
    drvtPluginFSInOtherPanel, // the plugin item: FS opened in other panel
};

struct CDriveData
{
    CDriveTypeEnum DriveType; // if it's drvtSeparator, other items are not valid
    char* DriveText;
    DWORD Param;      // at the moment for Hot Paths - its index
    BOOL Accessible;  // for drvtRemote: FALSE - gray symbol
    BOOL Shared;      // is the drive shared?
    HICON HIcon;      // drive symbol
    HICON HGrayIcon;  // black and white version of the drive symbol
    BOOL DestroyIcon; // should HIcon be released when cleaning up?

    // for drvtPluginFS and drvtPluginFSInOtherPanel only: interface FS (may be invalid, check it)
    CPluginFSInterfaceAbstract* PluginFS;
    // for drvtPluginCmd only: pointer taken from CPluginData (unique identification of the
    //  plugin - DLLName is allocated only once; may be invalid, check it); pointer to
    //  CPluginData is not enough, because it would be invalid when adding/removing plugins (array reallocation - see array.h)
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
    DWORD_PTR* DriveTypeParam; // x64: the referenced value must be able to hold a pointer, so DWORD_PTR
    int* PostCmd;              // post-cmd for context menu of a FS plugin
    void** PostCmdParam;       // post-cmd-parameter for context menu of a FS plugin
    BOOL* FromContextMenu;     // set to TRUE if the menu was invoked from a context menu
    char CurrentPath[MAX_PATH];
    TDirectArray<CDriveData>* Drives;
    CMenuPopup* MenuPopup;
    int FocusIndex; // what item from the Drives array should be focused

    DWORD CachedDrivesMask;        // bit array of drives, which we got during the last BuildData()
    DWORD CachedCloudStoragesMask; // bit array of cloud storages, which we got during the last BuildData()

public:
    // input:
    // driveType = dummy
    // driveTypeParam = letter of the drive to activate (or 0 (PluginFS) or '\\' (UNC))
    CDrivesList(CFilesWindow* filesWindow, const char* currentPath, CDriveTypeEnum* driveType,
                DWORD_PTR* driveTypeParam, int* postCmd, void** postCmdParam, BOOL* fromContextMenu);
    ~CDrivesList()
    {
        delete Drives;
        Drives = NULL;
    }

    // adding an item to Driver array (used during BuildData from Plugins)
    void AddDrive(CDriveData& drv, int& index)
    {
        index = Drives->Add(drv);
    }

    const CMenuPopup* GetMenuPopup() { return MenuPopup; }

    BOOL Track();

    // loads buttons to the toolbar; their IDs are controlled by 'bar2' if FALSE, they will be from
    // CM_DRIVEBAR_MIN, otherwise from CM_DRIVEBAR2_MIN
    BOOL FillDriveBar(CDriveBar* driveBar, BOOL bar2);

    // here FilesWindows passes information that user right-clicked on item
    // 'posByMouse' says whether we should popup the menu at mouse coordinates or under the selected item;
    // 'panel' says which panel is active (it can be also inactive panel when there are two DriveBars - from
    // Change Drive menu it is always PANEL_SOURCE, from DriveBars it is PANEL_LEFT or PANEL_RIGHT);
    // if 'pluginFSDLLName' is not NULL and we are popping up a context menu for a FS item, it returns the
    // name of the plugin DLL (or SPL); returns TRUE if we should execute the command on which the context
    // menu was popped up (FALSE does nothing); 'itemIndex' says for which item we are popping up the context
    // menu, 'posByMouse' must be TRUE; if 'itemIndex' is -1, the item is taken from the menu
    BOOL OnContextMenu(BOOL posByMouse, int itemIndex, int panel, const char** pluginFSDLLName);

    // A new loading of items into the menu is requested here. It is assumed that the menu is displayed
    // and the program is in the Track method.
    BOOL RebuildMenu();

    // if 'noTimeout' is TRUE, it waits for CD volume label without timeout (otherwise only 500ms);
    // if 'copyDrives' is not NULL, data are copied from it (instead of getting data from the system)
    // if 'getGrayIcons' is TRUE, black and white version of the icon is obtained for selected items
    // and the 'HGrayIcon' variable is set, otherwise it will be NULL
    BOOL BuildData(BOOL noTimeout, TDirectArray<CDriveData>* copyDrives = NULL,
                   DWORD copyCachedDrivesMask = 0, BOOL getGrayIcons = FALSE, BOOL forDriveBar = FALSE);
    void DestroyData();
    void DestroyDrives(TDirectArray<CDriveData>* drives);

    // helper function for adding an item to Drives
    void AddToDrives(CDriveData& drv, int textResId, char hotkey, CDriveTypeEnum driveType,
                     BOOL getGrayIcons, HICON icon, BOOL destroyIcon = TRUE, const char* itemText = NULL);

    // sets *DriveType and *DriveTypeParam according to the index
    // returns FALSE if the index is out of range or the path cannot be revived
    BOOL ExecuteItem(int index, HWND hwnd, const RECT* exclude, BOOL* fromDropDown);

    // insert tooltip corresponding to the drive determined by the variable index to text buffer
    BOOL GetDriveBarToolTip(int index, char* text);

    // for Drive bars only: searches data and if it finds an item with path from 'panel' panel, it sets
    // 'index' to its index and returns TRUE, otherwise it returns FALSE
    BOOL FindPanelPathIndex(CFilesWindow* panel, DWORD* index);

    // returns bit array of drives, which we got during the last BuildData()
    // if BuildDate() has not been called yet, returns 0
    // can be used for quick detection of any change in the drives
    DWORD GetCachedDrivesMask();

    // returns bit array of available cloud storages, which we got during the last BuildData()
    // if BuildDate() has not been called yet, returns 0
    // can be used for quick detection of any change in the cloud storages
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

    CDriveTypeEnum OwnGetDriveType(const char* rootPath); // translates system DriveType to our CDriveTypeEnum

    // helper method; helps to prevent two separators in a row
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
            AddAuxThread(Thread, TRUE); // thread can run only during shutdown, we will kill it by this
            Thread = NULL;
        }
        if (shutdown)
            ShutdownArrived = TRUE;
    }
};

extern CNBWNetAC3Thread NBWNetAC3Thread;
