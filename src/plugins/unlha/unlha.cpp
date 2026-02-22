// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "lha.h"
#include "unlha.h"
#include "unlha.rh"
#include "unlha.rh2"
#include "lang\lang.rh"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle to SPL - language-independent resources
HINSTANCE HLanguage = NULL;   // handle to SLG - language-dependent resources

// plugin interface object whose methods are called from Salamander
CPluginInterface PluginInterface;
// part of the CPluginInterface interface for the archiver
CPluginInterfaceForArchiver InterfaceForArchiver;
// general Salamander interface - valid from startup until the plugin shuts down
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
// interface for convenient work with files
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;
// variable definition for "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

BOOL WINAPI DllMain(HINSTANCE hinstDLL, DWORD fdwReason, LPVOID lpvReserved)
{
    if (fdwReason == DLL_PROCESS_ATTACH)
        DLLInstance = hinstDLL;
    return TRUE; // DLL can be loaded
}

char* LoadStr(int resID)
{
    return SalamanderGeneral->LoadStr(HLanguage, resID);
}

//****************************************************************************

int WINAPI SalamanderPluginGetReqVer()
{
    return LAST_VERSION_OF_SALAMANDER;
}

// ****************************************************************************

CPluginInterfaceAbstract* WINAPI SalamanderPluginEntry(CSalamanderPluginEntryAbstract* salamander)
{
    // set SalamanderDebug for "dbg.h"
    SalamanderDebug = salamander->GetSalamanderDebug();

    CALL_STACK_MESSAGE1("SalamanderPluginEntry()");

    // this plugin is built for the current Salamander version and newer - perform a check
    if (salamander->GetVersion() < LAST_VERSION_OF_SALAMANDER)
    { // reject older versions
        MessageBox(salamander->GetParentWindow(),
                   REQUIRE_LAST_VERSION_OF_SALAMANDER,
                   "UnLHA" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // load the language module (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "UnLHA" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // obtain the general Salamander interface
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    // initialize the unpacker
    LHAInit();

    // set basic information about the plugin
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_CUSTOMARCHIVERUNPACK,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   "UnLHA" /* do not translate! */, "lzh;lha;lzs");

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
}

void CPluginInterface::SaveConfiguration(HWND parent, HKEY regKey, CSalamanderRegistryAbstract* registry)
{
    CALL_STACK_MESSAGE1("CPluginInterface::SaveConfiguration(, ,)");
}

void CPluginInterface::Configuration(HWND parent)
{
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomUnpacker("UnLHA (Plugin)", "*.lzh;*.lha;*.lzs", FALSE);
    salamander->AddPanelArchiver("lzh;lha;lzs", FALSE, FALSE);
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

BOOL CPluginInterfaceForArchiver::ListArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                              CSalamanderDirectoryAbstract* dir,
                                              CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::ListArchive(, %s, ,)", fileName);
    pluginData = NULL;

    FILE* f;
    if (!LHAOpenArchive(f, fileName))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    int sortByExtDirsAsFiles;
    SalamanderGeneral->GetConfigParameter(SALCFG_SORTBYEXTDIRSASFILES, &sortByExtDirsAsFiles,
                                          sizeof(sortByExtDirsAsFiles), NULL);

    LHA_HEADER hdr;
    CFileData fd;
    int ret = TRUE, gh;
    int count = 0;
    int symlinkinfo = 0, crcinfo = 0;
    int currentOffset = ftell(f);

    while ((gh = LHAGetHeader(f, &hdr)) == GH_SUCCESS)
    {
        // if it ends with '\', remove it (because of directories)
        int l = lstrlen(hdr.name) - 1;
        if (hdr.name[l] == '\\')
        {
            hdr.name[l] = 0;
            l--;
        }
        // find the name and the path
        char* name = hdr.name + l;
        const char* path = hdr.name;
        while (name >= path && *name != '\\')
            name--;
        if (name >= path)
        {
            *name = 0;
            name++;
        }
        else
        {
            path = "";
            name = hdr.name;
        }

        ZeroMemory(&fd, sizeof(fd)); // just to be safe...

        fd.Name = SalamanderGeneral->DupStr(name);
        if (!fd.Name)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            ret = FALSE;
            break;
        }

        fd.Ext = _tcsrchr(fd.Name, '.');
        if (fd.Ext != NULL)
            fd.Ext++; // ".cvspass" counts as an extension on Windows
        else
            fd.Ext = fd.Name + _tcslen(fd.Name);
        fd.Size = CQuadWord(hdr.original_size, 0);
        fd.Attr = hdr.attribute;
        fd.Hidden = fd.Attr & FILE_ATTRIBUTE_HIDDEN ? 1 : 0;
        fd.PluginData = currentOffset;
        fd.LastWrite = hdr.last_modified_filetime;
        fd.DosName = NULL;
        fd.NameLen = _tcslen(fd.Name);
        fd.IsOffline = 0;

        if ((hdr.unix_mode & UNIX_FILE_TYPEMASK) == UNIX_FILE_REGULAR &&
            hdr.method != LZHDIRS_METHOD_NUM)
        {
            fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
            if (!dir->AddFile(path, fd, NULL))
                ret = FALSE;
        }
        else if ((hdr.unix_mode & UNIX_FILE_TYPEMASK) == UNIX_FILE_DIRECTORY ||
                 hdr.method == LZHDIRS_METHOD_NUM)
        {
            if (!sortByExtDirsAsFiles)
                fd.Ext = fd.Name + fd.NameLen; // directories have no extensions
            fd.IsLink = 0;
            if (!dir->AddDir(path, fd, NULL))
                ret = FALSE;
        }
        else if (!symlinkinfo && (hdr.unix_mode & UNIX_FILE_TYPEMASK) == UNIX_FILE_SYMLINK)
        {
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_SYMLINK), LoadStr(IDS_PLUGINNAME), MSGBOX_WARNING);
            symlinkinfo = 1;
        }

        if (!ret)
        {
            SalamanderGeneral->Free(fd.Name);
            SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            break;
        }

        fseek(f, hdr.packed_size, SEEK_CUR);
        count++;
        currentOffset = ftell(f);
    }

    fclose(f);

    if (gh == GH_ERROR)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        ret = FALSE;
    }

    // some files have already been listed, so do not pack and display them
    if (!ret && count)
        ret = TRUE;

    return ret;
}

