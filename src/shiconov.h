// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

void InitShellIconOverlays();
void ReleaseShellIconOverlays();

struct CSQLite3DynLoadBase
{
    BOOL OK; // TRUE if SQLite3 is successfully loaded and ready to use
    HINSTANCE SQLite3DLL;

    CSQLite3DynLoadBase()
    {
        OK = FALSE;
        SQLite3DLL = NULL;
    }
    ~CSQLite3DynLoadBase()
    {
        if (SQLite3DLL != NULL)
            HANDLES(FreeLibrary(SQLite3DLL));
    }
};

struct CShellIconOverlayItem
{
    char IconOverlayName[MAX_PATH];          // name of the key under HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers
    IShellIconOverlayIdentifier* Identifier; // IShellIconOverlayIdentifier object, NOTE: can only be used in the main thread
    CLSID IconOverlayIdCLSID;                // CLSID of the respective IShellIconOverlayIdentifier object
    int Priority;                            // priority of this icon overlay (0-100, the highest priority is zero)
    HICON IconOverlay[ICONSIZE_COUNT];       // icon overlay in all sizes
    BOOL GoogleDriveOverlay;                 // TRUE = Google Drive handler (their handler crashes, so we handle it with extra synchronization)

    void Cleanup();

    CShellIconOverlayItem();
    ~CShellIconOverlayItem();
};

class CShellIconOverlays
{
protected:
    TIndirectArray<CShellIconOverlayItem> Overlays; // priority-sorted list of icon overlays
    CRITICAL_SECTION GD_CS;                         // for Google Drive we need to mutually exclude calls to IsMemberOf from both icon readers (otherwise it crashes and corrupts its heap)
    BOOL GetGDAlreadyCalled;                        // TRUE = we already checked where the folder for Google Drive is located
    char GoogleDrivePath[MAX_PATH];                 // folder for Google Drive (we do not call their handler elsewhere; it is disgustingly slow and, without the extra synchronization, it crashes)
    BOOL GoogleDrivePathIsFromCfg;                  // TRUE if the folder for Google Drive obtained from the Google Drive configuration (FALSE = it may be only the default one and Google Drive may not be installed at all)
    BOOL GoogleDrivePathExists;                     // does the folder for Google Drive exist on disk?

public:
    CShellIconOverlays() : Overlays(1, 5)
    {
        HANDLES(InitializeCriticalSection(&GD_CS));
        GoogleDrivePath[0] = 0;
        GetGDAlreadyCalled = FALSE;
        GoogleDrivePathIsFromCfg = FALSE;
        GoogleDrivePathExists = FALSE;
    }
    ~CShellIconOverlays() { HANDLES(DeleteCriticalSection(&GD_CS)); }

    // adds 'item' to the array (previously sorted incorrectly by "priority")
    BOOL Add(CShellIconOverlayItem* item /*, int priority*/);

    // releases all icon overlays
    void Release() { Overlays.Destroy(); }

    // allocates an array of IShellIconOverlayIdentifier objects for the calling thread (we use COM
    // in the STA threading model, so the object must be created and used only in a single thread)
    IShellIconOverlayIdentifier** CreateIconReadersIconOverlayIds();

    // releases the array of IShellIconOverlayIdentifier objects
    void ReleaseIconReadersIconOverlayIds(IShellIconOverlayIdentifier** iconReadersIconOverlayIds);

    // returns the icon overlay index for the file/directory "wPath+name"
    DWORD GetIconOverlayIndex(WCHAR* wPath, WCHAR* wName, char* aPath, char* aName, char* name,
                              DWORD fileAttrs, int minPriority,
                              IShellIconOverlayIdentifier** iconReadersIconOverlayIds,
                              BOOL isGoogleDrivePath);

    HICON GetIconOverlay(int iconOverlayIndex, CIconSizeEnum iconSize)
    {
        return Overlays[iconOverlayIndex]->IconOverlay[iconSize];
    }

    // called when the display color depth changes, all overlay icons have to be reloaded
    // NOTE: can only be called from the main thread
    void ColorsChanged();

    // if we have not done so yet, find where Google Drive resides; 'sqlite3_Dyn_InOut'
    // serves as a cache for sqlite.dll (if it is already loaded, we reuse it, and if it is loaded
    // in this function, we return it)
    void InitGoogleDrivePath(CSQLite3DynLoadBase** sqlite3_Dyn_InOut, BOOL debugTestOverlays);

    BOOL HasGoogleDrivePath();

    BOOL GetPathForGoogleDrive(char* path, int pathLen)
    {
        strcpy_s(path, pathLen, GoogleDrivePath);
        return GoogleDrivePath[0] != 0;
    }

    void SetGoogleDrivePath(const char* path, BOOL pathIsFromConfig)
    {
        strcpy_s(GoogleDrivePath, path);
        GoogleDrivePathIsFromCfg = pathIsFromConfig;
        GoogleDrivePathExists = FALSE;
    }

    BOOL IsGoogleDrivePath(const char* path) { return GoogleDrivePath[0] != 0 && SalPathIsPrefix(GoogleDrivePath, path); }
};

struct CShellIconOverlayItem2 // plain list of icon overlay handlers (for the configuration dialog, the Icon Overlays page)
{
    char IconOverlayName[MAX_PATH];  // name of the key under HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers
    char IconOverlayDescr[MAX_PATH]; // description of the COM object of the icon overlay handler
};

extern CShellIconOverlays ShellIconOverlays;                           // array of all available icon overlays
extern TIndirectArray<CShellIconOverlayItem2> ListOfShellIconOverlays; // list of all icon overlay handlers
