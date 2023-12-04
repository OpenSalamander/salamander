// SPDX-FileCopyrightText: 2023 Open Salamander Authors
// SPDX-License-Identifier: GPL-2.0-or-later

#include "precomp.h"

// ****************************************************************************
//
// CPluginFSInterface - treti cast
//
//

BOOL ConfirmOnFileOverwrite, ConfirmOnSystemHiddenFileOverwrite,
    ConfirmOnOverwrite, ConfirmOnCreateTargetPath;

BOOL SafeOpenKey(int root, LPWSTR key, DWORD sam, HKEY& hKey,
                 int errorTitle, LPBOOL skip, LPBOOL skipAll)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE4("SafeOpenKey(%d, , 0x%X, , %d, , )", root, sam, errorTitle); // Petr: prilis pomaly call-stack
    while (1)
    {
        int res = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, sam, &hKey);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_OPEN, errorTitle, root, key, skip, skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL SafeCreateKey(int root, LPWSTR key, LPWSTR className, DWORD sam, HKEY& hKey,
                   int errorTitle, BOOL& skip, BOOL& skipAll)
{
    CALL_STACK_MESSAGE6("SafeCreateKey(%d, , , 0x%X, , %d, %d, %d)", root, sam,
                        errorTitle, skip, skipAll);
    while (1)
    {
        int res = RegCreateKeyExW(PredefinedHKeys[root].HKey, key, 0, className, 0, sam, NULL, &hKey, NULL);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_CREATE, errorTitle, root, key, &skip, &skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL SafeQueryInfoKey(HKEY hKey, int root, LPWSTR key, LPWSTR className,
                      LPDWORD maxData, FILETIME* time,
                      int errorTitle, BOOL& skip, BOOL& skipAll)
{
    CALL_STACK_MESSAGE_NONE
    //  CALL_STACK_MESSAGE5("SafeQueryInfoKey(, %d, , , , , %d, %d, %d)", root,
    //                      errorTitle, skip, skipAll);  // Petr: prilis pomaly call-stack
    while (1)
    {
        DWORD classSize = MAX_PATH;
        if (className)
            *className = L'\0'; // preventivne ho zakoncime
        int res = RegQueryInfoKeyW(hKey, className, &classSize, NULL, NULL, NULL, NULL, NULL, NULL, maxData, NULL, time);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_ACCESS2, errorTitle, root, key, &skip, &skipAll))
                return FALSE;
        }
        else
        {
#ifdef _DEBUG
            if (classSize > 0)
            {
                char keyNameA[MAX_KEYNAME];
                WStrToStr(keyNameA, MAX_KEYNAME, key);
                char classNameA[MAX_PATH];
                if (className)
                    WStrToStr(classNameA, MAX_PATH, className);
                else
                    classNameA[0] = 0;
                TRACE_I("registry klic " << keyNameA << " ma nastaveny class name: " << classNameA);
            }
#endif _DEBUG
            break;
        }
    }
    return TRUE;
}

BOOL SafeEnumValue(HKEY hKey, int root, LPWSTR key, DWORD& index,
                   LPWSTR name, DWORD& type, void* data, DWORD& size,
                   int errorTitle, BOOL& skip, BOOL& skipAll, BOOL& noMoreItems)
{
    //  CALL_STACK_MESSAGE9("SafeEnumValue(, %d, , 0x%X, , 0x%X, , 0x%X, %d, %d, %d, "
    //                      "%d)", root, index, type, size, errorTitle, skip,
    //                      skipAll, noMoreItems);  // Petr: prilis pomaly call-stack, zjednoduseny kvuli rychlosti + ignorujeme ze je pomaly (stvou me TRACE_E, ktery to generuje)
    SLOW_CALL_STACK_MESSAGE1("SafeEnumValue()");
    while (1)
    {
        DWORD nameSize = MAX_KEYNAME;
        int res = RegEnumValueW(hKey, index, name, &nameSize, NULL, &type, (LPBYTE)data, &size);
        if (res != ERROR_SUCCESS)
        {
            if (res == ERROR_NO_MORE_ITEMS)
            {
                noMoreItems = TRUE;
                skip = FALSE;
                return FALSE;
            }
            if (!RegOperationError(res, IDS_ACCESS2, errorTitle, root, key, &skip, &skipAll))
            {
                index++;
                noMoreItems = FALSE;
                return FALSE;
            }
        }
        else
            break;
    }
    index++;
    noMoreItems = FALSE;
    return TRUE;
}

