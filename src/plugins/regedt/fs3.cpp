// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

BOOL ExportStringsForViewerInASCII = TRUE;

// ****************************************************************************
//
// CPluginFSInterface - second part
//
//

BOOL CPluginFSInterface::TryCloseOrDetach(BOOL forceClose, BOOL canDetach, BOOL& detach, int reason)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::TryCloseOrDetach(%d, %d, %d)",
                        forceClose, canDetach, detach);
    detach = FALSE;
    return TRUE;
}

void CPluginFSInterface::Event(int event, DWORD param)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::Event(, 0x%X)", param);
}

HICON
CPluginFSInterface::GetFSIcon(BOOL& destroyIcon)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::GetFSIcon(%d)", destroyIcon);
    return (HICON)LoadImage(DLLInstance, MAKEINTRESOURCE(IDI_REGEDT), IMAGE_ICON,
                            16, 16, SG->GetIconLRFlags());
}

BOOL CPluginFSInterface::GetNextDirectoryLineHotPath(const char* text, int pathLen, int& offset)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::GetNextDirectoryLineHotPath(%s, %d, "
                        "%d)",
                        text, pathLen, offset);
    if (text[offset] == '\\')
        offset++;
    char* ret = (char*)memchr(text + offset, '\\', pathLen - offset);
    if (ret)
    {
        if (offset == 0)
            offset = (int)(ret - text + 1);
        else
            offset = (int)(ret - text);
        return TRUE;
    }
    else
        return FALSE;
}

BOOL TestKey(int root, LPWSTR key)
{
    CALL_STACK_MESSAGE2("TestKey(%d, )", root);
    while (1)
    {
        HKEY hKey;
        int res = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, NULL, KEY_READ, &hKey);
        if (res != ERROR_SUCCESS)
        {
            if (res == ERROR_FILE_NOT_FOUND)
            {
                return TRUE;
            }
            if (!RegOperationError(res, IDS_OPEN, IDS_RENAMEKEY, root, key, NULL, NULL))
                return FALSE;
        }
        else
        {
            SG->SalMessageBox(GetParent(), LoadStr(IDS_CANNOTRENAME), LoadStr(IDS_RENAMEKEY), MB_OK);
            return FALSE;
        }
    }
    return TRUE;
}

BOOL CPluginFSInterface::QuickRename(const char* fsName, int mode, HWND parent, CFileData& file, BOOL isDir,
                                     char* newName, BOOL& cancel)
{
    CALL_STACK_MESSAGE5("CPluginFSInterface::QuickRename(%s, %d, , , %d, , %d)",
                        fsName, mode, isDir, cancel);
    PARENT(parent);

    if (mode == 2)
    {
        cancel = FALSE;
        return FALSE;
    }
    cancel = TRUE;

    if (CurrentKeyRoot == -1)
        return FALSE; // HKEY_XXX keys cannot be renamed :-)

    CPluginData* pluginData = (CPluginData*)file.PluginData;

    // skip the default value if it is not set
    if (pluginData->Name == NULL && pluginData->Type == REG_NONE)
    {
        Error(IDS_CANNOTRENAMEDEFVAL);
        cancel = FALSE;
        return TRUE;
    }

    WCHAR keyName[MAX_FULL_KEYNAME];
    keyName[0] = 0;
    if (pluginData->Name)
        wcscpy(keyName, pluginData->Name);
    WCHAR text[MAX_KEYNAME + 100];
    swprintf_s(text, LoadStrW(isDir ? IDS_QUICKRENAMEKEY : IDS_QUICKRENAMEVAL), keyName);
    CNewKeyDialog dlg(parent, keyName, NULL, text, LoadStrW(IDS_QUICKRENAME), IDD_RENAME);
    if (dlg.Execute() != IDOK)
        return FALSE;

    if (CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                       pluginData->Name, -1,
                       keyName, -1) == CSTR_EQUAL)
    {
        SG->SalMessageBox(GetParent(), LoadStr(isDir ? IDS_SAMECASEKEY : IDS_SAMECASEVAL),
                          LoadStr(isDir ? IDS_RENAMEKEY : IDS_RENAMEVAL), MB_OK);
        return FALSE;
    }

    BOOL skip, success;
    BOOL skipAllErrors = FALSE; // skip all errors
    BOOL skipAllOverwrites = FALSE;
    BOOL skipAllLongNames = FALSE;
    BOOL skipAllClassNames = FALSE;
    BOOL overwriteAll = FALSE;
    if (isDir)
    {
        // ensure the path does not contain disallowed characters
        if (keyName[wcscspn(keyName, L"\\")] != L'\0')
        {
            SG->SalMessageBox(GetParent(), LoadStr(IDS_SYNTAX), LoadStr(IDS_RENAMEKEY), MB_OK);
            return FALSE;
        }

        WCHAR sourceKey[MAX_KEYNAME];
        WCHAR targetKey[MAX_KEYNAME];
        PathAppend(wcscpy(sourceKey, CurrentKeyName), pluginData->Name, MAX_KEYNAME);
        PathAppend(wcscpy(targetKey, CurrentKeyName), keyName, MAX_KEYNAME);

        // ensure that the target name does not exist
        if (!TestKey(CurrentKeyRoot, targetKey))
            return FALSE;

        GetAsyncKeyState(VK_ESCAPE); // initialize GetAsyncKeyState - see the help
        SG->CreateSafeWaitWindow(LoadStr(IDS_MOVEPROGRESS), LoadStr(IDS_PLUGINNAME), 500, TRUE, SG->GetMainWindowHWND());

        // stack for enumerated subkey names
        // (subkeys must be enumerated all at once
        // before they can be deleted during a move)
        TIndirectArray<WCHAR> stack(100, 100);
        WCHAR nameBuffer[MAX_KEYNAME];
        success = CopyOrMoveKey(CurrentKeyRoot, sourceKey, CurrentKeyRoot, targetKey, TRUE,
                                skip, skipAllErrors, skipAllLongNames,
                                skipAllOverwrites, overwriteAll,
                                skipAllClassNames, nameBuffer, stack);
    }
    else
    {
        SG->CreateSafeWaitWindow(LoadStr(IDS_MOVEPROGRESS), LoadStr(IDS_PLUGINNAME), 500, TRUE, SG->GetMainWindowHWND());

        success = CopyOrMoveValue(CurrentKeyRoot, CurrentKeyName, pluginData->Name,
                                  CurrentKeyRoot, CurrentKeyName, keyName, TRUE,
                                  NULL, NULL, NULL, NULL);
    }

    SG->DestroySafeWaitWindow();

    if (success)
    {
        WStrToStr(newName, MAX_PATH, keyName);
        cancel = FALSE;
        return TRUE;
    }
    else
        return FALSE;
}

