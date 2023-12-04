// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "uniso.h"
#include "isoimage.h"
#include "dialogs.h"

#include "uniso.rh"
#include "uniso.rh2"
#include "lang\lang.rh"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// ZLIB compression/decompression interface;
CSalamanderZLIBAbstract* SalZLIB = NULL;

// BZIP2 compression/decompression interface;
CSalamanderBZIP2Abstract* SalBZIP2 = NULL;

// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
CSalamanderGUIAbstract* SalamanderGUI = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 6
const char* CONFIG_VERSION = "Version";

// zatim staci tohleto misto konfigurace
//DWORD Options;
COptions Options;

CSalamanderBZIP2Abstract* GetSalamanderBZIP2();

const SYSTEMTIME MinTime = {1980, 01, 2, 01, 00, 00, 00, 000};

int SortByExtDirsAsFiles = FALSE; // aktualni hodnota konfiguracni promenne Salamandera SALCFG_SORTBYEXTDIRSASFILES

//const char *CONFIG_OPTIONS = "Options";
const char* CONFIG_CLEAR_READONLY = "Clear Read Only";
const char* CONFIG_SESSION_AS_DIR = "Show Session As Directory";
const char* CONFIG_BOOTIMAGE_AS_FILE = "Show Boot Image As File";

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    switch (fdwReason)
    {
    case DLL_PROCESS_ATTACH:
    {
        DLLInstance = hinstDLL;
        break;
    }

    case DLL_PROCESS_DETACH:
    {
        break;
    }
    }
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // nastavime SalamanderDebug pro "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // nastavime SalamanderVersion pro "spl_com.h"
    SalamanderVersion = salamander->GetVersion();
    HANDLES_CAN_USE_TRACE();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnISO" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnISO" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalZLIB = SalamanderGeneral->GetSalamanderZLIB();
    // ziskame rozhrani poskytujici upravene Windows controly pouzivane v Salamanderovi
    SalamanderGUI = salamander->GetSalamanderGUI();

#if LAST_VERSION_OF_SALAMANDER >= 33
    SalBZIP2 = SalamanderGeneral->GetSalamanderBZIP2();
#else
    SalBZIP2 = GetSalamanderBZIP2();
#endif

    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    // nastavime jmeno souboru s helpem
    SalamanderGeneral->SetHelpFileName("uniso.chm");

    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                          sizeof(SortByExtDirsAsFiles), NULL);

    if (!InterfaceForArchiver.Init())
        return NULL;

    if (!InitializeWinLib("UnISO" /* neprekladat! */, DLLInstance))
        return NULL;
    SetWinLibStrings("Invalid number!", LoadStr(IDS_PLUGINNAME));

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION |
                                       FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnISO" /* neprekladat! */, "iso;isz;nrg;bin;img;pdi;cdi;cif;ncd;c2d;dmg");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

BOOL Warning(int resID, BOOL quiet, ...)
{
    if (!quiet)
    {
        char buf[1024];
        buf[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(buf, LoadStr(resID), arglist);
        va_end(arglist);

        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_WARNING);
    }
    return FALSE;
}

BOOL Error(int resID, BOOL quiet, ...)
{
    if (!quiet)
    {
        char buf[1024];
        buf[0] = 0;
        va_list arglist;
        va_start(arglist, quiet);
        vsprintf(buf, LoadStr(resID), arglist);
        va_end(arglist);

        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
    }
    return FALSE;
}

BOOL Error(char* msg, DWORD err, BOOL quiet)
{
    if (!quiet)
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES) // jde o chybu
        {
            char buf[1024];
            sprintf(buf, "%s\n\n%s", msg, SalamanderGeneral->GetErrorText(err));
            SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        }

    return FALSE;
}

BOOL SysError(int title, int error, ...)
{
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE3("SysError(%d, %d, ...)", title, error);
    char buf[1024];
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        strcat(buf, " ");
        int l = (int)strlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->ShowMessageBox(buf, LoadStr(title), MSGBOX_ERROR);
    return FALSE;
}

