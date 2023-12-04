// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "array2.h"

#include "fdi.h"
#include "uncab.h"
#include "dialogs.h"

#include "uncab.rh"
#include "uncab.rh2"
#include "lang\lang.rh"

// The only sizes seen so far are 0x10660 & 0x17c7c
#define SFX_BUFF_SIZE 0x18000

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

// zatim staci tohleto misto konfigurace
DWORD Options;

const SYSTEMTIME MinTime = {1980, 01, 2, 01, 00, 00, 00, 000};

const char* CONFIG_OPTIONS = "Options";

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

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // tento plugin je delany pro aktualni verzi Salamandera a vyssi - provedeme kontrolu
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // starsi verze odmitneme
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnCAB" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnCAB" /* neprekladat! */);
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
                                   "UnCAB" /* neprekladat! */, "cab");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
// CCABCacheEntry
//

CCABCacheEntry::CCABCacheEntry(char* name, char* path)
{
    CALL_STACK_MESSAGE_NONE
    strcpy(CABName, name);
    strcpy(CABPath, path);
}

// ****************************************************************************
//
// Callback functions
//

void HUGE* FAR DIAMONDAPI Malloc(ULONG cb)
{
    CALL_STACK_MESSAGE_NONE
    return malloc(cb);
}

void FAR DIAMONDAPI Free(void HUGE* pv)
{
    CALL_STACK_MESSAGE_NONE
    free(pv);
}

INT_PTR FAR DIAMONDAPI Open(LPSTR pszFile, int oflag, int pmode)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Open(pszFile, oflag, pmode);
}

UINT FAR DIAMONDAPI Read(INT_PTR hf, void FAR* pv, UINT cb)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Read(hf, pv, cb);
}

UINT FAR DIAMONDAPI Write(INT_PTR hf, void FAR* pv, UINT cb)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Write(hf, pv, cb);
}

int FAR DIAMONDAPI Close(INT_PTR hf)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Close(hf);
}

long FAR DIAMONDAPI Seek(INT_PTR hf, long dist, int seektype)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Seek(hf, dist, seektype);
}

INT_PTR FAR DIAMONDAPI Notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.Notify(fdint, pfdin);
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

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    Options = 0;
    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Options, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
    registry->SetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Options, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    ConfigDialog(parent);
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomUnpacker("UnCAB (Plugin)", "*.cab", FALSE);
    salamander->AddPanelArchiver("cab", FALSE, FALSE);
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForArchiver;
}

// ****************************************************************************
//
// CPluginInterfaceForArchiver
//

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    Salamander = salamander;
    pluginData = NULL;
    Action = CA_LIST;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    NotWholeArchListed = FALSE;
    IOError = FALSE;
    FirstCAB = TRUE;
    HFDI hfdi;
    ERF err;
    char arcPath[MAX_PATH + 2];
    char* arcName;
    Dir = dir;
    Count = 0;

    strcpy(arcPath, fileName);
    if (!SalamanderGeneral->CutDirectory(arcPath, &arcName))
        return FALSE;
    strcpy(NextCAB, arcName);
    SalamanderGeneral->SalPathAddBackslash(arcPath, MAX_PATH + 2);

    memset(&err, 0, sizeof(ERF));
    hfdi = FDICreate(Malloc, Free, ::Open, ::Read, ::Write, ::Close, ::Seek, cpuUNKNOWN, &err);
    if (!hfdi)
        return FDIError(err.erfOper);

    while (1)
    {
        FirstCABINET_INFO = TRUE;
        ret = FDICopy(hfdi, NextCAB, arcPath, 0, ::Notify, NULL, this);
        if (!ret)
        {
            FDIError(err.erfOper);
            break;
        }
        if (!*NextCAB)
            break;
        FirstCAB = FALSE;
        char buffer[MAX_PATH + CB_MAX_CABINET_NAME + 1];
        strcpy(buffer, arcPath);
        SalamanderGeneral->SalPathAppend(buffer, NextCAB, MAX_PATH + CB_MAX_CABINET_NAME + 1);
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(buffer);
        if (attr == -1 || attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            NotWholeArchListed = TRUE;
            break;
        }
    }

    FDIDestroy(hfdi);

    if (ret && NotWholeArchListed && !(Options & OP_NO_VOL_ATTENTION))
        AttentionDialog(SalamanderGeneral->GetMainWindowHWND());

    //nejake soubory jsme jiz vylistovali, tak to nezabalime a zobrazime je
    if (!ret && Count)
        ret = TRUE;

    Salamander = NULL;

    return ret;
}

