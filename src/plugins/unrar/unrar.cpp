// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

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

HINSTANCE UnRarDll = NULL;

// funkce vyvezene z unrar.dll
FRAROpenArchiveEx _RAROpenArchiveEx;
FRARCloseArchive _RARCloseArchive;
//FRARReadHeader RARReadHeader;
FRARReadHeaderEx _RARReadHeaderEx;
FRARProcessFile _RARProcessFile;
//FRARSetChangeVolProc _RARSetChangeVolProc;
//FRARSetProcessDataProc _RARSetProcessDataProc;
FRARSetPassword _RARSetPassword;
FRARSetCallback _RARSetCallback;

struct CConfiguration Config;

const SYSTEMTIME MinTime = {1980, 01, 2, 01, 00, 00, 00, 000};

const char* CONFIG_OPTIONS = "Options";

const char* CONFIG_LIST_INFO_PACKED_SIZE = "List Info Packed Size";
const char* CONFIG_COL_PACKEDSIZE_FIXEDWIDTH = "Column PackedSize FixedWidth";
const char* CONFIG_COL_PACKEDSIZE_WIDTH = "Column PackedSize Width";

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

LPCTSTR LoadStr(int resID)
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
                   "UnRAR" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnRAR" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    if (!InterfaceForArchiver.Init())
        return NULL;

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK |
                                       FUNCTION_CONFIGURATION | FUNCTION_LOADSAVECONFIGURATION,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnRAR" /* neprekladat! */, "rar");

    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
// stuby do UnRAR.DLL (kvuli callstacku)
//

HANDLE RAROpenArchiveEx(struct RAROpenArchiveDataEx* ArchiveData)
{
    CALL_STACK_MESSAGE1("RarOpenArchiveEx( )");
    return _RAROpenArchiveEx(ArchiveData);
}

int RARCloseArchive(HANDLE hArcData)
{
    CALL_STACK_MESSAGE1("RARCloseArchive( )");
    return _RARCloseArchive(hArcData);
}

int RARReadHeaderEx(HANDLE hArcData, struct RARHeaderDataEx* HeaderData)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("RARReadHeaderEx( , )");
    return _RARReadHeaderEx(hArcData, HeaderData);
}

int RARProcessFile(HANDLE hArcData, int Operation, const char* DestPath, char* DestName)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE4("RARProcessFile( , %d, %s, %s", Operation, DestPath, DestName);
    return _RARProcessFile(hArcData, Operation, DestPath, DestName);
}

void RARSetCallback(HANDLE hArcData, UNRARCALLBACK Callback, LPARAM UserData)
{
    CALL_STACK_MESSAGE2("RARSetCallback( , , 0x%IX)", UserData);
    _RARSetCallback(hArcData, Callback, UserData);
}

/*void RARSetChangeVolProc(HANDLE hArcData,int (PASCAL *ChangeVolProc)(char *ArcName,int Mode))
{
  CALL_STACK_MESSAGE1("RARSetChangeVolProc( , )");
  _RARSetChangeVolProc(hArcData, ChangeVolProc);
}

void RARSetProcessDataProc(HANDLE hArcData,int (PASCAL *ProcessDataProc)(unsigned char *Addr,int Size))
{
  CALL_STACK_MESSAGE1("RARSetProcessDataProc( , )");
  _RARSetProcessDataProc(hArcData, ProcessDataProc);
}*/

void RARSetPassword(HANDLE hArcData, char* Password)
{
    CALL_STACK_MESSAGE1("RARSetPassword( , ***)");
    _RARSetPassword(hArcData, Password);
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
    //  if (PakLibDLL) FreeLibrary(PakLibDLL);
    FreeLibrary(UnRarDll);
    return TRUE;
}

void CPluginInterface::LoadConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::LoadConfiguration(, ,)");
    memset(&Config, 0, sizeof(Config));
    Config.ListInfoPackedSize = TRUE;

    if (regKey != NULL) // load z registry
    {
        registry->GetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Config.Options, sizeof(DWORD));
        Config.Options &= OP_SAVED_IN_REGISTRY;
        registry->GetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
        registry->GetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
    }
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");

    Config.Options &= OP_SAVED_IN_REGISTRY;
    registry->SetValue(regKey, CONFIG_OPTIONS, REG_DWORD, &Config.Options, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_LIST_INFO_PACKED_SIZE, REG_DWORD, &Config.ListInfoPackedSize, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_FIXEDWIDTH, REG_DWORD, &Config.ColumnPackedSizeFixedWidth, sizeof(DWORD));
    registry->SetValue(regKey, CONFIG_COL_PACKEDSIZE_WIDTH, REG_DWORD, &Config.ColumnPackedSizeWidth, sizeof(DWORD));
}

void CPluginInterface::Configuration(HWND parent)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Configuration()");
    if (IDOK == ConfigDialog(parent))
    {
        if (SalamanderGeneral->GetPanelPluginData(PANEL_LEFT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_LEFT);
        if (SalamanderGeneral->GetPanelPluginData(PANEL_RIGHT) != NULL)
            SalamanderGeneral->PostRefreshPanelPath(PANEL_RIGHT);
    }
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomUnpacker("UnRAR (Plugin)", "*.rar;*.r##", FALSE);
    salamander->AddPanelArchiver("rar;r##", FALSE, FALSE);
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
    List = TRUE;
    BOOL ret = TRUE;
    Abort = FALSE;
    NotWholeArchListed = FALSE;
    int count = 0;

    BOOL saveFirstVolume = FALSE;
    if (!SwitchToFirstVol(fileName, &saveFirstVolume))
        return FALSE;

    pluginData = PluginData = new CPluginDataInterface(saveFirstVolume ? _tcsdup(ArcFileName) : NULL);

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    if (OpenArchive())
    {
        CFileHeader header;
        CFileData fileData;
        LPTSTR slash;
        LPCTSTR path;
        LPTSTR name;

        while ((ret = ReadHeader(&header)) != 0 && *header.FileName)
        {
            if (header.Flags & RHDF_SPLITBEFORE)
                NotWholeArchListed = TRUE;
            path = header.FileName;
            name = header.FileName;
            slash = _tcsrchr(header.FileName, '\\');
            if (slash)
            {
                *slash = 0;
                name = slash + 1;
            }
            else
                path = "";
            fileData.NameLen = lstrlen(name);
            fileData.Name = (LPTSTR)SalamanderGeneral->Alloc((fileData.NameLen + 1) * sizeof(TCHAR));
            if (!fileData.Name)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                ret = FALSE;
                break;
            }
            lstrcpy(fileData.Name, name);
            fileData.Ext = _tcsrchr(fileData.Name, '.');
            if (fileData.Ext)
                fileData.Ext++; // ".cvspass" ve Windows je pripona
            else
                fileData.Ext = fileData.Name + fileData.NameLen;
            fileData.Size = header.Size;
            fileData.Attr = header.Attr;
            if (header.Flags & RHDF_ENCRYPTED)
            {
                fileData.Attr |= FILE_ATTRIBUTE_ENCRYPTED;
                if (header.Flags & RHDF_SOLID)
                    PluginData->SolidEncrypted = 1;
            }
            fileData.Hidden = fileData.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
            fileData.PluginData = (DWORD_PTR) new CRARFileData(header.CompSize.Value, count);
            fileData.LastWrite = header.Time;
            fileData.DosName = NULL;
            fileData.IsOffline = 0;
            if (header.Flags & RHDF_DIRECTORY)
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
                delete (CRARFileData*)fileData.PluginData;
                SalamanderGeneral->Free(fileData.Name);
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                break;
            }
            if (slash)
                *slash = '\\';
            //if (!ProcessFile(OP_SKIP, "", header.FileName)) */
            if (!ProcessFile(RAR_SKIP, header.FileName))
            {
                if (Abort)
                    ret = FALSE;
                break;
            }
            count++;
        }

        RARCloseArchive(ArcHandle);
        ArcHandle = NULL;
    }
    else
        ret = FALSE;

    if (NotWholeArchListed && !(Config.Options & OP_NO_VOL_ATTENTION))
        AttentionDialog(SalamanderGeneral->GetMainWindowHWND());

    //nejake soubory jsme jiz vylistovali, tak to nezabalime a zobrazime je
    if (!ret && count)
        ret = TRUE;

    Salamander = NULL;
    if (!ret)
        delete pluginData;

    return ret;
}