void CPluginFSInterface::AcceptChangeOnPathNotification(const char* fsName, const char* path, BOOL includingSubdirs)
{
    CALL_STACK_MESSAGE4("CPluginFSInterface::AcceptChangeOnPathNotification(%s, %s, %d)",
                        fsName, path, includingSubdirs);
    if (CurrentKeyRoot != -1)
    {
        // compare paths or at least their prefixes (only paths on our FS have a chance;
        // disk paths and paths on other FS types in 'path' are filtered out automatically
        // because they can never match 'fsName'+':' at the beginning of 'path2' below)
        char path1[MAX_PATH * 2];
        char path2[MAX_FULL_KEYNAME + MAX_PATH];
        char root[MAX_PREDEF_KEYNAME];
        char key[MAX_KEYNAME];
        lstrcpyn(path1, path, MAX_PATH * 2);
        WStrToStr(root, MAX_PREDEF_KEYNAME, PredefinedHKeys[CurrentKeyRoot].KeyName);
        WStrToStr(key, MAX_KEYNAME, CurrentKeyName);
        SalPrintf(path2, MAX_FULL_KEYNAME + MAX_PATH, "%s:\\%s\\%s", fsName, root, key);
        RemoveTrailingSlashes(path1);
        RemoveTrailingSlashes(path2);
        int len1 = (int)strlen(path1);
        BOOL refresh = SG->StrNICmp(path1, path2, len1) == 0 &&
                       (path2[len1] == 0 || includingSubdirs && path2[len1] == '\\');
        if (refresh)
        {
            SG->PostRefreshPanelFS(this, FocusFirstNewItem); // refresh if this FS is displayed in a panel
        }
    }
}

BOOL CPluginFSInterface::CreateDir(const char* fsName, int mode, HWND parent, char* newName, BOOL& cancel)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::CreateDir(%d, , , %d)", mode, cancel);
    PARENT(parent);

    if (mode == 2)
    {
        cancel = FALSE;
        return FALSE;
    }
    cancel = TRUE;

    if (CurrentKeyRoot == -1)
        return FALSE; //Error(IDS_NEWKEYINROOT);

    WCHAR enteredKeyName[MAX_KEYNAME];
    enteredKeyName[0] = 0;
    while (1)
    {
        WCHAR keyName[MAX_KEYNAME], fullName[MAX_FULL_KEYNAME];
        BOOL direct = FALSE;
        CNewKeyDialog dlg(parent, enteredKeyName, &direct);
        if (dlg.Execute() != IDOK)
            return FALSE;

        wcscpy(keyName, enteredKeyName);

        // extract the user portion from the FS path
        if (!direct)
        {
            if (!RemoveFSNameFromPath(keyName))
            {
                Error(IDS_NOTREGEDTPATH);
                continue;
            }
            if (!wcslen(keyName))
            {
                Error(IDS_BADPATH);
                continue;
            }
        }

        BOOL success;
        wcscpy(fullName, keyName);
        GetFullFSPathW(fullName, MAX_FULL_KEYNAME, success);
        if (!success)
            continue;

        WCHAR* key;
        int root;
        if (!ParseFullPath(fullName, key, root))
        {
            Error(IDS_BADPATH);
            continue;
        }

        HKEY hKey;
        DWORD disp;
        int err = RegCreateKeyExW(PredefinedHKeys[root].HKey, key, 0, NULL, 0, KEY_READ, NULL, &hKey, &disp);
        if (err != ERROR_SUCCESS)
        {
            ErrorL(err, IDS_NEWKEY);
            continue;
        }

        if (disp == REG_OPENED_EXISTING_KEY)
        {
            SG->SalMessageBox(parent, LoadStr(IDS_KEYEXISTS), LoadStr(IDS_PLUGINNAME), MB_ICONINFORMATION);
            RegCloseKey(hKey);
            continue;
        }

        if (WStrToStr(newName, 2 * MAX_PATH, keyName) <= 0)
            *newName = 0;

        RegCloseKey(hKey);

        break;
    }

    cancel = FALSE;
    return TRUE;
}