BOOL TestValue(HKEY hKey, int root, LPWSTR key, LPWSTR name,
               int sourceRoot, LPWSTR sourceKey, LPWSTR sourceName,
               int errorTitle, LPBOOL skip, LPBOOL skipAllErrors,
               LPBOOL overwriteAll, LPBOOL skipAllOvewrites)
{
    //  CALL_STACK_MESSAGE4("TestValue(, %d, , , %d, , , %d, , , , )", root,
    //                      sourceRoot, errorTitle);  // Petr: prilis pomaly call-stack, zjednoduseny kvuli rychlosti + ignorujeme ze je pomaly (stvou me TRACE_E, ktery to generuje)
    SLOW_CALL_STACK_MESSAGE1("TestValue()");
    while (1)
    {
        DWORD type, size;
        int res = RegQueryValueExW(hKey, name, NULL, &type, NULL, &size);
        if (res != ERROR_SUCCESS)
        {
            if (res == ERROR_FILE_NOT_FOUND)
            {
                return TRUE;
            }
            if (!RegOperationError(res, IDS_ACCESS2, errorTitle, root, key, skip, skipAllErrors))
                return FALSE;
        }
        else
        {
            if (overwriteAll && *overwriteAll)
                return TRUE;

            if (skipAllOvewrites && *skipAllOvewrites)
            {
                *skip = TRUE;
                return FALSE;
            }

            // dotaz na prepis
            char fullFSPath[MAX_FULL_KEYNAME];
            char rootName[MAX_PREDEF_KEYNAME];
            char path[MAX_KEYNAME];
            char valueName[MAX_KEYNAME];
            char sourceFullFSPath[MAX_FULL_KEYNAME];
            char sourceRootName[MAX_PREDEF_KEYNAME];
            char sourcePath[MAX_KEYNAME];
            char sourceValueName[MAX_KEYNAME];

            WStrToStr(rootName, MAX_PREDEF_KEYNAME, PredefinedHKeys[root].KeyName);
            WStrToStr(path, MAX_KEYNAME, key);
            if (name && name[0] != L'\0')
                WStrToStr(valueName, MAX_KEYNAME, name);
            else
                strcpy(valueName, LoadStr(IDS_DEFAULTVALUE));
            SalPrintf(fullFSPath, MAX_FULL_KEYNAME, "%s\\%s\\%s", rootName, path, valueName);

            WStrToStr(sourceRootName, MAX_PREDEF_KEYNAME, PredefinedHKeys[sourceRoot].KeyName);
            WStrToStr(sourcePath, MAX_KEYNAME, sourceKey);
            if (name[0] != L'\0')
                WStrToStr(sourceValueName, MAX_KEYNAME, sourceName);
            else
                strcpy(sourceValueName, LoadStr(IDS_DEFAULTVALUE));
            SalPrintf(sourceFullFSPath, MAX_FULL_KEYNAME, "%s\\%s\\%s", sourceRootName, sourcePath, sourceValueName);

            res = skip ? SG->DialogOverwrite(GetParent(), BUTTONS_YESALLSKIPCANCEL, fullFSPath, "", sourceFullFSPath, "") : SG->DialogOverwrite(GetParent(), BUTTONS_YESNOCANCEL, fullFSPath, "", sourceFullFSPath, "");
            switch (res)
            {
            case DIALOG_ALL:
                *overwriteAll = TRUE;
            case DIALOG_YES:
                return TRUE;
            case DIALOG_SKIPALL:
                *skipAllOvewrites = TRUE;
            case DIALOG_SKIP:
                *skip = TRUE;
                return FALSE;
            default:
                if (skip)
                    *skip = FALSE;
                return FALSE;
            }
        }
    }
    return TRUE;
}

