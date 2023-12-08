// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "fileio.h"
#include "tardll.h"
#include "tar.h"
#include "gzip/gzip.h"
#include "rpm/rpm.h"

#include "tar.rh"
#include "tar.rh2"
#include "lang\lang.rh"

// TODO: vyresit case-sensitivity
// TODO: vyresit vyskyt vice souboru se stejnym jmenem v jednom archivu
// TODO: dodelat vypis u rpm vieweru (konverze datumu do citelneho formatu atp.)
// TODO: zacistit dealokace u vieweru, pri chybe to dela dost nepruhledne veci
// TODO: zkontrolovat tar proti specifikaci, jestli pokryvam maximum flagu (GNU rozsireni atp.)
// TODO: udelat revizi chybovych hlasek a kde se pouzivaji (TAR x CPIO)
// TODO: opravit hlaseni chyb, pokud failne stream (nespolehat na jeho ErrorNumber, ale zajistit, aby k nemu byl vzdy text)
// TODO: vyresit multivolume archivy
// TODO: vyresit archivy s ridkymi soubory
// TODO: nesly by z linku delat shortcuty?
// TODO: pridat LZMA kompresi do RPM (viz flex-32bit-2.5.35-43.88.s390x.rpm sample) Vypada to na dost ojedinele pouzivane, asi pridat az se ozve vic lidi...

//
// ****************************************************************************
//
// Deklarace a definice
//
// ****************************************************************************
//

// ConfigVersion: 0 - instalace pluginu nebo verze pustena se Servant Salamander 2.0
//                    (bez cisla konfigurace)
//                1 - pracovni verze pred Servant Salamander 2.5 beta 1, pridan TBZ,BZ,BZ2 a RPM
//                2 - pracovni verze pred Servant Salamander 2.5 beta 1, pridano CPIO (i viewer *.CPIO)
//                3 - pracovni verze pred Servant Salamander 2.5 beta 1, vyhozeni vieweru *.CPIO
//                4 - pracovni verze pred Servant Salamander 2.5 beta 1, pridani .z archivu
//                5 - pracovni verze pred Servant Salamander 2.52 beta 2, pridani .DEB archivu

int ConfigVersion = 0;
#define CURRENT_CONFIG_VERSION 5
const char* CONFIG_VERSION = "Version";

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;
// cast interfacu CPluginInterface pro viewer
CPluginInterfaceForViewer InterfaceForViewer;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

//
// ****************************************************************************
//
// Pomocne funkce
//
// ****************************************************************************
//

// Natahuje string z dll
char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

// da dohromady string z resourcu a pripadny chybovy string
char* LoadErr(int resID, DWORD LastError)
{
    static char buffer[1000];
    strcpy(buffer, LoadStr(resID));
    if (LastError != 0)
        strcat(buffer, SalamanderGeneral->GetErrorText(LastError));
    return buffer;
}

//
// ****************************************************************************
//
// Inicializace pluginu
//
// ****************************************************************************
//

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;
    return TRUE; // DLL can be loaded
}

//
// ****************************************************************************
//
// Funkce pro komunikaci se Salamandrem
//
// ****************************************************************************
//

// ****************************************************************************
// SalamanderPluginGetReqVer - vraci pozadovanou verzi Salamandera
//
int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

// ****************************************************************************
// SalamanderPluginEntry - hlavni vstupni bod
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
                   "TAR" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "TAR" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    // vypis testovaciho hlaseni
    TRACE_I("SalamanderPluginEntry called, Salamander version " << salamander->GetVersion());

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_LOADSAVECONFIGURATION |
                                       FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_VIEWER,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "TAR" /* neprekladat! */, "tar;tgz;taz;tbz;gz;bz;bz2;xz;zst;z;rpm;cpio;deb");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