// ****************************************************************************
//
// CPluginInterface
//

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    ReleaseWinLib(DLLInstance);

    return TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
            ConfigVersion = 0; // default configuration
    }
    else
    {
        ConfigVersion = 0; // default configuration
    }

    // set defaults
    Options.ClearReadOnly = TRUE;
    Options.SessionAsDirectory = TRUE; // implicitne ukazeme, jak jsme dobry (muzou si to pripadne vypnout)
    Options.BootImageAsFile = TRUE;    // implicitne ukazujeme boot image (muzou si to pripadne vypnout)

    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_CLEAR_READONLY, REG_DWORD, &Options.ClearReadOnly, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_SESSION_AS_DIR, REG_DWORD, &Options.SessionAsDirectory, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_BOOTIMAGE_AS_FILE, REG_DWORD, &Options.BootImageAsFile, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));

    registry->SetValue(regKey, CONFIG_CLEAR_READONLY, REG_DWORD, &Options.ClearReadOnly, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_SESSION_AS_DIR, REG_DWORD, &Options.SessionAsDirectory, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_BOOTIMAGE_AS_FILE, REG_DWORD, &Options.BootImageAsFile, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CConfigurationDialog dlg(parent);
    dlg.Execute();
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    /*  OBECNA PRAVIDLA PRO IMPLEMENTACI CONNECT (pro slozitejsi pluginy s verzi konfigurace - promenna
                                                ConfigVersion a konstanta CURRENT_CONFIG_VERSION):
    -s kazdou zmenou je potreba zvysit cislo verze CURRENT_CONFIG_VERSION
     (v prvni verzi je CURRENT_CONFIG_VERSION=1, neni 0, aby se dal odlisit upgrade
      od instalace)
    -do zakladni casti (pred podminky "if (ConfigVersion < YYY)"):
      -se napise kod pro prvni instalaci pluginu (stav, kdy plugin jeste nema zaznam
       v Salamanderovi)
      -u volani AddCustomPacker resp. AddCustomUnpacker dame do parametru 'update'
       podminku "ConfigVersion < XXX", kde XXX je cislo posledni verze, kde se
       menily pripony pro custom packery resp. unpackery (XXX pro packery muze byt jine
       nez pro unpackery)
      -AddMenuItem, SetChangeDriveMenuItem a SetThumbnailLoader funguje pri kazdem loadu
       pluginu stejne (instalace/upgrady se nelisi - vzdy se zacina na zelene louce)
    -do casti pro upgrady (za zakladni casti):
      -pridame podminku "if (ConfigVersion < XXX)", kde XXX je nova hodnota
       konstanty CURRENT_CONFIG_VERSION + pridame komentar od teto verze;
       v tele teto podminky zavolame:
        -pokud pribyly pripony pro "panel archiver", zavolame
         "AddPanelArchiver(PPP, EEE, TRUE)", kde PPP jsou jen nove pripony oddelene
         strednikem a EEE je TRUE/FALSE ("panel view+edit"/"jen panel view")
        -pokud pribyly pripony pro "viewer", zavolame "AddViewer(PPP, TRUE)",
         kde PPP jsou jen nove pripony oddelene strednikem
        -pokud se maji smazat nejake stare pripony pro "viewer", zavolame
         pro kazdou takovou priponu PPP "ForceRemoveViewer(PPP)"
        -pokud se maji smazat nejake pripony pro "panel archiver", dejte vedet
         Petrovi, zatim to nikdo nepotreboval, takze to neni implementovane
  */

    // Davide, az budes pridavat dalsi pripony, je treba zvedat CURRENT_CONFIG_VERSION, viz ^^^

    // ZAKLADNI CAST
    // AddViewer a AddPanelArchiver budou podlehat CASTI PRO UPGRADY
    salamander->AddViewer("*.bin;*.img;*.iso;*.isz;*.nrg;*.pdi;*.cdi;*.cif;*.ncd;*.c2d;*.mdf", FALSE); // default (install pluginu), jinak Salam ignoruje
    salamander->AddPanelArchiver("iso;isz;nrg;bin;img;pdi;cdi;cif;ncd;c2d;mdf;dmg", FALSE, FALSE);

    // ve verzi 3 jsme pridali C2D
    // ve verzi 4 jsme pridali MDF
    salamander->AddCustomUnpacker("UnISO (Plugin)",
                                  "*.iso;*.isz;*.nrg;*.bin;*.img;*.pdi;*.cdi;*.cif;*.ncd;*.c2d;*.mdf;*.dmg", ConfigVersion < 6);

    // nastavime ikonku pluginu
    HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_UNISO),
                                      IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
    salamander->SetBitmapWithIcons(hBmp);
    DeleteObject(hBmp);
    salamander->SetPluginIcon(0);
    salamander->SetPluginMenuAndToolbarIcon(0);

    // CAST PRO UPGRADY
    if (ConfigVersion < 2) // pridani nrg, pdi, cdi, cif, ncd
    {
        salamander->AddViewer("*.nrg;*.pdi;*.cdi;*.cif;*.ncd", TRUE);
        salamander->AddPanelArchiver("nrg;pdi;cdi;cif;ncd", FALSE, TRUE);
    }

    if (ConfigVersion < 3) // pridani c2d
    {
        salamander->AddViewer("*.c2d", TRUE);
        salamander->AddPanelArchiver("c2d", FALSE, TRUE);
    }

    if (ConfigVersion < 4) // pridani mdf/mds
    {
        salamander->AddViewer("*.mdf", TRUE);
        salamander->AddPanelArchiver("mdf", FALSE, TRUE);
    }

    if (ConfigVersion < 5) // pridani dmg
    {
        salamander->AddViewer("*.dmg", TRUE);
        salamander->AddPanelArchiver("dmg", FALSE, TRUE);
    }

    if (ConfigVersion < 6) // pridani isz
    {
        salamander->AddViewer("*.isz", TRUE);
        salamander->AddPanelArchiver("isz", FALSE, TRUE);
    }
    //  _CrtSetBreakAlloc(13012);
}