static BOOL ProgressCallback(int size) // callback invoked during decompression
{
    if (InterfaceForArchiver.UnpackWhole)
        return InterfaceForArchiver.Salamander->ProgressSetSize(CQuadWord(size, 0), CQuadWord(-1, -1), TRUE);
    else
        return InterfaceForArchiver.Salamander->ProgressSetSize(CQuadWord(size, 0),
                                                                InterfaceForArchiver.currentProgress + CQuadWord(size, 0), TRUE);
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

    FILE* f;
    if (!LHAOpenArchive(f, fileName))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    LHA_HEADER hdr;
    fseek(f, (long)fileData->PluginData, SEEK_SET);
    if (LHAGetHeader(f, &hdr) != GH_SUCCESS)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_UNPACKERROR), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        fclose(f);
        return FALSE;
    }

    if (hdr.method == LHA_UNKNOWNMETHOD && hdr.original_size != 0 /* see below : */)
    {
        // I do not know why, but LHA sometimes packs zero-length files with a nonsensical method name...
        // If the file length is 0, I therefore ignore the method type.
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_METHOD), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        fclose(f);
        return FALSE;
    }

    char justName[MAX_PATH];
    lstrcpy(justName, nameInArchive);
    SalamanderGeneral->SalPathStripPath(justName);
    char targetName[MAX_PATH];
    lstrcpy(targetName, targetDir);
    SalamanderGeneral->SalPathAppend(targetName, justName, MAX_PATH);
    BOOL skip;
    DWORD silent = 0;

    char buf[100];
    GetInfo(buf, &hdr.last_modified_filetime, hdr.original_size);

    HANDLE hOutFile = SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE, FILE_SHARE_READ,
                                                         hdr.attribute & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                         FALSE, SalamanderGeneral->GetMsgBoxParent(),
                                                         nameInArchive, buf, &silent, TRUE, &skip, NULL, 0, NULL, NULL);

    if (hOutFile == INVALID_HANDLE_VALUE)
    {
        fclose(f);
        return FALSE;
    }

    int crc = 0, uf = 1;
    if (hdr.original_size)
        uf = LHAUnpackFile(f, hOutFile, &hdr, &crc, targetName);

    CloseHandle(hOutFile);
    fclose(f);

    if (!uf)
    {
        if (iLHAErrorStrId != IDS_WRITEERROR) // because of SafeWriteFile
            SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }
    if (hdr.has_crc && crc != hdr.crc)
    {
        SalamanderGeneral->ClearReadOnlyAttr(targetName); // so a read-only file can be deleted
        DeleteFile(targetName);
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_CRCERROR), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    Salamander = salamander;
    Silent = 0;
    Ret = TRUE;
    Abort = FALSE;
    UnpackWhole = FALSE;
    CRCSkipAll = FALSE;
    ArcRoot = archiveRoot ? archiveRoot : "";
    if (*ArcRoot == '\\')
        ArcRoot++;
    RootLen = lstrlen(ArcRoot);
    TDirectArray<int> offsets(256, 256);

    if (!MakeFilesList(offsets, next, nextParam, targetDir))
        return FALSE;

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(fileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    FILE* f;
    if (!LHAOpenArchive(f, fileName))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        Salamander->CloseProgressDialog();
        return FALSE;
    }

    currentProgress = CQuadWord(0, 0);
    Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
    pfLHAProgress = ProgressCallback;

    LHA_HEADER hdr;
    int i;
    for (i = 0; i < offsets.Count; i++)
    {
        fseek(f, offsets[i], SEEK_SET);
        if (LHAGetHeader(f, &hdr) != GH_SUCCESS)
        {
            Ret = FALSE;
            break;
        }

        Salamander->ProgressSetTotalSize(CQuadWord(hdr.original_size, 0), ProgressTotal);
        if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
        {
            Ret = FALSE;
            break;
        }

        UnpackInnerBody(f, targetDir, fileName, hdr, FALSE);
        if (Abort)
            break;

        currentProgress += CQuadWord(hdr.original_size, 0);
        Salamander->ProgressSetSize(CQuadWord(hdr.original_size, 0), currentProgress, TRUE);
    }

    pfLHAProgress = NULL;
    Salamander->CloseProgressDialog();
    fclose(f);

    return Ret;
}