void FreeString(void* strig)
{
    CALL_STACK_MESSAGE_NONE
    free(strig);
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    Salamander = salamander;
    Action = CA_UNPACK;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    IOError = FALSE;
    FirstCAB = TRUE;
    HFDI hfdi;
    ERF err;
    Count = 0;
    ArcRoot = archiveRoot ? archiveRoot : "";
    if (*ArcRoot == '\\')
        ArcRoot++;
    RootLen = lstrlen(ArcRoot);
    TargetDir = targetDir;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;

    strcpy(CurrentCABPath, fileName);
    char* arcName;
    if (!SalamanderGeneral->CutDirectory(CurrentCABPath, &arcName))
        return FALSE;
    strcpy(CurrentCAB, arcName);
    SalamanderGeneral->SalPathAddBackslash(CurrentCABPath, MAX_PATH + 2);

    memset(&err, 0, sizeof(ERF));
    hfdi = FDICreate(Malloc, Free, ::Open, ::Read, ::Write, ::Close, ::Seek, cpuUNKNOWN, &err);
    if (!hfdi)
        return FDIError(err.erfOper);

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), CurrentCAB);
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    ret = MakeFilesList(Files, next, nextParam, targetDir);
    if (ret && Files.Count)
    {
        Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
        Salamander->ProgressSetTotalSize(CQuadWord(-1, -1), ProgressTotal);
        while (!Abort)
        {
            FirstCABINET_INFO = TRUE;
            ret = FDICopy(hfdi, CurrentCAB, CurrentCABPath, 0, ::Notify, NULL, this);
            if (!ret)
            {
                FDIError(err.erfOper);
                break;
            }
            if (!*NextCAB || !Files.Count)
                break;
            FirstCAB = FALSE;
            char buffer[CB_MAX_CAB_PATH + CB_MAX_CABINET_NAME + 1];
            strcpy(CurrentCAB, NextCAB);
            BOOL firstTry;
            firstTry = TRUE;
            while (1)
            {
                GetCachedCABPath(CurrentCAB, CurrentCABPath);
                strcpy(buffer, CurrentCABPath);
                SalamanderGeneral->SalPathAppend(buffer, CurrentCAB, MAX_PATH + CB_MAX_CABINET_NAME + 1);
                DWORD attr = SalamanderGeneral->SalGetFileAttributes(buffer);
                if (attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    INT_PTR f = Open(buffer, _O_RDONLY | _O_EXCL, 0);
                    if (f == -1)
                        break;
                    FDICABINETINFO ci;
                    BOOL r = FDIIsCabinet(hfdi, f, &ci);
                    Close(f);
                    if (r)
                    {
                        if (ci.hasprev && ci.setID == SetID && ci.iCabinet == NextCABIndex)
                            break; //OK
                        err.erfOper = FDIERROR_WRONG_CABINET;
                        err.fError = TRUE;
                    }
                    if (!firstTry)
                        FDIError(err.erfOper);
                }
                if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), CurrentCAB, CurrentCABPath,
                                     NextDISK, NextCABIndex + 1) != IDOK)
                {
                    Abort = TRUE;
                    break;
                }
                firstTry = FALSE;
            }
        }
    }

    Files.Destroy();
    CABCache.Destroy();
    Salamander->CloseProgressDialog();
    FDIDestroy(hfdi);
    Salamander = NULL;

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

    Salamander = salamander;
    Action = CA_UNPACK_ONE_FILE;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    IOError = FALSE;
    FirstCAB = TRUE;
    HFDI hfdi;
    ERF err;
    Count = 0;
    char rootDir[MAX_PATH + 2];
    strcpy(rootDir, nameInArchive);
    char* c = strrchr(rootDir, '\\');
    if (!c)
        c = rootDir;
    *c = 0;
    ArcRoot = rootDir;
    if (*ArcRoot == '\\')
        ArcRoot++;
    RootLen = lstrlen(ArcRoot);
    TargetDir = targetDir;
    NameInArchive = nameInArchive;
    OneFileSuccess = FALSE;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;

    strcpy(CurrentCABPath, fileName);
    char* arcName;
    if (!SalamanderGeneral->CutDirectory(CurrentCABPath, &arcName))
        return FALSE;
    strcpy(CurrentCAB, arcName);
    SalamanderGeneral->SalPathAddBackslash(CurrentCABPath, MAX_PATH + 2);

    memset(&err, 0, sizeof(ERF));
    hfdi = FDICreate(Malloc, Free, ::Open, ::Read, ::Write, ::Close, ::Seek, cpuUNKNOWN, &err);
    if (!hfdi)
        return FDIError(err.erfOper);

    while (!Abort)
    {
        FirstCABINET_INFO = TRUE;
        ret = FDICopy(hfdi, CurrentCAB, CurrentCABPath, 0, ::Notify, NULL, this);
        if (!ret)
        {
            if (OneFileSuccess)
                ret = TRUE;
            else
                FDIError(err.erfOper);
            break;
        }
        if (OneFileSuccess)
            break;
        if (!*NextCAB)
        {
            ret = Error(IDS_NOTFOUND);
            break;
        }
        FirstCAB = FALSE;
        char buffer[CB_MAX_CAB_PATH + CB_MAX_CABINET_NAME + 1];
        strcpy(CurrentCAB, NextCAB);
        BOOL firstTry;
        firstTry = TRUE;
        while (1)
        {
            GetCachedCABPath(CurrentCAB, CurrentCABPath);
            strcpy(buffer, CurrentCABPath);
            SalamanderGeneral->SalPathAppend(buffer, CurrentCAB, MAX_PATH + CB_MAX_CABINET_NAME + 1);
            DWORD attr = SalamanderGeneral->SalGetFileAttributes(buffer);
            if (attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                INT_PTR f = Open(buffer, _O_RDONLY | _O_EXCL, 0);
                if (f == -1)
                    break;
                FDICABINETINFO ci;
                BOOL r = FDIIsCabinet(hfdi, f, &ci);
                Close(f);
                if (r)
                {
                    if (ci.hasprev && ci.setID == SetID && ci.iCabinet == NextCABIndex)
                        break; //OK
                    err.erfOper = FDIERROR_WRONG_CABINET;
                    err.fError = TRUE;
                }
                if (!firstTry)
                    FDIError(err.erfOper);
            }
            if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), CurrentCAB, CurrentCABPath,
                                 NextDISK, NextCABIndex + 1) != IDOK)
            {
                Abort = TRUE;
                break;
            }
            firstTry = FALSE;
        }
    }

    CABCache.Destroy();
    FDIDestroy(hfdi);
    Salamander = NULL;

    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    Salamander = salamander;
    Action = CA_UNPACK_WHOLE_ARCHIVE;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    IOError = FALSE;
    FirstCAB = TRUE;
    HFDI hfdi;
    ERF err;
    Count = 0;
    ArcRoot = "";
    RootLen = 0;
    TargetDir = targetDir;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;

    strcpy(CurrentCABPath, fileName);
    char* arcName;
    if (!SalamanderGeneral->CutDirectory(CurrentCABPath, &arcName))
        return FALSE;
    strcpy(CurrentCAB, arcName);
    SalamanderGeneral->SalPathAddBackslash(CurrentCABPath, MAX_PATH + 2);

    memset(&err, 0, sizeof(ERF));
    hfdi = FDICreate(Malloc, Free, ::Open, ::Read, ::Write, ::Close, ::Seek, cpuUNKNOWN, &err);
    if (!hfdi)
        return FDIError(err.erfOper);

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), CurrentCAB);
    Salamander->OpenProgressDialog(title, FALSE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    ret = ConstructMaskArray(Masks, mask);
    if (ret && Masks.Count)
    {
        Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
        //Salamander->ProgressSetTotalSize(CQuadWord(-1, -1), ProgressTotal); // FIXME: ProgressTotal total neni napocitany; potom pujde povolit druhy progress
        while (!Abort)
        {
            FirstCABINET_INFO = TRUE;
            if (delArchiveWhenDone)
            {
                // Petr: jmena svazku multi-volume archivu posbirame tady, tady se projde cely archiv svazek po svazku;
                // uvnitr FDICopy v Notify (viz fdintCABINET_INFO) se prochazi dalsi svazky v pripade, ze soubor
                // z aktualniho svazku presahuje do dalsich svazku ... nicmene ty dalsi se projdou i tady pak znovu,
                // takze sbirani v Notify mi neprijde zrovna stastne (nutne zahazovani opakujicich se jmen svazku)
                int len = (int)strlen(CurrentCABPath);
                archiveVolumes->Add(CurrentCABPath, len);
                if (len > 0 && CurrentCABPath[len - 1] != '\\' && CurrentCABPath[len - 1] != '/')
                    archiveVolumes->Add("\\", 1);
                archiveVolumes->Add(CurrentCAB, -2);
            }
            ret = FDICopy(hfdi, CurrentCAB, CurrentCABPath, 0, ::Notify, NULL, this);
            if (!ret)
            {
                FDIError(err.erfOper);
                break;
            }
            if (!*NextCAB)
                break;
            FirstCAB = FALSE;
            char buffer[CB_MAX_CAB_PATH + CB_MAX_CABINET_NAME + 1];
            strcpy(CurrentCAB, NextCAB);
            BOOL firstTry;
            firstTry = TRUE;
            while (1)
            {
                GetCachedCABPath(CurrentCAB, CurrentCABPath);
                strcpy(buffer, CurrentCABPath);
                SalamanderGeneral->SalPathAppend(buffer, CurrentCAB, MAX_PATH + CB_MAX_CABINET_NAME + 1);
                DWORD attr = SalamanderGeneral->SalGetFileAttributes(buffer);
                if (attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                {
                    INT_PTR f = Open(buffer, _O_RDONLY | _O_EXCL, 0);
                    if (f == -1)
                        break;
                    FDICABINETINFO ci;
                    BOOL r = FDIIsCabinet(hfdi, f, &ci);
                    Close(f);
                    if (r)
                    {
                        if (ci.hasprev && ci.setID == SetID && ci.iCabinet == NextCABIndex)
                            break; //OK
                        err.erfOper = FDIERROR_WRONG_CABINET;
                        err.fError = TRUE;
                    }
                    if (!firstTry)
                        FDIError(err.erfOper);
                }
                if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), CurrentCAB, CurrentCABPath,
                                     NextDISK, NextCABIndex + 1) != IDOK)
                {
                    Abort = TRUE;
                    break;
                }
                firstTry = FALSE;
            }
        }
    }

    Masks.Destroy();
    CABCache.Destroy();
    Salamander->CloseProgressDialog();
    FDIDestroy(hfdi);
    Salamander = NULL;

    return ret;
}