BOOL SafeSetValue(HKEY hKey, int root, LPWSTR key,
                  LPWSTR name, DWORD type, void* data, DWORD size,
                  int errorTitle, LPBOOL skip, LPBOOL skipAll)
{
    //  CALL_STACK_MESSAGE5("SafeSetValue(, %d, , , 0x%X, , 0x%X, %d, , )", root,
    //                      type, size, errorTitle); // Petr: prilis pomaly call-stack, zjednoduseny kvuli rychlosti + ignorujeme ze je pomaly (stvou me TRACE_E, ktery to generuje)
    SLOW_CALL_STACK_MESSAGE1("SafeSetValue()");
    while (1)
    {
        int res = RegSetValueExW(hKey, name, 0, type, (LPBYTE)data, size);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_SETVAL2, errorTitle, root, key, skip, skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL SafeEnumKey(HKEY hKey, int root, LPWSTR key, DWORD& index,
                 LPWSTR name, FILETIME* time,
                 int errorTitle, BOOL& skip, BOOL& skipAll, BOOL& noMoreItems)
{
    //  CALL_STACK_MESSAGE7("SafeEnumKey(, %d, , 0x%X, , , %d, %d, %d, %d)", root,
    //                      index, errorTitle, skip, skipAll, noMoreItems); // Petr: prilis pomaly call-stack, zjednoduseny kvuli rychlosti + ignorujeme ze je pomaly (stvou me TRACE_E, ktery to generuje)
    SLOW_CALL_STACK_MESSAGE1("SafeEnumKey()");
    while (1)
    {
        DWORD nameSize = MAX_KEYNAME;
        int res = RegEnumKeyExW(hKey, index, name, &nameSize, NULL, NULL, NULL, time);
        if (res != ERROR_SUCCESS && res != ERROR_MORE_DATA)
        {
            if (res == ERROR_NO_MORE_ITEMS)
            {
                noMoreItems = TRUE;
                skip = FALSE;
                return FALSE;
            }
            if (!RegOperationError(res, IDS_ACCESS2, errorTitle, root, key, &skip, &skipAll))
            {
                index++;
                noMoreItems = FALSE;
                return FALSE;
            }
        }
        else
            break;
    }
    index++;
    noMoreItems = TRUE;
    return TRUE;
}

BOOL SafeDeleteKey(int root, LPWSTR key, int errorTitle, BOOL& skip, BOOL& skipAll)
{
    CALL_STACK_MESSAGE5("SafeDeleteKey(%d, , %d, %d, %d)", root, errorTitle,
                        skip, skipAll);
    while (1)
    {
        int res = RegDeleteKeyW(PredefinedHKeys[root].HKey, key);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_REMOVESOURCE, errorTitle, root, key, &skip, &skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL CopyOrMoveKey(int sourceRoot, LPWSTR source, int targetRoot, LPWSTR target,
                   BOOL move, BOOL& skip,
                   BOOL& skipAllErrors, BOOL& skipAllLongNames,
                   BOOL& skipAllOverwrites, BOOL& overwriteAll,
                   BOOL& skipAllClassNames, LPWSTR nameBuffer,
                   TIndirectArray<WCHAR>& stack)
{
    //  CALL_STACK_MESSAGE10("CopyOrMoveKey(%d, , %d, , %d, %d, %d, %d, %d, %d, %d, , )",
    //                       sourceRoot, targetRoot, move, skip, skipAllErrors,
    //                       skipAllLongNames, skipAllOverwrites, overwriteAll,
    //                       skipAllClassNames); // Petr: prilis pomaly call-stack, zjednoduseny kvuli rychlosti
    CALL_STACK_MESSAGE1("CopyOrMoveKey()");
    // test na preruseni uzivatelem
    if (TestForCancel())
        return skip = FALSE;

    HKEY sourceHKey, targetHKey;
    int errorTitle = move ? IDS_MOVEKEY : IDS_COPYKEY;

    // otevreme zdrojovy klic
    if (!SafeOpenKey(sourceRoot, source, KEY_READ, sourceHKey,
                     errorTitle, &skip, &skipAllErrors))
        return FALSE;

    // nacteme jmeno tridy a max data
    DWORD maxData;
    WCHAR sourceClassName[MAX_PATH];
    if (!SafeQueryInfoKey(sourceHKey, sourceRoot, source, sourceClassName,
                          &maxData, NULL, errorTitle, skip, skipAllErrors))
    {
        RegCloseKey(sourceHKey);
        return FALSE;
    }

    // MS nekdy vraci polovicni velikost (pozorovano na MULTI_SZ v
    // klici HKEY_LOCAL_MACHINE\SYSTEM\ControlSet002\Services\NetBT\Linkage
    maxData *= 2;

    // vytvorime cilovy klic
    if (!SafeCreateKey(targetRoot, target, sourceClassName, KEY_WRITE | KEY_READ, targetHKey,
                       errorTitle, skip, skipAllErrors))
    {
        RegCloseKey(sourceHKey);
        return FALSE;
    }

    // overime, ze novy klic ma stejne jmeno tridy
    if (!skipAllClassNames)
    {
        WCHAR targetClassName[MAX_PATH];
        if (!SafeQueryInfoKey(sourceHKey, sourceRoot, source, targetClassName, NULL, NULL, errorTitle, skip, skipAllErrors))
        {
            RegCloseKey(sourceHKey);
            RegCloseKey(targetHKey);
            return FALSE;
        }

        if (CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                           sourceClassName, -1,
                           targetClassName, -1) != CSTR_EQUAL)
        {
            char fileName[MAX_FULL_KEYNAME];
            SalPrintf(fileName, MAX_FULL_KEYNAME, "%ls\\%ls", PredefinedHKeys[sourceRoot].KeyName, source);
            switch (SG->DialogQuestion(GetParent(), BUTTONS_YESALLCANCEL, fileName, LoadStr(IDS_COPYCLASSNAME), LoadStr(IDS_WARNING)))
            {
            case DIALOG_ALL:
                skipAllClassNames = TRUE;
            case DIALOG_YES:
                break;

            default:
                skip = FALSE;
                RegCloseKey(sourceHKey);
                RegCloseKey(targetHKey);
                return FALSE;
            }
        }
    }

    // zkopirujeme obsah klice - nejprve hodnoty
    void* data = malloc(maxData);
    if (!data)
    {
        skip = FALSE;
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return Error(IDS_LOWMEM);
    }

    DWORD index = 0;
    LPWSTR name = nameBuffer;
    DWORD type, size;
    BOOL noMore;
    BOOL someFileSkipped = FALSE;
    BOOL success;

    while (1)
    {
        success =
            SafeEnumValue(sourceHKey, sourceRoot, source, index,
                          name, type, data, size = maxData,
                          errorTitle, skip, skipAllErrors, noMore) &&
            TestValue(targetHKey, targetRoot, target, name,
                      sourceRoot, source, name,
                      errorTitle, &skip, &skipAllErrors,
                      &overwriteAll, &skipAllOverwrites) &&
            SafeSetValue(targetHKey, targetRoot, target, name,
                         type, data, size, errorTitle, &skip, &skipAllErrors);

        if (!success)
        {
            if (skip)
            {
                someFileSkipped = TRUE;
                skip = FALSE;
            }
            else
                break;
        }

        // test na preruseni uzivatelem
        if (TestForCancel())
            break;
    }

    free(data);

    if (!noMore)
    {
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return skip = FALSE;
    }

    // jeste zkopirujeme rekurzivne podklice

    // nejprve naenumerujeme vsechny klice na stack
    int sourceLen = (int)wcslen(source), targetLen = (int)wcslen(target);
    LPWSTR sourceSubkey = source + sourceLen, targetSubkey = target + targetLen;
    *sourceSubkey++ = L'\\';
    *targetSubkey++ = L'\\';
    int maxSubkey = min(MAX_KEYNAME - sourceLen - 2, MAX_KEYNAME - targetLen - 2);
    index = 0;
    int top = stack.Count; // ulozime si ukazatel na vrsek zasobniku
    while (1)
    {
        if (SafeEnumKey(sourceHKey, sourceRoot, source, index, name, NULL,
                        errorTitle, skip, skipAllErrors, noMore))
        {
            int nameLen = (int)wcslen(name);
            if (nameLen > maxSubkey)
            {
                if (skipAllLongNames)
                {
                    someFileSkipped = TRUE;
                    continue;
                }

                char nameA[MAX_KEYNAME];
                WStrToStr(nameA, MAX_KEYNAME, name);

                int res = SG->DialogError(GetParent(), BUTTONS_SKIPCANCEL, nameA, LoadStr(IDS_LONGNAME), LoadStr(errorTitle));
                switch (res)
                {
                case DIALOG_SKIPALL:
                    skipAllLongNames = TRUE;
                case DIALOG_SKIP:
                    someFileSkipped = TRUE;
                    continue;

                default:
                    skip = FALSE;
                    RegCloseKey(sourceHKey);
                    RegCloseKey(targetHKey);
                    return FALSE; // DIALOG_CANCEL
                }
            }

            LPWSTR ptr = new WCHAR[nameLen + 1];
            if (!ptr || stack.Add(ptr) == ULONG_MAX)
            {
                if (ptr)
                    delete[] ptr;
                skip = FALSE;
                RegCloseKey(sourceHKey);
                RegCloseKey(targetHKey);
                return Error(IDS_LOWMEM);
            }
            wcscpy(ptr, name);
        }
        else
        {
            if (skip)
            {
                someFileSkipped = TRUE;
                skip = FALSE;
            }
            else
                break;
        }

        // test na preruseni uzivatelem
        if (TestForCancel())
            break;
    }

    if (!noMore)
    {
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return skip = FALSE;
    }

    // zkopirujeme klice ulozene na stacku
    int i;
    for (i = stack.Count - 1; i >= top; i--)
    {
        wcscpy(sourceSubkey, stack[i]);
        wcscpy(targetSubkey, stack[i]);

        if (!CopyOrMoveKey(sourceRoot, source, targetRoot, target, move,
                           skip, skipAllErrors, skipAllLongNames,
                           skipAllOverwrites, overwriteAll,
                           skipAllClassNames, nameBuffer, stack))
        {
            if (skip)
                someFileSkipped = TRUE;
            else
            {
                RegCloseKey(sourceHKey);
                RegCloseKey(targetHKey);
                return FALSE;
            }
        }
        stack.Delete(i);
    }

    *--sourceSubkey = L'\0';
    *--targetSubkey = L'\0';

    RegCloseKey(sourceHKey);
    RegCloseKey(targetHKey);

    if (someFileSkipped)
    {
        skip = TRUE;
        return FALSE;
    }

    if (move)
        return SafeDeleteKey(sourceRoot, source, errorTitle, skip, skipAllErrors);

    return TRUE;
}

BOOL SafeQueryValue(HKEY hKey, int root, LPWSTR key, LPWSTR value,
                    DWORD& type, void* data, DWORD& size,
                    int errorTitle, LPBOOL skip, LPBOOL skipAll)
{
    CALL_STACK_MESSAGE5("SafeQueryValue(, %d, , , 0x%X, , 0x%X, %d, , )", root,
                        type, size, errorTitle);
    while (1)
    {
        int res = RegQueryValueExW(hKey, value, NULL, &type, (LPBYTE)data, &size);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_ACCESS2, errorTitle, root, key, skip, skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL SafeDeleteValue(HKEY hKey, int root, LPWSTR key, LPWSTR value,
                     int errorTitle, LPBOOL skip, LPBOOL skipAll)
{
    CALL_STACK_MESSAGE3("SafeDeleteValue(, %d, , , %d, , )", root, errorTitle);
    while (1)
    {
        int res = RegDeleteValueW(hKey, value);
        if (res != ERROR_SUCCESS)
        {
            if (!RegOperationError(res, IDS_REMOVESOURCE2, errorTitle, root, key, skip, skipAll))
                return FALSE;
        }
        else
            break;
    }
    return TRUE;
}

BOOL CopyOrMoveValue(int sourceRoot, LPWSTR sourcePath, LPWSTR sourceName,
                     int targetRoot, LPWSTR targetPath, LPWSTR targetName,
                     BOOL move, LPBOOL skip,
                     LPBOOL skipAllErrors, LPBOOL skipAllOverwrites, LPBOOL overwriteAll)
{
    CALL_STACK_MESSAGE4("CopyOrMoveValue(%d, , , %d, , , %d, , , , )",
                        sourceRoot, targetRoot, move);
    HKEY sourceHKey, targetHKey;
    int errorTitle = move ? IDS_MOVEVALUE : IDS_COPYVALUE;

    // otevreme zdrojovy klic
    if (!SafeOpenKey(sourceRoot, sourcePath, KEY_READ | (move ? KEY_WRITE : 0), sourceHKey,
                     errorTitle, skip, skipAllErrors))
        return FALSE;

    // otevreme cilovy klic
    if (!SafeOpenKey(targetRoot, targetPath, KEY_READ | KEY_WRITE, targetHKey,
                     errorTitle, skip, skipAllErrors))
    {
        RegCloseKey(sourceHKey);
        return FALSE;
    }

    // otestujeme moznost prepisu
    if (!TestValue(targetHKey, targetRoot, targetPath, targetName,
                   sourceRoot, sourcePath, sourceName,
                   errorTitle, skip, skipAllErrors, overwriteAll, skipAllOverwrites))
    {
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return FALSE;
    }

    // nacteme velikost hodnoty
    DWORD size = 0, type;
    if (!SafeQueryValue(sourceHKey, sourceRoot, sourcePath, sourceName, type, NULL, size,
                        errorTitle, skip, skipAllErrors))
    {
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return FALSE;
    }

    // nacteme hodnotu
    void* data = malloc(size);
    if (!data)
    {
        skip = FALSE;
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return Error(IDS_LOWMEM);
    }

    if (!SafeQueryValue(sourceHKey, sourceRoot, sourcePath, sourceName, type, data, size,
                        errorTitle, skip, skipAllErrors))
    {
        free(data);
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return FALSE;
    }

    // nastavime hodnotu
    if (!SafeSetValue(targetHKey, targetRoot, targetPath, targetName, type, data, size,
                      errorTitle, skip, skipAllErrors))
    {
        free(data);
        RegCloseKey(sourceHKey);
        RegCloseKey(targetHKey);
        return FALSE;
    }

    free(data);
    RegCloseKey(targetHKey);

    // jde-li o move hodnotu smazeme ve zdrojovem klici
    BOOL ret = move ? SafeDeleteValue(sourceHKey, sourceRoot, sourcePath, sourceName,
                                      errorTitle, skip, skipAllErrors)
                    : TRUE;

    RegCloseKey(sourceHKey);

    return ret;
}

BOOL CreateTargetPath(int root, LPWSTR key, int errorTitle)
{
    CALL_STACK_MESSAGE3("CreateTargetPath(%d, , %d)", root, errorTitle);
    WCHAR targetPath[MAX_KEYNAME];
    targetPath[0] = 0;
    int i = 0, j;
    while (key[i] != L'\0')
    {
        j = (int)wcscspn(key + i + 1, L"\\") + 1;
        memcpy(targetPath + i, key + i, j * 2);
        i += j;
        targetPath[i] = L'\0';

        int err;
        HKEY hKey;
        if ((err = RegCreateKeyExW(PredefinedHKeys[root].HKey, targetPath, 0, NULL, 0, KEY_READ, NULL, &hKey, NULL)) != ERROR_SUCCESS)
        {
            return !ErrorL(err, IDS_CREATE, PredefinedHKeys[root].KeyName, targetPath);
        }
        else
            RegCloseKey(hKey);
    }
    return TRUE;
}

int ExpandPluralFilesDirs(LPWSTR buffer, int bufferSize, int files, int dirs,
                          int panel, BOOL focused, BOOL copy)
{
    CALL_STACK_MESSAGE7("ExpandPluralFilesDirs(, %d, %d, %d, %d, %d, %d)",
                        bufferSize, files, dirs, panel, focused, copy);
    char formatA[200];
    WCHAR formatW[200];
    LPWSTR copyOrMove = LoadStrW(copy ? IDS_COPY : IDS_MOVE);

    if (files > 0 && dirs > 0)
    {
        CQuadWord parametersArray[] = {CQuadWord(files, 0), CQuadWord(dirs, 0)};
        SG->ExpandPluralString(formatA, 200, LoadStr(IDS_COPYORMOVE5), 2, parametersArray);
        StrToWStr(formatW, 200, formatA);
        return SalPrintfW(buffer, bufferSize, formatW, copyOrMove, files, dirs);
    }
    if (files == 1 || dirs == 1)
    {
        int index = 0;
        const CFileData* f;
        BOOL isDir;
        if (focused)
            f = SG->GetPanelFocusedItem(panel, &isDir);
        else
            f = SG->GetPanelSelectedItem(panel, &index, &isDir);
        CPluginData* pd = (CPluginData*)f->PluginData;

        return SalPrintfW(buffer, bufferSize, LoadStrW(files == 1 ? IDS_COPYORMOVE1 : IDS_COPYORMOVE2),
                          copyOrMove, pd->Name == NULL ? LoadStrW(IDS_DEFAULTVALUE) : pd->Name);
    }
    CQuadWord param = CQuadWord(files + dirs, 0);
    SG->ExpandPluralString(formatA, 200, LoadStr(files ? IDS_COPYORMOVE3 : IDS_COPYORMOVE4), 1, &param);
    StrToWStr(formatW, 200, formatA);
    return SalPrintfW(buffer, bufferSize, formatW, copyOrMove, files + dirs);
}

BOOL CPluginFSInterface::CopyOrMoveFromFS(BOOL copy, int mode, const char* fsName, HWND parent,
                                          int panel, int selectedFiles, int selectedDirs,
                                          char* targetPath, BOOL& operationMask,
                                          BOOL& cancelOrHandlePath, HWND dropTarget)
{
    CALL_STACK_MESSAGE9("CPluginFSInterface::CopyOrMoveFromFS(%d, %d, %s, , %d, %d, "
                        "%d, , %d, %d, )",
                        copy, mode, fsName, panel, selectedFiles,
                        selectedDirs, operationMask, cancelOrHandlePath);
    PARENT(parent);

    BOOL focused = (selectedFiles == 0 && selectedDirs == 0);
    int errorTitle = copy ? IDS_COPYKEY : IDS_MOVEKEY;
    int title = copy ? IDS_COPY : IDS_MOVERENAME;

    cancelOrHandlePath = TRUE;

    WCHAR targetPathW[MAX_FULL_KEYNAME];
    // cilova cesta zadana pres drag&drop
    if (mode == 5 && MultiByteToWideChar(CP_ACP, 0, targetPath, -1, targetPathW, MAX_FULL_KEYNAME) <= 0)
        return TRUE;

    // pro jistotu
    if (mode != 1 && mode != 5)
        return TRUE;

    if (CurrentKeyRoot == -1)
        return TRUE; //HKEY_XXX nejde kopirovat

    WCHAR enteredTargetPathW[MAX_FULL_KEYNAME];
    int targetPanel = (panel == PANEL_LEFT ? PANEL_RIGHT : PANEL_LEFT);

    // podivame se jestli jsou v druhem panelu otevrene registry
    CPluginFSInterface* targetFS = (CPluginFSInterface*)SG->GetPanelPluginFS(targetPanel);
    if (mode != 5 && targetFS && targetFS->CurrentKeyRoot != -1)
    {
        if (*targetFS->CurrentKeyName)
            swprintf_s(enteredTargetPathW, L"%hs:\\%s\\%s\\", fsName, PredefinedHKeys[targetFS->CurrentKeyRoot].KeyName, targetFS->CurrentKeyName);
        else
            swprintf_s(enteredTargetPathW, L"%hs:\\%s\\", fsName, PredefinedHKeys[targetFS->CurrentKeyRoot].KeyName);

        SG->SetUserWorkedOnPanelPath(PANEL_TARGET); // default akce = prace s cestou v cilovem panelu
    }
    else
        *enteredTargetPathW = L'\0';

    BOOL firstRound = TRUE;
    while (1)
    {
        if (!firstRound && mode == 5)
            return TRUE; // chyba na ceste zadane drag&dropem, koncime
        firstRound = FALSE;

        // zjistime od uzivatele cestu
        BOOL direct = FALSE;
        WCHAR text[MAX_KEYNAME + 200];
        ExpandPluralFilesDirs(text, MAX_KEYNAME + 200, selectedFiles, selectedDirs, panel, focused, copy);
        CCopyOrMoveDialog dlg(parent, enteredTargetPathW, &direct, text, LoadStrW(title));
        if (mode != 5 && dlg.Execute() != IDOK)
            return TRUE;

        if (mode != 5)
            wcscpy(targetPathW, enteredTargetPathW);

        // vyseparujeme z FS path user part
        if (!direct)
        {
            if (!RemoveFSNameFromPath(targetPathW))
            {
                Error(IDS_NOTREGEDTPATH);
                continue;
            }
            if (!wcslen(targetPathW))
            {
                Error(IDS_BADPATH);
                continue;
            }
        }

        // relativni cestu prevedeme na uplnou
        BOOL success; // FALSE v pripade chyby nebo preruseni uzivatelem
        GetFullFSPathW(targetPathW, MAX_FULL_KEYNAME, success);
        if (!success)
            continue;

        WCHAR* key;
        int root;
        if (!ParseFullPath(targetPathW, key, root))
        {
            Error(IDS_BADPATH);
            continue;
        }

        if (root == -1)
        {
            Error(IDS_COPYTOTROOT);
            continue;
        }

        WCHAR targetName[MAX_KEYNAME]; // jmeno ciloveho souboru (pokud je vybran pouze jeden soubor)
        BOOL useTargetName = FALSE;
        int len = (int)wcslen(key), err;
        HKEY hKey;

        // zjistime o jaky typ operace jde
        if (len > 0)
        {
            len--;
            if (key[len] != L'\\')
            {
                if ((err = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, KEY_READ, &hKey)) != ERROR_SUCCESS)
                {
                    // pokud cesta nezakoncena lomitkem neexistuje, povazujeme posledni cast cesty, za
                    // jmeno ciloveho souboru (masky neumime), dava smysl pouze pokud je vybran jen
                    // jeden soubor
                    if (err != ERROR_FILE_NOT_FOUND)
                    {
                        ErrorL(err, IDS_ACCESS, PredefinedHKeys[root].KeyName, key);
                        continue;
                    }
                    if (selectedFiles + selectedDirs > 1)
                    {
                        Error(IDS_SAMETARGET);
                        continue;
                    }
                    if (!CutDirectory(key, targetName, MAX_KEYNAME))
                    {
                        Error(IDS_LONGNAME);
                        continue;
                    }
                    useTargetName = TRUE;
                }
                else
                    RegCloseKey(hKey);
            }
            else
                key[len] = L'\0'; // ostranime lomitko z konce cesty
        }

        // overime, zda cilova cesta existuje
        if ((err = RegOpenKeyExW(PredefinedHKeys[root].HKey, key, 0, KEY_READ, &hKey)) != ERROR_SUCCESS)
        {
            if (err != ERROR_FILE_NOT_FOUND)
            {
                ErrorL(err, IDS_ACCESS, PredefinedHKeys[root].KeyName, key);
                continue;
            }

            // cesta neexistuje, zeptame se, zda ji mame vytvorit
            char message[MAX_KEYNAME + 200];
            SalPrintf(message, MAX_KEYNAME, LoadStr(IDS_CREATETARGET), PredefinedHKeys[root].KeyName, key);
            if (SG->SalMessageBox(GetParent(), message, LoadStr(title), MB_YESNO) != IDYES)
                continue;

            // vytvorime ji
            if (!CreateTargetPath(root, key, errorTitle))
                continue;
        }
        else
            RegCloseKey(hKey);

        // vyzvedneme hodnoty "Confirm on" z konfigurace
        SG->GetConfigParameter(SALCFG_CNFRMFILEOVER, &ConfirmOnFileOverwrite, 4, NULL);
        SG->GetConfigParameter(SALCFG_CNFRMSHFILEOVER, &ConfirmOnSystemHiddenFileOverwrite, 4, NULL);
        SG->GetConfigParameter(SALCFG_CNFRMCREATEPATH, &ConfirmOnCreateTargetPath, 4, NULL);
        //ConfirmOnOverwrite = ConfirmOnFileOverwrite || ConfirmOnSystemHiddenFileOverwrite;
        ConfirmOnOverwrite = TRUE;

        const CFileData* f = NULL; // ukazatel na soubor/adresar v panelu, ktery se ma zpracovat
        BOOL isDir = FALSE;        // TRUE pokud 'f' je adresar
        int index = 0;
        BOOL skipAllErrors = FALSE; // skip all errors
        BOOL skipAllOverwrites = FALSE;
        BOOL skipAllLongNames = FALSE;
        BOOL skipAllClassNames = FALSE;
        BOOL overwriteAll = !ConfirmOnOverwrite;
        char nextFocus[2 * MAX_PATH];
        nextFocus[0] = 0;
        BOOL sourceEqualsTarget = FALSE;

        // overime, ze nekopirujeme soubor do sebe sameho
        if (root == CurrentKeyRoot &&
            CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                           key, -1,
                           CurrentKeyName, -1) == CSTR_EQUAL)
        {
            // vyzvedneme data o prvnim zpracovavanem souboru
            int index2 = 0;
            if (focused)
                f = SG->GetPanelFocusedItem(panel, &isDir);
            else
                f = SG->GetPanelSelectedItem(panel, &index2, &isDir);
            CPluginData* pd = (CPluginData*)f->PluginData;

            if (!useTargetName ||
                pd->Name && CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                           pd->Name, -1,
                                           targetName, -1) == CSTR_EQUAL)
            {
                Error(copy ? IDS_CANTCOPYTOITSELF : IDS_CANTMOVETOITSELF);
                continue;
            }

            // jde o rename
            if (!copy)
                WStrToStr(nextFocus, 2 * MAX_PATH, targetName);
            sourceEqualsTarget = TRUE;
        }

        int sourceRoot = CurrentKeyRoot;
        WCHAR sourceKey[MAX_KEYNAME];
        wcscpy(sourceKey, CurrentKeyName);
        int sourceLen = (int)wcslen(sourceKey), targetLen = (int)wcslen(key);
        LPWSTR sourceSubkey = sourceKey + sourceLen, targetSubkey = key + targetLen;
        int maxSubkey = min(MAX_KEYNAME - sourceLen - 2, MAX_KEYNAME - targetLen - 2);
        // zasobnik na enumerovana jmena podklicu
        // (podklice se musi nejpreve vsechny najednou naenumerovat
        // a potom se mohou mazat (pri presunu)
        TIndirectArray<WCHAR> stack(100, 100);

        GetAsyncKeyState(VK_ESCAPE); // init GetAsyncKeyState - viz help
        SG->CreateSafeWaitWindow(LoadStr(copy ? IDS_COPYPROGRESS : IDS_MOVEPROGRESS),
                                 LoadStr(IDS_PLUGINNAME), 500, TRUE, SG->GetMainWindowHWND());

        while (1)
        {
            // vyzvedneme data o zpracovavanem souboru
            if (focused)
                f = SG->GetPanelFocusedItem(panel, &isDir);
            else
                f = SG->GetPanelSelectedItem(panel, &index, &isDir);

            // provedeme copy/move na souboru/adresari
            if (f != NULL)
            {
                CPluginData* pd = (CPluginData*)f->PluginData;
                BOOL skip = FALSE;
                if (isDir)
                {
                    int nameLen = (int)wcslen(pd->Name);
                    if (nameLen > maxSubkey)
                    {
                        if (skipAllLongNames)
                        {
                            skip = TRUE;
                        }
                        else
                        {
                            char nameA[MAX_KEYNAME];
                            WStrToStr(nameA, MAX_KEYNAME, pd->Name);

                            int res = SG->DialogError(GetParent(), BUTTONS_SKIPCANCEL, nameA, LoadStr(IDS_LONGNAME), LoadStr(errorTitle));
                            switch (res)
                            {
                            case DIALOG_SKIPALL:
                                skipAllLongNames = TRUE;
                            case DIALOG_SKIP:
                                skip = TRUE;
                                break;

                            default:
                                success = FALSE;
                                break;
                            }
                        }
                    }

                    if (success && !skip)
                    {
                        if (sourceSubkey > sourceKey)
                        {
                            sourceSubkey[0] = L'\\';
                            wcscpy(sourceSubkey + 1, pd->Name);
                        }
                        else
                            wcscpy(sourceSubkey, pd->Name);

                        if (targetSubkey > key)
                        {
                            targetSubkey[0] = L'\\';
                            wcscpy(targetSubkey + 1, useTargetName ? targetName : pd->Name);
                        }
                        else
                            wcscpy(targetSubkey, useTargetName ? targetName : pd->Name);

                        // jeste overime, ze sourceKey neni prefixem targetKey
                        unsigned sourceLen2 = (int)wcslen(sourceKey);
                        if (root == sourceRoot &&
                            sourceLen2 <= wcslen(key) &&
                            CompareStringW(LOCALE_USER_DEFAULT, NORM_IGNORECASE,
                                           sourceKey, sourceLen2,
                                           key, sourceLen2) == CSTR_EQUAL &&
                            (key[sourceLen2] == L'\0' || key[sourceLen2] == L'\\'))
                        {
                            char message[MAX_KEYNAME + 200];
                            SalPrintf(message, MAX_KEYNAME + 200,
                                      LoadStr(copy ? IDS_CANTCOPYTOITSELF2 : IDS_CANTMOVETOITSELF2),
                                      pd->Name);

                            if (SG->SalMessageBox(GetParent(), message, LoadStr(errorTitle), MB_OKCANCEL) == IDOK)
                                skip = TRUE;
                            else
                                success = FALSE;
                        }

                        if (!skip && success)
                        {
                            WCHAR nameBuffer[MAX_KEYNAME];
                            success = CopyOrMoveKey(sourceRoot, sourceKey, root, key, !copy,
                                                    skip, skipAllErrors, skipAllLongNames,
                                                    skipAllOverwrites, overwriteAll,
                                                    skipAllClassNames, nameBuffer, stack) ||
                                      skip;
                        }

                        if (sourceSubkey > sourceKey)
                            sourceSubkey[0] = L'\0';
                        if (targetSubkey > key)
                            targetSubkey[0] = L'\0';
                    }
                }
                else
                {
                    // nebudeme zpracovavat defaultni hodnotu pokud neni nastavena
                    if (pd->Name != NULL || pd->Type != REG_NONE)
                    {
                        success = CopyOrMoveValue(sourceRoot, sourceKey, pd->Name,
                                                  root, key, useTargetName ? targetName : pd->Name, !copy,
                                                  &skip, &skipAllErrors, &skipAllOverwrites, &overwriteAll) ||
                                  skip;
                    }
                }
            }

            // zjistime jestli ma cenu pokracovat (pokud neni cancel a existuje jeste nejaka dalsi oznacena polozka)
            if (!success || focused || f == NULL)
                break;
        }

        SG->DestroySafeWaitWindow();

        if (success)
        {
            strcpy(targetPath, nextFocus); // uspech
            cancelOrHandlePath = FALSE;
        }
        else
            cancelOrHandlePath = TRUE; // error/cancel

        break;
    }

    return TRUE;
}

BOOL CPluginFSInterface::OpenFindDialog(const char* fsName, int panel)
{
    CALL_STACK_MESSAGE3("CPluginFSInterface::OpenFindDialog(%s, %d)", fsName, panel);

    PARENT(SG->GetMainWindowHWND());
    WCHAR path[MAX_FULL_KEYNAME];
    int len = StrToWStr(path, MAX_FULL_KEYNAME, fsName);
    path[len - 1] = L':';
    if (!GetCurrentPathW(path + len, MAX_FULL_KEYNAME))
    {
        wcscpy(path + len, L"\\");
    }
    CFindDialogThread* t = new CFindDialogThread(path);
    if (t)
    {
        if (!t->Create(ThreadQueue))
            delete t;
    }
    else
        Error(IDS_LOWMEM);
    return TRUE;
}

void CPluginFSInterface::OpenActiveFolder(const char* fsName, HWND parent)
{
    // regedit nema parametr, kterym by bylo mozne nastavit cestu v Registry, ktera by se mela zobrazit
    // lze mu ale nastavit HKEY_CURRENT_USER\Software\Microsoft\Windows\CurrentVersion\Applets\Regedit\LastKey
    // ve tvaru (napr) "Computer\HKEY_CURRENT_USER\AppEvents\Schemes\Apps"
    // POZOR: slovo "Computer" je na českých Win7 lokalizováno jako "Počítač", nastesti ale regedit.exe
    // toto slovo nevyzaduje a cesta muze zacinat az rootem HKEY_*

    // aktualni cestu v panelu vlozime do LastKey hodnoty pro RegEdit
    HKEY hKey;
    DWORD disp;
    RegCreateKeyEx(HKEY_CURRENT_USER, "Software\\Microsoft\\Windows\\CurrentVersion\\Applets\\Regedit", 0, NULL,
                   REG_OPTION_NON_VOLATILE, KEY_CREATE_SUB_KEY | KEY_WRITE, NULL, &hKey, &disp);
    char path[100 + MAX_PATH];
    GetCurrentPath(path);
    const char* path2 = path;
    if (*path2 == '\\')
        path2++;
    RegSetValueEx(hKey, "LastKey", 0, REG_SZ, (LPBYTE)path2, lstrlen(path2) + 1);
    RegCloseKey(hKey);

    // spustime regedit s moznosti eskalace (vista/7)
    char regEditPath[MAX_PATH];
    if (!GetWindowsDirectory(regEditPath, MAX_PATH))
        *regEditPath = 0;
    else
        SG->SalPathAddBackslash(regEditPath, MAX_PATH);
    lstrcat(regEditPath, "regedit.exe");

    SHELLEXECUTEINFO sei = {0};
    sei.cbSize = sizeof(SHELLEXECUTEINFO);
    sei.fMask = SEE_MASK_NOCLOSEPROCESS;
    sei.hwnd = SG->GetMainWindowHWND();
    sei.lpFile = regEditPath;
    sei.lpParameters = "";
    sei.lpDirectory = "";
    sei.nShow = SW_SHOWNORMAL;
    if (!ShellExecuteEx(&sei))
    {
        DWORD err = GetLastError();
        if (err != ERROR_CANCELLED)
            Error(IDS_PROCESS2, "regedit.exe");
    }
}