BOOL ExportValueData(int root, LPCWSTR key, LPCWSTR value, const char* tmpFileName, CQuadWord* newFileSize)
{
    CALL_STACK_MESSAGE3("ExportValueData(%d, , , %s, )", root, tmpFileName);
    HKEY hKey;
    int ret = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, KEY_QUERY_VALUE, &hKey);
    if (ret != ERROR_SUCCESS)
        ErrorL(ret, IDS_QUERYVAL);

    DWORD type;
    LPBYTE data, translated;
    DWORD size;

    // determine the data size
    ret = RegQueryValueExW(hKey, value, 0, &type, NULL, &size);
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        return ErrorL(ret, IDS_QUERYVAL);
    }

    // allocate a buffer for the data
    int extra = 0;
    if (ExportStringsForViewerInASCII &&
        (type == REG_SZ || type == REG_EXPAND_SZ || type == REG_MULTI_SZ))
        extra = size / 2;
    translated = data = (LPBYTE)malloc(size + extra);
    if (!data)
    {
        RegCloseKey(hKey);
        return Error(IDS_LOWMEM);
    }

    // read the data
    ret = RegQueryValueExW(hKey, value, 0, &type, data, &size);
    if (ret != ERROR_SUCCESS)
    {
        RegCloseKey(hKey);
        free(data);
        return ErrorL(ret, IDS_QUERYVAL);
    }

    // if needed, convert the data to ASCII
    if (extra)
    {
        WStrToStr((char*)data + size, size / 2, (LPWSTR)data, size / 2);
        translated = data + size;
        size /= 2;
    }

    // create or open the temporary file
    HANDLE file = CreateFile(tmpFileName, GENERIC_WRITE, FILE_SHARE_READ, NULL,
                             CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (file == INVALID_HANDLE_VALUE)
    {
        RegCloseKey(hKey);
        free(data);
        return Error(IDS_CREATETEMP);
    }

    DWORD written;
    BOOL b = WriteFile(file, translated, size, &written, NULL) || written != size;

    DWORD err;
    SG->SalGetFileSize(file, *newFileSize, err); // ignore errors
    CloseHandle(file);
    RegCloseKey(hKey);
    free(data);

    return b || Error(IDS_WRITETEMP);
}

void CPluginFSInterface::ViewFile(const char* fsName, HWND parent,
                                  CSalamanderForViewFileOnFSAbstract* salamander,
                                  CFileData& file)
{
    CALL_STACK_MESSAGE2("CPluginFSInterface::ViewFile(%s, , , )", fsName);
    PARENT(parent);

    CPluginData* pluginData = (CPluginData*)file.PluginData;

    // skip the default value if it is not set
    // and do nothing at the registry root either
    if (pluginData->Name == NULL && pluginData->Type == REG_NONE || CurrentKeyRoot == -1)
        return;

    // build a unique file name for the disk cache (standard Salamander path format)
    char uniqueFileName[MAX_FULL_KEYNAME + MAX_PATH];
    SalPrintf(uniqueFileName, MAX_FULL_KEYNAME + MAX_PATH, "%s:\\%ls\\%ls", fsName,
              PredefinedHKeys[CurrentKeyRoot].KeyName, CurrentKeyName);
    SG->SalPathAppend(uniqueFileName, file.Name, MAX_FULL_KEYNAME + MAX_PATH);

    // disk names are case-insensitive, the disk cache is case-sensitive; converting
    // to lowercase makes the cache behave case-insensitively as well
    SG->ToLowerCase(uniqueFileName);

    // create a file name that Windows can handle
    char fileName[MAX_PATH];
    fileName[0] = '_';
    lstrcpyn(fileName + 1, file.Name, MAX_PATH - 1);

    // obtain the copy name in the disk cache
    BOOL fileExists;
    const char* tmpFileName = salamander->AllocFileNameInCache(parent, uniqueFileName,
                                                               ReplaceUnsafeCharacters(fileName),
                                                               NULL, fileExists);
    if (tmpFileName == NULL)
        return; // fatal error

    // prepare or refresh the key data copy in the disk cache
    BOOL newFileOK = FALSE;
    CQuadWord newFileSize(0, 0);
    if (ExportValueData(CurrentKeyRoot, CurrentKeyName,
                        pluginData->Name ? pluginData->Name : L"",
                        tmpFileName, &newFileSize)) // the copy succeeded
    {
        newFileOK = TRUE; // if getting the file size fails, newFileSize stays zero (not critical)
    }

    // open the viewer
    HANDLE fileLock;
    BOOL fileLockOwner;
    if (!newFileOK || // open the viewer only if the copy is valid
        !salamander->OpenViewer(parent, tmpFileName, &fileLock, &fileLockOwner))
    { // on error reset the "lock"
        fileLock = NULL;
        fileLockOwner = FALSE;
    }

    // call FreeFileNameInCache to pair with AllocFileNameInCache (linking the viewer and disk cache)
    salamander->FreeFileNameInCache(uniqueFileName, fileExists, newFileOK,
                                    newFileSize, fileLock, fileLockOwner, TRUE);
}