BOOL CPluginInterfaceForArchiver::Error(int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::Error(%d, )", error);
    char buf[1024]; //temp variable
    *buf = 0;
    va_list arglist;
    va_start(arglist, error);
    vsprintf(buf, LoadStr(error), arglist);
    va_end(arglist);
    if (lastErr != ERROR_SUCCESS)
    {
        int l = lstrlen(buf);
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + l, 1024 - l, NULL);
    }
    SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);

    return FALSE;
}

BOOL CPluginInterfaceForArchiver::FDIError(int erfOper)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::FDIError(%d)", erfOper);
    if (!erfOper)
        return TRUE;
    int ret;
    switch (erfOper)
    {
    case FDIERROR_NONE:
        ret = 0;
        break;
    case FDIERROR_CABINET_NOT_FOUND:
        ret = IDS_CABINET_NOT_FOUND;
        break;
    case FDIERROR_NOT_A_CABINET:
        ret = IDS_NOT_A_CABINET;
        break;
    case FDIERROR_UNKNOWN_CABINET_VERSION:
        ret = IDS_UNKNOWN_CABINET_VERSION;
        break;
    case FDIERROR_CORRUPT_CABINET:
        if (!IOError)
            ret = IDS_CORRUPT_CABINET;
        else
            ret = 0;
        break;
    case FDIERROR_ALLOC_FAIL:
        ret = IDS_LOWMEM;
        break;
    case FDIERROR_BAD_COMPR_TYPE:
        ret = IDS_BAD_COMPR_TYPE;
        break;
    case FDIERROR_MDI_FAIL:
        ret = IDS_MDI_FAIL;
        break;
    case FDIERROR_TARGET_FILE:
        ret = 0;
        break;
    case FDIERROR_RESERVE_MISMATCH:
        ret = IDS_RESERVE_MISMATCH;
        break;
    case FDIERROR_WRONG_CABINET:
        ret = IDS_WRONG_CABINET;
        break;
    case FDIERROR_USER_ABORT:
        ret = 0;
        break;
    default:
        ret = IDS_UNKNOWN;
    }
    if (ret)
        return Error(ret);
    else
        return TRUE;
}

