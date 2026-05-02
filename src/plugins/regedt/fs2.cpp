// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CPluginFSInterface
//
//

CPluginFSInterface::CPluginFSInterface()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::CPluginFSInterface()");
    FocusFirstNewItem = TRUE;

    PathError = FALSE;
    *NewPath = L'\0';
    NewPathValid = FALSE;

    WCHAR* keyName;
    if (ParseFullPath(RecentFullPath, keyName, CurrentKeyRoot))
        wcscpy(CurrentKeyName, keyName);
    else
        CurrentKeyRoot = -1;

    FirstChangePath = TRUE;
}

CPluginFSInterface::~CPluginFSInterface()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::~CPluginFSInterface()");
    // stop monitoring for this FS instance
    ChangeMonitor.Cancel(this);
}

BOOL CPluginFSInterface::SetNewPath(WCHAR* newPath)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::SetNewPath()");
    if (wcslen(newPath) >= MAX_KEYNAME)
        return Error(IDS_LONGNAME);
    wcscpy(NewPath, newPath);
    NewPathValid = TRUE;
    return TRUE;
}

BOOL CPluginFSInterface::GetCurrentPathW(WCHAR* userPart, int size)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::GetCurrentPathW(, %d)", size);
    // !!! report an error if the buffer is too small
    if (CurrentKeyRoot == -1)
    {
        lstrcpynW(userPart, L"\\", size);
        return TRUE;
    }
    else
    {
        lstrcpynW(userPart, L"\\", size);
        lstrcpynW(userPart + 1, PredefinedHKeys[CurrentKeyRoot].KeyName, size - 1);
        return PathAppend(userPart, CurrentKeyName, size) || Error(IDS_LONGNAME);
    }
}

BOOL CPluginFSInterface::GetCurrentPath(char* userPart)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetCurrentPath()");
    WCHAR buf[MAX_PATH];

    if (!GetCurrentPathW(buf, MAX_PATH))
        return FALSE;

    if (WStrToStr(userPart, MAX_PATH, buf) <= 0)
        return Error(IDS_LONGNAME);

    return TRUE;
}

BOOL CPluginFSInterface::GetFullName(CFileData& file, int isDir, char* buf, int bufSize)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetFullName(, %d, , %d)", isDir,
                        bufSize);
    CPluginData* pluginData = (CPluginData*)file.PluginData;
    WCHAR buffer[MAX_FULL_KEYNAME];

    if (!GetCurrentPathW(buffer, min(MAX_FULL_KEYNAME, bufSize)))
        return FALSE;

    if (isDir == 2) // up-dir
    {
        if (!CutDirectory(buffer, NULL, 0) ||
            WStrToStr(buf, bufSize, buffer) <= 0)
            return Error(IDS_LONGNAME);
    }
    else
    {
        if (!PathAppend(buffer, pluginData->Name != NULL && *pluginData->Name != 0 ? pluginData->Name : LoadStrW(IDS_DEFAULTVALUE), MAX_FULL_KEYNAME) ||
            WStrToStr(buf, bufSize, buffer) <= 0)
            return Error(IDS_LONGNAME);
    }
    return TRUE;
}

// type:
// 0 - end of path
// 1 - '.'
// 2 - '..'

WCHAR*
GetNextPathEscape(WCHAR* path, int& type)
{
    CALL_STACK_MESSAGE2("GetNextPathEscape(, %d)", type);
    WCHAR* ptr = wcsstr(path, L"\\.");
    if (ptr == NULL)
    {
        type = 0;
        return path + wcslen(path);
    }
    if (ptr[2] == L'\\' || ptr[2] == L'\0')
    {
        type = 1; // '.'
        return ptr;
    }
    else
    {
        if (ptr[2] == L'.' && (ptr[3] == L'\\' || ptr[3] == L'\0'))
        {
            type = 2; // '..'
            return ptr;
        }
        else
        {
            return GetNextPathEscape(ptr + 2, type);
        }
    }
}