BOOL CPluginFSInterface::DeleteKey(WCHAR* keyName, BOOL& skip,
                                   BOOL& skipAllErrors, TIndirectArray<WCHAR>& stack)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::DeleteKey(, %d, %d, )", skip,
                        skipAllErrors);
    // check whether the user requested cancellation
    if (TestForCancel())
        return skip = FALSE;

    skip = FALSE;
    HKEY key;
    while (1)
    {
        int ret = RegOpenKeyExW(PredefinedHKeys[CurrentKeyRoot].HKey, keyName, 0, KEY_ENUMERATE_SUB_KEYS, &key);
        if (ret != ERROR_SUCCESS)
        {
            if (!RegOperationError(ret, IDS_OPEN, IDS_DELKEY, CurrentKeyRoot, keyName, &skip, &skipAllErrors))
                return FALSE;
        }
        else
            break;
    }

    DWORD i = 0;
    WCHAR name[MAX_KEYNAME];
    DWORD nameLen;
    BOOL success = TRUE;
    int top = stack.Count;
    while (success)
    {
        nameLen = MAX_KEYNAME;
        int ret = RegEnumKeyExW(key, i, name, &nameLen, 0, NULL, NULL, NULL);
        if (ret == ERROR_NO_MORE_ITEMS)
            break;
        if (ret != ERROR_SUCCESS && ret != ERROR_MORE_DATA)
        {
            success = RegOperationError(ret, IDS_ACCESS2, IDS_DELKEY, CurrentKeyRoot, keyName, &skip, &skipAllErrors);
            continue;
        }

        WCHAR* ptr = new WCHAR[wcslen(name) + 1];
        if (!ptr || stack.Add(ptr) == ULONG_MAX)
        {
            if (ptr)
                delete[] ptr;
            return Error(IDS_LOWMEM);
        }
        wcscpy(ptr, name);

        i++;
    }

    RegCloseKey(key);

    int j;
    for (j = stack.Count - 1; j >= top; j--)
    {
        if (stack[j] && success)
        {
            WCHAR subKey[MAX_KEYNAME];
            wcscpy(subKey, keyName);
            if (!PathAppend(subKey, stack[j], MAX_KEYNAME))
            {
                TRACE_E("name is too long");
                skip = TRUE;
                success = FALSE;
            }
            else
            {
                BOOL _skip;
                if (!DeleteKey(subKey, _skip, skipAllErrors, stack))
                {
                    skip = _skip;
                    if (!skip)
                        success = FALSE;
                }
            }
        }
        stack.Delete(j);
    }

    if (!success)
        return FALSE;

    // no subkey was skipped
    if (!skip)
    {
        while (success)
        {
            int ret = RegDeleteKeyW(PredefinedHKeys[CurrentKeyRoot].HKey, keyName);
            if (ret != ERROR_SUCCESS)
            {
                success = RegOperationError(ret, IDS_DELETE, IDS_DELKEY, CurrentKeyRoot, keyName, &skip, &skipAllErrors);
            }
            else
                break; // delete succeeded
        }
    }

    return success;
}