BOOL CPluginInterfaceForArchiver::Init()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Init()");

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::MakeFilesList(TIndirectArray2<char>& files, SalEnumSelection next, void* nextParam, const char* targetDir)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::MakeFilesList(, , , %s)", targetDir);
    const char* nextName;
    BOOL isDir;
    CQuadWord size;
    char dir[MAX_PATH];
    char* addDir;
    int dirLen;
    int errorOccured;

    lstrcpy(dir, targetDir);
    addDir = dir + lstrlen(dir);
    if (*(addDir - 1) != '\\')
    {
        *addDir++ = '\\';
        *addDir = 0;
    }
    dirLen = lstrlen(dir);

    ProgressTotal = CQuadWord(0, 0);
    while ((nextName = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, NULL, nextParam, &errorOccured)) != NULL)
    {
        if (!isDir)
        {
            char* str = new char[RootLen + lstrlen(nextName) + 2];
            if (!str)
                return Error(IDS_LOWMEM);
            lstrcpy(str, ArcRoot);
            char* ptr = str + RootLen;
            if (RootLen && *(ptr - 1) != '\\')
                *ptr++ = '\\';
            lstrcpy(ptr, nextName);
            if (!files.Add(str))
            {
                delete str;
                return Error(IDS_LOWMEM);
            }
            ProgressTotal += size;
        }
    }
    return errorOccured != SALENUM_CANCEL && // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci (tlacitko Cancel)
           SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(), targetDir, ProgressTotal, LoadStr(IDS_PLUGINNAME));
}

BOOL CPluginInterfaceForArchiver::ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ConstructMaskArray(, %s)", masks);
    const char* sour;
    char* dest;
    char* newMask;
    int newMaskLen;
    char buffer[MAX_PATH];

    sour = masks;
    while (*sour)
    {
        dest = buffer;
        while (*sour)
        {
            if (*sour == ';')
            {
                if (*(sour + 1) == ';')
                    sour++;
                else
                    break;
            }
            if (dest == buffer + MAX_PATH - 1)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGMASK), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                return FALSE;
            }
            *dest++ = *sour++;
        }
        while (--dest >= buffer && *dest <= ' ')
            ;
        *(dest + 1) = 0;
        dest = buffer;
        while (*dest != 0 && *dest <= ' ')
            dest++;
        newMaskLen = (int)strlen(dest);
        if (newMaskLen)
        {
            newMask = new char[newMaskLen + 1];
            if (!newMask)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                return FALSE;
            }
            SalamanderGeneral->PrepareMask(newMask, dest);
            if (!maskArray.Add(newMask))
            {
                delete newMask;
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                return FALSE;
            }
        }
        if (*sour)
            sour++;
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UpdateCABCache(char* name, char* path)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::UpdateCABCache(, )");
    int i;
    for (i = 0; i < CABCache.Count; i++)
    {
        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                          name, -1, CABCache[i]->CABName, -1) == CSTR_EQUAL)
        {
            strcpy(CABCache[i]->CABPath, path);
            return TRUE;
        }
    }
    CCABCacheEntry* entry = new CCABCacheEntry(name, path);
    if (!entry)
        return Error(IDS_LOWMEM);
    if (!CABCache.Add(entry))
    {
        delete entry;
        return Error(IDS_LOWMEM);
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::GetCachedCABPath(char* name, char* path)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::GetCachedCABPath(, )");
    int i;
    for (i = 0; i < CABCache.Count; i++)
    {
        if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                          name, -1, CABCache[i]->CABName, -1) == CSTR_EQUAL)
        {
            strcpy(path, CABCache[i]->CABPath);
            return TRUE;
        }
    }
    return FALSE;
}