void CPluginInterface::About(HWND parent)
{
    char buf[1000];
    _snprintf_s(buf, _TRUNCATE,
                "%s " VERSINFO_VERSION "\n"
                VERSINFO_COPYRIGHT "\n\n"
                "bzip2 library Copyright © 1996-2010 Julian R Seward\n"
                "Zstandard library Copyright © 2016-2023 Facebook, Inc.\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    if (regKey != NULL) // load z registry
    {
        if (!registry->GetValue(regKey, CONFIG_VERSION, REG_DWORD, &ConfigVersion, sizeof(DWORD)))
        {
            ConfigVersion = 0; // chyba - bereme verzi bez loadu
        }
    }
    else // default config
    {
        ConfigVersion = 0; // bez loadu
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    DWORD v = CURRENT_CONFIG_VERSION;
    registry->SetValue(regKey, CONFIG_VERSION, REG_DWORD, &v, sizeof(DWORD));
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect()");

    // zakladni cast:
    salamander->AddCustomUnpacker("TAR (Plugin)",
                                  "*.tar;*.tgz;*.tbz;*.taz;"
                                  "*.tar.gz;*.tar.bz;*.tar.bz2;*.tar.z;"
                                  "*_tar.gz;*_tar.bz;*_tar.bz2;*_tar.z;"
                                  "*_tar_gz;*_tar_bz;*_tar_bz2;*_tar_z;"
                                  "*.tar_gz;*.tar_bz;*.tar_bz2;*.tar_z;"
                                  "*.gz;*.bz;*.bz2;*.z;"
                                  "*.rpm;*.cpio;*.deb",
                                  ConfigVersion < 5);                                       // pri upgradech se ignoruje, az na pripad, kdy se upgraduje na verzi 4 - nutny update kvuli "*.z" a dalsim
    salamander->AddPanelArchiver("tgz;tbz;taz;tar;gz;bz;bz2;xz;zst;z;rpm;cpio;deb", FALSE, FALSE); // pri upgradech pluginu se ignoruje
    salamander->AddViewer("*.rpm", FALSE);                                                  // pri upgradech pluginu se ignoruje, az na pripad, kdy se upgraduje z verze, ktera jeste viewer nemela (verze pustena s SS 2.0)

    // cast pro upgrady:
    if (ConfigVersion < 1) // 1 - pracovni verze pred Servant Salamander 2.5 beta 1, pridan tbz,bz,bz2 a rpm
    {
        salamander->AddPanelArchiver("tbz;bz;bz2;rpm", FALSE, TRUE);
    }

    if (ConfigVersion < 2) // 2 - pracovni verze pred Servant Salamander 2.5 beta 1, pridano cpio (i viewer *.cpio - to byl omyl)
    {
        salamander->AddPanelArchiver("cpio", FALSE, TRUE);

        // pridani *.cpio byla chyba, ve verzi 3 ji odmazeme
        //salamander->AddViewer("*.cpio", TRUE);
    }

    /*
  if (ConfigVersion < 3)    // 3 - pracovni verze pred Servant Salamander 2.5 beta 1, vyhozeni vieweru *.cpio
  {
    salamander->ForceRemoveViewer("*.cpio");   // zakomentujeme az budeme povazovat za dead-code
  }
*/

    if (ConfigVersion < 4) // 4 - pracovni verze pred Servant Salamander 2.5 beta 1, pridani .z archivu
    {
        salamander->AddPanelArchiver("taz;z", FALSE, TRUE);
    }
    if (ConfigVersion < 5) // 5 - pracovni verze pred Servant Salamander 2.52 beta 2, pridani .deb archivu
    {
        salamander->AddPanelArchiver("deb", FALSE, TRUE);
    }
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

//
// ****************************************************************************
//
// Vykonne funkce pluginu
//
// ****************************************************************************
//

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander,
                                              const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    pluginData = NULL;

    // vytvorime objekt archivu
    CArchiveAbstract* archive = CreateArchive(fileName, salamander);
    // a vylistujeme ho
    if (archive)
    {
        BOOL ret = archive->ListArchive(NULL, dir);
        delete archive;
        return ret;
    }
    return FALSE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* targetDir, const char* archiveRoot,
                                                SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s,,,)",
                        fileName, targetDir, archiveRoot);

    // vytvorime objekt archivu
    CArchiveAbstract* archive = CreateArchive(fileName, salamander);
    // a vybalime ho
    if (archive)
    {
        BOOL ret = archive->UnpackArchive(targetDir, archiveRoot, next, nextParam);
        delete archive;
        return ret;
    }
    return FALSE;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* nameInArchive, const CFileData* fileData,
                                                const char* targetDir, const char* newFileName,
                                                BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, , %s, , %s, ,)",
                        fileName, nameInArchive, targetDir);

    // vytvorime objekt archivu
    CArchiveAbstract* archive = CreateArchive(fileName, salamander);
    // a vybalime ho
    if (archive)
    {
        BOOL ret = archive->UnpackOneFile(nameInArchive, fileData, targetDir, newFileName);
        delete archive;
        return ret;
    }
    return FALSE;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander,
                                                     const char* fileName, const char* mask,
                                                     const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    // vytvorime objekt archivu
    CArchiveAbstract* archive = CreateArchive(fileName, salamander);
    // a vybalime ho
    if (archive)
    {
        if (delArchiveWhenDone)
            archiveVolumes->Add(fileName, -2); // FIXME: az plugin doucime multi-volume archivy, musime sem napridavat vsechny svazky archivu (aby se smazal kompletni archiv)
        BOOL ret = archive->UnpackWholeArchive(mask, targetDir);
        delete archive;
        return ret;
    }
    return FALSE;
}