void FreeString(void* strig)
{
    CALL_STACK_MESSAGE_NONE
    free(strig);
}

BOOL CPluginInterfaceForArchiver::SetSolidPassword()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::SetSolidPassword()");
    if (!(PluginData->Silent & SF_ALLENRYPT))
    {
        if (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), ArcFileName, PluginData->Password,
                           PD_NOSKIP | PD_NOSKIPALL | PD_NOALL) != IDCANCEL)
        {
            PluginData->Silent |= SF_ALLENRYPT;
        }
        else
            return FALSE;
    }
    RARSetPassword(ArcHandle, PluginData->Password);
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginDataPar, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    Salamander = salamander;
    List = FALSE;
    BOOL ret = TRUE;
    Abort = FALSE;
    //  FirstFile = TRUE;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;
    ArcRoot = archiveRoot ? archiveRoot : "";
    if (*ArcRoot == '\\')
        ArcRoot++;
    RootLen = lstrlen(ArcRoot);
    TIndirectArray2<CRARExtractInfo> files(256);
    PluginData = (CPluginDataInterface*)pluginDataPar;

    // extract files

    // overime si, zda nemame v plugin data ulozeny prvni volume
    const CFileData* fd;
    DWORD attr;
    LPCTSTR fv;
    if (next(NULL /* nechceme zadne dotazy */, 1, NULL, NULL, &fd, nextParam, NULL) &&
        (fv = PluginData->GetFirstVolume()) != NULL &&
        (attr = SalamanderGeneral->SalGetFileAttributes(fv)) != -1 &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        _tcscpy(ArcFileName, fv);
    }
    else
    {
        SwitchToFirstVol(fileName);
    }

    next(NULL, -1, NULL, NULL, NULL, nextParam, NULL);

    /*  if (PluginData->SolidEncrypted) // The archive is not opened yet
    if (!SetSolidPassword())
      return FALSE;*/

    TCHAR title[1024];
    _stprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ArcFileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    PluginData->Silent &= SF_ALLENRYPT;

    ret = OpenArchive();
    if (ret)
    {
        ret = MakeFilesList(files, next, nextParam, targetDir);
        if (ret)
        {
            CFileHeader header;
            int op, count = 0;
            BOOL match;
            CQuadWord currentProgress = CQuadWord(0, 0);
            Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
            while (files.Count && (ret = ReadHeader(&header)) != 0 && *header.FileName)
            {
                op = RAR_SKIP;
                match = FALSE;
                TargetFile = INVALID_HANDLE_VALUE;
                /*if ((header.Flags & RHDF_SOLID) && FirstFile && (header.Flags & RHDF_ENCRYPTED))
        {
          if (PluginData->Silent & SF_ENCRYPTED) goto UA_NEXT;
          if (!(PluginData->Silent & SF_ALLENRYPT))
          {
            switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName, PluginData->Password, PD_NOSKIP))
            {
              case IDALL: PluginData->Silent |= SF_ALLENRYPT;
              case IDOK:  break;
              case IDSKIPALL: PluginData->Silent |= SF_ENCRYPTED; goto UA_NEXT;
              default: Abort = TRUE; goto UA_NEXT;
            }
          }
          RARSetPassword(ArcHandle, PluginData->Password);
        }*/
                int i;
                for (i = 0; i < files.Count; i++)
                {
                    if (!(header.Flags & RHDF_DIRECTORY) &&
                        (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                       header.FileName, -1, files[i]->FileName, -1) == CSTR_EQUAL) &&
                        (files[i]->ItemNumber == count))
                    {
                        match = TRUE;
                        files.Delete(i);
                        Salamander->ProgressSetTotalSize(header.Size, ProgressTotal);
                        if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                        {
                            Abort = TRUE;
                            break;
                        }
                        if (DoThisFile(&header, fileName, targetDir))
                            op = RAR_TEST;
                        break;
                    }
                }

                //UA_NEXT:
                ret = ProcessFile(op, header.FileName);
                if (op == RAR_TEST)
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
                    currentProgress += header.Size;
                    if (!Salamander->ProgressSetSize(header.Size, currentProgress, TRUE))
                    {
                        ret = FALSE;
                        break;
                    }
                }
                if (!(header.Flags & RHDF_SPLITBEFORE))
                {
                    // Don't count the same file twice
                    count++;
                }
                //        if (!(header.Flags & RHDF_DIRECTORY)) FirstFile = FALSE;
            }
        }
        Config.Options &= ~OP_SKITHISFILE;
        RARCloseArchive(ArcHandle);
        ArcHandle = NULL;
    }

    Salamander->CloseProgressDialog();

    Salamander = NULL;

    return ret;
}

static void DestroyIllegalChars(LPTSTR pszPath)
{
    CALL_STACK_MESSAGE2("DestroyIllegalChars(%s)", pszPath);
    while (*pszPath)
    {
        if (_tcschr(_T("*?<>|\":"), *pszPath) != NULL)
            *pszPath = '_';
        pszPath++;
    }
}