BOOL CPluginInterfaceForArchiver::ListFile(char* fileName, DWORD size, WORD date, WORD time, DWORD attributes)
{
    CALL_STACK_MESSAGE6("CPluginInterfaceForArchiver::ListFile( %s, 0x%X, 0x%X, "
                        "0x%X, 0x%X)",
                        fileName, size, date, time, attributes);
    CFileData fileData;
    char* slash;
    const char* path;
    char* name;
    BOOL ret = TRUE;

    path = fileName;
    name = fileName;
    slash = strrchr(fileName, '\\');
    if (slash)
    {
        *slash = 0;
        name = slash + 1;
    }
    else
        path = "";
    fileData.Name = SalamanderGeneral->DupStr(name);
    if (!fileData.Name)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        if (slash)
            *slash = '\\';
        return FALSE;
    }
    fileData.Ext = strrchr(fileData.Name, '.');
    if (fileData.Ext != NULL)
        fileData.Ext++; // ".cvspass" ve Windows je pripona
    else
        fileData.Ext = fileData.Name + lstrlen(fileData.Name);
    fileData.Size = CQuadWord(size, 0);
    fileData.Attr = attributes & FILE_ATTRIBUTE_MASK;
    fileData.Hidden = fileData.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
    fileData.PluginData = -1; // zbytecne, jen tak pro formu
    FILETIME ft;
    if (!DosDateTimeToFileTime(date, time, &ft))
    {
        SystemTimeToFileTime(&MinTime, &ft);
    }
    LocalFileTimeToFileTime(&ft, &fileData.LastWrite);
    fileData.DosName = NULL;
    fileData.NameLen = lstrlen(fileData.Name);
    fileData.IsLink = SalamanderGeneral->IsFileLink(fileData.Ext);
    fileData.IsOffline = 0;
    if (!Dir->AddFile(path, fileData, NULL))
    {
        free(fileData.Name);
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        ret = FALSE;
    }
    else
        Count++;
    if (slash)
        *slash = '\\';
    return ret;
}

BOOL CPluginInterfaceForArchiver::DoThisFile(char* fileName)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::DoThisFile(%s)", fileName);
    BOOL ret = FALSE;
    switch (Action)
    {
    case CA_UNPACK:
    {
        int i;
        for (i = 0; i < Files.Count; i++)
        {
            if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                              fileName, -1, Files[i], -1) == CSTR_EQUAL)
            {
                ret = TRUE;
                Files.Delete(i);
                break;
            }
        }
        break;
    }
    case CA_UNPACK_ONE_FILE:
        return CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                             fileName, -1, NameInArchive, -1) == CSTR_EQUAL;
    case CA_UNPACK_WHOLE_ARCHIVE:
    {
        const char* name = SalamanderGeneral->SalPathFindFileName(fileName);
        BOOL nameHasExt = strchr(name, '.') != NULL; // ".cvspass" ve Windows je pripona
        int i;
        for (i = 0; i < Masks.Count; i++)
        {
            if (SalamanderGeneral->AgreeMask(name, Masks[i], nameHasExt))
            {
                ret = TRUE;
                break;
            }
        }
        break;
    }
    }
    return ret;
}

INT_PTR
CPluginInterfaceForArchiver::UnpackFile(char* fileName, DWORD size, WORD date, WORD time, DWORD attributes)
{
    CALL_STACK_MESSAGE6("CPluginInterfaceForArchiver::UnpackFile( %s, 0x%X, 0x%X, "
                        "0x%X, 0x%X)",
                        fileName, size, date, time, attributes);
    if (!DoThisFile(fileName))
        return 0;

    char message[MAX_PATH + 32];
    lstrcpy(message, LoadStr(IDS_EXTRACTING));
    lstrcat(message, fileName);
    if (Action != CA_UNPACK_ONE_FILE)
        Salamander->ProgressDialogAddText(message, TRUE);

    if (Action != CA_UNPACK_ONE_FILE)
        Salamander->ProgressSetTotalSize(CQuadWord(size, 0), CQuadWord(-1, -1));
    if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
    {
        Abort = TRUE;
        return -1;
    }

    CFile* ret = new CFile;
    if (!ret)
    {
        Error(IDS_LOWMEM);
        Abort = TRUE;
        return -1;
    }
    strncpy_s(ret->FileName, TargetDir, _TRUNCATE);
    if (!SalamanderGeneral->SalPathAppend(ret->FileName, fileName + RootLen, MAX_PATH))
    {
        delete ret;
        ret = NULL;
        if (Silent & SF_LONGNAMES)
            return 0;
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, fileName, LoadStr(IDS_TOOLONGNAME), NULL))
        {
        case DIALOG_SKIPALL:
            Silent |= SF_LONGNAMES;
        case DIALOG_SKIP:
        {
            if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(size, TRUE))
            {
                Abort = TRUE;
                return -1;
            }
            return 0;
        }
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
    }
    char nameInArc[MAX_PATH + MAX_PATH];
    strcpy(nameInArc, CurrentCABPath);
    SalamanderGeneral->SalPathAppend(nameInArc, CurrentCAB, MAX_PATH + MAX_PATH);
    SalamanderGeneral->SalPathAppend(nameInArc, fileName, MAX_PATH + MAX_PATH);
    char buf[100];
    FILETIME ft, lft;
    if (!DosDateTimeToFileTime(date, time, &lft))
    {
        SystemTimeToFileTime(&MinTime, &lft);
    }
    LocalFileTimeToFileTime(&lft, &ft);
    GetInfo(buf, &ft, size);
    BOOL skip;
    CQuadWord q = CQuadWord(size, 0);
    BOOL allocate = AllocateWholeFile &&
                    CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
    if (TestAllocateWholeFile)
        q += CQuadWord(0, 0x80000000);
    ret->Handle = SalamanderSafeFile->SafeFileCreate(ret->FileName, GENERIC_WRITE, FILE_SHARE_READ,
                                                     attributes & FILE_ATTRIBUTE_MASK & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                     FALSE, SalamanderGeneral->GetMsgBoxParent(), nameInArc, buf, &Silent, TRUE, &skip, NULL, 0,
                                                     allocate ? &q : NULL, NULL);
    if (skip)
    {
        delete ret;
        if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(size, TRUE))
        {
            Abort = TRUE;
            return -1;
        }
        return 0;
    }
    if (ret->Handle == INVALID_HANDLE_VALUE)
    {
        delete ret;
        Abort = TRUE;
        return -1;
    }
    if (q == CQuadWord(0, 0x80000000))
    {
        // allokace se nepovedla a uz se o to snazit nebudem
        AllocateWholeFile = false;
        TestAllocateWholeFile = false;
    }
    else if (q == CQuadWord(0, 0x00000000))
    {
        // allokace se nepovedla, ale priste to zkusime znova
    }
    else
    {
        // allokace se povedla
        TestAllocateWholeFile = false;
    }
    ret->Flags = FF_EXTRFILE;

    return (INT_PTR)ret;
}