BOOL CPluginFSInterface::GetFullFSPathW(WCHAR* path, int pathSize, BOOL& success)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetFullFSPathW(, %d, %d)", pathSize,
                        success);
    WCHAR row[2 * MAX_FULL_KEYNAME];
    WCHAR translated[2 * MAX_FULL_KEYNAME];
    success = FALSE;
    if (*path == L'\\')
    {
        if (wcslen(path) >= 2 * MAX_FULL_KEYNAME)
            return Error(IDS_LONGNAME), TRUE;
        wcscpy(row, path);
    }
    else
    {
        if (!GetCurrentPathW(row, MAX_FULL_KEYNAME))
            return TRUE;
        if (!PathAppend(row, path, 2 * MAX_FULL_KEYNAME))
            return Error(IDS_LONGNAME), TRUE;
    }
    // remove ".." and "."
    int s = 0, d = 0;
    WCHAR* ptr;
    int len;
    int type = -1;
    while (type != 0)
    {
        ptr = GetNextPathEscape(row + s, type);
        len = (int)(ptr - (row + s));
        memcpy(translated + d, row + s, len * 2);
        d += len;
        s += len;
        switch (type)
        {
        case 1:
            s += 2;
            break;
        case 2:
        {
            translated[d] = L'\0';
            if (!CutDirectory(translated))
                return Error(IDS_BADPATH), TRUE;
            s += 3;
            d = (int)wcslen(translated);
        }
        }
    }
    translated[d++] = L'\0';
    if (d > pathSize)
        return Error(IDS_LONGNAME), TRUE;
    wcscpy(path, translated);
    success = TRUE;
    return TRUE;
}

BOOL CPluginFSInterface::GetFullFSPath(HWND parent, const char* fsName, char* path, int pathSize, BOOL& success)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::GetFullFSPath(, , , %d, %d)",
                        pathSize, success);
    PARENT(parent);

    if (path[0] == '?')
    {
        TRACE_E("CPluginFSInterface::GetFullFSPath called with path '?'");
        success = FALSE;
        return TRUE;
    }
    else
    {
        WCHAR buffer[MAX_FULL_KEYNAME];
        if (MultiByteToWideChar(CP_ACP, 0, path, -1, buffer, MAX_FULL_KEYNAME) <= 0)
            return TRUE;
        GetFullFSPathW(buffer, MAX_FULL_KEYNAME, success);
        if (!success)
            return TRUE;

        lstrcpyn(path, fsName, pathSize);
        StrNCat(path, ":", pathSize);
        int l = (int)strlen(path);
        success = WStrToStr(path + l, pathSize - l, buffer) >= 0 || Error(IDS_LONGNAME);
        return TRUE;
    }
}

BOOL CPluginFSInterface::GetRootPath(char* userPart)
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::GetRootPath()");
    strcpy(userPart, "\\");
    return TRUE;
}

BOOL CPluginFSInterface::IsCurrentPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::IsCurrentPath(%d, %d, %s)", currentFSNameIndex, fsNameIndex, userPart);
    WCHAR currentPath[MAX_FULL_KEYNAME];
    WCHAR buffer[MAX_FULL_KEYNAME];
    WCHAR* user;
    if (userPart[0] == '?' && *NewPath != L'\0')
    {
        user = (WCHAR*)NewPath;
    }
    else
    {
        if (MultiByteToWideChar(CP_ACP, 0, userPart, -1, buffer, MAX_FULL_KEYNAME) <= 0)
            return FALSE;
        user = buffer;
    }
    BOOL ret = GetCurrentPathW(currentPath, MAX_FULL_KEYNAME);
    return ret && CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE, currentPath, -1, user, -1) == CSTR_EQUAL;
}

BOOL CPluginFSInterface::IsOurPath(int currentFSNameIndex, int fsNameIndex, const char* userPart)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::IsOurPath(%d, %d, %s)", currentFSNameIndex, fsNameIndex, userPart);
    return TRUE;
}

