// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

#include "dumpmem.h"
#include "array2.h"

#include "..\dll\pakiface.h"
#include "pak.rh"
#include "pak.rh2"
#include "lang\lang.rh"
#include "pak.h"

// ****************************************************************************

HINSTANCE DLLInstance = NULL; // handle k SPL-ku - jazykove nezavisle resourcy
HINSTANCE HLanguage = NULL;   // handle k SLG-cku - jazykove zavisle resourcy
//HINSTANCE PakLibDLL = NULL;

/*
FPAKGetIFace PAKGetIFace;
FPAKReleaseIFace PAKReleaseIFace;
*/

// objekt interfacu pluginu, jeho metody se volaji ze Salamandera
CPluginInterface PluginInterface;
// dalsi casti interfacu CPluginInterface
CPluginInterfaceForArchiver InterfaceForArchiver;
CPluginInterfaceForMenuExt InterfaceForMenuExt;

// obecne rozhrani Salamandera - platne od startu az do ukonceni pluginu
CSalamanderGeneralAbstract* SalamanderGeneral = NULL;
// interface pro komfortni praci se soubory
CSalamanderSafeFileAbstract* SalamanderSafeFile = NULL;

// definice promenne pro "dbg.h"
CSalamanderDebugAbstract* SalamanderDebug = NULL;

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
                   "PAK" /* neprekladat! */, MB_OK | MB_ICONERROR);
        return NULL;
    }

    // nechame nacist jazykovy modul (.slg)
    HLanguage = salamander->LoadLanguageModule(salamander->GetParentWindow(), "PAK" /* neprekladat! */);
    if (HLanguage == NULL)
        return NULL;

    // ziskame obecne rozhrani Salamandera
    SalamanderGeneral = salamander->GetSalamanderGeneral();
    SalamanderSafeFile = salamander->GetSalamanderSafeFile();

    // nastavime jmeno souboru s helpem
    SalamanderGeneral->SetHelpFileName("pak.chm");

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

    /*
  char buf[1024];
  BOOL ok = FALSE;
  if (GetModuleFileName(DLLInstance, buf, 1024))
  {
    PathRemoveFileSpec(buf);
    PathAppend(buf, "paklib.dll");
    PakLibDLL = LoadLibrary(buf);
    if (PakLibDLL)
    {
      if ((PAKGetIFace = (FPAKGetIFace) GetProcAddress(PakLibDLL, "PAKGetIFace")) &&
          (PAKReleaseIFace = (FPAKReleaseIFace) GetProcAddress(PakLibDLL, "PAKReleaseIFace")))
      {
        ok = TRUE;
      }
    }
  }
  if (!ok)
  {
    lstrcpy(buf, LoadStr(IDS_ERRLOADPAKLIB));
    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
    MessageBox(salamander->GetParentWindow(), buf, LoadStr(IDS_PLUGINNAME), MB_OK | MB_ICONERROR);
    return NULL;
  }
*/

    // nastavime zakladni informace o pluginu
    salamander->SetBasicPluginData(LoadStr(IDS_PLUGINNAME),
                                   FUNCTION_PANELARCHIVERVIEW | FUNCTION_PANELARCHIVEREDIT |
                                       FUNCTION_CUSTOMARCHIVERPACK | FUNCTION_CUSTOMARCHIVERUNPACK,
                                   VERSINFO_VERSION_NO_PLATFORM,
                                   VERSINFO_COPYRIGHT,
                                   LoadStr(IDS_PLUGIN_DESCRIPTION),
                                   NULL, "pak");

    // nastavime URL home-page pluginu
    salamander->SetPluginHomePageURL("www.altap.cz");

    return &PluginInterface;
}

// ****************************************************************************
//
// CFileInfo
//

CFileInfo::CFileInfo(const char* name, DWORD status, DWORD dirDepth, DWORD size)
{
    CALL_STACK_MESSAGE_NONE
    if ((Name = (char*)malloc(lstrlen(name) + 1)) != NULL)
        lstrcpy(Name, name);
    Status = status;
    DirDepth = dirDepth;
    Size = size;
}

