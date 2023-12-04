// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"
#include "dbg.h"

#include "chmlib/types.h"
//#include "lzx.h"
#include "unchm.h"
#include "chmfile.h"

#include "unchm.rh"
#include "unchm.rh2"
#include "lang\lang.rh"

void GetInfo(char* buffer, FILETIME* lastWrite, CQuadWord size)
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

BOOL SafeWriteFile(HANDLE hFile, LPVOID lpBuffer, DWORD nBytesToWrite, DWORD* pnBytesWritten, char* fileName, HWND parent)
{
    while (!WriteFile(hFile, lpBuffer, nBytesToWrite, pnBytesWritten, NULL))
    {
        int lastErr = GetLastError();
        char error[1024];
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, lastErr,
                      MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), error, 1024, NULL);
        if (SalamanderGeneral->DialogError(parent == NULL ? SalamanderGeneral->GetMsgBoxParent() : parent, BUTTONS_RETRYCANCEL,
                                           fileName, error, LoadStr(IDS_WRITEERROR)) != DIALOG_RETRY)
            return FALSE;
    }
    return TRUE;
}

// ****************************************************************************
//
// CCHMFile
//

CCHMFile::CCHMFile()
{
    CALL_STACK_MESSAGE1("CCHMFile::CCHMFile()");

    CHM = NULL;

    ChmOpen = NULL;
    ChmClose = NULL;
    ChmEnumerate = NULL;
    ChmRetrieveObject = NULL;

    FileName = NULL;
}

CCHMFile::~CCHMFile()
{
    CALL_STACK_MESSAGE1("CCHMFile::~CCHMFile()");
    Close();

    delete[] FileName;
}

BOOL CCHMFile::Open(const char* fileName, BOOL quiet /* = FALSE*/)
{
    CALL_STACK_MESSAGE3("CCHMFile::Open(%s, %d)", fileName, quiet);

    char dllPath[MAX_PATH];
    if (!GetModuleFileName(DLLInstance, dllPath, MAX_PATH))
        return FALSE;
    lstrcpy(strrchr(dllPath, '\\') + 1, "chmlib.dll");

    HMODULE hDLL = LoadLibrary(dllPath);
    if (hDLL != NULL)
    {
        SalamanderDebug->AddModuleWithPossibleMemoryLeaks(dllPath);
        ChmOpen = (CHM_OPEN_PROC)GetProcAddress(hDLL, "chm_open");
        ChmClose = (CHM_CLOSE_PROC)GetProcAddress(hDLL, "chm_close");
        ChmEnumerate = (CHM_ENUMERATE_PROC)GetProcAddress(hDLL, "chm_enumerate");
        ChmRetrieveObject = (CHM_RETRIEVE_OBJECT_PROC)GetProcAddress(hDLL, "chm_retrieve_object");

        // get filetime (must go before chm_open)
        HANDLE hCHM = CreateFile(fileName, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
        if (hCHM != INVALID_HANDLE_VALUE)
        {
            GetFileTime(hCHM, NULL, NULL, &FileTime);
            CloseHandle(hCHM);
        }
        else
        {
            // can not obtain last write, use current time
            SYSTEMTIME st;
            GetLocalTime(&st);
            SystemTimeToFileTime(&st, &FileTime);
        }

        CHM = ChmOpen(fileName);
        if (CHM == NULL)
            return Error(IDS_CANT_OPEN_FILE, quiet);
        else
        {
            FileName = SalamanderGeneral->DupStr(fileName);
            return TRUE;
        }
    }
    else
        return FALSE;
}

BOOL CCHMFile::Close()
{
    CALL_STACK_MESSAGE1("CCHMFile::Close(, , ,)");

    if (CHM != NULL)
        ChmClose(CHM);

    return TRUE;
}

BOOL CCHMFile::AddFileDir(struct chmUnitInfo* ui, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE1("CCHMFile::AddFileDir(, , ,)");

    CFileData fd;

    char* path = ui->path;
    // convert '/' to '\'
    char* ch = path;
    while (*ch != '\0')
    {
        if (*ch == '/')
            *ch = '\\';
        ch++;
    }

    char* fileName = NULL;
    char* bs = strrchr(path, '\\');
    if (bs != NULL)
    {
        *bs = '\0';
        fileName = bs + 1;
    }
    else
    {
        fileName = ui->path;
    }

    // ignore empty files
    if (strlen(fileName) == 0)
        return TRUE;

    if (path[0] == '\\' && path <= bs)
        path++;

    fd.Name = SalamanderGeneral->DupStr(fileName);
    if (fd.Name == NULL)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    } // if

    fd.NameLen = strlen(fd.Name);
    char* s = strrchr(fd.Name, '.');
    if (s != NULL)
        fd.Ext = s + 1; // ".cvspass" ve Windows je pripona
    else
        fd.Ext = fd.Name + fd.NameLen;

    fd.LastWrite = FileTime;

    struct chmUnitInfo* data = new chmUnitInfo;
    if (data == NULL)
    {
        free(fd.Name);
        Error(IDS_INSUFFICIENT_MEMORY);
        return FALSE;
    } // if

    *data = *ui;
    fd.PluginData = (DWORD_PTR)data;

    fd.DosName = NULL;

    fd.Attr = FILE_ATTRIBUTE_READONLY; // vse je defaultne read-only
    fd.Hidden = 0;

    fd.Size.SetUI64(ui->length);

    // soubor
    fd.IsLink = SalamanderGeneral->IsFileLink(fd.Ext);
    fd.IsOffline = 0;
    if (dir && !dir->AddFile(path, fd, pluginData))
    {
        free(fd.Name);
        dir->Clear(pluginData);
        return Error(IDS_ERROR);
    }

    return TRUE;
}