BOOL CPluginInterfaceForArchiver::UnpackOneFile(CSalamanderForOperationsAbstract* salamander,
                                                const char* fileName, CPluginDataInterfaceAbstract* pluginDataPar,
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
    BOOL ret = FALSE;
    ArcRoot = "";
    RootLen = Abort = 0;
    //  FirstFile = TRUE;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;
    TCHAR justName[MAX_PATH];
    lstrcpy(justName, nameInArchive);
    SalamanderGeneral->SalPathStripPath(justName);
    PluginData = (CPluginDataInterface*)pluginDataPar;

    // extract files
    List = FALSE;

    // overime si, zda nemame v plugin data ulozeny prvi volume
    DWORD attr;
    LPCTSTR fv;
    if (fileData &&
        (fv = PluginData->GetFirstVolume()) != NULL &&
        (attr = SalamanderGeneral->SalGetFileAttributes(fv)) != -1 &&
        (attr & FILE_ATTRIBUTE_DIRECTORY) == 0)
    {
        _tcscpy(ArcFileName, fv);
    }
    else
    {
        SwitchToFirstVol(fileName);
    }

    PluginData->Silent &= SF_ALLENRYPT;

    if (OpenArchive())
    {
        if (PluginData->SolidEncrypted)
            if (!SetSolidPassword())
            {
                RARCloseArchive(ArcHandle);
                ArcHandle = NULL;
                return FALSE;
            }

        CFileHeader header;
        int op, count = 0;
        BOOL match = FALSE;
        CRARFileData* rarFileData = (CRARFileData*)fileData->PluginData;
        while (ReadHeader(&header) && *header.FileName)
        {
            TargetFile = INVALID_HANDLE_VALUE;
            op = RAR_SKIP;
            /*if ((header.Flags & RHDF_SOLID) && FirstFile && (header.Flags & RHDF_ENCRYPTED))
      {
        if (PluginData->Silent & SF_ENCRYPTED) goto UOF_NEXT;
        if (!(PluginData->Silent & SF_ALLENRYPT))
        {
          switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName, PluginData->Password, PD_NOSKIP))
          {
            case IDALL: PluginData->Silent |= SF_ALLENRYPT;
            case IDOK:  break;
            case IDSKIPALL: PluginData->Silent |= SF_ENCRYPTED; goto UOF_NEXT;
            default: Abort = TRUE; goto UOF_NEXT;
          }
        }
        RARSetPassword(ArcHandle, PluginData->Password);
      }*/
            if (!(header.Flags & RHDF_DIRECTORY) &&
                (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                               nameInArchive, -1, header.FileName, -1) == CSTR_EQUAL) &&
                (!rarFileData || (count == rarFileData->ItemNumber)))
            {
                match = TRUE;
                if (header.Flags & RHDF_SPLITBEFORE)
                {
                    ContinuedFileDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName);
                    goto UOF_NEXT;
                }
                strncpy_s(TargetName, targetDir, _TRUNCATE);
                DestroyIllegalChars(justName);
                if (!SalamanderGeneral->SalPathAppend(TargetName, justName, MAX_PATH))
                {
                    TargetName[0] = 0;
                    Error(IDS_TOOLONGNAME);
                    goto UOF_NEXT;
                }
                if (!(header.Flags & RHDF_SOLID) && (header.Flags & RHDF_ENCRYPTED))
                {
                    if (!(PluginData->Silent & SF_ALLENRYPT))
                    {
                        switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName, PluginData->Password, PD_NOSKIP | PD_NOSKIPALL))
                        {
                        case IDALL:
                            PluginData->Silent |= SF_ALLENRYPT;
                        case IDOK:
                            break;
                        default:
                            goto UOF_NEXT;
                        }
                    }
                    RARSetPassword(ArcHandle, PluginData->Password);
                }
                char buf[100];
                GetInfo(buf, &header.Time, header.Size);
                BOOL skip;
                CQuadWord q = header.Size;
                bool allocate = CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
                q += CQuadWord(0, 0x80000000);
                TargetFile = SalamanderSafeFile->SafeFileCreate(TargetName,
                                                                GENERIC_WRITE, FILE_SHARE_READ,
                                                                header.Attr & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                                FALSE, SalamanderGeneral->GetMsgBoxParent(), nameInArchive, buf,
                                                                &PluginData->Silent, TRUE, &skip, NULL, 0, allocate ? &q : NULL, NULL);
                if (TargetFile != INVALID_HANDLE_VALUE)
                    op = RAR_TEST;
            }

        UOF_NEXT:
            int r;
            r = ProcessFile(op, header.FileName);
            if (op == RAR_TEST)
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
            if (!(header.Flags & RHDF_SPLITBEFORE))
            {
                // Don't count the same file twice
                count++;
            }
            //      if (!(header.Flags & RHDF_DIRECTORY)) FirstFile = FALSE;
        }
        Config.Options &= ~OP_SKITHISFILE;
        RARCloseArchive(ArcHandle);
        ArcHandle = NULL;
    }

    Salamander = NULL;
    if (!ret)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_FILENOTFOUND), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
    }

    return ret;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    BOOL ret = TRUE;
    Abort = FALSE;
    ArcRoot = "";
    RootLen = 0;
    TIndirectArray2<char> masks(16);

    if (!ConstructMaskArray(masks, mask) || masks.Count == 0)
        return FALSE;

    // extract files
    if (!SwitchToFirstVol(fileName))
    {
        return FALSE;
    }

    PluginData = new CPluginDataInterface;

    Salamander = salamander;
    BOOL haveTotalProgress = UnpackWholeArchiveCalculateProgress(masks);
    if (Abort)
    {
        Salamander = NULL;
        delete PluginData;
        return FALSE;
    }

    List = FALSE;
    //  FirstFile = TRUE;
    AllocateWholeFile = TRUE;
    TestAllocateWholeFile = TRUE;

    // Petr: nelze umistit pred volani UnpackWholeArchiveCalculateProgress, jinak se posbiraji
    // jmena svazku archivu 2x (krome prvniho svazku, ten bude jen jednou)
    if (delArchiveWhenDone)
        archiveVolumes->Add(ArcFileName, -2);
    if (ArchiveVolumes != NULL)
        TRACE_E("CPluginInterfaceForArchiver::UnpackWholeArchive(): unexpected situation: ArchiveVolumes is not NULL!");
    ArchiveVolumes = delArchiveWhenDone ? archiveVolumes : NULL;

    TCHAR title[1024];
    _stprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(ArcFileName));
    Salamander->OpenProgressDialog(title, haveTotalProgress, NULL, TRUE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);
    if (haveTotalProgress)
        Salamander->ProgressSetTotalSize(CQuadWord(-1, -1), ProgressTotal);

    ret = OpenArchive();
    if (ret)
    {
        CFileHeader header;
        CQuadWord currentProgress = CQuadWord(0, 0);
        Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
        while ((ret = ReadHeader(&header)) != 0 && *header.FileName)
        {
            if (!(header.Flags & RHDF_SPLITBEFORE))
            {
                // This means that the file is supposed to be skipped.
                // Extracting a file spanning multiple volumes is done at once and a fragment with RHDF_SPLITBEFORE is never met
                //        continue;
                Config.Options &= ~OP_SKITHISFILE;
            }
            int op = RAR_SKIP;
            BOOL match = FALSE;
            TargetFile = INVALID_HANDLE_VALUE;
            /*if ((header.Flags & RHDF_SOLID) && FirstFile && (header.Flags & RHDF_ENCRYPTED))
      {
        if (PluginData->Silent & SF_ENCRYPTED) goto UWA_NEXT;
        if (!(PluginData->Silent & SF_ALLENRYPT))
        {
          switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), header.FileName, PluginData->Password, PD_NOSKIP))
          {
            case IDALL: PluginData->Silent |= SF_ALLENRYPT;
            case IDOK:  break;
            case IDSKIPALL: PluginData->Silent |= SF_ENCRYPTED; goto UWA_NEXT;
            default: Abort = TRUE; goto UWA_NEXT;
          }
        }
        RARSetPassword(ArcHandle, PluginData->Password);
      }*/

            const char* name = SalamanderGeneral->SalPathFindFileName(header.FileName);
            BOOL nameHasExt = _tcsrchr(name, '.') != NULL; // ".cvspass" ve Windows je pripona

            int i;
            for (i = 0; i < masks.Count; i++)
            {
                if (SalamanderGeneral->AgreeMask(name, masks[i], nameHasExt))
                {
                    match = TRUE;
                    Salamander->ProgressSetTotalSize(header.Size, CQuadWord(-1, -1));
                    if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                    {
                        Abort = TRUE;
                        break;
                    }
                    if (DoThisFile(&header, fileName, targetDir))
                    {
                        if (!(header.Flags & RHDF_DIRECTORY))
                            op = RAR_TEST;
                        else
                            SetFileAttributes(TargetName, header.Attr);
                    }
                    break;
                }
            }

            //UWA_NEXT:
            ret = ProcessFile(op, header.FileName);
            if (op == RAR_TEST)
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
                currentProgress += header.Size;
                if (!Salamander->ProgressSetSize(header.Size, haveTotalProgress ? currentProgress : CQuadWord(-1, -1), TRUE))
                {
                    ret = FALSE;
                    break;
                }
            }
            //      if (!(header.Flags & RHDF_DIRECTORY)) FirstFile = FALSE;
        }
        Config.Options &= ~OP_SKITHISFILE;
        RARCloseArchive(ArcHandle);
        ArcHandle = NULL;
    }

    Salamander->CloseProgressDialog();
    Salamander = NULL;
    ArchiveVolumes = NULL;
    delete PluginData;

    return ret;
}

