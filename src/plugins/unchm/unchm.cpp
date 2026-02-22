// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "chmlib/types.h"
#include "unchm.h"
#include "chmfile.h"

#include "unchm.rh"
#include "unchm.rh2"
#include "lang\lang.rh"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle to the SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to the SLG - language-dependent resources

// plugin interface object; its methods are called by Salamander
CPluginInterface PluginInterface;
// archiver-specific part of CPluginInterface
CPluginInterfaceForArchiver InterfaceForArchiver;

// general Salamander interface - valid from startup until the plugin shuts down
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;

// interface for convenient work with files
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

// variable definition for "spl_com.h"
int SalamanderVersion = 0;

// configuration
COptions Options;

//const SYSTEMTIME  MinTime = { 1980, 01, 2, 01, 00, 00, 00, 000};

//const char *CONFIG_OPTIONS = "Options";
//const char *CONFIG_CLEAR_READONLY = "Clear Read Only";
//const char *CONFIG_SESSION_AS_DIR = "Show Session As Directory";
//const char *CONFIG_BOOTIMAGE_AS_FILE = "Show Boot Image As File";

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
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();
    // set SalamanderVersion for "spl_com.h"
    SalamanderVersion = salamander->GetVersion();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is built for the current Salamander version and newer - perform a check
    if (SalamanderVersion < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnCHM" /* do not translate! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // let it load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnCHM" /* do not translate! */);
    if (HLanguage == NULL)
        return NULL;

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    if (!InterfaceForArchiver.Init())
        return NULL;

    if (!InitializeWinLib("UnCHM" /* do not translate! */, DLLInstance))
        return NULL;
    SetWinLibStrings(LoadStr(IDS_INVALIDNUMBER), LoadStr(IDS_PLUGINNAME));

    // set the basic information about the plugin
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnCHM" /* do not translate! */, "chm");

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
        if (err != ERROR_FILE_NOT_FOUND && err != ERROR_NO_MORE_FILES) // this is an error
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
                "%s " VERSINFO_VERSION "\n\n" VERSINFO_COPYRIGHT "\n%s\n\n"
                "%s",
                LoadStr(IDS_PLUGINNAME),
                LoadStr(IDS_CHMLIBCOPYRIGHT),
                LoadStr(IDS_PLUGIN_DESCRIPTION));
    SalamanderGeneral->SalMessageBox(parent, buf, LoadStr(IDS_ABOUT), MB_OK | MB_ICONINFORMATION);
}

