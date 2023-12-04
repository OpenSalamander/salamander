// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

void InitShellIconOverlays();
void ReleaseShellIconOverlays();

struct CSQLite3DynLoadBase
{
    BOOL OK; // TRUE pokud je SQLite3 uspesne nahrany a pripraveny k pouziti
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
    char IconOverlayName[MAX_PATH];          // jmeno klice pod HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers
    IShellIconOverlayIdentifier* Identifier; // objekt IShellIconOverlayIdentifier, POZOR: da se pouzit jen v hlavnim threadu
    CLSID IconOverlayIdCLSID;                // CLSID prislusneho objektu IShellIconOverlayIdentifier
    int Priority;                            // priorita tohoto icon-overlaye (0-100, nejvyssi priorita je nula)
    HICON IconOverlay[ICONSIZE_COUNT];       // icon-overlay ve vsech velikostech
    BOOL GoogleDriveOverlay;                 // TRUE = Google Drive handler (pada jim to, resime extra synchronizaci)

    void Cleanup();

    CShellIconOverlayItem();
    ~CShellIconOverlayItem();
};

class CShellIconOverlays
{
protected:
    TIndirectArray<CShellIconOverlayItem> Overlays; // prioritne serazeny seznam icon-overlays
    CRITICAL_SECTION GD_CS;                         // pro GoogleDrive je potreba vzajemne vyloucit volani IsMemberOf z obou icon-readeru (jinak pada, corruptej si heap)
    BOOL GetGDAlreadyCalled;                        // TRUE = uz se zjistovalo, kde je slozka pro Google Drive
    char GoogleDrivePath[MAX_PATH];                 // slozka pro Google Drive (jinde jejich handler nezavolame, je nechutne pomaly a bez pridane synchronizace pada)
    BOOL GoogleDrivePathIsFromCfg;                  // je slozka pro Google Drive vytazena z konfigurace Google Drive (FALSE = muze byt jen defaultni + Google Drive vubec nemusi byt instalovany)
    BOOL GoogleDrivePathExists;                     // existuje na disku slozka pro Google Drive?

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

    // prida (drive se chybne zarazovano podle 'priority') 'item' do pole
    BOOL Add(CShellIconOverlayItem* item /*, int priority*/);

    // uvolni vsechny icon-overlays
    void Release() { Overlays.Destroy(); }

    // alokuje pole objektu IShellIconOverlayIdentifier pro volajici thread (pouzivame COM v
    // STA threading modelu, takze objekt musi byt vytvoren a pouzivan jen v jedinem threadu)
    IShellIconOverlayIdentifier** CreateIconReadersIconOverlayIds();

    // uvolni pole objektu IShellIconOverlayIdentifier
    void ReleaseIconReadersIconOverlayIds(IShellIconOverlayIdentifier** iconReadersIconOverlayIds);

    // vraci icon-overlay index pro soubor/adresar "wPath+name"
    DWORD GetIconOverlayIndex(WCHAR* wPath, WCHAR* wName, char* aPath, char* aName, char* name,
                              DWORD fileAttrs, int minPriority,
                              IShellIconOverlayIdentifier** iconReadersIconOverlayIds,
                              BOOL isGoogleDrivePath);

    HICON GetIconOverlay(int iconOverlayIndex, CIconSizeEnum iconSize)
    {
        return Overlays[iconOverlayIndex]->IconOverlay[iconSize];
    }

    // vola se pri zmene barevne hloubky displaye, vsechny ikonky overlayu se musi nacist znovu
    // POZOR: mozne volat jen z hl. threadu
    void ColorsChanged();

    // pokud jsme to jeste nedelali, zjistime kde Google Drive bydli; 'sqlite3_Dyn_InOut'
    // slouzi jako cache pro sqlite.dll (pokud uz je loadle, pouzijeme ho + pokud se loadne
    // v teto funkci, vracime ho)
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

struct CShellIconOverlayItem2 // jen seznam icon overlay handleru (pro konfiguracni dialog, stranku Icon Overlays)
{
    char IconOverlayName[MAX_PATH];  // jmeno klice pod HKEY_LOCAL_MACHINE\Software\Microsoft\Windows\CurrentVersion\Explorer\ShellIconOverlayIdentifiers
    char IconOverlayDescr[MAX_PATH]; // popis COM objektu icon overlay handleru
};

extern CShellIconOverlays ShellIconOverlays;                           // pole vsech dostupnych icon-overlays
extern TIndirectArray<CShellIconOverlayItem2> ListOfShellIconOverlays; // seznam vsech icon overlay handleru