BOOL ReportPathError(int err, int keyRoot, WCHAR* path)
{
    CALL_STACK_MESSAGE3("ReportPathError(%d, %d, )", err, keyRoot);
    WCHAR buffer[MAX_FULL_KEYNAME + 200];
    wcscpy(buffer, PredefinedHKeys[keyRoot].KeyName);
    PathAppend(buffer, path, MAX_FULL_KEYNAME + 200);
    return ErrorL(err, IDS_OPENKEY, buffer);
}

BOOL CPluginFSInterface::ChangePath(int currentFSNameIndex, char* fsName, int fsNameIndex,
                                    const char* userPart, char* cutFileName, BOOL* pathWasCut,
                                    BOOL forceRefresh, int mode)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::ChangePath(%d, %s, %d, %s, , %d, %d)",
                        currentFSNameIndex, fsName, fsNameIndex, userPart, forceRefresh, mode);
    BOOL firstChangePath = FirstChangePath;
    FirstChangePath = FALSE;

    FocusFirstNewItem = FALSE;
    if (pathWasCut != NULL)
        *pathWasCut = FALSE;
    if (cutFileName != NULL)
        *cutFileName = 0;

    // build the full path
    WCHAR path[MAX_FULL_KEYNAME];
    if (userPart[0] == '?' && NewPathValid)
    {
        NewPathValid = FALSE;
        wcscpy(path, NewPath);
    }
    else
    {
        if (MultiByteToWideChar(CP_ACP, 0, userPart, -1, path, MAX_FULL_KEYNAME) <= 0)
            return FALSE;
    }
    BOOL sucess;
    GetFullFSPathW(path, MAX_FULL_KEYNAME, sucess);
    if (!sucess)
        return FALSE;

    WCHAR* keyName;
    int keyRoot;
    HKEY openHKey;
    if (!ParseFullPath(path, keyName, keyRoot))
        return Error(IDS_BADPATH);

    WCHAR cutFileNameW[MAX_PATH];
    *cutFileNameW = L'\0';
    BOOL fileNameAlreadyCut = FALSE;
    BOOL errorReported = FALSE;
    LONG err = ERROR_SUCCESS;
    if (PathError) // listing failed, try enumerating the trimmed path
    {
        PathError = FALSE;
        if (!CutDirectory(keyName, cutFileNameW, MAX_PATH))
        {
            keyRoot = -1; // fall back to the root
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
            fileNameAlreadyCut = TRUE;
            *cutFileNameW = L'\0'; // it can no longer be a file name
        }
        else
        {
            fileNameAlreadyCut = TRUE;
            if (pathWasCut != NULL)
                *pathWasCut = TRUE;
        }
    }
    while (1)
    {
        int prevErr = err; // for mode 3 remember the previous error (on the untrimmed path)
        if (keyRoot == -1 ||
            (err = RegOpenKeyExW(PredefinedHKeys[keyRoot].HKey, keyName, 0, KEY_READ, &openHKey)) == ERROR_SUCCESS)
        {
            // success, use the path as the current one
            if (keyRoot != -1)
            {
                if (cutFileName != NULL && *cutFileNameW != L'\0')
                {
                    if (RegQueryValueExW(openHKey, cutFileNameW, 0, NULL, NULL, NULL) != ERROR_SUCCESS)
                    {
                        if (mode == 3)
                        {
                            // report the error and abort
                            RegCloseKey(openHKey);
                            PathAppend(keyName, cutFileNameW, MAX_PATH);
                            return ReportPathError(prevErr, keyRoot, keyName);
                        }
                        *cutFileNameW = L'\0';
                    }
                }
                if (keyName[0] == L'\0')
                    ChangeMonitor.IgnoreNextRootChange(keyRoot);
                RegCloseKey(openHKey);
            }
            else
                *cutFileNameW = L'\0';
            wcscpy(CurrentKeyName, keyName);
            CurrentKeyRoot = keyRoot;
            if (cutFileName)
            {
                if (!WStrToStr(cutFileName, MAX_PATH, cutFileNameW))
                {
                    *cutFileName = 0;
                }
            }

            GetCurrentPathW(RecentFullPath, MAX_FULL_KEYNAME);

            ChangeMonitor.Cancel(this);
            return TRUE;
        }
        else // failure, try to shorten the path
        {
            if (mode == 1 && err != ERROR_FILE_NOT_FOUND ||
                mode == 2)
            {
                // report the error and keep shortening
                if (!errorReported)
                {
                    if (!firstChangePath)
                        ReportPathError(err, keyRoot, keyName);
                    errorReported = TRUE;
                }
            }
            else
            {
                // report the error and stop
                if (mode == 3 && (fileNameAlreadyCut || *keyName == L'\0'))
                    return ReportPathError(err, keyRoot, keyName);
            }

            if (!CutDirectory(keyName, cutFileNameW, MAX_PATH)) // nothing left to shorten
            {
                keyRoot = -1; // fall back to the root
                if (pathWasCut != NULL)
                    *pathWasCut = TRUE;
                fileNameAlreadyCut = TRUE;
                *cutFileNameW = L'\0'; // it can no longer be a file name
            }
            else
            {
                if (pathWasCut != NULL)
                    *pathWasCut = TRUE;
                if (!fileNameAlreadyCut) // it can be a file name only during the first trimming
                {
                    fileNameAlreadyCut = TRUE;
                }
                else
                {
                    *cutFileNameW = L'\0'; // it can no longer be a file name
                }
            }
        }
    }

    return FALSE;
}

