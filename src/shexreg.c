// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include <windows.h>
#include <shlobj.h>
//
// Initialize GUIDs (should be done only and at-least once per DLL/EXE)
//
#pragma data_seg(".text")
#include <initguid.h>
#include <shlguid.h>
#define INITGUID
#include "lstrfix.h"
#include "shexreg.h"
#pragma data_seg()

#define NOHANDLES(function) function // obrana proti zanaseni maker HANDLES do zdrojaku pomoci CheckHnd

//const char *SALSHEXT_SHAREDMEMMUTEXNAME = "SalShExt_SharedMemMutex"; // salshext.dll (Sal 2.5 beta 1)
//const char *SALSHEXT_SHAREDMEMNAME = "SalShExt_SharedMem";           // salshext.dll (Sal 2.5 beta 1)
//const char *SALSHEXT_SHAREDMEMMUTEXNAME = "SalExten_SharedMemMutex"; // salexten.dll - pracovni verze, pred 2.5 beta 2
//const char *SALSHEXT_SHAREDMEMNAME = "SalExten_SharedMem";           // salexten.dll - pracovni verze, pred 2.5 beta 2
//const char *SALSHEXT_SHAREDMEMMUTEXNAME = "SalExten_SharedMemMutex2";// salexten.dll (od verze 2.5 beta 2) + salamext.dll
//const char *SALSHEXT_SHAREDMEMNAME = "SalExten_SharedMem2";          // salexten.dll (od verze 2.5 beta 2) + salamext.dll
//const char *SALSHEXT_DOPASTEEVENTNAME = "SalExten_DoPasteEvent2";    // salamext.dll - pracovni verze pro 2.52 beta 1, pouzivala se jen pod Vista+
//const char *SALSHEXT_SHAREDMEMMUTEXNAME = "SalExten_SharedMemMutex3";// salamext.dll (od verze 2.52 beta 1)
//const char *SALSHEXT_SHAREDMEMNAME = "SalExten_SharedMem3";          // salamext.dll (od verze 2.52 beta 1)
//const char *SALSHEXT_DOPASTEEVENTNAME = "SalExten_DoPasteEvent3";    // salamext.dll (od verze 2.52 beta 1, pouziva se jen pod Vista+)
const char* SALSHEXT_SHAREDMEMMUTEXNAME = "SalExten_SharedMemMutex4"; // salextx86.dll a salextx64.dll (od verze 3.0 beta 1)
const char* SALSHEXT_SHAREDMEMNAME = "SalExten_SharedMem4";           // salextx86.dll a salextx64.dll (od verze 3.0 beta 1)
const char* SALSHEXT_DOPASTEEVENTNAME = "SalExten_DoPasteEvent4";     // salextx86.dll a salextx64.dll (od verze 3.0 beta 1, pouziva se jen pod Vista+)

//const char *SHEXREG_OPENSALAMANDER = "ServantSalamander";                                // salshext.dll (Sal 2.5 beta 1)
//const char *SHEXREG_OPENSALAMANDER_DESCR = "Shell Extension for Servant Salamander";     // salshext.dll (Sal 2.5 beta 1)
//const char *SHEXREG_OPENSALAMANDER = "ServantSalamander25";                              // salexten.dll - 2.5 beta 2 az RC1
//const char *SHEXREG_OPENSALAMANDER_DESCR = "Shell Extension for Servant Salamander 2.5"; // salexten.dll - 2.5 beta 2 az RC1
//const char* SHEXREG_OPENSALAMANDER = "AltapSalamanderVer" SALSHEXT_SHAREDNAMESAPPENDIX;  // salexten.dll - do 4.0
const char* SHEXREG_OPENSALAMANDER = "OpenSalamanderVer" SALSHEXT_SHAREDNAMESAPPENDIX;
#ifdef INSIDE_SALAMANDER
#include "versinfo.rh2"
const char* SHEXREG_OPENSALAMANDER_DESCR = "Shell Extension (%s) for Open Salamander " VERSINFO_VERSION;
#endif // INSIDE_SALAMANDER

