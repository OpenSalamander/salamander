// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "array2.h"

#include "unarjdll.h"
#include "unarjspl.h"
#include "dialogs.h"

#include "unarj.rh"
#include "unarj.rh2"
#include "lang\lang.rh"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy

/*
FPAKGetIFace PAKGetIFace;
FPAKReleaseIFace PAKReleaseIFace;
*/

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
                   "UnARJ" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnARJ" /* neprekladat! */);
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

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnARJ" /* neprekladat! */, "arj");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
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

    salamander->AddCustomUnpacker("UnARJ (Plugin)", "*.arj;*.a##", FALSE);
    salamander->AddPanelArchiver("arj;a##", FALSE, FALSE);
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

BOOL WINAPI
ARJChangeVolProc(char* volName, char* prevName, int mode)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.ChangeVolProc(volName, prevName, mode);
}

BOOL WINAPI
ARJProcessDataProc(const void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.ProcessDataProc(buffer, size);
}

BOOL WINAPI
ARJErrorProc(int error, BOOL flags)
{
    CALL_STACK_MESSAGE_NONE
    return InterfaceForArchiver.ErrorProc(error, flags);
}

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    Salamander = salamander;
    pluginData = NULL;
    List = TRUE;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    NotWholeArchListed = FALSE;
    int count = 0;

    SwitchToFirstVol(fileName);
    CARJOpenData od;
    od.ArcName = ArcFileName;
    od.ARJChangeVolProc = ARJChangeVolProc;
    od.ARJErrorProc = ARJErrorProc;
    od.ARJProcessDataProc = ARJProcessDataProc;
    od.AchiveVolumes = NULL;
    ret = ARJOpenArchive(&od);
    if (ret)
    {
        int sortByExtDirsAsFiles;
        SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                              sizeof(sortByExtDirsAsFiles), NULL);

        CARJHeaderData header;
        CFileData fileData;
        char* slash;
        const char* path;
        char* name;
        while ((ret = ARJReadHeader(&header)) != 0 && *header.FileName)
        {
            if (header.Flags & FF_EXTFILE)
                NotWholeArchListed = TRUE;
            path = header.FileName;
            name = header.FileName;
            slash = strrchr(header.FileName, '\\');
            if (slash)
            {
                *slash = 0;
                name = slash + 1;
            }
            else
                path = "";
            fileData.Name = (char*)malloc(lstrlen(name) + 1);
            if (!fileData.Name)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ret = FALSE;
                break;
            }
            lstrcpy(fileData.Name, name);
            fileData.Ext = strrchr(fileData.Name, '.');
            if (fileData.Ext != NULL)
                fileData.Ext++; // ".cvspass" ve Windows je pripona
            else
                fileData.Ext = fileData.Name + lstrlen(fileData.Name);
            fileData.Attr = header.Attr;
            if (header.FileType == FT_DIRECTORY)
                fileData.Attr |= FILE_ATTRIBUTE_DIRECTORY;
            if (header.Flags & FF_ENCRYPTED && !(fileData.Attr & FILE_ATTRIBUTE_DIRECTORY))
                fileData.Attr |= FILE_ATTRIBUTE_ENCRYPTED;
            fileData.Hidden = fileData.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            fileData.PluginData = -1; // zbytecne, jen tak pro formu
            fileData.LastWrite = header.Time;
            fileData.DosName = NULL;
            fileData.NameLen = lstrlen(fileData.Name);

            //if (slash) *slash = '\\';
            //if (!ProcessFile(OP_SKIP, "", header.FileName)) */
            if (!ARJProcessFile(PFO_SKIP, &header.Size))
            {
                if (Abort)
                    ret = FALSE;
                free(fileData.Name);
                break;
            }

            fileData.Size = CQuadWord(header.Size, 0);
            fileData.IsOffline = 0;
            if (fileData.Attr & FILE_ATTRIBUTE_DIRECTORY)
            {
                if (!sortByExtDirsAsFiles)
                    fileData.Ext = fileData.Name + fileData.NameLen; // adresare nemaji pripony
                fileData.IsLink = 0;
                if (!dir->AddDir(path, fileData, NULL))
                    ret = FALSE;
            }
            else
            {
                fileData.IsLink = SalamanderGeneral->IsFileLink(fileData.Ext);
                if (!dir->AddFile(path, fileData, NULL))
                    ret = FALSE;
            }

            if (!ret)
            {
                free(fileData.Name);
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                break;
            }

            count++;
        }
        ARJCloseArchive();
    }

    if (NotWholeArchListed && !(Options & OP_NO_VOL_ATTENTION))
        AttentionDialog(SalamanderGeneral->GetMainWindowHWND());

    Salamander = NULL;

    //nejake soubory jsme jiz vylistovali, tak to nezabalime a zobrazime je
    if (!ret && count)
        ret = TRUE;

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
    List = FALSE;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    UnPackWholeArchive = FALSE;
    ArcRoot = archiveRoot ? archiveRoot : "";
    if (*ArcRoot == '\\')
        ArcRoot++;
    RootLen = lstrlen(ArcRoot);
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;
    TIndirectArray2<char> files(256);

    // extract files
    SwitchToFirstVol(fileName);

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ArcFileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    CARJOpenData od;
    od.ArcName = ArcFileName;
    od.ARJChangeVolProc = ARJChangeVolProc;
    od.ARJErrorProc = ARJErrorProc;
    od.ARJProcessDataProc = ARJProcessDataProc;
    od.AchiveVolumes = NULL;
    ret = ARJOpenArchive(&od);
    if (ret)
    {
        ret = MakeFilesList(files, next, nextParam, targetDir);
        if (ret)
        {
            CARJHeaderData header;
            int op;
            BOOL match;
            CQuadWord currentProgress(0, 0);
            Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
            while (files.Count && (ret = ARJReadHeader(&header)) != 0 && *header.FileName)
            {
                op = PFO_SKIP;
                match = FALSE;
                TargetFile = INVALID_HANDLE_VALUE;
                int i;
                for (i = 0; i < files.Count; i++)
                {
                    if (!(header.Attr & FILE_ATTRIBUTE_DIRECTORY || header.FileType == FT_DIRECTORY) &&
                        CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                      header.FileName, -1, files[i], -1) == CSTR_EQUAL)
                    {
                        match = TRUE;
                        files.Delete(i);
                        Salamander->ProgressSetTotalSize(CQuadWord(header.Size, 0), ProgressTotal);
                        if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                        {
                            Abort = TRUE;
                            break;
                        }
                        if (DoThisFile(&header, fileName, targetDir))
                            op = PFO_EXTRACT;
                        break;
                    }
                }

                UpdateProgress = TRUE;
                ret = ARJProcessFile(op);
                if (op == PFO_EXTRACT)
                {
                    SetFileTime(TargetFile, NULL, NULL, &header.Time);
                    CloseHandle(TargetFile);
                    if (!ret)
                        DeleteFile(TargetName);
                    else
                        SetFileAttributes(TargetName, header.Attr);
                }
                if (Abort)
                {
                    ret = FALSE;
                    break;
                }
                if (match)
                {
                    currentProgress += CQuadWord(header.Size, 0);
                    if (!Salamander->ProgressSetSize(CQuadWord(header.Size, 0), currentProgress, TRUE))
                    {
                        ret = FALSE;
                        break;
                    }
                }
            }
        }
        ARJCloseArchive();
    }

    Salamander->CloseProgressDialog();

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
    Silent = 0;
    BOOL ret = FALSE;
    ArcRoot = "";
    RootLen = 0;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;
    char justName[MAX_PATH];
    lstrcpy(justName, nameInArchive);
    SalamanderGeneral->SalPathStripPath(justName);

    // extract files
    List = FALSE;
    SwitchToFirstVol(fileName);
    CARJOpenData od;
    od.ArcName = ArcFileName;
    od.ARJChangeVolProc = ARJChangeVolProc;
    od.ARJErrorProc = ARJErrorProc;
    od.ARJProcessDataProc = ARJProcessDataProc;
    od.AchiveVolumes = NULL;
    if (ARJOpenArchive(&od))
    {
        CARJHeaderData header;
        int op;
        BOOL match = FALSE;
        while (ARJReadHeader(&header) && *header.FileName)
        {
            TargetFile = INVALID_HANDLE_VALUE;
            op = PFO_SKIP;
            if (!(header.Attr & FILE_ATTRIBUTE_DIRECTORY || header.FileType == FT_DIRECTORY) &&
                CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                              nameInArchive, -1, header.FileName, -1) == CSTR_EQUAL)
            {
                match = TRUE;
                if (header.Flags & FF_EXTFILE)
                {
                    ContinuedFileDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName);
                    goto UOF_NEXT;
                }
                if (lstrlen(justName) + lstrlen(targetDir) + 2 > MAX_PATH)
                {
                    Error(IDS_TOOLONGNAME);
                    goto UOF_NEXT;
                }
                char buf[100];
                GetInfo(buf, &header.Time, header.Size);
                lstrcpy(TargetName, targetDir);
                SalamanderGeneral->SalPathAppend(TargetName, justName, MAX_PATH);
                BOOL skip;
                CQuadWord q = CQuadWord(header.Size, 0);
                bool allocate = CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
                q += CQuadWord(0, 0x80000000);
                TargetFile = SalamanderSafeFile->SafeFileCreate(TargetName,
                                                                GENERIC_WRITE, FILE_SHARE_READ,
                                                                header.Attr & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                                FALSE, SalamanderGeneral->GetMsgBoxParent(), nameInArchive, buf, &Silent,
                                                                TRUE, &skip, NULL, 0, allocate ? &q : NULL, NULL);
                if (TargetFile != INVALID_HANDLE_VALUE)
                    op = PFO_EXTRACT;
            }

        UOF_NEXT:
            UpdateProgress = TRUE;
            int r;
            r = ARJProcessFile(op);
            if (op == PFO_EXTRACT)
            {
                SetFileTime(TargetFile, NULL, NULL, &header.Time);
                CloseHandle(TargetFile);
                if (r)
                {
                    SetFileAttributes(TargetName, header.Attr);
                    ret = TRUE;
                }
                else
                    DeleteFile(TargetName);
            }
            if (match || !r)
                break;
        }
        ARJCloseArchive();
    }

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
    List = FALSE;
    Silent = 0;
    BOOL ret = TRUE;
    Abort = FALSE;
    UnPackWholeArchive = TRUE;
    ArcRoot = "";
    RootLen = 0;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;
    TIndirectArray2<char> masks(16);

    if (!ConstructMaskArray(masks, mask) || masks.Count == 0)
        return FALSE;

    // extract files
    SwitchToFirstVol(fileName);

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ArcFileName));
    Salamander->OpenProgressDialog(title, FALSE, NULL, TRUE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    CARJOpenData od;
    od.ArcName = ArcFileName;
    od.ARJChangeVolProc = ARJChangeVolProc;
    od.ARJErrorProc = ARJErrorProc;
    od.ARJProcessDataProc = ARJProcessDataProc;
    od.AchiveVolumes = delArchiveWhenDone ? archiveVolumes : NULL;
    ret = ARJOpenArchive(&od);
    if (ret)
    {
        if (delArchiveWhenDone)
            archiveVolumes->Add(ArcFileName, -2);
        CARJHeaderData header;
        int op;
        BOOL match;
        Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
        while ((ret = ARJReadHeader(&header)) != 0 && *header.FileName)
        {
            op = PFO_SKIP;
            match = FALSE;
            TargetFile = INVALID_HANDLE_VALUE;

            const char* name = SalamanderGeneral->SalPathFindFileName(header.FileName);
            BOOL nameHasExt = strchr(name, '.') != NULL; // ".cvspass" ve Windows je pripona
            int i;
            for (i = 0; i < masks.Count; i++)
            {
                if (SalamanderGeneral->AgreeMask(name, masks[i], nameHasExt))
                {
                    match = TRUE;
                    Salamander->ProgressSetTotalSize(CQuadWord(header.Size, 0), CQuadWord(-1, -1));
                    if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                    {
                        Abort = TRUE;
                        break;
                    }
                    if (DoThisFile(&header, fileName, targetDir) && !(header.Attr & FILE_ATTRIBUTE_DIRECTORY ||
                                                                      header.FileType == FT_DIRECTORY))
                        op = PFO_EXTRACT;
                    break;
                }
            }

            UpdateProgress = TRUE;
            ret = ARJProcessFile(op);
            if (op == PFO_EXTRACT)
            {
                SetFileTime(TargetFile, NULL, NULL, &header.Time);
                CloseHandle(TargetFile);
                if (!ret)
                    DeleteFile(TargetName);
                else
                    SetFileAttributes(TargetName, header.Attr);
            }
            if (Abort)
            {
                ret = FALSE;
                break;
            }
            if (match)
            {
                if (!Salamander->ProgressSetSize(CQuadWord(header.Size, 0), CQuadWord(-1, -1), TRUE))
                {
                    ret = FALSE;
                    break;
                }
            }
        }
        ARJCloseArchive();
    }

    Salamander->CloseProgressDialog();

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

int CPluginInterfaceForArchiver::ChangeVolProc(char* arcName, char* prevName, int mode)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ChangeVolProc(, %d)", mode);
    UpdateProgress = FALSE;
    if (mode == CVM_ASK)
    {
        if (List)
        {
            NotWholeArchListed = TRUE;
            return 0; // listujeme jen dokud to jde bez ptani se na dalsi volumy
        }
        if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), arcName, prevName) != IDOK)
            return 0;
    }
    return 1;
}