const char* Bin2ASCII = "0123456789abcdef";

void PrintHexValue(unsigned char* data, int size, char* buffer, int bufSize)
{
    CALL_STACK_MESSAGE3("PrintHexValue(, %d, , %d)", size, bufSize);
    int i;
    for (i = 0; i < min(size, (bufSize + 1) / 3); i++)
    {
        buffer[i * 3] = Bin2ASCII[data[i] >> 4];
        buffer[i * 3 + 1] = Bin2ASCII[data[i] & 0x0F];
        if (i + 1 < min(size, (bufSize + 1) / 3))
            buffer[i * 3 + 2] = ' ';
    }
    if (i != size)
    {
        // doesn't fit, append an ellipsis
        memcpy(buffer + bufSize - 3, "...", 3);
    }
}

BOOL CPluginFSInterface::ListCurrentPath(CSalamanderDirectoryAbstract* dir,
                                         CPluginDataInterfaceAbstract*& pluginData,
                                         int& iconsType, BOOL forceRefresh)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::ListCurrentPath(, , , %d)",
                        forceRefresh);
    CFileData file;
    FILETIME time;

    dir->Clear(NULL);
    dir->SetValidData(VALID_DATA_DATE | VALID_DATA_TIME | VALID_DATA_SIZE);

    pluginData = new CPluginDataInterface();

    if (CurrentKeyRoot == -1)
    {
        int i = 0;
        while (PredefinedHKeys[i].HKey != NULL)
        {
            if (RegQueryInfoKeyW(PredefinedHKeys[i].HKey, NULL, NULL, NULL, NULL, NULL, NULL,
                                 NULL, NULL, NULL, NULL, &time) == ERROR_SUCCESS)
            {
                file.Name = DupStrA(PredefinedHKeys[i].KeyName);
                file.NameLen = strlen(file.Name);
                file.Size = CQuadWord(0, 0);
                FileTimeToLocalFileTime(&time, &file.LastWrite);
                file.PluginData = (DWORD_PTR) new CPluginData(DupStr(PredefinedHKeys[i].KeyName), 0);
                dir->AddDir(NULL, file, pluginData);
            }
            i++;
        }

        iconsType = pitFromPlugin;

        return TRUE;
    }

    PathError = TRUE;

    HKEY openHKey;
    LONG err;
    BOOL errorReported = FALSE;
    if ((err = RegOpenKeyExW(PredefinedHKeys[CurrentKeyRoot].HKey, CurrentKeyName, 0, KEY_READ, &openHKey)) == ERROR_SUCCESS)
    {
        DWORD subKeys;
        DWORD values;
        DWORD maxValueLen;
        if ((err = RegQueryInfoKeyW(openHKey, NULL, NULL, NULL, &subKeys, NULL, NULL,
                                    &values, NULL, &maxValueLen, NULL, &time)) == ERROR_SUCCESS)
        {
            // MS sometimes returns half the size (observed on MULTI_SZ in
            // key HKEY_LOCAL_MACHINE\SYSTEM\ControlSet002\Services\NetBT\Linkage
            maxValueLen *= 2;
            // add the parent directory entry
            file.Name = SG->DupStr("..");
            file.NameLen = strlen(file.Name);
            FileTimeToLocalFileTime(&time, &file.LastWrite);
            file.PluginData = (DWORD_PTR) new CPluginData(DupStr(L".."), 0);
            dir->AddDir(NULL, file, pluginData);

            // enumerate subkeys
            WCHAR name[MAX_KEYNAME];
            DWORD nameLen;
            DWORD i;
            for (i = 0; i < subKeys; i++)
            {
                nameLen = MAX_KEYNAME;
                if ((err = RegEnumKeyExW(openHKey, i, name, &nameLen, 0, NULL, NULL, &time)) == ERROR_SUCCESS ||
                    err == ERROR_MORE_DATA)
                {
                    file.Name = DupStrA(name);
                    file.NameLen = strlen(file.Name);
                    file.Size = CQuadWord(0, 0);
                    FileTimeToLocalFileTime(&time, &file.LastWrite);
                    file.PluginData = (DWORD_PTR) new CPluginData(DupStr(name), 0);
                    dir->AddDir(NULL, file, pluginData);
                }
                else
                {
                    if (!errorReported)
                    {
                        char buf1[1024];
                        SalPrintf(buf1, 1024, LoadStr(IDS_ENUMKEY), SG->GetErrorText(err));
                        MSGBOXEX_PARAMS mbp;
                        memset(&mbp, 0, sizeof(mbp));
                        mbp.HParent = SG->GetMainWindowHWND();
                        mbp.Text = buf1;
                        mbp.Caption = LoadStr(IDS_REGEDTERR);
                        mbp.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONHAND;
                        mbp.CheckBoxText = NULL;
                        mbp.CheckBoxValue = NULL;
                        errorReported = TRUE;
                        if (SG->SalMessageBoxEx(&mbp) != IDYES)
                            break;
                    }
                }
            }

            // enumerate values only if enumerating subkeys succeeded
            if (i >= subKeys)
            {
                time.dwLowDateTime = 0;
                time.dwHighDateTime = 0;
                DWORD type;
                DWORD size;
                BOOL hasDefault = FALSE;
                LPBYTE data = (LPBYTE)malloc(maxValueLen);
                for (i = 0; i < values; i++)
                {
                    nameLen = MAX_KEYNAME;
                    size = maxValueLen;
                    if ((err = RegEnumValueW(openHKey, i, name, &nameLen, 0, &type, data, &size)) == ERROR_SUCCESS)
                    {
                        if (*name == 0)
                        {
                            hasDefault = TRUE;
                            file.Name = SG->DupStr(LoadStr(IDS_DEFAULTVALUE));
                        }
                        else
                        {
                            file.Name = DupStrA(name);
                        }
                        file.NameLen = strlen(file.Name);
                        file.Size = CQuadWord(size, 0);
                        file.LastWrite = time;
                        CPluginData* pd = new CPluginData(DupStr(name), type);
                        file.PluginData = (DWORD_PTR)pd;
                        switch (type)
                        {
                        case REG_MULTI_SZ:
                        {
                            // replace separator NULL characters with spaces
                            WCHAR* ptr = (WCHAR*)data;
                            while (ptr < (WCHAR*)data + min(size / 2, MAX_DATASIZE))
                            {
                                if (*ptr == L'\0')
                                {
                                    if (ptr + 2 == (WCHAR*)data + size / 2)
                                    {
                                        size -= 2; // ignore the last '\0'; there are two of them
                                        break;
                                    }
                                    else
                                    {
                                        *ptr = L' ';
                                    }
                                }
                                ptr++;
                            }
                            // continue processing
                        }
                        case REG_EXPAND_SZ:
                        case REG_SZ:
                            pd->Allocated = 1;
                            pd->DataSize = (unsigned char)min(size / 2, MAX_DATASIZE);
                            if (pd->DataSize)
                            {
                                pd->Data = (DWORD_PTR)malloc(pd->DataSize);
                                WStrToStr((char*)pd->Data, pd->DataSize, (WCHAR*)data, pd->DataSize);
                                if (pd->DataSize < size / 2)
                                    strcpy((char*)pd->Data + pd->DataSize - 4, "...");
                            }
                            break;

                        case REG_DWORD:
                            pd->Data = *(DWORD*)data;
                            break;

                        case REG_DWORD_BIG_ENDIAN:
                            pd->Data = (DWORD)data[3] | (data[2] << 8) | (data[1] << 16) | (data[0] << 24);
                            break;

                        case REG_QWORD:
                            if (size > 0)
                            {
                                pd->Allocated = 1;
                                pd->Data = (DWORD_PTR)malloc(8);
                                memcpy((void*)pd->Data, data, 8);
                            }
                            break;

                        default:
                            if (size > 0)
                            {
                                pd->Allocated = 1;
                                pd->DataSize = (unsigned char)min(size * 3 - 1, MAX_DATASIZE);
                                pd->Data = (DWORD_PTR)malloc(pd->DataSize);
                                PrintHexValue(data, size, (char*)pd->Data, pd->DataSize);
                            }
                        }
                        dir->AddFile(NULL, file, pluginData);
                    }
                    else
                    {
                        if (!errorReported)
                        {
                            char buf1[1024];
                            SalPrintf(buf1, 1024, LoadStr(IDS_ENUMKEY), SG->GetErrorText(err));
                            MSGBOXEX_PARAMS mbp;
                            memset(&mbp, 0, sizeof(mbp));
                            mbp.HParent = SG->GetMainWindowHWND();
                            mbp.Text = buf1;
                            mbp.Caption = LoadStr(IDS_REGEDTERR);
                            mbp.Flags = MSGBOXEX_YESNO | MSGBOXEX_ICONHAND;
                            mbp.CheckBoxText = NULL;
                            mbp.CheckBoxValue = NULL;
                            errorReported = TRUE;
                            if (SG->SalMessageBoxEx(&mbp) != IDYES)
                                break;
                        }
                    }
                }
                free(data);
                if (i >= values)
                {
                    if (!hasDefault)
                    {
                        file.Name = SG->DupStr(LoadStr(IDS_DEFAULTVALUE));
                        file.NameLen = strlen(file.Name);
                        file.Size = CQuadWord(0, 0);
                        file.LastWrite = time;
                        file.PluginData = (DWORD_PTR) new CPluginData(NULL, REG_NONE);
                        dir->AddFile(NULL, file, pluginData);
                    }
                    PathError = FALSE; // success
                }
            }
        }
        if (CurrentKeyName[0] == L'\0')
            ChangeMonitor.IgnoreNextRootChange(CurrentKeyRoot);
        RegCloseKey(openHKey);
    }

    if (PathError)
    {
        dir->Clear(pluginData);
        delete pluginData;
        return !errorReported && ErrorL(err, IDS_OPENKEY2, PredefinedHKeys[CurrentKeyRoot].KeyName, CurrentKeyName);
    }

    iconsType = pitFromPlugin;

    // register the new path with the change monitor
    ChangeMonitor.AddPath(CurrentKeyRoot, CurrentKeyName, this);

    return TRUE;
}
