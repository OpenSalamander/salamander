// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "unOLE2.h"
#include "dialogs.h"

#include "unOLE2.rh"
#include "unOLE2.rh2"
#include "lang\lang.rh"

#define BUF_SIZE 65536

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// cast interfacu CPluginInterface pro archivator
CPluginInterfaceForArchiver InterfaceForArchiver;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// definice promenne pro "spl_com.h"
int SalamanderVersion = 0;

const SYSTEMTIME MinTime = {1980, 01, 2, 01, 00, 00, 00, 000};

//const char *CONFIG_OPTIONS = "Options";

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    CALL_STACK_MESSAGE_NONE
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
    CALL_STACK_MESSAGE_NONE
    return LAST_VERSION_OF_SALAMANDER;
}

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    CALL_STACK_MESSAGE_NONE
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
                   "UnOLE2" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnOLE2" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    /*
  //beta plati do konce unora 2001
  SYSTEMTIME st;
  GetLocalTime(&st);
  if (st.wYear == 2001 && st.wMonth > 2 || st.wYear > 2001)
  {
    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_EXPIRE), LoadStr(IDS_PLUGINNAME), MSGBOX_INFO);
    return NULL;
  }
  */

    if (!InterfaceForArchiver.Init())
        return NULL;

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnOLE2" /* neprekladat! */, "ole"); // "UnOLE2", "doc;xls");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
// Callback functions
//

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

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");

    /*  Options = 0;
  if (regKey != NULL)   // load z registry
  {
    registry->GetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Options, sizeof(DWORD));
  }*/
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
    //  registry->SetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Options, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomUnpacker("UnOLE2 (Plugin)", "*.ole", FALSE);
    salamander->AddPanelArchiver("ole", FALSE, FALSE);
    //  salamander->AddCustomUnpacker("UnOLE2 (Plugin)", "*.doc;*.xls", FALSE);
    //  salamander->AddPanelArchiver("doc;xls", FALSE, FALSE);
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForArchiver;
}

int SortByExtDirsAsFiles = FALSE; // globalka, kterou je potreba obnovit pred volanim ParseStorage