BOOL CPluginFSInterface::Delete(const char* fsName, int mode, HWND parent, int panel,
                                int selectedFiles, int selectedDirs, BOOL& cancelOrError)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::Delete(%s, %d, , %d, %d, %d, %d)", fsName, mode,
                        panel, selectedFiles, selectedDirs, cancelOrError);
    PARENT(parent);

    if (CurrentKeyRoot == -1)
    {
        cancelOrError = FALSE;
        //Error(IDS_DELKEYINROOT);
        return TRUE;
    }

    cancelOrError = FALSE;
    if (mode == 1)
        return FALSE; // request the standard prompt (if SALCFG_CNFRMFILEDIRDEL is TRUE)

    BOOL ConfirmOnNotEmptyDirDelete;
    if (!SG->GetConfigParameter(SALCFG_CNFRMNEDIRDEL, &ConfirmOnNotEmptyDirDelete, sizeof(BOOL), NULL))
        ConfirmOnNotEmptyDirDelete = TRUE;

    char buf[MAX_KEYNAME * 2]; // buffer for error texts

    WCHAR keyName[MAX_KEYNAME]; // buffer for the full name
    wcscpy(keyName, CurrentKeyName);
    WCHAR* end = keyName + wcslen(keyName); // space for names from the panel
    WCHAR* backslash = NULL;
    if (end > keyName && *(end - 1) != L'\\')
    {
        backslash = end;
        *end++ = L'\\';
        *end = L'\0';
    }
    int endSize = MAX_KEYNAME - (int)(end - keyName); // maximum number of characters for a panel name

    const CFileData* f = NULL; // pointer to the panel file/directory being processed
    BOOL isDir = FALSE;        // TRUE if 'f' is a directory
    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int index = 0;
    BOOL success = TRUE;        // FALSE if an error occurs or the user cancels
    BOOL skipAllErrors = FALSE; // skip all errors

    GetAsyncKeyState(VK_ESCAPE); // initialize GetAsyncKeyState - see the help
    SG->CreateSafeWaitWindow(LoadStr(IDS_DELETEPROGRESS), LoadStr(IDS_PLUGINNAME), 500, TRUE, SG->GetMainWindowHWND());

    while (1)
    {
        // fetch data about the item being processed
        if (focused)
            f = SG->GetPanelFocusedItem(panel, &isDir);
        else
            f = SG->GetPanelSelectedItem(panel, &index, &isDir);

        // delete the file or directory
        if (f != NULL)
        {
            BOOL skip = FALSE;
            CPluginData* pd = (CPluginData*)f->PluginData;

            if (isDir)
            {
                lstrcpynW(end, pd->Name, endSize);

                BOOL empty;
                HKEY key;
                while (success)
                {
                    int ret = RegOpenKeyExW(PredefinedHKeys[CurrentKeyRoot].HKey, keyName, 0, KEY_QUERY_VALUE, &key);
                    if (ret == ERROR_SUCCESS)
                    {
                        DWORD subKeys;
                        DWORD values;
                        ret = RegQueryInfoKeyW(key, NULL, NULL, NULL, &subKeys, NULL, NULL,
                                               &values, NULL, NULL, NULL, NULL);
                        empty = subKeys == 0 && values == 0;
                        RegCloseKey(key);
                    }
                    if (ret != ERROR_SUCCESS)
                    {
                        if (!RegOperationError(ret, IDS_ACCESS2, IDS_DELKEY, CurrentKeyRoot, keyName, &skip, &skipAllErrors))
                        {
                            if (!skip)
                                success = FALSE;
                            break;
                        }
                    }
                    else
                        break;
                }

                if (success && !skip && ConfirmOnNotEmptyDirDelete && !empty)
                {
                    sprintf(buf, LoadStr(IDS_CONFIRNNONEMPTYDELETE), f->Name);
                    switch (SG->SalMessageBox(parent, buf, LoadStr(IDS_QUESTION), MB_ICONQUESTION | MB_YESNOCANCEL))
                    {
                    case IDYES:
                        break;
                    case IDNO:
                        skip = TRUE;
                        break;
                    case IDCANCEL:
                    default:
                        success = FALSE;
                        break;
                    }
                }

                if (success && !skip)
                {
                    TIndirectArray<WCHAR> stack(1, 128);
                    if (!DeleteKey(keyName, skip, skipAllErrors, stack) && !skip)
                        success = FALSE;
                }
            }
            else
            {
                // skip deleting the default value if it is not set
                if (pd->Name != NULL || pd->Type != REG_NONE)
                {
                    while (1)
                    {
                        *end = L'\0';
                        HKEY key;
                        int ret = RegOpenKeyExW(PredefinedHKeys[CurrentKeyRoot].HKey, keyName, 0, KEY_SET_VALUE, &key);
                        if (ret == ERROR_SUCCESS)
                        {
                            ret = RegDeleteValueW(key, pd->Name);
                            RegCloseKey(key);
                        }
                        if (ret != ERROR_SUCCESS)
                        {
                            if (!skipAllErrors)
                            {
                                char fullName[MAX_FULL_KEYNAME + 30]; // buffer for the full REG name shown in an error dialog
                                int l = sprintf(fullName, "%s:\\", fsName);
                                GetCurrentPath(fullName + l);
                                SG->SalPathAppend(fullName, f->Name, MAX_FULL_KEYNAME + 30);
                                int res = SG->DialogError(parent, BUTTONS_RETRYSKIPCANCEL, fullName,
                                                          SG->GetErrorText(ret), LoadStr(IDS_DELVAL));
                                switch (res)
                                {
                                case DIALOG_RETRY:
                                    break;

                                case DIALOG_SKIPALL:
                                    skipAllErrors = TRUE;
                                case DIALOG_SKIP:
                                    skip = TRUE;
                                    break;

                                default:
                                    success = FALSE;
                                    break; // DIALOG_CANCEL
                                }
                            }
                            else
                                skip = TRUE;
                        }
                        else
                            break; // delete succeeded

                        if (!success || skip)
                            break;
                    }
                }
            }
        }

        // decide whether it makes sense to continue (if no error and another marked item exists)
        if (!success || focused || f == NULL)
            break;
    }

    SG->DestroySafeWaitWindow();

    return success;
}