CFileInfo::~CFileInfo()
{
    CALL_STACK_MESSAGE_NONE
    if (Name)
        free(Name);
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
    return TRUE;
}

void CPluginInterface::Connect(HWND parent, CSalamanderConnectAbstract* salamander)
{
    CALL_STACK_MESSAGE1("CPluginInterface::Connect(,)");

    salamander->AddCustomPacker("PAK (Plugin)", "pak", FALSE);
    salamander->AddCustomUnpacker("PAK (Plugin)", "*.pak", FALSE);
    salamander->AddPanelArchiver("pak", TRUE, FALSE);

    // j.r. Tahle polozka zbytecne strasi v menu, uz to pravdepodobne nikdo nepouziva.
    // Zkusebne ji vyhodim a uvidime zda se nekdo ozve.
    /*
  
///* slouzi pro skript export_mnu.py, ktery generuje salmenu.mnu pro Translator
//   udrzovat synchronizovane s volani salamander->AddMenuItem() dole...
//MENU_TEMPLATE_ITEM PluginMenu[] = 
//{
//  {MNTT_PB, 0
//  {MNTT_IT, IDS_MENUOPTIMIZE
//  {MNTT_PE, 0
//};
//* /
  
  salamander->AddMenuItem(-1, LoadStr(IDS_MENUOPTIMIZE), 0, OPTIMIZE_MENUID, TRUE, 0, 0,
                          MENU_SKILLLEVEL_INTERMEDIATE | MENU_SKILLLEVEL_ADVANCED);
  // nastavime ikonku pluginu
  HBITMAP hBmp = (HBITMAP)LoadImage(DLLInstance, MAKEINTRESOURCE(IDB_PAK),
                                    IMAGE_BITMAP, 16, 16, LR_DEFAULTCOLOR);
  salamander->SetBitmapWithIcons(hBmp);
  DeleteObject(hBmp);
  salamander->SetPluginIcon(0);
  salamander->SetPluginMenuAndToolbarIcon(0);
*/
}

CPluginInterfaceForArchiverAbstract*
CPluginInterface::GetInterfaceForArchiver()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForArchiver;
}

CPluginInterfaceForMenuExtAbstract*
CPluginInterface::GetInterfaceForMenuExt()
{
    CALL_STACK_MESSAGE_NONE
    return &InterfaceForMenuExt;
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
    BOOL ret = TRUE;

    PakIFace = PAKGetIFace();
    if (!PakIFace)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    Salamander = salamander;
    PakFileName = fileName;

    CPakCallbacks pakCalls(this);
    PakIFace->Init(&pakCalls);

    if (!PakIFace->OpenPak(fileName, OP_READ_MODE))
        ret = FALSE;
    else
    {
        char file[PAK_MAXPATH];
        DWORD size;
        if (!PakIFace->GetFirstFile(file, &size))
            ret = FALSE;
        else
        {
            CFileData fileData;
            char* slash;
            const char* path = file;
            char* name = file;
            while (*file)
            {
                path = file;
                name = file;
                slash = strrchr(file, '\\');
                if (slash)
                {
                    *slash++ = 0;
                    name = slash;
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
                fileData.Size = CQuadWord(size, 0);
                fileData.Attr = FILE_ATTRIBUTE_NORMAL;
                fileData.Hidden = 0;
                fileData.PluginData = -1; // zbytecne, jen tak pro formu
                PakIFace->GetPakTime(&fileData.LastWrite);
                fileData.DosName = NULL;
                fileData.NameLen = lstrlen(fileData.Name);
                fileData.IsLink = SalamanderGeneral->IsFileLink(fileData.Ext);
                fileData.IsOffline = 0;
                if (!dir->AddFile(path, fileData, NULL))
                {
                    free(fileData.Name);
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_ERRLIST), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    break;
                }
                if (!PakIFace->GetNextFile(file, &size))
                {
                    ret = FALSE;
                    break;
                }
            }
        }
        PakIFace->ClosePak();
    }

    PAKReleaseIFace(PakIFace);

    return ret;
}

/*
void FreeFileInfo(void * file)
{
  free(((CFileInfo*)file)->Name);
  free(file);
}
*/