BOOL ParseStorage(CSalamanderDirectoryAbstract* Dir, LPSTORAGE CF, LPMALLOC pIMalloc, char* path)
{
    STATSTG element;
    HRESULT hr;
    IEnumSTATSTG* pIEnum;
    CFileData fileData;
    char* oldPath = path + strlen(path);
    char name[MAX_PATH];
    FILETIME* pFT;
    BOOL ret = TRUE;
    int tmp;

    hr = CF->EnumElements(0, NULL, 0 /*threeth:)) reserved*/, &pIEnum);
    if (FAILED(hr))
        return FALSE;
    while (!FAILED(hr))
    {
        hr = pIEnum->Next(1, &element, NULL);
        if (FAILED(hr) || !element.pwcsName)
            break;

        WideCharToMultiByte(CP_ACP, 0, element.pwcsName, -1, name, sizeof(name), NULL, NULL);
        name[sizeof(name) - 1] = 0;
        fileData.Name = SalamanderGeneral->DupStr(name);
        if (!fileData.Name)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            ret = FALSE;
            pIMalloc->Free(element.pwcsName);
            continue;
        }
        fileData.Ext = strrchr(fileData.Name, '.');
        if (fileData.Ext != NULL)
            fileData.Ext++; // ".cvspass" ve Windows je pripona
        else
            fileData.Ext = fileData.Name + lstrlen(fileData.Name);
        fileData.Size = CQuadWord(element.cbSize.u.LowPart, element.cbSize.u.HighPart);
        fileData.Attr = 0;
        if (element.type == STGTY_STORAGE)
        {
            fileData.Attr |= FILE_ATTRIBUTE_DIRECTORY;
        }
        fileData.Hidden = 0;
        fileData.PluginData = -1; // zbytecne, jen tak pro formu
        if (element.mtime.dwLowDateTime && element.mtime.dwHighDateTime)
        {
            pFT = &element.mtime;
        }
        else if (element.ctime.dwLowDateTime && element.ctime.dwHighDateTime)
        {
            pFT = &element.ctime;
        }
        else
        {
            pFT = &element.atime;
        }
        fileData.LastWrite.dwLowDateTime = pFT->dwLowDateTime;
        fileData.LastWrite.dwHighDateTime = pFT->dwHighDateTime;
        fileData.DosName = NULL;
        fileData.NameLen = lstrlen(fileData.Name);
        fileData.IsOffline = 0;
        if (element.type == STGTY_STORAGE)
        {
            fileData.IsLink = 0;
            if (!SortByExtDirsAsFiles)
                fileData.Ext = fileData.Name + fileData.NameLen; // adresare nemaji pripony
            tmp = Dir->AddDir(path, fileData, NULL);
        }
        else
        {
            fileData.IsLink = SalamanderGeneral->IsFileLink(fileData.Ext);
            tmp = Dir->AddFile(path, fileData, NULL);
        }
        if (!tmp)
        {
            free(fileData.Name);
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            ret = FALSE;
        }
        if (element.type == STGTY_STORAGE)
        {
            LPSTORAGE pSubStorage;

            // concatenate path
            if (path != oldPath)
            {
                strcpy(oldPath, "\\");
            }
            strcat(oldPath, fileData.Name);

            hr = CF->OpenStorage(element.pwcsName, NULL /*priority*/,
                                 /*STGM_READ | */ STGM_DIRECT | STGM_SHARE_EXCLUSIVE /*| STGM_SHARE_DENY_WRITE/*EXCLUSIVE*/, NULL /*skip*/,
                                 0 /*reserved*/, &pSubStorage);
            if (SUCCEEDED(hr))
            {
                ret &= ParseStorage(Dir, pSubStorage, pIMalloc, path);
                pSubStorage->Release();
            }
            // restore path
            *oldPath = 0;
        }
        pIMalloc->Free(element.pwcsName);
    }
    pIEnum->Release();
    return ret;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