void CPluginInterface::Event(int event, DWORD param)
{
    if (event == PLUGINEVENT_CONFIGURATIONCHANGED)
    {
        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                              sizeof(SortByExtDirsAsFiles), NULL);
    }
}

void CPluginInterface::ReleasePluginDataInterface(CPluginDataInterfaceAbstract* pluginData)
{
    delete ((CPluginDataInterface*)pluginData);
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    return &InterfaceForArchiver;
}

CPluginInterfaceForViewerAbstract*
CPluginInterface::GetInterfaceForViewer()
{
    return &InterfaceForViewer;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

CPluginInterfaceForArchiver::CPluginInterfaceForArchiver()
{
}

BOOL CPluginInterfaceForArchiver::Init()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Init()");

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander,
                                              const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);

    pluginData = new CPluginDataInterface();
    if (pluginData == NULL)
    {
        return Error(IDS_INSUFFICIENT_MEMORY);
    }

    CPluginDataInterface* pd = (CPluginDataInterface*)pluginData;

    // pokusime se otevrit ISO image
    BOOL ret = FALSE;
    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    CISOImage isoImage; // destruktor zavola Close
    isoImage.DisplayMissingCCDWarning = pd->DisplayMissingCCDWarning;
    if (isoImage.Open(fileName, FALSE))
    {
        // predame kompletni listing jadru Salamandera
        if (isoImage.ListImage(dir, pluginData))
        {
            ret = TRUE;

            pd->DisplayMissingCCDWarning = isoImage.DisplayMissingCCDWarning;
        }
    }

    if (ret == FALSE)
    {
        delete (CPluginDataInterface*)pluginData;
        pluginData = NULL;
    }

    return ret;
}