BOOL CPluginInterfaceForArchiver::SafeSeek(DWORD position)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::SafeSeek(0x%X)", position);
    char buf[1024];
    while (1)
    {
        if (SetFilePointer(TargetFile, position, NULL, FILE_BEGIN) != 0xFFFFFFFF)
            return TRUE; // success
        lstrcpy(buf, LoadStr(IDS_UNABLESEEK));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS)
            return FALSE;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return FALSE;
        }
    }
}

int CPluginInterfaceForArchiver::ProcessDataProc(const void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ProcessDataProc(, 0x%X)", size);
    if (TargetFile == INVALID_HANDLE_VALUE)
        return 1;
    if (size == 0)
        return 1;
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(TargetFile, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        lstrcpy(buf, LoadStr(IDS_UNABLEGETFIELPOS));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS)
            return 0;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return 0;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return 0;
        }
    }
    DWORD written;
    while (1)
    {
        if (WriteFile(TargetFile, buffer, size, &written, NULL))
        {
            DWORD a = UpdateProgress ? size : 0;
            if (!Salamander->ProgressAddSize(a, TRUE))
            {
                Abort = TRUE;
                return 0;
            }
            return 1; // sucess
        }
        lstrcpy(buf, LoadStr(IDS_UNABLEWRITE));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (Silent & SF_IOERRORS)
            return 0;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return 0;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return 0;
        }
        if (!SafeSeek(pos))
            return 0;
    }
    return 1;
}