void CPluginFSInterface::ContextMenu(const char* fsName, HWND parent, int menuX, int menuY, int type,
                                     int panel, int selectedFiles, int selectedDirs)
{
    CALL_STACK_MESSAGE7("CPluginFSInterface::ContextMenu(%s, , %d, %d, , %d, %d, %d)",
                        fsName, menuX, menuY, panel, selectedFiles, selectedDirs);
    PARENT(parent);

    // create the menu
    CGUIMenuPopupAbstract* menu = SalGUI->CreateMenuPopup();
    if (!menu)
        return;

    BOOL focusIsDir;
    const CFileData* fd = SG->GetPanelFocusedItem(panel, &focusIsDir);
    CPluginData* pd = (CPluginData*)fd->PluginData;

    BOOL targetIsReg = SG->GetPanelPluginFS(PANEL_TARGET) != NULL;

    int i = 0;
    char name[256];
    if (type == fscmItemsInPanel)
    {
        if (!pd)
            return;
        BOOL rawEdit = !focusIsDir &&
                       pd->Type != REG_DWORD_BIG_ENDIAN &&
                       pd->Type != REG_DWORD &&
                       pd->Type != REG_QWORD &&
                       pd->Type != REG_SZ &&
                       pd->Type != REG_EXPAND_SZ &&
                       pd->Type != REG_NONE;

        // open
        MENU_ITEM_INFO mi;
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = rawEdit ? MENU_STATE_GRAYED : MENU_STATE_DEFAULT;
        mi.ID = SALCMD_OPEN + 1;
        SG->GetSalamanderCommand(SALCMD_OPEN, name, 256, NULL, NULL);
        if (!focusIsDir)
        {
            // append Salamander's shortcut to the command name
            const char* str = LoadStr(IDS_EDIT);
            int len = (int)strlen(str);
            char* tab = strrchr(name, '\t');
            if (!rawEdit && tab)
            {
                memmove(name + len, tab, strlen(tab) + 1);
                memmove(name, str, len);
            }
            else
                strcpy(name, str);
        }
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        /* used by the export_mnu.py script that generates salmenu.mnu for Translator
   keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM ItemsInPanelMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_RAWEDIT
  {MNTT_IT, IDS_EXPORT
  {MNTT_IT, IDS_SEARCH
  {MNTT_IT, IDS_COPYFULLNAME
  {MNTT_IT, IDS_COPYNAME
  {MNTT_PE, 0
};
*/

        // raw edit
        mi.State = focusIsDir ? MENU_STATE_GRAYED : 0;
        if (rawEdit)
        {
            mi.ID = SALCMD_OPEN + 1;
            mi.State |= MENU_STATE_DEFAULT;
            SG->GetSalamanderCommand(SALCMD_OPEN, name, 256, NULL, NULL);

            // append Salamander's shortcut to the command name
            const char* str = LoadStr(IDS_RAWEDIT);
            int len = (int)strlen(str);
            char* tab = strrchr(name, '\t');
            if (rawEdit && tab)
            {
                memmove(name + len, tab, strlen(tab) + 1);
                memmove(name, str, len);
            }
            else
                strcpy(name, str);
            mi.String = name;
        }
        else
        {
            mi.ID = rawEdit ? SALCMD_OPEN + 1 : CMD_RAWEDIT;
            mi.String = LoadStr(IDS_RAWEDIT);
        }
        menu->InsertItem(i++, TRUE, &mi);

        // rename
        mi.State = CurrentKeyRoot == -1 ? MENU_STATE_GRAYED : 0;
        mi.ID = SALCMD_QUICKRENAME + 1;
        SG->GetSalamanderCommand(SALCMD_QUICKRENAME, name, 256, NULL, NULL);
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // view
        mi.State = focusIsDir ? MENU_STATE_GRAYED : 0;
        mi.ID = SALCMD_VIEW + 1;
        SG->GetSalamanderCommand(SALCMD_VIEW, name, 256, NULL, NULL);
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // copy
        mi.State = CurrentKeyRoot == -1 /*|| !targetIsReg*/ ? MENU_STATE_GRAYED : 0;
        mi.ID = SALCMD_COPY + 1;
        SG->GetSalamanderCommand(SALCMD_COPY, name, 256, NULL, NULL);
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // delete
        mi.State = CurrentKeyRoot == -1 ? MENU_STATE_GRAYED : 0;
        mi.ID = SALCMD_DELETE + 1;
        SG->GetSalamanderCommand(SALCMD_DELETE, name, 256, NULL, NULL);
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // separator
        mi.Mask = MENU_MASK_TYPE;
        mi.Type = MENU_TYPE_SEPARATOR;
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // export
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = !focusIsDir ? MENU_STATE_GRAYED : 0;
        mi.ID = CMD_EXPORT;
        mi.String = LoadStr(IDS_EXPORT);
        menu->InsertItem(i++, TRUE, &mi);

        // search
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = !focusIsDir ? MENU_STATE_GRAYED : 0;
        mi.ID = CMD_SEARCH;
        mi.String = LoadStr(IDS_SEARCH);
        menu->InsertItem(i++, TRUE, &mi);

        // separator
        mi.Mask = MENU_MASK_TYPE;
        mi.Type = MENU_TYPE_SEPARATOR;
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // copy full name
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = 0;
        mi.ID = CMD_COPYFULLNAME;
        mi.String = LoadStr(IDS_COPYFULLNAME);
        menu->InsertItem(i++, TRUE, &mi);

        // copy name
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = 0;
        mi.ID = CMD_COPYNAME;
        mi.String = LoadStr(IDS_COPYNAME);
        menu->InsertItem(i++, TRUE, &mi);
    }
    if (type == fscmPathInPanel || type == fscmPanel)
    {
        /* used by the export_mnu.py script that generates salmenu.mnu for Translator
   keep synchronized with the InsertItem() calls below...
MENU_TEMPLATE_ITEM PanelMenu[] = 
{
  {MNTT_PB, 0
  {MNTT_IT, IDS_MENUNEWKEY
  {MNTT_IT, IDS_MENUNEWVAL
  {MNTT_IT, IDS_EXPORT
  {MNTT_IT, IDS_SEARCH
  {MNTT_IT, IDS_COPYPATH
  {MNTT_PE, 0
};
*/

        // new key
        MENU_ITEM_INFO mi;
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = CurrentKeyRoot == -1 ? MENU_STATE_GRAYED : 0;
        mi.ID = SALCMD_CREATEDIRECTORY + 1;
        // append Salamander's shortcut to the command name
        SG->GetSalamanderCommand(SALCMD_CREATEDIRECTORY, name, 256, NULL, NULL);
        const char* str = LoadStr(IDS_MENUNEWKEY);
        int len = (int)strlen(str);
        char* tab = strrchr(name, '\t');
        if (tab)
        {
            memmove(name + len, tab, strlen(tab) + 1);
            memmove(name, str, len);
        }
        else
            strcpy(name, str);
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // new value
        mi.State = CurrentKeyRoot == -1 ? MENU_STATE_GRAYED : 0;
        mi.ID = CMD_NEWVALUE;
        mi.String = LoadStr(IDS_MENUNEWVAL);
        menu->InsertItem(i++, TRUE, &mi);

        // separator
        mi.Mask = MENU_MASK_TYPE;
        mi.Type = MENU_TYPE_SEPARATOR;
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // export
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = 0;
        mi.ID = CMD_EXPORT;
        mi.String = LoadStr(IDS_EXPORT);
        menu->InsertItem(i++, TRUE, &mi);

        // search
        mi.State = 0;
        mi.ID = CMD_SEARCH;
        mi.String = LoadStr(IDS_SEARCH);
        menu->InsertItem(i++, TRUE, &mi);

        // separator
        mi.Mask = MENU_MASK_TYPE;
        mi.Type = MENU_TYPE_SEPARATOR;
        mi.String = name;
        menu->InsertItem(i++, TRUE, &mi);

        // copy full path
        mi.Mask = MENU_MASK_TYPE | MENU_MASK_STATE | MENU_MASK_ID | MENU_MASK_STRING;
        mi.Type = MENU_TYPE_STRING;
        mi.State = 0;
        mi.ID = CMD_COPYPATH;
        mi.String = LoadStr(IDS_COPYPATH);
        menu->InsertItem(i++, TRUE, &mi);
    }

    DWORD cmd = menu->Track(MENU_TRACK_RETURNCMD | MENU_TRACK_RIGHTBUTTON | MENU_TRACK_NONOTIFY,
                            menuX, menuY, parent, NULL);
    TRACE_I("cmd = " << cmd);

    switch (cmd)
    {
    case CMD_RAWEDIT:
        EditValue(CurrentKeyRoot, CurrentKeyName, pd->Name, TRUE);
        break;
    case CMD_NEWVALUE:
        EditNewFile();
        break;
    case CMD_EXPORT:
    {
        WCHAR path[MAX_FULL_KEYNAME];
        int len = StrToWStr(path, MAX_FULL_KEYNAME, AssignedFSName);
        path[len - 1] = L':';
        if (!GetCurrentPathW(path + len, MAX_FULL_KEYNAME - len))
        {
            wcscpy(path + len, L"\\");
        }
        if (type == fscmItemsInPanel && focusIsDir)
        {
            PathAppend(path, pd->Name, MAX_FULL_KEYNAME);
        }
        ExportKey(path);
        break;
    }

    case CMD_SEARCH:
    {
        WCHAR path[MAX_FULL_KEYNAME];
        int len = StrToWStr(path, MAX_FULL_KEYNAME, AssignedFSName);
        path[len - 1] = L':';
        if (!GetCurrentPathW(path + len, MAX_FULL_KEYNAME))
        {
            wcscpy(path + len, L"\\");
        }
        if (type == fscmItemsInPanel)
        {
            PathAppend(path, pd->Name, MAX_FULL_KEYNAME);
        }
        CFindDialogThread* t = new CFindDialogThread(path);
        if (t)
        {
            if (!t->Create(ThreadQueue))
                delete t;
        }
        else
            Error(IDS_LOWMEM);
        break;
    }

    case CMD_COPYNAME:
    {
        SG->CopyTextToClipboard(fd->Name, -1, FALSE, NULL);
        break;
    }

    case CMD_COPYFULLNAME:
    {
        char path[MAX_PATH];
        if (GetCurrentPath(path) && SG->SalPathAppend(path, fd->Name, MAX_PATH))
        {
            SG->CopyTextToClipboard(*path == '\\' ? path + 1 : path, -1,
                                    FALSE, NULL);
        }
        break;
    }

    case CMD_COPYPATH:
    {
        char path[MAX_PATH];
        if (GetCurrentPath(path))
        {
            SG->CopyTextToClipboard(*path == '\\' ? path + 1 : path, -1,
                                    FALSE, NULL);
        }
        break;
    }

    default:
        if (cmd > 0 && cmd <= 500)
            SG->PostSalamanderCommand(cmd - 1);
    }

    SalGUI->DestroyMenuPopup(menu);
}