void FreeString(void* strig)
{
    free(strig);
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    if (pluginData == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    CPluginDataInterface* pd = (CPluginDataInterface*)pluginData;

    // pokusime se otevrit ISO image
    CISOImage isoImage; // destruktor zavola Close
    isoImage.DisplayMissingCCDWarning = pd->DisplayMissingCCDWarning;
    if (!isoImage.Open(fileName, FALSE))
        return FALSE;

    BOOL ret = FALSE;
    // spocitat 'totalSize' pro progress dialog
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    CQuadWord fileCount(0, 0);
    const char* name;
    const CFileData* fileData;
    int errorOccured;
    while ((name = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, &fileData, nextParam, &errorOccured)) != NULL)
    {
        if (!isDir)
        {
            totalSize += size;
            ++fileCount;
        } // if

        totalSize += CQuadWord(1, 0);
    }

    // vybalit
    BOOL delTempDir = TRUE;
    if (errorOccured != SALENUM_CANCEL && // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci (tlacitko Cancel)
        SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                         targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
    {
        salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), TRUE, NULL, FALSE);
        salamander->ProgressSetTotalSize(CQuadWord(0, 0), totalSize);

        DWORD silent = 0;
        BOOL toSkip = FALSE, bAudioEncountered = FALSE;

        char currentISOPath[ISO_MAX_PATH_LEN];
        strcpy(currentISOPath, archiveRoot);

        ret = TRUE;
        next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
        while ((name = next(NULL /* podruhe uz chyby nepiseme */, 1, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            // adresare nas nezajimaji, jejich vytvareni se dela pri vybalovani souboru
            char destPath[MAX_PATH];
            strncpy_s(destPath, targetDir, _TRUNCATE);

            if (SalamanderGeneral->SalPathAppend(destPath, name, MAX_PATH))
            {
                if (isDir)
                {
                    if (isoImage.UnpackDir(destPath, fileData) == UNPACK_CANCEL ||
                        !salamander->ProgressAddSize(1, TRUE))
                    {
                        ret = FALSE;
                        break;
                    }
                }
                else
                {
                    salamander->ProgressDialogAddText(name, TRUE); // delayedPaint==TRUE, abychom nebrzdili

                    //  pokud neexistuje cesta kam rozbalujeme -> vytvorit ji
                    char* lastComp = strrchr(destPath, '\\');
                    if (lastComp != NULL)
                    {
                        *lastComp = '\0';
                        SalamanderGeneral->CheckAndCreateDirectory(destPath);
                    } // if

                    salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
                    salamander->ProgressSetTotalSize(fileData->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

                    int err = isoImage.UnpackFile(salamander, currentISOPath, destPath, fileData, silent, toSkip);

                    if ((UNPACK_AUDIO_UNSUP == err) && !bAudioEncountered)
                    {
                        bAudioEncountered = TRUE;
                        Error(IDS_AUDIO_NOT_EXTRACTABLE);
                    }

                    if (err == UNPACK_CANCEL || !salamander->ProgressAddSize(1, TRUE)) // correction for zero-sized files
                    {
                        ret = FALSE;
                        break;
                    }
                }
            }
            else
            {
                Error(IDS_ERR_TOO_LONG_NAME);
                ret = FALSE;
                break;
            }
        } // while

        salamander->CloseProgressDialog();
    }

    pd->DisplayMissingCCDWarning = isoImage.DisplayMissingCCDWarning;

    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* nameInArchive, const CFileData* fileData,
                                                const char* targetDir, const char* newFileName,
                                                BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);

    if (newFileName != NULL)
    {
        *renamingNotSupported = TRUE;
        return FALSE;
    }

    if (pluginData == NULL)
    {
        TRACE_E("Internal error");
        return FALSE;
    }

    CPluginDataInterface* pd = (CPluginDataInterface*)pluginData;

    // pokusime se otevrit ISO image
    CISOImage isoImage; // destruktor zavola Close
    isoImage.DisplayMissingCCDWarning = pd->DisplayMissingCCDWarning;
    if (!isoImage.Open(fileName, FALSE))
        return FALSE;

    BOOL ret = TRUE;

    salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), FALSE, NULL, FALSE);
    salamander->ProgressSetTotalSize(fileData->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

    char name[MAX_PATH];
    strncpy_s(name, targetDir, _TRUNCATE);
    const char* lastComp = strrchr(nameInArchive, '\\');
    if (lastComp != NULL)
        lastComp++;
    else
        lastComp = nameInArchive;

    if (SalamanderGeneral->SalPathAppend(name, lastComp, MAX_PATH))
    {
        DWORD silent = 0;
        BOOL toSkip = FALSE;
        int err;

        salamander->ProgressDialogAddText(name, TRUE); // delayedPaint==TRUE, abychom nebrzdili

        char srcPath[MAX_PATH];
        strcpy(srcPath, nameInArchive);
        char* lComp = strrchr(srcPath, '\\');
        if (lComp != NULL)
            *lComp = '\0';

        err = isoImage.UnpackFile(salamander, srcPath, targetDir, fileData, silent, toSkip);
        if (UNPACK_AUDIO_UNSUP == err)
            Error(IDS_AUDIO_NOT_EXTRACTABLE);
        ret = err == UNPACK_OK;
    } // if
    else
    {
        Error(IDS_ERR_TOO_LONG_NAME);
        ret = FALSE;
    }

    salamander->CloseProgressDialog();

    pd->DisplayMissingCCDWarning = isoImage.DisplayMissingCCDWarning;

    return ret;
}