BOOL CPluginInterfaceForArchiver::ErrorProc(int error, BOOL flags)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::ErrorProc(%d, %d)", error, flags);
    int err;
    DWORD silent;
    switch (error)
    {
    case AE_SUCCESS:
        return TRUE;

    case AE_OPEN:
        err = IDS_ERROPENARC;
        goto EP_ARC;
    case AE_ACCESS:
        err = IDS_ERRACCESSARC;
        goto EP_ARC;
    case AE_EOF:
        err = IDS_ARCEOF;
        goto EP_ARC;
    case AE_BADARC:
        err = IDS_BADARC;
        goto EP_ARC;
    case AE_BADDATA:
        err = IDS_BADARC;
        goto EP_ARC;
    case AE_BADVOL:
        err = IDS_BADVOL;

    EP_ARC:
        if (SalamanderGeneral->SalMessageBox(SalamanderGeneral->GetMsgBoxParent(), LoadStr(err), LoadStr(IDS_PLUGINNAME),
                                             MB_ICONEXCLAMATION | (flags & EF_RETRY ? MB_RETRYCANCEL : MB_OK)) == IDRETRY)
            return TRUE;
        Abort = TRUE;
        break;

    case AE_BADVERSION:
        err = IDS_VERSION;
        silent = SF_VERSION;
        goto EP_FILE;
    case AE_ENCRYPT:
        err = IDS_ENCRYPT;
        silent = SF_ENCRYPTED;
        goto EP_FILE;
    case AE_METHOD:
        err = IDS_METHOD;
        silent = SF_METHOD;
        goto EP_FILE;
    case AE_UNKNTYPE:
        err = IDS_TYPE;
        silent = SF_TYPE;
        goto EP_FILE;
    case AE_CRC:
        err = IDS_CRC;
        silent = SF_DATA;

    EP_FILE:
        if (!(Silent & silent))
        {
            switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, TargetName, LoadStr(err), NULL))
            {
            case DIALOG_SKIPALL:
                Silent |= silent;
            case DIALOG_SKIP:
                break;
            case DIALOG_CANCEL:
            case DIALOG_FAIL:
                Abort = TRUE;
                break;
            }
        }
        break;

    default:
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_UNKNOWN), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        Abort = TRUE;
    }
    return FALSE;
}

