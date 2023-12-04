// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

// vim: et:sw=2:ts=2

#include "precomp.h"

#include "peviewer.h"
#include "peviewer.rh"
#include "peviewer.rh2"
#include "lang\lang.rh"
#include "pefile.h"
#include "cfgdlg.h"
#include "cfg.h"

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalGeneral = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// ConfigVersion: 0 - default
//                1 - pracovni verze SS 2.1 beta 1
//                2 - verze SS 2.5 beta 1
//                3 - AS 3.07: lze nastavit viditelnost jednotlivych casti dumpu
int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 3
LPCTSTR CONFIG_VERSION = _T("Version");
LPCTSTR CONFIG_DUMPERS = _T("Dumpers");

struct CFG_LOAD_ITEM
{
    const CFG_DUMPER* pDumperCfg;
    DWORD dwOrder;
};

//
// ****************************************************************************
// DllMain
//

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;
    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
// LoadStr
//

LPTSTR LoadStr(int resID)
{
    return SalGeneral->LoadStr(HLanguage, resID);
}

//
// ****************************************************************************
// SalamanderPluginGetReqVer
//

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

//
// ****************************************************************************
// SalamanderPluginEntry
//

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   _T("Portable Executable Viewer") /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), _T("Portable Executable Viewer") /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    if (!InitializeWinLib(_T("PEVIEWER") /* neprekladat! */, DLLInstance))
    {
        return NULL;
    }

    // ziskame obecne rozhrani Salamandera
    SalGeneral = salamander->GetSalamanderGeneral();

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGIN_NAME),
                                   FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION | FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "PEVIEWER");

    salamander->SetPluginHomePageURL("www.altap.cz");

    // Defaulni konfigurace.
    BuildDefaultDumperChain();

    return &PluginInterface;
}

//
// ****************************************************************************
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    TCHAR buf[1000];
    _sntprintf(buf, 1000,
               _T("%s ") VERSINFO_VERSION _T("\n\n") VERSINFO_COPYRIGHT _T("\n\n")
               _T("%s"),
               LoadStr(IDS_PLUGIN_NAME),
               LoadStr(IDS_PLUGIN_DESCRIPTION));
    buf[999] = 0;
    SalGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    ReleaseWinLib(DLLInstance);
    return TRUE;
}

static int CmpCfgLoadItems(void* context, const void* x, const void* y)
{
    UNREFERENCED_PARAMETER(context);

    const CFG_LOAD_ITEM* item1 = (const CFG_LOAD_ITEM*)x;
    const CFG_LOAD_ITEM* item2 = (const CFG_LOAD_ITEM*)y;

    if (item1->dwOrder == 0)
    {
        if (item2->dwOrder == 0)
        {
            return 0;
        }
        else
        {
            return 1;
        }
    }
    else if (item2->dwOrder == 0)
    {
        return -1;
    }

    return (int)item1->dwOrder - (int)item2->dwOrder;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = CURRENT_CONFIG_VERSION; // asi nejakej nenechavec... ;-)
        }

        HKEY subKey;
        if (registry->OpenKey(regKey, CONFIG_DUMPERS, subKey))
        {
            // Poradi dumperu nacitam z registru do tohoto pole. Polozky pak seradim
            // podle hodnot nactenych z registru. Nepouzivam hodnotu z registru primo
            // pro indexovani pole, protoze nejaky stroura to muze zmenit, konfigurace
            // muze byt poskozena apod.
            CFG_LOAD_ITEM dumpersInRegistry[AVAILABLE_PE_DUMPERS] = {
                0,
            };

            for (int i = 0; i < AVAILABLE_PE_DUMPERS; i++)
            {
                DWORD dwOrder;
                if (!registry->GetValue(subKey, g_cfgDumpers[i].pszKey, REG_DWORD, &dwOrder, sizeof(DWORD)))
                {
                    // Pokud neni dumper nastaven v registru, defaultne ho zapnu, ale az nakonec stavajiciho vystupu.
                    dwOrder = 1000 + i;
                }
                dumpersInRegistry[i].pDumperCfg = &g_cfgDumpers[i];
                dumpersInRegistry[i].dwOrder = dwOrder;
            }

            registry->CloseKey(subKey);

            qsort_s(dumpersInRegistry, AVAILABLE_PE_DUMPERS, sizeof(CFG_LOAD_ITEM), CmpCfgLoadItems, NULL);

            g_cfgChainLength = 0;
            for (int i = 0; i < AVAILABLE_PE_DUMPERS; i++)
            {
                if (dumpersInRegistry[i].dwOrder == 0)
                {
                    // Disablovane dumpery byly prerazeny na konec pole, jakmile narazim
                    // na prvni disablovany, tak koncim.
                    break;
                }

                g_cfgChain[i].pDumperCfg = dumpersInRegistry[i].pDumperCfg;
                ++g_cfgChainLength;
            }
        }
    }
    else // default configuration
    {
        ConfigVersion = 0;
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));

    HKEY subKey;
    if (registry->CreateKey(regKey, CONFIG_DUMPERS, subKey))
    {
        for (int i = 0; i < AVAILABLE_PE_DUMPERS; i++)
        {
            const CFG_DUMPER* dumper = &g_cfgDumpers[i];
            int nDumperIndexInChain = FindDumperInChain(dumper);                   // 0..n-1, -1=not found
            DWORD dwOrder = nDumperIndexInChain < 0 ? 0 : nDumperIndexInChain + 1; // 1..n, 0=disabled
            registry->SetValue(subKey, dumper->pszKey, REG_DWORD, &dwOrder, sizeof(dwOrder));
        }

        registry->CloseKey(subKey);
    }
}