#ifdef ENABLE_SH_MENU_EXT

//
// ============================================= spolecna cast
//
/*
const char *SHELLEXT_ROOT_REG = "Software\\Altap\\Servant Salamander\\Shell Extension";
const char *SHELLEXT_CONTEXTMENU = "Context Menu";
const char *SHELLEXT_VERSION = "Version";

const char *SHELLEXT_CM_NAME = "Name";
const char *SHELLEXT_CM_ONEFILE = "One File";
const char *SHELLEXT_CM_ONEDIRECTORY = "One Directory";
const char *SHELLEXT_CM_MOREFILES = "More Files";
const char *SHELLEXT_CM_MOREDIRECTORIES = "More Directories";
const char *SHELLEXT_CM_AND = "Logical AND";

const char *SHELLEXT_CM_SUBMENU = "Show In Submenu";
const char *SHELLEXT_CM_SUBMENUNAME = "Submenu Name";

BOOL ShellExtConfigSubmenu = FALSE;
char ShellExtConfigSubmenuName[] = "&Servant Salamander";

CShellExtConfigItem *ShellExtConfigFirst = NULL;
DWORD ShellExtConfigVersion = 0;


void 
SECClearItem(CShellExtConfigItem *item)
{
  item->Name[0] = 0;
  item->OneFile = TRUE;
  item->OneDirectory = TRUE;
  item->MoreFiles = TRUE;
  item->MoreDirectories = TRUE;
  item->LogicalAnd = FALSE;

  item->Next = NULL;
}

BOOL 
SECLoadRegistry()
{
  char key[MAX_PATH];
  HKEY hKey;
  LONG res;
  DWORD gettedType;
  DWORD bufferSize;
  DWORD version;
  BOOL reRead = TRUE;

  lstrcpy(key, SHELLEXT_ROOT_REG);
  lstrcat(key, "\\");
  lstrcat(key, SHELLEXT_CONTEXTMENU);

  // otevru klic Shell Extensions
  res = NOHANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, key, 0, KEY_READ, &hKey));
  if (res != ERROR_SUCCESS) return FALSE;

  // zkontroluju verzi - je potreba nacist znovu data?
  bufferSize = sizeof(DWORD);
  res = SalRegQueryValueEx(hKey, SHELLEXT_VERSION, 0, &gettedType, (BYTE *)&version, &bufferSize);
  if (res == ERROR_SUCCESS)
  {
    // po prvni spusteni nacitam vzdy data
    if (ShellExtConfigVersion != 0 && ShellExtConfigVersion == version)
      reRead = FALSE;
    else
      ShellExtConfigVersion = version;
  }
  else
    ShellExtConfigVersion = 0;

  // je-li treba, nactu data
  if (reRead)
  {
    HKEY hItemKey;
    int i = 1;

//    MessageBox(NULL, "loading registry", "shellext.dll", MB_OK);
    // zrusim drzena data
    SECDeleteAllItems();

    // a nactu nova
    lstrcpy(key, "1");
    while (NOHANDLES(RegOpenKeyEx(hKey, key, 0, KEY_READ, &hItemKey)) == ERROR_SUCCESS) 
    {
      // ted vytvorim a nactu jednu polozku
      CShellExtConfigItem *item;
      if (SECAddItem(&item) != -1)
      {
        bufferSize = SEC_NAMEMAX;
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_NAME, 0, &gettedType, (BYTE *)item->Name, &bufferSize);
        bufferSize = sizeof(BOOL);
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_ONEFILE, 0, &gettedType, (BYTE *)&item->OneFile, &bufferSize);
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_ONEDIRECTORY, 0, &gettedType, (BYTE *)&item->OneDirectory, &bufferSize);
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_MOREFILES, 0, &gettedType, (BYTE *)&item->MoreFiles, &bufferSize);
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_MOREDIRECTORIES, 0, &gettedType, (BYTE *)&item->MoreDirectories, &bufferSize);
        SalRegQueryValueEx(hItemKey, SHELLEXT_CM_AND, 0, &gettedType, (BYTE *)&item->LogicalAnd, &bufferSize);
      }
      NOHANDLES(RegCloseKey(hItemKey));
      wsprintf(key, "%d", ++i);
    }

    // nactu jednotlive promenne konfigurace
    bufferSize = sizeof(BOOL);
    SalRegQueryValueEx(hKey, SHELLEXT_CM_SUBMENU, 0, &gettedType, (BYTE *)&ShellExtConfigSubmenu, &bufferSize);
    bufferSize = sizeof(ShellExtConfigSubmenuName);
    SalRegQueryValueEx(hKey, SHELLEXT_CM_SUBMENUNAME, 0, &gettedType, (BYTE *)ShellExtConfigSubmenuName, &bufferSize);
  }

  NOHANDLES(RegCloseKey(hKey));
  return TRUE;
}

CShellExtConfigItem*
SECGetItem(int index)
{
  CShellExtConfigItem *iterator = ShellExtConfigFirst;

  int i = 0;
  while (iterator != NULL && i != index)
  {
    iterator = iterator->Next;
    i++;
  }

  return iterator;
}

BOOL
SECGetItemIndex(UINT cmd, int *index)
{
  CShellExtConfigItem *iterator = ShellExtConfigFirst;

  int i = 0;
  while (iterator != NULL && iterator->Cmd != cmd)
  {
    iterator = iterator->Next;
    i++;
  }

  if (iterator == NULL)
  {
    return FALSE;
  }
  
  *index = i;
  return TRUE;
}


// vytahne nazev polozky 
const char *
SECGetName(int index)
{
  CShellExtConfigItem *item = SECGetItem(index);

  if (item == NULL)
    return "";

  return item->Name;
}

// vyhodi ze seznamu vsechny polozky
void 
SECDeleteAllItems()
{
  CShellExtConfigItem *iterator = ShellExtConfigFirst;

  while (iterator != NULL)
  {
    CShellExtConfigItem *tmp = iterator;
    iterator = iterator->Next;
    NOHANDLES(GlobalFree(tmp));
  }

  ShellExtConfigFirst = NULL;
}

int 
SECAddItem(CShellExtConfigItem **refItem)
{
  CShellExtConfigItem *item;
  CShellExtConfigItem *iterator = ShellExtConfigFirst;
  int index;

  // naalokuju polozku
  item = (CShellExtConfigItem*)NOHANDLES(GlobalAlloc(GMEM_FIXED, sizeof(CShellExtConfigItem)));

  if (refItem != NULL)
    *refItem = item;

  if (item == NULL)
    return -1;

  // inicializace promennych
  SECClearItem(item);

  // pripojim ji na konec seznamu
  index = 0;
  if (ShellExtConfigFirst == NULL)
  {
    ShellExtConfigFirst = item;
  }
  else
  {
    index++;
    while (iterator->Next != NULL)
    {
      iterator = iterator->Next;
      index++;
    }
    iterator->Next = item;
  }

  // vratim jeji index
  return index;
}
*/