INT_PTR
CPluginInterfaceForArchiver::Open(char* pszFile, int oflag, int pmode)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::Open(%s, 0x%X)", pszFile, oflag);
    IOError = TRUE;

    DWORD fileaccess;
    switch (oflag & (_O_RDONLY | _O_WRONLY | _O_RDWR))
    {
    case _O_RDONLY: /* read access */
        fileaccess = GENERIC_READ;
        break;
    case _O_WRONLY: /* write access */
        fileaccess = GENERIC_WRITE;
        break;
    case _O_RDWR: /* read and write access */
        fileaccess = GENERIC_READ | GENERIC_WRITE;
        break;
    default: /* error, bad oflag */
        return -1;
    }

    DWORD filecreate;
    switch (oflag & (_O_CREAT | _O_EXCL | _O_TRUNC))
    {
    case 0:
    case _O_EXCL: // ignore EXCL w/o CREAT
        filecreate = OPEN_EXISTING;
        break;
    case _O_CREAT:
        filecreate = OPEN_ALWAYS;
        break;
    case _O_CREAT | _O_EXCL:
    case _O_CREAT | _O_TRUNC | _O_EXCL:
        filecreate = CREATE_NEW;
        break;
    case _O_TRUNC:
    case _O_TRUNC | _O_EXCL: // ignore EXCL w/o CREAT
        filecreate = TRUNCATE_EXISTING;
        break;
    case _O_CREAT | _O_TRUNC:
        filecreate = CREATE_ALWAYS;
        break;
    default:
        // this can't happen ... all cases are covered
        return -1;
    }

    DWORD fileattrib = FILE_ATTRIBUTE_NORMAL; /* default */
    if (oflag & _O_TEMPORARY)
    {
        fileattrib |= FILE_FLAG_DELETE_ON_CLOSE;
        fileaccess |= DELETE;
    }
    if (oflag & _O_SHORT_LIVED)
        fileattrib |= FILE_ATTRIBUTE_TEMPORARY;
    if (oflag & _O_SEQUENTIAL)
        fileattrib |= FILE_FLAG_SEQUENTIAL_SCAN;
    else if (oflag & _O_RANDOM)
        fileattrib |= FILE_FLAG_RANDOM_ACCESS;

    CFile* ret = new CFile;
    if (!ret)
    {
        Error(IDS_LOWMEM);
        return -1;
    }

    while (1)
    {
        ret->Handle = CreateFile((LPTSTR)pszFile, fileaccess, FILE_SHARE_READ, NULL, filecreate, fileattrib, NULL);
        if (ret->Handle != INVALID_HANDLE_VALUE)
        {
            lstrcpyn(ret->FileName, pszFile, MAX_PATH);
            ret->Flags = 0;
            IOError = FALSE;
            ret->cabOffset = 0;
            if (oflag == _O_BINARY)
            {
                // opening in binary mode for reading only
                DWORD magic;

                Read((INT_PTR)ret, &magic, sizeof(magic));
                if (magic != 'FCSM')
                {
                    // does not start with CAB magic -> check for SFX EXE part
                    char* ptr = (char*)malloc(SFX_BUFF_SIZE);
                    int size;
                    if (ptr)
                    {
                        char* ptr2 = ptr;

                        size = Read((INT_PTR)ret, ptr, SFX_BUFF_SIZE) - (2 * sizeof(magic) - 1);
                        while (size > 0)
                        {
                            if ((*(DWORD*)ptr2 == 'FCSM') && (((DWORD*)ptr2)[1] == 0 /*reserved*/))
                            {
                                // there must be more than just testing FCSM to avoid wrong match with progra, code
                                ret->cabOffset = (DWORD)(ptr2 - ptr + sizeof(magic));
                                break;
                            }
                            ptr2++;
                            size--;
                        }
                        free(ptr);
                    }
                }
                SafeSeek(ret, 0, FILE_BEGIN);
            }

            return (INT_PTR)ret; //sucess
        }
        char buf[1024];
        lstrcpy(buf, LoadStr(IDS_UNABLECREATE));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
        if (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, pszFile, buf, NULL) != DIALOG_RETRY)
        {
            Abort = TRUE;
            delete ret;
            return -1;
        }
    }
}