BOOL CPluginInterfaceForArchiver::UnpackWholeArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                     const char* mask, const char* targetDir, BOOL delArchiveWhenDone,
                                                     CDynamicString* archiveVolumes)
{
    CALL_STACK_MESSAGE5("CPluginInterfaceForArchiver::UnpackWholeArchive(, %s, %s, %s, %d,)",
                        fileName, mask, targetDir, delArchiveWhenDone);

    Salamander = salamander;
    Silent = 0;
    Abort = FALSE;
    Ret = TRUE;
    RootLen = 0;
    UnpackWhole = TRUE;
    CRCSkipAll = FALSE;

    TIndirectArray<char> masks(16, 16, dtDelete);
    if (!ConstructMaskArray(masks, mask) || masks.Count == 0)
        return FALSE;

    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2);

    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(fileName));
    Salamander->OpenProgressDialog(title, FALSE, NULL, TRUE);

    FILE* f;
    if (!LHAOpenArchive(f, fileName))
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        Salamander->CloseProgressDialog();
        return FALSE;
    }

    pfLHAProgress = ProgressCallback;

    LHA_HEADER hdr;
    int gh;
    while (!Abort && ((gh = LHAGetHeader(f, &hdr)) == GH_SUCCESS))
    {
        unpacked = FALSE;

        const char* name = SalamanderGeneral->SalPathFindFileName(hdr.name);
        char nameBuf[MAX_PATH];
        int nameLen = (int)strlen(name);
        if (nameLen > 0 && name[nameLen - 1] == '\\') // due to SalPathFindFileName the '\\' can only be at the end; remove it before calling AgreeMask
        {
            lstrcpyn(nameBuf, name, min(nameLen, MAX_PATH));
            name = nameBuf;
        }
        BOOL nameHasExt = strchr(name, '.') != NULL; // ".cvspass" is considered an extension on Windows

        int i;
        for (i = 0; i < masks.Count; i++)
        {
            if (SalamanderGeneral->AgreeMask(name, masks[i], nameHasExt))
            {
                BOOL bFile = (hdr.unix_mode & UNIX_FILE_TYPEMASK) == UNIX_FILE_REGULAR &&
                             hdr.method != LZHDIRS_METHOD_NUM;
                BOOL bDir = (hdr.unix_mode & UNIX_FILE_TYPEMASK) == UNIX_FILE_DIRECTORY ||
                            hdr.method == LZHDIRS_METHOD_NUM;
                if (!bFile && !bDir)
                    break; // symlink...?

                Salamander->ProgressSetTotalSize(CQuadWord(hdr.original_size, 0), CQuadWord(-1, -1));
                if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                {
                    Abort = TRUE;
                    Ret = FALSE;
                    break;
                }

                UnpackInnerBody(f, targetDir, fileName, hdr, bDir);

                if (!bDir)
                    Salamander->ProgressSetSize(CQuadWord(hdr.original_size, 0), CQuadWord(-1, -1), TRUE);
                break;
            }
        }

        if (!unpacked)
            fseek(f, hdr.packed_size, SEEK_CUR);
    }

    Salamander->CloseProgressDialog();
    pfLHAProgress = NULL;
    fclose(f);

    return Ret;
}