void CalcSize(CSalamanderDirectoryAbstract const* dir, const char* mask, char* path, int pathBufSize,
              CQuadWord& size, CQuadWord& fileCount)
{
    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetFile(i);
        //    TRACE_I("CalcSize(): file: " << path << (path[0] != 0 ? "\\" : "") << file->Name);

        if (SalamanderGeneral->AgreeMask(file->Name, mask, file->Ext[0] != 0))
        {
            size += file->Size + CQuadWord(1, 0);
            ++fileCount;
        }
    }

    count = dir->GetDirsCount();
    int pathLen = (int)strlen(path);
    int j;
    for (j = 0; j < count; j++)
    {
        CFileData const* file = dir->GetDir(j);
        //    TRACE_I("CalcSize(): directory: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
        SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize);
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);
        CalcSize(subDir, mask, path, pathBufSize, size, fileCount);
        path[pathLen] = 0;
    }
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    CSalamanderDirectoryAbstract* dir = SalamanderGeneral->AllocSalamanderDirectory(FALSE);
    if (dir == NULL)
        return Error(IDS_INSUFFICIENT_MEMORY);

    BOOL ret = FALSE;
    CPluginDataInterfaceAbstract* pluginData = NULL;
    if (ListArchive(salamander, fileName, dir, pluginData))
    {
        char path[MAX_PATH];
        path[0] = 0;

        CQuadWord totalSize(0, 0);
        CQuadWord fileCount(0, 0);
        CalcSize(dir, mask, path, MAX_PATH, totalSize, fileCount);

        BOOL delTempDir = TRUE;
        if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                             targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
        {
            salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), TRUE, NULL, FALSE);
            salamander->ProgressSetTotalSize(CQuadWord(0, 0), totalSize);

            char modmask[256];
            SalamanderGeneral->PrepareMask(modmask, mask);

            // pokusime se otevrit ISO image
            CISOImage isoImage; // destruktor zavola Close
            if (isoImage.Open(fileName, FALSE))
            {
                if (delArchiveWhenDone)
                    archiveVolumes->Add(fileName, -2);

                DWORD silent = 0;
                BOOL toSkip = FALSE;
                char strTarget[MAX_PATH];
                strcpy(strTarget, targetDir);
                char srcPath[ISO_MAX_PATH_LEN];
                srcPath[0] = '\0';
                ret = isoImage.ExtractAllItems(salamander, srcPath, dir, modmask, strTarget, MAX_PATH, silent, toSkip) != UNPACK_CANCEL;
            }

            salamander->CloseProgressDialog();
        }

        if (pluginData != NULL)
        {
            dir->Clear(pluginData);
            PluginInterface.ReleasePluginDataInterface(pluginData);
        }

        int panel = -1;
        CanCloseArchive(salamander, fileName, TRUE, panel);
    }

    SalamanderGeneral->FreeSalamanderDirectory(dir);

    return ret;
}