BOOL OpenStorage(const char* fileName, LPSTORAGE* ppStorage, LPMALLOC* ppIMalloc)
{
    OLECHAR* CFName = NULL;
    HRESULT hr;
    int len;

    len = (int)strlen(fileName) + 1;
    CFName = new OLECHAR[len];
    if (!CFName)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }
    MultiByteToWideChar(CP_ACP, 0, fileName, len, CFName, len);
    CFName[len - 1] = 0;

    hr = StgOpenStorage(CFName, NULL, STGM_DIRECT | STGM_READ | STGM_SHARE_DENY_WRITE /*EXCLUSIVE*/, NULL, 0, ppStorage);
    delete[] CFName;
    if (FAILED(hr))
    {
        Error(hr, IDS_CANNOT_OPEN, fileName);
        return FALSE;
    }
    hr = CoGetMalloc(MEMCTX_TASK, ppIMalloc);
    if (FAILED(hr))
    {
        Error(hr, IDS_MEMORY_ALLOCATOR);
        (*ppStorage)->Release();
        return FALSE;
    }
    return TRUE;
} /* OpenStorage */

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    BOOL ret;
    char path[MAX_PATH + 2];
    LPSTORAGE pStorage;
    LPMALLOC pIMalloc;

    if (!OpenStorage(fileName, &pStorage, &pIMalloc))
    {
        return FALSE;
    }

    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &SortByExtDirsAsFiles,
                                          sizeof(SortByExtDirsAsFiles), NULL);

    // path is used to accumulate path inside the CF
    path[0] = 0;
    ret = ParseStorage(dir, pStorage, pIMalloc, path);
    pStorage->Release();
    pIMalloc->Release();
    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    LPSTORAGE pStorage;
    LPMALLOC pIMalloc;
    BOOL ret = TRUE;

    if (!OpenStorage(fileName, &pStorage, &pIMalloc))
    {
        return FALSE;
    }

    salamander->OpenProgressDialog(LoadStr(IDS_EXTRPROGTITLE), TRUE, NULL, FALSE);

    // unpack the files

    pStorage->Release();
    pIMalloc->Release();
    salamander->CloseProgressDialog();

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

    LPSTORAGE pStorage;
    LPMALLOC pIMalloc;
    BOOL ret = TRUE;
    LPSTREAM pStream;

    if (!OpenStorage(fileName, &pStorage, &pIMalloc))
    {
        return FALSE;
    }
    // extract the file
    if (!strchr(nameInArchive, '\\'))
    {
        OLECHAR* StreamName = NULL;
        HRESULT hr;
        char targetFileName[MAX_PATH];
        char* pBuf;
        HANDLE hFile;
        int len;

        len = (int)strlen(nameInArchive) + 1;
        StreamName = new OLECHAR[len];
        pBuf = (char*)malloc(BUF_SIZE);
        if (StreamName && pBuf)
        {
            MultiByteToWideChar(CP_ACP, 0, nameInArchive, len, StreamName, len);
            StreamName[len - 1] = 0;
            hr = pStorage->OpenStream(StreamName, NULL, STGM_DIRECT | STGM_READ | STGM_SHARE_EXCLUSIVE, NULL, &pStream);
            if (SUCCEEDED(hr))
            {
                strcpy(targetFileName, targetDir);
                SalamanderGeneral->SalPathAddBackslash(targetFileName, MAX_PATH + 2);
                strcat(targetFileName, nameInArchive);
                hFile = CreateFile(targetFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_NEW, NULL, NULL);
                if (hFile != INVALID_HANDLE_VALUE)
                {
                    for (;;)
                    {
                        ULONG nBytesRead;
                        DWORD nBytesWritten;

                        hr = pStream->Read(pBuf, BUF_SIZE, &nBytesRead);
                        if (FAILED(hr))
                        {
                            Error(hr, IDS_CANNOT_READ, targetFileName);
                            ret = FALSE;
                            break;
                        }
                        if (!nBytesRead)
                        {
                            // done
                            break;
                        }
                        WriteFile(hFile, pBuf, nBytesRead, &nBytesWritten, NULL);
                        if (nBytesRead != nBytesWritten)
                        {
                            Error(GetLastError(), IDS_CANNOT_WRITE);
                            ret = FALSE;
                            break;
                        }
                    }
                    CloseHandle(hFile);
                }
                else
                {
                    Error(GetLastError(), IDS_CANNOT_CREATE_FILE, targetFileName);
                    ret = FALSE;
                }
                pStream->Release();
            }
            else
            {
                Error(hr, IDS_CANNOT_OPEN_STREAM, nameInArchive);
                ret = FALSE;
            }
        }
        else
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            ret = FALSE;
        }
        if (pBuf)
            free(pBuf);
        if (StreamName)
            delete[] StreamName;
    }
    else
    {
        // not supported yet
        _ASSERTE(strchr(nameInArchive, '\\'));
    }
    pStorage->Release();
    pIMalloc->Release();

    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    LPSTORAGE pStorage;
    LPMALLOC pIMalloc;
    BOOL ret = TRUE;

    if (!OpenStorage(fileName, &pStorage, &pIMalloc))
    {
        return FALSE;
    }

    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2); // FIXME: az plugin doucime multi-volume archivy, musime sem napridavat vsechny svazky archivu (aby se smazal kompletni archiv)

    salamander->OpenProgressDialog(LoadStr(IDS_EXTRPROGTITLE), FALSE, NULL, TRUE);

    // Unpack the compound file

    pStorage->Release();
    pIMalloc->Release();
    salamander->CloseProgressDialog();

    return ret;
}

BOOL Error(HRESULT hr, int error, ...)
{
    CALL_STACK_MESSAGE_NONE

    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::Error(%d, )", error);
    char buf[1024]; //temp variable
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (hr != S_OK)
    {
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, hr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);

    return FALSE;
}

BOOL CPluginInterfaceForArchiver::Init()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Init()");

    return TRUE;
}