void CPluginInterface::Configuration(HWND parent)
{
    CPeViewerConfigDialog(parent).Execute();
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");
    salamander->AddViewer("*.cpl;*.dll;*.drv;*.exe;*.ocx;*.spl;*.sys;*.scr", FALSE); // default (install pluginu), jinak Salam ignoruje

    if (ConfigVersion == 1) // v teto pracovni verzi pred SS 2.5 beta 1 jsme *.SCR odstranili (toto je oprava)
    {
        salamander->AddViewer("*.scr", TRUE); // opetovne pridani pripony "*.scr" (uz mame CPluginInterfaceForViewerAbstract::CanViewFile)
    }
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}

//
// ****************************************************************************
// CPluginInterfaceForViewer
//

BOOL MapFileToMemory(LPCTSTR name, HANDLE& hFile, HANDLE& hFileMapping,
                     LPVOID& lpFileBase, DWORD& fileSize, BOOL quietMode)
{
    hFile = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL,
                       OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);

    if (hFile == INVALID_HANDLE_VALUE)
    {
        if (!quietMode)
        {
            TCHAR buff[2000];
            _stprintf(buff, LoadStr(IDS_ERR_OPEN_FILE), name);
            SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), buff,
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
        }
        return FALSE;
    }

    fileSize = GetFileSize(hFile, NULL);
    if (fileSize == -1)
    {
        if (!quietMode)
        {
            TCHAR buff[2000];
            _stprintf(buff, LoadStr(IDS_ERR_OPEN_FILE), name);
            SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), buff,
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
        }
        CloseHandle(hFile);
        return FALSE;
    }

    hFileMapping = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hFileMapping == 0)
    {
        if (!quietMode)
        {
            TCHAR buff[2000];
            _stprintf(buff, LoadStr(IDS_ERR_OPEN_FILE), name);
            SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), buff,
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
        }
        CloseHandle(hFile);
        return FALSE;
    }

    lpFileBase = MapViewOfFile(hFileMapping, FILE_MAP_READ, 0, 0, 0);
    if (lpFileBase == 0)
    {
        if (!quietMode)
        {
            TCHAR buff[2000];
            _stprintf(buff, LoadStr(IDS_ERR_OPEN_FILE), name);
            SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), buff,
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
        }
        CloseHandle(hFileMapping);
        CloseHandle(hFile);
        return FALSE;
    }
    return TRUE;
}

void UnmapFileFromMemory(HANDLE& hFile, HANDLE& hFileMapping, LPVOID& lpFileBase)
{
    UnmapViewOfFile(lpFileBase);
    CloseHandle(hFileMapping);
    CloseHandle(hFile);
}