BOOL CPluginInterfaceForArchiver::MakeFileList(TIndirectArray2<CFileInfo>& files, const char* archiveRoot,
                                               SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE2("CPluginInterface::MakeFileList(, %s, , )", archiveRoot);
    const char* nextName;
    char nextFull[PAK_MAXPATH];
    int len;
    BOOL isDir;
    char file[PAK_MAXPATH];
    DWORD size;

    ProgressTotal = CQuadWord(0, 0);

    while ((nextName = next(NULL, 0, &isDir, NULL, NULL, nextParam, NULL)) != NULL)
    {
        lstrcpy(nextFull, archiveRoot);
        SalamanderGeneral->SalPathAppend(nextFull, nextName, PAK_MAXPATH);
        len = lstrlen(nextFull);
        if (!PakIFace->GetFirstFile(file, &size))
            return FALSE;
        while (*file)
        {
            if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, nextFull, len, file, len) == CSTR_EQUAL &&
                (file[len] == 0 || file[len] == '\\'))
            {
                CFileInfo* f = new CFileInfo(file, 0, 0, 0);
                if (!f || !f->Name)
                {
                    if (f)
                        delete f;
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    return FALSE;
                }
                if (!files.Add(f))
                {
                    delete f;
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    return FALSE;
                }
                ProgressTotal += CQuadWord(size, 0);
                if (!isDir)
                    break;
            }
            if (!PakIFace->GetNextFile(file, &size))
                return FALSE;
        }
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackFiles(TIndirectArray2<CFileInfo>& files, const char* arcFile,
                                              int rootLen, const char* targetDir)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackFiles(, %s, %d, %s)", arcFile,
                        rootLen, targetDir);
    char file[PAK_MAXPATH];
    DWORD size;
    char message[PAK_MAXPATH + 32];
    CFileInfo* info;
    CQuadWord currentProgress(0, 0);
    unsigned left = files.Count;
    CQuadWord q;

    PakIFace->GetFirstFile(file, &size);
    while (*file && left)
    {
        int i;
        for (i = 0; i < files.Count; i++)
        {
            info = files[i];
            if (info->Status == STATUS_OK)
                continue;
            if (CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, info->Name, -1, file, -1) == CSTR_EQUAL)
            {
                lstrcpy(message, LoadStr(IDS_EXTRACTING));
                lstrcat(message, file);
                Salamander->ProgressDialogAddText(message, TRUE);
                if (lstrlen(targetDir) + lstrlen(file) - rootLen >= MAX_PATH)
                {
                    if (Silent & SF_LONGNAMES)
                        goto l_next;
                    switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_SKIPCANCEL, file, LoadStr(IDS_TOOLONGNAME), NULL))
                    {
                    case DIALOG_SKIPALL:
                        Silent |= SF_LONGNAMES;
                    case DIALOG_SKIP:
                        goto l_next;
                    case DIALOG_CANCEL:
                    case DIALOG_FAIL:
                        return FALSE;
                    }
                }
                char targetName[MAX_PATH];
                lstrcpy(targetName, targetDir);
                SalamanderGeneral->SalPathAppend(targetName, file + rootLen, MAX_PATH);
                IOFileName = targetName;
                char arcName[MAX_PATH + PAK_MAXPATH];
                lstrcpy(arcName, arcFile);
                SalamanderGeneral->SalPathAppend(arcName, file, MAX_PATH + PAK_MAXPATH);
                FILETIME ft;
                PakIFace->GetPakTime(&ft);
                char buf[100];
                GetInfo(buf, &ft, size);
                BOOL skip;
                q = CQuadWord(size, 0);
                bool allocate;
                allocate = CQuadWord(2, 0) < q && q < CQuadWord(0, 0x80000000);
                q += CQuadWord(0, 0x80000000);
                IOFile = SalamanderSafeFile->SafeFileCreate(targetName, GENERIC_WRITE,
                                                            FILE_SHARE_READ, FILE_ATTRIBUTE_NORMAL, FALSE,
                                                            SalamanderGeneral->GetMsgBoxParent(), arcName, buf, &Silent, TRUE,
                                                            &skip, NULL, 0, allocate ? &q : NULL, NULL);
                if (skip)
                    goto l_next;
                if (IOFile == INVALID_HANDLE_VALUE)
                    return FALSE;
                BOOL ret;
                Abort = TRUE;
                if (!Salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE))
                {
                    CloseHandle(IOFile);
                    return FALSE;
                }
                Salamander->ProgressSetTotalSize(CQuadWord(size, 0), ProgressTotal);
                ret = PakIFace->ExtractFile();
                SetFileTime(IOFile, NULL, NULL, &ft);
                CloseHandle(IOFile);
                if (!ret)
                {
                    DeleteFile(targetName);
                    if (Abort)
                        return FALSE;
                }

            l_next:
                currentProgress += CQuadWord(size, 0);
                if (!Salamander->ProgressSetSize(CQuadWord(size, 0), currentProgress, TRUE))
                    return FALSE;
                left--;
                break;
            }
        }
        PakIFace->GetNextFile(file, &size);
    }
    return TRUE;
}