BOOL CPluginFSInterface::EditNewFile()
{
    CALL_STACK_MESSAGE1("CPluginFSInterface::EditNewFile()");
    if (CurrentKeyRoot == -1)
        return TRUE; //Error(IDS_NEWKEYINROOT);

    WCHAR enteredPathName[MAX_KEYNAME];
    enteredPathName[0] = 0;
    while (1)
    {
        WCHAR relativePathName[MAX_KEYNAME], valName[MAX_KEYNAME], fullName[MAX_FULL_KEYNAME];
        DWORD type = REG_SZ;
        BOOL direct = FALSE;
        CNewValDialog dlg(GetParent(), enteredPathName, &type, &direct);
        if (dlg.Execute() != IDOK)
            return FALSE;

        wcscpy(relativePathName, enteredPathName);

        // extract the user portion from the FS path
        if (!direct)
        {
            if (!RemoveFSNameFromPath(relativePathName))
            {
                Error(IDS_NOTREGEDTPATH);
                continue;
            }
            if (!wcslen(relativePathName))
            {
                Error(IDS_BADPATH);
                continue;
            }
        }

        BOOL success;
        wcscpy(fullName, relativePathName);
        GetFullFSPathW(fullName, MAX_FULL_KEYNAME, success);
        if (!success)
            continue;

        WCHAR* key;
        int root;
        if (!ParseFullPath(fullName, key, root) || !CutDirectory(key, valName, MAX_KEYNAME))
        {
            Error(IDS_BADPATH);
            continue;
        }

        HKEY hKey;
        int ret = RegOpenKeyExW(PredefinedHKeys[root].HKey, key,
                                0, KEY_QUERY_VALUE | KEY_SET_VALUE, &hKey);
        if (ret != ERROR_SUCCESS)
        {
            ErrorL(ret, IDS_NEWVAL);
            continue;
        }

        char fullNameA[MAX_FULL_KEYNAME];
        if (WStrToStr(fullNameA, MAX_KEYNAME, fullName) <= 0)
            *fullNameA = 0;

        // check whether it exists
        DWORD existType;
        ret = RegQueryValueExW(hKey, valName, 0, &existType, NULL, 0);
        if (ret == ERROR_SUCCESS)
        {
            char buf[MAX_FULL_KEYNAME * 2];
            sprintf(buf, LoadStr(IDS_REPLACEVAL), fullNameA);

            if (SG->SalMessageBox(GetParent(), buf, LoadStr(IDS_QUESTION), MB_ICONQUESTION | MB_YESNOCANCEL) != IDYES)
            {
                RegCloseKey(hKey);
                continue;
            }
        }

        switch (type)
        {
        case REG_SZ:
        case REG_EXPAND_SZ:
            ret = RegSetValueExW(hKey, valName, 0, type, (CONST BYTE*)L"", 2);
            break;
        case REG_MULTI_SZ:
            ret = RegSetValueExW(hKey, valName, 0, type, (CONST BYTE*)L"\0", 4);
            break;

        case REG_DWORD_BIG_ENDIAN:
        case REG_DWORD:
        {
            DWORD d = 0;
            ret = RegSetValueExW(hKey, valName, 0, type, (CONST BYTE*)&d, 4);
            break;
        }

        case REG_QWORD:
        {
            QWORD d = 0;
            ret = RegSetValueExW(hKey, valName, 0, type, (CONST BYTE*)&d, 8);
            break;
        }

        default:
            ret = RegSetValueExW(hKey, valName, 0, type, (CONST BYTE*)L"", 0);
            break;
        }

        RegCloseKey(hKey);

        if (ret != ERROR_SUCCESS)
            return ErrorL(ret, IDS_NEWVAL);

        // ensure the new item receives focus
        FocusFirstNewItem = TRUE;

        break;
    }

    return TRUE;
}
