// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

//****************************************************************************
//
// Copyright (c) 2023 Open Salamander Authors
//
// This is a part of the Open Salamander SDK library.
//
//****************************************************************************

#include "precomp.h"

#include "unfat.h"
#include "fat.h"

#include "unfat.rh"
#include "unfat.rh2"
#include "lang\lang.rh"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// rozhrani pro praci se soubory - platne od startu az do ukonceni pluginu
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

// zatim staci tohleto misto konfigurace
//DWORD Options;
COptions Options;

int SortByExtDirsAsFiles = FALSE; // aktualni hodnota konfiguracni promenne Salamandera SALCFG_SORTBYEXTDIRSASFILES

// casto pouzita chybova hlaska
const char* LOW_MEMORY = "Low memory";

//const char *CONFIG_OPTIONS = "Options";
//const char *CONFIG_CLEAR_READONLY = "Clear Read Only";
//const char *CONFIG_SESSION_AS_DIR = "Show Session As Directory";

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

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnFAT" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnFAT" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                          sizeof(SortByExtDirsAsFiles), NULL);

    if (!InitializeWinLib("UnFAT" /* neprekladat! */, DLLInstance))
        return NULL;
    SetWinLibStrings("Invalid number!", LoadStr(IDS_PLUGINNAME));

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnFAT" /* neprekladat! */, "ima");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
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
    {
        //    if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES)  // jde o chybu
        //    {
        char buf[1024];
        sprintf(buf, "%s\n\n%s", msg, SalamanderGeneral->GetErrorText(err));
        SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        //    }
    }
    return FALSE;
}