void CPluginInterfaceForArchiver::SwitchToFirstVol(const char* arcName)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::SwitchToFirstVol(%s)", arcName);
    lstrcpy(ArcFileName, arcName);
    char* ext = PathFindExtension(ArcFileName);
    if (lstrlen(ext) > 3 &&
        isdigit(ext[2]) && isdigit(ext[3]))
    {
        char oldExt[MAX_PATH];
        lstrcpy(oldExt, ext);
        lstrcpy(ext, ".arj");
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(ArcFileName);
        if (attr == -1 || attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            lstrcpy(ext, oldExt);
        }
    }
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
        if (isDir)
        {
            if (dirLen + lstrlen(nextName) + 1 >= MAX_PATH)
            {
                if (Silent & SF_LONGNAMES)
                    continue;
                switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, nextName, LoadStr(IDS_TOOLONGNAME), NULL))
                {
                case DIALOG_SKIPALL:
                    Silent |= SF_LONGNAMES;
                case DIALOG_SKIP:
                    continue;
                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return FALSE;
                }
            }
            lstrcpy(addDir, nextName);
            BOOL skip;
            if (SalamanderSafeFile->SafeFileCreate(dir, 0, 0, 0, TRUE, SalamanderGeneral->GetMsgBoxParent(), NULL, NULL,
                                                   &Silent, TRUE, &skip, NULL, 0, NULL, NULL) == INVALID_HANDLE_VALUE &&
                !skip)
            {
                return FALSE;
            }
        }
        else
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