// UnpackInnerBody - called from UnpackWholeArchive and UnpackArchive

void CPluginInterfaceForArchiver::UnpackInnerBody(FILE* f, const char* targetDir, const char* fileName, LHA_HEADER& hdr, BOOL bDir)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackInnerBody( , %s, %s, , %ld)", targetDir, fileName, bDir);

    char message[MAX_PATH + 32];
    lstrcpy(message, LoadStr(IDS_UNPACKING));
    lstrcat(message, hdr.name);
    Salamander->ProgressDialogAddText(message, TRUE);

    if (hdr.method == LHA_UNKNOWNMETHOD && hdr.original_size)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_METHOD), LoadStr(IDS_PLUGINNAME), MSGBOX_WARNING);
        Salamander->ProgressDialogAddText(LoadStr(IDS_METHOD2), TRUE);
        Ret = FALSE;
        return;
    }

    char targetName[MAX_PATH];
    strncpy_s(targetName, targetDir, _TRUNCATE);
    if (!SalamanderGeneral->SalPathAppend(targetName, hdr.name + RootLen, MAX_PATH))
    {
        if (!(Silent & SF_LONGNAMES))
            switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                   hdr.name, LoadStr(IDS_TOOLONGNAME), NULL))
            {
            case DIALOG_SKIPALL:
                Silent |= SF_LONGNAMES;
            case DIALOG_SKIP:
                break;
            case DIALOG_CANCEL:
            case DIALOG_FAIL:
                Abort = TRUE;
                Ret = FALSE;
            }
        return;
    }

    char nameInArc[MAX_PATH + MAX_PATH];
    lstrcpy(nameInArc, fileName);
    SalamanderGeneral->SalPathAppend(nameInArc, hdr.name, MAX_PATH + MAX_PATH);
    char buf[100];
    GetInfo(buf, &hdr.last_modified_filetime, hdr.original_size);
    BOOL skip;
    HANDLE hOutFile = SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE, FILE_SHARE_READ,
                                                         hdr.attribute & ~FILE_ATTRIBUTE_READONLY | FILE_FLAG_SEQUENTIAL_SCAN,
                                                         bDir, SalamanderGeneral->GetMsgBoxParent(),
                                                         nameInArc, buf, &Silent, TRUE, &skip, NULL, 0, NULL, NULL);

    if (!bDir)
    {
        if (hOutFile == INVALID_HANDLE_VALUE)
        {
            if (!skip)
            {
                Abort = TRUE;
                Ret = FALSE;
            }
            return;
        }

        int crc = 0, uf = 1;
        if (hdr.original_size)
            uf = LHAUnpackFile(f, hOutFile, &hdr, &crc, targetName);
        SetFileTime(hOutFile, NULL, NULL, &hdr.last_modified_filetime);
        CloseHandle(hOutFile);

        BOOL bCanceled = !uf && iLHAErrorStrId == -1;
        BOOL bCRCError = hdr.has_crc && crc != hdr.crc;

        if (bCanceled || bCRCError)
        {
            DeleteFile(targetName);
            if (bCanceled)
                Abort = TRUE;
        }
        else
            SetFileAttributes(targetName, hdr.attribute);

        if (!bCanceled)
        {
            if (!uf)
            {
                if (iLHAErrorStrId != IDS_WRITEERROR) // because of SafeWriteFile
                    SalamanderGeneral->ShowMessageBox(LoadStr(iLHAErrorStrId), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                Abort = TRUE;
                Ret = FALSE;
            }
            else if (bCRCError)
            {
                if (!CRCSkipAll)
                    switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                           hdr.name + RootLen, LoadStr(IDS_CRCERROR), NULL))
                    {
                    case DIALOG_SKIPALL:
                        CRCSkipAll = TRUE;
                    case DIALOG_SKIP:
                        break;
                    case DIALOG_CANCEL:
                        Abort = TRUE;
                    }
                Ret = FALSE;
            }
        }
        unpacked = TRUE;
    }
}