#endif // ENABLE_SH_MENU_EXT

BOOL MyClearKey(HKEY key, REGSAM regView)
{
    char name[MAX_PATH];
    DWORD size = MAX_PATH;
    HKEY subKey;

    while (RegEnumKey(key, 0, name, MAX_PATH) == ERROR_SUCCESS)
    {
        if (NOHANDLES(RegOpenKeyEx(key, name, 0, KEY_READ | KEY_WRITE | regView, &subKey)) == ERROR_SUCCESS)
        {
            BOOL ret = MyClearKey(subKey, regView);
            NOHANDLES(RegCloseKey(subKey));
            if (!ret || RegDeleteKey(key, name) != ERROR_SUCCESS)
                return FALSE;
        }
        else
            return FALSE;
    }

    while (RegEnumValue(key, 0, name, &size, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
        if (RegDeleteValue(key, name) != ERROR_SUCCESS)
            break;
        else
            size = MAX_PATH;

    return TRUE;
}

BOOL MyDeleteKey(HKEY key, const char* keyName, REGSAM regView)
{
    HKEY delKey;
    if (NOHANDLES(RegOpenKeyEx(key, keyName, 0, KEY_READ | KEY_WRITE | regView, &delKey)) == ERROR_SUCCESS)
    {
        MyClearKey(delKey, regView);
        NOHANDLES(RegCloseKey(delKey));
    }
    if (NOHANDLES(RegOpenKeyEx(key, NULL, 0, KEY_READ | KEY_WRITE | regView, &delKey)) == ERROR_SUCCESS)
    {
        BOOL ret = RegDeleteKey(delKey, keyName) == ERROR_SUCCESS;
        NOHANDLES(RegCloseKey(delKey));
        return ret;
    }
    else
        return FALSE;
}

//
// ============================================= pouze SalShExt
//

#ifndef INSIDE_SALAMANDER

HRESULT DllUnregisterServerBody(REGSAM regView)
{
    char key[MAX_PATH];
    WCHAR buff2[MAX_PATH];
    char shellExtIID[MAX_PATH];
    HKEY hKey;

    StringFromGUID2(&CLSID_ShellExtension, buff2, MAX_PATH);
    WideCharToMultiByte(CP_ACP,
                        0,
                        buff2,
                        -1,
                        shellExtIID,
                        MAX_PATH,
                        NULL,
                        NULL);
    shellExtIID[MAX_PATH - 1] = 0;

    wsprintf(key, "CLSID\\%s", shellExtIID);
    MyDeleteKey(HKEY_CLASSES_ROOT, key, regView);
    wsprintf(key, "Software\\Classes\\CLSID\\%s", shellExtIID);
    MyDeleteKey(HKEY_CURRENT_USER, key, regView);
    wsprintf(key, "directory\\shellex\\CopyHookHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CLASSES_ROOT, key, regView);
    wsprintf(key, "Software\\Classes\\directory\\shellex\\CopyHookHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CURRENT_USER, key, regView);

#ifdef ENABLE_SH_MENU_EXT

    wsprintf(key, "*\\shellex\\ContextMenuHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CLASSES_ROOT, key, regView);
    wsprintf(key, "Software\\Classes\\*\\shellex\\ContextMenuHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CURRENT_USER, key, regView);
    wsprintf(key, "Directory\\shellex\\ContextMenuHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CLASSES_ROOT, key, regView);
    wsprintf(key, "Software\\Classes\\Directory\\shellex\\ContextMenuHandlers\\%s", SHEXREG_OPENSALAMANDER);
    MyDeleteKey(HKEY_CURRENT_USER, key, regView);

#endif // ENABLE_SH_MENU_EXT

    lstrcpy(key, "Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved");
    if (NOHANDLES(RegOpenKeyEx(HKEY_LOCAL_MACHINE, key, 0, KEY_READ | KEY_WRITE | regView, &hKey)) == ERROR_SUCCESS)
    {
        RegDeleteValue(hKey, shellExtIID);
        NOHANDLES(RegCloseKey(hKey));
    }
    return S_OK;
}

STDAPI DllUnregisterServer()
{
    return DllUnregisterServerBody(0);
}

// slouzi pro odregistrovani shell extensiony "druhe platformy" (x86 pro x64 a naopak),
// pouziva remove.exe a setup.exe (pri upgradech odinstalovava predchozi verzi)
STDAPI DllUnregisterServerOtherPlatform()
{
#ifdef _WIN64
    return DllUnregisterServerBody(KEY_WOW64_32KEY);
#else  // _WIN64
    return DllUnregisterServerBody(KEY_WOW64_64KEY);
#endif // _WIN64
}

#endif // INSIDE_SALAMANDER

//
// ============================================= pouze Open Salamander
//

#ifdef INSIDE_SALAMANDER

BOOL FileExists(const char* fileName);

BOOL MyCreateKey(HKEY hKey, const char* name, HKEY* createdKey, REGSAM regView)
{
    DWORD createType; // info jestli byl klic vytvoren nebo jen otevren
    LONG res = NOHANDLES(RegCreateKeyEx(hKey, name, 0, NULL, REG_OPTION_NON_VOLATILE,
                                        KEY_READ | KEY_WRITE | regView, NULL, createdKey,
                                        &createType));
    return res == ERROR_SUCCESS;
}

// nase varianta funkce RegQueryValueEx, narozdil od API varianty zajistuje
// pridani null-terminatoru pro typy REG_SZ, REG_MULTI_SZ a REG_EXPAND_SZ
// POZOR: pri zjistovani potrebne velikosti bufferu vraci o jeden nebo dva (dva
//        jen u REG_MULTI_SZ) znaky vic pro pripad, ze by string bylo potreba
//        zakoncit nulou/nulami
LONG SalRegQueryValueEx(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved,
                        LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData);

BOOL MyGetValue(HKEY hKey, const char* name, DWORD type, void* buffer, DWORD bufferSize)
{
    DWORD gettedType;
    LONG res = SalRegQueryValueEx(hKey, name, 0, &gettedType, (BYTE*)buffer, &bufferSize);
    return res == ERROR_SUCCESS && gettedType == type;
}

BOOL CheckVersionOfDLL(const char* name)
{
    typedef HRESULT(STDAPICALLTYPE * FDllCheckVersion)(REFCLSID rclsid);
    BOOL ok = FALSE;
    HMODULE dll = LoadLibrary(name);
    if (dll != NULL)
    {
        FDllCheckVersion DllCheckVersion = (FDllCheckVersion)GetProcAddress(dll, "DllCheckVersion"); // nas export
        if (DllCheckVersion != NULL && DllCheckVersion(&CLSID_ShellExtension) == S_OK)
            ok = TRUE; // verze souboru je OK
        FreeLibrary(dll);
    }
    return ok;
}

// Info pro UNINSTALL: (rutina pro uninstall je implementovana v DllUnregisterServer())
// - od verze 2.5 RC2 uz se nepouziva: - delete souboru z TEMPu (default value v HKEY_CLASSES_ROOT\CLSID\{C78B6131-F3EA-11D2-94A1-00E0292A01E3}\InProcServer32) (pocitat s tim, ze nemusi jit smazat hned - umet naplanovat po rebootu masiny)
// - od verze 3.0 B1: pocitat s tim, ze utils\salextx86.dll a salextx64.dll nemusi jit smazat hned - umet naplanovat po rebootu masiny
// - smazat HKEY_CLASSES_ROOT\CLSID\{C78B61??-F3EA-11D2-94A1-00E0292A01E3} (aktualni CLSID je v CLSID_ShellExtension)
// - smazat HKEY_CLASSES_ROOT\Directory\shellex\CopyHookHandlers\AltapSalamander?? (aktualni jmeno klice je v SHEXREG_OPENSALAMANDER)
// - smazat v klici HKEY_LOCAL_MACHINE\SOFTWARE\Microsoft\Windows\CurrentVersion\Shell Extensions\Approved
//   hodnotu {C78B61??-F3EA-11D2-94A1-00E0292A01E3} (aktualni CLSID je v CLSID_ShellExtension)
// - je-li definovano makro ENABLE_SH_MENU_EXT:
//   - smazat HKEY_CLASSES_ROOT\*\shellex\ContextMenuHandlers\AltapSalamander?? (aktualni jmeno klice je v SHEXREG_OPENSALAMANDER)
//   - smazat HKEY_CLASSES_ROOT\Directory\shellex\ContextMenuHandlers\AltapSalamander?? (aktualni jmeno klice je v SHEXREG_OPENSALAMANDER)
// - vse co bylo receno o klici HKEY_CLASSES_ROOT je potreba zkusit smazat tez z klice
//   HKEY_CURRENT_USER\Software\Classes (vyuziva se pokud user nema prava pro zapis do
//   HKEY_CLASSES_ROOT)
BOOL SECRegisterToRegistry(const char* shellExtensionPath, BOOL doNotLoadDLL, REGSAM regView)
{
    HKEY hKey;
    char key[MAX_PATH];
    char shellExtIID[MAX_PATH];
    char shellExtPath[MAX_PATH];
    char* str;
    WCHAR buff2[MAX_PATH];
    BOOL registered;
    HKEY classesKey;

    if (!doNotLoadDLL && !CheckVersionOfDLL(shellExtensionPath))
        return FALSE;

    StringFromGUID2(&CLSID_ShellExtension, buff2, MAX_PATH);
    WideCharToMultiByte(CP_ACP,
                        0,
                        buff2,
                        -1,
                        shellExtIID,
                        MAX_PATH,
                        NULL,
                        NULL);
    shellExtIID[MAX_PATH - 1] = 0;

    // zjistime jestli uz je nase shell extensiona registrovana, pripadne kde je jeji DLL a jestli je to spravna verze
    registered = FALSE;
    lstrcpy(key, "CLSID\\");
    lstrcat(key, shellExtIID);
    lstrcat(key, "\\InProcServer32");
    if (NOHANDLES(RegOpenKeyEx(HKEY_CLASSES_ROOT, key, 0, KEY_READ | regView, &hKey)) == ERROR_SUCCESS)
    {
        if (MyGetValue(hKey, NULL /* default value */, REG_SZ, shellExtPath, MAX_PATH))
        {
            if (doNotLoadDLL && FileExists(shellExtPath) ||       // kdyz ho nemuzu loadit, aspon overim, ze existuje
                !doNotLoadDLL && CheckVersionOfDLL(shellExtPath)) // jinak ho naloadim a zjistim od nej jeho verzi
            {
                registered = TRUE; // DLL je registrovane + je to spravna verze DLL
            }
        }
        NOHANDLES(RegCloseKey(hKey));
    }

    if (!registered)
    {
        classesKey = HKEY_CLASSES_ROOT;

    REG_TRY_AGAIN:

#ifdef ENABLE_SH_MENU_EXT

        lstrcpy(key, "*\\shellex\\ContextMenuHandlers\\");
        lstrcat(key, SHEXREG_OPENSALAMANDER);
        if (MyCreateKey(classesKey, key, &hKey, regView))
        {
            RegSetValueEx(hKey, NULL, 0, REG_SZ,
                          (BYTE*)shellExtIID, lstrlen(shellExtIID) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }
        // else;  // chybu otevirani klice pod HKEY_CLASSES_ROOT resime az dale (zde uz by bylo zbytecne)

        lstrcpy(key, "Directory\\shellex\\ContextMenuHandlers\\");
        lstrcat(key, SHEXREG_OPENSALAMANDER);
        if (MyCreateKey(classesKey, key, &hKey, regView))
        {
            RegSetValueEx(hKey, NULL, 0, REG_SZ,
                          (BYTE*)shellExtIID, lstrlen(shellExtIID) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }

#endif // ENABLE_SH_MENU_EXT

        wsprintf(key, "CLSID\\%s", shellExtIID);
        if (MyCreateKey(classesKey, key, &hKey, regView))
        {
            char descrBuf[200];
#ifdef _WIN64
            wsprintf(descrBuf, SHEXREG_OPENSALAMANDER_DESCR, (regView & KEY_WOW64_32KEY) ? "x86" : "x64");
#else  // _WIN64
            wsprintf(descrBuf, SHEXREG_OPENSALAMANDER_DESCR, (regView & KEY_WOW64_64KEY) ? "x64" : "x86");
#endif // _WIN64
            RegSetValueEx(hKey, NULL, 0, REG_SZ, (BYTE*)descrBuf, lstrlen(descrBuf) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }
        else
        {
            if (classesKey == HKEY_CLASSES_ROOT)
            {
                if (NOHANDLES(RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\Classes", 0,
                                           KEY_READ | KEY_WRITE | regView, &classesKey)) == ERROR_SUCCESS)
                {
                    if (MyCreateKey(classesKey, "CLSID", &hKey, regView))
                        NOHANDLES(RegCloseKey(hKey)); // klic "CLSID" ta tomto miste nemusi existovat, vytvorime si ho
                    goto REG_TRY_AGAIN;
                }
            }
            if (classesKey != HKEY_CLASSES_ROOT)
                NOHANDLES(RegCloseKey(classesKey));
            return FALSE;
        }

        wsprintf(key, "CLSID\\%s\\InProcServer32", shellExtIID);
        if (MyCreateKey(classesKey, key, &hKey, regView))
        {
            RegSetValueEx(hKey, NULL, 0, REG_SZ,
                          (BYTE*)shellExtensionPath, lstrlen(shellExtensionPath) + 1);
            str = "Apartment";
            RegSetValueEx(hKey, "ThreadingModel", 0, REG_SZ, (BYTE*)str, lstrlen(str) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }

        lstrcpy(key, "directory\\shellex\\CopyHookHandlers\\");
        lstrcat(key, SHEXREG_OPENSALAMANDER);
        if (MyCreateKey(classesKey, key, &hKey, regView))
        {
            RegSetValueEx(hKey, NULL, 0, REG_SZ,
                          (BYTE*)shellExtIID, lstrlen(shellExtIID) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }

        // bez "As Admin" je tohle "dead code", aspon pod Vista+
        wsprintf(key, "Software\\Microsoft\\Windows\\CurrentVersion\\Shell Extensions\\Approved");
        if (MyCreateKey(HKEY_LOCAL_MACHINE, key, &hKey, regView))
        {
            char descrBuf[200];
#ifdef _WIN64
            wsprintf(descrBuf, SHEXREG_OPENSALAMANDER_DESCR, (regView & KEY_WOW64_32KEY) ? "x86" : "x64");
#else  // _WIN64
            wsprintf(descrBuf, SHEXREG_OPENSALAMANDER_DESCR, (regView & KEY_WOW64_64KEY) ? "x64" : "x86");
#endif // _WIN64
            RegSetValueEx(hKey, shellExtIID, 0, REG_SZ, (BYTE*)descrBuf, lstrlen(descrBuf) + 1);
            NOHANDLES(RegCloseKey(hKey));
        }
        if (classesKey != HKEY_CLASSES_ROOT)
            NOHANDLES(RegCloseKey(classesKey));

        // tohle by melo shell informovat o tom, ze je potreba reloadnout shell extensiony
        // (ovsem pro copy-hook to nefunguje; tak snad bude aspon pro menu)
        SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, 0, 0);
    }

    return TRUE;
}

#ifdef ENABLE_SH_MENU_EXT

BOOL SECSaveRegistry()
{
    HKEY hKey;
    char key[MAX_PATH];
    DWORD bufferSize;
    DWORD gettedType;

    lstrcpy(key, SHELLEXT_ROOT_REG);
    lstrcat(key, "\\");
    lstrcat(key, SHELLEXT_CONTEXTMENU);

    if (MyCreateKey(HKEY_CURRENT_USER, key, &hKey))
    {
        DWORD version;
        int res;
        int i = 1;
        CShellExtConfigItem* iterator = ShellExtConfigFirst;

        // zvetsim cislo verze o jednicku
        ShellExtConfigVersion = 0;
        bufferSize = sizeof(DWORD);
        res = SalRegQueryValueEx(hKey, SHELLEXT_VERSION, 0, &gettedType, (BYTE*)&version, &bufferSize);
        if (res == ERROR_SUCCESS)
            ShellExtConfigVersion = version;
        ShellExtConfigVersion++;

        // podrezeme stavajici polozky v registry
        MyClearKey(hKey, 0);

        RegSetValueEx(hKey, SHELLEXT_VERSION, 0, REG_DWORD,
                      (BYTE*)&ShellExtConfigVersion, sizeof(DWORD));

        while (iterator != NULL)
        {
            HKEY hItemKey;
            wsprintf(key, "%d", i);

            if (MyCreateKey(hKey, key, &hItemKey))
            {
                RegSetValueEx(hItemKey, SHELLEXT_CM_NAME, 0, REG_SZ,
                              (CONST BYTE*)iterator->Name, strlen(iterator->Name) + 1);
                RegSetValueEx(hItemKey, SHELLEXT_CM_ONEFILE, 0, REG_DWORD, (BYTE*)&iterator->OneFile, sizeof(BOOL));
                RegSetValueEx(hItemKey, SHELLEXT_CM_ONEDIRECTORY, 0, REG_DWORD, (BYTE*)&iterator->OneDirectory, sizeof(BOOL));
                RegSetValueEx(hItemKey, SHELLEXT_CM_MOREFILES, 0, REG_DWORD, (BYTE*)&iterator->MoreFiles, sizeof(BOOL));
                RegSetValueEx(hItemKey, SHELLEXT_CM_MOREDIRECTORIES, 0, REG_DWORD, (BYTE*)&iterator->MoreDirectories, sizeof(BOOL));
                RegSetValueEx(hItemKey, SHELLEXT_CM_AND, 0, REG_DWORD, (BYTE*)&iterator->LogicalAnd, sizeof(BOOL));

                NOHANDLES(RegCloseKey(hItemKey));
            }
            else
            {
                NOHANDLES(RegCloseKey(hKey));
                return FALSE;
            }

            iterator = iterator->Next;
            i++;
        }

        RegSetValueEx(hKey, SHELLEXT_VERSION, 0, REG_DWORD,
                      (BYTE*)&ShellExtConfigVersion, sizeof(DWORD));

        // ulozim jednotlive promenne konfigurace
        RegSetValueEx(hKey, SHELLEXT_CM_SUBMENU, 0, REG_DWORD, (BYTE*)&ShellExtConfigSubmenu, sizeof(BOOL));
        RegSetValueEx(hKey, SHELLEXT_CM_SUBMENUNAME, 0, REG_SZ, (BYTE*)ShellExtConfigSubmenuName, strlen(ShellExtConfigSubmenuName) + 1);

        NOHANDLES(RegCloseKey(hKey));
        return TRUE;
    }
    return FALSE;
}

int SECGetCount()
{
    int count = 0;
    CShellExtConfigItem* iterator = ShellExtConfigFirst;

    while (iterator != NULL)
    {
        iterator = iterator->Next;
        count++;
    }

    return count;
}

BOOL SECDeleteItem(int index)
{
    CShellExtConfigItem* item;
    CShellExtConfigItem* next;

    item = SECGetItem(index);
    if (item == NULL)
        return FALSE;

    next = item->Next;
    if (index > 0)
        SECGetItem(index - 1)->Next = next;
    else
        ShellExtConfigFirst = next;

    NOHANDLES(GlobalFree(item));

    return TRUE;
}

// prohodi dve polozky v seznamu
BOOL SECSwapItems(int index1, int index2)
{
    CShellExtConfigItem* item1;
    CShellExtConfigItem* item2;
    CShellExtConfigItem* next1;
    CShellExtConfigItem* next2;
    CShellExtConfigItem tmp;

    item1 = SECGetItem(index1);
    item2 = SECGetItem(index2);

    if (item1 == NULL || item2 == NULL)
        return FALSE;

    next1 = item1->Next;
    next2 = item2->Next;

    tmp = *item1;
    *item1 = *item2;
    *item2 = tmp;

    item1->Next = next1;
    item2->Next = next2;
    return TRUE;
}

// nastavi nazev polozky
BOOL SECSetName(int index, const char* name)
{
    CShellExtConfigItem* item = SECGetItem(index);

    if (item == NULL)
        return FALSE;

    lstrcpyn(item->Name, name, SEC_NAMEMAX);

    return TRUE;
}

#endif // ENABLE_SH_MENU_EXT

#endif //INSIDE_SALAMANDER