struct SEnumObjHelper
{
    CCHMFile* File;
    CSalamanderDirectoryAbstract* Dir;
    CPluginDataInterfaceAbstract*& PluginData;

    SEnumObjHelper(CCHMFile* file, CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData) : PluginData(pluginData)
    {
        File = file;
        Dir = dir;
    }
};

int ChmEnumObjectsCallBack(struct chmFile* CHM, struct chmUnitInfo* ui, void* context)
{
    SEnumObjHelper* helper = (SEnumObjHelper*)context;
    if (helper->File->AddFileDir(ui, helper->Dir, helper->PluginData))
    {
        return CHM_ENUMERATOR_CONTINUE;
    }
    else
        return CHM_ENUMERATOR_FAILURE;
}

BOOL CCHMFile::EnumObjects(CSalamanderDirectoryAbstract* dir, CPluginDataInterfaceAbstract*& pluginData)
{
    CALL_STACK_MESSAGE1("CCHMFile::EnumObjects( , ,)");

    SEnumObjHelper* helper = new SEnumObjHelper(this, dir, pluginData);
    ChmEnumerate(CHM, CHM_ENUMERATE_NORMAL, ChmEnumObjectsCallBack, (void*)helper);
    // for DEBUG purposes
    //  ChmEnumerate(CHM, CHM_ENUMERATE_ALL, ChmEnumObjectsCallBack, (void *) helper);
    delete helper;

    return TRUE;
}

int CCHMFile::ExtractObject(CSalamanderForOperationsAbstract* salamander, const char* srcPath, const char* path,
                            const CFileData* fileData, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE5("CCHMFile::ExtractObject( , %s, %s, , %u, %d)", srcPath, path, silent, toSkip);

    char nameInArc[MAX_PATH + MAX_PATH];
    strcpy(nameInArc, FileName);
    SalamanderGeneral->SalPathAppend(nameInArc, srcPath, MAX_PATH + MAX_PATH);
    SalamanderGeneral->SalPathAppend(nameInArc, fileData->Name, MAX_PATH + MAX_PATH);

    ///
    char name[MAX_PATH];
    strcpy(name, path);
    if (!SalamanderGeneral->SalPathAppend(name, fileData->Name, MAX_PATH))
        return UNPACK_ERROR;

    char fileInfo[100];
    FILETIME ft = fileData->LastWrite;
    GetInfo(fileInfo, &ft, fileData->Size);

    DWORD attrs = fileData->Attr;

    HANDLE file = SalamanderSafeFile->SafeFileCreate(name, GENERIC_WRITE, FILE_SHARE_READ, attrs, FALSE,
                                                     SalamanderGeneral->GetMainWindowHWND(), nameInArc, fileInfo,
                                                     &silent, TRUE, &toSkip, NULL, 0, NULL, NULL);

    // set file time
    SetFileTime(file, &ft, &ft, &ft);

    // celkova operace muze pokracovat dal. pouze skip
    if (toSkip)
        return UNPACK_ERROR;

    // celkova operace nemuze pokracovat dal. cancel
    if (file == INVALID_HANDLE_VALUE)
        return UNPACK_CANCEL;

    BOOL whole = TRUE;

    BOOL ret = UNPACK_OK;
    chmUnitInfo* ui = (chmUnitInfo*)fileData->PluginData;
    CQuadWord remain = fileData->Size;
    DWORD bufferSize = 8192;
    BYTE* buffer = new BYTE[bufferSize];
    if (!buffer)
    {
        Error(IDS_INSUFFICIENT_MEMORY);
        return UNPACK_CANCEL;
    }

    LONGUINT64 offset = 0;
    while (remain.Value > 0)
    {
        LONGINT64 len = ChmRetrieveObject(CHM, ui, buffer, offset, bufferSize);
        if (len > 0)
        {
            ULONG written;
            SafeWriteFile(file, buffer, (DWORD)len, &written, name, SalamanderGeneral->GetMainWindowHWND());
            offset += len;
            remain.Value -= len;
        }
        else
        {
            if (silent == 0)
            {
                char error[1024];
                sprintf(error, LoadStr(IDS_ERROR_UNPACKING));
                int userAction = SalamanderGeneral->DialogError(SalamanderGeneral->GetMsgBoxParent(), ButtonFlags,
                                                                fileData->Name, error, LoadStr(IDS_READERROR));

                switch (userAction)
                {
                case DIALOG_CANCEL:
                    ret = UNPACK_CANCEL;
                    break;
                case DIALOG_SKIP:
                    ret = UNPACK_ERROR;
                    break;
                case DIALOG_SKIPALL:
                    ret = UNPACK_ERROR;
                    silent = 1;
                    break;
                }
            }

            whole = FALSE;
            break;
        }

        if (!salamander->ProgressAddSize((int)len, TRUE)) // delayedPaint==TRUE, abychom nebrzdili
        {
            salamander->ProgressDialogAddText(LoadStr(IDS_CANCELOPER), FALSE);
            salamander->ProgressEnableCancel(FALSE);

            ret = UNPACK_CANCEL;
            whole = FALSE;
            break; // preruseni akce
        }
    } // while

    delete[] buffer;

    CloseHandle(file);

    if (!whole)
    {
        if (ret == UNPACK_OK)
            ret = UNPACK_CANCEL;

        // protoze je vytvoren s read-only atributem, musime R attribut
        // shodit, aby sel soubor smazat
        attrs &= ~FILE_ATTRIBUTE_READONLY;
        if (!SetFileAttributes(name, attrs))
            Error(LoadStr(IDS_CANT_SET_ATTRS), GetLastError());

        // user zrusil operaci
        // smazat po sobe neuplny soubor
        if (!DeleteFile(name))
            Error(LoadStr(IDS_CANT_DELETE_TEMP_FILE), GetLastError());
    }

    return ret;
    //  return UNPACK_CANCEL;
}