UINT CPluginInterfaceForArchiver::Read(INT_PTR hf, void* pv, UINT cb)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::Read(, 0x%X)", cb);
    CFile* file = (CFile*)hf;
    if (cb == 0)
        return 0;
    if (file->Flags & FF_SKIPFILE)
        return -1;
    IOError = TRUE;
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(file->Handle, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        lstrcpy(buf, LoadStr(IDS_UNABLEGETFIELPOS));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS && file->Flags & FF_EXTRFILE)
            return -1;

        int ret;
        if (file->Flags & FF_EXTRFILE)
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, file->FileName, buf, NULL);
        else
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, file->FileName, buf, NULL);
        switch (ret)
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return -1;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
    }
    while (1)
    {
        DWORD read;
        if (ReadFile(file->Handle, pv, cb, &read, NULL))
        {
            IOError = FALSE;
            return read; //success
        }
        lstrcpy(buf, LoadStr(IDS_UNABLEREAD));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS && file->Flags & FF_EXTRFILE)
            return -1;

        int ret;
        if (file->Flags & FF_EXTRFILE)
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, file->FileName, buf, NULL);
        else
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, file->FileName, buf, NULL);
        switch (ret)
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return -1;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
        if (SafeSeek(file, pos - file->cabOffset, FILE_BEGIN) == -1)
            return -1;
    }
    return -1;
} /* CPluginInterfaceForArchiver::Read */

UINT CPluginInterfaceForArchiver::Write(INT_PTR hf, void* pv, UINT cb)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::Write(, 0x%X)", cb);
    CFile* file = (CFile*)hf;
    if (cb == 0)
        return 0;
    if (file->Flags & FF_SKIPFILE)
    {
        if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
        {
            Abort = TRUE;
            return -1;
        }
        return cb;
    }
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(file->Handle, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        lstrcpy(buf, LoadStr(IDS_UNABLEGETFIELPOS));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS && file->Flags & FF_EXTRFILE)
        {
            if (Action != CA_UNPACK_ONE_FILE)
                Salamander->ProgressDialogAddText(LoadStr(IDS_SKIPPING), TRUE);
            file->Flags |= FF_SKIPFILE;
            if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
            {
                Abort = TRUE;
                return -1;
            }
            return cb;
        }

        int ret;
        if (file->Flags & FF_EXTRFILE)
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, file->FileName, buf, NULL);
        else
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, file->FileName, buf, NULL);
        switch (ret)
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
        {
            if (file->Flags & FF_EXTRFILE)
            {
                if (Action != CA_UNPACK_ONE_FILE)
                    Salamander->ProgressDialogAddText(LoadStr(IDS_SKIPPING), TRUE);
                file->Flags |= FF_SKIPFILE;
                if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
                {
                    Abort = TRUE;
                    return -1;
                }
                return cb;
            }
            else
                return -1;
        }
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
    }
    while (1)
    {
        DWORD written;
        if (WriteFile(file->Handle, pv, cb, &written, NULL))
        {
            if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
            {
                Abort = TRUE;
                return -1;
            }
            return written; // sucess
        }

        lstrcpy(buf, LoadStr(IDS_UNABLEWRITE));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS && file->Flags & FF_EXTRFILE)
        {
            if (Action != CA_UNPACK_ONE_FILE)
                Salamander->ProgressDialogAddText(LoadStr(IDS_SKIPPING), TRUE);
            file->Flags |= FF_SKIPFILE;
            if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
            {
                Abort = TRUE;
                return -1;
            }
            return cb;
        }

        int ret;
        if (file->Flags & FF_EXTRFILE)
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, file->FileName, buf, NULL);
        else
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, file->FileName, buf, NULL);
        switch (ret)
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
        {
            if (file->Flags & FF_EXTRFILE)
            {
                if (Action != CA_UNPACK_ONE_FILE)
                    Salamander->ProgressDialogAddText(LoadStr(IDS_SKIPPING), TRUE);
                file->Flags |= FF_SKIPFILE;
                if (Action != CA_UNPACK_ONE_FILE && !Salamander->ProgressAddSize(cb, TRUE))
                {
                    Abort = TRUE;
                    return -1;
                }
                return cb;
            }
            else
                return -1;
        }
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
        if (SafeSeek(file, pos - file->cabOffset, FILE_BEGIN) == -1)
            return -1;
    }
    return -1;
} /* CPluginInterfaceForArchiver::Write */

int CPluginInterfaceForArchiver::Close(INT_PTR hf)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Close( )");
    CFile* file = (CFile*)hf;
    CloseHandle(file->Handle);
    //nebyl uspesne robaleny tak ho smazeme
    if (file->Flags & FF_EXTRFILE)
        DeleteFile(file->FileName);
    delete file;
    return 0;
}

long CPluginInterfaceForArchiver::SafeSeek(CFile* file, DWORD distance, DWORD method)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::SafeSeek(, %u, 0x%X)", distance, method);
    if (file->Flags & FF_SKIPFILE)
        return distance;
    char buf[1024];
    while (1)
    {
        DWORD pos;
        if (method == FILE_BEGIN)
            distance += file->cabOffset;
        pos = SetFilePointer(file->Handle, distance, NULL, method);
        if (pos != 0xFFFFFFFF)
            return pos - file->cabOffset; //success
        lstrcpy(buf, LoadStr(IDS_UNABLESEEK));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS && file->Flags & FF_EXTRFILE)
        {
            file->Flags |= FF_SKIPFILE;
            return distance;
        }

        int ret;
        if (file->Flags & FF_EXTRFILE)
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, file->FileName, buf, NULL);
        else
            ret = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, file->FileName, buf, NULL);
        switch (ret)
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
        {
            if (file->Flags & FF_EXTRFILE)
            {
                file->Flags |= FF_SKIPFILE;
                return distance;
            }
            else
                return -1;
        }
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
    }
}

