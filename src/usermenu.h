// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later
// CommentsTranslationProject: TRANSLATED

#pragma once

#define USRMNUARGS_MAXLEN 32772    // buffer size (+1 compared to the maximum string length) (=32776 (Vista/Win7 via .bat) - 5 ("C:\\a ") + 1)
#define USRMNUCMDLINE_MAXLEN 32777 // buffer size (+1 compared to the maximum string length)

//****************************************************************************
//
// CUserMenuIconBkgndReader
//

struct CUserMenuIconData
{
    char FileName[MAX_PATH];  // name of the file from which we should read the icon at IconIndex (via ExtractIconEx())
    DWORD IconIndex;          // see the comment for FileName
    char UMCommand[MAX_PATH]; // name of the file from which we should read the icon (via GetFileOrPathIconAux())

    HICON LoadedIcon; // NULL = icon not loaded, otherwise handle of the loaded icon

    CUserMenuIconData(const char* fileName, DWORD iconIndex, const char* umCommand);
    ~CUserMenuIconData();

    void Clear();
};

class CUserMenuIconDataArr : public TIndirectArray<CUserMenuIconData>
{
protected:
    DWORD IRThreadID; // unique thread ID for loading these icons

public:
    CUserMenuIconDataArr() : TIndirectArray<CUserMenuIconData>(50, 50) { IRThreadID = 0; }

    void SetIRThreadID(DWORD id) { IRThreadID = id; }
    DWORD GetIRThreadID() { return IRThreadID; }

    HICON GiveIconForUMI(const char* fileName, DWORD iconIndex, const char* umCommand);
};

class CUserMenuIconBkgndReader
{
protected:
    BOOL SysColorsChanged; // helper variable to detect changes in system colors since the config dialog opened

    CRITICAL_SECTION CS;       // critical section protecting the object's data
    DWORD IconReaderThreadUID; // generator of unique thread IDs for reading icons
    BOOL CurIRThreadIDIsValid; // TRUE = the thread is running and CurIRThreadID is valid
    DWORD CurIRThreadID;       // unique thread ID (see IconReaderThreadUID) that loads icons for the current user menu version
    BOOL AlreadyStopped;       // TRUE = no more icon reading; the main window has closed/is closing

    int UserMenuIconsInUse;                            // > 0: user menu icons are currently in an open menu, so we cannot update them immediately; at most 2 (Salamander cfg + Find: user menu)
    CUserMenuIconDataArr* UserMenuIIU_BkgndReaderData; // storage for new icons when UserMenuIconsInUse > 0
    DWORD UserMenuIIU_ThreadID;                        // storage for thread IDs (to determine data freshness) when UserMenuIconsInUse > 0

public:
    CUserMenuIconBkgndReader();
    ~CUserMenuIconBkgndReader();

    // The main window is closing -> we no longer want to receive any user menu icon data.
    void EndProcessing();

    // WARNING: 'bkgndReaderData' is deallocated inside.
    void StartBkgndReadingIcons(CUserMenuIconDataArr* bkgndReaderData);

    BOOL IsCurrentIRThreadID(DWORD threadID);

    BOOL IsReadingIcons();

    // WARNING: after calling this function this object is responsible for freeing 'bkgndReaderData'.
    void ReadingFinished(DWORD threadID, CUserMenuIconDataArr* bkgndReaderData);

    // Enter/leave the section where user menu icons are being used and therefore cannot
    // be updated while the section is executing (mainly when opening the user menu).
    void BeginUserMenuIconsInUse();
    void EndUserMenuIconsInUse();

    // If icons for an already outdated user menu were loaded, returns FALSE. Otherwise:
    // if the icons are currently in an open menu (see UserMenuIconsInUse), returns FALSE;
    // if the icons are not in an open menu, returns TRUE and WARNING: keeps the CS locked,
    // so access from other threads is blocked (mainly access to the user menu from the Find thread).
    // After updating the icons, use LeaveCSAfterUMIconsUpdate() to exit the CS.
    BOOL EnterCSIfCanUpdateUMIcons(CUserMenuIconDataArr** bkgndReaderData, DWORD threadID);
    void LeaveCSAfterUMIconsUpdate();

    void ResetSysColorsChanged() { SysColorsChanged = FALSE; }
    void SetSysColorsChanged() { SysColorsChanged = TRUE; }
    BOOL HasSysColorsChanged() { return SysColorsChanged; }
};

extern CUserMenuIconBkgndReader UserMenuIconBkgndReader;

//****************************************************************************
//
// CUserMenuItem
//

enum CUserMenuItemType
{
    umitItem,         // standard item
    umitSubmenuBegin, // marks the start of a submenu
    umitSubmenuEnd,   // marks the end of a submenu
    umitSeparator     // separator item
};

struct CUserMenuItem
{
    char *ItemName,
        *UMCommand,
        *Arguments,
        *InitDir,
        *Icon;

    int ThroughShell,
        CloseShell,
        UseWindow,
        ShowInToolbar;

    CUserMenuItemType Type;

    HICON UMIcon;

    CUserMenuItem(char* name, char* umCommand, char* arguments, char* initDir, char* icon,
                  int throughShell, int closeShell, int useWindow, int showInToolbar,
                  CUserMenuItemType type, CUserMenuIconDataArr* bkgndReaderData);

    CUserMenuItem();

    CUserMenuItem(CUserMenuItem& item, CUserMenuIconDataArr* bkgndReaderData);

    ~CUserMenuItem();

    // Tries to retrieve the icon handle in this order:
    // a) the Icon variable
    // b) SHGetFileInfo
    // c) the system default icon
    // Background icon loading: if 'bkgndReaderData' is NULL, read immediately; otherwise the icons
    // are read in the background. If 'getIconsFromReader' is FALSE, collect what should be loaded into
    // 'bkgndReaderData'. If it is TRUE, the icons are already loaded and we only take the loaded icon
    // handles from 'bkgndReaderData'.
    BOOL GetIconHandle(CUserMenuIconDataArr* bkgndReaderData, BOOL getIconsFromReader);

    // Searches ItemName for '&' and returns the hot key in key; returns TRUE if one is found.
    BOOL GetHotKey(char* key);

    BOOL Set(char* name, char* umCommand, char* arguments, char* initDir, char* icon);
    void SetType(CUserMenuItemType type);
    BOOL IsGood() { return ItemName != NULL && UMCommand != NULL &&
                           Arguments != NULL && InitDir != NULL && Icon != NULL; }
};

//****************************************************************************
//
// CUserMenuItems
//

class CUserMenuItems : public TIndirectArray<CUserMenuItem>
{
public:
    CUserMenuItems(DWORD base, DWORD delta, CDeleteType dt = dtDelete)
        : TIndirectArray<CUserMenuItem>(base, delta, dt) {}

    // Copies the list from 'source'.
    BOOL LoadUMI(CUserMenuItems& source, BOOL readNewIconsOnBkgnd);

    // Searches for the closing submenu item referenced by the 'index' variable.
    // Returns -1 if no closing item is found.
    int GetSubmenuEndIndex(int index);
};