BOOL CPluginInterfaceForArchiver::DoThisFile(CARJHeaderData* hdr, const char* arcName, const char* targetDir)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DoThisFile(, %s, %s)", arcName,
                        targetDir);
    char message[MAX_PATH + 32];

    lstrcpy(message, LoadStr(IDS_EXTRACTING));
    lstrcat(message, hdr->FileName);
    if (UnPackWholeArchive)
        Salamander->ProgressDialogAddText(message, TRUE);
    else
        Salamander->ProgressDialogAddText(message, TRUE);
    if (hdr->Flags & FF_EXTFILE)
    {
        TRACE_I("Skipping file continued from previous volume: " << hdr->FileName);
        if (!(Options & OP_SKIPCONTINUED) && ContinuedFileDialog(SalamanderGeneral->GetMsgBoxParent(), hdr->FileName) != IDOK)
            Abort = TRUE;
        return FALSE;
    }
    if (lstrlen(targetDir) + lstrlen(hdr->FileName) - RootLen >= MAX_PATH)
    {
        if (Silent & SF_LONGNAMES)
            return FALSE;
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, hdr->FileName, LoadStr(IDS_TOOLONGNAME), NULL))
        {
        case DIALOG_SKIPALL:
            Silent |= SF_LONGNAMES;
        case DIALOG_SKIP:
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return FALSE;
        }
    }
    lstrcpy(TargetName, targetDir);
    SalamanderGeneral->SalPathAppend(TargetName, hdr->FileName + RootLen, MAX_PATH);
    char nameInArc[MAX_PATH + ARJ_MAX_PATH];
    lstrcpy(nameInArc, arcName);
    SalamanderGeneral->SalPathAppend(nameInArc, hdr->FileName, MAX_PATH + ARJ_MAX_PATH);
    char buf[100];
    GetInfo(buf, &hdr->Time, hdr->Size);
    BOOL skip;
    CQuadWord q = CQuadWord(hdr->Size, 0);
    BOOL allocate = AllocateWholeFile &&
                    CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
    if (TestAllocateWholeFile)
        q += CQuadWord(0, 0x80000000);
    TargetFile = SalamanderSafeFile->SafeFileCreate(TargetName, GENERIC_WRITE, FILE_SHARE_READ,
                                                    hdr->Attr & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                    hdr->Attr & FILE_ATTRIBUTE_DIRECTORY || hdr->FileType == FT_DIRECTORY,
                                                    SalamanderGeneral->GetMsgBoxParent(), nameInArc, buf, &Silent, TRUE, &skip, NULL, 0,
                                                    allocate ? &q : NULL, NULL);
    if (skip)
        return FALSE;
    if (TargetFile == INVALID_HANDLE_VALUE)
    {
        Abort = TRUE;
        return FALSE;
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
    if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
    {
        CloseHandle(TargetFile);
        Abort = TRUE;
        return FALSE;
    }
    //if (hdr->Attr & FILE_ATTRIBUTE_DIRECTORY) hdr->Size = 1;
    //Salamander->ProgressSetTotalSize(hdr->Size, ProgressTotal);
    return TRUE;
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

//***********************************************************************************
//
// Rutiny ze SHLWAPI.DLL
//

LPTSTR PathFindExtension(LPTSTR pszPath)
{
    CALL_STACK_MESSAGE_NONE
    if (pszPath == NULL)
    {
        TRACE_E("pszPath == NULL");
        return NULL;
    }
    int len = lstrlen(pszPath);
    char* iterator = pszPath + len - 1;
    while (iterator >= pszPath)
    {
        if (*iterator == '.') // ".cvspass" ve Windows je pripona
        {
            return iterator;
        }
        if (*iterator == '\\')
            break;
        iterator--;
    }
    return pszPath + len;
}