long CPluginInterfaceForArchiver::Seek(INT_PTR hf, long dist, int seektype)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::Seek(0x%IX, %d, %d)", hf,
                        dist, seektype);
    CFile* file = (CFile*)hf;
    DWORD method;
    switch (seektype)
    {
    case SEEK_SET:
        method = FILE_BEGIN;
        break;
    case SEEK_CUR:
        method = FILE_CURRENT;
        break;
    case SEEK_END:
        method = FILE_END;
        break;
    default:
        return -1;
    }
    long ret = SafeSeek(file, dist, method);
    if (ret == -1)
        IOError = TRUE;
    return ret;
} /* CPluginInterfaceForArchiver::Seek */

INT_PTR
CPluginInterfaceForArchiver::Notify(FDINOTIFICATIONTYPE fdint, PFDINOTIFICATION pfdin)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Notify(, )");
    switch (fdint)
    {
    case fdintCABINET_INFO:
    {
        if (Action == CA_UNPACK || Action == CA_UNPACK_WHOLE_ARCHIVE)
        {
            strcpy(CurrentCABPath, pfdin->psz3);
            if (!UpdateCABCache(CurrentCAB, CurrentCABPath))
                return -1;
        }
        if (FirstCABINET_INFO)
        {
            strcpy(NextCAB, pfdin->psz1);
            strcpy(NextDISK, pfdin->psz2);
            NextCABIndex = pfdin->iCabinet + 1;
            SetID = pfdin->setID;
        }
        CurrentCABIndex = pfdin->iCabinet;
        if (FirstCAB && pfdin->iCabinet != 0)
            NotWholeArchListed = TRUE;
        FirstCABINET_INFO = FALSE;
        break;
    }

    case fdintPARTIAL_FILE:
    {
        if (Action == CA_LIST)
        {
            if (FirstCAB && !ListFile(pfdin->psz1, 0, 0, 0, 0))
                return -1;
        }
        else
        {
            if (DoThisFile(pfdin->psz1) && (Action != CA_UNPACK_WHOLE_ARCHIVE || FirstCAB))
            {
                INT_PTR ret;
                if (Silent & SF_CONTINUED)
                    ret = IDSKIP;
                else
                {
                    ret = ContinuedFileDialog(SalamanderGeneral->GetMsgBoxParent(), pfdin->psz1);
                    if (ret == IDALL)
                    {
                        Silent |= SF_CONTINUED;
                        ret = IDSKIP;
                    }
                }
                if (ret != IDSKIP)
                {
                    Abort = TRUE;
                    return -1;
                }
            }
        }
        break;
    }

    case fdintCOPY_FILE:
    {
        if (Action == CA_LIST)
        {
            if (!ListFile(pfdin->psz1, pfdin->cb, pfdin->date, pfdin->time, pfdin->attribs))
                return -1;
        }
        else
            return UnpackFile(pfdin->psz1, pfdin->cb, pfdin->date, pfdin->time, pfdin->attribs);
        break;
    }

    case fdintCLOSE_FILE_INFO:
    {
        CFile* file = (CFile*)pfdin->hf;
        FILETIME ft, lft;
        if (!DosDateTimeToFileTime(pfdin->date, pfdin->time, &lft))
        {
            SystemTimeToFileTime(&MinTime, &lft);
        }
        LocalFileTimeToFileTime(&lft, &ft);
        SetFileTime(file->Handle, NULL, NULL, &ft);
        CloseHandle(file->Handle);
        if (file->Flags & FF_SKIPFILE)
            DeleteFile(file->FileName);
        else
            SetFileAttributes(file->FileName, pfdin->attribs & FILE_ATTRIBUTE_MASK);
        delete file;
        if (Action == CA_UNPACK_ONE_FILE)
        {
            OneFileSuccess = TRUE;
            return FALSE;
        }
        return TRUE;
    }

    case fdintNEXT_CABINET:
    {
        static BOOL firstTry;
        if (pfdin->fdie == FDIERROR_NONE)
        {
            firstTry = TRUE;
            strcpy(CurrentCAB, pfdin->psz1);
            char buffer[CB_MAX_CAB_PATH + CB_MAX_CABINET_NAME + 1];
            GetCachedCABPath(CurrentCAB, pfdin->psz3);
            strcpy(buffer, pfdin->psz3);
            SalamanderGeneral->SalPathAppend(buffer, CurrentCAB, CB_MAX_CAB_PATH + CB_MAX_CABINET_NAME + 1);
            DWORD attr = SalamanderGeneral->SalGetFileAttributes(buffer);
            if (attr != -1 && !(attr & FILE_ATTRIBUTE_DIRECTORY))
                break;
        }
        else if (!firstTry)
            FDIError(pfdin->fdie);
        if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), CurrentCAB, pfdin->psz3,
                             pfdin->psz2, CurrentCABIndex + 2) != IDOK)
        {
            Abort = TRUE;
            return -1;
        }
        firstTry = FALSE;
        break;
    }

    case fdintENUMERATE:
        return 0;
    default:
        return 0;
    }
    return 0;
}

// ****************************************************************************

void GetInfo(char* buffer, FILETIME* lastWrite, unsigned size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%X)", size);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, CQuadWord(size, 0)), date, time);
}