BOOL CPluginInterfaceForArchiver::CanCloseArchive(CSalamanderForOperationsAbstract* salamander,
                                                  const char* fileName,
                                                  BOOL force, int panel)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::CanCloseArchive(, %s, %d, %d)", fileName, force, panel);

    return TRUE;
}

//
// ****************************************************************************
// CPluginInterfaceForViewer
//

BOOL CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width,
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

    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    CISOImage* image;
    if ((image = new CISOImage()) == NULL)
    {
        SetCursor(hOldCur);
        return Error(IDS_INSUFFICIENT_MEMORY);
    }

    if (!image->Open(name))
    {
        delete image;
        SetCursor(hOldCur);
        return FALSE;
    }

    /*
  // open the last track
  int lastTrack = image->GetLastTrack();
  if (!image->OpenTrack(lastTrack)) {
    Error(IDS_CANT_OPEN_TRACK, FALSE, lastTrack);
    delete image;
    return FALSE;
  }
*/

    char tempFileName[MAX_PATH];
    if (SalamanderGeneral->SalGetTempFileName(NULL, "ISO", tempFileName, TRUE, NULL))
    {
        char caption[2000];
        int err;
        CSalamanderPluginInternalViewerData vData;

        // vytvorim docasny soubor a naleju do nej dump modulu
        FILE* outStream = fopen(tempFileName, "w");
        if (!image->DumpInfo(outStream))
        {
            // ?muze vubec nastat?
        }
        fclose(outStream);
        delete image;

        // soubor predam Salamanderovi - ten si jej presune do cache a az ho prestane
        // pouzivat, smaze ho
        vData.Size = sizeof(vData);
        vData.FileName = tempFileName;
        vData.Mode = 0; // text mode
        sprintf(caption, "%s - %s", name, LoadStr(IDS_PLUGINNAME));
        vData.Caption = caption;
        vData.WholeCaption = TRUE;
        if (!SalamanderGeneral->ViewFileInPluginViewer(NULL, &vData, TRUE, NULL, "iso_dump.txt", err))
        {
            // soubor je smazan i v pripade neuspechu
        }
    }
    else
    {
        SetCursor(hOldCur);
        SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(), LoadStr(IDS_ERR_TMP),
                                         LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONEXCLAMATION);
        delete image;
    }

    SetCursor(hOldCur);
    return TRUE;
}

BOOL CPluginInterfaceForViewer::CanViewFile(const char* name)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForViewer::CanViewFile(%s)", name);

    BOOL canView = FALSE;

    CISOImage* image;
    if ((image = new CISOImage()) != NULL)
    {
        if (image->Open(name, TRUE))
            canView = TRUE;

        delete image;
    }

    return canView;
}

// ****************************************************************************
//
// CPluginDataInterface
//

CPluginDataInterface::CPluginDataInterface()
{
    // initialy display the 'missing CCD file' warning
    DisplayMissingCCDWarning = TRUE;
}

CPluginDataInterface::~CPluginDataInterface()
{
}

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    CALL_STACK_MESSAGE1("CPluginDataInterface::ReleasePluginData(, )");

    delete (CISOImage::CFilePos*)(file.PluginData);
    file.PluginData = NULL;
}