/*
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
    int l = strlen(buf);
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                  MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
  }
  SalamanderGeneral->ShowMessageBox(buf, LoadStr(title), MSGBOX_ERROR);
  return FALSE;
}
*/
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
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    // ZAKLADNI CAST
    salamander->AddPanelArchiver("ima", FALSE, FALSE);
    salamander->AddCustomUnpacker("UnFAT (Plugin)", "*.ima", FALSE);
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
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    return &InterfaceForArchiver;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander,
                                              const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    pluginData = NULL;

    // pokusime se otevrit FAT image
    BOOL ret = FALSE;
    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    CFATImage fatImage; // destruktor zavola Close
    if (fatImage.Open(fileName, FALSE, hParent))
    {
        // predame kompletni listing jadru Salamandera
        if (fatImage.ListImage(dir, hParent))
        {
            ret = TRUE;
        }
    }
    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData,
                                                const char* targetDir, const char* archiveRoot, SalEnumSelection next,
                                                void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, %s, %s, ,)", fileName, targetDir, archiveRoot);

    HWND hParent = SalamanderGeneral->GetMsgBoxParent();

    // pokusime se otevrit FAT image
    CFATImage fatImage; // destruktor zavola Close
    if (!fatImage.Open(fileName, FALSE, hParent))
        return FALSE;

    BOOL ret = FALSE;
    // spocitame 'totalSize' pro progress dialog
    BOOL isDir;
    CQuadWord size;
    CQuadWord totalSize(0, 0);
    CQuadWord realTotalSize(0, 0);
    const char* name;
    const CFileData* fileData;
    int errorOccured;
    while ((name = next(hParent, 1, &isDir, &size, &fileData, nextParam, &errorOccured)) != NULL)
    {
        if (isDir)
        {
            // directory
            size = COPY_MIN_FILE_SIZE;
        }
        else
        {
            // file
            realTotalSize += size;
            if (size < COPY_MIN_FILE_SIZE)
                size = COPY_MIN_FILE_SIZE;
        }
        totalSize += size;
    }

    // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci (tlacitko Cancel) +
    // test volneho mista na disku + pripadne vybalit
    if (errorOccured != SALENUM_CANCEL &&
        SalamanderGeneral->TestFreeSpace(hParent, targetDir, realTotalSize, LoadStr(IDS_PLUGINNAME)))
    {
        char title[2 * MAX_PATH];
        sprintf(title, LoadStr(IDS_EXTRACTING_ARCHIVE), SalamanderGeneral->SalPathFindFileName(fileName));
        salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
        // nastavime maximalni hodnotu pro teplomer TOTAL
        salamander->ProgressSetTotalSize(CQuadWord(-1, -1), totalSize);

        BOOL toSkip = FALSE;

        CAllocWholeFileEnum allocWholeFileOnStart = awfNeededTest;
        char skipPath[2 * MAX_PATH]; // vsechny poadresare a soubory pod touto cestou budou ignorovany
        skipPath[0] = 0;             // zatim zakazeme skip
        DWORD silentMask = 0;        // maska drzici skipovaci rezimy; 0 = no-skip
        ret = TRUE;
        next(NULL, -1, NULL, NULL, NULL, nextParam, NULL); // reset enumerace
        while ((name = next(NULL /* podruhe uz chyby nepiseme */, 1, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            char destPath[2 * MAX_PATH];
            strcpy(destPath, targetDir);

            char nameInArchive[2 * MAX_PATH];
            strcpy(nameInArchive, archiveRoot);
            SalamanderGeneral->SalPathAppend(nameInArchive, name, 2 * MAX_PATH);

            // nastavime hodnotu pro teplomer FILE na pocatek
            salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);

            if (SalamanderGeneral->SalPathAppend(destPath, name, 2 * MAX_PATH))
            {
                if (skipPath[0] != 0)
                {
                    if (SalamanderGeneral->PathIsPrefix(skipPath, destPath))
                        continue; // polozka lezi pod skipPath, takze ji ignorujeme
                    else
                        skipPath[0] = 0; // polozka nepatri pod skipPath, takze skipPath zahodime
                }

                if (isDir)
                {
                    // vytvorime cilovou cestu
                    BOOL skipped;
                    if (SalamanderSafeFile->SafeFileCreate(destPath, 0, 0, 0, TRUE, hParent, NULL, NULL, &silentMask, TRUE, &skipped,
                                                           skipPath, 2 * MAX_PATH, NULL, NULL) == INVALID_HANDLE_VALUE)
                    {
                        if (!skipped)
                        {
                            ret = FALSE;
                            break;
                        }
                    }
                    // nastavime maximalni hodnotu pro teplomer FILE
                    salamander->ProgressSetTotalSize(COPY_MIN_FILE_SIZE, CQuadWord(-1, -1));
                    // "extracting: %s..."
                    char progressText[2 * MAX_PATH + 100];
                    sprintf(progressText, LoadStr(IDS_EXTRACTING), nameInArchive);
                    salamander->ProgressDialogAddText(progressText, TRUE);

                    CQuadWord size2 = COPY_MIN_FILE_SIZE;
                    if (!salamander->ProgressAddSize((int)size2.Value, TRUE))
                    {
                        ret = FALSE;
                        break; // preruseni akce
                    }
                }
                else
                {
                    // file
                    // nastavime maximalni hodnotu pro teplomer FILE
                    salamander->ProgressSetTotalSize(fileData->Size, CQuadWord(-1, -1));

                    char* lastComp = _tcsrchr(destPath, '\\');
                    if (lastComp != NULL)
                        *lastComp = '\0';

                    BOOL skipped;
                    if (!fatImage.UnpackFile(salamander, fileName, nameInArchive, fileData, destPath,
                                             &silentMask, TRUE, &skipped, skipPath, 2 * MAX_PATH,
                                             hParent, &allocWholeFileOnStart))
                    {
                        if (!skipped)
                        {
                            ret = FALSE;
                            break;
                        }
                    }
                }
            }
            else
                TRACE_E("skipping " << name);
        } // while

        salamander->CloseProgressDialog();
    }
    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginData,
                                                const char* nameInArchive,
                                                const CFileData* fileData, const char* targetDir,
                                                const char* newFileName, BOOL* renamingNotSupported)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackOneFile(, %s, %s, , %s, ,)", fileName,
                        nameInArchive, targetDir);

    if (newFileName != NULL)
    {
        *renamingNotSupported = TRUE;
        return FALSE;
    }

    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    // pokusime se otevrit FAT image
    CFATImage fatImage; // destruktor zavola Close
    if (!fatImage.Open(fileName, FALSE, hParent))
        return FALSE;

    BOOL ret = TRUE;

    char title[2 * MAX_PATH];
    sprintf(title, LoadStr(IDS_EXTRACTING_ARCHIVE), SalamanderGeneral->SalPathFindFileName(fileName));
    salamander->OpenProgressDialog(title, FALSE, NULL, FALSE);
    CQuadWord totalSize;
    if (fileData->Size < COPY_MIN_FILE_SIZE)
        totalSize = COPY_MIN_FILE_SIZE;
    else
        totalSize = fileData->Size;
    salamander->ProgressSetTotalSize(totalSize, CQuadWord(-1, -1));

    DWORD silentDummy = 0;

    CAllocWholeFileEnum allocWholeFileOnStart = awfNeededTest;
    ret = fatImage.UnpackFile(salamander, fileName, nameInArchive, fileData, targetDir,
                              &silentDummy, FALSE, NULL, NULL, 0, hParent,
                              &allocWholeFileOnStart);

    salamander->CloseProgressDialog();

    return ret;
}