BOOL CPluginInterfaceForArchiver::Error(int error, ...)
{
    CALL_STACK_MESSAGE_NONE
    int lastErr = GetLastError();
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::Error(%d, )", error);
    TCHAR buf[1024]; //temp variable
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

BOOL CPluginInterfaceForArchiver::Init()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::Init()");
    ArchiveVolumes = NULL;
    char buf[MAX_PATH + 12];
    if (!GetModuleFileName(DLLInstance, buf, 1024))
        return Error(IDS_ERRMODULEFN);
    SalamanderGeneral->CutDirectory(buf);
    SalamanderGeneral->SalPathAppend(buf, "unrar.dll", MAX_PATH + 12);
    UnRarDll = LoadLibrary(buf);
    if (!UnRarDll)
        return Error(IDS_ERRLOADLIB, buf);

    FRARGetDllVersion RARGetDllVersion = (FRARGetDllVersion)GetProcAddress(UnRarDll, "RARGetDllVersion");
    if (RARGetDllVersion == NULL || RARGetDllVersion() < RAR_DLL_VERSION)
    {
        FreeLibrary(UnRarDll);
        UnRarDll = NULL;
        return Error(IDS_BADDLL);
    }

    if ((_RAROpenArchiveEx = (FRAROpenArchiveEx)GetProcAddress(UnRarDll, "RAROpenArchiveEx")) == NULL ||
        (_RARCloseArchive = (FRARCloseArchive)GetProcAddress(UnRarDll, "RARCloseArchive")) == NULL ||
        (_RARProcessFile = (FRARProcessFile)GetProcAddress(UnRarDll, "RARProcessFile")) == NULL ||
        //(RARReadHeader = (FRARReadHeader) GetProcAddress(UnRarDll, "RARReadHeader")) == NULL ||
        (_RARReadHeaderEx = (FRARReadHeaderEx)GetProcAddress(UnRarDll, "RARReadHeaderEx")) == NULL ||
        (_RARSetCallback = (FRARSetCallback)GetProcAddress(UnRarDll, "RARSetCallback")) == NULL ||
        //(_RARSetChangeVolProc = (FRARSetChangeVolProc) GetProcAddress(UnRarDll, "RARSetChangeVolProc")) == NULL ||
        //(_RARSetProcessDataProc = (FRARSetProcessDataProc) GetProcAddress(UnRarDll, "RARSetProcessDataProc")) == NULL ||
        (_RARSetPassword = (FRARSetPassword)GetProcAddress(UnRarDll, "RARSetPassword")) == NULL)
    {
        FreeLibrary(UnRarDll);
        UnRarDll = NULL;
        return Error(IDS_ERRGETPROCADDR);
    }
    return TRUE;
}

/*int PASCAL
RARChangeVolProc(char *arcName, int mode)
{
  return InterfaceForArchiver.ChangeVolProc(arcName, mode);
}

int PASCAL
RARProcessDataProc(unsigned char *addr, int size)
{
  return InterfaceForArchiver.ProcessDataProc(addr, size);
}*/

int CALLBACK RARCallback(UINT msg, LPARAM UserData, LPARAM P1, LPARAM P2)
{
    CALL_STACK_MESSAGE_NONE
    switch (msg)
    {
    case UCM_CHANGEVOLUME:
        return InterfaceForArchiver.ChangeVolProc((char*)P1, (int)P2);

    case UCM_PROCESSDATA:
        return InterfaceForArchiver.ProcessDataProc((unsigned char*)P1, (int)P2);

    case UCM_NEEDPASSWORD:
        return InterfaceForArchiver.NeedPassword((char*)P1, (int)P2);
    }
    return 0;
}