BOOL CPluginInterfaceForArchiver::UnpackArchive(CSalamanderForOperationsAbstract* salamander, const char* fileName,
                                                CPluginDataInterfaceAbstract* pluginData, const char* targetDir,
                                                const char* archiveRoot, SalEnumSelection next, void* nextParam)
{
    CALL_STACK_MESSAGE4("CPluginInterfaceForArchiver::UnpackArchive(, %s, , %s, %s, ,)", fileName, targetDir, archiveRoot);

    PakIFace = PAKGetIFace();
    if (!PakIFace)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    BOOL ret = TRUE;
    Salamander = salamander;
    PakFileName = fileName;
    Silent = 0;
    Abort = FALSE;

    CPakCallbacks pakCalls(this);
    PakIFace->Init(&pakCalls);
    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(PakFileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    if (!PakIFace->OpenPak(fileName, OP_READ_MODE))
        ret = FALSE;
    else
    {
        TIndirectArray2<CFileInfo> files(256);
        const char* arcRoot = archiveRoot ? archiveRoot : "";
        if (*arcRoot == '\\')
            arcRoot++;
        if (!MakeFileList(files, arcRoot, next, nextParam))
            ret = FALSE;
        {
            if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                                  targetDir, ProgressTotal, LoadStr(IDS_PLUGINNAME)))
                ret = FALSE;
            else
            {
                Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
                if (!UnpackFiles(files, fileName, lstrlen(arcRoot), targetDir))
                    ret = FALSE;
            }
        }
        PakIFace->ClosePak();
    }

    Salamander->CloseProgressDialog();

    PAKReleaseIFace(PakIFace);

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

    PakIFace = PAKGetIFace();
    if (!PakIFace)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    BOOL ret = FALSE;
    Salamander = salamander;
    PakFileName = fileName;
    Silent = 0;
    Abort = FALSE;

    CPakCallbacks pakCalls(this);
    PakIFace->Init(&pakCalls);

    if (PakIFace->OpenPak(fileName, OP_READ_MODE))
    {
        DWORD size;
        if (PakIFace->FindFile(nameInArchive, &size) && size != -1)
        {
            char targetName[MAX_PATH];
            const char* name = strrchr(nameInArchive, '\\');
            if (name)
                name++;
            else
                name = nameInArchive;
            if (lstrlen(targetDir) + lstrlen(name) + 1 >= MAX_PATH)
            {
                SalamanderGeneral->ShowMessageBox(LoadStr(IDS_TOOLONGNAME), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
            }
            else
            {
                lstrcpy(targetName, targetDir);
                SalamanderGeneral->SalPathAppend(targetName, name, MAX_PATH);
                IOFileName = targetName;
                IOFile = CreateFile(targetName, GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS,
                                    FILE_ATTRIBUTE_NORMAL, NULL);
                if (IOFile == INVALID_HANDLE_VALUE)
                {
                    char buf[1024];

                    lstrcpy(buf, LoadStr(IDS_ERROPEN));
                    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                                  GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
                    SalamanderGeneral->ShowMessageBox(buf, LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                }
                else
                {
                    BOOL r = PakIFace->ExtractFile();
                    FILETIME ft;
                    PakIFace->GetPakTime(&ft);
                    SetFileTime(IOFile, NULL, NULL, &ft);
                    CloseHandle(IOFile);
                    if (r)
                        ret = TRUE;
                    else
                        DeleteFile(targetName);
                }
            }
        }
        PakIFace->ClosePak();
    }

    PAKReleaseIFace(PakIFace);
    return ret;
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

void FreeString(void* string)
{
    CALL_STACK_MESSAGE_NONE
    free(string);
}

BOOL CPluginInterfaceForArchiver::MakeFileList2(TIndirectArray2<char>& masks, TIndirectArray2<CFileInfo>& files)
{
    CALL_STACK_MESSAGE1("CPluginInterface::MakeFileList2(, )");
    BOOL ret = TRUE;
    ProgressTotal = CQuadWord(0, 0);
    char file[PAK_MAXPATH];
    DWORD size;

    if (!PakIFace->GetFirstFile(file, &size))
        return FALSE;

    while (*file)
    {
        const char* fileName = SalamanderGeneral->SalPathFindFileName(file);
        BOOL fileNameHasExt = strchr(fileName, '.') != NULL; // ".cvspass" ve Windows je pripona
        int i;
        for (i = 0; i < masks.Count; i++)
        {
            if (SalamanderGeneral->AgreeMask(fileName, masks[i], fileNameHasExt))
            {
                CFileInfo* f = new CFileInfo(file, 0, 0, 0);
                if (!f || !f->Name)
                {
                    if (f)
                        delete f;
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    return FALSE;
                }
                if (!files.Add(f))
                {
                    delete f;
                    SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
                    return FALSE;
                }
                ProgressTotal += CQuadWord(size, 0);
                break;
            }
        }
        if (!PakIFace->GetNextFile(file, &size))
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

    TIndirectArray2<char> masks(16);

    Salamander = salamander;

    if (!ConstructMaskArray(masks, mask) || masks.Count == 0)
        return FALSE;

    PakIFace = PAKGetIFace();
    if (!PakIFace)
    {
        SalamanderGeneral->ShowMessageBox(LoadStr(IDS_LOWMEM), LoadStr(IDS_PLUGINNAME), MSGBOX_ERROR);
        return FALSE;
    }

    BOOL ret = TRUE;
    if (delArchiveWhenDone)
        archiveVolumes->Add(fileName, -2);
    PakFileName = fileName;
    Silent = 0;
    Abort = FALSE;

    CPakCallbacks pakCalls(this);
    PakIFace->Init(&pakCalls);
    char title[1024];
    sprintf(title, LoadStr(IDS_EXTRPROGTITLE), SalamanderGeneral->SalPathFindFileName(PakFileName));
    Salamander->OpenProgressDialog(title, TRUE, NULL, FALSE);
    Salamander->ProgressDialogAddText(LoadStr(IDS_PREPAREDATA), FALSE);

    if (!PakIFace->OpenPak(fileName, OP_READ_MODE))
        ret = FALSE;
    else
    {
        TIndirectArray2<CFileInfo> files(256);
        if (!MakeFileList2(masks, files))
            ret = FALSE;
        {
            if (!SalamanderGeneral->TestFreeSpace(SalamanderGeneral->GetMsgBoxParent(),
                                                  targetDir, ProgressTotal, LoadStr(IDS_PLUGINNAME)))
                ret = FALSE;
            else
            {
                Salamander->ProgressDialogAddText(LoadStr(IDS_EXTRACTFILES), FALSE);
                if (!UnpackFiles(files, fileName, 0, targetDir))
                    ret = FALSE;
            }
        }
        PakIFace->ClosePak();
    }

    Salamander->CloseProgressDialog();

    PAKReleaseIFace(PakIFace);
    return ret;

    return TRUE;
}

/*
void CPluginInterfaceForArchiver::InitPlugin(CSalamanderForOperationsAbstract *salamander, CPakIfaceAbstract * pakIFace,
                const char * pakFileName)
{
  Salamander = salamander;
  PakIFace = pakIFace;
  PakFileName = pakFileName;
}
*/
/*
HWND
CPluginInterfaceForArchiver::GetParentWindow()
{
  HWND parent = Salamander->ProgressGetHWND();
  if (parent) return parent;
  return SalamanderGeneral->GetMainWindowHWND();
}
*/

// ****************************************************************************
//
// CPakCallbacks
//

CPakCallbacks::CPakCallbacks(CPluginInterfaceForArchiver* plugin)
{
    CALL_STACK_MESSAGE1("CPakCallbacks::CPakCallbacks()");
    Plugin = plugin;
    UserBreak = FALSE;
}

BOOL CPakCallbacks::HandleError(DWORD flags, int errorID, va_list arglist)
{
    CALL_STACK_MESSAGE3("CPakCallbacks::HandleError(0x%X, %d, )", flags, errorID);
    char buffer[1024];
    Plugin->PakIFace->FormatMessage(buffer, errorID, arglist);
    if (flags & HE_RETRY)
    {
        if (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYCANCEL, Plugin->PakFileName, buffer, NULL) == DIALOG_RETRY)
            return TRUE;
    }
    else
    {
        SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_OK, Plugin->PakFileName, buffer, NULL);
    }
    return FALSE;
}

BOOL CPakCallbacks::SafeSeek(DWORD position)
{
    CALL_STACK_MESSAGE2("CPakCallbacks::SafeSeek(0x%X)", position);
    char buf[1024];
    while (1)
    {
        if (SetFilePointer(Plugin->IOFile, position, NULL, FILE_BEGIN) != 0xFFFFFFFF)
            return TRUE;
        lstrcpy(buf, LoadStr(IDS_UNABLESEEK));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
        if (Plugin->Silent & SF_IOERRORS)
        {
            Plugin->Abort = FALSE;
            return FALSE;
        }
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, Plugin->IOFileName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Plugin->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            Plugin->Abort = FALSE;
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            return FALSE;
        }
    }
}