//****************************************************************************
//
//  Helper functions
//

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

static int __cdecl compare_offsets(const void* elem1, const void* elem2)
{
    return *((int*)elem1) - *((int*)elem2);
}

BOOL CPluginInterfaceForArchiver::MakeFilesList(TDirectArray<int>& offsets, SalEnumSelection next, void* nextParam, const char* targetDir)
{
    CALL_STACK_MESSAGE2("CPluginInterfaceForArchiver::MakeFilesList(, , , %s)", targetDir);
    const char* nextName;
    BOOL isDir;
    CQuadWord size;
    char dir[MAX_PATH];
    char* addDir;
    int dirLen;
    const CFileData* pfd;
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
    while ((nextName = next(SalamanderGeneral->GetMsgBoxParent(), 1, &isDir, &size, &pfd, nextParam, &errorOccured)) != NULL)
    {
        if (isDir)
        {
            if (dirLen + lstrlen(nextName) + 1 >= MAX_PATH)
            {
                if (Silent & SF_LONGNAMES)
                    continue;
                switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL,
                                                       nextName, LoadStr(IDS_TOOLONGNAME), NULL))
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
                return FALSE;
        }
        else
        {
            offsets.Add((const int)pfd->PluginData);
            ProgressTotal += size;
        }
    }
    // test whether no error occurred and the user did not request to abort the operation (Cancel button)
    if (errorOccured == SALENUM_CANCEL)
        return FALSE;

    qsort(offsets.GetData(), offsets.Count, sizeof(int), compare_offsets);
    return SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(), targetDir, ProgressTotal, LoadStr(IDS_PLUGINNAME));
}

BOOL CPluginInterfaceForArchiver::ConstructMaskArray(TIndirectArray<char>& maskArray, const char* masks)
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
            maskArray.Add(newMask);
            if (!maskArray.IsGood())
            {
                maskArray.ResetState();
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