BOOL CPluginInterfaceForArchiver::OpenArchive()
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::OpenArchive()");
    RAROpenArchiveDataEx oad;
    ZeroMemory(&oad, sizeof(oad));
    oad.ArcName = ArcFileName;
    oad.ArcNameW = NULL;
    // Warning: RAR_OM_LIST lists files spanned over multiple parts only once,
    // but RAR_OM_EXTRACT lists every file segment
    oad.OpenMode = List ? RAR_OM_LIST : RAR_OM_EXTRACT;
    oad.CmtBuf = NULL;
    oad.CmtBufSize = 0;
    oad.Callback = RARCallback;
    PluginData->PasswordForOpenArchive = TRUE;
    ArcHandle = RAROpenArchiveEx(&oad);
    PluginData->PasswordForOpenArchive = FALSE;
    if (!ArcHandle)
    {
        int err;
        switch (oad.OpenResult)
        {
        case ERAR_NO_MEMORY:
            err = IDS_LOWMEM;
            break;
        case ERAR_BAD_DATA:
            err = IDS_BADDATA;
            break;
        case ERAR_UNKNOWN_FORMAT:
            err = IDS_BADARC;
            break;
        case ERAR_BAD_ARCHIVE:
            err = IDS_BADARC;
            break;
        case ERAR_EOPEN:
            err = IDS_ERROPENARC;
            break;
        case ERAR_BAD_PASSWORD:
            err = IDS_BADPASSWORD;
            break;
        case ERAR_MISSING_PASSWORD:
            return FALSE; // Cancel v dialogu pro zadani hesla, dalsi hlaska nema smysl (user vi, proc se archiv neotevrel)
        default:
            err = IDS_UNKNOWN;
        }
        return Error(err);
    }
    ArcFlags = oad.Flags;
    //RARSetChangeVolProc(ArcHandle, RARChangeVolProc);
    //RARSetProcessDataProc(ArcHandle, RARProcessDataProc);
    //RARSetCallback(ArcHandle, RARCallback, 0);

    // u RAR4 archivu se sifrovanymi jmeny souboru to po zadani spatneho hesla v callbacku
    // dojde sem a diky PasswordForOpenArchive==TRUE pri volani RAROpenArchiveEx je po zadani
    // hesla nastaveno SF_ALLENRYPT i jen po kliknuti na OK v dialogu zadani hesla,
    // coz zajisti, abysme se znovu neptali na heslo (misto toho se ukaze chybova hlaska o spatnem heslu)
    if ((oad.Flags & 0x0080) /* block headers encrypted */)
    {
        if (!(PluginData->Silent & SF_ALLENRYPT))
        {
            if (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), ArcFileName, PluginData->Password,
                               PD_NOSKIP | PD_NOSKIPALL | PD_NOALL) == IDCANCEL)
            {
                RARCloseArchive(ArcHandle);
                ArcHandle = NULL;
                return FALSE;
            }
            else
                PluginData->Silent |= SF_ALLENRYPT;
        }
        RARSetPassword(ArcHandle, PluginData->Password);
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::ReadHeader(CFileHeader* header)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::ReadHeader()");
    RARHeaderDataEx headerData;
    ZeroMemory(&headerData, sizeof(headerData));
    headerData.CmtBuf = NULL;
    headerData.CmtBufSize = 0;
    int err = 0;
    switch (RARReadHeaderEx(ArcHandle, &headerData))
    {
    case 0:
    {
        // Not every char representable in ANSI page can be reprsented in OEM page used by RAR files
        // e.g. the Ellipsis character 0x2026
        if (headerData.FileNameW[0])
        {
            WideCharToMultiByte(CP_ACP, WC_COMPOSITECHECK, headerData.FileNameW, -1, header->FileName,
                                sizeof(header->FileName), NULL, NULL);
        }
        else
        {
            lstrcpy(header->FileName, headerData.FileName + (headerData.FileName[0] == '\\' ? 1 : 0));
            OemToChar(header->FileName, header->FileName);
        }
        header->Size = CQuadWord(headerData.UnpSize, headerData.UnpSizeHigh);
        header->CompSize = CQuadWord(headerData.PackSize, headerData.PackSizeHigh);
        FILETIME ft;
        if (!DosDateTimeToFileTime(HIWORD(headerData.FileTime),
                                   LOWORD(headerData.FileTime), &ft))
        {
            SystemTimeToFileTime(&MinTime, &ft);
        }
        LocalFileTimeToFileTime(&ft, &header->Time);

        // uprava atributu z archivu z Unixu, a dalsich systemu nekompatibilnich s Win32
        header->Attr = headerData.FileAttr & FILE_ATTRIBUTE_MASK;
        switch (headerData.HostOS)
        {
        case 0 /* MS-DOS */:
        case 1 /* OS/2 */:
        case 2 /* Win32 */:
            break;

            //      case HOST_UNIX:
            //      case HOST_BEOS:
        default:
        {
            if (headerData.Flags & RHDF_DIRECTORY)
                header->Attr = FILE_ATTRIBUTE_DIRECTORY;
            else
                header->Attr = FILE_ATTRIBUTE_ARCHIVE;
            break;
        }
        }

        if (headerData.Flags & RHDF_DIRECTORY)
            header->Attr |= FILE_ATTRIBUTE_DIRECTORY;
        else
            header->Attr &= ~FILE_ATTRIBUTE_DIRECTORY;

        header->Flags = headerData.Flags;
        break;
    }

    case ERAR_END_ARCHIVE:
        header->FileName[0] = 0;
        break;

    case ERAR_BAD_DATA:
        err = IDS_BADDATA;
        break;
    case ERAR_EOPEN:
        if (NotWholeArchListed)
        {
            // Looks like missing volume -> abort listing the archive
            // returning TRUE could mean listing spanned file twice
            return FALSE;
        }
        err = IDS_VOLUMEERR;
        break;
    default:
        err = IDS_UNKNOWN;
    }
    if (err)
        return Error(err);
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::ProcessFile(int operation, char* fileName)
{
    DEBUG_SLOW_CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ProcessFile(%d, )", operation);
    int err = 0;
    Success = FALSE;
    int ret = RARProcessFile(ArcHandle, operation, "", NULL);
    if (Abort)
        return FALSE;
    switch (ret)
    {
    case 0:
        break;
    case ERAR_NO_MEMORY:
        err = IDS_LOWMEM;
        break;
    case ERAR_UNKNOWN_FORMAT:
    case ERAR_BAD_ARCHIVE:
        err = IDS_BADARC;
        break;
    case ERAR_EOPEN:
        if (List)
            return FALSE;
        err = IDS_VOLUMEERR;
        break;
    case ERAR_EREAD:
        err = IDS_ERRREADARC;
        break;

    case ERAR_BAD_PASSWORD:
    case ERAR_BAD_DATA:
    {
        DWORD silentFlag = ret == ERAR_BAD_PASSWORD ? SF_PASSWD : SF_DATA;
        if (!(PluginData->Silent & silentFlag) && !Abort)
        {
            PluginData->Silent &= ~SF_ALLENRYPT;
            switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                   fileName, LoadStr(ret == ERAR_BAD_PASSWORD ? IDS_BADPASSWORD : IDS_CRC), NULL))
            {
            case DIALOG_SKIPALL:
                PluginData->Silent |= silentFlag;
            case DIALOG_SKIP:
                return FALSE;
            default:
                Abort = TRUE;
                return FALSE;
            }
        }
        return FALSE;
    }

    case ERAR_END_ARCHIVE:
        if (List)
        {
            Abort = TRUE;
            return Error(IDS_UNKNOWN);
        }
        return Success;

    //case ERAR_EREFERENCE:
    default:
        err = IDS_UNKNOWN;
    }
    if (err)
    {
        if (!Abort)
        {
            Abort = TRUE;
            Error(err);
        }
        return FALSE;
    }
    return TRUE;
}