BOOL CCHMFile::UnpackDir(const char* dirName, const CFileData* fileData)
{
    CALL_STACK_MESSAGE3("CCHMFile::UnpackDir(%s, %p)", dirName, fileData);

    if (!SalamanderGeneral->CheckAndCreateDirectory(dirName))
        return UNPACK_ERROR;

    /*
  DWORD attrs = fileData->Attr;

  // set attrs to dir
  if (Options.ClearReadOnly)
    // set ReadOnly Attribute
    attrs &= ~FILE_ATTRIBUTE_READONLY;

  if (!SetFileAttributes(dirName, attrs))
    Error(LoadStr(IDS_CANT_SET_ATTRS), GetLastError());
*/

    return UNPACK_OK;
}

int CCHMFile::ExtractAllObjects(CSalamanderForOperationsAbstract* salamander, char* srcPath,
                                CSalamanderDirectoryAbstract const* dir, const char* mask,
                                char* path, int pathBufSize, DWORD& silent, BOOL& toSkip)
{
    CALL_STACK_MESSAGE7("CCHMFile::ExtractAllObjects(, %s, , %s, %s, %d, %u, %d)", srcPath, mask, path, pathBufSize, silent, toSkip);

    int count = dir->GetFilesCount();
    int i;
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetFile(i);
        salamander->ProgressDialogAddText(file->Name, TRUE); // delayedPaint==TRUE, abychom nebrzdili

        salamander->ProgressSetSize(CQuadWord(0, 0), CQuadWord(-1, -1), TRUE);
        salamander->ProgressSetTotalSize(file->Size + CQuadWord(1, 0), CQuadWord(-1, -1));

        if (SalamanderGeneral->AgreeMask(file->Name, mask, file->Ext[0] != 0))
        {
            if (ExtractObject(salamander, srcPath, path, file, silent, toSkip) == UNPACK_CANCEL ||
                !salamander->ProgressAddSize(1, TRUE))
                return UNPACK_CANCEL;
        }
    } // for

    count = dir->GetDirsCount();
    int pathLen = (int)strlen(path);
    int srcPathLen = (int)strlen(srcPath);
    for (i = 0; i < count; i++)
    {
        CFileData const* file = dir->GetDir(i);
        SalamanderGeneral->SalPathAppend(path, file->Name, pathBufSize);
        if (UnpackDir(path, file) == UNPACK_CANCEL)
            return UNPACK_CANCEL;

        CSalamanderDirectoryAbstract const* subDir = dir->GetSalDir(i);
        SalamanderGeneral->SalPathAppend(srcPath, file->Name, MAX_PATH);
        if (ExtractAllObjects(salamander, srcPath, subDir, mask, path, pathBufSize, silent, toSkip) == UNPACK_CANCEL)
            return UNPACK_CANCEL;

        srcPath[srcPathLen] = '\0';
        path[pathLen] = '\0';
    }

    return UNPACK_OK;
}