/*
BOOL
CPluginInterfaceForArchiver::PackToArchive(CSalamanderForOperationsAbstract *salamander,
                                           const char *fileName, const char *archiveRoot,
                                           BOOL move, const char *sourcePath,
                                           SalEnumSelection2 next, void *nextParam)
{
  CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::PackToArchive(, %s, %s, %d, %s,,)",
                      fileName, archiveRoot, move, sourcePath);
  return FALSE;
}

BOOL
CPluginInterfaceForArchiver::DeleteFromArchive(CSalamanderForOperationsAbstract *salamander,
                                               const char *fileName, CPluginDataInterfaceAbstract *pluginData,
                                               const char *archiveRoot, SalEnumSelection next,
                                               void *nextParam)
{
  CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DeleteFromArchive(, %s, , %s, ,)",
                      fileName, archiveRoot);
  return FALSE;
}
*/

// funkce pro "file viewer", vola se pri pozadavku na otevreni viewru a nacteni souboru
// 'name', 'left'+'right'+'width'+'height'+'showCmd'+'alwaysOnTop' je doporucene umisteni
// okna, je-li 'returnUnlock' FALSE nemaji 'unlock'+'unlockOwner' zadny vyznam,
// je-li 'returnUnlock' TRUE, mel by viewer vratit system-event 'unlock'
// v non-signaled stavu, do signaled stavu 'unlock' prejde v okamziku ukonceni prohlizeni
// souboru 'name' (soubor je v tomto okamziku odstranen z docasneho adresare), dale by mel
// vratit v 'unlockOnwer' TRUE pokud ma byt objekt 'unlock' uzavren volajicim (FALSE znamena,
// ze si viewer 'unlock' rusi sam); pokud viewer nenastavi 'unlock' (zustava NULL)
// je soubor 'name' platny jen do ukonceni volani teto metody ViewFile; neni-li
// 'viewerData' NULL, jde o predani rozsirenych parametru viewru (viz
// CSalamanderGeneralAbstract::ViewFileInPluginViewer); vraci TRUE pri uspechu
// (FALSE znamena neuspech, 'unlock' a 'unlockOwner' v tomto pripade nemaji zadny vyznam)
BOOL CPluginInterfaceForViewer::ViewFile(const char* name, int left, int top, int width, int height,
                                         UINT showCmd, BOOL alwaysOnTop, BOOL returnUnlock, HANDLE* unlock,
                                         BOOL* unlockOwner, CSalamanderPluginViewerData* viewerData,
                                         int enumFilesSourceUID, int enumFilesCurrentIndex)
{
    CALL_STACK_MESSAGE11("CPluginInterfaceForViewer::ViewFile(%s, %d, %d, %d, %d, "
                         "0x%X, %d, %d, , , , %d, %d)",
                         name, left, top, width, height,
                         showCmd, alwaysOnTop, returnUnlock, enumFilesSourceUID, enumFilesCurrentIndex);

    // unlock zustava NULL, nepotrebujeme lockovani
    //   takze returnUnlock a unlockOwner take ignorujeme
    // viewerData nepouzivame

    // ulozime puvodni kurzor a nahodime presypacky (i kdyz dlouho by to trvat nemelo...)
    HCURSOR hOldCur = SetCursor(LoadCursor(NULL, IDC_WAIT));

    // otevreme vstupni soubor
    HANDLE file = CreateFile(name, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
                             FILE_FLAG_SEQUENTIAL_SCAN, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        int err = GetLastError();
        SetCursor(hOldCur);
        SalamanderGeneral->ShowMessageBox(LoadErr(IDS_GZERR_FOPEN, err),
                                          LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // naalokujeme buffer pro cteni souboru
    unsigned char* buffer = (unsigned char*)malloc(BUFSIZE);
    if (buffer == NULL)
    {
        SetCursor(hOldCur);
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_GZERR_TITLE),
                                          MSGBOX_ERROR);
        CloseHandle(file);
        return FALSE;
    }
    // precteme prvni blok dat
    DWORD read;
    if (!ReadFile(file, buffer, BUFSIZE, &read, NULL))
    {
        // chyba cteni
        int err = GetLastError();
        SetCursor(hOldCur);
        free(buffer);
        CloseHandle(file);
        SalamanderGeneral->ShowMessageBox(LoadErr(IDS_ERR_FREAD, err), LoadStr(IDS_GZERR_TITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // ziskame nazev pro temporary textovy soubor s vysledky
    char tempFileName[MAX_PATH];
    if (!SalamanderGeneral->SalGetTempFileName(NULL, "RPV", tempFileName, TRUE, NULL))
    {
        SetCursor(hOldCur);
        free(buffer);
        CloseHandle(file);
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_RPMERR_TMPNAME), LoadStr(IDS_ERR_RPMTITLE), MSGBOX_ERROR);
        return FALSE;
    }

    // vytvorime docasny soubor
    FILE* fContents = fopen(tempFileName, "wt");
    if (fContents == NULL)
    {
        char buff[500];
        SetCursor(hOldCur);
        free(buffer);
        CloseHandle(file);
        buff[499] = '\0';
        strcpy(buff, LoadStr(IDS_RPMERR_TMPFILE));
        strncat(buff, name, 499 - strlen(buff));
        SalamanderGeneral->ShowMessageBox(buff, LoadStr(IDS_ERR_RPMTITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // vytvorime RPM objekt a naplnime docasny soubor informacema
    CDecompressFile* archive = new CRPM(name, file, buffer, read, fContents);
    if (archive == NULL)
    {
        SetCursor(hOldCur);
        fclose(fContents);
        DeleteFile(tempFileName);
        free(buffer);
        CloseHandle(file);
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_MEMORY), LoadStr(IDS_ERR_RPMTITLE), MSGBOX_ERROR);
        return FALSE;
    }
    // TODO: asi by to chtelo zkontrolovat, aby se vevnitr chyby nehlasily a jen se nastavila promenna podle chyby
    if (!archive->IsOk())
    {
        SetCursor(hOldCur);
        fclose(fContents);
        DeleteFile(tempFileName);
        free(buffer);
        CloseHandle(file);
        if (archive->GetErrorCode() == 0)
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERR_NORPM), LoadStr(IDS_ERR_RPMTITLE), MSGBOX_ERROR);
        else
            SalamanderGeneral->ShowMessageBox(LoadStr(archive->GetErrorCode()),
                                              LoadStr(IDS_ERR_RPMTITLE), MSGBOX_ERROR);
        delete archive;
        return FALSE;
    }
    // ted uz muzeme archiv zase zavrit...
    // vstupni soubor a buffer se ulizi v destruktoru CDecompressFile
    delete archive;
    // hotovo
    fclose(fContents);

    // pripravime strukturu pro text viewer salamandra
    CSalamanderPluginInternalViewerData textViewerData;
    textViewerData.Size = sizeof(textViewerData);
    textViewerData.FileName = tempFileName;
    textViewerData.Mode = 0; // text mode
    char caption[500];
    strncpy_s(caption, 451, name, _TRUNCATE);
    strcat(caption, " - ");
    strcat(caption, LoadStr(IDS_RPM_VIEWTITLE));
    textViewerData.Caption = caption;
    textViewerData.WholeCaption = TRUE;
    // zobrazime soubor v text-view salamandera s jeho naslednym smazanim
    int err;
    SalamanderGeneral->ViewFileInPluginViewer(NULL, &textViewerData, TRUE, NULL, "rpm_dump.txt", err);

    // a nakonec uklid
    SetCursor(hOldCur);
    return TRUE;
}