BOOL CPakCallbacks::Write(void* buffer, DWORD size)
{
    CALL_STACK_MESSAGE2("CPakCallbacks::Write(, 0x%X)", size);
    if (size == 0)
        return TRUE;
    char buf[1024];
    DWORD pos;
    while (1)
    {
        pos = SetFilePointer(Plugin->IOFile, 0, NULL, FILE_CURRENT);
        if (pos != 0xFFFFFFFF)
            break;
        lstrcpy(buf, LoadStr(IDS_UNABLEGETFIELPOS));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
        if (Plugin->Silent & SF_IOERRORS)
        {
            Plugin->Abort = FALSE;
            return FALSE;
        }
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, Plugin->IOFileName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Plugin->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            Plugin->Abort = FALSE;
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            return FALSE;
        }
    }
    DWORD written;
    while (1)
    {
        if (WriteFile(Plugin->IOFile, buffer, size, &written, NULL))
            return TRUE;
        lstrcpy(buf, LoadStr(IDS_UNABLEWRITE));
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL,
                      GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf + lstrlen(buf), 1024 - lstrlen(buf), NULL);
        if (Plugin->Silent & SF_IOERRORS)
        {
            Plugin->Abort = FALSE;
            return FALSE;
        }
        switch (SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), BUTTONS_RETRYSKIPCANCEL, Plugin->IOFileName, buf, NULL))
        {
        case DIALOG_SKIPALL:
            Plugin->Silent |= SF_IOERRORS;
        case DIALOG_SKIP:
            Plugin->Abort = FALSE;
            return FALSE;
        case DIALOG_CANCEL:
        case DIALOG_FAIL:
            return FALSE;
        }
        if (!SafeSeek(pos))
            return FALSE;
    }
    return TRUE;
}

BOOL CPakCallbacks::AddProgress(unsigned size)
{
    CALL_STACK_MESSAGE2("CPakCallbacks::AddProgress(0x%X)", size);
    BOOL ret;
    ret = Plugin->Salamander->ProgressAddSize(size, TRUE);
    if (!ret)
    {
        if (!UserBreak)
            Plugin->Salamander->ProgressDialogAddText(LoadStr(IDS_CANCELING), FALSE);
        UserBreak = TRUE;
        Plugin->Salamander->ProgressEnableCancel(FALSE);
    }
    return ret;
}

// ****************************************************************************
//
// Other functions
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