BOOL CPluginInterface::Release(HWND parent, BOOL force)
{
    CALL_STACK_MESSAGE2("CPluginInterface::Release(, %d)", force);

    ReleaseWinLib(DLLInstance);

    return TRUE;
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomUnpacker("UnCHM (Plugin)", "*.chm", FALSE);
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

    BOOL ret = FALSE;
    HWND hParent = SalamanderGeneral->GetMsgBoxParent();
    CCHMFile chm; // destructor calls Close
    if (chm.Open(fileName, FALSE))
    {
        // pass the complete listing to Salamander's core
        if (chm.EnumObjects(dir, pluginData))
            ret = TRUE;
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

    // try to open the CHM image
    CCHMFile chm; // destructor calls Close
    if (!chm.Open(fileName, FALSE))
        return FALSE;

    BOOL ret = FALSE;
    // compute 'totalSize' for the progress dialog
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
    // check whether no error occurred and the user did not request to cancel the operation (Cancel button)
    if (errorOccured == SALENUM_CANCEL)
        return FALSE;

    chm.ButtonFlags = BUTTONS_SKIPCANCEL;
    // unpack
    BOOL delTempDir = TRUE;
    if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                         targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
    {
        salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), TRUE, NULL, FALSE);
        salamander->ProgressSetTotalSize(CQuadWord(0, 0), totalSize);

        DWORD silent = 0;
        BOOL toSkip = FALSE;

        char currentPath[MAX_PATH];
        strcpy(currentPath, archiveRoot);

        ret = TRUE;
        next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);
        while ((name = next(NULL /* do not report errors the second time */, 1, &isDir, &size, &fileData, nextParam, NULL)) != NULL)
        {
            // directories do not concern us; they are created when unpacking files
            char destPath[MAX_PATH];
            strcpy(destPath, targetDir);

            if (SalamanderGeneral->SalPathAppend(destPath, name, MAX_PATH))
            {
                if (isDir)
                {
                    if (chm.UnpackDir(destPath, fileData) == UNPACK_CANCEL ||
                        !salamander->ProgressAddSize(1, TRUE))
                    {
                        ret = FALSE;
                        break;
                    }
                }
                else
                {
                    salamander->ProgressDialogAddText(name, TRUE); // delayedPaint==TRUE so we do not slow down

                    //  if the path we are unpacking to does not exist -> create it
                    char* lastComp = strrchr(destPath, '\\');
                    if (lastComp != NULL)
                    {
                        *lastComp = '\0';
                        SalamanderGeneral->CheckAndCreateDirectory(destPath);
                    } // if

                    salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
                    salamander->ProgressSetTotalSize(fileData->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

                    if (chm.ExtractObject(salamander, currentPath, destPath, fileData, silent, toSkip) == UNPACK_CANCEL ||
                        !salamander->ProgressAddSize(1, TRUE)) // correction for zero-sized files
                    {
                        ret = FALSE;
                        break;
                    }
                }
            }
        } // while

        salamander->CloseProgressDialog();
    }

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

    // open the CHM
    CCHMFile chm; // destructor calls Close
    if (!chm.Open(fileName, FALSE))
        return FALSE;

    BOOL ret = TRUE;

    salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), FALSE, NULL, FALSE);
    salamander->ProgressSetTotalSize(fileData->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

    char name[MAX_PATH];
    strcpy(name, targetDir);
    const char* lastComp = strrchr(nameInArchive, '\\');
    if (lastComp != NULL)
        lastComp++;
    else
        lastComp = nameInArchive;

    if (SalamanderGeneral->SalPathAppend(name, lastComp, MAX_PATH))
    {
        DWORD silent = 0;
        BOOL toSkip = FALSE;

        salamander->ProgressDialogAddText(name, TRUE); // delayedPaint==TRUE so we do not slow down

        char srcPath[MAX_PATH];
        lstrcpy(srcPath, nameInArchive);
        char* lComp = strrchr(srcPath, '\\');
        if (lComp != NULL)
            *lComp = '\0';

        chm.ButtonFlags = BUTTONS_OK;
        ret = chm.ExtractObject(salamander, srcPath, targetDir, fileData, silent, toSkip);
    } // if

    salamander->CloseProgressDialog();

    return ret == UNPACK_OK;
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
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetDir(i);
        //    TRACE_I("CalcSize(): directory: " << path << (path[0] != 0 ? "\\" : "") << file->Name);
        SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize);
        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(i);
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

        CQuadWord totalSize(0, 0);
        CQuadWord fileCount(0, 0);
        CalcSize(dir, mask, path, MAX_PATH, totalSize, fileCount);

        if (delArchiveWhenDone)
            archiveVolumes->Add(fileName, -2);

        //
        BOOL delTempDir = TRUE;
        if (SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                             targetDir, totalSize, LoadStr(IDS_UNPACKING_ARCHIVE)))
        {
            salamander->OpenProgressDialog(LoadStr(IDS_UNPACKING_ARCHIVE), TRUE, NULL, FALSE);
            salamander->ProgressSetTotalSize(CQuadWord(0, 0), totalSize);

            char modmask[256];
            SalamanderGeneral->PrepareMask(modmask, mask);

            // try to open the CHM image
            CCHMFile chm; // destructor calls Close
            if (chm.Open(fileName, FALSE))
            {
                DWORD silent = 0;
                BOOL toSkip = FALSE;
                char strTarget[MAX_PATH];
                lstrcpy(strTarget, targetDir);
                char srcPath[MAX_PATH];
                srcPath[0] = '\0';

                chm.ButtonFlags = BUTTONS_SKIPCANCEL;
                ret = chm.ExtractAllObjects(salamander, srcPath, dir, modmask, strTarget, MAX_PATH, silent, toSkip) != UNPACK_CANCEL;
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

// ****************************************************************************
//
// CPluginDataInterface
//

CPluginDataInterface::CPluginDataInterface()
{
}

CPluginDataInterface::~CPluginDataInterface()
{
}

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    CALL_STACK_MESSAGE1("CPluginDataInterface::ReleasePluginData(, )");

    delete (chmUnitInfo*)file.PluginData;
    file.PluginData = NULL;
}