BOOL CPluginInterfaceForViewer::ViewFile(LPCTSTR name, int left, int top, int width,
                                         int height, UINT showCmd, BOOL alwaysOnTop,
                                         BOOL returnLock, HANDLE* lock, BOOL* lockOwner,
                                         CSalamanderPluginViewerData* viewerData,
                                         int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    CALL_STACK_MESSAGE11("CPluginInterfaceForViewer::ViewFile(%s, %d, %d, %d, %d, "
                         "0x%X, %d, %d, , , , %d, %d)",
                         name, left, top, width, height,
                         showCmd, alwaysOnTop, returnLock, enumFilesSourceUID, enumFilesCurrentIndex);

    // 'lock' ani 'lockOwner' nenastavujeme, staci nam platnost souboru 'name' jen
    // v ramci teto metody

    HANDLE hFile;
    HANDLE hFileMapping;
    LPVOID lpFileBase;
    DWORD fileSize;

    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    // pokusim se soubor namapovat do pameti
    if (!MapFileToMemory(name, hFile, hFileMapping, lpFileBase, fileSize, FALSE))
    {
        SetCursor(hOldCur);
        return FALSE;
    }

    TCHAR tempFileName[MAX_PATH];
    if (SalGeneral->SalGetTempFileName(NULL, _T("PEV"), tempFileName, TRUE, NULL))
    {
        TCHAR caption[2000];
        int err;
        CSalamanderPluginInternalViewerData vData;

        // vytvorim docasny soubor a naleju do nej dump modulu
        FILE* outStream = _tfopen(tempFileName, _T("w"));
        if (!DumpFileInfo(lpFileBase, fileSize, outStream))
        {
            TCHAR buff[2000];
            SetCursor(hOldCur);
            _stprintf(buff, LoadStr(IDS_EXCEPTION), name);
            SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), buff,
                                      LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
            SetCursor(LoadCursor(NULL, IDC_WAIT));
        }
        fclose(outStream);

        // soubor predam Salamanderovi - ten si jej presune do cache a az ho prestane
        // pouzivat, smaze ho
        vData.Size = sizeof(vData);
        vData.FileName = tempFileName;
        vData.Mode = 0; // text mode
        _stprintf(caption, _T("%s - %s"), name, LoadStr(IDS_PLUGIN_NAME));
        vData.Caption = caption;
        vData.WholeCaption = TRUE;
        if (!SalGeneral->ViewFileInPluginViewer(NULL, &vData, TRUE, NULL, _T("pe_dump.txt"), err))
        {
            // soubor je smazan i v pripade neuspechu
        }
    }
    else
    {
        SetCursor(hOldCur);
        SalGeneral->SalMessageBox(SalGeneral->GetMsgBoxParent(), LoadStr(IDS_ERR_TMP),
                                  LoadStr(IDS_PLUGIN_NAME), MB_OK | MB_ICONEXCLAMATION);
    }

    // odmapuju soubor z pameti
    UnmapFileFromMemory(hFile, hFileMapping, lpFileBase);

    SetCursor(hOldCur);
    return TRUE;
}

BOOL CPluginInterfaceForViewer::CanViewFile(LPCTSTR name)
{
    HANDLE hFile;
    HANDLE hFileMapping;
    LPVOID lpFileBase;
    DWORD fileSize;

    // pokusim se soubor namapovat do pameti (tise -- zadne chybove hlasky)
    if (!MapFileToMemory(name, hFile, hFileMapping, lpFileBase, fileSize, TRUE))
        return FALSE;

    // vytahnu typ havicky
    CPEFile PEFile(lpFileBase, fileSize);
    DWORD imageType = PEFile.ImageFileType(NULL);

    // odmapuju soubor z pameti
    UnmapFileFromMemory(hFile, hFileMapping, lpFileBase);

    // pokud jde o znamou hlavicku, povolim nase otevreni
    return imageType == IMAGE_DOS_SIGNATURE || imageType == IMAGE_OS2_SIGNATURE ||
           imageType == IMAGE_VXD_SIGNATURE || imageType == IMAGE_NT_SIGNATURE;
}