void CalcSize(CSalamanderDirectoryAbstract const* dir, CSalamanderMaskGroup* maskGroup,
              char* path, int pathBufSize, CQuadWord* totalSize, CQuadWord* realTotalSize)
{
    *totalSize += COPY_MIN_FILE_SIZE; // za adresar

    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetFile(i);

        if (maskGroup->AgreeMasks(file->Name, file->Ext))
        {
            *realTotalSize += file->Size;

            if (file->Size < COPY_MIN_FILE_SIZE)
                *totalSize += COPY_MIN_FILE_SIZE;
            else
                *totalSize += file->Size;
        }
    }

    count = dir->GetDirsCount();
    size_t pathLen = strlen(path);
    int j;
    for (j = 0; j < count; j++)
    {
        CFileData const* file = dir->GetDir(j);
        SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize);
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);
        CalcSize(subDir, maskGroup, path, pathBufSize, totalSize, realTotalSize);
        path[pathLen] = 0;
    }
}

BOOL ExtractArchive(CSalamanderDirectoryAbstract const* dir, CSalamanderMaskGroup* maskGroup,
                    CSalamanderForOperationsAbstract* salamander, CFATImage* img,
                    const char* archiveName, const char* targetDir,
                    char* path, int pathBufSize, DWORD* silent, char* skipPath, int skipPathMax)
{
    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    // napred soubory
    CAllocWholeFileEnum allocWholeFileOnStart = awfNeededTest;
    int filesCount = dir->GetFilesCount();
    int unpackedCount = 0;
    size_t pathLen = strlen(path);

    int i;
    for (i = 0; i < filesCount; i++)
    {
        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);

        CFileData const* fileData = dir->GetFile(i);

        if (maskGroup->AgreeMasks(fileData->Name, fileData->Ext))
        {
            // nastavime maximalni hodnotu pro teplomer FILE
            salamander->ProgressSetTotalSize(fileData->Size, CQuadWord(-1, -1));

            char myTargetDir[2 * MAX_PATH];
            lstrcpyn(myTargetDir, targetDir, 2 * MAX_PATH);
            SalamanderGeneral->SalPathAppend(myTargetDir, path, 2 * MAX_PATH);

            SalamanderGeneral->SalPathAppend(path, fileData->Name, pathBufSize);

            BOOL unpack = TRUE;
            if (skipPath[0] != 0)
            {
                if (SalamanderGeneral->PathIsPrefix(skipPath, myTargetDir))
                {
                    unpack = FALSE; // polozka lezi pod skipPath, takze ji ignorujeme
                }
                else
                    skipPath[0] = 0; // polozka nepatri pod skipPath, takze skipPath zahodime
            }

            if (unpack)
            {
                BOOL skipped;
                if (!img->UnpackFile(salamander, archiveName, path, fileData, myTargetDir, silent,
                                     TRUE, &skipped, skipPath, 2 * MAX_PATH, hParent, &allocWholeFileOnStart))
                {
                    // skip || skip all || cancel || chyba
                    if (!skipped)
                        return FALSE; // Cancel, musime vypadnout
                }
            }

            unpackedCount++;

            path[pathLen] = 0;
        }
    }

    // potom rekurzivne adresare
    int dirsCount = dir->GetDirsCount();
    pathLen = strlen(path);
    int j;
    for (j = 0; j < dirsCount; j++)
    {
        CFileData const* subDirData = dir->GetDir(j);
        SalamanderGeneral->SalPathAppend(path, subDirData->Name, pathBufSize);
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(j);

        BOOL unpack = TRUE;
        if (skipPath[0] != 0)
        {
            char testPath[2 * MAX_PATH];
            lstrcpyn(testPath, targetDir, 2 * MAX_PATH);
            SalamanderGeneral->SalPathAppend(testPath, path, 2 * MAX_PATH);

            if (SalamanderGeneral->PathIsPrefix(skipPath, testPath))
            {
                unpack = FALSE; // polozka lezi pod skipPath, takze ji ignorujeme
            }
            else
                skipPath[0] = 0; // polozka nepatri pod skipPath, takze skipPath zahodime
        }

        if (unpack)
        {
            if (!ExtractArchive(subDir, maskGroup, salamander, img, archiveName,
                                targetDir, path, pathBufSize, silent, skipPath, skipPathMax))
                return FALSE;
        }
        path[pathLen] = 0;
    }

    if (unpackedCount == 0 && dirsCount == 0)
    {
        // zanoreni do posledniho prazdneho adresare -- nechame vytvorit alespon ten adresar
        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
        // nastavime maximalni hodnotu pro teplomer FILE
        salamander->ProgressSetTotalSize(COPY_MIN_FILE_SIZE, CQuadWord(-1, -1));

        // prazdny adresar musime vytvorit explicitne
        char dirName[MAX_PATH];
        lstrcpyn(dirName, targetDir, MAX_PATH);
        SalamanderGeneral->SalPathAppend(dirName, path, MAX_PATH);

        // "extracting: %s..."
        char progressText[MAX_PATH + 100];
        sprintf(progressText, LoadStr(IDS_EXTRACTING), path);
        salamander->ProgressDialogAddText(progressText, TRUE);

        // vytvorime cilovou cestu
        BOOL skipped;
        if (SalamanderSafeFile->SafeFileCreate(dirName, 0, 0, 0, TRUE, hParent, NULL, NULL, silent, TRUE, &skipped,
                                               skipPath, 2 * MAX_PATH, NULL, NULL) == INVALID_HANDLE_VALUE)
        {
            if (!skipped)
                return FALSE;
        }

        CQuadWord size = COPY_MIN_FILE_SIZE;
        if (!salamander->ProgressAddSize((int)size.Value, TRUE))
            return FALSE;
    }

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    CSalamanderDirectoryAbstract* dir = SalamanderGeneral->AllocSalamanderDirectory(FALSE);
    if (dir == NULL)
        return FALSE;

    BOOL ret = FALSE;
    CPluginDataInterfaceAbstract* pluginData = NULL;
    if (ListArchive(salamander, fileName, dir, pluginData))
    {
        CSalamanderMaskGroup* maskGroup = SalamanderGeneral->AllocSalamanderMaskGroup();
        if (maskGroup != NULL)
        {
            maskGroup->SetMasksString(mask, FALSE);
            int err;
            if (maskGroup->PrepareMasks(err))
            {
                char path[MAX_PATH];
                path[0] = 0;
                CQuadWord totalSize(0, 0);
                CQuadWord realTotalSize(0, 0);
                CalcSize(dir, maskGroup, path, MAX_PATH, &totalSize, &realTotalSize);

                if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                                     targetDir, realTotalSize, LoadStr(IDS_PLUGINNAME)))
                {
                    if (delArchiveWhenDone)
                        archiveVolumes->Add(fileName, -2);

                    char title[2 * MAX_PATH];
                    sprintf(title, LoadStr(IDS_EXTRACTING_ARCHIVE), SalamanderGeneral->SalPathFindFileName(fileName));
                    salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
                    // nastavime maximalni hodnotu pro teplomer TOTAL
                    salamander->ProgressSetTotalSize(CQuadWord(-1, -1), totalSize);

                    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
                    CFATImage fatImage; // destruktor zavola Close
                    if (fatImage.Open(fileName, FALSE, hParent))
                    {
                        path[0] = 0;
                        DWORD silent = 0;
                        char skipPath[2 * MAX_PATH]; // vsechny poadresare a soubory pod touto cestou budou ignorovany
                        skipPath[0] = 0;             // zatim zakazeme skip
                        ret = ExtractArchive(dir, maskGroup, salamander, &fatImage, fileName, targetDir, path, MAX_PATH, &silent, skipPath, 2 * MAX_PATH);
                    }

                    salamander->CloseProgressDialog();
                }
            }
            SalamanderGeneral->FreeSalamanderMaskGroup(maskGroup);
        }

        if (pluginData != NULL)
        {
            // dir->Clear(pluginData); // nepouzivame, neni treba uvolnovat
            PluginInterface.ReleasePluginDataInterface(pluginData);
        }
    }

    SalamanderGeneral->FreeSalamanderDirectory(dir);

    return ret;
}