int CPluginInterfaceForArchiver::ChangeVolProc(char* arcName, int mode)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ChangeVolProc(, %d)", mode);
    if (mode == RAR_VOL_ASK)
    {
        if (List)
        {
            NotWholeArchListed = TRUE;
            return -1; // listujeme, jen dokud to jde bez ptani se na dalsi volumy
        }
        if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), arcName) != IDOK)
        {
            Abort = TRUE;
            return -1;
        }
    }
    else
    {
        if (mode == RAR_VOL_NOTIFY && ArchiveVolumes != NULL)
            ArchiveVolumes->Add(arcName, -2);
    }
    return 1;
}

BOOL CPluginInterfaceForArchiver::SafeSeek(CQuadWord position)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::SafeSeek(0x%I64X)", position.Value);
    char buf[1024];
    while (1)
    {
        CQuadWord pos = position;
        if (SetFilePointer(TargetFile, pos.LoDWord, LPLONG(&pos.HiDWord), FILE_BEGIN) != 0xFFFFFFFF ||
            GetLastError() == NO_ERROR)
            return TRUE; // success
        lstrcpy(buf, LoadStr(IDS_UNABLESEEK));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (PluginData->Silent & SF_IOERRORS)
            return FALSE;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            PluginData->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return FALSE;
        }
    }
}

int CPluginInterfaceForArchiver::ProcessDataProc(unsigned char* addr, int size)
{
    SLOW_CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ProcessDataProc(, %d)", size);
    Success = FALSE;
    if (TargetFile == INVALID_HANDLE_VALUE)
        return 1;
    if (size == 0)
    {
        Success = TRUE;
        return 1;
    }
    char buf[1024];
    CQuadWord pos = CQuadWord(0, 0);
    while (1)
    {
        pos.LoDWord = SetFilePointer(TargetFile, 0, LPLONG(&pos.HiDWord), FILE_CURRENT);
        if (pos.LoDWord != 0xFFFFFFFF || GetLastError() == NO_ERROR)
            break;
        lstrcpy(buf, LoadStr(IDS_UNABLEGETFIELPOS));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (PluginData->Silent & SF_IOERRORS)
            return -1;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            PluginData->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return -1;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
    }
    DWORD written;
    while (1)
    {
        if (WriteFile(TargetFile, addr, size, &written, NULL))
        {
            if (!Salamander->ProgressAddSize(size, TRUE))
            {
                Abort = TRUE;
                return -1;
            }
            Success = TRUE;
            return 1; // sucess
        }
        lstrcpy(buf, LoadStr(IDS_UNABLEWRITE));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);

        if (PluginData->Silent & SF_IOERRORS)
            return -1;

        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, TargetName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            PluginData->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            return -1;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return -1;
        }
        if (!SafeSeek(pos))
            return -1;
    }
    return 1;
}

int CPluginInterfaceForArchiver::NeedPassword(char* password, int size)
{
    if (!(PluginData->Silent & SF_ALLENRYPT))
    {
        switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), ArcFileName, PluginData->Password,
                               PD_NOSKIP | PD_NOSKIPALL | (PluginData->PasswordForOpenArchive ? PD_NOALL : 0)))
        {
        case IDOK:
            if (!PluginData->PasswordForOpenArchive)
                break;
            // else break; // Petr: tady break nechybi! (OK ma funkci All)
        case IDALL:
            PluginData->Silent |= SF_ALLENRYPT;
            break;
        default:
            Abort = TRUE;
            return -1;
        }
    }
    lstrcpyn(password, PluginData->Password, size);
    return 1;
}

BOOL CPluginInterfaceForArchiver::SwitchToFirstVol(LPCTSTR arcName, BOOL* saveFirstVolume)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::SwitchToFirstVol(%s)", arcName);
    lstrcpy(ArcFileName, arcName);
    LPTSTR ext = PathFindExtension(ArcFileName);
    if (!ext)
        return TRUE;
    if (lstrlen(ext) > 3 &&
        isdigit(ext[2]) && isdigit(ext[3]))
    {
        TCHAR oldExt[MAX_PATH];
        lstrcpy(oldExt, ext);
        if (isdigit(ext[1]))
        {
            lstrcpy(ext, _T(".001")); // .001, .002, .003, etc.
        }
        else
        {
            lstrcpy(ext, _T(".rar")); // .rar, .r00, .r01, etc.
        }
        DWORD attr = SalamanderGeneral->SalGetFileAttributes(ArcFileName);
        if (attr == -1 || attr & FILE_ATTRIBUTE_DIRECTORY)
        {
            TCHAR path[MAX_PATH];
            _tcscpy(path, ArcFileName);
            if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), path, LoadStr(IDS_SELECTFIRST)) == IDOK)
            {
                _tcscpy(ArcFileName, path);
                if (saveFirstVolume)
                    *saveFirstVolume = TRUE;
                return TRUE;
            }
            else
            {
                lstrcpy(ext, oldExt);
                return FALSE;
            }
        }
        return TRUE;
    }
    // jeste otestujeme, jestli je to archiv podle novych jmennych konvenci
    LPTSTR part = ext - 1;
    while (part >= ArcFileName)
    {
        if (*part == '.')
            break; // ".cvspass" ve Windows je pripona
        part--;
    }
    if (part >= ArcFileName && _tcsnicmp(part, _T(".part"), 5) == 0)
    {
        LPTSTR iterator = part + 5;
        while (isdigit(*iterator))
            iterator++;
        int digits = (int)(iterator - part - 5);
        if (digits && *iterator == '.')
        {
            // je to soubor pojmenovany podle nove konvence
            TCHAR path[MAX_PATH];

            _tcsncpy_s(path, ArcFileName, part - ArcFileName);
            _stprintf_s(path + (part - ArcFileName), _countof(path) - (part - ArcFileName), _T(".part%0*d.rar"), digits, 1);

            DWORD attr = SalamanderGeneral->SalGetFileAttributes(path);
            if (!(attr & FILE_ATTRIBUTE_DIRECTORY))
            {
                strcpy(ArcFileName, path);
                if (saveFirstVolume)
                    *saveFirstVolume = TRUE;
                return TRUE;
            }
            if (NextVolumeDialog(SalamanderGeneral->GetMsgBoxParent(), path, LoadStr(IDS_SELECTFIRST)) == IDOK)
            {
                _tcscpy(ArcFileName, path);
                if (saveFirstVolume)
                    *saveFirstVolume = TRUE;
            }
            else
            {
                return FALSE;
            }
            // Why is this here?
            UpdateWindow(SalamanderGeneral->GetMainWindowHWND());
        }
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::MakeFilesList(TIndirectArray2<CRARExtractInfo>& files, SalEnumSelection next, void* nextParam, const char* targetDir)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::MakeFilesList(, , , %s)", targetDir);
    LPCTSTR nextName;
    BOOL isDir;
    CQuadWord size;
    TCHAR dir[MAX_PATH];
    LPTSTR addDir;
    int dirLen;
    const CFileData* pFileData;
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
    while ((nextName = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, &pFileData, nextParam, &errorOccured)) != NULL)
    {
        if (isDir)
        {
            if (dirLen + lstrlen(nextName) + 1 >= MAX_PATH)
            {
                if (PluginData->Silent & SF_LONGNAMES)
                    continue;
                switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, nextName, LoadStr(IDS_TOOLONGNAME), NULL))
                {
                case DIALOG_SKIPALL:
                    PluginData->Silent |= SF_LONGNAMES;
                case DIALOG_SKIP:
                    continue;
                case DIALOG_CANCEL:
                case DIALOG_FAIL:
                    return FALSE;
                }
            }
            lstrcpy(addDir, nextName);
            DestroyIllegalChars(addDir);
            BOOL skip;
            if (SalamanderSafeFile->SafeFileCreate(dir, 0, 0, 0, TRUE, SalamanderGeneral->GetMsgBoxParent(), NULL, NULL,
                                                   &PluginData->Silent, TRUE, &skip, NULL, 0, NULL, NULL) == INVALID_HANDLE_VALUE &&
                !skip)
                return FALSE;
            SetFileAttributes(dir, pFileData->Attr);
        }
        else
        {
            CRARExtractInfo* ei = (CRARExtractInfo*)malloc(sizeof(CRARExtractInfo) + RootLen + lstrlen(nextName) + 2);
            if (!ei)
                return Error(IDS_LOWMEM);
            lstrcpy(ei->FileName, ArcRoot);
            LPTSTR ptr = ei->FileName + RootLen;
            if (RootLen && *(ptr - 1) != '\\')
                *ptr++ = '\\';
            lstrcpy(ptr, nextName);
            CRARFileData* rarFileData = (CRARFileData*)pFileData->PluginData;
            ei->ItemNumber = rarFileData ? rarFileData->ItemNumber : -1;
            if (!files.Add(ei))
            {
                free(ei);
                return Error(IDS_LOWMEM);
            }
            ProgressTotal += size;
        }
    }
    return errorOccured != SALENUM_CANCEL && // test, jestli nenastala chyba a uzivatel si nepral prerusit operaci (tlacitko Cancel)
           SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(), targetDir, ProgressTotal, LoadStr(IDS_PLUGINNAME));
}

BOOL CPluginInterfaceForArchiver::DoThisFile(CFileHeader* header, const char* arcName, const char* targetDir)
{
    CALL_STACK_MESSAGE3("CPluginInterfaceForArchiver::DoThisFile(, %s, %s)", arcName,
                        targetDir);
    char message[MAX_PATH + 32];

    lstrcpy(message, LoadStr(IDS_EXTRACTING));
    lstrcat(message, header->FileName);
    Salamander->ProgressDialogAddText(message, TRUE);
    if (header->Flags & RHDF_SPLITBEFORE)
    {
        TRACE_I("Skipping file continued from previuous volume: " << header->FileName);
        if (!(Config.Options & (OP_SKIPCONTINUED | OP_SKITHISFILE)) && ContinuedFileDialog(SalamanderGeneral->GetMsgBoxParent(), header->FileName) != IDOK)
            Abort = TRUE;
        return FALSE;
    }
    if (!(header->Flags & RHDF_SOLID) && (header->Flags & RHDF_ENCRYPTED))
    {
        if (PluginData->Silent & SF_ENCRYPTED)
            return FALSE;
        if (!(PluginData->Silent & SF_ALLENRYPT))
        {
            switch (PasswordDialog(SalamanderGeneral->GetMsgBoxParent(), header->FileName, PluginData->Password, 0))
            {
            case IDALL:
                PluginData->Silent |= SF_ALLENRYPT;
            case IDOK:
                break;
            case IDSKIPALL:
                PluginData->Silent |= SF_ENCRYPTED;
            case IDSKIP:
                return FALSE;
            default:
                Abort = TRUE;
                return FALSE;
            }
        }
        RARSetPassword(ArcHandle, PluginData->Password);
    }
    strncpy_s(TargetName, targetDir, _TRUNCATE);
    if (!SalamanderGeneral->SalPathAppend(TargetName, header->FileName + RootLen, MAX_PATH))
    {
        TargetName[0] = 0;
        if (PluginData->Silent & SF_LONGNAMES)
            return FALSE;
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, header->FileName, LoadStr(IDS_TOOLONGNAME), NULL))
        {
        case DIALOG_SKIPALL:
            PluginData->Silent |= SF_LONGNAMES;
        case DIALOG_SKIP:
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            Abort = TRUE;
            return FALSE;
        }
    }
    char nameInArc[MAX_PATH + MAX_PATH];
    lstrcpy(nameInArc, arcName);
    SalamanderGeneral->SalPathAppend(nameInArc, header->FileName, MAX_PATH + MAX_PATH);
    char buf[100];
    GetInfo(buf, &header->Time, header->Size);
    DestroyIllegalChars(TargetName + 2);
    BOOL skip;
    CQuadWord q = header->Size;
    BOOL allocate = AllocateWholeFile &&
                    CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
    if (TestAllocateWholeFile)
        q += CQuadWord(0, 0x80000000);
    TargetFile = SalamanderSafeFile->SafeFileCreate(TargetName, GENERIC_WRITE, FILE_SHARE_READ,
                                                    header->Attr & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN, (header->Flags & RHDF_DIRECTORY) != 0,
                                                    SalamanderGeneral->GetMsgBoxParent(), nameInArc, buf, &PluginData->Silent, TRUE, &skip, NULL, 0,
                                                    allocate ? &q : NULL, NULL);
    if (skip)
    {
        if (header->Flags & RHDF_SPLITAFTER)
        {
            Config.Options |= OP_SKITHISFILE;
        }
        return FALSE;
    }
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
    //if (header->Attr & FILE_ATTRIBUTE_DIRECTORY) header->Size = 1;
    //Salamander->ProgressSetTotalSize(header->Size, ProgressTotal);
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::ConstructMaskArray(TIndirectArray2<char>& maskArray, const char* masks)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ConstructMaskArray(, %s)", masks);
    LPCTSTR sour;
    LPTSTR dest;
    size_t newMaskLen;
    TCHAR buffer[MAX_PATH];

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
        newMaskLen = _tcslen(dest);
        if (newMaskLen)
        {
            LPTSTR newMask = new TCHAR[newMaskLen + 1];
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

BOOL CPluginInterfaceForArchiver::UnpackWholeArchiveCalculateProgress(TIndirectArray2<char>& masks)
{
    CALL_STACK_MESSAGE1("CPluginInterfaceForArchiver::UnpackWholeArchiveCalculateProgress()");
    List = TRUE;
    BOOL ret = TRUE;
    NotWholeArchListed = FALSE;

    ProgressTotal = CQuadWord(0, 0);

    if (OpenArchive())
    {
        if ((ArcFlags & AF_VOLUME) && !(ArcFlags & AF_FIRST_VOLUME))
        {
            NotWholeArchListed = TRUE;
        }
        else
        {
            CFileHeader header;
            CFileData fileData;
            while ((ret = ReadHeader(&header)) != 0 && *header.FileName)
            {
                if (header.Flags & RHDF_SPLITBEFORE)
                {
                    NotWholeArchListed = TRUE;
                    break;
                }
                const char* name = SalamanderGeneral->SalPathFindFileName(header.FileName);
                BOOL nameHasExt = _tcsrchr(name, '.') != NULL; // ".cvspass" ve Windows je pripona
                int i;
                for (i = 0; i < masks.Count; i++)
                {
                    if (SalamanderGeneral->AgreeMask(name, masks[i], nameHasExt))
                    {
                        ProgressTotal += header.Size;
                        break;
                    }
                }
                if (!ProcessFile(RAR_SKIP, header.FileName))
                {
                    if (Abort)
                        ret = FALSE;
                    break;
                }
            }
        }

        RARCloseArchive(ArcHandle);
        ArcHandle = NULL;
    }
    else
        ret = FALSE;

    if (NotWholeArchListed)
        ret = FALSE;

    return ret;
}

// ****************************************************************************

void GetInfo(char* buffer, FILETIME* lastWrite, CQuadWord& size)
{
    CALL_STACK_MESSAGE2("GetInfo(, , 0x%I64X)", size.Value);
    SYSTEMTIME st;
    FILETIME ft;
    FileTimeToLocalFileTime(lastWrite, &ft);
    FileTimeToSystemTime(&ft, &st);

    char date[50], time[50], number[50];
    if (GetTimeFormat(LOCALE_USER_DEFAULT, 0, &st, NULL, time, 50) == 0)
        sprintf(time, "%u:%02u:%02u", st.wHour, st.wMinute, st.wSecond);
    if (GetDateFormat(LOCALE_USER_DEFAULT, DATE_SHORTDATE, &st, NULL, date, 50) == 0)
        sprintf(date, "%u.%u.%u", st.wDay, st.wMonth, st.wYear);
    sprintf(buffer, "%s, %s, %s", SalamanderGeneral->NumberToStr(number, size), date, time);
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
    LPTSTR iterator = pszPath + len - 1;
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

//
// ****************************************************************************
// CPluginDataInterface
//

void CPluginDataInterface::ReleasePluginData(CFileData& file, BOOL isDir)
{
    // file.PluginData is NULL for folders not having extra items in the archive - see GetFileDataForUpDir & GetFileDataForNewDir
    delete (CRARFileData*)file.PluginData; // However, delete NULL is perfectly OK
}

// Callback called by Salamander to obtain custom column text - see spl_com.h / FColumnGetText
// Global variables - pointers to global variables used by Salamander
static const CFileData** TransferFileData = NULL;
static int* TransferIsDir = NULL;
static char* TransferBuffer = NULL;
static int* TransferLen = NULL;
static DWORD* TransferRowData = NULL;
static CPluginDataInterfaceAbstract** TransferPluginDataIface = NULL;
static DWORD* TransferActCustomData = NULL;

static void WINAPI GetPackedSizeText()
{
    if (*TransferIsDir)
    {
        *TransferLen = 0;
    }
    else
    {
        CRARFileData* rarFileData = (CRARFileData*)(*TransferFileData)->PluginData;
        if (rarFileData->PackedSize != 0)
        {
            SalamanderGeneral->NumberToStr(TransferBuffer, CQuadWord().SetUI64(rarFileData->PackedSize));
            *TransferLen = (int)_tcslen(TransferBuffer);
        }
        else
        {
            *TransferLen = 0;
        }
    }
}

void WINAPI
CPluginDataInterface::SetupView(BOOL leftPanel, CSalamanderViewAbstract* view, const char* archivePath,
                                const CFileData* upperDir)
{
    view->GetTransferVariables(TransferFileData, TransferIsDir, TransferBuffer, TransferLen, TransferRowData,
                               TransferPluginDataIface, TransferActCustomData);

    // Special columns added only in Detailed mode
    if (view->GetViewMode() == VIEW_MODE_DETAILED)
    {
        CColumn column;
        if (Config.ListInfoPackedSize)
        {
            // We add Packed Size just after the Size column; or at the end in case of failure
            int sizeIndex = view->GetColumnsCount();
            int i;
            for (i = 0; i < sizeIndex; i++)
                if (view->GetColumn(i)->ID == COLUMN_ID_SIZE)
                {
                    sizeIndex = i + 1;
                    break;
                }

            _tcscpy(column.Name, LoadStr(IDS_LISTINFO_PAKEDSIZE));
            _tcscpy(column.Description, LoadStr(IDS_LISTINFO_PAKEDSIZE_DESC));
            column.GetText = GetPackedSizeText;
            column.SupportSorting = 0;
            column.LeftAlignment = 0;
            column.ID = COLUMN_ID_CUSTOM;
            column.CustomData = 0;
            column.Width = leftPanel ? LOWORD(Config.ColumnPackedSizeWidth) : HIWORD(Config.ColumnPackedSizeWidth);
            column.FixedWidth = leftPanel ? LOWORD(Config.ColumnPackedSizeFixedWidth) : HIWORD(Config.ColumnPackedSizeFixedWidth);
            view->InsertColumn(sizeIndex, &column);
        }
    }
}

void CPluginDataInterface::ColumnFixedWidthShouldChange(BOOL leftPanel, const CColumn* column, int newFixedWidth)
{
    if (column->CustomData == 0)
    {
        if (leftPanel)
            Config.ColumnPackedSizeFixedWidth = MAKELONG(newFixedWidth, HIWORD(Config.ColumnPackedSizeFixedWidth));
        else
            Config.ColumnPackedSizeFixedWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeFixedWidth), newFixedWidth);
    }
    if (newFixedWidth)
        ColumnWidthWasChanged(leftPanel, column, column->Width);
}

void CPluginDataInterface::ColumnWidthWasChanged(BOOL leftPanel, const CColumn* column, int newWidth)
{
    if (column->CustomData == 0)
    {
        if (leftPanel)
            Config.ColumnPackedSizeWidth = MAKELONG(newWidth, HIWORD(Config.ColumnPackedSizeWidth));
        else
            Config.ColumnPackedSizeWidth = MAKELONG(LOWORD(Config.ColumnPackedSizeWidth), newWidth);
    }
}
